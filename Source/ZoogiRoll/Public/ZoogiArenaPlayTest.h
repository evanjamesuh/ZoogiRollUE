#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArenaPlayTest.generated.h"

class AZoogiArena;
class AZoogiBall;
class AZoogiOrb;
class ACameraActor;

/** One scripted check for -ZoogiArenaPlayTest. */
struct FZoogiArenaCheck
{
	enum EKind : uint8 { OrbOut, OrbStays, ZoogiDamages };
	FString Name;
	EKind Kind = OrbOut;
	FVector From = FVector::ZeroVector;   // start (Z = ground; radius is added)
	FVector2D Vel = FVector2D::ZeroVector; // launch velocity cm/s
	FName PieceKind;                      // ZoogiDamages: piece kind that must reach DAMAGED or worse
	FVector Near = FVector::ZeroVector;   // ZoogiDamages: count pieces of PieceKind within Radius of Near
	float Radius = 500.f;
	float Wait = 3.0f;
};

/**
 * -ZoogiArenaPlayTest (generic map checks, Oct 7): after the stage forms (+2 s) each check runs in turn and logs
 * "ARENATEST <name>: ... PASS/FAIL", then "ARENATEST SUMMARY". OrbOut = an orb launched through an opening must meet the
 * knockoff rule (Z < FallOutZ or r > 8000) -> ORB OUT; OrbStays = control shot that must stay in; ZoogiDamages = a
 * normal-power Zoogi shot that must crack a breakable piece. Captures go to -ZoogiShots=<dir> as arenatest_<n>_<name>.png.
 */
UCLASS()
class ZOOGIROLL_API AZoogiArenaPlayTest : public AActor
{
	GENERATED_BODY()

public:
	AZoogiArenaPlayTest();
	virtual void Tick(float DeltaSeconds) override;
	TWeakObjectPtr<AZoogiArena> Arena;
	FString ShotDir;
	TArray<FZoogiArenaCheck> Checks;

	/** Spawns the tester if -ZoogiArenaPlayTest is on the command line (returns null otherwise). */
	static AZoogiArenaPlayTest* MaybeSpawn(AZoogiArena* InArena);

private:
	int32 Step = -1;
	double StepStart = 0.0;
	bool bStarted = false, bShot = false;
	int32 Before = 0;
	TWeakObjectPtr<AZoogiBall> TestBall;
	TWeakObjectPtr<AZoogiOrb> TestOrb;
	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	TArray<FString> Results;
	int32 CountDamaged(FName Kind, const FVector& Near, float Radius) const;
	void View(const FVector& Look, float Yaw, float Pitch, float Dist);
};
