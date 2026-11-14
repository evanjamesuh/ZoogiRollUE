#include "ZoogiRoll.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMemory.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectArray.h"
#include "CoreGlobals.h"

DEFINE_LOG_CATEGORY(LogZoogi);

// Nov 11 (Claude) B252: the iPhone build closes at launch (FUObjectArray::AllocateUObjectIndex fatal in FEngineLoop::Init) and no
// crash report / log reaches us. This writes Documents/ZoogiLaunch.txt (visible in the Files app: On My iPhone > Zoogi Roll):
// the object-pool numbers at module start, and on any fatal error the engine's own error message.
namespace ZoogiLaunchDiag
{
	static FString Path() { return FString(FPlatformProcess::UserDir()) / TEXT("ZoogiLaunch.txt"); }
	static void Write(const FString& S, bool bAppend)
	{
		FFileHelper::SaveStringToFile(S + TEXT("\n"), *Path(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), bAppend ? FILEWRITE_Append : FILEWRITE_None);
	}
	static FString Pool(const TCHAR* Tag)
	{
		int32 InGame = -1, NotGC = -1;
		if (GConfig) { GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsInGame"), InGame, GEngineIni); GConfig->GetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsNotConsideredByGC"), NotGC, GEngineIni); }
		const FPlatformMemoryStats M = FPlatformMemory::GetStats();
		return FString::Printf(TEXT("%s %s | objects %d of capacity %d | disregard pool open %d, enabled %d, first GC index %d | ini MaxObjectsInGame %d, MaxObjectsNotConsideredByGC %d | mem used %llu MB, avail %llu MB"),
			*FDateTime::Now().ToString(), Tag, GUObjectArray.GetObjectArrayNum(), GUObjectArray.GetObjectArrayCapacity(), GUObjectArray.IsOpenForDisregardForGC() ? 1 : 0,
			GUObjectArray.DisregardForGCEnabled() ? 1 : 0, GUObjectArray.GetFirstGCIndex(), InGame, NotGC, (unsigned long long)(M.UsedPhysical >> 20), (unsigned long long)(M.AvailablePhysical >> 20));
	}
}

class FZoogiRollModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		ZoogiLaunchDiag::Write(TEXT("=== Zoogi Roll launch (build 252) ===\n") + ZoogiLaunchDiag::Pool(TEXT("module start")), false);
		FCoreDelegates::OnHandleSystemError.AddLambda([]()
		{
			ZoogiLaunchDiag::Write(FString(TEXT("FATAL: ")) + GErrorHist + TEXT("\n") + ZoogiLaunchDiag::Pool(TEXT("at fatal")), true);
		});
		FCoreDelegates::OnPostEngineInit.AddLambda([]() { ZoogiLaunchDiag::Write(ZoogiLaunchDiag::Pool(TEXT("engine init done")), true); });
		FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([]() { ZoogiLaunchDiag::Write(ZoogiLaunchDiag::Pool(TEXT("launch complete")), true); });
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FZoogiRollModule, ZoogiRoll, "ZoogiRoll");
