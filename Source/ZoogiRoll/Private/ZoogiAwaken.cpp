// Milestone "Awaken": Draw / Awaken (Infuse, Guard, Save), powered collisions, guard blocks, statuses and Resolve.
// Rules approved by Evan (Oct 4): one Zoogi per player, no HP (damage = knockback / status / score), deterministic crits,
// Guard covers only the next opponent attack, affinity map in ZoogiArtifacts::Affinity.
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiPlayerController.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

namespace
{
	FVector Pos(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}

	void Push(AActor* A, const FVector& Planar, float Up, int32 Credit)
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

	void AddToHand(UZoogiArtifactComponent* AC, EZoogiArtifact A)
	{
		if (A == EZoogiArtifact::None) return;
		AC->Hand.Add(A);
		while (AC->Hand.Num() > UZoogiArtifactComponent::HandCap)
		{
			UE_LOG(LogZoogi, Log, TEXT("Hand full: discarded %s"), *ZoogiArtifacts::Name(AC->Hand[0]));
			AC->Hand.RemoveAt(0); // oldest discarded
		}
	}

	/** Takes the picked artifact (0 = drawn, 1.. = hand slot); an unused drawn artifact is saved to the hand. */
	EZoogiArtifact TakePicked(UZoogiArtifactComponent* AC, int32 Pick)
	{
		EZoogiArtifact A = EZoogiArtifact::None;
		if (Pick <= 0 && AC->Drawn != EZoogiArtifact::None)
		{
			A = AC->Drawn;
			AC->Drawn = EZoogiArtifact::None;
			return A;
		}
		const int32 Slot = Pick <= 0 ? 0 : Pick - 1;
		if (!AC->Hand.IsValidIndex(Slot)) return EZoogiArtifact::None;
		A = AC->Hand[Slot];
		AC->Hand.RemoveAt(Slot);
		if (AC->Drawn != EZoogiArtifact::None) { AddToHand(AC, AC->Drawn); AC->Drawn = EZoogiArtifact::None; }
		return A;
	}

	float EdgeDistance(const AZoogiArena* Arena, const FVector& L)
	{
		const FVector2D P(L.X, L.Y);
		if (!Arena) return ZoogiConst::FloorRadius - P.Size();
		float Best = TNumericLimits<float>::Max();
		for (int32 k = 0; k < 8; ++k) Best = FMath::Min(Best, Arena->DistanceToEdge(P, FVector2D(1.f, 0.f).GetRotated(k * 45.f)));
		return Best;
	}
}

void AZoogiGameMode::EmitAnime(FName Event, AZoogiBall* Who, EZoogiArtifact A, AActor* Focus, const FVector& Where, float Magnitude, bool bFreeze, int32 TargetPlayer)
{
	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	if (!Anime) return;
	FZoogiCombatEvent E;
	E.Event = Event;
	if (Who) { E.ZoogiId = Who->Def.Id; E.ZoogiName = Who->Def.Name; E.Player = Who->PlayerIndex; }
	E.Artifact = A;
	E.TargetPlayer = TargetPlayer;
	E.Location = Where;
	E.Magnitude = Magnitude;
	E.bFreeze = bFreeze;
	E.Focus = Focus ? Focus : Who;
	bool bHuman = false;
	for (AZoogiBall* B : Balls) if (B && !B->bCPU) bHuman = true;
	E.bCPUOnly = !bHuman;
	Anime->Emit(E);
}

void AZoogiGameMode::RefreshArtifactMass(AZoogiBall* B, bool bShooting)
{
	if (!B || !B->Artifact) return;
	float Mul = 1.f;
	if (bShooting && B->Artifact->Infused != EZoogiArtifact::None) Mul *= ZoogiArtifacts::Get(B->Artifact->Infused).MassMul;
	if (!bShooting && B->Artifact->Guard != EZoogiArtifact::None) Mul *= ZoogiArtifacts::Get(B->Artifact->Guard).GuardMassMul;
	B->SetMassScale(Mul);
}

// ------------------------------------------------------------------ Draw / Awaken

