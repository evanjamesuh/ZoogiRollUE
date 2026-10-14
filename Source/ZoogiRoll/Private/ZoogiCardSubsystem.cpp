#include "ZoogiCardSubsystem.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiTypes.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UZoogiCardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Tune.Load();
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiCardTest="), TestCase);
	if (Tune.bCards)
		UE_LOG(LogZoogi, Log, TEXT("CARDS on: hand=%d energy=%d deck=%d maxcopies=%d test=%s"), Tune.HandSize, Tune.EnergyPerTurn, Tune.DeckSize, Tune.MaxCopies, TestCase.IsEmpty() ? TEXT("-") : *TestCase);
}

UZoogiCardSubsystem* UZoogiCardSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UZoogiCardSubsystem>() : nullptr;
}

int32 UZoogiCardSubsystem::ActivePlayer(AZoogiGameMode* G) const
{
	const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	return B ? B->PlayerIndex : -1;
}

void UZoogiCardSubsystem::LoadData(TArray<FZoogiCardDef>& Defs, TArray<FName>& Deck)
{
	Defs = ZoogiCards::BuiltInDefs();
	Deck = ZoogiCards::BuiltInDeck();
	DataSource = TEXT("builtin");
	FString Text, Err;
	const FString Dir = FPaths::ProjectContentDir() / TEXT("Data");
	TArray<FZoogiCardDef> Csv;
	if (FFileHelper::LoadFileToString(Text, *(Dir / TEXT("Cards.csv"))))
	{
		if (ZoogiCards::ParseCardsCsv(Text, Csv, Err) && Csv.Num() > 0) { Defs = Csv; DataSource = TEXT("csv"); }
		else UE_LOG(LogZoogi, Warning, TEXT("CARDS Cards.csv rejected (%s): built-in table used"), *Err);
	}
	TMap<FString, TArray<FName>> Decks;
	if (FFileHelper::LoadFileToString(Text, *(Dir / TEXT("Decks.csv"))) && ZoogiCards::ParseDecksCsv(Text, Decks, Err))
	{
		const FString Want = Tune.DefaultDeck.IsEmpty() ? FString(TEXT("starter")) : Tune.DefaultDeck;
		if (const TArray<FName>* D = Decks.Find(Want)) Deck = *D;
		else UE_LOG(LogZoogi, Warning, TEXT("CARDS deck '%s' not in Decks.csv: built-in starter used"), *Want);
	}
}

void UZoogiCardSubsystem::EnsureMatch(AZoogiGameMode* G)
{
	bool bSame = MatchBalls.Num() == G->Balls.Num() && RulesV.NumPlayers() == G->Balls.Num();
	for (int32 i = 0; bSame && i < G->Balls.Num(); ++i) bSame = MatchBalls[i].Get() == G->Balls[i];
	if (bSame) return;
	MatchBalls.Reset();
	for (AZoogiBall* B : G->Balls) MatchBalls.Add(B);
	TArray<FZoogiCardDef> Defs; TArray<FName> Deck;
	LoadData(Defs, Deck);
	TSharedPtr<IZoogiCardBackend> Backend = MakeShared<FZoogiNativeCardBackend>(Defs);
	TArray<TArray<FName>> Decks; Decks.Init(Deck, G->Balls.Num());
	const int32 Seed = G->GameSeed ^ 0x5EEDCA4D; // own stream: card draws never consume GameRng numbers
	FString LogS;
	const bool bOk = RulesV.Init(Backend, Tune, Seed, Decks, ZoogiCards::BuiltInDeck(), LogS);
	LastStandbyTurn = -1;
	CollapseEffectId = 0; CollapseOwner = -1; CollapsePiece.Reset();
	UE_LOG(LogZoogi, Log, TEXT("CARDS match: backend=%s data=%s defs=%d players=%d seed=%d ok=%d%s"), *Backend->BackendName().ToString(), *DataSource, Defs.Num(), G->Balls.Num(), Seed, bOk ? 1 : 0, *LogS);
}

bool UZoogiCardSubsystem::IsCollapseEligible(const AZoogiArenaPiece* Piece)
{
	return Piece && !Piece->IsIndestructible() && !Piece->IsBroken() && Piece->IsBuilt();
}

