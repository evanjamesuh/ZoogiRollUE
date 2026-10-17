// clash-menu (Oct 17, milestone 1): Clash-style front end for UZoogiMenuRoot. The Home screen becomes the Battle tab of a
// tabbed shell: persistent top bar (level badge + XP, gold, gems), row 2 (avatar, name plate + clan shield, trophies,
// hamburger), the arena column (Arena N label, name ribbon, map preview, trophy bar, big yellow BATTLE, 3 chest slots),
// Battle Pass card (left), Trophy Road (right) and a 5-tab bottom nav. Laid out for landscape in a DesignW x 1080 canvas
// inside the menu's SafeZone (notches). Every shape is drawn in code (UMG rounded boxes + procedurally rasterized icons)
// in Zoogi Roll's own art direction; the only images used are Zoogi Roll's own MenuArt (map tiles, portraits, coin, gem).
// The old Home is kept behind -ZoogiClassicMenu. Values come from UZoogiFrontProfileSubsystem (local save).
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuWidget.h" // UZoogiButton (non-focusable button)
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiUIFx.h"
#include "ZoogiUIStyle.h"
#include "ZoogiWinnerOverlay.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
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
#include "Components/ScrollBox.h"
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

// ------------------------------------------------------------------------------------------------ state
bool UZoogiMenuRoot::ClashOn() const { return !ZCClassic(); }

void UZoogiMenuRoot::SetClashTab(int32 T)
{
	T = FMath::Clamp(T, 0, 4);
	if (T == ZCCards)
		if (UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this))
			if (FP->Data().bCardsNew) { FP->Mutable().bCardsNew = false; FP->Save(); }
	if (ClashTab != T) { UE_LOG(LogZoogi, Log, TEXT("CLASH tab %s"), ZCTabName(T)); ClashScrollOffset = 0.f; ClashPendingScroll = -1.f; ClashScrollKey = -1; ClashDeckView = -1; }
	ClashTab = T;
	ClashPopup = EZoogiClashPopup::None;
	bDirty = true;
}

void UZoogiMenuRoot::OpenClashPopup(EZoogiClashPopup P, const FString& Title, const FString& Body)
{
	const bool bReturning = (ClashPopup == EZoogiClashPopup::Rewards && P == ClashAfterRewards && P != EZoogiClashPopup::None)
		|| (ClashPopup == EZoogiClashPopup::ChestOpen && P == ClashOpenBack && P != EZoogiClashPopup::None)
		|| (ClashPopup == EZoogiClashPopup::Replay && P == EZoogiClashPopup::BattleLog);
	if ((ClashBigPopup(P) || P == EZoogiClashPopup::BattleLog) && !bReturning && P != ClashPopup) { ClashScrollKey = -1; ClashScrollOffset = 0.f; ClashPendingScroll = -1.f; }
	if (P != EZoogiClashPopup::Rewards) ClashAfterRewards = EZoogiClashPopup::None;
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

UWidget* UZoogiMenuRoot::CChunkyVisual(UWidget* Inner, const FLinearColor& Face, float X, float Y, float Wd, float Ht, float R)
{
	// chunky 3D plate: dark outline, darker lip along the bottom, bright face, white gloss on the upper face
	const float Lip = FMath::Clamp(Ht * 0.11f, 5.f, 13.f);
	if (R < 0.f) R = FMath::Min(22.f, Ht * 0.3f);
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(ZCLip(Face), ZCDark, 5.f, R))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(Face, FLinearColor::Transparent, 0.f, FMath::Max(3.f, R - 4.f)))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(5.f, 5.f, 5.f, 5.f + Lip)); }
	{
		// vertical face gradient (BATTLE: #FFD84A -> #F2A500; other faces: face -> 18% darker)
		UImage* Grad = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush GBr = ZCImgBrush(ClashArt(TEXT("vgrad")), Wd, Ht, FMath::Max(3.f, R - 4.f), false);
		Grad->SetBrush(GBr);
		Grad->SetColorAndOpacity(ZCGradBottom(Face)); // BATTLE / gold / green per spec, other faces 18% darker
		if (UOverlaySlot* S = O->AddChildToOverlay(Grad)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(5.f, 5.f, 5.f, 5.f + Lip)); }
	}
	USizeBox* GB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GB->SetHeightOverride(FMath::Max(4.f, (Ht - Lip - 10.f) * 0.42f));
	GB->AddChild(CBox(FLinearColor(1.f, 1.f, 1.f, 0.30f), FLinearColor::Transparent, 0.f, FMath::Max(3.f, R - 8.f)));
	if (UOverlaySlot* S = O->AddChildToOverlay(GB)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(11.f, 8.f, 11.f, 0.f)); }
	if (Inner)
		if (UOverlaySlot* S = O->AddChildToOverlay(Inner)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(6.f, 4.f, 6.f, 4.f + Lip)); }
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(O, X, Y, Wd, Ht);
}

