#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZoogiTypes.h"
#include "ZoogiGameMode.generated.h"

class AZoogiBall;
class AZoogiOrb;
class AZoogiArena;
class AZoogiClone;
class UPhysicalMaterial;

UENUM(BlueprintType)
enum class EZoogiPhase : uint8
{
	Menu,
	Aiming,
	Rolling,
	GameOver
};

/** Duel-style turn stages, each with its own camera profile (see ZoogiPlayerController.cpp, ZoogiCamTune). */
UENUM(BlueprintType)
enum class EZoogiStage : uint8
{
	Draw,     // hero shot introduction of the active Zoogi
	Standby,  // calm overview; a click soft-locks a target
	Main,     // tactical over-the-shoulder; lock / switch / Tab-cycle targets, Space commits
	Battle,   // dramatic lock-on cinematic, short hold, then the shot fires
	End       // follow the shot, then aerial resolution while everything settles
};

/** Stage timings (seconds). */
namespace ZoogiStageTune
{
	constexpr float DrawHuman = 1.4f;
	constexpr float DrawCPU = 1.0f;
	constexpr float StandbyMin = 0.9f;      // human: earliest Standby -> Main after a soft lock
	constexpr float StandbyMax = 2.6f;      // human: Standby -> Main even without a target
	constexpr float SoftLockBeat = 0.45f;   // soft lock shows this long before Main swings in
	constexpr float CPUStandby = 0.8f;
	constexpr float CPULockAt = 0.25f;      // CPU locks this long into Main
	constexpr float CPUMain = 1.1f;
	constexpr float BattleSwing = 0.85f;    // cinematic lock-on swing
	constexpr float BattleHold = 0.5f;
	constexpr float CPUBattleHold = 0.3f;
	constexpr float LockImpactSpeed = 650.f; // cm/s the shot still has when it reaches the target (plus 0.25 x distance)
}

USTRUCT(BlueprintType)
struct FZoogiPlayerSetup
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ZoogiIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCPU = false;
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

	// ---- targeting + turn stages (ZoogiTargeting.cpp)
	UPROPERTY(BlueprintReadOnly) EZoogiStage Stage = EZoogiStage::Draw;
	float StageTime = 0.f;
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
