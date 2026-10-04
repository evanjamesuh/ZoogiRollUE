// Zoogi powers (abilities from ZOOGI_ROSTER), implemented on AZoogiGameMode.
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiClone.h"
#include "ZoogiFX.h"
#include "ZoogiArena.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/SphereComponent.h"

namespace
{
	constexpr float ExplosionRadius = 310.f;   // balance Oct 3: 450 -> 390 -> 360; Oct 4: 360 -> 310 (Hotstreak won 62% of the 3rd 20-match sample)
	constexpr float ShadowRadius = 520.f;
	constexpr float LockOnCone = 35.f;
	constexpr float LockOnRange = 2600.f;
	constexpr float BindShotScale = 0.65f;
	constexpr float BindHitBoost = 1.25f;
	// Everyone gets a perfect lock-on line now, so Pinpoint's power also makes the locked shot hit harder.
	constexpr float PinpointPowerBoost = 1.3f;
	constexpr int32 MaxCharges = 3;

	FLinearColor PowerColor(EZoogiPower P)
	{
		switch (P)
		{
		case EZoogiPower::WolfPack: return FLinearColor(0.45f, 0.65f, 1.f);
		case EZoogiPower::Explosion: return FLinearColor(1.f, 0.38f, 0.06f);
		case EZoogiPower::Ricochet: return FLinearColor(0.25f, 0.55f, 1.f);
		case EZoogiPower::LockOn: return FLinearColor(0.7f, 0.35f, 1.f);
		case EZoogiPower::StaticShock: return FLinearColor(1.f, 0.85f, 0.15f);
		case EZoogiPower::BandageBind: return FLinearColor(0.9f, 0.78f, 0.55f);
		case EZoogiPower::ShadowStun: return FLinearColor(0.45f, 0.15f, 0.95f);
		}
		return FLinearColor::White;
	}

	FVector ActorPos(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}

	void PushActor(AActor* A, const FVector& Planar, float Up, int32 Credit)
	{
		if (AZoogiBall* B = Cast<AZoogiBall>(A))
		{
			const FVector V = B->GetBallVelocity();
			B->SetPlanarVelocity(FVector(V.X, V.Y, 0.f) + Planar, Up);
			if (Credit >= 0 && B->PlayerIndex != Credit) B->LastHitPlayer = Credit;
		}
		else if (AZoogiOrb* O = Cast<AZoogiOrb>(A))
		{
			O->Sphere->WakeRigidBody();
			const FVector V = O->Sphere->GetPhysicsLinearVelocity();
			O->Sphere->SetPhysicsLinearVelocity(FVector(V.X + Planar.X, V.Y + Planar.Y, Up != 0.f ? Up : V.Z));
			if (Credit >= 0) O->LastHitBy = Credit;
		}
	}
}

TArray<AActor*> AZoogiGameMode::LiveTargets(const AZoogiBall* Except) const
{
	TArray<AActor*> Out;
	for (AZoogiOrb* O : Orbs) if (O && !O->bOut) Out.Add(O);
	for (AZoogiBall* B : Balls) if (B && B != Except && !B->bOut) Out.Add(B);
	return Out;
}

bool AZoogiGameMode::CanUsePower() const
{
	const AZoogiBall* B = GetActiveBall();
	if (!B || B->bOut || bPowerUsed) return false;
	if (bArtifactMode) return false; // artifact mode: signature powers only ride along with an affinity Infuse (ZoogiAwaken.cpp)
	if (B->Charges <= 0 && !bPowerArmed) return false;
	const EZoogiPower P = B->Def.PowerKind;
	if (ZoogiUtil::IsArmedPower(P)) return Phase == EZoogiPhase::Aiming;
	if (P == EZoogiPower::WolfPack) return Phase == EZoogiPhase::Rolling && PhaseTime < 6.f; // "while moving"
	return (Phase == EZoogiPhase::Aiming || Phase == EZoogiPhase::Rolling) && PhaseTime < 8.f;
}

