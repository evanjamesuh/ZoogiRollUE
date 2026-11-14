// b149 (Evan Nov 5 18:43, "the Battle Royale version"): BATTLE ROYALE is now a mix of the two games. Its rounds alternate:
//   CLASSIC round - the turn-based marble game (every Zoogi takes one turn; the map's boss attacks after the last shot)
//   ACTION round  - the real-time brawl: drive, JUMP, TRANSFORM ball <-> robot and shoot, for a fixed time (endless lives)
// and every round moves to the next arena. POINTS PER ROUND: the classic rounds score as Classic does; an action round adds
// +KO points for every ring-out / destroy you get and -fall points for every life you lose. The totals carry from round to round
// and the highest total after the last round wins (the last round is always a CLASSIC round, so the match ends on its results).
// Round count = the PLAY screen's rounds. -ZoogiRoyaleOld = the b148 lives-only brawl, -ZoogiMixActionSec=45 = action round length.
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiRoyaleMix
{
	/** The royale card plays the mix (default on). */
	ZOOGIROLL_API bool Enabled();
	/** A mix match is running. */
	ZOOGIROLL_API bool Active();
	/** The current round is an ACTION round. */
	ZOOGIROLL_API bool ActionRound();
	/** Action round length (s). */
	ZOOGIROLL_API float ActionSeconds();
	/** StartMatch in royale mode: starts round 1. */
	ZOOGIROLL_API void Begin(AZoogiGameMode* G);
	/** FinishGame of a classic round: true = handled (the next round follows); false on the last round (its results are the match's). */
	ZOOGIROLL_API bool OnClassicFinished(AZoogiGameMode* G);
	/** RoyaleFinish during an action round: true = handled. */
	ZOOGIROLL_API bool OnRoyaleFinished(AZoogiGameMode* G);
	/** Unscaled seconds: the action round clock. */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** Back to the menu: the mix is over (the PLAY screen keeps BATTLE ROYALE selected). */
	ZOOGIROLL_API void Stop(AZoogiGameMode* G);
	ZOOGIROLL_API void OnMenu(AZoogiGameMode* G);
}
