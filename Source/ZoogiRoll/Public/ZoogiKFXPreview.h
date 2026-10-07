#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiKFXPreview.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;
class UNiagaraComponent;

/** -ZoogiKFXPreview -ZoogiShots=<dir>: spawns every ported KriptoFX Niagara system (NS_KFX_*) far from the arena on a
 *  plain floor and saves two scene-capture PNGs of each (kfx_<Prefab>_a/b.png). Review tool only, never in normal play. */
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
};
