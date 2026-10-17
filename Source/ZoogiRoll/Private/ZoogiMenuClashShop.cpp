// clash-menu (Oct 17, milestone 2): Shop tab (Daily Deals, Chests, Gold, Gems), Cards tab (Decks / Collection / Emotes),
// chest slots on Home, and the popups behind them (deal, chest, gold, rewards, card, match result, chest slot).
// Economy lives in UZoogiFrontProfileSubsystem (local save; gems = the progression ledger). Real-money gem packs need the
// store backend: they are shown with their price and a NEEDS STORE note, and nothing is charged or granted.
// Same look as ZoogiMenuClash.cpp (shapes drawn in code + Zoogi Roll's own card art / MenuArt).
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
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
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

namespace
{
	FString ZCNum(int32 V) { return FText::AsNumber(V).ToString(); }
	FString ZCChestName(FName Type) { const FZoogiFrontChestDef* C = UZoogiFrontProfileSubsystem::FindChest(Type); return C ? FString(C->Name).ToUpper() : Type.ToString().ToUpper(); }
	FString ZCCardName(FName Id) { const FZoogiCardDef* C = UZoogiFrontProfileSubsystem::FindCard(Id); return C ? C->Name.ToUpper() : Id.ToString().ToUpper(); }
	int32 ZCRarity(FName Id) { const FZoogiCardDef* C = UZoogiFrontProfileSubsystem::FindCard(Id); return C ? UZoogiFrontProfileSubsystem::RarityOf(*C) : 0; }
}

// ------------------------------------------------------------------------------------------------ helpers
UWidget* UZoogiMenuRoot::CLive(TFunction<FString()> Fn, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, int32 Justify, float Outline)
{
	UWidget* W = CText(Fn ? Fn() : FString(), X, Y, Wd, Ht, Size, C, Justify, Outline);
	if (UBorder* B = Cast<UBorder>(W))
		if (UTextBlock* T = Cast<UTextBlock>(B->GetContent())) ClashLive.Add(TPair<TWeakObjectPtr<UTextBlock>, TFunction<FString()>>(T, MoveTemp(Fn)));
	return W;
}

UWidget* UZoogiMenuRoot::CSection(const FString& Title, float Y)
{
	const float C = DesignW * 0.5f;
	CImg(ClashArt(TEXT("ribbon")), C - 300.f, Y, 600.f, 100.f);
	return CText(Title, C - 220.f, Y + 2.f, 440.f, 76.f, 40.f, FLinearColor::White);
}

UWidget* UZoogiMenuRoot::CCard(FName Id, float X, float Y, float Wd, float Ht, bool bLocked, const FString& Badge, bool bLevel)
{
	// card frame in its rarity colour, Zoogi Roll card art, energy cost bubble, name strip, optional badge / level pill
	const FZoogiCardDef* Def = UZoogiFrontProfileSubsystem::FindCard(Id);
	const int32 Rar = Def ? UZoogiFrontProfileSubsystem::RarityOf(*Def) : 0;
	const FLinearColor Frame = bLocked ? ZCHex(TEXT("5B6787")) : ZCRarityColor(Rar);
	UCanvasPanel* G = CGroupBegin();
	const float R = FMath::Clamp(Wd * 0.09f, 8.f, 18.f);
	CAt(CBox(Frame, ZCDark, 4.f, R), 0.f, 0.f, Wd, Ht);
	CAt(CBox(ZCNavyDeep, ZCDarken(Frame, 0.3f), 2.f, FMath::Max(4.f, R - 5.f)), 7.f, 7.f, Wd - 14.f, Ht - 14.f);
	const float NameH = FMath::Clamp(Ht * 0.17f, 24.f, 44.f);
	CImg(Art(Id.ToString()), 9.f, 9.f, Wd - 18.f, Ht - 18.f - NameH + 6.f, FMath::Max(3.f, R - 7.f), true, bLocked ? FLinearColor(0.32f, 0.34f, 0.42f, 1.f) : FLinearColor::White);
	CAt(CBox(ZCDark.CopyWithNewOpacity(0.82f), FLinearColor::Transparent, 0.f, 6.f), 9.f, Ht - 9.f - NameH, Wd - 18.f, NameH);
	CText(Def ? Def->Name.ToUpper() : Id.ToString().ToUpper(), 10.f, Ht - 9.f - NameH, Wd - 20.f, NameH, FMath::Clamp(Wd * 0.105f, 12.f, 24.f), bLocked ? ZCMuted : FLinearColor::White, 1, 2.f);
	const float CS = FMath::Clamp(Wd * 0.26f, 26.f, 54.f);
	CAt(CBox(ZCViolet, ZCDark, 3.f, CS * 0.5f), -6.f, -6.f, CS, CS);
	CText(FString::FromInt(Def ? Def->Cost : 1), -6.f, -6.f, CS, CS - 2.f, CS * 0.55f, FLinearColor::White, 1, 2.f);
	if (bLocked) CImg(ClashArt(TEXT("lock")), Wd * 0.5f - Wd * 0.2f, Ht * 0.36f - Wd * 0.2f, Wd * 0.4f, Wd * 0.4f);
	if (!Badge.IsEmpty())
	{
		const float BW = FMath::Clamp(Wd * 0.42f, 44.f, 96.f), BH = FMath::Clamp(Wd * 0.22f, 26.f, 46.f);
		CAt(CBox(ZCYellow, ZCDark, 3.f, BH * 0.5f), Wd - BW + 6.f, -6.f, BW, BH);
		CText(Badge, Wd - BW + 6.f, -6.f, BW, BH - 2.f, BH * 0.6f, FLinearColor::White, 1, 2.f);
	}
	else if (bLevel && !bLocked)
	{
		int32 Into = 0, Need = 1;
		const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
		const int32 Lv = UZoogiFrontProfileSubsystem::CardLevel(FP ? FP->Copies(Id) : 1, Into, Need);
		const float BW = FMath::Clamp(Wd * 0.44f, 48.f, 96.f), BH = FMath::Clamp(Wd * 0.2f, 24.f, 42.f);
		CAt(CBox(ZCBlueDeep, ZCDark, 3.f, BH * 0.5f), Wd - BW + 6.f, -6.f, BW, BH);
		CText(FString::Printf(TEXT("LV %d"), Lv), Wd - BW + 6.f, -6.f, BW, BH - 2.f, BH * 0.55f, FLinearColor::White, 1, 2.f);
	}
	CGroupEnd();
	G->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(G, X, Y, Wd, Ht);
}

void UZoogiMenuRoot::ClashBuy(TFunction<void()> Fn)
{
	if (ClashBuyT0 >= 0.f) return; // one purchase at a time
	ClashBuyT0 = Time;
	ClashBuyFn = MoveTemp(Fn);
	UE_LOG(LogZoogi, Log, TEXT("CLASH buy: spinner"));
	bDirty = true;
}

void UZoogiMenuRoot::ClashFail(const FString& Why)
{
	UE_LOG(LogZoogi, Log, TEXT("CLASH purchase refused: %s"), *Why);
	OpenClashPopup(EZoogiClashPopup::Info, TEXT("NOT YET!"), Why);
}

void UZoogiMenuRoot::ClashShowRewards(const FString& Title, FName Chest, const TArray<FZoogiFrontReward>& R)
{
	ClashRewards = R;
	ClashRewardsChest = Chest;
	OpenClashPopup(EZoogiClashPopup::Rewards, Title);
}

