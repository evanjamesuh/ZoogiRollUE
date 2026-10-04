#include "ZoogiAnimeSubsystem.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiPlayerController.h"
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
#include "MediaTexture.h"
#include "FileMediaSource.h"

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
}

void UZoogiAnimeSubsystem::Deinitialize()
{
	if (bFrozen) Unfreeze();
	if (MoviePlayer) MoviePlayer->Close();
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
	const FString Id = ResolveId(E, nullptr);
	if (Mode == EZoogiAnimeMode::Off)
	{
		UE_LOG(LogZoogi, Log, TEXT("Anime event %s -> %s (skipped: Anime=Off)"), *E.Event.ToString(), *Id);
		return;
	}
	if (bPlaying)
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
	const FZoogiAnimeRow* RowPtr = nullptr;
	CurrentId = ResolveId(E, &RowPtr);
	const FZoogiAnimeRow Row = RowPtr ? *RowPtr : DefaultRow(E.Event);
	int32& Plays = FullPlays.FindOrAdd(CurrentId);
	bShort = Mode == EZoogiAnimeMode::Short || E.bCPUOnly || Plays >= FMath::Max(1, Row.FullPlaysPerMatch);
	if (!bShort) ++Plays;
	Length = FMath::Max(0.2f, bShort ? Row.ShortLength : Row.Length);
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
	bPlaying = true;
	Age = 0.f;
	RealStart = FPlatformTime::Seconds();
	if (E.bFreeze) Freeze();
	bMovie = TryStartMovie(CurrentId);
	UE_LOG(LogZoogi, Log, TEXT("Anime START %s (%s, %s %.2fs%s%s)"), *CurrentId, *Row.Tier, bShort ? TEXT("Short") : TEXT("Full"), Length,
		bFrozen ? TEXT(", world frozen") : TEXT(""), bMovie ? TEXT(", movie") : TEXT(""));
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnAnimeStarted(Current);
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
	if (!bPlaying) return;
	Age = (float)(FPlatformTime::Seconds() - RealStart);
	if (Age >= Length) Finish(false);
}

void UZoogiAnimeSubsystem::Skip()
{
	if (bPlaying && Age >= 0.3f)
	{
		UE_LOG(LogZoogi, Log, TEXT("Anime %s skipped by input at %.2fs"), *CurrentId, Age);
		Finish(true);
	}
}

void UZoogiAnimeSubsystem::Finish(bool bSkipped)
{
	if (!bPlaying) return;
	bPlaying = false;
	if (bMovie && MoviePlayer) MoviePlayer->Close();
	bMovie = false;
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
	if (bPlaying) Finish(true);
	Queue.Reset();
	FullPlays.Reset();
}