UWidget* UZoogiMenuRoot::CChunkyW(UWidget* Inner, const FLinearColor& Face, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, bool bPrimary, float R)
{
	UCanvasPanel* G = CGroupBegin();
	CChunkyVisual(Inner, Face, 0.f, 0.f, Wd, Ht, R);
	CGroupEnd();
	G->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(CButton(G, MoveTemp(Fn), bPrimary), X, Y, Wd, Ht);
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
	else if (Key.StartsWith(TEXT("ribbon")))
	{
		// ribbon (default blue), ribbon_gold (Trophy Road, dark-brown edges), ribbon_purple (Battle Pass), ribbon_blue (Arenas)
		W = 768; H = 128; UW = 640.f; UH = 106.7f;
		FLinearColor Base = ZCHex(TEXT("4A90EE")), Edge = ZCDark;
		if (Key == TEXT("ribbon_gold")) { Base = ZCRibGold; Edge = ZCRibGoldEdge; }
		else if (Key == TEXT("ribbon_purple")) Base = ZCRibPurple;
		else if (Key == TEXT("ribbon_blue")) Base = ZCRibBlue;
		FLinearColor FaceA = ZCLighten(Base, 0.25f), FaceB = ZCDarken(Base, 0.08f);
		FLinearColor TailA = ZCDarken(Base, 0.18f), TailB = ZCDarken(Base, 0.42f), Fold = ZCDarken(Base, 0.6f);
		if (Key == TEXT("ribbon_section")) { FaceA = ZCSecTop; FaceB = ZCSecBot; TailA = ZCDarken(ZCSecBot, 0.1f); TailB = ZCDarken(ZCSecBot, 0.3f); Fold = ZCSecFold; Edge = ZCSecFold; }
		else if (Key == TEXT("ribbon_open")) { FaceA = ZCOpenTop; FaceB = ZCOpenBot; TailA = ZCDarken(ZCOpenBot, 0.15f); TailB = ZCDarken(ZCOpenBot, 0.4f); Fold = ZCDarken(ZCOpenBot, 0.6f); }
		auto Add = [&S, Edge](FZCShape X) { X.EdgeC = Edge; S.Add(X); };
		Add(ZCPoly({ { 4, 28 }, { 110, 28 }, { 110, 100 }, { 4, 100 }, { 34, 64 } }, TailA, TailB, 5.f, false));
		Add(ZCPoly({ { 636, 28 }, { 530, 28 }, { 530, 100 }, { 636, 100 }, { 606, 64 } }, TailA, TailB, 5.f, false));
		Add(ZCPoly({ { 80, 84 }, { 112, 84 }, { 112, 100 } }, Fold, Fold, 3.f, false));
		Add(ZCPoly({ { 560, 84 }, { 528, 84 }, { 528, 100 } }, Fold, Fold, 3.f, false));
		Add(ZCPoly({ { 72, 6 }, { 568, 6 }, { 568, 86 }, { 72, 86 } }, FaceA, FaceB, 5.f));
	}
	else if (Key == TEXT("star"))
	{
		S.Add(ZCPoly({ { 50, 4 }, { 62, 34 }, { 95, 36 }, { 70, 57 }, { 79, 90 }, { 50, 72 }, { 21, 90 }, { 30, 57 }, { 5, 36 }, { 38, 34 } }, GoldA, GoldB, 5.f));
	}
	else if (Key == TEXT("vgrad"))
	{
		ZoogiUIFx::FPixels P; P.W = 4; P.H = 64; P.Px.SetNumUninitialized(P.W * P.H);
		for (int32 y = 0; y < P.H; ++y) for (int32 x = 0; x < P.W; ++x) P.Px[y * P.W + x] = FLinearColor(1.f, 1.f, 1.f, FMath::Pow((float)y / (P.H - 1), 1.2f)).ToFColor(true);
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		ArtCache.Add(CacheKey, T);
		return T;
	}
	else if (Key == TEXT("banner"))
	{
		// popup header: sky, soft clouds, a castle silhouette on a green hill (painted in code)
		ZoogiUIFx::FPixels P; P.W = 768; P.H = 192; P.Px.SetNumUninitialized(P.W * P.H);
		const FLinearColor SkyT = ZCHex(TEXT("6EC6FF")), SkyB = ZCHex(TEXT("BDE8FF")), Grass = ZCHex(TEXT("5CC83A")), GrassD = ZCHex(TEXT("2F8F22")), Stone = ZCHex(TEXT("8FA6C8")), StoneD = ZCHex(TEXT("5F7598"));
		auto Castle = [](float x, float y) -> int32 // 0 none, 1 wall, 2 roof
		{
			const float CX = 384.f, Base = 150.f;
			auto Box = [&](float X0, float X1, float Top) { return x >= X0 && x <= X1 && y >= Top && y <= Base; };
			auto Merlons = [&](float X0, float X1, float Top) { return x >= X0 && x <= X1 && y >= Top - 10.f && y < Top && FMath::Fmod(x - X0, 16.f) < 9.f; };
			auto Roof = [&](float X0, float X1, float Top, float Hh) { const float M = (X0 + X1) * 0.5f, Hw = (X1 - X0) * 0.5f + 6.f; return y < Top && y >= Top - Hh && FMath::Abs(x - M) <= Hw * (1.f - (Top - y) / Hh); };
			if (Roof(CX - 150.f, CX - 110.f, 54.f, 40.f) || Roof(CX + 110.f, CX + 150.f, 54.f, 40.f) || Roof(CX - 26.f, CX + 26.f, 30.f, 34.f)) return 2;
			if (Box(CX - 150.f, CX - 110.f, 54.f) || Box(CX + 110.f, CX + 150.f, 54.f) || Box(CX - 26.f, CX + 26.f, 30.f) || Box(CX - 110.f, CX + 110.f, 92.f)) return 1;
			if (Merlons(CX - 110.f, CX + 110.f, 92.f)) return 1;
			return 0;
		};
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				FLinearColor C = FMath::Lerp(SkyT, SkyB, (float)y / P.H);
				const float Cl = FMath::Sin(x * 0.021f) * 6.f + FMath::Sin(x * 0.057f + 1.3f) * 4.f;
				if (y > 26.f + Cl && y < 48.f + Cl * 0.5f && FMath::Fmod(x + 40.f, 260.f) < 150.f) C = FMath::Lerp(C, FLinearColor::White, 0.75f);
				const int32 K = Castle((float)x, (float)y);
				if (K == 1) C = FMath::Lerp(Stone, StoneD, (float)y / P.H);
				else if (K == 2) C = ZCHex(TEXT("E9493F"));
				const float Hill = 150.f + FMath::Sin(x * 0.012f) * 10.f;
				if (y > Hill) C = FMath::Lerp(Grass, GrassD, FMath::Clamp((y - Hill) / 40.f, 0.f, 1.f));
				P.Px[y * P.W + x] = C.ToFColor(true);
			}
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		ArtCache.Add(CacheKey, T);
		return T;
	}
	else if (Key.StartsWith(TEXT("chest_")))
	{
		// wooden = warm wood + gold bands, royal = blue + gold, epic = violet + gold with a gem lock
		const bool bRoyal = Key == TEXT("chest_royal"), bEpic = Key == TEXT("chest_epic");
		const FLinearColor A = bEpic ? ZCHex(TEXT("B07CFF")) : bRoyal ? ZCHex(TEXT("5FA8FF")) : ZCHex(TEXT("D2843C"));
		const FLinearColor B = bEpic ? ZCHex(TEXT("5A20B5")) : bRoyal ? ZCHex(TEXT("1E4FC2")) : ZCHex(TEXT("7A3F14"));
		S.Add(ZCRect(10, 46, 90, 90, A, B));
		S.Add(ZCPoly({ { 10, 48 }, { 10, 30 }, { 21, 17 }, { 50, 12 }, { 79, 17 }, { 90, 30 }, { 90, 48 } }, ZCLighten(A, 0.12f), A));
		S.Add(ZCRect(22, 14, 31, 90, GoldA, GoldB, 2.5f));
		S.Add(ZCRect(69, 14, 78, 90, GoldA, GoldB, 2.5f));
		S.Add(ZCRect(10, 44, 90, 52, GoldA, GoldB, 2.5f));
		if (bEpic) S.Add(ZCPoly({ { 50, 36 }, { 61, 48 }, { 50, 64 }, { 39, 48 } }, ZCHex(TEXT("FF8BE8")), ZCHex(TEXT("C2269E")), 3.f));
		else S.Add(ZCRect(41, 38, 59, 60, GoldA, GoldB, 3.f));
	}
	else if (Key == TEXT("goldpile"))
	{
		const FVector2f Coins[] = { { 22, 76 }, { 50, 80 }, { 78, 76 }, { 36, 60 }, { 64, 60 }, { 50, 42 }, { 30, 84 }, { 70, 84 } };
		for (const FVector2f& Cn : Coins) { S.Add(ZCCircle(Cn, 16, GoldA, GoldB, 3.f)); S.Add(ZCCircle(Cn, 8, ZCHex(TEXT("FFF2A8")), GoldA, 2.f, false)); }
	}
	else if (Key == TEXT("lock"))
	{
		S.Add(ZCPoly({ { 30, 46 }, { 30, 30 }, { 38, 16 }, { 50, 12 }, { 62, 16 }, { 70, 30 }, { 70, 46 }, { 60, 46 }, { 60, 31 }, { 56, 24 }, { 50, 22 }, { 44, 24 }, { 40, 31 }, { 40, 46 } }, SilverA, SilverB, 3.f));
		S.Add(ZCRect(20, 44, 80, 90, GoldA, GoldB, 4.f));
		S.Add(ZCCircle({ 50, 64 }, 7, ZCDark, ZCDark, 0.f, false));
		S.Add(ZCRect(47, 64, 53, 78, ZCDark, ZCDark, 0.f, false));
	}
	else if (Key == TEXT("emote"))
	{
		S.Add(ZCPoly({ { 14, 18 }, { 86, 18 }, { 86, 70 }, { 46, 70 }, { 26, 88 }, { 30, 70 }, { 14, 70 } }, FLinearColor::White, ZCHex(TEXT("C9D7F2")), 4.f));
		S.Add(ZCCircle({ 36, 40 }, 6, ZCDark, ZCDark, 0.f, false));
		S.Add(ZCCircle({ 64, 40 }, 6, ZCDark, ZCDark, 0.f, false));
		S.Add(ZCPoly({ { 32, 52 }, { 68, 52 }, { 60, 62 }, { 40, 62 } }, ZCHex(TEXT("FF6A5C")), ZCHex(TEXT("C2271F")), 2.5f, false));
	}
	else if (Key == TEXT("rays") || Key == TEXT("spinner"))
	{
		// painted directly: rays = 16 soft cream wedges fading out; spinner = a 300-degree ring with a bright head
		ZoogiUIFx::FPixels P; P.W = P.H = (Key == TEXT("rays")) ? 512 : 128; P.Px.SetNumUninitialized(P.W * P.H);
		const bool bRays = Key == TEXT("rays");
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float DX = (x + 0.5f) / P.W - 0.5f, DY = (y + 0.5f) / P.H - 0.5f;
				const float R = FMath::Sqrt(DX * DX + DY * DY) * 2.f; // 0 centre .. 1 edge
				const float Ang = FMath::Atan2(DY, DX);              // -pi..pi
				FLinearColor C = FLinearColor::Transparent;
				if (bRays)
				{
					const float Wv = 0.5f + 0.5f * FMath::Cos(Ang * 16.f);
					const float Wedge = FMath::Clamp((Wv - 0.45f) * 6.f, 0.f, 1.f);
					const float Fade = FMath::Clamp(1.f - R, 0.f, 1.f);
					const float Glow = FMath::Clamp(1.f - R * 1.8f, 0.f, 1.f);
					const float A = FMath::Clamp(Wedge * Fade * 0.55f + Glow * Glow * 0.75f, 0.f, 1.f);
					C = FLinearColor(1.f, 0.93f, 0.62f, A);
				}
				else
				{
					const float Band = FMath::Clamp(1.f - FMath::Abs(R - 0.78f) / 0.16f, 0.f, 1.f);
					const float T = (Ang + PI) / (2.f * PI); // 0..1 around
					const float A = T < 0.17f ? 0.f : FMath::Clamp(Band * 3.f, 0.f, 1.f) * FMath::Clamp((T - 0.17f) * 2.5f, 0.15f, 1.f);
					const float Edge = FMath::Clamp(1.f - FMath::Abs(R - 0.78f) / 0.2f, 0.f, 1.f);
					C = FMath::Lerp(ZCDark, FLinearColor::White, FMath::Clamp(Band * 1.6f - 0.3f, 0.f, 1.f));
					C.A = FMath::Max(A, T < 0.17f ? 0.f : Edge * 0.9f * (A > 0.f ? 1.f : 0.f));
				}
				P.Px[y * P.W + x] = C.ToFColor(true);
			}
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		ArtCache.Add(CacheKey, T);
		return T;
	}
	else if (Key == TEXT("copy"))
	{
		S.Add(ZCRect(14, 8, 66, 66, ZCHex(TEXT("DDE8FA")), ZCHex(TEXT("A9BCDD")), 5.f, false));
		S.Add(ZCRect(34, 30, 86, 92, FLinearColor::White, ZCHex(TEXT("C9D7F2")), 5.f, false));
	}
	// ---- DETAILED PASS 3 icons: clan badges (bolt, phoenix, axe, wolf), magnifier, elixir drop, up arrow, sort arrow
	else if (Key == TEXT("bolt"))
	{
		S.Add(ZCPoly({ { 58, 4 }, { 22, 54 }, { 46, 54 }, { 36, 96 }, { 80, 40 }, { 54, 40 }, { 66, 4 } }, ZCHex(TEXT("D9A6FF")), ZCHex(TEXT("7B3FBF")), 4.f));
	}
	else if (Key == TEXT("phoenix"))
	{
		const FLinearColor A = ZCHex(TEXT("FFC24A")), B = ZCHex(TEXT("E2591C"));
		S.Add(ZCPoly({ { 50, 30 }, { 30, 12 }, { 6, 14 }, { 20, 30 }, { 8, 40 }, { 30, 50 }, { 42, 60 } }, A, B, 3.f));
		S.Add(ZCPoly({ { 50, 30 }, { 70, 12 }, { 94, 14 }, { 80, 30 }, { 92, 40 }, { 70, 50 }, { 58, 60 } }, A, B, 3.f));
		S.Add(ZCPoly({ { 50, 18 }, { 60, 34 }, { 58, 62 }, { 66, 92 }, { 50, 80 }, { 34, 92 }, { 42, 62 }, { 40, 34 } }, ZCHex(TEXT("FFE07A")), B, 3.f));
	}
	else if (Key == TEXT("axe"))
	{
		S.Add(ZCRect(46, 14, 56, 94, ZCHex(TEXT("B07A3E")), ZCHex(TEXT("6E4518")), 3.f));
		S.Add(ZCPoly({ { 54, 18 }, { 84, 8 }, { 94, 30 }, { 88, 52 }, { 54, 44 } }, ZCHex(TEXT("8CF07A")), ZCHex(TEXT("2E9A3C")), 4.f));
		S.Add(ZCPoly({ { 48, 18 }, { 26, 12 }, { 20, 30 }, { 26, 46 }, { 48, 42 } }, ZCHex(TEXT("8CF07A")), ZCHex(TEXT("2E9A3C")), 4.f));
	}
	else if (Key == TEXT("wolf"))
	{
		const FLinearColor A = ZCHex(TEXT("D6DEE8")), B = ZCHex(TEXT("6E7C90"));
		S.Add(ZCPoly({ { 20, 8 }, { 38, 30 }, { 62, 30 }, { 80, 8 }, { 84, 46 }, { 70, 70 }, { 50, 94 }, { 30, 70 }, { 16, 46 } }, A, B, 4.f));
		S.Add(ZCPoly({ { 30, 46 }, { 44, 50 }, { 32, 54 } }, ZCDark, ZCDark, 0.f, false));
		S.Add(ZCPoly({ { 70, 46 }, { 56, 50 }, { 68, 54 } }, ZCDark, ZCDark, 0.f, false));
		S.Add(ZCPoly({ { 42, 74 }, { 58, 74 }, { 50, 86 } }, ZCDark, ZCDark, 0.f, false));
	}
	else if (Key == TEXT("search"))
	{
		S.Add(ZCPoly({ { 58, 62 }, { 66, 54 }, { 92, 82 }, { 84, 90 } }, FLinearColor::White, ZCHex(TEXT("C9D7F2")), 3.f));
		S.Add(ZCCircle({ 40, 40 }, 28, FLinearColor::White, ZCHex(TEXT("C9D7F2")), 4.f));
		S.Add(ZCCircle({ 40, 40 }, 17, ZCHex(TEXT("6FB4FF")), ZCHex(TEXT("2D74D6")), 2.f, false));
	}
	else if (Key == TEXT("drop"))
	{
		S.Add(ZCPoly({ { 50, 4 }, { 70, 34 }, { 82, 56 }, { 80, 74 }, { 66, 90 }, { 50, 95 }, { 34, 90 }, { 20, 74 }, { 18, 56 }, { 30, 34 } }, ZCHex(TEXT("FF7AE6")), ZCHex(TEXT("B0129A")), 4.f));
	}
	else if (Key == TEXT("arrow_up"))
	{
		S.Add(ZCPoly({ { 50, 6 }, { 90, 50 }, { 66, 50 }, { 66, 94 }, { 34, 94 }, { 34, 50 }, { 10, 50 } }, ZCHex(TEXT("FFE45C")), ZCHex(TEXT("E0A000")), 4.f));
	}
	else if (Key == TEXT("arrow_down"))
	{
		S.Add(ZCPoly({ { 50, 94 }, { 90, 50 }, { 66, 50 }, { 66, 6 }, { 34, 6 }, { 34, 50 }, { 10, 50 } }, FLinearColor::White, ZCHex(TEXT("C9D7F2")), 4.f));
	}
	else if (Key == TEXT("check"))
	{
		S.Add(ZCCircle({ 50, 50 }, 44, ZCHex(TEXT("7BF07B")), ZCHex(TEXT("2FAE2F")), 5.f));
		S.Add(ZCPoly({ { 24, 50 }, { 34, 40 }, { 45, 52 }, { 70, 26 }, { 80, 36 }, { 45, 72 } }, FLinearColor::White, ZCHex(TEXT("E2F5E2")), 3.f, false));
	}
	else if (Key == TEXT("pencil"))
	{
		S.Add(ZCRotRect({ 52, 48 }, 34, 10, -45, ZCHex(TEXT("FFD54A")), ZCHex(TEXT("E39410"))));
		S.Add(ZCPoly({ { 20, 80 }, { 16, 92 }, { 28, 88 } }, ZCDark, ZCDark, 2.f, false));
		S.Add(ZCRotRect({ 76, 24 }, 7, 10, -45, ZCHex(TEXT("FF8A80")), ZCHex(TEXT("D84339"))));
	}
	else if (Key == TEXT("knob"))
	{
		S.Add(ZCCircle({ 50, 50 }, 42, FLinearColor::White, ZCHex(TEXT("D6E2F5")), 6.f));
	}
	else if (Key == TEXT("cardback"))
	{
		// face-down card: navy with a gold star (chest opening)
		W = 128; H = 160; UW = 100.f; UH = 125.f;
		S.Add(ZCRect(4, 4, 96, 121, ZCHex(TEXT("2B4C9B")), ZCHex(TEXT("14295E")), 5.f));
		S.Add(ZCRect(14, 14, 86, 111, ZCHex(TEXT("1D3A80")), ZCHex(TEXT("102454")), 2.5f, false));
		S.Add(ZCPoly({ { 50, 34 }, { 57, 52 }, { 77, 53 }, { 61, 65 }, { 67, 85 }, { 50, 74 }, { 33, 85 }, { 39, 65 }, { 23, 53 }, { 43, 52 } }, GoldA, GoldB, 3.5f));
	}
	else if (Key == TEXT("zdot"))
	{
		// replay token: white disc (tinted per Zoogi) with a dark notch pointing +X (rotated by the recorded yaw)
		S.Add(ZCCircle({ 50, 50 }, 40, FLinearColor::White, ZCHex(TEXT("C8CCD6")), 7.f));
		S.Add(ZCPoly({ { 62, 40 }, { 92, 50 }, { 62, 60 } }, ZCDark, ZCDark, 0.f, false));
	}
	else if (Key == TEXT("play"))
	{
		S.Add(ZCPoly({ { 28, 14 }, { 84, 50 }, { 28, 86 } }, FLinearColor::White, ZCHex(TEXT("DDE8FA")), 4.f, false));
	}
	else if (Key == TEXT("pause"))
	{
		S.Add(ZCRect(22, 14, 42, 86, FLinearColor::White, ZCHex(TEXT("DDE8FA")), 4.f, false));
		S.Add(ZCRect(58, 14, 78, 86, FLinearColor::White, ZCHex(TEXT("DDE8FA")), 4.f, false));
	}
	else if (Key == TEXT("chevron"))
	{
		S.Add(ZCPoly({ { 50, 10 }, { 90, 52 }, { 70, 52 }, { 70, 90 }, { 30, 90 }, { 30, 52 }, { 10, 52 } }, ZCHex(TEXT("FFE66B")), ZCHex(TEXT("F5A300")), 5.f));
	}
	else if (Key == TEXT("bg_shop"))
	{
		// DETAILED PASS 2 shop backdrop: lattice gradient #1E6BD6 -> #0E3F92
		ZoogiUIFx::FPixels P; P.W = 512; P.H = 288; P.Px.SetNumUninitialized(P.W * P.H);
		const FLinearColor Top = ZCHex(TEXT("1E6BD6")), Bot = ZCHex(TEXT("0E3F92"));
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float T = (float)y / (P.H - 1);
				FLinearColor C = FMath::Lerp(Top, Bot, T);
				const float Cell = 48.f;
				const float U = FMath::Fmod(x + y, Cell) / Cell, V = FMath::Fmod(x - y + 4096.f, Cell) / Cell;
				const float Line = FMath::Min(FMath::Min(U, 1.f - U), FMath::Min(V, 1.f - V));
				C = FMath::Lerp(C, ZCLighten(C, 0.16f), FMath::Clamp((0.04f - Line) * 40.f, 0.f, 0.85f));
				P.Px[y * P.W + x] = C.ToFColor(true);
			}
		UTexture2D* T = ZoogiUIFx::ToTexture(P);
		ArtCache.Add(CacheKey, T);
		return T;
	}
	else if (Key == TEXT("bg"))
	{
		// backdrop: bright sky blue -> royal blue with a faint diamond lattice (painted directly, no shapes)
		ZoogiUIFx::FPixels P; P.W = 512; P.H = 288; P.Px.SetNumUninitialized(P.W * P.H);
		// DETAILED PASS 1: royal blue ~#2A6FC4 with ~#3E8BDD lattice lines, deep-blue vignette at the top
		const FLinearColor Top = ZCHex(TEXT("1E4FA0")), Mid = ZCHex(TEXT("2A6FC4")), Bot = ZCHex(TEXT("2563B4")), LineC = ZCHex(TEXT("3E8BDD"));
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				const float T = (float)y / (P.H - 1);
				FLinearColor C = T < 0.35f ? FMath::Lerp(Top, Mid, T / 0.35f) : FMath::Lerp(Mid, Bot, (T - 0.35f) / 0.65f);
				const float Cell = 48.f;
				const float U = FMath::Fmod(x + y, Cell) / Cell, V = FMath::Fmod(x - y + 4096.f, Cell) / Cell;
				const float Line = FMath::Min(FMath::Min(U, 1.f - U), FMath::Min(V, 1.f - V));
				C = FMath::Lerp(C, LineC, FMath::Clamp((0.04f - Line) * 40.f, 0.f, 0.85f));
				const float Vig = FMath::Clamp(FVector2f((x - P.W * 0.5f) / P.W, (y - P.H * 0.62f) / P.H).Size() * 1.1f, 0.f, 1.f);
				C = FMath::Lerp(C, ZCHex(TEXT("123A86")), Vig * Vig * 0.45f + FMath::Clamp(0.22f - T, 0.f, 0.22f) * 1.6f);
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
	if (UTexture2D* Bg = ClashArt(ClashTab == ZCShop ? TEXT("bg_shop") : TEXT("bg")))
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
	if (!bClashAudioInit) { bClashAudioInit = true; ClashApplyAudio(true); } // saved Settings volumes -> the real audio
	const bool bTabSwitch = ClashBuiltTab != -1 && ClashBuiltTab != ClashTab;
	const bool bEnter = bAnimEnter || bTabSwitch;
	ClashBuiltTab = ClashTab;
	// top bar + nav pop in only when the screen is entered, tab content pops on every tab switch
	bAnimEnter = bAnimEnter && !bTabSwitch;
	BuildClashTopBar();
	BuildClashNav();
	bAnimEnter = bEnter;
	ClashLive.Reset();
	ClashSpinW.Reset(); ClashRaysW.Reset();
	// back from a match: show the real result once (rewards were already applied by UZoogiFrontRewardBridge)
	if (UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this))
		if (FP->Data().LastMatch.bPending && ClashTab == ZCBattle && ClashPopup == EZoogiClashPopup::None)
		{
			// DETAILED PASS 5: no separate results screen (the Winner overlay was the end screen); straight to Home
			FP->Mutable().LastMatch.bPending = false;
			FP->Save();
			UE_LOG(LogZoogi, Log, TEXT("CLASH back from match (outcome %d): Home, no results popup"), FP->Data().LastMatch.Outcome);
		}
	switch (ClashTab)
	{
	case ZCBattle: BuildClashBattleTab(); break;
	case ZCShop: BuildClashShopTab(); break;
	case ZCCards: BuildClashCardsTab(); break;
	case ZCClan: BuildClashClanTab(); break;
	default: BuildClashLeaderboardTab(); break;
	}
	bAnimEnter = false;
	if (ClashPopup != EZoogiClashPopup::None) BuildClashPopup();
	else { ClashPopupW.Reset(); ClashPopupScrim.Reset(); }
	// toast (DETAILED PASS 4: "Card removed from deck." etc.), 2 s, above everything
	if (!ClashToastMsg.IsEmpty() && Time - ClashToastT < 2.f)
	{
		const float TW = FMath::Min(DesignW - 80.f, 120.f + ClashToastMsg.Len() * 17.f), TY = ClashNavY() - 150.f;
		UWidget* Pl = CPlate(DesignW * 0.5f - TW * 0.5f, TY, TW, 90.f, ZCHex(TEXT("0B1E44")).CopyWithNewOpacity(0.94f), 45.f, 4.f, false);
		UWidget* Tx = CText(ClashToastMsg, DesignW * 0.5f - TW * 0.5f, TY, TW, 90.f, 30.f, FLinearColor::White, 1, 3.f);
		if (Pl) Pl->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (Tx) Tx->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UZoogiMenuRoot::BuildClashTopBar()
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	const float W = DesignW;
	const float M = 24.f; // M4 portrait: 1080-wide canvas

	// ---- row 1 left: level badge (blue hex) over the XP bar; both inert (DETAILED PASS 2)
	const int32 Need = UZoogiFrontProfileSubsystem::XPForLevel(D.Level);
	CBar(M + 54.f, 30.f, 250.f, 46.f, Need > 0 ? float(D.XP) / Need : 0.f, ZCCyan, FString::Printf(TEXT("%d/%d"), D.XP, Need), 24.f);
	CImg(ClashArt(TEXT("level")), M, 9.f, 88.f, 88.f);
	CText(FString::FromInt(D.Level), M, 9.f, 88.f, 84.f, 36.f, FLinearColor::White);

	// ---- row 1 right: gold + gem pills (translucent navy #0B2A5C), green '+' squares (#4CD964): both open the Shop at the TOP
	auto Pill = [this](float X, UTexture2D* Icon, int32 Value, TFunction<void()> Plus)
	{
		const float PW = 232.f, PH = 56.f, Y = 24.f;
		CPlate(X, Y, PW, PH, ZCPill.CopyWithNewOpacity(0.82f), PH * 0.5f, 4.f, false);
		CText(FText::AsNumber(Value).ToString(), X + 54.f, Y, PW - 54.f - 66.f, PH, 30.f, FLinearColor::White, 2);
		CImg(Icon, X - 22.f, Y - 9.f, 74.f, 74.f);
		CChunky(TEXT("+"), ZCPlus, MoveTemp(Plus), X + PW - 60.f, Y - 1.f, 58.f, 58.f, 38.f);
	};
	const float GemX = W - M - 232.f, GoldX = GemX - 34.f - 232.f;
	auto ShopTop = [this]() { SetClashTab(ZCShop); ClashScrollOffset = 0.f; ClashPendingScroll = 0.f; bDirty = true; }; // Daily Deals
	// gems = the progression ledger (one balance for the menu and every purchase)
	Pill(GoldX, Art(TEXT("icon_coin")), D.Gold, ShopTop);
	Pill(GemX, Art(TEXT("icon_gem")), FP ? FP->Gems() : 0, ShopTop);

	if (ClashTab != ZCBattle) return; // row 2 belongs to the Battle (home) tab

	// ---- row 2 left: round avatar (Profile), name plate + clan shield
	const float Y2 = 116.f;
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
		CPlate(0.f, 0.f, 360.f, 84.f, ZCNavy.CopyWithNewOpacity(0.92f), 18.f, 4.f, true);
		CText(D.GuestName, 20.f, 6.f, 260.f, 42.f, 28.f, FLinearColor::White, 0);
		CText(TEXT("NO CLAN"), 20.f, 44.f, 260.f, 32.f, 20.f, ZCMuted, 0, 3.f);
		CImg(ClashArt(TEXT("shield")), 280.f, 6.f, 72.f, 72.f);
		CGroupEnd();
		CAt(CButton(G, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); }), M + 116.f, Y2, 360.f, 84.f);
	}

	// ---- row 2 right: trophy count (Profile, DETAILED PASS 2) + hamburger (Settings)
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
		CAt(CButton(G, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); }), W - M - 88.f - 24.f - 220.f, Y2, 220.f, 84.f);
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
	// M4 portrait (Clash Battle demo): Battle Pass + Trophy Road cards side by side under row 2, then the centre column
	// (Arena N, name ribbon, map, trophy bar, BATTLE, chest slots). Extra height on 9:19.5 / 9:20 goes into the gaps.
	const float Y0 = 222.f, CardH = 196.f, CW = (W - 72.f) * 0.5f;
	const float Extra = FMath::Max(0.f, ClashNavY() - 1520.f);

	// ---- Battle Pass card (left)
	{
		const float X = 24.f, Y = Y0, CH = CardH;
		UCanvasPanel* Grp = CGroupBegin();
		CGoldPanel(0.f, 0.f, CW, CH, ZCViolet, 26.f);
		CText(TEXT("BATTLE PASS"), 16.f, 12.f, CW - 32.f, 44.f, 30.f, FLinearColor::White);
		CImg(ClashArt(TEXT("crown")), 16.f, 58.f, 84.f, 84.f);
		CImg(ClashArt(TEXT("star")), 102.f, 64.f, 68.f, 68.f);
		CText(FString::FromInt(D.BattlePassLevel), 102.f, 70.f, 68.f, 56.f, 26.f, FLinearColor::White);
		CBar(176.f, 76.f, CW - 196.f, 42.f, float(D.BattlePassXP) / UZoogiFrontProfileSubsystem::BattlePassXPPerLevel, ZCGreen,
			FString::Printf(TEXT("%d/%d"), D.BattlePassXP, UZoogiFrontProfileSubsystem::BattlePassXPPerLevel), 22.f);
		CText(FString::Printf(TEXT("Season %d"), UZoogiFrontProfileSubsystem::PassSeason), 176.f, 120.f, CW - 196.f, 30.f, 19.f, ZCSoft, 0, 3.f);
		CText(TEXT("TAP TO VIEW"), 16.f, CH - 44.f, CW - 32.f, 34.f, 20.f, ZCGold, 1, 3.f);
		CGroupEnd();
		CAt(CButton(Grp, [this]() { OpenClashPopup(EZoogiClashPopup::BattlePass, TEXT("BATTLE PASS")); }), X, Y, CW, CH);
		if (FP && FP->PassClaimableCount() > 0) CDot(X + CW - 30.f, Y - 14.f, 46.f, TEXT("!")); // red dot only while a tier is claimable
	}

	// ---- Trophy Road card (right)
	{
		const float X = W - 24.f - CW, Y = Y0;
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(ClashArt(TEXT("trophy")), 92.f, 92.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f)); }
		UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		V->AddChildToVerticalBox(CTxt(TEXT("TROPHY ROAD"), 34.f, FLinearColor::White, 4.f));
		const int32 A = UZoogiFrontProfileSubsystem::ArenaForTrophies(D.Trophies);
		V->AddChildToVerticalBox(CTxt(FString::Printf(TEXT("Next: Arena %d at %d"), A + 2, UZoogiFrontProfileSubsystem::ArenaTrophies(A + 1)), 19.f, ZCSoft, 3.f));
		V->AddChildToVerticalBox(CTxt(TEXT("TAP TO VIEW"), 20.f, ZCGold, 3.f));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(V)) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(H, ZCBlue, [this]() { OpenClashPopup(EZoogiClashPopup::TrophyRoad); }, X, Y, CW, CardH);
		if (FP && FP->RoadClaimableCount() > 0) CDot(X + CW - 30.f, Y - 14.f, 46.f, TEXT("!"));
	}

	// ---- centre column
	float Y = Y0 + CardH + 26.f + Extra * 0.25f;
	CText(FString::Printf(TEXT("Arena %d"), ArenaIdx + 1), C - 200.f, Y, 400.f, 40.f, 28.f, ZCSoft);
	CImg(ClashArt(TEXT("ribbon")), C - 330.f, Y + 34.f, 660.f, 110.f);
	CText(ZoogiUtil::ArenaName((EZoogiArena)ArenaIdx).ToUpper(), C - 250.f, Y + 36.f, 500.f, 84.f, 44.f, FLinearColor::White);
	Y += 150.f;
	{
		const float PW = 900.f, PH = 480.f, PX = C - PW * 0.5f, PY = Y;
		// arena preview opens the ARENAS screen (the name ribbon above does nothing, like the demo)
		UCanvasPanel* PG = CGroupBegin();
		CGoldPanel(0.f, 0.f, PW, PH, ZCNavyDeep, 26.f);
		UTexture2D* Map = Art(FString(ZCArenaSlug(ArenaIdx)) + TEXT("_map"));
		if (!Map) Map = Art(TEXT("meadows_background"));
		CImg(Map, 13.f, 13.f, PW - 26.f, PH - 26.f, 14.f, true);
		CGroupEnd();
		CAt(CButton(PG, [this]() { ClashArenaSel = -1; OpenClashPopup(EZoogiClashPopup::Arenas, TEXT("ARENAS")); }), PX, PY, PW, PH);
	}
	Y += 480.f + 22.f + Extra * 0.15f;
	{
		const int32 A = UZoogiFrontProfileSubsystem::ArenaForTrophies(D.Trophies);
		const int32 From = UZoogiFrontProfileSubsystem::ArenaTrophies(A), To = UZoogiFrontProfileSubsystem::ArenaTrophies(A + 1);
		const float BarW = 560.f, BX = C - BarW * 0.5f, BY = Y + 6.f;
		// trophy bar opens TROPHY ROAD
		UCanvasPanel* TG = CGroupBegin();
		CBar(44.f, 12.f, BarW - 44.f, 44.f, To > From ? float(D.Trophies - From) / (To - From) : 1.f, ZCGold, FString::Printf(TEXT("%d/%d"), D.Trophies, To), 26.f);
		CImg(ClashArt(TEXT("trophy")), 0.f, 0.f, 66.f, 66.f);
		CGroupEnd();
		CAt(CButton(TG, [this]() { OpenClashPopup(EZoogiClashPopup::TrophyRoad, TEXT("TROPHY ROAD")); }), BX - 10.f, BY - 6.f, BarW + 10.f, 70.f);
	}
	Y += 76.f + 24.f + Extra * 0.3f;
	CChunky(TEXT("BATTLE"), ZCYellow, [this]()
	{
		UE_LOG(LogZoogi, Log, TEXT("CLASH BATTLE pressed -> matchmaking screen -> MenuAction Start (current arena / players)"));
		ClashStartMatchmaking(); // DETAILED PASS 3: search screen first, then the real match
	}, C - 270.f, Y, 540.f, 150.f, 66.f, nullptr, true);
	Y += 150.f + 30.f + Extra * 0.15f;
	BuildClashChestSlots(Y);
}

