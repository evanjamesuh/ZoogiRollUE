// MODE SELECTION (Evan Oct 26): the Marble Clash "MODE SELECTION" screen, landscape / wide screen. It is the only menu while
// we test: the game opens on it (no title / home / play setup) and a card tap starts that mode right away.
//   BACK tab + MODE SELECTION title (top left), gems / coins pills (top right), a sideways-scrolling row of tilted cards:
//   BATTLE ROYALE, 2V2 BATTLE, CLASSIC, TRAINING, STORY. Art: Content/Data/MenuArt/ModeSelect (Scripts/make_mode_select.py),
//   font: Content/Data/Fonts/LilitaOne-Regular.ttf (SIL OFL). -ZoogiOldMenu brings the old menus back.
#include "ZoogiArena.h"
#include "ZoogiVoyagerLevels.h" // voyager port
#include "ZoogiShooter.h" // b234
#include "ZoogiMenuRoot.h"
#include "ZoogiRaid.h"
#include "ZoogiParty.h" // b273
#include "ZoogiLoadScreen.h" // b207
#include "ZoogiTrickShot.h"
#include "ZoogiSkillLab.h"
#include "ZoogiStory.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "ZoogiGameMode.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiArenaRoute.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiP5Style.h"
#include "ZoogiFront.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"

namespace ZoogiModeSel
{
	// reference geometry (design units, 1080 tall): cards 603 x 641 (with the player tab), 660 apart, first one 234 in
	constexpr float CardW = 603.f, CardH = 641.f, Pitch = 660.f, Lead = 234.f, CardTop = 222.f;

	TSharedPtr<const FCompositeFont> Font()
	{
		static TSharedPtr<const FCompositeFont> F;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			FString P = FPaths::ProjectContentDir() / TEXT("Data/Fonts/LilitaOne-Regular.ttf");
			if (!FPaths::FileExists(P)) P = FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Black.ttf");
			if (FPaths::FileExists(P)) F = MakeShared<FStandaloneCompositeFont>(FName(TEXT("Regular")), P, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		}
		return F;
	}

	FSlateFontInfo FontInfo(float Size, float Outline)
	{
		TSharedPtr<const FCompositeFont> C = Font();
		FSlateFontInfo F = C.IsValid() ? FSlateFontInfo(C, Size, FName(TEXT("Regular"))) : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		F.OutlineSettings.OutlineSize = FMath::RoundToInt(Outline);
		F.OutlineSettings.OutlineColor = FLinearColor(0.08f, 0.1f, 0.12f, 1.f);
		return F;
	}

