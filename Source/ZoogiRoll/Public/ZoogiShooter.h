// b234 (Evan Nov 10: "We need a mode for 3rd person shooter" -> "All three, in that order"): SHOOTER modes on the Battle Royale engine.
// Everyone fights on foot (robot form, never the ball) as a Voyager soldier (ZoogiVoyager) with Voyager guns.
//  1 HOLD THE LINE   - you (+ the Voyager trooper and minigun turret) vs waves of 3 Voyager soldiers; each wave tougher, wave 5 = boss
//                       wave; clear 5 waves to win, run out of lives and it is over (score = waves held)
//  2 SHOOTER ROYALE  - you + 3 bots, last one standing (royale rules, on foot)
//  3 TEAM DEATHMATCH - P1 + P2 (blue) vs P3 + P4 (red), no friendly fire, endless respawns, first team to 20 knockouts
// Mode Select has three cards; -ZoogiShooter=<1|2|3> starts one from the command line (with -ZoogiRoyale).
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiShooter
{
	ZOOGIROLL_API int32 Kind();               // 0 = not a shooter match
	ZOOGIROLL_API void SetKind(int32 K);
	ZOOGIROLL_API bool On(const AZoogiGameMode* G);
	ZOOGIROLL_API int32 Team(const AZoogiGameMode* G, int32 Idx);
	ZOOGIROLL_API bool Allies(const AZoogiGameMode* G, int32 A, int32 B);
	ZOOGIROLL_API bool KeepLives(const AZoogiGameMode* G); // TDM: a knockout never costs a life
	ZOOGIROLL_API void OnStart(AZoogiGameMode* G);
	ZOOGIROLL_API void OnFight(AZoogiGameMode* G);
	ZOOGIROLL_API void OnRespawn(AZoogiGameMode* G, int32 Idx);
	ZOOGIROLL_API void OnLoseLife(AZoogiGameMode* G, int32 Victim, int32 CreditedTo);
	/** after an elimination: true = handled here (next wave / match over), skip the royale "last one standing" check */
	ZOOGIROLL_API bool OnEliminated(AZoogiGameMode* G, int32 Victim);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt);
	ZOOGIROLL_API FString Hud(const AZoogiGameMode* G);
	/** b237: the human pulls the trigger - false while reloading (30 rounds, 1.5 s reload) */
	ZOOGIROLL_API bool TryShot(AZoogiGameMode* G);
	/** b236: TRANSFORM in a shooter match. bToBall: needs 3 crystals (spends them all, 2 s of ball per crystal). false = refused. */
	ZOOGIROLL_API bool AllowTransform(AZoogiGameMode* G, int32 Idx, bool bToBall);
	/** for the raid crew in HOLD THE LINE: the enemy to shoot at, -1 none */
	ZOOGIROLL_API int32 CrewTarget(const AZoogiGameMode* G, const FVector& From);
}
