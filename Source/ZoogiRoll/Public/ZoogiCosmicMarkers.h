// Nov 3 b87 (Evan: "for cosmic make these stage alterants that grow from the floor one per round, on cosmic only"):
// each new round on the Cosmic arena one rune marker (SM_CosmicMarker, Evan's red-rune spindle) grows out of the floor at a
// free spot and stays for the rest of the match as a solid obstacle. -ZoogiNoCosmicMarkers turns it off.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiCosmicMarkers
{
	ZOOGIROLL_API void OnMatchStart(AZoogiGameMode* G); // clears last match's markers
	ZOOGIROLL_API void OnRoundStart(AZoogiGameMode* G); // Cosmic only: one more marker rises
}
