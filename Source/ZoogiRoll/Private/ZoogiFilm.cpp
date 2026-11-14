#include "ZoogiFilm.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiActionFun.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "UnrealClient.h"
#include "AudioMixerBlueprintLibrary.h" // b214: the clip records the game's own sound (audio.wav)
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include <atomic>

namespace ZoogiFilm
{
	namespace
	{
		TWeakObjectPtr<UWorld> GW;
		FTSTicker::FDelegateHandle GTick;
		FDelegateHandle GShot;
		bool GRec = false;
		double GT0 = 0.0, GEndAt = 0.0;
		int32 GFrame = 0, GClips = 0, GMaxClips = 6, GPending = 0;
		FString GDir, GTimes, GEvents;
		TArray<double> GReqT;
		float GRollSecs = 0.f;
		EZoogiPhase GPrevPhase = EZoogiPhase::Menu; int32 GPrevTurn = -1; int32 GUltTurn = -1;
		std::atomic<int32> GInFlight{0};
		// b214 (Evan: "add audio recording to the game's video capture, so every hit, voice line and crowd reaction lands exactly in sync"):
		// every clip also records the master mix from its first frame -> <clip>/audio.wav (frame 0 = audio 0 s). -ZoogiFilmNoAudio = off.
		bool GAudio = false;
		double GStartT = 0.0; float GAt = -1.f, GAtLen = 20.f; FString GAtName = TEXT("film"); bool GAtDone = false;
		bool AudioOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmNoAudio")); return b; }
		bool NoUI() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmNoUI")); return b; }