bool AZoogiGameMode::TryPower()
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !CanUsePower()) return false;
	const EZoogiPower P = B->Def.PowerKind;
	if (ZoogiUtil::IsArmedPower(P))
	{
		bPowerArmed = !bPowerArmed; // toggle; the charge is spent when the shot fires
		ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.7f, bPowerArmed ? 1.f : 0.8f);
		AZoogiFX::Spawn(GetWorld(), B->BallLocation(), PowerColor(P), bPowerArmed ? 120.f : 60.f, 0.45f, true, 5.f, B);
		UE_LOG(LogZoogi, Log, TEXT("P%d %s %s %s"), B->PlayerIndex + 1, *B->Def.Name, bPowerArmed ? TEXT("armed") : TEXT("disarmed"), *B->Def.Ability);
		return true;
	}
	FireInstantPower(B);
	return true;
}

void AZoogiGameMode::FireInstantPower(AZoogiBall* Ball)
{
	const EZoogiPower P = Ball->Def.PowerKind;
	const FLinearColor C = PowerColor(P);
	const FVector L = Ball->BallLocation();
	Ball->Charges = FMath::Max(0, Ball->Charges - 1);
	bPowerUsed = true;
	AddPopup(Ball->Def.Ability.ToUpper() + TEXT("!"), L + FVector(0, 0, 140.f), C);
	ZoogiUtil::PlaySfx(this, P == EZoogiPower::Explosion ? TEXT("Explosion") : TEXT("Zap"), 0.9f, P == EZoogiPower::ShadowStun ? 0.7f : 1.f);
	TakeShotDelayed(FString::Printf(TEXT("power_%s"), *Ball->Def.Id.ToString()), P == EZoogiPower::WolfPack ? 0.6f : 0.15f);

	switch (P)
	{
	case EZoogiPower::Explosion:
	{
		AZoogiFX::Spawn(GetWorld(), L, C, ExplosionRadius, 0.7f, false, 16.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Explosion"), L);
		AZoogiFX::Spawn(GetWorld(), L, FLinearColor(1.f, 0.85f, 0.3f), ExplosionRadius * 0.45f, 0.35f, false, 25.f);
		AZoogiFX::Spawn(GetWorld(), L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), FLinearColor(1.f, 0.55f, 0.1f), ExplosionRadius * 1.15f, 0.8f, true, 10.f);
		int32 N = 0;
		for (AActor* T : LiveTargets(Ball))
		{
			const FVector D = ActorPos(T) - L;
			const float Dist = FVector2D(D.X, D.Y).Size();
			if (Dist > ExplosionRadius + ZoogiConst::ZoogiRadius) continue;
			const float K = 1.f - FMath::Clamp(Dist / (ExplosionRadius + ZoogiConst::ZoogiRadius), 0.f, 1.f);
			const FVector Dir = FVector(D.X, D.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, 0));
			PushActor(T, Dir * (470.f + 1100.f * K), 200.f + 180.f * K, Ball->PlayerIndex); // Oct 4: 550+1300k -> 470+1100k; // balance Oct 3 (20-match sample): 600+1400k -> 550+1300k, Hotstreak won 50%
			++N;
		}
		UE_LOG(LogZoogi, Log, TEXT("P%d Hotstreak explosion pushed %d"), Ball->PlayerIndex + 1, N);
		break;
	}
	case EZoogiPower::ShadowStun:
	{
		AZoogiFX::Spawn(GetWorld(), L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), C, ShadowRadius, 1.0f, true, 14.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Purple"), L, FRotator::ZeroRotator, 1.6f);
		AZoogiFX::Spawn(GetWorld(), L, FLinearColor(0.35f, 0.08f, 0.9f), ShadowRadius * 0.8f, 0.9f, false, 12.f);
		int32 N = 0;
		for (AZoogiBall* B : Balls)
		{
			if (!B || B == Ball || B->bOut) continue;
			if (FVector::Dist2D(B->BallLocation(), L) > ShadowRadius + ZoogiConst::ZoogiRadius) continue;
			B->StunTurns = FMath::Max(B->StunTurns, 1);
			// Artifact rules: the stun fires on Nightshade's powered impact (Void affinity); freezing the target in place
			// cancelled the knock (affinity turns averaged -1 pt). There the stun only skips the victim's next turn.
			if (!bArtifactMode) B->SetPlanarVelocity(FVector::ZeroVector);
			AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 110.f, 0.6f, false, 6.f, B);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Purple"), B->BallLocation(), FRotator::ZeroRotator, 0.7f);
			AddPopup(TEXT("FROZEN"), B->BallLocation() + FVector(0, 0, 110.f), C);
			++N;
		}
		UE_LOG(LogZoogi, Log, TEXT("P%d Nightshade shadow stun froze %d"), Ball->PlayerIndex + 1, N);
		break;
	}
	case EZoogiPower::WolfPack:
	{
		TArray<AActor*> Targets = LiveTargets(Ball);
		Targets.Sort([&L](const AActor& A, const AActor& B) { return FVector::DistSquared2D(ActorPos(&A), L) < FVector::DistSquared2D(ActorPos(&B), L); });
		const FVector V = Ball->GetBallVelocity();
		FVector Fwd = FVector(V.X, V.Y, 0.f).GetSafeNormal();
		if (Fwd.IsNearlyZero()) Fwd = FRotator(0.f, Ball->AimYaw, 0.f).Vector();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
		const FVector Offsets[3] = { Right * 115.f, -Right * 115.f, -Fwd * 120.f };
		UStaticMesh* Wolf = ZoogiUtil::LoadMesh(TEXT("/Game/Zoogis/wolfgang/SM_wolfgang.SM_wolfgang"));
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector SpawnL = L + Offsets[i] + FVector(0, 0, 8.f);
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AZoogiClone* Cl = GetWorld()->SpawnActor<AZoogiClone>(AZoogiClone::StaticClass(), FTransform(Fwd.Rotation(), SpawnL), SP);
			if (!Cl) continue;
			AActor* T = Targets.IsValidIndex(i) ? Targets[i] : (Targets.Num() ? Targets[0] : nullptr);
			Cl->InitClone(Wolf, T, Ball->PlayerIndex, 1750.f); // balance Oct 3: 1800 -> 1650 -> 1750 (52% win share in sample 1, 5% in sample 2)
			Cl->Sphere->SetPhysicsLinearVelocity(Fwd * 900.f + Offsets[i].GetSafeNormal() * 400.f);
			Clones.Add(Cl);
			AZoogiFX::Spawn(GetWorld(), SpawnL, C, 90.f, 0.4f, false, 6.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_White"), SpawnL, FRotator::ZeroRotator, 0.6f);
		}
		UE_LOG(LogZoogi, Log, TEXT("P%d Wolfgang wolf pack: %d clones"), Ball->PlayerIndex + 1, Clones.Num());
		break;
	}
	default:
		break;
	}
}