FZoogiCardContext UZoogiCardSubsystem::BuildContext(AZoogiGameMode* G) const
{
	FZoogiCardContext C;
	const AZoogiBall* Me = G ? G->GetActiveBall() : nullptr;
	C.bActorKnockedOut = !Me || Me->bKnockedOut;
	C.bHasNeutralOrb = false;
	if (G) for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { C.bHasNeutralOrb = true; break; }
	C.bHasEnemy = false;
	if (G && Me) for (const AZoogiBall* B : G->Balls)
		if (B && B != Me && !B->bKnockedOut && (!G->bComboMode || B->PlayerIndex / 2 != Me->PlayerIndex / 2)) { C.bHasEnemy = true; break; }
	C.bHasBankSurface = true;   // every arena has rim walls / bumpers (Bank Strike validity is decided by real contacts later)
	C.bHasSurfaceLane = true;
	C.bHasCrystalPiece = false;
	if (UWorld* W = GetWorld()) for (TActorIterator<AZoogiArenaPiece> It(W); It; ++It) if (IsCollapseEligible(*It)) { C.bHasCrystalPiece = true; break; }
	return C;
}

void UZoogiCardSubsystem::BeginStandby(AZoogiGameMode* G)
{
	if (!Tune.bCards || !G) return;
	EnsureMatch(G);
	const int32 P = ActivePlayer(G);
	if (P < 0) return;
	ExpireCollapse(TEXT("turn_end"));
	RulesV.BeginStandby(P, G->TurnCounter);
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	FString Hand;
	for (const FZoogiCardInstance& C : S->Hand) Hand += FString::Printf(TEXT(" %s#%d"), *C.CardId.ToString(), C.InstanceId);
	UE_LOG(LogZoogi, Log, TEXT("CARDS standby P%d turn=%d energy=%d hand=[%s ] draw=%d discard=%d"), P + 1, G->TurnCounter, S->Energy, *Hand, S->DrawPile.Num(), S->Discard.Num());
	if (LastStandbyTurn != G->TurnCounter) { LastStandbyTurn = G->TurnCounter; TurnPhases.Reset(); TurnPhases.Add(TEXT("STANDBY")); LastPhase = TEXT("STANDBY"); }
	TestOnStandby(G, P);
}

EZoogiCardResult UZoogiCardSubsystem::ToggleSlot(AZoogiGameMode* G, int32 Slot)
{
	const int32 P = ActivePlayer(G);
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	if (!S) return EZoogiCardResult::UnknownPlayer;
	if (!S->Hand.IsValidIndex(Slot)) return EZoogiCardResult::UnknownInstance;
	const EZoogiCardResult R = RulesV.Toggle(P, S->Hand[Slot].InstanceId, BuildContext(G));
	UE_LOG(LogZoogi, Log, TEXT("CARDS toggle P%d slot=%d %s -> %s (selected %d)"), P + 1, Slot, *S->Hand[Slot].CardId.ToString(), ZoogiCardResultName(R), S->Selected.Num());
	return R;
}

void UZoogiCardSubsystem::ClearSelection(AZoogiGameMode* G) { RulesV.ClearSelection(ActivePlayer(G)); }

