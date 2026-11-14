// v3 cards: AZoogiGameMode glue for the card Standby (state lives in UZoogiCardSubsystem, the single owner).
#include "ZoogiGameMode.h"
#include "ZoogiWallCine.h" // b238
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
#include "ZoogiP5Command.h"
#include "ZoogiSkills.h"
#include "ZoogiActionFun.h"

bool AZoogiGameMode::CardsOn() const
{
	const UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	return bArtifactMode && C && C->IsOn();
}

// b80 (Evan: "Awaken power should last a lot longer"): turns of Awakening still to come per player (re-infused at each Standby)
namespace { TWeakObjectPtr<AZoogiGameMode> GAwakeG; TMap<int32, int32> GAwakeLeft; }
static int32 ZoogiAwakenExtraTurns() { static int32 N = -1; if (N < 0) { N = 3; FParse::Value(FCommandLine::Get(), TEXT("ZoogiAwakenTurns="), N); N = FMath::Clamp(N, 0, 20); } return N; }

void AZoogiGameMode::CardsBeginStandby()
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C) return;
	C->BeginStandby(this);
	AwakenChoiceText.Reset();
	bStandbyAwaken = false;
	if (GAwakeG.Get() != this) { GAwakeLeft.Reset(); GAwakeG = this; }
	if (AZoogiBall* Bw = GetActiveBall())
		if (int32* Left = GAwakeLeft.Find(Bw->PlayerIndex))
			if (*Left > 0 && Bw->Artifact)
			{	// b80: still awakened - the affinity stays infused for this turn's shot too
				--(*Left);
				bStandbyAwaken = true;
				const EZoogiArtifact Aw = ZoogiArtifacts::Affinity(Bw->Def.Id);
				Bw->Artifact->SetInfused(Aw);
				AddPopup(*Left > 0 ? FString::Printf(TEXT("AWAKENED  -  %d MORE TURNS"), *Left) : FString(TEXT("AWAKENED  -  LAST TURN")), Bw->BallLocation() + FVector(0, 0, 190.f), ZoogiArtifacts::Color(Aw));
				UE_LOG(LogZoogi, Log, TEXT("AWAKEN carries on for P%d (%d turns after this)"), Bw->PlayerIndex + 1, *Left);
				if (*Left <= 0) GAwakeLeft.Remove(Bw->PlayerIndex);
			}
	bCardAwakenView = false; CardAwakenPick = -1; // Choose-your-Awaken: a fresh pick every turn
	if (AZoogiBall* B = GetActiveBall())
		if (const FZoogiCardPlayerState* S = C->Rules().Player(B->PlayerIndex))
			if (!ZoogiP5Cmd::On()) // Oct 31: P5 command menu = no card popups
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

bool AZoogiGameMode::CardsPlayWindowOpen() const
{
	if (!CardsOn()) return false;
	const AZoogiBall* B = GetActiveBall();
	const UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* S = (B && C) ? C->Rules().Player(B->PlayerIndex) : nullptr;
	const bool bOpenNow = S && S->bStandbyOpen && S->Turn == TurnCounter;
	return ZoogiCards::CanPlayInPhase(B && !B->bCPU, bOpenNow, CardsWindowOpen(), Phase == EZoogiPhase::Rolling, SettleTimer > 0.f);
}

// b188 (Evan: "why is wood shield able to combo"): Wood Shield never shares a turn with another skill - no other skill once it
// is cast, and no shield once another skill was spent this turn
static bool ZoogiWoodConflict(const FZoogiCardPlayerState* S, FName Id)
{
	if (!S || !ZoogiSkills::IsSkill(Id)) return false;
	const FName Wood(TEXT("wood_shield"));
	bool bWood = false, bOther = false;
	for (const FZoogiCardCommitted& Cc : S->Committed) { if (Cc.Card.CardId == Wood) bWood = true; else if (ZoogiSkills::IsSkill(Cc.Card.CardId)) bOther = true; }
	return Id == Wood ? (bWood || bOther) : bWood;
}

