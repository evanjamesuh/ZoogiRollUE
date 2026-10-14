// v3 Phase 5 automation tests on FZoogiEpisodeCore (ZoogiRoll.Episode.P5.*). The runtime case
// -ZoogiEpisodeP5Test=gauge covers the same rules in a real match (watch and -ZoogiEpisodeSkip).
#include "ZoogiEpisodeCore.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

constexpr EAutomationTestFlags ZoogiEpFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
#define EP_TEST(Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiEpP5##Name, "ZoogiRoll.Episode.P5." #Name, ZoogiEpFlags) bool FZoogiEpP5##Name::RunTest(const FString&)

namespace
{
	int32 GSeq = 0;
	FZoogiEpFact Act(int32 Actor, TArray<int32> Targets, int32 ActionId, bool bFollow = false)
	{
		FZoogiEpFact F; F.Kind = TEXT("ACTION"); F.Seq = GSeq++; F.EventId = FString::Printf(TEXT("ev_%04d_action"), F.Seq);
		F.Actor = Actor; F.Targets = Targets; F.bCollision = Targets.Num() > 0; F.ActionId = ActionId; F.Turn = ActionId; F.bFollowUp = bFollow;
		F.FinalHash = FString::Printf(TEXT("h%d"), ActionId);
		return F;
	}
	FZoogiEpFact End(int32 Winner) { FZoogiEpFact F; F.Kind = TEXT("MATCH_END"); F.Seq = GSeq++; F.EventId = TEXT("ev_end"); F.Actor = Winner; F.ActionId = 999; return F; }
	FZoogiEpisodeCore NewCore(bool bTeams = false) { FZoogiEpisodeCore C; C.Reset(4, bTeams, { TEXT("Blue"), TEXT("Tavi"), TEXT("Vex"), TEXT("Moss") }); return C; }
}

EP_TEST(LedgerCountsOnce)
{
	FZoogiEpisodeCore C = NewCore();
	const FZoogiEpFact F = Act(0, { 2 }, 1);
	TestTrue(TEXT("first delivery added"), C.AddFact(F));
	TestFalse(TEXT("re-delivery ignored"), C.AddFact(F));
	TestEqual(TEXT("one action counted"), C.ActionFacts(), 1);
	TestEqual(TEXT("one duplicate"), C.Duplicates(), 1);
	TestEqual(TEXT("opening counted once"), C.Contribution(0).Opening, 1);
	TestEqual(TEXT("receiving counted once"), C.Contribution(2).Receiving, 1);
	// replay: the same facts in the same order give the same hash
	FZoogiEpisodeCore A = NewCore(), B = NewCore();
	for (FZoogiEpisodeCore* X : { &A, &B }) { X->AddFact(Act(0, { 2 }, 1)); X->AddFact(Act(1, {}, 2)); X->AddFact(End(0)); }
	TestEqual(TEXT("replay hash identical"), A.Hash(), B.Hash());
	return true;
}

EP_TEST(RolesAndMiss)
{
	FZoogiEpisodeCore C = NewCore(true);
	C.AddFact(Act(0, { 2 }, 1));
	C.AddFact(Act(1, { 3 }, 1, true)); // partner follow-up
	C.AddFact(Act(2, {}, 2));          // miss
	const auto& F = C.Facts();
	TestEqual(TEXT("opener"), C.RoleOf(F[0], 0), EZoogiEpRole::Opening);
	TestEqual(TEXT("target"), C.RoleOf(F[0], 2), EZoogiEpRole::ReceivingImpact);
	TestEqual(TEXT("bystander prepares"), C.RoleOf(F[0], 3), EZoogiEpRole::PreparingResponse);
	TestEqual(TEXT("follow-through"), C.RoleOf(F[1], 1), EZoogiEpRole::FollowThrough);
	TestEqual(TEXT("miss"), C.RoleOf(F[2], 2), EZoogiEpRole::Miss);
	TestEqual(TEXT("miss tallied"), C.Contribution(2).Misses, 1);
	for (int32 p = 0; p < 4; ++p)
	{
		const FZoogiEpContribution& X = C.Contribution(p);
		TestEqual(TEXT("one role per action"), X.Opening + X.FollowThrough + X.Receiving + X.Preparing + X.Misses, 3);
	}
	FZoogiEpisode E;
	TestTrue(TEXT("spend on the miss"), C.TrySpend(2, 100.f, 2, E));
	TestTrue(TEXT("miss acknowledged"), E.Perspectives[0].Line.Contains(TEXT("misses")));
	TestEqual(TEXT("miss is a failure branch"), E.Branch, EZoogiEpBranch::Failure);
	FZoogiEpisode E2;
	TestTrue(TEXT("spend on the follow-up"), C.TrySpend(1, 100.f, 1, E2));
	TestTrue(TEXT("follow-through names the opener"), E2.Perspectives[0].Line.Contains(TEXT("follows through on P1 Blue")));
	return true;
}

