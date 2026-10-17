// clash-menu (Oct 17, milestone 1): Clash-style front end for UZoogiMenuRoot. The Home screen becomes the Battle tab of a
// tabbed shell: persistent top bar (level badge + XP, gold, gems), row 2 (avatar, name plate + clan shield, trophies,
// hamburger), the arena column (Arena N label, name ribbon, map preview, trophy bar, big yellow BATTLE, 3 chest slots),
// Battle Pass card (left), Trophy Road (right) and a 5-tab bottom nav. Laid out for landscape in a DesignW x 1080 canvas
// inside the menu's SafeZone (notches). Every shape is drawn in code (UMG rounded boxes + procedurally rasterized icons)
// in Zoogi Roll's own art direction; the only images used are Zoogi Roll's own MenuArt (map tiles, portraits, coin, gem).
// The old Home is kept behind -ZoogiClassicMenu. Values come from UZoogiFrontProfileSubsystem (local save).
#include "ZoogiMenuRoot.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuWidget.h" // UZoogiButton (non-focusable button)
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiUIFx.h"
#include "ZoogiUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	FLinearColor ZCHex(const TCHAR* H) { return FLinearColor::FromSRGBColor(FColor::FromHex(H)); }
	// palette (Zoogi Roll's take on the chunky card-battler look)
	const FLinearColor ZCDark = ZCHex(TEXT("14122B"));      // every outline
	const FLinearColor ZCGold = ZCHex(TEXT("FFC531"));
	const FLinearColor ZCGoldDeep = ZCHex(TEXT("B97A0E"));
	const FLinearColor ZCYellow = ZCHex(TEXT("FFCB2E"));
	const FLinearColor ZCBlue = ZCHex(TEXT("3D8BF0"));
	const FLinearColor ZCBlueDeep = ZCHex(TEXT("2559C4"));
	const FLinearColor ZCNavy = ZCHex(TEXT("173A86"));
	const FLinearColor ZCNavyDeep = ZCHex(TEXT("0F2A66"));
	const FLinearColor ZCPanel = ZCHex(TEXT("2F6BD0"));
	const FLinearColor ZCViolet = ZCHex(TEXT("6C3BD1"));
	const FLinearColor ZCGreen = ZCHex(TEXT("62CC34"));
	const FLinearColor ZCRed = ZCHex(TEXT("EE3E36"));
	const FLinearColor ZCSoft = ZCHex(TEXT("CFE3FF"));
	const FLinearColor ZCMuted = ZCHex(TEXT("9DB4DA"));
	const FLinearColor ZCCyan = ZCHex(TEXT("53D8FF"));

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

	bool ZCClassic() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClassicMenu")); return b; }
}

// ------------------------------------------------------------------------------------------------ state
bool UZoogiMenuRoot::ClashOn() const { return !ZCClassic(); }

void UZoogiMenuRoot::SetClashTab(int32 T)
{
	T = FMath::Clamp(T, 0, 4);
	if (T == ZCCards)
		if (UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this))
			if (FP->Data().bCardsNew) { FP->Mutable().bCardsNew = false; FP->Save(); }
	if (ClashTab != T) UE_LOG(LogZoogi, Log, TEXT("CLASH tab %s"), ZCTabName(T));
	ClashTab = T;
	ClashPopup = EZoogiClashPopup::None;
	bDirty = true;
}

void UZoogiMenuRoot::OpenClashPopup(EZoogiClashPopup P, const FString& Title, const FString& Body)
{
	ClashPopup = P;
	ClashInfoTitle = Title;
	ClashInfoBody = Body;
	ClashPopupT0 = Time;
	UE_LOG(LogZoogi, Log, TEXT("CLASH popup %d %s"), (int32)P, *Title);
	bDirty = true;
}

// ------------------------------------------------------------------------------------------------ helpers
UWidget* UZoogiMenuRoot::CAt(UWidget* W, float X, float Y, float Wd, float Ht)
{
	if (!W || !Canvas) return W;
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W))
	{
		S->SetAnchors(FAnchors(0.f, 0.f));
		S->SetPosition(FVector2D(X, Y));
		S->SetSize(FVector2D(Wd, Ht));
	}
	return W;
}

UTextBlock* UZoogiMenuRoot::CTxt(const FString& S, float Size, const FLinearColor& C, float Outline)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(S));
	T->SetFont(ZCFont(Size, Outline));
	T->SetColorAndOpacity(FSlateColor(C));
	T->SetShadowOffset(FVector2D(0.f, Outline > 0.f ? FMath::Max(2.f, Size * 0.09f) : 0.f));
	T->SetShadowColorAndOpacity(ZCDark.CopyWithNewOpacity(Outline > 0.f ? 0.9f : 0.f));
	T->SetVisibility(ESlateVisibility::HitTestInvisible);
	return T;
}

UWidget* UZoogiMenuRoot::CText(const FString& S, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, int32 Justify, float Outline, bool bWrap)
{
	UTextBlock* T = CTxt(S, Size, C, Outline);
	T->SetJustification(Justify == 1 ? ETextJustify::Center : Justify == 2 ? ETextJustify::Right : ETextJustify::Left);
	T->SetAutoWrapText(bWrap);
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrushColor(FLinearColor::Transparent);
	B->SetPadding(FMargin(0.f));
	B->SetHorizontalAlignment(Justify == 1 ? HAlign_Center : Justify == 2 ? HAlign_Right : HAlign_Left);
	B->SetVerticalAlignment(VAlign_Center);
	B->SetContent(T);
	B->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(B, X, Y, Wd, Ht);
}

UWidget* UZoogiMenuRoot::CImg(UTexture2D* Tex, float X, float Y, float Wd, float Ht, float Radius, bool bCrop, const FLinearColor& Tint)
{
	if (!Tex) return nullptr;
	UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	I->SetBrush(ZCImgBrush(Tex, Wd, Ht, Radius, bCrop));
	I->SetColorAndOpacity(Tint);
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(I, X, Y, Wd, Ht);
}

UBorder* UZoogiMenuRoot::CBox(const FLinearColor& Fill, const FLinearColor& Edge, float EdgeW, float R)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrush(ZoogiUIStyle::RoundBrush(Fill, Edge, EdgeW, R));
	B->SetPadding(FMargin(0.f));
	B->SetVisibility(ESlateVisibility::HitTestInvisible);
	return B;
}

UWidget* UZoogiMenuRoot::CPlate(float X, float Y, float Wd, float Ht, const FLinearColor& Fill, float R, float EdgeW, bool bGloss)
{
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(Fill, ZCDark, EdgeW, R))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	if (bGloss)
	{
		USizeBox* GB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		GB->SetHeightOverride(FMath::Max(4.f, Ht * 0.4f));
		GB->AddChild(CBox(FLinearColor(1.f, 1.f, 1.f, 0.16f), FLinearColor::Transparent, 0.f, FMath::Max(2.f, R - EdgeW)));
		if (UOverlaySlot* S = O->AddChildToOverlay(GB)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(EdgeW + 4.f, EdgeW + 2.f, EdgeW + 4.f, 0.f)); }
	}
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(O, X, Y, Wd, Ht);
}

