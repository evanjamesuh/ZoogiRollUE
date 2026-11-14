// b101 (Evan: "a full gameplay epic of the best use cases ... P5 banners, teach how to do each"): the MASTERCLASS film recorder.
// Only with -ZoogiFilm on the command line (shipping / normal runs untouched). While a clip is recording, every frame is captured
// WITH the HUD (UI screenshot -> UGameViewportClient::OnScreenshotCaptured) and written as a JPEG on a worker thread, in real time,
// with its real timestamp (times.txt) so the editor can retime the clip to a constant 30 fps without speeding anything up.
//   -ZoogiShots=<dir>        clips go to <dir>/<name>_<n>/f_00000.jpg + times.txt
//   -ZoogiFilmRoll=<secs>    also record each of P1's rolls from its launch for <secs> (-ZoogiFilmMaxClips=N, default 6)
//   -ZoogiAwakenTest         P1's roll: full meter at 0.6 s, then SUPERPOWER (its Awaken power), recorded
//   -ZoogiAwakenBall=<Ball>  P1 rolls as that Zoogi (skin + Awaken power)
//   b214: every clip records the game's sound too (<clip>/audio.wav, -ZoogiFilmNoAudio = off); -ZoogiFilmNoUI = no HUD at all;
//   -ZoogiFilmAt=<s> -ZoogiFilmLen=<s> -ZoogiFilmName=<n> = one clip at a set time; -ZoogiFilmBoss=<s> = a clip on every titan attack
//   -ZoogiFilmWolfTaps       the human's Wolf Pack pick taps its targets one by one (rivals first)
#pragma once
#include "CoreMinimal.h"

class UWorld;

namespace ZoogiFilm
{
	ZOOGIROLL_API bool On();
	/** Hooks the per-frame tick (call from the game mode's BeginPlay). */
	ZOOGIROLL_API void Start(UWorld* W);
	/** Starts a clip of Seconds (real); a clip already recording is extended instead. */
	ZOOGIROLL_API void Begin(const FString& Name, float Seconds);
	ZOOGIROLL_API bool Recording();
	/** b214: a named moment (Boss, ...) - records -ZoogiFilm<Kind>=<s> seconds when that switch is given. */
	ZOOGIROLL_API void Moment(const TCHAR* Kind);
	/** A line in the clip's events.txt (frame number + text). */
	ZOOGIROLL_API void Mark(const FString& Ev);
}
