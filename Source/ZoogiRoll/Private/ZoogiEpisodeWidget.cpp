#include "ZoogiEpisodeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"

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

	constexpr int32 MaxPanels = 4;
	const FVector2D PanelSize(420.f, 236.f);
}

TSharedRef<SWidget> UZoogiEpisodeWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EpisodeRoot"));
	WidgetTree->RootWidget = Root;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	const FAnchors Full(0.f, 0.f, 1.f, 1.f);
	DarkImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Dark"));
	DarkImg->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.02f, 0.f));
	Anchor(Root, DarkImg, Full, FVector2D::ZeroVector, FMargin(0.f));

	TintImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Tint"));
	TintImg->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	Anchor(Root, TintImg, Full, FVector2D::ZeroVector, FMargin(0.f));

	// power frame: four edge bars
	const FAnchors EdgeA[4] = { FAnchors(0.f, 0.f, 1.f, 0.f), FAnchors(0.f, 1.f, 1.f, 1.f), FAnchors(0.f, 0.f, 0.f, 1.f), FAnchors(1.f, 0.f, 1.f, 1.f) };
	const FVector2D EdgeAlign[4] = { FVector2D(0.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, 0.f), FVector2D(1.f, 0.f) };
	const FMargin EdgeOff[4] = { FMargin(0.f, 0.f, 0.f, 14.f), FMargin(0.f, 0.f, 0.f, 14.f), FMargin(0.f, 0.f, 14.f, 0.f), FMargin(0.f, 0.f, 14.f, 0.f) };
	for (int32 i = 0; i < 4; ++i)
	{
		UImage* Bar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Bar->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
		Anchor(Root, Bar, EdgeA[i], EdgeAlign[i], EdgeOff[i]);
		EdgeBars.Add(Bar);
	}

	for (int32 i = 0; i < MaxPanels; ++i)
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(EpSolid(FLinearColor(0.95f, 0.95f, 1.f, 1.f), FLinearColor::Transparent, 0.f, 4.f));
		Frame->SetPadding(FMargin(5.f));
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Frame->SetContent(Img);
		Frame->SetRenderOpacity(0.f);
		Frame->SetVisibility(ESlateVisibility::Collapsed);
		Anchor(Root, Frame, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FMargin(0.f, 0.f, PanelSize.X, PanelSize.Y));
		PanelFrames.Add(Frame);
		PanelImages.Add(Img);
	}

	PowerTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PowerTitle"));
	PowerTitle->SetFont(EpFont(24.f, true));
	PowerTitle->SetShadowOffset(FVector2D(2.f, 2.f));
	PowerTitle->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	PowerTitle->SetJustification(ETextJustify::Center);
	PowerTitle->SetRenderOpacity(0.f);
	Anchor(Root, PowerTitle, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FMargin(0.f, 104.f, 0.f, 0.f), true); // below the HUD's round / phase bars

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

	FlashImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Flash"));
	FlashImg->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.f));
	Anchor(Root, FlashImg, Full, FVector2D::ZeroVector, FMargin(0.f));

	return Super::RebuildWidget();
}

void UZoogiEpisodeWidget::ShowCallout(const FString& Main, const FString& Sub, const FLinearColor& Color, int32 Tier)
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

void UZoogiEpisodeWidget::ShowPanels(EZoogiPanelLayout InLayout, const TArray<UTextureRenderTarget2D*>& Targets, const FLinearColor& Frame, float Seconds)
{
	if (PanelFrames.Num() == 0) return;
	Layout = InLayout;
	PanelHold = Seconds;
	PanelSlot.Reset(); PanelFrom.Reset(); PanelAngle.Reset();
	// Slots keep the centre of the screen free: the live Zoogi stays centre-frame.
	switch (Layout)
	{
	case EZoogiPanelLayout::CornerPair:
		PanelSlot = { FVector2D(0.17f, 0.24f), FVector2D(0.83f, 0.76f) };
		PanelFrom = { FVector2D(-900.f, -200.f), FVector2D(900.f, 200.f) };
		PanelAngle = { -14.f, 14.f };
		break;
	case EZoogiPanelLayout::SidePair:
		PanelSlot = { FVector2D(0.15f, 0.5f), FVector2D(0.85f, 0.5f) };
		PanelFrom = { FVector2D(-900.f, 0.f), FVector2D(900.f, 0.f) };
		PanelAngle = { -8.f, 8.f };
		break;
	case EZoogiPanelLayout::Trio:
		PanelSlot = { FVector2D(0.15f, 0.56f), FVector2D(0.85f, 0.56f), FVector2D(0.5f, 0.15f) };
		PanelFrom = { FVector2D(-900.f, 120.f), FVector2D(900.f, 120.f), FVector2D(0.f, -600.f) };
		PanelAngle = { -12.f, 12.f, 0.f };
		break;
	case EZoogiPanelLayout::Quad:
	default:
		PanelSlot = { FVector2D(0.15f, 0.22f), FVector2D(0.85f, 0.22f), FVector2D(0.15f, 0.78f), FVector2D(0.85f, 0.78f) };
		PanelFrom = { FVector2D(-900.f, -300.f), FVector2D(900.f, -300.f), FVector2D(-900.f, 300.f), FVector2D(900.f, 300.f) };
		PanelAngle = { -16.f, 16.f, 16.f, -16.f };
		break;
	}
	PanelCount = FMath::Min3(PanelSlot.Num(), Targets.Num(), PanelFrames.Num());
	for (int32 i = 0; i < PanelFrames.Num(); ++i)
	{
		UBorder* F = PanelFrames[i];
		if (i >= PanelCount || !Targets[i]) { F->SetVisibility(ESlateVisibility::Collapsed); continue; }
		FSlateBrush B;
		B.SetResourceObject(Targets[i]);
		B.ImageSize = PanelSize;
		B.DrawAs = ESlateBrushDrawType::Image;
		PanelImages[i]->SetBrush(B);
		F->SetBrushColor(Frame);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(F->Slot)) S->SetAnchors(FAnchors(PanelSlot[i].X, PanelSlot[i].Y));
		F->SetVisibility(ESlateVisibility::HitTestInvisible);
		F->SetRenderOpacity(0.f);
	}
	PanelState = 1;
	PanelAge = 0.f;
}

