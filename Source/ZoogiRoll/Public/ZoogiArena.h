#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;
class UPhysicalMaterial;
class UMaterialInterface;
class UStaticMesh;

/**
 * Builds an arena and its sky at runtime: the floating playfield island
 * (floor top at Z = 0, open edge at ZoogiConst::KnockoffRadius), the rim
 * obstacles, dressing, and the light rig.
 */
UCLASS()
class ZOOGIROLL_API AZoogiArena : public AActor
{
	GENERATED_BODY()

public:
	AZoogiArena();

	void Build(EZoogiArena InTheme);
	EZoogiArena GetTheme() const { return Theme; }
	/** Rolling deceleration for marbles on this floor (ice is slippery). */
	float GetRollingDecel() const;
	FVector GetSpawnPoint(int32 PlayerIndex) const;
	float GetSpawnYaw(int32 PlayerIndex) const;
	float GetKnockoffRadius() const { return ZoogiConst::KnockoffRadius; }

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SkySphere;

protected:
	void Clear();
	void SetupSky(EZoogiArena InTheme);
	void BuildMeadow();
	void BuildFrozen();
	void BuildVolcanic();
	void BuildCosmic();

	/** Floating island: top disc, rim band and a craggy underside. */
	void BuildIsland(const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Band, const FLinearColor& RockA, const FLinearColor& RockB, float FloorFriction);
	void BuildRim(const TArray<FVector4>& Marks, const FLinearColor& A, const FLinearColor& B, float Restitution);
	void BuildPads();
	void BuildIslet(const FVector& Center, float Radius, const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Rock, UStaticMesh* Tree, int32 TreeCount, float TreeHeight, FRandomStream& Rng);
	void ScatterInstances(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten = false);

	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide, bool bShadow = true);
	/** Basic Cylinder resized: radius, height, top Z. */
	UStaticMeshComponent* AddCylinder(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat, bool bCollide);
	/** Basic Cone flipped tip-down under a disc. */
	UStaticMeshComponent* AddUnderCone(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat);
	UStaticMeshComponent* AddTree(UStaticMesh* Tree, const FVector& Base, float Height, float Yaw);

	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Built;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> FloorPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> RockPM;
	EZoogiArena Theme = EZoogiArena::Meadow;
};
