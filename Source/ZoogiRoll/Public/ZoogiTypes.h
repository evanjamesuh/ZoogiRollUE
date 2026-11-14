#pragma once

#include "CoreMinimal.h"
#include "ZoogiTypes.generated.h"

class UMaterialInstanceDynamic;
class UStaticMesh;
class AZoogiOrb;

/** Arenas from the web game. Only some are built so far; see AZoogiArena. */
UENUM(BlueprintType)
enum class EZoogiArena : uint8
{
	Meadow,
	Frozen,
	Volcanic,
	Cosmic,
	Arabian,
	Tomb,
	Neon,
	Training, // Oct 7: Training Arena (Meadow r22 layout at A 1.3 + builder pack)
	Teaser, // Oct 23: Teaser Arena (Zoogi Roll Teaser star-bumper plateau, A 1.3, arena id 'teaser')
	Ancient, // Oct 31: Ancient Collision Meadows (multi-island floating arena, arena id 'ancient', ZoogiArenaAncient.cpp)
	MAX UMETA(Hidden)
};

/** Zoogi powers (ZOOGI_ROSTER abilities; since Oct 6 carried by the 8 bible characters, see Docs/BIBLE_CHARACTERS.md). */
UENUM(BlueprintType)
enum class EZoogiPower : uint8
{
	WolfPack,     // Wolfgang: three clones chase the nearest orb or rival
	Explosion,    // Hotstreak: fiery blast pushes everything nearby away
	Ricochet,     // Lars: after the next hit, bounce toward the nearest target
	LockOn,       // Pinpoint: the shot snaps onto the target in the pull direction
	StaticShock,  // Bolt: phase through rivals this shot and stun them
	BandageBind,  // Wraps: rivals you touch are slowed and bound (weaker next shot)
	ShadowStun,   // Dark Zoogy (was Nightshade): shadow pulse freezes nearby rivals, they skip a turn
	RuneSeal      // Sera: rune pulse (Shadow Stun's radius check, smaller) that puts the Bind curse on nearby rivals
};

/** One playable Zoogi. Stats and abilities are ported from ZOOGI_ROSTER in useZoogiGame.tsx. */
USTRUCT(BlueprintType)
struct FZoogiDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Type;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Secondary = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Ability;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString AbilityDesc;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Speed = 60;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Power = 60;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Defense = 60;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 Control = 60;
	/** Nightshade: keep the file pivot (ball centre) instead of fitting the bounds (horns stick out). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPivotFit = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EZoogiPower PowerKind = EZoogiPower::Explosion;
	/** Bible character asset name: loads /Game/Characters/Bible/<MeshAsset>/SM_<MeshAsset> (+ SM_<MeshAsset>_Nubs).
	 *  Empty = the old /Game/Zoogis/<Id>/SM_<Id> path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MeshAsset;
	/** Extra yaw (degrees) so the face looks along the aim. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ModelYaw = 0.f;

	/** zoogiMassFromDefense: defense / 60, clamped 0.55..1.8 (60 defense = reference body). */
	float MassRatio() const { return FMath::Clamp(Defense / 60.f, 0.55f, 1.8f); }
};

/**
 * Units: 1 web-game unit (one Zoogi diameter) = 100 cm.
 * Numbers come from simFeel.ts, restHeight.ts, arenaConstants.ts and roundRim.ts.
 */
