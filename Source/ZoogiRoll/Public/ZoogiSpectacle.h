// b268 (Evan: "Yes to all four ideas put them in"): match spectacle -
//  1 KNOCKOUT MOMENT: a short slow-mo, a low camera on the court watching the Zoogi fly off, the crowd jumps, confetti out of the stands
//  2 SHOT CAM: after a strong human shot the camera rides behind the ball for a moment, then goes back to the match camera
//  3 CHAIN LIGHTS: every orb knocked out in the same shot lights the next ring outward in gold with a rising note
//  4 FINAL ROUND: the lights dim and the arena pulses gold on the music's beat
// Off switches: -ZoogiNoKOMoment -ZoogiNoShotCam -ZoogiNoChainLights -ZoogiNoFinalPulse (all: -ZoogiNoSpectacle)
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiSpectacle
{
	/** Called every frame from UZoogiFunSys::Tick (real seconds). */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** > 0 while the knockout slow-mo runs: the time dilation it wants. */
	ZOOGIROLL_API float SlowMo();
}
