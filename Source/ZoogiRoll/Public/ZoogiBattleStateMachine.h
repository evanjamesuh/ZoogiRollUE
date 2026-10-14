// Battle Episode Engine (spec 2026-10-08, T1/T2): the single authoritative battle state machine.
//
//   WAIT_FOR_COMMAND -> ACTION_PREVIEW -> [MINIGAME] -> PHYSICS_RESOLVE -> [MINIGAME (Air Reaction)] -> RESULT
//        -> EPISODE_EVENT -> NEXT_TURN -> WAIT_FOR_COMMAND ...                                       -> MATCH_END
//   a failed challenge:  ... -> SHATTER_FAILURE -> NEXT_TURN (no consolation card / scene / hold in between)
//
// The existing turn timers (ZoogiTargeting.cpp TickStages) keep running the game; this subsystem observes them every
// frame, validates every transition (BATTLE state <from> -> <to> reason=...), owns per-player battle state, Episode
// Energy, Awakening, combos / challenges and the shatter path. With [ZoogiRoll.Battle] Enabled=false (default) and no
// -ZoogiBattle=On it only observes + logs: existing play is unchanged.
//
// RULE ZERO: nothing here changes physics outcomes. The only physics-changing feature is the Air Reaction (a gameplay
// action by design, T3), offered only to an Awakened Zoogi with the flow on.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiAerialProps.h"
#include "ZoogiBattleStateMachine.generated.h"

class AZoogiGameMode;
class AZoogiBall;
class UZoogiEpisodeSubsystem;
class UZoogiEpisodeEnergyComponent;
class UZoogiReactionComponent;
class UZoogiMiniGame;
class UZoogiEpisodeLedger;
class UZoogiEpisodeAssembler;
class AZoogiAerialDecor;
class AHUD;
class UCanvas;

/** Oct 8: a CPU turn's prop plan (Frozen map layout): what the CPU aimed at and which combo / rail window goes with it. */
/** Oct 8 (Evan 9:26 PM): per-Zoogi peak height during one action (ZoogiAerialReach.cpp). Logging / tests only. */
struct FZoogiPeakTrack
{
	float PeakZ = -1.f;          // ball centre z (floor = 0, a resting Zoogi = ZoogiRadius)
	FString Cause = TEXT("floor");
	FString PeakCause = TEXT("floor");
	FVector PeakAt = FVector::ZeroVector;
	bool bAirSinceCause = false; // left the floor since the cause was set (landing again resets it to floor)
	bool bWentOut = false;       // rang out during the action: its flight is a ring-out ricochet, not a usable launch
	float LastZ = -1.f;
};

struct FZoogiCPUPropPlan
{
	int32 Player = -1;
	int32 Turn = -1;
	EZoogiPropKind Kind = EZoogiPropKind::SkyBumper;
	FName Combo;
	TWeakObjectPtr<AZoogiAerialProp> Prop;
	TWeakObjectPtr<AActor> Target;
	EZoogiRailWindow RailWindow = EZoogiRailWindow::Center;
	FString Desc;
	bool IsFor(int32 InPlayer, int32 InTurn) const { return Player == InPlayer && Turn == InTurn && Player >= 0; }
};

