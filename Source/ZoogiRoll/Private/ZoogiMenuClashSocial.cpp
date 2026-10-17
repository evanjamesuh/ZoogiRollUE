// clash-menu DETAILED PASS 3 (Oct 17), portrait: Clan tab (Clan / Friends), Leaderboard tab, Matchmaking screen.
// Everything online (clan search results, clan creation, friends, global ranks) needs a backend that does not exist yet:
// the UI is fully built and shows honest empty / offline states or the local player only. No other players are invented.
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuWidget.h"
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
#include "Components/TextBlock.h"
#include "HAL/PlatformApplicationMisc.h"

namespace
{
	const TCHAR* ZSBadgeIcon[] = { TEXT("crown"), TEXT("swords"), TEXT("bolt"), TEXT("phoenix"), TEXT("axe"), TEXT("wolf") };
	const TCHAR* ZSBadgeName[] = { TEXT("Crown"), TEXT("Crossed Swords"), TEXT("Lightning"), TEXT("Phoenix"), TEXT("Axe"), TEXT("Wolf") };
	const TCHAR* ZSClanType[] = { TEXT("Open"), TEXT("Invite Only"), TEXT("Closed") };
	const TCHAR* ZSTips[] = {
		TEXT("Check the Shop every day for new offers."),
		TEXT("Save energy to defend, then counterattack!"),
		TEXT("Guard and Anchor Guard soak up the next big hit."),
		TEXT("Bank shots off the walls to reach hidden Zoogis."),
		TEXT("Upgrade cards in Collection to earn player XP."),
		TEXT("Trophy Road rewards wait for you at every milestone.") };

}

UWidget* UZoogiMenuRoot::CBadge(int32 Badge, float X, float Y, float S)
{
	static const TCHAR* Fill[] = { TEXT("2F6BD0"), TEXT("B8322B"), TEXT("5B2FA6"), TEXT("D2621E"), TEXT("2E8A3A"), TEXT("5E6878") };
	const int32 B = FMath::Clamp(Badge, 0, 5);
	UCanvasPanel* G = CGroupBegin();
	CImg(ClashArt(TEXT("shield")), 0.f, 0.f, S, S, 0.f, false, ZCLighten(ZCHex(Fill[B]), 0.25f));
	CImg(ClashArt(ZSBadgeIcon[B]), S * 0.22f, S * 0.18f, S * 0.56f, S * 0.56f, 0.f, false, B == 1 ? ZCHex(TEXT("FF8A80")) : FLinearColor::White);
	CGroupEnd();
	G->SetVisibility(ESlateVisibility::HitTestInvisible);
	return CAt(G, X, Y, S, S);
}

