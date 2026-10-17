// clash-menu: see ZoogiFrontProfile.h
#include "ZoogiFrontProfile.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiGameMode.h"
#include "ZoogiOrb.h"
#include "ZoogiPowerUnlockSubsystem.h"
#include "ZoogiProgression.h"
#include "ZoogiProgressionSubsystem.h"
#include "ZoogiRoll.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "TimerManager.h"

namespace
{
	TArray<TSharedPtr<FJsonValue>> ZFNames(const TArray<FName>& A)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FName& N : A) Out.Add(MakeShared<FJsonValueString>(N.ToString()));
		return Out;
	}
	TArray<FName> ZFReadNames(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		TArray<FName> Out;
		for (const TSharedPtr<FJsonValue>& V : A) if (V.IsValid() && !V->AsString().IsEmpty()) Out.Add(FName(*V->AsString()));
		return Out;
	}
	int32 ZFInt(const TSharedPtr<FJsonObject>& O, const TCHAR* K, int32 Def = 0) { int32 V = Def; if (O.IsValid()) O->TryGetNumberField(K, V); return V; }
}

// ------------------------------------------------------------------------------------------------ json
FString FZoogiFrontProfileData::ToJsonString() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("schema"), SchemaVersion);
	J->SetStringField(TEXT("guest_name"), GuestName);
	J->SetNumberField(TEXT("level"), Level);
	J->SetNumberField(TEXT("xp"), XP);
	J->SetNumberField(TEXT("gold"), Gold);
	J->SetNumberField(TEXT("trophies"), Trophies);
	J->SetNumberField(TEXT("best_trophies"), BestTrophies);
	J->SetNumberField(TEXT("battle_pass_level"), BattlePassLevel);
	J->SetNumberField(TEXT("battle_pass_xp"), BattlePassXP);
	J->SetNumberField(TEXT("wins"), Wins);
	J->SetNumberField(TEXT("battles"), Battles);
	J->SetNumberField(TEXT("losses"), Losses);
	J->SetNumberField(TEXT("draws"), Draws);
	J->SetNumberField(TEXT("three_crown_wins"), ThreeCrownWins);
	J->SetNumberField(TEXT("win_streak"), WinStreak);
	J->SetNumberField(TEXT("best_streak"), BestStreak);
	J->SetStringField(TEXT("tag"), Tag);
	J->SetStringField(TEXT("chest_stock_day"), ChestStockDay);
	{
		TSharedRef<FJsonObject> CB = MakeShared<FJsonObject>();
		for (const TPair<FName, int32>& P : ChestBought) CB->SetNumberField(P.Key.ToString(), P.Value);
		J->SetObjectField(TEXT("chest_bought"), CB);
		TArray<TSharedPtr<FJsonValue>> BL;
		for (const FZoogiFrontBattle& B : BattleLog)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("utc"), LexToString(B.Utc));
			O->SetNumberField(TEXT("outcome"), B.Outcome);
			O->SetStringField(TEXT("opponent"), B.Opponent);
			O->SetNumberField(TEXT("trophies"), B.Trophies);
			O->SetNumberField(TEXT("my_score"), B.MyScore);
			O->SetNumberField(TEXT("opp_score"), B.OppScore);
			O->SetNumberField(TEXT("duration"), B.Duration);
			O->SetStringField(TEXT("replay"), B.ReplayId);
			O->SetNumberField(TEXT("arena"), B.Arena);
			O->SetBoolField(TEXT("dev"), B.bDev);
			BL.Add(MakeShared<FJsonValueObject>(O));
		}
		J->SetArrayField(TEXT("battle_log"), BL);
	}
	J->SetBoolField(TEXT("cards_new"), bCardsNew);
	auto IntSet = [](const TSet<int32>& Set) { TArray<int32> A = Set.Array(); A.Sort(); TArray<TSharedPtr<FJsonValue>> V; for (int32 X : A) V.Add(MakeShared<FJsonValueNumber>(X)); return V; };
	J->SetArrayField(TEXT("trophy_claimed"), IntSet(TrophyClaimed));
	J->SetArrayField(TEXT("pass_free_claimed"), IntSet(PassFreeClaimed));
	J->SetArrayField(TEXT("pass_premium_claimed"), IntSet(PassPremiumClaimed));
	J->SetBoolField(TEXT("pass_premium"), bPassPremium);
	J->SetStringField(TEXT("season_end_utc"), LexToString(SeasonEndUtc));
	J->SetNumberField(TEXT("chest_cycle"), ChestCycle);
	TArray<TSharedPtr<FJsonValue>> C;
	for (const FZoogiFrontChestSlot& S : Chests)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("type"), S.Type.IsNone() ? FString() : S.Type.ToString());
		O->SetStringField(TEXT("unlock_at_utc"), LexToString(S.UnlockAtUtc));
		C.Add(MakeShared<FJsonValueObject>(O));
	}
	J->SetArrayField(TEXT("chests"), C);
	TSharedRef<FJsonObject> Copies = MakeShared<FJsonObject>();
	for (const TPair<FName, int32>& P : CardCopies) Copies->SetNumberField(P.Key.ToString(), P.Value);
	J->SetObjectField(TEXT("card_copies"), Copies);
	TArray<TSharedPtr<FJsonValue>> Dk;
	for (const TArray<FName>& Deck : Decks) Dk.Add(MakeShared<FJsonValueArray>(ZFNames(Deck)));
	J->SetArrayField(TEXT("decks"), Dk);
	J->SetNumberField(TEXT("active_deck"), ActiveDeck);
	J->SetStringField(TEXT("deals_day"), DealsDay);
	TArray<TSharedPtr<FJsonValue>> Dl;
	for (const FZoogiFrontDeal& Deal : Deals)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("card"), Deal.CardId.ToString());
		O->SetNumberField(TEXT("count"), Deal.Count);
		O->SetNumberField(TEXT("price"), Deal.Price);
		O->SetBoolField(TEXT("free"), Deal.bFree);
		O->SetBoolField(TEXT("bought"), Deal.bBought);
		Dl.Add(MakeShared<FJsonValueObject>(O));
	}
	J->SetArrayField(TEXT("deals"), Dl);
	TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
	M->SetBoolField(TEXT("pending"), LastMatch.bPending);
	M->SetNumberField(TEXT("outcome"), LastMatch.Outcome);
	M->SetNumberField(TEXT("trophies"), LastMatch.Trophies);
	M->SetNumberField(TEXT("gold"), LastMatch.Gold);
	M->SetNumberField(TEXT("xp"), LastMatch.XP);
	M->SetNumberField(TEXT("bp"), LastMatch.BP);
	M->SetStringField(TEXT("chest"), LastMatch.Chest.IsNone() ? FString() : LastMatch.Chest.ToString());
	M->SetBoolField(TEXT("slots_full"), LastMatch.bSlotsFull);
	M->SetNumberField(TEXT("new_level"), LastMatch.NewLevel);
	J->SetObjectField(TEXT("last_match"), M);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(J, W);
	return Out;
}

