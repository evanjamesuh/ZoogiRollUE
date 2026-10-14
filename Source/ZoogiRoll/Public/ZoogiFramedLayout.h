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

	ZOOGIROLL_API bool IsEnabled();
	/** Enabled and a match screen is up (not the main menu, not -ZoogiNoHUD). */
	ZOOGIROLL_API bool IsActive(const UWorld* World);
	ZOOGIROLL_API FLayout Compute(const FVector2D& ViewportPx, const FMargin& SafePx, float TrayOpen);
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
	/** Major-event light pulse on the arena frame (0..1, decays). */
	ZOOGIROLL_API void Pulse(const FLinearColor& Color);
	ZOOGIROLL_API float PulseAmount(FLinearColor& OutColor);
	ZOOGIROLL_API void TickPulse(float Dt);
	/** -ZoogiFramedShots with -ZoogiShots=<dir>: framed_* / classic_* review captures (engine screenshots). */
	ZOOGIROLL_API void TickShots(UWorld* World);
	/** Projection / input-mapping self-check, logged as "FRAMED aimcheck" (framed runs only). */
	ZOOGIROLL_API void TickAimCheck(UWorld* World);
}
