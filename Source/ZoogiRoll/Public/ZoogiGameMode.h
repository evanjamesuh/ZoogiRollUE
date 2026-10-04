#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZoogiTypes.h"
#include "ZoogiArtifacts.h"
#include "ZoogiGameMode.generated.h"

class AZoogiBall;
class AZoogiOrb;
class AZoogiArena;
class AZoogiClone;
class UPhysicalMaterial;

/** Coarse match state. Aiming covers Draw / Awaken / Battle aim, Rolling the launched shot, Resolving the Resolve phase. */
UENUM(BlueprintType)
enum class EZoogiPhase : uint8
{
	Menu,
	Aiming,
	Rolling,
	Resolving,
	GameOver
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
	constexpr float DrawCPU = 0.9f;
	constexpr float AwakenCPU = 0.9f;        // CPU decides this long into Awaken
	constexpr float AwakenBeat = 0.75f;      // after Infuse / Guard: activation beat before Battle (placeholder anime plays here)
	constexpr float AwakenSaveBeat = 0.35f;
	constexpr float CPULockAt = 0.35f;       // CPU locks this long into Battle aim
	constexpr float CPUMain = 1.2f;          // ...and confirms here
	constexpr float BattleSwing = 0.85f;     // cinematic camera: lock-on swing
	constexpr float BattleHold = 0.5f;
	constexpr float CPUBattleHold = 0.3f;
	constexpr float PowerUpTactical = 0.45f; // tactical camera: short power-up beat
	constexpr float PowerUpInfused = 0.8f;   // longer when an artifact is infused
	constexpr float LongLockDist = 1100.f;   // locks farther than this get the two-beat Battle camera (cinematic style)
	constexpr float TargetBeat = 0.55f;
	constexpr float LockImpactSpeed = 650.f; // cm/s the shot still has when it reaches the target (plus 0.25 x distance)
	constexpr float SettleTime = 0.8f;       // everything at rest this long -> Resolve
	constexpr float ResolveTimeout = 12.f;   // after launch: force Resolve (bodies are stopped where they are)
	constexpr float ResolveMin = 0.7f;       // Resolve phase shows at least this long
	constexpr float EndBeat = 0.35f;
	constexpr float MaxCutDeg = 30.f;        // angled aiming: cut nudges up to +-30 deg
	constexpr float CutStepDeg = 2.f;
	constexpr float CritImpactFrac = 0.75f;  // crit: powered hit keeping >= 75% of its launch speed at contact...
	constexpr float CritMaxCutDeg = 8.f;     // ...and hitting within 8 deg of head-on
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
	void StartMatch();
	void ReturnToMenu();

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

	// ---- angled aiming (clickable): free floor aim, cut nudges, power steps, bank preview
	bool bFreeAim = false;
	FVector FreeAimPoint = FVector::ZeroVector;
	float AimCutDeg = 0.f;
	/** 0 = auto (distance-scaled), 1 soft, 2 medium, 3 full. */
	int32 AimPowerStep = 0;
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
	float AwakenDoneAt = 0.f;
	FString AwakenChoiceText;
	int32 TurnCounter = 0;
	EZoogiArtifact PickedArtifact() const;
	bool AwakenInfuse(int32 Pick);
	bool AwakenGuard(int32 Pick);
	bool AwakenSave();
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
	FVector PullCenter = FVector::ZeroVector;
	float PullLeft = 0.f, PullAccel = 0.f, PullRadius = 0.f;
	int32 PullCredit = -1;
	float ResolveStart = 0.f;
	/** Resolve banner ("P1 +100 KO   P2 -75") and per-player deltas for the HUD. */
	FString ResolveSummary;
	TArray<int32> ResolveDeltas;
	void EmitAnime(FName Event, AZoogiBall* Who, EZoogiArtifact A, AActor* Focus, const FVector& Where, float Magnitude, bool bFreeze, int32 TargetPlayer = -1);
	/** Titan Core (infused, while shooting) / guard mass. */
	void RefreshArtifactMass(AZoogiBall* B, bool bShooting);
	TArray<TWeakObjectPtr<AZoogiBall>> FrozenBalls;
	/** Locked target (rival Zoogi or orb). Soft = picked during Standby (ring only, camera stays wide). */
	TWeakObjectPtr<AActor> TargetLock;
	bool bSoftLock = false;
	float LockTime = 0.f;
	int32 LockSerial = 0;
	void SetStage(EZoogiStage S);
	static const TCHAR* StageName(EZoogiStage S);
	bool IsValidTarget(const AActor* A) const;
	/** Live targets for the active Zoogi, nearest first. */
	TArray<AActor*> TargetCandidates() const;
	void LockOn(AActor* Target, bool bSoft);
	void ClearLock();
	void CycleLock(bool bSoft);
	/** Space / click on the locked target during Main: start the Battle stage. */
	bool CommitLock();
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
	/** Aim pull after Lock-On snapping (what the shot will really use). */
	FVector GetEffectivePull() const;
	/** Lock-On target for the current pull, if any. */
	AActor* GetLockTarget() const;
	/** The active shooter touched something (hit or Bolt overlap). */
	void OnShooterContact(AZoogiBall* Shooter, AActor* Other);
	UPROPERTY() TArray<TObjectPtr<AZoogiClone>> Clones;

	// ---- automation / screenshots (command line: -ZoogiAuto -ZoogiPlayers=4 -ZoogiArena=1 -ZoogiShots=<dir> -ZoogiQuit=<sec> -ZoogiMenuShot)
	FString ShotDir;
	void TakeShot(const FString& Name);
	void TakeShotDelayed(const FString& Name, float Delay);
	TMap<FString, float> DelayedShots;

protected:
	void BeginTurn(int32 PlayerIdx);
	void EndTurn();
	void CheckKnockoffs();
	bool AllAtRest() const;
	void ClearMatch();
	void SpawnOrbs();
	bool OrbSpot(FVector& L) const;
	void RespawnBall(AZoogiBall* Ball);
	FVector FindFreeSpot(const FVector& Desired) const;
	void AddPopup(const FString& Text, const FVector& World, const FLinearColor& Color);
	void AddFeed(const FString& Text, const FLinearColor& Color);
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
