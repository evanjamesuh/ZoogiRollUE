// clash-menu (Oct 17): local front-end profile for the Clash-style menu (level, XP, gold, trophies, battle pass, chest
// slots, card copies, 3 decks, daily deals, last match result, guest name). Saved next to ZoogiProgression.json as
// <profile>.frontend.json (atomic temp + move, .bak), so -ZoogiProfile=<name> test profiles keep their own file.
// Gems are NOT stored here: the one gem balance is the existing gem ledger in the progression save
// (UZoogiProgressionSubsystem wallet); the menu shows whatever it holds and every gem purchase spends from it. No menu
// grant. Real-money gem packs need the store backend and are never faked.
// M2: UZoogiFrontRewardBridge reads real match results (AZoogiGameMode::OnMatchEvent, read-only) and applies trophies,
// XP, gold, battle-pass XP and a chest in a free slot.
// M3 (DETAILED PASS 2): player tag, battle stats, Battle Log (last 25 real matches), nickname change (10 gold), daily
// chest stock (Royal 3 / Epic 1), spec gold + gem packs, and match replays: during real matches the bridge samples every
// Zoogi's position + rotation and every orb's position at 10 Hz, read-only from the existing actors (no physics or
// gameplay change), and saves the last 10 under Saved/ZoogiReplays/<profile>/ (zlib, capped at 20 minutes per match).
// Dev test flags (compiled out of Shipping, never on by default): -ZoogiDevGrant=<gems> (one-time ledger grant, source
// "dev.grant", only together with a -ZoogiProfile=<test profile>) and -ZoogiDevForceWin (the front-end rewards treat the
// match as a player win; the battle log marks it DEV).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "ZoogiCards.h"
#include "ZoogiMatchEvents.h"
#include "ZoogiFrontProfile.generated.h"

struct FZoogiFrontChestSlot
{
	FName Type;              // wooden / royal / epic; None = empty slot ("Win a battle")
	int64 UnlockAtUtc = 0;   // 0 = locked, not started; > 0 = unix seconds when it can be opened
};

struct FZoogiFrontDeal
{
	FName CardId;
	int32 Count = 1;
	int32 Price = 0;         // gold
	bool bFree = false;
	bool bBought = false;
};

struct FZoogiFrontReward
{
	FName Kind;              // gold / card / trophies / xp / bp / chest / level
	FName Id;                // card id / chest type
	int32 Amount = 0;
};

struct FZoogiFrontMatchResult
{
	bool bPending = false;   // shown once on Home after the match
	int32 Outcome = 0;       // 0 win, 1 loss, 2 draw, 3 left the match
	int32 Trophies = 0, Gold = 0, XP = 0, BP = 0;
	FName Chest;
	bool bSlotsFull = false;
	int32 NewLevel = 0;      // > 0 = levelled up to this
};

/** Trophy Road milestone / Battle Pass reward: gold, chest (Id = wooden/royal/epic) or hero (N copies of one owned card). */
struct FZoogiFrontRoadReward
{
	int32 Trophies = 0;      // Trophy Road only
	FName Kind;              // gold / chest / hero
	FName Chest;
	int32 Amount = 0;
};

/** One Battle Log row (real matches only). */
struct FZoogiFrontBattle
{
	int64 Utc = 0;             // match end (unix s)
	int32 Outcome = 0;         // 0 win, 1 loss, 2 draw, 3 left
	FString Opponent;          // e.g. "TAVI (CPU)"
	int32 Trophies = 0;        // trophy change
	int32 MyScore = 0, OppScore = 0; // shown as the crowns column (Zoogi Roll scores points, not crowns)
	int32 Duration = 0;        // seconds
	FString ReplayId;          // file name under Saved/ZoogiReplays/<profile>/ ("" = none)
	uint8 Arena = 0;
	bool bDev = false;         // -ZoogiDevForceWin result
};

/** A recorded match, loaded for the Replay viewer. Positions are world cm; rotations degrees. */
struct ZOOGIROLL_API FZoogiReplay
{
	uint8 Arena = 0;
	float Radius = 1000.f;     // knock-off radius of the arena (open edge)
	FVector2f Center = FVector2f::ZeroVector;
	float Hz = 10.f;
	TArray<FString> Names;     // per Zoogi
	TArray<FLinearColor> Colors;
	TArray<int32> Players;
	int32 NumOrbs = 0;
	TArray<float> Times;       // per frame (s from match start)
	TArray<float> Z;           // frame * NumZ * 6: x y z pitch yaw roll
	TArray<uint8> ZFlags;      // frame * NumZ: 1 = out
	TArray<float> O;           // frame * NumOrbs * 2: x y
	TArray<uint8> OFlags;      // frame * NumOrbs: 1 = out, 2 = star
	int32 NumZ() const { return Names.Num(); }
	int32 NumFrames() const { return Times.Num(); }
	float Duration() const { return Times.Num() ? Times.Last() : 0.f; }
};

