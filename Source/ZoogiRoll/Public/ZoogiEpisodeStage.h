#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiEpisodeStage.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPostProcessComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class ACameraActor;

/**
 * Episode Engine presentation actor: everything here is visual only (no collision, no physics, never touches gameplay
 * actors). Driven by UZoogiEpisodeSubsystem with real (undilated) time, so it animates during a presentation freeze.
 *  - SceneCapture panels for the centre-frame compositor (render targets created on first use)
 *  - glow pulse copies on the wall / piece a trick shot used
 *  - Titan Meteor: darkening post process, lifting dust and pebbles, a giant rock silhouette that falls in perception
 *    space (no collision), impact light + shockwave ring, and its own camera
 */
UCLASS(NotPlaceable)
class ZOOGIROLL_API AZoogiEpisodeStage : public AActor
{
	GENERATED_BODY()
public:
	AZoogiEpisodeStage();

	// ---- panels
	void EnsureCaptures(int32 Num, int32 Res);
	void SetCaptureView(int32 Index, const FVector& Location, const FVector& LookAt, float FOV);
	void CaptureAll(int32 Num);
	TArray<UTextureRenderTarget2D*> GetTargets(int32 Num) const;

	// ---- glow pulse on a used surface
	void GlowSurface(UPrimitiveComponent* Surface, const FVector& At, const FVector& Normal, const FLinearColor& Color);

	// ---- Titan Meteor
	void BeginMeteor(const FVector& Target, const FVector& Shooter, const FLinearColor& Color);
	/** T = 0..1 normalized meteor time. Returns true on the frame the rock reaches the target (impact). */
	bool UpdateMeteor(float T);
	void EndMeteor();
	ACameraActor* GetCamera() const { return Cam; }
	FVector GetMeteorRockLocation() const;

	void TickStage(float RealDt);

protected:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<USceneCaptureComponent2D>> Captures;
	UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> Targets;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Rock;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RockHeat;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pebbles;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Dust;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Shock;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShockMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeatMID;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> DustMIDs;
	UPROPERTY() TObjectPtr<UPointLightComponent> ImpactLight;
	UPROPERTY() TObjectPtr<ACameraActor> Cam;

	struct FGlow { TWeakObjectPtr<UPrimitiveComponent> Comp; TWeakObjectPtr<UMaterialInstanceDynamic> MID; float Age = 0.f; float Life = 0.9f; float Strength = 6.f; bool bRing = false; };
	TArray<FGlow> Glows;
	UPROPERTY() TArray<TObjectPtr<UPrimitiveComponent>> GlowComps;   // keeps the glow components alive
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMIDs;

	// meteor state
	bool bMeteor = false;
	bool bImpacted = false;
	FVector MTarget = FVector::ZeroVector;
	FVector MFloor = FVector::ZeroVector;
	FVector MFrom = FVector::ZeroVector;    // sky start
	FVector MCamBase = FVector::ZeroVector;
	FVector ShooterAt = FVector::ZeroVector;
	FLinearColor MColor = FLinearColor::White;
	float RockScale = 1.f;
	TArray<FVector> PebbleBase;
	TArray<FVector> DustBase;

	UProceduralMeshComponent* MakeRing(float Inner, float Outer, int32 Segments);
	void BuildMeteorParts();
};
