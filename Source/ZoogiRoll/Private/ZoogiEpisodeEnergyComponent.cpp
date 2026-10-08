#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiRoll.h"

UZoogiEpisodeEnergyComponent::UZoogiEpisodeEnergyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UZoogiEpisodeEnergyComponent::Gain(float Amount, FName Source, int32 ActionId)
{
	if (Amount <= 0.f) return 0.f;
	const float Before = Value;
	Value = FMath::Min(Cap, Value + Amount);
	const float Added = Value - Before;
	TotalGained += Added;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE energy P%d %+.0f src=%s action=%d -> %.0f/%.0f%s"), Player + 1, Added, *Source.ToString(), ActionId, Value, Cap,
		Added + KINDA_SMALL_NUMBER < Amount ? *FString::Printf(TEXT(" (capped, wanted %+.0f)"), Amount) : TEXT(""));
	if (Added != 0.f) OnChanged.Broadcast(Player, Added, Value);
	return Added;
}

bool UZoogiEpisodeEnergyComponent::Spend(float Cost, FName Why)
{
	if (Cost <= 0.f) return true;
	if (!CanAfford(Cost))
	{
		UE_LOG(LogZoogi, Log, TEXT("BATTLE energy P%d spend %.0f for %s REFUSED (have %.0f)"), Player + 1, Cost, *Why.ToString(), Value);
		return false;
	}
	Value = FMath::Max(0.f, Value - Cost);
	TotalSpent += Cost;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE energy P%d %+.0f src=spend:%s -> %.0f/%.0f"), Player + 1, -Cost, *Why.ToString(), Value, Cap);
	OnChanged.Broadcast(Player, -Cost, Value);
	return true;
}

void UZoogiEpisodeEnergyComponent::Refund(float Amount, FName Why)
{
	if (Amount <= 0.f) return;
	const float Before = Value;
	Value = FMath::Min(Cap, Value + Amount);
	TotalSpent -= Value - Before;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE energy P%d %+.0f src=refund:%s -> %.0f/%.0f"), Player + 1, Value - Before, *Why.ToString(), Value, Cap);
	OnChanged.Broadcast(Player, Value - Before, Value);
}

void UZoogiEpisodeEnergyComponent::SetValue(float V, FName Why)
{
	const float Before = Value;
	Value = FMath::Clamp(V, 0.f, Cap);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE energy P%d set %.0f src=%s -> %.0f/%.0f"), Player + 1, Value - Before, *Why.ToString(), Value, Cap);
	OnChanged.Broadcast(Player, Value - Before, Value);
}
