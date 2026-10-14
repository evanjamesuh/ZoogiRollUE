#include "ZoogiAwakenWidget.h"
#include "ZoogiCards.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Async/Async.h"
#include "Misc/App.h"

namespace
{
	// Same colours / metrics as the Canvas HUD (ZoogiHUD.cpp), in 1080p units (UMG's DPI curve is 1.0 at 1080p,
	// the Canvas HUD's U is 1.0 at 1080p, so the two stay the same size at every resolution).
	const FLinearColor GPanelCol(0.02f, 0.03f, 0.08f, 0.72f);
	const float AwCW = 230.f, AwCH = 250.f, AwGap = 18.f;

	FSlateFontInfo HudFontU(float Scale, bool bBig)
	{
		UFont* F = bBig ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
		FSlateFontInfo Fi = F->GetLegacySlateFontInfo();
		Fi.Size = FMath::Max(6.f, FMath::RoundToFloat(Fi.Size * Scale * (bBig ? 1.75f : 2.3f)));
		if (bBig) Fi.TypefaceFontName = FName(TEXT("Bold"));
		return Fi;
	}

	FSlateBrush Solid(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineW = 0.f)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f), FSlateColor(Outline), OutlineW);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* P, UWidget* W, FVector2D Pos, FVector2D Size, FVector2D Align = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* S = P->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.f, 0.f));
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		if (Size.X > 0.f) { S->SetAutoSize(false); S->SetSize(Size); }
		else S->SetAutoSize(true);
		return S;
	}

	bool ViewportGeo(const UWorld* W, FGeometry& Out)
	{
		UGameViewportClient* VC = W ? W->GetGameViewport() : nullptr;
		TSharedPtr<SViewport> VW = VC ? VC->GetGameViewportWidget() : nullptr;
		if (!VW.IsValid()) return false;
		Out = VW->GetCachedGeometry();
		return Out.GetLocalSize().X > 0.f;
	}
}

// ------------------------------------------------------------------ clicks

void UZoogiAwakenClick::Click()
{
	if (Owner) Owner->Fire(Action, Index);
}

void UZoogiAwakenClick::Press()
{
	if (Owner) Owner->NotePress();
}

void UZoogiAwakenWidget::Fire(FName Action, int32 Index)
{
	NotePress();
	UE_LOG(LogZoogi, Log, TEXT("UMG click: %s %d"), *Action.ToString(), Index);
	LastFiredAction = Action; LastFiredIndex = Index; LastFiredFrame = GFrameCounter;
	if (AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr) G->MenuAction(Action, Index);
}

void UZoogiAwakenWidget::NotePress()
{
	PressFrame = GFrameCounter;
}

bool UZoogiAwakenWidget::ConsumedClickRecently() const
{
	return PressFrame != 0 && GFrameCounter - PressFrame <= 1;
}

FEventReply UZoogiAwakenWidget::OnPanelMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	// A press on a panel's background (between / around the controls) is swallowed here, so it never
	// reaches the game viewport (no floor aim, no target lock through the panel).
	NotePress();
	return FEventReply(true);
}

// ------------------------------------------------------------------ build helpers

TSharedRef<SWidget> UZoogiAwakenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Root;
	}
	return Super::RebuildWidget();
}

UTextBlock* UZoogiAwakenWidget::MakeText(const FString& S, float Scale, const FLinearColor& C, bool bBig)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(S));
	T->SetFont(HudFontU(Scale, bBig));
	T->SetColorAndOpacity(FSlateColor(C));
	T->SetShadowOffset(FVector2D(2.f, 2.f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f * C.A));
	T->SetVisibility(ESlateVisibility::HitTestInvisible);
	return T;
}

UTextBlock* UZoogiAwakenWidget::PlaceText(UCanvasPanel* P, const FString& S, FVector2D Pos, float Scale, const FLinearColor& C, bool bBig, float AlignX)
{
	UTextBlock* T = MakeText(S, Scale, C, bBig);
	Place(P, T, Pos, FVector2D::ZeroVector, FVector2D(AlignX, 0.f));
	return T;
}

UBorder* UZoogiAwakenWidget::MakePanel(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineW)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrush(Solid(Fill, Outline, OutlineW));
	B->SetPadding(FMargin(0.f));
	return B;
}

void UZoogiAwakenWidget::RegisterControl(FName Action, int32 Index, UWidget* W)
{
	FControl C;
	C.Action = Action;
	C.Index = Index;
	C.bAim = bBuildingAim;
	C.Widget = W;
	Controls.Add(C);
}