AActor* AZoogiGameMode::GetLockTarget() const
{
	const AZoogiBall* B = GetActiveBall();
	if (!B || !bPowerArmed || B->Def.PowerKind != EZoogiPower::LockOn) return nullptr;
	const FVector2D Pull(AimPull.X, AimPull.Y);
	if (Pull.Size() < 20.f) return nullptr;
	const FVector2D Dir = Pull.GetSafeNormal();
	const FVector2D From(B->BallLocation());
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* T : LiveTargets(B))
	{
		const FVector2D To = FVector2D(ActorPos(T)) - From;
		const float Dist = To.Size();
		if (Dist < 1.f || Dist > LockOnRange) continue;
		const float Ang = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(To / Dist, Dir), -1.f, 1.f)));
		if (Ang > LockOnCone) continue;
		const float Score = Ang * 25.f + Dist * 0.15f;
		if (Score < BestScore) { BestScore = Score; Best = T; }
	}
	return Best;
}

FVector AZoogiGameMode::GetEffectivePull() const
{
	if (AActor* T = GetLockTarget())
	{
		const FVector To = ActorPos(T) - GetActiveBall()->BallLocation();
		const float Len = FMath::Min(FVector2D(AimPull.X, AimPull.Y).Size() * PinpointPowerBoost, ZoogiConst::FullPull / ZoogiConst::DragScale);
		return FVector(To.X, To.Y, 0.f).GetSafeNormal() * Len;
	}
	return AimPull;
}