bool AZoogiGameMode::CardsSlotPlayableNow(int32 Slot) const
{
	if (!CardsPlayWindowOpen()) return false;
	const AZoogiBall* B = GetActiveBall();
	const UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* S = (B && C) ? C->Rules().Player(B->PlayerIndex) : nullptr;
	if (!S || !S->Hand.IsValidIndex(Slot)) return Phase != EZoogiPhase::Rolling;
	const FName Id = S->Hand[Slot].CardId;
	if (Id == FName(TEXT("orb_magnet")) && !C->MagnetTargetNow(this, B->PlayerIndex)) return false; // Oct 22: no orb in range
	if (ZoogiWoodConflict(S, Id)) return false; // b188: Wood Shield is cast on its own
	return Phase != EZoogiPhase::Rolling || ZoogiCards::MidRollMode(Id) != ZoogiCards::EMidRoll::Blocked;
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
	if (!C || !B || !CardsPlayWindowOpen()) return false;
	if (!CardsSlotPlayableNow(Slot))
	{
		if (const FZoogiCardPlayerState* Pw = C->Rules().Player(B->PlayerIndex)) if (Pw->Hand.IsValidIndex(Slot) && ZoogiWoodConflict(Pw, Pw->Hand[Slot].CardId))
		{ AddPopup(TEXT("WOOD SHIELD IS CAST ON ITS OWN"), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f)); return false; } // b188
		AddPopup(Phase == EZoogiPhase::Rolling ? TEXT("CAN'T PLAY NOW") : TEXT("NO TARGET"), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f));
		return false; // never silently waste a card
	}
	// b63 (Evan): Fab skills are magic cast inside the ARC range - pick the target first, spend on FIRE
	FName SkillId;
	if (const FZoogiCardPlayerState* Ps = C->Rules().Player(B->PlayerIndex)) if (Ps->Hand.IsValidIndex(Slot)) SkillId = Ps->Hand[Slot].CardId;
	if (ZoogiSkills::IsSkill(SkillId))
	{
		const FZoogiCardDef* Sd = C->Def(SkillId);
		const FZoogiCardPlayerState* Ps = C->Rules().Player(B->PlayerIndex);
		if (Sd && Ps && Sd->Cost > Ps->Energy) { ZoogiSkills::Cancel(TEXT("energy")); AddPopup(TEXT("NOT ENOUGH SP"), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f)); return false; }
		if (ZoogiSkills::WantPick(this, Slot, SkillId)) return true; // the pick UI is open; nothing spent yet
	}
	C->AllowAnotherCommit(this); // an earlier ACTIVATE / launch commit does not lock the hand; once-per-card is a rule
	C->ClearSelection(this);
	const EZoogiCardResult R = C->ToggleSlot(this, Slot);
	if (R != EZoogiCardResult::Ok)
	{
		AddPopup(FString(ZoogiCardResultName(R)).Replace(TEXT("_"), TEXT(" ")).ToUpper(), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f));
		return false;
	}
	FString Choice;
	const int32 Refunds0 = C->MagnetRefunds;
	if (C->Commit(this, Choice) != EZoogiCardResult::Ok) { C->ClearSelection(this); ZoogiSkills::Cancel(TEXT("commit failed")); return false; }
	C->AllowAnotherCommit(this); // the hand stays playable this turn
	if (C->MagnetRefunds != Refunds0) // refunded (no orb in range): no play popup / Awaken charge
	{
		AddPopup(TEXT("NO ORB IN RANGE - REFUNDED"), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.8f, 0.4f));
		return false;
	}
	AwakenChoiceText = Choice;
	AddPopup(Choice.Replace(TEXT("CARDS "), TEXT("")).TrimStart(), B->BallLocation() + FVector(0, 0, 160.f), FLinearColor(0.6f, 1.f, 0.7f));
	AddFeed(FString::Printf(TEXT("P%d %s  %s"), B->PlayerIndex + 1, *B->Def.Name, *Choice), FLinearColor(0.6f, 1.f, 0.7f));
	if (SkillId != FName(TEXT("energy_buff"))) CardsAddAwakenCharge(B->PlayerIndex, 1, TEXT("tap play")); // b188: Energy Buff gives its own 3
	UE_LOG(LogZoogi, Log, TEXT("CARDS tap-play P%d slot %d: %s"), B->PlayerIndex + 1, Slot, *Choice);
	if (ZoogiSkills::IsSkill(SkillId)) ZoogiSkills::Execute(this, B, SkillId);
	return true;
}

