#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPhysicalMaterial;
class UZoogiArtifactComponent;
class UZoogiExpressionComponent;

/** A Zoogi marble: a Chaos-simulated sphere with the character model riding inside it. */
UCLASS()
class ZOOGIROLL_API AZoogiBall : public APawn
{
	GENERATED_BODY()

public:
	AZoogiBall();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
	/** Bible characters: the two flipper nubs (SM_<Asset>_Nubs), a visual-only child of Visual (no collision).
	 *  Hidden project-wide with [ZoogiRoll.Characters] ShowFlipperNubs=False in Config/DefaultGame.ini. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Nubs;
	/** Flat glowing ring under the marble in the player's colour (does not roll). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UPointLightComponent> TurnLight;
	/** Milestone "Awaken": artifact hand / infuse / guard / status and the transformation visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UZoogiArtifactComponent> Artifact;
	/** Facial expressions + light character animation driven by the turn phases (ZoogiExpressionComponent.h). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UZoogiExpressionComponent> Expression;
	/** Expressions: while bActive, turns the model (face = local +X) toward WorldFace (slerp, Rate 1/s). Inactive keeps the
	 *  current offset frozen in the sphere's frame, so the face simply rolls with the ball. Visual only. */
	void SetFaceTarget(bool bActive, const FQuat& WorldFace, float Rate = 4.f);
	/** Owner / team (one Zoogi per player for now; lets N Zoogis per player come later). */
	UPROPERTY(BlueprintReadOnly) int32 TeamIndex = 0;
	/** Mass with no artifact modifiers (Titan Core / guard scale this temporarily). */
	float BaseMassKg = 10.f;
	void SetMassScale(float Mul);
	bool IsActiveTurn() const { return bActiveTurn; }

	void InitZoogi(const FZoogiDef& InDef, int32 InPlayerIndex, bool bInCPU, UPhysicalMaterial* PhysMat, float InRollingDecel);
	void Launch(const FVector2D& PlanarVelocity);
	void PlaceAt(const FVector& Location, float FacingYaw);
	void SetOut();
	void SetActiveTurn(bool bActive);

	bool IsAtRest() const;
	float PlanarSpeed() const;
	FVector GetBallVelocity() const;
	/** Velocity sampled at the start of this frame's tick (before this frame's physics step): the approach velocity for hit events. */
	FVector PrevVelocity = FVector::ZeroVector;
	FVector BallLocation() const;

	UPROPERTY(BlueprintReadOnly) FZoogiDef Def;
	UPROPERTY(BlueprintReadOnly) int32 PlayerIndex = 0;
	UPROPERTY(BlueprintReadOnly) bool bCPU = false;
	UPROPERTY(BlueprintReadOnly) int32 Score = 0;
	float TrailClock = 0.f;
	float SparkClock = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bOut = false;
	UPROPERTY(BlueprintReadOnly) int32 LastHitPlayer = -1;
	UPROPERTY(BlueprintReadOnly) FVector SpawnLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float SpawnYaw = 0.f;
	/** Heading the camera and aim use at the start of this player's turn. */
	UPROPERTY(BlueprintReadOnly) float AimYaw = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 KnockOuts = 0;
	/** 2v2 Combo: Ring Marks (2 = two diamonds). First ring-out -> Recovery Gate return, second -> knocked out for the round. */
	UPROPERTY(BlueprintReadOnly) int32 RingMarks = 2;
	UPROPERTY(BlueprintReadOnly) bool bKnockedOut = false;
	bool bPendingRecovery = false;
	/** Guard Reaction: the next push over the edge this turn is caught at the rim. */
	bool bEdgeSave = false;
	UPROPERTY(BlueprintReadOnly) int32 OrbsScored = 0;
	float RollingDecel = ZoogiConst::RollingDecel;

	// ---- powers
	/** Power charges (start with 1, star orbs give more, max 3). */
	UPROPERTY(BlueprintReadOnly) int32 Charges = 1;
	/** Turns to skip (Static Shock / Shadow Stun). */
	UPROPERTY(BlueprintReadOnly) int32 StunTurns = 0;
	/** Wraps' Bandage Bind: the next shot is weaker. */
	UPROPERTY(BlueprintReadOnly) int32 BoundShots = 0;
	UPROPERTY(BlueprintReadOnly) bool bPhasing = false;
	/** Bolt: pass through other Zoogis (they overlap instead of colliding). */
	void SetPhasing(bool bOn);
	/** Sets the planar velocity directly (power effects). */
	void SetPlanarVelocity(const FVector& V, float Up = 0.f);
	float StatusTime = 0.f;
	float LastImpactFx = -10.f;
	float LastScarAt = -10.f;

