// menu (Oct 14, Evan approved mockups 8:45 AM CT): power unlocks + per-Zoogi equipped power.
// menu-progression: gems, mastery and unlocks are read from / written to UZoogiProgressionSubsystem (ZoogiProgression.json);
// slot "ZoogiUnlocks" only keeps EquippedByZoogi (old UnlockedPowers are migrated once, the placeholder gems are not).
// Animations & Powers read this from their ability code (IsUnlocked / GetEquipped); this file never touches ability code.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "ZoogiPowerUnlockSubsystem.generated.h"

/** Stable power ids. Animations & Powers gate on these. */
// Same guard + inline definition as the kfx-vfx-ap shim (a1d0ed6) so either header can be in a TU; this file replaces the shim.
#ifndef ZOOGI_POWER_IDS_DEFINED
#define ZOOGI_POWER_IDS_DEFINED 1
namespace ZoogiPowerIds
{
	inline const FName Void2TitanMeteor(TEXT("void_2_titan_meteor"));
}
#endif

/** One power entry. Name / desc / art from BuildPowerDefs(); requirement / cost from Content/Data/Progression/Powers.csv. */
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
	UPROPERTY() TSet<FName> UnlockedPowers;             // legacy: migrated into progression on load, then emptied
	UPROPERTY() TMap<FName, FName> EquippedByZoogi;   // ZoogiId -> PowerId
	UPROPERTY() TMap<FName, int32> Stats;             // legacy placeholder, unused
	UPROPERTY() int32 Gems = 0;                       // legacy placeholder, unused (gems: progression wallet)
	UPROPERTY() int32 Version = 1;
};

DECLARE_MULTICAST_DELEGATE(FOnZoogiUnlocksChanged);
using FZoogiOnUnlocksChanged = FOnZoogiUnlocksChanged; // name used by the kfx-vfx-ap shim

UCLASS()
class ZOOGIROLL_API UZoogiPowerUnlockSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static const TCHAR* SlotName; // "ZoogiUnlocks"

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsUnlocked(FName PowerId) const;
	/** Checks CanUnlock, pays (gems unless the stat requirement is met), marks unlocked and saves. */
	bool Unlock(FName PowerId);
	bool CanUnlock(FName PowerId, FText& Why) const;
	/** Shim signature (kfx-vfx-ap a1d0ed6). */
	bool CanUnlock(FName PowerId) const { FText Why; return CanUnlock(PowerId, Why); }
	/** NAME_None = the Zoogi's default power. -ZoogiEquip=<PowerId> (capture / test runs, as in the shim) forces that power for every Zoogi. */
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
	/** -ZoogiAuto / -ZoogiBattleTest / -ZoogiEpisodeP5Test / automation: the player's profile never shapes decks (parity). */
	static bool IsTestRun();

	FOnZoogiUnlocksChanged OnUnlocksChanged;

	static UZoogiPowerUnlockSubsystem* Get(const UObject* WorldContext);

private:
	class UZoogiProgressionSubsystem* Prog() const;
	FDelegateHandle ProgHandle;
	void Save();
	UPROPERTY() TObjectPtr<UZoogiUnlockSave> Data;
	TArray<FZoogiPowerDef> Defs;
	bool bUnlockAll = false;
};
