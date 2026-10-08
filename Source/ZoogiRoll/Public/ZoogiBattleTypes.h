// Battle Episode Engine (spec 2026-10-08): shared types for the battle state machine, Episode Energy, Awakening /
// reactions, mini-games, the episode ledger and the match-end assembler. ENGINE ONLY: debug visuals / log lines.
//
// Everything tunable lives in FZoogiBattleTuning (defaults below; DefaultGame.ini sections [ZoogiRoll.Battle],
// [ZoogiRoll.EpisodeEnergy], [ZoogiRoll.Awakening], [ZoogiRoll.Unlocks]) and in the CSV tables
// Content/Data/EpisodeEnergyGains.csv, EpisodeCombos.csv, MiniGames.csv (built-in defaults when absent).
#pragma once

#include "CoreMinimal.h"
#include "ZoogiBattleTypes.generated.h"

/** The single authoritative battle flow (spec 6.1). */
UENUM()
enum class EZoogiBattleState : uint8
{
	None,
	WaitForCommand,   // Draw / Awaken / Battle aim of the active Zoogi
	ActionPreview,    // command confirmed: power-up beat before the launch
	MiniGame,         // a constrained challenge is taking input (combo challenge / Air Reaction)
	PhysicsResolve,   // launched: rolling physics until rest
	ShatterFailure,   // a challenge failed: the pending scene shatters
	Result,           // Resolve committed the authoritative result
	EpisodeEvent,     // the action's episode event is recorded (ledger)
	NextTurn,         // hand-off to the next eligible player
	MatchEnd
};

/** What a combo / mini-game requires to complete. */
UENUM()
enum class EZoogiChallenge : uint8
{
	None,         // completes on launch (plus the combo's tag / tier requirements)
	Strike,       // the shooter's first Zoogi contact is the declared target
	HaloBank,     // bank off a surface, then reach the declared target
	OrbLine,      // touch every orb committed on the route
	AirReaction,  // Awakened Zoogi reacts while launched and lands on the court
	SkyBumper,    // (ref minigame_layout_frozen) touch a Zone 2 aerial bumper / disc, then a body contact (config stub)
	PartnerIntercept // (ref minigame_layout_frozen) 2v2: the partner is the first body hit, then a rival (config stub)
};

/** Frozen Ring layers (ref frozen_zones_aerial): debug zone tags, no art. */
UENUM()
enum class EZoogiZone : uint8
{
	Court,     // Zone 1: the floor play space
	Aerial,    // Zone 2: aerial play space above the arena (halo rings, rails, floating discs, bumpers)
	Recovery   // Zone 3: danger / recovery orbit ring around the arena (red / blue gates)
};

UENUM()
enum class EZoogiChallengeResult : uint8
{
	None,
	Pending,
	Success,
	Failed,
	Timeout,
	Cancelled
};

/** Comic / segment state recorded in the ledger. A failed segment is never recorded as completed. */
UENUM()
enum class EZoogiComicState : uint8
{
	None,
	Pending,
	Completed,
	Shattered
};

UENUM()
enum class EZoogiReactionKind : uint8
{
	Air
};

/** Air Reaction choices (data-driven amounts in FZoogiBattleTuning). */
UENUM()
enum class EZoogiAirChoice : uint8
{
	None,
	Brace,     // damp the horizontal speed (stay in)
	Redirect   // turn the horizontal velocity toward the arena centre
};

struct FZoogiEnergyGainRow
{
	FName Source;          // Hit, StrongHit, Impact, Crack, Shatter, OrbOut, RingOut, PowerHit, Feat, GuardBlock, NearMiss, OrbLineOrb
	float Amount = 0.f;
};

struct FZoogiEpisodeComboRow
{
	FName Id;
	FString Label;
	float Cost = 0.f;
	EZoogiChallenge Challenge = EZoogiChallenge::None;
	int32 MinTier = 0;               // recognized tier the action must reach
	FName RequiredTag;               // optional EPISODE tag name (e.g. POWER_HIT)
	FName ClipId;                    // placeholder clip / comic segment id awarded on success
	bool bCPU = true;                // CPUs may pick it
};

