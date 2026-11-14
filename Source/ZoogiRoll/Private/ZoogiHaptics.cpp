// Oct 22: see ZoogiHaptics.h
#include "ZoogiHaptics.h"
#include "ZoogiRoll.h"
#include "ZoogiFramedLayout.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#if PLATFORM_IOS
#import <CoreHaptics/CoreHaptics.h>
#include <dlfcn.h>
#import <AudioToolbox/AudioToolbox.h>
#endif
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace ZoogiHaptics
{
	struct FTune
	{
		bool bEnabled = true, bLogAll = false;
		bool bVibratorFallback = true, bForceVibratorUnsupported = false;
		float HitMinImpulse = 1500.f, HitFullImpulse = 32000.f, HitMinInterval = 0.09f;
		float HitMinIntensity = 0.25f, HitMaxIntensity = 1.f, HitMinDuration = 0.035f, HitMaxDuration = 0.16f;
		float ReleaseMinIntensity = 0.45f, ReleaseDuration = 0.12f;
		float AimMinPull = 0.05f, AimSlowInterval = 0.24f, AimFastInterval = 0.06f, AimMinIntensity = 0.12f, AimMaxIntensity = 0.65f, AimPulseDuration = 0.035f;
		// Visual stand-in scales when vibrator unsupported (do not change the intensity fields above).
		float FallbackAimHudMul = 1.75f, FallbackAimPulse = 0.28f, FallbackHitShake = 0.85f, FallbackReleaseShake = 0.9f;
		FTune()
		{
			const TCHAR* S = TEXT("ZoogiRoll.Haptics");
			if (GConfig)
			{
				GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
				GConfig->GetBool(S, TEXT("bVibratorFallback"), bVibratorFallback, GGameIni);
				GConfig->GetBool(S, TEXT("bForceVibratorUnsupported"), bForceVibratorUnsupported, GGameIni);
				GConfig->GetFloat(S, TEXT("HitMinImpulse"), HitMinImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("HitFullImpulse"), HitFullImpulse, GGameIni);
				GConfig->GetFloat(S, TEXT("HitMinInterval"), HitMinInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("HitMinIntensity"), HitMinIntensity, GGameIni);
				GConfig->GetFloat(S, TEXT("HitMaxIntensity"), HitMaxIntensity, GGameIni);
				GConfig->GetFloat(S, TEXT("HitMinDuration"), HitMinDuration, GGameIni);
				GConfig->GetFloat(S, TEXT("HitMaxDuration"), HitMaxDuration, GGameIni);
				GConfig->GetFloat(S, TEXT("ReleaseMinIntensity"), ReleaseMinIntensity, GGameIni);
				GConfig->GetFloat(S, TEXT("ReleaseDuration"), ReleaseDuration, GGameIni);
				GConfig->GetFloat(S, TEXT("AimMinPull"), AimMinPull, GGameIni);
				GConfig->GetFloat(S, TEXT("AimSlowInterval"), AimSlowInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("AimFastInterval"), AimFastInterval, GGameIni);
				GConfig->GetFloat(S, TEXT("AimMinIntensity"), AimMinIntensity, GGameIni);
				GConfig->GetFloat(S, TEXT("AimMaxIntensity"), AimMaxIntensity, GGameIni);
				GConfig->GetFloat(S, TEXT("AimPulseDuration"), AimPulseDuration, GGameIni);
				GConfig->GetFloat(S, TEXT("FallbackAimHudMul"), FallbackAimHudMul, GGameIni);
				GConfig->GetFloat(S, TEXT("FallbackAimPulse"), FallbackAimPulse, GGameIni);
				GConfig->GetFloat(S, TEXT("FallbackHitShake"), FallbackHitShake, GGameIni);
				GConfig->GetFloat(S, TEXT("FallbackReleaseShake"), FallbackReleaseShake, GGameIni);
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHaptics"))) bEnabled = false;
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVibratorFallback"))) bVibratorFallback = false;
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiVibratorUnsupported"))) bForceVibratorUnsupported = true;
			bLogAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHapticsLog"));
			HitFullImpulse = FMath::Max(HitFullImpulse, HitMinImpulse + 1.f);
			AimFastInterval = FMath::Max(AimFastInterval, AimPulseDuration + 0.01f);
			FallbackAimHudMul = FMath::Clamp(FallbackAimHudMul, 1.f, 3.f);
			FallbackAimPulse = FMath::Clamp(FallbackAimPulse, 0.f, 1.f);
			FallbackHitShake = FMath::Clamp(FallbackHitShake, 0.f, 1.f);
			FallbackReleaseShake = FMath::Clamp(FallbackReleaseShake, 0.f, 1.f);
			// UE's Android vibrator is on/off above 0.3 by default; 0 = intensity follows the value (API 26+ amplitude)
			if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("Android.VibrationThreshold"))) CV->Set(0.f, ECVF_SetByGameSetting);
		}
	};
	static const FTune& Tune() { static FTune T; return T; }

	static double GLastHit = -10.0, GLastLog = -10.0, GNextAimPulse = 0.0;
	static float GAimPull = 0.f;
	static double GAimTickAt = -10.0;
	static int32 GCount[3] = { 0, 0, 0 };
	static bool GVibChecked = false, GVibratorUnsupported = false;

