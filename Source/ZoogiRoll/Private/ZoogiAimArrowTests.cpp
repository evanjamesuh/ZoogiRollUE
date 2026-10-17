// aim-arrow: ZoogiRoll.Aim.ArrowRotate - visibility, grab-and-rotate, drag-elsewhere launch power, hidden in Pinpoint / zoom.
// Pure logic (no world): the same functions the controller / HUD call (ZoogiAimArrow.h). The live counterpart with real
// touch events and screenshots is -ZoogiArrowTest (ARROWTEST PASS / FAIL lines).
#include "ZoogiAimArrow.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	constexpr EAutomationTestFlags ZoogiAimFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// top-down test camera: world +X = screen up, +Y = screen right, 0.5 px per cm, ball at the screen centre
	FVector2D TopDown(const FVector& W) { return FVector2D(960.f + W.Y * 0.5f, 540.f - W.X * 0.5f); }
	FVector2D Dir(float YawDeg) { const float R = FMath::DegreesToRadians(YawDeg); return FVector2D(FMath::Cos(R), FMath::Sin(R)); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiAimArrowRotateTest, "ZoogiRoll.Aim.ArrowRotate", ZoogiAimFlags)
bool FZoogiAimArrowRotateTest::RunTest(const FString& Parameters)
{
	using namespace ZoogiAimArrow;
	FTune T; // code defaults (shaft 0.3D, head 0.7D, length 1.75D, glow off, faces arrow on)
	TestFalse(TEXT("glow defaults OFF"), T.bGlow);
	TestTrue(TEXT("Zoogi faces arrow defaults ON"), T.bFaceArrow);

	// 1) visible at aim start (human shooter, Battle Aim step), not for CPU / outside the aim step
	FVisInput V; V.bEnabled = true; V.bHumanAimStep = true; V.bBallInPlay = true;
	TestTrue(TEXT("arrow visible at aim start"), ShouldShow(V));
	{ FVisInput C = V; C.bHumanAimStep = false; TestFalse(TEXT("hidden on CPU turns / outside the aim step"), ShouldShow(C)); }
	{ FVisInput C = V; C.bEnabled = false; TestFalse(TEXT("hidden with -ZoogiNoAimArrow"), ShouldShow(C)); }

	// shape: anchored at the ball's front base, constant size from the ball diameter (D = 100 cm)
	const float R = 50.f, D = 100.f;
	FVector W[7];
	Outline(FVector(0, 0, R), R, 0.f, T, W);
	TestEqual(TEXT("shaft starts at the ball's front base"), (float)W[0].X, R, 0.01f);
	TestEqual(TEXT("total length 1.75 D from the base"), (float)(W[3].X - W[0].X), 1.75f * D, 0.01f);
	TestEqual(TEXT("shaft width 0.3 D"), (float)FVector::Dist(W[0], W[6]), 0.3f * D, 0.01f);
	TestEqual(TEXT("arrowhead width 0.7 D"), (float)FVector::Dist(W[2], W[4]), 0.7f * D, 0.01f);
	TestEqual(TEXT("flat on the floor (lift only)"), (float)W[3].Z, T.LiftCm, 0.01f);
	FVector W2[7];
	Outline(FVector(300, -200, R), R, 0.f, T, W2);
	TestEqual(TEXT("length independent of position / power"), (float)FVector::Dist(W2[0], W2[3]) , (float)FVector::Dist(W[0], W[3]), 0.01f);

	// 2) grab on the arrowhead and drag round the Zoogi -> the aim yaw follows the finger's angle
	FVector2D S[7];
	for (int32 i = 0; i < 7; ++i) S[i] = TopDown(W[i]);
	const FVector2D BallS = TopDown(FVector::ZeroVector);
	const float BallRpx = R * 0.5f;
	const FVector2D HeadMid = (S[2] + S[3] + S[4]) / 3.f;
	TestTrue(TEXT("press on the arrowhead grabs"), HitTest(S, BallS, BallRpx, HeadMid, 1.f, T));
	TestTrue(TEXT("press just beside the arrowhead (generous area) grabs"), HitTest(S, BallS, BallRpx, HeadMid + FVector2D(55.f, 0.f), 1.f, T));
	TestFalse(TEXT("press on the Zoogi itself keeps the old press (bGrabOnZoogi off)"), HitTest(S, BallS, BallRpx, BallS, 1.f, T));
	const float HeadDist = 0.5f * (W[2].X + W[3].X); // cm from the ball centre along the arrow
	FGesture Gs;
	Gs.Begin(0.f, Dir(0.f), HeadMid, 800.f);
	const float Targets[] = { -40.f, -90.f, 40.f, 135.f };
	for (float Want : Targets)
	{
		// finger walks round the ball to Want (screen point -> world floor direction, as the controller derives it)
		for (int32 Step = 1; Step <= 10; ++Step)
		{
			const float A = FMath::Lerp(Gs.Yaw, Want, Step / 10.f);
			const FVector2D Finger = TopDown(FVector(Dir(A) * HeadDist, 0.f));
			const FVector2D Fw((540.f - Finger.Y) / 0.5f, (Finger.X - 960.f) / 0.5f);
			Gs.Move(Fw.GetSafeNormal(), Finger, T.RotateStartPx);
		}
		TestTrue(*FString::Printf(TEXT("drag rotates (to %.0f deg)"), Want), Gs.bRotating);
		TestEqual(*FString::Printf(TEXT("aim yaw follows the finger to %.0f deg"), Want), Gs.Yaw, FRotator::NormalizeAxis(Want), 0.05f);
	}
	TestFalse(TEXT("a rotate drag is not a tap"), Gs.End());
	// off-centre grab: relative mapping, no jump on press
	Gs.Begin(30.f, Dir(55.f), FVector2D(100, 100), 800.f);
	Gs.Move(Dir(55.f), FVector2D(130, 100), T.RotateStartPx);
	TestEqual(TEXT("off-centre grab does not jump the arrow"), Gs.Yaw, 30.f, 0.05f);
	Gs.Move(Dir(75.f), FVector2D(140, 110), T.RotateStartPx);
	TestEqual(TEXT("off-centre grab rotates by the finger's angle change"), Gs.Yaw, 50.f, 0.05f);
	Gs.End();
	// a press on the arrow released without moving stays the old tap (tap-to-target on release)
	Gs.Begin(0.f, Dir(0.f), HeadMid, 800.f);
	Gs.Move(Dir(0.f), HeadMid + FVector2D(2.f, 1.f), T.RotateStartPx);
	TestTrue(TEXT("press + release on the arrow without moving = tap (old tap-to-aim)"), Gs.End());

	// 3) a drag that starts elsewhere is the normal launch, power unchanged
	const FVector2D Elsewhere = BallS + FVector2D(-300.f, 220.f);
	TestFalse(TEXT("press away from the arrow does not grab (normal drag launch)"), HitTest(S, BallS, BallRpx, Elsewhere, 1.f, T));
	struct FCase { float Back, UPx, MaxPx, MaxF; } Cases[] = { { 130.f, 1.f, 260.f, 1.f }, { 0.f, 1.f, 260.f, 1.f }, { 400.f, 1.f, 260.f, 1.f }, { 130.f, 2.f, 260.f, 0.8f }, { 77.f, 1.333f, 300.f, 1.f } };
	for (const FCase& C : Cases)
	{
		const float Old = FMath::Min(C.Back / (C.MaxPx * C.UPx), 1.f) * C.MaxF; // the controller's pre-arrow formula
		TestEqual(*FString::Printf(TEXT("launch pull unchanged (back %.0f px, scale %.2f)"), C.Back, C.UPx), SlingshotPull(C.Back, C.UPx, C.MaxPx, C.MaxF), Old, 1e-6f);
	}
	TestEqual(TEXT("130 px back of 260 = 50% pull"), SlingshotPull(130.f, 1.f, 260.f, 1.f), 0.5f, 1e-6f);

	// 4) hidden in Pinpoint / target-choosing modes (current crosshair / tap-to-lock controls kept)
	{ FVisInput C = V; C.bPinpointArmed = true; TestFalse(TEXT("hidden with Pinpoint lock-on armed"), ShouldShow(C)); }
	{ FVisInput C = V; C.bZoomMode = true; TestFalse(TEXT("hidden in AIM ZOOM crosshair / tap-to-lock"), ShouldShow(C)); }
	return true;
}
#endif
