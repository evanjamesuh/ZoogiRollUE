#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;
class UPhysicalMaterial;
class UMaterialInterface;
class UStaticMesh;

/**
 * Oct 22 item 12: the Arabian Nights "floor lights up under the orb" look, shared by every map. It is the orb's own
 * shadowless point light (AZoogiOrb::Glow) pooling on the floor; each arena sets the tint / strength / radius here and
 * orbs pick it up on their next tick (serial check, no allocations). Star orbs keep their gold.
 */
struct FZoogiOrbFloorGlow
{
	FLinearColor Color = FLinearColor(0.25f, 0.85f, 0.95f); // #3FD8F2, the Arabian original
	FLinearColor AltColor = FLinearColor(0.25f, 0.85f, 0.95f); // every other orb (Night Circuit magenta / cyan)
	float Scale = 1.f;   // x the orb's base 7 cd (+-2.5 pulse)
	float Radius = 260.f;
};

/**
 * Oct 22 quality queue #13/#26: per-map impact particle tint + Niagara pick, and a soft fading ground mark colour.
 * Shared by every marble/orb impact (AZoogiArena::PlayMapImpact). No gameplay change.
 */
struct FZoogiMapImpactFX
{
	const TCHAR* Niagara = TEXT("NS_Sparks");
	const TCHAR* NiagaraAlt = TEXT("NS_Dust");
	FLinearColor Tint = FLinearColor(0.85f, 0.95f, 1.f);
	FLinearColor MarkTint = FLinearColor(0.18f, 0.14f, 0.10f);
	float ParticleScale = 1.f;
	float MarkLife = 1.6f;
};

/**
 * Oct 22 quality queue #14/#30: rim-light colour + ambient pulse. Rim lights sit just inside the knockoff radius;
 * ambient motion only pulses intensities / fog (no layout rewrite; Cosmic R2 structural leave-alone still applies).
 */
struct FZoogiArenaAmbient
{
	FLinearColor RimColor = FLinearColor(0.55f, 0.75f, 1.f);
	float RimCandela = 3.5f;
	float RimRadius = 520.f;
	float RimHeight = 55.f;
	int32 RimCount = 10;
	float PulseAmp = 0.18f;   // fraction of base candela
	float PulseHz = 0.35f;
	float FogWobble = 0.0f;   // relative fog-density amp (0 = off)
	float AmbientBurstSec = 0.f; // 0 = no ambient Niagara puffs
	const TCHAR* AmbientNiagara = TEXT("NS_Dust");
	FLinearColor AmbientTint = FLinearColor(1.f, 1.f, 1.f);
	float AmbientScale = 0.55f;
};

/**
 * Builds an arena and its sky at runtime: the floating playfield island
 * (floor top at Z = 0, open edge at ZoogiConst::KnockoffRadius), the rim
 * obstacles, dressing, and the light rig.
 */
UCLASS()
class ZOOGIROLL_API AZoogiArena : public AActor
{
	GENERATED_BODY()

public:
	AZoogiArena();

