// Milestone "Awaken": Draw / Awaken (Infuse, Guard, Save), powered collisions, guard blocks, statuses and Resolve.
// Rules approved by Evan (Oct 4): one Zoogi per player, no HP (damage = knockback / status / score), deterministic crits,
// Guard covers only the next opponent attack, affinity map in ZoogiArtifacts::Affinity.
#include "ZoogiGameMode.h"
#include "Misc/Crc.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiAwakenFX.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiPlayerController.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "ZoogiBattleStateMachine.h"

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

	/** Adds to the hand; returns the artifact discarded to stay under the cap (oldest first), or None. */
	EZoogiArtifact AddToHand(UZoogiArtifactComponent* AC, EZoogiArtifact A)
	{
		EZoogiArtifact Discarded = EZoogiArtifact::None;
		if (A == EZoogiArtifact::None) return Discarded;
		AC->Hand.Add(A);
		while (AC->Hand.Num() > UZoogiArtifactComponent::HandCap)
		{
			UE_LOG(LogZoogi, Log, TEXT("Hand full: discarded %s"), *ZoogiArtifacts::Name(AC->Hand[0]));
			Discarded = AC->Hand[0];
			AC->Hand.RemoveAt(0); // oldest discarded
		}
		return Discarded;
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
	AC->Drawn = ZoogiArtifacts::Random(&GameRng);
	FString Extra;
	for (int32 i = 0; i < AC->BonusDraws; ++i)
	{
		const EZoogiArtifact X = ZoogiArtifacts::Random(&GameRng);
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
	if (CardsOn()) return false; // v3: cards replace the artifact hand
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	EZoogiArtifact A = TakePicked(B->Artifact, Pick);
	// test flag (Oct 14): -ZoogiForceArt=Ember|Storm|Frost|Titan|Void forces the infused artifact (Void 1 / Void 2 captures)
	{ FString FA; if (A != EZoogiArtifact::None && FParse::Value(FCommandLine::Get(), TEXT("ZoogiForceArt="), FA)) for (EZoogiArtifact X : { EZoogiArtifact::Ember, EZoogiArtifact::Storm, EZoogiArtifact::Frost, EZoogiArtifact::Titan, EZoogiArtifact::Void }) if (ZoogiArtifacts::Key(X) == FA) A = X; }
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
	if (ZoogiUtil::ShowZoogiSparkles()) AZoogiFX::Niagara(GetWorld(), *ZoogiArtifacts::Get(A).AuraNiagara.ToString(), B->BallLocation(), FRotator::ZeroRotator, 0.9f); // Oct 8: ZoogiSparkles
	AZoogiAwakenFX::OnInfuse(GetWorld(), B, A); // launch-power fire ring (+ Shock chain lightning / Void portal for those artifacts)
	UE_LOG(LogZoogi, Log, TEXT("Awaken: P%d %s INFUSES %s%s"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), bAff ? TEXT(" (affinity)") : TEXT(""));
	EmitAnime(TEXT("Infuse"), B, A, B, B->BallLocation(), 1.f, false);
	return true;
}

bool AZoogiGameMode::AwakenGuard(int32 Pick)
{
	if (CardsOn()) return false; // v3: cards replace the artifact hand
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	EZoogiArtifact A = TakePicked(B->Artifact, Pick);
	// test flag (Oct 14): -ZoogiForceArt=Ember|Storm|Frost|Titan|Void forces the infused artifact (Void 1 / Void 2 captures)
	{ FString FA; if (A != EZoogiArtifact::None && FParse::Value(FCommandLine::Get(), TEXT("ZoogiForceArt="), FA)) for (EZoogiArtifact X : { EZoogiArtifact::Ember, EZoogiArtifact::Storm, EZoogiArtifact::Frost, EZoogiArtifact::Titan, EZoogiArtifact::Void }) if (ZoogiArtifacts::Key(X) == FA) A = X; }
	if (A == EZoogiArtifact::None) return false;
	B->Artifact->SetGuard(A, TurnCounter);
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	AwakenChoiceText = FString::Printf(TEXT("GUARD  %s"), *ZoogiArtifacts::Name(A).ToUpper());
	const FLinearColor C = ZoogiArtifacts::Color(A);
	AddPopup(FString::Printf(TEXT("GUARD  %s"), *ZoogiArtifacts::Name(A).ToUpper()), B->BallLocation() + FVector(0, 0, 160.f), C);
	AddFeed(FString::Printf(TEXT("P%d %s  guards with %s (next opponent attack)"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A)), C);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 130.f, 0.6f, false, 6.f, B);
	UE_LOG(LogZoogi, Log, TEXT("Awaken: P%d %s GUARDS with %s (turn %d)"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), TurnCounter);
	EmitAnime(TEXT("Guard"), B, A, B, B->BallLocation(), 1.f, false);
	return true;
}

