#include "ZoogiBattleStateMachine.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiReactionComponent.h"
#include "ZoogiMiniGame.h"
#include "ZoogiEpisodeLedger.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiRoll.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// =====================================================================================================================
// lifecycle

bool UZoogiBattleStateMachine::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UZoogiBattleStateMachine::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UZoogiEpisodeSubsystem* Ep = Collection.InitializeDependency<UZoogiEpisodeSubsystem>();
	Tune.Load();
	bFlow = Tune.bEnabled;
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Value(Cmd, TEXT("ZoogiBattleTest="), TestCase)) { TestCase = TestCase.ToLower(); bFlow = true; }
	FString V;
	if (FParse::Value(Cmd, TEXT("ZoogiBattle="), V)) bFlow = V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V == TEXT("1") || V.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	float F = 0.f;
	if (FParse::Value(Cmd, TEXT("ZoogiBattleFollowUpTimeout="), F)) Tune.FollowUpOfferTimeout = F;
	if (FParse::Value(Cmd, TEXT("ZoogiBattleInputTimeout="), F)) Tune.MiniGameInputTimeout = F;
	if (FParse::Value(Cmd, TEXT("ZoogiBattleFollowUpCost="), V)) Tune.FollowUpCostSource = V;
	if (FParse::Value(Cmd, TEXT("ZoogiBattleAwaken="), V)) Tune.ActivationRule = V;
	if (FParse::Param(Cmd, TEXT("ZoogiBattleNoOverlay"))) Tune.bDebugOverlay = false;
	FParse::Value(Cmd, TEXT("ZoogiAerialShots="), AerialShotDir); // aerial layer / HUD prompt captures (engine viewport)
	hHUD = AHUD::OnHUDPostRender.AddUObject(this, &UZoogiBattleStateMachine::OnHUDPostRender); // battle HUD (energy / prompts)
	if (Ep)
	{
		hStart = Ep->OnActionStarted.AddUObject(this, &UZoogiBattleStateMachine::HandleActionStarted);
		hResolving = Ep->OnActionResolving.AddUObject(this, &UZoogiBattleStateMachine::HandleActionResolving);
		hFinal = Ep->OnActionFinalized.AddUObject(this, &UZoogiBattleStateMachine::HandleActionFinalized);
		hAir = Ep->OnAirborneLive.AddUObject(this, &UZoogiBattleStateMachine::HandleAirborne);
	}
	Ledger = NewObject<UZoogiEpisodeLedger>(this);
	Assembler = NewObject<UZoogiEpisodeAssembler>(this);
	MatchId = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE engine: flow=%s test=%s gains=%s combos=%s(%d) follow_up_cost=%s awaken_rule=%s miss_card=%d follow_up_timeout=%.1f input_timeout=%.1f"),
		bFlow ? TEXT("ON") : TEXT("OFF"), TestCase.IsEmpty() ? TEXT("-") : *TestCase, *Tune.GainsSource, *Tune.CombosSource, Tune.Combos.Num(), *Tune.FollowUpCostSource,
		*Tune.ActivationRule, Tune.bMissConsolationCard ? 1 : 0, Tune.FollowUpOfferTimeout, Tune.MiniGameInputTimeout);
}

void UZoogiBattleStateMachine::Deinitialize()
{
	if (UZoogiEpisodeSubsystem* Ep = Episode())
	{
		Ep->OnActionStarted.Remove(hStart);
		Ep->OnActionResolving.Remove(hResolving);
		Ep->OnActionFinalized.Remove(hFinal);
		Ep->OnAirborneLive.Remove(hAir);
	}
	AHUD::OnHUDPostRender.Remove(hHUD);
	Super::Deinitialize();
}

TStatId UZoogiBattleStateMachine::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiBattleStateMachine, STATGROUP_Tickables);
}

UZoogiBattleStateMachine* UZoogiBattleStateMachine::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UZoogiBattleStateMachine>() : nullptr;
}

bool UZoogiBattleStateMachine::FlowOn(const UObject* Ctx)
{
	const UZoogiBattleStateMachine* S = Get(Ctx);
	return S && S->bFlow;
}

AZoogiGameMode* UZoogiBattleStateMachine::GM() const
{
	const UWorld* W = GetWorld();
	return W ? Cast<AZoogiGameMode>(W->GetAuthGameMode()) : nullptr;
}

UZoogiEpisodeSubsystem* UZoogiBattleStateMachine::Episode() const
{
	const UWorld* W = GetWorld();
	return W ? W->GetSubsystem<UZoogiEpisodeSubsystem>() : nullptr;
}

// =====================================================================================================================
// hooks for the game (flow off: the old behaviour exactly)

bool UZoogiBattleStateMachine::AllowMissCard(const UObject* Ctx)
{
	UZoogiBattleStateMachine* S = Get(Ctx);
	if (!S || !S->bFlow || S->Tune.bMissConsolationCard) return true;
	++S->SuppressedMissCards;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE miss card suppressed (no consolation scene; bMissConsolationCard=0)"));
	return false;
}

bool UZoogiBattleStateMachine::AllowStarOrbDraw(const UObject* Ctx)
{
	const UZoogiBattleStateMachine* S = Get(Ctx);
	return !S || !S->bFlow || S->Tune.bStarOrbDraws;
}

bool UZoogiBattleStateMachine::AllowResonanceDraws(const UObject* Ctx)
{
	const UZoogiBattleStateMachine* S = Get(Ctx);
	return !S || !S->bFlow || S->Tune.bResonanceSurgeDraws;
}

bool UZoogiBattleStateMachine::AllowArenaControlGauge(const UObject* Ctx)
{
	const UZoogiBattleStateMachine* S = Get(Ctx);
	return !S || !S->bFlow || S->Tune.bArenaControlGauge;
}

bool UZoogiBattleStateMachine::CanAffordFollowUp(const UObject* Ctx, int32 Partner, float TeamGauge, float Cost)
{
	const UZoogiBattleStateMachine* S = Get(Ctx);
	if (!S || !S->bFlow || !S->Tune.FollowUpUsesEnergy()) return TeamGauge >= Cost;
	return S->GetEnergy(Partner) + KINDA_SMALL_NUMBER >= Cost;
}