	void Build(EZoogiArena InTheme);
	EZoogiArena GetTheme() const { return Theme; }
	const TArray<TObjectPtr<class AZoogiObstacle>>& GetObstacles() const { return Obstacles; } // Oct 15: CPU hazard planning
	/** Rolling deceleration for marbles on this floor (ice is slippery). */
	float GetRollingDecel() const;
	FVector GetSpawnPoint(int32 PlayerIndex) const;
	float GetSpawnYaw(int32 PlayerIndex) const;
	float GetKnockoffRadius() const { return KnockoffRadiusOverride > 0.f ? KnockoffRadiusOverride : ZoogiConst::KnockoffRadius; }
	/** r19 per-map scale (Meadow 1.3): the arena actor and every piece are scaled; world radii below follow it. */
	float BuildScale = 1.f;
	float KnockoffRadiusOverride = 0.f;
	/** Per-map starting orb count on the ring (0 = ZoogiConst::OrbCount + centre triangle + extra spots). */
	int32 OrbCountOverride = 0;
	/** Knockoff radius of the arena built last (world cm), for code without an arena pointer (pieces, combo). */
	static inline float ActiveKnockoffR = ZoogiConst::KnockoffRadius;
	/** Orb floor glow of the arena built last (item 12); ActiveOrbGlowSerial bumps on every Build so orbs re-tint. */
	static FZoogiOrbFloorGlow OrbFloorGlowFor(EZoogiArena InTheme);
	static inline FZoogiOrbFloorGlow ActiveOrbGlow;
	static inline int32 ActiveOrbGlowSerial = 0;
	/** Oct 22 QQ #13/#26: map-aware impact burst + fading mark. */
	static FZoogiMapImpactFX MapImpactFXFor(EZoogiArena InTheme);
	static inline FZoogiMapImpactFX ActiveMapImpact;
	static void PlayMapImpact(UWorld* World, const FVector& At, const FVector& Normal, float Impulse);
	/** Oct 22 QQ #29: expanding ring ripple at the exit point (visual only). */
	static void SpawnRingOutRipple(UWorld* World, const FVector& ExitPoint);
	/** b179 (Evan: "Meadows is a flat plain - knocked out things should fall through the ground"): true on the Meadow (-ZoogiMeadowNoSink = off) */
	static bool OutsSinkThroughGround(UWorld* World);
	/** keeps a knocked-out body falling (gravity, its own speed) with no collision, so it drops through the floor */
	static void SinkThroughGround(UWorld* World, class UPrimitiveComponent* Body, const FVector& Vel);
	/** puts back the collision responses a sink switched off (call on respawn) */
	static void RestoreAfterSink(class UPrimitiveComponent* Body);
	/** Oct 22 QQ #14/#30: rim lights + ambient pulse table for the active theme. */
	static FZoogiArenaAmbient AmbientFor(EZoogiArena InTheme);
	static inline FZoogiArenaAmbient ActiveAmbient;
	virtual void Tick(float DeltaSeconds) override;
	/** Frozen Ring 2 (Oct 14): ring-out radius beyond the court (0 = off). Out = fell below FallOutZ, or r > OuterOutR when set. */
	static inline float OuterOutR = 0.f;
	/** Nov 1 b53: stands / bleachers / rafters around the court are solid now; anything past this radius (on top of them) is out (0 = off). */
	static inline float StandBlockR = 0.f;
	static bool IsRingOut(const FVector& L);
	/** Oct 31: Ancient Collision Meadows scoring portals - an orb inside a portal opening counts as knocked out (existing orb rules). */
	static bool IsPortalGoal(const FVector& L);
	static inline bool bAncientActive = false;
	/** Camera boom multiplier for the tactical / overview profiles (1 = default). */
	float CamScale = 1.f;
	void BuildFrozenRing2();
	void ApplyFrozenRR2Visuals(); // ZoogiFrozenRR2.cpp: rev_realism_v2 visual-only swap
	/** Extra orb spots off the main disc (Frozen bleacher islets, round 3c); world positions, Z = orb centre. Rebuilt per arena. */
	TArray<FVector> ExtraOrbSpots;
	/** Oct 29 (build 20): coloured power orbs on the court (Frozen: where the Resonance pillars stood). */
	TArray<FVector> PowerOrbSpots;
	TArray<FLinearColor> PowerOrbColors;
	/** Per-map starting orb ring (0 = ZoogiConst::OrbRingRadius) and its yaw offset in degrees; reset on every build. */
	float OrbRingRadiusOverride = 0.f;
	float OrbRingYawOffset = 0.f;
	/** True past the knockout line (circle, or the Night Circuit rectangle). */
	bool IsOffCourt(const FVector2D& P) const;
	/** False if a ball of this radius would overlap a solid court piece (Night Circuit pads/posts). */
	bool IsSpotClear(const FVector2D& P, float Radius) const;
	/** Distance from P along Dir to the knockout line. */
	float DistanceToEdge(const FVector2D& P, const FVector2D& Dir) const;
	/** True if a ball of the given radius rolling straight From -> To would hit a solid pad or post (Night Circuit). */
	bool IsPathBlocked(const FVector2D& From, const FVector2D& To, float Radius) const;
	bool IsNeon() const { return Theme == EZoogiArena::Neon; }
	bool IsNeonRect() const { return false; } // Oct 7: Night Circuit is round now; the old rectangular-court special cases key off this
	/** Destructible rocks placed by this arena (Meadow rim). Restored at match start. */
	void ResetRocks();
	const TArray<TObjectPtr<class AZoogiRock>>& GetRocks() const { return Rocks; }
	class AZoogiRock* NearestStandingRock(const FVector2D& From) const;

