#include "ZoogiAnimeSubsystem.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiArenaHUDWidget.h"
#include "ZoogiHoloSpire.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiPlayerController.h"
#include "ZoogiP5Style.h"
#include "ZoogiActionFun.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "MediaPlayer.h"
#include "HAL/FileManager.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "ZoogiUIFx.h"
#include "ZoogiScreenFx.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Engine/GameViewportClient.h"

namespace
{
	// Minimal CSV line splitter (quoted fields, "" escapes).
	TArray<FString> SplitCsv(const FString& Line)
	{
		TArray<FString> Out;
		FString Cur;
		bool bQ = false;
		for (int32 i = 0; i < Line.Len(); ++i)
		{
			const TCHAR C = Line[i];
			if (bQ)
			{
				if (C == TEXT('"'))
				{
					if (i + 1 < Line.Len() && Line[i + 1] == TEXT('"')) { Cur.AppendChar(C); ++i; }
					else bQ = false;
				}
				else Cur.AppendChar(C);
			}
			else if (C == TEXT('"')) bQ = true;
			else if (C == TEXT(',')) { Out.Add(Cur.TrimStartAndEnd()); Cur.Reset(); }
			else Cur.AppendChar(C);
		}
		Out.Add(Cur.TrimStartAndEnd());
		return Out;
	}

	FString EventFromId(const FString& Id)
	{
		// Anime_<Zoogi>_<Artifact>_<Event...>_NN
		TArray<FString> P;
		Id.ParseIntoArray(P, TEXT("_"));
		if (P.Num() < 5) return FString();
		FString E;
		for (int32 i = 3; i < P.Num() - 1; ++i) E += (E.IsEmpty() ? TEXT("") : TEXT("_")) + P[i];
		return E;
	}

	FString FirstAnimeToken(const FString& S)
	{
		const int32 At = S.Find(TEXT("Anime_"));
		if (At == INDEX_NONE) return FString();
		FString T = S.Mid(At);
		int32 End = 0;
		while (End < T.Len() && (FChar::IsAlnum(T[End]) || T[End] == TEXT('_'))) ++End;
		return T.Left(End);
	}

	FLinearColor EventColor(const FZoogiCombatEvent& E)
	{
		if (E.Artifact != EZoogiArtifact::None) return ZoogiArtifacts::Color(E.Artifact);
		if (E.Event == TEXT("KO")) return FLinearColor(1.f, 0.35f, 0.25f);
		if (E.Event == TEXT("Combo")) return FLinearColor(1.f, 0.8f, 0.25f);
		if (E.Event == TEXT("Miss")) return FLinearColor(0.6f, 0.6f, 0.7f);
		return FLinearColor::White;
	}
}

void UZoogiAnimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString M;
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Value(Cmd, TEXT("ZoogiAnime="), M))
	{
		Mode = M.Equals(TEXT("Off"), ESearchCase::IgnoreCase) ? EZoogiAnimeMode::Off : M.Equals(TEXT("Short"), ESearchCase::IgnoreCase) ? EZoogiAnimeMode::Short : EZoogiAnimeMode::Full;
	}
	else if (FParse::Param(Cmd, TEXT("ZoogiAuto")) && !FParse::Value(Cmd, TEXT("ZoogiShots="), M))
	{
		Mode = EZoogiAnimeMode::Off; // balance streams: no anime pauses
	}
	LoadRows();
	LoadMovieTable();
}

void UZoogiAnimeSubsystem::Deinitialize()
{
	if (bFrozen) Unfreeze();
	if (MoviePlayer) MoviePlayer->Close();
	Panels.Hide();
	Super::Deinitialize();
}

FZoogiAnimeRow UZoogiAnimeSubsystem::DefaultRow(FName Event) const
{
	FZoogiAnimeRow R;
	R.Event = Event.ToString();
	R.Status = TEXT("Built-in default");
	auto Set = [&R](const TCHAR* Tier, float L, float S, int32 Plays = 1) { R.Tier = Tier; R.Length = L; R.ShortLength = S; R.FullPlaysPerMatch = Plays; };
	const FString E = R.Event;
	if (E == TEXT("Infuse")) Set(TEXT("Micro"), 1.2f, 0.5f);
	else if (E == TEXT("Guard")) Set(TEXT("Micro"), 0.8f, 0.4f);
	else if (E == TEXT("GuardBlock")) Set(TEXT("Micro"), 0.8f, 0.3f, 2);
	else if (E == TEXT("PoweredHit")) Set(TEXT("Micro"), 1.0f, 0.4f);
	else if (E == TEXT("Critical")) Set(TEXT("Standard"), 3.0f, 1.2f);
	else if (E == TEXT("KO")) Set(TEXT("Standard"), 2.0f, 0.8f);
	else if (E == TEXT("Combo")) Set(TEXT("Standard"), 2.5f, 1.0f);
	else if (E == TEXT("Ultimate")) Set(TEXT("Special"), 5.0f, 1.5f);
	else Set(TEXT("Micro"), 0.8f, 0.3f); // Miss, Status_X
	return R;
}