	struct FMap { const TCHAR* Art; const TCHAR* Name; const TCHAR* Hex; EZoogiArena Arena; };
	// Oct 29 (Evan): classic map choices (card art = an in-game shot of the arena, Scripts/make_mode_select.py)
	const FMap Maps[] = {
		{ TEXT("ModeSelect/ms_map_frozen"), TEXT("FROZEN RING"), TEXT("1F7FD0"), EZoogiArena::Frozen },
		{ TEXT("ModeSelect/ms_map_meadow"), TEXT("MEADOW ARENA"), TEXT("3FA34D"), EZoogiArena::Meadow },
		{ TEXT("ModeSelect/ms_map_volcanic"), TEXT("VOLCANIC PIT"), TEXT("E2541C"), EZoogiArena::Volcanic },
		{ TEXT("ModeSelect/ms_map_cosmic"), TEXT("COSMIC PLATFORM"), TEXT("6206D3"), EZoogiArena::Cosmic },
		{ TEXT("ModeSelect/ms_map_arabian"), TEXT("ARABIAN NIGHTS"), TEXT("E0A010"), EZoogiArena::Arabian },
		{ TEXT("ModeSelect/ms_map_tomb"), TEXT("PHARAOH'S TOMB"), TEXT("B07A2A"), EZoogiArena::Tomb },
		{ TEXT("ModeSelect/ms_map_neon"), TEXT("NIGHT CIRCUIT"), TEXT("00A8D3"), EZoogiArena::Neon },
		// b202 (Evan: "delete teaser arena"): no TEASER ARENA card
		{ TEXT("ModeSelect/ms_map_darkrealm"), TEXT("DARK REALM"), TEXT("5B2A86"), EZoogiArena::Cosmic }, // b203 (Evan): the Underrealm (Grok's Shadow Realm v3 on the Cosmic court) as a map
	};
	// b154a (Evan): 2V2 BATTLE -> MULTIPLAYER, RAID under CLASSIC; Id drives StartModeCard (not the position)
	enum class ECardId : uint8 { Royale, Multi, Classic, Raid, Training, Story, TrickShot, ShooterHold, ShooterRoyale, ShooterTeam, Voyager, Party, Koth, HotOrb, OrbStorm, BossRush, TrickCourse }; // b234, b241 Voyager, b273 PARTY
	struct FCard { const TCHAR* Art; const TCHAR* Name; const TCHAR* Sub; const TCHAR* Elem; ECardId Id; };
	const FCard Cards[] = {
		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY"), TEXT("EPISODES 1 - 3"), TEXT("Void"), ECardId::Story }, // b156 (Evan): STORY at the top
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("BATTLE ROYALE"), TEXT("LAST ZOOGI STANDING"), TEXT("Fire"), ECardId::Royale },
		// b265 (Evan: "Delete voyager mode"): the VOYAGER card is gone, so its 3 shooter modes can no longer be reached
		// { TEXT("ModeSelect/ms_card_voyager"), TEXT("VOYAGER"), TEXT("3RD PERSON SHOOTER - 3 MODES"), TEXT("Shock"), ECardId::Voyager }, // b241 (Evan: "PUT MODES 3,4,5 IN TO A NEW VOYAGER MODE")
		// b273 (Evan: "finish still to do" - the review's new modes): PARTY MODES, a card that opens 5 modes (the old shooter cards are gone)
		{ TEXT("ModeSelect/ms_card_multi"), TEXT("PARTY MODES"), TEXT("5 NEW WAYS TO PLAY"), TEXT("Shock"), ECardId::Party },
		{ TEXT("ModeSelect/ms_card_classic"), TEXT("KING OF THE HILL"), TEXT("HOLD THE CENTRE RING"), TEXT("Fire"), ECardId::Koth },
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("HOT ORB"), TEXT("DON'T TOUCH THE BOMB LAST"), TEXT("Fire"), ECardId::HotOrb },
		{ TEXT("ModeSelect/ms_card_training"), TEXT("ORB STORM"), TEXT("90 SECONDS OF RAINING ORBS"), TEXT("Frost"), ECardId::OrbStorm },
		{ TEXT("ModeSelect/ms_card_raid"), TEXT("BOSS RUSH"), TEXT("3 TITANS, ONE POOL OF LIVES"), TEXT("Void"), ECardId::BossRush },
		{ TEXT("ModeSelect/ms_card_story"), TEXT("TRICK COURSE"), TEXT("3 HOLES - FEWEST SHOTS WINS"), TEXT("Shock"), ECardId::TrickCourse },
		{ TEXT("ModeSelect/ms_card_multi"), TEXT("MULTIPLAYER"), TEXT("2v2 ONLINE BATTLES"), TEXT("Shock"), ECardId::Multi },
		{ TEXT("ModeSelect/ms_card_classic"), TEXT("CLASSIC"), TEXT("ORIGINAL ZOOGI BATTLES"), TEXT(""), ECardId::Classic },
		{ TEXT("ModeSelect/ms_card_raid"), TEXT("RAID"), TEXT("CO-OP BOSS CHALLENGES"), TEXT("Void"), ECardId::Raid }, // b151 (Evan): co-op vs the map's titan
		{ TEXT("ModeSelect/ms_card_training"), TEXT("TRAINING"), TEXT("LEARN \u2022 PRACTICE \u2022 MASTER"), TEXT("Frost"), ECardId::Training },
	};
	bool GRaidMaps = false; // b151: the map cards were opened from RAID
	bool GCountSel = false; // b154a: MULTIPLAYER - the 2 / 3 / 4 PLAYERS step
	bool GVoySel = false; // b241: the VOYAGER card's 3 modes
	bool IsVoyMode(ECardId Id) { return Id == ECardId::Koth || Id == ECardId::HotOrb || Id == ECardId::OrbStorm || Id == ECardId::BossRush || Id == ECardId::TrickCourse; } // b273: the PARTY sub-cards (was the shooter cards)
	int32 GMultiN = 0;      // b154a: > 0 = the map cards start a pass-and-play match with this many human players
}

bool UZoogiMenuRoot::ModeSelectOn() const
{
	static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMenu"));
	return !bOld;
}

void UZoogiMenuRoot::BuildFront()
{
	UZoogiFrontWidget* W = WidgetTree->ConstructWidget<UZoogiFrontWidget>(UZoogiFrontWidget::StaticClass());
	W->Menu = this;
	if (UOverlaySlot* Sl = Root->AddChildToOverlay(W)) { Sl->SetHorizontalAlignment(HAlign_Fill); Sl->SetVerticalAlignment(VAlign_Fill); }
	UE_LOG(LogZoogi, Log, TEXT("FRONT built (equipped %s)"), ZoogiFront::Equipped(this).Name);
}

