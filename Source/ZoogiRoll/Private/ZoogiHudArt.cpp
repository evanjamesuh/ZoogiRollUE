// hud-v3: see ZoogiHudArt.h
#include "ZoogiHudArt.h"
#include "ZoogiRoll.h"
#include "ZoogiUIStyle.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "HAL/PlatformApplicationMisc.h"

namespace ZoogiHudArt
{
	const FLinearColor Cyan(0.25f, 0.92f, 1.f, 1.f);
	const FLinearColor GoldText(1.f, 0.84f, 0.45f, 1.f);
	const FLinearColor Steel(0.78f, 0.84f, 0.92f, 1.f);
	const FLinearColor IceText(0.62f, 0.86f, 1.f, 1.f);

	namespace
	{
		TMap<FString, UTexture2D*>& Cache() { static TMap<FString, UTexture2D*> C; return C; }
		FString PathOf(const TCHAR* Name) { return FPaths::ProjectContentDir() / TEXT("Data/HudV3") / (FString(Name) + TEXT(".png")); }
		void Draw(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FSlateBrush& B, const FLinearColor& C)
		{
			if (S.X <= 0.5f || S.Y <= 0.5f) return;
			FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2f(S), FSlateLayoutTransform(FVector2f(P))), &B, ESlateDrawEffect::None, C);
		}
	}

	UTexture2D* Tex(const TCHAR* Name)
	{
		const FString Key(Name);
		if (UTexture2D** T = Cache().Find(Key)) return *T;
		const FString P = PathOf(Name);
		UTexture2D* T = FPaths::FileExists(P) ? FImageUtils::ImportFileAsTexture2D(P) : nullptr;
		if (T) { T->AddToRoot(); T->Filter = TF_Trilinear; T->LODGroup = TEXTUREGROUP_UI; T->AddressX = TA_Wrap; T->AddressY = TA_Wrap; T->UpdateResource(); }
		Cache().Add(Key, T);
		UE_LOG(LogZoogi, Log, TEXT("HUDV3 art %s: %s %dx%d"), Name, T ? TEXT("loaded") : TEXT("MISSING"), T ? T->GetSizeX() : 0, T ? T->GetSizeY() : 0);
		return T;
	}

	FVector2D SizeT(const TCHAR* Name)
	{
		UTexture2D* T = Tex(Name);
		return T ? FVector2D(T->GetSizeX(), T->GetSizeY()) / K : FVector2D::ZeroVector;
	}

	bool Img(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, const FLinearColor& Tint)
	{
		UTexture2D* T = Tex(Name);
		if (!T) return false;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = S; B.DrawAs = ESlateBrushDrawType::Image;
		Draw(Out, L, G, P, S, B, Tint);
		return true;
	}

	FVector2D ImgT(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, float Px, const TCHAR* Name, const FLinearColor& Tint)
	{
		const FVector2D S = SizeT(Name) * Px;
		Img(Out, L, G, P, S, Name, Tint);
		return S;
	}

	bool Nine(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, const FMargin& M, float Px, const FLinearColor& Tint)
	{
		UTexture2D* T = Tex(Name);
		if (!T) return false;
		const FVector2D TS(T->GetSizeX() / K, T->GetSizeY() / K);
		FSlateBrush B; B.SetResourceObject(T); B.DrawAs = ESlateBrushDrawType::Box;
		B.ImageSize = TS * Px;
		B.Margin = FMargin(M.Left / TS.X, M.Top / TS.Y, M.Right / TS.X, M.Bottom / TS.Y);
		// never let the margins overlap on a box smaller than the corners
		const float Sx = FMath::Min(1.f, S.X / FMath::Max(1.f, (M.Left + M.Right) * Px)), Sy = FMath::Min(1.f, S.Y / FMath::Max(1.f, (M.Top + M.Bottom) * Px));
		if (Sx < 1.f || Sy < 1.f) B.ImageSize = TS * Px * FMath::Min(Sx, Sy);
		Draw(Out, L, G, P, S, B, Tint);
		return true;
	}

	bool ImgRot(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& C, const FVector2D& S, const TCHAR* Name, float AngleRad, const FLinearColor& Tint)
	{
		UTexture2D* T = Tex(Name);
		if (!T || S.X <= 0.5f) return false;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = S; B.DrawAs = ESlateBrushDrawType::Image;
		const FSlateRenderTransform R{ FQuat2D(AngleRad) };
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(S), FSlateLayoutTransform(FVector2D(C - S * 0.5f)), R, FVector2D(0.5f, 0.5f)), &B, ESlateDrawEffect::None, Tint);
		return true;
	}

	bool Part(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, const FBox2D& Src, const FLinearColor& Tint)
	{
		UTexture2D* T = Tex(Name);
		if (!T) return false;
		const float TW = T->GetSizeX() / K, TH = T->GetSizeY() / K;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = S; B.DrawAs = ESlateBrushDrawType::Image;
		B.SetUVRegion(FBox2f(FVector2f(Src.Min.X / TW, Src.Min.Y / TH), FVector2f(Src.Max.X / TW, Src.Max.Y / TH)));
		Draw(Out, L, G, P, S, B, Tint);
		return true;
	}

	void Tiled(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const TCHAR* Name, float TileT, float Px, const FLinearColor& Tint)
	{
		UTexture2D* T = Tex(Name);
		if (!T || S.X <= 0.5f || S.Y <= 0.5f) return;
		const float Tile = FMath::Max(8.f, TileT * Px);
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(Tile, Tile); B.DrawAs = ESlateBrushDrawType::Image; B.Tiling = ESlateBrushTileType::Both;
		Draw(Out, L, G, P, S, B, Tint);
	}

	FVector2D Measure(const FString& T, const FSlateFontInfo& F)
	{
		if (!FSlateApplication::IsInitialized()) return FVector2D(T.Len() * F.Size * 0.6f, F.Size * 1.2f);
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, F);
	}

	void Text(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FString& T, const FSlateFontInfo& F, const FLinearColor& C, float AlignX, float AlignY)
	{
		if (T.IsEmpty()) return;
		FVector2D Q = P;
		if (AlignX != 0.f || AlignY != 0.f) { const FVector2D M = Measure(T, F); Q.X -= M.X * AlignX; Q.Y -= M.Y * AlignY; }
		ZoogiUIStyle::SText(Out, L, G, Q, T, F, C);
	}

	float TapPx(float Dp, float FallbackPx)
	{
		static int32 Dens = -1;
		if (Dens < 0)
		{
			int32 D = 0;
			const EScreenPhysicalAccuracy Acc = FPlatformApplicationMisc::GetPhysicalScreenDensity(D);
			Dens = Acc != EScreenPhysicalAccuracy::Unknown && D > 0 ? D : 0;
			int32 Ov = 0;
			if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiDPI="), Ov) && Ov > 0) Dens = Ov;
			UE_LOG(LogZoogi, Log, TEXT("HUDV3 tap targets: screen density %d dpi (%s) -> 48 dp = %.0f px"), Dens, Dens > 0 ? TEXT("dp scaled") : TEXT("unknown: 1080p units"), Dens > 0 ? 48.f * Dens / 160.f : 0.f);
		}
		return Dens > 0 ? FMath::Max(FallbackPx, Dp * Dens / 160.f) : FallbackPx;
	}
}
