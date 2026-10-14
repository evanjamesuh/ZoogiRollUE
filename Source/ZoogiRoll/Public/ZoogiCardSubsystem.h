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
#include "ZoogiCardEffects.h"
#include "ZoogiCardFantasy.h"
#include "ZoogiCardSubsystem.generated.h"

class AZoogiGameMode;
class AZoogiBall;
class AZoogiArenaPiece;
class AZoogiOrb;

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

	// ---- Phase 3 card effects in the rolling simulation (ZoogiCardFxWorld.cpp; all no-ops with cards off)
	void FxModifyShot(AZoogiGameMode* G, AZoogiBall* Ball, FVector2D& V);   // before Launch: Storm / Heavy Roll / Precision
	void FxAfterShot(AZoogiGameMode* G, AZoogiBall* Ball, const FVector2D& V); // after Launch: Titan mass, Frost lane, Orb Magnet target
	void FxContact(AZoogiBall* Self, AActor* Other, const FVector& NormalImpulse, const FHitResult& Hit); // AZoogiBall::OnSphereHit
	const FZoogiCardEffectCore& FxCore() const { return Fx; }
	static bool IsEnemy(const AZoogiGameMode* G, const AZoogiBall* A, const AZoogiBall* B);
	/** Oct 14: CPU aim error multiplier from the acting player's armed cards (Precision x0.7, Heavy Roll x1.2); 1 with cards off. */
	float AimErrorMul(int32 Player) const;
	/** Oct 14 playtest fix: the speed multiplier FxModifyShot will apply to this player's next shot (Storm A x Heavy Roll A; 1 with cards off). */
	float ShotMul(int32 Player) const;
	/** Oct 14: Crystal Collapse armed for this player this turn: nearest eligible piece within CPU range (null otherwise). */
	class AZoogiArenaPiece* CPUCollapseTarget(const AZoogiBall* Ball) const;
	static constexpr float CPUCollapseRange = 1600.f;
	/** Oct 14 fantasy layer: a human tap (floor point) for Precision redirects / the Recovery Roll dash. True = consumed. */
	bool FantasyTap(AZoogiGameMode* G, const FVector& World);
	/** CPU planning hint from the armed cards: 0 none, 1 wants a rival on its line, 2 Titan (stop on a rival: the meteor lands there). */
	int32 CPUCardIntent(int32 Player) const;
	/** Deck notice for the human (short saved deck refilled at match start), shown by the card tray for a few seconds. */
	FString DeckNotice;
	float DeckNoticeTime = -100.f;

	void DumpCoverage(const FString& Tag) const;
