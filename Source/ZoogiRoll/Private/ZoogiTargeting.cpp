// Turn phase state machine (Draw -> Awaken -> Battle -> Resolve -> End), lock-on targeting and clickable angled aiming.
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiPlayerController.h"
#include "ZoogiAnimeSubsystem.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace
{
	FVector TargetPos(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}
	float TargetRadius(const AActor* A) { return Cast<AZoogiOrb>(A) ? ZoogiConst::OrbRadius : ZoogiConst::ZoogiRadius; }

	float InvertLaunchSpeed(float Speed)
	{
		float Lo = 0.f, Hi = 1.f;
		for (int32 i = 0; i < 30; ++i)
		{
			const float Mid = 0.5f * (Lo + Hi);
			if (ZoogiConst::LaunchSpeedForPull(Mid) < Speed) Lo = Mid; else Hi = Mid;
		}
		return 0.5f * (Lo + Hi);
	}
}

const TCHAR* AZoogiGameMode::StageName(EZoogiStage S)
{
	switch (S)
	{
	case EZoogiStage::Draw: return TEXT("DRAW");
	case EZoogiStage::Awaken: return TEXT("AWAKEN");
	case EZoogiStage::Battle: return TEXT("BATTLE");
	case EZoogiStage::Resolve: return TEXT("RESOLVE");
	case EZoogiStage::End: return TEXT("END");
	}
	return TEXT("");
}

const TCHAR* AZoogiGameMode::StepName(EZoogiBattleStep S)
{
	switch (S)
	{
	case EZoogiBattleStep::Aim: return TEXT("Aim");
	case EZoogiBattleStep::PowerUp: return TEXT("PowerUp");
	case EZoogiBattleStep::Rolling: return TEXT("Rolling");
	}
	return TEXT("");
}

float AZoogiGameMode::StageDuration() const
{
	const AZoogiBall* B = GetActiveBall();
	const bool bCPU = B && B->bCPU;
	using namespace ZoogiStageTune;
	switch (Stage)
	{
	case EZoogiStage::Draw: return bCPU ? DrawCPU : DrawHuman;
	case EZoogiStage::Awaken: return bCPU ? AwakenCPU + AwakenBeat : 0.f;
	case EZoogiStage::Battle: return BattleStep == EZoogiBattleStep::PowerUp ? BattleDuration() : (bCPU ? CPUMain : 0.f);
	case EZoogiStage::Resolve: return ResolveMin;
	case EZoogiStage::End: return EndBeat;
	}
	return 0.f;
}

bool AZoogiGameMode::IsLongLock() const
{
	const AZoogiBall* B = GetActiveBall();
	AActor* T = TargetLock.Get();
	if (!B || !T || bSoftLock) return false;
	return FVector::Dist2D(B->BallLocation(), TargetPos(T)) > ZoogiStageTune::LongLockDist;
}

float AZoogiGameMode::BattleDuration() const
{
	using namespace ZoogiStageTune;
	const AZoogiBall* B = GetActiveBall();
	if (bCinematicCam) return BattleSwing + (B && B->bCPU ? CPUBattleHold : BattleHold) + (IsLongLock() ? TargetBeat : 0.f);
	const bool bInfused = B && B->Artifact && B->Artifact->Infused != EZoogiArtifact::None;
	return bInfused ? PowerUpInfused : PowerUpTactical;
}

void AZoogiGameMode::SetStage(EZoogiStage S)
{
	const EZoogiStage Prev = Stage;
	Stage = S;
	StageTime = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Phase %s -> %s (P%d)"), StageName(Prev), StageName(S), ActivePlayer + 1);
	switch (S)
	{
	case EZoogiStage::Draw:
		if (bArtifactMode) DoDraw();
		break;
	case EZoogiStage::Awaken:
		bAwakenDone = false;
		AwakenPick = 0;
		AwakenChoiceText.Reset();
		break;
	case EZoogiStage::Battle:
		BattleStep = EZoogiBattleStep::Aim;
		StepTime = 0.f;
		break;
	case EZoogiStage::Resolve:
		Phase = EZoogiPhase::Resolving;
		ResolveStart = Clock;
		DoResolve();
		break;
	default:
		break;
	}
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnStageChanged(S);
}

