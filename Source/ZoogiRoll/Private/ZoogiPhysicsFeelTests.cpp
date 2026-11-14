// ZoogiRoll.Physics.Feel* — pure helpers for quality items 21 / 23 / 25 (no world).
#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	constexpr EAutomationTestFlags ZoogiFeelFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiFeelFlickSpin, "ZoogiRoll.Physics.Feel.FlickSpinCurve", ZoogiFeelFlags)
bool FZoogiFeelFlickSpin::RunTest(const FString&)
{
	using namespace ZoogiPhysics;
	TestEqual(TEXT("pure back-pull (side 0) = no spin"), FlickSpinFromSide(0.f), 0.f, 0.001f);
	TestEqual(TEXT("dead-zone keeps near-straight flicks straight"), FlickSpinFromSide(Feel::FlickSideDeadZone * 0.5f), 0.f, 0.001f);
	const float Full = FlickSpinFromSide(1.f);
	TestTrue(TEXT("full right flick arms readable spin"), Full > 1.f);
	TestEqual(TEXT("full left is opposite of full right"), FlickSpinFromSide(-1.f), -Full, 0.001f);
	TestEqual(TEXT("spin capped at FlickSpinMaxRad"), Full, Feel::FlickSpinMaxRad, 0.05f);

	const FVector2D Vel(2000.f, 0.f);
	const FVector2D Acc = FlickLateralAccel(Vel, Full);
	TestTrue(TEXT("positive yaw bends +Y for +X travel"), Acc.Y > 100.f);
	TestEqual(TEXT("no forward accel from flick"), (float)Acc.X, 0.f, 1.f);
	TestTrue(TEXT("lateral accel capped"), FMath::Abs(Acc.Y) <= Feel::FlickMaxLatAccel + 0.1f);

	const FVector2D Acc0 = FlickLateralAccel(Vel, 0.f);
	TestTrue(TEXT("no spin = no curve"), Acc0.IsNearlyZero());

	const float Decayed = FlickDecaySpin(Full, 0.75f);
	TestTrue(TEXT("spin decays over ~0.75s"), Decayed > 0.f && Decayed < Full * 0.55f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiFeelRails, "ZoogiRoll.Physics.Feel.AngleSensitiveRails", ZoogiFeelFlags)
bool FZoogiFeelRails::RunTest(const FString&)
{
	using namespace ZoogiPhysics;
	const FRailResponse Head = RailResponse(0.95f);
	const FRailResponse Glance = RailResponse(0.15f);
	const FRailResponse Mid = RailResponse(0.60f);
	TestTrue(TEXT("head-on keeps/boosts normal rebound"), Head.NormalScale >= 1.f);
	TestTrue(TEXT("glancing damps normal rebound"), Glance.NormalScale < 0.5f);
	TestTrue(TEXT("glancing sticks tangentially more than head-on"), Glance.TangentScale < Head.TangentScale);
	TestTrue(TEXT("mid incidence interpolates"), Mid.NormalScale > Glance.NormalScale && Mid.NormalScale < Head.NormalScale);
	TestTrue(TEXT("head-on exit faster than glancing on normal"), Head.NormalScale > Glance.NormalScale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiFeelMassSkid, "ZoogiRoll.Physics.Feel.HeavyGrindLightSkid", ZoogiFeelFlags)
bool FZoogiFeelMassSkid::RunTest(const FString&)
{
	using namespace ZoogiPhysics;
	const float LightBlend = MassRollBlend(Feel::MassMin);
	const float HeavyBlend = MassRollBlend(Feel::MassMax);
	const float MidBlend = MassRollBlend(1.f);
	TestTrue(TEXT("light blends slower toward perfect roll (more skid)"), LightBlend < 0.35f);
	TestTrue(TEXT("heavy blends hard toward perfect roll (grind)"), HeavyBlend > 0.95f);
	TestTrue(TEXT("mid mass between light and heavy"), MidBlend > LightBlend && MidBlend < HeavyBlend);

	const float LightDecel = MassDecelMul(Feel::MassMin);
	const float HeavyDecel = MassDecelMul(Feel::MassMax);
	TestTrue(TEXT("heavy slows a bit more (grind)"), HeavyDecel > 1.f);
	TestTrue(TEXT("light slows a bit less (skid)"), LightDecel < 1.f);
	TestTrue(TEXT("same MassRatio API matches ZoogiConst mass clamp"), Feel::MassMin == 0.55f && Feel::MassMax == 1.8f);

	// Same launch speed: heavier effective decel => shorter skid distance (v^2 / 2a).
	const float V0 = ZoogiConst::FullLaunchSpeed;
	const float Base = ZoogiConst::RollingDecel;
	const float DistLight = (V0 * V0) / (2.f * Base * LightDecel);
	const float DistHeavy = (V0 * V0) / (2.f * Base * HeavyDecel);
	TestTrue(TEXT("light skids farther than heavy for same launch"), DistLight > DistHeavy);
	return true;
}

#endif