// Milestone "Awaken": anime sequence director.
// Gameplay emits combat events; this subsystem picks a sequence id from the anime matrix (fallback chain), decides Full / Short / skip,
// freezes the world for impact moments (snapshot + restore so physics continues exactly where it stopped) and plays a placeholder:
// a real-time HUD card + camera shot, or Content/Movies/<id>.mp4 when that file exists (alpha-free test clips for now).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiArtifacts.h"
#include "ZoogiAnimePanels.h"
#include "ZoogiAnimeCues.h"
#include "ZoogiAnimeSubsystem.generated.h"

class AZoogiBall;
class UMediaPlayer;
class UCanvas;
class UMediaTexture;
class UFileMediaSource;

/** Event names follow the matrix: Infuse, Guard, GuardBlock, PoweredHit, Critical, KO, Combo, Miss, Status_<X>, Ultimate. */
USTRUCT(BlueprintType)
struct FZoogiCombatEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FName Event;
	UPROPERTY(BlueprintReadWrite) FName ZoogiId;            // attacker / owner ("hotstreak")
	UPROPERTY(BlueprintReadWrite) FString ZoogiName;        // "Hotstreak"
	UPROPERTY(BlueprintReadWrite) EZoogiArtifact Artifact = EZoogiArtifact::None;
	UPROPERTY(BlueprintReadWrite) int32 Player = -1;
	UPROPERTY(BlueprintReadWrite) int32 TargetPlayer = -1;
	UPROPERTY(BlueprintReadWrite) FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite) float Magnitude = 1.f;
	/** Impact moment: freeze the world while it plays (PoweredHit / Critical / GuardBlock). */
	UPROPERTY(BlueprintReadWrite) bool bFreeze = false;
	UPROPERTY(BlueprintReadWrite) bool bCPUOnly = false;    // nobody human involved -> always Short
	/** Nov 1 (Evan): skill (Awaken infuse) + crit reward -> always the Full clip (random Anime_Any_Any_Critical_0N), shatter transition into it. */
	UPROPERTY(BlueprintReadWrite) bool bRewardShatter = false;
	TWeakObjectPtr<AActor> Focus;                           // camera focus (active Zoogi / target)
};

/** One matrix row (DataTable /Game/Data/DT_AnimeSequences, or Content/Data/AnimeSequences.csv, or built-in defaults). */
USTRUCT(BlueprintType)
struct FZoogiAnimeRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Event;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Tier = TEXT("Micro"); // Micro / Standard / Special
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float Length = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ShortLength = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 FullPlaysPerMatch = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Fallback;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Status;               // Needed / Placeholder / Done
};

UENUM()
enum class EZoogiAnimeMode : uint8
{
	Off,
	Short,
	Full
};

