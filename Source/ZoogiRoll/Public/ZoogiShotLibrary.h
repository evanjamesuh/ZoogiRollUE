// Episode Engine: reusable camera shot library (spec v1.1 sections 4, 15, 18; priority 5).
// Data-driven shot specs that the camera director (ZoogiEpisodeCameraDirector.h) plays on a focus actor and an optional
// secondary target, either into a render target for the panel compositor or temporarily on the main view.
// Presentation only: nothing here reads or writes gameplay / physics state (Rule Zero).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Camera/CameraTypes.h"
#include "ZoogiShotLibrary.generated.h"

class UCurveFloat;

/** Shot families. Custom = use the spec's numbers as-is (no family defaults). */
UENUM(BlueprintType)
enum class EZoogiShotType : uint8
{
	CloseUp,
	LowAngleHero,
	TopDownImpact,
	Tracking,
	PartnerTwoShot,
	Custom
};

/** Where a shot is rendered. RenderTarget is the primary path (panels); MainView temporarily takes the player's view. */
UENUM(BlueprintType)
enum class EZoogiShotOutput : uint8
{
	RenderTarget,
	MainView
};

UENUM(BlueprintType)
enum class EZoogiShotEase : uint8
{
	Linear,
	SmoothStep,
	CubicInOut,
	ExpoOut
};

/** What the shot's yaw is measured from. */
UENUM(BlueprintType)
enum class EZoogiShotHeading : uint8
{
	/** Yaw of the gameplay camera when the shot starts (YawOffset 0 = same side as the player's view). */
	GameplayCamera,
	/** Yaw of the line focus -> secondary (falls back to GameplayCamera without a secondary). */
	FocusToSecondary,
	/** Yaw of the focus actor's velocity, smoothed (tracking shots; falls back to GameplayCamera when slow). */
	FocusVelocity,
	/** World yaw 0. */
	World
};

/**
 * One reusable shot. Geometry: Pivot = focus centre + PivotOffset (two-shots: lerp(focus, secondary, SecondaryBias));
 * camera = Pivot + spherical(Yaw, Pitch, Distance) where Yaw = heading + YawOffset + OrbitDegrees * motion;
 * the camera looks at LookPoint = Pivot + LookOffset. Distance / Pitch / FOV interpolate from start to End over the shot.
 */
USTRUCT(BlueprintType)
struct ZOOGIROLL_API FZoogiShotSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") FName Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") EZoogiShotType Type = EZoogiShotType::Custom;

	// ---- timing (real seconds; presentation time is independent of time dilation)
	/** Whole shot length including ease in / out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.1")) float Duration = 2.0f;
	/** MainView: blend from the gameplay view into the shot. RenderTarget: ignored (the panel owns its own transition). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0")) float EaseIn = 0.35f;
	/** MainView: blend from the shot back to the live gameplay view (ends exactly on it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0")) float EaseOut = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing") EZoogiShotEase Ease = EZoogiShotEase::SmoothStep;
	/** Optional: motion progress curve over normalized shot time (0..1 -> 0..1). Overrides Ease for the camera move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing") TObjectPtr<UCurveFloat> MotionCurve = nullptr;

	// ---- framing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") EZoogiShotHeading Heading = EZoogiShotHeading::GameplayCamera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float YawOffset = 0.f;
	/** Extra yaw swept over the shot (orbit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float OrbitDegrees = 0.f;
	/** Degrees above the pivot (negative = below, low angle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float Pitch = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float PitchEnd = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "10")) float Distance = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "10")) float DistanceEnd = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "5", ClampMax = "170")) float FOV = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "5", ClampMax = "170")) float FOVEnd = 45.f;
	/** Dutch roll, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float Roll = 0.f;
	/** World-space offset of the pivot from the focus centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") FVector PivotOffset = FVector::ZeroVector;
	/** World-space offset of the look point from the pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") FVector LookOffset = FVector(0, 0, 15.f);
	/** Two-shots: pivot position between focus (0) and secondary (1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "0", ClampMax = "1")) float SecondaryBias = 0.f;
	/** Widen the FOV (never narrow it) so focus and secondary both fit, with FramePadding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") bool bFrameBoth = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "1")) float FramePadding = 1.35f;
	/** Radius (cm) assumed around each framed actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float SubjectRadius = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") float MaxFOV = 100.f;
	/** Pivot follow damping (1/s). 0 = rigidly locked to the focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing", meta = (ClampMin = "0")) float TrackingLag = 0.f;
	/** Pull the camera in front of static geometry between the look point and the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Framing") bool bAvoidOcclusion = true;

	// ---- look (subtle grade on top of the gameplay camera's post-process; no toon shading)
	/** Start from the gameplay camera's post-process so panels match the game look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") bool bInheritGameplayPostProcess = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float VignetteAdd = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float SaturationMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ContrastMul = 1.f;
};

/** A set of shot specs (create a data asset of this class to tune shots without code). */
UCLASS(BlueprintType)
class ZOOGIROLL_API UZoogiShotLibrary : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shots") TArray<FZoogiShotSpec> Shots;

	/** First spec of this family, or nullptr. */
	const FZoogiShotSpec* FindByType(EZoogiShotType Type) const;
	const FZoogiShotSpec* FindByName(FName Name) const;

	/** Built-in tuning for each family (close-up, low-angle hero, top-down impact, tracking, partner two-shot). */
	static FZoogiShotSpec MakeDefault(EZoogiShotType Type);
	/** Fills Shots with MakeDefault for every family. */
	void FillDefaults();
};

namespace ZoogiShot
{
	ZOOGIROLL_API float ApplyEase(EZoogiShotEase Ease, float T);
	ZOOGIROLL_API const TCHAR* TypeName(EZoogiShotType Type);
	ZOOGIROLL_API const TCHAR* OutputName(EZoogiShotOutput Output);
}
