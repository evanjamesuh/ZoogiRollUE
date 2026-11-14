// Occlusion fade (Evan Oct 23: "make walls and objects transparent so I can see and aim"). Camera-side, presentation only:
// meshes between the camera and the active Zoogi (and a short stretch of the aim line) swap to a lit translucent ghost
// (M_ZoogiDust MID) and ease to ~25% opacity, then ease back and restore their original materials. Collision / physics are
// NEVER touched. Zoogis, the floor and anything under the Zoogi are ignored. CVars: zoogi.Cam.OcclusionFade (1),
// zoogi.Cam.OcclusionOpacity (0.25), zoogi.Cam.OcclusionSpeed (4 /s), zoogi.Cam.OcclusionRadius (45 cm).
#pragma once

#include "CoreMinimal.h"

class APlayerController;
class AZoogiGameMode;

namespace ZoogiOcclusionFade
{
	bool Enabled();
	void Tick(APlayerController* PC, AZoogiGameMode* G, float Dt);
	/** Restore every faded mesh immediately (map change / end play). */
	void RestoreAll();
	/** Debug / tests: number of meshes currently faded (alpha < 1). */
	int32 NumFaded();
}
