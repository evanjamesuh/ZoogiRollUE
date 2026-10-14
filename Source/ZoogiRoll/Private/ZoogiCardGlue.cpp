// v3 cards: AZoogiGameMode glue for the card Standby (state lives in UZoogiCardSubsystem, the single owner).
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiCardSubsystem.h"

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
	if (AZoogiBall* B = GetActiveBall())
		if (const FZoogiCardPlayerState* S = C->Rules().Player(B->PlayerIndex))
			AddPopup(FString::Printf(TEXT("STANDBY  %d CARDS  %d ENERGY"), S->Hand.Num(), S->Energy), B->BallLocation() + FVector(0, 0, 150.f), FLinearColor(0.6f, 0.85f, 1.f));
}

void AZoogiGameMode::CardsToggle(int32 Slot)
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C || !CardsOn() || Stage != EZoogiStage::Awaken || bAwakenDone || !IsHumanAiming()) return;
	const EZoogiCardResult R = C->ToggleSlot(this, Slot);
	if (R != EZoogiCardResult::Ok && R != EZoogiCardResult::NotSelected)
		if (AZoogiBall* B = GetActiveBall()) AddPopup(FString(ZoogiCardResultName(R)).Replace(TEXT("_"), TEXT(" ")).ToUpper(), B->BallLocation() + FVector(0, 0, 200.f), FLinearColor(1.f, 0.45f, 0.35f));
}

void AZoogiGameMode::CardsClear()
{
	if (UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this)) if (CardsOn() && Stage == EZoogiStage::Awaken && !bAwakenDone) C->ClearSelection(this);
}

bool AZoogiGameMode::CardsCommit()
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C || !CardsOn() || Phase != EZoogiPhase::Aiming || Stage != EZoogiStage::Awaken || bAwakenDone) return false;
	FString Choice;
	if (C->Commit(this, Choice) != EZoogiCardResult::Ok) return false;
	bAwakenDone = true;
	AwakenDoneAt = StageTime;
	AwakenChoiceText = Choice;
	if (AZoogiBall* B = GetActiveBall())
	{
		AddPopup(Choice, B->BallLocation() + FVector(0, 0, 160.f), FLinearColor(0.6f, 1.f, 0.7f));
		AddFeed(FString::Printf(TEXT("P%d %s  %s"), B->PlayerIndex + 1, *B->Def.Name, *Choice), FLinearColor(0.6f, 1.f, 0.7f));
	}
	return true;
}

void AZoogiGameMode::CardsPlanCPU()
{
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(this);
	if (!C || !CardsOn() || bAwakenDone) return;
	C->PlanCPU(this); // Select x N + Commit through the same rules as a human
	const AZoogiBall* B = GetActiveBall();
	const FZoogiCardPlayerState* S = B ? C->Rules().Player(B->PlayerIndex) : nullptr;
	if (S && S->bCommitted)
	{
		bAwakenDone = true;
		AwakenDoneAt = StageTime;
		UE_LOG(LogZoogi, Log, TEXT("CPU P%d Standby decision: %s"), B->PlayerIndex + 1, *AwakenChoiceText);
	}
}
