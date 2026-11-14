#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZoogiTypes.h"
#include "ZoogiArenaRoute.h"
#include "ZoogiArtifacts.h"
#include "ZoogiMatchEvents.h"
#include "ZoogiLaunchFlow.h"
#include "ZoogiDropIntro.h"
#include "ZoogiRoyale.h"
#include "ZoogiFrozenRoam.h"
#include "ZoogiGameMode.generated.h"

struct FHitResult;
class AZoogiBall;
class AZoogiOrb;
class AZoogiArena;
class AZoogiClone;
class AZoogiComboArena;
class UPhysicalMaterial;

/** 2v2 Competitive Combo (Evan's package Part 4): the three movement forces of the Battle input. */
enum class EZoogiForce : uint8
{
	Control, // low power, high accuracy: orb passes, redirects, precision setups
	Drive,   // balanced standard competitive force
	Launch   // maximum momentum: ring-outs, aerial attacks, heavy collisions; less forgiving
};

/** Partner Follow-Up triggers (bit flags, one shot can set several). */
namespace ZoogiComboTrigger
{
	constexpr uint32 AllyBump = 1 << 0;
	constexpr uint32 BankSetup = 1 << 1;
	constexpr uint32 AirborneSetup = 1 << 2;
	constexpr uint32 OrbRelay = 1 << 3;
	constexpr uint32 GuardBreak = 1 << 4;
	constexpr uint32 PowerSetup = 1 << 5;
	constexpr uint32 TerrainSetup = 1 << 6;
	constexpr int32 Num = 7;
}

/** 2v2 Combo tuning (cm, cm/s, gauge points). */
namespace ZoogiComboTune
{
	constexpr float ControlMaxFrac = 0.50f;  // CONTROL: stops on the aim point / arrives soft, capped at this pull fraction
	constexpr float DriveFrac = 0.60f;       // DRIVE: ~14 m/s
	constexpr float LaunchFrac = 0.90f;      // LAUNCH: ~26 m/s
	constexpr float LaunchMassMul = 1.15f;   // LAUNCH: heavier collisions
	constexpr float FollowUpSpeedMul = 1.08f;
	constexpr float GaugeMax = 100.f;
	constexpr float FollowUpCost = 30.f;
	constexpr float EBank = 15.f, EAllyBump = 25.f, EOrbOut = 20.f, ERelay = 30.f, EGuardBreak = 20.f, EAirborne = 20.f, ETerrain = 15.f, EPower = 10.f, ERingOut = 15.f, EGuardReaction = 15.f;
	constexpr float AllyBumpMinSpeed = 250.f; // relative normal speed for a "meaningful" teammate redirect
	constexpr float RelayRadius = 450.f;      // relayed orb ends this close to the partner (or touches it)
	constexpr float AirborneHeight = 18.f;    // gap under the ball that counts as airborne
	constexpr float ControlOrbOut = 25.f;     // Arena Control per ORB OUT
	constexpr float ControlRingOut = 15.f;
	constexpr float ShiftAtControl = 50.f;    // Resonance Shift: first team to 50 Arena Control
	constexpr int32 ResonanceSurge = 3;       // 3 Resonance -> +1 artifact draw
	constexpr int32 OrbScore = 50;
	constexpr int32 RelayFinishBonus = 25;
	constexpr int32 RingOutScore = 100;
}

struct FZoogiCallout
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
};

/** Coarse match state. Aiming covers Draw / Awaken / Battle aim, Rolling the launched shot, Resolving the Resolve phase. */
UENUM(BlueprintType)
enum class EZoogiPhase : uint8
{
	Menu,
	Aiming,
	Rolling,
	Resolving,
	GameOver,
	Forming,  // Frozen Ring (Oct 6): the stage assembles before the first turn (skippable)
	DropIntro // Live ship cabin hold -> Tac bird's-eye -> Drop (Evan Oct 23)
};

/**
 * The turn phase loop (Milestone "Awaken", Evan Oct 4): Draw -> Awaken -> Battle -> Resolve -> End.
 * Only AZoogiGameMode::SetStage changes it (ZoogiTargeting.cpp); the camera director keys off it.
 */
UENUM(BlueprintType)
enum class EZoogiStage : uint8
{
	Draw,     // an artifact is drawn (hero beat on the active Zoogi)
	Awaken,   // no movement: choose Infuse / Guard / Save for the drawn (or a saved) artifact
	Battle,   // aim (lock-on or free floor aim, cut nudges), confirm, power-up, launch, roll
	Resolve,  // after everything settles (or times out): commit score, statuses, guard, KOs, anime
	End       // short beat, then the next player's turn
};

/** Battle sub-steps. */
UENUM(BlueprintType)
enum class EZoogiBattleStep : uint8
{
	Aim,      // select target / trajectory (clickable)
	PowerUp,  // confirmed: power animation (shell expands, core flares), then launch
	Rolling   // launched: collision, artifact effect, physics reaction
};

/** Turn timings (seconds). */
namespace ZoogiStageTune
{
	constexpr float DrawHuman = 1.2f;
	constexpr float DrawCPU = 0.35f;        // build 23 #2: snappy CPU turns (was 0.9)
	constexpr float AwakenCPU = 0.45f;  // build 23 #2 (was 0.9)        // CPU decides this long into Awaken
	constexpr float AwakenBeat = 0.75f;      // after Infuse / Guard: activation beat before Battle (placeholder anime plays here)
	constexpr float AwakenSaveBeat = 0.35f;
	constexpr float CPULockAt = 0.15f;  // build 23 #2 (was 0.35)       // CPU locks this long into Battle aim
	constexpr float CPUMain = 0.55f;    // build 23 #2 (was 1.2)          // ...and confirms here
	constexpr float BattleSwing = 0.85f;     // cinematic camera: lock-on swing
	constexpr float BattleHold = 0.5f;
	constexpr float CPUBattleHold = 0.15f; // build 23 #2 (was 0.3)
	constexpr float PowerUpTactical = 0.45f; // tactical camera: short power-up beat
	constexpr float PowerUpInfused = 0.8f;   // longer when an artifact is infused
	constexpr float LongLockDist = 1100.f;   // locks farther than this get the two-beat Battle camera (cinematic style)
	constexpr float TargetBeat = 0.55f;
	constexpr float LockImpactSpeed = 650.f; // cm/s the shot still has when it reaches the target (plus 0.25 x distance)
	constexpr float SettleTime = 0.45f;      // build 23 #1 (was 0.8)       // everything at rest this long -> Resolve
	constexpr float ResolveTimeout = 12.f;   // after launch: force Resolve (bodies are stopped where they are)
	constexpr float ResolveMin = 0.45f;      // build 23 #1 (was 0.7)       // Resolve phase shows at least this long
	constexpr float EndBeat = 0.2f;          // build 23 #1 (was 0.35)
	constexpr float MaxCutDeg = 30.f;        // angled aiming: cut nudges up to +-30 deg
	constexpr float CutStepDeg = 2.f;
	constexpr float CritImpactFrac = 0.90f;  // crit: powered hit keeping >= 90% of its launch speed at contact (Oct 4: 75% -> 90%)...
	constexpr float CritMaxCutDeg = 5.f;     // ...and hitting within 5 deg of head-on (Oct 4: 8 -> 5)
	constexpr float CritEffectMul = 1.3f;
}