UCLASS()
class ZOOGIROLL_API UZoogiAnimeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiAnimeSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickableWhenPaused() const override { return true; }

	void Emit(const FZoogiCombatEvent& E);
	/** Oct 4 anime SFX: every combat event plays its sound at emission (also with Anime=Off), real-time and
	 *  independent of the simulation. Crit / KO / Combo boom assets open with a ~0.12 s silent beat (the held-pose
	 *  micro-pause before the hit lands) and dip the music. */
	void PlayEventSfx(const FZoogiCombatEvent& E);
	/** A sequence is playing (gameplay waits before the next phase change). */
	bool IsBusy() const { return bActive || CrashState > 0 || ZoogiAnimeCues::Busy(); } // b164: + a director cue
	/** b59 (Evan): the clash loser's crash - SMOKE wipes in, the per-map anime crash clip plays full screen (tinted to the loser),
	 *  smoke wipes out back to the game. False when the clip is missing or a cut-in is playing. */
	bool PlayCrash(const FString& Id, const FLinearColor& Tint);
	/** b145a (Evan: the Hotstreak Awaken "from full screen to top card view"): plays Content/Anime/Movies/<Id>.mp4 once, whole, in the
	 *  top-centre anime card (never an avatar circle) with Caption under it; the game keeps running. False when the clip is missing. */
	bool PlayTopCard(const FString& Id, const FString& InCaption, int32 Player, const FLinearColor& Tint);
	bool bTopCard = false;
	/** The crash clip for an arena (Content/Anime/Movies/Anime_Crash_<map words>*.mp4), empty when none. */
	FString CrashClipFor(uint8 Arena) const;
	bool CrashOn() const { return CrashState > 0; }
	/** b62 (Evan: "each super power needs its own 15 sec epic intro clip - for now Wolfgang and Meteor Strike"): plays
	 *  Anime_Awaken_<Key>_01.mp4 full screen behind the smoke wipe (tap to skip), the first time each player uses it in a match. */
	bool PlayPowerIntro(const FString& Key, int32 Player, const FLinearColor& Tint);
	TSet<FString> IntroSeen;
	/** b70 (Evan): a skill's anime is on - gameplay stops under it */
	bool PowerIntroOn() const { return CrashState > 0 && CrashId.StartsWith(TEXT("Anime_Awaken_")); }
	/** b88 (Evan: "move the wolf skill animation to play in that player's avi space - I can't see the target when it plays"): the
	 *  Wolf Pack summon clip runs inside the caster's avatar circle (ZoogiArenaHUDWidget swaps the avatar brush to MovieTexture).
	 *  True while its frames show; -ZoogiWolfClipTop = the top card. */
	bool AviClip(int32& OutPlayer) const;
	int32 IntroPlayer = -1;
	/** b70: runs Fn when the current crash / power clip has ended (now when none is playing) */
	void AfterCrash(TFunction<void()> Fn);
	TArray<TFunction<void()>> AfterCrashFns;
	float CrashSeekTo = -1.f, CrashSegStart = 0.f;
	TMap<FString, int32> LastIntroSeg;
	void DrawCrash(UCanvas* C);
	/** Click / Space: skip (after the minimum 0.3 s). */
	void Skip();
	void ResetMatch();
	EZoogiAnimeMode Mode = EZoogiAnimeMode::Full;

	// ---- read by the HUD / camera
	/** HUD-facing: a centre-card sequence is on screen. False while the Awaken clip plays as edge comic panels (bPanelMode),
	 *  so the HUD draws no card / letterbox over the Zoogi; gameplay waits on IsBusy() (bActive) either way. */
	bool bPlaying = false;
	bool bActive = false;
	bool bPanelMode = false;
	FString CurrentId;
	FString CurrentTier;
	FString Caption;        // big line ("CRITICAL!", "EMBER STONE INFUSED")
	FString SubCaption;     // debug only (log / -ZoogiAnimeDebug): "Anime_Hotstreak_Ember_Critical_01  (placeholder, Full 3.0 s)"; not drawn on the cards
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
	float Length = 0.f;
	bool bShort = false;
	bool bFrozen = false;
	FZoogiCombatEvent Current;
	UPROPERTY() TObjectPtr<UMediaTexture> MovieTexture;
	bool HasMovie() const { return bMovie; }
	/** b59: the clip has produced its first frame (the HUD keeps the streak card until then - fight slices are only ~1 s long). */
	bool MovieHasFrames() const;
	bool bSliceMovie = false; // b59: the current clip is a fight slice
	bool bCrashFrames = false;
	int32 LastKOClipTurn = -100; // b119: the last turn a knockout played its anime clip (every other turn)
	int32 CrashState = 0; float CrashAge = 0.f, CrashLen = 2.3f, CrashWait = 0.f; double CrashT0 = 0.0; FLinearColor CrashTint = FLinearColor::White; FString CrashId;
	/** Main menu WATCH ANIME (Oct 7, Evan): play the next / previous Movies.csv clip (cut-ins and episodes, not the menu
	 *  backdrop) into MovieTexture. Menu only: no freeze, no gameplay sequence. Returns the clip id, empty when none plays. */
	FString PlayMenuMovie(int32 Step = 1);
	/** Open a specific Movies.csv id (base filename, no .mp4). Used by STORY EP tiles so WATCH ANIME plays that episode. */
	FString PlayMenuMovieById(const FString& Id);
	void StopMenuMovie();

	/** Resolve the sequence id for an event (fallback chain), for tests and the HUD. */
	FString ResolveId(const FZoogiCombatEvent& E, const FZoogiAnimeRow** OutRow) const;
	static int32 Priority(FName Event);

protected:
	void LoadRows();
	void Start(const FZoogiCombatEvent& E);
	void Finish(bool bSkipped);
	void Freeze();
	void Unfreeze();
	FZoogiAnimeRow DefaultRow(FName Event) const;
	bool TryStartMovie(const FString& Id);
	/** Cut-in clip for an event: the first existing Content/Anime/Movies/<id>.mp4 along the fallback chain
	 *  (exact id, Any_<Artifact>, <Zoogi>_Any, Any_Any). Empty when none (or -ZoogiNoAnimeMovies). */
	FString FindMovie(const FZoogiCombatEvent& E, const FString& ResolvedId) const;
	void LoadMovieTable();
	TMap<FString, float> MovieSeconds; // Content/Anime/Movies/Movies.csv
	void SyncPlaying() { bPlaying = bActive && !bPanelMode; }
	FZoogiAnimePanels Panels;
	int32 PanelShots = 0;
	bool bMoviesOff = false;
	bool bCutInShot = false;
public:
	FString CurrentMovie;
	/** Oct 30 (P5 brief step 2): this event plays the P5-style character cut-in (ZoogiP5::PlayCutIn) - the HUD card stays off. */
	bool bP5CutIn = false;
protected:

	TMap<FString, FZoogiAnimeRow> Rows;
	FString RowsSource;
	TMap<FString, int32> FullPlays;
	TMap<FName, int32> EventFullPlays; // build 23 #4: one full cut-in per event type per match
	TArray<FZoogiCombatEvent> Queue;

	struct FBodySnap
	{
		TWeakObjectPtr<class UPrimitiveComponent> Comp;
		FTransform Xf;
		FVector Lin = FVector::ZeroVector;
		FVector Ang = FVector::ZeroVector;
	};
	TArray<FBodySnap> Snaps;
	double RealStart = 0.0;
	bool bMovie = false;
	UPROPERTY() TObjectPtr<UMediaPlayer> MoviePlayer;
	float SliceWait = 0.f, PrevAge = 0.f; // b59 fight slices: the timeline waits for the first frame
	UPROPERTY() TObjectPtr<UFileMediaSource> MovieSource;
};
