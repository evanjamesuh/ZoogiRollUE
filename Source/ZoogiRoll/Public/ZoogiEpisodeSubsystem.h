#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiEpisodeSubsystem.generated.h"

class AZoogiBall;
class AZoogiOrb;
class AZoogiGameMode;
class UZoogiEpisodeCalloutWidget;
class UPrimitiveComponent;

/** A presentation request (see FZoogiEpisodeCue). Receivers render it; they never touch gameplay state. */
DECLARE_MULTICAST_DELEGATE_OneParam(FZoogiEpisodeCueEvent, const FZoogiEpisodeCue& /*Cue*/);
/** The finished action: truth, recognition, and what presentation was requested / held for it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FZoogiEpisodeActionStart, const FZoogiGameplayTruthRecord& /*Truth*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FZoogiEpisodeActionResolving, const FZoogiGameplayTruthRecord& /*Truth*/, const FZoogiEpisodeRecognitionRecord& /*Live*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FZoogiEpisodeAirborne, int32 /*Body*/, float /*PeakGap*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FZoogiEpisodeActionEvent, const FZoogiGameplayTruthRecord& /*Truth*/, const FZoogiEpisodeRecognitionRecord& /*Rec*/, const FZoogiCinematicPlan& /*Plan*/);

/**
 * Zoogi Roll Episode Engine core, phase 1 (spec v1.1 priorities 1-4, 12, 13). World subsystem that
 *  1. captures the gameplay truth of every action (launch -> rest) from real game events (FZoogiGameplayTruthRecord),
 *  2. tags it, scores it deterministically and selects tier 1-5 + archetype (FZoogiEpisodeRecognitionRecord),
 *  3. applies the presentation mode (Off / Competitive / Balanced / Full) and recency (no back-to-back callout variant
 *     or panel layout), and
 *  4. sends presentation CUES to the pieces that render them. It does not render panels, camera shots, slow-mo or
 *     power visuals itself:
 *       OnPanels    -> Gameplay & UI: UZoogiEpisodePanelWidget::PlayPanels(TArray<FZoogiPanelShot>, TotalSeconds)
 *       OnCamera    -> Game Camera: UZoogiEpisodeCameraDirector (shot spec on Actor / Target, eased time dilation,
 *                      exact restore of the gameplay camera and dilation)
 *       OnPower     -> Animations & Powers: power visual grammar (tier 4+)
 *       OnArchetype -> Animations & Powers: AZoogiTitanMeteorFX::Play(Start, Impact, Seconds), played inside a
 *                      presentation HOLD owned by this subsystem (see BeginHold)
 *       OnTier / OnCallout / OnEpilogue -> anyone (a minimal fallback text overlay shows callouts / epilogue while
 *                      nothing is bound to those two)
 *     Bind from anywhere: GetWorld()->GetSubsystem<UZoogiEpisodeSubsystem>()->OnPanels.AddUObject(this, &X::Handle).
 *     A cue whose delegate has no receiver is logged ("EPISODE cue ... receivers=0") and otherwise skipped.
 *
 * RULE ZERO: nothing here (or in a receiver) may change physics, results, velocities, scoring or turn order.
 * The core enforces / proves it: the hold snapshots every ball + orb and verifies (and restores) them on release, the
 * time dilation in effect at launch must be back at the end of the action, and every action logs one EPISODE line
 * with result_unchanged=1|0.
 *
 * Command line: -ZoogiEpisode=Full|Balanced|Competitive|Off (default Balanced, or [ZoogiRoll.Episode] Mode),
 * -ZoogiEpisodeTest=A|B|E|F|S (Titan / power test director, classic mode, P1 turn 1), -ZoogiEpisodeShots=<dir>.
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

	// ---- integration hooks (presentation receivers bind here)
	FZoogiEpisodeCueEvent OnTier;        // every live tier step 1..5 (cue.Tier); tier 2 carries the used Surfaces (glow)
	FZoogiEpisodeCueEvent OnCallout;     // every newly recognized tag, recency-picked text variant
	FZoogiEpisodeCueEvent OnPanels;      // tier 3+ (not Competitive / Off): layout + count + seconds, recency-checked
	FZoogiEpisodeCueEvent OnCamera;      // tier 3+ (not Competitive / Off): suggested shot + slow-mo (receiver restores exactly)
	FZoogiEpisodeCueEvent OnPower;       // tier 4+: power escalation (cue.Power / PowerColor)
	FZoogiEpisodeCueEvent OnArchetype;   // Titan Meteor: world is held for cue.Seconds (cue.HoldToken)
	FZoogiEpisodeCueEvent OnEpilogue;    // outcome confirmed by Resolve (real result only)
	FZoogiEpisodeActionEvent OnActionFinalized;
	// battle engine (UZoogiBattleStateMachine) hooks; fire in every mode, Off included
	FZoogiEpisodeActionStart OnActionStarted;          // inside FireShot, right after the launch (aim / lock still valid)
	FZoogiEpisodeActionResolving OnActionResolving;    // Resolve committed the result, before the epilogue
	FZoogiEpisodeAirborne OnAirborneLive;              // once per body per action, while it is still in the air
	/** Battle shatter: no epilogue / holds / air shot for the rest of the current action. */
	void SuppressPresentation(const TCHAR* Why);
	bool IsPresentationSuppressed() const { return bPresentationSuppressed; }
	/** A gameplay reaction owns the airborne moment: the presentation air shot yields for this action. */
	void ClaimAirborne() { bAirClaimed = true; }

	/**
	 * Presentation hold: pauses the game (the game clock stops, so turn timing is unaffected) after snapshotting every
	 * ball + orb. Released after MaxSeconds of real time or by ReleaseHold(Token), whichever comes first; on release the
	 * bodies are verified (and restored exactly if anything moved them). While held the subsystem drives the player
	 * camera manager each real frame, so SetViewTarget / camera modifiers keep working. Returns 0 if a hold is already
	 * active, the mode is Off, or the world cannot be held.
	 */
	int32 BeginHold(float MaxSeconds, const TCHAR* Reason, bool bGameplay = false); // bGameplay: also in Off mode (Air Reaction)
	void ReleaseHold(int32 Token);
	bool IsHeld() const { return bFrozen; }

	EZoogiEpisodeMode GetMode() const { return Mode; }
	const FZoogiGameplayTruthRecord& GetTruth() const { return Truth; }
	const FZoogiEpisodeRecognitionRecord& GetLiveRecognition() const { return LiveRec; }
	const FZoogiCinematicPlan& GetPlan() const { return Plan; }
	const FZoogiEpisodeTuning& GetTuning() const { return Tune; }
	bool IsActionActive() const { return bActive; }

	/** Recognition from a (possibly partial) truth record. Deterministic, used live and at the end of the action. */
	static FZoogiEpisodeRecognitionRecord Recognize(const FZoogiGameplayTruthRecord& T, const FZoogiEpisodeTuning& Tune);

	friend struct FZoogiSaveAccess; // v3 P6 save / resume
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

	// cue planning (ZoogiEpisodePresent.cpp)
	FZoogiEpisodeCue MakeCue(EZoogiEpisodeCueKind Kind, int32 Tier) const;
	void SendCue(const FZoogiEpisodeCueEvent& Event, FZoogiEpisodeCue& Cue);
	void EnsureFallback();
	void UpdateLive();
	void PresentTier(int32 Tier, const FZoogiEpisodeRecognitionRecord& R);
	void QueueCallout(EZoogiEpisodeTag Tag, int32 Tier);
	void PumpCallouts();
	void RequestPanels(int32 Tier);
	void ShowEpilogue();
	void TryStartArchetype();
	void TickPresentation(float RealDt);
	void Shot(const FString& Name);
	void ShotLater(const FString& Name, float Delay);
	TArray<TPair<FString, double>> LaterShots;
	int32 PresentCap() const;
	void FreezeWorld();
	void UnfreezeWorld();

	// shot_id selection (ZoogiEpisodeShotPick.cpp): spec section 15 library, recency-weighted
	void PickShots(FZoogiEpisodeCue& Cue);
	int32 SubjectBody(EZoogiShotSubject S) const;
	void NoteShots(const FZoogiEpisodeCue& Cue);
	TArray<FName> RecentShots;          // newest last, capped (recency weighting)

	// tier 1-3 fallback presentation (only while no receiver is bound): inserts / panels via the Game Camera director,
	// contact glow, airborne tracking shot with stepped slow-mo (full-size physics steps on a fraction of frames)
	void FallbackTier(int32 Tier);
	bool FallbackPanel(int32 Slot, AActor* Focus, AActor* Secondary, uint8 ShotType, float Seconds, const FString& Caption, const FLinearColor& Frame, bool bSmall);
	void FallbackGlow(int32 MaxN);
	void StartAirShot(int32 Body, float Gap);
	void TickAirSlow(float RealDt);
	void EndAirSlow(const TCHAR* Why);
	void BindDirector();
	TMap<int32, float> NearMin;         // near-miss candidates: body -> smallest gap so far
	TSet<int32> NearTouched;
	bool bAirShotDone = false;
	bool bAirClaimed = false;
	bool bPresentationSuppressed = false;
	TSet<int32> AirAnnounced;
	bool bAirSlow = false;
	int32 AirBody = -1; int32 AirHoldToken = 0; bool bStepPaused = false;
	float AirSlowLeft = 0.f;
	float AirAcc = 0.f;
	int32 AirShotId = 0;
	TWeakObjectPtr<AActor> AirPrevView;
	bool bDirectorBound = false;
	TArray<FZoogiEpisodeShot> LastTierShots;

	// test director (ZoogiEpisodeTest.cpp)
	void TickTest();
	void TestFinalize();
	void TestSetupSimple();
	bool TestSimple() const;
	void TestPickSimpleAim();

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
	float StartCinematicDilation = 1.f, StartGlobalDilation = 1.f;
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

	// ---- live cue state + recency
	int32 PresentedTier = 0;
	TSet<EZoogiEpisodeTag> Announced;
	TArray<FZoogiEpisodeCue> CalloutQueue;
	double RealClock = 0.0;
	double NextCalloutAt = 0.0;
	TMap<EZoogiEpisodeTag, int32> LastVariant;   // recency: callout variant per tag
	FString LastCalloutText;                      // recency: never the same callout twice in a row
	EZoogiPanelLayout LastLayout = EZoogiPanelLayout::None; // recency: never the same panel layout twice in a row
	bool bPowerSent = false;
	bool bEpilogueShown = false;
	UPROPERTY() TObjectPtr<UZoogiEpisodeCalloutWidget> Fallback;

	// ---- hold (presentation freeze)
	bool bFrozen = false;
	int32 HoldToken = 0;
	int32 NextHoldToken = 1;
	float HoldLeft = 0.f;
	FString HoldReason;
	struct FBodySnap { TWeakObjectPtr<UPrimitiveComponent> Comp; FTransform Xf; FVector Lin = FVector::ZeroVector; FVector Ang = FVector::ZeroVector; };
	TArray<FBodySnap> Snap;

	// ---- archetype (Titan Meteor) trigger
	bool bArchetypePending = false;
	float ArchetypeHitGame = 0.f;
	TWeakObjectPtr<AZoogiBall> ArchetypeTarget;
	TWeakObjectPtr<AZoogiBall> ArchetypeShooter;

	// ---- test director
	FString TestVariant;
	int32 TestStep = 0;
	int32 TestActionId = -1;
	int32 TestTarget = 1;
	double TestQuitAt = 0.0;
	FVector TestAimPoint = FVector::ZeroVector;
	float TestStepAt = 0.f;
};