/** Shot preview for the HUD (and CPU planning): dotted path, ghost-ball contact, departure arrows, one bank bounce. */
struct FZoogiAimPreview
{
	bool bValid = false;
	FVector Start = FVector::ZeroVector;
	FVector Dir = FVector::ForwardVector;
	float Speed = 0.f;
	float Fraction = 0.f;
	TArray<FVector> Path;         // ground points (stopping distance, one reflection)
	TArray<bool> PathOff;         // point is off court
	bool bContact = false;
	TWeakObjectPtr<AActor> ContactActor;
	FVector Ghost = FVector::ZeroVector;      // shooter centre at contact
	FVector TargetDir = FVector::ZeroVector;  // target departure (along the contact normal)
	FVector ShooterDir = FVector::ZeroVector; // shooter deflection (scaled by how much speed it keeps)
	float CutDeg = 0.f;                       // angle between the shot and the contact normal
	bool bBank = false;
	FVector BankPoint = FVector::ZeroVector;
};

/** What happened during the roll, committed in Resolve (authoritative results). */
struct FZoogiLedgerEntry
{
	int32 Player = -1;
	int32 Points = 0;
	bool bStar = false;
	bool bKO = false;       // Player knocked out Victim
	int32 Victim = -1;
	bool bFall = false;     // Victim fell (penalty)
};

USTRUCT(BlueprintType)
struct FZoogiPlayerSetup
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ZoogiIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCPU = false;
};

USTRUCT()
struct FZoogiFeedItem
{
	GENERATED_BODY()
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
};

USTRUCT()
struct FZoogiPopup
{
	GENERATED_BODY()
	FString Text;
	FVector World = FVector::ZeroVector;
	float Age = 0.f;
	FLinearColor Color = FLinearColor::White;
};

/** Orb pickup: a glowing orb that flies from where it fell to the scorer's scoreboard row. */
struct FZoogiFlyer
{
	FVector World = FVector::ZeroVector;
	int32 Player = 0;
	int32 Points = 0;
	float Age = 0.f;
	FLinearColor Color = FLinearColor::White;
};

/**
 * Match rules (ported from useZoogiGame.tsx): hotseat turns, a turn ends
 * once everything has been at rest for SETTLE_DELAY_SECONDS, orbs knocked
 * off score for the last Zoogi that touched them, Zoogis knocked off lose
 * points and give the attacker a knockout bonus.
 */
UCLASS()
class ZOOGIROLL_API AZoogiGameMode : public AGameModeBase
{
	GENERATED_BODY()
	friend class UZoogiFunSys; // Oct 29 build 23: action / fun / timing pack (ZoogiActionFun.h)

public:
	AZoogiGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- menu
	UPROPERTY(BlueprintReadOnly) int32 MenuNumPlayers = 2;
	UPROPERTY(BlueprintReadOnly) TArray<FZoogiPlayerSetup> MenuPlayers;
	UPROPERTY(BlueprintReadOnly) EZoogiArena MenuArena = EZoogiArena::Meadow;
	UPROPERTY(BlueprintReadOnly) int32 MenuRounds = 8;
	/** Bumped on every menu change so the UMG menu knows to refresh. */
	int32 MenuRevision = 0;
	void MenuAction(FName Action, int32 Index);
	/** b77: the WOLF button (Wolf Pack skill) - the old Wolf Pack superpower for this ball, no charge spent */
	void FireWolfSkill(AZoogiBall* Ball);
	/** b86: round start - the spire throws in fresh orbs (the missing ones, up to 6) */
	void SpireRefillOrbs(int32 MaxNew = 6, bool bRoundStart = true); // b193: also a small top-up every turn
	int32 SpawnedOrbCount = 0;
	void StartMatch();
	/** Plays the arena's stage-forming intro (if it has one) before the first turn. Returns false when there is none. */
	bool BeginStageForming(bool bCombo);
	void SkipStageForming();
	bool bFormingCombo = false;
	float FormTimeScale = 1.f;      // -ZoogiFormSlow=4 (forming shots)
	bool bElevatorsPlayed = false;  // spawn elevators started this match (presentation only)
	bool bNoForming = false;        // -ZoogiNoForm
	bool bFormShots = false;        // -ZoogiFormShots: frames of the forming intro + pillar rise close-ups
	bool bDamageShots = false;      // -ZoogiDamageShots: every piece type through its damage states
	int32 FormShotIdx = 0;
	float DamageShotClock = -1.f;
	int32 DamageShotStep = 0;
	void TickFormShots();
	void TickDamageShots(float Dt);
	int32 FormPass = 0;             // forming shots: pass 0 = overview sweep, pass 1 = pillar close-up
	int32 FormCamMode = 0;          // read by the controller: 0 sweep, 1 pillar close-up
	UPROPERTY() TObjectPtr<class ACameraActor> ShotCam;
	double DamageShotReal = 0.0;
	/** Combo arena for a menu pick: the arena the player picked (Oct 22; was Frozen-or-Meadow). See ZoogiArenaRoute.h. */
	static EZoogiArena ComboArenaFor(EZoogiArena Pick, int32 Demo) { return ZoogiArenaRoute::ComboArena(Pick, Demo); }
	void ReturnToMenu();
	/** After Match results "Change Zoogi": ReturnToMenu then HUD opens Zoogi select. */
	bool bPendingZoogiSelect = false;

	// ---- match state
	UPROPERTY(BlueprintReadOnly) EZoogiPhase Phase = EZoogiPhase::Menu;
	UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<AZoogiBall>> Balls;
	UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<AZoogiOrb>> Orbs;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AZoogiArena> Arena;
	UPROPERTY(BlueprintReadOnly) int32 ActivePlayer = 0;
	UPROPERTY(BlueprintReadOnly) int32 RoundNumber = 1;
	UPROPERTY(BlueprintReadOnly) int32 WinnerIndex = -1;
	float PhaseTime = 0.f;
	float SettleTimer = 0.f;
	TArray<FZoogiPopup> Popups;
	/** Big centre-screen callout (KNOCKOUT!) */
	FString BigText;
	FLinearColor BigColor = FLinearColor::White;
	float BigAge = 99.f;
	TArray<FZoogiFlyer> Flyers;
	TArray<FZoogiFeedItem> Feed;  // HUD event feed (knock-outs, orbs, stars, stuns)
	FString Banner;
	float BannerTime = 0.f;

	AZoogiBall* GetActiveBall() const;
	int32 OrbsLeft() const;
	bool IsHumanAiming() const;

