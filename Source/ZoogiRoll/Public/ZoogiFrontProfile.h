// clash-menu (Oct 17, milestone 1): local front-end profile for the Clash-style menu shell (level, XP, gold, gems,
// trophies, battle pass, chest slots, guest name). Saved next to ZoogiProgression.json as <profile>.frontend.json
// (same atomic temp + move write with a .bak), so -ZoogiProfile=<name> test profiles keep their own front-end file.
// Local only: nothing here is a real-money or online value. The gem count shown on the menu is this soft local
// balance; the real gem ledger (UZoogiProgressionSubsystem wallet, purchases) is untouched until a backend decision.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZoogiFrontProfile.generated.h"

struct ZOOGIROLL_API FZoogiFrontProfileData
{
	static constexpr int32 CurrentSchema = 1;
	int32 SchemaVersion = CurrentSchema;
	FString GuestName;            // PLAYER-###### (generated once)
	int32 Level = 1;
	int32 XP = 0;                 // XP inside the current level
	int32 Gold = 600;
	int32 Gems = 100;
	int32 Trophies = 0;
	int32 BestTrophies = 0;
	int32 BattlePassLevel = 1;
	int32 BattlePassXP = 0;       // 0 / 100 per level
	int32 Wins = 0;
	int32 Battles = 0;
	TArray<FName> Chests;         // 3 slots; NAME_None = empty ("Win a battle")
	bool bCardsNew = true;        // red dot on the Cards tab

	FString ToJsonString() const;
	bool FromJsonString(const FString& S, FString& OutError);
};

UCLASS()
class ZOOGIROLL_API UZoogiFrontProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	static UZoogiFrontProfileSubsystem* Get(const UObject* WorldContext);

	const FZoogiFrontProfileData& Data() const { return D; }
	FZoogiFrontProfileData& Mutable() { return D; }
	bool Save();
	FString ProfilePath() const;

	/** XP needed to finish Level (level 1 = 20). */
	static int32 XPForLevel(int32 Level);
	/** Trophies needed to enter arena index A (0-based: Arena 1 = 0, Arena 2 = 300, ...). */
	static int32 ArenaTrophies(int32 A) { return FMath::Max(0, A) * 300; }
	/** 0-based arena index the trophy count has reached. */
	static int32 ArenaForTrophies(int32 Trophies);
	static constexpr int32 NumTrophyArenas = 7;
	static constexpr int32 BattlePassXPPerLevel = 100;

private:
	FZoogiFrontProfileData D;
};
