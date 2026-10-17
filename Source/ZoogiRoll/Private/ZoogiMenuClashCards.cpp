// clash-menu DETAILED PASS 3 + 4 (Oct 17), portrait: Cards tab.
// Decks: 5 deck tabs, editable deck name (edit + save), 8 slots (4x2), average-cost + count pills; tap a deck card =
// Details / Remove (instant, toast). Collection: Cards / Emotes; "Found X/Y"; sort direction arrow + mode button
// (By Rarity / By Elixir (card cost) / By Name / By Level); count bars green (can upgrade, up arrow) or blue (x/y);
// tap = UPGRADE [gold] + USE or DETAILS + USE; USE drops the card into the next empty slot of the deck and jumps to Decks.
// Details: art, level, count bar, rarity / type / element, 6 stat tiles from the REAL card data (Cards.csv), UPGRADE + USE.
// Level Up: sunburst, card, "Level N -> N+1", count bar, tiles, player XP. Emotes: 4 equipped slots + available grid (saved).
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiCards.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiProgression.h"
#include "ZoogiProgressionSubsystem.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

namespace
{
	FString ZKNum(int32 V) { return FText::AsNumber(V).ToString(); }
	FString ZKCardName(FName Id) { const FZoogiCardDef* C = UZoogiFrontProfileSubsystem::FindCard(Id); return C ? C->Name.ToUpper() : Id.ToString().ToUpper(); }
	int32 ZKRarity(FName Id) { const FZoogiCardDef* C = UZoogiFrontProfileSubsystem::FindCard(Id); return C ? UZoogiFrontProfileSubsystem::RarityOf(*C) : 0; }
	const TCHAR* ZKSortName(int32 M) { static const TCHAR* N[] = { TEXT("By Rarity"), TEXT("By Elixir"), TEXT("By Name"), TEXT("By Level") }; return N[FMath::Clamp(M, 0, 3)]; }

	// ---- the 6 Details tiles, mapped to REAL values from Content/Data/Cards.csv (cost / param_a / param_b / type / target / expiry).
	// param_a / param_b labels follow each card's rule text in the card book.
	struct FZKStat { FString Label; FString Value; const TCHAR* Icon; };
	FString ZKFmt(float V, TCHAR Kind)
	{
		if (FMath::IsNearlyZero(V)) return TEXT("-");
		FString N = FMath::IsNearlyEqual(V, FMath::RoundToFloat(V)) ? FString::FromInt(FMath::RoundToInt(V)) : FString::SanitizeFloat(FMath::RoundToFloat(V * 100.f) / 100.f);
		switch (Kind)
		{
		case 'x': return TEXT("x") + N;
		case '%': return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f));
		case 's': return N + TEXT(" s");
		case 'd': return N + TEXT(" diam.");
		default: return N;
		}
	}
	TArray<FZKStat> ZKStats(const FZoogiCardDef& C)
	{
		struct FMap { const TCHAR* Id; const TCHAR* LA; TCHAR KA; const TCHAR* LB; TCHAR KB; };
		static const FMap M[] = {
			{ TEXT("storm"), TEXT("Roll Control"), 'x', nullptr, 'n' }, { TEXT("bank_strike"), nullptr, 'n', nullptr, 'n' },
			{ TEXT("guard"), TEXT("Absorb"), 'n', TEXT("Impulse Taken"), 'x' }, { TEXT("orb_relay"), TEXT("Combo Needed"), 'n', nullptr, 'n' },
			{ TEXT("heavy_roll"), TEXT("Force"), 'x', TEXT("Aim Error"), 'x' }, { TEXT("ember"), TEXT("Enemy Push"), 'x', nullptr, 'n' },
			{ TEXT("titan"), TEXT("Mass"), 'x', TEXT("Push Taken"), 'x' }, { TEXT("frost"), TEXT("Lane Friction"), 'n', nullptr, 'n' },
			{ TEXT("precision"), TEXT("Aim Error"), 'x', TEXT("Rebounds Shown"), 'n' }, { TEXT("momentum_steal"), TEXT("Max Transfer"), '%', TEXT("Damage"), 'x' },
			{ TEXT("crystal_collapse"), TEXT("States per Hit"), 'n', nullptr, 'n' }, { TEXT("orb_magnet"), TEXT("Pull Time"), 's', TEXT("Pull Range"), 'd' },
			{ TEXT("double_impact"), TEXT("2nd Burst After"), 's', TEXT("2nd Burst Power"), '%' }, { TEXT("anchor_guard"), TEXT("Absorb"), 'n', TEXT("Next Push"), 'x' },
			{ TEXT("recovery_roll"), TEXT("Deceleration"), 'x', nullptr, 'n' } };
		const FMap* Hit = nullptr;
		for (const FMap& X : M) if (C.Id == FName(X.Id)) Hit = &X;
		TArray<FZKStat> Out;
		Out.Add({ TEXT("Energy Cost"), FString::FromInt(C.Cost), TEXT("drop") });
		Out.Add({ Hit && Hit->LA ? FString(Hit->LA) : FString(TEXT("Effect")), Hit && Hit->LA ? ZKFmt(C.ParamA, Hit->KA) : ZKFmt(C.ParamA, 'n'), TEXT("star") });
		Out.Add({ Hit && Hit->LB ? FString(Hit->LB) : FString(TEXT("Effect 2")), Hit && Hit->LB ? ZKFmt(C.ParamB, Hit->KB) : ZKFmt(C.ParamB, 'n'), TEXT("star") });
		Out.Add({ TEXT("Type"), ZoogiCards::TypeName(C.Type), TEXT("cards") });
		Out.Add({ TEXT("Target"), ZoogiCards::TargetName(C.Target), TEXT("swords") });
		Out.Add({ TEXT("Lasts Until"), ZoogiCards::ExpiryName(C.Expiry), TEXT("level") });
		return Out;
	}
}

// ------------------------------------------------------------------------------------------------ small helpers
void UZoogiMenuRoot::ClashToast(const FString& Msg)
{
	ClashToastMsg = Msg; ClashToastT = Time;
	UE_LOG(LogZoogi, Log, TEXT("CLASH toast: %s"), *Msg);
	bDirty = true;
}

