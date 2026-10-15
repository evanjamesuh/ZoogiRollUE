// Oct 14 (Evan, 9:33 PM): Crystal Collapse ice wall. A committed Crystal Collapse raises a wall of crystal ice shards across
// the hit / target area, perpendicular to the shot line. Solid (BlockAll box per segment), it stays for the rest of the match
// unless a Zoogi hits a segment faster than ShatterSpeed (normal component): that segment shatters (shard burst + sound) and its
// collision is removed. Cards-on only (spawned by UZoogiCardSubsystem). The CPU planner reads LiveSegments() as obstacles
// (AZoogiArena::IsPathBlocked / IsSpotClear) and as bank surfaces (AZoogiGameMode::PlanCPUShot).
// Config: [ZoogiRoll.IceWall] Length, Height, Thickness, Segments, ShatterSpeed, PassThroughKeep, bPerSegment, Mesh.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiIceWall.generated.h"

class UStaticMeshComponent;
class AZoogiBall;

struct ZOOGIROLL_API FZoogiIceWallTune
{
	float Length = 560.f;          // total wall length (cm)
	float Height = 150.f;          // shard height (cm)
	float Thickness = 46.f;        // collider thickness (cm)
	int32 Segments = 5;            // independently breakable segments
	float ShatterSpeed = 1500.f;   // Zoogi speed into the wall (cm/s, normal component) that shatters a segment
	float PassThroughKeep = 0.6f;  // fraction of the pre-hit velocity a Zoogi keeps after smashing through (0 = normal bounce)
	bool bPerSegment = true;       // false: one hard hit shatters the whole wall
	float Offset = 0.f;            // wall centre offset along the shot line from the hit point (cm)
	FString Mesh = TEXT("/Game/Env/Incoming/tomb/SM_Gem_FrostCrystal/SM_Gem_FrostCrystal/StaticMeshes/SM_Gem_FrostCrystal.SM_Gem_FrostCrystal");
	FLinearColor Glow = FLinearColor(0.45f, 0.85f, 1.f, 1.f);
	void Load();
};

UCLASS()
class ZOOGIROLL_API AZoogiIceWall : public AActor
{
	GENERATED_BODY()
public:
	AZoogiIceWall();
	static const FZoogiIceWallTune& Tune();
	/** Raises a wall centred at At (floor level taken from At.Z), perpendicular to ShotDir. Null if every segment was blocked. */
	static AZoogiIceWall* SpawnWall(UWorld* World, const FVector& At, const FVector& ShotDir, int32 OwnerIdx, int64 EffectId);
	struct FSeg2D { FVector2D A, B, N; float Half = 0.f; };
	/** Live (unshattered) segments of every ice wall in this world, as 2D capsules. */
	static void LiveSegments(const UWorld* World, TArray<FSeg2D>& Out);
	static int32 NumWalls(const UWorld* World);
	/** Match reset / rematch: removes every wall. */
	static void DestroyAll(UWorld* World, const TCHAR* Why);

	void OnBallHit(AZoogiBall* Ball, UPrimitiveComponent* Comp, const FVector& NormalImpulse, const FHitResult& Hit);
	int32 LiveCount() const;
	virtual void Tick(float Dt) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Colliders;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shards;
	TArray<int32> ShardSeg;
	TArray<FVector> ShardScale;
	TArray<bool> SegLive;
	TArray<FVector2D> SegA, SegB;
	FVector2D Axis = FVector2D(1.f, 0.f), Normal = FVector2D(0.f, 1.f);
	float Age = 0.f, BaseZ = 0.f;
	int32 OwnerP = -1;
	int64 Effect = 0;
	TArray<FVector> PrevVel; // per player index, last frame
	struct FDebris { FVector Vel = FVector::ZeroVector; float Spin = 0.f; float Life = 0.f; };
	TArray<FDebris> Debris;
	float LastHitLog = -10.f;

private:
	void Build(const FVector& Center, int32 OwnerIdx, int64 EffectId);
	void ShatterSegment(int32 Seg, const FVector& At, float Speed, AZoogiBall* By);
	static FVector2D D2Jit(FRandomStream& R, float A);
	static bool SegClear(UWorld* World, const FVector2D& A2, const FVector2D& B2);
	static int32 ClearCount(UWorld* World, const FVector2D& Center, const FVector2D& Ax);
};
