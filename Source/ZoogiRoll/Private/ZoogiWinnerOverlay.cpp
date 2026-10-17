// clash-menu DETAILED PASS 5: "Winner!" overlay -> OK -> loading -> Home. See ZoogiWinnerOverlay.h.
#include "ZoogiWinnerOverlay.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuWidget.h" // UZoogiButton (non-focusable)
#include "ZoogiRoll.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

FString UZoogiWinnerOverlay::TourPrefix;

namespace
{
	constexpr float ZWDesign = 1000.f;  // square design canvas: ScaleToFit keeps it whole in landscape and portrait
	constexpr float ZWLoadSecs = 1.6f;  // 0 -> 100 %
	constexpr float ZWLoadHold = 0.4f;  // holds at 100 % briefly (spec), then Home
}

UZoogiWinnerOverlay* UZoogiWinnerOverlay::Show(UWorld* World, int32 InOutcome, const FString& InWinner, int32 InMy, int32 InOpp)
{
	if (!World) return nullptr;
	APlayerController* PC = World->GetFirstPlayerController();
	UZoogiWinnerOverlay* O = PC ? CreateWidget<UZoogiWinnerOverlay>(PC, UZoogiWinnerOverlay::StaticClass()) : CreateWidget<UZoogiWinnerOverlay>(World, UZoogiWinnerOverlay::StaticClass());
	if (!O) return nullptr;
	O->Outcome = InOutcome; O->WinnerName = InWinner; O->MyScore = InMy; O->OppScore = InOpp;
	O->AddToViewport(1000); // above the battle HUD
	UE_LOG(LogZoogi, Log, TEXT("CLASH winner overlay: outcome %d winner '%s' %d-%d"), InOutcome, *InWinner, InMy, InOpp);
	return O;
}

TSharedRef<SWidget> UZoogiWinnerOverlay::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Build();
	return Super::RebuildWidget();
}

UWidget* UZoogiWinnerOverlay::At(UWidget* W, float X, float Y, float Wd, float Ht)
{
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W)) { S->SetPosition(FVector2D(X, Y)); S->SetSize(FVector2D(Wd, Ht)); }
	return W;
}

UTextBlock* UZoogiWinnerOverlay::Txt(const FString& S, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, float Outline)
{
	UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TB->SetText(FText::FromString(S));
	TB->SetFont(ZCFont(Size, Outline));
	TB->SetColorAndOpacity(FSlateColor(C));
	TB->SetJustification(ETextJustify::Center);
	TB->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(TB, X, Y + (Ht - Size * 1.35f) * 0.5f, Wd, Size * 1.4f);
	return TB;
}

UImage* UZoogiWinnerOverlay::Img(UTexture2D* Tex, float X, float Y, float Wd, float Ht, const FLinearColor& Tint)
{
	UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	I->SetBrush(ZCImgBrush(Tex, Wd, Ht, 0.f, false));
	I->SetColorAndOpacity(Tint);
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(I, X, Y, Wd, Ht);
	return I;
}

UTexture2D* UZoogiWinnerOverlay::LoadArt(const FString& Name)
{
	for (const TCHAR* Ext : { TEXT(".png"), TEXT(".jpg") })
	{
		const FString P = FPaths::ProjectContentDir() / TEXT("Data/MenuArt") / (Name + Ext);
		if (FPaths::FileExists(P)) if (UTexture2D* Tx = FImageUtils::ImportFileAsTexture2D(P)) { Keep.Add(Tx); return Tx; }
	}
	return nullptr;
}

UTexture2D* UZoogiWinnerOverlay::Crown()
{
	// the same procedural crown the Clash menu draws (the menu root stays alive, hidden, during a match)
	for (TObjectIterator<UZoogiMenuRoot> It; It; ++It)
		if (It->GetWorld() == GetWorld()) if (UTexture2D* Tx = It->SharedClashArt(TEXT("crown"))) { Keep.Add(Tx); return Tx; }
	return nullptr;
}

