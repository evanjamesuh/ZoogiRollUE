#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiRock.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UPhysicalMaterial;
class AZoogiBall;

UENUM()
enum class EZoogiRockState : uint8 { Intact, Cracked, Shattered };

/**
 * Reusable destructible boulder (Oct 4). A solid round collider with a squashed-sphere visual.
 * A Zoogi hitting it above CrackSpeed cracks it (darker, chipped, crack lines); a second hit above ShatterSpeed,
 * or any strong hit (awakened shooter, or crit-grade contact speed) shatters it: collider off, 8 chunks fly out on
 * scripted ballistic arcs (no physics bodies, so nothing can trap a Zoogi and the simulation stays deterministic),
 * dust + debris FX, a small camera kick, and low rubble without collision. ResetRock() restores it (match start).
 * Any arena can place these with InitRock(); the Meadow rim uses them.
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
	void Crack(const FVector& P, const FVector& Dir, AZoogiBall* By);
	void Shatter(const FVector& P, const FVector& Dir, AZoogiBall* By);
	void Launch(int32 N, const FVector& P, const FVector& Dir, float Speed, float SizeMul);
	UStaticMeshComponent* MakePart(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bShadow);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Collider;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Side;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CrackLines;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rubble;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ChunkPool;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatIntact;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatCracked;

	struct FChunk { int32 Comp = 0; FVector Pos, Vel, Scale; FRotator Rot, Spin; float Age = 0.f, Half = 10.f; bool bLive = false; };
	TArray<FChunk> Chunks;
	int32 NextChunk = 0;
	EZoogiRockState State = EZoogiRockState::Intact;
	float Radius = 100.f, Height = 80.f, Yaw = 0.f;
	float LastHitTime = -10.f;
	int32 Seed = 0;
	FVector BodyScale = FVector::OneVector;
};
