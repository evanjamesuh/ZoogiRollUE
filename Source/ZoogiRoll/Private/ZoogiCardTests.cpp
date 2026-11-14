// Automation tests for the v3 card rules (no world needed): Session Frontend / -ExecCmds="Automation RunTests ZoogiRoll.Cards".
#include "ZoogiCards.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ZoogiCardTest
{
	static FZoogiCardTune Tune() { FZoogiCardTune T; T.bCards = true; return T; } // defaults: hand 5, energy 5, deck 15, max 2
	static TSharedPtr<IZoogiCardBackend> Backend() { return MakeShared<FZoogiNativeCardBackend>(ZoogiCards::BuiltInDefs()); }
	static bool Make(FZoogiCardRules& R, int32 Players = 2, int32 Seed = 7)
	{
		TArray<TArray<FName>> D; D.Init(ZoogiCards::BuiltInDeck(), Players);
		FString L;
		return R.Init(Backend(), Tune(), Seed, D, ZoogiCards::BuiltInDeck(), L);
	}
	static int32 CostOf(const FZoogiCardRules& R, const FZoogiCardInstance& C) { return R.Backend()->FindDef(C.CardId)->Cost; }
	static int32 FindByCost(const FZoogiCardRules& R, int32 P, int32 Cost)
	{
		for (const FZoogiCardInstance& C : R.Player(P)->Hand) if (CostOf(R, C) == Cost) return C.InstanceId;
		return -1;
	}
}
using namespace ZoogiCardTest;

