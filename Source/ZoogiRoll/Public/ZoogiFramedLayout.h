// framed (Oct 14, Evan spec v2.0 "Complete Game Interface & Arena Viewport Redesign"): the framed in-match layout.
// The live 3D arena renders into a centred rectangle (the local player's real view rect, splitscreen-style
// ULocalPlayer::Origin/Size set by UZoogiViewportClient), with cartoon UI panels in the margins (UZoogiFramedHUDWidget).
// OFF by default: -ZoogiFramedHUD turns it on, -ZoogiNoFramedHUD forces it off, [ZoogiRoll.UI] bFramedHUD=True in
// DefaultGame.ini flips the default. Visual / view-rect only: never changes rules, physics or turn order.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class APlayerController;
class AZoogiGameMode;

namespace ZoogiFramed
{
	/** Pixel-space layout of one frame (viewport pixels, origin top-left). */
	struct FLayout
	{
		FVector2D Viewport = FVector2D(1920, 1080);
		float U = 1.f;            // pixels per 1080p unit
		bool bCompact = false;    // narrow / ultra-wide (19.5:9) / small: compact columns + collapsible tray
		float TrayOpen = 1.f;     // 0 = collapsed (thin system-text strip), 1 = full tray (card hand / Awaken / battle controls)
		FMargin Safe;             // safe-area insets (px)
		FBox2D Top, Left, Right, Arena, Tray, Prompt;
		// hud-v3: the layout is the 1376x768 target scaled by T (screen px per target px), arena 16:9..20:9 in between
		float T = 1.f;            // px per target px (= U * 1080/768 unless the width is the limit)
		float ColT = 220.f;       // column width in target px
		FBox2D Hand;              // card tray recess interior (fan area)
		FVector2D Deck = FVector2D::ZeroVector, Discard = FVector2D::ZeroVector, Energy = FVector2D::ZeroVector; // pile / orb centres
		float TrayLeftX1 = 0.f, TrayRightX0 = 0.f; // end of the deck slab, start of the discard / energy slab
		// tray art anchors (both orientations): top-left of the tray_left (deck) and tray_right (discard / orb) pieces and their
		// px per target px; every tray readout / control is placed relative to these (landscape: (X0 | TrayRightX0, TrayTop + 12 T), T)
		FVector2D TrayL = FVector2D::ZeroVector, TrayR = FVector2D::ZeroVector;
		float TrayT = 1.f;
		// portrait (Evan, Oct 17 12:10 PM): HUD HIDDEN = the game is played vertically. The arena fills the whole (tall) screen,
		// a few small controls sit along the top edge (HUD toggle, turn / score indicator, ZOOM) and the tray (hand recess over
		// a deck | discard + orb row) is overlaid at the bottom only while the human's Awaken / Standby is open.
		bool bPortrait = false;
		float P = 1.f;            // portrait: px per design px (design = 1080 wide)
		FBox2D HudBtn = FBox2D(ForceInit), ZoomBtn = FBox2D(ForceInit), Indicator = FBox2D(ForceInit), BottomRow = FBox2D(ForceInit);
		bool bValid = false;
	};

	/** hud-v3 tray readouts published by the card hand each frame (the framed painter draws them on the tray art). */
	struct FTrayInfo
	{
		bool bCards = false;
		int32 Draw = 0, Discard = 0, Energy = 0, EnergyMax = 0;
		float EnergyFrac = 0.f, Flash = 0.f, Shake = 0.f;
		double Stamp = 0.0;
	};
	ZOOGIROLL_API void SetTrayInfo(const FTrayInfo& I);
	ZOOGIROLL_API const FTrayInfo& TrayInfo();

	/** Oct 14 (Evan): HUD frame shake + controller / phone rumble on hard impacts. [ZoogiRoll.FrameShake] bEnabled, ImpulseThreshold,
	 *  ImpulseFull, MaxOffset (1080p px), Duration, bRumble, RumbleScale; Settings > GRAPHICS > FRAME SHAKE (menu key FrameShake)
	 *  switches the shake, Settings > AUDIO > HAPTICS the rumble. Visual / haptic only: never touches physics or RNG. */
	ZOOGIROLL_API void ImpactShake(float Impulse, UWorld* World);
	/** Direct kick, Strength 0..1 (scaled like an impact at that fraction of the threshold..full range). */
	ZOOGIROLL_API void FrameShake(float Strength, UWorld* World);
	/** Advances the shake by Dt and returns the frame offset in 1080p units (zero when idle or switched off). */
	ZOOGIROLL_API FVector2D FrameShakeOffset(float Dt);
	ZOOGIROLL_API bool FrameShakeEnabled();
	ZOOGIROLL_API int32 FrameShakeKicks();

