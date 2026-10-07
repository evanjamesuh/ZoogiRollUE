#pragma once
#include "CoreMinimal.h"
#include "ZoogiTypes.h"

/** Meadow Arena layout (hand-built, no Aura). Distances cm, yaws degrees. */
namespace ZoogiMeadow
{
	inline constexpr float FloorR = ZoogiConst::KnockoffRadius; // 1550

	// Resonance pillars (unbreakable): r 800 at diagonals
	inline constexpr float PillarR = 800.f;
	inline constexpr float PillarYaw[4] = { 40.f, 187.f, 310.f, 355.f }; // r5: measured off the Buzzy top-down
	inline constexpr float PillarH = 340.f;   // r3: Buzzy proportions (~3.4 Zoogi diameters)
	inline constexpr float PillarS = 1.3f;     // r3: cross-section scale vs r2

	// Stadium-seat wall (MEADOW ONLY) with 4 orb-exit gaps — keep clear of VFX/debris.
	// Gap width: OrbRadius 40 + ZoogiRadius 50 + margin 70 = 240 cm (~8.9 deg half at r 1550).
	inline constexpr float SeatWallH = 55.f;
	inline constexpr float SeatWallThick = 36.f;
	inline constexpr float OrbExitGapWidthCm = 240.f;
	inline constexpr float OrbExitGapHalfDeg = 8.9f;
	inline constexpr float OrbExitGapYaw[4] = { 0.f, 90.f, 180.f, 270.f };

	// r5 ramps, measured off the Buzzy top-down (screen up = -Y = yaw 270, screen right = +X = yaw 0; 1 art px ~ 4.37 cm).
	// Central spiral ramp: floor-level foot at yaw 125, climbs counter-clockwise (125 -> 0 -> -90) to the raised disc at yaw 270.
	inline constexpr float SpiralRIn = 300.f, SpiralROut = 600.f;   // r9: full art width (~3 m); orb ring moved out to r690
	inline constexpr float SpiralA0 = 125.f, SpiralA1 = -90.f;       // foot -> top
	inline constexpr float SpiralH = 140.f;
	inline constexpr float SpiralRailH = 55.f; // r6: the art's tall outer ring wall
	inline constexpr float HubWallA0 = 140.f, HubWallA1 = 255.f; // r6: west half of the yin-yang ring wall
	inline constexpr float DiscYaw = 270.f, DiscR = 450.f, DiscRad = 160.f;
	// r9: Meadow starting orb ring (15 evenly spread) between the spiral and the pillars, rotated clear of the pillars.
	inline constexpr float OrbRingR = 690.f, OrbRingYaw = 7.f;
	// Two tall wooden quarter-pipes flanking the north (270) gap, radial, rising outward.
	inline constexpr float PipeYaw[2] = { 250.f, 290.f };
	inline constexpr float PipeR0 = 1300.f, PipeRq = 520.f, PipeEndDeg = 70.f, PipeWid = 280.f;
	// Two low curved lips at the front stand ends (SW/SE), rising toward the stand end and curling inward.
	inline constexpr float LipYawLow[2] = { 115.f, 65.f }, LipYawHigh[2] = { 142.f, 38.f };
	inline constexpr float LipRLow = 1470.f, LipRHigh = 1290.f, LipWid = 170.f, LipH = 100.f;
	// Wooden bench blocks (breakable) in the mid ring.
	inline constexpr int32 NumBenches = 7;
	inline constexpr float BenchYaw[NumBenches] = { 235.f, 211.f, 331.f, 163.f, 115.f, 67.f, 19.f }; // r9: snapped (<=6 deg) to gaps between ring orbs
	inline constexpr float BenchR = 700.f;

	// Outer wooden stands: 4 arcs hugging the floor edge (art), gaps at 0/90/180/270 between them.
	inline constexpr float StandRIn = 1640.f;
	inline constexpr int32 NumStands = 4;
	inline constexpr float StandCenters[NumStands] = { 25.f, 155.f, 218.f, 322.f };
	inline constexpr float StandArcs[NumStands] = { 30.f, 30.f, 52.f, 52.f };
	inline constexpr int32 StandTiers = 5;

	// Recovery gate shrines
	inline constexpr float GateR = 1720.f;
	inline constexpr float GateYaw[2] = { 123.f, 56.f }; // r5: the two red shrine gates either side of the south (90) gap

	inline bool NearOrbExitGap(float A, float Tol = 0.f)
	{
		for (float G : OrbExitGapYaw)
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, G)) <= OrbExitGapHalfDeg + Tol) return true;
		return false;
	}
}
