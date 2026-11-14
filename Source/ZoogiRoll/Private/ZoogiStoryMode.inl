// ================================================================================================================ STORY MODE
// Included at the end of ZoogiP5Style.cpp (it paints with the P5 helpers above). See ZoogiStory.h.
// Flow: STORY card -> HUB (4 episodes) -> episode title CARD -> MAP (tap a stop: Blue's token travels there and the live 3D arena
// behind the screen becomes that place) -> ROLL OUT -> DIALOGUE scene -> VS -> the battle (fixed party, cards on, CPU tier per
// mission) -> MISSION CLEAR / FAILED -> the win scene -> MAP, or EPISODE CLEAR after the last stop.
// Sound design: every speaker has a voice "blip" pitch for the typewriter, a whoosh when the shot changes, and each line can cue
// stingers / impacts (Dialogue.csv); the place's own arena theme plays under the scenes and its battle stem under the VS screen.

#include "ZoogiStory.h"
#include "ZoogiGameMode.h"
#include "ZoogiMusicSubsystem.h"
#include "ZoogiProgression.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "MediaSoundComponent.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "Components/SphereComponent.h"

// ------------------------------------------------------------------------------------------------ data
namespace ZoogiStoryData
{
	struct FChapter { FString Id, Ep, Title, Sub, Element, Bg, Hero, Film; };
	struct FSpot { FString Chapter, Id, Label, Sub, Scene, Mission; EZoogiArena Env = EZoogiArena::Meadow; bool bUnder = false; }; // b178e: env=Underrealm
	struct FMission
	{
		FString Id, Title, Mode, Tier, WinScene, LoseWho, LoseLine, Reward; bool bUnder = false; // b178e
		TArray<FString> Players;
		TArray<bool> CPU;
		EZoogiArena Env = EZoogiArena::Meadow;
		int32 Rounds = 3;
	};
	struct FLine { FString Who, Text, Fx, Sfx; };
	struct FDB
	{
		TArray<FChapter> Chapters;
		TArray<FSpot> Spots;
		TMap<FString, FMission> Missions;
		TMap<FString, TArray<FLine>> Scenes;
		bool bLoaded = false;
	};
	static FDB GDB;

	static const TCHAR* ArenaNames[] = { TEXT("Meadow"), TEXT("Frozen"), TEXT("Volcanic"), TEXT("Cosmic"), TEXT("Arabian"), TEXT("Tomb"), TEXT("Neon"), TEXT("Training"), TEXT("Teaser"), TEXT("Ancient") };
	static EZoogiArena ArenaOf(const FString& S)
	{
		if (S.Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase)) return EZoogiArena::Cosmic; // b178e: the Cosmic court, Shadow Realm dressing
		for (int32 a = 0; a < (int32)UE_ARRAY_COUNT(ArenaNames) && a < (int32)EZoogiArena::MAX; ++a)
			if (S.Equals(ArenaNames[a], ESearchCase::IgnoreCase)) return (EZoogiArena)a;
		return EZoogiArena::Meadow;
	}
	static FString ArenaKey(EZoogiArena A)
	{
		const int32 i = (int32)A;
		return (i >= 0 && i < (int32)UE_ARRAY_COUNT(ArenaNames)) ? FString(ArenaNames[i]).ToLower() : FString(TEXT("meadow"));
	}

	static void ReadTable(const TCHAR* File, TArray<TMap<FString, FString>>& Rows)
	{
		FString Text;
		const FString P = FPaths::ProjectContentDir() / TEXT("Data/Story") / File;
		if (!FFileHelper::LoadFileToString(Text, *P)) { UE_LOG(LogZoogi, Warning, TEXT("STORY: missing %s"), *P); return; }
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		TArray<FString> Head;
		for (FString Ln : Lines)
		{
			Ln.TrimStartAndEndInline();
			if (Ln.IsEmpty() || Ln.StartsWith(TEXT("#"))) continue;
			TArray<FString> Cells;
			Ln.ParseIntoArray(Cells, TEXT("|"), false);
			for (FString& C : Cells) C.TrimStartAndEndInline();
			if (Head.Num() == 0) { Head = Cells; continue; }
			TMap<FString, FString> R;
			for (int32 i = 0; i < Head.Num(); ++i) R.Add(Head[i], Cells.IsValidIndex(i) ? Cells[i] : FString());
			Rows.Add(MoveTemp(R));
		}
	}

	static void Load()
	{
		if (GDB.bLoaded) return;
		GDB.bLoaded = true;
		TArray<TMap<FString, FString>> R;
		ReadTable(TEXT("Chapters.csv"), R);
		for (const TMap<FString, FString>& X : R)
		{
			FChapter C;
			C.Id = X.FindRef(TEXT("id")); C.Ep = X.FindRef(TEXT("ep")); C.Title = X.FindRef(TEXT("title")); C.Sub = X.FindRef(TEXT("subtitle"));
			C.Element = X.FindRef(TEXT("element")); C.Bg = X.FindRef(TEXT("bg")); C.Hero = X.FindRef(TEXT("hero")); C.Film = X.FindRef(TEXT("film"));
			GDB.Chapters.Add(C);
		}
		R.Reset();
		ReadTable(TEXT("Spots.csv"), R);
		for (const TMap<FString, FString>& X : R)
		{
			FSpot S;
			S.Chapter = X.FindRef(TEXT("chapter")); S.Id = X.FindRef(TEXT("id")); S.Label = X.FindRef(TEXT("label")); S.Sub = X.FindRef(TEXT("sub"));
			S.Env = ArenaOf(X.FindRef(TEXT("env"))); S.bUnder = X.FindRef(TEXT("env")).Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase); S.Scene = X.FindRef(TEXT("scene")); S.Mission = X.FindRef(TEXT("mission"));
			GDB.Spots.Add(S);
		}
		R.Reset();
		ReadTable(TEXT("Missions.csv"), R);
		for (const TMap<FString, FString>& X : R)
		{
			FMission M;
			M.Id = X.FindRef(TEXT("id")); M.Title = X.FindRef(TEXT("title")); M.Mode = X.FindRef(TEXT("mode")).ToLower(); M.Env = ArenaOf(X.FindRef(TEXT("env"))); M.bUnder = X.FindRef(TEXT("env")).Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase);
			M.Tier = X.FindRef(TEXT("tier")); M.Rounds = FMath::Clamp(FCString::Atoi(*X.FindRef(TEXT("rounds"))), 1, 30);
			M.WinScene = X.FindRef(TEXT("win_scene")); M.LoseWho = X.FindRef(TEXT("lose_speaker")); M.LoseLine = X.FindRef(TEXT("lose_line")); M.Reward = X.FindRef(TEXT("reward"));
			TArray<FString> P;
			X.FindRef(TEXT("players")).ParseIntoArray(P, TEXT(","), true);
			for (FString Id : P)
			{
				Id.TrimStartAndEndInline();
				const bool bCPU = Id.EndsWith(TEXT("*"));
				if (bCPU) Id.LeftChopInline(1);
				M.Players.Add(Id.ToLower());
				M.CPU.Add(bCPU);
			}
			GDB.Missions.Add(M.Id, M);
		}
		R.Reset();
		ReadTable(TEXT("Dialogue.csv"), R);
		for (const TMap<FString, FString>& X : R)
		{
			FLine L;
			L.Who = X.FindRef(TEXT("who")).ToLower(); L.Text = X.FindRef(TEXT("line")); L.Fx = X.FindRef(TEXT("fx")).ToLower(); L.Sfx = X.FindRef(TEXT("sfx"));
			GDB.Scenes.FindOrAdd(X.FindRef(TEXT("scene"))).Add(L);
		}
		UE_LOG(LogZoogi, Log, TEXT("STORY data: %d episodes, %d stops, %d missions, %d scenes"), GDB.Chapters.Num(), GDB.Spots.Num(), GDB.Missions.Num(), GDB.Scenes.Num());
	}

	static TArray<int32> ChapterSpots(int32 Ci)
	{
		TArray<int32> Out;
		if (!GDB.Chapters.IsValidIndex(Ci)) return Out;
		for (int32 i = 0; i < GDB.Spots.Num(); ++i) if (GDB.Spots[i].Chapter == GDB.Chapters[Ci].Id) Out.Add(i);
		return Out;
	}
	static int32 ChapterOfSpot(int32 Si)
	{
		if (!GDB.Spots.IsValidIndex(Si)) return 0;
		for (int32 c = 0; c < GDB.Chapters.Num(); ++c) if (GDB.Chapters[c].Id == GDB.Spots[Si].Chapter) return c;
		return 0;
	}

	// who's who: name tag, MenuArt face, colour, typewriter voice pitch, side of the two-shot
	struct FWho { const TCHAR* Id; const TCHAR* Name; const TCHAR* Art; FColor Col; float Pitch; bool bRight; };
	static const FWho Whos[] = {
		{ TEXT("blue"), TEXT("FROSTI"), TEXT("blue"), FColor(34, 195, 230), 1.25f, false },
		{ TEXT("tavi"), TEXT("TAVI"), TEXT("tavi"), FColor(240, 138, 28), 1.45f, false },
		{ TEXT("bolt"), TEXT("BOLT"), TEXT("bolt"), FColor(250, 200, 30), 1.35f, false },     // Oct 30: the yellow electric one (Evan's cast sheet)
		{ TEXT("hotstreak"), TEXT("HOTSTREAK"), TEXT("hotstreak"), FColor(255, 110, 30), 1.2f, true },  // the orange lava one (Vex's partner)
		// Oct 31 (Evan: "full cast"): the rest of the roster, shadow-touched opponents
		{ TEXT("nightshade"), TEXT("NIGHTSHADE"), TEXT("nightshade"), FColor(140, 70, 230), 0.75f, true },
		{ TEXT("wraps"), TEXT("WRAPS"), TEXT("wraps"), FColor(222, 196, 140), 0.85f, true },
		{ TEXT("pinpoint"), TEXT("PINPOINT"), TEXT("pinpoint"), FColor(120, 190, 255), 1.3f, true },
		{ TEXT("lars"), TEXT("LARS"), TEXT("lars"), FColor(60, 220, 210), 1.1f, true },
		{ TEXT("moss"), TEXT("MOSS"), TEXT("moss"), FColor(108, 198, 154), 0.8f, false },
		{ TEXT("vex"), TEXT("VEX"), TEXT("vex"), FColor(209, 26, 42), 0.95f, true },
		{ TEXT("orin"), TEXT("MASTER ORIN"), TEXT("master_orin"), FColor(217, 164, 65), 0.7f, true },
		{ TEXT("sera"), TEXT("SERA"), TEXT("sera"), FColor(199, 182, 242), 1.15f, true },
		{ TEXT("luma"), TEXT("LUMI"), TEXT("luma"), FColor(232, 28, 200), 1.6f, true },
		{ TEXT("dark"), TEXT("DARK ZOOGY"), TEXT("dark_zoogy"), FColor(255, 48, 72), 0.55f, true },
		{ TEXT("shadow"), TEXT("???"), TEXT("dark_zoogy"), FColor(255, 48, 72), 0.5f, true },
	};
	static const FWho* Who(const FString& Id)
	{
		const FString K = Id == TEXT("darkzoogy") ? FString(TEXT("dark")) : Id;
		for (const FWho& W : Whos) if (K.Equals(W.Id, ESearchCase::IgnoreCase)) return &W;
		return nullptr;
	}
	static FLinearColor WhoCol(const FString& Id) { const FWho* W = Who(Id); return W ? FLinearColor::FromSRGBColor(W->Col) : Accent(); }
	/** Oct 30 (Evan's full renders): Data/MenuArt/Faces/story_<key>.png head shots (Frosti / Lumi / Vex / Hotstreak) first */
	static UTexture2D* StoryArt(const FString& K) { if (UTexture2D* T = Portrait(TEXT("story_") + K)) return T; return Portrait(K); }
	static UTexture2D* Face(const FString& Id) { const FWho* W = Who(Id); return StoryArt(W ? FString(W->Art) : Id); }
	static FString WhoName(const FString& Id) { const FWho* W = Who(Id); return W ? FString(W->Name) : Id.ToUpper(); }

	static FLinearColor ElemCol(const FString& E)
	{
		if (E.Equals(TEXT("Ember"), ESearchCase::IgnoreCase)) return ElementColor(TEXT("fire"));
		if (E.Equals(TEXT("Storm"), ESearchCase::IgnoreCase)) return ElementColor(TEXT("shock"));
		if (E.Equals(TEXT("Void"), ESearchCase::IgnoreCase)) return ElementColor(TEXT("void"));
		return ElementColor(TEXT("frost"));
	}
	static int32 RosterIndex(const FString& Id)
	{
		const TArray<FZoogiDef>& R = ZoogiUtil::Roster();
		for (int32 i = 0; i < R.Num(); ++i) if (R[i].Id.ToString().Equals(Id, ESearchCase::IgnoreCase)) return i;
		UE_LOG(LogZoogi, Warning, TEXT("STORY: no Zoogi '%s' in the roster"), *Id);
		return 0;
	}
	static FString ModeTag(const FMission& M) { return M.Mode == TEXT("shooter1") ? TEXT("SHOOTER: HOLD THE LINE") : M.Mode == TEXT("shooter2") ? TEXT("SHOOTER ROYALE") : M.Mode == TEXT("shooter3") ? TEXT("SHOOTER: TEAM DEATHMATCH") : M.Mode == TEXT("combo") ? TEXT("2V2 TAG BATTLE") : (M.Mode == TEXT("royale") ? TEXT("BATTLE ROYALE") : (M.Players.Num() > 2 ? TEXT("CLASSIC FREE-FOR-ALL") : TEXT("1V1 DUEL"))); } // b151: a 4-Zoogi classic mission
}

// ------------------------------------------------------------------------------------------------ progress
namespace ZoogiStorySave
{
	static TSet<FString> GClear;
	static bool bLoaded = false;
	static bool Test() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryTest")); return b; }
	static bool All() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryAll")); return b; }
	static FString Path() { return FPaths::ProjectSavedDir() / TEXT("SaveGames") / FString(Test() ? TEXT("ZoogiStoryTest.txt") : TEXT("ZoogiStory.txt")); }
	static void Load()
	{
		if (bLoaded) return;
		bLoaded = true;
		if (Test()) { IFileManager::Get().Delete(*Path()); return; }
		FString T;
		if (!FFileHelper::LoadFileToString(T, *Path())) return;
		TArray<FString> L;
		T.ParseIntoArrayLines(L, true);
		for (FString& S : L) { S.TrimStartAndEndInline(); if (!S.IsEmpty()) GClear.Add(S); }
		UE_LOG(LogZoogi, Log, TEXT("STORY save: %d stops cleared"), GClear.Num());
	}
	static void Save()
	{
		const FString P = Path(), Tmp = P + TEXT(".tmp");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(P), true);
		FString T;
		for (const FString& S : GClear) T += S + TEXT("\n");
		if (FFileHelper::SaveStringToFile(T, *Tmp)) IFileManager::Get().Move(*P, *Tmp, true);
	}
	static bool Cleared(int32 Si) { return ZoogiStoryData::GDB.Spots.IsValidIndex(Si) && GClear.Contains(ZoogiStoryData::GDB.Spots[Si].Id); }
	static int32 Done(int32 Ci) { int32 N = 0; for (int32 s : ZoogiStoryData::ChapterSpots(Ci)) N += Cleared(s) ? 1 : 0; return N; }
	static int32 Total(int32 Ci) { return ZoogiStoryData::ChapterSpots(Ci).Num(); }
	static bool ChapterOpen(int32 Ci) { return Ci <= 0 || All() || (Total(Ci - 1) > 0 && Done(Ci - 1) >= Total(Ci - 1)); }
	static bool SpotOpen(int32 Si)
	{
		const int32 Ci = ZoogiStoryData::ChapterOfSpot(Si);
		if (!ChapterOpen(Ci)) return false;
		const TArray<int32> S = ZoogiStoryData::ChapterSpots(Ci);
		const int32 k = S.IndexOfByKey(Si);
		return k <= 0 || All() || Cleared(S[k - 1]);
	}
}

