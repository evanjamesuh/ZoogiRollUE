// aim-arrow (Evan, Oct 17 1:29-1:31 PM CT): a visible AIM ARROW on the human shooter's Zoogi in normal shots.
// Grab the arrow (press on / near it, generous area round the arrowhead) and drag to rotate the shot direction round the
// Zoogi; a press anywhere else is the unchanged drag-slingshot (power / release / feel untouched). The arrow is a flat
// world-space shape lying on the arena floor (aspect independent: landscape HUD-on and portrait HUD-hidden), drawn on the
// HUD canvas from projected world points, so it reads over geometry; the shooter's own ball hides the shaft's base.
// Hidden in Pinpoint / target-choosing modes (classic Pinpoint lock-on power armed, AIM ZOOM crosshair tap-to-lock).
// Every size / colour is a tunable in [ZoogiRoll.AimArrow] (DefaultGame.ini or -ini overrides); -ZoogiNoAimArrow turns it off.
#pragma once

#include "CoreMinimal.h"

class UCanvas;
class AZoogiGameMode;
class AZoogiBall;
class APlayerController;
class AZoogiPlayerController;

namespace ZoogiAimArrow
{
	struct ZOOGIROLL_API FTune
	{
		bool bEnabled = true;
		// sizes as multiples of the ball diameter (Evan's Imagine Cup reference, frame-checked Oct 17)
		float ShaftWidthD = 0.30f;   // shaft thickness
		float HeadWidthD = 0.70f;    // arrowhead base width
		float LengthD = 1.75f;       // total length from the ball's front base to the tip (constant: never scales with power)
		float HeadLengthD = 0.55f;   // arrowhead length (part of LengthD)
		float LiftCm = 2.f;          // hover above the floor
		// look: flat solid yellow, crisp slightly darker greenish stroke round the whole outline, no dots / dashes
		FLinearColor Fill = FLinearColor(0.96f, 0.97f, 0.16f, 1.f);
		FLinearColor Stroke = FLinearColor(0.42f, 0.58f, 0.06f, 1.f);
		float StrokePx = 2.5f;       // at 1080 px short side
		bool bGlow = false;          // soft semi-transparent halo (Evan: no glow; kept as one switch)
		float GlowPx = 10.f, GlowAlpha = 0.28f;
		// grab area (screen px at 1080 px short side)
		float GrabShaftPx = 40.f;    // margin round the shaft
		float GrabHeadPx = 70.f;     // margin round the arrowhead (players grab near the head)
		bool bGrabOnZoogi = false;   // a press on the shooter's own ball also grabs (off: the ball keeps the old press)
		float RotateStartPx = 6.f;   // finger travel before the grab rotates (below = a tap: the old tap-to-aim on release)
		float RotateDegPerPx = 0.25f; // rotation per px of HORIZONTAL finger travel (1080 px short side): right = clockwise seen from
		                              // above. Screen-space on purpose: the camera orbits with the aim, so a world point under the
		                              // finger would feed back (Oct 17 fix: overshoot / drift after release)
		float DefaultAimDistCm = 800.f; // free-aim distance the arrow uses when nothing was aimed yet
		// the Zoogi turns in place to face the arrow (visual mesh yaw only, physics untouched)
		bool bFaceArrow = true;
		float FaceRate = 40.f;       // slerp rate 1/s (40 = visually locked to the arrow)
	};
	ZOOGIROLL_API const FTune& Tune();
	/** Test hook: replaces the cached tune (automation only). */
	ZOOGIROLL_API void SetTuneForTest(const FTune& T);

	/** World outline, flat on the floor: 0 shaft base L, 1 shaft end L, 2 head L, 3 tip, 4 head R, 5 shaft end R, 6 shaft base R.
	 *  The shaft starts at the ball's front base (centre + dir * R) and the shape pivots round the ball centre. */
	ZOOGIROLL_API void Outline(const FVector& BallCentre, float Radius, float YawDeg, const FTune& T, FVector Out[7]);

	struct FVisInput
	{
		bool bEnabled = true;
		bool bHumanAimStep = false; // human shooter, Battle Aim step
		bool bBallInPlay = false;
		bool bZoomMode = false;     // AIM ZOOM: crosshair + tap-to-lock
		bool bPinpointArmed = false; // classic Pinpoint lock-on power armed (crosshair / LOCKED reticle)
	};
	ZOOGIROLL_API bool ShouldShow(const FVisInput& In);
	ZOOGIROLL_API FVisInput GatherVis(const AZoogiGameMode* G, const AZoogiPlayerController* PC);
	ZOOGIROLL_API bool IsShown(const AZoogiGameMode* G, const AZoogiPlayerController* PC);

	/** Direction the arrow shows (deg): the real shot direction when an aim exists (lock / free aim incl. cut), else toward
	 *  the soft-lock candidate, else toward the arena centre. False without a shooter. */
	ZOOGIROLL_API bool CurrentYaw(const AZoogiGameMode* G, float& OutYaw);
	/** Facing hook for the expression component: true + the yaw while the arrow shows on this ball and bFaceArrow is on. */
	ZOOGIROLL_API bool FaceYaw(const AZoogiGameMode* G, const AZoogiBall* B, float& OutYaw);

	/** Screen-space grab test. S = projected outline (7 points), BallS / BallRpx = the ball's screen disc, Px = 1080-short-side scale. */
	ZOOGIROLL_API bool HitTest(const FVector2D S[7], const FVector2D& BallS, float BallRpx, const FVector2D& P, float Px, const FTune& T);

	/** Unchanged slingshot pull (the exact formula the controller always used): back px along -Fwd over the full-pull px. */
	ZOOGIROLL_API float SlingshotPull(float BackPx, float UPx, float MaxDragDistancePx, float MaxDragForce);

	/** Grab-and-rotate gesture: yaw = yaw at the grab + horizontal finger travel since the press x DegPerPx. Purely
	 *  screen-space (no world projection), so camera motion during or after the drag can never change the aim. */
	struct ZOOGIROLL_API FGesture
	{
		bool bActive = false, bRotating = false;
		float Yaw0 = 0.f, Yaw = 0.f, AimDist = 800.f;
		FVector2D Press = FVector2D::ZeroVector;
		void Begin(float CurYaw, const FVector2D& PressScreen, float Dist);
		/** Updates Yaw from the finger's screen position (DegPerPx in degrees per actual px); true once rotating (moved past StartPx). */
		bool Move(const FVector2D& Screen, float StartPx, float DegPerPx);
		/** Ends the gesture and clears all state; true = it was a tap (never rotated). */
		bool End();
		void Cancel() { bActive = bRotating = false; Press = FVector2D::ZeroVector; }
	};

	/** Draws the arrow for the human shooter (no-op when hidden). Called from AZoogiHUD::DrawHUD before any early-out so it
	 *  also shows with the HUD hidden. */
	ZOOGIROLL_API void Draw(UCanvas* Canvas, AZoogiGameMode* G, APlayerController* PC);
}
