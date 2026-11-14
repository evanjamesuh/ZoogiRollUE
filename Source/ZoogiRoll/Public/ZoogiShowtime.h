#pragma once
// b122 (Evan: "when SHOWTIME is pressed the phone vibrates for 2 s, the ancient symbols glow like the Frozen arena, the lights dim and the
// player gets the Energy Buff effect, then they select a target"). -ZoogiOldShowtime = off.
#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class APlayerController;

namespace ZoogiShowtime
{
	ZOOGIROLL_API void Begin(AZoogiGameMode* G, AZoogiBall* B);
	/** the "tap your target" step is open (the player controller sends field taps here) */
	ZOOGIROLL_API bool Picking();
	ZOOGIROLL_API void Tap(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Screen);
	/** b142: SHOWTIME armed this player's Awaken for this turn (the SUPERPOWER button fires it) */
	ZOOGIROLL_API bool Armed(const AZoogiGameMode* G, int32 Player);
	/** b142: SUPERPOWER with an armed Awaken: the power (Hotstreak = meteor rain), then the full screen anime. True = fired. */
	ZOOGIROLL_API bool FireArmed(AZoogiGameMode* G);
	/** b243 (Evan): Settings > DARK ARENA keeps the SHOWTIME light dim on for the whole match. Call every frame from the match tick. */
	ZOOGIROLL_API void DarkTick(AZoogiGameMode* G);
}
