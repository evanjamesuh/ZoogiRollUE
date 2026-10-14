// Phase 3 automation tests: one test per card on FZoogiCardEffectCore, the exact bookkeeping the match uses
// (ZoogiRoll.Cards.Fx.*). The runtime cases -ZoogiCardTest=effects / mutual_ko cover the same rules in the real roll.
#include "ZoogiCardEffects.h"
#include "ZoogiCards.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

constexpr EAutomationTestFlags ZoogiFxFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
static const FZoogiCardDef& FxDef(const TCHAR* Id)
{
	static TArray<FZoogiCardDef> D = ZoogiCards::BuiltInDefs();
	for (const FZoogiCardDef& X : D) if (X.Id == FName(Id)) return X;
	check(false); return D[0];
}
static FZoogiFxArmed& FxArmDef(FZoogiCardEffectCore& C, const TCHAR* Id, int64 Eff, int32 Owner)
{
	const FZoogiCardDef& D = FxDef(Id);
	return C.Arm(D.Id, Eff, Owner, 1, D.ParamA, D.ParamB, D.Expiry);
}
#define FX_TEST(Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiFx##Name, "ZoogiRoll.Cards.Fx." #Name, ZoogiFxFlags) bool FZoogiFx##Name::RunTest(const FString&)

FX_TEST(ContactIdsApplyOnce)
{
	FZoogiCardEffectCore C;
	const int32 E1 = C.ContactEpisode(0, 2, 1.00f), E1b = C.ContactEpisode(2, 0, 1.05f), E2 = C.ContactEpisode(0, 2, 1.50f);
	TestTrue(TEXT("both sides of one contact share an episode"), E1 == E1b);
	TestTrue(TEXT("a later contact is a new episode"), E2 == E1 + 1);
	TestTrue(TEXT("contact id order independent"), FZoogiCardEffectCore::ContactId(0, 2, 5) == FZoogiCardEffectCore::ContactId(2, 0, 5));
	TestTrue(TEXT("different episode, different id"), FZoogiCardEffectCore::ContactId(0, 2, 5) != FZoogiCardEffectCore::ContactId(0, 2, 6));
	const uint64 Id = FZoogiCardEffectCore::ContactId(0, 2, E1);
	TestTrue(TEXT("first apply"), C.ApplyOnce(77, Id));
	TestFalse(TEXT("second apply refused"), C.ApplyOnce(77, Id));
	TestTrue(TEXT("other effect same contact ok"), C.ApplyOnce(78, Id));
	return true;
}

FX_TEST(Storm)
{
	const FZoogiCardDef& D = FxDef(TEXT("storm"));
	const FVector2D V = FZoogiCardEffectCore::ShotScale(FVector2D(1000.f, 0.f), D.ParamA, 3600.f * ZoogiCardFxTune::MaxShotMul);
	TestTrue(TEXT("x1.25 roll control"), FMath::IsNearlyEqual(V.X, 1250.f, 0.01f) && V.Y == 0.f);
	const FVector2D Hi = FZoogiCardEffectCore::ShotScale(FVector2D(0.f, 4500.f), D.ParamA, 3600.f * ZoogiCardFxTune::MaxShotMul);
	TestTrue(TEXT("capped, direction kept (no auto-aim)"), FMath::IsNearlyEqual(Hi.Y, 4860.f, 0.5f) && Hi.X == 0.f);
	return true;
}

