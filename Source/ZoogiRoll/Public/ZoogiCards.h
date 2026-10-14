// v3.0.0 Card-Driven Rolling Combat, Phase 2: card data, deck rules and the backend adapter (no physics here).
//
// Data: Content/Data/Cards.csv (15 universal cards, stable FName ids) and Content/Data/Decks.csv; the built-in table
// below mirrors the CSV so a missing / broken file never changes rules silently (logged as CARDS data=builtin).
// Rules (FZoogiCardRules) are plain C++ (no UObject / world) so automation tests can drive them directly.
//   * validate before spend: Select / Deselect / Clear never spend; Commit re-validates everything, then spends once.
//   * apply once: Commit is idempotent per player turn (a second Commit returns AlreadyCommitted, spends nothing).
//   * Standby refresh: Card Energy is SET to EnergyPerTurn (not added), the hand is drawn up to HandSize.
//   * turn end: hand + committed cards go to the discard pile; an empty draw pile reshuffles the discard (seeded).
// Toolkit adapter: IZoogiCardBackend is the only seam a deck-building plugin would replace (definitions + shuffle).
// The Roguelike Deckbuilder Toolkit is NOT installed (Phase 1); FZoogiNativeCardBackend is the game's own backend.
// Tuning marked PROPOSED (deck 12 / max 2 copies / defaults) is Evan's Oct 13 call, configurable in [ZoogiRoll.Cards].
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

enum class EZoogiCardType : uint8 { Infusion, Technique, Defense, Utility, Impact, Terrain, Control };

/** Expiry tokens from the card book (Phase 3/4 consume them; Phase 2 only stores them). */
enum class EZoogiCardExpiry : uint8
{
	TurnEnd, LeadActionEnd, NextQualifyingEnemyImpact, UntilLocallyClearedOrMatchReset, PersistentTerrainState,
	FirstQualifiedContactOrLeadActionEnd, CommitPreparationThenLeadAction
};

/** What a card needs to be legal at Standby (validated before any spend). */
enum class EZoogiCardTarget : uint8
{
	ActingZoogi,       // Storm, Heavy Roll, Ember, Titan, Precision, Double Impact, Anchor Guard, Recovery Roll
	DefendingZoogi,    // Guard (the acting Zoogi defends itself after its roll)
	BankSurface,       // Bank Strike: a legal static bank surface (arena walls / bumpers)
	NeutralOrb,        // Orb Relay, Orb Magnet: a neutral Arena Orb in play (never an Awaken / power orb)
	EnemyContact,      // Momentum Steal: needs at least one opponent in play
	SurfaceLane,       // Frost
	CrystalPiece       // Crystal Collapse: an intact breakable arena piece (pillars / indestructible excluded). Exempt from the
	                   // global [ZoogiRoll.Arena] bDestruction=False for this card only (Evan, Oct 13 7:58 PM).
};

struct ZOOGIROLL_API FZoogiCardDef
{
	FName Id;                    // stable id, e.g. "storm" (never renamed; saves / ledgers key on it)
	FString Name;                // display, e.g. "STORM"
	int32 Cost = 1;              // Card Energy
	EZoogiCardType Type = EZoogiCardType::Control;
	EZoogiCardTarget Target = EZoogiCardTarget::ActingZoogi;
	EZoogiCardExpiry Expiry = EZoogiCardExpiry::LeadActionEnd;
	float ParamA = 0.f;          // primary tuning (e.g. Heavy Roll force x1.25), Phase 3+ reads it
	float ParamB = 0.f;          // secondary tuning
	int32 CPUWeight = 1;         // CPU Standby preference (higher first)
	FString Rule;                // one-line rule text (card book)
	bool bRequiresAlive = true;  // a KO actor cannot activate (Recovery Roll: explicit in the card book)
};

/** One physical card in a deck (two copies of Storm are two instances). */
struct FZoogiCardInstance
{
	int32 InstanceId = -1;
	FName CardId;
	bool operator==(const FZoogiCardInstance& O) const { return InstanceId == O.InstanceId; }
};

enum class EZoogiCardResult : uint8
{
	Ok, NotInStandby, UnknownPlayer, UnknownInstance, UnknownCard, AlreadySelected, NotSelected, DuplicateCardType,
	Unaffordable, InvalidTarget, ActorKnockedOut, AlreadyCommitted
};
ZOOGIROLL_API const TCHAR* ZoogiCardResultName(EZoogiCardResult R);

/** Live facts used for target validation at Standby (filled by the game; tests fill it by hand). */
struct FZoogiCardContext
{
	bool bActorKnockedOut = false;
	bool bHasNeutralOrb = true;
	bool bHasEnemy = true;
	bool bHasBankSurface = true;
	bool bHasSurfaceLane = true;
	bool bHasCrystalPiece = false;
};

struct FZoogiCardEvent
{
	int64 EventId = 0;       // unique, monotonically increasing per match (replay-safe accounting)
	int32 Turn = 0;
	int32 Player = -1;
	FName Kind;              // standby / draw / reshuffle / commit / commit_card / discard / reject
	FName CardId;
	int32 InstanceId = -1;
	int32 Cost = 0;
	int32 EnergyAfter = 0;
	int64 CommitId = 0;
	FString Detail;
};

