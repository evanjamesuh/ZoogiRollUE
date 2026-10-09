#include "ZoogiMiniGame.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiReactionComponent.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	float SegDist(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float L2 = AB.SizeSquared();
		const float T = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + AB * T);
	}
	bool IsBankSurface(EZoogiContactKind K)
	{
		return K == EZoogiContactKind::Wall || K == EZoogiContactKind::Bumper || K == EZoogiContactKind::Ramp || K == EZoogiContactKind::Piece || K == EZoogiContactKind::Rock;
	}
	const TCHAR* KindName(EZoogiContactKind K)
	{
		switch (K)
		{
		case EZoogiContactKind::Wall: return TEXT("wall");
		case EZoogiContactKind::Bumper: return TEXT("bumper");
		case EZoogiContactKind::Ramp: return TEXT("ramp");
		case EZoogiContactKind::Piece: return TEXT("piece");
		case EZoogiContactKind::Rock: return TEXT("rock");
		case EZoogiContactKind::Floor: return TEXT("floor");
		case EZoogiContactKind::Orb: return TEXT("orb");
		case EZoogiContactKind::Ally: return TEXT("ally");
		default: return TEXT("zoogi");
		}
	}
	/** Shooter's contacts in time order: (time, other body (-1 world), kind). */
	TArray<TTuple<float, int32, EZoogiContactKind>> ShooterContacts(const FZoogiGameplayTruthRecord& T, int32 Shooter)
	{
		TArray<TTuple<float, int32, EZoogiContactKind>> Out;
		for (const FZoogiContactEvent& E : T.Contacts)
		{
			if (E.Body == Shooter) Out.Add(MakeTuple(E.Time, E.OtherBody, E.Kind));
			else if (E.OtherBody == Shooter && E.Body >= 0) Out.Add(MakeTuple(E.Time, E.Body, E.Body >= 100 ? EZoogiContactKind::Orb : EZoogiContactKind::Zoogi));
		}
		Out.StableSort([](const TTuple<float, int32, EZoogiContactKind>& A, const TTuple<float, int32, EZoogiContactKind>& B) { return A.Get<0>() < B.Get<0>(); });
		return Out;
	}
}

UZoogiMiniGame* UZoogiMiniGame::Create(UObject* Outer, EZoogiChallenge K)
{
	switch (K)
	{
	case EZoogiChallenge::HaloBank: return NewObject<UZoogiMiniGameHaloBank>(Outer);
	case EZoogiChallenge::OrbLine: return NewObject<UZoogiMiniGameOrbLine>(Outer);
	case EZoogiChallenge::AirReaction: return NewObject<UZoogiMiniGameAirReaction>(Outer);
	case EZoogiChallenge::RailRelease: return NewObject<UZoogiMiniGameRailRelease>(Outer);
	default: return nullptr;
	}
}

void UZoogiMiniGame::Enter(UZoogiBattleStateMachine* SM, int32 Player, FZoogiPendingSegment& Seg)
{
	Owner = Player;
	bInputOpen = false;
	InputLeft = SM ? SM->GetTuning().MiniGameInputTimeout : 6.f;
}

EZoogiChallengeResult UZoogiMiniGame::TickInput(UZoogiBattleStateMachine* SM, float RealDt)
{
	if (!bInputOpen) return EZoogiChallengeResult::Pending;
	InputLeft -= RealDt;
	return InputLeft <= 0.f ? EZoogiChallengeResult::Timeout : EZoogiChallengeResult::Pending;
}

EZoogiChallengeResult UZoogiMiniGame::Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const
{
	OutDetail = TEXT("no objective");
	return EZoogiChallengeResult::Success;
}

