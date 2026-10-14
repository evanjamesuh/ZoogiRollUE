// cardui: see ZoogiCardHandWidget.h
#include "ZoogiCardHandWidget.h"
#include "ZoogiCardWidget.h"
#include "ZoogiCardTargetArrow.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCards.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Misc/ConfigCacheIni.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "TimerManager.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "ZoogiUIStyle.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiHudArt.h"

namespace
{
	constexpr float CHDrawDur = 0.32f, CHDrawStagger = 0.1f;  // 5 cards ~0.72 s
	constexpr float CHDiscardDur = 0.38f, CHDiscardStagger = 0.03f; // whole hand <= 0.5 s
	constexpr float CHAssignDur = 0.28f, CHHoverScale = 1.3f;

	float CHEaseOut(float A) { const float U = 1.f - FMath::Clamp(A, 0.f, 1.f); return 1.f - U * U * U; }
	FVector2D CHBez(const FVector2D& A, const FVector2D& C, const FVector2D& B, float T) { const float U = 1.f - T; return A * (U * U) + C * (2.f * U * T) + B * (T * T); }
	FString CHWrap(const FString& S, int32 Max)
	{
		TArray<FString> Words; S.ParseIntoArray(Words, TEXT(" "));
		FString Out, Line;
		for (const FString& W : Words)
		{
			if (Line.Len() + W.Len() + 1 > Max && !Line.IsEmpty()) { Out += Line + TEXT("\n"); Line.Reset(); }
			Line += (Line.IsEmpty() ? TEXT("") : TEXT(" ")) + W;
		}
		return Out + Line;
	}
	int32 CHLines(const FString& S) { int32 N = 1; for (TCHAR C : S) if (C == '\n') ++N; return N; }
	void CHBox(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& Fill, const FLinearColor& Edge, float R = 10.f, float W = 2.f)
	{
		ZoogiUIStyle::SBox(Out, L, G, P, S, Fill, Edge, R, W);
	}
	void CHText(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FString& T, int32 Size, const FLinearColor& C, bool bBold = false)
	{
		// uistyle: bold outlined text with a drop shadow
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		ZoogiUIStyle::AddOutline(F, Size >= 18 ? 2.5f : 1.5f);
		ZoogiUIStyle::SText(Out, L, G, P, T, F, C);
	}
	// Default ON whenever cards mode is on: [ZoogiRoll.Cards] bCardHandUI (default True) or -ZoogiCardHandUI; -ZoogiNoCardHandUI turns it off.
	bool CHFlagOn()
	{
		static const bool b = []() { bool bIni = true; if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Cards"), TEXT("bCardHandUI"), bIni, GGameIni);
			return !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCardHandUI")) && (bIni || FParse::Param(FCommandLine::Get(), TEXT("ZoogiCardHandUI"))); }();
		return b;
	}
}

bool UZoogiCardHandWidget::IsEnabled(const AZoogiGameMode* G) { return CHFlagOn() && G && G->CardsOn(); }

void UZoogiCardHandWidget::Spawn(AZoogiGameMode* G)
{
	if (!G || !G->GetWorld() || !CHFlagOn()) return;
	TSharedRef<FTimerHandle> H = MakeShared<FTimerHandle>();
	TWeakObjectPtr<AZoogiGameMode> WG(G);
	G->GetWorldTimerManager().SetTimer(*H, FTimerDelegate::CreateLambda([WG, H]()
	{
		AZoogiGameMode* GM = WG.Get();
		if (!GM) return;
		APlayerController* PC = GM->GetWorld()->GetFirstPlayerController();
		if (!PC || !PC->IsLocalController()) return;
		GM->GetWorldTimerManager().ClearTimer(*H);
		UZoogiCardHandWidget* W = CreateWidget<UZoogiCardHandWidget>(PC, UZoogiCardHandWidget::StaticClass());
		if (W) W->AddToViewport(6);
		UE_LOG(LogZoogi, Log, TEXT("CARDUI hand spawned (%s) cards=%d"), W ? TEXT("ok") : TEXT("FAILED"), GM->CardsOn() ? 1 : 0);
	}), 0.25f, true);
}

TSharedRef<SWidget> UZoogiCardHandWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardHandRoot"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Root;
		auto MakeBtn = [&](const TCHAR* Label, const FLinearColor& C, UTextBlock*& OutText) -> UButton*
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
				B->SetStyle(ZoogiUIStyle::ButtonStyle(C, true)); // uistyle: chunky saturated plate + navy outline (was the default gray UButton)
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			T->SetText(FText::FromString(Label));
			T->SetFont(ZoogiUIStyle::BoldFont(17, 2.f));
			T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			T->SetShadowOffset(FVector2D(2.f, 3.f));
			T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.06f, 0.6f));
			B->SetContent(T);
			B->SetVisibility(ESlateVisibility::Collapsed);
			OutText = T;
			return B;
		};
		UTextBlock* Dummy = nullptr; UTextBlock* CText = nullptr;
		ConfirmBtn = MakeBtn(TEXT("CONFIRM"), FLinearColor(0.35f, 1.f, 0.45f), CText); ConfirmText = CText;
		ClearBtn = MakeBtn(TEXT("CLEAR"), FLinearColor(1.f, 0.45f, 0.38f), Dummy);
		ConfirmBtn->OnClicked.AddDynamic(this, &UZoogiCardHandWidget::OnConfirm);
		ClearBtn->OnClicked.AddDynamic(this, &UZoogiCardHandWidget::OnClear);
		for (UButton* B : { ConfirmBtn.Get(), ClearBtn.Get() })
		{
			UCanvasPanelSlot* S = Root->AddChildToCanvas(B);
			S->SetAnchors(FAnchors(1.f, 1.f)); S->SetAlignment(FVector2D(1.f, 1.f)); S->SetAutoSize(false); S->SetZOrder(1000);
		}
		Cast<UCanvasPanelSlot>(ConfirmBtn->Slot)->SetPosition(FVector2D(-24.f, -196.f)); Cast<UCanvasPanelSlot>(ConfirmBtn->Slot)->SetSize(FVector2D(220.f, 52.f));
		Cast<UCanvasPanelSlot>(ClearBtn->Slot)->SetPosition(FVector2D(-24.f, -256.f)); Cast<UCanvasPanelSlot>(ClearBtn->Slot)->SetSize(FVector2D(220.f, 52.f)); // >= 48 dp tap target
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Super::RebuildWidget();
}

