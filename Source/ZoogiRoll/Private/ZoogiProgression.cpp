#include "ZoogiProgression.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ZoogiProgressHooks
{
	FOnCardEffectUsed& CardEffectUsed() { static FOnCardEffectUsed D; return D; }
	FOnPowerUsed& PowerUsed() { static FOnPowerUsed D; return D; }
}

// ================================================================== helpers

namespace ZoogiProgress
{
	const TCHAR* TxnResultName(EZoogiTxnResult R)
	{
		switch (R) { case EZoogiTxnResult::Applied: return TEXT("applied"); case EZoogiTxnResult::Duplicate: return TEXT("duplicate");
		case EZoogiTxnResult::Invalid: return TEXT("invalid"); case EZoogiTxnResult::Insufficient: return TEXT("insufficient"); }
		return TEXT("?");
	}
	static const TCHAR* TierNames[] = { TEXT("Easy"), TEXT("Normal"), TEXT("Hard"), TEXT("Expert") };
	const TCHAR* TierName(EZoogiCPUTier T) { return TierNames[(int32)T]; }
	bool ParseTier(const FString& S, EZoogiCPUTier& Out)
	{
		for (int32 i = 0; i < 4; ++i) if (S.TrimStartAndEnd().Equals(TierNames[i], ESearchCase::IgnoreCase)) { Out = (EZoogiCPUTier)i; return true; }
		return false;
	}
	FGuid DeterministicId(const FString& Key)
	{
		FTCHARToUTF8 U(*Key);
		FMD5 Md5; Md5.Update((const uint8*)U.Get(), U.Length());
		uint8 D[16]; Md5.Final(D);
		auto W = [&](int32 i) { return (uint32)D[i] << 24 | (uint32)D[i + 1] << 16 | (uint32)D[i + 2] << 8 | (uint32)D[i + 3]; };
		return FGuid(W(0), W(4), W(8), W(12));
	}
	FDateTime ResetBoundary(const FDateTime& Local, int32 ResetHour)
	{
		FDateTime B = Local.GetDate() + FTimespan::FromHours(FMath::Clamp(ResetHour, 0, 23));
		if (Local < B) B -= FTimespan::FromDays(1);
		return B;
	}
	bool ReadCsvRows(const FString& Text, TArray<TMap<FString, FString>>& Out, FString& OutError)
	{
		Out.Reset();
		TArray<FString> Lines; Text.Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Lines, TEXT("\n"), true);
		Lines.RemoveAll([](const FString& L) { const FString T = L.TrimStartAndEnd(); return T.IsEmpty() || T.StartsWith(TEXT("#")); });
		if (Lines.Num() < 1) { OutError = TEXT("no header"); return false; }
		TArray<FString> H; Lines[0].ParseIntoArray(H, TEXT(","), false);
		for (FString& X : H) X = X.TrimStartAndEnd().ToLower();
		for (int32 L = 1; L < Lines.Num(); ++L)
		{
			TArray<FString> F; Lines[L].ParseIntoArray(F, TEXT(","), false);
			if (F.Num() != H.Num()) { OutError = FString::Printf(TEXT("line %d: %d fields, header has %d"), L + 1, F.Num(), H.Num()); Out.Reset(); return false; }
			TMap<FString, FString>& R = Out.AddDefaulted_GetRef();
			for (int32 i = 0; i < H.Num(); ++i) R.Add(H[i], F[i].TrimStartAndEnd());
		}
		return true;
	}
	static FString Iso(const FDateTime& T) { return T.ToIso8601(); }
	static FDateTime FromIso(const FString& S) { FDateTime T(0); if (!S.IsEmpty()) FDateTime::ParseIso8601(*S, T); return T; }
	static FString Gs(const FGuid& G) { return G.ToString(EGuidFormats::DigitsWithHyphens); }
	static FGuid Gp(const FString& S) { FGuid G; FGuid::Parse(S, G); return G; }
	static bool B(const FString& S) { return S.Equals(TEXT("true"), ESearchCase::IgnoreCase) || S == TEXT("1"); }
}
using namespace ZoogiProgress;

// ================================================================== wallet

void FZoogiWallet::Add(FZoogiGemTxn&& T)
{
	T.EarnedAfter = EarnedV; T.PurchasedAfter = PurchasedV;
	Ids.Add(T.TxnId);
	LedgerV.Add(MoveTemp(T));
}

EZoogiTxnResult FZoogiWallet::Grant(const FGuid& TxnId, EZoogiGemBucket Bucket, int32 Amount, FName Source, const FDateTime& Utc, const FString& ExternalId, int32* OutApplied)
{
	if (OutApplied) *OutApplied = 0;
	if (!TxnId.IsValid() || Amount <= 0) return EZoogiTxnResult::Invalid;
	if (Ids.Contains(TxnId)) return EZoogiTxnResult::Duplicate;
	FZoogiGemTxn T; T.TxnId = TxnId; T.Source = Source; T.Utc = Utc; T.ExternalId = ExternalId;
	if (Bucket == EZoogiGemBucket::Purchased)
	{
		T.Kind = TEXT("purchase"); T.PurchasedDelta = Amount; PurchasedV += Amount; LifetimePurchased += Amount;
	}
	else
	{
		const int32 Room = FMath::Max(0, EarnedCap - EarnedV);
		const int32 A = FMath::Min(Amount, Room);
		T.Kind = TEXT("grant"); T.EarnedDelta = A; EarnedV += A; LifetimeEarned += A;
	}
	if (OutApplied) *OutApplied = T.EarnedDelta + T.PurchasedDelta;
	Add(MoveTemp(T));
	return EZoogiTxnResult::Applied;
}

EZoogiTxnResult FZoogiWallet::Spend(const FGuid& TxnId, int32 Amount, FName Sink, const FDateTime& Utc, const FString& ExternalId)
{
	if (!TxnId.IsValid() || Amount <= 0) return EZoogiTxnResult::Invalid;
	if (Ids.Contains(TxnId)) return EZoogiTxnResult::Duplicate;
	if (Amount > Total()) return EZoogiTxnResult::Insufficient;
	int32 Left = Amount, FromE = 0, FromP = 0;
	auto TakeE = [&] { FromE = FMath::Min(Left, EarnedV); Left -= FromE; };
	auto TakeP = [&] { FromP = FMath::Min(Left, PurchasedV); Left -= FromP; };
	if (SpendOrder == EZoogiSpendOrder::EarnedFirst) { TakeE(); TakeP(); } else { TakeP(); TakeE(); }
	check(Left == 0);
	EarnedV -= FromE; PurchasedV -= FromP; LifetimeSpent += Amount;
	FZoogiGemTxn T; T.TxnId = TxnId; T.Kind = TEXT("spend"); T.Source = Sink; T.Utc = Utc; T.ExternalId = ExternalId; T.EarnedDelta = -FromE; T.PurchasedDelta = -FromP;
	Add(MoveTemp(T));
	return EZoogiTxnResult::Applied;
}

bool FZoogiWallet::Verify(FString& OutError) const
{
	int64 E = 0, P = 0; TSet<FGuid> Seen;
	for (const FZoogiGemTxn& T : LedgerV)
	{
		if (Seen.Contains(T.TxnId)) { OutError = TEXT("duplicate txn id ") + Gs(T.TxnId); return false; }
		Seen.Add(T.TxnId);
		E += T.EarnedDelta; P += T.PurchasedDelta;
		if (E < 0 || P < 0) { OutError = TEXT("negative balance at ") + Gs(T.TxnId); return false; }
		if (E != T.EarnedAfter || P != T.PurchasedAfter) { OutError = TEXT("after-balance mismatch at ") + Gs(T.TxnId); return false; }
	}
	if (E != EarnedV || P != PurchasedV) { OutError = TEXT("balance != ledger"); return false; }
	return true;
}