UWidget* UZoogiAwakenWidget::MakeButton(UCanvasPanel* Parent, const FString& Label, FVector2D Pos, FVector2D Size, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected, bool bEnabled, float Shimmer)
{
	// Same look as AZoogiHUD::Button: dark (or accent when selected) plate, 4 px accent bar at the bottom, bold label.
	UButton* Btn = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	FLinearColor Bg = bSelected ? Accent * 0.75f : FLinearColor(0.08f, 0.1f, 0.18f, 0.9f);
	Bg.A = 0.92f;
	FLinearColor Hv = Bg + FLinearColor(0.12f, 0.12f, 0.12f, 0.f);
	Hv.A = 0.95f;
	FLinearColor Pr = Accent * 0.5f;
	Pr.A = 0.95f;
	FButtonStyle Style;
	Style.SetNormal(Solid(Bg));
	Style.SetHovered(Solid(Hv, Accent.CopyWithNewOpacity(0.9f), 2.f));
	Style.SetPressed(Solid(Pr, FLinearColor::White, 2.f));
	Style.SetDisabled(Solid(Bg * FLinearColor(1.f, 1.f, 1.f, 0.6f)));
	Style.SetNormalPadding(FMargin(0.f));
	Style.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	Btn->SetStyle(Style);
	Btn->SetIsEnabled(bEnabled);

	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UTextBlock* T = MakeText(Label, 0.55f, FLinearColor::White, true);
	T->SetJustification(ETextJustify::Center);
	if (UOverlaySlot* S = O->AddChildToOverlay(T)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetHeightOverride(4.f);
	BarBox->AddChild(MakePanel(Accent.CopyWithNewOpacity(1.f)));
	if (UOverlaySlot* S = O->AddChildToOverlay(BarBox)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Bottom); }
	O->SetClipping(EWidgetClipping::ClipToBounds);
	FBtnFx Fx;
	Fx.Btn = Btn;
	Fx.W = Size.X;
	Fx.H = Size.Y;
	Fx.Shimmer = bEnabled ? (Shimmer > 0.f ? Shimmer : (bSelected ? 0.12f : 0.f)) : 0.f;
	Fx.Offset = FMath::Frac(Pos.X * 0.0037f);
	if (Fx.Shimmer > 0.f) Fx.Band = AddBand(O, FVector2D(FMath::Max(40.f, Size.Y * 0.9f), Size.Y * 2.2f), FLinearColor(1.f, 0.97f, 0.88f));
	BtnFx.Add(Fx);
	if (UButtonSlot* S = Cast<UButtonSlot>(Btn->AddChild(O))) { S->SetPadding(FMargin(0.f)); S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	Place(Parent, Btn, Pos, Size);
	Btn->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	UZoogiAwakenClick* C = NewObject<UZoogiAwakenClick>(this);
	C->Owner = this;
	C->Action = Action;
	C->Index = Index;
	(bBuildingAim ? AimClicks : AwakenClicks).Add(C);
	Btn->OnClicked.AddDynamic(C, &UZoogiAwakenClick::Click);
	Btn->OnPressed.AddDynamic(C, &UZoogiAwakenClick::Press);
	RegisterControl(Action, Index, Btn);
	return Btn;
}

// Procedural gem (same layers as AZoogiHUD::Gem: glow, rim n-gon, core n-gon, Void ring, highlight), rasterised
// once per artifact into a small transient texture with 4x4 supersampling.
UTexture2D* UZoogiAwakenWidget::GemTexture(uint8 ArtifactId)
{
	if (TObjectPtr<UTexture2D>* Found = Gems.Find(ArtifactId)) return *Found;
	const EZoogiArtifact A = (EZoogiArtifact)ArtifactId;
	if (A == EZoogiArtifact::None) return nullptr;
	const FZoogiArtifactRow& Row = ZoogiArtifacts::Get(A);
	int32 Sides = 6;
	switch (A)
	{
	case EZoogiArtifact::Ember: Sides = 3; break;
	case EZoogiArtifact::Storm: Sides = 4; break;
	case EZoogiArtifact::Frost: Sides = 6; break;
	case EZoogiArtifact::Titan: Sides = 8; break;
	case EZoogiArtifact::Void: Sides = 24; break;
	default: break;
	}
	const int32 N = 160;
	const float R = 46.f, C0 = N * 0.5f;
	auto InNGon = [](float X, float Y, float Rad, int32 Sd)
	{
		const float Rr = FMath::Sqrt(X * X + Y * Y);
		if (Rr < 1e-3f) return true;
		const float Seg = UE_TWO_PI / Sd;
		float Th = FMath::Atan2(Y, X);
		if (Th < 0.f) Th += UE_TWO_PI;
		const float Mid = (FMath::FloorToFloat(Th / Seg) + 0.5f) * Seg;
		return Rr * FMath::Cos(Th - Mid) <= Rad * FMath::Cos(UE_PI / Sd);
	};
	TArray<FColor> Px;
	Px.SetNumZeroed(N * N);
	const int32 SS = 4;
	for (int32 y = 0; y < N; ++y)
	{
		for (int32 x = 0; x < N; ++x)
		{
			FLinearColor Acc(0.f, 0.f, 0.f, 0.f); // premultiplied sum
			for (int32 sy = 0; sy < SS; ++sy)
			{
				for (int32 sx = 0; sx < SS; ++sx)
				{
					const float X = x + (sx + 0.5f) / SS - C0, Y = y + (sy + 0.5f) / SS - C0;
					FLinearColor P(0.f, 0.f, 0.f, 0.f); // premultiplied
					auto Over = [&P](const FLinearColor& Col, float Al)
					{
						P.R = Col.R * Al + P.R * (1.f - Al);
						P.G = Col.G * Al + P.G * (1.f - Al);
						P.B = Col.B * Al + P.B * (1.f - Al);
						P.A = Al + P.A * (1.f - Al);
					};
					if (X * X + Y * Y <= FMath::Square(R * 1.55f)) Over(Row.RimColor, 0.25f);
					if (InNGon(X, Y, R * 1.08f, Sides)) Over(Row.RimColor, 1.f);
					if (InNGon(X, Y, R * 0.86f, Sides)) Over(Row.CoreColor, 1.f);
					if (A == EZoogiArtifact::Void)
					{
						const float Q = FMath::Sqrt(FMath::Square(X / (R * 1.35f)) + FMath::Square(Y / (R * 0.5f)));
						if (FMath::Abs(Q - 1.f) * R * 0.5f <= 1.1f) Over(FLinearColor(0.75f, 0.5f, 1.f), 1.f);
					}
					if (FMath::Square(X + R * 0.28f) + FMath::Square(Y + R * 0.3f) <= FMath::Square(R * 0.22f)) Over(FLinearColor::White, 0.55f);
					Acc += P;
				}
			}
			Acc /= float(SS * SS);
			FLinearColor Out(0.f, 0.f, 0.f, Acc.A);
			if (Acc.A > 1e-4f) { Out.R = Acc.R / Acc.A; Out.G = Acc.G / Acc.A; Out.B = Acc.B / Acc.A; }
			Px[y * N + x] = Out.ToFColor(true);
		}
	}
	UTexture2D* T = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
	if (!T) return nullptr;
	T->SRGB = true;
	T->Filter = TF_Bilinear;
	FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Px.GetData(), Px.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	T->UpdateResource();
	Gems.Add(ArtifactId, T);
	return T;
}

// ------------------------------------------------------------------ Awaken panel

FString UZoogiAwakenWidget::AwakenSignature(AZoogiGameMode* G) const
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !B->Artifact) return FString();
	FString S = FString::Printf(TEXT("%p|%d|%d|%d|"), B, B->bCPU ? 1 : 0, (int32)B->Artifact->Drawn, G->AwakenPick);
	for (EZoogiArtifact H : B->Artifact->Hand) S += FString::Printf(TEXT("%d,"), (int32)H);
	for (const AZoogiBall* O : G->Balls)
	{
		if (!O || O == B || O->bOut || !O->Artifact) continue;
		S += FString::Printf(TEXT("|g%d"), (int32)O->Artifact->Guard);
	}
	return S;
}

