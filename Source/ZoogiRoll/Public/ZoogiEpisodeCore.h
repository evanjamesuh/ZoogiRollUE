// v3 Phase 5: Battle Episode Engine extension, pure bookkeeping (no world, no UObjects).
// Extends the existing engine. UZoogiEpisodeLedger is still the one authoritative ledger; this core consumes its events
// read-only and never writes back into the simulation.
//   * facts     : one FZoogiEpFact per ledger event, de-duplicated by a content key (a replayed / re-delivered event is
//                 counted once). Event ids stay the ledger's stable ev_<seq>_<kind>.
//   * roles     : per action, every player gets exactly one role: OPENING / FOLLOW_THROUGH (the acting Zoogi), MISS
//                 (acting Zoogi, nothing connected - acknowledged, not hidden), RECEIVING_IMPACT (targets / rung out),
//                 PREPARING_RESPONSE (everyone else). Contributions are tallied from those roles.
//   * episodes  : Episode Gauge = the existing per-Zoogi Episode Energy (cap 100). At 100 it is spent once (-100) on a
//                 12 s episode of four 3 s perspectives (spender first). Awakening is never read or written here.
//                 One spend per (player, fact). Branch: SUCCESS / FAILURE / RECOVERY / ELIMINATION / VICTORY / LOSS,
//                 decided from resolved facts only.
//   * playback  : FZoogiEpPlayback holds a COPY of a resolved episode; watching or skipping only moves its cursor.
//   * recap     : full-battle recap unlocks, PROPOSED rules U1-U4 (see RecapUnlock).
#pragma once

#include "CoreMinimal.h"

namespace ZoogiEpTune
{
	constexpr float GaugeCost = 100.f;          // card book: "At 100 Episode, spend once"
	constexpr float EpisodeSeconds = 12.f;      // "12-second result-driven branch"
	constexpr float PerspectiveSeconds = 3.f;   // "four three-second perspectives"
	// PROPOSED recap rules (not in the card book)
	constexpr int32 RecapMinActions = 6;        // U2: the match had at least this many counted ACTION facts
	constexpr int32 RecapMinActive = 2;         // U3: OPENING + FOLLOW_THROUGH that connected (or on the winning side)
}

enum class EZoogiEpBranch : uint8 { Success, Failure, Recovery, Elimination, Victory, Loss };
enum class EZoogiEpRole : uint8 { Opening, FollowThrough, ReceivingImpact, PreparingResponse, Miss };
enum class EZoogiRecap : uint8 { None, Partial, Full };

ZOOGIROLL_API const TCHAR* ZoogiEpBranchName(EZoogiEpBranch B);
ZOOGIROLL_API const TCHAR* ZoogiEpRoleName(EZoogiEpRole R);
ZOOGIROLL_API const TCHAR* ZoogiRecapName(EZoogiRecap R);

/** A resolved fact, taken from one ledger event. */
struct FZoogiEpFact
{
	FString EventId;                 // ledger ev_<seq>_<kind> (stable, replay-safe)
	int32 Seq = -1;
	FString Kind;                    // ACTION | SHATTER | REACTION | TURN | MATCH_END
	int32 Turn = 0, Round = 0, ActionId = -1;
	int32 Actor = -1;                // MATCH_END: winning player (team*2 in 2v2), -1 none
	TArray<int32> Targets;           // Zoogi targets only (orbs dropped)
	TArray<int32> RingOuts;
	bool bCollision = false;
	bool bFollowUp = false;
	bool bShattered = false;
	bool bObjectiveFailed = false;   // FAILED / TIMEOUT
	bool bActorRecovered = false;    // crossed into the recovery zone and was not rung out
	bool bMatchOver = false;         // this action ended the match
	FString FinalHash;
	ZOOGIROLL_API FString Key() const; // content key (no seq / time): a re-delivered event has the same key
};

struct FZoogiEpContribution
{
	int32 Opening = 0, FollowThrough = 0, Receiving = 0, Preparing = 0, Misses = 0;
	int32 ActiveWithContact = 0;     // OPENING / FOLLOW_THROUGH that connected
	int32 RungOut = 0;
};

