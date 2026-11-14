#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;

/** Easy Shockwaves VFX (Fab / Vefects): size from impact Mag. */
enum class EZoogiShockwaveSize : uint8 { Small = 0, Normal, Big };
/** Flat ground ring (Zoogi collisions / pad) vs vertical burst (ring-out). */
enum class EZoogiShockwaveKind : uint8 { Flat = 0, Vertical };

/** Short-lived additive glow: an expanding sphere or flat ring with a light flash (powers, impacts, knock-offs). */
UCLASS()
class ZOOGIROLL_API AZoogiFX : public AActor
{
	GENERATED_BODY()

public:
	AZoogiFX();

	/** Spawns an effect. Radius is the final radius in cm. bRing = flat shockwave ring instead of a sphere. */
	static AZoogiFX* Spawn(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, bool bRing = false, float Strength = 6.f, AActor* FollowActor = nullptr);
	/** Cheap ground puff with no light (used for the rolling shooter's trail). */
	static AZoogiFX* SpawnTrail(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, float Strength);
	/** Oct 22 queue #16: cheap confetti burst (many short trails, no Niagara). */
	static void SpawnConfetti(UWorld* World, const FVector& Location, const FLinearColor& Accent, int32 Count = 28);
	/** Oct 22 QQ #26: soft fading scuff mark on the floor (no light; dark GlowMID ring). */
	static AZoogiFX* SpawnMark(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration);

	/** Spawns /Game/FX/<SystemName> (Niagara, auto-destroys). No-op if missing or with -ZoogiNoNiagara. */
	static void Niagara(UWorld* World, const TCHAR* SystemName, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.f);
	/** b57 (Evan: "No sparks during clash"): while set, Spawn / Niagara / Stylized / KFX / Shockwave spawn nothing (UZoogiFunSys sets it during a power clash). */
	static inline bool bMuted = false;
	/** Nov 1 (Evan bought the Stylized VFX Pack): cartoon blasts from /Game/VFX_PackVol1/Particles/P_<Name>_Converted
	 *  (Big_Explosion[_v2|_v3], Mid_Explosion[_v2|_v3], ElectricShock[_v2|_v3], Fire[_v2|_v3], Smoke[_v2], Trail, WheelSmoke). -ZoogiNoStyFX = off. */
	static void Stylized(UWorld* World, const TCHAR* Name, const FVector& Location, float Scale = 1.f, const FRotator& Rotation = FRotator::ZeroRotator);
	/** b171: screen shake for a big blast (every Big_Explosion and every player knockout). */
	static void BigBlastShake(UWorld* World, float Scale = 1.f);
	/** Looping pack effect (fires on braziers): a persistent component, attached when Attach is given. */
	static UNiagaraComponent* StylizedLoop(UWorld* World, const TCHAR* Name, const FVector& Location, float Scale = 1.f, USceneComponent* Attach = nullptr);
	/** The Niagara burst that matches a Zoogi power (NS_Explosion for Hotstreak, coloured bursts otherwise). */
	static const TCHAR* PowerBurst(EZoogiPower Power);
	/** Spawns a ported KriptoFX system /Game/VFX/KriptoFX/<EffectN>/NS_KFX_<Prefab> (e.g. "Effect6_Collision"). MaxLife > 0 deactivates it
	 *  after that many seconds (some emitters loop). Attach = follow that actor (upright). No-op with -ZoogiNoNiagara / -ZoogiNoKFX. */
	static UNiagaraComponent* KFX(UWorld* World, const TCHAR* Prefab, const FVector& Location, float Scale = 1.f, float MaxLife = 3.f, AActor* Attach = nullptr);

	/**
	 * Spawns Vefects Easy Shockwaves Niagara + matching SFX cue.
	 * Distortion only when overall scalability >= High (skip Low / mobile low).
	 * Caps concurrent instances; uses Niagara auto-pool. No-op with -ZoogiNoNiagara / -ZoogiNoShockwave.
	 */
	static void Shockwave(UWorld* World, const FVector& Location, EZoogiShockwaveSize Size, EZoogiShockwaveKind Kind = EZoogiShockwaveKind::Flat,
		const FLinearColor* Tint = nullptr, bool bRingOutSfx = false);
	/** Picks Small / Normal / Big from Mag (impact impulse or speed) then Shockwave. */
	static void ShockwaveByMag(UWorld* World, const FVector& Location, float Mag, const FLinearColor* Tint = nullptr, bool bVertical = false, bool bRingOutSfx = false);
	/** Preloads white + vertical + distortion systems into Keep (call from Prewarm sites). */
	static void PrewarmShockwaves(UWorld* World, TArray<TObjectPtr<UObject>>& Keep);

	/**
	 * Free Arrow Trail (Fab): attach ribbon Niagara to an actor (upright, absolute rot/scale).
	 * Which: "Basic" / "Basic_02" (clean white) or "Water" (blue orb ribbon). MaxLife > 0 auto-stops;
	 * 0 = caller manages via ReleaseArrowTrail. Soft pool ~6 concurrent (~4 orbs + fast-roll). No-op with -ZoogiNoNiagara / -ZoogiNoArrowTrail.
	 */
	static UNiagaraComponent* ArrowTrail(UWorld* World, AActor* Attach, const TCHAR* Which = TEXT("Basic"),
		float Scale = 0.45f, float MaxLife = 0.f);
	/** Deactivate + destroy (or free pool slot) for a ribbon from ArrowTrail. */
	static void ReleaseArrowTrail(UNiagaraComponent* C);
	/** Preloads Basic / Basic_02 / Water into Keep. */
	static void PrewarmArrowTrails(UWorld* World, TArray<TObjectPtr<UObject>>& Keep);

	virtual void Tick(float DeltaSeconds) override;

	/** Oct 14 polish: loads these /Game/FX Niagara systems now (no first-use hitch) and fills the glow / ring actor pool. */
	static void Prewarm(UWorld* World, const TArray<const TCHAR*>& Systems, TArray<TObjectPtr<UObject>>& Keep, int32 PoolActors = 16);
	static int32 PoolReused() { return Reused; }

protected:
	/** Finished effects go back to a small pool (hidden, not ticking) instead of being destroyed; Spawn reuses them. */
	void Release();
	static int32 Reused;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Flash;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	UPROPERTY() TWeakObjectPtr<AActor> Follow;
	float Age = 0.f;
	float Life = 0.6f;
	float EndRadius = 300.f;
	float BaseStrength = 6.f;
	float BaseLight = 0.f;
	bool bIsRing = false;
};

