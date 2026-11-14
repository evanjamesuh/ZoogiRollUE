// b136 (Evan 08:22, "the cosmos boss as a watcher"): a colossal dark mech (SK_Mecha_Aurex_03, violet armour, red eyes) rises out of
// the void beyond the Cosmic arena and watches the match:
//  - it stays on the far side of the arena from the camera (drifting slowly), its head level with the court, and turns to follow the leader
//  - KO: a slow power gesture, eyes blaze, rumble. Critical: eyes flare. COMBO (REDIRECT x5+ / combo anime): it leans in and pumps a fist.
//  - Miss: a scoffing eye flicker and a mocking gesture. Final round: it rises a little higher and roars (eyes stay hot).
// Cosmic only (-ZoogiCosmosBossAll = every arena for tests, -ZoogiNoCosmosBoss = off).
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class UWorld;

namespace ZoogiCosmosBoss
{
	ZOOGIROLL_API void OnMatchStart(AZoogiGameMode* G);
	ZOOGIROLL_API void OnRoundStart(AZoogiGameMode* G);
	/** anime events: KO / Critical / Combo / Miss / PoweredHit ... */
	ZOOGIROLL_API void OnEvent(UWorld* W, const FString& Ev);
	// b146 (ZoogiBossAttack): the peeking boss as the attacker
	ZOOGIROLL_API bool Present(const AZoogiGameMode* G);
	/** b148a: this map's boss ("FROST TITAN" ...) and its colour */
	ZOOGIROLL_API FString Name();
	/** b151 RAID */
	ZOOGIROLL_API float Angle();
	/** b216: the Tripo titan's middle + half height (Meadow golem / Dark Realm brute), false when there is none */
	ZOOGIROLL_API bool TitanFocus(FVector& OutCenter, float& OutHalfH);
	/** b217: the boss cam is showing the titan - the HUD banners stay off so nothing covers it */
	ZOOGIROLL_API bool BossCamOn();
	ZOOGIROLL_API void HoldAt(bool bOn, float AngDeg);
	ZOOGIROLL_API void Flinch();
	ZOOGIROLL_API void Defeat();
	ZOOGIROLL_API FLinearColor Tint();
	/** bOn: both hands reach into the court at AngDeg (the sweep drives the angle); off = back to peeking. */
	ZOOGIROLL_API void Sweep(bool bOn, float AngDeg);
	/** comes up on that side and slams the rim */
	ZOOGIROLL_API void SlamToward(float AngDeg);
	/** the warning: rears up, eyes blaze */
	ZOOGIROLL_API void Taunt();
}