struct FZoogiEpPerspective
{
	int32 Player = -1;
	EZoogiEpRole Role = EZoogiEpRole::PreparingResponse;
	FString Line;
	float Start = 0.f, Duration = ZoogiEpTune::PerspectiveSeconds;
};

struct FZoogiEpisode
{
	int32 EpisodeId = 0;             // 1..n in spend order (replay-stable)
	int32 Spender = -1;
	FString FactEventId;
	int32 FactSeq = -1;
	EZoogiEpBranch Branch = EZoogiEpBranch::Success;
	float Duration = ZoogiEpTune::EpisodeSeconds;
	TArray<FZoogiEpPerspective> Perspectives;
};

/** Read-only playback of one resolved episode. Holds a copy: nothing it does can reach the facts or the match. */
struct ZOOGIROLL_API FZoogiEpPlayback
{
	FZoogiEpisode Ep;
	float T = 0.f;
	bool bSkipped = false;
	explicit FZoogiEpPlayback(const FZoogiEpisode& In) : Ep(In) {}
	void Tick(float Dt) { if (!bSkipped) T = FMath::Min(Ep.Duration, T + FMath::Max(0.f, Dt)); }
	void Skip() { bSkipped = true; T = Ep.Duration; }
	bool Done() const { return bSkipped || T >= Ep.Duration - KINDA_SMALL_NUMBER; }
	int32 PerspectiveIndex() const;
};

class ZOOGIROLL_API FZoogiEpisodeCore
{
public:
	void Reset(int32 InPlayers, bool bInTeams, const TArray<FString>& InNames);
	/** Adds a fact. False (and nothing counted) when the same content key was already added. */
	bool AddFact(const FZoogiEpFact& F);
	/** Role of player P in action fact F. */
	EZoogiEpRole RoleOf(const FZoogiEpFact& F, int32 P) const;
	/** Branch for an episode spent by Spender on fact F. */
	EZoogiEpBranch BranchFor(const FZoogiEpFact& F, int32 Spender) const;
	/** Spends: requires Gauge >= 100 and no earlier spend by P on this fact. Builds the 4 perspectives. */
	bool TrySpend(int32 P, float Gauge, int32 FactIndex, FZoogiEpisode& Out);
	/** PROPOSED recap rules. U1 MATCH_END fact present; U2 >= RecapMinActions ACTION facts; U3 player active-with-contact
	 *  >= RecapMinActive or on the winning side; U4 watching / skipping never counts. FULL = U1&U2&U3;
	 *  PARTIAL = U1 and the player had any non-PREPARING role; else NONE. */
	EZoogiRecap RecapUnlock(int32 P, FString* OutWhy = nullptr) const;
	bool IsWinnerSide(int32 P) const;
	uint32 Hash() const;

	const TArray<FZoogiEpFact>& Facts() const { return FactsV; }
	const TArray<FZoogiEpisode>& Episodes() const { return EpisodesV; }
	const FZoogiEpContribution& Contribution(int32 P) const { return ContribV[P]; }
	int32 NumPlayers() const { return Num; }
	int32 Duplicates() const { return Dups; }
	int32 ActionFacts() const { return NumActions; }

private:
	FString PerspectiveLine(const FZoogiEpFact& F, int32 P, EZoogiEpRole R, EZoogiEpBranch B, int32 Spender) const;
	FString Nm(int32 P) const;
	const FZoogiEpFact* PrevAction(const FZoogiEpFact& F) const;
	int32 Num = 4;
	bool bTeams = false;
	TArray<FString> Names;
	TArray<FZoogiEpFact> FactsV;
	TSet<FString> Keys;
	TArray<FZoogiEpContribution> ContribV;
	TArray<FZoogiEpisode> EpisodesV;
	TSet<FString> Spent;             // "<player>|<fact key>"
	int32 Dups = 0, NumActions = 0;
	int32 Winner = -1;               // from MATCH_END
	bool bEnded = false;
};