void UZoogiMenuRoot::BuildP5Menu()
{
	using namespace ZoogiModeSel;
	UZoogiP5MenuWidget* W = WidgetTree->ConstructWidget<UZoogiP5MenuWidget>(UZoogiP5MenuWidget::StaticClass());
	ZoogiP5::FMenuSpec& S = W->Spec;
	TWeakObjectPtr<UZoogiMenuRoot> Self = this;
	S.Ctx = this;
	// Oct 31: -ZoogiMapSelectTest opens CLASSIC -> MAP SELECT once with the last card (Ancient Collision Meadows) selected (menu proof)
	static bool bMapTestDone = false;
	const bool bMapTest = !bMapTestDone && FParse::Param(FCommandLine::Get(), TEXT("ZoogiMapSelectTest"));
	if (bMapTest) { bMapTestDone = true; bMapSelect = true; }
	S.bMapList = bMapSelect;
	{ static bool bVT = false; if (!bVT && FParse::Param(FCommandLine::Get(), TEXT("ZoogiVoySelTest"))) { bVT = true; GVoySel = true; } } // b241 preview
	S.TitleA = GCountSel ? TEXT("PLAYERS") : bMapSelect ? TEXT("MAP") : GVoySel ? TEXT("PARTY") : TEXT("MODE");
	S.TitleB = TEXT("SELECT");
	if (GCountSel && !bMapSelect)
	{	// b154a MULTIPLAYER: how many players share this device
		for (int32 n = 2; n <= 4; ++n)
		{
			ZoogiP5::FItem It;
			It.Name = FString::Printf(TEXT("%d PLAYERS"), n); It.Sub = TEXT("PASS THE PHONE - TAKE TURNS");
			UTexture2D* T = Art(TEXT("ModeSelect/ms_card_multi")); It.Art = T; if (T) W->Keep.Add(T);
			It.Col = ZoogiP5::ElementColor(TEXT("Shock"));
			It.Fn = [Self, n]() { if (!Self.IsValid()) return; GCountSel = false; GMultiN = n; Self->bMapSelect = true; Self->bTrainingMaps = false; Self->bDirty = true; UE_LOG(LogZoogi, Log, TEXT("MULTIPLAYER: %d players -> map select"), n); };
			S.Items.Add(It);
		}
	}
	else if (bMapSelect)
	{
		if (bTrainingMaps)
		{	// b166 (Evan): the EP1 stop-1 LEAGUE COURT is a training level
			ZoogiP5::FItem It;
			It.Name = TEXT("LEAGUE COURT"); It.Sub = TEXT("TRAINING  -  EP1 PRACTICE COURT");
			UTexture2D* T = Art(TEXT("ModeSelect/ms_map_training")); It.Art = T; if (T) W->Keep.Add(T);
			UTexture2D* Tf = Art(TEXT("ModeSelect/ms_map_training_p")); It.ArtFull = Tf; if (Tf) W->Keep.Add(Tf);
			It.Col = FLinearColor::FromSRGBColor(FColor(122, 58, 203));
			It.Fn = [Self]() { if (Self.IsValid()) Self->StartMapCard(100); };
			S.Items.Add(It);
		}
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Maps); ++i)
		{
			ZoogiP5::FItem It;
			It.Name = Maps[i].Name; It.Sub = TEXT("CLASSIC  -  4 ZOOGIS");
			UTexture2D* T = Art(Maps[i].Art); It.Art = T; if (T) W->Keep.Add(T);
			UTexture2D* Tf = Art(FString(Maps[i].Art) + TEXT("_p")); It.ArtFull = Tf; if (Tf) W->Keep.Add(Tf); // b120: portrait full-screen shot
			It.Col = FLinearColor::FromSRGBColor(FColor::FromHex(Maps[i].Hex));
			It.Fn = [Self, i]() { if (Self.IsValid()) Self->StartMapCard(i); };
			S.Items.Add(It);
		}
		if (bMapTest) S.StartSel = S.Items.Num() - 1;
	}
	else
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Cards); ++i)
		{
			if (GVoySel != IsVoyMode(Cards[i].Id)) continue; // b241: the 3 shooter modes live under the VOYAGER card
			ZoogiP5::FItem It;
			It.Name = Cards[i].Name; It.Sub = Cards[i].Sub; // b154a: per-card (the old tables were one short once RAID was added)
			UTexture2D* T = Art(Cards[i].Art); It.Art = T; if (T) W->Keep.Add(T);
			It.Col = Cards[i].Elem[0] ? ZoogiP5::ElementColor(Cards[i].Elem) : ZoogiP5::Accent();
			It.Fn = [Self, i]() { if (Self.IsValid()) Self->StartModeCard(i); };
			S.Items.Add(It);
		}
	}
	S.Back = [Self]() { if (!Self.IsValid()) return; if (ZoogiModeSel::GVoySel && !Self->bMapSelect) { ZoogiModeSel::GVoySel = false; Self->bDirty = true; return; } if (ZoogiModeSel::GCountSel && !Self->bMapSelect) { ZoogiModeSel::GCountSel = false; Self->bDirty = true; return; } if (Self->bMapSelect) { ZoogiModeSel::GMultiN = 0; ZoogiModeSel::GRaidMaps = false; Self->bMapSelect = false; Self->bDirty = true; UE_LOG(LogZoogi, Log, TEXT("MAP SELECT back to MODE SELECTION")); } else if (ZoogiFront::On()) { Self->bFrontBattle = false; Self->bDirty = true; UE_LOG(LogZoogi, Log, TEXT("MODE SELECT back to the front HOME")); } };
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	S.Gems = FP ? FP->Gems() : 0;
	S.Coins = FP ? FP->Data().Gold : 0;
	S.Shot = [Self](const FString& N) { if (Self.IsValid()) if (AZoogiGameMode* G = Self->GM()) G->TakeShot(N); };
	if (UOverlaySlot* Sl = Root->AddChildToOverlay(W)) { Sl->SetHorizontalAlignment(HAlign_Fill); Sl->SetVerticalAlignment(VAlign_Fill); }
	UE_LOG(LogZoogi, Log, TEXT("P5 MENU built: %s (%d items)"), bMapSelect ? TEXT("MAP SELECT") : TEXT("MODE SELECT"), S.Items.Num());
}

