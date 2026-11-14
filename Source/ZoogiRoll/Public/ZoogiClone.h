#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiClone.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USkeletalMeshComponent;
class UAnimSequence;

/** Wolfgang's Wolf Pack: a ghostly wolf marble that homes in on an orb or rival for a few seconds, then vanishes. */
UCLASS()
class ZOOGIROLL_API AZoogiClone : public AActor
{
	GENERATED_BODY()

public:
	AZoogiClone();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;

	void InitClone(UStaticMesh* WolfMesh, AActor* InTarget, int32 InOwnerPlayer, float InSpeed, FLinearColor Tint = FLinearColor(0.3f, 0.55f, 1.f));
	virtual void Tick(float DeltaSeconds) override;

	int32 OwnerPlayer = -1;
	// Nov 1 Wolf Pack 2.0 (ZoogiWolfPack.h): set by UZoogiWolfPackSys right after InitClone
	int32 PackIdx = 0;
	bool bAlpha = false, bDire = false, bIce = false, bFire = false;
	float HitMul = 1.f;          // extra shove on a rival it hits (Alpha 1.5, Dire 2.5)
	float BlastMul = 1.f;        // detonation radius / push (Alpha, level, fire)
	float ChargeMul = 1.f;       // hold-to-charge (1 .. 1.8)
	float ExtraLife = 0.f;       // pack level 2 / ice
	int32 Rebounds = 0;          // pack level 4: re-target once after the first hit
	FVector FormDir = FVector::ForwardVector; float FormUntil = 0.f, FormSpeed = 1400.f; // V formation run
	void SetBodyScale(float S);
	/** b117 (Evan): fired as a missile - straight at the target, smoke trail, blows on impact. bOrb = a glowing purple orb, no wolf body. */
	void SetMissile(float InSpeed, bool bOrb);
	/** b119: a glowing purple orb instead of the micro-werewolf body (the pack's icon floats over it) */
	void MakeOrb();
	/** b119 (Evan: "the wolves summon on the field ... when chosen the closest wolf strikes"): parked on the field until Strike */
	bool bWaiting = false;
	void Hold();
	void Strike(AActor* InTarget);
	void StartRecall(class AZoogiBall* To);
	bool IsRecalling() const { return Recall.IsValid(); }
	void BlowNow() { Detonate(TEXT("charged")); }
	/** Parried: a puff of sparks, no blast. */
	void Pop();

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	TWeakObjectPtr<AActor> Target;
	float Speed = 1700.f;
	float Age = 0.f;
	bool bHit = false;
	// Oct 29 (Evan): "after clone orb momentum stops it detonates showing the shockwave and pushing orbs away"
	void Detonate(const TCHAR* Why);
	bool bDetonated = false;
	bool bImpactBlow = false; // b88: hit its target -> blows next tick
	bool bMissile = false; float PuffT = 0.f; // b117: straight-line missile run with a smoke trail, blows on impact
	bool bLatchPending = false, bLatched = false; float LatchT = 0.f; FVector LatchOff = FVector::ZeroVector; // b95: stays on its target until called
	float SlowT = 0.f;            // how long it has been nearly still
	FLinearColor Tint = FLinearColor(0.3f, 0.55f, 1.f);
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> MID;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Tail;   // tail streak
	TWeakObjectPtr<class AZoogiBall> Recall;
	TSet<TWeakObjectPtr<AActor>> Shoved;
	// Nov 1 (Evan: "Make the wolf's this I bought"): Bitgem Micro Werewolf body (walks upright, bites on a hit); -ZoogiOldWolfMesh = the marble
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BodyMID;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimWalk;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimBite;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimCharge;
	UPROPERTY() TObjectPtr<UAnimSequence> AnimAir;
	FVector BodyBase = FVector::ZeroVector; float BodyFit = 1.f; float BodyYaw = 0.f; float BiteUntil = -1.f; int32 BodyAnim = -1;
	bool SetupBody();
	void UpdateBody(float Dt, float Charge);
	void PlayBody(int32 Which, bool bForce = false);
	float PawT = 0.f; int32 PawSide = 1; float BodyScale = 1.f; float BumpT = -10.f;
};
