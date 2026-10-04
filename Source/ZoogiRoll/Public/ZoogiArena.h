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
	/** True past the knockout line (circle, or the Night Circuit rectangle). */
	bool IsOffCourt(const FVector2D& P) const;
	/** False if a ball of this radius would overlap a solid court piece (Night Circuit pads/posts). */
	bool IsSpotClear(const FVector2D& P, float Radius) const;
	/** Distance from P along Dir to the knockout line. */
	float DistanceToEdge(const FVector2D& P, const FVector2D& Dir) const;
	/** True if a ball of the given radius rolling straight From -> To would hit a solid pad or post (Night Circuit). */
	bool IsPathBlocked(const FVector2D& From, const FVector2D& To, float Radius) const;
	bool IsNeon() const { return Theme == EZoogiArena::Neon; }
	/** Destructible rocks placed by this arena (Meadow rim). Restored at match start. */
	void ResetRocks();
	const TArray<TObjectPtr<class AZoogiRock>>& GetRocks() const { return Rocks; }
	class AZoogiRock* NearestStandingRock(const FVector2D& From) const;

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
	void BuildArabian();
	void BuildTomb();
	void BuildNeon();
	/** Axis-aligned box (min/max XY in cm, height) with optional collision. */
	UStaticMeshComponent* AddBox(const FVector2D& Min, const FVector2D& Max, float Bottom, float Top, UMaterialInterface* Mat, bool bCollide);
	TArray<FBox2D> SolidBoxes;
	TArray<FVector2D> SolidPosts;
	void AddStars(int32 Count, float SizeMin, float SizeMax, const FLinearColor& Color, int32 Seed);
	class UPointLightComponent* AddPointLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius);
	/** Solid round bumper: invisible collider cylinder + visual mesh scaled to the footprint. */
	void AddBumper(const FVector2D& XY, float Radius, float Height, UStaticMesh* Mesh, UMaterialInterface* Mat, const FVector& VisualScale, float VisualZ, float Restitution);

	/** Floating island: top disc, rim band and a craggy underside. */
	void BuildIsland(const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Band, const FLinearColor& RockA, const FLinearColor& RockB, float FloorFriction);
	void BuildRim(const TArray<FVector4>& Marks, const FLinearColor& A, const FLinearColor& B, float Restitution, bool bDestructible = false);
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
	UPROPERTY() TArray<TObjectPtr<class AZoogiRock>> Rocks;
	/** Poppable pizza toppings (Arabian court). Destroyed in Clear(), restored with the rocks in ResetRocks(). */
	UPROPERTY() TArray<TObjectPtr<class AZoogiTopping>> Toppings;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> FloorPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> RockPM;
	EZoogiArena Theme = EZoogiArena::Meadow;
};
