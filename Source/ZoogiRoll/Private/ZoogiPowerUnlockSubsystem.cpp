#include "ZoogiPowerUnlockSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiRoll.h"
#include "ZoogiCards.h"
#include "ZoogiProgression.h"
#include "ZoogiProgressionSubsystem.h"

const TCHAR* UZoogiPowerUnlockSubsystem::SlotName = TEXT("ZoogiUnlocks");

namespace
{
	// Presentation data (name, description, card art) for each power. Requirements and costs are only fallbacks here:
	// with UZoogiProgressionSubsystem present (always, in game) they are overwritten from Content/Data/Progression/Powers.csv.
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
			false, 550, "void_mastery", 5, TEXT("titan"));
		Add("shock_1_storm", TEXT("Storm"), "Shock", 1, TEXT("A crackling storm charge that stuns what it touches."), true, 0, NAME_None, 0, TEXT("storm"));
		Add("frost_2_frost", TEXT("Frost"), "Frost", 2, TEXT("Freezes the floor in a line; rivals slide."), true, 0, NAME_None, 0, TEXT("frost"));
		Add("fire_2_ember", TEXT("Ember"), "Fire", 2, TEXT("Leaves a burning trail behind your roll."), false, 400, "fire_mastery", 3, TEXT("ember"));
		Add("void_1_precision", TEXT("Precision"), "Void", 1, TEXT("Snaps your shot onto the closest target."), false, 300, "void_mastery", 2, TEXT("precision"));
		Add("void_2_crystal_collapse", TEXT("Crystal Collapse"), "Void", 2, TEXT("Shatters nearby crystal pieces into a shrapnel ring."), false, 500, "void_mastery", 4, TEXT("crystal_collapse"));
		return R;
	}

	FText StatLabel(FName Stat)
	{
		FString S = Stat.ToString().Replace(TEXT("_"), TEXT(" "));
		if (S.Len() > 0) S[0] = FChar::ToUpper(S[0]);
		return FText::FromString(S);
	}

	/** "void_mastery" -> Void. */
	bool MasteryStatElement(FName Stat, EZoogiElement& Out)
	{
		FString S = Stat.ToString();
		return S.RemoveFromEnd(TEXT("_mastery")) && ZoogiCards::ParseElement(S, Out) && Out != EZoogiElement::Neutral;
	}
}

UZoogiPowerUnlockSubsystem* UZoogiPowerUnlockSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZoogiPowerUnlockSubsystem>() : nullptr;
}

UZoogiProgressionSubsystem* UZoogiPowerUnlockSubsystem::Prog() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
}

void UZoogiPowerUnlockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UZoogiProgressionSubsystem>();
	Super::Initialize(Collection);
	Defs = BuildPowerDefs();
	bUnlockAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUnlockAll"));
	// ZoogiUnlocks now only keeps EquippedByZoogi (per-Zoogi equipped power). Gems, mastery and unlocks live in ZoogiProgression.json.
	Data = Cast<UZoogiUnlockSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Data) Data = Cast<UZoogiUnlockSave>(UGameplayStatics::CreateSaveGameObject(UZoogiUnlockSave::StaticClass()));

	UZoogiProgressionSubsystem* P = Prog();
	if (P)
	{
		// Requirements / costs from Powers.csv (retuned values); presentation stays in BuildPowerDefs.
		for (FZoogiPowerDef& D : Defs)
			if (const FZoogiPowerRow* Row = P->Core().Config().FindPower(D.PowerId))
			{
				D.bDefaultUnlocked = Row->bDefaultUnlocked;
				D.GemCost = Row->GemCost;
				D.RequirementValue = Row->MasteryLevel;
				D.RequirementStat = Row->MasteryLevel > 0 ? FName(*FString::Printf(TEXT("%s_mastery"), *FString(ZoogiCards::ElementName(Row->Element)).ToLower())) : NAME_None;
			}
		// One-time migration of unlocks bought in the old slot (never the placeholder 340 gems / void_mastery 3).
		if (Data->UnlockedPowers.Num() > 0)
		{
			const FDateTime Now = FDateTime::UtcNow();
			int32 Moved = 0;
			for (const FName& Id : Data->UnlockedPowers)
				if (!P->Core().IsPowerUnlocked(Id)) { P->Core().Mutable().Unlocks.Add({ Id, TEXT("migrated"), Now }); ++Moved; }
			Data->UnlockedPowers.Empty();
			Data->Stats.Empty();
			if (Moved > 0) P->SaveProfile();
			Save();
			UE_LOG(LogZoogi, Log, TEXT("UNLOCKS migrated %d unlock(s) from slot %s into the progression profile"), Moved, SlotName);
		}
		ProgHandle = P->OnChanged.AddWeakLambda(this, [this]() { OnUnlocksChanged.Broadcast(); });
	}
	else UE_LOG(LogZoogi, Warning, TEXT("UNLOCKS: no UZoogiProgressionSubsystem - everything but default powers reads as locked, 0 gems"));
	UE_LOG(LogZoogi, Log, TEXT("UNLOCKS loaded: %d equipped, gems=%d (progression), unlockAll=%d"), Data->EquippedByZoogi.Num(), GetGems(), bUnlockAll ? 1 : 0);
	// Deck building asks the unlock state through ZoogiCards (2acd98e). No card -> power links are set here (RequiredPower stays empty).
	TWeakObjectPtr<UZoogiPowerUnlockSubsystem> Weak(this);
	ZoogiCards::SetPowerUnlockQuery([Weak](FName PowerId) { const UZoogiPowerUnlockSubsystem* S = Weak.Get(); return !S || S->IsUnlocked(PowerId); });
}

