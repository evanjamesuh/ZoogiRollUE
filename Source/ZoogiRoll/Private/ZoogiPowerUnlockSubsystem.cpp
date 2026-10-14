#include "ZoogiPowerUnlockSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiRoll.h"

const TCHAR* UZoogiPowerUnlockSubsystem::SlotName = TEXT("ZoogiUnlocks");

namespace
{
	// THE ONE PLACE for power data, unlock requirements and costs. All values are placeholders (Oct 14 mockups:
	// "Void mastery 5 or 300 gems") until the progression / economy design lands.
	TArray<FZoogiPowerDef> BuildPowerDefs()
	{
		TArray<FZoogiPowerDef> R;
		auto Add = [&R](FName Id, const TCHAR* Name, FName Type, int32 Tier, const TCHAR* Desc, bool bFree, int32 Gems, FName Stat, int32 StatVal, const TCHAR* Art)
		{
			FZoogiPowerDef D;
			D.PowerId = Id; D.Name = FText::FromString(Name); D.Type = Type; D.Tier = Tier; D.Desc = FText::FromString(Desc);
			D.bDefaultUnlocked = bFree; D.GemCost = Gems; D.RequirementStat = Stat; D.RequirementValue = StatVal; D.CardArt = Art;
			R.Add(D);
		};
		Add(ZoogiPowerIds::Void2TitanMeteor, TEXT("Titan Meteor"), "Void", 2,
			TEXT("Calls a giant void meteor down on your shot's real impact point. Bigger knockback ring, then a gravity pull."),
			false, 300, "void_mastery", 5, TEXT("titan"));
		Add("shock_1_storm", TEXT("Storm"), "Shock", 1, TEXT("A crackling storm charge that stuns what it touches."), true, 0, NAME_None, 0, TEXT("storm"));
		Add("frost_2_frost", TEXT("Frost"), "Frost", 2, TEXT("Freezes the floor in a line; rivals slide."), true, 0, NAME_None, 0, TEXT("frost"));
		Add("fire_2_ember", TEXT("Ember"), "Fire", 2, TEXT("Leaves a burning trail behind your roll."), false, 200, "fire_mastery", 4, TEXT("ember"));
		Add("void_1_precision", TEXT("Precision"), "Void", 1, TEXT("Snaps your shot onto the closest target."), false, 150, "void_mastery", 2, TEXT("precision"));
		Add("void_2_crystal_collapse", TEXT("Crystal Collapse"), "Void", 2, TEXT("Shatters nearby crystal pieces into a shrapnel ring."), false, 250, "void_mastery", 4, TEXT("crystal_collapse"));
		return R;
	}

	FText StatLabel(FName Stat)
	{
		FString S = Stat.ToString().Replace(TEXT("_"), TEXT(" "));
		if (S.Len() > 0) S[0] = FChar::ToUpper(S[0]);
		return FText::FromString(S);
	}
}

UZoogiPowerUnlockSubsystem* UZoogiPowerUnlockSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZoogiPowerUnlockSubsystem>() : nullptr;
}

void UZoogiPowerUnlockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Defs = BuildPowerDefs();
	bUnlockAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUnlockAll"));
	Data = Cast<UZoogiUnlockSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Data)
	{
		Data = Cast<UZoogiUnlockSave>(UGameplayStatics::CreateSaveGameObject(UZoogiUnlockSave::StaticClass()));
		Data->Stats.Add("void_mastery", 3); // placeholder progress shown in the mockup (3 / 5)
	}
	UE_LOG(LogZoogi, Log, TEXT("UNLOCKS loaded: %d unlocked, %d equipped, gems=%d, unlockAll=%d"),
		Data->UnlockedPowers.Num(), Data->EquippedByZoogi.Num(), Data->Gems, bUnlockAll ? 1 : 0);
}

const FZoogiPowerDef* UZoogiPowerUnlockSubsystem::FindPower(FName PowerId) const
{
	return Defs.FindByPredicate([PowerId](const FZoogiPowerDef& D) { return D.PowerId == PowerId; });
}

