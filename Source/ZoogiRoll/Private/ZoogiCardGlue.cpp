// v3 cards: AZoogiGameMode glue for the card Standby (state lives in UZoogiCardSubsystem, the single owner).
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiFX.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

bool AZoogiGameMode::CardsOn() const
{
	const UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	return bArtifactMode && C && C->IsOn();
}

void AZoogiGameMode::CardsBeginStandby()
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C) return;
	C->BeginStandby(this);
	AwakenChoiceText.Reset();
	bStandbyAwaken = false;
	bCardAwakenView = false; CardAwakenPick = -1; // Choose-your-Awaken: a fresh pick every turn
	if (AZoogiBall* B = GetActiveBall())
		if (const FZoogiCardPlayerState* S = C->Rules().Player(B->PlayerIndex))
			AddPopup(FString::Printf(TEXT("STANDBY  %d CARDS  %d ENERGY"), S->Hand.Num(), S->Energy), B->BallLocation() + FVector(0, 0, 150.f), FLinearColor(0.6f, 0.85f, 1.f));
}

void AZoogiGameMode::CardsToggle(int32 Slot)
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C || !CardsWindowOpen()) return;
	const EZoogiCardResult R = C->ToggleSlot(this, Slot);
	if (R != EZoogiCardResult::Ok && R != EZoogiCardResult::NotSelected)
		if (AZoogiBall* B = GetActiveBall()) AddPopup(FString(ZoogiCardResultName(R)).Replace(TEXT("_"), TEXT(" ")).ToUpper(), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f));
}

void AZoogiGameMode::CardsToggleAwaken()
{
	// Episode Awakening (Air Reaction entitlement) stays independent of the cards: its own toggle, no Card Energy, no card.
	// Oct 15 playtest: the Awaken control (energy orb / keyboard A) toggles the tray between the cards and the 3 Awaken
	// tablets; picking a tablet turns Awakening ON for this turn, SKIP turns it OFF.
	CardsAwakenView(0);
}

void AZoogiGameMode::CardsClear()
{
	if (UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this)) if (CardsWindowOpen()) C->ClearSelection(this);
}

bool AZoogiGameMode::CardsCommit()
{
	bCardAwakenView = false; // committing always shows the cards again
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C || !CardsOn() || Phase != EZoogiPhase::Aiming || bAwakenDone) return false;
	// Standby, or (cards optional) any Battle step before the roll: aiming, or the launch itself (CardsCommitAtLaunch)
	if (!(Stage == EZoogiStage::Awaken || (CardsOptionalAim() && Stage == EZoogiStage::Battle && BattleStep != EZoogiBattleStep::Rolling))) return false;
	FString Choice;
	const AZoogiBall* Act = GetActiveBall();
	const FZoogiCardPlayerState* Pre = Act ? C->Rules().Player(Act->PlayerIndex) : nullptr;
	const int32 Before = Pre ? Pre->Committed.Num() : 0; // tap-played cards were already charged
	if (C->Commit(this, Choice) != EZoogiCardResult::Ok) return false;
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	AwakenChoiceText = Choice;
	if (AZoogiBall* B = GetActiveBall())
	{
		AddPopup(Choice, B->BallLocation() + FVector(0, 0, 160.f), FLinearColor(0.6f, 1.f, 0.7f));
		AddFeed(FString::Printf(TEXT("P%d %s  %s"), B->PlayerIndex + 1, *B->Def.Name, *Choice), FLinearColor(0.6f, 1.f, 0.7f));
		if (const FZoogiCardPlayerState* S = C->Rules().Player(B->PlayerIndex)) CardsAddAwakenCharge(B->PlayerIndex, S->Committed.Num() - Before, TEXT("human commit"));
	}
	return true;
}

// ---- cards optional before aiming (Evan, Oct 20)
bool AZoogiGameMode::CardsOptionalAim() const
{
	static const bool bOn = [] {
		bool b = true;
		GConfig->GetBool(TEXT("ZoogiRoll.Cards"), TEXT("bOptionalBeforeAim"), b, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiCardsBeforeAim"))) b = false;
		return b;
	}();
	return bOn && CardsOn();
}

bool AZoogiGameMode::CardsWindowOpen() const
{
	if (!CardsOn() || bAwakenDone || !IsHumanAiming()) return false;
	return Stage == EZoogiStage::Awaken || (CardsOptionalAim() && IsAimStep());
}

void AZoogiGameMode::CardsCommitAtLaunch()
{
	// the human launched without ACTIVATE: commit whatever is selected (nothing = pass, no energy spent) so the turn ends cleanly
	const AZoogiBall* B = GetActiveBall();
	if (!CardsOptionalAim() || bAwakenDone || !B || B->bCPU) return;
	if (const UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this))
		if (const FZoogiCardPlayerState* S = C->Rules().Player(B->PlayerIndex))
			if (S->Selected.Num() == 0) { bAwakenDone = true; AwakenDoneAt = StageTime; return; } // tap-to-play: nothing pending
	const bool bOk = CardsCommit();
	UE_LOG(LogZoogi, Log, TEXT("CARDS optional: P%d launched without ACTIVATE -> selection committed at launch (%s)"), B->PlayerIndex + 1, bOk ? *AwakenChoiceText : TEXT("rejected"));
}

