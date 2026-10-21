// v3 Phase 6 automation tests (ZoogiRoll.P6.*): save / resume of every pure core, duplicate input, repeated contact.
// The runtime cases -ZoogiSaveTest / -ZoogiMatchTest / -ZoogiCardTest=dup_input|keys cover the same in real matches.
#include "ZoogiCards.h"
#include "ZoogiCardEffects.h"
#include "ZoogiEpisodeCore.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/NameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

constexpr EAutomationTestFlags ZoogiP6Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
#define P6_TEST(Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiP6##Name, "ZoogiRoll.P6." #Name, ZoogiP6Flags) bool FZoogiP6##Name::RunTest(const FString&)

namespace
{
	FZoogiCardTune P6Tune() { FZoogiCardTune T; T.bCards = true; return T; }
	bool P6Make(FZoogiCardRules& R, int32 Seed = 7)
	{
		TArray<TArray<FName>> D; D.Init(ZoogiCards::BuiltInDeck(), 4);
		FString L;
		return R.Init(MakeShared<FZoogiNativeCardBackend>(ZoogiCards::BuiltInDefs()), P6Tune(), Seed, D, ZoogiCards::BuiltInDeck(), L);
	}
	void P6Play(FZoogiCardRules& R, int32 From, int32 To)
	{
		FZoogiCardContext C;
		for (int32 T = From; T < To; ++T)
		{
			const int32 P = T % 4;
			R.BeginStandby(P, T + 1);
			const FZoogiCardPlayerState* S = R.Player(P);
			for (const FZoogiCardInstance& I : TArray<FZoogiCardInstance>(S->Hand)) R.Select(P, I.InstanceId, C);
			R.Commit(P, C);
		}
	}
	template <class T> TArray<uint8> Save(T& X) { TArray<uint8> B; FMemoryWriter W(B); FNameAsStringProxyArchive A(W); X.Serialize(A); return B; }
	template <class T> void Load(T& X, const TArray<uint8>& B) { FMemoryReader R(B); FNameAsStringProxyArchive A(R); X.Serialize(A); }
}

P6_TEST(CardRulesSaveResume)
{
	FZoogiCardRules A, B;
	P6Make(A); P6Make(B, 999); // B starts from a different seed: the load must overwrite everything that matters
	P6Play(A, 0, 9);
	const TArray<uint8> Bytes = Save(A);
	Load(B, Bytes);
	TestEqual(TEXT("loaded state hash"), B.StateHash(), A.StateHash());
	TestEqual(TEXT("re-saved bytes identical"), Save(B), Bytes);
	P6Play(A, 9, 20); P6Play(B, 9, 20); // shuffles included: the RNG stream was restored
	TestEqual(TEXT("resume continues identically"), B.StateHash(), A.StateHash());
	return true;
}

P6_TEST(EffectCoreSaveResume)
{
	FZoogiCardEffectCore A, B;
	A.Arm(FName(TEXT("ember")), 11, 0, 1, 1.2f, 0.f, EZoogiCardExpiry::TurnEnd);
	const int32 Ep = A.ContactEpisode(0, 2, 1.f);
	const uint64 Cid = FZoogiCardEffectCore::ContactId(0, 2, Ep);
	TestTrue(TEXT("first apply"), A.ApplyOnce(11, Cid));
	A.TryBurst(Cid); A.AddFrostLane(FVector::ZeroVector, FVector::ForwardVector, 6); A.SaveMass(1, 42.f); A.ScoreOrbOnce(0, 0);
	Load(B, Save(A));
	TestEqual(TEXT("hash after load"), B.Hash(), A.Hash());
	TestFalse(TEXT("apply-once survives resume"), B.ApplyOnce(11, Cid));
	TestFalse(TEXT("orb scored once survives resume"), B.ScoreOrbOnce(0, 0));
	float M = 0.f; TestTrue(TEXT("saved mass survives"), B.RestoreMass(1, M) && M == 42.f);
	TestEqual(TEXT("same contact episode after resume"), B.ContactEpisode(0, 2, 1.05f), Ep);
	return true;
}

P6_TEST(EpisodeCoreSaveResume)
{
	FZoogiEpisodeCore A, B;
	A.Reset(4, true, { TEXT("Blue"), TEXT("Tavi"), TEXT("Vex"), TEXT("Moss") });
	FZoogiEpFact F; F.Kind = TEXT("ACTION"); F.Seq = 0; F.EventId = TEXT("ev_0000_action"); F.Actor = 0; F.Targets = { 2 }; F.bCollision = true; F.ActionId = 1;
	A.AddFact(F);
	FZoogiEpisode E; A.TrySpend(0, 100.f, 0, E);
	Load(B, Save(A));
	TestEqual(TEXT("hash after load"), B.Hash(), A.Hash());
	FZoogiEpisode X;
	TestFalse(TEXT("spend once survives resume"), B.TrySpend(0, 100.f, 0, X));
	TestFalse(TEXT("counted once survives resume"), B.AddFact(F));
	return true;
}

P6_TEST(DuplicateInput)
{
	FZoogiCardRules R; P6Make(R);
	R.BeginStandby(0, 1);
	R.BeginStandby(0, 1); // duplicate Standby: idempotent
	const FZoogiCardPlayerState* S = R.Player(0);
	TestEqual(TEXT("duplicate standby draws once"), S->Hand.Num(), 5);
	FZoogiCardContext C;
	const int32 Id = S->Hand[0].InstanceId;
	R.Select(0, Id, C);
	R.Select(0, Id, C); // the same selection delivered twice
	TestEqual(TEXT("duplicate select counted once"), S->Selected.Num(), 1);
	const int32 E0 = S->Energy;
	TestEqual(TEXT("first commit"), R.Commit(0, C), EZoogiCardResult::Ok);
	TestEqual(TEXT("duplicate commit rejected"), R.Commit(0, C), EZoogiCardResult::AlreadyCommitted);
	TestTrue(TEXT("spent once"), S->Energy < E0 && R.TotalSpent(0) == E0 - S->Energy);
	return true;
}

P6_TEST(RepeatedContact)
{
	FZoogiCardEffectCore C;
	const int32 Ep = C.ContactEpisode(1, 3, 2.0f);
	int32 Applied = 0, Bursts = 0;
	for (int32 i = 0; i < 100; ++i)
	{
		const float T = 2.0f + i * 0.001f; // 100 contact callbacks of one physical touch (within the episode gap)
		const int32 E = C.ContactEpisode(1, 3, T);
		TestEqual(TEXT("one contact episode"), E, Ep);
		Applied += C.ApplyOnce(77, FZoogiCardEffectCore::ContactId(1, 3, E)) ? 1 : 0;
		Bursts += C.TryBurst(FZoogiCardEffectCore::ContactId(1, 3, E)) ? 1 : 0;
	}
	TestEqual(TEXT("effect applied once across 100 callbacks"), Applied, 1);
	TestTrue(TEXT("double impact bursts capped at 2"), Bursts <= 2);
	TestTrue(TEXT("a later touch is a new episode"), C.ContactEpisode(1, 3, 3.0f) == Ep + 1);
	return true;
}

#endif
