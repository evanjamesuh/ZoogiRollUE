#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoogiStageForming.generated.h"

class AZoogiArenaPiece;
class UMaterialInstanceDynamic;

/** How a piece arrives during the stage-forming intro. */
UENUM()
enum class EZoogiFormStyle : uint8
{
	Rise,    // comes up from below (through the floor for court pieces), eases in, optional small overshoot
	Drop,    // falls in from above and lands
	Grow,    // scales up from flat (ice "freezing" into shape)
	Slide    // slides in horizontally from an offset
};

/**
 * Reusable stage-forming intro (Frozen Ring, Oct 6; any arena can register pieces). At match start every registered
 * actor / component moves from its start pose to its final pose on its own delay + duration, while scalar parameters
 * (floor rune glow) ramp up. Colliders of registered actors stay OFF until that actor has settled exactly on its final
 * transform (no floating, misaligned or interpenetrating pieces when play starts). Skip() snaps everything to final.
 * Total length is the latest delay + duration (Frozen: ~3 s). IsDone() gates the first turn.
 */
UCLASS(ClassGroup = (Zoogi))
class ZOOGIROLL_API UZoogiStageForming : public UActorComponent
{
	GENERATED_BODY()

public:
	UZoogiStageForming();

	void Clear();
	/** Register an actor (its root moves; colliders of AZoogiArenaPiece actors are held off until settled). */
	void AddActor(AActor* A, EZoogiFormStyle Style, const FVector& Offset, float Delay, float Duration, float Overshoot = 0.f);
	/** Register a plain component (scenery without collision). */
	void AddComponent(USceneComponent* C, EZoogiFormStyle Style, const FVector& Offset, float Delay, float Duration, float Overshoot = 0.f);
	/** Scalar parameter ramp (e.g. floor GlowStrength 0 -> target). */
	void AddParamRamp(UMaterialInstanceDynamic* M, FName Param, float From, float To, float Delay, float Duration);

	/** Puts everything at its start pose and starts the clock. TimeScale > 1 slows it down (shots). */
	void Begin(float TimeScale = 1.f);
	void Skip();
	bool IsPlaying() const { return bPlaying; }
	bool IsDone() const { return !bPlaying; }
	float GetLength() const;
	float GetTime() const { return Time; }
	/** 0..1 progress (for HUD / shots). */
	float GetProgress() const { const float L = GetLength(); return L > 0.f ? FMath::Clamp(Time / L, 0.f, 1.f) : 1.f; }
	int32 Num() const { return Entries.Num(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	struct FEntry
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<USceneComponent> Comp;
		EZoogiFormStyle Style = EZoogiFormStyle::Rise;
		FVector Offset = FVector::ZeroVector;
		float Delay = 0.f, Duration = 1.f, Overshoot = 0.f;
		FTransform Final;
		bool bSettled = false;
		bool bHadVis = true;
	};
	struct FRamp
	{
		TWeakObjectPtr<UMaterialInstanceDynamic> M;
		FName Param;
		float From = 0.f, To = 1.f, Delay = 0.f, Duration = 1.f;
	};
	void Pose(FEntry& E, float T);
	void Settle(FEntry& E);
	USceneComponent* Target(const FEntry& E) const;

	TArray<FEntry> Entries;
	TArray<FRamp> Ramps;
	float Time = 0.f;
	float Scale = 1.f;
	bool bPlaying = false;
};
