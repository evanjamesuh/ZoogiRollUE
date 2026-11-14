#include "ZoogiEpisodeDirector.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiEpisodeLedger.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiReactionComponent.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"

namespace
{
	int32 PlayerOfId(const FString& Id)
	{
		// "P<n>_<name>" -> n-1 ; orbs "O<n>" -> -1
		if (!Id.StartsWith(TEXT("P"))) return -1;
		int32 U = INDEX_NONE;
		if (!Id.FindChar(TCHAR('_'), U)) U = Id.Len();
		return FCString::Atoi(*Id.Mid(1, U - 1)) - 1;
	}
	uint32 LinesHash(const FZoogiEpisode& E)
	{
		uint32 H = FCrc::StrCrc32(*FString::Printf(TEXT("%d|%d|%d"), E.EpisodeId, E.Spender, (int32)E.Branch));
		for (const FZoogiEpPerspective& V : E.Perspectives) H = FCrc::StrCrc32(*FString::Printf(TEXT("%s|%.1f"), *V.Line, V.Start), H);
		return H;
	}
}

UZoogiEpisodeDirector* UZoogiEpisodeDirector::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UZoogiEpisodeDirector>() : nullptr;
}

void UZoogiEpisodeDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bSkipAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEpisodeSkip"));
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiEpisodeP5Test="), TestCase);
}

void UZoogiEpisodeDirector::Deinitialize()
{
	if (!TestCase.IsEmpty() && !bTestDone && GetWorld() && GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		TestCheck(false, TEXT("match ended before the test finished (no MATCH_END)"));
		TestFinish(nullptr);
	}
	Super::Deinitialize();
}

bool UZoogiEpisodeDirector::IsActive() const
{
	const AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return false;
	static const bool bParam = FParse::Param(FCommandLine::Get(), TEXT("ZoogiEpisodeP5"));
	return bParam || G->CardsOn();
}

FZoogiEpFact UZoogiEpisodeDirector::FactFromLedger(const FZoogiEpisodeLedgerEvent& E)
{
	FZoogiEpFact F;
	F.EventId = E.event_id; F.Seq = E.seq; F.Kind = E.kind; F.Turn = E.turn_index; F.Round = E.round; F.ActionId = E.action_id; F.Actor = E.actor_player;
	for (const FString& T : E.target_ids) { const int32 P = PlayerOfId(T); if (P >= 0) F.Targets.AddUnique(P); }
	for (const FString& T : E.ring_out_ids) { const int32 P = PlayerOfId(T); if (P >= 0) F.RingOuts.AddUnique(P); }
	F.Targets.Sort(); F.RingOuts.Sort();
	F.bCollision = E.collision;
	F.bFollowUp = E.command == TEXT("FOLLOW_UP");
	F.bShattered = E.comic_state == TEXT("SHATTERED");
	F.bObjectiveFailed = E.objective_result == TEXT("FAILED") || E.objective_result == TEXT("TIMEOUT");
	F.bActorRecovered = E.zones.Contains(TEXT("recovery")) && !F.RingOuts.Contains(F.Actor);
	F.bMatchOver = E.next_state == TEXT("MATCH_END") && E.kind == TEXT("ACTION");
	F.FinalHash = E.final_hash;
	return F;
}

void UZoogiEpisodeDirector::EnsureReset(UZoogiBattleStateMachine* SM)
{
	if (MatchKey == SM->MatchId && CoreV.NumPlayers() == FMath::Max(1, SM->Players.Num())) return;
	MatchKey = SM->MatchId;
	TArray<FString> Names;
	for (const FZoogiBattlePlayerState& P : SM->Players) Names.Add(P.Name);
	const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	CoreV.Reset(SM->Players.Num(), G && G->bComboMode, Names);
	Queue.Reset();
	UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 match %s players=%d teams=%d skip_all=%d"), *MatchKey, SM->Players.Num(), G && G->bComboMode ? 1 : 0, bSkipAll ? 1 : 0);
}