bool AZoogiGameMode::AwakenSave()
{
	if (CardsOn()) return false; // v3: cards replace the artifact hand
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	UZoogiArtifactComponent* AC = B->Artifact;
	const EZoogiArtifact A = AC->Drawn;
	EZoogiArtifact Dropped = EZoogiArtifact::None;
	if (A != EZoogiArtifact::None) { Dropped = AddToHand(AC, A); AC->Drawn = EZoogiArtifact::None; }
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	AwakenChoiceText = A != EZoogiArtifact::None ? FString::Printf(TEXT("SAVE  %s"), *ZoogiArtifacts::Name(A).ToUpper()) : FString(TEXT("SAVE"));
	if (A != EZoogiArtifact::None) AddPopup(FString::Printf(TEXT("SAVED  %s"), *ZoogiArtifacts::Name(A).ToUpper()), B->BallLocation() + FVector(0, 0, 160.f), ZoogiArtifacts::Color(A));
	if (Dropped != EZoogiArtifact::None)
	{
		AddPopup(FString::Printf(TEXT("DISCARDED  %s"), *ZoogiArtifacts::Name(Dropped).ToUpper()), B->BallLocation() + FVector(0, 0, 215.f), FLinearColor(0.65f, 0.68f, 0.75f));
		AddFeed(FString::Printf(TEXT("P%d %s  saved %s, hand full: %s discarded"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), *ZoogiArtifacts::Name(Dropped)), FLinearColor(0.65f, 0.68f, 0.75f));
	}
	UE_LOG(LogZoogi, Log, TEXT("Awaken: P%d %s SAVES %s (hand %d)"), B->PlayerIndex + 1, *B->Def.Name, A != EZoogiArtifact::None ? *ZoogiArtifacts::Name(A) : TEXT("nothing"), AC->Hand.Num());
	return true;
}

