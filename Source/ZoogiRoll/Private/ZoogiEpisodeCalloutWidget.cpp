#include "ZoogiEpisodeCalloutWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "ZoogiUIStyle.h"

namespace
{
	FSlateFontInfo EpFont(float Size, bool bBold)
	{
		UFont* F = GEngine->GetLargeFont();
		FSlateFontInfo Fi = F->GetLegacySlateFontInfo();
		Fi.Size = Size;
		if (bBold) Fi.TypefaceFontName = FName(TEXT("Bold"));
		ZoogiUIStyle::AddOutline(Fi, Size >= 30.f ? 3.f : 2.f); // uistyle: navy text outline
		return Fi;
	}

	FSlateBrush EpSolid(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineW = 0.f, float Radius = 6.f)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineW);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	UCanvasPanelSlot* Anchor(UCanvasPanel* P, UWidget* W, const FAnchors& A, const FVector2D& Align, const FMargin& Offsets, bool bAutoSize = false)
	{
		UCanvasPanelSlot* S = P->AddChildToCanvas(W);
		S->SetAnchors(A);
		S->SetAlignment(Align);
		S->SetOffsets(Offsets);
		S->SetAutoSize(bAutoSize);
		return S;
	}

	float EaseOutBack(float T) { const float C1 = 1.70158f, C3 = C1 + 1.f; T = FMath::Clamp(T, 0.f, 1.f); return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f); }
	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T); }

}

TSharedRef<SWidget> UZoogiEpisodeCalloutWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EpisodeRoot"));
	WidgetTree->RootWidget = Root;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	CalloutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Callout"));
	CalloutText->SetFont(EpFont(44.f, true));
	CalloutText->SetShadowOffset(FVector2D(3.f, 3.f));
	CalloutText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.75f));
	CalloutText->SetJustification(ETextJustify::Center);
	CalloutText->SetRenderOpacity(0.f);
	Anchor(Root, CalloutText, FAnchors(0.5f, 0.2f), FVector2D(0.5f, 0.5f), FMargin(0.f), true);

	CalloutSub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CalloutSub"));
	CalloutSub->SetFont(EpFont(20.f, false));
	CalloutSub->SetShadowOffset(FVector2D(2.f, 2.f));
	CalloutSub->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	CalloutSub->SetJustification(ETextJustify::Center);
	CalloutSub->SetRenderOpacity(0.f);
	Anchor(Root, CalloutSub, FAnchors(0.5f, 0.2f), FVector2D(0.5f, 0.5f), FMargin(0.f, 48.f, 0.f, 0.f), true);

	BannerBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Banner"));
	BannerBox->SetBrush(EpSolid(ZoogiUIStyle::PanelNavy, ZoogiUIStyle::Outline, 4.f, 14.f)); // uistyle: saturated navy, navy outline
	BannerBox->SetPadding(FMargin(36.f, 10.f));
	BannerBox->SetHorizontalAlignment(HAlign_Center);
	BannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BannerText"));
	BannerText->SetFont(EpFont(36.f, true));
	BannerText->SetJustification(ETextJustify::Center);
	BannerBox->SetContent(BannerText);
	BannerBox->SetRenderOpacity(0.f);
	// Evan Oct 14 1:40 AM CT: the idle (empty, opacity-0) banner frame still showed as a "random square" at screen centre
	// (anchor 0.5, 0.74). It is only a presentation frame, so it stays Collapsed except while a banner is actually showing.
	BannerBox->SetVisibility(ESlateVisibility::Collapsed);
	Anchor(Root, BannerBox, FAnchors(0.5f, 0.74f), FVector2D(0.5f, 0.5f), FMargin(0.f), true);
	BannerSub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BannerSub"));
	BannerSub->SetFont(EpFont(18.f, false));
	BannerSub->SetShadowOffset(FVector2D(2.f, 2.f));
	BannerSub->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	BannerSub->SetJustification(ETextJustify::Center);
	BannerSub->SetRenderOpacity(0.f);
	Anchor(Root, BannerSub, FAnchors(0.5f, 0.74f), FVector2D(0.5f, 0.5f), FMargin(0.f, 46.f, 0.f, 0.f), true);

	// fallback panels: framed live views, clear of the centre of the screen and of the HUD corners
	const FVector2D PanelAnchor[3] = { FVector2D(0.16f, 0.47f), FVector2D(0.84f, 0.47f), FVector2D(0.15f, 0.73f) };
	for (int32 i = 0; i < 3; ++i)
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(EpSolid(FLinearColor(0.96f, 0.96f, 1.f, 1.f), FLinearColor::Transparent, 0.f, 4.f));
		Frame->SetPadding(FMargin(4.f));
		UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Ov->AddChildToOverlay(Img);
		UTextBlock* Cap = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Cap->SetFont(EpFont(15.f, true));
		Cap->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Cap->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		if (UOverlaySlot* CS = Ov->AddChildToOverlay(Cap)) { CS->SetHorizontalAlignment(HAlign_Left); CS->SetVerticalAlignment(VAlign_Bottom); CS->SetPadding(FMargin(8.f, 0.f, 0.f, 6.f)); }
		Frame->SetContent(Ov);
		Frame->SetRenderOpacity(0.f);
		Frame->SetVisibility(ESlateVisibility::Collapsed);
		Anchor(Root, Frame, FAnchors(PanelAnchor[i].X, PanelAnchor[i].Y), FVector2D(0.5f, 0.5f), FMargin(0.f), true);
		PanelFrames.Add(Frame);
		PanelImages.Add(Img);
		PanelCaps.Add(Cap);
	}

	return Super::RebuildWidget();
}