bool FZoogiFrontProfileData::FromJsonString(const FString& S, FString& OutError)
{
	TSharedPtr<FJsonObject> J;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(S);
	if (!FJsonSerializer::Deserialize(R, J) || !J.IsValid()) { OutError = TEXT("not JSON"); return false; }
	int32 Schema = 0;
	if (!J->TryGetNumberField(TEXT("schema"), Schema) || Schema < 1 || Schema > CurrentSchema) { OutError = TEXT("schema"); return false; }
	FZoogiFrontProfileData N;
	J->TryGetStringField(TEXT("guest_name"), N.GuestName);
	N.Level = ZFInt(J, TEXT("level"), 1);
	N.XP = ZFInt(J, TEXT("xp"));
	N.Gold = ZFInt(J, TEXT("gold"), 600);
	N.Trophies = ZFInt(J, TEXT("trophies"));
	N.BestTrophies = ZFInt(J, TEXT("best_trophies"));
	N.BattlePassLevel = ZFInt(J, TEXT("battle_pass_level"), 1);
	N.BattlePassXP = ZFInt(J, TEXT("battle_pass_xp"));
	N.Wins = ZFInt(J, TEXT("wins"));
	N.Battles = ZFInt(J, TEXT("battles"));
	N.Losses = ZFInt(J, TEXT("losses")); N.Draws = ZFInt(J, TEXT("draws")); N.ThreeCrownWins = ZFInt(J, TEXT("three_crown_wins"));
	N.WinStreak = ZFInt(J, TEXT("win_streak")); N.BestStreak = ZFInt(J, TEXT("best_streak"));
	J->TryGetStringField(TEXT("tag"), N.Tag);
	J->TryGetStringField(TEXT("chest_stock_day"), N.ChestStockDay);
	{
		const TSharedPtr<FJsonObject>* CB = nullptr;
		if (J->TryGetObjectField(TEXT("chest_bought"), CB) && CB)
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*CB)->Values) N.ChestBought.Add(FName(*P.Key), FMath::Max(0, (int32)P.Value->AsNumber()));
		const TArray<TSharedPtr<FJsonValue>>* BL = nullptr;
		if (J->TryGetArrayField(TEXT("battle_log"), BL) && BL)
			for (const TSharedPtr<FJsonValue>& V : *BL)
			{
				if (!V.IsValid() || V->Type != EJson::Object) continue;
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FZoogiFrontBattle B; FString U;
				O->TryGetStringField(TEXT("utc"), U); LexFromString(B.Utc, *U);
				B.Outcome = ZFInt(O, TEXT("outcome")); O->TryGetStringField(TEXT("opponent"), B.Opponent);
				B.Trophies = ZFInt(O, TEXT("trophies")); B.MyScore = ZFInt(O, TEXT("my_score")); B.OppScore = ZFInt(O, TEXT("opp_score"));
				B.Duration = ZFInt(O, TEXT("duration")); O->TryGetStringField(TEXT("replay"), B.ReplayId);
				B.Arena = (uint8)FMath::Clamp(ZFInt(O, TEXT("arena")), 0, 255); O->TryGetBoolField(TEXT("dev"), B.bDev);
				N.BattleLog.Add(B);
			}
	}
	N.ChestCycle = ZFInt(J, TEXT("chest_cycle"));
	J->TryGetBoolField(TEXT("cards_new"), N.bCardsNew);
	// schema 1 files: gems_local is ignored (the progression ledger is the only gem balance)
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	auto ReadSet = [&J](const TCHAR* K, TSet<int32>& Out) { const TArray<TSharedPtr<FJsonValue>>* V = nullptr; if (J->TryGetArrayField(K, V) && V) for (const TSharedPtr<FJsonValue>& X : *V) if (X.IsValid()) Out.Add((int32)X->AsNumber()); };
	if (J->HasField(TEXT("trophy_claimed"))) ReadSet(TEXT("trophy_claimed"), N.TrophyClaimed); else N.TrophyClaimed.Add(0);
	ReadSet(TEXT("pass_free_claimed"), N.PassFreeClaimed);
	ReadSet(TEXT("pass_premium_claimed"), N.PassPremiumClaimed);
	J->TryGetBoolField(TEXT("pass_premium"), N.bPassPremium);
	{ FString SE; J->TryGetStringField(TEXT("season_end_utc"), SE); LexFromString(N.SeasonEndUtc, *SE); }
	if (J->TryGetArrayField(TEXT("chests"), A) && A)
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			FZoogiFrontChestSlot Slot;
			if (V.IsValid() && V->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				FString T, U; O->TryGetStringField(TEXT("type"), T); O->TryGetStringField(TEXT("unlock_at_utc"), U);
				Slot.Type = T.IsEmpty() ? NAME_None : FName(*T);
				LexFromString(Slot.UnlockAtUtc, *U);
			}
			else if (V.IsValid() && V->Type == EJson::String && !V->AsString().IsEmpty()) Slot.Type = FName(*V->AsString()); // schema 1
			N.Chests.Add(Slot);
		}
	N.Chests.SetNum(3);
	const TSharedPtr<FJsonObject>* Cp = nullptr;
	if (J->TryGetObjectField(TEXT("card_copies"), Cp) && Cp)
		for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*Cp)->Values) N.CardCopies.Add(FName(*P.Key), FMath::Max(0, (int32)P.Value->AsNumber()));
	if (J->TryGetArrayField(TEXT("decks"), A) && A)
		for (const TSharedPtr<FJsonValue>& V : *A) N.Decks.Add(V.IsValid() && V->Type == EJson::Array ? ZFReadNames(V->AsArray()) : TArray<FName>());
	N.ActiveDeck = FMath::Clamp(ZFInt(J, TEXT("active_deck")), 0, 2);
	J->TryGetStringField(TEXT("deals_day"), N.DealsDay);
	if (J->TryGetArrayField(TEXT("deals"), A) && A)
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FZoogiFrontDeal Deal; FString Id;
			O->TryGetStringField(TEXT("card"), Id); Deal.CardId = FName(*Id);
			Deal.Count = ZFInt(O, TEXT("count"), 1); Deal.Price = ZFInt(O, TEXT("price"));
			O->TryGetBoolField(TEXT("free"), Deal.bFree); O->TryGetBoolField(TEXT("bought"), Deal.bBought);
			N.Deals.Add(Deal);
		}
	const TSharedPtr<FJsonObject>* M = nullptr;
	if (J->TryGetObjectField(TEXT("last_match"), M) && M)
	{
		const TSharedPtr<FJsonObject>& O = *M;
		O->TryGetBoolField(TEXT("pending"), N.LastMatch.bPending);
		N.LastMatch.Outcome = ZFInt(O, TEXT("outcome")); N.LastMatch.Trophies = ZFInt(O, TEXT("trophies")); N.LastMatch.Gold = ZFInt(O, TEXT("gold"));
		N.LastMatch.XP = ZFInt(O, TEXT("xp")); N.LastMatch.BP = ZFInt(O, TEXT("bp")); N.LastMatch.NewLevel = ZFInt(O, TEXT("new_level"));
		FString Ch; O->TryGetStringField(TEXT("chest"), Ch); N.LastMatch.Chest = Ch.IsEmpty() ? NAME_None : FName(*Ch);
		O->TryGetBoolField(TEXT("slots_full"), N.LastMatch.bSlotsFull);
	}
	N.Level = FMath::Max(1, N.Level);
	N.BattlePassLevel = FMath::Max(1, N.BattlePassLevel);
	N.XP = FMath::Max(0, N.XP); N.Gold = FMath::Max(0, N.Gold); N.Trophies = FMath::Max(0, N.Trophies);
	*this = N;
	return true;
}

// ------------------------------------------------------------------------------------------------ lifecycle
void UZoogiFrontProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UZoogiProgressionSubsystem::StaticClass());
	Super::Initialize(Collection);
	FString Json, Err;
	if (FFileHelper::LoadFileToString(Json, *ProfilePath()) && D.FromJsonString(Json, Err) && !D.GuestName.IsEmpty())
	{
		UE_LOG(LogZoogi, Log, TEXT("FRONTEND profile loaded %s: %s L%d %d/%d xp gold=%d trophies=%d"), *ProfilePath(), *D.GuestName, D.Level, D.XP, XPForLevel(D.Level), D.Gold, D.Trophies);
	}
	else
	{
		if (!Json.IsEmpty()) { UE_LOG(LogZoogi, Warning, TEXT("FRONTEND profile rejected (%s): kept as .rejected, new profile"), *Err); IFileManager::Get().Move(*(ProfilePath() + TEXT(".rejected")), *ProfilePath()); }
		D = FZoogiFrontProfileData();
		D.Chests.SetNum(3);
		D.TrophyClaimed.Add(0); // the 0-trophy milestone starts done
		D.GuestName = FString::Printf(TEXT("PLAYER-%06d"), FMath::RandRange(0, 999999)); // generated once, then saved
		UE_LOG(LogZoogi, Log, TEXT("FRONTEND new profile %s (%s)"), *D.GuestName, *ProfilePath());
	}
	D.SchemaVersion = FZoogiFrontProfileData::CurrentSchema;
	if (D.SeasonEndUtc <= 0) D.SeasonEndUtc = NowUtc() + (28 * 24 + 11) * 3600; // "Ends in 28d 11h" on first run, then a real countdown
	if (D.Tag.Len() != 7)
	{
		static const TCHAR* Alpha = TEXT("0289CGJLPQRUVY"); // tag alphabet without look-alikes
		D.Tag.Reset();
		for (int32 i = 0; i < 7; ++i) D.Tag.AppendChar(Alpha[FMath::RandRange(0, 13)]);
	}
	InitDecks();
	Save();
#if !UE_BUILD_SHIPPING
	// Dev test flag (never in Shipping, never on by default): a one-time ledger grant for capture / test runs, only on a
	// named test profile so a real save is never touched. Recorded in the ledger as source "dev.grant".
	int32 Grant = 0; FString ProfileArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiDevGrant="), Grant) && Grant > 0)
	{
		UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
		if (!FParse::Value(FCommandLine::Get(), TEXT("ZoogiProfile="), ProfileArg) || ProfileArg.IsEmpty() || FPaths::GetBaseFilename(ProfileArg) == TEXT("ZoogiProgression"))
		{
			UE_LOG(LogZoogi, Warning, TEXT("FRONTEND dev grant ignored: needs -ZoogiProfile=<test profile>"));
		}
		else if (PS)
		{
			const FGuid Id = ZoogiProgress::DeterministicId(FString::Printf(TEXT("devgrant:%s:%d"), *D.GuestName, Grant));
			const EZoogiTxnResult R = PS->Core().Mutable().Wallet.Grant(Id, EZoogiGemBucket::Earned, Grant, FName(TEXT("dev.grant")), FDateTime::UtcNow());
			if (R == EZoogiTxnResult::Applied) PS->SaveProfile();
			UE_LOG(LogZoogi, Log, TEXT("FRONTEND DEV grant %d gems (%s): %s, ledger total %d"), Grant, *ProfileArg, ZoogiProgress::TxnResultName(R), PS->Core().Get().Wallet.Total());
		}
	}
