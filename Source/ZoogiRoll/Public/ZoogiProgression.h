// Zoogi Roll v1.0 progression + economy core (Oct 14, Evan's decisions EVAN_DECISIONS_2026-10-14.md; branch `progression`).
// Plain C++ (no UObject / world): the currency service, mastery, match rewards + daily cap/reset, power unlocks, Zoogi Affinity,
// achievements, titles and arena cosmetics, all on one sync-ready profile. UZoogiProgressionSubsystem owns one FZoogiProgression,
// supplies the real clock and the file; automation tests drive this class directly with injected times.
// Rule Zero: nothing here touches physics, card rules, the CPU or the battle sim (cards-off parity e3f55c2b).
// All balance values come from Content/Data/Progression/*.csv (FZoogiProgressionConfig); the code only has safe fallbacks.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "ZoogiCards.h" // EZoogiElement

class FJsonObject;

/** Read-only gameplay notifications for progression (Oct 14). Broadcast from the card-effect and power code; no listener =
 *  no-op. Listeners must never change game state (Rule Zero / parity). */
namespace ZoogiProgressHooks
{
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCardEffectUsed, int32 /*PlayerIndex*/, FName /*CardId*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPowerUsed, int32 /*PlayerIndex*/);
	ZOOGIROLL_API FOnCardEffectUsed& CardEffectUsed();
	ZOOGIROLL_API FOnPowerUsed& PowerUsed();
}

enum class EZoogiGemBucket : uint8 { Earned, Purchased };
enum class EZoogiSpendOrder : uint8 { EarnedFirst, PurchasedFirst };
enum class EZoogiTxnResult : uint8 { Applied, Duplicate, Invalid, Insufficient };
enum class EZoogiCPUTier : uint8 { Easy, Normal, Hard, Expert };
enum class EZoogiMatchOutcome : uint8 { Win, Loss, Draw, Forfeit, Abandoned };
/** Where the player is when they try to change the Focus Element (Evan Q8: changeable in Standby only). */
enum class EZoogiPlayContext : uint8 { Menu, Standby, Battle };

namespace ZoogiProgress
{
	ZOOGIROLL_API const TCHAR* TxnResultName(EZoogiTxnResult R);
	ZOOGIROLL_API const TCHAR* TierName(EZoogiCPUTier T);
	ZOOGIROLL_API bool ParseTier(const FString& S, EZoogiCPUTier& Out);
	/** Name-based GUID (MD5 of the key): the same logical grant always gets the same transaction id (idempotent system grants). */
	ZOOGIROLL_API FGuid DeterministicId(const FString& Key);
	/** Most recent local reset boundary (ResetHour:00 local) at or before Local. */
	ZOOGIROLL_API FDateTime ResetBoundary(const FDateTime& Local, int32 ResetHour);
	/** Reads a '#'-commented CSV (header row) into rows of column -> value. */
	ZOOGIROLL_API bool ReadCsvRows(const FString& Text, TArray<TMap<FString, FString>>& Out, FString& OutError);
}

// ------------------------------------------------------------------ currency (P2)

struct ZOOGIROLL_API FZoogiGemTxn
{
	FGuid TxnId;               // unique; idempotency key (purchases: derived from the platform transaction id)
	FString Kind;              // grant | spend | purchase
	FName Source;              // e.g. match / mastery.level / achievement / unlock.power / store.apple
	int32 EarnedDelta = 0;
	int32 PurchasedDelta = 0;
	int32 EarnedAfter = 0;
	int32 PurchasedAfter = 0;
	FDateTime Utc;             // per-record timestamp (sync)
	FString ExternalId;        // platform transaction id / match id (sync + support)
};

/** Centralized gem wallet: earned vs purchased buckets, no negatives, ledger with unique ids, idempotent by transaction id. */
class ZOOGIROLL_API FZoogiWallet
{
public:
	int32 EarnedCap = 999999;  // UI safety cap on the earned bucket only (purchased gems are never clipped)
	EZoogiSpendOrder SpendOrder = EZoogiSpendOrder::EarnedFirst;