EZoogiChallengeResult UZoogiMiniGame::EvaluateRoute(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail)
{
	switch (Seg.Challenge)
	{
	case EZoogiChallenge::HaloBank: return GetDefault<UZoogiMiniGameHaloBank>()->Evaluate(SM, Seg, T, OutDetail);
	case EZoogiChallenge::OrbLine: return GetDefault<UZoogiMiniGameOrbLine>()->Evaluate(SM, Seg, T, OutDetail);
	case EZoogiChallenge::AirReaction: return GetDefault<UZoogiMiniGameAirReaction>()->Evaluate(SM, Seg, T, OutDetail);
	case EZoogiChallenge::RailRelease: return GetDefault<UZoogiMiniGameRailRelease>()->Evaluate(SM, Seg, T, OutDetail);
	case EZoogiChallenge::SkyBumper:
	{
		// config stub (ref minigame_layout_frozen): reach the Zone 2 aerial layer (airborne or a bumper / ramp kick) before the first body contact
		bool bAerial = false;
		for (const FZoogiAirSegment& A : T.Air) if (A.Body == T.Actor) bAerial = true;
		if (Seg.BumperAt >= 0.f) bAerial = true; // a Sky Bumper prop rebound (aerial props)
		for (const TTuple<float, int32, EZoogiContactKind>& C : ShooterContacts(T, T.Actor))
		{
			if (C.Get<1>() < 0) { if (C.Get<2>() == EZoogiContactKind::Bumper || C.Get<2>() == EZoogiContactKind::Ramp) bAerial = true; continue; }
			OutDetail = FString::Printf(TEXT("zone=%s first_body=%s"), bAerial ? TEXT("aerial") : TEXT("court"), *SM->BodyLabel(C.Get<1>()));
			return bAerial ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Failed;
		}
		OutDetail = FString::Printf(TEXT("zone=%s no body contact"), bAerial ? TEXT("aerial") : TEXT("court"));
		return EZoogiChallengeResult::Failed;
	}
	case EZoogiChallenge::PartnerIntercept:
	{
		// config stub (2v2): the partner is the first body touched, then any rival is touched
		const AZoogiGameMode* G = SM->GetGame();
		bool bPartnerFirst = false, bRival = false;
		int32 N = 0;
		for (const TTuple<float, int32, EZoogiContactKind>& C : ShooterContacts(T, T.Actor))
		{
			const int32 O = C.Get<1>();
			if (O < 0 || O >= 100) continue;
			const bool bAlly = G && G->bComboMode && AZoogiGameMode::TeamOfIndex(O) == AZoogiGameMode::TeamOfIndex(T.Actor);
			if (N++ == 0) bPartnerFirst = bAlly;
			else if (!bAlly) bRival = true;
		}
		OutDetail = FString::Printf(TEXT("partner_first=%d rival_after=%d"), bPartnerFirst ? 1 : 0, bRival ? 1 : 0);
		return bPartnerFirst && bRival ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Failed;
	}
	default: OutDetail = TEXT("unknown challenge"); return EZoogiChallengeResult::Failed;
	}
}

// ---------------------------------------------------------------------------------------------------- Halo Bank

EZoogiChallengeResult UZoogiMiniGameHaloBank::Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const
{
	if (Seg.DeclaredTarget < 0) { OutDetail = TEXT("no declared target"); return EZoogiChallengeResult::Failed; }
	int32 Banks = 0;
	FString BankOn = TEXT("-");
	for (const TTuple<float, int32, EZoogiContactKind>& C : ShooterContacts(T, T.Actor))
	{
		const int32 Other = C.Get<1>();
		if (Other < 0)
		{
			if (IsBankSurface(C.Get<2>())) { if (Banks++ == 0) BankOn = KindName(C.Get<2>()); }
			continue;
		}
		if (Seg.HaloAt >= 0.f && Seg.HaloAt <= C.Get<0>() + 0.02f) { Banks++; BankOn = FString::Printf(TEXT("halo_%s"), *Seg.HaloEvent); } // Resonance Halo pass / rim (aerial props)
		OutDetail = FString::Printf(TEXT("banks=%d first=%s halo=%s first_body=%s target=%s"), Banks, *BankOn, Seg.HaloEvent.IsEmpty() ? TEXT("-") : *Seg.HaloEvent, *SM->BodyLabel(Other), *SM->BodyLabel(Seg.DeclaredTarget));
		return (Banks >= 1 && Other == Seg.DeclaredTarget) ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Failed;
	}
	OutDetail = FString::Printf(TEXT("banks=%d first=%s no body contact"), Banks, *BankOn);
	return EZoogiChallengeResult::Failed;
}

