#include "ZoogiCards.h"
#include "ZoogiProgression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"

const TCHAR* ZoogiCardResultName(EZoogiCardResult R)
{
	switch (R)
	{
	case EZoogiCardResult::Ok: return TEXT("ok");
	case EZoogiCardResult::NotInStandby: return TEXT("not_in_standby");
	case EZoogiCardResult::UnknownPlayer: return TEXT("unknown_player");
	case EZoogiCardResult::UnknownInstance: return TEXT("unknown_instance");
	case EZoogiCardResult::UnknownCard: return TEXT("unknown_card");
	case EZoogiCardResult::AlreadySelected: return TEXT("already_selected");
	case EZoogiCardResult::NotSelected: return TEXT("not_selected");
	case EZoogiCardResult::DuplicateCardType: return TEXT("duplicate_card_type");
	case EZoogiCardResult::Unaffordable: return TEXT("unaffordable");
	case EZoogiCardResult::InvalidTarget: return TEXT("invalid_target");
	case EZoogiCardResult::ActorKnockedOut: return TEXT("actor_knocked_out");
	case EZoogiCardResult::AlreadyCommitted: return TEXT("already_committed");
	}
	return TEXT("?");
}

// ------------------------------------------------------------------ backend

const FZoogiCardDef* FZoogiNativeCardBackend::FindDef(FName Id) const
{
	for (const FZoogiCardDef& D : Defs) if (D.Id == Id) return &D;
	return nullptr;
}

void FZoogiNativeCardBackend::Shuffle(TArray<FZoogiCardInstance>& Cards, FRandomStream& Rng) const
{
	for (int32 i = Cards.Num() - 1; i > 0; --i) Cards.Swap(i, Rng.RandRange(0, i)); // Fisher-Yates on the seeded stream
}

// ------------------------------------------------------------------ toolkit backend (reflection on the BP struct FCard)

namespace
{
	FProperty* FindAuthored(const UStruct* S, const TCHAR* Name)
	{
		for (TFieldIterator<FProperty> It(S); It; ++It) if (It->GetAuthoredName() == Name) return *It;
		return nullptr;
	}
	bool ReadUseCost(const FProperty* UseRules, const void* Row, int32& OutCost)
	{
		const FArrayProperty* Arr = CastField<FArrayProperty>(UseRules);
		const FStructProperty* Inner = Arr ? CastField<FStructProperty>(Arr->Inner) : nullptr;
		if (!Inner) return false;
		FScriptArrayHelper H(Arr, Arr->ContainerPtrToValuePtr<void>(Row));
		const FProperty* RuleP = FindAuthored(Inner->Struct, TEXT("Rule"));
		const FNumericProperty* CostP = CastField<FNumericProperty>(FindAuthored(Inner->Struct, TEXT("Cost")));
		if (!CostP) return false;
		for (int32 i = 0; i < H.Num(); ++i)
		{
			const uint8* E = H.GetRawPtr(i);
			FString RuleName;
			if (const FObjectPropertyBase* OP = CastField<FObjectPropertyBase>(RuleP)) RuleName = GetNameSafe(OP->GetObjectPropertyValue_InContainer(E));
			if (!RuleName.Contains(TEXT("StatCost"))) continue;
			const void* V = CostP->ContainerPtrToValuePtr<void>(E);
			OutCost = CostP->IsFloatingPoint() ? FMath::RoundToInt(CostP->GetFloatingPointPropertyValue(V)) : (int32)CostP->GetSignedIntPropertyValue(V);
			return true;
		}
		return false;
	}
}

TSharedPtr<FZoogiToolkitCardBackend> FZoogiToolkitCardBackend::TryLoad(const TArray<FZoogiCardDef>& NativeDefs, FString& OutWhy)
{
	const UDataTable* DT = LoadObject<UDataTable>(nullptr, TablePath);
	if (!DT) { OutWhy = TEXT("DT_ZoogiCards not found"); return nullptr; }
	const UScriptStruct* RS = DT->GetRowStruct();
	if (!RS || RS->GetPathName() != RowStructPath) { OutWhy = FString::Printf(TEXT("row struct %s is not the toolkit FCard"), RS ? *RS->GetPathName() : TEXT("none")); return nullptr; }
	const FProperty* NameP = FindAuthored(RS, TEXT("CardName"));
	const FProperty* DescP = FindAuthored(RS, TEXT("Description"));
	const FProperty* UseP = FindAuthored(RS, TEXT("UseRules"));
	if (!NameP || !UseP) { OutWhy = TEXT("FCard fields CardName/UseRules not found"); return nullptr; }
	TSharedPtr<FZoogiToolkitCardBackend> B = MakeShared<FZoogiToolkitCardBackend>();
	for (const FZoogiCardDef& N : NativeDefs)
	{
		const uint8* const* Row = DT->GetRowMap().Find(N.Id);
		if (!Row) { OutWhy = FString::Printf(TEXT("row %s missing"), *N.Id.ToString()); return nullptr; }
		FZoogiCardDef D = N;
		if (!ReadUseCost(UseP, *Row, D.Cost)) { OutWhy = FString::Printf(TEXT("row %s has no BP_UseRule_StatCost cost"), *N.Id.ToString()); return nullptr; }
		if (const FTextProperty* TP = CastField<FTextProperty>(NameP)) D.Name = TP->GetPropertyValue_InContainer(*Row).ToString().ToUpper();
		if (const FTextProperty* TP = CastField<FTextProperty>(DescP)) D.Rule = TP->GetPropertyValue_InContainer(*Row).ToString();
		B->Defs.Add(D);
	}
	return B;
}

