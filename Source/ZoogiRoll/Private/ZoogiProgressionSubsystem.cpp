#include "ZoogiProgressionSubsystem.h"
#include "ZoogiCPUBrain.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UZoogiProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FZoogiProgressionConfig Cfg = FZoogiProgressionConfig::BuiltIn();
	FString Log, Err;
	Cfg.LoadFromDir(FPaths::ProjectContentDir() / TEXT("Data/Progression"), Log);
	if (!Cfg.Validate(Err)) { UE_LOG(LogZoogi, Warning, TEXT("PROGRESSION config invalid (%s): built-in defaults used"), *Err); Cfg = FZoogiProgressionConfig::BuiltIn(); }
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *ProfilePath()) && Prog.Load(Cfg, Json, Err))
		UE_LOG(LogZoogi, Log, TEXT("PROGRESSION profile loaded %s gems=%d (earned %d / purchased %d) data=%s [%s]"), *Prog.Get().ProfileId.ToString(), Prog.Get().Wallet.Total(), Prog.Get().Wallet.Earned(), Prog.Get().Wallet.Purchased(), *Cfg.Source, *Log);
	else
	{
		if (!Json.IsEmpty()) { UE_LOG(LogZoogi, Warning, TEXT("PROGRESSION profile rejected (%s): kept as .rejected, new profile"), *Err); IFileManager::Get().Move(*(ProfilePath() + TEXT(".rejected")), *ProfilePath()); }
		Prog.Init(Cfg);
		SaveProfile();
		UE_LOG(LogZoogi, Log, TEXT("PROGRESSION new profile %s data=%s [%s]"), *Prog.Get().ProfileId.ToString(), *Cfg.Source, *Log);
	}
	Prog.CheckDailyReset(FDateTime::UtcNow(), FDateTime::Now());
}

UZoogiProgressionSubsystem* UZoogiProgressionSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
}

FString UZoogiProgressionSubsystem::ProfilePath() const
{
	FString Name = TEXT("ZoogiProgression.json");
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiProfile="), Name);
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / Name;
}

bool UZoogiProgressionSubsystem::SaveProfile()
{
	const FString P = ProfilePath(), Tmp = P + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(Prog.Save(), *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
	if (IFileManager::Get().FileExists(*P)) IFileManager::Get().Copy(*(P + TEXT(".bak")), *P);
	const bool bOk = IFileManager::Get().Move(*P, *Tmp, true);
	OnChanged.Broadcast();
	return bOk;
}

void UZoogiProgressionSubsystem::BeginMatch(EZoogiCPUTier Tier, FName ZoogiId)
{
	OpenTally = FZoogiMatchTally(); OpenTally.MatchId = FGuid::NewGuid(); OpenTally.Tier = Tier; OpenTally.ZoogiId = ZoogiId;
	bMatchOpen = true;
	UE_LOG(LogZoogi, Log, TEXT("PROGRESSION match open %s tier=%s zoogi=%s"), *OpenTally.MatchId.ToString(), ZoogiProgress::TierName(Tier), *ZoogiId.ToString());
}

FZoogiMatchRewardResult UZoogiProgressionSubsystem::EndMatch(EZoogiMatchOutcome Outcome)
{
	if (!bMatchOpen) return Last;
	bMatchOpen = false;
	Last = Prog.ApplyMatch(OpenTally, Outcome, FDateTime::UtcNow(), FDateTime::Now()); // commit, then display (crash-safe)
	SaveProfile();
	UE_LOG(LogZoogi, Log, TEXT("PROGRESSION match end outcome=%d gems=%d (base %d x%.2f, cap clipped %d) today=%d remaining=%d xp V%d F%d S%d Fr%d zero=%s ach=%d"),
		(int32)Outcome, Last.GemsPaid, Last.BaseGems, Last.GemMult, Last.CapClipped, Last.EarnedToday, Last.RemainingToday, Last.XP[1], Last.XP[2], Last.XP[3], Last.XP[4], *Last.ZeroReason, Last.NewAchievements.Num());
	return Last;
}

bool UZoogiProgressionSubsystem::SetFocusElement(EZoogiCardElement E, EZoogiPlayContext Ctx, FString& OutWhy)
{
	const bool b = Prog.SetFocusElement(E, Ctx, FDateTime::UtcNow(), OutWhy);
	if (b) SaveProfile();
	return b;
}

bool UZoogiProgressionSubsystem::UnlockPower(FName PowerId, FString& OutWhy)
{
	const bool b = Prog.UnlockPower(PowerId, FDateTime::UtcNow(), OutWhy);
	if (b) SaveProfile();
	return b;
}

// ------------------------------------------------------------------ match bridge (read-only listener)

bool UZoogiProgressionMatchBridge::ShouldCreateSubsystem(UObject* Outer) const
{
	// Test / capture runs never touch the player's profile.
	const TCHAR* C = FCommandLine::Get();
	FString V;
	if (FParse::Value(C, TEXT("ZoogiProgression="), V) && V.Equals(TEXT("Off"), ESearchCase::IgnoreCase)) return false;
	return !(FParse::Param(C, TEXT("ZoogiAuto")) || FParse::Value(C, TEXT("ZoogiBattleTest="), V) || FParse::Value(C, TEXT("ZoogiEpisodeP5Test="), V) || GIsAutomationTesting);
}
bool UZoogiProgressionMatchBridge::DoesSupportWorldType(const EWorldType::Type T) const { return T == EWorldType::Game || T == EWorldType::PIE; }

void UZoogiProgressionMatchBridge::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (AZoogiGameMode* G = InWorld.GetAuthGameMode<AZoogiGameMode>()) { GM = G; Handle = G->OnMatchEvent.AddUObject(this, &UZoogiProgressionMatchBridge::OnMatchEvent); }
	CardHandle = ZoogiProgressHooks::CardEffectUsed().AddUObject(this, &UZoogiProgressionMatchBridge::OnCardEffect);
	PowerHandle = ZoogiProgressHooks::PowerUsed().AddUObject(this, &UZoogiProgressionMatchBridge::OnPowerUsed);
}
void UZoogiProgressionMatchBridge::Deinitialize()
{
	if (AZoogiGameMode* G = GM.Get()) G->OnMatchEvent.Remove(Handle);
	ZoogiProgressHooks::CardEffectUsed().Remove(CardHandle);
	ZoogiProgressHooks::PowerUsed().Remove(PowerHandle);
	Super::Deinitialize();
}