USTRUCT()
struct FZoogiBattleTuning
{
	GENERATED_BODY()
	// [ZoogiRoll.Battle]
	bool bEnabled = false;                  // the new battle flow (combos, shatter, reactions); OFF = existing play unchanged
	bool bMissConsolationCard = false;      // flow on: keep the old "MISSED..." anime card after a missed Infuse
	float FollowUpOfferTimeout = 12.f;      // flow on: a human Partner Follow-Up offer is declined after this (s, 0 = never)
	float MiniGameInputTimeout = 6.f;       // human mini-game input window (s)
	float ShatterScorePenalty = 0.f;        // points taken from the owner when a segment shatters
	float ShatterRefundFrac = 0.f;          // fraction of the combo cost refunded on a shatter
	bool bSpendOnSelect = true;             // combo cost is paid when committed (else only on success)
	bool bCPUCombos = true;                 // CPU players pick affordable combos
	bool bDebugOverlay = true;              // on-screen debug lines (energy, awakening, challenge, SHATTER)
	// [ZoogiRoll.EpisodeEnergy]
	float EnergyCap = 100.f;
	float StrongHitSpeed = 900.f;           // relative normal speed for a "strong" hit (cm/s)
	int32 ImpactCap = 4;                    // surface impacts counted per action
	FString FollowUpCostSource = TEXT("TeamGauge"); // 2v2 Partner Follow-Up paid from TeamGauge (default) or EpisodeEnergy (the partner's)
	bool bResonanceSurgeDraws = true;       // 2v2: every 3 Resonance -> +1 artifact draw (existing rule)
	bool bArenaControlGauge = true;         // 2v2: 100 Arena Control -> +30 team gauge (existing rule)
	bool bStarOrbDraws = true;              // star orb -> +1 bonus draw (existing rule)
	// [ZoogiRoll.Awakening]
	FString ActivationRule = TEXT("InfuseOrGuard"); // InfuseOrGuard | Infuse | Guard | Always | Never  (never Episode Energy)
	int32 AwakenTurns = 1;                  // stays Awakened until its own N-th next turn starts
	int32 ReactionsPerAwakening = 1;
	bool bAirReactionOwnTurn = true;        // the shooter may react to its own launch
	float AirReactionMaxWait = 0.6f;        // wait this long (game s) for the game's hit-stop to end, else no offer
	float AirReactionHoldCPU = 0.6f;        // real seconds the reaction beat holds for a CPU
	float BraceDamp = 0.45f;                // Brace: horizontal speed kept
	float RedirectDeg = 35.f;               // Redirect: max turn toward the centre
	// [ZoogiRoll.Unlocks]
	float PerfWeightScore = 0.01f;          // performance = score*w + KOs*w + completed*w + tier3plus*w - shatters*w
	float PerfWeightKO = 2.f;
	float PerfWeightSegment = 3.f;
	float PerfWeightSpotlight = 1.f;
	float PerfWeightShatter = 1.f;
	float FullEpisodePerf = 12.f;           // winner + performance >= this + MinSegments + MaxShatters -> full episode
	int32 FullEpisodeMinSegments = 2;
	int32 FullEpisodeMaxShatters = 1;
	float FragmentPerf = 1.f;               // performance needed for a player's own highlight fragments

	TMap<FName, float> Gains;
	TArray<FZoogiEpisodeComboRow> Combos;
	FString GainsSource, CombosSource;

	void Load();
	float Gain(FName Source) const { const float* V = Gains.Find(Source); return V ? *V : 0.f; }
	const FZoogiEpisodeComboRow* FindCombo(FName Id) const;
	bool FollowUpUsesEnergy() const { return FollowUpCostSource.Equals(TEXT("EpisodeEnergy"), ESearchCase::IgnoreCase); }
};

/** Per player / Zoogy (spec 5). Energy + Awakening live on their components; this is the bookkeeping. */
USTRUCT()
struct FZoogiBattlePlayerState
{
	GENERATED_BODY()
	UPROPERTY() int32 Player = -1;
	UPROPERTY() FString Name;
	UPROPERTY() int32 Team = 0;
	UPROPERTY() int32 Turns = 0;
	UPROPERTY() int32 Actions = 0;
	UPROPERTY() int32 Hits = 0;
	UPROPERTY() int32 KOs = 0;
	UPROPERTY() int32 TimesKOd = 0;
	UPROPERTY() int32 OrbOuts = 0;
	UPROPERTY() int32 Contributions = 0;     // ledger events this player acted in / was a target of
	UPROPERTY() int32 Spotlights = 0;        // tier >= 3 actions as actor
	UPROPERTY() int32 SegmentsCompleted = 0;
	UPROPERTY() int32 SegmentsShattered = 0;
	UPROPERTY() int32 Reactions = 0;
	UPROPERTY() float EnergyGained = 0.f;
	UPROPERTY() float EnergySpent = 0.f;
};