bool UZoogiBattleStateMachine::PayFollowUpFromEnergy(const UObject* Ctx, int32 Partner, float Cost)
{
	UZoogiBattleStateMachine* S = Get(Ctx);
	if (!S || !S->bFlow || !S->Tune.FollowUpUsesEnergy()) return false;
	return S->SpendEnergy(Partner, Cost, TEXT("partner_follow_up"));
}

bool UZoogiBattleStateMachine::FollowUpOfferTimedOut(const UObject* Ctx, float OfferAge)
{
	UZoogiBattleStateMachine* S = Get(Ctx);
	if (!S || !S->bFlow || S->Tune.FollowUpOfferTimeout <= 0.f || OfferAge < S->Tune.FollowUpOfferTimeout) return false;
	++S->FollowUpTimeouts;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE follow_up offer timeout after %.1fs (limit %.1fs): auto-decline"), OfferAge, S->Tune.FollowUpOfferTimeout);
	return true;
}

// =====================================================================================================================
// state

bool UZoogiBattleStateMachine::IsValidTransition(EZoogiBattleState From, EZoogiBattleState To)
{
	using S = EZoogiBattleState;
	if (To == S::None || To == S::MatchEnd) return From != To; // reset / match over from anywhere
	switch (From)
	{
	case S::None: return To == S::WaitForCommand;
	case S::WaitForCommand: return To == S::ActionPreview || To == S::PhysicsResolve || To == S::MiniGame || To == S::NextTurn;
	case S::ActionPreview: return To == S::PhysicsResolve || To == S::WaitForCommand || To == S::MiniGame || To == S::NextTurn;
	case S::MiniGame: return To == S::PhysicsResolve || To == S::ActionPreview || To == S::ShatterFailure || To == S::Result;
	case S::PhysicsResolve: return To == S::MiniGame || To == S::Result || To == S::ShatterFailure;
	case S::ShatterFailure: return To == S::NextTurn || To == S::PhysicsResolve; // Air Reaction fail: ordinary resolution continues
	case S::Result: return To == S::EpisodeEvent || To == S::ShatterFailure;
	case S::EpisodeEvent: return To == S::NextTurn;
	case S::NextTurn: return To == S::WaitForCommand;
	case S::MatchEnd: return false;
	default: return false;
	}
}

bool UZoogiBattleStateMachine::Transition(EZoogiBattleState To, const FString& Reason)
{
	if (To == State) return true;
	const bool bValid = IsValidTransition(State, To);
	const AZoogiGameMode* G = GM();
	UE_LOG(LogZoogi, Log, TEXT("BATTLE state %s -> %s reason=%s P%d turn=%d%s"), ZoogiBattle::StateName(State), ZoogiBattle::StateName(To), *Reason,
		G ? G->ActivePlayer + 1 : 0, G ? G->TurnCounter : 0, bValid ? TEXT("") : TEXT(" INVALID"));
	if (!bValid) ++InvalidTransitions;
	if (StateTrail.Num() < 4000) StateTrail.Add(FString::Printf(TEXT("%s>%s:%s"), ZoogiBattle::StateName(State), ZoogiBattle::StateName(To), *Reason));
	State = To;
	StateReal = 0.f;
	return bValid;
}

void UZoogiBattleStateMachine::EnsurePlayers()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	if (Players.Num() != G->Balls.Num())
	{
		Players.SetNum(G->Balls.Num());
	}
	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		AZoogiBall* B = G->Balls[i];
		FZoogiBattlePlayerState& P = Players[i];
		P.Player = i;
		if (!B) continue;
		P.Name = B->Def.Name;
		P.Team = G->bComboMode ? AZoogiGameMode::TeamOfIndex(i) : i;
		if (!B->FindComponentByClass<UZoogiEpisodeEnergyComponent>())
		{
			UZoogiEpisodeEnergyComponent* E = NewObject<UZoogiEpisodeEnergyComponent>(B, TEXT("EpisodeEnergy"));
			E->Player = i;
			E->Cap = Tune.EnergyCap;
			E->RegisterComponent();
			float Start = 0.f;
			if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiBattleEnergy="), Start) && Start > 0.f) E->SetValue(Start, TEXT("test_start"));
		}
		if (!B->FindComponentByClass<UZoogiReactionComponent>())
		{
			UZoogiReactionComponent* R = NewObject<UZoogiReactionComponent>(B, TEXT("Reaction"));
			R->Player = i;
			R->RegisterComponent();
		}
	}
}

UZoogiEpisodeEnergyComponent* UZoogiBattleStateMachine::Energy(int32 Player) const
{
	const AZoogiGameMode* G = GM();
	if (!G || !G->Balls.IsValidIndex(Player) || !G->Balls[Player]) return nullptr;
	return G->Balls[Player]->FindComponentByClass<UZoogiEpisodeEnergyComponent>();
}

UZoogiReactionComponent* UZoogiBattleStateMachine::Reaction(int32 Player) const
{
	const AZoogiGameMode* G = GM();
	if (!G || !G->Balls.IsValidIndex(Player) || !G->Balls[Player]) return nullptr;
	return G->Balls[Player]->FindComponentByClass<UZoogiReactionComponent>();
}

float UZoogiBattleStateMachine::GetEnergy(int32 Player) const
{
	const UZoogiEpisodeEnergyComponent* E = Energy(Player);
	return E ? E->Value : 0.f;
}

void UZoogiBattleStateMachine::AddEnergy(int32 Player, float Amount, FName Source, int32 ActionId)
{
	UZoogiEpisodeEnergyComponent* E = Energy(Player);
	if (!E || Amount <= 0.f) return;
	const float Added = E->Gain(Amount, Source, ActionId);
	if (Players.IsValidIndex(Player)) Players[Player].EnergyGained += Added;
	ActionEnergyDelta.FindOrAdd(Player) += Added;
}

bool UZoogiBattleStateMachine::SpendEnergy(int32 Player, float Amount, FName Why)
{
	UZoogiEpisodeEnergyComponent* E = Energy(Player);
	if (!E || !E->Spend(Amount, Why)) return false;
	if (Players.IsValidIndex(Player)) Players[Player].EnergySpent += Amount;
	ActionEnergyDelta.FindOrAdd(Player) -= Amount;
	return true;
}