void AZoogiGameMode::SetBattleStep(EZoogiBattleStep S)
{
	BattleStep = S;
	StepTime = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Battle step: %s (P%d)"), StepName(S), ActivePlayer + 1);
	if (S == EZoogiBattleStep::PowerUp)
	{
		ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.8f, 0.7f);
		if (AZoogiBall* B = GetActiveBall()) if (B->Artifact) B->Artifact->Pulse(B->Artifact->Infused != EZoogiArtifact::None ? 1.2f : 0.5f);
	}
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnBattleStep(S);
}

bool AZoogiGameMode::IsValidTarget(const AActor* A) const
{
	if (!A || !IsValid(A)) return false;
	const AZoogiBall* Me = GetActiveBall();
	if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B != Me && !B->bOut && Balls.Contains(B);
	if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return !O->bOut && Orbs.Contains(O);
	return false;
}

TArray<AActor*> AZoogiGameMode::TargetCandidates() const
{
	TArray<AActor*> Out;
	const AZoogiBall* Me = GetActiveBall();
	if (!Me) return Out;
	for (AZoogiBall* B : Balls) if (IsValidTarget(B)) Out.Add(B);
	for (AZoogiOrb* O : Orbs) if (IsValidTarget(O)) Out.Add(O);
	const FVector L = Me->BallLocation();
	Out.Sort([&L](const AActor& A, const AActor& B) { return FVector::DistSquared2D(TargetPos(&A), L) < FVector::DistSquared2D(TargetPos(&B), L); });
	return Out;
}

void AZoogiGameMode::LockOn(AActor* Target, bool bSoft)
{
	if (!IsValidTarget(Target)) return;
	if (TargetLock.Get() == Target && bSoftLock == bSoft && !bFreeAim) return;
	TargetLock = Target;
	bSoftLock = bSoft;
	bFreeAim = false;
	AimCutDeg = 0.f;
	LockTime = 0.f;
	LockSerial++;
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.7f, bSoft ? 1.1f : 1.4f);
	UE_LOG(LogZoogi, Log, TEXT("P%d %s-locked %s"), ActivePlayer + 1, bSoft ? TEXT("soft") : TEXT("hard"), *TargetName(Target));
}

void AZoogiGameMode::ClearLock()
{
	if (!TargetLock.IsValid() && !bSoftLock && !bFreeAim) return;
	TargetLock.Reset();
	bSoftLock = false;
	bFreeAim = false;
	AimCutDeg = 0.f;
	LockTime = 0.f;
	LockSerial++;
}

void AZoogiGameMode::CycleLock(bool bSoft)
{
	const TArray<AActor*> C = TargetCandidates();
	if (C.Num() == 0) return;
	const int32 Cur = C.IndexOfByKey(TargetLock.Get());
	LockOn(C[(Cur + 1) % C.Num()], bSoft);
}

void AZoogiGameMode::SetFreeAim(const FVector& P)
{
	const AZoogiBall* Me = GetActiveBall();
	if (!Me || !IsAimStep()) return;
	if (FVector::Dist2D(P, Me->BallLocation()) < ZoogiConst::ZoogiRadius * 1.5f) return;
	TargetLock.Reset();
	bSoftLock = false;
	bFreeAim = true;
	FreeAimPoint = FVector(P.X, P.Y, Me->BallLocation().Z - ZoogiConst::ZoogiRadius);
	AimCutDeg = 0.f;
	LockTime = 0.f;
	LockSerial++;
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.6f, 1.2f);
	UE_LOG(LogZoogi, Log, TEXT("P%d free aim at (%.0f, %.0f)"), ActivePlayer + 1, P.X, P.Y);
}