const FZoogiCardDef* FZoogiToolkitCardBackend::FindDef(FName Id) const
{
	for (const FZoogiCardDef& D : Defs) if (D.Id == Id) return &D;
	return nullptr;
}

void FZoogiToolkitCardBackend::Shuffle(TArray<FZoogiCardInstance>& Cards, FRandomStream& Rng) const
{
	for (int32 i = Cards.Num() - 1; i > 0; --i) Cards.Swap(i, Rng.RandRange(0, i));
}

// ------------------------------------------------------------------ tune

void FZoogiCardTune::Load()
{
	const TCHAR* S = TEXT("ZoogiRoll.Cards");
	if (GConfig)
	{
		GConfig->GetBool(S, TEXT("bCards"), bCards, GGameIni);
		GConfig->GetInt(S, TEXT("HandSize"), HandSize, GGameIni);
		GConfig->GetInt(S, TEXT("EnergyPerTurn"), EnergyPerTurn, GGameIni);
		GConfig->GetInt(S, TEXT("DeckSize"), DeckSize, GGameIni);
		GConfig->GetInt(S, TEXT("MaxCopies"), MaxCopies, GGameIni);
		GConfig->GetInt(S, TEXT("MaxCardsPerCommit"), MaxCardsPerCommit, GGameIni);
		GConfig->GetString(S, TEXT("DefaultDeck"), DefaultDeck, GGameIni);
		GConfig->GetString(S, TEXT("Backend"), Backend, GGameIni);
	}
	const TCHAR* Cmd = FCommandLine::Get();
	FString V;
	if (FParse::Value(Cmd, TEXT("ZoogiCards="), V)) bCards = !(V.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || V == TEXT("0") || V.Equals(TEXT("False"), ESearchCase::IgnoreCase));
	else if (FParse::Param(Cmd, TEXT("ZoogiCards"))) bCards = true;
	FParse::Value(Cmd, TEXT("ZoogiCardDeck="), DefaultDeck);
	FParse::Value(Cmd, TEXT("ZoogiCardBackend="), Backend);
	HandSize = FMath::Clamp(HandSize, 1, 10);
	EnergyPerTurn = FMath::Clamp(EnergyPerTurn, 0, 20);
	DeckSize = FMath::Clamp(DeckSize, 1, 60);
	MaxCopies = FMath::Clamp(MaxCopies, 1, 10);
	MaxCardsPerCommit = FMath::Clamp(MaxCardsPerCommit, 1, 10);
}

// ------------------------------------------------------------------ data

namespace ZoogiCards
{
	static FZoogiCardDef Def(const TCHAR* Id, const TCHAR* Name, int32 Cost, EZoogiCardType T, EZoogiCardTarget Tg, EZoogiCardExpiry E, float A, float B, int32 W, const TCHAR* Rule, EZoogiCardElement El = EZoogiCardElement::Neutral)
	{
		FZoogiCardDef D;
		D.Id = Id; D.Name = Name; D.Cost = Cost; D.Type = T; D.Target = Tg; D.Expiry = E; D.ParamA = A; D.ParamB = B; D.CPUWeight = W; D.Rule = Rule; D.Element = El;
		return D;
	}

