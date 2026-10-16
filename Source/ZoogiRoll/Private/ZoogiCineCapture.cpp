// See ZoogiCineCapture.h. Capture-only: nothing here runs unless -ZoogiCineCapture=<shot> is on the command line.
#include "ZoogiCineCapture.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
	float Smooth(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }
	float Smoother(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * X * (X * (X * 6.f - 15.f) + 10.f); }
	FVector Damp(const FVector& A, const FVector& B, float K, float Dt) { return A + (B - A) * (1.f - FMath::Exp(-K * Dt)); }
	FVector Flat(const FVector& V) { return FVector(V.X, V.Y, 0.f); }
}

bool UZoogiCineCapture::IsRequested()
{
	FString S;
	return FParse::Value(FCommandLine::Get(), TEXT("ZoogiCineCapture="), S) && !S.IsEmpty();
}

bool UZoogiCineCapture::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return IsRequested() && W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UZoogiCineCapture::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("ZoogiCineCapture="), ShotName, false);
	ShotName = ShotName.ToLower();
	ShotName.ParseIntoArray(ShotList, TEXT(","));
	if (ShotList.Num() == 0) ShotList.Add(TEXT("track"));
	SetShot(ShotList[0]);
	FParse::Value(Cmd, TEXT("ZoogiCineTake="), Take);
	FParse::Value(Cmd, TEXT("ZoogiCineSlow="), Slow);
	Slow = FMath::Clamp(Slow, 1, 8);
	FParse::Value(Cmd, TEXT("ZoogiCineMaxTurns="), MaxTurns);
	FParse::Value(Cmd, TEXT("ZoogiCineMaxSeconds="), MaxReal);
	FString TurnsArg = TEXT("1"), Focus;
	FParse::Value(Cmd, TEXT("ZoogiCineTurns="), TurnsArg);
	bAllTurns = TurnsArg.Equals(TEXT("all"), ESearchCase::IgnoreCase);
	if (!bAllTurns) { TArray<FString> Parts; TurnsArg.ParseIntoArray(Parts, TEXT(",")); for (const FString& P : Parts) Turns.Add(FCString::Atoi(*P)); }
	if (FParse::Value(Cmd, TEXT("ZoogiCineFocus="), Focus)) bPowerOnly = Focus.Equals(TEXT("power"), ESearchCase::IgnoreCase);
	if (!FParse::Value(Cmd, TEXT("ZoogiCineOut="), OutRoot)) OutRoot = FPaths::ProjectSavedDir() / TEXT("CineCapture") / FString::Printf(TEXT("take%d"), Take);
	Side = (Take % 2 == 1) ? 1.f : -1.f;
	StartReal = FPlatformTime::Seconds();
	UE_LOG(LogZoogi, Log, TEXT("CINECAP on: shot %s take %d slow x%d launches %s%s -> %s"), *ShotName, Take, Slow, *TurnsArg, bPowerOnly ? TEXT(" (power launches only)") : TEXT(""), *OutRoot);
}

void UZoogiCineCapture::SetShot(const FString& N)
{
	CurShot = N;
	Shot = N == TEXT("pushin") ? EShot::PushIn : N == TEXT("orbit") ? EShot::Orbit : N == TEXT("crane") ? EShot::Crane
		: N == TEXT("whip") ? EShot::Whip : N == TEXT("ringout") ? EShot::RingOut : EShot::Track;
}

void UZoogiCineCapture::Deinitialize()
{
	if (bRec) { FApp::SetUseFixedTimeStep(false); bRec = false; }
	Super::Deinitialize();
}

AZoogiGameMode* UZoogiCineCapture::GM() const
{
	return GetWorld() ? Cast<AZoogiGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
}

bool UZoogiCineCapture::WantLaunch(int32 N, AZoogiGameMode* G) const
{
	if (bPowerOnly)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B || !B->Artifact || B->Artifact->Infused == EZoogiArtifact::None) return false;
	}
	return bAllTurns ? Filmed < MaxTurns : Turns.Contains(N);
}

void UZoogiCineCapture::Event(const FString& What)
{
	Events.Add(FString::Printf(TEXT("frame %d t %.3f %s"), Frame, T, *What));
	UE_LOG(LogZoogi, Log, TEXT("CINECAP launch %d frame %d t %.3f: %s"), LaunchNo, Frame, T, *What);
}