	int32 Earned() const { return EarnedV; }
	int32 Purchased() const { return PurchasedV; }
	int32 Total() const { return EarnedV + PurchasedV; }
	/** Amount must be > 0. A TxnId already in the ledger returns Duplicate and changes nothing. */
	EZoogiTxnResult Grant(const FGuid& TxnId, EZoogiGemBucket Bucket, int32 Amount, FName Source, const FDateTime& Utc, const FString& ExternalId = FString(), int32* OutApplied = nullptr);
	/** All-or-nothing; never goes negative. Drains buckets in SpendOrder. */
	EZoogiTxnResult Spend(const FGuid& TxnId, int32 Amount, FName Sink, const FDateTime& Utc, const FString& ExternalId = FString());
	bool HasTxn(const FGuid& TxnId) const { return Ids.Contains(TxnId); }
	const TArray<FZoogiGemTxn>& Ledger() const { return LedgerV; }
	/** Replays the ledger: balances must match and never dip below zero; ids unique. */
	bool Verify(FString& OutError) const;
	int64 LifetimeEarned = 0, LifetimePurchased = 0, LifetimeSpent = 0;

	TSharedRef<FJsonObject> ToJson() const;
	bool FromJson(const TSharedPtr<FJsonObject>& J, FString& OutError);
private:
	void Add(FZoogiGemTxn&& T);
	int32 EarnedV = 0, PurchasedV = 0;
	TArray<FZoogiGemTxn> LedgerV;
	TSet<FGuid> Ids;
};

// ------------------------------------------------------------------ config (all values from data)

struct FZoogiRewardRow { FName Source; FName Tag; int32 Gems = 0; int32 XP = 0; int32 PerMatchCap = 1; bool bXPToFocus = true; };
struct FZoogiTierRow
{
	EZoogiCPUTier Tier = EZoogiCPUTier::Normal;
	int32 Candidates = 3; float PickSpread = 350.f; int32 Lookahead = 0; bool bComboPlan = false; int32 EnergyReserve = 0; float AimNoise = 1.f;
	float GemMult = 1.f; float XPMult = 1.f;
};
struct FZoogiPowerRow { FName PowerId; FString Name; EZoogiElement Element = EZoogiElement::Neutral; FName Tier; bool bDefaultUnlocked = false; int32 MasteryLevel = 0; int32 GemCost = 0; FName CardId; };
struct FZoogiAchievementRow { FName Id; FString Name; FName Stat; int32 Target = 1; int32 Gems = 0; FName Title; FName Cosmetic; };
struct FZoogiTitleRow { FName Id; FString Name; FName Source; };
struct FZoogiCosmeticRow { FName Id; FString Name; FName Slot; FString Source; bool bDisplayed = false; };
struct FZoogiAffinityLevelRow { int32 Level = 1; int32 CumulativeXP = 0; FName RewardKind; FString RewardId; };
struct FZoogiGemPackageRow { FName PackageId; int32 Gems = 0; int32 BonusGems = 0; float PriceUSD = 0.f; FString AppleProductId, GoogleProductId, SteamItemId, EpicOfferId; };

