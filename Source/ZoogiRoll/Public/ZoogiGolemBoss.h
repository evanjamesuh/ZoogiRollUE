// b177g (Evan Nov 7 18:01: "Volcano golem more full body shot and use the hammer not the hands"): the MAGMA TITAN is the Stone Golem
// standing full-body on the Volcano rim with its own animations (idles, 5 hammer attacks, hits, death), facing the viewer. The
// peek-boss logic still decides where / when (the titan puppet is hidden); the boss attacks are hammer blows:
// GROUND SLAM = an overhead hammer smash, the sweep = HAMMER SWEEP (a wide hammer swing). -ZoogiOldMagmaBoss = the old peek puppet.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiGolem
{
	ZOOGIROLL_API bool Wanted(const AZoogiGameMode* G);
	ZOOGIROLL_API bool Spawn(AZoogiGameMode* G);
	ZOOGIROLL_API bool Active();
	ZOOGIROLL_API void Clear();
	/** Dip: 0 = up on the rim, 1+ = below it (the peek logic's) */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt, const FVector& Ctr, float RimR, float RimZ, float Ang, float Dip, bool bDefeated);
	ZOOGIROLL_API void Sweep(bool bOn, float AngDeg);
	/** Slam / Roar / Taunt / Hit / Threat */
	ZOOGIROLL_API void Event(const TCHAR* What);
	/** b177l: the slam side - it steps there before the hammer comes down */
	ZOOGIROLL_API void GoTo(float AngDeg);
	ZOOGIROLL_API FVector HeadLoc();
	ZOOGIROLL_API bool CamSpot(FVector& OutPos, FVector& OutLook);
}
