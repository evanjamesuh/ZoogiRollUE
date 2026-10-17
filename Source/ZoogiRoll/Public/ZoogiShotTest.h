// -ZoogiShotTest: live-match verification of the Episode camera director (ZoogiEpisodeCameraDirector.h).
// Starts a match (P1 human, so the match holds still on P1's input), then cycles every shot family in both outputs
// (base five, Tier 1 reaction close-ups, Tier 2 trick shots, Tier 3 airborne with slow-mo entry), interruption cases
// (cancel, eased stop, overlap, focus destroyed, airborne cancelled mid slow-mo), the opt-in time-scale ease and a level end.
// After each case the gameplay camera is compared to the pre-shot snapshot and a "ZOOGI SHOTTEST PASS|FAIL" line is
// logged. Screenshots (with a debug panel showing the live render target, drawn only in this mode) go to
// %USERPROFILE%\Pictures\ZoogiShotTest (override with -ZoogiShotTestDir=<dir>).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateBrush.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "ZoogiShotTest.generated.h"

class SWidget;
class SBox;
class STextBlock;
class SZoogiShotRTView;
class AStaticMeshActor;
class AZoogiGameMode;
class UTextureRenderTarget2D;

namespace ZoogiShotTest
{
	enum class ECaseKind : uint8 { Shot, Cancel, StopEased, Overlap, FocusDestroyed, TimeScaleRefused, TimeScale, TimeScaleCancel, AirborneSlowMoCancel, AirborneSlowMoRefused, AvoidParticles, AvoidOcclusion, RingOutSimLegacy, RingOutSimFixed, RingOutDirector, ElevatorPush, Portrait, LevelEnd };
	enum class EStep : uint8 { StartMatch, WaitLive, WaitSettle, Run, WaitEnd, Verify, Restored, Done, Summary };
	struct FCase
	{
		FString Name;
		ECaseKind Kind = ECaseKind::Shot;
		EZoogiShotType Type = EZoogiShotType::CloseUp;
		EZoogiShotOutput Output = EZoogiShotOutput::RenderTarget;
	};
}

UCLASS(Transient, NotPlaceable)
class ZOOGIROLL_API AZoogiShotTestDriver : public AActor
{
	GENERATED_BODY()

public:
	AZoogiShotTestDriver();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	AZoogiGameMode* GM() const;
	UZoogiEpisodeCameraDirector* Dir() const;
	void BuildCases();
	void BuildOverlay();
	void ShowPanel(UTextureRenderTarget2D* RT, const FString& Caption);
	void HidePanel();
	void SetStatus(const FString& S);
	void Shot(const FString& Name);
	void ExportRT(UTextureRenderTarget2D* RT, const FString& Name);
	void PumpScreenshots();
	bool TrackSettle(float RealDt);
	void StartCase();
	void RunCase(float RealDt);
	void Verify();
	void Result(bool bPass, const FString& What, const FString& Detail);
	void Finish();
	AActor* Focus() const;
	AActor* SecondaryFor(AActor* F) const;
	AStaticMeshActor* SpawnProp(const FVector& At, float Scale);
	/** Plays one library shot the way the case's family needs it (props, secondary, event point). */
	void CheckTrickLookup();
	// Ring-out fall (Frozen Ring): a Zoogi-sized prop rolls off the rim and falls; the gameplay Tactical follow is
	// simulated with the controller's own rule (legacy or fixed) on a temp camera, or a director shot is played on it.
	FVector FallPos(float T) const;
	void StartRingOut(EZoogiShotType Type, bool bSim);
	void TickRingOut(float Dt);
	void CheckRingOutView(const FMinimalViewInfo& V, float Dt);
	bool bRingOut = false, bRingOutSim = false, bRingOutLegacy = false, bRingOutDone = false, bLateShot = false;
	float FallT = 0.f, FallEdgeT = 0.f, SimCourtZ = 50.f, RingLossNow = 0.f, RingLossMax = 0.f, RingMinCamZ = 1e9f, RingSeconds = 0.f;
	int32 RingFrames = 0, RingOutFrames = 0;
	FVector FallStart = FVector::ZeroVector, FallDir = FVector::ZeroVector, SimPivot = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<class ACameraActor> SimCam = nullptr;
	// Spawn-elevator push-in (Oct 16): the intro's forming-end window replayed on a temp camera with the real elevators,
	// pass 0 = the unchanged sweep (BEFORE), pass 1 = with the controller's push-in (AFTER).
	void StartElevPush();
	void TickElevPush(float Dt);
	bool bElevPush = false, bElevDone = false, bElevShot = false;
	int32 ElevPass = 0;
	float ElevT = 0.f, ElevFormLen = 6.f, ElevPeakW = 0.f, ElevEndDev = 0.f, ElevMinZ = 1e9f, ElevSpreadBase = 0.f, ElevSpreadPush = 0.f;
	int32 ElevPadsOut = 0, ElevPads = 0;
	bool bElevGateLive = true, bElevSpawned = false;
	FVector ElevFitPivot = FVector::ZeroVector; float ElevFitPitch = 0.f, ElevFitDist = 0.f, ElevBaseDist = 0.f;
	TArray<FVector> ElevPadFloors;
	// Portrait framing (Oct 17): BEFORE = the pre-portrait overview / aim view on this (narrow) screen, AFTER = ZoogiPortraitCam's.
	void StartPortrait();
	void TickPortrait(float Dt);
	bool bPortrait = false, bPortDone = false, bPortRestored = false;
	int32 PortStage = 0;
	float PortT = 0.f, PortAspect = 1.f, PortTop = 0.f, PortBot = 0.f, PortCourtR = 0.f, PortLegacyOut = 0.f, PortVFov = 0.f, PortCourtZ = 0.f;
	int32 PortCourtOut = 0, PortCourtN = 0, PortRing2Out = 0, PortRing2N = 0, PortShooterOut = 0, PortShooterN = 0;
	float PortCourtMaxX = 0.f, PortShooterMinY = 0.f, PortShooterMaxY = 0.f, PortAimPitch = 0.f;
	bool bPortModeOk = false, bPortBehind = false, bPortAimMode = false;
	float PortAimArm = 1.f, PortAimWant = 0.f, PortAimGot = 0.f;
	float ChaseEngineY = -1.f; int32 PortAxis = -1;
	bool bChaseRan = false; float ChaseShooterY = 0.f, ChaseAimX = 0.f, ChaseFarY = 0.f, ChasePitchGot = 0.f, ChaseCamH = 0.f, ChaseBoomGot = 0.f, ChaseArm = 1.f;
	FString PortModeLine;
	bool bBankMove = false; bool bBankHitLogged = false; float BankT = 0.f, BankTc = 0.f, BankSpeed = 0.f; FVector BankE = FVector::ZeroVector, BankIn = FVector::ZeroVector, BankOut = FVector::ZeroVector;
	void TintProp(class AStaticMeshActor* A, const FLinearColor& C);
	FZoogiShotHandle PlayCaseShot(EZoogiShotType Type, EZoogiShotOutput Output, FString& OutSetup);
	void StartArc(const FVector& From);
	void CheckLevelEnd(const TCHAR* When);