void AZoogiGameMode::DoDraw()
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact) return;
	UZoogiArtifactComponent* AC = B->Artifact;
	if (AC->Drawn != EZoogiArtifact::None) { AddToHand(AC, AC->Drawn); AC->Drawn = EZoogiArtifact::None; }
	AC->Drawn = ZoogiArtifacts::Random();
	FString Extra;
	for (int32 i = 0; i < AC->BonusDraws; ++i)
	{
		const EZoogiArtifact X = ZoogiArtifacts::Random();
		AddToHand(AC, X);
		Extra += FString::Printf(TEXT(" +%s"), *ZoogiArtifacts::Name(X));
	}
	AC->BonusDraws = 0;
	AC->Pulse(0.35f);
	const FLinearColor C = ZoogiArtifacts::Color(AC->Drawn);
	AddPopup(FString::Printf(TEXT("DRAW: %s"), *ZoogiArtifacts::Name(AC->Drawn).ToUpper()), B->BallLocation() + FVector(0, 0, 150.f), C);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 110.f, 0.5f, false, 6.f, B);
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.6f, 0.8f);
	UE_LOG(LogZoogi, Log, TEXT("P%d %s draws %s%s (hand: %d)"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(AC->Drawn), *Extra, AC->Hand.Num());
}

EZoogiArtifact AZoogiGameMode::PickedArtifact() const
{
	const AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact) return EZoogiArtifact::None;
	const UZoogiArtifactComponent* AC = B->Artifact;
	if (AwakenPick <= 0) return AC->Drawn != EZoogiArtifact::None ? AC->Drawn : (AC->Hand.Num() ? AC->Hand[0] : EZoogiArtifact::None);
	return AC->Hand.IsValidIndex(AwakenPick - 1) ? AC->Hand[AwakenPick - 1] : EZoogiArtifact::None;
}

bool AZoogiGameMode::AwakenInfuse(int32 Pick)
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	const EZoogiArtifact A = TakePicked(B->Artifact, Pick);
	if (A == EZoogiArtifact::None) return false;
	B->Artifact->SetInfused(A);
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	const bool bAff = ZoogiArtifacts::Affinity(B->Def.Id) == A;
	AwakenChoiceText = FString::Printf(TEXT("INFUSE  %s%s"), *ZoogiArtifacts::Name(A).ToUpper(), bAff ? TEXT("  (AFFINITY)") : TEXT(""));
	const FLinearColor C = ZoogiArtifacts::Color(A);
	AddPopup(FString::Printf(TEXT("INFUSED  %s"), *ZoogiArtifacts::Name(A).ToUpper()), B->BallLocation() + FVector(0, 0, 160.f), C);
	if (bAff) AddPopup(FString::Printf(TEXT("AFFINITY: %s"), *B->Def.Ability.ToUpper()), B->BallLocation() + FVector(0, 0, 220.f), FLinearColor(1.f, 0.9f, 0.4f));
	AddFeed(FString::Printf(TEXT("P%d %s  infused %s%s"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), bAff ? TEXT("  (affinity!)") : TEXT("")), C);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 200.f, 0.6f, false, 10.f, B);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), C, 260.f, 0.7f, true, 8.f);
	AZoogiFX::Niagara(GetWorld(), *ZoogiArtifacts::Get(A).AuraNiagara.ToString(), B->BallLocation(), FRotator::ZeroRotator, 0.9f);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.9f, 0.9f);
	UE_LOG(LogZoogi, Log, TEXT("Awaken: P%d %s INFUSES %s%s"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), bAff ? TEXT(" (affinity)") : TEXT(""));
	EmitAnime(TEXT("Infuse"), B, A, B, B->BallLocation(), 1.f, false);
	return true;
}

bool AZoogiGameMode::AwakenGuard(int32 Pick)
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	const EZoogiArtifact A = TakePicked(B->Artifact, Pick);
	if (A == EZoogiArtifact::None) return false;
	B->Artifact->SetGuard(A, TurnCounter);
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	AwakenChoiceText = FString::Printf(TEXT("GUARD  %s"), *ZoogiArtifacts::Name(A).ToUpper());
	const FLinearColor C = ZoogiArtifacts::Color(A);
	AddPopup(FString::Printf(TEXT("GUARD  %s"), *ZoogiArtifacts::Name(A).ToUpper()), B->BallLocation() + FVector(0, 0, 160.f), C);
	AddFeed(FString::Printf(TEXT("P%d %s  guards with %s (next opponent attack)"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A)), C);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 130.f, 0.6f, false, 6.f, B);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.7f, 0.7f);
	UE_LOG(LogZoogi, Log, TEXT("Awaken: P%d %s GUARDS with %s (turn %d)"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), TurnCounter);
	EmitAnime(TEXT("Guard"), B, A, B, B->BallLocation(), 1.f, false);
	return true;
}

