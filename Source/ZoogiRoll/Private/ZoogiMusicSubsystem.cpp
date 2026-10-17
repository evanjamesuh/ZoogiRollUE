// Adaptive arena music + the AZoogiGameMode match-event plumbing (Oct 7, branch `music`).
#include "ZoogiMusicSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiCombo.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Quartz/QuartzSubsystem.h"
#include "Quartz/AudioMixerClockHandle.h"
#include "Sound/QuartzQuantizationUtilities.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static const FName GZoogiMusicClock(TEXT("ZoogiMusic"));
static constexpr float GMusicBus = 0.35f; // same level as the old MusicComp (about -23 LUFS in game)

// ------------------------------------------------------------------ table

static const FZoogiArenaMusicDef GArenaMusic[] = {
	{ EZoogiArena::Meadow,   TEXT("MeadowArena"),    168.f, 4, 56 },
	{ EZoogiArena::Frozen,   TEXT("FrozenRing"),     160.f, 4, 48 },
	{ EZoogiArena::Volcanic, TEXT("VolcanicPit"),    180.f, 4, 56 },
	{ EZoogiArena::Cosmic,   TEXT("CosmicPlatform"), 135.f, 4, 48 },
	{ EZoogiArena::Arabian,  TEXT("ArabianNights"),  150.f, 4, 48 },
	{ EZoogiArena::Tomb,     TEXT("PharaohsTomb"),   144.f, 4, 48 },
	{ EZoogiArena::Neon,     TEXT("NightCircuit"),   156.f, 4, 52 },
	// TODO(music): EZoogiArena has no Training value yet. The stems are imported (/Game/Audio/Music/Training); map this
	// row by setting Arena = EZoogiArena::Training once the enum value (appended before MAX) and the arena exist.
	{ EZoogiArena::MAX,      TEXT("Training"),       120.f, 4, 40 },
};

const FZoogiArenaMusicDef* ZoogiMusic::DefFor(EZoogiArena A)
{
	if (A == EZoogiArena::MAX) return nullptr;
	for (const FZoogiArenaMusicDef& D : GArenaMusic) if (D.Arena == A) return &D;
	return nullptr;
}

const FZoogiArenaMusicDef* ZoogiMusic::DefForSlug(const FString& Slug)
{
	for (const FZoogiArenaMusicDef& D : GArenaMusic) if (Slug.Equals(D.Slug, ESearchCase::IgnoreCase)) return &D;
	return nullptr;
}

