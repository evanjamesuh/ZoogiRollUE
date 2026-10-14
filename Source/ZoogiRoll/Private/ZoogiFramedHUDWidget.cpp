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

namespace
{
	FVector2D Measure(const FString& T, const FSlateFontInfo& F)
	{
		if (!FSlateApplication::IsInitialized()) return FVector2D(T.Len() * F.Size * 0.6f, F.Size * 1.2f);
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, F);
	}
	// Outline-only rounded ring (fill transparent): frames the arena without covering it.
	void FrameRing(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, float R, float W, const FLinearColor& C)
	{
		if (C.A <= 0.f || W <= 0.f) return;
		const FSlateRoundedBoxBrush B(FLinearColor::Transparent, R, C, W);
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2f(S), FSlateLayoutTransform(FVector2f(P))), &B, ESlateDrawEffect::None, FLinearColor::White);
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
		MenuBtn = MakeButton(TEXT("II  MENU"), ZoogiUIStyle::Gold, true, Tx);
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
	const float Avail = (L.Viewport.Y - L.Safe.Bottom - L.Prompt.Max.Y) * s + 6.f;
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
		LastStage = LastActive = LastPhase = -1;
		return;
	}
	const float u = L.U * s;
	auto Box = [&](const FBox2D& B) { return TPair<FVector2D, FVector2D>(B.Min * s, (B.Max - B.Min) * s); };
	const auto LB = Box(L.Left), RB = Box(L.Right);
	ColL->UScale = ColR->UScale = u * (L.bCompact ? 0.86f : 1.f);
	ColL->bCompact = ColR->bCompact = L.bCompact;
	PlaceW(ColL, LB.Key, LB.Value, true);
	PlaceW(ColR, RB.Key, RB.Value, true);
	// header buttons (right end of the top bar)
	const float BH = (L.bCompact ? 40.f : 46.f) * u, BY = L.Top.Min.Y * s + ((L.Top.Max.Y - L.Top.Min.Y) * s - BH) * 0.5f;
	const float MW = 150.f * u, CW = 170.f * u, BX1 = L.Top.Max.X * s - 14.f * u;
	PlaceW(MenuBtn, FVector2D(BX1 - MW, BY), FVector2D(MW, BH), true);
	PlaceW(CamBtn, FVector2D(BX1 - MW - 10.f * u - CW, BY), FVector2D(CW, BH), true);
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
	const FString P = PromptText(G);
	if (P != LastPrompt) { LastPrompt = P; PromptAnim = 0.f; }
}

FString UZoogiFramedHUDWidget::PromptText(const AZoogiGameMode* G) const
{
	if (!G) return FString();
	if (G->Phase == EZoogiPhase::Forming) return TEXT("The arena is forming...  click or press SPACE to skip");
	if (G->Phase == EZoogiPhase::GameOver) return FString::Printf(TEXT("MATCH OVER  -  %s   REMATCH or MENU (results in the arena)"), *G->Banner);
	if (UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) if (Anime->bPlaying) return TEXT("Episode playing...  click / SPACE to skip");
	const AZoogiBall* B = G->GetActiveBall();
	if (!B) return FString();
	if (G->Phase == EZoogiPhase::Rolling) return TEXT("Rolling...");
	if (G->Phase == EZoogiPhase::Resolving) return TEXT("Resolving...");
	if (G->Stage == EZoogiStage::Draw) return TEXT("Drawing...");
	if (G->Stage == EZoogiStage::Awaken)
	{
		if (G->bAwakenDone) return G->AwakenChoiceText.IsEmpty() ? TEXT("Awaken done") : G->AwakenChoiceText;
		if (B->bCPU) return TEXT("CPU is awakening...");
		return G->CardsOn() ? TEXT("STANDBY: drag cards onto a target (or click 1-5), then CONFIRM  -  CLEAR resets") : TEXT("AWAKEN: pick a card, then INFUSE / GUARD / SAVE");
	}
	if (B->bCPU) return G->TargetLock.IsValid() ? FString::Printf(TEXT("CPU is targeting %s"), *G->TargetName(G->TargetLock.Get())) : TEXT("CPU is choosing a target");
	if (G->IsPowerUpStep()) return TEXT("POWER UP - launching!");
	if (G->HasAim()) return FString::Printf(TEXT("AIMING AT %s  -  A/D cut, Up/Down force, SPACE or CONFIRM to launch"), G->bFreeAim ? TEXT("FLOOR POINT") : *G->TargetName(G->TargetLock.Get()).ToUpper());
	return TEXT("Click a rival or orb in the arena to lock on - or click the floor to aim at a point");
}

