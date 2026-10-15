#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;

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

	/** Spawns /Game/FX/<SystemName> (Niagara, auto-destroys). No-op if missing or with -ZoogiNoNiagara. */
	static void Niagara(UWorld* World, const TCHAR* SystemName, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.f);
	/** The Niagara burst that matches a Zoogi power (NS_Explosion for Hotstreak, coloured bursts otherwise). */
	static const TCHAR* PowerBurst(EZoogiPower Power);
	/** Spawns a ported KriptoFX system /Game/VFX/KriptoFX/<EffectN>/NS_KFX_<Prefab> (e.g. "Effect6_Collision"). MaxLife > 0 deactivates it
	 *  after that many seconds (some emitters loop). Attach = follow that actor (upright). No-op with -ZoogiNoNiagara / -ZoogiNoKFX. */
	static UNiagaraComponent* KFX(UWorld* World, const TCHAR* Prefab, const FVector& Location, float Scale = 1.f, float MaxLife = 3.f, AActor* Attach = nullptr);

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
