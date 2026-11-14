// Nov 2 b67 (Evan: "more P5 style conversations, comments shown between players, AI talks to each other"): short exchanges in the
// P5 speech bubbles (face + line) at the top: a line, then the other Zoogi answers. Triggers: turn starts (taunts), big scores, misses,
// KOs, combos, duels and their results, awaken. One exchange at a time, a few seconds apart. -ZoogiNoBanter = off.
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class UCanvas;

namespace ZoogiBanter
{
	enum class EKind : uint8 { Taunt, TauntReply, DuelOpen, DuelReply, WinGloat, LoseRetort, KOGloat, KOVictim, BigScore, BigScoreReact, MissMock, MissReply, Combo, ComboReact, Awaken, AwakenReact };
	/** A says KindA (about B), B answers with KindB 1.7 s later. Dropped if an exchange is still running (unless bForce). */
	ZOOGIROLL_API void Exchange(AZoogiGameMode* G, int32 A, int32 B, EKind KindA, EKind KindB, bool bForce = false);
	/** One line, no answer. */
	ZOOGIROLL_API void Say(AZoogiGameMode* G, int32 Speaker, int32 About, EKind Kind);
	ZOOGIROLL_API int32 RandomRival(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void Tick(float RealDt);
	ZOOGIROLL_API bool Busy();
	/** Draws the running lines (slot 0 / 1 under each other, starting below a callout bubble when one shows). */
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, float Wd, float Ht, float U, bool bTopTaken);
	ZOOGIROLL_API void Reset();
}
