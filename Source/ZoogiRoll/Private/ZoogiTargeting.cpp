// RPG-style lock-on targeting and the duel turn stages (Draw / Standby / Main / Battle / End).
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiPlayerController.h"
#include "Engine/World.h"

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
	case EZoogiStage::Standby: return TEXT("STANDBY");
	case EZoogiStage::Main: return TEXT("MAIN");
	case EZoogiStage::Battle: return TEXT("BATTLE");
	case EZoogiStage::End: return TEXT("END");
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
	case EZoogiStage::Standby: return bCPU ? CPUStandby : StandbyMax;
	case EZoogiStage::Main: return bCPU ? CPUMain : 0.f;
	case EZoogiStage::Battle: return BattleSwing + (bCPU ? CPUBattleHold : BattleHold);
	default: return 0.f;
	}
}

void AZoogiGameMode::SetStage(EZoogiStage S)
{
	Stage = S;
	StageTime = 0.f;
	if (S == EZoogiStage::Main && bSoftLock && TargetLock.IsValid())
	{
		bSoftLock = false; // the soft pick from Standby becomes the real lock
		LockTime = 0.f;
		LockSerial++;
	}
	if (S == EZoogiStage::Battle) ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.8f, 0.7f);
	UE_LOG(LogZoogi, Log, TEXT("Stage: %s (P%d)"), StageName(S), ActivePlayer + 1);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnStageChanged(S);
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
	if (TargetLock.Get() == Target && bSoftLock == bSoft) return;
	TargetLock = Target;
	bSoftLock = bSoft;
	LockTime = 0.f;
	LockSerial++;
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.7f, bSoft ? 1.1f : 1.4f);
	UE_LOG(LogZoogi, Log, TEXT("P%d %s-locked %s"), ActivePlayer + 1, bSoft ? TEXT("soft") : TEXT("hard"), *TargetName(Target));
}

void AZoogiGameMode::ClearLock()
{
	if (!TargetLock.IsValid() && !bSoftLock) return;
	TargetLock.Reset();
	bSoftLock = false;
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

bool AZoogiGameMode::CommitLock()
{
	if (Phase != EZoogiPhase::Aiming || !TargetLock.IsValid()) return false;
	if (Stage == EZoogiStage::Standby) { SetStage(EZoogiStage::Main); return true; }
	if (Stage != EZoogiStage::Main || bSoftLock) return false;
	SetStage(EZoogiStage::Battle);
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
	const bool bLocked = TargetLock.IsValid() && !bSoftLock && (Stage == EZoogiStage::Main || Stage == EZoogiStage::Battle);
	if (!bLocked) { SetAim(false, FVector::ZeroVector); return; }
	if (Ball->bCPU)
	{
		SetAim(true, CPUTargetPull);
		return;
	}
	float Frac = 0.f, Impact = 0.f;
	const FVector2D V = LockShotVelocity(Frac, Impact);
	if (V.IsNearlyZero()) { SetAim(false, FVector::ZeroVector); return; }
	const FVector2D Dir = V.GetSafeNormal();
	SetAim(true, FVector(Dir.X, Dir.Y, 0.f) * (Frac * ZoogiConst::FullPull / ZoogiConst::DragScale));
	Ball->AimYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
}

void AZoogiGameMode::FireLockedShot()
{
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	if (Ball->bCPU) SetAim(true, CPUTargetPull);
	else
	{
		UpdateLockAim();
		if (!bAimActive) { SetStage(EZoogiStage::Main); return; }
	}
	UE_LOG(LogZoogi, Log, TEXT("P%d fires at %s"), ActivePlayer + 1, TargetLock.IsValid() ? *TargetName(TargetLock.Get()) : TEXT("(no lock)"));
	if (!ReleaseAim())
	{
		SetAim(false, FVector::ZeroVector);
		SetStage(EZoogiStage::Main);
	}
}

void AZoogiGameMode::TickStages(float Dt)
{
	using namespace ZoogiStageTune;
	StageTime += Dt;
	LockTime += Dt;
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	if (!TargetLock.IsValid() || !IsValidTarget(TargetLock.Get()))
	{
		if (TargetLock.IsValid() || bSoftLock) ClearLock(); // knocked off / gone
	}
	const bool bCPU = Ball->bCPU;
	switch (Stage)
	{
	case EZoogiStage::Draw:
		if (StageTime >= (bCPU ? DrawCPU : DrawHuman)) SetStage(EZoogiStage::Standby);
		break;
	case EZoogiStage::Standby:
		if (bCPU) { if (StageTime >= CPUStandby) SetStage(EZoogiStage::Main); }
		else if ((TargetLock.IsValid() && bSoftLock && LockTime >= SoftLockBeat && StageTime >= StandbyMin) || StageTime >= StandbyMax) SetStage(EZoogiStage::Main);
		break;
	case EZoogiStage::Main:
		if (bCPU && bCPUPlanned)
		{
			if (StageTime >= CPULockAt && !TargetLock.IsValid() && IsValidTarget(CPULock.Get())) LockOn(CPULock.Get(), false);
			if (StageTime >= CPUMain) SetStage(EZoogiStage::Battle);
		}
		break;
	case EZoogiStage::Battle:
		if (!bCPU && !TargetLock.IsValid()) { SetStage(EZoogiStage::Main); break; }
		if (StageTime >= BattleSwing + (bCPU ? CPUBattleHold : BattleHold)) FireLockedShot();
		break;
	default:
		break;
	}
	UpdateLockAim();

	// Automation: one screenshot per stage for the first few turns.
	if (Phase == EZoogiPhase::Aiming && ShotsFired < 4)
	{
		const int32 N = ShotsFired + 1;
		if (Stage == EZoogiStage::Draw && StageTime > (bCPU ? DrawCPU : DrawHuman) * 0.75f) TakeShot(FString::Printf(TEXT("s%d_1_draw"), N));
		if (Stage == EZoogiStage::Standby && StageTime > 0.7f) TakeShot(FString::Printf(TEXT("s%d_2_standby"), N));
		if (Stage == EZoogiStage::Main && StageTime > 0.95f) TakeShot(FString::Printf(TEXT("s%d_3_main"), N));
		if (Stage == EZoogiStage::Battle && StageTime > 0.3f) TakeShot(FString::Printf(TEXT("s%d_4_battle_swing"), N));
		if (Stage == EZoogiStage::Battle && StageTime > BattleSwing + 0.1f) TakeShot(FString::Printf(TEXT("s%d_5_battle_locked"), N));
	}
}