void UZoogiMenuRoot::BuildModeSelect()
{
	using namespace ZoogiModeSel;
	if (ZoogiFront::On() && !bFrontBattle && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5MenuTest"))) { BuildFront(); return; } // Nov 1 b52: the new front end first
	if (ZoogiP5::MenuOn()) { BuildP5Menu(); return; }
	const float Aspect = BuiltViewport.X / FMath::Max(1.f, BuiltViewport.Y);
	// Oct 28 (Evan): "Menu in portrait should convert to fit portrait" - a phone held upright gets a 1080-wide layout:
	// top bar, title, then the cards in a two-column grid (scrolls vertically when the screen is short)
	const bool bPortrait = BuiltViewport.Y > BuiltViewport.X;
	DesignH = bPortrait ? FMath::Clamp(1080.f / FMath::Max(0.1f, Aspect), 1500.f, 2600.f) : 1080.f;
	DesignW = bPortrait ? 1080.f : FMath::Clamp(1080.f * Aspect, 1300.f, 2600.f);
	auto MakeImage = [this](UTexture2D* T, float W, float H) -> UImage*
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush B;
		B.SetResourceObject(T);
		B.ImageSize = FVector2D(W, H);
		I->SetBrush(B);
		if (!T) I->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.f));
		return I;
	};
	auto At = [this](UWidget* W, float X, float Y, float Wd, float Ht)
	{
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W))
		{
			S->SetAnchors(FAnchors(0.f, 0.f));
			S->SetPosition(FVector2D(X, Y));
			S->SetSize(FVector2D(Wd, Ht));
		}
		return W;
	};

	// full-bleed backdrop (sky-blue bands, faint ZOOGI ROLL watermark, dark top strip)
	{
		UScaleBox* Fill = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Fill->SetStretch(EStretch::ScaleToFill);
		UImage* I = MakeImage(Art(TEXT("ModeSelect/ms_bg")), 2400.f, 1080.f);
		Fill->AddChild(I);
		Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* S = Root->AddChildToOverlay(Fill)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	}
	USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass());
	if (UOverlaySlot* S = Root->AddChildToOverlay(Safe)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Fit->SetStretch(EStretch::ScaleToFit);
	Safe->AddChild(Fit);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(DesignW);
	Size->SetHeightOverride(DesignH);
	Fit->AddChild(Size);
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Size->AddChild(Canvas);

	// cards: the reference shows ~3.2 cards across; narrower screens scale the row down so three still fit
	// Oct 29 (Evan): CLASSIC opens a MAP SELECTION step (same look: one card per arena with a real shot of that map, BACK
	// returns to the modes). The same card row / grid builds either list.
	struct FMenuCard { FString Art; FString Name; FLinearColor Col; TFunction<void()> Fn; };
	TArray<FMenuCard> List;
	if (bMapSelect)
	{
		if (bTrainingMaps) List.Add({ FString(TEXT("ModeSelect/ms_map_training")), FString(TEXT("LEAGUE COURT")), FLinearColor::FromSRGBColor(FColor(122, 58, 203)), [this]() { StartMapCard(100); } }); // b166
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Maps); ++i)
			if (!(ZoogiModeSel::GRaidMaps && Maps[i].Arena == EZoogiArena::Meadow)) // b219 (Evan Nov 10: "remove meadows arena from raid")
			List.Add({ FString(Maps[i].Art), FString(Maps[i].Name), FLinearColor::FromSRGBColor(FColor::FromHex(Maps[i].Hex)), [this, i]() { StartMapCard(i); } });
	}
	else
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Cards); ++i)
			List.Add({ FString(Cards[i].Art), FString(Cards[i].Name), FLinearColor::FromSRGBColor(FColor(182, 14, 101)), [this, i]() { StartModeCard(i); } });
	}
	// a card picture, or (art not on this device yet) a plain plate in the card colour with the name
	auto CardFace = [this, &MakeImage](const FMenuCard& C) -> UWidget*
	{
		if (UTexture2D* T = Art(C.Art)) return MakeImage(T, CardW, CardH);
		UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(C.Col);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(10.f, 10.f, 10.f, 10.f), FSlateColor(FLinearColor::Black), 8.f);
		Plate->SetBrush(B);
		Plate->SetHorizontalAlignment(HAlign_Center);
		Plate->SetVerticalAlignment(VAlign_Bottom);
		Plate->SetPadding(FMargin(10.f, 10.f, 10.f, 46.f));
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(C.Name));
		T->SetFont(FontInfo(46.f, 4.f));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(255, 241, 168))));
		T->SetJustification(ETextJustify::Center);
		T->SetAutoWrapText(true);
		Plate->SetContent(T);
		return Plate;
	};
	const float CS = bPortrait ? 486.f / CardW : FMath::Clamp(DesignW / (Lead + 3.2f * Pitch), 0.6f, 1.f);
	const bool bSameView = ModeScroll.IsValid() && bPortrait == bModePortrait && bMapSelect == bModeMapView;
	const float KeepScroll = bSameView ? ModeScroll->GetScrollOffset() : 0.f; // a rebuild (window resize, return from a match) keeps the row where it was
	bModePortrait = bPortrait;
	bModeMapView = bMapSelect;
	const int32 N = List.Num();
	if (bPortrait)
	{
		// portrait: two-column grid (odd last card centred), 36 apart, under the title
		const float GridY = 214.f, Gap = 36.f, CWp = CardW * CS, CHp = CardH * CS;
		UScrollBox* Col = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Col->SetOrientation(Orient_Vertical);
		Col->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		Col->SetAllowOverscroll(true);
		Col->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		for (int32 r = 0; r * 2 < N; ++r)
		{
			UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			for (int32 c = 0; c < 2 && r * 2 + c < N; ++c)
			{
				const int32 i = r * 2 + c;
				USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Box->SetWidthOverride(CWp);
				Box->SetHeightOverride(CHp);
				const TFunction<void()> Fn = List[i].Fn;
				Box->AddChild(CButton(CardFace(List[i]), [Fn]() { Fn(); }, i == 0));
				if (UHorizontalBoxSlot* HS = RowBox->AddChildToHorizontalBox(Box)) HS->SetPadding(FMargin(c == 0 ? 0.f : Gap, 0.f, 0.f, 0.f));
			}
			if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(Col->AddChild(RowBox)))
			{
				S->SetPadding(FMargin(0.f, r == 0 ? 0.f : Gap, 0.f, 0.f));
				S->SetHorizontalAlignment(HAlign_Center);
			}
		}
		USpacer* Tail = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Tail->SetSize(FVector2D(10.f, 60.f));
		Col->AddChild(Tail);
		At(Col, 0.f, GridY, DesignW, DesignH - GridY);
		Col->SetScrollOffset(KeepScroll);
		ModeScroll = Col;
	}
	else
	{
		UScrollBox* Row = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Row->SetOrientation(Orient_Horizontal);
		Row->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		Row->SetAllowOverscroll(true);
		Row->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		const float RowY = 120.f, RowH = 900.f;
		const float Top = (CardTop + CardH * 0.5f) - CardH * 0.5f * CS - RowY; // keep the cards centred on the reference's middle
		USpacer* LeadSp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		LeadSp->SetSize(FVector2D(Lead * CS, 10.f));
		Row->AddChild(LeadSp);
		for (int32 i = 0; i < N; ++i)
		{
			USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Box->SetWidthOverride(CardW * CS);
			Box->SetHeightOverride(CardH * CS);
			const TFunction<void()> Fn = List[i].Fn;
			UButton* B = CButton(CardFace(List[i]), [Fn]() { Fn(); }, i == 0);
			Box->AddChild(B);
			if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(Row->AddChild(Box)))
			{
				S->SetPadding(FMargin(0.f, Top, (Pitch - CardW) * CS, 0.f));
				S->SetVerticalAlignment(VAlign_Top);
			}
		}
		USpacer* Tail = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Tail->SetSize(FVector2D(Lead * CS, 10.f));
		Row->AddChild(Tail);
		At(Row, 0.f, RowY, DesignW, RowH);
		Row->SetScrollOffset(KeepScroll);
		ModeScroll = Row;
	}

	// top bar: BACK, title, gems + coins
	{
		UButton* Back = CButton(MakeImage(Art(TEXT("ModeSelect/ms_back")), 262.f, 82.f), [this]()
		{
			// MAP SELECTION: back to the modes. MODE SELECTION: nothing behind it while we test - the card row returns to the start
			if (bMapSelect) { bMapSelect = false; bDirty = true; UE_LOG(LogZoogi, Log, TEXT("MAP SELECT back to MODE SELECTION")); }
			else if (UScrollBox* R = ModeScroll.Get()) R->ScrollToStart();
		});
		At(Back, 0.f, 0.f, 262.f, 82.f);
		UImage* Title = MakeImage(Art(bMapSelect ? TEXT("ModeSelect/ms_title_map") : TEXT("ModeSelect/ms_title")), 520.f, 80.f);
		Title->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (bPortrait) At(Title, DesignW * 0.5f - 260.f, 108.f, 520.f, 80.f); // portrait: own line under the bar
		else At(Title, 350.f, 14.f, 520.f, 80.f);
		const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
		const int32 Gems = FP ? FP->Gems() : 0;
		const int32 Gold = FP ? FP->Data().Gold : 0;
		const struct { const TCHAR* Pill; int32 Value; float Right; } Pills[] = { { TEXT("ModeSelect/ms_pill_gem"), Gems, 839.f }, { TEXT("ModeSelect/ms_pill_coin"), Gold, 473.f } };
		for (const auto& P : Pills)
		{
			const float X = bPortrait ? DesignW - (P.Right > 600.f ? 692.f : 348.f) : DesignW - P.Right; // portrait: both pills packed right of BACK
			UImage* I = MakeImage(Art(P.Pill), 332.f, 76.f);
			I->SetVisibility(ESlateVisibility::HitTestInvisible);
			At(I, X, 0.f, 332.f, 76.f);
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			T->SetText(FText::AsNumber(P.Value, &FNumberFormattingOptions::DefaultNoGrouping()));
			T->SetFont(FontInfo(44.f, 3.f));
			T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			T->SetShadowOffset(FVector2D(0.f, 3.f));
			T->SetShadowColorAndOpacity(FLinearColor(0.08f, 0.1f, 0.12f, 1.f));
			T->SetJustification(ETextJustify::Center);
			T->SetVisibility(ESlateVisibility::HitTestInvisible);
			At(T, X + 88.f, 8.f, 160.f, 60.f);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("MODE SELECT built (%.0f x %.0f design, card scale %.2f, %s)"), DesignW, DesignH, CS, bPortrait ? TEXT("portrait grid") : TEXT("landscape row"));
}