void UZoogiEpisodeDirector::OnLedgerEvent(UZoogiBattleStateMachine* SM, const FZoogiEpisodeLedgerEvent& E)
{
	if (!SM || !IsActive() || E.kind == TEXT("EPISODE")) return;
	EnsureReset(SM);
	const FZoogiEpFact F = FactFromLedger(E);
	if (!CoreV.AddFact(F)) { UE_LOG(LogZoogi, Warning, TEXT("EPISODE_P5 duplicate event %s ignored (counted once)"), *E.event_id); return; }
	const int32 Fi = CoreV.Facts().Num() - 1;
	if (F.Kind == TEXT("ACTION"))
	{
		FString Roles;
		for (int32 p = 0; p < CoreV.NumPlayers(); ++p) Roles += FString::Printf(TEXT(" P%d=%s"), p + 1, ZoogiEpRoleName(CoreV.RoleOf(CoreV.Facts()[Fi], p)));
		UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 fact %s action=%d roles:%s"), *F.EventId, F.ActionId, *Roles);
		if (!TestCase.IsEmpty() && !bTestDone) TestOnFact(SM, Fi, true);
		TrySpendOn(SM, Fi);
		if (!TestCase.IsEmpty() && !bTestDone) TestOnFact(SM, Fi, false);
	}
	else if (F.Kind == TEXT("MATCH_END"))
	{
		TrySpendOn(SM, Fi); // a full gauge at the end is spent on the VICTORY / LOSS branch
		for (int32 p = 0; p < CoreV.NumPlayers(); ++p)
		{
			FString Why;
			const EZoogiRecap R = CoreV.RecapUnlock(p, &Why);
			const FZoogiEpContribution& C = CoreV.Contribution(p);
			UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 recap P%d %s (PROPOSED rules: %s) contrib open=%d follow=%d recv=%d prep=%d miss=%d out=%d"), p + 1, ZoogiRecapName(R), *Why,
				C.Opening, C.FollowThrough, C.Receiving, C.Preparing, C.Misses, C.RungOut);
		}
		UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 match end: facts=%d actions=%d episodes=%d dups=%d core=%08x"), CoreV.Facts().Num(), CoreV.ActionFacts(), CoreV.Episodes().Num(), CoreV.Duplicates(), CoreV.Hash());
		if (!TestCase.IsEmpty() && !bTestDone) TestFinish(SM);
	}
}