EZoogiCardResult UZoogiCardSubsystem::Commit(AZoogiGameMode* G, FString& OutChoice)
{
	const int32 P = ActivePlayer(G);
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	if (!S) return EZoogiCardResult::UnknownPlayer;
	const int32 EBefore = S->Energy, SpentBefore = RulesV.TotalSpent(P);
	TArray<FZoogiCardCommitted> Out;
	const EZoogiCardResult R = RulesV.Commit(P, BuildContext(G), &Out);
	if (R != EZoogiCardResult::Ok) { UE_LOG(LogZoogi, Log, TEXT("CARDS commit P%d rejected: %s (energy %d unchanged)"), P + 1, ZoogiCardResultName(R), S->Energy); return R; }
	int32 Cost = 0;
	OutChoice = Out.Num() ? TEXT("CARDS ") : TEXT("CARDS  PASS");
	for (int32 k = 0; k < Out.Num(); ++k)
	{
		const FZoogiCardCommitted& K = Out[k];
		const FZoogiCardDef* D = Def(K.Card.CardId);
		Cost += D ? D->Cost : 0;
		OutChoice += FString::Printf(TEXT("%s%s"), k == 0 ? TEXT(" ") : TEXT(" + "), D ? *D->Name : *K.Card.CardId.ToString());
		if (K.Card.CardId == FName(TEXT("crystal_collapse"))) { CollapseEffectId = K.EffectId; CollapseOwner = P; CollapsePiece.Reset(); }
	}
	UE_LOG(LogZoogi, Log, TEXT("CARDS commit P%d commit=%lld cards=%d cost=%d energy %d -> %d (effects: Phase 3/4 except crystal_collapse)"), P + 1, Out.Num() ? Out[0].CommitId : 0, Out.Num(), Cost, EBefore, S->Energy);
	if (LastPhase != TEXT("COMMIT")) { TurnPhases.Add(TEXT("COMMIT")); LastPhase = TEXT("COMMIT"); }
	TestOnCommit(G, P, EBefore, SpentBefore, Cost);
	return R;
}

void UZoogiCardSubsystem::PlanCPU(AZoogiGameMode* G)
{
	const int32 P = ActivePlayer(G);
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	if (!S || S->bCommitted) return;
	// Same rules as a human: try cards by CPU weight (then cost), each through Select (rejections spend nothing).
	TArray<int32> Order;
	for (int32 i = 0; i < S->Hand.Num(); ++i) Order.Add(i);
	const TArray<FZoogiCardInstance> Hand = S->Hand;
	Order.Sort([&](int32 A, int32 B)
	{
		const FZoogiCardDef* DA = Def(Hand[A].CardId); const FZoogiCardDef* DB = Def(Hand[B].CardId);
		const int32 WA = DA ? DA->CPUWeight * 10 + DA->Cost : 0, WB = DB ? DB->CPUWeight * 10 + DB->Cost : 0;
		return WA != WB ? WA > WB : Hand[A].InstanceId < Hand[B].InstanceId;
	});
	const FZoogiCardContext Ctx = BuildContext(G);
	for (int32 i : Order) RulesV.Select(P, Hand[i].InstanceId, Ctx);
	FString Choice;
	Commit(G, Choice);
	G->AwakenChoiceText = Choice;
}

// ------------------------------------------------------------------ Crystal Collapse (card-scoped destruction exemption)

void UZoogiCardSubsystem::OnPieceContact(AZoogiArenaPiece* Piece, AZoogiBall* Ball, const FVector& At, const FVector& Dir)
{
	if (!Tune.bCards || CollapseEffectId == 0 || CollapsePiece.IsValid() || !Ball || Ball->PlayerIndex != CollapseOwner) return;
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || G->Stage != EZoogiStage::Battle || ActivePlayer(G) != CollapseOwner) return; // only the committed lead action
	if (!IsCollapseEligible(Piece)) { UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld contact with %s ignored (not eligible: indestructible/broken)"), CollapseEffectId, *GetNameSafe(Piece)); return; }
	CollapsePiece = Piece; CollapseAt = At; CollapseDir = Dir;
	UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld qualified contact P%d -> %s (applies at the Resolve boundary)"), CollapseEffectId, CollapseOwner + 1, *Piece->GetName());
}

void UZoogiCardSubsystem::ApplyPendingCollapses(AZoogiGameMode* G)
{
	if (CollapseEffectId == 0 || !CollapsePiece.IsValid()) return;
	AZoogiArenaPiece* Piece = CollapsePiece.Get();
	const FString Before = AZoogiArenaPiece::StateName(Piece->GetPieceState());
	const bool bChanged = Piece->ApplyCardCollapse(CollapseEffectId, CollapseAt, CollapseDir);
	UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld %s: %s -> %s changed=%d (global bDestruction=%d)"), CollapseEffectId, *Piece->GetName(), *Before,
		*AZoogiArenaPiece::StateName(Piece->GetPieceState()), bChanged ? 1 : 0, ZoogiUtil::DestructionEnabled() ? 1 : 0);
	CollapseEffectId = 0; CollapseOwner = -1; CollapsePiece.Reset();
}