void AZoogiGameMode::PlanCPUAwaken()
{
	if (CardsOn()) { CardsPlanCPU(); return; }
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || bAwakenDone) return;
	UZoogiArtifactComponent* AC = B->Artifact;
	if (AC->Drawn == EZoogiArtifact::None && AC->Hand.Num() == 0) { AwakenSave(); return; }
	if (bRockTest) { AwakenSave(); UE_LOG(LogZoogi, Log, TEXT("CPU P%d Awaken decision: SAVE (rock test)"), B->PlayerIndex + 1); return; }
	// Prefer the affinity artifact wherever it is.
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	int32 Pick = AC->Drawn != EZoogiArtifact::None ? 0 : 1;
	if (AC->Drawn != Aff) for (int32 i = 0; i < AC->Hand.Num(); ++i) if (AC->Hand[i] == Aff) { Pick = i + 1; break; }
	const EZoogiArtifact A = Pick == 0 ? AC->Drawn : AC->Hand[Pick - 1];
	const bool bRival = Cast<AZoogiBall>(CPULock.Get()) != nullptr;
	const bool bAff = A == Aff;
	const float Edge = EdgeDistance(Arena, B->BallLocation());
	const bool bFull = AC->Drawn != EZoogiArtifact::None && AC->Hand.Num() >= UZoogiArtifactComponent::HandCap;
	bool bLeader = B->Score > 0;
	for (const AZoogiBall* O : Balls) if (O && O != B && O->Score >= B->Score) bLeader = false;
	const TCHAR* Why = TEXT("");
	if (bPowerTest || bRival || bAff) { Why = bPowerTest ? TEXT("test") : bAff ? TEXT("affinity") : TEXT("rival target"); AwakenInfuse(Pick); }
	else if (AC->Guard == EZoogiArtifact::None && (Edge < 450.f || (bLeader && GameRng.FRand() < 0.5f) || GameRng.FRand() < 0.2f))
	{
		Why = Edge < 450.f ? TEXT("near the edge") : bLeader ? TEXT("protecting the lead") : TEXT("playing safe");
		AwakenGuard(Pick);
	}
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
		// (Static Shock, now Luma's Hype Shock, is the exception: phasing through would skip the powered contact, so it stuns on impact instead.)
		if (ZoogiArtifacts::Affinity(Ball->Def.Id) == AC->Infused && ZoogiUtil::IsArmedPower(Ball->Def.PowerKind) && Ball->Def.PowerKind != EZoogiPower::StaticShock)
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
		const float AffPush = ZoogiArtifacts::Affinity(Shooter->Def.Id) == A ? 0.3f : 1.f; // 0.55 -> 0.3 (bal4+5: Hotstreak's affinity turns scored 91 pts/turn, others 25-67)
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
	else if (ZoogiArtifacts::Affinity(Shooter->Def.Id) == A && Shooter->Def.PowerKind == EZoogiPower::StaticShock && !bPowerUsed && OB)
	{
		// Balance Oct 4: Bolt's affinity used to phase through the target (affinity turns scored half of his plain infused
		// turns). Now the shock lands with the hit: the victim keeps the knock and skips its next turn.
		bPowerUsed = true;
		OB->StunTurns = FMath::Max(OB->StunTurns, 1);
		OB->LastHitPlayer = Shooter->PlayerIndex;
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Yellow"), OB->BallLocation());
		AddPopup(FString::Printf(TEXT("%s!  STUNNED"), *Shooter->Def.Ability.ToUpper()), OB->BallLocation() + FVector(0, 0, 110.f), FLinearColor(1.f, 0.85f, 0.15f));
		ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.8f, 1.6f);
		UE_LOG(LogZoogi, Log, TEXT("Affinity: %s's %s stunned P%d on impact"), *Shooter->Def.Name, *Shooter->Def.Ability, OB->PlayerIndex + 1);
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
	if (IsAlly(Victim, Attacker)) return; // 2v2: a teammate bump never triggers the guard
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
		if (bComboMode && ComboGuardBreak(V, Att)) continue; // LAUNCH / crit shatters the guard: full knockback
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
		if (ZoogiUtil::ShowZoogiSparkles()) AZoogiFX::Niagara(GetWorld(), *R.AuraNiagara.ToString(), V->BallLocation(), FRotator::ZeroRotator, 0.8f); // Oct 8: ZoogiSparkles
		AddPopup(FString::Printf(TEXT("GUARD  -%d%%"), FMath::RoundToInt((1.f - R.GuardKnockbackMul) * 100.f)), V->BallLocation() + FVector(0, 0, 150.f), R.CoreColor);
		AddFeed(FString::Printf(TEXT("P%d %s  blocked with %s"), V->PlayerIndex + 1, *V->Def.Name, *ZoogiArtifacts::Name(G)), R.CoreColor);
		ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.9f, 0.6f);
		UE_LOG(LogZoogi, Log, TEXT("Guard block: P%d %s (%s) took %.0f%% knockback from P%d"), V->PlayerIndex + 1, *V->Def.Name, *ZoogiArtifacts::Name(G),
			R.GuardKnockbackMul * 100.f, Att ? Att->PlayerIndex + 1 : 0);
		EmitAnime(TEXT("GuardBlock"), V, G, V, V->BallLocation(), 1.f, true, Att ? Att->PlayerIndex : -1);
		if (bComboMode) ComboGuardReaction(V, Att); // Guard Reaction extras; the guard is consumed
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
		if (E.bStar && UZoogiBattleStateMachine::AllowStarOrbDraw(this)) // battle flow flag [ZoogiRoll.EpisodeEnergy] bStarOrbDraws
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
		if (!bComboMode && B->Artifact->Guard != EZoogiArtifact::None && B->Artifact->GuardSetTurn < TurnCounter) // combo: guards expire at the end of the team turn
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

	if (bComboMode) ComboResolve(); // relay, Recovery Gate, orb respawn, Resonance / Arena Control, follow-up offer

	// 3) Anime events for the outcome (final now).
	if (Shooter)
	{
		if (IsClimaxMoment(Shooter, KOs)) BeginClimax(Shooter, ShotArtifact, KOs, ShooterScores); // once per match: hit lands after the rumble
		else if (ShooterScores >= 2) EmitAnime(TEXT("Combo"), Shooter, ShotArtifact, Shooter, Shooter->BallLocation(), ShooterScores, false);
		else if (KOs > 0) EmitAnime(TEXT("KO"), Shooter, ShotArtifact, Shooter, Shooter->BallLocation(), KOs, false);
		else if (ShotArtifact != EZoogiArtifact::None && !bInfusedContact && UZoogiBattleStateMachine::AllowMissCard(this)) EmitAnime(TEXT("Miss"), Shooter, ShotArtifact, Shooter, Shooter->BallLocation(), 1.f, false); // battle flow: no consolation card
	}
	UE_LOG(LogZoogi, Log, TEXT("Resolve (turn %d, P%d): %s (%d scoring, %d KO)%s"), TurnCounter, ActivePlayer + 1, *ResolveSummary, ShooterScores, KOs,
		bLastHitCrit ? TEXT(", crit") : TEXT(""));
}

