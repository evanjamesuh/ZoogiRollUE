// framed: see ZoogiFramedHUDWidget.h
#include "ZoogiFramedHUDWidget.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiTeamColumnWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiHUD.h"
#include "ZoogiAwakenWidget.h"
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

UButton* UZoogiFramedHUDWidget::MakeButton(const FString& Label, const FLinearColor& Accent, bool bFilled, UTextBlock*& OutText, int32 FontSize)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
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
			MenuText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.08f, 0.22f)));
			MenuText->SetShadowColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.25f)); MenuText->SetShadowOffset(FVector2D(0.f, 1.f));
			MenuText->SetFont(ZoogiUIStyle::BoldFont(16, 0.f));
		}
		CamBtn->OnClicked.AddDynamic(this, &UZoogiFramedHUDWidget::OnCam);
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
			for (int32 i = 0; i < 7; ++i)
			{
				UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
				B->SetStyle(St);
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
	const ESlateVisibility Want = bShow ? (W->IsA<UButton>() || W == PauseShade ? ESlateVisibility::Visible : (W->IsA<UZoogiTeamColumnWidget>() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::HitTestInvisible)) : ESlateVisibility::Collapsed;
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
		for (UWidget* W : { (UWidget*)ColL.Get(), (UWidget*)ColR.Get(), (UWidget*)CamBtn.Get(), (UWidget*)MenuBtn.Get(), (UWidget*)PauseShade.Get(), (UWidget*)PausePanel.Get(), (UWidget*)PauseTitle.Get(),
			(UWidget*)VolDown.Get(), (UWidget*)VolUp.Get(), (UWidget*)VolText.Get(), (UWidget*)SetCamBtn.Get() }) PlaceW(W, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : PauseBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : StoneBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		for (UButton* B : ActBtns) PlaceW(B, FVector2D::ZeroVector, FVector2D::ZeroVector, false);
		LastStage = LastActive = LastPhase = -1;
		return;
	}
	const float u = L.U * s;
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
		PlaceW(MenuBtn, FVector2D(X1 - 282.f * t + 156.f * t, BY), FVector2D(107.f * t, BH), true);
		PlaceW(CamBtn, FVector2D(X1 - 282.f * t + 26.f * t, BY), FVector2D(120.f * t, BH), true);
	}
	CamText->SetText(FText::FromString(G->bCinematicCam ? TEXT("CAM: CINE") : TEXT("CAM: TACT")));
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
		return G->CardsOn() ? TEXT("Drag cards onto a target (or press 1-5), then CONFIRM. CLEAR resets") : TEXT("Pick a card, then INFUSE / GUARD / SAVE");
	}
	if (B->bCPU) return G->TargetLock.IsValid() ? FString::Printf(TEXT("CPU is targeting %s"), *G->TargetName(G->TargetLock.Get())) : TEXT("CPU is choosing a target");
	if (G->IsPowerUpStep()) return TEXT("POWER UP - launching!");
	if (G->HasAim()) return FString::Printf(TEXT("Aiming at %s. A/D cut, Up/Down force, SPACE or CONFIRM to launch"), G->bFreeAim ? TEXT("FLOOR POINT") : *G->TargetName(G->TargetLock.Get()).ToUpper());
	return TEXT("Click a rival or orb in the arena to lock on, or click the floor to aim at a point");
}