struct ZOOGIROLL_API FZoogiProgressionConfig
{
	// Economy.csv
	int32 DailyMatchGemCap = 500;
	int32 DailyResetHourLocal = 4;
	int32 RollbackToleranceMinutes = 10;
	int32 StartGems = 0;
	int32 EarnedBalanceCap = 999999;
	EZoogiSpendOrder SpendOrder = EZoogiSpendOrder::EarnedFirst;
	float IdleFractionZeroReward = 0.5f;
	EZoogiPlayContext FocusChangeContext = EZoogiPlayContext::Standby;
	int32 RewardedMatchIdsKept = 64;
	int32 DRAfterMatches = 5;            // daily diminishing returns: from match N+1 of the day...
	float DRMult = 0.5f;                 // ...match gems + match XP x DRMult
	// tables
	TArray<int32> MasteryCumulativeXP;   // index = level-1 (10 levels)
	TArray<int32> MasteryLevelGems;      // index = level-1
	TArray<FZoogiRewardRow> Rewards;
	TArray<FZoogiTierRow> Tiers;
	TArray<FZoogiPowerRow> Powers;
	TArray<FZoogiAchievementRow> Achievements;
	TArray<FZoogiTitleRow> Titles;
	TArray<FZoogiCosmeticRow> Cosmetics;
	TArray<FZoogiAffinityLevelRow> AffinityLevels;
	TMap<FName, TPair<int32, int32>> AffinityXP; // source -> (xp, per-match cap)
	TArray<FZoogiGemPackageRow> GemPackages;
	FString Source = TEXT("builtin");    // builtin | csv

	/** Safe built-in defaults (same numbers as the shipped CSVs; used only when a file is missing / broken). */
	static FZoogiProgressionConfig BuiltIn();
	/** Loads every table from Dir (Content/Data/Progression). Any broken file -> false (+ error) and that table keeps its default. */
	bool LoadFromDir(const FString& Dir, FString& OutLog);
	/** Data rules: 10 increasing mastery levels, advanced powers mastery 6-7 & 400-600 gems, 7 titles, unique ids ... */
	bool Validate(FString& OutError) const;

	const FZoogiRewardRow* FindReward(FName Source, FName Tag = NAME_None) const;
	const FZoogiTierRow& Tier(EZoogiCPUTier T) const;
	const FZoogiPowerRow* FindPower(FName PowerId) const;
	int32 LevelForXP(int32 XP) const;
	int32 MaxLevel() const { return MasteryCumulativeXP.Num(); }
};

// ------------------------------------------------------------------ match tally (award hooks, P3/P4)

struct FZoogiAwardEvent { FName Source; FName Tag; EZoogiElement XPElement = EZoogiElement::Neutral; };

/** Built during a match by the award hooks; applied once at match end (idempotent by MatchId). Neutral events resolve to the
 *  Focus Element at the moment they happen (not retroactive). Awakening is counted for achievements only (Evan Q7). */
struct ZOOGIROLL_API FZoogiMatchTally
{
	FGuid MatchId;
	EZoogiCPUTier Tier = EZoogiCPUTier::Normal;
	FName ZoogiId;                 // the Zoogi the player controlled (affinity)
	bool bVsExpert() const { return Tier == EZoogiCPUTier::Expert; }
	int32 Turns = 0;
	int32 IdleTurns = 0;
	int32 Awakenings = 0;
	int32 SignatureHits = 0;
	TArray<FZoogiAwardEvent> Events;
};

struct FZoogiRewardLine { FName Source; FName Tag; int32 Count = 0; int32 Gems = 0; int32 XP = 0; EZoogiElement Element = EZoogiElement::Neutral; };

struct ZOOGIROLL_API FZoogiMatchRewardResult
{
	bool bApplied = false;          // false: duplicate match id / zero-reward case
	bool bDuplicate = false;
	FString ZeroReason;             // forfeit / abandoned / idle (empty = paid)
	TArray<FZoogiRewardLine> Lines;
	int32 BaseGems = 0;
	float GemMult = 1.f, XPMult = 1.f;
	int32 GemsBeforeCap = 0;
	int32 GemsPaid = 0;             // after the daily cap
	int32 CapClipped = 0;
	int32 XP[5] = { 0, 0, 0, 0, 0 }; // by EZoogiElement (Neutral stays 0: resolved to Focus)
	int32 LevelUpGems = 0;
	int32 AchievementGems = 0;
	TArray<FName> NewAchievements;
	TArray<FName> LevelUps;         // "Void:3"
	int32 AffinityXP = 0;
	int32 EarnedToday = 0, RemainingToday = 0;
};

