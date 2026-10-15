#include "ZoogiCardSubsystem.h"
#include "ZoogiIceWall.h"
#include "ZoogiCPUBrain.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void UZoogiCardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Tune.Load();
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiCardTest="), TestCase);
	if (Tune.bCards)
		UE_LOG(LogZoogi, Log, TEXT("CARDS on: hand=%d drawperturn=%d handcap=%d energy=%d deck=%d maxcopies=%d test=%s"), Tune.HandSize, Tune.DrawPerTurn, Tune.HandCap, Tune.EnergyPerTurn, Tune.DeckSize, Tune.MaxCopies, TestCase.IsEmpty() ? TEXT("-") : *TestCase);
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
	// Oct 14: power cards carry their power (Powers.csv card_id) so locked powers keep their cards out of matches.
	{ FString LinkLog; ZoogiCards::LinkRequiredPowersFromData(Defs, LinkLog); UE_LOG(LogZoogi, Log, TEXT("CARDS power links:%s"), *LinkLog); }
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
	TSharedPtr<IZoogiCardBackend> Backend;
	FString Why = TEXT("Backend=native");
	if (!Tune.Backend.Equals(TEXT("native"), ESearchCase::IgnoreCase)) Backend = FZoogiToolkitCardBackend::TryLoad(Defs, Why);
	if (!Backend.IsValid()) { Backend = MakeShared<FZoogiNativeCardBackend>(Defs); UE_LOG(LogZoogi, Log, TEXT("CARDS backend native (%s)"), *Why); }
	FString LogS;
	// Oct 14 (Evan): power unlocks. No registered unlock query (the default) = everything unlocked, decks untouched.
	TArray<FName> Fallback = ZoogiCards::BuiltInDeck();
	// Oct 14: human players use the saved deck (menu Deck screen) when it is legal; CPUs keep the default deck.
	TArray<FName> HumanDeck = Deck;
	if (const ZoogiCards::FSavedDeckQuery& SQ = ZoogiCards::GetSavedDeckQuery())
	{
		const TArray<FName> Saved = SQ();
		FString DeckErr;
		TArray<FName> Use = Saved;
		DeckNotice.Reset();
		if (Use.Num() > 0 && Use.Num() < Tune.DeckSize)
		{
			// Oct 14 (Evan): a short saved deck is refilled to DeckSize from owned (unlocked) cards, with an on-screen notice
			const int32 Was = Use.Num();
			TArray<FName> Added;
			ZoogiCards::RefillDeck(Use, *Backend, Tune.DeckSize, Tune.MaxCopies, ZoogiCards::GetPowerUnlockQuery(), Added);
			TArray<FString> Names; for (const FName& A : Added) { const FZoogiCardDef* D = Backend->FindDef(A); Names.Add(D ? D->Name : A.ToString()); }
			DeckNotice = FString::Printf(TEXT("Your deck had %d/%d cards - added %s from your collection"), Was, Tune.DeckSize, *FString::Join(Names, TEXT(", ")));
			DeckNoticeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			LogS += FString::Printf(TEXT(" saved deck %d/%d refilled to %d [+%s];"), Was, Tune.DeckSize, Use.Num(), *FString::Join(Names, TEXT(",")));
		}
		if (Use.Num() > 0)
		{
			if (ZoogiCards::ValidateDeck(Use, *Backend, Tune.DeckSize, Tune.MaxCopies, DeckErr)) { HumanDeck = Use; LogS += TEXT(" human deck = saved deck;"); }
			else LogS += FString::Printf(TEXT(" saved deck not legal (%s): default deck;"), *DeckErr);
		}
	}
	if (const ZoogiCards::FPowerUnlockQuery& Q = ZoogiCards::GetPowerUnlockQuery())
	{
		Deck = ZoogiCards::FilterDeckByUnlocks(Deck, *Backend, Tune.DeckSize, Tune.MaxCopies, Q, LogS);
		HumanDeck = ZoogiCards::FilterDeckByUnlocks(HumanDeck, *Backend, Tune.DeckSize, Tune.MaxCopies, Q, LogS);
		Fallback = ZoogiCards::FilterDeckByUnlocks(Fallback, *Backend, Tune.DeckSize, Tune.MaxCopies, Q, LogS);
	}
	TArray<TArray<FName>> Decks;
	for (const AZoogiBall* B : G->Balls) Decks.Add(B && !B->bCPU ? HumanDeck : Deck);
	const int32 Seed = G->GameSeed ^ 0x5EEDCA4D; // own stream: card draws never consume GameRng numbers
	const bool bOk = RulesV.Init(Backend, Tune, Seed, Decks, Fallback, LogS);
	LastStandbyTurn = -1;
	FanTune.Load();
	FxReset();
	if (!DeckNotice.IsEmpty()) UE_LOG(LogZoogi, Log, TEXT("CARDS deck notice: %s"), *DeckNotice);
	CollapseEffectId = 0; CollapseOwner = -1; CollapsePiece.Reset(); bCollapseHit = false;
	AZoogiIceWall::DestroyAll(GetWorld(), TEXT("new match"));
	// Oct 14 polish: load every card VFX system + the ice wall mesh now and fill the glow-actor pool (no mid-shot hitches)
	AZoogiFX::Prewarm(GetWorld(), { TEXT("NS_Burst_Purple"), TEXT("NS_Burst_White"), TEXT("NS_Burst_Blue"), TEXT("NS_Burst_Red"), TEXT("NS_Burst_Gold"),
		TEXT("NS_Burst_Green"), TEXT("NS_Burst_Yellow"), TEXT("NS_Explosion"), TEXT("NS_Sparks"), TEXT("NS_Dust") }, WarmAssets, 20);
	if (UObject* IceMesh = AZoogiIceWall::Tune().Mesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *AZoogiIceWall::Tune().Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet)) WarmAssets.AddUnique(IceMesh);
	UE_LOG(LogZoogi, Log, TEXT("CARDS match: backend=%s data=%s defs=%d players=%d seed=%d ok=%d%s"), *Backend->BackendName().ToString(), *DataSource, Defs.Num(), G->Balls.Num(), Seed, bOk ? 1 : 0, *LogS);
}

