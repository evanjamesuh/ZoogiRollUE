// clash-menu (Oct 17, DETAILED PASS 1): the Home system screens behind the Battle tab.
// - ARENAS (tap the arena preview): trophy track 0..1800 in steps of 100; arena tiles at each gate (Arena N+1 at N*300,
//   Arena 2 = 300 as recorded), locked marker tiles in between (tap only selects).
// - TROPHY ROAD (tap the trophy bar / Trophy Road button): the exact 0-5000 milestone table, claimable, arena gates.
// - BATTLE PASS (tap the Battle Pass card): Season 8, 35 tiers, exact free / premium table, Unlock Premium (500 gems
//   from the progression gem ledger).
// Look: sky banner header + coloured ribbon (gold / purple / blue), #2F7FE0 road cards, dark-navy medallions.
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

namespace
{
	FString ZHDays(int64 Secs)
	{
		Secs = FMath::Max<int64>(0, Secs);
		if (Secs >= 86400) Secs += 3599; // round hours up: a fresh season reads "28d 11h"
		const int64 Dd = Secs / 86400, Hh = (Secs / 3600) % 24, Mm = (Secs / 60) % 60;
		if (Dd > 0) return FString::Printf(TEXT("%lldd %lldh"), Dd, Hh);
		return FString::Printf(TEXT("%lldh %02lldm"), Hh, Mm);
	}
	FString ZHNum(int32 V) { return FText::AsNumber(V).ToString(); }
	const TCHAR* ZHChestName(FName C) { return C == FName(TEXT("epic")) ? TEXT("EPIC CHEST") : C == FName(TEXT("royal")) ? TEXT("ROYAL CHEST") : TEXT("WOODEN CHEST"); }
}

bool UZoogiMenuRoot::ClashBigPopup(EZoogiClashPopup P) const
{
	return P == EZoogiClashPopup::Arenas || P == EZoogiClashPopup::TrophyRoad || P == EZoogiClashPopup::BattlePass;
}

