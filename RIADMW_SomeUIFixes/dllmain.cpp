#include "SDK.hpp"
#include <thread>
#include <atomic>
#include <MinHook.h>

using namespace SDK;

constexpr float TARGET_ASPECT = 16.0f / 9.0f;
constexpr uintptr_t PROCESSEVENT_RVA = 0x01821740; // from OffsetsInfo.json (new build)

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

void DoWork()
{
	if (!g_Ready)
		return;

	bool bIsDimUniverse = false;
	UWorld* World = UWorld::GetWorld();
	if (!World)
		return;
	if (World)
	{
		APlayerController* pc = UGameplayStatics::GetPlayerController(World, 0);
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

	void* Target = reinterpret_cast<void*>(base + PROCESSEVENT_RVA);

	if (MH_CreateHook(Target, reinterpret_cast<void*>(&HookedProcessEvent), reinterpret_cast<void**>(&g_OrigProcessEvent)) != MH_OK || MH_EnableHook(Target) != MH_OK)
	{
		MH_Uninitialize();
		return false;
	}
	return true;
}

void MainThread()
{
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
