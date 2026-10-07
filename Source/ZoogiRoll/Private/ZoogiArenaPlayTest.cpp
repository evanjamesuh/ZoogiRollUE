// Generic arena gameplay checks (-ZoogiArenaPlayTest). See ZoogiArenaPlayTest.h.
#include "ZoogiArenaPlayTest.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

AZoogiArenaPlayTest::AZoogiArenaPlayTest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AZoogiArenaPlayTest* AZoogiArenaPlayTest::MaybeSpawn(AZoogiArena* InArena)
{
	if (!InArena || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiArenaPlayTest"))) return nullptr;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiArenaPlayTest* PT = InArena->GetWorld()->SpawnActor<AZoogiArenaPlayTest>(AZoogiArenaPlayTest::StaticClass(), FTransform::Identity, SP);
	if (PT)
	{
		PT->Arena = InArena;
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), PT->ShotDir);
	}
	return PT;
}

int32 AZoogiArenaPlayTest::CountDamaged(FName Kind, const FVector& Near, float Radius) const
{
	int32 N = 0;
	if (!Arena.IsValid()) return 0;
	for (const AZoogiArenaPiece* P : Arena->GetPieces())
		if (P && P->GetKind() == Kind && P->GetPieceState() != EZoogiPieceState::Intact && FVector::Dist2D(P->GetActorLocation(), Near) < Radius) ++N;
	return N;
}

void AZoogiArenaPlayTest::View(const FVector& Look, float Yaw, float Pitch, float Dist)
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	if (!Cam) Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
	if (!Cam) return;
	const FRotator H(0.f, Yaw, 0.f);
	const float Pr = FMath::DegreesToRadians(Pitch);
	const FVector Loc = Look - H.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation((Look - Loc).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(70.f);
	PC->SetViewTarget(Cam);
}

void AZoogiArenaPlayTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W || !Arena.IsValid() || Step >= Checks.Num()) return;
	const double Now = W->GetTimeSeconds();
	if (!bStarted)
	{
		const bool bFormed = !Arena->Forming || Arena->Forming->GetProgress() >= 1.f;
		if (!bFormed) { StepStart = Now; return; }
		if (Now - StepStart < 2.0) return;
		for (TActorIterator<AZoogiBall> It(W); It; ++It) if (It->PlayerIndex == 3) TestBall = *It;
		if (!TestBall.IsValid()) for (TActorIterator<AZoogiBall> It(W); It; ++It) TestBall = *It;
		for (TActorIterator<AZoogiOrb> It(W); It; ++It) if (!It->bOut) TestOrb = *It;
		bStarted = true; Step = 0; StepStart = Now; bShot = false; Before = -1;
		UE_LOG(LogZoogi, Log, TEXT("ARENATEST: %d checks (ball %d, orb %d)"), Checks.Num(), TestBall.IsValid() ? 1 : 0, TestOrb.IsValid() ? 1 : 0);
	}
	const FZoogiArenaCheck& C = Checks[Step];
	const double T = Now - StepStart;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(C.Vel.Y, C.Vel.X));
	auto Next = [&](const FString& Res)
	{
		Results.Add(Res);
		UE_LOG(LogZoogi, Log, TEXT("ARENATEST %s"), *Res);
		++Step; StepStart = Now; bShot = false; Before = -1;
		if (Step >= Checks.Num())
		{
			UE_LOG(LogZoogi, Log, TEXT("ARENATEST SUMMARY: %s"), *FString::Join(Results, TEXT(" | ")));
			if (APlayerController* PC = W->GetFirstPlayerController()) if (APawn* Pw = PC->GetPawn()) PC->SetViewTarget(Pw);
		}
	};
	if (C.Kind == FZoogiArenaCheck::ZoogiLaunch)
	{
		AZoogiBall* B = TestBall.Get();
		if (!B) { Next(C.Name + TEXT(": no Zoogi FAIL")); return; }
		if (Before < 0)
		{
			Before = CountDamaged(C.PieceKind, C.Near, 400.f);
			B->PlaceAt(C.From + FVector(0, 0, ZoogiConst::ZoogiRadius + 1.f), Yaw);
			B->Launch(C.Vel);
			StartZ = MaxZ = B->GetActorLocation().Z;
			View(C.Near + FVector(0, 0, 60.f), Yaw + 90.f, 18.f, 1500.f);
		}
		MaxZ = FMath::Max(MaxZ, (float)B->GetActorLocation().Z);
		if (T > 0.8 && !bShot && !ShotDir.IsEmpty()) { bShot = true; FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotDir, FString::Printf(TEXT("arenatest_%d_%s.png"), Step, *C.Name)), false, false); }
		if (T > C.Wait)
		{
			const float Rise = MaxZ - StartZ;
			const int32 N = CountDamaged(C.PieceKind, C.Near, 400.f);
			Next(FString::Printf(TEXT("%s: %.0f cm/s Zoogi roll -> peak rise %.0f cm (need %.0f), %s pieces damaged %d->%d %s"), *C.Name, C.Vel.Size(), Rise, C.Radius, *C.PieceKind.ToString(), Before, N, Rise >= C.Radius ? TEXT("PASS") : TEXT("FAIL")));
		}
		return;
	}
	if (C.Kind == FZoogiArenaCheck::ZoogiDamages)
	{
		AZoogiBall* B = TestBall.Get();
		if (!B) { Next(C.Name + TEXT(": no Zoogi FAIL")); return; }
		if (Before < 0)
		{
			Before = CountDamaged(C.PieceKind, C.Near, C.Radius);
			B->PlaceAt(C.From + FVector(0, 0, ZoogiConst::ZoogiRadius + 1.f), Yaw);
			B->Launch(C.Vel);
			View(C.Near + FVector(0, 0, 40.f), Yaw, 24.f, 1200.f);
		}
		if (T > 0.9 && !bShot && !ShotDir.IsEmpty()) { bShot = true; FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotDir, FString::Printf(TEXT("arenatest_%d_%s.png"), Step, *C.Name)), false, false); }
		if (T > C.Wait)
		{
			const int32 N = CountDamaged(C.PieceKind, C.Near, C.Radius);
			Next(FString::Printf(TEXT("%s: %.0f cm/s Zoogi shot -> %d %s piece(s) damaged or worse (before %d) %s"), *C.Name, C.Vel.Size(), N, *C.PieceKind.ToString(), Before, N > Before ? TEXT("PASS") : TEXT("FAIL")));
		}
		return;
	}
	AZoogiOrb* O = TestOrb.Get();
	if (!O) { Next(C.Name + TEXT(": no orb FAIL")); return; }
	if (Before < 0)
	{
		Before = 0;
		O->Respawn(C.From + FVector(0, 0, ZoogiConst::OrbRadius + 1.f));
		O->Sphere->SetPhysicsLinearVelocity(FVector(C.Vel, 0.f));
		View(C.From + FVector(C.Vel.GetSafeNormal() * 300.f, 0.f), Yaw + 90.f, 30.f, 1600.f);
	}
	if (T > 0.7 && !bShot && !ShotDir.IsEmpty()) { bShot = true; FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotDir, FString::Printf(TEXT("arenatest_%d_%s.png"), Step, *C.Name)), false, false); }
	if (T > C.Wait)
	{
		const FVector L = O->OrbLocation();
		const bool bOutRule = O->bOut || L.Z < ZoogiConst::FallOutZ || FVector2D(L.X, L.Y).Size() > 8000.f;
		const bool bPass = (C.Kind == FZoogiArenaCheck::OrbOut) == bOutRule;
		Next(FString::Printf(TEXT("%s: orb %.0f cm/s from (%.0f, %.0f) -> ends (%.0f, %.0f, %.0f) r %.0f -> %s (expected %s) %s"), *C.Name, C.Vel.Size(), C.From.X, C.From.Y,
			L.X, L.Y, L.Z, FVector2D(L.X, L.Y).Size(), bOutRule ? TEXT("ORB OUT") : TEXT("in play"), C.Kind == FZoogiArenaCheck::OrbOut ? TEXT("ORB OUT") : TEXT("in play"), bPass ? TEXT("PASS") : TEXT("FAIL")));
		// Put the orb back on the court so the match is unaffected.
		O->Respawn(FVector(0.f, -300.f, ZoogiConst::OrbRadius + 1.f));
	}
}
