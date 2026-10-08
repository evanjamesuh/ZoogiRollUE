// Episode Engine: camera director + episode time-scale control (spec v1.1 priorities 5 and 7).
//
// Plays FZoogiShotSpec shots on a focus actor (+ optional secondary target) into
//   (a) a UTextureRenderTarget2D via a scene capture (primary path: panels; never touches the main view), or
//   (b) the main view, by temporarily switching the view target to a director-owned camera.
//
// Restoration contract: the director NEVER writes to the gameplay camera actor, its components, its post-process or
// the PlayerCameraManager settings. The only things it changes are (1) the PlayerController's view target while a
// MainView shot runs and (2) AWorldSettings::CinematicTimeDilation while an opt-in time-scale ease runs. Both are
// snapshotted before the first change and put back exactly when the last shot / ease ends, is stopped, is replaced,
// loses its focus actor, or the world tears down. CaptureGameplaySnapshot / CompareSnapshots let callers verify it.
//
// Rule Zero: presentation only. No gameplay or physics state is read-modified-written. Time scaling slows the physics
// step too (the project steps Chaos with variable substeps, not a fixed step), so it is opt-in and by default refuses
// to run while any simulated body is awake. See SetTimeScalePolicy.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraTypes.h"
#include "Engine/Scene.h"
#include "ZoogiShotLibrary.h"
#include "ZoogiEpisodeCameraDirector.generated.h"

class ACameraActor;
class APlayerController;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UCurveFloat;

USTRUCT(BlueprintType)
struct ZOOGIROLL_API FZoogiShotHandle
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Shot") int32 Id = 0;
	bool IsValid() const { return Id > 0; }
	static FZoogiShotHandle Make(int32 InId) { FZoogiShotHandle H; H.Id = InId; return H; }
	bool operator==(const FZoogiShotHandle& O) const { return Id == O.Id; }
};

UENUM(BlueprintType)
enum class EZoogiShotEndReason : uint8
{
	Completed,
	Stopped,
	/** A newer MainView shot took over the main view. */
	Replaced,
	FocusLost,
	WorldEnding
};

USTRUCT(BlueprintType)
struct ZOOGIROLL_API FZoogiShotPlayParams
{
	GENERATED_BODY()
	/** Render target size when the director creates the target (RenderTarget output). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FIntPoint RenderTargetSize = FIntPoint(960, 540);
	/** Optional caller-owned target (e.g. the panel compositor's). The director creates one when null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
	/** > 0 overrides Spec.Duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") float DurationOverride = -1.f;
	/** Index of the local player whose view a MainView shot takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") int32 PlayerIndex = 0;
	/** Trick shots: the event point (bank / wall contact). Without it, event-based shots use the arena centre at the focus' height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") bool bHasEventLocation = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FVector EventLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct ZOOGIROLL_API FZoogiTimeScaleSpec
{
	GENERATED_BODY()
	/** Cinematic time dilation to ease to (multiplies gameplay's own TimeDilation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeScale", meta = (ClampMin = "0.01", ClampMax = "4")) float Target = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeScale", meta = (ClampMin = "0")) float EaseIn = 0.25f;
	/** Seconds (real) at Target. < 0 = hold until StopTimeScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeScale") float Hold = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeScale", meta = (ClampMin = "0")) float EaseOut = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeScale") EZoogiShotEase Ease = EZoogiShotEase::SmoothStep;
	/** Optional: overrides Ease for both ramps (0..1 -> 0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeScale") TObjectPtr<UCurveFloat> Curve = nullptr;
};

UENUM(BlueprintType)
enum class EZoogiTimeScalePolicy : uint8
{
	/** Default: EaseTimeScale refuses. */
	Disabled,
	/** Allowed only while no simulated rigid body is awake; aborts (exact restore) the moment one wakes. */
	OnlyWhenPhysicsAtRest,
	/** Always allowed. Only for callers that have locked the gameplay truth already (see the report on physics stepping). */
	Always
};

/** Everything about the player's camera that the director promises to leave untouched / put back. */
USTRUCT(BlueprintType)
struct ZOOGIROLL_API FZoogiCameraSnapshot
{
	GENERATED_BODY()
	UPROPERTY() bool bValid = false;
	UPROPERTY() TWeakObjectPtr<APlayerController> PC;
	UPROPERTY() TWeakObjectPtr<AActor> ViewTarget;
	/** Final rendered POV (PlayerCameraManager cache). */
	FMinimalViewInfo POV;
	bool bPCMBlending = false;
	// Gameplay camera actor (view target) and its camera component.
	bool bHasCameraComponent = false;
	FTransform CameraActorTransform = FTransform::Identity;
	FTransform CameraComponentTransform = FTransform::Identity;
	float CameraFOV = 90.f;
	bool bCameraConstrainAspect = false;
	float CameraAspectRatio = 1.777f;
	float CameraPPBlendWeight = 1.f;
	FPostProcessSettings CameraPP;
	// PlayerCameraManager.
	float PCMDefaultFOV = 90.f;
	float PCMLockedFOV = 0.f;
	FName PCMCameraStyle;
	bool bPCMOrthographic = false;
	bool bPCMEnableFading = false;
	float PCMFadeAmount = 0.f;
	float PCMViewPitchMin = 0.f, PCMViewPitchMax = 0.f;
	bool bAutoManageActiveCameraTarget = true;
	// World time.
	float TimeDilation = 1.f;
	float CinematicTimeDilation = 1.f;
};

