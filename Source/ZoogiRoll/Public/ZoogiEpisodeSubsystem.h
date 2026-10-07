#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Camera/CameraModifier.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiEpisodeSubsystem.generated.h"

class AZoogiBall;
class AZoogiOrb;
class AZoogiGameMode;
class AZoogiEpisodeStage;
class UZoogiEpisodeWidget;
class UPrimitiveComponent;

/** Camera nudge / punch for callouts (presentation only; offsets the final view, never the game camera state). */
UCLASS()
class ZOOGIROLL_API UZoogiEpisodeCamMod : public UCameraModifier
{
	GENERATED_BODY()
public:
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	void Nudge(float Strength) { Kick = FMath::Max(Kick * 0.5f, FMath::Clamp(Strength, 0.f, 1.f)); KickAge = 0.f; Seed += 1.37f; }
protected:
	float Kick = 0.f;
	float KickAge = 99.f;
	float Seed = 0.f;
};

/**
 * Zoogi Roll Episode Engine, phase 1 (spec v1.1). World subsystem that
 *  1. captures the gameplay truth of every action (launch -> rest) from real game events (FZoogiGameplayTruthRecord),
 *  2. recognizes it (tags, deterministic drama score, tier 1-5, archetype; FZoogiEpisodeRecognitionRecord),
 *  3. plans and plays presentation (FZoogiCinematicPlan): callouts, glow, centre-frame panels, power escalation,
 *     Titan Meteor.
 *
 * RULE ZERO: presentation never changes physics, results, velocities, scoring or turn order. The only things it does
 * to the simulation are (a) CinematicTimeDilation slow-mo, restored to the exact previous value, and (b) a
 * presentation freeze (game paused; game clock stops, so turn timing is unaffected) with a before / after snapshot of
 * every ball and orb that proves nothing moved. Every action logs one EPISODE line.
 *
 * Command line: -ZoogiEpisode=Full|Balanced|Competitive|Off (default Balanced, or [ZoogiRoll.Episode] Mode),
 * -ZoogiEpisodeTest=A|B|E|F (Titan Meteor test director, classic mode, P1 turn 1), -ZoogiEpisodeShots=<dir>.
 */
UCLASS()
class ZOOGIROLL_API UZoogiEpisodeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** GameMode hook (FireShot): a launch starts a new action. */
	void OnShotFired(AZoogiBall* Ball);

	UFUNCTION() void OnBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	EZoogiEpisodeMode GetMode() const { return Mode; }
	bool IsFrozen() const { return bFrozen; }
	const FZoogiGameplayTruthRecord& GetTruth() const { return Truth; }

	/** Recognition from a (possibly partial) truth record. Deterministic, used live and at the end of the action. */
	static FZoogiEpisodeRecognitionRecord Recognize(const FZoogiGameplayTruthRecord& T, const FZoogiEpisodeTuning& Tune);

