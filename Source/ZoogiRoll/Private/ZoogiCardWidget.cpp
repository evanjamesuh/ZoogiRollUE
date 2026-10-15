// cardui: see ZoogiCardWidget.h
#include "ZoogiCardWidget.h"
#include "ZoogiCardHandWidget.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Layout/Clipping.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "ZoogiUIStyle.h"
#include "ZoogiHudArt.h"

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
	// Evan 9:57 PM "unify cards": every card gets the same frame, art window, name plate, cost gem and element badge.
	// The element shows as the tablet badge + the thin colour line round the art; the art is cropped to its illustration
	// (inside its own printed frame / plate / cost) and fitted the same way into one fixed window.
	const FLinearColor Gold = FLinearColor(0.86f, 0.70f, 0.36f, 1.f) * Dim;
	ZoogiUIStyle::SBox(Out, Layer, Geo, FVector2D::ZeroVector, S, FLinearColor(0.035f, 0.07f, 0.18f, 0.98f), Gold, 10.f, 2.5f);
	const FVector2D WP(8.f, 8.f), WS(S.X - 16.f, FMath::RoundToFloat(S.Y * 0.745f) - 10.f);
	FLinearColor Back = Color * 0.3f * Dim + FLinearColor(0.02f, 0.04f, 0.12f, 0.f); Back.A = 1.f;
	ZoogiUIStyle::SBox(Out, Layer + 1, Geo, WP, WS, Back, FLinearColor::Transparent, 6.f, 0.f);
	if (Art)
	{
		// illustration crop shared by all card art (printed frame / cost / name plate left out), then cover-fit to the window
		float U0 = 0.07f, U1 = 0.93f, V0 = 0.045f, V1 = 0.785f;
		const float TW = FMath::Max(1.f, (float)Art->GetSurfaceWidth()), TH = FMath::Max(1.f, (float)Art->GetSurfaceHeight());
		const float CW = (U1 - U0) * TW, CH = (V1 - V0) * TH, A = WS.X / WS.Y;
		if (CW / CH > A) { const float D = (CW - CH * A) / TW * 0.5f; U0 += D; U1 -= D; }
		else { const float D = (CH - CW / A) / TH; V0 += D * 0.35f; V1 -= D * 0.65f; } // keep a little more of the top (heads / impact)
		// whole art scaled so the crop fills the window, clipped to the window
		const FVector2D DS(WS.X / (U1 - U0), WS.Y / (V1 - V0)), DP = WP - FVector2D(U0 * DS.X, V0 * DS.Y);
		FSlateBrush B;
		B.SetResourceObject(Art);
		B.ImageSize = DS;
		B.DrawAs = ESlateBrushDrawType::Image;
		Out.PushClip(FSlateClippingZone(Geo.ToPaintGeometry(FVector2f(WS), FSlateLayoutTransform(FVector2f(WP)))));
		FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2f(DS), FSlateLayoutTransform(FVector2f(DP))), &B, ESlateDrawEffect::None, Dim);
		Out.PopClip();
	}
	else if (Tablet && ZoogiHudArt::Tex(Tablet))
	{
		const float H = WS.Y * 0.86f, W = H * 360.f / 506.f;
		ZoogiHudArt::Img(Out, Layer + 2, Geo, WP + FVector2D((WS.X - W) * 0.5f, (WS.Y - H) * 0.5f), FVector2D(W, H), Tablet, Dim);
	}
	{
		// thin element-colour line round the art window (4 bars: SBox fills solid under its edge, so no outline-only box)
		const FLinearColor EC = FLinearColor(Color.R, Color.G, Color.B, 0.9f) * Dim;
		const FVector2D A0 = WP - FVector2D(1.f, 1.f), A1 = WP + WS + FVector2D(1.f, 1.f);
		ZoogiUIStyle::SBox(Out, Layer + 3, Geo, A0, FVector2D(A1.X - A0.X, 2.f), EC, FLinearColor::Transparent, 0.f, 0.f);
		ZoogiUIStyle::SBox(Out, Layer + 3, Geo, FVector2D(A0.X, A1.Y - 2.f), FVector2D(A1.X - A0.X, 2.f), EC, FLinearColor::Transparent, 0.f, 0.f);
		ZoogiUIStyle::SBox(Out, Layer + 3, Geo, A0, FVector2D(2.f, A1.Y - A0.Y), EC, FLinearColor::Transparent, 0.f, 0.f);
		ZoogiUIStyle::SBox(Out, Layer + 3, Geo, FVector2D(A1.X - 2.f, A0.Y), FVector2D(2.f, A1.Y - A0.Y), EC, FLinearColor::Transparent, 0.f, 0.f);
	}
	// name plate: same bar, same font, centred (shrinks only for long names)
	const FVector2D NP(6.f, WP.Y + WS.Y + 4.f), NS(S.X - 12.f, 28.f);
	ZoogiUIStyle::SBox(Out, Layer + 3, Geo, NP, NS, FLinearColor(0.06f, 0.11f, 0.26f, 1.f), Gold, 6.f, 1.5f);
	{
		const FString N = Name.ToUpper();
		int32 Sz = 13; FSlateFontInfo F = ZoogiUIStyle::BoldFont(Sz, 1.5f);
		while (Sz > 9 && ZoogiHudArt::Measure(N, F).X > NS.X - 12.f) F = ZoogiUIStyle::BoldFont(--Sz, 1.5f);
		const FVector2D M = ZoogiHudArt::Measure(N, F);
		const FVector2D TP(FMath::RoundToFloat(NP.X + (NS.X - M.X) * 0.5f), FMath::RoundToFloat(NP.Y + (NS.Y - M.Y) * 0.5f));
		FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(FVector2f(M), FSlateLayoutTransform(FVector2f(TP))), N, F, ESlateDrawEffect::None, FLinearColor::White * Dim);
	}
	// element badge: the tablet, bottom centre under the plate (same spot on every card)
	const float BY = NP.Y + NS.Y + 1.f, BH = FMath::Max(12.f, S.Y - BY - 2.f), BW = BH * 360.f / 506.f;
	if (Tablet && ZoogiHudArt::Tex(Tablet)) ZoogiHudArt::Img(Out, Layer + 4, Geo, FVector2D(FMath::RoundToFloat((S.X - BW) * 0.5f), BY), FVector2D(BW, BH), Tablet, Dim);
	else ZoogiUIStyle::SBox(Out, Layer + 4, Geo, FVector2D(S.X * 0.5f - 6.f, BY + BH * 0.5f - 6.f), FVector2D(12.f, 12.f), Color * Dim, Gold, 6.f, 1.5f);
	Layer += 4;
	++Layer;
	// cost circle (top-left corner, live cost from the card system covers the printed one)
	const float R = 17.f;
	const FVector2D C(R + 3.f, R + 3.f);
	// uistyle: chunky violet Card Energy coin with a navy outline and a white gloss cap (muted blue when unaffordable)
	ZoogiUIStyle::SBox(Out, Layer, Geo, C - FVector2D(R, R), FVector2D(R * 2.f, R * 2.f), bAffordable ? ZoogiUIStyle::CardEnergyB : ZoogiUIStyle::Muted, ZoogiUIStyle::Outline, R, 3.5f);
	ZoogiUIStyle::SBox(Out, Layer, Geo, C - FVector2D(R - 7.f, R - 7.f), FVector2D(R * 2.f - 14.f, R * 2.f - 14.f), bAffordable ? ZoogiUIStyle::CardEnergyA * FLinearColor(1.f, 1.f, 1.f, 0.6f) : FLinearColor(0.3f, 0.42f, 0.75f, 0.6f), FLinearColor::Transparent, R - 7.f, 0.f);
	ZoogiUIStyle::SBox(Out, Layer, Geo, C - FVector2D(R * 0.55f, R - 6.f), FVector2D(R * 1.1f, 6.f), FLinearColor(1.f, 1.f, 1.f, 0.55f), FLinearColor::Transparent, 3.f, 0.f);
	const FSlateFontInfo CF = ZoogiUIStyle::BoldFont(17);
	const FString CostS = FString::FromInt(Cost);
	const FVector2D CM = ZoogiHudArt::Measure(CostS, CF);
	FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(CM), FSlateLayoutTransform(FVector2f(FMath::RoundToFloat(C.X - CM.X * 0.5f), FMath::RoundToFloat(C.Y - CM.Y * 0.5f)))),
		CostS, CF, ESlateDrawEffect::None, bAffordable ? FLinearColor::White : FLinearColor(1.f, 0.55f, 0.5f));
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
