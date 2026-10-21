// Oct 14 (Evan, 9:34 PM): Void-active look. While Void is active for the acting player (Awaken Void infusion = Void power /
// Void 1 gravity pull armed, or a committed Void-element card: Titan, Precision, Crystal Collapse) that player's Zoogi gets a
// purple ring of fire at ground level that follows it, and a purple void overlay material (dark swirl + rim glow) on its
// model, until the turn is over; then both revert cleanly (the previous overlay is restored, the ring is hidden and pooled).
// Visual only: no physics, no RNG. Config: [ZoogiRoll.VoidLook] bEnabled, RingColor, CoreColor, RimColor, RingRadius,
// FlameCount, FlameHeight, RingIntensity, OverlayIntensity. -ZoogiNoVoidLook turns it off for a run.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiVoidLook.generated.h"

class AZoogiBall;
class AZoogiGameMode;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

struct FZoogiVoidLookTune
{
	bool bEnabled = true;
	FLinearColor RingColor = FLinearColor(0.42f, 0.04f, 1.f, 1.f);
	FLinearColor CoreColor = FLinearColor(0.10f, 0.02f, 0.22f, 1.f);
	FLinearColor RimColor = FLinearColor(0.50f, 0.08f, 1.f, 1.f);
	float RingRadius = 95.f;
	int32 FlameCount = 16;
	float FlameHeight = 70.f;
	float RingIntensity = 2.2f;
	float OverlayIntensity = 1.3f;
	void Load();
};

UCLASS()
class ZOOGIROLL_API UZoogiVoidLook : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiVoidLook, STATGROUP_Tickables); }
	virtual void Deinitialize() override;
	/** Why Void is active for this Zoogi right now (empty = not active). */
	static FString VoidReason(const AZoogiGameMode* G, const AZoogiBall* B);
	bool IsApplied(int32 Player) const;

private:
	struct FLook
	{
		TWeakObjectPtr<AZoogiBall> Ball;
		TWeakObjectPtr<UMaterialInterface> Saved;
		bool bOn = false;
		int32 Turn = -1;
		float Age = 0.f;
		int32 First = 0; // index of this look's first flame in Comps (flames, then the floor ring)
	};
	void Apply(AZoogiGameMode* G, FLook& L, const FString& Why);
	void Revert(FLook& L, const TCHAR* Why);
	void UpdateRing(FLook& L, float Dt);
	TArray<FLook> Looks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Comps;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs; // [0] overlay, [1] flames, [2] floor ring
	FZoogiVoidLookTune Tune;
	bool bLoaded = false;
	float Time = 0.f;
};