bool UZoogiBattleStateMachine::IsAwakened(int32 Player) const
{
	const UZoogiReactionComponent* R = Reaction(Player);
	return R && R->bAwakened;
}

int32 UZoogiBattleStateMachine::BodyCodeOf(const AActor* A) const
{
	const AZoogiGameMode* G = GM();
	if (!G || !A) return -1;
	for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i] == A) return i;
	for (int32 i = 0; i < G->Orbs.Num(); ++i) if (G->Orbs[i] == A) return 100 + i;
	return -1;
}

FString UZoogiBattleStateMachine::BodyLabel(int32 Code) const
{
	if (Code < 0) return TEXT("-");
	return Code >= 100 ? FString::Printf(TEXT("O%d"), Code - 99) : FString::Printf(TEXT("P%d"), Code + 1);
}

// =====================================================================================================================
// tick: observe the game's own stage machine and mirror it into the battle states

void UZoogiBattleStateMachine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W || W->bIsTearingDown) return;
	AZoogiGameMode* G = GM();
	if (!G) return;
	const float RealDt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.1f);
	RealClock += RealDt;
	StateReal += RealDt;
	if (G->Phase != EZoogiPhase::Menu) EnsurePlayers();
	Observe();
	TickPeaks(); // Oct 8: peak-height log per action (observation only)
	if (G->Phase == EZoogiPhase::GameOver && PeakTracks.Num() > 0) FlushPeaks(TEXT("match_end"));
	if (Props.Num() > 0 || (bFlow && Tune.bAerialProps)) TickProps();
	if (bFlow)
	{
		TickMiniGame(RealDt);
		DebugInput();
		if (Tune.bDebugOverlay) DebugOverlay();
	}
	if (AerialLater.Num() > 0) TickAerialShots();
	if (!TestCase.IsEmpty()) TickTest(RealDt);
}

void UZoogiBattleStateMachine::Observe()
{
	AZoogiGameMode* G = GM();
	using S = EZoogiBattleState;
	if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming)
	{
		if (State != S::None && State != S::MatchEnd) Transition(S::None, TEXT("menu"));
		if (G->Phase == EZoogiPhase::Menu) { LastTurnKey = INT32_MIN; bMatchEnded = false; }
		return;
	}
	if (G->Phase == EZoogiPhase::GameOver)
	{
		if (State != S::MatchEnd)
		{
			if (State == S::Result) Transition(S::EpisodeEvent, TEXT("final_result"));
			Transition(S::MatchEnd, TEXT("game_over"));
		}
		if (!bMatchEnded) { bMatchEnded = true; OnMatchEnd(TEXT("game_over")); }
		return;
	}
	if (State == S::MatchEnd) return;

	// turn hand-off (BeginTurn, or a 2v2 Partner Follow-Up: same TurnCounter, other ActivePlayer)
	const int32 Key = G->TurnCounter * 8 + G->ActivePlayer;
	if (Key != LastTurnKey)
	{
		const bool bFirst = LastTurnKey == INT32_MIN;
		const bool bFollowUp = !bFirst && G->bComboMode && G->bInFollowUp && (LastTurnKey / 8) == G->TurnCounter;
		LastTurnKey = Key;
		if (!bFirst)
		{
			if (State == S::Result) Transition(S::EpisodeEvent, TEXT("action_recorded"));
			if (State == S::PhysicsResolve || State == S::MiniGame) Transition(S::Result, TEXT("late_resolve"));
			if (State == S::Result) Transition(S::EpisodeEvent, TEXT("action_recorded"));
			if (State != S::NextTurn) Transition(S::NextTurn, bFollowUp ? TEXT("partner_follow_up") : TEXT("end_turn"));
		}
		Transition(S::WaitForCommand, FString::Printf(TEXT("%s P%d"), bFirst ? TEXT("match_start") : TEXT("turn_start"), G->ActivePlayer + 1));
		OnTurnStarted(G->ActivePlayer, bFollowUp ? TEXT("follow_up") : TEXT("turn"));
	}

	switch (G->Phase)
	{
	case EZoogiPhase::Aiming:
		if (G->Stage == EZoogiStage::Awaken) OnAwakenObserved();
		if (G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::PowerUp)
		{
			if (State == S::WaitForCommand) Transition(S::ActionPreview, TEXT("command_confirmed"));
		}
		else if (State == S::ActionPreview) Transition(S::WaitForCommand, TEXT("command_cancelled"));
		break;
	case EZoogiPhase::Rolling:
		if (State == S::WaitForCommand || State == S::ActionPreview) Transition(S::PhysicsResolve, TEXT("launch"));
		break;
	case EZoogiPhase::Resolving:
		if (State == S::PhysicsResolve || State == S::MiniGame)
		{
			// normally HandleActionResolving (episode tick) got here first; this is the fallback ordering
			if (const UZoogiEpisodeSubsystem* Ep = Episode()) EvaluateAtResolve(Ep->GetTruth(), Ep->GetLiveRecognition());
			if (State == S::PhysicsResolve || State == S::MiniGame) Transition(S::Result, TEXT("resolve"));
		}
		if (G->Stage == EZoogiStage::End && State == S::Result) Transition(S::EpisodeEvent, TEXT("end_beat"));
		break;
	default:
		break;
	}
}

void UZoogiBattleStateMachine::OnTurnStarted(int32 Player, const FString& Why)
{
	AZoogiGameMode* G = GM();
	if (Players.IsValidIndex(Player)) Players[Player].Turns++;
	if (UZoogiReactionComponent* R = Reaction(Player)) R->OnOwnTurn(Players.IsValidIndex(Player) ? Players[Player].Turns : 0);
	if (bFlow && Tune.ActivationRule.Equals(TEXT("Always"), ESearchCase::IgnoreCase))
		if (UZoogiReactionComponent* R = Reaction(Player)) if (!R->bAwakened) R->BeginAwakening(TEXT("rule_always"), Players[Player].Turns, Tune.AwakenTurns, Tune.ReactionsPerAwakening);
	// a combo committed but never launched (turn skipped): refund it
	if (Pending.Owner >= 0 && Pending.ActionId < 0 && Pending.Owner != Player)
	{
		if (Pending.bPaid) { if (UZoogiEpisodeEnergyComponent* E = Energy(Pending.Owner)) E->Refund(Pending.Cost, TEXT("not_launched")); }
		UE_LOG(LogZoogi, Log, TEXT("BATTLE combo %s cancelled: P%d never launched"), *Pending.ComboId.ToString(), Pending.Owner + 1);
		Pending = FZoogiPendingSegment();
	}
	AwakenSeenTurn = -1;
	HumanCursor = -1;
	TestTurnOrder.Add(Player);
	TestTurnFollowUp.Add(Why == TEXT("follow_up"));
	if (G && Players.IsValidIndex(Player))
		UE_LOG(LogZoogi, Log, TEXT("BATTLE turn P%d %s (%s) own_turn=%d energy=%.0f awakened=%d"), Player + 1, *Players[Player].Name, *Why, Players[Player].Turns, GetEnergy(Player), IsAwakened(Player) ? 1 : 0);
}