void UZoogiEpisodeDirector::TrySpendOn(UZoogiBattleStateMachine* SM, int32 FactIndex)
{
	const FZoogiEpFact F = CoreV.Facts()[FactIndex];
	// one episode per fact: the actor first, then the others in player order
	TArray<int32> Order;
	if (F.Kind == TEXT("ACTION") && F.Actor >= 0) Order.Add(F.Actor);
	for (int32 p = 0; p < CoreV.NumPlayers(); ++p) Order.AddUnique(p);
	for (int32 P : Order)
	{
		UZoogiEpisodeEnergyComponent* En = SM->Energy(P);
		if (!En || !En->CanAfford(ZoogiEpTune::GaugeCost)) continue;
		FZoogiEpisode Ep;
		if (!CoreV.TrySpend(P, En->Value, FactIndex, Ep)) continue;
		En->Spend(ZoogiEpTune::GaugeCost, TEXT("episode"));
		UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 spend P%d -100 -> %.0f episode=%d on %s branch=%s (%.0f s, %d perspectives; Awakening untouched)"), P + 1, En->Value, Ep.EpisodeId,
			*Ep.FactEventId, ZoogiEpBranchName(Ep.Branch), Ep.Duration, Ep.Perspectives.Num());
		for (const FZoogiEpPerspective& V : Ep.Perspectives) UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5   %4.1fs P%d %s: %s"), V.Start, V.Player + 1, ZoogiEpRoleName(V.Role), *V.Line);
		if (SM->Ledger)
		{
			FZoogiEpisodeLedgerEvent L;
			L.kind = TEXT("EPISODE");
			L.turn_index = F.Turn; L.round = F.Round; L.action_id = F.ActionId;
			L.actor_player = P;
			L.actor_id = FString::Printf(TEXT("P%d_%s"), P + 1, SM->Players.IsValidIndex(P) ? *SM->Players[P].Name.ToLower() : TEXT("zoogi"));
			L.command = FString::Printf(TEXT("EPISODE:%s"), ZoogiEpBranchName(Ep.Branch));
			L.episode_segment_id = FString::Printf(TEXT("ep_%03d_from_%s"), Ep.EpisodeId, *Ep.FactEventId);
			for (const FZoogiEpPerspective& V : Ep.Perspectives) { L.spotlight.Add(V.Player); L.tags.Add(FString::Printf(TEXT("PERSPECTIVE:P%d:%s"), V.Player + 1, ZoogiEpRoleName(V.Role))); }
			for (int32 p = 0; p < SM->Players.Num(); ++p) L.episode_energy_delta.Add(p == P ? -ZoogiEpTune::GaugeCost : 0.f);
			L.next_state = TEXT("PLAYBACK");
			SM->Ledger->Append(L);
		}
		FZoogiEpPlayback Pb(Ep);
		if (bSkipAll) { Pb.Skip(); ++Skipped; UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 playback episode=%d skipped (resolved state kept)"), Ep.EpisodeId); }
		else Queue.Add(Pb);
		return; // one episode per fact
	}
}

void UZoogiEpisodeDirector::SkipCurrent()
{
	if (!Queue.Num()) return;
	Queue[0].Skip();
	++Skipped;
	UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 playback episode=%d skipped by player at %.1f s"), Queue[0].Ep.EpisodeId, Queue[0].T);
}

void UZoogiEpisodeDirector::Tick(float Dt)
{
	if (!Queue.Num()) return;
	FZoogiEpPlayback& P = Queue[0];
	const int32 Before = P.PerspectiveIndex();
	P.Tick(Dt);
	if (P.PerspectiveIndex() != Before && !P.Done()) UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 playback episode=%d perspective %d/4 P%d"), P.Ep.EpisodeId, P.PerspectiveIndex() + 1, P.Ep.Perspectives[P.PerspectiveIndex()].Player + 1);
	if (P.Done())
	{
		if (!P.bSkipped) ++Played;
		if (TestEpisodes > 0 && P.Ep.Spender == TestSpender) PlaybackLineHash = LinesHash(P.Ep);
		UE_LOG(LogZoogi, Log, TEXT("EPISODE_P5 playback episode=%d %s"), P.Ep.EpisodeId, P.bSkipped ? TEXT("ended (skipped)") : TEXT("watched to the end (12 s)"));
		Queue.RemoveAt(0);
	}
}

// ------------------------------------------------------------------ -ZoogiEpisodeP5Test=gauge

void UZoogiEpisodeDirector::TestCheck(bool bOk, const FString& What)
{
	++TestChecks;
	if (!bOk) { ++TestFails; if (TestFirstFail.IsEmpty()) TestFirstFail = What; UE_LOG(LogZoogi, Warning, TEXT("EP5_TEST check FAIL: %s"), *What); }
}