bool AZoogiGameMode::AwakenSave()
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	UZoogiArtifactComponent* AC = B->Artifact;
	const EZoogiArtifact A = AC->Drawn;
	if (A != EZoogiArtifact::None) { AddToHand(AC, A); AC->Drawn = EZoogiArtifact::None; }
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	AwakenChoiceText = A != EZoogiArtifact::None ? FString::Printf(TEXT("SAVE  %s"), *ZoogiArtifacts::Name(A).ToUpper()) : FString(TEXT("SAVE"));
	if (A != EZoogiArtifact::None) AddPopup(FString::Printf(TEXT("SAVED  %s"), *ZoogiArtifacts::Name(A).ToUpper()), B->BallLocation() + FVector(0, 0, 160.f), ZoogiArtifacts::Color(A));
	UE_LOG(LogZoogi, Log, TEXT("Awaken: P%d %s SAVES %s (hand %d)"), B->PlayerIndex + 1, *B->Def.Name, A != EZoogiArtifact::None ? *ZoogiArtifacts::Name(A) : TEXT("nothing"), AC->Hand.Num());
	return true;
}

void AZoogiGameMode::PlanCPUAwaken()
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || bAwakenDone) return;
	UZoogiArtifactComponent* AC = B->Artifact;
	if (AC->Drawn == EZoogiArtifact::None && AC->Hand.Num() == 0) { AwakenSave(); return; }
	// Prefer the affinity artifact wherever it is.
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	int32 Pick = AC->Drawn != EZoogiArtifact::None ? 0 : 1;
	if (AC->Drawn != Aff) for (int32 i = 0; i < AC->Hand.Num(); ++i) if (AC->Hand[i] == Aff) { Pick = i + 1; break; }
	const EZoogiArtifact A = Pick == 0 ? AC->Drawn : AC->Hand[Pick - 1];
	const bool bRival = Cast<AZoogiBall>(CPULock.Get()) != nullptr;
	const bool bAff = A == Aff;
	const float Edge = EdgeDistance(Arena, B->BallLocation());
	const bool bFull = AC->Drawn != EZoogiArtifact::None && AC->Hand.Num() >= UZoogiArtifactComponent::HandCap;
	const TCHAR* Why = TEXT("");
	if (bPowerTest || bRival || bAff) { Why = bPowerTest ? TEXT("test") : bAff ? TEXT("affinity") : TEXT("rival target"); AwakenInfuse(Pick); }
	else if (Edge < 450.f && AC->Guard == EZoogiArtifact::None) { Why = TEXT("near the edge"); AwakenGuard(Pick); }
	else if (bFull || AC->Hand.Num() >= 1) { Why = bFull ? TEXT("hand full") : TEXT("hand stocked"); AwakenInfuse(Pick); }
	else { Why = TEXT("orb target, saving"); AwakenSave(); }
	UE_LOG(LogZoogi, Log, TEXT("CPU P%d Awaken decision: %s (%s, edge %.0f cm)"), B->PlayerIndex + 1, *AwakenChoiceText, Why, Edge);
	// The plan may change with Titan's mass / speed; re-plan the shot so the power fraction accounts for it.
}