void AZoogiGameMode::OnArmedShot(AZoogiBall* Ball)
{
	if (!bPowerArmed || !Ball) return;
	const EZoogiPower P = Ball->Def.PowerKind;
	Ball->Charges = FMath::Max(0, Ball->Charges - 1);
	bPowerArmed = false;
	bPowerUsed = true;
	PowerTouched.Reset();
	const FLinearColor C = PowerColor(P);
	AZoogiFX::Spawn(GetWorld(), Ball->BallLocation(), C, 140.f, 0.6f, false, 7.f, Ball);
	if (P != EZoogiPower::Explosion && P != EZoogiPower::ShadowStun) AZoogiFX::Niagara(GetWorld(), AZoogiFX::PowerBurst(P), Ball->BallLocation(), FRotator::ZeroRotator, 0.8f);
	AddPopup(Ball->Def.Ability.ToUpper() + TEXT("!"), Ball->BallLocation() + FVector(0, 0, 140.f), C);
	TakeShotDelayed(FString::Printf(TEXT("power_%s"), *Ball->Def.Id.ToString()), 0.6f);
	ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.6f, 1.3f);
	switch (P)
	{
	case EZoogiPower::StaticShock: Ball->SetPhasing(true); break;
	case EZoogiPower::Ricochet: RicochetsLeft = 2; RicochetFrom = nullptr; RicochetTimer = -1.f; break;
	default: break;
	}
	UE_LOG(LogZoogi, Log, TEXT("P%d %s used %s"), Ball->PlayerIndex + 1, *Ball->Def.Name, *Ball->Def.Ability);
}

void AZoogiGameMode::OnShooterContact(AZoogiBall* Shooter, AActor* Other)
{
	// First contact of the shot: impact flash in the shooter's colour, hit-stop and camera kick.
	if (Phase == EZoogiPhase::Rolling && Shooter && Other && !bShotImpacted)
	{
		bShotImpacted = true;
		const float S = FMath::Clamp((ShotSpeed - 500.f) / 2200.f, 0.2f, 1.f);
		const FVector Mid = (Shooter->BallLocation() + ActorPos(Other)) * 0.5f;
		const FLinearColor PC = ZoogiUtil::PlayerColor(Shooter->PlayerIndex);
		AZoogiFX::Spawn(GetWorld(), Mid, FLinearColor(1.f, 0.97f, 0.9f), 90.f + 90.f * S, 0.22f, false, 14.f);
		AZoogiFX::Spawn(GetWorld(), Mid - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), PC, 150.f + 150.f * S, 0.45f, true, 10.f);
		if (S > 0.6f) AddPopup(TEXT("SMASH!"), Mid + FVector(0, 0, 150.f), PC);
		if (AZoogiPlayerController* PCtl = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PCtl->OnImpact(S);
	}
	if (Phase == EZoogiPhase::Rolling && Shooter && Other) ApplyInfuseImpact(Shooter, Other);
	if (Phase != EZoogiPhase::Rolling || !Shooter || !Other || !bPowerUsed) return;
	const EZoogiPower P = Shooter->Def.PowerKind;
	AZoogiBall* OtherBall = Cast<AZoogiBall>(Other);
	switch (P)
	{
	case EZoogiPower::Ricochet:
		if (RicochetsLeft > 0 && RicochetTimer < 0.f && RicochetFrom.Get() != Other)
		{
			RicochetFrom = Other;
			RicochetTimer = 0.08f;
		}
		break;
	case EZoogiPower::StaticShock:
		if (OtherBall && Shooter->bPhasing && !PowerTouched.Contains(OtherBall))
		{
			PowerTouched.Add(OtherBall);
			OtherBall->StunTurns = FMath::Max(OtherBall->StunTurns, 1);
			OtherBall->LastHitPlayer = Shooter->PlayerIndex;
			const FVector D = OtherBall->BallLocation() - Shooter->BallLocation();
			const FVector V = Shooter->GetBallVelocity();
			FVector Side = FVector::CrossProduct(FVector::UpVector, FVector(V.X, V.Y, 0.f).GetSafeNormal());
			if (FVector::DotProduct(Side, D) < 0.f) Side = -Side;
			OtherBall->SetPlanarVelocity(Side * 680.f + FVector(V.X, V.Y, 0.f) * 0.3f, 150.f); // balance: 450 -> 680 (Bolt scored 0 in 5 matches)
			AZoogiFX::Spawn(GetWorld(), OtherBall->BallLocation(), PowerColor(P), 140.f, 0.5f, false, 9.f, OtherBall);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Yellow"), OtherBall->BallLocation());
			AddPopup(TEXT("STUNNED"), OtherBall->BallLocation() + FVector(0, 0, 110.f), PowerColor(P));
			ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.8f, 1.6f);
			UE_LOG(LogZoogi, Log, TEXT("Bolt phased through P%d and stunned it"), OtherBall->PlayerIndex + 1);
		}
		break;
	case EZoogiPower::BandageBind:
		if (OtherBall && !PowerTouched.Contains(OtherBall))
		{
			PowerTouched.Add(OtherBall);
			// Balance Oct 3: the bind used to damp the victim's knock velocity by 45%, which made Wraps' powered hits unable to knock anyone off
			// (Wraps scored 0 in all 9 sample matches). Now the hit keeps its full force and the curse lasts two shots.
			OtherBall->BoundShots = 2;
			// The curse also yanks: the bound victim takes the hit 25% harder (Wraps won 4% of sample 2 even with full hit force).
			{
				const FVector V = OtherBall->GetBallVelocity();
				OtherBall->SetPlanarVelocity(FVector(V.X, V.Y, 0.f) * BindHitBoost);
			}
			AZoogiFX::Spawn(GetWorld(), OtherBall->BallLocation(), PowerColor(P), 120.f, 0.7f, true, 6.f, OtherBall);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Gold"), OtherBall->BallLocation(), FRotator::ZeroRotator, 0.8f);
			AddPopup(TEXT("BOUND"), OtherBall->BallLocation() + FVector(0, 0, 110.f), PowerColor(P));
			ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.6f, 0.6f);
			UE_LOG(LogZoogi, Log, TEXT("Wraps bound P%d"), OtherBall->PlayerIndex + 1);
		}
		break;
	default:
		break;
	}
}

