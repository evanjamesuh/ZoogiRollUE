#pragma once



#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
class UNiagaraComponent;
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

	/** Oct 22 QQ #28: soft blob shadow under the orb when grounded (visual only). */

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LandShadow;



	void InitOrb(UPhysicalMaterial* PhysMat, float InRollingDecel);

	void SetOut();

	/** 2v2 Combo: neutral Arena Orbs are never destroyed; after ORB OUT they come back here. */

	void Respawn(const FVector& Location);

	bool IsAtRest() const;

	FVector OrbLocation() const;



	/** Gold star orb: worth double and gives the scorer a power charge. */

	void MakeStar();
	/** Oct 29 (build 20): a coloured power orb (scores like a star orb: double points + a power charge). Call after the orb style. */
	void MakePower(const FLinearColor& Color);
	void ApplyPowerLook();
	UPROPERTY(BlueprintReadOnly) bool bPower = false;
	FLinearColor PowerColor = FLinearColor::White;

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
	float PopStrength = 0.f, LastPop = -10.f; // b57: playful scatter (applied the tick after the hit)

	/** Item 12 floor glow: arena tint serial applied, glow scale, smoothed floor-contact factor. */

	void ApplyFloorGlowTint();

	int32 GlowSerial = -1;

	float GlowScale = 1.f;

	float Contact = 1.f;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShadowMID;
	float ShadowContact = 0.f;
	/** Free Arrow Trail blue ribbon (NS_ArrowTrail_Water) while orb is active. */
	UPROPERTY() TWeakObjectPtr<UNiagaraComponent> BlueRibbon;
	void UpdateBlueRibbon();
};

