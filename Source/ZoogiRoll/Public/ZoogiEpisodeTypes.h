#pragma once

#include "CoreMinimal.h"
#include "ZoogiArtifacts.h"
#include "ZoogiEpisodeTypes.generated.h"

/**
 * Zoogi Roll Episode Engine (spec v1.1, phase 1): data model.
 *
 *   physics shot -> FZoogiGameplayTruthRecord (what really happened, captured from real game events only)
 *                -> FZoogiEpisodeRecognitionRecord (tags, drama score, tier, archetype)
 *                -> FZoogiCinematicPlan (what the presentation layer shows; never feeds back into gameplay)
 *
 * RULE ZERO: nothing here changes physics, results, velocities, scoring or turn order. All tuning lives in
 * FZoogiEpisodeTuning (defaults below, overridable in DefaultGame.ini [ZoogiRoll.Episode]).
 */

UENUM()
enum class EZoogiEpisodeMode : uint8
{
	Off,          // capture + log only, no presentation
	Competitive,  // short: callouts, glow, short meteor; no panels, no slow-mo
	Balanced,     // default: panels from tier 3, power escalation from tier 4, meteor
	Full          // longer beats, four panels, lower panel threshold
};

UENUM()
enum class EZoogiEpisodeTag : uint8
{
	Bank,
	DoubleBank,
	AllyBump,
	OrbRelay,
	WallRedirect,
	BoundarySave,
	Airborne,
	PowerHit,
	GuardBreak,
	OrbOut,
	RingOut,
	Terrain,
	PartnerFollowUp,
	Num UMETA(Hidden)
};

UENUM()
enum class EZoogiContactKind : uint8
{
	Zoogi,   // rival zoogi
	Ally,    // teammate (2v2)
	Orb,
	Wall,
	Bumper,
	Ramp,
	Piece,   // destructible arena piece (AZoogiArenaPiece)
	Rock,    // destructible rock (AZoogiRock)
	Floor
};

UENUM()
enum class EZoogiEpisodeArchetype : uint8
{
	None,
	TitanMeteor
};

UENUM()
enum class EZoogiPanelLayout : uint8
{
	None,
	CornerPair,     // 2 panels: upper-left + lower-right
	SidePair,       // 2 panels: left + right
	Trio,           // 3 panels: left, right, top
	Quad            // 4 panels: corners
};

/** One real contact between two bodies of the current action (deduplicated, floor rolling ignored). */
USTRUCT()
struct FZoogiContactEvent
{
	GENERATED_BODY()
	UPROPERTY() float Time = 0.f;            // game seconds since launch
	UPROPERTY() int32 Body = -1;             // ball player index, or 100 + orb index
	UPROPERTY() int32 OtherBody = -1;        // same encoding, -1 for world geometry
	UPROPERTY() EZoogiContactKind Kind = EZoogiContactKind::Floor;
	UPROPERTY() FName OtherName;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FVector Normal = FVector::ZeroVector;
	UPROPERTY() float RelSpeed = 0.f;        // normal relative speed at contact (cm/s)
	UPROPERTY() float Impulse = 0.f;
	UPROPERTY() int32 Depth = 0;             // causal chain depth (0 = shooter)
	UPROPERTY() float Weight = 0.f;          // significance 0..1.5 (speed based, diminishing for repeats)
	UPROPERTY() bool bMeaningful = false;
	TWeakObjectPtr<UPrimitiveComponent> Surface;
};

USTRUCT()
struct FZoogiAirSegment
{
	GENERATED_BODY()
	UPROPERTY() int32 Body = -1;
	UPROPERTY() float Start = 0.f;
	UPROPERTY() float End = 0.f;
	UPROPERTY() float PeakGap = 0.f;         // highest gap under the ball (cm)
};

USTRUCT()
struct FZoogiBoundaryEvent
{
	GENERATED_BODY()
	UPROPERTY() int32 Body = -1;
	UPROPERTY() float Time = 0.f;
	UPROPERTY() float MinEdgeDist = 0.f;     // closest approach to the knockout line (cm)
	UPROPERTY() float SpeedAtEdge = 0.f;
	UPROPERTY() bool bCrossed = false;       // went past the line
	UPROPERTY() bool bFellOut = false;       // ring-out / orb out confirmed by the game
	UPROPERTY() bool bSettledIn = false;     // came to rest in bounds after the near miss
	UPROPERTY() FVector Location = FVector::ZeroVector;
};

