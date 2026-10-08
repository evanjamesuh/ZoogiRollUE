#include "ZoogiEpisodeCalloutWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
	FSlateFontInfo EpFont(float Size, bool bBold)
	{
		UFont* F = GEngine->GetLargeFont();
		FSlateFontInfo Fi = F->GetLegacySlateFontInfo();
		Fi.Size = Size;
		if (bBold) Fi.TypefaceFontName = FName(TEXT("Bold"));
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
	BannerBox->SetBrush(EpSolid(FLinearColor(0.02f, 0.03f, 0.08f, 0.78f), FLinearColor::White, 2.f, 8.f));
	BannerBox->SetPadding(FMargin(36.f, 10.f));
	BannerBox->SetHorizontalAlignment(HAlign_Center);
	BannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BannerText"));
	BannerText->SetFont(EpFont(36.f, true));
	BannerText->SetJustification(ETextJustify::Center);
	BannerBox->SetContent(BannerText);
	BannerBox->SetRenderOpacity(0.f);
	Anchor(Root, BannerBox, FAnchors(0.5f, 0.74f), FVector2D(0.5f, 0.5f), FMargin(0.f), true);
	BannerSub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BannerSub"));
	BannerSub->SetFont(EpFont(18.f, false));
	BannerSub->SetShadowOffset(FVector2D(2.f, 2.f));
	BannerSub->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	BannerSub->SetJustification(ETextJustify::Center);
	BannerSub->SetRenderOpacity(0.f);
	Anchor(Root, BannerSub, FAnchors(0.5f, 0.74f), FVector2D(0.5f, 0.5f), FMargin(0.f, 46.f, 0.f, 0.f), true);

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
	BannerBox->SetBrush(EpSolid(FLinearColor(0.02f, 0.03f, 0.08f, 0.8f), Color, 2.f, 8.f));
	BannerAge = 0.f;
	BannerLife = Seconds;
}

void UZoogiEpisodeCalloutWidget::TickEpisode(float Dt)
{
	if (!Root) return;
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
		BannerBox->SetRenderScale(FVector2D(FMath::Lerp(1.25f, 1.f, EaseOutBack(T / 0.25f))));
	}
}