	// ---- aim (shared by the mouse drag and the CPU)
	/** Pull vector on the ground (drag start minus drag end), in cm, before DragScale. */
	FVector AimPull = FVector::ZeroVector;
	bool bAimActive = false;
	void SetAim(bool bActive, const FVector& Pull);
	/** Fires the active Zoogi if the pull is long enough. Returns true on a shot. */
	bool ReleaseAim();
	static FVector2D PullToVelocity(const FVector& Pull, float& OutFraction);
	float AimFraction() const;
	void RotateAim(float DeltaYaw);

	// ---- targeting + turn phases (ZoogiTargeting.cpp)
	UPROPERTY(BlueprintReadOnly) EZoogiStage Stage = EZoogiStage::Draw;
	UPROPERTY(BlueprintReadOnly) EZoogiBattleStep BattleStep = EZoogiBattleStep::Aim;
	float StageTime = 0.f;
	float StepTime = 0.f;
	void SetBattleStep(EZoogiBattleStep S);
	static const TCHAR* StepName(EZoogiBattleStep S);
	bool IsAimStep() const { return Phase == EZoogiPhase::Aiming && Stage == EZoogiStage::Battle && BattleStep == EZoogiBattleStep::Aim; }
	bool IsPowerUpStep() const { return Phase == EZoogiPhase::Aiming && Stage == EZoogiStage::Battle && BattleStep == EZoogiBattleStep::PowerUp; }
	/** Camera style: Tactical (default, Evan Oct 4) or the Oct 3 per-phase Cinematic cameras (-ZoogiCinematic / HUD toggle). */
	bool bCinematicCam = false;
	/** Oct 24 (Evan): Cinema view hidden + out of the button cycle. Oct 27 evening (Evan): back ON - classic matches start in the
	 *  Cinematic camera (hero shot, target close-up, battle whip); the camera button cycles Cinematic -> Normal -> Tac. */
	bool bCinemaEnabled = true;
	/** Oct 24 (Evan): camera button cycles Normal (false) <-> Tac bird's-eye (true). Button-only, no manual zoom. */
	bool bBirdEyeCam = false;

	// ---- angled aiming (clickable): free floor aim, cut nudges, power steps, bank preview
	bool bFreeAim = false;
	FVector FreeAimPoint = FVector::ZeroVector;
	float AimCutDeg = 0.f;
	/** 0 = auto (distance-scaled), 1 soft, 2 medium, 3 full. */
	int32 AimPowerStep = 0;
	/** Oct 30 (Evan): ARC toggle right of LAUNCH - the next human shot skips up and over (walls, rivals) and lands within ArcRange(). One shot, then off. */
	bool bArcShot = false;
	float ArcRange() const;
	/** Oct 30 (Evan): the way back from Frozen's outer ring. A Zoogi outside Ring 1 (outer ring / bridges) gets JUMP IN - the ARC
	 *  button, armed at its turn start (CPUs always jump): a steep lob over the Ring 1 wall that lands inside the court (the aim point
	 *  when it is on the court, else straight in). It is that turn's shot. Still out at the start of a third turn -> AutoJumpIn():
	 *  the Zoogi is set on the nearest bridge and catapulted in (costs that turn). -ZoogiOuterTest = capture run. */
	bool InOuterRing(const AZoogiBall* B) const;
	bool ActiveInOuterRing() const;
	FVector2D JumpInLanding(const FVector2D& From, const FVector2D& Want) const;
	void AutoJumpIn();
	TArray<int32> OuterTurns;
	bool bAutoJumpPending = false, bForceJumpIn = false;
	/** Oct 30 (Evan): the four halos over Frozen's outer ring are launchers - roll under one and it fires you back onto the court
	 *  (glowing pad on the floor under each). */
	void TickHaloPads();
	bool bHaloPadsBuilt = false;
	TArray<TWeakObjectPtr<AActor>> HaloPadMarks;
	TMap<int32, double> HaloPadUsed;
	TWeakObjectPtr<AZoogiBall> JumpBody; // the jump-in / halo launch in the air: its slide is cut when it lands
	double JumpT0 = 0.0;
	FVector JumpV3 = FVector::ZeroVector;
	/** Oct 14 drag aim (human only): pull fraction from the slingshot drag (-1 = none). Combo maps it onto CONTROL / DRIVE / LAUNCH. */
	float HumanDragFrac = -1.f;
	/** Item 21: lateral slingshot flick (-1..1) captured at release; applied as gameplay spin on FireShot. */
	float HumanFlickSide = 0.f;
	/** aim-arrow (Oct 17): the aim was last set by rotating the aim arrow (ZoogiAimArrow); the tactical camera holds its framing. */
	bool bArrowAim = false;
	/** aim-arrow: free aim along YawDeg at Dist cm (clamped onto the court like a floor tap). Quiet: no click, no lock swing. */
	void SetArrowAim(float YawDeg, float Dist);
	/** Build 27 (Evan): launch / drag-launch without aiming first - no aim yet = aim along the arrow (or the pad landing point). True = has an aim now. */
	bool EnsureArrowAim(const TCHAR* Why);
	FZoogiAimPreview Preview;
	void SetFreeAim(const FVector& GroundPoint);
	void NudgeAim(int32 Steps);
	bool HasAim() const;
	/** Computes the human shot (lock or free aim, cut, power step). */
	FVector2D ComputeShotVelocity(float& OutFraction, float& OutImpactSpeed) const;
	void UpdatePreview();
	FVector2D PowerStepVelocity(const FVector2D& Dir, float BaseFraction, float& OutFraction) const;