void AZoogiGameMode::NudgeAim(int32 Steps)
{
	if (!HasAim() || !IsAimStep()) return;
	AimCutDeg = FMath::Clamp(AimCutDeg + Steps * ZoogiStageTune::CutStepDeg, -ZoogiStageTune::MaxCutDeg, ZoogiStageTune::MaxCutDeg);
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.4f, 1.6f + 0.02f * AimCutDeg);
}

bool AZoogiGameMode::HasAim() const
{
	return (TargetLock.IsValid() && !bSoftLock) || bFreeAim;
}

bool AZoogiGameMode::CommitLock()
{
	if (!IsAimStep() || !HasAim()) return false;
	SetBattleStep(EZoogiBattleStep::PowerUp);
	return true;
}

FVector2D AZoogiGameMode::LockShotVelocity(float& OutFraction, float& OutImpactSpeed) const
{
	OutFraction = 0.f;
	OutImpactSpeed = 0.f;
	const AZoogiBall* Me = GetActiveBall();
	const AActor* T = TargetLock.Get();
	if (!Me || !T) return FVector2D::ZeroVector;
	const FVector2D D = FVector2D(TargetPos(T)) - FVector2D(Me->BallLocation());
	const float Dist = D.Size();
	if (Dist < 1.f) return FVector2D::ZeroVector;
	const float Decel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	const float Gap = FMath::Max(0.f, Dist - ZoogiConst::ZoogiRadius - TargetRadius(T));
	// Closer = softer, farther = harder: arrive with a base speed that grows with distance, capped at full power.
	const float Arrive = ZoogiStageTune::LockImpactSpeed + Dist * 0.25f;
	const float VMax = ZoogiConst::LaunchSpeedForPull(1.f);
	const float V0 = FMath::Min(FMath::Sqrt(Arrive * Arrive + 2.f * Decel * Gap), VMax);
	OutFraction = FMath::Clamp(InvertLaunchSpeed(V0), 0.08f, 1.f);
	const float V = ZoogiConst::LaunchSpeedForPull(OutFraction);
	OutImpactSpeed = FMath::Sqrt(FMath::Max(0.f, V * V - 2.f * Decel * Gap));
	return D / Dist * V;
}

FVector2D AZoogiGameMode::PowerStepVelocity(const FVector2D& Dir, float BaseFraction, float& OutFraction) const
{
	static const float Steps[4] = { -1.f, 0.42f, 0.66f, 1.f };
	OutFraction = AimPowerStep > 0 ? Steps[FMath::Clamp(AimPowerStep, 1, 3)] : BaseFraction;
	OutFraction = FMath::Clamp(OutFraction, 0.08f, 1.f);
	return Dir * ZoogiConst::LaunchSpeedForPull(OutFraction);
}

FVector2D AZoogiGameMode::ComputeShotVelocity(float& OutFraction, float& OutImpactSpeed) const
{
	OutFraction = 0.f;
	OutImpactSpeed = 0.f;
	const AZoogiBall* Me = GetActiveBall();
	if (!Me) return FVector2D::ZeroVector;
	const float Decel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	FVector2D Dir;
	float BaseFrac = 0.f;
	if (bFreeAim)
	{
		const FVector2D D = FVector2D(FreeAimPoint) - FVector2D(Me->BallLocation());
		const float Dist = D.Size();
		if (Dist < 1.f) return FVector2D::ZeroVector;
		Dir = D / Dist;
		// Free aim: the shot stops on the clicked point (stopping distance v^2 / 2a).
		const float V0 = FMath::Min(FMath::Sqrt(2.f * Decel * Dist), ZoogiConst::LaunchSpeedForPull(1.f));
		BaseFrac = InvertLaunchSpeed(V0);
	}
	else
	{
		float Impact = 0.f;
		const FVector2D V = LockShotVelocity(BaseFrac, Impact);
		if (V.IsNearlyZero()) return FVector2D::ZeroVector;
		Dir = V.GetSafeNormal();
		OutImpactSpeed = Impact;
	}
	Dir = Dir.GetRotated(AimCutDeg);
	const FVector2D V = PowerStepVelocity(Dir, BaseFrac, OutFraction);
	if (AimPowerStep > 0 && !bFreeAim && TargetLock.IsValid())
	{
		const float Gap = FMath::Max(0.f, FVector::Dist2D(TargetPos(TargetLock.Get()), Me->BallLocation()) - ZoogiConst::ZoogiRadius - TargetRadius(TargetLock.Get()));
		OutImpactSpeed = FMath::Sqrt(FMath::Max(0.f, V.SizeSquared() - 2.f * Decel * Gap));
	}
	return V;
}