	TArray<FZoogiCardDef> BuiltInDefs()
	{
		using T = EZoogiCardType; using G = EZoogiCardTarget; using E = EZoogiCardExpiry;
		TArray<FZoogiCardDef> O;
		O.Add(Def(TEXT("storm"), TEXT("STORM"), 1, T::Infusion, G::ActingZoogi, E::TurnEnd, 1.25f, 0.f, 3, TEXT("Temporary multiplier on roll control; no auto-aim, no extra shot."), EZoogiCardElement::Shock));
		O.Add(Def(TEXT("bank_strike"), TEXT("BANK STRIKE"), 2, T::Technique, G::BankSurface, E::LeadActionEnd, 0.f, 0.f, 2, TEXT("Mark a bank; only a real qualifying wall/bumper contact emits BankSetup.")));
		O.Add(Def(TEXT("guard"), TEXT("GUARD"), 1, T::Defense, G::DefendingZoogi, E::NextQualifyingEnemyImpact, 12.f, 0.75f, 2, TEXT("Absorb 12 and x0.75 the next qualifying enemy impulse, once.")));
		O.Add(Def(TEXT("orb_relay"), TEXT("ORB RELAY"), 1, T::Utility, G::NeutralOrb, E::LeadActionEnd, 30.f, 0.f, 2, TEXT("Physical Orb contact creates a relay candidate; partner + 30 Combo required.")));
		O.Add(Def(TEXT("heavy_roll"), TEXT("HEAVY ROLL"), 2, T::Impact, G::ActingZoogi, E::LeadActionEnd, 1.25f, 1.2f, 3, TEXT("Force impulse x1.25, aim error envelope x1.2.")));
		O.Add(Def(TEXT("ember"), TEXT("EMBER"), 2, T::Infusion, G::ActingZoogi, E::TurnEnd, 1.2f, 0.f, 2, TEXT("Enemy push x1.2; clears overlapping Frost cells only."), EZoogiCardElement::Fire));
		O.Add(Def(TEXT("titan"), TEXT("TITAN"), 2, T::Infusion, G::ActingZoogi, E::TurnEnd, 1.35f, 0.8f, 2, TEXT("Mass x1.35, received push x0.8; prior mass cached and restored."), EZoogiCardElement::Void));
		O.Add(Def(TEXT("frost"), TEXT("FROST"), 2, T::Terrain, G::SurfaceLane, E::UntilLocallyClearedOrMatchReset, 0.35f, 0.f, 1, TEXT("Shared bounded low-friction lane; never stacks."), EZoogiCardElement::Frost));
		O.Add(Def(TEXT("precision"), TEXT("PRECISION"), 1, T::Control, G::ActingZoogi, E::LeadActionEnd, 0.7f, 1.f, 3, TEXT("Initial aim error x0.7; preview first contact + one rebound."), EZoogiCardElement::Void));
		O.Add(Def(TEXT("momentum_steal"), TEXT("MOMENTUM STEAL"), 2, T::Technique, G::EnemyContact, E::FirstQualifiedContactOrLeadActionEnd, 0.25f, 0.6f, 1, TEXT("Glancing hit: <=25% tangential transfer, damage x0.6.")));
		O.Add(Def(TEXT("crystal_collapse"), TEXT("CRYSTAL COLLAPSE"), 2, T::Terrain, G::CrystalPiece, E::PersistentTerrainState, 1.f, 0.f, 1, TEXT("Qualifying contact advances one breakable piece by one state (exempt from bDestruction=False; pillars never)."), EZoogiCardElement::Void));
		O.Add(Def(TEXT("orb_magnet"), TEXT("ORB MAGNET"), 1, T::Utility, G::NeutralOrb, E::CommitPreparationThenLeadAction, 0.6f, 1.5f, 1, TEXT("0.6 s bounded pull on a neutral Orb, <=1.5 diameters, blocked by geometry.")));
		O.Add(Def(TEXT("double_impact"), TEXT("DOUBLE IMPACT"), 2, T::Impact, G::EnemyContact, E::LeadActionEnd, 0.12f, 0.5f, 2, TEXT("Max two bursts in one contact episode; second at 50% after 0.12 s.")));
		O.Add(Def(TEXT("anchor_guard"), TEXT("ANCHOR GUARD"), 1, T::Defense, G::ActingZoogi, E::NextQualifyingEnemyImpact, 12.f, 0.3f, 1, TEXT("Arms after settling: absorb 12, next enemy push x0.3.")));
		O.Add(Def(TEXT("recovery_roll"), TEXT("RECOVERY ROLL"), 1, T::Control, G::ActingZoogi, E::LeadActionEnd, 1.5f, 0.f, 1, TEXT("Deceleration x1.5 near the chosen safe zone; no heal, no Ring Mark change.")));
		return O;
	}

	TArray<FName> BuiltInDeck()
	{
		return { TEXT("storm"), TEXT("storm"), TEXT("heavy_roll"), TEXT("heavy_roll"), TEXT("guard"), TEXT("guard"),
			TEXT("precision"), TEXT("bank_strike"), TEXT("titan"), TEXT("orb_relay"), TEXT("recovery_roll"), TEXT("crystal_collapse"), TEXT("precision"), TEXT("ember"), TEXT("anchor_guard") };
	}