/** Truth of one action (one launch until rest / Resolve). Real game events only. */
USTRUCT()
struct FZoogiGameplayTruthRecord
{
	GENERATED_BODY()
	UPROPERTY() int32 ActionId = 0;
	UPROPERTY() int32 Round = 0;
	UPROPERTY() int32 Turn = 0;
	UPROPERTY() bool bCombo = false;
	UPROPERTY() int32 Actor = -1;            // acting player index
	UPROPERTY() FString ActorName;
	UPROPERTY() int32 ActorTeam = 0;
	UPROPERTY() FVector LaunchStart = FVector::ZeroVector;
	UPROPERTY() FVector LaunchVelocity = FVector::ZeroVector;
	UPROPERTY() float LaunchPower = 0.f;     // fraction of the full launch speed
	UPROPERTY() EZoogiArtifact Awaken = EZoogiArtifact::None; // active infused artifact at launch
	UPROPERTY() bool bFollowUp = false;
	UPROPERTY() TArray<FZoogiContactEvent> Contacts;
	UPROPERTY() TArray<FZoogiAirSegment> Air;
	UPROPERTY() TArray<FZoogiBoundaryEvent> Boundary;
	UPROPERTY() TArray<int32> RingOuts;      // player indices that fell out this action
	UPROPERTY() int32 OrbOuts = 0;
	UPROPERTY() TArray<int32> ScoreDelta;    // per player
	UPROPERTY() TArray<int32> RingMarkDelta; // per player
	UPROPERTY() int32 ResonanceDelta = 0;    // acting team (2v2)
	UPROPERTY() float ControlDelta = 0.f;    // acting team arena control (2v2)
	UPROPERTY() uint32 ComboTriggers = 0;    // ZoogiComboTrigger flags earned (2v2)
	UPROPERTY() int32 PiecesDamaged = 0;     // state steps across all pieces
	UPROPERTY() int32 RocksDamaged = 0;
	UPROPERTY() bool bPowerHit = false;      // infused first contact happened
	UPROPERTY() bool bCrit = false;
	UPROPERTY() int32 PowerHitTarget = -1;   // body code of the infused contact
	UPROPERTY() float PowerHitTime = -1.f;
	UPROPERTY() FVector PowerHitLocation = FVector::ZeroVector;
	UPROPERTY() bool bGuardBreak = false;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() float MaxRelSpeed = 0.f;
	UPROPERTY() TArray<FVector> FinalBallPos;
	UPROPERTY() TArray<bool> FinalBallOut;
};

/** Recognition: what the action means. Deterministic from the truth record + tuning. */
USTRUCT()
struct FZoogiEpisodeRecognitionRecord
{
	GENERATED_BODY()
	UPROPERTY() int32 ActionId = 0;
	UPROPERTY() TArray<EZoogiEpisodeTag> Tags;
	UPROPERTY() TArray<float> TagPoints;     // points each tag contributed
	UPROPERTY() float Score = 0.f;
	UPROPERTY() int32 Tier = 0;              // 0 = nothing to present, 1..5
	UPROPERTY() EZoogiEpisodeArchetype Archetype = EZoogiEpisodeArchetype::None;
	UPROPERTY() FString Breakdown;
	UPROPERTY() FString Headline;
};

/** Plan: what the presentation layer did / does for the action. */
USTRUCT()
struct FZoogiCinematicPlan
{
	GENERATED_BODY()
	UPROPERTY() int32 ActionId = 0;
	UPROPERTY() int32 Tier = 0;              // highest tier presented live
	UPROPERTY() EZoogiEpisodeMode Mode = EZoogiEpisodeMode::Balanced;
	UPROPERTY() EZoogiPanelLayout Layout = EZoogiPanelLayout::None;
	UPROPERTY() int32 Panels = 0;
	UPROPERTY() int32 Callouts = 0;
	UPROPERTY() FString LastCallout;
	UPROPERTY() bool bGlow = false;
	UPROPERTY() bool bPowerEscalation = false;
	UPROPERTY() FLinearColor PowerColor = FLinearColor::White;
	UPROPERTY() bool bSlowMo = false;
	UPROPERTY() float SlowMoScale = 1.f;
	UPROPERTY() bool bMeteor = false;
	UPROPERTY() float MeteorSeconds = 0.f;
	UPROPERTY() FString Epilogue;            // shown only after physics confirmed the outcome
	// RULE ZERO proof
	UPROPERTY() bool bDilationRestored = true;
	UPROPERTY() float FreezeDriftCm = 0.f;   // largest position drift seen across a presentation freeze
	UPROPERTY() float FreezeDriftVel = 0.f;
	UPROPERTY() int32 Freezes = 0;
	bool ResultUnchanged() const { return bDilationRestored && FreezeDriftCm < 0.01f && FreezeDriftVel < 0.01f; }
};