FString AZoogiGameMode::TargetName(const AActor* A) const
{
	if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return FString::Printf(TEXT("P%d %s"), B->PlayerIndex + 1, *B->Def.Name);
	if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->bStar ? TEXT("Gold Star Orb") : TEXT("Orb");
	return TEXT("");
}

void AZoogiGameMode::UpdateLockAim()
{
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball || Phase != EZoogiPhase::Aiming) return;
	const bool bBattle = Stage == EZoogiStage::Battle && BattleStep != EZoogiBattleStep::Rolling;
	if (Ball->bCPU)
	{
		const bool bLocked = bBattle && TargetLock.IsValid() && !bSoftLock;
		SetAim(bLocked, bLocked ? CPUTargetPull : FVector::ZeroVector);
		return;
	}
	if (!bBattle || !HasAim()) { SetAim(false, FVector::ZeroVector); return; }
	float Frac = 0.f, Impact = 0.f;
	const FVector2D V = ComputeShotVelocity(Frac, Impact);
	if (V.IsNearlyZero()) { SetAim(false, FVector::ZeroVector); return; }
	const FVector2D Dir = V.GetSafeNormal();
	SetAim(true, FVector(Dir.X, Dir.Y, 0.f) * (Frac * ZoogiConst::FullPull / ZoogiConst::DragScale));
	Ball->AimYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
}

