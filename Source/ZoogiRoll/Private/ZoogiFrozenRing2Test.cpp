// Frozen Ring 2 ring-out rule (Oct 14): out = fell below FallOutZ (crevasse / edges), or past r2300 (beyond Ring 2's outer edge).
#include "Misc/AutomationTest.h"
#include "ZoogiArena.h"
#include "ZoogiRoll.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiFrozenRing2RingOutTest, "ZoogiRoll.Frozen.Ring2RingOut", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FZoogiFrozenRing2RingOutTest::RunTest(const FString& Parameters)
{
	const float Saved = AZoogiArena::OuterOutR;
	AZoogiArena::OuterOutR = 2300.f;
	TestFalse(TEXT("court r1000 in play"), AZoogiArena::IsRingOut(FVector(1000.f, 0.f, 50.f)));
	TestFalse(TEXT("Ring 2 r1905 z-30 in play"), AZoogiArena::IsRingOut(FVector(0.f, 1905.f, -10.f)));
	TestFalse(TEXT("orb pocket r2200 in play"), AZoogiArena::IsRingOut(FVector(-2200.f, 0.f, -10.f)));
	TestTrue(TEXT("crevasse fall r1650 below FallOutZ is out"), AZoogiArena::IsRingOut(FVector(1650.f, 0.f, ZoogiConst::FallOutZ - 10.f)));
	TestFalse(TEXT("crevasse r1650 still falling (above FallOutZ) not yet out"), AZoogiArena::IsRingOut(FVector(1650.f, 0.f, -200.f)));
	TestTrue(TEXT("past Ring 2 outer edge r2350 is out"), AZoogiArena::IsRingOut(FVector(0.f, -2350.f, -10.f)));
	TestTrue(TEXT("islet r2900 is out"), AZoogiArena::IsRingOut(FVector(2900.f, 0.f, -40.f)));
	AZoogiArena::OuterOutR = 0.f; // other arenas: only the fall rule
	TestFalse(TEXT("rule off: r2350 on an islet stays in"), AZoogiArena::IsRingOut(FVector(0.f, -2350.f, -10.f)));
	TestTrue(TEXT("rule off: fall is out"), AZoogiArena::IsRingOut(FVector(0.f, 0.f, ZoogiConst::FallOutZ - 1.f)));
	AZoogiArena::OuterOutR = Saved;
	return true;
}