void UZoogiCineCapture::Begin(AZoogiGameMode* G)
{
	Shooter = G->GetActiveBall();
	AActor* Lock = G->TargetLock.Get();
	Target = Cast<AZoogiBall>(Lock);
	AZoogiBall* S = Shooter.Get();
	const FVector SL = S ? S->BallLocation() : FVector::ZeroVector;
	if (!Target.IsValid())
	{
		// no lock: the nearest live rival
		float Best = 1e12f;
		for (AZoogiBall* B : G->Balls) if (B && B != S && !B->bOut) { const float D = FVector::DistSquared(B->BallLocation(), SL); if (D < Best) { Best = D; Target = B; } }
	}
	const FVector TL = Target.IsValid() ? Target->BallLocation() : (Lock ? Lock->GetActorLocation() : FVector::ZeroVector);
	AimDir = Flat(TL - SL).GetSafeNormal();
	if (AimDir.IsNearlyZero()) AimDir = Flat(-SL).GetSafeNormal();
	if (AimDir.IsNearlyZero()) AimDir = FVector::ForwardVector;
	VelDir = AimDir;
	StartPos.Reset();
	for (AZoogiBall* B : G->Balls) if (B) StartPos.Add(B, B->BallLocation());
	Struck = nullptr; Faller = nullptr;
	T = 0.f; LaunchT = ImpactT = OutT = ResolveT = -1.f; Frame = 0; RigN = 0; Events.Reset(); bDumping = false;
	ImpactPos = TL; OutDir = AimDir;
	SetShot(ShotList[Filmed % ShotList.Num()]); // shots cycle over the filmed launches
	Side = ((Take + Filmed / ShotList.Num()) % 2 == 1) ? 1.f : -1.f;
	Dir = OutRoot / FString::Printf(TEXT("launch%02d_%s"), LaunchNo, *CurShot);
	IFileManager::Get().MakeDirectory(*Dir, true);
	if (!Cam)
	{
		FActorSpawnParameters P; P.ObjectFlags |= RF_Transient;
		Cam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(0, 0, 1000), FRotator(-45, 0, 0), P);
		if (Cam) Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	CamLoc = SL + FVector(0, 0, 300); LookAt = SL;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PrevVT = PC->GetViewTarget();
	bRec = true;
	FApp::SetUseFixedTimeStep(true);
	FApp::SetFixedDeltaTime(1.0 / (60.0 * Slow));
	GEngine->Exec(GetWorld(), TEXT("DisableAllScreenMessages"));
	Event(FString::Printf(TEXT("powerup shot %s shooter %s target %s artifact %d fps_game %d"), *CurShot, *GetNameSafe(S), *GetNameSafe(Target.Get()),
		(S && S->Artifact) ? (int32)S->Artifact->Infused : -1, 60 * Slow));
}

void UZoogiCineCapture::End(const TCHAR* Why)
{
	Event(FString::Printf(TEXT("end (%s) frames %d"), Why, Frame));
	FFileHelper::SaveStringArrayToFile(Events, *(Dir / TEXT("events.txt")));
	bRec = false;
	FApp::SetUseFixedTimeStep(false);
	++Filmed;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && Cam && PC->GetViewTarget() == Cam)
		if (PrevVT.IsValid()) PC->SetViewTarget(PrevVT.Get());
	const bool bMore = bAllTurns ? Filmed < MaxTurns : Turns.ContainsByPredicate([&](int32 N) { return N > LaunchNo; });
	if (!bMore && !bQuitting) { bQuitting = true; QuitAt = FPlatformTime::Seconds() + 1.0; UE_LOG(LogZoogi, Log, TEXT("CINECAP done: %d launches filmed"), Filmed); }
}

void UZoogiCineCapture::Apply(const FVector& Loc, const FVector& Look, float Fov, float Fstop, float Blur)
{
	if (!Cam) return;
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation((Look - Loc).Rotation());
	UCameraComponent* C = Cam->GetCameraComponent();
	C->SetFieldOfView(Fov);
	FPostProcessSettings& PP = C->PostProcessSettings;
	PP.bOverride_DepthOfFieldFocalDistance = true; PP.DepthOfFieldFocalDistance = FMath::Max(50.f, FVector::Dist(Loc, Look));
	PP.bOverride_DepthOfFieldFstop = true; PP.DepthOfFieldFstop = Fstop;
	PP.bOverride_MotionBlurAmount = true; PP.MotionBlurAmount = Blur;
	C->PostProcessBlendWeight = 1.f;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); PC && PC->GetViewTarget() != Cam) PC->SetViewTarget(Cam);
}