namespace ZoogiConst
{
	constexpr float ZoogiRadius = 50.f;          // ZOOGI_DRAW_RADIUS 0.5
	constexpr float OrbRadius = 40.f;            // ORB_DRAW_RADIUS 0.4
	constexpr float FloorRadius = 1520.f;        // ROUND_FLOOR_RADIUS 15.2
	constexpr float KnockoffRadius = 1550.f;     // ROUND_KNOCKOFF_RADIUS 15.5
	constexpr float RollingDecel = 450.f;        // ROLLING_DECEL 4.5 u/s^2
	constexpr float IceRollingDecel = 115.f;     // ICE_ROLLING_DECEL 1.15 u/s^2
	constexpr float FullLaunchSpeed = 3600.f;    // FULL_LAUNCH_SPEED_PER_SEC 36
	constexpr float FullPull = 1500.f;           // FULL_PULL_DISTANCE 15
	constexpr float SlingshotPull = 0.8f;        // SLINGSHOT_PULL
	constexpr float RestSpeed = 30.f;            // below this (cm/s) a grounded body is snapped to rest
	constexpr float SettleDelay = 1.5f;          // SETTLE_DELAY_SECONDS
	constexpr float FallOutZ = -600.f;           // FALL_OUT_Y -6
	constexpr float MassKg = 10.f;               // kg per unit of web-game mass
	constexpr float OrbMassRatio = 0.512f;       // (0.4 / 0.5)^3
	constexpr float SpawnRadius = 1100.f;        // start pads, inside the floor
	constexpr float OrbRingRadius = 500.f;       // createOrbs(ringRadius = 5)
	constexpr int32 OrbCount = 30; // Oct 23 Evan: field orbs x2 (was 15)
	constexpr int32 FrozenOrbTotal = 48; // Oct 24 Evan: every map tops up to Frozen's total (30 ring + 6 centre + 12 islet)
	constexpr int32 ScoreOrb = 50;               // KNOCKOUT_SCORE_ORB
	constexpr int32 ScorePlayer = 100;           // KNOCKOUT_SCORE_PLAYER
	constexpr int32 PenaltyFall = 75;            // KNOCKOUT_PENALTY
	/** Ground drag is multiplied by this before it becomes "pull", so a full pull fits on screen. */
	constexpr float DragScale = 2.0f;

	/** launchSpeedPerSecForPull, in cm/s. Quadratic through 80%, then a smoothstep rise to a full shot. */
	ZOOGIROLL_API float LaunchSpeedForPull(float PullFraction);
}

namespace ZoogiUtil
{
	ZOOGIROLL_API FLinearColor Hex(const TCHAR* Hex);
	ZOOGIROLL_API const TArray<FZoogiDef>& Roster();
	ZOOGIROLL_API const FZoogiDef* FindZoogi(FName Id);
	ZOOGIROLL_API FLinearColor PlayerColor(int32 PlayerIndex);
	/** Nov 1 (Evan: all four logos, one per season): MenuArt key of this season's ZOOGI ROLL ARENA logo (logo_spring / summer / fall / winter). */
	ZOOGIROLL_API FString SeasonLogoKey();
	/** Maps the old single logo key (zoogi-roll-arena-logo) to the season's logo; any other key is returned unchanged. */
	ZOOGIROLL_API FString MenuArtKey(const FString& Key);
	/** 2v2 Combo: team palette (A = sky / indigo, B = rose / orange) instead of the four free-for-all colours. */
	ZOOGIROLL_API void SetComboColors(bool bOn);
	ZOOGIROLL_API FString ArenaName(EZoogiArena Arena);

	/** MID of /Game/Materials/M_ZoogiSurface (or the engine BasicShapeMaterial if the project material is missing). */
	ZOOGIROLL_API UMaterialInstanceDynamic* SurfaceMID(UObject* Outer, const FLinearColor& Color, float Roughness = 0.6f,
		const FLinearColor& Emissive = FLinearColor::Black, float EmissiveStrength = 0.f, float Metallic = 0.f);
	/** MID of /Game/Materials/M_ZoogiGlow: emissive body with a fresnel rim (orbs, pads, rings). */
	ZOOGIROLL_API UMaterialInstanceDynamic* GlowMID(UObject* Outer, const FLinearColor& Color, float Strength);
	/** MID of /Game/Materials/M_ZoogiGround: two colours blended by world-space noise. */
	ZOOGIROLL_API UMaterialInstanceDynamic* GroundMID(UObject* Outer, const FLinearColor& A, const FLinearColor& B, float NoiseScale = 0.002f, float Roughness = 0.9f);

