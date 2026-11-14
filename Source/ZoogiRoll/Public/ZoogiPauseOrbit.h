#pragma once
// b120 (Evan, REDIRECT pick screenshot: "during gameplay pauses player can rotate the camera around the player then smoothly
// transitions back to normal play"): while an in-play pick pauses the game (REDIRECT - tap a glowing target, the superpower bullet-time
// target pick) a drag turns the camera round the turn player's Zoogi; when the pick ends the match camera eases back. Those picks choose
// on release, so a drag never picks. -ZoogiNoPauseOrbit = off.
#include "CoreMinimal.h"

class UWorld;

namespace ZoogiPauseOrbit
{
	/** registers the drag reader once (cheap to call every frame) */
	ZOOGIROLL_API void Ensure();
	/** true while an in-play pick pause allows the orbit */
	ZOOGIROLL_API bool Active(UWorld* World);
	/** the current / last press moved far enough to be a drag (a pick ignores its release) */
	ZOOGIROLL_API bool WasDrag();
	/** match camera: turns the computed view round the turn player's Zoogi by the orbit; eases it back to zero once the pick ends */
	ZOOGIROLL_API void Apply(UWorld* World, FVector& Loc, FRotator& Rot, float Dt);
}
