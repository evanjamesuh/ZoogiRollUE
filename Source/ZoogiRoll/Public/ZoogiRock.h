#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiRock.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class AZoogiBall;

UENUM()
enum class EZoogiRockState : uint8 { Intact, Cracked, Shattered };

/**
 * Reusable destructible boulder (Oct 4). A solid round collider with a procedural (lumpy, faceted) boulder visual.
 * A Zoogi hitting it above CrackSpeed cracks it; a second hit above ShatterSpeed, or any strong hit (awakened shooter,
 * or crit-grade contact speed) shatters it: collider off, angular shards fly out on scripted ballistic arcs (no physics
 * bodies, so nothing can trap a Zoogi and the simulation stays deterministic), dust, a small camera kick, and low
 * rubble without collision. ResetRock() restores it (match start). Any arena can place these with InitRock().
 *
 * Realism pass (Oct 4 evening): the crack is a real split along a jagged fracture through the impact point (one half
 * tilts away a few cm and shows dark fracture faces), branching zigzag cracks with dark gaps and lighter chipped edges
 * conform to the surface, a flake pops off and leaves a dented fresh-stone scar; stone grit, pebbles and slow
 * non-emissive dust replace the old glow FX. All debris motion is scripted against the court plane (Z = 0).
 */
UCLASS()
class ZOOGIROLL_API AZoogiRock : public AActor
{
	GENERATED_BODY()

public:
	AZoogiRock();

	/** Radius / height of the footprint in cm, visual yaw, two rock colours, physics material, seed for the chip layout. */
	void InitRock(float InRadius, float InHeight, float InYaw, const FLinearColor& ColA, const FLinearColor& ColB, UPhysicalMaterial* PM, int32 InSeed);
	void ResetRock();
	/** Called from the ball's hit event. NormalImpulse in kg cm/s. */
	void OnBallHit(AZoogiBall* Ball, const FVector& NormalImpulse, const FVector& ImpactPoint);
	bool IsStanding() const { return State != EZoogiRockState::Shattered; }
	EZoogiRockState GetState() const { return State; }
	float GetRadius() const { return Radius; }

	UPROPERTY(EditAnywhere, Category = "Rock") float CrackSpeed = 900.f;   // normal contact speed, cm/s (full launch is 3600; a ~half-power straight hit cracks)
	UPROPERTY(EditAnywhere, Category = "Rock") float ShatterSpeed = 1600.f; // second hit on a cracked rock (~45% of full launch)
	UPROPERTY(EditAnywhere, Category = "Rock") int32 ChunkCount = 8;       // keep low (RX 6600 / 16 GB)
	UPROPERTY(EditAnywhere, Category = "Rock") float ChunkLife = 3.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	/** Boulder surface in rock space (actor space before Yaw): a lumpy, faceted ellipsoid, plus an optional impact dent. */
	struct FShape
	{
		FVector Semi = FVector(100.f);
		FVector O1 = FVector::ZeroVector, O2 = FVector::ZeroVector, O3 = FVector::ZeroVector;
		FVector FacetN[5];
		float FacetH[5] = { 2.f, 2.f, 2.f, 2.f, 2.f };
		float Lump = 1.f;
		FVector DentC = FVector::ZeroVector, DentN = FVector::UpVector;
		float DentR = 0.f, DentD = 0.f;
		void Init(const FVector& InSemi, FRandomStream& Rng, float InLump);
		float RadiusScale(const FVector& U) const;
		/** U: unit direction in normalised (unit-sphere) space. */
		FVector Point(const FVector& U) const;
		FVector Normal(const FVector& U) const;
		FVector ToU(const FVector& P) const { return (P / Semi).GetSafeNormal(); }
	};

	void Crack(const FVector& P, const FVector& Dir, AZoogiBall* By);
	void Shatter(const FVector& P, const FVector& Dir, AZoogiBall* By);
	UStaticMeshComponent* MakePart(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bShadow);
	UProceduralMeshComponent* MakeProc(bool bShadow);

