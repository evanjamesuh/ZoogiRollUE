// b177 (Evan Nov 7: "this but fo sure would be the frozen boss" + "a awaken where it flydown and beams the arena from its mouth"):
// DRACORIA, the Frozen map boss. The peek-boss logic in ZoogiCosmosBoss still decides where / when the boss is (its puppet is hidden);
// this file is the dragon itself: it glides in and lands on the rim, idles and roars there, takes off when it leaves, and for the
// boss attack's sweep it dives in, hovers over the rim and sweeps an ice beam from its mouth across the court (ICE BREATH).
// -ZoogiOldFrostBoss = the old Frost Titan.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiDragon
{
	ZOOGIROLL_API bool Wanted(const AZoogiGameMode* G);
	/** b177d: the Frozen islet the dragon sits on instead of a bleacher (-1 = none, -ZoogiOldFrostBoss) */
	ZOOGIROLL_API int32 SeatIslet();
	ZOOGIROLL_API bool Spawn(AZoogiGameMode* G);
	ZOOGIROLL_API bool Active();
	ZOOGIROLL_API void Clear();
	/** every peek-boss tick: Ctr = arena centre, RimR / RimZ = the bowl wall top, PeekAng = where the boss stands, bShow = in view */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt, const FVector& Ctr, float ER, float RimR, float RimZ, float PeekAng, bool bShow, bool bDefeated, const FVector& LookAt);
	/** the boss attack's sweep: bOn + the beam's current angle on the court */
	ZOOGIROLL_API void Sweep(bool bOn, float AngDeg);
	/** Roar / Stomp / Hit / Death / Taunt */
	ZOOGIROLL_API void Event(const TCHAR* What);
	ZOOGIROLL_API FVector HeadLoc();
	/** b177e: where the boss cam stands to see its face (false = no dragon) */
	ZOOGIROLL_API bool CamSpot(FVector& OutPos, FVector& OutLook);
	/** seconds the sweep waits for the dive (0 = no dragon) and how much slower the beam crosses the slice */
	ZOOGIROLL_API float SweepLead();
	ZOOGIROLL_API float SweepSlow();
	/** "ICE BREATH" with the dragon, else the given name */
	ZOOGIROLL_API const TCHAR* SweepName(const TCHAR* Def);
	/** b178h: the fly-in intro - true while the first turn should wait for it; Skip = a tap (true = it was skipped) */
	ZOOGIROLL_API bool IntroHold();
	ZOOGIROLL_API bool IntroPlayed();
	ZOOGIROLL_API bool SkipIntro();
}
