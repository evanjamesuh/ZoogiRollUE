// b146 (Evan Nov 5 16:34, "build the classic version"): the Cosmos boss WAKES UP for the closing rounds (the final round, and the
// one before it when a match has 4+ rounds). Once per round it attacks the arena, and you dodge with your shot:
//  1. WARNING - at the round's first turn the boss picks a target (the leader 2 times in 3) and a red zone appears on the floor
//     with a countdown ("HAND SWEEP - 3 TURNS"). Everyone gets one turn to roll out of it - or to knock a rival into it.
//  2. The attack lands when the round's last shot has settled (before the next round / the results):
//     HAND SWEEP - the boss reaches over the rim and drags its hand along a 70 deg slice of the outer court; Zoogis in the slice are
//       flung outward. COVER: a pillar / wall / obstacle between a Zoogi and the rim stops the hand. BRACE: as the hand reaches a
//       human Zoogi the world slows - tap to brace and only a nudge lands.
//     GROUND SLAM - a fist slams the rim and the shockwave runs to the marked spot: strong / medium / weak rings push Zoogis and
//       orbs outward from it. GUARD (the command) anchors a Zoogi: it barely moves.
//  Nobody scores a boss ring-out (it is a normal -fall for the victim). Cosmos only (the boss is there). -ZoogiNoBossAttack = off,
//  -ZoogiBossAttackTest = every round, aimed at the human, sweep then slam (captures boss_*).
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class UCanvas;
class APlayerController;

namespace ZoogiBossAttack
{
	/** From the action ticker, unscaled seconds. */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** Rolling phase: true = the turn may not resolve yet (the round's attack is due / running). Starts the attack once all is at rest. */
	ZOOGIROLL_API bool Hold(AZoogiGameMode* G);
	/** World speed the BRACE window wants (0 = none). */
	ZOOGIROLL_API float SlowMo();
	/** b147a: a CPU in a pending zone - true = roll OutRun cm along OutDir instead of its plan (it may GUARD the slam and keep its plan). */
	ZOOGIROLL_API bool CpuDodge(AZoogiGameMode* G, AZoogiBall* Me, FVector2D& OutDir, float& OutRun);
	/** b147a: the boss-attack camera is on */
	ZOOGIROLL_API bool CamActive();
	/** b190: a boss GRAB (tap to break free) or a BOSS DUEL (tap on the flash) is running for the human - field taps belong to it */
	ZOOGIROLL_API bool DuelActive();
	/** b190 BOSS LOCK-ON: where to shoot the boss (true while the boss is in play) */
	ZOOGIROLL_API bool LockOnTarget(const AZoogiGameMode* G, FVector& OutP);
	/** b200 raid challenges: a big boss attack is announced and not landed yet */
	ZOOGIROLL_API bool Pending(const AZoogiGameMode* G);
	/** b200: P is inside the announced attack's red zone */
	ZOOGIROLL_API bool InDangerZone(const AZoogiGameMode* G, const FVector& P);
	/** b200 INTERRUPT: the announced attack is cancelled */
	ZOOGIROLL_API void Interrupt(AZoogiGameMode* G, int32 ByPlayer);
	ZOOGIROLL_API void HitBoss(AZoogiGameMode* G, int32 ByPlayer);
	/** b190: the anger meter, the grab bars, the duel prompt */
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	/** b155a: the current round is one of the boss's attack rounds (the boss should be in view) */
	ZOOGIROLL_API bool Engaged(const AZoogiGameMode* G);
	/** b207 raid HUD A: the anger meter (0..1), whether it is full and who duels */
	ZOOGIROLL_API float AngerFrac(bool* bReady = nullptr, int32* By = nullptr);
	/** b207 raid HUD A: the coming attack's warning line for the ticker (false = none pending) */
	ZOOGIROLL_API bool Warning(const AZoogiGameMode* G, FString& Out);
}
