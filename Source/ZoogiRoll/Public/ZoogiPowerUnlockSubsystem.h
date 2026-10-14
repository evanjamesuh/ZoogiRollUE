// menu (Oct 14, Evan approved mockups 8:45 AM CT): power unlocks + per-Zoogi equipped power, saved in slot "ZoogiUnlocks".
// Animations & Powers read this from their ability code (IsUnlocked / GetEquipped); this file never touches ability code.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "ZoogiPowerUnlockSubsystem.generated.h"

/** Stable power ids. Animations & Powers gate on these. */
namespace ZoogiPowerIds
{
	static const FName Void2TitanMeteor(TEXT("void_2_titan_meteor"));
}

/** One power entry. ALL requirement / cost numbers live in ZoogiPowerUnlockSubsystem.cpp::BuildPowerDefs() (placeholders). */
USTRUCT()
struct FZoogiPowerDef
{
	GENERATED_BODY()
	UPROPERTY() FName PowerId;
	UPROPERTY() FText Name;
	UPROPERTY() FName Type;            // Void, Shock, Frost, Fire ...
	UPROPERTY() int32 Tier = 1;
	UPROPERTY() FText Desc;
	UPROPERTY() bool bDefaultUnlocked = false;
	UPROPERTY() int32 GemCost = 0;                 // 0 = no gem route
	UPROPERTY() FName RequirementStat;             // e.g. void_mastery (NAME_None = no stat route)
	UPROPERTY() int32 RequirementValue = 0;
	UPROPERTY() FString CardArt;                   // Content/Data/CardArt/<CardArt>.png
};

UCLASS()
class ZOOGIROLL_API UZoogiUnlockSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() TSet<FName> UnlockedPowers;
	UPROPERTY() TMap<FName, FName> EquippedByZoogi;   // ZoogiId -> PowerId
	UPROPERTY() TMap<FName, int32> Stats;             // placeholder progression stats (void_mastery ...)
	UPROPERTY() int32 Gems = 340;                     // placeholder currency until the economy exists
	UPROPERTY() int32 Version = 1;
};

DECLARE_MULTICAST_DELEGATE(FOnZoogiUnlocksChanged);

UCLASS()
class ZOOGIROLL_API UZoogiPowerUnlockSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static const TCHAR* SlotName; // "ZoogiUnlocks"

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool IsUnlocked(FName PowerId) const;
	/** Checks CanUnlock, pays (gems unless the stat requirement is met), marks unlocked and saves. */
	bool Unlock(FName PowerId);
	bool CanUnlock(FName PowerId, FText& Why) const;
	/** NAME_None = the Zoogi's default power. */
	FName GetEquipped(FName ZoogiId) const;
	/** Fails when PowerId is locked. NAME_None clears back to the default power. */
	bool SetEquipped(FName ZoogiId, FName PowerId);

	const TArray<FZoogiPowerDef>& GetAllPowers() const { return Defs; }
	const FZoogiPowerDef* FindPower(FName PowerId) const;
	/** Human-readable requirement, e.g. "Void mastery 5 or 300 gems". */
	FText RequirementText(FName PowerId) const;
	int32 GetStat(FName Stat) const;
	int32 GetGems() const;
	/** -ZoogiUnlockAll dev cheat: everything reads as unlocked (not written to the save). */
	bool IsUnlockAllCheat() const { return bUnlockAll; }

	FOnZoogiUnlocksChanged OnUnlocksChanged;

	static UZoogiPowerUnlockSubsystem* Get(const UObject* WorldContext);

private:
	void Save();
	UPROPERTY() TObjectPtr<UZoogiUnlockSave> Data;
	TArray<FZoogiPowerDef> Defs;
	bool bUnlockAll = false;
};