void UZoogiBattleStateMachine::OnAwakenObserved()
{
	AZoogiGameMode* G = GM();
	if (!bFlow || !G->bAwakenDone || AwakenSeenTurn == G->TurnCounter) return;
	AwakenSeenTurn = G->TurnCounter;
	const int32 P = G->ActivePlayer;
	const bool bInfuse = G->AwakenChoiceText.StartsWith(TEXT("INFUSE"));
	const bool bGuard = G->AwakenChoiceText.StartsWith(TEXT("GUARD"));
	const FString& Rule = Tune.ActivationRule;
	bool bOn = false;
	if (Rule.Equals(TEXT("Infuse"), ESearchCase::IgnoreCase)) bOn = bInfuse;
	else if (Rule.Equals(TEXT("Guard"), ESearchCase::IgnoreCase)) bOn = bGuard;
	else if (Rule.Equals(TEXT("InfuseOrGuard"), ESearchCase::IgnoreCase)) bOn = bInfuse || bGuard;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE awaken choice P%d \"%s\" rule=%s -> awakened=%d (energy %.0f is not a factor)"), P + 1, *G->AwakenChoiceText, *Rule, bOn ? 1 : 0, GetEnergy(P));
	if (bOn)
		if (UZoogiReactionComponent* R = Reaction(P)) R->BeginAwakening(bInfuse ? TEXT("infuse") : TEXT("guard"), Players.IsValidIndex(P) ? Players[P].Turns : 0, Tune.AwakenTurns, Tune.ReactionsPerAwakening);
}

// =====================================================================================================================
// combos

bool UZoogiBattleStateMachine::CanAffordCombo(int32 Player, const FZoogiEpisodeComboRow& C) const
{
	return GetEnergy(Player) + KINDA_SMALL_NUMBER >= C.Cost;
}

bool UZoogiBattleStateMachine::SelectCombo(int32 Player, FName ComboId, const FString& Why)
{
	AZoogiGameMode* G = GM();
	if (!bFlow || !G || G->ActivePlayer != Player) return false;
	if (State != EZoogiBattleState::WaitForCommand && State != EZoogiBattleState::ActionPreview) return false;
	const FZoogiEpisodeComboRow* C = Tune.FindCombo(ComboId);
	if (!C) return false;
	if (Pending.Owner >= 0) CancelCombo(TEXT("replaced"));
	if (!CanAffordCombo(Player, *C))
	{
		UE_LOG(LogZoogi, Log, TEXT("BATTLE combo %s refused for P%d: cost %.0f > energy %.0f"), *ComboId.ToString(), Player + 1, C->Cost, GetEnergy(Player));
		return false;
	}
	Pending = FZoogiPendingSegment();
	Pending.SegmentId = FString::Printf(TEXT("seg_%03d_p%d_%s"), NextSegment++, Player + 1, *ComboId.ToString());
	Pending.ComboId = C->Id;
	Pending.ClipId = FName(*C->ClipId.ToString().Replace(TEXT("{zoogi}"), *Players[Player].Name.ToLower()));
	Pending.Owner = Player;
	Pending.Challenge = C->Challenge;
	Pending.Cost = C->Cost;
	Pending.Result = EZoogiChallengeResult::Pending;
	Pending.Comic = EZoogiComicState::Pending;
	if (Tune.bSpendOnSelect) Pending.bPaid = SpendEnergy(Player, C->Cost, ComboId);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE combo select P%d %s (%s) challenge=%s cost=%.0f paid=%d segment=%s why=%s"), Player + 1, *ComboId.ToString(), *C->Label,
		ZoogiBattle::ChallengeName(C->Challenge), C->Cost, Pending.bPaid ? 1 : 0, *Pending.SegmentId, *Why);
	return true;
}

void UZoogiBattleStateMachine::CancelCombo(const FString& Why)
{
	if (Pending.Owner < 0) return;
	if (Pending.ActionId >= 0) return; // already launched: it resolves (success / shatter)
	if (Pending.bPaid) if (UZoogiEpisodeEnergyComponent* E = Energy(Pending.Owner)) E->Refund(Pending.Cost, TEXT("cancel"));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE combo cancel P%d %s why=%s"), Pending.Owner + 1, *Pending.ComboId.ToString(), *Why);
	Pending = FZoogiPendingSegment();
}

