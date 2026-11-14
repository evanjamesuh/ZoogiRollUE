// Oct 31 b46 (Evan): "after so many rounds the map takes another form - all of this has to be really realistic".
//  Classic: halfway through the match (round MenuRounds/2 + 1) a P5 wipe "STAGE SHIFT!" covers the arena; under it the light turns to dusk
//           and four new star bumpers rise out of the floor (stage-forming Rise) - the field plays differently for the second half.
//  Royale on Ancient Collision Meadows: at 70 s of the fight the volcano erupts - the sky reddens, the crown spits fire and lava bombs
//           arc out of the crater onto the islands (red warning ring 1.3 s, then a blast: damage + push).
//  -ZoogiNoStageShift turns both off; -ZoogiStageShiftTest triggers them early (classic round 2, eruption at 12 s).
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiStageShift
{
	ZOOGIROLL_API void OnMatchStart(AZoogiGameMode* G);   // undo the previous match's shift (props, light)
	ZOOGIROLL_API void OnRoundStart(AZoogiGameMode* G);   // classic: shift when the trigger round begins
	ZOOGIROLL_API void RoyaleTick(AZoogiGameMode* G, float Dt);
	ZOOGIROLL_API bool Shifted();
}