void AZoogiGameMode::ApplyStatusOnShot(AZoogiBall* Ball, FVector2D& V)
{
	if (!Ball || !Ball->Artifact) return;
	UZoogiArtifactComponent* AC = Ball->Artifact;
	const FVector Above = Ball->BallLocation() + FVector(0, 0, 130.f);
	if (AC->Status == EZoogiStatus::Burned)
	{
		V *= 0.85f;
		AddPopup(TEXT("BURNED  -15%"), Above, ZoogiArtifacts::Color(EZoogiArtifact::Ember));
		UE_LOG(LogZoogi, Log, TEXT("P%d shot weakened by Burned (-15%%)"), Ball->PlayerIndex + 1);
		AC->Status = EZoogiStatus::None; AC->StatusTurns = 0;
	}
	else if (AC->Status == EZoogiStatus::Shocked)
	{
		const float Cap = ZoogiConst::LaunchSpeedForPull(0.7f);
		if (V.Size() > Cap) V = V.GetSafeNormal() * Cap;
		AddPopup(TEXT("SHOCKED  MAX 70%"), Above, ZoogiArtifacts::Color(EZoogiArtifact::Storm));
		UE_LOG(LogZoogi, Log, TEXT("P%d shot capped by Shocked (70%%)"), Ball->PlayerIndex + 1);
		AC->Status = EZoogiStatus::None; AC->StatusTurns = 0;
	}
	if (bArtifactMode && AC->Infused != EZoogiArtifact::None)
	{
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(AC->Infused);
		V *= R.LaunchSpeedMul;
		RefreshArtifactMass(Ball, true);
		// Affinity: the Zoogi's signature power rides along. Armed powers fire with this shot (OnArmedShot), instant ones on impact.
		if (ZoogiArtifacts::Affinity(Ball->Def.Id) == AC->Infused && ZoogiUtil::IsArmedPower(Ball->Def.PowerKind))
		{
			bPowerArmed = true;
			Ball->Charges = FMath::Max(Ball->Charges, 1);
			UE_LOG(LogZoogi, Log, TEXT("Affinity: %s's %s armed with the infused shot"), *Ball->Def.Name, *Ball->Def.Ability);
		}
	}
}

// ------------------------------------------------------------------ Battle: powered collision