#endif
}

UZoogiFrontProfileSubsystem* UZoogiFrontProfileSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZoogiFrontProfileSubsystem>() : nullptr;
}

FString UZoogiFrontProfileSubsystem::ProfilePath() const
{
	FString Name = TEXT("ZoogiProgression.json");
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiProfile="), Name);
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (FPaths::GetBaseFilename(Name) + TEXT(".frontend.json"));
}

bool UZoogiFrontProfileSubsystem::Save()
{
	const FString P = ProfilePath(), Tmp = P + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(P), true);
	if (!FFileHelper::SaveStringToFile(D.ToJsonString(), *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
	if (IFileManager::Get().FileExists(*P)) IFileManager::Get().Copy(*(P + TEXT(".bak")), *P);
	return IFileManager::Get().Move(*P, *Tmp, true);
}

// ------------------------------------------------------------------------------------------------ gems (ledger)
int32 UZoogiFrontProfileSubsystem::Gems() const
{
	const UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
	return PS ? PS->Core().Get().Wallet.Total() : 0;
}

bool UZoogiFrontProfileSubsystem::SpendGems(int32 Amount, FName Sink)
{
	UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
	if (!PS || Amount <= 0) return false;
	const EZoogiTxnResult R = PS->Core().Mutable().Wallet.Spend(FGuid::NewGuid(), Amount, Sink, FDateTime::UtcNow());
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND spend %d gems on %s: %s (total %d)"), Amount, *Sink.ToString(), ZoogiProgress::TxnResultName(R), PS->Core().Get().Wallet.Total());
	if (R != EZoogiTxnResult::Applied) return false;
	PS->SaveProfile();
	return true;
}

// ------------------------------------------------------------------------------------------------ levels
int32 UZoogiFrontProfileSubsystem::XPForLevel(int32 Level)
{
	static const int32 Table[] = { 20, 50, 100, 200, 400, 800, 1500, 3000, 5000, 8000, 12000, 16000, 20000 };
	return Table[FMath::Clamp(Level - 1, 0, (int32)UE_ARRAY_COUNT(Table) - 1)];
}

int32 UZoogiFrontProfileSubsystem::ArenaForTrophies(int32 Trophies)
{
	int32 A = 0;
	while (A + 1 < NumTrophyArenas && Trophies >= ArenaTrophies(A + 1)) ++A;
	return A;
}

// ------------------------------------------------------------------------------------------------ cards
const TArray<FZoogiCardDef>& UZoogiFrontProfileSubsystem::CardDefs()
{
	// Same sources as the Deck screen / UZoogiCardSubsystem: Content/Data/Cards.csv, built-in fallback.
	static TArray<FZoogiCardDef> Defs = []()
	{
		TArray<FZoogiCardDef> R = ZoogiCards::BuiltInDefs();
		FString Text, Err;
		TArray<FZoogiCardDef> Csv;
		if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/Cards.csv"))) && ZoogiCards::ParseCardsCsv(Text, Csv, Err) && Csv.Num() > 0) R = Csv;
		FString Log; ZoogiCards::LinkRequiredPowersFromData(R, Log);
		return R;
	}();
	return Defs;
}

const FZoogiCardDef* UZoogiFrontProfileSubsystem::FindCard(FName Id)
{
	return CardDefs().FindByPredicate([Id](const FZoogiCardDef& C) { return C.Id == Id; });
}

TArray<FName> UZoogiFrontProfileSubsystem::StarterDeck()
{
	TArray<FName> Deck = ZoogiCards::BuiltInDeck();
	FString Text, Err;
	TMap<FString, TArray<FName>> Decks;
	if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/Decks.csv"))) && ZoogiCards::ParseDecksCsv(Text, Decks, Err))
		if (const TArray<FName>* S = Decks.Find(TEXT("starter"))) Deck = *S;
	return Deck;
}

int32 UZoogiFrontProfileSubsystem::RarityOf(const FZoogiCardDef& C)
{
	if (C.Element != EZoogiCardElement::Neutral) return 2;
	return C.Cost >= 2 ? 1 : 0;
}

const TCHAR* UZoogiFrontProfileSubsystem::RarityName(int32 R)
{
	return R == 2 ? TEXT("Epic") : R == 1 ? TEXT("Rare") : TEXT("Common");
}

bool UZoogiFrontProfileSubsystem::IsCardOwned(FName Id) const
{
	// Same rule as the Deck screen: a card is owned when the power gating it is unlocked (or nothing gates it).
	const FZoogiCardDef* C = FindCard(Id);
	if (!C) return false;
	FName P = C->RequiredPower;
	const UGameInstance* GI = GetGameInstance();
	if (P.IsNone())
		if (const UZoogiProgressionSubsystem* PS = GI ? GI->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr)
			for (const FZoogiPowerRow& R : PS->Core().Config().Powers) if (R.CardId == Id) { P = R.PowerId; break; }
	if (P.IsNone()) return true;
	const UZoogiPowerUnlockSubsystem* U = GI ? GI->GetSubsystem<UZoogiPowerUnlockSubsystem>() : nullptr;
	return !U || U->IsUnlocked(P);
}

FString UZoogiFrontProfileSubsystem::LockText(FName Id) const
{
	const FZoogiCardDef* C = FindCard(Id);
	FName P = C ? C->RequiredPower : NAME_None;
	const UGameInstance* GI = GetGameInstance();
	if (P.IsNone())
		if (const UZoogiProgressionSubsystem* PS = GI ? GI->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr)
			for (const FZoogiPowerRow& R : PS->Core().Config().Powers) if (R.CardId == Id) { P = R.PowerId; break; }
	const UZoogiPowerUnlockSubsystem* U = GI ? GI->GetSubsystem<UZoogiPowerUnlockSubsystem>() : nullptr;
	return U && !P.IsNone() ? U->RequirementText(P).ToString() : FString(TEXT("Unlock its power"));
}

int32 UZoogiFrontProfileSubsystem::Copies(FName Id) const
{
	const int32* N = D.CardCopies.Find(Id);
	return (IsCardOwned(Id) ? 1 : 0) + (N ? *N : 0);
}

int32 UZoogiFrontProfileSubsystem::CardLevel(int32 Copies, int32& OutInto, int32& OutNeed)
{
	static const int32 Cum[] = { 1, 3, 7, 17, 37, 87, 187, 387 };
	int32 L = 0;
	while (L + 1 < (int32)UE_ARRAY_COUNT(Cum) && Copies >= Cum[L + 1]) ++L;
	if (L + 1 >= (int32)UE_ARRAY_COUNT(Cum)) { OutInto = OutNeed = 1; return L + 1; }
	OutInto = Copies - Cum[L];
	OutNeed = Cum[L + 1] - Cum[L];
	return L + 1;
}

void UZoogiFrontProfileSubsystem::AddCopies(FName Id, int32 N)
{
	if (N > 0) D.CardCopies.FindOrAdd(Id) += N;
}

TArray<FName> UZoogiFrontProfileSubsystem::OwnedCards() const
{
	TArray<FName> Out;
	for (const FZoogiCardDef& C : CardDefs()) if (IsCardOwned(C.Id)) Out.Add(C.Id);
	return Out;
}

// ------------------------------------------------------------------------------------------------ decks
void UZoogiFrontProfileSubsystem::InitDecks()
{
	if (D.Decks.Num() >= 3) { D.Decks.SetNum(3); return; }
	const UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
	const TArray<FName> Starter = StarterDeck();
	// Deck 1 = the deck the existing Deck screen saved (what matches already use), else the starter deck.
	const TArray<FName> First = (PS && PS->SavedDeck().Num() > 0) ? PS->SavedDeck() : Starter;
	while (D.Decks.Num() < 3) D.Decks.Add(D.Decks.Num() == 0 ? First : Starter);
}

const TArray<FName>& UZoogiFrontProfileSubsystem::Deck(int32 I) const
{
	static const TArray<FName> Empty;
	return D.Decks.IsValidIndex(I) ? D.Decks[I] : Empty;
}

void UZoogiFrontProfileSubsystem::SetDeck(int32 I, const TArray<FName>& Cards)
{
	if (!D.Decks.IsValidIndex(I)) return;
	D.Decks[I] = Cards;
	Save();
	if (I == D.ActiveDeck)
		if (UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr)
			PS->SetSavedDeck(Cards); // the existing saved deck (matches top short decks up to 15 at start)
}

void UZoogiFrontProfileSubsystem::SetActiveDeck(int32 I)
{
	D.ActiveDeck = FMath::Clamp(I, 0, 2);
	SetDeck(D.ActiveDeck, Deck(D.ActiveDeck));
}

// ------------------------------------------------------------------------------------------------ daily deals
static int32 ZFResetHour(const UGameInstance* GI)
{
	const UZoogiProgressionSubsystem* PS = GI ? GI->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
	return PS ? PS->Core().Config().DailyResetHourLocal : 4;
}

FTimespan UZoogiFrontProfileSubsystem::TimeToDealsReset() const
{
	const FDateTime Now = FDateTime::Now();
	return ZoogiProgress::ResetBoundary(Now, ZFResetHour(GetGameInstance())) + FTimespan::FromDays(1) - Now;
}

void UZoogiFrontProfileSubsystem::EnsureDeals()
{
	const FString Day = ZoogiProgress::ResetBoundary(FDateTime::Now(), ZFResetHour(GetGameInstance())).ToString(TEXT("%Y%m%d"));
	const FString Key = Day + TEXT("v3"); // v3: DETAILED PASS 2 counts / prices (rolled once more on old saves)
	if (D.DealsDay == Key && D.Deals.Num() == 6) return;
	TArray<FName> Owned = OwnedCards();
	if (Owned.Num() == 0) return;
	FRandomStream R((int32)GetTypeHash(Day + D.GuestName));
	D.Deals.Reset();
	for (int32 i = 0; i < 6; ++i)
	{
		FZoogiFrontDeal Deal;
		// slot 0 is the daily free pick (a common); the rest avoid repeats while possible
		TArray<FName> Pool;
		for (const FName& Id : Owned)
		{
			const FZoogiCardDef* C = FindCard(Id);
			if (i == 0 && C && RarityOf(*C) != 1) continue; // the free pick is a Rare x4 (demo: Barbarian Rare x4 Free)
			if (!D.Deals.ContainsByPredicate([&](const FZoogiFrontDeal& X) { return X.CardId == Id; })) Pool.Add(Id);
		}
		if (Pool.Num() == 0) Pool = Owned;
		Deal.CardId = Pool[R.RandRange(0, Pool.Num() - 1)];
		const FZoogiCardDef* C = FindCard(Deal.CardId);
		const int32 Rar = C ? RarityOf(*C) : 0;
		// demo prices: Common 10 gold a copy (x16 160, x21 210, x30 300), Rare 100 a copy (x6 600, x3 300); Epic 500 a copy
		static const int32 PerCopy[] = { 10, 100, 500 };
		Deal.Count = Rar == 2 ? R.RandRange(1, 2) : Rar == 1 ? R.RandRange(3, 6) : R.RandRange(10, 30);
		Deal.Price = Deal.Count * PerCopy[Rar];
		if (i == 0) { Deal.bFree = true; Deal.Count = 4; Deal.Price = 0; }
		D.Deals.Add(Deal);
	}
	D.DealsDay = Key;
	Save();
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND daily deals rolled for %s"), *Day);
}

bool UZoogiFrontProfileSubsystem::BuyDeal(int32 I, FString& OutWhy)
{
	EnsureDeals();
	if (!D.Deals.IsValidIndex(I)) { OutWhy = TEXT("That deal is gone."); return false; }
	FZoogiFrontDeal& Deal = D.Deals[I];
	if (Deal.bBought) { OutWhy = TEXT("Already purchased today."); return false; }
	if (D.Gold < Deal.Price) { OutWhy = FString::Printf(TEXT("Not enough gold: %d needed."), Deal.Price); return false; }
	D.Gold -= Deal.Price;
	AddCopies(Deal.CardId, Deal.Count);
	Deal.bBought = true;
	Save();
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND deal %d bought: %s x%d for %d gold (gold now %d)"), I, *Deal.CardId.ToString(), Deal.Count, Deal.Price, D.Gold);
	return true;
}

// ------------------------------------------------------------------------------------------------ chests
const TArray<FZoogiFrontChestDef>& UZoogiFrontProfileSubsystem::ChestDefs()
{
	static const TArray<FZoogiFrontChestDef> Defs = {
		// DETAILED PASS 2 shop prices: Wooden 80, Royal 400, Epic 1000 gems
		{ FName(TEXT("wooden")), TEXT("Wooden Chest"), 80, 15 * 60, 6, 30, 60, 80, 18, 2 },
		{ FName(TEXT("royal")), TEXT("Royal Chest"), 400, 60 * 60, 15, 120, 200, 70, 25, 5 },
		{ FName(TEXT("epic")), TEXT("Epic Chest"), 1000, 4 * 60 * 60, 24, 300, 450, 50, 35, 15 },
	};
	return Defs;
}

const FZoogiFrontChestDef* UZoogiFrontProfileSubsystem::FindChest(FName Id)
{
	return ChestDefs().FindByPredicate([Id](const FZoogiFrontChestDef& C) { return C.Id == Id; });
}

int64 UZoogiFrontProfileSubsystem::NowUtc() { return FDateTime::UtcNow().ToUnixTimestamp(); }

int32 UZoogiFrontProfileSubsystem::ChestSeconds(FName Type) const
{
	const FZoogiFrontChestDef* C = FindChest(Type);
	const int32 S = C ? C->UnlockSec : 900;
	static const bool bFast = FParse::Param(FCommandLine::Get(), TEXT("ZoogiChestFast"));
	return bFast ? FMath::Max(5, S / 60) : S;
}

TArray<FZoogiFrontReward> UZoogiFrontProfileSubsystem::RollChest(FName Type, FRandomStream& R)
{
	TArray<FZoogiFrontReward> Out;
	const FZoogiFrontChestDef* C = FindChest(Type);
	if (!C) return Out;
	const int32 Gold = R.RandRange(C->GoldMin, C->GoldMax);
	D.Gold += Gold;
	Out.Add({ FName(TEXT("gold")), NAME_None, Gold });
	TArray<FName> ByR[3];
	for (const FName& Id : OwnedCards()) if (const FZoogiCardDef* Def = FindCard(Id)) ByR[RarityOf(*Def)].Add(Id);
	TMap<FName, int32> Got;
	for (int32 i = 0; i < C->Cards; ++i)
	{
		const int32 Roll = R.RandRange(1, C->WCommon + C->WRare + C->WEpic);
		int32 Rar = Roll <= C->WCommon ? 0 : Roll <= C->WCommon + C->WRare ? 1 : 2;
		while (Rar > 0 && ByR[Rar].Num() == 0) --Rar;
		if (ByR[Rar].Num() == 0) continue;
		// cards come in a few stacks, like a real chest: reuse an already-rolled card of that rarity 60% of the time
		TArray<FName> Same; for (const TPair<FName, int32>& P : Got) if (ByR[Rar].Contains(P.Key)) Same.Add(P.Key);
		const FName Id = (Same.Num() && R.FRand() < 0.6f) ? Same[R.RandRange(0, Same.Num() - 1)] : ByR[Rar][R.RandRange(0, ByR[Rar].Num() - 1)];
		Got.FindOrAdd(Id) += 1;
	}
	Got.ValueSort([](int32 A, int32 B) { return A > B; });
	for (const TPair<FName, int32>& P : Got) { AddCopies(P.Key, P.Value); Out.Add({ FName(TEXT("card")), P.Key, P.Value }); }
	return Out;
}

bool UZoogiFrontProfileSubsystem::BuyChest(FName Type, TArray<FZoogiFrontReward>& Out, FString& OutWhy)
{
	const FZoogiFrontChestDef* C = FindChest(Type);
	if (!C) { OutWhy = TEXT("Unknown chest."); return false; }
	if (ChestStockLeft(Type) == 0) { OutWhy = TEXT("Sold out today. More arrive at the daily reset."); return false; }
	if (Gems() < C->GemCost) { OutWhy = FString::Printf(TEXT("Not enough gems: %d needed."), C->GemCost); return false; }
	if (!SpendGems(C->GemCost, FName(*(TEXT("frontend.chest.") + Type.ToString())))) { OutWhy = TEXT("The gem payment failed."); return false; }
	const FString Day = ZoogiProgress::ResetBoundary(FDateTime::Now(), ZFResetHour(GetGameInstance())).ToString(TEXT("%Y%m%d"));
	if (D.ChestStockDay != Day) { D.ChestStockDay = Day; D.ChestBought.Reset(); }
	D.ChestBought.FindOrAdd(Type) += 1;
	FRandomStream R(FMath::Rand());
	Out = RollChest(Type, R);
	Save();
	return true;
}

int32 UZoogiFrontProfileSubsystem::ChestStockLeft(FName Type) const
{
	const int32 Limit = Type == FName(TEXT("royal")) ? 3 : Type == FName(TEXT("epic")) ? 1 : -1;
	if (Limit < 0) return -1;
	const FString Day = ZoogiProgress::ResetBoundary(FDateTime::Now(), ZFResetHour(GetGameInstance())).ToString(TEXT("%Y%m%d"));
	const int32* N = D.ChestStockDay == Day ? D.ChestBought.Find(Type) : nullptr;
	return FMath::Max(0, Limit - (N ? *N : 0));
}

bool UZoogiFrontProfileSubsystem::AnyUnlocking() const
{
	const int64 Now = NowUtc();
	for (const FZoogiFrontChestSlot& S : D.Chests) if (!S.Type.IsNone() && S.UnlockAtUtc > Now) return true;
	return false;
}

bool UZoogiFrontProfileSubsystem::StartUnlock(int32 Slot, FString& OutWhy)
{
	if (!D.Chests.IsValidIndex(Slot) || D.Chests[Slot].Type.IsNone()) { OutWhy = TEXT("Empty slot."); return false; }
	if (D.Chests[Slot].UnlockAtUtc > 0) { OutWhy = TEXT("Already unlocking."); return false; }
	if (AnyUnlocking()) { OutWhy = TEXT("Another chest is unlocking. One at a time!"); return false; }
	D.Chests[Slot].UnlockAtUtc = NowUtc() + ChestSeconds(D.Chests[Slot].Type);
	Save();
	return true;
}

int64 UZoogiFrontProfileSubsystem::SecondsLeft(int32 Slot) const
{
	if (!D.Chests.IsValidIndex(Slot) || D.Chests[Slot].Type.IsNone() || D.Chests[Slot].UnlockAtUtc <= 0) return -1;
	return FMath::Max<int64>(0, D.Chests[Slot].UnlockAtUtc - NowUtc());
}

bool UZoogiFrontProfileSubsystem::IsReady(int32 Slot) const { return SecondsLeft(Slot) == 0; }

int32 UZoogiFrontProfileSubsystem::OpenNowCost(int32 Slot) const
{
	if (!D.Chests.IsValidIndex(Slot) || D.Chests[Slot].Type.IsNone()) return 0;
	const int64 L = SecondsLeft(Slot);
	const int64 Secs = L < 0 ? ChestSeconds(D.Chests[Slot].Type) : L;
	return Secs <= 0 ? 0 : FMath::Max(1, (int32)FMath::DivideAndRoundUp<int64>(Secs, 600)); // 1 gem per 10 minutes left
}

bool UZoogiFrontProfileSubsystem::OpenChest(int32 Slot, bool bPayGems, TArray<FZoogiFrontReward>& Out, FString& OutWhy)
{
	if (!D.Chests.IsValidIndex(Slot) || D.Chests[Slot].Type.IsNone()) { OutWhy = TEXT("Empty slot."); return false; }
	if (!IsReady(Slot))
	{
		if (!bPayGems) { OutWhy = TEXT("Still locked."); return false; }
		const int32 Cost = OpenNowCost(Slot);
		if (Gems() < Cost) { OutWhy = FString::Printf(TEXT("Not enough gems: %d needed."), Cost); return false; }
		if (!SpendGems(Cost, FName(TEXT("frontend.chest.open_now")))) { OutWhy = TEXT("The gem payment failed."); return false; }
	}
	FRandomStream R(FMath::Rand());
	Out = RollChest(D.Chests[Slot].Type, R);
	D.Chests[Slot] = FZoogiFrontChestSlot();
	Save();
	return true;
}

// ------------------------------------------------------------------------------------------------ gold packs
const TArray<UZoogiFrontProfileSubsystem::FGoldPack>& UZoogiFrontProfileSubsystem::GoldPacks()
{
	static const TArray<FGoldPack> P = { { 1000, 50, TEXT("Stack of Gold"), TEXT("") }, { 10000, 500, TEXT("Sack of Gold"), TEXT("Popular") }, { 50000, 2250, TEXT("Cart of Gold"), TEXT("Best Value") } };
	return P;
}

TArray<UZoogiFrontProfileSubsystem::FGemPack> UZoogiFrontProfileSubsystem::GemPacks() const
{
	// single source: GemPackages.csv via the progression config (the same rows the store pipeline grants from)
	TArray<FGemPack> Out;
	const UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
	if (!PS) return Out;
	struct FLabel { const TCHAR* Id; const TCHAR* Name; const TCHAR* Tag; };
	static const FLabel Labels[] = {
		{ TEXT("gems_small"), TEXT("Handful of Gems"), TEXT("") }, { TEXT("gems_medium"), TEXT("Sack of Gems"), TEXT("Popular") },
		{ TEXT("gems_large"), TEXT("Box of Gems"), TEXT("") }, { TEXT("gems_huge"), TEXT("Chest of Gems"), TEXT("") },
		{ TEXT("gems_vault"), TEXT("Vault of Gems"), TEXT("") }, { TEXT("gems_treasure"), TEXT("Treasure of Gems"), TEXT("Best Value") } };
	for (const FZoogiGemPackageRow& R : PS->Core().Config().GemPackages)
	{
		FGemPack P;
		P.Id = R.PackageId; P.Gems = R.Gems + R.BonusGems; P.USD = R.PriceUSD;
		P.Name = FString::Printf(TEXT("%d Gems"), P.Gems);
		for (const FLabel& L : Labels) if (R.PackageId == FName(L.Id)) { P.Name = L.Name; P.Tag = L.Tag; }
		Out.Add(P);
	}
	return Out;
}

// ------------------------------------------------------------------------------------------------ profile
bool UZoogiFrontProfileSubsystem::ChangeNickname(const FString& NewName, FString& OutWhy)
{
	const FString N = NewName.TrimStartAndEnd();
	if (N.Len() < 3 || N.Len() > 16) { OutWhy = TEXT("Names are 3 to 16 characters."); return false; }
	for (TCHAR Ch : N) if (!(FChar::IsAlnum(Ch) || Ch == TEXT(' ') || Ch == TEXT('-') || Ch == TEXT('_'))) { OutWhy = TEXT("Use letters, numbers, spaces, - or _."); return false; }
	if (N == D.GuestName) { OutWhy = TEXT("That is already your name."); return false; }
	if (D.Gold < NicknameCost) { OutWhy = FString::Printf(TEXT("Not enough gold: changing your name costs %d."), NicknameCost); return false; }
	D.Gold -= NicknameCost;
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND nickname %s -> %s (-%d gold)"), *D.GuestName, *N, NicknameCost);
	D.GuestName = N;
	Save();
	return true;
}

int32 UZoogiFrontProfileSubsystem::CardsFound() const { return OwnedCards().Num(); }

void UZoogiFrontProfileSubsystem::CardLevelStats(float& OutAvg, int32& OutMax) const
{
	OutAvg = 0.f; OutMax = 0;
	const TArray<FName> Owned = OwnedCards();
	for (const FName& Id : Owned) { int32 I = 0, Nd = 1; const int32 L = CardLevel(Copies(Id), I, Nd); OutAvg += L; OutMax = FMath::Max(OutMax, L); }
	if (Owned.Num()) OutAvg /= Owned.Num();
}

// ------------------------------------------------------------------------------------------------ replays (files)
FString UZoogiFrontProfileSubsystem::ReplayDir() const
{
	return FPaths::ProjectSavedDir() / TEXT("ZoogiReplays") / FPaths::GetBaseFilename(FPaths::GetBaseFilename(ProfilePath())); // <profile>.frontend -> <profile>
}

bool UZoogiFrontProfileSubsystem::HasReplay(const FString& Id) const
{
	return !Id.IsEmpty() && IFileManager::Get().FileExists(*(ReplayDir() / Id));
}

bool UZoogiFrontProfileSubsystem::LoadReplay(const FString& Id, FZoogiReplay& Out) const
{
	TArray<uint8> File;
	if (Id.IsEmpty() || !FFileHelper::LoadFileToArray(File, *(ReplayDir() / Id)) || File.Num() < 12) return false;
	uint32 Magic = 0; int32 RawSize = 0;
	FMemory::Memcpy(&Magic, File.GetData(), 4); FMemory::Memcpy(&RawSize, File.GetData() + 4, 4);
	if (Magic != 0x5A52505A /* ZRPZ */ || RawSize <= 0 || RawSize > 64 * 1024 * 1024) return false;
	TArray<uint8> Raw; Raw.SetNumUninitialized(RawSize);
	if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawSize, File.GetData() + 8, File.Num() - 8)) return false;
	FMemoryReader Rd(Raw);
	int32 Version = 0, NZ = 0, NO = 0, NF = 0;
	Rd << Version;
	if (Version != 1) return false;
	Rd << Out.Arena << Out.Radius << Out.Center.X << Out.Center.Y << Out.Hz << NZ;
	if (NZ < 0 || NZ > 16) return false;
	for (int32 i = 0; i < NZ; ++i)
	{
		FString Nm; FLinearColor Cl; int32 Pl = 0;
		Rd << Nm << Cl.R << Cl.G << Cl.B << Cl.A << Pl;
		Out.Names.Add(Nm); Out.Colors.Add(Cl); Out.Players.Add(Pl);
	}
	Rd << NO << NF;
	if (NO < 0 || NO > 256 || NF < 0 || NF > 100000) return false;
	Out.NumOrbs = NO;
	Out.Times.SetNumUninitialized(NF); Out.Z.SetNumUninitialized(NF * NZ * 6); Out.ZFlags.SetNumUninitialized(NF * NZ);
	Out.O.SetNumUninitialized(NF * NO * 2); Out.OFlags.SetNumUninitialized(NF * NO);
	for (int32 f = 0; f < NF && !Rd.IsError(); ++f)
	{
		Rd << Out.Times[f];
		for (int32 i = 0; i < NZ; ++i) { for (int32 k = 0; k < 6; ++k) Rd << Out.Z[(f * NZ + i) * 6 + k]; Rd << Out.ZFlags[f * NZ + i]; }
		for (int32 i = 0; i < NO; ++i) { Rd << Out.O[(f * NO + i) * 2] << Out.O[(f * NO + i) * 2 + 1] << Out.OFlags[f * NO + i]; }
	}
	return !Rd.IsError();
}

