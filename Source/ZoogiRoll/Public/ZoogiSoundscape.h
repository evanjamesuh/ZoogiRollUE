// b163 (Evan Nov 6 13:32 "Finish the sound scape"; Arena Life Plan: ambient soundscape per map + crowd reactions): every arena has its
// own living ambience (Scripts/synth_ambience.py -> /Game/Audio/Sounds/Amb), a crowd bed that is quiet early and loud in the closing
// round, a drumroll in the final moments, random world accents, a crowd groan on a missed shot and a whoosh + echoing blast under the
// arena when a Zoogi falls off. -ZoogiNoSoundscape = off.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
namespace ZoogiSoundscape
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt);
	/** a Zoogi went over the edge: whoosh now, a blast far below a moment later, the crowd lifts */
	ZOOGIROLL_API void OnKnockOff(AActor* Ball);
	/** crowd excitement (0..1), decays by itself */
	ZOOGIROLL_API void Excite(float Amount);
}
