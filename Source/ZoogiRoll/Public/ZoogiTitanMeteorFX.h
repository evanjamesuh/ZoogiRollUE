#pragma once

#include "CoreMinimal.h"
#include "ZoogiAPFXBase.h"
#include "ZoogiTitanMeteorFX.generated.h"

class UStaticMeshComponent;
class UDecalComponent;
class UPointLightComponent;
class UPostProcessComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;

/** Optional knobs for AZoogiTitanMeteorFX (defaults are the reference look; see ANIM_POWERS_API.md). */
USTRUCT()
struct FZoogiMeteorOptions
{
	GENERATED_BODY()
	/** Symbolic rock size (cm). The Frozen floor is 30 m across; 2600 = "that thing is coming HERE". */
	UPROPERTY() float RockDiameter = 1700.f;
	/** Height the rock starts at above the impact point (cm). It comes in along the real attack direction (Start -> Impact). */
	UPROPERTY() float SkyHeight = 7000.f;
	/** Fraction of Seconds at which the rock reaches the impact point (0 = auto: 0.80, or 0.62 for a short / Competitive play < 1.6 s). */
	UPROPERTY() float ImpactFraction = 0.f;
	/** Darken + warm-grade the arena into the Titan perception space (post process, HUD untouched). */
	UPROPERTY() bool bPerceptionSpace = true;
	/** Rumble -> near silence -> impact transient (2D, plays while the game is paused). */
	UPROPERTY() bool bAudio = true;
	/** Optional: the real target Zoogi (read only). Used only to keep the debris ring clear of it. */
	TWeakObjectPtr<AActor> Target;
	/** Rock mesh variant (0 = Tomb canyon boulder, 1 = KriptoFX rock with AwakenFX stone material). -ZoogiMeteorRock=N overrides. */
	UPROPERTY() int32 RockVariant = -1;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FZoogiMeteorImpactEvent, const FVector& /*ImpactPoint*/);

/**
 * TITAN METEOR (Episode Engine spec v1.1, section 11 + priority 10): a giant symbolic meteor wrapped around the REAL
 * impact point. Presentation only (RULE ZERO): it reads nothing from the simulation except what you pass in, has no
 * collision / physics, moves no gameplay actor, and leaves nothing behind (every mark fades out by the end, so it
 * never suggests terrain damage the game did not do). It ticks on real time and while the game is paused, so it plays
 * inside UZoogiEpisodeSubsystem's presentation hold.
 *
 *   AZoogiTitanMeteorFX::PlayMeteor(World, Start, Impact, Seconds);
 *     Start   = where the real attack came from (the attacker / launch point). Only its horizontal direction is used:
 *               the rock descends along Start -> Impact, from SkyHeight above.
 *     Impact  = the validated impact point (e.g. FZoogiEpisodeCue::ImpactPoint = target ball location). The rock's
 *               lowest point reaches exactly this XY (on the floor under it) at Seconds * ImpactFraction.
 *     Seconds = total length in real seconds (the hold length). < 1.6 s plays the short (Competitive) variant.
 *
 * Beats (u = 0..1 of Seconds, impact at uI): 0-0.15 arena darkens to the Titan perception space, low rumble, dust and
 * pebbles start to lift; 0.05-uI the rock comes in from the sky along the attack direction (slow far reveal, then
 * accelerating), its shadow grows over the target, debris vibrates; uI-0.1 near silence; uI white/stone flash, impact
 * light, shockwave ring + refraction ring, dust wall, Titan fracture lines across the floor, the rock collapses into
 * chunks; after uI the perception space lifts so the real arena returns at the real result by u = 1.
 *
 * Camera is not taken (Game Camera owns it). GetSuggestedView() gives the reference framing for each beat and
 * GetShake() a 0..1 shake curve; the -ZoogiMeteorTest flag uses both.
 */
UCLASS(NotPlaceable)
class ZOOGIROLL_API AZoogiTitanMeteorFX : public AZoogiAPFXBase
{
	GENERATED_BODY()
public:
	AZoogiTitanMeteorFX();

	/** Spawn + Play in one call. Returns null only without a world. */
	static AZoogiTitanMeteorFX* PlayMeteor(UWorld* World, const FVector& Start, const FVector& Impact, float Seconds, const FZoogiMeteorOptions& Options = FZoogiMeteorOptions());

	/** Starts the sequence on an already spawned actor (call once). */
	void Play(FVector Start, FVector Impact, float Seconds);
	/** End early (e.g. the hold was released early): everything fades out over FadeSeconds, then the actor destroys itself. */
	void Stop(float FadeSeconds = 0.2f);

	FZoogiMeteorOptions Options;
	/** Fires once, on the impact frame. */
	FZoogiMeteorImpactEvent OnImpact;

	bool HasImpacted() const { return bImpacted; }
	/** Real seconds from Play to the impact frame. */
	float GetImpactTime() const { return Seconds * ImpactU; }
	FVector GetImpactPoint() const { return Contact; }
	FVector GetRockLocation() const;
	float GetRockRadius() const { return RockR; }
	/** 0..1 suggested camera shake (rises with the descent, spikes at impact, decays). */
	float GetShake() const;
	/** Reference framing per beat: low angle on the target looking up (A/D), wide scale reveal (E), close impact. */
	bool GetSuggestedView(FVector& OutLocation, FRotator& OutRotation, float& OutFOV) const;

protected:
	virtual bool TickFX(float Dt, float U) override;

	FVector StartP = FVector::ZeroVector, ImpactP = FVector::ZeroVector, Contact = FVector::ZeroVector, Dir = FVector::ForwardVector;
	FVector SkyFrom = FVector::ZeroVector, RockEnd = FVector::ZeroVector, RockPos = FVector::ZeroVector, PrevRockPos = FVector::ZeroVector;
	float ImpactU = 0.8f;
	float RockR = 1300.f;
	float RockUnit = 1.f;
	float FloorZv = 0.f;
	bool bShort = false;
	bool bImpacted = false;
	float ImpactAge = 0.f;
	float StopAt = -1.f, StopLen = 0.2f;
	bool bSilenced = false;
	FRotator RockRot0 = FRotator::ZeroRotator;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Rock;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Chunks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pebbles;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Puffs;      // trail smoke (sky)
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DustRing;   // impact dust wall
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Refract;
	UPROPERTY() TObjectPtr<UDecalComponent> Shadow;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Scar;      // temporary dark Titan fracture stain (fades out, nothing persists)
	UPROPERTY() TObjectPtr<UPointLightComponent> ImpactLight;
	UPROPERTY() TObjectPtr<UPointLightComponent> UnderLight;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TObjectPtr<UAudioComponent> Rumble;

	struct FBody { FVector P, V; FRotator R, W; float S0 = 1.f; float Born = 0.f; };
	TArray<FBody> ChunkB, PebbleB, PuffB, DustB;
	struct FCrack { TArray<FVector> Pts; float Width = 20.f; float Delay = 0.f; };
	TArray<FCrack> Cracks;
	int32 NextPuff = 0;
	float PuffAcc = 0.f;
	FBatch BFlat, BGlow, BRing, BMote, BDark;
	FVector From = FVector::BackwardVector;   // horizontal direction the rock comes from (attack line biased away from the view)

	void Build();
	void BuildCracks();
	void Impact();
	float GlobalFade() const;
	void Sfx(const TCHAR* Name, float Vol, float Pitch = 1.f);
};