UWidget* UZoogiMenuRoot::CGoldPanel(float X, float Y, float Wd, float Ht, const FLinearColor& Inner, float R)
{
	// dark outline -> gold rim -> deep-gold inner line -> saturated inner fill with a top sheen
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	auto Layer = [&](UWidget* W, float Pad)
	{
		if (UOverlaySlot* S = O->AddChildToOverlay(W)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(Pad)); }
	};
	Layer(CBox(ZCGold, ZCDark, 5.f, R), 0.f);
	Layer(CBox(Inner, ZCGoldDeep, 3.f, FMath::Max(4.f, R - 9.f)), 9.f);
	USizeBox* GB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GB->SetHeightOverride(FMath::Clamp(Ht * 0.22f, 10.f, 70.f));
	GB->AddChild(CBox(FLinearColor(1.f, 1.f, 1.f, 0.13f), FLinearColor::Transparent, 0.f, FMath::Max(4.f, R - 12.f)));
	if (UOverlaySlot* S = O->AddChildToOverlay(GB)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(15.f, 13.f, 15.f, 0.f)); }
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(O, X, Y, Wd, Ht);
}

UButton* UZoogiMenuRoot::CButton(UWidget* Content, TFunction<void()> Fn, bool bPrimary)
{
	UButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	B->SetStyle(ZCClearStyle());
	B->SetClickMethod(EButtonClickMethod::PreciseClick);
	B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	if (Content)
	{
		B->AddChild(Content);
		if (UButtonSlot* S = Cast<UButtonSlot>(Content->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(0.f)); }
	}
	BindClick(B, MoveTemp(Fn));
	AddAnim(B, nullptr, bPrimary, false);
	return B;
}

UCanvasPanel* UZoogiMenuRoot::CGroupBegin()
{
	UCanvasPanel* G = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	ClashCanvasStack.Add(Canvas);
	Canvas = G;
	return G;
}

void UZoogiMenuRoot::CGroupEnd()
{
	if (ClashCanvasStack.Num()) Canvas = ClashCanvasStack.Pop();
}

UWidget* UZoogiMenuRoot::CChunkyW(UWidget* Inner, const FLinearColor& Face, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, bool bPrimary, float R)
{
	// chunky 3D button: dark outline, darker lip along the bottom, bright face, white gloss on the upper face
	const float Lip = FMath::Clamp(Ht * 0.11f, 5.f, 13.f);
	if (R < 0.f) R = FMath::Min(22.f, Ht * 0.3f);
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(ZCDarken(Face, 0.38f), ZCDark, 5.f, R))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(Face, FLinearColor::Transparent, 0.f, FMath::Max(3.f, R - 4.f)))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(5.f, 5.f, 5.f, 5.f + Lip)); }
	USizeBox* GB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GB->SetHeightOverride(FMath::Max(4.f, (Ht - Lip - 10.f) * 0.42f));
	GB->AddChild(CBox(FLinearColor(1.f, 1.f, 1.f, 0.30f), FLinearColor::Transparent, 0.f, FMath::Max(3.f, R - 8.f)));
	if (UOverlaySlot* S = O->AddChildToOverlay(GB)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(11.f, 8.f, 11.f, 0.f)); }
	if (Inner)
		if (UOverlaySlot* S = O->AddChildToOverlay(Inner)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(6.f, 4.f, 6.f, 4.f + Lip)); }
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(CButton(O, MoveTemp(Fn), bPrimary), X, Y, Wd, Ht);
}

UWidget* UZoogiMenuRoot::CChunky(const FString& Label, const FLinearColor& Face, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, float FontSize, UTexture2D* Icon, bool bPrimary)
{
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (Icon)
	{
		const float IS = FontSize * 1.45f;
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(Icon, IS, IS, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, Label.IsEmpty() ? 0.f : 12.f, 0.f)); }
	}
	if (!Label.IsEmpty())
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(Label, FontSize, FLinearColor::White, FMath::Clamp(FontSize * 0.11f, 3.f, 6.f)))) S->SetVerticalAlignment(VAlign_Center);
	H->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CChunkyW(H, Face, MoveTemp(Fn), X, Y, Wd, Ht, bPrimary);
}

UWidget* UZoogiMenuRoot::CBar(float X, float Y, float Wd, float Ht, float Frac, const FLinearColor& Fill, const FString& Text, float FontSize)
{
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	const float R = Ht * 0.5f;
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(ZCNavyDeep, ZCDark, 4.f, R))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Frac = FMath::Clamp(Frac, 0.f, 1.f);
	if (Frac > 0.001f)
	{
		USizeBox* FB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FB->SetWidthOverride(FMath::Max(Ht - 8.f, (Wd - 8.f) * Frac));
		UOverlay* FO = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		if (UOverlaySlot* S = FO->AddChildToOverlay(CBox(Fill, FLinearColor::Transparent, 0.f, R - 4.f))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
		USizeBox* GB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		GB->SetHeightOverride((Ht - 8.f) * 0.4f);
		GB->AddChild(CBox(FLinearColor(1.f, 1.f, 1.f, 0.3f), FLinearColor::Transparent, 0.f, R * 0.5f));
		if (UOverlaySlot* S = FO->AddChildToOverlay(GB)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(6.f, 3.f, 6.f, 0.f)); }
		FB->AddChild(FO);
		if (UOverlaySlot* S = O->AddChildToOverlay(FB)) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(4.f)); }
	}
	if (!Text.IsEmpty())
		if (UOverlaySlot* S = O->AddChildToOverlay(CTxt(Text, FontSize, FLinearColor::White, 3.f))) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(O, X, Y, Wd, Ht);
}

UWidget* UZoogiMenuRoot::CDot(float X, float Y, float S, const FString& Txt)
{
	UBorder* B = CBox(ZCRed, ZCDark, 3.f, S * 0.5f);
	B->SetHorizontalAlignment(HAlign_Center); B->SetVerticalAlignment(VAlign_Center);
	if (!Txt.IsEmpty()) B->SetContent(CTxt(Txt, S * 0.42f, FLinearColor::White, 2.f));
	return CAt(B, X, Y, S, S);
}

