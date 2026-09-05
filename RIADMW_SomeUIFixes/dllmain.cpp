#include "SDK.hpp"
#include <thread>
#include <cmath>
#include <numbers>
#include <atomic>
#include <cwchar>
#include <MinHook.h>

using namespace SDK;

constexpr float TARGET_ASPECT = 16.0f / 9.0f;
constexpr uintptr_t PROCESSEVENT_RVA = 0x01821740; // from OffsetsInfo.json (new build)

// FOV boost. Every reflected FOV function is dead on this build (verified:
// GetFOVAngle, SetFieldOfView, the game's ASevCameraAnimActor::GetFieldOfView,
// CineCamera getters, etc. all see 0 calls). The game writes
// UCameraComponent::FieldOfView (a plain public float member at offset 0x240)
// directly and does NOT rewrite it during gameplay. So write that member
// directly — a plain memory store, unlike the SDK SetFieldOfView() method
// which goes through a broken reflected dispatch (2x doubling, no-op).
// Multiplier on tan(FOV/2): 1.0 = off, ~1.05 subtle, ~1.12 noticeable, 1.2+ strong.
// Overridable via fov.ini next to the .asi:  [fov] boost=1.5
static float g_FovBoost = 1.5f;

// FOV boost state. The boost is derived ONLY from the tracked authored base
// (g_Base), never from the current value — the game feeds our boosted value
// back through camera cross-links (save points), so boosting "cur" compounds
// into fisheye. Any readback matching a value we previously wrote is our own
// echo (same camera, or a cross-linked one) and is never re-adopted as
// authored input.
constexpr int FOV_WRITE_HISTORY = 8;
constexpr float FOV_EPS = 0.01f; // float slack: ULP noise + blend jitter
static float g_Base = 0.f;        // current authored FOV (75 normal, 45 ADS)
static float g_LastWritten = 0.f; // last value we stored into FieldOfView
static float g_WriteHistory[FOV_WRITE_HISTORY] = {}; // recent stores, to spot cross-linked echoes
static int g_WriteHistoryIdx = 0;

std::atomic<bool> g_Running{ true };
std::atomic<bool> g_Ready{ false };
HMODULE g_Module = nullptr;
float g_ConstrainedW = 0;
float g_ConstrainedH = 0;
float g_SlateW = 0;
float g_SlateH = 0;
UClass* g_WidgetClass = nullptr;
UClass* g_FadeClass = nullptr;
UClass* g_CinemaScopeClass = nullptr;
UClass* g_CinematicPauseMenuClass = nullptr;
UClass* g_SimplePauseMenuClass = nullptr;
UClass* g_PauseMenuOption2Class = nullptr;
UClass* g_CameraClass = nullptr;
UFunction* g_TickFunc = nullptr;

typedef void (*UProcessEvent)(UObject*, UFunction*, void*);
UProcessEvent g_OrigProcessEvent = nullptr;

static float BoostFOV(float fovDeg)
{
	if (fovDeg <= 0.f || fovDeg >= 180.f)
		return fovDeg;
	const float halfTan = std::tan(fovDeg * std::numbers::pi_v<float> / 360.f) * g_FovBoost;
	return 2.f * std::atan(halfTan) * (180.f / std::numbers::pi_v<float>);
}

// Optional user config: fov.ini next to the .asi.
//	[fov]
//	boost=1.5
static void LoadFovConfig()
{
	wchar_t iniPath[MAX_PATH] = {};
	if (GetModuleFileNameW(g_Module, iniPath, MAX_PATH) == 0)
		return;
	wchar_t* slash = std::wcsrchr(iniPath, L'\\');
	if (!slash)
		return;
	lstrcpyW(slash + 1, L"fov.ini");

	wchar_t buf[64] = {};
	GetPrivateProfileStringW(L"fov", L"boost", L"1.5", buf, 64, iniPath);
	wchar_t* end = nullptr;
	const float v = std::wcstof(buf, &end);
	if (end != buf && v >= 1.0f && v <= 4.0f)
		g_FovBoost = v;
}

static void ApplyFullWidthToFadeOrCinemaScope(UWidget* w, float negW)
{
	if (!w) return;
	bool isFullWidth = (g_FadeClass && w->IsA(g_FadeClass))
		|| (g_CinemaScopeClass && w->IsA(g_CinemaScopeClass))
		|| (g_CinematicPauseMenuClass && w->IsA(g_CinematicPauseMenuClass))
		|| (g_SimplePauseMenuClass && w->IsA(g_SimplePauseMenuClass))
		|| (g_PauseMenuOption2Class && w->IsA(g_PauseMenuOption2Class));
	if (isFullWidth)
	{
		UCanvasPanelSlot* slot = UWidgetLayoutLibrary::SlotAsCanvasSlot(w);
		if (slot)
			slot->SetOffsets(FMargin(negW, 0.0f, negW, 0.0f));
	}
	if (w->IsA(UPanelWidget::StaticClass()))
	{
		UPanelWidget* panel = static_cast<UPanelWidget*>(w);
		TArray<UWidget*> children = panel->GetAllChildren();
		for (int32 c = 0; c < children.Num(); ++c)
		{
			if (UWidget* child = children[c])
				ApplyFullWidthToFadeOrCinemaScope(child, negW);
		}
	}
}

