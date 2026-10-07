#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiKFXPreview.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;
class UNiagaraComponent;

/** -ZoogiKFXPreview -ZoogiShots=<dir>: spawns every ported KriptoFX Niagara system (NS_KFX_*) far from the arena on a
 *  plain floor and saves two scene-capture PNGs of each (kfx_<Prefab>_a/b.png). Review tool only, never in normal play.
 *  -ZoogiKFXPlan=<file>: frame-by-frame comparison mode. Each "shot" block in the plan sets a camera (matched to the
 *  KriptoFX demo framing), spawn events (prefab, time, place, scale, optional velocity for projectiles), stand-in props
 *  for scale, and capture times; PNGs are written as <shot>_<ms>.png. */
UCLASS()
class ZOOGIROLL_API AZoogiKFXPreview : public AActor
{
	GENERATED_BODY()
public:
	AZoogiKFXPreview();
	virtual void Tick(float DeltaSeconds) override;
	FString ShotDir;
protected:
	virtual void BeginPlay() override;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> RT;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Floor;
	UPROPERTY() TObjectPtr<UNiagaraComponent> Current;
	TArray<FString> Prefabs;
	int32 Index = -1;
	float T = 0.f;
	int32 Shot = 0;
	bool bDone = false;
	void Next();

	// --- plan mode
	struct FPlanEvent { FString Prefab; float Time = 0.f; FVector Pos = FVector::ZeroVector; float Scale = 1.f; FVector Vel = FVector::ZeroVector; float MaxLife = 0.f; bool bFired = false; };
	struct FPlanProp { FVector Pos = FVector::ZeroVector; float Height = 180.f; float Radius = 25.f; };
	struct FPlanShot { FString Name; FVector Cam = FVector(-700, 0, 170); FVector Look = FVector(0, 0, 80); float Fov = 60.f; TArray<float> Times; TArray<FPlanEvent> Events; TArray<FPlanProp> Props; };
	TArray<FPlanShot> Plan;
	int32 PlanIndex = -1;
	int32 PlanPass = 0;
	struct FMover { TWeakObjectPtr<AActor> Actor; FVector Vel; };
	TArray<FMover> Movers;
	UPROPERTY() TArray<TObjectPtr<AActor>> Spawned;
	bool LoadPlan(const FString& File);
	void NextPlan();
	void TickPlan(float Dt);
	void ClearPlanShot();
};