void UZoogiPowerUnlockSubsystem::Deinitialize()
{
	ZoogiCards::SetPowerUnlockQuery(nullptr); // back to "no query = everything unlocked"
	if (UZoogiProgressionSubsystem* P = Prog()) P->OnChanged.Remove(ProgHandle);
	Super::Deinitialize();
}

const FZoogiPowerDef* UZoogiPowerUnlockSubsystem::FindPower(FName PowerId) const
{
	return Defs.FindByPredicate([PowerId](const FZoogiPowerDef& D) { return D.PowerId == PowerId; });
}

bool UZoogiPowerUnlockSubsystem::IsUnlocked(FName PowerId) const
{
	const FZoogiPowerDef* D = FindPower(PowerId);
	if (!D) return false;
	if (bUnlockAll || D->bDefaultUnlocked) return true;
	const UZoogiProgressionSubsystem* P = Prog();
	return P && P->Core().IsPowerUnlocked(PowerId);
}

int32 UZoogiPowerUnlockSubsystem::GetStat(FName Stat) const
{
	const UZoogiProgressionSubsystem* P = Prog();
	if (!P) return 0;
	EZoogiElement E;
	if (MasteryStatElement(Stat, E)) return P->Core().MasteryLevel(E);
	return P->Core().Stat(Stat);
}

int32 UZoogiPowerUnlockSubsystem::GetGems() const
{
	const UZoogiProgressionSubsystem* P = Prog();
	return P ? P->Core().Get().Wallet.Total() : 0;
}

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
	const UZoogiProgressionSubsystem* P = Prog();
	if (!P) { Why = FText::FromString(TEXT("Progression unavailable")); return false; }
	bool bByMastery = false;
	FString W;
	if (P->Core().CanUnlockPower(PowerId, bByMastery, W)) { Why = FText::GetEmpty(); return true; }
	Why = FText::FromString(FString::Printf(TEXT("Need: %s"), *RequirementText(PowerId).ToString()));
	return false;
}

bool UZoogiPowerUnlockSubsystem::Unlock(FName PowerId)
{
	FText Why;
	UZoogiProgressionSubsystem* P = Prog();
	if (!P || !CanUnlock(PowerId, Why)) return false;
	FString W;
	if (!P->UnlockPower(PowerId, W)) { UE_LOG(LogZoogi, Warning, TEXT("UNLOCKS %s refused by progression: %s"), *PowerId.ToString(), *W); return false; }
	const FZoogiUnlockRecord* Rec = P->Core().Get().Unlocks.FindByPredicate([PowerId](const FZoogiUnlockRecord& R) { return R.PowerId == PowerId; });
	UE_LOG(LogZoogi, Log, TEXT("UNLOCKS unlocked %s (%s) gems now %d"), *PowerId.ToString(), Rec ? *Rec->Method : TEXT("?"), GetGems());
	OnUnlocksChanged.Broadcast(); // SaveProfile already broadcast OnChanged too; menu rebuilds are idempotent
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