// ---------------------------------------------------------------------------------------------------- Orb Line

void UZoogiMiniGame::CommitOrbLine(const UZoogiBattleStateMachine* SM, FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T)
{
	const AZoogiGameMode* G = SM ? SM->GetGame() : nullptr;
	if (!G || Seg.CommittedOrbs.Num() > 0) return;
	TArray<FVector2D> Path;
	if (G->Preview.bValid && G->Preview.Path.Num() >= 2) for (const FVector& P : G->Preview.Path) Path.Add(FVector2D(P));
	else
	{
		const FVector2D V(T.LaunchVelocity);
		const float Decel = G->Arena ? G->Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
		const float Dist = V.SizeSquared() / FMath::Max(1.f, 2.f * Decel);
		Path = { FVector2D(T.LaunchStart), FVector2D(T.LaunchStart) + V.GetSafeNormal() * Dist };
	}
	// a preview path ends at the first contact (the shooter's centre one contact distance short of the body): extend it
	if (Path.Num() >= 2 && G->Preview.bValid && G->Preview.bContact)
	{
		const FVector2D D = (Path.Last() - Path[Path.Num() - 2]).GetSafeNormal();
		Path.Add(Path.Last() + D * (ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 10.f));
	}
	const float Corridor = ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius - 8.f; // centre line passes through the orb
	TArray<TPair<float, int32>> Hits; // distance along the path -> orb
	for (int32 i = 0; i < G->Orbs.Num(); ++i)
	{
		const AZoogiOrb* O = G->Orbs[i];
		if (!O || O->bOut) continue;
		const FVector2D OP(O->OrbLocation());
		float Along = 0.f;
		for (int32 k = 1; k < Path.Num(); ++k)
		{
			if (SegDist(OP, Path[k - 1], Path[k]) <= Corridor)
			{
				Hits.Add({ Along + FVector2D::Distance(Path[k - 1], OP), 100 + i });
				break;
			}
			Along += FVector2D::Distance(Path[k - 1], Path[k]);
		}
	}
	// the aim preview's first contact, when it is an orb, is always on the route
	if (G->Preview.bValid && G->Preview.bContact)
		for (int32 i = 0; i < G->Orbs.Num(); ++i)
			if (G->Orbs[i] && (const AActor*)G->Orbs[i].Get() == G->Preview.ContactActor.Get() && !Hits.ContainsByPredicate([i](const TPair<float, int32>& H) { return H.Value == 100 + i; }))
				Hits.Add({ 0.f, 100 + i });
	Hits.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 i = 0; i < Hits.Num() && i < 3; ++i) Seg.CommittedOrbs.Add(Hits[i].Value);
	FString L;
	for (int32 O : Seg.CommittedOrbs) L += FString::Printf(TEXT("%s%s"), L.IsEmpty() ? TEXT("") : TEXT(","), *SM->BodyLabel(O));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame OrbLine enter P%d committed=[%s] path_pts=%d corridor=%.0f"), Seg.Owner + 1, L.IsEmpty() ? TEXT("-") : *L, Path.Num(), Corridor);
}

void UZoogiMiniGameOrbLine::Enter(UZoogiBattleStateMachine* SM, int32 Player, FZoogiPendingSegment& Seg)
{
	Super::Enter(SM, Player, Seg);
	if (const UZoogiEpisodeSubsystem* Ep = SM ? SM->GetEpisode() : nullptr) CommitOrbLine(SM, Seg, Ep->GetTruth());
}

