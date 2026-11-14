// Test-only roster gallery (-ZoogiRosterGallery=<dir>): close-ups of every Tripo model with its expression decal.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiRosterGallery.generated.h"

class AStaticMeshActor;
class ACameraActor;
class UMaterialInstanceDynamic;

struct FZoogiGalleryEntry { FString Name; AStaticMeshActor* Actor = nullptr; UMaterialInstanceDynamic* Eyes = nullptr; };

UCLASS()
class UZoogiRosterGallery : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
private:
	void Build();
	void Step();
	TArray<FZoogiGalleryEntry> Entries;
	TArray<TPair<int32, int32>> Plan; // (entry, expression | -1 = lineup, -2..-5 = yaw check)
	int32 PlanIdx = 0;
	bool bArmed = false;
	UPROPERTY() TObjectPtr<ACameraActor> Cam = nullptr;
	FString Dir;
	FTimerHandle Timer;
};
