// b124 (Evan 21:44: "Fix cpu ai - they should play like a person would and be smarter"):
// a thin "person" layer over the CPU planner. Decision-making only (no stat, energy or card changes).
//  - skills are chosen for the moment on the table (a water cannon only with a target in reach, a shield when the Zoogi is
//    exposed, meteors where things are bunched up...) instead of "most expensive first"; each CPU has its own temperament
//  - aiming skills are cast while rolling, from where the Zoogi is at that moment (the same rule as the player's COMBO casts)
//  - smarter targets: rivals near their edge, the score leader, bunches for area skills; ice walls guard the CPU's own edge
// -ZoogiCPULegacy = the b123 CPU.
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class AActor;

namespace ZoogiCPUSmart
{
	ZOOGIROLL_API bool On();
	/** this CPU's temperament for the match: 0.75 (careful) .. 1.3 (aggressive) */
	ZOOGIROLL_API float Aggression(const AZoogiGameMode* G, int32 Player);
	/** how much a person would want this card right now (0 = pointless, ~100 = perfect moment) */
	ZOOGIROLL_API float CardValue(AZoogiGameMode* G, const AZoogiBall* Me, FName Id, float Noise);
	/** aiming skills (and the slash) are cast while rolling, from where the Zoogi is then */
	ZOOGIROLL_API bool CastMidRoll(FName Id);
	/** b125: shield / bolt / buff - only one of these on the Zoogi per turn */
	ZOOGIROLL_API bool IsAura(FName Id);
	/** the moment a person would cast this mid-roll skill (bLastChance: the roll is about to end) */
	ZOOGIROLL_API bool WantCastNow(AZoogiGameMode* G, const AZoogiBall* Me, FName Id, bool& bLastChance);
	/** smallest distance from P to the ring-out edge over 16 directions (cm) */
	ZOOGIROLL_API float EdgeDist(const AZoogiGameMode* G, const FVector2D& P, FVector2D* OutDir = nullptr);
	/** target rank for a tap skill (lower = better): rivals, rivals near their edge (away from Me), the leader */
	/** b244 (Evan): earth walls a CPU raises mid-roll to block a rival (not its own backstop) */
	ZOOGIROLL_API bool IsEarthBlock(FName Id);
	ZOOGIROLL_API float TapRank(const AZoogiGameMode* G, const AZoogiBall* Me, AActor* T);
}