bool UZoogiCardSubsystem::IsCollapseEligible(const AZoogiArenaPiece* Piece)
{
	return Piece && !Piece->IsIndestructible() && !Piece->IsBroken() && Piece->IsBuilt();
}

AZoogiArenaPiece* UZoogiCardSubsystem::CPUCollapseTarget(const AZoogiBall* Ball) const
{
	if (!Tune.bCards || !Ball || CollapseEffectId == 0 || CollapseOwner != Ball->PlayerIndex || !GetWorld()) return nullptr;
	const FVector2D From(Ball->BallLocation());
	AZoogiArenaPiece* Best = nullptr; float BestD = CPUCollapseRange;
	for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It)
		if (IsCollapseEligible(*It)) { const float D = FVector2D::Distance(From, FVector2D(It->GetActorLocation())); if (D < BestD) { BestD = D; Best = *It; } }
	return Best;
}

// -ZoogiForceCard=<id> (dev / playtest): that card is put in every hand at Standby (replaces the last drawn card if missing).
static FName ZoogiForcedCard()
{
	static const FName N = []() { FString S; return FParse::Value(FCommandLine::Get(), TEXT("ZoogiForceCard="), S) && !S.IsEmpty() ? FName(*S) : NAME_None; }();
	return N;
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
	if (LastStandbyTurn != G->TurnCounter) FxTurnEnd(G);
	FanStandby(G, P); // fantasy: this player's round-long effects (Anchor, Recovery, burning patches) end here
	RulesV.BeginStandby(P, G->TurnCounter);
	if (ZoogiForcedCard() != NAME_None)
	{
		if (Def(ZoogiForcedCard())) { if (RulesV.ForceInHand(P, ZoogiForcedCard())) UE_LOG(LogZoogi, Log, TEXT("CARDS force card %s -> P%d hand (-ZoogiForceCard)"), *ZoogiForcedCard().ToString(), P + 1); }
		else UE_LOG(LogZoogi, Warning, TEXT("CARDS -ZoogiForceCard=%s: unknown card id"), *ZoogiForcedCard().ToString());
	}
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
	const int32 Inst = S->Hand[Slot].InstanceId; const FName CardId = S->Hand[Slot].CardId;
	const EZoogiCardResult R = RulesV.Toggle(P, Inst, BuildContext(G));
	if (R == EZoogiCardResult::Ok && S->Selected.Contains(Inst)) CovSelected.FindOrAdd(CardId)++;
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
		if (K.Card.CardId == FName(TEXT("crystal_collapse"))) { CollapseEffectId = K.EffectId; CollapseOwner = P; CollapsePiece.Reset(); bCollapseHit = false; }
		FxArm(G, P, K.Card.CardId, K.EffectId);
	}
	if (TestCase == TEXT("followup") && !bTestDone)
	{
		// follow-up test: every lead action carries the two setup cards (real contacts decide whether they fire)
		FxArm(G, P, FName(TEXT("bank_strike")), 9100000 + FxForceIdx++);
		FxArm(G, P, FName(TEXT("orb_relay")), 9100000 + FxForceIdx++);
		// TEST FIXTURE (test only, logged): the CPU earns ~10 Combo per action, so a short match rarely reaches the 30 cost.
		// Every other commit the team gauge is topped up to 40 so both the affordable (offer -> continue / pass) and the
		// unaffordable (no offer) paths are exercised. The real game never does this; charging is still checked on accept.
		if ((FxForceIdx / 2) % 2 == 1)
		{
			const int32 T = AZoogiGameMode::TeamOfIndex(P);
			if (G->ComboEnergy[T] < 40.f) { UE_LOG(LogZoogi, Log, TEXT("CARD_TEST followup: TEST FIXTURE team %d gauge %.0f -> 40"), T, G->ComboEnergy[T]); G->ComboEnergy[T] = 40.f; }
		}
	}
	if (TestCase == TEXT("effects") && !bTestDone)
	{
		// effect test: additionally arm one card per turn in a fixed rotation (every card gets exercised in the real roll)
		static const TCHAR* Rot[] = { TEXT("titan"), TEXT("guard"), TEXT("storm"), TEXT("double_impact"), TEXT("heavy_roll"), TEXT("frost"), TEXT("ember"),
			TEXT("momentum_steal"), TEXT("anchor_guard"), TEXT("bank_strike"), TEXT("orb_magnet"), TEXT("orb_relay"), TEXT("precision"), TEXT("recovery_roll") };
		for (int32 k = 0; k < 5; ++k) // five per turn: the whole rotation fits in 3 turns (Phase 4 real follow-ups can end a seed early)
		{
			FxArm(G, P, FName(Rot[FxForceIdx % UE_ARRAY_COUNT(Rot)]), 9000000 + FxForceIdx);
			++FxForceIdx;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("CARDS commit P%d commit=%lld cards=%d cost=%d energy %d -> %d (effects armed)"), P + 1, Out.Num() ? Out[0].CommitId : 0, Out.Num(), Cost, EBefore, S->Energy);
	if (LastPhase != TEXT("COMMIT")) { TurnPhases.Add(TEXT("COMMIT")); LastPhase = TEXT("COMMIT"); }
	TestOnCommit(G, P, EBefore, SpentBefore, Cost);
	return R;
}