// ------------------------------------------------------------------------------------------------ procedural art
UTexture2D* UZoogiMenuRoot::ClashArt(const FString& Key)
{
	const FString CacheKey = TEXT("clash:") + Key;
	if (TObjectPtr<UTexture2D>* Hit = ArtCache.Find(CacheKey)) return Hit->Get();
	TArray<FZCShape> S;
	int32 W = 128, H = 128; float UW = 100.f, UH = 100.f;
	const FLinearColor GoldA = ZCHex(TEXT("FFE66B")), GoldB = ZCHex(TEXT("E39410"));
	const FLinearColor SilverA = ZCHex(TEXT("F4F8FF")), SilverB = ZCHex(TEXT("9AA9C4"));
	if (Key == TEXT("trophy"))
	{
		S.Add(ZCPoly({ { 26, 18 }, { 9, 20 }, { 11, 38 }, { 30, 48 } }, GoldA, GoldB));
		S.Add(ZCPoly({ { 74, 18 }, { 91, 20 }, { 89, 38 }, { 70, 48 } }, GoldA, GoldB));
		S.Add(ZCPoly({ { 44, 56 }, { 56, 56 }, { 58, 74 }, { 42, 74 } }, GoldA, GoldB));
		S.Add(ZCPoly({ { 28, 72 }, { 72, 72 }, { 77, 90 }, { 23, 90 } }, ZCHex(TEXT("C7781A")), ZCHex(TEXT("8C4B0E"))));
		S.Add(ZCPoly({ { 20, 10 }, { 80, 10 }, { 78, 34 }, { 69, 50 }, { 56, 60 }, { 44, 60 }, { 31, 50 }, { 22, 34 } }, GoldA, GoldB));
	}
	else if (Key == TEXT("shield") || Key == TEXT("clan"))
	{
		const bool bClan = Key == TEXT("clan");
		const FLinearColor A = bClan ? ZCHex(TEXT("7FC2FF")) : ZCHex(TEXT("B8C6DE")), B = bClan ? ZCHex(TEXT("2D62D6")) : ZCHex(TEXT("5D6F92"));
		S.Add(ZCPoly({ { 50, 6 }, { 88, 18 }, { 86, 50 }, { 50, 94 }, { 14, 50 }, { 12, 18 } }, A, B, 4.f));
		S.Add(ZCPoly({ { 50, 20 }, { 74, 28 }, { 72, 50 }, { 50, 78 }, { 28, 50 }, { 26, 28 } }, ZCLighten(B, 0.15f), ZCDarken(B, 0.2f), 2.5f, false));
		if (bClan) S.Add(ZCCircle({ 50, 46 }, 10, GoldA, GoldB, 2.5f));
	}
	else if (Key == TEXT("chest"))
	{
		const FLinearColor WoodA = ZCHex(TEXT("D2843C")), WoodB = ZCHex(TEXT("7A3F14"));
		S.Add(ZCRect(12, 46, 88, 88, WoodA, WoodB));
		S.Add(ZCPoly({ { 12, 48 }, { 12, 32 }, { 22, 20 }, { 50, 16 }, { 78, 20 }, { 88, 32 }, { 88, 48 } }, ZCLighten(WoodA, 0.1f), WoodA));
		S.Add(ZCRect(24, 18, 32, 88, GoldA, GoldB, 2.5f));
		S.Add(ZCRect(68, 18, 76, 88, GoldA, GoldB, 2.5f));
		S.Add(ZCRect(42, 40, 58, 60, GoldA, GoldB, 3.f));
	}
	else if (Key == TEXT("swords"))
	{
		S.Add(ZCRotRect({ 50, 46 }, 38, 6, 45, SilverA, SilverB));
		S.Add(ZCRotRect({ 50, 46 }, 38, 6, -45, SilverA, SilverB));
		S.Add(ZCRotRect({ 26, 72 }, 14, 5, 45 + 90, GoldA, GoldB));
		S.Add(ZCRotRect({ 74, 72 }, 14, 5, -45 + 90, GoldA, GoldB));
		S.Add(ZCCircle({ 17, 83 }, 7, ZCHex(TEXT("E9493F")), ZCHex(TEXT("A11F1C")), 3.f));
		S.Add(ZCCircle({ 83, 83 }, 7, ZCHex(TEXT("E9493F")), ZCHex(TEXT("A11F1C")), 3.f));
	}
	else if (Key == TEXT("cards"))
	{
		S.Add(ZCRotRect({ 34, 54 }, 30, 20, 90 - 18, ZCHex(TEXT("8E63F5")), ZCHex(TEXT("5226B8"))));
		S.Add(ZCRotRect({ 66, 54 }, 30, 20, 90 + 18, ZCHex(TEXT("5BC7FF")), ZCHex(TEXT("1F6FD6"))));
		S.Add(ZCRotRect({ 50, 50 }, 32, 21, 90, GoldA, GoldB));
		S.Add(ZCCircle({ 50, 50 }, 9, ZCHex(TEXT("FFFFFF")), ZCHex(TEXT("D7E6FF")), 3.f));
	}
	else if (Key == TEXT("shop"))
	{
		S.Add(ZCPoly({ { 14, 40 }, { 86, 40 }, { 80, 90 }, { 20, 90 } }, ZCHex(TEXT("6CD84A")), ZCHex(TEXT("2E8E1C"))));
		S.Add(ZCPoly({ { 8, 26 }, { 92, 26 }, { 88, 44 }, { 12, 44 } }, ZCHex(TEXT("FF6A5C")), ZCHex(TEXT("C2271F"))));
		S.Add(ZCCircle({ 50, 64 }, 15, GoldA, GoldB));
		S.Add(ZCPoly({ { 30, 26 }, { 40, 8 }, { 60, 8 }, { 70, 26 } }, ZCHex(TEXT("FFFFFF")), ZCHex(TEXT("C9D7F2")), 3.f));
	}
	else if (Key == TEXT("podium"))
	{
		S.Add(ZCRect(8, 50, 36, 90, SilverA, SilverB));
		S.Add(ZCRect(64, 60, 92, 90, ZCHex(TEXT("F0A35E")), ZCHex(TEXT("A8561D"))));
		S.Add(ZCRect(34, 32, 66, 90, GoldA, GoldB));
		S.Add(ZCPoly({ { 50, 6 }, { 56, 17 }, { 68, 18 }, { 59, 26 }, { 62, 38 }, { 50, 32 }, { 38, 38 }, { 41, 26 }, { 32, 18 }, { 44, 17 } }, GoldA, GoldB, 3.f));
	}
	else if (Key == TEXT("level"))
	{
		S.Add(ZCPoly({ { 50, 4 }, { 90, 27 }, { 90, 73 }, { 50, 96 }, { 10, 73 }, { 10, 27 } }, ZCHex(TEXT("6FD3FF")), ZCHex(TEXT("1F62D8")), 5.f));
		S.Add(ZCPoly({ { 50, 16 }, { 79, 33 }, { 79, 67 }, { 50, 84 }, { 21, 67 }, { 21, 33 } }, ZCHex(TEXT("3C8EF0")), ZCHex(TEXT("1A4CB8")), 2.5f, false));
	}
	else if (Key == TEXT("crown"))
	{
		S.Add(ZCPoly({ { 10, 72 }, { 14, 26 }, { 33, 48 }, { 50, 14 }, { 67, 48 }, { 86, 26 }, { 90, 72 } }, GoldA, GoldB, 4.f));
		S.Add(ZCRect(10, 70, 90, 88, ZCHex(TEXT("E9A520")), ZCHex(TEXT("A8650C")), 4.f));
		S.Add(ZCCircle({ 50, 79 }, 6, ZCHex(TEXT("8E63F5")), ZCHex(TEXT("5226B8")), 2.5f));
		S.Add(ZCCircle({ 14, 24 }, 6, GoldA, GoldB, 3.f));
		S.Add(ZCCircle({ 50, 12 }, 6, GoldA, GoldB, 3.f));
		S.Add(ZCCircle({ 86, 24 }, 6, GoldA, GoldB, 3.f));
	}
	else if (Key == TEXT("ribbon"))
	{
		W = 768; H = 128; UW = 640.f; UH = 106.7f;
		const FLinearColor TailA = ZCHex(TEXT("2E66D8")), TailB = ZCHex(TEXT("1A3F9C"));
		S.Add(ZCPoly({ { 4, 28 }, { 110, 28 }, { 110, 100 }, { 4, 100 }, { 34, 64 } }, TailA, TailB, 5.f, false));
		S.Add(ZCPoly({ { 636, 28 }, { 530, 28 }, { 530, 100 }, { 636, 100 }, { 606, 64 } }, TailA, TailB, 5.f, false));
		S.Add(ZCPoly({ { 80, 84 }, { 112, 84 }, { 112, 100 } }, ZCHex(TEXT("102A6E")), ZCHex(TEXT("102A6E")), 3.f, false));
		S.Add(ZCPoly({ { 560, 84 }, { 528, 84 }, { 528, 100 } }, ZCHex(TEXT("102A6E")), ZCHex(TEXT("102A6E")), 3.f, false));
		S.Add(ZCPoly({ { 72, 6 }, { 568, 6 }, { 568, 86 }, { 72, 86 } }, ZCHex(TEXT("6DB6FF")), ZCHex(TEXT("2C68DE")), 5.f));
	}
	else if (Key == TEXT("bg"))
	{
		// backdrop: bright sky blue -> royal blue with a faint diamond lattice (painted directly, no shapes)
		ZoogiUIFx::FPixels P; P.W = 512; P.H = 288; P.Px.SetNumUninitialized(P.W * P.H);
		const FLinearColor Top = ZCHex(TEXT("5AB0F7")), Mid = ZCHex(TEXT("3577E0")), Bot = ZCHex(TEXT("1D47AE"));
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float T = (float)y / (P.H - 1);
				FLinearColor C = T < 0.55f ? FMath::Lerp(Top, Mid, T / 0.55f) : FMath::Lerp(Mid, Bot, (T - 0.55f) / 0.45f);
				const float Cell = 48.f;
				const float U = FMath::Fmod(x + y, Cell) / Cell, V = FMath::Fmod(x - y + 4096.f, Cell) / Cell;
				const float Line = FMath::Min(FMath::Min(U, 1.f - U), FMath::Min(V, 1.f - V));
				C = FMath::Lerp(C, FLinearColor::White, Line < 0.035f ? 0.07f : 0.f);
				const float Vig = FMath::Clamp(FVector2f((x - P.W * 0.5f) / P.W, (y - P.H * 0.42f) / P.H).Size() * 1.1f, 0.f, 1.f);
				C = FMath::Lerp(C, ZCHex(TEXT("142F80")), Vig * Vig * 0.55f);
				P.Px[y * P.W + x] = C.ToFColor(true);
			}
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		ArtCache.Add(CacheKey, T);
		return T;
	}
	UTexture2D* T = S.Num() ? ZoogiUIFx::ToTexture(ZCRaster(W, H, UW, UH, S)) : nullptr;
	ArtCache.Add(CacheKey, T);
	return T;
}

