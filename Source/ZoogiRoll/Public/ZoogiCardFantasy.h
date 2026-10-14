// Oct 14 (Evan): "fantasy" card layer. Every card gets an exaggerated, visible effect on top of its card-book rule.
// Cards mode only (UZoogiCardSubsystem never runs it with bCards=False, so cards-off parity is unchanged) and never during
// the scripted card tests (-ZoogiCardTest=...), which keep checking the book numbers. Values: [ZoogiRoll.CardFantasy] in
// DefaultGame.ini. The matching equipped power (Powers.csv card_id) amplifies its card by PowerAmp.
#pragma once

#include "CoreMinimal.h"

class AActor;
class AZoogiBall;
class AZoogiOrb;

struct ZOOGIROLL_API FZoogiCardFantasyTune
{
	bool bOn = true;
	float PowerAmp = 1.5f;
	// Storm: chain lightning
	int32 StormLinks = 3; float StormChainR = 900.f, StormPush = 1100.f;
	// Bank Strike: pinball
	float BankStep = 0.25f, BankMaxSpeed = 4800.f;
	// Guard: shield dome reflect
	float GuardReflect = 0.9f;
	// Orb Relay: beam pull to the partner
	float RelayRange = 1400.f, RelayPull = 1100.f, RelayTime = 0.9f;
	// Heavy Roll: boulder
	float HeavyScale = 2.f, HeavyMass = 3.f, HeavyScatterR = 260.f, HeavyScatterPush = 1000.f;
	// Ember: fireballs + burning patches
	int32 EmberCount = 3; float EmberSpreadDeg = 18.f, EmberSpeed = 1800.f, EmberLife = 0.9f, EmberHitPush = 900.f, EmberPatchR = 170.f;
	// Titan: meteor at the aim point
	float TitanMeteorR = 520.f, TitanMeteorPush = 1300.f, TitanDelay = 0.9f;
	// Frost: ice beam freeze
	float FrostRange = 1800.f, FrostConeDeg = 40.f; int32 FrostTurns = 1;
	// Precision: homing + tap redirects
	float PrecisionConeDeg = 40.f, PrecisionTurnDeg = 140.f; int32 PrecisionRedirects = 2;
	// Momentum Steal: tether
	float StealRange = 1200.f, StealTime = 1.5f, StealRate = 0.9f, StealOnHit = 0.4f;
	// Crystal Collapse: spike burst (pushes Zoogis only; no extra destruction)
	float CrystalSpikeR = 420.f, CrystalSpikePush = 1200.f;
	// Orb Magnet: vortex
	float MagnetR = 1000.f, MagnetTime = 1.5f, MagnetAccel = 900.f, MagnetSwirl = 0.5f;
	// Double Impact: shadow clone second hit (fraction of the first hit's push)
	float DoubleCloneHit = 0.8f;
	// chain arrow trail (golden kit arrow); ChainArrowFX = optional Niagara path to swap/add a visual
	float ChainArrowTime = 0.9f; FString ChainArrowFX;
	// Anchor Guard: near-immovable for a round
	float AnchorMass = 8.f, AnchorPushK = 0.15f;
	// Recovery Roll: mid-air dash back to the centre
	float RecoveryDash = 1500.f, RecoveryUp = 350.f;
	void Load();
};

struct FZoogiCardFantasyState
{
	struct FFireball { FVector P = FVector::ZeroVector, V = FVector::ZeroVector; float Life = 0.f, PuffT = 0.f; int32 Owner = -1; };
	struct FPatch { FVector At = FVector::ZeroVector; float R = 0.f; int32 Owner = -1; TSet<int32> Burned; };
	TArray<FFireball> Fireballs;
	TArray<FPatch> Patches;
	// Titan meteor
	bool bMeteor = false; FVector MeteorAt = FVector::ZeroVector; float MeteorT = 0.f, MeteorR = 0.f, MeteorPush = 0.f; int32 MeteorOwner = -1;
	// Precision homing
	TWeakObjectPtr<AActor> HomingTarget; FVector HomingPoint = FVector::ZeroVector; bool bHomingPoint = false; int32 HomingOwner = -1, RedirectsLeft = 0; float HomingTurn = 0.f;
	// Momentum Steal tether
	TWeakObjectPtr<AZoogiBall> TetherRival; int32 TetherOwner = -1; float TetherT = 0.f, BeamT = 0.f;
	// Orb Relay beam
	TWeakObjectPtr<AZoogiOrb> RelayOrb; TWeakObjectPtr<AZoogiBall> RelayTo; float RelayT = -1.f;
	// Orb Magnet vortex
	int32 VortexOwner = -1; float VortexT = -1.f;
	// Bank Strike
	int32 BankBounces = 0; float BankLastT = -10.f; FVector BankPrev = FVector::ZeroVector;
	// Heavy Roll boulder
	int32 HeavyOwner = -1; float HeavySavedMass = 0.f, HeavySavedScale = 1.f; TSet<int32> Scattered;
	// once-per-shot latches
	bool bStormDone = false, bSpikesDone = false, bCloneShown = false;
	// Anchor Guard round (owner -> saved mass) and Recovery Roll owners (until their next Standby)
	TMap<int32, float> Anchors;
	TMap<int32, bool> Recovery; // owner -> dash used
	int32 RecoveryPrompt = -1;
	void ResetShot() { Fireballs.Reset(); bMeteor = false; HomingTarget.Reset(); bHomingPoint = false; HomingOwner = -1; TetherRival.Reset(); TetherOwner = -1; RelayOrb.Reset(); RelayTo.Reset(); RelayT = -1.f; VortexOwner = -1; VortexT = -1.f; BankBounces = 0; Scattered.Reset(); bStormDone = bSpikesDone = bCloneShown = false; }
};