	template <typename EnumT, int32 N>
	static bool ParseEnum(const FString& S, const TCHAR* (&Names)[N], EnumT& Out)
	{
		for (int32 i = 0; i < N; ++i) if (S.Equals(Names[i], ESearchCase::IgnoreCase)) { Out = (EnumT)i; return true; }
		return false;
	}
	static const TCHAR* TypeNames[] = { TEXT("Infusion"), TEXT("Technique"), TEXT("Defense"), TEXT("Utility"), TEXT("Impact"), TEXT("Terrain"), TEXT("Control") };
	static const TCHAR* ExpiryNames[] = { TEXT("TurnEnd"), TEXT("LeadActionEnd"), TEXT("NextQualifyingEnemyImpact"), TEXT("UntilLocallyClearedOrMatchReset"),
		TEXT("PersistentTerrainState"), TEXT("FirstQualifiedContactOrLeadActionEnd"), TEXT("CommitPreparationThenLeadAction") };
	static const TCHAR* TargetNames[] = { TEXT("ActingZoogi"), TEXT("DefendingZoogi"), TEXT("BankSurface"), TEXT("NeutralOrb"), TEXT("EnemyContact"), TEXT("SurfaceLane"), TEXT("CrystalPiece") };
	const TCHAR* TypeName(EZoogiCardType T) { return TypeNames[(int32)T]; }
	const TCHAR* ExpiryName(EZoogiCardExpiry E) { return ExpiryNames[(int32)E]; }
	const TCHAR* TargetName(EZoogiCardTarget T) { return TargetNames[(int32)T]; }
	static const TCHAR* ElementNames[] = { TEXT("Neutral"), TEXT("Void"), TEXT("Fire"), TEXT("Shock"), TEXT("Frost") };
	const TCHAR* ElementName(EZoogiCardElement E) { return ElementNames[(int32)E]; }
	bool ParseElement(const FString& S, EZoogiCardElement& Out) { return ParseEnum(S.TrimStartAndEnd(), ElementNames, Out); }

