#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArtifacts.h"
#include "ZoogiFloorScars.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture;
class UMaterialInterface;

/**
 * Reactive court floor (Frozen Ring, Oct 6): impacts leave scars that escalate DAMAGED (hairline cracks + frost) ->
 * CRACKED (cyan glow-crack burst) -> CRATER (charcoal crater with cyan cracks, the damaged.png look). Purely visual and
 * flat (decals a few mm over the floor collider), so the court can never become unwinnable or trap a Zoogi.
 * Ember scars one stage deeper and tints them warm, Frost refreezes a scar one stage back. Reset at match start.
 */
UCLASS()
class ZOOGIROLL_API AZoogiFloorScars : public AActor
{
	GENERATED_BODY()

public:
	AZoogiFloorScars();
	void Init(float InMaxRadius);
	/** Returns the scar stage after the impact (0 = none / rejected). Strength ~ impulse-like (1 = light, 3 = huge). */
	int32 Impact(const FVector& P, float Strength, EZoogiArtifact Element);
	/** Forces a scar at P to a stage (tests / shots). */
	void SetScar(const FVector& P, int32 Stage, float Size = 1.f);
	void ResetScars();
	int32 NumScars() const;

	/**
	 * Crater slab hook (Oct 6, A&P anime crater spec): spawns a ring of heaved-up slabs of the map's own floor material
	 * around Loc, tilted out of the plane given by Normal, count/tilt/size scaled by Power (1 = light .. 3 = huge).
	 * Placeholder today: 6-12 tilted box slabs (20-55 deg), pop/overshoot/settle, no collision (never traps a Zoogi).
	 * A data-driven 'CraterSlab' ring (destruction_states.json + curve CSV) can replace the body without touching callers.
	 * Called automatically when a scar reaches CRATER; any arena / Shattered state may call it directly.
	 */
	void SpawnCraterSlabs(const FVector& Loc, const FVector& Normal, float Power, UMaterialInterface* Mat = nullptr);
	void ClearCraterSlabs();
	/** Default slab material for this map (Frozen sets its ice). */
	UPROPERTY() TObjectPtr<UMaterialInterface> SlabMat;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

protected:
	struct FScar
	{
		FVector P = FVector::ZeroVector;
		int32 Stage = 0;
		float Size = 1.f, Yaw = 0.f, Pulse = 0.f, Warm = 0.f, LastAt = -10.f;
		EZoogiArtifact Elem = EZoogiArtifact::None; // last power that hit it (round 3 power overlay layer)
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Layers; // 0 base (crack / crater), 1 glow crack, 2 frost / shards, 3 power overlay / Titan web
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> MIDs;
	};
	void Apply(FScar& S);
	UStaticMeshComponent* NewLayer(float Z);

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pool;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PoolMIDs;
	UPROPERTY() TObjectPtr<UTexture> TexCrack;
	UPROPERTY() TObjectPtr<UTexture> TexGlow;
	UPROPERTY() TObjectPtr<UTexture> TexCrater;
	UPROPERTY() TObjectPtr<UTexture> TexArtCrater;
	UPROPERTY() TObjectPtr<UTexture> TexFrost;
	UPROPERTY() TObjectPtr<UTexture> TexShards;
	// Round 3: colour-baked display decals derived from the Animations & Powers pack (gen_frozen_r3.py).
	UPROPERTY() TObjectPtr<UTexture> TexS1;
	UPROPERTY() TObjectPtr<UTexture> TexS2;
	UPROPERTY() TObjectPtr<UTexture> TexS3;
	UPROPERTY() TObjectPtr<UTexture> TexTitan;
	UPROPERTY() TObjectPtr<UTexture> TexPackCrater;
	UPROPERTY() TObjectPtr<UTexture> TexPowEmber;
	UPROPERTY() TObjectPtr<UTexture> TexPowFrost;
	UPROPERTY() TObjectPtr<UTexture> TexPowStorm;
	TArray<FScar> Scars;
	struct FSlab { float Born = -10.f; FVector Scale = FVector::OneVector; FVector Base = FVector::ZeroVector; FVector Lift = FVector::ZeroVector; };
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SlabPool;
	TArray<FSlab> Slabs;
	int32 NextSlab = 0;
	float MaxRadius = 1480.f;
	float Clock = 0.f, LastNew = -10.f;
	bool bReady = false;
};
