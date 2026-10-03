#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/** Short-lived additive glow: an expanding sphere or flat ring with a light flash (powers, impacts, knock-offs). */
UCLASS()
class ZOOGIROLL_API AZoogiFX : public AActor
{
	GENERATED_BODY()

public:
	AZoogiFX();

	/** Spawns an effect. Radius is the final radius in cm. bRing = flat shockwave ring instead of a sphere. */
	static AZoogiFX* Spawn(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, bool bRing = false, float Strength = 6.f, AActor* FollowActor = nullptr);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Flash;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	UPROPERTY() TWeakObjectPtr<AActor> Follow;
	float Age = 0.f;
	float Life = 0.6f;
	float EndRadius = 300.f;
	float BaseStrength = 6.f;
	float BaseLight = 0.f;
	bool bIsRing = false;
};
