#pragma once
#include "CoreMinimal.h"
#include "ZoogiTypes.h"

/** Meadow Arena layout (hand-built, no Aura). Distances cm, yaws degrees. */
namespace ZoogiMeadow
{
	inline constexpr float FloorR = ZoogiConst::KnockoffRadius; // 1550

	// Resonance pillars (unbreakable): r 800 at diagonals
	inline constexpr float PillarR = 800.f;
	inline constexpr float PillarYaw[4] = { 45.f, 135.f, 225.f, 315.f };
	inline constexpr float PillarH = 230.f;

	// Stadium-seat wall (MEADOW ONLY) with 4 orb-exit gaps — keep clear of VFX/debris.
	// Gap width: OrbRadius 40 + ZoogiRadius 50 + margin 70 = 240 cm (~8.9 deg half at r 1550).
	inline constexpr float SeatWallH = 55.f;
	inline constexpr float SeatWallThick = 36.f;
	inline constexpr float OrbExitGapWidthCm = 240.f;
	inline constexpr float OrbExitGapHalfDeg = 8.9f;
	inline constexpr float OrbExitGapYaw[4] = { 0.f, 90.f, 180.f, 270.f };

	// Curved ricochet wall arcs (inside disc) — stay clear of orb-exit gaps and spawn lanes.
	inline constexpr float WallR = 1150.f;
	inline constexpr float WallH = 85.f;
	inline constexpr float WallA0[2] = { 200.f, 20.f };   // two arcs
	inline constexpr float WallA1[2] = { 250.f, 70.f };

	// Launch ramp (+X) and raised platform
	inline constexpr float RampYaw = 0.f;
	inline constexpr float RampLen = 520.f;
	inline constexpr float RampWid = 280.f;
	inline constexpr float RampH = 95.f;
	inline constexpr float RampCX = 650.f;
	inline constexpr float PlatX = -720.f, PlatY = 420.f, PlatR = 160.f, PlatH = 110.f;

	// Outer wooden stand arcs
	inline constexpr float StandRIn = 2100.f;
	// A&P stand copies with the proposed gap moves (355 -> 335, 175 -> 235; 235 nudged to 230 to clear the 250 gate shrine).
	inline constexpr int32 NumStands = 6;
	inline constexpr float StandCenters[NumStands] = { 25.f, 115.f, 145.f, 205.f, 230.f, 335.f };
	inline constexpr float StandArcDeg = 30.f;
	inline constexpr int32 StandTiers = 3;

	// Recovery gate shrines
	inline constexpr float GateR = 2150.f;
	inline constexpr float GateYaw[2] = { 250.f, 290.f };

	inline bool NearOrbExitGap(float A, float Tol = 0.f)
	{
		for (float G : OrbExitGapYaw)
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, G)) <= OrbExitGapHalfDeg + Tol) return true;
		return false;
	}
}