void UZoogiAwakenWidget::BuildAwaken(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	if (!B || !B->Artifact) return;
	UZoogiArtifactComponent* AC = B->Artifact;
	const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
	bBuildingAim = false;
	if (!DotTex) DotTex = ZoogiUIFx::ToTexture(ZoogiUIFx::SoftDot(128));
	if (!BandTex) BandTex = ZoogiUIFx::ToTexture(ZoogiUIFx::ShimmerBand(64, 256));

	TArray<TPair<int32, EZoogiArtifact>> Cards;
	if (AC->Drawn != EZoogiArtifact::None) Cards.Add({ 0, AC->Drawn });
	for (int32 i = 0; i < AC->Hand.Num(); ++i) Cards.Add({ i + 1, AC->Hand[i] });
	const float PW = FMath::Max(3, Cards.Num()) * (AwCW + AwGap) + 40.f + 400.f;
	const float PH = AwCH + 96.f;
	const float Y0 = 56.f; // panel-local top of the cards

	AwakenPanel = MakePanel(GPanelCol);
	AwakenPanel->SetVisibility(ESlateVisibility::Visible);
	AwakenPanel->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiAwakenWidget, OnPanelMouseDown));
	AwakenPanel->OnMouseDoubleClickEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiAwakenWidget, OnPanelMouseDown));
	UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(AwakenPanel);
	RootSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RootSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RootSlot->SetPosition(FVector2D(0.f, -120.f));
	RootSlot->SetSize(FVector2D(PW, PH));
	UCanvasPanel* P = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	AwakenPanel->SetContent(P);

	Place(P, MakePanel(PC), FVector2D(0.f, 0.f), FVector2D(PW, 5.f));
	PlaceText(P, FString::Printf(TEXT("AWAKEN  -  %s%s"), *B->Def.Name.ToUpper(), B->bCPU ? TEXT("  (CPU deciding)") : TEXT("")), FVector2D(24.f, 12.f), 0.6f, FLinearColor(1.f, 0.95f, 0.85f), true);
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	const bool bFullHand = AC->Drawn != EZoogiArtifact::None && AC->Hand.Num() >= UZoogiArtifactComponent::HandCap;
	{
		FString Guards;
		for (const AZoogiBall* O : G->Balls)
		{
			if (!O || O == B || O->bOut || !O->Artifact || O->Artifact->Guard == EZoogiArtifact::None) continue;
			Guards += FString::Printf(TEXT("%sP%d %s (%s)"), Guards.IsEmpty() ? TEXT("") : TEXT(",  "), O->PlayerIndex + 1, *O->Def.Name, *ZoogiArtifacts::Name(O->Artifact->Guard));
		}
		const FString Line = Guards.IsEmpty() ? FString(TEXT("no rival guards up")) : FString::Printf(TEXT("GUARDED: %s"), *Guards);
		PlaceText(P, Line, FVector2D(PW - 24.f, 16.f), 0.36f, Guards.IsEmpty() ? FLinearColor(0.55f, 0.6f, 0.7f) : FLinearColor(0.55f, 0.8f, 1.f), false, 1.f);
	}
	{
		const float FY = Y0 + AwCH + 6.f;
		if (!B->bCPU) PlaceText(P, TEXT("keys:  1-3 pick card    I infuse    G guard    S save      (or click)"), FVector2D(24.f, FY), 0.32f, FLinearColor(0.55f, 0.6f, 0.72f), false);
		if (bFullHand)
		{
			const FString Warn = FString::Printf(TEXT("HAND FULL - saving %s discards %s"), *ZoogiArtifacts::Name(AC->Drawn), *ZoogiArtifacts::Name(AC->Hand[0]));
			PlaceText(P, Warn, FVector2D(PW - 24.f, FY), 0.34f, FLinearColor(1.f, 0.6f, 0.35f), false, 1.f);
		}
	}
	int32 Pick = G->AwakenPick;
	if (!Cards.ContainsByPredicate([Pick](const TPair<int32, EZoogiArtifact>& C) { return C.Key == Pick; }) && Cards.Num()) Pick = Cards[0].Key;
	SelGem = nullptr;
	CardFx.Reset();
	float X = 20.f;
	for (const TPair<int32, EZoogiArtifact>& C : Cards)
	{
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(C.Value);
		const bool bSel = C.Key == Pick;
		const FLinearColor Bg = bSel ? FLinearColor(R.CoreColor.R * 0.3f, R.CoreColor.G * 0.3f, R.CoreColor.B * 0.3f, 0.95f) : FLinearColor(0.05f, 0.07f, 0.14f, 0.9f);
		UCanvasPanel* Card = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		if (!B->bCPU)
		{
			// The whole card is a button: click = pick it (same as keys 1-3).
			UButton* Btn = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
			FButtonStyle Style;
			const FLinearColor Rim = bSel ? R.RimColor : FLinearColor::Transparent;
			Style.SetNormal(Solid(Bg, Rim, bSel ? 4.f : 0.f));
			Style.SetHovered(Solid(FLinearColor(Bg.R + 0.05f, Bg.G + 0.05f, Bg.B + 0.07f, 0.98f), bSel ? R.RimColor : R.RimColor * FLinearColor(1.f, 1.f, 1.f, 0.6f), bSel ? 4.f : 2.f));
			Style.SetPressed(Solid(FLinearColor(R.CoreColor.R * 0.4f, R.CoreColor.G * 0.4f, R.CoreColor.B * 0.4f, 0.98f), R.RimColor, 4.f));
			Style.SetNormalPadding(FMargin(0.f));
			Style.SetPressedPadding(FMargin(0.f));
			Btn->SetStyle(Style);
			if (UButtonSlot* S = Cast<UButtonSlot>(Btn->AddChild(Card))) { S->SetPadding(FMargin(0.f)); S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
			Place(P, Btn, FVector2D(X, Y0), FVector2D(AwCW, AwCH));
			UZoogiAwakenClick* Ck = NewObject<UZoogiAwakenClick>(this);
			Ck->Owner = this;
			Ck->Action = TEXT("AwakenPick");
			Ck->Index = C.Key;
			AwakenClicks.Add(Ck);
			Btn->OnClicked.AddDynamic(Ck, &UZoogiAwakenClick::Click);
			Btn->OnPressed.AddDynamic(Ck, &UZoogiAwakenClick::Press);
			RegisterControl(TEXT("AwakenPick"), C.Key, Btn);
		}
		else
		{
			UBorder* Plate = MakePanel(Bg, bSel ? R.RimColor : FLinearColor::Transparent, bSel ? 4.f : 0.f);
			Plate->SetContent(Card);
			Place(P, Plate, FVector2D(X, Y0), FVector2D(AwCW, AwCH));
		}
		// Card fx: light sweep across the card (clipped), a pulsing glow behind the stone, the stone itself turning
		// in 3D (pre-rendered frames, two stacked images cross-faded), a pulsing rim on the picked card.
		FCardFx Fx;
		Fx.Card = Card->GetParent() ? (UWidget*)Card->GetParent() : (UWidget*)Card;
		Fx.Btn = Cast<UButton>(Card->GetParent());
		Fx.Artifact = (uint8)C.Value;
		Fx.Key = C.Key;
		Fx.Slot = CardFx.Num();
		Fx.bSel = bSel;
		Fx.Lift = bSel ? 1.f : 0.f;
		if (UWidget* CW = Fx.Card.Get()) CW->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Card->SetClipping(EWidgetClipping::ClipToBounds);
		{
			UImage* Glow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush GB;
			GB.SetResourceObject(DotTex);
			GB.ImageSize = FVector2D(220.f, 220.f);
			GB.TintColor = FSlateColor(FLinearColor(R.RimColor.R, R.RimColor.G, R.RimColor.B, 1.f));
			Glow->SetBrush(GB);
			Glow->SetVisibility(ESlateVisibility::HitTestInvisible);
			Glow->SetRenderOpacity(bSel ? 0.55f : 0.32f);
			Glow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Place(Card, Glow, FVector2D(AwCW * 0.5f, 80.f), FVector2D(220.f, 220.f), FVector2D(0.5f, 0.5f));
			Fx.Glow = Glow;
		}
		{
			UOverlay* Stone = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			Stone->SetVisibility(ESlateVisibility::HitTestInvisible);
			Stone->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			UImage* Imgs[2];
			for (int32 k = 0; k < 2; ++k)
			{
				Imgs[k] = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				Imgs[k]->SetVisibility(ESlateVisibility::HitTestInvisible);
				FSlateBrush SB;
				SB.ImageSize = FVector2D(148.f, 148.f);
				Imgs[k]->SetBrush(SB);
				if (UOverlaySlot* OS = Stone->AddChildToOverlay(Imgs[k])) { OS->SetHorizontalAlignment(HAlign_Fill); OS->SetVerticalAlignment(VAlign_Fill); }
			}
			UTexture2D* A = nullptr; UTexture2D* Bt = nullptr; float Blend = 0.f;
			float& Ang = CardAngle.FindOrAdd(Fx.Slot, Fx.Slot * 37.f);
			if (GemFrameAt(Fx.Artifact, Ang, A, Bt, Blend))
			{
				Imgs[0]->SetBrushFromTexture(A, false);
				Imgs[1]->SetBrushFromTexture(Bt, false);
				Imgs[1]->SetRenderOpacity(Blend);
			}
			else if (UTexture2D* Tex = GemTexture((uint8)C.Value))
			{
				Imgs[0]->SetBrushFromTexture(Tex, false);
				Imgs[1]->SetRenderOpacity(0.f);
			}
			Place(Card, Stone, FVector2D(AwCW * 0.5f, 80.f), FVector2D(148.f, 148.f), FVector2D(0.5f, 0.5f));
			Fx.Stone = Stone;
			Fx.StoneA = Imgs[0];
			Fx.StoneB = Imgs[1];
			if (bSel) SelGem = Imgs[0];
		}
		Fx.Band = AddBand(Card, FVector2D(70.f, AwCH * 1.6f), FLinearColor(R.RimColor.R * 0.5f + 0.5f, R.RimColor.G * 0.5f + 0.5f, R.RimColor.B * 0.5f + 0.5f, 1.f));
		if (bSel)
		{
			UBorder* Rim = MakePanel(FLinearColor::Transparent, FLinearColor(R.RimColor.R * 0.5f + 0.5f, R.RimColor.G * 0.5f + 0.5f, R.RimColor.B * 0.5f + 0.5f, 1.f), 3.f);
			Rim->SetVisibility(ESlateVisibility::HitTestInvisible);
			Place(Card, Rim, FVector2D(2.f, 2.f), FVector2D(AwCW - 4.f, AwCH - 4.f));
			Fx.SelRim = Rim;
		}
		CardFx.Add(Fx);
		PlaceText(Card, FString::Printf(TEXT("%s%s"), B->bCPU ? TEXT("") : *FString::Printf(TEXT("[%d] "), C.Key + 1), C.Key == 0 ? TEXT("NEW") : *FString::Printf(TEXT("SAVED %d"), C.Key)),
			FVector2D(12.f, 8.f), 0.34f, C.Key == 0 ? FLinearColor(1.f, 0.85f, 0.3f) : FLinearColor(0.7f, 0.75f, 0.85f), false);
		if (bFullHand && C.Key == 1) PlaceText(Card, TEXT("discarded on SAVE"), FVector2D(AwCW * 0.5f, AwCH - 28.f), 0.3f, FLinearColor(1.f, 0.55f, 0.35f), false, 0.5f);
		if (C.Value == Aff) PlaceText(Card, TEXT("AFFINITY"), FVector2D(AwCW - 12.f, 8.f), 0.34f, FLinearColor(1.f, 0.9f, 0.4f), false, 1.f);
		PlaceText(Card, R.DisplayName, FVector2D(AwCW * 0.5f, 150.f), 0.5f, FLinearColor::White, true, 0.5f);
		PlaceText(Card, ZoogiArtifacts::Key(C.Value).ToUpper(), FVector2D(AwCW * 0.5f, 192.f), 0.36f, R.RimColor, false, 0.5f);
		X += AwCW + AwGap;
	}
	for (int32 e = Cards.Num(); e < 3; ++e)
	{
		UBorder* EmptySlot = MakePanel(FLinearColor(0.04f, 0.05f, 0.1f, 0.55f));
		UCanvasPanel* SC = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		EmptySlot->SetContent(SC);
		Place(P, EmptySlot, FVector2D(X, Y0), FVector2D(AwCW, AwCH));
		Place(SC, MakePanel(FLinearColor(0.3f, 0.35f, 0.45f, 0.5f)), FVector2D(10.f, 10.f), FVector2D(AwCW - 20.f, 2.f));
		PlaceText(SC, TEXT("HAND SLOT"), FVector2D(AwCW * 0.5f, AwCH * 0.42f), 0.38f, FLinearColor(0.5f, 0.55f, 0.65f, 0.8f), false, 0.5f);
		PlaceText(SC, TEXT("empty"), FVector2D(AwCW * 0.5f, AwCH * 0.42f + 32.f), 0.32f, FLinearColor(0.45f, 0.5f, 0.6f, 0.7f), false, 0.5f);
		X += AwCW + AwGap;
	}
	// Right column: picked artifact's texts + INFUSE / GUARD / SAVE.
	const float RX = PW - 410.f;
	const int32 PickIdx = Cards.IndexOfByPredicate([Pick](const TPair<int32, EZoogiArtifact>& C) { return C.Key == Pick; });
	const EZoogiArtifact PA = PickIdx != INDEX_NONE ? Cards[PickIdx].Value : EZoogiArtifact::None;
	if (PA != EZoogiArtifact::None)
	{
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(PA);
		UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		V->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(P, V, FVector2D(RX, Y0), FVector2D(392.f, AwCH - 62.f));
		auto Add = [&](const FString& S, const FLinearColor& Col, bool bWrap, float Bottom)
		{
			UTextBlock* T = MakeText(S, 0.38f, Col, false);
			T->SetAutoWrapText(bWrap);
			if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(T)) VS->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
		};
		Add(TEXT("INFUSE"), FLinearColor(1.f, 0.7f, 0.35f), false, 0.f);
		Add(R.InfuseText, FLinearColor(0.85f, 0.88f, 0.95f), true, 6.f);
		if (PA == Aff) Add(FString::Printf(TEXT("Affinity: %s fires with it"), *B->Def.Ability), FLinearColor(1.f, 0.88f, 0.4f), true, 6.f);
		Add(TEXT("GUARD"), FLinearColor(0.55f, 0.8f, 1.f), false, 0.f);
		Add(R.GuardText, FLinearColor(0.85f, 0.88f, 0.95f), true, 0.f);
	}
	if (!B->bCPU)
	{
		const float BY = Y0 + AwCH - 56.f;
		const bool bHas = PA != EZoogiArtifact::None;
		MakeButton(P, TEXT("INFUSE"), FVector2D(RX - 10.f, BY), FVector2D(130.f, 56.f), TEXT("Infuse"), Pick, FLinearColor(1.f, 0.5f, 0.15f), bHas, bHas, 0.3f);
		MakeButton(P, TEXT("GUARD"), FVector2D(RX + 128.f, BY), FVector2D(130.f, 56.f), TEXT("Guard"), Pick, FLinearColor(0.35f, 0.7f, 1.f), bHas, bHas, 0.22f);
		MakeButton(P, TEXT("SAVE"), FVector2D(RX + 266.f, BY), FVector2D(124.f, 56.f), TEXT("Save"), 0, FLinearColor(0.6f, 0.65f, 0.75f), false);
	}
}