TSharedRef<FJsonObject> FZoogiWallet::ToJson() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("earned"), EarnedV); J->SetNumberField(TEXT("purchased"), PurchasedV);
	J->SetNumberField(TEXT("lifetime_earned"), (double)LifetimeEarned); J->SetNumberField(TEXT("lifetime_purchased"), (double)LifetimePurchased); J->SetNumberField(TEXT("lifetime_spent"), (double)LifetimeSpent);
	TArray<TSharedPtr<FJsonValue>> A;
	for (const FZoogiGemTxn& T : LedgerV)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("id"), Gs(T.TxnId)); X->SetStringField(TEXT("kind"), T.Kind); X->SetStringField(TEXT("source"), T.Source.ToString());
		X->SetNumberField(TEXT("de"), T.EarnedDelta); X->SetNumberField(TEXT("dp"), T.PurchasedDelta); X->SetNumberField(TEXT("e"), T.EarnedAfter); X->SetNumberField(TEXT("p"), T.PurchasedAfter);
		X->SetStringField(TEXT("utc"), Iso(T.Utc)); X->SetStringField(TEXT("ext"), T.ExternalId);
		A.Add(MakeShared<FJsonValueObject>(X));
	}
	J->SetArrayField(TEXT("ledger"), A);
	return J;
}

bool FZoogiWallet::FromJson(const TSharedPtr<FJsonObject>& J, FString& OutError)
{
	*this = FZoogiWallet();
	if (!J.IsValid()) { OutError = TEXT("no wallet"); return false; }
	EarnedV = (int32)J->GetNumberField(TEXT("earned")); PurchasedV = (int32)J->GetNumberField(TEXT("purchased"));
	LifetimeEarned = (int64)J->GetNumberField(TEXT("lifetime_earned")); LifetimePurchased = (int64)J->GetNumberField(TEXT("lifetime_purchased")); LifetimeSpent = (int64)J->GetNumberField(TEXT("lifetime_spent"));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("ledger"), A))
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> X = V->AsObject(); if (!X) continue;
			FZoogiGemTxn T; T.TxnId = Gp(X->GetStringField(TEXT("id"))); T.Kind = X->GetStringField(TEXT("kind")); T.Source = FName(*X->GetStringField(TEXT("source")));
			T.EarnedDelta = (int32)X->GetNumberField(TEXT("de")); T.PurchasedDelta = (int32)X->GetNumberField(TEXT("dp")); T.EarnedAfter = (int32)X->GetNumberField(TEXT("e")); T.PurchasedAfter = (int32)X->GetNumberField(TEXT("p"));
			T.Utc = FromIso(X->GetStringField(TEXT("utc"))); T.ExternalId = X->GetStringField(TEXT("ext"));
			Ids.Add(T.TxnId); LedgerV.Add(T);
		}
	return Verify(OutError); // a tampered / inconsistent ledger is rejected (caller keeps a backup)
}

// ================================================================== config

FZoogiProgressionConfig FZoogiProgressionConfig::BuiltIn()
{
	// Safe fallback only; the shipped values live in Content/Data/Progression/*.csv (the game logs which source it used).
	FZoogiProgressionConfig C;
	C.MasteryCumulativeXP = { 0, 150, 450, 800, 1200, 2000, 2900, 4000, 5400, 7000 };
	C.MasteryLevelGems = { 0, 10, 15, 20, 30, 40, 50, 60, 80, 150 };
	auto R = [&](const TCHAR* S, const TCHAR* T, int32 G, int32 X, int32 Cap, bool bF) { C.Rewards.Add({ FName(S), T ? FName(T) : NAME_None, G, X, Cap, bF }); };
	R(TEXT("completion"), nullptr, 4, 12, 1, true); R(TEXT("win"), nullptr, 5, 8, 1, true); R(TEXT("draw"), nullptr, 2, 4, 1, true);
	R(TEXT("ko"), nullptr, 1, 6, 4, false); R(TEXT("ring_out"), nullptr, 1, 3, 3, false);
	R(TEXT("trick"), TEXT("Trick.BankStrike"), 1, 5, 2, true); R(TEXT("trick"), TEXT("Trick.MomentumSteal"), 1, 5, 2, true); R(TEXT("trick"), TEXT("Trick.DoubleImpact"), 2, 6, 2, true);
	R(TEXT("trick"), TEXT("Trick.OrbMagnet"), 1, 3, 2, false); R(TEXT("trick"), TEXT("Trick.FrostLane"), 1, 5, 2, false); R(TEXT("trick"), TEXT("Trick.CrystalCollapse"), 1, 5, 2, false);
	R(TEXT("trick"), TEXT("Trick.Other"), 1, 3, 2, false); R(TEXT("power_used"), nullptr, 1, 4, 3, false); R(TEXT("orb_collected"), nullptr, 1, 1, 4, true); R(TEXT("episode"), nullptr, 2, 0, 2, true);
	C.Tiers = { { EZoogiCPUTier::Easy, 5, 900.f, 0, false, 0, 1.8f, 0.5f, 0.5f }, { EZoogiCPUTier::Normal, 3, 350.f, 0, false, 0, 1.f, 1.f, 1.f },
		{ EZoogiCPUTier::Hard, 2, 150.f, 1, true, 1, 0.7f, 1.3f, 1.3f }, { EZoogiCPUTier::Expert, 1, 0.f, 1, true, 2, 0.45f, 1.6f, 1.5f } };
	auto P = [&](const TCHAR* Id, const TCHAR* N, EZoogiElement E, const TCHAR* T, bool bF, int32 M, int32 G, const TCHAR* Card) { C.Powers.Add({ FName(Id), N, E, FName(T), bF, M, G, FName(Card) }); };
	P(TEXT("shock_1_storm"), TEXT("Storm"), EZoogiElement::Shock, TEXT("free"), true, 0, 0, TEXT("storm"));
	P(TEXT("frost_2_frost"), TEXT("Frost"), EZoogiElement::Frost, TEXT("free"), true, 0, 0, TEXT("frost"));
	P(TEXT("void_1_precision"), TEXT("Precision"), EZoogiElement::Void, TEXT("starter"), false, 2, 300, TEXT("precision"));
	P(TEXT("fire_2_ember"), TEXT("Ember"), EZoogiElement::Fire, TEXT("starter"), false, 3, 400, TEXT("ember"));
	P(TEXT("void_2_crystal_collapse"), TEXT("Crystal Collapse"), EZoogiElement::Void, TEXT("starter"), false, 4, 500, TEXT("crystal_collapse"));
	P(TEXT("void_2_titan_meteor"), TEXT("Titan Meteor"), EZoogiElement::Void, TEXT("starter"), false, 5, 550, TEXT("titan"));
	const TCHAR* TT[7][3] = { { TEXT("arena_rookie"), TEXT("Arena Rookie"), TEXT("A_FIRST_MATCH") }, { TEXT("rolling_challenger"), TEXT("Rolling Challenger"), TEXT("A_WINS_10") },
		{ TEXT("marble_master"), TEXT("Marble Master"), TEXT("A_MASTERY_5") }, { TEXT("elemental_specialist"), TEXT("Elemental Specialist"), TEXT("A_MASTERY_ALL5") },
		{ TEXT("arena_champion"), TEXT("Arena Champion"), TEXT("A_WINS_50") }, { TEXT("master_strategist"), TEXT("Master Strategist"), TEXT("A_EXPERT_WINS_10") }, { TEXT("zoogi_legend"), TEXT("Zoogi Legend"), TEXT("A_AFFINITY_MAX") } };
	for (auto& X : TT) C.Titles.Add({ FName(X[0]), X[1], FName(X[2]) });
	C.Achievements = { { "A_FIRST_MATCH", TEXT("First Roll"), "matches_completed", 1, 10, "arena_rookie", NAME_None } };
	C.Cosmetics = { { "border_default", TEXT("Plain Border"), "animated_border", TEXT("default"), true } };
	C.AffinityLevels = { { 1, 0, NAME_None, FString() }, { 2, 100, "emote", TEXT("emote_{zoogi}_wave") }, { 3, 300, "victory_pose", TEXT("pose_{zoogi}") },
		{ 4, 600, "profile_animation", TEXT("profile_{zoogi}") }, { 5, 1000, "intro", TEXT("intro_{zoogi}") } };
	C.AffinityXP.Add("participation", { 10, 1 }); C.AffinityXP.Add("win", { 5, 1 }); C.AffinityXP.Add("ko", { 3, 4 }); C.AffinityXP.Add("signature_hit", { 2, 3 });
	return C;
}

