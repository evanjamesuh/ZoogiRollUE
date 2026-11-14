// Nov 1 b59 (Evan): "During arc enabled launches the camera animates to these angles that close in a 160 degree direction from back
// toward front." An ARC launch hands the view to a flight camera that starts behind the Zoogi (looking along the shot), swings
// 160 degrees round one side while it closes in, and ends in front of it as it lands; then the match camera takes over again.
// -ZoogiNoArcCam = off.
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;

namespace ZoogiArcCam
{
	/** Dir = horizontal launch direction, Flight = seconds until it lands. */
	ZOOGIROLL_API void Start(AZoogiGameMode* G, AZoogiBall* B, const FVector2D& Dir, float Flight);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt);
	ZOOGIROLL_API bool Active();
}