// ------------------------------------------------------------------ aim bar

FString UZoogiAwakenWidget::AimSignature(AZoogiGameMode* G) const
{
	if (G->bComboMode) return FString::Printf(TEXT("c%d|%d|%d|%d"), (int32)G->ComboForce, G->HasAim() ? 1 : 0, G->ActivePlayer, G->CanComboSelect() ? 1 : 0);
	return FString::Printf(TEXT("%d|%d"), G->AimPowerStep, G->HasAim() ? 1 : 0);
}

void UZoogiAwakenWidget::BuildAim(AZoogiGameMode* G)
{
	bBuildingAim = true;
	if (!BandTex) BandTex = ZoogiUIFx::ToTexture(ZoogiUIFx::ShimmerBand(64, 256));
	const float PW = 1180.f, PH = 118.f;
	AimPanel = MakePanel(GPanelCol);
	AimPanel->SetVisibility(ESlateVisibility::Visible);
	AimPanel->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiAwakenWidget, OnPanelMouseDown));
	AimPanel->OnMouseDoubleClickEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiAwakenWidget, OnPanelMouseDown));
	UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(AimPanel);
	RootSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RootSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RootSlot->SetPosition(FVector2D(0.f, -96.f));
	RootSlot->SetSize(FVector2D(PW, PH));
	UCanvasPanel* P = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	AimPanel->SetContent(P);

	AimInfo = PlaceText(P, TEXT(""), FVector2D(PW * 0.5f, 8.f), 0.42f, FLinearColor(0.9f, 0.95f, 1.f), false, 0.5f);
	const FLinearColor Acc(0.55f, 0.8f, 1.f);
	float X = 16.f;
	const float BY = 52.f, BH = 52.f;
	MakeButton(P, TEXT("<<"), FVector2D(X, BY), FVector2D(62.f, BH), TEXT("AimNudge"), -5, Acc, false); X += 68.f;
	MakeButton(P, TEXT("<"), FVector2D(X, BY), FVector2D(56.f, BH), TEXT("AimNudge"), -1, Acc, false); X += 62.f;
	MakeButton(P, TEXT(">"), FVector2D(X, BY), FVector2D(56.f, BH), TEXT("AimNudge"), 1, Acc, false); X += 62.f;
	MakeButton(P, TEXT(">>"), FVector2D(X, BY), FVector2D(62.f, BH), TEXT("AimNudge"), 5, Acc, false); X += 84.f;
	if (G->bComboMode)
	{
		// 2v2 Combo: no power steps - pick the force (1 / 2 / 3); SWITCH hands the shot to the partner (Q).
		static const TCHAR* ForceNames[3] = { TEXT("1 CONTROL"), TEXT("2 DRIVE"), TEXT("3 LAUNCH") };
		static const FLinearColor ForceCols[3] = { FLinearColor(0.45f, 0.9f, 1.f), FLinearColor(1.f, 0.8f, 0.3f), FLinearColor(1.f, 0.4f, 0.3f) };
		for (int32 i = 0; i < 3; ++i) { MakeButton(P, ForceNames[i], FVector2D(X, BY), FVector2D(128.f, BH), TEXT("ComboForce"), i, ForceCols[i], (int32)G->ComboForce == i); X += 134.f; }
		X += 18.f;
		if (G->CanComboSelect()) MakeButton(P, TEXT("Q SWITCH"), FVector2D(X, BY), FVector2D(110.f, BH), TEXT("ComboSelect"), -1, Acc, false);
		else MakeButton(P, TEXT("NEXT"), FVector2D(X, BY), FVector2D(110.f, BH), TEXT("AimNext"), 0, Acc, false);
		X += 118.f;
	}
	else
	{
	for (int32 i = 0; i < 4; ++i) { MakeButton(P, ZoogiCards::ForceLabel(i, G->CardsOn()), FVector2D(X, BY), FVector2D(96.f, BH), TEXT("AimPower"), i, FLinearColor(1.f, 0.8f, 0.3f), G->AimPowerStep == i); X += 102.f; }
	X += 18.f;
	MakeButton(P, TEXT("NEXT"), FVector2D(X, BY), FVector2D(110.f, BH), TEXT("AimNext"), 0, Acc, false); X += 118.f;
	}
	MakeButton(P, TEXT("CONFIRM"), FVector2D(X, BY), FVector2D(170.f, BH), TEXT("AimConfirm"), 0, FLinearColor(0.35f, 1.f, 0.45f), G->HasAim(), true, G->HasAim() ? 0.35f : 0.f); X += 178.f;
	MakeButton(P, TEXT("CANCEL"), FVector2D(X, BY), FVector2D(150.f, BH), TEXT("AimCancel"), 0, FLinearColor(1.f, 0.4f, 0.35f), false);
	bBuildingAim = false;
}

