// Capture-only cinematic camera for trailer footage (Oct 16, Movie Director card-power trailer).
// Exists ONLY with -ZoogiCineCapture=<shot> on the command line (ShouldCreateSubsystem), so the shipped game, its
// in-match camera and every test run are untouched when the flag is off.
//
// While a filmed launch plays (the Nth PowerUp of the match, -ZoogiCineTurns=1,3 or "all"), it takes the main view with
// its own camera actor on a scripted path, switches the engine to a FIXED time step of 1/(60*Slow) s and dumps one
// HUD-free PNG per frame. Game time per frame is fixed, so the dump is a perfect 60 fps (Slow=1) or a Slow x
// over-cranked take that plays back slow when encoded at 60 fps. Presentation only: global time dilation is never
// touched, physics runs its normal rules at the fixed step.
//
//   -ZoogiCineCapture=track|pushin|orbit|crane|whip|ringout   shot path (see ZoogiCineCapture.cpp)
//   -ZoogiCineTake=N        variation (side / angle / speed), default 1
//   -ZoogiCineTurns=1,2|all which launches to film (1-based), default 1; with "all" up to -ZoogiCineMaxTurns (4)
//   -ZoogiCineSlow=K        over-crank (fixed step 1/(60K)), default 1
//   -ZoogiCineOut=<dir>     frame folder root (default Saved/CineCapture/<shot>_take<N>)
//   -ZoogiCineFocus=launch|power  (whose launch: any launch, or only launches with an infused artifact / armed card)
// Writes <out>/launchNN/f_00000.png ... plus events.txt (frame numbers of power-up, launch, first impact, ring-out,
// resolve) and quits when the last filmed launch has been recorded (or after -ZoogiCineMaxSeconds, default 420 real s).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiCineCapture.generated.h"

class ACameraActor;
class AZoogiBall;
class AZoogiGameMode;

UCLASS()
class ZOOGIROLL_API UZoogiCineCapture : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	static bool IsRequested();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiCineCapture, STATGROUP_Tickables); }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	enum class EShot : uint8 { Track, PushIn, Orbit, Crane, Whip, RingOut };
	EShot Shot = EShot::Track;
	FString ShotName, OutRoot;
	int32 Take = 1, Slow = 1, MaxTurns = 4;
	TArray<int32> Turns;
	bool bAllTurns = false, bPowerOnly = false;
	double StartReal = 0.0, MaxReal = 420.0;

	UPROPERTY() TObjectPtr<ACameraActor> Cam = nullptr;
	TWeakObjectPtr<AZoogiBall> Shooter, Target, Struck, Faller;
	bool bWasPowerUp = false, bRec = false, bQuitting = false;
	int32 LaunchNo = 0, Filmed = 0, Frame = 0;
	float T = 0.f;              // game seconds since this take's power-up
	float LaunchT = -1.f, ImpactT = -1.f, OutT = -1.f, ResolveT = -1.f;
	FVector AimDir = FVector::ForwardVector, ImpactPos = FVector::ZeroVector, OutDir = FVector::ForwardVector;
	FVector CamLoc = FVector::ZeroVector, LookAt = FVector::ZeroVector, VelDir = FVector::ForwardVector;
	TMap<TWeakObjectPtr<AZoogiBall>, FVector> StartPos;
	TArray<FString> Events;
	FString Dir;
	float Side = 1.f, OrbitAng = 0.f;
	TWeakObjectPtr<AActor> PrevVT;
	double QuitAt = 0.0;

	AZoogiGameMode* GM() const;
	bool WantLaunch(int32 N, AZoogiGameMode* G) const;
	void Begin(AZoogiGameMode* G);
	void End(const TCHAR* Why);
	void Event(const FString& What);
	void Rig(AZoogiGameMode* G, float Dt);
	void Apply(const FVector& Loc, const FVector& Look, float Fov, float Fstop, float Blur);
};