// ------------------------------------------------------------------------------------------------ build
void UZoogiMenuRoot::BuildClashBackdrop()
{
	if (UTexture2D* Bg = ClashArt(TEXT("bg")))
	{
		UScaleBox* Fill = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Fill->SetStretch(EStretch::ScaleToFill);
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush B; B.SetResourceObject(Bg); B.ImageSize = FVector2D(Bg->GetSizeX(), Bg->GetSizeY());
		I->SetBrush(B);
		Fill->AddChild(I);
		Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* S = Root->AddChildToOverlay(Fill)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	}
}

void UZoogiMenuRoot::BuildClash()
{
	const bool bTabSwitch = ClashBuiltTab != -1 && ClashBuiltTab != ClashTab;
	const bool bEnter = bAnimEnter || bTabSwitch;
	ClashBuiltTab = ClashTab;
	// top bar + nav pop in only when the screen is entered, tab content pops on every tab switch
	bAnimEnter = bAnimEnter && !bTabSwitch;
	BuildClashTopBar();
	BuildClashNav();
	bAnimEnter = bEnter;
	switch (ClashTab)
	{
	case ZCBattle: BuildClashBattleTab(); break;
	case ZCShop: BuildClashEmptyTab(TEXT("SHOP"), TEXT("Daily deals, chests and gold offers arrive in a later milestone.\nGem packs (real money) need the store backend."), false); break;
	case ZCCards: BuildClashEmptyTab(TEXT("CARDS"), TEXT("Card collection and upgrades arrive in a later milestone.\nYour current battle deck is one tap away:"), true); break;
	case ZCClan: BuildClashEmptyTab(TEXT("CLAN"), TEXT("Clans, chat and donations need the online backend."), false); break;
	default: BuildClashEmptyTab(TEXT("LEADERBOARD"), TEXT("Global and friends leaderboards need the online backend."), false); break;
	}
	bAnimEnter = false;
	if (ClashPopup != EZoogiClashPopup::None) BuildClashPopup();
	else { ClashPopupW.Reset(); ClashPopupScrim.Reset(); }
}

