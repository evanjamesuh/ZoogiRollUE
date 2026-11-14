// Oct 30 (Evan): Persona 5 Royal-STYLE menus + transition for Zoogi Roll (see ZoogiP5Style.h). Own art, own fonts (SIL OFL).
#include "ZoogiP5Style.h"
#include "ZoogiWallCine.h" // b232
#include "ZoogiArena.h"
#include "ZoogiUIFx.h"
#include "ZoogiHaptics.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "ZoogiGuide.h"
#include "ZoogiAnimeCues.h"

// ------------------------------------------------------------------------------------------------ palette + fonts
namespace ZoogiP5
{
	bool MenuOn()
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClassicMenu")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMenu"));
		return !bOff;
	}
	FLinearColor Accent() { return FLinearColor::FromSRGBColor(FColor(255, 18, 128)); } // Zoogi Roll hot magenta
	FLinearColor ElementColor(const FString& E)
	{
		const FString L = E.ToLower();
		if (L == TEXT("void")) return FLinearColor::FromSRGBColor(FColor(124, 58, 237));
		if (L == TEXT("fire")) return FLinearColor::FromSRGBColor(FColor(255, 72, 24));
		if (L == TEXT("shock")) return FLinearColor::FromSRGBColor(FColor(255, 214, 0));
		if (L == TEXT("frost")) return FLinearColor::FromSRGBColor(FColor(56, 189, 248));
		return FLinearColor::FromSRGBColor(FColor(230, 230, 230)); // neutral
	}
	static TSharedPtr<const FCompositeFont> FontFile(int32 Which)
	{
		static TSharedPtr<const FCompositeFont> F[4];
		static bool bTried[4] = { false, false, false, false };
		Which = FMath::Clamp(Which, 0, 3);
		if (!bTried[Which])
		{
			bTried[Which] = true;
			static const TCHAR* Files[] = { TEXT("Anton-Regular.ttf"), TEXT("ArchivoBlack-Regular.ttf"), TEXT("AbrilFatface-Regular.ttf"), TEXT("Bangers-Regular.ttf") };
			const FString P = FPaths::ProjectContentDir() / TEXT("Data/Fonts") / Files[Which];
			if (FPaths::FileExists(P)) F[Which] = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), P, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			else UE_LOG(LogZoogi, Warning, TEXT("P5 style: font missing %s"), *P);
		}
		return F[Which];
	}
	FSlateFontInfo Font(int32 Which, float Size, float Outline)
	{
		Size = FMath::Clamp(Size, 1.f, 1000.f); // Oct 31 b41: ease-out-back pops start below 0 (SlateFontInfo ensure + a 2 s hitch)
		TSharedPtr<const FCompositeFont> C = FontFile(Which);
		FSlateFontInfo Fi = C.IsValid() ? FSlateFontInfo(C, Size, FName(TEXT("Regular"))) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Fi.OutlineSettings.OutlineSize = FMath::RoundToInt(Outline);
		Fi.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
		return Fi;
	}

	// ---- textures (rasterised once, white, tinted at draw time)
	static UTexture2D* MakeTex(int32 Size, TFunctionRef<float(float, float)> Coverage)
	{
		ZoogiUIFx::FPixels P; P.W = P.H = Size; P.Px.SetNumZeroed(Size * Size);
		for (int32 y = 0; y < Size; ++y)
			for (int32 x = 0; x < Size; ++x)
			{
				float A = 0.f;
				for (int32 s = 0; s < 4; ++s) A += Coverage(x + ((s % 2) + 0.5f) / 2.f, y + ((s / 2) + 0.5f) / 2.f);
				A *= 0.25f;
				if (A > 0.f) P.Px[y * Size + x] = FColor(255, 255, 255, (uint8)FMath::Clamp(A * 255.f, 0.f, 255.f));
			}
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		if (T) T->AddToRoot();
		return T;
	}
	static bool InPoly(const TArray<FVector2D>& Poly, float X, float Y)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
			if (((Poly[i].Y > Y) != (Poly[j].Y > Y)) && (X < (Poly[j].X - Poly[i].X) * (Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X)) bIn = !bIn;
		return bIn;
	}
	/** jagged hand-cut starburst, 12 uneven spikes */
	static UTexture2D* BurstTex()
	{
		static UTexture2D* T = nullptr;
		if (T) return T;
		TArray<FVector2D> Poly;
		for (int32 k = 0; k < 24; ++k)
		{
			const float A = UE_TWO_PI * k / 24.f + 0.06f * FMath::Sin(k * 1.9f);
			const float R = (k % 2 == 0) ? 126.f * (0.78f + 0.22f * FMath::Abs(FMath::Sin(k * 2.7f + 0.3f))) : 80.f + 8.f * FMath::Sin(k * 1.3f);
			Poly.Add(FVector2D(128.f + FMath::Cos(A) * R, 128.f + FMath::Sin(A) * R));
		}
		T = MakeTex(256, [&Poly](float X, float Y) { return InPoly(Poly, X, Y) ? 1.f : 0.f; });
		return T;
	}
	/** halftone dots, one tile = 8 x 8 dots, dot radius fixed (the draw size sets the dot scale) */
	static UTexture2D* DotsTex()
	{
		static UTexture2D* T = nullptr;
		if (T) return T;
		T = MakeTex(128, [](float X, float Y)
		{
			const float Cx = FMath::Fmod(X, 16.f) - 8.f, Cy = FMath::Fmod(Y, 16.f) - 8.f;
			return FMath::Sqrt(Cx * Cx + Cy * Cy) < 4.6f ? 1.f : 0.f;
		});
		return T;
	}
	/** torn-paper strip: a ragged top edge (used along panel edges) */
	static UTexture2D* TornTex()
	{
		static UTexture2D* T = nullptr;
		if (T) return T;
		T = MakeTex(256, [](float X, float Y)
		{
			const float Edge = 70.f + 22.f * FMath::Sin(X * 0.21f) + 14.f * FMath::Sin(X * 0.57f + 1.3f) + 9.f * FMath::Sin(X * 1.31f + 0.4f);
			return Y > Edge ? 1.f : 0.f;
		});
		return T;
	}

	// ---- paint helpers (all in the design-space geometry handed in)
	static const FSlateBrush* White() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	static FSlateVertex Vx(const FSlateRenderTransform& RT, const FVector2D& P, const FColor& C) { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P), FVector2f(0.5f, 0.5f), C); }
	static void Quad(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Col)
	{
		const FSlateResourceHandle RH = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White());
		const FSlateRenderTransform& RT = G.GetAccumulatedRenderTransform();
		const FColor Cc = Col.ToFColor(true);
		TArray<FSlateVertex> V = { Vx(RT, A, Cc), Vx(RT, B, Cc), Vx(RT, C, Cc), Vx(RT, D, Cc) };
		TArray<SlateIndex> I = { 0, 1, 2, 0, 2, 3 };
		FSlateDrawElement::MakeCustomVerts(Out, L, RH, V, I, nullptr, 0, 0);
	}
	static void Tri(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Col)
	{
		Quad(Out, G, L, A, B, C, C, Col);
	}
	/** rotated box: Pos = centre */
	static void Box(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& Ctr, const FVector2D& Size, float AngDeg, const FLinearColor& Col, const FSlateBrush* Brush = nullptr)
	{
		const FGeometry C = G.MakeChild(Size, FSlateLayoutTransform(Ctr - Size * 0.5f), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngDeg))), FVector2D(0.5f, 0.5f));
		FSlateDrawElement::MakeBox(Out, L, C.ToPaintGeometry(), Brush ? Brush : White(), ESlateDrawEffect::None, Col);
	}
	static void Tex(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& Ctr, const FVector2D& Size, float AngDeg, const FLinearColor& Col)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		Box(Out, G, L, Ctr, Size, AngDeg, Col, &B);
	}
	/** b120: the texture cover-cropped to Size (centred), no rotation */
	static void TexCover(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& Ctr, const FVector2D& Size, const FLinearColor& Col)
	{
		if (!T || Size.X < 1.f || Size.Y < 1.f) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = Size;
		const float Ta = (float)T->GetSizeX() / FMath::Max(1, T->GetSizeY()), Ba = Size.X / Size.Y;
		if (Ta > Ba) { const float W = Ba / Ta; B.SetUVRegion(FBox2f(FVector2f(0.5f - W * 0.5f, 0.f), FVector2f(0.5f + W * 0.5f, 1.f))); }
		else { const float Hh = Ta / Ba; B.SetUVRegion(FBox2f(FVector2f(0.f, 0.5f - Hh * 0.5f), FVector2f(1.f, 0.5f + Hh * 0.5f))); }
		Box(Out, G, L, Ctr, Size, 0.f, Col, &B);
	}
	static void Tiled(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL, const FVector2D& Size, float Tile, const FLinearColor& Col)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(Tile, Tile); B.Tiling = ESlateBrushTileType::Both;
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Size, FSlateLayoutTransform(TL)), &B, ESlateDrawEffect::None, Col);
	}
	static FVector2D Measure(const FString& S, const FSlateFontInfo& F) { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F); }
	/** text centred on Ctr, rotated */
	static void Text(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& Ctr, float AngDeg, const FLinearColor& Col)
	{
		const FVector2D Sz = Measure(S, F);
		const FGeometry C = G.MakeChild(Sz, FSlateLayoutTransform(Ctr - Sz * 0.5f), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngDeg))), FVector2D(0.5f, 0.5f));
		FSlateDrawElement::MakeText(Out, L, C.ToPaintGeometry(), S, F, ESlateDrawEffect::None, Col);
	}
	/** b170 (Evan's mode select examples): the same text, rotated and slanted (italic) */
	static void TextItalic(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& Ctr, float AngDeg, const FLinearColor& Col, float Slant = 0.2f)
	{
		const FVector2D Sz = Measure(S, F);
		const FGeometry C = G.MakeChild(Sz, FSlateLayoutTransform(Ctr - Sz * 0.5f), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngDeg))), FVector2D(0.5f, 0.5f));
		const FGeometry C2 = C.MakeChild(Sz, FSlateLayoutTransform(), FSlateRenderTransform(FShear2D(-Slant, 0.f)), FVector2D(0.5f, 0.5f));
		FSlateDrawElement::MakeText(Out, L, C2.ToPaintGeometry(), S, F, ESlateDrawEffect::None, Col);
	}
	/** Oct 31 b46 (Evan: "I still wanna see Zoogi Roll written in the background like the mode selection screen on selections and menus"):
	 *  the old mode-select wordmark wallpaper - tilted two-line ZOOGI / ROLL stamps in a staggered grid, faint, drifting slowly. */
	static void WordmarkWall(FSlateWindowElementList& Out, const FGeometry& G, int32 L, float DW, float DH, float T, const FLinearColor& Col)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWordmark"));
		if (bOff) return;
		const FSlateFontInfo FA = Font(1, 46.f), FB = Font(1, 30.f);
		const float SX = 300.f, SY = 150.f, Ang = -12.f, Dx = FMath::Fmod(T * 14.f, SX);
		int32 Row = 0;
		for (float Y = -SY; Y < DH + SY; Y += SY, ++Row)
			for (float X = -SX + Dx + (Row % 2) * SX * 0.5f; X < DW + SX; X += SX)
			{
				Text(Out, G, L, TEXT("ZOOGI"), FA, FVector2D(X, Y), Ang, Col);
				Text(Out, G, L, TEXT("ROLL"), FB, FVector2D(X + 8.f, Y + 36.f), Ang, Col);
			}
	}
	/** textured quad (custom verts): P[] corners clockwise from top-left, UV[] matching texture coords */
	static void TexQuad(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D P[4], const FVector2D UV[4], const FLinearColor& Col)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		const FSlateResourceHandle RH = FSlateApplication::Get().GetRenderer()->GetResourceHandle(B);
		const FSlateRenderTransform& RT = G.GetAccumulatedRenderTransform();
		const FColor Cc = Col.ToFColor(true);
		TArray<FSlateVertex> V;
		for (int32 i = 0; i < 4; ++i) V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P[i]), FVector2f(UV[i]), Cc));
		TArray<SlateIndex> I = { 0, 1, 2, 0, 2, 3 };
		FSlateDrawElement::MakeCustomVerts(Out, L, RH, V, I, nullptr, 0, 0);
	}
	/** rotated box filled with a tiling texture */
	static void TiledRot(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& Ctr, const FVector2D& Size, float AngDeg, float Tile, const FLinearColor& Col)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(Tile, Tile); B.Tiling = ESlateBrushTileType::Both;
		Box(Out, G, L, Ctr, Size, AngDeg, Col, &B);
	}
	static float EaseOutBack(float T) { T = FMath::Clamp(T, 0.f, 1.f); const float C1 = 1.9f, C3 = C1 + 1.f; return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f); }
	static float EaseInOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T < 0.5f ? 4.f * T * T * T : 1.f - FMath::Pow(-2.f * T + 2.f, 3.f) * 0.5f; }
	static uint32 Hash(uint32 X) { X ^= X >> 16; X *= 0x7feb352d; X ^= X >> 15; X *= 0x846ca68b; X ^= X >> 16; return X; }

	/** Ransom-note word(s): every letter its own font / size / tilt / box (white-on-black, black-on-white, accent), slamming in one
	 *  after another from Age 0. Left edge at Left, vertical centre at Y. Returns the total width. */
	static float Ransom(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, float Left, float Y, float Base, float AngDeg, uint32 Seed, float Age, const FLinearColor& Acc)
	{
		float X = Left;
		const float Rad = FMath::DegreesToRadians(AngDeg);
		const FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
		for (int32 i = 0; i < S.Len(); ++i)
		{
			const FString Ch = S.Mid(i, 1);
			if (Ch == TEXT(" ")) { X += Base * 0.38f; continue; }
			const uint32 H = Hash(Seed * 131u + i * 977u);
			const int32 FontIdx = (int32)(H % 4u);
			const float Sz = Base * (0.84f + 0.32f * ((H >> 4) % 100u) / 100.f) * (FontIdx == 0 ? 1.12f : 1.f); // Anton runs narrow: a bit bigger
			const float Tilt = AngDeg + (((int32)((H >> 12) % 21u)) - 10) * 1.1f;
			const int32 Style = (int32)((H >> 20) % 5u); // 0,1 white box / black letter, 2,3 black box / white letter, 4 accent box / white letter
			const FSlateFontInfo F = Font(FontIdx, Sz, 0.f);
			const FVector2D TS = Measure(Ch, F);
			const FVector2D Pad(Sz * 0.16f, Sz * 0.04f);
			const FVector2D BoxSz = TS + Pad * 2.f;
			const float LAge = Age - 0.035f * i;
			if (LAge < 0.f) { X += BoxSz.X + Base * 0.05f; continue; }
			const float K = FMath::Clamp(LAge / 0.16f, 0.f, 1.f);
			const float Slam = 1.f + 1.4f * (1.f - EaseOutBack(K)); // big -> overshoot -> settle (never a soft fade)
			const FVector2D Ctr = FVector2D(Left, Y) + Dir * (X - Left) + FVector2D(0.f, (((int32)((H >> 8) % 9u)) - 4) * Sz * 0.02f) + FVector2D(BoxSz.X * 0.5f * Dir.X, BoxSz.X * 0.5f * Dir.Y); // the word tilts around its own left edge
			const FLinearColor BoxC = Style <= 1 ? FLinearColor::White : (Style <= 3 ? FLinearColor::Black : Acc);
			const FLinearColor TxtC = Style <= 1 ? FLinearColor::Black : FLinearColor::White;
			Box(Out, G, L, Ctr + FVector2D(Sz * 0.07f, Sz * 0.09f) * Slam, BoxSz * Slam, Tilt, Style == 4 ? FLinearColor::Black : Acc); // offset shadow block
			Box(Out, G, L + 1, Ctr, BoxSz * Slam, Tilt, BoxC);
			Text(Out, G, L + 2, Ch, Font(FontIdx, Sz * Slam, 0.f), Ctr, Tilt, TxtC);
			X += BoxSz.X + Base * 0.05f;
		}
		return X - Left;
	}
	/** the width Ransom() will use for S (same per-letter metrics, nothing drawn) */
	static float RansomWidth(const FString& S, float Base, uint32 Seed)
	{
		float X = 0.f;
		for (int32 i = 0; i < S.Len(); ++i)
		{
			const FString Ch = S.Mid(i, 1);
			if (Ch == TEXT(" ")) { X += Base * 0.38f; continue; }
			const uint32 H = Hash(Seed * 131u + i * 977u);
			const int32 FontIdx = (int32)(H % 4u);
			const float Sz = Base * (0.84f + 0.32f * ((H >> 4) % 100u) / 100.f) * (FontIdx == 0 ? 1.12f : 1.f);
			const FVector2D TS = Measure(Ch, Font(FontIdx, Sz, 0.f));
			X += TS.X + Sz * 0.32f + Base * 0.05f;
		}
		return X;
	}
}

