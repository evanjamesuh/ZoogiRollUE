#include "ZoogiAnimePanels.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Images/SImage.h"

namespace
{
	// Crop of the 16:9 clip shown in each panel (u0, v0, u1, v1): wide / close / low / reaction, like a comic page.
	const FBox2f GCrop[4] = {
		FBox2f(FVector2f(0.04f, 0.10f), FVector2f(0.62f, 0.70f)),
		FBox2f(FVector2f(0.34f, 0.22f), FVector2f(0.76f, 0.66f)),
		FBox2f(FVector2f(0.00f, 0.34f), FVector2f(0.58f, 0.94f)),
		FBox2f(FVector2f(0.42f, 0.30f), FVector2f(0.98f, 0.88f)) };
	const float GTilt[4] = { -2.2f, 1.6f, 1.8f, -1.4f };
}

FBox2f FZoogiAnimePanels::PanelRect(int32 Index)
{
	// two panels per side, inside a ~3% landscape safe margin; centre band x 0.25..0.75 is left free for the Zoogi
	const bool bRight = Index == 1 || Index == 3;
	const bool bLow = Index >= 2;
	const float X0 = bRight ? 0.755f : 0.03f, X1 = bRight ? 0.97f : 0.245f;
	const float Y0 = bLow ? 0.575f : 0.335f, Y1 = bLow ? 0.79f : 0.555f; // between the HUD side panels (top) and the hand card (bottom right)
	return FBox2f(FVector2f(X0, Y0), FVector2f(X1, Y1));
}

void FZoogiAnimePanels::Show(UGameViewportClient* Viewport, UTexture* Movie, const FLinearColor& Accent, float Length)
{
	Hide();
	if (!Viewport || !Movie) return;
	VC = Viewport;
	State = MakeShared<FState>();
	State->Length = FMath::Max(0.4f, Length);
	State->Accent = Accent;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox"));
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
	for (int32 i = 0; i < 4; ++i)
	{
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
		B->SetResourceObject(Movie);
		B->ImageSize = FVector2D(1280.f, 720.f);
		B->SetUVRegion(GCrop[i]);
		State->Brushes.Add(B);
		const FBox2f R = PanelRect(i);
		TSharedPtr<FState> St = State;
		const float Start = i * 0.25f;
		auto Reveal = [St, Start]() { return FMath::Clamp((St->Age / St->Length - Start) / 0.06f, 0.f, 1.f); };
		TSharedRef<SWidget> Panel =
			SNew(SBorder) // black comic outline
			.BorderImage(White).BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.012f, 1.f)).Padding(5.f)
			[
				SNew(SBorder) // white inner frame
				.BorderImage(White).BorderBackgroundColor(FLinearColor(0.97f, 0.97f, 0.95f, 1.f)).Padding(3.f)
				[
					SNew(SBorder) // thin accent keyline (artifact colour)
					.BorderImage(White).BorderBackgroundColor_Lambda([St]() { return St->Accent; }).Padding(2.f)
					[
						SNew(SImage).Image(B.Get())
						.ColorAndOpacity_Lambda([St, Start]()
						{
							// the newest panel is full bright, earlier ones settle slightly darker (reading order)
							const float U = St->Age / St->Length;
							const float Next = Start + 0.25f;
							const float D = (Start < 0.75f && U > Next) ? 0.82f : 1.f;
							return FSlateColor(FLinearColor(D, D, D, 1.f));
						})
					]
				]
			];
		Panel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		const float Tilt = GTilt[i];
		Panel->SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::CreateLambda([Reveal, Tilt]()
		{
			const float K = Reveal();
			const float Sc = FMath::Max(0.01f, K < 1.f ? FMath::Lerp(1.25f, 1.f, K) : 1.f); // pop in
			return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FScale2D(Sc), FVector2D::ZeroVector).Concatenate(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Tilt)))));
		}));
		Panel->SetVisibility(TAttribute<EVisibility>::CreateLambda([Reveal]() { return Reveal() > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }));
		Canvas->AddSlot()
			.Anchors(FAnchors(R.Min.X, R.Min.Y, R.Max.X, R.Max.Y))
			.Offset(FMargin(0.f))
			[
				Panel
			];
	}
	Root = Canvas;
	Viewport->AddViewportWidgetContent(Root.ToSharedRef(), 50);
}

void FZoogiAnimePanels::SetAge(float Age)
{
	if (State) State->Age = Age;
}

void FZoogiAnimePanels::Hide()
{
	if (Root.IsValid())
	{
		if (UGameViewportClient* V = VC.Get()) V->RemoveViewportWidgetContent(Root.ToSharedRef());
		Root.Reset();
	}
	State.Reset();
}
