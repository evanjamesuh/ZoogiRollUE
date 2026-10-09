#include "ZoogiArenaShowcase.h"
#include "ZoogiRoll.h"
#include "ZoogiArena.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
	bool GShowcaseStarted = false;
	constexpr float FreezeDilation = 0.0001f;
}

AZoogiArenaShowcase::AZoogiArenaShowcase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void AZoogiArenaShowcase::MaybeStart(UWorld* World, const FString& ArenaSlug)
{
	if (GShowcaseStarted || !World || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiArenaShots"))) return;
	FString ShotDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), ShotDir)) return;
	GShowcaseStarted = true;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiArenaShowcase* S = World->SpawnActor<AZoogiArenaShowcase>(AZoogiArenaShowcase::StaticClass(), FTransform::Identity, SP);
	if (!S) return;
	S->Slug = ArenaSlug;
	S->Dir = ShotDir;
	S->StartReal = FPlatformTime::Seconds();
	// Name, look-at, yaw (camera sits at -yaw direction from the look point), pitch (down), boom, FOV.
	S->Poses = {
		{ TEXT("aerial"),    FVector(0, 0, 0),     0.f, 83.f, 4000.f, 80.f },  // End aerial
		{ TEXT("tactical"),  FVector(0, 0, 0),     0.f, 55.f, 3600.f, 80.f },  // Tactical
		{ TEXT("standby"),   FVector(0, 0, 0),    35.f, 66.f, 3500.f, 78.f },  // Standby overview
		{ TEXT("hero"),      FVector(0, 0, 120),  20.f, 16.f, 3500.f, 70.f },  // backdrop / sky
		{ TEXT("low34"),     FVector(0, 0, 0),   140.f, 32.f, 2600.f, 75.f },  // low three-quarter
		{ TEXT("rim"),       FVector(1130, 700, 40), 32.f, 30.f, 900.f, 70.f }, // rim rock at 32 deg + edge
		{ TEXT("hub"),       FVector(0, 0, 120), 250.f, 24.f, 1500.f, 70.f },  // hub + pillars / centre orbs
	};
	if (ArenaSlug.Contains(TEXT("meadow")))
	{
		// r5: Buzzy Meadow compositions in the art orientation (screen up = -Y): top-down, 3/4, hero, ramp close-ups.
		S->Poses.Insert(FPose{ TEXT("buzzy_spiral"), FVector(0, -60, 40),     270.f, 42.f, 1700.f, 70.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_lips"),   FVector(0, 1250, 40),     270.f, 30.f, 1900.f, 75.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_ramps"),  FVector(0, -1450, 120),   270.f, 20.f, 2300.f, 72.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_hero"),   FVector(0, -900, 300),    270.f, 15.f, 5200.f, 62.f }, 0); // r23: lower, horizon + sky like v4
		S->Poses.Insert(FPose{ TEXT("match55"),      FVector(0, -100, 0),      270.f, 55.f, 4300.f, 62.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_34"),     FVector(0, -300, -100),   270.f, 29.f, 4400.f, 80.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_top"),    FVector(0, 0, 0),         270.f, 86.f, 5400.f, 62.f }, 0);
	}
	if (ArenaSlug.Contains(TEXT("ice")) || ArenaSlug.Contains(TEXT("frozen")))
	{
		// Buzzy Frozen Ring compositions (screen up = -Y, the art orientation): top-down, 3/4, hero, hub, plus a damaged top.
		S->Poses.Insert(FPose{ TEXT("ref_aerial"),  FVector(0, -400, 700),  270.f, 30.f, 7600.f, 55.f }, 0); // polish r1: frozen_zones_aerial framing
		S->Poses.Insert(FPose{ TEXT("buzzy_hub"),   FVector(0, -40, 110),   270.f, 22.f, 1900.f, 75.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_hero"),  FVector(0, -250, -250), 270.f, 16.f, 5300.f, 70.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_34"),    FVector(0, 0, 0),       270.f, 50.f, 6000.f, 62.f }, 0);
		S->Poses.Insert(FPose{ TEXT("buzzy_top"),   FVector(0, 0, 0),       270.f, 64.f, 5800.f, 62.f }, 0);
		// Stands / bleachers review (islets: W 186 deg d2750, E 354 d2890, S-W 140 d2530, S-E 36 d2690).
		auto IsletLook = [](float Yaw, float D, float Z) { const float R = FMath::DegreesToRadians(Yaw); return FVector(FMath::Cos(R) * D, FMath::Sin(R) * D, Z); };
		S->Poses.Insert(FPose{ TEXT("stands_side_W"),  IsletLook(186.f, 2750.f, 120.f), 276.f, 20.f, 1700.f, 70.f }, 0);
		S->Poses.Insert(FPose{ TEXT("stands_mid_SW"),  IsletLook(140.f, 2530.f, 120.f), 140.f, 28.f, 2300.f, 70.f }, 0);
		S->Poses.Insert(FPose{ TEXT("stands_mid_SE"),  IsletLook(36.f, 2690.f, 120.f),   36.f, 28.f, 2300.f, 70.f }, 0);
		S->Poses.Insert(FPose{ TEXT("stands_close_E"), IsletLook(354.f, 2890.f, 140.f), 354.f, 16.f, 1250.f, 70.f }, 0);
		S->Poses.Insert(FPose{ TEXT("stands_close_W"), IsletLook(186.f, 2750.f, 140.f), 186.f, 16.f, 1250.f, 70.f }, 0);
	}
	if (ArenaSlug.Contains(TEXT("meadow")))
		for (FPose& P : S->Poses) { P.Look *= 1.3f; P.Dist *= 1.3f; } // r19: Meadow is built 1.3x larger
	UE_LOG(LogZoogi, Log, TEXT("Arena showcase: %d views for %s -> %s"), S->Poses.Num(), *ArenaSlug, *ShotDir);
}

