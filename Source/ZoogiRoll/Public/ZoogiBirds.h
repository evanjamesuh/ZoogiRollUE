// b179 (Evan Nov 7: "add birds to meadow arena that fly away naturally if rolled near and some at start of match for ambiance"):
// small low-poly songbirds built from the engine's basic shapes (no asset pack needed). A flock perches on the Meadow court,
// pecks and hops, and takes off - flapping, climbing away from the danger, startling its neighbours - when a Zoogi rolls near;
// they glide back in and land somewhere else a while later. At the start of a match a flock flies over the arena.
// -ZoogiNoBirds = off, -ZoogiBirdsAll = on every map (test).
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiBirds
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Clear();
}