	// ---- artifacts / Awaken (ZoogiAwaken.cpp)
	/** Artifact mode (default). -ZoogiClassicPowers: the Oct 3 Charges + E/Q powers, no Awaken. */
	bool bArtifactMode = true;
	/** Awaken selection: 0 = the drawn artifact, 1.. = saved hand slots. */
	int32 AwakenPick = 0;
	bool bAwakenDone = false;
	bool bStandbyAwaken = false;        // v3 cards: Episode Awakening requested at Standby (independent of the cards chosen)
	FName LastGameplayAction; int32 LastGameplayIndex = -1; uint64 LastGameplayFrame = 0; int32 DuplicateInputsIgnored = 0; // v3 P6 duplicate-input guard
	float AwakenDoneAt = 0.f;
	FString AwakenChoiceText;
	int32 TurnCounter = 0;
	EZoogiArtifact PickedArtifact() const;
	bool AwakenInfuse(int32 Pick);
	bool AwakenGuard(int32 Pick);
	bool AwakenSave();
	// ---- v3 cards (ZoogiCardGlue.cpp): with [ZoogiRoll.Cards] bCards / -ZoogiCards the Awaken stage is the card STANDBY
	bool CardsOn() const;
	void CardsBeginStandby();
	void CardsToggle(int32 HandSlot);
	void CardsClear();
	bool CardsCommit();
	/** Oct 20 (Evan): cards are optional before aiming - the human aims right away; cards can still be picked and ACTIVATEd
	 *  during the aim step, and an unactivated turn launches with no card (the selection, if any, is committed at launch). */
	bool CardsOptionalAim() const;
	bool CardsWindowOpen() const; // human may still pick / ACTIVATE cards this turn
	void CardsCommitAtLaunch();
	/** Oct 20 (Evan): tap a card = play it now (select + commit that one card); more taps play more cards while energy lasts. */
	bool CardsTapPlay(int32 HandSlot);
	/** b120g COMBO: commit the card in this slot (SP spent) without casting it; true + its id when armed */
	bool CardsArmCombo(int32 HandSlot, FName& OutId);
	/** Oct 22 (Evan): tap-to-play window = pre-launch (CardsWindowOpen) OR mid-roll until the turn's at-rest wait. */
	bool CardsPlayWindowOpen() const;
	/** Oct 22: the window is open AND this hand slot's card has a mid-roll effect when rolling (Blocked cards grey out). */
	bool CardsSlotPlayableNow(int32 HandSlot) const;
	void CardsToggleAwaken();
	// ---- Choose-your-Awaken (Evan playtest, Oct 15): after the draw the tray shows 3 Awaken stone tablets; the pick
	// infuses that element for this turn and requests Episode Awakening; SKIP = no Awaken. Offers = 3 of the 5 classic
	// artifact elements (Ember / Storm / Frost / Titan / Void), shuffled by a (turn, player) hash - never a GameRng draw.
	bool bCardAwakenView = false;       // the tray shows the 3 tablets instead of the cards (orb / A toggles)
	int32 CardAwakenPick = -1;          // offer index picked this turn (-1 = none / skipped)
	int32 CardAwakenOfferTurn = -1;     // turn the offers were built for
	uint8 CardAwakenOffers[3] = { 0, 0, 0 }; // EZoogiArtifact
	void CardsBuildAwakenOffers(int32 Player);
	// ---- Awaken charge (Evan, Oct 20): every committed card ability adds 1 pip; at AwakenChargeMax the AWAKEN button lights
	// and infuses the Zoogi's own affinity element for this turn (replaces the 3 tablets). [ZoogiRoll.Awakening]
	// bChargeFromCards=True (default) / -ZoogiAwakenTablets restores Choose-your-Awaken.
	static constexpr int32 AwakenChargeMax = 4;
	int32 AwakenCharge[4] = { 0, 0, 0, 0 };
	bool AwakenChargeOn() const;
	void CardsAddAwakenCharge(int32 Player, int32 Pips, const TCHAR* Why);
	bool CardsAwakenReady(int32 Player) const;
	bool CardsUseAwaken(const TCHAR* Who);
	void CardsAwakenView(int32 Mode);   // 0 toggle, 1 open (auto, with the CHOOSE YOUR AWAKEN call), 2 close
	bool CardsAwakenPick(int32 Offer);
	void CardsAwakenSkip();
	bool CardsAwakenViewOpen() const;
	bool CardsApplyAwakenPick(int32 Offer, const TCHAR* Who);
	void CardsPlanCPU();
	float CardsComboGainMul() const;
	float CardsEpisodeGainMul() const;
	float CardsComboStartGauge() const;  // v3 P6 PROPOSED
	/** v3 P5: CPU Standby Awakening request in cards mode, same decision inputs as the classic Infuse/Guard choice. */
	void CardsPlanCPUAwaken();
	void DoDraw();
	void PlanCPUAwaken();
	/** Infused shooter's first contact: apply the artifact effect (computed now = authoritative). */
	void ApplyInfuseImpact(AZoogiBall* Shooter, AActor* Other);
	/** A guarded Zoogi was hit by the attacker (called from AZoogiBall::OnSphereHit). */
	void OnGuardedHit(AZoogiBall* Victim, AZoogiBall* Attacker);
	void TickArtifacts(float Dt);
	void DoResolve();
	/** Applies Ledger to the scores (Resolve; late falls during Resolve are committed the same way). */
	void CommitLedger(int32& ShooterScores, int32& KOs);
	void ApplyStatusOnShot(AZoogiBall* Ball, FVector2D& V);
	TArray<FZoogiLedgerEntry> Ledger;
	bool bInfusedContact = false;
	float ContactImpactSpeed = 0.f;
	bool bLastHitCrit = false;
	float CPUPlannedCut = 0.f;
	/** Gameplay randomness (artifact draws, CPU planning) on its own host-owned stream, so cosmetic FX/SFX
	 * that use the global FMath::Rand can never shift a draw or a CPU shot. Seed: -ZoogiMatchSeed=, else
	 * -ZoogiEpisodeSeed=, else a fresh seed per match (logged as "GAME rng seed="). */
	FRandomStream GameRng;
	int32 GameSeed = 0;
	FVector PullCenter = FVector::ZeroVector;
	float PullLeft = 0.f, PullAccel = 0.f, PullRadius = 0.f;
	int32 PullCredit = -1;
	float ResolveStart = 0.f;
	/** Resolve banner ("P1 +100 KO   P2 -75") and per-player deltas for the HUD. */
	FString ResolveSummary;
	FString ResolveNotes;      // powered hit / crit / KO / guard lines for the Resolve panel
	int32 TurnGuardBlocks = 0;
	TArray<int32> ResolveDeltas;
	void EmitAnime(FName Event, AZoogiBall* Who, EZoogiArtifact A, AActor* Focus, const FVector& Where, float Magnitude, bool bFreeze, int32 TargetPlayer = -1, bool bRewardShatter = false);
	/** Titan Core (infused, while shooting) / guard mass. */
	void RefreshArtifactMass(AZoogiBall* B, bool bShooting);
	TArray<TWeakObjectPtr<AZoogiBall>> FrozenBalls;
	/** Locked target (rival Zoogi or orb). Soft = picked during Standby (ring only, camera stays wide). */
	TWeakObjectPtr<AActor> TargetLock;
	bool bSoftLock = false;
	float LockTime = 0.f;
	int32 LockSerial = 0;
	void SetStage(EZoogiStage S);
	/** b62 (Evan: "After clash cut scene animation go to next turn"): end the roll now (knockoffs checked, Resolve). */
	void ForceResolveNow(const TCHAR* Why);
	static const TCHAR* StageName(EZoogiStage S);
	bool IsValidTarget(const AActor* A) const;
	/** Live targets for the active Zoogi, nearest first. */
	TArray<AActor*> TargetCandidates() const;
	void LockOn(AActor* Target, bool bSoft);
	/** b122: LiveTargets for outside systems (SHOWTIME target pick) */
	TArray<AActor*> PublicLiveTargets(const AZoogiBall* Except) const { return LiveTargets(Except); }
	void ClearLock();
	/** Battle map props (Oct 8): re-plan the active CPU's shot before its Battle step (the map layout / a test placement
	 *  arrived after the turn-start plan). No-op for humans or once the CPU is in its Battle step. */
	void ReplanCPUShot();
	void CycleLock(bool bSoft);
	/** Space / click on the locked target during Main: start the Battle stage. */
	bool CommitLock();
	/** Human drag aim: live power while dragging (bLaunch false) or release = launch (bLaunch true). SideFrac = lateral flick (-1..1). Returns true when launched. */
	bool HumanDrag(float Frac, bool bLaunch, float SideFrac = 0.f);
	void HumanDragCancel();
	/** Shot straight at the locked target: speed scales with distance (capped at full power). */
	FVector2D LockShotVelocity(float& OutFraction, float& OutImpactSpeed) const;
	FString TargetName(const AActor* A) const;
	float StageDuration() const;
	/** Long lock: the Battle stage opens with a close-up on the target (two-beat camera) and runs TargetBeat longer. */
	bool IsLongLock() const;
	float BattleDuration() const;
	bool AllAtRestPublic() const { return AllAtRest(); }

