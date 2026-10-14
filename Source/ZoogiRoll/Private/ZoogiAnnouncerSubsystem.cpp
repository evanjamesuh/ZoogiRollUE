// Tournament announcer VO (Oct 7, branch `music`). Lines: Scripts/music/vo_lines (generated on the build box).
#include "ZoogiAnnouncerSubsystem.h"
#include "ZoogiMusicSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static constexpr float GVoVolume = 0.9f;
static constexpr float GVoDuck = 0.5f; // music -6 dB under the announcer

// Priority (higher interrupts lower) and per-line cooldown (s).
namespace ZoogiVO
{
	enum : int32 { PLow = 1, PInfo = 3, PPlay = 5, PBig = 7, PHuge = 9, PEnd = 10 };
}

bool UZoogiAnnouncerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UZoogiAnnouncerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVO"));
	Rng.Initialize((int32)(FPlatformTime::Cycles() & 0x7fffffff));
	EnsureBound();
}

void UZoogiAnnouncerSubsystem::Deinitialize()
{
	if (AZoogiGameMode* G = BoundGM.Get()) G->OnMatchEvent.Remove(EventHandle);
	Current = nullptr;
	Super::Deinitialize();
}

TStatId UZoogiAnnouncerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiAnnouncerSubsystem, STATGROUP_Tickables);
}

void UZoogiAnnouncerSubsystem::EnsureBound()
{
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || (bBound && BoundGM.Get() == G)) return;
	EventHandle = G->OnMatchEvent.AddUObject(this, &UZoogiAnnouncerSubsystem::OnMatchEvent);
	BoundGM = G;
	bBound = true;
}

FZoogiVoLineSet* UZoogiAnnouncerSubsystem::Lines(const FString& Line)
{
	if (FZoogiVoLineSet* S = Sets.Find(Line)) return S;
	FZoogiVoLineSet& S = Sets.Add(Line);
	for (int32 i = 1; i <= 6; ++i)
	{
		const FString N = FString::Printf(TEXT("S_VO_%s_%d"), *Line, i);
		USoundBase* W = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/VO/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!W) break;
		S.Variants.Add(W);
		Keep.Add(W);
	}
	if (S.Variants.Num() == 0) UE_LOG(LogZoogi, Warning, TEXT("VO: no lines for %s (/Game/Audio/VO/S_VO_%s_1)"), *Line, *Line);
	return &S;
}

bool UZoogiAnnouncerSubsystem::Say(const FString& Line, int32 Priority, float Cooldown, float Delay)
{
	if (bDisabled) return false;
	if (Delay > 0.f) { Pending.Add({ Line, Priority, Cooldown, FPlatformTime::Seconds() + Delay, false }); return false; }
	return PlayNow(Line, Priority, Cooldown);
}

bool UZoogiAnnouncerSubsystem::PlayNow(const FString& Line, int32 Priority, float Cooldown, bool bRetry)
{
	FZoogiVoLineSet* S = Lines(Line);
	if (!S || S->Variants.Num() == 0) return false;
	const double Now = FPlatformTime::Seconds();
	if (Now < S->ReadyAt) { UE_LOG(LogZoogi, Verbose, TEXT("VO skip %s (cooldown)"), *Line); return false; }
	const bool bBusy = Now < BusyUntil;
	if (bBusy && Priority <= CurrentPriority)
	{
		// Important lines wait for the current one to finish (once, if it ends soon); minor ones are dropped.
		if (!bRetry && Priority >= ZoogiVO::PPlay && BusyUntil - Now < 2.5)
		{
			Pending.Add({ Line, Priority, Cooldown, BusyUntil + 0.15, true });
			UE_LOG(LogZoogi, Log, TEXT("VO queue %s (p%d) after %s"), *Line, Priority, *CurrentLine);
			return false;
		}
		UE_LOG(LogZoogi, Log, TEXT("VO skip %s (p%d): %s (p%d) is still talking"), *Line, Priority, *CurrentLine, CurrentPriority);
		return false;
	}
	if (!bBusy && Priority < ZoogiVO::PPlay && Now < QuietUntil) return false; // small breath between minor lines
	// random variant, never the same one twice in a row
	int32 Idx = 0;
	if (S->Variants.Num() > 1)
	{
		Idx = Rng.RandRange(0, S->Variants.Num() - 2);
		if (Idx >= S->Last && S->Last >= 0) ++Idx;
	}
	S->Last = Idx;
	USoundBase* W = S->Variants[Idx];
	if (bBusy && Current) { Current->FadeOut(0.08f, 0.f); }
	Current = UGameplayStatics::CreateSound2D(this, W, GVoVolume, 1.f, 0.f, nullptr, false, true);
	if (Current) { Current->bIsUISound = true; Current->Play(); }
	const float Dur = FMath::Max(0.3f, W->GetDuration() < 1000.f ? W->GetDuration() : 2.f);
	if (UZoogiMusicSubsystem* M = UZoogiMusicSubsystem::Get(this)) M->DuckFor(Dur + 0.15f, GVoDuck);
	UE_LOG(LogZoogi, Log, TEXT("VO play %s #%d (p%d, %.2f s)%s%s"), *Line, Idx + 1, Priority, Dur, bBusy ? *FString::Printf(TEXT(" interrupts %s"), *CurrentLine) : TEXT(""),
		Current ? TEXT("") : TEXT(" [silent: no audio device]"));
	CurrentLine = Line;
	CurrentPriority = Priority;
	BusyUntil = Now + Dur;
	QuietUntil = BusyUntil + 0.6;
	S->ReadyAt = Now + Cooldown;
	return true;
}