using namespace ZoogiP5;

// ------------------------------------------------------------------------------------------------ the wipe
namespace ZoogiP5Wipe
{
	class SWipe : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SWipe) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		float T = 0.f;
		FLinearColor Tint = FLinearColor::White;
		FString Shout;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
		virtual bool ComputeVolatility() const override { return true; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (ZoogiWallCine::Active() || ZoogiWallCine::BannerUp()) { return Layer; } // b232: the wall-smash cinematic plays on a clean screen
			const FVector2D Sz = Geo.GetLocalSize();
			const float W = Sz.X, H = Sz.Y, Sk = W * 0.22f;
			// five shards sweep right -> left; together they cover the screen around 0.28 s, then clear it
			const FLinearColor Cols[] = { FLinearColor::White, Accent(), FLinearColor::Black, Tint, FLinearColor::Black };
			for (int32 k = 0; k < 5; ++k)
			{
				const float E = EaseInOut((T - 0.022f * k) / 0.56f);
				const float Cx = FMath::Lerp(W * 1.95f, -W * 0.95f, E);
				const float Hw = W * 0.66f - k * W * 0.02f;
				Quad(Out, Geo, Layer + k, FVector2D(Cx - Hw + Sk, -10.f), FVector2D(Cx + Hw + Sk, -10.f), FVector2D(Cx + Hw - Sk, H + 10.f), FVector2D(Cx - Hw - Sk, H + 10.f), Cols[k]);
			}
			// the shout rides the front (black) shard, and a burst flashes at full cover
			const float E4 = EaseInOut((T - 0.088f) / 0.56f);
			const float C4 = FMath::Lerp(W * 1.95f, -W * 0.95f, E4);
			if (UTexture2D* B = BurstTex())
			{
				const float Fl = FMath::Clamp(1.f - FMath::Abs(T - 0.3f) / 0.12f, 0.f, 1.f);
				if (Fl > 0.f) Tex(Out, Geo, Layer + 6, B, FVector2D(C4, H * 0.5f), FVector2D(H * 0.9f, H * 0.9f) * (0.8f + 0.4f * Fl), T * 90.f, Accent().CopyWithNewOpacity(Fl));
			}
			const float Base = H * 0.12f;
			Ransom(Out, Geo, Layer + 7, Shout, C4 - Base * 2.6f, H * 0.5f, Base, -8.f, 7u, 1.f, Accent());
			return Layer + 10;
		}
	};
	struct FState { TSharedPtr<SWipe> W; TWeakObjectPtr<UGameViewportClient> VC; TFunction<void()> OnCovered; bool bCovered = false; FTSTicker::FDelegateHandle Tick; TWeakObjectPtr<UObject> Ctx; };
	static FState GState;
	static void Remove()
	{
		if (GState.W.IsValid()) if (UGameViewportClient* V = GState.VC.Get()) V->RemoveViewportWidgetContent(GState.W.ToSharedRef());
		GState.W.Reset();
		if (GState.Tick.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(GState.Tick);
		GState.Tick.Reset();
		GState.OnCovered = nullptr;
	}
}

namespace ZoogiP5
{
	bool WipeBusy() { return ZoogiP5Wipe::GState.W.IsValid(); }

	void PlayWipe(UWorld* World, TFunction<void()> OnCovered, const FLinearColor& Tint, const FString& Shout)
	{
		using namespace ZoogiP5Wipe;
		UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
		if (!VC) { if (OnCovered) OnCovered(); return; }
		if (GState.W.IsValid()) { if (!GState.bCovered && GState.OnCovered) { TFunction<void()> F = MoveTemp(GState.OnCovered); F(); } Remove(); }
		GState.W = SNew(SWipe);
		GState.W->Tint = Tint;
		GState.W->Shout = Shout;
		GState.VC = VC;
		GState.OnCovered = MoveTemp(OnCovered);
		GState.bCovered = false;
		GState.Ctx = World;
		VC->AddViewportWidgetContent(GState.W.ToSharedRef(), 95);
		ZoogiUtil::PlaySfx(World, TEXT("Dash"), 0.9f, 1.15f);
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5MenuTest")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5ResultsTest"));
		GState.Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt)
		{
			if (!GState.W.IsValid()) return false;
			GState.W->T += bTest ? 1.f / 24.f : FMath::Min(Dt, 0.05f);
			if (!GState.bCovered && GState.W->T >= 0.29f)
			{
				GState.bCovered = true;
				if (UObject* C = GState.Ctx.Get()) ZoogiUtil::PlaySfx(C, TEXT("Clack"), 1.f, 0.9f);
				if (GState.OnCovered) { TFunction<void()> F = MoveTemp(GState.OnCovered); F(); }
			}
			if (GState.W.IsValid() && GState.W->T >= 0.72f) { Remove(); return false; }
			return true;
		}));
		UE_LOG(LogZoogi, Log, TEXT("P5 WIPE start (%s)"), *Shout);
	}
}