	static void SplitLines(const FString& Text, TArray<FString>& Lines)
	{
		Text.Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Lines, TEXT("\n"), true);
		Lines.RemoveAll([](const FString& L) { const FString T = L.TrimStartAndEnd(); return T.IsEmpty() || T.StartsWith(TEXT("#")); });
	}

	bool ParseCardsCsv(const FString& Text, TArray<FZoogiCardDef>& Out, FString& OutError)
	{
		Out.Reset();
		TArray<FString> Lines; SplitLines(Text, Lines);
		if (Lines.Num() < 2) { OutError = TEXT("no rows"); return false; }
		// id,name,cost,type,target,expiry,param_a,param_b,cpu_weight,requires_alive,[element,]rule (rule = rest of the line)
		// Oct 14: optional 'element' column (header-detected, before rule); absent = Neutral, so older CSVs still parse.
		TArray<FString> Hdr; Lines[0].ParseIntoArray(Hdr, TEXT(","), false);
		const bool bHasElement = Hdr.Num() > 10 && Hdr[10].TrimStartAndEnd().Equals(TEXT("element"), ESearchCase::IgnoreCase);
		const int32 RuleCol = bHasElement ? 11 : 10;
		for (int32 L = 1; L < Lines.Num(); ++L)
		{
			TArray<FString> F; Lines[L].ParseIntoArray(F, TEXT(","), false);
			if (F.Num() < RuleCol + 1) { OutError = FString::Printf(TEXT("line %d: %d fields"), L + 1, F.Num()); Out.Reset(); return false; }
			FZoogiCardDef D;
			D.Id = FName(*F[0].TrimStartAndEnd().ToLower());
			D.Name = F[1].TrimStartAndEnd();
			D.Cost = FCString::Atoi(*F[2]);
			const bool bOk = ParseEnum(F[3].TrimStartAndEnd(), TypeNames, D.Type) && ParseEnum(F[4].TrimStartAndEnd(), TargetNames, D.Target) && ParseEnum(F[5].TrimStartAndEnd(), ExpiryNames, D.Expiry);
			if (!bOk || D.Id.IsNone() || D.Cost < 0 || D.Cost > 10) { OutError = FString::Printf(TEXT("line %d: bad id/cost/type/target/expiry"), L + 1); Out.Reset(); return false; }
			D.ParamA = FCString::Atof(*F[6]); D.ParamB = FCString::Atof(*F[7]); D.CPUWeight = FCString::Atoi(*F[8]);
			D.bRequiresAlive = !F[9].TrimStartAndEnd().Equals(TEXT("false"), ESearchCase::IgnoreCase) && F[9].TrimStartAndEnd() != TEXT("0");
			if (bHasElement && !ParseElement(F[10], D.Element)) { OutError = FString::Printf(TEXT("line %d: bad element '%s'"), L + 1, *F[10]); Out.Reset(); return false; }
			TArray<FString> Rest; for (int32 i = RuleCol; i < F.Num(); ++i) Rest.Add(F[i]);
			D.Rule = FString::Join(Rest, TEXT(",")).TrimStartAndEnd().TrimQuotes();
			for (const FZoogiCardDef& X : Out) if (X.Id == D.Id) { OutError = FString::Printf(TEXT("duplicate id %s"), *D.Id.ToString()); Out.Reset(); return false; }
			Out.Add(D);
		}
		return true;
	}

	bool ParseDecksCsv(const FString& Text, TMap<FString, TArray<FName>>& Out, FString& OutError)
	{
		Out.Reset();
		TArray<FString> Lines; SplitLines(Text, Lines);
		for (int32 L = 1; L < Lines.Num(); ++L)
		{
			TArray<FString> F; Lines[L].ParseIntoArray(F, TEXT(","), false);
			if (F.Num() < 2) { OutError = FString::Printf(TEXT("line %d"), L + 1); Out.Reset(); return false; }
			Out.FindOrAdd(F[0].TrimStartAndEnd()).Add(FName(*F[1].TrimStartAndEnd().ToLower()));
		}
		return Out.Num() > 0;
	}

	bool ValidateDeck(const TArray<FName>& Deck, const IZoogiCardBackend& Backend, int32 DeckSize, int32 MaxCopies, FString& OutError)
	{
		if (Deck.Num() != DeckSize) { OutError = FString::Printf(TEXT("size %d != %d"), Deck.Num(), DeckSize); return false; }
		TMap<FName, int32> N;
		for (const FName& Id : Deck)
		{
			if (!Backend.FindDef(Id)) { OutError = FString::Printf(TEXT("unknown card %s"), *Id.ToString()); return false; }
			if (++N.FindOrAdd(Id) > MaxCopies) { OutError = FString::Printf(TEXT("%s x%d > %d copies"), *Id.ToString(), N[Id], MaxCopies); return false; }
		}
		return true;
	}

	static FPowerUnlockQuery& UnlockQueryRef() { static FPowerUnlockQuery Q; return Q; }
	void SetPowerUnlockQuery(FPowerUnlockQuery Query) { UnlockQueryRef() = MoveTemp(Query); }
	const FPowerUnlockQuery& GetPowerUnlockQuery() { return UnlockQueryRef(); }
	static FSavedDeckQuery& SavedDeckRef() { static FSavedDeckQuery Q; return Q; }
	void SetSavedDeckQuery(FSavedDeckQuery Query) { SavedDeckRef() = MoveTemp(Query); }
	const FSavedDeckQuery& GetSavedDeckQuery() { return SavedDeckRef(); }
	int32 LinkRequiredPowers(TArray<FZoogiCardDef>& Defs, const FString& PowersCsv, FString& OutLog)
	{
		TArray<TMap<FString, FString>> Rows; FString Err;
		if (!ZoogiProgress::ReadCsvRows(PowersCsv, Rows, Err)) { OutLog += FString::Printf(TEXT(" Powers.csv unreadable (%s): no card power links;"), *Err); return 0; }
		int32 N = 0;
		for (const TMap<FString, FString>& R : Rows)
		{
			const FString* Card = R.Find(TEXT("card_id")); const FString* Power = R.Find(TEXT("power_id"));
			if (!Card || !Power || Card->IsEmpty() || Power->IsEmpty()) continue;
			for (FZoogiCardDef& D : Defs) if (D.Id == FName(**Card)) { D.RequiredPower = FName(**Power); ++N; }
		}
		OutLog += FString::Printf(TEXT(" %d card(s) linked to powers;"), N);
		return N;
	}
	int32 LinkRequiredPowersFromData(TArray<FZoogiCardDef>& Defs, FString& OutLog)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/Progression/Powers.csv")))) { OutLog += TEXT(" no Powers.csv: no card power links;"); return 0; }
		return LinkRequiredPowers(Defs, Text, OutLog);
	}

	TArray<FName> FilterDeckByUnlocks(const TArray<FName>& Deck, const IZoogiCardBackend& Backend, int32 DeckSize, int32 MaxCopies, const FPowerUnlockQuery& IsUnlocked, FString& OutLog)
	{
		if (!IsUnlocked) return Deck; // no unlock system: everything unlocked, deck untouched
		auto Eligible = [&](FName Id) { const FZoogiCardDef* D = Backend.FindDef(Id); return D && (D->RequiredPower.IsNone() || IsUnlocked(D->RequiredPower)); };
		TArray<FName> Out; TMap<FName, int32> N; TArray<FString> Dropped;
		for (const FName& Id : Deck)
		{
			if (!Eligible(Id)) { Dropped.Add(Id.ToString()); continue; }
			if (N.FindOrAdd(Id) >= MaxCopies) continue;
			++N[Id]; Out.Add(Id);
		}
		if (Dropped.Num() == 0) return Deck;
		int32 Added = 0;
		for (bool bProgress = true; Out.Num() < DeckSize && bProgress; )
		{
			bProgress = false;
			for (const FZoogiCardDef& D : Backend.AllDefs())
			{
				if (Out.Num() >= DeckSize) break;
				if (!Eligible(D.Id) || N.FindOrAdd(D.Id) >= MaxCopies) continue;
				++N[D.Id]; Out.Add(D.Id); ++Added; bProgress = true;
			}
		}
		OutLog += FString::Printf(TEXT(" locked-power cards removed [%s], refilled %d -> %d cards;"), *FString::Join(Dropped, TEXT(",")), Added, Out.Num());
		return Out;
	}

	const TCHAR* ForceLabel(int32 Step, bool bCardsMode)
	{
		static const TCHAR* Classic[4] = { TEXT("AUTO"), TEXT("SOFT"), TEXT("MED"), TEXT("FULL") };
		static const TCHAR* Cards[4] = { TEXT("AUTO"), TEXT("CONTROL"), TEXT("DRIVE"), TEXT("LAUNCH") };
		return (bCardsMode ? Cards : Classic)[FMath::Clamp(Step, 0, 3)];
	}
}

// ------------------------------------------------------------------ rules