static const FName NCrystalCPU(TEXT("crystal_collapse"));

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
	// Oct 14 CPU tiers: Normal keeps the legacy greedy order above; other tiers only change WHICH legal cards are tried
	// (same hand, same Energy, every Select still validated by the rules). Seeded from the match seed + turn (replay-safe).
	const EZoogiCPUTier CPUTier = ZoogiCPU::ActiveTier();
	if (CPUTier != EZoogiCPUTier::Normal)
	{
		TArray<FZoogiCPUCardOption> Opts;
		for (const FZoogiCardInstance& Ci : Hand) { const FZoogiCardDef* D = Def(Ci.CardId); Opts.Add({ Ci.CardId, D ? D->Cost : 99, D ? D->CPUWeight : 0 }); }
		FRandomStream TierRng(HashCombine(GetTypeHash(P), GetTypeHash(S->Turn)));
		Order = ZoogiCPU::PickCards(Opts, S->Energy, ZoogiCPU::TierRow(CPUTier), ZoogiCPU::Combos(), TierRng);
		UE_LOG(LogZoogi, Log, TEXT("CARDS cpu tier=%s P%d tries %d of %d cards"), ZoogiProgress::TierName(CPUTier), P + 1, Order.Num(), Hand.Num());
	}
	const FZoogiCardContext Ctx = BuildContext(G);
	// Oct 14 playtest fix: Crystal Collapse first when a breakable piece is within CPU range (the planner then aims at it).
	{
		const AZoogiBall* Me = G ? G->GetActiveBall() : nullptr;
		bool bPieceNear = false;
		if (Me && GetWorld()) for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It)
			if (IsCollapseEligible(*It) && FVector2D::Distance(FVector2D(Me->BallLocation()), FVector2D(It->GetActorLocation())) < CPUCollapseRange) { bPieceNear = true; break; }
		const int32 CI = Hand.IndexOfByPredicate([](const FZoogiCardInstance& I) { return I.CardId == NCrystalCPU; });
		if (bPieceNear && CI != INDEX_NONE) { Order.Remove(CI); Order.Insert(CI, 0); UE_LOG(LogZoogi, Log, TEXT("CARDS cpu P%d: breakable piece in range -> crystal_collapse first"), P + 1); }
	}
	// -ZoogiForceCard: the forced card is tried first (per-card capture runs)
	if (ZoogiForcedCard() != NAME_None)
	{
		const int32 FI = Hand.IndexOfByPredicate([](const FZoogiCardInstance& I) { return I.CardId == ZoogiForcedCard(); });
		if (FI != INDEX_NONE) { Order.Remove(FI); Order.Insert(FI, 0); }
	}
	for (int32 i : Order) if (RulesV.Select(P, Hand[i].InstanceId, Ctx) == EZoogiCardResult::Ok) CovSelected.FindOrAdd(Hand[i].CardId)++;
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
	CollapsePiece = Piece; CollapseAt = At; CollapseDir = Dir; CollapseFloorZ = Ball->BallLocation().Z - ZoogiConst::ZoogiRadius;
	FanSpikes(G, Ball, At); // fantasy: crystal spikes burst at the contact (pushes Zoogis only)
	CovCond.FindOrAdd(FName(TEXT("crystal_collapse"))).Add(CollapseEffectId);
	UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld qualified contact P%d -> %s (applies at the Resolve boundary)"), CollapseEffectId, CollapseOwner + 1, *Piece->GetName());
}