void UZoogiBattleStateMachine::PickCPUCombo(int32 Player)
{
	AZoogiGameMode* G = GM();
	if (!G || !Tune.bCPUCombos || Pending.Owner >= 0) return;
	const AActor* Target = G->TargetLock.IsValid() ? G->TargetLock.Get() : (G->Preview.bContact ? G->Preview.ContactActor.Get() : nullptr);
	if (CPUPlan.IsFor(Player, G->TurnCounter))
	{
		// Oct 8: the CPU is playing a map prop route (PlanCPUProp): commit the matching route combo when affordable
		const FZoogiEpisodeComboRow* Row = Tune.Combos.FindByPredicate([this](const FZoogiEpisodeComboRow& C) { return C.Id == CPUPlan.Combo; });
		if (Row && Row->bCPU && CanAffordCombo(Player, *Row))
		{
			SelectCombo(Player, Row->Id, TEXT("cpu_prop"));
			if (Pending.Owner == Player && CPUPlan.Target.IsValid()) Pending.DeclaredTarget = BodyCodeOf(CPUPlan.Target.Get());
			UE_LOG(LogZoogi, Log, TEXT("BATTLE cpu prop combo P%d %s target=%s (%s)"), Player + 1, *Row->Id.ToString(), *BodyLabel(Pending.DeclaredTarget), *CPUPlan.Desc);
			return;
		}
		UE_LOG(LogZoogi, Log, TEXT("BATTLE cpu prop route P%d without combo (%s not affordable)"), Player + 1, *CPUPlan.Combo.ToString());
		return; // the prop route itself still plays (rail mini-game / halo / bumper events), no other combo this turn
	}
	const FZoogiEpisodeComboRow* Best = nullptr;
	for (const FZoogiEpisodeComboRow& C : Tune.Combos)
	{
		if (!C.bCPU || !CanAffordCombo(Player, C)) continue;
		if (C.Challenge != EZoogiChallenge::None && C.Challenge != EZoogiChallenge::Strike) continue;
		if (C.Challenge == EZoogiChallenge::Strike && !Cast<AZoogiBall>(Target)) continue;
		if (!Best || C.Cost > Best->Cost) Best = &C;
	}
	if (Best) SelectCombo(Player, Best->Id, TEXT("cpu"));
}

// =====================================================================================================================
// episode events

void UZoogiBattleStateMachine::HandleActionStarted(const FZoogiGameplayTruthRecord& Truth)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	CurrentActionId = Truth.ActionId;
	CurrentActor = Truth.Actor;
	bResolvedThisAction = false;
	ActionEnergyDelta.Reset();
	ActionZoneTags.Reset();
	ActionPropTags.Reset();
	if (const UWorld* W = GetWorld()) ActionStartGame = W->GetTimeSeconds();
	if (Players.IsValidIndex(Truth.Actor)) Players[Truth.Actor].Actions++;
	if (bFlow && G->Balls.IsValidIndex(Truth.Actor) && G->Balls[Truth.Actor] && G->Balls[Truth.Actor]->bCPU && (TestCase.IsEmpty() || TestCase.StartsWith(TEXT("map_")))) PickCPUCombo(Truth.Actor);
	const bool bRoute = bFlow && Pending.Owner == Truth.Actor && Pending.ActionId < 0 && (Pending.Challenge == EZoogiChallenge::HaloBank || Pending.Challenge == EZoogiChallenge::OrbLine || Pending.Challenge == EZoogiChallenge::SkyBumper || Pending.Challenge == EZoogiChallenge::PartnerIntercept || Pending.Challenge == EZoogiChallenge::RailRelease);
	if (!bRoute && (State == EZoogiBattleState::WaitForCommand || State == EZoogiBattleState::ActionPreview)) Transition(EZoogiBattleState::PhysicsResolve, TEXT("launch"));
	if (!bFlow) return;
	if (Pending.Owner != Truth.Actor || Pending.ActionId >= 0) return;
	Pending.ActionId = Truth.ActionId;
	// declared target: the lock (or the aim preview's first contact) at the moment of launch
	const AActor* T = (!G->bFreeAim && G->TargetLock.IsValid()) ? G->TargetLock.Get() : nullptr;
	if (!T && G->Preview.bContact) T = G->Preview.ContactActor.Get();
	if (Pending.DeclaredTarget < 0) Pending.DeclaredTarget = BodyCodeOf(T);
	if (Pending.Challenge == EZoogiChallenge::HaloBank || Pending.Challenge == EZoogiChallenge::OrbLine || Pending.Challenge == EZoogiChallenge::SkyBumper || Pending.Challenge == EZoogiChallenge::PartnerIntercept || Pending.Challenge == EZoogiChallenge::RailRelease)
	{
		// route mini-game: the objective is committed from the real launch (aim preview / lock), then physics decides
		Transition(EZoogiBattleState::MiniGame, FString::Printf(TEXT("objective_commit %s"), ZoogiBattle::ChallengeName(Pending.Challenge)));
		if (UZoogiMiniGame* M = UZoogiMiniGame::Create(this, Pending.Challenge)) M->Enter(this, Truth.Actor, Pending);
		UE_LOG(LogZoogi, Log, TEXT("BATTLE minigame %s enter P%d target=%s orbs=%d"), ZoogiBattle::ChallengeName(Pending.Challenge), Truth.Actor + 1, *BodyLabel(Pending.DeclaredTarget), Pending.CommittedOrbs.Num());
		Transition(EZoogiBattleState::PhysicsResolve, TEXT("launch"));
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE combo launch P%d %s challenge=%s target=%s orbs=%d action=%d"), Truth.Actor + 1, *Pending.ComboId.ToString(), ZoogiBattle::ChallengeName(Pending.Challenge),
		*BodyLabel(Pending.DeclaredTarget), Pending.CommittedOrbs.Num(), Truth.ActionId);
}

void UZoogiBattleStateMachine::HandleActionResolving(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec)
{
	AZoogiGameMode* G = GM();
	if (bTestInjectOffer && G && G->bComboMode && Truth.Actor == 0)
	{
		// test only (timeout / queued_effects): a Partner Follow-Up offer is pending at Resolve
		bTestInjectOffer = false;
		G->bFollowUpOffered = true;
		G->FollowUpOfferAt = G->GetClock();
		UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST injected a Partner Follow-Up offer (partner P2 human=%d)"), G->Balls.IsValidIndex(1) && G->Balls[1] && !G->Balls[1]->bCPU ? 1 : 0);
	}
	EvaluateAtResolve(Truth, Rec);
	if (State == EZoogiBattleState::PhysicsResolve || State == EZoogiBattleState::MiniGame) Transition(EZoogiBattleState::Result, TEXT("resolve"));
}

