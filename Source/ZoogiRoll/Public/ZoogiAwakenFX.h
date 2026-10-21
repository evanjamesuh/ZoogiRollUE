#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArtifacts.h"
#include "ZoogiAwakenFX.generated.h"

class AZoogiBall;
class UDecalComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UNiagaraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;

/** Awaken visuals rebuilt from the KriptoFX Realistic Effects 4 demo (Evan's picks, Oct 7):
 *  - LaunchRing (Effect4, orange): fire ring on the floor around the shooter after Awaken; radius and intensity follow the
 *    aim preview's launch fraction (read-only), fades out when the shot leaves.
 *  - ShockChain (Effect25): violet chain lightning from above the Zoogi's head to the floor with scorch splats; stops on the
 *    Zoogi's first non-floor collision and plays ShockHit (the Effect25_Collision burst) there.
 *  - VoidPortal (Effect5): violet ring portal with a starry interior; no rock / projectile (Void 1 = gravity pull only); the portal
 *    stays open briefly and pulls nearby orbs toward it before closing.
 *  Assets (materials, textures, Niagara) come from Scripts/kfx/finale/kfx_finale.py under /Game/VFX/AwakenFX. */
UENUM()
enum class EZoogiAwakenFX : uint8 { LaunchRing, ShockChain, ShockHit, VoidPortal, MeteorArm, VoidArm }; // VoidArm (Oct 14): invisible Void 1 watcher, opens the portal at the first orb / player collision // MeteorArm: invisible Void 2 watcher (fires the Titan Meteor on the first orb / player collision)

/** Void tuning, [ZoogiRoll.Void] in DefaultGame.ini (Evan, Oct 14). Void 1 = portal gravity pull only; Void 2 = Titan Meteor. */
struct FZoogiVoidTune
{
	float PullRadius = 1000.f;   // cm (was 650)
	float PullAccel = 1700.f;    // cm/s^2 at the rim scale 0.3..1 toward the sink (was 1100)
	float PullSeconds = 5.0f;    // s: portal stays open and pulls this long (Evan 12:58 PM; was 2.0, originally 1.4)
	float FadeSeconds = 1.2f;    // s: smooth close at the end of the pull (pull strength fades with it)
	float MeteorSeconds = 2.4f;  // Void 2 presentation length (real s)
	bool bPortalAfterHit = true; // Oct 14 (Evan): the Void 1 black hole opens at the collision point after the hit lands (False = old: at infuse)
};

UCLASS()
class ZOOGIROLL_API AZoogiAwakenFX : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAwakenFX();
	virtual void Tick(float DeltaSeconds) override;

	/** Awaken Infuse hook: launch ring for any artifact, plus the Storm / Void signature effect. */
	static void OnInfuse(UWorld* World, AZoogiBall* Ball, EZoogiArtifact Artifact);
	/** Review / capture entry (AZoogiKFXPreview plan "AFX_*" events). Param: LaunchRing (power from, power to, ramp s),
	 *  ShockChain (stop time, 0 = never), VoidPortal (target offset). */
	static AZoogiAwakenFX* SpawnFX(UWorld* World, EZoogiAwakenFX Kind, const FVector& Ground, float Scale, AZoogiBall* Ball, const FVector& Param, AActor* TargetActor = nullptr);
	/** Captures render from a scene capture: billboards face this when set. */
	static TOptional<FVector> ViewOverride;
	static FZoogiVoidTune GetVoidTune();
	/** True when Void 2 (Titan Meteor) is unlocked AND equipped on this Zoogi (UZoogiPowerUnlockSubsystem); else Void 1. */
	static bool UseVoid2(UWorld* World, FName ZoogiId);
	/** Void 2: Titan Meteor strikes Impact (the targeted orb / player), coming in along From -> Impact. Presentation only. */
	static AActor* PlayVoid2(UWorld* World, const FVector& From, const FVector& Impact, AActor* Target);

	UFUNCTION() void OnBallHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
	EZoogiAwakenFX Kind = EZoogiAwakenFX::LaunchRing;
	TWeakObjectPtr<AZoogiBall> Ball;
	TWeakObjectPtr<AActor> TargetActor;
	FVector Ground = FVector::ZeroVector;   // floor point under the effect
	FVector Param = FVector::ZeroVector;
	float S = 1.f;                          // 1 = KriptoFX demo size (180 cm character)
	float Age = 0.f;
	float EndAt = -1.f;                     // fade-out start (-1 = running)
	float Power = 0.f;
	FRandomStream Rng;

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Quads;      // billboards / bolts rebuilt every frame
	UPROPERTY() TObjectPtr<UDecalComponent> Decal;
	UPROPERTY() TObjectPtr<UDecalComponent> Decal2;
	UPROPERTY() TObjectPtr<UDecalComponent> Decal3;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light2;
	UPROPERTY() TObjectPtr<UNiagaraComponent> Niagara;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Card0;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Card1;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Card2;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Rock;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Chunks;
	UPROPERTY() TArray<TObjectPtr<UDecalComponent>> Splats;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> HitLights;
	UPROPERTY() TArray<TObjectPtr<UDecalComponent>> HitGlows; // soft lavender wash under each Shock strike point
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;

	// shock
	struct FBolt { FVector End; float NextMove = 0.f; float Flick = 1.f; };
	TArray<FBolt> Bolts;
	TArray<float> SplatBorn;
	float NextStrike = 0.f;
	int32 StrikeSeed = 0;
	// void
	FVector PortalC = FVector::ZeroVector, PortalN = FVector::ForwardVector, RockFrom = FVector::ZeroVector, RockTo = FVector::ZeroVector;
	float HitAt = -1.f;
	TArray<FVector> ChunkVel;
	struct FMote { float Born; float Ang; float Rad; float H; };
	TArray<FMote> Motes;
	float MoteAcc = 0.f;

	void Init();
	void TickRing(float Dt, float Fade);
	void TickShock(float Dt, float Fade);
	void TickShockHit(float Dt);
	void TickVoid(float Dt);
	void TickMeteorArm();
	void OpenVoidAt(const FVector& Floor, const FVector& Dir, AActor* Other);
	void Finish(float FadeTime);

	FVector ViewPos() const;
	UMaterialInstanceDynamic* MID(const TCHAR* Path);
	UDecalComponent* MakeDecal(const TCHAR* Mat, float Half, float Depth);
	UPointLightComponent* MakeLight(const FLinearColor& C, float Radius);
	UStaticMeshComponent* MakeCard(const TCHAR* Mat, const TCHAR* Mesh = nullptr);
	void SplatAt(const FVector& P, float Size, float Glow);

	// per-frame quad batch (local space = world - actor location)
	TArray<FVector> V; TArray<int32> Tri; TArray<FVector2D> UV0, UV1; TArray<FLinearColor> Col; TArray<FVector> Nrm;
	void BeginQuads();
	void Billboard(const FVector& C, float W, float H, const FLinearColor& Color, float Rand, bool bUpright);
	void Ribbon(const TArray<FVector>& Pts, float Width, const FLinearColor& Color);
	void EndQuads(int32 Section);
};