void UZoogiMenuRoot::StartModeCard(int32 Card)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	const TCHAR* Name = Card >= 0 && Card < (int32)UE_ARRAY_COUNT(ZoogiModeSel::Cards) ? ZoogiModeSel::Cards[Card].Name : TEXT("?");
	UE_LOG(LogZoogi, Log, TEXT("MODE SELECT card %d %s"), Card, Name);
	auto CPUs = [G]() { for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0; };
	const ZoogiModeSel::ECardId Id = Card >= 0 && Card < (int32)UE_ARRAY_COUNT(ZoogiModeSel::Cards) ? ZoogiModeSel::Cards[Card].Id : ZoogiModeSel::ECardId::Story;
	if (Id != ZoogiModeSel::ECardId::BossRush) ZoogiParty::Arm(G, ZoogiParty::EKind::None); // b273: any other card clears a party mode
	if (ZoogiModeSel::IsVoyMode(Id))
	{	// b273 PARTY MODES
		ZoogiModeSel::GVoySel = false; ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; ZoogiModeSel::GCountSel = false; bMapSelect = false; bTrainingMaps = false;
		ZoogiShooter::SetKind(0);
		const ZoogiParty::EKind K = Id == ZoogiModeSel::ECardId::Koth ? ZoogiParty::EKind::Koth : Id == ZoogiModeSel::ECardId::HotOrb ? ZoogiParty::EKind::HotOrb : Id == ZoogiModeSel::ECardId::OrbStorm ? ZoogiParty::EKind::OrbStorm : Id == ZoogiModeSel::ECardId::BossRush ? ZoogiParty::EKind::BossRush : ZoogiParty::EKind::TrickCourse;
		ZoogiParty::Arm(G, K);
		ZoogiLoadScreen::ArmMode(); // b283: a loading screen with this mode's how-to
		Fire("Mode", 0);
		if (K == ZoogiParty::EKind::TrickCourse)
		{	// solo, no bots
			if (G->MenuArena != EZoogiArena::Meadow) Fire("Arena", (int32)EZoogiArena::Meadow);
			G->ClassicStartTraining();
			return;
		}
		if (K == ZoogiParty::EKind::BossRush)
		{	// you + 2 CPU allies, the Volcanic Pit titan first
			Fire("Players", 3); CPUs();
			if (G->MenuArena != EZoogiArena::Volcanic) Fire("Arena", (int32)EZoogiArena::Volcanic);
			ZoogiRaid::Arm(G);
			Fire("Start", 0);
			return;
		}
		Fire("Players", 4); CPUs();
		if (G->MenuArena != EZoogiArena::Meadow) Fire("Arena", (int32)EZoogiArena::Meadow);
		if (K == ZoogiParty::EKind::OrbStorm) Fire("Rounds", 30); // the clock ends it, not the rounds
		Fire("Start", 0);
		return;
	}
	if (Id == ZoogiModeSel::ECardId::Party)
	{	// b273: open the PARTY modes (the b241 VOYAGER sub-list)
		ZoogiModeSel::GVoySel = true; ZoogiModeSel::GCountSel = false; ZoogiModeSel::GRaidMaps = false; bMapSelect = false; bTrainingMaps = false; bDirty = true;
		return;
	}
	if (ZoogiModeSel::IsVoyMode(Id)) ZoogiModeSel::GVoySel = false; // b241: back on the main list next time
	if (Id == ZoogiModeSel::ECardId::Multi)
	{	// b154a MULTIPLAYER: players step (P5 menu) -> map select; the older card menu goes straight to the maps with 2 players
		ZoogiModeSel::GRaidMaps = false;
		if (ZoogiP5::MenuOn()) { ZoogiModeSel::GCountSel = true; ZoogiModeSel::GMultiN = 0; }
		else { ZoogiModeSel::GMultiN = 2; bMapSelect = true; }
		bTrainingMaps = false; bDirty = true;
		return;
	}
	if (Id == ZoogiModeSel::ECardId::TrickShot)
	{	// b156 DAILY TRICK SHOT (b157: started from the main-menu pop-up, StartTrickShot)
		StartTrickShot();
		return;
	}
	if (false)
	{
		ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; bMapSelect = false; bTrainingMaps = false;
		Fire("Mode", 0);
		Fire("Players", 4);
		CPUs();
		const EZoogiArena Ta = ZoogiTrickShot::TodayArena();
		if (G->MenuArena != Ta) Fire("Arena", (int32)Ta);
		ZoogiTrickShot::Arm(G);
		Fire("Start", 0);
		return;
	}
	if ((Id == ZoogiModeSel::ECardId::ShooterHold || Id == ZoogiModeSel::ECardId::ShooterRoyale || Id == ZoogiModeSel::ECardId::ShooterTeam)
		&& ZoogiVoyLevels::Open(G, Id == ZoogiModeSel::ECardId::ShooterHold ? 1 : Id == ZoogiModeSel::ECardId::ShooterRoyale ? 2 : 3)) return; // Nov 11 (Evan): the REAL Voyager game
	if (Id == ZoogiModeSel::ECardId::ShooterHold || Id == ZoogiModeSel::ECardId::ShooterRoyale || Id == ZoogiModeSel::ECardId::ShooterTeam)
	{	// b234: the SHOOTER modes - the royale engine on Ancient Collision Meadows, everyone on foot as a Voyager soldier
		ZoogiShooter::SetKind(Id == ZoogiModeSel::ECardId::ShooterHold ? 1 : Id == ZoogiModeSel::ECardId::ShooterRoyale ? 2 : 3);
		Fire("Mode", 2);
		if (G->MenuArena != EZoogiArena::Meadow) Fire("Arena", (int32)EZoogiArena::Meadow); // b237 (Evan): Ancient Collision Meadows is out of Battle Royale
		Fire("Start", 0);
		return;
	}
	if (!ZoogiModeSel::IsVoyMode(Id)) ZoogiShooter::SetKind(0); // b234, b243: any non-shooter card clears a leftover shooter mode
	Card = Id == ZoogiModeSel::ECardId::Royale ? 0 : Id == ZoogiModeSel::ECardId::Classic ? 2 : Id == ZoogiModeSel::ECardId::Training ? 3 : Id == ZoogiModeSel::ECardId::Story ? 4 : 5; // the b151 numbering below
	ZoogiModeSel::GMultiN = 0;
	switch (Card)
	{
	case 0: // BATTLE ROYALE: 4 Zoogis, you + 3 CPU - Oct 31 (Evan: "the larger field we are building is for battle royale only"): on Ancient Collision Meadows
		ZoogiLoadScreen::ArmMode(); // b283
		Fire("Mode", 2);
		if (G->MenuArena != EZoogiArena::Meadow) Fire("Arena", (int32)EZoogiArena::Meadow); // b237 (Evan): Ancient Collision Meadows is out of Battle Royale
		Fire("Start", 0);
		break;
	case 1: // 2V2 BATTLE: you + CPU ally vs 2 CPUs (the old BATTLE / PLAY match, no search screen)
	{
		Fire("Mode", 1);
		const EZoogiArena Want = ZoogiArenaRoute::Resolve(EZoogiMatchRoute::HomeBattle, G->MenuArena);
		if (G->MenuArena != Want) Fire("Arena", (int32)Want);
		CPUs();
		Fire("Start", 0);
		break;
	}
	case 2: // CLASSIC: Oct 29 (Evan) - pick the arena first (MAP SELECTION), then 4-player free-for-all turns there
		bMapSelect = true; bTrainingMaps = false;
		bDirty = true;
		break;
	case 3: // TRAINING: b63 (Evan) - map select first, then you alone on that map (no bots) with your skill hand; -ZoogiOldTraining = the royale sandbox
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldTraining"))) { Fire("Mode", 0); ClashStartTraining(); break; }
		bMapSelect = true; bTrainingMaps = true;
		bDirty = true;
		break;
	case 5: // b151 RAID: pick a map (each has its titan), then you + 2 CPU allies against it
		bMapSelect = true; bTrainingMaps = false; ZoogiModeSel::GRaidMaps = true;
		bDirty = true;
		break;
	case 4: // STORY: Oct 30 - STORY MODE (ZoogiStory.h); -ZoogiStoryAnime = the old episode 1 anime clip
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryAnime"))) { if (!PlayStoryAnimeById(TEXT("Episode_01_TheFrozenRing"))) ShowComingSoon(TEXT("No anime clips found (Content/Anime/Movies).")); }
		else ZoogiStory::Open(GetWorld());
		break;
	default:
		break;
	}
}