void AZoogiGameMode::ApplyInfuseImpact(AZoogiBall* Shooter, AActor* Other)
{
	if (!bArtifactMode || bInfusedContact || !Shooter || !Other || !Shooter->Artifact) return;
	const EZoogiArtifact A = Shooter->Artifact->Infused;
	if (A == EZoogiArtifact::None) return;
	bInfusedContact = true;
	const FZoogiArtifactRow& R = ZoogiArtifacts::Get(A);
	const FVector SP = Shooter->BallLocation();
	const FVector OP = Pos(Other);
	const FVector Mid = (SP + OP) * 0.5f;
	const int32 Credit = Shooter->PlayerIndex;
	AZoogiBall* OB = Cast<AZoogiBall>(Other);

	// Deterministic crit: speed the shot still had at contact (launch speed minus rolling loss over the travelled distance)
	// and the cut angle between the travel direction and the contact normal.
	const float Decel = Shooter->RollingDecel;
	const float Travel = FVector::Dist2D(ShotStart, SP);
	ContactImpactSpeed = FMath::Sqrt(FMath::Max(0.f, ShotSpeed * ShotSpeed - 2.f * Decel * Travel));
	FVector2D TravelDir = FVector2D(SP - ShotStart).GetSafeNormal();
	if (TravelDir.IsNearlyZero()) TravelDir = FVector2D(Shooter->GetBallVelocity()).GetSafeNormal();
	const FVector2D N = FVector2D(OP - SP).GetSafeNormal();
	const float Cut = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(TravelDir, N), -1.f, 1.f)));
	bLastHitCrit = ShotSpeed > 1.f && ContactImpactSpeed >= ZoogiStageTune::CritImpactFrac * ShotSpeed && Cut <= ZoogiStageTune::CritMaxCutDeg;
	const float Mul = bLastHitCrit ? ZoogiStageTune::CritEffectMul : 1.f;
	const FLinearColor C = R.CoreColor;

	Shooter->Artifact->ImpactPulse(bLastHitCrit ? 1.5f : 1.f);
	AZoogiFX::Spawn(GetWorld(), Mid, C, 140.f * Mul, 0.35f, false, 16.f);
	AZoogiFX::Spawn(GetWorld(), Mid - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), R.RimColor, FMath::Max(R.ImpactRadius, 200.f) * Mul, 0.6f, true, 10.f);
	AZoogiFX::Niagara(GetWorld(), *R.ImpactNiagara.ToString(), Mid, FRotator::ZeroRotator, bLastHitCrit ? 1.4f : 1.f);
	if (bLastHitCrit)
	{
		AZoogiFX::Spawn(GetWorld(), Mid, FLinearColor::White, 260.f, 0.25f, false, 25.f);
		AddPopup(TEXT("CRITICAL!"), Mid + FVector(0, 0, 200.f), FLinearColor(1.f, 0.95f, 0.5f));
	}
	ZoogiUtil::PlaySfx(this, A == EZoogiArtifact::Ember || A == EZoogiArtifact::Titan ? TEXT("Explosion") : TEXT("Zap"), 0.9f, A == EZoogiArtifact::Frost ? 1.5f : A == EZoogiArtifact::Void ? 0.6f : 1.f);

	int32 Affected = 0;
	switch (A)
	{
	case EZoogiArtifact::Ember:
	{
		const float Rad = R.ImpactRadius * Mul;
		// Balance Oct 4: with affinity (Hotstreak) the signature explosion also fires, so Ember's own burst is weaker.
		const float AffPush = ZoogiArtifacts::Affinity(Shooter->Def.Id) == A ? 0.55f : 1.f;
		for (AActor* T : LiveTargets(Shooter))
		{
			const FVector D = Pos(T) - Mid;
			const float Dist = FVector2D(D.X, D.Y).Size();
			if (Dist > Rad + ZoogiConst::ZoogiRadius) continue;
			const float K = 1.f - FMath::Clamp(Dist / (Rad + ZoogiConst::ZoogiRadius), 0.f, 1.f);
			const FVector Dir = T == Other ? FVector(N.X, N.Y, 0.f) : FVector(D.X, D.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector(N.X, N.Y, 0.f));
			Push(T, Dir * R.ImpactPush * Mul * AffPush * (0.5f + 0.5f * K), 150.f, Credit);
			++Affected;
		}
		if (OB && OB->Artifact) { OB->Artifact->Status = EZoogiStatus::Burned; OB->Artifact->StatusTurns = R.StatusTurns; AddPopup(TEXT("BURNED"), OP + FVector(0, 0, 120.f), C); }
		break;
	}
	case EZoogiArtifact::Storm:
	{
		TArray<AActor*> Hit = { Other };
		Push(Other, FVector(N.X, N.Y, 0.f) * R.ImpactPush * Mul, 120.f, Credit);
		AActor* From = Other;
		for (int32 c = 0; c < R.ChainCount; ++c)
		{
			AActor* Best = nullptr;
			float BestD = R.ImpactRadius * Mul;
			for (AActor* T : LiveTargets(Shooter))
			{
				if (Hit.Contains(T)) continue;
				const float D = FVector::Dist2D(Pos(T), Pos(From));
				if (D < BestD) { BestD = D; Best = T; }
			}
			if (!Best) break;
			const FVector A0 = Pos(From), A1 = Pos(Best);
			for (int32 s = 1; s <= 5; ++s) AZoogiFX::Spawn(GetWorld(), FMath::Lerp(A0, A1, s / 6.f) + FVector(0, 0, FMath::FRandRange(-20.f, 40.f)), FLinearColor(0.75f, 0.92f, 1.f), 45.f, 0.3f, false, 18.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_White"), A1, FRotator::ZeroRotator, 0.6f);
			Push(Best, (A1 - A0).GetSafeNormal2D() * R.ImpactPush * 0.8f * Mul, 120.f, Credit);
			Hit.Add(Best);
			From = Best;
		}
		Affected = Hit.Num();
		if (OB && OB->Artifact) { OB->Artifact->Status = EZoogiStatus::Shocked; OB->Artifact->StatusTurns = R.StatusTurns; AddPopup(TEXT("SHOCKED"), OP + FVector(0, 0, 120.f), C); }
		break;
	}
	case EZoogiArtifact::Frost:
	{
		if (OB)
		{
			OB->RollingDecel = FMath::Min(OB->RollingDecel, ZoogiConst::IceRollingDecel * 1.5f / Mul);
			FrozenBalls.AddUnique(OB);
			if (OB->Artifact) { OB->Artifact->Status = EZoogiStatus::Frozen; OB->Artifact->StatusTurns = R.StatusTurns; }
			AddPopup(TEXT("FROZEN - ICE SLIDE"), OP + FVector(0, 0, 120.f), C);
			for (int32 s = 0; s < 4; ++s) AZoogiFX::SpawnTrail(GetWorld(), OP - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f) + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 0.f), C, 70.f, 2.2f, 4.f);
		}
		else
		{
			Push(Other, FVector(N.X, N.Y, 0.f) * R.ImpactPush * Mul, 0.f, Credit);
		}
		for (AActor* T : LiveTargets(Shooter))
		{
			if (T == Other || !Cast<AZoogiOrb>(T)) continue; // shards push small things
			const FVector D = Pos(T) - Mid;
			if (FVector2D(D.X, D.Y).Size() > R.ImpactRadius * Mul) continue;
			Push(T, FVector(D.X, D.Y, 0.f).GetSafeNormal() * R.ImpactPush * Mul, 80.f, Credit);
			++Affected;
		}
		break;
	}
	case EZoogiArtifact::Titan:
	{
		const float Rad = R.ImpactRadius * Mul;
		AZoogiFX::Spawn(GetWorld(), SP - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), FLinearColor(0.75f, 0.6f, 0.4f), Rad * 1.3f, 0.7f, true, 9.f);
		for (AActor* T : LiveTargets(Shooter))
		{
			const FVector D = Pos(T) - SP;
			const float Dist = FVector2D(D.X, D.Y).Size();
			if (Dist > Rad + ZoogiConst::ZoogiRadius) continue;
			const float K = 1.f - FMath::Clamp(Dist / (Rad + ZoogiConst::ZoogiRadius), 0.f, 1.f);
			Push(T, FVector(D.X, D.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector(N.X, N.Y, 0.f)) * R.ImpactPush * Mul * K, 200.f * K, Credit);
			++Affected;
		}
		break;
	}
	case EZoogiArtifact::Void:
	{
		PullCenter = Mid;
		PullLeft = R.PullTime * Mul;
		PullAccel = R.PullAccel * Mul;
		PullRadius = R.ImpactRadius;
		PullCredit = Credit;
		AZoogiFX::Spawn(GetWorld(), Mid - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), R.RimColor, R.ImpactRadius, PullLeft, true, 8.f);
		break;
	}
	default: break;
	}

	// Affinity: instant signature powers fire on the powered impact (armed ones were armed at launch).
	if (ZoogiArtifacts::Affinity(Shooter->Def.Id) == A && !ZoogiUtil::IsArmedPower(Shooter->Def.PowerKind) && !bPowerUsed)
	{
		Shooter->Charges = FMath::Max(Shooter->Charges, 1);
		FireInstantPower(Shooter);
	}

	UE_LOG(LogZoogi, Log, TEXT("Powered hit: P%d %s %s -> %s (contact %.0f / launch %.0f cm/s = %.0f%%, cut %.1f deg)%s, affected %d"),
		Credit + 1, *Shooter->Def.Name, *ZoogiArtifacts::Name(A), *TargetName(Other), ContactImpactSpeed, ShotSpeed,
		ShotSpeed > 1.f ? 100.f * ContactImpactSpeed / ShotSpeed : 0.f, Cut, bLastHitCrit ? TEXT(" CRITICAL") : TEXT(""), Affected);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnPoweredHit(bLastHitCrit);
	EmitAnime(bLastHitCrit ? TEXT("Critical") : TEXT("PoweredHit"), Shooter, A, OB ? (AActor*)OB : Other, Mid, Mul, true, OB ? OB->PlayerIndex : -1);
}

