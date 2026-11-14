// b144c (Evan Nov 5: "Camera gets stuck at high elevations how can we fix this and make it a fun part of the game action" -> SKY DIVE cam):
// when the turn's Zoogi is thrown high into the air (a banked Bolt Balls, a stands bounce, a bumper pop), a flight camera swoops up
// above it with a short slow-mo and a "SKY HIGH!" callout, then rides it back down looking at where it will land, and hands back to
// the match camera with a landing thump.  -ZoogiNoSkyDive = off.  -ZoogiSkyDiveH=420 = height above its last floor (cm) that starts it.
// -ZoogiSkyDiveTest = the human's first roll is popped up so the whole sequence plays (captures skydive_*).
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiSkyDiveCam
{
	/** RealDt = unscaled seconds (the cam stays smooth through its own slow-mo). */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API bool Active();
	/** The world speed the swoop wants right now (0 = no request). */
	ZOOGIROLL_API float SlowMo();
}
