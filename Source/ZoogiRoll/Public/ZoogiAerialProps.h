// Battle Episode Engine (T3, refs frozen_zones_aerial / sky_bumper_detail / resonance_halo_detail / rail_release_timing):
// gameplay props for the Frozen Ring aerial layer. Collision shapes + simple placeholder visuals in the Frozen materials
// (Oct 8; visuals never collide). Indestructible fixtures (not arena pieces). Spawned by the Frozen map layout in normal
// matches ([ZoogiRoll.Aerial] bAerialProps + AerialArenas=Frozen), -ZoogiBattleProps (all arenas), or by a battle test.
//   Gate          : Zone 3 recovery gate on a bridge (trigger only): crossing it tags "recovery" + a small energy grant.
//
//   SkyBumper     : spring node; a Zoogi that hits it is redirected along the reflection of its incoming velocity about
//                   the contact normal (computed from the two centres) at ReboundMul x speed (default 2.0, capped).
//   ResonanceHalo : a standing / hanging stone ring. Passing through the opening ("pass") and striking the rim ("rim",
//                   a normal physics rebound) are reported separately. Halo Bank uses them.
//   LaunchRail    : entry of a launch rail; a Zoogi entering it starts the Rail Release timing mini-game.
// Hits come from the prop's own hit notify and from the Zoogi's (the battle state machine forwards); a per-Zoogi
// cooldown makes it one event. Trigger volumes have no collision and are polled by the state machine.
// RULE ZERO: these are gameplay mechanics acting through normal physics velocity changes in the simulation (no time
// dilation, no presentation holds), behind settings, so default play is unchanged.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiAerialProps.generated.h"

class USphereComponent;
class UBoxComponent;
class AZoogiBall;
class AZoogiArena;
class UProceduralMeshComponent;
class UMaterialInterface;
struct FZoogiMeshBuilder;

UENUM()
enum class EZoogiPropKind : uint8
{
	SkyBumper,
	ResonanceHalo,
	LaunchRail,
	Gate            // Zone 3 recovery gate (red / blue): trigger only, no collision (Frozen map layout, Oct 8)
};

UCLASS()
class ZOOGIROLL_API AZoogiAerialProp : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAerialProp();
	EZoogiPropKind Kind = EZoogiPropKind::SkyBumper;
	int32 PropId = 0;
	FVector Forward = FVector::ForwardVector;   // halo pass axis / rail direction
	float Size = 70.f;
	float ReboundMul = 2.f;
	float ReboundMaxSpeed = 3200.f;
	/** Hanging Zone 2 halo: full ring (no floor opening) centred on Location, pass trigger at the ring centre. */
	bool bAerialRing = false;
	/** Launch Rail: minimum release speed for this rail (0 = [ZoogiRoll.Aerial] RailMinSpeed). */
	float RailMinSpeed = 0.f;
	/** Launch Rail: world points of the curved approach track (visual only; the rail entry is the trigger). */
	TArray<FVector> VisualPath;
	/** Gate colour / map-layout name ("red", "blue", "chute", ...; empty for test props). */
	FName Tag;
	/** Placeholder visuals (procedural meshes, Frozen materials). Never collide. */
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Visuals;
	UPROPERTY() TArray<TObjectPtr<USphereComponent>> Solids;
	UPROPERTY() TObjectPtr<UBoxComponent> Trigger;

	void Build(EZoogiPropKind InKind, int32 Id, const FVector& Location, const FVector& InForward, float InSize);
	FString Label() const;
	void DrawDebug() const;
	/** Builds the placeholder visuals (no collision) with the arena's Frozen materials (fallback colours elsewhere). */
	void BuildVisuals(AZoogiArena* Arena);
	/** World point a Zoogi must reach to use the prop (halo opening / rail entry / bumper centre). */
	FVector UsePoint() const;

	UFUNCTION() void OnSolidHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	/** Pass / rail trigger volume (no collision: polled by the battle state machine, plane crossing / entry). */
	bool InTrigger(const FVector& Local) const;
	// last Sky Bumper rebound (tests / log)
	FVector LastIn = FVector::ZeroVector, LastOut = FVector::ZeroVector, LastNormal = FVector::ZeroVector;
	int32 Rebounds = 0;
	/** One event per Zoogi per channel (0 = solid hit, 1 = trigger) within Seconds of game time. */
	bool Cooldown(AActor* A, float Seconds, int32 Channel = 0);
protected:
	TMap<TWeakObjectPtr<AActor>, float> LastEvent[2];
	UProceduralMeshComponent* NewVisual(UMaterialInterface* M, const FZoogiMeshBuilder& B);
};

/** Frozen Ring aerial layer set dressing (Zone 2 floating discs / hanging rings / cables, Zone 3 orbit ring). Visual only,
 *  no collision, no gameplay: spawned with the map prop layout (Oct 8). */
UCLASS()
class ZOOGIROLL_API AZoogiAerialDecor : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAerialDecor();
	void Build(AZoogiArena* Arena, const TArray<AZoogiAerialProp*>& Props);
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
protected:
	void Add(UMaterialInterface* M, const FZoogiMeshBuilder& B);
};