void UZoogiMenuRoot::BuildClashEmptyTab(const FString& Title, const FString& Sub, bool bDeckButton)
{
	const float W = DesignW, C = W * 0.5f;
	const float PW = FMath::Min(1000.f, W - 80.f), PH = 620.f, PX = C - PW * 0.5f, PY = FMath::Max(260.f, (ClashNavY() - PH) * 0.42f);
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
	const float W = DesignW, TW = W / 5.f, Y = ClashNavY(), H = 150.f;
	// M4 portrait: nav strip spans the 1080 column only (pillar-boxed on a landscape window), bleeds past the bottom safe edge
	CAt(CBox(ZCNavyDeep, ZCDark, 0.f, 0.f), 0.f, Y, W, H + 300.f);
	CAt(CBox(ZCGoldDeep, ZCDark, 0.f, 0.f), 0.f, Y, W, 5.f);
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
			CImg(ClashArt(Icons[t]), TW * 0.5f - 56.f, 2.f, 112.f, 112.f);
			CText(ZCTabName(t), 0.f, 108.f, TW, 40.f, 26.f, FLinearColor::White);
		}
		else
		{
			if (t > 0) CAt(CBox(ZCNavy, ZCDark, 0.f, 0.f), 0.f, 30.f, 3.f, H - 50.f);
			CImg(ClashArt(Icons[t]), TW * 0.5f - 46.f, 28.f, 92.f, 92.f, 0.f, false, FLinearColor(0.82f, 0.86f, 1.f, 1.f));
		}
		if (t == ZCCards && FP && (FP->Data().bCardsNew || FP->UpgradableCount() > 0)) CDot(TW * 0.5f + 26.f, bSel ? 0.f : 18.f, 38.f, TEXT("!"));
		CGroupEnd();
		CAt(CButton(Grp, [this, t]() { SetClashTab(t); }), X, bSel ? Y - 24.f : Y, TW, bSel ? H + 24.f : H);
	}
}

