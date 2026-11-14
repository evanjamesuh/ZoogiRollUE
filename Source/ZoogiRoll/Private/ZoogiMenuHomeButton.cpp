// Oct 22 (Evan): every menu screen gets a clear way back to the main menu. One HOME button for all of them, drawn after
// the screen in the current menu skin (fantasy reskin: chunky stone plate in a metallic gold bezel + silver house icon),
// sized off the canvas height so it is a big touch target on phones and still a normal button with a mouse:
// - pushed screens (PLAY setup, Zoogi select, Powers, Settings, Collection, Story, Events, Shop, Social, Deck),
// - the clash tabs other than Home (Shop, Cards, Clan, Leaderboard) - just above the bottom nav,
// - the full-screen popups (Arenas, Trophy Road, Battle Pass, Matchmaking) and the anime viewer.
// Esc / Android back keep their step-back behaviour (HandleBack); the button always goes straight to the main menu.
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

bool UZoogiMenuRoot::NeedsHomeButton() const
{
	if (bAnimeView) return true;
	if (Stack.Num() > 1) return true;
	if (!ClashOn()) return false;
	if (ClashPopup == EZoogiClashPopup::Matchmaking || ClashBigPopup(ClashPopup)) return true;
	if (ClashPopup != EZoogiClashPopup::None) return false; // small popups have their own X over a scrim
	return ClashTab != ZCBattle;
}

void UZoogiMenuRoot::GoHome()
{
	UE_LOG(LogZoogi, Log, TEXT("MENU HOME button -> main menu"));
	bModal = false;
	DeckPreview = NAME_None;
	if (bAnimeView)
	{
		if (UZoogiAnimeSubsystem* A = GetWorld() ? GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr) A->StopMenuMovie();
		bAnimeView = false;
		SetBackdropPlaying(true);
	}
	PopToHome();
	if (ClashOn())
	{
		if (ClashPopup == EZoogiClashPopup::Matchmaking) ClashCancelMatchmaking();
		SetClashTab(ZCBattle); // also closes any popup
	}
	bDirty = true;
}

void UZoogiMenuRoot::BuildHomeButton()
{
	if (!Canvas || !NeedsHomeButton()) return;
	// ~7% of the canvas height (76 px on a 1080 canvas, 134 on a 1920 portrait one), never under 88 design units
	const float H = FMath::Max(88.f, DesignH * 0.07f);
	const float W = H * 2.35f;
	const float M = 24.f;
	float Y = DesignH - H - M; // bottom-left
	if (bAnimeView || (ClashOn() && Top() == EZoogiScreen::Home && ClashPopup != EZoogiClashPopup::None)) Y = M; // full-screen popups / anime: top-left
	else if (ClashOn() && Top() == EZoogiScreen::Home) Y = ClashNavY() - H - M; // clash tabs: above the bottom nav
	// Oct 22 queue #18: stack / anime / full-screen popups get BACK (one step via HandleBack); clash tabs keep HOME.
	const bool bBack = (Stack.Num() > 1) || bAnimeView
		|| (ClashOn() && Top() == EZoogiScreen::Home && ClashPopup != EZoogiClashPopup::None && ClashPopup != EZoogiClashPopup::Matchmaking);
	if (bBack)
		CChunky(TEXT("BACK"), ZCNavyBtn, [this]() { HandleBack(); }, M, Y, W, H, H * 0.36f, ClashArt(TEXT("back")));
	else
		CChunky(TEXT("HOME"), ZCNavyBtn, [this]() { GoHome(); }, M, Y, W, H, H * 0.36f, ClashArt(TEXT("home")));
	// Deep stacks also get HOME so players can jump straight to the main menu.
	if (bBack && Stack.Num() > 1)
		CChunky(TEXT("HOME"), ZCNavyBtn, [this]() { GoHome(); }, M + W + 16.f, Y, W, H, H * 0.36f, ClashArt(TEXT("home")));
}

void UZoogiMenuRoot::BuildLoadingTip()
{
	// Oct 22 queue #19: short tip while a screen opens / settles (visual only; A&P owns transition timing).
	if (!Canvas) return;
	static const TCHAR* Tips[] = {
		TEXT("Tip: Aim with a soft pull - short draws bank, long draws fly."),
		TEXT("Tip: Tap a card while aiming to charge it for the launch."),
		TEXT("Tip: Four Awaken pips fill from plays - spend them on your stone."),
		TEXT("Tip: In 2v2, bump your ally to start a Partner Follow-Up."),
		TEXT("Tip: Ring Marks hurt - stay on the floor when the bowl tilts."),
		TEXT("Tip: Settings and Story live on the home shelf - keep it clean."),
		TEXT("Tip: Rematch keeps the arena; Change Zoogi opens the picker."),
		TEXT("Tip: BACK steps one screen; HOME jumps to the main menu."),
	};
	const bool bShow = (Time < TipUntil) || AnyButtonEntering();
	if (!bShow && TipUntil > 0.f && Time > TipUntil + 0.45f) return;
	const float Fade = bShow ? 1.f : FMath::Clamp(1.f - (Time - TipUntil) / 0.45f, 0.f, 1.f);
	if (Fade <= 0.01f) return;
	const FString Tip = Tips[TipIdx % UE_ARRAY_COUNT(Tips)];
	const float Tw = FMath::Min(DesignW - 80.f, 920.f), Th = 52.f;
	const float Tx = DesignW * 0.5f - Tw * 0.5f;
	// Oct 22 portrait-clutter: tip used to sit above BACK/HOME and cover CONFIRM / START / primary actions.
	// Park it under the top bar (HitTestInvisible) so content stays tappable; clash matchmaking keeps bottom placement.
	float Ty = 78.f; // under BuildTopBar (~Y 20..72)
	const bool bClashMM = ClashOn() && Top() == EZoogiScreen::Home && ClashPopup == EZoogiClashPopup::Matchmaking;
	if (bClashMM)
	{
		Ty = DesignH - Th - 18.f;
		if (NeedsHomeButton())
		{
			const float BH = FMath::Max(88.f, DesignH * 0.07f);
			const float BY = ClashNavY() - BH - 24.f;
			if (BY > DesignH * 0.5f) Ty = BY - Th - 12.f;
		}
	}
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UOverlaySlot* S = O->AddChildToOverlay(CBox(ZCHex(TEXT("1A1C22")).CopyWithNewOpacity(0.82f * Fade), ZCGold.CopyWithNewOpacity(0.55f * Fade), 3.f, 16.f)))
	{ S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	if (UOverlaySlot* S = O->AddChildToOverlay(CTxt(Tip, 22.f, FLinearColor(1.f, 0.95f, 0.82f, Fade), 2.f)))
	{ S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(18.f, 6.f)); }
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	// Add tip first-ish among late chrome: still HitTestInvisible; top Y keeps it off primary CTAs.
	CAt(O, Tx, Ty, Tw, Th);
}