/** Every drama number in one place. Defaults here; DefaultGame.ini [ZoogiRoll.Episode] may override any of them. */
struct FZoogiEpisodeTuning
{
	// contact significance
	float MeaningfulSpeed = 250.f;      // relative normal speed (cm/s) for a contact to count
	float FullWeightSpeed = 900.f;      // contact weight 1.0 at this speed (max 1.5)
	float DedupeSeconds = 0.25f;        // same pair inside this window = one contact
	float AirborneGap = 18.f;           // gap under the ball that counts as airborne (cm)
	float AirborneMinTime = 0.22f;
	float AirborneMinPeak = 26.f;
	float EdgeNearDist = 160.f;         // closer than this to the knockout line at speed ...
	float EdgeNearSpeed = 260.f;        // ... and stays in = BOUNDARY SAVE
	// tag points
	float PBank = 9.f, PDoubleBank = 16.f, PAllyBump = 12.f, POrbRelay = 18.f, PWallRedirect = 8.f, PBoundarySave = 14.f;
	float PAirborne = 10.f, PPowerHit = 20.f, PCrit = 8.f, PGuardBreak = 14.f, POrbOut = 12.f, PRingOut = 30.f;
	float PTerrain = 8.f, PTerrainExtra = 4.f, PPartnerFollowUp = 10.f;
	float RepeatFalloff = 0.5f;         // each extra instance of the same outcome tag is worth this much of the previous
	float SynergyPer = 6.f;             // per distinct tag category beyond the second
	float ChainBonus = 4.f;             // per causal depth beyond 1 (capped)
	float ChainBonusMax = 10.f;
	float ScoreSwingBonus = 6.f;        // acting player gained >= ScoreSwingPoints
	int32 ScoreSwingPoints = 100;
	// tiers (score thresholds)
	float T1 = 8.f, T2 = 22.f, T3 = 45.f, T4 = 70.f, T5 = 100.f;
	int32 T5MinCategories = 4;
	float FullScoreMul = 1.15f;         // Full mode: recognition score x this for presentation tiers
	// presentation
	float CalloutGap = 0.45f;           // real seconds between live callouts
	float SlowMoScale = 0.35f;          // tier 3 entry (CinematicTimeDilation; restored exactly)
	float SlowMoSeconds = 0.45f;        // real seconds
	float PanelSeconds = 1.9f;          // real seconds the panel composite stays (Balanced)
	float MeteorSeconds = 2.6f;         // Balanced meteor freeze (real seconds)
	float MeteorSecondsShort = 1.1f;    // Competitive
	float MeteorSecondsFull = 3.2f;
	float MeteorMaxDelay = 0.3f;        // game seconds after the hit; later = skip the meteor
	int32 PanelRes = 512;

	void Load();
};

namespace ZoogiEpisode
{
	ZOOGIROLL_API const TCHAR* TagName(EZoogiEpisodeTag T);       // "BANK", "DOUBLE_BANK" ...
	ZOOGIROLL_API const TCHAR* TagLabel(EZoogiEpisodeTag T);      // "BANK", "DOUBLE BANK" ...
	ZOOGIROLL_API const TCHAR* ModeName(EZoogiEpisodeMode M);
	ZOOGIROLL_API const TCHAR* LayoutName(EZoogiPanelLayout L);
	ZOOGIROLL_API const TCHAR* ArchetypeName(EZoogiEpisodeArchetype A);
	ZOOGIROLL_API const TCHAR* KindName(EZoogiContactKind K);
	/** Episode power palette (spec 9): Storm / Shock violet-electric, Void, Ember, Titan, Frost. */
	ZOOGIROLL_API FLinearColor PowerColor(EZoogiArtifact A);
	ZOOGIROLL_API FString PowerTitle(EZoogiArtifact A);
}