void UZoogiEpisodeWidget::CollapsePanels()
{
	if (PanelState == 1 || PanelState == 2) { PanelState = 3; PanelAge = 0.f; }
}

void UZoogiEpisodeWidget::SetPower(bool bOn, const FLinearColor& Color, const FString& Title)
{
	if (bOn && !bPower) PowerAge = 0.f;
	bPower = bOn;
	PowerColor = Color;
	if (PowerTitle) { PowerTitle->SetText(FText::FromString(Title)); PowerTitle->SetColorAndOpacity(FSlateColor(Color)); }
}

void UZoogiEpisodeWidget::Flash(float Strength, const FLinearColor& Color)
{
	FlashA = FMath::Max(FlashA, FMath::Clamp(Strength, 0.f, 1.f));
	FlashColor = Color;
}

void UZoogiEpisodeWidget::SetDarken(float Alpha) { DarkA = FMath::Clamp(Alpha, 0.f, 1.f); }

void UZoogiEpisodeWidget::ShowBanner(const FString& Text, const FString& Sub, const FLinearColor& Color, float Seconds)
{
	if (!BannerText) return;
	BannerText->SetText(FText::FromString(Text));
	BannerText->SetColorAndOpacity(FSlateColor(Color));
	BannerSub->SetText(FText::FromString(Sub));
	BannerBox->SetBrush(EpSolid(FLinearColor(0.02f, 0.03f, 0.08f, 0.8f), Color, 2.f, 8.f));
	BannerAge = 0.f;
	BannerLife = Seconds;
}

void UZoogiEpisodeWidget::TickEpisode(float Dt)
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
	// panels
	if (PanelState != 0)
	{
		PanelAge += Dt;
		const float In = 0.32f, Out = 0.26f;
		if (PanelState == 1 && PanelAge >= In) { PanelState = 2; PanelAge = 0.f; }
		else if (PanelState == 2 && PanelAge >= PanelHold) { PanelState = 3; PanelAge = 0.f; }
		else if (PanelState == 3 && PanelAge >= Out) { PanelState = 0; PanelAge = 0.f; }
		for (int32 i = 0; i < PanelFrames.Num(); ++i)
		{
			UBorder* F = PanelFrames[i];
			if (i >= PanelCount) continue;
			if (PanelState == 0) { F->SetVisibility(ESlateVisibility::Collapsed); continue; }
			float Slide = 1.f, Scale = 1.f, Opac = 1.f, Angle = 0.f;
			if (PanelState == 1)
			{
				const float T = FMath::Clamp((PanelAge - 0.05f * i) / (In - 0.05f * i + 0.001f), 0.f, 1.f);
				Slide = EaseOutBack(T);
				Opac = FMath::Clamp(T * 3.f, 0.f, 1.f);
				Angle = PanelAngle[i] * (1.f - EaseOut(T));
			}
			else if (PanelState == 2)
			{
				Angle = 0.6f * FMath::Sin((PanelAge + i) * 2.2f); // a little life while held
			}
			else
			{
				const float T = FMath::Clamp(PanelAge / Out, 0.f, 1.f);
				Scale = FMath::Lerp(1.f, 0.05f, EaseOut(T));    // collapse toward the slot centre
				Opac = 1.f - T;
				Angle = PanelAngle[i] * 0.5f * T;
			}
			FWidgetTransform X;
			X.Translation = PanelFrom[i] * (1.f - Slide);
			X.Scale = FVector2D(Scale, PanelState == 3 ? Scale * 0.6f : Scale);
			X.Angle = Angle;
			F->SetRenderTransform(X);
			F->SetRenderOpacity(Opac);
		}
	}
	// power escalation frame
	PowerAge += Dt;
	{
		const float Target = bPower ? 1.f : 0.f;
		const float Pulse = bPower ? 0.75f + 0.25f * FMath::Sin(PowerAge * 9.f) : 0.f;
		const float A = Target * FMath::Clamp(PowerAge / 0.15f, 0.f, 1.f) * Pulse;
		for (UImage* Bar : EdgeBars) Bar->SetColorAndOpacity(FLinearColor(PowerColor.R, PowerColor.G, PowerColor.B, 0.85f * A));
		TintImg->SetColorAndOpacity(FLinearColor(PowerColor.R, PowerColor.G, PowerColor.B, 0.10f * A));
		PowerTitle->SetRenderOpacity(A);
		PowerTitle->SetRenderScale(FVector2D(FMath::Lerp(1.4f, 1.f, EaseOut(PowerAge / 0.2f))));
	}
	// flash / darken
	FlashImg->SetColorAndOpacity(FLinearColor(FlashColor.R, FlashColor.G, FlashColor.B, FlashA));
	FlashA = FMath::Max(0.f, FlashA - Dt * 3.2f);
	DarkImg->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.02f, DarkA));
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
