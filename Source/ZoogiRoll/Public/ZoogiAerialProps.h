// Battle Episode Engine (T3, refs frozen_zones_aerial / sky_bumper_detail / resonance_halo_detail / rail_release_timing):
// gameplay props for the Frozen Ring aerial layer. DEBUG GEOMETRY ONLY (collision shapes + debug lines, no art).
// Off by default: spawned only with [ZoogiRoll.Battle] bAerialProps=true, -ZoogiBattleProps, or by a battle test.
//
//   SkyBumper     : spring node; a Zoogi that hits it is redirected along the reflection of its incoming velocity about
//                   the contact normal (computed from the two centres) at ReboundMul x speed (default 2.0, capped).
//   ResonanceHalo : a standing / hanging stone ring. Passing through the opening ("pass") and striking the rim ("rim",
//                   a normal physics rebound) are reported separately. Halo Bank uses them.
//   LaunchRail    : entry of a launch rail; a Zoogi entering it starts the Rail Release timing mini-game.
// Hits come from the prop's own hit notify and from the Zoogi's (the battle state machine forwards); a per-Zoogi
// cooldown makes it one event. Trigger volumes have no collision and are polled by the state machine.
// RULE ZERO: these are gameplay mechanics acting through normal physics velocity changes in the simulation (no time
// dilation, no presentation holds), behind settings, so default play is unchanged.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiAerialProps.generated.h"

class USphereComponent;
class UBoxComponent;
class AZoogiBall;

UENUM()
enum class EZoogiPropKind : uint8
{
	SkyBumper,
	ResonanceHalo,
	LaunchRail
};

UCLASS()
class ZOOGIROLL_API AZoogiAerialProp : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAerialProp();
	EZoogiPropKind Kind = EZoogiPropKind::SkyBumper;
	int32 PropId = 0;
	FVector Forward = FVector::ForwardVector;   // halo pass axis / rail direction
	float Size = 70.f;
	float ReboundMul = 2.f;
	float ReboundMaxSpeed = 3200.f;
	UPROPERTY() TArray<TObjectPtr<USphereComponent>> Solids;
	UPROPERTY() TObjectPtr<UBoxComponent> Trigger;

	void Build(EZoogiPropKind InKind, int32 Id, const FVector& Location, const FVector& InForward, float InSize);
	FString Label() const;
	void DrawDebug() const;

	UFUNCTION() void OnSolidHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	/** Pass / rail trigger volume (no collision: polled by the battle state machine, plane crossing / entry). */
	bool InTrigger(const FVector& Local) const;
	// last Sky Bumper rebound (tests / log)
	FVector LastIn = FVector::ZeroVector, LastOut = FVector::ZeroVector, LastNormal = FVector::ZeroVector;
	int32 Rebounds = 0;
	/** One event per Zoogi per channel (0 = solid hit, 1 = trigger) within Seconds of game time. */
	bool Cooldown(AActor* A, float Seconds, int32 Channel = 0);
protected:
	TMap<TWeakObjectPtr<AActor>, float> LastEvent[2];
};