extern bool GZoogiComboArming;
bool AZoogiGameMode::CardsArmCombo(int32 Slot, FName& OutId)
{	// b120g: like CardsTapPlay, but no target pick and no cast - the skill waits for a SUPERPOWER tap after the launch
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	AZoogiBall* B = GetActiveBall();
	if (!C || !B || !CardsPlayWindowOpen() || !CardsSlotPlayableNow(Slot)) return false;
	const FZoogiCardPlayerState* Ps = C->Rules().Player(B->PlayerIndex);
	if (!Ps || !Ps->Hand.IsValidIndex(Slot)) return false;
	OutId = Ps->Hand[Slot].CardId;
	const FZoogiCardDef* Sd = C->Def(OutId);
	if (Sd && Sd->Cost > Ps->Energy) return false;
	C->AllowAnotherCommit(this);
	C->ClearSelection(this);
	if (C->ToggleSlot(this, Slot) != EZoogiCardResult::Ok) return false;
	FString Choice;
	GZoogiComboArming = true;
	const EZoogiCardResult R = C->Commit(this, Choice);
	GZoogiComboArming = false;
	if (R != EZoogiCardResult::Ok) { C->ClearSelection(this); return false; }
	C->AllowAnotherCommit(this);
	CardsAddAwakenCharge(B->PlayerIndex, 1, TEXT("combo arm"));
	AddFeed(FString::Printf(TEXT("P%d %s  COMBO ARMED %s"), B->PlayerIndex + 1, *B->Def.Name, *Choice.Replace(TEXT("CARDS "), TEXT("")).TrimStart()), FLinearColor(1.f, 0.82f, 0.15f));
	UE_LOG(LogZoogi, Log, TEXT("CARDS combo arm P%d slot %d: %s"), B->PlayerIndex + 1, Slot, *Choice);
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
	if (ZoogiAwakenMeter()) if (UZoogiFunSys* Fm = UZoogiFunSys::Get(this)) Fm->AddAwakenMeter(this, Player, 16.f, Why); // b135: each card cast feeds the AWAKEN meter (+8 after the halving)
	// b89 (Evan: "fix CPU instantly getting full crystals"): a CPU committing 4 cards at once filled all 4; now one crystal per
	// player per turn at most (-ZoogiPipPerCard = one per card again)
	static const bool bPerCard = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPipPerCard"));
	static int32 PipTurn[4] = { -1, -1, -1, -1 };
	if (!bPerCard)
	{
		if (PipTurn[Player] == TurnCounter && AwakenCharge[Player] > 0) { UE_LOG(LogZoogi, Log, TEXT("AWAKEN charge P%d: already +1 this turn (%s ignored)"), Player + 1, Why); return; }
		PipTurn[Player] = TurnCounter;
		Pips = 1;
	}
	const int32 Was = AwakenCharge[Player];
	AwakenCharge[Player] = FMath::Max(Was, FMath::Min(AwakenChargeMax, Was + Pips)); // b141: never pulls an Energy Buff stack back to the cap
	UE_LOG(LogZoogi, Log, TEXT("AWAKEN charge P%d %d -> %d/%d (+%d, %s)"), Player + 1, Was, AwakenCharge[Player], AwakenChargeMax, Pips, Why);
	if (Was < AwakenChargeMax && AwakenCharge[Player] >= AwakenChargeMax)
		if (Balls.IsValidIndex(Player) && Balls[Player] && !ZoogiAwakenMeter()) // b135: crystals are REDIRECT fuel now
			AddPopup(TEXT("AWAKEN READY"), Balls[Player]->BallLocation() + FVector(0, 0, 210.f), FLinearColor(1.f, 0.82f, 0.35f));
}

bool AZoogiGameMode::CardsAwakenReady(int32 Player) const
{
	if (Player < 0 || Player >= 4) return false;
	if (ZoogiAwakenMeter()) if (const UZoogiFunSys* Fm = UZoogiFunSys::Get(this)) if (Fm->IsActive(this)) return Fm->AwakenFull(Player); // b135: the AWAKEN meter
	return AwakenChargeOn() && AwakenCharge[Player] >= AwakenChargeMax;
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
	{	// b135: SHOWTIME spends the AWAKEN meter (crystals stay for REDIRECT)
		UZoogiFunSys* Fm = ZoogiAwakenMeter() ? UZoogiFunSys::Get(this) : nullptr;
		if (Fm && Fm->IsActive(this)) Fm->SpendAwaken(B->PlayerIndex); else AwakenCharge[B->PlayerIndex] = 0;
	}
	bStandbyAwaken = true;
	ZoogiWallCine::NoteAwaken(this, B->PlayerIndex); // b238: a knockout this turn = the AWAKEN KNOCKOUT moment
	if (GAwakeG.Get() != this) { GAwakeLeft.Reset(); GAwakeG = this; }
	if (ZoogiAwakenExtraTurns() > 0) GAwakeLeft.Add(B->PlayerIndex, ZoogiAwakenExtraTurns()); // b80: lasts this turn + the next N of this player's
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