bool UZoogiFrontProfileSubsystem::BuyGold(int32 I, FString& OutWhy)
{
	if (!GoldPacks().IsValidIndex(I)) return false;
	const FGoldPack& P = GoldPacks()[I];
	if (Gems() < P.Gems) { OutWhy = FString::Printf(TEXT("Not enough gems: %d needed."), P.Gems); return false; }
	if (!SpendGems(P.Gems, FName(TEXT("frontend.gold")))) { OutWhy = TEXT("The gem payment failed."); return false; }
	D.Gold += P.Gold;
	Save();
	return true;
}

// ------------------------------------------------------------------------------------------------ Trophy Road
const TArray<FZoogiFrontRoadReward>& UZoogiFrontProfileSubsystem::TrophyRoad()
{
	// DETAILED PASS 1 table, exact: trophies -> gold or chest
	static const TArray<FZoogiFrontRoadReward> T = []()
	{
		TArray<FZoogiFrontRoadReward> R;
		auto G = [&R](int32 Tr, int32 Gold) { FZoogiFrontRoadReward X; X.Trophies = Tr; X.Kind = FName(TEXT("gold")); X.Amount = Gold; R.Add(X); };
		auto C = [&R](int32 Tr, const TCHAR* Chest) { FZoogiFrontRoadReward X; X.Trophies = Tr; X.Kind = FName(TEXT("chest")); X.Chest = FName(Chest); X.Amount = 1; R.Add(X); };
		G(0, 100); G(50, 150); G(100, 200); C(200, TEXT("wooden")); G(300, 250); G(400, 300); C(500, TEXT("wooden")); G(600, 350); G(700, 400); G(800, 450);
		C(1000, TEXT("royal")); G(1100, 500); G(1200, 500); G(1300, 600); G(1400, 600); C(1500, TEXT("wooden")); G(1600, 700); G(1800, 800); C(2000, TEXT("royal"));
		G(2200, 800); G(2400, 1000); G(2600, 1000); G(2800, 1200); C(3000, TEXT("royal")); G(3200, 1500); G(3400, 1500); G(3600, 1800); G(3800, 1800);
		C(4000, TEXT("epic")); G(4200, 2000); G(4400, 2500); G(4600, 2500); G(4800, 3000); C(5000, TEXT("epic"));
		return R;
	}();
	return T;
}

