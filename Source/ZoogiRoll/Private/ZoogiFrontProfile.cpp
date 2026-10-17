// clash-menu: see ZoogiFrontProfile.h
#include "ZoogiFrontProfile.h"
#include "ZoogiRoll.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FZoogiFrontProfileData::ToJsonString() const
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("schema"), SchemaVersion);
	J->SetStringField(TEXT("guest_name"), GuestName);
	J->SetNumberField(TEXT("level"), Level);
	J->SetNumberField(TEXT("xp"), XP);
	J->SetNumberField(TEXT("gold"), Gold);
	J->SetNumberField(TEXT("gems_local"), Gems);
	J->SetNumberField(TEXT("trophies"), Trophies);
	J->SetNumberField(TEXT("best_trophies"), BestTrophies);
	J->SetNumberField(TEXT("battle_pass_level"), BattlePassLevel);
	J->SetNumberField(TEXT("battle_pass_xp"), BattlePassXP);
	J->SetNumberField(TEXT("wins"), Wins);
	J->SetNumberField(TEXT("battles"), Battles);
	J->SetBoolField(TEXT("cards_new"), bCardsNew);
	TArray<TSharedPtr<FJsonValue>> C;
	for (const FName& N : Chests) C.Add(MakeShared<FJsonValueString>(N.IsNone() ? FString() : N.ToString()));
	J->SetArrayField(TEXT("chests"), C);
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
	J->TryGetNumberField(TEXT("level"), N.Level);
	J->TryGetNumberField(TEXT("xp"), N.XP);
	J->TryGetNumberField(TEXT("gold"), N.Gold);
	J->TryGetNumberField(TEXT("gems_local"), N.Gems);
	J->TryGetNumberField(TEXT("trophies"), N.Trophies);
	J->TryGetNumberField(TEXT("best_trophies"), N.BestTrophies);
	J->TryGetNumberField(TEXT("battle_pass_level"), N.BattlePassLevel);
	J->TryGetNumberField(TEXT("battle_pass_xp"), N.BattlePassXP);
	J->TryGetNumberField(TEXT("wins"), N.Wins);
	J->TryGetNumberField(TEXT("battles"), N.Battles);
	J->TryGetBoolField(TEXT("cards_new"), N.bCardsNew);
	const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
	if (J->TryGetArrayField(TEXT("chests"), C) && C)
		for (const TSharedPtr<FJsonValue>& V : *C) { const FString Str = V.IsValid() ? V->AsString() : FString(); N.Chests.Add(Str.IsEmpty() ? NAME_None : FName(*Str)); }
	N.Chests.SetNum(3);
	N.Level = FMath::Max(1, N.Level);
	N.BattlePassLevel = FMath::Max(1, N.BattlePassLevel);
	N.XP = FMath::Max(0, N.XP); N.Gold = FMath::Max(0, N.Gold); N.Gems = FMath::Max(0, N.Gems); N.Trophies = FMath::Max(0, N.Trophies);
	*this = N;
	return true;
}

void UZoogiFrontProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Json, Err;
	if (FFileHelper::LoadFileToString(Json, *ProfilePath()) && D.FromJsonString(Json, Err) && !D.GuestName.IsEmpty())
	{
		UE_LOG(LogZoogi, Log, TEXT("FRONTEND profile loaded %s: %s L%d %d/%d xp gold=%d gems(local)=%d trophies=%d"), *ProfilePath(), *D.GuestName, D.Level, D.XP, XPForLevel(D.Level), D.Gold, D.Gems, D.Trophies);
		return;
	}
	if (!Json.IsEmpty()) { UE_LOG(LogZoogi, Warning, TEXT("FRONTEND profile rejected (%s): kept as .rejected, new profile"), *Err); IFileManager::Get().Move(*(ProfilePath() + TEXT(".rejected")), *ProfilePath()); }
	D = FZoogiFrontProfileData();
	D.Chests.SetNum(3);
	D.GuestName = FString::Printf(TEXT("PLAYER-%06d"), FMath::RandRange(0, 999999)); // generated once, then saved
	Save();
	UE_LOG(LogZoogi, Log, TEXT("FRONTEND new profile %s (%s)"), *D.GuestName, *ProfilePath());
}

UZoogiFrontProfileSubsystem* UZoogiFrontProfileSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZoogiFrontProfileSubsystem>() : nullptr;
}

FString UZoogiFrontProfileSubsystem::ProfilePath() const
{
	// Same folder + name as the progression profile (honours -ZoogiProfile=<name>), with a .frontend.json suffix.
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