	/** Oct 14: the team column publishes each player's profile card (portrait box) rect in absolute desktop px each paint;
	 *  the anime comic strip lays its panels over the acting player's box. False if not painted in the last second. */
	ZOOGIROLL_API void SetProfileRect(int32 Player, const FVector2D& AbsMin, const FVector2D& AbsSize);
	ZOOGIROLL_API bool ProfileRect(int32 Player, FVector2D& OutAbsMin, FVector2D& OutAbsSize);

	ZOOGIROLL_API bool IsEnabled();
	/** Enabled and a match screen is up (not the main menu, not -ZoogiNoHUD). */
	ZOOGIROLL_API bool IsActive(const UWorld* World);
	ZOOGIROLL_API FLayout Compute(const FVector2D& ViewportPx, const FMargin& SafePx, float TrayOpen);
	/** Oct 17 (Evan): HUD shown = landscape framed hud-v3; HUD hidden = portrait, arena full screen with minimal edge controls.
	 *  Starts hidden when the first framed frame is taller than wide (-ZoogiHudShown / -ZoogiHudHidden force it). */
	ZOOGIROLL_API bool IsHudHidden();
	ZOOGIROLL_API void SetHudHidden(bool bHidden);
	/** Extra UI rects (viewport px) that count as UI, never world picks (portrait edge controls, the Standby tray overlay). */
	ZOOGIROLL_API void SetUiRects(const TArray<FBox2D>& Rects);
	/** Aspect-aware battle FOV (portrait only): the horizontal FOV is narrowed to the geometric mean of the landscape (16:9)
	 *  and portrait views, so the arena stretches up the tall screen without a fish-eye vertical FOV. Aspect >= 1: unchanged. */
	ZOOGIROLL_API float PortraitFov(float HorizontalFov, float Aspect);
	/** Minimum tap target in portrait design px (Evan, Oct 17: 58 px at 1080 wide). */
	constexpr float PortraitTap = 58.f;
	/** Last layout applied by the viewport client (bValid false when the frame is not active). */
	ZOOGIROLL_API const FLayout& Current();
	ZOOGIROLL_API void SetCurrent(const FLayout& L);
	ZOOGIROLL_API void Clear();
	/** Safe-area insets in pixels: platform title-safe padding, or -ZoogiSafeArea=L,T,R,B (1080p units) to simulate a notch. */
	ZOOGIROLL_API FMargin SafeInsets(const FVector2D& ViewportPx);
	/** Should the bottom tray be open this frame (compact layouts collapse it when no choice is pending). */
	ZOOGIROLL_API bool WantTrayOpen(const AZoogiGameMode* G);
	/** Aspect of the player's real view rect (falls back to the viewport). Used by camera framing. */
	ZOOGIROLL_API float ViewAspect(const APlayerController* PC);
	/** Viewport pixel position is in the UI margins (outside the arena rect) while the frame is active. */
	ZOOGIROLL_API bool IsOutsideArena(const UWorld* World, const FVector2D& ViewportPos);
	/** Oct 14 aim zoom: the ZOOM toggle sits inside the arena window; its rect (viewport px) counts as UI, never a world pick. */
	ZOOGIROLL_API void SetZoomButtonRect(const FBox2D& ViewportRect, bool bVisible);
	ZOOGIROLL_API bool InZoomButton(const FVector2D& ViewportPos);
	ZOOGIROLL_API bool ZoomButtonRect(FBox2D& Out); // viewport px, false when hidden
	/** Major-event light pulse on the arena frame (0..1, decays). */
	ZOOGIROLL_API void Pulse(const FLinearColor& Color);
	ZOOGIROLL_API float PulseAmount(FLinearColor& OutColor);
	ZOOGIROLL_API void TickPulse(float Dt);
	/** -ZoogiFramedShots with -ZoogiShots=<dir>: framed_* / classic_* review captures (engine screenshots). */
	ZOOGIROLL_API void TickShots(UWorld* World);
	/** Projection / input-mapping self-check, logged as "FRAMED aimcheck" (framed runs only). */
	ZOOGIROLL_API void TickAimCheck(UWorld* World);
}