#if PLATFORM_ANDROID
	static bool AndroidHasVibratorFeature()
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env) return true; // cannot probe: assume present so we do not force the fallback wrongly
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Activity) return true;
		jclass ActivityClass = Env->GetObjectClass(Activity);
		if (!ActivityClass) return true;
		jmethodID GetPM = Env->GetMethodID(ActivityClass, "getPackageManager", "()Landroid/content/pm/PackageManager;");
		if (!GetPM) { Env->DeleteLocalRef(ActivityClass); return true; }
		jobject PM = Env->CallObjectMethod(Activity, GetPM);
		Env->DeleteLocalRef(ActivityClass);
		if (!PM || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck()) { Env->ExceptionClear(); }
			return true;
		}
		jclass PMClass = Env->GetObjectClass(PM);
		jmethodID HasFeat = PMClass ? Env->GetMethodID(PMClass, "hasSystemFeature", "(Ljava/lang/String;)Z") : nullptr;
		jstring Feat = Env->NewStringUTF("android.hardware.vibrator");
		bool bHas = true;
		if (HasFeat && Feat) bHas = Env->CallBooleanMethod(PM, HasFeat, Feat) == JNI_TRUE;
		if (Env->ExceptionCheck()) { Env->ExceptionClear(); bHas = true; }
		if (Feat) Env->DeleteLocalRef(Feat);
		if (PMClass) Env->DeleteLocalRef(PMClass);
		Env->DeleteLocalRef(PM);
		return bHas;
	}
