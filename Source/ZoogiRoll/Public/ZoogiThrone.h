// b156: the ZOOGI KING's throne on Ancient Meadows (see ZoogiThrone.cpp)
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
namespace ZoogiThrone
{
	/** builds the throne while the arena is Ancient Meadows, removes it on any other arena */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G);
	ZOOGIROLL_API FVector SeatLocation();
}