void AZoogiGameMode::UpdatePreview()
{
	Preview = FZoogiAimPreview();
	AZoogiBall* Me = GetActiveBall();
	if (!Me || !bAimActive) return;
	float Frac = 0.f;
	const FVector2D V = PullToVelocity(GetEffectivePull(), Frac);
	if (V.IsNearlyZero()) return;
	const float Speed = V.Size();
	const float Decel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	FVector Dir(V.X / Speed, V.Y / Speed, 0.f);
	Preview.bValid = true;
	Preview.Start = Me->BallLocation();
	Preview.Dir = Dir;
	Preview.Speed = Speed;
	Preview.Fraction = Frac;
	float Stop = FMath::Min(Speed * Speed / (2.f * Decel), 2600.f);

	// Ghost-ball contact: the first Zoogi / orb the shooter's circle meets along the line.
	const FVector2D S2(Preview.Start);
	float BestT = Stop;
	for (AActor* A : TargetCandidates())
	{
		const FVector2D P(TargetPos(A));
		const float R = ZoogiConst::ZoogiRadius + TargetRadius(A);
		const FVector2D D2(Dir.X, Dir.Y);
		const float Along = FVector2D::DotProduct(P - S2, D2);
		if (Along <= 0.f) continue;
		const float Perp2 = (P - S2).SizeSquared() - Along * Along;
		if (Perp2 > R * R) continue;
		const float T = Along - FMath::Sqrt(FMath::Max(0.f, R * R - Perp2));
		if (T < BestT && T > 0.f)
		{
			BestT = T;
			Preview.bContact = true;
			Preview.ContactActor = A;
		}
	}

	// One bank bounce off solid court pieces (rims, Tomb bumpers, Arabian walls, Neon rails) before any contact.
	FVector BankDir = Dir;
	float BankAt = -1.f;
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiAimBank), false);
		for (AZoogiBall* B : Balls) if (B) Q.AddIgnoredActor(B);
		for (AZoogiOrb* O : Orbs) if (O) Q.AddIgnoredActor(O);
		const FVector From = Preview.Start + FVector(0, 0, 10.f);
		const float Reach = Preview.bContact ? BestT : Stop;
		FHitResult Hit;
		if (Reach > 20.f && GetWorld()->SweepSingleByChannel(Hit, From, From + Dir * Reach, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius - 8.f), Q) && !Hit.bStartPenetrating)
		{
			FVector N = Hit.ImpactNormal;
			N.Z = 0.f;
			if (N.Normalize() && FVector::DotProduct(N, Dir) < -0.15f)
			{
				BankAt = Hit.Distance;
				BankDir = (Dir - 2.f * FVector::DotProduct(Dir, N) * N).GetSafeNormal2D();
				Preview.bBank = true;
				Preview.BankPoint = Hit.Location - FVector(0, 0, ZoogiConst::ZoogiRadius + 6.f);
				Preview.bContact = false; // the wall comes first
			}
		}
	}

	// Path points (ground level).
	const FVector G0 = Preview.Start - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f);
	const float PathLen = Preview.bContact ? BestT : Stop;
	const int32 Dots = FMath::Clamp(FMath::RoundToInt(PathLen / 55.f), 2, 48);
	for (int32 i = 1; i <= Dots; ++i)
	{
		const float D = PathLen * i / Dots;
		FVector P;
		if (BankAt >= 0.f && D > BankAt)
		{
			// After the bank: restitution ~0.75 and the remaining stopping distance shrinks with it.
			const float Rest = (D - BankAt) * 0.75f;
			P = G0 + Dir * BankAt + BankDir * Rest;
		}
		else
		{
			P = G0 + Dir * D;
		}
		Preview.Path.Add(P);
		Preview.PathOff.Add(Arena ? Arena->IsOffCourt(FVector2D(P.X, P.Y)) : FVector2D(P.X, P.Y).Size() > ZoogiConst::KnockoffRadius);
	}

	if (Preview.bContact && Preview.ContactActor.IsValid())
	{
		AActor* A = Preview.ContactActor.Get();
		Preview.Ghost = Preview.Start + Dir * BestT;
		FVector N = TargetPos(A) - Preview.Ghost;
		N.Z = 0.f;
		N = N.GetSafeNormal();
		Preview.TargetDir = N;
		Preview.CutDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(N, Dir), -1.f, 1.f)));
		// Shooter keeps the tangential part (90 deg rule), plus a bit of follow-through for heavier shooters.
		const float M1 = Me->Def.MassRatio();
		const float M2 = Cast<AZoogiBall>(A) ? Cast<AZoogiBall>(A)->Def.MassRatio() : ZoogiConst::OrbMassRatio;
		const FVector Tang = Dir - FVector::DotProduct(Dir, N) * N;
		const float Follow = FMath::Max(0.f, (M1 - 0.75f * M2) / (M1 + M2)) * FVector::DotProduct(Dir, N);
		Preview.ShooterDir = Tang + N * Follow;
	}
}

void AZoogiGameMode::FireLockedShot()
{
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	if (Ball->bCPU) SetAim(true, CPUTargetPull);
	else
	{
		UpdateLockAim();
		if (!bAimActive) { SetBattleStep(EZoogiBattleStep::Aim); return; }
	}
	UE_LOG(LogZoogi, Log, TEXT("P%d fires at %s (cut %.0f deg)"), ActivePlayer + 1, bFreeAim ? TEXT("a floor point") : (TargetLock.IsValid() ? *TargetName(TargetLock.Get()) : TEXT("(no lock)")), AimCutDeg);
	if (!ReleaseAim())
	{
		SetAim(false, FVector::ZeroVector);
		SetBattleStep(EZoogiBattleStep::Aim);
	}
}