bool FZoogiProgressionConfig::LoadFromDir(const FString& Dir, FString& OutLog)
{
	bool bAll = true;
	auto Load = [&](const TCHAR* File, TFunctionRef<bool(const TArray<TMap<FString, FString>>&, FString&)> Fn)
	{
		FString Text, Err; TArray<TMap<FString, FString>> Rows;
		if (!FFileHelper::LoadFileToString(Text, *(Dir / File))) { OutLog += FString::Printf(TEXT("%s missing; "), File); bAll = false; return; }
		if (!ReadCsvRows(Text, Rows, Err) || !Fn(Rows, Err)) { OutLog += FString::Printf(TEXT("%s rejected (%s); "), File, *Err); bAll = false; return; }
		OutLog += FString::Printf(TEXT("%s %d rows; "), File, Rows.Num());
	};
	auto I = [](const TMap<FString, FString>& R, const TCHAR* K) { const FString* V = R.Find(K); return V ? FCString::Atoi(**V) : 0; };
	auto F = [](const TMap<FString, FString>& R, const TCHAR* K) { const FString* V = R.Find(K); return V ? FCString::Atof(**V) : 0.f; };
	auto S = [](const TMap<FString, FString>& R, const TCHAR* K) { const FString* V = R.Find(K); return V ? *V : FString(); };
	auto Nm = [&](const TMap<FString, FString>& R, const TCHAR* K) { const FString V = S(R, K); return V.IsEmpty() ? NAME_None : FName(*V); };

	Load(TEXT("Economy.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		for (const TMap<FString, FString>& R : Rows)
		{
			const FString K = S(R, TEXT("key")), V = S(R, TEXT("value"));
			if (K == TEXT("daily_match_gem_cap")) DailyMatchGemCap = FCString::Atoi(*V);
			else if (K == TEXT("daily_reset_hour_local")) DailyResetHourLocal = FCString::Atoi(*V);
			else if (K == TEXT("rollback_tolerance_minutes")) RollbackToleranceMinutes = FCString::Atoi(*V);
			else if (K == TEXT("start_gems")) StartGems = FCString::Atoi(*V);
			else if (K == TEXT("earned_balance_cap")) EarnedBalanceCap = FCString::Atoi(*V);
			else if (K == TEXT("spend_order")) SpendOrder = V.Equals(TEXT("PurchasedFirst"), ESearchCase::IgnoreCase) ? EZoogiSpendOrder::PurchasedFirst : EZoogiSpendOrder::EarnedFirst;
			else if (K == TEXT("idle_fraction_zero_reward")) IdleFractionZeroReward = FCString::Atof(*V);
			else if (K == TEXT("focus_change_context")) FocusChangeContext = V.Equals(TEXT("Menu"), ESearchCase::IgnoreCase) ? EZoogiPlayContext::Menu : EZoogiPlayContext::Standby;
			else if (K == TEXT("rewarded_match_ids_kept")) RewardedMatchIdsKept = FCString::Atoi(*V);
			else if (K == TEXT("dr_after_matches")) DRAfterMatches = FCString::Atoi(*V);
			else if (K == TEXT("dr_mult")) DRMult = FCString::Atof(*V);
		}
		return true;
	});
	Load(TEXT("MasteryCurve.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<int32> X, G;
		for (const TMap<FString, FString>& R : Rows) { if (I(R, TEXT("level")) != X.Num() + 1) { Err = TEXT("levels must be 1..N in order"); return false; } X.Add(I(R, TEXT("cumulative_xp"))); G.Add(I(R, TEXT("level_gems"))); }
		MasteryCumulativeXP = X; MasteryLevelGems = G; return true;
	});
	Load(TEXT("MatchRewards.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiRewardRow> Out;
		for (const TMap<FString, FString>& R : Rows) Out.Add({ Nm(R, TEXT("source")), Nm(R, TEXT("tag")), I(R, TEXT("gems")), I(R, TEXT("xp")), I(R, TEXT("per_match_cap")), S(R, TEXT("xp_to")) != TEXT("Event") });
		Rewards = Out; return true;
	});
	Load(TEXT("CPUTiers.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiTierRow> Out;
		for (const TMap<FString, FString>& R : Rows)
		{
			FZoogiTierRow T; if (!ParseTier(S(R, TEXT("tier")), T.Tier)) { Err = TEXT("bad tier ") + S(R, TEXT("tier")); return false; }
			T.Candidates = I(R, TEXT("candidates")); T.PickSpread = F(R, TEXT("pick_spread")); T.Lookahead = I(R, TEXT("lookahead")); T.bComboPlan = I(R, TEXT("combo_plan")) != 0;
			T.EnergyReserve = I(R, TEXT("energy_reserve")); T.AimNoise = F(R, TEXT("aim_noise")); T.GemMult = F(R, TEXT("gem_mult")); T.XPMult = F(R, TEXT("xp_mult"));
			Out.Add(T);
		}
		Tiers = Out; return true;
	});
	Load(TEXT("Powers.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiPowerRow> Out;
		for (const TMap<FString, FString>& R : Rows)
		{
			FZoogiPowerRow P; P.PowerId = Nm(R, TEXT("power_id")); P.Name = S(R, TEXT("name"));
			if (!ZoogiCards::ParseElement(S(R, TEXT("element")), P.Element)) { Err = TEXT("bad element"); return false; }
			P.Tier = Nm(R, TEXT("tier")); P.bDefaultUnlocked = B(S(R, TEXT("default_unlocked"))); P.MasteryLevel = I(R, TEXT("mastery_level")); P.GemCost = I(R, TEXT("gem_cost")); P.CardId = Nm(R, TEXT("card_id"));
			Out.Add(P);
		}
		Powers = Out; return true;
	});
	Load(TEXT("Achievements.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiAchievementRow> Out;
		for (const TMap<FString, FString>& R : Rows) Out.Add({ Nm(R, TEXT("id")), S(R, TEXT("name")), Nm(R, TEXT("stat")), I(R, TEXT("target")), I(R, TEXT("gems")), Nm(R, TEXT("reward_title")), Nm(R, TEXT("reward_cosmetic")) });
		Achievements = Out; return true;
	});
	Load(TEXT("Titles.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiTitleRow> Out; for (const TMap<FString, FString>& R : Rows) Out.Add({ Nm(R, TEXT("id")), S(R, TEXT("name")), Nm(R, TEXT("source")) });
		Titles = Out; return true;
	});
	Load(TEXT("Cosmetics.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiCosmeticRow> Out; for (const TMap<FString, FString>& R : Rows) Out.Add({ Nm(R, TEXT("id")), S(R, TEXT("name")), Nm(R, TEXT("slot")), S(R, TEXT("source")), I(R, TEXT("displayed")) != 0 });
		Cosmetics = Out; return true;
	});
	Load(TEXT("Affinity.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiAffinityLevelRow> Out; for (const TMap<FString, FString>& R : Rows) Out.Add({ I(R, TEXT("level")), I(R, TEXT("cumulative_xp")), Nm(R, TEXT("reward_kind")), S(R, TEXT("reward_id")) });
		AffinityLevels = Out; return true;
	});
	Load(TEXT("AffinityXP.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TMap<FName, TPair<int32, int32>> Out; for (const TMap<FString, FString>& R : Rows) Out.Add(Nm(R, TEXT("source")), { I(R, TEXT("xp")), I(R, TEXT("per_match_cap")) });
		AffinityXP = Out; return true;
	});
	Load(TEXT("GemPackages.csv"), [&](const TArray<TMap<FString, FString>>& Rows, FString& Err)
	{
		TArray<FZoogiGemPackageRow> Out;
		for (const TMap<FString, FString>& R : Rows)
			Out.Add({ Nm(R, TEXT("package_id")), I(R, TEXT("gems")), I(R, TEXT("bonus_gems")), F(R, TEXT("price_usd")), S(R, TEXT("apple_product_id")), S(R, TEXT("google_product_id")), S(R, TEXT("steam_item_id")), S(R, TEXT("epic_offer_id")) });
		GemPackages = Out; return true;
	});
	Source = bAll ? TEXT("csv") : TEXT("csv+builtin");
	return bAll;
}