bool FZoogiCardRules::Init(TSharedPtr<IZoogiCardBackend> InBackend, const FZoogiCardTune& InTune, int32 Seed, const TArray<TArray<FName>>& Decks, const TArray<FName>& Fallback, FString& OutLog)
{
	BackendPtr = InBackend;
	TuneV = InTune;
	Rng.Initialize(Seed);
	Players.Reset(); Ledger.Reset(); Spent.Reset();
	NextEventId = NextCommitId = NextEffectId = 1;
	if (!BackendPtr.IsValid()) { OutLog = TEXT("no backend"); return false; }
	FString Err;
	const bool bFallbackOk = ZoogiCards::ValidateDeck(Fallback, *BackendPtr, TuneV.DeckSize, TuneV.MaxCopies, Err);
	if (!bFallbackOk) { OutLog = FString::Printf(TEXT("fallback deck invalid: %s"), *Err); return false; }
	int32 NextInstance = 1;
	Players.SetNum(Decks.Num());
	Spent.SetNumZeroed(Decks.Num());
	for (int32 P = 0; P < Decks.Num(); ++P)
	{
		const bool bOk = ZoogiCards::ValidateDeck(Decks[P], *BackendPtr, TuneV.DeckSize, TuneV.MaxCopies, Err);
		if (!bOk) OutLog += FString::Printf(TEXT(" P%d deck invalid (%s) -> fallback;"), P + 1, *Err);
		for (const FName& Id : bOk ? Decks[P] : Fallback) Players[P].DrawPile.Add({ NextInstance++, Id });
		BackendPtr->Shuffle(Players[P].DrawPile, Rng);
		Log(P, TEXT("deck"), nullptr, 0, 0, FString::Printf(TEXT("%d cards%s"), Players[P].DrawPile.Num(), bOk ? TEXT("") : TEXT(" (fallback)")));
	}
	return true;
}

void FZoogiCardRules::Log(int32 P, FName Kind, const FZoogiCardInstance* C, int32 Cost, int64 CommitId, const FString& Detail)
{
	FZoogiCardEvent E;
	E.EventId = NextEventId++;
	E.Player = P;
	E.Turn = Players.IsValidIndex(P) ? Players[P].Turn : -1;
	E.Kind = Kind;
	if (C) { E.CardId = C->CardId; E.InstanceId = C->InstanceId; }
	E.Cost = Cost;
	E.EnergyAfter = Players.IsValidIndex(P) ? Players[P].Energy : 0;
	E.CommitId = CommitId;
	E.Detail = Detail;
	Ledger.Add(E);
}

void FZoogiCardRules::Draw(int32 P, int32 Count)
{
	FZoogiCardPlayerState& S = Players[P];
	for (int32 i = 0; i < Count; ++i)
	{
		if (S.DrawPile.Num() == 0)
		{
			if (S.Discard.Num() == 0) break;
			S.DrawPile = MoveTemp(S.Discard);
			S.Discard.Reset();
			BackendPtr->Shuffle(S.DrawPile, Rng);
			Log(P, TEXT("reshuffle"), nullptr, 0, 0, FString::Printf(TEXT("%d cards"), S.DrawPile.Num()));
		}
		const FZoogiCardInstance C = S.DrawPile.Pop(EAllowShrinking::No);
		S.Hand.Add(C);
		Log(P, TEXT("draw"), &C, 0, 0, FString());
	}
}

void FZoogiCardRules::BeginStandby(int32 P, int32 Turn)
{
	if (!Players.IsValidIndex(P)) return;
	if (Players[P].bStandbyOpen && Players[P].Turn == Turn) return; // apply once
	for (int32 Q = 0; Q < Players.Num(); ++Q) if (Players[Q].bStandbyOpen || Players[Q].Committed.Num() > 0 || Players[Q].Hand.Num() > 0) EndTurn(Q);
	FZoogiCardPlayerState& S = Players[P];
	S.Turn = Turn;
	S.bStandbyOpen = true;
	S.bCommitted = false;
	S.Selected.Reset();
	S.Energy = TuneV.EnergyPerTurn; // refresh, not accumulate
	Log(P, TEXT("standby"), nullptr, 0, 0, FString::Printf(TEXT("energy=%d"), S.Energy));
	Draw(P, FMath::Max(0, TuneV.HandSize - S.Hand.Num()));
}

const FZoogiCardInstance* FZoogiCardRules::FindInHand(int32 P, int32 InstanceId) const
{
	for (const FZoogiCardInstance& C : Players[P].Hand) if (C.InstanceId == InstanceId) return &C;
	return nullptr;
}

