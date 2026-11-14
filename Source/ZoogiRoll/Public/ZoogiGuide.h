// Nov 2 b66 (Evan: "let's run through a guided first turn"; playtest: no onboarding, nine choices on turn one): the guided first turn.
// b67 (Evan: "Players have option to play guide on first story mission"): only runs when armed - the GUIDE toggle on the first
// story stop (default ON until it has been played once). Classic matches no longer auto-run it. -ZoogiGuideTest forces it.
// Was: the player's first classic match (or -ZoogiGuideTest): 4 crystals, the command menu's SKILL / GUARD / ITEM hidden, then step cards:
// 1 drag back + release (ghost hand)  2 launches always hit boosted  3 tap REACTION to redirect (the world slows)  4 tap a glowing
// target  5 chain more redirects (combo)  6 a rival in range = DUEL  -> done after that turn (saved: [ZoogiRoll.Controls] GuideDone=1).
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class APlayerController;
class UCanvas;

namespace ZoogiGuide
{
	ZOOGIROLL_API bool Active();
	ZOOGIROLL_API bool HideMenus();   // P5 command menu: ATTACK only on the guided turn
	ZOOGIROLL_API bool WantSlow();    // the world slows while step 3 waits for REACTION
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	/** b67: the guided turn has been played before ([ZoogiRoll.Controls] GuideDone) */
	ZOOGIROLL_API bool DoneBefore();
	/** b67: the next match runs the guided first turn (the first story mission's GUIDE toggle) */
	ZOOGIROLL_API void Arm(bool bOn);
}