	virtual void Tick(float DeltaSeconds) override;

	// ---- visual hooks (checklist item 4). Visual only: the physics sphere is never scaled or rotated by these.
	// The character model stays a sphere with no limbs; everything here is a short, subtle transform of the Visual mesh.
	/** Core pulse: the artifact glow / light pulse plus a small shell expansion of the model (1.0 -> 1.06 -> 1.0 at Strength 1). */
	void PulseCore(float Strength);
	/** Squash and stretch for <= 0.15 s along a world direction: scale (1+a, 1-a/2, 1-a/2) in that frame, volume roughly kept.
	 *  Amount > 0 stretches along Dir (launch), Amount < 0 compresses along Dir (impact). Clamped to +-0.15. */
	void Squash(float Amount, const FVector& WorldDir);
	/** Extra visual spin about a world axis (deg/s, decays over ~0.35 s); the model ends rolled ahead of the sphere, rolling stays true. */
	void SpinBoost(float DegPerSec, const FVector& WorldAxis);
	/** Small decaying tilt wobble (degrees, <= 6) away from a world direction, e.g. the impact normal. */
	void Wobble(float Degrees, const FVector& WorldDir);
	/** Shell expansion only (PulseCore without the glow): uniform model scale 1 -> 1 + 0.06 * Strength -> 1 over 0.5 s. */
	void ShellExpand(float Strength);
	/** Persistent model scale on top of the fit (Titan Core infuse: 1.04, "rock plating"); 1 = normal size. */
	void SetModelScale(float Scale);
	/** -ZoogiHookGain=<x> scales every hook (0 disables them, default 1); -ZoogiNoHooks is the same as 0. */
	static float HookGain();

protected:
	/** Applies the hook state to Visual's relative transform (called from Tick, and once from FitVisual). */
	void TickVisualHooks(float Dt);
	void ResetVisualHooks();
	FQuat BaseVisualRot = FQuat::Identity;
	FVector BaseVisualScale = FVector::OneVector;
	FVector BaseVisualOrigin = FVector::ZeroVector; // mesh bounds centre (local units) kept at the sphere centre while scaling
	FQuat SpinLocal = FQuat::Identity;              // accumulated extra spin in the sphere's local frame
	FVector SpinAxisWorld = FVector::ZeroVector;
	float SpinRate = 0.f;                           // deg/s
	float SquashAge = 10.f, SquashAmount = 0.f;
	FVector SquashDirWorld = FVector::ForwardVector;
	float WobbleAge = 10.f, WobbleDeg = 0.f;
	FVector WobbleAxisWorld = FVector::RightVector;
	float ExpandAge = 10.f, ExpandStrength = 0.f;
	float LastSquashTime = -10.f;
	float ModelScale = 1.f;
	FQuat FaceAlign = FQuat::Identity;              // expression face turn, in the sphere's local frame
	FQuat FaceTargetWorld = FQuat::Identity;
	bool bFaceTarget = false;
	float FaceRate = 4.f;
	/** -ZoogiHookDemo (with -ZoogiShots): close-up test camera on P1 during its first turn, firing each hook once for review shots. */
	void TickHookDemo(float Dt);
	float HookDemoClock = -1.f;
	int32 HookDemoStep = 0;
	UPROPERTY() TObjectPtr<class ACameraActor> HookDemoCam;
	TWeakObjectPtr<AActor> HookDemoPrevView;
	bool bHooksDirty = false;
	virtual void BeginPlay() override;
	void FitVisual();

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool bActiveTurn = false;
	float RingPulse = 0.f;
	/** Roll loop (team brief): one looping audio component per Zoogi, volume / pitch follow speed and weight, stopped at rest. */
	void TickRollSound();
	UPROPERTY() TObjectPtr<class UAudioComponent> RollAudio;
	bool bRollSoundMissing = false;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> RingMID;
};