EZoogiChallengeResult UZoogiMiniGameOrbLine::Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const
{
	if (Seg.CommittedOrbs.Num() == 0) { OutDetail = TEXT("no orbs on the route"); return EZoogiChallengeResult::Failed; }
	TSet<int32> Touched;
	for (const TTuple<float, int32, EZoogiContactKind>& C : ShooterContacts(T, T.Actor)) if (C.Get<1>() >= 100) Touched.Add(C.Get<1>());
	int32 Got = 0;
	FString L;
	for (int32 O : Seg.CommittedOrbs)
	{
		const bool b = Touched.Contains(O);
		Got += b ? 1 : 0;
		L += FString::Printf(TEXT("%s%s:%d"), L.IsEmpty() ? TEXT("") : TEXT(","), *SM->BodyLabel(O), b ? 1 : 0);
	}
	OutDetail = FString::Printf(TEXT("orbs %d/%d [%s]"), Got, Seg.CommittedOrbs.Num(), *L);
	return Got == Seg.CommittedOrbs.Num() ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Failed;
}

// ---------------------------------------------------------------------------------------------------- Air Reaction

bool UZoogiMiniGameAirReaction::CanEnter(const UZoogiBattleStateMachine* SM, int32 Player) const
{
	const UZoogiReactionComponent* R = SM ? SM->Reaction(Player) : nullptr;
	return R && R->CanReact(EZoogiReactionKind::Air);
}

void UZoogiMiniGameAirReaction::Enter(UZoogiBattleStateMachine* SM, int32 Player, FZoogiPendingSegment& Seg)
{
	Super::Enter(SM, Player, Seg);
	Body = Player;
	const AZoogiGameMode* G = SM->GetGame();
	bCPU = G && G->Balls.IsValidIndex(Player) && G->Balls[Player] && G->Balls[Player]->bCPU;
	CPUDelay = SM->GetTuning().AirReactionHoldCPU;
	ReturnTo = EZoogiBattleState::PhysicsResolve;
	if (UZoogiReactionComponent* R = SM->Reaction(Player)) R->Consume(EZoogiReactionKind::Air);
}

bool UZoogiMiniGameAirReaction::SubmitInput(UZoogiBattleStateMachine* SM, int32 InChoice, const FString& Source)
{
	if (!bInputOpen || Choice != EZoogiAirChoice::None) return false;
	Choice = (EZoogiAirChoice)InChoice;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame AirReaction input P%d choice=%s src=%s left=%.2fs"), Owner + 1, ZoogiBattle::AirChoiceName(Choice), *Source, InputLeft);
	return true;
}

EZoogiChallengeResult UZoogiMiniGameAirReaction::TickInput(UZoogiBattleStateMachine* SM, float RealDt)
{
	if (!bInputOpen) return EZoogiChallengeResult::Pending;
	if (Choice != EZoogiAirChoice::None) return EZoogiChallengeResult::Success; // input taken (the landing decides the result)
	if (bCPU)
	{
		CPUDelay -= RealDt;
		if (CPUDelay <= 0.f) SubmitInput(SM, (int32)DecideCPU(SM), TEXT("cpu"));
		return Choice != EZoogiAirChoice::None ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Pending;
	}
	return Super::TickInput(SM, RealDt);
}

EZoogiAirChoice UZoogiMiniGameAirReaction::DecideCPU(const UZoogiBattleStateMachine* SM) const
{
	const AZoogiGameMode* G = SM->GetGame();
	const AZoogiBall* B = G && G->Balls.IsValidIndex(Body) ? G->Balls[Body].Get() : nullptr;
	if (!B) return EZoogiAirChoice::Brace;
	const FVector2D P(B->BallLocation()), V(B->GetBallVelocity());
	// heading outward (away from the centre): brace; otherwise steer toward the centre
	return FVector2D::DotProduct(P.GetSafeNormal(), V.GetSafeNormal()) > 0.3f ? EZoogiAirChoice::Brace : EZoogiAirChoice::Redirect;
}