void UZoogiMenuRoot::StartTrickShot()
{	// b157 (Evan): the DAILY TRICK SHOT pop-up's PLAY - today's arena, you + 3 rivals that never shoot, one shot per try
	AZoogiGameMode* G = GM();
	if (!G) return;
	ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; ZoogiModeSel::GCountSel = false;
	bMapSelect = false; bTrainingMaps = false; SetFrontBattle(false);
	UE_LOG(LogZoogi, Log, TEXT("DAILY TRICK SHOT -> today's shot"));
	Fire("Mode", 0);
	Fire("Players", 4);
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0;
	const EZoogiArena Ta = ZoogiTrickShot::TodayArena();
	if (G->MenuArena != Ta) Fire("Arena", (int32)Ta);
	ZoogiTrickShot::Arm(G);
	ZoogiLoadScreen::ArmMode(); // b283
	Fire("Start", 0);
}

void UZoogiMenuRoot::StartSkillLab()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	bMapSelect = false; bTrainingMaps = false; SetFrontBattle(false);
	UE_LOG(LogZoogi, Log, TEXT("SKILL LAB -> solo lab match"));
	Fire("Mode", 0);
	if (G->MenuArena == EZoogiArena::Frozen || G->MenuArena == EZoogiArena::Ancient) Fire("Arena", (int32)EZoogiArena::Meadow); // b110: no roam bridges in the lab
	ZoogiSkillLab::RequestLab();
	G->ClassicStartTraining();
}