void AZoogiGameMode::CardsPlanCPUAwaken()
{
	// Same decision inputs as the classic CPU (PlanCPUAwaken): a rival target, near the edge, protecting the lead,
	// playing safe, or a stocked artifact hand all lead to Infuse / Guard there, i.e. an Awakening under the
	// InfuseOrGuard rule. In cards mode that becomes the independent Standby request; OnAwakenObserved still applies
	// the configured ActivationRule exactly as for a human. The coin flips use a hash of (turn, player) instead of
	// GameRng so the cards-mode RNG stream (shots, cards) is not shifted by this decision.
	AZoogiBall* B = GetActiveBall();
	if (!B || bAwakenDone || !B->bCPU || bStandbyAwaken) return;
	const uint32 H = FCrc::MemCrc32(&TurnCounter, sizeof(TurnCounter), (uint32)B->PlayerIndex * 2654435761u);
	const float U1 = (H & 0xffff) / 65536.f, U2 = ((H >> 16) & 0xffff) / 65536.f;
	const bool bRival = Cast<AZoogiBall>(CPULock.Get()) != nullptr;
	const float Edge = EdgeDistance(Arena, B->BallLocation());
	bool bLeader = B->Score > 0;
	for (const AZoogiBall* O : Balls) if (O && O != B && O->Score >= B->Score) bLeader = false;
	const bool bStocked = B->Artifact && B->Artifact->Hand.Num() >= 1;
	const TCHAR* Why = bRival ? TEXT("rival target") : Edge < 450.f ? TEXT("near the edge") : (bLeader && U1 < 0.5f) ? TEXT("protecting the lead") : U2 < 0.2f ? TEXT("playing safe") : bStocked ? TEXT("hand stocked") : nullptr;
	if (Why && Stage == EZoogiStage::Awaken)
	{
		bStandbyAwaken = true; UE_LOG(LogZoogi, Log, TEXT("CARDS standby awakening request ON (CPU)"));
		// Choose-your-Awaken: the CPU picks one of the same 3 offered tablets; the tier changes only the decision (no RNG)
		CardsBuildAwakenOffers(B->PlayerIndex);
		auto Find = [&](EZoogiArtifact X) { for (int32 i = 0; i < 3; ++i) if ((EZoogiArtifact)CardAwakenOffers[i] == X) return i; return -1; };
		const EZoogiCPUTier Tier = ZoogiCPU::ActiveTier();
		int32 Pick = 0;
		if (Tier != EZoogiCPUTier::Easy)
		{
			const int32 Aff = Find(ZoogiArtifacts::Affinity(B->Def.Id));
			if (Aff >= 0) Pick = Aff;
			else if (Tier == EZoogiCPUTier::Hard || Tier == EZoogiCPUTier::Expert)
			{
				const EZoogiArtifact Pref[2] = { Edge < 450.f ? EZoogiArtifact::Titan : bRival ? EZoogiArtifact::Ember : EZoogiArtifact::Void, Edge < 450.f ? EZoogiArtifact::Frost : EZoogiArtifact::Storm };
				for (EZoogiArtifact X : Pref) { const int32 k = Find(X); if (k >= 0) { Pick = k; break; } }
			}
		}
		CardsApplyAwakenPick(Pick, Tier == EZoogiCPUTier::Easy ? TEXT("CPU easy") : Tier == EZoogiCPUTier::Normal ? TEXT("CPU normal") : Tier == EZoogiCPUTier::Hard ? TEXT("CPU hard") : TEXT("CPU expert"));
	}
	UE_LOG(LogZoogi, Log, TEXT("CPU P%d Standby Awakening: %s (%s, edge %.0f cm)"), B->PlayerIndex + 1, Why ? TEXT("REQUEST") : TEXT("no"), Why ? Why : TEXT("orb target, saving"), Edge);
}

