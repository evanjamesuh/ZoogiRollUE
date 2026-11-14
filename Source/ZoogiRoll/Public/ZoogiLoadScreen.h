// b207 (Evan Nov 9 "yes to both loading screens ... put these in game"): a P5 loading screen before every RAID and CLASSIC match
//  - RAID (option E): the boss splash, "RAID: <BOSS>", 3 tip cards (HIT THE WEAK POINT / SURVIVE ITS ATTACKS / KEEP YOUR TEAM LIVES),
//    the loading bar, then TAP TO START.
//  - CLASSIC (option F): the map art, "CLASSIC - <MAP>", 3 tips (KNOCK ORBS OUT / KNOCK RIVALS OFF / SKILLS, ITEMS, REACTION),
//    a rotating TIP line, the loading bar, then TAP TO START.
//  The match holds (no stage forming, no boss intro) until the tap. Armed by MODE SELECT's map cards (StartMapCard);
//  -ZoogiLoadScreen = also on a command-line match (tests: it starts by itself after a few seconds with -ZoogiAuto);
//  -ZoogiNoLoadScreen = off.
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiLoadScreen
{
	/** MODE SELECT map card: the next match shows the loading screen (Art = "ModeSelect/ms_map_cosmic"). */
	ZOOGIROLL_API void Arm(bool bRaid, const FString& MapName, const FString& Art);
	/** b283: every other mode card (Battle Royale, party modes, training, trick shot) - the mode is read when the match starts. */
	ZOOGIROLL_API void ArmMode();
	/** StartMatch: true = hold the match on the loading screen. */
	ZOOGIROLL_API bool Begin(AZoogiGameMode* G);
	ZOOGIROLL_API bool Holding();
	/** the Forming phase while holding (taps, the test auto start) */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
}