// ------------------------------------------------------------------------------------------------ character cut-in (crit)
namespace ZoogiP5Cut
{
	// Choreography on a 1.1 s clock K (squeezed for shorter plays):
	//  0.00  scene snaps darker (black diagonal slab), 2-frame white impact frame
	//  0.00  the slanted band slams in from the right (overshoot), white rim + magenta offset strip, element colour, halftone, speed lines
	//  0.04  the eyes close-up rides in faster (parallax) inside a slanted window, then slowly pushes in
	//  0.10  name tag (ransom)          0.16  impact text slams (ransom) on a jagged burst + "clack"
	//  0.86  everything shoots off to the left (ease-in, no fade) -> gone at 1.06
	class SCutIn : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCutIn) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		float T = 0.f, Len = 1.1f;
		FCutIn C;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
		virtual bool ComputeVolatility() const override { return true; }
		float Clock() const { return T * 1.1f / FMath::Max(0.3f, Len); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (ZoogiWallCine::Active() || ZoogiWallCine::BannerUp()) { return Layer; } // b232: the wall-smash cinematic plays on a clean screen
			const FVector2D Sz = Geo.GetLocalSize();
			const float W = Sz.X, H = Sz.Y;
			const float K = Clock();
			const float OutK = FMath::Clamp((K - 0.86f) / 0.2f, 0.f, 1.f);
			const float OutE = OutK * OutK * OutK;
			const FLinearColor Col = C.Col, Acc = Accent();
			const FLinearColor Deep(Col.R * 0.35f, Col.G * 0.35f, Col.B * 0.42f, 1.f);
			const float Ang = -7.f;
			const float Rad = FMath::DegreesToRadians(Ang);
			const FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad)), Nrm(-FMath::Sin(Rad), FMath::Cos(Rad));
			const float Bh = FMath::Min(H * 0.30f, W * 0.42f);
			const float Ex = -W * 1.9f * OutE;
			int32 L = Layer;

			// 1. the scene behind snaps darker: a black slab with slanted edges sweeps on, and off with the rest
			{
				const float In = EaseInOut(K / 0.08f);
				const float Cx = FMath::Lerp(W * 1.7f, W * 0.5f, In) + Ex;
				const float Hw = W * 0.8f, Sk = H * 0.22f;
				Quad(Out, Geo, L, FVector2D(Cx - Hw + Sk, -10.f), FVector2D(Cx + Hw + Sk, -10.f), FVector2D(Cx + Hw - Sk, H + 10.f), FVector2D(Cx - Hw - Sk, H + 10.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
			}
			if (K < 0.05f) Box(Out, Geo, L + 1, Sz * 0.5f, Sz + FVector2D(40.f, 40.f), 0.f, FLinearColor(1.f, 1.f, 1.f, 0.8f));

			// impact text metrics (the text sits UNDER the band so the eyes stay clear; its burst peeks out from behind the band)
			const float TB = FMath::Min(H * 0.115f, W * 0.095f);
			const float Tw2 = RansomWidth(C.Shout, TB, 41u);
			// 2. the band
			const float SlamX = (1.f - EaseOutBack(K / 0.13f)) * W * 1.25f;
			const FVector2D BC = FVector2D(W * 0.5f, H * 0.44f) + FVector2D(SlamX + Ex, 0.f);
			const float Bw = W * 1.6f + Bh * 2.f;
			auto At = [&](float U, float V) { return BC + Dir * U + Nrm * V; };
			const bool bWide = W > H * 1.2f;
			const FVector2D TC = At(bWide ? -W * 0.17f : -W * 0.06f, Bh * 0.5f + TB * 0.45f); // wide screens: text left, face right
			if (K > 0.14f)
				if (UTexture2D* Bt = BurstTex())
				{
					const float Bk = EaseOutBack((K - 0.14f) / 0.12f);
					Tex(Out, Geo, L + 1, Bt, TC + FVector2D(TB * 0.3f, TB * 0.25f), FVector2D(Tw2 * 0.9f, Tw2 * 0.55f) * Bk, 8.f + K * 25.f, FLinearColor::Black);
					Tex(Out, Geo, L + 2, Bt, TC, FVector2D(Tw2 * 0.8f, Tw2 * 0.48f) * Bk, -6.f - K * 40.f, Col);
				}
			Box(Out, Geo, L + 2, BC + FVector2D(-Bh * 0.08f, Bh * 0.14f), FVector2D(Bw, Bh * 1.08f), Ang, Acc);                 // magenta offset strip
			Box(Out, Geo, L + 3, BC, FVector2D(Bw, Bh * 1.1f), Ang, FLinearColor::White);                                          // white rim
			Box(Out, Geo, L + 4, BC, FVector2D(Bw, Bh), Ang, FLinearColor::Black);
			Box(Out, Geo, L + 5, BC, FVector2D(Bw, Bh * 0.84f), Ang, Col);                                                          // element colour
			TiledRot(Out, Geo, L + 6, DotsTex(), BC, FVector2D(Bw, Bh * 0.84f), Ang, Bh * 0.22f, Deep.CopyWithNewOpacity(0.5f));   // halftone
			// speed lines racing left (the panel is never still)
			for (int32 k = 0; k < 16; ++k)
			{
				const uint32 Hh = Hash(4099u + k * 7919u);
				const float SLen = Bw * (0.08f + 0.16f * (Hh % 100u) / 100.f);
				const float Sp = W * (2.2f + 1.6f * ((Hh >> 8) % 100u) / 100.f);
				const float U0 = FMath::Fmod((float)((Hh >> 3) % 4096u) / 4096.f * Bw * 2.f - K * Sp + Bw * 8.f, Bw * 1.2f) - Bw * 0.6f;
				const float V0 = (((float)((Hh >> 16) % 1000u) / 1000.f) - 0.5f) * Bh * 0.78f;
				const float Th = (k % 4 == 0 ? 0.035f : 0.014f) * Bh;
				Box(Out, Geo, L + 7, At(U0, V0), FVector2D(SLen, Th), Ang, (k % 5 == 0 ? Deep : FLinearColor::White).CopyWithNewOpacity(k % 5 == 0 ? 0.6f : 0.55f));
			}
			// thin slash lines riding above and below the band
			Box(Out, Geo, L + 3, At(-W * 0.1f, -Bh * 0.68f), FVector2D(Bw, Bh * 0.025f), Ang, FLinearColor::White);
			Box(Out, Geo, L + 3, At(W * 0.15f, Bh * 0.70f), FVector2D(Bw, Bh * 0.02f), Ang, Col);

			// 3. the eyes close-up in a slanted window (white frame + magenta collage block behind it)
			if (UTexture2D* F = C.Face.Get())
			{
				const float Ki = FMath::Clamp((K - 0.04f) / 0.12f, 0.f, 1.f);
				const float Par = (1.f - EaseOutBack(Ki)) * W * 0.7f;
				const float Hh = Bh * 0.40f, Sl = Hh * 0.5f;
				const float Tw = (float)F->GetSizeX(), Th2 = (float)F->GetSizeY();
				const float Ra = ((C.UV1.X - C.UV0.X) * Tw) / FMath::Max(1.f, (C.UV1.Y - C.UV0.Y) * Th2);
				const float Ww = FMath::Min(2.f * Hh * Ra, W * 0.95f);
				const float Uc = (bWide ? W * 0.15f : W * 0.1f) + Par + K * W * 0.03f;   // slow drift right while it holds
				const float U0 = Uc - Ww * 0.5f, U1 = Uc + Ww * 0.5f;
				auto Win = [&](float Grow, const FVector2D& Off, FVector2D P[4])
				{
					P[0] = At(U0 + Sl - Grow, -Hh - Grow) + Off; P[1] = At(U1 + Sl + Grow, -Hh - Grow) + Off;
					P[2] = At(U1 - Sl + Grow, Hh + Grow) + Off;  P[3] = At(U0 - Sl - Grow, Hh + Grow) + Off;
				};
				FVector2D P[4];
				Win(Bh * 0.05f, FVector2D(Bh * 0.07f, Bh * 0.09f), P); Quad(Out, Geo, L + 8, P[0], P[1], P[2], P[3], FLinearColor::Black);   // hard shadow
				Win(Bh * 0.045f, FVector2D(-Bh * 0.05f, -Bh * 0.04f), P); Quad(Out, Geo, L + 8, P[0], P[1], P[2], P[3], Acc);
				Win(Bh * 0.03f, FVector2D::ZeroVector, P); Quad(Out, Geo, L + 9, P[0], P[1], P[2], P[3], FLinearColor::White);
				Win(0.f, FVector2D::ZeroVector, P);
				// texture coords: the eye strip, the slanted ends cut into it; a slow push-in (zoom ~6 %)
				const float Z = 0.03f * FMath::Clamp(K, 0.f, 1.f);
				const FVector2D A0 = C.UV0 + (C.UV1 - C.UV0) * Z, A1 = C.UV1 - (C.UV1 - C.UV0) * Z;
				const float Sx = (A1.X - A0.X) * Sl / FMath::Max(1.f, Ww + 2.f * Sl) * 2.f;
				FVector2D UV[4] = { FVector2D(A0.X + Sx, A0.Y), FVector2D(A1.X + Sx, A0.Y), FVector2D(A1.X - Sx, A1.Y), FVector2D(A0.X - Sx, A1.Y) };
				TexQuad(Out, Geo, L + 10, F, P, UV, FLinearColor::White);
			}

			// 4. name tag + impact text
			const float NB = Bh * 0.2f;
			if (K > 0.10f)
				Ransom(Out, Geo, L + 12, C.Name, At(-W * 0.44f, -Bh * 0.6f).X, At(-W * 0.44f, -Bh * 0.6f).Y, NB, Ang - 3.f, 23u, K - 0.10f, Acc);
			if (K > 0.16f) Ransom(Out, Geo, L + 18, C.Shout, TC.X - Tw2 * 0.5f, TC.Y, TB, Ang - 2.f, 41u, K - 0.16f, Acc);
			return L + 22;
		}
	};
	struct FState { TSharedPtr<SCutIn> W; TWeakObjectPtr<UGameViewportClient> VC; FTSTicker::FDelegateHandle Tick; TWeakObjectPtr<UObject> Ctx; int32 Step = 0; int32 Shots = 0; bool bClack = false; };
	static FState GState;
	static void Remove()
	{
		if (GState.W.IsValid()) if (UGameViewportClient* V = GState.VC.Get()) V->RemoveViewportWidgetContent(GState.W.ToSharedRef());
		GState.W.Reset();
		if (GState.Tick.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(GState.Tick);
		GState.Tick.Reset();
	}
}

