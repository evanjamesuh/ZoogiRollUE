// Automation tests for the v1.0 progression core (no world): -ExecCmds="Automation RunTests ZoogiRoll.Progression+ZoogiRoll.CPU+ZoogiRoll.Store"
#include "ZoogiProgression.h"
#include "ZoogiCPUBrain.h"
#include "ZoogiStore.h"
#include "Misc/AutomationTest.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ZoogiProgTest
{
	constexpr EAutomationTestFlags ZoogiProgTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	static FZoogiProgressionConfig Cfg()
	{
		FZoogiProgressionConfig C = FZoogiProgressionConfig::BuiltIn(); FString L;
		C.LoadFromDir(FPaths::ProjectContentDir() / TEXT("Data/Progression"), L);
		return C;
	}
	static FDateTime At(int32 D, int32 H, int32 M = 0) { return FDateTime(2026, 10, D, H, M, 0); }
	static FGuid Id(const TCHAR* K) { return ZoogiProgress::DeterministicId(K); }
	/** Automation-only store (NOT a payment method): approves receipts whose payload is "ok", rejects anything else. */
	class FTestStore : public IZoogiStoreBackend
	{
	public:
		int32 Finalized = 0; int32 Verifies = 0; bool bPending = false;
		virtual FString StoreName() const override { return TEXT("test"); }
		virtual bool IsAvailable(FString& W) const override { W = TEXT("automation test backend"); return true; }
		virtual void QueryProducts(const TArray<FString>& Ids, TFunction<void(bool, const TArray<FZoogiStoreProduct>&)> Done) override { TArray<FZoogiStoreProduct> O; for (const FString& I : Ids) O.Add({ I, FName(*I), I, TEXT("$TEST") }); Done(true, O); }
		virtual void Purchase(const FString& P, TFunction<void(bool, const FZoogiStoreReceipt&, const FString&)> Done) override { Done(true, { TEXT("test"), FGuid::NewGuid().ToString(), P, TEXT("ok"), true }, FString()); }
		virtual EZoogiVerifyResult Verify(const FZoogiStoreReceipt& R, FString& W) override { ++Verifies; if (bPending) return EZoogiVerifyResult::Pending; W = R.Payload; return R.Payload == TEXT("ok") ? EZoogiVerifyResult::Valid : EZoogiVerifyResult::Invalid; }
		virtual void Finalize(const FZoogiStoreReceipt&) override { ++Finalized; }
		virtual void QueryUnfinished(TFunction<void(const TArray<FZoogiStoreReceipt>&)> Done) override { Done({}); }
	};
}
using namespace ZoogiProgTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgConfigTest, "ZoogiRoll.Progression.Config", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgConfigTest::RunTest(const FString&)
{
	FZoogiProgressionConfig C = FZoogiProgressionConfig::BuiltIn(); FString L, E;
	TestTrue(TEXT("all CSVs load: ") + L, C.LoadFromDir(FPaths::ProjectContentDir() / TEXT("Data/Progression"), L));
	TestTrue(TEXT("valid: ") + E, C.Validate(E));
	TestEqual(TEXT("cap 500"), C.DailyMatchGemCap, 500);
	TestEqual(TEXT("reset 04:00"), C.DailyResetHourLocal, 4);
	TestEqual(TEXT("10 mastery levels"), C.MaxLevel(), 10);
	TestEqual(TEXT("7 titles"), C.Titles.Num(), 7);
	TestEqual(TEXT("4 tiers"), C.Tiers.Num(), 4);
	TestTrue(TEXT("gem packages"), C.GemPackages.Num() >= 1);
	TestNull(TEXT("awakening pays nothing (no reward row)"), C.FindReward(TEXT("awakening")));
	int32 Starter = 0; for (const FZoogiPowerRow& P : C.Powers) if (P.Tier == FName(TEXT("starter"))) ++Starter;
	TestEqual(TEXT("4 starter powers"), Starter, 4);
	// advanced powers rule (Evan Q12): mastery 6-7 OR 400-600 gems
	FZoogiProgressionConfig A = C; A.Powers.Add({ TEXT("x_adv"), TEXT("X"), EZoogiCardElement::Fire, TEXT("advanced"), false, 6, 500, NAME_None });
	TestTrue(TEXT("advanced 6/500 ok"), A.Validate(E));
	A.Powers.Last().MasteryLevel = 8; TestFalse(TEXT("advanced mastery 8 rejected"), A.Validate(E));
	A.Powers.Last().MasteryLevel = 7; A.Powers.Last().GemCost = 700; TestFalse(TEXT("advanced 700 gems rejected"), A.Validate(E));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgCurrencyTest, "ZoogiRoll.Progression.Currency", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgCurrencyTest::RunTest(const FString&)
{
	FZoogiWallet W; const FDateTime T = At(14, 12);
	TestTrue(TEXT("grant earned"), W.Grant(Id(TEXT("a")), EZoogiGemBucket::Earned, 100, TEXT("t"), T) == EZoogiTxnResult::Applied);
	TestTrue(TEXT("grant purchased"), W.Grant(Id(TEXT("b")), EZoogiGemBucket::Purchased, 50, TEXT("t"), T) == EZoogiTxnResult::Applied);
	TestTrue(TEXT("same txn id = duplicate"), W.Grant(Id(TEXT("a")), EZoogiGemBucket::Earned, 100, TEXT("t"), T) == EZoogiTxnResult::Duplicate);
	TestEqual(TEXT("no double grant"), W.Total(), 150);
	TestTrue(TEXT("zero / negative invalid"), W.Grant(Id(TEXT("c")), EZoogiGemBucket::Earned, 0, TEXT("t"), T) == EZoogiTxnResult::Invalid && W.Grant(Id(TEXT("d")), EZoogiGemBucket::Earned, -5, TEXT("t"), T) == EZoogiTxnResult::Invalid);
	TestTrue(TEXT("overspend refused"), W.Spend(Id(TEXT("s0")), 151, TEXT("x"), T) == EZoogiTxnResult::Insufficient);
	TestEqual(TEXT("balance unchanged"), W.Total(), 150);
	TestTrue(TEXT("spend earned first"), W.Spend(Id(TEXT("s1")), 120, TEXT("x"), T) == EZoogiTxnResult::Applied);
	TestEqual(TEXT("earned drained"), W.Earned(), 0); TestEqual(TEXT("purchased 30"), W.Purchased(), 30);
	TestTrue(TEXT("spend replay = duplicate"), W.Spend(Id(TEXT("s1")), 120, TEXT("x"), T) == EZoogiTxnResult::Duplicate);
	FZoogiWallet P; P.SpendOrder = EZoogiSpendOrder::PurchasedFirst;
	P.Grant(Id(TEXT("a")), EZoogiGemBucket::Earned, 100, TEXT("t"), T); P.Grant(Id(TEXT("b")), EZoogiGemBucket::Purchased, 50, TEXT("t"), T);
	P.Spend(Id(TEXT("s")), 60, TEXT("x"), T);
	TestTrue(TEXT("configurable: purchased first"), P.Purchased() == 0 && P.Earned() == 90);
	FString Err; TestTrue(TEXT("ledger replays: ") + Err, W.Verify(Err));
	TSet<FGuid> U; for (const FZoogiGemTxn& X : W.Ledger()) U.Add(X.TxnId);
	TestEqual(TEXT("ledger ids unique"), U.Num(), W.Ledger().Num());
	TestEqual(TEXT("ledger has 3 applied txns"), W.Ledger().Num(), 3);
	TestTrue(TEXT("per-record timestamps"), W.Ledger()[0].Utc == T);
	FZoogiWallet C; C.EarnedCap = 100; int32 Applied = 0;
	C.Grant(Id(TEXT("x")), EZoogiGemBucket::Earned, 150, TEXT("t"), T, FString(), &Applied);
	C.Grant(Id(TEXT("y")), EZoogiGemBucket::Purchased, 150, TEXT("t"), T);
	TestTrue(TEXT("earned cap clips earned only"), Applied == 100 && C.Purchased() == 150);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgPersistTest, "ZoogiRoll.Progression.Persistence", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgPersistTest::RunTest(const FString&)
{
	FZoogiProgression A; A.Init(Cfg()); const FDateTime T = At(14, 12);
	A.GrantGems(Id(TEXT("g1")), EZoogiGemBucket::Earned, 70, TEXT("t"), T);
	A.GrantGems(Id(TEXT("g2")), EZoogiGemBucket::Purchased, 550, TEXT("store.test"), T, TEXT("test:1"));
	A.SpendGems(Id(TEXT("s1")), 100, TEXT("x"), T);
	A.AddMasteryXP(EZoogiCardElement::Fire, 500, T);
	FString Why; A.SetFocusElement(EZoogiCardElement::Frost, EZoogiPlayContext::Standby, T, Why);
	A.Mutable().Affinity.FindOrAdd(TEXT("blue")).XP = 120;
	const FString J = A.Save();
	FZoogiProgression B; FString Err;
	TestTrue(TEXT("reload: ") + Err, B.Load(Cfg(), J, Err));
	TestEqual(TEXT("schema"), B.Get().SchemaVersion, FZoogiProfile::CurrentSchema);
	TestTrue(TEXT("profile guid kept"), B.Get().ProfileId == A.Get().ProfileId && B.Get().ProfileId.IsValid());
	TestEqual(TEXT("earned"), B.Get().Wallet.Earned(), A.Get().Wallet.Earned());
	TestEqual(TEXT("purchased"), B.Get().Wallet.Purchased(), A.Get().Wallet.Purchased());
	TestEqual(TEXT("ledger"), B.Get().Wallet.Ledger().Num(), A.Get().Wallet.Ledger().Num());
	TestTrue(TEXT("duplicate after reload still blocked"), B.GrantGems(Id(TEXT("g2")), EZoogiGemBucket::Purchased, 550, TEXT("store.test"), T) == EZoogiTxnResult::Duplicate);
	TestEqual(TEXT("fire xp"), B.MasteryXP(EZoogiCardElement::Fire), 500);
	TestTrue(TEXT("focus"), B.Focus() == EZoogiCardElement::Frost);
	TestEqual(TEXT("affinity"), B.Get().Affinity.FindRef(TEXT("blue")).XP, 120);
	FString Bad = J.Replace(TEXT("\"earned\":"), TEXT("\"earned\":9")); // tamper with the balance
	FZoogiProgression C;
	TestFalse(TEXT("tampered balance rejected"), C.Load(Cfg(), Bad, Err));
	TestFalse(TEXT("future schema rejected"), C.Load(Cfg(), J.Replace(TEXT("\"schema\":1"), TEXT("\"schema\":99")), Err));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgMasteryTest, "ZoogiRoll.Progression.Mastery", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgMasteryTest::RunTest(const FString&)
{
	FZoogiProgression P; P.Init(Cfg()); const FZoogiProgressionConfig& C = P.Config(); const FDateTime T = At(14, 12);
	TestEqual(TEXT("starts L1"), P.MasteryLevel(EZoogiCardElement::Void), 1);
	TArray<FName> Ups; int32 Gems = 0;
	const int32 Gained = P.AddMasteryXP(EZoogiCardElement::Void, C.MasteryCumulativeXP[3], T, &Ups, &Gems);
	TestEqual(TEXT("to L4"), P.MasteryLevel(EZoogiCardElement::Void), 4);
	TestEqual(TEXT("3 levels"), Gained, 3);
	TestEqual(TEXT("level gems"), Gems, C.MasteryLevelGems[1] + C.MasteryLevelGems[2] + C.MasteryLevelGems[3]);
	TestEqual(TEXT("other elements independent"), P.MasteryLevel(EZoogiCardElement::Fire), 1);
	P.AddMasteryXP(EZoogiCardElement::Void, 10000000, T);
	TestEqual(TEXT("caps at 10"), P.MasteryLevel(EZoogiCardElement::Void), 10);
	// focus: neutral XP goes to focus; changes only in Standby
	FString Why;
	TestFalse(TEXT("no change in Battle"), P.SetFocusElement(EZoogiCardElement::Fire, EZoogiPlayContext::Battle, T, Why));
	TestFalse(TEXT("no change in Menu"), P.SetFocusElement(EZoogiCardElement::Fire, EZoogiPlayContext::Menu, T, Why));
	TestFalse(TEXT("Neutral not a focus"), P.SetFocusElement(EZoogiCardElement::Neutral, EZoogiPlayContext::Standby, T, Why));
	TestTrue(TEXT("Standby ok"), P.SetFocusElement(EZoogiCardElement::Fire, EZoogiPlayContext::Standby, T, Why));
	P.AddMasteryXP(EZoogiCardElement::Neutral, 40, T);
	TestEqual(TEXT("neutral -> focus (Fire)"), P.MasteryXP(EZoogiCardElement::Fire), 40);
	// hooks resolve at event time: not retroactive
	FZoogiMatchTally Tally; Tally.MatchId = FGuid::NewGuid();
	P.HookKO(Tally, EZoogiCardElement::Neutral);                    // Fire (focus now)
	P.SetFocusElement(EZoogiCardElement::Shock, EZoogiPlayContext::Standby, T, Why);
	P.HookKO(Tally, EZoogiCardElement::Neutral);                    // Shock
	P.HookPowerUsed(Tally, EZoogiCardElement::Frost);               // element of the power
	P.HookTrickShot(Tally, TEXT("Trick.FrostLane"), EZoogiCardElement::Frost);
	P.HookTrickShot(Tally, TEXT("Trick.BankStrike"), EZoogiCardElement::Neutral);
	P.HookOrbCollected(Tally); P.HookEpisodeTriggered(Tally); P.HookRingOut(Tally, EZoogiCardElement::Void);
	P.HookAwakening(Tally);
	TestTrue(TEXT("KO1 -> Fire"), Tally.Events[0].XPElement == EZoogiCardElement::Fire);
	TestTrue(TEXT("KO2 -> Shock"), Tally.Events[1].XPElement == EZoogiCardElement::Shock);
	TestTrue(TEXT("power -> Frost"), Tally.Events[2].XPElement == EZoogiCardElement::Frost);
	TestTrue(TEXT("frost lane -> Frost"), Tally.Events[3].XPElement == EZoogiCardElement::Frost);
	TestTrue(TEXT("bank strike -> focus"), Tally.Events[4].XPElement == EZoogiCardElement::Shock);
	TestEqual(TEXT("awakening is not an award event"), Tally.Events.Num(), 8); // 2 KO + power + 2 tricks + orb + episode + ring-out
	TestEqual(TEXT("awakening counted"), Tally.Awakenings, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgAwakeningTest, "ZoogiRoll.Progression.AwakeningAchievementsOnly", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgAwakeningTest::RunTest(const FString&)
{
	FZoogiProgression A, B; A.Init(Cfg()); B.Init(Cfg());
	FZoogiMatchTally T1; T1.MatchId = Id(TEXT("m1")); FZoogiMatchTally T2 = T1; T2.Awakenings = 3;
	const FZoogiMatchRewardResult R1 = A.ApplyMatch(T1, EZoogiMatchOutcome::Loss, At(14, 12), At(14, 7));
	const FZoogiMatchRewardResult R2 = B.ApplyMatch(T2, EZoogiMatchOutcome::Loss, At(14, 12), At(14, 7));
	TestEqual(TEXT("same match gems with/without Awakening"), R2.GemsPaid, R1.GemsPaid);
	int32 X1 = 0, X2 = 0; for (int32 i = 0; i < 5; ++i) { X1 += R1.XP[i]; X2 += R2.XP[i]; }
	TestEqual(TEXT("same XP"), X2, X1);
	TestEqual(TEXT("awakenings stat"), B.Stat(TEXT("awakenings")), 3);
	TestTrue(TEXT("A_AWAKEN_1 achieved"), B.HasAchievement(TEXT("A_AWAKEN_1")));
	TestEqual(TEXT("awakening achievement pays 0 gems"), R2.AchievementGems, R1.AchievementGems);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgMatchTest, "ZoogiRoll.Progression.MatchRewards", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgMatchTest::RunTest(const FString&)
{
	const FZoogiProgressionConfig C = Cfg();
	auto Win = [&](EZoogiCPUTier Tier) { FZoogiProgression P; P.Init(C); FZoogiMatchTally T; T.MatchId = FGuid::NewGuid(); T.Tier = Tier;
		for (int32 i = 0; i < 10; ++i) P.HookKO(T, EZoogiCardElement::Void); // capped at per_match_cap
		return P.ApplyMatch(T, EZoogiMatchOutcome::Win, At(14, 12), At(14, 7)); };
	const FZoogiMatchRewardResult N = Win(EZoogiCPUTier::Normal), E = Win(EZoogiCPUTier::Easy), H = Win(EZoogiCPUTier::Hard), X = Win(EZoogiCPUTier::Expert);
	const int32 KoCap = C.FindReward(TEXT("ko"))->PerMatchCap;
	const int32 Base = C.FindReward(TEXT("completion"))->Gems + C.FindReward(TEXT("win"))->Gems + KoCap * C.FindReward(TEXT("ko"))->Gems;
	TestEqual(TEXT("normal base (KO capped)"), N.BaseGems, Base);
	TestEqual(TEXT("normal paid"), N.GemsPaid, Base);
	TestEqual(TEXT("easy x0.5"), E.GemsPaid, FMath::RoundToInt(Base * C.Tier(EZoogiCPUTier::Easy).GemMult));
	TestEqual(TEXT("hard x1.3"), H.GemsPaid, FMath::RoundToInt(Base * C.Tier(EZoogiCPUTier::Hard).GemMult));
	TestEqual(TEXT("expert x1.6"), X.GemsPaid, FMath::RoundToInt(Base * C.Tier(EZoogiCPUTier::Expert).GemMult));
	TestTrue(TEXT("Void XP from KOs"), N.XP[(int32)EZoogiCardElement::Void] > 0);
	// idempotent by MatchId
	FZoogiProgression P; P.Init(C); FZoogiMatchTally T; T.MatchId = Id(TEXT("same"));
	const FZoogiMatchRewardResult A = P.ApplyMatch(T, EZoogiMatchOutcome::Win, At(14, 12), At(14, 7));
	const int32 Bal = P.Get().Wallet.Total();
	const FZoogiMatchRewardResult B = P.ApplyMatch(T, EZoogiMatchOutcome::Win, At(14, 12), At(14, 7));
	TestTrue(TEXT("second apply = duplicate"), A.bApplied && B.bDuplicate && !B.bApplied);
	TestEqual(TEXT("balance unchanged on replay"), P.Get().Wallet.Total(), Bal);
	// forfeit / abandoned / idle: zero
	auto Zero = [&](EZoogiMatchOutcome O, int32 Turns, int32 Idle) { FZoogiProgression Q; Q.Init(C); FZoogiMatchTally Z; Z.MatchId = FGuid::NewGuid(); Z.Turns = Turns; Z.IdleTurns = Idle; Q.HookKO(Z, EZoogiCardElement::Void);
		const FZoogiMatchRewardResult R = Q.ApplyMatch(Z, O, At(14, 12), At(14, 7)); return R.GemsPaid == 0 && R.XP[1] == 0 && Q.Get().Wallet.Total() == 0 && !R.ZeroReason.IsEmpty(); };
	TestTrue(TEXT("forfeit = 0"), Zero(EZoogiMatchOutcome::Forfeit, 10, 0));
	TestTrue(TEXT("abandoned = 0"), Zero(EZoogiMatchOutcome::Abandoned, 10, 0));
	TestTrue(TEXT("idle >= 50% = 0"), Zero(EZoogiMatchOutcome::Win, 10, 5));
	TestFalse(TEXT("idle 40% pays"), Zero(EZoogiMatchOutcome::Win, 10, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgDailyTest, "ZoogiRoll.Progression.DailyCapAndReset", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgDailyTest::RunTest(const FString&)
{
	FZoogiProgression P; P.Init(Cfg()); const int32 Cap = P.Config().DailyMatchGemCap;
	auto Play = [&](const FDateTime& Utc, const FDateTime& Local)
	{
		FZoogiMatchTally T; T.MatchId = FGuid::NewGuid(); T.Tier = EZoogiCPUTier::Expert;
		for (int32 i = 0; i < 4; ++i) { P.HookKO(T, EZoogiCardElement::Void); P.HookOrbCollected(T); }
		return P.ApplyMatch(T, EZoogiMatchOutcome::Win, Utc, Local);
	};
	int32 Paid = 0, XPTotal = 0; FZoogiMatchRewardResult R;
	for (int32 i = 0; i < 200; ++i) { R = Play(At(14, 15, i % 60), At(14, 10, i % 60)); Paid += R.GemsPaid; XPTotal += R.XP[1]; }
	TestEqual(TEXT("match gems capped at 500/day"), Paid, Cap);
	TestEqual(TEXT("earned today"), R.EarnedToday, Cap);
	TestEqual(TEXT("remaining 0"), R.RemainingToday, 0);
	TestTrue(TEXT("XP continues past the cap"), R.XP[1] > 0);
	// achievements are not capped
	const int32 Before = P.Get().Wallet.Total();
	P.AddStat(TEXT("tricks"), 25); int32 AG = 0; P.EvaluateAchievements(At(14, 15), &AG);
	TestTrue(TEXT("achievement gems ignore cap"), AG > 0 && P.Get().Wallet.Total() == Before + AG);
	// 03:59 local next day: same day; 04:00: reset
	R = Play(At(15, 8, 59), At(15, 3, 59)); TestEqual(TEXT("03:59 still capped"), R.GemsPaid, 0);
	R = Play(At(15, 9, 0), At(15, 4, 0)); TestTrue(TEXT("04:00 reset pays again"), R.GemsPaid > 0);
	TestTrue(TEXT("reset boundary saved"), P.Get().Daily.DayStartLocal == At(15, 4));
	// clock rollback: device clock moved back a day -> no reset, counters kept
	const int32 Today = P.EarnedToday();
	P.CheckDailyReset(At(14, 9), At(16, 5)); // UTC far behind LastSeenUtc (tampered local time says "tomorrow")
	TestTrue(TEXT("rollback detected"), P.Get().Daily.bRollbackDetected);
	TestEqual(TEXT("no reset on rollback"), P.EarnedToday(), Today);
	P.CheckDailyReset(At(16, 10), At(16, 5)); // real time passes again
	TestFalse(TEXT("rollback cleared"), P.Get().Daily.bRollbackDetected);
	TestEqual(TEXT("normal reset resumes"), P.EarnedToday(), 0);
	TestTrue(TEXT("next reset 04:00 tomorrow"), P.NextResetLocal() == At(17, 4));
	TestTrue(TEXT("boundary math"), ZoogiProgress::ResetBoundary(At(20, 3, 59), 4) == At(19, 4) && ZoogiProgress::ResetBoundary(At(20, 4), 4) == At(20, 4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgPowersTest, "ZoogiRoll.Progression.PowerUnlocks", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgPowersTest::RunTest(const FString&)
{
	FZoogiProgression P; P.Init(Cfg()); const FDateTime T = At(14, 12); FString Why; bool bM = false;
	TestTrue(TEXT("storm free"), P.IsPowerUnlocked(TEXT("shock_1_storm")));
	TestFalse(TEXT("ember locked"), P.IsPowerUnlocked(TEXT("fire_2_ember")));
	TestFalse(TEXT("cannot unlock broke"), P.UnlockPower(TEXT("fire_2_ember"), T, Why));
	const FZoogiPowerRow* Ember = P.Config().FindPower(TEXT("fire_2_ember"));
	P.GrantGems(Id(TEXT("g")), EZoogiGemBucket::Earned, Ember->GemCost, TEXT("t"), T);
	TestTrue(TEXT("gem path"), P.CanUnlockPower(TEXT("fire_2_ember"), bM, Why) && !bM);
	TestTrue(TEXT("unlock by gems"), P.UnlockPower(TEXT("fire_2_ember"), T, Why));
	TestEqual(TEXT("gems spent"), P.Get().Wallet.Total(), 0);
	const FZoogiPowerRow* Prec = P.Config().FindPower(TEXT("void_1_precision"));
	P.AddMasteryXP(EZoogiCardElement::Void, P.Config().MasteryCumulativeXP[Prec->MasteryLevel - 1], T);
	const int32 G0 = P.Get().Wallet.Total();
	TestTrue(TEXT("mastery path"), P.CanUnlockPower(TEXT("void_1_precision"), bM, Why) && bM);
	TestTrue(TEXT("unlock by mastery"), P.UnlockPower(TEXT("void_1_precision"), T, Why));
	TestEqual(TEXT("mastery unlock costs nothing"), P.Get().Wallet.Total(), G0);
	FZoogiProgression Q; FString Err; Q.Load(Cfg(), P.Save(), Err);
	TestTrue(TEXT("unlocks persist"), Q.IsPowerUnlocked(TEXT("fire_2_ember")) && Q.IsPowerUnlocked(TEXT("void_1_precision")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiProgMetaTest, "ZoogiRoll.Progression.AffinityAchievementsTitlesCosmetics", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiProgMetaTest::RunTest(const FString&)
{
	FZoogiProgression P; P.Init(Cfg()); const FDateTime T = At(14, 12); FString R, Why;
	// affinity: independent per Zoogi
	P.AddAffinityXP(TEXT("blue"), 320, T);
	TestEqual(TEXT("blue L3"), P.AffinityLevel(TEXT("blue")), 3);
	TestEqual(TEXT("tavi L1"), P.AffinityLevel(TEXT("tavi")), 1);
	TestTrue(TEXT("claim L2"), P.ClaimAffinityReward(TEXT("blue"), 2, T, R));
	TestEqual(TEXT("emote id"), R, FString(TEXT("emote_blue_wave")));
	TestFalse(TEXT("claim once"), P.ClaimAffinityReward(TEXT("blue"), 2, T, R));
	TestFalse(TEXT("L4 not reached"), P.ClaimAffinityReward(TEXT("blue"), 4, T, R));
	TestFalse(TEXT("tavi nothing"), P.ClaimAffinityReward(TEXT("tavi"), 2, T, R));
	TestTrue(TEXT("affinity reward is cosmetic"), P.Get().OwnedCosmetics.Contains(TEXT("emote_blue_wave")));
	// match completion -> Arena Rookie title, equipped
	FZoogiMatchTally M; M.MatchId = FGuid::NewGuid(); M.ZoogiId = TEXT("tavi");
	const FZoogiMatchRewardResult Res = P.ApplyMatch(M, EZoogiMatchOutcome::Win, T, At(14, 7));
	TestTrue(TEXT("first match achievement"), P.HasAchievement(TEXT("A_FIRST_MATCH")));
	TestTrue(TEXT("Arena Rookie owned+equipped"), P.Get().OwnedTitles.Contains(TEXT("arena_rookie")) && P.Get().EquippedTitle == FName(TEXT("arena_rookie")));
	TestEqual(TEXT("title name"), P.TitleName(TEXT("arena_rookie")), FString(TEXT("Arena Rookie")));
	TestTrue(TEXT("tavi affinity from match"), Res.AffinityXP > 0 && P.AffinityLevel(TEXT("tavi")) >= 1 && P.Get().Affinity.Contains(TEXT("tavi")));
	TestFalse(TEXT("cannot equip unowned title"), P.EquipTitle(TEXT("zoogi_legend"), T));
	// all 7 titles reachable through data
	for (const FZoogiTitleRow& Ti : P.Config().Titles)
		TestTrue(TEXT("title has achievement ") + Ti.Id.ToString(), P.Config().Achievements.ContainsByPredicate([&](const FZoogiAchievementRow& A) { return A.Title == Ti.Id; }));
	// cosmetics: default equipped, buy + equip + persist
	TestTrue(TEXT("default border equipped"), P.EquippedCosmetic(TEXT("animated_border")) == FName(TEXT("border_default")));
	TestFalse(TEXT("cannot equip unowned"), P.EquipCosmetic(TEXT("border_gold"), T));
	TestFalse(TEXT("achievement cosmetic not for sale"), P.BuyCosmetic(TEXT("border_awaken"), T, Why));
	P.GrantGems(Id(TEXT("c")), EZoogiGemBucket::Earned, 1000, TEXT("t"), T);
	TestTrue(TEXT("buy gold border"), P.BuyCosmetic(TEXT("border_gold"), T, Why));
	TestTrue(TEXT("equip"), P.EquipCosmetic(TEXT("border_gold"), T));
	FZoogiProgression Q; FString Err; Q.Load(Cfg(), P.Save(), Err);
	TestTrue(TEXT("equip state persists"), Q.EquippedCosmetic(TEXT("animated_border")) == FName(TEXT("border_gold")) && Q.Get().EquippedTitle == FName(TEXT("arena_rookie")));
	TestTrue(TEXT("claims persist"), Q.Get().Affinity.FindRef(TEXT("blue")).ClaimedLevels.Contains(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiCPUTierTest, "ZoogiRoll.CPU.Tiers", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiCPUTierTest::RunTest(const FString&)
{
	const FZoogiProgressionConfig C = Cfg();
	// A fixed board: option 0 is cheapest but leaves the shooter on its edge; option 2 is slightly worse but safe.
	TArray<FZoogiCPUShotOption> Opts = { { 100.f, 0.9f, 0.8f }, { 180.f, 0.5f, 0.6f }, { 220.f, 0.0f, 0.1f }, { 380.f, 0.f, 0.f }, { 900.f, 0.f, 0.f } };
	const EZoogiCPUTier Tiers[4] = { EZoogiCPUTier::Easy, EZoogiCPUTier::Normal, EZoogiCPUTier::Hard, EZoogiCPUTier::Expert };
	TArray<FString> Sig;
	for (EZoogiCPUTier Ti : Tiers)
	{
		const FZoogiTierRow& R = C.Tier(Ti);
		FString S;
		for (int32 Seed : { 1, 2, 3, 4, 5, 6, 7, 8 }) { FRandomStream Rng(Seed); S += FString::FromInt(ZoogiCPU::PickShot(Opts, R, Rng)); }
		Sig.Add(S);
		AddInfo(FString::Printf(TEXT("%s picks %s aim_noise %.2f"), ZoogiProgress::TierName(Ti), *S, R.AimNoise));
	}
	TestTrue(TEXT("expert always the safe lookahead shot"), Sig[3] == TEXT("22222222"));
	TestFalse(TEXT("normal: legacy top-3 window, never the weak shots"), Sig[1].Contains(TEXT("3")) || Sig[1].Contains(TEXT("4")));
	TestFalse(TEXT("hard/expert never take the edge trap (lookahead)"), Sig[2].Contains(TEXT("0")) || Sig[3].Contains(TEXT("0")));
	{ bool bWeak = false; for (int32 Seed = 1; Seed <= 200 && !bWeak; ++Seed) { FRandomStream Rng(Seed); bWeak = ZoogiCPU::PickShot(Opts, C.Tier(EZoogiCPUTier::Easy), Rng) >= 3; } TestTrue(TEXT("easy considers weak shots (200 seeds)"), bWeak); }
	for (int32 a = 0; a < 4; ++a) for (int32 b = a + 1; b < 4; ++b) TestNotEqual(FString::Printf(TEXT("tier %d vs %d differ"), a, b), Sig[a], Sig[b]);
	TestTrue(TEXT("aim noise ordered (skill only)"), C.Tier(EZoogiCPUTier::Easy).AimNoise > C.Tier(EZoogiCPUTier::Normal).AimNoise && C.Tier(EZoogiCPUTier::Normal).AimNoise > C.Tier(EZoogiCPUTier::Hard).AimNoise && C.Tier(EZoogiCPUTier::Hard).AimNoise > C.Tier(EZoogiCPUTier::Expert).AimNoise);
	// cards: same hand + same energy for every tier (no resource differences); different choices
	TArray<FZoogiCPUCardOption> Hand = { { TEXT("guard"), 1, 2 }, { TEXT("heavy_roll"), 2, 3 }, { TEXT("double_impact"), 2, 2 }, { TEXT("recovery_roll"), 1, 1 }, { TEXT("storm"), 1, 3 } };
	TArray<FZoogiCPUCombo> Combos = { { TEXT("heavy_roll"), TEXT("double_impact"), 30 } };
	TArray<FString> CardSig;
	for (EZoogiCPUTier Ti : Tiers)
	{
		FRandomStream Rng(7); FString S; for (int32 i : ZoogiCPU::PickCards(Hand, 5, C.Tier(Ti), Combos, Rng)) S += Hand[i].CardId.ToString() + TEXT(" ");
		CardSig.Add(S); AddInfo(FString::Printf(TEXT("%s cards: %s"), ZoogiProgress::TierName(Ti), *S));
	}
	TestTrue(TEXT("hard plans the heavy_roll + double_impact combo"), CardSig[2].Contains(TEXT("heavy_roll")) && CardSig[2].Contains(TEXT("double_impact")));
	TestFalse(TEXT("expert conserves: no filler recovery_roll"), CardSig[3].Contains(TEXT("recovery_roll")));
	TestNotEqual(TEXT("easy vs normal cards differ"), CardSig[0], CardSig[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiStorePipelineTest, "ZoogiRoll.Store.PurchasePipeline_TestBackendOnly", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiStorePipelineTest::RunTest(const FString&)
{
	AddInfo(TEXT("AUTOMATION TEST BACKEND ONLY - no real store, no real payment."));
	FZoogiProgression P; P.Init(Cfg()); FTestStore S; const FDateTime T = At(14, 12); FString Why;
	const FZoogiGemPackageRow& Pkg = P.Config().GemPackages[0];
	const FZoogiStoreReceipt R{ TEXT("test"), TEXT("TXN-1"), Pkg.PackageId.ToString(), TEXT("ok"), true };
	TestTrue(TEXT("granted"), FZoogiPurchaseService::ProcessReceipt(P, S, R, T, Why) == EZoogiPurchaseOutcome::Granted);
	TestEqual(TEXT("verified amount"), P.Get().Wallet.Purchased(), Pkg.Gems + Pkg.BonusGems);
	TestTrue(TEXT("consumed after grant"), S.Finalized == 1 && P.Get().Purchases[0].State == EZoogiPurchaseState::Consumed);
	TestTrue(TEXT("replay -> already granted"), FZoogiPurchaseService::ProcessReceipt(P, S, R, T, Why) == EZoogiPurchaseOutcome::AlreadyGranted);
	TestEqual(TEXT("no duplicate gems"), P.Get().Wallet.Purchased(), Pkg.Gems + Pkg.BonusGems);
	TestTrue(TEXT("bad receipt rejected"), FZoogiPurchaseService::ProcessReceipt(P, S, { TEXT("test"), TEXT("TXN-2"), Pkg.PackageId.ToString(), TEXT("forged"), true }, T, Why) == EZoogiPurchaseOutcome::Rejected);
	TestTrue(TEXT("unknown product"), FZoogiPurchaseService::ProcessReceipt(P, S, { TEXT("test"), TEXT("TXN-3"), TEXT("nope"), TEXT("ok"), true }, T, Why) == EZoogiPurchaseOutcome::UnknownProduct);
	S.bPending = true;
	TestTrue(TEXT("pending verification grants nothing"), FZoogiPurchaseService::ProcessReceipt(P, S, { TEXT("test"), TEXT("TXN-4"), Pkg.PackageId.ToString(), TEXT("ok"), true }, T, Why) == EZoogiPurchaseOutcome::PendingVerification);
	TestEqual(TEXT("still one package"), P.Get().Wallet.Purchased(), Pkg.Gems + Pkg.BonusGems);
	S.bPending = false;
	TestTrue(TEXT("pending resumes later"), FZoogiPurchaseService::ProcessReceipt(P, S, { TEXT("test"), TEXT("TXN-4"), Pkg.PackageId.ToString(), TEXT("ok"), true }, T, Why) == EZoogiPurchaseOutcome::Granted);
	// AC (Evan Q2): buy, receive, spend, restart, balance correct
	P.SpendGems(Id(TEXT("spend")), 50, TEXT("cosmetic"), T);
	const int32 Bal = P.Get().Wallet.Total();
	FZoogiProgression Q; FString Err; TestTrue(TEXT("restart"), Q.Load(Cfg(), P.Save(), Err));
	TestEqual(TEXT("balance after restart"), Q.Get().Wallet.Total(), Bal);
	TestTrue(TEXT("replay after restart blocked"), FZoogiPurchaseService::ProcessReceipt(Q, S, R, T, Why) == EZoogiPurchaseOutcome::AlreadyGranted && Q.Get().Wallet.Total() == Bal);
	TestEqual(TEXT("purchase records persisted"), Q.Get().Purchases.Num(), P.Get().Purchases.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiStoreAppleJwsTest, "ZoogiRoll.Store.AppleJwsClaims", ZoogiProgTest::ZoogiProgTestFlags)
bool FZoogiStoreAppleJwsTest::RunTest(const FString&)
{
	auto B64 = [](const FString& S) { FTCHARToUTF8 U(*S); return FBase64::Encode((const uint8*)U.Get(), U.Length()).Replace(TEXT("+"), TEXT("-")).Replace(TEXT("/"), TEXT("_")).Replace(TEXT("="), TEXT("")); };
	const FString H = B64(TEXT("{\"alg\":\"ES256\",\"x5c\":[\"a\",\"b\",\"c\"]}"));
	auto Make = [&](const FString& Extra) { return H + TEXT(".") + B64(TEXT("{\"transactionId\":\"2000000123\",\"bundleId\":\"com.zoogiroll.game\",\"productId\":\"com.zoogiroll.gems.small\",\"type\":\"Consumable\",\"environment\":\"Sandbox\"") + Extra + TEXT("}")) + TEXT(".sig"); };
	const FZoogiStoreReceipt R{ TEXT("apple"), TEXT("2000000123"), TEXT("com.zoogiroll.gems.small"), Make(TEXT("")), true };
	FString Why;
	TestTrue(TEXT("valid claims"), ZoogiAppleJws::CheckClaims(R.Payload, R, TEXT("com.zoogiroll.game"), Why) == EZoogiVerifyResult::Valid);
	TestTrue(TEXT("wrong bundle"), ZoogiAppleJws::CheckClaims(R.Payload, R, TEXT("com.other"), Why) == EZoogiVerifyResult::Invalid);
	FZoogiStoreReceipt R2 = R; R2.TransactionId = TEXT("999");
	TestTrue(TEXT("txn mismatch"), ZoogiAppleJws::CheckClaims(R.Payload, R2, TEXT("com.zoogiroll.game"), Why) == EZoogiVerifyResult::Invalid);
	TestTrue(TEXT("revoked"), ZoogiAppleJws::CheckClaims(Make(TEXT(",\"revocationDate\":1")), R, TEXT("com.zoogiroll.game"), Why) == EZoogiVerifyResult::Invalid);
	FZoogiStoreReceipt R3 = R; R3.bSandbox = false;
	TestTrue(TEXT("sandbox in production"), ZoogiAppleJws::CheckClaims(R.Payload, R3, TEXT("com.zoogiroll.game"), Why) == EZoogiVerifyResult::Invalid);
	TestTrue(TEXT("garbage"), ZoogiAppleJws::CheckClaims(TEXT("x.y"), R, TEXT("com.zoogiroll.game"), Why) == EZoogiVerifyResult::Invalid);
	// real backends are never "available" without platform + verification server
	FZoogiOSSStoreBackend Apple(TEXT("apple"), TEXT("IOS")); FZoogiSteamStoreBackend Steam;
	TestFalse(TEXT("apple unavailable on this box"), Apple.IsAvailable(Why)); AddInfo(TEXT("apple: ") + Why);
	TestFalse(TEXT("steam unavailable"), Steam.IsAvailable(Why));
	return true;
}

#endif