// ------------------------------------------------------------------ per frame

void UZoogiAwakenWidget::Refresh(float DeltaSeconds, bool bHideAll)
{
	Time += DeltaSeconds;
	if (!bGemJobsStarted) StartGemJobs();
	PumpGemJobs();
	BtnFx.RemoveAll([](const FBtnFx& F) { return !F.Btn.IsValid() || !F.Btn->GetParent(); });
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	bool bWantAwaken = false, bWantAim = false, bWantCards = false;
	if (G && Root && !bHideAll && G->Phase == EZoogiPhase::Aiming)
	{
		AZoogiBall* B = G->GetActiveBall();
		bWantAwaken = G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && B && B->Artifact && !G->CardsOn(); // v3: Canvas card panel instead
		bWantAim = G->IsHumanAiming() && G->IsAimStep();
		bWantCards = G->CardsOn() && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && B && G->IsHumanAiming();
	}
	FillPendingCards();
	if (bWantAwaken || bWantCards)
	{
		const FString Sig = bWantCards ? CardsSignature(G) : AwakenSignature(G);
		if (Sig != AwakenSig || !AwakenPanel)
		{
			if (AwakenPanel) AwakenPanel->RemoveFromParent();
			AwakenPanel = nullptr;
			Controls.RemoveAll([](const FControl& C) { return !C.bAim; });
			AwakenClicks.Reset();
			if (bWantCards) BuildCards(G); else BuildAwaken(G);
			AwakenSig = Sig;
			++NumRebuilds;
		}
	}
	if (AwakenPanel)
	{
		const ESlateVisibility V = (bWantAwaken || bWantCards) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (AwakenPanel->GetVisibility() != V) AwakenPanel->SetVisibility(V);
		if (!bWantAwaken && !bWantCards) AwakenSig.Reset();
	}
	if (bWantAim)
	{
		const FString Sig = AimSignature(G);
		if (Sig != AimSig || !AimPanel)
		{
			if (AimPanel) AimPanel->RemoveFromParent();
			AimPanel = nullptr;
			AimInfo = nullptr;
			Controls.RemoveAll([](const FControl& C) { return C.bAim; });
			AimClicks.Reset();
			BuildAim(G);
			AimSig = Sig;
			++NumRebuilds;
		}
		if (AimInfo)
		{
			float Frac = 0.f, Impact = 0.f;
			if (G->HasAim()) G->ComputeShotVelocity(Frac, Impact);
			const FString Info = G->HasAim()
				? FString::Printf(TEXT("%s   cut %+.0f deg   %s%s"), G->bFreeAim ? TEXT("FLOOR AIM") : *G->TargetName(G->TargetLock.Get()), G->AimCutDeg,
					G->bComboMode ? *FString::Printf(TEXT("%s  %.0f cm/s"), AZoogiGameMode::ForceName(G->ComboForce), FVector2D(G->ComputeShotVelocity(Frac, Impact)).Size() * G->ComboShotMul(G->GetActiveBall())) : *FString::Printf(TEXT("power %d%%"), FMath::RoundToInt(Frac * 100.f)),
					G->Preview.bContact ? *FString::Printf(TEXT("   contact cut %.0f deg"), G->Preview.CutDeg) : TEXT(""))
				: FString(TEXT("Click a rival / orb to lock on, or the floor to aim at a point"));
			AimInfo->SetText(FText::FromString(Info));
		}
	}
	if (AimPanel)
	{
		const ESlateVisibility V = bWantAim ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (AimPanel->GetVisibility() != V) AimPanel->SetVisibility(V);
		if (!bWantAim) AimSig.Reset();
	}
	bAwakenShown = bWantAwaken && AwakenPanel != nullptr;
	bCardsShown = bWantCards && AwakenPanel != nullptr;
	bAimShown = bWantAim && AimPanel != nullptr;
	if (bAwakenShown && !bAwakenWasShown) AwakenIntro = 0.f;
	if (bAimShown && !bAimWasShown) AimIntro = 0.f;
	bAwakenWasShown = bAwakenShown;
	bAimWasShown = bAimShown;
	AnimateFx(DeltaSeconds);
}

