// Oct 22 (Evan): redesigned Crystal Collapse. When the card's Zoogi rolls it leaves a trail of solid ice shards behind it
// for TrailSeconds (one shard every Spacing cm of travel). Each shard is a BlockAll collider (tactical barrier). After
// BurstDelay (+ BurstStagger per shard, in spawn order) a shard bursts: Effect22 ice burst + frost sting / shatter sound,
// a radial push on nearby Zoogis, orbs and loose simulated physics objects, and (Crystal Collapse being the one card-scoped
// destruction exemption) one break state on eligible breakable pieces in range (pillars / indestructible pieces never).
// The burst leaves NO crater / cracks (Evan, Oct 22). Its fragments fly across the field, each lighting the floor under it
// (capped point lights + short floor glows, phone budget).
// Config: [ZoogiRoll.IceTrail] (see FZoogiIceTrailTune). -ZoogiCrystalTest runs a self-check (spawn / block / burst / push).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiIceTrail.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class AZoogiBall;

struct ZOOGIROLL_API FZoogiIceTrailTune
{
	bool bOn = true;               // false: old Oct 14 ice wall at the first contact
	float TrailSeconds = 3.0f;     // s the trail is laid once the Zoogi is rolling
	float ArmTimeout = 20.f;       // s to wait for the roll after commit
	float MinSpeed = 150.f;        // cm/s: no shards while (nearly) still
	float Spacing = 110.f;         // cm of travel between shards
	int32 MaxShards = 12;
	float BackOffset = 30.f;       // gap behind the Zoogi's surface (cm)
	float ShardRadius = 28.f;      // collider half width (cm)
	float ShardHeight = 110.f;     // cm
	float BurstDelay = 3.0f;       // s after spawning
	float BurstStagger = 0.12f;    // extra s per shard index (spawn order)
	float BurstRadius = 260.f;     // push / break radius (cm)
	float ZoogiPush = 900.f;       // cm/s added to Zoogis at the centre (falls to 40% at the rim)
	float ZoogiUp = 150.f;         // cm/s up
	float PhysicsImpulse = 60000.f;// radial impulse for orbs / loose simulated objects (velocity change off)
	bool bBreakPieces = true;      // card-scoped destruction exemption (pillars / indestructible never)
	int32 Fragments = 5;           // flying fragments per burst
	float FragSpeed = 900.f;       // cm/s
	float FragLife = 0.9f;         // s
	int32 MaxFragLights = 4;       // live fragment point lights (whole world)
	float FragLightIntensity = 2500.f;
	float FloorGlowEvery = 0.07f;  // s between floor glow splats under a fragment
	FString Mesh = TEXT("/Game/Env/Incoming/tomb/SM_Gem_FrostCrystal/SM_Gem_FrostCrystal/StaticMeshes/SM_Gem_FrostCrystal.SM_Gem_FrostCrystal");
	FString BurstKFX = TEXT("Effect22");
	FLinearColor Glow = FLinearColor(0.45f, 0.85f, 1.f, 1.f);
	void Load();
};

UCLASS(Transient, NotPlaceable)
class ZOOGIROLL_API AZoogiIceTrail : public AActor
{
	GENERATED_BODY()
public:
	AZoogiIceTrail();
	static const FZoogiIceTrailTune& Tune();
	/** Arms a trail for Ball (Crystal Collapse commit). Shards start when the Zoogi rolls. */
	static AZoogiIceTrail* Start(UWorld* World, AZoogiBall* Ball, int64 EffectId, float Amp = 1.f);
	static void DestroyAll(UWorld* World, const TCHAR* Why);
	static void EnsureTest(UWorld* World); // -ZoogiCrystalTest driver (called from AZoogiBall::Tick)
	/** Places one shard at Floor (z = floor). Returns its index or INDEX_NONE when the spot is blocked. */
	int32 AddShard(const FVector& Floor, float Delay);
	virtual void Tick(float Dt) override;

	struct FShard { TObjectPtr<UStaticMeshComponent> Col; TObjectPtr<UStaticMeshComponent> Vis; FVector Floor; FVector Scale; float Age = 0.f, BurstAt = 3.f; bool bBurst = false; };
	struct FFrag { TObjectPtr<UStaticMeshComponent> Mesh; TObjectPtr<UPointLightComponent> Light; FVector Vel; float Life = 0.f, Spin = 0.f, NextGlow = 0.f; FVector Scale; };
	TArray<FShard> Shards;
	TArray<FFrag> Frags;
	TWeakObjectPtr<AZoogiBall> Ball;
	int64 Effect = 0;
	int32 OwnerP = -1, Bursts = 0, PushedZoogis = 0, PushedPhysics = 0, BrokenPieces = 0;
	float Amp = 1.f, Age = 0.f, RollT = -1.f, Travel = 0.f;
	FVector LastPos = FVector::ZeroVector;
	bool bDoneLaying = false;

	UPROPERTY() TArray<TObjectPtr<UObject>> Keep;
	UPROPERTY() TObjectPtr<USceneComponent> Root;

private:
	void Burst(int32 i);
	bool SpotClear(const FVector& Floor) const;
	void TickFrags(float Dt);
};

UCLASS(Transient, NotPlaceable)
class ZOOGIROLL_API AZoogiCrystalTest : public AActor
{
	GENERATED_BODY()
public:
	AZoogiCrystalTest();
	virtual void Tick(float Dt) override;
	float T = 0.f; int32 Step = 0; float StepT = 0.f;
	TWeakObjectPtr<AZoogiIceTrail> Trail, PushTrail;
	FVector PushDir = FVector::ZeroVector; float VelBefore = 0.f; int32 ShardsLaid = 0;
	bool bSpawn = false, bBlock = false, bBurst = false, bPush = false;
	void Finish(const TCHAR* Why);
};