#endif

	void RefreshVibratorSupport()
	{
		const FTune& T = Tune();
		GVibChecked = true;
		if (!T.bVibratorFallback)
		{
			GVibratorUnsupported = false;
			UE_LOG(LogZoogi, Log, TEXT("HAPTIC vibrator fallback off"));
			return;
		}
		if (T.bForceVibratorUnsupported)
		{
			GVibratorUnsupported = true;
			UE_LOG(LogZoogi, Log, TEXT("HAPTIC vibrator unsupported (forced) -> HUD/frame shake fallback"));
			return;
		}
#if PLATFORM_ANDROID
		const bool bHas = AndroidHasVibratorFeature();
		GVibratorUnsupported = !bHas;
		UE_LOG(LogZoogi, Log, TEXT("HAPTIC Android vibrator feature=%d -> fallback=%d"), bHas ? 1 : 0, GVibratorUnsupported ? 1 : 0);
#else
		GVibratorUnsupported = false; // desktop/editor: gamepad motors via same API; use force flag to test fallback
		UE_LOG(LogZoogi, Log, TEXT("HAPTIC vibrator probe skipped (non-Android); fallback off unless forced"));
#endif
	}

	bool VibratorUnsupported()
	{
		if (!GVibChecked) RefreshVibratorSupport();
		return GVibratorUnsupported && Tune().bVibratorFallback;
	}

	static bool MenuOn()
	{
		int32 V = 1;
		if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), TEXT("Haptics"), V, GGameUserSettingsIni);
		return V != 0;
	}
	bool Enabled() { return Tune().bEnabled && MenuOn(); }
	float AimPull() { return FPlatformTime::Seconds() - GAimTickAt < 0.25 ? GAimPull : 0.f; } // stale drag (turn changed) -> 0
	float AimHudMul() { return VibratorUnsupported() ? Tune().FallbackAimHudMul : 1.f; }
	int32 Count(int32 Kind) { return Kind >= 0 && Kind < 3 ? GCount[Kind] : 0; }

	static double Now(UWorld* World) { return World ? World->GetRealTimeSeconds() : FPlatformTime::Seconds(); }

	static void FallbackShake(float Strength01)
	{
		if (!VibratorUnsupported() || Strength01 <= 0.01f) return;
		ZoogiFramed::FrameShakeVisual(FMath::Clamp(Strength01, 0.f, 1.f));
	}

	static bool Play(UWorld* World, float Intensity, float Duration)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->IsLocalController()) return false;
		PC->PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), FMath::Max(Duration, 0.02f), true, true, true, true);
		return true;
	}

	static void Log(UWorld* World, bool bForce, const FString& Msg)
	{
		const double T = Now(World);
		if (!bForce && !Tune().bLogAll && T - GLastLog < 0.5) return;
		GLastLog = T;
		UE_LOG(LogZoogi, Log, TEXT("HAPTIC %s (hits %d releases %d aimpulses %d vibUnsupported=%d)"),
			*Msg, GCount[0], GCount[1], GCount[2], VibratorUnsupported() ? 1 : 0);
	}

	void Kick(float Strength, UWorld* World, const TCHAR* What)
	{
		if (!Enabled() || !World) return;
		const FTune& T = Tune();
		const float K = FMath::Clamp(Strength, 0.f, 1.f);
		const double Tn = Now(World);
		if (Tn - GLastHit < T.HitMinInterval) return; // throttle: rapid collisions don't spam the motor
		const float I = FMath::Lerp(T.HitMinIntensity, T.HitMaxIntensity, K);
		const float D = FMath::Lerp(T.HitMinDuration, T.HitMaxDuration, K);
		const bool bPlayed = Play(World, I, D);
		FallbackShake(K * T.FallbackHitShake); // vibratorless: same strength scale via HUD/frame shake
		if (!bPlayed && !VibratorUnsupported()) return;
		GLastHit = Tn; ++GCount[0];
		Log(World, false, FString::Printf(TEXT("%s k=%.2f intensity=%.2f dur=%.3f"), What, K, I, D));
	}

	void Hit(float Impulse, UWorld* World, const TCHAR* What)
	{
		const FTune& T = Tune();
		if (Impulse < T.HitMinImpulse) return;
		Kick((Impulse - T.HitMinImpulse) / (T.HitFullImpulse - T.HitMinImpulse), World, What);
	}

	void AimStop(UWorld* World)
	{
		if (GAimPull > 0.f) Log(World, true, TEXT("aim stop"));
		GAimPull = 0.f; GNextAimPulse = 0.0;
	}

	void Release(float Power, UWorld* World)
	{
		GAimPull = 0.f; GNextAimPulse = 0.0;
		if (!Enabled() || !World) return;
		const FTune& T = Tune();
		const float K = FMath::Clamp(Power, 0.f, 1.f);
		const float I = FMath::Lerp(T.ReleaseMinIntensity, 1.f, K);
		const float D = T.ReleaseDuration * (0.7f + 0.6f * K);
		const bool bPlayed = Play(World, I, D);
		FallbackShake(FMath::Lerp(T.ReleaseMinIntensity, 1.f, K) * T.FallbackReleaseShake);
		if (!bPlayed && !VibratorUnsupported()) return;
		++GCount[1];
		GLastHit = Now(World) - T.HitMinInterval * 0.5; // a contact right at launch still gets through shortly after
		Log(World, true, FString::Printf(TEXT("release power=%.2f intensity=%.2f dur=%.3f"), K, I, D));
	}

	void AimTick(float Pull, float Dt, UWorld* World)
	{
		const FTune& T = Tune();
		const float P = FMath::Clamp(Pull, 0.f, 1.f);
		if (P < T.AimMinPull) { if (GAimPull > 0.f) GAimPull = 0.f; return; }
		GAimTickAt = FPlatformTime::Seconds();
		GAimPull = P; // HUD jitter follows even with the vibrator switched off (FRAME SHAKE governs it)
		if (!Enabled() || !World) return;
		const double Tn = Now(World);
		if (Tn < GNextAimPulse) return;
		// pulse rate and strength rise with the pull (ease-in so the top end feels tense)
		const float E = P * P;
		const float Interval = FMath::Lerp(T.AimSlowInterval, T.AimFastInterval, E);
		const float I = FMath::Lerp(T.AimMinIntensity, T.AimMaxIntensity, E);
		const float D = T.AimPulseDuration * (1.f + 0.6f * P);
		GNextAimPulse = Tn + Interval;
		const bool bPlayed = Play(World, I, D);
		FallbackShake(I * T.FallbackAimPulse); // vibratorless: short frame kicks on the same cadence as aim pulses
		if (!bPlayed && !VibratorUnsupported()) return;
		++GCount[2];
		Log(World, false, FString::Printf(TEXT("aim pull=%.2f intensity=%.2f interval=%.3f"), P, I, Interval));
	}
}