void UZoogiBattleStateMachine::EvaluateAtResolve(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec)
{
	if (bResolvedThisAction || Truth.ActionId != CurrentActionId) return;
	bResolvedThisAction = true;
	if (ActiveMiniGame) FinishMiniGame(EZoogiChallengeResult::Cancelled, TEXT("resolved_before_input"));
	if (!bFlow) return;
	FString Fail;
	// Air Reaction segment: success = the reacting Zoogi landed in (not rung out)
	if (AirSegment.Owner >= 0 && AirSegment.ActionId == Truth.ActionId && AirSegment.Result == EZoogiChallengeResult::Pending)
	{
		FString Detail;
		const EZoogiChallengeResult R = UZoogiMiniGame::EvaluateRoute(this, AirSegment, Truth, Detail);
		AirSegment.Result = R;
		UE_LOG(LogZoogi, Log, TEXT("BATTLE challenge AirReaction P%d result=%s detail=\"%s\" action=%d"), AirSegment.Owner + 1, ZoogiBattle::ResultName(R), *Detail, Truth.ActionId);
		if (R == EZoogiChallengeResult::Success)
		{
			AirSegment.Comic = EZoogiComicState::Completed;
			AirSegment.Detail = Detail;
			if (Players.IsValidIndex(AirSegment.Owner)) Players[AirSegment.Owner].SegmentsCompleted++;
			AddEnergy(AirSegment.Owner, Tune.Gain(TEXT("Reaction")), TEXT("Reaction"), Truth.ActionId);
			UE_LOG(LogZoogi, Log, TEXT("BATTLE segment COMPLETED %s clip=%s P%d"), *AirSegment.SegmentId, *AirSegment.ClipId.ToString(), AirSegment.Owner + 1);
			History.Add(AirSegment);
			AirSegment = FZoogiPendingSegment();
		}
		else Fail = FString::Printf(TEXT("AirReaction_%s: %s"), ZoogiBattle::ResultName(R), *Detail);
	}
	if (Pending.Owner >= 0 && Pending.ActionId == Truth.ActionId)
	{
		FString Detail;
		const EZoogiChallengeResult R = EvaluateChallenge(Truth, Rec, Detail);
		Pending.Result = R;
		Pending.Detail = Detail;
		UE_LOG(LogZoogi, Log, TEXT("BATTLE challenge %s P%d combo=%s result=%s detail=\"%s\" action=%d"), ZoogiBattle::ChallengeName(Pending.Challenge), Pending.Owner + 1, *Pending.ComboId.ToString(),
			ZoogiBattle::ResultName(R), *Detail, Truth.ActionId);
		if (R == EZoogiChallengeResult::Success)
		{
			Pending.Comic = EZoogiComicState::Completed;
			if (!Pending.bPaid && !Tune.bSpendOnSelect) Pending.bPaid = SpendEnergy(Pending.Owner, Pending.Cost, Pending.ComboId);
			if (Pending.Challenge == EZoogiChallenge::OrbLine) AddEnergy(Pending.Owner, Tune.Gain(TEXT("OrbLineOrb")) * Pending.CommittedOrbs.Num(), TEXT("OrbLineOrb"), Truth.ActionId);
			if (Players.IsValidIndex(Pending.Owner)) Players[Pending.Owner].SegmentsCompleted++;
			UE_LOG(LogZoogi, Log, TEXT("BATTLE segment COMPLETED %s clip=%s P%d"), *Pending.SegmentId, *Pending.ClipId.ToString(), Pending.Owner + 1);
			History.Add(Pending);
			Pending = FZoogiPendingSegment();
		}
		else Fail += FString::Printf(TEXT("%s%s_%s: %s"), Fail.IsEmpty() ? TEXT("") : TEXT(" + "), ZoogiBattle::ChallengeName(Pending.Challenge), ZoogiBattle::ResultName(R), *Detail);
	}
	if (!Fail.IsEmpty()) Shatter(Fail);
}

EZoogiChallengeResult UZoogiBattleStateMachine::EvaluateChallenge(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, FString& OutDetail) const
{
	const FZoogiEpisodeComboRow* C = Tune.FindCombo(Pending.ComboId);
	if (C && Rec.Tier < C->MinTier) { OutDetail = FString::Printf(TEXT("tier %d < %d"), Rec.Tier, C->MinTier); return EZoogiChallengeResult::Failed; }
	if (C && C->RequiredTag != NAME_None)
	{
		bool bHas = false;
		for (EZoogiEpisodeTag T : Rec.Tags) if (C->RequiredTag == FName(ZoogiEpisode::TagName(T))) bHas = true;
		if (!bHas) { OutDetail = FString::Printf(TEXT("missing tag %s"), *C->RequiredTag.ToString()); return EZoogiChallengeResult::Failed; }
	}
	switch (Pending.Challenge)
	{
	case EZoogiChallenge::None:
		OutDetail = TEXT("launched");
		return EZoogiChallengeResult::Success;
	case EZoogiChallenge::Strike:
	{
		// the shooter's first body contact (Zoogi or orb, either side reported) must be the declared target
		int32 First = -1; float FirstT = 1e9f;
		for (const FZoogiContactEvent& E : Truth.Contacts)
		{
			int32 Other = -1;
			if (E.Body == Truth.Actor && E.OtherBody >= 0) Other = E.OtherBody;
			else if (E.OtherBody == Truth.Actor && E.Body >= 0) Other = E.Body;
			if (Other >= 0 && E.Time < FirstT) { FirstT = E.Time; First = Other; }
		}
		if (Pending.DeclaredTarget < 0) { OutDetail = TEXT("no declared target"); return EZoogiChallengeResult::Failed; }
		OutDetail = FString::Printf(TEXT("first_contact=%s target=%s"), *BodyLabel(First), *BodyLabel(Pending.DeclaredTarget));
		return First == Pending.DeclaredTarget ? EZoogiChallengeResult::Success : EZoogiChallengeResult::Failed;
	}
	default:
		return UZoogiMiniGame::EvaluateRoute(this, Pending, Truth, OutDetail);
	}
}