int32 UZoogiProgressionMatchBridge::HumanIndex() const
{
	const AZoogiGameMode* G = GM.Get();
	if (!G) return -1;
	for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) return B->PlayerIndex;
	return -1;
}
int32 UZoogiProgressionMatchBridge::TeamOf(int32 P) const { const AZoogiGameMode* G = GM.Get(); return (G && G->bComboMode) ? AZoogiGameMode::TeamOfIndex(P) : P; }

void UZoogiProgressionMatchBridge::OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag)
{
	UZoogiProgressionSubsystem* S = UZoogiProgressionSubsystem::Get(this);
	AZoogiGameMode* G = GM.Get();
	const int32 H = HumanIndex();
	if (!S || !G || H < 0) return; // CPU-only demo: no profile changes
	FZoogiProgression& P = S->Core();
	switch (E)
	{
	case EZoogiMatchEvent::MatchStart:
	{
		FName Zid; for (const AZoogiBall* B : G->Balls) if (B && B->PlayerIndex == H) Zid = B->Def.Id;
		S->BeginMatch(ZoogiCPU::ActiveTier(), Zid);
		break;
	}
	case EZoogiMatchEvent::KnockedOut:
		// Who = team of the Zoogi that fell. Credit only enemy falls (no attribution data yet: element Neutral -> Focus).
		if (S->HasOpenMatch() && Who != TeamOf(H)) P.HookKO(S->Tally(), EZoogiCardElement::Neutral);
		break;
	case EZoogiMatchEvent::RingOut:
		if (S->HasOpenMatch() && Who != TeamOf(H)) P.HookRingOut(S->Tally(), EZoogiCardElement::Neutral);
		break;
	case EZoogiMatchEvent::GaugeFull:
		if (S->HasOpenMatch() && Who == TeamOf(H)) P.HookEpisodeTriggered(S->Tally());
		break;
	case EZoogiMatchEvent::OrbOut:
		if (S->HasOpenMatch() && Who == TeamOf(H)) P.HookOrbCollected(S->Tally()); // 2v2: Who = team that scored the orb
		break;
	case EZoogiMatchEvent::MatchOver:
	{
		if (!S->HasOpenMatch()) break;
		const int32 Mine = G->bComboMode ? AZoogiGameMode::TeamOfIndex(H) : H;
		S->EndMatch(Who < 0 ? EZoogiMatchOutcome::Draw : (Who == Mine ? EZoogiMatchOutcome::Win : EZoogiMatchOutcome::Loss));
		break;
	}
	case EZoogiMatchEvent::ReturnToMenu:
		if (S->HasOpenMatch()) S->EndMatch(EZoogiMatchOutcome::Forfeit); // left mid-match: zero rewards
		break;
	default: break;
	}
}


// Card effects that really happened (UZoogiCardSubsystem::FxUse) and classic power charges spent (ZoogiPowers.cpp).
void UZoogiProgressionMatchBridge::OnCardEffect(int32 Player, FName CardId)
{
	UZoogiProgressionSubsystem* S = UZoogiProgressionSubsystem::Get(this);
	if (!S || !S->HasOpenMatch() || Player != HumanIndex()) return;
	static const TMap<FName, FName> Tricks = { { "bank_strike", "Trick.BankStrike" }, { "momentum_steal", "Trick.MomentumSteal" }, { "double_impact", "Trick.DoubleImpact" },
		{ "orb_magnet", "Trick.OrbMagnet" }, { "frost", "Trick.FrostLane" }, { "crystal_collapse", "Trick.CrystalCollapse" } };
	static const TArray<FZoogiCardDef> Defs = ZoogiCards::BuiltInDefs();
	const FZoogiCardDef* D = Defs.FindByPredicate([&](const FZoogiCardDef& X) { return X.Id == CardId; });
	const EZoogiCardElement E = D ? D->Element : EZoogiCardElement::Neutral;
	if (const FName* Tag = Tricks.Find(CardId)) S->Core().HookTrickShot(S->Tally(), *Tag, E);
	else if (E != EZoogiCardElement::Neutral) S->Core().HookPowerUsed(S->Tally(), E);
}
void UZoogiProgressionMatchBridge::OnPowerUsed(int32 Player)
{
	UZoogiProgressionSubsystem* S = UZoogiProgressionSubsystem::Get(this);
	if (S && S->HasOpenMatch() && Player == HumanIndex()) S->Core().HookPowerUsed(S->Tally(), EZoogiCardElement::Neutral); // classic powers carry no element tag: Focus
}