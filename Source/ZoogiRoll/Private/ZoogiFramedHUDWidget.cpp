// framed: see ZoogiFramedHUDWidget.h
#include "ZoogiFramedHUDWidget.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiTeamColumnWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiHUD.h"
#include "ZoogiAwakenWidget.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiUIStyle.h"
#include "ZoogiEpisodeDirector.h"
#include "ZoogiAnimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/SlateTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiHudArt.h"
#include "ZoogiPlayerController.h"
#include "ZoogiArtifacts.h"
#include "ZoogiCardHandWidget.h"
#include "ZoogiArtifactComponent.h"
#include "UnrealEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformMisc.h"

namespace
{
	FVector2D Measure(const FString& T, const FSlateFontInfo& F)
	{
		if (!FSlateApplication::IsInitialized()) return FVector2D(T.Len() * F.Size * 0.6f, F.Size * 1.2f);
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, F);
	}
	// Rectangular ring made of four bands around (P, S): frames the arena without ever covering it.
	void FillRect(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& C);
	void FrameRing(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, float /*R*/, float W, const FLinearColor& C)
	{
		if (C.A <= 0.f || W <= 0.f) return;
		FillRect(Out, L, G, P, FVector2D(S.X, W), C);
		FillRect(Out, L, G, FVector2D(P.X, P.Y + S.Y - W), FVector2D(S.X, W), C);
		FillRect(Out, L, G, FVector2D(P.X, P.Y + W), FVector2D(W, S.Y - 2.f * W), C);
		FillRect(Out, L, G, FVector2D(P.X + S.X - W, P.Y + W), FVector2D(W, S.Y - 2.f * W), C);
	}
	void FillRect(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& C)
	{
		if (S.X <= 0.f || S.Y <= 0.f) return;
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2f(S), FSlateLayoutTransform(FVector2f(P))), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, C);
	}
}

int32 UZoogiFramedPainter::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	return Owner.IsValid() ? (bRotateLayer ? Owner->PaintRotate(Geo, Out, Layer) : Owner->PaintBack(Geo, Out, Layer)) : Layer;
}

AZoogiGameMode* UZoogiFramedHUDWidget::GM() const { return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr; }

UButton* UZoogiFramedHUDWidget::MakeButton(const FString& Label, const FLinearColor& Accent, bool bFilled, UTextBlock*& OutText, int32 FontSize, bool bPressFires)
{
	// bPressFires (Oct 15, ZOOM): non-focusable UZoogiButton that fires on the press, like the tray controls (a focusable
	// button can lose its press to the viewport focus keeper; the press is consumed by the button, never the world)
	UButton* B = WidgetTree->ConstructWidget<UButton>(bPressFires ? UZoogiButton::StaticClass() : UButton::StaticClass());
	if (bPressFires) B->SetClickMethod(EButtonClickMethod::MouseDown);
	B->SetStyle(ZoogiUIStyle::ButtonStyle(Accent, bFilled)); // hover / pressed / disabled plates
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Label));
	T->SetFont(ZoogiUIStyle::BoldFont(FontSize, 2.f));
	T->SetColorAndOpacity(FSlateColor(bFilled && Accent.G > 0.7f && Accent.R > 0.9f ? ZoogiUIStyle::Outline : FLinearColor::White));
	T->SetShadowOffset(FVector2D(2.f, 3.f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.06f, 0.5f));
	T->SetJustification(ETextJustify::Center);
	B->SetContent(T);
	UCanvasPanelSlot* S = Root->AddChildToCanvas(B);
	S->SetAutoSize(false);
	S->SetZOrder(20);
	B->SetVisibility(ESlateVisibility::Collapsed);
	OutText = T;
	return B;
}

TSharedRef<SWidget> UZoogiFramedHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FramedRoot"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Root;
		Painter = CreateWidget<UZoogiFramedPainter>(GetOwningPlayer(), UZoogiFramedPainter::StaticClass());
		Painter->Owner = this;
		Painter->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Painter)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); S->SetZOrder(0); }
		for (int32 i = 0; i < 2; ++i)
		{
			UZoogiTeamColumnWidget* C = CreateWidget<UZoogiTeamColumnWidget>(GetOwningPlayer(), UZoogiTeamColumnWidget::StaticClass());
			C->Side = i;
			C->SetVisibility(ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* S = Root->AddChildToCanvas(C)) { S->SetAutoSize(false); S->SetZOrder(5); }
			(i == 0 ? ColL : ColR) = C;
		}
		UTextBlock* Tx = nullptr;
		CamBtn = MakeButton(TEXT("CAM: TACT"), ZoogiUIStyle::RoyalBlue, false, Tx); CamText = Tx;
		MenuBtn = MakeButton(TEXT("II  MENU"), ZoogiUIStyle::Gold, true, Tx); MenuText = Tx;
		{
			// hud-v3: the plates are in the top_right art; the buttons only add hover / press light + the live label
			FButtonStyle St;
			St.SetNormal(ZoogiUIStyle::RoundBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 10.f));
			St.SetHovered(ZoogiUIStyle::RoundBrush(FLinearColor(1.f, 0.95f, 0.8f, 0.12f), FLinearColor(1.f, 0.9f, 0.6f, 0.5f), 2.f, 10.f));
			St.SetPressed(ZoogiUIStyle::RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.25f), FLinearColor(1.f, 0.95f, 0.8f, 0.7f), 2.f, 10.f));
			St.SetDisabled(ZoogiUIStyle::RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.3f), FLinearColor::Transparent, 0.f, 10.f));
			St.SetNormalPadding(FMargin(0.f)); St.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
			CamBtn->SetStyle(St); MenuBtn->SetStyle(St);
			// clearer header buttons (Evan 6:55 PM): bigger bright labels with a dark outline; icons are painted on the plates
			for (UTextBlock* T : { CamText.Get(), MenuText.Get() })
			{
				FSlateFontInfo F = ZoogiUIStyle::BoldFont(19, 2.5f);
				F.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.03f, 0.08f, 1.f);
				T->SetFont(F);
				T->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 0.97f)));
				T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f)); T->SetShadowOffset(FVector2D(0.f, 2.f));
			}
			MenuText->SetText(FText::FromString(TEXT("    MENU")));
			{ FSlateFontInfo F = CamText->GetFont(); F.Size = 16; CamText->SetFont(F); }
		}
		CamBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnCam);
		ZoomBtn = MakeButton(TEXT("ZOOM"), ZoogiUIStyle::Violet, true, Tx, 18, true); ZoomText = Tx;
		ZoomBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnZoom);
		MenuBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnPause);
		// Oct 17 (Evan): styled HUD toggle - HIDE HUD (landscape, framed) <-> SHOW HUD (portrait play, arena full screen)
		HudBtn = MakeButton(TEXT("HIDE HUD"), ZoogiUIStyle::Gold, false, Tx, 15, true); HudText = Tx;
		HudBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnHudToggle);
		RotPainter = CreateWidget<UZoogiFramedPainter>(GetOwningPlayer(), UZoogiFramedPainter::StaticClass());
		RotPainter->Owner = this;
		RotPainter->bRotateLayer = true;
		RotPainter->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(RotPainter)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); S->SetZOrder(40); }
		// Pause / menu overlay (swallows clicks while up: Visible border).
		PauseShade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PauseShade->SetBrushColor(FLinearColor(0.f, 0.02f, 0.10f, 0.62f));
		PauseShade->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(PauseShade)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); S->SetZOrder(30); }
		PausePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		PausePanel->SetBrush(ZoogiUIStyle::RoundBrush(FLinearColor(0.03f, 0.12f, 0.42f, 0.97f), ZoogiUIStyle::Outline, 5.f, 22.f));
		PausePanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(PausePanel)) { S->SetAutoSize(false); S->SetZOrder(31); }
		PauseTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		PauseTitle->SetText(FText::FromString(TEXT("PAUSED")));
		PauseTitle->SetFont(ZoogiUIStyle::BoldFont(40, 3.f));
		PauseTitle->SetColorAndOpacity(FSlateColor(ZoogiUIStyle::Gold));
		PauseTitle->SetJustification(ETextJustify::Center);
		PauseTitle->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(PauseTitle)) { S->SetAutoSize(false); S->SetZOrder(32); }
		struct FDef { const TCHAR* L; FLinearColor C; bool bF; };
		const FDef Defs[] = { { TEXT("RESUME"), ZoogiUIStyle::Gold, true }, { TEXT("SETTINGS"), ZoogiUIStyle::RoyalBlue, false }, { TEXT("RESTART MATCH"), ZoogiUIStyle::RoyalBlue, false },
			{ TEXT("MAIN MENU"), ZoogiUIStyle::RoyalBlue, false }, { TEXT("EXIT GAME"), ZoogiUIStyle::Enemy, true } };
		for (const FDef& D : Defs) { UButton* B = MakeButton(D.L, D.C, D.bF, Tx, 20); Cast<UCanvasPanelSlot>(B->Slot)->SetZOrder(33); PauseBtns.Add(B); }
		PauseBtns[0]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnResume);
		PauseBtns[1]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnSettings);
		PauseBtns[2]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnRestart);
		PauseBtns[3]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnMainMenu);
		PauseBtns[4]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnExit);
		VolDown = MakeButton(TEXT("SFX -"), ZoogiUIStyle::RoyalBlue, false, Tx, 18); Cast<UCanvasPanelSlot>(VolDown->Slot)->SetZOrder(33);
		VolUp = MakeButton(TEXT("SFX +"), ZoogiUIStyle::RoyalBlue, false, Tx, 18); Cast<UCanvasPanelSlot>(VolUp->Slot)->SetZOrder(33);
		SetCamBtn = MakeButton(TEXT("CAMERA: TACTICAL"), ZoogiUIStyle::Violet, true, Tx, 18); SetCamText = Tx; Cast<UCanvasPanelSlot>(SetCamBtn->Slot)->SetZOrder(33);
		VolDown->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnVolDown);
		VolUp->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnVolUp);
		SetCamBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnCam);
		// hud-v3: Awaken stones + INFUSE / GUARD / SAVE live in the tray (the central picker popup is gone in framed)
		{
			FButtonStyle St;
			St.SetNormal(ZoogiUIStyle::RoundBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 8.f));
			St.SetHovered(ZoogiUIStyle::RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f), FLinearColor::Transparent, 0.f, 8.f));
			St.SetPressed(ZoogiUIStyle::RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.15f), FLinearColor::Transparent, 0.f, 8.f));
			St.SetDisabled(ZoogiUIStyle::RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.35f), FLinearColor::Transparent, 0.f, 8.f));
			St.SetNormalPadding(FMargin(0.f)); St.SetPressedPadding(FMargin(0.f));
			TrayHit = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			TrayHit->SetBrushColor(FLinearColor::Transparent);
			TrayHit->SetVisibility(ESlateVisibility::Collapsed);
			TrayHit->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiFramedHUDWidget, OnTrayHitDown));
			TrayHit->OnMouseDoubleClickEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiFramedHUDWidget, OnTrayHitDown));
			if (UCanvasPanelSlot* S = Root->AddChildToCanvas(TrayHit)) { S->SetAutoSize(false); S->SetZOrder(20); }
			for (int32 i = 0; i < 7; ++i)
			{
				UButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass()); // non-focusable like the old Awaken panel's buttons (a focusable SButton lost its press to the viewport focus keeper)
				B->SetStyle(St);
				// a mouse press picks / acts at once (no hover-on-release requirement, which a stale Slate hover state broke);
				// touch keeps tap = down + up over the control
				B->SetClickMethod(EButtonClickMethod::MouseDown);
				B->SetVisibility(ESlateVisibility::Collapsed);
				if (UCanvasPanelSlot* S = Root->AddChildToCanvas(B)) { S->SetAutoSize(false); S->SetZOrder(21); }
				(i < 4 ? StoneBtns : ActBtns).Add(B);
			}
			StoneBtns[0]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnStone0);
			StoneBtns[1]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnStone1);
			StoneBtns[2]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnStone2);
			StoneBtns[3]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnStone3);
			ActBtns[0]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnInfuse);
			ActBtns[1]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnGuard);
			ActBtns[2]->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnSave);
			for (UButton* B : StoneBtns) { B->OnPressed.AddDynamic(this, &UZoogiFramedHUDWidget::OnTrayBtnPressed); }
			for (UButton* B : ActBtns) B->OnPressed.AddDynamic(this, &UZoogiFramedHUDWidget::OnTrayBtnPressed);
			// Oct 15 playtest: the energy orb is the cards-mode Awaken control (same non-focusable, press-fires button)
			OrbBtn = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
			OrbBtn->SetStyle(St);
			OrbBtn->SetClickMethod(EButtonClickMethod::MouseDown);
			OrbBtn->SetVisibility(ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* S = Root->AddChildToCanvas(OrbBtn)) { S->SetAutoSize(false); S->SetZOrder(22); }
			OrbBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnOrb);
			OrbBtn->OnPressed.AddDynamic(this, &UZoogiFramedHUDWidget::OnTrayBtnPressed);
			for (int32 i = 0; i < 2; ++i)
			{
				UButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
				FButtonStyle Hs; // fully transparent: the hand widget draws the CONFIRM / CLEAR plates underneath
				Hs.SetNormal(ZoogiUIStyle::RoundBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 8.f));
				Hs.SetHovered(ZoogiUIStyle::RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.10f), FLinearColor::Transparent, 0.f, 8.f));
				Hs.SetPressed(ZoogiUIStyle::RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.15f), FLinearColor::Transparent, 0.f, 8.f));
				Hs.SetDisabled(ZoogiUIStyle::RoundBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 8.f));
				Hs.SetNormalPadding(FMargin(0.f)); Hs.SetPressedPadding(FMargin(0.f));
				B->SetStyle(Hs);
				B->SetClickMethod(EButtonClickMethod::MouseDown);
				B->SetVisibility(ESlateVisibility::Collapsed);
				if (UCanvasPanelSlot* S = Root->AddChildToCanvas(B)) { S->SetAutoSize(false); S->SetZOrder(22); }
				(i == 0 ? CardConfirmHit : CardClearHit) = B;
			}
			CardConfirmHit->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnCardConfirm);
			CardClearHit->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnCardClear);
		}
		VolText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		VolText->SetFont(ZoogiUIStyle::BoldFont(18, 2.f));
		VolText->SetJustification(ETextJustify::Center);
		VolText->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(VolText)) { S->SetAutoSize(false); S->SetZOrder(33); }
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Super::RebuildWidget();
}

