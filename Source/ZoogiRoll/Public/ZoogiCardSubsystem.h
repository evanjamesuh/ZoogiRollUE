// v3.0.0 Phase 2: the single authoritative owner of Card Energy, decks, hands, selections and commits during a match.
// Off by default ([ZoogiRoll.Cards] bCards=False): with it off nothing here runs and the Awaken artifact hand is unchanged.
// With -ZoogiCards the Awaken stage becomes STANDBY: draw to 5, Card Energy refreshed to 5, select cards (mouse / touch
// buttons or keys 1-5), COMMIT (Enter), then the existing Aim > Force (CONTROL / DRIVE / LAUNCH) > Confirm > Roll > Resolve.
// Human, CPU and tests all go through the same Select / Commit calls (validate before spend, spend once).
// Card effects arrive in Phase 3/4; Phase 2 only carries Crystal Collapse (Evan, Oct 13 7:58 PM): a committed Crystal
// Collapse + a real qualifying contact of the acting Zoogi with a breakable arena piece advances that piece by one state
// at the next Resolve boundary, through AZoogiArenaPiece::ApplyCardCollapse - the only exemption from the global
// [ZoogiRoll.Arena] bDestruction=False. Indestructible pieces (pillars) never change; rocks are not affected.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.generated.h"

class AZoogiGameMode;
class AZoogiBall;
class AZoogiArenaPiece;

UCLASS()
class ZOOGIROLL_API UZoogiCardSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiCardSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return Tune.bCards; } // inert (no tick) with cards off

	static UZoogiCardSubsystem* Get(const UObject* WorldContext);
	bool IsOn() const { return Tune.bCards; }

	// ---- GameMode hooks (Standby)
	void BeginStandby(AZoogiGameMode* G);
	EZoogiCardResult ToggleSlot(AZoogiGameMode* G, int32 HandSlot);
	void ClearSelection(AZoogiGameMode* G);
	EZoogiCardResult Commit(AZoogiGameMode* G, FString& OutChoiceText);
	void PlanCPU(AZoogiGameMode* G);
	FZoogiCardContext BuildContext(AZoogiGameMode* G) const;

	// ---- contact hook (AZoogiArenaPiece::OnBallHit, qualifying contact only)
	void OnPieceContact(AZoogiArenaPiece* Piece, AZoogiBall* Ball, const FVector& At, const FVector& Dir);
	static bool IsCollapseEligible(const AZoogiArenaPiece* Piece);

	const FZoogiCardRules& Rules() const { return RulesV; }
	const FZoogiCardDef* Def(FName Id) const { return RulesV.Backend() ? RulesV.Backend()->FindDef(Id) : nullptr; }
	int32 ActivePlayer(AZoogiGameMode* G) const;
	FString DataSource;
	/** Backend in use this match (rldb_toolkit or native). */
	FName BackendName() const { return RulesV.Backend() ? RulesV.Backend()->BackendName() : NAME_None; }

private:
	void EnsureMatch(AZoogiGameMode* G);
	void LoadData(TArray<FZoogiCardDef>& Defs, TArray<FName>& Deck);
	void ObservePhase(AZoogiGameMode* G);
	void ApplyPendingCollapses(AZoogiGameMode* G);
	void ExpireCollapse(const TCHAR* Why);
	void TestOnStandby(AZoogiGameMode* G, int32 P);
	void TestOnCommit(AZoogiGameMode* G, int32 P, int32 EnergyBefore, int32 SpentBefore, int32 Cost);
	void TestCrystal(AZoogiGameMode* G);
	void TestCheck(bool bOk, const FString& What);
	void TestFinish(AZoogiGameMode* G);
	void TickForecast(AZoogiGameMode* G, float Dt);
	UPROPERTY() TObjectPtr<class UNiagaraComponent> Trail;
	UPROPERTY() TObjectPtr<class UNiagaraComponent> AimPoint;
	float ForecastS = 0.f;
	bool bNoForecast = false;

	FZoogiCardTune Tune;
	FZoogiCardRules RulesV;
	TArray<TWeakObjectPtr<AZoogiBall>> MatchBalls;
	int32 LastStandbyTurn = -1;

	// Crystal Collapse: the committed effect of the current lead action, then the contact it earned
	int64 CollapseEffectId = 0;
	int32 CollapseOwner = -1;
	TWeakObjectPtr<AZoogiArenaPiece> CollapsePiece;
	FVector CollapseAt = FVector::ZeroVector, CollapseDir = FVector::ZeroVector;

	// phase observation (CARDS phase lines) + runtime tests (-ZoogiCardTest=standby_flow|crystal_collapse)
	FString LastPhase;
	TArray<FString> TurnPhases;
	FString TestCase;
	int32 TestChecks = 0, TestFails = 0, TestStandbys = 0, TestResolved = 0;
	bool bTestDone = false, bCrystalTested = false;
	FString TestFirstFail;
};