/** One combo / challenge committed by a player for its next action (or an Air Reaction). */
USTRUCT()
struct FZoogiPendingSegment
{
	GENERATED_BODY()
	UPROPERTY() FString SegmentId;
	UPROPERTY() FName ComboId;
	UPROPERTY() FName ClipId;
	UPROPERTY() int32 Owner = -1;
	UPROPERTY() EZoogiChallenge Challenge = EZoogiChallenge::None;
	UPROPERTY() EZoogiChallengeResult Result = EZoogiChallengeResult::None;
	UPROPERTY() EZoogiComicState Comic = EZoogiComicState::None;
	UPROPERTY() float Cost = 0.f;
	UPROPERTY() bool bPaid = false;
	UPROPERTY() int32 ActionId = -1;
	UPROPERTY() int32 DeclaredTarget = -1;   // body code (Strike / HaloBank)
	UPROPERTY() TArray<int32> CommittedOrbs; // body codes (OrbLine)
	UPROPERTY() FString Detail;              // why it succeeded / failed
	UPROPERTY() EZoogiAirChoice AirChoice = EZoogiAirChoice::None;
};

/** One canonical ledger event (spec 6 sample contract, extended). */
USTRUCT()
struct FZoogiEpisodeLedgerEvent
{
	GENERATED_BODY()
	UPROPERTY() FString event_id;
	UPROPERTY() int32 seq = 0;
	UPROPERTY() int32 turn_index = 0;
	UPROPERTY() int32 round = 0;
	UPROPERTY() int32 action_id = -1;
	UPROPERTY() float game_time = 0.f;
	UPROPERTY() FString kind;                  // ACTION | REACTION | SHATTER | TURN | MATCH_END
	UPROPERTY() FString actor_id;
	UPROPERTY() int32 actor_player = -1;
	UPROPERTY() TArray<FString> target_ids;
	UPROPERTY() FString command;               // SHOT | FOLLOW_UP | COMBO:<id> | AIR_REACTION:<choice>
	UPROPERTY() FString minigame;              // None | Strike | HaloBank | OrbLine | AirReaction
	UPROPERTY() FString objective_result;      // NONE | SUCCESS | FAILED | TIMEOUT | CANCELLED
	UPROPERTY() bool collision = false;
	UPROPERTY() bool ring_out = false;
	UPROPERTY() TArray<FString> ring_out_ids;
	UPROPERTY() int32 orb_outs = 0;
	UPROPERTY() TArray<int32> score_delta;
	UPROPERTY() TArray<FString> tags;
	UPROPERTY() int32 tier = 0;
	UPROPERTY() TArray<float> episode_energy_delta; // per player
	UPROPERTY() FString episode_segment_id;
	UPROPERTY() FString clip_id;
	UPROPERTY() TArray<FString> shot_ids;      // presentation shot_ids (placeholder library)
	UPROPERTY() TArray<FString> slice_ids;     // comic slice placeholders: slice_eye_<zoogi>_<calm|surprised|angry|targeting>
	UPROPERTY() TArray<FString> zones;         // zone tags touched (court / aerial / recovery)
	UPROPERTY() FString comic_state;           // NONE | COMPLETED | SHATTERED
	UPROPERTY() FString next_state;
	UPROPERTY() TArray<int32> spotlight;       // players featured
	UPROPERTY() FString variant;               // dialogue / camera variant id (placeholder)
	UPROPERTY() FString final_hash;            // final positions + scores (replay consistency)
};

/** Match-end fragment (spec 5): something a player can watch / unlock after the match. */
USTRUCT()
struct FZoogiEpisodeFragment
{
	GENERATED_BODY()
	UPROPERTY() FString fragment_id;
	UPROPERTY() int32 player = -1;
	UPROPERTY() FString kind;        // signature | spotlight | witness | defeat | full_episode
	UPROPERTY() FString clip_id;
	UPROPERTY() int32 source_seq = -1;
	UPROPERTY() bool unlocked = false;
};

namespace ZoogiBattle
{
	ZOOGIROLL_API const TCHAR* StateName(EZoogiBattleState S);
	ZOOGIROLL_API const TCHAR* ChallengeName(EZoogiChallenge C);
	ZOOGIROLL_API const TCHAR* ResultName(EZoogiChallengeResult R);
	ZOOGIROLL_API const TCHAR* ComicName(EZoogiComicState S);
	ZOOGIROLL_API const TCHAR* AirChoiceName(EZoogiAirChoice C);
	ZOOGIROLL_API EZoogiChallenge ParseChallenge(const FString& S);
	/** Loads a loose CSV (header row + rows) from Content/Data/<Name>; false if absent. */
	ZOOGIROLL_API bool LoadCsv(const FString& Name, TArray<TArray<FString>>& OutRows, FString& OutPath);
}