void UZoogiBattleStateMachine::RecordShatter(FZoogiPendingSegment Seg, const FString& Why)
{
	AZoogiGameMode* G = GM();
	Seg.Comic = EZoogiComicState::Shattered;
	if (Seg.Result == EZoogiChallengeResult::Pending || Seg.Result == EZoogiChallengeResult::None) Seg.Result = EZoogiChallengeResult::Failed;
	if (Seg.Detail.IsEmpty()) Seg.Detail = Why;
	++ShatterCount;
	LastShatterAction = CurrentActionId;
	LastShatterReason = Why;
	LastShatterReal = RealClock;
	AerialShot(TEXT("shatter"), 0.25f);
	const int32 Owner = Seg.Owner >= 0 ? Seg.Owner : (G ? G->ActivePlayer : 0);
	if (Players.IsValidIndex(Owner)) Players[Owner].SegmentsShattered++;
	if (Seg.bPaid && Tune.ShatterRefundFrac > 0.f) if (UZoogiEpisodeEnergyComponent* E = Energy(Owner)) E->Refund(Seg.Cost * Tune.ShatterRefundFrac, TEXT("shatter_refund"));
	if (Tune.ShatterScorePenalty > 0.f && G && G->Balls.IsValidIndex(Owner) && G->Balls[Owner])
		G->Balls[Owner]->Score = FMath::Max(0, G->Balls[Owner]->Score - FMath::RoundToInt(Tune.ShatterScorePenalty));
	UE_LOG(LogZoogi, Log, TEXT("BATTLE shatter segment=%s P%d combo=%s challenge=%s action=%d reason=\"%s\""), Seg.SegmentId.IsEmpty() ? TEXT("-") : *Seg.SegmentId, Owner + 1,
		*Seg.ComboId.ToString(), ZoogiBattle::ChallengeName(Seg.Challenge), CurrentActionId, *Why);
	History.Add(Seg);
	LedgerSimple(TEXT("SHATTER"), Owner, FString::Printf(TEXT("%s|%s|%s"), *Seg.SegmentId, *Seg.ComboId.ToString(), *Why));
}

void UZoogiBattleStateMachine::Shatter(const FString& Why)
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	if (!G || !W) return;
	if (Pending.Owner >= 0) { FZoogiPendingSegment S = Pending; Pending = FZoogiPendingSegment(); RecordShatter(S, Why); }
	if (AirSegment.Owner >= 0) { FZoogiPendingSegment S = AirSegment; AirSegment = FZoogiPendingSegment(); RecordShatter(S, Why); }
	if (State != EZoogiBattleState::ShatterFailure) Transition(EZoogiBattleState::ShatterFailure, TEXT("challenge_failed"));

	// FAIL -> SHATTER -> NEXT_PLAYER: no follow-up offer, no consolation card / scene / hold; end the turn now.
	if (G->bComboMode && G->bFollowUpOffered) { G->bFollowUpOffered = false; UE_LOG(LogZoogi, Log, TEXT("BATTLE shatter cancels the pending Partner Follow-Up offer")); }
	if (UZoogiEpisodeSubsystem* Ep = Episode()) Ep->SuppressPresentation(TEXT("shatter"));
	const bool bClimax = G->ClimaxLeft > 0.f;
	if (UZoogiAnimeSubsystem* Anime = W->GetSubsystem<UZoogiAnimeSubsystem>()) if (Anime->IsBusy() && !bClimax) Anime->Skip();
	if (G->bComboMode && G->bComboMatchOver) return; // the match is decided: ComboFinish runs from the Resolve hold
	if (G->Stage == EZoogiStage::Resolve && !bClimax)
	{
		G->SetStage(EZoogiStage::End);
		G->StageTime = ZoogiStageTune::EndBeat; // EndTurn on the next game tick (End waits for nothing else)
		UE_LOG(LogZoogi, Log, TEXT("BATTLE shatter -> next player now (Resolve wait skipped)"));
	}
}

void UZoogiBattleStateMachine::HandleActionFinalized(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, const FZoogiCinematicPlan& Plan)
{
	if (bFlow) GrantEnergyFromTruth(Truth, Rec);
	if (Players.IsValidIndex(Truth.Actor))
	{
		FZoogiBattlePlayerState& P = Players[Truth.Actor];
		if (Rec.Tier >= 3) P.Spotlights++;
		for (int32 R : Truth.RingOuts) if (Players.IsValidIndex(R)) { if (R != Truth.Actor) P.KOs++; Players[R].TimesKOd++; }
		P.OrbOuts += Truth.OrbOuts;
	}
	LedgerAction(Truth, Rec, Plan);
	if (State == EZoogiBattleState::Result) Transition(EZoogiBattleState::EpisodeEvent, TEXT("action_recorded"));
	if (!TestCase.IsEmpty()) TestOnFinalized(Truth, Rec, Plan);
}

void UZoogiBattleStateMachine::GrantEnergyFromTruth(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec)
{
	const AZoogiGameMode* G = GM();
	const int32 A = Truth.Actor;
	if (!G || !G->Balls.IsValidIndex(A)) return;
	TMap<int32, float> RivalSpeed;      // rival body -> strongest meaningful contact with the actor
	int32 Impacts = 0;
	for (const FZoogiContactEvent& E : Truth.Contacts)
	{
		if (!E.bMeaningful) continue;
		int32 Other = -2;
		if (E.Body == A) Other = E.OtherBody;
		else if (E.OtherBody == A) Other = E.Body;
		if (Other == -2) continue;
		if (Other == -1) { ++Impacts; continue; }
		if (Other >= 100) continue;
		if (E.Kind == EZoogiContactKind::Ally || (G->bComboMode && AZoogiGameMode::TeamOfIndex(Other) == AZoogiGameMode::TeamOfIndex(A))) continue;
		float& Sp = RivalSpeed.FindOrAdd(Other);
		Sp = FMath::Max(Sp, E.RelSpeed);
	}
	const int32 Act = Truth.ActionId;
	for (const TPair<int32, float>& R : RivalSpeed)
	{
		AddEnergy(A, Tune.Gain(TEXT("Hit")), TEXT("Hit"), Act);
		if (R.Value >= Tune.StrongHitSpeed) AddEnergy(A, Tune.Gain(TEXT("StrongHit")), TEXT("StrongHit"), Act);
		if (Players.IsValidIndex(A)) Players[A].Hits++;
		// a guarded rival that absorbed the hit earns its own Guard block energy
		const AZoogiBall* V = G->Balls.IsValidIndex(R.Key) ? G->Balls[R.Key].Get() : nullptr;
		if (V && V->Artifact && V->Artifact->Guard != EZoogiArtifact::None) AddEnergy(R.Key, Tune.Gain(TEXT("GuardBlock")), TEXT("GuardBlock"), Act);
	}
	if (Impacts > 0) AddEnergy(A, Tune.Gain(TEXT("Impact")) * FMath::Min(Impacts, Tune.ImpactCap), TEXT("Impact"), Act);
	const int32 Cracks = Truth.PiecesDamaged + Truth.RocksDamaged;
	if (Cracks > 0) AddEnergy(A, Tune.Gain(TEXT("Crack")) * Cracks, TEXT("Crack"), Act);
	if (Truth.OrbOuts > 0) AddEnergy(A, Tune.Gain(TEXT("OrbOut")) * Truth.OrbOuts, TEXT("OrbOut"), Act);
	int32 KOs = 0;
	for (int32 R : Truth.RingOuts) if (R != A && !(G->bComboMode && AZoogiGameMode::TeamOfIndex(R) == AZoogiGameMode::TeamOfIndex(A))) ++KOs;
	if (KOs > 0) AddEnergy(A, Tune.Gain(TEXT("RingOut")) * KOs, TEXT("RingOut"), Act);
	if (Truth.bPowerHit) AddEnergy(A, Tune.Gain(TEXT("PowerHit")), TEXT("PowerHit"), Act);
	if (Rec.Tier >= 2) AddEnergy(A, Tune.Gain(TEXT("Feat")) * Rec.Tier, TEXT("Feat"), Act);
	FString Sum;
	for (const TPair<int32, float>& D : ActionEnergyDelta) Sum += FString::Printf(TEXT("%sP%d%+.0f"), Sum.IsEmpty() ? TEXT("") : TEXT(","), D.Key + 1, D.Value);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE energy action=%d actor=P%d rivals_hit=%d impacts=%d cracks=%d orb_outs=%d kos=%d power=%d tier=%d delta=%s energy=%.0f"), Act, A + 1, RivalSpeed.Num(), Impacts, Cracks,
		Truth.OrbOuts, KOs, Truth.bPowerHit ? 1 : 0, Rec.Tier, Sum.IsEmpty() ? TEXT("-") : *Sum, GetEnergy(A));
}