	// ---- powers (ZoogiPowers.cpp)
	/** An armed power (Ricochet, Lock-On, Static Shock, Bandage Bind) fires with the next shot. */
	UPROPERTY(BlueprintReadOnly) bool bPowerArmed = false;
	UPROPERTY(BlueprintReadOnly) bool bPowerUsed = false;
	bool CanUsePower() const;
	/** SPACE / E / HUD button: arm, disarm or fire the active Zoogi's power. Returns true if something happened. */
	bool TryPower();
	// Oct 27 Phase 3 (Evan's spec, build 9): the royale buttons in classic. TRANSFORM = SUPERPOWER (the Zoogi's Awaken when
	// charged, else its classic power), JUMP = REACTION (a hop while the shot rolls, once per shot, no delay).
	bool ClassicButtonsOn() const;
	/** Oct 27 evening (Evan): classic aims with the early-October aim bar again. Oct 28 (Evan): the bar is just AUTO / CONTROL /
	 *  DRIVE / LAUNCH presets (tap one after picking a target = shoot) and the drag-to-aim arrow + slingshot is back alongside it
	 *  (no arrows / NEXT / CONFIRM / CANCEL). -ZoogiDragAim = drag only (no bar). */
	bool ClassicAimBar() const;
	bool ClassicSuperpowerReady() const;
	bool ClassicSuperpower();
	bool ClassicReactionReady() const;
	bool ClassicReaction();
	/** Oct 28 (Evan): Wolf Pack (Wolfgang clones) is the only power in classic and 2v2 (not Battle Royale). */
	bool WolfPackOnly() const;
	/** Gives this Zoogi Wolf Pack as its power when WolfPackOnly(). Call right after InitZoogi. */
	void ApplyWolfPackOnly(AZoogiBall* B);
	/** Oct 28 (Evan): classic bar preset (0 AUTO .. 3 LAUNCH): sets the strength and, with a target picked, shoots right away. */
	void AimPreset(int32 Step);
	bool bReactionUsed = false;
	/** PowerUp hold before the shot leaves: 0 for a human classic shot (launch on release), BattleDuration() otherwise. */
	float PowerUpHold() const;
	// Oct 27 Frozen Ring roam (ZoogiFrozenRoam.cpp): outer-ring free roll -> gate -> bridge launch point -> right-stick launch.
	bool FrozenRoamOn() const;
	bool FrozenNeedsEntry(int32 Player) const;
	bool FrozenRoamActive() const;
	bool FrozenPrepReady() const;
	bool FrozenCanLaunch() const;
	void FrozenStartMatch();
	void FrozenRespawn(AZoogiBall* Ball);
	void FrozenTestSpawn(int32 SpawnIdx);
	void FrozenSetMoveInput(const FVector2D& WorldInput);
	void FrozenAdjustAim(float DeltaDeg);
	float FrozenPrepYaw() const;
	bool FrozenHumanLaunch(float Yaw, float Power);
	void FrozenOnLaunched();
	bool FrozenBlocksCPUShot() const;
	void TickFrozenRoam(float Dt);
	void SetRoamState(EZoogiRoamState S, const TCHAR* Why);
	TArray<bool> RoamEntered;
	EZoogiRoamState RoamState = EZoogiRoamState::None;
	float RoamStateT = 0.f;
	int32 RoamBridge = -1, RoamCpuBridge = -1;
	float RoamAimOffset = 0.f, RoamPrepYaw = 0.f;
	FVector RoamPrepStart = FVector::ZeroVector, RoamPrepAnchor = FVector::ZeroVector;
	FVector2D RoamMoveInput = FVector2D::ZeroVector;
	bool bRoamLaunchGate = false;
	bool bRoamCpuPlanned = false;
	/** Aim pull after Lock-On snapping (what the shot will really use). */
	FVector GetEffectivePull() const;
	/** Lock-On target for the current pull, if any. */
	AActor* GetLockTarget() const;
	/** The active shooter touched something (hit or Bolt overlap). */
	void OnShooterContact(AZoogiBall* Shooter, AActor* Other);
	/** Destructible rocks (AZoogiRock): popup, feed line and automation screenshots. */
	void OnRockEvent(class AZoogiRock* Rock, bool bShattered, AZoogiBall* By, const FVector& PopupAt);
	float GetShotSpeed() const { return ShotSpeed; }
	/** Dips the music for a big hit (the beat of silence around the boom), back up after Seconds. */
	void DuckMusic(float Seconds);
	// ---- Climax (team brief): ONE moment per match, the match-winning KO (or the KO that takes the sole lead in the
	// final round). Music drops out completely, ClimaxRumble plays (~2.2 s + 0.3 s silence), then the KO hit lands with an
	// impact flash + CritBoom on the same frame and the music swells back.
	bool IsClimaxMoment(const AZoogiBall* Shooter, int32 KOs) const;
	void BeginClimax(AZoogiBall* Shooter, EZoogiArtifact A, int32 KOs, int32 ShooterScores);
	void LandClimax();
	/** PlayEventSfx asks this for the KO/Combo it is about to sound: true once, for the climax hit. */
	bool ConsumeClimaxHit() { const bool b = bClimaxHitPending; bClimaxHitPending = false; return b; }
	bool bClimaxUsed = false;
	bool bClimaxTest = false; // -ZoogiClimaxTest: the first KO of the match (any round) gets the climax
	bool bClimaxHitPending = false;
	float ClimaxLeft = 0.f;          // > 0 while the rumble plays (holds Resolve like an anime card)
	float ClimaxFlashAt = -100.f;    // Clock time of the climax impact flash (HUD draws a white frame for ~0.1 s)
	float GetClock() const { return Clock; }
	TWeakObjectPtr<AZoogiBall> ClimaxShooter;
	EZoogiArtifact ClimaxArtifact = EZoogiArtifact::None;
	int32 ClimaxKOs = 0, ClimaxScores = 0;
	/** Victory crowd: two synthesized waves (second one denser) on the game-over screen. */
	void PlayVictoryCrowd();
	/** -ZoogiRockTest: CPUs aim at the nearest standing rock and always SAVE (crack first, shatter on the second hit). */
	bool bRockTest = false;
	UPROPERTY() TArray<TObjectPtr<AZoogiClone>> Clones;