// ------------------------------------------------------------------------------------------------ director (state + flow)
class SZoogiStory;
namespace ZoogiStoryDir
{
	using namespace ZoogiStoryData;
	enum class EScr : uint8 { None, Hub, Card, Map, Dialogue, VS, Mission, Results, Clear };
	struct FDir
	{
		EScr Scr = EScr::None;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UGameViewportClient> VC;
		TSharedPtr<SZoogiStory> W;
		FTSTicker::FDelegateHandle Tick;
		int32 Chapter = 0;
		int32 Spot = -1;            // where Blue's token stands (index into GDB.Spots)
		bool bPanel = false;        // MAP: arrived, the ROLL OUT panel is up
		int32 GuidePick = -1;       // b67: the first stop's GUIDE toggle (-1 = default: ON until played once)
		FString Mission, Scene, Place;
		int32 Line = 0;
		TFunction<void()> After;    // what plays when the scene ends
		bool bWon = false, bOverPending = false, bChapterDone = false, bJustCleared = false;
		float MissionT = 0.f, Clock = 0.f, NextBattleShot = 3.f;
		bool bTest = false, bTestFinished = false, bTestActed = false, bTestQuit = false, bTestFilmDone = false;
		int32 TestRound = 0;
		TStrongObjectPtr<UAudioComponent> Music;
		FString MusicKey;
	};
	static FDir GS;

	static AZoogiGameMode* GM() { UWorld* Wd = GS.World.Get(); return Wd ? Wd->GetAuthGameMode<AZoogiGameMode>() : nullptr; }
	static void Sfx(const TCHAR* N, float V = 0.8f, float P = 1.f) { if (UWorld* Wd = GS.World.Get()) ZoogiUtil::PlaySfx(Wd, N, V, P); }
	static void PlayVO(const FString& Line, float V = 1.f)
	{
		UWorld* Wd = GS.World.Get();
		if (!Wd) return;
		const FString N = TEXT("S_VO_") + Line;
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/VO/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet))
			UGameplayStatics::PlaySound2D(Wd, S, V);
	}
	/** a Dialogue.csv sfx cue list: "StingFrost+CritBoom", "ClimaxRumble:0.5", "vo_Victory_1" */
	static void Cue(const FString& List)
	{
		TArray<FString> Parts;
		List.ParseIntoArray(Parts, TEXT("+"), true);
		for (FString P : Parts)
		{
			P.TrimStartAndEndInline();
			float Vol = 0.9f;
			FString Name = P, V;
			if (P.Split(TEXT(":"), &Name, &V)) Vol = FMath::Clamp(FCString::Atof(*V), 0.05f, 2.f);
			if (Name.StartsWith(TEXT("vo_"))) PlayVO(Name.Mid(3), Vol);
			else Sfx(*Name, Vol, 1.f);
		}
	}

	// ---- music: the place's own arena theme under the story screens (the menu loop is ducked while the story is open)
	static UAudioComponent* MenuMusic(AZoogiGameMode* G)
	{
		static FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(AZoogiGameMode::StaticClass(), TEXT("MusicComp"));
		return (P && G) ? Cast<UAudioComponent>(P->GetObjectPropertyValue_InContainer(G)) : nullptr;
	}
	static void DuckMenuMusic(bool bDuck)
	{
		if (UAudioComponent* M = MenuMusic(GM())) M->SetVolumeMultiplier(bDuck ? 0.001f : 0.35f * ZoogiUtil::GetMusicVolume());
	}
	static void StopMusic(float Fade)
	{
		if (UAudioComponent* A = GS.Music.Get()) { if (Fade > 0.f) A->FadeOut(Fade, 0.f); else A->Stop(); }
		GS.Music.Reset();
		GS.MusicKey.Reset();
	}
	static void PlayMusic(EZoogiArena A, const TCHAR* Part)
	{
		const FZoogiArenaMusicDef* Def = ZoogiMusic::DefFor(A);
		UWorld* Wd = GS.World.Get();
		if (!Def || !Def->Slug || !Wd || ZoogiUtil::GetMusicVolume() <= 0.001f) return; // no theme for this place: keep what plays
		const FString Key = FString(Def->Slug) + Part;
		if (Key == GS.MusicKey && GS.Music.IsValid()) return;
		USoundBase* Snd = ZoogiMusic::LoadMusic(Def->Slug, Part);
		if (!Snd) return;
		if (USoundWave* SW = Cast<USoundWave>(Snd)) SW->bLooping = true;
		StopMusic(0.5f);
		UAudioComponent* C = UGameplayStatics::CreateSound2D(Wd, Snd, 0.34f * ZoogiUtil::GetMusicVolume());
		if (!C) return;
		C->FadeIn(0.6f, 1.f);
		GS.Music.Reset(C);
		GS.MusicKey = Key;
	}

	static void Go(EScr S);
	static void SetEnv(EZoogiArena A, bool bUnder = false)
	{
		AZoogiGameMode* G = GM();
		ZoogiUnderrealm::SetPicked(false); // b203: the story decides
		const bool bFlip = ZoogiUnderrealm::On() != bUnder;
		ZoogiUnderrealm::Set(bUnder); // b178e
		if (G && G->Phase == EZoogiPhase::Menu && (G->MenuArena != A || bFlip))
		{
			UE_LOG(LogZoogi, Log, TEXT("STORY env -> %s"), *ZoogiUtil::ArenaName(A));
			G->MenuAction(TEXT("Arena"), (int32)A);
		}
		PlayMusic(A, TEXT("Base"));
	}
	static void Wipe(const FString& Shout, const FLinearColor& Col, TFunction<void()> Fn) { PlayWipe(GS.World.Get(), MoveTemp(Fn), Col, Shout); }
	static FLinearColor ChapterCol() { return GDB.Chapters.IsValidIndex(GS.Chapter) ? ElemCol(GDB.Chapters[GS.Chapter].Element) : Accent(); }

	// ---- FILM (Oct 30, Evan: "chop the episode up and tailor the game play to it ... the full unchopped episode must be unlocked
	// after completing it"): the buzzy Episode 1 film cut into parts that play inside the scenes (Dialogue.csv who=film,
	// line=<Content/Anime/Movies id>), and the full cut on the hub card once the episode is cleared (Chapters.csv film).
	struct FFilm
	{
		TStrongObjectPtr<UMediaPlayer> Player;
		TStrongObjectPtr<UMediaTexture> Tex;
		TStrongObjectPtr<UFileMediaSource> Src;
		TWeakObjectPtr<UMediaSoundComponent> Snd;
		FString Id;
		float T = 0.f;
		bool bOn = false, bFull = false, bStarted = false;
	};
	static FFilm GFilm;
	static FString FilmPath(const FString& Id) { return FPaths::ProjectContentDir() / TEXT("Anime/Movies") / (Id + TEXT(".mp4")); }
	static bool FilmExists(const FString& Id) { return !Id.IsEmpty() && FPaths::FileExists(FilmPath(Id)); }
	static void StopFilm()
	{
		if (GFilm.Player.IsValid()) GFilm.Player->Close();
		if (UMediaSoundComponent* Sc = GFilm.Snd.Get()) { Sc->Stop(); Sc->DestroyComponent(); }
		GFilm.Snd.Reset();
		if (GFilm.bOn) UE_LOG(LogZoogi, Log, TEXT("STORY film stop: %s at %.1fs"), *GFilm.Id, GFilm.T);
		GFilm.bOn = GFilm.bFull = GFilm.bStarted = false;
		GFilm.Id.Reset();
	}
	static bool StartFilm(const FString& Id, bool bFull)
	{
		StopFilm();
		if (!FilmExists(Id)) { UE_LOG(LogZoogi, Warning, TEXT("STORY film %s missing"), *Id); return false; }
		if (!GFilm.Player.IsValid())
		{
			GFilm.Player.Reset(NewObject<UMediaPlayer>());
			GFilm.Player->PlayOnOpen = true;
			GFilm.Player->SetLooping(false);
			GFilm.Tex.Reset(NewObject<UMediaTexture>());
			GFilm.Tex->AutoClear = true;
			GFilm.Tex->SetMediaPlayer(GFilm.Player.Get());
			GFilm.Tex->UpdateResource();
			GFilm.Src.Reset(NewObject<UFileMediaSource>());
		}
		if (AZoogiGameMode* G = GM())
		{	// the film's own soundtrack (voices + score)
			UMediaSoundComponent* Sc = NewObject<UMediaSoundComponent>(G);
			Sc->bIsUISound = true;
			Sc->bAllowSpatialization = false;
			Sc->SetMediaPlayer(GFilm.Player.Get());
			Sc->RegisterComponent();
			Sc->Start();
			GFilm.Snd = Sc;
		}
		GFilm.Src->SetFilePath(FilmPath(Id));
		if (!GFilm.Player->OpenSource(GFilm.Src.Get())) { StopFilm(); return false; }
		GFilm.Id = Id; GFilm.bOn = true; GFilm.bFull = bFull; GFilm.T = 0.f; GFilm.bStarted = false;
		StopMusic(0.3f);
		UE_LOG(LogZoogi, Log, TEXT("STORY film: %s%s"), *Id, bFull ? TEXT(" (full episode)") : TEXT(""));
		return true;
	}
	/** advance the film clock; true once it has played out (or never got going) */
	static bool TickFilm(float Dt)
	{
		if (!GFilm.bOn) return false;
		GFilm.T += Dt;
		UMediaPlayer* P = GFilm.Player.Get();
		if (!P) return true;
		if (P->IsPlaying()) GFilm.bStarted = true;
		if (!GFilm.bStarted) return GFilm.T > 8.f;
		const FTimespan D = P->GetDuration();
		return !P->IsReady() || (D > FTimespan::Zero() && P->GetTime() >= D - FTimespan::FromSeconds(0.15)) || GFilm.T > 900.f;
	}

	// flow (defined below the widget)
	static void Close();
	static void GoHub();
	static void PickChapter(int32 Ci);
	static void GoMap();
	static void TapSpot(int32 Si);
	static void Arrive(int32 Si);
	static void RollOut();
	static bool FirstStop();
	static bool GuideOn();
	static void PlayScene(const FString& Id, TFunction<void()> After);
	static void SceneDone();
	static void GoVS();
	static void StartMission();
	static void ResultButton(int32 B);
	static void ClearDone();
	static void Back();
}

