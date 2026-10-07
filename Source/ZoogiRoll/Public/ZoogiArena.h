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

	// ---- Frozen Ring (Oct 6): destructible stage pieces, reactive floor scars, stage-forming intro ----
	/** Reusable stage-forming component (any arena can register pieces; Frozen does). */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UZoogiStageForming> Forming;
	/** Resets every piece / scar and plays the forming intro (match start). */
	void BeginForming(float TimeScale = 1.f);
	bool IsForming() const;
	void ResetPieces();
	const TArray<TObjectPtr<class AZoogiArenaPiece>>& GetPieces() const { return Pieces; }
	class AZoogiFloorScars* GetScars() const { return Scars; }
	class AZoogiArenaPiece* SpawnPiece(const FVector& Loc, float Yaw, FName Kind, FName Surface, float HP, bool bIndestructible = false);
	/** Frozen materials by key (Ice, IceDeep, IcePane, Snow, SnowShade, Stone, StoneDark, Wood, WoodDark, Cliff, Rune). */
	UMaterialInterface* FrozenMat(FName Key) const;
	class UMaterialInstanceDynamic* GetFloorMID() const { return FloorMID; }

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
	UMaterialInterface* MakeFrozenMat(FName Key, const FLinearColor& Color, const TCHAR* Detail, float DetailScale, float DetailStrength,
		float Roughness, float Specular, const FLinearColor& Emissive, float EmissiveStrength);
	USceneComponent* NewPivot(const FVector& Loc);
	class UProceduralMeshComponent* AddFrozenProc(const struct FZoogiMeshBuilder& M, UMaterialInterface* Mat, USceneComponent* Parent, bool bShadow);
	UPROPERTY() TArray<TObjectPtr<class AZoogiArenaPiece>> Pieces;
	UPROPERTY() TObjectPtr<class AZoogiFloorScars> Scars;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> FloorMID;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> FrozenMats;
	float FloorGlowTarget = 1.6f;
	float SpawnRadiusOverride = 0.f;
	void BuildVolcanic();
	void BuildCosmic();
	/** Cosmic: animated space-journey backdrop (nebula sky sphere, GPU star streaks, drifting planets). False if assets are missing. */
	bool AddSpaceBackdrop();
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

	/** Floating island: top disc, rim band and a craggy underside. TopOverride replaces the noise floor material (visual only). */
	void BuildIsland(const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Band, const FLinearColor& RockA, const FLinearColor& RockB, float FloorFriction, UMaterialInterface* TopOverride = nullptr);
	void BuildRim(const TArray<FVector4>& Marks, const FLinearColor& A, const FLinearColor& B, float Restitution, bool bDestructible = false,
		float CarvedLump = 0.f, const FLinearColor& RuneColor = FLinearColor::Black, float RuneStrength = 0.f);
	void BuildPads();
	void BuildIslet(const FVector& Center, float Radius, const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Rock, UStaticMesh* Tree, int32 TreeCount, float TreeHeight, FRandomStream& Rng);
	void ScatterInstances(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten = false);

	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide, bool bShadow = true);
	/** Basic Cylinder resized: radius, height, top Z. */
	UStaticMeshComponent* AddCylinder(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat, bool bCollide);
	/** Basic Cone flipped tip-down under a disc. */
	UStaticMeshComponent* AddUnderCone(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat);
	UStaticMeshComponent* AddTree(UStaticMesh* Tree, const FVector& Base, float Height, float Yaw);

	// ---- Buzzy arena kit (ZoogiArenaKit.cpp, Oct 6 re-skin). All visual: no colliders, no gameplay change. ----
	/** MID of /Game/Env/Arenas/M_ZoogiArenaFloor with T_<Key>Floor / T_<Key>Glow, or nullptr if the assets are missing. */
	UMaterialInterface* KitFloorMaterial(const TCHAR* Key, const FLinearColor& GlowColor, float GlowStrength, float BaseBright = 1.f);
	/** Painted panorama sky sphere (M_ArenaSky + T_<Key>Sky). False if missing. */
	bool AddPanoramaSky(const TCHAR* Key, float Bright, float NadirDim, const FLinearColor& NadirColor);
	class UProceduralMeshComponent* NewProcMesh(bool bShadow, bool bCameraBlock = false);
	UStaticMeshComponent* AddBoxR(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bShadow = true);
	/** Terrace ring outside the knockout line: flat at Z out to FlatTo (K14: stays below -100 near the edge), rising to RimZ at ROut. */
	void AddTerrace(float RIn, float ROut, float Z, float FlatTo, float RimZ, UMaterialInterface* Top, UMaterialInterface* Wall, UMaterialInterface* Under);
	float TerraceHeight(float R, float AngleRad) const;
	void ScatterTerrace(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten);
	void AddCurb(float ROut, float TopZ, float Depth, UMaterialInterface* Mat);
	void AddStands(float CenterDeg, float ArcDeg, float RIn, float BaseZ, int32 Tiers, float TierDepth, float TierRise, UMaterialInterface* Seat, UMaterialInterface* SeatAlt, UMaterialInterface* Riser, UMaterialInterface* Stone);
	void AddPagoda(const FVector& Base, float Yaw, int32 Storeys, float Size, UMaterialInterface* Body, UMaterialInterface* Panel, UMaterialInterface* Roof, UMaterialInterface* Trim, UMaterialInterface* Stone, UMaterialInterface* Gold);
	void AddShrineGate(const FVector& Base, float Yaw, float Scale, UMaterialInterface* PostMat, UMaterialInterface* Beam, UMaterialInterface* Emblem, UMaterialInterface* Portal, UMaterialInterface* Stone);
	void AddResonancePillar(const FVector& Base, float Yaw, float Height, UMaterialInterface* Stone, UMaterialInterface* Slit, UMaterialInterface* Cup, UMaterialInterface* Orb, UMaterialInterface* Vine);
	/** Floating spinning crystal (no collider, no shadow, tagged NoCamBlock). */
	void AddHubCore(const FVector& Center, float HalfHeight, UMaterialInterface* Crystal);
	void AddGem(USceneComponent* Parent, const FVector& Center, float HalfHeight, float Radius, int32 Sides, UMaterialInterface* Mat, bool bShadow);
	void AddWaterfall(const FVector& Top, float Yaw, float Width, float Height, float Lip, UMaterialInterface* MatOverride = nullptr);
	UMaterialInterface* KitLavaMaterial(float TileU, float TileV, float SpeedU, float SpeedV, float Bright, float CrustAmount);
	void AddObsidianOutcrop(const FVector2D& P, float Rad, float H, float Yaw, int32 Seed, UMaterialInterface* Rock, UMaterialInterface* Crack);
	void AddFortressWall(float CenterDeg, float ArcDeg, float RIn, float Thick, float Height, float GateHalfWidth, UMaterialInterface* Stone, UMaterialInterface* Trim, UMaterialInterface* Slit);
	void AddFortressGate(const FVector& Base, float Yaw, float Height, UMaterialInterface* Stone, UMaterialInterface* Trim, UMaterialInterface* Emblem, UMaterialInterface* Portal);
	void AddLavaChannel(float Deg, float R0, float R1, float Width, UMaterialInterface* Mat);
	void AddBridge(const FVector& A, const FVector& B, float Width, float Sag, UMaterialInterface* Plank, UMaterialInterface* Rope);
	void AddMagmaCore(const FVector& Center, float Radius, UMaterialInterface* Mat, const FLinearColor& LightColor, float Candela, float LightRadius);
	void AddFillLight(const FVector& Pos, const FLinearColor& Color, float Candela, float Radius);
	float TerraceZ = -160.f, TerraceFlatTo = 2100.f, TerraceRimZ = -40.f, TerraceRIn = 1680.f, TerraceROut = 2400.f;

	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Built;
	UPROPERTY() TArray<TObjectPtr<class AZoogiRock>> Rocks;
	/** Poppable pizza toppings (Arabian court). Destroyed in Clear(), restored with the rocks in ResetRocks(). */
	UPROPERTY() TArray<TObjectPtr<class AZoogiTopping>> Toppings;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> FloorPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> RockPM;
	EZoogiArena Theme = EZoogiArena::Meadow;
};