namespace ZoogiP5
{
	bool CutInFor(const FString& ZoogiName)
	{
		if (!MenuOn()) return false;
		static const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CutInAll"));
		return bAll || ZoogiName.Equals(TEXT("Blue"), ESearchCase::IgnoreCase);
	}
	FLinearColor ZoogiElementColor(const FString& ZoogiName)
	{
		const FString N = ZoogiName.ToLower();
		if (N == TEXT("blue")) return ElementColor(TEXT("Frost"));
		if (N == TEXT("hotstreak")) return ElementColor(TEXT("Fire"));
		if (N == TEXT("bolt")) return ElementColor(TEXT("Shock"));
		if (N == TEXT("nightshade") || N == TEXT("dark zoogy")) return ElementColor(TEXT("Void"));
		return Accent();
	}
	UTexture2D* Portrait(const FString& ZoogiName)
	{
		static TMap<FString, UTexture2D*> Cache;
		FString Key = ZoogiName.ToLower().Replace(TEXT(" "), TEXT("_"));
		if (Key == TEXT("orin")) Key = TEXT("master_orin");
		if (Key == TEXT("darkzoogy")) Key = TEXT("dark_zoogy");
		if (UTexture2D** Found = Cache.Find(Key)) return *Found;
		UTexture2D* T = nullptr;
		// Oct 30 (Evan: "stop using the old avi ... don't let me see the old avi again anywhere"): the new head art, transparent,
		// in Data/MenuArt/Faces (Frosti / Lumi / Vex / Hotstreak from Evan; the rest are recolours of those heads for now)
		for (const TCHAR* Dir : { TEXT("Data/MenuArt/Faces"), TEXT("Data/MenuArt") })
			for (const TCHAR* E : { TEXT(".png"), TEXT(".jpg") })
			{
				const FString P = FPaths::ProjectContentDir() / Dir / (Key + E);
				if (!T && FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
			}
		if (T) T->AddToRoot();
		Cache.Add(Key, T);
		return T;
	}
	bool CutInBusy() { return ZoogiP5Cut::GState.W.IsValid(); }
	void StopCutIn() { ZoogiP5Cut::Remove(); }
	void PlayCutIn(UWorld* World, const FCutIn& C, float Length)
	{
		using namespace ZoogiP5Cut;
		UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
		Remove();
		if (!VC) return;
		GState.W = SNew(SCutIn);
		GState.W->C = C;
		GState.W->Len = FMath::Max(0.3f, Length);
		GState.W->SetVisibility(EVisibility::HitTestInvisible);
		GState.VC = VC;
		GState.Ctx = World;
		GState.Step = 0;
		GState.bClack = false;
		VC->AddViewportWidgetContent(GState.W.ToSharedRef(), 94);
		ZoogiUtil::PlaySfx(World, TEXT("Dash"), 0.85f, 1.3f);
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CritTest"));
		GState.Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt)
		{
			if (!GState.W.IsValid()) return false;
			GState.W->T += bTest ? 1.f / 24.f : FMath::Min(Dt, 0.05f);
			++GState.Step;
			if (!GState.bClack && GState.W->Clock() >= 0.16f)
			{
				GState.bClack = true;
				if (UObject* Cx = GState.Ctx.Get()) ZoogiUtil::PlaySfx(Cx, TEXT("Clack"), 1.f, 0.85f);
			}
			if (bTest && GState.W->C.Shot && (GState.Step % 2 == 1) && GState.Shots < 20) GState.W->C.Shot(FString::Printf(TEXT("p5crit_%02d"), ++GState.Shots));
			if (GState.W->T >= GState.W->Len) { Remove(); return false; }
			return true;
		}));
		UE_LOG(LogZoogi, Log, TEXT("P5 CUT-IN start %s \"%s\" (%.2fs, face %s)"), *C.Name, *C.Shout, Length, C.Face.IsValid() ? *C.Face->GetName() : TEXT("none"));
	}
}

