#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiOrb.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPhysicalMaterial;

/** Glowing light-blue orb. Knock it off the edge to score (KNOCKOUT_SCORE_ORB). */
UCLASS()
class ZOOGIROLL_API AZoogiOrb : public AActor
{
	GENERATED_BODY()

public:
	AZoogiOrb();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;

	void InitOrb(UPhysicalMaterial* PhysMat, float InRollingDecel);
	void SetOut();
	bool IsAtRest() const;
	FVector OrbLocation() const;

	/** Gold star orb: worth double and gives the scorer a power charge. */
	void MakeStar();
	UPROPERTY(BlueprintReadOnly) bool bStar = false;
	UPROPERTY(BlueprintReadOnly) bool bOut = false;
	UPROPERTY(BlueprintReadOnly) int32 LastHitBy = -1;
	float RollingDecel = 450.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	float Phase = 0.f;
};