void UZoogiCardSubsystem::ExpireCollapse(const TCHAR* Why)
{
	if (CollapseEffectId == 0) return;
	UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld expired (%s, no qualifying contact)"), CollapseEffectId, Why);
	CollapseEffectId = 0; CollapseOwner = -1; CollapsePiece.Reset();
}

// ------------------------------------------------------------------ phase observation

void UZoogiCardSubsystem::ObservePhase(AZoogiGameMode* G)
{
	if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver) return;
	FString Ph;
	switch (G->Stage)
	{
	case EZoogiStage::Draw: case EZoogiStage::Awaken: Ph = G->bAwakenDone ? TEXT("COMMIT") : TEXT("STANDBY"); break;
	case EZoogiStage::Battle: Ph = G->BattleStep == EZoogiBattleStep::Aim ? TEXT("AIM_FORCE") : G->BattleStep == EZoogiBattleStep::PowerUp ? TEXT("CONFIRM") : TEXT("ROLL"); break;
	case EZoogiStage::Resolve: Ph = TEXT("RESOLVE"); break;
	default: Ph = TEXT("END"); break;
	}
	if (Ph == LastPhase) return;
	LastPhase = Ph;
	TurnPhases.Add(Ph);
	UE_LOG(LogZoogi, Log, TEXT("CARDS phase %s P%d turn=%d"), *Ph, ActivePlayer(G) + 1, G->TurnCounter);
	if (Ph == TEXT("RESOLVE"))
	{
		ApplyPendingCollapses(G);
		if (!TestCase.IsEmpty() && TestStandbys > 0)
		{
			// order check for this turn: STANDBY < COMMIT < (AIM_FORCE) < CONFIRM < ROLL < RESOLVE
			const FString Seq = FString::Join(TurnPhases, TEXT(">"));
			const int32 iS = TurnPhases.Find(TEXT("STANDBY")), iC = TurnPhases.Find(TEXT("COMMIT")), iR = TurnPhases.Find(TEXT("ROLL")), iV = TurnPhases.Find(TEXT("RESOLVE"));
			TestCheck(iS == 0 && iC > iS && iR > iC && iV > iR, FString::Printf(TEXT("phase order %s"), *Seq));
			++TestResolved;
		}
	}
}

void UZoogiCardSubsystem::Tick(float Dt)
{
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || !Tune.bCards) return;
	ObservePhase(G);
	if (!TestCase.IsEmpty() && !bTestDone)
	{
		if (TestCase == TEXT("crystal_collapse") && !bCrystalTested && TestStandbys >= 1) TestCrystal(G);
		const int32 Need = TestCase == TEXT("crystal_collapse") ? 1 : 8;
		if (TestResolved >= Need || G->Phase == EZoogiPhase::GameOver) TestFinish(G);
	}
}

// ------------------------------------------------------------------ runtime tests

void UZoogiCardSubsystem::TestCheck(bool bOk, const FString& What)
{
	++TestChecks;
	if (!bOk) { ++TestFails; if (TestFirstFail.IsEmpty()) TestFirstFail = What; UE_LOG(LogZoogi, Warning, TEXT("CARD_TEST check FAIL: %s"), *What); }
}

