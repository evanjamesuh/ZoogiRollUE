#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTopping.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPrimitiveComponent;

UENUM()
enum class EZoogiToppingKind : uint8 { Pepperoni, Cheese };

/**
 * Poppable pizza topping (Arabian "pizza" court, Oct 4). Purely cosmetic: a flat pepperoni slice or a melted cheese
 * blob lying on the floor, with a query-only overlap trigger that responds to the Pawn channel only (the Zoogi marbles).
 * It never blocks, so shots, knockback, scoring, turns and the aim / CPU sweeps (ECC_WorldStatic) are unaffected.
 * When a Zoogi rolls over it, it pops: a quick squash-and-stretch, then it vanishes while a few non-colliding bits
 * fly out on scripted arcs and shrink away (~0.6 s). Popped toppings stay gone for the match; ResetTopping() brings
 * them back (AZoogiArena::ResetRocks, called at match start). Separate from AZoogiRock on purpose.
 */
UCLASS()
class ZOOGIROLL_API AZoogiTopping : public AActor
{
	GENERATED_BODY()

public:
	AZoogiTopping();

	/** Kind, footprint radius in cm, yaw, seed for the speckle / debris layout. */
	void InitTopping(EZoogiToppingKind InKind, float InRadius, float InYaw, int32 InSeed);
	void ResetTopping();
	bool IsPopped() const { return bPopped; }
	EZoogiToppingKind GetKind() const { return Kind; }
	float GetRadius() const { return Radius; }

	UPROPERTY(EditAnywhere, Category = "Topping") int32 BitCount = 6;     // keep low (RX 6600 / 16 GB)
	UPROPERTY(EditAnywhere, Category = "Topping") float BitLife = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Topping") float PopTime = 0.18f;  // squash + vanish

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	void Pop(const FVector& From, const FVector& Vel);
	UStaticMeshComponent* MakePart(UStaticMesh* Mesh, UMaterialInterface* Mat, USceneComponent* Parent, bool bShadow);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	/** Parent of every visible piece; scaled for the pop squash. */
	UPROPERTY() TObjectPtr<USceneComponent> Visual;
	UPROPERTY() TObjectPtr<USphereComponent> Trigger;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BitPool;

	struct FBit { FVector Pos, Vel, Scale; FRotator Rot, Spin; float Age = 0.f; bool bLive = false; };
	TArray<FBit> Bits;
	EZoogiToppingKind Kind = EZoogiToppingKind::Pepperoni;
	float Radius = 40.f, Yaw = 0.f;
	int32 Seed = 0;
	int32 PopCount = 0;
	float PopAge = 0.f;
	bool bPopped = false;
	bool bPopping = false;
};