FLinearColor UZoogiCardHandWidget::TypeColor(uint8 Type)
{
	switch ((EZoogiCardType)Type)
	{
	case EZoogiCardType::Infusion:  return FLinearColor(1.f, 0.55f, 0.15f);
	case EZoogiCardType::Technique: return FLinearColor(0.2f, 0.85f, 1.f);
	case EZoogiCardType::Defense:   return FLinearColor(0.35f, 0.55f, 1.f);
	case EZoogiCardType::Utility:   return FLinearColor(0.35f, 1.f, 0.5f);
	case EZoogiCardType::Impact:    return FLinearColor(1.f, 0.3f, 0.25f);
	case EZoogiCardType::Terrain:   return FLinearColor(0.6f, 0.95f, 1.f);
	default:                        return FLinearColor(0.8f, 0.45f, 1.f);
	}
}

UTexture2D* UZoogiCardHandWidget::ArtFor(FName CardId)
{
	if (TObjectPtr<UTexture2D>* T = ArtCache.Find(CardId)) return *T;
	if (ArtTried.Contains(CardId)) return nullptr;
	ArtTried.Add(CardId);
	// Card art from Evan's v3.0.0 package (Card_Art/<card id>.png), loaded at runtime from Content/Data/CardArt
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/CardArt") / (CardId.ToString() + TEXT(".png"));
	UTexture2D* T = FPaths::FileExists(Path) ? FImageUtils::ImportFileAsTexture2D(Path) : nullptr;
	if (T) ArtCache.Add(CardId, T);
	UE_LOG(LogZoogi, Log, TEXT("CARDUI art %s: %s"), *CardId.ToString(), T ? TEXT("loaded") : TEXT("missing (plain face)"));
	return T;
}

UZoogiCardWidget* UZoogiCardHandWidget::MakeCard(int32 InstanceId, FName CardId)
{
	UZoogiCardWidget* C = CreateWidget<UZoogiCardWidget>(this, UZoogiCardWidget::StaticClass());
	const UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(this);
	const FZoogiCardDef* D = Cards ? Cards->Def(CardId) : nullptr;
	C->Hand = this;
	C->InstanceId = InstanceId;
	C->CardId = CardId;
	C->Name = D ? D->Name : CardId.ToString();
	C->Rule = D ? D->Rule : FString();
	C->Cost = D ? D->Cost : 0;
	C->Color = TypeColor(D ? (uint8)D->Type : 6);
	C->Art = ArtFor(CardId);
	switch (D ? D->Element : EZoogiCardElement::Neutral)
	{
	case EZoogiCardElement::Void:  C->Tablet = TEXT("tablet_void"); break;
	case EZoogiCardElement::Fire:  C->Tablet = TEXT("tablet_fire"); break;
	case EZoogiCardElement::Shock: C->Tablet = TEXT("tablet_storm"); break;
	case EZoogiCardElement::Frost: C->Tablet = TEXT("tablet_frost"); break;
	default:                       C->Tablet = TEXT("tablet_arcane"); break;
	}
	UCanvasPanelSlot* S = Root->AddChildToCanvas(C);
	S->SetAnchors(FAnchors(0.f, 0.f));
	S->SetAlignment(FVector2D(0.5f, 1.f));
	S->SetAutoSize(false);
	S->SetSize(FVector2D(UZoogiCardWidget::CardW, UZoogiCardWidget::CardH));
	C->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	AllCards.Add(C);
	return C;
}

void UZoogiCardHandWidget::Fly(FVis& V, const FVector2D& To, float Dur, float Delay, float ToScale, float ToAngle, float Lift, bool bToLayout)
{
	V.bFly = true; V.T = 0.f; V.Dur = Dur; V.Delay = Delay;
	V.From = V.Pos; V.FromAngle = V.Angle; V.FromScale = V.Scale;
	V.To = To; V.ToScale = ToScale; V.ToAngle = ToAngle; V.Lift = Lift; V.bToLayout = bToLayout;
}

FVector2D UZoogiCardHandWidget::Local(const FPointerEvent& Ev) const { return CachedGeo.AbsoluteToLocal(Ev.GetScreenSpacePosition()); }