struct FZoogiCardCommitted
{
	FZoogiCardInstance Card;
	int64 CommitId = 0;
	int64 EffectId = 0;      // unique per committed card (Phase 3 effects key on it: apply once)
};

struct FZoogiCardPlayerState
{
	TArray<FZoogiCardInstance> DrawPile;    // top = last element
	TArray<FZoogiCardInstance> Hand;
	TArray<FZoogiCardInstance> Discard;
	TArray<int32> Selected;                  // instance ids, in selection order (not spent)
	TArray<FZoogiCardCommitted> Committed;  // spent this turn
	int32 Energy = 0;
	int32 Turn = -1;                         // turn of the open / last Standby
	bool bStandbyOpen = false;
	bool bCommitted = false;
};

/** Toolkit seam: card definitions + shuffling. Swap in a plugin-backed implementation later; rules stay the same. */
class ZOOGIROLL_API IZoogiCardBackend
{
public:
	virtual ~IZoogiCardBackend() = default;
	virtual FName BackendName() const = 0;
	virtual const FZoogiCardDef* FindDef(FName Id) const = 0;
	virtual const TArray<FZoogiCardDef>& AllDefs() const = 0;
	/** Deterministic for a given stream state (replay-safe). */
	virtual void Shuffle(TArray<FZoogiCardInstance>& Cards, FRandomStream& Rng) const = 0;
};

class ZOOGIROLL_API FZoogiNativeCardBackend : public IZoogiCardBackend
{
public:
	explicit FZoogiNativeCardBackend(TArray<FZoogiCardDef> InDefs) : Defs(MoveTemp(InDefs)) {}
	virtual FName BackendName() const override { return TEXT("native"); }
	virtual const FZoogiCardDef* FindDef(FName Id) const override;
	virtual const TArray<FZoogiCardDef>& AllDefs() const override { return Defs; }
	virtual void Shuffle(TArray<FZoogiCardInstance>& Cards, FRandomStream& Rng) const override;
private:
	TArray<FZoogiCardDef> Defs;
};

/** The Roguelike Deckbuilder Toolkit backend (Fab sample project, Blueprint-only, UE 5.6; content migrated to /Game/RLDB).
 *  Card definitions come from /Game/Cards/DT_ZoogiCards, a DataTable on the toolkit's row struct
 *  /Game/RLDB/Blueprints/Structs/FCard (built by Scripts/make_toolkit_cards.py): CardName, Description and the
 *  BP_UseRule_StatCost Cost (paid in the toolkit's Mana stat = our Card Energy) are read by reflection. Gameplay fields the
 *  toolkit has no slot for (target, expiry, physics params, CPU weight) come from the native table by the same row id.
 *  Shuffling stays on our seeded stream: the toolkit's BP_PileComponent shuffle is not seedable (replay safety). */
class ZOOGIROLL_API FZoogiToolkitCardBackend : public IZoogiCardBackend
{
public:
	static constexpr const TCHAR* TablePath = TEXT("/Game/Cards/DT_ZoogiCards.DT_ZoogiCards");
	static constexpr const TCHAR* RowStructPath = TEXT("/Game/RLDB/Blueprints/Structs/FCard.FCard");
	/** Returns null (OutWhy says why) unless the table loads, uses FCard and covers every native id with a readable cost. */
	static TSharedPtr<FZoogiToolkitCardBackend> TryLoad(const TArray<FZoogiCardDef>& NativeDefs, FString& OutWhy);
	virtual FName BackendName() const override { return TEXT("rldb_toolkit"); }
	virtual const FZoogiCardDef* FindDef(FName Id) const override;
	virtual const TArray<FZoogiCardDef>& AllDefs() const override { return Defs; }
	virtual void Shuffle(TArray<FZoogiCardInstance>& Cards, FRandomStream& Rng) const override;
private:
	TArray<FZoogiCardDef> Defs;
};

struct FZoogiCardTune
{
	bool bCards = false;        // [ZoogiRoll.Cards] bCards (default False) / -ZoogiCards / -ZoogiCards=Off
	int32 HandSize = 5;         // card book: "Draw five"
	int32 EnergyPerTurn = 5;    // card book: "refresh five Energy"
	int32 DeckSize = 12;        // PROPOSED (Evan, Oct 13)
	int32 MaxCopies = 2;        // PROPOSED (Evan, Oct 13)
	int32 MaxCardsPerCommit = 5;
	FString DefaultDeck;        // deck id in Decks.csv
	FString Backend = TEXT("toolkit"); // toolkit (RLDB DataTable, falls back to native) | native
	void Load();                // ini + command line
};