private:
	friend struct FZoogiSaveAccess; // v3 P6 save / resume
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

	// Phase 3 effects state
	void FxArm(AZoogiGameMode* G, int32 P, FName Card, int64 EffectId);
	void FxTick(AZoogiGameMode* G, float Dt);
	void FxLeadActionEnd(AZoogiGameMode* G);
	void FxTurnEnd(AZoogiGameMode* G);
	void FxReset();
	void FxUse(const FZoogiFxArmed& F, const FString& What);
	bool CardSetup(AZoogiGameMode* G, uint32 Flag, const FVector& Where, int32 Orb, int32 Life);
	int32 FxCardSetups = 0, FxSetupRefused = 0;
	FVector Prev(const AZoogiBall* B) const;
	FZoogiCardEffectCore Fx;
	// Oct 14 fantasy layer (ZoogiCardFantasy.cpp)
	FZoogiCardFantasyTune FanTune;
	FZoogiCardFantasyState Fan;
	bool FanOn() const;
	float FanAmp(const AZoogiBall* B, FName Card) const;
	void FanShot(AZoogiGameMode* G, FName Card, const TCHAR* Suffix = TEXT(""));
	void FanOnArm(AZoogiGameMode* G, int32 P, FName Card);
	void FanAnchorArmed(AZoogiGameMode* G, int32 P);
	void FanStandby(AZoogiGameMode* G, int32 P);
	void FanReset();
	void FanOnShot(AZoogiGameMode* G, AZoogiBall* Ball, const FVector2D& V);
	void FanBallContact(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* Att, const FHitResult& Hit);
	void FanGuardReflect(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* Att);
	void FanWall(AZoogiGameMode* G, AZoogiBall* Ball, const FHitResult& Hit);
	void FanSpikes(AZoogiGameMode* G, AZoogiBall* Owner, const FVector& At);
	void FanCloneHit(AZoogiGameMode* G, AZoogiBall* Victim);
	void FanDash(AZoogiGameMode* G, AZoogiBall* B);
	void FanTick(AZoogiGameMode* G, float Dt);
	void FanLeadActionEnd(AZoogiGameMode* G);
	// Oct 14: equipped power riders (the Zoogi's equipped power from the Powers screen upgrades its linked card)
	bool PowerRider(const AZoogiBall* B, FName Card) const;
	TSet<FIntPoint> EmberTrail;
	int32 EmberOwner = -1, MeteorOwner = -1;
	FVector MeteorAt = FVector::ZeroVector;
	float MeteorT = -1.f;
	bool bMeteorUsed = false;
	TArray<FVector> PrevVel;
	struct FBurst { TWeakObjectPtr<AZoogiBall> Victim; FVector Dv = FVector::ZeroVector; float At = 0.f; int64 Effect = 0; uint64 Contact = 0; int32 Owner = -1; };
	TArray<FBurst> PendingBursts;
	TArray<int32> OrbLife;
	TArray<bool> OrbWasOut;
	TWeakObjectPtr<AZoogiOrb> MagnetOrb, RelayOrb;
	FVector MagnetStart = FVector::ZeroVector, MagnetAdded = FVector::ZeroVector;
	float MagnetT = 0.f, MagnetPullDisp = 0.f;
	int64 MagnetEffect = 0;
	int32 MagnetOwner = -1, RelayLife = 0, FxShooter = -1, FxRingMarksAtShot = -1;
	// effect test telemetry (-ZoogiCardTest=effects / mutual_ko)
	TMap<FName, int32> FxUses;
	// v3 P6 coverage: selected (accepted selections), condition (effect ids whose trigger condition was met), triggered = FxUses
	TMap<FName, int32> CovSelected;
	TMap<FName, TSet<int64>> CovCond;
	void FxCond(FName Card, int32 Owner);
	void FxCondIf(const struct FZoogiFxArmed* F);
	/** One CARD_COV line per card: selected / committed / condition / triggered (event-supported). */
	float FxShotIn = 0.f, FxShotOut = 0.f, FxShotMul = 1.f, FxTitanPrev = 0.f, FxTitanSet = 0.f, FxMagnetMoved = 0.f, FxMagnetMax = 0.f;
	int32 FxMaxBursts = 0, FxTitanRestores = 0, FxFrostTicks = 0, FxForceIdx = 0;
	bool FxTitanRestoreOk = true, FxNoHealOk = true, bMutualTested = false;
	void TestEffectsOnShot(AZoogiGameMode* G, AZoogiBall* Ball);
	void TestMutualKO(AZoogiGameMode* G);
	void TestFollowUpTick(AZoogiGameMode* G);
	void TestHandUiTick(AZoogiGameMode* G);
	TWeakObjectPtr<class AZoogiBall> HandUiBall;
	int32 HandUiStep = 0, HandUiWait = 0, HandUiCost = 0, HandUiEnergy = 0, HandUiShowWait = 0;
	bool bFuPrevOffered = false, bFuPrevIn = false;
	float FuPrevGauge[2] = { 0.f, 0.f };
	int32 FuPrevTeam = -1, FuThisTeamTurn = 0, FuOffers = 0, FuAccepts = 0, FuDeclines = 0;
	void TestStageContact(AZoogiGameMode* G, AZoogiBall* Ball, const FVector2D& V);
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
