// b211 (Evan Nov 9: "splash first then WALK 3 - PLAY TO LEARN (with skip), after WALK 6 - BLUE BUDDY"):
//  first launch -> the splash -> a short guided match against one CPU (the guided first turn + a FIRST MATCH checklist:
//  aim + roll / knock out 2 orbs / use a skill) with a SKIP button -> "YOU'RE READY!" -> home, where Blue (the buddy) gives tips.
//  Saved: [ZoogiRoll.Controls] TutorialDone. -ZoogiTutorialTest = run it anyway (captures), -ZoogiNoTutorial = never.
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiTutorial
{
	/** played or skipped before */
	ZOOGIROLL_API bool Done();
	/** the front calls this once the splash is gone: true = start it now */
	ZOOGIROLL_API bool ShouldAutoRun();
	/** start the guided match (also the buddy's REPLAY TUTORIAL) */
	ZOOGIROLL_API void Begin(AZoogiGameMode* G);
	ZOOGIROLL_API bool Active();
}