USoundBase* ZoogiMusic::LoadMusic(const FString& Slug, const TCHAR* Part)
{
	const FString N = FString::Printf(TEXT("S_Music_%s_%s"), *Slug, Part);
	return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Music/%s/%s.%s"), *Slug, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

int32 ZoogiMusic::EndResult(const AZoogiGameMode* G, int32 Who)
{
	if (!G || Who < 0) return 2;
	bool bAnyHuman = false, bHumanWon = false;
	for (const AZoogiBall* B : G->Balls)
	{
		if (!B || B->bCPU) continue;
		bAnyHuman = true;
		if (G->bComboMode ? B->TeamIndex == Who : B->PlayerIndex == Who) bHumanWon = true;
	}
	// Spectator (all CPU) and hotseat (every human wins something) both get the Victory stinger.
	return (!bAnyHuman || bHumanWon) ? 1 : 0;
}

const TCHAR* ZoogiMatch::EventName(EZoogiMatchEvent E)
{
	static const TCHAR* N[] = { TEXT("MatchStart"), TEXT("FormingBegin"), TEXT("FormingDone"), TEXT("RollOut"), TEXT("StageChanged"),
		TEXT("OrbOut"), TEXT("RingOut"), TEXT("KnockedOut"), TEXT("RecoveryGate"), TEXT("ResonanceSurge"), TEXT("ResonanceShift"),
		TEXT("FollowUpOffered"), TEXT("FollowUpStart"), TEXT("GaugeChanged"), TEXT("GaugeFull"), TEXT("GuardBreak"), TEXT("FloorChanged"),
		TEXT("AnimeCombo"), TEXT("AnimeKO"), TEXT("AnimeCritical"), TEXT("ClimaxBegin"), TEXT("ClimaxLand"), TEXT("Duck"),
		TEXT("FinalRound"), TEXT("MatchPoint"), TEXT("MatchOver"), TEXT("ReturnToMenu") };
	static_assert(UE_ARRAY_COUNT(N) == (int32)EZoogiMatchEvent::Count, "event names out of date");
	const int32 I = (int32)E;
	return I >= 0 && I < (int32)UE_ARRAY_COUNT(N) ? N[I] : TEXT("?");
}

// ------------------------------------------------------------------ AZoogiGameMode hooks (declared in ZoogiGameMode.h)

bool AZoogiGameMode::NotifyMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag)
{
	UZoogiMusicSubsystem* M = UZoogiMusicSubsystem::Get(this);
	if (M) M->EnsureBound();
	OnMatchEvent.Broadcast(E, Who, Mag);
	const bool bArena = M && M->IsActive();
	switch (E)
	{
	case EZoogiMatchEvent::MatchStart:
		if (bArena)
		{
			if (MusicComp) { MusicComp->FadeOut(1.0f, 0.f); MusicComp = nullptr; }
			CurrentMusic = TEXT("ArenaMusic"); // so ReturnToMenu's PlayMusic("MenuMusic") starts the menu loop again
		}
		else PlayMusic(TEXT("MatchMusic"));
		break;
	case EZoogiMatchEvent::ClimaxBegin:
		if (!bArena && MusicComp) MusicComp->AdjustVolume(0.25f, 0.f); // the one full music drop-out of the match
		break;
	case EZoogiMatchEvent::ClimaxLand:
		if (!bArena && MusicComp) MusicComp->AdjustVolume(1.6f, 1.f);
		break;
	default:
		break;
	}
	return bArena;
}

void AZoogiGameMode::NotifyAnimeEvent(const FString& Ev)
{
	if (bClimaxHitPending) return; // the climax hit has its own event (ClimaxLand)
	if (Ev == TEXT("KO")) NotifyMatchEvent(EZoogiMatchEvent::AnimeKO);
	else if (Ev == TEXT("Critical")) NotifyMatchEvent(EZoogiMatchEvent::AnimeCritical);
	else if (Ev == TEXT("Combo")) NotifyMatchEvent(EZoogiMatchEvent::AnimeCombo);
}

// ------------------------------------------------------------------ subsystem

UZoogiMusicSubsystem* UZoogiMusicSubsystem::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UZoogiMusicSubsystem>() : nullptr;
}

bool UZoogiMusicSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UZoogiMusicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bDebug = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMusicDebug"));
	bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoArenaMusic"));
	EnsureBound();
}

void UZoogiMusicSubsystem::Deinitialize()
{
	for (int32 i = 0; i < 3; ++i) Stem[i] = nullptr; // world teardown stops the components; the clock dies with the world
	Clock = nullptr;
	bActive = false;
	if (AZoogiGameMode* G = BoundGM.Get()) G->OnMatchEvent.Remove(EventHandle);
	Super::Deinitialize();
}

TStatId UZoogiMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiMusicSubsystem, STATGROUP_Tickables);
}

AZoogiGameMode* UZoogiMusicSubsystem::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
}

void UZoogiMusicSubsystem::EnsureBound()
{
	AZoogiGameMode* G = GM();
	if (!G || (bBound && BoundGM.Get() == G)) return;
	EventHandle = G->OnMatchEvent.AddUObject(this, &UZoogiMusicSubsystem::OnMatchEvent);
	BoundGM = G;
	bBound = true;
}

float UZoogiMusicSubsystem::MasterGain() const
{
	return GMusicBus * DuckCur * ClimaxMul * ZoogiUtil::GetMusicVolume(); // user music volume (Settings slider)
}

bool UZoogiMusicSubsystem::IsShifted() const
{
	const AZoogiGameMode* G = GM();
	return G && G->bComboMode && G->ComboArena && G->ComboArena->IsShifted();
}