void UZoogiAnimeSubsystem::LoadRows()
{
	Rows.Reset();
	// 1) DataTable (editor-authored)
	if (UDataTable* DT = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_AnimeSequences.DT_AnimeSequences"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		if (DT->GetRowStruct() == FZoogiAnimeRow::StaticStruct())
		{
			DT->ForeachRow<FZoogiAnimeRow>(TEXT("ZoogiAnime"), [this](const FName& Key, const FZoogiAnimeRow& Row)
			{
				FZoogiAnimeRow R = Row;
				if (R.Id.IsEmpty()) R.Id = Key.ToString();
				if (R.Event.IsEmpty()) R.Event = EventFromId(R.Id);
				Rows.Add(R.Id, R);
			});
			RowsSource = TEXT("DT_AnimeSequences");
		}
	}
	// 2) The matrix CSV (Docs/MilestoneAwaken/ANIME-SEQUENCE-MATRIX.csv copied to Content/Anime/AnimeSequences.csv, staged as a loose file)
	if (Rows.Num() == 0)
	{
		FString Text;
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/AnimeSequences.csv"));
		if (FFileHelper::LoadFileToString(Text, *Path))
		{
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			TMap<FString, int32> Col;
			for (int32 li = 0; li < Lines.Num(); ++li)
			{
				const TArray<FString> F = SplitCsv(Lines[li]);
				if (li == 0) { for (int32 c = 0; c < F.Num(); ++c) Col.Add(F[c], c); continue; }
				auto Get = [&](const TCHAR* Name) -> FString { const int32* C = Col.Find(Name); return C && F.IsValidIndex(*C) ? F[*C] : FString(); };
				FZoogiAnimeRow R;
				R.Id = Get(TEXT("Example id"));
				if (!R.Id.StartsWith(TEXT("Anime_"))) continue;
				R.Event = EventFromId(R.Id);
				R.Tier = Get(TEXT("Tier"));
				R.Length = FCString::Atof(*Get(TEXT("Length (s)")));
				R.ShortLength = FCString::Atof(*Get(TEXT("Short (s)")));
				R.FullPlaysPerMatch = Get(TEXT("Skip / shorten rule")).Contains(TEXT("twice")) ? 2 : 1;
				R.Fallback = FirstAnimeToken(Get(TEXT("Fallback")));
				R.Status = Get(TEXT("Status"));
				if (R.Length <= 0.f) { const FZoogiAnimeRow D = DefaultRow(FName(*R.Event)); R.Length = D.Length; R.ShortLength = D.ShortLength; }
				Rows.Add(R.Id, R);
			}
			RowsSource = Path;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("Anime: %d matrix rows from %s, mode %s"), Rows.Num(), Rows.Num() ? *RowsSource : TEXT("(built-in defaults)"),
		Mode == EZoogiAnimeMode::Off ? TEXT("Off") : Mode == EZoogiAnimeMode::Short ? TEXT("Short") : TEXT("Full"));
}

int32 UZoogiAnimeSubsystem::Priority(FName Event)
{
	// Ultimate > Combo > KO > Critical > PoweredHit > GuardBlock > Status (matrix "Queue" rule); activations sit below.
	const FString E = Event.ToString();
	if (E == TEXT("Ultimate")) return 9;
	if (E == TEXT("Combo")) return 8;
	if (E == TEXT("KO")) return 7;
	if (E == TEXT("Critical")) return 6;
	if (E == TEXT("PoweredHit")) return 5;
	if (E == TEXT("GuardBlock")) return 4;
	if (E == TEXT("Infuse") || E == TEXT("Guard")) return 3;
	if (E.StartsWith(TEXT("Status_"))) return 2;
	if (E == TEXT("Miss")) return 1;
	return 0;
}

FString UZoogiAnimeSubsystem::ResolveId(const FZoogiCombatEvent& E, const FZoogiAnimeRow** OutRow) const
{
	const FString Z = E.ZoogiName.IsEmpty() ? FString(TEXT("Any")) : E.ZoogiName.Replace(TEXT(" "), TEXT(""));
	const FString A = E.Artifact == EZoogiArtifact::None ? FString(TEXT("Any")) : ZoogiArtifacts::Key(E.Artifact);
	const FString Ev = E.Event.ToString();
	const FString Chain[4] = {
		FString::Printf(TEXT("Anime_%s_%s_%s_01"), *Z, *A, *Ev),
		FString::Printf(TEXT("Anime_Any_%s_%s_01"), *A, *Ev),
		FString::Printf(TEXT("Anime_%s_Any_%s_01"), *Z, *Ev),
		FString::Printf(TEXT("Anime_Any_Any_%s_01"), *Ev) };
	for (const FString& Id : Chain)
	{
		if (const FZoogiAnimeRow* R = Rows.Find(Id))
		{
			if (OutRow) *OutRow = R;
			return Id;
		}
	}
	if (OutRow) *OutRow = nullptr;
	return E.Artifact == EZoogiArtifact::None ? Chain[3] : Chain[1];
}

void UZoogiAnimeSubsystem::PlayEventSfx(const FZoogiCombatEvent& E)
{
	const FString Ev = E.Event.ToString();
	auto Sting = [&](float Vol)
	{
		switch (E.Artifact)
		{
		case EZoogiArtifact::Ember: ZoogiUtil::PlaySfx(this, TEXT("StingEmber"), Vol); break;
		case EZoogiArtifact::Storm: ZoogiUtil::PlaySfx(this, TEXT("StingStorm"), Vol); break;
		case EZoogiArtifact::Frost: ZoogiUtil::PlaySfx(this, TEXT("StingFrost"), Vol); break;
		case EZoogiArtifact::Titan: ZoogiUtil::PlaySfx(this, TEXT("StingTitan"), Vol); break;
		case EZoogiArtifact::Void: ZoogiUtil::PlaySfx(this, TEXT("StingVoid"), Vol); break;
		default: break;
		}
	};
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (G) G->NotifyAnimeEvent(Ev); // announcer + music surge (KO / Critical / Combo)
	ZoogiCosmosBoss::OnEvent(GetWorld(), Ev); // b136: the cosmos boss reacts
	if (Ev == TEXT("Infuse")) { ZoogiUtil::PlaySfx(this, TEXT("AwakenCharge"), 0.9f); Sting(0.35f); }
	else if (Ev == TEXT("Guard")) ZoogiUtil::PlaySfx(this, TEXT("GuardClang"), 0.75f, 0.92f);
	else if (Ev == TEXT("GuardBlock")) ZoogiUtil::PlaySfx(this, TEXT("GuardClang"), 1.f, 1.1f);
	else if (Ev == TEXT("PoweredHit")) Sting(0.9f);
	else if (Ev == TEXT("Critical")) { ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 1.f); Sting(0.55f); if (G) G->DuckMusic(1.4f); }
	else if ((Ev == TEXT("KO") || Ev == TEXT("Combo")) && G && G->ConsumeClimaxHit())
	{
		// The match's one climax hit: full boom (slightly lower), the artifact layer; the music swell is GameMode's.
		ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 1.f, 0.9f);
		Sting(0.6f);
	}
	else if (Ev == TEXT("KO")) { ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.8f, 0.9f); if (G) G->DuckMusic(1.1f); }
	else if (Ev == TEXT("Combo")) { ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.7f, 1.12f); if (G) G->DuckMusic(1.f); }
}

void UZoogiAnimeSubsystem::Emit(const FZoogiCombatEvent& E)
{
	PlayEventSfx(E);
	if (ZoogiAnimeCues::OnEvent(GetWorld(), E.Event.ToString(), E.ZoogiId.ToString(), E.Player)) return; // b164: a director cue replaces the built-in cut-in
	const FString Id = ResolveId(E, nullptr);
	if (Mode == EZoogiAnimeMode::Off)
	{
		UE_LOG(LogZoogi, Log, TEXT("Anime event %s -> %s (skipped: Anime=Off)"), *E.Event.ToString(), *Id);
		return;
	}
	if (bActive)
	{
		// Same beat: a higher-priority event replaces the current one; otherwise queue (max 2) or collapse into the feed.
		if (Age < 0.15f && Priority(E.Event) > Priority(Current.Event))
		{
			UE_LOG(LogZoogi, Log, TEXT("Anime %s replaced by higher-priority %s"), *CurrentId, *E.Event.ToString());
			const bool bWasFrozen = bFrozen;
			TArray<FZoogiCombatEvent> Pending = MoveTemp(Queue); // keep Finish from starting a queued event
			Queue.Reset();
			Finish(true);
			Queue = MoveTemp(Pending);
			FZoogiCombatEvent E2 = E;
			E2.bFreeze = E.bFreeze || bWasFrozen;
			Start(E2);
			return;
		}
		if (Queue.Num() < 2) Queue.Add(E);
		else UE_LOG(LogZoogi, Log, TEXT("Anime event %s collapsed (queue full)"), *E.Event.ToString());
		return;
	}
	Start(E);
}