void AZoogiGameMode::TickStages(float Dt)
{
	using namespace ZoogiStageTune;
	StageTime += Dt;
	StepTime += Dt;
	LockTime += Dt;
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	if (TargetLock.IsValid() && !IsValidTarget(TargetLock.Get())) ClearLock(); // knocked off / gone
	const bool bCPU = Ball->bCPU;
	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	const bool bAnimeBusy = Anime && Anime->IsBusy();
	switch (Stage)
	{
	case EZoogiStage::Draw:
		if (StageTime >= (bCPU ? DrawCPU : DrawHuman)) SetStage(bArtifactMode ? EZoogiStage::Awaken : EZoogiStage::Battle);
		break;
	case EZoogiStage::Awaken:
		if (!bAwakenDone && bCPU && StageTime >= AwakenCPU) PlanCPUAwaken();
		if (!bAwakenDone && Ball->Artifact && Ball->Artifact->Drawn == EZoogiArtifact::None && Ball->Artifact->Hand.Num() == 0 && StageTime > 0.6f) AwakenSave(); // nothing to use
		if (bAwakenDone && !bAnimeBusy && StageTime - AwakenDoneAt >= (AwakenChoiceText.StartsWith(TEXT("SAVE")) ? AwakenSaveBeat : AwakenBeat)) SetStage(EZoogiStage::Battle);
		break;
	case EZoogiStage::Battle:
		if (BattleStep == EZoogiBattleStep::Aim)
		{
			if (bCPU && bCPUPlanned)
			{
				if (StepTime >= CPULockAt && !TargetLock.IsValid() && IsValidTarget(CPULock.Get())) LockOn(CPULock.Get(), false);
				if (StepTime >= CPUMain) SetBattleStep(EZoogiBattleStep::PowerUp);
			}
		}
		else if (BattleStep == EZoogiBattleStep::PowerUp)
		{
			if (!bCPU && !HasAim()) { SetBattleStep(EZoogiBattleStep::Aim); break; }
			if (StepTime >= BattleDuration() && !bAnimeBusy) FireLockedShot();
		}
		break;
	case EZoogiStage::Resolve:
		if (StageTime >= ResolveMin && !bAnimeBusy) SetStage(EZoogiStage::End);
		break;
	case EZoogiStage::End:
		if (StageTime >= EndBeat && !bAnimeBusy) EndTurn();
		return;
	}
	if (Phase == EZoogiPhase::Aiming)
	{
		UpdateLockAim();
		UpdatePreview();
	}

	// Automation: one screenshot per phase for the first few turns.
	if (ShotsFired < 4)
	{
		const int32 N = ShotsFired + 1;
		if (Stage == EZoogiStage::Draw && StageTime > (bCPU ? DrawCPU : DrawHuman) * 0.75f) TakeShot(FString::Printf(TEXT("s%d_1_draw"), N));
		if (Stage == EZoogiStage::Awaken && StageTime > 0.6f && !bAwakenDone) TakeShot(FString::Printf(TEXT("s%d_2_awaken"), N));
		if (Stage == EZoogiStage::Awaken && bAwakenDone && StageTime - AwakenDoneAt > 0.35f) TakeShot(FString::Printf(TEXT("s%d_2b_awakened"), N));
		if (Stage == EZoogiStage::Battle && BattleStep == EZoogiBattleStep::Aim && StepTime > 1.0f) TakeShot(FString::Printf(TEXT("s%d_3_aim"), N));
		if (Stage == EZoogiStage::Battle && BattleStep == EZoogiBattleStep::PowerUp && StepTime > BattleDuration() * 0.6f) TakeShot(FString::Printf(TEXT("s%d_4_powerup"), N));
		if (Stage == EZoogiStage::Resolve && StageTime > 0.3f) TakeShot(FString::Printf(TEXT("s%d_6_resolve"), N - 1));
	}
}
