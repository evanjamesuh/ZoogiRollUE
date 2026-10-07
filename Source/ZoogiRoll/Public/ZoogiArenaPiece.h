#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArenaPiece.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class AZoogiBall;
struct FZoogiMeshBuilder;

/** Destruction states shared by every arena piece (Frozen Ring, Oct 6): INTACT -> DAMAGED -> CRACKED -> BROKEN. */
UENUM(BlueprintType)
enum class EZoogiPieceState : uint8
{
	Intact,
	Damaged,  // surface cracks (cyan hairlines), slight frost / melt tint
	Cracked,  // deep glowing cracks, parts shifted, chips fall off
	Broken    // shattered: colliders off, scripted debris (no physics bodies), low rubble without collision
};

/**
 * Reusable destructible stage piece (Frozen Ring, Oct 6). One actor = one breakable unit (a wall segment, an arch, a
 * stand, a rim ice-block cluster, the launch ramp, the raised platform, an obelisk ...).
 *  - Visual parts (static or procedural meshes) + optional collider parts (tagged ZWall / ZRamp / ZPlatform / ZBumper so
 *    the 2v2 combo rules keep reading them).
 *  - HP drives the state. Damage comes from Zoogi impacts (OnBallHit), heavy landings, ring-outs and powers
 *    (ApplyPower / DamageArea). The per-surface power hook: SurfaceTag (Ice, Snow, Stone, Wood, Crystal) x element
 *    (Ember melts and cracks ice, Frost refreezes ice one state, Storm shocks, Titan crushes, Void erodes). New
 *    powers only need a row in PowerMultiplier().
 *  - Fairness: damaged / cracked states keep the collider shape (no sudden lips), broken turns colliders off, Frost
 *    never restores a collider while a Zoogi or orb overlaps it, debris has no collision. Indestructible pieces
 *    (the 4 Resonance Hub pillars) ignore all damage.
 *  - Visual damage uses the "Damage" / "Melt" / "Frost" parameters of /Game/Env/Arenas/M_FrozenPiece when present,
 *    with a tint fallback, plus part offsets (cracked), chips and shards (broken). ResetPiece() restores it.
 */
UCLASS()
class ZOOGIROLL_API AZoogiArenaPiece : public AActor
{
	GENERATED_BODY()

public:
	AZoogiArenaPiece();

	/** Kind is a free label for logs / shots ("wall", "arch", "stand" ...). */
	void Setup(FName InKind, FName InSurface, float InMaxHP, bool bInIndestructible = false);

