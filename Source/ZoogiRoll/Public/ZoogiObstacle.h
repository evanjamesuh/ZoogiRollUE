// Oct 14 (arenas-import, Evan approved): obstacle sets from reskin-incoming (Meadow batch 6, Night Circuit rev 18.1).
// One actor per placed obstacle: the pack's visual meshes (no collision), the pack's own UCX hulls as one invisible
// convex collider (BlockAll, camera ignored, placeholder phys mat) and its trigger volumes, polled each tick against the
// Zoogis: Boost / Launch / Energy orb / floor Zone (slick or speed lane). All values are the packs' PLACEHOLDERS (untuned).
// Obstacles are NOT arena pieces: unbreakable, never in Crystal Collapse or destruction, built with the arena (so the
// main-menu map preview shows them too) and destroyed in AZoogiArena::Clear().
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiObstacle.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class AZoogiBall;

struct FZoogiObsTrigger
{
	int32 Kind = 0;   // 1 Boost, 2 Launch, 3 Energy, 4 Zone, 5 heading snap, 6 min speed, 7 two-way mult
	int32 Shape = 0;  // 0 box, 1 sphere, 2 cylinder, 3 arc around the actor origin
	FVector C = FVector::ZeroVector, E = FVector::ZeroVector;
	FVector2D D = FVector2D(1.f, 0.f);
	float P[4] = { 0.f, 0.f, 0.f, 0.f };
	TMap<TWeakObjectPtr<AZoogiBall>, float> Cool;
	TMap<TWeakObjectPtr<AZoogiBall>, float> Inside; // ball -> rolling decel it had on entering (zones)
	float RechargeAt = -1.f;
};

UCLASS()
class ZOOGIROLL_API AZoogiObstacle : public AActor
{
	GENERATED_BODY()
public:
	AZoogiObstacle();
	virtual void Tick(float DeltaSeconds) override;
	/** Dev test -ZoogiR2OrbTest (Frozen Ring 2 pocket orb): while true the game mode holds the turn flow in Aiming. */
	static inline bool bOrbTestHold = false;
	void TickOrbTest(class AZoogiGameMode* G, float Now);

	FName ObsName;
	float BounceGain = 0.f, MaxOut = 0.f;
	TArray<FZoogiObsTrigger> Triggers;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Vis;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Spent;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Col;
	UPROPERTY() TObjectPtr<USceneComponent> Root;

	/** Adds a visual mesh part (no collision). bSpent parts start hidden (energy orb spent state). */
	UStaticMeshComponent* AddVisual(class UStaticMesh* Mesh, float Scale, bool bSpent);
	/** Builds the invisible convex collider from hull point lists (mesh-local cm, scaled by Scale). */
	void BuildCollider(const TArray<TArray<FVector>>& Hulls, float Scale, class UPhysicalMaterial* PM);
	int32 NumHulls = 0;

	/** Ball sphere (world centre, radius) against a trigger shape. */
	bool Overlaps(const FZoogiObsTrigger& T, const FVector& BallW, float R) const;

protected:
	UFUNCTION() void OnColHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	TMap<TWeakObjectPtr<AZoogiBall>, float> GainCool;
	TArray<TWeakObjectPtr<AZoogiBall>> PendingGain;
	void SetOrbShown(bool bShown);
	void Fire(FZoogiObsTrigger& T, AZoogiBall* B, float Now);
	float EdgeR() const;
	float SafeLaunch(const FVector& P, FVector& H, float Up) const;
	static constexpr float MaxBoost = 2200.f; // tuning: no trigger pushes a Zoogi past this planar speed
};

// Oct 15 (Evan): relaunch guard for anything that launches / boosts a Zoogi or orb (obstacle launch / boost pads, fountains,
// Frozen boost portals). The same pad may not relaunch the same object within Cooldown seconds, nor more than MaxPerTurn times
// in one turn - an upward fountain whose ball falls back onto it relaunched one Zoogi 9x in a turn (12 s resolve timeout).
// Config [ZoogiRoll.Relaunch] Cooldown=1.5 MaxPerTurn=2 (MaxPerTurn 0 = no cap); -ZoogiRelaunchCool= / -ZoogiRelaunchMax= override.
namespace ZoogiRelaunch
{
	ZOOGIROLL_API bool Allow(const UObject* Pad, const AActor* Who, int32 Turn, double Now, const TCHAR* PadName);
	ZOOGIROLL_API int32 Blocked();
}