void CalcConstrainedSize(FVector2D vp, float uiScale)
{
	g_SlateW = vp.X / uiScale;
	g_SlateH = vp.Y / uiScale;
	float aspect = g_SlateW / g_SlateH;

	if (aspect > TARGET_ASPECT)
	{
		g_ConstrainedH = g_SlateH; g_ConstrainedW = g_SlateH * TARGET_ASPECT;
	}
	else
	{
		g_ConstrainedW = g_SlateW; g_ConstrainedH = g_SlateW / TARGET_ASPECT;
	}
}

// The view target's camera component — the one that actually renders.
// GetViewTarget + GetComponentByClass are reflected calls; both verified
// working on this build (the boost is visible in-game), unlike the SDK's
// FOV *setter* dispatch.
static UCameraComponent* GetViewTargetCamera(APlayerController* pc)
{
	if (!pc)
		return nullptr;
	AActor* vt = pc->GetViewTarget();
	if (!vt)
		return nullptr;
	return static_cast<UCameraComponent*>(vt->GetComponentByClass(UCameraComponent::StaticClass()));
}

void DoWork()
{
	if (!g_Ready)
		return;

	bool bIsDimUniverse = false;
	APlayerController* pc = nullptr;
	UWorld* World = UWorld::GetWorld();
	if (!World)
		return;

	pc = UGameplayStatics::GetPlayerController(World, 0);
	if (pc)
	{
		APawn* pawn = pc->AcknowledgedPawn;
		bIsDimUniverse = (pawn && pawn->IsA(APaperCharacter::StaticClass()));
		if (!bIsDimUniverse && pc->IsA(ASevPlayerControllerCharacter::StaticClass()))
		{
			ASevPlayerControllerCharacter* pcChara = static_cast<ASevPlayerControllerCharacter*>(pc);
			if (pcChara->mpPlayer2D)
				bIsDimUniverse = true;
		}
	}

	FVector2D vp = UWidgetLayoutLibrary::GetViewportSize(World);
	if (vp.X > 0)
	{
		float uiScale = UWidgetLayoutLibrary::GetViewportScale(World);
		CalcConstrainedSize(vp, uiScale);
	}

	TUObjectArray* Objects = UObject::GObjects.GetTypedPtr();
	if (!Objects)
		return;

	if (g_CameraClass)
	{
		for (int32 i = 0; i < Objects->Num(); ++i)
		{
			UObject* Obj = Objects->GetByIndex(i);
			if (!Obj || Obj->IsDefaultObject() || !Obj->IsA(g_CameraClass))
				continue;

			UCameraComponent* Cam = static_cast<UCameraComponent*>(Obj);

			Cam->SetConstraintAspectRatio(bIsDimUniverse);

			if (Cam->AspectRatioAxisConstraint != EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV)
			{
				Cam->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);
			}
		}
	}

	// FOV boost: direct member write to the view camera's FieldOfView. Only
	// once the pawn is acknowledged, so menu/cutscene cameras never seed the
	// base. Any readback that is neither our last write nor a recent store
	// (g_WriteHistory) is game-authored: adopt it as the new base (ADS, zooms,
	// cutscene punch-ins) so the boost follows the game instead of fighting it.
	if (!bIsDimUniverse)
	{
		UCameraComponent* vcam = GetViewTargetCamera(pc);
		if (vcam && pc && pc->AcknowledgedPawn)
		{
			const float cur = vcam->FieldOfView;
			if (cur > 0.f)
			{
				if (g_Base == 0.f)
					g_Base = cur; // first gameplay-authored FOV (75)

				bool isOurs = std::fabs(cur - g_LastWritten) < FOV_EPS
					|| std::fabs(cur - BoostFOV(g_Base)) < FOV_EPS;
				if (!isOurs)
				{
					for (int h = 0; h < FOV_WRITE_HISTORY; ++h)
					{
						if (g_WriteHistory[h] != 0.f && std::fabs(cur - g_WriteHistory[h]) < FOV_EPS)
						{
							isOurs = true;
							break;
						}
					}
				}

				if (!isOurs)
					g_Base = cur; // game authored a new FOV: adopt, then re-boost

				const float target = BoostFOV(g_Base);
				if (std::fabs(cur - target) > FOV_EPS)
				{
					vcam->FieldOfView = target; // DIRECT store (plain member)
					g_LastWritten = target;
					g_WriteHistory[g_WriteHistoryIdx] = target;
					g_WriteHistoryIdx = (g_WriteHistoryIdx + 1) % FOV_WRITE_HISTORY;
				}
			}
		}
	}

	if (!g_WidgetClass || g_ConstrainedW <= 0)
		return;

	for (int32 i = 0; i < Objects->Num(); ++i)
	{
		UObject* Obj = Objects->GetByIndex(i);
		if (!Obj || Obj->IsDefaultObject() || !Obj->IsA(g_WidgetClass))
			continue;

		UUserWidget* Widget = static_cast<UUserWidget*>(Obj);

		if (Widget->IsInViewport())
		{
			if (Widget->WidgetTree && Widget->WidgetTree->RootWidget)
			{
				float halfLetterbox = (g_SlateW - g_ConstrainedW) * 0.5f;
				ApplyFullWidthToFadeOrCinemaScope(Widget->WidgetTree->RootWidget, -halfLetterbox);
			}

			bool bFullWidth = (g_FadeClass && Widget->IsA(g_FadeClass))
				|| (g_CinemaScopeClass && Widget->IsA(g_CinemaScopeClass))
				|| (g_CinematicPauseMenuClass && Widget->IsA(g_CinematicPauseMenuClass))
				|| (g_SimplePauseMenuClass && Widget->IsA(g_SimplePauseMenuClass))
				|| (g_PauseMenuOption2Class && Widget->IsA(g_PauseMenuOption2Class));
			if (!bFullWidth)
			{
				FVector2D targetSize = FVector2D{ g_ConstrainedW, g_ConstrainedH };
				Widget->SetDesiredSizeInViewport(targetSize);
				Widget->SetAlignmentInViewport(FVector2D{ 0.5f, 0.5f });
				FAnchors anchors;
				anchors.Minimum = anchors.Maximum = FVector2D{ 0.5f, 0.5f };
				Widget->SetAnchorsInViewport(anchors);
			}
		}
	}
}