FString UZoogiFramedHUDWidget::HintText(const AZoogiGameMode* G) const
{
	if (G && G->bComboMode) return TEXT("Awaken: card + I/G/S   Battle: Q switch, click target, A/D cut, 1-3 force, SPACE   Follow-Up F/X   C camera   Esc menu");
	if (G && G->CardsOn()) return TEXT("Standby: cards 1-5 + Enter, Backspace clears   Aim: click target / floor, A/D cut, Up/Down force, SPACE   RMB cancel   C camera");
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
			SBox(Out, Ly + 3, Geo, Cc - FVector2D(30.f, 30.f) * t, FVector2D(60.f, 60.f) * t, GC, FLinearColor::Transparent, 30.f * t, 0.f);
			HA::Img(Out, Ly + 4, Geo, CP[i], CS, CN[i]);
		}
	}

	// 3) top bar: title plate (round / orbs), phase pills + active player / map plate, CAM / MENU plate
	const AZoogiBall* AB = G->GetActiveBall();
	{
		HA::ImgT(Out, Ly + 2, Geo, FVector2D(X0, Y0), t, TEXT("top_title"));
		TxtT(Ly + 6, FVector2D(X0 + 182.5f * t, Y0 + 34.5f * t), FString::Printf(TEXT("%d / %d"), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds), 11.f, FLinearColor::White, 0.5f);
		if (G->bComboMode)
		{
			SBox(Out, Ly + 5, Geo, FVector2D(X0 + 220.f * t, Y0 + 9.f * t), FVector2D(64.f * t, 15.f * t), FLinearColor(0.11f, 0.10f, 0.09f, 1.f), FLinearColor::Transparent, 3.f * t, 0.f);
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
			}
			TxtT(Ly + 6, PP + PS * 0.5f, Lb, 8.5f, bNow ? FLinearColor::White : (bDone ? FLinearColor(0.9f, 1.f, 0.92f) : FLinearColor(0.86f, 0.86f, 0.84f)), 0.5f);
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
		if (AB) SBox(Out, Ly + 5, Geo, FVector2D(WC.X - WS.X * 0.5f - 11.f * t, WC.Y - 3.f * t), FVector2D(6.f * t, 6.f * t), ZoogiUtil::PlayerColor(AB->PlayerIndex), FLinearColor::Transparent, 3.f * t, 0.f);
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
		TxtT(Ly + 6, TL + FVector2D(71.5f, 139.f) * t, bCards ? FString::FromInt(TI.Draw) : FString(TEXT("-")), 15.f, FLinearColor::White, 0.5f);
		// discard portal: breathing cyan light + count
		const float Br = 0.5f + 0.5f * FMath::Sin(Time * 2.f);
		SBox(Out, Ly + 4, Geo, TR + FVector2D(28.f, 20.f) * t, FVector2D(90.f, 110.f) * t, FLinearColor(0.4f, 0.95f, 1.f, 0.06f + 0.08f * Br), FLinearColor::Transparent, 30.f * t, 0.f);
		TxtT(Ly + 6, TR + FVector2D(155.f, 184.f) * t, bCards ? FString::Printf(TEXT("DISCARD %d"), TI.Discard) : FString(TEXT("DISCARD")), 8.f, FLinearColor::White, 0.5f);
		// energy orb: the swirl turns, number + bar live
		const FVector2D Sh = bCards ? FVector2D(FMath::Sin(Time * 60.f), FMath::Cos(Time * 47.f)) * 4.f * t * TI.Shake : FVector2D::ZeroVector;
		HA::ImgRot(Out, Ly + 4, Geo, TR + FVector2D(252.f, 50.f) * t, FVector2D(128.f, 128.f) * t, TEXT("orb_swirl"), Time * 0.7f, FLinearColor(1.f, 1.f, 1.f, 1.f));
		if (bCards && TI.Flash > 0.f) SBox(Out, Ly + 5, Geo, TR + FVector2D(292.f, 78.f) * t, FVector2D(72.f, 72.f) * t, FLinearColor(1.f, 1.f, 1.f, 0.5f * TI.Flash), FLinearColor::Transparent, 36.f * t, 0.f);
		TxtT(Ly + 6, TR + FVector2D(328.f, 108.f) * t + Sh, bCards ? FString::Printf(TEXT("%d/%d"), TI.Energy, TI.EnergyMax) : FString(TEXT("--")), 16.f, TI.Energy >= 0 ? FLinearColor::White : FLinearColor(1.f, 0.45f, 0.4f), 0.5f);
		const float BF = bCards ? FMath::Clamp(TI.EnergyFrac, 0.f, 1.f) : 0.f;
		if (BF > 0.f) SBox(Out, Ly + 5, Geo, TR + FVector2D(277.f, 168.f) * t, FVector2D(92.f * BF, 7.f) * t, FLinearColor(0.78f, 0.45f, 1.f, 1.f), FLinearColor(0.95f, 0.8f, 1.f, 0.6f), 3.5f * t, 1.f * t);
		// awaken stones (artifact mode): the old central picker lives in the tray now
		PaintTrayAwaken(Geo, Out, Ly + 4, t);
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
	if (!bActive || !B || !B->Artifact || G->CardsOn() || G->Phase != EZoogiPhase::Aiming || G->Stage != EZoogiStage::Awaken || G->bAwakenDone) return false;
	const UZoogiArtifactComponent* AC = B->Artifact;
	if (AC->Drawn != EZoogiArtifact::None) Out.Stones.Add({ 0, (uint8)AC->Drawn });
	for (int32 i = 0; i < AC->Hand.Num(); ++i) Out.Stones.Add({ i + 1, (uint8)AC->Hand[i] });
	Out.Pick = G->AwakenPick;
	if (!Out.Stones.ContainsByPredicate([&](const TPair<int32, uint8>& C) { return C.Key == Out.Pick; }) && Out.Stones.Num()) Out.Pick = Out.Stones[0].Key;
	Out.bHuman = !B->bCPU;
	const float s = LocalScale, t = L.T * s;
	const FVector2D H0 = L.Hand.Min * s, H1 = FVector2D(L.Hand.Max.X * s, (L.Viewport.Y - L.Safe.Bottom) * s);
	const float TH = FMath::Min(H1.Y - H0.Y - 10.f * t, 170.f * t), TW = TH * 360.f / 506.f;
	float X = H0.X + 14.f * t;
	for (int32 i = 0; i < Out.Stones.Num(); ++i) { Out.StoneRects.Add(FBox2D(FVector2D(X, H0.Y + 4.f * t), FVector2D(X + TW, H0.Y + 4.f * t + TH))); X += TW + 10.f * t; }
	const float BH = FMath::Max(ZoogiHudArt::TapPx(48.f, 48.f * L.U * s), 34.f * t), BW = BH * 95.f / 24.f * 0.62f;
	const float BX = H1.X - BW - 10.f * t, BY = H0.Y + 6.f * t;
	for (int32 i = 0; i < 3; ++i) Out.BtnRects.Add(FBox2D(FVector2D(BX, BY + i * (BH + 6.f * t)), FVector2D(BX + BW, BY + i * (BH + 6.f * t) + BH)));
	Out.InfoX0 = X; Out.InfoX1 = BX - 10.f * t; Out.InfoY = H0.Y + 8.f * t;
	return true;
}