void UZoogiAnnouncerSubsystem::Tick(float DeltaTime)
{
	EnsureBound();
	if (Pending.Num() == 0) return;
	const double Now = FPlatformTime::Seconds();
	for (int32 i = Pending.Num() - 1; i >= 0; --i)
		if (Now >= Pending[i].At)
		{
			const FZoogiVoPending P = Pending[i];
			Pending.RemoveAt(i);
			PlayNow(P.Line, P.Priority, P.Cooldown, P.bRetry);
		}
}

void UZoogiAnnouncerSubsystem::OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag)
{
	using namespace ZoogiVO;
	AZoogiGameMode* G = BoundGM.Get();
	switch (E)
	{
	case EZoogiMatchEvent::MatchStart:
	{
		Pending.Reset();
		const FZoogiArenaMusicDef* D = G && G->Arena ? ZoogiMusic::DefFor(G->Arena->GetTheme()) : nullptr;
		if (D) Say(FString::Printf(TEXT("Intro_%s"), D->Slug), PHuge, 0.f);
		break;
	}
	case EZoogiMatchEvent::FormingBegin:   Say(TEXT("Forming"), PInfo, 10.f); break; // dropped while the arena intro plays
	case EZoogiMatchEvent::RollOut:        Say(TEXT("RollOut"), PPlay, 10.f); break;
	case EZoogiMatchEvent::StageChanged:
		// "Line it up!" now and then on a Battle phase start, never every turn.
		if (Who == (int32)EZoogiStage::Battle && FPlatformTime::Seconds() - LastBattleLine > 25.0 && Rng.FRand() < 0.4f)
			if (Say(TEXT("Battle"), PLow, 25.f)) LastBattleLine = FPlatformTime::Seconds();
		break;
	case EZoogiMatchEvent::OrbOut:          Say(TEXT("OrbOut"), PPlay - 1, 6.f); break;
	case EZoogiMatchEvent::RingOut:         Say(TEXT("RingOut"), PPlay + 1, 4.f); break;
	case EZoogiMatchEvent::KnockedOut:      Say(TEXT("KnockedOut"), PBig, 3.f); break;
	case EZoogiMatchEvent::RecoveryGate:    Say(TEXT("RecoveryGate"), PInfo - 1, 15.f, 0.3f); break;
	case EZoogiMatchEvent::ResonanceSurge:  Say(TEXT("Surge"), PPlay, 8.f); break;
	case EZoogiMatchEvent::ResonanceShift:  Say(TEXT("Shift"), PHuge - 1, 0.f); break;
	case EZoogiMatchEvent::FollowUpStart:   Say(TEXT("FollowUp"), PPlay + 1, 8.f); break;
	case EZoogiMatchEvent::GaugeFull:       Say(TEXT("GaugeFull"), PInfo, 20.f); break;
	case EZoogiMatchEvent::GuardBreak:      Say(TEXT("GuardBreak"), PPlay, 6.f); break;
	case EZoogiMatchEvent::FloorChanged:    Say(TEXT("FloorCrack"), PInfo + 1, 15.f); break;
	case EZoogiMatchEvent::AnimeCritical:   Say(TEXT("Critical"), PPlay + 1, 5.f); break;
	case EZoogiMatchEvent::AnimeKO:         Say(TEXT("KO"), PBig, 4.f); break;
	case EZoogiMatchEvent::AnimeCombo:      Say(TEXT("Combo"), PPlay + 1, 5.f); break;
	case EZoogiMatchEvent::ClimaxLand:      Say(TEXT("Climax"), PHuge, 0.f); break;
	case EZoogiMatchEvent::FinalRound:      Say(TEXT("FinalRound"), PBig + 1, 0.f, 0.8f); break;
	case EZoogiMatchEvent::MatchPoint:      Say(TEXT("MatchPoint"), PBig, 20.f, 1.0f); break;
	case EZoogiMatchEvent::MatchOver:
	{
		Pending.Reset();
		const int32 R = ZoogiMusic::EndResult(G, Who);
		Say(R == 2 ? TEXT("Draw") : (R == 1 ? TEXT("Victory") : TEXT("Defeat")), PEnd, 0.f, 0.9f);
		break;
	}
	case EZoogiMatchEvent::ReturnToMenu:
		Pending.Reset();
		if (Current) Current->FadeOut(0.3f, 0.f);
		Current = nullptr;
		BusyUntil = 0.0;
		break;
	default: break;
	}
}