bool UZoogiMusicSubsystem::IsFinalMoments() const
{
	const AZoogiGameMode* G = GM();
	if (!G) return false;
	if (G->MenuRounds > 1 && G->RoundNumber >= G->MenuRounds) return true;
	if (G->bComboMode)
		for (int32 t = 0; t < 2; ++t)
		{
			int32 Marks = 0, Alive = 0;
			for (const AZoogiBall* B : G->Balls) if (B && B->TeamIndex == t && !B->bKnockedOut) { Marks += B->RingMarks; ++Alive; }
			if (Alive > 0 && Marks <= 1) return true;
		}
	return false;
}

bool UZoogiMusicSubsystem::StartArena(EZoogiArena Theme)
{
	Stop(0.2f);
	if (bDisabled) { UE_LOG(LogZoogi, Log, TEXT("MUSIC: -ZoogiNoArenaMusic -> MatchMusic fallback")); return false; }
	const FZoogiArenaMusicDef* D = ZoogiMusic::DefFor(Theme);
	if (!D) { UE_LOG(LogZoogi, Log, TEXT("MUSIC: no arena theme for %s -> MatchMusic fallback"), *ZoogiUtil::ArenaName(Theme)); return false; }
	static const TCHAR* Parts[3] = { TEXT("Base"), TEXT("Battle"), TEXT("Shift") };
	for (int32 i = 0; i < 3; ++i)
	{
		Wave[i] = ZoogiMusic::LoadMusic(D->Slug, Parts[i]);
		if (!Wave[i])
		{
			UE_LOG(LogZoogi, Warning, TEXT("MUSIC: %s stem %s missing -> MatchMusic fallback"), D->Slug, Parts[i]);
			for (int32 k = 0; k < 3; ++k) Wave[k] = nullptr;
			return false;
		}
		// Silent layers must keep their playback position so they come back in sync.
		Wave[i]->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	}
	ShiftHitS = ZoogiMusic::LoadMusic(D->Slug, TEXT("ShiftHit"));
	VictoryS = ZoogiMusic::LoadMusic(D->Slug, TEXT("Victory"));
	DefeatS = ZoogiMusic::LoadMusic(D->Slug, TEXT("Defeat"));
	Def = D;
	BarLen = 60.0 / D->BPM * D->BeatsPerBar;
	Bar = 0;
	SurgeBars = PreArmBars = DrawAwakenBars = 0;
	bSeenBattle = false;
	bEnded = false;
	PendingHitAt = -1.0;
	DuckCur = DuckTarget = 1.f;
	DuckUntil = 0.0;
	ClimaxMul = ClimaxTarget = 1.f;
	for (int32 i = 0; i < 3; ++i) Target[i] = Applied[i] = (i == 0 ? 1.f : 0.f);

	UQuartzClockHandle* H = nullptr;
	if (UQuartzSubsystem* Q = UQuartzSubsystem::Get(GetWorld()))
	{
		FQuartzClockSettings S;
		S.TimeSignature.NumBeats = D->BeatsPerBar;
		H = Q->CreateNewClock(this, GZoogiMusicClock, S, /*bOverrideSettingsIfClockExists*/ true);
		if (H) H->SetBeatsPerMinute(this, FQuartzQuantizationBoundary(EQuartzCommandQuantization::None), FOnQuartzCommandEventBP(), H, D->BPM);
	}
	Clock = H;
	int32 NComp = 0;
	FQuartzQuantizationBoundary NextBar(EQuartzCommandQuantization::Bar, 1.f, EQuarztQuantizationReference::BarRelative, true);
	for (int32 i = 0; i < 3; ++i)
	{
		Stem[i] = UGameplayStatics::CreateSound2D(this, Wave[i], MasterGain(), 1.f, 0.f, nullptr, false, false);
		if (!Stem[i]) continue;
		++NComp;
		Stem[i]->bIsUISound = true; // keeps playing through anime freeze-frames (game pause), like the SFX
		if (H) Stem[i]->PlayQuantized(this, H, NextBar, FOnQuartzCommandEventBP(), 0.f, i == 0 ? 0.5f : 0.f, Applied[i]);
		else { Stem[i]->Play(); Stem[i]->AdjustVolume(i == 0 ? 0.5f : 0.f, Applied[i]); }
	}
	if (H) H->StartClock(this, H);
	StartTime = FPlatformTime::Seconds();
	LastMaster = MasterGain();
	bActive = true;
	UE_LOG(LogZoogi, Log, TEXT("MUSIC start: arena=%s theme=%s bpm=%.0f loop=%d bars (%.3f s) stems=3/3 components=%d/3 quartz=%s%s"),
		*ZoogiUtil::ArenaName(Theme), D->Slug, D->BPM, D->LoopBars, D->LoopBars * BarLen, NComp, H ? TEXT("yes") : TEXT("no"),
		NComp == 0 ? TEXT(" (no audio device: state machine runs silently)") : TEXT(""));
	return true;
}