void AZoogiGameMode::TickPowers(float Dt)
{
	Clones.RemoveAll([](const TObjectPtr<AZoogiClone>& C) { return !IsValid(C.Get()); });
	AZoogiBall* Ball = GetActiveBall();
	if (RicochetTimer >= 0.f && Ball && !Ball->bOut)
	{
		RicochetTimer -= Dt;
		if (RicochetTimer < 0.f)
		{
			// Bounce toward the target that is easiest to knock off from here (close, and near the edge along the bounce line),
			// rather than simply the nearest one (that kept Lars juggling centre orbs: 9 avg points in the 20-match sample).
			const FVector L = Ball->BallLocation();
			const float Decel = Arena ? Arena->GetRollingDecel() : 600.f;
			AActor* Best = nullptr;
			float BestD = TNumericLimits<float>::Max();
			float BestNeed = 1050.f;
			for (AActor* T : LiveTargets(Ball))
			{
				if (T == RicochetFrom.Get()) continue;
				const FVector TP = ActorPos(T);
				const float D = FVector::Dist2D(TP, L);
				if (D > 2200.f || D < 1.f) continue;
				const FVector2D Dir2 = FVector2D(TP - L).GetSafeNormal();
				const float Edge = Arena ? Arena->DistanceToEdge(FVector2D(TP), Dir2) : 1000.f;
				// Too close to the rim on this line and Lars would follow it off.
				const float Cost = D * 0.6f + Edge + (Edge < 160.f ? 700.f : 0.f) + (Cast<AZoogiBall>(T) ? -150.f : 0.f);
				if (Cost < BestD)
				{
					BestD = Cost; Best = T;
					const float TM = Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->Def.MassRatio() : ZoogiConst::OrbMassRatio;
					const float Transfer = 1.7f * Ball->Def.MassRatio() / (Ball->Def.MassRatio() + TM);
					const float VIn = FMath::Sqrt(2.f * Decel * (Edge + 200.f)) / FMath::Max(Transfer, 0.3f);
					BestNeed = FMath::Sqrt(VIn * VIn + 2.f * Decel * FMath::Max(0.f, D - 2.f * ZoogiConst::ZoogiRadius));
				}
			}
			if (Best)
			{
				const FVector Dir = (ActorPos(Best) - L).GetSafeNormal2D();
				const float Speed = FMath::Clamp(FMath::Max3(Ball->PlanarSpeed(), ShotSpeed * 0.6f, BestNeed), 1050.f, 1900.f);
				Ball->SetPlanarVelocity(Dir * Speed);
				AZoogiFX::Spawn(GetWorld(), L, PowerColor(EZoogiPower::Ricochet), 120.f, 0.4f, false, 8.f);
				AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Green"), L, FRotator::ZeroRotator, 0.8f);
				AddPopup(TEXT("RICOCHET"), L + FVector(0, 0, 110.f), PowerColor(EZoogiPower::Ricochet));
				ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.5f, 2.f);
				RicochetsLeft--;
				UE_LOG(LogZoogi, Log, TEXT("Lars ricochet toward %s (%.0f cm), %d left"), *Best->GetName(), BestD, RicochetsLeft);
			}
			else
			{
				RicochetsLeft = 0;
			}
		}
	}
}

