#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPhysicalMaterial;

/** A Zoogi marble: a Chaos-simulated sphere with the character model riding inside it. */
UCLASS()
class ZOOGIROLL_API AZoogiBall : public APawn
{
	GENERATED_BODY()

public:
	AZoogiBall();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
	/** Flat glowing ring under the marble in the player's colour (does not roll). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> TurnLight;

	void InitZoogi(const FZoogiDef& InDef, int32 InPlayerIndex, bool bInCPU, UPhysicalMaterial* PhysMat, float InRollingDecel);
	void Launch(const FVector2D& PlanarVelocity);
	void PlaceAt(const FVector& Location, float FacingYaw);
	void SetOut();
	void SetActiveTurn(bool bActive);

	bool IsAtRest() const;
	float PlanarSpeed() const;
	FVector GetBallVelocity() const;
	FVector BallLocation() const;

	UPROPERTY(BlueprintReadOnly) FZoogiDef Def;
	UPROPERTY(BlueprintReadOnly) int32 PlayerIndex = 0;
	UPROPERTY(BlueprintReadOnly) bool bCPU = false;
	UPROPERTY(BlueprintReadOnly) int32 Score = 0;
	UPROPERTY(BlueprintReadOnly) bool bOut = false;
	UPROPERTY(BlueprintReadOnly) int32 LastHitPlayer = -1;
	UPROPERTY(BlueprintReadOnly) FVector SpawnLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float SpawnYaw = 0.f;
	/** Heading the camera and aim use at the start of this player's turn. */
	UPROPERTY(BlueprintReadOnly) float AimYaw = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 KnockOuts = 0;
	UPROPERTY(BlueprintReadOnly) int32 OrbsScored = 0;
	float RollingDecel = ZoogiConst::RollingDecel;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	void FitVisual();

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool bActiveTurn = false;
	float RingPulse = 0.f;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> RingMID;
};