USTRUCT(BlueprintType)
struct ZOOGIROLL_API FZoogiCameraTolerance
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verify") float LocationCm = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verify") float RotationDeg = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verify") float FOVDeg = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Verify") float Scalar = 0.0001f;
};

/** Internal per-shot state (USTRUCT so the render target / capture component are GC-referenced). */
USTRUCT()
struct FZoogiActiveShot
{
	GENERATED_BODY()
	UPROPERTY() int32 Id = 0;
	UPROPERTY() FZoogiShotSpec Spec;
	UPROPERTY() EZoogiShotOutput Output = EZoogiShotOutput::RenderTarget;
	UPROPERTY() TWeakObjectPtr<AActor> Focus;
	UPROPERTY() TWeakObjectPtr<AActor> Secondary;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture = nullptr;
	float Duration = 2.f;
	float Age = 0.f;
	/** Stop requested with ease: blend out from StopFrom over the spec's EaseOut. */
	bool bStopping = false;
	float StopAge = 0.f;
	float StopWeight = 1.f;
	// Framing state.
	bool bInit = false;
	float BaseYaw = 0.f;
	float VelYaw = 0.f;
	bool bVelSnapPending = false;
	FVector Pivot = FVector::ZeroVector;
	FVector LastFocus = FVector::ZeroVector;
	FVector LastSecondary = FVector::ZeroVector;
	bool bHasSecondary = false;
	/** Event point (trick shots); always set (default = arena centre at the focus' start height). */
	FVector EventLoc = FVector::ZeroVector;
	bool bEventFromCaller = false;
	/** Slow-mo entry attempted (once per shot) / accepted. */
	bool bSlowMoTried = false;
	bool bSlowMoRunning = false;
	/** MainView ease-in source: the live gameplay view, or the previous shot's last view when this one replaced it. */
	bool bBlendFromFrozen = false;
	FMinimalViewInfo BlendFrom;
	FMinimalViewInfo LastShotView;
	FMinimalViewInfo LastOutputView;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnZoogiShotFinished, FZoogiShotHandle /*Shot*/, EZoogiShotEndReason /*Reason*/);

UCLASS()
class ZOOGIROLL_API UZoogiEpisodeCameraDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UZoogiEpisodeCameraDirector* Get(const UObject* WorldContext);

	// USubsystem / FTickableGameObject
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiEpisodeCameraDirector, STATGROUP_Tickables); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// ------------------------------------------------------------------ shots
	/** The shot library (built-in defaults until SetShotLibrary is called with a data asset). */
	UZoogiShotLibrary* GetShotLibrary() const { return Library; }
	void SetShotLibrary(UZoogiShotLibrary* InLibrary);

	/**
	 * Plays Spec on Focus (required) with an optional Secondary target. RenderTarget output: the frames go to
	 * Params.RenderTarget (or a new 960x540 sRGB target; get it with GetShotRenderTarget). MainView output: the player's
	 * view eases into the shot and back; a newer MainView shot replaces an older one (the original gameplay snapshot is kept).
	 * Returns an invalid handle if Focus is null.
	 */
	FZoogiShotHandle PlayShot(const FZoogiShotSpec& Spec, AActor* Focus, AActor* Secondary, EZoogiShotOutput Output, const FZoogiShotPlayParams& Params = FZoogiShotPlayParams());
	/** Same with the library's spec for Type. */
	FZoogiShotHandle PlayShotType(EZoogiShotType Type, AActor* Focus, AActor* Secondary, EZoogiShotOutput Output, const FZoogiShotPlayParams& Params = FZoogiShotPlayParams());
	/** Ends a shot. bImmediate cuts straight back (MainView); otherwise it eases out over the spec's EaseOut. */
	void StopShot(FZoogiShotHandle Shot, bool bImmediate = false);
	void StopAllShots(bool bImmediate = true);
	bool IsShotActive(FZoogiShotHandle Shot) const;
	/** The target a RenderTarget shot draws into (stays valid after the shot ends, holding its last frame). */
	UTextureRenderTarget2D* GetShotRenderTarget(FZoogiShotHandle Shot) const;
	bool IsMainViewTaken() const { return MainViewShotId != 0; }
	int32 NumActiveShots() const { return Shots.Num(); }
	int32 NumActiveCaptures() const;
	/** The director-owned camera used for MainView shots (never the gameplay camera). */
	ACameraActor* GetShotCamera() const { return ShotCam; }

	FOnZoogiShotFinished OnShotFinished;
	/** Broadcast on world tear-down right after every shot / ease has been ended and the camera + time scale restored
	 *  (before any actor ends play), so a caller can verify the restoration at that exact moment. */
	FSimpleMulticastDelegate OnWorldTearDownRestored;

	// ------------------------------------------------------------------ time scale (opt-in)
	void SetTimeScalePolicy(EZoogiTimeScalePolicy InPolicy) { Policy = InPolicy; }
	EZoogiTimeScalePolicy GetTimeScalePolicy() const { return Policy; }
	/**
	 * Eases AWorldSettings::CinematicTimeDilation to Spec.Target, holds, and eases back to the exact value it had before the
	 * first ease (overlapping eases keep that original value). Returns false (and changes nothing) when the policy refuses.
	 */
	bool EaseTimeScale(const FZoogiTimeScaleSpec& Spec);
	/** bImmediate puts the prior value back this frame; otherwise eases out from the current value. */
	void StopTimeScale(bool bImmediate = true);
	bool IsTimeScaleActive() const { return TS.bActive; }
	/** True while the running time-scale ease was started by this shot's slow-mo entry. */
	bool IsTimeScaleOwnedBy(FZoogiShotHandle Shot) const { return TS.bActive && Shot.IsValid() && TimeScaleOwnerShotId == Shot.Id; }
	/** Name of an awake simulated body, or empty if all are asleep. */
	FString FindAwakePhysicsBody() const;

	// ------------------------------------------------------------------ verification
	FZoogiCameraSnapshot CaptureGameplaySnapshot(int32 PlayerIndex = 0) const;
	/** True when B matches A within tolerance; OutReport lists the deltas either way. */
	static bool CompareSnapshots(const FZoogiCameraSnapshot& A, const FZoogiCameraSnapshot& B, const FZoogiCameraTolerance& Tol, FString& OutReport);
	/** Snapshot taken before the current MainView takeover (invalid when the main view is not taken). */
	const FZoogiCameraSnapshot& GetMainViewSnapshot() const { return MainViewSnapshot; }