// ------------------------------------------------------------------ ui-polish fx

UImage* UZoogiAwakenWidget::AddBand(UPanelWidget* Clip, FVector2D Size, const FLinearColor& Tint)
{
	if (!BandTex) BandTex = ZoogiUIFx::ToTexture(ZoogiUIFx::ShimmerBand(64, 256));
	UImage* Band = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush B;
	B.SetResourceObject(BandTex);
	B.ImageSize = Size;
	B.TintColor = FSlateColor(Tint);
	Band->SetBrush(B);
	Band->SetVisibility(ESlateVisibility::HitTestInvisible);
	Band->SetRenderOpacity(0.f);
	if (UCanvasPanel* CP = Cast<UCanvasPanel>(Clip))
	{
		Place(CP, Band, FVector2D(0.f, -Size.Y * 0.2f), Size);
	}
	else if (UOverlay* O = Cast<UOverlay>(Clip))
	{
		if (UOverlaySlot* OS = O->AddChildToOverlay(Band)) { OS->SetHorizontalAlignment(HAlign_Left); OS->SetVerticalAlignment(VAlign_Center); }
	}
	return Band;
}

void UZoogiAwakenWidget::StartGemJobs()
{
	// The five stones are ray-traced once on the thread pool (a few hundred ms in total); until a stone's frames
	// are in, its card shows the flat procedural gem.
	bGemJobsStarted = true;
	const EZoogiArtifact All[5] = { EZoogiArtifact::Ember, EZoogiArtifact::Storm, EZoogiArtifact::Frost, EZoogiArtifact::Titan, EZoogiArtifact::Void };
	for (EZoogiArtifact A : All)
	{
		const FZoogiArtifactRow& Row = ZoogiArtifacts::Get(A);
		TSharedPtr<FGemJob, ESPMode::ThreadSafe> Job = MakeShared<FGemJob, ESPMode::ThreadSafe>();
		GemJobs.Add(Job);
		const uint8 Id = (uint8)A;
		const FLinearColor Core = Row.CoreColor, Rim = Row.RimColor;
		(void)Async(EAsyncExecution::ThreadPool, [Job, Id, Core, Rim]()
		{
			Job->Result = ZoogiUIFx::RenderGem(Id, Core, Rim, 176, 2.5f);
			Job->bDone.store(true);
		});
	}
}

void UZoogiAwakenWidget::PumpGemJobs()
{
	for (int32 j = GemJobs.Num() - 1; j >= 0; --j)
	{
		if (!GemJobs[j]->bDone.load()) continue;
		GemPending.Add(MoveTemp(GemJobs[j]->Result));
		GemJobs.RemoveAt(j);
	}
	// Turn finished frames into textures, a few per frame (each is a 176x176 upload).
	int32 Budget = 10;
	while (Budget > 0 && GemPending.Num())
	{
		ZoogiUIFx::FGemFrames& F = GemPending[0];
		if (F.Frames.Num() == 0) { GemPending.RemoveAt(0); GemPendingFrame = 0; continue; }
		if (GemPendingFrame == 0) GemRange.Add(F.Artifact, FIntPoint(GemFrameTex.Num(), 0)); // count stays 0 until complete
		GemFrameTex.Add(ZoogiUIFx::ToTexture(F.Frames[GemPendingFrame]));
		--Budget;
		if (++GemPendingFrame >= F.Frames.Num())
		{
			GemRange.FindChecked(F.Artifact).Y = F.Frames.Num();
			GemPeriod.Add(F.Artifact, F.PeriodDeg);
			UE_LOG(LogZoogi, Log, TEXT("Awaken stone %d ready: %d frames over %.0f deg"), (int32)F.Artifact, F.Frames.Num(), F.PeriodDeg);
			GemPending.RemoveAt(0);
			GemPendingFrame = 0;
		}
	}
}

