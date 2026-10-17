// menu (Oct 14, Evan approved the mockups 8:45 AM CT): the new main menu. One C++ UMG root with a screen stack
// (Home, Play setup, Zoogi select, Powers, Settings, Collection, Story, Events, Shop, Social, Deck) that replaces
// UZoogiMenuWidget's role (that file stays in the tree, unused, for an easy revert). Layout is authored in the mockup's
// 2400x1080 landscape space and fitted to any viewport; touch targets >= 120 px, the primary action bottom right.
// Every button that starts or configures a match calls the existing AZoogiGameMode::MenuAction entry points.
#pragma once

#include "CoreMinimal.h"
#include "ZoogiUIStyle.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiFrontProfile.h" // clash-menu: FZoogiFrontReward
#include "ZoogiMenuRoot.generated.h"

class UCanvasPanel;
class UOverlay;
class UTexture2D;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;
class AZoogiGameMode;
class UButton;
class UTextBlock;

/** One live menu button + its animation state (not a UPROPERTY: weak refs, rebuilt with the screen). */
struct FZoogiMenuAnimBtn { TWeakObjectPtr<UButton> B; TWeakObjectPtr<class UWidget> Gloss; ZoogiUIStyle::FButtonAnim A; float Start = 0.f; };

enum class EZoogiMenuAnchor : uint8 { Left, Center, Right };

/** clash-menu: popups of the Clash-style front end (ZoogiMenuClash.cpp). */
enum class EZoogiClashPopup : uint8 { None, Profile, Settings, TrophyRoad, Info, Deal, Chest, Gold, Rewards, Card, MatchResult, ChestSlot, Arenas, BattlePass, PremiumUnlock };

UENUM()
enum class EZoogiScreen : uint8
{
	Home, Play, ZoogiSelect, Powers, Settings, Collection, Story, Events, Shop, Social, Deck // Deck replaced Create (Oct 14)
};

/** One per button: runs a bound lambda. */
UCLASS()
class ZOOGIROLL_API UZoogiMenuRootClick : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void()> Fn;
	UFUNCTION() void Click() { if (Fn) Fn(); }
};

UCLASS()
class ZOOGIROLL_API UZoogiMenuRoot : public UUserWidget
{
	GENERATED_BODY()
public:
	// ---- same surface the HUD used on UZoogiMenuWidget
	/** Called by the HUD every frame while the menu is up: back keys, tour, rebuild when dirty. */
	void Refresh(float DeltaSeconds);
	/** HUD calls this when the menu is shown again (start or return from a match): back to HOME. */
	void MarkDirty();
	void SetBackdropPlaying(bool bPlay);

	// ---- screen stack
	void Push(EZoogiScreen S);
	void Pop();
	void PopToHome();
	/** Top-left BACK, Android back, Esc. */
	void HandleBack();
	EZoogiScreen Top() const { return Stack.Num() ? Stack.Last() : EZoogiScreen::Home; }

	/** Forward to AZoogiGameMode::MenuAction (unchanged match logic). */
	void Fire(FName Action, int32 Index);
	void ShowComingSoon(const FString& What);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	void Rebuild();
	AZoogiGameMode* GM() const;
	UTexture2D* Art(const FString& Name);
	FString PortraitFor(FName ZoogiId) const;