void UZoogiCardSubsystem::TestOnStandby(AZoogiGameMode* G, int32 P)
{
	if (TestCase.IsEmpty() || bTestDone) return;
	++TestStandbys;
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	const int32 E0 = S->Energy, Sp0 = RulesV.TotalSpent(P), H0 = S->Hand.Num();
	const int32 Avail = S->Hand.Num() + S->DrawPile.Num() + S->Discard.Num();
	TestCheck(E0 == Tune.EnergyPerTurn, FString::Printf(TEXT("P%d energy refresh %d == %d"), P + 1, E0, Tune.EnergyPerTurn));
	TestCheck(H0 == FMath::Min(Tune.HandSize, Avail), FString::Printf(TEXT("P%d hand %d == min(%d,%d)"), P + 1, H0, Tune.HandSize, Avail));
	RulesV.BeginStandby(P, G->TurnCounter); // second call for the same turn: no extra draw / refresh
	TestCheck(S->Hand.Num() == H0 && S->Energy == E0, TEXT("BeginStandby twice draws/refreshes once"));
	// invalid / unaffordable actions spend nothing
	const FZoogiCardContext Ctx = BuildContext(G);
	TestCheck(RulesV.Select(P, -999, Ctx) == EZoogiCardResult::UnknownInstance, TEXT("unknown instance rejected"));
	if (H0 > 0)
	{
		FZoogiCardContext Ko = Ctx; Ko.bActorKnockedOut = true;
		TestCheck(RulesV.Select(P, S->Hand[0].InstanceId, Ko) == EZoogiCardResult::ActorKnockedOut, TEXT("KO actor rejected"));
		FZoogiCardContext NoTargets = Ctx; NoTargets.bHasNeutralOrb = NoTargets.bHasEnemy = NoTargets.bHasCrystalPiece = false;
		for (const FZoogiCardInstance& C : S->Hand)
		{
			const FZoogiCardDef* D = Def(C.CardId);
			if (D && (D->Target == EZoogiCardTarget::NeutralOrb || D->Target == EZoogiCardTarget::EnemyContact || D->Target == EZoogiCardTarget::CrystalPiece))
				TestCheck(RulesV.Select(P, C.InstanceId, NoTargets) == EZoogiCardResult::InvalidTarget, FString::Printf(TEXT("%s without target rejected"), *C.CardId.ToString()));
		}
		bool bSawLimit = false;
		for (const FZoogiCardInstance& C : S->Hand) { const EZoogiCardResult R = RulesV.Select(P, C.InstanceId, Ctx); bSawLimit |= R == EZoogiCardResult::Unaffordable || R == EZoogiCardResult::DuplicateCardType; }
		RulesV.ClearSelection(P);
		UE_LOG(LogZoogi, Log, TEXT("CARD_TEST P%d select-all probe: limit_hit=%d"), P + 1, bSawLimit ? 1 : 0);
	}
	TestCheck(S->Energy == E0 && RulesV.TotalSpent(P) == Sp0 && S->Hand.Num() == H0 && S->Selected.Num() == 0, FString::Printf(TEXT("P%d rejected/cleared selections spent nothing (energy %d)"), P + 1, S->Energy));
	TestCheck(RulesV.Deselect(P, -5) == EZoogiCardResult::NotSelected, TEXT("deselect unknown"));
}

void UZoogiCardSubsystem::TestOnCommit(AZoogiGameMode* G, int32 P, int32 EBefore, int32 SpentBefore, int32 Cost)
{
	if (TestCase.IsEmpty() || bTestDone) return;
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	TestCheck(S->Energy == EBefore - Cost && S->Energy >= 0, FString::Printf(TEXT("P%d commit spends cost once (%d - %d = %d)"), P + 1, EBefore, Cost, S->Energy));
	TestCheck(RulesV.TotalSpent(P) == SpentBefore + Cost, TEXT("total spent += cost"));
	const int32 E1 = S->Energy;
	TestCheck(RulesV.Commit(P, BuildContext(G)) == EZoogiCardResult::AlreadyCommitted && S->Energy == E1 && RulesV.TotalSpent(P) == SpentBefore + Cost, TEXT("second commit spends nothing"));
	if (S->Hand.Num() > 0) TestCheck(RulesV.Select(P, S->Hand[0].InstanceId, BuildContext(G)) == EZoogiCardResult::AlreadyCommitted && S->Energy == E1, TEXT("select after commit rejected"));
}

