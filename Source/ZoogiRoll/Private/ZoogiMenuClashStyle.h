// clash-menu: shared look of the Clash-style front end (palette, chunky font, procedural raster shapes, brushes).
// Private to ZoogiMenuClash.cpp / ZoogiMenuClashShop.cpp (anonymous namespace: one copy per translation unit).
#pragma once

#include "CoreMinimal.h"
#include "ZoogiUIFx.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	FLinearColor ZCHex(const TCHAR* H) { return FLinearColor::FromSRGBColor(FColor::FromHex(H)); }
	// palette (Zoogi Roll's take on the chunky card-battler look)
	const FLinearColor ZCDark = ZCHex(TEXT("14122B"));      // every outline
	const FLinearColor ZCGold = ZCHex(TEXT("FFC531"));
	const FLinearColor ZCGoldDeep = ZCHex(TEXT("B97A0E"));
	const FLinearColor ZCYellow = ZCHex(TEXT("FFD84A"));    // BATTLE gradient top (DETAILED PASS 1)
	const FLinearColor ZCYellowBot = ZCHex(TEXT("F2A500")); // BATTLE gradient bottom
	const FLinearColor ZCBlue = ZCHex(TEXT("3D8BF0"));
	const FLinearColor ZCBlueDeep = ZCHex(TEXT("2559C4"));
	const FLinearColor ZCNavy = ZCHex(TEXT("173A86"));
	const FLinearColor ZCNavyDeep = ZCHex(TEXT("0F2A66"));
	const FLinearColor ZCPanel = ZCHex(TEXT("2F6BD0"));
	const FLinearColor ZCViolet = ZCHex(TEXT("6C3BD1"));
	const FLinearColor ZCGreen = ZCHex(TEXT("62CC34"));
	const FLinearColor ZCRed = ZCHex(TEXT("E53935"));        // close button
	const FLinearColor ZCSoft = ZCHex(TEXT("CFE3FF"));
	const FLinearColor ZCMuted = ZCHex(TEXT("9DB4DA"));
	const FLinearColor ZCCyan = ZCHex(TEXT("53D8FF"));
	// DETAILED PASS 1 style values
	const FLinearColor ZCSlot = ZCHex(TEXT("1F3A6B"));       // chest slots
	const FLinearColor ZCRoadCard = ZCHex(TEXT("2F7FE0"));   // Trophy Road cards
	const FLinearColor ZCRibGold = ZCHex(TEXT("F5B942")), ZCRibGoldEdge = ZCHex(TEXT("5A3510")); // Trophy Road ribbon
	const FLinearColor ZCRibPurple = ZCHex(TEXT("7B3FD0"));  // Battle Pass ribbon
	const FLinearColor ZCRibBlue = ZCHex(TEXT("3B82D6"));    // Arenas ribbon

	FLinearColor ZCLighten(const FLinearColor& C, float T) { return FMath::Lerp(C, FLinearColor::White, T).CopyWithNewOpacity(C.A); }
	FLinearColor ZCDarken(const FLinearColor& C, float T) { return FMath::Lerp(C, FLinearColor::Black, T).CopyWithNewOpacity(C.A); }

	enum EZCTab : int32 { ZCShop = 0, ZCCards = 1, ZCBattle = 2, ZCClan = 3, ZCLeader = 4 };
	const TCHAR* ZCTabName(int32 T)
	{
		static const TCHAR* N[] = { TEXT("SHOP"), TEXT("CARDS"), TEXT("BATTLE"), TEXT("CLAN"), TEXT("LEADERBOARD") };
		return N[FMath::Clamp(T, 0, 4)];
	}

	// ---- chunky font: the engine's own Roboto Black (Engine/Content/Slate/Fonts), thick dark outline
	TSharedPtr<const FCompositeFont> ZCComposite()
	{
		static TSharedPtr<const FCompositeFont> F;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			const FString P = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Black.ttf");
			if (FPaths::FileExists(P)) F = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Black")), P, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		}
		return F;
	}
	FSlateFontInfo ZCFont(float Size, float Outline)
	{
		TSharedPtr<const FCompositeFont> C = ZCComposite();
		FSlateFontInfo F = C.IsValid() ? FSlateFontInfo(C, Size, FName(TEXT("Black"))) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		F.OutlineSettings.OutlineSize = FMath::RoundToInt(Outline);
		F.OutlineSettings.OutlineColor = ZCDark;
		return F;
	}

	// ---- procedural icons: signed-distance raster of outlined, gradient-filled, glossed shapes (no image files)
	struct FZCShape
	{
		TArray<FVector2f> P;       // polygon (unit space)
		FVector2f C = FVector2f::ZeroVector; float R = 0.f; bool bCircle = false;
		FLinearColor A = FLinearColor::White, B = FLinearColor::White; // gradient top -> bottom
		float Edge = 3.5f;         // outline width (unit space)
		FLinearColor EdgeC = ZCDark;
		bool bGloss = true;
	};
	FZCShape ZCPoly(std::initializer_list<FVector2f> Pts, const FLinearColor& A, const FLinearColor& B, float Edge = 3.5f, bool bGloss = true)
	{
		FZCShape S; S.P = Pts; S.A = A; S.B = B; S.Edge = Edge; S.bGloss = bGloss; return S;
	}
	FZCShape ZCCircle(FVector2f C, float R, const FLinearColor& A, const FLinearColor& B, float Edge = 3.5f, bool bGloss = true)
	{
		FZCShape S; S.bCircle = true; S.C = C; S.R = R; S.A = A; S.B = B; S.Edge = Edge; S.bGloss = bGloss; return S;
	}
	FZCShape ZCRect(float X0, float Y0, float X1, float Y1, const FLinearColor& A, const FLinearColor& B, float Edge = 3.5f, bool bGloss = true)
	{
		return ZCPoly({ { X0, Y0 }, { X1, Y0 }, { X1, Y1 }, { X0, Y1 } }, A, B, Edge, bGloss);
	}
	FZCShape ZCRotRect(FVector2f C, float HalfL, float HalfW, float Deg, const FLinearColor& A, const FLinearColor& B, float Edge = 3.5f)
	{
		const float R = FMath::DegreesToRadians(Deg);
		const FVector2f U(FMath::Cos(R), FMath::Sin(R)), V(-U.Y, U.X);
		return ZCPoly({ C - U * HalfL - V * HalfW, C + U * HalfL - V * HalfW, C + U * HalfL + V * HalfW, C - U * HalfL + V * HalfW }, A, B, Edge);
	}
	float ZCSegDist(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float T = FMath::Clamp(FVector2f::DotProduct(P - A, AB) / FMath::Max(1e-6f, AB.SizeSquared()), 0.f, 1.f);
		return (P - (A + AB * T)).Size();
	}
	float ZCSigned(const FZCShape& S, const FVector2f& P)
	{
		if (S.bCircle) return (P - S.C).Size() - S.R;
		float D = TNumericLimits<float>::Max(); bool bIn = false;
		for (int32 i = 0, j = S.P.Num() - 1; i < S.P.Num(); j = i++)
		{
			D = FMath::Min(D, ZCSegDist(P, S.P[j], S.P[i]));
			const FVector2f& A = S.P[i]; const FVector2f& B = S.P[j];
			if (((A.Y > P.Y) != (B.Y > P.Y)) && (P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y + 1e-9f) + A.X)) bIn = !bIn;
		}
		return bIn ? -D : D;
	}
	ZoogiUIFx::FPixels ZCRaster(int32 W, int32 H, float UnitW, float UnitH, const TArray<FZCShape>& Shapes)
	{
		ZoogiUIFx::FPixels Out; Out.W = W; Out.H = H;
		TArray<FLinearColor> Acc; Acc.Init(FLinearColor::Transparent, W * H);
		const float Sc = W / UnitW; // px per unit
		for (const FZCShape& S : Shapes)
		{
			float MinY = 1e9f, MaxY = -1e9f;
			if (S.bCircle) { MinY = S.C.Y - S.R; MaxY = S.C.Y + S.R; }
			else for (const FVector2f& V : S.P) { MinY = FMath::Min(MinY, V.Y); MaxY = FMath::Max(MaxY, V.Y); }
			const float Hh = FMath::Max(1e-3f, MaxY - MinY);
			for (int32 y = 0; y < H; ++y)
				for (int32 x = 0; x < W; ++x)
				{
					const FVector2f P((x + 0.5f) / Sc, (y + 0.5f) * (UnitH / H));
					const float D = ZCSigned(S, P) * Sc; // px
					const float Cov = FMath::Clamp(0.5f - D, 0.f, 1.f);
					if (Cov <= 0.f) continue;
					const float T = FMath::Clamp((P.Y - MinY) / Hh, 0.f, 1.f);
					FLinearColor Fill = FMath::Lerp(S.A, S.B, T);
					if (S.bGloss) Fill = FMath::Lerp(Fill, FLinearColor::White, 0.26f * FMath::Clamp((0.46f - T) * 12.f, 0.f, 1.f));
					const float EdgeT = FMath::Clamp(-D - S.Edge * Sc + 0.5f, 0.f, 1.f); // 0 = outline, 1 = fill
					FLinearColor C = FMath::Lerp(S.EdgeC, Fill, EdgeT);
					FLinearColor& Dst = Acc[y * W + x];
					const float OA = Cov + Dst.A * (1.f - Cov);
					if (OA > 0.f)
					{
						const FLinearColor RGB = (C * Cov + Dst * (Dst.A * (1.f - Cov))) * (1.f / OA);
						Dst = FLinearColor(RGB.R, RGB.G, RGB.B, OA);
					}
				}
		}
		Out.Px.SetNumUninitialized(W * H);
		for (int32 i = 0; i < W * H; ++i) Out.Px[i] = Acc[i].ToFColor(true);
		return Out;
	}

	FSlateBrush ZCImgBrush(UTexture2D* Tex, float W, float H, float Radius, bool bCrop)
	{
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.ImageSize = FVector2D(W, H);
		if (Radius > 0.f)
		{
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(FLinearColor::Transparent), 0.f);
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		}
		if (bCrop && Tex && W > 0.f && H > 0.f)
		{
			const float TA = (float)Tex->GetSizeX() / FMath::Max(1, Tex->GetSizeY()), SA = W / H;
			if (SA > TA) { const float V = TA / SA; B.SetUVRegion(FBox2f(FVector2f(0.f, (1.f - V) * 0.5f), FVector2f(1.f, (1.f + V) * 0.5f))); }
			else { const float U = SA / TA; B.SetUVRegion(FBox2f(FVector2f((1.f - U) * 0.5f, 0.f), FVector2f((1.f + U) * 0.5f, 1.f))); }
		}
		return B;
	}

	FButtonStyle ZCClearStyle()
	{
		FSlateBrush N; N.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle St;
		St.SetNormal(N); St.SetHovered(N); St.SetPressed(N); St.SetDisabled(N);
		St.SetNormalPadding(FMargin(0.f)); St.SetPressedPadding(FMargin(0.f));
		return St;
	}

	const TCHAR* ZCArenaSlug(int32 A)
	{
		static const TCHAR* S[] = { TEXT("meadow"), TEXT("frozen"), TEXT("volcanic"), TEXT("cosmic"), TEXT("arabian"), TEXT("tomb"), TEXT("neon"), TEXT("training") };
		return S[FMath::Clamp(A, 0, (int32)UE_ARRAY_COUNT(S) - 1)];
	}

	// milestone 2: rarity frames + short durations ("3h 05m", "14m 20s", "9s")
	FLinearColor ZCRarityColor(int32 R) { return R == 2 ? ZCHex(TEXT("B45CFF")) : R == 1 ? ZCHex(TEXT("FF9A2E")) : ZCHex(TEXT("62A8F5")); }
	FString ZCDur(int64 Secs)
	{
		Secs = FMath::Max<int64>(0, Secs);
		const int64 H = Secs / 3600, M = (Secs / 60) % 60, S = Secs % 60;
		if (H > 0) return FString::Printf(TEXT("%lldh %02lldm"), H, M);
		if (M > 0) return FString::Printf(TEXT("%lldm %02llds"), M, S);
		return FString::Printf(TEXT("%llds"), S);
	}

	bool ZCClassic() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClassicMenu")); return b; }
}