void UZoogiFramedHUDWidget::PlaceW(UWidget* W, const FVector2D& Pos, const FVector2D& Size, bool bShow)
{
	if (!W) return;
	const ESlateVisibility Want = bShow ? (W->IsA<UButton>() || W == PauseShade || W == TrayHit ? ESlateVisibility::Visible : (W->IsA<UZoogiTeamColumnWidget>() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::HitTestInvisible)) : ESlateVisibility::Collapsed;
	if (W->GetVisibility() != Want) W->SetVisibility(Want);
	if (!bShow) return;
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot)) { S->SetPosition(Pos); S->SetSize(Size); }
}

void UZoogiFramedHUDWidget::SetPaused(bool bPause)
{
	bPaused = bPause;
	if (!bPause) bSettings = false;
	UGameplayStatics::SetGamePaused(this, bPause);
	UE_LOG(LogZoogi, Log, TEXT("FRAMED %s"), bPause ? TEXT("paused (menu open)") : TEXT("resumed"));
}

void UZoogiFramedHUDWidget::OnZoom()
{
	// the click does what the button showed (ON -> off, ZOOM -> on), then the label re-reads the controller at once
	AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetOwningPlayer());
	if (!PC) return;
	UE_LOG(LogZoogi, Log, TEXT("AIMZOOM HUD button click (showed %s)"), bZoomShownOn ? TEXT("ON") : TEXT("ZOOM"));
	PC->SetZoomMode(!bZoomShownOn, TEXT("HUD ZOOM button"));
	bZoomShownOn = PC->IsZoomShownOn();
	if (ZoomText) ZoomText->SetText(FText::FromString(bZoomShownOn ? TEXT("ZOOM: ON") : TEXT("ZOOM")));
}
void UZoogiFramedHUDWidget::OnCam() { if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("CamStyle"), 0); }
void UZoogiFramedHUDWidget::OnPause() { SetPaused(!bPaused); }
void UZoogiFramedHUDWidget::OnResume() { SetPaused(false); }
void UZoogiFramedHUDWidget::OnSettings() { bSettings = !bSettings; }
void UZoogiFramedHUDWidget::OnRestart() { SetPaused(false); if (AZoogiGameMode* G = GM()) { UE_LOG(LogZoogi, Log, TEXT("FRAMED restart")); G->MenuAction(TEXT("Rematch"), 0); } }
void UZoogiFramedHUDWidget::OnMainMenu() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Menu"), 0); }
void UZoogiFramedHUDWidget::OnExit() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Quit"), 0); }
void UZoogiFramedHUDWidget::OnVolDown() { if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("SfxVol"), FMath::Clamp(FMath::RoundToInt(ZoogiUtil::GetSfxVolume() * 100.f) - 10, 0, 100)); }
void UZoogiFramedHUDWidget::OnVolUp() { if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("SfxVol"), FMath::Clamp(FMath::RoundToInt(ZoogiUtil::GetSfxVolume() * 100.f) + 10, 0, 100)); }

void UZoogiFramedHUDWidget::RouteTray(AZoogiGameMode* G, float s)
{
	// The existing UMG Awaken panel / aim bar are anchored bottom-centre in a full-viewport canvas: scale that widget
	// about its bottom-centre so the visible panel fits inside the tray under the arena (Slate hit-testing follows
	// the render transform, so its buttons keep working). The card hand fans up from the bottom edge already, i.e.
	// into the tray; its world-target projection stays in full-viewport widget space and is unaffected.
	APlayerController* PC = GetOwningPlayer();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	UZoogiAwakenWidget* AW = Hud ? Hud->GetAwakenWidget() : nullptr;
	if (!AW) return;
	if (!bActive)
	{
		if (bTrayScaled) { AW->SetRenderTransform(FWidgetTransform()); bTrayScaled = false; }
		return;
	}
	const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
	if (L.bPortrait && !AW->ShowsAwaken() && !AW->ShowsCards())
	{
		// portrait play (HUD hidden): the aim / artifact info panel would float over the full-screen arena - keep it out of
		// the way (collapsed to a point at the bottom centre, so it can't catch drags either); the top strip carries the prompt
		AW->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
		AW->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(0.001f, 0.001f), FVector2D::ZeroVector, 0.f));
		bTrayScaled = true;
		return;
	}
	const float Avail = (L.Viewport.Y - L.Safe.Bottom - L.Tray.Min.Y - 6.f * L.U) * s; // v2: no prompt bar in the tray
	float Need = 1.f;
	if (AW->ShowsAwaken()) Need = FMath::Max(Need, 466.f);
	if (AW->ShowsCards()) Need = FMath::Max(Need, 520.f);
	if (AW->ShowsAim()) Need = FMath::Max(Need, 214.f);
	float Sc = FMath::Clamp(Avail / Need, 0.4f, 1.f);
	Sc = FMath::Min(Sc, FMath::Max(0.4f, (L.Tray.Max.X - L.Tray.Min.X) * s / 1220.f));
	AW->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	AW->SetRenderTransform(FWidgetTransform(FVector2D(0.f, -L.Safe.Bottom * s), FVector2D(Sc, Sc), FVector2D::ZeroVector, 0.f));
	bTrayScaled = true;
}

void UZoogiFramedHUDWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	Time += Dt;
	AZoogiGameMode* G = GM();
	const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
	bActive = G && ZoogiFramed::IsActive(GetWorld()) && L.bValid;
	const float s = L.Viewport.X > 0.f ? Geo.GetLocalSize().X / L.Viewport.X : 1.f;
	LocalScale = s;
	RouteTray(G, s);
	if (!bActive)
	{
		if (bPaused) SetPaused(false);
		// Oct 17 (Evan): leaving battle -> rotate prompt back to portrait (menus are portrait); it keeps running over the menu
		if (bWasActive) { bWasActive = false; StartBattleEdgePrompt(false, Geo.GetLocalSize()); }
		if (RotateT >= 0.f && RotateReason == 2) { RotateT += FMath::Min(Dt, 0.1f); if (RotateT >= 2.0f) RotateT = -1.f; }
		else RotateT = -1.f;
		ZoogiFramed::SetUiRects(TArray<FBox2D>());
		for (UWidget* W : { (UWidget*)HudBtn.Get(), (UWidget*)ColL.Get(), (UWidget*)ColR.Get(), (UWidget*)CamBtn.Get(), (UWidget*)ZoomBtn.Get(), (UWidget*)MenuBtn.Get(), (UWidget*)PauseShade.Get(), (UWidget*)PausePanel.Get(), (UWidget*)PauseTitle.Get(),
			(UWidget*)VolDown.Get(), (UWidget*)VolUp.Get(), (UWidget*)VolText.Get(), (UWidget*)SetCamBtn.Get() }) PlaceW(W, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : PauseBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : StoneBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : ActBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		PlaceW(OrbBtn, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		PlaceW(CardConfirmHit, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		PlaceW(CardClearHit, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		ZoogiFramed::SetZoomButtonRect(FBox2D(ForceInit), false);
		PlaceW(TrayHit, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		if (APlayerController* PC = GetOwningPlayer()) if (AZoogiHUD* H = Cast<AZoogiHUD>(PC->GetHUD())) if (UZoogiAwakenWidget* AW = H->GetAwakenWidget()) AW->ClearTrayControls();
		LastStage = LastActive = LastPhase = -1;
		return;
	}
	const float u = L.U * s;
	// Oct 14: HUD frame shake on hard impacts (frame art + team columns; buttons stay put so taps never miss)
	{
		const FVector2D ShakeOff = ZoogiFramed::FrameShakeOffset(Dt) * u;
		for (UWidget* W : { (UWidget*)Painter.Get(), (UWidget*)ColL.Get(), (UWidget*)ColR.Get() }) if (W) W->SetRenderTranslation(ShakeOff);
	}
	auto Box = [&](const FBox2D& B) { return TPair<FVector2D, FVector2D>(B.Min * s, (B.Max - B.Min) * s); };
	const auto LB = Box(L.Left), RB = Box(L.Right);
	ColL->UScale = ColR->UScale = u * (L.bCompact ? 0.86f : 1.f);
	ColL->bCompact = ColR->bCompact = L.bCompact;
	ColL->TScale = ColR->TScale = L.T * s;
	PlaceW(ColL, LB.Key, LB.Value, !L.bPortrait); // portrait play: no team columns (the top indicator carries turn / score)
	PlaceW(ColR, RB.Key, RB.Value, !L.bPortrait);
	// header buttons: on the CAM / MENU plates of the top_right art (>= 48 dp tap targets)
	TArray<FBox2D> UiRects; // portrait: edge controls + tray overlay count as UI (viewport px)
	if (!L.bPortrait)
	{
		const float t = L.T * s, X1 = (L.Viewport.X - L.Safe.Right) * s, Y0 = L.Safe.Top * s;
		const float BH = FMath::Max(34.f * t, ZoogiHudArt::TapPx(48.f, 48.f * u)), BY = Y0 + 26.f * t - BH * 0.5f;
		PlaceW(MenuBtn, FVector2D(X1 - 126.f * t, BY), FVector2D(FMath::Max(104.f * t, BH), BH), true);
		PlaceW(CamBtn, FVector2D(X1 - 255.f * t, BY), FVector2D(FMath::Max(117.f * t, BH), BH), true);
		// Oct 17: HIDE HUD toggle just left of the CAM / MENU plate
		const float HW = FMath::Max(112.f * t, BH * 2.2f);
		PlaceW(HudBtn, FVector2D(X1 - 282.f * t - 10.f * t - HW, BY), FVector2D(HW, BH), RotateT < 0.f);
	}
	else
	{
		PlaceW(MenuBtn, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		PlaceW(CamBtn, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		PlaceW(HudBtn, L.HudBtn.Min * s, L.HudBtn.GetSize() * s, RotateT < 0.f);
		UiRects.Add(L.HudBtn); UiRects.Add(L.Indicator);
		if (PortraitTrayShown()) UiRects.Add(L.Tray);
	}
	if (HudText) HudText->SetText(FText::FromString(L.bPortrait ? TEXT("SHOW HUD") : TEXT("HIDE HUD")));
	// Oct 14 aim ZOOM toggle: bottom-left of the arena window while the human aims (>= 48 dp, also clickable)
	{
		AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetOwningPlayer());
		const bool bShowZ = PC && !bPaused && (PC->CanShowZoom() || PC->IsZoomShownOn());
		const float t = L.T * s;
		float ZH = FMath::Max(40.f * t, ZoogiHudArt::TapPx(48.f, 48.f * u)), ZW = FMath::Max(132.f * t, ZH * 2.4f);
		FVector2D ZP(L.Arena.Min.X * s + 14.f * t, L.Arena.Max.Y * s - ZH - 14.f * t);
		if (L.bPortrait) { ZP = L.ZoomBtn.Min * s; ZW = L.ZoomBtn.GetSize().X * s; ZH = L.ZoomBtn.GetSize().Y * s; } // portrait: top-right edge
		PlaceW(ZoomBtn, ZP, FVector2D(ZW, ZH), bShowZ);
		// single source: the controller's mode (button, wheel, = -, Z, pinch all land there); reset when the mode ends
		const bool bOnNow = PC && PC->IsZoomShownOn();
		if (ZoomText && (bOnNow != bZoomShownOn || !bZoomLabelInit)) { ZoomText->SetText(FText::FromString(bOnNow ? TEXT("ZOOM: ON") : TEXT("ZOOM"))); bZoomLabelInit = true; }
		bZoomShownOn = bOnNow;
		if (ZoomBtn && bShowZ) ZoomBtn->SetRenderOpacity(bOnNow ? 1.f : 0.85f);
		ZoogiFramed::SetZoomButtonRect(s > 0.f ? FBox2D(ZP / s, (ZP + FVector2D(ZW, ZH)) / s) : FBox2D(ForceInit), bShowZ);
	}
	ZoogiFramed::SetUiRects(UiRects);
	// Oct 17 (Evan): entering battle -> rotate prompt when the battle's view (HUD shown = landscape, hidden = portrait) differs
	if (!bWasActive)
	{
		bWasActive = true;
		if (RotateReason == 2) RotateT = -1.f;
		bModeEventPending = true; ModeEventT = 0.f; bRotateApplied = false; // every match entry reports its mode (and tells the camera) once
		StartBattleEdgePrompt(true, Geo.GetLocalSize());
	}
	// Oct 17: HUD toggle -> rotate prompt; the layout + platform orientation switch at mid-prompt
	if (RotateT >= 0.f)
	{
		RotateT += FMath::Min(Dt, 0.1f);
		if (!bRotateApplied && RotateT >= 0.95f) { bRotateApplied = true; ApplyOrientation(bRotateTo); bModeEventPending = true; ModeEventT = 0.f; }
		if (RotateT >= 2.0f) RotateT = -1.f;
	}
	TickViewModeEvent(Dt);
	TickHudToggleTest(G, Dt);
	CamText->SetText(FText::FromString(G->bCinematicCam ? TEXT("      CAM: CINE") : TEXT("      CAM: TACT"))); // leading space = room for the camera icon
	CamBtn->SetIsEnabled(G->Phase != EZoogiPhase::GameOver && G->Phase != EZoogiPhase::Forming); // disabled state when the camera style can't matter
	// pause / menu overlay
	const FVector2D V = Geo.GetLocalSize();
	PlaceW(PauseShade, FVector2D::ZeroVector, V, bPaused);
	const float PW = 460.f * u, PBH = 58.f * u, PG = 14.f * u;
	const int32 NB = PauseBtns.Num();
	const float PH = 110.f * u + NB * (PBH + PG) + (bSettings ? 150.f * u : 0.f);
	const FVector2D PP((V.X - PW) * 0.5f, (V.Y - PH) * 0.5f);
	PlaceW(PausePanel, PP, FVector2D(PW, PH), bPaused);
	PlaceW(PauseTitle, PP + FVector2D(0.f, 22.f * u), FVector2D(PW, 60.f * u), bPaused);
	float Y = PP.Y + 100.f * u;
	for (int32 i = 0; i < NB; ++i)
	{
		PlaceW(PauseBtns[i], FVector2D(PP.X + 40.f * u, Y), FVector2D(PW - 80.f * u, PBH), bPaused);
		Y += PBH + PG;
		if (i == 1 && bSettings)
		{
			const int32 Vol = FMath::RoundToInt(ZoogiUtil::GetSfxVolume() * 100.f);
			PlaceW(VolDown, FVector2D(PP.X + 40.f * u, Y), FVector2D(110.f * u, 50.f * u), true);
			PlaceW(VolText, FVector2D(PP.X + 150.f * u, Y + 10.f * u), FVector2D(PW - 300.f * u, 40.f * u), true);
			PlaceW(VolUp, FVector2D(PP.X + PW - 150.f * u, Y), FVector2D(110.f * u, 50.f * u), true);
			VolText->SetText(FText::FromString(FString::Printf(TEXT("SFX %d%%"), Vol)));
			VolDown->SetIsEnabled(Vol > 0);
			VolUp->SetIsEnabled(Vol < 100);
			PlaceW(SetCamBtn, FVector2D(PP.X + 40.f * u, Y + 64.f * u), FVector2D(PW - 80.f * u, 50.f * u), true);
			SetCamText->SetText(FText::FromString(G->bCinematicCam ? TEXT("CAMERA: CINEMATIC") : TEXT("CAMERA: TACTICAL")));
			Y += 150.f * u;
		}
	}
	if (!bPaused || !bSettings) { PlaceW(VolDown, FVector2D::ZeroVector, FVector2D::ZeroVector, false); PlaceW(VolUp, FVector2D::ZeroVector, FVector2D::ZeroVector, false); PlaceW(VolText, FVector2D::ZeroVector, FVector2D::ZeroVector, false); PlaceW(SetCamBtn, FVector2D::ZeroVector, FVector2D::ZeroVector, false); }
	PauseBtns[2]->SetIsEnabled(G->Phase != EZoogiPhase::Forming);
	StonePop = FMath::Min(1.5f, StonePop + Dt * 3.f);
	TickTrayAwaken();

	// feedback animations + frame light pulses on major events
	StageAnim = FMath::Min(1.f, StageAnim + Dt * 2.f);
	TurnAnim = FMath::Min(1.f, TurnAnim + Dt * 2.f);
	PromptAnim = FMath::Min(1.f, PromptAnim + Dt * 4.f);
	if ((int32)G->Stage != LastStage) { if (LastStage >= 0) { StageAnim = 0.f; if (G->Stage == EZoogiStage::Battle) ZoogiFramed::Pulse(ZoogiUIStyle::Violet); } LastStage = (int32)G->Stage; }
	if (G->ActivePlayer != LastActive) { if (LastActive >= 0) TurnAnim = 0.f; LastActive = G->ActivePlayer; }
	if ((int32)G->Phase != LastPhase)
	{
		if (G->Phase == EZoogiPhase::GameOver && LastPhase >= 0) { EndAnim = 0.001f; ZoogiFramed::Pulse(ZoogiUIStyle::Gold); UE_LOG(LogZoogi, Log, TEXT("FRAMED match end shown (winner P%d)"), G->WinnerIndex + 1); }
		if (G->Phase != EZoogiPhase::GameOver) EndAnim = 0.f;
		LastPhase = (int32)G->Phase;
	}
	if (EndAnim > 0.f) EndAnim = FMath::Min(1.f, EndAnim + Dt);
	if (G->BigAge < LastBigAge && G->BigAge < 0.2f) ZoogiFramed::Pulse(G->BigColor); // knock-out / big callout
	LastBigAge = G->BigAge;
	// -ZoogiFramedNavTest: drive the real nav buttons' handlers once (pause -> settings -> resume, restart at match end)
	static const bool bNavTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFramedNavTest"));
	if (bNavTest)
	{
		static int32 NavStep = 0;
		static float NavT = 0.f;
		NavT += Dt;
		if (NavStep == 0 && G->Stage == EZoogiStage::Battle && G->RoundNumber >= 1 && G->Phase == EZoogiPhase::Aiming) { OnPause(); OnSettings(); NavStep = 1; NavT = 0.f; }
		else if (NavStep == 1 && NavT > 0.6f) { G->TakeShot(TEXT("framed_pause_menu")); NavStep = 2; NavT = 0.f; }
		else if (NavStep == 2 && NavT > 0.6f) { OnResume(); UE_LOG(LogZoogi, Log, TEXT("FRAMED navtest: pause/settings/resume ok (paused=%d)"), UGameplayStatics::IsGamePaused(this) ? 1 : 0); NavStep = 3; }
		else if (NavStep == 3 && G->Phase == EZoogiPhase::GameOver && G->PhaseTime > 2.2f) { G->TakeShot(TEXT("framed_match_end")); OnRestart(); NavStep = 4; NavT = 0.f; }
		else if (NavStep == 4 && NavT > 3.f) { UE_LOG(LogZoogi, Log, TEXT("FRAMED navtest: restart -> phase %d round %d (new match running=%d)"), (int32)G->Phase, G->RoundNumber, G->Phase != EZoogiPhase::GameOver ? 1 : 0); NavStep = 5; }
	}
	const FString P = PromptText(G);
	if (P != LastPrompt) { LastPrompt = P; PromptAnim = 0.f; }
}

FString UZoogiFramedHUDWidget::ActionLabel(const AZoogiGameMode* G)
{
	if (!G) return FString();
	if (UZoogiAnimeSubsystem* Anime = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr) if (Anime->bPlaying) return TEXT("EPISODE");
	if (G->Phase == EZoogiPhase::Rolling) return TEXT("ROLLING");
	if (G->Phase == EZoogiPhase::Resolving) return TEXT("RESOLVING");
	if (G->Stage == EZoogiStage::Draw) return TEXT("DRAW");
	if (G->Stage == EZoogiStage::Awaken) return G->bAwakenDone ? TEXT("AWAKEN  DONE") : (G->CardsOn() ? TEXT("STANDBY") : TEXT("AWAKEN"));
	if (G->Stage == EZoogiStage::Battle) return FString::Printf(TEXT("BATTLE: %s"), AZoogiGameMode::StepName(G->BattleStep)).ToUpper();
	return FString(AZoogiGameMode::StageName(G->Stage)).ToUpper();
}

FString UZoogiFramedHUDWidget::PromptText(const AZoogiGameMode* G)
{
	if (!G) return FString();
	if (G->Phase == EZoogiPhase::Forming) return TEXT("The arena is forming...  click or press SPACE to skip");
	if (G->Phase == EZoogiPhase::GameOver) return FString::Printf(TEXT("MATCH OVER  -  %s   REMATCH or MENU (results in the arena)"), *G->Banner);
	if (UZoogiAnimeSubsystem* Anime = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr) if (Anime->bPlaying) return TEXT("Episode playing...  click / SPACE to skip");
	const AZoogiBall* B = G->GetActiveBall();
	if (!B) return FString();
	if (G->Phase == EZoogiPhase::Rolling) return TEXT("Rolling...");
	if (G->Phase == EZoogiPhase::Resolving) return TEXT("Resolving...");
	if (G->Stage == EZoogiStage::Draw) return TEXT("Drawing...");
	if (G->Stage == EZoogiStage::Awaken)
	{
		if (G->bAwakenDone) return G->AwakenChoiceText.IsEmpty() ? TEXT("Awaken done") : G->AwakenChoiceText;
		if (B->bCPU) return TEXT("CPU is awakening...");
		return G->CardsOn() ? TEXT("STANDBY: tap the orb (A) to choose your Awaken, play cards (drag onto a target or 1-7), then CONFIRM") : TEXT("Pick a card, then INFUSE / GUARD / SAVE");
	}
	if (B->bCPU) return G->TargetLock.IsValid() ? FString::Printf(TEXT("CPU is targeting %s"), *G->TargetName(G->TargetLock.Get())) : TEXT("CPU is choosing a target");
	if (G->IsPowerUpStep()) return TEXT("POWER UP - launching!");
	if (G->HasAim()) return FString::Printf(TEXT("Aiming at %s. Press, hold and DRAG BACK to set power - release launches (drag back to the Zoogi cancels)"), G->bFreeAim ? TEXT("FLOOR POINT") : *G->TargetName(G->TargetLock.Get()).ToUpper());
	return TEXT("Tap a rival or orb to lock on, or the floor to aim - then drag back like a slingshot");
}

FString UZoogiFramedHUDWidget::HintText(const AZoogiGameMode* G) const
{
	if (G && G->bComboMode) return TEXT("Awaken: card + I/G/S   Battle: Q switch, click target, A/D cut, 1-3 force, SPACE   Follow-Up F/X   C camera   Esc menu");
	if (G && G->CardsOn()) return TEXT("Standby: cards 1-5 + Enter, Backspace clears   Aim: tap target / floor, drag back + release to launch (A/D cut)   RMB cancel   C camera");
	return TEXT("Awaken: card + I/G/S   Aim: click target / floor, A/D cut, Up/Down power, SPACE   RMB cancel   C camera   Ctrl+R restart");
}

int32 UZoogiFramedHUDWidget::PaintBack(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer) const
{
	// hud-v3 (Evan's 3:53 PM target): dark stone + steel frame sliced from his art (ZoogiHudArt), live text on top.
	using namespace ZoogiUIStyle;
	namespace HA = ZoogiHudArt;
	const AZoogiGameMode* G = GM();
	const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
	if (!bActive || !G || !L.bValid) return Layer;
	const float s = LocalScale, t = L.T * s;
	const FVector2D V = Geo.GetLocalSize();
	const FVector2D A0 = L.Arena.Min * s, A1 = L.Arena.Max * s, AS = A1 - A0;
	const float X0 = L.Safe.Left * s, X1 = V.X - L.Safe.Right * s, Y0 = L.Safe.Top * s;
	int32 Ly = Layer;
	auto FontT = [&](float Sz) { return BoldFont(FMath::Max(9, FMath::RoundToInt(Sz * t)), FMath::Max(1.f, 1.6f * t)); };
	auto TxtT = [&](int32 Lay, const FVector2D& P, const FString& Tx, float Sz, const FLinearColor& C, float Ax = 0.f, float Ay = 0.5f) { HA::Text(Out, Lay, Geo, P, Tx, FontT(Sz), C, Ax, Ay); };

	// 1) dark stone in the four margins only (never over the arena rect); portrait play: the arena is the whole screen
	FLinearColor PulseC; const float Pulse = ZoogiFramed::PulseAmount(PulseC);
	if (!L.bPortrait)
	{
	const FLinearColor Bg(0.035f, 0.035f, 0.04f, 1.f), StoneTint(0.42f, 0.42f, 0.44f, 1.f);
	const FVector2D Bands[4][2] = { { FVector2D(0.f, 0.f), FVector2D(V.X, A0.Y) }, { FVector2D(0.f, A1.Y), FVector2D(V.X, V.Y - A1.Y) },
		{ FVector2D(0.f, A0.Y), FVector2D(A0.X, AS.Y) }, { FVector2D(A1.X, A0.Y), FVector2D(V.X - A1.X, AS.Y) } };
	for (const auto& Bd : Bands) { FillRect(Out, Ly, Geo, Bd[0], Bd[1], Bg); HA::Tiled(Out, Ly + 1, Geo, Bd[0], Bd[1], TEXT("stone_tile"), 340.f, t, StoneTint); }

	// 2) arena frame: steel bands + gold / crystal corner brackets with a cyan glow (light pulse on major events)
	HA::Img(Out, Ly + 3, Geo, FVector2D(A0.X - 16.f * t, A0.Y), FVector2D(16.f * t, AS.Y), TEXT("edge_l"));
	HA::Img(Out, Ly + 3, Geo, FVector2D(A1.X, A0.Y), FVector2D(16.f * t, AS.Y), TEXT("edge_r"));
	HA::Img(Out, Ly + 3, Geo, FVector2D(A0.X - 16.f * t, A0.Y - 16.f * t), FVector2D(AS.X + 32.f * t, 16.f * t), TEXT("edge_t"));
	HA::Img(Out, Ly + 3, Geo, FVector2D(A0.X - 16.f * t, A1.Y), FVector2D(AS.X + 32.f * t, 16.f * t), TEXT("edge_b"));
	{
		const float Gl = 0.35f + 0.25f * FMath::Sin(Time * 2.4f) + 0.6f * Pulse;
		const FLinearColor GC = FMath::Lerp(HA::Cyan, PulseC, Pulse * 0.7f) * FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Gl, 0.f, 1.f) * 0.45f);
		const FVector2D CS(54.f * t, 56.f * t);
		const FVector2D CP[4] = { A0 - FVector2D(16.f, 14.f) * t, FVector2D(A1.X - 38.f * t, A0.Y - 14.f * t), FVector2D(A0.X - 16.f * t, A1.Y - 38.f * t), FVector2D(A1.X - 38.f * t, A1.Y - 38.f * t) };
		const TCHAR* CN[4] = { TEXT("corner_tl"), TEXT("corner_tr"), TEXT("corner_bl"), TEXT("corner_br") };
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2D Cc = CP[i] + CS * 0.5f + FVector2D(i % 2 ? 10.f : -10.f, i >= 2 ? 10.f : -10.f) * t;
			ZoogiUIStyle::SBox(Out, Ly + 3, Geo, Cc - FVector2D(30.f, 30.f) * t, FVector2D(60.f, 60.f) * t, GC, FLinearColor::Transparent, 30.f * t, 0.f);
			HA::Img(Out, Ly + 4, Geo, CP[i], CS, CN[i]);
		}
	}
	}

	// hud-v3 overlay (Evan 4:25 PM): his target image as ONE full-frame overlay; arena / pills / plate are holes,
	// live values are drawn on top (layer 6+), the sliced pieces underneath only show through the holes.
	const bool bOverlay = !L.bPortrait && HA::Tex(TEXT("overlay")) != nullptr;
	if (bOverlay)
	{
		// split at the arena edges: the left (0..380) and right (966..1376) target strips keep their shape, the middle
		// strip (arena + card tray recess) stretches. Part() takes source px in texture/K units; the overlay is 1x.
		const float OH = V.Y - L.Safe.Bottom * s - Y0, LW = 380.f * t, RW = 410.f * t;
		const FVector2D SK = HA::SizeT(TEXT("overlay")) / FVector2D(1376.f, 768.f);
		auto Strip = [&](float Dx, float Dw, float Sx0, float Sx1, float Sy0 = 0.f, float Sy1 = 768.f)
		{
			if (Dw > 0.5f) HA::Part(Out, Ly + 5, Geo, FVector2D(Dx, Y0 + Sy0 / 768.f * OH), FVector2D(Dw, (Sy1 - Sy0) / 768.f * OH), TEXT("overlay"), FBox2D(FVector2D(Sx0 * SK.X, Sy0 * SK.Y), FVector2D(Sx1 * SK.X, Sy1 * SK.Y)));
		};
		Strip(X0, LW, 0.f, 380.f);
		// middle: the top band (pills + plate, rows 0..74) keeps its shape centred on the arena, only its plain ends stretch
		const float MX0 = X0 + LW, MX1 = X1 - RW, Xm = (A0.X + A1.X) * 0.5f, BW = (952.f - 425.f) * t;
		Strip(MX0, Xm - BW * 0.5f - MX0, 380.f, 425.f, 0.f, 74.f);
		Strip(Xm - BW * 0.5f, BW, 425.f, 952.f, 0.f, 74.f);
		Strip(Xm + BW * 0.5f, MX1 - Xm - BW * 0.5f, 952.f, 966.f, 0.f, 74.f);
		Strip(MX0, MX1 - MX0, 380.f, 966.f, 74.f, 768.f);
		Strip(X1 - RW, RW, 966.f, 1376.f);
	}

	// header buttons (Evan 6:55 PM "top right menu could be clearer"): gold MENU edge glow + brighter plate, camera / menu icons
	if (bOverlay)
	{
		const float CY = Y0 + 26.5f * t;
		const FLinearColor Ink(0.02f, 0.03f, 0.08f, 1.f), Bright(1.f, 1.f, 0.97f, 1.f);
		const FVector2D MP(X1 - 126.f * t, Y0 + 11.f * t), MS(104.f * t, 31.f * t);
		const float Gw = 0.55f + 0.25f * FMath::Sin(Time * 3.f);
		ZoogiUIStyle::SBox(Out, Ly + 6, Geo, MP - FVector2D(5.f, 5.f) * t, MS + FVector2D(10.f, 10.f) * t, FLinearColor(1.f, 0.78f, 0.25f, 0.18f * Gw), FLinearColor(1.f, 0.82f, 0.3f, 0.9f * Gw), 9.f * t, 2.5f * t);
		ZoogiUIStyle::SBox(Out, Ly + 6, Geo, MP, MS, FLinearColor(0.95f, 0.62f, 0.08f, 0.22f), FLinearColor(1.f, 0.88f, 0.45f, 0.9f), 7.f * t, 1.5f * t); // deeper gold, bright rim
		// hamburger icon (3 bars, bright with a dark outline)
		for (int32 i = 0; i < 3; ++i)
			ZoogiUIStyle::SBox(Out, Ly + 7, Geo, FVector2D(MP.X + 12.f * t, CY - 7.f * t + i * 6.f * t), FVector2D(17.f * t, 3.6f * t), Bright, Ink, 1.8f * t, 1.2f * t);
		// camera icon on the CAM plate: body, viewfinder bump, lens
		const FVector2D CI(X1 - 255.f * t + 9.f * t, CY - 5.f * t);
		ZoogiUIStyle::SBox(Out, Ly + 7, Geo, CI + FVector2D(3.f, -2.5f) * t, FVector2D(6.f, 3.5f) * t, Bright, Ink, 1.5f * t, 1.2f * t);
		ZoogiUIStyle::SBox(Out, Ly + 7, Geo, CI, FVector2D(16.f, 11.f) * t, Bright, Ink, 2.5f * t, 1.3f * t);
		ZoogiUIStyle::SBox(Out, Ly + 8, Geo, CI + FVector2D(4.5f, 2.f) * t, FVector2D(7.f, 7.f) * t, FLinearColor(0.15f, 0.55f, 0.85f, 1.f), Ink, 3.5f * t, 1.1f * t);
	}

	// 3) top bar: title plate (round / orbs), phase pills + active player / map plate, CAM / MENU plate
	const AZoogiBall* AB = G->GetActiveBall();
	if (L.bPortrait) PaintPortraitStrip(Geo, Out, Ly + 6);
	else
	{
		HA::ImgT(Out, Ly + 2, Geo, FVector2D(X0, Y0), t, TEXT("top_title"));
		TxtT(Ly + 6, FVector2D(X0 + 182.5f * t, Y0 + 34.5f * t), FString::Printf(TEXT("%d / %d"), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds), 11.f, FLinearColor::White, 0.5f);
		if (G->bComboMode)
		{
			ZoogiUIStyle::SBox(Out, Ly + 5, Geo, FVector2D(X0 + 220.f * t, Y0 + 9.f * t), FVector2D(64.f * t, 15.f * t), FLinearColor(0.11f, 0.10f, 0.09f, 1.f), FLinearColor::Transparent, 3.f * t, 0.f);
			TxtT(Ly + 6, FVector2D(X0 + 226.f * t, Y0 + 16.5f * t), TEXT("SCORE"), 7.5f, HA::Steel);
			TxtT(Ly + 6, FVector2D(X0 + 226.f * t, Y0 + 34.5f * t), FString::Printf(TEXT("%d - %d"), G->TeamScore(0), G->TeamScore(1)), 11.f, HA::GoldText);
		}
		else TxtT(Ly + 6, FVector2D(X0 + 226.f * t, Y0 + 34.5f * t), FString::FromInt(G->OrbsLeft()), 11.f, FLinearColor::White);
		HA::ImgT(Out, Ly + 2, Geo, FVector2D(X1 - 282.f * t, Y0), t, TEXT("top_right"));
		// pills centred on the arena; the plate hangs under them
		const float Xc = A0.X + AS.X * 0.5f;
		const FVector2D BarP(Xc - 270.f * t, Y0);
		HA::ImgT(Out, Ly + 2, Geo, FVector2D(Xc - 90.f * t, Y0 + 26.f * t), t, TEXT("name_plate"));
		HA::ImgT(Out, Ly + 3, Geo, BarP, t, TEXT("pills_bar"));
		const int32 Cur = (G->Phase == EZoogiPhase::Forming) ? -1 : (int32)G->Stage;
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector2D PP = BarP + FVector2D(14.f + 105.f * i, 6.f) * t, PS(95.f * t, 24.f * t);
			const bool bNow = i == Cur, bDone = Cur >= 0 && i < Cur;
			FString Lb = FString(AZoogiGameMode::StageName((EZoogiStage)i)).ToUpper();
			if (bDone) { HA::Img(Out, Ly + 4, Geo, PP, PS, TEXT("pill_done")); Lb += TEXT(" >"); }
			if (bNow)
			{
				const float Pop = 1.f + 0.12f * (1.f - StageAnim) * FMath::Sin(StageAnim * PI);
				const FVector2D OS = FVector2D(156.f, 50.f) * t * Pop, OC = PP + PS * 0.5f;
				HA::Img(Out, Ly + 4, Geo, OC - OS * 0.5f + FVector2D(0.f, 1.f * t), OS, TEXT("pill_on"), FLinearColor(1.f, 1.f, 1.f, 0.85f + 0.15f * FMath::Sin(Time * 5.f)));
				ZoogiUIStyle::SBox(Out, Ly + 3, Geo, OC - FVector2D(70.f, 22.f) * t * Pop, FVector2D(140.f, 44.f) * t * Pop, FLinearColor(HA::Cyan.R, HA::Cyan.G, HA::Cyan.B, 0.28f + 0.12f * FMath::Sin(Time * 5.f)), FLinearColor::Transparent, 22.f * t, 0.f);
				HA::Img(Out, Ly + 4, Geo, OC - OS * 0.5f + FVector2D(0.f, 1.f * t), OS, TEXT("pill_on"), FLinearColor(1.2f, 1.2f, 1.2f, 0.6f)); // second pass = stronger glow
			}
			TxtT(Ly + 6, PP + PS * 0.5f, Lb, bNow ? 10.5f : 9.5f, bNow ? FLinearColor::White : (bDone ? FLinearColor(0.95f, 1.f, 0.96f) : FLinearColor(0.97f, 0.96f, 0.93f)), 0.5f);
		}
		// active player pill text + map name (the plate shows the current map on every arena)
		const FVector2D NP(Xc - 90.f * t, Y0 + 26.f * t);
		FString Who;
		if (G->Phase == EZoogiPhase::Forming) Who = TEXT("STAGE FORMING");
		else if (G->Phase == EZoogiPhase::GameOver) Who = TEXT("MATCH OVER");
		else if (AB) Who = FString::Printf(TEXT("P%d %s (%s)"), AB->PlayerIndex + 1, *AB->Def.Name.ToUpper(), AB->bCPU ? TEXT("CPU") : TEXT("YOU"));
		const float TPop = 1.f + 0.2f * (1.f - TurnAnim) * FMath::Sin(TurnAnim * PI);
		const FSlateFontInfo WF = FontT(8.5f * TPop);
		const FVector2D WS = HA::Measure(Who, WF);
		const FVector2D WC = NP + FVector2D(94.5f, 13.f) * t;
		if (AB) ZoogiUIStyle::SBox(Out, Ly + 5, Geo, FVector2D(WC.X - WS.X * 0.5f - 11.f * t, WC.Y - 3.f * t), FVector2D(6.f * t, 6.f * t), ZoogiUtil::PlayerColor(AB->PlayerIndex), FLinearColor::Transparent, 3.f * t, 0.f);
		HA::Text(Out, Ly + 6, Geo, WC + FVector2D(4.f * t, 0.f), Who, WF, FLinearColor::White, 0.5f, 0.5f);
		const UEnum* E = StaticEnum<EZoogiArena>();
		FString Map = E ? E->GetDisplayNameTextByValue((int64)G->MenuArena).ToString().ToUpper() : TEXT("ARENA");
		if (Map == TEXT("FROZEN")) Map = TEXT("FROZEN RING");
		TxtT(Ly + 6, NP + FVector2D(94.f, 32.f) * t, Map, 13.5f, FMath::Lerp(HA::GoldText, FLinearColor::White, Pulse * 0.5f), 0.5f);
	}

	// 4) bottom tray: deck slab, card tray recess, discard portal + energy orb (portrait play: overlaid at the bottom of the
	//    arena only while the human's Awaken / Standby is open)
	if (!L.bPortrait || PortraitTrayShown())
	{
		const float TT = L.Tray.Min.Y * s, HL = L.Hand.Min.X * s;
		const ZoogiFramed::FTrayInfo& TI = ZoogiFramed::TrayInfo();
		const bool bCards = TI.bCards && G->CardsOn();
		const float LX1 = L.TrayLeftX1 * s, RX0 = L.TrayRightX0 * s;
		const FVector2D TL = L.TrayL * s, TR = L.TrayR * s; // tray art anchors (landscape: (X0 | RX0, TT + 12 t) as before)
		if (!L.bPortrait)
		{
			HA::Nine(Out, Ly + 2, Geo, FVector2D(LX1 - 2.f * t, TT), FVector2D(RX0 - LX1 + 6.f * t, V.Y - TT + 24.f * t), TEXT("tray_mid"), FMargin(22.f, 22.f, 32.f, 22.f), t);
			HA::ImgT(Out, Ly + 3, Geo, TL, t, TEXT("tray_left"));
			HA::ImgT(Out, Ly + 3, Geo, TR, t, TEXT("tray_right"));
		}
		else
		{
			// portrait: stone row behind the deck / discard + orb slabs, the recess across the full width above it, the deck
			// slab cropped out of tray_left (its right part is the landscape recess edge)
			const FVector2D R0 = L.BottomRow.Min * s, RS = FVector2D(V.X, V.Y - R0.Y);
			FillRect(Out, Ly + 1, Geo, FVector2D(0.f, TT), FVector2D(V.X, V.Y - TT), FLinearColor(0.035f, 0.035f, 0.04f, 0.96f));
			HA::Tiled(Out, Ly + 1, Geo, R0, RS, TEXT("stone_tile"), 340.f, t, FLinearColor(0.42f, 0.42f, 0.44f, 1.f));
			HA::Nine(Out, Ly + 2, Geo, FVector2D(0.f, TT), FVector2D(V.X, R0.Y - TT + 8.f * t), TEXT("tray_mid"), FMargin(22.f, 22.f, 32.f, 22.f), t);
			const FVector2D LS = HA::SizeT(TEXT("tray_left"));
			HA::Part(Out, Ly + 3, Geo, TL, FVector2D(200.f, LS.Y) * t, TEXT("tray_left"), FBox2D(FVector2D::ZeroVector, FVector2D(200.f, LS.Y)));
			HA::ImgT(Out, Ly + 3, Geo, TR, t, TEXT("tray_right"));
		}
		TxtT(Ly + 8, TL + FVector2D(71.5f, 139.f) * t, bCards ? FString::FromInt(TI.Draw) : FString(TEXT("-")), 19.f, FLinearColor::White, 0.5f);
		// discard portal: breathing cyan light + count
		const float Br = 0.5f + 0.5f * FMath::Sin(Time * 2.f);
		ZoogiUIStyle::SBox(Out, bOverlay ? Ly + 6 : Ly + 4, Geo, TR + FVector2D(28.f, 20.f) * t, FVector2D(90.f, 110.f) * t, FLinearColor(0.4f, 0.95f, 1.f, 0.06f + 0.08f * Br), FLinearColor::Transparent, 30.f * t, 0.f);
		TxtT(Ly + 6, TR + FVector2D(155.f, 184.f) * t, bCards ? FString::Printf(TEXT("DISCARD %d"), TI.Discard) : FString(TEXT("DISCARD")), 8.f, FLinearColor::White, 0.5f);
		// energy orb (Oct 14, Evan: no spin): static swirl art; a subtle glow breathes behind it only while energy is ready
		const FVector2D Sh = bCards ? FVector2D(FMath::Sin(Time * 60.f), FMath::Cos(Time * 47.f)) * 4.f * t * TI.Shake : FVector2D::ZeroVector;
		if (bCards && TI.Energy > 0)
		{
			const float Gl = 0.5f + 0.5f * FMath::Sin(Time * 2.4f);
			ZoogiUIStyle::SBox(Out, bOverlay ? Ly + 5 : Ly + 3, Geo, TR + FVector2D(262.f, 60.f) * t, FVector2D(108.f, 108.f) * t, FLinearColor(0.75f, 0.45f, 1.f, 0.10f + 0.12f * Gl), FLinearColor::Transparent, 54.f * t, 0.f);
		}
		HA::ImgRot(Out, bOverlay ? Ly + 6 : Ly + 4, Geo, TR + FVector2D(252.f, 50.f) * t, FVector2D(128.f, 128.f) * t, TEXT("orb_swirl"), 0.f, FLinearColor(1.f, 1.f, 1.f, bOverlay ? 0.75f : 1.f));
		if (bCards && TI.Flash > 0.f) ZoogiUIStyle::SBox(Out, Ly + 5, Geo, TR + FVector2D(292.f, 78.f) * t, FVector2D(72.f, 72.f) * t, FLinearColor(1.f, 1.f, 1.f, 0.5f * TI.Flash), FLinearColor::Transparent, 36.f * t, 0.f);
		TxtT(Ly + 8, TR + FVector2D(328.f, 108.f) * t + Sh, bCards ? FString::Printf(TEXT("%d/%d"), TI.Energy, TI.EnergyMax) : FString(TEXT("--")), 22.f, TI.Energy >= 0 ? FLinearColor::White : FLinearColor(1.f, 0.45f, 0.4f), 0.5f);
		const float BF = bCards ? FMath::Clamp(TI.EnergyFrac, 0.f, 1.f) : 0.f;
		if (BF > 0.f) ZoogiUIStyle::SBox(Out, Ly + 5, Geo, TR + FVector2D(277.f, 168.f) * t, FVector2D(92.f * BF, 7.f) * t, FLinearColor(0.78f, 0.45f, 1.f, 1.f), FLinearColor(0.95f, 0.8f, 1.f, 0.6f), 3.5f * t, 1.f * t);
		// Oct 15: cards mode - the orb is the Awaken control: a pulsing crosshair reticle round it while it can be tapped,
		// and the turn's Awaken state above it (AWAKEN: OFF / AWAKEN: <ELEMENT>)
		if (bCards && OrbLive())
		{
			const FVector2D OC = TR + FVector2D(252.f, 50.f) * t; // the swirl orb's centre (ImgRot positions by centre)
			const float Pz = 0.5f + 0.5f * FMath::Sin(Time * 4.2f);
			const float Rr = (60.f + 5.f * Pz) * t;
			const FLinearColor RC(0.85f, 0.7f, 1.f, 0.55f + 0.4f * Pz);
			const bool bHovO = OrbBtn && OrbBtn->IsHovered();
			{
				// the ring is a circle of small dots: ZoogiUIStyle::SBox's outline pass and a rounded brush's fill (which takes the
				// element tint) both draw a filled disk over the orb art
				const int32 ND = 56; const float Dd = (bHovO ? 4.5f : 3.4f) * t;
				for (int32 d = 0; d < ND; ++d)
				{
					const float A = 2.f * PI * d / ND + Time * 0.35f;
					ZoogiUIStyle::SBox(Out, Ly + 7, Geo, OC + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr - FVector2D(Dd, Dd) * 0.5f, FVector2D(Dd, Dd), bHovO ? FLinearColor::White : RC, FLinearColor::Transparent, Dd * 0.5f, 0.f);
				}
			}
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector2D D = k == 0 ? FVector2D(1, 0) : k == 1 ? FVector2D(-1, 0) : k == 2 ? FVector2D(0, 1) : FVector2D(0, -1);
				const FVector2D Tk = D.X != 0.f ? FVector2D(16.f, 4.f) * t : FVector2D(4.f, 16.f) * t;
				const FVector2D Pc = OC + D * (Rr - 9.f * t); // crosshair ticks point inward (the CONFIRM plate sits left of the orb)
				ZoogiUIStyle::SBox(Out, Ly + 7, Geo, Pc - Tk * 0.5f, Tk, RC, FLinearColor::Transparent, 2.f * t, 0.f);
			}
			const EZoogiArtifact Pk = G->CardAwakenPick >= 0 && G->CardAwakenPick < 3 ? (EZoogiArtifact)G->CardAwakenOffers[G->CardAwakenPick] : EZoogiArtifact::None;
			const bool bAwOn = G->bStandbyAwaken && Pk != EZoogiArtifact::None;
			const FString Lab = G->CardsAwakenViewOpen() ? FString(TEXT("TAP: CARDS")) : bAwOn ? FString::Printf(TEXT("AWAKEN: %s"), *ZoogiArtifacts::Name(Pk).ToUpper()) : FString(TEXT("AWAKEN: OFF"));
			// state label in the free slab strip left of the orb, above CLEAR / CONFIRM (never over the orb art)
			FString L1 = Lab, L2;
			if (Lab.Split(TEXT(": "), &L1, &L2)) L1 += TEXT(":");
			const FLinearColor LC = bAwOn ? ZoogiArtifacts::Color(Pk) : FLinearColor(0.85f, 0.8f, 0.95f);
			TxtT(Ly + 8, TR + FVector2D(158.f, 24.f) * t, L1, 9.f, LC, 0.5f);
			if (!L2.IsEmpty()) TxtT(Ly + 8, TR + FVector2D(158.f, 42.f) * t, L2, 9.f, LC, 0.5f);
		}
		// awaken stones (artifact mode): the old central picker lives in the tray now
		PaintTrayAwaken(Geo, Out, bOverlay ? Ly + 7 : Ly + 4, t);
		// system text (stage forming / match over) in the recess
		if (!AB || G->Phase == EZoogiPhase::GameOver || G->Phase == EZoogiPhase::Forming)
		{
			const FVector2D P0 = L.Prompt.Min * s, PS = (L.Prompt.Max - L.Prompt.Min) * s;
			const float Sl = 1.f - FMath::Square(1.f - PromptAnim);
			FString P = LastPrompt;
			const FSlateFontInfo PF = FontT(11.f);
			while (P.Len() > 8 && HA::Measure(P, PF).X > PS.X - 30.f * t) P = P.LeftChop(4) + TEXT("..");
			HA::Text(Out, Ly + 6, Geo, P0 + PS * 0.5f + FVector2D(0.f, (1.f - Sl) * 8.f * t), P, PF, FLinearColor(1.f, 1.f, 1.f, Sl), 0.5f, 0.5f);
			if (EndAnim > 0.f && G->Phase == EZoogiPhase::GameOver)
			{
				const float Kk = FMath::Min(1.f, EndAnim * 3.f);
				TxtT(Ly + 7, P0 + FVector2D(PS.X * 0.5f, -34.f * t + 14.f * t * (1.f - Kk)), TEXT("MATCH OVER"), 20.f * (0.6f + 0.4f * Kk), FLinearColor(HA::GoldText.R, HA::GoldText.G, HA::GoldText.B, Kk), 0.5f);
			}
		}
		(void)HL;
	}
	return Ly + 10;
}

