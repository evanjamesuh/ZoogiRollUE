// Nov 2 b65 (Evan: "If an orb or player flies near this ring a vortex sucks it in and shoots it back towards the center from
// another random similar ring"): on the Frozen Ring the upright ice rings (the resonance pillars on the court and the Ring 2 orb
// pockets) are vortex portals. Anything moving that passes within reach is pulled in, then fired from a random other ring
// toward the centre of the court. -ZoogiNoVortexRings = off, -ZoogiVortexTest = a test orb is sent into a ring.
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiVortexRings
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt);
}
