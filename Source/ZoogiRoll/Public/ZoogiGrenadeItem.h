// b130 (Evan 03:25: "have the orb generator spit a grenade after round one, play - who touches it first collects it. Can be used to do
// the same shockwave effect but the arc line is 2x farther than normal"):
//  - from round 2 the centre orb generator lobs one PRISM GRENADE onto the court each round (when none is lying there already)
//  - the first Zoogi (player or CPU) to touch it carries it (kept for the match)
//  - ITEM -> PRISM GRENADE opens a target circle with twice the normal arc range; FIRE lobs the grenade along an arc and it bursts
//    into the Energy Shockwave (pull, then blast) where it lands. CPUs throw theirs at a crowd within reach.
// -ZoogiNoGrenadeDrops = off. Classic matches only (not Royale / 2v2 combo).
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;

namespace ZoogiGrenade
{
	/** round start (2+): the generator drops one if none is on the court */
	ZOOGIROLL_API void OnRound(AZoogiGameMode* G);
	/** grenades this player carries */
	ZOOGIROLL_API int32 Count(const AZoogiGameMode* G, int32 Player);
	/** spends one (false if none) */
	ZOOGIROLL_API bool Take(AZoogiGameMode* G, int32 Player);
	/** the throw: the grenade arcs from the Zoogi to At, then the shockwave bursts there */
	ZOOGIROLL_API void Lob(AZoogiGameMode* G, AZoogiBall* B, const FVector& At);
}