UWidget* UZoogiMenuRoot::CField(const FString& Text, const FString& Hint, float X, float Y, float Wd, float Ht, TWeakObjectPtr<UEditableTextBox>* Out, bool bMulti)
{
	// dark navy input field (~#1B2A4A), white text (DETAILED PASS 3 style)
	const FSlateBrush Field = ZoogiUIStyle::RoundBrush(ZCHex(TEXT("1B2A4A")), ZCDark, 4.f, 14.f);
	if (bMulti)
	{
		UMultiLineEditableTextBox* E = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass());
		E->SetText(FText::FromString(Text));
		E->SetHintText(FText::FromString(Hint));
		FEditableTextBoxStyle St = E->WidgetStyle;
		St.SetFont(ZCFont(24.f, 0.f));
		St.SetForegroundColor(FSlateColor(FLinearColor::White));
		St.SetBackgroundImageNormal(Field); St.SetBackgroundImageHovered(Field); St.SetBackgroundImageFocused(Field); St.SetBackgroundImageReadOnly(Field);
		St.SetPadding(FMargin(18.f, 12.f));
		E->WidgetStyle = St;
		ClashDescBox = E;
		return CAt(E, X, Y, Wd, Ht);
	}
	UEditableTextBox* E = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	E->SetText(FText::FromString(Text));
	E->SetHintText(FText::FromString(Hint));
	FEditableTextBoxStyle St = E->WidgetStyle;
	St.SetFont(ZCFont(28.f, 0.f));
	St.SetForegroundColor(FSlateColor(FLinearColor::White));
	St.SetBackgroundImageNormal(Field); St.SetBackgroundImageHovered(Field); St.SetBackgroundImageFocused(Field); St.SetBackgroundImageReadOnly(Field);
	St.SetPadding(FMargin(18.f, 10.f));
	E->WidgetStyle = St;
	if (Out) *Out = E;
	return CAt(E, X, Y, Wd, Ht);
}

void UZoogiMenuRoot::ClashUseCard(FName Id)
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return;
	const int32 View = ClashDeckView < 0 ? FP->Data().ActiveDeck : FMath::Clamp(ClashDeckView, 0, UZoogiFrontProfileSubsystem::NumDecks - 1);
	TArray<FName> Dk = FP->Deck(View);
	ClashPopoverId = NAME_None; ClashPopoverSlot = -1;
	ClashPopup = EZoogiClashPopup::None;
	ClashCardsSub = 0; ClashScrollKey = -1;
	if (!FP->IsCardOwned(Id)) { ClashToast(TEXT("Unlock this card first.")); return; }
	if (Dk.Contains(Id)) { ClashToast(FString::Printf(TEXT("%s is already in %s."), *ZKCardName(Id), *FP->DeckName(View))); return; }
	if (Dk.Num() >= UZoogiFrontProfileSubsystem::DeckSize) { ClashToast(TEXT("Deck is full: remove a card first.")); return; }
	Dk.Add(Id);
	FP->SetDeck(View, Dk);
	UE_LOG(LogZoogi, Log, TEXT("CLASH use %s -> deck %d slot %d"), *Id.ToString(), View + 1, Dk.Num());
	ClashToast(FString::Printf(TEXT("Added to %s."), *FP->DeckName(View)));
}

