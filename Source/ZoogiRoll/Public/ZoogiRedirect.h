// Nov 2 b66 (Evan): the new turn. "imagine a turn no more react to boost after launch ... now it hits as if it were tapped and
// boosted, then at this point while the crosshairs is shown on the floor you can tap reaction to pause then target ... within a arc
// radius to redirect a smash that direction once per crystal (the circles under the profile avatar) that has to be accumulated over
// time, multiple crystals gives more target selections and a opportunity to develop a pattern combo". CPU gets it too: it must earn it.
//  - crystals = AZoogiGameMode::AwakenCharge (the pips under each avatar), +1 at the start of each of that player's turns (max 4)
//  - REACTION while the ball rolls -> world near-frozen, range ring around the ball, glowing targets inside it -> tap one -> the ball
//    smashes off toward it (1 crystal; Wolf Pack skill picks are free); COMBO xN per turn
//  - a redirect aimed at a rival Zoogi arms the DUEL (UZoogiFunSys::StartDuel) for when they meet
//  - Bolt Balls skill: every redirect (and the launch) zaps the nearest targets with the electric shock
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class AActor;
class APlayerController;
class UCanvas;

namespace ZoogiRedirect
{
	ZOOGIROLL_API bool Available(AZoogiGameMode* G);   // REACTION would open a redirect pick right now (human ball rolling + a crystal / free pick)
	ZOOGIROLL_API bool Begin(AZoogiGameMode* G);       // REACTION pressed
	ZOOGIROLL_API bool Picking();
	ZOOGIROLL_API void CancelPick(const TCHAR* Why); // b122b: closes an open pick (nothing spent)
	ZOOGIROLL_API void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	ZOOGIROLL_API int32 Combo();                      // redirects this turn
	ZOOGIROLL_API int32 Crystals(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void GrantFree(int32 Player, int32 N); // Wolf Pack skill: N free picks this turn
	ZOOGIROLL_API void ArmBolt(int32 Player);            // Bolt Balls skill: this turn's redirects zap
	/** SuperContact: Sh (the redirected shooter) met R - true once if R was the redirect's rival target (start the duel). */
	ZOOGIROLL_API bool TakeDuel(AZoogiBall* Sh, AZoogiBall* R);
	ZOOGIROLL_API float Range(AZoogiGameMode* G);
	ZOOGIROLL_API void Reset();
}