FString UZoogiMiniGameAirReaction::Apply(UZoogiBattleStateMachine* SM) const
{
	const AZoogiGameMode* G = SM->GetGame();
	AZoogiBall* B = G && G->Balls.IsValidIndex(Body) ? G->Balls[Body].Get() : nullptr;
	if (!B || !B->Sphere) return TEXT("no body");
	const FVector V = B->Sphere->GetPhysicsLinearVelocity();
	FVector2D H(V.X, V.Y);
	const FVector2D Before = H;
	const FZoogiBattleTuning& T = SM->GetTuning();
	if (Choice == EZoogiAirChoice::Brace) H *= T.BraceDamp;
	else if (Choice == EZoogiAirChoice::Redirect)
	{
		const FVector2D ToC = (-FVector2D(B->BallLocation())).GetSafeNormal();
		const float Cur = FMath::Atan2(H.Y, H.X), Want = FMath::Atan2(ToC.Y, ToC.X);
		const float Delta = FMath::Clamp(FMath::FindDeltaAngleRadians(Cur, Want), -FMath::DegreesToRadians(T.RedirectDeg), FMath::DegreesToRadians(T.RedirectDeg));
		H = H.GetRotated(FMath::RadiansToDegrees(Delta));
	}
	else return TEXT("none");
	B->Sphere->SetPhysicsLinearVelocity(FVector(H.X, H.Y, V.Z));
	return FString::Printf(TEXT("%s v(%.0f,%.0f)->(%.0f,%.0f) at (%.0f,%.0f,%.0f)"), ZoogiBattle::AirChoiceName(Choice), Before.X, Before.Y, H.X, H.Y, B->BallLocation().X, B->BallLocation().Y, B->BallLocation().Z);
}

EZoogiChallengeResult UZoogiMiniGameAirReaction::Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const
{
	const bool bOut = T.RingOuts.Contains(Seg.Owner);
	OutDetail = FString::Printf(TEXT("choice=%s landed_in=%d"), ZoogiBattle::AirChoiceName(Seg.AirChoice), bOut ? 0 : 1);
	return bOut ? EZoogiChallengeResult::Failed : EZoogiChallengeResult::Success;
}

// =====================================================================================================================
// state machine side of the mini-games (MINIGAME state runner, Air Reaction offer / hold / apply)

bool UZoogiBattleStateMachine::IsAirReactionOpen() const
{
	const UZoogiMiniGameAirReaction* M = Cast<UZoogiMiniGameAirReaction>(ActiveMiniGame);
	return M && M->bInputOpen && M->Choice == EZoogiAirChoice::None;
}

bool UZoogiBattleStateMachine::SubmitAirChoice(EZoogiAirChoice Choice, const FString& Source)
{
	UZoogiMiniGameAirReaction* M = Cast<UZoogiMiniGameAirReaction>(ActiveMiniGame);
	if (!M || Choice == EZoogiAirChoice::None) return false;
	return M->SubmitInput(this, (int32)Choice, Source);
}

bool UZoogiBattleStateMachine::CanOfferAir(int32 Player, FString* OutWhy) const
{
	FString Why;
	bool bOk = bFlow;
	if (!bFlow) Why = TEXT("flow_off");
	const UZoogiReactionComponent* R = Reaction(Player);
	if (bOk && !(R && R->bAwakened)) { bOk = false; Why = TEXT("not_awakened"); }
	if (bOk && R->ReactionsLeft <= 0) { bOk = false; Why = TEXT("no_reactions_left"); }
	if (bOk && Player == CurrentActor && !Tune.bAirReactionOwnTurn) { bOk = false; Why = TEXT("own_turn"); }
	if (bOk && (ActiveMiniGame != nullptr || AirSegment.Owner >= 0)) { bOk = false; Why = TEXT("busy"); }
	if (bOk) Why = TEXT("awakened");
	if (OutWhy) *OutWhy = Why;
	return bOk;
}

