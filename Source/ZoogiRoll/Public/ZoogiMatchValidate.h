// v3 Phase 6: complete-match validation.
//   * UZoogiMatchSave (USaveGame) + FZoogiSaveAccess: save / resume of the required match state (game flow, every Zoogi
//     and orb body, arena pieces, card rules + effects, combo / episode gauges, Awakening, battle ledger + segments,
//     episode facts + playback). FullHash = CRC of the quantized state (times excluded).
//   * UZoogiMatchValidator: -ZoogiSaveTest=<boundary,commit,followup,episode>: save to a slot, perturb, load, the hash
//     must come back identical (and the re-captured bytes too); -ZoogiMatchTest=<tag>: CPU complete-match progression
//     (no stall, declared winner), counts, card coverage and one MATCH_TEST line with the replay hashes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiMatchValidate.generated.h"

class AZoogiGameMode;

UCLASS()
class ZOOGIROLL_API UZoogiMatchSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FString Version;
	UPROPERTY() FString Point;
	UPROPERTY() int32 Turn = 0;
	UPROPERTY() uint32 StateHash = 0;
	UPROPERTY() TArray<uint8> Blob;
};

struct ZOOGIROLL_API FZoogiSaveAccess
{
	static const TCHAR* Version() { return TEXT("zoogi-v3-p6-save-1"); }
	static bool Capture(AZoogiGameMode* G, TArray<uint8>& Out);
	static bool Restore(AZoogiGameMode* G, const TArray<uint8>& In, FString* OutErr = nullptr);
	/** CRC of the quantized required state (positions 0.1 cm, velocities 1 cm/s, gauges 0.1); timers excluded. */
	static uint32 FullHash(AZoogiGameMode* G);
	static bool SaveSlot(AZoogiGameMode* G, const FString& Slot, const FString& Point, TArray<uint8>* OutBlob = nullptr);
	static bool LoadSlot(AZoogiGameMode* G, const FString& Slot, FString* OutErr = nullptr);
	/** Test only: changes balls, gauges, RNG, ledgers so a load has something to undo. */
	static void Perturb(AZoogiGameMode* G);
private:
	static void Visit(FArchive& Ar, AZoogiGameMode* G, bool bHash);
};

UCLASS()
class ZOOGIROLL_API UZoogiMatchValidator : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiMatchValidator, STATGROUP_Tickables); }
	virtual bool IsTickableWhenPaused() const override { return true; }
private:
	void TrySavePoint(AZoogiGameMode* G, const FString& Point);
	void Finish(AZoogiGameMode* G, const FString& Why);
	void Check(bool bOk, const FString& What);
	FString MatchTag;
	TArray<FString> SavePoints;
	TSet<FString> SavesDone;
	int32 SavesOk = 0, SavesTried = 0;
	int32 Checks = 0, Fails = 0;
	FString FirstFail;
	bool bDone = false;
	int32 LastTurn = -1;
	float SinceTurn = 0.f, MaxTurnGap = 0.f, QuitIn = -1.f;
	int32 FollowOffers = 0;
	bool bWasOffered = false;
};