bool UZoogiAwakenWidget::GemFrameAt(uint8 Artifact, float AngleDeg, UTexture2D*& OutA, UTexture2D*& OutB, float& OutBlend) const
{
	const FIntPoint* R = GemRange.Find(Artifact);
	const float* Per = GemPeriod.Find(Artifact);
	if (!R || !Per || R->Y <= 0) return false;
	const float Step = *Per / R->Y;
	float F = FMath::Fmod(AngleDeg, *Per);
	if (F < 0.f) F += *Per;
	const float Fi = F / Step;
	const int32 I0 = FMath::FloorToInt(Fi) % R->Y;
	const int32 I1 = (I0 + 1) % R->Y;
	OutA = GemFrameTex[R->X + I0];
	OutB = GemFrameTex[R->X + I1];
	OutBlend = Fi - FMath::FloorToFloat(Fi);
	return OutA && OutB;
}

void UZoogiAwakenWidget::AnimateFx(float Dt)
{
	using namespace ZoogiUIFx;
	Dt = FMath::Clamp(Dt, 0.f, 0.1f);
	AwakenIntro += Dt;
	AimIntro += Dt;
	// Panels rise + fade in when they appear.
	if (AwakenPanel && bAwakenShown)
	{
		const float K = EaseOutCubic(FMath::Clamp(AwakenIntro / 0.38f, 0.f, 1.f));
		AwakenPanel->SetRenderTranslation(FVector2D(0.f, 46.f * (1.f - K)));
		AwakenPanel->SetRenderOpacity(K);
	}
	if (AimPanel && bAimShown)
	{
		const float K = EaseOutCubic(FMath::Clamp(AimIntro / 0.3f, 0.f, 1.f));
		AimPanel->SetRenderTranslation(FVector2D(0.f, 30.f * (1.f - K)));
		AimPanel->SetRenderOpacity(K);
	}
	if (bAwakenShown)
	{
		for (FCardFx& C : CardFx)
		{
			UWidget* CW = C.Card.Get();
			if (!CW) continue;
			const bool bHover = C.Btn.IsValid() && C.Btn->IsHovered();
			const bool bPress = C.Btn.IsValid() && C.Btn->IsPressed();
			const float HoverT = bPress ? 0.97f : (bHover ? 1.045f : 1.f);
			C.Hover = FMath::FInterpTo(C.Hover, HoverT, Dt, 14.f);
			C.Lift = FMath::FInterpTo(C.Lift, C.bSel ? 1.f : 0.f, Dt, 8.f);
			// Cards come in one after another, then float gently (each on its own phase).
			const float In = EaseOutBack(FMath::Clamp((AwakenIntro - 0.06f * C.Slot) / 0.45f, 0.f, 1.f));
			const float Bob = 3.f * FMath::Sin(Time * 1.5f + C.Slot * 1.3f);
			const float S = C.Hover * (1.f + 0.02f * C.Lift);
			CW->SetRenderTransform(FWidgetTransform(FVector2D(0.f, Bob - 8.f * C.Lift + 60.f * (1.f - In)), FVector2D(S, S), FVector2D::ZeroVector, 0.f));
			// The stone turns (faster on the picked card) and bobs; frame pair cross-fade keeps the turn smooth.
			float& Ang = CardAngle.FindOrAdd(C.Slot, C.Slot * 37.f);
			Ang = FMath::Fmod(Ang + Dt * (C.bSel ? 44.f : 28.f), 360000.f);
			UTexture2D* A = nullptr; UTexture2D* B = nullptr; float Blend = 0.f;
			if (GemFrameAt(C.Artifact, Ang, A, B, Blend) && C.StoneA.IsValid() && C.StoneB.IsValid())
			{
				if (C.StoneA->GetBrush().GetResourceObject() != A) C.StoneA->SetBrushFromTexture(A, false);
				if (C.StoneB->GetBrush().GetResourceObject() != B) C.StoneB->SetBrushFromTexture(B, false);
				C.StoneB->SetRenderOpacity(Blend);
			}
			if (UWidget* St = C.Stone.Get())
			{
				const float SB = (C.bSel ? 4.f : 2.5f) * FMath::Sin(Time * (C.bSel ? 2.6f : 1.9f) + C.Slot);
				const float SS = 1.f + (C.bSel ? 0.04f : 0.f) + 0.015f * FMath::Sin(Time * 2.2f + C.Slot * 0.7f);
				St->SetRenderTransform(FWidgetTransform(FVector2D(0.f, SB), FVector2D(SS, SS), FVector2D::ZeroVector, 0.f));
			}
			if (UWidget* Gw = C.Glow.Get())
			{
				const float P = 0.5f + 0.5f * FMath::Sin(Time * 2.4f + C.Slot * 1.7f);
				Gw->SetRenderOpacity((C.bSel ? 0.48f : 0.26f) + (C.bSel ? 0.22f : 0.12f) * P);
				const float GS = 0.92f + 0.12f * P;
				Gw->SetRenderScale(FVector2D(GS, GS));
			}
			if (UWidget* Bd = C.Band.Get())
			{
				const float Ph = SweepPhase(Time, 4.2f, C.Slot * 0.23f, 0.3f);
				if (Ph < 0.f) Bd->SetRenderOpacity(0.f);
				else
				{
					Bd->SetRenderTransform(FWidgetTransform(FVector2D(-110.f + Ph * (AwCW + 180.f), 0.f), FVector2D(1.f, 1.f), FVector2D::ZeroVector, 18.f));
					Bd->SetRenderOpacity((C.bSel ? 0.28f : 0.16f) * FMath::Sin(Ph * UE_PI));
				}
			}
			if (UWidget* Rm = C.SelRim.Get()) Rm->SetRenderOpacity(0.25f + 0.45f * (0.5f + 0.5f * FMath::Sin(Time * 3.4f)));
		}
	}
	for (FBtnFx& F : BtnFx)
	{
		UButton* Bt = F.Btn.Get();
		if (!Bt || !Bt->GetIsEnabled()) { if (Bt) Bt->SetRenderScale(FVector2D(1.f, 1.f)); continue; }
		const float T = Bt->IsPressed() ? 0.95f : (Bt->IsHovered() ? 1.06f : 1.f);
		F.Scale = FMath::FInterpTo(F.Scale, T, Dt, 16.f);
		Bt->SetRenderScale(FVector2D(F.Scale, F.Scale));
		if (UWidget* Bd = F.Band.Get())
		{
			const float Ph = SweepPhase(Time, 3.2f, F.Offset, 0.35f);
			if (Ph < 0.f) Bd->SetRenderOpacity(0.f);
			else
			{
				Bd->SetRenderTransform(FWidgetTransform(FVector2D(-F.H * 1.4f + Ph * (F.W + F.H * 2.f), 0.f), FVector2D(1.f, 1.f), FVector2D::ZeroVector, 18.f));
				Bd->SetRenderOpacity(F.Shimmer * FMath::Sin(Ph * UE_PI));
			}
		}
	}
}

