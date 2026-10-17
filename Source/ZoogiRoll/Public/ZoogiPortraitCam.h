// Portrait (vertical phone, 9:16 .. 9:20) camera framing helpers (Oct 17). The match camera's FOVs were tuned as horizontal
// FOVs on a 16:9 screen; on a narrow screen the engine keeps the horizontal FOV (AspectRatio_MaintainXFOV), so the vertical
// FOV balloons (80 deg h -> ~118 deg v at 9:18). Here: cap the vertical FOV, pull the boom back so the same width still fits
// at the pivot, keep the play focus inside the safe middle band (portrait is HUD-hidden play: top 5% controls, bottom 10% thumb zone; cvars), and fit whole-court
// views (court / Frozen Ring 2) inside that band. Everything is a no-op on landscape screens.
#pragma once

#include "CoreMinimal.h"

class APlayerController;

namespace ZoogiPortrait
{
	/** zoogi.Cam.Portrait (1): portrait adaptation on narrow screens. */
	ZOOGIROLL_API bool Enabled();
	/** zoogi.Cam.SafeTop (0.05, HUD-hidden play; 0.12 with a full HUD): top screen fraction kept clear of the play focus. */
	ZOOGIROLL_API float SafeTop();
	/** zoogi.Cam.SafeBottom (0.10, HUD-hidden thumb zone; 0.22 with the card hand up): bottom screen fraction kept clear. */
	ZOOGIROLL_API float SafeBottom();
	/** zoogi.Cam.PortraitVFovMax (78): vertical FOV cap on portrait screens. */
	ZOOGIROLL_API float VFovMax();
	/** zoogi.Cam.PortraitBoomMax (2.6): cap on the boom pull-back that keeps the landscape width. */
	ZOOGIROLL_API float BoomMax();
	/** zoogi.Cam.CourtMargin (0.94): whole-court views keep the court inside this fraction of the half-width. */
	ZOOGIROLL_API float CourtMargin();

	/** zoogi.Cam.SafeTopMin (0.05): the top band never shrinks below this (HUD-hidden controls: show-HUD, turn / score pill, ZOOM). */
	ZOOGIROLL_API float SafeTopMin();

	/** Portrait camera mode hook (AZoogiPlayerController::SetPortraitCameraMode): -1 auto (aspect < 1), 0 forced off, 1 forced on. */
	ZOOGIROLL_API void SetModeOverride(int32 Mode);
	ZOOGIROLL_API int32 GetModeOverride();
	/** Portrait framing for a view of this aspect: forced on / off by the hook, otherwise aspect < 1 with zoogi.Cam.Portrait on. */
	ZOOGIROLL_API bool IsPortrait(float Aspect);
	/** The view's aspect (framed HUD: the arena window; otherwise the viewport). */
	ZOOGIROLL_API float ViewAspect(const APlayerController* PC);
	/** Safe band fractions for this PC at Aspect: 0 / 0 on landscape, and while the framed HUD already keeps the HUD out of the
	 *  arena window. */
	ZOOGIROLL_API void Band(const APlayerController* PC, float Aspect, float& OutTop, float& OutBottom);

	/** Landscape-tuned horizontal FOV -> horizontal FOV to use at Aspect (vertical capped at VFovMax), and the boom multiplier
	 *  that keeps the landscape width at the pivot (1 when nothing changes). */
	ZOOGIROLL_API void AdaptFov(float LandscapeHFov, float Aspect, float& OutHFov, float& OutBoomMul);
	/** Degrees to pitch the camera down so the look point projects at the safe band's centre (0 when the band is symmetric). */
	ZOOGIROLL_API float BandPitch(float HFov, float Aspect, float Top, float Bottom);
	/** Projects P for a camera pose (MaintainXFOV): normalised x (right) / y (up) in -1..1 across the view. False behind. */
	ZOOGIROLL_API bool Project(const FVector& CamLoc, const FRotator& Rot, float HFov, float Aspect, const FVector& P, FVector2D& Out);
	/** Shortest boom (>= MinDist, <= MaxDist) at which a horizontal circle (centre C, radius R) fits |x| <= CourtMargin and the
	 *  safe band in y, for a boom at Yaw / Pitch looking at C with the band pitch applied. Returns MaxDist if nothing fits. */
	ZOOGIROLL_API float FitCircleDist(const FVector& C, float R, float Yaw, float Pitch, float HFov, float Aspect, float Top, float Bottom, float MinDist, float MaxDist);
	/** Shortest boom in [MinDist, MaxDist] (looking at Pivot along Yaw / Pitch, band pitch applied) at which every point fits
	 *  |x| <= CourtMargin and the safe band shrunk by YMargin. Returns MaxDist if nothing fits. */
	ZOOGIROLL_API float FitPointsDist(TArrayView<const FVector> Pts, const FVector& Pivot, float Yaw, float Pitch, float HFov, float Aspect, float Top, float Bottom, float YMargin, float MinDist, float MaxDist);

	/** Portrait aim / rest framing (Evan's tablet build): from behind the shooter on its pad, looking up the arena along the
	 *  screen's long axis. zoogi.Cam.PortraitAim (1) on/off, .PortraitAimPitch (48), .PortraitAimAhead (0.5: look point from the
	 *  shooter toward the aim point / arena centre), .PortraitRestAhead (0.4, Draw / Awaken), .PortraitAimWidth (1.4: ground width
	 *  at the look point, x the play radius), .PortraitShooterMargin (0.06: shooter this far inside the band's bottom edge). */
	ZOOGIROLL_API bool AimFraming();
	ZOOGIROLL_API float AimPitch(); // zoogi.Cam.PortraitRestPitch (48): Draw / Awaken / PowerUp boom pitch
	/** Portrait AIM chase view (Evan's tablet reference): low, just behind and above the Zoogi, the arena running away up the
	 *  screen, aim line straight up. zoogi.Cam.PortraitAimPitch (27: look pitch below horizontal), .PortraitAimBoom (800 cm x
	 *  arena camera scale: camera to shooter), .PortraitAimScreenY (0.68: shooter's screen height from the top). */
	ZOOGIROLL_API float ChasePitch();
	ZOOGIROLL_API float ChaseBoom();
	ZOOGIROLL_API float ChaseScreenY();
	ZOOGIROLL_API float AimAhead();
	ZOOGIROLL_API float RestAhead();
	ZOOGIROLL_API float AimWidth();
	ZOOGIROLL_API float ShooterMargin();
	/** zoogi.Cam.PortraitProbeHeight (700): the aim / rest boom's collision probe starts this high over the look point. */
	ZOOGIROLL_API float AimProbeHeight();

	/** Camera rotation for a boom looking at Look from Loc, with the band pitch applied. */
	ZOOGIROLL_API FRotator BandRotation(const FVector& Loc, const FVector& Look, float HFov, float Aspect, float Top, float Bottom);
	/** Boom location (same convention as the match camera: Pitch above horizontal, looking along Yaw). */
	ZOOGIROLL_API FVector Boom(const FVector& Pivot, float Yaw, float Pitch, float Dist);
}