void UZoogiCineCapture::Rig(AZoogiGameMode* G, float Dt)
{
	AZoogiBall* S = Shooter.Get();
	const FVector SL = S ? S->BallLocation() : FVector::ZeroVector;
	const FVector SV = S ? S->GetBallVelocity() : FVector::ZeroVector;
	const float Speed = Flat(SV).Size();
	if (Speed > 80.f) VelDir = Damp(VelDir, Flat(SV).GetSafeNormal(), 6.f, Dt).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, VelDir).GetSafeNormal();
	const FVector TL = Target.IsValid() ? Target->BallLocation() : ImpactPos;
	const float R = AZoogiPlayerController::PlayRadius(G ? G->Arena : nullptr);
	const float Pg = LaunchT < 0.f ? T : T - LaunchT; // seconds into the current beat
	switch (Shot)
	{
	case EShot::Track:
	{
		// Low ground-level tracking beside the rolling Zoogi: right at floor level, a little behind, long-lens.
		const float H = 28.f + 6.f * (Take - 1);
		FVector Want = SL + Right * (Side * 360.f) - VelDir * (LaunchT < 0.f ? 260.f : 140.f) + FVector(0, 0, H - ZoogiConst::ZoogiRadius);
		if (LaunchT >= 0.f && Speed < 40.f && T - LaunchT > 1.f) Want.Z += 120.f; // settled: lift off the floor
		CamLoc = (RigN == 0) ? Want : Damp(CamLoc, Want, 7.f, Dt);
		CamLoc.Z = FMath::Max(CamLoc.Z, 22.f);
		LookAt = (RigN == 0) ? SL : Damp(LookAt, SL + VelDir * 120.f, 10.f, Dt);
		Apply(CamLoc, LookAt, 42.f, 2.8f, 0.6f);
		break;
	}
	case EShot::PushIn:
	{
		// Slow push-in on the charging Zoogi from a low front three-quarter, then pan with it after launch.
		const FVector Front = (AimDir.RotateAngleAxis(Side * (35.f + 10.f * (Take > 2)), FVector::UpVector)).GetSafeNormal();
		const float A = LaunchT < 0.f ? Smoother(T / 1.6f) : 1.f;
		const float D = FMath::Lerp(820.f, 330.f, A);
		const FVector Base = (StartPos.Contains(Shooter) ? StartPos[Shooter] : SL);
		const FVector Want = Base + Front * D + FVector(0, 0, FMath::Lerp(120.f, 40.f, A));
		if (LaunchT < 0.f) CamLoc = Want; // after launch the camera stays put and pans with the Zoogi
		LookAt = (LaunchT < 0.f) ? SL + FVector(0, 0, 10.f) : Damp(LookAt, SL, 5.f, Dt);
		Apply(CamLoc, LookAt, FMath::Lerp(48.f, 36.f, A), 2.0f, 0.5f);
		break;
	}
	case EShot::Orbit:
	{
		// Orbit / 360 around the impact: centred on the predicted contact until the first impact, then on the impact point.
		const FVector C = (ImpactT >= 0.f ? ImpactPos : (LaunchT < 0.f ? (SL + TL) * 0.5f : TL)) + FVector(0, 0, 30.f);
		const float Rad = 640.f - 40.f * (Take - 1), Hgt = 170.f + 30.f * (Take - 1);
		const float Spin = Side * (ImpactT >= 0.f ? 72.f : 30.f);
		float& Ang = OrbitAng;
		if (RigN == 0) Ang = FMath::RadiansToDegrees(FMath::Atan2(-AimDir.Y, -AimDir.X)) + Side * 70.f;
		Ang += Spin * Dt;
		const FVector Want = C + FVector(FMath::Cos(FMath::DegreesToRadians(Ang)), FMath::Sin(FMath::DegreesToRadians(Ang)), 0.f) * Rad + FVector(0, 0, Hgt);
		CamLoc = (RigN == 0) ? Want : Damp(CamLoc, Want, 5.f, Dt);
		LookAt = (RigN == 0) ? C : Damp(LookAt, C, 6.f, Dt);
		Apply(CamLoc, LookAt, 50.f, 3.2f, 0.5f);
		break;
	}
	case EShot::Crane:
	{
		// High crane reveal: from floor level at the rim behind the action, rising and pulling back over the whole arena.
		const float A = Smoother(T / 6.5f);
		const FVector Mid = (SL + TL) * 0.5f;
		const FVector Back = (Flat(Mid).IsNearlyZero() ? -AimDir : Flat(Mid).GetSafeNormal()).RotateAngleAxis(Side * 25.f, FVector::UpVector);
		const FVector Start = Mid + Back * 520.f + FVector(0, 0, 45.f);
		const FVector Top = Back * (R * 0.85f) + FVector(0, 0, 2600.f + 200.f * (Take - 1));
		CamLoc = FMath::Lerp(Start, Top, A);
		LookAt = FMath::Lerp(Mid, FVector(0, 0, 0), A);
		Apply(CamLoc, LookAt, FMath::Lerp(55.f, 68.f, A), FMath::Lerp(2.8f, 16.f, A), 0.4f);
		break;
	}
	case EShot::Whip:
	{
		// Whip-pan from attacker to target: side-on, framed on the attacker through the power-up; right after launch a
		// 0.22 s whip to the target (strong ease, full motion blur), then hold on the target / impact.
		const FVector Mid = ((StartPos.Contains(Shooter) ? StartPos[Shooter] : SL) + TL) * 0.5f;
		const FVector Perp = FVector::CrossProduct(FVector::UpVector, AimDir).GetSafeNormal() * Side;
		const float Span = FVector::Dist2D(SL, TL);
		CamLoc = Mid + Perp * FMath::Clamp(Span * 0.55f, 420.f, 900.f) - AimDir * 120.f + FVector(0, 0, 70.f);
		const float W = LaunchT < 0.f ? 0.f : Smoother((T - LaunchT - 0.12f) / 0.22f);
		const FVector Look0 = SL, Look1 = ImpactT >= 0.f ? ImpactPos : TL;
		const FVector D0 = (Look0 - CamLoc).GetSafeNormal(), D1 = (Look1 - CamLoc).GetSafeNormal();
		const FVector D = FMath::Lerp(D0, D1, W).GetSafeNormal();
		const float Blur = (W > 0.02f && W < 0.98f) ? 1.f : 0.35f;
		Apply(CamLoc, CamLoc + D * FMath::Lerp(FVector::Dist(CamLoc, Look0), FVector::Dist(CamLoc, Look1), W), FMath::Lerp(34.f, 42.f, W), 2.4f, Blur);
		break;
	}
	case EShot::RingOut:
	{
		// Dramatic ring-out: a three-quarter chase on the shooter; once a Zoogi leaves play, a camera over the rim above
		// the exit point looks down and follows the faller into the drop.
		AZoogiBall* F = Faller.Get();
		if (!F)
		{
			const FVector Want = SL - VelDir * 420.f + Right * (Side * 260.f) + FVector(0, 0, 230.f);
			CamLoc = (RigN == 0) ? Want : Damp(CamLoc, Want, 4.f, Dt);
			LookAt = (RigN == 0) ? SL : Damp(LookAt, SL + VelDir * 200.f, 6.f, Dt);
			Apply(CamLoc, LookAt, 52.f, 4.f, 0.5f);
		}
		else
		{
			const FVector FL = F->BallLocation();
			const FVector Rim = OutDir * (R * 0.98f);
			const FVector Want = Rim - OutDir * 120.f + OutDir.RotateAngleAxis(90.f, FVector::UpVector) * (Side * 260.f) + FVector(0, 0, FMath::Max(160.f, FL.Z + 520.f));
			CamLoc = Damp(CamLoc, Want, 3.f, Dt);
			LookAt = Damp(LookAt, FL, 8.f, Dt);
			Apply(CamLoc, LookAt, 50.f, 3.5f, 0.6f);
		}
		break;
	}
	}
}

