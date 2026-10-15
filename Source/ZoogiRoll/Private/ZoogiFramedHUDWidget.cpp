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
#include "ZoogiArtifactComponent.h"

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
	return Owner.IsValid() ? Owner->PaintBack(Geo, Out, Layer) : Layer;
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
		for (UWidget* W : { (UWidget*)ColL.Get(), (UWidget*)ColR.Get(), (UWidget*)CamBtn.Get(), (UWidget*)ZoomBtn.Get(), (UWidget*)MenuBtn.Get(), (UWidget*)PauseShade.Get(), (UWidget*)PausePanel.Get(), (UWidget*)PauseTitle.Get(),
			(UWidget*)VolDown.Get(), (UWidget*)VolUp.Get(), (UWidget*)VolText.Get(), (UWidget*)SetCamBtn.Get() }) PlaceW(W, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : PauseBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : StoneBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : ActBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		PlaceW(OrbBtn, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
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
	PlaceW(ColL, LB.Key, LB.Value, true);
	PlaceW(ColR, RB.Key, RB.Value, true);
	// header buttons: on the CAM / MENU plates of the top_right art (>= 48 dp tap targets)
	{
		const float t = L.T * s, X1 = (L.Viewport.X - L.Safe.Right) * s, Y0 = L.Safe.Top * s;
		const float BH = FMath::Max(34.f * t, ZoogiHudArt::TapPx(48.f, 48.f * u)), BY = Y0 + 26.f * t - BH * 0.5f;
		PlaceW(MenuBtn, FVector2D(X1 - 126.f * t, BY), FVector2D(FMath::Max(104.f * t, BH), BH), true);
		PlaceW(CamBtn, FVector2D(X1 - 255.f * t, BY), FVector2D(FMath::Max(117.f * t, BH), BH), true);
	}
	// Oct 14 aim ZOOM toggle: bottom-left of the arena window while the human aims (>= 48 dp, also clickable)
	{
		AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetOwningPlayer());
		const bool bShowZ = PC && !bPaused && (PC->CanShowZoom() || PC->IsZoomShownOn());
		const float t = L.T * s;
		const float ZH = FMath::Max(40.f * t, ZoogiHudArt::TapPx(48.f, 48.f * u)), ZW = FMath::Max(132.f * t, ZH * 2.4f);
		const FVector2D ZP(L.Arena.Min.X * s + 14.f * t, L.Arena.Max.Y * s - ZH - 14.f * t);
		PlaceW(ZoomBtn, ZP, FVector2D(ZW, ZH), bShowZ);
		// single source: the controller's mode (button, wheel, = -, Z, pinch all land there); reset when the mode ends
		const bool bOnNow = PC && PC->IsZoomShownOn();
		if (ZoomText && (bOnNow != bZoomShownOn || !bZoomLabelInit)) { ZoomText->SetText(FText::FromString(bOnNow ? TEXT("ZOOM: ON") : TEXT("ZOOM"))); bZoomLabelInit = true; }
		bZoomShownOn = bOnNow;
		if (ZoomBtn && bShowZ) ZoomBtn->SetRenderOpacity(bOnNow ? 1.f : 0.85f);
		ZoogiFramed::SetZoomButtonRect(s > 0.f ? FBox2D(ZP / s, (ZP + FVector2D(ZW, ZH)) / s) : FBox2D(ForceInit), bShowZ);
	}
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

	// 1) dark stone in the four margins only (never over the arena rect)
	const FLinearColor Bg(0.035f, 0.035f, 0.04f, 1.f), StoneTint(0.42f, 0.42f, 0.44f, 1.f);
	const FVector2D Bands[4][2] = { { FVector2D(0.f, 0.f), FVector2D(V.X, A0.Y) }, { FVector2D(0.f, A1.Y), FVector2D(V.X, V.Y - A1.Y) },
		{ FVector2D(0.f, A0.Y), FVector2D(A0.X, AS.Y) }, { FVector2D(A1.X, A0.Y), FVector2D(V.X - A1.X, AS.Y) } };
	for (const auto& Bd : Bands) { FillRect(Out, Ly, Geo, Bd[0], Bd[1], Bg); HA::Tiled(Out, Ly + 1, Geo, Bd[0], Bd[1], TEXT("stone_tile"), 340.f, t, StoneTint); }

	// 2) arena frame: steel bands + gold / crystal corner brackets with a cyan glow (light pulse on major events)
	FLinearColor PulseC; const float Pulse = ZoogiFramed::PulseAmount(PulseC);
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

	// hud-v3 overlay (Evan 4:25 PM): his target image as ONE full-frame overlay; arena / pills / plate are holes,
	// live values are drawn on top (layer 6+), the sliced pieces underneath only show through the holes.
	const bool bOverlay = HA::Tex(TEXT("overlay")) != nullptr;
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

	// 4) bottom tray: deck slab, card tray recess, discard portal + energy orb
	{
		const float TT = L.Tray.Min.Y * s, HL = L.Hand.Min.X * s;
		const ZoogiFramed::FTrayInfo& TI = ZoogiFramed::TrayInfo();
		const bool bCards = TI.bCards && G->CardsOn();
		const float LX1 = L.TrayLeftX1 * s, RX0 = L.TrayRightX0 * s;
		HA::Nine(Out, Ly + 2, Geo, FVector2D(LX1 - 2.f * t, TT), FVector2D(RX0 - LX1 + 6.f * t, V.Y - TT + 24.f * t), TEXT("tray_mid"), FMargin(22.f, 22.f, 32.f, 22.f), t);
		HA::ImgT(Out, Ly + 3, Geo, FVector2D(X0, TT + 12.f * t), t, TEXT("tray_left"));
		HA::ImgT(Out, Ly + 3, Geo, FVector2D(RX0, TT + 12.f * t), t, TEXT("tray_right"));
		const FVector2D TL(X0, TT + 12.f * t), TR(RX0, TT + 12.f * t);
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
			const FVector2D OC = TR + FVector2D(316.f, 114.f) * t;
			const float Pz = 0.5f + 0.5f * FMath::Sin(Time * 4.2f);
			const float Rr = (68.f + 6.f * Pz) * t;
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
			TxtT(Ly + 8, TR + FVector2D(316.f, 16.f) * t, Lab, 9.f, bAwOn ? ZoogiArtifacts::Color(Pk) : FLinearColor(0.85f, 0.8f, 0.95f), 0.5f);
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
	if (G->CardsOn())
	{
		// Choose-your-Awaken (Oct 15): 3 big stone tablets centred in the card recess under a CHOOSE YOUR AWAKEN header,
		// SKIP on the right; the cards are lowered out of the recess while this shows
		if (!G->CardsAwakenViewOpen()) return false;
		Out.bCards = true; Out.bHuman = true;
		for (int32 i = 0; i < 3; ++i) Out.Stones.Add({ i, G->CardAwakenOffers[i] });
		Out.Pick = G->CardAwakenPick;
		const float s = LocalScale, t = L.T * s;
		const FVector2D H0 = L.Hand.Min * s, H1 = FVector2D(L.Hand.Max.X * s, (L.Viewport.Y - L.Safe.Bottom) * s);
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
	const FVector2D H0 = L.Hand.Min * s, H1 = FVector2D(L.Hand.Max.X * s, (L.Viewport.Y - L.Safe.Bottom) * s);
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
		Bounds = FBox2D(FL.Hand.Min * LocalScale + FVector2D(14.f, 6.f) * t, FVector2D(FL.Hand.Max.X * LocalScale - 10.f * t, (FL.Viewport.Y - FL.Safe.Bottom) * LocalScale - 6.f * t));
	}
	// cards: energy orb = the Awaken control while the human's Standby is open
	const bool bOrb = OrbLive();
	{
		const ZoogiFramed::FLayout& FL = ZoogiFramed::Current();
		const FVector2D TR(FL.TrayRightX0 * LocalScale, FL.Tray.Min.Y * LocalScale + 12.f * t);
		PlaceW(OrbBtn, bOrb ? TR + FVector2D(256.f, 54.f) * t : FVector2D::ZeroVector, bOrb ? FVector2D(120.f, 120.f) * t : FVector2D::ZeroVector, bOrb);
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