void UZoogiAnimeSubsystem::Start(const FZoogiCombatEvent& E)
{
	if (CrashState > 0) return; // b59: the crash clip owns the screen (and the movie player)
	if (E.Event == FName(TEXT("KO")) && !E.bRewardShatter)
	{	// b119 (Evan: "if knocked out still play the anime clips every other turn"): a knockout plays the arena's anime crash clip,
		// at most every other turn (-ZoogiKOClipOff = the old first-KO-only cut-in)
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiKOClipOff"));
		AZoogiGameMode* Gk = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		const int32 Turn = Gk ? Gk->TurnCounter : 0;
		if (Turn < LastKOClipTurn) LastKOClipTurn = -100; // a new match
		if (!bOff && Gk && Gk->Arena && Turn - LastKOClipTurn >= 2)
		{
			const FString Clip = CrashClipFor((uint8)Gk->Arena->GetTheme());
			if (!Clip.IsEmpty() && PlayCrash(Clip, EventColor(E))) { LastKOClipTurn = Turn; UE_LOG(LogZoogi, Log, TEXT("Anime KO: crash clip %s (turn %d)"), *Clip, Turn); return; }
		}
	}
	const FZoogiAnimeRow* RowPtr = nullptr;
	CurrentId = ResolveId(E, &RowPtr);
	const FZoogiAnimeRow Row = RowPtr ? *RowPtr : DefaultRow(E.Event);
	int32& Plays = FullPlays.FindOrAdd(CurrentId);
	// Oct 29 night (Evan, build 23 #4): the full cut-in plays only the first time each event type happens in a match
	// (first KO, first combo ...); repeats are a 0.4 s flash. -ZoogiCutInAlways = old per-row rule.
	static const bool bCutInAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCutInAlways"));
	int32& EvPlays = EventFullPlays.FindOrAdd(E.Event);
	bShort = !E.bRewardShatter && (Mode == EZoogiAnimeMode::Short || E.bCPUOnly || Plays >= FMath::Max(1, Row.FullPlaysPerMatch) || (!bCutInAlways && EvPlays >= 1)); // reward shatter: always Full
	if (!bShort) { ++Plays; ++EvPlays; }
	Length = FMath::Max(0.2f, bShort ? (bCutInAlways ? Row.ShortLength : FMath::Min(Row.ShortLength, 0.4f)) : Row.Length);
	CurrentTier = Row.Tier;
	Current = E;
	Color = EventColor(E);
	const FString Art = E.Artifact != EZoogiArtifact::None ? ZoogiArtifacts::Name(E.Artifact).ToUpper() : FString();
	const FString Ev = E.Event.ToString();
	if (Ev == TEXT("Infuse")) Caption = FString::Printf(TEXT("%s  AWAKENS  %s"), *E.ZoogiName.ToUpper(), *Art);
	else if (Ev == TEXT("Guard")) Caption = FString::Printf(TEXT("%s  GUARD"), *Art);
	else if (Ev == TEXT("GuardBlock")) Caption = TEXT("GUARD  BLOCK!");
	else if (Ev == TEXT("PoweredHit")) Caption = FString::Printf(TEXT("%s  HIT!"), *Art);
	else if (Ev == TEXT("Critical")) Caption = TEXT("CRITICAL!");
	else if (Ev == TEXT("KO")) Caption = TEXT("KNOCKOUT!");
	else if (Ev == TEXT("Combo")) Caption = FString::Printf(TEXT("COMBO  x%d"), FMath::RoundToInt(E.Magnitude));
	else if (Ev == TEXT("Miss")) Caption = TEXT("MISSED...");
	else Caption = Ev.ToUpper().Replace(TEXT("_"), TEXT(" "));
	SubCaption = FString::Printf(TEXT("%s   %s %s %.1fs   (%s)"), *CurrentId, *Row.Tier, bShort ? TEXT("Short") : TEXT("Full"), Length,
		RowPtr ? (Row.Status.IsEmpty() ? TEXT("matrix") : *Row.Status) : TEXT("built-in default"));
	bActive = true;
	Age = 0.f;
	RealStart = FPlatformTime::Seconds();
	static const bool bAnimeFreeze = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeFreeze")); // b85 (Evan): gameplay keeps going under the anime
	if (E.bFreeze && bAnimeFreeze) Freeze();
	// Cut-in clip (Evan's anime art, Content/Anime/Movies): only on Full plays, so repeats stay short real-time cards.
	// Oct 30 (Evan, P5 brief step 2): Blue's critical plays the P5-style character cut-in (slanted eyes panel, ~1.1 s; repeats 0.6 s)
	// instead of the clip / placeholder card.
	bP5CutIn = !E.bRewardShatter && Ev == TEXT("Critical") && ZoogiP5::CutInFor(E.ZoogiName) && !ZoogiCritSpire::Enabled(); // reward shatter always hands off to the movie; b89b: crits play their clip on the spire scoreboard
	CurrentMovie = (bShort || bP5CutIn) ? FString() : (E.bRewardShatter ? FindMovie(E, TEXT("Anime_Any_Any_Critical_01")) : FindMovie(E, CurrentId)); // reward: random variant of the 3 crit clips
	bMovie = !CurrentMovie.IsEmpty() && TryStartMovie(CurrentMovie);
	// b59 (Evan: "on scenes where the cutscene streaks appear is exactly when I want slices of anime videos filling in the gaps with an
	// action fight scene"): an event with no clip of its own (the streak-only card, incl. the short repeats) plays a ~1 s anime fight slice
	bool bSlice = false;
	{
		static const bool bNoSlices = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFightSlices"));
		if (!bMovie && !bP5CutIn && !bMoviesOff && !bNoSlices && Ev != TEXT("Infuse") && Ev != TEXT("Miss"))
		{
			static FString LastSlice;
			const FString Dir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"));
			TArray<FString> Pool;
			for (int32 N = 1; N <= 16; ++N) { const FString Id = FString::Printf(TEXT("Anime_Fight_Slice_%02d"), N); if (FPaths::FileExists(FPaths::Combine(Dir, Id + TEXT(".mp4")))) Pool.Add(Id); }
			if (Pool.Num() > 1) Pool.Remove(LastSlice);
			if (Pool.Num() > 0)
			{
				const FString Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
				if (TryStartMovie(Pick)) { CurrentMovie = Pick; bMovie = true; bSlice = true; LastSlice = Pick; }
			}
		}
	}
	bSliceMovie = bSlice; SliceWait = 0.f; PrevAge = 0.f;
	if (bMovie && !bSlice && E.Event != TEXT("Infuse")) UZoogiScreenFx::Shatter(GetWorld(), *FString::Printf(TEXT("anime cut-in %s"), *CurrentMovie)); // b57 (Evan): the shatter IS the transition into the clip (the frame cracks and falls away over it)
	bCutInShot = false;
	PanelShots = 0;
	// Awaken (Infuse) clip -> four comic panels along the screen edges instead of the centre card, so the Zoogi's real
	// in-world transformation stays visible. -ZoogiAnimeCentreCard restores the old card.
	bPanelMode = bMovie && FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimePanels")); // b77 (Evan: "all anime animation play at the top"): the top card again; -ZoogiAnimePanels = the b72 side windows
	if (false) bPanelMode = bMovie && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeCentreCard")); // b72 (Evan: "anime plays in the small windows from the sides like originally"): every clip as the edge panels (was Infuse only)
	if (bMovie)
	{
		const float* Secs = MovieSeconds.Find(CurrentMovie);
		static const bool bShortClips = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeLong")); // b72 (Evan: "revert the anime play time to previous"): short again; -ZoogiAnimeLong = the b67 15 s rule
		Length = bSlice ? FMath::Clamp(Secs ? *Secs : 1.2f, 0.6f, 1.6f) : (bShortClips ? FMath::Clamp(Secs ? *Secs : 2.f, 1.5f, 3.f) : FMath::Max(Secs ? *Secs : 2.f, 15.f)); // b67 (Evan: "don't stop the anime, each should play at least 15 secs"): the whole clip, looped up to 15 s
		if (MoviePlayer) MoviePlayer->SetLooping(!bSlice && !bShortClips && (Secs ? *Secs : 2.f) < 15.f);
		SubCaption = FString::Printf(TEXT("%s   cut-in %s.mp4 %.1fs"), *CurrentId, *CurrentMovie, Length);
	}
	if (bP5CutIn)
	{
		ZoogiP5::FCutIn C;
		C.Name = E.ZoogiName.ToUpper();
		C.Col = ZoogiP5::ZoogiElementColor(E.ZoogiName);
		C.Face = ZoogiP5::Portrait(E.ZoogiName);
		// the Zoogi plays every cut-in in its ultimate form (build 29), so the close-up is the ultimate face (Evan's ult_avi art)
		AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		UZoogiFunSys* Fun = GetWorld()->GetSubsystem<UZoogiFunSys>();
		if (GM && Fun)
			for (AZoogiBall* B : GM->Balls)
				if (B && B->PlayerIndex == E.Player)
					if (UTexture2D* Ult = Fun->UltAvatar(B)) { C.Face = Ult; C.UV0 = FVector2D(0.17f, 0.53f); C.UV1 = FVector2D(0.83f, 0.75f); }
		static const bool bCritTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CritTest"));
		if (bCritTest)
		{
			TWeakObjectPtr<UWorld> Wk = GetWorld();
			C.Shot = [Wk](const FString& N) { if (UWorld* Wd = Wk.Get()) if (AZoogiGameMode* G2 = Wd->GetAuthGameMode<AZoogiGameMode>()) G2->TakeShot(N); };
		}
		Length = bShort ? 0.6f : 1.1f;
		ZoogiP5::PlayCutIn(GetWorld(), C, Length);
	}
	UE_LOG(LogZoogi, Log, TEXT("Anime START %s (%s, %s %.2fs%s%s)"), *CurrentId, *Row.Tier, bShort ? TEXT("Short") : TEXT("Full"), Length,
		bFrozen ? TEXT(", world frozen") : TEXT(""), bMovie ? *FString::Printf(TEXT(", cut-in %s.mp4"), *CurrentMovie) : TEXT(""));
	if (bPanelMode) Panels.Show(GetWorld()->GetGameViewport(), MovieTexture, Color, Length, E.Player); else Panels.Hide();
	// Oct 29 (Evan, build 20): Vefects Easy Impact Frames whenever a focus-line cut-in plays (impact events, every card)
	if (!bPanelMode && Ev != TEXT("Miss") && Ev != TEXT("Infuse") && Ev != TEXT("Guard") && !Ev.StartsWith(TEXT("Status_")))
	{
		int32 V = 1;
		if (Ev == TEXT("Critical")) V = 3; else if (Ev == TEXT("KO")) V = 5; else if (Ev == TEXT("Combo")) V = 7; else if (Ev == TEXT("Ultimate")) V = 9;
		else if (Ev == TEXT("PoweredHit")) V = 2; else if (Ev == TEXT("GuardBlock")) V = 6;
		AActor* F = E.Focus.Get();
		ZoogiShow::ImpactFrame(GetWorld(), F ? F->GetActorLocation() : FVector(0, 0, 100.f), V);
	}
	SyncPlaying();
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnAnimeStarted(Current);
}

