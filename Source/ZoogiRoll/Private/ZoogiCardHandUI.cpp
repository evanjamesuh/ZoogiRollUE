// v3 Phase 4: the Standby card hand as real UMG, built on the Roguelike Deckbuilder Toolkit's card widget
// (/Game/RLDB/Widgets/WBP_CardVisual). Each hand card is a toolkit WBP_CardVisual (frame, mana crystal, name, type,
// description) inside a transparent UZoogiButton, so mouse, touch and the existing keys (1-5 toggle, A awakening,
// Backspace clear, Enter / Space confirm) all reach the same AZoogiGameMode::MenuAction calls as before.
// The toolkit widget is display only: we fill its bound text widgets and call its own ToggleCardGlow for selection; its
// toolkit gameplay (BP_Card actor, hand manager) is not used. Missing asset -> plain text cards (same controls).
// Shows: per-card cost + affordability (dimmed with the reason), total committed cost, energy left, and the effects
// of the selected cards before CONFIRM.
#include "ZoogiAwakenWidget.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCards.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UnrealType.h"

namespace
{
	FSlateBrush CardSolid(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float W = 0.f)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(10.f, 10.f, 10.f, 10.f), FSlateColor(Outline), W);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}
	UCanvasPanelSlot* CardPlace(UCanvasPanel* P, UWidget* W, FVector2D Pos, FVector2D Size)
	{
		UCanvasPanelSlot* S = P->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.f, 0.f));
		S->SetPosition(Pos);
		if (Size.X > 0.f) { S->SetAutoSize(false); S->SetSize(Size); } else S->SetAutoSize(true);
		return S;
	}
	template <class T> T* BoundWidget(UUserWidget* W, const TCHAR* Name)
	{
		if (!W) return nullptr;
		const FObjectProperty* P = FindFProperty<FObjectProperty>(W->GetClass(), Name);
		return P ? Cast<T>(P->GetObjectPropertyValue_InContainer(W)) : nullptr;
	}
	/** Calls ToggleCardGlow(bool bEnable, FLinearColor Color) on the toolkit widget (its own glow animation). */
	void CallGlow(UUserWidget* W, bool bOn, const FLinearColor& C)
	{
		UFunction* F = W ? W->FindFunction(TEXT("ToggleCardGlow")) : nullptr;
		if (!F) return;
		TArray<uint8> Buf; Buf.SetNumZeroed(F->ParmsSize);
		for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (const FBoolProperty* B = CastField<FBoolProperty>(*It)) B->SetPropertyValue_InContainer(Buf.GetData(), bOn);
			else if (const FStructProperty* S = CastField<FStructProperty>(*It)) if (S->Struct == TBaseStructure<FLinearColor>::Get()) *S->ContainerPtrToValuePtr<FLinearColor>(Buf.GetData()) = C;
		}
		W->ProcessEvent(F, Buf.GetData());
	}
}

FString UZoogiAwakenWidget::CardsSignature(AZoogiGameMode* G) const
{
	const UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(G);
	const AZoogiBall* B = G->GetActiveBall();
	const FZoogiCardPlayerState* S = C && B ? C->Rules().Player(B->PlayerIndex) : nullptr;
	if (!S) return FString();
	FString Sig = FString::Printf(TEXT("cards|%p|%d|%d|%d|"), B, S->Energy, G->bStandbyAwaken ? 1 : 0, B->bCPU ? 1 : 0);
	for (const FZoogiCardInstance& I : S->Hand) Sig += FString::Printf(TEXT("%d%s,"), I.InstanceId, S->Selected.Contains(I.InstanceId) ? TEXT("*") : TEXT(""));
	return Sig;
}