// ------------------------------------------------------------------------------------------------ Cards tab
void UZoogiMenuRoot::BuildClashCardsTab()
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return;
	const float W = DesignW, C = W * 0.5f, NavY = ClashNavY();
	// Decks / Collection toggle
	const TCHAR* Subs[] = { TEXT("DECKS"), TEXT("COLLECTION") };
	for (int32 k = 0; k < 2; ++k)
	{
		const float BW = 420.f, BX = C - BW - 12.f + k * (BW + 24.f);
		CChunky(Subs[k], ClashCardsSub == k ? ZCYellow : ZCBlueDeep, [this, k]()
		{
			if (ClashCardsSub != k) { ClashCardsSub = k; ClashScrollOffset = 0.f; ClashPendingScroll = -1.f; ClashScrollKey = -1; UE_LOG(LogZoogi, Log, TEXT("CLASH cards sub %d"), k); }
			ClashPopoverId = NAME_None; ClashPopoverSlot = -1; bClashDeckNameEdit = false;
			bDirty = true;
		}, BX, 112.f, BW, 84.f, 32.f);
	}
	const TArray<FZoogiCardDef>& Defs = UZoogiFrontProfileSubsystem::CardDefs();
	auto Popover = [this](float X, float Y, float Wd, const FString& A, const FLinearColor& AC, TFunction<void()> FA, UWidget* AInner, const FString& B, TFunction<void()> FB)
	{
		// two stacked buttons right under the tapped card (Clash-style), tap anywhere else closes
		const float BH = 74.f;
		CPlate(X - 8.f, Y - 8.f, Wd + 16.f, BH * 2.f + 26.f, ZCNavyDeep.CopyWithNewOpacity(0.95f), 18.f, 4.f, false);
		if (AInner) CChunkyW(AInner, AC, MoveTemp(FA), X, Y, Wd, BH);
		else CChunky(A, AC, MoveTemp(FA), X, Y, Wd, BH, 26.f);
		CChunky(B, ZCGreen, MoveTemp(FB), X, Y + BH + 10.f, Wd, BH, 28.f);
	};

	if (ClashCardsSub == 0)
	{
		// ================================================================ DECKS
		const int32 Active = FP->Data().ActiveDeck;
		const int32 View = ClashDeckView < 0 ? Active : FMath::Clamp(ClashDeckView, 0, UZoogiFrontProfileSubsystem::NumDecks - 1);
		const TArray<FName> Deck = FP->Deck(View);
		const float LX = 24.f, LY = 222.f, LW = W - 48.f, LH = 880.f;
		CGoldPanel(LX, LY, LW, LH, ZCPanel, 26.f);
		for (int32 d = 0; d < UZoogiFrontProfileSubsystem::NumDecks; ++d)
			CChunky(FString::FromInt(d + 1), d == View ? ZCYellow : ZCBlueDeep, [this, d]() { ClashDeckView = d; bClashDeckNameEdit = false; ClashPopoverSlot = -1; bDirty = true; },
				LX + 24.f + d * 118.f, LY + 24.f, 106.f, 84.f, 36.f);
		if (View == Active) { CPlate(LX + LW - 24.f - 270.f, LY + 32.f, 270.f, 68.f, ZCHex(TEXT("2E9A3C")), 16.f, 4.f, true); CText(TEXT("IN USE"), LX + LW - 24.f - 270.f, LY + 32.f, 270.f, 68.f, 28.f, FLinearColor::White); }
		else CChunky(TEXT("USE THIS DECK"), ZCGreen, [this, View]() { if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this)) { F->SetActiveDeck(View); UE_LOG(LogZoogi, Log, TEXT("CLASH active deck %d"), View + 1); ClashToast(FString::Printf(TEXT("%s is now in use."), *F->DeckName(View))); } bDirty = true; },
			LX + LW - 24.f - 270.f, LY + 24.f, 270.f, 84.f, 26.f);
		// deck name: field + edit (pencil) / save (green check)
		const float NY = LY + 128.f, NW = 470.f;
		if (bClashDeckNameEdit)
		{
			CField(ClashDeckNameDraft, FString::Printf(TEXT("Deck %d"), View + 1), LX + 24.f, NY, NW, 76.f, &ClashDeckNameBox);
			UImage* Ok = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Ok->SetBrush(ZCImgBrush(ClashArt(TEXT("check")), 48.f, 48.f, 0.f, false));
			Ok->SetVisibility(ESlateVisibility::HitTestInvisible);
			CChunkyW(Ok, ZCGreen, [this, View]()
			{
				UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
				const FString Nm = ClashDeckNameBox.IsValid() ? ClashDeckNameBox->GetText().ToString() : ClashDeckNameDraft;
				if (F) { F->SetDeckName(View, Nm); ClashToast(FString::Printf(TEXT("Saved as \"%s\"."), *F->DeckName(View))); }
				bClashDeckNameEdit = false; bDirty = true;
			}, LX + 24.f + NW + 14.f, NY - 2.f, 90.f, 80.f);
		}
		else
		{
			CAt(CBox(ZCHex(TEXT("1B2A4A")), ZCDark, 4.f, 14.f), LX + 24.f, NY, NW, 76.f);
			CText(FP->DeckName(View), LX + 44.f, NY, NW - 40.f, 76.f, 30.f, FLinearColor::White, 0, 3.f);
			UImage* Pen = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Pen->SetBrush(ZCImgBrush(ClashArt(TEXT("pencil")), 46.f, 46.f, 0.f, false));
			Pen->SetVisibility(ESlateVisibility::HitTestInvisible);
			CChunkyW(Pen, ZCBlue, [this, View]()
			{
				if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this)) ClashDeckNameDraft = F->DeckName(View);
				bClashDeckNameEdit = true; bDirty = true;
			}, LX + 24.f + NW + 14.f, NY - 2.f, 90.f, 80.f);
		}
		// average cost (magenta drop) + count pills
		{
			float Sum = 0.f; for (const FName& Id : Deck) if (const FZoogiCardDef* Df = UZoogiFrontProfileSubsystem::FindCard(Id)) Sum += Df->Cost;
			const float PX = LX + LW - 24.f - 360.f;
			CPlate(PX + 20.f, NY + 8.f, 160.f, 60.f, ZCNavyDeep.CopyWithNewOpacity(0.9f), 30.f, 4.f, false);
			CImg(ClashArt(TEXT("drop")), PX, NY - 2.f, 76.f, 76.f);
			CText(Deck.Num() ? FString::Printf(TEXT("%.1f"), Sum / Deck.Num()) : FString(TEXT("0.0")), PX + 72.f, NY + 8.f, 100.f, 60.f, 30.f, FLinearColor::White);
			const bool bFull = Deck.Num() == UZoogiFrontProfileSubsystem::DeckSize;
			CPlate(PX + 200.f, NY + 8.f, 160.f, 60.f, bFull ? ZCHex(TEXT("2E9A3C")) : ZCNavyDeep.CopyWithNewOpacity(0.9f), 30.f, 4.f, false);
			CText(FString::Printf(TEXT("%d/%d"), Deck.Num(), UZoogiFrontProfileSubsystem::DeckSize), PX + 200.f, NY + 8.f, 160.f, 60.f, 30.f, bFull ? FLinearColor::White : ZCGold);
		}
		// 8 slots, 4 x 2
		const float Gap = 20.f, GX = LX + 24.f, GY = LY + 236.f, SW = (LW - 48.f - Gap * 3.f) / 4.f, SH = (LH - 236.f - 30.f - Gap) * 0.5f;
		const float CW = FMath::Min(SW, SH / 1.25f), CH = CW * 1.25f;
		for (int32 s = 0; s < UZoogiFrontProfileSubsystem::DeckSize; ++s)
		{
			const float X = GX + (s % 4) * (SW + Gap) + (SW - CW) * 0.5f, Y = GY + (s / 4) * (SH + Gap) + (SH - CH) * 0.5f;
			if (s == ClashPopoverSlot && ClashPopoverW <= 0.f) { ClashPopoverPos = FVector2D(X, Y + CH); ClashPopoverW = CW; } // (tour: popover opened without a tap)
			if (Deck.IsValidIndex(s))
			{
				const FName Id = Deck[s];
				UCanvasPanel* T = CGroupBegin();
				CCard(Id, 0.f, 0.f, CW, CH, !FP->IsCardOwned(Id), FString(), true);
				CGroupEnd();
				CAt(CButton(T, [this, s, X, Y, CW, CH]() { ClashPopoverSlot = ClashPopoverSlot == s ? -1 : s; ClashPopoverPos = FVector2D(X, Y + CH); ClashPopoverW = CW; bDirty = true; }), X, Y, CW, CH);
			}
			else
			{
				CPlate(X, Y, CW, CH, ZCNavyDeep.CopyWithNewOpacity(0.7f), 16.f, 3.f, false);
				CText(TEXT("+"), X, Y, CW, CH, 64.f, ZCMuted, 1, 3.f);
			}
		}
		CText(Deck.Num() < UZoogiFrontProfileSubsystem::DeckSize ? TEXT("Add cards from COLLECTION: tap a card, then USE.") : TEXT("Tap a card in the deck for Details or Remove."),
			LX, LY + LH + 20.f, LW, 40.f, 24.f, FLinearColor::White, 1, 3.f);
		CChunky(TEXT("ADD CARDS"), ZCYellow, [this]() { ClashCardsSub = 1; ClashCollSub = 0; ClashScrollKey = -1; ClashPopoverSlot = -1; bDirty = true; }, C - 200.f, LY + LH + 76.f, 400.f, 100.f, 36.f, ClashArt(TEXT("cards")), true);
		CText(TEXT("Battles use 15 cards: a short deck is topped up from your collection when the match starts."), LX + 40.f, LY + LH + 200.f, LW - 80.f, 60.f, 19.f, ZCSoft, 1, 2.f, true);
		// deck card popover: Details / Remove
		if (Deck.IsValidIndex(ClashPopoverSlot))
		{
			const FName Id = Deck[ClashPopoverSlot];
			const int32 SlotIdx = ClashPopoverSlot;
			if (UButton* Catch = CButton(CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f), [this]() { ClashPopoverSlot = -1; bDirty = true; })) { Catch->SetStyle(ZCClearStyle()); CAt(Catch, 0.f, 0.f, W, NavY); }
			const float PWd = FMath::Max(ClashPopoverW, 220.f), PX = FMath::Clamp(ClashPopoverPos.X + ClashPopoverW * 0.5f - PWd * 0.5f, 24.f, W - 24.f - PWd);
			const float PY = FMath::Min(ClashPopoverPos.Y + 10.f, NavY - 190.f);
			Popover(PX, PY, PWd, TEXT("DETAILS"), ZCBlue, [this, Id]() { ClashSelId = Id; ClashPopoverSlot = -1; ClashDetailsBack = EZoogiClashPopup::None; OpenClashPopup(EZoogiClashPopup::CardDetails, ZKCardName(Id)); }, nullptr,
				TEXT("REMOVE"), [this, View, SlotIdx]()
				{
					if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this))
					{
						TArray<FName> Dk = F->Deck(View);
						if (Dk.IsValidIndex(SlotIdx)) { UE_LOG(LogZoogi, Log, TEXT("CLASH deck %d remove %s"), View + 1, *Dk[SlotIdx].ToString()); Dk.RemoveAt(SlotIdx); F->SetDeck(View, Dk); }
					}
					ClashPopoverSlot = -1;
					ClashToast(TEXT("Card removed from deck."));
				});
		}
		return;
	}

	// ================================================================ COLLECTION
	const TCHAR* Sub2[] = { TEXT("CARDS"), TEXT("EMOTES") };
	for (int32 k = 0; k < 2; ++k)
		CChunky(Sub2[k], ClashCollSub == k ? ZCBlue : ZCNavyBtn, [this, k]() { if (ClashCollSub != k) { ClashCollSub = k; ClashScrollKey = -1; ClashScrollOffset = 0.f; ClashPendingScroll = -1.f; } ClashPopoverId = NAME_None; bDirty = true; },
			C - 260.f + k * 270.f, 214.f, 250.f, 70.f, 26.f);
	if (ClashCollSub == 0)
	{
		TArray<FName> Owned, Locked;
		for (const FZoogiCardDef& Df : Defs) (FP->IsCardOwned(Df.Id) ? Owned : Locked).Add(Df.Id);
		// sort: mode + direction (saved in the profile)
		const int32 Mode = FP->Data().CollectionSort; const bool bDesc = FP->Data().bSortDesc;
		auto Key = [FP, Mode](FName Id) -> int32
		{
			const FZoogiCardDef* Df = UZoogiFrontProfileSubsystem::FindCard(Id);
			return Mode == 0 ? (Df ? UZoogiFrontProfileSubsystem::RarityOf(*Df) : 0) : Mode == 1 ? (Df ? Df->Cost : 0) : Mode == 3 ? FP->CardLevelOf(Id) : 0;
		};
		auto Sorter = [Mode, bDesc, Key](const FName& A, const FName& B)
		{
			if (Mode == 2) { const FString NA = ZKCardName(A), NB = ZKCardName(B); return bDesc ? NB < NA : NA < NB; }
			const int32 KA = Key(A), KB = Key(B);
			if (KA != KB) return bDesc ? KA > KB : KA < KB;
			return ZKCardName(A) < ZKCardName(B);
		};
		Owned.Sort(Sorter); Locked.Sort(Sorter);
		CText(FString::Printf(TEXT("Card Collection - Found %d/%d"), Owned.Num(), Defs.Num()), 24.f, 300.f, 620.f, 56.f, 28.f, FLinearColor::White, 0);
		{
			UImage* Ar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Ar->SetBrush(ZCImgBrush(ClashArt(TEXT("arrow_down")), 40.f, 40.f, 0.f, false));
			Ar->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Ar->SetRenderTransformAngle(bDesc ? 0.f : 180.f);
			Ar->SetVisibility(ESlateVisibility::HitTestInvisible);
			CChunkyW(Ar, ZCNavyBtn, [this]() { if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this)) { F->Mutable().bSortDesc = !F->Data().bSortDesc; F->Save(); } bDirty = true; }, W - 24.f - 250.f - 90.f - 12.f, 296.f, 90.f, 70.f);
			CChunky(ZKSortName(Mode), ZCBlue, [this]() { if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this)) { F->Mutable().CollectionSort = (F->Data().CollectionSort + 1) % 4; F->Save(); UE_LOG(LogZoogi, Log, TEXT("CLASH sort %s"), ZKSortName(F->Data().CollectionSort)); } bDirty = true; },
				W - 24.f - 250.f, 296.f, 250.f, 70.f, 26.f);
		}
		const float TopY = 384.f, ViewH = NavY - TopY;
		if (UScrollBox* Old = ClashScrollW.Get()) if (ClashScrollKey == 1 && ClashPendingScroll < 0.f) ClashScrollOffset = Old->GetScrollOffset();
		ClashScrollKey = 1;
		UCanvasPanel* G = CGroupBegin();
		const float CG = 20.f, CardW = (W - 48.f - CG * 3.f) / 4.f, CardH = CardW * 1.25f, CellH = CardH + 92.f;
		const int32 Cols = 4;
		const float X0 = 24.f;
		float Y = 10.f;
		float PopX = -1.f, PopY = 0.f; FName PopId;
		auto Grid = [&](const TArray<FName>& Ids, bool bLocked)
		{
			for (int32 i = 0; i < Ids.Num(); ++i)
			{
				const FName Id = Ids[i];
				const float X = X0 + (i % Cols) * (CardW + CG), YY = Y + (i / Cols) * (CellH + CG);
				UCanvasPanel* T = CGroupBegin();
				CCard(Id, 0.f, 0.f, CardW, CardH, bLocked);
				if (!bLocked)
				{
					const int32 Lv = FP->CardLevelOf(Id), Sp = FP->Spare(Id), Need = UZoogiFrontProfileSubsystem::UpgradeCopies(Lv);
					const bool bMax = Lv >= UZoogiFrontProfileSubsystem::MaxCardLevel, bUp = FP->CanUpgrade(Id);
					CText(bMax ? FString(TEXT("Max Level")) : FString::Printf(TEXT("Level %d"), Lv), 0.f, CardH + 4.f, CardW, 38.f, 24.f, ZCGold, 1, 3.f);
					CBar(0.f, CardH + 46.f, CardW, 40.f, bMax ? 1.f : FMath::Min(1.f, float(Sp) / FMath::Max(1, Need)), bUp ? ZCGreen : ZCSfxBlue, bMax ? FString(TEXT("MAX")) : FString::Printf(TEXT("%d/%d"), Sp, Need), 20.f);
					if (bUp) CImg(ClashArt(TEXT("arrow_up")), -10.f, CardH + 38.f, 54.f, 54.f);
				}
				else CText(TEXT("LOCKED"), 0.f, CardH + 10.f, CardW, 40.f, 22.f, ZCMuted, 1, 3.f);
				CGroupEnd();
				CAt(CButton(T, [this, Id, bLocked]()
				{
					if (bLocked) { ClashSelId = Id; ClashDetailsBack = EZoogiClashPopup::None; OpenClashPopup(EZoogiClashPopup::CardDetails, ZKCardName(Id)); return; }
					ClashPopoverId = ClashPopoverId == Id ? NAME_None : Id; bDirty = true;
				}), X, YY, CardW, CellH);
				if (!bLocked && Id == ClashPopoverId) { PopX = X; PopY = YY + CellH; PopId = Id; }
			}
			Y += FMath::DivideAndRoundUp(Ids.Num(), Cols) * (CellH + CG);
		};
		Grid(Owned, false);
		Y += 30.f;
		ClashLockedY = Y - 10.f;
		CSection(TEXT("LOCKED CARDS"), Y);
		Y += 120.f;
		if (Locked.Num() == 0) { CText(TEXT("Every card is unlocked!"), C - 300.f, Y, 600.f, 50.f, 26.f, ZCSoft, 1, 3.f); Y += 60.f; }
		else Grid(Locked, true);
		Y += 200.f; // room for a popover under the last row
		if (PopX >= 0.f)
		{
			if (UButton* Catch = CButton(CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f), [this]() { ClashPopoverId = NAME_None; bDirty = true; })) { Catch->SetStyle(ZCClearStyle()); CAt(Catch, 0.f, 0.f, W, Y); }
			const float PWd = FMath::Max(CardW + 20.f, 250.f), PX = FMath::Clamp(PopX + CardW * 0.5f - PWd * 0.5f, 16.f, W - 16.f - PWd);
			const int32 Lv = FP->CardLevelOf(PopId);
			if (FP->CanUpgrade(PopId))
			{
				UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(TEXT("UPGRADE"), 22.f, FLinearColor::White, 4.f))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
				UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				I->SetBrush(ZCImgBrush(Art(TEXT("icon_coin")), 34.f, 34.f, 0.f, false));
				if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f)); }
				const int32 Cost = UZoogiFrontProfileSubsystem::UpgradeGold(Lv);
				if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(ZKNum(Cost), 24.f, FP->Data().Gold >= Cost ? FLinearColor::White : ZCPriceRed, 4.f))) S->SetVerticalAlignment(VAlign_Center);
				H->SetVisibility(ESlateVisibility::HitTestInvisible);
				Popover(PX, PopY, PWd, FString(), ZCYellow, [this, PopId]() { ClashSelId = PopId; ClashPopoverId = NAME_None; ClashDetailsBack = EZoogiClashPopup::None; OpenClashPopup(EZoogiClashPopup::CardDetails, ZKCardName(PopId)); }, H,
					TEXT("USE"), [this, PopId]() { ClashUseCard(PopId); });
			}
			else
				Popover(PX, PopY, PWd, TEXT("DETAILS"), ZCBlue, [this, PopId]() { ClashSelId = PopId; ClashPopoverId = NAME_None; ClashDetailsBack = EZoogiClashPopup::None; OpenClashPopup(EZoogiClashPopup::CardDetails, ZKCardName(PopId)); }, nullptr,
					TEXT("USE"), [this, PopId]() { ClashUseCard(PopId); });
		}
		CGroupEnd();
		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SB->SetWidthOverride(W); SB->SetHeightOverride(Y); SB->AddChild(G);
		UScrollBox* Sc = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Sc->SetAnimateWheelScrolling(true);
		Sc->SetScrollBarVisibility(ESlateVisibility::Visible);
		Sc->AddChild(SB);
		CAt(Sc, 0.f, TopY, W, ViewH);
		ClashScrollW = Sc;
		if (ClashPendingScroll < 0.f) ClashPendingScroll = ClashScrollOffset;
		Sc->SetScrollOffset(ClashPendingScroll);
		return;
	}

	// ================================================================ EMOTES: 4 equipped slots + available grid (DETAILED PASS 4)
	TArray<FName> Emotes;
	const UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
	if (PS)
		for (const FName& Id : PS->Core().Get().OwnedCosmetics)
		{
			bool bEmote = Id.ToString().StartsWith(TEXT("emote"));
			for (const FZoogiCosmeticRow& R : PS->Core().Config().Cosmetics) if (R.Id == Id && R.Slot == FName(TEXT("emote"))) bEmote = true;
			if (bEmote) Emotes.Add(Id);
		}
	Emotes.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	auto EmoteName = [PS](FName Id)
	{
		FString Nm = Id.ToString();
		if (PS) for (const FZoogiCosmeticRow& R : PS->Core().Config().Cosmetics) if (R.Id == Id && !R.Name.IsEmpty()) Nm = R.Name;
		return Nm.ToUpper();
	};
	auto EmoteArt = [this](FName Id) -> UTexture2D*
	{
		FString Zoogi = Id.ToString(); Zoogi.RemoveFromStart(TEXT("emote_")); int32 Us = INDEX_NONE;
		if (Zoogi.FindLastChar(TEXT('_'), Us)) Zoogi = Zoogi.Left(Us);
		return Art(PortraitFor(FName(*Zoogi)));
	};
	TArray<FName> Eq = FP->Data().EquippedEmotes; Eq.SetNum(4);
	for (FName& E : Eq) if (!E.IsNone() && !Emotes.Contains(E)) E = NAME_None; // only emotes the profile really owns
	int32 NumEq = 0; for (const FName& E : Eq) NumEq += E.IsNone() ? 0 : 1;
	const float PX = 24.f, PY = 310.f, PWd = W - 48.f, PHt = 330.f;
	CGoldPanel(PX, PY, PWd, PHt, ZCPanel, 26.f);
	CText(FString::Printf(TEXT("EQUIPPED %d/4"), NumEq), PX + 24.f, PY + 16.f, 500.f, 50.f, 30.f, FLinearColor::White, 0);
	CText(TEXT("Tap to take off"), PX + PWd - 344.f, PY + 16.f, 320.f, 50.f, 22.f, ZCSoft, 2, 3.f);
	const float SS = (PWd - 48.f - 3.f * 20.f) / 4.f;
	for (int32 s = 0; s < 4; ++s)
	{
		const float X = PX + 24.f + s * (SS + 20.f), Y = PY + 80.f, SHt = PHt - 104.f;
		if (Eq[s].IsNone()) { CPlate(X, Y, SS, SHt, ZCNavyDeep.CopyWithNewOpacity(0.7f), 18.f, 3.f, false); CText(TEXT("+"), X, Y, SS, SHt, 60.f, ZCMuted, 1, 3.f); continue; }
		const FName Id = Eq[s];
		UCanvasPanel* T = CGroupBegin();
		CPlate(0.f, 0.f, SS, SHt, ZCNavy, 18.f, 4.f, true);
		CImg(EmoteArt(Id), SS * 0.5f - 64.f, 14.f, 128.f, 128.f, 64.f, true);
		CText(EmoteName(Id), 6.f, 150.f, SS - 12.f, SHt - 156.f, 18.f, FLinearColor::White, 1, 3.f, true);
		CGroupEnd();
		CAt(CButton(T, [this, s]()
		{
			if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this)) { TArray<FName> E2 = F->Data().EquippedEmotes; E2.SetNum(4); E2[s] = NAME_None; F->SetEquippedEmotes(E2); }
			ClashToast(TEXT("Emote taken off. Saved."));
		}), X, Y, SS, SHt);
	}
	if (Emotes.Num() == 0)
	{
		const float BY = PY + PHt + 30.f;
		CGoldPanel(PX, BY, PWd, 520.f, ZCPanel, 30.f);
		CImg(ClashArt(TEXT("emote")), C - 80.f, BY + 40.f, 160.f, 160.f);
		CText(TEXT("NO EMOTES YET"), PX, BY + 210.f, PWd, 60.f, 42.f, FLinearColor::White);
		CText(TEXT("Emotes are earned, not bought: level up a Zoogi's affinity by playing with it (affinity level 2 unlocks its wave emote). Earned emotes show up here to equip."),
			PX + 50.f, BY + 284.f, PWd - 100.f, 130.f, 23.f, ZCSoft, 1, 3.f, true);
		CText(TEXT("Sending emotes in battle needs the battle HUD hook (not part of the menu work)."), PX + 50.f, BY + 430.f, PWd - 100.f, 60.f, 19.f, ZCMuted, 1, 3.f, true);
		return;
	}
	CText(TEXT("AVAILABLE - tap to equip"), PX, PY + PHt + 24.f, PWd, 44.f, 26.f, FLinearColor::White, 0);
	const float TW = (PWd - 3.f * 20.f) / 4.f, TH = 250.f;
	for (int32 i = 0; i < Emotes.Num(); ++i)
	{
		const FName Id = Emotes[i];
		const bool bOn = Eq.Contains(Id);
		const float X = PX + (i % 4) * (TW + 20.f), Y = PY + PHt + 80.f + (i / 4) * (TH + 20.f);
		if (Y + TH > NavY - 10.f) break;
		UCanvasPanel* T = CGroupBegin();
		CPlate(0.f, 0.f, TW, TH, bOn ? ZCHex(TEXT("2E9A3C")) : ZCNavy, 18.f, 4.f, true);
		CImg(EmoteArt(Id), TW * 0.5f - 60.f, 16.f, 120.f, 120.f, 60.f, true);
		CText(EmoteName(Id), 6.f, 146.f, TW - 12.f, 70.f, 18.f, FLinearColor::White, 1, 3.f, true);
		if (bOn) CImg(ClashArt(TEXT("check")), TW - 52.f, 6.f, 46.f, 46.f);
		CGroupEnd();
		CAt(CButton(T, [this, Id]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			TArray<FName> E2 = F->Data().EquippedEmotes; E2.SetNum(4);
			const int32 At = E2.IndexOfByKey(Id);
			if (At != INDEX_NONE) { E2[At] = NAME_None; F->SetEquippedEmotes(E2); ClashToast(TEXT("Emote taken off. Saved.")); return; }
			const int32 Free = E2.IndexOfByKey(NAME_None);
			if (Free == INDEX_NONE) { ClashToast(TEXT("All 4 slots are full: tap an equipped emote to take it off.")); return; }
			E2[Free] = Id; F->SetEquippedEmotes(E2);
			ClashToast(TEXT("Emote equipped. Saved."));
		}), X, Y, TW, TH);
	}
}