void UZoogiCardSubsystem::RaiseIceWall(AZoogiGameMode* G, const FVector& At, const FVector& Dir, const TCHAR* Why)
{
	// Oct 14 (Evan 9:33 PM): the collapse raises a permanent wall of ice shards across the hit, perpendicular to the shot line
	const FVector Floor(At.X, At.Y, CollapseFloorZ);
	AZoogiIceWall* W = AZoogiIceWall::SpawnWall(GetWorld(), Floor, Dir, CollapseOwner, CollapseEffectId);
	UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld P%d ice wall %s at (%.0f, %.0f) (%s)"), CollapseEffectId, CollapseOwner + 1, W ? TEXT("raised") : TEXT("blocked"), At.X, At.Y, Why);
	if (W) { FxUses.FindOrAdd(FName(TEXT("crystal_collapse")))++; if (G) G->TakeShot(TEXT("icewall_raised")); }
}

void UZoogiCardSubsystem::ApplyPendingCollapses(AZoogiGameMode* G)
{
	if (CollapseEffectId == 0) return;
	if (!CollapsePiece.IsValid())
	{
		if (bCollapseHit)
		{
			RaiseIceWall(G, CollapseHitAt, CollapseHitDir, TEXT("first contact"));
			CollapseEffectId = 0; CollapseOwner = -1; bCollapseHit = false;
		}
		return;
	}
	RaiseIceWall(G, CollapseAt, CollapseDir, TEXT("breakable piece contact"));
	AZoogiArenaPiece* Piece = CollapsePiece.Get();
	const FString Before = AZoogiArenaPiece::StateName(Piece->GetPieceState());
	const bool bChanged = Piece->ApplyCardCollapse(CollapseEffectId, CollapseAt, CollapseDir);
	if (bChanged) FxUses.FindOrAdd(FName(TEXT("crystal_collapse")))++;
	// Oct 14 power rider (Crystal Collapse equipped): nearby breakable pieces shatter one state too, plus a shrapnel ring
	AZoogiBall* Owner = G && G->Balls.IsValidIndex(CollapseOwner) ? G->Balls[CollapseOwner] : nullptr;
	if (bChanged && Owner && PowerRider(Owner, FName(TEXT("crystal_collapse"))))
	{
		int32 Extra = 0;
		for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It)
		{
			AZoogiArenaPiece* Pc = *It;
			if (Pc == Piece || !IsCollapseEligible(Pc) || FVector::Dist2D(Pc->GetActorLocation(), CollapseAt) > 600.f) continue;
			const FVector Dir = (Pc->GetActorLocation() - CollapseAt).GetSafeNormal2D();
			if (Pc->ApplyCardCollapse(CollapseEffectId + 1000 + Extra, Pc->GetActorLocation(), Dir)) ++Extra;
		}
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bOut || !B->Sphere) continue;
			const FVector D = FVector(B->BallLocation().X - CollapseAt.X, B->BallLocation().Y - CollapseAt.Y, 0.f);
			if (D.Size() < 450.f && D.Size() > 1.f) B->Sphere->SetPhysicsLinearVelocity(B->GetBallVelocity() + D.GetSafeNormal() * 600.f);
		}
		UE_LOG(LogZoogi, Log, TEXT("CARDS power crystal_collapse P%d: %d nearby pieces shattered, shrapnel ring"), CollapseOwner + 1, Extra);
	}
	UE_LOG(LogZoogi, Log, TEXT("CARDS crystal_collapse effect=%lld %s: %s -> %s changed=%d (global bDestruction=%d)"), CollapseEffectId, *Piece->GetName(), *Before,
		*AZoogiArenaPiece::StateName(Piece->GetPieceState()), bChanged ? 1 : 0, ZoogiUtil::DestructionEnabled() ? 1 : 0);
	CollapseEffectId = 0; CollapseOwner = -1; CollapsePiece.Reset(); bCollapseHit = false;
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
		FxLeadActionEnd(G);
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
	FxTick(G, Dt);
	if (TestCase == TEXT("followup") && !bTestDone) TestFollowUpTick(G);
	if ((TestCase == TEXT("hand_ui") || TestCase == TEXT("keys") || TestCase == TEXT("dup_input")) && !bTestDone) { TestHandUiTick(G); return; }
	TickForecast(G, Dt);
	if (!TestCase.IsEmpty() && !bTestDone)
	{
		if (TestCase == TEXT("crystal_collapse") && !bCrystalTested && TestStandbys >= 1) TestCrystal(G);
		if (TestCase == TEXT("ui_probe") && !bTestDone)
		{
			bTestDone = true;
			UClass* C = LoadClass<UObject>(nullptr, TEXT("/Game/RLDB/Widgets/WBP_CardVisual.WBP_CardVisual_C"));
			UE_LOG(LogZoogi, Log, TEXT("UIPROBE class=%s super=%s"), *GetNameSafe(C), C ? *GetNameSafe(C->GetSuperClass()) : TEXT("-"));
			if (C)
			{
				for (TFieldIterator<FProperty> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
					UE_LOG(LogZoogi, Log, TEXT("UIPROBE prop %s : %s flags=%llx"), *It->GetName(), *It->GetCPPType(), (uint64)It->PropertyFlags);
				for (TFieldIterator<UFunction> It(C, EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					FString Ps;
					for (TFieldIterator<FProperty> P(*It); P; ++P) Ps += FString::Printf(TEXT("%s %s%s, "), *P->GetCPPType(), *P->GetName(), P->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm) ? TEXT("(out)") : TEXT(""));
					UE_LOG(LogZoogi, Log, TEXT("UIPROBE func %s(%s) flags=%x"), *It->GetName(), *Ps, (uint32)It->FunctionFlags);
				}
				for (const FImplementedInterface& I : C->Interfaces) UE_LOG(LogZoogi, Log, TEXT("UIPROBE iface %s"), *GetNameSafe(I.Class));
			}
			UE_LOG(LogZoogi, Log, TEXT("CARD_TEST case=ui_probe result=PASS checks=0 fails=0"));
		}
		if (TestCase == TEXT("mutual_ko") && !bMutualTested && TestStandbys >= 1) { TestMutualKO(G); TestFinish(G); return; }
		const int32 Need = TestCase == TEXT("crystal_collapse") ? 1 : TestCase == TEXT("effects") ? 28 : TestCase == TEXT("followup") ? 20 : 8;
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
	if ((TestCase == TEXT("hand_ui") || TestCase == TEXT("keys") || TestCase == TEXT("dup_input")) && !HandUiBall.IsValid()) if (AZoogiBall* Hb = G->GetActiveBall()) { Hb->bCPU = false; HandUiBall = Hb; UE_LOG(LogZoogi, Log, TEXT("CARDS hand_ui: P%d human for this Standby"), Hb->PlayerIndex + 1); }
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	const int32 E0 = S->Energy, Sp0 = RulesV.TotalSpent(P), H0 = S->Hand.Num();
	const int32 Avail = S->Hand.Num() + S->DrawPile.Num() + S->Discard.Num();
	TestCheck(E0 == Tune.EnergyPerTurn, FString::Printf(TEXT("P%d energy refresh %d == %d"), P + 1, E0, Tune.EnergyPerTurn));
	TestCheck(H0 == S->HandBeforeDraw + FMath::Min(S->LastDrawWant, Avail - S->HandBeforeDraw), FString::Printf(TEXT("P%d hand %d == %d + min(%d,%d)"), P + 1, H0, S->HandBeforeDraw, S->LastDrawWant, Avail - S->HandBeforeDraw));
	TestCheck(S->Turn != G->TurnCounter || S->LastDrawWant <= FMath::Max(Tune.HandSize, Tune.DrawPerTurn), FString::Printf(TEXT("P%d per-turn draw %d"), P + 1, S->LastDrawWant));
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
	FString Uses;
	if (TestCase == TEXT("followup"))
	{
		TestCheck(FuOffers > 0, FString::Printf(TEXT("at least one follow-up offer (%d)"), FuOffers));
		TestCheck(FuAccepts > 0 && FuDeclines > 0, FString::Printf(TEXT("continue and pass both exercised (accept %d, pass %d)"), FuAccepts, FuDeclines));
		Uses = FString::Printf(TEXT("offers:%d,accepts:%d,passes:%d,card_setups:%d,refused:%d,"), FuOffers, FuAccepts, FuDeclines, FxCardSetups, FxSetupRefused);
	}
	if (TestCase == TEXT("effects"))
	{
		TestCheck(FxTitanRestoreOk, TEXT("titan mass restored to the actual previous value"));
		TestCheck(FxNoHealOk, TEXT("no card heals Ring Marks (recovery_roll)"));
		TestCheck(FxMaxBursts <= 2, FString::Printf(TEXT("double_impact bursts per contact %d <= 2"), FxMaxBursts));
		TestCheck(FxMagnetMoved <= FxMagnetMax + 15.f, FString::Printf(TEXT("orb_magnet displacement %.0f <= bound %.0f (pull-attributable, +1 frame)"), FxMagnetMoved, FxMagnetMax));
		TestCheck(FxForceIdx >= 14, FString::Printf(TEXT("every effect card armed at least once (%d)"), FxForceIdx));
		TArray<FName> Keys; FxUses.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
		for (const FName& K : Keys) Uses += FString::Printf(TEXT("%s:%d,"), *K.ToString(), FxUses[K]);
	}
	UE_LOG(LogZoogi, Log, TEXT("CARD_TEST case=%s result=%s checks=%d fails=%d standbys=%d resolved=%d commits=%d rejects=%d events=%d hash=%08x fx=%08x uses=%s first_fail=\"%s\""),
		*TestCase, TestFails == 0 ? TEXT("PASS") : TEXT("FAIL"), TestChecks, TestFails, TestStandbys, TestResolved, Commits, Rejects, RulesV.Events().Num(), RulesV.StateHash(), Fx.Hash(), Uses.IsEmpty() ? TEXT("-") : *Uses, *TestFirstFail);
}

// ------------------------------------------------------------------ Standby aim forecast (ArrowTrail pack, visual only)
// A Niagara ribbon (NS_ArrowTrail_Basic) runs along the existing aim preview path (first contact + one rebound) while the
// shooter is in Aim / Force; an aim-point marker (NS_ArrowTrail_Magic) sits on the preview end / contact ghost. They read
// FZoogiAimPreview only (Rule Zero: never touch physics) and are hidden outside AIM_FORCE.

void UZoogiCardSubsystem::TickForecast(AZoogiGameMode* G, float Dt)
{
	const bool bShow = G->IsAimStep() && G->Preview.bValid && G->Preview.Path.Num() >= 2 && !bNoForecast;
	auto Make = [this](TObjectPtr<UNiagaraComponent>& C, const TCHAR* Path)
	{
		if (C) return;
		if (UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, Path))
		{
			C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Sys, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.6f), false, false);
			if (C) UE_LOG(LogZoogi, Log, TEXT("CARDS forecast fx %s"), Path);
		}
		else { bNoForecast = true; UE_LOG(LogZoogi, Warning, TEXT("CARDS forecast fx missing: %s"), Path); }
	};
	if (!bShow)
	{
		if (Trail && Trail->IsActive()) Trail->Deactivate();
		if (AimPoint && AimPoint->IsActive()) AimPoint->Deactivate();
		ForecastS = 0.f;
		return;
	}
	Make(Trail, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Basic.NS_ArrowTrail_Basic"));
	Make(AimPoint, TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Magic.NS_ArrowTrail_Magic"));
	const TArray<FVector>& Path = G->Preview.Path;
	float Len = 0.f;
	for (int32 i = 1; i < Path.Num(); ++i) Len += FVector::Dist(Path[i - 1], Path[i]);
	const FVector Lift(0, 0, 25.f);
	if (AimPoint)
	{
		AimPoint->SetWorldLocation((G->Preview.bContact ? G->Preview.Ghost : Path.Last()) + Lift);
		if (!AimPoint->IsActive()) AimPoint->Activate(true);
	}
	if (!Trail || Len < 1.f) return;
	const bool bWrap = ForecastS <= 0.f || ForecastS > Len;
	ForecastS = bWrap ? 0.f : ForecastS + Dt * 1400.f; // runner speed cm/s (frame-rate independent)
	float S = ForecastS; FVector P = Path[0];
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		const float L = FVector::Dist(Path[i - 1], Path[i]);
		if (S <= L) { P = FMath::Lerp(Path[i - 1], Path[i], L > 0.f ? S / L : 0.f); break; }
		S -= L; P = Path[i];
	}
	Trail->SetWorldLocation(P + Lift);
	if (bWrap) { Trail->ResetSystem(); ForecastS = 0.01f; }
	else if (!Trail->IsActive()) Trail->Activate(true);
}