// ------------------------------------------------------------------ hud-v3: Awaken stones in the tray (artifact mode)
bool UZoogiFramedHUDWidget::TrayAwakenLayout(FTrayAwaken& Out) const
{
	Out = FTrayAwaken();
	const AZoogiGameMode* G = GM();
	const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
	const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	if (!bActive || !B || !B->Artifact || G->Phase != EZoogiPhase::Aiming || G->Stage != EZoogiStage::Awaken || G->bAwakenDone) return false;
	if (L.bPortrait && !PortraitTrayShown()) return false; // portrait play: the tray overlay is the human's only
	if (G->CardsOn())
	{
		// Choose-your-Awaken (Oct 15): 3 big stone tablets centred in the card recess under a CHOOSE YOUR AWAKEN header,
		// SKIP on the right; the cards are lowered out of the recess while this shows
		if (!G->CardsAwakenViewOpen()) return false;
		Out.bCards = true; Out.bHuman = true;
		for (int32 i = 0; i < 3; ++i) Out.Stones.Add({ i, G->CardAwakenOffers[i] });
		Out.Pick = G->CardAwakenPick;
		const float s = LocalScale, t = L.T * s;
		const FVector2D H0 = L.Hand.Min * s, H1 = L.Hand.Max * s; // recess bottom (landscape: Viewport.Y - Safe.Bottom)
		const float HeadH = 30.f * t, Gap = 22.f * t;
		const float BH = FMath::Max(ZoogiHudArt::TapPx(48.f, 48.f * L.U * s), 36.f * t), BW = FMath::Max(BH * 2.2f, 104.f * t);
		const float BX = H1.X - BW - 12.f * t;
		float TH = FMath::Min(H1.Y - H0.Y - HeadH - 14.f * t, 190.f * t), TW = TH * 360.f / 506.f;
		const float RowRoom = BX - 16.f * t - (H0.X + 12.f * t);
		if (3.f * TW + 2.f * Gap > RowRoom) { TW = (RowRoom - 2.f * Gap) / 3.f; TH = TW * 506.f / 360.f; }
		const float RowW = 3.f * TW + 2.f * Gap, CX = (H0.X + 12.f * t + BX - 16.f * t) * 0.5f;
		const float SY = H0.Y + HeadH + FMath::Max(0.f, (H1.Y - H0.Y - HeadH - 14.f * t - TH) * 0.5f);
		for (int32 i = 0; i < 3; ++i) { const float X = CX - RowW * 0.5f + i * (TW + Gap); Out.StoneRects.Add(FBox2D(FVector2D(X, SY), FVector2D(X + TW, SY + TH))); }
		const float BY = SY + TH * 0.5f - BH * 0.5f;
		for (int32 i = 0; i < 3; ++i) Out.BtnRects.Add(FBox2D(FVector2D(BX, BY), FVector2D(BX + BW, BY + BH))); // only [2] (SKIP) is placed
		Out.Header = FVector2D(CX, H0.Y + HeadH * 0.5f + 2.f * t);
		return true;
	}
	const UZoogiArtifactComponent* AC = B->Artifact;
	if (AC->Drawn != EZoogiArtifact::None) Out.Stones.Add({ 0, (uint8)AC->Drawn });
	for (int32 i = 0; i < AC->Hand.Num(); ++i) Out.Stones.Add({ i + 1, (uint8)AC->Hand[i] });
	Out.Pick = G->AwakenPick;
	if (!Out.Stones.ContainsByPredicate([&](const TPair<int32, uint8>& C) { return C.Key == Out.Pick; }) && Out.Stones.Num()) Out.Pick = Out.Stones[0].Key;
	Out.bHuman = !B->bCPU;
	const float s = LocalScale, t = L.T * s;
	const FVector2D H0 = L.Hand.Min * s, H1 = L.Hand.Max * s; // recess bottom (landscape: Viewport.Y - Safe.Bottom)
	// one gap everywhere (stone to stone, stones to text, text to buttons, buttons to the recess edge); the button stack
	// is centred on the stone row. When the recess is tight (1920) the stones shrink a little so the INFUSE / GUARD text fits.
	const float Gap = 10.f * t, BG = 8.f * t;
	const float THMax = FMath::Min(H1.Y - H0.Y - 10.f * t, 170.f * t);
	float TH = THMax, TW = TH * 360.f / 506.f;
	const float BH = FMath::Max(ZoogiHudArt::TapPx(48.f, 48.f * L.U * s), 34.f * t), BW = BH * 95.f / 24.f * 0.62f;
	const float BX = H1.X - BW - Gap;
	const float SX0 = H0.X + 14.f * t, Room = (BX - Gap) - SX0, InfoMin = 170.f * t;
	const int32 NS = FMath::Max(1, Out.Stones.Num());
	if (Room - NS * (TW + Gap) < InfoMin)
	{
		const float TW2 = FMath::Max((Room - InfoMin) / NS - Gap, FMath::Max(TW * 0.62f, 56.f * t)); // stays a big tap target
		if (TW2 < TW) { TW = TW2; TH = TW * 506.f / 360.f; }
	}
	const float SY = H0.Y + 4.f * t + FMath::RoundToFloat((THMax - TH) * 0.5f);
	float X = SX0;
	for (int32 i = 0; i < Out.Stones.Num(); ++i) { Out.StoneRects.Add(FBox2D(FVector2D(X, SY), FVector2D(X + TW, SY + TH))); X += TW + Gap; }
	const float Stack = 3.f * BH + 2.f * BG;
	const float BY = FMath::Max(H0.Y + 4.f * t, H0.Y + 4.f * t + (THMax - Stack) * 0.5f);
	for (int32 i = 0; i < 3; ++i) Out.BtnRects.Add(FBox2D(FVector2D(BX, BY + i * (BH + BG)), FVector2D(BX + BW, BY + i * (BH + BG) + BH)));
	Out.InfoX0 = X; Out.InfoX1 = BX - Gap; Out.InfoY = H0.Y + 8.f * t;
	Out.bInfo = Out.InfoX1 - Out.InfoX0 >= 120.f * t;
	return true;
}

