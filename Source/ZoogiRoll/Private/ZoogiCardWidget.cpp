// cardui: see ZoogiCardWidget.h
#include "ZoogiCardWidget.h"
#include "ZoogiCardHandWidget.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "ZoogiUIStyle.h"

int32 UZoogiCardWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D S = Geo.GetLocalSize();
	const FLinearColor Dim = bAffordable ? FLinearColor::White : FLinearColor(0.5f, 0.6f, 0.9f, 1.f); // uistyle: cool blue dim, not gray
	// glow in the card color when hovered / dragged
	if (bHighlight)
	{
		const FSlateRoundedBoxBrush Glow(FLinearColor(Color.R, Color.G, Color.B, 0.32f), 16.f, Color, 5.f);
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(S + FVector2D(14.f, 14.f)), FSlateLayoutTransform(FVector2f(-7.f, -7.f))), &Glow);
	}
	++Layer;
	if (Art)
	{
		FSlateBrush B;
		B.SetResourceObject(Art);
		B.ImageSize = FVector2D(S);
		B.DrawAs = ESlateBrushDrawType::Image;
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(), &B, ESlateDrawEffect::None, Dim);
	}
	else
	{
		// no art file: plain face from the card data (name + rule)
		ZoogiUIStyle::SBox(Out, Layer, Geo, FVector2D::ZeroVector, S, Color * 0.45f * Dim + FLinearColor(0.02f, 0.06f, 0.25f, 0.9f), ZoogiUIStyle::Outline, 12.f, 3.5f);
		const FSlateFontInfo F = ZoogiUIStyle::BoldFont(13);
		FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(S.X - 16.f, 30.f), FSlateLayoutTransform(FVector2f(10.f, S.Y - 48.f))), Name, F, ESlateDrawEffect::None, FLinearColor::White * Dim);
	}
	++Layer;
	// cost circle (top-left corner, live cost from the card system covers the printed one)
	const float R = 17.f;
	const FVector2D C(R + 3.f, R + 3.f);
	// uistyle: chunky violet Card Energy coin with a navy outline and a white gloss cap (muted blue when unaffordable)
	ZoogiUIStyle::SBox(Out, Layer, Geo, C - FVector2D(R, R), FVector2D(R * 2.f, R * 2.f), bAffordable ? ZoogiUIStyle::CardEnergyB : ZoogiUIStyle::Muted, ZoogiUIStyle::Outline, R, 3.5f);
	ZoogiUIStyle::SBox(Out, Layer, Geo, C - FVector2D(R - 7.f, R - 7.f), FVector2D(R * 2.f - 14.f, R * 2.f - 14.f), bAffordable ? ZoogiUIStyle::CardEnergyA * FLinearColor(1.f, 1.f, 1.f, 0.6f) : FLinearColor(0.3f, 0.42f, 0.75f, 0.6f), FLinearColor::Transparent, R - 7.f, 0.f);
	ZoogiUIStyle::SBox(Out, Layer, Geo, C - FVector2D(R * 0.55f, R - 6.f), FVector2D(R * 1.1f, 6.f), FLinearColor(1.f, 1.f, 1.f, 0.55f), FLinearColor::Transparent, 3.f, 0.f);
	const FSlateFontInfo CF = ZoogiUIStyle::BoldFont(17);
	FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(R * 2.f, R * 2.f), FSlateLayoutTransform(FVector2f(C - FVector2D(6.5f, 13.f)))),
		FString::FromInt(Cost), CF, ESlateDrawEffect::None, bAffordable ? FLinearColor::White : FLinearColor(1.f, 0.55f, 0.5f));
	return Super::NativePaint(Args, Geo, Cull, Out, Layer + 2, Style, bParentEnabled);
}

void UZoogiCardWidget::NativeOnMouseEnter(const FGeometry& Geo, const FPointerEvent& Ev) { Super::NativeOnMouseEnter(Geo, Ev); if (Hand) Hand->CardHover(this, true); }
void UZoogiCardWidget::NativeOnMouseLeave(const FPointerEvent& Ev) { Super::NativeOnMouseLeave(Ev); if (Hand) Hand->CardHover(this, false); }

FReply UZoogiCardWidget::NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev)
{
	if (Hand && Ev.GetEffectingButton() == EKeys::LeftMouseButton && Hand->CardPress(this, Ev)) return FReply::Handled().CaptureMouse(TakeWidget());
	if (Hand && Ev.GetEffectingButton() == EKeys::RightMouseButton && Hand->IsDragging(this)) { Hand->CardRelease(this, Ev, true); return FReply::Handled().ReleaseMouseCapture(); }
	return FReply::Unhandled();
}

FReply UZoogiCardWidget::NativeOnMouseMove(const FGeometry& Geo, const FPointerEvent& Ev)
{
	if (Hand && Hand->IsDragging(this)) { Hand->CardDragMove(this, Ev); return FReply::Handled(); }
	return FReply::Unhandled();
}

FReply UZoogiCardWidget::NativeOnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Ev)
{
	if (Hand && Hand->IsDragging(this) && Ev.GetEffectingButton() == EKeys::LeftMouseButton) { Hand->CardRelease(this, Ev, false); return FReply::Handled().ReleaseMouseCapture(); }
	return FReply::Unhandled();
}

void UZoogiCardWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Ev)
{
	Super::NativeOnMouseCaptureLost(Ev);
	if (Hand && Hand->IsDragging(this)) Hand->CancelDrag(TEXT("capture_lost"));
}