UCLASS()
class ZOOGIROLL_API UZoogiBattleStateMachine : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	static UZoogiBattleStateMachine* Get(const UObject* Ctx);
	/** The new battle flow is on ([ZoogiRoll.Battle] Enabled, -ZoogiBattle=On, or any -ZoogiBattleTest). */
	static bool FlowOn(const UObject* Ctx);

	// ---- small hooks called from the game (ZoogiAwaken.cpp / ZoogiCombo.cpp). Flow off: the old behaviour exactly.
	/** ZoogiAwaken.cpp DoResolve: may the "MISSED..." anime card play? (flow on: only with bMissConsolationCard) */
	static bool AllowMissCard(const UObject* Ctx);
	/** Soft couplings (flow on: [ZoogiRoll.EpisodeEnergy] flags; flow off: always true). */
	static bool AllowStarOrbDraw(const UObject* Ctx);
	static bool AllowResonanceDraws(const UObject* Ctx);
	static bool AllowArenaControlGauge(const UObject* Ctx);
	/** 2v2 Partner Follow-Up cost source (TeamGauge default; EpisodeEnergy = the partner's own energy). */
	static bool CanAffordFollowUp(const UObject* Ctx, int32 Partner, float TeamGauge, float Cost);
	/** Returns true if the cost was paid from Episode Energy (the caller then skips the team gauge). */
	static bool PayFollowUpFromEnergy(const UObject* Ctx, int32 Partner, float Cost);
	/** A human Partner Follow-Up offer older than FollowUpOfferTimeout is declined (flow on). */
	static bool FollowUpOfferTimedOut(const UObject* Ctx, float OfferAge);

	// ---- state
	EZoogiBattleState GetState() const { return State; }
	static bool IsValidTransition(EZoogiBattleState From, EZoogiBattleState To);
	/** Validated transition. An invalid one is still applied (the game is the authority) but logged + counted. */
	bool Transition(EZoogiBattleState To, const FString& Reason);
	bool IsFlowOn() const { return bFlow; }
	const FZoogiBattleTuning& GetTuning() const { return Tune; }
	FZoogiBattleTuning& MutableTuning() { return Tune; }

	// ---- players / energy / awakening
	TArray<FZoogiBattlePlayerState> Players;
	UZoogiEpisodeEnergyComponent* Energy(int32 Player) const;
	UZoogiReactionComponent* Reaction(int32 Player) const;
	float GetEnergy(int32 Player) const;
	void AddEnergy(int32 Player, float Amount, FName Source, int32 ActionId);
	bool SpendEnergy(int32 Player, float Amount, FName Why);
	bool IsAwakened(int32 Player) const;

	// ---- combos / challenges (commands)
	/** Commits a combo for Player's next launch (pays the cost if bSpendOnSelect). */
	bool SelectCombo(int32 Player, FName ComboId, const FString& Why);
	void CancelCombo(const FString& Why);
	bool CanAffordCombo(int32 Player, const FZoogiEpisodeComboRow& C) const;
	const FZoogiPendingSegment& GetPending() const { return Pending; }
	/** FAIL -> SHATTER -> NEXT_PLAYER: cancels the pending segment, skips cards / scenes / holds, ends the turn now. */
	void Shatter(const FString& Why);

	// ---- T3 mini-games
	UPROPERTY() TObjectPtr<UZoogiMiniGame> ActiveMiniGame;
	/** Air Reaction choice from the HUD / debug keys / test director (only while an Air Reaction is taking input). */
	bool SubmitAirChoice(EZoogiAirChoice Choice, const FString& Source);
	bool IsAirReactionOpen() const;

	// ---- T3 aerial props / zones / Rail Release (ZoogiAerialPlay.cpp). Off unless bAerialProps / -ZoogiBattleProps / a test.
	UPROPERTY() TArray<TObjectPtr<AZoogiAerialProp>> Props;
	AZoogiAerialProp* SpawnProp(EZoogiPropKind Kind, const FVector& Location, const FVector& Forward, float Size = -1.f, bool bAerialRing = false, FName Tag = NAME_None);
	void ClearProps();
	/** A prop reports an event for a Zoogi: bumper (rebound applied), rim, pass, rail_enter. */
	void OnPropEvent(AZoogiAerialProp* Prop, AZoogiBall* Ball, FName Event, const FVector& Where, float SpeedIn, float SpeedOut);
	UFUNCTION() void OnBallHitProp(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	/** Debug zone tag of a point (ref frozen_zones_aerial): Zone 1 court / Zone 2 aerial (z) / Zone 3 recovery (outer ring). */
	EZoogiZone ZoneOf(const FVector& P) const;
	/** Rail Release input ("release now") from the debug key / test director. */
	bool SubmitRailRelease(const FString& Source);
	bool IsRailOpen() const;
	int32 HaloPasses = 0, HaloRims = 0, BumperRebounds = 0, RailEntries = 0, RailReleases = 0, RailTimeouts = 0;
	EZoogiRailWindow LastRailWindow = EZoogiRailWindow::None;
	FVector LastRailVelocity = FVector::ZeroVector;
	float LastRailEntrySpeed = 0.f;
	float BumperPostSpeed = 0.f;          // the Zoogi's 2D speed two frames after a Sky Bumper rebound (the sim carried it)
	TWeakObjectPtr<AZoogiBall> BumperCheckBall;
	int32 BumperCheckFrames = 0;

	// ---- Oct 8: Frozen map layout in normal play, CPU prop plans, in-game battle HUD (ZoogiAerialPlay.cpp / ZoogiBattleHUD.cpp)
	/** Map-layout props wanted now: flow on, bAerialProps, the arena in AerialArenas, not 2v2 combo; tests only map_* cases. */
	bool WantsMapProps() const;
	bool HasMapLayout() const { return bMapLayout; }
	/** GameMode PlanCPUShot hook (after the aim error): may replace the shot with a rail / halo-pass / bumper-bank plan. Uses
	 *  the GameMode's GameRng only when map props exist (other arenas: no draw, behaviour identical). */
	bool PlanCPUProp(int32 Player, FVector2D& Dir, float& Frac, TWeakObjectPtr<AActor>& Lock);
	/** Plans one prop shot for Player (Kind) without the chance roll (tests / CPU). False if not feasible. */
	bool ComputePropPlan(int32 Player, EZoogiPropKind Kind, FVector2D& OutDir, float& OutFrac, AActor*& OutTarget, AZoogiAerialProp*& OutProp, FString& OutDesc) const;
	FZoogiCPUPropPlan CPUPlan;
	int32 CPUPropPlans = 0, CPUPropRolls = 0, GatePasses = 0;
	TMap<FString, int32> CPUPlanKinds;
	UPROPERTY() TObjectPtr<AZoogiAerialDecor> Decor;

	// ---- T4 ledger / assembler
	UPROPERTY() TObjectPtr<UZoogiEpisodeLedger> Ledger;
	UPROPERTY() TObjectPtr<UZoogiEpisodeAssembler> Assembler;

	AZoogiGameMode* GetGame() const { return GM(); }
	UZoogiEpisodeSubsystem* GetEpisode() const { return Episode(); }
	FString BodyLabel(int32 Code) const;
	/** Air Reaction entitlement: flow on, Awakened, a reaction left (never Episode Energy). */
	bool CanOfferAir(int32 Player, FString* OutWhy = nullptr) const;
	const TArray<FZoogiPendingSegment>& GetHistory() const { return History; }

	// ---- diagnostics (tests)
	int32 AirOffersSeen = 0;
	int32 AirOffersAllowed = 0;
	TArray<int32> AirOfferedTo;
	TArray<FString> StateTrail;     // "FROM>TO:reason"
	int32 InvalidTransitions = 0;
	int32 ShatterCount = 0;
	int32 SuppressedMissCards = 0;
	int32 FollowUpTimeouts = 0;
	int32 LastShatterAction = -1;
	FString LastShatterReason;
	double LastShatterReal = -100.0;
	FString TestCase;

protected:
	AZoogiGameMode* GM() const;
	UZoogiEpisodeSubsystem* Episode() const;
	void EnsurePlayers();
	void Observe();
	void OnAwakenObserved();
	void OnTurnStarted(int32 Player, const FString& Why);
	void DebugInput();
	void DebugOverlay();
	int32 BodyCodeOf(const AActor* A) const;
	/** Records a failed segment as SHATTERED (ledger + counters + debug flash). Does not change the turn. */
	void RecordShatter(FZoogiPendingSegment Seg, const FString& Why);

	// episode subsystem events
	void HandleActionStarted(const FZoogiGameplayTruthRecord& Truth);
	void HandleActionResolving(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec);
	void HandleActionFinalized(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, const FZoogiCinematicPlan& Plan);
	void HandleAirborne(int32 Body, float Peak);
	/** Evaluates the pending challenge against the truth at Resolve. Idempotent per action. */
	void EvaluateAtResolve(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec);
	EZoogiChallengeResult EvaluateChallenge(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, FString& OutDetail) const;
	void GrantEnergyFromTruth(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec);
	void PickCPUCombo(int32 Player);

	// T3 (ZoogiMiniGame.cpp)
	void TickMiniGame(float RealDt);
	void FinishMiniGame(EZoogiChallengeResult R, const FString& Detail);

	// T3 aerial (ZoogiAerialPlay.cpp)
	void TickProps();
	void SpawnDefaultProps();
	void StartRail(AZoogiAerialProp* Rail, AZoogiBall* Ball);
	void TickRail(class UZoogiMiniGameRailRelease* M);
	void FinishRail(class UZoogiMiniGameRailRelease* M, EZoogiChallengeResult R, const FString& Why);
	void DrawZones() const;
	void SpawnMapLayout();
	void ClearMapLayout(const TCHAR* Why);
	/** Engine-viewport screenshot (+HUD) of an aerial moment (-ZoogiAerialShots=<dir> only; max 2 per name). */
	void AerialShot(const FString& Name, float Delay = 0.f);
	void TickAerialShots();
	FString AerialShotDir;
	TArray<TPair<FString, double>> AerialLater;
	TMap<FString, int32> AerialShotCounts;
	uint8 MapTheme = 0;                    // EZoogiArena of the arena the map layout was built for
	bool bMapLayout = false;
	TWeakObjectPtr<class AZoogiArena> MapArena;
	TSet<TWeakObjectPtr<AZoogiBall>> PropBoundBalls;
	TMap<int64, FVector> PropPrevLocal;   // (prop, ball) -> previous ball position in the trigger frame
	bool bDefaultPropsSpawned = false;
	double ActionStartGame = 0.0;
	TArray<FString> ActionZoneTags;
	TArray<FString> ActionPropTags;

	// Oct 8 in-game battle HUD (ZoogiBattleHUD.cpp): drawn through AHUD::OnHUDPostRender (ZoogiHUD untouched)
	void OnHUDPostRender(AHUD* HUD, UCanvas* Canvas);
	FDelegateHandle hHUD;
	int32 HudSeenHistory = 0, HudSeenRail = 0;
	double HudFlashUntil = 0.0, HudRailUntil = 0.0;
	FString HudFlashText, HudRailText;
	FLinearColor HudFlashColor = FLinearColor::White;

	// T4 (ZoogiEpisodeLedger.cpp)
	void LedgerAction(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, const FZoogiCinematicPlan& Plan);
	void LedgerSimple(const FString& Kind, int32 Actor, const FString& Detail);
	void OnMatchEnd(const FString& Why);

	// T5 (ZoogiBattleTest.cpp)
	void TickTest(float RealDt);
	void TestOnFinalized(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, const FZoogiCinematicPlan& Plan);
	void TestOnMatchEnd();
	void TestReport(bool bPass, const FString& Detail);
	bool TestLayout(int32 Shooter, int32 Target, int32 Variant);
	/** map_* cases: waits for the Frozen map layout (and P1's first turn), places P1 where the forced prop route is feasible. */
	bool TestMapSetup();
	int32 TestForceProp = -1;                                   // map_* cases: CPU prop plans only of this EZoogiPropKind
	EZoogiRailWindow TestForceWindow = EZoogiRailWindow::None;  // map_rail: the CPU's release window

public:
	// ---- Oct 8 (Evan 9:26 PM): aerial reachability (ZoogiAerialReach.cpp). The peak heights Zoogis reach in play (per launch
	// type: rail_early / rail_center / rail_late / ramp / bumper / floor) are logged per action ("BATTLE peak") and the measured
	// envelope ([ZoogiRoll.Aerial] ReachPeaks) caps the Zone 2 prop / decor heights. Observation only: never touches physics.
	void NotePeakCause(int32 Body, const TCHAR* Cause);
	/** Envelope peak (ball centre z) of a launch type from ReachPeaks; -1 if unknown. */
	float ReachEnvelope(const FString& Cause) const;
	float ReachEnvelopeMax() const;
	/** Lowest ball-centre z at which a Zoogi can use the prop (halo trigger bottom, bumper contact, gate / rail deck). */
	float PropReqZ(const AZoogiAerialProp* P) const;
	/** The launch type with the lowest envelope peak that still reaches ReqZ ("" if none). */
	FString BestLaunchFor(float ReqZ, float& OutPeak) const;
	/** Logs "BATTLE reach" per placed prop; returns the number of props no launch type reaches. */
	int32 LogReach(FString* OutDetail = nullptr) const;
	TMap<FString, float> RunPeaks;          // this run: max peak per launch type
	TMap<FString, int32> RunPeakCounts;
protected:
	void TickPeaks();
	void FlushPeaks(const TCHAR* Why);
	TMap<int32, FZoogiPeakTrack> PeakTracks;
	int32 PeakActionId = -1;

	FZoogiBattleTuning Tune;
	bool bFlow = false;
	EZoogiBattleState State = EZoogiBattleState::None;
	float StateReal = 0.f;
	double RealClock = 0.0;
	int32 LastTurnKey = INT32_MIN;
	int32 LastActive = -1;
	bool bResolvedThisAction = false;
	int32 CurrentActionId = -1;
	int32 CurrentActor = -1;
	FZoogiPendingSegment Pending;
	TArray<FZoogiPendingSegment> History;  // every segment (completed / shattered), match order
	int32 NextSegment = 1;
	int32 AwakenSeenTurn = -1;
	int32 HumanCursor = -1;               // debug combo cursor
	FDelegateHandle hStart, hResolving, hFinal, hAir;
	bool bMatchEnded = false;
	FString MatchId;
	TMap<int32, float> ActionEnergyDelta;  // player -> energy change during the current action

	// T3 air reaction runtime
	int32 AirPendingBody = -1;
	float AirPendingPeak = 0.f;
	float AirPendingWait = 0.f;
	int32 AirHoldToken = 0;
	int32 AirApplyFrames = 0;              // >0: apply the chosen reaction on the first unpaused frame
	FZoogiPendingSegment AirSegment;

	// T5 test director
	int32 TestStep = 0;
	int32 TestShooter = 0, TestTarget = 1;
	FVector TestAim = FVector::ZeroVector;
	double TestQuitAt = 0.0;
	double TestDeadline = 0.0;
	int32 TestActionId = -1;
	bool bTestDone = false;
	int32 TestTurnsSeen = 0;
	TArray<int32> TestTurnOrder;      // active player at each turn start (incl. follow-ups)
	TArray<bool> TestTurnFollowUp;
	TArray<FString> TestNotes;
	int32 TestActions = 0;            // actions finalized since the scripted one started
	int32 TestScriptedTurns = 0;      // scripted P1 turns set up so far
	double TestWaitUntil = 0.0;
	double TestShatterAt = 0.0;
	double TestResolveAt = 0.0;
	int32 TestOrb = -1;
	FString TestFinal;
	bool bTestInjectOffer = false;
	bool bInTestCheck = false;
	bool TestAimLock = false;
	FName TestCombo;
	void TestScriptTurn();
	void TestCheck();
	friend class UZoogiMiniGame;
	friend class UZoogiEpisodeLedger;
	friend class UZoogiEpisodeDirector;
	friend class UZoogiEpisodeAssembler;
};
