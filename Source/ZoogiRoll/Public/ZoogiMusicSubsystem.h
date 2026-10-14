#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiTypes.h"
#include "ZoogiMatchEvents.h"
#include "ZoogiMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class UQuartzClockHandle;
class AZoogiGameMode;

/** One arena theme: three sample-aligned looping stems (Base / Battle / Shift) + ShiftHit / Victory / Defeat stingers. */
struct FZoogiArenaMusicDef
{
	EZoogiArena Arena;      // EZoogiArena::MAX = not mapped to an arena yet
	const TCHAR* Slug;      // assets: /Game/Audio/Music/<Slug>/S_Music_<Slug>_<Part>
	float BPM;
	int32 BeatsPerBar;
	int32 LoopBars;
};

namespace ZoogiMusic
{
	ZOOGIROLL_API const FZoogiArenaMusicDef* DefFor(EZoogiArena A);
	ZOOGIROLL_API const FZoogiArenaMusicDef* DefForSlug(const FString& Slug);
	ZOOGIROLL_API USoundBase* LoadMusic(const FString& Slug, const TCHAR* Part);
	/** End-of-match result for the local players: 1 victory, 0 defeat, 2 draw. */
	ZOOGIROLL_API int32 EndResult(const AZoogiGameMode* G, int32 Who);
}

/**
 * Adaptive arena music (INTEGRATION.md). Three looping 2D stems started together on a Quartz bar boundary; layer gains
 * change on bar boundaries (fade in 0.04 s on the downbeat, fade out over one bar); the ShiftHit plays on the next beat.
 * Also polls the match state and broadcasts the derived events (FormingBegin, RollOut, StageChanged, FinalRound,
 * MatchPoint, GaugeFull) for the announcer. Falls back to the old MatchMusic when an arena has no stems.
 */
UCLASS()
class ZOOGIROLL_API UZoogiMusicSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UZoogiMusicSubsystem* Get(const UObject* Ctx);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	void EnsureBound();
	bool IsActive() const { return bActive; }
	bool StartArena(EZoogiArena Theme);
	void Stop(float Fade);
	void EndMatch(int32 Result);
	/** Dips the music to Level (linear) for Seconds (announcer: 0.5 = -6 dB; big hits 0.6). */
	void DuckFor(float Seconds, float Level);
	void ClimaxDrop();
	void ClimaxLand();
	void PlayShiftHit(bool bNextBeat);

private:
	void OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag);
	void OnBar();
	void Recompute(FString& Why);
	void PollMatch();
	void Derive(EZoogiMatchEvent E, int32 Who = -1, float Mag = 0.f);
	float MasterGain() const;
	AZoogiGameMode* GM() const;
	bool IsShifted() const;
	bool IsFinalMoments() const;

	UPROPERTY() TObjectPtr<UAudioComponent> Stem[3];
	UPROPERTY() TObjectPtr<USoundBase> Wave[3];
	UPROPERTY() TObjectPtr<USoundBase> ShiftHitS;
	UPROPERTY() TObjectPtr<USoundBase> VictoryS;
	UPROPERTY() TObjectPtr<USoundBase> DefeatS;
	UPROPERTY() TObjectPtr<UQuartzClockHandle> Clock;

	const FZoogiArenaMusicDef* Def = nullptr;
	bool bActive = false;
	bool bEnded = false;
	bool bBound = false;
	bool bDebug = false;
	bool bDisabled = false;     // -ZoogiNoArenaMusic: force the MatchMusic fallback
	TWeakObjectPtr<AZoogiGameMode> BoundGM;
	FDelegateHandle EventHandle;

	float Target[3] = { 1.f, 0.f, 0.f };
	float Applied[3] = { 1.f, 0.f, 0.f };
	double StartTime = 0.0;
	double BarLen = 1.5;
	int32 Bar = 0;
	int32 SurgeBars = 0;
	int32 PreArmBars = 0;
	int32 DrawAwakenBars = 0;
	bool bSeenBattle = false;
	double PendingHitAt = -1.0;

	float DuckCur = 1.f, DuckTarget = 1.f;
	double DuckUntil = 0.0;
	float ClimaxMul = 1.f, ClimaxTarget = 1.f, ClimaxSecs = 0.25f;
	float LastMaster = -1.f;

	// match poller (derived events)
	bool bInMatch = false;
	bool bRollOutSent = false;
	bool bFinalSent = false;
	bool bMatchPointSent[2] = { false, false };
	float LastGauge[2] = { 0.f, 0.f };
	uint8 LastPhase = 0;
	uint8 LastStage = 255;
	TArray<TTuple<EZoogiMatchEvent, int32, float>> Derived;
};
