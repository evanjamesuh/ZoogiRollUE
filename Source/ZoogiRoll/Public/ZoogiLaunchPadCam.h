#pragma once

#include "CoreMinimal.h"

class AActor;
class AZoogiBall;
class UWorld;

/**
 * Launch pad camera (Evan Oct 22, pad-as-gameplay): by default (zoogi.Cam.PadView 0) there is no special SolvePose framing -
 * while IsOnLaunchPad the controller uses normal AIM / pad-clear-aim (behind Zoogi), not 7398f61 SolvePose. AimYaw drives aim.
 * The aim-zoom view-centre knockoff-radius clamp is skipped while on a launch pad (811d899).
 * Query() reports pad occupancy even when PadView=0 so UpdatePadView can track PadPhase pad->flight->land (behind through
 * the aerial launch arc only; after land PadPhase clears and main follow is top-down). SolvePose stays Enabled()-gated.
 *
 * zoogi.Cam.PadView 1 restores the legacy special pad path (aim-from-pad / SolvePose). Prefer 0.
 *
 * Real hooks (arena/launch-pads + launch flow): AZoogiBall::IsOnLaunchPad() AND AZoogiGameMode::IsOnLaunchPad(ball);
 * pad = AZoogiArena::GetLaunchPad(). Fallback soft interface by name when those are absent.
 * Cvars: zoogi.Cam.PadView 0 (default), PadRadius 160, PadBlend 0.5 s, PadMaxFlight 3 s; PadBoom/PadPitch/PadScreenY/PadPortrait (legacy).
 */
namespace ZoogiLaunchPadCam
{
	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API float Radius();
	ZOOGIROLL_API float BlendTime();
	ZOOGIROLL_API float MaxFlight();
	/** Pad framing overrides (legacy PadView 1): Def when the cvar is 0. */
	ZOOGIROLL_API float Boom(float Def);
	ZOOGIROLL_API float Pitch(float Def);
	ZOOGIROLL_API float ScreenY(float Def);
	ZOOGIROLL_API bool PortraitOn();
	/** Legacy special pad pose screen-y (PadView 1 only). Soft-off when PadView 0. */
	ZOOGIROLL_API float TargetScreenY(bool bPortrait);
	/**
	 * Legacy special pad pose (PadView 1 / 7398f61). Soft-disabled: the controller no longer applies this while PadView is 0;
	 * the normal gameplay camera is used on the pad instead. Kept for optional re-enable / ShotTest.
	 */
	ZOOGIROLL_API void SolvePose(UWorld* World, const AActor* Shooter, const FVector& ZoogiLoc, float Yaw, float TanV, float TargetY, float ExtraPitch,
		FVector& OutLoc, FRotator& OutRot, float& OutPitch, float& OutBoom);
	ZOOGIROLL_API void SetRealHooks(bool bOn);
	ZOOGIROLL_API void SetTestPad(bool bOn, const FVector& Loc = FVector::ZeroVector, const FVector& Into = FVector::ZeroVector);
	ZOOGIROLL_API bool Query(const UObject* WorldCtx, const AActor* GameMode, const AZoogiBall* Shooter, FVector& OutPadLoc, FVector& OutInto, FString& OutSource);
}