bool UZoogiFrontProfileSubsystem::IsRoadClaimed(int32 I) const { return TrophyRoad().IsValidIndex(I) && D.TrophyClaimed.Contains(TrophyRoad()[I].Trophies); }
bool UZoogiFrontProfileSubsystem::IsRoadClaimable(int32 I) const
{
	// reached with the best trophy count (dropping below later does not take a milestone away)
	return TrophyRoad().IsValidIndex(I) && !IsRoadClaimed(I) && FMath::Max(D.Trophies, D.BestTrophies) >= TrophyRoad()[I].Trophies;
}
int32 UZoogiFrontProfileSubsystem::RoadClaimableCount() const { int32 N = 0; for (int32 i = 0; i < TrophyRoad().Num(); ++i) N += IsRoadClaimable(i) ? 1 : 0; return N; }

TArray<FZoogiFrontReward> UZoogiFrontProfileSubsystem::PayRoadReward(const FZoogiFrontRoadReward& R, int32 Salt)
{
	TArray<FZoogiFrontReward> Out;
	if (R.Kind == FName(TEXT("gold"))) { D.Gold += R.Amount; Out.Add({ FName(TEXT("gold")), NAME_None, R.Amount }); }
	else if (R.Kind == FName(TEXT("chest"))) { FRandomStream S(FMath::Rand()); Out = RollChest(R.Chest, S); }
	else if (R.Kind == FName(TEXT("hero"))) { const FName Id = HeroCardFor(Salt); if (!Id.IsNone()) { AddCopies(Id, R.Amount); Out.Add({ FName(TEXT("card")), Id, R.Amount }); } }
	return Out;
}