bool AZoogiGameMode::CardsTapPlay(int32 Slot)
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	AZoogiBall* B = GetActiveBall();
	if (!C || !B || !CardsWindowOpen()) return false;
	C->ClearSelection(this);
	const EZoogiCardResult R = C->ToggleSlot(this, Slot);
	if (R != EZoogiCardResult::Ok)
	{
		AddPopup(FString(ZoogiCardResultName(R)).Replace(TEXT("_"), TEXT(" ")).ToUpper(), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f));
		return false;
	}
	FString Choice;
	if (C->Commit(this, Choice) != EZoogiCardResult::Ok) { C->ClearSelection(this); return false; }
	C->AllowAnotherCommit(this); // the hand stays playable this turn
	AwakenChoiceText = Choice;
	AddPopup(Choice.Replace(TEXT("CARDS "), TEXT("")).TrimStart(), B->BallLocation() + FVector(0, 0, 160.f), FLinearColor(0.6f, 1.f, 0.7f));
	AddFeed(FString::Printf(TEXT("P%d %s  %s"), B->PlayerIndex + 1, *B->Def.Name, *Choice), FLinearColor(0.6f, 1.f, 0.7f));
	CardsAddAwakenCharge(B->PlayerIndex, 1, TEXT("tap play"));
	UE_LOG(LogZoogi, Log, TEXT("CARDS tap-play P%d slot %d: %s"), B->PlayerIndex + 1, Slot, *Choice);
	return true;
}

// ---- Awaken charge (Evan, Oct 20)
bool AZoogiGameMode::AwakenChargeOn() const
{
	static const bool bOn = [] {
		bool b = true;
		GConfig->GetBool(TEXT("ZoogiRoll.Awakening"), TEXT("bChargeFromCards"), b, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAwakenTablets"))) b = false;
		return b;
	}();
	return bOn && CardsOn();
}

void AZoogiGameMode::CardsAddAwakenCharge(int32 Player, int32 Pips, const TCHAR* Why)
{
	if (!AwakenChargeOn() || Player < 0 || Player > 3 || Pips <= 0) return;
	const int32 Was = AwakenCharge[Player];
	AwakenCharge[Player] = FMath::Min(AwakenChargeMax, Was + Pips);
	UE_LOG(LogZoogi, Log, TEXT("AWAKEN charge P%d %d -> %d/%d (+%d, %s)"), Player + 1, Was, AwakenCharge[Player], AwakenChargeMax, Pips, Why);
	if (Was < AwakenChargeMax && AwakenCharge[Player] >= AwakenChargeMax)
		if (Balls.IsValidIndex(Player) && Balls[Player])
			AddPopup(TEXT("AWAKEN READY"), Balls[Player]->BallLocation() + FVector(0, 0, 210.f), FLinearColor(1.f, 0.82f, 0.35f));
}

bool AZoogiGameMode::CardsAwakenReady(int32 Player) const
{
	return AwakenChargeOn() && Player >= 0 && Player < 4 && AwakenCharge[Player] >= AwakenChargeMax;
}

bool AZoogiGameMode::CardsUseAwaken(const TCHAR* Who)
{
	// Spends a full charge during the Standby: the Zoogi's own affinity element is infused for this turn's shot (same artifact
	// effects as the classic INFUSE) and the Standby Awakening request turns ON. One use per turn.
	AZoogiBall* B = GetActiveBall();
	if (!B || !B->Artifact || !AwakenChargeOn() || Phase != EZoogiPhase::Aiming || bStandbyAwaken) return false;
	if (!(Stage == EZoogiStage::Awaken || IsAimStep())) return false;
	if (!CardsAwakenReady(B->PlayerIndex)) return false;
	const EZoogiArtifact A = ZoogiArtifacts::Affinity(B->Def.Id);
	AwakenCharge[B->PlayerIndex] = 0;
	bStandbyAwaken = true;
	B->Artifact->SetInfused(A);
	const FLinearColor Col = ZoogiArtifacts::Color(A);
	BigText = FString::Printf(TEXT("AWAKEN  %s"), *ZoogiArtifacts::Name(A).ToUpper()); BigColor = Col; BigAge = 0.f;
	AddPopup(BigText, B->BallLocation() + FVector(0, 0, 190.f), Col);
	AddFeed(FString::Printf(TEXT("P%d %s  awaken %s"), B->PlayerIndex + 1, *B->Def.Name, *ZoogiArtifacts::Name(A)), Col);
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), Col, 260.f, 0.8f, false, 14.f, B);
	UE_LOG(LogZoogi, Log, TEXT("AWAKEN used P%d (%s): %s infused, standby awakening ON, charge reset"), B->PlayerIndex + 1, Who, *ZoogiArtifacts::Name(A));
	return true;
}

void AZoogiGameMode::CardsPlanCPU()
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C || !CardsOn() || bAwakenDone) return;
	CardsPlanCPUAwaken(); // v3 P5: same Awakening rules as the classic CPU
	C->PlanCPU(this); // Select x N + Commit through the same rules as a human
	const AZoogiBall* B = GetActiveBall();
	const FZoogiCardPlayerState* S = B ? C->Rules().Player(B->PlayerIndex) : nullptr;
	if (S && S->bCommitted)
	{
		CardsAddAwakenCharge(B->PlayerIndex, S->Committed.Num(), TEXT("CPU commit"));
		ReplanCPUShot(); // Oct 14 playtest fix: re-aim with the committed cards (Storm / Heavy speed x, Crystal Collapse target)
		bAwakenDone = true;
		AwakenDoneAt = StageTime;
		UE_LOG(LogZoogi, Log, TEXT("CPU P%d Standby decision: %s"), B->PlayerIndex + 1, *AwakenChoiceText);
	}
}