// ------------------------------------------------------------------ profile (sync-ready)

struct FZoogiDailyState
{
	FDateTime DayStartLocal;        // current reset boundary (local 04:00); MinValue = never reset
	FDateTime LastResetUtc;
	FDateTime LastSeenUtc;          // clock-rollback guard
	int32 MatchGemsToday = 0;
	int32 MatchesToday = 0;
	bool bRollbackDetected = false;
};
struct FZoogiUnlockRecord { FName PowerId; FString Method; FDateTime Utc; };
struct FZoogiAffinityTrack { int32 XP = 0; TSet<int32> ClaimedLevels; FDateTime UpdatedUtc; };
struct FZoogiAchievementRecord { FName Id; FDateTime Utc; };
enum class EZoogiPurchaseState : uint8 { Pending, Verified, Consumed, Rejected };
struct FZoogiPurchaseRecord
{
	FString StoreTransactionId;      // Apple transactionId / Google purchaseToken / Steam orderid / EOS entitlement id
	FString Store;                   // apple | google | steam | epic | test
	FName PackageId;
	FString ProductId;
	int32 Gems = 0;
	EZoogiPurchaseState State = EZoogiPurchaseState::Pending;
	FGuid GrantTxnId;
	FDateTime CreatedUtc, UpdatedUtc;
	FString Note;
};

struct ZOOGIROLL_API FZoogiProfile
{
	static constexpr int32 CurrentSchema = 1;
	int32 SchemaVersion = CurrentSchema;
	FGuid ProfileId;                 // account-link key (Evan Q6)
	FString LinkedAccountId;         // empty until online account linking
	FDateTime CreatedUtc, UpdatedUtc;
	FZoogiWallet Wallet;
	int32 MasteryXP[5] = { 0, 0, 0, 0, 0 };
	FDateTime MasteryUpdatedUtc;
	EZoogiElement FocusElement = EZoogiElement::Void;
	FDateTime FocusChangedUtc;
	FZoogiDailyState Daily;
	TArray<FGuid> RewardedMatchIds;
	TArray<FZoogiUnlockRecord> Unlocks;
	TMap<FName, int32> Stats;        // lifetime stats (achievements)
	TArray<FZoogiAchievementRecord> Achievements;
	TMap<FName, FZoogiAffinityTrack> Affinity;
	TSet<FName> OwnedTitles;
	FName EquippedTitle;
	TSet<FName> OwnedCosmetics;
	TMap<FName, FName> EquippedCosmetics; // slot -> cosmetic id
	TArray<FZoogiPurchaseRecord> Purchases;

	FString ToJsonString() const;
	bool FromJsonString(const FString& S, FString& OutError);
};

// ------------------------------------------------------------------ the service

class ZOOGIROLL_API FZoogiProgression
{
public:
	void Init(const FZoogiProgressionConfig& InConfig);           // fresh profile (StartGems, Level 1)
	bool Load(const FZoogiProgressionConfig& InConfig, const FString& Json, FString& OutError);
	FString Save() const { return Profile.ToJsonString(); }
	const FZoogiProfile& Get() const { return Profile; }
	FZoogiProfile& Mutable() { return Profile; }
	const FZoogiProgressionConfig& Config() const { return Cfg; }

	// --- currency
	EZoogiTxnResult GrantGems(const FGuid& TxnId, EZoogiGemBucket Bucket, int32 Amount, FName Source, const FDateTime& Utc, const FString& ExternalId = FString());
	EZoogiTxnResult SpendGems(const FGuid& TxnId, int32 Amount, FName Sink, const FDateTime& Utc);