// =====================================================================================================================
// Choose-your-Awaken (Evan playtest, Oct 15 7:56 AM). After the Standby draw the tray announces CHOOSE YOUR AWAKEN and
// shows 3 stone tablets (the hud-v3 tray tablet art). Design (documented in Saved/cpu_tuning_progress.md):
//  - the options are the classic artifact elements Ember / Storm / Frost / Titan / Void (all five are available to every
//    Zoogi; progression unlocks powers, not elements); each turn offers 3 of them, shuffled by a (turn, player) hash -
//    no GameRng draw, so the shot / card RNG stream is unchanged by the offer;
//  - picking a tablet = that element is Infused for this turn's shot (same artifact effects as the classic INFUSE) and the
//    Standby Awakening request is ON (OnAwakenObserved applies the ActivationRule); SKIP = no element, request OFF;
//  - the energy orb (or A) toggles the tray between the cards and the tablets; 1-3 pick while the tablets show;
//  - CPUs pick by tier when CardsPlanCPUAwaken requests Awakening: Easy = first offer, Normal = affinity element when
//    offered (else first), Hard / Expert = affinity, else situational (near the edge: Titan > Frost, rival target:
//    Ember > Storm, else Void > Storm). Decision only, no RNG.
// No anime / cinematic is emitted for the pick (cinematics unchanged).
#include "ZoogiCPUBrain.h"
#include "ZoogiArtifacts.h"

void AZoogiGameMode::CardsBuildAwakenOffers(int32 Player)
{
	if (CardAwakenOfferTurn == TurnCounter * 8 + Player) return;
	CardAwakenOfferTurn = TurnCounter * 8 + Player;
	EZoogiArtifact All[5] = { EZoogiArtifact::Ember, EZoogiArtifact::Storm, EZoogiArtifact::Frost, EZoogiArtifact::Titan, EZoogiArtifact::Void };
	uint32 H = FCrc::MemCrc32(&TurnCounter, sizeof(TurnCounter), (uint32)(Player + 1) * 0x9E3779B9u ^ 0xA11CE5u);
	for (int32 i = 4; i > 0; --i) { H = H * 1664525u + 1013904223u; const int32 j = (int32)((H >> 8) % (uint32)(i + 1)); Swap(All[i], All[j]); }
	for (int32 i = 0; i < 3; ++i) CardAwakenOffers[i] = (uint8)All[i];
	UE_LOG(LogZoogi, Log, TEXT("CARDS awaken offers P%d turn=%d: %s / %s / %s"), Player + 1, TurnCounter,
		*ZoogiArtifacts::Name(All[0]), *ZoogiArtifacts::Name(All[1]), *ZoogiArtifacts::Name(All[2]));
}