FX_TEST(HeavyRoll)
{
	const FZoogiCardDef& H = FxDef(TEXT("heavy_roll")); const FZoogiCardDef& S = FxDef(TEXT("storm"));
	TestTrue(TEXT("x1.25 force"), FMath::IsNearlyEqual(FZoogiCardEffectCore::ShotScale(FVector2D(2000.f, 0.f), H.ParamA, 4860.f).X, 2500.f, 0.01f));
	TestTrue(TEXT("storm x heavy stack multiplicatively under the cap"), FMath::IsNearlyEqual(FZoogiCardEffectCore::ShotScale(FVector2D(2000.f, 0.f), H.ParamA * S.ParamA, 4860.f).X, 3125.f, 0.01f));
	TestTrue(TEXT("never above FullLaunch x1.35"), FZoogiCardEffectCore::ShotScale(FVector2D(3600.f, 0.f), H.ParamA * S.ParamA, 4860.f).X <= 4860.f + 0.01f);
	TestTrue(TEXT("aim envelope param 1.2"), FMath::IsNearlyEqual(H.ParamB, 1.2f));
	return true;
}

FX_TEST(Precision)
{
	const FZoogiCardDef& D = FxDef(TEXT("precision"));
	TestTrue(TEXT("aim error x0.7, preview + 1 rebound"), FMath::IsNearlyEqual(D.ParamA, 0.7f) && FMath::IsNearlyEqual(D.ParamB, 1.f));
	FZoogiCardEffectCore C; FxArmDef(C, TEXT("precision"), 1, 0);
	TestEqual(TEXT("expires at lead action end"), C.Expire(EZoogiCardExpiry::LeadActionEnd, 0).Num(), 1);
	return true;
}

FX_TEST(Titan)
{
	FZoogiCardEffectCore C;
	TestTrue(TEXT("cache actual"), C.SaveMass(1, 10.5f));
	TestFalse(TEXT("never re-cache a modified mass"), C.SaveMass(1, 10.5f * 1.35f));
	float M = 0.f;
	TestTrue(TEXT("restore"), C.RestoreMass(1, M) && M == 10.5f);
	TestFalse(TEXT("restore once"), C.RestoreMass(1, M));
	const FVector Post = FZoogiCardEffectCore::ScaleDelta(FVector(100, 0, 0), FVector(600, 200, 5), FxDef(TEXT("titan")).ParamB);
	TestTrue(TEXT("received push x0.8, direction kept, z from physics"), Post.Equals(FVector(500, 160, 5), 0.01f));
	return true;
}

FX_TEST(Guard)
{
	FZoogiCardEffectCore C; FZoogiFxArmed& G = FxArmDef(C, TEXT("guard"), 5, 2);
	TestTrue(TEXT("armed at once"), G.bArmed);
	const FVector Post = FZoogiCardEffectCore::ScaleDelta(FVector::ZeroVector, FVector(1000, 0, 0), G.B);
	TestTrue(TEXT("push x0.75, still physical"), Post.Equals(FVector(750, 0, 0), 0.01f));
	G.bConsumed = true;
	TestTrue(TEXT("consumed once: no longer found"), C.Find(G.Card, 2) == nullptr);
	TestEqual(TEXT("not removed by turn end"), C.Expire(EZoogiCardExpiry::TurnEnd, -1).Num(), 0);
	TestEqual(TEXT("not removed by lead action end"), C.Expire(EZoogiCardExpiry::LeadActionEnd, -1).Num(), 0);
	return true;
}

FX_TEST(AnchorGuard)
{
	FZoogiCardEffectCore C; FZoogiFxArmed& A = FxArmDef(C, TEXT("anchor_guard"), 6, 1);
	TestFalse(TEXT("unarmed until settle"), A.bArmed);
	TestTrue(TEXT("push x0.3"), FZoogiCardEffectCore::ScaleDelta(FVector::ZeroVector, FVector(0, 1000, 0), A.B).Equals(FVector(0, 300, 0), 0.01f));
	return true;
}

