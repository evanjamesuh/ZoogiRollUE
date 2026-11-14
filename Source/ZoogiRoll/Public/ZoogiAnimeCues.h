// b164 (Evan Nov 6 13:45 / 14:03): the ANIME DIRECTOR. Evan places the anime clips himself - when they play (a match moment, a story
// line), on which modes / maps / Zoogis, how often, full screen or in a window (landscape and portrait set separately), the intro / exit
// animation templates, what the game does meanwhile (pause / slow / keep playing), music duck, skip and trim. Edited in the PC game
// (Dev Mode -> ANIME DIRECTOR, or F6), saved to Content/Data/Anime/AnimeCues.json, which the phone build reads unchanged.
// A cue that matches an event replaces the built-in cut-in for that moment; story cues replace the Dialogue.csv "film" rows.
// -ZoogiNoAnimeCues = the old behaviour.
#pragma once
#include "CoreMinimal.h"
class UWorld;
class AZoogiGameMode;

namespace ZoogiAnimeCues
{
	/** a window: centre (X, Y) and size (W, H) as fractions of the screen (portrait: of the phone screen) */
	struct FRect01 { float X = 0.5f, Y = 0.5f, W = 1.f, H = 1.f; bool bFull = true; };
	struct FCue
	{
		FString Id, Clip;
		bool bOn = true;
		// when
		FString Trigger = TEXT("KO");          // KO, Critical, Combo, PoweredHit, GuardBlock, Guard, Infuse, Miss, Ultimate, Showtime, RoundStart, FinalRound, MatchWin, MatchLose, BossAttack, TrickShot3Star, Story
		FString Mode = TEXT("any");            // any, classic, battle, royale, raid, story, training, trickshot
		FString Arena = TEXT("any");           // any or an arena key (meadow, frozen, ...)
		FString Zoogi = TEXT("any");           // any or a roster id
		FString Who = TEXT("any");             // any, human, cpu
		FString Rule = TEXT("first");          // first (once per match), always, chance
		float Chance = 1.f, Cooldown = 0.f;
		FString Scene; int32 Line = 0;         // Trigger = Story: before line <Line> of Dialogue.csv scene <Scene>
		// look
		FRect01 Land, Port;
		FString Border = TEXT("p5");           // none, p5, thin, comic
		FString Intro = TEXT("p5slash"), Outro = TEXT("fade"); // none, fade, zoom, slide_left, slide_right, slide_up, slide_down, wipe, p5slash, comic, shatter (exit)
		float IntroSec = 0.3f, OutroSec = 0.25f;
		// game
		FString Game = TEXT("pause");          // pause, slow, play
		float Slow = 0.25f, Duck = 0.3f, SkipAfter = 0.5f, Volume = 1.f, TrimStart = 0.f, TrimEnd = 0.f;
	};
	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API TArray<FCue>& Cues();
	ZOOGIROLL_API FString FilePath();
	ZOOGIROLL_API bool Load();
	ZOOGIROLL_API bool Save();
	ZOOGIROLL_API TArray<FString> Clips();                 // Content/Anime/Movies/*.mp4 ids
	ZOOGIROLL_API const TArray<FString>& Triggers();
	ZOOGIROLL_API const TArray<FString>& Templates(bool bExit);
	/** a combat event (UZoogiAnimeSubsystem::Emit): true when a cue played (the built-in cut-in is skipped) */
	ZOOGIROLL_API bool OnEvent(UWorld* W, const FString& Event, const FString& ZoogiId, int32 Player);
	/** other match moments (BossAttack, TrickShot3Star, ...) */
	ZOOGIROLL_API bool Fire(UWorld* W, const FString& Event, int32 Player = -1);
	/** story: a line is about to show; true when a cue started */
	ZOOGIROLL_API bool OnStoryLine(UWorld* W, const FString& Scene, int32 Line);
	/** the cues file has story cues: Dialogue.csv film rows are skipped */
	ZOOGIROLL_API bool StoryOwnsFilms();
	/** RoundStart / FinalRound / MatchWin / MatchLose detection + the player clock (called every frame from the action layer) */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float Dt);
	ZOOGIROLL_API bool Play(UWorld* W, const FCue& C, bool bPreview = false);
	ZOOGIROLL_API void Stop();
	ZOOGIROLL_API void Skip();
	ZOOGIROLL_API bool Playing();
	/** playing and the game should wait (Game = pause / slow) */
	ZOOGIROLL_API bool Busy();
	/** time dilation the cue wants (0 = none) */
	ZOOGIROLL_API float WantDilation();
	// ---- director (PC)
	ZOOGIROLL_API void ToggleDirector();
	/** b180 (Evan: "make f6 active in any mode at anytime"): a Slate pre-processor catches F6 everywhere (menu, any mode, panel focused). */
	ZOOGIROLL_API void EnsureF6Hook();
	ZOOGIROLL_API bool DirectorOpen();
	ZOOGIROLL_API void ResetMatch();
}