void UZoogiFramedHUDWidget::TickTrayAwaken()
{
	FTrayAwaken TA;
	const bool bOn = TrayAwakenLayout(TA) && TA.bHuman && !bPaused;
	for (int32 i = 0; i < StoneBtns.Num(); ++i) PlaceW(StoneBtns[i], bOn && TA.StoneRects.IsValidIndex(i) ? TA.StoneRects[i].Min : FVector2D::ZeroVector, bOn && TA.StoneRects.IsValidIndex(i) ? TA.StoneRects[i].GetSize() : FVector2D::ZeroVector, bOn && TA.StoneRects.IsValidIndex(i));
	for (int32 i = 0; i < ActBtns.Num(); ++i) { const bool bB = bOn && (!TA.bCards || i == 2); PlaceW(ActBtns[i], bB ? TA.BtnRects[i].Min : FVector2D::ZeroVector, bB ? TA.BtnRects[i].GetSize() : FVector2D::ZeroVector, bB); }
	bTrayCards = TA.bCards;
	StoneKeys.Reset();
	for (const auto& St : TA.Stones) StoneKeys.Add(St.Key);
	TrayPick = TA.Pick;
	if (ActBtns.Num() == 3) { ActBtns[0]->SetIsEnabled(TA.Stones.Num() > 0); ActBtns[1]->SetIsEnabled(TA.Stones.Num() > 0); }
	// background hit panel around the stones + buttons, and the registration the old Awaken panel's hit-test path / UiTest use
	FBox2D Bounds(ForceInit);
	if (bOn) { for (const FBox2D& R : TA.StoneRects) Bounds += R; for (const FBox2D& R : TA.BtnRects) Bounds += R; }
	const float t = ZoogiFramed::Current().T * LocalScale;
	if (bOn && TA.bCards)
	{
		// cards: the whole recess swallows presses while the tablets show (no press reaches the arena / the hidden cards)
		const ZoogiFramed::FLayout& FL = ZoogiFramed::Current();
		Bounds = FBox2D(FL.Hand.Min * LocalScale + FVector2D(14.f, 6.f) * t, FVector2D(FL.Hand.Max.X * LocalScale - 10.f * t, FL.Hand.Max.Y * LocalScale - 6.f * t));
	}
	// cards: energy orb = the Awaken control while the human's Standby is open
	const bool bOrb = OrbLive();
	{
		const ZoogiFramed::FLayout& FL = ZoogiFramed::Current();
		const FVector2D TR = FL.TrayR * LocalScale; // tray_right art origin (landscape: (TrayRightX0, TrayTop + 12 T))
		// the swirl orb (centre TR+(252,50)) and the top of the energy bubble; stops above CLEAR (x < 248, y > 88)
		PlaceW(OrbBtn, bOrb ? TR + FVector2D(192.f, -12.f) * t : FVector2D::ZeroVector, bOrb ? FVector2D(188.f, 100.f) * t : FVector2D::ZeroVector, bOrb);
	}
	{
		// cards: CONFIRM / CLEAR hit areas over the hand widget's plates (hand widget px -> this widget's px)
		if (!CardHand.IsValid()) CardHand = UZoogiCardHandWidget::Find(GetWorld());
		FBox2D Rc(ForceInit), Rl(ForceInit); bool bC = false, bL = false;
		UZoogiCardHandWidget* HW = CardHand.Get();
		const float Vx = HW ? HW->GetViewSize().X : 0.f;
		const float r = Vx > 0.f ? ZoogiFramed::Current().Viewport.X * LocalScale / Vx : 0.f;
		if (HW && !bPaused && r > 0.f) HW->FramedBtnRects(Rc, bC, Rl, bL);
		bC = bC && Rc.bIsValid; bL = bL && Rl.bIsValid;
		PlaceW(CardConfirmHit, bC ? Rc.Min * r : FVector2D::ZeroVector, bC ? Rc.GetSize() * r : FVector2D::ZeroVector, bC);
		PlaceW(CardClearHit, bL ? Rl.Min * r : FVector2D::ZeroVector, bL ? Rl.GetSize() * r : FVector2D::ZeroVector, bL);
	}
	if (TrayHit) PlaceW(TrayHit, bOn && Bounds.bIsValid ? Bounds.Min - FVector2D(14.f, 6.f) * t : FVector2D::ZeroVector, bOn && Bounds.bIsValid ? Bounds.GetSize() + FVector2D(24.f, 12.f) * t : FVector2D::ZeroVector, bOn && Bounds.bIsValid);
	APlayerController* PC = GetOwningPlayer();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	if (UZoogiAwakenWidget* AW = Hud ? Hud->GetAwakenWidget() : nullptr)
	{
		if (!bOn && bOrb) { TArray<TPair<FName, int32>> Keys; TArray<UWidget*> Ws; Keys.Add({ TEXT("CardAwakenView"), 0 }); Ws.Add(OrbBtn); AW->SetTrayControls(Keys, Ws, OrbBtn); }
		else if (!bOn) AW->ClearTrayControls();
		else if (TA.bCards)
		{
			TArray<TPair<FName, int32>> Keys; TArray<UWidget*> Ws;
			for (int32 i = 0; i < 3 && i < StoneBtns.Num(); ++i) { Keys.Add({ TEXT("CardAwakenPick"), i }); Ws.Add(StoneBtns[i]); }
			Keys.Add({ TEXT("CardAwakenSkip"), 0 }); Ws.Add(ActBtns[2]);
			Keys.Add({ TEXT("CardAwakenView"), 0 }); Ws.Add(OrbBtn);
			AW->SetTrayControls(Keys, Ws, TrayHit);
		}
		else
		{
			TArray<TPair<FName, int32>> Keys; TArray<UWidget*> Ws;
			for (int32 i = 0; i < TA.Stones.Num() && i < StoneBtns.Num(); ++i) { Keys.Add({ TEXT("AwakenPick"), TA.Stones[i].Key }); Ws.Add(StoneBtns[i]); }
			Keys.Add({ TEXT("Infuse"), TrayPick }); Ws.Add(ActBtns[0]);
			Keys.Add({ TEXT("Guard"), TrayPick }); Ws.Add(ActBtns[1]);
			Keys.Add({ TEXT("Save"), 0 }); Ws.Add(ActBtns[2]);
			AW->SetTrayControls(Keys, Ws, TrayHit);
		}
	}
}

