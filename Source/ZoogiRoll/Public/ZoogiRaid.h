// b151 (Evan Nov 5 22:48 "Let's do it" - RAID MODE): co-op classic against the map's titan.
//  - MODE SELECT -> RAID -> pick a map (each map's titan). You + 2 CPU allies, one team; 10 rounds; 5 shared team lives.
//  - The titan's HAND on the rim is the weak point (a glowing gold target): roll your Zoogi into it or knock orbs into it -
//    damage by impact speed (a gold star orb hits hardest). SHOWTIME (your Awaken) hits it for 20% of its health.
//  - After every round the titan takes its turn: the boss attacks (warning zone a round ahead, sweep / slam) and it moves to
//    a new spot on the rim. Phase 2 (66%): it moves after EVERY turn. Phase 3 (33%, enraged): bigger attack zones.
//  - A Zoogi that falls costs a TEAM LIFE. Win: health 0 (it sinks off the rim). Lose: no team lives, or 10 rounds pass.
//  - Results: RAID CLEARED / RAID FAILED, the MVP (most damage), damage + hits per Zoogi.
// -ZoogiRaid = start a raid from the command line (test), -ZoogiRaidHP=N = boss health.
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class UCanvas;

namespace ZoogiRaid
{
	/** The next classic match is a raid (MODE SELECT RAID -> map card). */
	ZOOGIROLL_API void Arm(AZoogiGameMode* G);
	ZOOGIROLL_API bool Armed();
	ZOOGIROLL_API bool Active();
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C);
	/** CPU shot planning: the weak point to aim at (null when no raid). */
	ZOOGIROLL_API class AActor* HandActor();
	ZOOGIROLL_API void OnAwaken(AZoogiGameMode* G, AZoogiBall* B);
	/** FinishGame: records the result (banner). */
	ZOOGIROLL_API void OnFinished(AZoogiGameMode* G);
	ZOOGIROLL_API void PreFinish(AZoogiGameMode* G);
	ZOOGIROLL_API void OnMenu(AZoogiGameMode* G);
	/** results screen: the last raid's outcome */
	ZOOGIROLL_API bool HasResult();
	ZOOGIROLL_API bool Cleared();
	ZOOGIROLL_API int32 DamageOf(int32 Player);
	ZOOGIROLL_API int32 HitsOf(int32 Player);
	ZOOGIROLL_API int32 LivesLeft();
	ZOOGIROLL_API int32 HPLeft();
	ZOOGIROLL_API int32 MaxHP();
	/** boss attack zone scale (1 / 1.3 enraged) */
	ZOOGIROLL_API float Rage();
	/** b200 raid challenges */
	ZOOGIROLL_API void BonusDamage(AZoogiGameMode* G, int32 Player, int32 Amt, const TCHAR* What);
	ZOOGIROLL_API void SpawnArmor(AZoogiGameMode* G);   // ARMOR BREAK: 2 ice shards guard the weak point this turn
	ZOOGIROLL_API void ClearArmor();
	ZOOGIROLL_API int32 ArmorBroken();
	ZOOGIROLL_API void Expose(AZoogiGameMode* G, int32 Turns); // the weak point takes x2 for the next N turns
	/** b207 HUD A (one boss plate + one warning ticker) is on (a raid is running, no -ZoogiOldRaidHud) */
	ZOOGIROLL_API bool NewHud();
	/** b207 D: a HOW TO HURT IT card under the plate for 3 s (0 weak point, 1 sweep, 2 slam, 3 grab, 4 anger duel, 5 boss attack / SHOWTIME, 6 invasion) */
	ZOOGIROLL_API void HowTo(int32 Kind, const FString& Title = FString());
	ZOOGIROLL_API bool CardUp();
	/** b207: a line in the one warning slot under the plate (3 s) */
	ZOOGIROLL_API void Ticker(const FString& Line, const FLinearColor& Col);
	/** b207: BOSS ATTACK - a raid SHOWTIME's grade (0..1) is the damage */
	ZOOGIROLL_API void ShowtimeHit(AZoogiGameMode* G, int32 Player, float Grade01);
}
