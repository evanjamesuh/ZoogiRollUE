// b157: the purple + gold "Z" Zoogi flags waving on every arena (see ZoogiFlags.cpp)
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
namespace ZoogiFlags
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt);
}
