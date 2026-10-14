// v3.0.0 Phase 3: card effects in the real rolling simulation (cards on only; with bCards=False none of this runs).
// FZoogiCardEffectCore is the pure, engine-light bookkeeping every effect goes through, so automation tests exercise the
// exact code the match uses:
//   * stable contact ids: (effect, pair, contact episode) -> an effect applies at most once per contact;
//   * apply-once ledger keyed on EffectId (from FZoogiCardCommitted) + contact id;
//   * temporary properties (Titan mass) are cached from the ACTUAL value before the change and restored to it exactly;
//   * Double Impact: max 2 bursts per contact episode (the physical one + one 50% burst after 0.12 s);
//   * Frost: one shared set of lane cells (never stacks), Ember clears only overlapping cells;
//   * Orb scoring once per orb life (Orb Relay), Orb Magnet bounded (time, displacement, line of sight).
// All tuning below is PROPOSED (card book numbers where given: Cards.csv param_a / param_b; the rest marked PROPOSED).
#pragma once

#include "CoreMinimal.h"
#include "ZoogiCards.h"

namespace ZoogiCardFxTune
{
	constexpr float QualifyImpulse = 3000.f;     // PROPOSED: |NormalImpulse| for a "qualifying" Zoogi contact (spark threshold is 3500)
	constexpr float EpisodeGapS = 0.15f;         // PROPOSED: contacts of one pair closer than this are one contact episode
	constexpr float FrostCell = 120.f;           // PROPOSED: Frost lane cell size (cm)
	constexpr int32 FrostLaneCells = 6;          // PROPOSED: lane length in cells from the launch point
	constexpr float EmberClearR = 90.f;          // PROPOSED: Ember clears Frost cells within this radius of the Zoogi centre
	constexpr float RecoveryZoneR = 900.f;       // PROPOSED: Recovery Roll safe-zone radius around the Recovery Gate / arena centre
	constexpr float BankBonus = 1.15f;           // PROPOSED: first enemy push after a real BankSetup contact
	constexpr float MagnetAccel = 900.f;         // PROPOSED: Orb Magnet pull (cm/s^2)
	constexpr float MagnetRange = 1500.f;        // PROPOSED: Orb Magnet acquire range (cm)
	constexpr float GlancingDot = 0.7f;          // PROPOSED: |v^ . n| below this = glancing (Momentum Steal)
	constexpr float MaxShotMul = 1.35f;          // PROPOSED: Storm x Heavy Roll never exceed FullLaunchSpeed x this
}

struct FZoogiFxArmed
{
	FName Card;
	int64 EffectId = 0;
	int32 Owner = -1;
	int32 Turn = 0;
	float A = 0.f, B = 0.f;
	EZoogiCardExpiry Expiry = EZoogiCardExpiry::TurnEnd;
	bool bArmed = true;        // Anchor Guard starts unarmed (arms at settle)
	bool bConsumed = false;
	bool bBankSetup = false;   // Bank Strike: a real bank contact happened
	int32 Uses = 0;            // times it changed the simulation
};

struct ZOOGIROLL_API FZoogiCardEffectCore
{
	TArray<FZoogiFxArmed> Active;

	void Reset();
	FZoogiFxArmed& Arm(FName Card, int64 EffectId, int32 Owner, int32 Turn, float A, float B, EZoogiCardExpiry Expiry);
	FZoogiFxArmed* Find(FName Card, int32 Owner);
	const FZoogiFxArmed* Find(FName Card, int32 Owner) const;
	/** Remove effects of Owner (-1 = all) whose expiry matches; returns the removed list. */
	TArray<FZoogiFxArmed> Expire(EZoogiCardExpiry Kind, int32 Owner);
	void Remove(int64 EffectId);

	// ---- stable contact ids + apply once
	/** Episode counter per unordered actor pair: a new episode when the last contact is older than EpisodeGapS. */
	int32 ContactEpisode(int32 A, int32 B, float Now);
	static uint64 ContactId(int32 A, int32 B, int32 Episode);
	bool ApplyOnce(int64 EffectId, uint64 ContactId);
	bool WasApplied(int64 EffectId, uint64 ContactId) const;
	int32 AppliedCount() const { return Applied.Num(); }

	// ---- Double Impact bursts per contact episode (max 2 incl. the physical hit)
	int32 BurstsFor(uint64 ContactId) const { const int32* N = Bursts.Find(ContactId); return N ? *N : 0; }
	bool TryBurst(uint64 ContactId);

	// ---- temporary properties: cache the ACTUAL previous value once, restore exactly
	bool SaveMass(int32 Player, float Actual);
	bool RestoreMass(int32 Player, float& OutPrevious);
	bool HasSavedMass(int32 Player) const { return SavedMass.Contains(Player); }

	// ---- Frost lane (shared, never stacks) / Ember
	static FIntPoint Cell(const FVector& P);
	int32 AddFrostLane(const FVector& Start, const FVector& Dir, int32 N);
	int32 ClearFrostNear(const FVector& P, float R);
	bool InFrost(const FVector& P) const { return Frost.Contains(Cell(P)); }
	int32 FrostCells() const { return Frost.Num(); }

	// ---- orbs
	bool ScoreOrbOnce(int32 Orb, int32 Life);

	// ---- pure physics helpers (planar)
	/** Victim velocity after scaling the impulse it just received: Prev + (Post - Prev) * K (push stays physical in direction). */
	static FVector ScaleDelta(const FVector& Prev, const FVector& Post, float K);
	/** Glancing contact: move Frac of the shooter's tangential velocity to the victim. Returns false when not glancing. */
	static bool MomentumSteal(FVector& Shooter, FVector& Victim, const FVector& Normal, float Frac);
	/** Shot speed after Storm / Heavy Roll, capped. */
	static FVector2D ShotScale(const FVector2D& V, float Mul, float Cap);
	/** Orb Magnet step: velocity change toward Target this frame; zero once the orb is MaxMove from where the pull began
	 *  (the glue then removes the pull velocity it added, so the bound holds and no residual momentum is left). */
	static FVector MagnetStep(const FVector& OrbP, const FVector& Start, const FVector& Target, float MaxMove, float Accel, float Dt);

	uint32 Hash() const;
	/** v3 P6 save / resume (every field). */
	void Serialize(FArchive& Ar);

private:
	TSet<uint64> Applied;
	TMap<uint64, int32> Bursts;
	TMap<int32, float> SavedMass;
	TSet<FIntPoint> Frost;
	TSet<uint64> OrbScored;
	struct FPairT { int32 Episode = 0; float Last = -1e9f; };
	TMap<uint64, FPairT> Pairs;
};
