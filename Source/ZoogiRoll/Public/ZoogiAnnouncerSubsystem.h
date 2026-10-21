#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiMatchEvents.h"
#include "ZoogiAnnouncerSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class AZoogiGameMode;

struct FZoogiVoLineSet { TArray<USoundBase*> Variants; int32 Last = -1; double ReadyAt = 0.0; }; // variants are GC-rooted in Keep
struct FZoogiVoPending { FString Line; int32 Priority; float Cooldown; double At; bool bRetry; };

/**
 * Tournament announcer (Oct 7). Listens to AZoogiGameMode::OnMatchEvent and plays /Game/Audio/VO/S_VO_<Line>_<n>:
 * a random variant per event (never the same one twice in a row), with priority (a higher priority line interrupts a
 * lower one, equal/lower lines are dropped while one is playing) and per-line cooldowns so it never spams. The music
 * ducks -6 dB while a line plays. -ZoogiNoVO turns it off.
 */
UCLASS()
class ZOOGIROLL_API UZoogiAnnouncerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Plays a line now (subject to priority / cooldown). Returns true if it started. */
	bool Say(const FString& Line, int32 Priority, float Cooldown, float Delay = 0.f);

private:
	void EnsureBound();
	void OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag);
	FZoogiVoLineSet* Lines(const FString& Line);
	bool PlayNow(const FString& Line, int32 Priority, float Cooldown, bool bRetry = false);

	TMap<FString, FZoogiVoLineSet> Sets;
	UPROPERTY() TArray<TObjectPtr<USoundBase>> Keep; // GC roots for the loaded lines
	UPROPERTY() TObjectPtr<UAudioComponent> Current;
	FString CurrentLine;
	int32 CurrentPriority = -1;
	double BusyUntil = 0.0;
	double QuietUntil = 0.0;
	double LastBattleLine = -100.0;
	TArray<FZoogiVoPending> Pending;
	bool bBound = false;
	bool bDisabled = false;
	TWeakObjectPtr<AZoogiGameMode> BoundGM;
	FDelegateHandle EventHandle;
	FRandomStream Rng;
};
