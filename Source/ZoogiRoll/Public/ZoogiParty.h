// b273 (Evan: "finish still to do" - the skills review's NEW MODES): PARTY MODES, opened from the PARTY card in Mode Select
//  KING OF THE HILL - the centre ring scores 10 for every Zoogi sitting in it when a round ends
//  HOT ORB          - one glowing bomb orb; after 4 turns it blows up and whoever touched it last is blasted toward the edge
//  ORB STORM        - a 90-second match: new orbs keep raining onto the court
//  BOSS RUSH        - three raids back to back (Volcanic Pit, Frozen Ring, Tomb) on one pool of team lives
//  TRICK COURSE     - solo: knock the marked orb into the hole in as few shots as you can, 3 holes
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiParty
{
	enum class EKind : uint8 { None, Koth, HotOrb, OrbStorm, BossRush, TrickCourse };
	/** Arms a party mode for the match about to start (None clears it). */
	ZOOGIROLL_API void Arm(AZoogiGameMode* G, EKind Kind);
	ZOOGIROLL_API EKind Kind();
	/** Every frame from UZoogiFunSys::Tick (real seconds). */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** BOSS RUSH: called when a raid has ended - true = the next raid started (do not finish the game). */
	ZOOGIROLL_API bool BossRushNext(AZoogiGameMode* G, bool bWin, int32 LivesLeft);
	/** BOSS RUSH: team lives the next raid starts with (-1 = the raid's own count). */
	ZOOGIROLL_API int32 CarryLives();
}