void UZoogiAwakenWidget::BuildCards(AZoogiGameMode* G)
{
	UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(G);
	AZoogiBall* B = G->GetActiveBall();
	const FZoogiCardPlayerState* S = Cards && B ? Cards->Rules().Player(B->PlayerIndex) : nullptr;
	if (!S) return;
	bBuildingAim = false;
	CardVisuals.Reset();
	if (!CardVisualClass && !bCardVisualMissing)
	{
		CardVisualClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/RLDB/Widgets/WBP_CardVisual.WBP_CardVisual_C"));
		bCardVisualMissing = CardVisualClass == nullptr;
		UE_LOG(LogZoogi, Log, TEXT("CARDS hand ui: toolkit card widget %s"), CardVisualClass ? *CardVisualClass->GetPathName() : TEXT("MISSING (text cards)"));
	}
	const FZoogiCardContext Ctx = Cards->BuildContext(G);
	const int32 P = B->PlayerIndex;
	int32 SelCost = 0;
	TArray<const FZoogiCardDef*> SelDefs;
	for (int32 Id : S->Selected) for (const FZoogiCardInstance& I : S->Hand) if (I.InstanceId == Id) if (const FZoogiCardDef* D = Cards->Def(I.CardId)) { SelCost += D->Cost; SelDefs.Add(D); }
	const int32 N = S->Hand.Num();
	const float CW = 190.f, CH = 266.f, Gap = 14.f, SideW = 330.f;
	const float PW = FMath::Max(N, 3) * (CW + Gap) + 40.f + SideW, PH = CH + 150.f;

	AwakenPanel = MakePanel(FLinearColor(0.02f, 0.03f, 0.08f, 0.82f));
	AwakenPanel->SetVisibility(ESlateVisibility::Visible);
	AwakenPanel->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiAwakenWidget, OnPanelMouseDown));
	UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(AwakenPanel);
	RootSlot->SetAnchors(FAnchors(0.5f, 1.f));
	RootSlot->SetAlignment(FVector2D(0.5f, 1.f));
	RootSlot->SetPosition(FVector2D(0.f, -110.f));
	RootSlot->SetSize(FVector2D(PW, PH));
	UCanvasPanel* Cv = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	AwakenPanel->SetContent(Cv);
	PlaceText(Cv, FString::Printf(TEXT("STANDBY  -  P%d %s%s"), P + 1, *B->Def.Name.ToUpper(), B->bCPU ? TEXT("  (CPU deciding)") : TEXT("")), FVector2D(24.f, 14.f), 0.6f, FLinearColor(0.85f, 0.95f, 1.f), true);

	float X = 20.f;
	const float Y = 58.f;
	for (int32 i = 0; i < N; ++i)
	{
		const FZoogiCardInstance& I = S->Hand[i];
		const FZoogiCardDef* D = Cards->Def(I.CardId);
		const bool bSel = S->Selected.Contains(I.InstanceId);
		const EZoogiCardResult Why = bSel ? EZoogiCardResult::Ok : Cards->Rules().CanSelect(P, I.InstanceId, Ctx);
		const bool bOk = bSel || Why == EZoogiCardResult::Ok;
		UButton* Btn = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
		FButtonStyle St;
		const FLinearColor Edge = bSel ? FLinearColor(0.4f, 1.f, 0.55f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.f);
		St.SetNormal(CardSolid(FLinearColor(0, 0, 0, 0.01f), Edge, bSel ? 4.f : 0.f));
		St.SetHovered(CardSolid(FLinearColor(1, 1, 1, 0.05f), FLinearColor(1.f, 0.9f, 0.5f, 0.9f), 3.f));
		St.SetPressed(CardSolid(FLinearColor(1, 1, 1, 0.1f), FLinearColor::White, 3.f));
		St.SetNormalPadding(FMargin(4.f)); St.SetPressedPadding(FMargin(4.f, 6.f, 4.f, 2.f));
		Btn->SetStyle(St);
		UWidget* Content = nullptr;
		if (CardVisualClass)
		{
			UUserWidget* V = CreateWidget<UUserWidget>(this, CardVisualClass);
			if (V)
			{
				if (FBoolProperty* BP = FindFProperty<FBoolProperty>(V->GetClass(), TEXT("bBlockHoverEvents"))) BP->SetPropertyValue_InContainer(V, true);
				V->SetVisibility(ESlateVisibility::HitTestInvisible); // the button takes mouse / touch
				CardVisuals.Add(V);
				Content = V;
				PendingFill.Add({ V, I.CardId, bSel, bOk });
			}
		}
		if (!Content) Content = MakeText(D ? FString::Printf(TEXT("%s\n\n(%d)\n\n%s"), *D->Name, D->Cost, *D->Rule) : I.CardId.ToString(), 0.42f, FLinearColor::White, false);
		UScaleBox* Sb = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Sb->SetStretch(EStretch::ScaleToFit);
		Sb->AddChild(Content);
		if (UButtonSlot* BS = Cast<UButtonSlot>(Btn->AddChild(Sb))) { BS->SetHorizontalAlignment(HAlign_Fill); BS->SetVerticalAlignment(VAlign_Fill); }
		Btn->SetRenderOpacity(bOk ? 1.f : 0.45f);
		Btn->SetIsEnabled(!B->bCPU);
		CardPlace(Cv, Btn, FVector2D(X, Y), FVector2D(CW, CH));
		UZoogiAwakenClick* Ck = NewObject<UZoogiAwakenClick>(this);
		Ck->Owner = this; Ck->Action = TEXT("CardToggle"); Ck->Index = i;
		AwakenClicks.Add(Ck);
		Btn->OnClicked.AddDynamic(Ck, &UZoogiAwakenClick::Click);
		Btn->OnPressed.AddDynamic(Ck, &UZoogiAwakenClick::Press);
		RegisterControl(TEXT("CardToggle"), i, Btn);
		const FString Under = bSel ? FString(TEXT("SELECTED")) : !bOk ? FString(ZoogiCardResultName(Why)).Replace(TEXT("_"), TEXT(" ")).ToUpper() : FString::Printf(TEXT("%d  -  cost %d"), i + 1, D ? D->Cost : 0);
		PlaceText(Cv, Under, FVector2D(X + CW * 0.5f, Y + CH + 6.f), 0.42f, bSel ? FLinearColor(0.45f, 1.f, 0.6f) : bOk ? FLinearColor(0.75f, 0.8f, 0.9f) : FLinearColor(1.f, 0.5f, 0.4f), false, 0.5f);
		X += CW + Gap;
	}
	// right column: energy, committed cost, effects of the selection, controls
	const float RX = FMath::Max(N, 3) * (CW + Gap) + 30.f;
	const int32 Left = S->Energy - SelCost;
	PlaceText(Cv, FString::Printf(TEXT("CARD ENERGY  %d / %d"), S->Energy, Cards->Rules().Tune().EnergyPerTurn), FVector2D(RX, Y), 0.5f, FLinearColor(0.6f, 0.9f, 1.f), true);
	PlaceText(Cv, FString::Printf(TEXT("COMMITTED COST  %d    LEFT  %d"), SelCost, Left), FVector2D(RX, Y + 34.f), 0.5f, Left >= 0 ? FLinearColor(0.5f, 1.f, 0.6f) : FLinearColor(1.f, 0.45f, 0.35f), true);
	FString Fx = SelDefs.Num() ? TEXT("") : TEXT("no cards selected (CONFIRM = pass)");
	for (const FZoogiCardDef* D : SelDefs) Fx += FString::Printf(TEXT("%s%s (%d): %s"), Fx.IsEmpty() ? TEXT("") : TEXT("\n"), *D->Name, D->Cost, *D->Rule);
	UTextBlock* FxT = PlaceText(Cv, Fx, FVector2D(RX, Y + 72.f), 0.36f, FLinearColor(0.85f, 0.88f, 0.95f), false);
	if (FxT) { FxT->SetAutoWrapText(true); FxT->SetWrapTextAt(SideW - 20.f); }
	if (!B->bCPU)
	{
		const float BY = Y + CH - 60.f;
		MakeButton(Cv, G->bStandbyAwaken ? TEXT("AWAKEN ON") : TEXT("AWAKEN"), FVector2D(RX, BY - 66.f), FVector2D(150.f, 54.f), TEXT("CardAwaken"), 0, FLinearColor(0.75f, 0.55f, 1.f), G->bStandbyAwaken);
		MakeButton(Cv, TEXT("CLEAR"), FVector2D(RX + 160.f, BY - 66.f), FVector2D(140.f, 54.f), TEXT("CardClear"), 0, FLinearColor(1.f, 0.4f, 0.35f), false, S->Selected.Num() > 0);
		MakeButton(Cv, SelDefs.Num() ? FString::Printf(TEXT("CONFIRM  -%d"), SelCost) : FString(TEXT("CONFIRM (PASS)")), FVector2D(RX, BY), FVector2D(300.f, 60.f), TEXT("CardCommit"), 0, FLinearColor(0.35f, 1.f, 0.45f), true, Left >= 0, 0.35f);
		PlaceText(Cv, TEXT("keys: 1-5 card   A awaken   Backspace clear   Enter confirm"), FVector2D(24.f, PH - 30.f), 0.32f, FLinearColor(0.55f, 0.6f, 0.72f), false);
	}
	UE_LOG(LogZoogi, Log, TEXT("CARDS hand ui built P%d cards=%d toolkit_visuals=%d selected=%d committed_cost=%d left=%d"), P + 1, N, CardVisuals.Num(), SelDefs.Num(), SelCost, Left);
}