struct FZoogiFrontChestDef
{
	FName Id; const TCHAR* Name; int32 GemCost; int32 UnlockSec; int32 Cards; int32 GoldMin, GoldMax; int32 WCommon, WRare, WEpic;
};

struct ZOOGIROLL_API FZoogiFrontProfileData
{
	static constexpr int32 CurrentSchema = 3;
	int32 SchemaVersion = CurrentSchema;
	FString GuestName;            // PLAYER-###### (generated once)
	int32 Level = 1;
	int32 XP = 0;                 // XP inside the current level
	int32 Gold = 600;
	int32 Trophies = 0;
	int32 BestTrophies = 0;
	int32 BattlePassLevel = 1;
	int32 BattlePassXP = 0;       // 0 / 100 per level
	int32 Wins = 0;
	int32 Battles = 0;
	int32 Losses = 0, Draws = 0, ThreeCrownWins = 0, WinStreak = 0, BestStreak = 0; // schema 3
	FString Tag;                   // player tag, 7 chars (shown as #XXXXXXX), generated once
	TArray<FZoogiFrontBattle> BattleLog; // newest first, last 25
	FString ChestStockDay;         // daily reset the chest stock belongs to
	TMap<FName, int32> ChestBought; // chests bought today (Royal: 3 a day, Epic: 1 a day)
	TArray<FZoogiFrontChestSlot> Chests; // 3 slots
	int32 ChestCycle = 0;
	bool bCardsNew = true;        // red dot on the Cards tab
	TSet<int32> TrophyClaimed;     // Trophy Road milestones claimed (by trophy value); 0 starts done
	TSet<int32> PassFreeClaimed, PassPremiumClaimed; // Battle Pass tiers claimed (1..35)
	bool bPassPremium = false;
	int64 SeasonEndUtc = 0;        // Season 8 end (unix s), set once: 28d 11h after the profile was made
	TMap<FName, int32> CardCopies; // extra copies collected (shop, chests); an owned card always counts at least 1
	TArray<TArray<FName>> Decks;   // Deck 1/2/3
	int32 ActiveDeck = 0;
	FString DealsDay;              // reset boundary the deals were rolled for (yyyymmdd)
	TArray<FZoogiFrontDeal> Deals;
	FZoogiFrontMatchResult LastMatch;

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

	// ---- gems: the progression save's gem ledger (one balance, no menu grant)
	int32 Gems() const;
	bool SpendGems(int32 Amount, FName Sink);

	// ---- levels / arenas
	static int32 XPForLevel(int32 Level);
	static int32 ArenaTrophies(int32 A) { return FMath::Max(0, A) * 300; }
	static int32 ArenaForTrophies(int32 Trophies);
	static constexpr int32 NumTrophyArenas = 7;
	static constexpr int32 BattlePassXPPerLevel = 100;

	// ---- cards
	static const TArray<FZoogiCardDef>& CardDefs();
	static const FZoogiCardDef* FindCard(FName Id);
	static TArray<FName> StarterDeck();
	/** 0 Common (neutral, cost 1), 1 Rare (neutral, cost 2), 2 Epic (element / power cards). Proposal until a rarity column exists. */
	static int32 RarityOf(const FZoogiCardDef& C);
	static const TCHAR* RarityName(int32 R);
	bool IsCardOwned(FName Id) const;
	FString LockText(FName Id) const;
	int32 Copies(FName Id) const;
	/** Display level from total copies (cumulative 1, 3, 7, 17, 37, 87, 187); OutInto / OutNeed = progress inside the level. */
	static int32 CardLevel(int32 Copies, int32& OutInto, int32& OutNeed);
	void AddCopies(FName Id, int32 N);

	// ---- decks (Deck 1/2/3; the active one is mirrored into the progression saved deck the matches use)
	static constexpr int32 DeckSize = 15;
	static constexpr int32 MaxCopiesInDeck = 2;
	const TArray<FName>& Deck(int32 I) const;
	void SetDeck(int32 I, const TArray<FName>& Cards);
	void SetActiveDeck(int32 I);

	// ---- daily deals
	void EnsureDeals();
	FTimespan TimeToDealsReset() const;
	bool BuyDeal(int32 I, FString& OutWhy);

	// ---- chests
	static const FZoogiFrontChestDef* FindChest(FName Id);
	static const TArray<FZoogiFrontChestDef>& ChestDefs();
	static int64 NowUtc();
	int32 ChestSeconds(FName Type) const;   // honours -ZoogiChestFast (minutes -> seconds) for capture runs
	bool BuyChest(FName Type, TArray<FZoogiFrontReward>& Out, FString& OutWhy);
	/** Chests left to buy today: -1 = unlimited (Wooden), Royal 3 / Epic 1 per daily reset. */
	int32 ChestStockLeft(FName Type) const;
	bool AnyUnlocking() const;
	bool StartUnlock(int32 Slot, FString& OutWhy);
	int64 SecondsLeft(int32 Slot) const;  // -1 = not started
	bool IsReady(int32 Slot) const;
	int32 OpenNowCost(int32 Slot) const;
	bool OpenChest(int32 Slot, bool bPayGems, TArray<FZoogiFrontReward>& Out, FString& OutWhy);

