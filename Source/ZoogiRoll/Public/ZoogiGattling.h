// b182 (Evan Nov 8): SHOWTIME - the GATTLING moment (Chain Dash x Beat Floor). A reward the leader earns:
//  - its own gold SHOWTIME meter (separate from AWAKEN) fills with combos - orb + rival hits in a row - only while you lead;
//  - full + still leading -> a gold SHOWTIME button; the match pauses, it's just you and the music (~32 beats on the theme);
//  - the court becomes a dance floor of hex tiles flashing to the beat, the next target tile lights up with an orb gushed from the
//    generator; tap on the beat (anywhere - assisted) and your Zoogi dash-hops through it; chain them; hold = auto-fire on the beat;
//  - the finale SHOWDOWN blasts the rivals outward, harder the bigger the combo.
// -ZoogiNoGattling = off; -ZoogiGattlingTest = auto-fills + auto-taps on the first human aim (frames); -ZoogiGattlingFast = 4x meter.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class AZoogiBall;
class AActor;
class UCanvas;
class APlayerController;

namespace ZoogiGattling
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	/** the shooter touched a Zoogi / orb (AZoogiGameMode::OnShooterContact) - fills the meter */
	ZOOGIROLL_API void OnContact(AZoogiGameMode* G, AZoogiBall* Shooter, AActor* Other);
	/** SHOWTIME is playing (the match is paused; it owns the pointer) */
	ZOOGIROLL_API bool Active();
	/** pointer while Active */
	ZOOGIROLL_API void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease);
	/** a press on the SHOWTIME button starts it; true = consumed */
	ZOOGIROLL_API bool TryButton(AZoogiGameMode* G, const FVector2D& Ptr);
	/** b194: the SHOWTIME command in the P5 menu - ready for the human on this aim (meter full) / start it */
	ZOOGIROLL_API bool Ready(AZoogiGameMode* G);
	ZOOGIROLL_API bool StartFromMenu(AZoogiGameMode* G, bool bMidRoll = false); // b195: a COMBO can start it mid-roll
	ZOOGIROLL_API float Meter(int32 Player);
	ZOOGIROLL_API void Reset();
}