EZoogiCardResult FZoogiCardRules::CheckTarget(const FZoogiCardDef& D, const FZoogiCardContext& Ctx) const
{
	if (D.bRequiresAlive && Ctx.bActorKnockedOut) return EZoogiCardResult::ActorKnockedOut;
	switch (D.Target)
	{
	case EZoogiCardTarget::NeutralOrb: return Ctx.bHasNeutralOrb ? EZoogiCardResult::Ok : EZoogiCardResult::InvalidTarget;
	case EZoogiCardTarget::EnemyContact: return Ctx.bHasEnemy ? EZoogiCardResult::Ok : EZoogiCardResult::InvalidTarget;
	case EZoogiCardTarget::BankSurface: return Ctx.bHasBankSurface ? EZoogiCardResult::Ok : EZoogiCardResult::InvalidTarget;
	case EZoogiCardTarget::SurfaceLane: return Ctx.bHasSurfaceLane ? EZoogiCardResult::Ok : EZoogiCardResult::InvalidTarget;
	case EZoogiCardTarget::CrystalPiece: return Ctx.bHasCrystalPiece ? EZoogiCardResult::Ok : EZoogiCardResult::InvalidTarget;
	default: return EZoogiCardResult::Ok;
	}
}

EZoogiCardResult FZoogiCardRules::CanSelect(int32 P, int32 InstanceId, const FZoogiCardContext& Ctx) const
{
	if (!Players.IsValidIndex(P)) return EZoogiCardResult::UnknownPlayer;
	const FZoogiCardPlayerState& S = Players[P];
	if (S.bCommitted) return EZoogiCardResult::AlreadyCommitted;
	if (!S.bStandbyOpen) return EZoogiCardResult::NotInStandby;
	const FZoogiCardInstance* C = FindInHand(P, InstanceId);
	if (!C) return EZoogiCardResult::UnknownInstance;
	const FZoogiCardDef* D = BackendPtr->FindDef(C->CardId);
	if (!D) return EZoogiCardResult::UnknownCard;
	if (S.Selected.Contains(InstanceId)) return EZoogiCardResult::AlreadySelected;
	int32 Cost = 0;
	for (int32 Id : S.Selected)
	{
		const FZoogiCardInstance* X = FindInHand(P, Id);
		if (X && X->CardId == C->CardId) return EZoogiCardResult::DuplicateCardType; // one instance per committed lead action
		if (const FZoogiCardDef* XD = X ? BackendPtr->FindDef(X->CardId) : nullptr) Cost += XD->Cost;
	}
	if (S.Selected.Num() >= TuneV.MaxCardsPerCommit || Cost + D->Cost > S.Energy) return EZoogiCardResult::Unaffordable;
	return CheckTarget(*D, Ctx);
}

EZoogiCardResult FZoogiCardRules::Select(int32 P, int32 InstanceId, const FZoogiCardContext& Ctx)
{
	const EZoogiCardResult R = CanSelect(P, InstanceId, Ctx);
	if (R == EZoogiCardResult::Ok) Players[P].Selected.Add(InstanceId);
	else if (Players.IsValidIndex(P)) { const FZoogiCardInstance* C = FindInHand(P, InstanceId); Log(P, TEXT("reject"), C, 0, 0, ZoogiCardResultName(R)); }
	return R;
}

EZoogiCardResult FZoogiCardRules::Deselect(int32 P, int32 InstanceId)
{
	if (!Players.IsValidIndex(P)) return EZoogiCardResult::UnknownPlayer;
	if (Players[P].bCommitted) return EZoogiCardResult::AlreadyCommitted;
	return Players[P].Selected.Remove(InstanceId) > 0 ? EZoogiCardResult::Ok : EZoogiCardResult::NotSelected;
}

EZoogiCardResult FZoogiCardRules::Toggle(int32 P, int32 InstanceId, const FZoogiCardContext& Ctx)
{
	if (Players.IsValidIndex(P) && Players[P].Selected.Contains(InstanceId)) return Deselect(P, InstanceId);
	return Select(P, InstanceId, Ctx);
}

void FZoogiCardRules::ClearSelection(int32 P)
{
	if (Players.IsValidIndex(P) && !Players[P].bCommitted) Players[P].Selected.Reset();
}

EZoogiCardResult FZoogiCardRules::Commit(int32 P, const FZoogiCardContext& Ctx, TArray<FZoogiCardCommitted>* Out)
{
	if (!Players.IsValidIndex(P)) return EZoogiCardResult::UnknownPlayer;
	FZoogiCardPlayerState& S = Players[P];
	if (S.bCommitted) { Log(P, TEXT("reject"), nullptr, 0, 0, TEXT("already_committed")); return EZoogiCardResult::AlreadyCommitted; }
	if (!S.bStandbyOpen) return EZoogiCardResult::NotInStandby;
	// validate the whole selection again (state may have changed since Select), nothing spent yet
	TArray<int32> Sel = S.Selected;
	S.Selected.Reset();
	for (int32 Id : Sel)
	{
		const EZoogiCardResult R = CanSelect(P, Id, Ctx);
		if (R != EZoogiCardResult::Ok) { S.Selected = Sel; Log(P, TEXT("reject"), FindInHand(P, Id), 0, 0, FString::Printf(TEXT("commit %s"), ZoogiCardResultName(R))); return R; }
		S.Selected.Add(Id);
	}
	// spend once
	const int64 CommitId = NextCommitId++;
	int32 Total = 0;
	for (int32 Id : Sel)
	{
		const int32 Idx = S.Hand.IndexOfByPredicate([Id](const FZoogiCardInstance& C) { return C.InstanceId == Id; });
		const FZoogiCardInstance C = S.Hand[Idx];
		const int32 Cost = BackendPtr->FindDef(C.CardId)->Cost;
		S.Hand.RemoveAt(Idx);
		S.Energy -= Cost;
		Total += Cost;
		FZoogiCardCommitted K; K.Card = C; K.CommitId = CommitId; K.EffectId = NextEffectId++;
		S.Committed.Add(K);
		if (Out) Out->Add(K);
		Log(P, TEXT("commit_card"), &C, Cost, CommitId, FString::Printf(TEXT("effect=%lld"), K.EffectId));
	}
	check(S.Energy >= 0);
	Spent[P] += Total;
	S.Selected.Reset();
	S.bCommitted = true;
	Log(P, TEXT("commit"), nullptr, Total, CommitId, FString::Printf(TEXT("%d cards"), Sel.Num()));
	return EZoogiCardResult::Ok;
}

