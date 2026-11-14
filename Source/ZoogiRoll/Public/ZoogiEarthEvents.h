// b180 (Evan Nov 8: "build the earth bender events"): the Earthbender effects as map events that reshape the court.
//  - COURT SHIFT (start of every round): the map's wall / pillar erupts and STAYS as solid rock for the rest of the match;
//  - TREMOR (now and then during a roll): a warning circle, then a burst knocks back whatever is inside (nobody's credit);
//  - CATACLYSM (start of the final round, once): the map's big one - a huge burst + a ring of rock.
// Each map has its own set (matched to its stone). -ZoogiNoEarthEvents = off, -ZoogiEarthEventsTest = a tremor every roll.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class UCanvas;
class APlayerController;
class AZoogiFabFX; // b213

namespace ZoogiEarthEvents
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	ZOOGIROLL_API void Clear();
	/** b180: a skill / power blast at P breaks the event rocks within Radius. */
	ZOOGIROLL_API void BlastAt(AZoogiGameMode* G, const FVector& P, float Radius, int32 Credit);
	/** b213 (Evan: "I want them to be solid while they're there ... bank off it or smash an enemy into it ... the blast from the side
	 *  of the rock"): an earth bend from a SKILL stands solid while its effect shows. HalfThick > 0 = a wall (a box HalfLen x HalfThick
	 *  turned by Yaw), HalfThick <= 0 = a post of radius HalfLen. Fx = the effect it belongs to (the solid goes when it ends, max Life s).
	 *  Every hit on a rock (skill or event) blasts out of the rock's SIDE where it was struck and throws the hitter back off it. */
	/** b215: one rock-face blast in style 1-8 (the options reel): BIG BLAST, ROCK BURST, SHOCKWAVE, FIRE, ELECTRIC, BIG BLAST + IMPACT FRAME, CRYSTAL, MEGA */
	ZOOGIROLL_API void BlastStyleAt(AZoogiGameMode* G, const FVector& P, const FVector& Out, float Vn, int32 Style);
	ZOOGIROLL_API void AddSolid(AZoogiGameMode* G, const FVector& Base, float Yaw, float HalfLen, float HalfThick, float H, float Life, AZoogiFabFX* Fx = nullptr);
}