void UZoogiBattleStateMachine::HandleAirborne(int32 Body, float Peak)
{
	AZoogiGameMode* G = GM();
	if (!G || Body < 0 || Body >= 100 || !G->Balls.IsValidIndex(Body)) return;
	FString Why;
	const bool bAllowed = CanOfferAir(Body, &Why);
	++AirOffersSeen;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE awakening P%d react=air allowed=%d why=%s peak=%.0f action=%d"), Body + 1, bAllowed ? 1 : 0, *Why, Peak, CurrentActionId);
	if (!bAllowed) return;
	if (G->Balls[Body] && G->Balls[Body]->bCPU && CPUPlan.IsFor(Body, G->TurnCounter) && CPUPlan.Kind == EZoogiPropKind::LaunchRail && LastRailWindow == EZoogiRailWindow::Early)
	{
		// Oct 8: a CPU threading the hanging halo off an EARLY rail release keeps its arc (no reaction this flight)
		UE_LOG(LogZoogi, Log, TEXT("BATTLE awakening P%d react=air declined (cpu rail EARLY route)"), Body + 1);
		return;
	}
	++AirOffersAllowed;
	AirOfferedTo.Add(Body);
	if (UZoogiEpisodeSubsystem* Ep = Episode()) Ep->ClaimAirborne();
	AirSegment = FZoogiPendingSegment();
	AirSegment.SegmentId = FString::Printf(TEXT("seg_%03d_p%d_air_reaction"), NextSegment++, Body + 1);
	AirSegment.ComboId = TEXT("air_reaction");
	AirSegment.ClipId = FName(*FString::Printf(TEXT("clip_%s_air_reaction"), Players.IsValidIndex(Body) ? *Players[Body].Name.ToLower() : TEXT("zoogi")));
	AirSegment.Owner = Body;
	AirSegment.Challenge = EZoogiChallenge::AirReaction;
	AirSegment.Result = EZoogiChallengeResult::Pending;
	AirSegment.Comic = EZoogiComicState::Pending;
	AirSegment.ActionId = CurrentActionId;
	UZoogiMiniGame* M = UZoogiMiniGame::Create(this, EZoogiChallenge::AirReaction);
	ActiveMiniGame = M;
	M->Enter(this, Body, AirSegment);
	AirPendingBody = Body;
	AirPendingPeak = Peak;
	AirPendingWait = 0.f;
	AirHoldToken = 0;
	Transition(EZoogiBattleState::MiniGame, FString::Printf(TEXT("air_reaction P%d"), Body + 1));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame AirReaction enter P%d cpu=%d timeout=%.1fs segment=%s"), Body + 1, Cast<UZoogiMiniGameAirReaction>(M)->bCPU ? 1 : 0, M->InputLeft, *AirSegment.SegmentId);
}

void UZoogiBattleStateMachine::TickMiniGame(float RealDt)
{
	if (UZoogiMiniGameRailRelease* RM = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame)) { TickRail(RM); return; } // game time (ZoogiAerialPlay.cpp)
	UZoogiMiniGameAirReaction* M = Cast<UZoogiMiniGameAirReaction>(ActiveMiniGame);
	if (!M) return;
	UWorld* W = GetWorld();
	UZoogiEpisodeSubsystem* Ep = Episode();
	if (!W || !Ep) { FinishMiniGame(EZoogiChallengeResult::Cancelled, TEXT("no_episode")); return; }
	if (AirHoldToken == 0)
	{
		// wait until the game's own hit-stop / slow-mo is over, then hold the real world (gameplay hold, any mode)
		const AWorldSettings* WS = W->GetWorldSettings();
		const bool bClean = WS && FMath::IsNearlyEqual(WS->TimeDilation, 1.f) && FMath::IsNearlyEqual(WS->CinematicTimeDilation, 1.f) && !W->IsPaused() && !Ep->IsHeld();
		AirPendingWait += RealDt;
		if (bClean)
		{
			const float Max = (M->bCPU ? Tune.AirReactionHoldCPU : Tune.MiniGameInputTimeout) + 1.5f;
			AirHoldToken = Ep->BeginHold(Max, TEXT("AirReaction"), true);
			if (AirHoldToken == 0) { FinishMiniGame(EZoogiChallengeResult::Cancelled, TEXT("hold_refused")); return; }
			M->bInputOpen = true;
			UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame AirReaction hold P%d token=%d waited=%.2fs input=%s"), M->Owner + 1, AirHoldToken, AirPendingWait, M->bCPU ? TEXT("cpu") : TEXT("human"));
		}
		else if (AirPendingWait > Tune.AirReactionMaxWait) FinishMiniGame(EZoogiChallengeResult::Cancelled, TEXT("no_clean_moment"));
		return;
	}
	if (!Ep->IsHeld()) { FinishMiniGame(M->Choice != EZoogiAirChoice::None ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Timeout, TEXT("hold_ended")); return; }
	const EZoogiChallengeResult R = M->TickInput(this, RealDt);
	if (R != EZoogiChallengeResult::Pending) FinishMiniGame(R, R == EZoogiChallengeResult::Timeout ? TEXT("input_timeout") : TEXT("input"));
}

