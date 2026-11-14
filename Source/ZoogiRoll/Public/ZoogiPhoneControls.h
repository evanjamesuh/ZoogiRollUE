// ZoogiPhoneControls — in-match phone touch UI LOOK + layout + hit targets + callbacks.
// Visual MUST match /workspace/zoogi-phone-controls/approved-omni-sticks.png (Evan Oct 22 revised):
//   LEFT stick (aim): blue outer ring + dark glossy center + CENTER CROSSHAIR + 4 light-blue D-pad triangles.
//   RIGHT stick (power/slingshot): same blue bubble, center crosshair, NO D-pad. Pull to charge, release to fire.
//   Ability arc (TOP-LEFT of right stick): 4 smaller blue bubbles — shield, target/rings, crystalline Z/bolt, 8-point star.
//   AWAKEN (BOTTOM-RIGHT of right stick): orange/gold sun with fiery ring. UI placement/look only.
//
// Mobile & Online Play owns low-level tablet/phone input wiring. This module owns look/layout/hit-targets and exposes:
//   OnAimAxis(FVector2D)          — left stick normalized axis (aim only; never mid-roll steer)
//   OnPowerPull01(float)          — right stick pull 0..1 while held
//   OnFireRelease(float, FVector2D) — release pull + optional flick delta for spin
//   OnCancelAbort()               — drag past cancel ring / abort
//   OnAbilityIndex(int32 0..3)    — shared ability tap
//   OnAwakenPressed()             — Awaken bubble tap
// Standby (not your turn): sticks must NOT move the marble. Left scrub look-around / right peek hand+emotes are stubs
// until Mobile wires them (bStandbyLook / bStandbyPeekHand flags + OnStandbyLookAxis / OnStandbyPeek).
// Tune sizes/positions in Config/DefaultGame.ini [ZoogiRoll.PhoneControls] without rebuild fights.
#pragma once

#include "CoreMinimal.h"

class UCanvas;
class AZoogiGameMode;
class AHUD;

namespace ZoogiPhoneControls
{
	/** Shared ability slot icons (fixed order, every Zoogi): 0 shield, 1 target/rings, 2 crystalline Z/bolt, 3 8-point star. */
	enum class EAbilityIcon : uint8 { Shield = 0, Target = 1, CrystalZ = 2, Star = 3 };

	struct ZOOGIROLL_API FTune
	{
		bool bEnabled = true;
		// Overlay mode (Evan 2026-10-23): tablet already has aim/power — ADD power orbs, do NOT replace with dual sticks.
		// "Power orbs" = the 4 shared ability bubbles + Awaken sun from approved-omni-sticks.png (NOT a separate energy/charge set).
		bool bDrawOmniSticks = false;   // default OFF — keep existing tablet aim/power widgets
		bool bDrawAbilityOrbs = false;  // Oct 24 Evan: removed (double tap fires the power) -   // default ON — shield / target / Z-bolt / star
		bool bDrawAwakenOrb = false;    // Oct 24 Evan: removed -     // default ON — orange/gold fiery sun
		// When Awaken orb is drawn: hide Arena HUD large gold AWAKEN button (one path). Charge pips stay.
		bool bHideArenaAwakenWithOrbs = true;
		// Optional screen-% pivot for the orb arc (0..1 from top-left). When false, RightStickCX/CY (invisible if sticks off) is the anchor.
		bool bUseOrbScreenAnchor = false;
		float OrbAnchorXPct = 0.89f;    // ~right thumb zone at 1080p landscape
		float OrbAnchorYPct = 0.81f;
		// Left stick (1080p units from bottom-left / bottom-right anchors)
		float LeftStickCX = 170.f;   // from left
		float LeftStickCY = 200.f;   // from bottom
		float LeftStickR = 118.f;    // outer ring radius
		float LeftKnobR = 46.f;      // glossy center radius
		// Right stick
		float RightStickCX = 210.f;  // from right
		float RightStickCY = 210.f;  // from bottom
		float RightStickR = 118.f;
		float RightKnobR = 46.f;
		// Ability arc (polar around right stick center; angles deg, 0 = +X / right, CCW; top-left arc ~ 100..200)
		float AbilityArcR = 168.f;   // distance from right stick center to ability bubble centers
		float AbilityBubbleR = 36.f;
		float AbilityAngle0 = 155.f; // top (shield)
		float AbilityAngle1 = 130.f;
		float AbilityAngle2 = 105.f;
		float AbilityAngle3 = 80.f;  // bottom of arc (star) — still top-left quadrant-ish of stick
		// Awaken bubble
		float AwakenAngle = -40.f;   // bottom-right of right stick
		float AwakenR = 168.f;
		float AwakenBubbleR = 34.f;
		// Cancel ring around right stick (visual radius; Mobile uses for abort-past-ring)
		float CancelRingR = 195.f;
		float CancelRingThick = 3.5f;
		// Hit-target padding (extra px beyond drawn radius for fat fingers)
		float HitPad = 10.f;
		// Opacity / glow
		float Alpha = 0.92f;
		float GlowMul = 1.f;
	};