	TArray<ZoogiShotTest::FCase> Cases;
	int32 CaseIdx = -1;
	ZoogiShotTest::EStep Step = ZoogiShotTest::EStep::StartMatch;
	float Clock = 0.f;      // real seconds since BeginPlay
	float StepTime = 0.f;   // real seconds in the current step / case
	float StableTime = 0.f;
	int32 WaitFrames = 0;
	bool bHaveLastPOV = false;
	FMinimalViewInfo LastPOV;
	FString ShotDir;
	int32 ShotSeq = 0;
	TArray<FString> PendingShots;

	// Current case.
	FZoogiCameraSnapshot Snap0;
	TArray<FZoogiShotHandle> Handles;
	bool bMidDone = false, bActionDone = false, bAction2Done = false;
	float CaseMid = 1.f;
	float MaxMainDev = 0.f;       // RenderTarget cases: worst main-view deviation while the capture ran
	bool bTimeScaleStarted = false;
	float MinEffDilation = 1.f;
	TArray<FString> FinishLog;

	UPROPERTY() TObjectPtr<AStaticMeshActor> TrackProp = nullptr;
	UPROPERTY() TObjectPtr<AStaticMeshActor> TempProp = nullptr;
	UPROPERTY() TObjectPtr<AActor> TempFX = nullptr;
	UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> KeepTargets;
	FVector TrackCenter = FVector::ZeroVector;
	float TrackAngle = 0.f;
	/** Airborne prop: a kinematic (no physics, no collision) sphere flown on a ballistic arc in dilated game time. */
	UPROPERTY() TObjectPtr<AStaticMeshActor> ArcProp = nullptr;
	FVector ArcStart = FVector::ZeroVector;
	FVector ArcVel = FVector::ZeroVector;
	float ArcTime = 0.f;
	float MaxArcHeight = 0.f;
	FDelegateHandle TearDownHandle;

	TSharedPtr<SWidget> Overlay;
	TSharedPtr<SBox> PanelBox;
	TSharedPtr<SZoogiShotRTView> PanelView;
	TSharedPtr<STextBlock> PanelCaption;
	TSharedPtr<STextBlock> StatusText;
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	FDelegateHandle FinishedHandle;
};