bool FZoogiProgressionConfig::Validate(FString& OutError) const
{
	if (MasteryCumulativeXP.Num() != 10 || MasteryLevelGems.Num() != 10) { OutError = TEXT("mastery curve must have 10 levels"); return false; }
	if (MasteryCumulativeXP[0] != 0) { OutError = TEXT("level 1 must be 0 XP"); return false; }
	for (int32 i = 1; i < 10; ++i) if (MasteryCumulativeXP[i] <= MasteryCumulativeXP[i - 1]) { OutError = TEXT("mastery XP must increase"); return false; }
	if (DailyMatchGemCap <= 0 || DailyResetHourLocal < 0 || DailyResetHourLocal > 23) { OutError = TEXT("bad daily cap/reset hour"); return false; }
	if (Tiers.Num() != 4) { OutError = TEXT("need 4 CPU tiers"); return false; }
	TSet<FName> Seen;
	for (const FZoogiPowerRow& P : Powers)
	{
		if (Seen.Contains(P.PowerId)) { OutError = TEXT("duplicate power ") + P.PowerId.ToString(); return false; }
		Seen.Add(P.PowerId);
		if (P.Tier == FName("advanced") && (P.MasteryLevel < 6 || P.MasteryLevel > 7 || P.GemCost < 400 || P.GemCost > 600))
		{ OutError = FString::Printf(TEXT("advanced power %s must be mastery 6-7 OR 400-600 gems"), *P.PowerId.ToString()); return false; }
		if (!P.bDefaultUnlocked && (P.MasteryLevel < 1 || P.MasteryLevel > 10)) { OutError = TEXT("locked power needs a mastery level 1-10: ") + P.PowerId.ToString(); return false; }
	}
	if (Titles.Num() != 7) { OutError = TEXT("need exactly 7 titles"); return false; }
	for (const FZoogiAchievementRow& A : Achievements)
		if (!A.Title.IsNone() && !Titles.ContainsByPredicate([&](const FZoogiTitleRow& T) { return T.Id == A.Title; })) { OutError = TEXT("unknown title ") + A.Title.ToString(); return false; }
	for (const FZoogiAchievementRow& A : Achievements)
		if (!A.Cosmetic.IsNone() && !Cosmetics.ContainsByPredicate([&](const FZoogiCosmeticRow& C) { return C.Id == A.Cosmetic; })) { OutError = TEXT("unknown cosmetic ") + A.Cosmetic.ToString(); return false; }
	return true;
}

const FZoogiRewardRow* FZoogiProgressionConfig::FindReward(FName S, FName Tag) const
{
	const FZoogiRewardRow* R = Rewards.FindByPredicate([&](const FZoogiRewardRow& X) { return X.Source == S && X.Tag == Tag; });
	if (!R && S == FName("trick")) R = Rewards.FindByPredicate([&](const FZoogiRewardRow& X) { return X.Source == S && X.Tag == FName("Trick.Other"); });
	return R;
}
const FZoogiTierRow& FZoogiProgressionConfig::Tier(EZoogiCPUTier T) const
{
	static const FZoogiTierRow Def;
	const FZoogiTierRow* R = Tiers.FindByPredicate([&](const FZoogiTierRow& X) { return X.Tier == T; });
	return R ? *R : Def;
}
const FZoogiPowerRow* FZoogiProgressionConfig::FindPower(FName Id) const { return Powers.FindByPredicate([&](const FZoogiPowerRow& P) { return P.PowerId == Id; }); }
int32 FZoogiProgressionConfig::LevelForXP(int32 XP) const
{
	int32 L = 1;
	for (int32 i = 0; i < MasteryCumulativeXP.Num(); ++i) if (XP >= MasteryCumulativeXP[i]) L = i + 1;
	return L;
}

// ================================================================== profile JSON