void AZoogiGameMode::EndPowers()
{
	bPowerArmed = false;
	bPowerUsed = false;
	RicochetsLeft = 0;
	RicochetTimer = -1.f;
	PowerTouched.Reset();
	for (AZoogiClone* C : Clones) if (IsValid(C)) C->Destroy();
	Clones.Reset();
	for (AZoogiBall* B : Balls)
	{
		if (!B || !B->bPhasing) continue;
		B->SetPhasing(false);
		// Bolt may have stopped inside another Zoogi: move it to the closest free spot.
		for (AZoogiBall* O : Balls)
		{
			if (!O || O == B || O->bOut || B->bOut) continue;
			if (FVector::Dist2D(O->BallLocation(), B->BallLocation()) < 2.f * ZoogiConst::ZoogiRadius + 2.f)
			{
				FVector Away = (B->BallLocation() - O->BallLocation()).GetSafeNormal2D();
				if (Away.IsNearlyZero()) Away = FVector(1, 0, 0);
				B->PlaceAt(O->BallLocation() + Away * (2.f * ZoogiConst::ZoogiRadius + 15.f) + FVector(0, 0, 5.f), B->AimYaw);
			}
		}
	}
}

void AZoogiGameMode::CPUPowerTick()
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->bCPU || !bCPUUsePower || !CanUsePower()) return;
	const EZoogiPower P = B->Def.PowerKind;
	const bool bLockedMain = Phase == EZoogiPhase::Aiming && IsAimStep() && TargetLock.IsValid() && !bSoftLock;
	if (ZoogiUtil::IsArmedPower(P))
	{
		// Arm once the target is locked in Main, so the "armed" flash reads before the Battle camera swings in.
		if (bLockedMain && !bPowerArmed && LockTime > 0.25f) TryPower();
		return;
	}
	const FVector L = B->BallLocation();
	if (Phase == EZoogiPhase::Aiming)
	{
		CPUExplPrevN = 0;
		// Shadow Stun before shooting if a rival is already close.
		if (P == EZoogiPower::ShadowStun && bLockedMain && LockTime > 0.2f)
		{
			for (AZoogiBall* O : Balls) if (O && O != B && !O->bOut && FVector::Dist2D(O->BallLocation(), L) < ShadowRadius) { TryPower(); break; }
		}
		return;
	}
	if (P == EZoogiPower::WolfPack)
	{
		// Release the pack while still fast and with targets around (clones home on the 3 nearest).
		int32 Near = 0;
		for (AActor* T : LiveTargets(B)) if (FVector::Dist2D(ActorPos(T), L) < 900.f) ++Near;
		if ((Near >= 2 && PhaseTime > 0.2f) || PhaseTime > 0.7f) TryPower();
	}
	else if (P == EZoogiPower::Explosion)
	{
		int32 N = 0;
		bool bEdgeKill = false;
		for (AActor* T : LiveTargets(B))
		{
			const FVector D = ActorPos(T) - L;
			const float Dist = FVector2D(D.X, D.Y).Size();
			if (Dist >= ExplosionRadius * 0.85f) continue;
			++N;
			// A rival close to the rim inside the blast = likely knock-off.
			if (Cast<AZoogiBall>(T) && Dist > 1.f && Arena && Arena->DistanceToEdge(FVector2D(ActorPos(T)), FVector2D(D.X, D.Y) / Dist) < 450.f) bEdgeKill = true;
		}
		const bool bPastPeak = N >= 2 && N < CPUExplPrevN; // the crowd is starting to thin: blow now
		const bool bSlow = B->PlanarSpeed() < 200.f && PhaseTime > 0.5f;
		if (N >= 3 || bPastPeak || bEdgeKill || (N >= 1 && bSlow)) TryPower();
		CPUExplPrevN = FMath::Max(CPUExplPrevN, N);
	}
	else if (P == EZoogiPower::ShadowStun)
	{
		for (AZoogiBall* O : Balls)
		{
			if (O && O != B && !O->bOut && FVector::Dist2D(O->BallLocation(), L) < ShadowRadius + 40.f)
			{
				TryPower();
				break;
			}
		}
	}
}