void UZoogiMenuRoot::BuildClashTopBar()
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	const float W = DesignW;
	const float M = 36.f;

	// ---- row 1 left: level badge over the XP bar (badge opens Profile)
	const int32 Need = UZoogiFrontProfileSubsystem::XPForLevel(D.Level);
	CBar(M + 58.f, 30.f, 300.f, 46.f, Need > 0 ? float(D.XP) / Need : 0.f, ZCCyan, FString::Printf(TEXT("%d/%d"), D.XP, Need), 24.f);
	{
		UCanvasPanel* G = CGroupBegin();
		CImg(ClashArt(TEXT("level")), 0.f, 0.f, 88.f, 88.f);
		CText(FString::FromInt(D.Level), 0.f, 0.f, 88.f, 84.f, 36.f, FLinearColor::White);
		CGroupEnd();
		CAt(CButton(G, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); }), M, 9.f, 88.f, 88.f);
	}

	// ---- row 1 right: gold + gems pills, each with a green '+' (both jump to the Shop tab)
	auto Pill = [this](float X, UTexture2D* Icon, int32 Value, TFunction<void()> Plus)
	{
		const float PW = 260.f, PH = 56.f, Y = 24.f;
		CPlate(X, Y, PW, PH, ZCNavyDeep.CopyWithNewOpacity(0.88f), PH * 0.5f, 4.f, false);
		CText(FString::FromInt(Value), X + 54.f, Y, PW - 54.f - 66.f, PH, 30.f, FLinearColor::White, 2);
		CImg(Icon, X - 22.f, Y - 9.f, 74.f, 74.f);
		CChunky(TEXT("+"), ZCGreen, MoveTemp(Plus), X + PW - 60.f, Y - 1.f, 58.f, 58.f, 38.f);
	};
	const float GemX = W - M - 260.f, GoldX = GemX - 40.f - 260.f;
	Pill(GoldX, Art(TEXT("icon_coin")), D.Gold, [this]() { SetClashTab(ZCShop); });
	Pill(GemX, Art(TEXT("icon_gem")), D.Gems, [this]() { SetClashTab(ZCShop); });

	if (ClashTab != ZCBattle) return; // row 2 belongs to the Battle (home) tab

	// ---- row 2 left: round avatar (Profile), name plate + clan shield
	const float Y2 = 108.f;
	{
		AZoogiGameMode* GMode = GM();
		const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
		UTexture2D* Portrait = nullptr;
		if (GMode && GMode->MenuPlayers.Num() > 0 && Roster.Num() > 0) Portrait = Art(PortraitFor(Roster[GMode->MenuPlayers[0].ZoogiIndex % Roster.Num()].Id));
		UCanvasPanel* G = CGroupBegin();
		CAt(CBox(ZCGold, ZCDark, 5.f, 50.f), 0.f, 0.f, 100.f, 100.f);
		CAt(CBox(ZCBlueDeep, ZCGoldDeep, 3.f, 41.f), 9.f, 9.f, 82.f, 82.f);
		CImg(Portrait, 11.f, 11.f, 78.f, 78.f, 39.f, true);
		CGroupEnd();
		CAt(CButton(G, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); }), M, Y2 - 6.f, 100.f, 100.f);
	}
	{
		UCanvasPanel* G = CGroupBegin();
		CPlate(0.f, 0.f, 380.f, 84.f, ZCNavy.CopyWithNewOpacity(0.92f), 18.f, 4.f, true);
		CText(D.GuestName, 20.f, 6.f, 280.f, 42.f, 28.f, FLinearColor::White, 0);
		CText(TEXT("NO CLAN"), 20.f, 44.f, 280.f, 32.f, 20.f, ZCMuted, 0, 3.f);
		CImg(ClashArt(TEXT("shield")), 300.f, 6.f, 72.f, 72.f);
		CGroupEnd();
		CAt(CButton(G, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); }), M + 116.f, Y2, 380.f, 84.f);
	}

	// ---- row 2 right: trophy count (Trophy Road) + hamburger (Settings)
	{
		UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (int32 i = 0; i < 3; ++i)
		{
			USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			SB->SetWidthOverride(46.f); SB->SetHeightOverride(11.f);
			SB->AddChild(CBox(FLinearColor::White, ZCDark, 3.f, 5.f));
			if (UVerticalBoxSlot* S = Bars->AddChildToVerticalBox(SB)) S->SetPadding(FMargin(0.f, i ? 6.f : 0.f, 0.f, 0.f));
		}
		Bars->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(Bars, ZCBlue, [this]() { OpenClashPopup(EZoogiClashPopup::Settings); }, W - M - 88.f, Y2, 88.f, 84.f);
	}
	{
		UCanvasPanel* G = CGroupBegin();
		CPlate(30.f, 12.f, 190.f, 60.f, ZCNavyDeep.CopyWithNewOpacity(0.88f), 30.f, 4.f, false);
		CText(FString::FromInt(D.Trophies), 82.f, 12.f, 124.f, 60.f, 32.f, ZCGold, 1);
		CImg(ClashArt(TEXT("trophy")), 0.f, 0.f, 84.f, 84.f);
		CGroupEnd();
		CAt(CButton(G, [this]() { OpenClashPopup(EZoogiClashPopup::TrophyRoad); }), W - M - 88.f - 24.f - 220.f, Y2, 220.f, 84.f);
	}
}

