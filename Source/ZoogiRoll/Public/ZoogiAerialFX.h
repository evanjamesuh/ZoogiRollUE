// Oct 9 (Evan, refs sky_bumper_detail / air_reaction_vs_stone_orb): cosmetic aerial FX. Visual only, NoCollision, no
// gameplay state, local random streams (never GameRng): RULE ZERO.
//   AZoogiImpactFlash : Sky Bumper hit - bright star-burst of jagged cyan / white / orange rays + flying ice shards + light
//                       pulse (no round sparkles).
//   AZoogiAirReticle  : cyan targeting reticle (ring, dashed inner ring, tick marks, corner brackets) around a Zoogi while
//                       its Air Reaction offer is open; faces the camera.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiAerialFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ZOOGIROLL_API AZoogiImpactFlash : public AActor
{
	GENERATED_BODY()
public:
	AZoogiImpactFlash();
	static AZoogiImpactFlash* Spawn(UWorld* W, const FVector& At, const FVector& Normal, int32 Seed, float Scale = 1.f);
	virtual void Tick(float Dt) override;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rays;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Glows;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shards;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
protected:
	void Setup(const FVector& Normal, int32 Seed, float Scale);
	void Place(float K);
	struct FRay { FVector Dir; float Len; float Thick; FVector Kink1, Kink2; };
	struct FShard { FVector Vel; FVector Axis; float Spin; float Len; };
	TArray<FRay> RayDefs;
	TArray<FShard> ShardDefs;
	FVector Origin = FVector::ZeroVector;
	float Age = 0.f, Life = 0.42f, Scale = 1.f;
};

UCLASS()
class ZOOGIROLL_API AZoogiAirReticle : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAirReticle();
	void Follow(AActor* InTarget, float InRadius);
	void Release() { bActive = false; }
	bool IsFollowing(const AActor* A) const { return Target.Get() == A; }
	virtual void Tick(float Dt) override;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segs;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
protected:
	TWeakObjectPtr<AActor> Target;
	float Radius = 60.f, Age = 0.f, Alpha = 0.f;
	bool bActive = false;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatRing;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatHot;
};
