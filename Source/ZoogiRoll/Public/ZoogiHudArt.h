// hud-v3 (Evan, Oct 14 3:53 PM target "hud_v3_target.png"): art kit for the framed battle HUD. Every piece is sliced
// from Evan's own target image by Scripts/ui/hudv3/slice_hudv3.py into Content/Data/HudV3/*.png (2x the 1376x768
// target), loaded at runtime (FImageUtils, staged with Content/Data as UFS so it works from the pak) and drawn with
// Slate: plain images, 9-slice boxes (margins in target px) and a rotated image for the energy orb swirl.
// Texture loading / DPI tap-target helper reused from stone-hud (ZoogiStoneStyle, 48ee82a).
#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class FSlateWindowElementList;
struct FGeometry;
struct FSlateFontInfo;

namespace ZoogiHudArt
{
	/** Texture pixels per target pixel (the slicer upscales the 1376x768 target 2x). */
	constexpr float K = 2.f;
	/** Target pixels -> 1080p units (768 target rows = 1080 rows). */
	constexpr float T1080 = 1080.f / 768.f;

	ZOOGIROLL_API UTexture2D* Tex(const TCHAR* Name);
	/** Size of a piece in target px (0,0 if missing). */
	ZOOGIROLL_API FVector2D SizeT(const TCHAR* Name);
	/** Stretched image. */
	ZOOGIROLL_API bool Img(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, const FLinearColor& Tint = FLinearColor::White);
	/** Image at its native aspect, Px screen px per target px, top-left at P. Returns drawn size. */
	ZOOGIROLL_API FVector2D ImgT(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, float Px, const TCHAR* Name, const FLinearColor& Tint = FLinearColor::White);
	/** 9-slice box: Margin in target px of the piece, drawn at Px screen px per target px. */
	ZOOGIROLL_API bool Nine(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, const FMargin& MarginT, float Px, const FLinearColor& Tint = FLinearColor::White);
	/** Image rotated by AngleRad about its centre C. */
	ZOOGIROLL_API bool ImgRot(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& C, const FVector2D& S, const TCHAR* Name, float AngleRad, const FLinearColor& Tint = FLinearColor::White);
	/** Sub-rect of a piece (target px of the piece) stretched to P,S. */
	ZOOGIROLL_API bool Part(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, const FBox2D& SrcT, const FLinearColor& Tint = FLinearColor::White);
	/** Tiled stone backdrop over P,S (tile = TileT target px). */
	ZOOGIROLL_API void Tiled(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, float TileT, float Px, const FLinearColor& Tint = FLinearColor::White);
	/** Outlined text aligned at P (AlignX/AlignY 0 = left/top, 0.5 centre, 1 right/bottom). */
	ZOOGIROLL_API void Text(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FString& T, const FSlateFontInfo& F, const FLinearColor& C, float AlignX = 0.f, float AlignY = 0.f);
	ZOOGIROLL_API FVector2D Measure(const FString& T, const FSlateFontInfo& F);
	/** >= Dp density-independent pixels (48 dp tap targets) or FallbackPx when the screen density is unknown (PC). */
	ZOOGIROLL_API float TapPx(float Dp, float FallbackPx);

	// accents of the v3 target
	ZOOGIROLL_API extern const FLinearColor Cyan;
	ZOOGIROLL_API extern const FLinearColor GoldText;
	ZOOGIROLL_API extern const FLinearColor Steel;
	ZOOGIROLL_API extern const FLinearColor IceText;
}