FEventReply UZoogiFramedHUDWidget::OnTrayHitDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	// a press between the stones / buttons is swallowed (never reaches the game), like the old panel background
	APlayerController* PC = GetOwningPlayer();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	if (UZoogiAwakenWidget* AW = Hud ? Hud->GetAwakenWidget() : nullptr) AW->NotePress();
	return FEventReply(true);
}

void UZoogiFramedHUDWidget::FireAwaken(FName Action, int32 Index)
{
	APlayerController* PC = GetOwningPlayer();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	if (UZoogiAwakenWidget* AW = Hud ? Hud->GetAwakenWidget() : nullptr) AW->Fire(Action, Index); // same path as the old panel (logs, click swallow)
	else if (AZoogiGameMode* G = GM()) G->MenuAction(Action, Index);
	StonePop = 0.f;
}
void UZoogiFramedHUDWidget::OnTrayBtnPressed()
{
	// like the old panel's buttons: a press on a tray control is a consumed UI press (never a world click)
	APlayerController* PC = GetOwningPlayer();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	if (UZoogiAwakenWidget* AW = Hud ? Hud->GetAwakenWidget() : nullptr) AW->NotePress();
}
void UZoogiFramedHUDWidget::OnStone0() { if (StoneKeys.IsValidIndex(0)) FireAwaken(bTrayCards ? TEXT("CardAwakenPick") : TEXT("AwakenPick"), StoneKeys[0]); }
void UZoogiFramedHUDWidget::OnStone1() { if (StoneKeys.IsValidIndex(1)) FireAwaken(bTrayCards ? TEXT("CardAwakenPick") : TEXT("AwakenPick"), StoneKeys[1]); }
void UZoogiFramedHUDWidget::OnStone2() { if (StoneKeys.IsValidIndex(2)) FireAwaken(bTrayCards ? TEXT("CardAwakenPick") : TEXT("AwakenPick"), StoneKeys[2]); }
void UZoogiFramedHUDWidget::OnStone3() { if (StoneKeys.IsValidIndex(3)) FireAwaken(TEXT("AwakenPick"), StoneKeys[3]); }
void UZoogiFramedHUDWidget::OnInfuse() { FireAwaken(TEXT("Infuse"), TrayPick); }
void UZoogiFramedHUDWidget::OnGuard() { FireAwaken(TEXT("Guard"), TrayPick); }
void UZoogiFramedHUDWidget::OnSave() { FireAwaken(bTrayCards ? TEXT("CardAwakenSkip") : TEXT("Save"), 0); }
void UZoogiFramedHUDWidget::OnOrb() { FireAwaken(TEXT("CardAwakenView"), 0); }
void UZoogiFramedHUDWidget::OnCardConfirm() { UE_LOG(LogZoogi, Log, TEXT("CARDUI CONFIRM pressed (framed hit)")); if (UZoogiCardHandWidget* HW = CardHand.Get()) HW->PressConfirm(); }
void UZoogiFramedHUDWidget::OnCardClear() { UE_LOG(LogZoogi, Log, TEXT("CARDUI CLEAR pressed (framed hit)")); if (UZoogiCardHandWidget* HW = CardHand.Get()) HW->PressClear(); }
bool UZoogiFramedHUDWidget::OrbLive() const
{
	const AZoogiGameMode* G = GM();
	return bActive && !bPaused && G && G->CardsOn() && G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && G->IsHumanAiming();
}