void FZoogiCardRules::EndTurn(int32 P)
{
	if (!Players.IsValidIndex(P)) return;
	FZoogiCardPlayerState& S = Players[P];
	if (!S.bStandbyOpen && S.Hand.Num() == 0 && S.Committed.Num() == 0) return;
	const int32 N = S.Hand.Num() + S.Committed.Num();
	for (const FZoogiCardCommitted& K : S.Committed) S.Discard.Add(K.Card);
	S.Discard.Append(S.Hand);
	S.Hand.Reset(); S.Committed.Reset(); S.Selected.Reset();
	S.bStandbyOpen = false;
	S.bCommitted = false;
	S.Energy = 0; // unspent Card Energy does not carry over (refresh, not bank)
	Log(P, TEXT("discard"), nullptr, 0, 0, FString::Printf(TEXT("%d cards"), N));
}

uint32 FZoogiCardRules::StateHash() const
{
	uint32 H = 2166136261u;
	auto Mix = [&H](uint32 V) { H = (H ^ V) * 16777619u; };
	for (const FZoogiCardPlayerState& S : Players)
	{
		for (const FZoogiCardInstance& C : S.DrawPile) Mix(C.InstanceId);
		Mix(0xA1);
		for (const FZoogiCardInstance& C : S.Hand) Mix(C.InstanceId);
		Mix(0xB2);
		for (const FZoogiCardInstance& C : S.Discard) Mix(C.InstanceId);
		Mix((uint32)S.Energy);
	}
	for (const FZoogiCardEvent& E : Ledger) { Mix((uint32)E.EventId); Mix(FCrc::StrCrc32(*E.Kind.ToString())); Mix((uint32)E.InstanceId); Mix((uint32)E.Cost); }
	return H;
}

// ------------------------------------------------------------------ v3 P6 save / resume
namespace
{
	void SerInst(FArchive& Ar, FZoogiCardInstance& I) { Ar << I.InstanceId << I.CardId; }
	void SerInsts(FArchive& Ar, TArray<FZoogiCardInstance>& A)
	{
		int32 N = A.Num(); Ar << N; if (Ar.IsLoading()) A.SetNum(N);
		for (FZoogiCardInstance& I : A) SerInst(Ar, I);
	}
	void SerRng(FArchive& Ar, FRandomStream& R)
	{
		// FRandomStream keeps its state privately; it is a trivially copyable pair of seeds.
		static_assert(TIsTriviallyCopyConstructible<FRandomStream>::Value, "FRandomStream must stay trivially copyable for saves");
		Ar.Serialize(&R, sizeof(FRandomStream));
	}
}

void FZoogiCardRules::Serialize(FArchive& Ar)
{
	uint32 Ver = 1; Ar << Ver;
	SerRng(Ar, Rng);
	int32 NP = Players.Num(); Ar << NP; if (Ar.IsLoading()) Players.SetNum(NP);
	for (FZoogiCardPlayerState& S : Players)
	{
		SerInsts(Ar, S.DrawPile); SerInsts(Ar, S.Hand); SerInsts(Ar, S.Discard);
		Ar << S.Selected;
		int32 NC = S.Committed.Num(); Ar << NC; if (Ar.IsLoading()) S.Committed.SetNum(NC);
		for (FZoogiCardCommitted& C : S.Committed) { SerInst(Ar, C.Card); Ar << C.CommitId << C.EffectId; }
		Ar << S.Energy << S.Turn << S.bStandbyOpen << S.bCommitted;
	}
	int32 NE = Ledger.Num(); Ar << NE; if (Ar.IsLoading()) Ledger.SetNum(NE);
	for (FZoogiCardEvent& E : Ledger) Ar << E.EventId << E.Turn << E.Player << E.Kind << E.CardId << E.InstanceId << E.Cost << E.EnergyAfter << E.CommitId << E.Detail;
	Ar << Spent << NextEventId << NextCommitId << NextEffectId;
}