	// layout helpers (coordinates in the 1024x461 mockup preview space; see .cpp)
	using EAnchor = EZoogiMenuAnchor;
	UZoogiMenuRootClick* BindClick(UButton* B, TFunction<void()> Fn);
	UWidget* Place(UWidget* W, float X, float Y, float Wd, float Ht, EAnchor A = EAnchor::Center);
	class UTextBlock* Txt(const FString& S, int32 Size, const FLinearColor& C, float Outline = 3.f, bool bWrap = false);
	UWidget* Label(const FString& S, float X, float Y, float Wd, float Ht, int32 Size, const FLinearColor& C, EAnchor A = EAnchor::Center, int32 Justify = 0, bool bWrap = false);
	UWidget* Panel(float X, float Y, float Wd, float Ht, const FLinearColor& Fill, EAnchor A = EAnchor::Center);
	UWidget* Btn(const FString& Label, const FLinearColor& Fill, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, int32 FontSize = 30,
		bool bSelected = false, bool bDisabled = false, EAnchor A = EAnchor::Center, const FString& Icon = FString(), const FString& Sub = FString());
	UWidget* ImgTile(UTexture2D* Tex, const FString& Caption, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, bool bSelected, bool bLocked,
		const FLinearColor& Frame, bool bCaptionBottomLeft = true, int32 CaptionSize = 22, const FLinearColor& ImgTint = FLinearColor::White);
	UWidget* Image(UTexture2D* Tex, float X, float Y, float Wd, float Ht, const FLinearColor& Tint = FLinearColor::White, EAnchor A = EAnchor::Center, float Radius = 0.f);
	UWidget* Bar(float X, float Y, float Wd, float Ht, float Frac, const FLinearColor& A, const FLinearColor& B);
	UWidget* Tabs(const TArray<FString>& Names, int32& Current, float X, float Y, float TabW, float Gap, bool bVertical = false);

	void BuildTopBar(const FString& Title);
	void BuildHome();
	void BuildPlay();
	void BuildZoogiSelect();
	void BuildPowers();
	void BuildSettings();
	void BuildCollection();
	void BuildStory();
	void BuildEvents();
	void BuildShop();
	void BuildSocial();
	void BuildDeck();
	FName DeckCardPower(FName CardId, const struct FZoogiCardDef* Def) const;
	void BuildModal();
	void BuildAnimeView();

	void ApplyPlayMode(int32 Mode);
	void CycleSlot(int32 Slot);
	int32 CurrentPlayMode() const;
	void SetLocalSetting(const TCHAR* Key, int32 Value);
	int32 GetLocalSetting(const TCHAR* Key, int32 Default) const;
	void TickTour(float Dt);
	/** Content/Data/PowerArt/<id>.png, else a procedural placeholder card (bOutPlaceholder = true). */
	UTexture2D* PowerArt(const struct FZoogiPowerDef& D, bool& bOutPlaceholder);
	void PowerPlaceholderText(const struct FZoogiPowerDef& D, float X, float Y, float Wd, float Ht);
	TSet<FName> PlaceholderPowers;
	void OnUnlocksChanged() { bDirty = true; }

	UPROPERTY() TObjectPtr<UOverlay> Root;
	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TArray<TObjectPtr<UZoogiMenuRootClick>> Clicks;
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> ArtCache;

	TArray<EZoogiScreen> Stack;
	bool bDirty = true;
	int32 BuiltRevision = -1;
	FVector2D BuiltViewport = FVector2D::ZeroVector;
	float DesignW = 2400.f;
	float Time = 0.f;
	// button animation (ZoogiUIStyle::FButtonAnim per live button)
	TArray<FZoogiMenuAnimBtn> AnimBtns;
	int32 AnimScreen = -1;
	bool bAnimEnter = false;
	void AddAnim(class UButton* B, UWidget* Gloss, bool bPrimary, bool bDisabled);
	void TickAnims(float Dt);
	/** True while any button is still in its screen-enter pop-in (tour captures wait for it). */
	bool AnyButtonEntering() const;
	// -ZoogiMenuClip=<dir>: per-frame captures of the button animations
	FString ClipDir; int32 ClipFrame = -1; float ClipClock = 0.f; bool ClipHover = false, ClipPress = false;
	void TickClip(float Dt);
	FDelegateHandle UnlockHandle;