void UZoogiWinnerOverlay::Build()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("WinnerRoot"));
	WidgetTree->RootWidget = Root;
	auto Fill = [Root](UWidget* W) { if (UOverlaySlot* S = Root->AddChildToOverlay(W)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); } };
	// dim the battle and swallow taps (only OK does anything)
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(FLinearColor(0.f, 0.02f, 0.08f, 0.62f));
	Dim->SetVisibility(ESlateVisibility::Visible);
	Fill(Dim);
	// loading key art (full bleed, hidden until OK)
	UImage* Bg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (UTexture2D* Tx = LoadArt(TEXT("meadows_background"))) Bg->SetBrush(ZCImgBrush(Tx, 1920.f, 1080.f, 0.f, false));
	Bg->SetVisibility(ESlateVisibility::Collapsed);
	Fill(Bg);
	LoadingBg = Bg;
	UScaleBox* SB = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	SB->SetStretch(EStretch::ScaleToFit);
	USizeBox* SzBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SzBox->SetWidthOverride(ZWDesign); SzBox->SetHeightOverride(ZWDesign);
	SB->AddChild(SzBox);
	UCanvasPanel* Top = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	SzBox->AddChild(Top);
	if (UOverlaySlot* S = Root->AddChildToOverlay(SB)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	SB->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Top->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	auto Group = [this, Top]() -> UCanvasPanel*
	{
		UCanvasPanel* G = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		G->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* S = Top->AddChildToCanvas(G)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); }
		return G;
	};
	const float C = ZWDesign * 0.5f;
	// ---- winner view
	Canvas = Group();
	WinnerGroup = Canvas;
	UTexture2D* CrownTx = Crown();
	const bool bDraw = Outcome == 2;
	const FLinearColor Blue = ZCHex(TEXT("2F6BD0")), Red = ZCHex(TEXT("C2362F"));
	if (CrownTx) Img(CrownTx, C - 120.f, 70.f, 240.f, 240.f, bDraw ? FLinearColor(0.7f, 0.75f, 0.85f, 1.f) : FLinearColor::White);
	Txt(bDraw ? TEXT("Draw!") : TEXT("Winner!"), 0.f, 300.f, ZWDesign, 140.f, 104.f, ZCYellow, 7.f);
	{
		UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush B; B.DrawAs = ESlateBrushDrawType::RoundedBox; B.TintColor = FSlateColor(bDraw ? ZCHex(TEXT("5E6878")) : (Outcome == 0 ? Blue : Red));
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(26.f, 26.f, 26.f, 26.f), FSlateColor(ZCDark), 5.f);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Plate->SetBrush(B);
		Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
		At(Plate, C - 400.f, 460.f, 800.f, 130.f);
		Txt(bDraw ? FString(TEXT("No winner")) : WinnerName, C - 390.f, 460.f, 780.f, 130.f, WinnerName.Len() > 18 ? 40.f : 54.f, FLinearColor::White, 5.f);
	}
	// crown tally: blue = you, red = opponent (Zoogi Roll scores points; shown as the crowns, as in the battle log)
	if (CrownTx) { Img(CrownTx, C - 300.f, 630.f, 110.f, 110.f, ZCLighten(Blue, 0.35f)); Img(CrownTx, C + 190.f, 630.f, 110.f, 110.f, ZCLighten(Red, 0.35f)); }
	Txt(FString::FromInt(MyScore), C - 190.f, 630.f, 120.f, 110.f, 70.f, ZCLighten(Blue, 0.55f), 6.f);
	Txt(TEXT("-"), C - 60.f, 630.f, 120.f, 110.f, 70.f, FLinearColor::White, 6.f);
	Txt(FString::FromInt(OppScore), C + 70.f, 630.f, 120.f, 110.f, 70.f, ZCLighten(Red, 0.55f), 6.f);
	{
		UZoogiButton* Ok = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
		FSlateBrush N; N.DrawAs = ESlateBrushDrawType::RoundedBox; N.TintColor = FSlateColor(ZCHex(TEXT("3FA2FF")));
		N.OutlineSettings = FSlateBrushOutlineSettings(FVector4(28.f, 28.f, 28.f, 28.f), FSlateColor(ZCDark), 5.f);
		N.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		FSlateBrush P = N; P.TintColor = FSlateColor(ZCHex(TEXT("2F86E0")));
		FButtonStyle St; St.SetNormal(N); St.SetHovered(N); St.SetPressed(P);
		Ok->SetStyle(St);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		L->SetText(FText::FromString(TEXT("OK"))); L->SetFont(ZCFont(60.f, 5.f)); L->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Ok->AddChild(L);
		Ok->OnClicked.AddDynamic(this, &UZoogiWinnerOverlay::HandleOk);
		At(Ok, C - 170.f, 800.f, 340.f, 130.f);
	}
	// ---- loading view (logo, "Loading..", %, yellow bar)
	Canvas = Group();
	LoadingGroup = Canvas;
	if (UTexture2D* Logo = LoadArt(TEXT("zoogi-roll-arena-logo"))) Img(Logo, C - 380.f, 140.f, 760.f, 760.f * Logo->GetSizeY() / FMath::Max(1, Logo->GetSizeX()));
	Txt(TEXT("Loading.."), 100.f, 700.f, 500.f, 70.f, 44.f, FLinearColor::White, 5.f)->SetJustification(ETextJustify::Left);
	PctText = Txt(TEXT("0%"), 600.f, 700.f, 300.f, 70.f, 44.f, FLinearColor::White, 5.f);
	PctText->SetJustification(ETextJustify::Right);
	{
		UBorder* Track = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Track->SetBrushColor(ZCHex(TEXT("0B1E44")).CopyWithNewOpacity(0.85f));
		Track->SetVisibility(ESlateVisibility::HitTestInvisible);
		At(Track, 100.f, 790.f, 800.f, 54.f);
		UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bar->SetBrushColor(ZCYellow);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		At(Bar, 106.f, 796.f, 0.f, 42.f);
		BarFill = Bar;
	}
	LoadingGroup->SetVisibility(ESlateVisibility::Collapsed);
}