// ------------------------------------------------------------------------------------------------ Level Up (full screen)
void UZoogiMenuRoot::BuildClashLevelUp()
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiCardDef* Df = UZoogiFrontProfileSubsystem::FindCard(ClashLvId);
	const float W = DesignW, C = W * 0.5f, OY = FMath::Max(0.f, (DesignH - 1600.f) * 0.5f);
	CAt(CBox(ZCHex(TEXT("08163A")).CopyWithNewOpacity(0.9f), FLinearColor::Transparent, 0.f, 0.f), 0.f, -400.f, W, DesignH + 800.f);
	auto Done = [this]()
	{
		const EZoogiClashPopup Back = ClashDetailsBack;
		ClashDetailsBack = EZoogiClashPopup::None;
		if (Back == EZoogiClashPopup::CardDetails) OpenClashPopup(EZoogiClashPopup::CardDetails, ZKCardName(ClashLvId));
		else { ClashPopup = EZoogiClashPopup::None; bDirty = true; }
	};
	if (UButton* Tap = CButton(CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f), Done)) { Tap->SetStyle(ZCClearStyle()); CAt(Tap, 0.f, -400.f, W, DesignH + 800.f); }
	if (!Df || !FP) return;
	UWidget* Ray = CImg(ClashArt(TEXT("rays")), C - 460.f, OY + 120.f, 920.f, 920.f, 0.f, false, ZCHex(TEXT("FFD54A")));
	if (Ray) { Ray->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); ClashRaysW = Ray; }
	CText(TEXT("Level Up!"), C - 400.f, OY + 90.f, 800.f, 110.f, 84.f, ZCGold, 1, 6.f);
	CCard(ClashLvId, C - 180.f, OY + 270.f, 360.f, 450.f, false);
	CText(Df->Name.ToUpper(), C - 450.f, OY + 740.f, 900.f, 70.f, 54.f, FLinearColor::White);
	CText(FString::Printf(TEXT("Level %d  >  Level %d"), ClashLvFrom, ClashLvFrom + 1), C - 400.f, OY + 812.f, 800.f, 56.f, 38.f, ZCGreen, 1, 4.f);
	const int32 Lv = FP->CardLevelOf(ClashLvId), Sp = FP->Spare(ClashLvId), Need = UZoogiFrontProfileSubsystem::UpgradeCopies(Lv);
	const bool bMax = Lv >= UZoogiFrontProfileSubsystem::MaxCardLevel;
	CBar(C - 260.f, OY + 880.f, 520.f, 50.f, bMax ? 1.f : FMath::Min(1.f, float(Sp) / FMath::Max(1, Need)), FP->CanUpgrade(ClashLvId) ? ZCGreen : ZCSfxBlue, bMax ? FString(TEXT("MAX LEVEL")) : FString::Printf(TEXT("%d/%d"), Sp, Need), 24.f);
	// stat tiles: the card's real values; levels do not change battle values (battle side untouched)
	const TArray<FZKStat> St = ZKStats(*Df);
	const float TW = (W - 48.f - 20.f) * 0.5f, TH = 96.f;
	for (int32 i = 0; i < 2; ++i)
	{
		const float X = 24.f + i * (TW + 20.f), Y = OY + 960.f;
		CPlate(X, Y, TW, TH, ZCNavyDeep.CopyWithNewOpacity(0.9f), 16.f, 4.f, false);
		CImg(ClashArt(St[i].Icon), X + 12.f, Y + 18.f, 60.f, 60.f);
		CText(St[i].Label, X + 84.f, Y + 6.f, TW - 100.f, 40.f, 22.f, ZCSoft, 0, 3.f);
		CText(St[i].Value, X + 84.f, Y + 44.f, TW - 260.f, 46.f, 32.f, FLinearColor::White, 0, 3.f);
		CText(TEXT("+0"), X + TW - 170.f, Y + 44.f, 150.f, 46.f, 28.f, ZCMuted, 2, 3.f);
	}
	CText(TEXT("Card levels don't change battle values yet: the match reads Cards.csv as is."), 40.f, OY + 1070.f, W - 80.f, 40.f, 19.f, ZCSoft, 1, 2.f, true);
	CPlate(C - 330.f, OY + 1130.f, 660.f, 120.f, ZCNavyDeep.CopyWithNewOpacity(0.9f), 20.f, 4.f, false);
	CImg(ClashArt(TEXT("level")), C - 310.f, OY + 1144.f, 92.f, 92.f);
	CText(FString::FromInt(FP->Data().Level), C - 310.f, OY + 1144.f, 92.f, 88.f, 36.f, FLinearColor::White);
	CText(FString::Printf(TEXT("+%d XP%s"), ClashLvXP, bClashLvPlayerUp ? TEXT("  -  LEVEL UP!") : TEXT("")), C - 200.f, OY + 1136.f, 510.f, 50.f, 30.f, ZCCyan, 0, 4.f);
	const int32 NeedXP = UZoogiFrontProfileSubsystem::XPForLevel(FP->Data().Level);
	CBar(C - 200.f, OY + 1190.f, 500.f, 44.f, float(FP->Data().XP) / FMath::Max(1, NeedXP), ZCCyan, FString::Printf(TEXT("%d/%d"), FP->Data().XP, NeedXP), 22.f);
	CText(TEXT("Tap to continue"), C - 400.f, FMath::Min(OY + 1300.f, ClashNavY() + 40.f), 800.f, 56.f, 34.f, FLinearColor::White, 1, 4.f);
}