	// screen state
	int32 SelectSlot = 0, PendingZoogi = -1;
	FName PowerSel;
	int32 PowerFilter = 0;           // 0 Void, 1 all types
	int32 SettingsTab = 0, CollectionTab = 0, CollectionSel = 3, EventsTab = 0, ShopTab = 0, ShopSel = 0, SocialTab = 0, StorySel = 1, DeckFilter = 0;
	FName DeckSel, DeckPreview;   // Deck screen: selected card, card shown in the large preview (None = closed)
	// modal
	bool bModal = false, bModalQuit = false;
	FString ModalTitle, ModalBody;
	// WATCH ANIME
	bool bAnimeView = false;
	FString AnimeClip;
	// backdrop video (ported from UZoogiMenuWidget; off unless [ZoogiRoll.Menu] bMenuVideo or -ZoogiMenuVideo)
	bool EnsureBackdrop();
	UPROPERTY() TObjectPtr<UMediaPlayer> BackdropPlayer;
	UPROPERTY() TObjectPtr<UMediaTexture> BackdropTexture;
	UPROPERTY() TObjectPtr<UFileMediaSource> BackdropSource;
	int32 BackdropState = 0;
	// -ZoogiMenuTour=<dir>: automated screen captures (engine viewport screenshots), then starts a match from PLAY.
	FString TourDir;
	int32 TourStep = -1;
	float TourClock = 0.f;
	bool bTourSetupDone = false;

	// ---- clash-menu (Oct 17, milestone 1): Clash-style front end. HOME = Battle tab of a 5-tab shell with the persistent
	// top bar + bottom nav (ZoogiMenuClash.cpp). Default on; -ZoogiClassicMenu brings the previous HOME back.
	bool ClashOn() const;
	bool ClashHome() const { return ClashOn() && Top() == EZoogiScreen::Home && !bAnimeView; }
	void SetClashTab(int32 T);
	void OpenClashPopup(EZoogiClashPopup P, const FString& Title = FString(), const FString& Body = FString());
	void BuildClashBackdrop();
	void BuildClash();
	void BuildClashTopBar();
	void BuildClashBattleTab();
	void BuildClashEmptyTab(const FString& Title, const FString& Sub, bool bDeckButton);
	void BuildClashNav();
	void BuildClashPopup();
	void TickClash(float Dt);
	void TickClashTour(float Dt);
	// clash layout helpers: raw DesignW x 1080 canvas units
	UWidget* CAt(UWidget* W, float X, float Y, float Wd, float Ht);
	class UTextBlock* CTxt(const FString& S, float Size, const FLinearColor& C, float Outline = 4.f);
	UWidget* CText(const FString& S, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, int32 Justify = 1, float Outline = 4.f, bool bWrap = false);
	UWidget* CImg(UTexture2D* Tex, float X, float Y, float Wd, float Ht, float Radius = 0.f, bool bCrop = false, const FLinearColor& Tint = FLinearColor::White);
	class UBorder* CBox(const FLinearColor& Fill, const FLinearColor& Edge, float EdgeW, float R);
	UWidget* CPlate(float X, float Y, float Wd, float Ht, const FLinearColor& Fill, float R, float EdgeW, bool bGloss);
	UWidget* CGoldPanel(float X, float Y, float Wd, float Ht, const FLinearColor& Inner, float R = 22.f);
	UButton* CButton(UWidget* Content, TFunction<void()> Fn, bool bPrimary = false);
	UCanvasPanel* CGroupBegin();
	void CGroupEnd();
	UWidget* CChunkyW(UWidget* Inner, const FLinearColor& Face, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, bool bPrimary = false, float R = -1.f);
	UWidget* CChunky(const FString& Label, const FLinearColor& Face, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, float FontSize, UTexture2D* Icon = nullptr, bool bPrimary = false);
	UWidget* CBar(float X, float Y, float Wd, float Ht, float Frac, const FLinearColor& Fill, const FString& Text, float FontSize);
	UWidget* CDot(float X, float Y, float S, const FString& Txt = FString());
	/** Procedurally rasterized clash-menu art (icons, ribbon, backdrop), cached in ArtCache under "clash:<key>". */
	UTexture2D* ClashArt(const FString& Key);
	int32 ClashTab = 2;           // 0 Shop, 1 Cards, 2 Battle (default), 3 Clan, 4 Leaderboard
	int32 ClashBuiltTab = -1;
	EZoogiClashPopup ClashPopup = EZoogiClashPopup::None;
	float ClashPopupT0 = 0.f;
	FString ClashInfoTitle, ClashInfoBody;
	TWeakObjectPtr<UWidget> ClashPopupW, ClashPopupScrim;
	TArray<UCanvasPanel*> ClashCanvasStack;
	FString ClashTourDir; int32 ClashTourStep = -1; float ClashTourClock = 0.f; bool bClashTourSetup = false;