// ------------------------------------------------------------------------------------------------ the screens (one Slate layer)
class SZoogiStory : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiStory) {}
	SLATE_END_ARGS()

	float T = 0.f, PrevT = 0.f;
	int32 Frame = 0;
	bool bTest = false;
	// dialogue
	float LineT = 0.f;
	int32 Shown = 0, LastBlip = 0;
	FString LeftWho, RightWho;
	float LeftIn = -10.f, RightIn = -10.f, FlashT = -10.f, ShakeT = -10.f;
	bool bBlack = false, bRed = false, bCold = false, bCrack = false;
	// map
	int32 TravelFrom = -1, TravelTo = -1;
	float TravelT = -10.f, PanelT = -10.f, DenyT = -10.f;
	int32 DenySpot = -1;

	struct FHit { FBox2D R; int32 Kind = 0; int32 Arg = 0; };
	mutable TArray<FHit> Hits;
	mutable float S = 1.f, DW = 1920.f, DH = 1080.f;
	mutable FVector2D Off = FVector2D::ZeroVector;
	mutable bool bPortrait = false;

	void Construct(const FArguments&)
	{
		bTest = ZoogiStorySave::Test();
		SetCanTick(true);
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual bool ComputeVolatility() const override { return true; }

	void Enter()
	{
		T = PrevT = 0.f; Frame = 0;
		TravelT = PanelT = DenyT = FlashT = ShakeT = LeftIn = RightIn = -10.f;
		TravelFrom = TravelTo = DenySpot = -1;
		bBlack = bRed = bCold = bCrack = false;
		if (ZoogiStoryDir::GS.Scr == ZoogiStoryDir::EScr::Dialogue) { LeftWho.Reset(); RightWho.Reset(); StartLine(); }
	}
	bool Cross(float X) const { return PrevT < X && T >= X; }

	// ---- dialogue
	const ZoogiStoryData::FLine* CurLine() const
	{
		const TArray<ZoogiStoryData::FLine>* L = ZoogiStoryData::GDB.Scenes.Find(ZoogiStoryDir::GS.Scene);
		return (L && L->IsValidIndex(ZoogiStoryDir::GS.Line)) ? &(*L)[ZoogiStoryDir::GS.Line] : nullptr;
	}
	bool IsSpeaker(const FString& W) const { return W != TEXT("narrator") && W != TEXT("title"); }
	void StartLine()
	{
		using namespace ZoogiStoryDir;
		const ZoogiStoryData::FLine* L = CurLine();
		LineT = 0.f; Shown = 0; LastBlip = 0;
		bBlack = bRed = bCold = bCrack = false;
		if (!L) return;
		ZoogiAnimeCues::OnStoryLine(GS.World.Get(), GS.Scene, GS.Line); // b164: a director cue placed before this line
		if (L->Who == TEXT("film") && ZoogiAnimeCues::StoryOwnsFilms()) { Shown = L->Text.Len(); return; } // b164: the director's cues replace the film rows
		if (L->Who == TEXT("film")) { Shown = L->Text.Len(); StartFilm(L->Text, false); return; } // a missing part is skipped next tick
		TArray<FString> Fx;
		L->Fx.ParseIntoArray(Fx, TEXT(","), true);
		for (FString F : Fx)
		{
			F.TrimStartAndEndInline();
			if (F == TEXT("flash")) FlashT = T;
			else if (F == TEXT("shake")) ShakeT = T;
			else if (F == TEXT("black")) bBlack = true;
			else if (F == TEXT("red")) bRed = true;
			else if (F == TEXT("cold")) bCold = true;
			else if (F == TEXT("crack")) bCrack = true;
			else if (F.StartsWith(TEXT("env="))) SetEnv(ZoogiStoryData::ArenaOf(F.Mid(4)), F.Mid(4).Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase));
		}
		if (IsSpeaker(L->Who))
		{
			const ZoogiStoryData::FWho* W = ZoogiStoryData::Who(L->Who);
			const bool bRight = W && W->bRight;
			FString& Slot = bRight ? RightWho : LeftWho;
			if (Slot != L->Who) { Slot = L->Who; (bRight ? RightIn : LeftIn) = T; Sfx(TEXT("Dash"), 0.32f, 1.5f); }
		}
		if (L->Who == TEXT("title")) { Shown = L->Text.Len(); if (L->Sfx.IsEmpty()) Sfx(TEXT("Clack"), 1.f, 0.9f); }
		Cue(L->Sfx);
	}
	void Advance()
	{
		using namespace ZoogiStoryDir;
		const ZoogiStoryData::FLine* L = CurLine();
		if (L && L->Who == TEXT("film")) StopFilm();
		else if (L && Shown < L->Text.Len()) { Shown = L->Text.Len(); return; }
		++GS.Line;
		if (!CurLine()) { SceneDone(); return; }
		Sfx(TEXT("UiClick"), 0.35f, 1.3f);
		StartLine();
	}
	void TickDialogue(float Dt)
	{
		const ZoogiStoryData::FLine* L = CurLine();
		if (!L) return;
		if (ZoogiAnimeCues::Busy()) return; // b164: the story waits while a director cue plays
		LineT += Dt;
		if (L->Who == TEXT("film") && ZoogiAnimeCues::StoryOwnsFilms()) { Advance(); return; } // b164
		if (L->Who == TEXT("film")) { if (!ZoogiStoryDir::GFilm.bOn || ZoogiStoryDir::TickFilm(Dt) || (bTest && LineT > 4.f)) Advance(); return; }
		const int32 Len = L->Text.Len();
		const float Cps = L->Who == TEXT("narrator") ? 34.f : 44.f;
		const int32 Want = FMath::Min(Len, FMath::FloorToInt(LineT * Cps));
		if (Want > Shown)
		{
			Shown = Want;
			if (IsSpeaker(L->Who) && Shown - LastBlip >= 2 && Shown < Len && L->Text[Shown - 1] != TEXT(' '))
			{
				LastBlip = Shown;
				const ZoogiStoryData::FWho* W = ZoogiStoryData::Who(L->Who);
				const float P = (W ? W->Pitch : 1.f) * (0.94f + 0.12f * ((Hash(Shown * 31u) % 100u) / 100.f));
				ZoogiStoryDir::Sfx(TEXT("UiClick"), 0.16f, P);
			}
		}
		if (bTest && Shown >= Len && LineT > Len / Cps + (L->Who == TEXT("title") ? 1.6f : 0.75f)) Advance();
	}

	// ---- input
	FVector2D ToDesign(const FGeometry& G, const FVector2D& Screen) const { return (G.AbsoluteToLocal(Screen) - Off) / FMath::Max(S, 0.01f); }
	FReply Press(const FGeometry& G, const FVector2D& Screen)
	{
		using namespace ZoogiStoryDir;
		if (WipeBusy() || T < 0.25f) return FReply::Handled();
		if (GFilm.bOn)
		{	// films: tap to skip (after a second)
			if (GFilm.T > 1.f) { Sfx(TEXT("UiCancel"), 0.6f); if (GFilm.bFull) StopFilm(); else Advance(); }
			return FReply::Handled();
		}
		const FVector2D P = ToDesign(G, Screen);
		const FHit* Hit = nullptr;
		for (int32 i = Hits.Num() - 1; i >= 0; --i) if (Hits[i].R.bIsValid && Hits[i].R.IsInside(P)) { Hit = &Hits[i]; break; }
		const int32 Kind = Hit ? Hit->Kind : 0, Arg = Hit ? Hit->Arg : 0;
		switch (GS.Scr)
		{
		case EScr::Hub:
			if (Kind == 4) Back();
			else if (Kind == 8) { if (GDB.Chapters.IsValidIndex(Arg) && StartFilm(GDB.Chapters[Arg].Film, true)) Sfx(TEXT("UiConfirm"), 0.9f); }
			else if (Kind == 1) PickChapter(Arg);
			break;
		case EScr::Card: GoMap(); break;
		case EScr::Map:
			if (Kind == 4) Back();
			else if (Kind == 3) RollOut();
			else if (Kind == 9) { GS.GuidePick = GuideOn() ? 0 : 1; Sfx(TEXT("Clack"), 0.9f, GuideOn() ? 1.2f : 0.9f); } // b67: GUIDE toggle
			else if (Kind == 2) TapSpot(Arg);
			break;
		case EScr::Dialogue: if (Kind == 5) { Sfx(TEXT("UiCancel"), 0.7f); SceneDone(); } else Advance(); break;
		case EScr::VS: if (T > 0.6f) StartMissionWipe(); break;
		case EScr::Results: if (Kind == 6) ResultButton(Arg); break;
		case EScr::Clear: if (T > 1.2f) ClearDone(); break;
		default: break;
		}
		return FReply::Handled();
	}
	void StartMissionWipe()
	{
		using namespace ZoogiStoryDir;
		Sfx(TEXT("UiConfirm"), 0.9f);
		Wipe(TEXT("LET'S ROLL!"), ChapterCol(), []() { StartMission(); });
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Handled() : Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { return Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }

	virtual void Tick(const FGeometry& G, const double Now, const float Dt) override
	{
		using namespace ZoogiStoryDir;
		const float Step = bTest ? 1.f / 24.f : FMath::Min(Dt, 0.05f);
		PrevT = T;
		T += Step;
		++Frame;
		if (GFilm.bOn && GFilm.bFull) { if (TickFilm(Step)) StopFilm(); return; }
		switch (GS.Scr)
		{
		case EScr::Hub:
			for (int32 i = 0; i < GDB.Chapters.Num(); ++i) if (Cross(0.2f + 0.07f * i)) Sfx(TEXT("Clack"), 0.55f, 1.1f + 0.08f * i);
			break;
		case EScr::Card:
			if (Cross(0.01f) && GDB.Chapters.IsValidIndex(GS.Chapter)) Sfx(*(TEXT("Sting") + GDB.Chapters[GS.Chapter].Element), 1.f);
			if (Cross(0.3f)) { Sfx(TEXT("CritBoom"), 0.8f); Sfx(TEXT("Clack"), 1.f, 0.85f); }
			if (Cross(3.4f)) GoMap();
			break;
		case EScr::Map:
			for (int32 i = 0; i < 3; ++i) if (Cross(0.25f + 0.12f * i)) Sfx(TEXT("Clack"), 0.5f, 1.2f + 0.1f * i);
			if (TravelT > 0.f)
			{
				if (FMath::Fmod(T - TravelT, 0.2f) < Step) Sfx(TEXT("HitClick"), 0.3f, 1.6f);
				if (T - TravelT >= 0.85f) { const int32 To = TravelTo; TravelT = -10.f; Sfx(TEXT("Clack"), 0.9f); Arrive(To); }
			}
			break;
		case EScr::Dialogue: TickDialogue(Step); break;
		case EScr::VS:
			if (Cross(0.12f)) Sfx(TEXT("Clack"), 0.9f, 1.1f);
			if (Cross(0.22f)) Sfx(TEXT("Clack"), 0.9f, 0.9f);
			if (Cross(0.36f))
			{
				Sfx(TEXT("CritBoom"), 1.f); Sfx(TEXT("Zap"), 0.7f);
				if (const FMission* M = GDB.Missions.Find(GS.Mission)) if (const FZoogiArenaMusicDef* Def = ZoogiMusic::DefFor(M->Env)) if (Def->Slug) PlayVO(FString::Printf(TEXT("Intro_%s_1"), Def->Slug));
			}
			if (Cross(0.6f)) Sfx(TEXT("CrowdWave1"), 0.8f);
			break;
		case EScr::Results:
			if (GS.bWon) { if (Cross(0.05f)) Sfx(TEXT("Win"), 1.f); if (Cross(0.25f)) Sfx(TEXT("CrowdWave2"), 0.9f); if (Cross(0.6f)) PlayVO(TEXT("Victory_1")); if (Cross(1.1f)) Sfx(TEXT("Score"), 0.8f); }
			else { if (Cross(0.05f)) Sfx(TEXT("Fall"), 0.9f); if (Cross(0.5f)) PlayVO(TEXT("Defeat_1")); if (Cross(0.9f)) Sfx(TEXT("StingVoid"), 0.6f, 0.8f); }
			break;
		case EScr::Clear:
			if (Cross(0.05f)) { Sfx(TEXT("Win"), 1.f); if (GDB.Chapters.IsValidIndex(GS.Chapter)) Sfx(*(TEXT("Sting") + GDB.Chapters[GS.Chapter].Element), 1.f); }
			if (Cross(0.35f)) { Sfx(TEXT("CritBoom"), 0.9f); Sfx(TEXT("CrowdWave2"), 1.f); }
			break;
		default: break;
		}
	}

	// ---- paint helpers
	static FVector2D Rot(float AngDeg, const FVector2D& V) { const float R = FMath::DegreesToRadians(AngDeg); return FVector2D(V.X * FMath::Cos(R) - V.Y * FMath::Sin(R), V.X * FMath::Sin(R) + V.Y * FMath::Cos(R)); }
	/** texture cropped to cover the rotated box (no stretching) */
	static void TexCover(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* Tx, const FVector2D& C, const FVector2D& Sz, float Ang, const FLinearColor& Col, const FVector2D& Focus = FVector2D(0.5f, 0.5f))
	{
		if (!Tx) return;
		const float Ta = (float)Tx->GetSizeX() / FMath::Max(1, Tx->GetSizeY()), Ba = Sz.X / FMath::Max(1.f, Sz.Y);
		FVector2D Uv(1.f, 1.f);
		if (Ta > Ba) Uv.X = Ba / Ta; else Uv.Y = Ta / Ba;
		const FVector2D U0(FMath::Clamp(Focus.X - Uv.X * 0.5f, 0.f, 1.f - Uv.X), FMath::Clamp(Focus.Y - Uv.Y * 0.5f, 0.f, 1.f - Uv.Y));
		const FVector2D Hx = Rot(Ang, FVector2D(Sz.X * 0.5f, 0.f)), Hy = Rot(Ang, FVector2D(0.f, Sz.Y * 0.5f));
		const FVector2D P[4] = { C - Hx - Hy, C + Hx - Hy, C + Hx + Hy, C - Hx + Hy };
		const FVector2D UV[4] = { U0, U0 + FVector2D(Uv.X, 0.f), U0 + Uv, U0 + FVector2D(0.f, Uv.Y) };
		TexQuad(Out, G, L, Tx, P, UV, Col);
	}
	static TArray<FString> Wrap(const FString& Str, const FSlateFontInfo& F, float MaxW)
	{
		TArray<FString> Words, Lines;
		Str.ParseIntoArray(Words, TEXT(" "), true);
		FString Cur;
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (!Cur.IsEmpty() && Measure(Try, F).X > MaxW) { Lines.Add(Cur); Cur = Wd; }
			else Cur = Try;
		}
		if (!Cur.IsEmpty()) Lines.Add(Cur);
		return Lines;
	}
	/** wrapped text, left-aligned from TL, first N characters only (typewriter); returns the height used */
	static float TextBlock(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& Str, const FSlateFontInfo& F, const FVector2D& TL, float MaxW, int32 N, const FLinearColor& Col, float LineH)
	{
		const TArray<FString> Lines = Wrap(Str, F, MaxW);
		int32 Left = N;
		float Y = TL.Y;
		for (const FString& Ln : Lines)
		{
			if (Left <= 0) break;
			const FString Part = Ln.Left(Left);
			const FVector2D Sz = Measure(Part, F);
			Text(Out, G, L, Part, F, FVector2D(TL.X + Sz.X * 0.5f, Y + LineH * 0.5f), 0.f, Col);
			Left -= Ln.Len() + 1;
			Y += LineH;
		}
		return Lines.Num() * LineH;
	}
	/** ransom title scaled to fit a width */
	static float FitBase(const FString& Str, float Want, float MaxW, uint32 Seed) { const float W1 = RansomWidth(Str, 100.f, Seed); return W1 > 0.f ? FMath::Min(Want, 100.f * MaxW / W1) : Want; }
	void Bg(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FLinearColor& Col, const FVector2D& RayC, float Spin, float Bright) const
	{
		Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, FLinearColor::FromSRGBColor(FColor(8, 6, 10)));
		const int32 N = 18;
		const float R = FMath::Max(DW, DH) * 1.6f;
		for (int32 k = 0; k < N; k += 2)
		{
			const float A0 = FMath::DegreesToRadians(T * Spin + 360.f * k / N), A1 = FMath::DegreesToRadians(T * Spin + 360.f * (k + 1) / N);
			Tri(Out, G, L + 1, RayC, RayC + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, RayC + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, (Col * ((k % 4 == 0) ? 0.55f : 0.32f) * Bright).CopyWithNewOpacity(1.f));
		}
		const float Drift = FMath::Fmod(T * 30.f, 64.f);
		Tiled(Out, G, L + 2, DotsTex(), FVector2D(-64.f + Drift, -64.f + Drift * 0.5f), FVector2D(DW + 128.f, DH + 128.f), 64.f, FLinearColor(0.f, 0.f, 0.f, 0.35f));
	}
	/** darkening veil over the live 3D world (stacked strips = a cheap gradient) */
	void Veil(FSlateWindowElementList& Out, const FGeometry& G, int32 L, float Base, float Edge) const
	{
		Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, FLinearColor(0.f, 0.f, 0.f, Base));
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = Edge * (1.f - k / 8.f) * 0.22f;
			const float H = DH * 0.05f;
			Box(Out, G, L, FVector2D(DW * 0.5f, H * (k + 0.5f)), FVector2D(DW + 40.f, H), 0.f, FLinearColor(0.f, 0.f, 0.f, A));
			Box(Out, G, L, FVector2D(DW * 0.5f, DH - H * (k + 0.5f)), FVector2D(DW + 40.f, H), 0.f, FLinearColor(0.f, 0.f, 0.f, A));
		}
	}
	void PTag(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& Str, const FVector2D& C, float Size, float Ang, const FLinearColor& BoxC, const FLinearColor& TxtC, const FLinearColor& Shadow, int32 FontIdx = 0) const
	{
		const FSlateFontInfo F = Font(FontIdx, Size);
		const FVector2D Sz = Measure(Str, F) + FVector2D(Size * 0.8f, Size * 0.3f);
		Box(Out, G, L, C + FVector2D(Size * 0.14f, Size * 0.14f), Sz, Ang, Shadow);
		Box(Out, G, L + 1, C, Sz, Ang, BoxC);
		Text(Out, G, L + 2, Str, F, C, Ang, TxtC);
	}
	void Button(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& Label, const FVector2D& C, int32 Style, int32 Kind, int32 Arg, float In) const
	{
		const FVector2D Sz(Style == 0 ? 420.f : 300.f, 96.f);
		const float P = Style == 0 ? 1.f + 0.04f * FMath::Sin(T * 7.f) : 1.f;
		const FVector2D Cc = C + FVector2D((1.f - EaseOutBack(In)) * 900.f, 0.f);
		const FLinearColor BoxC = Style == 0 ? Accent() : FLinearColor::White, TxtC = Style == 0 ? FLinearColor::White : FLinearColor::Black;
		Box(Out, G, L, Cc + FVector2D(10.f, 10.f), Sz * P, -5.f, Style == 0 ? FLinearColor::Black : Accent());
		Box(Out, G, L + 1, Cc, Sz * P, -5.f, BoxC);
		Text(Out, G, L + 2, Label, Font(0, 56.f * P), Cc, -5.f, TxtC);
		Hits.Add({ FBox2D(Cc - Sz * 0.55f, Cc + Sz * 0.55f), Kind, Arg });
	}
	void BackTag(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		const FVector2D BC(DW - 170.f, DH - (bPortrait ? 150.f : 100.f));
		Box(Out, G, L, BC + FVector2D(8.f, 8.f), FVector2D(230.f, 78.f), 7.f, Accent());
		Box(Out, G, L + 1, BC, FVector2D(230.f, 78.f), 7.f, FLinearColor::White);
		Text(Out, G, L + 2, TEXT("BACK"), Font(0, 54.f), BC, 7.f, FLinearColor::Black);
		Hits.Add({ FBox2D(BC - FVector2D(125.f, 45.f), BC + FVector2D(125.f, 45.f)), 4, 0 });
	}
	/** a face in a tilted white frame over a jagged burst (Dim < 1 = the listener in a two-shot; bShadow = silhouette) */
	void FaceCard(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* F, const FVector2D& C, float Size, float Ang, const FLinearColor& Col, float Dim, bool bShadow) const
	{
		Tex(Out, G, L, BurstTex(), C + FVector2D(10.f, 6.f), FVector2D(Size, Size) * 1.45f, T * 22.f, Col.CopyWithNewOpacity(Dim));
		Box(Out, G, L + 1, C + FVector2D(Size * 0.05f, Size * 0.06f), FVector2D(Size, Size) * 1.06f, Ang, FLinearColor::Black);
		Box(Out, G, L + 2, C, FVector2D(Size, Size) * 1.06f, Ang, bShadow ? Col : FLinearColor::White);
		const FLinearColor Tint = bShadow ? FLinearColor(0.07f, 0.02f, 0.04f, 1.f) : FLinearColor(Dim, Dim, Dim, 1.f);
		if (F) TexCover(Out, G, L + 3, F, C, FVector2D(Size, Size), Ang, Tint, FVector2D(0.5f, 0.42f));
		else Box(Out, G, L + 3, C, FVector2D(Size, Size), Ang, Col * 0.4f);
		if (bShadow)
		{	// two red eyes in the dark
			const FVector2D E = Rot(Ang, FVector2D(Size * 0.13f, 0.f));
			const float Bl = 0.75f + 0.25f * FMath::Sin(T * 5.f);
			Box(Out, G, L + 4, C - E + FVector2D(0.f, -Size * 0.05f), FVector2D(Size * 0.09f, Size * 0.035f), Ang - 8.f, FLinearColor(1.f, 0.1f, 0.15f, Bl));
			Box(Out, G, L + 4, C + E + FVector2D(0.f, -Size * 0.05f), FVector2D(Size * 0.09f, Size * 0.035f), Ang + 8.f, FLinearColor(1.f, 0.1f, 0.15f, Bl));
		}
		const FVector2D Up = Rot(Ang, FVector2D(0.f, -Size * 0.53f - 6.f));
		Tex(Out, G, L + 4, TornTex(), C + Up, FVector2D(Size + 50.f, 64.f), Ang + 180.f, bShadow ? Col : FLinearColor::White);
	}

	// ---- FILM: full screen, letterboxed, SKIP tag after a second
	void PaintFilm(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryDir;
		Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(60.f, 60.f), 0.f, FLinearColor::Black);
		UMediaTexture* Tx = GFilm.Tex.Get();
		if (Tx && GFilm.bStarted)
		{
			const float Ar = (Tx->GetWidth() > 0 && Tx->GetHeight() > 0) ? (float)Tx->GetWidth() / (float)Tx->GetHeight() : 16.f / 9.f;
			FVector2D Sz(DW, DW / Ar);
			if (Sz.Y > DH) Sz = FVector2D(DH * Ar, DH);
			FSlateBrush B; B.SetResourceObject(Tx); B.ImageSize = Sz;
			Box(Out, G, L + 1, FVector2D(DW, DH) * 0.5f, Sz, 0.f, FLinearColor::White, &B);
		}
		if (GFilm.T > 1.f) PTag(Out, G, L + 2, TEXT("SKIP >>"), FVector2D(DW - 140.f, DH - (bPortrait ? 150.f : 70.f)), 30.f, -4.f, FLinearColor::White, FLinearColor::Black, Accent());
	}

	// ---- HUB: the four episodes
	int32 PaintHub(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		const FLinearColor Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		Bg(Out, G, L, Acc, FVector2D(DW * 0.5f, DH * 0.62f), 7.f, 1.f);
		L += 4;
		const FVector2D TP = bPortrait ? FVector2D(56.f, 120.f) : FVector2D(64.f, 96.f);
		const float Wa = Ransom(Out, G, L, TEXT("STORY"), TP.X, TP.Y, 74.f, -7.f, 11u, T - 0.05f, Acc);
		Ransom(Out, G, L, TEXT("MODE"), TP.X + Wa * 0.35f, TP.Y + 104.f, 60.f, -7.f, 23u, T - 0.25f, Acc);
		if (T > 0.5f)
		{
			const FString Tl = TEXT("THE RINGS WERE NEVER A GAME.");
			const FSlateFontInfo F = Font(2, bPortrait ? 30.f : 34.f);
			const FVector2D Sz = Measure(Tl, F);
			const FVector2D C = bPortrait ? FVector2D(DW - Sz.X * 0.5f - 60.f, 300.f) : FVector2D(TP.X + Wa + 120.f + Sz.X * 0.5f, TP.Y + 40.f);
			Box(Out, G, L + 3, C + FVector2D(6.f, 6.f), Sz + FVector2D(36.f, 18.f), -3.f, Acc);
			Box(Out, G, L + 4, C, Sz + FVector2D(36.f, 18.f), -3.f, Blk);
			Text(Out, G, L + 5, Tl, F, C, -3.f, Wht);
		}
		L += 8;
		const int32 N = GDB.Chapters.Num();
		int32 Current = -1;
		for (int32 i = 0; i < N; ++i) if (ZoogiStorySave::ChapterOpen(i) && ZoogiStorySave::Done(i) < ZoogiStorySave::Total(i)) { Current = i; break; }
		static const float Tilts[] = { -4.f, 3.f, -3.f, 4.f };
		for (int32 i = 0; i < N; ++i)
		{
			const FChapter& Ch = GDB.Chapters[i];
			const FLinearColor Col = ElemCol(Ch.Element);
			FVector2D C, Sz;
			if (bPortrait)
			{
				const float CH = FMath::Min(640.f, (DH - 560.f) * 0.5f - 50.f);
				Sz = FVector2D(450.f, CH);
				C = FVector2D(540.f + ((i % 2) ? 245.f : -245.f), 420.f + CH * 0.5f + (i / 2) * (CH + 70.f));
			}
			else
			{
				const float Sp = FMath::Min(430.f, (DW - 100.f) / FMath::Max(1, N));
				Sz = FVector2D(Sp - 46.f, FMath::Min(640.f, (Sp - 46.f) * 1.65f));
				C = FVector2D(DW * 0.5f + (i - (N - 1) * 0.5f) * Sp, DH * 0.6f);
			}
			const bool bOpen = ZoogiStorySave::ChapterOpen(i);
			const int32 Done = ZoogiStorySave::Done(i), Tot = ZoogiStorySave::Total(i);
			const bool bCur = i == Current;
			const float In = EaseOutBack((T - 0.12f - 0.07f * i) / 0.32f);
			C.Y += (1.f - In) * DH;
			const float Ang = Tilts[i % 4] + (bCur ? 0.8f * FMath::Sin(T * 9.f) : 0.f);
			if (bCur) Sz *= 1.04f + 0.015f * FMath::Sin(T * 4.f);
			Box(Out, G, L, C + FVector2D(18.f, 20.f), Sz + FVector2D(26.f, 26.f), Ang, Blk);
			Box(Out, G, L + 1, C + FVector2D(-12.f, -10.f), Sz + FVector2D(26.f, 26.f), Ang, bOpen ? Col : FLinearColor(0.2f, 0.2f, 0.22f));
			Box(Out, G, L + 2, C, Sz + FVector2D(20.f, 20.f), Ang, bOpen ? Wht : FLinearColor(0.45f, 0.45f, 0.48f));
			TexCover(Out, G, L + 3, Portrait(Ch.Bg), C, Sz, Ang, bOpen ? Wht : FLinearColor(0.18f, 0.18f, 0.2f));
			Box(Out, G, L + 4, C + Rot(Ang, FVector2D(0.f, Sz.Y * 0.3f)), FVector2D(Sz.X, Sz.Y * 0.42f), Ang, FLinearColor(0.f, 0.f, 0.f, 0.55f));
			// the episode's face
			const float FS = Sz.X * 0.62f;
			const FVector2D FC = C + Rot(Ang, FVector2D(0.f, -Sz.Y * 0.06f));
			if (bOpen) FaceCard(Out, G, L + 5, ZoogiStoryData::StoryArt(Ch.Hero), FC, FS, Ang + 4.f, Col, 1.f, false);
			else FaceCard(Out, G, L + 5, ZoogiStoryData::StoryArt(Ch.Hero), FC, FS, Ang + 4.f, FLinearColor(0.3f, 0.3f, 0.32f), 0.6f, true);
			// tags
			PTag(Out, G, L + 10, Ch.Ep, C + Rot(Ang, FVector2D(-Sz.X * 0.18f, -Sz.Y * 0.44f)), 30.f, Ang - 5.f, Blk, bOpen ? Col : Wht, bOpen ? Col : Blk);
			const FSlateFontInfo TF = Font(0, FMath::Min(60.f, Sz.X * 0.16f));
			const TArray<FString> TL = Wrap(Ch.Title, TF, Sz.X - 40.f);
			for (int32 k = 0; k < TL.Num(); ++k)
			{
				const FVector2D LC = C + Rot(Ang, FVector2D(0.f, Sz.Y * 0.27f + (k - (TL.Num() - 1) * 0.5f) * TF.Size * 1.1f));
				const FVector2D Ts = Measure(TL[k], TF);
				Box(Out, G, L + 13, LC, Ts + FVector2D(24.f, 4.f), Ang, Blk);
				Text(Out, G, L + 14, TL[k], TF, LC, Ang, Wht);
			}
			for (int32 k = 0; k < Tot; ++k)
			{
				const FVector2D PC = C + Rot(Ang, FVector2D((k - (Tot - 1) * 0.5f) * 40.f, Sz.Y * 0.44f));
				Box(Out, G, L + 13, PC, FVector2D(26.f, 26.f), 45.f + Ang, Blk);
				Box(Out, G, L + 14, PC, FVector2D(18.f, 18.f), 45.f + Ang, k < Done ? Col : FLinearColor(1.f, 1.f, 1.f, 0.3f));
			}
			if (!bOpen)
			{
				TiledRot(Out, G, L + 15, DotsTex(), C, Sz, Ang, 26.f, Acc.CopyWithNewOpacity(0.18f));
				PTag(Out, G, L + 16, TEXT("LOCKED"), C + Rot(Ang, FVector2D(0.f, -Sz.Y * 0.06f)), 52.f, Ang - 6.f, Wht, Blk, Acc);
				PTag(Out, G, L + 16, FString::Printf(TEXT("CLEAR EPISODE %d"), i), C + Rot(Ang, FVector2D(0.f, Sz.Y * 0.08f)), 22.f, Ang - 3.f, Blk, Wht, Acc, 1);
				if (DenySpot == 100 + i && T - DenyT < 0.4f) Box(Out, G, L + 17, C, Sz, Ang, FLinearColor(1.f, 0.f, 0.2f, 0.4f * (1.f - (T - DenyT) / 0.4f)));
			}
			else if (Tot > 0 && Done >= Tot)
			{
				const FVector2D SC = C + Rot(Ang, FVector2D(Sz.X * 0.3f, -Sz.Y * 0.32f));
				Tex(Out, G, L + 16, BurstTex(), SC, FVector2D(150.f, 150.f), 12.f, Blk);
				Tex(Out, G, L + 17, BurstTex(), SC, FVector2D(132.f, 132.f), 12.f, Col);
				Text(Out, G, L + 18, TEXT("CLEAR!"), Font(3, 40.f, 3.f), SC, -12.f, Wht);
			}
			if (bCur && T > 0.6f)
			{
				const float P = 1.f + 0.08f * FMath::Sin(T * 10.f);
				const FVector2D GC = C + Rot(Ang, FVector2D(Sz.X * 0.34f, -Sz.Y * 0.33f));
				Tex(Out, G, L + 16, BurstTex(), GC, FVector2D(150.f, 150.f) * P, T * 40.f, Blk);
				Tex(Out, G, L + 17, BurstTex(), GC, FVector2D(134.f, 134.f) * P, T * 40.f, Acc);
				Text(Out, G, L + 18, TEXT("GO!"), Font(3, 56.f * P, 3.f), GC, -12.f, Wht);
			}
			Hits.Add({ FBox2D(C - Sz * 0.5f, C + Sz * 0.5f), 1, i });
			if (Tot > 0 && Done >= Tot && ZoogiStoryDir::FilmExists(Ch.Film) && T > 0.5f)
			{	// the full, unchopped episode film: unlocked by clearing the episode
				const FVector2D BC = C + Rot(Ang, FVector2D(0.f, Sz.Y * 0.5f + (bPortrait ? 40.f : 62.f)));
				const float P = 1.f + 0.05f * FMath::Sin(T * 7.f);
				const FString Lb = TEXT("WATCH FULL EPISODE");
				const FSlateFontInfo F = Font(0, 30.f * P);
				const FVector2D Ts = Measure(Lb, F) + FVector2D(40.f, 16.f);
				Box(Out, G, L + 18, BC + FVector2D(7.f, 7.f), Ts, Ang - 3.f, Blk);
				Box(Out, G, L + 19, BC, Ts, Ang - 3.f, Acc);
				Text(Out, G, L + 19, Lb, F, BC, Ang - 3.f, Wht);
				Hits.Add({ FBox2D(BC - Ts * 0.62f, BC + Ts * 0.62f), 8, i });
			}
		}
		L += 20;
		BackTag(Out, G, L);
		return L + 4;
	}

	// ---- CARD: the episode title card
	int32 PaintCard(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		if (!GDB.Chapters.IsValidIndex(ZoogiStoryDir::GS.Chapter)) return L;
		const FChapter& Ch = GDB.Chapters[ZoogiStoryDir::GS.Chapter];
		const FLinearColor Col = ElemCol(Ch.Element), Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, Col);
		TexCover(Out, G, L + 1, Portrait(Ch.Bg), FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, FLinearColor(1.f, 1.f, 1.f, 0.35f));
		const float Drift = FMath::Fmod(T * 40.f, 64.f);
		Tiled(Out, G, L + 2, DotsTex(), FVector2D(-64.f + Drift, -64.f), FVector2D(DW + 128.f, DH + 128.f), 48.f, FLinearColor(0.f, 0.f, 0.f, 0.3f));
		// the black slab slams across
		{
			const float In = EaseOutBack(T / 0.28f);
			const float X = FMath::Lerp(-DW, 0.f, In);
			const float Sk = DH * 0.18f;
			if (bPortrait) Quad(Out, G, L + 3, FVector2D(-40.f + X, 120.f), FVector2D(DW + 40.f + X, 40.f), FVector2D(DW + 40.f + X, 620.f), FVector2D(-40.f + X, 700.f), Blk);
			else Quad(Out, G, L + 3, FVector2D(-40.f + X, -40.f), FVector2D(DW * 0.62f + Sk + X, -40.f), FVector2D(DW * 0.62f - Sk + X, DH + 40.f), FVector2D(-40.f + X, DH + 40.f), Blk);
		}
		// the face
		{
			const float In = EaseOutBack((T - 0.12f) / 0.32f);
			const FVector2D C = bPortrait ? FVector2D(540.f, DH * 0.64f) : FVector2D(DW * 0.74f, DH * 0.52f);
			const float Sz = bPortrait ? 760.f : 640.f;
			FaceCard(Out, G, L + 4, ZoogiStoryData::StoryArt(Ch.Hero), C + FVector2D((1.f - In) * 1400.f, 0.f), Sz, 5.f, Wht, 1.f, false);
		}
		// titles
		const float X0 = bPortrait ? 60.f : 90.f;
		const float MaxW = bPortrait ? DW - 120.f : DW * 0.5f;
		const float Y0 = bPortrait ? 210.f : DH * 0.3f;
		Ransom(Out, G, L + 12, Ch.Ep, X0, Y0, 54.f, -6.f, 5u, T - 0.15f, Col);
		const float B = FitBase(Ch.Title, 116.f, MaxW, 9u);
		Ransom(Out, G, L + 16, Ch.Title, X0, Y0 + 70.f + B * 0.9f, B, -6.f, 9u, T - 0.3f, Acc);
		if (T > 0.8f)
		{
			const FSlateFontInfo F = Font(2, bPortrait ? 34.f : 36.f);
			const int32 N = FMath::FloorToInt((T - 0.8f) * 40.f);
			const FVector2D TL(X0 + 10.f, Y0 + 120.f + B * 1.6f);
			const TArray<FString> Lines = Wrap(Ch.Sub, F, MaxW);
			Box(Out, G, L + 20, TL + FVector2D(MaxW * 0.5f - 10.f, Lines.Num() * F.Size * 0.7f + 4.f), FVector2D(MaxW + 20.f, Lines.Num() * F.Size * 1.4f + 30.f), -1.5f, FLinearColor(0.f, 0.f, 0.f, 0.75f));
			TextBlock(Out, G, L + 21, Ch.Sub, F, TL + FVector2D(10.f, 10.f), MaxW - 20.f, N, Wht, F.Size * 1.4f);
		}
		if (T > 1.4f) Ransom(Out, G, L + 24, TEXT("TAP TO GO"), X0, DH - (bPortrait ? 120.f : 90.f), 30.f * (1.f + 0.05f * FMath::Sin(T * 8.f)), -4.f, 41u, 1.f, Acc);
		return L + 30;
	}

	// ---- MAP: tap to travel
	FVector2D NodePos(int32 k, int32 N) const
	{
		if (bPortrait)
		{
			const float Top = 330.f, Bot = DH - 470.f;
			return FVector2D((k % 2) ? 760.f : 320.f, Top + (k + 0.5f) / FMath::Max(1, N) * (Bot - Top));
		}
		const float X = DW * (0.2f + 0.6f * (N > 1 ? (float)k / (N - 1) : 0.5f));
		return FVector2D(X, (k % 2) ? DH * 0.4f : DH * 0.6f);
	}
	FVector2D TokenPos(const TArray<int32>& Sp) const
	{
		using namespace ZoogiStoryDir;
		const int32 Cur = FMath::Max(0, Sp.IndexOfByKey(GS.Spot));
		if (TravelT > 0.f)
		{
			const int32 A = FMath::Max(0, Sp.IndexOfByKey(TravelFrom)), B = FMath::Max(0, Sp.IndexOfByKey(TravelTo));
			const float K = EaseInOut((T - TravelT) / 0.85f);
			const FVector2D P = FMath::Lerp(NodePos(A, Sp.Num()), NodePos(B, Sp.Num()), K);
			return P + FVector2D(0.f, -FMath::Sin(K * UE_PI) * 120.f);
		}
		return NodePos(Cur, Sp.Num());
	}
	int32 PaintMap(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		using namespace ZoogiStoryDir;
		if (!GDB.Chapters.IsValidIndex(GS.Chapter)) return L;
		const FChapter& Ch = GDB.Chapters[GS.Chapter];
		const FLinearColor Col = ElemCol(Ch.Element), Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		const TArray<int32> Sp = ChapterSpots(GS.Chapter);
		const int32 N = Sp.Num();
		Veil(Out, G, L, 0.18f, 1.f);
		L += 2;
		// header
		{
			const float In = EaseOutBack(T / 0.3f);
			const FVector2D HC(-40.f + (1.f - In) * -700.f, bPortrait ? 110.f : 70.f);
			Quad(Out, G, L, HC + FVector2D(0.f, -54.f), HC + FVector2D(bPortrait ? 1000.f : 900.f, -70.f), HC + FVector2D(bPortrait ? 960.f : 860.f, 70.f), HC + FVector2D(0.f, 64.f), Blk);
			Quad(Out, G, L + 1, HC + FVector2D(0.f, 64.f), HC + FVector2D(bPortrait ? 960.f : 860.f, 70.f), HC + FVector2D(bPortrait ? 955.f : 855.f, 84.f), HC + FVector2D(0.f, 80.f), Col);
			Text(Out, G, L + 2, Ch.Ep, Font(1, 22.f), HC + FVector2D(150.f, -36.f), -1.f, Col);
			Ransom(Out, G, L + 3, Ch.Title, HC.X + 70.f, HC.Y + 26.f, 40.f, -2.f, 31u, T - 0.1f, Acc);
		}
		L += 8;
		// the route: dashed, marching
		for (int32 k = 0; k + 1 < N; ++k)
		{
			const FVector2D A = NodePos(k, N), B = NodePos(k + 1, N);
			const FVector2D D = B - A;
			const float Len = D.Size();
			const float Ang = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			const bool bOpen = ZoogiStorySave::SpotOpen(Sp[k + 1]);
			const float Grow = FMath::Clamp((T - 0.3f - 0.15f * k) / 0.4f, 0.f, 1.f);
			const float Dash = 34.f, Gap = 22.f, Ph = FMath::Fmod(T * 70.f, Dash + Gap);
			for (float s = -Dash + Ph; s < Len * Grow; s += Dash + Gap)
			{
				const float s0 = FMath::Max(0.f, s), s1 = FMath::Min(Len * Grow, s + Dash);
				if (s1 <= s0) continue;
				const FVector2D M = A + D / Len * ((s0 + s1) * 0.5f);
				Box(Out, G, L, M + FVector2D(4.f, 5.f), FVector2D(s1 - s0, 14.f), Ang, Blk);
				Box(Out, G, L + 1, M, FVector2D(s1 - s0, 10.f), Ang, bOpen ? Wht : FLinearColor(1.f, 1.f, 1.f, 0.3f));
			}
		}
		L += 3;
		// the stops
		int32 Next = -1;
		for (int32 k = 0; k < N; ++k) if (ZoogiStorySave::SpotOpen(Sp[k]) && !ZoogiStorySave::Cleared(Sp[k])) { Next = k; break; }
		for (int32 k = 0; k < N; ++k)
		{
			const FSpot& Spot = GDB.Spots[Sp[k]];
			const bool bOpen = ZoogiStorySave::SpotOpen(Sp[k]), bDone = ZoogiStorySave::Cleared(Sp[k]), bHere = GS.Spot == Sp[k];
			const FVector2D Sz = bPortrait ? FVector2D(420.f, 250.f) : FVector2D(380.f, 230.f);
			const float In = EaseOutBack((T - 0.2f - 0.12f * k) / 0.3f);
			FVector2D C = NodePos(k, N) + FVector2D(0.f, (1.f - In) * 900.f);
			if (DenySpot == Sp[k] && T - DenyT < 0.35f) C.X += 14.f * FMath::Sin((T - DenyT) * 80.f);
			const float Ang = ((k % 2) ? 3.f : -3.f) + (k == Next ? 0.7f * FMath::Sin(T * 8.f) : 0.f);
			const float Sc = bHere && GS.bPanel ? 1.08f : 1.f;
			UTexture2D* Art = Portrait(ArenaKey(Spot.Env) + TEXT("_map"));
			if (!Art) Art = Portrait(Ch.Bg);
			Box(Out, G, L, C + FVector2D(14.f, 16.f), (Sz + FVector2D(20.f, 20.f)) * Sc, Ang, Blk);
			Box(Out, G, L + 1, C, (Sz + FVector2D(18.f, 18.f)) * Sc, Ang, bHere ? Col : (bOpen ? Wht : FLinearColor(0.4f, 0.4f, 0.42f)));
			TexCover(Out, G, L + 2, Art, C, Sz * Sc, Ang, bOpen ? Wht : FLinearColor(0.2f, 0.2f, 0.22f));
			// label slab under the art
			const FVector2D LC = C + Rot(Ang, FVector2D(-Sz.X * 0.05f, Sz.Y * 0.5f + 34.f));
			const FSlateFontInfo LF = Font(0, 40.f), SF = Font(1, 18.f);
			const FVector2D Ls = Measure(Spot.Label, LF);
			Box(Out, G, L + 3, LC + FVector2D(7.f, 7.f), Ls + FVector2D(36.f, 10.f), Ang - 2.f, bOpen ? Col : Blk);
			Box(Out, G, L + 4, LC, Ls + FVector2D(36.f, 10.f), Ang - 2.f, Blk);
			Text(Out, G, L + 5, Spot.Label, LF, LC, Ang - 2.f, bOpen ? Wht : FLinearColor(1.f, 1.f, 1.f, 0.45f));
			if (bOpen)
			{
				const FVector2D SC = LC + Rot(Ang, FVector2D(0.f, 44.f));
				const FVector2D Ss = Measure(Spot.Sub, SF);
				Box(Out, G, L + 4, SC, Ss + FVector2D(20.f, 8.f), Ang - 1.f, Wht);
				Text(Out, G, L + 5, Spot.Sub, SF, SC, Ang - 1.f, Blk);
			}
			// number tab
			const FVector2D NC = C + Rot(Ang, FVector2D(-Sz.X * 0.5f + 10.f, -Sz.Y * 0.5f + 10.f));
			Box(Out, G, L + 6, NC, FVector2D(62.f, 62.f), Ang + 8.f, bOpen ? Acc : Blk);
			Text(Out, G, L + 7, FString::FromInt(k + 1), Font(0, 46.f), NC, Ang + 8.f, Wht);
			if (!bOpen)
			{
				TiledRot(Out, G, L + 6, DotsTex(), C, Sz, Ang, 22.f, Acc.CopyWithNewOpacity(0.2f));
				PTag(Out, G, L + 7, TEXT("LOCKED"), C, 44.f, Ang - 6.f, Wht, Blk, Acc);
			}
			else if (bDone)
			{
				const FVector2D SC = C + Rot(Ang, FVector2D(Sz.X * 0.36f, -Sz.Y * 0.36f));
				const bool bSlam = GS.bJustCleared && bHere;
				const float K = bSlam ? EaseOutBack((T - 0.5f) / 0.25f) : 1.f;
				if (K > 0.f)
				{
					Tex(Out, G, L + 8, BurstTex(), SC, FVector2D(130.f, 130.f) * (1.f + (1.f - K) * 2.f), 15.f, Blk);
					Tex(Out, G, L + 9, BurstTex(), SC, FVector2D(114.f, 114.f) * (1.f + (1.f - K) * 2.f), 15.f, Col);
					Text(Out, G, L + 10, TEXT("CLEAR!"), Font(3, 34.f * (1.f + (1.f - K) * 2.f), 3.f), SC, -12.f, Wht);
				}
			}
			if (k == Next && !(bHere && GS.bPanel) && T > 0.7f)
			{
				const float P = 1.f + 0.09f * FMath::Sin(T * 10.f);
				const FVector2D GC = C + Rot(Ang, FVector2D(Sz.X * 0.42f, Sz.Y * 0.3f));
				Tex(Out, G, L + 8, BurstTex(), GC, FVector2D(140.f, 140.f) * P, T * 40.f, Blk);
				Tex(Out, G, L + 9, BurstTex(), GC, FVector2D(124.f, 124.f) * P, T * 40.f, Acc);
				Text(Out, G, L + 10, TEXT("GO!"), Font(3, 50.f * P, 3.f), GC, -12.f, Wht);
			}
			Hits.Add({ FBox2D(C - Sz * 0.55f, C + Sz * 0.55f + FVector2D(0.f, 60.f)), 2, Sp[k] });
		}
		L += 12;
		// Blue's token (bounces; hops along the route when travelling)
		if (N > 0 && T > 0.4f)
		{
			const FVector2D P = TokenPos(Sp) + FVector2D(0.f, -(bPortrait ? 200.f : 190.f) - FMath::Abs(FMath::Sin(T * 5.f)) * 14.f);
			Tri(Out, G, L, P + FVector2D(-22.f, 52.f), P + FVector2D(22.f, 52.f), P + FVector2D(0.f, 92.f), Acc);
			Box(Out, G, L + 1, P + FVector2D(6.f, 7.f), FVector2D(118.f, 118.f), -6.f, Blk);
			Box(Out, G, L + 2, P, FVector2D(118.f, 118.f), -6.f, Acc);
			TexCover(Out, G, L + 3, ZoogiStoryData::StoryArt(Ch.Element == TEXT("Void") ? TEXT("void_frosti") : TEXT("blue")), P, FVector2D(100.f, 100.f), -6.f, Wht, FVector2D(0.5f, 0.4f)); // b166: EP4 = void Frosti
		}
		L += 5;
		// arrived: the ROLL OUT panel
		if (GS.bPanel && GDB.Spots.IsValidIndex(GS.Spot))
		{
			const FSpot& Spot = GDB.Spots[GS.Spot];
			const FMission* M = GDB.Missions.Find(Spot.Mission);
			const float In = EaseOutBack((T - PanelT) / 0.3f);
			const FVector2D PC = bPortrait ? FVector2D(540.f, DH - 290.f) : FVector2D(DW * 0.42f, DH - 110.f);
			const FVector2D PS = bPortrait ? FVector2D(1020.f, 300.f) : FVector2D(DW * 0.74f, 170.f);
			const FVector2D C = PC + FVector2D(0.f, (1.f - In) * 500.f);
			Box(Out, G, L, C + FVector2D(12.f, 12.f), PS, -2.f, Col);
			Box(Out, G, L + 1, C, PS, -2.f, Blk);
			const FVector2D TL = C - PS * 0.5f + FVector2D(40.f, 26.f);
			if (M)
			{
				Ransom(Out, G, L + 2, M->Title, TL.X, TL.Y + 26.f, FitBase(M->Title, bPortrait ? 40.f : 44.f, PS.X - (bPortrait ? 90.f : 420.f), 17u), -2.f, 17u, T - PanelT - 0.1f, Acc);
				TArray<FString> Foes;
				for (int32 i = 0; i < M->Players.Num(); ++i)
				{
					const bool bFoe = M->Mode == TEXT("combo") ? i >= 2 : i >= 1;
					if (bFoe) Foes.AddUnique(WhoName(M->Players[i]));
				}
				FString Info = FString::Printf(TEXT("%s  /  VS %s  /  %s"), *ModeTag(*M), *FString::Join(Foes, TEXT(" & ")), *M->Tier.ToUpper());
				if (M->Mode != TEXT("royale")) Info += TEXT("  /  CARDS ON");
				const FSlateFontInfo IF = Font(1, bPortrait ? 22.f : 24.f);
				TextBlock(Out, G, L + 3, Info, IF, TL + FVector2D(0.f, bPortrait ? 70.f : 74.f), PS.X - (bPortrait ? 80.f : 520.f), 999, Wht, IF.Size * 1.35f);
			}
			const bool bDone = ZoogiStorySave::Cleared(GS.Spot);
			const FVector2D BC = bPortrait ? C + FVector2D(250.f, 80.f) : FVector2D(DW - 260.f, DH - 110.f) + FVector2D(0.f, (1.f - In) * 500.f);
			Button(Out, G, L + 4, bDone ? TEXT("REPLAY") : TEXT("ROLL OUT!"), BC, 0, 3, 0, (T - PanelT) / 0.3f);
			if (FirstStop())
			{	// b67 (Evan): the first story mission offers the guided first turn
				const bool bOn = GuideOn();
				const float GIn = FMath::Clamp((T - PanelT - 0.15f) / 0.3f, 0.f, 1.f);
				const FVector2D GC = BC + FVector2D(-480.f + (1.f - EaseOutBack(GIn)) * 900.f, 4.f); // beside ROLL OUT (above it covered the mission line)
				const FVector2D GSz(380.f, 78.f);
				Box(Out, G, L + 5, GC + FVector2D(8.f, 8.f), GSz, -4.f, FLinearColor::Black);
				Box(Out, G, L + 6, GC, GSz, -4.f, bOn ? FLinearColor(1.f, 0.82f, 0.25f) : FLinearColor::White);
				Text(Out, G, L + 7, bOn ? TEXT("PLAY GUIDE: YES") : TEXT("PLAY GUIDE: NO"), Font(0, 36.f), GC, -4.f, FLinearColor::Black);
				Hits.Add({ FBox2D(GC - GSz * 0.55f, GC + GSz * 0.55f), 9, 0 });
			}
		}
		else BackTag(Out, G, L + 4);
		L += 8;
		if (!GS.bPanel && T > 1.f && TravelT < 0.f)
			Ransom(Out, G, L, TEXT("TAP A STOP TO TRAVEL"), 60.f, DH - (bPortrait ? 140.f : 100.f), 28.f * (1.f + 0.04f * FMath::Sin(T * 7.f)), -3.f, 51u, 1.f, Acc);
		return L + 4;
	}

	// ---- DIALOGUE
	int32 PaintDialogue(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		using namespace ZoogiStoryDir;
		const FLine* Ln = CurLine();
		if (!Ln) return L;
		const FLinearColor Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White, Col = ChapterCol();
		const float LB = bPortrait ? 150.f : 76.f;
		const float LbIn = EaseOutBack(T / 0.3f);
		Veil(Out, G, L, bBlack ? 1.f : 0.12f, 0.9f);
		if (bRed) Box(Out, G, L + 1, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, FLinearColor(0.9f, 0.f, 0.08f, 0.24f + 0.08f * FMath::Sin(T * 6.f)));
		if (bCold) Box(Out, G, L + 1, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, FLinearColor(0.15f, 0.55f, 1.f, 0.2f));
		L += 3;
		// cinematic bars
		Box(Out, G, L, FVector2D(DW * 0.5f, LB * 0.5f - (1.f - LbIn) * LB), FVector2D(DW + 40.f, LB), 0.f, Blk);
		Box(Out, G, L, FVector2D(DW * 0.5f, DH - LB * 0.5f + (1.f - LbIn) * LB), FVector2D(DW + 40.f, LB), 0.f, Blk);
		// the place card (first moments of a scene)
		if (!GS.Place.IsEmpty() && T < 3.2f)
		{
			const float In = EaseOutBack(T / 0.3f), Out2 = FMath::Clamp((T - 2.9f) / 0.3f, 0.f, 1.f);
			const FVector2D PC(40.f + (1.f - In) * -600.f - Out2 * Out2 * 900.f, LB + 70.f);
			Ransom(Out, G, L + 1, GS.Place, PC.X, PC.Y, 38.f, -4.f, 61u, T - 0.1f, Col);
		}
		L += 6;
		// faces (two-shot: the listener stays, dimmed)
		const bool bTitle = Ln->Who == TEXT("title"), bNarr = Ln->Who == TEXT("narrator");
		const float PSz = bPortrait ? 520.f : 430.f;
		const float PY = bPortrait ? DH - 760.f : DH - 470.f;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FString& Wh = Side ? RightWho : LeftWho;
			if (Wh.IsEmpty() || bTitle) continue;
			const bool bActive = Wh == Ln->Who;
			const float In = EaseOutBack((T - (Side ? RightIn : LeftIn)) / 0.28f);
			const float X = Side ? (bPortrait ? 760.f : DW - 300.f) : (bPortrait ? 320.f : 300.f);
			const FVector2D C(X + (Side ? 1.f : -1.f) * (1.f - In) * 900.f, PY + (bActive ? 0.f : 26.f));
			const float Sz = PSz * (bActive ? 1.f : 0.88f);
			FaceCard(Out, G, L + (bActive ? 8 : 0), Face(Wh), C, Sz, Side ? 4.f : -4.f, WhoCol(Wh), bActive && !bNarr ? 1.f : 0.42f, Wh == TEXT("shadow"));
		}
		L += 14;
		if (bTitle)
		{
			const float K = EaseOutBack(LineT / 0.25f);
			const float BH = bPortrait ? 300.f : 240.f;
			Box(Out, G, L, FVector2D(DW * 0.5f, DH * 0.48f) + FVector2D(0.f, 14.f), FVector2D(DW * 1.3f, BH * K + 24.f), -4.f, Col);
			Box(Out, G, L + 1, FVector2D(DW * 0.5f, DH * 0.48f), FVector2D(DW * 1.3f, BH * K), -4.f, Blk);
			Tex(Out, G, L + 2, BurstTex(), FVector2D(DW * 0.5f, DH * 0.48f), FVector2D(BH, BH) * 2.2f * K, LineT * 30.f, Acc.CopyWithNewOpacity(0.5f));
			const float B = FitBase(Ln->Text, bPortrait ? 84.f : 110.f, DW - 140.f, 71u);
			const float Wd = RansomWidth(Ln->Text, B, 71u);
			Ransom(Out, G, L + 4, Ln->Text, DW * 0.5f - Wd * 0.5f, DH * 0.48f, B, -4.f, 71u, LineT - 0.08f, Acc);
			L += 10;
		}
		else
		{
			// the speech panel
			const FVector2D PC = bPortrait ? FVector2D(540.f, DH - LB - 230.f) : FVector2D(DW * 0.5f, DH - LB - 122.f);
			const FVector2D PS = bPortrait ? FVector2D(1020.f, 400.f) : FVector2D(FMath::Min(DW - 120.f, 1700.f), 236.f);
			const float K = EaseOutBack(LineT / 0.18f);
			const FLinearColor SC = bNarr ? Col : WhoCol(Ln->Who);
			Box(Out, G, L, PC + FVector2D(14.f, 14.f), PS * FVector2D(1.f, K), -1.2f, SC);
			Box(Out, G, L + 1, PC, PS * FVector2D(1.f, K) + FVector2D(10.f, 10.f), -1.2f, Wht);
			Box(Out, G, L + 2, PC, PS * FVector2D(1.f, K), -1.2f, Blk);
			if (bNarr) Box(Out, G, L + 3, PC - FVector2D(PS.X * 0.5f - 10.f, 0.f), FVector2D(14.f, PS.Y * K - 30.f), -1.2f, Col);
			const FSlateFontInfo F = bNarr ? Font(2, bPortrait ? 42.f : 38.f) : Font(1, bPortrait ? 42.f : 38.f);
			const FVector2D TL = PC - PS * 0.5f + FVector2D(bNarr ? 60.f : 50.f, bPortrait ? 70.f : 54.f);
			TextBlock(Out, G, L + 4, Ln->Text, F, TL, PS.X - 120.f, Shown, Wht, F.Size * 1.32f);
			if (IsSpeaker(Ln->Who))
			{
				const ZoogiStoryData::FWho* W = ZoogiStoryData::Who(Ln->Who);
				const FString Nm = WhoName(Ln->Who);
				const float NB = bPortrait ? 46.f : 42.f;
				const float NW = RansomWidth(Nm, NB, 81u);
				const float NX = (W && W->bRight) ? PC.X + PS.X * 0.5f - NW - 40.f : PC.X - PS.X * 0.5f + 30.f;
				Ransom(Out, G, L + 6, Nm, NX, PC.Y - PS.Y * 0.5f - 6.f, NB, -4.f, 81u, LineT, SC);
			}
			if (Shown >= Ln->Text.Len())
			{
				const FVector2D AC = PC + FVector2D(PS.X * 0.5f - 50.f, PS.Y * 0.5f - 40.f + FMath::Abs(FMath::Sin(T * 6.f)) * -10.f);
				Tri(Out, G, L + 10, AC + FVector2D(-18.f, -12.f), AC + FVector2D(18.f, -12.f), AC + FVector2D(0.f, 14.f), Acc);
			}
			L += 12;
		}
		// the screen cracks
		if (bCrack)
		{
			const FVector2D O(DW * 0.56f, DH * 0.42f);
			for (int32 r = 0; r < 9; ++r)
			{
				FVector2D P = O;
				float A = r * 40.f + (Hash(r * 7u + 3u) % 30u);
				const float K = FMath::Clamp(LineT / 0.12f, 0.f, 1.f);
				for (int32 s = 0; s < 7; ++s)
				{
					A += (float)((int32)(Hash(r * 131u + s * 17u) % 50u) - 25);
					const float Len = (60.f + (Hash(r * 13u + s * 101u) % 120u)) * K;
					const FVector2D Q = P + Rot(A, FVector2D(Len, 0.f));
					const FVector2D M = (P + Q) * 0.5f;
					Box(Out, G, L, M, FVector2D(Len + 2.f, 9.f - s * 0.8f), A, Blk);
					Box(Out, G, L + 1, M, FVector2D(Len, 4.f - s * 0.4f), A, Wht);
					P = Q;
				}
			}
			L += 2;
		}
		// SKIP
		{
			const FVector2D SC(DW - 120.f, LB + 56.f);
			Box(Out, G, L, SC + FVector2D(6.f, 6.f), FVector2D(170.f, 64.f), 6.f, Acc);
			Box(Out, G, L + 1, SC, FVector2D(170.f, 64.f), 6.f, Wht);
			Text(Out, G, L + 2, TEXT("SKIP"), Font(0, 42.f), SC, 6.f, Blk);
			Hits.Add({ FBox2D(SC - FVector2D(95.f, 40.f), SC + FVector2D(95.f, 40.f)), 5, 0 });
		}
		L += 3;
		const float Fl = 1.f - (T - FlashT) / 0.2f;
		if (Fl > 0.f) Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(60.f, 60.f), 0.f, FLinearColor(1.f, 1.f, 1.f, 0.85f * Fl));
		return L + 2;
	}

	// ---- VS
	int32 PaintVS(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		using namespace ZoogiStoryDir;
		const FMission* M = GDB.Missions.Find(GS.Mission);
		if (!M) return L;
		const FLinearColor Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		const FLinearColor ACol = FLinearColor::FromSRGBColor(FColor(20, 120, 230)), BCol = FLinearColor::FromSRGBColor(FColor(200, 14, 40));
		TArray<FString> TA, TB;
		for (int32 i = 0; i < M->Players.Num(); ++i) ((M->Mode == TEXT("combo") ? i < 2 : i < 1) ? TA : TB).Add(M->Players[i]);
		const float InA = EaseOutBack(T / 0.26f), InB = EaseOutBack((T - 0.1f) / 0.26f);
		if (bPortrait)
		{
			const float Y = DH * 0.5f;
			Quad(Out, G, L, FVector2D(-40.f - (1.f - InA) * 1400.f, -40.f), FVector2D(DW + 40.f - (1.f - InA) * 1400.f, -40.f), FVector2D(DW + 40.f - (1.f - InA) * 1400.f, Y - 90.f), FVector2D(-40.f - (1.f - InA) * 1400.f, Y + 90.f), ACol);
			Quad(Out, G, L, FVector2D(-40.f + (1.f - InB) * 1400.f, Y + 90.f), FVector2D(DW + 40.f + (1.f - InB) * 1400.f, Y - 90.f), FVector2D(DW + 40.f + (1.f - InB) * 1400.f, DH + 40.f), FVector2D(-40.f + (1.f - InB) * 1400.f, DH + 40.f), BCol);
			Quad(Out, G, L + 1, FVector2D(-40.f, Y + 82.f), FVector2D(DW + 40.f, Y - 98.f), FVector2D(DW + 40.f, Y - 82.f), FVector2D(-40.f, Y + 98.f), Wht);
		}
		else
		{
			const float Sk = DW * 0.07f;
			Quad(Out, G, L, FVector2D(-40.f - (1.f - InA) * 1600.f, -40.f), FVector2D(DW * 0.5f + Sk - (1.f - InA) * 1600.f, -40.f), FVector2D(DW * 0.5f - Sk - (1.f - InA) * 1600.f, DH + 40.f), FVector2D(-40.f - (1.f - InA) * 1600.f, DH + 40.f), ACol);
			Quad(Out, G, L, FVector2D(DW * 0.5f + Sk + (1.f - InB) * 1600.f, -40.f), FVector2D(DW + 40.f + (1.f - InB) * 1600.f, -40.f), FVector2D(DW + 40.f + (1.f - InB) * 1600.f, DH + 40.f), FVector2D(DW * 0.5f - Sk + (1.f - InB) * 1600.f, DH + 40.f), BCol);
			Quad(Out, G, L + 1, FVector2D(DW * 0.5f + Sk - 10.f, -40.f), FVector2D(DW * 0.5f + Sk + 10.f, -40.f), FVector2D(DW * 0.5f - Sk + 10.f, DH + 40.f), FVector2D(DW * 0.5f - Sk - 10.f, DH + 40.f), Wht);
		}
		const float Drift = FMath::Fmod(T * 60.f, 64.f);
		Tiled(Out, G, L + 2, DotsTex(), FVector2D(-64.f + Drift, -64.f), FVector2D(DW + 128.f, DH + 128.f), 40.f, FLinearColor(0.f, 0.f, 0.f, 0.28f));
		// speed lines out of the middle
		for (int32 k = 0; k < 22; ++k)
		{
			const uint32 Hh = Hash(911u + k * 7919u);
			const float A = (Hh % 360u) * 1.f;
			const float R0 = FMath::Fmod(T * (900.f + (Hh >> 9) % 600u) + (Hh >> 3) % 800u, 1400.f);
			Box(Out, G, L + 3, FVector2D(DW, DH) * 0.5f + Rot(A, FVector2D(R0 + 200.f, 0.f)), FVector2D(160.f + (Hh % 140u), 6.f), A, FLinearColor(1.f, 1.f, 1.f, 0.35f));
		}
		L += 5;
		auto Team = [&](const TArray<FString>& Ids, bool bA, float In)
		{
			const int32 Nn = Ids.Num();
			if (Nn == 0) return;
			const float Big = bPortrait ? 480.f : 520.f;
			const float Sz = Nn == 1 ? Big : (Nn == 2 ? Big * 0.78f : Big * 0.6f);
			const FVector2D Base = bPortrait ? FVector2D(540.f, bA ? DH * 0.25f + 40.f : DH * 0.72f) : FVector2D(bA ? DW * 0.25f : DW * 0.75f, DH * 0.52f);
			for (int32 i = 0; i < Nn; ++i)
			{
				const float o = (i - (Nn - 1) * 0.5f);
				const FVector2D C = Base + (bPortrait ? FVector2D(o * Sz * 0.72f, o * 30.f) : FVector2D(o * Sz * 0.5f, o * Sz * 0.36f)) + FVector2D((bA ? -1.f : 1.f) * (1.f - In) * 1500.f, 0.f);
				FaceCard(Out, G, L + i * 6, Face(Ids[i]), C, Sz, bA ? -5.f : 5.f, WhoCol(Ids[i]), 1.f, false);
			}
			TArray<FString> Names;
			for (const FString& Id : Ids) Names.AddUnique(WhoName(Id));
			const FString Nm = FString::Join(Names, TEXT(" & "));
			const float NB = FitBase(Nm, 54.f, bPortrait ? 900.f : DW * 0.4f, 91u);
			const float Wd = RansomWidth(Nm, NB, 91u);
			const FVector2D NC = Base + FVector2D(0.f, bPortrait ? Sz * 0.5f + 34.f : Sz * 0.5f + 70.f);
			Ransom(Out, G, L + 30, Nm, NC.X - Wd * 0.5f, NC.Y, NB, -5.f, 91u, T - 0.3f, bA ? ACol : BCol);
		};
		Team(TA, true, EaseOutBack((T - 0.08f) / 0.3f));
		Team(TB, false, EaseOutBack((T - 0.18f) / 0.3f));
		L += 36;
		// VS
		if (T > 0.34f)
		{
			const float K = EaseOutBack((T - 0.34f) / 0.2f);
			const FVector2D C(DW * 0.5f, DH * 0.5f);
			const float VS = bPortrait ? 0.72f : 1.f;
			Tex(Out, G, L, BurstTex(), C, FVector2D(420.f, 420.f) * VS * (2.4f - 1.4f * K), T * 30.f, Blk);
			Tex(Out, G, L + 1, BurstTex(), C, FVector2D(380.f, 380.f) * VS * (2.4f - 1.4f * K), -T * 20.f, Acc);
			Text(Out, G, L + 2, TEXT("VS"), Font(3, 230.f * VS * (2.f - K), 8.f), C, -10.f, Wht);
		}
		L += 4;
		// title + tags
		{
			const float B = FitBase(M->Title, 62.f, DW - 120.f, 13u);
			const float Wd = RansomWidth(M->Title, B, 13u);
			const float Ty = bPortrait ? 110.f : 70.f;
			Ransom(Out, G, L, M->Title, DW * 0.5f - Wd * 0.5f, Ty, B, -3.f, 13u, T - 0.4f, Acc);
			TArray<FString> Tags = { ModeTag(*M), M->Tier.ToUpper() };
			if (M->Mode != TEXT("royale")) Tags.Add(TEXT("CARDS ON"));
			float X = DW * 0.5f - (Tags.Num() - 1) * 0.5f * (bPortrait ? 300.f : 290.f);
			for (int32 i = 0; i < Tags.Num(); ++i, X += bPortrait ? 300.f : 290.f)
				if (T > 0.5f + 0.08f * i) PTag(Out, G, L + 4, Tags[i], FVector2D(X, Ty + (bPortrait ? 90.f : 82.f)), 28.f, -3.f, i == 1 ? Acc : Blk, Wht, i == 1 ? Blk : Acc, 1);
		}
		if (T > 1.0f)
		{
			const float B = 44.f * (1.f + 0.05f * FMath::Sin(T * 8.f));
			const float Wd = RansomWidth(TEXT("TAP TO ROLL!"), B, 47u);
			Ransom(Out, G, L + 8, TEXT("TAP TO ROLL!"), DW * 0.5f - Wd * 0.5f, DH - (bPortrait ? 130.f : 80.f), B, -3.f, 47u, 1.f, Acc);
		}
		return L + 12;
	}

	// ---- RESULTS
	int32 PaintResults(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		using namespace ZoogiStoryDir;
		const FMission* M = GDB.Missions.Find(GS.Mission);
		if (!M) return L;
		const bool bWon = GS.bWon;
		const FLinearColor Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		const FLinearColor Col = bWon ? ChapterCol() : FLinearColor(0.55f, 0.02f, 0.06f);
		Bg(Out, G, L, Col, bPortrait ? FVector2D(540.f, DH * 0.42f) : FVector2D(DW * 0.3f, DH * 0.55f), bWon ? 10.f : 2.f, bWon ? 1.2f : 0.8f);
		L += 4;
		const FString Head = bWon ? TEXT("MISSION CLEAR!") : TEXT("MISSION FAILED");
		const float HB = FitBase(Head, bPortrait ? 96.f : 100.f, bPortrait ? DW - 140.f : DW * 0.62f, 3u);
		const float HY = bPortrait ? 150.f : 170.f;
		Ransom(Out, G, L, Head, 60.f, HY, HB, -4.f, 3u, T - 0.1f, bWon ? Acc : Col);
		if (T > 0.3f) PTag(Out, G, L + 4, M->Title, FVector2D(60.f + Measure(M->Title, Font(0, 40.f)).X * 0.5f + 30.f, HY + HB + 30.f), 40.f, -3.f, Blk, Wht, Col);
		L += 8;
		const FString FaceId = bWon ? TEXT("blue") : M->LoseWho;
		const float FIn = EaseOutBack((T - 0.15f) / 0.3f);
		const FVector2D FC = bPortrait ? FVector2D(540.f, DH * 0.44f) : FVector2D(DW * 0.28f, DH * 0.58f);
		const float FS = bPortrait ? 560.f : 470.f;
		FaceCard(Out, G, L, Face(FaceId), FC + FVector2D((1.f - FIn) * -1400.f, 0.f), FS, -5.f, bWon ? WhoCol(TEXT("blue")) : WhoCol(FaceId), 1.f, FaceId == TEXT("shadow"));
		L += 8;
		// right side: the reward, or the rival's last word
		const FVector2D PC = bPortrait ? FVector2D(540.f, DH * 0.44f + FS * 0.5f + 190.f) : FVector2D(DW * 0.67f, DH * 0.45f);
		const FVector2D PS = bPortrait ? FVector2D(960.f, 230.f) : FVector2D(FMath::Min(820.f, DW * 0.5f), 260.f);
		const float PIn = EaseOutBack((T - 0.5f) / 0.3f);
		if (PIn > 0.f)
		{
			const FVector2D C = PC + FVector2D((1.f - PIn) * 1400.f, 0.f);
			Box(Out, G, L, C + FVector2D(12.f, 12.f), PS, -3.f, Col);
			Box(Out, G, L + 1, C, PS + FVector2D(10.f, 10.f), -3.f, Wht);
			Box(Out, G, L + 2, C, PS, -3.f, Blk);
			const FVector2D TL = C - PS * 0.5f + FVector2D(44.f, 64.f);
			if (bWon)
			{
				PTag(Out, G, L + 3, TEXT("REWARD"), C - PS * 0.5f + FVector2D(110.f, 0.f), 34.f, -6.f, Acc, Wht, Blk);
				TextBlock(Out, G, L + 4, M->Reward, Font(0, 52.f), TL + FVector2D(0.f, 10.f), PS.X - 90.f, 999, Wht, 62.f);
			}
			else
			{
				Ransom(Out, G, L + 3, WhoName(M->LoseWho), C.X - PS.X * 0.5f + 30.f, C.Y - PS.Y * 0.5f - 6.f, 40.f, -4.f, 81u, T - 0.5f, WhoCol(M->LoseWho));
				TextBlock(Out, G, L + 4, M->LoseLine, Font(1, 36.f), TL, PS.X - 90.f, FMath::FloorToInt((T - 0.7f) * 44.f), Wht, 48.f);
			}
		}
		L += 10;
		const float BIn = (T - 1.0f) / 0.3f;
		if (BIn > 0.f)
		{
			if (bPortrait)
			{
				const float Y = DH - 220.f;
				if (bWon) Button(Out, G, L, TEXT("CONTINUE"), FVector2D(540.f, Y), 0, 6, 0, BIn);
				else { Button(Out, G, L, TEXT("RETRY"), FVector2D(360.f, Y), 0, 6, 0, BIn); Button(Out, G, L, TEXT("MAP"), FVector2D(820.f, Y + 20.f), 1, 6, 1, BIn - 0.3f); }
			}
			else
			{
				const float Y = DH - 120.f;
				if (bWon) Button(Out, G, L, TEXT("CONTINUE"), FVector2D(DW - 300.f, Y), 0, 6, 0, BIn);
				else { Button(Out, G, L, TEXT("RETRY"), FVector2D(DW - 640.f, Y), 0, 6, 0, BIn); Button(Out, G, L, TEXT("MAP"), FVector2D(DW - 240.f, Y + 10.f), 1, 6, 1, BIn - 0.3f); }
			}
		}
		return L + 6;
	}

	// ---- EPISODE CLEAR
	int32 PaintClear(FSlateWindowElementList& Out, const FGeometry& G, int32 L) const
	{
		using namespace ZoogiStoryData;
		using namespace ZoogiStoryDir;
		if (!GDB.Chapters.IsValidIndex(GS.Chapter)) return L;
		const FChapter& Ch = GDB.Chapters[GS.Chapter];
		const FLinearColor Col = ElemCol(Ch.Element), Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		Bg(Out, G, L, Col, FVector2D(DW * 0.5f, DH * 0.5f), 12.f, 1.3f);
		TexCover(Out, G, L + 3, Portrait(Ch.Bg), FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH) + FVector2D(40.f, 40.f), 0.f, FLinearColor(1.f, 1.f, 1.f, 0.22f));
		L += 5;
		const float Y0 = bPortrait ? 260.f : 190.f;
		const float B1 = FitBase(Ch.Ep, 70.f, DW - 140.f, 7u);
		Ransom(Out, G, L, Ch.Ep, DW * 0.5f - RansomWidth(Ch.Ep, B1, 7u) * 0.5f, Y0, B1, -5.f, 7u, T - 0.1f, Col);
		const bool bLast = GS.Chapter >= GDB.Chapters.Num() - 1;
		const FString Big = bLast ? TEXT("STORY COMPLETE!") : TEXT("CLEAR!");
		const float B2 = FitBase(Big, bPortrait ? 150.f : 180.f, DW - 140.f, 19u);
		Ransom(Out, G, L + 4, Big, DW * 0.5f - RansomWidth(Big, B2, 19u) * 0.5f, Y0 + 60.f + B2 * 0.8f, B2, -5.f, 19u, T - 0.35f, Acc);
		L += 8;
		// the cast that rolled through this episode
		TArray<FString> Cast;
		for (int32 s : ChapterSpots(GS.Chapter))
			if (const FMission* M = GDB.Missions.Find(GDB.Spots[s].Mission))
				for (const FString& P : M->Players) Cast.AddUnique(P == TEXT("darkzoogy") ? FString(TEXT("dark")) : P);
		const int32 N = FMath::Min(Cast.Num(), bPortrait ? 4 : 6);
		const float FS = bPortrait ? 220.f : 230.f;
		for (int32 i = 0; i < N; ++i)
		{
			const float In = EaseOutBack((T - 0.6f - 0.08f * i) / 0.28f);
			const FVector2D C(DW * 0.5f + (i - (N - 1) * 0.5f) * (FS + 30.f), (bPortrait ? DH * 0.55f : DH * 0.62f) + (1.f - In) * 900.f + ((i % 2) ? 26.f : -10.f));
			FaceCard(Out, G, L + (i % 2) * 6, Face(Cast[i]), C, FS, (i % 2) ? 5.f : -5.f, WhoCol(Cast[i]), 1.f, false);
		}
		L += 14;
		if (T > 1.2f)
		{
			const FString Nx = bLast ? FString(TEXT("THANK YOU FOR PLAYING. THE RINGS ARE FREE."))
				: FString::Printf(TEXT("NEXT: %s  -  %s"), *GDB.Chapters[GS.Chapter + 1].Ep, *GDB.Chapters[GS.Chapter + 1].Title);
			PTag(Out, G, L, Nx, FVector2D(DW * 0.5f, DH - (bPortrait ? 360.f : 190.f)), bPortrait ? 30.f : 38.f, -2.f, Blk, Wht, Col, 1);
			if (FilmExists(GDB.Chapters[GS.Chapter].Film) && T > 0.9f)
				PTag(Out, G, L + 2, TEXT("FULL EPISODE FILM UNLOCKED!  WATCH IT ON THE EPISODE CARD"), FVector2D(DW * 0.5f, DH - (bPortrait ? 460.f : 262.f)), bPortrait ? 26.f : 32.f, 2.f, Acc, Wht, Blk, 1);
			Ransom(Out, G, L + 4, TEXT("TAP TO CONTINUE"), DW * 0.5f - RansomWidth(TEXT("TAP TO CONTINUE"), 30.f, 41u) * 0.5f, DH - (bPortrait ? 200.f : 90.f), 30.f * (1.f + 0.05f * FMath::Sin(T * 8.f)), -3.f, 41u, 1.f, Acc);
		}
		return L + 8;
	}

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 L0, const FWidgetStyle&, bool) const override
	{
		using namespace ZoogiStoryDir;
		const FVector2D Px = Geo.GetLocalSize();
		bPortrait = Px.Y > Px.X;
		S = bPortrait ? Px.X / 1080.f : Px.Y / 1080.f;
		DW = Px.X / S; DH = Px.Y / S;
		const float Sh = 1.f - (T - ShakeT) / 0.45f;
		Off = (GS.Scr == EScr::Dialogue && Sh > 0.f) ? FVector2D(FMath::Sin(T * 90.f) * 22.f, FMath::Cos(T * 77.f) * 14.f) * Sh * S : FVector2D::ZeroVector;
		Hits.Reset();
		const FGeometry G = Geo.MakeChild(FVector2D(DW, DH), FSlateLayoutTransform(S, Off));
		if (GFilm.bOn) { PaintFilm(Out, G, L0); return L0 + 4; }
		switch (GS.Scr)
		{
		case EScr::Hub: return PaintHub(Out, G, L0);
		case EScr::Card: return PaintCard(Out, G, L0);
		case EScr::Map: return PaintMap(Out, G, L0);
		case EScr::Dialogue: return PaintDialogue(Out, G, L0);
		case EScr::VS: return PaintVS(Out, G, L0);
		case EScr::Results: return PaintResults(Out, G, L0);
		case EScr::Clear: return PaintClear(Out, G, L0);
		default: return L0;
		}
	}
};