FString FZoogiProfile::ToJsonString() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("schema"), SchemaVersion);
	J->SetStringField(TEXT("profile_id"), Gs(ProfileId)); J->SetStringField(TEXT("linked_account"), LinkedAccountId);
	J->SetStringField(TEXT("created_utc"), Iso(CreatedUtc)); J->SetStringField(TEXT("updated_utc"), Iso(UpdatedUtc));
	J->SetObjectField(TEXT("wallet"), Wallet.ToJson());
	TArray<TSharedPtr<FJsonValue>> Mx; for (int32 i = 0; i < 5; ++i) Mx.Add(MakeShared<FJsonValueNumber>(MasteryXP[i]));
	J->SetArrayField(TEXT("mastery_xp"), Mx); J->SetStringField(TEXT("mastery_utc"), Iso(MasteryUpdatedUtc));
	J->SetStringField(TEXT("focus"), ZoogiCards::ElementName(FocusElement)); J->SetStringField(TEXT("focus_utc"), Iso(FocusChangedUtc));
	TSharedRef<FJsonObject> D = MakeShared<FJsonObject>();
	D->SetStringField(TEXT("day_start_local"), Iso(Daily.DayStartLocal)); D->SetStringField(TEXT("last_reset_utc"), Iso(Daily.LastResetUtc)); D->SetStringField(TEXT("last_seen_utc"), Iso(Daily.LastSeenUtc));
	D->SetNumberField(TEXT("match_gems_today"), Daily.MatchGemsToday); D->SetNumberField(TEXT("matches_today"), Daily.MatchesToday); D->SetBoolField(TEXT("rollback"), Daily.bRollbackDetected);
	J->SetObjectField(TEXT("daily"), D);
	TArray<TSharedPtr<FJsonValue>> Rm; for (const FGuid& G : RewardedMatchIds) Rm.Add(MakeShared<FJsonValueString>(Gs(G)));
	J->SetArrayField(TEXT("rewarded_matches"), Rm);
	TArray<TSharedPtr<FJsonValue>> Un;
	for (const FZoogiUnlockRecord& U : Unlocks) { TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("power"), U.PowerId.ToString()); X->SetStringField(TEXT("method"), U.Method); X->SetStringField(TEXT("utc"), Iso(U.Utc)); Un.Add(MakeShared<FJsonValueObject>(X)); }
	J->SetArrayField(TEXT("unlocks"), Un);
	TSharedRef<FJsonObject> St = MakeShared<FJsonObject>(); for (const auto& P : Stats) St->SetNumberField(P.Key.ToString(), P.Value);
	J->SetObjectField(TEXT("stats"), St);
	TArray<TSharedPtr<FJsonValue>> Ac;
	for (const FZoogiAchievementRecord& A : Achievements) { TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetStringField(TEXT("id"), A.Id.ToString()); X->SetStringField(TEXT("utc"), Iso(A.Utc)); Ac.Add(MakeShared<FJsonValueObject>(X)); }
	J->SetArrayField(TEXT("achievements"), Ac);
	TSharedRef<FJsonObject> Af = MakeShared<FJsonObject>();
	for (const auto& P : Affinity)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>(); X->SetNumberField(TEXT("xp"), P.Value.XP); X->SetStringField(TEXT("utc"), Iso(P.Value.UpdatedUtc));
		TArray<TSharedPtr<FJsonValue>> C; for (int32 L : P.Value.ClaimedLevels) C.Add(MakeShared<FJsonValueNumber>(L));
		X->SetArrayField(TEXT("claimed"), C); Af->SetObjectField(P.Key.ToString(), X);
	}
	J->SetObjectField(TEXT("affinity"), Af);
	auto NameSet = [](const TSet<FName>& S) { TArray<TSharedPtr<FJsonValue>> A; for (const FName& N : S) A.Add(MakeShared<FJsonValueString>(N.ToString())); return A; };
	J->SetArrayField(TEXT("titles"), NameSet(OwnedTitles)); J->SetStringField(TEXT("equipped_title"), EquippedTitle.IsNone() ? FString() : EquippedTitle.ToString());
	J->SetArrayField(TEXT("cosmetics"), NameSet(OwnedCosmetics));
	TSharedRef<FJsonObject> Eq = MakeShared<FJsonObject>(); for (const auto& P : EquippedCosmetics) Eq->SetStringField(P.Key.ToString(), P.Value.ToString());
	J->SetObjectField(TEXT("equipped_cosmetics"), Eq);
	TArray<TSharedPtr<FJsonValue>> Pu;
	for (const FZoogiPurchaseRecord& P : Purchases)
	{
		TSharedRef<FJsonObject> X = MakeShared<FJsonObject>();
		X->SetStringField(TEXT("store_txn"), P.StoreTransactionId); X->SetStringField(TEXT("store"), P.Store); X->SetStringField(TEXT("package"), P.PackageId.ToString()); X->SetStringField(TEXT("product"), P.ProductId);
		X->SetNumberField(TEXT("gems"), P.Gems); X->SetNumberField(TEXT("state"), (int32)P.State); X->SetStringField(TEXT("grant_txn"), Gs(P.GrantTxnId));
		X->SetStringField(TEXT("created_utc"), Iso(P.CreatedUtc)); X->SetStringField(TEXT("updated_utc"), Iso(P.UpdatedUtc)); X->SetStringField(TEXT("note"), P.Note);
		Pu.Add(MakeShared<FJsonValueObject>(X));
	}
	J->SetArrayField(TEXT("purchases"), Pu);
	FString Out; TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(J, W);
	return Out;
}

bool FZoogiProfile::FromJsonString(const FString& S, FString& OutError)
{
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S), J) || !J.IsValid()) { OutError = TEXT("not JSON"); return false; }
	FZoogiProfile P;
	P.SchemaVersion = (int32)J->GetNumberField(TEXT("schema"));
	if (P.SchemaVersion < 1 || P.SchemaVersion > CurrentSchema) { OutError = FString::Printf(TEXT("unsupported schema %d"), P.SchemaVersion); return false; }
	P.ProfileId = Gp(J->GetStringField(TEXT("profile_id"))); P.LinkedAccountId = J->GetStringField(TEXT("linked_account"));
	P.CreatedUtc = FromIso(J->GetStringField(TEXT("created_utc"))); P.UpdatedUtc = FromIso(J->GetStringField(TEXT("updated_utc")));
	if (!P.Wallet.FromJson(J->GetObjectField(TEXT("wallet")), OutError)) return false;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("mastery_xp"), A)) for (int32 i = 0; i < FMath::Min(5, A->Num()); ++i) P.MasteryXP[i] = FMath::Max(0, (int32)(*A)[i]->AsNumber());
	P.MasteryUpdatedUtc = FromIso(J->GetStringField(TEXT("mastery_utc")));
	ZoogiCards::ParseElement(J->GetStringField(TEXT("focus")), P.FocusElement);
	if (P.FocusElement == EZoogiElement::Neutral) P.FocusElement = EZoogiElement::Void;
	P.FocusChangedUtc = FromIso(J->GetStringField(TEXT("focus_utc")));
	if (const TSharedPtr<FJsonObject>* D = nullptr; J->TryGetObjectField(TEXT("daily"), D))
	{
		P.Daily.DayStartLocal = FromIso((*D)->GetStringField(TEXT("day_start_local"))); P.Daily.LastResetUtc = FromIso((*D)->GetStringField(TEXT("last_reset_utc")));
		P.Daily.LastSeenUtc = FromIso((*D)->GetStringField(TEXT("last_seen_utc"))); P.Daily.MatchGemsToday = (int32)(*D)->GetNumberField(TEXT("match_gems_today"));
		P.Daily.MatchesToday = (int32)(*D)->GetNumberField(TEXT("matches_today")); P.Daily.bRollbackDetected = (*D)->GetBoolField(TEXT("rollback"));
	}
	if (J->TryGetArrayField(TEXT("rewarded_matches"), A)) for (const auto& V : *A) P.RewardedMatchIds.Add(Gp(V->AsString()));
	if (J->TryGetArrayField(TEXT("unlocks"), A)) for (const auto& V : *A) { const auto X = V->AsObject(); P.Unlocks.Add({ FName(*X->GetStringField(TEXT("power"))), X->GetStringField(TEXT("method")), FromIso(X->GetStringField(TEXT("utc"))) }); }
	if (const TSharedPtr<FJsonObject>* O = nullptr; J->TryGetObjectField(TEXT("stats"), O)) for (const auto& KV : (*O)->Values) P.Stats.Add(FName(*KV.Key), (int32)KV.Value->AsNumber());
	if (J->TryGetArrayField(TEXT("achievements"), A)) for (const auto& V : *A) { const auto X = V->AsObject(); P.Achievements.Add({ FName(*X->GetStringField(TEXT("id"))), FromIso(X->GetStringField(TEXT("utc"))) }); }
	if (const TSharedPtr<FJsonObject>* O = nullptr; J->TryGetObjectField(TEXT("affinity"), O))
		for (const auto& KV : (*O)->Values)
		{
			const auto X = KV.Value->AsObject(); FZoogiAffinityTrack T; T.XP = (int32)X->GetNumberField(TEXT("xp")); T.UpdatedUtc = FromIso(X->GetStringField(TEXT("utc")));
			const TArray<TSharedPtr<FJsonValue>>* C = nullptr; if (X->TryGetArrayField(TEXT("claimed"), C)) for (const auto& L : *C) T.ClaimedLevels.Add((int32)L->AsNumber());
			P.Affinity.Add(FName(*KV.Key), T);
		}
	if (J->TryGetArrayField(TEXT("titles"), A)) for (const auto& V : *A) P.OwnedTitles.Add(FName(*V->AsString()));
	{ const FString E = J->GetStringField(TEXT("equipped_title")); P.EquippedTitle = E.IsEmpty() ? NAME_None : FName(*E); }
	if (J->TryGetArrayField(TEXT("cosmetics"), A)) for (const auto& V : *A) P.OwnedCosmetics.Add(FName(*V->AsString()));
	if (const TSharedPtr<FJsonObject>* O = nullptr; J->TryGetObjectField(TEXT("equipped_cosmetics"), O)) for (const auto& KV : (*O)->Values) P.EquippedCosmetics.Add(FName(*KV.Key), FName(*KV.Value->AsString()));
	if (J->TryGetArrayField(TEXT("purchases"), A))
		for (const auto& V : *A)
		{
			const auto X = V->AsObject(); FZoogiPurchaseRecord R;
			R.StoreTransactionId = X->GetStringField(TEXT("store_txn")); R.Store = X->GetStringField(TEXT("store")); R.PackageId = FName(*X->GetStringField(TEXT("package"))); R.ProductId = X->GetStringField(TEXT("product"));
			R.Gems = (int32)X->GetNumberField(TEXT("gems")); R.State = (EZoogiPurchaseState)FMath::Clamp((int32)X->GetNumberField(TEXT("state")), 0, 3); R.GrantTxnId = Gp(X->GetStringField(TEXT("grant_txn")));
			R.CreatedUtc = FromIso(X->GetStringField(TEXT("created_utc"))); R.UpdatedUtc = FromIso(X->GetStringField(TEXT("updated_utc"))); R.Note = X->GetStringField(TEXT("note"));
			P.Purchases.Add(R);
		}
	*this = MoveTemp(P);
	return true;
}