	// Geometry (all cosmetic, no collision).
	FVector ImpactU(const FVector& WorldP, const FVector& Dir) const;
	void PrepareSplit(const FVector& U, FRandomStream& Rng);
	void BuildBody();
	void BuildCracks(const FVector& U, FRandomStream& Rng);
	void BuildSide(FRandomStream& Rng);
	void BuildRubble(FRandomStream& Rng);
	void BuildChunks(FRandomStream& Rng);
	int32 SideOf(const FVector& U) const;
	FVector SplitPos(const FVector& P, int32 InSide) const;
	FVector SplitNrm(const FVector& N, int32 InSide) const;
	FVector Surf(const FVector& RockP, float Offset, FVector* OutN = nullptr) const;
	FVector RockToActor(const FVector& V) const;

	// Scripted debris.
	void LaunchChunk(int32 Index, const FVector& Pos, const FVector& Vel, const FVector& AngVel, bool bMayStay);
	void SpawnGrit(int32 First, int32 Count, const FVector& Origin, float Spread, const FVector& MainDir, float Cone, float SpeedMin, float SpeedMax, FRandomStream& Rng);
	void SpawnPuff(const FVector& Pos, const FVector& Vel, float R0, float R1, float Life, float Peak, float Drag, float Settle, float Flat, const FLinearColor& Col);
	bool HasGround(const FVector& LocalPos) const;
	bool TickChunks(float Dt, float FloorZ);
	bool TickGrit(float Dt, float FloorZ);
	bool TickPuffs(float Dt, float FloorZ);
	void HideAllDebris();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Collider;
	/** Boulder: section 0 outer surface, section 1 fracture faces (only when split). */
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Body;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Side;
	/** Surface cracks (no shadow): section 0 dark gaps, 1 lighter chipped edges, 2 fresh scar. */
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CrackMesh;
	/** Broken stump + loose stones + stone powder (sections 0 outer, 1 fresh faces, 2 powder). No collision. */
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Rubble;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> ChunkPool;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> GritISM;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PuffMIDs;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatIntact;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatCracked;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatGap;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatEdge;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatScar;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatFracture;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatFreshDark;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> MatShard;

	struct FChunk
	{
		FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector, W = FVector::ZeroVector;
		FQuat Rot = FQuat::Identity;
		TArray<FVector> Corners; // mesh-local corners, for floor contact
		float Size = 10.f, Age = 0.f, RestAge = 0.f, Sink = 0.f;
		bool bLive = false, bRest = false, bMayStay = false, bSinking = false, bGroundChecked = false, bNoGround = false;
	};
	struct FGrit
	{
		FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector, W = FVector::ZeroVector, Half = FVector(1.f);
		FQuat Rot = FQuat::Identity;
		float Age = 0.f;
		bool bLive = false, bRest = false, bGroundChecked = false, bNoGround = false;
	};
	struct FPuff
	{
		FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector;
		float R0 = 10.f, R1 = 50.f, Age = 0.f, Life = 2.f, Peak = 0.4f, Drag = 2.f, Settle = 10.f, Flat = 1.f;
		bool bLive = false;
	};
	TArray<FChunk> Chunks;
	TArray<FGrit> GritParts; // [0, GritPerSet) dark set, [GritPerSet, 2 * GritPerSet) light set
	TArray<FPuff> PuffState;
	int32 NextPuff = 0;

	FShape Shape;
	// Fracture (crack stage): boundary meridians at SplitAz + Jag (half A) and SplitAz + PI + Jag (half B).
	TArray<float> JagA, JagB;
	float SplitAz = 0.f, SplitSep = 0.f;
	FVector SplitN = FVector::YAxisVector;
	FQuat SplitQ = FQuat::Identity;
	bool bSplit = false;
	bool bBodyBuiltIntact = false;
	bool bHasDust = false;
	FLinearColor DustCol = FLinearColor(0.3f, 0.28f, 0.24f);

	EZoogiRockState State = EZoogiRockState::Intact;
	float Radius = 100.f, Height = 80.f, Yaw = 0.f;
	float LastHitTime = -10.f;
	int32 Seed = 0;
};
