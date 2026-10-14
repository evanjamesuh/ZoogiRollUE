// UZoogiProgressionSubsystem (Oct 14, branch `progression`): owns the v1.0 progression profile (FZoogiProgression) with the
// real clock and persistence (Saved/SaveGames/ZoogiProgression.json, atomic write + .bak). The menu branch's
// UZoogiPowerUnlockSubsystem can delegate gems / mastery / unlocks here (Docs/Progression/INTEGRATION.md).
// UZoogiProgressionMatchBridge listens to AZoogiGameMode::OnMatchEvent (read-only) and feeds the award hooks.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiProgression.h"
#include "ZoogiMatchEvents.h"
#include "ZoogiProgressionSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnZoogiProgressionChanged);

UCLASS()
class ZOOGIROLL_API UZoogiProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	static UZoogiProgressionSubsystem* Get(const UObject* WorldContext);

	FZoogiProgression& Core() { return Prog; }
	const FZoogiProgression& Core() const { return Prog; }
	/** Writes the profile (atomic: temp file + move, previous kept as .bak). */
	bool SaveProfile();
	FString ProfilePath() const;

	// match flow (called by the bridge; also usable by Gameplay & UI directly)
	void BeginMatch(EZoogiCPUTier Tier, FName ZoogiId);
	bool HasOpenMatch() const { return bMatchOpen; }
	FZoogiMatchTally& Tally() { return OpenTally; }
	FZoogiMatchRewardResult EndMatch(EZoogiMatchOutcome Outcome);
	const FZoogiMatchRewardResult& LastResult() const { return Last; }
	bool SetFocusElement(EZoogiCardElement E, EZoogiPlayContext Ctx, FString& OutWhy);
	bool UnlockPower(FName PowerId, FString& OutWhy);

	FOnZoogiProgressionChanged OnChanged;
private:
	FZoogiProgression Prog;
	FZoogiMatchTally OpenTally;
	bool bMatchOpen = false;
	FZoogiMatchRewardResult Last;
};

UCLASS()
class ZOOGIROLL_API UZoogiProgressionMatchBridge : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
private:
	void OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag);
	int32 HumanIndex() const;
	int32 TeamOf(int32 PlayerIdx) const;
	TWeakObjectPtr<class AZoogiGameMode> GM;
	FDelegateHandle Handle, CardHandle, PowerHandle;
	void OnCardEffect(int32 Player, FName CardId);
	void OnPowerUsed(int32 Player);
};