void UZoogiEpisodeCalloutWidget::ShowCallout(const FString& Main, const FString& Sub, const FLinearColor& Color, int32 Tier)
{
	if (!CalloutText) return;
	CalloutText->SetText(FText::FromString(Main));
	CalloutSub->SetText(FText::FromString(Sub));
	CalloutText->SetFont(EpFont(Tier >= 2 ? 50.f : 40.f, true));
	CalloutColor = Color;
	CalloutTier = Tier;
	CalloutAge = 0.f;
	CalloutLife = Tier >= 2 ? 1.5f : 1.1f;
}

void UZoogiEpisodeCalloutWidget::ShowBanner(const FString& Text, const FString& Sub, const FLinearColor& Color, float Seconds)
{
	if (!BannerText) return;
	BannerText->SetText(FText::FromString(Text));
	BannerText->SetColorAndOpacity(FSlateColor(Color));
	BannerSub->SetText(FText::FromString(Sub));
	BannerBox->SetBrush(EpSolid(FLinearColor(Color.R * 0.25f + 0.02f, Color.G * 0.25f + 0.08f, Color.B * 0.25f + 0.3f, 0.88f), ZoogiUIStyle::Outline, 4.f, 14.f)); // uistyle
	BannerAge = 0.f;
	BannerLife = Seconds;
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UZoogiEpisodeCalloutWidget::ShowPanel(int32 PanelSlot, UTexture* Tex, float Seconds, const FString& Caption, const FLinearColor& Frame, bool bSmall)
{
	if (!PanelFrames.IsValidIndex(PanelSlot) || !Tex) return;
	FSlateBrush B;
	B.SetResourceObject(Tex);
	B.ImageSize = bSmall ? FVector2D(288.f, 162.f) : FVector2D(400.f, 225.f);
	B.DrawAs = ESlateBrushDrawType::Image;
	PanelImages[PanelSlot]->SetBrush(B);
	PanelFrames[PanelSlot]->SetBrushColor(Frame);
	PanelCaps[PanelSlot]->SetText(FText::FromString(Caption));
	PanelCaps[PanelSlot]->SetColorAndOpacity(FSlateColor(Frame));
	PanelFrames[PanelSlot]->SetVisibility(ESlateVisibility::HitTestInvisible);
	PanelAge[PanelSlot] = 0.f;
	PanelLife[PanelSlot] = FMath::Max(0.4f, Seconds);
}

void UZoogiEpisodeCalloutWidget::TickEpisode(float Dt)
{
	if (!Root) return;
	for (int32 i = 0; i < PanelFrames.Num() && i < 3; ++i)
	{
		PanelAge[i] += Dt;
		const float T = PanelAge[i], L = PanelLife[i];
		UBorder* F = PanelFrames[i];
		if (T >= L) { if (F->GetVisibility() != ESlateVisibility::Collapsed) F->SetVisibility(ESlateVisibility::Collapsed); continue; }
		const float In = FMath::Clamp(T / 0.22f, 0.f, 1.f), Out = FMath::Clamp((L - T) / 0.2f, 0.f, 1.f);
		const float Side = i == 1 ? 1.f : -1.f;
		FWidgetTransform X;
		X.Translation = FVector2D(Side * 500.f * (1.f - EaseOutBack(In)), 0.f);
		X.Scale = FVector2D(1.f, FMath::Lerp(0.1f, 1.f, Out));                // collapse out
		X.Angle = Side * 10.f * (1.f - EaseOut(In));
		F->SetRenderTransform(X);
		F->SetRenderOpacity(FMath::Min(In * 3.f, 1.f) * Out);
	}
	// callout pop: scale 1.5 -> 1 with a back-ease, hold, fade
	CalloutAge += Dt;
	{
		const float T = CalloutAge;
		float A = 0.f, S = 1.f;
		if (T < CalloutLife)
		{
			A = T < 0.08f ? T / 0.08f : (T > CalloutLife - 0.3f ? (CalloutLife - T) / 0.3f : 1.f);
			S = FMath::Lerp(CalloutTier >= 2 ? 1.7f : 1.45f, 1.f, EaseOutBack(T / 0.22f));
		}
		CalloutText->SetRenderOpacity(A);
		CalloutText->SetRenderScale(FVector2D(S));
		CalloutText->SetColorAndOpacity(FSlateColor(CalloutColor));
		CalloutText->SetRenderTransformAngle(CalloutTier >= 2 ? FMath::Lerp(-5.f, -2.f, EaseOut(T / 0.25f)) : 0.f);
		CalloutSub->SetRenderOpacity(A * 0.95f);
	}
	// result banner
	BannerAge += Dt;
	{
		const float T = BannerAge;
		const float A = T < BannerLife ? (T < 0.12f ? T / 0.12f : (T > BannerLife - 0.3f ? (BannerLife - T) / 0.3f : 1.f)) : 0.f;
		BannerBox->SetRenderOpacity(A);
		BannerSub->SetRenderOpacity(A);
		const ESlateVisibility BV = A > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (BannerBox->GetVisibility() != BV) BannerBox->SetVisibility(BV);
		BannerBox->SetRenderScale(FVector2D(FMath::Lerp(1.25f, 1.f, EaseOutBack(T / 0.25f))));
	}
}
