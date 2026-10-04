// Milestone "Awaken": anime sequence director.
// Gameplay emits combat events; this subsystem picks a sequence id from the anime matrix (fallback chain), decides Full / Short / skip,
// freezes the world for impact moments (snapshot + restore so physics continues exactly where it stopped) and plays a placeholder:
// a real-time HUD card + camera shot, or Content/Movies/<id>.mp4 when that file exists (alpha-free test clips for now).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiArtifacts.h"
#include "ZoogiAnimeSubsystem.generated.h"

class AZoogiBall;
class UMediaPlayer;
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
	bool IsBusy() const { return bPlaying; }
	/** Click / Space: skip (after the minimum 0.3 s). */
	void Skip();
	void ResetMatch();
	EZoogiAnimeMode Mode = EZoogiAnimeMode::Full;

	// ---- read by the HUD / camera
	bool bPlaying = false;
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
	bool bMoviesOff = false;
	bool bCutInShot = false;
public:
	FString CurrentMovie;
protected:

	TMap<FString, FZoogiAnimeRow> Rows;
	FString RowsSource;
	TMap<FString, int32> FullPlays;
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
	UPROPERTY() TObjectPtr<UFileMediaSource> MovieSource;
};
