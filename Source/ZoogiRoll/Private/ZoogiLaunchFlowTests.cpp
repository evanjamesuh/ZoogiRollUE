// ZoogiRoll.LaunchFlow.* - match-start launch flow and respawn-to-pad (pure logic, no world; the game mode drives the
// same FZoogiLaunchFlow / ZoogiLaunchPads calls).
#include "ZoogiLaunchFlow.h"
#include "ZoogiTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	constexpr EAutomationTestFlags ZoogiLFFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// Plays an opening for N players from First; returns the turn order seen until gameplay starts plus the next normal turn.
	TArray<int32> PlayOpening(FZoogiLaunchFlow& F, int32 N, int32 First)
	{
		F.BeginOpeningWithFirst(N, First);
		TArray<int32> Order;
		int32 P = First;
		for (int32 Guard = 0; Guard < 16 && F.bOpening; ++Guard) { Order.Add(P); P = F.OnOpeningTurnEnded(P); }
		Order.Add(P);
		return Order;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFRandomFirst, "ZoogiRoll.LaunchFlow.RandomFirstThenOrder", ZoogiLFFlags)
bool FZoogiLFRandomFirst::RunTest(const FString&)
{
	TSet<int32> Firsts;
	for (int32 Seed = 1; Seed <= 64; ++Seed)
	{
		FRandomStream R(Seed);
		FZoogiLaunchFlow F;
		F.BeginOpening(4, R);
		TestTrue(TEXT("first in range"), F.FirstPlayer >= 0 && F.FirstPlayer < 4);
		Firsts.Add(F.FirstPlayer);
	}
	TestEqual(TEXT("every player can go first"), Firsts.Num(), 4);
	FZoogiLaunchFlow F;
	const TArray<int32> O = PlayOpening(F, 4, 2);
	TestTrue(TEXT("order from random first"), (O) == TArray<int32>({ 2, 3, 0, 1, 2 }));
	const TArray<int32> O3 = PlayOpening(F, 3, 0);
	TestTrue(TEXT("3 players from P1"), (O3) == TArray<int32>({ 0, 1, 2, 0 }));
	TestTrue(TEXT("round starts with the first launcher"), F.IsRoundStart(0) && !F.IsRoundStart(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFAllLaunch, "ZoogiRoll.LaunchFlow.AllLaunchBeforeGameplay", ZoogiLFFlags)
bool FZoogiLFAllLaunch::RunTest(const FString&)
{
	FZoogiLaunchFlow F;
	F.BeginOpeningWithFirst(4, 1);
	TestTrue(TEXT("opening"), F.bOpening);
	int32 P = 1;
	for (int32 i = 0; i < 3; ++i)
	{
		P = F.OnOpeningTurnEnded(P);
		TestFalse(TEXT("no gameplay before all launched"), F.bGameplayStarted);
		TestTrue(TEXT("still opening"), F.bOpening);
	}
	P = F.OnOpeningTurnEnded(P);
	TestTrue(TEXT("gameplay after the last launch"), F.bGameplayStarted && !F.bOpening && F.AllLaunched());
	TestEqual(TEXT("normal order resumes at the first launcher"), P, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFEnergy, "ZoogiRoll.LaunchFlow.EnergyStartsAfterOpening", ZoogiLFFlags)
bool FZoogiLFEnergy::RunTest(const FString&)
{
	FZoogiLaunchFlow F;
	F.BeginOpeningWithFirst(2, 1);
	TestFalse(TEXT("energy off at opening start"), F.EnergyRunning());
	F.OnOpeningTurnEnded(1);
	TestFalse(TEXT("energy off mid opening"), F.EnergyRunning());
	F.OnOpeningTurnEnded(0);
	TestTrue(TEXT("energy on after opening"), F.EnergyRunning());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFKnockoff, "ZoogiRoll.LaunchFlow.KnockoffReturnsToPad", ZoogiLFFlags)
bool FZoogiLFKnockoff::RunTest(const FString&)
{
	const float KR = ZoogiConst::KnockoffRadius;
	TSet<FIntPoint> Seen;
	for (int32 p = 0; p < 4; ++p)
	{
		const FTransform Pad = ZoogiLaunchPads::GetPadTransform(nullptr, nullptr, p, 4); // no world -> stub pad
		const FVector L = Pad.GetLocation();
		TestTrue(TEXT("stub pad inside the knock-off ring"), FVector2D(L.X, L.Y).Size() < KR);
		TestTrue(TEXT("stub pad near the edge"), FVector2D(L.X, L.Y).Size() > KR * 0.5f);
		const FVector Fwd = Pad.GetRotation().Vector();
		TestTrue(TEXT("pad faces the centre"), FVector::DotProduct(Fwd, -FVector(L.X, L.Y, 0.f).GetSafeNormal()) > 0.99f);
		Seen.Add(FIntPoint(FMath::RoundToInt(L.X), FMath::RoundToInt(L.Y)));
	}
	TestEqual(TEXT("one pad per player"), Seen.Num(), 4);
	const FTransform S = ZoogiLaunchPads::StubPadTransform(1000.f, 0, 2, FVector(0, 500, 0), 50.f);
	TestTrue(TEXT("stub follows the spawn direction"), S.GetLocation().Equals(FVector(0, 1000.f - ZoogiLaunchPads::EdgeInset, 50.f), 0.5f));
	TestTrue(TEXT("knocked-off player returns"), (FZoogiLaunchFlow::PadReturns({ false, true, false })) == TArray<int32>({ 1 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFOthers, "ZoogiRoll.LaunchFlow.OthersStayPut", ZoogiLFFlags)
bool FZoogiLFOthers::RunTest(const FString&)
{
	TestEqual(TEXT("nobody out -> nobody moves"), FZoogiLaunchFlow::PadReturns({ false, false, false, false }).Num(), 0);
	const TArray<int32> R = FZoogiLaunchFlow::PadReturns({ true, false, false, true });
	TestTrue(TEXT("only the knocked-off ones"), (R) == TArray<int32>({ 0, 3 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFCrane, "ZoogiRoll.LaunchFlow.CraneHookFallback", ZoogiLFFlags)
bool FZoogiLFCrane::RunTest(const FString&)
{
	TestFalse(TEXT("unhandled hook -> place directly"), FZoogiLaunchFlow::ShouldWaitForCrane(false));
	TestTrue(TEXT("handled hook -> wait for the crane"), FZoogiLaunchFlow::ShouldWaitForCrane(true));
	FZoogiLaunchFlow F;
	F.BeginOpeningWithFirst(2, 0);
	F.BeginCarry(1, 2.f);
	TestTrue(TEXT("carry pending"), F.IsCarryPending(1));
	TestEqual(TEXT("not timed out yet"), F.TickCarries(1.f).Num(), 0);
	TestTrue(TEXT("crane callback completes"), F.CompleteCarry(1));
	TestFalse(TEXT("no longer pending"), F.IsCarryPending(1));
	TestEqual(TEXT("completed carry never falls back"), F.TickCarries(5.f).Num(), 0);
	F.BeginCarry(0, 2.f);
	TestTrue(TEXT("silent crane times out -> fallback"), (F.TickCarries(2.5f)) == TArray<int32>({ 0 }));
	TestFalse(TEXT("late callback is harmless"), F.CompleteCarry(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFNoArenaSwitch, "ZoogiRoll.LaunchFlow.NoArenaSwitchBetweenRounds", ZoogiLFFlags)
bool FZoogiLFNoArenaSwitch::RunTest(const FString&)
{
	TestFalse(TEXT("arena stays between rounds"), FZoogiLaunchFlow::ArenaSwitchesBetweenRounds());
	FZoogiLaunchFlow F;
	TArray<int32> O = PlayOpening(F, 3, 1);
	int32 P = O.Last(), Rounds = 0;
	for (int32 t = 0; t < 9; ++t) { if (F.IsRoundStart(P)) ++Rounds; P = FZoogiLaunchFlow::NextInOrder(P, 3); }
	TestEqual(TEXT("3 rounds in 9 normal turns"), Rounds, 3);
	TestFalse(TEXT("no opening again on later rounds"), F.bOpening);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFPadVsParked, "ZoogiRoll.LaunchFlow.PadVsParkedChoice", ZoogiLFFlags)
bool FZoogiLFPadVsParked::RunTest(const FString&)
{
	// Only the active seat sits on the pad; everyone else who needs the pad parks under it (no stacking on the deck).
	const TArray<bool> Need = { true, true, true, true };
	TestEqual(TEXT("active opener seats on pad"), FZoogiLaunchFlow::PadSeatPlayer(Need, 2), 2);
	TestEqual(TEXT("no active need -> nobody on pad"), FZoogiLaunchFlow::PadSeatPlayer({ true, false, true }, 1), -1);
	const TArray<int32> Park = FZoogiLaunchFlow::ParkSlots(Need, 0);
	TestEqual(TEXT("3 park under when P0 seats"), Park.Num(), 3);
	TestTrue(TEXT("park slots exclude the seat"), Park == TArray<int32>({ 1, 2, 3 }));
	const FVector Deck(1000.f, 2000.f, 500.f);
	const FVector A = ZoogiLaunchPads::ParkUnderPadLocation(Deck, 0);
	const FVector B = ZoogiLaunchPads::ParkUnderPadLocation(Deck, 1);
	const FVector C = ZoogiLaunchPads::ParkUnderPadLocation(Deck, 2);
	TestTrue(TEXT("park same XY as pad"), FMath::IsNearlyEqual(A.X, Deck.X) && FMath::IsNearlyEqual(A.Y, Deck.Y));
	TestTrue(TEXT("park Z well below deck"), A.Z < Deck.Z - ZoogiLaunchPads::ParkDepthBelowDeck + 1.f);
	TestTrue(TEXT("no stacking: distinct park Z"), !FMath::IsNearlyEqual(A.Z, B.Z) && !FMath::IsNearlyEqual(B.Z, C.Z) && !FMath::IsNearlyEqual(A.Z, C.Z));
	TestTrue(TEXT("park slots stay under the deck footprint"), FMath::IsNearlyEqual(B.X, Deck.X) && FMath::IsNearlyEqual(C.Y, Deck.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFNoPadStack, "ZoogiRoll.LaunchFlow.NoPadStacking", ZoogiLFFlags)
bool FZoogiLFNoPadStack::RunTest(const FString&)
{
	// Opening: one seat, N-1 parked. Returned mid-match with someone already seating: still one seat.
	for (int32 N : { 2, 3, 4 })
	{
		TArray<bool> Need; Need.Init(true, N);
		for (int32 Active = 0; Active < N; ++Active)
		{
			TestEqual(*FString::Printf(TEXT("N=%d active=%d one seat"), N, Active), FZoogiLaunchFlow::PadSeatPlayer(Need, Active), Active);
			TestEqual(*FString::Printf(TEXT("N=%d active=%d park count"), N, Active), FZoogiLaunchFlow::ParkSlots(Need, Active).Num(), N - 1);
		}
		Need[1] = false; // one already in play
		TestEqual(*FString::Printf(TEXT("N=%d partial seat"), N), FZoogiLaunchFlow::PadSeatPlayer(Need, 1), -1);
		// Seat P0, parks = everyone still needing except P0
		int32 ExpectPark = 0; for (int32 i = 0; i < N; ++i) if (Need[i] && i != 0) ++ExpectPark;
		TestEqual(*FString::Printf(TEXT("N=%d returned parks"), N), FZoogiLaunchFlow::ParkSlots(Need, 0).Num(), ExpectPark);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiLFPadBallistic, "ZoogiRoll.LaunchFlow.PadBallisticLaunch", ZoogiLFFlags)
bool FZoogiLFPadBallistic::RunTest(const FString&)
{
	// No real pad: stub 30 deg at ~1172 along the aim yaw.
	for (float Yaw : { 0.f, 90.f, -135.f })
	{
		const FVector V = ZoogiLaunchPads::PadLaunchVelocity(FVector::ZeroVector, Yaw);
		const float H = FVector2D(V.X, V.Y).Size();
		TestTrue(TEXT("upward component"), V.Z > 0.f);
		TestTrue(TEXT("~30 deg elevation"), FMath::IsNearlyEqual(FMath::RadiansToDegrees(FMath::Atan2(V.Z, H)), 30.f, 0.5f));
		TestTrue(TEXT("~1172 speed"), FMath::IsNearlyEqual(V.Size(), 1172.f, 1.f));
		TestTrue(TEXT("along the aim yaw"), FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)), Yaw), 0.f, 0.5f));
	}
	// Real pad velocity (facing +X, 30 deg, 1172): keeps speed / elevation, turned to the aim.
	const FVector Pad = FVector(FMath::Cos(PI / 6.f), 0.f, FMath::Sin(PI / 6.f)) * 1172.f;
	const FVector R = ZoogiLaunchPads::PadLaunchVelocity(Pad, 60.f);
	TestTrue(TEXT("pad speed kept"), FMath::IsNearlyEqual(R.Size(), Pad.Size(), 1.f));
	TestTrue(TEXT("pad elevation kept"), FMath::IsNearlyEqual(R.Z, Pad.Z, 1.f));
	TestTrue(TEXT("pad shot turned to aim"), FMath::IsNearlyEqual(FMath::RadiansToDegrees(FMath::Atan2(R.Y, R.X)), 60.f, 0.5f));
	// Ring-out suppressed until the first landing (contact right at the lip ignored), and the safety timeout.
	FZoogiLaunchFlow F;
	F.BeginPadAir(1);
	TestTrue(TEXT("airborne after launch"), F.IsPadAirborne(1));
	TestFalse(TEXT("other players unaffected"), F.IsPadAirborne(0));
	TestFalse(TEXT("lip contact ignored"), F.TickPadAir(1, 0.1f, true));
	TestTrue(TEXT("still airborne in flight"), F.IsPadAirborne(1) && !F.TickPadAir(1, 0.5f, false));
	TestTrue(TEXT("landing ends it"), F.TickPadAir(1, 0.1f, true) && !F.IsPadAirborne(1));
	// Solved launch: 30 deg, lands at the requested distance (checked by integrating the undamped arc), same for every yaw.
	{
		const float S = ZoogiLaunchPads::SolvePadLaunchSpeed(1800.f, 500.f, 980.f, 30.f);
		const float Th = FMath::DegreesToRadians(30.f), T = ZoogiLaunchPads::PadFlightTime(1800.f, 500.f, 980.f, 30.f);
		TestTrue(TEXT("solved speed positive"), S > 0.f && T > 1.f);
		TestTrue(TEXT("arc returns to floor at T"), FMath::IsNearlyEqual(500.f + S * FMath::Sin(Th) * T - 0.5f * 980.f * T * T, 0.f, 1.f));
		TestTrue(TEXT("arc covers the distance"), FMath::IsNearlyEqual(S * FMath::Cos(Th) * T, 1800.f, 1.f));
		TestTrue(TEXT("damping adds speed"), ZoogiLaunchPads::SolvePadLaunchSpeed(1800.f, 500.f, 980.f, 30.f, 0.02f) > S);
		TestEqual(TEXT("target distance wins"), ZoogiLaunchPads::PadLandingDistance(1800.f, 2250.f, 1550.f), 1800.f);
		TestEqual(TEXT("fallback 0.6 x centre distance"), ZoogiLaunchPads::PadLandingDistance(0.f, 2250.f, 1550.f), 1350.f);
		const float Far = ZoogiLaunchPads::PadLandingDistance(0.f, 5000.f, 1550.f);
		TestTrue(TEXT("fallback stays inside knock-off radius"), 5000.f - Far <= 1250.f + 0.1f);
		// along the aim exactly: straight at the target -> its distance; off-axis -> nearest point, kept inside SafeR
		TestTrue(TEXT("aim at target: full distance"), FMath::IsNearlyEqual(ZoogiLaunchPads::PadLandingDistanceAlong(FVector2D(-3000, 0), FVector2D(1, 0), FVector2D(-1000, 0), 1150.f), 2000.f, 0.5f));
		const float Off = ZoogiLaunchPads::PadLandingDistanceAlong(FVector2D(-3000, 0), FVector2D(1, 0.2f).GetSafeNormal(), FVector2D(0, 0), 1150.f);
		TestTrue(TEXT("off-axis aim still lands inside SafeR"), (FVector2D(-3000, 0) + FVector2D(1, 0.2f).GetSafeNormal() * Off).Size() <= 1150.f + 0.5f);
	}
	// Landing only counts past the peak and clear of the deck (deck 170 + Zoogi radius).
	const float ZR = ZoogiConst::ZoogiRadius;
	TestFalse(TEXT("rising: no landing"), FZoogiLaunchFlow::PadLandingEligible(200.f, 1500.f, 170.f, ZR));
	TestFalse(TEXT("over the deck: no landing"), FZoogiLaunchFlow::PadLandingEligible(-300.f, 170.f + ZR - 1.f, 170.f, ZR));
	TestTrue(TEXT("falling and clear of the deck: landing"), FZoogiLaunchFlow::PadLandingEligible(-300.f, 170.f + ZR + 1.f, 170.f, ZR));
	TestTrue(TEXT("at the peak and clear: landing"), FZoogiLaunchFlow::PadLandingEligible(0.f, 900.f, 170.f, ZR));
	F.BeginPadAir(2);
	TestFalse(TEXT("no contact before timeout"), F.TickPadAir(2, 3.9f, false));
	TestTrue(TEXT("timeout safety"), F.TickPadAir(2, 0.2f, false) && !F.IsPadAirborne(2));
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
