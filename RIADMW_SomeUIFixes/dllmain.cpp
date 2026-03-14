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
UClass* g_WidgetClass = nullptr;
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
	float slateW = vp.X / uiScale;
	float slateH = vp.Y / uiScale;
	float aspect = slateW / slateH;

	if (aspect > TARGET_ASPECT) 
	{ 
		g_ConstrainedH = slateH; g_ConstrainedW = slateH * TARGET_ASPECT; 
	}
	else 
	{ 
		g_ConstrainedW = slateW; g_ConstrainedH = slateW / TARGET_ASPECT; 
	}
}

void HookedEngineTick(UEngine* Engine, float DeltaSeconds, bool bIdleMode) 
{
	g_OrigTick(Engine, DeltaSeconds, bIdleMode);

	if (!g_Ready) 
		return;

	UWorld* World = GetWorld();
	if (World) 
	{
		FVector2D vp = UWidgetLayoutLibrary::GetViewportSize(World);
		if (vp.X > 0) 
		{
			float uiScale = UWidgetLayoutLibrary::GetViewportScale(World);
			float slateW = vp.X / uiScale;
			float slateH = vp.Y / uiScale;
			float aspect = slateW / slateH;
			float newW = (aspect > TARGET_ASPECT) ? slateH * TARGET_ASPECT : slateW;
			float newH = (aspect > TARGET_ASPECT) ? slateH : slateW / TARGET_ASPECT;
			if (newW != g_ConstrainedW || newH != g_ConstrainedH) {
				g_ConstrainedW = newW;
				g_ConstrainedH = newH;
				Log("Viewport changed: %.0fx%.0f (scale %.2f) -> Constrained: %.0fx%.0f",
					vp.X, vp.Y, uiScale, g_ConstrainedW, g_ConstrainedH);
			}
		}
	}

	TUObjectArray* Objects = UObject::GObjects.GetTypedPtr();
	if (!Objects || !g_WidgetClass || g_ConstrainedW <= 0)
		return;

	for (int32 i = 0; i < Objects->Num(); ++i) 
	{
		UObject* Obj = Objects->GetByIndex(i);
		if (!Obj || Obj->IsDefaultObject() || !Obj->IsA(g_WidgetClass)) 
			continue;

		UUserWidget* Widget = static_cast<UUserWidget*>(Obj);
		if (!Widget->IsInViewport()) 
			continue;

		Widget->SetDesiredSizeInViewport(FVector2D{ g_ConstrainedW, g_ConstrainedH });
		Widget->SetAlignmentInViewport(FVector2D{ 0.5f, 0.5f });
		FAnchors anchors;
		anchors.Minimum = anchors.Maximum = FVector2D{ 0.5f, 0.5f };
		Widget->SetAnchorsInViewport(anchors);
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
	g_WidgetClass = UUserWidget::StaticClass();

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