	// --- mastery
	int32 MasteryLevel(EZoogiElement E) const;
	int32 MasteryXP(EZoogiElement E) const;
	/** Adds XP (Neutral -> Focus); level-ups pay the curve's gem bonus once each (idempotent ids). Returns levels gained. */
	int32 AddMasteryXP(EZoogiElement E, int32 Amount, const FDateTime& Utc, TArray<FName>* OutLevelUps = nullptr, int32* OutGems = nullptr);
	bool SetFocusElement(EZoogiElement E, EZoogiPlayContext Ctx, const FDateTime& Utc, FString& OutWhy);
	EZoogiElement Focus() const { return Profile.FocusElement; }
	EZoogiElement Resolve(EZoogiElement E) const { return E == EZoogiElement::Neutral ? Profile.FocusElement : E; }

	// --- award hooks (record into a tally; Neutral resolves to the CURRENT focus)
	void HookKO(FZoogiMatchTally& T, EZoogiElement CauseElement) const;
	void HookRingOut(FZoogiMatchTally& T, EZoogiElement CauseElement) const;
	void HookTrickShot(FZoogiMatchTally& T, FName TrickTag, EZoogiElement CardElement) const;
	void HookPowerUsed(FZoogiMatchTally& T, EZoogiElement PowerElement) const;
	void HookOrbCollected(FZoogiMatchTally& T) const;
	void HookEpisodeTriggered(FZoogiMatchTally& T) const;
	void HookAwakening(FZoogiMatchTally& T) const { ++T.Awakenings; } // achievements only: no gems, no XP (Evan Q7)

	// --- daily
	/** Rolls the day over at the local reset hour; clock rollback (> tolerance before LastSeenUtc) freezes resets. */
	void CheckDailyReset(const FDateTime& NowUtc, const FDateTime& NowLocal);
	int32 EarnedToday() const { return Profile.Daily.MatchGemsToday; }
	int32 RemainingToday() const { return FMath::Max(0, Cfg.DailyMatchGemCap - Profile.Daily.MatchGemsToday); }
	FDateTime NextResetLocal() const;

	// --- match end
	FZoogiMatchRewardResult ApplyMatch(const FZoogiMatchTally& T, EZoogiMatchOutcome Outcome, const FDateTime& NowUtc, const FDateTime& NowLocal);

	// --- powers (Evan Q12: mastery OR gems; persist)
	bool IsPowerUnlocked(FName PowerId) const;
	bool CanUnlockPower(FName PowerId, bool& bByMastery, FString& OutWhy) const;
	bool UnlockPower(FName PowerId, const FDateTime& Utc, FString& OutWhy);

	// --- affinity / achievements / titles / cosmetics
	int32 AffinityLevel(FName ZoogiId) const;
	void AddAffinityXP(FName ZoogiId, int32 XP, const FDateTime& Utc);
	/** Claims the level's reward once (owned cosmetic id). False if not reached / already claimed. */
	bool ClaimAffinityReward(FName ZoogiId, int32 Level, const FDateTime& Utc, FString& OutRewardId);
	void AddStat(FName Stat, int32 Delta);
	int32 Stat(FName Stat) const;
	/** Completes every achievement whose stat reached target (grants gems/title/cosmetic once). */
	TArray<FName> EvaluateAchievements(const FDateTime& Utc, int32* OutGems = nullptr);
	bool HasAchievement(FName Id) const;
	bool EquipTitle(FName TitleId, const FDateTime& Utc);
	bool BuyCosmetic(FName CosmeticId, const FDateTime& Utc, FString& OutWhy);
	bool EquipCosmetic(FName CosmeticId, const FDateTime& Utc);
	FName EquippedCosmetic(FName Slot) const { const FName* N = Profile.EquippedCosmetics.Find(Slot); return N ? *N : NAME_None; }
	FString TitleName(FName TitleId) const;

private:
	int32 DerivedStat(FName Stat) const;
	void Touch(const FDateTime& Utc) { Profile.UpdatedUtc = Utc; }
	FZoogiProgressionConfig Cfg;
	FZoogiProfile Profile;
};