// ------------------------------------------------------------------------------------------------ CLAN tab
void UZoogiMenuRoot::BuildClashClanTab()
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const float W = DesignW, C = W * 0.5f, NavY = ClashNavY();
	const TCHAR* Subs[] = { TEXT("CLAN"), TEXT("FRIENDS") };
	for (int32 k = 0; k < 2; ++k)
		CChunky(Subs[k], ClashSocialSub == k ? ZCYellow : ZCBlueDeep, [this, k]() { ClashSocialSub = k; bDirty = true; }, C - 432.f + k * 444.f, 112.f, 420.f, 84.f, 32.f);
	if (ClashSocialSub == 0)
	{
		// row of the 6 clan badges
		for (int32 b = 0; b < 6; ++b) CBadge(b, C - 3.f * 156.f + 8.f + b * 156.f, 236.f, 140.f);
		const float PX = 24.f, PY = 420.f, PWd = W - 48.f, PHt = FMath::Min(900.f, NavY - PY - 40.f);
		CGoldPanel(PX, PY, PWd, PHt, ZCPanel, 30.f);
		CImg(ClashArt(TEXT("shield")), C - 120.f, PY + 60.f, 240.f, 240.f, 0.f, false, FLinearColor(0.55f, 0.6f, 0.7f, 1.f));
		CText(TEXT("You are not in a clan yet"), PX, PY + 320.f, PWd, 64.f, 40.f, FLinearColor::White);
		CText(TEXT("Join a clan to chat, donate cards and battle together."), PX + 60.f, PY + 390.f, PWd - 120.f, 44.f, 24.f, ZCSoft, 1, 3.f, true);
		CChunky(TEXT("SEARCH CLANS"), ZCHex(TEXT("5FB8FF")), [this]() { ClashSearchNote.Reset(); OpenClashPopup(EZoogiClashPopup::SearchClans, TEXT("SEARCH CLANS")); }, C - 420.f, PY + 470.f, 400.f, 110.f, 30.f, ClashArt(TEXT("search")));
		CChunky(TEXT("CREATE CLAN"), ZCYellow, [this]() { OpenClashPopup(EZoogiClashPopup::CreateClan, TEXT("CREATE CLAN")); }, C + 20.f, PY + 470.f, 400.f, 110.f, 30.f, ClashArt(TEXT("pencil")), true);
		CText(TEXT("Clans need the online backend (not connected yet): searching shows no clans and creating saves nothing."), PX + 50.f, PY + PHt - 120.f, PWd - 100.f, 80.f, 20.f, ZCMuted, 1, 3.f, true);
		return;
	}
	// FRIENDS
	CText(TEXT("Friends 0/100"), 40.f, 236.f, 560.f, 80.f, 42.f, FLinearColor::White, 0);
	CChunky(TEXT("ADD FRIEND"), ZCGreen, [this]() { ClashSearchNote.Reset(); OpenClashPopup(EZoogiClashPopup::AddFriend, TEXT("ADD FRIEND")); }, W - 40.f - 340.f, 232.f, 340.f, 92.f, 30.f);
	const float PX = 24.f, PY = 360.f, PWd = W - 48.f, PHt = FMath::Min(820.f, NavY - PY - 40.f);
	CGoldPanel(PX, PY, PWd, PHt, ZCPanel, 30.f);
	CImg(ClashArt(TEXT("clan")), C - 110.f, PY + 70.f, 220.f, 220.f, 0.f, false, FLinearColor(0.7f, 0.75f, 0.85f, 1.f));
	CText(TEXT("No friends yet"), PX, PY + 310.f, PWd, 64.f, 42.f, FLinearColor::White);
	CText(TEXT("Share your tag or search for a player to add them."), PX + 60.f, PY + 382.f, PWd - 120.f, 44.f, 24.f, ZCSoft, 1, 3.f, true);
	CChunky(TEXT("Add Friend"), ZCGreen, [this]() { ClashSearchNote.Reset(); OpenClashPopup(EZoogiClashPopup::AddFriend, TEXT("ADD FRIEND")); }, C - 260.f, PY + 470.f, 520.f, 130.f, 44.f, ClashArt(TEXT("clan")), true);
	CText(FString::Printf(TEXT("Your tag: #%s"), FP ? *FP->Data().Tag : TEXT("-")), PX, PY + 630.f, PWd, 44.f, 26.f, ZCGold, 1, 3.f);
	CText(TEXT("Friends need the online backend (not connected yet)."), PX + 50.f, PY + PHt - 80.f, PWd - 100.f, 50.f, 20.f, ZCMuted, 1, 3.f, true);
}