void UZoogiAnimeSubsystem::LoadMovieTable()
{
	bMoviesOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAnimeMovies"));
	MovieSeconds.Reset();
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies/Movies.csv"))))
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (int32 i = 1; i < Lines.Num(); ++i)
		{
			FString File, Rest;
			if (!Lines[i].Split(TEXT(","), &File, &Rest)) continue;
			FString Secs;
			Rest.Split(TEXT(","), &Secs, nullptr);
			MovieSeconds.Add(FPaths::GetBaseFilename(File), FCString::Atof(*Secs));
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("Anime cut-in clips: %d listed in Movies.csv%s"), MovieSeconds.Num(), bMoviesOff ? TEXT(" (disabled: -ZoogiNoAnimeMovies)") : TEXT(""));
}

namespace
{
	// Last cut-in variant played per base clip id (e.g. Anime_Any_Any_KO_01 -> Anime_Any_Any_KO_03), kept for the whole session
	// so two plays in a row (same match or the next one) never show the same clip.
	TMap<FString, FString> GLastCutInVariant;
}

bool UZoogiAnimeSubsystem::PlayCrash(const FString& Id, const FLinearColor& Tint)
{
	if (bActive || CrashState > 0 || Id.IsEmpty() || bMoviesOff) return false;
	if (MovieSeconds.Num() == 0) LoadMovieTable();
	if (!TryStartMovie(Id)) return false;
	CurrentMovie = Id; bMovie = false; // drawn by DrawCrash, not the cut-in card
	CrashId = Id; CrashTint = Tint; bCrashFrames = false; CrashState = 1; CrashT0 = FPlatformTime::Seconds(); CrashAge = 0.f; CrashWait = 0.f;
	const float* Secs = MovieSeconds.Find(Id);
	CrashLen = FMath::Clamp(Secs ? *Secs : 2.3f, 1.2f, 16.f);
	ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.8f, 0.7f);
	UE_LOG(LogZoogi, Log, TEXT("Anime crash: smoke wipe -> %s.mp4 (%.1f s)"), *Id, CrashLen);
	return true;
}

FString UZoogiAnimeSubsystem::CrashClipFor(uint8 Arena) const
{
	static TArray<FString> Files; static bool bScanned = false;
	if (!bScanned)
	{
		bScanned = true;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), TEXT("Anime_Crash_*.mp4")), true, false);
		Files.Sort();
	}
	static const TCHAR* Keys[10][2] = { { TEXT("Meadow"), nullptr }, { TEXT("Frozen"), nullptr }, { TEXT("Volcan"), nullptr }, { TEXT("Cosmic"), nullptr },
		{ TEXT("Arabian"), nullptr }, { TEXT("Tomb"), TEXT("Pharaoh") }, { TEXT("Neon"), TEXT("Night") }, { TEXT("Training"), nullptr }, { TEXT("Meadow"), nullptr }, { TEXT("Ancient"), nullptr } };
	const int32 I = FMath::Clamp((int32)Arena, 0, 9);
	TArray<FString> Match;
	for (const FString& F : Files)
	{
		const FString Base = FPaths::GetBaseFilename(F);
		const bool bAncientClip = Base.Contains(TEXT("Ancient"));
		if (I != 9 && bAncientClip) continue; // "Ancient Collision Meadows" is not the Meadow
		for (const TCHAR* K : Keys[I]) if (K && Base.Contains(K)) { Match.Add(Base); break; }
	}
	if (Match.Num() == 0) return FString();
	static FString LastCrash;
	if (Match.Num() > 1) Match.Remove(LastCrash); // variants (_01, _02): never the same one twice in a row
	LastCrash = Match[FMath::RandRange(0, Match.Num() - 1)];
	return LastCrash;
}