	// ---- 2v2 Competitive Combo (ZoogiCombo.cpp). Classic stays the default; -ZoogiCombo or the menu MODE row turns it on.
	bool bComboMode = false;
	bool bClassicTeams = false; // b166: story 2v2 on the classic turn flow (P1+P2 vs P3+P4); cleared by MenuAction Mode
	EZoogiForce ComboForce = EZoogiForce::Drive;   // chosen force for the current aim
	EZoogiForce ComboShotForce = EZoogiForce::Drive; // force of the shot in flight
	int32 ComboTeam = 0;                // team taking the current team turn (0 = A: P1+P2, 1 = B: P3+P4)
	int32 ComboNextPick[2] = { 0, 0 };  // which teammate is pre-selected next team turn
	float ComboEnergy[2] = { 0.f, 0.f };
	int32 ComboResonance[2] = { 0, 0 };
	float ArenaControl[2] = { 0.f, 0.f };
	int32 ComboOrbOuts[2] = { 0, 0 };
	bool bFollowUpUsed = false;         // one Partner Follow-Up per team turn
	bool bFollowUpOffered = false;
	bool bInFollowUp = false;
	bool bFollowUpBroken = false;
	float FollowUpOfferAt = 0.f;
	uint32 ShotTriggers = 0;            // follow-up triggers earned by this shot
	uint32 OfferTriggers = 0;
	bool bComboMatchOver = false;
	int32 ComboWinner = -1;
	FString ComboOverReason;
	TArray<FZoogiCallout> Callouts;
	TArray<FVector> ComboOrbHome;
	UPROPERTY() TObjectPtr<AZoogiComboArena> ComboArena;
	static int32 TeamOfIndex(int32 PlayerIdx) { return PlayerIdx / 2; }
	static FString TeamName(int32 Team) { return Team == 0 ? TEXT("TEAM A") : TEXT("TEAM B"); }
	static FLinearColor TeamColor(int32 Team);
	static const TCHAR* ForceName(EZoogiForce F);
	static FString TriggerNames(uint32 Mask);
	bool IsAlly(const AActor* A, const AActor* B) const;
	AZoogiBall* PartnerOf(const AZoogiBall* B) const;
	/** Oct 14: floating popup for the card layer (AddPopup is protected). */
	void CardPopup(const FString& Text, const FVector& World, const FLinearColor& Color) { AddPopup(Text, World, Color); }
	int32 TeamAlive(int32 Team) const;
	int32 TeamScore(int32 Team) const;
	void ComboSetMode(bool bOn);
	void ComboStartMatch();
	void ComboBeginTeamTurn(int32 Team);
	void ComboBeginShot(int32 PlayerIdx);
	void ComboResetShot();
	bool ComboSelect(int32 PlayerIdx);
	bool CanComboSelect() const;
	void ComboSetForce(EZoogiForce F);
	bool ComboAcceptFollowUp();
	bool ComboDeclineFollowUp();
	void ComboEndTurn();
	void ComboFinish(int32 WinTeam, const FString& Why);
	void ComboAddEnergy(int32 Team, float Amount, const FString& Why);
	void ComboTrigger(uint32 Flag, float Energy, const FVector& Where);
	void ComboCallout(const FString& Text, const FLinearColor& Color);
	void ComboOnBallHit(AZoogiBall* Self, AActor* Other, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit);
	void ComboModifyShot(AZoogiBall* Ball, FVector2D& V);
	float ComboShotMul(const AZoogiBall* Ball) const;
	float ComboForceFraction(float BaseFrac) const;
	void ComboTick(float Dt);
	void ComboCheckKnockoffs();
	void ComboResolve();
	bool ComboHoldResolve();
	/** Guard hit in combo: true if the guard was BROKEN (no damping). Otherwise the reaction extras are applied (after the default damping). */
	bool ComboGuardBreak(AZoogiBall* Victim, AZoogiBall* Attacker);
	void ComboGuardReaction(AZoogiBall* Victim, AZoogiBall* Attacker);
	void ComboSpawnOrbs();
	void ComboPlanCPUForce(float& Frac);
	// per-shot combo tracking
	TWeakObjectPtr<AZoogiOrb> RelayOrb;
	TWeakObjectPtr<AZoogiOrb> FollowRelayOrb;
	TWeakObjectPtr<AZoogiBall> BumpedAlly;
	bool bRelayTouched = false;
	bool bShotBanked = false;
	bool bShotTerrain = false;        // ramp / lip launch or bumper kick before the contact
	bool bShotContact = false;
	FString ShotTerrainWhat;
	int32 ComboMainShooter = 0;
	struct FBallAir { bool bAir = false; float AirTime = 0.f; bool bTerrainLaunch = false; float LastBumpAt = -10.f; float PrevVz = 0.f; FName GroundTag; };
	void ComboOnFloorChanged(AZoogiBall* By);
	TArray<FBallAir> AirState; // by player index
	struct FPendingKick { TWeakObjectPtr<AZoogiBall> Ball; FVector Dir; float Speed; };
	TArray<FPendingKick> PendingKicks;
	// -ZoogiComboDemo=1 (Definition of Done), 2 (Orb Relay + Resonance Shift), 3 (Reactive Terrain): scripted inputs through the same calls the HUD uses.
	int32 ComboDemo = 0;
	int32 DemoTurn = 0;      // team turns seen
	int32 DemoStep = 0;
	float DemoClock = 0.f;
	float DemoFUAt = -1.f;
	float BankLogAt = -1.f;
	int32 DemoSeenTurn = -1;
	bool bDemoShotFired = false;
	void TickComboDemo(float Dt);
	bool DemoAimBankInto(AZoogiBall* Into, float AngFrom, float AngTo, float R);
	bool DemoAimRingOut(AActor* Target, EZoogiForce F);
	void DemoStage();
	FString ComboShot(const FString& Name, float Delay = 0.f);

	// ---- automation / screenshots (command line: -ZoogiAuto -ZoogiPlayers=4 -ZoogiArena=1 -ZoogiShots=<dir> -ZoogiQuit=<sec> -ZoogiMenuShot)
	FString ShotDir;
	void TakeShot(const FString& Name);
	void TakeShotDelayed(const FString& Name, float Delay);
	TMap<FString, float> DelayedShots;
	FOnZoogiMatchEvent OnMatchEvent; void NotifyAnimeEvent(const FString& Ev);
	bool NotifyMatchEvent(EZoogiMatchEvent E, int32 Who = -1, float Mag = 0.f); // music + announcer (ZoogiMusicSubsystem.cpp); true while arena music drives the score