constexpr EAutomationTestFlags ZoogiCardFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardDataTest, "ZoogiRoll.Cards.Data", ZoogiCardFlags)
bool FZoogiCardDataTest::RunTest(const FString&)
{
	const TArray<FZoogiCardDef> Defs = ZoogiCards::BuiltInDefs();
	TestEqual(TEXT("15 universal cards"), Defs.Num(), 15);
	TSet<FName> Ids; for (const FZoogiCardDef& D : Defs) Ids.Add(D.Id);
	TestEqual(TEXT("stable unique ids"), Ids.Num(), 15);
	const TCHAR* Book[15] = { TEXT("storm"), TEXT("bank_strike"), TEXT("guard"), TEXT("orb_relay"), TEXT("heavy_roll"), TEXT("ember"), TEXT("titan"), TEXT("frost"),
		TEXT("precision"), TEXT("momentum_steal"), TEXT("crystal_collapse"), TEXT("orb_magnet"), TEXT("double_impact"), TEXT("anchor_guard"), TEXT("recovery_roll") };
	const int32 Cost[15] = { 1, 2, 1, 1, 2, 2, 2, 2, 1, 2, 2, 1, 2, 1, 1 }; // card book p.17-31
	for (int32 i = 0; i < 15; ++i) TestEqual(FString::Printf(TEXT("cost %s"), Book[i]), Defs[i].Cost, Cost[i]);
	// the shipped CSV must match the built-in table exactly (ids, costs, types, targets, expiries)
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Cards.csv");
	if (TestTrue(TEXT("Cards.csv present"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		TArray<FZoogiCardDef> Csv; FString Err;
		TestTrue(FString::Printf(TEXT("Cards.csv parses (%s)"), *Err), ZoogiCards::ParseCardsCsv(Text, Csv, Err));
		if (TestEqual(TEXT("csv rows"), Csv.Num(), 15))
			for (int32 i = 0; i < 15; ++i)
			{
				TestTrue(TEXT("csv id"), Csv[i].Id == Defs[i].Id);
				TestEqual(TEXT("csv cost"), Csv[i].Cost, Defs[i].Cost);
				TestTrue(TEXT("csv type/target/expiry"), Csv[i].Type == Defs[i].Type && Csv[i].Target == Defs[i].Target && Csv[i].Expiry == Defs[i].Expiry);
				TestTrue(TEXT("csv params"), FMath::IsNearlyEqual(Csv[i].ParamA, Defs[i].ParamA) && FMath::IsNearlyEqual(Csv[i].ParamB, Defs[i].ParamB));
			}
	}
	TArray<FZoogiCardDef> Bad; FString Err;
	TestFalse(TEXT("malformed csv rejected"), ZoogiCards::ParseCardsCsv(TEXT("id,name\nstorm,STORM,x\n"), Bad, Err));
	TestFalse(TEXT("duplicate id rejected"), ZoogiCards::ParseCardsCsv(TEXT("h\nstorm,S,1,Infusion,ActingZoogi,TurnEnd,0,0,1,true,r\nstorm,S,1,Infusion,ActingZoogi,TurnEnd,0,0,1,true,r\n"), Bad, Err));
	TestEqual(TEXT("force labels"), FString(ZoogiCards::ForceLabel(3, true)), FString(TEXT("LAUNCH")));
	TestEqual(TEXT("force labels off"), FString(ZoogiCards::ForceLabel(3, false)), FString(TEXT("FULL")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardElementTest, "ZoogiRoll.Cards.Elements", ZoogiCardFlags)
bool FZoogiCardElementTest::RunTest(const FString&)
{
	// Oct 14 (Evan Q4): power cards carry their power's element, generic cards are Neutral; identity (id/cost/type) unchanged.
	TMap<FName, EZoogiCardElement> Want = { { "storm", EZoogiCardElement::Shock }, { "ember", EZoogiCardElement::Fire }, { "titan", EZoogiCardElement::Void },
		{ "frost", EZoogiCardElement::Frost }, { "precision", EZoogiCardElement::Void }, { "crystal_collapse", EZoogiCardElement::Void } };
	const TArray<FZoogiCardDef> Defs = ZoogiCards::BuiltInDefs();
	for (const FZoogiCardDef& D : Defs)
	{
		const EZoogiCardElement* W = Want.Find(D.Id);
		TestEqual(FString::Printf(TEXT("builtin element %s"), *D.Id.ToString()), (int32)D.Element, (int32)(W ? *W : EZoogiCardElement::Neutral));
	}
	FString Text, Err; TArray<FZoogiCardDef> Csv;
	if (TestTrue(TEXT("Cards.csv"), FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/Cards.csv")))) && TestTrue(TEXT("parses"), ZoogiCards::ParseCardsCsv(Text, Csv, Err)) && TestEqual(TEXT("15 rows"), Csv.Num(), 15))
		for (int32 i = 0; i < 15; ++i)
		{
			TestTrue(TEXT("csv element == builtin"), Csv[i].Element == Defs[i].Element);
			TestTrue(TEXT("csv identity"), Csv[i].Id == Defs[i].Id && Csv[i].Cost == Defs[i].Cost && Csv[i].Type == Defs[i].Type);
			TestFalse(TEXT("element not in rule text"), Csv[i].Rule.StartsWith(ZoogiCards::ElementName(Csv[i].Element)));
		}
	TArray<FZoogiCardDef> Old; // legacy CSV without the column still parses, Neutral
	TestTrue(TEXT("legacy csv"), ZoogiCards::ParseCardsCsv(TEXT("h\nember,E,2,Infusion,ActingZoogi,TurnEnd,0,0,1,true,r\n"), Old, Err) && Old.Num() == 1 && Old[0].Element == EZoogiCardElement::Neutral);
	TestFalse(TEXT("bad element rejected"), ZoogiCards::ParseCardsCsv(TEXT("id,name,cost,type,target,expiry,param_a,param_b,cpu_weight,requires_alive,element,rule\nember,E,2,Infusion,ActingZoogi,TurnEnd,0,0,1,true,Lava,r\n"), Old, Err));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardDeckTest, "ZoogiRoll.Cards.DeckRules", ZoogiCardFlags)
bool FZoogiCardDeckTest::RunTest(const FString&)
{
	auto B = Backend(); FString Err;
	TestTrue(TEXT("starter deck legal"), ZoogiCards::ValidateDeck(ZoogiCards::BuiltInDeck(), *B, 15, 2, Err));
	TArray<FName> D = ZoogiCards::BuiltInDeck(); D.Pop();
	TestFalse(TEXT("14 cards rejected"), ZoogiCards::ValidateDeck(D, *B, 15, 2, Err));
	D.Add(TEXT("storm")); // third storm
	TestFalse(TEXT("3 copies rejected"), ZoogiCards::ValidateDeck(D, *B, 15, 2, Err));
	D.Last() = TEXT("not_a_card");
	TestFalse(TEXT("unknown id rejected"), ZoogiCards::ValidateDeck(D, *B, 15, 2, Err));
	// an illegal player deck falls back to the starter deck
	FZoogiCardRules R; FString L;
	TArray<TArray<FName>> Decks = { ZoogiCards::BuiltInDeck(), D };
	TestTrue(TEXT("init with one bad deck"), R.Init(B, Tune(), 1, Decks, ZoogiCards::BuiltInDeck(), L));
	TestEqual(TEXT("fallback deck size"), R.Player(1)->DrawPile.Num(), 15);
	// determinism: same seed same order, different seed different order
	FZoogiCardRules A1, A2, A3; Make(A1, 2, 42); Make(A2, 2, 42); Make(A3, 2, 43);
	TestTrue(TEXT("same seed same state"), A1.StateHash() == A2.StateHash());
	TestTrue(TEXT("different seed differs"), A1.StateHash() != A3.StateHash());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardDrawTest, "ZoogiRoll.Cards.DrawAndEnergyRefresh", ZoogiCardFlags)
bool FZoogiCardDrawTest::RunTest(const FString&)
{
	// Draw rule (Evan, Oct 14, commit 3e2214c): the opening Standby draws HandSize (5); every later Standby draws DrawPerTurn (1)
	// and never above HandCap (7); the unplayed hand is KEPT at turn end, only committed cards go to the discard. Energy refreshes to 5.
	FZoogiCardRules R; TestTrue(TEXT("init"), Make(R));
	const FZoogiCardTune T = Tune();
	TestEqual(TEXT("tune: opening hand 5"), T.HandSize, 5);
	TestEqual(TEXT("tune: draw 1 per turn"), T.DrawPerTurn, 1);
	TestEqual(TEXT("tune: hand cap 7"), T.HandCap, 7);
	const FZoogiCardContext Ctx;
	auto PlayOne = [&](int32 P) -> bool // commit the first selectable card in hand (moves it to Committed -> discard at turn end)
	{
		for (const FZoogiCardInstance& C : R.Player(P)->Hand)
			if (R.Select(P, C.InstanceId, Ctx) == EZoogiCardResult::Ok) return R.Commit(P, Ctx) == EZoogiCardResult::Ok;
		return false;
	};
	R.BeginStandby(0, 1);
	TestEqual(TEXT("opening draw 5"), R.Player(0)->Hand.Num(), 5);
	TestEqual(TEXT("energy 5"), R.Player(0)->Energy, 5);
	TestEqual(TEXT("pile 10"), R.Player(0)->DrawPile.Num(), 10);
	R.BeginStandby(0, 1); // same turn again: apply once
	TestEqual(TEXT("no double draw"), R.Player(0)->Hand.Num(), 5);
	TestEqual(TEXT("P2 untouched"), R.Player(1)->Hand.Num(), 0);
	// spend 1, end turn: energy does not carry over, next standby refreshes to exactly 5
	const int32 One = FindByCost(R, 0, 1);
	int32 Played = 0;
	if (TestTrue(TEXT("opening hand has a 1-cost card"), One >= 0))
	{
		TestTrue(TEXT("select"), R.Select(0, One, Ctx) == EZoogiCardResult::Ok);
		TestTrue(TEXT("commit"), R.Commit(0, Ctx) == EZoogiCardResult::Ok);
		TestEqual(TEXT("energy 4"), R.Player(0)->Energy, 4);
		Played = 1;
	}
	R.BeginStandby(1, 2); // opening P2's turn ends P1's
	TestEqual(TEXT("P1 unplayed hand kept"), R.Player(0)->Hand.Num(), 5 - Played);
	TestEqual(TEXT("P1 discard = committed cards only"), R.Player(0)->Discard.Num(), Played);
	TestEqual(TEXT("P1 energy cleared"), R.Player(0)->Energy, 0);
	TestEqual(TEXT("P2 opening draw 5"), R.Player(1)->Hand.Num(), 5);
	R.BeginStandby(0, 3);
	TestEqual(TEXT("refresh to 5 (not 4+5)"), R.Player(0)->Energy, 5);
	TestEqual(TEXT("later turn draws 1"), R.Player(0)->LastDrawn, 1);
	TestEqual(TEXT("hand = kept + 1"), R.Player(0)->Hand.Num(), 5 - Played + 1);
	TestEqual(TEXT("pile 9"), R.Player(0)->DrawPile.Num(), 9);
	// no plays: +1 per turn up to the cap of 7, then no more draws
	int32 Turn = 4;
	for (int32 k = 0; k < 4; ++k) { R.BeginStandby(1, Turn++); R.BeginStandby(0, Turn++); }
	TestEqual(TEXT("hand capped at 7"), R.Player(0)->Hand.Num(), 7);
	TestEqual(TEXT("no draw at the cap"), R.Player(0)->LastDrawn, 0);
	// playing one card per turn keeps drawing 1; the 15-card pile runs dry and the discard reshuffles in
	bool bReshuffled = false, bCapOk = true, bOnePerTurn = true;
	for (int32 k = 0; k < 30 && !bReshuffled; ++k)
	{
		PlayOne(0);
		R.BeginStandby(1, Turn++); R.BeginStandby(0, Turn++);
		bCapOk &= R.Player(0)->Hand.Num() <= T.HandCap;
		bOnePerTurn &= R.Player(0)->LastDrawn <= T.DrawPerTurn;
		for (const FZoogiCardEvent& E : R.Events()) bReshuffled |= E.Kind == FName(TEXT("reshuffle")) && E.Player == 0;
	}
	TestTrue(TEXT("reshuffle logged"), bReshuffled);
	TestTrue(TEXT("hand never above HandCap"), bCapOk);
	TestTrue(TEXT("never more than DrawPerTurn after the opening hand"), bOnePerTurn);
	const FZoogiCardPlayerState* S = R.Player(0);
	TestEqual(TEXT("15 cards conserved"), S->Hand.Num() + S->DrawPile.Num() + S->Discard.Num() + S->Committed.Num(), 15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardInvalidTest, "ZoogiRoll.Cards.InvalidSpendsNothing", ZoogiCardFlags)
bool FZoogiCardInvalidTest::RunTest(const FString&)
{
	FZoogiCardRules R; Make(R);
	FZoogiCardContext Ctx;
	const int32 Any = 1;
	TestTrue(TEXT("not in standby"), (R.Select(0, Any, Ctx)) == EZoogiCardResult::NotInStandby);
	TestTrue(TEXT("commit outside standby"), (R.Commit(0, Ctx)) == EZoogiCardResult::NotInStandby);
	TestTrue(TEXT("bad player"), (R.Select(9, Any, Ctx)) == EZoogiCardResult::UnknownPlayer);
	R.BeginStandby(0, 1);
	const FZoogiCardPlayerState* S = R.Player(0);
	TestTrue(TEXT("unknown instance"), (R.Select(0, 99999, Ctx)) == EZoogiCardResult::UnknownInstance);
	TestTrue(TEXT("other player's card"), (R.Select(0, R.Player(1)->DrawPile[0].InstanceId, Ctx)) == EZoogiCardResult::UnknownInstance);
	FZoogiCardContext Ko; Ko.bActorKnockedOut = true;
	TestTrue(TEXT("KO actor"), (R.Select(0, S->Hand[0].InstanceId, Ko)) == EZoogiCardResult::ActorKnockedOut);
	// targets: Orb Relay without a neutral orb, Crystal Collapse without an eligible piece
	FZoogiCardContext NoT; NoT.bHasNeutralOrb = false; NoT.bHasCrystalPiece = false; NoT.bHasEnemy = false;
	for (const FZoogiCardInstance& C : S->Hand)
	{
		const FZoogiCardDef* D = R.Backend()->FindDef(C.CardId);
		if (D->Target == EZoogiCardTarget::NeutralOrb || D->Target == EZoogiCardTarget::CrystalPiece || D->Target == EZoogiCardTarget::EnemyContact)
			TestTrue(*FString::Printf(TEXT("%s no target"), *C.CardId.ToString()), (R.Select(0, C.InstanceId, NoT)) == EZoogiCardResult::InvalidTarget);
	}
	// unaffordable: select until the 5 energy run out, the next card is rejected
	Ctx.bHasCrystalPiece = true;
	int32 Sum = 0; bool bLimit = false;
	for (const FZoogiCardInstance& C : S->Hand)
	{
		const EZoogiCardResult X = R.Select(0, C.InstanceId, Ctx);
		if (X == EZoogiCardResult::Ok) Sum += CostOf(R, C);
		else { TestTrue(TEXT("only limit rejections"), X == EZoogiCardResult::Unaffordable || X == EZoogiCardResult::DuplicateCardType); bLimit |= X == EZoogiCardResult::Unaffordable; if (X == EZoogiCardResult::Unaffordable) TestTrue(TEXT("really over budget"), Sum + CostOf(R, C) > 5); }
	}
	TestTrue(TEXT("selection within budget"), Sum <= 5);
	TestEqual(TEXT("selection spends nothing"), S->Energy, 5);
	TestTrue(TEXT("already selected"), (R.Select(0, S->Selected[0], Ctx)) == EZoogiCardResult::AlreadySelected);
	R.ClearSelection(0);
	TestEqual(TEXT("cancel spends nothing"), S->Energy, 5);
	TestEqual(TEXT("cancel keeps hand"), S->Hand.Num(), 5);
	TestEqual(TEXT("nothing spent"), R.TotalSpent(0), 0);
	// commit re-validates: a selection made legal, then the target disappears -> commit rejected, nothing spent
	for (const FZoogiCardInstance& C : S->Hand)
	{
		const FZoogiCardDef* D = R.Backend()->FindDef(C.CardId);
		if (D->Target == EZoogiCardTarget::CrystalPiece || D->Target == EZoogiCardTarget::NeutralOrb)
		{
			TestTrue(TEXT("select ok"), (R.Select(0, C.InstanceId, Ctx)) == EZoogiCardResult::Ok);
			TestTrue(TEXT("commit revalidates"), (R.Commit(0, NoT)) == EZoogiCardResult::InvalidTarget);
			TestEqual(TEXT("failed commit spends nothing"), S->Energy, 5);
			TestFalse(TEXT("not committed"), S->bCommitted);
			R.ClearSelection(0);
			break;
		}
	}
	// duplicate card type in one commit (two copies of storm/guard/heavy roll in hand)
	TMap<FName, int32> Seen;
	for (const FZoogiCardInstance& C : S->Hand)
	{
		if (int32* First = Seen.Find(C.CardId))
		{
			TestTrue(TEXT("first copy"), (R.Select(0, *First, Ctx)) == EZoogiCardResult::Ok);
			TestTrue(TEXT("second copy"), (R.Select(0, C.InstanceId, Ctx)) == EZoogiCardResult::DuplicateCardType);
			R.ClearSelection(0);
			break;
		}
		Seen.Add(C.CardId, C.InstanceId);
	}
	TestEqual(TEXT("still nothing spent"), R.TotalSpent(0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardCommitOnceTest, "ZoogiRoll.Cards.CommitAppliesOnce", ZoogiCardFlags)
bool FZoogiCardCommitOnceTest::RunTest(const FString&)
{
	for (int32 Seed = 1; Seed <= 20; ++Seed)
	{
		FZoogiCardRules R; Make(R, 2, Seed);
		FZoogiCardContext Ctx; Ctx.bHasCrystalPiece = true;
		R.BeginStandby(0, 1);
		const FZoogiCardPlayerState* S = R.Player(0);
		int32 Cost = 0;
		for (const FZoogiCardInstance& C : TArray<FZoogiCardInstance>(S->Hand)) if (R.Select(0, C.InstanceId, Ctx) == EZoogiCardResult::Ok) Cost += CostOf(R, C);
		const int32 NSel = S->Selected.Num();
		TArray<FZoogiCardCommitted> Out;
		TestTrue(TEXT("commit"), (R.Commit(0, Ctx, &Out)) == EZoogiCardResult::Ok);
		TestEqual(TEXT("committed count"), Out.Num(), NSel);
		TestEqual(TEXT("energy = 5 - cost"), S->Energy, 5 - Cost);
		TestEqual(TEXT("spent once"), R.TotalSpent(0), Cost);
		TestEqual(TEXT("hand shrinks"), S->Hand.Num(), 5 - NSel);
		TSet<int64> Eff; for (const FZoogiCardCommitted& K : Out) Eff.Add(K.EffectId);
		TestEqual(TEXT("unique effect ids"), Eff.Num(), Out.Num());
		TestTrue(TEXT("second commit"), (R.Commit(0, Ctx)) == EZoogiCardResult::AlreadyCommitted);
		TestEqual(TEXT("energy unchanged"), S->Energy, 5 - Cost);
		TestEqual(TEXT("spent unchanged"), R.TotalSpent(0), Cost);
		if (S->Hand.Num()) TestTrue(TEXT("select after commit"), (R.Select(0, S->Hand[0].InstanceId, Ctx)) == EZoogiCardResult::AlreadyCommitted);
		int32 Commits = 0; for (const FZoogiCardEvent& E : R.Events()) Commits += E.Kind == FName(TEXT("commit"));
		TestEqual(TEXT("one commit event"), Commits, 1);
		R.EndTurn(0); R.EndTurn(0); // idempotent
		TestEqual(TEXT("all 15 conserved"), S->Hand.Num() + S->DrawPile.Num() + S->Discard.Num(), 15);
	}
	// empty commit (pass) is legal and spends 0
	FZoogiCardRules R; Make(R);
	R.BeginStandby(0, 1);
	TestTrue(TEXT("pass"), (R.Commit(0, FZoogiCardContext())) == EZoogiCardResult::Ok);
	TestEqual(TEXT("pass spends 0"), R.Player(0)->Energy, 5);
	// replay: identical call sequences give identical hashes
	auto Play = [](int32 Seed) { FZoogiCardRules X; Make(X, 4, Seed); FZoogiCardContext C; for (int32 T = 0; T < 12; ++T) { const int32 P = T % 4; X.BeginStandby(P, T + 1); const auto* S = X.Player(P); if (S->Hand.Num()) X.Select(P, S->Hand[0].InstanceId, C); X.Commit(P, C); } return X.StateHash(); };
	TestTrue(TEXT("replay hash"), Play(9) == Play(9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardToolkitTest, "ZoogiRoll.Cards.ToolkitBackend", ZoogiCardFlags)
bool FZoogiCardToolkitTest::RunTest(const FString&)
{
	// The editor-only Aura plugin (unrelated to cards) indexes the project over HTTP after startup. When its login token has expired,
	// its reply lands on the game thread while this test synchronously loads DT_ZoogiCards (several seconds), and the automation
	// framework attributes every Error log emitted while a test is current to that test. Ignore only Aura's auth/indexing lines;
	// every card assertion below still runs and any other error still fails the test.
	AddExpectedMessage(TEXT("^LogAura: .*(Response code: 401|User not authenticated|Authentication required|Failed to get UserId)"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, -1, true);
	FString Why;
	const TArray<FZoogiCardDef> Native = ZoogiCards::BuiltInDefs();
	TSharedPtr<FZoogiToolkitCardBackend> T = FZoogiToolkitCardBackend::TryLoad(Native, Why);
	if (!TestTrue(*FString::Printf(TEXT("toolkit DT_ZoogiCards loads on FCard (%s)"), *Why), T.IsValid())) return true;
	TestEqual(TEXT("15 toolkit rows"), T->AllDefs().Num(), 15);
	for (const FZoogiCardDef& N : Native)
	{
		const FZoogiCardDef* D = T->FindDef(N.Id);
		if (!TestNotNull(*N.Id.ToString(), D)) continue;
		TestEqual(*FString::Printf(TEXT("%s toolkit StatCost == card book"), *N.Id.ToString()), D->Cost, N.Cost);
		TestFalse(TEXT("name read"), D->Name.IsEmpty());
	}
	// the same rules run on either backend: identical decks + seed -> identical state
	FZoogiCardRules A, B; FString L;
	TArray<TArray<FName>> Decks; Decks.Init(ZoogiCards::BuiltInDeck(), 2);
	A.Init(MakeShared<FZoogiNativeCardBackend>(Native), Tune(), 5, Decks, ZoogiCards::BuiltInDeck(), L);
	B.Init(T, Tune(), 5, Decks, ZoogiCards::BuiltInDeck(), L);
	A.BeginStandby(0, 1); B.BeginStandby(0, 1);
	TestTrue(TEXT("backend-independent rules"), A.StateHash() == B.StateHash());
	return true;
}

// Oct 14 (Evan): a card tied to a locked power never enters the deck; the deck is refilled to 15 (max 2) with eligible cards.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardPowerUnlockTest, "ZoogiRoll.Cards.PowerUnlockDeck", ZoogiCardFlags)
bool FZoogiCardPowerUnlockTest::RunTest(const FString&)
{
	TestFalse(TEXT("no unlock query registered by default (everything unlocked)"), (bool)ZoogiCards::GetPowerUnlockQuery());
	TArray<FZoogiCardDef> Defs = ZoogiCards::BuiltInDefs();
	bool bAllEmpty = true; for (const FZoogiCardDef& D : Defs) bAllEmpty &= D.RequiredPower.IsNone();
	TestTrue(TEXT("no card requires a power yet"), bAllEmpty);
	const TArray<FName> Starter = ZoogiCards::BuiltInDeck();
	FString L;
	{
		FZoogiNativeCardBackend B(Defs);
		TestTrue(TEXT("no query: deck unchanged"), ZoogiCards::FilterDeckByUnlocks(Starter, B, 15, 2, ZoogiCards::FPowerUnlockQuery(), L) == Starter);
		TestTrue(TEXT("empty RequiredPower: deck unchanged even with everything locked"), ZoogiCards::FilterDeckByUnlocks(Starter, B, 15, 2, [](FName) { return false; }, L) == Starter);
	}
	const TSet<FName> Gated = { TEXT("titan"), TEXT("ember"), TEXT("precision"), TEXT("crystal_collapse") };
	for (FZoogiCardDef& D : Defs) if (Gated.Contains(D.Id)) D.RequiredPower = FName(*(TEXT("power_") + D.Id.ToString()));
	FZoogiNativeCardBackend B(Defs);
	TestTrue(TEXT("all unlocked: deck unchanged"), ZoogiCards::FilterDeckByUnlocks(Starter, B, 15, 2, [](FName) { return true; }, L) == Starter);
	const TArray<FName> D1 = ZoogiCards::FilterDeckByUnlocks(Starter, B, 15, 2, [](FName P) { return P != FName(TEXT("power_titan")); }, L);
	FString Err;
	TestTrue(TEXT("titan locked: refilled deck legal (15, max 2)"), ZoogiCards::ValidateDeck(D1, B, 15, 2, Err));
	TestFalse(TEXT("titan locked: no titan in deck"), D1.Contains(FName(TEXT("titan"))));
	const TArray<FName> D2 = ZoogiCards::FilterDeckByUnlocks(Starter, B, 15, 2, [](FName) { return false; }, L);
	TestTrue(TEXT("4 powers locked: refilled deck legal"), ZoogiCards::ValidateDeck(D2, B, 15, 2, Err));
	bool bNone = true; for (const FName& Id : D2) bNone &= !Gated.Contains(Id);
	TestTrue(TEXT("4 powers locked: none of their cards in deck"), bNone);
	TestTrue(TEXT("deterministic"), ZoogiCards::FilterDeckByUnlocks(Starter, B, 15, 2, [](FName) { return false; }, L) == D2);
	return true;
}


// Oct 14 (deck-edit): Powers.csv card_id links the power cards; with those powers locked the deck drops them and refills to 15.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardPowerLinkTest, "ZoogiRoll.Cards.PowerLinks", ZoogiCardFlags)
bool FZoogiCardPowerLinkTest::RunTest(const FString&)
{
	TArray<FZoogiCardDef> Defs = ZoogiCards::BuiltInDefs();
	FString L;
	const int32 N = ZoogiCards::LinkRequiredPowers(Defs, TEXT("power_id,name,element,tier,default_unlocked,mastery_level,gem_cost,card_id\nvoid_2_titan_meteor,Titan Meteor,Void,starter,false,5,550,titan\nfire_2_ember,Ember,Fire,starter,false,3,400,ember\nshock_1_storm,Storm,Shock,free,true,0,0,storm\n"), L);
	TestEqual(TEXT("3 cards linked"), N, 3);
	const FZoogiCardDef* T = Defs.FindByPredicate([](const FZoogiCardDef& D) { return D.Id == FName(TEXT("titan")); });
	TestTrue(TEXT("titan -> void_2_titan_meteor"), T && T->RequiredPower == FName(TEXT("void_2_titan_meteor")));
	FZoogiNativeCardBackend B(Defs);
	const TSet<FName> Locked = { TEXT("void_2_titan_meteor"), TEXT("fire_2_ember") };
	const TArray<FName> D = ZoogiCards::FilterDeckByUnlocks(ZoogiCards::BuiltInDeck(), B, 15, 2, [&](FName P) { return !Locked.Contains(P); }, L);
	FString Err;
	TestTrue(TEXT("refilled to a legal 15"), ZoogiCards::ValidateDeck(D, B, 15, 2, Err));
	TestFalse(TEXT("no titan"), D.Contains(FName(TEXT("titan"))));
	TestFalse(TEXT("no ember"), D.Contains(FName(TEXT("ember"))));
	TArray<FZoogiCardDef> Data = ZoogiCards::BuiltInDefs();
	FString L2;
	TestTrue(TEXT("project Powers.csv links the 6 power cards"), ZoogiCards::LinkRequiredPowersFromData(Data, L2) >= 4);
	return true;
}

// Oct 22 (Evan): cards play before launch (aiming) AND after launch (rolling / airborne) until the turn ends; energy applies,
// each card at most once per turn, nothing after the turn ends. Mirrors AZoogiGameMode::CardsTapPlay (select + commit +
// AllowAnotherCommit) behind the same phase gate ZoogiCards::CanPlayInPhase.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardPlayMidRollTest, "ZoogiRoll.Cards.PlayMidRoll", ZoogiCardFlags)
bool FZoogiCardPlayMidRollTest::RunTest(const FString&)
{
	FZoogiCardRules R; if (!TestTrue(TEXT("init"), Make(R))) return false;
	FZoogiCardContext Ctx; Ctx.bHasCrystalPiece = true;
	const int32 Turn = 1;
	R.BeginStandby(0, Turn);
	const FZoogiCardPlayerState* S = R.Player(0);
	auto Gate = [&](bool bPre, bool bRolling, bool bRest, bool bHuman = true)
	{
		return ZoogiCards::CanPlayInPhase(bHuman, S->bStandbyOpen && S->Turn == Turn, bPre, bRolling, bRest);
	};
	auto TapPlay = [&](int32 Inst, bool bPre, bool bRolling, bool bRest) -> EZoogiCardResult
	{
		if (!Gate(bPre, bRolling, bRest)) return EZoogiCardResult::NotInStandby;
		R.AllowAnotherCommit(0); R.ClearSelection(0);
		const EZoogiCardResult X = R.Select(0, Inst, Ctx);
		if (X != EZoogiCardResult::Ok) return X;
		const EZoogiCardResult Y = R.Commit(0, Ctx);
		R.AllowAnotherCommit(0);
		return Y;
	};
	// gate truth table
	TestTrue(TEXT("gate: aiming open"), Gate(true, false, false));
	TestTrue(TEXT("gate: rolling open"), Gate(false, true, false));
	TestFalse(TEXT("gate: at-rest wait closed"), Gate(false, true, true));
	TestFalse(TEXT("gate: resolve / neither closed"), Gate(false, false, false));
	TestFalse(TEXT("gate: opponent / CPU turn closed"), Gate(true, true, false, false));

	// pick two distinct cheap cards (cost 1, different ids) so energy allows both
	int32 PreInst = -1, MidInst = -1; FName PreId, MidId;
	for (const FZoogiCardInstance& C : S->Hand)
	{
		if (CostOf(R, C) != 1) continue;
		if (PreInst < 0) { PreInst = C.InstanceId; PreId = C.CardId; }
		else if (C.CardId != PreId && MidInst < 0) { MidInst = C.InstanceId; MidId = C.CardId; }
	}
	if (MidInst < 0) // fall back: any two distinct ids within 5 energy
		for (const FZoogiCardInstance& C : S->Hand)
			if (C.InstanceId != PreInst && C.CardId != PreId && (PreInst < 0 || CostOf(R, C) <= 4)) { MidInst = C.InstanceId; MidId = C.CardId; break; }
	if (!TestTrue(TEXT("hand has two distinct playable cards"), PreInst >= 0 && MidInst >= 0)) return false;

	// pre-launch play
	const int32 E0 = S->Energy;
	const int32 PreCost = R.Backend()->FindDef(PreId)->Cost, MidCost = R.Backend()->FindDef(MidId)->Cost;
	TestTrue(TEXT("pre-launch play ok"), TapPlay(PreInst, true, false, false) == EZoogiCardResult::Ok);
	TestEqual(TEXT("pre-launch spends energy"), S->Energy, E0 - PreCost);
	// mid-roll play (launch happened: no longer pre-launch, rolling)
	TestTrue(TEXT("mid-roll play ok"), TapPlay(MidInst, false, true, false) == EZoogiCardResult::Ok);
	TestEqual(TEXT("mid-roll spends energy"), S->Energy, E0 - PreCost - MidCost);
	TestEqual(TEXT("total spent"), R.TotalSpent(0), PreCost + MidCost);
	// same card again this turn fails (the instance, and any other copy of that card)
	const int32 E1 = S->Energy;
	TestTrue(TEXT("same instance again fails"), TapPlay(MidInst, false, true, false) != EZoogiCardResult::Ok);
	for (const FZoogiCardInstance& C : S->Hand)
		if (C.CardId == MidId || C.CardId == PreId)
			TestTrue(TEXT("another copy of a played card fails"), TapPlay(C.InstanceId, false, true, false) == EZoogiCardResult::DuplicateCardType);
	TestEqual(TEXT("failed plays spend nothing"), S->Energy, E1);
	// energy still applies mid-roll: an unaffordable card is rejected
	for (const FZoogiCardInstance& C : S->Hand)
		if (CostOf(R, C) > S->Energy && C.CardId != MidId && C.CardId != PreId)
		{ TestTrue(TEXT("mid-roll unaffordable rejected"), TapPlay(C.InstanceId, false, true, false) == EZoogiCardResult::Unaffordable); break; }
	// at-rest wait: closed
	const int32 Any = S->Hand.Num() ? S->Hand[0].InstanceId : -1;
	TestTrue(TEXT("at-rest wait blocks play"), TapPlay(Any, false, true, true) != EZoogiCardResult::Ok);
	// turn end: closed (even if the phase flags would allow it)
	R.EndTurn(0);
	TestTrue(TEXT("after turn end: play fails"), TapPlay(Any, false, true, false) != EZoogiCardResult::Ok);
	TestTrue(TEXT("after turn end: rules reject too"), R.Select(0, Any, Ctx) == EZoogiCardResult::NotInStandby);
	TestEqual(TEXT("nothing spent after turn end"), R.TotalSpent(0), PreCost + MidCost);
	// next own turn: once-per-turn resets
	R.BeginStandby(1, Turn + 1); R.BeginStandby(0, Turn + 2);
	bool bReplay = false;
	for (const FZoogiCardInstance& C : R.Player(0)->Hand)
		if (C.CardId == PreId || C.CardId == MidId) { bReplay = R.CanSelect(0, C.InstanceId, Ctx) != EZoogiCardResult::DuplicateCardType; break; }
		else bReplay = true;
	TestTrue(TEXT("once-per-turn resets at turn start"), bReplay);
	return true;
}
// Oct 22 (Evan): every card's behaviour when played after launch: Immediate (launch-time effect re-applied to the moving
// Zoogi now), Live (contact / tick / settle hooks already run mid-roll) or Blocked (greyed out while rolling). No card is
// silently wasted: every built-in card has an explicit entry, and the speed boost keeps direction and vertical speed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCardMidRollEffectsTest, "ZoogiRoll.Cards.MidRollEffects", ZoogiCardFlags)
bool FZoogiCardMidRollEffectsTest::RunTest(const FString&)
{
	using ZoogiCards::EMidRoll;
	const TMap<FName, EMidRoll> Want = {
		{ TEXT("storm"), EMidRoll::Immediate }, { TEXT("heavy_roll"), EMidRoll::Immediate }, { TEXT("titan"), EMidRoll::Immediate },
		{ TEXT("frost"), EMidRoll::Immediate }, { TEXT("orb_magnet"), EMidRoll::Immediate },
		{ TEXT("bank_strike"), EMidRoll::Live }, { TEXT("guard"), EMidRoll::Live }, { TEXT("orb_relay"), EMidRoll::Live }, { TEXT("ember"), EMidRoll::Live },
		{ TEXT("momentum_steal"), EMidRoll::Live }, { TEXT("crystal_collapse"), EMidRoll::Live }, { TEXT("double_impact"), EMidRoll::Live },
		{ TEXT("anchor_guard"), EMidRoll::Live }, { TEXT("recovery_roll"), EMidRoll::Live },
		{ TEXT("precision"), EMidRoll::Blocked } };
	const TArray<FZoogiCardDef> Defs = ZoogiCards::BuiltInDefs();
	for (const FZoogiCardDef& D : Defs)
	{
		const EMidRoll* W = Want.Find(D.Id);
		if (TestNotNull(*FString::Printf(TEXT("%s has a mid-roll rule"), *D.Id.ToString()), W))
			TestTrue(*FString::Printf(TEXT("%s mid-roll mode"), *D.Id.ToString()), ZoogiCards::MidRollMode(D.Id) == *W);
	}
	TestEqual(TEXT("all 15 cards covered"), Want.Num(), Defs.Num());
	TestTrue(TEXT("unknown card blocked (never wasted)"), ZoogiCards::MidRollMode(TEXT("no_such_card")) == EMidRoll::Blocked);

	// speed boost on the moving Zoogi (Storm / Heavy Roll A)
	const FVector V(300.f, 400.f, -120.f); // 500 cm/s horizontal, falling
	const FVector B = ZoogiCards::MidRollVelocity(V, 1.2f, 10000.f);
	TestTrue(TEXT("boost scales horizontal speed"), FMath::IsNearlyEqual(FVector2D(B).Size(), 600.f, 0.5f));
	TestTrue(TEXT("boost keeps direction"), FVector2D(B).GetSafeNormal().Equals(FVector2D(V).GetSafeNormal(), 1e-3f));
	TestEqual(TEXT("boost keeps vertical speed (airborne)"), B.Z, V.Z);
	const FVector Cp = ZoogiCards::MidRollVelocity(V, 1.5f, 550.f);
	TestTrue(TEXT("boost capped"), FMath::IsNearlyEqual(FVector2D(Cp).Size(), 550.f, 0.5f));
	const FVector Fast = ZoogiCards::MidRollVelocity(FVector(900.f, 0.f, 0.f), 1.2f, 550.f);
	TestTrue(TEXT("cap never slows an already fast roll"), FMath::IsNearlyEqual(Fast.X, 900.f, 0.5f));
	TestTrue(TEXT("resting Zoogi unchanged"), ZoogiCards::MidRollVelocity(FVector(0.2f, 0.f, 0.f), 1.2f, 550.f).Equals(FVector(0.2f, 0.f, 0.f)));

	// blocked cards are unplayable while rolling but fine pre-launch (gate + mode, as CardsSlotPlayableNow)
	auto Playable = [](FName Id, bool bRolling)
	{
		return ZoogiCards::CanPlayInPhase(true, true, !bRolling, bRolling, false) && (!bRolling || ZoogiCards::MidRollMode(Id) != EMidRoll::Blocked);
	};
	TestTrue(TEXT("precision pre-launch ok"), Playable(TEXT("precision"), false));
	TestFalse(TEXT("precision greyed out mid-roll"), Playable(TEXT("precision"), true));
	for (const FZoogiCardDef& D : Defs)
		if (D.Id != FName(TEXT("precision"))) TestTrue(*FString::Printf(TEXT("%s playable mid-roll"), *D.Id.ToString()), Playable(D.Id, true));

	// Orb Magnet: greyed out with no orb in range (play check), refunded if played and nothing is found
	const TArray<FVector> Orbs = { FVector(3000.f, 0.f, 0.f), FVector(0.f, 1400.f, 500.f) };
	TestTrue(TEXT("magnet: orb in range"), ZoogiCards::MagnetHasTarget(FVector::ZeroVector, Orbs, 1500.f));
	TestFalse(TEXT("magnet: no orb in range"), ZoogiCards::MagnetHasTarget(FVector(0.f, -2000.f, 0.f), Orbs, 1500.f));
	TestFalse(TEXT("magnet: no orbs at all"), ZoogiCards::MagnetHasTarget(FVector::ZeroVector, {}, 1500.f));
	{
		FZoogiCardRules R; if (!TestTrue(TEXT("init"), Make(R))) return false;
		R.BeginStandby(0, 1);
		const FZoogiCardPlayerState* S = R.Player(0);
		int32 Inst = -1; for (const FZoogiCardInstance& C : S->Hand) if (CostOf(R, C) <= 5) { Inst = C.InstanceId; break; }
		const FName Id = S->Hand[0].CardId;
		FZoogiCardContext Ctx; Ctx.bHasCrystalPiece = true;
		const int32 E0 = S->Energy, H0 = S->Hand.Num();
		TestTrue(TEXT("select"), R.Select(0, Inst, Ctx) == EZoogiCardResult::Ok);
		TArray<FZoogiCardCommitted> Out;
		TestTrue(TEXT("commit"), R.Commit(0, Ctx, &Out) == EZoogiCardResult::Ok && Out.Num() == 1);
		R.AllowAnotherCommit(0);
		TestTrue(TEXT("energy spent"), S->Energy < E0);
		TestTrue(TEXT("refund ok"), R.RefundCommitted(0, Out[0].EffectId));
		TestEqual(TEXT("refund restores energy"), S->Energy, E0);
		TestEqual(TEXT("refund puts the card back in hand"), S->Hand.Num(), H0);
		TestEqual(TEXT("refund: nothing counted as spent"), R.TotalSpent(0), 0);
		TestEqual(TEXT("refund: not played this turn"), S->Committed.Num(), 0);
		TestTrue(TEXT("refunded card playable again this turn"), R.CanSelect(0, Inst, Ctx) == EZoogiCardResult::Ok);
		TestFalse(TEXT("double refund rejected"), R.RefundCommitted(0, Out[0].EffectId));
		(void)Id;
	}
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS (Shipping build: the Oct 14 deck-edit tests were outside the guard)