void UZoogiEpisodeDirector::TestOnFact(UZoogiBattleStateMachine* SM, int32 Fi, bool bBeforeSpend)
{
	const FZoogiEpFact& F = CoreV.Facts()[Fi];
	if (bBeforeSpend)
	{
		if (bFixtureDone || F.Actor < 0) return;
		UZoogiEpisodeEnergyComponent* En = SM->Energy(F.Actor);
		if (!En) return;
		// TEST FIXTURE (test only, logged): the CPU earns ~10 Episode per action, a short match never reaches 100.
		bFixtureDone = true;
		TestSpender = F.Actor;
		for (int32 p = 0; p < SM->Players.Num(); ++p)
		{
			UZoogiEpisodeEnergyComponent* X = SM->Energy(p);
			TestCheck(!X || X->Value < 100.f, FString::Printf(TEXT("no gauge full before the fixture (P%d %.0f)"), p + 1, X ? X->Value : 0.f));
		}
		En->SetValue(100.f, TEXT("test_fixture_p5"));
		if (UZoogiReactionComponent* R = SM->Reaction(F.Actor)) { bAwakeBefore = R->bAwakened; AwakeActBefore = R->Activations; AwakeLeftBefore = R->ReactionsLeft; }
		return;
	}
	if (TestSpender < 0 || TestEpisodes > 0) return;
	const int32 Spender = TestSpender;
	TestEpisodes = CoreV.Episodes().Num();
	TestCheck(TestEpisodes == 1, FString::Printf(TEXT("exactly one episode spent (%d)"), TestEpisodes));
	if (!TestEpisodes) return;
	const FZoogiEpisode& E = CoreV.Episodes()[0];
	CoreLineHash = LinesHash(E);
	UZoogiEpisodeEnergyComponent* En = SM->Energy(Spender);
	TestCheck(E.Spender == Spender && En && FMath::IsNearlyEqual(En->Value, 0.f), FString::Printf(TEXT("spender P%d gauge 100 -> %.0f"), Spender + 1, En ? En->Value : -1.f));
	FZoogiEpisode Again;
	TestCheck(!CoreV.TrySpend(Spender, 100.f, Fi, Again), TEXT("second spend on the same fact refused"));
	TestCheck(E.Perspectives.Num() == 4 && FMath::IsNearlyEqual(E.Duration, 12.f), FString::Printf(TEXT("4 perspectives, 12 s (%d, %.1f)"), E.Perspectives.Num(), E.Duration));
	TSet<int32> Seen;
	for (int32 k = 0; k < E.Perspectives.Num(); ++k)
	{
		const FZoogiEpPerspective& V = E.Perspectives[k];
		Seen.Add(V.Player);
		TestCheck(FMath::IsNearlyEqual(V.Start, k * 3.f) && FMath::IsNearlyEqual(V.Duration, 3.f) && !V.Line.IsEmpty(), FString::Printf(TEXT("perspective %d at %.0f s, 3 s, has a line"), k + 1, V.Start));
		TestCheck(V.Role == CoreV.RoleOf(F, V.Player), FString::Printf(TEXT("perspective P%d role %s matches the fact"), V.Player + 1, ZoogiEpRoleName(V.Role)));
		if (V.Role == EZoogiEpRole::Miss) TestCheck(V.Line.Contains(TEXT("misses")), TEXT("a miss is acknowledged in the line"));
		if (V.Role == EZoogiEpRole::ReceivingImpact) TestCheck(V.Line.Contains(TEXT("takes the impact")), TEXT("receiving impact line"));
	}
	TestCheck(E.Perspectives[0].Player == Spender, TEXT("spender's perspective first"));
	TestCheck(Seen.Num() == FMath::Min(4, CoreV.NumPlayers()), FString::Printf(TEXT("all %d players have a perspective"), Seen.Num()));
	TestCheck(E.Branch == CoreV.BranchFor(F, Spender), TEXT("branch from resolved facts"));
	if (UZoogiReactionComponent* R = SM->Reaction(Spender))
		TestCheck(R->bAwakened == bAwakeBefore && R->Activations == AwakeActBefore && R->ReactionsLeft == AwakeLeftBefore, TEXT("Awakening untouched by the episode spend"));
	bool bLedger = false;
	for (const FZoogiEpisodeLedgerEvent& L : SM->Ledger->Events) if (L.kind == TEXT("EPISODE") && L.actor_player == Spender) bLedger = true;
	TestCheck(bLedger, TEXT("EPISODE event in the ledger"));
	// replay-safe: the same ledger event delivered again is counted once
	const int32 Dups0 = CoreV.Duplicates();
	const FZoogiEpContribution C0 = CoreV.Contribution(Spender);
	const bool bAdded = CoreV.AddFact(F);
	TestCheck(!bAdded && CoreV.Duplicates() == Dups0 + 1 && CoreV.Contribution(Spender).Opening == C0.Opening && CoreV.Contribution(Spender).Misses == C0.Misses, TEXT("re-delivered event counted once"));
	UE_LOG(LogZoogi, Log, TEXT("EP5_TEST gauge: episode %d by P%d branch=%s on %s"), E.EpisodeId, Spender + 1, ZoogiEpBranchName(E.Branch), *E.FactEventId);
}

