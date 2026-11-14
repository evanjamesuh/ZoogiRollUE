// b103 (Evan: "make the game the most epic - epic devastating war drums, realistic rock and debris, more realistic physics"):
//  - EPIC HITS (Evan: "don't take the war drum literally"): cinematic trailer impacts (sub drop + body + metal ring), bigger ones and
//    risers (Scripts/synth_epic_sfx.py) on the big moments - knockouts, Awaken powers, meteors, duels, the final round.
//  - DEBRIS: real physics rock chunks + dust burst out of the floor on blasts, meteors and the hardest hits (capped for phones).
//  -ZoogiNoEpicHits / -ZoogiNoDebris turn them off.
#pragma once
#include "CoreMinimal.h"
#include "ZoogiMatchEvents.h"

class AZoogiGameMode;
class UWorld;

namespace ZoogiEpic
{
	enum class EHit : uint8 { Impact, Big, Riser };
	/** Every frame (the fun tick). */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Hit(const UObject* Ctx, EHit H);
	ZOOGIROLL_API void OnMatchEvent(AZoogiGameMode* G, EZoogiMatchEvent Ev, int32 Who, float Mag);
	/** Rock chunks + dust from the floor at At; Power ~0.3 (hard hit) .. 1.5 (meteor strike). */
	ZOOGIROLL_API void DebrisBurst(UWorld* W, const FVector& At, float Power);
}
