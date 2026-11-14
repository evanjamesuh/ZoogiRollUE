// Oct 22 (Evan / Effects Manager): quality-feel pass — hit-stop, impact camera shake, per-weight collision sounds,
// teeter wobble + gasp near rim (47), comic flail on knock-off (43), crowd gasp/roar on big moments (27).
#pragma once

#include "CoreMinimal.h"

class AZoogiBall;
class AActor;
class AZoogiPlayerController;
class UWorld;
struct FHitResult;

/** Coherent feel layer for hard contacts + rim/fall drama. Visual / audio / time-dilation only: never changes physics
 *  outcomes, marble rules, camera mode switching (buttons only), or turn flow beyond a brief ~50ms dilation.
 *  Tunables: [ZoogiRoll.Feel] in DefaultGame.ini. -ZoogiNoFeel / -ZoogiFeelTest / -ZoogiFeelLog. */
namespace ZoogiFeel
{
	/** Hard Zoogi-on-Zoogi / Zoogi-on-obstacle contact. Dedupes Zoogi pairs (lower PlayerIndex). */
	ZOOGIROLL_API void OnHardContact(AZoogiBall* Self, AActor* Other, float Impulse, const FHitResult& Hit);

	/** Brief world time dilation (~50ms real), throttled. Safe with PC SlowMoUntil restore. */
	ZOOGIROLL_API void TryHitStop(float Strength01, UWorld* World);

	/** Impulse-scaled camera shake via AZoogiPlayerController::OnImpact (button cams only; no free-cam). */
	ZOOGIROLL_API void TryCameraShake(float Strength01, UWorld* World);

	/** Collision thud by mass/weight class (reuses S_HitClick / S_HitCrack / S_Clack / S_GuardClang). */
	ZOOGIROLL_API void PlayWeightCollision(AZoogiBall* Self, AActor* Other, float Impulse);

	/** Item 47: near-rim teeter wobble + short gasp (S_Inhale). Call from Ball::Tick while on court. */
	ZOOGIROLL_API void TickNearRim(AZoogiBall* Ball, float DeltaSeconds);
	/** Sonic-style visual curl while slingshot pulled: pull 0..1 -> spin DPS on the ball mesh only; StopAimSpin blends out. */
	ZOOGIROLL_API void TickAimSpin(AZoogiBall* Ball, float Pull01, const FVector& ShotDir);
	ZOOGIROLL_API void StopAimSpin(AZoogiBall* Ball);

	/** Item 43+27: comic flail (spin/squash/wobble + Niagara) and crowd roar/gasp. Call from SetOut BEFORE bOut=true. */
	ZOOGIROLL_API void OnKnockOff(AZoogiBall* Ball);

	/** Item 27: mobile-friendly crowd layer (S_CrowdWave1 gasp / S_CrowdWave2 roar). Throttled. */
	ZOOGIROLL_API void TryCrowd(AActor* Host, float Strength01, bool bRoar);

	/** Seconds to keep the marble visible after SetOut so the flail reads (0 = hide immediately). */
	ZOOGIROLL_API float FlailVisibleSeconds();

	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API float HitStopSeconds();
	ZOOGIROLL_API float HitStopScale();

	/** Counters for -ZoogiFeelTest: 0 hitstops, 1 shakes, 2 sounds, 3 teeters, 4 flails, 5 crowds. */
	ZOOGIROLL_API int32 Count(int32 Kind);
}