void UZoogiCardHandWidget::DiscardAll(const FZoogiCardPlayerState* S)
{
	const ZoogiFramed::FLayout& SFL = ZoogiFramed::Current();
	const float Sk = SFL.bValid && SFL.Viewport.X > 0.f ? ViewSize.X / SFL.Viewport.X : 1.f;
	const FVector2D Pile(ViewSize.X - 330.f - (SFL.bValid ? SFL.Safe.Right * Sk : 0.f), ViewSize.Y - 40.f - (SFL.bValid ? SFL.Safe.Bottom * Sk : 0.f)); // altar, inside the safe area
	int32 N = 0, Gone = 0;
	for (TPair<int32, FVis>& P : Vis)
	{
		FVis& V = P.Value;
		bool bCommitted = false;
		if (S) for (const FZoogiCardCommitted& K : S->Committed) if (K.Card.InstanceId == P.Key) bCommitted = true;
		if (V.State == EVis::Hand && bCommitted) { V.State = EVis::ToTarget; Fly(V, TargetPos, CHAssignDur, 0.f, 0.3f, 0.f, 60.f, false); ++Gone; }
		else if (V.State == EVis::Hand) { V.State = EVis::ToDiscard; Fly(V, Pile, CHDiscardDur, N * CHDiscardStagger, 0.45f, 25.f, 140.f, false); ++N; }
		else if (V.State == EVis::Assigned) { V.State = EVis::ToDiscard; V.bFly = false; V.T = 1.f; V.Opacity = 0.f; }
	}
	HandOrder.Reset();
	if (N || Gone) UE_LOG(LogZoogi, Log, TEXT("CARDUI discard P%d turn=%d leftover=%d -> discard pile, committed=%d"), KeyPlayer + 1, KeyTurn, N, Gone);
}

void UZoogiCardHandWidget::UpdateTarget(AZoogiGameMode* G)
{
	bTargetOnScreen = false;
	AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	APlayerController* PC = GetOwningPlayer();
	if (!B || !PC) return;
	TargetName = FString::Printf(TEXT("P%d %s"), B->PlayerIndex + 1, *B->Def.Name);
	const FVector L = B->BallLocation();
	FVector2D P, E;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, L, P, false)) return;
	const FVector Right = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().RotateVector(FVector::RightVector) : FVector::RightVector;
	const float R = FMath::Max(B->GetSimpleCollisionRadius(), 30.f);
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, L + Right * R, E, false)) TargetR = FMath::Clamp(FVector2D::Distance(P, E) * 1.25f, 40.f, 220.f);
	TargetPos = P;
	bTargetOnScreen = true;
}

bool UZoogiCardHandWidget::OverTarget() const { return bTargetOnScreen && FVector2D::Distance(Cursor, TargetPos) <= FMath::Max(TargetR * 1.35f, 60.f); }

void UZoogiCardHandWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	CachedGeo = Geo;
	ViewSize = Geo.GetLocalSize();
	{
		// framed HUD: CONFIRM / CLEAR sit in the bottom tray's right corner (inside the safe area), not over the arena
		const bool bFr = ZoogiFramed::IsActive(GetWorld()) && ZoogiFramed::Current().bValid;
		const ZoogiFramed::FLayout& FL = ZoogiFramed::Current();
		const float k = bFr && FL.Viewport.X > 0.f ? ViewSize.X / FL.Viewport.X : 1.f;
		const float R = bFr ? -24.f - FL.Safe.Right * k : -24.f, Bm = bFr ? -FL.Safe.Bottom * k : 0.f;
		if (ConfirmBtn) if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(ConfirmBtn->Slot)) S->SetPosition(FVector2D(R, bFr ? Bm - 30.f : -196.f));
		if (ClearBtn) if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(ClearBtn->Slot)) S->SetPosition(FVector2D(R, bFr ? Bm - 92.f : -256.f));
	}
	Time += Dt;
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(this);
	AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	const FZoogiCardPlayerState* S = (Cards && B && IsEnabled(G)) ? Cards->Rules().Player(B->PlayerIndex) : nullptr;
	const bool bShow = S && G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && S->bStandbyOpen;
	bInteractive = bShow && G->IsHumanAiming() && !S->bCommitted;
	UpdateTarget(G);

	// standby closed / another player's or turn's standby: leftovers fly to the discard pile
	const bool bKeyChanged = bShow && (KeyPlayer != B->PlayerIndex || KeyTurn != S->Turn);
	if ((bShown && !bShow) || (bKeyChanged && Vis.Num()))
	{
		if (Dragging.IsValid()) CancelDrag(TEXT("standby_closed"));
		DiscardAll(Cards ? Cards->Rules().Player(KeyPlayer) : nullptr);
	}
	bShown = bShow;
	if (bKeyChanged) { KeyPlayer = B->PlayerIndex; KeyTurn = S->Turn; }

	int32 SelCost = 0;
	const FZoogiCardContext Ctx = (bShow && Cards) ? Cards->BuildContext(G) : FZoogiCardContext();
	if (bShow)
	{
		// sync with the card system's hand: new instances are drawn from the deck, selected ones go to the target
		FVector2D Deck(90.f, ViewSize.Y - 40.f);
		{
			const ZoogiFramed::FLayout& DFL = ZoogiFramed::Current();
			if (DFL.bValid && ZoogiFramed::IsActive(GetWorld()) && DFL.Viewport.X > 0.f) Deck = DFL.Deck * (ViewSize.X / DFL.Viewport.X) + FVector2D(0.f, 60.f * DFL.T * ViewSize.X / DFL.Viewport.X); // hud-v3: out of the rune deck
		}
		int32 NewN = 0;
		FString Drawn;
		HandOrder.Reset();
		for (const FZoogiCardInstance& I : S->Hand)
		{
			const bool bSel = S->Selected.Contains(I.InstanceId);
			if (bSel) if (const FZoogiCardDef* D = Cards->Def(I.CardId)) SelCost += D->Cost;
			FVis* V = Vis.Find(I.InstanceId);
			if (!V)
			{
				FVis NV; NV.W = MakeCard(I.InstanceId, I.CardId); NV.Pos = Deck; NV.Scale = 0.55f; NV.Angle = -20.f; NV.Opacity = 0.f;
				V = &Vis.Add(I.InstanceId, NV);
				Fly(*V, Deck, CHDrawDur, NewN * CHDrawStagger, 1.f, 0.f, 180.f, true);
				++NewN; Drawn += FString::Printf(TEXT(" %s#%d"), *I.CardId.ToString(), I.InstanceId);
			}
			if (bSel && V->State == EVis::Hand)
			{
				// selected without a drag (CPU plan, keys 1-5): same flight to the acting Zoogi
				V->State = EVis::ToTarget; Fly(*V, TargetPos, CHAssignDur, 0.f, 0.3f, 0.f, 60.f, false);
				UE_LOG(LogZoogi, Log, TEXT("CARDUI assign %s#%d -> %s (selected outside drag)"), *I.CardId.ToString(), I.InstanceId, *TargetName);
			}
			else if (!bSel && (V->State == EVis::Assigned || V->State == EVis::ToTarget))
			{
				V->State = EVis::Hand; V->Pos = TargetPos; V->Scale = 0.3f; V->Opacity = 1.f;
				Fly(*V, TargetPos, 0.25f, 0.f, 1.f, 0.f, 80.f, true); // deselected (CLEAR / key): back into the hand
			}
			if (V->State == EVis::Hand) HandOrder.Add(I.InstanceId);
			if (UZoogiCardWidget* W = V->W.Get()) W->bAffordable = bSel || Cards->Rules().CanSelect(B->PlayerIndex, I.InstanceId, Ctx) == EZoogiCardResult::Ok;
		}
		if (NewN) UE_LOG(LogZoogi, Log, TEXT("CARDUI draw P%d turn=%d n=%d [%s ] energy=%d deck=%d discard=%d"), B->PlayerIndex + 1, S->Turn, NewN, *Drawn, S->Energy, S->DrawPile.Num(), S->Discard.Num());
	}

	// energy counter + piles
	if (Cards && IsEnabled(G) && G->Phase != EZoogiPhase::Menu)
	{
		const FZoogiCardPlayerState* KS = Cards->Rules().Player(bShow ? B->PlayerIndex : KeyPlayer);
		bEnergyVisible = KS != nullptr;
		if (KS)
		{
			EnergyShown = KS->Energy - (KS->bCommitted ? 0 : SelCost);
			EnergyMax = Cards->Rules().Tune().EnergyPerTurn;
			DrawCount = KS->DrawPile.Num(); DiscardCount = KS->Discard.Num();
			EnergyJuice.Update(EnergyMax > 0 ? FMath::Clamp((float)EnergyShown / EnergyMax, 0.f, 1.f) : 0.f, Dt);
			if (EnergyShown != LastLoggedEnergy) { UE_LOG(LogZoogi, Log, TEXT("CARDUI energy P%d %d/%d"), (bShow ? B->PlayerIndex : KeyPlayer) + 1, EnergyShown, EnergyMax); LastLoggedEnergy = EnergyShown; }
		}
	}
	else bEnergyVisible = false;
	{
		ZoogiFramed::FTrayInfo TI;
		TI.bCards = bEnergyVisible; TI.Draw = DrawCount; TI.Discard = DiscardCount; TI.Energy = EnergyShown; TI.EnergyMax = EnergyMax;
		TI.EnergyFrac = EnergyMax > 0 ? FMath::Clamp((float)EnergyShown / EnergyMax, 0.f, 1.f) : 0.f; TI.Flash = EnergyJuice.Flash; TI.Shake = EnergyJuice.Shake;
		ZoogiFramed::SetTrayInfo(TI); // hud-v3: the framed painter draws deck / discard / energy on the tray art
	}

	// touch: armed card keeps the lifted pose + tooltip; the whole layer takes the next tap while armed
	if (Armed.IsValid() && (!bInteractive || !Vis.Contains(Armed->InstanceId) || Vis[Armed->InstanceId].State != EVis::Hand)) Armed.Reset();
	if (Armed.IsValid() && !Dragging.IsValid()) Hovered = Armed;
	if (Dragging.IsValid()) PressHeld += Dt;
	{
		const ESlateVisibility Want = Armed.IsValid() ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible;
		if (GetVisibility() != Want) SetVisibility(Want);
	}

	// fan layout targets (bottom center, overlapping, tilted outward; hover = 1.3x, lift, 0 deg, neighbors spread)
	const int32 N = HandOrder.Num();
	const int32 HoverIdx = Hovered.IsValid() ? HandOrder.IndexOfByKey(Hovered->InstanceId) : INDEX_NONE;
	float Spacing = FMath::Min(108.f, 640.f / FMath::Max(N, 1));
	// framed HUD (v2): the tray no longer carries the prompt bar, so the hand is laid out fully inside it: bigger cards,
	// whole art visible at the card's own aspect (no bottom crop), a gentle fan, hover grows a little and stays in the tray
	const bool bFrTray = ZoogiFramed::IsActive(GetWorld()) && ZoogiFramed::Current().bValid;
	float FanCX = ViewSize.X * 0.5f;
	float Sc0 = 1.f, BaseY = ViewSize.Y + 70.f, ArcK = 7.f, TiltK = 6.f, HoverSc = CHHoverScale, HoverY = ViewSize.Y - 8.f, Spread = 1.f;
	FramedFanL = -1.f;
	if (bFrTray)
	{
		const ZoogiFramed::FLayout& FL = ZoogiFramed::Current();
		const float k = FL.Viewport.X > 0.f ? ViewSize.X / FL.Viewport.X : 1.f;
		// hud-v3: the fan sits in the dark card tray recess between the deck slab and the discard / energy slab
		const float TT = FL.Hand.Min.Y * k, TB = FL.Hand.Max.Y * k, HX0 = FL.Hand.Min.X * k, HX1 = FL.Hand.Max.X * k;
		FramedTrayTop = TT;
		Sc0 = FMath::Clamp((TB - TT - 24.f * FL.T * k) / UZoogiCardWidget::CardH, 0.45f, 1.3f); // shorter: the fan sits down in the recess with less overlap
		const float Avail = HX1 - HX0 - 20.f * FL.T * k;
		Spacing = FMath::Min(UZoogiCardWidget::CardW * Sc0 * 1.06f, (Avail - UZoogiCardWidget::CardW * Sc0) / FMath::Max(N - 1, 1));
		BaseY = TB + 8.f * FL.T * k; ArcK = 1.f; TiltK = 2.5f; HoverSc = Sc0 * 1.1f; HoverY = TB; Spread = Sc0 * 0.5f;
		FanCX = (HX0 + HX1) * 0.5f;
		FramedFanL = FanCX - ((N - 1) * Spacing + UZoogiCardWidget::CardW * Sc0) * 0.5f;
	}
	TMap<int32, FVector4> Layout; // x, y, angle, scale
	for (int32 i = 0; i < N; ++i)
	{
		const float K = i - (N - 1) * 0.5f;
		float X = FanCX + K * Spacing, Y = BaseY + K * K * ArcK, A = K * TiltK, Sc = Sc0;
		if (HoverIdx != INDEX_NONE)
		{
			const int32 D = i - HoverIdx;
			if (D == 0) { A = 0.f; Sc = HoverSc; Y = HoverY; }
			else X += FMath::Sign((float)D) * (FMath::Abs(D) == 1 ? 58.f : 28.f) * Spread;
		}
		Layout.Add(HandOrder[i], FVector4(X, Y, A, Sc));
	}

	// animate + apply
	TArray<int32> Remove;
	for (TPair<int32, FVis>& P : Vis)
	{
		FVis& V = P.Value;
		UZoogiCardWidget* W = V.W.Get();
		if (!W) { Remove.Add(P.Key); continue; }
		const FVector4* L = Layout.Find(P.Key);
		FVector2D Goal = L ? FVector2D(L->X, L->Y) : V.Pos;
		float GoalA = L ? (float)L->Z : V.Angle, GoalS = L ? (float)L->W : V.Scale;
		if (V.bFly)
		{
			V.T += Dt;
			const float A = (V.T - V.Delay) / FMath::Max(V.Dur, 0.01f);
			if (A <= 0.f) { V.Opacity = V.State == EVis::Hand && V.bToLayout ? 0.f : V.Opacity; }
			else
			{
				const FVector2D To = V.bToLayout ? Goal : V.To;
				const float ToA = V.bToLayout ? GoalA : V.ToAngle, ToS = V.bToLayout ? GoalS : V.ToScale;
				const float E = CHEaseOut(A);
				const FVector2D Ctrl = (V.From + To) * 0.5f + FVector2D(0.f, -V.Lift);
				V.Pos = CHBez(V.From, Ctrl, To, E);
				V.Angle = FMath::Lerp(V.FromAngle, ToA, E);
				V.Scale = FMath::Lerp(V.FromScale, ToS, E);
				V.Opacity = V.State == EVis::Hand ? FMath::Min(1.f, A * 3.f) : (A > 0.7f ? FMath::Lerp(1.f, 0.f, (A - 0.7f) / 0.3f) : 1.f);
				if (A >= 1.f)
				{
					V.bFly = false;
					if (V.State == EVis::ToTarget) { V.State = EVis::Assigned; V.Opacity = 0.f; }
					else if (V.State == EVis::ToDiscard) Remove.Add(P.Key);
				}
			}
		}
		else if (V.State == EVis::Hand)
		{
			const float K = 1.f - FMath::Exp(-16.f * Dt);
			V.Pos = FMath::Lerp(V.Pos, Goal, K); V.Angle = FMath::Lerp(V.Angle, GoalA, K); V.Scale = FMath::Lerp(V.Scale, GoalS, K); V.Opacity = 1.f;
		}
		else if (V.State == EVis::ToDiscard) Remove.Add(P.Key);
		if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(W->Slot))
		{
			CS->SetPosition(V.Pos);
			const int32 Idx = HandOrder.IndexOfByKey(P.Key);
			CS->SetZOrder(Idx == HoverIdx && Idx != INDEX_NONE ? 500 : (Idx != INDEX_NONE ? 10 + Idx : 400));
		}
		W->SetRenderTransformAngle(V.Angle);
		W->SetRenderScale(FVector2D(V.Scale, V.Scale));
		W->SetRenderOpacity(V.Opacity);
		W->bHighlight = (W == Hovered.Get() || W == Dragging.Get()) && V.State == EVis::Hand;
		W->SetVisibility(V.Opacity <= 0.01f ? ESlateVisibility::Hidden : (bInteractive && V.State == EVis::Hand && !V.bFly ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible));
	}
	for (int32 Id : Remove) { if (FVis* V = Vis.Find(Id)) { if (UZoogiCardWidget* W = V->W.Get()) { W->RemoveFromParent(); AllCards.Remove(W); } } Vis.Remove(Id); }

	// tooltip / drag preview text
	HoverTip.Reset(); HoverTitle.Reset(); DragPreview.Reset(); bDragValid = false;
	if (UZoogiCardWidget* H = Hovered.Get()) if (bShow && Vis.Contains(H->InstanceId) && Vis[H->InstanceId].State == EVis::Hand)
	{
		const FZoogiCardDef* D = Cards->Def(H->CardId);
		HoverTitle = FString::Printf(TEXT("%s   %s   cost %d"), *H->Name, D ? ZoogiCards::TypeName(D->Type) : TEXT(""), H->Cost);
		HoverTip = CHWrap(H->Rule, 46);
		const EZoogiCardResult R = Cards->Rules().CanSelect(B->PlayerIndex, H->InstanceId, Ctx);
		if (R != EZoogiCardResult::Ok) HoverTip += FString::Printf(TEXT("\n[can't play: %s]"), ZoogiCardResultName(R));
	}
	if (UZoogiCardWidget* Dg = Dragging.Get())
	{
		DragColor = Dg->Color;
		const EZoogiCardResult R = Cards ? Cards->Rules().CanSelect(B->PlayerIndex, Dg->InstanceId, Ctx) : EZoogiCardResult::UnknownPlayer;
		bDragValid = R == EZoogiCardResult::Ok;
		DragPreview = bDragValid
			? FString::Printf(TEXT("%s -> %s\n%s\nenergy %d -> %d"), *Dg->Name, *TargetName, *CHWrap(Dg->Rule, 40), EnergyShown, EnergyShown - Dg->Cost)
			: FString::Printf(TEXT("%s: %s"), *Dg->Name, ZoogiCardResultName(R));
	}
	for (FReact& R : Reacts) R.T += Dt;
	Reacts.RemoveAll([](const FReact& R) { return R.T > 0.6f; });

	const bool bBtns = bInteractive;
	ConfirmBtn->SetVisibility(bBtns ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ClearBtn->SetVisibility(bBtns && S && S->Selected.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bBtns) { ConfirmText->SetText(FText::FromString(SelCost ? FString::Printf(TEXT("CONFIRM  -%d"), SelCost) : FString(TEXT("CONFIRM (PASS)")))); ConfirmBtn->SetIsEnabled(EnergyShown >= 0); }
}