const TCHAR* UZoogiFramedHUDWidget::TabletFor(uint8 Artifact)
{
	switch ((EZoogiArtifact)Artifact)
	{
	case EZoogiArtifact::Ember: return TEXT("tablet_fire");
	case EZoogiArtifact::Storm: return TEXT("tablet_storm");
	case EZoogiArtifact::Frost: return TEXT("tablet_frost");
	case EZoogiArtifact::Titan: return TEXT("tablet_earth");
	case EZoogiArtifact::Void:  return TEXT("tablet_void");
	default:                    return TEXT("tablet_arcane");
	}
}
void UZoogiFramedHUDWidget::PaintTrayAwaken(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Ly, float t) const
{
	namespace HA = ZoogiHudArt;
	FTrayAwaken TA;
	if (!TrayAwakenLayout(TA)) return;
	auto FontT = [&](float Sz) { return ZoogiUIStyle::BoldFont(FMath::Max(9, FMath::RoundToInt(Sz * t)), FMath::Max(1.f, 1.6f * t)); };
	const float Pop = FMath::Clamp(StonePop, 0.f, 1.f);
	if (TA.bCards)
	{
		// Choose-your-Awaken: header, 3 tablets (key 1-3 medallion, name, affinity tag), SKIP
		const AZoogiGameMode* G = GM();
		const AZoogiBall* AB = G ? G->GetActiveBall() : nullptr;
		const EZoogiArtifact Aff = AB ? ZoogiArtifacts::Affinity(AB->Def.Id) : EZoogiArtifact::None;
		const float Hp = 0.5f + 0.5f * FMath::Sin(Time * 3.f);
		HA::Text(Out, Ly + 3, Geo, TA.Header, TEXT("CHOOSE YOUR AWAKEN"), FontT(15.f), FMath::Lerp(HA::GoldText, FLinearColor::White, 0.35f * Hp), 0.5f, 0.5f);
		for (int32 i = 0; i < TA.Stones.Num() && i < TA.StoneRects.Num(); ++i)
		{
			const FBox2D R = TA.StoneRects[i];
			const EZoogiArtifact A = (EZoogiArtifact)TA.Stones[i].Value;
			const FZoogiArtifactRow& Row = ZoogiArtifacts::Get(A);
			const bool bSel = TA.Stones[i].Key == TA.Pick;
			const bool bHov = StoneBtns.IsValidIndex(i) && StoneBtns[i]->IsHovered();
			const float Sc = (bHov ? 1.05f : 1.f) * (0.85f + 0.15f * ZoogiUIStyle::EaseOutBack(FMath::Min(1.f, Pop + i * 0.12f)));
			const FVector2D Cn = R.GetCenter() - FVector2D(0.f, bHov ? 4.f * t : 0.f), Sz = R.GetSize() * Sc;
			ZoogiUIStyle::SBox(Out, Ly, Geo, Cn - Sz * 0.5f - FVector2D(7.f, 7.f) * t, Sz + FVector2D(14.f, 14.f) * t, FLinearColor(Row.RimColor.R, Row.RimColor.G, Row.RimColor.B, (bSel ? 0.45f : 0.18f) + 0.12f * FMath::Sin(Time * 4.f + i)), FLinearColor::Transparent, 14.f * t, 0.f);
			HA::Img(Out, Ly + 1, Geo, Cn - Sz * 0.5f, Sz, TabletFor(TA.Stones[i].Value));
			HA::Text(Out, Ly + 3, Geo, Cn + FVector2D(0.f, Sz.Y * 0.33f), ZoogiArtifacts::Name(A).ToUpper(), FontT(10.f), FLinearColor::White, 0.5f, 0.5f);
			if (A == Aff) HA::Text(Out, Ly + 3, Geo, Cn + FVector2D(0.f, Sz.Y * 0.43f), TEXT("AFFINITY"), FontT(7.5f), FLinearColor(1.f, 0.9f, 0.4f), 0.5f, 0.5f);
			const FVector2D KC = Cn - Sz * 0.5f + FVector2D(Sz.X * 0.13f, Sz.Y * 0.08f);
			const float KR = 15.f * t * Sc;
			HA::Img(Out, Ly + 2, Geo, KC - FVector2D(KR, KR), FVector2D(KR, KR) * 2.f, TEXT("mark_cyan"), bSel || bHov ? FLinearColor::White : FLinearColor(0.7f, 0.75f, 0.85f, 0.9f));
			HA::Text(Out, Ly + 3, Geo, KC, FString::FromInt(i + 1), FontT(10.f), FLinearColor::White, 0.5f, 0.5f);
			if (bSel) HA::Img(Out, Ly + 4, Geo, Cn - Sz * 0.6f, Sz * 1.2f, TEXT("prompt_cyanbox"), FLinearColor(1.f, 1.f, 1.f, 0.8f));
		}
		if (TA.BtnRects.IsValidIndex(2))
		{
			const FBox2D R = TA.BtnRects[2];
			const bool bHov = ActBtns.IsValidIndex(2) && ActBtns[2]->IsHovered(), bPr = ActBtns.IsValidIndex(2) && ActBtns[2]->IsPressed();
			const FVector2D Sz = R.GetSize() * (bPr ? 0.94f : bHov ? 1.04f : 1.f), P = R.GetCenter() - Sz * 0.5f;
			if (bHov && !bPr) HA::Img(Out, Ly + 1, Geo, P - Sz * FVector2D(0.3f, 0.25f), Sz * FVector2D(1.6f, 1.5f), TEXT("pill_on"), FLinearColor(1.f, 1.f, 1.f, 0.8f));
			HA::Img(Out, Ly + 2, Geo, P, Sz, TEXT("pill_off"));
			HA::Text(Out, Ly + 3, Geo, R.GetCenter(), TEXT("SKIP"), FontT(11.f), FLinearColor::White, 0.5f, 0.5f);
			HA::Text(Out, Ly + 3, Geo, FVector2D(R.GetCenter().X, R.Max.Y + 12.f * t), TEXT("no Awaken"), FontT(7.5f), FLinearColor(0.75f, 0.8f, 0.9f), 0.5f, 0.5f);
		}
		return;
	}
	for (int32 i = 0; i < TA.Stones.Num(); ++i)
	{
		const FBox2D R = TA.StoneRects[i];
		const bool bSel = TA.Stones[i].Key == TA.Pick;
		const FZoogiArtifactRow& Row = ZoogiArtifacts::Get((EZoogiArtifact)TA.Stones[i].Value);
		const bool bHov = StoneBtns.IsValidIndex(i) && StoneBtns[i]->IsHovered();
		const float Sc = (bSel ? 1.04f + 0.03f * FMath::Sin(Time * 4.f) : 1.f) * (bHov ? 1.05f : 1.f) * (0.8f + 0.2f * ZoogiUIStyle::EaseOutBack(FMath::Min(1.f, Pop + i * 0.1f)));
		const FVector2D C = R.GetCenter() - FVector2D(0.f, bSel ? 6.f * t : 0.f), S = R.GetSize() * Sc;
		if (bSel) ZoogiUIStyle::SBox(Out, Ly, Geo, C - S * 0.5f - FVector2D(6.f, 6.f) * t, S + FVector2D(12.f, 12.f) * t, FLinearColor(Row.RimColor.R, Row.RimColor.G, Row.RimColor.B, 0.35f + 0.2f * FMath::Sin(Time * 5.f)), FLinearColor::Transparent, 14.f * t, 0.f);
		HA::Img(Out, Ly + 1, Geo, C - S * 0.5f, S, TabletFor(TA.Stones[i].Value));
		HA::Text(Out, Ly + 3, Geo, C + FVector2D(0.f, S.Y * 0.33f), ZoogiArtifacts::Name((EZoogiArtifact)TA.Stones[i].Value).ToUpper(), FontT(8.5f), FLinearColor::White, 0.5f, 0.5f);
		{
			// key medallion: Evan's ring mark (lit on the picked stone, dimmed otherwise)
			const FVector2D KC = C - S * 0.5f + FVector2D(S.X * 0.11f, S.Y * 0.07f);
			const float KR = 14.f * t * Sc;
			HA::Img(Out, Ly + 2, Geo, KC - FVector2D(KR, KR), FVector2D(KR, KR) * 2.f, TEXT("mark_cyan"), bSel ? FLinearColor::White : FLinearColor(0.55f, 0.6f, 0.7f, 0.8f));
			HA::Text(Out, Ly + 3, Geo, KC, TA.Stones[i].Key == 0 ? TEXT("N") : FString::FromInt(TA.Stones[i].Key), FontT(9.f), FLinearColor::White, 0.5f, 0.5f);
		}
		if (bSel) HA::Img(Out, Ly + 4, Geo, C - S * 0.62f, S * 1.24f, TEXT("prompt_cyanbox"), FLinearColor(1.f, 1.f, 1.f, 0.8f));
	}
	// picked stone's texts in the free middle of the recess
	const int32 PI_ = TA.Stones.IndexOfByPredicate([&](const TPair<int32, uint8>& C) { return C.Key == TA.Pick; });
	if (PI_ != INDEX_NONE && TA.bInfo)
	{
		const FZoogiArtifactRow& Row = ZoogiArtifacts::Get((EZoogiArtifact)TA.Stones[PI_].Value);
		const float W = TA.InfoX1 - TA.InfoX0;
		HA::Img(Out, Ly + 1, Geo, FVector2D(TA.InfoX0 + 4.f * t, TA.InfoY), FVector2D(20.f, 28.f) * t, TabletFor(TA.Stones[PI_].Value)); // element badge = Evan's element tablet (4:06 PM)
		HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0 + 34.f * t, TA.InfoY + 14.f * t), FString::Printf(TEXT("AWAKEN  %s%s"), *ZoogiArtifacts::Name((EZoogiArtifact)TA.Stones[PI_].Value).ToUpper(), TA.bHuman ? TEXT("") : TEXT("  (CPU deciding)")), FontT(10.f), HA::GoldText, 0.f, 0.5f);
		auto Para = [&](const FString& Head, const FString& Body, float Y, const FLinearColor& HC) -> float
		{
			HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0, Y), Head, FontT(8.5f), HC);
			TArray<FString> Words; Body.ParseIntoArrayWS(Words);
			FString Line; float LY = Y + 13.f * t; const FSlateFontInfo F = FontT(8.f);
			for (const FString& Wd : Words)
			{
				const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
				if (!Line.IsEmpty() && HA::Measure(Try, F).X > W) { HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0, LY), Line, F, FLinearColor(0.88f, 0.9f, 0.95f)); LY += 12.f * t; Line = Wd; if (LY > TA.InfoY + 160.f * t) return LY; }
				else Line = Try;
			}
			HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0, LY), Line, F, FLinearColor(0.88f, 0.9f, 0.95f));
			return LY + 12.f * t;
		};
		const float GY = Para(TEXT("INFUSE"), Row.InfuseText, TA.InfoY + 36.f * t, FLinearColor(1.f, 0.7f, 0.35f));
		Para(TEXT("GUARD"), Row.GuardText, FMath::Max(TA.InfoY + 96.f * t, GY + 8.f * t), FLinearColor(0.55f, 0.85f, 1.f));
	}
	if (!TA.bHuman) return;
	const TCHAR* Lbl[3] = { TEXT("INFUSE"), TEXT("GUARD"), TEXT("SAVE") };
	for (int32 i = 0; i < 3; ++i)
	{
		const FBox2D R = TA.BtnRects[i];
		const bool bHov = ActBtns.IsValidIndex(i) && ActBtns[i]->IsHovered(), bPr = ActBtns.IsValidIndex(i) && ActBtns[i]->IsPressed();
		const float Sq = bPr ? 0.94f : (bHov ? 1.04f : 1.f);
		const FVector2D S = R.GetSize() * Sq, P = R.GetCenter() - S * 0.5f;
		if (bHov && !bPr) HA::Img(Out, Ly + 1, Geo, P - S * FVector2D(0.3f, 0.25f), S * FVector2D(1.6f, 1.5f), TEXT("pill_on"), FLinearColor(1.f, 1.f, 1.f, 0.8f));
		const bool bOn = !ActBtns.IsValidIndex(i) || ActBtns[i]->GetIsEnabled();
		HA::Img(Out, Ly + 2, Geo, P, S, i == 0 ? TEXT("pill_done") : TEXT("pill_off"), bOn ? FLinearColor::White : FLinearColor(0.55f, 0.6f, 0.7f, 0.7f));
		HA::Text(Out, Ly + 3, Geo, R.GetCenter(), Lbl[i], FontT(10.f), bOn ? FLinearColor::White : FLinearColor(0.7f, 0.74f, 0.82f, 0.8f), 0.5f, 0.5f);
	}
}

// ------------------------------------------------------------------ Oct 17 (Evan 12:10 PM): HUD toggle = orientation switch
// HUD shown = landscape framed hud-v3; HUD hidden = portrait play: the arena fills the tall screen, the camera frames it
// vertically (Game Camera: SetPortraitCameraMode + OnBattleViewModeChanged) and only the HUD toggle, the turn / score indicator and ZOOM
// stay on the top edge (+ the tray overlay during the human's Awaken / Standby). Each toggle shows the rotate-phone prompt.
void UZoogiFramedHUDWidget::OnHudToggle()
{
	if (RotateT >= 0.f || !bActive) return;
	bRotateTo = !ZoogiFramed::IsHudHidden();
	RotateT = 0.f;
	bRotateApplied = false;
	RotateReason = 0;
	UE_LOG(LogZoogi, Log, TEXT("FRAMED HUD toggle pressed -> rotate prompt (to %s)"), bRotateTo ? TEXT("PORTRAIT, HUD hidden") : TEXT("LANDSCAPE, HUD shown"));
}

void UZoogiFramedHUDWidget::StartBattleEdgePrompt(bool bEntering, const FVector2D& V)
{
	// Oct 17 (Evan): the rotate-phone prompt also shows when entering / leaving battle, whenever the view has to turn.
	// Entering: the battle keeps the HUD state (shown = landscape, hidden = portrait; the very first match follows the device),
	// so the prompt runs only when the current view doesn't match - the orientation switch happens at mid-prompt as for the toggle.
	// Leaving: menus are portrait. Mobile asks for portrait under the prompt; PC keeps its window (the menus own it) and shows
	// nothing unless -ZoogiRotatePromptTest.
	if (V.X <= 0.f || V.Y <= 0.f) return;
	const bool bPortraitNow = V.Y > V.X;
	if (bEntering)
	{
		const bool bWantPortrait = ZoogiFramed::IsHudHidden();
		if (bWantPortrait == bPortraitNow || RotateT >= 0.f)
		{
#if PLATFORM_IOS || PLATFORM_ANDROID
			// already facing the right way: just pin the battle's orientation (the config allows both)
			FPlatformMisc::SetAllowedDeviceOrientation(bWantPortrait ? EDeviceScreenOrientation::Portrait : EDeviceScreenOrientation::LandscapeSensor);
#endif
			return;
		}
		bRotateTo = bWantPortrait;
		RotateT = 0.f;
		bRotateApplied = false;
		RotateReason = 1;
		UE_LOG(LogZoogi, Log, TEXT("FRAMED entering battle -> rotate prompt (to %s)"), bWantPortrait ? TEXT("PORTRAIT, HUD hidden") : TEXT("LANDSCAPE, HUD shown"));
		return;
	}
	if (bPortraitNow) return;
#if !(PLATFORM_IOS || PLATFORM_ANDROID)
	static const bool bForce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRotatePromptTest"));
	if (!bForce) return;
#endif
	bRotateTo = true;
	RotateT = 0.f;
	bRotateApplied = true; // nothing to apply in the HUD: the menus take over
	RotateReason = 2;
#if PLATFORM_IOS || PLATFORM_ANDROID
	FPlatformMisc::SetAllowedDeviceOrientation(EDeviceScreenOrientation::Portrait);
#endif
	UE_LOG(LogZoogi, Log, TEXT("FRAMED leaving battle -> rotate prompt (to PORTRAIT, menus)"));
}

void UZoogiFramedHUDWidget::ApplyOrientation(bool bHidden)
{
	ZoogiFramed::SetHudHidden(bHidden);
	CallPortraitCameraMode(bHidden); // while the rotate prompt still covers the screen
#if PLATFORM_IOS || PLATFORM_ANDROID
	// Config allows both orientations (DefaultEngine.ini); the battle picks one at runtime
	FPlatformMisc::SetAllowedDeviceOrientation(bHidden ? EDeviceScreenOrientation::Portrait : EDeviceScreenOrientation::LandscapeSensor);
	UE_LOG(LogZoogi, Log, TEXT("FRAMED orientation -> %s"), bHidden ? TEXT("Portrait") : TEXT("LandscapeSensor"));
#else
	// PC: a windowed game turns its window (width <-> height), clamped to the desktop work area; fullscreen / -ZoogiNoWindowSwap
	// only switch the layout
	static const bool bNoSwap = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWindowSwap"));
	UGameViewportClient* VC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!bNoSwap && VC && VC->Viewport && GSystemResolution.WindowMode == EWindowMode::Windowed)
	{
		const FIntPoint Cur = VC->Viewport->GetSizeXY();
		if (Cur.X > 0 && Cur.Y > 0 && bHidden != (Cur.Y > Cur.X))
		{
			float W = Cur.Y, H = Cur.X;
			if (FSlateApplication::IsInitialized())
			{
				FDisplayMetrics DM; FSlateApplication::Get().GetInitialDisplayMetrics(DM);
				const float MW = DM.PrimaryDisplayWorkAreaRect.Right - DM.PrimaryDisplayWorkAreaRect.Left, MH = DM.PrimaryDisplayWorkAreaRect.Bottom - DM.PrimaryDisplayWorkAreaRect.Top - 40.f;
				const float K = (MW > 100.f && MH > 100.f) ? FMath::Min3(1.f, MW / W, MH / H) : 1.f;
				W *= K; H *= K;
			}
			FSystemResolution::RequestResolutionChange(FMath::RoundToInt(W), FMath::RoundToInt(H), EWindowMode::Windowed);
			UE_LOG(LogZoogi, Log, TEXT("FRAMED window turned %dx%d -> %dx%d (%s)"), Cur.X, Cur.Y, FMath::RoundToInt(W), FMath::RoundToInt(H), bHidden ? TEXT("portrait") : TEXT("landscape"));
		}
	}
#endif
}

void UZoogiFramedHUDWidget::CallPortraitCameraMode(bool bPortrait)
{
	// HOOK (Game Camera, branch game-camera-portrait): BlueprintCallable AZoogiPlayerController::SetPortraitCameraMode(bool).
	// Looked up by name so this compiles and runs without their branch; once it is merged this can become a direct call.
	APlayerController* PC = GetOwningPlayer();
	UFunction* Fn = PC ? PC->FindFunction(FName(TEXT("SetPortraitCameraMode"))) : nullptr;
	FBoolProperty* Arg = Fn ? CastField<FBoolProperty>(Fn->ChildProperties) : nullptr;
	if (!Fn || !Arg || Fn->NumParms != 1)
	{
		UE_LOG(LogZoogi, Log, TEXT("FRAMED SetPortraitCameraMode(%d): not available on %s (Game Camera branch not merged) - skipped"), bPortrait ? 1 : 0, PC ? *PC->GetClass()->GetName() : TEXT("no PC"));
		return;
	}
	uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max<int32>(1, (int32)Fn->ParmsSize));
	FMemory::Memzero(Parms, FMath::Max<int32>(1, (int32)Fn->ParmsSize));
	Arg->SetPropertyValue_InContainer(Parms, bPortrait);
	PC->ProcessEvent(Fn, Parms);
	UE_LOG(LogZoogi, Log, TEXT("FRAMED SetPortraitCameraMode(%d) called"), bPortrait ? 1 : 0);
}