	// ---- clash-menu milestone 2: Shop, Cards (Decks / Collection / Emotes), chests, match rewards (ZoogiMenuClashShop.cpp)
	void BuildClashShopTab();
	void BuildClashCardsTab();
	void BuildClashChestSlots(float Y);
	/** Popup bodies added in milestone 2. Returns false when the popup kind is not one of them. */
	bool BuildClashShopPopup(EZoogiClashPopup P, float PW, float PH);
	FVector2D ClashPopupSize(EZoogiClashPopup P) const;
	void ClashBuy(TFunction<void()> Fn);          // spinner ~0.8 s, then Fn (applies the purchase)
	void ClashShowRewards(const FString& Title, FName Chest, const TArray<FZoogiFrontReward>& R);
	void ClashFail(const FString& Why);
	void ClashTapChestSlot(int32 SlotIdx);
	UWidget* CChunkyVisual(UWidget* Inner, const FLinearColor& Face, float X, float Y, float Wd, float Ht, float R = -1.f);
	UWidget* CCard(FName Id, float X, float Y, float Wd, float Ht, bool bLocked, const FString& Badge = FString(), bool bLevel = false);
	UWidget* CLive(TFunction<FString()> Fn, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, int32 Justify = 1, float Outline = 3.f);
	UWidget* CSection(const FString& Title, float Y);
	int32 ClashSel = -1;                 // deal index / chest slot / gold pack
	FName ClashSelId;                    // card id / chest type
	FString ClashRewardsTitle; FName ClashRewardsChest;
	TArray<FZoogiFrontReward> ClashRewards;
	float ClashBuyT0 = -1.f;
	TFunction<void()> ClashBuyFn;
	int32 ClashCardsSub = 0;             // 0 Decks, 1 Collection, 2 Emotes
	int32 ClashDeckView = -1;            // deck being edited (-1 = the active deck)
	float ClashScrollOffset = 0.f; float ClashPendingScroll = -1.f; int32 ClashScrollKey = -1; int32 ClashScrollApply = 0;
	float ClashShopY[4] = { 0.f, 0.f, 0.f, 0.f }; // deals, chests, gold, gems
	float ClashLockedY = 0.f;
	TWeakObjectPtr<class UScrollBox> ClashScrollW;
	TWeakObjectPtr<UWidget> ClashSpinW, ClashRaysW;
	TArray<TPair<TWeakObjectPtr<UTextBlock>, TFunction<FString()>>> ClashLive;
	float ClashLiveClock = 0.f;
	bool bClashChestWasUnlocking = false;
	bool bClashTourInMatch = false;
	// ---- DETAILED PASS 1 (Oct 17): Arenas screen, Trophy Road 0-5000, Battle Pass Season 8 (ZoogiMenuClashHome.cpp)
	bool BuildClashHomePopup(EZoogiClashPopup P, float PW, float PH);
	bool ClashBigPopup(EZoogiClashPopup P) const;
	EZoogiClashPopup ClashAfterRewards = EZoogiClashPopup::None; // popup to return to after COLLECT
	int32 ClashArenaSel = -1;            // Arenas screen: selected track marker (trophies)
};