void UZoogiMusicSubsystem::Stop(float Fade)
{
	for (int32 i = 0; i < 3; ++i)
	{
		if (Stem[i]) { if (Fade > 0.f) Stem[i]->FadeOut(Fade, 0.f); else Stem[i]->Stop(); }
		Stem[i] = nullptr;
	}
	if (Clock)
	{
		if (UQuartzSubsystem* Q = UQuartzSubsystem::Get(GetWorld())) Q->DeleteClockByName(this, GZoogiMusicClock);
		Clock = nullptr;
	}
	if (bActive) UE_LOG(LogZoogi, Log, TEXT("MUSIC stop (%.1f s fade)"), Fade);
	bActive = false;
	Def = nullptr;
}

void UZoogiMusicSubsystem::EndMatch(int32 Result)
{
	if (!bActive || bEnded) return;
	bEnded = true;
	for (int32 i = 0; i < 3; ++i) if (Stem[i]) Stem[i]->AdjustVolume(0.4f, 0.f);
	USoundBase* S = Result == 1 ? VictoryS.Get() : DefeatS.Get();
	if (S)
		if (UAudioComponent* C = UGameplayStatics::CreateSound2D(this, S, GMusicBus * ZoogiUtil::GetMusicVolume(), 1.f, 0.f, nullptr, false, true)) { C->bIsUISound = true; C->Play(); }
	UE_LOG(LogZoogi, Log, TEXT("MUSIC end: stems out (0.4 s), %s stinger%s"), Result == 1 ? TEXT("Victory") : (Result == 2 ? TEXT("Defeat (draw)") : TEXT("Defeat")), S ? TEXT("") : TEXT(" (missing)"));
}

void UZoogiMusicSubsystem::DuckFor(float Seconds, float Level)
{
	if (!bActive) return;
	DuckTarget = FMath::Min(DuckTarget, FMath::Clamp(Level, 0.f, 1.f));
	DuckUntil = FMath::Max(DuckUntil, FPlatformTime::Seconds() + Seconds);
}

void UZoogiMusicSubsystem::ClimaxDrop()
{
	ClimaxTarget = 0.f;
	ClimaxSecs = 0.25f;
	UE_LOG(LogZoogi, Log, TEXT("MUSIC climax: drop-out"));
}

void UZoogiMusicSubsystem::ClimaxLand()
{
	ClimaxTarget = 1.f;
	ClimaxSecs = 1.6f;
	SurgeBars = FMath::Max(SurgeBars, 8);
	PlayShiftHit(false);
	UE_LOG(LogZoogi, Log, TEXT("MUSIC climax: land -> ShiftHit now, swell back 1.6 s, surge 8 bars"));
}