void UZoogiMenuRoot::ClashTapChestSlot(int32 SlotIdx)
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP || !FP->Data().Chests.IsValidIndex(SlotIdx)) return;
	const FName Type = FP->Data().Chests[SlotIdx].Type;
	if (Type.IsNone()) { OpenClashPopup(EZoogiClashPopup::Info, TEXT("CHEST SLOT"), TEXT("Win a battle to earn a chest.\nWins fill a free slot; each chest unlocks on a timer.")); return; }
	if (FP->IsReady(SlotIdx))
	{
		TArray<FZoogiFrontReward> R; FString Why;
		if (FP->OpenChest(SlotIdx, false, R, Why)) ClashShowRewards(ZCChestName(Type), Type, R);
		else ClashFail(Why);
		return;
	}
	ClashSel = SlotIdx;
	ClashSelId = Type;
	OpenClashPopup(EZoogiClashPopup::ChestSlot, ZCChestName(Type));
}

FVector2D UZoogiMenuRoot::ClashPopupSize(EZoogiClashPopup P) const
{
	switch (P)
	{
	case EZoogiClashPopup::Deal: return FVector2D(900.f, 800.f);
	case EZoogiClashPopup::Chest: return FVector2D(900.f, 780.f);
	case EZoogiClashPopup::Gold: return FVector2D(820.f, 660.f);
	case EZoogiClashPopup::Rewards: return FVector2D(1100.f, 840.f);
	case EZoogiClashPopup::Card: return FVector2D(1000.f, 720.f);
	case EZoogiClashPopup::MatchResult: return FVector2D(900.f, 800.f);
	case EZoogiClashPopup::ChestSlot: return FVector2D(880.f, 720.f);
	case EZoogiClashPopup::Arenas:
	case EZoogiClashPopup::TrophyRoad:
	case EZoogiClashPopup::BattlePass: return FVector2D(FMath::Min(1560.f, DesignW - 120.f), 950.f);
	case EZoogiClashPopup::PremiumUnlock: return FVector2D(860.f, 620.f);
	default: return FVector2D(980.f, 640.f);
	}
}

// ------------------------------------------------------------------------------------------------ Home chest slots
void UZoogiMenuRoot::BuildClashChestSlots(float Y)
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const float C = DesignW * 0.5f;
	for (int32 i = 0; i < 3; ++i)
	{
		const float SW = 176.f, SH = 108.f, SX = C - (SW * 1.5f + 20.f) + i * (SW + 20.f);
		const FZoogiFrontChestSlot ChestS = FP && FP->Data().Chests.IsValidIndex(i) ? FP->Data().Chests[i] : FZoogiFrontChestSlot();
		const bool bHas = !ChestS.Type.IsNone();
		const bool bReady = bHas && FP->IsReady(i);
		const bool bUnlocking = bHas && !bReady && FP->SecondsLeft(i) > 0;
		if (!bHas)
		{
			// empty slot (#1F3A6B, grey chest silhouette, "Win a battle"): no response, like the demo
			CPlate(SX, Y, SW, SH, ZCSlot, 16.f, 4.f, false);
			CImg(ClashArt(TEXT("chest")), SX + SW * 0.5f - 34.f, Y + 6.f, 68.f, 68.f, 0.f, false, FLinearColor(0.32f, 0.38f, 0.5f, 0.55f));
			CText(TEXT("Win a battle"), SX + 6.f, Y + SH - 38.f, SW - 12.f, 32.f, 19.f, ZCMuted, 1, 3.f);
			continue;
		}
		UCanvasPanel* Grp = CGroupBegin();
		CPlate(0.f, 0.f, SW, SH, bReady ? ZCHex(TEXT("2E9A3C")) : ZCSlot, 16.f, 4.f, true);
		{
			CImg(ClashArt(TEXT("chest_") + ChestS.Type.ToString()), SW * 0.5f - 36.f, 2.f, 72.f, 72.f);
			if (bReady) CText(TEXT("OPEN!"), 4.f, SH - 38.f, SW - 8.f, 34.f, 24.f, ZCYellow);
			else if (bUnlocking)
			{
				const int32 Idx = i;
				TWeakObjectPtr<const UZoogiFrontProfileSubsystem> WFP(FP);
				CLive([WFP, Idx]() { const UZoogiFrontProfileSubsystem* F = WFP.Get(); return F ? ZCDur(F->SecondsLeft(Idx)) : FString(); }, 4.f, SH - 36.f, SW - 8.f, 32.f, 22.f, ZCGold);
			}
			else CText(TEXT("TAP TO UNLOCK"), 4.f, SH - 34.f, SW - 8.f, 30.f, 16.f, FLinearColor::White, 1, 3.f);
		}
		CGroupEnd();
		CAt(CButton(Grp, [this, i]() { ClashTapChestSlot(i); }), SX, Y, SW, SH);
		if (bReady) CDot(SX + SW - 30.f, Y - 12.f, 40.f, TEXT("!"));
	}
}