FString UZoogiFramedHUDWidget::HintText(const AZoogiGameMode* G) const
{
	if (G && G->bComboMode) return TEXT("Awaken: card + I/G/S   Battle: Q switch, click target, A/D cut, 1-3 force, SPACE   Follow-Up F/X   C camera   Esc menu");
	if (G && G->CardsOn()) return TEXT("Standby: cards 1-5 + Enter, Backspace clears   Aim: click target / floor, A/D cut, Up/Down force, SPACE   RMB cancel   C camera");
	return TEXT("Awaken: card + I/G/S   Aim: click target / floor, A/D cut, Up/Down power, SPACE   RMB cancel   C camera   Ctrl+R restart");
}

int32 UZoogiFramedHUDWidget::PaintBack(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer) const
{
	using namespace ZoogiUIStyle;
	const AZoogiGameMode* G = GM();
	const ZoogiFramed::FLayout& L = ZoogiFramed::Current();
	if (!bActive || !G || !L.bValid) return Layer;
	const float s = LocalScale, u = L.U * s;
	const FVector2D V = Geo.GetLocalSize();
	const FVector2D A0 = L.Arena.Min * s, A1 = L.Arena.Max * s, AS = A1 - A0;
	int32 Ly = Layer;
	auto Font = [&](float Sz) { return BoldFont(FMath::Max(7, FMath::RoundToInt(Sz * u)), FMath::Max(1.f, 2.f * u)); };
	auto Txt = [&](float X, float Y, const FString& T, float Sz, const FLinearColor& C, float AlignX = 0.f)
	{
		const FSlateFontInfo F = Font(Sz);
		if (AlignX != 0.f) X -= Measure(T, F).X * AlignX;
		ZoogiUIStyle::SText(Out, Ly + 6, Geo, FVector2D(X, Y), T, F, C);
	};

	// 1) deep-blue backdrop in the four margins only (never over the arena rect)
	const FLinearColor Bg = DeepBg, Bg2(0.02f, 0.07f, 0.24f, 1.f);
	FillRect(Out, Ly, Geo, FVector2D(0.f, 0.f), FVector2D(V.X, A0.Y), Bg);
	FillRect(Out, Ly, Geo, FVector2D(0.f, A1.Y), FVector2D(V.X, V.Y - A1.Y), Bg);
	FillRect(Out, Ly, Geo, FVector2D(0.f, A0.Y), FVector2D(A0.X, AS.Y), Bg);
	FillRect(Out, Ly, Geo, FVector2D(A1.X, A0.Y), FVector2D(V.X - A1.X, AS.Y), Bg);
	// soft tech dot grid in the margins
	const float Step = 36.f * u;
	for (float y = Step * 0.5f; y < V.Y; y += Step)
		for (float x = Step * 0.5f; x < V.X; x += Step)
			if (x < A0.X - 20.f * u || x > A1.X + 20.f * u || y < A0.Y - 20.f * u || y > A1.Y + 20.f * u)
				FillRect(Out, Ly + 1, Geo, FVector2D(x, y), FVector2D(2.f * u, 2.f * u), FLinearColor(Bg2.R * 3.f, Bg2.G * 3.f, Bg2.B * 2.f, 0.35f));

	// 2) arena frame: shadow, layered bevel rings, corner brackets, light pulse, map title plate
	FLinearColor PulseC; const float Pulse = ZoogiFramed::PulseAmount(PulseC);
	FrameRing(Out, Ly + 2, Geo, A0 - FVector2D(14.f, 10.f) * u + FVector2D(4.f, 8.f) * u, AS + FVector2D(28.f, 20.f) * u, 18.f * u, 10.f * u, FLinearColor(0.f, 0.f, 0.03f, 0.45f));
	const FLinearColor GlowC = FMath::Lerp(FLinearColor(0.2f, 0.85f, 1.f, 0.35f), FLinearColor(PulseC.R, PulseC.G, PulseC.B, 0.95f), Pulse);
	FrameRing(Out, Ly + 3, Geo, A0 - FVector2D(12.f, 12.f) * u * (1.f + 0.4f * Pulse), AS + FVector2D(24.f, 24.f) * u * (1.f + 0.4f * Pulse), 18.f * u, (6.f + 6.f * Pulse) * u, GlowC);
	FrameRing(Out, Ly + 4, Geo, A0 - FVector2D(8.f, 8.f) * u, AS + FVector2D(16.f, 16.f) * u, 14.f * u, 7.f * u, Outline);
	FrameRing(Out, Ly + 5, Geo, A0 - FVector2D(3.f, 3.f) * u, AS + FVector2D(6.f, 6.f) * u, 8.f * u, 3.f * u, FLinearColor(0.25f, 0.8f, 1.f, 1.f));
	FrameRing(Out, Ly + 5, Geo, A0 - FVector2D(1.f, 1.f) * u, AS + FVector2D(2.f, 2.f) * u, 4.f * u, 1.5f * u, Outline);
	// tiny backdrop corner caps so no square scene corner peeks outside the rounded rings
	const float CL = 34.f * u, CT = 6.f * u;
	const FVector2D Cor[4] = { A0 - FVector2D(10.f, 10.f) * u, FVector2D(A1.X + 10.f * u - CL, A0.Y - 10.f * u), FVector2D(A0.X - 10.f * u, A1.Y + 10.f * u - CT), FVector2D(A1.X + 10.f * u - CL, A1.Y + 10.f * u - CT) };
	for (int32 i = 0; i < 4; ++i)
	{
		const bool bR = i == 1 || i == 3, bB = i >= 2;
		ZoogiUIStyle::SBox(Out, Ly + 6, Geo, Cor[i], FVector2D(CL, CT), Gold, Outline, 3.f * u, 1.5f * u);
		ZoogiUIStyle::SBox(Out, Ly + 6, Geo, FVector2D(bR ? A1.X + 10.f * u - CT : A0.X - 10.f * u, bB ? A1.Y + 10.f * u - CL : A0.Y - 10.f * u), FVector2D(CT, CL), Gold, Outline, 3.f * u, 1.5f * u);
	}
	{
		const UEnum* E = StaticEnum<EZoogiArena>();
		FString Map = E ? E->GetDisplayNameTextByValue((int64)G->MenuArena).ToString().ToUpper() : TEXT("ARENA");
		if (Map == TEXT("FROZEN")) Map = TEXT("FROZEN RING");
		const FString Sub = G->bCinematicCam ? TEXT("LIVE  -  CINEMATIC CAM") : TEXT("LIVE  -  TACTICAL CAM");
		const FSlateFontInfo F = Font(20.f), F2 = Font(11.f);
		const float TW = FMath::Max(Measure(Map, F).X, Measure(Sub, F2).X) + 44.f * u, TH = 30.f * u;
		const FVector2D TP(A0.X + AS.X * 0.5f - TW * 0.5f, A0.Y - TH * 0.72f);
		ZoogiUIStyle::SBox(Out, Ly + 7, Geo, TP + FVector2D(2.f, 4.f) * u, FVector2D(TW, TH), FLinearColor(0.f, 0.f, 0.05f, 0.4f), FLinearColor::Transparent, 12.f * u, 0.f);
		ZoogiUIStyle::SBox(Out, Ly + 7, Geo, TP, FVector2D(TW, TH), FMath::Lerp(FLinearColor(0.05f, 0.2f, 0.62f, 1.f), PulseC, Pulse * 0.5f), Outline, 12.f * u, 3.f * u);
		ZoogiUIStyle::SText(Out, Ly + 8, Geo, FVector2D(TP.X + (TW - Measure(Map, F).X) * 0.5f, TP.Y + 1.f * u), Map, F, Gold);
	}

	// 3) top header: title, round, orbs, phase stepper, active player + status
	{
		const FVector2D T0 = L.Top.Min * s, TS = (L.Top.Max - L.Top.Min) * s;
		ZoogiUIStyle::SPanel(Out, Ly + 2, Geo, T0, TS, FLinearColor(0.03f, 0.11f, 0.40f, 0.95f), FLinearColor(0.25f, 0.85f, 1.f, 1.f), 16.f * u);
		float X = T0.X + 18.f * u;
		const float CY = T0.Y + TS.Y * 0.5f;
		Txt(X, CY - 16.f * u, TEXT("ZOOGI ROLL"), 22.f, Gold);
		X += Measure(TEXT("ZOOGI ROLL"), Font(22.f)).X + 20.f * u;
		auto Pill = [&](const FString& Label, const FString& Val, const FLinearColor& C)
		{
			const FSlateFontInfo F1 = Font(11.f), F2 = Font(18.f);
			const float W = FMath::Max(Measure(Label, F1).X, Measure(Val, F2).X) + 26.f * u, H = TS.Y - 18.f * u;
			ZoogiUIStyle::SBox(Out, Ly + 4, Geo, FVector2D(X, T0.Y + 9.f * u), FVector2D(W, H), FLinearColor(0.02f, 0.06f, 0.24f, 0.95f), Outline, 10.f * u, 2.5f * u);
			ZoogiUIStyle::SText(Out, Ly + 6, Geo, FVector2D(X + 13.f * u, T0.Y + 11.f * u), Label, F1, MutedText);
			ZoogiUIStyle::SText(Out, Ly + 6, Geo, FVector2D(X + 13.f * u, T0.Y + 11.f * u + H * 0.38f), Val, F2, C);
			X += W + 10.f * u;
		};
		Pill(TEXT("ROUND"), FString::Printf(TEXT("%d / %d"), FMath::Min(G->RoundNumber, G->MenuRounds), G->MenuRounds), FLinearColor::White);
		if (G->bComboMode) Pill(TEXT("SCORE"), FString::Printf(TEXT("A %d - %d B"), G->TeamScore(0), G->TeamScore(1)), Gold);
		else Pill(TEXT("ORBS LEFT"), FString::FromInt(G->OrbsLeft()), FLinearColor(0.55f, 0.85f, 1.f));
		const float LeftEnd = X;
		// stepper, centred on the arena
		const int32 N = 5;
		const float Avail = (L.Top.Max.X * s - 14.f * u - 340.f * u) - LeftEnd;
		const float ChipW = FMath::Clamp((Avail - 20.f * u) / N - 14.f * u, 70.f * u, 132.f * u), ChipH = 30.f * u, Gap = 14.f * u;
		const float SW = N * ChipW + (N - 1) * Gap;
		float SX = FMath::Clamp(A0.X + AS.X * 0.5f - SW * 0.5f, LeftEnd + 10.f * u, LeftEnd + FMath::Max(10.f * u, Avail - SW));
		const float SY = T0.Y + 7.f * u;
		const int32 Cur = (G->Phase == EZoogiPhase::Forming) ? -1 : (int32)G->Stage;
		const AZoogiBall* AB = G->GetActiveBall();
		const FLinearColor PC = AB ? ZoogiUtil::PlayerColor(AB->PlayerIndex) : Friendly;
		FillRect(Out, Ly + 3, Geo, FVector2D(SX + ChipW * 0.5f, SY + ChipH * 0.5f - 3.f * u), FVector2D(SW - ChipW, 6.f * u), Outline);
		if (Cur > 0) FillRect(Out, Ly + 3, Geo, FVector2D(SX + ChipW * 0.5f, SY + ChipH * 0.5f - 1.5f * u), FVector2D(Cur * (ChipW + Gap), 3.f * u), Mint);
		for (int32 i = 0; i < N; ++i)
		{
			const float CX = SX + i * (ChipW + Gap);
			const bool bNow = i == Cur, bDone = Cur >= 0 && i < Cur;
			const float Pop = bNow ? 1.f + 0.18f * (1.f - StageAnim) * FMath::Sin(StageAnim * PI) + 0.03f * FMath::Sin(Time * 6.f) : 1.f;
			const FVector2D CS(ChipW * Pop, ChipH * Pop), CP(CX - (CS.X - ChipW) * 0.5f, SY - (CS.Y - ChipH) * 0.5f);
			const FLinearColor Fill = bNow ? FLinearColor(PC.R * 0.85f, PC.G * 0.85f, PC.B * 0.85f, 1.f) : (bDone ? FLinearColor(0.05f, 0.42f, 0.38f, 1.f) : FLinearColor(0.04f, 0.12f, 0.40f, 1.f));
			if (bNow) ZoogiUIStyle::SBox(Out, Ly + 4, Geo, CP - FVector2D(4.f, 4.f) * u, CS + FVector2D(8.f, 8.f) * u, FLinearColor(1.f, 1.f, 1.f, 0.25f + 0.2f * FMath::Sin(Time * 6.f)), FLinearColor::Transparent, 12.f * u, 0.f);
			ZoogiUIStyle::SBox(Out, Ly + 5, Geo, CP, CS, Fill, bNow ? FLinearColor::White : Outline, 9.f * u, 3.f * u);
			FString Lb = AZoogiGameMode::StageName((EZoogiStage)i);
			if (bDone) Lb = FString::Printf(TEXT("%s +"), *Lb);
			const FSlateFontInfo F = Font(12.f);
			ZoogiUIStyle::SText(Out, Ly + 6, Geo, FVector2D(CP.X + (CS.X - Measure(Lb, F).X) * 0.5f, CP.Y + (CS.Y - Measure(Lb, F).Y) * 0.5f), Lb, F, bNow || bDone ? FLinearColor::White : MutedText);
		}
		// active player + status under the stepper
		FString Status;
		if (G->Phase == EZoogiPhase::Forming) Status = TEXT("STAGE FORMING");
		else if (G->Phase == EZoogiPhase::GameOver) Status = TEXT("MATCH OVER");
		else if (AB) Status = FString::Printf(TEXT("P%d %s  %s%s"), AB->PlayerIndex + 1, *AB->Def.Name.ToUpper(), AB->bCPU ? TEXT("(CPU)") : TEXT("(HUMAN)"),
			G->Stage == EZoogiStage::Battle ? *FString::Printf(TEXT("   BATTLE: %s"), AZoogiGameMode::StepName(G->BattleStep)) : TEXT(""));
		const float TPop = 1.f + 0.25f * (1.f - TurnAnim) * FMath::Sin(TurnAnim * PI);
		const FSlateFontInfo SF = Font(12.f * TPop);
		const FVector2D SSz = Measure(Status, SF);
		const float StX = SX + SW * 0.5f - SSz.X * 0.5f - 16.f * u, StY = SY + ChipH + 3.f * u;
		ZoogiUIStyle::SBox(Out, Ly + 5, Geo, FVector2D(StX, StY), FVector2D(SSz.X + 32.f * u, FMath::Max(18.f * u, SSz.Y + 2.f * u)), FLinearColor(PC.R * 0.4f, PC.G * 0.4f, PC.B * 0.4f, 0.95f), Outline, 8.f * u, 2.f * u);
		ZoogiUIStyle::SBox(Out, Ly + 6, Geo, FVector2D(StX + 7.f * u, StY + 4.f * u), FVector2D(10.f * u, 10.f * u), PC, Outline, 5.f * u, 1.5f * u);
		ZoogiUIStyle::SText(Out, Ly + 7, Geo, FVector2D(StX + 22.f * u, StY + 1.f * u), Status, SF, FLinearColor::White);
	}

	// 4) bottom tray + prompt bar
	{
		const FVector2D R0 = L.Tray.Min * s, RS = (L.Tray.Max - L.Tray.Min) * s;
		ZoogiUIStyle::SPanel(Out, Ly + 2, Geo, R0, RS, FLinearColor(0.03f, 0.10f, 0.36f, 0.95f), FLinearColor(0.25f, 0.85f, 1.f, 1.f), 16.f * u);
		const FVector2D P0 = L.Prompt.Min * s, PS = (L.Prompt.Max - L.Prompt.Min) * s;
		const float Sl = 1.f - FMath::Square(1.f - PromptAnim);
		ZoogiUIStyle::SBox(Out, Ly + 4, Geo, P0, PS, FLinearColor(0.02f, 0.06f, 0.24f, 0.97f), FMath::Lerp(FLinearColor::White, Gold, Sl), PS.Y * 0.5f, 3.f * u);
		FString P = LastPrompt;
		const FSlateFontInfo PF = Font(15.f);
		while (P.Len() > 8 && Measure(P, PF).X > PS.X - 40.f * u) P = P.LeftChop(4) + TEXT("..");
		const FVector2D PSz = Measure(P, PF);
		ZoogiUIStyle::SText(Out, Ly + 6, Geo, FVector2D(P0.X + (PS.X - PSz.X) * 0.5f, P0.Y + (PS.Y - PSz.Y) * 0.5f + (1.f - Sl) * 10.f * u), P, PF, FLinearColor(1.f, 1.f, 1.f, Sl));
		if (L.TrayOpen > 0.6f && !L.bCompact)
		{
			const FString H = HintText(G);
			Txt(R0.X + 18.f * u, R0.Y + RS.Y - 26.f * u, H, 10.f, FLinearColor(MutedText.R, MutedText.G, MutedText.B, 0.8f));
		}
		if (EndAnim > 0.f && G->Phase == EZoogiPhase::GameOver)
		{
			const float K = FMath::Min(1.f, EndAnim * 3.f);
			Txt(P0.X + PS.X * 0.5f, P0.Y + PS.Y + 14.f * u + 20.f * u * (1.f - K), TEXT("MATCH OVER"), 26.f * (0.6f + 0.4f * K), FLinearColor(Gold.R, Gold.G, Gold.B, K), 0.5f);
		}
	}
	return Ly + 10;
}