// ------------------------------------------------------------------------------------------------ LEADERBOARD tab
void UZoogiMenuRoot::BuildClashLeaderboardTab()
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	const float W = DesignW, C = W * 0.5f, NavY = ClashNavY();
	CImg(ClashArt(TEXT("ribbon_gold")), C - 340.f, 104.f, 680.f, 114.f);
	CText(TEXT("LEADERBOARD"), C - 260.f, 108.f, 520.f, 84.f, 46.f, FLinearColor::White);
	const TCHAR* Subs[] = { TEXT("PLAYERS"), TEXT("CLANS") };
	for (int32 k = 0; k < 2; ++k)
		CChunky(Subs[k], ClashLbSub == k ? ZCHex(TEXT("3FA2FF")) : ZCHex(TEXT("3A4A66")), [this, k]() { ClashLbSub = k; ClashLbPage = 0; bDirty = true; }, C - 420.f + k * 430.f, 236.f, 410.f, 80.f, 30.f);
	CPlate(24.f, 336.f, W - 48.f, 70.f, ZCNavyDeep.CopyWithNewOpacity(0.85f), 18.f, 4.f, false);
	CText(TEXT("OFFLINE: global ranks need the online backend. Showing this device only."), 44.f, 336.f, W - 88.f, 70.f, 21.f, ZCYellow, 1, 3.f, true);
	const float ListY = 426.f, RowH = 116.f, PinY = NavY - 120.f - RowH - 20.f;
	auto LevelHex = [this](float X, float Y, int32 L) { CImg(ClashArt(TEXT("level")), X, Y, 70.f, 70.f); CText(FString::FromInt(L), X, Y, 70.f, 66.f, 28.f, FLinearColor::White); };
	auto PlayerRow = [&](float Y, int32 Rank, bool bPinned)
	{
		const FLinearColor Fill = bPinned ? ZCHex(TEXT("FFD54A")) : ZCNavy;
		const FLinearColor TC = bPinned ? ZCDark : FLinearColor::White;
		CPlate(24.f, Y, W - 48.f, RowH, Fill, 18.f, 4.f, true);
		static const TCHAR* Medal[] = { TEXT("FFC531"), TEXT("C9D3E0"), TEXT("D08A4A") };
		if (Rank >= 1 && Rank <= 3)
		{
			CAt(CBox(ZCHex(Medal[Rank - 1]), ZCDark, 4.f, 34.f), 44.f, Y + RowH * 0.5f - 34.f, 68.f, 68.f);
			CText(FString::FromInt(Rank), 44.f, Y + RowH * 0.5f - 34.f, 68.f, 64.f, 32.f, ZCDark, 1, 0.f);
		}
		else CText(Rank > 0 ? FString::FromInt(Rank) : FString(TEXT("-")), 30.f, Y, 100.f, RowH, 32.f, TC, 1, bPinned ? 0.f : 3.f);
		LevelHex(134.f, Y + RowH * 0.5f - 35.f, D.Level);
		CText(D.GuestName, 220.f, Y + 12.f, 460.f, 52.f, 30.f, TC, 0, bPinned ? 0.f : 3.f);
		CImg(ClashArt(TEXT("shield")), 220.f, Y + 62.f, 40.f, 40.f, 0.f, false, FLinearColor(0.6f, 0.65f, 0.75f, 1.f));
		CText(TEXT("No Clan"), 266.f, Y + 62.f, 300.f, 40.f, 22.f, bPinned ? ZCDark : ZCMuted, 0, bPinned ? 0.f : 3.f);
		CPlate(W - 48.f - 230.f, Y + RowH * 0.5f - 30.f, 210.f, 60.f, ZCNavyDeep.CopyWithNewOpacity(0.9f), 30.f, 4.f, false);
		CImg(ClashArt(TEXT("trophy")), W - 48.f - 250.f, Y + RowH * 0.5f - 40.f, 76.f, 76.f);
		CText(FText::AsNumber(D.Trophies).ToString(), W - 48.f - 176.f, Y + RowH * 0.5f - 30.f, 150.f, 60.f, 30.f, ZCGold);
	};
	if (ClashLbSub == 0)
	{
		CText(TEXT("ON THIS DEVICE"), 40.f, ListY, 600.f, 44.f, 24.f, ZCSoft, 0, 3.f);
		PlayerRow(ListY + 54.f, 1, false); // the local player only: no other players are invented
		CText(TEXT("Other players appear here once online leaderboards exist."), 40.f, ListY + 54.f + RowH + 24.f, W - 80.f, 44.f, 22.f, ZCMuted, 1, 3.f, true);
		CText(TEXT("YOUR GLOBAL RANK"), 40.f, PinY - 48.f, 600.f, 44.f, 24.f, ZCSoft, 0, 3.f);
		PlayerRow(PinY, 0, true); // own row pinned at the bottom in yellow; global rank unknown offline
	}
	else
	{
		CGoldPanel(24.f, ListY, W - 48.f, 520.f, ZCPanel, 26.f);
		CImg(ClashArt(TEXT("shield")), C - 90.f, ListY + 50.f, 180.f, 180.f, 0.f, false, FLinearColor(0.55f, 0.6f, 0.7f, 1.f));
		CText(TEXT("No clan rankings offline"), 24.f, ListY + 250.f, W - 48.f, 60.f, 38.f, FLinearColor::White);
		CText(TEXT("Clan rows (rank, badge, name, #tag, members, trophies) need the online backend."), 74.f, ListY + 320.f, W - 148.f, 90.f, 22.f, ZCSoft, 1, 3.f, true);
	}
	// pager
	const float GY = NavY - 110.f;
	CChunky(TEXT("<"), ZCNavyBtn, [this]() { bDirty = true; }, C - 230.f, GY, 100.f, 84.f, 40.f);
	CText(ClashLbSub == 0 ? TEXT("1/1") : TEXT("0/0"), C - 120.f, GY, 240.f, 84.f, 34.f, FLinearColor::White);
	CChunky(TEXT(">"), ZCNavyBtn, [this]() { bDirty = true; }, C + 130.f, GY, 100.f, 84.f, 40.f);
}