bool AZoogiGameMode::CardsAwakenViewOpen() const
{
	return bCardAwakenView && CardsOn() && Phase == EZoogiPhase::Aiming && Stage == EZoogiStage::Awaken && !bAwakenDone && IsHumanAiming();
}

void AZoogiGameMode::CardsAwakenView(int32 Mode)
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !CardsOn() || Stage != EZoogiStage::Awaken || bAwakenDone || !IsHumanAiming()) { UE_LOG(LogZoogi, Log, TEXT("CARDS awaken view %d ignored (not the human's Standby)"), Mode); return; }
	CardsBuildAwakenOffers(B->PlayerIndex);
	const bool bWas = bCardAwakenView;
	bCardAwakenView = Mode == 1 ? true : Mode == 2 ? false : !bCardAwakenView;
	if (bCardAwakenView && Mode == 1)
	{
		BigText = TEXT("CHOOSE YOUR AWAKEN"); BigColor = FLinearColor(0.75f, 0.5f, 1.f); BigAge = 0.f;
	}
	UE_LOG(LogZoogi, Log, TEXT("CARDS awaken view %s -> %s (pick %d, awaken %s)"), Mode == 1 ? TEXT("auto-open") : Mode == 2 ? TEXT("close") : TEXT("toggle"),
		bCardAwakenView ? TEXT("TABLETS") : TEXT("CARDS"), CardAwakenPick, bStandbyAwaken ? TEXT("ON") : TEXT("off"));
	(void)bWas;
}

bool AZoogiGameMode::CardsApplyAwakenPick(int32 Offer, const TCHAR* Who)
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || Offer < 0 || Offer > 2) return false;
	CardsBuildAwakenOffers(B->PlayerIndex);
	const EZoogiArtifact A = (EZoogiArtifact)CardAwakenOffers[Offer];
	CardAwakenPick = Offer;
	bStandbyAwaken = true;
	B->Artifact->SetInfused(A);
	const FLinearColor C = ZoogiArtifacts::Color(A);
	const bool bAff = ZoogiArtifacts::Affinity(B->Def.Id) == A;
	AddPopup(FString::Printf(TEXT("AWAKEN  %s%s"), *ZoogiArtifacts::Name(A).ToUpper(), bAff ? TEXT("  (AFFINITY)") : TEXT("")), B->BallLocation() + FVector(0, 0, 190.f), C);
	AddFeed(FString::Printf(TEXT("P%d %s  awaken %s%s"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), bAff ? TEXT("  (affinity!)") : TEXT("")), C);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), C, 200.f, 0.6f, false, 10.f, B);
	UE_LOG(LogZoogi, Log, TEXT("CARDS awaken pick P%d (%s): tablet %d = %s%s -> infused, standby awakening ON"), B->PlayerIndex + 1, Who, Offer + 1, *ZoogiArtifacts::Name(A), bAff ? TEXT(" (affinity)") : TEXT(""));
	return true;
}

bool AZoogiGameMode::CardsAwakenPick(int32 Offer)
{
	if (!CardsOn() || Stage != EZoogiStage::Awaken || bAwakenDone || !IsHumanAiming()) return false;
	if (!CardsApplyAwakenPick(Offer, TEXT("human"))) return false;
	bCardAwakenView = false; // back to the cards
	return true;
}

void AZoogiGameMode::CardsAwakenSkip()
{
	AZoogiBall* B = GetActiveBall();
	if (!B || !CardsOn() || Stage != EZoogiStage::Awaken || bAwakenDone || !IsHumanAiming()) return;
	bStandbyAwaken = false; CardAwakenPick = -1; bCardAwakenView = false;
	if (B->Artifact) B->Artifact->ClearInfused();
	AddPopup(TEXT("NO AWAKEN"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.7f, 0.75f, 0.85f));
	UE_LOG(LogZoogi, Log, TEXT("CARDS awaken skip P%d -> standby awakening off"), B->PlayerIndex + 1);
}
