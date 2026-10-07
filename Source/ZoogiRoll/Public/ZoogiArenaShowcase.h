#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArenaShowcase.generated.h"

class ACameraActor;

/**
 * -ZoogiArenaShots (test runs only, with -ZoogiShots=<dir>): right after the arena is built, freezes the match clock,
 * waits for shader compiles, then frames a fixed set of views (End-aerial top-down, Tactical, Standby, hero/backdrop,
 * low three-quarter, rim rock, hub) and saves one screenshot per view as arena_<slug>_<view>.png. Afterwards the game
 * camera and the clock are restored and the CPU match continues normally. Same poses every run, so before/after A/B
 * comparisons line up. Visual tooling only: no gameplay state is touched.
 */
UCLASS()
class ZOOGIROLL_API AZoogiArenaShowcase : public AActor
{
	GENERATED_BODY()

public:
	AZoogiArenaShowcase();
	virtual void Tick(float DeltaSeconds) override;
	/** Spawns the showcase once per process if -ZoogiArenaShots is on the command line. */
	static void MaybeStart(UWorld* World, const FString& ArenaSlug);

protected:
	struct FPose { const TCHAR* Name; FVector Look; float Yaw, Pitch, Dist, Fov; };
	void ApplyPose(int32 Index);
	void Finish();

	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	TWeakObjectPtr<AActor> PrevView;
	TArray<FPose> Poses;
	FString Slug, Dir;
	double StartReal = 0.0, PoseStart = 0.0;
	int32 PoseIndex = -1;
	bool bShotTaken = false, bDone = false;
};
