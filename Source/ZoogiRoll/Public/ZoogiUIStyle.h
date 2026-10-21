// uistyle (Oct 14, Evan 1:39 AM CT): shared "clean cartoon + tech" UI look, recreated procedurally in code (no pack art).
// Chunky rounded bars with a thick deep-navy outline, saturated gradient fills, a white gloss strip along the top,
// inner tick marks + hex notch accents, saturated navy / teal panels (never flat gray), bold outlined text with a drop
// shadow, and value-change "juice" (fill lerp, white flash on gain, shake + red trail on loss).
// Two back ends: Canvas (AHUD / OnHUDPostRender overlays) and Slate (UMG NativePaint). Visual only.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

class UCanvas;
class FSlateWindowElementList;
struct FGeometry;
struct FButtonStyle;
struct FSlateBrush;

namespace ZoogiUIStyle
{
	// ---- palette
	ZOOGIROLL_API extern const FLinearColor Outline;      // deep navy, used for every outline
	ZOOGIROLL_API extern const FLinearColor PanelNavy;    // saturated navy panel fill (slightly transparent)
	ZOOGIROLL_API extern const FLinearColor PanelTeal;    // saturated teal panel fill (slightly transparent)
	ZOOGIROLL_API extern const FLinearColor Track;        // empty bar track
	ZOOGIROLL_API extern const FLinearColor ButtonNavy;   // idle button plate
	ZOOGIROLL_API extern const FLinearColor Muted;        // "disabled / out" tint (desaturated blue, not gray)
	ZOOGIROLL_API extern const FLinearColor MutedText;
	ZOOGIROLL_API extern const FLinearColor EnergyA, EnergyB;         // Episode Energy cyan -> electric blue
	ZOOGIROLL_API extern const FLinearColor CardEnergyA, CardEnergyB; // Card Energy violet
	ZOOGIROLL_API extern const FLinearColor Gold;
	ZOOGIROLL_API extern const FLinearColor LossRed;
	// framed (spec v2.0 palette): deep blue background, friendly cyan / electric blue, enemy coral, primary buttons gold
	// (Gold above), powers violet, secondary royal blue, positive mint.
	ZOOGIROLL_API extern const FLinearColor DeepBg;
	ZOOGIROLL_API extern const FLinearColor Friendly;
	ZOOGIROLL_API extern const FLinearColor Enemy;
	ZOOGIROLL_API extern const FLinearColor Violet;
	ZOOGIROLL_API extern const FLinearColor RoyalBlue;
	ZOOGIROLL_API extern const FLinearColor Mint;

	/** Health colour by value: green (full) -> yellow -> red (empty). */
	ZOOGIROLL_API FLinearColor HealthColor(float Frac);

	/** Value-change juice for one bar / counter. Feed it the target value each frame. */
	struct ZOOGIROLL_API FBarJuice
	{
		float Shown = -1.f;   // lerped display value
		float Trail = -1.f;   // red loss trail (lags behind on loss)
		float Flash = 0.f;    // white flash on gain (1 -> 0)
		float Shake = 0.f;    // shake on loss (1 -> 0)
		float Last = -1.f;
		void Update(float Target, float Dt);
		/** Pixel offset for the shake this frame. */
		FVector2D ShakeOffset(float Amp, float Time) const;
	};

	// ---- fonts
	/** Bold font with a navy outline (Slate/UMG and the Canvas Slate-font path both honour OutlineSettings). */
	ZOOGIROLL_API FSlateFontInfo BoldFont(int32 Size, float OutlinePx = 2.f);
	ZOOGIROLL_API void AddOutline(FSlateFontInfo& F, float OutlinePx = 2.f);
	/** Bold outlined font for Canvas text (backed by the engine font object so FCanvasTextItem can draw it). */
	ZOOGIROLL_API FSlateFontInfo CanvasFont(int32 Size, float OutlinePx = 2.f);

	// ---- Canvas back end (U = pixels per 1080p unit)
	ZOOGIROLL_API void CRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);
	ZOOGIROLL_API void CRoundRect(UCanvas* C, float X, float Y, float W, float H, float R, const FLinearColor& Col);
	/** Rounded panel: navy outline, saturated fill, faint top sheen. */
	ZOOGIROLL_API void CPanel(UCanvas* C, float X, float Y, float W, float H, float U, const FLinearColor& Fill, const FLinearColor& Accent = FLinearColor::Transparent);
	/** Chunky bar: outline, track, horizontal gradient fill, red loss trail, gloss strip, tick marks, hex end notches, flash. */
	ZOOGIROLL_API void CBar(UCanvas* C, float X, float Y, float W, float H, float U, float Frac, const FLinearColor& A, const FLinearColor& B,
		const FBarJuice* Juice = nullptr, int32 Ticks = 4, float Time = 0.f);
	/** Outlined + shadowed text with a Slate font. */
	ZOOGIROLL_API void CText(UCanvas* C, const FString& S, float X, float Y, const FSlateFontInfo& Font, const FLinearColor& Col, float U = 1.f);

	// ---- Slate back end (local units)
	ZOOGIROLL_API void SBox(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& Fill, const FLinearColor& Edge, float R, float EdgeW);
	ZOOGIROLL_API void SPanel(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& Fill, const FLinearColor& Accent = FLinearColor::Transparent, float R = 12.f);
	/** Uses layers L .. L+3. */
	ZOOGIROLL_API void SBar(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, float Frac, const FLinearColor& A, const FLinearColor& B,
		const FBarJuice* Juice = nullptr, int32 Ticks = 4);
	ZOOGIROLL_API void SText(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FString& T, const FSlateFontInfo& F, const FLinearColor& C);

	/** Rounded, outlined brush for UMG borders / buttons. */
	ZOOGIROLL_API FSlateBrush RoundBrush(const FLinearColor& Fill, const FLinearColor& Edge, float EdgeW, float R = 10.f);
	/** Chunky cartoon button: saturated accent plate (navy when idle), navy outline, brighter on hover. */
	ZOOGIROLL_API FButtonStyle ButtonStyle(const FLinearColor& Accent, bool bFilled);

	// ---- button animation (menu now, in-match HUD later). Drive one FButtonAnim per button each frame; apply the
	// returned scale as the widget's render scale (pivot centre), Opacity as render opacity and Gloss to the gloss strip.
	/** Ease-out-back (overshoot ~10%), T in 0..1. */
	ZOOGIROLL_API float EaseOutBack(float T);
	struct ZOOGIROLL_API FButtonAnim
	{
		float Delay = 0.f;        // screen-enter stagger (s)
		bool bPrimary = false;    // PLAY / START: subtle idle pulse + gloss shine
		bool bDisabled = false;   // no animation at all
		bool bEnter = true;       // pop in on screen enter
		float Age = 0.f, Scale = 1.f, Vel = 0.f, Gloss = 0.f;
		bool bWasPressed = false;
		// outputs
		float OutScale = 1.f, OutOpacity = 1.f, OutGloss = 0.f; // OutGloss 0 idle .. 1 bright
		/** Hover/focus grow 1.05 (eased), press squash 0.94 (on touch/mouse down), springy release, enter pop (ease-out-back, 180 ms). */
		void Tick(bool bHovered, bool bPressed, float Dt, float Time);
	};
}