void UZoogiAwakenWidget::FillPendingCards()
{
	// The toolkit widget binds its child widgets in Initialize(): fill them the frame after creation.
	UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(this);
	for (const FPendingCardFill& F : PendingFill)
	{
		UUserWidget* V = F.Visual.Get();
		const FZoogiCardDef* D = Cards ? Cards->Def(F.CardId) : nullptr;
		if (!V || !D) continue;
		if (UTextBlock* T = BoundWidget<UTextBlock>(V, TEXT("CardName"))) T->SetText(FText::FromString(D->Name));
		if (UTextBlock* T = BoundWidget<UTextBlock>(V, TEXT("ManaCost"))) T->SetText(FText::AsNumber(D->Cost));
		if (UTextBlock* T = BoundWidget<UTextBlock>(V, TEXT("TypeText"))) T->SetText(FText::FromString(ZoogiCards::TypeName(D->Type)));
		if (URichTextBlock* T = BoundWidget<URichTextBlock>(V, TEXT("RichDescription"))) T->SetText(FText::FromString(D->Rule));
		CallGlow(V, F.bSelected, F.bSelected ? FLinearColor(0.4f, 1.f, 0.55f) : FLinearColor::White);
		++CardFills;
	}
	PendingFill.Reset();
}

bool UZoogiAwakenWidget::SynthTouch(const FVector2D& Abs)
{
	// Real Slate touch (finger 0): start + end at Abs, routed through the same hit-testing as a tap on a touch screen.
	if (!FSlateApplication::IsInitialized()) return false;
	FSlateApplication& S = FSlateApplication::Get();
	if (!S.IsActive()) S.ProcessApplicationActivationEvent(true);
	S.ProcessTouchStartedEvent(nullptr, FPointerEvent(0, 0, Abs, Abs, 1.f, true));
	S.ProcessTouchEndedEvent(FPointerEvent(0, 0, Abs, Abs, 0.f, true));
	LastSynthFrame = GFrameCounter;
	return true;
}