bool UZoogiPowerUnlockSubsystem::IsUnlocked(FName PowerId) const
{
	const FZoogiPowerDef* D = FindPower(PowerId);
	if (!D) return false;
	return bUnlockAll || D->bDefaultUnlocked || (Data && Data->UnlockedPowers.Contains(PowerId));
}

int32 UZoogiPowerUnlockSubsystem::GetStat(FName Stat) const
{
	const int32* V = Data ? Data->Stats.Find(Stat) : nullptr;
	return V ? *V : 0;
}

int32 UZoogiPowerUnlockSubsystem::GetGems() const { return Data ? Data->Gems : 0; }

FText UZoogiPowerUnlockSubsystem::RequirementText(FName PowerId) const
{
	const FZoogiPowerDef* D = FindPower(PowerId);
	if (!D || D->bDefaultUnlocked) return FText::GetEmpty();
	TArray<FString> Parts;
	if (!D->RequirementStat.IsNone()) Parts.Add(FString::Printf(TEXT("%s %d"), *StatLabel(D->RequirementStat).ToString(), D->RequirementValue));
	if (D->GemCost > 0) Parts.Add(FString::Printf(TEXT("%d gems"), D->GemCost));
	return FText::FromString(FString::Join(Parts, TEXT(" or ")));
}

bool UZoogiPowerUnlockSubsystem::CanUnlock(FName PowerId, FText& Why) const
{
	const FZoogiPowerDef* D = FindPower(PowerId);
	if (!D) { Why = FText::FromString(TEXT("Unknown power")); return false; }
	if (IsUnlocked(PowerId)) { Why = FText::FromString(TEXT("Already unlocked")); return false; }
	if (!D->RequirementStat.IsNone() && GetStat(D->RequirementStat) >= D->RequirementValue) { Why = FText::GetEmpty(); return true; }
	if (D->GemCost > 0 && GetGems() >= D->GemCost) { Why = FText::GetEmpty(); return true; }
	Why = FText::FromString(FString::Printf(TEXT("Need: %s"), *RequirementText(PowerId).ToString()));
	return false;
}

bool UZoogiPowerUnlockSubsystem::Unlock(FName PowerId)
{
	FText Why;
	if (!Data || !CanUnlock(PowerId, Why)) return false;
	const FZoogiPowerDef* D = FindPower(PowerId);
	const bool bByStat = !D->RequirementStat.IsNone() && GetStat(D->RequirementStat) >= D->RequirementValue;
	if (!bByStat) Data->Gems -= D->GemCost;
	Data->UnlockedPowers.Add(PowerId);
	UE_LOG(LogZoogi, Log, TEXT("UNLOCKS unlocked %s (%s)"), *PowerId.ToString(), bByStat ? TEXT("requirement") : TEXT("gems"));
	Save();
	OnUnlocksChanged.Broadcast();
	return true;
}

FName UZoogiPowerUnlockSubsystem::GetEquipped(FName ZoogiId) const
{
	const FName* P = Data ? Data->EquippedByZoogi.Find(ZoogiId) : nullptr;
	return (P && IsUnlocked(*P)) ? *P : NAME_None;
}

bool UZoogiPowerUnlockSubsystem::SetEquipped(FName ZoogiId, FName PowerId)
{
	if (!Data || ZoogiId.IsNone()) return false;
	if (PowerId.IsNone()) Data->EquippedByZoogi.Remove(ZoogiId);
	else
	{
		if (!IsUnlocked(PowerId)) return false;
		Data->EquippedByZoogi.Add(ZoogiId, PowerId);
	}
	UE_LOG(LogZoogi, Log, TEXT("UNLOCKS equip %s -> %s"), *ZoogiId.ToString(), *PowerId.ToString());
	Save();
	OnUnlocksChanged.Broadcast();
	return true;
}

void UZoogiPowerUnlockSubsystem::Save()
{
	if (Data) UGameplayStatics::AsyncSaveGameToSlot(Data, SlotName, 0);
}