// ------------------------------------------------------------------------------------------------ the menu
class SZoogiP5Menu : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiP5Menu) {}
	SLATE_END_ARGS()
	FMenuSpec Spec;
	int32 Sel = 0;
	float T = 0.f, SelT = -10.f, ConfirmT = -10.f;
	bool bTest = false, bConfirmed = false;
	int32 Frame = 0;
	mutable TArray<FBox2D> ItemRects;
	mutable FBox2D ArtRect = FBox2D(ForceInit), BackRect = FBox2D(ForceInit);
	int32 PrIdx = -3; float PrT = -10.f; // b156: the pressed bar squashes
	mutable float S = 1.f, DW = 1920.f, DH = 1080.f;
	mutable FVector2D Off = FVector2D::ZeroVector;
	mutable bool bPortrait = false;

	void Construct(const FArguments&, const FMenuSpec& InSpec)
	{
		Spec = InSpec;
		Sel = FMath::Clamp(Spec.StartSel, 0, FMath::Max(0, Spec.Items.Num() - 1));
		SelT = 0.25f;
		bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5MenuTest"));
		SetCanTick(true);
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual bool ComputeVolatility() const override { return true; }

	void Sfx(const TCHAR* N, float V = 0.8f, float P = 1.f) const { if (UObject* C = Spec.Ctx.Get()) ZoogiUtil::PlaySfx(C, N, V, P); }
	void Select(int32 I)
	{
		if (I == Sel || !Spec.Items.IsValidIndex(I)) return;
		Sel = I; SelT = T;
		Sfx(TEXT("UiClick"), 0.7f, 1.1f + 0.04f * I);
	}
	void Confirm()
	{
		if (bConfirmed || !Spec.Items.IsValidIndex(Sel) || WipeBusy()) return;
		bConfirmed = true; ConfirmT = T;
		Sfx(TEXT("UiConfirm"), 0.9f, 1.f);
		UE_LOG(LogZoogi, Log, TEXT("P5 MENU confirm %s"), *Spec.Items[Sel].Name);
		TFunction<void()> Fn = Spec.Items[Sel].Fn;
		UWorld* W = Spec.Ctx.IsValid() ? Spec.Ctx->GetWorld() : nullptr;
		PlayWipe(W, [Fn]() { if (Fn) Fn(); }, Spec.Items[Sel].Col, Spec.bMapList ? TEXT("SHOWTIME!") : TEXT("LET'S ROLL!"));
	}
	FVector2D ToDesign(const FGeometry& G, const FVector2D& Screen) const { return (G.AbsoluteToLocal(Screen) - Off) / FMath::Max(S, 0.01f); }
	FReply Press(const FGeometry& G, const FVector2D& Screen)
	{
		const FVector2D D = ToDesign(G, Screen);
		auto Buzz = [this]() { if (UObject* C = Spec.Ctx.Get()) ZoogiHaptics::Kick(0.25f, C->GetWorld(), TEXT("ui_tap")); }; // b156
		if (BackRect.bIsValid && BackRect.IsInside(D)) { PrIdx = -2; PrT = T; Buzz(); Sfx(TEXT("UiCancel"), 0.8f); if (Spec.Back) { TFunction<void()> B = Spec.Back; UWorld* W = Spec.Ctx.IsValid() ? Spec.Ctx->GetWorld() : nullptr; if (Spec.bMapList) PlayWipe(W, [B]() { B(); }, Accent(), TEXT("BACK!")); else B(); } return FReply::Handled(); }
		for (int32 i = 0; i < ItemRects.Num(); ++i)
			if (ItemRects[i].bIsValid && ItemRects[i].IsInside(D))
			{
				PrIdx = i; PrT = T; Buzz();
				if (i == Sel && T - SelT > 0.12f) Confirm(); else Select(i);
				return FReply::Handled();
			}
		if (ArtRect.bIsValid && ArtRect.IsInside(D)) { Confirm(); return FReply::Handled(); }
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Handled() : Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { return Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
	{
		if (E.IsTouchEvent()) return FReply::Unhandled();
		const FVector2D D = ToDesign(G, E.GetScreenSpacePosition());
		for (int32 i = 0; i < ItemRects.Num(); ++i) if (ItemRects[i].bIsValid && ItemRects[i].IsInside(D)) { Select(i); break; }
		return FReply::Unhandled();
	}

	virtual void Tick(const FGeometry& G, const double Now, const float Dt) override
	{
		T += bTest ? 1.f / 24.f : FMath::Min(Dt, 0.05f);
		if (!bTest || !Spec.Shot) return;
		// scripted review run: frames every other step + key moments, then select / confirm like a player would
		++Frame;
		const FString Pre = Spec.bMapList ? TEXT("p5_map_") : TEXT("p5_mode_");
		if (Frame % 2 == 0 && T < 4.2f) Spec.Shot(FString::Printf(TEXT("%sf%03d"), *Pre, Frame));
		if (!Spec.bMapList)
		{
			static const bool bAllModes = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5AllModes")); // b170: one settled shot of every mode card, no confirm
			if (bAllModes)
			{
				for (int32 i = 0; i < Spec.Items.Num(); ++i)
				{
					if (FMath::IsNearlyEqual(T, 1.0f + 0.8f * i, 0.021f)) Select(i);
					if (FMath::IsNearlyEqual(T, 1.6f + 0.8f * i, 0.021f)) Spec.Shot(FString::Printf(TEXT("p5_allmode_%d"), i));
				}
				return;
			}
			if (FMath::IsNearlyEqual(T, 1.5f, 0.021f)) Select(1);
			if (FMath::IsNearlyEqual(T, 2.2f, 0.021f)) Select(2);
			if (FMath::IsNearlyEqual(T, 3.0f, 0.021f)) Confirm(); // CLASSIC -> wipe -> MAP SELECT
		}
		else if (FMath::IsNearlyEqual(T, 1.6f, 0.021f)) Select(3);
	}

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 L0, const FWidgetStyle&, bool) const override
	{
		if (ZoogiWallCine::Active() || ZoogiWallCine::BannerUp()) { return L0; } // b232: the wall-smash cinematic plays on a clean screen
		const FVector2D Px = Geo.GetLocalSize();
		bPortrait = Px.Y > Px.X;
		S = bPortrait ? Px.X / 1080.f : Px.Y / 1080.f;
		DW = Px.X / S; DH = Px.Y / S;
		Off = FVector2D::ZeroVector;
		const FGeometry G = Geo.MakeChild(FVector2D(DW, DH), FSlateLayoutTransform(S, Off));
		const FLinearColor Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White;
		const FLinearColor SelCol = Spec.Items.IsValidIndex(Sel) ? Spec.Items[Sel].Col : Acc;
		int32 L = L0;

		// 1) background: black, rotating accent rays, drifting halftone, sliding stripes
		Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH), 0.f, FLinearColor::FromSRGBColor(FColor(8, 6, 10)));
		const FVector2D RayC = bPortrait ? FVector2D(DW * 0.5f, DH * 0.3f) : FVector2D(DW * 0.31f, DH * 0.55f);
		{
			const int32 N = 18;
			const float Rot = T * 7.f;
			const float R = FMath::Max(DW, DH) * 1.6f;
			for (int32 k = 0; k < N; k += 2)
			{
				const float A0 = FMath::DegreesToRadians(Rot + 360.f * k / N), A1 = FMath::DegreesToRadians(Rot + 360.f * (k + 1) / N);
				Tri(Out, G, L + 1, RayC, RayC + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, RayC + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, (k % 4 == 0 ? Acc * 0.55f : Acc * 0.32f).CopyWithNewOpacity(1.f));
			}
		}
		const float Drift = FMath::Fmod(T * 30.f, 64.f);
		Tiled(Out, G, L + 2, DotsTex(), FVector2D(-64.f + Drift, -64.f + Drift * 0.5f), FVector2D(DW + 128.f, DH + 128.f), 64.f, Blk.CopyWithNewOpacity(0.35f));
		WordmarkWall(Out, G, L + 2, DW, DH, T, FLinearColor(1.f, 1.f, 1.f, 0.10f));
		{	// diagonal stripe band along the bottom, sliding
			const float Y0 = DH - (bPortrait ? 150.f : 120.f), Hb = 70.f, Sl = FMath::Fmod(T * 160.f, 120.f);
			Quad(Out, G, L + 3, FVector2D(-20.f, Y0 + 20.f), FVector2D(DW + 20.f, Y0 - 30.f), FVector2D(DW + 20.f, Y0 - 30.f + Hb), FVector2D(-20.f, Y0 + 20.f + Hb), Blk);
			for (float X = -240.f + Sl; X < DW + 120.f; X += 120.f)
				Quad(Out, G, L + 4, FVector2D(X, Y0 + 20.f - X * 50.f / DW), FVector2D(X + 50.f, Y0 + 20.f - (X + 50.f) * 50.f / DW), FVector2D(X + 20.f, Y0 + 20.f + Hb - (X + 20.f) * 50.f / DW), FVector2D(X - 30.f, Y0 + 20.f + Hb - (X - 30.f) * 50.f / DW), Acc.CopyWithNewOpacity(0.9f));
		}
		L += 6;

		// b120 (Evan: "map pictures - resize for portrait, fill screen"): portrait map select = the selected map fills the screen
		UTexture2D* FullArt = (bPortrait && Spec.bMapList && Spec.Items.IsValidIndex(Sel)) ? Spec.Items[Sel].ArtFull.Get() : nullptr;
		const int32 NFull = Spec.Items.Num();
		const float StepFull = FMath::Clamp(DH * 0.5f / FMath::Max(1, NFull), 84.f, 122.f);
		const float ListBottomFull = DH - 300.f, Y0Full = ListBottomFull - (NFull - 1) * StepFull;
		if (FullArt)
		{
			const float K = FMath::Clamp((T - SelT) / 0.28f, 0.f, 1.f);
			const float Zm = 1.f + 0.07f * FMath::Square(1.f - K);
			TexCover(Out, G, L, FullArt, FVector2D(DW * 0.5f, DH * 0.5f), FVector2D(DW, DH) * Zm, Wht);
			if (K < 1.f) Box(Out, G, L + 1, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH), 0.f, Wht.CopyWithNewOpacity(0.45f * FMath::Square(1.f - K))); // change flash
			// top band (title / tags) and a gradient under the list
			for (int32 s = 0; s < 8; ++s) Box(Out, G, L + 1, FVector2D(DW * 0.5f, s * 40.f + 20.f), FVector2D(DW, 40.f), 0.f, Blk.CopyWithNewOpacity(0.5f * (1.f - s / 8.f)));
			const float G0 = Y0Full - 260.f;
			for (int32 s = 0; s < 16; ++s) { const float Y = G0 + (DH - G0) * s / 16.f; Box(Out, G, L + 1, FVector2D(DW * 0.5f, Y + (DH - G0) / 32.f), FVector2D(DW, (DH - G0) / 16.f + 2.f), 0.f, Blk.CopyWithNewOpacity(FMath::Min(0.86f, 0.06f + 0.06f * s))); }
			// accent edge stripes (P5 frame)
			Box(Out, G, L + 2, FVector2D(10.f, DH * 0.5f), FVector2D(20.f, DH), 0.f, SelCol);
			Box(Out, G, L + 2, FVector2D(DW - 10.f, DH * 0.5f), FVector2D(20.f, DH), 0.f, SelCol);
			ArtRect = FBox2D(FVector2D(0.f, 300.f), FVector2D(DW, G0));
			if (!Spec.Items[Sel].Sub.IsEmpty())
			{
				const FSlateFontInfo SF = Font(0, 36.f);
				const FVector2D SS = Measure(Spec.Items[Sel].Sub, SF);
				const FVector2D Cc(70.f + SS.X * 0.5f + 20.f, Y0Full - 150.f);
				Box(Out, G, L + 3, Cc + FVector2D(7.f, 7.f), SS + FVector2D(40.f, 16.f), -4.f, SelCol);
				Box(Out, G, L + 4, Cc, SS + FVector2D(40.f, 16.f), -4.f, Blk);
				Text(Out, G, L + 5, Spec.Items[Sel].Sub, SF, Cc, -4.f, Wht);
			}
			if (!bConfirmed)
			{
				const float P = 1.f + 0.08f * FMath::Sin(T * 10.f);
				const FVector2D Gc(DW - 170.f, Y0Full - 190.f);
				Tex(Out, G, L + 3, BurstTex(), Gc, FVector2D(190.f, 190.f) * P, T * 40.f, Blk);
				Tex(Out, G, L + 4, BurstTex(), Gc, FVector2D(170.f, 170.f) * P, T * 40.f, Acc);
				Text(Out, G, L + 5, TEXT("GO!"), Font(3, 70.f * P, 3.f), Gc, -12.f, Wht);
			}
			L += 6;
		}

		// 2) the selected item's art: tilted torn-edge frame slamming in on every change, accent burst behind it
		if (!FullArt)
		{
			const float K = FMath::Clamp((T - SelT) / 0.2f, 0.f, 1.f);
			const float Slam = 1.f + 0.32f * (1.f - EaseOutBack(K));
			const FVector2D Ctr = bPortrait ? FVector2D(DW * 0.5f, 560.f) : FVector2D(DW * 0.31f, 575.f);
			bool bWideArt = false;
			FVector2D ASz0 = bPortrait ? FVector2D(600.f, 638.f) : FVector2D(560.f, 595.f);
			if (Spec.Items.IsValidIndex(Sel)) if (UTexture2D* A0 = Spec.Items[Sel].Art.Get())
			{	// b170 (Evan Nov 7: "make it look like the example"): wide art (the new mode cards) fills a wide frame instead of being squeezed
				const float Ar = (float)A0->GetSizeX() / FMath::Max(1, A0->GetSizeY());
				if (Ar > 1.05f) { const float Wd = bPortrait ? 720.f : 680.f; ASz0 = FVector2D(Wd, Wd / Ar); bWideArt = true; } // b170: Evan's examples - a slightly wide frame (~1.13)
			}
			const FVector2D ASz = ASz0 * Slam;
			const float Ang = (bWideArt ? -1.5f : -5.f) + 7.f * FMath::Square(1.f - K) + (bWideArt ? 0.3f : 0.6f) * FMath::Sin(T * 2.1f); // b170: the mode cards sit nearly straight (Evan's examples)
			const float In = EaseOutBack((T - 0.05f) / 0.3f);
			const FVector2D C2 = Ctr + FVector2D(-(1.f - In) * 900.f, bWideArt && !bPortrait ? 20.f : 0.f);
			Tex(Out, G, L, BurstTex(), C2 + FVector2D(30.f, 10.f), FVector2D(ASz.Y, ASz.Y) * (1.45f + 0.04f * FMath::Sin(T * 9.f)), -T * 25.f, SelCol);
			Tex(Out, G, L + 1, BurstTex(), C2 + FVector2D(-20.f, 30.f), FVector2D(ASz.Y, ASz.Y) * 1.2f, T * 18.f, Wht);
			Box(Out, G, L + 2, C2 + FVector2D(22.f, 26.f), ASz + FVector2D(40.f, 40.f), Ang, Blk);                 // drop block
			Box(Out, G, L + 3, C2, ASz + FVector2D(36.f, 36.f), Ang, Wht);                                        // white frame
			if (Spec.Items.IsValidIndex(Sel)) if (UTexture2D* A = Spec.Items[Sel].Art.Get()) Tex(Out, G, L + 4, A, C2, ASz, Ang, Wht);
			// torn strips along the frame's top and bottom edges
			const float R = FMath::DegreesToRadians(Ang);
			const FVector2D Up(FMath::Sin(R), -FMath::Cos(R));
			if (!bWideArt) Tex(Out, G, L + 5, TornTex(), C2 - Up * (ASz.Y * 0.5f + 6.f), FVector2D(ASz.X + 60.f, 70.f), Ang + 180.f, Wht); // b170: (this one sits on the BOTTOM edge) the mode cards keep a clean bottom so the yellow title strip shows (Evan's examples)
			Tex(Out, G, L + 5, TornTex(), C2 + Up * (ASz.Y * 0.5f + (bWideArt ? 40.f : 6.f)), FVector2D(ASz.X + 60.f, bWideArt ? 44.f : 70.f), Ang, Blk); // (top edge) b170: above the picture, never over it (Evan: the whole image visible)
			ArtRect = FBox2D(C2 - ASz * 0.5f, C2 + ASz * 0.5f);
			if (Spec.Items.IsValidIndex(Sel) && !Spec.Items[Sel].Sub.IsEmpty())
			{	// caption tag under the art: what this mode is
				const FSlateFontInfo SF = Font(0, 34.f);
				const FVector2D SS = Measure(Spec.Items[Sel].Sub, SF);
				const FVector2D Cc = C2 + FVector2D(-ASz.X * (bWideArt ? 0.08f : 0.12f), ASz.Y * 0.5f + (bWideArt ? 46.f : 52.f));
				if (!bWideArt) Box(Out, G, L + 6, Cc + FVector2D(7.f, 7.f), SS + FVector2D(40.f, 16.f), Ang - 2.f, SelCol);
				Box(Out, G, L + 7, Cc, SS + FVector2D(40.f, 16.f), Ang - 2.f, Blk);
				Text(Out, G, L + 8, Spec.Items[Sel].Sub, SF, Cc, Ang - 2.f, Wht);
			}
			if (!bConfirmed && Spec.Items.IsValidIndex(Sel))
			{	// "GO!" sticker that pulses on the art
				const float P = 1.f + 0.08f * FMath::Sin(T * 10.f);
				const FVector2D Gc = C2 + (bWideArt ? FVector2D(ASz.X * 0.45f, ASz.Y * 0.5f + 34.f) : FVector2D(ASz.X * 0.38f, ASz.Y * 0.4f)); // b170: below the picture, beside the strip
				const float Gs = bWideArt ? 1.3f : 1.f; // b170: Evan's examples - a bigger GO! on the corner
				Tex(Out, G, L + 6, BurstTex(), Gc, FVector2D(170.f, 170.f) * P * Gs, T * 40.f, Blk);
				Tex(Out, G, L + 7, BurstTex(), Gc, FVector2D(150.f, 150.f) * P * Gs, T * 40.f, Acc);
				Text(Out, G, L + 8, TEXT("GO!"), Font(3, 62.f * P * Gs, 3.f), Gc, -12.f, Wht);
			}
			L += 9;
		}

		// 3) title: ransom-note, slamming in letter by letter
		{
			const FVector2D TP = bPortrait ? FVector2D(56.f, 120.f) : FVector2D(64.f, 100.f);
			const float Wa = Ransom(Out, G, L, Spec.TitleA, TP.X, TP.Y, 74.f, -7.f, 11u, T - 0.05f, Acc);
			Ransom(Out, G, L, Spec.TitleB, TP.X + Wa * 0.3f, TP.Y + 112.f, 64.f, -7.f, 23u, T - 0.05f - 0.035f * Spec.TitleA.Len(), Acc);
			L += 4;
		}

		// 4) the list: stacked tilted bars whipping in from off-screen; the selected one flips to white with a jittering accent shape
		ItemRects.SetNum(Spec.Items.Num());
		{
			const int32 N = Spec.Items.Num();
			const float Step = FullArt ? StepFull : bPortrait ? FMath::Min(150.f, (DH - 1080.f - 260.f) / FMath::Max(1, N)) : FMath::Min(132.f, (N > 8 ? 620.f : 680.f) / FMath::Max(1, N)); // Oct 31: 9-map list leaves room for the hint
			const float X0 = bPortrait ? 90.f : DW * 0.535f, Y0 = FullArt ? Y0Full : bPortrait ? 1010.f : 240.f; // b120: full-screen picture -> the list in the lower half
			const float BW = bPortrait ? 860.f : FMath::Min(720.f, DW - X0 - 60.f), BH = FMath::Min(108.f, Step - 14.f);
			for (int32 i = 0; i < N; ++i)
			{
				const bool bSel = i == Sel;
				const float Kp = PrIdx == i ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PrT) / 0.22f, 0.f, 1.f) : 1.f; // b156: press squash
				const float In = EaseOutBack((T - 0.15f - 0.055f * i) / 0.3f);
				const float Xo = (1.f - In) * 1500.f;
				const float Ang = (Spec.bMapList ? -6.f : -2.5f) + (bSel ? (Spec.bMapList ? 1.2f : 0.5f) * FMath::Sin(T * 23.f) : 0.f); // b170: mode list flatter (Evan's examples)
				const FVector2D Ctr(X0 + BW * 0.5f + i * 24.f + Xo + (bSel ? 30.f : 0.f), Y0 + i * Step);
				ItemRects[i] = FBox2D(Ctr - FVector2D(BW * 0.5f, BH * 0.5f + 6.f), Ctr + FVector2D(BW * 0.5f, BH * 0.5f + 6.f));
				const FLinearColor RowCol = Spec.bMapList ? SelCol : FLinearColor::FromSRGBColor(FColor(110, 70, 255)); // b170: purple ribbon on every mode (Evan's examples)
				if (bSel)
				{
					const FVector2D J(5.f * FMath::Sin(T * 37.f), 4.f * FMath::Cos(T * 29.f));
					Box(Out, G, L, Ctr + J + FVector2D(18.f, 14.f), FVector2D(BW + 60.f, BH + 34.f) * Kp, Ang - 3.f, RowCol);              // jittering colour slab
					Tri(Out, G, L, Ctr + J + FVector2D(-BW * 0.5f - 70.f, -10.f), Ctr + J + FVector2D(-BW * 0.5f + 10.f, -BH * 0.7f), Ctr + J + FVector2D(-BW * 0.5f + 10.f, BH * 0.6f), RowCol);
					Tri(Out, G, L, Ctr + J + FVector2D(BW * 0.5f + 90.f, 16.f), Ctr + J + FVector2D(BW * 0.5f - 10.f, -BH * 0.6f), Ctr + J + FVector2D(BW * 0.5f - 10.f, BH * 0.7f), RowCol);
					Box(Out, G, L + 1, Ctr + FVector2D(10.f, 10.f), FVector2D(BW, BH) * Kp, Ang, Blk);
					Box(Out, G, L + 2, Ctr, FVector2D(BW, BH) * Kp, Ang, Wht);
				}
				else
				{
					Box(Out, G, L + 1, Ctr + FVector2D(9.f, 9.f), FVector2D(BW, BH) * Kp, Ang, Acc.CopyWithNewOpacity(0.85f));
					Box(Out, G, L + 2, Ctr, FVector2D(BW, BH) * Kp, Ang, Blk);
				}
				// index tab + name
				const float R = FMath::DegreesToRadians(Ang);
				const FVector2D Ax(FMath::Cos(R), FMath::Sin(R));
				const FVector2D TabC = Ctr - Ax * (BW * 0.5f - 46.f);
				Box(Out, G, L + 3, TabC, FVector2D(70.f, BH * 0.78f), Ang + 8.f, bSel ? Acc : Wht);
				if (Spec.bMapList) Text(Out, G, L + 4, FString::FromInt(i + 1), Font(0, BH * 0.62f), TabC, Ang + 8.f, bSel ? Wht : Blk);
				else TextItalic(Out, G, L + 4, FString::FromInt(i + 1), Font(0, BH * 0.62f), TabC, Ang + 8.f, bSel ? Wht : Blk);
				const FSlateFontInfo NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f), 0.f);
				const FString Nm = Spec.Items[i].Name;
				const FVector2D NS = Measure(Nm, NF);
				if (Spec.bMapList) Text(Out, G, L + 4, Nm, NF, TabC + Ax * (60.f + NS.X * 0.5f), Ang, bSel ? Blk : Wht);
				else TextItalic(Out, G, L + 4, Nm, NF, TabC + Ax * (60.f + NS.X * 0.5f), Ang, bSel ? Blk : Wht); // b170: italic like Evan's examples
			}
			L += 6;
			// hint under the list
			if (!bConfirmed)
			{
				const float P = 1.f + 0.05f * FMath::Sin(T * 8.f);
				const FVector2D HC(X0 + 40.f + N * 12.f, N > 8 ? Y0 + (N - 1) * Step + BH * 0.5f + 72.f : Y0 + (N - 0.5f) * Step + 44.f); // Oct 31: clear of a 9th bar
				if (T > 0.6f) Ransom(Out, G, L, TEXT("TAP AGAIN TO ROLL"), HC.X, HC.Y, 30.f * P, -4.f, 41u, 1.f, Acc);
			}
		}
		L += 4;

		// 5) BACK tag (bottom right) + gems / coins tags (top right)
		{
			const FVector2D BC(DW - 170.f, DH - (bPortrait ? 150.f : 110.f));
			const float Kb = PrIdx == -2 ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PrT) / 0.22f, 0.f, 1.f) : 1.f; // b156
			Box(Out, G, L, BC + FVector2D(8.f, 8.f), FVector2D(230.f, 78.f) * Kb, 7.f, Acc);
			Box(Out, G, L + 1, BC, FVector2D(230.f, 78.f) * Kb, 7.f, Wht);
			Text(Out, G, L + 2, TEXT("BACK"), Font(0, 54.f), BC, 7.f, Blk);
			BackRect = FBox2D(BC - FVector2D(125.f, 45.f), BC + FVector2D(125.f, 45.f));
			const struct { const TCHAR* Lbl; int32 V; } Tags[] = { { TEXT("GEMS"), Spec.Gems }, { TEXT("COINS"), Spec.Coins } };
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector2D TC(DW - 190.f - k * 270.f, bPortrait ? 64.f : 56.f);
				Box(Out, G, L, TC + FVector2D(6.f, 6.f), FVector2D(240.f, 62.f), -4.f, Acc);
				Box(Out, G, L + 1, TC, FVector2D(240.f, 62.f), -4.f, Blk);
				Text(Out, G, L + 2, Tags[k].Lbl, Font(1, 18.f), TC + FVector2D(-70.f, 0.f), -4.f, Acc);
				Text(Out, G, L + 2, FString::FromInt(Tags[k].V), Font(0, 40.f), TC + FVector2D(36.f, -2.f), -4.f, Wht);
			}
		}
		return L + 4;
	}
};

