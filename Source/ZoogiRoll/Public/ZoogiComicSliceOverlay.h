// Oct 9 (Evan 3:35 PM): comic-slice overlay. The cut comic panels / eye slices (Content/UI/ComicSlices, cut from the
// episode refs by Scripts/ui/comic_slices/cut_slices.py) slam in diagonally from a screen edge for ~1.2 s on big moments:
//   Sky Bumper rebound -> panel_sky_bumper_impact / panel_sky_bumper   Resonance Halo pass -> panel_halo_bank (rim -> halo_rebound)
//   Air Reaction offer -> panel_air_reaction   shatter -> panel_air_vs_stone   Orb Line success -> panel_orb_line
//   2v2 Partner Intercept success -> panel_partner_intercept
//   turn start (each player's first turn, Awakened turns) / Episode tier 4+ -> the shooter's eye slice by character colour
// Observation only: polls the public counters of UZoogiBattleStateMachine / UZoogiEpisodeSubsystem every frame and draws
// through AHUD::OnHUDPostRender (no hooks in ZoogiHUD / the state machine). RULE ZERO: purely visual, no RNG, no game writes.
// One slice on screen at most, 3 s cooldown after it leaves, never while a cinematic holds the view (Episode hold /
// camera-director main view / climax); an event during one waits up to 2.5 s for it to end.
// Toggles: [ZoogiRoll.Comic] bComicSlices=True (default on when missing), -ZoogiComicSlices=Off.
// Review: -ZoogiComicDemo cycles every slice; -ZoogiComicShots (with -ZoogiShots=<dir>) requests engine-viewport
// screenshots (comic_##_<slice>.png) while a slice is up (viewport + HUD only, never the desktop).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiComicSliceOverlay.generated.h"

class AHUD;
class UCanvas;
class UTexture2D;
class AZoogiGameMode;

UCLASS()
class ZOOGIROLL_API UZoogiComicSliceOverlay : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Config + command-line toggle. */
	static bool Enabled();
	/** Queues a slice (name without the T_ prefix, e.g. panel_halo_bank). Higher priority replaces a lower pending one. */
	void Trigger(FName Slice, int32 Priority, const TCHAR* Why, int32 Player = -1);
	/** Oct 14 (Evan): a player's turn-start eye strip is drawn small over that player's profile card in the team column
	 *  (UZoogiTeamColumnWidget paints it) instead of across the arena. True while that strip is up for Player. */
	bool ProfileSlice(int32 Player, UTexture2D*& OutTex, float& OutAlpha, float& OutScale) const;

	int32 Shown = 0;

private:
	void OnHUDPostRender(AHUD* HUD, UCanvas* Canvas);
	void Observe(AZoogiGameMode* G, double Now);
	bool Suppressed(const AZoogiGameMode* G) const;
	UTexture2D* Tex(FName Slice);
	FName EyesFor(const AZoogiGameMode* G, int32 Player) const;
	void ResetSeen();
	void Start(double Now);

	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> Textures;
	TSet<FName> Missing;
	FDelegateHandle hHUD;
	bool bOn = false;
	bool bDemo = false;
	bool bShots = false;
	FString ShotDir;
	int32 ShotsTaken = 0;

	// active slice (real time)
	FName Cur;
	int32 CurPlayer = -1, PendPlayer = -1;
	double CurStart = -100.0;
	double NextAllowed = 0.0;
	bool bCurShot = false;
	int32 SideFlip = 0;
	bool bEyes = false;

	// pending
	FName PendSlice;
	int32 PendPri = -1;
	double PendAt = 0.0;

	// observed counters
	bool bSeeded = false;
	int32 SeenBumper = 0, SeenHalo = 0, SeenRims = 0, SeenShatter = 0, SeenHistory = 0;
	bool bAirWas = false;
	int32 LastTurnKey = INT32_MIN;
	int32 LastBigAction = -1;
	TSet<int32> FirstTurnSeen;
	int32 BumperFlip = 0;
	int32 DemoIdx = 0;
};
