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
	constexpr float WallWA0 = 166.f, WallWA1 = 194.f;
	constexpr float WallEA0 = 330.f, WallEA1 = 352.f;
	// Rim arches (the SE one is the Recovery Gate).
	constexpr float ArchR = 1590.f;
	constexpr float ArchYaw[4] = { 45.f, 140.f, 230.f, 310.f };
	constexpr float ArchHalfW = 135.f;
	constexpr float ArchPostR = 24.f;
	constexpr float ArchH = 270.f;
	// Snow chute launch ramp (NW, rises outward toward the arch): centre, yaw of the rise, length, width, lip height.
	constexpr float RampCX = -740.f, RampCY = -930.f, RampYaw = 231.5f, RampLen = 300.f, RampWid = 210.f, RampH = 45.f;
	// NE raised hex-ice platform.
	constexpr float PlatX = 820.f, PlatY = -800.f, PlatR = 236.f, PlatH = 50.f;
	// SW hex-ice reactive floor section (combo).
	constexpr float SecX0 = -1050.f, SecY0 = 550.f, SecX1 = -650.f, SecY1 = 950.f;
	// Combo bumpers (ice crystal spires).
	constexpr float BumpX[3] = { 1150.f, -400.f, -1150.f };
	constexpr float BumpY[3] = { 350.f, 1200.f, -450.f };
	// Recovery gate (SE arch) and its return pad.
	constexpr float GateYaw = 45.f;
	constexpr float GateReturnR = 1230.f;
	// Ice boulders (AZoogiRock, 4-stage): yaw, distance (web units), radius, height (web units).
	constexpr float RockYaw[3] = { 5.f, 75.f, 250.f };
	constexpr float RockDist = 14.0f, RockRad = 0.55f, RockH = 1.5f;
	// Stand islets: yaw, distance, and whether a pavilion stands behind the bleachers.
	constexpr float IsletYaw[4] = { 186.f, 354.f, 140.f, 36.f };
	constexpr float IsletDist[4] = { 2580.f, 2720.f, 2360.f, 2520.f };
	constexpr bool IsletPavilion[4] = { false, false, true, true };
	constexpr float IsletR = 470.f;
	// NW snowy outcrop and the two rim falls.
	constexpr float OutcropYaw = 242.f, OutcropDist = 1980.f;
	constexpr float FallYaw[2] = { 126.f, 62.f };
}