	// ---- live match drop intro (ZoogiDropIntroGame.cpp): ship -> Tac -> Drop before normal rounds
	FZoogiDropIntro DropIntro;
	bool IsDropIntro() const { return Phase == EZoogiPhase::DropIntro; }
	bool IsDropPick() const { return Phase == EZoogiPhase::DropIntro && DropIntro.Step == EZoogiDropStep::TacPick; }
	bool ConfirmDrop(); // Drop button / Space: physics-fall active player onto PickPoint
	void SetDropPick(const FVector& WorldPoint);
	void BeginDropIntro();
	void TickDropIntro(float Dt);
	void FinishDropIntro(); // hand off to normal playable round (no further cinematic)
	void SkipDropIntro();

	// ---- launch flow (ZoogiLaunchFlowGame.cpp, see ZoogiLaunchFlow.h): opening launches from the pads, respawn to pad
	FZoogiLaunchFlow LaunchFlow;
	bool bLaunchFlow = true;          // -ZoogiNoLaunchFlow: old flow (P1 first from the spawn points)
	int32 LaunchFirstOverride = -1;   // -ZoogiLaunchFirst=N (0-based, 0 = P1) instead of the random first player
	float CraneTimeout = 6.f;         // crane hook bound but NotifyZoogiPlacedOnPad never came: place directly after this
	/** Glove crane (Animations & Powers): a knocked-off Zoogi needs carrying to its pad. Call NotifyZoogiPlacedOnPad when done. */
	FZoogiCarryToPad OnZoogiNeedsCarryToPad;
	void NotifyZoogiPlacedOnPad(AZoogiBall* Zoogi);
	/** Pad camera (Game Camera): pad phase start / end per player (also OnLaunchPadPhaseStarted/Ended <Player> by name on the PlayerController). */
	FZoogiPadPhase OnLaunchPadPhase;
	FTransform GetLaunchPadTransform(int32 Player) const;
	bool IsEpisodeEnergyRunning() const;
	bool IsOnLaunchPad(const AZoogiBall* B) const;
	bool IsParked(const AZoogiBall* B) const;
	FVector GetPadLaunchVelocity(const AZoogiBall* Ball, float AimYaw) const;
	/** Opening landing point for a player: the pad's LandingTarget (else the arena centre), spread sideways by player index so openers don't land on each other. */
	FVector GetPadLandingPoint(int32 PlayerIdx) const;
	bool IsPadAirborne(const AZoogiBall* B) const;
	UFUNCTION(BlueprintCallable, Category = "Zoogi") bool IsBallOnLaunchPad(AZoogiBall* B) const { return IsOnLaunchPad(B); }
	int32 GetPadPhasePlayer() const { return PadPhasePlayer; }
	int32 RoundStartPlayer = 0;       // a round starts when the turn returns to this player (the random first launcher)


	// ---- BATTLE ROYALE (ZoogiRoyale.cpp, see ZoogiRoyale.h): free-for-all real-time ring-out brawl on any arena.
	// Menu MODE "BATTLE ROYALE" (MenuAction Mode 2) or -ZoogiRoyale. Every Zoogi drives at once; lives (stocks), last one in wins.
	bool bRoyaleMode = false;
	FZoogiRoyale Royale;
	float RoyaleFloorZ = 0.f;
	void RoyaleSetMode(bool bOn);
	// ---- TRAINING sandbox (Oct 27, Evan): the royale engine on the Training (Teaser) island - just you unless bots are added
	// from the in-game menu, no eliminations, no surge; controls switch live between BRAWL (move stick / jump / transform)
	// and MARBLE (pull back + release to launch, classic slingshot feel). Menu actions TrainMenu / TrainBot / TrainControls /
	// TrainReset / TrainExit (RoyaleTrainAction).
	bool bRoyaleTraining = false;
	/** b63 (Evan): TRAINING = map select, then a classic match with only you on it (no bots); orbs refill, no match end. */
	bool bSoloTraining = false, bWantSoloTraining = false;
	void ClassicStartTraining();
	int32 RoyaleTrainBots = 0;       // CPU Zoogis besides you (0..3)
	bool bRoyaleMarble = false;      // MARBLE controls (slingshot) instead of BRAWL
	bool bRoyaleTrainPanel = false;  // the training menu is open
	void RoyaleStartTraining();
	void RoyaleTrainAction(const FName& Action, int32 Index);
	/** MARBLE controls: launch the human's Zoogi along WorldDir with PullFrac (0..1) of full power. False when not ready. */
	bool RoyaleMarbleLaunch(const FVector2D& WorldDir, float PullFrac);
	bool RoyaleMarbleReady() const;
	void RoyaleStartMatch();
	void RoyaleTick(float Dt);
	/** Transform (Awaken) a Zoogi: needs a full charge; heavier, faster, harder hits for AwakenSeconds + a shockwave burst. */
	bool RoyaleAwaken(int32 PlayerIdx, const TCHAR* Who);
	/** Controller -> the human's drive this frame (world XY, length <= 1). */
	void RoyaleSetHumanMove(const FVector2D& WorldMove) { Royale.HumanMove = WorldMove.GetClampedToMaxSize(1.f); }
	/** AZoogiBall::OnSphereHit in a royale match: knockback boost + ring-out credit (orbs: credit only). */
	void RoyaleOnBallHit(AZoogiBall* Self, AActor* Other, const FVector& NormalImpulse, const FHitResult& Hit);
	bool IsRoyaleMatch() const { return bRoyaleMode && Phase != EZoogiPhase::Menu && Royale.Step != EZoogiRoyaleStep::Off; }
	/** Camera / HUD focus: the human's Zoogi while it is in, else the leading Zoogi still in (the winner at the end). */
	AZoogiBall* RoyaleFocusBall() const;
	void RoyaleBeginFight();
	void RoyaleCheckOuts();
	void RoyaleRingOut(AZoogiBall* B);
	/** Oct 27: HP damage (bolts / rams); 0 HP destroys the Zoogi (a life lost, credited to By). */
	void RoyaleDamage(int32 Victim, int32 By, int32 Amount, bool bCrit, const FVector& At);
	/** Ring-out and HP-destroy share this: bDestroyed = blown up on the court (HP 0) instead of falling / pushed off. */
	void RoyaleLoseLife(AZoogiBall* B, bool bDestroyed);
	void RoyaleRespawn(int32 Idx);
	void RoyaleFinish();
	void RoyaleEndAwaken(int32 Idx);
	void RoyaleTickCPU(int32 Idx, float Dt);
	FVector RoyaleSpawnSpot(int32 Idx) const;
	/** Off the court (with a small margin) or no floor under the point: CPU edge sense / respawn spots. */
	bool RoyaleDanger(const FVector2D& P) const;
	void RoyaleFrameDump(float Dt);
	// Oct 26 (Marble Clash controls): TRANSFORM ball <-> robot, JUMP, crosshair aim + auto-fire cannons, plasma bolts
	/** Ball <-> robot (ZoogiMorph): any time in the fight, MorphCooldown between taps. */
	bool RoyaleTransform(int32 PlayerIdx, const TCHAR* Who);
	/** A hop (on the ground only, JumpCooldown between hops). */
	bool RoyaleJump(int32 PlayerIdx, const TCHAR* Who);
	/** Controller -> the crosshair ray (camera through the screen centre) this frame. */
	void RoyaleSetHumanAim(const FVector& CamLoc, const FVector& CamDir) { Royale.HumanCamLoc = CamLoc; Royale.HumanCamDir = CamDir; Royale.bHumanAim = true; }
	void RoyaleAimHuman(int32 Idx);
	void RoyaleFire(int32 Idx);
	// Oct 31 (ZoogiRoyaleHero.cpp): the human plays as the new crystal-mech model; bots keep the transform model and leave relics
	void RoyaleHeroStart();
	void RoyaleHeroTick(float Dt);
	void RoyaleRelicDrop(int32 FromBot, const FVector& Near);
	/** Human shot: muzzle from the hero's hand, relic effects. True = the relic consumed the shot (melee / blast, no bolt). */
	bool RoyaleHeroFire(int32 Idx, FVector& Muzzle, const FVector& Dir, float& Power);
	void RoyaleTickBolts(float Dt);
	void RoyaleBoltHit(int32 Victim, const FZoogiRoyaleBolt& Bolt, const FVector& At);
	/** Clear line (world static only) between two Zoogis. */
	bool RoyaleSight(const FVector& From, const FVector& To) const;