void UZoogiFramedHUDWidget::TickTrayAwaken()
{
	FTrayAwaken TA;
	const bool bOn = TrayAwakenLayout(TA) && TA.bHuman && !bPaused;
	for (int32 i = 0; i < StoneBtns.Num(); ++i) PlaceW(StoneBtns[i], bOn && TA.StoneRects.IsValidIndex(i) ? TA.StoneRects[i].Min : FVector2D::ZeroVector, bOn && TA.StoneRects.IsValidIndex(i) ? TA.StoneRects[i].GetSize() : FVector2D::ZeroVector, bOn && TA.StoneRects.IsValidIndex(i));
	for (int32 i = 0; i < ActBtns.Num(); ++i) PlaceW(ActBtns[i], bOn ? TA.BtnRects[i].Min : FVector2D::ZeroVector, bOn ? TA.BtnRects[i].GetSize() : FVector2D::ZeroVector, bOn);
	StoneKeys.Reset();
	for (const auto& St : TA.Stones) StoneKeys.Add(St.Key);
	TrayPick = TA.Pick;
	if (ActBtns.Num() == 3) { ActBtns[0]->SetIsEnabled(TA.Stones.Num() > 0); ActBtns[1]->SetIsEnabled(TA.Stones.Num() > 0); }
}