	/** Live per-frame state (visual only; Mobile drives axes via Set/Notify). */
	struct ZOOGIROLL_API FState
	{
		bool bYourTurn = true;
		bool bStandbyLook = false;     // stub: left stick scrub look-around while !bYourTurn
		bool bStandbyPeekHand = false; // stub: right stick peek hand / emotes while !bYourTurn
		FVector2D AimAxis = FVector2D::ZeroVector;     // -1..1
		float PowerPull01 = 0.f;                       // 0..1
		FVector2D PowerKnobOff = FVector2D::ZeroVector;// visual knob offset in px
		FVector2D AimKnobOff = FVector2D::ZeroVector;
		bool bCancelHot = false;       // finger past cancel ring
		bool bAwakenReady = true;
		bool AbilityEnabled[4] = { true, true, true, true };
		int32 HoverAbility = -1;
		bool bHoverAwaken = false;
		float PulseT = 0.f;
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAimAxis, FVector2D /*Axis*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPowerPull01, float /*Pull01*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFireRelease, float /*Pull01*/, FVector2D /*FlickDelta*/);
	DECLARE_MULTICAST_DELEGATE(FOnCancelAbort);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbilityIndex, int32 /*Index0to3*/);
	DECLARE_MULTICAST_DELEGATE(FOnAwakenPressed);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStandbyLookAxis, FVector2D /*Axis*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStandbyPeek, FVector2D /*Axis*/);

	ZOOGIROLL_API FOnAimAxis& AimAxisDelegate();
	ZOOGIROLL_API FOnPowerPull01& PowerPullDelegate();
	ZOOGIROLL_API FOnFireRelease& FireReleaseDelegate();
	ZOOGIROLL_API FOnCancelAbort& CancelAbortDelegate();
	ZOOGIROLL_API FOnAbilityIndex& AbilityIndexDelegate();
	ZOOGIROLL_API FOnAwakenPressed& AwakenPressedDelegate();
	ZOOGIROLL_API FOnStandbyLookAxis& StandbyLookAxisDelegate();
	ZOOGIROLL_API FOnStandbyPeek& StandbyPeekDelegate();

	ZOOGIROLL_API const FTune& Tune();
	ZOOGIROLL_API FState& State();
	ZOOGIROLL_API void ReloadTune();
	ZOOGIROLL_API bool IsEnabled();
	/** True when Phone Awaken orb is the active Awaken path — Arena large gold AWAKEN button should hide (pips stay). */
	ZOOGIROLL_API bool ShouldHideArenaAwakenButton();

	/** Screen-space centers / radii (viewport px) for the current canvas size. */
	struct ZOOGIROLL_API FLayout
	{
		FVector2D LeftC, RightC;
		float LeftR = 0.f, RightR = 0.f, LeftKnobR = 0.f, RightKnobR = 0.f;
		FVector2D AbilityC[4];
		float AbilityR = 0.f;
		FVector2D AwakenC;
		float AwakenR = 0.f;
		float CancelR = 0.f;
		float U = 1.f;
	};
	ZOOGIROLL_API FLayout ComputeLayout(float CanvasW, float CanvasH);

	/** Draw phone overlay: ability/Awaken orbs by default; omni sticks only if bDrawOmniSticks / -ZoogiPhoneControlsSticks. Physics untouched. */
	ZOOGIROLL_API void Draw(UCanvas* Canvas, float U, float TimeSeconds, AZoogiGameMode* G);

	/** Hit-test viewport pos. When sticks are hidden, only ability/Awaken orb rects claim touches (tablet aim/power widgets keep the rest). → action name ("PhoneAim","PhonePower","PhoneAbility","PhoneAwaken","PhoneCancel") + index. */
	ZOOGIROLL_API bool HitTest(float CanvasW, float CanvasH, const FVector2D& Pos, FName& OutAction, int32& OutIndex);

	/** Notify helpers Mobile can call after wiring touch → these (also fire multicast delegates). */
	ZOOGIROLL_API void NotifyAimAxis(const FVector2D& Axis);
	ZOOGIROLL_API void NotifyPowerPull01(float Pull01);
	ZOOGIROLL_API void NotifyFireRelease(float Pull01, const FVector2D& FlickDelta);
	ZOOGIROLL_API void NotifyCancelAbort();
	ZOOGIROLL_API void NotifyAbilityIndex(int32 Index0to3);
	ZOOGIROLL_API void NotifyAwakenPressed();

	/** Battle Royale HUD (ZoogiRoyaleHUD.cpp): the approved stick / Awaken sun look drawn anywhere (viewport px). */
	ZOOGIROLL_API void DrawStickAt(UCanvas* Canvas, const FVector2D& Center, float RingR, float KnobR, const FVector2D& KnobOff, bool bDPad, float Alpha, float TimeSeconds, float U);
	ZOOGIROLL_API void DrawAwakenAt(UCanvas* Canvas, const FVector2D& Center, float R, float Alpha, float TimeSeconds);
	ZOOGIROLL_API void DrawRingAt(UCanvas* Canvas, const FVector2D& Center, float R, float Thick, const FLinearColor& Color);
}