bool UZoogiFrontProfileSubsystem::ClaimRoad(int32 I, TArray<FZoogiFrontReward>& Out, FString& OutWhy)
{
	if (!IsRoadClaimable(I)) { OutWhy = IsRoadClaimed(I) ? TEXT("Already claimed.") : TEXT("Reach this many trophies first."); return false; }
	const FZoogiFrontRoadReward& R = TrophyRoad()[I];
	D.TrophyClaimed.Add(R.Trophies);
	Out = PayRoadReward(R, 1000 + R.Trophies);
	Save();
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND trophy road %d claimed (%s %d %s), gold now %d"), R.Trophies, *R.Kind.ToString(), R.Amount, *R.Chest.ToString(), D.Gold);
	return true;
}

// ------------------------------------------------------------------------------------------------ Battle Pass
const FZoogiFrontRoadReward& UZoogiFrontProfileSubsystem::PassReward(int32 Tier, bool bPremium)
{
	// DETAILED PASS 1 table, exact. Free: gold, or a Wooden chest every 5 tiers. Premium: gold, "hero xN" (N copies of
	// one of your cards: Zoogi Roll has no hero units, cards are the equivalent), Royal every 5 tiers, Epic at 35.
	static const TArray<FZoogiFrontRoadReward> Free = []()
	{
		const int32 G[] = { 60, 80, 60, 100, 0, 80, 120, 60, 100, 0, 120, 80, 150, 100, 0, 120, 80, 150, 100, 0, 150, 120, 150, 100, 0, 150, 120, 200, 150, 0, 200, 150, 200, 150, 0 };
		TArray<FZoogiFrontRoadReward> R;
		for (int32 i = 0; i < 35; ++i) { FZoogiFrontRoadReward X; if (G[i] > 0) { X.Kind = FName(TEXT("gold")); X.Amount = G[i]; } else { X.Kind = FName(TEXT("chest")); X.Chest = FName(TEXT("wooden")); X.Amount = 1; } R.Add(X); }
		return R;
	}();
	static const TArray<FZoogiFrontRoadReward> Prem = []()
	{
		// > 0 gold, < 0 hero x(-n), 0 chest
		const int32 G[] = { 100, -2, 150, -2, 0, 200, -3, 200, -4, 0, 250, -2, 300, -2, 0, 300, -2, 350, -3, 0, 350, -2, 400, -2, 0, 400, -2, 450, -3, 0, 450, -2, 500, -2, 0 };
		TArray<FZoogiFrontRoadReward> R;
		for (int32 i = 0; i < 35; ++i)
		{
			FZoogiFrontRoadReward X;
			if (G[i] > 0) { X.Kind = FName(TEXT("gold")); X.Amount = G[i]; }
			else if (G[i] < 0) { X.Kind = FName(TEXT("hero")); X.Amount = -G[i]; }
			else { X.Kind = FName(TEXT("chest")); X.Chest = FName(i == 34 ? TEXT("epic") : TEXT("royal")); X.Amount = 1; }
			R.Add(X);
		}
		return R;
	}();
	const TArray<FZoogiFrontRoadReward>& A = bPremium ? Prem : Free;
	return A[FMath::Clamp(Tier - 1, 0, 34)];
}

