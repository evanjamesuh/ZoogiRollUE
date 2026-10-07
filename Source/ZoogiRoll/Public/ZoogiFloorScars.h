#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArtifacts.h"
#include "ZoogiFloorScars.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture;

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
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

protected:
	struct FScar
	{
		FVector P = FVector::ZeroVector;
		int32 Stage = 0;
		float Size = 1.f, Yaw = 0.f, Pulse = 0.f, Warm = 0.f, LastAt = -10.f;
		TArray<TWeakObjectPtr<UStaticMeshComponent>> Layers; // 0 base (crack / crater), 1 glow crack, 2 frost / shards
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
	TArray<FScar> Scars;
	float MaxRadius = 1480.f;
	float Clock = 0.f, LastNew = -10.f;
	bool bReady = false;
};