// =====================================================================================================================
// debug input (F5 cycle combo, F6 confirm / cancel, F7 Brace, F8 Redirect, F9 Rail Release) + overlay. Engine-only placeholders.

void UZoogiBattleStateMachine::DebugInput()
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!G || !PC || !G->Balls.IsValidIndex(G->ActivePlayer) || !G->Balls[G->ActivePlayer]) return;
	if (PC->WasInputKeyJustPressed(EKeys::F7)) SubmitAirChoice(EZoogiAirChoice::Brace, TEXT("key"));
	if (PC->WasInputKeyJustPressed(EKeys::F8)) SubmitAirChoice(EZoogiAirChoice::Redirect, TEXT("key"));
	if (PC->WasInputKeyJustPressed(EKeys::F9)) SubmitRailRelease(TEXT("key"));
	const int32 P = G->ActivePlayer;
	if (G->Balls[P]->bCPU || (State != EZoogiBattleState::WaitForCommand && State != EZoogiBattleState::ActionPreview)) return;
	if (PC->WasInputKeyJustPressed(EKeys::F5) && Tune.Combos.Num() > 0)
	{
		HumanCursor = (HumanCursor + 1) % Tune.Combos.Num();
		const FZoogiEpisodeComboRow& C = Tune.Combos[HumanCursor];
		UE_LOG(LogZoogi, Log, TEXT("BATTLE combo cursor P%d -> %s cost=%.0f affordable=%d"), P + 1, *C.Id.ToString(), C.Cost, CanAffordCombo(P, C) ? 1 : 0);
	}
	if (PC->WasInputKeyJustPressed(EKeys::F6))
	{
		if (Pending.Owner == P) CancelCombo(TEXT("key"));
		else if (Tune.Combos.IsValidIndex(HumanCursor)) SelectCombo(P, Tune.Combos[HumanCursor].Id, TEXT("key"));
	}
}

void UZoogiBattleStateMachine::DebugOverlay()
{
	const AZoogiGameMode* G = GM();
	if (!GEngine || !G || G->Phase == EZoogiPhase::Menu) return;
	int32 Key = 7700;
	const FColor Dim(200, 210, 230);
	GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor::White, FString::Printf(TEXT("BATTLE %s   P%d   turn %d   [F5 combo  F6 select/cancel  F7 brace  F8 redirect  F9 rail]"),
		ZoogiBattle::StateName(State), G->ActivePlayer + 1, G->TurnCounter));
	for (const FZoogiBattlePlayerState& P : Players)
	{
		const UZoogiReactionComponent* R = Reaction(P.Player);
		GEngine->AddOnScreenDebugMessage(Key++, 0.f, P.Player == G->ActivePlayer ? FColor::Yellow : Dim, FString::Printf(TEXT("P%d %-10s EN %3.0f/%.0f  %s  segs %d ok / %d shattered  KOs %d"),
			P.Player + 1, *P.Name, GetEnergy(P.Player), Tune.EnergyCap, (R && R->bAwakened) ? *FString::Printf(TEXT("AWAKENED(%d)"), R->ReactionsLeft) : TEXT("-          "),
			P.SegmentsCompleted, P.SegmentsShattered, P.KOs));
	}
	if (Pending.Owner >= 0)
		GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor::Cyan, FString::Printf(TEXT("COMBO %s  %s -> %s  (%s)"), *Pending.ComboId.ToString(), ZoogiBattle::ChallengeName(Pending.Challenge),
			*BodyLabel(Pending.DeclaredTarget), ZoogiBattle::ResultName(Pending.Result)));
	else if (Tune.Combos.IsValidIndex(HumanCursor))
		GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor::Cyan, FString::Printf(TEXT("combo cursor: %s (cost %.0f)  F6 to commit"), *Tune.Combos[HumanCursor].Label, Tune.Combos[HumanCursor].Cost));
	if (IsRailOpen())
		GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor::Green, TEXT("RAIL RELEASE: F9 to release (early = high / center = flat / late = bank)"));
	if (IsAirReactionOpen())
		GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor::Orange, FString::Printf(TEXT("AIR REACTION P%d: F7 BRACE / F8 REDIRECT"), AirSegment.Owner + 1));
	if (RealClock - LastShatterReal < 2.0)
		GEngine->AddOnScreenDebugMessage(Key++, 0.f, FColor::Red, FString::Printf(TEXT("SHATTER!  %s"), *LastShatterReason));
}
