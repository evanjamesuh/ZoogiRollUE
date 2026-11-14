#pragma once
// Oct 27 (Evan's Dev Mode spec, Part 2): the Camera Configuration Manager. Camera numbers per game mode, in three switchable
// profiles (A standard / B close cinematic / C overhead tactical), edited live from the Dev Mode CAMERA tab and saved in the
// dev store (ZoogiDevConfig, scope "mode:<mode>"). Ordinary players (no dev unlock) always get the shipped values below;
// an EXPORT of the store is how chosen values become the shipped ones.
#include "CoreMinimal.h"

struct ZOOGIROLL_API FZoogiCamSettings
{
	// royale / training chase camera (ZoogiRoyaleInput.cpp)
	float Back = 270.f, Up = 125.f, Pitch = 1.f, Fov = 95.f;   // behind / above the Zoogi (cm), look-down degrees, horizontal FOV
	float RobotBack = 470.f, RobotUp = 175.f;                  // robot form
	float Follow = 28.f;                                       // how tightly the camera follows the Zoogi (1/s)
	float Turn = 14.f;                                         // how fast it turns to the aim (1/s)
	float Shake = 1.f;                                         // camera shake strength (0 = off)
	// classic / 2v2 tactical camera (ZoogiCamTune Tactical profile)
	float TacPitch = 40.f, TacDist = 2000.f, TacFov = 78.f, TacLag = 2.f;
};

namespace ZoogiCamConfig
{
	struct FParam
	{
		const TCHAR* Key;
		const TCHAR* Label;
		float Min, Max, Step;
		bool bChase; // true: royale / training chase camera; false: classic tactical camera
		float FZoogiCamSettings::* Field;
	};
	ZOOGIROLL_API const TArray<FParam>& Params();

	/** "classic" | "combo" | "royale" | "training" for the running match. */
	ZOOGIROLL_API FString ModeName(const UObject* WorldContext);
	ZOOGIROLL_API bool IsChaseMode(const FString& Mode);

	/** The shipped numbers for a profile (A = the game's defaults, B close, C overhead). */
	ZOOGIROLL_API FZoogiCamSettings ProfileDefaults(TCHAR Profile);
	/** What the camera uses now for this mode: shipped A unless the device is dev-unlocked (then its active profile). */
	ZOOGIROLL_API const FZoogiCamSettings& Get(const FString& Mode);

	ZOOGIROLL_API TCHAR ActiveProfile(const FString& Mode);
	ZOOGIROLL_API void SetActiveProfile(const FString& Mode, TCHAR Profile);
	ZOOGIROLL_API float GetValue(const FString& Mode, TCHAR Profile, const FParam& P);
	ZOOGIROLL_API void SetValue(const FString& Mode, TCHAR Profile, const FParam& P, float Value);
	ZOOGIROLL_API void ResetProfile(const FString& Mode, TCHAR Profile);
}
