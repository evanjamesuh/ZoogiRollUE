// Nov 2 b63 (Evan: "game and players start standing as ultimate form, player remains as such until action is taken"): when a
// classic match starts every Zoogi stands on the court as its ultimate form (the Tripo form, posed by AZoogiAurexPuppet); it turns
// back into its ball the moment that player takes an action (its launch), or when something knocks it. -ZoogiNoStandForms = off.
// b67 (Evan: "Players appear standing once they stop rolling but no transform sounds until it's that player's turn; stop all
// transform prior to a hit"): a Zoogi that comes to rest stands up again quietly (no pop sound / flash); being knocked turns it back
// into its ball quietly; the transform sound plays only for the player whose turn starts. -ZoogiStandOnce = only at match start.
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;

namespace ZoogiStandForms
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** Turns B back into its ball now (another puppet - parry / edge hang - is about to take over). */
	ZOOGIROLL_API void Release(AZoogiBall* B, bool bFx = true);
	ZOOGIROLL_API bool IsStanding(const AZoogiBall* B);
}