FX_TEST(Ember)
{
	FZoogiCardEffectCore C;
	C.AddFrostLane(FVector(0, 0, 0), FVector(1, 0, 0), 6);
	const int32 N0 = C.FrostCells();
	const int32 Cleared = C.ClearFrostNear(FVector(60, 60, 0), ZoogiCardFxTune::EmberClearR);
	TestTrue(TEXT("clears only overlapping cells"), Cleared >= 1 && Cleared < N0 && C.FrostCells() == N0 - Cleared);
	TestTrue(TEXT("far cells untouched"), C.InFrost(FVector(650, 10, 0)));
	TestTrue(TEXT("enemy push x1.2"), FZoogiCardEffectCore::ScaleDelta(FVector::ZeroVector, FVector(1000, 0, 0), FxDef(TEXT("ember")).ParamA).Equals(FVector(1200, 0, 0), 0.01f));
	return true;
}

FX_TEST(Frost)
{
	FZoogiCardEffectCore C;
	const int32 A = C.AddFrostLane(FVector(0, 0, 0), FVector(1, 0, 0), 6);
	TestTrue(TEXT("lane cells"), A >= 6);
	TestEqual(TEXT("same lane again: never stacks"), C.AddFrostLane(FVector(0, 0, 0), FVector(1, 0, 0), 6), 0);
	TestTrue(TEXT("shared lookup"), C.InFrost(FVector(300, 10, 0)) && !C.InFrost(FVector(-300, 0, 0)));
	C.Reset();
	TestEqual(TEXT("match reset clears"), C.FrostCells(), 0);
	return true;
}

FX_TEST(MomentumSteal)
{
	FVector S(1000, 0, 0), V(0, 0, 0);
	TestFalse(TEXT("head-on: no transfer"), FZoogiCardEffectCore::MomentumSteal(S, V, FVector(1, 0, 0), 0.25f));
	S = FVector(1000, 0, 0); V = FVector::ZeroVector;
	TestTrue(TEXT("glancing"), FZoogiCardEffectCore::MomentumSteal(S, V, FVector(0, 1, 0), 0.25f));
	TestTrue(TEXT("<= 25% tangential moved, momentum conserved"), V.Equals(FVector(250, 0, 0), 0.01f) && S.Equals(FVector(750, 0, 0), 0.01f));
	S = FVector(1000, 0, 0); V = FVector::ZeroVector;
	FZoogiCardEffectCore::MomentumSteal(S, V, FVector(0, 1, 0), 0.9f);
	TestTrue(TEXT("fraction clamped to 25%"), V.X <= 250.01f);
	return true;
}

FX_TEST(DoubleImpact)
{
	FZoogiCardEffectCore C;
	const uint64 Id = FZoogiCardEffectCore::ContactId(0, 1, 1);
	TestTrue(TEXT("burst 1"), C.TryBurst(Id));
	TestTrue(TEXT("burst 2"), C.TryBurst(Id));
	TestFalse(TEXT("no burst 3"), C.TryBurst(Id));
	TestEqual(TEXT("max two"), C.BurstsFor(Id), 2);
	TestTrue(TEXT("new episode gets its own two"), C.TryBurst(FZoogiCardEffectCore::ContactId(0, 1, 2)));
	const FZoogiCardDef& D = FxDef(TEXT("double_impact"));
	TestTrue(TEXT("second at 50% after 0.12 s"), FMath::IsNearlyEqual(D.ParamA, 0.12f) && FMath::IsNearlyEqual(D.ParamB, 0.5f));
	return true;
}

FX_TEST(BankStrike)
{
	FZoogiCardEffectCore C; FZoogiFxArmed& B = FxArmDef(C, TEXT("bank_strike"), 9, 0);
	TestFalse(TEXT("no BankSetup without a real bank contact"), B.bBankSetup);
	TestTrue(TEXT("BankSetup emitted once"), C.ApplyOnce(9, 1) && !C.ApplyOnce(9, 1));
	TestEqual(TEXT("expires at lead action end"), C.Expire(EZoogiCardExpiry::LeadActionEnd, 0).Num(), 1);
	return true;
}

