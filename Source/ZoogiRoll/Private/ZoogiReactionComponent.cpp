#include "ZoogiReactionComponent.h"
#include "ZoogiRoll.h"

UZoogiReactionComponent::UZoogiReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UZoogiReactionComponent::BeginAwakening(const FString& Why, int32 OwnTurns, int32 Turns, int32 Reactions)
{
	bAwakened = true;
	OwnTurnsAtActivation = OwnTurns;
	TurnsToLast = FMath::Max(1, Turns);
	ReactionsLeft = FMath::Max(0, Reactions);
	Source = Why;
	++Activations;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE awakening P%d on src=%s reactions=%d turns=%d"), Player + 1, *Why, ReactionsLeft, TurnsToLast);
}

void UZoogiReactionComponent::EndAwakening(const FString& Why)
{
	if (!bAwakened) return;
	bAwakened = false;
	ReactionsLeft = 0;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE awakening P%d off why=%s"), Player + 1, *Why);
}

void UZoogiReactionComponent::OnOwnTurn(int32 OwnTurns)
{
	if (bAwakened && OwnTurns - OwnTurnsAtActivation >= TurnsToLast) EndAwakening(TEXT("own_turn"));
}

void UZoogiReactionComponent::Consume(EZoogiReactionKind Kind)
{
	ReactionsLeft = FMath::Max(0, ReactionsLeft - 1);
	++ReactionsUsed;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE awakening P%d react=air used left=%d"), Player + 1, ReactionsLeft);
}