namespace ZoogiCards
{
	/** The 15 universal cards (card book p.17-31). Costs / types / expiries VERIFIED-DOC; Param values PROPOSED (book says so). */
	ZOOGIROLL_API TArray<FZoogiCardDef> BuiltInDefs();
	/** Default 12-card deck (PROPOSED): every card once except Storm / Guard / Precision twice... see Decks.csv. */
	ZOOGIROLL_API TArray<FName> BuiltInDeck();
	/** Parses Cards.csv text (header row required). Returns false (and leaves Out empty) on any malformed row. */
	ZOOGIROLL_API bool ParseCardsCsv(const FString& Text, TArray<FZoogiCardDef>& Out, FString& OutError);
	/** Parses Decks.csv text: deck_id,card_id rows (one row per copy). */
	ZOOGIROLL_API bool ParseDecksCsv(const FString& Text, TMap<FString, TArray<FName>>& Out, FString& OutError);
	/** Deck legality: exact DeckSize, <= MaxCopies per id, known ids. */
	ZOOGIROLL_API bool ValidateDeck(const TArray<FName>& Deck, const IZoogiCardBackend& Backend, int32 DeckSize, int32 MaxCopies, FString& OutError);
	ZOOGIROLL_API const TCHAR* TypeName(EZoogiCardType T);
	ZOOGIROLL_API const TCHAR* ExpiryName(EZoogiCardExpiry E);
	ZOOGIROLL_API const TCHAR* TargetName(EZoogiCardTarget T);
	/** Force labels in cards mode: CONTROL / DRIVE / LAUNCH = the existing SOFT / MED / FULL power steps (PROPOSED mapping). */
	ZOOGIROLL_API const TCHAR* ForceLabel(int32 PowerStep, bool bCardsMode);
}

/** All card rules for one match. No UObjects; deterministic for (Seed, decks, call sequence). */
class ZOOGIROLL_API FZoogiCardRules
{
public:
	FZoogiCardRules() = default;
	FZoogiCardRules(const FZoogiCardRules&) = delete;
	FZoogiCardRules& operator=(const FZoogiCardRules&) = delete;

	/** Decks: one per player (invalid decks fall back to Fallback, which must be valid). Returns false if nothing valid. */
	bool Init(TSharedPtr<IZoogiCardBackend> InBackend, const FZoogiCardTune& InTune, int32 Seed, const TArray<TArray<FName>>& Decks, const TArray<FName>& Fallback, FString& OutLog);

	/** Opens Standby for P at Turn: closes any other open turn first (end-of-turn discard), refreshes Energy, draws to HandSize.
	 *  Idempotent for the same (P, Turn). */
	void BeginStandby(int32 P, int32 Turn);
	EZoogiCardResult Select(int32 P, int32 InstanceId, const FZoogiCardContext& Ctx);
	EZoogiCardResult Deselect(int32 P, int32 InstanceId);
	EZoogiCardResult Toggle(int32 P, int32 InstanceId, const FZoogiCardContext& Ctx);
	void ClearSelection(int32 P);
	/** Validates the whole selection again, then spends once. Out = the committed cards. */
	EZoogiCardResult Commit(int32 P, const FZoogiCardContext& Ctx, TArray<FZoogiCardCommitted>* Out = nullptr);
	/** Ends P's turn: hand + committed -> discard, Standby closed. Safe to call twice. */
	void EndTurn(int32 P);

	/** Pure check (no state change): would selecting this instance now be legal? */
	EZoogiCardResult CanSelect(int32 P, int32 InstanceId, const FZoogiCardContext& Ctx) const;

	const FZoogiCardPlayerState* Player(int32 P) const { return Players.IsValidIndex(P) ? &Players[P] : nullptr; }
	int32 NumPlayers() const { return Players.Num(); }
	const TArray<FZoogiCardEvent>& Events() const { return Ledger; }
	const IZoogiCardBackend* Backend() const { return BackendPtr.Get(); }
	const FZoogiCardTune& Tune() const { return TuneV; }
	/** Order-sensitive hash of every player's piles, energy and the ledger (replay / save checks). */
	uint32 StateHash() const;
	/** v3 P6 save / resume: piles, energy, selection, ledger, ids and the shuffle stream (backend + tune are not saved). */
	void Serialize(FArchive& Ar);
	/** Total Card Energy spent by P over the match (tests: "costs applied once"). */
	int32 TotalSpent(int32 P) const { return Spent.IsValidIndex(P) ? Spent[P] : 0; }

private:
	void Draw(int32 P, int32 Count);
	void Log(int32 P, FName Kind, const FZoogiCardInstance* C, int32 Cost, int64 CommitId, const FString& Detail);
	const FZoogiCardInstance* FindInHand(int32 P, int32 InstanceId) const;
	EZoogiCardResult CheckTarget(const FZoogiCardDef& D, const FZoogiCardContext& Ctx) const;

	TSharedPtr<IZoogiCardBackend> BackendPtr;
	FZoogiCardTune TuneV;
	FRandomStream Rng;
	TArray<FZoogiCardPlayerState> Players;
	TArray<FZoogiCardEvent> Ledger;
	TArray<int32> Spent;
	int64 NextEventId = 1;
	int64 NextCommitId = 1;
	int64 NextEffectId = 1;
};
