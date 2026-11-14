// b179 (Evan Nov 7, picked options 3 + 4 to replace the CPU-only STRIKE combos): on every human turn
//  - BOUNTY: the game crowns one rival (the score leader) - make it your FIRST hit for bonus points;
//  - TURN CHALLENGE: a small optional goal (2 orbs in one roll, hit 2 rivals, knock a rival off...) - done = AWAKEN meter + points.
// Success only: nothing is lost and no banner shows when you miss. -ZoogiNoBounty / -ZoogiNoTurnChallenge turn them off.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class AZoogiBall;
class AActor;
class UCanvas;
class APlayerController;

namespace ZoogiBounty
{
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	/** the shooter touched a Zoogi / orb (AZoogiGameMode::OnShooterContact) */
	ZOOGIROLL_API void OnContact(AZoogiGameMode* G, AZoogiBall* Shooter, AActor* Other);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	ZOOGIROLL_API void Reset();
	/** b200: the raid weak point took a hit (ZoogiRaid::Damage) */
	ZOOGIROLL_API void OnRaidHit(AZoogiGameMode* G, int32 Player, const TCHAR* What, const FVector& At);
}