void UZoogiMusicSubsystem::PlayShiftHit(bool bNextBeat)
{
	if (!bActive || !ShiftHitS) return;
	UAudioComponent* C = UGameplayStatics::CreateSound2D(this, ShiftHitS, GMusicBus * 0.71f * ZoogiUtil::GetMusicVolume(), 1.f, 0.f, nullptr, false, true); // -3 dB under the stems
	const double Beat = BarLen / FMath::Max(1, Def ? Def->BeatsPerBar : 4);
	if (C)
	{
		C->bIsUISound = true;
		UQuartzClockHandle* H = Clock;
		if (bNextBeat && H)
		{
			FQuartzQuantizationBoundary NextBeat(EQuartzCommandQuantization::Beat, 1.f, EQuarztQuantizationReference::BarRelative, true);
			C->PlayQuantized(this, H, NextBeat, FOnQuartzCommandEventBP());
		}
		else C->Play();
	}
	const double Now = FPlatformTime::Seconds();
	const double Into = FMath::Fmod(Now - StartTime, Beat);
	UE_LOG(LogZoogi, Log, TEXT("MUSIC ShiftHit: %s (in %.2f s)%s"), bNextBeat ? TEXT("next beat") : TEXT("immediate"), bNextBeat ? Beat - Into : 0.0, C ? TEXT("") : TEXT(" [silent: no audio device]"));
}

void UZoogiMusicSubsystem::OnMatchEvent(EZoogiMatchEvent E, int32 Who, float Mag)
{
	AZoogiGameMode* G = GM();
	switch (E)
	{
	case EZoogiMatchEvent::MatchStart:
		bInMatch = true; bRollOutSent = false; bFinalSent = false; bMatchPointSent[0] = bMatchPointSent[1] = false;
		LastGauge[0] = LastGauge[1] = 0.f; LastStage = 255; LastPhase = G ? (uint8)G->Phase : 0;
		Derived.Reset();
		StartArena(G && G->Arena ? G->Arena->GetTheme() : (G ? G->MenuArena : EZoogiArena::Meadow));
		break;
	case EZoogiMatchEvent::ReturnToMenu: bInMatch = false; Stop(1.f); break;
	case EZoogiMatchEvent::MatchOver: bInMatch = false; EndMatch(ZoogiMusic::EndResult(G, Who)); break;
	case EZoogiMatchEvent::ResonanceShift: SurgeBars = FMath::Max(SurgeBars, 16); PlayShiftHit(true); break;
	case EZoogiMatchEvent::ResonanceSurge:
	case EZoogiMatchEvent::KnockedOut:
	case EZoogiMatchEvent::AnimeCombo: SurgeBars = FMath::Max(SurgeBars, 8); break;
	case EZoogiMatchEvent::FollowUpStart: SurgeBars = FMath::Max(SurgeBars, 6); break;
	case EZoogiMatchEvent::OrbOut:
	case EZoogiMatchEvent::RingOut:
	case EZoogiMatchEvent::FloorChanged:
	case EZoogiMatchEvent::AnimeKO:
	case EZoogiMatchEvent::AnimeCritical: SurgeBars = FMath::Max(SurgeBars, 4); break;
	case EZoogiMatchEvent::GuardBreak: SurgeBars = FMath::Max(SurgeBars, 2); break;
	case EZoogiMatchEvent::FollowUpOffered: PreArmBars = FMath::Max(PreArmBars, 2); break;
	case EZoogiMatchEvent::ClimaxBegin: if (bActive) ClimaxDrop(); break;
	case EZoogiMatchEvent::ClimaxLand: if (bActive) ClimaxLand(); break;
	case EZoogiMatchEvent::Duck: DuckFor(Mag, 0.6f); break;
	case EZoogiMatchEvent::GaugeChanged:
		if (Who >= 0 && Who < 2)
		{
			if (LastGauge[Who] < ZoogiComboTune::GaugeMax && Mag >= ZoogiComboTune::GaugeMax) Derive(EZoogiMatchEvent::GaugeFull, Who, Mag);
			LastGauge[Who] = Mag;
		}
		break;
	case EZoogiMatchEvent::StageChanged:
		if (Who == (int32)EZoogiStage::Battle) bSeenBattle = true;
		break;
	default: break;
	}
	if (bActive && E != EZoogiMatchEvent::StageChanged && E != EZoogiMatchEvent::GaugeChanged && E != EZoogiMatchEvent::Duck)
		UE_LOG(LogZoogi, Log, TEXT("MUSIC event %s (who %d, %.0f): surge %d bars, applies at bar %d"), ZoogiMatch::EventName(E), Who, Mag, SurgeBars, Bar + 1);
}

