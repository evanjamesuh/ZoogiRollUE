// Battle Episode Engine (T4): the match's canonical, append-only episode ledger + the match-end assembler.
//   ledger   : one FZoogiEpisodeLedgerEvent per action / reaction / shatter / turn / match end, in order. Logged as
//              BATTLE_LEDGER {json}; dumped with the assembly to Saved/Episodes/<match>.json; ledger hash for replays.
//   assembler: per-player spotlight / contributions, fragments (signature / spotlight / witness / defeat), the
//              canonical recap playlist (ledger order, bridges for gaps), per-player edits, full-episode unlocks
//              ([ZoogiRoll.Unlocks] thresholds). Clip / shot / slice ids are placeholders (no art yet).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeLedger.generated.h"

class UZoogiBattleStateMachine;

UCLASS()
class ZOOGIROLL_API UZoogiEpisodeLedger : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TArray<FZoogiEpisodeLedgerEvent> Events;
	/** Appends (assigns seq + event_id) and logs one BATTLE_LEDGER line. */
	const FZoogiEpisodeLedgerEvent& Append(FZoogiEpisodeLedgerEvent E);
	static FString ToJson(const FZoogiEpisodeLedgerEvent& E);
	/** CRC of every event's JSON without wall-clock fields (identical for an identical replay). */
	uint32 Hash() const;
	void Reset() { Events.Reset(); }
};

USTRUCT()
struct FZoogiPlaylistEntry
{
	GENERATED_BODY()
	UPROPERTY() int32 seq = -1;          // ledger seq (-1 = bridge)
	UPROPERTY() FString kind;            // segment | spotlight | ko | bridge | finale
	UPROPERTY() FString clip_id;
	UPROPERTY() FString actor_id;
	UPROPERTY() int32 actor_player = -1;
	UPROPERTY() TArray<int32> players;   // players featured (actor + targets)
	UPROPERTY() TArray<FString> slice_ids;
};

USTRUCT()
struct FZoogiPlayerAssembly
{
	GENERATED_BODY()
	UPROPERTY() int32 player = -1;
	UPROPERTY() FString name;
	UPROPERTY() bool winner = false;
	UPROPERTY() float performance = 0.f;
	UPROPERTY() int32 contributions = 0;
	UPROPERTY() int32 spotlights = 0;
	UPROPERTY() int32 segments_completed = 0;
	UPROPERTY() int32 segments_shattered = 0;
	UPROPERTY() int32 kos = 0;
	UPROPERTY() FString unlock;          // full | partial | none
	UPROPERTY() TArray<FZoogiEpisodeFragment> fragments;
	UPROPERTY() TArray<int32> edit;      // indices into the canonical playlist (only this player's / shared entries)
};

UCLASS()
class ZOOGIROLL_API UZoogiEpisodeAssembler : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TArray<FZoogiPlaylistEntry> Playlist;
	UPROPERTY() TArray<FZoogiPlayerAssembly> PlayerResults;
	FString LastPath;
	/** Builds everything from the ledger + battle player state. Winner: player index (classic) / team*2 (2v2), -1 none. */
	void Assemble(UZoogiBattleStateMachine* SM, const UZoogiEpisodeLedger* L, int32 WinnerPlayer, int32 WinnerTeam, bool bTeams, const FString& MatchId, const FString& Why);
	FString Dump(const UZoogiEpisodeLedger* L, const FString& MatchId) const;
};