	/** Oct 31: -ZoogiAncientTest portal checks run the knock-off rules while the turn flow is held. */
	void TestCheckKnockoffs() { CheckKnockoffs(); }
	/** Oct 31 b46: public feed / popup for helper systems (ZoogiStageShift). */
	void PublicFeed(const FString& Text, const FLinearColor& Color) { AddFeed(Text, Color); }
	void PublicPopup(const FString& Text, const FVector& World, const FLinearColor& Color) { AddPopup(Text, World, Color); }
	void PublicBeginTurn(int32 P) { BeginTurn(P); } // b156: DAILY TRICK SHOT - the next try
	void PublicFinishGame() { FinishGame(); } // b151: RAID ends the match on a win / out of lives
	/** b273 PARTY: one more orb at L (ORB STORM rain, TRICK COURSE reset) */
	class AZoogiOrb* PublicSpawnOrb(const FVector& L, bool bStar);
	/** b108 SKILL LAB: a full fresh set of orbs */
	void LabFreshOrbs();
	/** b90b: a skill that ends the turn without a shot (WOOD SHIELD) */
	void SkillEndsTurn() { SetStage(EZoogiStage::End); }
protected:
	void BeginTurn(int32 PlayerIdx);
	void EndTurn();
	void CheckKnockoffs();
	bool AllAtRest() const;
	void ClearMatch();
	void SpawnOrbs();
	bool OrbSpot(FVector& L) const;
	void RespawnBall(AZoogiBall* Ball);
	void BeginOpeningLaunches();
	/** Pad shot: the pad's GetLaunchVelocity() (stub 30 deg / 1172 without a real pad) turned to the shot's yaw; no ring-out until landing. */
	void FirePadLaunch(AZoogiBall* Ball, const FVector2D& ShotVelocity);
	void TickPadFlights(float Dt);
	/** Launch flow on unless -ZoogiNoLaunchFlow (read from the command line, valid before BeginOpeningLaunches). t782u: elevators play even when launch flow is on. */
	static bool LaunchFlowEnabled();
	/** -ZoogiLaunchFirst=N (0-based, 0 = P1); in-game test drivers (-Zoogi*Test*, -ZoogiClashTour) default to 0; else -1 = random. */
	static int32 ParseLaunchFirst();
	void PlaceOnPad(AZoogiBall* Ball, bool bFX);
	/** Waiting / returned: under the pad deck (collision off). BeginTurn seats the active one on the pad. */
	void ParkUnderPad(AZoogiBall* Ball);
	FVector GetParkLocation(int32 Player) const;
	void SendToPad(AZoogiBall* Ball);
	/** Sphere sweep (ball radius) vs WorldStatic+WorldDynamic for pad arc pre-check and in-flight hit log; ignores pad + all Zoogis. */
	bool PadArcObstacleSweep(const FVector& Start, const FVector& End, FHitResult& OutHit, const AZoogiBall* Self) const;
	void SetPadPhase(int32 Player, bool bOn);
	void TickLaunchFlow(float Dt);
	int32 OpeningNextTurn();
	TArray<bool> OnPad;
	TArray<FVector> PadAnchor;
	int32 PadPhasePlayer = -1;
	FVector FindFreeSpot(const FVector& Desired) const;
	void AddPopup(const FString& Text, const FVector& World, const FLinearColor& Color);
	void AddFeed(const FString& Text, const FLinearColor& Color);
	void CPUSafetyClamp(class AZoogiBall* Ball, FVector2D& V); // Oct 14: no CPU self ring-out (cards mode)
	void FireShot(const FVector2D& Velocity);
	void FinishGame();

	// powers
	void FireInstantPower(AZoogiBall* Ball);
	void OnArmedShot(AZoogiBall* Ball);
	void TickPowers(float Dt);
	void EndPowers();
	void CPUPowerTick();
	TArray<AActor*> LiveTargets(const AZoogiBall* Except) const;
	int32 RicochetsLeft = 0;
	float RicochetTimer = -1.f;
	TWeakObjectPtr<AActor> RicochetFrom;
	TSet<TWeakObjectPtr<AActor>> PowerTouched;
	bool bCPUUsePower = false;

	// CPU
	void PlanCPUShot();
	void TickStages(float Dt);
	void UpdateLockAim();
	void FireLockedShot();
	TWeakObjectPtr<AActor> CPULock;
	bool bCPUPlanned = false;
	bool bCPUPlanDeferred = false;   // build 23 #1: bodies still drifting at turn start - plan when they settle
	bool bReplayDoneThisEnd = false; // build 23 #10: round-end replay already offered for this EndTurn
	FVector CPUTargetPull = FVector::ZeroVector;
	float CPUStartDelay = 1.7f;
	int32 CPUExplPrevN = 0;      // CPU Hotstreak: most targets seen inside the blast this roll
	double HardQuitAt = 0.0;
	bool bShotImpacted = false;  // first contact of the current shot already celebrated     // automation: forced exit time if QuitGame didn't finish
	FVector ShotStart = FVector::ZeroVector;
	float ShotSpeed = 0.f;

	// audio
	void PlayMusic(const TCHAR* Name);
	UPROPERTY() TObjectPtr<class UAudioComponent> MusicComp;
	FString CurrentMusic;

	UPROPERTY() TObjectPtr<UPhysicalMaterial> BallPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> OrbPM;

	// automation
	bool bAuto = false;
	bool bPowerTest = false;
	bool bMenuShot = false;
	float QuitAfter = 0.f;
	float Clock = 0.f;
	int32 ShotsFired = 0;
	float ShotClock = -1.f;
	int32 ShotSeq = 0;
	TSet<FString> ShotsTaken;
};

