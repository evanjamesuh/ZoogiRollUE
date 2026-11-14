// Oct 22 (Evan): device vibration (Android phone / tablet vibrator, gamepad motors elsewhere) + aim pull-back rumble.
// When the device has no vibrator (Android feature check / forced), HUD frame shake + aim jitter stand in — intensities unchanged.
#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Haptics for Zoogi Roll. All calls go through APlayerController::PlayDynamicForceFeedback, which UE maps to the
 *  device vibrator on Android (FAndroidInputInterface -> AndroidThunkCpp_Vibrate; android.permission.VIBRATE is in
 *  UE's generated manifest) and to gamepad motors on desktop. [ZoogiRoll.Haptics] in DefaultGame.ini tunes it;
 *  Settings > HAPTICS (menu key Haptics) switches it off. -ZoogiNoHaptics disables for a run, -ZoogiHapticsLog logs
 *  every call (otherwise a throttled summary). Visual / haptic only: never touches physics or RNG.
 *
 *  Vibratorless fallback: detect once (Android android.hardware.vibrator, or -ZoogiVibratorUnsupported / ini force).
 *  When unsupported, reuse ZoogiFramed::FrameShakeVisual + stronger aim HUD jitter (FRAME SHAKE). Disable with
 *  bVibratorFallback=False / -ZoogiNoVibratorFallback. Does not retune haptic intensities. */
namespace ZoogiHaptics
{
	/** Any collision. Impulse = |NormalImpulse|; strength ramps HitMinImpulse..HitFullImpulse. Throttled (HitMinInterval). */
	ZOOGIROLL_API void Hit(float Impulse, UWorld* World, const TCHAR* What = TEXT("hit"));
	/** Direct kick, Strength 0..1 (shares the hit throttle). */
	ZOOGIROLL_API void Kick(float Strength, UWorld* World, const TCHAR* What);
	/** Launch release pulse, Power 0..1 (launch speed / full-pull speed). Stops the aim rumble. */
	ZOOGIROLL_API void Release(float Power, UWorld* World);
	/** While the slingshot is pulled back: Pull 0..1. Pulsed vibration, rate + intensity rise with the pull. */
	ZOOGIROLL_API void AimTick(float Pull, float Dt, UWorld* World);
	/** Drag ended / cancelled (no pulse). */
	ZOOGIROLL_API void AimStop(UWorld* World);
	/** Current aim pull 0..1 while the rumble runs (0 otherwise), for the HUD jitter. */
	ZOOGIROLL_API float AimPull();
	/** HUD aim-jitter multiplier (1 normally; >1 when vibrator unsupported and fallback on). */
	ZOOGIROLL_API float AimHudMul();
	/** True when the device vibrator is missing/unsupported and the HUD/frame-shake fallback is enabled. */
	ZOOGIROLL_API bool VibratorUnsupported();
	/** Re-run the one-shot (or forced) vibrator support check. */
	ZOOGIROLL_API void RefreshVibratorSupport();
	ZOOGIROLL_API bool Enabled();
	/** b66: long hard rumble (loser clash anime, skill reaction): iOS Core Haptics buzz + bangs; elsewhere force feedback. */
	ZOOGIROLL_API void Rumble(float Seconds, float Intensity, UWorld* World, const TCHAR* What);
	ZOOGIROLL_API int32 Count(int32 Kind); // 0 hits, 1 releases, 2 aim pulses
}
