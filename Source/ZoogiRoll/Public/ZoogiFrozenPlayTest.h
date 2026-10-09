#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiFrozenPlayTest.generated.h"

class AZoogiArena;
class AZoogiBall;
class ACameraActor;

/**
 * -ZoogiFrozenPlayTest (Frozen Ring round 3c checks, Oct 6): after the stage forms, a non-active Zoogi is placed and
 * launched for 5 scripted checks, each logged as PLAYTEST ... PASS / FAIL with a capture:
 *  1 wall     normal-power shot (55% of full launch) into an ice ricochet wall -> it must reach DAMAGED or worse
 *  2 stand    normal-power shot from an islet apron into the bleachers -> a bleacher section damaged or worse
 *  3 bridge   medium roll from the court out over a bridge -> the Zoogi ends on the islet (not out)
 *  4 lip      medium roll across an islet into its snow lip -> stays on the islet
 *  5 ringout  full-power roll across an islet -> hops the lip and falls (ring out still possible)
 *  6-9 bridge_<yaw> (Oct 8, Evan 10:50 PM) normal shot from r1150 straight along each bridge centreline (186 / 354 / 140 / 36):
 *     through the bridge arch / wall gap -> passes within ZoogiRadius + OrbRadius + 10 of the islet orb spot, ends on the islet
 */
UCLASS()
class ZOOGIROLL_API AZoogiFrozenPlayTest : public AActor
{
	GENERATED_BODY()

public:
	AZoogiFrozenPlayTest();
	virtual void Tick(float DeltaSeconds) override;
	TWeakObjectPtr<AZoogiArena> Arena;
	FString ShotDir;

private:
	int32 Step = 0;
	double StepStart = 0.0;
	bool bShot = false;
	TWeakObjectPtr<AZoogiBall> TestBall;
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	TArray<FString> Results;
	int32 Before = 0;
	float BridgeMin = 0.f, BridgeMaxR = -1.f; // closest approach to the islet orb spot, reach time (-1 = not yet)
	FVector BridgeAt = FVector::ZeroVector;
	int32 CountDamaged(FName Kind, const FVector& Near, float Radius) const;
	void View(const FVector& Look, float Yaw, float Pitch, float Dist);
	void Shot(const TCHAR* Name);
};