void UZoogiFramedHUDWidget::FireAwaken(FName Action, int32 Index)
{
	APlayerController* PC = GetOwningPlayer();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	if (UZoogiAwakenWidget* AW = Hud ? Hud->GetAwakenWidget() : nullptr) AW->Fire(Action, Index); // same path as the old panel (logs, click swallow)
	else if (AZoogiGameMode* G = GM()) G->MenuAction(Action, Index);
	StonePop = 0.f;
}
void UZoogiFramedHUDWidget::OnStone0() { if (StoneKeys.IsValidIndex(0)) FireAwaken(TEXT("AwakenPick"), StoneKeys[0]); }
void UZoogiFramedHUDWidget::OnStone1() { if (StoneKeys.IsValidIndex(1)) FireAwaken(TEXT("AwakenPick"), StoneKeys[1]); }
void UZoogiFramedHUDWidget::OnStone2() { if (StoneKeys.IsValidIndex(2)) FireAwaken(TEXT("AwakenPick"), StoneKeys[2]); }
void UZoogiFramedHUDWidget::OnStone3() { if (StoneKeys.IsValidIndex(3)) FireAwaken(TEXT("AwakenPick"), StoneKeys[3]); }
void UZoogiFramedHUDWidget::OnInfuse() { FireAwaken(TEXT("Infuse"), TrayPick); }
void UZoogiFramedHUDWidget::OnGuard() { FireAwaken(TEXT("Guard"), TrayPick); }
void UZoogiFramedHUDWidget::OnSave() { FireAwaken(TEXT("Save"), 0); }

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
		HA::Text(Out, Ly + 3, Geo, C - S * 0.5f + FVector2D(S.X * 0.11f, S.Y * 0.07f), TA.Stones[i].Key == 0 ? TEXT("N") : FString::FromInt(TA.Stones[i].Key), FontT(9.f), FLinearColor::White, 0.5f, 0.5f);
		if (bSel) HA::Img(Out, Ly + 4, Geo, C - S * 0.62f, S * 1.24f, TEXT("prompt_cyanbox"), FLinearColor(1.f, 1.f, 1.f, 0.8f));
	}
	// picked stone's texts in the free middle of the recess
	const int32 PI_ = TA.Stones.IndexOfByPredicate([&](const TPair<int32, uint8>& C) { return C.Key == TA.Pick; });
	if (PI_ != INDEX_NONE && TA.InfoX1 - TA.InfoX0 > 160.f * t)
	{
		const FZoogiArtifactRow& Row = ZoogiArtifacts::Get((EZoogiArtifact)TA.Stones[PI_].Value);
		const float W = TA.InfoX1 - TA.InfoX0;
		HA::Img(Out, Ly + 1, Geo, FVector2D(TA.InfoX0 + 4.f * t, TA.InfoY), FVector2D(20.f, 28.f) * t, TabletFor(TA.Stones[PI_].Value)); // element badge = Evan's element tablet (4:06 PM)
		HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0 + 34.f * t, TA.InfoY + 14.f * t), FString::Printf(TEXT("AWAKEN  %s%s"), *ZoogiArtifacts::Name((EZoogiArtifact)TA.Stones[PI_].Value).ToUpper(), TA.bHuman ? TEXT("") : TEXT("  (CPU deciding)")), FontT(10.f), HA::GoldText, 0.f, 0.5f);
		auto Para = [&](const FString& Head, const FString& Body, float Y, const FLinearColor& HC)
		{
			HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0, Y), Head, FontT(8.5f), HC);
			TArray<FString> Words; Body.ParseIntoArrayWS(Words);
			FString Line; float LY = Y + 13.f * t; const FSlateFontInfo F = FontT(8.f);
			for (const FString& Wd : Words)
			{
				const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
				if (!Line.IsEmpty() && HA::Measure(Try, F).X > W) { HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0, LY), Line, F, FLinearColor(0.88f, 0.9f, 0.95f)); LY += 12.f * t; Line = Wd; if (LY > TA.InfoY + 160.f * t) return; }
				else Line = Try;
			}
			HA::Text(Out, Ly + 3, Geo, FVector2D(TA.InfoX0, LY), Line, F, FLinearColor(0.88f, 0.9f, 0.95f));
		};
		Para(TEXT("INFUSE"), Row.InfuseText, TA.InfoY + 36.f * t, FLinearColor(1.f, 0.7f, 0.35f));
		Para(TEXT("GUARD"), Row.GuardText, TA.InfoY + 96.f * t, FLinearColor(0.55f, 0.85f, 1.f));
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
		HA::Img(Out, Ly + 2, Geo, P, S, i == 0 ? TEXT("pill_done") : TEXT("pill_off"));
		HA::Text(Out, Ly + 3, Geo, R.GetCenter(), Lbl[i], FontT(9.5f), FLinearColor::White, 0.5f, 0.5f);
	}
}
