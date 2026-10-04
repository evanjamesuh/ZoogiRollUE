// Per-Zoogi artifact state (hand, infused, guard, status) and the temporary transformation visuals:
// core glow -> shell change -> aura -> particles -> trail -> powered collision -> impact.
// Zoogis stay spheres with no limbs: everything here is light, overlay material, shell, orbiting bits and particles.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArtifactComponent.generated.h"

class AZoogiBall;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup = (Zoogi), meta = (BlueprintSpawnableComponent))
class ZOOGIROLL_API UZoogiArtifactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZoogiArtifactComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- state (driven by AZoogiGameMode, ZoogiAwaken.cpp)
	static constexpr int32 HandCap = 2;
	/** Saved artifacts (oldest first). */
	UPROPERTY(BlueprintReadOnly) TArray<EZoogiArtifact> Hand;
	/** Drawn this turn and not used / saved yet. */
	UPROPERTY(BlueprintReadOnly) EZoogiArtifact Drawn = EZoogiArtifact::None;
	/** Bonded for this turn's attack. */
	UPROPERTY(BlueprintReadOnly) EZoogiArtifact Infused = EZoogiArtifact::None;
	/** Protects this Zoogi during the next opponent attack. */
	UPROPERTY(BlueprintReadOnly) EZoogiArtifact Guard = EZoogiArtifact::None;
	/** Player whose turn armed the guard (it expires at the end of the next opponent's turn). */
	int32 GuardSetTurn = -1;
	UPROPERTY(BlueprintReadOnly) EZoogiStatus Status = EZoogiStatus::None;
	UPROPERTY(BlueprintReadOnly) int32 StatusTurns = 0;
	/** Bonus draws earned (star orbs) for the next Draw. */
	UPROPERTY(BlueprintReadOnly) int32 BonusDraws = 0;
	/** Guard hit this frame: knockback damping is applied on the next GameMode tick (after the solver). */
	bool bGuardHitPending = false;
	TWeakObjectPtr<AZoogiBall> GuardAttacker;
	/** World time of the last absorbed hit (contacts repeat for a few frames: damp once). */
	float LastGuardHitTime = -10.f;

	void SetInfused(EZoogiArtifact A);
	void SetGuard(EZoogiArtifact A, int32 Turn);
	void ClearInfused();
	void ClearGuard();
	void ResetAll();
	/** Activation flash: shell expands, core flares (Awaken Infuse / Guard, Battle PowerUp). */
	void Pulse(float Strength);
	/** Powered collision: impact compression + flare. */
	void ImpactPulse(float Strength);
	/** Brief guard shield flash when it absorbs a hit. */
	void GuardFlash();
	FString StatusLabel() const;

protected:
	virtual void BeginPlay() override;
	void BuildVisuals();
	void ApplyLook();
	void TickVisuals(float Dt);
	AZoogiBall* Ball() const;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Shell;      // translucent shell (infuse) / bubble (guard)
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GuardShell;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Orbiters; // Storm arcs, Void rings, Ember motes
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Plates;   // Titan stone plates / Frost shards (roll with the ball)
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShellMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GuardMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OverlayMID;
	/** Item 2: dynamic instances of MI_Artifact_<Key> (shell, overlay) and MI_Guard_<Key>, one per artifact used. */
	UPROPERTY() TMap<EZoogiArtifact, TObjectPtr<UMaterialInstanceDynamic>> ArtifactMIDs;
	UPROPERTY() TMap<EZoogiArtifact, TObjectPtr<UMaterialInstanceDynamic>> OverlayMIDs;
	UPROPERTY() TMap<EZoogiArtifact, TObjectPtr<UMaterialInstanceDynamic>> GuardMIDs;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> OrbitMIDs;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PlateMIDs;

	float LookAge = 0.f;      // seconds since the current infuse look started (spread animation)
	float PulseAge = 10.f, PulseStrength = 0.f;
	float ImpactAge = 10.f, ImpactStrength = 0.f;
	float GuardFlashAge = 10.f;
	float AuraClock = 0.f, TrailClock = 0.f, Clock = 0.f;
	bool bBuilt = false;
};