	// ---- gold packs (bought with gems)
	struct FGoldPack { int32 Gold; int32 Gems; const TCHAR* Name; const TCHAR* Tag; };
	static const TArray<FGoldPack>& GoldPacks();
	bool BuyGold(int32 I, FString& OutWhy);
	/** Real-money gem packs (DETAILED PASS 2 list). They need the platform store: shown, never charged or granted. */
	/** Gem packs straight from Content/Data/Progression/GemPackages.csv (the progression config): gems + bonus_gems, price_usd.
	 *  Only the display name and the Popular / Best Value tag are menu-side (by package id); the csv has no column for them. */
	struct FGemPack { FName Id; int32 Gems = 0; float USD = 0.f; FString Name; FString Tag; };
	TArray<FGemPack> GemPacks() const;

	// ---- profile / settings (DETAILED PASS 2)
	static constexpr int32 NicknameCost = 10;
	/** 3-16 letters, digits, space, - or _ ; costs NicknameCost gold. */
	bool ChangeNickname(const FString& NewName, FString& OutWhy);
	int32 CardsFound() const;
	void CardLevelStats(float& OutAvg, int32& OutMax) const;

	// ---- replays (written by UZoogiFrontRewardBridge during real matches)
	static constexpr int32 MaxReplays = 10;
	static constexpr int32 MaxBattleLog = 25;
	FString ReplayDir() const;
	bool HasReplay(const FString& Id) const;
	bool LoadReplay(const FString& Id, FZoogiReplay& Out) const;

	// ---- Trophy Road (0-5000, DETAILED PASS 1 table) / arena gates (Arena N+1 at N*300; Arena 2 = 300 per the spec)
	static const TArray<FZoogiFrontRoadReward>& TrophyRoad();
	bool IsRoadClaimed(int32 I) const;
	bool IsRoadClaimable(int32 I) const;
	bool ClaimRoad(int32 I, TArray<FZoogiFrontReward>& Out, FString& OutWhy);
	int32 RoadClaimableCount() const;

	// ---- Battle Pass: Season 8, 35 tiers, 100 points per tier; free + premium (500 gems from the ledger)
	static constexpr int32 PassSeason = 8;
	static constexpr int32 PassTiers = 35;
	static constexpr int32 PassPremiumGems = 500;
	static const FZoogiFrontRoadReward& PassReward(int32 Tier, bool bPremium);
	bool IsPassClaimed(int32 Tier, bool bPremium) const;
	bool IsPassClaimable(int32 Tier, bool bPremium) const;
	bool ClaimPass(int32 Tier, bool bPremium, TArray<FZoogiFrontReward>& Out, FString& OutWhy);
	int32 PassClaimableCount() const;
	bool UnlockPremium(FString& OutWhy);
	int64 SeasonSecondsLeft() const;
	FName HeroCardFor(int32 Salt) const;

	// ---- match results (UZoogiFrontRewardBridge)
	void ApplyMatch(int32 Outcome, const FZoogiFrontBattle* Info = nullptr);

private:
	FZoogiFrontProfileData D;
	TArray<FZoogiFrontReward> PayRoadReward(const FZoogiFrontRoadReward& R, int32 Salt);
	void InitDecks();
	TArray<FZoogiFrontReward> RollChest(FName Type, FRandomStream& R);
	TArray<FName> OwnedCards() const;
};

/** Reads real match results (read-only on AZoogiGameMode::OnMatchEvent) and pays the front-end rewards. */
UCLASS()
class ZOOGIROLL_API UZoogiFrontRewardBridge : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
private:
	void OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag);
	int32 PlayerIndex() const;
	FZoogiFrontBattle MakeBattle(int32 Outcome) const;
	// replay recorder (read-only sampling of the existing actors on a 10 Hz world timer)
	void StartRecording();
	void SampleFrame();
	FString StopRecording(bool bSave);
	TWeakObjectPtr<class AZoogiGameMode> GM;
	FDelegateHandle Handle;
	bool bOpen = false;
	int32 Player = -1;
	double MatchT0 = 0.0;
	FTimerHandle RecTimer;
	TArray<TWeakObjectPtr<class AZoogiBall>> RecBalls;
	TArray<TWeakObjectPtr<class AZoogiOrb>> RecOrbs;
	TArray<uint8> RecFrames;
	int32 RecCount = 0;
	bool bRecTruncated = false;
};