void UZoogiWinnerOverlay::HandleOk()
{
	if (LoadT >= 0.f) return;
	LoadT = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("CLASH winner overlay OK -> loading -> Home"));
	if (WinnerGroup) WinnerGroup->SetVisibility(ESlateVisibility::Collapsed);
	if (LoadingGroup) LoadingGroup->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (LoadingBg) LoadingBg->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UZoogiWinnerOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	T += InDeltaTime;
	auto Shot = [this](const TCHAR* What)
	{
		const FString P = TourPrefix + TEXT("_") + What + TEXT(".png");
		FScreenshotRequest::RequestScreenshot(P, true, false);
		UE_LOG(LogZoogi, Log, TEXT("CLASH TOUR screenshot %s"), *P);
	};
	if (!TourPrefix.IsEmpty())
	{
		// capture runs: screenshot the overlay, press OK like a player, screenshot the loading screen
		if (TourShots == 0 && T >= 1.5f) { ++TourShots; Shot(TEXT("winner_overlay")); }
		else if (TourShots == 1 && T >= 2.5f) { ++TourShots; HandleOk(); }
		else if (TourShots == 2 && LoadT >= 0.9f) { ++TourShots; Shot(TEXT("loading_to_home")); }
	}
	if (LoadT < 0.f || bReturned) return;
	LoadT += InDeltaTime;
	const float A = FMath::Clamp(LoadT / ZWLoadSecs, 0.f, 1.f);
	if (UTextBlock* P = PctText.Get()) P->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(A * 100.f))));
	if (UWidget* B = BarFill.Get()) if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(B->Slot)) S->SetSize(FVector2D(788.f * A, 42.f));
	if (LoadT >= ZWLoadSecs + ZWLoadHold)
	{
		bReturned = true;
		UWorld* W = GetWorld();
		if (AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr) G->MenuAction(TEXT("Menu"), 0); // same as the battle's MENU: back to Home
		RemoveFromParent();
	}
}
