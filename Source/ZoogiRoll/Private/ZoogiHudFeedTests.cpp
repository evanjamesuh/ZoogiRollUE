// hud-feed automation test (ZoogiRoll.Hud.FeedPanel): the PIP live camera panel sits in the square stone slot right of the
// deck on the landscape hud-v3 layout, is hidden in portrait / HUD-hidden play, shows the idle panel without a texture (or
// while inactive) and draws the texture (fitted into the slot) when one is set and active.
#include "ZoogiHudFeed.h"
#include "ZoogiFramedLayout.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiHudFeedPanelTest, "ZoogiRoll.Hud.FeedPanel", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZoogiHudFeedPanelTest::RunTest(const FString&)
{
	using namespace ZoogiHudFeed;
	UTextureRenderTarget2D* const SavedTex = ZoogiFramed::GetHudFeedTexture();
	const bool bSavedActive = ZoogiFramed::IsHudFeedActive();
	const bool bSavedHidden = ZoogiFramed::IsHudHidden();
	ZoogiFramed::SetHudHidden(false); // HUD shown = the landscape hud-v3 layout

	// 1) landscape 1920x1080: a square slot right of the deck, left of the card recess, inside the tray
	const ZoogiFramed::FLayout L = ZoogiFramed::Compute(FVector2D(1920, 1080), FMargin(0.f), 1.f);
	FBox2D R;
	TestTrue(TEXT("1920x1080: slot present"), ZoogiFramed::HudFeedSlot(L, R));
	const FVector2D Sz = R.GetSize();
	TestTrue(TEXT("slot is square"), FMath::IsNearlyEqual(Sz.X, Sz.Y, 0.5f));
	TestTrue(TEXT("slot size = 162 target px * T"), FMath::IsNearlyEqual(Sz.X, 162.f * L.T, 0.5f));
	TestTrue(TEXT("slot rect at 1920x1080 = (253.1, 829.7)-(480.9, 1057.5)"), R.Min.Equals(FVector2D(253.125f, 829.6875f), 0.6f) && R.Max.Equals(FVector2D(480.9375f, 1057.5f), 0.6f));
	TestTrue(TEXT("slot right of the deck card"), R.Min.X > L.Deck.X + 60.f * L.T);
	TestTrue(TEXT("slot ends before the card recess"), R.Max.X < L.TrayLeftX1 && R.Max.X < L.Hand.Min.X);
	TestTrue(TEXT("slot inside the tray band"), R.Min.Y >= L.Tray.Min.Y && R.Max.Y <= L.Viewport.Y);
	TestFalse(TEXT("slot never over the arena"), R.Intersect(L.Arena));
	// other landscape shapes (phone 19.5:9 / 20:9, 4:3-ish width-limited, safe-area notch): still a square in the deck slab
	for (const FVector2D V : { FVector2D(2340, 1080), FVector2D(2400, 1080), FVector2D(1280, 720), FVector2D(1440, 1080) })
	{
		const ZoogiFramed::FLayout Lv = ZoogiFramed::Compute(V, FMargin(V.X > 2000.f ? 60.f : 0.f, 0.f, 0.f, 0.f), 1.f);
		FBox2D Rv;
		const bool bOk = ZoogiFramed::HudFeedSlot(Lv, Rv);
		TestTrue(FString::Printf(TEXT("%.0fx%.0f: slot present, square, between deck and recess, below the arena"), V.X, V.Y),
			bOk && FMath::IsNearlyEqual(Rv.GetSize().X, Rv.GetSize().Y, 0.5f) && Rv.Min.X > Lv.Deck.X && Rv.Max.X < Lv.Hand.Min.X && Rv.Min.Y > Lv.Arena.Max.Y && Rv.Max.Y <= V.Y + 0.5f);
	}

	// 2) portrait viewport / HUD-hidden layout / no layout: no panel at all
	ZoogiFramed::FLayout Lp = L; Lp.Viewport = FVector2D(1080, 1920);
	TestFalse(TEXT("portrait viewport: no slot"), ZoogiFramed::HudFeedSlot(Lp, R));
	ZoogiFramed::FLayout Lh = L; Lh.Left = Lh.Right = FBox2D(FVector2D(0, 0), FVector2D(0, 0)); Lh.Arena = FBox2D(FVector2D(0, 0), L.Viewport);
	TestFalse(TEXT("HUD hidden (full-screen arena, no columns): no slot"), ZoogiFramed::HudFeedSlot(Lh, R));
	ZoogiFramed::FLayout Lq = L; Lq.bPortrait = true;
	TestFalse(TEXT("portrait play layout (bPortrait): no slot"), ZoogiFramed::HudFeedSlot(Lq, R));
	ZoogiFramed::SetHudHidden(true);
	const ZoogiFramed::FLayout Lhh = ZoogiFramed::Compute(FVector2D(1920, 1080), FMargin(0.f), 1.f);
	TestFalse(TEXT("HUD hidden (real portrait-play layout, even in a landscape window): no slot"), ZoogiFramed::HudFeedSlot(Lhh, R));
	TestTrue(TEXT("HUD hidden: panel hidden (real layout)"), Plan(Lhh).State == EPanel::Hidden);
	ZoogiFramed::SetHudHidden(false);
	ZoogiFramed::FLayout Li = L; Li.bValid = false;
	TestFalse(TEXT("invalid layout: no slot"), ZoogiFramed::HudFeedSlot(Li, R));
	ZoogiFramed::SetHudFeedActive(true);
	TestTrue(TEXT("portrait: panel hidden even when active"), Plan(Lp).State == EPanel::Hidden);
	TestTrue(TEXT("HUD hidden: panel hidden"), Plan(Lh).State == EPanel::Hidden);

	// 3) idle: no texture (active or not), or texture but inactive
	ZoogiFramed::SetHudFeedTexture(nullptr);
	ZoogiFramed::SetHudFeedActive(true);
	FPlan P = Plan(L);
	TestTrue(TEXT("no texture: idle panel"), P.State == EPanel::Idle && P.Texture == nullptr);
	{ FBox2D R0; ZoogiFramed::HudFeedSlot(L, R0); TestTrue(TEXT("idle panel in the slot"), P.Slot.Min.Equals(R0.Min) && P.Slot.Max.Equals(R0.Max) && P.Image.GetSize().X > 0.f); }
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RT->SizeX = 256; RT->SizeY = 256; // Game Camera's persistent 256x256 square (no GPU resource needed for the plan)
	ZoogiFramed::SetHudFeedTexture(RT);
	ZoogiFramed::SetHudFeedActive(false);
	TestTrue(TEXT("texture but inactive: idle panel"), Plan(L).State == EPanel::Idle);

	// 4) live: texture set + active -> draws it in the framed inset, square 256 maps 1:1 (no crop)
	ZoogiFramed::SetHudFeedActive(true);
	TestTrue(TEXT("GetHudFeedTexture returns the setter's texture"), ZoogiFramed::GetHudFeedTexture() == RT);
	P = Plan(L);
	TestTrue(TEXT("texture + active: live panel"), P.State == EPanel::Live && P.Texture == RT);
	TestTrue(TEXT("picture inside the slot frame"), P.Slot.IsInside(P.Image.Min) && P.Slot.IsInside(P.Image.Max) && P.Image.GetSize().X > P.Slot.GetSize().X * 0.85f);
	TestTrue(TEXT("picture square"), FMath::IsNearlyEqual(P.Image.GetSize().X, P.Image.GetSize().Y, 0.5f));
	TestTrue(TEXT("256x256: full UV"), P.UV.Min.Equals(FVector2f(0.f, 0.f)) && P.UV.Max.Equals(FVector2f(1.f, 1.f)));
	TestTrue(TEXT("rounded mask"), P.ImageRadius > 0.f && P.Radius >= P.ImageRadius);
	RT->SizeX = 512; RT->SizeY = 256; // a wide feed is centre-cropped to fill the square, never letterboxed
	P = Plan(L);
	TestTrue(TEXT("2:1 feed: centre crop U 0.25..0.75"), FMath::IsNearlyEqual(P.UV.Min.X, 0.25f, 0.01f) && FMath::IsNearlyEqual(P.UV.Max.X, 0.75f, 0.01f) && P.UV.Min.Y == 0.f && P.UV.Max.Y == 1.f);
	RT->SizeX = 256;
	TestTrue(TEXT("portrait never draws the live feed"), Plan(Lp).State == EPanel::Hidden);

	// 5) weakly held: a destroyed texture drops back to the idle panel
	ZoogiFramed::SetHudFeedTexture(nullptr);
	TestTrue(TEXT("cleared texture: idle"), Plan(L).State == EPanel::Idle);

	ZoogiFramed::SetHudFeedTexture(SavedTex);
	ZoogiFramed::SetHudFeedActive(bSavedActive);
	ZoogiFramed::SetHudHidden(bSavedHidden);
	return true;
}

#endif