// ------------------------------------------------------------------------------------------------ MATCHMAKING (full screen)
void UZoogiMenuRoot::ClashStartMatchmaking()
{
	// DETAILED PASS 5: BATTLE needs a full deck (our rule: 8 cards in the active deck)
	if (const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this))
		if (FP->Deck(FP->Data().ActiveDeck).Num() < UZoogiFrontProfileSubsystem::DeckSize)
		{
			UE_LOG(LogZoogi, Log, TEXT("CLASH BATTLE blocked: deck %d/%d"), FP->Deck(FP->Data().ActiveDeck).Num(), UZoogiFrontProfileSubsystem::DeckSize);
			OpenClashPopup(EZoogiClashPopup::Info, TEXT("DECK NOT READY"), FString::Printf(TEXT("Your deck needs %d cards to battle."), UZoogiFrontProfileSubsystem::DeckSize));
			return;
		}
	ClashMMT0 = Time; ClashMMPhase = 0; ClashMMTip = FMath::RandRange(0, (int32)UE_ARRAY_COUNT(ZSTips) - 1);
	UE_LOG(LogZoogi, Log, TEXT("CLASH matchmaking: searching (offline -> local CPU match after %.0f s)"), ClashMMSearch);
	OpenClashPopup(EZoogiClashPopup::Matchmaking, TEXT("MATCHMAKING"));
}

void UZoogiMenuRoot::ClashCancelMatchmaking()
{
	if (ClashPopup != EZoogiClashPopup::Matchmaking) return;
	ClashMMT0 = -1.f; ClashMMPhase = 0;
	ClashPopup = EZoogiClashPopup::None;
	SetClashTab(ZCBattle);
	UE_LOG(LogZoogi, Log, TEXT("CLASH matchmaking cancelled -> Home"));
	bDirty = true;
}