// ------------------------------------------------------------------------------------------------ flow
namespace ZoogiStoryDir
{
	static void Go(EScr Scr)
	{
		GS.Scr = Scr;
		if (GS.W.IsValid())
		{
			GS.W->Enter();
			GS.W->SetVisibility((Scr == EScr::None || Scr == EScr::Mission) ? EVisibility::Collapsed : EVisibility::Visible);
		}
		GS.bTestActed = false;
		static const TCHAR* Names[] = { TEXT("none"), TEXT("hub"), TEXT("card"), TEXT("map"), TEXT("dialogue"), TEXT("vs"), TEXT("mission"), TEXT("results"), TEXT("clear") };
		UE_LOG(LogZoogi, Log, TEXT("STORY screen: %s (episode %d, stop %d, mission %s, scene %s)"), Names[(int32)Scr], GS.Chapter + 1, GS.Spot, *GS.Mission, *GS.Scene);
	}
	static void GoHub()
	{
		StopMusic(0.6f);
		Go(EScr::Hub);
	}
	static void Back()
	{
		Sfx(TEXT("UiCancel"), 0.8f);
		if (GS.Scr == EScr::Map) Wipe(TEXT("BACK!"), Accent(), []() { GoHub(); });
		else Wipe(TEXT("BACK!"), Accent(), []() { Close(); });
	}
	static void PickChapter(int32 Ci)
	{
		if (!GDB.Chapters.IsValidIndex(Ci)) return;
		if (!ZoogiStorySave::ChapterOpen(Ci))
		{
			Sfx(TEXT("UiCancel"), 0.9f); Sfx(TEXT("GuardClang"), 0.5f, 1.3f);
			if (GS.W.IsValid()) { GS.W->DenySpot = 100 + Ci; GS.W->DenyT = GS.W->T; }
			return;
		}
		Sfx(TEXT("UiConfirm"), 0.9f);
		Wipe(GDB.Chapters[Ci].Ep + TEXT("!"), ElemCol(GDB.Chapters[Ci].Element), [Ci]()
		{
			GS.Chapter = Ci;
			const TArray<int32> Sp = ChapterSpots(Ci);
			int32 At = Sp.Num() ? Sp[0] : -1;
			for (int32 s : Sp) if (ZoogiStorySave::Cleared(s)) At = s;
			GS.Spot = At;
			if (GDB.Spots.IsValidIndex(At)) SetEnv(GDB.Spots[At].Env, GDB.Spots[At].bUnder);
			Go(EScr::Card);
		});
	}
	static void GoMap()
	{
		if (WipeBusy()) return;
		GS.bPanel = false;
		Wipe(TEXT("LET'S GO!"), ChapterCol(), []()
		{
			if (GDB.Spots.IsValidIndex(GS.Spot)) SetEnv(GDB.Spots[GS.Spot].Env, GDB.Spots[GS.Spot].bUnder);
			GS.bJustCleared = false;
			Go(EScr::Map);
		});
	}
	static void TapSpot(int32 Si)
	{
		if (!GS.W.IsValid() || GS.W->TravelT > 0.f || !GDB.Spots.IsValidIndex(Si)) return;
		if (!ZoogiStorySave::SpotOpen(Si))
		{
			Sfx(TEXT("UiCancel"), 0.9f); Sfx(TEXT("GuardClang"), 0.5f, 1.3f);
			GS.W->DenySpot = Si; GS.W->DenyT = GS.W->T;
			return;
		}
		GS.bJustCleared = false;
		if (Si == GS.Spot) { if (!GS.bPanel) Arrive(Si); return; }
		GS.bPanel = false;
		GS.W->TravelFrom = GS.Spot;
		GS.W->TravelTo = Si;
		GS.W->TravelT = GS.W->T;
		Sfx(TEXT("Dash"), 0.9f, 0.9f);
		Sfx(TEXT("Launch"), 0.5f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("STORY travel -> %s"), *GDB.Spots[Si].Label);
	}
	static void Arrive(int32 Si)
	{
		if (!GDB.Spots.IsValidIndex(Si)) return;
		const bool bNewPlace = !GDB.Spots.IsValidIndex(GS.Spot) || GDB.Spots[GS.Spot].Env != GDB.Spots[Si].Env;
		GS.Spot = Si;
		auto Up = []() { GS.bPanel = true; if (GS.W.IsValid()) GS.W->PanelT = GS.W->T; Sfx(TEXT("Clack"), 0.9f, 1.1f); };
		AZoogiGameMode* G = GM();
		if (bNewPlace || (G && (G->MenuArena != GDB.Spots[Si].Env || ZoogiUnderrealm::On() != GDB.Spots[Si].bUnder)))
			Wipe(GDB.Spots[Si].Label, ChapterCol(), [Si, Up]() { SetEnv(GDB.Spots[Si].Env, GDB.Spots[Si].bUnder); Up(); });
		else Up();
	}
	static bool FirstStop() { const TArray<int32> S = ZoogiStoryData::ChapterSpots(0); return S.Num() > 0 && GS.Spot == S[0]; }
	static bool GuideOn() { return GS.GuidePick < 0 ? !ZoogiGuide::DoneBefore() : GS.GuidePick > 0; }
	static void RollOut()
	{
		if (!GDB.Spots.IsValidIndex(GS.Spot)) return;
		const FSpot& Sp = GDB.Spots[GS.Spot];
		GS.Mission = Sp.Mission;
		GS.Place = Sp.Label;
		Sfx(TEXT("UiConfirm"), 0.9f);
		const FString Scene = Sp.Scene;
		Wipe(TEXT("ROLL OUT!"), ChapterCol(), [Scene]() { PlayScene(Scene, []() { GoVS(); }); });
	}
	static void PlayScene(const FString& Id, TFunction<void()> After)
	{
		GS.Scene = Id;
		GS.Line = 0;
		GS.After = MoveTemp(After);
		if (Id.IsEmpty() || !GDB.Scenes.Contains(Id)) { TFunction<void()> A = MoveTemp(GS.After); if (A) A(); return; }
		Go(EScr::Dialogue);
	}
	static void SceneDone()
	{
		StopFilm();
		TFunction<void()> A = MoveTemp(GS.After);
		GS.After = nullptr;
		if (A) Wipe(TEXT("!!"), ChapterCol(), [A]() { A(); });
	}
	static void GoVS()
	{
		if (const FMission* M = GDB.Missions.Find(GS.Mission)) { SetEnv(M->Env, M->bUnder); PlayMusic(M->Env, TEXT("Battle")); }
		Go(EScr::VS);
	}
	static void StartMission()
	{
		AZoogiGameMode* G = GM();
		const FMission* M = GDB.Missions.Find(GS.Mission);
		if (!G || !M) { Go(EScr::Map); return; }
		static const bool bOldCombo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryOldCombo"));
		const bool bTeams = M->Mode == TEXT("combo") && !bOldCombo; // b166: 2v2 story fights on the classic turn flow
		const int32 ShK = M->Mode == TEXT("shooter1") ? 1 : M->Mode == TEXT("shooter2") ? 2 : M->Mode == TEXT("shooter3") ? 3 : 0; // b235: EPISODE 5 missions
		ZoogiShooter::SetKind(ShK);
		const int32 Mode = (M->Mode == TEXT("combo") && bOldCombo) ? 1 : ((M->Mode == TEXT("royale") || ShK > 0) ? 2 : 0);
		if (G->Phase != EZoogiPhase::Menu) G->MenuAction(TEXT("Menu"), 0);
		G->MenuAction(TEXT("Mode"), Mode);
		G->bClassicTeams = bTeams;
		if (G->MenuPlayers.Num() < 4) G->MenuPlayers.SetNum(4);
		const int32 N = FMath::Clamp(M->Players.Num(), 2, 4);
		for (int32 i = 0; i < N; ++i)
		{
			G->MenuPlayers[i].ZoogiIndex = RosterIndex(M->Players.IsValidIndex(i) ? M->Players[i] : FString(TEXT("darkzoogy")));
			G->MenuPlayers[i].bCPU = (M->CPU.IsValidIndex(i) && M->CPU[i]) || (i == 0 && GS.bTest && Mode != 2);
		}
		G->MenuNumPlayers = N;
		G->MenuRounds = (GS.bTest && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryNoFinish"))) ? 1 : M->Rounds;
		if (G->MenuArena != M->Env) G->MenuAction(TEXT("Arena"), (int32)M->Env);
		StopMusic(0.3f);
		DuckMenuMusic(false);
		GS.MissionT = 0.f;
		GS.NextBattleShot = 3.f;
		GS.bOverPending = false;
		GS.bTestFinished = false;
		ZoogiGuide::Arm(FirstStop() && GuideOn() && !GS.bTest); // b67: the guided first turn, if the player said YES
		Go(EScr::Mission);
		UE_LOG(LogZoogi, Log, TEXT("STORY mission %s: %s, %d players on %s, tier %s, cards %s"), *M->Id, *M->Mode, N, *ZoogiUtil::ArenaName(M->Env), *M->Tier, G->CardsOn() ? TEXT("on") : TEXT("off (config)"));
		G->MenuAction(TEXT("Start"), 0);
	}
	static void MissionOver(AZoogiGameMode* G)
	{
		if (GS.bOverPending) return;
		GS.bOverPending = true;
		GS.bWon = G->bComboMode ? G->ComboWinner == 0 : G->bClassicTeams ? (G->WinnerIndex >= 0 && AZoogiGameMode::TeamOfIndex(G->WinnerIndex) == 0) : G->WinnerIndex == 0; // b166: story teams
		UE_LOG(LogZoogi, Log, TEXT("STORY mission %s over: %s (winner %d, combo winner %d)"), *GS.Mission, GS.bWon ? TEXT("CLEAR") : TEXT("FAILED"), G->WinnerIndex, G->ComboWinner);
		Wipe(GS.bWon ? TEXT("FINISH!") : TEXT("GAME OVER"), GS.bWon ? Accent() : FLinearColor(0.5f, 0.f, 0.05f), []()
		{
			GS.bOverPending = false;
			Go(EScr::Results);
		});
	}
	static void AfterWin()
	{
		if (GS.bChapterDone) { GS.bChapterDone = false; Go(EScr::Clear); return; }
		GS.bPanel = false;
		GS.bJustCleared = true;
		if (GDB.Spots.IsValidIndex(GS.Spot)) SetEnv(GDB.Spots[GS.Spot].Env, GDB.Spots[GS.Spot].bUnder);
		Go(EScr::Map);
	}
	static void ResultButton(int32 B)
	{
		AZoogiGameMode* G = GM();
		const FMission* M = GDB.Missions.Find(GS.Mission);
		Sfx(TEXT("UiConfirm"), 0.9f);
		if (GS.bWon)
		{
			if (GDB.Spots.IsValidIndex(GS.Spot))
			{
				const int32 Ci = ChapterOfSpot(GS.Spot);
				const bool bWasDone = ZoogiStorySave::Done(Ci) >= ZoogiStorySave::Total(Ci);
				ZoogiStorySave::GClear.Add(GDB.Spots[GS.Spot].Id);
				ZoogiStorySave::Save();
				GS.bChapterDone = !bWasDone && ZoogiStorySave::Done(Ci) >= ZoogiStorySave::Total(Ci);
			}
			const FString Win = M ? M->WinScene : FString();
			Wipe(TEXT("NEXT!"), ChapterCol(), [Win]()
			{
				if (AZoogiGameMode* G2 = GM()) if (G2->Phase != EZoogiPhase::Menu) G2->MenuAction(TEXT("Menu"), 0);
				if (const FMission* M2 = GDB.Missions.Find(GS.Mission)) SetEnv(M2->Env, M2->bUnder);
				GS.Place.Reset();
				PlayScene(Win, []() { AfterWin(); });
			});
			return;
		}
		if (B == 0) { Wipe(TEXT("ONE MORE!"), Accent(), []() { StartMission(); }); return; }
		Wipe(TEXT("BACK!"), Accent(), []()
		{
			if (AZoogiGameMode* G2 = GM()) if (G2->Phase != EZoogiPhase::Menu) G2->MenuAction(TEXT("Menu"), 0);
			GS.bPanel = false;
			if (GDB.Spots.IsValidIndex(GS.Spot)) SetEnv(GDB.Spots[GS.Spot].Env, GDB.Spots[GS.Spot].bUnder);
			Go(EScr::Map);
		});
		(void)G;
	}
	static void ClearDone()
	{
		Sfx(TEXT("UiConfirm"), 0.9f);
		Wipe(TEXT("STORY!"), Accent(), []() { GoHub(); });
	}
	static void Close()
	{
		StopFilm();
		StopMusic(0.5f);
		Go(EScr::None);
		if (GS.W.IsValid()) if (UGameViewportClient* V = GS.VC.Get()) V->RemoveViewportWidgetContent(GS.W.ToSharedRef());
		GS.W.Reset();
		if (AZoogiGameMode* G = GM()) if (G->Phase == EZoogiPhase::Menu) G->MenuAction(TEXT("Mode"), 0);
		DuckMenuMusic(false);
		UE_LOG(LogZoogi, Log, TEXT("STORY closed"));
	}

	// ---- -ZoogiStoryTest: drive the whole loop like a player would and capture frames
	static void TestShot(const FString& Name) { if (AZoogiGameMode* G = GM()) G->TakeShot(Name); }
	static void TickTest(float Dt)
	{
		if (!GS.bTest || GS.bTestQuit) return;
		AZoogiGameMode* G = GM();
		if (!G) return;
		if (GS.Clock > (FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryNoFinish")) ? 900.f : 240.f)) { GS.bTestQuit = true; UKismetSystemLibrary::QuitGame(G, G->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false); return; }
		if (GS.Scr == EScr::Mission)
		{
			if (GS.MissionT > GS.NextBattleShot && G->Phase != EZoogiPhase::Menu) { TestShot(FString::Printf(TEXT("story_battle_%02d"), FMath::RoundToInt(GS.NextBattleShot))); GS.NextBattleShot += 3.f; }
			static float NextWatch = 0.f;
			if (GS.MissionT < 0.5f) NextWatch = 1.f;
			if (GS.MissionT > NextWatch)
			{	// stall watch: phase / stage / whose turn / what is still moving
				NextWatch += GS.MissionT < 16.f ? 1.f : 5.f;
				FString Mv;
				for (AZoogiBall* B : G->Balls) if (B && B->Sphere) Mv += FString::Printf(TEXT(" P%d%s:%.0f@(%.0f,%.0f,%.0f)"), B->PlayerIndex + 1, B->bOut ? TEXT("(out)") : TEXT(""), B->Sphere->GetPhysicsLinearVelocity().Size(), B->BallLocation().X, B->BallLocation().Y, B->BallLocation().Z);
				int32 OrbMv = 0, OrbIn = 0;
				for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->Sphere) { ++OrbIn; if (O->Sphere->GetPhysicsLinearVelocity().Size() > 15.f) ++OrbMv; }
				UE_LOG(LogZoogi, Log, TEXT("STORY watch t=%.0f phase=%d stage=%d step=%d active=P%d phaseTime=%.1f orbs in=%d moving=%d balls%s"),
					GS.MissionT, (int32)G->Phase, (int32)G->Stage, (int32)G->BattleStep, G->ActivePlayer + 1, G->PhaseTime, OrbIn, OrbMv, *Mv);
			}
			return;
		}
		if (!GS.W.IsValid() || WipeBusy()) return;
		SZoogiStory& W = *GS.W;
		static const TCHAR* Names[] = { TEXT("none"), TEXT("hub"), TEXT("card"), TEXT("map"), TEXT("dlg"), TEXT("vs"), TEXT("mission"), TEXT("results"), TEXT("clear") };
		const FString Pre = FString::Printf(TEXT("story%d_%s"), GS.TestRound, Names[(int32)GS.Scr]);
		static const int32 ShotFrames[] = { 3, 6, 10, 16, 24, 40, 60 };
		for (int32 F : ShotFrames) if (W.Frame == F) TestShot(FString::Printf(TEXT("%s_f%03d"), *Pre, F));
		switch (GS.Scr)
		{
		case EScr::Hub:
			{	// -ZoogiStoryMission=<id>: straight into that battle (all CPU), e.g. the Volcanic 2v2 stall repro
				static FString Want; static bool bRead = false;
				if (!bRead) { bRead = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStoryMission="), Want); }
				if (!Want.IsEmpty() && !GS.bTestActed && W.T > 1.f)
				{
					GS.bTestActed = true;
					for (int32 s = 0; s < GDB.Spots.Num(); ++s) if (GDB.Spots[s].Mission == Want)
					{
						GS.Spot = s; GS.Chapter = ChapterOfSpot(s); GS.Mission = Want; GS.Place = GDB.Spots[s].Label;
						UE_LOG(LogZoogi, Log, TEXT("STORY test: jumping into mission %s"), *Want);
						StartMission();
						break;
					}
					break;
				}
			}
			if (!GS.bTestFilmDone && FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryFilm")))
			{	// -ZoogiStoryFilm: the film player (picture + sound) for ~6 s before the run carries on
				if (!GFilm.bOn) { if (W.T > 1.f && !StartFilm(TEXT("Story_EP1_P2"), true)) GS.bTestFilmDone = true; }
				else
				{
					if (W.Frame % 24 == 0) TestShot(FString::Printf(TEXT("story_film_%03d"), W.Frame));
					if (GFilm.T > 6.5f) { StopFilm(); GS.bTestFilmDone = true; W.T = 1.f; }
				}
				break;
			}
			if (!GS.bTestActed && W.T > 1.7f) { GS.bTestActed = true; PickChapter(0); }
			break;
		case EScr::Map:
			if (W.TravelT > 0.f && W.Frame % 3 == 0) TestShot(FString::Printf(TEXT("%s_travel_%03d"), *Pre, W.Frame));
			if (GS.TestRound >= 1)
			{
				if (W.Frame == 50) TestShot(Pre + TEXT("_end"));
				if (W.T > 3.5f && !GS.bTestQuit) { GS.bTestQuit = true; UKismetSystemLibrary::QuitGame(G, G->GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false); }
				break;
			}
			if (!GS.bPanel && W.TravelT < 0.f && W.T > 1.3f && !GS.bTestActed)
			{
				GS.bTestActed = true;
				const TArray<int32> Sp = ChapterSpots(GS.Chapter);
				if (Sp.Num()) TapSpot(Sp[0]);
			}
			if (GS.bPanel && W.T - W.PanelT > 1.2f) { TestShot(Pre + TEXT("_panel")); RollOut(); }
			break;
		case EScr::Dialogue:
			if (W.Frame % 12 == 0) TestShot(FString::Printf(TEXT("%s_%s_%02d_%03d"), *Pre, *GS.Scene, GS.Line, W.Frame));
			break;
		case EScr::VS:
			if (!GS.bTestActed && W.T > 2.2f) { GS.bTestActed = true; W.StartMissionWipe(); }
			break;
		case EScr::Results:
			if (!GS.bTestActed && W.T > 2.8f) { GS.bTestActed = true; ++GS.TestRound; ResultButton(GS.bWon ? 0 : 1); }
			break;
		default: break;
		}
	}

	static bool TickDir(float Dt)
	{
		if (GS.Scr == EScr::None) { GS.Tick.Reset(); return false; }
		GS.Clock += Dt;
		AZoogiGameMode* G = GM();
		if (!G) return true;
		if (GS.Scr == EScr::Mission)
		{
			GS.MissionT += Dt;
			if (G->Phase == EZoogiPhase::GameOver && G->PhaseTime > 1.4f && !WipeBusy()) MissionOver(G);
			else if (G->Phase == EZoogiPhase::Menu && GS.MissionT > 2.f && !GS.bOverPending)
			{	// left the battle from the pause menu: back on the map
				UE_LOG(LogZoogi, Log, TEXT("STORY mission abandoned -> map"));
				GS.bPanel = false;
				Go(EScr::Map);
			}
		}
		else if (G->Phase == EZoogiPhase::Menu) DuckMenuMusic(GS.Music.IsValid()); // a place theme is playing: the menu loop steps aside
		TickTest(Dt);
		return true;
	}
}

// ------------------------------------------------------------------------------------------------ API
namespace ZoogiStory
{
	using namespace ZoogiStoryDir;
	void Open(UWorld* World)
	{
		UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
		if (!VC) return;
		ZoogiStoryData::Load();
		ZoogiStorySave::Load();
		if (GS.W.IsValid()) Close();
		if (GS.Tick.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(GS.Tick);
		GS = FDir();
		GS.World = World;
		GS.VC = VC;
		GS.bTest = ZoogiStorySave::Test();
		GS.W = SNew(SZoogiStory);
		VC->AddViewportWidgetContent(GS.W.ToSharedRef(), 30);
		GS.Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickDir));
		Sfx(TEXT("UiConfirm"), 0.9f);
		Go(EScr::Hub);
		UE_LOG(LogZoogi, Log, TEXT("STORY opened (%d stops cleared)"), ZoogiStorySave::GClear.Num());
	}
	bool IsOpen() { return GS.Scr != EScr::None; }
	bool InMission() { return GS.Scr == EScr::Mission || GS.Scr == EScr::Results; }
	bool BlocksHud(const AZoogiGameMode* G)
	{
		if (GS.Scr == EScr::None) return false;
		if (GS.Scr != EScr::Mission) return true;
		return G && G->Phase == EZoogiPhase::GameOver;
	}
	bool TierOverride(EZoogiCPUTier& Out)
	{
		if (GS.Scr != EScr::Mission) return false;
		const ZoogiStoryData::FMission* M = ZoogiStoryData::GDB.Missions.Find(GS.Mission);
		if (!M) return false;
		const FString T = M->Tier.ToLower();
		Out = T == TEXT("easy") ? EZoogiCPUTier::Easy : (T == TEXT("hard") ? EZoogiCPUTier::Hard : (T == TEXT("expert") ? EZoogiCPUTier::Expert : EZoogiCPUTier::Normal));
		return true;
	}
	bool TestWantsFinish()
	{
		if (!GS.bTest || GS.Scr != EScr::Mission || GS.bTestFinished || GS.MissionT < 14.f) return false;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryNoFinish"))) return false; // the battle plays out (stall repro)
		GS.bTestFinished = true;
		UE_LOG(LogZoogi, Log, TEXT("STORY test: finishing the battle"));
		return true;
	}
	void AutoOpenForTest(UWorld* World)
	{
		static bool bDone = false;
		static double T0 = 0.0;
		if (bDone || !ZoogiStorySave::Test()) return;
		if (T0 == 0.0) T0 = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - T0 < 2.5) return;
		bDone = true;
		Open(World);
	}
}