TSharedRef<SWidget> UZoogiP5MenuWidget::RebuildWidget()
{
	TSharedRef<SZoogiP5Menu> W = SNew(SZoogiP5Menu, Spec);
	MySlate = W;
	return W;
}

void UZoogiP5MenuWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MySlate.Reset();
}

// ------------------------------------------------------------------------------------------------ the results screen (brief step 3)
// Choreography (A = seconds after the FINISH! wipe uncovers the screen, ~0.3 s in):
//  A 0.00  background (winner-colour rays, halftone), the standings slab slides in, the title slams in letter by letter
//  A 0.00  the winner's face slams in from the left over a jagged burst (hard shadow), name tag at 0.18
//  A 0.45  standings bars whip in one beat apart (click each); scores count up; winner bar white + jittering colour slab + WIN!
//  A 1.25  your numbers slam in as slanted tags      A 1.60  REMATCH / CHANGE ZOOGI / MENU tags drop in
//  Tap during the entrance = skip to the end. A button = wipe with its own shout, then the action.
static uint64 GResultsPaintFrame = 0;
static const float GResD0 = 0.30f;
namespace ZoogiP5 { bool ResultsOn() { return GResultsPaintFrame != 0 && GFrameCounter <= GResultsPaintFrame + 2; } }

class SZoogiP5Results : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiP5Results) {}
	SLATE_END_ARGS()
	FResultsSpec Spec;
	float T = 0.f, PressT = -10.f;
	bool bTest = false, bConfirmed = false, bTitleSfx = false, bScoreSfx = false;
	int32 Frame = 0, Hover = -1, Pressed = -1;
	uint32 RowSfx = 0;
	mutable TArray<FBox2D> BtnRects;
	mutable float S = 1.f, DW = 1920.f, DH = 1080.f;
	mutable bool bPortrait = false;

	void Construct(const FArguments&, const FResultsSpec& InSpec)
	{
		Spec = InSpec;
		bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5ResultsTest"));
		SetCanTick(true);
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual bool ComputeVolatility() const override { return true; }

	float RowStart(int32 i) const { return GResD0 + 0.45f + 0.1f * i; }
	float CountEnd() const { return RowStart(FMath::Max(0, Spec.Rows.Num() - 1)) + 0.95f; }
	float StatStart(int32 k) const { return GResD0 + 1.25f + 0.07f * k; }
	float BtnStart(int32 b) const { return GResD0 + 1.6f + 0.06f * b; }
	float EndT() const { return GResD0 + 1.95f; }

	void Sfx(const TCHAR* N, float V = 0.8f, float P = 1.f) const { if (UObject* C = Spec.Ctx.Get()) ZoogiUtil::PlaySfx(C, N, V, P); }
	void Confirm(int32 i)
	{
		if (bConfirmed || !Spec.Buttons.IsValidIndex(i) || WipeBusy()) return;
		bConfirmed = true; Pressed = i; PressT = T;
		Sfx(TEXT("UiConfirm"), 0.9f, 1.f);
		UE_LOG(LogZoogi, Log, TEXT("P5 RESULTS %s"), *Spec.Buttons[i].Label);
		TFunction<void()> Fn = Spec.Buttons[i].Fn;
		UWorld* W = Spec.Ctx.IsValid() ? Spec.Ctx->GetWorld() : nullptr;
		PlayWipe(W, [Fn]() { if (Fn) Fn(); }, i == 0 ? Accent() : Spec.WinCol, Spec.Buttons[i].Shout);
	}
	FVector2D ToDesign(const FGeometry& G, const FVector2D& Screen) const { return G.AbsoluteToLocal(Screen) / FMath::Max(S, 0.01f); }
	FReply Press(const FGeometry& G, const FVector2D& Screen)
	{
		if (T < GResD0) return FReply::Handled();
		if (T < EndT()) { T = EndT(); RowSfx = 0xFFFFFFFFu; bScoreSfx = true; return FReply::Handled(); } // first tap skips the entrance
		const FVector2D D = ToDesign(G, Screen);
		for (int32 i = 0; i < BtnRects.Num(); ++i) if (BtnRects[i].bIsValid && BtnRects[i].IsInside(D)) { if (UObject* C = Spec.Ctx.Get()) ZoogiHaptics::Kick(0.3f, C->GetWorld(), TEXT("ui_tap")); Confirm(i); break; } // b156: haptic
		return FReply::Handled();
	}
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { return E.IsTouchEvent() ? FReply::Handled() : Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { return Press(G, E.GetScreenSpacePosition()); }
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
	{
		if (E.IsTouchEvent()) return FReply::Unhandled();
		const FVector2D D = ToDesign(G, E.GetScreenSpacePosition());
		int32 H = -1;
		for (int32 i = 0; i < BtnRects.Num(); ++i) if (BtnRects[i].bIsValid && BtnRects[i].IsInside(D)) { H = i; break; }
		if (H != Hover && H >= 0) Sfx(TEXT("UiClick"), 0.5f, 1.2f + 0.05f * H);
		Hover = H;
		return FReply::Unhandled();
	}

	virtual void Tick(const FGeometry&, const double, const float Dt) override
	{
		T += bTest ? 1.f / 24.f : FMath::Min(Dt, 0.05f);
		if (bConfirmed && !WipeBusy() && T - PressT > 1.2f) { bConfirmed = false; Pressed = -1; } // the action didn't leave this screen: buttons live again
		if (!bTitleSfx && T >= GResD0) { bTitleSfx = true; Sfx(TEXT("Clack"), 1.f, Spec.Outcome == 1 ? 0.8f : 1.f); }
		for (int32 i = 0; i < Spec.Rows.Num() && i < 31; ++i)
			if (!(RowSfx & (1u << i)) && T >= RowStart(i)) { RowSfx |= 1u << i; Sfx(TEXT("UiClick"), 0.65f, 1.f + 0.06f * i); }
		if (!bScoreSfx && T >= CountEnd()) { bScoreSfx = true; Sfx(TEXT("Clack"), 0.8f, 1.25f); }
		if (bTest && Spec.Shot)
		{
			++Frame;
			if (Frame % 2 == 0 && T < 4.4f) Spec.Shot(FString::Printf(TEXT("p5res_f%03d"), Frame));
			if (Frame == 150) Spec.Shot(TEXT("p5res_hold"));
		}
	}

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 L0, const FWidgetStyle&, bool) const override
	{
		if (ZoogiWallCine::Active() || ZoogiWallCine::BannerUp()) { return L0; } // b232: the wall-smash cinematic plays on a clean screen
		GResultsPaintFrame = GFrameCounter;
		if (T < 0.27f) return L0; // the FINISH! wipe is still sweeping over the match
		const FVector2D Px = Geo.GetLocalSize();
		bPortrait = Px.Y > Px.X;
		S = bPortrait ? Px.X / 1080.f : Px.Y / 1080.f;
		DW = Px.X / S; DH = Px.Y / S;
		const FGeometry G = Geo.MakeChild(FVector2D(DW, DH), FSlateLayoutTransform(S, FVector2D::ZeroVector));
		const FLinearColor Acc = Accent(), Blk = FLinearColor::Black, Wht = FLinearColor::White, WC = Spec.WinCol;
		const float A = T - GResD0;
		int32 L = L0;

		// 1) background: black, rays in the winner's colour (grey on a defeat), drifting halftone
		Box(Out, G, L, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH), 0.f, FLinearColor::FromSRGBColor(FColor(8, 6, 10)));
		const FVector2D HeroC = bPortrait ? FVector2D(DW * 0.5f, 590.f) : FVector2D(DW * 0.245f, 590.f);
		{
			const int32 N = 20;
			const float Rot = -T * 6.f, R = FMath::Max(DW, DH) * 1.7f;
			const bool bLost = Spec.Outcome == 1;
			const FLinearColor RA = bLost ? FLinearColor(0.2f, 0.2f, 0.25f) : WC * 0.6f, RB = bLost ? FLinearColor(0.1f, 0.1f, 0.13f) : WC * 0.33f;
			for (int32 k = 0; k < N; k += 2)
			{
				const float A0 = FMath::DegreesToRadians(Rot + 360.f * k / N), A1 = FMath::DegreesToRadians(Rot + 360.f * (k + 1) / N);
				Tri(Out, G, L + 1, HeroC, HeroC + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, HeroC + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, (k % 4 == 0 ? RA : RB).CopyWithNewOpacity(1.f));
			}
		}
		const float Drift = FMath::Fmod(T * 30.f, 64.f);
		Tiled(Out, G, L + 2, DotsTex(), FVector2D(-64.f + Drift, -64.f + Drift * 0.5f), FVector2D(DW + 128.f, DH + 128.f), 64.f, Blk.CopyWithNewOpacity(0.35f));
		WordmarkWall(Out, G, L + 2, DW, DH, T, FLinearColor(1.f, 1.f, 1.f, 0.08f)); // b46: ZOOGI ROLL wallpaper on the results too
		{	// the standings slab: a slanted black panel (accent edge) sliding in behind the list
			const float K = EaseOutBack(A / 0.25f);
			if (!bPortrait)
			{
				const float Xo = (1.f - K) * DW * 0.7f, XT = DW * 0.47f + Xo, XB = DW * 0.40f + Xo;
				Quad(Out, G, L + 3, FVector2D(XT - 26.f, -20.f), FVector2D(DW + 20.f, -20.f), FVector2D(DW + 20.f, DH + 20.f), FVector2D(XB - 26.f, DH + 20.f), Acc);
				Quad(Out, G, L + 4, FVector2D(XT, -20.f), FVector2D(DW + 20.f, -20.f), FVector2D(DW + 20.f, DH + 20.f), FVector2D(XB, DH + 20.f), Blk.CopyWithNewOpacity(0.86f));
			}
			else
			{
				const float Yo = (1.f - K) * DH * 0.5f, YL = 985.f + Yo, YR = 925.f + Yo;
				Quad(Out, G, L + 3, FVector2D(-20.f, YL - 22.f), FVector2D(DW + 20.f, YR - 22.f), FVector2D(DW + 20.f, DH + 20.f), FVector2D(-20.f, DH + 20.f), Acc);
				Quad(Out, G, L + 4, FVector2D(-20.f, YL), FVector2D(DW + 20.f, YR), FVector2D(DW + 20.f, DH + 20.f), FVector2D(-20.f, DH + 20.f), Blk.CopyWithNewOpacity(0.86f));
			}
		}
		L += 6;

		// 2) the winner: jagged bursts, the face slamming in from the left with a hard black shadow; a slow bob while it holds
		{
			const float K = EaseOutBack(A / 0.32f);
			const float HS = bPortrait ? 500.f : 600.f;
			const FVector2D C = HeroC + FVector2D(-(1.f - K) * 1100.f, 6.f * FMath::Sin(T * 2.4f));
			const float Ang = -4.f + 1.2f * FMath::Sin(T * 1.7f);
			Tex(Out, G, L, BurstTex(), C + FVector2D(26.f, 12.f), FVector2D(HS, HS) * (1.45f + 0.04f * FMath::Sin(T * 8.f)), T * 20.f, Spec.Outcome == 1 ? FLinearColor(0.35f, 0.35f, 0.4f) : WC);
			Tex(Out, G, L + 1, BurstTex(), C + FVector2D(-18.f, 20.f), FVector2D(HS, HS) * 1.18f, -T * 14.f, Wht);
			if (UTexture2D* H = Spec.Hero.Get())
			{
				if (Spec.bHeroCutout)
				{
					Tex(Out, G, L + 2, H, C + FVector2D(24.f, 24.f), FVector2D(HS, HS), Ang, Blk);
					Tex(Out, G, L + 3, H, C, FVector2D(HS, HS), Ang, Wht);
				}
				else
				{
					const FVector2D FS = FVector2D(HS * 0.8f, HS * 0.8f * H->GetSizeY() / FMath::Max(1, H->GetSizeX()));
					Box(Out, G, L + 2, C + FVector2D(22.f, 26.f), FS + FVector2D(40.f, 40.f), Ang, Blk);
					Box(Out, G, L + 2, C, FS + FVector2D(36.f, 36.f), Ang, Wht);
					Tex(Out, G, L + 3, H, C, FS, Ang, Wht);
					const float R = FMath::DegreesToRadians(Ang);
					const FVector2D Up(FMath::Sin(R), -FMath::Cos(R));
					Tex(Out, G, L + 4, TornTex(), C - Up * (FS.Y * 0.5f + 6.f), FVector2D(FS.X + 60.f, 70.f), Ang + 180.f, Wht);
					Tex(Out, G, L + 4, TornTex(), C + Up * (FS.Y * 0.5f + 6.f), FVector2D(FS.X + 60.f, 70.f), Ang, Blk);
				}
			}
			// name (ransom) + a small tag under it
			const FVector2D NP = C + FVector2D(-HS * 0.36f, HS * 0.5f + 30.f);
			if (A > 0.18f) Ransom(Out, G, L + 5, Spec.HeroName, NP.X, NP.Y, bPortrait ? 64.f : 72.f, -6.f, 53u, A - 0.18f, Acc);
			if (!Spec.HeroTag.IsEmpty() && A > 0.32f)
			{
				const FSlateFontInfo TF = Font(1, 22.f);
				const FVector2D TS = Measure(Spec.HeroTag, TF);
				const float Sl = 1.f + 0.5f * (1.f - EaseOutBack((A - 0.32f) / 0.16f));
				const FVector2D TC = NP + FVector2D(TS.X * 0.5f + 30.f, 66.f);
				Box(Out, G, L + 9, TC + FVector2D(6.f, 6.f), (TS + FVector2D(36.f, 14.f)) * Sl, -6.f, WC);
				Box(Out, G, L + 10, TC, (TS + FVector2D(36.f, 14.f)) * Sl, -6.f, Blk);
				Text(Out, G, L + 11, Spec.HeroTag, Font(1, 22.f * Sl), TC, -6.f, Wht);
			}
			L += 12;
		}

		// 3) title (ransom, slams in letter by letter) + who won + the combo reason
		{
			const FVector2D TP = bPortrait ? FVector2D(50.f, 175.f) : FVector2D(60.f, 178.f);
			Ransom(Out, G, L, Spec.Head, TP.X, TP.Y, bPortrait ? 92.f : 96.f, -7.f, 61u, A, Acc);
			float SubY = TP.Y + 108.f;
			if (A > 0.22f && !Spec.Sub.IsEmpty())
			{
				const FSlateFontInfo SF = Font(1, 40.f);
				const FVector2D SS = Measure(Spec.Sub, SF);
				const float Sl = 1.f + 0.6f * (1.f - EaseOutBack((A - 0.22f) / 0.18f));
				const FVector2D SC(TP.X + 30.f + SS.X * 0.5f, SubY);
				Box(Out, G, L + 4, SC + FVector2D(9.f, 9.f), (SS + FVector2D(44.f, 18.f)) * Sl, -5.f, Spec.Outcome == 1 ? Acc : WC);
				Box(Out, G, L + 5, SC, (SS + FVector2D(44.f, 18.f)) * Sl, -5.f, Wht);
				Text(Out, G, L + 6, Spec.Sub, Font(1, 40.f * Sl), SC, -5.f, Blk);
			}
			if (A > 0.4f && !Spec.Reason.IsEmpty())
			{
				const FSlateFontInfo RF = Font(0, 28.f);
				const FVector2D RS = Measure(Spec.Reason, RF);
				const FVector2D RC(TP.X + 46.f + RS.X * 0.5f, SubY + 62.f);
				Box(Out, G, L + 4, RC, RS + FVector2D(28.f, 10.f), -5.f, Blk);
				Text(Out, G, L + 6, Spec.Reason, RF, RC, -5.f, Wht);
			}
			L += 8;
		}

		// 4) standings: tilted bars whipping in from the right; the winner's bar white with a jittering colour slab + WIN! sticker
		const int32 N = Spec.Rows.Num();
		const float X0 = bPortrait ? 60.f : DW * 0.485f;
		const float BW = bPortrait ? 920.f : FMath::Min(880.f, DW - X0 - 90.f);
		const float Step = bPortrait ? 118.f : 122.f, BH = Step - 22.f;
		const float Y0 = bPortrait ? 1040.f : 236.f;
		for (int32 i = 0; i < N; ++i)
		{
			if (T < RowStart(i)) continue;
			const FResultRow& R = Spec.Rows[i];
			const float In = EaseOutBack((T - RowStart(i)) / 0.28f);
			const float Ang = -5.f + (R.bWin ? 0.8f * FMath::Sin(T * 19.f) : 0.f);
			const FVector2D Ctr(X0 + BW * 0.5f + i * 18.f + (1.f - In) * 1500.f, Y0 + i * Step);
			const float Rr = FMath::DegreesToRadians(Ang);
			const FVector2D Ax(FMath::Cos(Rr), FMath::Sin(Rr)), Nr(-FMath::Sin(Rr), FMath::Cos(Rr));
			if (R.bWin)
			{
				const FVector2D J(5.f * FMath::Sin(T * 37.f), 4.f * FMath::Cos(T * 29.f));
				Box(Out, G, L, Ctr + J + FVector2D(16.f, 12.f), FVector2D(BW + 50.f, BH + 30.f), Ang - 3.f, WC);
				Tri(Out, G, L, Ctr + J + FVector2D(-BW * 0.5f - 60.f, -8.f), Ctr + J + FVector2D(-BW * 0.5f + 10.f, -BH * 0.7f), Ctr + J + FVector2D(-BW * 0.5f + 10.f, BH * 0.6f), WC);
				Box(Out, G, L + 1, Ctr + FVector2D(10.f, 10.f), FVector2D(BW, BH), Ang, Blk);
				Box(Out, G, L + 2, Ctr, FVector2D(BW, BH), Ang, Wht);
			}
			else
			{
				Box(Out, G, L + 1, Ctr + FVector2D(9.f, 9.f), FVector2D(BW, BH), Ang, Acc.CopyWithNewOpacity(0.85f));
				Box(Out, G, L + 2, Ctr, FVector2D(BW, BH), Ang, FLinearColor::FromSRGBColor(FColor(16, 14, 20)));
			}
			const FLinearColor Ink = R.bWin ? Blk : Wht;
			// place tab
			static const TCHAR* Places[] = { TEXT("1ST"), TEXT("2ND"), TEXT("3RD"), TEXT("4TH"), TEXT("5TH"), TEXT("6TH") };
			const FVector2D TabC = Ctr - Ax * (BW * 0.5f - 58.f);
			Box(Out, G, L + 3, TabC, FVector2D(100.f, BH * 0.84f), Ang + 8.f, R.bWin ? Acc : Wht);
			Text(Out, G, L + 4, Places[FMath::Min(i, 5)], Font(0, BH * 0.44f), TabC, Ang + 8.f, R.bWin ? Wht : Blk);
			// face chip in the player's team colour
			const float FSz = BH * 0.84f;
			const FVector2D FC = TabC + Ax * (58.f + FSz * 0.5f + 14.f);
			Box(Out, G, L + 3, FC + FVector2D(5.f, 5.f), FVector2D(FSz, FSz), Ang - 4.f, R.Col);
			Box(Out, G, L + 4, FC, FVector2D(FSz, FSz), Ang - 4.f, Wht);
			if (UTexture2D* F = R.Face.Get()) Tex(Out, G, L + 5, F, FC, FVector2D(FSz, FSz) * 0.9f, Ang - 4.f, Wht);
			// name + stat line
			const FVector2D NL = FC + Ax * (FSz * 0.5f + 20.f);
			const FSlateFontInfo NF = Font(1, BH * 0.34f);
			const FVector2D NS = Measure(R.Name, NF);
			Text(Out, G, L + 5, R.Name, NF, NL + Ax * (NS.X * 0.5f) - Nr * (BH * 0.15f), Ang, Ink);
			if (R.bYou)
			{
				const FVector2D YC = NL + Ax * (NS.X + 50.f) - Nr * (BH * 0.15f);
				Box(Out, G, L + 5, YC, FVector2D(70.f, 30.f), Ang + 6.f, Acc);
				Text(Out, G, L + 6, TEXT("YOU"), Font(1, 18.f), YC, Ang + 6.f, Wht);
			}
			const FSlateFontInfo LF = Font(0, BH * 0.22f);
			const FVector2D LS = Measure(R.Line, LF);
			Text(Out, G, L + 5, R.Line, LF, NL + Ax * (LS.X * 0.5f) + Nr * (BH * 0.22f), Ang, R.bWin ? FLinearColor(0.25f, 0.25f, 0.28f) : FLinearColor(0.72f, 0.72f, 0.78f));
			// score, counting up
			const float CK = FMath::Clamp((T - RowStart(i) - 0.15f) / 0.8f, 0.f, 1.f);
			const FString Sc = FText::AsNumber(FMath::RoundToInt(R.Score * (1.f - FMath::Pow(1.f - CK, 3.f)))).ToString();
			const FSlateFontInfo SF = Font(0, BH * 0.62f);
			const FVector2D SS = Measure(Sc, SF);
			Text(Out, G, L + 5, Sc, SF, Ctr + Ax * (BW * 0.5f - 34.f - SS.X * 0.5f), Ang, Ink);
			if (R.bWin && CK >= 1.f)
			{
				const float P = 1.f + 0.07f * FMath::Sin(T * 10.f);
				const FVector2D WP = Ctr + Ax * (BW * 0.5f + 6.f) - Nr * (BH * 0.62f);
				Tex(Out, G, L + 6, BurstTex(), WP, FVector2D(150.f, 150.f) * P, T * 40.f, Blk);
				Tex(Out, G, L + 7, BurstTex(), WP, FVector2D(132.f, 132.f) * P, T * 40.f, Acc);
				Text(Out, G, L + 8, TEXT("WIN!"), Font(3, 50.f * P, 3.f), WP, -12.f, Wht);
			}
		}
		L += 10;

		// 5) your numbers: slanted tags slamming in one by one
		const int32 NStat = Spec.Stats.Num();
		if (NStat > 0 && T >= StatStart(0))
		{
			const float SY = Y0 + N * Step + (bPortrait ? 50.f : 44.f);
			const float SX0 = X0 + (bPortrait ? 0.f : N * 9.f);
			const float Gp = 12.f, TW = FMath::Min(200.f, (BW - (NStat - 1) * Gp) / NStat), TH = 96.f;
			const FSlateFontInfo HF = Font(1, 20.f);
			Box(Out, G, L, FVector2D(SX0 + 90.f, SY - TH * 0.5f - 34.f), FVector2D(180.f, 30.f), -5.f, Acc);
			Text(Out, G, L + 1, TEXT("YOUR MATCH"), HF, FVector2D(SX0 + 90.f, SY - TH * 0.5f - 34.f), -5.f, Wht);
			for (int32 k = 0; k < NStat; ++k)
			{
				if (T < StatStart(k)) continue;
				const float K = EaseOutBack((T - StatStart(k)) / 0.2f);
				const float Sl = 1.f + 0.7f * (1.f - K);
				const FVector2D C(SX0 + TW * 0.5f + k * (TW + Gp), SY - k * (TW + Gp) * 0.087f);
				Box(Out, G, L, C + FVector2D(7.f, 7.f), FVector2D(TW, TH) * Sl, -5.f, Spec.Outcome == 1 ? Acc : WC);
				Box(Out, G, L + 1, C, FVector2D(TW, TH) * Sl, -5.f, Blk);
				Text(Out, G, L + 2, Spec.Stats[k].Label, Font(1, 19.f * Sl), C + FVector2D(0.f, -26.f), -5.f, Acc);
				Text(Out, G, L + 2, FString::FromInt(Spec.Stats[k].Value), Font(0, 48.f * Sl), C + FVector2D(0.f, 12.f), -5.f, Wht);
			}
		}
		L += 4;

		// 6) buttons: REMATCH (magenta), CHANGE ZOOGI (white), MENU (black, white rim); the hovered / pressed one jitters
		const int32 NB = Spec.Buttons.Num();
		BtnRects.SetNum(NB);
		{
			const float BtW = bPortrait ? 300.f : 296.f, BtH = bPortrait ? 104.f : 96.f, Gp = 22.f;
			const float RowW = NB * BtW + (NB - 1) * Gp;
			const float BX0 = bPortrait ? (DW - RowW) * 0.5f : DW - 70.f - RowW;
			const float BY = bPortrait ? FMath::Min(DH - 150.f, Y0 + N * Step + 280.f) : DH - 112.f; // portrait: right under the stat tags on tall phones
			for (int32 b = 0; b < NB; ++b)
			{
				BtnRects[b] = FBox2D(ForceInit);
				if (T < BtnStart(b)) continue;
				const FResultButton& Bt = Spec.Buttons[b];
				const float In = EaseOutBack((T - BtnStart(b)) / 0.25f);
				const bool bHot = b == Hover || b == Pressed;
				const FVector2D J = bHot ? FVector2D(4.f * FMath::Sin(T * 41.f), 3.f * FMath::Cos(T * 33.f)) : FVector2D::ZeroVector;
				const FVector2D C(BX0 + BtW * 0.5f + b * (BtW + Gp), BY + (1.f - In) * 320.f);
				const float Ang = -4.f + (bHot ? 1.5f * FMath::Sin(T * 23.f) : 0.f);
				const FLinearColor Face = Bt.Style == 0 ? Acc : (Bt.Style == 1 ? Wht : Blk);
				const FLinearColor Ink = Bt.Style == 1 ? Blk : Wht;
				Box(Out, G, L, C + FVector2D(9.f, 9.f) + J, FVector2D(BtW, BtH) * (bHot ? 1.06f : 1.f), Ang, Bt.Style == 0 ? Blk : Acc);
				if (Bt.Style == 2 || bHot) Box(Out, G, L + 1, C, FVector2D(BtW + 10.f, BtH + 10.f), Ang, Bt.Style == 2 ? Wht : WC);
				const float Kq = b == Pressed ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PressT) / 0.22f, 0.f, 1.f) : 1.f; // b156: press squash
				Box(Out, G, L + 2, C, FVector2D(BtW, BtH) * Kq, Ang, Face);
				Text(Out, G, L + 3, Bt.Label, Font(0, Bt.Label.Len() > 8 ? 40.f : 52.f), C, Ang, Ink);
				BtnRects[b] = FBox2D(C - FVector2D(BtW * 0.5f, BtH * 0.5f + 8.f), C + FVector2D(BtW * 0.5f, BtH * 0.5f + 8.f));
			}
		}
		return L + 6;
	}
};

TSharedRef<SWidget> UZoogiP5ResultsWidget::RebuildWidget()
{
	TSharedRef<SZoogiP5Results> W = SNew(SZoogiP5Results, Spec);
	MySlate = W;
	return W;
}

void UZoogiP5ResultsWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MySlate.Reset();
}

// Oct 30: STORY MODE (shares the paint helpers above)
#include "ZoogiShooter.h" // b235
#include "ZoogiStoryMode.inl"