void UZoogiMenuRoot::BuildClashBattleTab()
{
	AZoogiGameMode* G = GM();
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	const float W = DesignW, C = W * 0.5f;
	const int32 ArenaIdx = G ? (int32)G->MenuArena : 0;

	// ---- centre column: Arena N, name ribbon, map preview, trophy bar, BATTLE, chest slots
	CText(FString::Printf(TEXT("Arena %d"), ArenaIdx + 1), C - 200.f, 196.f, 400.f, 40.f, 28.f, ZCSoft);
	CImg(ClashArt(TEXT("ribbon")), C - 300.f, 230.f, 600.f, 100.f);
	CText(ZoogiUtil::ArenaName((EZoogiArena)ArenaIdx).ToUpper(), C - 220.f, 232.f, 440.f, 76.f, 40.f, FLinearColor::White);
	{
		const float PX = C - 290.f, PY = 330.f, PW = 580.f, PH = 284.f;
		CGoldPanel(PX, PY, PW, PH, ZCNavyDeep, 26.f);
		UTexture2D* Map = Art(FString(ZCArenaSlug(ArenaIdx)) + TEXT("_map"));
		if (!Map) Map = Art(TEXT("meadows_background"));
		CImg(Map, PX + 13.f, PY + 13.f, PW - 26.f, PH - 26.f, 14.f, true);
	}
	{
		const int32 A = UZoogiFrontProfileSubsystem::ArenaForTrophies(D.Trophies);
		const int32 From = UZoogiFrontProfileSubsystem::ArenaTrophies(A), To = UZoogiFrontProfileSubsystem::ArenaTrophies(A + 1);
		const float BX = C - 200.f, BY = 630.f;
		CBar(BX + 30.f, BY + 6.f, 380.f, 42.f, To > From ? float(D.Trophies - From) / (To - From) : 1.f, ZCGold, FString::Printf(TEXT("%d/%d"), D.Trophies, To), 24.f);
		CImg(ClashArt(TEXT("trophy")), BX - 10.f, BY - 6.f, 62.f, 62.f);
	}
	CChunky(TEXT("BATTLE"), ZCYellow, [this]()
	{
		UE_LOG(LogZoogi, Log, TEXT("CLASH BATTLE pressed -> MenuAction Start (current arena / players)"));
		Fire("Start", 0);
	}, C - 210.f, 692.f, 420.f, 118.f, 58.f, nullptr, true);
	for (int32 i = 0; i < 3; ++i)
	{
		const float SW = 176.f, SH = 108.f, SX = C - (SW * 1.5f + 20.f) + i * (SW + 20.f), SY = 824.f;
		const FName Chest = D.Chests.IsValidIndex(i) ? D.Chests[i] : NAME_None;
		UCanvasPanel* Grp = CGroupBegin();
		CPlate(0.f, 0.f, SW, SH, Chest.IsNone() ? ZCNavyDeep.CopyWithNewOpacity(0.72f) : ZCNavy, 16.f, 4.f, !Chest.IsNone());
		if (Chest.IsNone()) CText(TEXT("Win a battle"), 8.f, 0.f, SW - 16.f, SH, 21.f, ZCMuted, 1, 3.f);
		else { CImg(ClashArt(TEXT("chest")), SW * 0.5f - 38.f, 4.f, 76.f, 76.f); CText(Chest.ToString().ToUpper(), 4.f, SH - 34.f, SW - 8.f, 30.f, 18.f, FLinearColor::White); }
		CGroupEnd();
		CAt(CButton(Grp, [this]() { OpenClashPopup(EZoogiClashPopup::Info, TEXT("CHEST SLOT"), TEXT("Win a battle to earn a chest.\nChests and their unlock timers arrive in a later milestone.")); }), SX, SY, SW, SH);
	}

	// ---- left: Battle Pass card
	const float SideW = FMath::Clamp(C - 300.f - 90.f, 280.f, 420.f);
	{
		const float X = 56.f, Y = 330.f, CW = SideW, CH = 236.f;
		UCanvasPanel* Grp = CGroupBegin();
		CGoldPanel(0.f, 0.f, CW, CH, ZCViolet, 26.f);
		CText(TEXT("BATTLE PASS"), 18.f, 18.f, CW - 36.f, 44.f, 30.f, FLinearColor::White);
		CImg(ClashArt(TEXT("crown")), 22.f, 72.f, 96.f, 96.f);
		CImg(ClashArt(TEXT("level")), 126.f, 82.f, 64.f, 64.f);
		CText(FString::FromInt(D.BattlePassLevel), 126.f, 82.f, 64.f, 60.f, 28.f, FLinearColor::White);
		CBar(196.f, 92.f, CW - 222.f, 42.f, float(D.BattlePassXP) / UZoogiFrontProfileSubsystem::BattlePassXPPerLevel, ZCGreen,
			FString::Printf(TEXT("%d/%d"), D.BattlePassXP, UZoogiFrontProfileSubsystem::BattlePassXPPerLevel), 22.f);
		CText(TEXT("Season 1 - free track"), 126.f, 150.f, CW - 146.f, 34.f, 19.f, ZCSoft, 0, 3.f);
		CText(TEXT("TAP TO VIEW"), 18.f, CH - 52.f, CW - 36.f, 34.f, 20.f, ZCGold, 1, 3.f);
		CGroupEnd();
		CAt(CButton(Grp, [this]() { OpenClashPopup(EZoogiClashPopup::Info, TEXT("BATTLE PASS"), TEXT("Season 1 rewards track (free + premium) arrives in a later milestone.\nBattle Pass XP comes from battles; the premium track needs the store backend.")); }), X, Y, CW, CH);
		CDot(X + CW - 30.f, Y - 14.f, 46.f, TEXT("!"));
	}

	// ---- right: Trophy Road
	{
		const float X = W - 56.f - SideW, Y = 330.f;
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(ClashArt(TEXT("trophy")), 92.f, 92.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f)); }
		UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		V->AddChildToVerticalBox(CTxt(TEXT("TROPHY ROAD"), 34.f, FLinearColor::White, 4.f));
		const int32 A = UZoogiFrontProfileSubsystem::ArenaForTrophies(D.Trophies);
		V->AddChildToVerticalBox(CTxt(FString::Printf(TEXT("Next: Arena %d at %d"), A + 2, UZoogiFrontProfileSubsystem::ArenaTrophies(A + 1)), 18.f, ZCSoft, 3.f));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(V)) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(H, ZCBlue, [this]() { OpenClashPopup(EZoogiClashPopup::TrophyRoad); }, X, Y, SideW, 170.f);
	}
}

void UZoogiMenuRoot::BuildClashEmptyTab(const FString& Title, const FString& Sub, bool bDeckButton)
{
	const float W = DesignW, C = W * 0.5f;
	const float PW = FMath::Min(1040.f, W - 200.f), PH = 560.f, PX = C - PW * 0.5f, PY = 210.f;
	CGoldPanel(PX, PY, PW, PH, ZCPanel, 30.f);
	CImg(ClashArt(TEXT("ribbon")), C - 320.f, PY - 44.f, 640.f, 106.f);
	CText(Title, C - 240.f, PY - 42.f, 480.f, 80.f, 48.f, FLinearColor::White);
	const TCHAR* Icon = ClashTab == ZCShop ? TEXT("shop") : ClashTab == ZCCards ? TEXT("cards") : ClashTab == ZCClan ? TEXT("clan") : TEXT("podium");
	CImg(ClashArt(Icon), C - 90.f, PY + 92.f, 180.f, 180.f);
	CText(Sub, PX + 60.f, PY + 290.f, PW - 120.f, 130.f, 26.f, FLinearColor::White, 1, 3.f, true);
	if (bDeckButton)
		CChunky(TEXT("EDIT DECK"), ZCYellow, [this]() { Push(EZoogiScreen::Deck); }, C - 180.f, PY + PH - 132.f, 360.f, 100.f, 40.f, ClashArt(TEXT("cards")));
	else
		CText(TEXT("COMING SOON"), PX, PY + PH - 120.f, PW, 70.f, 40.f, ZCGold);
}

void UZoogiMenuRoot::BuildClashNav()
{
	const float W = DesignW, TW = W / 5.f, Y = 944.f, H = 136.f;
	// nav bar plate (gold top edge) inside the safe canvas; the full-bleed strip behind it comes from the backdrop
	CAt(CBox(ZCNavyDeep, ZCDark, 0.f, 0.f), -400.f, Y, W + 800.f, H + 200.f);
	CAt(CBox(ZCGoldDeep, ZCDark, 0.f, 0.f), -400.f, Y, W + 800.f, 5.f);
	const TCHAR* Icons[] = { TEXT("shop"), TEXT("cards"), TEXT("swords"), TEXT("clan"), TEXT("podium") };
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	for (int32 t = 0; t < 5; ++t)
	{
		const bool bSel = ClashTab == t;
		const float X = t * TW;
		UCanvasPanel* Grp = CGroupBegin();
		if (bSel)
		{
			CPlate(6.f, 0.f, TW - 12.f, H + 24.f, ZCBlue, 22.f, 5.f, true);
			CImg(ClashArt(Icons[t]), TW * 0.5f - 52.f, 2.f, 104.f, 104.f);
			CText(ZCTabName(t), 0.f, 94.f, TW, 40.f, 26.f, FLinearColor::White);
		}
		else
		{
			if (t > 0) CAt(CBox(ZCNavy, ZCDark, 0.f, 0.f), 0.f, 30.f, 3.f, H - 50.f);
			CImg(ClashArt(Icons[t]), TW * 0.5f - 42.f, 26.f, 84.f, 84.f, 0.f, false, FLinearColor(0.82f, 0.86f, 1.f, 1.f));
		}
		if (t == ZCCards && FP && FP->Data().bCardsNew) CDot(TW * 0.5f + 26.f, bSel ? 0.f : 18.f, 38.f, TEXT("!"));
		CGroupEnd();
		CAt(CButton(Grp, [this, t]() { SetClashTab(t); }), X, bSel ? Y - 24.f : Y, TW, bSel ? H + 24.f : H);
	}
}