	// ---- Frozen Ring (Oct 6): destructible stage pieces, reactive floor scars, stage-forming intro ----
	/** Reusable stage-forming component (any arena can register pieces; Frozen does). */
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UZoogiStageForming> Forming;
	/** Resets every piece / scar and plays the forming intro (match start). */
	void BeginForming(float TimeScale = 1.f);
	bool IsForming() const;
	void ResetPieces();
	/** Build 28 (Evan): add the newer map additions (star bumpers, layout-table obstacles, Frozen crowd, Cosmic ship pad) to the opening stage-forming animation. Returns how many were added. */
	int32 AddLateForming();
	const TArray<TObjectPtr<class AZoogiArenaPiece>>& GetPieces() const { return Pieces; }
	class AZoogiFloorScars* GetScars() const { return Scars; }
	/** Oct 22: this map's launch pad (raised platform outside the knockoff edge); null before Build. Spawn players at GetLaunchPad()->LaunchPoint. */
	class AZoogiLaunchPad* GetLaunchPad() const { return LaunchPad; }
	/** Oct 24 dev mode: movable layout props (star bumpers + the launch ship). World cm, Z ignored for bumpers. */
	struct FZoogiDevProp
	{
		FName Type;            // "star_bumper" | "ship"
		FString Id;            // Oct 27: stable id (dev edits / export): "ship", "bumper_<n>"
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.f;
		float Scale = 1.f;
		TArray<TWeakObjectPtr<class UPrimitiveComponent>> Comps;
		// Oct 29 (Evan): "dev mode allows any obstacle to be selected" + active bumpers with a rebound level
		TWeakObjectPtr<AActor> Actor;      // "obstacle": the arena piece / obstacle / rock / battle prop it moves
		FTransform Base;                   // "obstacle": its transform as built (RESET / unchanged = not saved)
		float PickZ = 70.f;                // "obstacle": pick point height above Location
		bool bBumper = false;              // active bumper (ZoogiBumpers.h)
		int32 Bounce = 5;                  // rebound level 1..10
		bool IsMovedObstacle() const;
	};
	/** Oct 29: every obstacle on the court becomes a dev prop ("obstacle", ids like arch_2 / pillar_1 / rock_3) once the stage has
	 *  formed; later calls add props spawned since (battle halos / gates). Saved layout entries for them apply then. */
	void RegisterObstacleProps();
	bool bObstaclesRegistered = false;
	TArray<FZoogiDevProp> PendingObstacles;
	bool ApplyObstacleEntry(const FZoogiDevProp& E);
	TArray<FZoogiDevProp> DevProps;
	/** Oct 27: the layout as shipped (code + baked JSON) before any dev override - Dev Mode RESET goes back to it. */
	TArray<FZoogiDevProp> BaselineProps;
	int32 AddStarBumperProp(const FVector& WorldLoc, float Yaw, float Scale, const FString& Id = FString());
	void RemoveDevProp(int32 Index);
	void UpdateDevProp(int32 Index);   // re-applies Location/Yaw/Scale to the components (or the launch pad)
	void ClearStarBumperProps();
	/** Applies a layout (baked defaults or a dev JSON): replaces every star bumper and moves the ship. */
	void ApplyLayout(const TArray<FZoogiDevProp>& Layout);
	// Oct 27 Frozen Ring roam (ZoogiFrozenRoam.cpp): no ship; spawn points + bridge launch points as dev props ("spawn" / "launch")
	bool IsFrozenRoam() const { return bFrozenRoam; }
	void SetFrozenRoam(bool bOn);
	int32 NumRoamProps(FName Type) const;
	const FZoogiDevProp* FindRoamProp(FName Type, int32 Index) const;
	void UpdateRoamProp(int32 Index);
	/** Dev-only markers (spawn pads, gate width bars) shown while the Dev Mode panel is open. */
	void SetDevMarkersVisible(bool bOn);
	/** Types the dev tools may move but never delete / duplicate. */
	static bool IsFixedPropType(FName Type) { return Type != TEXT("star_bumper"); }
	bool bFrozenRoam = false;
	FString GetArenaSlug() const;
	class AZoogiArenaPiece* SpawnPiece(const FVector& Loc, float Yaw, FName Kind, FName Surface, float HP, bool bIndestructible = false, UClass* PieceClass = nullptr);
	/** Frozen materials by key (Ice, IceDeep, IcePane, Snow, SnowShade, Stone, StoneDark, Wood, WoodDark, Cliff, Rune). */
	UMaterialInterface* FrozenMat(FName Key) const;
	class UMaterialInstanceDynamic* GetFloorMID() const { return FloorMID; }

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SkySphere;

protected:
	void Clear();
	void SetupSky(EZoogiArena InTheme);
	void BuildMeadow();
	/** b62 (Evan): push the bleachers / scenery ring out by GapW (world cm) so a Zoogi can still fall off between court and stands. */
	void WidenStandGap(float GapW);
	void BuildFrozen();
	UMaterialInterface* MakeFrozenMat(FName Key, const FLinearColor& Color, const TCHAR* Detail, float DetailScale, float DetailStrength,
		float Roughness, float Specular, const FLinearColor& Emissive, float EmissiveStrength);
	USceneComponent* NewPivot(const FVector& Loc);
	class UProceduralMeshComponent* AddFrozenProc(const struct FZoogiMeshBuilder& M, UMaterialInterface* Mat, USceneComponent* Parent, bool bShadow);
	UPROPERTY() TArray<TObjectPtr<class AZoogiArenaPiece>> Pieces;
	UPROPERTY() TObjectPtr<class AZoogiFloorScars> Scars;
	UPROPERTY() TObjectPtr<class AZoogiLaunchPad> LaunchPad;
	void PlaceLaunchPad(); // ZoogiArenaLaunchPad.cpp
	void PlaceLegacy012(); // ZoogiArenaLegacy012.cpp: Teaser-only legacy 0.1.2 ship
	void ApplyLayoutOverrides(); // ZoogiArenaProps.cpp
	void PlaceStarProps(); // ZoogiArenaProps.cpp: Oct 24 star bumpers on every map + baked/dev layouts
	UPROPERTY() TObjectPtr<class UPhysicalMaterial> StarBumperPM;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> FloorMID;
	UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> FrozenMats;
	float FloorGlowTarget = 1.6f;
	float SpawnRadiusOverride = 0.f;
	void BuildVolcanic();
	void AddInfernoIsland(const FVector& At, float Scale, float Yaw); // Nov 1: Inferno World island beside the Volcanic court (ZoogiArenaInferno.cpp)
	void BuildCosmic();
	/** Cosmic: animated space-journey backdrop (nebula sky sphere, GPU star streaks, drifting planets). False if assets are missing. */
	bool AddSpaceBackdrop();
	void BuildArabian();
	void BuildTomb();
	void BuildNeon();
	void BuildTraining();
	void BuildTeaser(); // Oct 23: Zoogi Roll Teaser star-bumper plateau (ZoogiArenaTeaser.cpp)
	void BuildAncient(); // Oct 31: Ancient Collision Meadows multi-island arena (ZoogiArenaAncient.cpp, layout ZoogiAncientLayout.h)
	void TickAncient(float Dt); // blimps
	void BuildAncientLook(); // Oct 31: Battle Royale reference look (ZoogiArenaAncientLook.cpp)
	void BuildAncientV14();
	bool BuildLeagueCourt(); // b163: the Training arena = the League Training Court (ZoogiArenaLeagueCourt.cpp; -ZoogiOldTraining = old)
	bool bLeagueCourt = false; // Nov 1: Grok's Blender arena v14 is the playfield (ZoogiArenaAncientV14.cpp; -ZoogiAncientV2 = old)
	void BuildAncientMeadow(); // Nov 1: v2 sunset meadow look (ZoogiArenaAncientMeadow.cpp; -ZoogiAncientV1 = the Oct 31 look)
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> AncMovers;
	TArray<FVector4> AncMoverPath; // radius, z, deg/s, phase deg
	float AncClock = 0.f;
	void PlaceIncoming(); // Oct 7: reskin-incoming builder packs on the non-Meadow maps
	void PlaceObstacles(); // Oct 14: reskin-incoming obstacle sets (ZoogiArenaObstacles.cpp); unbreakable, not pieces
	UPROPERTY() TArray<TObjectPtr<class AZoogiObstacle>> Obstacles;
	/** Axis-aligned box (min/max XY in cm, height) with optional collision. */
	UStaticMeshComponent* AddBox(const FVector2D& Min, const FVector2D& Max, float Bottom, float Top, UMaterialInterface* Mat, bool bCollide);
	TArray<FBox2D> SolidBoxes;
	TArray<FVector2D> SolidPosts;
	void AddStars(int32 Count, float SizeMin, float SizeMax, const FLinearColor& Color, int32 Seed);
	class UPointLightComponent* AddPointLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius);
	/** Solid round bumper: invisible collider cylinder + visual mesh scaled to the footprint. */
	void AddBumper(const FVector2D& XY, float Radius, float Height, UStaticMesh* Mesh, UMaterialInterface* Mat, const FVector& VisualScale, float VisualZ, float Restitution);

	/** Floating island: top disc, rim band and a craggy underside. TopOverride replaces the noise floor material (visual only). */
	void BuildIsland(const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Band, const FLinearColor& RockA, const FLinearColor& RockB, float FloorFriction, UMaterialInterface* TopOverride = nullptr);
	void BuildRim(const TArray<FVector4>& Marks, const FLinearColor& A, const FLinearColor& B, float Restitution, bool bDestructible = false,
		float CarvedLump = 0.f, const FLinearColor& RuneColor = FLinearColor::Black, float RuneStrength = 0.f);
	void BuildPads();
	void BuildIslet(const FVector& Center, float Radius, const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Rock, UStaticMesh* Tree, int32 TreeCount, float TreeHeight, FRandomStream& Rng);
	void ScatterInstances(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten = false);

	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide, bool bShadow = true);
	/** Basic Cylinder resized: radius, height, top Z. */
	UStaticMeshComponent* AddCylinder(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat, bool bCollide);
	/** Basic Cone flipped tip-down under a disc. */
	UStaticMeshComponent* AddUnderCone(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat);
	UStaticMeshComponent* AddTree(UStaticMesh* Tree, const FVector& Base, float Height, float Yaw);

	// ---- Buzzy arena kit (ZoogiArenaKit.cpp, Oct 6 re-skin). All visual: no colliders, no gameplay change. ----
	/** MID of /Game/Env/Arenas/M_ZoogiArenaFloor with T_<Key>Floor / T_<Key>Glow, or nullptr if the assets are missing. */
	UMaterialInterface* KitFloorMaterial(const TCHAR* Key, const FLinearColor& GlowColor, float GlowStrength, float BaseBright = 1.f);
	/** Painted panorama sky sphere (M_ArenaSky + T_<Key>Sky). False if missing. */
	bool AddPanoramaSky(const TCHAR* Key, float Bright, float NadirDim, const FLinearColor& NadirColor);
	class UProceduralMeshComponent* NewProcMesh(bool bShadow, bool bCameraBlock = false);
	UStaticMeshComponent* AddBoxR(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bShadow = true);
	/** Terrace ring outside the knockout line: flat at Z out to FlatTo (K14: stays below -100 near the edge), rising to RimZ at ROut. */
	void AddTerrace(float RIn, float ROut, float Z, float FlatTo, float RimZ, UMaterialInterface* Top, UMaterialInterface* Wall, UMaterialInterface* Under);
	float TerraceHeight(float R, float AngleRad) const;
	/** Terrace drop channels (Meadow orb-exit gaps): the terrace ring is cut open +-TerraceGapHalfDeg around each yaw. Set before AddTerrace. */
	TArray<float> TerraceGapYaw;
	float TerraceGapHalfDeg = 0.f;
	bool InTerraceGap(float AngleRad) const;
	void ScatterTerrace(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten);
	void AddCurb(float ROut, float TopZ, float Depth, UMaterialInterface* Mat);
	void AddStands(float CenterDeg, float ArcDeg, float RIn, float BaseZ, int32 Tiers, float TierDepth, float TierRise, UMaterialInterface* Seat, UMaterialInterface* SeatAlt, UMaterialInterface* Riser, UMaterialInterface* Stone);
	void AddPagoda(const FVector& Base, float Yaw, int32 Storeys, float Size, UMaterialInterface* Body, UMaterialInterface* Panel, UMaterialInterface* Roof, UMaterialInterface* Trim, UMaterialInterface* Stone, UMaterialInterface* Gold);
	void AddShrineGate(const FVector& Base, float Yaw, float Scale, UMaterialInterface* PostMat, UMaterialInterface* Beam, UMaterialInterface* Emblem, UMaterialInterface* Portal, UMaterialInterface* Stone);
	void AddResonancePillar(const FVector& Base, float Yaw, float Height, UMaterialInterface* Stone, UMaterialInterface* Slit, UMaterialInterface* Cup, UMaterialInterface* Orb, UMaterialInterface* Vine);
	/** Floating spinning crystal (no collider, no shadow, tagged NoCamBlock). */
	void AddHubCore(const FVector& Center, float HalfHeight, UMaterialInterface* Crystal);
	void AddGem(USceneComponent* Parent, const FVector& Center, float HalfHeight, float Radius, int32 Sides, UMaterialInterface* Mat, bool bShadow);
	void AddWaterfall(const FVector& Top, float Yaw, float Width, float Height, float Lip, UMaterialInterface* MatOverride = nullptr);
	UMaterialInterface* KitLavaMaterial(float TileU, float TileV, float SpeedU, float SpeedV, float Bright, float CrustAmount);
	void AddObsidianOutcrop(const FVector2D& P, float Rad, float H, float Yaw, int32 Seed, UMaterialInterface* Rock, UMaterialInterface* Crack);
	void AddFortressWall(float CenterDeg, float ArcDeg, float RIn, float Thick, float Height, float GateHalfWidth, UMaterialInterface* Stone, UMaterialInterface* Trim, UMaterialInterface* Slit);
	void AddFortressGate(const FVector& Base, float Yaw, float Height, UMaterialInterface* Stone, UMaterialInterface* Trim, UMaterialInterface* Emblem, UMaterialInterface* Portal);
	void AddLavaChannel(float Deg, float R0, float R1, float Width, UMaterialInterface* Mat);
	void AddBridge(const FVector& A, const FVector& B, float Width, float Sag, UMaterialInterface* Plank, UMaterialInterface* Rope);
	void AddMagmaCore(const FVector& Center, float Radius, UMaterialInterface* Mat, const FLinearColor& LightColor, float Candela, float LightRadius);
	void AddFillLight(const FVector& Pos, const FLinearColor& Color, float Candela, float Radius);
	float TerraceZ = -160.f, TerraceFlatTo = 2100.f, TerraceRimZ = -40.f, TerraceRIn = 1680.f, TerraceROut = 2400.f;

	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Built;
	UPROPERTY() TArray<TObjectPtr<class AZoogiRock>> Rocks;
	/** Poppable pizza toppings (Arabian court). Destroyed in Clear(), restored with the rocks in ResetRocks(). */
	UPROPERTY() TArray<TObjectPtr<class AZoogiTopping>> Toppings;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> FloorPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> RockPM;
	EZoogiArena Theme = EZoogiArena::Meadow;
	/** Oct 22 QQ #14: soft rim lights around the knockoff radius (shadowless). */
	void AddArenaRimLights();
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> RimLights;
	TArray<float> RimLightBase;
	float AmbientClock = 0.f;
	float NextAmbientBurst = 0.f;
	float FogDensityBase = 0.f;
};

// b178e: THE UNDERREALM - the Cosmic court dressed as Grok's Shadow Realm v3 (Episode 4 stop 1)
namespace ZoogiUnderrealm { ZOOGIROLL_API void Set(bool bOn); ZOOGIROLL_API bool On(); ZOOGIROLL_API void SetPicked(bool bOn); ZOOGIROLL_API bool Picked(); } // b203: SetPicked = the DARK REALM map card