// ------------------------------------------------------------------ Guard

void AZoogiGameMode::OnGuardedHit(AZoogiBall* Victim, AZoogiBall* Attacker)
{
	if (!bArtifactMode || !Victim || !Attacker || !Victim->Artifact || Victim->Artifact->Guard == EZoogiArtifact::None) return;
	if (Phase != EZoogiPhase::Rolling) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - Victim->Artifact->LastGuardHitTime < 0.25f) return; // same contact, a few frames later
	Victim->Artifact->LastGuardHitTime = Now;
	Victim->Artifact->bGuardHitPending = true; // damped after the solver step (TickArtifacts)
	Victim->Artifact->GuardAttacker = Attacker;
}

void AZoogiGameMode::TickArtifacts(float Dt)
{
	if (!bArtifactMode) return;
	for (AZoogiBall* V : Balls)
	{
		if (!V || !V->Artifact || !V->Artifact->bGuardHitPending) continue;
		UZoogiArtifactComponent* AC = V->Artifact;
		AC->bGuardHitPending = false;
		const EZoogiArtifact G = AC->Guard;
		if (G == EZoogiArtifact::None) continue;
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(G);
		AZoogiBall* Att = AC->GuardAttacker.Get();
		const FVector VV = V->GetBallVelocity();
		V->SetPlanarVelocity(FVector(VV.X, VV.Y, 0.f) * R.GuardKnockbackMul);
		if (Att && !Att->bOut)
		{
			const FVector AV = Att->GetBallVelocity();
			FVector P(AV.X, AV.Y, 0.f);
			switch (G)
			{
			case EZoogiArtifact::Ember: P = P * R.GuardAttackerMul - (V->BallLocation() - Att->BallLocation()).GetSafeNormal2D() * 250.f; break; // flame ring bounce-back
			case EZoogiArtifact::Storm: if (Att->Artifact) { Att->Artifact->Status = EZoogiStatus::Shocked; Att->Artifact->StatusTurns = 1; } AddPopup(TEXT("SHOCKED"), Att->BallLocation() + FVector(0, 0, 120.f), R.CoreColor); break;
			case EZoogiArtifact::Frost: P *= R.GuardAttackerMul; break;
			case EZoogiArtifact::Void: P = P.RotateAngleAxis(FMath::Sign(FVector::CrossProduct(P, V->BallLocation() - Att->BallLocation()).Z + 0.01f) * -R.GuardReflectDeg, FVector::UpVector); break;
			default: break; // Titan: the mass did the work
			}
			Att->SetPlanarVelocity(P);
		}
		AC->GuardFlash();
		++TurnGuardBlocks;
		AZoogiFX::Spawn(GetWorld(), V->BallLocation(), R.RimColor, 120.f, 0.35f, false, 12.f, V);
		AZoogiFX::Niagara(GetWorld(), *R.AuraNiagara.ToString(), V->BallLocation(), FRotator::ZeroRotator, 0.8f);
		AddPopup(FString::Printf(TEXT("GUARD  -%d%%"), FMath::RoundToInt((1.f - R.GuardKnockbackMul) * 100.f)), V->BallLocation() + FVector(0, 0, 150.f), R.CoreColor);
		AddFeed(FString::Printf(TEXT("P%d %s  blocked with %s"), V->PlayerIndex + 1, *V->Def.Name, *ZoogiArtifacts::Name(G)), R.CoreColor);
		ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.9f, 0.6f);
		UE_LOG(LogZoogi, Log, TEXT("Guard block: P%d %s (%s) took %.0f%% knockback from P%d"), V->PlayerIndex + 1, *V->Def.Name, *ZoogiArtifacts::Name(G),
			R.GuardKnockbackMul * 100.f, Att ? Att->PlayerIndex + 1 : 0);
		EmitAnime(TEXT("GuardBlock"), V, G, V, V->BallLocation(), 1.f, true, Att ? Att->PlayerIndex : -1);
	}

	// Void Shard gravity well.
	if (PullLeft > 0.f)
	{
		PullLeft -= Dt;
		AZoogiBall* Shooter = GetActiveBall();
		for (AActor* T : LiveTargets(Shooter))
		{
			const FVector D = PullCenter - Pos(T);
			const float Dist = FVector2D(D.X, D.Y).Size();
			if (Dist > PullRadius || Dist < 60.f) continue;
			const float K = 0.3f + 0.7f * (1.f - Dist / PullRadius);
			Push(T, FVector(D.X, D.Y, 0.f).GetSafeNormal() * PullAccel * K * Dt, 0.f, PullCredit);
		}
		static float SwirlClock = 0.f;
		SwirlClock += Dt;
		if (SwirlClock > 0.12f)
		{
			SwirlClock = 0.f;
			AZoogiFX::Spawn(GetWorld(), PullCenter - FVector(0, 0, ZoogiConst::ZoogiRadius - 5.f), FLinearColor(0.55f, 0.25f, 1.f), PullRadius * 0.35f, 0.3f, true, 7.f);
		}
		if (PullLeft <= 0.f) UE_LOG(LogZoogi, Log, TEXT("Void pull ended"));
	}
}