EP_TEST(GaugeSpendOnce)
{
	FZoogiEpisodeCore C = NewCore();
	C.AddFact(Act(0, { 1 }, 1));
	FZoogiEpisode E;
	TestFalse(TEXT("99 is not enough"), C.TrySpend(0, 99.f, 0, E));
	TestTrue(TEXT("100 spends"), C.TrySpend(0, 100.f, 0, E));
	TestFalse(TEXT("second spend on the same fact refused"), C.TrySpend(0, 100.f, 0, E));
	TestEqual(TEXT("12 s"), E.Duration, 12.f);
	TestEqual(TEXT("four perspectives"), E.Perspectives.Num(), 4);
	TSet<int32> Seen;
	for (int32 k = 0; k < 4; ++k) { TestEqual(TEXT("3 s each"), E.Perspectives[k].Start, 3.f * k); Seen.Add(E.Perspectives[k].Player); }
	TestEqual(TEXT("all four players"), Seen.Num(), 4);
	TestEqual(TEXT("spender first"), E.Perspectives[0].Player, 0);
	TestFalse(TEXT("unknown kind cannot be spent on"), [&]() { FZoogiEpFact T; T.Kind = TEXT("TURN"); T.Seq = GSeq++; C.AddFact(T); FZoogiEpisode X; return C.TrySpend(1, 100.f, C.Facts().Num() - 1, X); }());
	return true;
}

EP_TEST(Branches)
{
	FZoogiEpisodeCore C = NewCore();
	FZoogiEpFact S = Act(0, { 1 }, 1);
	FZoogiEpFact Fl = Act(0, { 1 }, 2); Fl.bShattered = true;
	FZoogiEpFact R = Act(0, {}, 3); R.bActorRecovered = true; R.bCollision = true;
	FZoogiEpFact El = Act(0, { 1 }, 4); El.RingOuts = { 1 };
	TestEqual(TEXT("success"), C.BranchFor(S, 0), EZoogiEpBranch::Success);
	TestEqual(TEXT("failure (shatter)"), C.BranchFor(Fl, 0), EZoogiEpBranch::Failure);
	TestEqual(TEXT("recovery"), C.BranchFor(R, 0), EZoogiEpBranch::Recovery);
	TestEqual(TEXT("elimination"), C.BranchFor(El, 0), EZoogiEpBranch::Elimination);
	TestEqual(TEXT("the rung-out Zoogi's own episode is a failure"), C.BranchFor(El, 1), EZoogiEpBranch::Failure);
	C.AddFact(S);
	C.AddFact(End(0));
	FZoogiEpisode V, L;
	TestTrue(TEXT("spend at the end (winner)"), C.TrySpend(0, 100.f, C.Facts().Num() - 1, V));
	TestTrue(TEXT("spend at the end (loser)"), C.TrySpend(2, 100.f, C.Facts().Num() - 1, L));
	TestEqual(TEXT("victory"), V.Branch, EZoogiEpBranch::Victory);
	TestEqual(TEXT("loss"), L.Branch, EZoogiEpBranch::Loss);
	return true;
}

EP_TEST(PlaybackIsReadOnly)
{
	FZoogiEpisodeCore A = NewCore(), B = NewCore();
	const FZoogiEpFact F = Act(0, { 1 }, 1); // one fact, delivered to both cores
	A.AddFact(F); B.AddFact(F);
	FZoogiEpisode Ea, Eb;
	A.TrySpend(0, 100.f, 0, Ea); B.TrySpend(0, 100.f, 0, Eb);
	FZoogiEpPlayback Watch(Ea), Skip(Eb);
	for (int32 i = 0; i < 800; ++i) Watch.Tick(1.f / 60.f);
	Skip.Skip();
	TestTrue(TEXT("watched to the end"), Watch.Done() && !Watch.bSkipped);
	TestTrue(TEXT("skipped"), Skip.Done() && Skip.bSkipped);
	TestEqual(TEXT("perspective 4 at the end"), Watch.PerspectiveIndex(), 3);
	TestEqual(TEXT("watch vs skip: identical core"), A.Hash(), B.Hash());
	return true;
}

EP_TEST(RecapUnlockRules)
{
	FZoogiEpisodeCore C = NewCore();
	for (int32 i = 0; i < 6; ++i) C.AddFact(Act(i % 2 == 0 ? 0 : 1, i % 2 == 0 ? TArray<int32>{ 2 } : TArray<int32>{}, i + 1)); // P1 3 hits, P2 3 misses
	TestEqual(TEXT("no recap before MATCH_END (U1)"), C.RecapUnlock(0), EZoogiRecap::None);
	C.AddFact(End(3));
	TestEqual(TEXT("active player: FULL"), C.RecapUnlock(0), EZoogiRecap::Full);
	TestEqual(TEXT("misses only: PARTIAL"), C.RecapUnlock(1), EZoogiRecap::Partial);
	TestEqual(TEXT("received only: PARTIAL"), C.RecapUnlock(2), EZoogiRecap::Partial);
	TestEqual(TEXT("winner: FULL"), C.RecapUnlock(3), EZoogiRecap::Full);
	FZoogiEpisodeCore Short = NewCore();
	Short.AddFact(Act(0, { 2 }, 1)); Short.AddFact(Act(0, { 2 }, 2)); Short.AddFact(End(0));
	TestEqual(TEXT("short match: no FULL (U2)"), Short.RecapUnlock(0) == EZoogiRecap::Full && Short.ActionFacts() < 6, false);
	return true;
}

#endif