void UZoogiCardHandWidget::CardHover(UZoogiCardWidget* C, bool bOn)
{
	if (Dragging.IsValid()) return; // the dragged card keeps its hover pose
	if (bOn) Hovered = C; else if (Hovered.Get() == C) Hovered.Reset();
}

bool UZoogiCardHandWidget::CardPress(UZoogiCardWidget* C, const FPointerEvent& Ev)
{
	const FVis* V = C ? Vis.Find(C->InstanceId) : nullptr;
	if (!bInteractive || !V || V->State != EVis::Hand || V->bFly || Dragging.IsValid()) return false;
	Dragging = C; Hovered = C; Cursor = Local(Ev); PressPos = Cursor; PressHeld = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("CARDUI drag start %s#%d"), *C->CardId.ToString(), C->InstanceId);
	return true;
}

void UZoogiCardHandWidget::CardDragMove(UZoogiCardWidget* C, const FPointerEvent& Ev) { Cursor = Local(Ev); }

void UZoogiCardHandWidget::CancelDrag(const TCHAR* Why)
{
	if (UZoogiCardWidget* C = Dragging.Get()) UE_LOG(LogZoogi, Log, TEXT("CARDUI drop cancelled %s#%d (%s)"), *C->CardId.ToString(), C->InstanceId, Why);
	Dragging.Reset(); Hovered.Reset();
}