// ------------------------------------------------------------------ Resolve

void AZoogiGameMode::CommitLedger(int32& ShooterScores, int32& KOs)
{
	if (ResolveDeltas.Num() != Balls.Num()) ResolveDeltas.Init(0, Balls.Num());
	for (const FZoogiLedgerEntry& E : Ledger)
	{
		if (!Balls.IsValidIndex(E.Player)) continue;
		AZoogiBall* B = Balls[E.Player];
		const int32 Before = B->Score;
		B->Score = FMath::Max(0, B->Score + E.Points);
		ResolveDeltas[E.Player] += B->Score - Before;
		if (E.bKO) { B->KnockOuts++; ++KOs; }
		if (!E.bKO && !E.bFall) B->OrbsScored++;
		if (E.bStar)
		{
			if (bArtifactMode && B->Artifact) B->Artifact->BonusDraws = FMath::Min(2, B->Artifact->BonusDraws + 1);
			else B->Charges = FMath::Min(3, B->Charges + 1);
		}
		if (E.Player == ActivePlayer && E.Points > 0) ++ShooterScores;
	}
	Ledger.Reset();
	ResolveSummary.Reset();
	for (int32 i = 0; i < ResolveDeltas.Num(); ++i)
	{
		if (ResolveDeltas[i] == 0) continue;
		ResolveSummary += FString::Printf(TEXT("%sP%d %+d"), ResolveSummary.IsEmpty() ? TEXT("") : TEXT("     "), i + 1, ResolveDeltas[i]);
	}
	if (ResolveSummary.IsEmpty()) ResolveSummary = TEXT("No score change");
}

