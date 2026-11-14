// b124: see ZoogiCPUSmart.h
#include "ZoogiCPUSmart.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiCPUSmart
{
	bool IsEarthBlock(FName Id) { static const TSet<FName> L = { FName(TEXT("sapphire_wall")), FName(TEXT("long_rampart")), FName(TEXT("rock_rampart")), FName(TEXT("stone_barricade")), FName(TEXT("jade_wall")), FName(TEXT("obsidian_wall")), FName(TEXT("gold_wall")), FName(TEXT("rune_wall")), FName(TEXT("slab_row")), FName(TEXT("sandstone_ruins")) }; static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCpuNoEarthBlock")); return !bOff && L.Contains(Id); }
	bool On() { static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCPULegacy")); return !bOff; }

	float Aggression(const AZoogiGameMode* G, int32 Player)
	{
		if (!G) return 1.f;
		FRandomStream R(HashCombine(GetTypeHash(G->GameSeed), GetTypeHash(0xA66E + Player * 131)));
		return R.FRandRange(0.75f, 1.3f);
	}

	float EdgeDist(const AZoogiGameMode* G, const FVector2D& P, FVector2D* OutDir)
	{
		if (!G || !G->Arena) return 1e5f;
		float Best = 1e9f; FVector2D BD(1.f, 0.f);
		for (int32 k = 0; k < 16; ++k)
		{
			const float A = 2.f * PI * k / 16.f;
			const FVector2D D(FMath::Cos(A), FMath::Sin(A));
			const float E = G->Arena->DistanceToEdge(P, D);
			if (E >= 0.f && E < Best) { Best = E; BD = D; }
		}
		if (OutDir) *OutDir = BD;
		return Best < 1e8f ? Best : 1e5f;
	}

	static FVector2D Pos2(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return FVector2D(B->BallLocation());
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return FVector2D(O->OrbLocation());
		return A ? FVector2D(A->GetActorLocation()) : FVector2D::ZeroVector;
	}

	struct FScene
	{
		int32 RivalsIn = 0, OrbsIn = 0, Near = 0, BestBunch = 0, RivalsAlive = 0;
		float NearestRival = 1e9f, MyEdge = 1e5f, RivalEdgeMin = 1e5f;
	};
	static FScene Look(AZoogiGameMode* G, const AZoogiBall* Me)
	{
		FScene S;
		if (!G || !Me) return S;
		const FVector2D P(Me->BallLocation());
		const float R = FMath::Max(400.f, G->ArcRange());
		TArray<FVector2D> In;
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B == Me || B->bOut || G->IsAlly(Me, B)) continue;
			++S.RivalsAlive;
			const FVector2D Q(B->BallLocation()); const float D = FVector2D::Distance(P, Q);
			S.NearestRival = FMath::Min(S.NearestRival, D);
			if (D <= R) { ++S.RivalsIn; In.Add(Q); S.RivalEdgeMin = FMath::Min(S.RivalEdgeMin, G->Arena ? G->Arena->DistanceToEdge(Q, (Q - P).GetSafeNormal()) : 1e5f); }
			if (D <= 650.f) ++S.Near;
		}
		for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut) continue;
			const FVector2D Q(O->OrbLocation()); const float D = FVector2D::Distance(P, Q);
			if (D <= R) { ++S.OrbsIn; In.Add(Q); }
			if (D <= 650.f) ++S.Near;
		}
		for (const FVector2D& A : In) { int32 N = 0; for (const FVector2D& B : In) if (FVector2D::Distance(A, B) < 400.f) ++N; S.BestBunch = FMath::Max(S.BestBunch, N); }
		S.MyEdge = EdgeDist(G, P);
		return S;
	}

	float CardValue(AZoogiGameMode* G, const AZoogiBall* Me, FName Id, float Noise)
	{
		const FScene S = Look(G, Me);
		const float Ag = Me ? Aggression(G, Me->PlayerIndex) : 1.f, Df = 2.f - Ag;
		const FString K = Id.ToString();
		const float Exposed = FMath::Clamp(1.f - (S.MyEdge - 250.f) / 900.f, 0.f, 1.f); // 1 = right on the edge
		const float RivalClose = FMath::Clamp(1.f - (S.NearestRival - 400.f) / 1400.f, 0.f, 1.f);
		const float RivalEdgy = S.RivalsIn ? FMath::Clamp(1.f - (S.RivalEdgeMin - 200.f) / 1200.f, 0.f, 1.f) : 0.f;
		float V = 0.f;
		// offence
		if (K == TEXT("water_cannon")) V = Ag * (S.RivalsIn ? 55.f + 35.f * RivalEdgy : (S.OrbsIn ? 30.f : 0.f));
		else if (K == TEXT("magic_rays")) V = Ag * (S.RivalsIn + S.OrbsIn ? 18.f * FMath::Min(3, S.RivalsIn + S.OrbsIn) + 12.f * FMath::Min(1, S.RivalsIn) : 0.f);
		else if (K == TEXT("meteor_rain")) V = Ag * (S.Near >= 2 ? 30.f + 14.f * FMath::Min(4, S.Near) : 8.f); // b136: IMPULSE blasts from the CPU itself
		else if (K == TEXT("fire_meteors")) V = Ag * (S.BestBunch >= 2 ? 30.f + 12.f * FMath::Min(3, S.BestBunch) : (S.RivalsIn ? 30.f : 10.f));
		else if (K == TEXT("energy_shockwave")) V = S.OrbsIn >= 3 ? 42.f : (S.OrbsIn ? 18.f : 0.f);
		else if (K == TEXT("sword_slash")) V = Ag * (S.Near ? 30.f + 10.f * FMath::Min(3, S.Near) : 18.f);
		else if (K == TEXT("apple_bombs")) V = S.RivalsAlive ? 34.f + 18.f * RivalClose : 16.f; // b271 (review: strong but never picked) was 22
		// b271 (review): skills the CPU never or rarely picked
		else if (K == TEXT("crescent_slash")) V = Ag * (S.RivalsIn ? 50.f + 15.f * RivalEdgy : (S.OrbsIn ? 20.f : 0.f));
		else if (K == TEXT("glacier_lance")) V = Ag * (S.RivalsIn ? 45.f + 30.f * RivalEdgy : (S.OrbsIn ? 25.f : 0.f));
		else if (K == TEXT("cross_slash")) V = Df * (RivalClose > 0.4f ? 30.f + 40.f * RivalClose : 5.f);
		else if (K == TEXT("frost_spikes")) V = S.BestBunch >= 2 ? 34.f + 8.f * FMath::Min(3, S.BestBunch) : 14.f;
		else if (K == TEXT("ice_shards") || K == TEXT("pillar_field")) V = (S.OrbsIn + S.RivalsIn) ? 30.f + 7.f * FMath::Min(4, S.OrbsIn + S.RivalsIn) : 8.f;
		else if (K == TEXT("fault_line")) V = Ag * (S.RivalsIn ? 40.f + 25.f * RivalEdgy : (S.OrbsIn ? 26.f : 0.f));
		// b272 new skills
		else if (K == TEXT("grapple_hook")) V = Ag * (S.RivalsIn ? 34.f + 20.f * RivalEdgy : (S.OrbsIn ? 18.f : 0.f));
		else if (K == TEXT("edge_save")) V = Df * (10.f + 60.f * Exposed);
		else if (K == TEXT("bounce_pad")) V = (S.OrbsIn + S.RivalsIn) ? 26.f : 10.f;
		else if (K == TEXT("ricochet")) V = Ag * (S.MyEdge < 1200.f ? 30.f : 18.f);
		else if (K == TEXT("decoy")) V = Df * (RivalClose > 0.3f ? 25.f + 40.f * RivalClose : 6.f);
		else if (K == TEXT("tether")) V = S.RivalsAlive >= 2 ? Ag * (30.f + 20.f * RivalEdgy) : 0.f;
		else if (K == TEXT("tide_shift")) V = S.RivalsAlive ? 22.f + 20.f * RivalEdgy : 0.f;
		else if (K == TEXT("hex")) V = S.RivalsAlive ? 28.f : 0.f;
		else if (K == TEXT("star_magnet")) V = 20.f;
		else if (K == TEXT("golden_apple")) V = S.OrbsIn ? 38.f : 0.f;
		else if (K == TEXT("bolt_balls")) V = Ag * (S.Near ? 58.f : 34.f);
		else if (K == TEXT("wolf_pack")) V = S.RivalsAlive ? 52.f * Ag : 0.f;
		else if (K == TEXT("gattling_burst") || K == TEXT("gattling_beat")) V = Ag * (S.RivalsIn + S.OrbsIn ? 20.f + 8.f * FMath::Min(4, S.RivalsIn + S.OrbsIn) : 0.f); // b190
		else if (K == TEXT("gattling_turret")) V = S.RivalsAlive ? 26.f : 0.f; // b190
		else if (K == TEXT("energy_buff")) V = 30.f + 22.f * (S.NearestRival > 1100.f ? 1.f : 0.f);
		else if (IsEarthBlock(Id)) V = S.RivalsAlive ? Ag * (52.f + 22.f * RivalClose) + (S.OrbsIn ? 12.f : 0.f) : 0.f; // b244: knock orbs out AND wall a rival off
		// defence: a person protects themselves when they're exposed
		else if (K == TEXT("wood_shield")) V = (Exposed > 0.6f && RivalClose > 0.3f) ? Df * (25.f + 50.f * Exposed + 20.f * RivalClose) : 0.f; // b136 (Evan): only when it really needs it (the shield ends the turn)
		else if (K == TEXT("ice_wall")) V = Df * (10.f + 55.f * Exposed);
		else if (K == TEXT("guard") || K == TEXT("anchor_guard")) V = Df * (15.f + 45.f * Exposed + 15.f * RivalClose);
		else if (K == TEXT("recovery_roll")) V = 10.f + 40.f * Exposed;
		else V = -1.f; // not a skill this layer knows: the caller keeps the card's own weight
		return V < 0.f ? V : FMath::Max(0.f, V + Noise);
	}

	bool CastMidRoll(FName) { return true; } // b125 (Evan: the CPU did things a player can't): every skill after launch, like the player's COMBO
	bool IsAura(FName Id) { return Id == FName(TEXT("wood_shield")) || Id == FName(TEXT("bolt_balls")) || Id == FName(TEXT("energy_buff")); }

	bool WantCastNow(AZoogiGameMode* G, const AZoogiBall* Me, FName Id, bool& bLastChance)
	{
		bLastChance = false;
		if (!G || !Me) return false;
		const float Sp = Me->GetBallVelocity().Size2D();
		bLastChance = Sp < 260.f || G->PhaseTime > 7.f;
		if (bLastChance) return true;
		const FScene S = Look(G, Me);
		const FString K = Id.ToString();
		if (K == TEXT("sword_slash")) return (S.Near > 0 && S.NearestRival < 520.f) || S.Near >= 2;
		if (K == TEXT("golden_apple") || K == TEXT("energy_shockwave")) return S.OrbsIn > 0;
		if (K == TEXT("ice_wall")) return S.MyEdge < 900.f; // the Zoogi is heading for its edge: wall it off
		if (IsEarthBlock(Id)) return S.RivalsAlive > 0 && G->PhaseTime > 0.25f; // b244: straight away, while the roll still has its momentum
		if (K == TEXT("energy_buff") || K == TEXT("wolf_pack")) return true; // straight after launch
		if (K == TEXT("cross_slash")) return true; // b271: the counter goes up straight away
		if (K == TEXT("edge_save") || K == TEXT("decoy") || K == TEXT("hex") || K == TEXT("star_magnet") || K == TEXT("tide_shift") || K == TEXT("tether") || K == TEXT("ricochet") || K == TEXT("bounce_pad")) return true; // b272
		if (K == TEXT("grapple_hook")) return S.RivalsIn + S.OrbsIn > 0;
		if (K == TEXT("frost_spikes")) return S.BestBunch >= 2 || S.OrbsIn > 0; // b271: the trap goes where things gather
		if (K == TEXT("ice_shards") || K == TEXT("pillar_field")) return S.OrbsIn + S.RivalsIn > 0;
		if (K == TEXT("wood_shield")) return S.NearestRival < 1000.f || G->PhaseTime > 1.2f;
		if (K == TEXT("bolt_balls")) return S.Near > 0 || G->PhaseTime > 1.0f;
		if (K == TEXT("meteor_rain")) return S.Near >= 2 || G->PhaseTime > 2.5f; // b136: IMPULSE goes off when things are close
		if (K == TEXT("fire_meteors")) return S.BestBunch >= 2 || S.RivalsIn > 0;
		return S.RivalsIn > 0; // water cannon / rays / apple mines: a rival in reach
	}

	float TapRank(const AZoogiGameMode* G, const AZoogiBall* Me, AActor* T)
	{
		if (!G || !Me || !T) return 1e9f;
		const FVector2D P(Me->BallLocation()), Q = Pos2(T);
		float R = FVector2D::Distance(P, Q) * 0.5f;
		if (const AZoogiBall* B = Cast<AZoogiBall>(T))
		{
			if (G->IsAlly(Me, B)) return 1e9f; // never a teammate
			R -= 700.f;
			int32 Top = -1; for (const AZoogiBall* O : G->Balls) if (O && O != Me && !G->IsAlly(Me, O)) Top = FMath::Max(Top, O->Score);
			if (B->Score >= Top && B->Score > Me->Score) R -= 250.f; // go after the leader
		}
		if (G->Arena) R -= FMath::Clamp(1400.f - G->Arena->DistanceToEdge(Q, (Q - P).GetSafeNormal()), 0.f, 1400.f) * 0.6f; // close to falling off
		return R;
	}
}