// ================================================================== service

void FZoogiProgression::Init(const FZoogiProgressionConfig& InConfig)
{
	Cfg = InConfig;
	Profile = FZoogiProfile();
	Profile.ProfileId = FGuid::NewGuid();
	Profile.CreatedUtc = Profile.UpdatedUtc = FDateTime::UtcNow();
	Profile.Wallet.EarnedCap = Cfg.EarnedBalanceCap; Profile.Wallet.SpendOrder = Cfg.SpendOrder;
	if (Cfg.StartGems > 0) Profile.Wallet.Grant(DeterministicId(TEXT("start:") + Gs(Profile.ProfileId)), EZoogiGemBucket::Earned, Cfg.StartGems, "start", Profile.CreatedUtc);
	for (const FZoogiCosmeticRow& C : Cfg.Cosmetics) if (C.Source == TEXT("default")) { Profile.OwnedCosmetics.Add(C.Id); Profile.EquippedCosmetics.FindOrAdd(C.Slot) = C.Id; }
}

bool FZoogiProgression::Load(const FZoogiProgressionConfig& InConfig, const FString& Json, FString& OutError)
{
	Cfg = InConfig;
	FZoogiProfile P;
	if (!P.FromJsonString(Json, OutError)) return false;
	Profile = MoveTemp(P);
	Profile.Wallet.EarnedCap = Cfg.EarnedBalanceCap; Profile.Wallet.SpendOrder = Cfg.SpendOrder;
	for (const FZoogiCosmeticRow& C : Cfg.Cosmetics) if (C.Source == TEXT("default")) { Profile.OwnedCosmetics.Add(C.Id); if (!Profile.EquippedCosmetics.Contains(C.Slot)) Profile.EquippedCosmetics.Add(C.Slot, C.Id); }
	return true;
}

EZoogiTxnResult FZoogiProgression::GrantGems(const FGuid& TxnId, EZoogiGemBucket Bucket, int32 Amount, FName Source, const FDateTime& Utc, const FString& ExternalId)
{
	const EZoogiTxnResult R = Profile.Wallet.Grant(TxnId, Bucket, Amount, Source, Utc, ExternalId);
	if (R == EZoogiTxnResult::Applied) Touch(Utc);
	return R;
}
EZoogiTxnResult FZoogiProgression::SpendGems(const FGuid& TxnId, int32 Amount, FName Sink, const FDateTime& Utc)
{
	const EZoogiTxnResult R = Profile.Wallet.Spend(TxnId, Amount, Sink, Utc);
	if (R == EZoogiTxnResult::Applied) Touch(Utc);
	return R;
}

int32 FZoogiProgression::MasteryXP(EZoogiElement E) const { return Profile.MasteryXP[(int32)E]; }
int32 FZoogiProgression::MasteryLevel(EZoogiElement E) const { return E == EZoogiElement::Neutral ? 0 : Cfg.LevelForXP(Profile.MasteryXP[(int32)E]); }

int32 FZoogiProgression::AddMasteryXP(EZoogiElement E, int32 Amount, const FDateTime& Utc, TArray<FName>* OutLevelUps, int32* OutGems)
{
	if (Amount <= 0) return 0;
	E = Resolve(E);
	const int32 Before = MasteryLevel(E);
	Profile.MasteryXP[(int32)E] = (int32)FMath::Min<int64>((int64)Profile.MasteryXP[(int32)E] + Amount, MAX_int32 / 2);
	Profile.MasteryUpdatedUtc = Utc; Touch(Utc);
	const int32 After = MasteryLevel(E);
	for (int32 L = Before + 1; L <= After; ++L)
	{
		const int32 G = Cfg.MasteryLevelGems.IsValidIndex(L - 1) ? Cfg.MasteryLevelGems[L - 1] : 0;
		const FString Key = FString::Printf(TEXT("mastery:%s:%d:%s"), ZoogiCards::ElementName(E), L, *Gs(Profile.ProfileId));
		if (G > 0 && Profile.Wallet.Grant(DeterministicId(Key), EZoogiGemBucket::Earned, G, "mastery.level", Utc, Key) == EZoogiTxnResult::Applied && OutGems) *OutGems += G;
		if (OutLevelUps) OutLevelUps->Add(FName(*FString::Printf(TEXT("%s:%d"), ZoogiCards::ElementName(E), L)));
	}
	return After - Before;
}

bool FZoogiProgression::SetFocusElement(EZoogiElement E, EZoogiPlayContext Ctx, const FDateTime& Utc, FString& OutWhy)
{
	if (E == EZoogiElement::Neutral) { OutWhy = TEXT("Focus must be Void, Fire, Shock or Frost"); return false; }
	if (Ctx != EZoogiPlayContext::Standby && !(Cfg.FocusChangeContext == EZoogiPlayContext::Menu && Ctx == EZoogiPlayContext::Menu))
	{ OutWhy = TEXT("Focus Element can only be changed in Standby"); return false; }
	Profile.FocusElement = E; Profile.FocusChangedUtc = Utc; Touch(Utc);
	return true;
}

// --- hooks: element resolved NOW (a later focus change does not move already-earned XP)
void FZoogiProgression::HookKO(FZoogiMatchTally& T, EZoogiElement E) const { T.Events.Add({ "ko", NAME_None, Resolve(E) }); }
void FZoogiProgression::HookRingOut(FZoogiMatchTally& T, EZoogiElement E) const { T.Events.Add({ "ring_out", NAME_None, Resolve(E) }); }
void FZoogiProgression::HookTrickShot(FZoogiMatchTally& T, FName Tag, EZoogiElement E) const
{
	const FZoogiRewardRow* R = Cfg.FindReward("trick", Tag);
	const FName Key = R ? R->Tag : FName("Trick.Other");
	T.Events.Add({ "trick", Key, (R && R->bXPToFocus) ? Profile.FocusElement : Resolve(E) });
}
void FZoogiProgression::HookPowerUsed(FZoogiMatchTally& T, EZoogiElement E) const { T.Events.Add({ "power_used", NAME_None, Resolve(E) }); }
void FZoogiProgression::HookOrbCollected(FZoogiMatchTally& T) const { T.Events.Add({ "orb_collected", NAME_None, Profile.FocusElement }); }
void FZoogiProgression::HookEpisodeTriggered(FZoogiMatchTally& T) const { T.Events.Add({ "episode", NAME_None, Profile.FocusElement }); }

