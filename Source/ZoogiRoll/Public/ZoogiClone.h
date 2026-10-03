#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiClone.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UStaticMesh;

/** Wolfgang's Wolf Pack: a ghostly wolf marble that homes in on an orb or rival for a few seconds, then vanishes. */
UCLASS()
class ZOOGIROLL_API AZoogiClone : public AActor
{
	GENERATED_BODY()

public:
	AZoogiClone();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;

	void InitClone(UStaticMesh* WolfMesh, AActor* InTarget, int32 InOwnerPlayer, float InSpeed);
	virtual void Tick(float DeltaSeconds) override;

	int32 OwnerPlayer = -1;

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	TWeakObjectPtr<AActor> Target;
	float Speed = 1700.f;
	float Age = 0.f;
	bool bHit = false;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> MID;
};
