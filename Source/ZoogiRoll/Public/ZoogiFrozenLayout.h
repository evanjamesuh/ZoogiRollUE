#pragma once

#include "CoreMinimal.h"

/**
 * Frozen Ring layout (Oct 6, Buzzy art). UE yaw: 0 = +X (art right), 90 = +Y (art down), 270 = -Y (art up).
 * Shared by the arena build (both modes) and the 2v2 combo layout so gameplay props and visuals agree.
 */
namespace ZoogiFrozen
{
	constexpr float SpawnR = 980.f;            // classic start pads sit on the painted sockets
	constexpr float HubR = 680.f;               // pillar / obelisk ring
	constexpr float PillarYaw0 = 22.5f;         // 4 Resonance pillars (indestructible): 22.5 + 90k
	constexpr float ObeliskYaw0 = 67.5f;        // 4 ice obelisks (destructible): 67.5 + 90k
	constexpr float PillarHalf = 36.f;          // collider half width
	constexpr float PillarH = 350.f;
	constexpr float ObeliskR = 32.f;
	constexpr float ObeliskH = 170.f;
	// Curved ice ricochet walls (inner face radius, arcs in yaw degrees). N arc = the combo bank wall.
	constexpr float WallR = 1455.f;
	constexpr float WallThick = 34.f;
	constexpr float WallH = 95.f;
	constexpr float WallNA0 = 262.f, WallNA1 = 300.f;
	// Bridge alignment (Evan Oct 8 10:49 PM): every bridge gets a clear opening in the ring wall centred on its yaw,
	// +/- BridgeWallGapDeg (rails + Zoogi margin). W was 166-194 (closed the yaw-186 bridge), E was 330-352 (pinched 354).
	constexpr float BridgeWallGapDeg = 6.5f;
	// Symmetric layout (Evan Oct 9 12:30 / 12:33 AM): 6 identical bridges, 3 per side, mirrored about the camera axis
	// (yaw 90 / 270 stay open): 0 / 45 / 315 east, 135 / 180 / 225 west. Each has a centred gap of +/- BridgeWallGapDeg;
	// every other old gap is walled. N = combo bank wall (unchanged).
	constexpr float WallWA0 = 141.5f, WallWA1 = 173.5f;
	constexpr float WallEA0 = 321.5f, WallEA1 = 353.5f;
	// Extra breakable ice-wall arcs (same WallR / style, Polish r2) so the whole ring edge is walled except the 4 bridge gaps.
	// Each pair = yaw from / to (deg).
	inline const FVector2D WallFillArcs[5] = { FVector2D(6.5f, 38.5f), FVector2D(51.5f, 128.5f), FVector2D(186.5f, 218.5f), FVector2D(231.5f, 262.f), FVector2D(300.f, 308.5f) }; // gaps only at the 6 bridges +/- 6.5
	// Rim arches: one identical ruin arch centred on each bridge (future Boost Portals, aerial worker): clear opening
	// 2 * (BridgeArchHalfW - ArchPostR) = 252 cm, springline z 150, crown soffit ~z 280.
	constexpr float ArchR = 1590.f;
	constexpr float ArchYaw[6] = { 0.f, 45.f, 135.f, 180.f, 225.f, 315.f }; // one identical arch per bridge (6); old gate / ramp arch spots reused by bridges 45 / 225
	constexpr float ArchHalfW = 135.f;
	constexpr float BridgeArchHalfW = 150.f;     // bridge arches: post collider inner face 126 cm clears the deck rails (107)
	constexpr float ArchPostR = 24.f;
	constexpr float ArchH = 270.f;
	// Snow chute launch ramp: centre, yaw of the rise, length, width, lip height. Symmetric layout: it sits on the 225
	// bridge centreline (r750-1050), rising toward that bridge / arch (lip 485 cm before the deck starts).
	constexpr float RampCX = -636.4f, RampCY = -636.4f, RampYaw = 225.f, RampLen = 300.f, RampWid = 210.f, RampH = 45.f;
	// NE raised hex-ice platform.
	constexpr float PlatX = 1062.5f, PlatY = -440.1f, /* symmetric layout: off the 315 bridge lane, r1150 yaw 337.5 (between the 315 / 0 bridges) */ PlatR = 236.f, PlatH = 50.f;
	// SW hex-ice reactive floor section (combo).
	constexpr float SecX0 = -1050.f, SecY0 = 550.f, SecX1 = -650.f, SecY1 = 950.f;
	// Combo bumpers (ice crystal spires).
	constexpr float BumpX[3] = { 1150.f, -400.f, -1150.f };
	constexpr float BumpY[3] = { 350.f, 1200.f, -450.f };
	// Recovery gate and its return pad (ZoogiComboArena). Symmetric layout: no dedicated gate arch any more; 45 is now the
	// 45-bridge arch. The aerial worker relocates the Recovery Gate (see frozen-polish READY.txt).
	constexpr float GateYaw = 90.f; // Oct 9 (aerial worker): off the 6 bridge lanes, on the camera-side axis (return pad r1230 yaw 90; clear of every bridge)
	constexpr float GateReturnR = 1230.f;
	// Ice boulders (AZoogiRock, 4-stage): yaw, distance (web units), radius, height (web units).
	constexpr float RockYaw[3] = { 22.5f, 75.f, 250.f }; // symmetric layout: the 5 boulder moved off the 0 bridge lane
	constexpr float RockDist = 14.0f, RockRad = 0.55f, RockH = 1.5f;
	// Stand islets: yaw, distance, and whether a pavilion stands behind the bleachers.
	constexpr float IsletYaw[6] = { 180.f, 0.f, 135.f, 45.f, 225.f, 315.f }; // kept (nudged): 186->180, 354->0, 140->135, 36->45; added 225 (old snow-chute arch 230) + 315 (old 310 arch)
	constexpr float IsletDist[6] = { 2750.f, 2750.f, 2750.f, 2750.f, 2750.f, 2750.f }; // identical islets / bridge lengths
	constexpr bool IsletPavilion[6] = { false, false, false, false, false, false };
	constexpr float IsletR = 640.f;              // radial semi-axis (toward the court)
	constexpr float IsletStretch = 1.4f;         // tangential semi-axis = IsletR * IsletStretch (elliptic islet)
	constexpr float IsletTopZ = -60.f;           // islet snow top (walkable, collision)
	constexpr float IsletLipH = 30.f;            // low snow lip rail: stops normal rolls, hard hits hop it (ring out)
	// Open curved bleachers (round 3c, no roof): 6 rows, focus distance (curvature), row depth / rise, front half width.
	constexpr int32 StandRows = 6;
	constexpr float StandX0 = 200.f, StandFocus = 2300.f, StandDepth = 110.f, StandRise = 52.f, StandHalfW = 560.f;
	// Rollable bridges (court edge -> islet apron): deck half width, low rail height, islet lip opening (deg, local).
	constexpr float BridgeHalfW = 95.f, BridgeRailH = 30.f, BridgeGapDeg = 14.f;
	inline bool NearIsletYaw(float A, float Tol) { for (float Y : IsletYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, Y)) < Tol) return true; return false; }
	// NW snowy outcrop and the two rim falls.
	constexpr float OutcropYaw = 270.f, /* symmetric layout: moved between the 225 / 315 islets */ OutcropDist = 1980.f;
	constexpr float FallYaw[2] = { 112.5f, 67.5f }; // symmetric about the camera axis (yaw 90)

	// ---- Outer action ring (Ring 2), OUTER_RING_PLAN.md. Oct 15 (Evan: "Package now, outer ring next build"): ON by default.
	// [ZoogiRoll.Frozen] bRing2 (True), -ZoogiFrozenRing2=Off|On. Off = the pre-Ring-2 Frozen map exactly.
	ZOOGIROLL_API bool Ring2On();
	constexpr float R2CrevIn = 1550.f, R2In = 1750.f, R2Out = 2060.f, R2Z = -30.f, R2OutR = 2300.f;
	constexpr float R2RampYaw[6] = { 30.f, 67.5f, 112.5f, 150.f, 240.f, 300.f };
	constexpr float R2RampGapDeg = 4.f;                  // Ring 1 wall gap (+/-) at each re-entry ramp
	constexpr float R2IsletDist = 2900.f;                // islets move out 150 (bridges r1535 -> r2285)
	constexpr float R2BridgeHalfW = 110.f;
	constexpr float R2OutcropDist = 2650.f;
	constexpr float R2FallYaw[2] = { 100.f, 80.f };      // rim falls pour into the crevasse (were 112.5 / 67.5 = ramp yaws)
	constexpr float R2HaloYaw[4] = { 0.f, 90.f, 180.f, 270.f }; // one aerial access point (halo) per side above Ring 2
	ZOOGIROLL_API const TArray<FVector2D>& RimGapArcs(); // Ring 1 rim gap arcs (deg A0..A1), empty when -ZoogiFrozenRimGaps=Off
	inline float IsletD(int32 k) { return Ring2On() ? R2IsletDist : IsletDist[k]; }
}