void AZoogiArenaShowcase::ApplyPose(int32 Index)
{
	const FPose& P = Poses[Index];
	const FRotator Heading(0.f, P.Yaw, 0.f);
	const float Pr = FMath::DegreesToRadians(P.Pitch);
	const FVector Loc = P.Look - Heading.Vector() * P.Dist * FMath::Cos(Pr) + FVector(0, 0, P.Dist * FMath::Sin(Pr));
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation((P.Look - Loc).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(P.Fov);
}

void AZoogiArenaShowcase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDone) return;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	const double Now = FPlatformTime::Seconds();
	if (PoseIndex < 0)
	{
		// Let a stage-forming intro finish first (Frozen Ring): the views show the formed stage.
		for (TActorIterator<AZoogiArena> It(W); It; ++It)
		{
			if (It->IsForming()) { W->GetWorldSettings()->SetTimeDilation(1.f); CustomTimeDilation = 1.f; StartReal = FMath::Min(StartReal, Now - 2.0); return; }
		}
		// Freeze the match (stage machine, physics, CPU) while we frame the views; this actor runs on real time.
		W->GetWorldSettings()->SetTimeDilation(FreezeDilation);
		CustomTimeDilation = 1.f / FreezeDilation;
		bool bCompiling = false;
#if WITH_EDITOR
		bCompiling = GShaderCompilingManager && GShaderCompilingManager->IsCompiling();
#endif
		const double Waited = Now - StartReal;
		static double MinWait = -1.0;
		if (MinWait < 0.0) { MinWait = 4.0; FParse::Value(FCommandLine::Get(), TEXT("ZoogiShowcaseWait="), MinWait); }
		if ((bCompiling && Waited < 240.0) || Waited < MinWait) return;
		UE_LOG(LogZoogi, Log, TEXT("Arena showcase: starting after %.1f s (compiling=%d)"), Waited, bCompiling ? 1 : 0);
		Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		PrevView = PC->GetViewTarget();
		PC->SetViewTarget(Cam);
		PoseIndex = 0;
		PoseStart = Now;
		bShotTaken = false;
		ApplyPose(0);
		return;
	}
	PC->SetViewTarget(Cam); // something else (rock test cam, anime) may have taken the view
	ApplyPose(PoseIndex);
	const double T = Now - PoseStart;
	if (!bShotTaken && T > 1.6)
	{
		const FString Path = FPaths::Combine(Dir, FString::Printf(TEXT("arena_%s_%d_%s.png"), *Slug, PoseIndex, Poses[PoseIndex].Name));
		FScreenshotRequest::RequestScreenshot(Path, !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHUD")), false); // r15: -ZoogiNoHUD = viewport only
		UE_LOG(LogZoogi, Log, TEXT("Arena showcase screenshot: %s"), *Path);
		bShotTaken = true;
	}
	if (T > 2.4)
	{
		if (++PoseIndex >= Poses.Num()) { Finish(); return; }
		PoseStart = Now;
		bShotTaken = false;
		ApplyPose(PoseIndex);
	}
}

void AZoogiArenaShowcase::Finish()
{
	bDone = true;
	UWorld* W = GetWorld();
	if (W) W->GetWorldSettings()->SetTimeDilation(1.f);
	CustomTimeDilation = 1.f;
	if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
	{
		if (PrevView.IsValid()) PC->SetViewTarget(PrevView.Get());
	}
	if (Cam) Cam->Destroy();
	UE_LOG(LogZoogi, Log, TEXT("Arena showcase: done, match continues"));
	SetActorTickEnabled(false);
}