void FZoogiProgression::CheckDailyReset(const FDateTime& NowUtc, const FDateTime& NowLocal)
{
	FZoogiDailyState& D = Profile.Daily;
	const FTimespan Tol = FTimespan::FromMinutes(Cfg.RollbackToleranceMinutes);
	if (D.LastSeenUtc.GetTicks() > 0 && NowUtc < D.LastSeenUtc - Tol)
	{
		D.bRollbackDetected = true; // clock moved back: freeze resets (counters kept) until real time passes LastSeenUtc again
		return;
	}
	D.bRollbackDetected = false;
	if (NowUtc > D.LastSeenUtc) D.LastSeenUtc = NowUtc;
	const FDateTime B = ResetBoundary(NowLocal, Cfg.DailyResetHourLocal);
	if (B > D.DayStartLocal)
	{
		D.DayStartLocal = B; D.LastResetUtc = NowUtc; D.MatchGemsToday = 0; D.MatchesToday = 0;
	}
}

FDateTime FZoogiProgression::NextResetLocal() const { return Profile.Daily.DayStartLocal + FTimespan::FromDays(1); }

FZoogiMatchRewardResult FZoogiProgression::ApplyMatch(const FZoogiMatchTally& T, EZoogiMatchOutcome Outcome, const FDateTime& NowUtc, const FDateTime& NowLocal)
{
	FZoogiMatchRewardResult R;
	CheckDailyReset(NowUtc, NowLocal);
	auto Finish = [&]() { R.EarnedToday = EarnedToday(); R.RemainingToday = RemainingToday(); return R; };
	if (!T.MatchId.IsValid() || Profile.RewardedMatchIds.Contains(T.MatchId)) { R.bDuplicate = T.MatchId.IsValid(); R.ZeroReason = R.bDuplicate ? TEXT("duplicate") : TEXT("no match id"); return Finish(); }
	// remember the id even for zero-reward matches (a forfeit can't be "re-finished" for rewards)
	Profile.RewardedMatchIds.Add(T.MatchId);
	while (Profile.RewardedMatchIds.Num() > FMath::Max(1, Cfg.RewardedMatchIdsKept)) Profile.RewardedMatchIds.RemoveAt(0);
	if (Outcome == EZoogiMatchOutcome::Forfeit) { R.ZeroReason = TEXT("forfeit"); return Finish(); }
	if (Outcome == EZoogiMatchOutcome::Abandoned) { R.ZeroReason = TEXT("abandoned"); return Finish(); }
	if (T.Turns > 0 && (float)T.IdleTurns >= Cfg.IdleFractionZeroReward * T.Turns) { R.ZeroReason = TEXT("idle"); return Finish(); }

	TArray<FZoogiAwardEvent> Ev = T.Events;
	Ev.Insert({ "completion", NAME_None, Profile.FocusElement }, 0);
	if (Outcome == EZoogiMatchOutcome::Win) Ev.Insert({ "win", NAME_None, Profile.FocusElement }, 1);
	if (Outcome == EZoogiMatchOutcome::Draw) Ev.Insert({ "draw", NAME_None, Profile.FocusElement }, 1);
	TMap<FString, int32> Counted;
	int32 XPBase[5] = { 0, 0, 0, 0, 0 };
	for (const FZoogiAwardEvent& E : Ev)
	{
		const FZoogiRewardRow* Row = Cfg.FindReward(E.Source, E.Tag);
		if (!Row) continue;
		int32& N = Counted.FindOrAdd(Row->Source.ToString() + Row->Tag.ToString());
		if (N >= Row->PerMatchCap) continue; // per-source per-match cap
		++N;
		FZoogiRewardLine* L = R.Lines.FindByPredicate([&](const FZoogiRewardLine& X) { return X.Source == Row->Source && X.Tag == Row->Tag && X.Element == E.XPElement; });
		if (!L) { L = &R.Lines.AddDefaulted_GetRef(); L->Source = Row->Source; L->Tag = Row->Tag; L->Element = E.XPElement; }
		L->Count++; L->Gems += Row->Gems; L->XP += Row->XP;
		R.BaseGems += Row->Gems;
		const EZoogiElement XE = E.XPElement == EZoogiElement::Neutral ? Profile.FocusElement : E.XPElement;
		XPBase[(int32)XE] += Row->XP;
	}
	const FZoogiTierRow& Tier = Cfg.Tier(T.Tier);
	const float DR = Profile.Daily.MatchesToday >= Cfg.DRAfterMatches ? Cfg.DRMult : 1.f;
	R.GemMult = Tier.GemMult * DR; R.XPMult = Tier.XPMult * DR;
	R.GemsBeforeCap = FMath::RoundToInt(R.BaseGems * R.GemMult);
	R.GemsPaid = FMath::Min(R.GemsBeforeCap, RemainingToday()); // 500/day cap on repeatable match gems
	R.CapClipped = R.GemsBeforeCap - R.GemsPaid;
	const FString MKey = TEXT("match:") + Gs(T.MatchId);
	if (R.GemsPaid > 0) Profile.Wallet.Grant(DeterministicId(MKey), EZoogiGemBucket::Earned, R.GemsPaid, "match", NowUtc, MKey);
	Profile.Daily.MatchGemsToday += R.GemsPaid;
	Profile.Daily.MatchesToday += 1;
	for (int32 i = 1; i < 5; ++i)
	{
		R.XP[i] = FMath::RoundToInt(XPBase[i] * R.XPMult); // XP continues past the gem cap (Evan Q13)
		AddMasteryXP((EZoogiElement)i, R.XP[i], NowUtc, &R.LevelUps, &R.LevelUpGems);
	}
	// stats (achievements). Awakening counted here only.
	AddStat("matches_completed", 1);
	if (Outcome == EZoogiMatchOutcome::Win) { AddStat("wins", 1); if (T.Tier == EZoogiCPUTier::Expert) AddStat("expert_wins", 1); }
	int32 KOs = 0;
	for (const FZoogiAwardEvent& E : T.Events)
	{
		if (E.Source == FName("ko")) { AddStat("kos", 1); ++KOs; }
		else if (E.Source == FName("ring_out")) AddStat("ring_outs", 1);
		else if (E.Source == FName("trick")) AddStat("tricks", 1);
		else if (E.Source == FName("episode")) AddStat("episodes", 1);
		else if (E.Source == FName("orb_collected")) AddStat("orbs", 1);
	}
	if (T.Awakenings > 0) AddStat("awakenings", T.Awakenings);
	// affinity: the controlled Zoogi's independent track
	if (!T.ZoogiId.IsNone())
	{
		auto AX = [&](FName S, int32 N) { const TPair<int32, int32>* P = Cfg.AffinityXP.Find(S); return P ? P->Key * FMath::Min(N, P->Value) : 0; };
		R.AffinityXP = AX("participation", 1) + (Outcome == EZoogiMatchOutcome::Win ? AX("win", 1) : 0) + AX("ko", KOs) + AX("signature_hit", T.SignatureHits);
		AddAffinityXP(T.ZoogiId, R.AffinityXP, NowUtc);
	}
	R.NewAchievements = EvaluateAchievements(NowUtc, &R.AchievementGems);
	R.bApplied = true;
	Touch(NowUtc);
	return Finish();
}