void UZoogiAnimeSubsystem::DrawCrash(UCanvas* C)
{
	if (CrashState == 0 || !C) return;
	const float W = C->ClipX, H = C->ClipY;
	// b77 (Evan: "skill summon animation still needs to play at the top" / "all anime animation play at the top"): the crash and
	// skill-summon clips drop into the top card over the frozen game, no smoke cover. -ZoogiCrashFull = the full-screen smoke version
	static const bool bCrashFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrashFull"));
	if (!bCrashFull && CrashId.StartsWith(TEXT("Anime_Awaken_"))) // b80: only the skill summons go to the top card; the post-clash fail clip stays full screen
	{
		if (!MovieTexture || !MovieTexture->GetResource()) return;
		{ int32 Ap = -1; if (AviClip(Ap)) return; } // b88: the wolf summon plays in the caster's avatar instead
		float K = 0.f;
		if (CrashState == 2 && bCrashFrames) K = FMath::Clamp((CrashAge - CrashWait) / 0.2f, 0.f, 1.f);
		else if (CrashState == 3 && bCrashFrames) K = FMath::Clamp(1.f - CrashAge / 0.25f, 0.f, 1.f);
		if (K <= 0.f) return;
		const float E = 1.f - FMath::Pow(1.f - K, 3.f);
		const FBox2D VR = ZoogiArenaHUD::AnimeVideoRect(FVector2D(W, H));
		const float MW = VR.GetSize().X, MH = VR.GetSize().Y, MX = VR.Min.X, MY = VR.Min.Y - (1.f - E) * (VR.Max.Y + 20.f);
		const float Bd = FMath::Max(3.f, MH * 0.03f);
		FCanvasTileItem Sh(FVector2D(MX - Bd * 2.f + 6.f, MY - Bd * 2.f + 8.f), FVector2D(MW + Bd * 4.f, MH + Bd * 4.f), FLinearColor(0.f, 0.f, 0.f, 0.45f)); Sh.BlendMode = SE_BLEND_Translucent; C->DrawItem(Sh);
		FCanvasTileItem Ot(FVector2D(MX - Bd * 2.f, MY - Bd * 2.f), FVector2D(MW + Bd * 4.f, MH + Bd * 4.f), FLinearColor(0.04f, 0.03f, 0.04f, 1.f)); Ot.BlendMode = SE_BLEND_Opaque; C->DrawItem(Ot);
		FCanvasTileItem Rm(FVector2D(MX - Bd, MY - Bd), FVector2D(MW + Bd * 2.f, MH + Bd * 2.f), FMath::Lerp(FLinearColor::White, CrashTint, 0.55f)); Rm.BlendMode = SE_BLEND_Opaque; C->DrawItem(Rm);
		FCanvasTileItem T(FVector2D(MX, MY), MovieTexture->GetResource(), FVector2D(MW, MH), FVector2D(0, 0), FVector2D(1, 1), FMath::Lerp(FLinearColor::White, CrashTint, 0.12f));
		T.BlendMode = SE_BLEND_Opaque;
		C->DrawItem(T);
		return;
	}
	static UTexture2D* Dot = nullptr;
	if (!Dot) { Dot = ZoogiUIFx::ToTexture(ZoogiUIFx::SoftDot(128)); if (Dot) Dot->AddToRoot(); }
	// smoke cover 0..1: rises over the smoke-in, holds 0 during the clip (puffs drift off the edges), rises again then clears on the way out
	float Cover = 0.f, Edge = 0.f;
	if (CrashState == 1) Cover = FMath::Clamp(CrashAge / 0.35f, 0.f, 1.f);
	else if (CrashState == 2) { Cover = bCrashFrames ? FMath::Clamp(1.f - (CrashAge - CrashWait) / 0.3f, 0.f, 1.f) : 1.f; Edge = bCrashFrames ? 1.f : 0.f; } // smoke holds until the clip has a frame
	else Cover = CrashAge < 0.18f ? FMath::Clamp(CrashAge / 0.18f, 0.f, 1.f) : FMath::Clamp(1.f - (CrashAge - 0.18f) / 0.27f, 0.f, 1.f);
	// the clip, aspect-filled (portrait phones crop the sides)
	if (CrashState == 2 && MovieTexture && bCrashFrames)
	{
		const float Va = 16.f / 9.f, Sa = W / FMath::Max(H, 1.f);
		float U0 = 0.f, U1 = 1.f, V0 = 0.f, V1 = 1.f;
		if (Sa < Va) { const float Wf = Sa / Va; U0 = 0.5f - Wf * 0.5f; U1 = 0.5f + Wf * 0.5f; } else { const float Hf = Va / Sa; V0 = 0.5f - Hf * 0.5f; V1 = 0.5f + Hf * 0.5f; }
		FCanvasTileItem T(FVector2D(0, 0), MovieTexture->GetResource(), FVector2D(W, H), FVector2D(U0, V0), FVector2D(U1, V1), FMath::Lerp(FLinearColor::White, CrashTint, 0.16f));
		T.BlendMode = SE_BLEND_Opaque;
		C->DrawItem(T);
	}
	if (!Dot) return;
	FRandomStream R(9183);
	const float Mn = FMath::Min(W, H), T = (float)FPlatformTime::Seconds();
	auto Puff = [&](float X, float Y, float Rad, const FLinearColor& Col)
	{
		FCanvasTileItem P(FVector2D(X - Rad, Y - Rad), Dot->GetResource(), FVector2D(Rad * 2.f, Rad * 2.f), Col);
		P.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(P);
	};
	const FLinearColor Grey(0.62f, 0.6f, 0.64f), Light(0.93f, 0.92f, 0.95f);
	const FLinearColor Tg = FMath::Lerp(Grey, CrashTint, 0.18f), Tl = FMath::Lerp(Light, CrashTint, 0.1f);
	// edge smoke during the clip (a soft smoky frame)
	if (Edge > 0.f)
		for (int32 i = 0; i < 26; ++i)
		{
			const float Ang = R.FRandRange(0.f, 2.f * PI), Ph = R.FRandRange(0.f, 6.f);
			const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
			const float Rr = Mn * R.FRandRange(0.22f, 0.36f) * (1.f + 0.06f * FMath::Sin(T * 1.3f + Ph));
			const FVector2D P = FVector2D(W * 0.5f, H * 0.5f) + Dir * FVector2D(W * 0.62f, H * 0.62f) + Dir * 30.f * FMath::Sin(T + Ph);
			Puff(P.X, P.Y, Rr, (i % 2 ? Tg : Tl).CopyWithNewOpacity(0.55f));
		}
	if (Cover <= 0.f) return;
	// billowing puffs that swell in from the edges until the screen is filled
	for (int32 i = 0; i < 46; ++i)
	{
		const float Sx = R.FRandRange(-0.1f, 1.1f), Sy = R.FRandRange(-0.1f, 1.1f), Ph = R.FRandRange(0.f, 6.f), Sz = R.FRandRange(0.28f, 0.55f);
		const float Delay = FMath::Clamp(FVector2D::Distance(FVector2D(Sx, Sy), FVector2D(0.5f, 0.5f)) * 0.0f + R.FRandRange(0.f, 0.35f), 0.f, 0.35f);
		const float K = FMath::Clamp((Cover - Delay) / (1.f - Delay), 0.f, 1.f);
		if (K <= 0.f) continue;
		const float Rad = Mn * Sz * (0.35f + 0.65f * K) * (1.f + 0.05f * FMath::Sin(T * 2.f + Ph));
		const FVector2D Drift(12.f * FMath::Sin(T * 0.9f + Ph), 8.f * FMath::Cos(T * 1.1f + Ph));
		Puff(Sx * W + Drift.X, Sy * H + Drift.Y, Rad, (i % 3 == 0 ? Tg : Tl).CopyWithNewOpacity(FMath::Min(1.f, 0.35f + 0.9f * K)));
	}
	// solid haze at full cover so nothing of the switch shows through
	const float Haze = FMath::Clamp((Cover - 0.6f) / 0.4f, 0.f, 1.f);
	if (Haze > 0.f) { FCanvasTileItem F(FVector2D(0, 0), FVector2D(W, H), Tl.CopyWithNewOpacity(Haze)); F.BlendMode = SE_BLEND_Translucent; C->DrawItem(F); }
}

