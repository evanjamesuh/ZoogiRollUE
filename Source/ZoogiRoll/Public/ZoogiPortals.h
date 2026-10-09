// Oct 8 (Evan 11:48 PM): Boost Portals on Frozen Ring. "think of the arcs ruins in front of the bridges as portals that accelerate
// an entering orb or zoogi and when it does it it has a flash electric effect like a power surge".
// Every bridge ruin arch (arena pieces of kind "arch" on a bridge yaw: 0 / 45 / 135 / 180 / 225 / 315 in the symmetric layout; not the snow-chute ramp arch 230 or
// the SE Recovery Gate) gets a trigger filling its opening. A Zoogi or orb crossing the opening plane gets BoostAdd of planar speed along its current planar direction
// (capped at MaxSpeed, vertical velocity kept, never flips motion), at most once per Cooldown seconds per object. Applied by
// game logic (this tickable world subsystem, fixed rules, no randomness), only while the shot is rolling, not in 2v2 combo.
// Look: an electric power-surge flash across the opening (jagged cyan / white lightning streaks + light pulse + Zap SFX)
// and a short jagged electric trail on the boosted object. No round sparkles.
// Config [ZoogiRoll.Portals] bPortals / BoostAdd / MaxSpeed / MinSpeed / Cooldown; -ZoogiPortals=On|Off for one run.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "ZoogiPortals.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class AZoogiArena;
class AZoogiArenaPiece;

/** One portal fire (tests / log). */
struct FZoogiPortalFire
{
	int32 Portal = -1;
	FString Who;          // "P1" / "O3"
	bool bOrb = false;
	int32 Index = -1;     // player index / orb index
	FVector VIn = FVector::ZeroVector, VOut = FVector::ZeroVector;
	double Time = 0.0;
};

/** Surge flash across an arch opening + idle portal sheet (visual only, no collision). */
UCLASS()
class ZOOGIROLL_API AZoogiPortalFX : public AActor
{
	GENERATED_BODY()
public:
	AZoogiPortalFX();
	virtual void Tick(float DeltaSeconds) override;
	/** Opening in this actor's frame: Y = -HalfW..HalfW, Z = 0..Top. */
	void Setup(float InHalfW, float InTop);
	void Surge(const FVector& Dir);
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segs;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Glows;   // wide soft cyan halo along each bolt
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Sheet;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatHot;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatCyan;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatSheet;
protected:
	float HalfW = 100.f, Top = 250.f;
	float SurgeT = 0.f, ReboltT = 0.f;
	int32 Serial = 0;
	void Rebolt();
};

/** Short jagged electric trail following a boosted Zoogi / orb (visual only). */
UCLASS()
class ZOOGIROLL_API AZoogiPortalTrail : public AActor
{
	GENERATED_BODY()
public:
	AZoogiPortalTrail();
	virtual void Tick(float DeltaSeconds) override;
	void Follow(AActor* Target, float Radius, int32 Seed);
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segs;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
protected:
	TWeakObjectPtr<AActor> Target;
	FVector LastPos = FVector::ZeroVector;
	float Radius = 50.f, Age = 0.f, ReboltT = 0.f;
	int32 Serial = 0;
};

UCLASS()
class ZOOGIROLL_API UZoogiPortalSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	/** Oct 9: ticks while a presentation hold pauses the game (Air Reaction reticle / captures only; portal logic skips). */
	virtual bool IsTickableWhenPaused() const override { return true; }

	static bool Enabled();
	float BoostAdd = 450.f, MaxSpeed = 2600.f, MinSpeed = 60.f, Cooldown = 0.75f;

	struct FPortal
	{
		TWeakObjectPtr<AZoogiArenaPiece> Arch;
		FTransform X;          // arch frame (no scale): X = radial (through the opening), Y = across, Z = up
		float HalfW = 100.f;   // ball centre may cross within +-HalfW
		float Top = 250.f;     // ... and below Top
		TWeakObjectPtr<AZoogiPortalFX> FX;
		FString Name;
	};
	TArray<FPortal> Portals;
	TArray<FZoogiPortalFire> Fires;   // every fire this match (tests read the ones after their action start)
	int32 FireCount(const FString& Who, double Since) const;
	const FZoogiPortalFire* FirstFire(const FString& Who, double Since) const;
protected:
	void Build(AZoogiArena* Arena);
	void Clear();
	TWeakObjectPtr<AZoogiArena> BuiltFor;
	bool bLoaded = false;
	TMap<TWeakObjectPtr<AActor>, FVector> Prev[16];   // per portal: previous local position of each object
	TMap<TWeakObjectPtr<AActor>, double> LastFire;
	// -ZoogiPortalShots=<dir>: engine-viewport captures just after a surge (never the desktop)
	FString ShotDir;
	TArray<TPair<FString, double>> ShotsLater;
	int32 ShotCount = 0;
	// -ZoogiTopShot=<file.png>: one straight-down engine-viewport capture of the whole Frozen layout (camera only, no physics)
	FString TopShot, CentreShot; // -ZoogiCentreShot=<file.png>: the centre crystal / fountain from the camera side
	int32 TopView = 0;
	int32 TopStage = 0;
	double TopAt = 0.0;
	TWeakObjectPtr<AActor> TopCam, TopPrevView;
	void TickTopShot(UWorld* W);
	// Oct 9: Air Reaction targeting reticle (cosmetic, follows the offered Zoogi) + -ZoogiPropShots=<dir> close-ups
	// (Sky Bumper impact, halo, air reticle): camera-only engine-viewport captures, never the desktop.
	TWeakObjectPtr<class AZoogiAirReticle> Reticle;
	void TickAirReticle(class AZoogiGameMode* G);
	FString PropShotDir;
	struct FViewShot { FString Path; FVector Loc; FVector Look; float Fov; int32 Delay; int32 Hold; }; // Delay / Hold in frames (also runs while a hold pauses the game)
	TArray<FViewShot> ViewQ;
	int32 ViewStage = 0;
	uint64 ViewAt = 0;
	TWeakObjectPtr<AActor> ViewCam, ViewPrev;
	TMap<TWeakObjectPtr<AActor>, int32> SeenRebounds;
	int32 PropShotCount = 0;
	bool bHaloShot = false, bAirShot = false, bIdleShot = false;
	void TickPropShots(UWorld* W, class AZoogiGameMode* G);
};