	// ---- building (local space = actor space; place the actor first) ----
	UStaticMeshComponent* AddShape(const TCHAR* Shape, const FTransform& Local, UMaterialInterface* Mat, bool bShadow = true);
	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bShadow = true);
	UProceduralMeshComponent* AddProc(const FZoogiMeshBuilder& M, UMaterialInterface* Mat, bool bShadow = true);
	/** Glowing parts (runes, slits, orbs): they dim as the piece breaks and are not tinted. */
	void MarkGlow(UPrimitiveComponent* C) { GlowParts.AddUnique(C); }
	/** Invisible collider box / cylinder (BlockAll), optional component tag and physics material. */
	UStaticMeshComponent* AddColliderBox(const FVector& Center, const FVector& Size, const FRotator& Rot, FName Tag, UPhysicalMaterial* PM);
	UStaticMeshComponent* AddColliderCyl(const FVector2D& XY, float Radius, float Bottom, float Top, FName Tag, UPhysicalMaterial* PM);
	/** Footprint the CPU uses to avoid shooting through this piece (only while its collider is live). */
	void SetPathBlock(const FVector2D& WorldXY, float Radius) { PathXY = WorldXY; PathR = Radius; }
	/** Call once after building: records part transforms / materials for damage and reset. */
	void FinishBuild();

	// ---- gameplay ----
	void OnBallHit(AZoogiBall* Ball, const FVector& NormalImpulse, const FVector& ImpactPoint);
	/** Damage in HP (100 = a fresh piece). Element applies the surface reaction. Returns true if the state changed. */
	bool ApplyDamage(float Amount, const FVector& At, const FVector& Dir, EZoogiArtifact Element, AZoogiBall* By = nullptr);
	/** Power hook: a power / artifact effect reaches this piece. */
	bool ApplyPower(EZoogiArtifact Element, float Amount, const FVector& At, AZoogiBall* By = nullptr);
	/** Forces a state (shots, tests). bFx: debris / dust / sound. */
	void SetPieceState(EZoogiPieceState S, const FVector& At, const FVector& Dir, bool bFx);
	void ResetPiece();
	EZoogiPieceState GetPieceState() const { return State; }
	bool IsIndestructible() const { return bIndestructible; }
	bool IsBroken() const { return State == EZoogiPieceState::Broken; }
	bool HasColliders() const { return Colliders.Num() > 0; }
	bool CollisionLive() const { return bCollisionLive; }
	/** Forming / state control of the colliders (never turns on while broken). */
	void SetCollisionLive(bool bOn);
	bool GetPathBlock(FVector2D& OutXY, float& OutR) const;
	FName GetKind() const { return Kind; }
	FName GetSurface() const { return Surface; }
	float GetHP() const { return HP; }

	/** Element x surface multiplier (the power-reactive table). Frost on ice returns a negative value = refreeze. */
	static float PowerMultiplier(FName InSurface, EZoogiArtifact Element);
	/** Area damage to every piece whose bounds come within Radius of P. */
	static int32 DamageArea(UWorld* World, const FVector& P, float Radius, float Amount, EZoogiArtifact Element, AZoogiBall* By = nullptr, const AZoogiArenaPiece* Except = nullptr);
	/** Ring-out: the nearest outer piece (r > MinR) within ConeDeg of the exit direction takes the crash. */
	static AZoogiArenaPiece* RingOutCrash(UWorld* World, const FVector& ExitPoint, const FVector& Dir, float Amount, AZoogiBall* By = nullptr);
	static FString StateName(EZoogiPieceState S);
	static FLinearColor ElementColor(EZoogiArtifact E);

	/** Rubble kept on the ground when broken: parts whose bottom is below this height stay (flattened). */
	float RubbleKeepZ = 35.f;
	/** Crack glow colour (Frozen: cyan #19C3E6). */
	FLinearColor CrackColor = FLinearColor(0.10f, 0.76f, 0.90f);
	/** Damage per cm/s of normal contact speed above HitSpeedMin. */
	// Round 3c (Evan 10:57 PM): half the contact speed per damage stage (was 500 cm/s and 1/42 per cm/s).
	float HitSpeedMin = 250.f;
	float HitDamagePerSpeed = 1.f / 21.f;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

protected:
	struct FPart
	{
		TWeakObjectPtr<UPrimitiveComponent> C;
		FTransform Base;         // relative transform
		FVector Shift = FVector::ZeroVector;
		FRotator Tilt = FRotator::ZeroRotator;
		float BottomZ = 0.f;     // relative bottom (for rubble)
		bool bGlow = false;
	};
	struct FShard
	{
		TWeakObjectPtr<UStaticMeshComponent> C;
		FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector, W = FVector::ZeroVector, Scale = FVector(0.1f);
		FRotator Rot = FRotator::ZeroRotator;
		float Age = 0.f, Life = 2.5f, FloorZ = 0.f;
		bool bRest = false;
	};
	void ApplyVisualState();
	void SpawnShards(int32 Count, const FVector& At, const FVector& Dir, float Speed, float SizeMin, float SizeMax);
	bool AnythingOverlaps() const;
	UMaterialInterface* PickShardMat() const;

	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> Colliders;
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> GlowParts;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> ShardMats;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ShardPool;
	TArray<FPart> Parts;
	TArray<FShard> Shards;
	TArray<FLinearColor> MIDBaseTint;
	TArray<bool> MIDHasDamageParam;
	TMap<UMaterialInstanceDynamic*, float> GlowBase;

	FName Kind = TEXT("piece");
	FName Surface = TEXT("Ice");
	EZoogiPieceState State = EZoogiPieceState::Intact;
	float MaxHP = 100.f, HP = 100.f;
	float Melt = 0.f, Frost = 0.f, Flash = 0.f;
	float LastHit = -10.f, Clock = 0.f;
	bool bIndestructible = false;
	bool bCollisionLive = true;
	bool bBuilt = false;
	FVector2D PathXY = FVector2D::ZeroVector;
	float PathR = 0.f;
	int32 Seed = 1;
};