bool UZoogiAnimeSubsystem::MovieHasFrames() const
{
	return bMovie && MoviePlayer && MoviePlayer->IsPlaying() && MoviePlayer->GetTime() > FTimespan::FromMilliseconds(30);
}

FString UZoogiAnimeSubsystem::FindMovie(const FZoogiCombatEvent& E, const FString& ResolvedId) const
{
	if (bMoviesOff) return FString();
	const FString Z = E.ZoogiName.IsEmpty() ? FString(TEXT("Any")) : E.ZoogiName.Replace(TEXT(" "), TEXT(""));
	const FString A = E.Artifact == EZoogiArtifact::None ? FString(TEXT("Any")) : ZoogiArtifacts::Key(E.Artifact);
	const FString Ev = E.Event.ToString();
	const FString Chain[5] = {
		ResolvedId,
		FString::Printf(TEXT("Anime_%s_%s_%s_01"), *Z, *A, *Ev),
		FString::Printf(TEXT("Anime_Any_%s_%s_01"), *A, *Ev),
		FString::Printf(TEXT("Anime_%s_Any_%s_01"), *Z, *Ev),
		FString::Printf(TEXT("Anime_Any_Any_%s_01"), *Ev) };
	const FString Dir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"));
	auto Exists = [&Dir](const FString& Id) { return FPaths::FileExists(FPaths::Combine(Dir, Id + TEXT(".mp4"))); };
	for (const FString& Id : Chain)
	{
		if (!Exists(Id)) continue;
		// Variants: <stem>_01 .. <stem>_09 that exist on disk. Pick one at random, never the one this base id played last time.
		TArray<FString> Variants;
		Variants.Add(Id);
		if (Id.EndsWith(TEXT("_01")))
		{
			const FString Stem = Id.LeftChop(2);
			for (int32 N = 2; N <= 9; ++N)
			{
				const FString V = FString::Printf(TEXT("%s%02d"), *Stem, N);
				if (Exists(V)) Variants.Add(V);
			}
		}
		if (Variants.Num() == 1) return Id;
		FString& Last = GLastCutInVariant.FindOrAdd(Id);
		TArray<FString> Pool = Variants;
		Pool.Remove(Last);
		const FString Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		UE_LOG(LogZoogi, Log, TEXT("Anime cut-in variant: %s -> %s (%d variants, previous %s)"), *Id, *Pick, Variants.Num(), Last.IsEmpty() ? TEXT("none") : *Last);
		Last = Pick;
		return Pick;
	}
	return FString();
}

bool UZoogiAnimeSubsystem::TryStartMovie(const FString& Id)
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Id + TEXT(".mp4"));
	if (!FPaths::FileExists(Path)) return false;
	if (!MoviePlayer)
	{
		MoviePlayer = NewObject<UMediaPlayer>(this);
		MoviePlayer->PlayOnOpen = true;
		MoviePlayer->SetLooping(false);
		MovieTexture = NewObject<UMediaTexture>(this);
		MovieTexture->AutoClear = true;
		MovieTexture->SetMediaPlayer(MoviePlayer);
		MovieTexture->UpdateResource();
		MovieSource = NewObject<UFileMediaSource>(this);
	}
	MovieSource->SetFilePath(Path);
	return MoviePlayer->OpenSource(MovieSource);
}