void UZoogiBattleStateMachine::FinishMiniGame(EZoogiChallengeResult R, const FString& Detail)
{
	if (UZoogiMiniGameRailRelease* RM = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame)) { FinishRail(RM, R, Detail); return; }
	UZoogiMiniGame* Base = ActiveMiniGame;
	ActiveMiniGame = nullptr;
	UZoogiMiniGameAirReaction* M = Cast<UZoogiMiniGameAirReaction>(Base);
	if (!M) return;
	if (AirHoldToken != 0) { if (UZoogiEpisodeSubsystem* Ep = Episode()) Ep->ReleaseHold(AirHoldToken); }
	AirHoldToken = 0;
	FString Applied = TEXT("-");
	if (R == EZoogiChallengeResult::Success && M->Choice != EZoogiAirChoice::None)
	{
		Applied = M->Apply(this); // first frame after the release: the next physics step uses it
		AirSegment.AirChoice = M->Choice;
		AirSegment.Detail = Applied;
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame AirReaction result=%s P%d choice=%s applied=\"%s\" why=%s"), ZoogiBattle::ResultName(R), M->Owner + 1, ZoogiBattle::AirChoiceName(M->Choice), *Applied, *Detail);
	const bool bInMini = State == EZoogiBattleState::MiniGame;
	if (R == EZoogiChallengeResult::Cancelled)
	{
		if (UZoogiReactionComponent* RC = Reaction(M->Owner)) { RC->ReactionsLeft++; RC->ReactionsUsed--; } // never offered to the player
		AirSegment = FZoogiPendingSegment();
		if (bInMini) Transition(M->ReturnState(), TEXT("air_reaction_cancelled"));
		return;
	}
	if (Players.IsValidIndex(M->Owner)) Players[M->Owner].Reactions++;
	LedgerSimple(TEXT("REACTION"), M->Owner, FString::Printf(TEXT("AIR_REACTION:%s %s"), ZoogiBattle::AirChoiceName(M->Choice), ZoogiBattle::ResultName(R)));
	if (R == EZoogiChallengeResult::Timeout || R == EZoogiChallengeResult::Failed)
	{
		AirSegment.Result = R;
		FZoogiPendingSegment Seg = AirSegment;
		AirSegment = FZoogiPendingSegment();
		RecordShatter(Seg, FString::Printf(TEXT("AirReaction_%s"), ZoogiBattle::ResultName(R)));
		if (bInMini)
		{
			Transition(EZoogiBattleState::ShatterFailure, TEXT("air_reaction_failed"));
			Transition(EZoogiBattleState::PhysicsResolve, TEXT("ordinary_resolution"));
		}
		return;
	}
	if (bInMini) Transition(M->ReturnState(), TEXT("air_reaction_applied"));
}