void UZoogiMenuRoot::StartMapCard(int32 Index)
{
	if (Index == 100)
	{	// b166 (Evan "Add episode 1 lvl 1 arena as training level"): TRAINING on the LEAGUE COURT
		AZoogiGameMode* G2 = GM();
		if (!G2) return;
		bMapSelect = false; bTrainingMaps = false;
		Fire("Mode", 0);
		if (G2->MenuArena != EZoogiArena::Training) Fire("Arena", (int32)EZoogiArena::Training);
		UE_LOG(LogZoogi, Log, TEXT("MAP SELECT training: LEAGUE COURT"));
		ZoogiLoadScreen::ArmMode(); // b283
		G2->ClassicStartTraining();
		return;
	}
	AZoogiGameMode* G = GM();
	if (!G || Index < 0 || Index >= (int32)UE_ARRAY_COUNT(ZoogiModeSel::Maps)) return;
	const ZoogiModeSel::FMap& M = ZoogiModeSel::Maps[Index];
	UE_LOG(LogZoogi, Log, TEXT("MAP SELECT card %d %s"), Index, M.Name);
	bMapSelect = false;
	if (bTrainingMaps)
	{
		bTrainingMaps = false;
		Fire("Mode", 0);
		if (G->MenuArena != M.Arena) Fire("Arena", (int32)M.Arena);
		ZoogiLoadScreen::ArmMode(); // b283
		G->ClassicStartTraining(); // b63: solo on the picked map
		return;
	}
	Fire("Mode", 0);
	const bool bRaid = ZoogiModeSel::GRaidMaps; ZoogiModeSel::GRaidMaps = false;
	const int32 MultiN = ZoogiModeSel::GMultiN; ZoogiModeSel::GMultiN = 0;
	Fire("Players", MultiN > 0 ? MultiN : bRaid ? 3 : 4); // b151 RAID: you + 2 CPU allies; b154a MULTIPLAYER: N humans
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = MultiN > 0 ? false : i > 0;
	if (MultiN > 0) UE_LOG(LogZoogi, Log, TEXT("MULTIPLAYER pass & play: %d human players on %s"), MultiN, M.Name);
	{	// b203: DARK REALM = the Cosmic court dressed as the Underrealm - flipping it rebuilds the arena
		const bool bDark = FCString::Strcmp(M.Name, TEXT("DARK REALM")) == 0;
		const bool bFlip = ZoogiUnderrealm::Picked() != bDark;
		ZoogiUnderrealm::SetPicked(bDark);
		if (G->MenuArena != M.Arena || (bFlip && M.Arena == EZoogiArena::Cosmic)) Fire("Arena", (int32)M.Arena);
	}
	if (bRaid) { ZoogiRaid::Arm(G); UE_LOG(LogZoogi, Log, TEXT("RAID armed on %s"), M.Name); }
	ZoogiLoadScreen::Arm(bRaid, M.Name, M.Art); // b207 (Evan): the RAID / CLASSIC loading screen with tips, then TAP TO START
	Fire("Start", 0);
}
