#include "ZoogiCPUBrain.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ZoogiCPU
{
	struct FCache { bool bLoaded = false; FZoogiProgressionConfig Cfg; TArray<FZoogiCPUCombo> Combos; };
	static FCache& Cache()
	{
		static FCache C;
		if (!C.bLoaded)
		{
			C.bLoaded = true;
			C.Cfg = FZoogiProgressionConfig::BuiltIn();
			FString Log; C.Cfg.LoadFromDir(FPaths::ProjectContentDir() / TEXT("Data/Progression"), Log);
			FString Text, Err; TArray<TMap<FString, FString>> Rows;
			if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/Progression/CPUCombos.csv"))) && ZoogiProgress::ReadCsvRows(Text, Rows, Err))
				for (const TMap<FString, FString>& R : Rows) C.Combos.Add({ FName(*R.FindRef(TEXT("card_a"))), FName(*R.FindRef(TEXT("card_b"))), FCString::Atoi(*R.FindRef(TEXT("bonus"))) });
		}
		return C;
	}
	EZoogiCPUTier ActiveTier()
	{
		EZoogiCPUTier T = EZoogiCPUTier::Normal;
		FString S;
		if (GConfig && GConfig->GetString(TEXT("ZoogiRoll.CPU"), TEXT("Tier"), S, GGameIni)) ZoogiProgress::ParseTier(S, T);
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUTier="), S)) ZoogiProgress::ParseTier(S, T);
		return T;
	}
	const FZoogiTierRow& TierRow(EZoogiCPUTier T) { return Cache().Cfg.Tier(T); }
	const TArray<FZoogiCPUCombo>& Combos() { return Cache().Combos; }

	float Score(const FZoogiCPUShotOption& O, const FZoogiTierRow& T)
	{
		if (T.Lookahead <= 0) return O.Cost;
		// one-ply lookahead: penalize ending near our own edge and leaving ourselves open to the reply
		return O.Cost + 600.f * FMath::Clamp(O.SelfRisk, 0.f, 1.f) + 400.f * FMath::Clamp(O.Exposure, 0.f, 1.f);
	}

	int32 PickShot(const TArray<FZoogiCPUShotOption>& Opts, const FZoogiTierRow& T, FRandomStream& Rng)
	{
		if (Opts.Num() == 0) return INDEX_NONE;
		TArray<int32> Idx; for (int32 i = 0; i < Opts.Num(); ++i) Idx.Add(i);
		Idx.StableSort([&](int32 A, int32 B) { return Score(Opts[A], T) < Score(Opts[B], T); });
		const float Best = Score(Opts[Idx[0]], T);
		int32 N = 1;
		while (N < FMath::Min(FMath::Max(1, T.Candidates), Idx.Num()) && Score(Opts[Idx[N]], T) <= Best + T.PickSpread) ++N;
		return N > 1 ? Idx[Rng.RandRange(0, N - 1)] : Idx[0];
	}

	TArray<int32> PickCards(const TArray<FZoogiCPUCardOption>& Hand, int32 Energy, const FZoogiTierRow& T, const TArray<FZoogiCPUCombo>& CombosIn, FRandomStream& Rng)
	{
		TArray<int32> Out;
		if (Hand.Num() == 0 || Energy <= 0) return Out;
		if (!T.bComboPlan && T.Candidates >= 5) // Easy: plays one affordable card on impulse
		{
			TArray<int32> Aff; for (int32 i = 0; i < Hand.Num(); ++i) if (Hand[i].Cost <= Energy) Aff.Add(i);
			if (Aff.Num()) Out.Add(Aff[Rng.RandRange(0, Aff.Num() - 1)]);
			return Out;
		}
		if (!T.bComboPlan) // Normal: the legacy greedy order (weight*10 + cost), rules reject what doesn't fit
		{
			for (int32 i = 0; i < Hand.Num(); ++i) Out.Add(i);
			Out.StableSort([&](int32 A, int32 B) { const int32 WA = Hand[A].Weight * 10 + Hand[A].Cost, WB = Hand[B].Weight * 10 + Hand[B].Cost; return WA != WB ? WA > WB : A < B; });
			return Out;
		}
		// Hard / Expert: exhaustive best subset (hand <= 10): value = sum(weight*10) + pair bonuses, no duplicate card ids,
		// filler (weight 1) excluded when conserving (EnergyReserve > 0) unless it completes a combo.
		const int32 N = FMath::Min(Hand.Num(), 10);
		int32 BestMask = 0; int32 BestVal = 0; int32 BestCost = 0;
		for (int32 M = 1; M < (1 << N); ++M)
		{
			int32 Cost = 0, Val = 0; TSet<FName> Ids; bool bOk = true;
			for (int32 i = 0; i < N && bOk; ++i) if (M & (1 << i))
			{
				if (Ids.Contains(Hand[i].CardId)) { bOk = false; break; }
				Ids.Add(Hand[i].CardId); Cost += Hand[i].Cost; Val += Hand[i].Weight * 10;
			}
			if (!bOk || Cost > Energy) continue;
			int32 Bonus = 0;
			for (const FZoogiCPUCombo& C : CombosIn) if (Ids.Contains(C.A) && Ids.Contains(C.B)) Bonus += C.Bonus;
			Val += Bonus;
			if (T.EnergyReserve > 0)
				for (int32 i = 0; i < N; ++i) if ((M & (1 << i)) && Hand[i].Weight <= 1 && Bonus <= 0) Val -= 15 * T.EnergyReserve; // don't burn filler
			if (Val > BestVal || (Val == BestVal && Cost < BestCost)) { BestVal = Val; BestMask = M; BestCost = Cost; }
		}
		for (int32 i = 0; i < N; ++i) if (BestMask & (1 << i)) Out.Add(i);
		Out.StableSort([&](int32 A, int32 B) { return Hand[A].Weight != Hand[B].Weight ? Hand[A].Weight > Hand[B].Weight : A < B; });
		return Out;
	}
}