protected:
	AZoogiGameMode* GM() const;
	void BindBodies();
	int32 BodyCode(const AActor* A) const;
	AActor* BodyActor(int32 Code) const;
	FVector BodyVelocity(int32 Code) const;
	void BeginAction(AZoogiBall* Ball);
	void TrackAction(float GameDt);
	void FinalizeAction(const TCHAR* Why);
	void SnapshotStart();
	int32 PieceSteps() const;
	int32 RockSteps() const;

	// presentation
	void EnsurePresentation();
	void UpdateLive();
	void PresentTier(int32 Tier, const FZoogiEpisodeRecognitionRecord& R);
	void QueueCallout(EZoogiEpisodeTag Tag, int32 Tier);
	void PumpCallouts();
	void StartPanels(int32 Tier, const FZoogiEpisodeRecognitionRecord& R);
	void ShowEpilogue();
	void UpdatePanelCameras(float RealDt);
	void StartSlowMo();
	void StopSlowMo();
	void TickPresentation(float RealDt);
	void Nudge(float Strength);
	void Shot(const FString& Name);
	int32 PresentCap() const;

	// freeze + meteor
	void FreezeWorld();
	void UnfreezeWorld();
	void TryStartMeteor();
	void TickMeteor(float RealDt);
	void EndMeteor();

	// test director (ZoogiEpisodeTest.cpp)
	void TickTest();
	void TestFinalize();

	FZoogiEpisodeTuning Tune;
	EZoogiEpisodeMode Mode = EZoogiEpisodeMode::Balanced;
	bool bModeExplicit = false;
	FString ShotDir;
	TMap<FString, int32> ShotCounts;

	// ---- action capture
	bool bActive = false;
	bool bSawResolve = false;
	int32 NextActionId = 1;
	float ActionStartGame = 0.f;
	FZoogiGameplayTruthRecord Truth;
	FZoogiEpisodeRecognitionRecord LiveRec;
	FZoogiCinematicPlan Plan;
	TArray<int32> StartScore, StartMarks;
	TArray<bool> StartOut, StartOrbOut;
	int32 StartResonance = 0;
	float StartControl = 0.f;
	int32 StartPieceSteps = 0, StartRockSteps = 0;
	bool bPrevInfusedContact = false;
	int32 ShooterFirstContact = -1;     // body code of the shooter's first zoogi / orb contact
	TSet<int32> OutCounted;
	TMap<int32, int32> DepthMap;
	TMap<uint64, float> PairLast;
	TMap<int32, FVector> PrevVel;
	struct FAirTrack { bool bAir = false; float Start = 0.f; float Peak = 0.f; };
	TMap<int32, FAirTrack> Air;
	TMap<int32, int32> OpenBoundary;    // body -> index in Truth.Boundary
	TSet<UPrimitiveComponent*> Bound;

	// ---- live presentation
	int32 PresentedTier = 0;
	TSet<EZoogiEpisodeTag> Announced;
	struct FCallout { FString Main; FString Sub; FLinearColor Color; int32 Tier = 1; };
	TArray<FCallout> CalloutQueue;
	double RealClock = 0.0;
	double NextCalloutAt = 0.0;
	TMap<EZoogiEpisodeTag, int32> LastVariant;
	FString LastCalloutText;
	EZoogiPanelLayout LastLayout = EZoogiPanelLayout::None;
	TArray<FString> RecentPresent;      // "layout" history (recency)
	bool bPanels = false;
	int32 PanelCount = 0;
	float PanelAge = 0.f;
	TArray<int32> PanelSubject;         // body code (or -1 = wide) per panel
	bool bPowerShown = false;
	float PowerLeft = 0.f;
	bool bSlowMo = false;
	float SlowMoLeft = 0.f;
	float SavedCinematic = 1.f;
	float AppliedCinematic = 1.f;
	bool bEpilogueShown = false;

	UPROPERTY() TObjectPtr<UZoogiEpisodeWidget> Widget;
	UPROPERTY() TObjectPtr<AZoogiEpisodeStage> Stage;
	UPROPERTY() TObjectPtr<UZoogiEpisodeCamMod> CamMod;

	// ---- freeze
	bool bFrozen = false;
	struct FBodySnap { TWeakObjectPtr<UPrimitiveComponent> Comp; FTransform Xf; FVector Lin = FVector::ZeroVector; FVector Ang = FVector::ZeroVector; };
	TArray<FBodySnap> Snap;
	TWeakObjectPtr<AActor> PrevView;

	// ---- meteor
	bool bMeteorPending = false;
	bool bMeteor = false;
	float MeteorT = 0.f;
	float MeteorDur = 2.6f;
	float MeteorHitGame = 0.f;
	bool bMeteorImpactShot = false;
	TWeakObjectPtr<AZoogiBall> MeteorTarget;
	TWeakObjectPtr<AZoogiBall> MeteorShooter;

	// ---- test director
	FString TestVariant;
	int32 TestStep = 0;
	int32 TestActionId = -1;
	int32 TestTarget = 1;
	double TestQuitAt = 0.0;
	FVector TestAimPoint = FVector::ZeroVector;
	float TestStepAt = 0.f;
	bool bTestMeteorShown = false;
	FString TestDepicted;
};
