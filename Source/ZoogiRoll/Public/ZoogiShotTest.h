// -ZoogiShotTest: live-match verification of the Episode camera director (ZoogiEpisodeCameraDirector.h).
// Starts a match (P1 human, so the match holds still on P1's input), then cycles every shot family in both outputs,
// interruption cases (cancel, eased stop, overlap, focus destroyed), the opt-in time-scale ease and a level end.
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
	enum class ECaseKind : uint8 { Shot, Cancel, StopEased, Overlap, FocusDestroyed, TimeScaleRefused, TimeScale, TimeScaleCancel, LevelEnd };
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
	UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> KeepTargets;
	FVector TrackCenter = FVector::ZeroVector;
	float TrackAngle = 0.f;

	TSharedPtr<SWidget> Overlay;
	TSharedPtr<SBox> PanelBox;
	TSharedPtr<SZoogiShotRTView> PanelView;
	TSharedPtr<STextBlock> PanelCaption;
	TSharedPtr<STextBlock> StatusText;
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	FDelegateHandle FinishedHandle;
};