// Run DoWork() at most once per ~16 ms (~60 Hz), regardless of widget count.
static std::atomic<uint64_t> g_LastWorkMs{ 0 };

void HookedProcessEvent(UObject* Object, UFunction* Function, void* Parms)
{
	g_OrigProcessEvent(Object, Function, Parms);

	if (!g_Ready || !Function || Function != g_TickFunc)
		return;

	uint64_t now = GetTickCount64();
	uint64_t last = g_LastWorkMs.load(std::memory_order_relaxed);
	if (now - last >= 16)
	{
		g_LastWorkMs.store(now, std::memory_order_relaxed);
		DoWork();
	}
}

bool InstallHook()
{
	uintptr_t base = InSDKUtils::GetImageBase();

	if (MH_Initialize() != MH_OK)
		return false;

	void* peTarget = reinterpret_cast<void*>(base + PROCESSEVENT_RVA);
	if (MH_CreateHook(peTarget, reinterpret_cast<void*>(&HookedProcessEvent), reinterpret_cast<void**>(&g_OrigProcessEvent)) != MH_OK)
	{
		MH_Uninitialize();
		return false;
	}

	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
	{
		MH_Uninitialize();
		return false;
	}

	return true;
}

void MainThread()
{
	LoadFovConfig();

	while (g_Running)
	{
		UWorld* World = UWorld::GetWorld();
		if (World) // World* should be enabled by this time but anyways
		{
			FVector2D vp = UWidgetLayoutLibrary::GetViewportSize(World);
			if (vp.X > 0)
			{
				float uiScale = UWidgetLayoutLibrary::GetViewportScale(World);
				CalcConstrainedSize(vp, uiScale);
				break;
			}
		}
		Sleep(500);
	}
	if (!g_Running)
		return;

	g_WidgetClass     = UUserWidget::StaticClass();
	g_FadeClass       = UWB_Fade_C::StaticClass();
	g_CinemaScopeClass = UWB_CinemaScope_C::StaticClass();
	g_CinematicPauseMenuClass = UWB_CinematicPauseMenu_C::StaticClass();
	g_SimplePauseMenuClass   = UWB_SimplePauseMenu_C::StaticClass();
	g_PauseMenuOption2Class  = UWB_PauseMenu_Option2_C::StaticClass();
	g_CameraClass     = UCameraComponent::StaticClass();
	g_TickFunc        = UUserWidget::StaticClass()->GetFunction("UserWidget", "Tick");

	if (!InstallHook())
		return;

	g_Ready = true;
	while (g_Running)
		Sleep(1000);
	g_Ready = false;

	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();

	FreeLibraryAndExitThread(g_Module, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		g_Module = hModule;
		DisableThreadLibraryCalls(hModule);
		std::thread(MainThread).detach();
	}
	return TRUE;
}