bool UZoogiFrontProfileSubsystem::IsPassClaimed(int32 Tier, bool bPremium) const { return (bPremium ? D.PassPremiumClaimed : D.PassFreeClaimed).Contains(Tier); }
bool UZoogiFrontProfileSubsystem::IsPassClaimable(int32 Tier, bool bPremium) const
{
	// tier N is earned once the pass level reaches N (tier 1 at the start, like the demo)
	return Tier >= 1 && Tier <= PassTiers && Tier <= D.BattlePassLevel && !IsPassClaimed(Tier, bPremium) && (!bPremium || D.bPassPremium);
}
int32 UZoogiFrontProfileSubsystem::PassClaimableCount() const
{
	int32 N = 0;
	for (int32 t = 1; t <= PassTiers; ++t) N += (IsPassClaimable(t, false) ? 1 : 0) + (IsPassClaimable(t, true) ? 1 : 0);
	return N;
}

bool UZoogiFrontProfileSubsystem::ClaimPass(int32 Tier, bool bPremium, TArray<FZoogiFrontReward>& Out, FString& OutWhy)
{
	if (!IsPassClaimable(Tier, bPremium))
	{
		OutWhy = IsPassClaimed(Tier, bPremium) ? TEXT("Already claimed.") : (bPremium && !D.bPassPremium) ? TEXT("Unlock the premium track first.") : TEXT("Play battles to earn pass points and reach this tier.");
		return false;
	}
	(bPremium ? D.PassPremiumClaimed : D.PassFreeClaimed).Add(Tier);
	Out = PayRoadReward(PassReward(Tier, bPremium), Tier * 7 + (bPremium ? 3 : 0));
	Save();
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND pass tier %d %s claimed, gold now %d"), Tier, bPremium ? TEXT("premium") : TEXT("free"), D.Gold);
	return true;
}

bool UZoogiFrontProfileSubsystem::UnlockPremium(FString& OutWhy)
{
	if (D.bPassPremium) { OutWhy = TEXT("The premium track is already unlocked."); return false; }
	if (Gems() < PassPremiumGems) { OutWhy = FString::Printf(TEXT("Not enough gems: the premium track costs %d.\nYou have %d."), PassPremiumGems, Gems()); return false; }
	if (!SpendGems(PassPremiumGems, FName(*FString::Printf(TEXT("frontend.battle_pass_premium.s%d"), PassSeason)))) { OutWhy = TEXT("The gem payment failed."); return false; }
	D.bPassPremium = true;
	Save();
	return true;
}

int64 UZoogiFrontProfileSubsystem::SeasonSecondsLeft() const { return FMath::Max<int64>(0, D.SeasonEndUtc - NowUtc()); }

FName UZoogiFrontProfileSubsystem::HeroCardFor(int32 Salt) const
{
	const TArray<FName> Owned = OwnedCards();
	if (Owned.Num() == 0) return NAME_None;
	return Owned[(uint32)HashCombine(GetTypeHash(D.GuestName), (uint32)Salt) % (uint32)Owned.Num()];
}

// ------------------------------------------------------------------------------------------------ match results
void UZoogiFrontProfileSubsystem::ApplyMatch(int32 Outcome, const FZoogiFrontBattle* Info)
{
	FZoogiFrontMatchResult M;
	M.bPending = true;
	M.Outcome = Outcome;
	FRandomStream R(FMath::Rand());
	const int32 Arena = ArenaForTrophies(D.Trophies);
	if (Outcome == 0) { M.Trophies = R.RandRange(28, 32); M.Gold = 20 + 5 * Arena + R.RandRange(0, 10); M.XP = 10; M.BP = 25; }
	else if (Outcome == 1) { M.Trophies = -FMath::Min(15, D.Trophies); M.Gold = 5; M.XP = 4; M.BP = 10; }
	else if (Outcome == 2) { M.Gold = 10; M.XP = 6; M.BP = 15; }
	else { M.Trophies = -FMath::Min(15, D.Trophies); } // left the match: counts as a loss, no other rewards
	D.Trophies = FMath::Max(0, D.Trophies + M.Trophies);
	D.BestTrophies = FMath::Max(D.BestTrophies, D.Trophies);
	D.Gold += M.Gold;
	D.XP += M.XP;
	while (D.XP >= XPForLevel(D.Level)) { D.XP -= XPForLevel(D.Level); ++D.Level; M.NewLevel = D.Level; }
	D.BattlePassXP += M.BP;
	while (D.BattlePassXP >= BattlePassXPPerLevel && D.BattlePassLevel < PassTiers) { D.BattlePassXP -= BattlePassXPPerLevel; ++D.BattlePassLevel; }
	if (D.BattlePassLevel >= PassTiers) D.BattlePassXP = FMath::Min(D.BattlePassXP, BattlePassXPPerLevel);
	++D.Battles;
	if (Outcome == 0) { ++D.WinStreak; D.BestStreak = FMath::Max(D.BestStreak, D.WinStreak); }
	else D.WinStreak = 0; // a loss, draw or leaving ends the streak
	if (Outcome == 1 || Outcome == 3) ++D.Losses;
	else if (Outcome == 2) ++D.Draws;
	// "Three Crown Win" (proposal: Zoogi Roll has points, not crowns) = a win where the opponent scored nothing
	if (Outcome == 0 && Info && !Info->bDev && Info->OppScore == 0 && Info->MyScore > 0) ++D.ThreeCrownWins; // a real win, opponent held to 0
	if (Info)
	{
		FZoogiFrontBattle B = *Info;
		B.Outcome = Outcome;
		B.Trophies = M.Trophies;
		if (B.Utc <= 0) B.Utc = NowUtc();
		D.BattleLog.Insert(B, 0);
		if (D.BattleLog.Num() > MaxBattleLog) D.BattleLog.SetNum(MaxBattleLog);
	}
	if (Outcome == 0)
	{
		++D.Wins;
		static const TCHAR* Cycle[] = { TEXT("wooden"), TEXT("wooden"), TEXT("wooden"), TEXT("royal"), TEXT("wooden"), TEXT("wooden"), TEXT("royal"), TEXT("epic") };
		const int32 Free = D.Chests.IndexOfByPredicate([](const FZoogiFrontChestSlot& S) { return S.Type.IsNone(); });
		if (Free != INDEX_NONE)
		{
			M.Chest = FName(Cycle[D.ChestCycle % UE_ARRAY_COUNT(Cycle)]);
			D.ChestCycle++;
			D.Chests[Free].Type = M.Chest;
			D.Chests[Free].UnlockAtUtc = 0;
		}
		else M.bSlotsFull = true;
	}
	D.LastMatch = M;
	Save();
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND match result %d: trophies %+d (%d), gold +%d, xp +%d (L%d %d/%d), bp +%d, chest %s%s"), Outcome, M.Trophies, D.Trophies, M.Gold, M.XP,
		D.Level, D.XP, XPForLevel(D.Level), M.BP, *M.Chest.ToString(), M.bSlotsFull ? TEXT(" (slots full)") : TEXT(""));
}

// ------------------------------------------------------------------------------------------------ reward bridge
bool UZoogiFrontRewardBridge::ShouldCreateSubsystem(UObject* Outer) const
{
	// Same guard as the progression bridge: automated test / capture runs never pay rewards.
	const TCHAR* C = FCommandLine::Get();
	FString V;
	return !(FParse::Param(C, TEXT("ZoogiAuto")) || FParse::Value(C, TEXT("ZoogiBattleTest="), V) || FParse::Value(C, TEXT("ZoogiEpisodeP5Test="), V) || GIsAutomationTesting);
}
bool UZoogiFrontRewardBridge::DoesSupportWorldType(const EWorldType::Type T) const { return T == EWorldType::Game || T == EWorldType::PIE; }

void UZoogiFrontRewardBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (AZoogiGameMode* G = InWorld.GetAuthGameMode<AZoogiGameMode>()) { GM = G; Handle = G->OnMatchEvent.AddUObject(this, &UZoogiFrontRewardBridge::OnMatchEvent); }
}

void UZoogiFrontRewardBridge::Deinitialize()
{
	StopRecording(false);
	if (AZoogiGameMode* G = GM.Get()) G->OnMatchEvent.Remove(Handle);
	Super::Deinitialize();
}

// ---- replay recorder: reads the existing actors only (positions / rotations / flags); never touches physics or rules
static constexpr float ZFRecHz = 10.f;
static constexpr int32 ZFRecMaxFrames = 12000; // 20 minutes at 10 Hz