void UZoogiCardSubsystem::TestCrystal(AZoogiGameMode* G)
{
	bCrystalTested = true;
	AZoogiArenaPiece* Breakable = nullptr; AZoogiArenaPiece* Pillar = nullptr;
	int32 NB = 0, NI = 0;
	for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It)
	{
		if (It->IsIndestructible()) { ++NI; if (!Pillar) Pillar = *It; }
		else if (IsCollapseEligible(*It) && It->GetPieceState() == EZoogiPieceState::Intact) { ++NB; if (!Breakable) Breakable = *It; }
	}
	UE_LOG(LogZoogi, Log, TEXT("CARD_TEST crystal: breakable=%d indestructible=%d bDestruction=%d"), NB, NI, ZoogiUtil::DestructionEnabled() ? 1 : 0);
	TestCheck(!ZoogiUtil::DestructionEnabled(), TEXT("global bDestruction stays False"));
	TestCheck(Breakable != nullptr, TEXT("an eligible breakable piece exists"));
	if (Breakable)
	{
		const FVector At = Breakable->GetActorLocation(), Dir = FVector(1, 0, 0);
		TestCheck(!Breakable->ApplyDamage(5000.f, At, Dir, EZoogiArtifact::None) && Breakable->GetPieceState() == EZoogiPieceState::Intact, TEXT("ordinary damage still blocked by bDestruction=False"));
		TestCheck(Breakable->ApplyCardCollapse(900001, At, Dir) && Breakable->GetPieceState() == EZoogiPieceState::Damaged, TEXT("collapse effect 1: Intact -> Damaged"));
		TestCheck(!Breakable->ApplyCardCollapse(900001, At, Dir) && Breakable->GetPieceState() == EZoogiPieceState::Damaged, TEXT("same effect id applies once"));
		TestCheck(Breakable->ApplyCardCollapse(900002, At, Dir) && Breakable->GetPieceState() == EZoogiPieceState::Cracked, TEXT("effect 2: -> Cracked"));
		TestCheck(Breakable->ApplyCardCollapse(900003, At, Dir) && Breakable->IsBroken(), TEXT("effect 3: -> Broken"));
		TestCheck(!Breakable->ApplyCardCollapse(900004, At, Dir), TEXT("broken piece cannot collapse again"));
		TestCheck(!IsCollapseEligible(Breakable), TEXT("broken piece no longer a legal target"));
	}
	if (Pillar)
	{
		const EZoogiPieceState S0 = Pillar->GetPieceState();
		TestCheck(!Pillar->ApplyCardCollapse(900010, Pillar->GetActorLocation(), FVector(1, 0, 0)) && Pillar->GetPieceState() == S0, TEXT("indestructible pillar unchanged"));
		TestCheck(!IsCollapseEligible(Pillar), TEXT("pillar not a legal target"));
	}
	else UE_LOG(LogZoogi, Log, TEXT("CARD_TEST crystal: no indestructible piece on this arena (pillar check skipped)"));
	// legality through the rules: no eligible piece -> Crystal Collapse rejected without spend
	FZoogiCardContext None = BuildContext(G); None.bHasCrystalPiece = false;
	const int32 P = ActivePlayer(G);
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	if (S && !S->bCommitted) for (const FZoogiCardInstance& C : S->Hand) if (C.CardId == FName(TEXT("crystal_collapse")))
		TestCheck(RulesV.CanSelect(P, C.InstanceId, None) == EZoogiCardResult::InvalidTarget, TEXT("crystal_collapse with no eligible piece rejected"));
}

void UZoogiCardSubsystem::TestFinish(AZoogiGameMode* G)
{
	bTestDone = true;
	int32 Commits = 0, Rejects = 0;
	for (const FZoogiCardEvent& E : RulesV.Events()) { Commits += E.Kind == FName(TEXT("commit")); Rejects += E.Kind == FName(TEXT("reject")); }
	TSet<int64> Ids; bool bUnique = true;
	for (const FZoogiCardEvent& E : RulesV.Events()) { bool bDup = false; Ids.Add(E.EventId, &bDup); bUnique &= !bDup; }
	TestCheck(bUnique, TEXT("ledger event ids unique"));
	TestCheck(TestStandbys > 0, TEXT("at least one standby"));
	UE_LOG(LogZoogi, Log, TEXT("CARD_TEST case=%s result=%s checks=%d fails=%d standbys=%d resolved=%d commits=%d rejects=%d events=%d hash=%08x first_fail=\"%s\""),
		*TestCase, TestFails == 0 ? TEXT("PASS") : TEXT("FAIL"), TestChecks, TestFails, TestStandbys, TestResolved, Commits, Rejects, RulesV.Events().Num(), RulesV.StateHash(), *TestFirstFail);
}
