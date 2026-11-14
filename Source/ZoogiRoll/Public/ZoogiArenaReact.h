// b142 (Evan, Nov 5 11:23): arena responses to knock-outs.
//  * Cosmos is zero-g: a Zoogi that leaves the court edge does not drop - it floats straight out (keeps its heading, slows a
//    little, tumbles) for ~1.1 s and THEN the knock-out blast goes off where it is. -ZoogiCosmicGravity = the old fall.
//  * The Cosmos rim no longer bounces orbs / Zoogis back in (rim bumper pylons, deflector wings, bank arcs and re-entry pipes
//    lose their collision + triggers and hide). -ZoogiCosmicRimSolid = the old rim.
//  * Stands: a Zoogi flying into a grandstand (Frozen islet bleachers, or the solid stands of the other arenas) is NOT out - the
//    stands take the crash (shake, debris, the crowd roars / cheers), the crowd throws it back onto the court and it earns a
//    CROWD TOKEN (+1 crystal, no cap). -ZoogiNoStandsSave = the old rules.
//  * The knock-out explosions (player KO, orb out) read the Skill Lab entries PLAYER KNOCKOUT / ORB KNOCKOUT (blast type, size, colour).
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class AZoogiArena;
class UWorld;

namespace ZoogiArenaReact
{
	/** b149b: off the court and past the flight cap (6 m from its last spot on the court) = out, unless it is about to hit a building / wall. */
	ZOOGIROLL_API bool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);
	/** b171 test switches: -ZoogiKOLine circles, -ZoogiKOShow forced knockout + frames, -ZoogiFallOut=cm */
	ZOOGIROLL_API void KOShowTick(AZoogiGameMode* G);
	ZOOGIROLL_API bool FallOut(AZoogiGameMode* G, const FVector& L);
	/** Cosmos: the Zoogi crossed the court edge (zero-g) -> it is out right now (the caller runs the KO rules, then StartFlyOut). */
	ZOOGIROLL_API bool CosmicEdgeOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);
	/** Cosmos: after SetOut - the Zoogi floats out along V, then the KO blast at its spot. */
	ZOOGIROLL_API void StartFlyOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& V, const FLinearColor& Tint);
	/** The player knock-out blast (Skill Lab "PLAYER KNOCKOUT" picks the explosion / size / colour). */
	ZOOGIROLL_API void PlayerKOBlast(UWorld* W, const FVector& At, const FLinearColor& Tint);
	/** The orb knock-out burst (Skill Lab "ORB KNOCKOUT"). */
	ZOOGIROLL_API void OrbOutBlast(UWorld* W, const FVector& At, bool bStar);
	/** Stands: true = this Zoogi is heading into / bouncing off the stands or being thrown back by the crowd (skip the ring-out). */
	ZOOGIROLL_API bool StandsCheck(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);
	/** Cosmos rim: strip the rebounders (called after the arena's obstacles are placed). */
	ZOOGIROLL_API void OpenCosmicRim(AZoogiArena* A);
}
