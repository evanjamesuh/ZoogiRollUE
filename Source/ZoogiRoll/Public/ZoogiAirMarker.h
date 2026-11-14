// Oct 22 (Evan): air marker - a floor crosshair under every airborne Zoogi.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiAirMarker.generated.h"

class AZoogiBall;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** One per world (ZoogiAirMarker::Ensure from AZoogiBall::Tick). For every live Zoogi that is airborne (gap under the
 *  sphere > MinGap), traces straight down onto the ground / arena surface and draws a circle crosshair there, aligned
 *  to the surface normal: bright emissive ring + crosshair bars over a dark outline, so it reads on every map. Fades in
 *  ~0.1 s, fades out ~0.2 s on landing. Radius grows modestly with height. [ZoogiRoll.AirMarker] bEnabled, MinGap,
 *  Radius, RadiusPerM, MaxRadius, FadeIn, FadeOut; -ZoogiNoAirMarker for a run, -ZoogiAirMarkerShot (with -ZoogiShots)
 *  v3: white emissive core on a thick gold ring over a wide dark outline (opaque, stacked a few mm apart so no depth
 *  fighting), plus two expanding pulse rings (player tint, translucent sort priority 20) whose speed / reach grow with
 *  height. Saves a screenshot when one is first fully visible. Visual only: no collision, never touches physics. */
UCLASS()
class ZOOGIROLL_API AZoogiAirMarkers : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAirMarkers();
	virtual void Tick(float Dt) override;

	struct FEntry
	{
		TWeakObjectPtr<AZoogiBall> Ball;
		TArray<UStaticMeshComponent*> Bright, Dark, Core, Pulse; // Pulse: PulseRings x RingSegs
		UMaterialInstanceDynamic* PulseMat[2] = {nullptr, nullptr};
		float Alpha = 0.f, Age = 0.f;
		FVector Point = FVector::ZeroVector, Normal = FVector::UpVector;
		float Gap = 0.f;
		bool bAir = false, bHasPoint = false, bLogged = false;
	};

protected:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts; // keeps the segment components alive
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatBright;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatDark;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MatCore;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PulseMats;
	TArray<FEntry> Entries;
	int32 ShotN = 0; float LastShotAt = -10.f;
	float HopT = 0.f;
	int32 HopN = 0;
	FEntry& EntryFor(AZoogiBall* B);
	UStaticMeshComponent* NewSeg(UMaterialInterface* M, int32 SortPriority = 0);
	void Layout(FEntry& E);
};

namespace ZoogiAirMarker
{
	ZOOGIROLL_API void Ensure(UWorld* World);
}
