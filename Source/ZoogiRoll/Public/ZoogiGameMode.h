#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZoogiTypes.h"
#include "ZoogiGameMode.generated.h"

class AZoogiBall;
class AZoogiOrb;
class AZoogiArena;
class UPhysicalMaterial;

UENUM(BlueprintType)
enum class EZoogiPhase : uint8
{
	Menu,
	Aiming,
	Rolling,
	GameOver
};

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

	// ---- automation / screenshots (command line: -ZoogiAuto -ZoogiPlayers=4 -ZoogiArena=1 -ZoogiShots=<dir> -ZoogiQuit=<sec> -ZoogiMenuShot)
	FString ShotDir;
	void TakeShot(const FString& Name);

protected:
	void BeginTurn(int32 PlayerIdx);
	void EndTurn();
	void CheckKnockoffs();
	bool AllAtRest() const;
	void ClearMatch();
	void SpawnOrbs();
	void RespawnBall(AZoogiBall* Ball);
	FVector FindFreeSpot(const FVector& Desired) const;
	void AddPopup(const FString& Text, const FVector& World, const FLinearColor& Color);
	void FireShot(const FVector2D& Velocity);
	void FinishGame();

	// CPU
	void PlanCPUShot();
	bool bCPUPlanned = false;
	FVector CPUTargetPull = FVector::ZeroVector;
	float CPUStartDelay = 1.7f;
	FVector ShotStart = FVector::ZeroVector;
	float ShotSpeed = 0.f;

	UPROPERTY() TObjectPtr<UPhysicalMaterial> BallPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> OrbPM;

	// automation
	bool bAuto = false;
	bool bMenuShot = false;
	float QuitAfter = 0.f;
	float Clock = 0.f;
	int32 ShotsFired = 0;
	float ShotClock = -1.f;
	int32 ShotSeq = 0;
	TSet<FString> ShotsTaken;
};