void UZoogiMenuRoot::BuildClashMatchmaking()
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	const FZoogiFrontProfileData Dflt;
	const FZoogiFrontProfileData& D = FP ? FP->Data() : Dflt;
	AZoogiGameMode* G = GM();
	const int32 ArenaIdx = G ? (int32)G->MenuArena : 0;
	const float W = DesignW, C = W * 0.5f, H = DesignH, OY = FMath::Max(0.f, (H - 1920.f) * 0.5f);
	CAt(CBox(ZCHex(TEXT("0A1C46")).CopyWithNewOpacity(0.97f), FLinearColor::Transparent, 0.f, 0.f), 0.f, -400.f, W, H + 800.f);
	if (UButton* Block = CButton(CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f), []() {})) { Block->SetStyle(ZCClearStyle()); CAt(Block, 0.f, -400.f, W, H + 800.f); }
	if (ClashMMPhase >= 2)
	{
		// ---- loading: game logo over the village key art, "Loading.." + % + yellow bar
		CImg(Art(TEXT("meadows_background")), -420.f, -400.f, W + 840.f, H + 800.f, 0.f, true);
		CAt(CBox(FLinearColor(0.f, 0.02f, 0.08f, 0.25f), FLinearColor::Transparent, 0.f, 0.f), 0.f, -400.f, W, H + 800.f);
		if (UTexture2D* Logo = Art(TEXT("zoogi-roll-arena-logo"))) CImg(Logo, C - 440.f, H * 0.5f - 640.f, 880.f, 880.f * Logo->GetSizeY() / FMath::Max(1, Logo->GetSizeX()));
		const float LY = H - 360.f;
		CText(TEXT("Loading.."), 90.f, LY, 500.f, 70.f, 46.f, FLinearColor::White, 0, 5.f);
		TWeakObjectPtr<UZoogiMenuRoot> WL(this);
		CLive([WL]() { const UZoogiMenuRoot* M = WL.Get(); return M ? FString::Printf(TEXT("%d%%"), M->ClashMMLoadPct()) : FString(); }, W - 390.f, LY, 300.f, 70.f, 46.f, FLinearColor::White, 2, 5.f);
		CAt(CBox(ZCHex(TEXT("0B1E44")).CopyWithNewOpacity(0.85f), ZCDark, 4.f, 14.f), 90.f, LY + 84.f, W - 180.f, 60.f);
		UWidget* Bar = CAt(CBox(ZCYellow, FLinearColor::Transparent, 0.f, 10.f), 96.f, LY + 90.f, (W - 192.f) * ClashMMLoadPct() / 100.f, 48.f);
		ClashMMBar = Bar;
		return;
	}
	// arena banner
	UTexture2D* Map = Art(FString(ZCArenaSlug(ArenaIdx)) + TEXT("_map"));
	CGoldPanel(40.f, OY + 60.f, W - 80.f, 300.f, ZCNavyDeep, 26.f);
	CImg(Map ? Map : Art(TEXT("meadows_background")), 54.f, OY + 74.f, W - 108.f, 272.f, 16.f, true);
	CImg(ClashArt(TEXT("ribbon")), C - 330.f, OY + 300.f, 660.f, 110.f);
	CText(ZoogiUtil::ArenaName((EZoogiArena)ArenaIdx).ToUpper(), C - 250.f, OY + 302.f, 500.f, 84.f, 42.f, FLinearColor::White);
	// opponent (red, unknown) / VS / player (blue)
	const float BW = W - 80.f, BH = 190.f;
	CPlate(40.f, OY + 470.f, BW, BH, ZCHex(TEXT("C2362F")), 24.f, 5.f, true);
	if (ClashMMPhase == 0) CText(TEXT("? ? ?"), 40.f, OY + 470.f, BW, BH, 72.f, FLinearColor::White, 1, 5.f);
	else
	{
		// found: our real opponent for the local match (the CPU Zoogi picked in the menu); a bot, so "Bot" clan and 0 trophies
		FString Opp = TEXT("CPU");
		if (G && G->MenuPlayers.IsValidIndex(1))
		{
			const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
			const int32 Zi = G->MenuPlayers[1].ZoogiIndex;
			Opp = (Roster.IsValidIndex(Zi) ? Roster[Zi].Name.ToUpper() : FString(TEXT("PLAYER 2"))) + (G->MenuPlayers[1].bCPU ? TEXT(" (CPU)") : TEXT(" (local)"));
		}
		CImg(ClashArt(TEXT("level")), 70.f, OY + 470.f + BH * 0.5f - 50.f, 100.f, 100.f);
		CText(TEXT("1"), 70.f, OY + 470.f + BH * 0.5f - 50.f, 100.f, 96.f, 40.f, FLinearColor::White);
		CText(Opp, 196.f, OY + 492.f, BW - 420.f, 64.f, 40.f, FLinearColor::White, 0);
		CImg(ClashArt(TEXT("shield")), 196.f, OY + 566.f, 56.f, 56.f, 0.f, false, FLinearColor(0.85f, 0.7f, 0.7f, 1.f));
		CText(TEXT("Bot Clan"), 262.f, OY + 566.f, 300.f, 56.f, 28.f, ZCSoft, 0, 3.f);
		CImg(ClashArt(TEXT("trophy")), 40.f + BW - 250.f, OY + 470.f + BH * 0.5f - 44.f, 88.f, 88.f);
		CText(TEXT("0"), 40.f + BW - 160.f, OY + 470.f, 140.f, BH, 40.f, ZCGold, 0);
	}
	CText(TEXT("VS"), C - 150.f, OY + 690.f, 300.f, 150.f, 110.f, ZCYellow, 1, 7.f);
	CPlate(40.f, OY + 870.f, BW, BH, ZCHex(TEXT("2F6BD0")), 24.f, 5.f, true);
	CImg(ClashArt(TEXT("level")), 70.f, OY + 870.f + BH * 0.5f - 50.f, 100.f, 100.f);
	CText(FString::FromInt(D.Level), 70.f, OY + 870.f + BH * 0.5f - 50.f, 100.f, 96.f, 40.f, FLinearColor::White);
	CText(D.GuestName, 196.f, OY + 892.f, BW - 420.f, 64.f, 40.f, FLinearColor::White, 0);
	CImg(ClashArt(TEXT("shield")), 196.f, OY + 966.f, 56.f, 56.f, 0.f, false, FLinearColor(0.7f, 0.75f, 0.85f, 1.f));
	CText(TEXT("No Clan"), 262.f, OY + 966.f, 300.f, 56.f, 28.f, ZCSoft, 0, 3.f);
	CImg(ClashArt(TEXT("trophy")), 40.f + BW - 250.f, OY + 870.f + BH * 0.5f - 44.f, 88.f, 88.f);
	CText(FText::AsNumber(D.Trophies).ToString(), 40.f + BW - 160.f, OY + 870.f, 140.f, BH, 40.f, ZCGold, 0);
	// searching + timer counting up from 0:00
	TWeakObjectPtr<UZoogiMenuRoot> WThis(this);
	if (ClashMMPhase == 0)
	{
		CText(TEXT("Searching for an opponent..."), 40.f, OY + 1110.f, W - 80.f, 64.f, 40.f, FLinearColor::White);
		CLive([WThis]() { const UZoogiMenuRoot* M = WThis.Get(); return M ? ZCClock(M->Time - M->ClashMMT0) : FString(); }, C - 150.f, OY + 1180.f, 300.f, 70.f, 50.f, ZCGold, 1, 4.f);
		UWidget* Sp = CImg(ClashArt(TEXT("spinner")), C - 200.f - 44.f, OY + 1180.f, 70.f, 70.f);
		if (Sp) { Sp->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); ClashSpinW = Sp; }
	}
	else
	{
		CText(TEXT("Opponent found!"), 40.f, OY + 1110.f, W - 80.f, 90.f, 60.f, ZCYellow, 1, 6.f);
		CText(ZCClock(ClashMMSearch), C - 150.f, OY + 1200.f, 300.f, 60.f, 40.f, ZCGold, 1, 4.f);
	}
	CText(TEXT("Offline: online matchmaking needs the backend, so this starts a local match (CPU opponent)."), 70.f, OY + 1260.f, W - 140.f, 64.f, 21.f, ZCSoft, 1, 3.f, true);
	if (ClashMMPhase == 0) // cancel returns to Home (spec: destination not captured, assume Home)
		CChunky(TEXT("CANCEL"), ZCXRed, [this]() { ClashCancelMatchmaking(); }, C - 180.f, OY + 1350.f, 360.f, 110.f, 40.f);
	// tip box (rotates every 4 s)
	const float TY = OY + 1520.f;
	CGoldPanel(40.f, TY, W - 80.f, 230.f, ZCPanel, 22.f);
	CText(TEXT("Tip"), 70.f, TY + 16.f, 200.f, 50.f, 32.f, ZCYellow, 0, 4.f);
	CLive([WThis]() { const UZoogiMenuRoot* M = WThis.Get(); if (!M) return FString(); const int32 N = (int32)UE_ARRAY_COUNT(ZSTips); return FString(ZSTips[(M->ClashMMTip + FMath::FloorToInt(FMath::Max(0.f, M->Time - M->ClashMMT0) / 4.f)) % N]); },
		70.f, TY + 72.f, W - 140.f, 130.f, 28.f, FLinearColor::White, 0, 3.f);
}

