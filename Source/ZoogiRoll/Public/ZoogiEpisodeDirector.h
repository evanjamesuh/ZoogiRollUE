// v3 Phase 5: world side of the Battle Episode Engine extension (see ZoogiEpisodeCore.h).
// Active when cards are on or with -ZoogiEpisodeP5; otherwise it does nothing (cards-off parity).
// Fed by UZoogiBattleStateMachine right after each ledger append. Spends the existing Episode Energy (100) on an episode,
// appends one EPISODE ledger event, queues read-only playback (HUD). -ZoogiEpisodeSkip skips every playback.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiEpisodeCore.h"
#include "ZoogiEpisodeDirector.generated.h"

class UZoogiBattleStateMachine;
struct FZoogiEpisodeLedgerEvent;

UCLASS()
class ZOOGIROLL_API UZoogiEpisodeDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	static UZoogiEpisodeDirector* Get(const UObject* Ctx);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiEpisodeDirector, STATGROUP_Tickables); }
	virtual bool IsTickableWhenPaused() const override { return true; }

	void OnLedgerEvent(UZoogiBattleStateMachine* SM, const FZoogiEpisodeLedgerEvent& E);
	bool IsActive() const;
	const FZoogiEpPlayback* Current() const { return Queue.Num() ? &Queue[0] : nullptr; }
	void SkipCurrent();
	const FZoogiEpisodeCore& Core() const { return CoreV; }

	static FZoogiEpFact FactFromLedger(const FZoogiEpisodeLedgerEvent& E);

private:
	friend struct FZoogiSaveAccess;
	friend class UZoogiMatchValidator; // v3 P6 save / resume
	void EnsureReset(UZoogiBattleStateMachine* SM);
	void TrySpendOn(UZoogiBattleStateMachine* SM, int32 FactIndex);
	void TestOnFact(UZoogiBattleStateMachine* SM, int32 FactIndex, bool bBeforeSpend);
	void TestCheck(bool bOk, const FString& What);
	void TestFinish(UZoogiBattleStateMachine* SM);

	FZoogiEpisodeCore CoreV;
	TArray<FZoogiEpPlayback> Queue;
	FString MatchKey;
	int32 Played = 0, Skipped = 0;
	bool bSkipAll = false;
	// -ZoogiEpisodeP5Test=gauge
	FString TestCase;
	bool bTestDone = false, bFixtureDone = false;
	int32 TestChecks = 0, TestFails = 0, TestSpender = -1, TestEpisodes = 0;
	FString TestFirstFail;
	bool bAwakeBefore = false;
	int32 AwakeActBefore = 0, AwakeLeftBefore = 0;
	uint32 PlaybackLineHash = 0, CoreLineHash = 0;
};