void UZoogiFramedHUDWidget::TickViewModeEvent(float Dt)
{
	// fire OnBattleViewModeChanged once the orientation really switched: the viewport aspect matches the requested mode, or
	// after 1.5 s (fullscreen / -ZoogiNoWindowSwap / a device locked by the user) with whatever aspect it has then
	if (!bModeEventPending || !bActive) return;
	ModeEventT += Dt;
	const FVector2D V = ZoogiFramed::Current().Viewport;
	const bool bHidden = ZoogiFramed::IsHudHidden(), bPortrait = V.Y > V.X;
	if (bPortrait != bHidden && ModeEventT < 1.5f) return;
	bModeEventPending = false;
	if (!bRotateApplied && RotateT < 0.f) CallPortraitCameraMode(bHidden); // initial mode (no toggle yet): tell the camera once too
	if (ZoogiFramed::BroadcastBattleViewMode(bHidden, bPortrait)) OnBattleViewModeChanged.Broadcast(bHidden, bPortrait);
}

bool UZoogiFramedHUDWidget::IsBattleHudHidden() const { bool H, P; ZoogiFramed::GetBattleViewMode(H, P); return H; }
bool UZoogiFramedHUDWidget::IsBattlePortrait() const { bool H, P; ZoogiFramed::GetBattleViewMode(H, P); return P; }

bool UZoogiFramedHUDWidget::PortraitTrayShown() const
{
	const AZoogiGameMode* G = GM();
	return G && G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && G->IsHumanAiming();
}

void UZoogiFramedHUDWidget::PaintPortraitStrip(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Ly) const
{
	// portrait play: one small indicator plate on the top edge (turn + step on the first line, round and score / orbs on the
	// second) and the current prompt under the strip; the HUD toggle and ZOOM are real buttons beside it
	namespace HA = ZoogiHudArt;
	const AZoogiGameMode* G = GM();
	const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
	if (!G || !L.bPortrait) return;
	const float s = LocalScale, P = L.P * s;
	auto F = [&](float Px) { return ZoogiUIStyle::BoldFont(FMath::Max(8, FMath::RoundToInt(Px * P)), FMath::Max(1.f, 2.f * P)); };
	const FVector2D IP = L.Indicator.Min * s, IS = L.Indicator.GetSize() * s;
	const AZoogiBall* AB = G->GetActiveBall();
	const FLinearColor PC = AB ? ZoogiUtil::PlayerColor(AB->PlayerIndex) : HA::Cyan;
	const float TPop = 1.f + 0.06f * (1.f - TurnAnim) * FMath::Sin(TurnAnim * PI);
	ZoogiUIStyle::SBox(Out, Ly, Geo, IP, IS, FLinearColor(0.03f, 0.035f, 0.06f, 0.80f), FLinearColor(PC.R, PC.G, PC.B, 0.85f), 14.f * P, 2.f * P);
	ZoogiUIStyle::SBox(Out, Ly + 1, Geo, IP + FVector2D(14.f, IS.Y / P * 0.5f - 8.f) * P, FVector2D(16.f, 16.f) * P * TPop, PC, FLinearColor::Transparent, 8.f * P, 0.f);
	FString Who;
	if (G->Phase == EZoogiPhase::Forming) Who = TEXT("STAGE FORMING");
	else if (G->Phase == EZoogiPhase::GameOver) Who = TEXT("MATCH OVER");
	else if (AB) Who = FString::Printf(TEXT("P%d %s  %s"), AB->PlayerIndex + 1, *AB->Def.Name.ToUpper(), AB->bCPU ? TEXT("CPU") : TEXT("YOUR TURN"));
	const float TX = IP.X + 40.f * P, TW = IS.X - 52.f * P;
	FSlateFontInfo F1 = F(21.f);
	while (Who.Len() > 6 && HA::Measure(Who, F1).X > TW) Who = Who.LeftChop(3) + TEXT("..");
	HA::Text(Out, Ly + 2, Geo, FVector2D(TX, IP.Y + IS.Y * 0.32f), Who, F1, FLinearColor::White, 0.f, 0.5f);
	const FString Score = G->bComboMode ? FString::Printf(TEXT("SCORE %d - %d"), G->TeamScore(0), G->TeamScore(1)) : FString::Printf(TEXT("ORBS %d"), G->OrbsLeft());
	FString L2 = FString::Printf(TEXT("%s   R %d/%d   %s"), *ActionLabel(G), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds, *Score);
	if (AB) L2 += FString::Printf(TEXT("   PTS %d"), AB->Score);
	const FSlateFontInfo F2 = F(15.f);
	while (L2.Len() > 6 && HA::Measure(L2, F2).X > TW) L2 = L2.LeftChop(3) + TEXT("..");
	HA::Text(Out, Ly + 2, Geo, FVector2D(TX, IP.Y + IS.Y * 0.74f), L2, F2, HA::GoldText, 0.f, 0.5f);
	// prompt line (drag-aim hint etc.) under the strip, never over the tray
	FString Pr = LastPrompt;
	const FVector2D PP = L.Prompt.Min * s, PS = L.Prompt.GetSize() * s;
	const FSlateFontInfo F3 = F(16.f);
	while (Pr.Len() > 8 && HA::Measure(Pr, F3).X > PS.X) Pr = Pr.LeftChop(4) + TEXT("..");
	const float Sl = 1.f - FMath::Square(1.f - PromptAnim);
	if (!Pr.IsEmpty())
	{
		const FVector2D TS = HA::Measure(Pr, F3);
		ZoogiUIStyle::SBox(Out, Ly, Geo, FVector2D(PP.X + PS.X * 0.5f - TS.X * 0.5f - 12.f * P, PP.Y), FVector2D(TS.X + 24.f * P, PS.Y), FLinearColor(0.f, 0.f, 0.03f, 0.45f * Sl), FLinearColor::Transparent, 10.f * P, 0.f);
		HA::Text(Out, Ly + 2, Geo, PP + PS * 0.5f, Pr, F3, FLinearColor(1.f, 1.f, 1.f, Sl), 0.5f, 0.5f);
	}
	if (EndAnim > 0.f && G->Phase == EZoogiPhase::GameOver)
	{
		const float Kk = FMath::Min(1.f, EndAnim * 3.f);
		HA::Text(Out, Ly + 3, Geo, FVector2D(Geo.GetLocalSize().X * 0.5f, PP.Y + PS.Y + 40.f * P), TEXT("MATCH OVER"), F(44.f * (0.6f + 0.4f * Kk)), FLinearColor(HA::GoldText.R, HA::GoldText.G, HA::GoldText.B, Kk), 0.5f, 0.5f);
	}
}

int32 UZoogiFramedHUDWidget::PaintRotate(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer) const
{
	// rotate-phone prompt: dim, a phone outline turning 90 degrees, ROTATE TO PORTRAIT / LANDSCAPE (each way)
	if (RotateT < 0.f) return Layer;
	namespace HA = ZoogiHudArt;
	const FVector2D V = Geo.GetLocalSize();
	const float u = FMath::Min(V.X, V.Y) / 1080.f;
	const float A = FMath::Clamp(RotateT / 0.2f, 0.f, 1.f) * FMath::Clamp((2.f - RotateT) / 0.3f, 0.f, 1.f);
	FillRect(Out, Layer, Geo, FVector2D::ZeroVector, V, FLinearColor(0.f, 0.01f, 0.04f, 0.78f * A));
	// the phone turns during 0.25 .. 0.9 s (layout switches at 0.95 s), then holds in the new orientation
	const float K = ZoogiUIStyle::EaseOutBack(FMath::Clamp((RotateT - 0.25f) / 0.65f, 0.f, 1.f));
	const bool bToPortrait = bRotateTo;
	const float Ang = (bToPortrait ? (90.f - 90.f * K) : (90.f * K)); // portrait phone = 0 deg (tall), landscape = 90 deg
	const FVector2D C = V * FVector2D(0.5f, 0.44f), PS = FVector2D(170.f, 300.f) * u;
	PhoneBrush = ZoogiUIStyle::RoundBrush(FLinearColor(0.05f, 0.06f, 0.10f, A), FLinearColor(1.f, 0.82f, 0.35f, A), 6.f * u, 26.f * u);
	PhoneScreenBrush = ZoogiUIStyle::RoundBrush(FLinearColor(HA::Cyan.R * 0.5f, HA::Cyan.G * 0.5f, HA::Cyan.B * 0.6f, 0.85f * A), FLinearColor::Transparent, 0.f, 10.f * u);
	FSlateDrawElement::MakeRotatedBox(Out, Layer + 1, Geo.ToPaintGeometry(FVector2f(PS), FSlateLayoutTransform(FVector2f(C - PS * 0.5f))), &PhoneBrush, ESlateDrawEffect::None, FMath::DegreesToRadians(Ang));
	const FVector2D SS = PS - FVector2D(30.f, 70.f) * u;
	FSlateDrawElement::MakeRotatedBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2f(SS), FSlateLayoutTransform(FVector2f(C - SS * 0.5f))), &PhoneScreenBrush, ESlateDrawEffect::None, FMath::DegreesToRadians(Ang));
	// turning arrow: dots on an arc beside the phone
	for (int32 d = 0; d < 9; ++d)
	{
		const float Aa = FMath::DegreesToRadians(-60.f + d * 15.f) + (bToPortrait ? PI : 0.f);
		const float Dd = (d == 8 ? 16.f : 9.f) * u;
		ZoogiUIStyle::SBox(Out, Layer + 2, Geo, C + FVector2D(FMath::Cos(Aa), FMath::Sin(Aa)) * 230.f * u - FVector2D(Dd, Dd) * 0.5f, FVector2D(Dd, Dd), FLinearColor(1.f, 0.85f, 0.4f, A * (0.4f + 0.6f * d / 8.f)), FLinearColor::Transparent, Dd * 0.5f, 0.f);
	}
	const FSlateFontInfo F1 = ZoogiUIStyle::BoldFont(FMath::RoundToInt(44.f * u), 3.f * u), F2 = ZoogiUIStyle::BoldFont(FMath::RoundToInt(24.f * u), 2.f * u);
	HA::Text(Out, Layer + 3, Geo, FVector2D(V.X * 0.5f, C.Y + 250.f * u), bToPortrait ? TEXT("ROTATE TO PORTRAIT") : TEXT("ROTATE TO LANDSCAPE"), F1, FLinearColor(HA::GoldText.R, HA::GoldText.G, HA::GoldText.B, A), 0.5f, 0.5f);
	HA::Text(Out, Layer + 3, Geo, FVector2D(V.X * 0.5f, C.Y + 310.f * u), RotateReason == 2 ? TEXT("Menus are portrait") : RotateReason == 1 ? (bToPortrait ? TEXT("Battle - HUD hidden, play vertically") : TEXT("Battle - HUD shown, landscape")) : (bToPortrait ? TEXT("HUD hidden - play vertically") : TEXT("HUD shown - landscape")), F2, FLinearColor(1.f, 1.f, 1.f, 0.9f * A), 0.5f, 0.5f);
	return Layer + 4;
}

void UZoogiFramedHUDWidget::TickHudToggleTest(AZoogiGameMode* G, float Dt)
{
	// -ZoogiHudToggleTest (with -ZoogiShots=<dir>): landscape HUD aim -> toggle (rotate prompt shot) -> portrait aim, a shot in
	// progress and the Standby / Awaken tray overlay -> toggle back (prompt shot) -> landscape again. Real button handler.
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHudToggleTest"));
	if (!bOn || !G) return;
	ToggleTestT += Dt;
	const bool bAim = G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle && G->IsAimStep() && G->HasAim();
	const bool bHidden = ZoogiFramed::IsHudHidden();
	auto Next = [&](int32 St) { ToggleTestStep = St; ToggleTestT = 0.f; };
	switch (ToggleTestStep)
	{
	case 0: if (bAim && !bHidden && G->StepTime > 0.8f) { G->TakeShot(TEXT("hudtoggle_1_landscape_hud_aim")); OnHudToggle(); Next(1); }
		else if (bAim && bHidden && G->StepTime > 0.8f) Next(2); break; // started portrait: skip straight to the portrait shots
	case 1: if (ToggleTestT > 0.6f) { G->TakeShot(TEXT("hudtoggle_2_rotate_prompt_to_portrait")); Next(2); } break;
	case 2: if (ToggleTestT > 1.0f && RotateT < 0.f && bHidden && bAim && G->StepTime > 0.5f) { G->TakeShot(TEXT("hudtoggle_3_portrait_aim")); Next(3); } break;
	case 3: if (G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.45f) { G->TakeShot(TEXT("hudtoggle_4_portrait_shot_in_progress")); Next(4); } break;
	case 4: if (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && G->StageTime > 1.8f && (PortraitTrayShown() || ToggleTestT > 40.f))
			{ G->TakeShot(TEXT("hudtoggle_5_portrait_standby")); Next(5); }
		else if (ToggleTestT > 25.f && !G->IsHumanAiming()) Next(5); break; // all-CPU (-ZoogiAuto): no human tray to show, go on to the toggle back
	case 5: if (bAim && G->StepTime > 0.8f) { G->TakeShot(TEXT("hudtoggle_6_portrait_aim2")); OnHudToggle(); Next(6); } break;
	case 6: if (ToggleTestT > 0.6f) { G->TakeShot(TEXT("hudtoggle_7_rotate_prompt_to_landscape")); Next(7); } break;
	case 7: if (ToggleTestT > 1.6f && RotateT < 0.f && bAim && G->StepTime > 0.5f) { G->TakeShot(TEXT("hudtoggle_8_landscape_hud_back")); UE_LOG(LogZoogi, Log, TEXT("FRAMED hudtoggle test done (hidden=%d)"), ZoogiFramed::IsHudHidden() ? 1 : 0); Next(8); } break;
	default: break;
	}
}
