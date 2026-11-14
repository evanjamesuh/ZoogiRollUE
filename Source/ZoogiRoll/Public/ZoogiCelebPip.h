// Nov 2 b74 (Evan: "No more 3D character celebration fullscreen cam - it only gets played in a small window, not impeding the gameplay
// view, like the other animations"): while an anime event plays, the 3D close-up of the Zoogi is rendered by a scene capture into a
// small P5-framed window; the main camera stays on the game. -ZoogiAnimeFullCam = the old full-screen close-up camera.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class UCanvas;
class AActor;

namespace ZoogiCelebPip
{
	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API void Start(UWorld* W, AActor* Focus, float Yaw, float DistMul);
	ZOOGIROLL_API void Stop();
	ZOOGIROLL_API void Draw(UCanvas* C);
}