	/** MID of /Game/Materials/M_ZoogiFX: additive unlit glow for power effects (falls back to GlowMID). */
	ZOOGIROLL_API UMaterialInstanceDynamic* FxMID(UObject* Outer, const FLinearColor& Color, float Strength);
	/** True if the power is armed before/while shooting rather than fired instantly. */
	ZOOGIROLL_API bool IsArmedPower(EZoogiPower Power);
	/** Project toggle for the bible characters' visual-only flipper nubs: Config/DefaultGame.ini
	 *  [ZoogiRoll.Characters] ShowFlipperNubs=True|False; -ZoogiNoNubs / -ZoogiNubs override it on the command line. */
	ZOOGIROLL_API bool ShowFlipperNubs();
	/** Oct 8 (Evan 9:16 PM): round sparkles on/around the Zoogis (fast-roll Niagara spark bursts, Awakened aura bursts + the round artifact
	 *  orbiter motes/rings). [ZoogiRoll.Characters] ZoogiSparkles=True|False; -ZoogiSparkles / -ZoogiNoSparkles override. Visual only. */
	ZOOGIROLL_API bool ShowZoogiSparkles();
	/** Oct 15 (Evan 12:07 AM): "remove reaction pauses for the time being" - every collision-time pause / freeze / hit-stop /
	 *  slow-mo / wait (Episode presentation holds incl. the Air Reaction hold, anime freeze frames, camera hit-stop and lock
	 *  slow-mo) is skipped while this is false, so physics runs at full speed. Air Reactions still happen live (no wait).
	 *  [ZoogiRoll.Awakening] bReactionPauses=False (default); -ZoogiReactionPauses=On|Off overrides for one run. */
	ZOOGIROLL_API bool ReactionPauses();
	/** Oct 8 (Evan 9:16 PM): breakable arena pieces / rocks toughness. [ZoogiRoll.Arena] BreakToughness (1.0 = old tuning,
	 *  clamped 0.5..3): AZoogiArenaPiece max HP and AZoogiRock crack / shatter speeds scale by it; indestructible pillars ignore it.
	 *  -ZoogiBreakToughness=<x> overrides for one run. */
	ZOOGIROLL_API float BreakToughness();
	/** Oct 8 (Evan 11:15 PM): destruction switch. [ZoogiRoll.Arena] bDestruction=False -> arena pieces (walls, crystal posts,
	 *  buildings...) take no damage and rocks never crack / shatter, on every map; stage forming is unaffected.
	 *  -ZoogiDestruction=On / Off overrides for one run. */
	ZOOGIROLL_API bool DestructionEnabled();
	/** Oct 23 (Evan): [ZoogiRoll.Characters] OrbStyle=Original = Tripo /Game/Orbs/SM_Orb as Visual scaled to
	 *  ZoogiConst::OrbRadius (40 cm), materials kept; Core hidden. Star orbs: gold light tint only. Any other value
	 *  (or -ZoogiOrbStyle=Current) leaves AZoogiOrb's own look. Visual only (no collision / mass change). */
	ZOOGIROLL_API void ApplyOrbStyle(AZoogiOrb* Orb);
	/** Oct 24 (Evan): Oct 7 Blue-shell glass orb look (cfc9388) at OrbRadius; star orbs get the gold core/light. */
	ZOOGIROLL_API void ApplyBlueGlassOrb(AZoogiOrb* Orb);
	/** Oct 24 (Evan): Blue monkey face orb - SM_OrbMonkey at OrbRadius with the Blue glass shell MI + Blue core. */
	ZOOGIROLL_API void ApplyBlueMonkeyOrb(AZoogiOrb* Orb);

	/** Plays /Game/Audio/S_<Name> as a 2D sound (no-op if missing or with -NoSound). */
	/** 2D SFX from /Game/Audio/S_<Name>. StartTime skips into the sound (e.g. so a wind-up ends exactly on the launch).
	 *  Plays as a UI sound so anime freezes (game paused) don't swallow it. Scaled by the SFX volume setting. */
	ZOOGIROLL_API void PlaySfx(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f, float StartTime = 0.f);
	/** b70 (Evan: "sound doesn't linger"): the same sound, faded out after MaxSeconds. */
	ZOOGIROLL_API void PlaySfxShort(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch, float MaxSeconds);
	/** SFX volume setting 0..1 (menu "SFX" row, or -ZoogiSfxVol=0..100). */
	ZOOGIROLL_API float GetSfxVolume();
	ZOOGIROLL_API void SetSfxVolume(float V);
	/** Music volume setting 0..1 (clash-menu Settings "Music" slider): scales the menu loop and the match stems. */
	ZOOGIROLL_API float GetMusicVolume();
	ZOOGIROLL_API void SetMusicVolume(float V);
	ZOOGIROLL_API class USoundBase* LoadSound(const TCHAR* Name);

	ZOOGIROLL_API UStaticMesh* BasicShape(const TCHAR* Name); // Sphere, Cube, Cylinder, Cone, Plane
	ZOOGIROLL_API UStaticMesh* LoadMesh(const FString& Path);
}