void UZoogiFrontRewardBridge::StartRecording()
{
	AZoogiGameMode* G = GM.Get();
	if (!G) return;
	RecBalls.Reset(); RecOrbs.Reset(); RecFrames.Reset(); RecCount = 0; bRecTruncated = false;
	for (AZoogiBall* B : G->Balls) if (B) RecBalls.Add(B);
	for (AZoogiOrb* O : G->Orbs) if (O) RecOrbs.Add(O);
	RecFrames.Reserve(64 * 1024);
	G->GetWorldTimerManager().SetTimer(RecTimer, FTimerDelegate::CreateUObject(this, &UZoogiFrontRewardBridge::SampleFrame), 1.f / ZFRecHz, true, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND replay: recording %d zoogis, %d orbs at %.0f Hz"), RecBalls.Num(), RecOrbs.Num(), ZFRecHz);
}

void UZoogiFrontRewardBridge::SampleFrame()
{
	const UWorld* W = GetWorld();
	if (!W) return;
	if (RecCount >= ZFRecMaxFrames) { bRecTruncated = true; return; }
	FMemoryWriter Wr(RecFrames, false, true);
	Wr.Seek(RecFrames.Num());
	float T = (float)(W->GetTimeSeconds() - MatchT0);
	Wr << T;
	for (const TWeakObjectPtr<AZoogiBall>& WB : RecBalls)
	{
		const AZoogiBall* B = WB.Get();
		const FVector P = B ? B->GetActorLocation() : FVector::ZeroVector;
		const FRotator R = B ? B->GetActorRotation() : FRotator::ZeroRotator;
		float V[6] = { (float)P.X, (float)P.Y, (float)P.Z, (float)R.Pitch, (float)R.Yaw, (float)R.Roll };
		for (float& X : V) Wr << X;
		uint8 F = (!B || B->bOut || B->bKnockedOut) ? 1 : 0;
		Wr << F;
	}
	for (const TWeakObjectPtr<AZoogiOrb>& WO : RecOrbs)
	{
		const AZoogiOrb* O = WO.Get();
		const FVector P = O ? O->GetActorLocation() : FVector::ZeroVector;
		float X = (float)P.X, Y = (float)P.Y;
		uint8 F = (!O || O->bOut ? 1 : 0) | (O && O->bStar ? 2 : 0);
		Wr << X << Y << F;
	}
	++RecCount;
}

FString UZoogiFrontRewardBridge::StopRecording(bool bSave)
{
	AZoogiGameMode* G = GM.Get();
	if (G) G->GetWorldTimerManager().ClearTimer(RecTimer);
	if (!bSave || RecCount < 2 || !G) { RecFrames.Reset(); RecCount = 0; return FString(); }
	SampleFrame(); // the final positions
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return FString();
	TArray<uint8> Raw;
	FMemoryWriter Wr(Raw);
	int32 Version = 1;
	uint8 Arena = G->Arena ? (uint8)G->Arena->GetTheme() : (uint8)G->MenuArena;
	float Radius = G->Arena ? G->Arena->GetKnockoffRadius() : 1000.f;
	const FVector C = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
	float CX = (float)C.X, CY = (float)C.Y, Hz = ZFRecHz;
	int32 NZ = RecBalls.Num();
	Wr << Version << Arena << Radius << CX << CY << Hz << NZ;
	for (const TWeakObjectPtr<AZoogiBall>& WB : RecBalls)
	{
		const AZoogiBall* B = WB.Get();
		FString Nm = B ? B->Def.Name : FString(TEXT("?"));
		FLinearColor Cl = B ? B->Def.Color : FLinearColor::White;
		int32 Pl = B ? B->PlayerIndex : -1;
		Wr << Nm << Cl.R << Cl.G << Cl.B << Cl.A << Pl;
	}
	int32 NO = RecOrbs.Num(), NF = RecCount;
	Wr << NO << NF;
	Raw.Append(RecFrames);
	RecFrames.Reset(); RecCount = 0;
	int32 CompSize = FCompression::CompressMemoryBound(NAME_Zlib, Raw.Num());
	TArray<uint8> File; File.SetNumUninitialized(8 + CompSize);
	if (!FCompression::CompressMemory(NAME_Zlib, File.GetData() + 8, CompSize, Raw.GetData(), Raw.Num())) return FString();
	const uint32 Magic = 0x5A52505A; const int32 RawSize = Raw.Num();
	FMemory::Memcpy(File.GetData(), &Magic, 4); FMemory::Memcpy(File.GetData() + 4, &RawSize, 4);
	File.SetNum(8 + CompSize);
	const FString Dir = FP->ReplayDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Id = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".zrp");
	if (!FFileHelper::SaveArrayToFile(File, *(Dir / Id))) return FString();
	// keep the newest MaxReplays
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.zrp")), true, false);
	Files.Sort();
	for (int32 i = 0; i + UZoogiFrontProfileSubsystem::MaxReplays < Files.Num(); ++i) IFileManager::Get().Delete(*(Dir / Files[i]));
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND replay saved %s: %d frames, %d bytes (raw %d)%s"), *Id, NF, File.Num(), RawSize, bRecTruncated ? TEXT(", truncated at 20 min") : TEXT(""));
	return Id;
}

FZoogiFrontBattle UZoogiFrontRewardBridge::MakeBattle(int32 Outcome) const
{
	FZoogiFrontBattle B;
	const AZoogiGameMode* G = GM.Get();
	B.Outcome = Outcome;
	B.Utc = UZoogiFrontProfileSubsystem::NowUtc();
	if (!G) return B;
	const UWorld* W = GetWorld();
	B.Duration = W ? FMath::Max(0, FMath::RoundToInt((float)(W->GetTimeSeconds() - MatchT0))) : 0;
	B.Arena = G->Arena ? (uint8)G->Arena->GetTheme() : (uint8)G->MenuArena;
	const bool bTeams = G->bComboMode;
	const int32 Mine = bTeams ? AZoogiGameMode::TeamOfIndex(Player) : Player;
	TMap<int32, int32> SideScore;
	TArray<FString> Opp;
	for (const AZoogiBall* Ball : G->Balls)
	{
		if (!Ball) continue;
		const int32 Side = bTeams ? AZoogiGameMode::TeamOfIndex(Ball->PlayerIndex) : Ball->PlayerIndex;
		SideScore.FindOrAdd(Side) += Ball->Score;
		if (Side != Mine) Opp.Add(Ball->Def.Name.ToUpper() + (Ball->bCPU ? TEXT(" (CPU)") : TEXT("")));
	}
	B.MyScore = SideScore.FindRef(Mine);
	for (const TPair<int32, int32>& P : SideScore) if (P.Key != Mine) B.OppScore = FMath::Max(B.OppScore, P.Value);
	B.Opponent = Opp.Num() ? FString::Join(Opp, TEXT(" + ")) : FString(TEXT("?"));
	return B;
}

int32 UZoogiFrontRewardBridge::PlayerIndex() const
{
	const AZoogiGameMode* G = GM.Get();
	if (!G) return -1;
	for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) return B->PlayerIndex;
	// capture runs only: -ZoogiClashRewardsP1 scores a CPU-played P1 as "the player" (real match, real result)
	static const bool bP1 = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashRewardsP1"));
	return bP1 ? 0 : -1;
}

void UZoogiFrontRewardBridge::OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag)
{
	AZoogiGameMode* G = GM.Get();
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!G || !FP) return;
	if (E == EZoogiMatchEvent::MatchStart)
	{
		Player = PlayerIndex();
		bOpen = Player >= 0; // CPU-only demo: no rewards, no log, no replay
		if (UWorld* W = GetWorld()) MatchT0 = W->GetTimeSeconds();
		if (bOpen) StartRecording();
		return;
	}
	if (!bOpen) return;
	if (E == EZoogiMatchEvent::MatchOver)
	{
		bOpen = false;
		const int32 Mine = G->bComboMode ? AZoogiGameMode::TeamOfIndex(Player) : Player;
		int32 Outcome = Who < 0 ? 2 : (Who == Mine ? 0 : 1);
		bool bDev = false;
#if !UE_BUILD_SHIPPING
		// Dev test flag (never in Shipping, never on by default): the front-end rewards treat the match as a player win.
		// The match itself, its scores and the replay are untouched; the battle log row is marked DEV.
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevForceWin")) && Outcome != 0) { UE_LOG(LogZoogi, Log, TEXT("FRONTEND DEV force win (real outcome %d)"), Outcome); Outcome = 0; bDev = true; }
#endif
		FZoogiFrontBattle B = MakeBattle(Outcome);
		B.bDev = bDev;
		B.ReplayId = StopRecording(true);
		FP->ApplyMatch(Outcome, &B);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTourBattle")))
		{
			// capture runs: back to the menu after the results screen, like a player pressing MENU
			TWeakObjectPtr<AZoogiGameMode> WG = G;
			FTimerHandle H;
			G->GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(G, [WG]() { if (AZoogiGameMode* GG = WG.Get()) GG->MenuAction(TEXT("Menu"), 0); }), 6.f, false);
		}
	}
	else if (E == EZoogiMatchEvent::ReturnToMenu)
	{
		bOpen = false;
		FZoogiFrontBattle B = MakeBattle(3);
		B.ReplayId = StopRecording(true);
		FP->ApplyMatch(3, &B); // left mid-match
	}
}
