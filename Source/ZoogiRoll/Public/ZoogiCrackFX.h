// Oct 22 (Evan): glowing ground cracks / craters (fx/cracks). Original procedural look, see Scripts/cracks/cracks_fx.py.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArtifacts.h"
#include "ZoogiCrackFX.generated.h"

class AZoogiBall;
class UProceduralMeshComponent;
class UPointLightComponent;
class UNiagaraComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EZoogiCrackStyle : uint8 { Lava, Ice, Warm, Fire, Frost, Shock, Void, Titan, Crystal, Toxic, MAX UMETA(Hidden) };

/** A short-lived crater: a grid disc with the M_ZoogiCrack master (Voronoi stone cells, glowing seams, cells raised into a
 *  rim by WPO, soft irregular masked edge), a few additive light shafts rising from the seams, a Niagara spark burst and a
 *  small tinted light. Grows in ~0.15 s, holds, then the glow fades and the cells settle over the last ~1.2 s.
 *  Uses (all visual only: no collision, never touches physics or destruction):
 *   A) hard Zoogi-vs-Zoogi hits and hard landings (map-tinted: Volcanic lava, Frozen ice, others warm stone),
 *   B) Awaken infuse / hits coloured by element (Ember fire, Frost, Storm shock, Void, Titan),
 *   C) Crystal Collapse card (big cyan / purple crystal crater).
 *  [ZoogiRoll.Cracks] bEnabled, ImpactMin, LandSpeed, Life, MaxLive, bLight; -ZoogiNoCracks (or -ZoogiNoKFX) turns it off.
 *  -ZoogiCrackShot (with -ZoogiShots=<dir>) runs a showcase that spawns every style in view and saves screenshots. */
UCLASS(Transient, NotPlaceable)
class ZOOGIROLL_API AZoogiCrackFX : public AActor
{
	GENERATED_BODY()
public:
	AZoogiCrackFX();
	virtual void Tick(float Dt) override;

	static bool IsEnabled();
	/** Spawns a crater on the floor at Floor (z = floor surface). Radius in cm, Life in s, Strength ~0.5..1.5 (rim height, shafts). */
	static AZoogiCrackFX* Spawn(UWorld* World, const FVector& Floor, float Radius, EZoogiCrackStyle Style, float Life, float Strength, const TCHAR* Reason);
	static EZoogiCrackStyle StyleForMap(const UWorld* World);
	static EZoogiCrackStyle StyleForArtifact(EZoogiArtifact A);
	static const TCHAR* StyleName(EZoogiCrackStyle S);
	/** Floor point under P (static world trace), or P when nothing is found. */
	static FVector FloorUnder(const UWorld* World, const FVector& P);

	// ---- hooks
	/** A: hard Zoogi-vs-Zoogi impact (impulse in the OnSphereHit units). Element tint when one side is Awakened. */
	static void OnImpact(UWorld* World, const FVector& At, float Impulse, EZoogiArtifact Element);
	/** A: hard landing (floor contact with a big downward speed). Throttled per Zoogi. */
	static void OnLanding(AZoogiBall* Ball, const FVector& At, float DownSpeed);
	/** B: Awaken power triggers (infuse) or lands (hit). */
	static void OnAwaken(UWorld* World, const FVector& Floor, EZoogiArtifact Element, bool bHit);
	/** C: Crystal Collapse card contact / collapse. */
	static void OnCrystalCollapse(UWorld* World, const FVector& At, bool bBig);
	/** Showcase driver (-ZoogiCrackShot); called from AZoogiBall::Tick. */
	static void EnsureShowcase(UWorld* World);

protected:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UNiagaraComponent> Sparks;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShaftMat;
	EZoogiCrackStyle Style = EZoogiCrackStyle::Warm;
	float Radius = 150.f, Life = 3.f, Strength = 1.f, Age = 0.f, LightPeak = 0.f;
	struct FShaft { FVector P; float W, H, Delay, Len; };
	TArray<FShaft> Shafts;
	void Init();
	void BuildDisc();
	void UpdateShafts(float Fade);
};

/** -ZoogiCrackShot driver: cycles every use / style in front of the camera, one screenshot each. */
UCLASS(Transient, NotPlaceable)
class ZOOGIROLL_API AZoogiCrackShowcase : public AActor
{
	GENERATED_BODY()
public:
	AZoogiCrackShowcase();
	virtual void Tick(float Dt) override;
protected:
	float T = 0.f;
	int32 Step = -1;
	bool bShot = false;
	TWeakObjectPtr<AZoogiCrackFX> Cur;
};