void UZoogiMusicSubsystem::Derive(EZoogiMatchEvent E, int32 Who, float Mag)
{
	Derived.Add(MakeTuple(E, Who, Mag));
}

void UZoogiMusicSubsystem::PollMatch()
{
	AZoogiGameMode* G = GM();
	if (!G || !bInMatch) return;
	const uint8 Ph = (uint8)G->Phase;
	if (Ph != LastPhase)
	{
		if (G->Phase == EZoogiPhase::Forming) Derive(EZoogiMatchEvent::FormingBegin);
		else if (LastPhase == (uint8)EZoogiPhase::Forming) Derive(EZoogiMatchEvent::FormingDone);
		LastPhase = Ph;
	}
	if (!bRollOutSent && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling))
	{
		bRollOutSent = true;
		Derive(EZoogiMatchEvent::RollOut);
	}
	if (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving)
	{
		const uint8 St = (uint8)G->Stage;
		if (St != LastStage) { LastStage = St; Derive(EZoogiMatchEvent::StageChanged, (int32)St); }
	}
	if (!bFinalSent && G->MenuRounds > 1 && G->RoundNumber >= G->MenuRounds && G->Phase != EZoogiPhase::GameOver)
	{
		bFinalSent = true;
		Derive(EZoogiMatchEvent::FinalRound, -1, (float)G->RoundNumber);
	}
	if (G->bComboMode && G->Phase != EZoogiPhase::GameOver)
		for (int32 t = 0; t < 2; ++t)
		{
			if (bMatchPointSent[t]) continue;
			int32 Marks = 0, Alive = 0;
			for (const AZoogiBall* B : G->Balls) if (B && B->TeamIndex == t && !B->bKnockedOut) { Marks += B->RingMarks; ++Alive; }
			if (Alive > 0 && Marks <= 1) { bMatchPointSent[t] = true; Derive(EZoogiMatchEvent::MatchPoint, t, (float)Marks); }
		}
}

void UZoogiMusicSubsystem::Tick(float DeltaTime)
{
	EnsureBound();
	PollMatch();
	if (Derived.Num() > 0)
	{
		TArray<TTuple<EZoogiMatchEvent, int32, float>> Ev = MoveTemp(Derived);
		Derived.Reset();
		if (AZoogiGameMode* G = GM())
			for (const auto& T : Ev)
			{
				if (bDebug || T.Get<0>() != EZoogiMatchEvent::StageChanged)
					UE_LOG(LogZoogi, Log, TEXT("MATCH event (derived) %s who=%d mag=%.0f"), ZoogiMatch::EventName(T.Get<0>()), T.Get<1>(), T.Get<2>());
				G->NotifyMatchEvent(T.Get<0>(), T.Get<1>(), T.Get<2>());
			}
	}
	if (!bActive) return;
	const double Now = FPlatformTime::Seconds();
	const float Dt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.1f);
	// bar clock: the stems were started on the clock's first bar at StartTime
	while (Now >= StartTime + (Bar + 1) * BarLen) OnBar();
	// master gain (duck + climax), smoothed every frame
	if (Now >= DuckUntil) DuckTarget = 1.f;
	const float DuckRate = DuckTarget < DuckCur ? 1.f / 0.05f : 1.f / 0.6f;
	DuckCur = FMath::FInterpConstantTo(DuckCur, DuckTarget, Dt, DuckRate);
	ClimaxMul = FMath::FInterpConstantTo(ClimaxMul, ClimaxTarget, Dt, 1.f / FMath::Max(0.05f, ClimaxSecs));
	const float M = MasterGain();
	if (FMath::Abs(M - LastMaster) > 0.0005f)
	{
		for (int32 i = 0; i < 3; ++i) if (Stem[i]) Stem[i]->SetVolumeMultiplier(M);
		LastMaster = M;
	}
}