// ------------------------------------------------------------------------------------------------ Shop
void UZoogiMenuRoot::BuildClashShopTab()
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return;
	FP->EnsureDeals();
	const FZoogiFrontProfileData& D = FP->Data();
	const float W = DesignW, C = W * 0.5f, TopY = 104.f, ViewH = 944.f - TopY;
	if (UScrollBox* Old = ClashScrollW.Get()) if (ClashScrollKey == 0 && ClashPendingScroll < 0.f) ClashScrollOffset = Old->GetScrollOffset();
	ClashScrollKey = 0;

	UCanvasPanel* G = CGroupBegin();
	float Y = 20.f;
	// ---- Daily Deals: 3x2 tiles of our own cards, slot 0 free; resets daily at the progression reset hour
	ClashShopY[0] = 0.f;
	CSection(TEXT("DAILY DEALS"), Y);
	TWeakObjectPtr<UZoogiFrontProfileSubsystem> WFP(FP);
	CLive([WFP]() { const UZoogiFrontProfileSubsystem* F = WFP.Get(); return F ? FString::Printf(TEXT("New deals in %s"), *ZCDur((int64)F->TimeToDealsReset().GetTotalSeconds())) : FString(); },
		C - 300.f, Y + 96.f, 600.f, 40.f, 24.f, ZCSoft);
	{
		const float TW = 300.f, TH = 340.f, Gap = 28.f, X0 = C - (TW * 3.f + Gap * 2.f) * 0.5f, Y0 = Y + 150.f;
		for (int32 i = 0; i < D.Deals.Num() && i < 6; ++i)
		{
			const FZoogiFrontDeal Deal = D.Deals[i];
			const int32 Rar = ZCRarity(Deal.CardId);
			UCanvasPanel* T = CGroupBegin();
			CPlate(0.f, 0.f, TW, TH, Deal.bFree ? ZCHex(TEXT("2A7FD8")) : ZCNavy, 22.f, 5.f, true);
			CCard(Deal.CardId, TW * 0.5f - 76.f, 18.f, 152.f, 190.f, false, FString::Printf(TEXT("x%d"), Deal.Count));
			CText(ZCCardName(Deal.CardId), 0.f, 214.f, TW, 34.f, 24.f, FLinearColor::White);
			CText(UZoogiFrontProfileSubsystem::RarityName(Rar), 0.f, 244.f, TW, 26.f, 18.f, ZCLighten(ZCRarityColor(Rar), 0.2f), 1, 3.f);
			if (Deal.bBought)
			{
				CAt(CBox(ZCDark.CopyWithNewOpacity(0.62f), FLinearColor::Transparent, 0.f, 20.f), 3.f, 3.f, TW - 6.f, TH - 6.f);
				CChunkyVisual(CTxt(TEXT("PURCHASED"), 24.f, ZCMuted, 3.f), ZCHex(TEXT("4A5675")), 30.f, TH - 82.f, TW - 60.f, 66.f);
			}
			else if (Deal.bFree) CChunkyVisual(CTxt(TEXT("FREE"), 30.f, FLinearColor::White, 4.f), ZCGreen, 30.f, TH - 82.f, TW - 60.f, 66.f);
			else
			{
				UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
				I->SetBrush(ZCImgBrush(Art(TEXT("icon_coin")), 40.f, 40.f, 0.f, false));
				if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
				if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(ZCNum(Deal.Price), 30.f, FLinearColor::White, 4.f))) S->SetVerticalAlignment(VAlign_Center);
				CChunkyVisual(H, ZCGreen, 30.f, TH - 82.f, TW - 60.f, 66.f);
			}
			if (Deal.bFree && !Deal.bBought) CDot(TW - 34.f, -12.f, 44.f, TEXT("!"));
			CGroupEnd();
			const FName Id = Deal.CardId; const bool bBought = Deal.bBought;
			CAt(CButton(T, [this, i, Id, bBought]()
			{
				if (bBought) { OpenClashPopup(EZoogiClashPopup::Info, TEXT("PURCHASED"), TEXT("You already bought this deal today.\nNew deals arrive at the daily reset.")); return; }
				ClashSel = i; ClashSelId = Id; OpenClashPopup(EZoogiClashPopup::Deal, ZCCardName(Id));
			}), X0 + (i % 3) * (TW + Gap), Y0 + (i / 3) * (TH + Gap), TW, TH);
		}
		Y = Y0 + TH * 2.f + Gap + 60.f;
	}

	// ---- Chests (gems): open right away into card rewards
	ClashShopY[1] = Y - 10.f;
	CSection(TEXT("CHESTS"), Y);
	{
		const TArray<FZoogiFrontChestDef>& Defs = UZoogiFrontProfileSubsystem::ChestDefs();
		const float TW = 340.f, TH = 390.f, Gap = 34.f, X0 = C - (TW * Defs.Num() + Gap * (Defs.Num() - 1)) * 0.5f, Y0 = Y + 130.f;
		for (int32 i = 0; i < Defs.Num(); ++i)
		{
			const FZoogiFrontChestDef& Def = Defs[i];
			UCanvasPanel* T = CGroupBegin();
			CGoldPanel(0.f, 0.f, TW, TH, i == 2 ? ZCViolet : i == 1 ? ZCBlueDeep : ZCHex(TEXT("2F6BD0")), 24.f);
			CImg(ClashArt(TEXT("rays")), TW * 0.5f - 150.f, -10.f, 300.f, 300.f, 0.f, false, FLinearColor(1.f, 1.f, 1.f, 0.6f));
			CImg(ClashArt(TEXT("chest_") + Def.Id.ToString()), TW * 0.5f - 95.f, 36.f, 190.f, 190.f);
			CText(FString(Def.Name).ToUpper(), 0.f, 222.f, TW, 40.f, 28.f, FLinearColor::White);
			CText(FString::Printf(TEXT("%d cards  -  %d-%d gold"), Def.Cards, Def.GoldMin, Def.GoldMax), 10.f, 258.f, TW - 20.f, 30.f, 18.f, ZCSoft, 1, 3.f);
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			I->SetBrush(ZCImgBrush(Art(TEXT("icon_gem")), 42.f, 42.f, 0.f, false));
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(ZCNum(Def.GemCost), 30.f, FLinearColor::White, 4.f))) S->SetVerticalAlignment(VAlign_Center);
			CChunkyVisual(H, ZCGreen, 34.f, TH - 92.f, TW - 68.f, 70.f);
			CGroupEnd();
			const FName Id = Def.Id;
			CAt(CButton(T, [this, Id]() { ClashSelId = Id; OpenClashPopup(EZoogiClashPopup::Chest, ZCChestName(Id)); }), X0 + i * (TW + Gap), Y0, TW, TH);
		}
		Y = Y0 + TH + 60.f;
	}

	// ---- Gold (gems)
	ClashShopY[2] = Y - 10.f;
	CSection(TEXT("GOLD"), Y);
	{
		const TArray<UZoogiFrontProfileSubsystem::FGoldPack>& Packs = UZoogiFrontProfileSubsystem::GoldPacks();
		const float TW = 340.f, TH = 310.f, Gap = 34.f, X0 = C - (TW * Packs.Num() + Gap * (Packs.Num() - 1)) * 0.5f, Y0 = Y + 130.f;
		for (int32 i = 0; i < Packs.Num(); ++i)
		{
			UCanvasPanel* T = CGroupBegin();
			CPlate(0.f, 0.f, TW, TH, ZCNavy, 22.f, 5.f, true);
			const float GS = 120.f + i * 22.f;
			CImg(ClashArt(TEXT("goldpile")), TW * 0.5f - GS * 0.5f, 150.f - GS, GS, GS);
			CText(ZCNum(Packs[i].Gold), 0.f, 156.f, TW, 44.f, 34.f, ZCGold);
			CText(TEXT("GOLD"), 0.f, 194.f, TW, 26.f, 18.f, ZCSoft, 1, 3.f);
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			I->SetBrush(ZCImgBrush(Art(TEXT("icon_gem")), 40.f, 40.f, 0.f, false));
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(ZCNum(Packs[i].Gems), 30.f, FLinearColor::White, 4.f))) S->SetVerticalAlignment(VAlign_Center);
			CChunkyVisual(H, ZCGreen, 34.f, TH - 86.f, TW - 68.f, 68.f);
			CGroupEnd();
			CAt(CButton(T, [this, i]() { ClashSel = i; OpenClashPopup(EZoogiClashPopup::Gold, TEXT("GOLD")); }), X0 + i * (TW + Gap), Y0, TW, TH);
		}
		Y = Y0 + TH + 60.f;
	}

	// ---- Gems: real-money packs from GemPackages.csv; need the platform store (not faked)
	ClashShopY[3] = Y - 10.f;
	CSection(TEXT("GEMS"), Y);
	{
		const UZoogiProgressionSubsystem* PS = GetGameInstance() ? GetGameInstance()->GetSubsystem<UZoogiProgressionSubsystem>() : nullptr;
		TArray<FZoogiGemPackageRow> Packs;
		if (PS) Packs = PS->Core().Config().GemPackages;
		const int32 N = FMath::Max(1, Packs.Num());
		const float TW = 290.f, TH = 330.f, Gap = 26.f, X0 = C - (TW * N + Gap * (N - 1)) * 0.5f, Y0 = Y + 130.f;
		for (int32 i = 0; i < Packs.Num(); ++i)
		{
			const FZoogiGemPackageRow& P = Packs[i];
			UCanvasPanel* T = CGroupBegin();
			CPlate(0.f, 0.f, TW, TH, ZCHex(TEXT("2F4FA8")), 22.f, 5.f, true);
			const int32 Gems = FMath::Min(5, 1 + i);
			for (int32 g = 0; g < Gems; ++g)
				CImg(Art(TEXT("icon_gem")), TW * 0.5f - 46.f + (g - (Gems - 1) * 0.5f) * 26.f, 34.f + (g % 2) * 12.f, 92.f, 92.f);
			CText(ZCNum(P.Gems), 0.f, 140.f, TW, 44.f, 34.f, ZCCyan);
			CText(P.BonusGems > 0 ? FString::Printf(TEXT("+%s BONUS"), *ZCNum(P.BonusGems)) : FString(TEXT("GEMS")), 0.f, 180.f, TW, 26.f, 18.f, P.BonusGems > 0 ? ZCYellow : ZCSoft, 1, 3.f);
			CChunkyVisual(CTxt(FString::Printf(TEXT("$%.2f"), P.PriceUSD), 28.f, FLinearColor::White, 4.f), ZCHex(TEXT("6A7898")), 30.f, TH - 112.f, TW - 60.f, 64.f);
			CText(TEXT("NEEDS STORE"), 0.f, TH - 46.f, TW, 30.f, 18.f, ZCYellow, 1, 3.f);
			CGroupEnd();
			const FString Body = FString::Printf(TEXT("%s gems (%s) is a real-money pack.\nIt needs the platform store (App Store / Google Play / Steam), which is not connected yet.\nNo purchase was made and no gems were added."),
				*ZCNum(P.Gems + P.BonusGems), *FString::Printf(TEXT("$%.2f"), P.PriceUSD));
			CAt(CButton(T, [this, Body]() { OpenClashPopup(EZoogiClashPopup::Info, TEXT("NEEDS STORE"), Body); }), X0 + i * (TW + Gap), Y0, TW, TH);
		}
		if (Packs.Num() == 0) CText(TEXT("No gem packages configured (GemPackages.csv)."), C - 400.f, Y0, 800.f, 60.f, 24.f, ZCSoft, 1, 3.f);
		CText(TEXT("Gem packs are real-money purchases: they need the store backend. Nothing is charged or granted here."), C - 640.f, Y0 + TH + 20.f, 1280.f, 40.f, 20.f, ZCSoft, 1, 3.f);
		Y = Y0 + TH + 100.f;
	}
	CGroupEnd();

	USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SB->SetWidthOverride(W);
	SB->SetHeightOverride(Y);
	SB->AddChild(G);
	UScrollBox* Sc = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Sc->SetAnimateWheelScrolling(true);
	Sc->SetScrollBarVisibility(ESlateVisibility::Visible);
	Sc->AddChild(SB);
	CAt(Sc, 0.f, TopY, W, ViewH);
	ClashScrollW = Sc;
	if (ClashPendingScroll < 0.f) ClashPendingScroll = ClashScrollOffset; // applied in TickClash once Slate has the widget
	Sc->SetScrollOffset(ClashPendingScroll);
}

