#pragma once
// Oct 27 (Evan's Build 9 corrections, Frozen Ring first): the outer ring is a free-rolling zone.
//   Match start: no ship / launch pad; every Zoogi stands on the Ring 2 floor in front of the bleachers (spawn points).
//   A Zoogi that has not entered the arena yet (match start, or after it was knocked out) rolls on its turn with the LEFT stick,
//   picks a bridge by rolling onto it, is caught and steadied on the bridge's launch point (span A, over the crevasse), then the
//   RIGHT stick pull-back + release launches it into the court. Later turns aim from where it stopped (classic).
//   States of the active Zoogi: SPAWNING -> FREE_ROAM -> BRIDGE_APPROACH -> LAUNCH_PREP -> LAUNCHED.
// Classic only (not 2v2 / royale). Off: [ZoogiRoll.Frozen] bOuterRoam=False or -ZoogiFrozenRoam=Off.
// Spawn points and bridge launch points are dev props ("spawn" spawn_1..4, "launch" launch_<yaw>): the Dev Mode object tools move
// and turn them, the layout saves / exports with the rest. A launch prop's Scale widens / narrows its gate.
#include "CoreMinimal.h"

enum class EZoogiRoamState : uint8 { None, Spawning, FreeRoam, BridgeApproach, LaunchPrep, Launched };

namespace ZoogiFrozenRoam
{
	/** Config switch (default on). */
	ZOOGIROLL_API bool ConfigOn();
	/** Oct 27 evening (Evan): the outer-ring roll / gate / bridge entry is OFF - players start on their spawn spots in the court
	 *  and aim with the classic aim bar. Kept behind [ZoogiRoll.Frozen] bBridgeEntry=True / -ZoogiFrozenBridges=On. */
	ZOOGIROLL_API bool BridgeEntryOn();
	ZOOGIROLL_API const TCHAR* StateName(EZoogiRoamState S);
	constexpr int32 NumBridges = 6;
	constexpr float BridgeYaw[NumBridges] = { 0.f, 45.f, 135.f, 180.f, 225.f, 315.f };
	/** Bridge launch point radius (middle of span A, r1535 z0 -> r1750 z-30) and the deck height there. */
	constexpr float LaunchR = 1645.f, LaunchDeckZ = -15.f;
	/** Ring 2 rolling band (ball centre radius limits) and its floor height. */
	constexpr float RoamRMin = 1792.f, RoamRMax = 2018.f, FloorZ = -30.f;
	/** Default spawn spots on Ring 2 in front of the islet bleachers, clear of bridges, ramps, pockets and bumpers. */
	constexpr float SpawnR = 1950.f;
	constexpr float SpawnYaw[4] = { 125.f, 56.f, 212.f, 325.f };
}