void UZoogiMenuRoot::BuildClashPopup()
{
	const float W = DesignW, C = W * 0.5f;
	// full-screen flows (no popup panel): chest opening, replay viewer
	if (ClashPopup == EZoogiClashPopup::ChestOpen) { ClashPopupW.Reset(); ClashPopupScrim.Reset(); BuildClashChestOpen(); return; }
	if (ClashPopup == EZoogiClashPopup::Replay) { ClashPopupW.Reset(); ClashPopupScrim.Reset(); BuildClashReplay(); return; }
	if (ClashPopup == EZoogiClashPopup::LevelUp) { ClashPopupW.Reset(); ClashPopupScrim.Reset(); BuildClashLevelUp(); return; }
	if (ClashPopup == EZoogiClashPopup::Matchmaking) { ClashPopupW.Reset(); ClashPopupScrim.Reset(); BuildClashMatchmaking(); return; }
	// background dims and blurs behind every popup (DETAILED PASS 1)
	{
		UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass());
		Blur->SetBlurStrength(6.f);
		Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Blur)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f, -400.f, 0.f, -400.f)); }
	}
	// scrim: tap outside closes
	{
		UButton* Scrim = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
		FSlateBrush Dim; Dim.TintColor = FSlateColor(FLinearColor(0.f, 0.01f, 0.06f, 0.62f));
		FButtonStyle St; St.SetNormal(Dim); St.SetHovered(Dim); St.SetPressed(Dim);
		Scrim->SetStyle(St);
		BindClick(Scrim, [this]() { if (ClashBuyT0 < 0.f) { ClashPopup = EZoogiClashPopup::None; bDirty = true; } });
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Scrim)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f, -400.f, 0.f, -400.f)); }
		ClashPopupScrim = Scrim;
	}
	const FVector2D PSz = ClashPopupSize(ClashPopup);
	// M4 portrait: centred in the band between the top bar and the nav (taller popups use the whole column)
	const float PW = FMath::Min(PSz.X, W - 48.f), PH = FMath::Min(PSz.Y, DesignH - 80.f), PX = C - PW * 0.5f;
	const float Band = ClashNavY() - 116.f;
	const float PY = PH <= Band ? 116.f + (Band - PH) * 0.5f : FMath::Max(40.f, (DesignH - PH) * 0.5f);
	UCanvasPanel* Grp = CGroupBegin();
	CPopupBody(0.f, 0.f, PW, PH, 30.f); // light-blue body #4A9CF0 -> #2D74D6, border #1B4FA8 (DETAILED PASS 2)
	const bool bBig = ClashBigPopup(ClashPopup);
	if (bBig)
	{
		// sky-blue illustrated banner (castle + grass) with a coloured title ribbon
		CImg(ClashArt(TEXT("banner")), 14.f, 14.f, PW - 28.f, 168.f, 18.f, true);
		const TCHAR* Rib = ClashPopup == EZoogiClashPopup::TrophyRoad ? TEXT("ribbon_gold") : ClashPopup == EZoogiClashPopup::Arenas ? TEXT("ribbon_blue") : TEXT("ribbon_purple");
		CImg(ClashArt(Rib), PW * 0.5f - 330.f, 96.f, 660.f, 110.f);
	}
	FString Title = ClashInfoTitle;
	if (ClashPopup == EZoogiClashPopup::Profile) Title = TEXT("PROFILE");
	else if (ClashPopup == EZoogiClashPopup::Settings) Title = TEXT("SETTINGS");
	else if (ClashPopup == EZoogiClashPopup::BattleLog) Title = TEXT("BATTLE LOG");
	else if (ClashPopup == EZoogiClashPopup::ChangeNickname) Title = TEXT("CHANGE NICKNAME");
	else if (ClashPopup == EZoogiClashPopup::TrophyRoad) Title = TEXT("TROPHY ROAD");
	if (bBig) CText(Title, PW * 0.5f - 240.f, 98.f, 480.f, 80.f, 46.f, FLinearColor::White);
	else CText(Title, 40.f, 22.f, PW - 80.f, 92.f, Title.Len() > 22 ? 36.f : 48.f, FLinearColor::White, 1, 5.f); // white, navy outline
	CAt(CBox(FLinearColor::White, ZCDark, 3.f, 28.f), PW - 104.f, -26.f, 120.f, 116.f); // close: red #E53E3E, white rim, darker bevel
	CChunky(TEXT("X"), ZCXRed, [this]()
	{
		if (ClashBuyT0 >= 0.f) return;
		// sub-popups close back to their parent (Battle Log -> Profile, Change Nickname -> Settings)
		if (ClashPopup == EZoogiClashPopup::BattleLog) OpenClashPopup(EZoogiClashPopup::Profile);
		else if (ClashPopup == EZoogiClashPopup::ChangeNickname) OpenClashPopup(EZoogiClashPopup::Settings);
		else if (ClashPopup == EZoogiClashPopup::ChooseBadge) OpenClashPopup(EZoogiClashPopup::CreateClan, TEXT("CREATE CLAN"));
		else if (ClashPopup == EZoogiClashPopup::Blocked) OpenClashPopup(EZoogiClashPopup::AddFriend, TEXT("ADD FRIEND"));
		else { ClashPopup = EZoogiClashPopup::None; bDirty = true; }
	}, PW - 96.f, -20.f, 104.f, 100.f, 46.f);

	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	if (BuildClashShopPopup(ClashPopup, PW, PH) || BuildClashHomePopup(ClashPopup, PW, PH) || BuildClashProfilePopup(ClashPopup, PW, PH)
		|| BuildClashPass3Popup(ClashPopup, PW, PH) || BuildClashSocialPopup(ClashPopup, PW, PH)) {}
	else if (ClashPopup == EZoogiClashPopup::Profile) // (replaced by BuildClashProfilePopup; kept as a fallback)
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
	if (Dt <= 0.f) return;
	// milestone 2: spinning buy spinner + light rays, live countdowns, purchases finishing, chests becoming ready
	if (UWidget* Sp = ClashSpinW.Get()) Sp->SetRenderTransformAngle(FMath::Fmod(Time * 360.f, 360.f));
	if (UWidget* R = ClashRaysW.Get()) R->SetRenderTransformAngle(FMath::Fmod(Time * 14.f, 360.f));
	if (ClashBuyT0 >= 0.f && Time - ClashBuyT0 >= 0.8f)
	{
		ClashBuyT0 = -1.f;
		TFunction<void()> Fn = MoveTemp(ClashBuyFn);
		ClashBuyFn = nullptr;
		ClashPopup = EZoogiClashPopup::None;
		bDirty = true;
		if (Fn) Fn();
	}
	if (ClashPendingScroll >= 0.f && ClashScrollApply <= 0) ClashScrollApply = 3;
	if (ClashScrollApply > 0)
		if (UScrollBox* Sc = ClashScrollW.Get())
		{
			// re-applied for a few frames: the scroll box only takes an offset once Slate has built and measured it
			Sc->SetScrollOffset(FMath::Max(0.f, ClashPendingScroll >= 0.f ? ClashPendingScroll : ClashScrollOffset));
			if (ClashPendingScroll >= 0.f) { ClashScrollOffset = ClashPendingScroll; ClashPendingScroll = -1.f; }
			--ClashScrollApply;
		}
	ClashTickSettings(Dt);
	if (ClashPopup == EZoogiClashPopup::Replay && ClashReplay.IsValid())
	{
		if (bClashReplayPlaying)
		{
			ClashReplayT += Dt * ClashReplaySpeed;
			if (ClashReplayT >= ClashReplay->Duration()) { ClashReplayT = ClashReplay->Duration(); bClashReplayPlaying = false; bDirty = true; } // stops at the end
		}
		ClashReplayApply();
	}
	if (ClashCopiedT > 0.f && Time - ClashCopiedT > 1.5f) { ClashCopiedT = -100.f; bDirty = true; } // hide "Copied!"
	if (!ClashToastMsg.IsEmpty() && Time - ClashToastT >= 2.f) { ClashToastMsg.Reset(); bDirty = true; } // hide the toast
	// matchmaking (offline): short search screen, then the REAL match through the same MenuAction the old BATTLE button fired
	if (ClashPopup == EZoogiClashPopup::Matchmaking && ClashMMT0 >= 0.f)
	{
		const float E = Time - ClashMMT0;
		const int32 Ph = E < ClashMMSearch ? 0 : E < ClashMMSearch + ClashMMFound ? 1 : 2;
		if (Ph != ClashMMPhase) { ClashMMPhase = Ph; bDirty = true; UE_LOG(LogZoogi, Log, TEXT("CLASH matchmaking phase %d"), Ph); }
		if (UWidget* Bar = ClashMMBar.Get()) if (UCanvasPanelSlot* BS = Cast<UCanvasPanelSlot>(Bar->Slot)) BS->SetSize(FVector2D((DesignW - 192.f) * ClashMMLoadPct() / 100.f, 48.f));
	}
	if (ClashPopup == EZoogiClashPopup::Matchmaking && ClashMMT0 >= 0.f && Time - ClashMMT0 >= ClashMMSearch + ClashMMFound + ClashMMLoad)
	{
		ClashMMT0 = -1.f;
		ClashPopup = EZoogiClashPopup::None;
		UE_LOG(LogZoogi, Log, TEXT("CLASH matchmaking -> MenuAction Start (local match)"));
		Fire("Start", 0);
		return;
	}
	ClashLiveClock += Dt;
	if (ClashLiveClock >= 0.25f)
	{
		ClashLiveClock = 0.f;
		for (const TPair<TWeakObjectPtr<UTextBlock>, TFunction<FString()>>& L : ClashLive)
			if (UTextBlock* TB = L.Key.Get()) TB->SetText(FText::FromString(L.Value()));
		if (const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this))
		{
			const bool bUnlocking = FP->AnyUnlocking();
			if (bClashChestWasUnlocking && !bUnlocking) { UE_LOG(LogZoogi, Log, TEXT("CLASH chest ready")); bDirty = true; }
			bClashChestWasUnlocking = bUnlocking;
		}
	}
	TickClashTour(Dt);
}