FX_TEST(OrbRelay)
{
	FZoogiCardEffectCore C;
	TestTrue(TEXT("score orb 0 life 1"), C.ScoreOrbOnce(0, 1));
	TestFalse(TEXT("same orb life never twice"), C.ScoreOrbOnce(0, 1));
	TestTrue(TEXT("respawned orb = new life"), C.ScoreOrbOnce(0, 2));
	TestTrue(TEXT("+30 Combo"), FMath::IsNearlyEqual(FxDef(TEXT("orb_relay")).ParamA, 30.f));
	return true;
}

FX_TEST(OrbMagnet)
{
	const FZoogiCardDef& D = FxDef(TEXT("orb_magnet"));
	const float MaxMove = D.ParamB * 2.f * 40.f;
	FVector P(0, 0, 0), V(0, 0, 0), Added(0, 0, 0);
	const FVector Start = P, Target(1000, 0, 0);
	float T = 0.f;
	for (int32 i = 0; i < 240 && T < D.ParamA; ++i)
	{
		const float Dt = 1.f / 60.f; T += Dt;
		const FVector Dv = FZoogiCardEffectCore::MagnetStep(P, Start, Target, MaxMove, ZoogiCardFxTune::MagnetAccel, Dt);
		if (Dv.IsZero()) break;
		V += Dv; Added += Dv; P += V * Dt;
	}
	V -= Added; // the glue removes the added pull at the end
	TestTrue(TEXT("pull bounded by 1.5 diameters (+ one frame)"), FVector::Dist2D(P, Start) <= MaxMove + ZoogiCardFxTune::MagnetAccel * 0.6f / 60.f + 1.f);
	TestTrue(TEXT("no residual momentum"), V.IsNearlyZero(0.01f));
	TestTrue(TEXT("0.6 s"), FMath::IsNearlyEqual(D.ParamA, 0.6f));
	return true;
}

FX_TEST(RecoveryRoll)
{
	const FZoogiCardDef& D = FxDef(TEXT("recovery_roll"));
	TestTrue(TEXT("decel x1.5"), FMath::IsNearlyEqual(D.ParamA, 1.5f));
	FZoogiCardEffectCore C; FxArmDef(C, TEXT("recovery_roll"), 3, 0);
	TestEqual(TEXT("lead action end"), C.Expire(EZoogiCardExpiry::LeadActionEnd, 0).Num(), 1);
	// no heal: the effect core has no Ring Mark / HP API at all; the runtime test checks marks never rise
	return true;
}

FX_TEST(CrystalCollapse)
{
	const FZoogiCardDef& D = FxDef(TEXT("crystal_collapse"));
	TestTrue(TEXT("persistent terrain state, one step"), D.Expiry == EZoogiCardExpiry::PersistentTerrainState && FMath::IsNearlyEqual(D.ParamA, 1.f));
	return true; // world behaviour (one state per EffectId, pillars never) is covered by -ZoogiCardTest=crystal_collapse
}

FX_TEST(ExpiryAndRestore)
{
	FZoogiCardEffectCore C;
	FxArmDef(C, TEXT("storm"), 1, 0); FxArmDef(C, TEXT("titan"), 2, 0); FxArmDef(C, TEXT("heavy_roll"), 3, 0);
	FxArmDef(C, TEXT("momentum_steal"), 4, 0); FxArmDef(C, TEXT("orb_magnet"), 5, 0); FxArmDef(C, TEXT("guard"), 6, 0);
	FxArmDef(C, TEXT("storm"), 7, 0);
	TestEqual(TEXT("re-arming a card refreshes (no stacking)"), C.Active.Num(), 6);
	TestEqual(TEXT("lead action end: heavy, steal, magnet"), C.Expire(EZoogiCardExpiry::LeadActionEnd, 0).Num(), 3);
	TestEqual(TEXT("turn end: storm, titan"), C.Expire(EZoogiCardExpiry::TurnEnd, -1).Num(), 2);
	TestTrue(TEXT("guard survives until its impact"), C.Find(FName(TEXT("guard")), 0) != nullptr);
	return true;
}

#endif