void UZoogiMusicSubsystem::Recompute(FString& Why)
{
	const AZoogiGameMode* G = GM();
	if (!G) return;
	float B = Applied[1];
	const bool bForming = G->Phase == EZoogiPhase::Forming;
	const bool bBattle = G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving ||
		(G->Phase == EZoogiPhase::Aiming && (G->Stage == EZoogiStage::Battle || G->Stage == EZoogiStage::Resolve));
	if (bBattle) { B = 1.f; bSeenBattle = true; }
	else if (bForming || !bSeenBattle || DrawAwakenBars >= 2) B = 0.f;
	const bool bShift = IsShifted(), bFinal = IsFinalMoments();
	float C = 0.f;
	if (SurgeBars > 0) C = 1.f;
	if (PreArmBars > 0) C = FMath::Max(C, 0.35f);
	if (bShift) C = FMath::Max(C, B > 0.5f ? 0.6f : 0.35f);
	if (bFinal) C = FMath::Max(C, 0.8f);
	if (G->Phase == EZoogiPhase::GameOver) { B = 0.f; C = 0.f; }
	Target[0] = 1.f; Target[1] = B; Target[2] = C;
	Why = FString::Printf(TEXT("phase=%s stage=%s surge=%d shifted=%d final=%d drawAwakenBars=%d"),
		*UEnum::GetValueAsString(G->Phase), AZoogiGameMode::StageName(G->Stage), SurgeBars, bShift ? 1 : 0, bFinal ? 1 : 0, DrawAwakenBars);
}

void UZoogiMusicSubsystem::OnBar()
{
	++Bar;
	if (SurgeBars > 0) --SurgeBars;
	if (PreArmBars > 0) --PreArmBars;
	const AZoogiGameMode* G = GM();
	const bool bDecide = G && G->Phase == EZoogiPhase::Aiming && (G->Stage == EZoogiStage::Draw || G->Stage == EZoogiStage::Awaken);
	DrawAwakenBars = bDecide ? DrawAwakenBars + 1 : 0;
	if (bEnded) return;
	// Test hook: -ZoogiMusicForceShift=<bar> broadcasts a Resonance Shift through the game mode's match-event delegate at that bar
	// (the CPU demos rarely reach 50 Arena Control in 150 s), so the music + announcer response can be checked in a smoke log.
	static const int32 ForceShiftAt = [] { int32 V = -1; FParse::Value(FCommandLine::Get(), TEXT("ZoogiMusicForceShift="), V); return V; }();
	if (bInMatch && ForceShiftAt > 0 && Bar == ForceShiftAt && !IsShifted())
	{
		UE_LOG(LogZoogi, Log, TEXT("MUSIC test: -ZoogiMusicForceShift=%d -> broadcasting ResonanceShift"), ForceShiftAt);
		if (AZoogiGameMode* FG = GM()) FG->NotifyMatchEvent(EZoogiMatchEvent::ResonanceShift, 0, 50.f);
	}
	FString Why;
	Recompute(Why);
	FString Changes;
	static const TCHAR* L[3] = { TEXT("A"), TEXT("B"), TEXT("C") };
	for (int32 i = 1; i < 3; ++i)
	{
		if (FMath::IsNearlyEqual(Target[i], Applied[i], 0.01f)) continue;
		const float Fade = Target[i] > Applied[i] ? 0.04f : (float)BarLen; // hard entry on the downbeat, one-bar fade out
		if (Stem[i]) Stem[i]->AdjustVolume(Fade, Target[i]);
		Changes += FString::Printf(TEXT(" %s %.2f->%.2f"), L[i], Applied[i], Target[i]);
		Applied[i] = Target[i];
	}
	if (!Changes.IsEmpty() || bDebug)
		UE_LOG(LogZoogi, Log, TEXT("MUSIC bar %d (%s, loop bar %d):%s  [%s]"), Bar, Def ? Def->Slug : TEXT("?"), Def ? Bar % Def->LoopBars : Bar, Changes.IsEmpty() ? TEXT(" (no change)") : *Changes, *Why);
}