		FString Root()
		{
			FString D; if (!FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), D)) D = FPaths::ProjectSavedDir() / TEXT("Film");
			return D;
		}
		void Flush()
		{
			if (GDir.IsEmpty()) return;
			FFileHelper::SaveStringToFile(GTimes, *(GDir / TEXT("times.txt")));
			FFileHelper::SaveStringToFile(GEvents, *(GDir / TEXT("events.txt")));
		}
		void Stop()
		{
			if (!GRec) return;
			GRec = false;
			GEvents += FString::Printf(TEXT("%d end\n"), GFrame);
			if (GAudio)
			{	// b214: the clip's sound, saved beside its frames
				GAudio = false;
				if (UWorld* W = GW.Get()) UAudioMixerBlueprintLibrary::StopRecordingOutput(W, EAudioRecordingExportType::WavFile, TEXT("audio"), FPaths::ConvertRelativePathToFull(GDir));
				UE_LOG(LogZoogi, Log, TEXT("FILM audio -> %s/audio.wav"), *GDir);
			}
			Flush();
			UE_LOG(LogZoogi, Log, TEXT("FILM clip done: %s (%d frames, %.1f s)"), *GDir, GFrame, FPlatformTime::Seconds() - GT0);
		}
		void OnShot(int32 W, int32 H, const TArray<FColor>& Px)
		{
			if (GDir.IsEmpty() || GReqT.Num() == 0) return;
			const double T = GReqT[0]; GReqT.RemoveAt(0);
			const int32 Idx = GFrame++;
			GTimes += FString::Printf(TEXT("%d %.4f\n"), Idx, T);
			if (GInFlight.load() > 24) { UE_LOG(LogZoogi, Warning, TEXT("FILM writer behind - frame %d dropped"), Idx); return; }
			++GInFlight;
			const FString Path = GDir / FString::Printf(TEXT("f_%05d.jpg"), Idx);
			TArray<FColor> Copy = Px;
			Async(EAsyncExecution::ThreadPool, [Copy = MoveTemp(Copy), W, H, Path]() mutable
			{
				for (FColor& C : Copy) C.A = 255;
				TArray64<uint8> Jpg;
				if (FImageUtils::CompressImage(Jpg, TEXT("jpg"), FImageView(Copy.GetData(), W, H), 92)) FFileHelper::SaveArrayToFile(Jpg, *Path);
				--GInFlight;
			});
			if (Idx % 60 == 0) Flush();
		}
		bool TickFn(float)
		{
			UWorld* W = GW.Get();
			if (!W) return true;
			const double Now = FPlatformTime::Seconds();
			AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>();
			if (G)
			{
				// P1's roll from its launch
				const bool bNewRoll = G->Phase == EZoogiPhase::Rolling && (GPrevPhase != EZoogiPhase::Rolling || GPrevTurn != G->TurnCounter) && G->ActivePlayer == 0;
				if (bNewRoll && GRollSecs > 0.f) Begin(TEXT("roll"), GRollSecs);
				// the Awaken test: full meter at 0.6 s, then SUPERPOWER
				static const bool bAw = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAwakenTest"));
				if (bAw && G->Phase == EZoogiPhase::Rolling && G->ActivePlayer == 0 && G->PhaseTime > 0.6f && GUltTurn != G->TurnCounter)
					if (UZoogiFunSys* F = UZoogiFunSys::Get(G))
					{
						F->FillMeterForTest(G, 0);
						if (F->TryUltimate(G)) { GUltTurn = G->TurnCounter; Begin(TEXT("awaken"), 9.f); Mark(TEXT("awaken")); UE_LOG(LogZoogi, Log, TEXT("FILM awaken test fired")); }
						else if (G->PhaseTime > 3.f) GUltTurn = G->TurnCounter;
					}
				GPrevPhase = G->Phase; GPrevTurn = G->TurnCounter;
			}
			// b214: -ZoogiFilmAt=<s> starts one clip that many seconds after the recorder hooks in (-ZoogiFilmLen=<s>, -ZoogiFilmName=<n>):
			// the front end, the tutorial, anything that is not a P1 roll
			if (GAt >= 0.f && !GAtDone && Now - GStartT >= GAt) { GAtDone = true; Begin(GAtName, GAtLen); }
			if (GRec && NoUI())
				if (APlayerController* Pc = W->GetFirstPlayerController()) if (Pc->MyHUD) Pc->MyHUD->bShowHUD = false; // b214: clean footage (no canvas HUD either)
			if (!GRec) return true;
			if (Now >= GEndAt) { Stop(); return true; }
			static const bool bNoFrames = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmNoFrames")); // b101b: Game Bar records the screen + audio
			if (bNoFrames) return true;
			GReqT.Add(Now - GT0);
			FScreenshotRequest::RequestScreenshot(GDir / TEXT("req.png"), !NoUI(), false); // b213: -ZoogiFilmNoUI = a clean film
			return true;
		}
	}

	bool On() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilm")); return b; }

	void Start(UWorld* W)
	{
		if (!On() || !W) return;
		GW = W;
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiFilmRoll="), GRollSecs);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiFilmMaxClips="), GMaxClips);
		GStartT = FPlatformTime::Seconds(); FParse::Value(FCommandLine::Get(), TEXT("ZoogiFilmAt="), GAt); FParse::Value(FCommandLine::Get(), TEXT("ZoogiFilmLen="), GAtLen); FParse::Value(FCommandLine::Get(), TEXT("ZoogiFilmName="), GAtName); // b214
		if (!GTick.IsValid()) GTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickFn));
		if (!GShot.IsValid()) GShot = UGameViewportClient::OnScreenshotCaptured().AddStatic(&OnShot);
		UE_LOG(LogZoogi, Log, TEXT("FILM recorder ready -> %s (roll clips %.1f s, max %d clips)"), *Root(), GRollSecs, GMaxClips);
	}

	void Begin(const FString& Name, float Seconds)
	{
		if (!On()) return;
		const double Now = FPlatformTime::Seconds();
		if (GRec) { GEndAt = FMath::Max(GEndAt, Now + Seconds); GEvents += FString::Printf(TEXT("%d %s (extended)\n"), GFrame, *Name); return; }
		if (GClips >= GMaxClips) return;
		++GClips;
		GDir = Root() / FString::Printf(TEXT("%02d_%s"), GClips, *Name);
		IFileManager::Get().MakeDirectory(*GDir, true);
		GRec = true; GT0 = Now; GEndAt = Now + Seconds; GFrame = 0; GTimes.Reset(); GEvents.Reset(); GReqT.Reset();
		if (AudioOn()) if (UWorld* W = GW.Get()) { UAudioMixerBlueprintLibrary::StartRecordingOutput(W, Seconds + 30.f); GAudio = true; } // b214
		GEvents += FString::Printf(TEXT("0 begin %s\n"), *Name);
		{	// b101b: wall-clock moments for lining up an outside (Game Bar) recording
			const FDateTime Dt = FDateTime::Now();
			const FString Line = FString::Printf(TEXT("%s.%03d begin %s %.1f\n"), *Dt.ToString(TEXT("%Y-%m-%d %H:%M:%S")), Dt.GetMillisecond(), *Name, Seconds);
			FFileHelper::SaveStringToFile(Line, *(Root() / TEXT("moments.txt")), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
		}
		UE_LOG(LogZoogi, Log, TEXT("FILM clip %s started (%.1f s)"), *GDir, Seconds);
	}

	bool Recording() { return GRec; }

	void Moment(const TCHAR* Kind)
	{	// b214: -ZoogiFilm<Kind>=<s> records that long from this moment (e.g. -ZoogiFilmBoss=7 on every titan attack)
		if (!On()) return;
		float Secs = 0.f;
		if (FParse::Value(FCommandLine::Get(), *FString::Printf(TEXT("ZoogiFilm%s="), Kind), Secs) && Secs > 0.f) { Begin(FString(Kind).ToLower(), Secs); Mark(Kind); }
	}

	void Mark(const FString& Ev)
	{
		const FDateTime Dt = FDateTime::Now();
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s.%03d mark %s\n"), *Dt.ToString(TEXT("%Y-%m-%d %H:%M:%S")), Dt.GetMillisecond(), *Ev), *(Root() / TEXT("moments.txt")), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
		if (GRec) GEvents += FString::Printf(TEXT("%d %s\n"), GFrame, *Ev); UE_LOG(LogZoogi, Log, TEXT("FILM mark %s (frame %d)"), *Ev, GFrame); }
}
