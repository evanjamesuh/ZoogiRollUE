// b213: the scripted previs reel (-ZoogiDemoReel + -ZoogiFilm) - see ZoogiDemoReel.cpp
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
namespace ZoogiDemoReel
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** true while -ZoogiDemoReel holds the match (the human test player does not roll) */
	ZOOGIROLL_API bool Holding();
}