void UZoogiCardHandWidget::CardRelease(UZoogiCardWidget* C, const FPointerEvent& Ev, bool bCancel)
{
	Cursor = Local(Ev);
	if (bCancel) { CancelDrag(TEXT("right_click")); Armed.Reset(); return; }
	if (!OverTarget())
	{
		// touch / mobile: a tap (no real drag) arms the card; the next tap on the target assigns it, a tap elsewhere disarms
		if (FVector2D::Distance(Cursor, PressPos) < 14.f && PressHeld < 0.4f)
		{
			Armed = Armed.Get() == C ? nullptr : C;
			Dragging.Reset();
			UE_LOG(LogZoogi, Log, TEXT("CARDUI tap %s#%d -> %s"), *C->CardId.ToString(), C->InstanceId, Armed.IsValid() ? TEXT("armed (tap the target)") : TEXT("disarmed"));
			return;
		}
		CancelDrag(TEXT("released_off_target")); return;
	}
	AssignTo(C);
}

void UZoogiCardHandWidget::AssignTo(UZoogiCardWidget* C)
{
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(this);
	AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	const FZoogiCardPlayerState* S = Cards && B ? Cards->Rules().Player(B->PlayerIndex) : nullptr;
	const int32 HandSlot = S ? S->Hand.IndexOfByPredicate([C](const FZoogiCardInstance& I) { return I.InstanceId == C->InstanceId; }) : INDEX_NONE;
	Armed.Reset();
	if (HandSlot == INDEX_NONE) { CancelDrag(TEXT("not_in_hand")); return; }
	// the one select path (same as the Phase 4 panel / keys 1-5): the card system validates, nothing is spent here
	G->MenuAction(TEXT("CardToggle"), HandSlot);
	const bool bOk = S->Selected.Contains(C->InstanceId);
	UE_LOG(LogZoogi, Log, TEXT("CARDUI drop %s#%d slot=%d on %s -> %s (selected %d, energy left %d)"), *C->CardId.ToString(), C->InstanceId, HandSlot, *TargetName, bOk ? TEXT("assigned") : TEXT("rejected"), S->Selected.Num(), S->Energy);
	Dragging.Reset(); Hovered.Reset();
	if (bOk)
	{
		if (FVis* V = Vis.Find(C->InstanceId)) { V->State = EVis::ToTarget; Fly(*V, TargetPos, CHAssignDur, 0.f, 0.3f, 0.f, 60.f, false); }
		Reacts.Add({ TargetPos, 0.f, C->Color, C->Name });
	}
}