void UZoogiAnimeSubsystem::Tick(float DeltaTime)
{
	// b59 crash clip: smoke in (0.35 s) -> clip (its length, after the first frame) -> smoke out (0.45 s)
	if (CrashState > 0)
	{
		CrashAge = (float)(FPlatformTime::Seconds() - CrashT0);
		AZoogiGameMode* Gs = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrashTestClip")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTest"));
		auto Shot = [&](const TCHAR* N) { if (bShots && Gs && !Gs->ShotDir.IsEmpty()) Gs->TakeShot(N); };
		if (CrashSeekTo >= 0.f && MoviePlayer && MoviePlayer->IsReady() && MoviePlayer->Seek(FTimespan::FromSeconds(CrashSeekTo))) CrashSeekTo = -1.f; // b70: jump to the picked clip
		if (CrashState == 1 && CrashAge >= 0.35f) { CrashState = 2; CrashT0 = FPlatformTime::Seconds(); CrashWait = 0.f; Shot(TEXT("crash_smoke_in")); }
		else if (CrashState == 2)
		{
			const bool bFrames = MoviePlayer && MoviePlayer->IsPlaying() && MoviePlayer->GetTime() > FTimespan::FromMilliseconds(30) && CrashSeekTo < 0.f && MoviePlayer->GetTime() >= FTimespan::FromSeconds(FMath::Max(0.f, CrashSegStart - 0.05f));
			bCrashFrames = bFrames;
			if (!bFrames && CrashWait < 0.8f) { CrashWait = CrashAge; }
			const float Run = CrashAge - CrashWait;
			static bool bMid = false; if (Run > CrashLen * 0.5f && !bMid) { bMid = true; Shot(TEXT("crash_clip")); }
			if (Run >= CrashLen || (!bFrames && CrashWait >= 0.8f && CrashAge > 1.2f && MoviePlayer && !MoviePlayer->IsPlaying())) { CrashState = 3; CrashT0 = FPlatformTime::Seconds(); }
		}
		else if (CrashState == 3)
		{
			if (CrashAge > 0.12f) { static bool bOut = false; if (!bOut) { bOut = true; Shot(TEXT("crash_smoke_out")); } }
			if (CrashAge >= 0.45f)
			{
				CrashState = 0; CrashSeekTo = -1.f; CrashSegStart = 0.f; if (MoviePlayer) MoviePlayer->Close(); bMovie = false; CurrentMovie.Reset(); UE_LOG(LogZoogi, Log, TEXT("Anime crash %s done"), *CrashId);
				TArray<TFunction<void()>> Fns = MoveTemp(AfterCrashFns); AfterCrashFns.Reset(); // b70: the skill's effect now that the view is back
				for (TFunction<void()>& Fn : Fns) if (Fn) Fn();
			}
		}
	}
	if (!bActive) return;
	Age = (float)(FPlatformTime::Seconds() - RealStart);
	if (bSliceMovie && bMovie && SliceWait < 0.8f && !MovieHasFrames()) { const float D = FMath::Max(0.f, Age - PrevAge); SliceWait += D; Length += D; } // b59: decoder warm-up does not eat the ~1 s slice
	PrevAge = Age;
	// Screenshot runs: one capture in the middle of each of the first cut-ins (works while the world is frozen).
	static int32 CutInShots = 0;
	static TMap<FName, int32> CutInShotsPerEvent; // first 2 per event (Infuse comes up every turn and would use them all)
	if (bMovie && !bCutInShot && Age > Length * 0.45f && CutInShots < 12 && CutInShotsPerEvent.FindOrAdd(Current.Event) < 2)
	{
		bCutInShot = true;
		++CutInShotsPerEvent.FindOrAdd(Current.Event);
		if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) G->TakeShot(FString::Printf(TEXT("anime_cutin%d_%s"), ++CutInShots, *Current.Event.ToString()));
	}
	if (bPanelMode)
	{
		Panels.SetAge(Age);
		// capture runs: three frames of the first panel sequence (panels 1-2, 1-3, all four)
		static int32 PanelRuns = 0;
		const float At[3] = { 0.35f, 0.62f, 0.9f };
		if (PanelRuns < 1 && PanelShots < 3 && Age > Length * At[PanelShots])
			if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) { G->TakeShot(FString::Printf(TEXT("anime_panels_%c"), TEXT('a') + PanelShots)); if (++PanelShots == 3) ++PanelRuns; }
	}
	if (bP5CutIn) { if (!ZoogiP5::CutInBusy() || Age > Length + 4.f) Finish(false); return; } // the cut-in widget runs its own clock
	if (Age >= Length) Finish(false);
}

bool UZoogiAnimeSubsystem::PlayTopCard(const FString& Id, const FString& InCaption, int32 Player, const FLinearColor& Tint)
{
	if (CrashState > 0 || bMoviesOff) return false;
	if (bActive) { TArray<FZoogiCombatEvent> Pending = MoveTemp(Queue); Queue.Reset(); Finish(true); Queue = MoveTemp(Pending); }
	if (!TryStartMovie(Id)) return false;
	Current = FZoogiCombatEvent(); Current.Event = FName(TEXT("Showtime")); Current.Player = Player;
	CurrentId = Id; CurrentMovie = Id; CurrentTier = TEXT("S"); Color = Tint; Caption = InCaption;
	bShort = false; bMovie = true; bSliceMovie = false; SliceWait = 0.f; PrevAge = 0.f; bP5CutIn = false; bPanelMode = false; bCutInShot = false; PanelShots = 0;
	bTopCard = true;
	const float* Secs = MovieSeconds.Find(Id);
	Length = FMath::Clamp(Secs ? *Secs : 4.f, 1.5f, 15.f);
	if (MoviePlayer) MoviePlayer->SetLooping(false);
	SubCaption = FString::Printf(TEXT("%s   top card %.1fs"), *Id, Length);
	bActive = true; Age = 0.f; RealStart = FPlatformTime::Seconds();
	Panels.Hide();
	SyncPlaying();
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnAnimeStarted(Current);
	UE_LOG(LogZoogi, Log, TEXT("Anime TOP CARD %s for P%d (%.2fs) - %s"), *Id, Player + 1, Length, *InCaption);
	return true;
}

bool UZoogiAnimeSubsystem::AviClip(int32& OutPlayer) const
{
	if (bTopCard) return false; // b145a: always the top card
	static const bool bTop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfClipTop")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeTop"));
	if (bTop) return false;
	int32 Pl = -1;
	if (CrashState >= 2 && bCrashFrames && CrashId.StartsWith(TEXT("Anime_Awaken_"))) Pl = IntroPlayer; // skill summons
	else if (CrashState == 0 && bActive && bMovie && !bP5CutIn && !bPanelMode && MovieTexture && MovieHasFrames() && !ZoogiCritSpire::On(GetWorld())) Pl = Current.Player; // b88b: combo / character cut-ins (b89b: not the crit on the spire)
	if (Pl < 0) return false;
	FVector2D P, S;
	if (!ZoogiArenaHUD::AvatarRect(Pl, P, S)) return false;
	OutPlayer = Pl;
	return true;
}

void UZoogiAnimeSubsystem::AfterCrash(TFunction<void()> Fn)
{
	if (!Fn) return;
	static const bool bWait = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillAfterClip")); // b86 (Evan): skills trigger with the clip, not after it
	if (bWait && CrashState > 0) AfterCrashFns.Add(MoveTemp(Fn)); else Fn();
}

bool UZoogiAnimeSubsystem::PlayPowerIntro(const FString& Key, int32 Player, const FLinearColor& Tint)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPowerIntros"));
	const FString Id = FString::Printf(TEXT("Anime_Awaken_%s_01"), *Key);
	if (bOff || !FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Id + TEXT(".mp4")))) return false;
	const FString Seen = FString::Printf(TEXT("%s#%d"), *Key, Player);
	static const bool bOnce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerIntroOnce")); // b70: every cast plays (one clip, different each time)
	if (bOnce && IntroSeen.Contains(Seen)) return false;
	if (!PlayCrash(Id, Tint)) return false;
	IntroSeen.Add(Seen);
	IntroPlayer = Player; // b88
	// b70 (Evan: "from these videos only play one of the clips of the compilation, at random, different every time"): the
	// Anime_Awaken_* files are 4 clips of ~3.77 s back to back (scene cuts at 3.77 / 7.53 / 11.30 s)
	static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerIntroFull"));
	const float* Secs = MovieSeconds.Find(Id);
	const float Total = Secs ? *Secs : 15.067f;
	const int32 N = FMath::Max(1, FMath::RoundToInt(Total / 3.7667f));
	if (!bFull && N > 1)
	{
		const float Seg = Total / N;
		int32& Last = LastIntroSeg.FindOrAdd(Key, -1);
		int32 Pick = FMath::RandRange(0, N - 1);
		if (Pick == Last) Pick = (Pick + 1 + FMath::RandRange(0, N - 2)) % N;
		Last = Pick;
		CrashSegStart = Pick * Seg; CrashSeekTo = Pick > 0 ? CrashSegStart : -1.f;
		CrashLen = Seg - 0.06f;
		UE_LOG(LogZoogi, Log, TEXT("Anime power intro %s: clip %d of %d (%.2f - %.2f s)"), *Id, Pick + 1, N, CrashSegStart, CrashSegStart + Seg);
	}
	else CrashSegStart = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Anime power intro %s for P%d"), *Id, Player + 1);
	return true;
}