// ------------------------------------------------------------------ -ZoogiCardTest=hand_ui
// First Standby: the active Zoogi is made human for one turn. The test then drives the real UMG hand with real Slate
// input: a mouse click on card 1, a keyboard press of "2", a touch tap on card 1 again (deselect), checks the shown
// committed cost / energy each time, then taps CONFIRM and checks the commit spent exactly the shown cost.
#include "ZoogiHUD.h"
#include "GameFramework/PlayerController.h"

void UZoogiCardSubsystem::TestHandUiTick(AZoogiGameMode* G)
{
	AZoogiBall* B = G->GetActiveBall();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AZoogiHUD* Hud = PC ? Cast<AZoogiHUD>(PC->GetHUD()) : nullptr;
	UZoogiAwakenWidget* W = Hud ? Hud->GetAwakenWidget() : nullptr;
	if (!HandUiBall.IsValid() || !W) { if (++HandUiWait > 2000) { TestCheck(false, TEXT("hand_ui never started")); TestFinish(G); } return; }
	const int32 P = HandUiBall->PlayerIndex;
	const FZoogiCardPlayerState* S = RulesV.Player(P);
	auto Cost = [&]() { int32 C = 0; for (int32 Id : S->Selected) for (const FZoogiCardInstance& I : S->Hand) if (I.InstanceId == Id) if (const FZoogiCardDef* D = Def(I.CardId)) C += D->Cost; return C; };
	auto Tap = [&](FName A, int32 i, int32 Mode) -> bool
	{
		FVector2D Abs;
		if (!W->FindControl(A, i, Abs)) { TestCheck(false, FString::Printf(TEXT("control %s %d present"), *A.ToString(), i)); return false; }
		return Mode == 0 ? W->SynthClick(Abs) : W->SynthTouch(Abs);
	};
	auto Key = [&](FKey K)
	{
		FSlateApplication& Sa = FSlateApplication::Get();
		Sa.ProcessKeyDownEvent(FKeyEvent(K, FModifierKeysState(), 0, false, 0, 0));
		Sa.ProcessKeyUpEvent(FKeyEvent(K, FModifierKeysState(), 0, false, 0, 0));
	};
	if (HandUiStep == 0 && !W->ShowsCards() && ++HandUiShowWait < 900) return; // wait for Aiming / intro to finish
	if (HandUiStep == 0 && HandUiShowWait > 0) { UE_LOG(LogZoogi, Log, TEXT("CARDS hand_ui: panel wait %d frames phase=%d stage=%d done=%d"), HandUiShowWait, (int32)G->Phase, (int32)G->Stage, G->bAwakenDone ? 1 : 0); HandUiShowWait = -1000000; }
	if (++HandUiWait < 8) return; // let the panel build + the toolkit widgets fill
	HandUiWait = 0;
	if (TestCase == TEXT("keys") || TestCase == TEXT("dup_input"))
	{
		// v3 P6: keyboard-only Standby (1 / Backspace / A / 1 / Enter) and duplicate delivery of the same gameplay input
		// (same action twice in one frame, and a touch + mouse press on the same card in one frame).
		const bool bKeys = TestCase == TEXT("keys");
		auto Commits = [&]() { int32 N = 0; for (const FZoogiCardEvent& E : RulesV.Events()) if (E.Player == P && E.Kind == TEXT("commit") && E.Turn == S->Turn) ++N; return N; };
		switch (HandUiStep)
		{
		case 0:
			TestCheck(W->ShowsCards(), TEXT("UMG hand panel shown at human Standby"));
			HandUiEnergy = S->Energy;
			if (bKeys) Key(EKeys::One);
			else { G->MenuAction(TEXT("CardToggle"), 0); G->MenuAction(TEXT("CardToggle"), 0); }
			break;
		case 1:
			if (bKeys) { TestCheck(S->Selected.Num() == 1, TEXT("key 1 selected card 1")); Key(EKeys::BackSpace); }
			else
			{
				TestCheck(S->Selected.Num() == 1 && G->DuplicateInputsIgnored == 1, FString::Printf(TEXT("same toggle twice in one frame -> applied once (sel %d, ignored %d)"), S->Selected.Num(), G->DuplicateInputsIgnored));
				Tap(TEXT("CardToggle"), 1, 1); Tap(TEXT("CardToggle"), 1, 0); // touch + mouse on card 2 in the same frame
			}
			break;
		case 2:
			if (bKeys) { TestCheck(S->Selected.Num() == 0, TEXT("Backspace cleared the selection")); Key(EKeys::A); }
			else
			{
				const bool bOk = S->Selected.Num() == 2 || RulesV.CanSelect(P, S->Hand[1].InstanceId, BuildContext(G)) != EZoogiCardResult::Ok;
				TestCheck(bOk, FString::Printf(TEXT("touch + mouse on card 2 in one frame -> toggled once (sel %d, ignored %d)"), S->Selected.Num(), G->DuplicateInputsIgnored));
				HandUiCost = Cost();
				G->MenuAction(TEXT("CardCommit"), 0); G->MenuAction(TEXT("CardCommit"), 0);
			}
			break;
		case 3:
			if (bKeys)
			{
				UE_LOG(LogZoogi, Log, TEXT("CARDS keys: A -> awaken request %d (gauge permitting)"), G->bStandbyAwaken ? 1 : 0);
				Key(EKeys::One);
			}
			else
			{
				TestCheck(S->bCommitted && Commits() == 1 && S->Energy == HandUiEnergy - HandUiCost, FString::Printf(TEXT("double commit -> one commit, energy %d == %d - %d (commits %d)"), S->Energy, HandUiEnergy, HandUiCost, Commits()));
				TestCheck(G->DuplicateInputsIgnored >= 2, FString::Printf(TEXT("duplicates ignored %d"), G->DuplicateInputsIgnored));
				HandUiBall->bCPU = true;
				UE_LOG(LogZoogi, Log, TEXT("CARDS dup_input done: ignored=%d"), G->DuplicateInputsIgnored);
				TestFinish(G); return;
			}
			break;
		case 4:
			TestCheck(S->Selected.Num() == 1, TEXT("key 1 selected card 1 again"));
			HandUiCost = Cost(); Key(EKeys::Enter); break;
		case 5:
			TestCheck(S->bCommitted && S->Energy == HandUiEnergy - HandUiCost && Commits() == 1, FString::Printf(TEXT("Enter committed, energy %d == %d - %d"), S->Energy, HandUiEnergy, HandUiCost));
			TestCheck(!W->ShowsCards(), TEXT("hand panel closed after Enter"));
			HandUiBall->bCPU = true;
			UE_LOG(LogZoogi, Log, TEXT("CARDS keys done: keyboard-only Standby ok"));
			TestFinish(G); return;
		}
		++HandUiStep;
		return;
	}
	switch (HandUiStep)
	{
	case 0:
		TestCheck(W->ShowsCards(), TEXT("UMG hand panel shown at human Standby"));
		TestCheck(!W->UsesToolkitCards() || W->NumCardVisuals() == S->Hand.Num(), FString::Printf(TEXT("toolkit card visuals %d == hand %d (toolkit=%d)"), W->NumCardVisuals(), S->Hand.Num(), W->UsesToolkitCards() ? 1 : 0));
		TestCheck(W->UsesToolkitCards(), TEXT("WBP_CardVisual loaded"));
		TestCheck(W->NumCardFills() >= S->Hand.Num(), FString::Printf(TEXT("toolkit visuals filled %d"), W->NumCardFills()));
		Tap(TEXT("CardToggle"), 0, 0); break;
	case 1:
		TestCheck(S->Selected.Num() == 1 && S->Selected[0] == S->Hand[0].InstanceId, TEXT("mouse click selected card 1"));
		Key(EKeys::Two); break;
	case 2:
		TestCheck(S->Selected.Num() == 2 || (S->Selected.Num() == 1 && RulesV.CanSelect(P, S->Hand[1].InstanceId, BuildContext(G)) != EZoogiCardResult::Ok), FString::Printf(TEXT("keyboard 2 toggled card 2 (sel %d)"), S->Selected.Num()));
		Tap(TEXT("CardToggle"), 0, 1); break;
	case 3:
		TestCheck(!S->Selected.Contains(S->Hand[0].InstanceId), TEXT("touch tap deselected card 1"));
		if (S->Selected.Num() == 0) Tap(TEXT("CardToggle"), 0, 1); // keep one card to commit (touch select)
		break;
	case 4:
		HandUiCost = Cost(); HandUiEnergy = S->Energy;
		TestCheck(HandUiCost > 0, FString::Printf(TEXT("a card selected before confirm (cost %d)"), HandUiCost));
		Tap(TEXT("CardCommit"), 0, 1); break;
	case 5:
		TestCheck(S->bCommitted && S->Energy == HandUiEnergy - HandUiCost, FString::Printf(TEXT("confirm committed, energy %d == %d - %d"), S->Energy, HandUiEnergy, HandUiCost));
		TestCheck(!W->ShowsCards(), TEXT("hand panel closed after confirm"));
		HandUiBall->bCPU = true;
		UE_LOG(LogZoogi, Log, TEXT("CARDS hand_ui done: mouse/keyboard/touch ok, P%d back to CPU"), P + 1);
		TestFinish(G); return;
	}
	++HandUiStep;
}