void UZoogiCineCapture::Tick(float DeltaTime)
{
	if (bQuitting)
	{
		if (FPlatformTime::Seconds() > QuitAt) { QuitAt = 1e30; UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false); }
		return;
	}
	if (FPlatformTime::Seconds() - StartReal > MaxReal)
	{
		if (bRec) End(TEXT("time cap"));
		UE_LOG(LogZoogi, Warning, TEXT("CINECAP time cap (%.0f s): quitting with %d launches filmed"), MaxReal, Filmed);
		bQuitting = true; QuitAt = FPlatformTime::Seconds() + 0.5; return;
	}
	AZoogiGameMode* G = GM();
	if (!G) return;
	const bool bPU = G->IsPowerUpStep();
	if (bPU && !bWasPowerUp && !bRec)
	{
		++LaunchNo;
		if (WantLaunch(LaunchNo, G)) Begin(G);
		else UE_LOG(LogZoogi, Log, TEXT("CINECAP launch %d by %s: not filmed"), LaunchNo, *GetNameSafe(G->GetActiveBall()));
	}
	bWasPowerUp = bPU;
	if (!bRec) return;

	const float Dt = DeltaTime;
	// beats
	if (LaunchT < 0.f && G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::Rolling) { LaunchT = T; Event(TEXT("launch")); }
	if (LaunchT >= 0.f && ImpactT < 0.f)
	{
		for (auto& KV : StartPos)
		{
			AZoogiBall* B = KV.Key.Get();
			if (!B || B == Shooter.Get()) continue;
			if (Flat(B->GetBallVelocity()).Size() > 120.f || FVector::Dist2D(B->BallLocation(), KV.Value) > 25.f)
			{
				Struck = B; ImpactT = T;
				ImpactPos = (B->BallLocation() + (Shooter.IsValid() ? Shooter->BallLocation() : B->BallLocation())) * 0.5f;
				Event(FString::Printf(TEXT("impact %s at %s"), *B->GetName(), *ImpactPos.ToString()));
				break;
			}
		}
	}
	if (OutT < 0.f)
	{
		for (AZoogiBall* B : G->Balls)
		{
			if (!B) continue;
			const FVector L = B->BallLocation();
			if (B->bOut || L.Z < -60.f || AZoogiPlayerController::IsOffPlayArea(G->Arena, FVector2D(L.X, L.Y)))
			{
				const FVector* S0 = StartPos.Find(B);
				if (S0 && (S0->Z < -60.f)) continue; // was already gone
				Faller = B; OutT = T; OutDir = Flat(L).GetSafeNormal(); if (OutDir.IsNearlyZero()) OutDir = AimDir;
				Event(FString::Printf(TEXT("ringout %s at %s"), *B->GetName(), *L.ToString()));
				break;
			}
		}
	}
	if (ResolveT < 0.f && (G->Stage == EZoogiStage::Resolve || G->Stage == EZoogiStage::End || G->Phase == EZoogiPhase::Resolving)) { ResolveT = T; Event(TEXT("resolve")); }

	Rig(G, Dt);
	++RigN;
	// one HUD-free frame per tick (viewport only). Over-cranked takes skip the power-up (PNG dumps are the slow part):
	// they start 0.5 s (game) before the launch, recognised by the CPU's power-up length (~1.2 s) or the launch itself.
	const bool bDump = Slow == 1 || LaunchT >= 0.f || T >= 0.7f;
	if (bDump)
	{
		if (!bDumping) { bDumping = true; Event(TEXT("dump start")); }
		FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("f_%05d.png"), Frame), false, false);
		++Frame;
	}
	T += Dt;

	const float Cap = (Shot == EShot::Crane) ? 7.5f : (Shot == EShot::RingOut ? 9.5f : 8.f);
	const bool bAfterResolve = ResolveT >= 0.f && T - ResolveT > 1.0f && (Shot != EShot::RingOut || OutT < 0.f || T - OutT > 3.f) && (Shot != EShot::Crane || T > 6.8f);
	const bool bOutDone = Shot == EShot::RingOut && OutT >= 0.f && T - OutT > 3.2f;
	const float Hit = OutT >= 0.f ? OutT : ImpactT;
	if (Slow > 1 && Hit >= 0.f && T - Hit > 1.8f && Shot != EShot::Crane) End(TEXT("slow: hit + 1.8 s"));
	else if (Slow > 1 && LaunchT >= 0.f && Hit < 0.f && T - LaunchT > 3.5f) End(TEXT("slow: no hit"));
	else if (T >= Cap) End(TEXT("cap"));
	else if (bOutDone) End(TEXT("faller followed"));
	else if (bAfterResolve && T > 3.f) End(TEXT("resolved"));
	else if (G->Phase == EZoogiPhase::GameOver && T > 2.f) End(TEXT("game over"));
}