// --- powers
bool FZoogiProgression::IsPowerUnlocked(FName PowerId) const
{
	const FZoogiPowerRow* P = Cfg.FindPower(PowerId);
	if (!P) return true; // unknown to progression = not gated here
	return P->bDefaultUnlocked || Profile.Unlocks.ContainsByPredicate([&](const FZoogiUnlockRecord& U) { return U.PowerId == PowerId; });
}
bool FZoogiProgression::CanUnlockPower(FName PowerId, bool& bByMastery, FString& OutWhy) const
{
	bByMastery = false;
	const FZoogiPowerRow* P = Cfg.FindPower(PowerId);
	if (!P) { OutWhy = TEXT("unknown power"); return false; }
	if (IsPowerUnlocked(PowerId)) { OutWhy = TEXT("already unlocked"); return false; }
	if (P->MasteryLevel > 0 && MasteryLevel(P->Element) >= P->MasteryLevel) { bByMastery = true; return true; }
	if (P->GemCost > 0 && Profile.Wallet.Total() >= P->GemCost) return true;
	OutWhy = FString::Printf(TEXT("Need %s mastery %d or %d gems"), ZoogiCards::ElementName(P->Element), P->MasteryLevel, P->GemCost);
	return false;
}
bool FZoogiProgression::UnlockPower(FName PowerId, const FDateTime& Utc, FString& OutWhy)
{
	bool bByMastery = false;
	if (!CanUnlockPower(PowerId, bByMastery, OutWhy)) return false;
	const FZoogiPowerRow* P = Cfg.FindPower(PowerId);
	if (!bByMastery)
	{
		const FString Key = TEXT("unlock:") + PowerId.ToString() + TEXT(":") + Gs(Profile.ProfileId);
		if (Profile.Wallet.Spend(DeterministicId(Key), P->GemCost, "unlock.power", Utc, Key) != EZoogiTxnResult::Applied) { OutWhy = TEXT("spend failed"); return false; }
	}
	Profile.Unlocks.Add({ PowerId, bByMastery ? TEXT("mastery") : TEXT("gems"), Utc });
	Touch(Utc);
	return true;
}

// --- affinity
int32 FZoogiProgression::AffinityLevel(FName ZoogiId) const
{
	const FZoogiAffinityTrack* T = Profile.Affinity.Find(ZoogiId);
	const int32 XP = T ? T->XP : 0; int32 L = 1;
	for (const FZoogiAffinityLevelRow& R : Cfg.AffinityLevels) if (XP >= R.CumulativeXP) L = FMath::Max(L, R.Level);
	return L;
}
void FZoogiProgression::AddAffinityXP(FName ZoogiId, int32 XP, const FDateTime& Utc)
{
	if (ZoogiId.IsNone() || XP <= 0) return;
	FZoogiAffinityTrack& T = Profile.Affinity.FindOrAdd(ZoogiId); T.XP += XP; T.UpdatedUtc = Utc;
	int32 MaxL = 0; for (const auto& P : Profile.Affinity) MaxL = FMath::Max(MaxL, AffinityLevel(P.Key));
	Profile.Stats.FindOrAdd("max_affinity_level") = MaxL;
}
bool FZoogiProgression::ClaimAffinityReward(FName ZoogiId, int32 Level, const FDateTime& Utc, FString& OutRewardId)
{
	FZoogiAffinityTrack* T = Profile.Affinity.Find(ZoogiId);
	const FZoogiAffinityLevelRow* Row = Cfg.AffinityLevels.FindByPredicate([&](const FZoogiAffinityLevelRow& R) { return R.Level == Level; });
	if (!T || !Row || Row->RewardId.IsEmpty() || AffinityLevel(ZoogiId) < Level || T->ClaimedLevels.Contains(Level)) return false;
	OutRewardId = Row->RewardId.Replace(TEXT("{zoogi}"), *ZoogiId.ToString().ToLower());
	T->ClaimedLevels.Add(Level); T->UpdatedUtc = Utc;
	Profile.OwnedCosmetics.Add(FName(*OutRewardId)); // emote / pose / profile anim / intro: cosmetic, noncompetitive
	Touch(Utc);
	return true;
}

// --- achievements / titles / cosmetics
void FZoogiProgression::AddStat(FName S, int32 Delta) { Profile.Stats.FindOrAdd(S) += Delta; }
int32 FZoogiProgression::Stat(FName S) const { const int32* V = Profile.Stats.Find(S); return V ? *V : 0; }
int32 FZoogiProgression::DerivedStat(FName S) const
{
	if (S == FName("max_mastery_level")) { int32 M = 0; for (int32 i = 1; i < 5; ++i) M = FMath::Max(M, MasteryLevel((EZoogiElement)i)); return M; }
	if (S == FName("min_core_mastery_level")) { int32 M = 99; for (int32 i = 1; i < 5; ++i) M = FMath::Min(M, MasteryLevel((EZoogiElement)i)); return M; }
	return Stat(S);
}
bool FZoogiProgression::HasAchievement(FName Id) const { return Profile.Achievements.ContainsByPredicate([&](const FZoogiAchievementRecord& A) { return A.Id == Id; }); }
TArray<FName> FZoogiProgression::EvaluateAchievements(const FDateTime& Utc, int32* OutGems)
{
	TArray<FName> New;
	for (const FZoogiAchievementRow& A : Cfg.Achievements)
	{
		if (HasAchievement(A.Id) || DerivedStat(A.Stat) < A.Target) continue;
		Profile.Achievements.Add({ A.Id, Utc });
		const FString Key = TEXT("achievement:") + A.Id.ToString() + TEXT(":") + Gs(Profile.ProfileId);
		if (A.Gems > 0 && Profile.Wallet.Grant(DeterministicId(Key), EZoogiGemBucket::Earned, A.Gems, "achievement", Utc, Key) == EZoogiTxnResult::Applied && OutGems) *OutGems += A.Gems;
		if (!A.Title.IsNone()) { Profile.OwnedTitles.Add(A.Title); if (Profile.EquippedTitle.IsNone()) Profile.EquippedTitle = A.Title; }
		if (!A.Cosmetic.IsNone()) Profile.OwnedCosmetics.Add(A.Cosmetic);
		New.Add(A.Id);
	}
	if (New.Num()) Touch(Utc);
	return New;
}
bool FZoogiProgression::EquipTitle(FName Id, const FDateTime& Utc)
{
	if (!Id.IsNone() && !Profile.OwnedTitles.Contains(Id)) return false;
	Profile.EquippedTitle = Id; Touch(Utc); return true;
}
FString FZoogiProgression::TitleName(FName Id) const
{
	const FZoogiTitleRow* T = Cfg.Titles.FindByPredicate([&](const FZoogiTitleRow& X) { return X.Id == Id; });
	return T ? T->Name : FString();
}
bool FZoogiProgression::BuyCosmetic(FName Id, const FDateTime& Utc, FString& OutWhy)
{
	const FZoogiCosmeticRow* C = Cfg.Cosmetics.FindByPredicate([&](const FZoogiCosmeticRow& X) { return X.Id == Id; });
	if (!C) { OutWhy = TEXT("unknown cosmetic"); return false; }
	if (Profile.OwnedCosmetics.Contains(Id)) { OutWhy = TEXT("owned"); return false; }
	if (!C->Source.StartsWith(TEXT("gems:"))) { OutWhy = TEXT("earned through ") + C->Source; return false; }
	const int32 Price = FCString::Atoi(*C->Source.RightChop(5));
	const FString Key = TEXT("cosmetic:") + Id.ToString() + TEXT(":") + Gs(Profile.ProfileId);
	const EZoogiTxnResult R = Profile.Wallet.Spend(DeterministicId(Key), Price, "cosmetic", Utc, Key);
	if (R != EZoogiTxnResult::Applied) { OutWhy = TxnResultName(R); return false; }
	Profile.OwnedCosmetics.Add(Id); Touch(Utc);
	return true;
}
bool FZoogiProgression::EquipCosmetic(FName Id, const FDateTime& Utc)
{
	const FZoogiCosmeticRow* C = Cfg.Cosmetics.FindByPredicate([&](const FZoogiCosmeticRow& X) { return X.Id == Id; });
	if (!C || !Profile.OwnedCosmetics.Contains(Id)) return false;
	Profile.EquippedCosmetics.FindOrAdd(C->Slot) = Id; Touch(Utc);
	return true;
}
