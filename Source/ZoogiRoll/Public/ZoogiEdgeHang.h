// Nov 1 b58 (Evan): "When a player rolls off the edge the aurex form animate holding over the edge with two arm trying to pull its
// self up, players taps reaction button like clash a mini game of successfully uses 2 sp."
// A human Zoogi that drops off the court edge (not blasted far out) grabs the lip as its Aurex: the world slows, the camera cuts to
// the ledge, and the player taps REACTION (or anywhere) to fill the PULL bar before time runs out. Made it = climbs back onto the
// court (-2 SP / Card Energy). Missed = lets go and falls (normal ring-out). Needs 2 SP to grab when cards are on.
// b67 (Evan: "Remove the edge hanging mini game, keep the animation for visuals but it's counted as a knockout and always falls,
// no need to zoom"): by default the Aurex grabs the lip, slips and falls - no taps, no SP, no ledge camera, no slow-mo; the fall is a
// normal ring-out (KO). -ZoogiEdgeHangGame = the b58 mini-game.
// Toggles: -ZoogiNoEdgeHang. Tests: -ZoogiHangAll (CPU Zoogis grab too, auto-tapping), -ZoogiHangTest (proof shots hang_*).
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;

namespace ZoogiEdgeHang
{
	constexpr int32 Cost = 2;
	constexpr float Window = 3.5f;   // real seconds to pull up
	constexpr float PerTap = 0.11f;  // bar per tap
	constexpr float Slip = 0.1f;     // bar lost per second
	constexpr float Dilation = 0.03f;
	struct FView
	{
		bool bOn = false;
		int32 Phase = 0;   // 0 grab, 1 struggle, 2 climb, 3 drop, 4 cheer
		int32 Player = 0;
		bool bVisual = false; // b67: animation only, always falls
		bool bHuman = false, bFree = false, bNextTurn = false; // bNextTurn: off-turn save, the 2 SP come off the next turn
		float Prog = 0.f, Left = 0.f, PhaseAge = 0.f, LastTap = -10.f;
		int32 Result = 0;  // after the hang: 1 saved, 2 fell
		float ResultAge = -1.f;
		FString Name;
		FVector At = FVector::ZeroVector; // b175: where it hangs (the camera drifts over)
	};
	/** True while the mini-game holds the world (slow-mo, turn cannot resolve). */
	ZOOGIROLL_API bool Active();
	ZOOGIROLL_API const FView& View();
	/** CheckKnockoffs, per Zoogi: remembers its last spot on the court and grabs the edge when it drops off. True = held (skip ring-out). */
	ZOOGIROLL_API bool Track(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);
	/** b147a: this Zoogi gets the tap-to-pull-up mini-game (free) if it drops off the edge in the next Seconds (the boss-sweep BRACE). */
	ZOOGIROLL_API void Force(AZoogiBall* B, float Seconds);
	ZOOGIROLL_API bool Forced(const AZoogiBall* B);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** REACTION / tap anywhere. True = used. */
	ZOOGIROLL_API bool Tap(AZoogiGameMode* G);
	/** True when the REACTION button should read PULL! (a human is hanging and may tap). */
	ZOOGIROLL_API bool WantsTaps();
	/** b67: the hang is animation only (always falls, no camera / slow-mo). False with -ZoogiEdgeHangGame. */
	ZOOGIROLL_API bool Visual();
}