// ------------------------------------------------------------------------------------------------ popups
bool UZoogiMenuRoot::BuildClashSocialPopup(EZoogiClashPopup P, float PW, float PH)
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	auto SearchBtn = [this](TFunction<void()> Fn, float X, float Y, float S)
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(ClashArt(TEXT("search")), S * 0.6f, S * 0.6f, 0.f, false));
		I->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(I, ZCBlue, MoveTemp(Fn), X, Y, S, S);
	};
	if (P == EZoogiClashPopup::SearchClans)
	{
		CField(ClashSearchText, TEXT("Clan name or #tag"), 50.f, 140.f, PW - 100.f - 110.f, 90.f, &ClashFieldA);
		SearchBtn([this]()
		{
			ClashSearchText = ClashFieldA.IsValid() ? ClashFieldA->GetText().ToString() : ClashSearchText;
			ClashSearchNote = ClashSearchText.TrimStartAndEnd().Len() < 2 ? TEXT("Type 2+ letters or a #tag.") : TEXT("Clan search needs the online backend (not connected yet). No clans to show.");
			UE_LOG(LogZoogi, Log, TEXT("CLASH clan search '%s': backend needed"), *ClashSearchText);
			bDirty = true;
		}, PW - 50.f - 96.f, 137.f, 96.f);
		// column legend for the result rows (badge, name, #tag, members N/50, trophies, required trophies, type, JOIN)
		CPlate(50.f, 260.f, PW - 100.f, 56.f, ZCNavyDeep.CopyWithNewOpacity(0.7f), 14.f, 3.f, false);
		CText(TEXT("CLAN"), 70.f, 260.f, 300.f, 56.f, 20.f, ZCSoft, 0, 2.f);
		CText(TEXT("MEMBERS"), PW * 0.5f - 90.f, 260.f, 160.f, 56.f, 20.f, ZCSoft, 1, 2.f);
		CText(TEXT("TROPHIES"), PW * 0.5f + 80.f, 260.f, 160.f, 56.f, 20.f, ZCSoft, 1, 2.f);
		CText(TEXT("JOIN"), PW - 230.f, 260.f, 160.f, 56.f, 20.f, ZCSoft, 1, 2.f);
		CImg(ClashArt(TEXT("search")), PW * 0.5f - 90.f, 400.f, 180.f, 180.f, 0.f, false, FLinearColor(1.f, 1.f, 1.f, 0.6f));
		CText(ClashSearchNote.IsEmpty() ? FString(TEXT("Search for a clan by name or #tag.")) : ClashSearchNote, 70.f, 600.f, PW - 140.f, 120.f, 28.f, FLinearColor::White, 1, 3.f, true);
		CText(TEXT("0/0"), PW * 0.5f - 100.f, PH - 120.f, 200.f, 70.f, 32.f, FLinearColor::White);
		CChunky(TEXT("<"), ZCNavyBtn, []() {}, PW * 0.5f - 220.f, PH - 124.f, 90.f, 78.f, 36.f);
		CChunky(TEXT(">"), ZCNavyBtn, []() {}, PW * 0.5f + 130.f, PH - 124.f, 90.f, 78.f, 36.f);
		return true;
	}
	if (P == EZoogiClashPopup::CreateClan)
	{
		CBadge(ClashBadgeSel, 50.f, 130.f, 170.f);
		CChunky(TEXT("CHANGE BADGE"), ZCBlue, [this]()
		{
			ClashClanName = ClashFieldA.IsValid() ? ClashFieldA->GetText().ToString() : ClashClanName;
			ClashClanTag = ClashFieldB.IsValid() ? ClashFieldB->GetText().ToString() : ClashClanTag;
			ClashClanDesc = ClashDescBox.IsValid() ? ClashDescBox->GetText().ToString() : ClashClanDesc;
			ClashBadgePick = ClashBadgeSel; OpenClashPopup(EZoogiClashPopup::ChooseBadge, TEXT("Choose a Badge"));
		}, 250.f, 160.f, 340.f, 100.f, 28.f);
		CText(ZSBadgeName[FMath::Clamp(ClashBadgeSel, 0, 5)], 610.f, 160.f, PW - 660.f, 100.f, 26.f, ZCSoft, 0, 3.f);
		auto Label = [this](const TCHAR* T, float Y) { CText(T, 50.f, Y, 400.f, 40.f, 24.f, FLinearColor::White, 0, 3.f); };
		Label(TEXT("Name"), 324.f);
		CField(ClashClanName, TEXT("Clan name"), 50.f, 366.f, PW - 100.f, 84.f, &ClashFieldA);
		Label(TEXT("Tag"), 470.f);
		CField(ClashClanTag, TEXT("TAG"), 50.f, 512.f, 360.f, 84.f, &ClashFieldB);
		Label(TEXT("Description"), 616.f);
		CField(ClashClanDesc, TEXT("Tell players about your clan..."), 50.f, 658.f, PW - 100.f, 180.f, nullptr, true);
		// Type < Open >, Required Trophies < 0 >
		auto Stepper = [this, PW](float Y, const TCHAR* L, const FString& V, TFunction<void()> Dn, TFunction<void()> Up, bool bTrophy)
		{
			CPlate(50.f, Y, PW - 100.f, 96.f, ZCNavyDeep.CopyWithNewOpacity(0.82f), 16.f, 4.f, false);
			CText(L, 74.f, Y, 360.f, 96.f, 26.f, FLinearColor::White, 0, 3.f);
			const float SX = PW - 50.f - 440.f;
			CChunky(TEXT("<"), ZCBlue, MoveTemp(Dn), SX, Y + 10.f, 80.f, 76.f, 36.f);
			if (bTrophy) CImg(ClashArt(TEXT("trophy")), SX + 100.f, Y + 16.f, 64.f, 64.f);
			CText(V, SX + (bTrophy ? 160.f : 90.f), Y, bTrophy ? 180.f : 260.f, 96.f, 30.f, bTrophy ? ZCGold : FLinearColor::White);
			CChunky(TEXT(">"), ZCBlue, MoveTemp(Up), SX + 360.f, Y + 10.f, 80.f, 76.f, 36.f);
		};
		auto Keep = [this]()
		{
			ClashClanName = ClashFieldA.IsValid() ? ClashFieldA->GetText().ToString() : ClashClanName;
			ClashClanTag = ClashFieldB.IsValid() ? ClashFieldB->GetText().ToString() : ClashClanTag;
			ClashClanDesc = ClashDescBox.IsValid() ? ClashDescBox->GetText().ToString() : ClashClanDesc;
		};
		Stepper(862.f, TEXT("Type"), ZSClanType[FMath::Clamp(ClashClanType, 0, 2)], [this, Keep]() { Keep(); ClashClanType = (ClashClanType + 2) % 3; bDirty = true; }, [this, Keep]() { Keep(); ClashClanType = (ClashClanType + 1) % 3; bDirty = true; }, false);
		Stepper(974.f, TEXT("Required Trophies"), FString::FromInt(ClashClanReq), [this, Keep]() { Keep(); ClashClanReq = FMath::Max(0, ClashClanReq - 100); bDirty = true; }, [this, Keep]() { Keep(); ClashClanReq = FMath::Min(5000, ClashClanReq + 100); bDirty = true; }, true);
		CChunky(TEXT("CREATE"), ZCGreen, [this, Keep]()
		{
			Keep();
			UE_LOG(LogZoogi, Log, TEXT("CLASH create clan '%s' #%s badge %d type %d req %d: backend needed, nothing created"), *ClashClanName, *ClashClanTag, ClashBadgeSel, ClashClanType, ClashClanReq);
			OpenClashPopup(EZoogiClashPopup::Info, TEXT("CREATE CLAN"), TEXT("Creating a clan needs the online backend, which is not connected yet.\nNothing was created and nothing was charged."));
		}, PW * 0.5f - 200.f, PH - 150.f, 400.f, 112.f, 40.f, nullptr, true);
		return true;
	}
	if (P == EZoogiClashPopup::ChooseBadge)
	{
		const float S = 200.f, G = 40.f, X0 = PW * 0.5f - (3.f * S + 2.f * G) * 0.5f;
		for (int32 b = 0; b < 6; ++b)
		{
			const float X = X0 + (b % 3) * (S + G), Y = 140.f + (b / 3) * (S + 60.f);
			CBadge(b, X, Y, S);
			CAt(CButton(CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f), [this, b]() { ClashBadgeSel = b; UE_LOG(LogZoogi, Log, TEXT("CLASH clan badge %d"), b); OpenClashPopup(EZoogiClashPopup::CreateClan, TEXT("CREATE CLAN")); }), X, Y, S, S);
			if (b == ClashBadgeSel) CImg(ClashArt(TEXT("check")), X + S - 56.f, Y - 6.f, 62.f, 62.f);
			CText(ZSBadgeName[b], X - 20.f, Y + S, S + 40.f, 40.f, 20.f, ZCSoft, 1, 3.f);
		}
		return true;
	}
	if (P == EZoogiClashPopup::AddFriend)
	{
		const FString Tag = FP ? TEXT("#") + FP->Data().Tag : FString(TEXT("#-"));
		CText(TEXT("Your tag"), 50.f, 130.f, 300.f, 50.f, 26.f, ZCSoft, 0, 3.f);
		CPlate(50.f, 184.f, 380.f, 80.f, ZCPill.CopyWithNewOpacity(0.9f), 40.f, 4.f, false);
		CText(Tag, 50.f, 184.f, 380.f, 80.f, 34.f, ZCGold);
		{
			UCanvasPanel* Grp = CGroupBegin();
			CPlate(0.f, 0.f, 80.f, 76.f, ZCNavyBtn, 14.f, 3.f, false);
			CImg(ClashArt(TEXT("copy")), 16.f, 12.f, 48.f, 50.f);
			CGroupEnd();
			CAt(CButton(Grp, [this, Tag]() { FPlatformApplicationMisc::ClipboardCopy(*Tag); ClashCopiedT = Time; UE_LOG(LogZoogi, Log, TEXT("CLASH tag copied: %s"), *Tag); bDirty = true; }), 446.f, 186.f, 80.f, 76.f);
		}
		if (Time - ClashCopiedT < 1.5f) CText(TEXT("Copied!"), 540.f, 190.f, 200.f, 70.f, 28.f, ZCBoughtCheck, 0, 3.f);
		CField(ClashFriendText, TEXT("Player name or #TAG"), 50.f, 300.f, PW - 100.f - 110.f, 90.f, &ClashFieldA);
		SearchBtn([this]()
		{
			ClashFriendText = ClashFieldA.IsValid() ? ClashFieldA->GetText().ToString() : ClashFriendText;
			ClashSearchNote = ClashFriendText.TrimStartAndEnd().Len() < 2 ? TEXT("Type 2+ letters or a player #TAG.") : TEXT("Player search needs the online backend (not connected yet). No players to show.");
			UE_LOG(LogZoogi, Log, TEXT("CLASH friend search '%s': backend needed"), *ClashFriendText);
			bDirty = true;
		}, PW - 50.f - 96.f, 297.f, 96.f);
		CText(TEXT("Search by name (2+ letters) or by player tag."), 50.f, 400.f, PW - 100.f, 40.f, 22.f, ZCSoft, 0, 3.f);
		if (!ClashSearchNote.IsEmpty()) CText(ClashSearchNote, 50.f, 470.f, PW - 100.f, 100.f, 26.f, FLinearColor::White, 1, 3.f, true);
		CChunky(TEXT("BLOCKED PLAYERS"), ZCNavyBtn, [this]() { OpenClashPopup(EZoogiClashPopup::Blocked, TEXT("BLOCKED PLAYERS")); }, PW * 0.5f - 220.f, PH - 150.f, 440.f, 100.f, 30.f);
		return true;
	}
	if (P == EZoogiClashPopup::Blocked)
	{
		CImg(ClashArt(TEXT("shield")), PW * 0.5f - 70.f, 140.f, 140.f, 140.f, 0.f, false, FLinearColor(0.6f, 0.65f, 0.75f, 1.f));
		CText(TEXT("You haven't blocked anyone."), 40.f, 300.f, PW - 80.f, 60.f, 32.f, FLinearColor::White);
		return true;
	}
	return false;
}