void UZoogiMenuRoot::BuildClashPopup()
{
	const float W = DesignW, C = W * 0.5f;
	// scrim: tap outside closes
	{
		UButton* Scrim = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
		FSlateBrush Dim; Dim.TintColor = FSlateColor(FLinearColor(0.f, 0.01f, 0.06f, 0.62f));
		FButtonStyle St; St.SetNormal(Dim); St.SetHovered(Dim); St.SetPressed(Dim);
		Scrim->SetStyle(St);
		BindClick(Scrim, [this]() { ClashPopup = EZoogiClashPopup::None; bDirty = true; });
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Scrim)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(-400.f)); }
		ClashPopupScrim = Scrim;
	}
	const float PW = 980.f, PH = 640.f, PX = C - PW * 0.5f, PY = 200.f;
	UCanvasPanel* Grp = CGroupBegin();
	CGoldPanel(0.f, 0.f, PW, PH, ZCPanel, 30.f);
	CPlate(22.f, 22.f, PW - 44.f, 92.f, ZCNavy, 18.f, 4.f, true);
	FString Title = ClashInfoTitle;
	if (ClashPopup == EZoogiClashPopup::Profile) Title = TEXT("PROFILE");
	else if (ClashPopup == EZoogiClashPopup::Settings) Title = TEXT("SETTINGS");
	else if (ClashPopup == EZoogiClashPopup::TrophyRoad) Title = TEXT("TROPHY ROAD");
	CText(Title, 40.f, 22.f, PW - 80.f, 92.f, 48.f, FLinearColor::White);
	CChunky(TEXT("X"), ZCRed, [this]() { ClashPopup = EZoogiClashPopup::None; bDirty = true; }, PW - 96.f, -20.f, 104.f, 100.f, 46.f);

	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	if (ClashPopup == EZoogiClashPopup::Profile)
	{
		AZoogiGameMode* GMode = GM();
		const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
		UTexture2D* Portrait = nullptr;
		FString ZName;
		if (GMode && GMode->MenuPlayers.Num() > 0 && Roster.Num() > 0)
		{
			const FZoogiDef& Z = Roster[GMode->MenuPlayers[0].ZoogiIndex % Roster.Num()];
			Portrait = Art(PortraitFor(Z.Id)); ZName = Z.Name.ToUpper();
		}
		CAt(CBox(ZCGold, ZCDark, 6.f, 100.f), 60.f, 150.f, 200.f, 200.f);
		CImg(Portrait, 74.f, 164.f, 172.f, 172.f, 86.f, true);
		CText(ZName, 40.f, 360.f, 240.f, 40.f, 24.f, ZCSoft);
		CText(D.GuestName, 300.f, 150.f, 620.f, 60.f, 44.f, FLinearColor::White, 0);
		CImg(ClashArt(TEXT("level")), 300.f, 216.f, 64.f, 64.f);
		CText(FString::FromInt(D.Level), 300.f, 216.f, 64.f, 60.f, 28.f, FLinearColor::White);
		CText(FString::Printf(TEXT("Level %d  -  %d/%d XP"), D.Level, D.XP, UZoogiFrontProfileSubsystem::XPForLevel(D.Level)), 378.f, 220.f, 540.f, 56.f, 26.f, ZCSoft, 0, 3.f);
		CImg(ClashArt(TEXT("shield")), 300.f, 290.f, 64.f, 64.f);
		CText(TEXT("NO CLAN"), 378.f, 294.f, 540.f, 56.f, 26.f, ZCMuted, 0, 3.f);
		const FString Labels[] = { TEXT("TROPHIES"), TEXT("BEST"), TEXT("BATTLES"), TEXT("WINS") };
		const int32 Vals[] = { D.Trophies, D.BestTrophies, D.Battles, D.Wins };
		for (int32 i = 0; i < 4; ++i)
		{
			const float SX = 60.f + i * 220.f, SY = 420.f;
			CPlate(SX, SY, 200.f, 120.f, ZCNavyDeep.CopyWithNewOpacity(0.85f), 16.f, 4.f, false);
			CText(Labels[i], SX, SY + 12.f, 200.f, 36.f, 20.f, ZCMuted, 1, 3.f);
			CText(FString::FromInt(Vals[i]), SX, SY + 48.f, 200.f, 60.f, 40.f, i < 2 ? ZCGold : FLinearColor::White);
		}
		CText(TEXT("Local guest profile. Friends, name changes and an online account need the backend."), 40.f, PH - 86.f, PW - 80.f, 60.f, 19.f, ZCSoft, 1, 2.f, true);
	}
	else if (ClashPopup == EZoogiClashPopup::Settings)
	{
		auto Row = [this, PW](int32 R, const FString& Name, bool bOn, TFunction<void()> Fn)
		{
			const float Y = 150.f + R * 104.f;
			CPlate(60.f, Y, PW - 120.f, 88.f, ZCNavyDeep.CopyWithNewOpacity(0.8f), 18.f, 4.f, false);
			CText(Name, 90.f, Y, 460.f, 88.f, 32.f, FLinearColor::White, 0);
			CChunky(bOn ? TEXT("ON") : TEXT("OFF"), bOn ? ZCGreen : ZCRed, MoveTemp(Fn), PW - 60.f - 24.f - 180.f, Y + 8.f, 180.f, 72.f, 32.f);
		};
		const bool bSfx = ZoogiUtil::GetSfxVolume() > 0.001f;
		Row(0, TEXT("SOUND EFFECTS"), bSfx, [this, bSfx]() { Fire("SfxVol", bSfx ? 0 : 80); }); // existing SfxVol action (real)
		const bool bMusic = GetLocalSetting(TEXT("Music"), 60) > 0;
		Row(1, TEXT("MUSIC"), bMusic, [this, bMusic]() { SetLocalSetting(TEXT("Music"), bMusic ? 0 : 60); }); // same saved key as Settings > AUDIO
		const bool bShake = GetLocalSetting(TEXT("CameraShake"), 1) != 0;
		Row(2, TEXT("CAMERA SHAKE"), bShake, [this, bShake]() { SetLocalSetting(TEXT("CameraShake"), bShake ? 0 : 1); });
		CChunky(TEXT("MORE SETTINGS"), ZCBlue, [this]() { ClashPopup = EZoogiClashPopup::None; Push(EZoogiScreen::Settings); }, 60.f, PH - 140.f, 400.f, 96.f, 32.f);
		CChunky(TEXT("QUIT GAME"), ZCRed, [this]() { ClashPopup = EZoogiClashPopup::None; bModal = true; bModalQuit = true; ModalTitle = TEXT("QUIT ZOOGI ROLL?"); ModalBody = TEXT("See you next round!"); bDirty = true; },
			PW - 60.f - 400.f, PH - 140.f, 400.f, 96.f, 32.f);
	}
	else if (ClashPopup == EZoogiClashPopup::TrophyRoad)
	{
		const int32 Cur = UZoogiFrontProfileSubsystem::ArenaForTrophies(D.Trophies);
		for (int32 a = 0; a < UZoogiFrontProfileSubsystem::NumTrophyArenas; ++a)
		{
			const float Y = 140.f + a * 66.f;
			const bool bHere = a == Cur, bOpen = a <= Cur;
			CPlate(60.f, Y, PW - 120.f, 58.f, bHere ? ZCBlue : ZCNavyDeep.CopyWithNewOpacity(bOpen ? 0.9f : 0.6f), 14.f, 4.f, bHere);
			CText(FString::Printf(TEXT("ARENA %d"), a + 1), 84.f, Y, 180.f, 58.f, 24.f, bOpen ? ZCGold : ZCMuted, 0, 3.f);
			CText(ZoogiUtil::ArenaName((EZoogiArena)a).ToUpper(), 270.f, Y, 420.f, 58.f, 26.f, bOpen ? FLinearColor::White : ZCMuted, 0, 3.f);
			CImg(ClashArt(TEXT("trophy")), PW - 300.f, Y + 6.f, 46.f, 46.f);
			CText(FString::FromInt(UZoogiFrontProfileSubsystem::ArenaTrophies(a)), PW - 250.f, Y, 160.f, 58.f, 26.f, FLinearColor::White, 0, 3.f);
			if (bHere) CText(TEXT("YOU"), PW - 160.f, Y, 80.f, 58.f, 22.f, ZCYellow, 1, 3.f);
		}
	}
	else
	{
		CText(ClashInfoBody, 70.f, 160.f, PW - 140.f, 300.f, 28.f, FLinearColor::White, 1, 3.f, true);
		CChunky(TEXT("OK"), ZCYellow, [this]() { ClashPopup = EZoogiClashPopup::None; bDirty = true; }, PW * 0.5f - 150.f, PH - 150.f, 300.f, 104.f, 44.f);
	}
	CGroupEnd();
	Grp->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	CAt(Grp, PX, PY, PW, PH);
	ClashPopupW = Grp;
	TickClash(0.f);
}