// ------------------------------------------------------------------ hit tests + test helpers

bool UZoogiAwakenWidget::IsOverPanels(const FVector2D& ViewportPos) const
{
	FGeometry VG;
	if (!ViewportGeo(GetWorld(), VG)) return false;
	const FVector2D Abs = VG.LocalToAbsolute(ViewportPos / FMath::Max(VG.Scale, 0.01f));
	if (bAwakenShown && AwakenPanel && AwakenPanel->GetCachedGeometry().IsUnderLocation(Abs)) return true;
	if (bAimShown && AimPanel && AimPanel->GetCachedGeometry().IsUnderLocation(Abs)) return true;
	return false;
}

bool UZoogiAwakenWidget::AbsToViewport(const FVector2D& Abs, FVector2D& OutViewport) const
{
	FGeometry VG;
	if (!ViewportGeo(GetWorld(), VG)) return false;
	OutViewport = VG.AbsoluteToLocal(Abs) * VG.Scale;
	return true;
}

bool UZoogiAwakenWidget::FindControl(FName Action, int32 Index, FVector2D& OutAbs) const
{
	for (const FControl& C : Controls)
	{
		if (C.Action != Action || C.Index != Index || !C.Widget.IsValid()) continue;
		const FGeometry& Geo = C.Widget->GetCachedGeometry();
		if (Geo.GetLocalSize().X <= 0.f) return false; // not laid out yet
		OutAbs = Geo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
		return true;
	}
	return false;
}

bool UZoogiAwakenWidget::PanelPoint(bool bAimBar, const FVector2D& Frac, FVector2D& OutAbs) const
{
	const UBorder* P = bAimBar ? AimPanel.Get() : AwakenPanel.Get();
	if (!P || !(bAimBar ? bAimShown : bAwakenShown)) return false;
	const FGeometry& Geo = P->GetCachedGeometry();
	if (Geo.GetLocalSize().X <= 0.f) return false;
	OutAbs = Geo.GetAbsolutePositionAtCoordinates(Frac);
	return true;
}

bool UZoogiAwakenWidget::SynthHover(const FVector2D& Abs)
{
	if (!FSlateApplication::IsInitialized()) return false;
	FSlateApplication& S = FSlateApplication::Get();
	FPointerEvent Move(0, FSlateApplicationBase::CursorPointerIndex, Abs, Abs, TSet<FKey>(), EKeys::Invalid, 0.f, FModifierKeysState());
	S.ProcessMouseMoveEvent(Move);
	return true;
}

bool UZoogiAwakenWidget::SynthClick(const FVector2D& Abs)
{
	// Real Slate input: move, press, release at Abs (desktop space), routed through hit-testing exactly like a mouse.
	if (!FSlateApplication::IsInitialized()) return false;
	FSlateApplication& S = FSlateApplication::Get();
	if (!S.IsActive())
	{
		// The test window lost OS focus (someone clicked another app): Slate drops hover while inactive, so an SButton
		// would take the press but never fire OnClicked. Tell Slate the app is active again before clicking.
		UE_LOG(LogZoogi, Log, TEXT("UITEST note: application inactive, re-activating Slate before the synthetic click"));
		S.ProcessApplicationActivationEvent(true);
	}
	TSet<FKey> Down;
	Down.Add(EKeys::LeftMouseButton);
	SynthHover(Abs);
	FPointerEvent Press(0, FSlateApplicationBase::CursorPointerIndex, Abs, Abs, Down, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	S.ProcessMouseButtonDownEvent(nullptr, Press);
	FPointerEvent Release(0, FSlateApplicationBase::CursorPointerIndex, Abs, Abs, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	S.ProcessMouseButtonUpEvent(Release);
	LastSynthFrame = GFrameCounter;
	if (S.HasAnyMouseCaptor()) UE_LOG(LogZoogi, Log, TEXT("UITEST note: a widget still holds mouse capture after the synthetic release"));
	return true;
}

FString UZoogiAwakenWidget::ControlUnder(const FVector2D& Abs) const
{
	if (!FSlateApplication::IsInitialized()) return TEXT("?");
	FSlateApplication& S = FSlateApplication::Get();
	TArray<TSharedRef<SWindow>> Windows = S.GetInteractiveTopLevelWindows();
	const FWidgetPath Path = S.LocateWindowUnderMouse(Abs, Windows);
	if (!Path.IsValid()) return TEXT("no window");
	for (const FControl& C : Controls)
	{
		if (!C.Widget.IsValid()) continue;
		TSharedPtr<SWidget> SW = C.Widget->GetCachedWidget();
		if (SW.IsValid() && Path.ContainsWidget(SW.Get())) return FString::Printf(TEXT("%s %d"), *C.Action.ToString(), C.Index);
	}
	if (AwakenPanel && AwakenPanel->GetCachedWidget().IsValid() && Path.ContainsWidget(AwakenPanel->GetCachedWidget().Get())) return TEXT("Awaken panel background");
	if (AimPanel && AimPanel->GetCachedWidget().IsValid() && Path.ContainsWidget(AimPanel->GetCachedWidget().Get())) return TEXT("aim bar background");
	return FString::Printf(TEXT("other (%s)"), *Path.Widgets.Last().Widget->GetTypeAsString());
}

void UZoogiAwakenWidget::QueueClick(const FVector2D& Abs, const FString& Expect)
{
	QueuedAbs = Abs;
	QueuedExpect = Expect;
	QueuedAction = NAME_None;
	QueuedStage = 1;
	QueuedFrame = GFrameCounter;
}

void UZoogiAwakenWidget::QueueControlClick(FName Action, int32 Index)
{
	FVector2D Abs;
	FindControl(Action, Index, Abs);
	QueueClick(Abs, FString::Printf(TEXT("%s %d"), *Action.ToString(), Index));
	QueuedAction = Action;
	QueuedIndex = Index;
}

void UZoogiAwakenWidget::TickQueuedClick()
{
	if (QueuedStage <= 0 || GFrameCounter <= QueuedFrame) return;
	if (QueuedAction != NAME_None) FindControl(QueuedAction, QueuedIndex, QueuedAbs);
	const FString Under = ControlUnder(QueuedAbs);
	const int32 Waited = int32(GFrameCounter - QueuedFrame);
	if (Under != QueuedExpect && Waited < 30) return;
	UE_LOG(LogZoogi, Log, TEXT("UITEST press: Slate hit test finds [%s], expected [%s]%s"), *Under, *QueuedExpect, Waited > 1 ? *FString::Printf(TEXT(" (after %d frames)"), Waited) : TEXT(""));
	SynthClick(QueuedAbs);
	QueuedStage = 0;
}
