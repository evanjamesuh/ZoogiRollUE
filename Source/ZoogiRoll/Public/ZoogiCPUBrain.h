// Four CPU difficulty tiers (Evan Q3, Oct 14): Easy / Normal / Hard / Expert differ ONLY in decision-making. The tier never
// touches stats, mass, force limits, Card Energy, hand size, deck, cards drawn or any resource; it only changes WHICH legal
// shot / cards the CPU chooses and how much aim noise it allows itself (the existing control-based error, scaled = "skill").
// Normal is the legacy CPU: AZoogiGameMode keeps its exact pre-tier code path (no extra RNG draws), so parity is untouched.
// Parameters: Content/Data/Progression/CPUTiers.csv (+ CPUCombos.csv). Pure C++, deterministic for a seeded stream.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "ZoogiProgression.h"

struct FZoogiCPUShotOption
{
	float Cost = 0.f;          // the legacy planner's cost (lower = better)
	float SelfRisk = 0.f;      // 0..1: how close the shooter ends to its own edge after the shot (lookahead input)
	float Exposure = 0.f;      // 0..1: how open the shooter is to a rival counter-shot next turn (lookahead input)
};

struct FZoogiCPUCardOption
{
	FName CardId;
	int32 Cost = 1;
	int32 Weight = 1;          // card data CPUWeight
};

struct ZOOGIROLL_API FZoogiCPUCombo { FName A, B; int32 Bonus = 0; };

namespace ZoogiCPU
{
	/** Active tier: [ZoogiRoll.CPU] Tier=Normal in DefaultGame.ini, overridden by -ZoogiCPUTier=Easy|Normal|Hard|Expert. */
	ZOOGIROLL_API EZoogiCPUTier ActiveTier();
	/** Tier parameters (CPUTiers.csv, cached; built-in fallback). */
	ZOOGIROLL_API const FZoogiTierRow& TierRow(EZoogiCPUTier T);
	ZOOGIROLL_API const TArray<FZoogiCPUCombo>& Combos();
	/** Lookahead score (= Cost when Lookahead == 0). */
	ZOOGIROLL_API float Score(const FZoogiCPUShotOption& O, const FZoogiTierRow& T);
	/** Index of the chosen option: ranks by Score, keeps the top Candidates, then picks uniformly among those within
	 *  PickSpread of the best. One RandRange draw when more than one remains. */
	ZOOGIROLL_API int32 PickShot(const TArray<FZoogiCPUShotOption>& Opts, const FZoogiTierRow& T, FRandomStream& Rng);
	/** Aim error multiplier on the legacy control-based error (skill only). */
	inline float AimNoise(const FZoogiTierRow& T) { return T.AimNoise; }
	/** Ordered card picks within Energy (all legal; the rules still validate each Select):
	 *  - no combo plan, reserve 0, candidates >= 5 (Easy): one random affordable card;
	 *  - combo plan (Hard/Expert): best-value set by weight + CPUCombos bonuses, skipping weight-1 filler when EnergyReserve > 0. */
	ZOOGIROLL_API TArray<int32> PickCards(const TArray<FZoogiCPUCardOption>& Hand, int32 Energy, const FZoogiTierRow& T, const TArray<FZoogiCPUCombo>& Combos, FRandomStream& Rng);
}
