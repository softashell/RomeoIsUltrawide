#include "SDK.hpp"
#include <thread>
#include <atomic>
#include <cstdio>
#include <share.h>
#include <MinHook.h>

using namespace SDK;

constexpr float TARGET_ASPECT = 16.0f / 9.0f;
constexpr int GENGINE_TICK_VTABLE = 94;        // qword_14C033718 + 752LL uhh
constexpr uintptr_t GENGINE_PTR_RVA = 0xC033718; // qword_14C033718

std::atomic<bool> g_Running{ true };
std::atomic<bool> g_Ready{ false };
HMODULE g_Module = nullptr;
float g_ConstrainedW = 0;
float g_ConstrainedH = 0;
float g_SlateW = 0;
float g_SlateH = 0;
UClass* g_WidgetClass = nullptr;
UClass* g_FadeClass = nullptr;
UClass* g_CameraClass = nullptr;
FILE* g_LogFile = nullptr;

typedef void (*FEngineTick)(UEngine*, float, bool);
FEngineTick g_OrigTick = nullptr;
void* g_TickTarget = nullptr;

void Log(const char* fmt, ...)
{
	if (!g_LogFile) return;
	va_list args;
	va_start(args, fmt);
	fprintf(g_LogFile, "[RIADM_UI_Semi_Fix] ");
	vfprintf(g_LogFile, fmt, args);
	fprintf(g_LogFile, "\n");
	fflush(g_LogFile);
	va_end(args);
}

UWorld* GetWorld() 
{
	auto ptr = reinterpret_cast<UWorld**>(InSDKUtils::GetImageBase() + Offsets::GWorld);
	return ptr ? *ptr : nullptr;
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

void HookedEngineTick(UEngine* Engine, float DeltaSeconds, bool bIdleMode) 
{
	g_OrigTick(Engine, DeltaSeconds, bIdleMode);

	if (!g_Ready) 
		return;

	bool bIsDimUniverse = false;
	UWorld* World = GetWorld();
	if (World) 
	{
		bIsDimUniverse = (World->GetName() == "OWL_DimensionalUniverse");
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
			bool bIsFade = g_FadeClass && Widget->IsA(g_FadeClass);
			FVector2D targetSize = bIsFade
				? FVector2D{ g_SlateW, g_SlateH }
				: FVector2D{ g_ConstrainedW, g_ConstrainedH };
			Widget->SetDesiredSizeInViewport(targetSize);
			Widget->SetAlignmentInViewport(FVector2D{ 0.5f, 0.5f });
			FAnchors anchors;
			anchors.Minimum = anchors.Maximum = FVector2D{ 0.5f, 0.5f };
			Widget->SetAnchorsInViewport(anchors);
		}
	}
}

bool InstallHook() 
{
	uintptr_t base = InSDKUtils::GetImageBase();
	UEngine* Engine = *reinterpret_cast<UEngine**>(base + GENGINE_PTR_RVA);

	if (!Engine) 
	{ 
		Log("ERROR: GEngine is null"); return false;
	}

	void** vtable = *reinterpret_cast<void***>(Engine);
	g_TickTarget = vtable[GENGINE_TICK_VTABLE];
	Log("GEngine: 0x%p  vtable[%d]: 0x%p", Engine, GENGINE_TICK_VTABLE, g_TickTarget);

	if (MH_Initialize() != MH_OK) 
	{
		Log("ERROR: MH_Initialize failed"); return false; 
	}

	if (MH_CreateHook(g_TickTarget, &HookedEngineTick, reinterpret_cast<void**>(&g_OrigTick)) != MH_OK || MH_EnableHook(g_TickTarget) != MH_OK) 
	{
		Log("ERROR: Hook creation failed");
		MH_Uninitialize();
		return false;
	}

	Log("Hook installed (trampoline: 0x%p)", g_OrigTick);
	return true;
}

void MainThread() 
{
	// waiting for accessible to spin up
	while (g_Running) 
	{
		UWorld* World = GetWorld();
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

	Log("Constrained size: %.0fx%.0f", g_ConstrainedW, g_ConstrainedH);
	g_WidgetClass     = UUserWidget::StaticClass();
	g_FadeClass       = UWB_Fade_C::StaticClass();
	g_CameraClass     = UCameraComponent::StaticClass();

	if (!InstallHook()) 
		return; // uh uh, bad thing :(

	g_Ready = true;
	
	Log("Running.");
	
	while (g_Running)
	{
		Sleep(1000);
	}

	g_Ready = false;
	
	MH_DisableHook(g_TickTarget);
	MH_Uninitialize();
	
	Log("Unloaded.");

	fclose(g_LogFile);
	g_LogFile = nullptr;

	FreeLibraryAndExitThread(g_Module, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		g_Module = hModule;
		DisableThreadLibraryCalls(hModule);

		char logPath[MAX_PATH];
		GetModuleFileNameA(hModule, logPath, MAX_PATH);

		char* slash = strrchr(logPath, '\\');
		if (slash) strcpy_s(slash + 1, MAX_PATH - (slash - logPath) - 1, "sevwidefixes.log");
		g_LogFile = _fsopen(logPath, "w", _SH_DENYWR);

		std::thread(MainThread).detach();
	}
	return TRUE;
}