// ------------------------------------------------------------------------------------------------ Cards
void UZoogiMenuRoot::BuildClashCardsTab()
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return;
	const float W = DesignW, C = W * 0.5f;
	// sub-tabs
	const TCHAR* Subs[] = { TEXT("DECKS"), TEXT("COLLECTION"), TEXT("EMOTES") };
	for (int32 k = 0; k < 3; ++k)
	{
		const float BW = 290.f, BH = 78.f, BX = C - (BW * 1.5f + 20.f) + k * (BW + 20.f);
		CChunky(Subs[k], ClashCardsSub == k ? ZCYellow : ZCBlueDeep, [this, k]()
		{
			if (ClashCardsSub != k) { ClashCardsSub = k; ClashScrollOffset = 0.f; ClashPendingScroll = -1.f; ClashScrollKey = -1; UE_LOG(LogZoogi, Log, TEXT("CLASH cards sub %d"), k); }
			bDirty = true;
		}, BX, 116.f, BW, BH, 30.f);
	}
	const TArray<FZoogiCardDef>& Defs = UZoogiFrontProfileSubsystem::CardDefs();
	TArray<FName> Owned, Locked;
	for (const FZoogiCardDef& Def : Defs) (FP->IsCardOwned(Def.Id) ? Owned : Locked).Add(Def.Id);

	if (ClashCardsSub == 0)
	{
		// ---- Decks: Deck 1/2/3, 15 slots (tap to remove), owned cards (tap to add), USE THIS DECK (mirrors the saved deck matches use)
		const int32 Active = FP->Data().ActiveDeck;
		const int32 View = ClashDeckView < 0 ? Active : FMath::Clamp(ClashDeckView, 0, 2);
		const TArray<FName> Deck = FP->Deck(View);
		const float LX = 48.f, LY = 210.f, LW = FMath::Min(820.f, W * 0.46f), LH = 720.f;
		CGoldPanel(LX, LY, LW, LH, ZCPanel, 26.f);
		for (int32 d = 0; d < 3; ++d)
			CChunky(FString::Printf(TEXT("DECK %d"), d + 1), d == View ? ZCYellow : ZCBlueDeep, [this, d]() { ClashDeckView = d; bDirty = true; }, LX + 24.f + d * 162.f, LY + 22.f, 150.f, 72.f, 26.f);
		if (View == Active) { CPlate(LX + LW - 24.f - 210.f, LY + 28.f, 210.f, 60.f, ZCHex(TEXT("2E9A3C")), 16.f, 4.f, true); CText(TEXT("IN USE"), LX + LW - 24.f - 210.f, LY + 28.f, 210.f, 60.f, 26.f, FLinearColor::White); }
		else CChunky(TEXT("USE THIS DECK"), ZCGreen, [this, View]() { if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this)) { F->SetActiveDeck(View); UE_LOG(LogZoogi, Log, TEXT("CLASH active deck %d"), View + 1); } bDirty = true; },
			LX + LW - 24.f - 250.f, LY + 22.f, 250.f, 72.f, 24.f);
		CText(FString::Printf(TEXT("%d/%d CARDS"), Deck.Num(), UZoogiFrontProfileSubsystem::DeckSize), LX + 24.f, LY + 104.f, 300.f, 36.f, 24.f, Deck.Num() == UZoogiFrontProfileSubsystem::DeckSize ? ZCGold : FLinearColor::White, 0, 3.f);
		CText(Deck.Num() < UZoogiFrontProfileSubsystem::DeckSize ? TEXT("Add cards from the right (short decks are topped up)") : TEXT("Tap a card to take it out (max 2 copies)"),
			LX + 300.f, LY + 104.f, LW - 324.f, 36.f, 17.f, ZCSoft, 2, 2.f);
		const float Gap = 14.f, SW = (LW - 48.f - Gap * 4.f) / 5.f, SH = FMath::Min(SW * 1.25f, (LH - 160.f - Gap * 2.f - 20.f) / 3.f), GX = LX + 24.f, GY = LY + 150.f;
		const float CW = FMath::Min(SW, SH / 1.25f);
		for (int32 s = 0; s < UZoogiFrontProfileSubsystem::DeckSize; ++s)
		{
			const float X = GX + (s % 5) * (SW + Gap) + (SW - CW) * 0.5f, Y = GY + (s / 5) * (SH + Gap);
			if (Deck.IsValidIndex(s))
			{
				const FName Id = Deck[s];
				UCanvasPanel* T = CGroupBegin();
				CCard(Id, 0.f, 0.f, CW, SH, !FP->IsCardOwned(Id));
				CGroupEnd();
				CAt(CButton(T, [this, View, s]()
				{
					if (UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this))
					{
						TArray<FName> Dk = F->Deck(View);
						if (Dk.IsValidIndex(s)) { UE_LOG(LogZoogi, Log, TEXT("CLASH deck %d remove %s"), View + 1, *Dk[s].ToString()); Dk.RemoveAt(s); F->SetDeck(View, Dk); }
					}
					bDirty = true;
				}), X, Y, CW, SH);
			}
			else
			{
				CPlate(X, Y, CW, SH, ZCNavyDeep.CopyWithNewOpacity(0.7f), 14.f, 3.f, false);
				CText(TEXT("+"), X, Y, CW, SH, 54.f, ZCMuted, 1, 3.f);
			}
		}
		// owned cards to add
		const float RX = LX + LW + 32.f, RW = W - 48.f - RX, RY = 214.f;
		CText(TEXT("TAP A CARD TO ADD IT"), RX, RY, RW, 40.f, 28.f, FLinearColor::White);
		const float CG = 16.f, CardW = 150.f, CardH = 188.f;
		const int32 Cols = FMath::Max(1, FMath::FloorToInt((RW + CG) / (CardW + CG)));
		const float RX0 = RX + (RW - (Cols * (CardW + CG) - CG)) * 0.5f;
		for (int32 i = 0; i < Owned.Num(); ++i)
		{
			const FName Id = Owned[i];
			int32 InDeck = 0; for (const FName& N : Deck) InDeck += N == Id ? 1 : 0;
			const float X = RX0 + (i % Cols) * (CardW + CG), Y = RY + 60.f + (i / Cols) * (CardH + CG + 4.f);
			UCanvasPanel* T = CGroupBegin();
			CCard(Id, 0.f, 0.f, CardW, CardH, false, FString::Printf(TEXT("%d/%d"), InDeck, UZoogiFrontProfileSubsystem::MaxCopiesInDeck));
			if (InDeck >= UZoogiFrontProfileSubsystem::MaxCopiesInDeck) CAt(CBox(ZCDark.CopyWithNewOpacity(0.45f), FLinearColor::Transparent, 0.f, 12.f), 0.f, 0.f, CardW, CardH);
			CGroupEnd();
			CAt(CButton(T, [this, View, Id]()
			{
				UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
				if (!F) return;
				TArray<FName> Dk = F->Deck(View);
				int32 N = 0; for (const FName& X : Dk) N += X == Id ? 1 : 0;
				if (Dk.Num() >= UZoogiFrontProfileSubsystem::DeckSize) { ClashFail(TEXT("This deck is full (15 cards).\nTap a card in the deck to take it out first.")); return; }
				if (N >= UZoogiFrontProfileSubsystem::MaxCopiesInDeck) { ClashFail(TEXT("A deck can hold at most 2 copies of a card.")); return; }
				Dk.Add(Id); F->SetDeck(View, Dk);
				UE_LOG(LogZoogi, Log, TEXT("CLASH deck %d add %s (%d)"), View + 1, *Id.ToString(), Dk.Num());
				bDirty = true;
			}), X, Y, CardW, CardH);
		}
		const float NY = RY + 60.f + FMath::DivideAndRoundUp(Owned.Num(), Cols) * (CardH + CG + 4.f) + 10.f;
		if (Locked.Num() > 0)
			CText(FString::Printf(TEXT("%d more cards unlock with their powers (see COLLECTION)."), Locked.Num()), RX, NY, RW, 36.f, 20.f, ZCSoft, 1, 3.f);
		return;
	}

	if (ClashCardsSub == 1)
	{
		// ---- Collection: owned grid (level + copies bar), then LOCKED CARDS; tap = detail popover
		const float TopY = 210.f, ViewH = 944.f - TopY;
		if (UScrollBox* Old = ClashScrollW.Get()) if (ClashScrollKey == 1 && ClashPendingScroll < 0.f) ClashScrollOffset = Old->GetScrollOffset();
		ClashScrollKey = 1;
		UCanvasPanel* G = CGroupBegin();
		const float CardW = 170.f, CardH = 212.f, CG = 22.f, CellH = CardH + 46.f;
		const int32 Cols = FMath::Max(1, FMath::FloorToInt((W - 96.f + CG) / (CardW + CG)));
		const float X0 = C - (Cols * (CardW + CG) - CG) * 0.5f;
		CText(FString::Printf(TEXT("FOUND %d/%d"), Owned.Num(), Defs.Num()), C - 300.f, 6.f, 600.f, 44.f, 30.f, ZCGold);
		float Y = 70.f;
		auto Grid = [&](const TArray<FName>& Ids, bool bLocked)
		{
			for (int32 i = 0; i < Ids.Num(); ++i)
			{
				const FName Id = Ids[i];
				const float X = X0 + (i % Cols) * (CardW + CG), YY = Y + (i / Cols) * (CellH + CG);
				UCanvasPanel* T = CGroupBegin();
				CCard(Id, 0.f, 0.f, CardW, CardH, bLocked, FString(), true);
				if (!bLocked)
				{
					int32 Into = 0, Need = 1;
					UZoogiFrontProfileSubsystem::CardLevel(FP->Copies(Id), Into, Need);
					CBar(0.f, CardH + 8.f, CardW, 34.f, float(Into) / FMath::Max(1, Need), ZCGreen, FString::Printf(TEXT("%d/%d"), Into, Need), 17.f);
				}
				else CText(TEXT("LOCKED"), 0.f, CardH + 6.f, CardW, 34.f, 18.f, ZCMuted, 1, 3.f);
				CGroupEnd();
				CAt(CButton(T, [this, Id]() { ClashSelId = Id; OpenClashPopup(EZoogiClashPopup::Card, ZCCardName(Id)); }), X, YY, CardW, CellH);
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
		Y += 40.f;
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

	// ---- Emotes: emotes the progression profile really owns (affinity rewards), else an honest empty state
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
	if (Emotes.Num() == 0)
	{
		const float PW = FMath::Min(1040.f, W - 200.f), PH = 600.f, PX = C - PW * 0.5f, PY = 240.f;
		CGoldPanel(PX, PY, PW, PH, ZCPanel, 30.f);
		CImg(ClashArt(TEXT("emote")), C - 90.f, PY + 50.f, 180.f, 180.f);
		CText(TEXT("NO EMOTES YET"), PX, PY + 250.f, PW, 60.f, 44.f, FLinearColor::White);
		CText(TEXT("Emotes are earned, not bought: level up a Zoogi's affinity by playing with it\n(affinity level 2 unlocks its wave emote). Earned emotes show up here."), PX + 50.f, PY + 330.f, PW - 100.f, 120.f, 24.f, ZCSoft, 1, 3.f, true);
		CText(TEXT("Sending emotes in battle needs the battle HUD hook (later milestone)."), PX + 50.f, PY + PH - 100.f, PW - 100.f, 50.f, 20.f, ZCMuted, 1, 3.f, true);
		return;
	}
	const float TW = 230.f, TH = 260.f, Gap = 24.f;
	const int32 Cols = FMath::Max(1, FMath::FloorToInt((W - 96.f + Gap) / (TW + Gap)));
	const float X0 = C - (FMath::Min(Cols, Emotes.Num()) * (TW + Gap) - Gap) * 0.5f;
	for (int32 i = 0; i < Emotes.Num(); ++i)
	{
		const FString S = Emotes[i].ToString();
		FString Zoogi = S; Zoogi.RemoveFromStart(TEXT("emote_")); int32 Us = INDEX_NONE;
		if (Zoogi.FindLastChar(TEXT('_'), Us)) Zoogi = Zoogi.Left(Us);
		FString Name = S;
		if (PS) for (const FZoogiCosmeticRow& R : PS->Core().Config().Cosmetics) if (R.Id == Emotes[i] && !R.Name.IsEmpty()) Name = R.Name;
		const float X = X0 + (i % Cols) * (TW + Gap), Y = 230.f + (i / Cols) * (TH + Gap);
		CPlate(X, Y, TW, TH, ZCNavy, 22.f, 5.f, true);
		CImg(Art(PortraitFor(FName(*Zoogi))), X + TW * 0.5f - 70.f, Y + 20.f, 140.f, 140.f, 70.f, true);
		CImg(ClashArt(TEXT("emote")), X + TW - 90.f, Y + 10.f, 74.f, 74.f);
		CText(Name.ToUpper(), X + 8.f, Y + 170.f, TW - 16.f, 70.f, 20.f, FLinearColor::White, 1, 3.f, true);
	}
}

// ------------------------------------------------------------------------------------------------ popups
bool UZoogiMenuRoot::BuildClashShopPopup(EZoogiClashPopup P, float PW, float PH)
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return false;
	const bool bBuying = ClashBuyT0 >= 0.f;
	auto Rays = [this](float CX, float CY, float S)
	{
		UWidget* R = CImg(ClashArt(TEXT("rays")), CX - S * 0.5f, CY - S * 0.5f, S, S);
		if (R) { R->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); ClashRaysW = R; }
	};
	auto PriceRow = [this](UTexture2D* Icon, const FString& Txt, float Size)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (Icon)
		{
			UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			I->SetBrush(ZCImgBrush(Icon, Size * 1.4f, Size * 1.4f, 0.f, false));
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f)); }
		}
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(Txt, Size, FLinearColor::White, 5.f))) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		return H;
	};
	// big green buy button, or the spinner while a purchase is going through
	auto BuyButton = [&](UWidget* Inner, TFunction<void()> Fn, float Y)
	{
		const float BW = 400.f, BH = 112.f;
		if (bBuying)
		{
			CPlate(PW * 0.5f - BW * 0.5f, Y, BW, BH, ZCHex(TEXT("3A8A2A")), 26.f, 5.f, false);
			UWidget* Sp = CImg(ClashArt(TEXT("spinner")), PW * 0.5f - 44.f, Y + 12.f, 88.f, 88.f);
			if (Sp) { Sp->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); ClashSpinW = Sp; }
			return;
		}
		CChunkyW(Inner, ZCGreen, MoveTemp(Fn), PW * 0.5f - BW * 0.5f, Y, BW, BH, true);
	};

	if (P == EZoogiClashPopup::Deal)
	{
		const int32 Idx = ClashSel;
		if (!FP->Data().Deals.IsValidIndex(Idx)) return false;
		const FZoogiFrontDeal Deal = FP->Data().Deals[Idx];
		const int32 Rar = ZCRarity(Deal.CardId);
		Rays(PW * 0.5f, 320.f, 520.f);
		CCard(Deal.CardId, PW * 0.5f - 120.f, 170.f, 240.f, 300.f, false, FString::Printf(TEXT("x%d"), Deal.Count));
		CText(ZCCardName(Deal.CardId), 0.f, 486.f, PW, 50.f, 40.f, FLinearColor::White);
		CText(UZoogiFrontProfileSubsystem::RarityName(Rar), 0.f, 532.f, PW, 32.f, 24.f, ZCLighten(ZCRarityColor(Rar), 0.2f), 1, 3.f);
		CText(FString::Printf(TEXT("Adds %d copies of this card to your collection."), Deal.Count), 40.f, 566.f, PW - 80.f, 32.f, 22.f, ZCSoft, 1, 3.f);
		const int32 Have = FP->Copies(Deal.CardId);
		int32 Into = 0, Need = 1, Into2 = 0, Need2 = 1;
		const int32 Lv = UZoogiFrontProfileSubsystem::CardLevel(Have, Into, Need);
		const int32 Lv2 = UZoogiFrontProfileSubsystem::CardLevel(Have + Deal.Count, Into2, Need2);
		CText(FString::Printf(TEXT("LV %d"), Lv), PW * 0.5f - 330.f, 606.f, 100.f, 44.f, 24.f, ZCGold, 1, 3.f);
		CBar(PW * 0.5f - 220.f, 606.f, 440.f, 44.f, Lv2 > Lv ? 1.f : float(Into + Deal.Count) / FMath::Max(1, Need), ZCGreen,
			Lv2 > Lv ? FString::Printf(TEXT("LEVEL UP!  %d/%d -> LV %d"), FMath::Min(Into + Deal.Count, Need), Need, Lv2) : FString::Printf(TEXT("%d/%d  (+%d)"), Into + Deal.Count, Need, Deal.Count), 22.f);
		UWidget* Inner = Deal.bFree ? (UWidget*)CTxt(TEXT("FREE"), 44.f, FLinearColor::White, 5.f) : (UWidget*)PriceRow(Art(TEXT("icon_coin")), ZCNum(Deal.Price), 42.f);
		BuyButton(Inner, [this, Idx, Deal]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			if (F->Data().Gold < Deal.Price) { ClashFail(FString::Printf(TEXT("Not enough gold: this deal costs %s.\nWin battles or open chests for more gold."), *ZCNum(Deal.Price))); return; }
			ClashBuy([this, Idx]()
			{
				FString Why;
				if (UZoogiFrontProfileSubsystem* F2 = UZoogiFrontProfileSubsystem::Get(this)) if (!F2->BuyDeal(Idx, Why)) ClashFail(Why);
			});
		}, PH - 160.f);
		return true;
	}
	if (P == EZoogiClashPopup::Chest)
	{
		const FZoogiFrontChestDef* Def = UZoogiFrontProfileSubsystem::FindChest(ClashSelId);
		if (!Def) return false;
		Rays(PW * 0.5f, 290.f, 500.f);
		CImg(ClashArt(TEXT("chest_") + Def->Id.ToString()), PW * 0.5f - 140.f, 150.f, 280.f, 280.f);
		CText(FString::Printf(TEXT("%d cards  -  %d-%d gold"), Def->Cards, Def->GoldMin, Def->GoldMax), 0.f, 450.f, PW, 44.f, 32.f, FLinearColor::White);
		CText(FString::Printf(TEXT("Common %d%%   Rare %d%%   Epic %d%%"), Def->WCommon, Def->WRare, Def->WEpic), 0.f, 498.f, PW, 34.f, 24.f, ZCSoft, 1, 3.f);
		CText(TEXT("Cards come from your unlocked cards. Opens right away."), 40.f, 536.f, PW - 80.f, 32.f, 20.f, ZCMuted, 1, 3.f);
		const FName Id = Def->Id; const int32 Cost = Def->GemCost; const FString Name = FString(Def->Name).ToUpper();
		BuyButton(PriceRow(Art(TEXT("icon_gem")), ZCNum(Cost), 42.f), [this, Id, Cost, Name]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			if (F->Gems() < Cost) { ClashFail(FString::Printf(TEXT("Not enough gems: this chest costs %d.\nYou have %d."), Cost, F->Gems())); return; }
			ClashBuy([this, Id, Name]()
			{
				TArray<FZoogiFrontReward> R; FString Why;
				UZoogiFrontProfileSubsystem* F2 = UZoogiFrontProfileSubsystem::Get(this);
				if (F2 && F2->BuyChest(Id, R, Why)) ClashShowRewards(Name, Id, R);
				else ClashFail(Why);
			});
		}, PH - 170.f);
		return true;
	}
	if (P == EZoogiClashPopup::Gold)
	{
		if (!UZoogiFrontProfileSubsystem::GoldPacks().IsValidIndex(ClashSel)) return false;
		const UZoogiFrontProfileSubsystem::FGoldPack Pack = UZoogiFrontProfileSubsystem::GoldPacks()[ClashSel];
		Rays(PW * 0.5f, 250.f, 420.f);
		CImg(ClashArt(TEXT("goldpile")), PW * 0.5f - 110.f, 140.f, 220.f, 220.f);
		CText(FString::Printf(TEXT("%s GOLD"), *ZCNum(Pack.Gold)), 0.f, 370.f, PW, 56.f, 44.f, ZCGold);
		CText(TEXT("Gold buys daily deals (card upgrades come later)."), 40.f, 424.f, PW - 80.f, 32.f, 20.f, ZCSoft, 1, 3.f);
		const int32 Idx = ClashSel;
		BuyButton(PriceRow(Art(TEXT("icon_gem")), ZCNum(Pack.Gems), 42.f), [this, Idx, Pack]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			if (F->Gems() < Pack.Gems) { ClashFail(FString::Printf(TEXT("Not enough gems: this costs %d.\nYou have %d."), Pack.Gems, F->Gems())); return; }
			ClashBuy([this, Idx]() { FString Why; if (UZoogiFrontProfileSubsystem* F2 = UZoogiFrontProfileSubsystem::Get(this)) if (!F2->BuyGold(Idx, Why)) ClashFail(Why); });
		}, PH - 160.f);
		return true;
	}
	if (P == EZoogiClashPopup::Rewards)
	{
		if (!ClashRewardsChest.IsNone())
		{
			Rays(PW * 0.5f, 210.f, 360.f);
			CImg(ClashArt(TEXT("chest_") + ClashRewardsChest.ToString()), PW * 0.5f - 85.f, 125.f, 170.f, 170.f);
		}
		int32 Gold = 0; TArray<FZoogiFrontReward> Cards;
		for (const FZoogiFrontReward& R : ClashRewards) { if (R.Kind == FName(TEXT("gold"))) Gold += R.Amount; else if (R.Kind == FName(TEXT("card"))) Cards.Add(R); }
		{
			UWidget* Row = PriceRow(Art(TEXT("icon_coin")), FString::Printf(TEXT("+%s"), *ZCNum(Gold)), 36.f);
			UBorder* B = CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f);
			B->SetHorizontalAlignment(HAlign_Center); B->SetVerticalAlignment(VAlign_Center); B->SetContent(Row);
			CAt(B, 0.f, 300.f, PW, 60.f);
		}
		const int32 Cols = 7, Shown = FMath::Min(Cards.Num(), 14);
		const float CW = 120.f, CH = 150.f, G = 16.f;
		for (int32 i = 0; i < Shown; ++i)
		{
			const int32 RowN = FMath::Min(Cols, Shown - (i / Cols) * Cols);
			const float X0 = PW * 0.5f - (RowN * (CW + G) - G) * 0.5f;
			CCard(Cards[i].Id, X0 + (i % Cols) * (CW + G), 378.f + (i / Cols) * (CH + 16.f), CW, CH, false, FString::Printf(TEXT("x%d"), Cards[i].Amount));
		}
		if (Cards.Num() == 0) CText(TEXT("No unlocked cards to give: gold only."), 0.f, 420.f, PW, 40.f, 24.f, ZCSoft, 1, 3.f);
		CChunky(TEXT("COLLECT"), ZCYellow, [this]()
		{
			const EZoogiClashPopup Back = ClashAfterRewards; // e.g. back to the Battle Pass / Trophy Road after a claim
			if (Back != EZoogiClashPopup::None) OpenClashPopup(Back, Back == EZoogiClashPopup::BattlePass ? TEXT("BATTLE PASS") : TEXT("TROPHY ROAD"));
			else { ClashPopup = EZoogiClashPopup::None; bDirty = true; }
		}, PW * 0.5f - 170.f, PH - 128.f, 340.f, 100.f, 40.f, nullptr, true);
		return true;
	}
	if (P == EZoogiClashPopup::Card)
	{
		const FZoogiCardDef* Def = UZoogiFrontProfileSubsystem::FindCard(ClashSelId);
		if (!Def) return false;
		const bool bOwned = FP->IsCardOwned(Def->Id);
		const int32 Rar = UZoogiFrontProfileSubsystem::RarityOf(*Def);
		CCard(Def->Id, 60.f, 150.f, 300.f, 375.f, !bOwned);
		const float X = 400.f, TW = PW - X - 50.f;
		CText(FString::Printf(TEXT("%s  -  %s"), UZoogiFrontProfileSubsystem::RarityName(Rar), ZoogiCards::ElementName(Def->Element)), X, 146.f, TW, 40.f, 28.f, ZCLighten(ZCRarityColor(Rar), 0.2f), 0, 3.f);
		CText(FString::Printf(TEXT("%s card  -  costs %d energy"), ZoogiCards::TypeName(Def->Type), Def->Cost), X, 190.f, TW, 34.f, 22.f, ZCSoft, 0, 3.f);
		CPlate(X, 234.f, TW, 150.f, ZCNavyDeep.CopyWithNewOpacity(0.85f), 16.f, 4.f, false);
		CText(Def->Rule, X + 20.f, 244.f, TW - 40.f, 130.f, 22.f, FLinearColor::White, 0, 2.f, true);
		if (bOwned)
		{
			int32 Into = 0, Need = 1;
			const int32 Copies = FP->Copies(Def->Id);
			const int32 Lv = UZoogiFrontProfileSubsystem::CardLevel(Copies, Into, Need);
			CText(FString::Printf(TEXT("LEVEL %d"), Lv), X, 402.f, 200.f, 44.f, 30.f, ZCGold, 0, 3.f);
			CBar(X + 190.f, 404.f, TW - 190.f, 42.f, float(Into) / FMath::Max(1, Need), ZCGreen, FString::Printf(TEXT("%d/%d"), Into, Need), 22.f);
			CText(FString::Printf(TEXT("You have %d copies. Copies come from daily deals and chests."), Copies), X, 458.f, TW, 34.f, 19.f, ZCSoft, 0, 2.f, true);
			CText(TEXT("Upgrading with gold arrives in a later milestone."), X, 494.f, TW, 30.f, 18.f, ZCMuted, 0, 2.f);
		}
		else
		{
			CImg(ClashArt(TEXT("lock")), X, 400.f, 64.f, 64.f);
			CText(TEXT("LOCKED"), X + 80.f, 404.f, TW - 80.f, 56.f, 34.f, ZCMuted, 0, 3.f);
			CText(FString::Printf(TEXT("Unlock: %s"), *FP->LockText(Def->Id)), X, 470.f, TW, 64.f, 22.f, FLinearColor::White, 0, 2.f, true);
		}
		CChunky(TEXT("OK"), ZCYellow, [this]() { ClashPopup = EZoogiClashPopup::None; bDirty = true; }, PW * 0.5f - 150.f, PH - 132.f, 300.f, 100.f, 40.f);
		return true;
	}
	if (P == EZoogiClashPopup::MatchResult)
	{
		const FZoogiFrontMatchResult& M = FP->Data().LastMatch;
		const TCHAR* Head = M.Outcome == 0 ? TEXT("VICTORY!") : M.Outcome == 1 ? TEXT("DEFEAT") : M.Outcome == 2 ? TEXT("DRAW") : TEXT("YOU LEFT THE MATCH");
		const FLinearColor HeadC = M.Outcome == 0 ? ZCYellow : M.Outcome == 1 ? ZCHex(TEXT("FF7A70")) : ZCSoft;
		// the title plate says MATCH RESULT; the headline is the outcome
		CImg(ClashArt(M.Outcome == 0 ? TEXT("crown") : TEXT("swords")), PW * 0.5f - 60.f, 122.f, 120.f, 120.f);
		CText(Head, 0.f, 236.f, PW, 70.f, M.Outcome == 3 ? 40.f : 58.f, HeadC);
		struct FRow { UTexture2D* Icon; FString Label; FString Value; FLinearColor C; };
		TArray<FRow> Rows;
		Rows.Add({ ClashArt(TEXT("trophy")), TEXT("TROPHIES"), FString::Printf(TEXT("%+d"), M.Trophies), M.Trophies >= 0 ? ZCGold : ZCHex(TEXT("FF7A70")) });
		if (M.Outcome != 3)
		{
			Rows.Add({ Art(TEXT("icon_coin")), TEXT("GOLD"), FString::Printf(TEXT("+%d"), M.Gold), ZCGold });
			Rows.Add({ ClashArt(TEXT("level")), M.NewLevel > 0 ? FString::Printf(TEXT("XP  -  LEVEL UP! %d"), M.NewLevel) : FString(TEXT("XP")), FString::Printf(TEXT("+%d"), M.XP), ZCCyan });
			Rows.Add({ ClashArt(TEXT("crown")), TEXT("BATTLE PASS XP"), FString::Printf(TEXT("+%d"), M.BP), ZCGreen });
		}
		if (!M.Chest.IsNone()) Rows.Add({ ClashArt(TEXT("chest_") + M.Chest.ToString()), FString::Printf(TEXT("%s ADDED"), *ZCChestName(M.Chest)), TEXT("NEW"), ZCYellow });
		else if (M.bSlotsFull) Rows.Add({ ClashArt(TEXT("chest")), TEXT("CHEST SLOTS FULL"), TEXT("-"), ZCMuted });
		const float RW = 640.f, RH = 66.f;
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const float Y = 318.f + r * (RH + 10.f);
			CPlate(PW * 0.5f - RW * 0.5f, Y, RW, RH, ZCNavyDeep.CopyWithNewOpacity(0.85f), 16.f, 4.f, false);
			CImg(Rows[r].Icon, PW * 0.5f - RW * 0.5f + 12.f, Y + 5.f, 56.f, 56.f);
			CText(Rows[r].Label, PW * 0.5f - RW * 0.5f + 84.f, Y, RW - 250.f, RH, 24.f, FLinearColor::White, 0, 3.f);
			CText(Rows[r].Value, PW * 0.5f + RW * 0.5f - 160.f, Y, 140.f, RH, 32.f, Rows[r].C, 2, 3.f);
		}
		CChunky(TEXT("OK"), ZCYellow, [this]() { ClashPopup = EZoogiClashPopup::None; bDirty = true; }, PW * 0.5f - 150.f, PH - 124.f, 300.f, 100.f, 44.f, nullptr, true);
		return true;
	}
	if (P == EZoogiClashPopup::ChestSlot)
	{
		const int32 SlotIdx = ClashSel;
		if (!FP->Data().Chests.IsValidIndex(SlotIdx) || FP->Data().Chests[SlotIdx].Type.IsNone()) return false;
		const FName Type = FP->Data().Chests[SlotIdx].Type;
		const int64 Left = FP->SecondsLeft(SlotIdx);
		Rays(PW * 0.5f, 270.f, 440.f);
		CImg(ClashArt(TEXT("chest_") + Type.ToString()), PW * 0.5f - 120.f, 150.f, 240.f, 240.f);
		const FZoogiFrontChestDef* Def = UZoogiFrontProfileSubsystem::FindChest(Type);
		if (Def) CText(FString::Printf(TEXT("%d cards  -  %d-%d gold"), Def->Cards, Def->GoldMin, Def->GoldMax), 0.f, 400.f, PW, 40.f, 28.f, FLinearColor::White);
		TWeakObjectPtr<UZoogiFrontProfileSubsystem> WFP(FP);
		const float BY = PH - 170.f;
		if (Left < 0)
		{
			const bool bBusy = FP->AnyUnlocking();
			CText(bBusy ? TEXT("Another chest is unlocking. One at a time!") : *FString::Printf(TEXT("Unlocks in %s once started."), *ZCDur(FP->ChestSeconds(Type))), 0.f, 448.f, PW, 36.f, 22.f, bBusy ? ZCYellow : ZCSoft, 1, 3.f);
			CChunky(TEXT("START UNLOCK"), bBusy ? ZCHex(TEXT("4A5675")) : ZCGreen, [this, SlotIdx]()
			{
				FString Why;
				UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
				if (F && F->StartUnlock(SlotIdx, Why)) { UE_LOG(LogZoogi, Log, TEXT("CLASH chest slot %d unlocking"), SlotIdx); bClashChestWasUnlocking = true; ClashPopup = EZoogiClashPopup::None; bDirty = true; }
				else ClashFail(Why);
			}, 60.f, BY, 380.f, 112.f, 34.f, nullptr, !bBusy);
		}
		else
			CLive([WFP, SlotIdx]() { const UZoogiFrontProfileSubsystem* F = WFP.Get(); return F ? FString::Printf(TEXT("Unlocking: %s left"), *ZCDur(F->SecondsLeft(SlotIdx))) : FString(); }, 0.f, 448.f, PW, 40.f, 28.f, ZCGold);
		const int32 Cost = FP->OpenNowCost(SlotIdx);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(TEXT("OPEN NOW"), 30.f, FLinearColor::White, 4.f))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f)); }
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(Art(TEXT("icon_gem")), 44.f, 44.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(FString::FromInt(Cost), 30.f, FLinearColor::White, 4.f))) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(H, ZCBlue, [this, SlotIdx, Type]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			TArray<FZoogiFrontReward> R; FString Why;
			if (F->OpenChest(SlotIdx, true, R, Why)) ClashShowRewards(ZCChestName(Type), Type, R);
			else ClashFail(Why);
		}, Left < 0 ? PW - 60.f - 380.f : PW * 0.5f - 190.f, BY, 380.f, 112.f);
		return true;
	}
	return false;
}