private:
	void HandleWorldBeginTearDown(UWorld* InWorld);
	void TickShot(FZoogiActiveShot& S, float Dt, bool& bOutFinished, EZoogiShotEndReason& OutReason);
	bool EvaluateShotView(FZoogiActiveShot& S, float Dt, FMinimalViewInfo& OutView);
	bool GetLiveGameplayView(float Dt, FMinimalViewInfo& OutView) const;
	FPostProcessSettings BuildShotPostProcess(const FZoogiShotSpec& Spec, float FocusDistance, float Weight) const;
	void FinishShot(int32 Index, EZoogiShotEndReason Reason);
	void TakeMainView(int32 PlayerIndex);
	void RestoreMainView(EZoogiShotEndReason Reason);
	USceneCaptureComponent2D* AcquireCapture();
	void ReleaseCapture(USceneCaptureComponent2D* C);
	void TickTimeScale(float RealDt);
	void SetCinematicDilation(float Value);
	void RestoreTimeScaleExact(const TCHAR* Why);
	float CurveOrEase(UCurveFloat* Curve, EZoogiShotEase Ease, float T) const;
	void TryStartShotSlowMo(FZoogiActiveShot& S);

	UPROPERTY() TObjectPtr<UZoogiShotLibrary> Library = nullptr;
	UPROPERTY() TArray<FZoogiActiveShot> Shots;
	UPROPERTY() TObjectPtr<ACameraActor> ShotCam = nullptr;
	UPROPERTY() TObjectPtr<AActor> CaptureRig = nullptr;
	UPROPERTY() TArray<TObjectPtr<USceneCaptureComponent2D>> FreeCaptures;
	/** Targets of finished shots stay referenced for a while so a compositor can hold the last frame. */
	UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> RecentTargets;
	TMap<int32, TWeakObjectPtr<UTextureRenderTarget2D>> TargetById;

	int32 NextShotId = 1;
	int32 MainViewShotId = 0;
	FZoogiCameraSnapshot MainViewSnapshot;
	FDelegateHandle TearDownHandle;
	bool bTornDown = false;
	/** Shot whose slow-mo entry owns the running time-scale ease (0 = a direct EaseTimeScale caller owns it). */
	int32 TimeScaleOwnerShotId = 0;
	int32 PendingTimeScaleOwner = 0;

	EZoogiTimeScalePolicy Policy = EZoogiTimeScalePolicy::Disabled;
	struct FTimeScaleState
	{
		bool bActive = false;
		FZoogiTimeScaleSpec Spec;
		float Prior = 1.f;      // exact value before the first ease
		float From = 1.f;       // value when the current ease began
		float LastSet = 1.f;
		float Age = 0.f;
		bool bOut = false;      // in the ease-out ramp
		float OutFrom = 1.f;
		float OutAge = 0.f;
	} TS;
};