void AZoogiGameMode::DoResolve()
{
	PullLeft = 0.f;
	AZoogiBall* Shooter = GetActiveBall();
	const EZoogiArtifact ShotArtifact = Shooter && Shooter->Artifact ? Shooter->Artifact->Infused : EZoogiArtifact::None;

	// 1) Commit the roll's ledger (orbs, stars, knock-outs, falls).
	ResolveDeltas.Init(0, Balls.Num());
	int32 ShooterScores = 0, KOs = 0;
	CommitLedger(ShooterScores, KOs);

	// 2) Statuses / guards / infusion end.
	for (const TWeakObjectPtr<AZoogiBall>& W : FrozenBalls)
	{
		if (AZoogiBall* B = W.Get())
		{
			B->RollingDecel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
			if (B->Artifact && B->Artifact->Status == EZoogiStatus::Frozen) { B->Artifact->Status = EZoogiStatus::None; B->Artifact->StatusTurns = 0; }
		}
	}
	FrozenBalls.Reset();
	if (Shooter && Shooter->Artifact) Shooter->Artifact->ClearInfused();
	for (AZoogiBall* B : Balls)
	{
		if (!B || !B->Artifact) continue;
		if (B->Artifact->Guard != EZoogiArtifact::None && B->Artifact->GuardSetTurn < TurnCounter)
		{
			UE_LOG(LogZoogi, Log, TEXT("Guard expired on P%d (%s)"), B->PlayerIndex + 1, *ZoogiArtifacts::Name(B->Artifact->Guard));
			B->Artifact->ClearGuard();
		}
		RefreshArtifactMass(B, false);
	}

	// Notes for the Resolve panel.
	{
		TArray<FString> N;
		if (ShotArtifact != EZoogiArtifact::None)
			N.Add(bInfusedContact ? FString::Printf(TEXT("%s powered hit%s"), *ZoogiArtifacts::Name(ShotArtifact), bLastHitCrit ? TEXT("  CRITICAL") : TEXT("")) : FString::Printf(TEXT("%s missed"), *ZoogiArtifacts::Name(ShotArtifact)));
		if (KOs > 0) N.Add(FString::Printf(TEXT("KO x%d"), KOs));
		if (ShooterScores >= 2) N.Add(FString::Printf(TEXT("Combo x%d"), ShooterScores));
		if (TurnGuardBlocks > 0) N.Add(FString::Printf(TEXT("Guard block x%d"), TurnGuardBlocks));
		ResolveNotes = FString::Join(N, TEXT("     "));
	}

	// 3) Anime events for the outcome (final now).
	if (Shooter)
	{
		if (ShooterScores >= 2) EmitAnime(TEXT("Combo"), Shooter, ShotArtifact, Shooter, Shooter->BallLocation(), ShooterScores, false);
		else if (KOs > 0) EmitAnime(TEXT("KO"), Shooter, ShotArtifact, Shooter, Shooter->BallLocation(), KOs, false);
		else if (ShotArtifact != EZoogiArtifact::None && !bInfusedContact) EmitAnime(TEXT("Miss"), Shooter, ShotArtifact, Shooter, Shooter->BallLocation(), 1.f, false);
	}
	UE_LOG(LogZoogi, Log, TEXT("Resolve (turn %d, P%d): %s (%d scoring, %d KO)%s"), TurnCounter, ActivePlayer + 1, *ResolveSummary, ShooterScores, KOs,
		bLastHitCrit ? TEXT(", crit") : TEXT(""));
}