FReply UZoogiCardHandWidget::NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev)
{
	UZoogiCardWidget* A = Armed.Get();
	if (!A || !bInteractive) return Super::NativeOnMouseButtonDown(Geo, Ev);
	Cursor = Local(Ev);
	if (OverTarget()) AssignTo(A);
	else { UE_LOG(LogZoogi, Log, TEXT("CARDUI tap off target -> disarmed")); Armed.Reset(); Hovered.Reset(); }
	return FReply::Handled();
}

void UZoogiCardHandWidget::OnConfirm()
{
	if (AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr) { UE_LOG(LogZoogi, Log, TEXT("CARDUI confirm")); G->MenuAction(TEXT("CardCommit"), 0); }
}

void UZoogiCardHandWidget::OnClear()
{
	if (AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr) { UE_LOG(LogZoogi, Log, TEXT("CARDUI clear")); G->MenuAction(TEXT("CardClear"), 0); }
}

int32 UZoogiCardHandWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	int32 L = Super::NativePaint(Args, Geo, Cull, Out, Layer, Style, bParentEnabled) + 1;
	const FVector2D V = ViewSize;
	if (bEnergyVisible && !(ZoogiFramed::IsActive(GetWorld()) && ZoogiFramed::Current().bValid)) // hud-v3: framed draws them on the tray art
	{
		// deck (bottom left) and discard pile (bottom right) with counts
		const FVector2D CS(70.f, 104.f);
		// uistyle: saturated outlined pile plates (navy deck, magenta discard)
		for (int32 i = 2; i >= 0; --i) CHBox(Out, L, Geo, FVector2D(55.f + i * 4.f, V.Y - 128.f - i * 4.f), CS, FLinearColor(0.05f, 0.16f, 0.55f, 0.95f), ZoogiUIStyle::Outline, 10.f, 3.5f);
		CHBox(Out, L, Geo, FVector2D(63.f, V.Y - 132.f), FVector2D(CS.X - 16.f, 5.f), FLinearColor(1.f, 1.f, 1.f, 0.35f), FLinearColor::Transparent, 2.5f, 0.f);
		CHText(Out, L + 1, Geo, FVector2D(76.f, V.Y - 96.f), FString::FromInt(DrawCount), 24, FLinearColor::White, true);
		CHText(Out, L + 1, Geo, FVector2D(52.f, V.Y - 22.f), TEXT("DECK"), 12, FLinearColor(0.7f, 0.9f, 1.f), true);
		CHBox(Out, L, Geo, FVector2D(V.X - 365.f, V.Y - 128.f), CS, FLinearColor(0.55f, 0.06f, 0.35f, 0.92f), ZoogiUIStyle::Outline, 10.f, 3.5f);
		CHText(Out, L + 1, Geo, FVector2D(V.X - 343.f, V.Y - 96.f), FString::FromInt(DiscardCount), 24, FLinearColor::White, true);
		CHText(Out, L + 1, Geo, FVector2D(V.X - 376.f, V.Y - 22.f), TEXT("DISCARD"), 12, FLinearColor(1.f, 0.8f, 0.95f), true);
		// total Card Energy counter (bottom right): chunky violet disc + gloss, segmented violet bar, juice (flash on gain, shake + red trail on loss)
		const float R = 54.f;
		const FVector2D C = FVector2D(V.X - 100.f, V.Y - 96.f) + EnergyJuice.ShakeOffset(5.f, Time);
		CHBox(Out, L, Geo, C - FVector2D(R, R) + FVector2D(3.f, 5.f), FVector2D(R * 2.f, R * 2.f), FLinearColor(0.f, 0.f, 0.05f, 0.35f), FLinearColor::Transparent, R, 0.f);
		CHBox(Out, L, Geo, C - FVector2D(R, R), FVector2D(R * 2.f, R * 2.f), ZoogiUIStyle::CardEnergyB, ZoogiUIStyle::Outline, R, 4.f);
		CHBox(Out, L, Geo, C - FVector2D(R - 9.f, R - 9.f), FVector2D(R * 2.f - 18.f, R * 2.f - 18.f), ZoogiUIStyle::CardEnergyA * FLinearColor(1.f, 1.f, 1.f, 0.55f), FLinearColor(1.f, 1.f, 1.f, 0.25f), R - 9.f, 2.f);
		CHBox(Out, L, Geo, C + FVector2D(-R * 0.55f, -R + 9.f), FVector2D(R * 1.1f, 10.f), FLinearColor(1.f, 1.f, 1.f, 0.5f), FLinearColor::Transparent, 5.f, 0.f);
		if (EnergyJuice.Flash > 0.f) CHBox(Out, L, Geo, C - FVector2D(R, R), FVector2D(R * 2.f, R * 2.f), FLinearColor(1.f, 1.f, 1.f, 0.6f * EnergyJuice.Flash), FLinearColor::Transparent, R, 0.f);
		CHText(Out, L + 1, Geo, C + FVector2D(-36.f, -32.f), FString::Printf(TEXT("%d/%d"), EnergyShown, EnergyMax), 28, EnergyShown >= 0 ? FLinearColor::White : FLinearColor(1.f, 0.45f, 0.4f), true);
		CHText(Out, L + 1, Geo, C + FVector2D(-30.f, 8.f), TEXT("ENERGY"), 12, FLinearColor(1.f, 0.9f, 1.f), true);
		ZoogiUIStyle::SBar(Out, L + 1, Geo, FVector2D(V.X - 170.f, V.Y - 30.f), FVector2D(130.f, 14.f), 0.f, ZoogiUIStyle::CardEnergyA, ZoogiUIStyle::CardEnergyB, &EnergyJuice, FMath::Clamp(EnergyMax, 1, 10));
	}
	// hover tooltip with the card description
	const bool bLongPress = Dragging.IsValid() && PressHeld >= 0.4f && FVector2D::Distance(Cursor, PressPos) < 14.f; // touch: long-press = tooltip
	if (!HoverTitle.IsEmpty() && (!Dragging.IsValid() || bLongPress))
	{
		const int32 NL = CHLines(HoverTip);
		FVector2D Sz(400.f, 46.f + NL * 20.f);
		const bool bTrayTip = FramedFanL > 0.f && FramedFanL - 160.f >= 340.f;
		if (bTrayTip) Sz.X = FMath::Min(400.f, FramedFanL - 160.f);
		FVector2D P(FMath::Clamp(V.X * 0.5f - Sz.X * 0.5f, 10.f, V.X - Sz.X - 10.f), V.Y - UZoogiCardWidget::CardH * CHHoverScale - Sz.Y - 24.f);
		if (bTrayTip) P = FVector2D(FramedFanL - 10.f - Sz.X, FramedTrayTop + 12.f); // framed: in the tray left of the fan, not over the arena
		ZoogiUIStyle::SPanel(Out, L + 2, Geo, P, Sz, ZoogiUIStyle::PanelNavy, ZoogiUIStyle::Gold, 12.f);
		CHText(Out, L + 3, Geo, P + FVector2D(14.f, 8.f), HoverTitle, 14, FLinearColor(1.f, 0.9f, 0.6f), true);
		CHText(Out, L + 3, Geo, P + FVector2D(14.f, 34.f), HoverTip, 12, FLinearColor(0.9f, 0.92f, 1.f));
	}
	// drag: target glow in the card color + effect preview + segmented arrow from the hand to the cursor
	if (const UZoogiCardWidget* Dg = Dragging.Get())
	{
		const bool bOver = OverTarget();
		const FLinearColor C = bDragValid ? DragColor : FLinearColor(1.f, 0.3f, 0.25f);
		if (bTargetOnScreen)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 8.f);
			FLinearColor G1 = C; G1.A = bOver ? 0.95f : 0.55f + 0.25f * Pulse;
			FLinearColor G2 = C; G2.A = 0.25f;
			ZoogiCardTargetArrow::Ring(Out, L + 2, Geo, TargetPos, TargetR + 6.f * Pulse, G1, bOver ? 6.f : 4.f);
			ZoogiCardTargetArrow::Ring(Out, L + 2, Geo, TargetPos, TargetR + 14.f + 6.f * Pulse, G2, 10.f);
			const float BR = (TargetR + 22.f) * 2.f;
			ZoogiHudArt::Img(Out, L + 3, Geo, TargetPos - FVector2D(BR, BR) * 0.5f, FVector2D(BR, BR), bOver ? (bDragValid ? TEXT("prompt_lock") : TEXT("prompt_red")) : TEXT("prompt_gold")); // ui kit: gold = target, lock = locked, red = invalid
			if (bOver || !bDragValid)
			{
				const FVector2D P = TargetPos + FVector2D(TargetR + 24.f, -40.f);
				const FVector2D Sz(330.f, 30.f + CHLines(DragPreview) * 19.f);
				ZoogiUIStyle::SPanel(Out, L + 3, Geo, P, Sz, ZoogiUIStyle::PanelTeal, C, 10.f);
				CHText(Out, L + 4, Geo, P + FVector2D(12.f, 8.f), DragPreview, 12, FLinearColor::White, true);
			}
		}
		const FVis* DV = Vis.Find(Dg->InstanceId);
		const FVector2D Start = DV ? DV->Pos - FVector2D(0.f, UZoogiCardWidget::CardH * DV->Scale * 0.92f) : FVector2D(V.X * 0.5f, V.Y - 300.f);
		ZoogiCardTargetArrow::Paint(Out, L + 5, Geo, Start, bOver ? TargetPos : Cursor, bOver && bDragValid ? C : C * FLinearColor(1.f, 1.f, 1.f, 0.85f), Time);
	}
	// touch: armed card -> pulsing ring on the target + "TAP TARGET"
	if (const UZoogiCardWidget* Am = Armed.Get()) if (bTargetOnScreen && !Dragging.IsValid())
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 8.f);
		FLinearColor G1 = Am->Color; G1.A = 0.6f + 0.3f * Pulse;
		ZoogiCardTargetArrow::Ring(Out, L + 2, Geo, TargetPos, TargetR + 6.f * Pulse, G1, 5.f);
		CHText(Out, L + 4, Geo, TargetPos + FVector2D(-60.f, -TargetR - 44.f), TEXT("TAP TARGET"), 18, FLinearColor::White, true);
	}
	// target reaction: burst ring + card name pop
	for (const FReact& R : Reacts)
	{
		const float A = R.T / 0.6f;
		FLinearColor C = R.Color; C.A = 1.f - A;
		ZoogiCardTargetArrow::Ring(Out, L + 6, Geo, R.Pos, TargetR * (0.6f + 1.4f * CHEaseOut(A)), C, 8.f * (1.f - A) + 1.f);
		CHText(Out, L + 7, Geo, R.Pos + FVector2D(-50.f, -TargetR - 40.f - 40.f * A), R.Label, 20, C, true);
	}
	return L + 8;
}