void UZoogiAnimeSubsystem::Skip()
{
	static const bool bIntroSkip = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIntroSkip"));
	if (CrashState == 2 && CrashId.StartsWith(TEXT("Anime_Awaken_")) && !bIntroSkip) return; // b66 (Evan): "let them play out" - skill intros are not skippable
	if (CrashState == 2 && !bIntroSkip) return; // b67: the crash clips play out too
	if (CrashState == 2 && CrashAge > 1.f) { CrashState = 3; CrashT0 = FPlatformTime::Seconds(); UE_LOG(LogZoogi, Log, TEXT("Anime %s skipped by input"), *CrashId); return; } // b62
	static const bool bLongClips = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeLong"));
	if (bLongClips && bActive && bMovie && !bSliceMovie && !bIntroSkip) return; // b67 "don't stop the anime" (b72: only with -ZoogiAnimeLong)
	if (bActive && Age >= 0.3f)
	{
		UE_LOG(LogZoogi, Log, TEXT("Anime %s skipped by input at %.2fs"), *CurrentId, Age);
		Finish(true);
	}
}

void UZoogiAnimeSubsystem::Finish(bool bSkipped)
{
	if (!bActive) return;
	bActive = false;
	bTopCard = false; // b145a
	bPanelMode = false;
	Panels.Hide();
	if (bP5CutIn) { bP5CutIn = false; ZoogiP5::StopCutIn(); }
	SyncPlaying();
	if (bMovie && MoviePlayer) MoviePlayer->Close();
	bMovie = false;
	CurrentMovie.Reset();
	if (bFrozen) Unfreeze();
	UE_LOG(LogZoogi, Log, TEXT("Anime END %s (%.2fs%s)"), *CurrentId, Age, bSkipped ? TEXT(", skipped") : TEXT(""));
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnAnimeFinished();
	if (Queue.Num() > 0)
	{
		const FZoogiCombatEvent Next = Queue[0];
		Queue.RemoveAt(0);
		Start(Next);
	}
}

void UZoogiAnimeSubsystem::Freeze()
{
	AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (!G || bFrozen) return;
	if (!ZoogiUtil::ReactionPauses()) { UE_LOG(LogZoogi, Log, TEXT("Anime freeze skipped (no pauses): the cut-in plays over the running game")); return; } // Oct 24 (Evan): never pause for anime/crit cut-ins
	Snaps.Reset();
	auto Add = [this](UPrimitiveComponent* C)
	{
		if (!C || !C->IsSimulatingPhysics()) return;
		FBodySnap S;
		S.Comp = C;
		S.Xf = C->GetComponentTransform();
		S.Lin = C->GetPhysicsLinearVelocity();
		S.Ang = C->GetPhysicsAngularVelocityInRadians();
		Snaps.Add(S);
	};
	for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Add(B->Sphere);
	for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Add(O->Sphere);
	UGameplayStatics::SetGamePaused(this, true);
	bFrozen = true;
}

void UZoogiAnimeSubsystem::Unfreeze()
{
	if (!bFrozen) return;
	UGameplayStatics::SetGamePaused(this, false);
	bFrozen = false;
	// Restore: the anime must not leave any trace on the simulation (positions + velocities exactly as frozen).
	float MaxPos = 0.f, MaxVel = 0.f;
	for (const FBodySnap& S : Snaps)
	{
		UPrimitiveComponent* C = S.Comp.Get();
		if (!C || !C->IsSimulatingPhysics()) continue;
		MaxPos = FMath::Max(MaxPos, FVector::Dist(C->GetComponentLocation(), S.Xf.GetLocation()));
		MaxVel = FMath::Max(MaxVel, FVector::Dist(C->GetPhysicsLinearVelocity(), S.Lin));
		C->SetWorldTransform(S.Xf, false, nullptr, ETeleportType::TeleportPhysics);
		C->SetPhysicsLinearVelocity(S.Lin);
		C->SetPhysicsAngularVelocityInRadians(S.Ang);
	}
	UE_LOG(LogZoogi, Log, TEXT("Anime unfreeze: restored %d bodies (max drift %.2f cm, %.2f cm/s)"), Snaps.Num(), MaxPos, MaxVel);
	Snaps.Reset();
}

void UZoogiAnimeSubsystem::ResetMatch()
{
	Queue.Reset();
	IntroSeen.Reset(); // b62
	if (bActive) Finish(true);
	Queue.Reset();
	FullPlays.Reset();
	EventFullPlays.Reset();
}

FString UZoogiAnimeSubsystem::PlayMenuMovie(int32 Step)
{
	if (MovieSeconds.Num() == 0) LoadMovieTable();
	TArray<FString> Ids;
	for (const TPair<FString, float>& P : MovieSeconds) if (!P.Key.StartsWith(TEXT("Menu_")) && !P.Key.StartsWith(TEXT("Anime_Fight_Slice")) && !P.Key.StartsWith(TEXT("Anime_Crash_"))) Ids.Add(P.Key);
	Ids.Sort();
	static int32 Cursor = -1;
	for (int32 Tries = 0; Tries < Ids.Num(); ++Tries)
	{
		Cursor = ((Cursor + Step) % Ids.Num() + Ids.Num()) % Ids.Num();
		if (TryStartMovie(Ids[Cursor]))
		{
			CurrentMovie = Ids[Cursor];
			UE_LOG(LogZoogi, Log, TEXT("Menu anime: playing %s (%d/%d)"), *CurrentMovie, Cursor + 1, Ids.Num());
			return CurrentMovie;
		}
	}
	UE_LOG(LogZoogi, Warning, TEXT("Menu anime: no playable clip (%d listed in Movies.csv)"), Ids.Num());
	return FString();
}


FString UZoogiAnimeSubsystem::PlayMenuMovieById(const FString& Id)
{
	if (Id.IsEmpty()) return FString();
	if (MovieSeconds.Num() == 0) LoadMovieTable();
	if (TryStartMovie(Id))
	{
		CurrentMovie = Id;
		UE_LOG(LogZoogi, Log, TEXT("Menu anime: playing by id %s"), *CurrentMovie);
		return CurrentMovie;
	}
	UE_LOG(LogZoogi, Warning, TEXT("Menu anime: id %s not playable"), *Id);
	return FString();
}
void UZoogiAnimeSubsystem::StopMenuMovie()
{
	if (MoviePlayer && !bActive) MoviePlayer->Close();
}
