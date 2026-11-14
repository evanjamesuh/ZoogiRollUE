// Battle Episode Engine (T2): Episode Energy, per Zoogi. Earned from real gameplay truth (hits, strong hits, cracks,
// orb outs, ring outs, power hits, feats), spent on combos. It NEVER grants Awakening (see UZoogiReactionComponent).
// Created at runtime on each AZoogiBall by UZoogiBattleStateMachine (no ZoogiBall edit). Gains: EpisodeEnergyGains.csv.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoogiEpisodeEnergyComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FZoogiEnergyChanged, int32 /*Player*/, float /*Delta*/, float /*Now*/);

UCLASS(ClassGroup = (Zoogi))
class ZOOGIROLL_API UZoogiEpisodeEnergyComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UZoogiEpisodeEnergyComponent();
	UPROPERTY(VisibleAnywhere) int32 Player = -1;
	UPROPERTY(VisibleAnywhere) float Value = 0.f;
	UPROPERTY(VisibleAnywhere) float Cap = 100.f;
	UPROPERTY(VisibleAnywhere) float TotalGained = 0.f;
	UPROPERTY(VisibleAnywhere) float TotalSpent = 0.f;
	FZoogiEnergyChanged OnChanged;

	/** Adds (capped). Returns the amount actually added. Logs BATTLE energy P<n> +x src=<Source> -> v/cap. */
	float Gain(float Amount, FName Source, int32 ActionId);
	bool CanAfford(float Cost) const { return Value + KINDA_SMALL_NUMBER >= Cost; }
	bool Spend(float Cost, FName Why);
	void Refund(float Amount, FName Why);
	/** Test / debug only. */
	void SetValue(float V, FName Why);
	bool IsFull() const { return Value >= Cap - KINDA_SMALL_NUMBER; }
};