void UZoogiEpisodeDirector::TestFinish(UZoogiBattleStateMachine* SM)
{
	bTestDone = true;
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	FString Recap;
	if (SM)
	{
		TestCheck(TestEpisodes == 1, TEXT("the fixture produced an episode"));
		int32 Full = 0;
		for (int32 p = 0; p < CoreV.NumPlayers(); ++p)
		{
			FString Why;
			const EZoogiRecap R = CoreV.RecapUnlock(p, &Why);
			Recap += FString::Printf(TEXT("P%d:%s,"), p + 1, ZoogiRecapName(R));
			const FZoogiEpContribution& C = CoreV.Contribution(p);
			const bool bExpectFull = CoreV.ActionFacts() >= ZoogiEpTune::RecapMinActions && (C.ActiveWithContact >= ZoogiEpTune::RecapMinActive || CoreV.IsWinnerSide(p));
			TestCheck((R == EZoogiRecap::Full) == bExpectFull, FString::Printf(TEXT("recap P%d %s follows U1-U3 (%s)"), p + 1, ZoogiRecapName(R), *Why));
			Full += R == EZoogiRecap::Full;
			int32 Roles = 0;
			for (const FZoogiEpFact& F : CoreV.Facts()) if (F.Kind == TEXT("ACTION")) ++Roles;
			TestCheck(C.Opening + C.FollowThrough + C.Receiving + C.Preparing + C.Misses == Roles, FString::Printf(TEXT("P%d has exactly one role per action (%d)"), p + 1, Roles));
		}
		if (!bSkipAll) TestCheck(Played + Queue.Num() >= 1 || PlaybackLineHash != 0, TEXT("watch mode played the episode"));
		if (PlaybackLineHash) TestCheck(PlaybackLineHash == CoreLineHash, TEXT("playback showed exactly the resolved episode"));
		if (bSkipAll) TestCheck(Skipped >= 1 && Played == 0, TEXT("skip mode skipped every episode"));
	}
	FString Fin;
	if (G) for (const AZoogiBall* B : G->Balls) if (B) Fin += FString::Printf(TEXT("%d:%.0f,%.0f,%d,%d;"), B->PlayerIndex, B->BallLocation().X, B->BallLocation().Y, B->bOut ? 1 : 0, B->Score);
	const uint32 LedgerH = SM && SM->Ledger ? SM->Ledger->Hash() : 0;
	UE_LOG(LogZoogi, Log, TEXT("EP5_TEST case=%s result=%s checks=%d fails=%d mode=%s episodes=%d played=%d skipped=%d sim=%08x_%08x core=%08x recap=%s first_fail=\"%s\""), *TestCase, TestFails == 0 ? TEXT("PASS") : TEXT("FAIL"),
		TestChecks, TestFails, bSkipAll ? TEXT("skip") : TEXT("watch"), CoreV.Episodes().Num(), Played, Skipped, LedgerH, FCrc::StrCrc32(*Fin), CoreV.Hash(), *Recap, *TestFirstFail);
}
