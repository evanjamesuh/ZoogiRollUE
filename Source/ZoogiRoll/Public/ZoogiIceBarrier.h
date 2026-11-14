// Nov 2 b63 (Evan: "The ice walls should not be visual only, they become solid and form cracks when hit but eventually burst into
// ice blocks physically blasting away, but act as walls until gone"): a solid Hovl ice wall (SM_Icewall, M_Ice3) that rises with
// the NS_Ice_wall eruption, blocks Zoogis and orbs like any wall, cracks on hits (3 crack stages) and finally bursts into ice
// blocks that fly off with physics. Used by the Frozen stage shift and the ICE WALL skill.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiIceBarrier.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS(NotPlaceable)
class ZOOGIROLL_API AZoogiIceBarrier : public AActor
{
	GENERATED_BODY()
public:
	AZoogiIceBarrier();
	/** A wall from A to B on the floor (Z of A), Height tall. Rounds = bursts on its own after that many new rounds (0 = only hits). */
	static AZoogiIceBarrier* Spawn(UWorld* W, const FVector& A, const FVector& B, float Height, int32 OwnerIdx, int32 Rounds = 2);
	static void DestroyAll(UWorld* W);
	virtual void Tick(float Dt) override;
	/** Damage in crack stages (1 = one stage); 3 stages burst it. */
	void Damage(float Stages, const FVector& At, const FVector& Dir);
	void Burst(const FVector& Dir);

	UFUNCTION() void OnHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY() TObjectPtr<UBoxComponent> Box;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ice;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Cracks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Blocks;
	float Len = 300.f, Hgt = 160.f, Dmg = 0.f, Rise = 0.f, LastHit = -10.f, BurstAge = -1.f;
	int32 Owner = -1, BornRound = 0, LifeRounds = 2, Stage = 0;
	TArray<FVector> BlockVel;
};