bool UZoogiMenuRoot::BuildClashHomePopup(EZoogiClashPopup P, float PW, float PH)
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return false;
	const FZoogiFrontProfileData& D = FP->Data();

	// scroll body inside the popup; keeps its offset across rebuilds (claims), starts at InitialY when first opened
	auto Scroll = [this](UCanvasPanel* Content, float ContentH, float X, float Y, float W, float H, int32 Key, float InitialY)
	{
		if (UScrollBox* Old = ClashScrollW.Get()) if (ClashScrollKey == Key && ClashPendingScroll < 0.f) ClashScrollOffset = Old->GetScrollOffset();
		if (ClashScrollKey != Key) { ClashScrollOffset = FMath::Clamp(InitialY, 0.f, FMath::Max(0.f, ContentH - H)); ClashPendingScroll = -1.f; }
		ClashScrollKey = Key;
		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SB->SetWidthOverride(W); SB->SetHeightOverride(ContentH); SB->AddChild(Content);
		UScrollBox* Sc = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Sc->SetAnimateWheelScrolling(true);
		Sc->SetScrollBarVisibility(ESlateVisibility::Visible);
		Sc->AddChild(SB);
		CAt(Sc, X, Y, W, H);
		ClashScrollW = Sc;
		if (ClashPendingScroll < 0.f) ClashPendingScroll = ClashScrollOffset;
		Sc->SetScrollOffset(ClashPendingScroll);
	};
	auto TrophyPill = [this](float X, float Y, int32 Value)
	{
		CPlate(X + 30.f, Y + 6.f, 200.f, 56.f, ZCNavyDeep.CopyWithNewOpacity(0.9f), 28.f, 4.f, false);
		CText(ZHNum(Value), X + 80.f, Y + 6.f, 140.f, 56.f, 30.f, ZCGold);
		CImg(ClashArt(TEXT("trophy")), X, Y - 4.f, 76.f, 76.f);
	};
	auto Medallion = [this](float CX, float CY, float S, const FString& Num, bool bDone, float Font)
	{
		CAt(CBox(bDone ? FLinearColor::White : ZCNavyDeep, ZCDark, 4.f, S * 0.5f), CX - S * 0.5f, CY - S * 0.5f, S, S);
		CText(Num, CX - S * 0.5f, CY - S * 0.5f, S, S - 2.f, Font, bDone ? ZCDark : FLinearColor::White, 1, bDone ? 0.f : 2.f);
	};
	auto RewardIcon = [this, FP](const FZoogiFrontRoadReward& R, float X, float Y, float S, int32 Salt)
	{
		if (R.Kind == FName(TEXT("gold"))) CImg(Art(TEXT("icon_coin")), X, Y, S, S);
		else if (R.Kind == FName(TEXT("chest"))) CImg(ClashArt(TEXT("chest_") + R.Chest.ToString()), X, Y, S, S);
		else CCard(FP->HeroCardFor(Salt), X + S * 0.12f, Y, S * 0.76f, S * 0.95f, false);
	};
	auto RewardText = [FP](const FZoogiFrontRoadReward& R, int32 Salt) -> FString
	{
		if (R.Kind == FName(TEXT("gold"))) return FString::Printf(TEXT("%s GOLD"), *ZHNum(R.Amount));
		if (R.Kind == FName(TEXT("chest"))) return ZHChestName(R.Chest);
		const FZoogiCardDef* C = UZoogiFrontProfileSubsystem::FindCard(FP->HeroCardFor(Salt));
		return FString::Printf(TEXT("%s x%d"), C ? *C->Name.ToUpper() : TEXT("CARD"), R.Amount);
	};

	// ================================================================================================ TROPHY ROAD
	if (P == EZoogiClashPopup::TrophyRoad)
	{
		const int32 Best = FMath::Max(D.Trophies, D.BestTrophies);
		TrophyPill(PW * 0.5f - 130.f, 212.f, D.Trophies);
		const float BX = 30.f, BY = 290.f, BW = PW - 60.f, BH = PH - BY - 26.f;
		// merged list: milestones + arena gates (gate row before the milestone with the same trophy count)
		struct FRowE { int32 Trophies; int32 Milestone; int32 Arena; };
		TArray<FRowE> Rows;
		const TArray<FZoogiFrontRoadReward>& Road = UZoogiFrontProfileSubsystem::TrophyRoad();
		for (int32 i = 0; i < Road.Num(); ++i) Rows.Add({ Road[i].Trophies, i, -1 });
		for (int32 a = 1; a < UZoogiFrontProfileSubsystem::NumTrophyArenas; ++a) Rows.Add({ UZoogiFrontProfileSubsystem::ArenaTrophies(a), -1, a });
		Rows.StableSort([](const FRowE& A, const FRowE& B) { return A.Trophies != B.Trophies ? A.Trophies < B.Trophies : A.Arena > B.Arena; });
		UCanvasPanel* G = CGroupBegin();
		const float TX = 90.f, RowH = 116.f, GateH = 150.f, CardX = TX + 64.f, CardW = BW - CardX - 20.f; // M4 portrait column
		float Y = 20.f, InitialY = 0.f;
		TArray<float> Centers;
		for (const FRowE& R : Rows) { Centers.Add(Y + (R.Arena >= 0 ? GateH : RowH) * 0.5f); Y += (R.Arena >= 0 ? GateH : RowH) + 12.f; }
		const float ContentH = Y + 20.f;
		// track: grey-blue line, yellow where progressed
		for (int32 r = 0; r + 1 < Rows.Num(); ++r)
		{
			const bool bProg = Best >= Rows[r + 1].Trophies;
			CAt(CBox(bProg ? ZCYellow : ZCHex(TEXT("4A4D55")), ZCDark, 3.f, 6.f), TX - 9.f, Centers[r], 18.f, Centers[r + 1] - Centers[r]);
		}
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const FRowE& R = Rows[r];
			const float CY = Centers[r];
			const bool bReached = Best >= R.Trophies;
			if (bReached) InitialY = FMath::Max(0.f, CY - BH * 0.4f);
			if (R.Arena >= 0)
			{
				// arena gate
				const float GY = CY - GateH * 0.5f;
				CGoldPanel(CardX, GY, CardW, GateH, bReached ? ZCRibBlue : ZCHex(TEXT("3A3D45")), 22.f);
				UTexture2D* Map = Art(FString(ZCArenaSlug(R.Arena)) + TEXT("_map"));
				CImg(Map, CardX + 16.f, GY + 16.f, 210.f, GateH - 32.f, 10.f, true, bReached ? FLinearColor::White : FLinearColor(0.4f, 0.42f, 0.5f, 1.f));
				CText(FString::Printf(TEXT("ARENA %d  -  %s"), R.Arena + 1, *ZoogiUtil::ArenaName((EZoogiArena)R.Arena).ToUpper()), CardX + 246.f, GY + 22.f, CardW - 270.f, 50.f, 28.f, FLinearColor::White, 0);
				CText(bReached ? FString(TEXT("UNLOCKED")) : FString::Printf(TEXT("Reach %d trophies"), R.Trophies), CardX + 246.f, GY + 76.f, CardW - 270.f, 40.f, 24.f, bReached ? ZCGold : ZCSoft, 0, 3.f);
				if (!bReached) CImg(ClashArt(TEXT("lock")), CardX + CardW - 96.f, GY + GateH * 0.5f - 36.f, 72.f, 72.f, 0.f, false, FLinearColor(0.75f, 0.78f, 0.85f, 1.f));
				Medallion(TX, CY, 82.f, ZHNum(R.Trophies), bReached, R.Trophies >= 1000 ? 20.f : 24.f);
				continue;
			}
			const int32 i = R.Milestone;
			const FZoogiFrontRoadReward& M = Road[i];
			const bool bClaimed = FP->IsRoadClaimed(i), bClaimable = FP->IsRoadClaimable(i);
			const float RY = CY - RowH * 0.5f;
			CPlate(CardX, RY, CardW, RowH, bClaimed ? ZCDarken(ZCRoadCard, 0.25f) : ZCRoadCard, 18.f, 4.f, !bClaimed);
			RewardIcon(M, CardX + 18.f, RY + 12.f, RowH - 24.f, 0);
			CText(RewardText(M, 0), CardX + RowH + 10.f, RY, CardW - RowH - 300.f, RowH, 32.f, M.Kind == FName(TEXT("gold")) ? ZCGold : FLinearColor::White, 0);
			if (bClaimable)
				CChunky(TEXT("CLAIM"), ZCGreen, [this, i]()
				{
					UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
					TArray<FZoogiFrontReward> Out; FString Why;
					if (!F || !F->ClaimRoad(i, Out, Why)) { ClashFail(Why); return; }
					if (F->TrophyRoad()[i].Kind == FName(TEXT("chest"))) { ClashAfterRewards = EZoogiClashPopup::TrophyRoad; ClashShowRewards(ZHChestName(F->TrophyRoad()[i].Chest), F->TrophyRoad()[i].Chest, Out); }
					else bDirty = true;
				}, CardX + CardW - 250.f, RY + 16.f, 230.f, RowH - 32.f, 32.f, nullptr, true);
			else if (bClaimed) CText(TEXT("CLAIMED"), CardX + CardW - 260.f, RY, 240.f, RowH, 26.f, ZCSoft, 1, 3.f);
			else CImg(ClashArt(TEXT("lock")), CardX + CardW - 100.f, RY + RowH * 0.5f - 34.f, 68.f, 68.f, 0.f, false, FLinearColor(0.72f, 0.75f, 0.82f, 1.f)); // grey padlock
			Medallion(TX, CY, 74.f, ZHNum(R.Trophies), bReached, R.Trophies >= 1000 ? 18.f : 22.f);
		}
		CGroupEnd();
		Scroll(G, ContentH, BX, BY, BW, BH, 10, InitialY);
		return true;
	}

	// ================================================================================================ ARENAS
	if (P == EZoogiClashPopup::Arenas)
	{
		const int32 Here = UZoogiFrontProfileSubsystem::ArenaForTrophies(D.Trophies);
		TrophyPill(40.f, 212.f, D.Trophies);
		CText(FString::Printf(TEXT("Arena %d  -  %s"), Here + 1, *ZoogiUtil::ArenaName((EZoogiArena)Here)), 290.f, 212.f, PW - 330.f, 64.f, 30.f, FLinearColor::White, 0);
		const float BX = 30.f, BY = 290.f, BW = PW - 60.f, BH = PH - BY - 26.f;
		const int32 MaxT = UZoogiFrontProfileSubsystem::ArenaTrophies(UZoogiFrontProfileSubsystem::NumTrophyArenas - 1);
		const TArray<FZoogiCardDef>& Defs = UZoogiFrontProfileSubsystem::CardDefs();
		UCanvasPanel* G = CGroupBegin();
		const float TX = 90.f, TileX = TX + 80.f, TileW = BW - TileX - 20.f, ArenaH = 300.f, SmallH = 104.f, MapW = 320.f, TL = MapW + 36.f; // M4 portrait
		float Y = 20.f, InitialY = 0.f;
		TArray<float> Centers; TArray<int32> Marks;
		for (int32 t = 0; t <= MaxT; t += 100) { const bool bArena = t % 300 == 0; Marks.Add(t); Centers.Add(Y + (bArena ? ArenaH : SmallH) * 0.5f); Y += (bArena ? ArenaH : SmallH) + 14.f; }
		const float ContentH = Y + 20.f;
		for (int32 r = 0; r + 1 < Marks.Num(); ++r)
			CAt(CBox(D.Trophies >= Marks[r + 1] ? ZCYellow : ZCHex(TEXT("4A4D55")), ZCDark, 3.f, 6.f), TX - 9.f, Centers[r], 18.f, Centers[r + 1] - Centers[r]);
		for (int32 r = 0; r < Marks.Num(); ++r)
		{
			const int32 T = Marks[r];
			const float CY = Centers[r];
			const bool bOpen = D.Trophies >= T;
			const bool bSel = ClashArenaSel == T;
			// trophy marker
			CImg(ClashArt(TEXT("trophy")), TX - 34.f, CY - 46.f, 68.f, 68.f);
			CText(ZHNum(T), TX - 60.f, CY + 16.f, 120.f, 34.f, 22.f, bOpen ? ZCGold : FLinearColor::White);
			if (T % 300 == 0)
			{
				const int32 A = T / 300;
				const float TY = CY - ArenaH * 0.5f;
				UCanvasPanel* Tile = CGroupBegin();
				CGoldPanel(0.f, 0.f, TileW, ArenaH, bOpen ? ZCRibBlue : ZCHex(TEXT("3A3D45")), 24.f);
				if (bSel) CAt(CBox(FLinearColor::Transparent, ZCYellow, 6.f, 24.f), -4.f, -4.f, TileW + 8.f, ArenaH + 8.f);
				CImg(Art(FString(ZCArenaSlug(A)) + TEXT("_map")), 18.f, 18.f, MapW, ArenaH - 36.f, 14.f, true, bOpen ? FLinearColor::White : FLinearColor(0.38f, 0.4f, 0.48f, 1.f));
				if (!bOpen) CImg(ClashArt(TEXT("lock")), 18.f + MapW * 0.5f - 50.f, ArenaH * 0.5f - 50.f, 100.f, 100.f);
				CText(FString::Printf(TEXT("ARENA %d"), A + 1), TL, 22.f, TileW - TL - 20.f, 40.f, 26.f, ZCSoft, 0, 3.f);
				CText(ZoogiUtil::ArenaName((EZoogiArena)A).ToUpper(), TL, 58.f, TileW - TL - 20.f, 54.f, 34.f, FLinearColor::White, 0);
				if (A == Here) { CPlate(TL, 118.f, 260.f, 50.f, ZCGreen, 14.f, 4.f, true); CText(TEXT("You are here"), TL, 118.f, 260.f, 50.f, 24.f, FLinearColor::White); }
				else CText(bOpen ? FString(TEXT("UNLOCKED")) : FString::Printf(TEXT("Reach %d trophies"), T), TL, 118.f, TileW - TL - 20.f, 50.f, 24.f, bOpen ? ZCGold : ZCSoft, 0, 3.f);
				// new cards in this arena (display order of Cards.csv: 6 in Arena 1, then 1 per arena; PROPOSED until cards carry an arena)
				const int32 First = A == 0 ? 0 : 5 + A, Count = A == 0 ? 6 : 1;
				CText(Count > 1 ? TEXT("NEW CARDS") : TEXT("NEW CARD"), TL, 176.f, 200.f, 30.f, 20.f, ZCSoft, 0, 3.f);
				for (int32 c = 0; c < Count && Defs.IsValidIndex(First + c); ++c)
					CCard(Defs[First + c].Id, TL + c * 68.f, 206.f, 60.f, 76.f, !bOpen);
				CGroupEnd();
				CAt(CButton(Tile, [this, T]() { ClashArenaSel = T; bDirty = true; }), TileX, TY, TileW, ArenaH);
				if (A == Here) InitialY = FMath::Max(0.f, TY - 40.f);
			}
			else
			{
				// locked marker tile between gates: tapping only selects it
				const float TY = CY - SmallH * 0.5f;
				UCanvasPanel* Tile = CGroupBegin();
				CPlate(0.f, 0.f, TileW, SmallH, bOpen ? ZCRoadCard : ZCHex(TEXT("3A3D45")), 18.f, 4.f, true);
				if (bSel) CAt(CBox(FLinearColor::Transparent, ZCYellow, 5.f, 18.f), -4.f, -4.f, TileW + 8.f, SmallH + 8.f);
				CImg(ClashArt(TEXT("lock")), 24.f, SmallH * 0.5f - 32.f, 64.f, 64.f, 0.f, false, FLinearColor(0.72f, 0.75f, 0.82f, 1.f));
				CText(FString::Printf(TEXT("%d TROPHIES"), T), 110.f, 0.f, TileW - 140.f, SmallH, 28.f, bOpen ? FLinearColor::White : ZCSoft, 0, 3.f);
				CGroupEnd();
				CAt(CButton(Tile, [this, T]() { ClashArenaSel = T; bDirty = true; }), TileX, TY, TileW, SmallH);
			}
		}
		CGroupEnd();
		Scroll(G, ContentH, BX, BY, BW, BH, 11, InitialY);
		return true;
	}

	// ================================================================================================ BATTLE PASS
	if (P == EZoogiClashPopup::BattlePass)
	{
		const int32 Level = D.BattlePassLevel;
		TWeakObjectPtr<UZoogiFrontProfileSubsystem> WFP(FP);
		CText(FString::Printf(TEXT("SEASON %d"), UZoogiFrontProfileSubsystem::PassSeason), 50.f, 206.f, 420.f, 50.f, 40.f, FLinearColor::White, 0);
		CLive([WFP]() { const UZoogiFrontProfileSubsystem* F = WFP.Get(); return F ? FString::Printf(TEXT("Ends in %s"), *ZHDays(F->SeasonSecondsLeft())) : FString(); }, 50.f, 254.f, 420.f, 34.f, 22.f, ZCSoft, 0);
		if (!D.bPassPremium)
		{
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(TEXT("UNLOCK PREMIUM"), 26.f, FLinearColor::White, 4.f))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f)); }
			UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			I->SetBrush(ZCImgBrush(Art(TEXT("icon_gem")), 40.f, 40.f, 0.f, false));
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }
			if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(FString::FromInt(UZoogiFrontProfileSubsystem::PassPremiumGems), 26.f, FLinearColor::White, 4.f))) S->SetVerticalAlignment(VAlign_Center);
			H->SetVisibility(ESlateVisibility::HitTestInvisible);
			CChunkyW(H, ZCRibGold, [this]() { OpenClashPopup(EZoogiClashPopup::PremiumUnlock, TEXT("PREMIUM PASS")); }, PW - 50.f - 460.f, 206.f, 460.f, 84.f, true);
		}
		else { CPlate(PW - 50.f - 360.f, 212.f, 360.f, 70.f, ZCRibGold, 18.f, 4.f, true); CText(TEXT("PREMIUM ACTIVE"), PW - 50.f - 360.f, 212.f, 360.f, 70.f, 28.f, FLinearColor::White); }
		// progress: tier badge, bar x/100, next tier badge
		{
			const float CX = PW * 0.5f, Y = 300.f;
			CImg(ClashArt(TEXT("star")), CX - 380.f, Y - 6.f, 84.f, 84.f);
			CText(FString::FromInt(Level), CX - 380.f, Y + 4.f, 84.f, 66.f, 30.f, FLinearColor::White);
			const bool bMax = Level >= UZoogiFrontProfileSubsystem::PassTiers;
			CBar(CX - 280.f, Y + 12.f, 560.f, 48.f, bMax ? 1.f : float(D.BattlePassXP) / UZoogiFrontProfileSubsystem::BattlePassXPPerLevel, ZCGreen,
				bMax ? FString(TEXT("MAX TIER")) : FString::Printf(TEXT("%d/%d"), D.BattlePassXP, UZoogiFrontProfileSubsystem::BattlePassXPPerLevel), 24.f);
			CImg(ClashArt(TEXT("star")), CX + 296.f, Y - 6.f, 84.f, 84.f, 0.f, false, bMax ? FLinearColor::White : FLinearColor(0.7f, 0.72f, 0.8f, 1.f));
			CText(FString::FromInt(FMath::Min(Level + 1, UZoogiFrontProfileSubsystem::PassTiers)), CX + 296.f, Y + 4.f, 84.f, 66.f, 30.f, FLinearColor::White);
			CText(TEXT("Play battles to earn pass points"), CX - 300.f, Y + 64.f, 600.f, 30.f, 19.f, ZCSoft, 1, 3.f);
		}
		// column headers: Free (blue ribbon) left, Premium (gold ribbon + crown) right
		const float BX = 30.f, BY = 470.f, BW = PW - 60.f, BH = PH - BY - 26.f;
		const float Mid = BW * 0.5f, ColW = FMath::Min(520.f, Mid - 110.f), FreeX = Mid - 70.f - ColW, PremX = Mid + 70.f;
		CPlate(BX + FreeX, 408.f, ColW, 54.f, ZCRibBlue, 16.f, 4.f, true);
		CText(TEXT("FREE"), BX + FreeX, 408.f, ColW, 54.f, 28.f, FLinearColor::White);
		CPlate(BX + PremX, 408.f, ColW, 54.f, ZCRibGold, 16.f, 4.f, true);
		CImg(ClashArt(TEXT("crown")), BX + PremX + ColW * 0.5f - 130.f, 400.f, 62.f, 62.f);
		CText(TEXT("PREMIUM"), BX + PremX, 408.f, ColW, 54.f, 28.f, FLinearColor::White);

		UCanvasPanel* G = CGroupBegin();
		const float RowH = 150.f, Gap = 12.f, IconS = 104.f, TxX = IconS + 22.f; // M4 portrait: text on top, CLAIM under it
		float InitialY = -1.f;
		for (int32 t = 1; t <= UZoogiFrontProfileSubsystem::PassTiers; ++t)
		{
			const float Y = 10.f + (t - 1) * (RowH + Gap), CY = Y + RowH * 0.5f;
			if (t > 1) CAt(CBox(Level >= t ? ZCYellow : ZCHex(TEXT("4A4D55")), ZCDark, 3.f, 6.f), Mid - 9.f, CY - RowH - Gap, 18.f, RowH + Gap);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const bool bPrem = Side == 1;
				const FZoogiFrontRoadReward& R = UZoogiFrontProfileSubsystem::PassReward(t, bPrem);
				const int32 Salt = t * 7 + (bPrem ? 3 : 0);
				const bool bClaimed = FP->IsPassClaimed(t, bPrem), bClaimable = FP->IsPassClaimable(t, bPrem);
				const bool bLockedPrem = bPrem && !D.bPassPremium;
				if (bClaimable && InitialY < 0.f) InitialY = FMath::Max(0.f, Y - 20.f);
				const FLinearColor Face = bPrem ? ZCRibGold : ZCRoadCard;
				UCanvasPanel* Tile = CGroupBegin();
				CPlate(0.f, 0.f, ColW, RowH, bClaimed ? ZCDarken(Face, 0.35f) : Face, 18.f, 4.f, !bClaimed);
				RewardIcon(R, 12.f, (RowH - IconS) * 0.5f, IconS, Salt);
				CText(RewardText(R, Salt), TxX, 10.f, ColW - TxX - 12.f, 62.f, R.Kind == FName(TEXT("hero")) ? 19.f : 24.f, FLinearColor::White, 0, 3.f, true);
				if (bClaimable) CChunkyVisual(CTxt(TEXT("CLAIM"), 24.f, FLinearColor::White, 4.f), ZCGreen, TxX, 80.f, FMath::Min(170.f, ColW - TxX - 12.f), 58.f);
				else if (bClaimed) CText(TEXT("CLAIMED"), TxX, 80.f, ColW - TxX - 12.f, 58.f, 22.f, ZCSoft, 0, 3.f);
				else if (bLockedPrem) CImg(ClashArt(TEXT("lock")), TxX, 82.f, 54.f, 54.f);
				CGroupEnd();
				const float TXp = bPrem ? PremX : FreeX;
				CAt(CButton(Tile, [this, t, bPrem]()
				{
					UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
					if (!F) return;
					if (bPrem && !F->Data().bPassPremium) { OpenClashPopup(EZoogiClashPopup::PremiumUnlock, TEXT("PREMIUM PASS")); return; }
					if (!F->IsPassClaimable(t, bPrem)) return; // future / claimed tiles: nothing to do
					TArray<FZoogiFrontReward> Out; FString Why;
					if (!F->ClaimPass(t, bPrem, Out, Why)) { ClashFail(Why); return; }
					const FZoogiFrontRoadReward& R2 = UZoogiFrontProfileSubsystem::PassReward(t, bPrem);
					if (R2.Kind == FName(TEXT("gold"))) { bDirty = true; return; } // gold: straight into the wallet, the red dot clears
					ClashAfterRewards = EZoogiClashPopup::BattlePass;
					ClashShowRewards(R2.Kind == FName(TEXT("chest")) ? FString(ZHChestName(R2.Chest)) : FString::Printf(TEXT("TIER %d"), t), R2.Kind == FName(TEXT("chest")) ? R2.Chest : NAME_None, Out);
				}), TXp, Y, ColW, RowH);
			}
			Medallion(Mid, CY, 80.f, FString::FromInt(t), Level >= t, 30.f);
		}
		const float ContentH = 10.f + UZoogiFrontProfileSubsystem::PassTiers * (RowH + Gap) + 20.f;
		CGroupEnd();
		if (InitialY < 0.f) InitialY = FMath::Max(0.f, 10.f + (Level - 1) * (RowH + Gap) - 20.f);
		Scroll(G, ContentH, BX, BY, BW, BH, 12, InitialY);
		return true;
	}

	// ================================================================================================ UNLOCK PREMIUM
	if (P == EZoogiClashPopup::PremiumUnlock)
	{
		CImg(ClashArt(TEXT("rays")), PW * 0.5f - 160.f, 100.f, 320.f, 320.f, 0.f, false, FLinearColor(1.f, 1.f, 1.f, 0.7f));
		CImg(ClashArt(TEXT("crown")), PW * 0.5f - 85.f, 175.f, 170.f, 170.f);
		CText(TEXT("Unlock the premium track and claim exclusive rewards on every tier!"), 60.f, 340.f, PW - 120.f, 90.f, 28.f, FLinearColor::White, 1, 3.f, true);
		CText(FString::Printf(TEXT("Paid from your gems (you have %d)."), FP->Gems()), 60.f, 440.f, PW - 120.f, 30.f, 19.f, ZCSoft, 1, 2.f);
		const bool bBuying = ClashBuyT0 >= 0.f;
		const float BW = 380.f, BH = 108.f, BYp = PH - 140.f;
		if (bBuying)
		{
			CPlate(PW * 0.5f - BW * 0.5f, BYp, BW, BH, ZCHex(TEXT("3A8A2A")), 26.f, 5.f, false);
			UWidget* Sp = CImg(ClashArt(TEXT("spinner")), PW * 0.5f - 42.f, BYp + 12.f, 84.f, 84.f);
			if (Sp) { Sp->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); ClashSpinW = Sp; }
			return true;
		}
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(Art(TEXT("icon_gem")), 52.f, 52.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f)); }
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(FString::Printf(TEXT("%d gems"), UZoogiFrontProfileSubsystem::PassPremiumGems), 38.f, FLinearColor::White, 5.f))) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(H, ZCGreen, [this]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			if (F->Gems() < UZoogiFrontProfileSubsystem::PassPremiumGems)
			{
				ClashFail(FString::Printf(TEXT("Not enough gems: the premium track costs %d.\nYou have %d."), UZoogiFrontProfileSubsystem::PassPremiumGems, F->Gems()));
				return;
			}
			ClashBuy([this]()
			{
				FString Why;
				UZoogiFrontProfileSubsystem* F2 = UZoogiFrontProfileSubsystem::Get(this);
				if (F2 && F2->UnlockPremium(Why)) { UE_LOG(LogZoogi, Log, TEXT("CLASH battle pass premium unlocked")); OpenClashPopup(EZoogiClashPopup::BattlePass, TEXT("BATTLE PASS")); }
				else ClashFail(Why);
			});
		}, PW * 0.5f - BW * 0.5f, BYp, BW, BH, true);
		return true;
	}
	return false;
}
