// b230 (Evan Nov 10: "Now that we have more 3d models and a wall explosion bring back the cinematic animation of a model getting
// blown back into a wall then provide video options of when people would see that"): the b213 demo reel's rock smash as a short
// in-game cinematic. The attacker's and the victim's Tripo Baller models face off where the victim is, one punch blasts the victim
// back into a rock, the FIRE BLAST (rock blast option 4) bursts out of the rock face, the victim bounces off, KO.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class AZoogiBall;
class UCanvas;

namespace ZoogiWallCine
{
	/** Kind = the trigger option (banner): 1 MATCH POINT, 2 AWAKEN KO, 3 WALL SLAM, 4 FIRST KO, 5 MEGA HIT, 6 REPLAY. */
	ZOOGIROLL_API bool Play(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim, int32 Kind);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	// b238 (Evan Nov 11: "Wall smash moment 1-5"): the real triggers. An offer is queued and plays at the start of the next turn
	// (a quiet moment). Limits per match: MATCH POINT 1, AWAKEN 2, WALL SLAM 2, FIRST KO 1, MEGA HIT 2, 5 in all, never two turns
	// running (-ZoogiWallCineMax=, -ZoogiWallCineAlways = no limits).
	ZOOGIROLL_API bool Offer(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim, int32 Kind);
	/** a knockout: picks MATCH POINT (last round) / AWAKEN (the attacker awakened this turn) / FIRST KO */
	ZOOGIROLL_API void OfferKO(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim);
	ZOOGIROLL_API void NoteAwaken(AZoogiGameMode* G, int32 Player);
	/** b245: the moment's title over the side-window anime (fun HUD canvas) */
	ZOOGIROLL_API void DrawPip(UCanvas* C, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text);
	/** true while the cinematic owns the screen (HUD off, human test holds) */
	ZOOGIROLL_API bool Active();
	/** b239: the round award banner is up (the P5 HUD layers stay off under it) */
	ZOOGIROLL_API bool BannerUp();
	/** the letterbox + banner, drawn by the fun HUD while Active() */
	ZOOGIROLL_API void DrawHud(UCanvas* C, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text);
}