// ------------------------------------------------------------------------------------------------ Details popup (+ shared Pass 3 popups dispatch)
bool UZoogiMenuRoot::BuildClashPass3Popup(EZoogiClashPopup P, float PW, float PH)
{
	if (P != EZoogiClashPopup::CardDetails) return false;
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiCardDef* Df = UZoogiFrontProfileSubsystem::FindCard(ClashSelId);
	if (!FP || !Df) return false;
	const bool bOwned = FP->IsCardOwned(Df->Id);
	const int32 Rar = UZoogiFrontProfileSubsystem::RarityOf(*Df), Lv = FP->CardLevelOf(Df->Id), Sp = FP->Spare(Df->Id), Need = UZoogiFrontProfileSubsystem::UpgradeCopies(Lv);
	const bool bMax = Lv >= UZoogiFrontProfileSubsystem::MaxCardLevel, bUp = FP->CanUpgrade(Df->Id);
	// left: art, level, count bar
	const float AW = 300.f, AH = 375.f, AX = 50.f, AY = 140.f;
	CCard(Df->Id, AX, AY, AW, AH, !bOwned);
	if (bOwned)
	{
		CText(bMax ? FString(TEXT("Max Level")) : FString::Printf(TEXT("Level %d"), Lv), AX, AY + AH + 8.f, AW, 44.f, 30.f, ZCGold, 1, 4.f);
		CBar(AX, AY + AH + 58.f, AW, 46.f, bMax ? 1.f : FMath::Min(1.f, float(Sp) / FMath::Max(1, Need)), bUp ? ZCGreen : ZCSfxBlue, bMax ? FString(TEXT("MAX")) : FString::Printf(TEXT("%d/%d"), Sp, Need), 22.f);
		if (bUp) CImg(ClashArt(TEXT("arrow_up")), AX - 14.f, AY + AH + 48.f, 62.f, 62.f);
	}
	else CText(TEXT("LOCKED"), AX, AY + AH + 10.f, AW, 50.f, 32.f, ZCMuted, 1, 3.f);
	// right: Level / Rarity / Type / Element / Cost
	const float RX = AX + AW + 40.f, RW = PW - RX - 50.f;
	auto Row = [this, RX, RW](float Y, const FString& L, const FString& V, const FLinearColor& VC)
	{
		CPlate(RX, Y, RW, 70.f, ZCNavyDeep.CopyWithNewOpacity(0.82f), 14.f, 4.f, false);
		CText(L, RX + 18.f, Y, 200.f, 70.f, 22.f, ZCSoft, 0, 3.f);
		CText(V, RX + 200.f, Y, RW - 218.f, 70.f, 28.f, VC, 2, 4.f);
	};
	Row(140.f, TEXT("Level"), bOwned ? FString::FromInt(Lv) : FString(TEXT("-")), FLinearColor::White);
	Row(222.f, TEXT("Rarity"), UZoogiFrontProfileSubsystem::RarityName(Rar), ZCRarityLabel(Rar));
	Row(304.f, TEXT("Type"), ZoogiCards::TypeName(Df->Type), FLinearColor::White);
	Row(386.f, TEXT("Element"), ZoogiCards::ElementName(Df->Element), FLinearColor::White);
	CText(Df->Rule, RX, 466.f, RW, 110.f, 20.f, FLinearColor::White, 0, 2.f, true);
	// 6 stat tiles (real Cards.csv values) with the next-level delta
	const TArray<FZKStat> St = ZKStats(*Df);
	const float TY0 = 620.f, TW = (PW - 100.f - 20.f) * 0.5f, TH = 100.f;
	for (int32 i = 0; i < St.Num() && i < 6; ++i)
	{
		const float X = 50.f + (i % 2) * (TW + 20.f), Y = TY0 + (i / 2) * (TH + 14.f);
		CPlate(X, Y, TW, TH, ZCNavyDeep.CopyWithNewOpacity(0.85f), 16.f, 4.f, false);
		CImg(ClashArt(St[i].Icon), X + 12.f, Y + 20.f, 60.f, 60.f);
		CText(St[i].Label, X + 84.f, Y + 6.f, TW - 96.f, 40.f, 21.f, ZCSoft, 0, 3.f);
		CText(St[i].Value, X + 84.f, Y + 44.f, TW - 190.f, 48.f, St[i].Value.Len() > 14 ? 20.f : 28.f, FLinearColor::White, 0, 3.f);
		if (bOwned && !bMax) CText(TEXT("+0"), X + TW - 104.f, Y + 44.f, 90.f, 48.f, 26.f, ZCMuted, 2, 3.f);
	}
	const float NoteY = TY0 + 3.f * (TH + 14.f);
	CText(TEXT("Values from Cards.csv. Card levels don't change battle values yet (+0): upgrading raises the level, spends gold + copies and gives player XP."),
		50.f, NoteY, PW - 100.f, 64.f, 18.f, ZCSoft, 1, 2.f, true);
	if (!bOwned) { CText(FString::Printf(TEXT("Unlock: %s"), *FP->LockText(Df->Id)), 50.f, NoteY + 80.f, PW - 100.f, 60.f, 24.f, FLinearColor::White, 1, 3.f, true); return true; }
	// bottom: UPGRADE [gold] (grey when it can't) + green USE
	const float BY = PH - 150.f, BW = (PW - 100.f - 30.f) * 0.5f;
	const int32 Cost = UZoogiFrontProfileSubsystem::UpgradeGold(Lv);
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(bMax ? TEXT("MAX LEVEL") : TEXT("UPGRADE"), 30.f, FLinearColor::White, 4.f))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f)); }
	if (!bMax)
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(Art(TEXT("icon_coin")), 44.f, 44.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(ZKNum(Cost), 30.f, FP->Data().Gold >= Cost ? FLinearColor::White : ZCPriceRed, 4.f))) S->SetVerticalAlignment(VAlign_Center);
	}
	H->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (bUp)
		CChunkyW(H, ZCYellow, [this]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			const FName Id = ClashSelId; const int32 From = F->CardLevelOf(Id);
			FString Why; int32 XP = 0; bool bUpP = false;
			if (!F->UpgradeCard(Id, Why, XP, bUpP)) { ClashFail(Why); return; }
			ClashLvId = Id; ClashLvFrom = From; ClashLvXP = XP; bClashLvPlayerUp = bUpP;
			ClashDetailsBack = EZoogiClashPopup::CardDetails;
			OpenClashPopup(EZoogiClashPopup::LevelUp, TEXT("LEVEL UP"));
		}, 50.f, BY, BW, 110.f, true);
	else
	{
		UWidget* Dim = CChunkyVisual(H, ZCHex(TEXT("7A8499")), 50.f, BY, BW, 110.f);
		if (!bMax) CText(FString::Printf(TEXT("Needs %d/%d copies"), Sp, Need), 50.f, BY - 44.f, BW, 40.f, 22.f, ZCSoft, 1, 3.f);
		(void)Dim;
	}
	const FName UseId = Df->Id;
	CChunky(TEXT("USE"), ZCGreen, [this, UseId]() { ClashUseCard(UseId); }, 50.f + BW + 30.f, BY, BW, 110.f, 38.f, nullptr, true);
	return true;
}