// ------------------------------------------------------------------------------------------------ capture tour
// -ZoogiClashTour=<dir>: engine viewport screenshots of Home, Shop (deals, buying, chests, gold, gems), Cards (decks,
// collection, locked, card popover, emotes) and (milestone 3) Settings / Profile / Battle Log / Replay, named clash_<WxH>_NN_<what>.png. Every purchase goes through the same code
// as a tap. -ZoogiClashTourBattle: then play a real match (P1 switched to CPU so it finishes on its own; rewards counted
// for P1 only with -ZoogiClashRewardsP1), capture the result popup, Home with the rewards, and the chest slot flow
// (-ZoogiChestFast makes unlock timers seconds). -ZoogiClashTourQuit: exit when done.
void UZoogiMenuRoot::TickClashTour(float Dt)
{
	if (ClashTourStep == -1)
	{
		ClashTourStep = FParse::Value(FCommandLine::Get(), TEXT("ZoogiClashTour="), ClashTourDir) ? 0 : -2;
		if (ClashTourStep == 0)
		{
			IFileManager::Get().MakeDirectory(*ClashTourDir, true); UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR -> %s"), *ClashTourDir);
			for (const TCHAR* K : { TEXT("ClashMaster"), TEXT("ClashSfx"), TEXT("ClashMusic") }) ClashTourSaved.Add(K, GetLocalSetting(K, 50));
			ClashTourSaved.Add(TEXT("ClashLanguage"), GetLocalSetting(TEXT("ClashLanguage"), 0));
		}
		ClashTourClock = 0.f;
	}
	if (ClashTourStep < 0) return;
	ClashTourClock += Dt;
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const bool bBattle = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTourBattle"));
	const FString Res = FString::Printf(TEXT("%dx%d"), FMath::RoundToInt(BuiltViewport.X), FMath::RoundToInt(BuiltViewport.Y));
	auto FirstChestSlot = [FP]() { if (FP) for (int32 i = 0; i < FP->Data().Chests.Num(); ++i) if (!FP->Data().Chests[i].Type.IsNone()) return i; return -1; };

	struct FStep { const TCHAR* Name; float Wait; float Shot; TFunction<void()> Setup; };
	TArray<FStep> Steps;
	auto Affordable = [FP](int32 Skip) { if (FP) for (int32 i = 0; i < FP->Data().Deals.Num(); ++i) { const FZoogiFrontDeal& Dl = FP->Data().Deals[i]; if (i != Skip && !Dl.bBought && Dl.Price <= FP->Data().Gold) return i; } return -1; };
	Steps.Add({ TEXT("01_home"), 6.f, 1.5f, [this]() { PopToHome(); SetClashTab(ZCBattle); } });
	// ---- Settings (DETAILED PASS 2): nickname, sliders (real audio), language, account notes, version
	Steps.Add({ TEXT("settings"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::Settings); } });
	Steps.Add({ TEXT("settings_sliders_language"), 1.f, 1.5f, [this]()
	{
		// same values a knob drag / language tap saves + applies; the tour restores the user's values when it ends
		SetLocalSetting(TEXT("ClashMaster"), 80);
		SetLocalSetting(TEXT("ClashMusic"), 30);
		ClashApplyAudio(false);
		SetLocalSetting(TEXT("ClashLanguage"), 2);
		bDirty = true;
	} });
	Steps.Add({ TEXT("change_nickname"), 1.f, 1.5f, [this]() { ClashNickDraft = TEXT("Zoogi Tester"); ClashNickErr.Reset(); OpenClashPopup(EZoogiClashPopup::ChangeNickname); } });
	Steps.Add({ TEXT("nickname_changed"), 1.f, 1.5f, [this, FP]()
	{
		FString Why;
		if (FP && FP->ChangeNickname(TEXT("Zoogi Tester"), Why)) { ClashNickDraft.Reset(); OpenClashPopup(EZoogiClashPopup::Settings); }
		else { ClashNickErr = Why; bDirty = true; }
	} });
	Steps.Add({ TEXT("link_email_note"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::Info, TEXT("LINK EMAIL"), TEXT("Linking an email needs the account backend, which is not connected yet.\nYour progress is saved on this device.")); } });
	Steps.Add({ TEXT("profile"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); } });
	Steps.Add({ TEXT("battle_log_empty"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::BattleLog); } });
	// ---- Premium unlock (gems from -ZoogiDevGrant, spent from the one gem ledger)
	Steps.Add({ TEXT("premium_unlock_popup"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::PremiumUnlock, TEXT("PREMIUM PASS")); } });
	Steps.Add({ TEXT("premium_unlocked"), 1.f, 1.5f, [this, FP]()
	{
		FString Why;
		if (FP && FP->UnlockPremium(Why)) OpenClashPopup(EZoogiClashPopup::BattlePass, TEXT("BATTLE PASS"));
		else ClashFail(Why);
	} });
	// ---- Shop: instant buys, Purchased state, chest buy + full opening, gold, gem packs (needs store)
	Steps.Add({ TEXT("shop_deals"), 1.f, 1.5f, [this]() { SetClashTab(ZCShop); ClashScrollOffset = 0.f; ClashPendingScroll = 0.f; } });
	Steps.Add({ TEXT("deal_popup"), 1.f, 1.5f, [this, FP, Affordable]()
	{
		const int32 Pick = FMath::Max(0, Affordable(-1));
		ClashSel = Pick; ClashSelId = FP && FP->Data().Deals.IsValidIndex(Pick) ? FP->Data().Deals[Pick].CardId : NAME_None;
		OpenClashPopup(EZoogiClashPopup::Deal, ClashSelId.ToString().ToUpper());
	} });
	Steps.Add({ TEXT("deal_bought_in_popup"), 1.f, 1.5f, [this]() { ClashBuyDealNow(ClashSel); } });
	Steps.Add({ TEXT("deal_bought_on_tile"), 1.f, 1.5f, [this, Affordable]() { const int32 I = Affordable(-1); if (I >= 0) ClashBuyDealNow(I); } });
	Steps.Add({ TEXT("shop_chests"), 1.f, 1.5f, [this]() { ClashPendingScroll = ClashShopY[1]; bDirty = true; } });
	Steps.Add({ TEXT("chest_popup_epic"), 1.f, 1.5f, [this]() { ClashSelId = FName(TEXT("epic")); OpenClashPopup(EZoogiClashPopup::Chest, TEXT("EPIC CHEST")); } });
	Steps.Add({ TEXT("chest_open_closed"), 1.f, 1.5f, [this]() { ClashBuyChestNow(FName(TEXT("epic"))); } });
	Steps.Add({ TEXT("chest_open_card1"), 1.f, 1.5f, [this]() { if (ClashPopup == EZoogiClashPopup::ChestOpen) { ++ClashOpenIdx; bDirty = true; } } });
	Steps.Add({ TEXT("chest_open_card2"), 1.f, 1.5f, [this]() { if (ClashPopup == EZoogiClashPopup::ChestOpen && ClashOpenIdx + 1 < ClashOpenCards.Num()) { ++ClashOpenIdx; bDirty = true; } } });
	Steps.Add({ TEXT("chest_open_rewards"), 1.f, 1.5f, [this]() { if (ClashPopup == EZoogiClashPopup::ChestOpen) { ClashOpenIdx = ClashOpenCards.Num(); bDirty = true; } } });
	Steps.Add({ TEXT("shop_chests_after"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; ClashPendingScroll = ClashShopY[1]; bDirty = true; } });
	Steps.Add({ TEXT("shop_gold"), 1.f, 1.5f, [this]() { ClashPendingScroll = ClashShopY[2]; bDirty = true; } });
	Steps.Add({ TEXT("gold_popup"), 1.f, 1.5f, [this]() { ClashSel = 0; OpenClashPopup(EZoogiClashPopup::Gold, UZoogiFrontProfileSubsystem::GoldPacks()[0].Name); } });
	Steps.Add({ TEXT("gold_bought"), 1.f, 1.5f, [this, FP]()
	{
		FString Why;
		if (FP && FP->BuyGold(0, Why)) { ClashPopup = EZoogiClashPopup::None; bDirty = true; } else ClashFail(Why); // same as the popup button
	} });
	Steps.Add({ TEXT("gold_popup_not_enough"), 1.f, 1.5f, [this]() { ClashSel = 2; OpenClashPopup(EZoogiClashPopup::Gold, UZoogiFrontProfileSubsystem::GoldPacks()[2].Name); } });
	Steps.Add({ TEXT("shop_gems"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; ClashPendingScroll = ClashShopY[3]; bDirty = true; } });
	Steps.Add({ TEXT("gem_pack_popup"), 1.f, 1.5f, [this, FP]() { ClashSel = 1; if (FP && FP->GemPacks().IsValidIndex(1)) OpenClashPopup(EZoogiClashPopup::GemPack, FP->GemPacks()[1].Name); } });
	Steps.Add({ TEXT("gem_pack_needs_store"), 1.f, 1.5f, [this, FP]()
	{
		if (!FP || !FP->GemPacks().IsValidIndex(1)) return;
		const UZoogiFrontProfileSubsystem::FGemPack Pk = FP->GemPacks()[1];
		OpenClashPopup(EZoogiClashPopup::Info, TEXT("NEEDS STORE"), FString::Printf(TEXT("%d gems for $%.2f is a real-money pack.\nIt needs the platform store (App Store / Google Play / Steam), which is not connected yet.\nNo purchase was made and no gems were added."), Pk.Gems, Pk.USD));
	} });
	// ---- M4 portrait: every tab + the Home popups
	Steps.Add({ TEXT("arenas"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCBattle); ClashArenaSel = -1; OpenClashPopup(EZoogiClashPopup::Arenas, TEXT("ARENAS")); } });
	Steps.Add({ TEXT("trophy_road"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::TrophyRoad, TEXT("TROPHY ROAD")); } });
	Steps.Add({ TEXT("battle_pass"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::BattlePass, TEXT("BATTLE PASS")); } });
	// ---- DETAILED PASS 3 + 4: Cards (5 decks, 8 slots, name edit, pills, popovers, Details, Level Up, sort, emotes)
	auto OwnedIds = [FP]() { TArray<FName> R; if (FP) for (const FZoogiCardDef& Df : UZoogiFrontProfileSubsystem::CardDefs()) if (FP->IsCardOwned(Df.Id)) R.Add(Df.Id); return R; };
	Steps.Add({ TEXT("cards_decks"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCCards); ClashCardsSub = 0; ClashDeckView = -1; ClashScrollKey = -1; bDirty = true; } });
	Steps.Add({ TEXT("deck_name_edit"), 1.f, 1.5f, [this]() { ClashDeckNameDraft = TEXT("Rush Deck"); bClashDeckNameEdit = true; bDirty = true; } });
	Steps.Add({ TEXT("deck_name_saved"), 1.f, 1.5f, [this, FP]()
	{
		if (FP) FP->SetDeckName(FP->Data().ActiveDeck, TEXT("Rush Deck"));
		bClashDeckNameEdit = false; bDirty = true;
	} });
	Steps.Add({ TEXT("deck_tab_2"), 1.f, 1.5f, [this]() { ClashDeckView = 1; bDirty = true; } });
	Steps.Add({ TEXT("deck_card_popover"), 1.f, 1.5f, [this]() { ClashDeckView = 0; ClashPopoverSlot = 0; ClashPopoverW = 0.f; bDirty = true; } });
	Steps.Add({ TEXT("deck_card_removed_toast"), 1.f, 0.8f, [this, FP]()
	{
		if (FP) { TArray<FName> Dk = FP->Deck(0); if (Dk.Num() > 0) { Dk.RemoveAt(Dk.Num() - 1); FP->SetDeck(0, Dk); } }
		ClashPopoverSlot = -1; ClashToast(TEXT("Card removed from deck."));
	} });
	Steps.Add({ TEXT("cards_collection"), 1.f, 1.5f, [this]() { ClashCardsSub = 1; ClashCollSub = 0; ClashScrollOffset = 0.f; ClashPendingScroll = 0.f; ClashScrollKey = -1; bDirty = true; } });
	Steps.Add({ TEXT("collection_popover"), 1.f, 1.5f, [this, FP, OwnedIds]()
	{
		const TArray<FName> Ids = OwnedIds(); FName Pick = Ids.Num() ? Ids[0] : NAME_None;
		for (const FName& Id : Ids) if (FP && FP->CanUpgrade(Id)) { Pick = Id; break; } // show Upgrade/Use when one is upgradable
		ClashPopoverId = Pick; bDirty = true;
	} });
	Steps.Add({ TEXT("use_card_back_to_decks"), 1.f, 0.8f, [this, FP, OwnedIds]()
	{
		const TArray<FName> Ids = OwnedIds(); const TArray<FName> Dk = FP ? FP->Deck(0) : TArray<FName>();
		for (const FName& Id : Ids) if (!Dk.Contains(Id)) { ClashDeckView = 0; ClashUseCard(Id); break; }
	} });
	Steps.Add({ TEXT("sort_by_elixir_desc"), 1.f, 1.5f, [this, FP]()
	{
		if (FP) { FP->Mutable().CollectionSort = 1; FP->Mutable().bSortDesc = true; FP->Save(); }
		ClashCardsSub = 1; ClashCollSub = 0; ClashPopoverId = NAME_None; ClashScrollKey = -1; bDirty = true;
	} });
	Steps.Add({ TEXT("sort_by_name"), 1.f, 1.5f, [this, FP]() { if (FP) { FP->Mutable().CollectionSort = 2; FP->Mutable().bSortDesc = false; FP->Save(); } bDirty = true; } });
	Steps.Add({ TEXT("sort_by_rarity"), 1.f, 1.5f, [this, FP]() { if (FP) { FP->Mutable().CollectionSort = 0; FP->Mutable().bSortDesc = false; FP->Save(); } bDirty = true; } });
	Steps.Add({ TEXT("cards_locked"), 1.f, 1.5f, [this]() { ClashPendingScroll = ClashLockedY; bDirty = true; } });
	Steps.Add({ TEXT("card_details"), 1.f, 1.5f, [this, OwnedIds, FP]()
	{
		const TArray<FName> Ids = OwnedIds(); FName Pick = Ids.Num() ? Ids[0] : NAME_None;
		for (const FName& Id : Ids) if (FP && FP->CanUpgrade(Id)) { Pick = Id; break; }
		ClashSelId = Pick; ClashDetailsBack = EZoogiClashPopup::None; OpenClashPopup(EZoogiClashPopup::CardDetails, Pick.ToString().ToUpper());
	} });
	Steps.Add({ TEXT("level_up"), 1.f, 1.2f, [this, FP]()
	{
		FString Why; int32 XPGot = 0; bool bUp = false;
		const int32 From = FP ? FP->CardLevelOf(ClashSelId) : 1;
		if (FP && FP->UpgradeCard(ClashSelId, Why, XPGot, bUp))
		{
			ClashLvId = ClashSelId; ClashLvFrom = From; ClashLvXP = XPGot; bClashLvPlayerUp = bUp; ClashDetailsBack = EZoogiClashPopup::CardDetails;
			OpenClashPopup(EZoogiClashPopup::LevelUp, TEXT("LEVEL UP"));
		}
		else OpenClashPopup(EZoogiClashPopup::Info, TEXT("LEVEL UP"), Why);
	} });
	Steps.Add({ TEXT("level_up_done"), 2.f, 1.5f, [this]() { bDirty = true; } });
	Steps.Add({ TEXT("cards_emotes"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCCards); ClashCardsSub = 1; ClashCollSub = 1; ClashScrollKey = -1; bDirty = true; } });
	// ---- Clan / Friends / Leaderboard (offline: honest empty states, local player only)
	Steps.Add({ TEXT("clan_tab"), 1.f, 1.5f, [this]() { ClashSocialSub = 0; SetClashTab(ZCClan); } });
	Steps.Add({ TEXT("search_clans"), 1.f, 1.5f, [this]() { ClashSearchText = TEXT("Zoogi"); ClashSearchNote.Reset(); OpenClashPopup(EZoogiClashPopup::SearchClans, TEXT("SEARCH CLANS")); } });
	Steps.Add({ TEXT("search_clans_offline"), 1.f, 1.5f, [this]() { ClashSearchNote = TEXT("Clan search needs the online backend (not connected yet). No clans to show."); bDirty = true; } });
	Steps.Add({ TEXT("create_clan"), 1.f, 1.5f, [this]() { ClashClanName = TEXT("Zoogi Rollers"); ClashClanTag = TEXT("ZROLL"); ClashClanType = 1; ClashClanReq = 400; OpenClashPopup(EZoogiClashPopup::CreateClan, TEXT("CREATE CLAN")); } });
	Steps.Add({ TEXT("choose_badge"), 1.f, 1.5f, [this]() { ClashBadgeSel = 3; OpenClashPopup(EZoogiClashPopup::ChooseBadge, TEXT("Choose a Badge")); } });
	Steps.Add({ TEXT("friends_tab"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; ClashSocialSub = 1; bDirty = true; } });
	Steps.Add({ TEXT("add_friend"), 1.f, 1.5f, [this]() { ClashSearchNote.Reset(); ClashFriendText.Reset(); OpenClashPopup(EZoogiClashPopup::AddFriend, TEXT("ADD FRIEND")); } });
	Steps.Add({ TEXT("blocked_players"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::Blocked, TEXT("BLOCKED PLAYERS")); } });
	Steps.Add({ TEXT("leaderboard_tab"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; ClashLbSub = 0; SetClashTab(ZCLeader); } });
	Steps.Add({ TEXT("leaderboard_clans"), 1.f, 1.5f, [this]() { ClashLbSub = 1; bDirty = true; } });
	// ---- DETAILED PASS 5: BATTLE with a short deck is blocked
	static TArray<FName> TourSavedDeck; // (the step list is rebuilt every tick, so the saved deck must outlive it)
	TArray<FName>* SavedDeck = &TourSavedDeck;
	Steps.Add({ TEXT("battle_deck_not_full"), 1.f, 1.5f, [this, FP, SavedDeck]()
	{
		ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCBattle);
		if (!FP) return;
		const int32 A = FP->Data().ActiveDeck;
		*SavedDeck = FP->Deck(A);
		TArray<FName> Short = *SavedDeck; if (Short.Num() > 0) Short.RemoveAt(Short.Num() - 1);
		FP->SetDeck(A, Short);
		ClashStartMatchmaking(); // same call as the BATTLE button
	} });
	Steps.Add({ TEXT("home_before_battle"), 1.f, 1.5f, [this, FP, SavedDeck]()
	{
		if (FP && SavedDeck->Num() > 0) FP->SetDeck(FP->Data().ActiveDeck, *SavedDeck); // deck full again
		ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCBattle);
	} });
	// matchmaking search screen (timer counting up, tip box) and CANCEL -> Home, through the same calls as the buttons
	Steps.Add({ TEXT("matchmaking_search"), 1.f, 2.5f, [this]() { ClashPopup = EZoogiClashPopup::None; SetClashTab(ZCBattle); ClashStartMatchmaking(); } });
	Steps.Add({ TEXT("matchmaking_cancelled"), 0.5f, 1.5f, [this]() { ClashCancelMatchmaking(); } });
	const int32 BattleStep = Steps.Num();
	if (bBattle)
	{
		Steps.Add({ TEXT("matchmaking"), 1.f, 1.5f, [this, Res]()
		{
			AZoogiGameMode* G = GM();
			if (G && G->MenuPlayers.IsValidIndex(0) && !G->MenuPlayers[0].bCPU) Fire("ToggleCPU", 0); // capture run: P1 plays itself
			UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR: pressing BATTLE (matchmaking screen, then the real match)"));
			UZoogiWinnerOverlay::TourPrefix = FPaths::Combine(ClashTourDir, FString::Printf(TEXT("clash_%s_%02d"), *Res, ClashTourStep + 1)); // overlay shots + its OK press
			ClashStartMatchmaking();
			const FString P = FPaths::Combine(ClashTourDir, FString::Printf(TEXT("clash_%s_%02d_in_battle.png"), *Res, ClashTourStep + 1));
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [P]() { FScreenshotRequest::RequestScreenshot(P, true, false); UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR screenshot %s"), *P); }), ClashMMSearch + ClashMMFound + ClashMMLoad + 14.f, false);
		} });
		Steps.Add({ TEXT("opponent_found"), ClashMMSearch - 2.f, 1.5f, [this]() { bDirty = true; } });   // ~11 s into matchmaking
		Steps.Add({ TEXT("loading_battle"), 1.f, 1.0f, [this]() { bDirty = true; } });                   // ~13 s: logo loading, then the real match
		Steps.Add({ TEXT("home_after_match"), 2.f, 2.2f, [this]() { bDirty = true; } });                 // Winner overlay -> OK -> loading -> Home (no results popup)
		Steps.Add({ TEXT("slot_locked"), 1.f, 1.5f, [this]()
		{
			ClashPopup = EZoogiClashPopup::None; bDirty = true;
			if (AZoogiGameMode* G = GM()) if (G->MenuPlayers.IsValidIndex(0) && G->MenuPlayers[0].bCPU) Fire("ToggleCPU", 0); // P1 back to human
		} });
		Steps.Add({ TEXT("slot_popup"), 1.f, 1.5f, [this, FirstChestSlot]() { const int32 S = FirstChestSlot(); if (S >= 0) ClashTapChestSlot(S); } });
		Steps.Add({ TEXT("slot_unlocking"), 1.f, 1.5f, [this, FP, FirstChestSlot]()
		{
			const int32 S = FirstChestSlot(); FString Why;
			if (FP && S >= 0 && FP->StartUnlock(S, Why)) bClashChestWasUnlocking = true;
			ClashPopup = EZoogiClashPopup::None; bDirty = true;
		} });
		const int32 Slot0 = FirstChestSlot();
		const float WaitReady = (FP && Slot0 >= 0 && FP->SecondsLeft(Slot0) > 0) ? 1e9f : 1.5f; // hold until the chest is really ready
		Steps.Add({ TEXT("slot_ready"), WaitReady, 1.0f, [this]() { bDirty = true; } });
		Steps.Add({ TEXT("slot_open"), 1.f, 1.5f, [this, FirstChestSlot]() { const int32 S = FirstChestSlot(); if (S >= 0) ClashTapChestSlot(S); } });
		Steps.Add({ TEXT("slot_open_card1"), 1.f, 1.5f, [this]() { if (ClashPopup == EZoogiClashPopup::ChestOpen) { ++ClashOpenIdx; bDirty = true; } } });
		Steps.Add({ TEXT("slot_rewards"), 1.f, 1.5f, [this]() { if (ClashPopup == EZoogiClashPopup::ChestOpen) { ClashOpenIdx = ClashOpenCards.Num(); bDirty = true; } } });
		Steps.Add({ TEXT("home_after_chest"), 1.f, 1.5f, [this]() { ClashPopup = EZoogiClashPopup::None; bDirty = true; } });
		Steps.Add({ TEXT("profile_after_battle"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::Profile); } });
		Steps.Add({ TEXT("battle_log"), 1.f, 1.5f, [this]() { OpenClashPopup(EZoogiClashPopup::BattleLog); } });
		Steps.Add({ TEXT("replay_1x"), 1.f, 3.0f, [this]() { ClashOpenReplay(0); } });
		Steps.Add({ TEXT("replay_4x"), 0.2f, 2.5f, [this]() { ClashReplaySpeed = 4.f; bDirty = true; } });
		Steps.Add({ TEXT("replay_paused"), 0.2f, 1.0f, [this]() { bClashReplayPlaying = false; bDirty = true; } });
		Steps.Add({ TEXT("home_final"), 1.f, 1.5f, [this]() { ClashReplay.Reset(); ClashPopup = EZoogiClashPopup::None; bDirty = true; } });
	}

	int32 MaxSteps = Steps.Num();
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiClashTourSteps="), MaxSteps)) MaxSteps = FMath::Clamp(MaxSteps, 1, Steps.Num()); // short capture runs
	if (ClashTourStep >= MaxSteps)
	{
		ClashTourStep = -100;
		UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR done (%d steps)"), Steps.Num());
		// put the user's own Settings values back (the tour moved sliders / language to show them working)
		for (const TPair<FString, int32>& KV : ClashTourSaved) SetLocalSetting(*KV.Key, KV.Value);
		ClashApplyAudio(false);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTourQuit")))
		{
			TWeakObjectPtr<UZoogiMenuRoot> WeakThis(this);
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
			{
				if (UZoogiMenuRoot* M = WeakThis.Get()) { UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR quit")); UKismetSystemLibrary::QuitGame(M, M->GetOwningPlayer(), EQuitPreference::Quit, false); }
			}), 3.f, false);
		}
		return;
	}
	FStep& St = Steps[ClashTourStep];
	if (!bClashTourSetup)
	{
		if (ClashTourClock < St.Wait) return;
		bClashTourSetup = true;
		ClashTourClock = 0.f;
		UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR step %s"), St.Name);
		if (St.Setup) St.Setup();
		return;
	}
	if (ClashTourClock < St.Shot) return;
	// a battle step whose match lost: no chest was won. Retry the real match (max 3 battles) so the chest flow can be shown.
	static int32 TourBattles = 0;
	if (ClashTourStep == BattleStep + 4 && FP && FirstChestSlot() < 0 && ++TourBattles < 4)
	{
		UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR: no chest won (outcome %d), playing another real match"), FP->Data().LastMatch.Outcome);
		ClashPopup = EZoogiClashPopup::None;
		ClashTourStep = BattleStep; bClashTourSetup = false; ClashTourClock = 0.f;
		return;
	}
	FString ShotName(St.Name);
	if (ShotName.Len() > 3 && FChar::IsDigit(ShotName[0]) && FChar::IsDigit(ShotName[1]) && ShotName[2] == TEXT('_')) ShotName = ShotName.Mid(3);
	const FString P = FPaths::Combine(ClashTourDir, FString::Printf(TEXT("clash_%s_%02d_%s.png"), *Res, ClashTourStep + 1, *ShotName));
	FScreenshotRequest::RequestScreenshot(P, true, false);
	UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR screenshot %s (tab %s popup %d screen %d gold %d gems %d)"), *P, ZCTabName(ClashTab), (int32)ClashPopup, (int32)Top(),
		FP ? FP->Data().Gold : -1, FP ? FP->Gems() : -1);
	bClashTourSetup = false;
	ClashTourClock = 0.f;
	++ClashTourStep;
}