// b66 (Evan): "on the loser clash anime enable hard 5 sec rumble of iPhone vibration to simulate smashing against a wall" - also on
// the skill reaction moment. iOS: Core Haptics continuous buzz + a hard transient bang every 0.18 s (falls back to the system vibrate
// when the phone has no haptic engine). Elsewhere: one long PlayDynamicForceFeedback (Android vibrator / gamepad motors).
#if PLATFORM_IOS
static id GZoogiHapticEngine = nil; // CHHapticEngine, retained, main thread only
#endif
void ZoogiHaptics::Rumble(float Seconds, float Intensity, UWorld* World, const TCHAR* What)
{
	if (!Enabled()) return;
	Seconds = FMath::Clamp(Seconds, 0.1f, 25.f);
	Intensity = FMath::Clamp(Intensity, 0.f, 1.f);
	UE_LOG(LogZoogi, Log, TEXT("HAPTIC rumble %s %.1fs intensity %.2f"), What, Seconds, Intensity);
#if PLATFORM_IOS
	// CoreHaptics is loaded at run time (the Mac build keeps the cached UBT rules, so a new PublicFrameworks entry never links):
	// classes by name, constants by their string values. Falls back to the system vibrate when the framework / engine is missing.
	const float Sec = Seconds, Inten = Intensity;
	dispatch_async(dispatch_get_main_queue(), ^{
		static void* GCH = dlopen("/System/Library/Frameworks/CoreHaptics.framework/CoreHaptics", RTLD_NOW);
		Class CEng = GCH ? NSClassFromString(@"CHHapticEngine") : nil;
		Class CPar = NSClassFromString(@"CHHapticEventParameter"), CEvt = NSClassFromString(@"CHHapticEvent"), CPat = NSClassFromString(@"CHHapticPattern");
		if (!CEng || !CPar || !CEvt || !CPat) { AudioServicesPlaySystemSound(kSystemSoundID_Vibrate); return; }
		id Caps = [CEng performSelector:@selector(capabilitiesForHardware)];
		if (!Caps || ![Caps supportsHaptics]) { AudioServicesPlaySystemSound(kSystemSoundID_Vibrate); return; }
		NSError* Err = nil;
		if (!GZoogiHapticEngine)
		{
			id E = [[CEng alloc] initAndReturnError:&Err];
			if (!E) return;
			[E setPlaysHapticsOnly:YES];
			GZoogiHapticEngine = E; // keep the +1 retain
		}
		id Eng = GZoogiHapticEngine;
		if (![Eng startAndReturnError:&Err]) return;
		NSMutableArray* Events = [NSMutableArray array];
		id ParI = [[[CPar alloc] initWithParameterID:@"HapticIntensity" value:Inten] autorelease];
		id ParS = [[[CPar alloc] initWithParameterID:@"HapticSharpness" value:0.35f] autorelease];
		[Events addObject:[[[CEvt alloc] initWithEventType:@"HapticContinuous" parameters:@[ParI, ParS] relativeTime:0 duration:Sec] autorelease]];
		id BI = [[[CPar alloc] initWithParameterID:@"HapticIntensity" value:1.f] autorelease];
		id BS = [[[CPar alloc] initWithParameterID:@"HapticSharpness" value:1.f] autorelease];
		for (float T = 0.f; T < Sec; T += 0.18f)
			[Events addObject:[[[CEvt alloc] initWithEventType:@"HapticTransient" parameters:@[BI, BS] relativeTime:T] autorelease]];
		id Pat = [[[CPat alloc] initWithEvents:Events parameters:@[] error:&Err] autorelease];
		if (!Pat) return;
		id Pl = [Eng createPlayerWithPattern:Pat error:&Err];
		if (Pl) [Pl startAtTime:0 error:&Err];
	});
#else
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->IsLocalController()) PC->PlayDynamicForceFeedback(Intensity, Seconds, true, true, true, true);
	FallbackShake(Intensity);
#endif
}