void UZoogiMenuRoot::TickClash(float Dt)
{
	// popup "pop": ease-out-back scale 0.6 -> 1 over 260 ms + fade, scrim fades in with it (only right after opening)
	const float T = FMath::Clamp((Time - ClashPopupT0) / 0.26f, 0.f, 1.f);
	if (UWidget* P = ClashPopupW.Get())
	{
		const float S = FMath::Lerp(0.6f, 1.f, ZoogiUIStyle::EaseOutBack(T));
		P->SetRenderScale(FVector2D(S));
		P->SetRenderOpacity(FMath::Clamp(T * 2.5f, 0.f, 1.f));
	}
	if (UWidget* Sc = ClashPopupScrim.Get()) Sc->SetRenderOpacity(FMath::Clamp(T * 2.f, 0.f, 1.f));
	if (Dt > 0.f) TickClashTour(Dt);
}

// ------------------------------------------------------------------------------------------------ capture tour
// -ZoogiClashTour=<dir>: engine viewport screenshots of Home, each tab and the popups, named clash_<WxH>_NN_<what>.png.
// -ZoogiClashTourBattle: then press BATTLE and capture the started match 12 s later. -ZoogiClashTourQuit: exit when done.
void UZoogiMenuRoot::TickClashTour(float Dt)
{
	if (ClashTourStep == -1)
	{
		ClashTourStep = FParse::Value(FCommandLine::Get(), TEXT("ZoogiClashTour="), ClashTourDir) ? 0 : -2;
		if (ClashTourStep == 0) { IFileManager::Get().MakeDirectory(*ClashTourDir, true); UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR -> %s"), *ClashTourDir); }
	}
	if (ClashTourStep < 0) return;
	ClashTourClock += Dt;
	const float StepTime = 2.5f, T0 = 6.f + ClashTourStep * StepTime;
	if (ClashTourClock < T0) return;
	static const TCHAR* Names[] = { TEXT("01_home_battle"), TEXT("02_tab_shop"), TEXT("03_tab_cards"), TEXT("04_tab_clan"), TEXT("05_tab_leaderboard"),
		TEXT("06_popup_profile"), TEXT("07_popup_settings"), TEXT("08_popup_trophy_road"), TEXT("09_deck_from_cards"), TEXT("10_home_back") };
	const int32 N = UE_ARRAY_COUNT(Names);
	const FString Res = FString::Printf(TEXT("%dx%d"), FMath::RoundToInt(BuiltViewport.X), FMath::RoundToInt(BuiltViewport.Y));
	if (bClashTourSetup)
	{
		if (ClashTourClock < T0 + 1.5f) return;
		const FString P = FPaths::Combine(ClashTourDir, FString::Printf(TEXT("clash_%s_%s.png"), *Res, Names[ClashTourStep]));
		FScreenshotRequest::RequestScreenshot(P, true, false);
		UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR screenshot %s (tab %s popup %d screen %d)"), *P, ZCTabName(ClashTab), (int32)ClashPopup, (int32)Top());
		bClashTourSetup = false;
		++ClashTourStep;
		return;
	}
	bClashTourSetup = true;
	switch (ClashTourStep)
	{
	case 0: PopToHome(); SetClashTab(ZCBattle); break;
	case 1: SetClashTab(ZCShop); break;
	case 2: SetClashTab(ZCCards); break;
	case 3: SetClashTab(ZCClan); break;
	case 4: SetClashTab(ZCLeader); break;
	case 5: SetClashTab(ZCBattle); OpenClashPopup(EZoogiClashPopup::Profile); break;
	case 6: OpenClashPopup(EZoogiClashPopup::Settings); break;
	case 7: OpenClashPopup(EZoogiClashPopup::TrophyRoad); break;
	case 8: ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCCards); Push(EZoogiScreen::Deck); break;
	case 9: HandleBack(); SetClashTab(ZCBattle); break;
	default:
	{
		bClashTourSetup = false;
		ClashTourStep = -100;
		const bool bBattle = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTourBattle"));
		const bool bQuit = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTourQuit"));
		UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR menu shots done (%d)"), N);
		TWeakObjectPtr<UZoogiMenuRoot> WeakThis(this);
		auto Quit = [WeakThis]()
		{
			if (UZoogiMenuRoot* M = WeakThis.Get()) { UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR quit")); UKismetSystemLibrary::QuitGame(M, M->GetOwningPlayer(), EQuitPreference::Quit, false); }
		};
		if (bBattle)
		{
			UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR: pressing BATTLE"));
			Fire("Start", 0);
			const FString P = FPaths::Combine(ClashTourDir, FString::Printf(TEXT("clash_%s_11_battle_started.png"), *Res));
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [P, bQuit, Quit, this]()
			{
				FScreenshotRequest::RequestScreenshot(P, true, false);
				UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR screenshot %s"), *P);
				if (bQuit) { FTimerHandle H2; GetWorld()->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, Quit), 3.f, false); }
			}), 12.f, false);
		}
		else if (bQuit)
		{
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, Quit), 3.f, false);
		}
		return;
	}
	}
}
