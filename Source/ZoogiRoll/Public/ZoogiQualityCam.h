// quality items 4 + 31 (Evan / ZOOGI Oct 22): presentation-only edge slow-mo + best-shot replay into the side-panel PiP.
// Physics is NEVER dilated here (no SetGlobalTimeDilation). Edge slow-mo uses EpisodeCameraDirector MotionRate / grade on a
// short MainView EdgeFallArena shot that frames the arena; best-shot replays render to GetReplayPipTexture (null when idle).
#pragma once

#include "CoreMinimal.h"
#include "ZoogiShotLibrary.h"
#include "ZoogiEpisodeCameraDirector.h"

class AZoogiPlayerController;
class AZoogiBall;
class AZoogiGameMode;
class UTextureRenderTarget2D;

namespace ZoogiQualityCam
{
	/** CVars: zoogi.Cam.EdgeSlowMo (default 1), EdgeSlowMoScale, EdgeSlowMoHold, EdgeSlowMoDuration. */
	bool EdgeSlowMoEnabled();
	/** CVars: zoogi.Cam.BestShotReplay (default 1), BestShotHold, BestShotFps, BestShotMinAir. */
	bool BestShotEnabled();

	/** Rising-edge of a Zoogi leaving play -> brief MainView EdgeFallArena (arena framed, presentation slow-mo), then exact restore. */
	bool TryEdgeSlowMo(AZoogiPlayerController* PC, AZoogiBall* Falling, AZoogiGameMode* G);

	/** Notable moment -> short RenderTarget replay into GetReplayPipTexture + OnGameplayPipShot(REPLAY). Never takes MainView. */
	bool TryBestShotReplay(AZoogiPlayerController* PC, AZoogiBall* Focus, FName Reason);

	/** Per-frame: detect edge falls + best-shot heuristics; advance replay hold / clear GetReplayPipTexture when idle. */
	void Tick(AZoogiPlayerController* PC, AZoogiGameMode* G, float Dt);

	UTextureRenderTarget2D* GetReplayPipTexture(const AZoogiPlayerController* PC);
	UTextureRenderTarget2D* GetGameplayPipTexture(const AZoogiPlayerController* PC);
	void SetGameplayPipActive(AZoogiPlayerController* PC, bool bActive);

	/** ShotTest / debug: force an edge slow-mo or best-shot without waiting for heuristics. */
	bool ForceEdgeSlowMo(AZoogiPlayerController* PC, AZoogiBall* Falling);
	bool ForceBestShot(AZoogiPlayerController* PC, AZoogiBall* Focus, FName Reason);

	/** True while an EdgeFallArena MainView shot from us is owning the view. */
	bool IsEdgeSlowMoActive(const AZoogiPlayerController* PC);
	/** True while a best-shot RenderTarget replay is holding the side panel. */
	bool IsBestShotActive(const AZoogiPlayerController* PC);

	/** Build EdgeFallArena spec with live cvar scale/hold/duration (presentation MotionRate only). */
	FZoogiShotSpec MakeEdgeFallSpec();
}
