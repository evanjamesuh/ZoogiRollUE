// arena-hud (Oct 20, Evan "complete gameplay integration"): the full-screen in-match HUD in the approved Zoogy Roll HUD style
// (carved stone, metallic gold, sapphire / arcane gems). The live 3D arena fills the whole screen; this layer draws over it:
// top bar (4 Zoogi portraits with element badge, score bar and Awaken pips), TOP phase/round + charge crystals (NO mid-screen
// EPISODE box / gauge panel), combo callout, pause + match clock, the phase banner with its progress bar, the card row, the
// selected-card detail panel, ACTIVATE (commit) and AWAKEN (spend a full charge). Portrait and landscape layouts; the tray
// phase / DRAW banner rests at the top (slides up on turn change). Phase text above crystals at top only (Evan Oct 23). Bottom card tray removed.
// Every control calls the same AZoogiGameMode::MenuAction paths as the older HUDs: no rule changes here.
// On by default ([ZoogiRoll.UI] HudStyle=Arena); HudStyle=Framed (or -ZoogiFramedHUD / -ZoogiHudStyle=Framed) restores the
// framed layout. While it is on, the framed layout, the legacy card hand and the Canvas Awaken panels stay off.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiArenaHUDWidget.generated.h"

class AZoogiGameMode;
class UBorder;
class UButton;
class UCanvasPanel;
class UZoogiP5ResultsWidget;
class UImage;
class UTextBlock;
class UTexture2D;

namespace ZoogiArenaHUD
{
	/** HudStyle=Arena (default) and not forced off. Read once. */
	ZOOGIROLL_API bool IsEnabled();
	/** Enabled and a match is on screen (not the main menu). */
	ZOOGIROLL_API bool IsActive(const UWorld* World);
	/** Board Battle (portrait): screen fractions its top plates / bottom tray cover, for the portrait camera band. False when off. */
	ZOOGIROLL_API bool BoardBand(const UWorld* World, float& OutTop, float& OutBottom);
	ZOOGIROLL_API void SetBoardBand(float Top, float Bottom);
	/** Oct 27: the TAC camera button on screen (absolute Slate px) - the DEV button sits just left of it. False when hidden. */
	ZOOGIROLL_API bool TacRect(FVector2D& OutAbsPos, FVector2D& OutAbsSize);
	/** Oct 28: a player's avatar ring on screen (absolute Slate px) - the comic reaction bubbles stream out of it. False when hidden. */
	ZOOGIROLL_API bool AvatarRect(int32 Player, FVector2D& OutAbsPos, FVector2D& OutAbsSize);
	/** b79: viewport pixel is on the MENU button or a profile avatar (a tap there is UI, never a target pick) */
	ZOOGIROLL_API bool OverChrome(const UWorld* World, const FVector2D& ViewportPx);
	/** b81: absolute Slate point on the MENU button -> open / close the pause menu; true when it was */
	ZOOGIROLL_API bool PressMenuAt(const FVector2D& AbsPos);
	/** Oct 28: the TAC button's current fade (1 = fully shown) - the DEV button fades with it. */
	ZOOGIROLL_API float TacOpacity();
	/** Oct 22: where crit / anime cut-in videos play, in viewport pixels: top centre, 16:9, under 30% (portrait) / 42%
	 *  (landscape) of the screen height. Used by AZoogiHUD::DrawAnime; the panels slide clear of it. */
	ZOOGIROLL_API FBox2D AnimeVideoRect(const FVector2D& Viewport);
	/** Oct 22: panel split ease (smooth in-out cubic), T in [0,1]. */
	ZOOGIROLL_API float SplitEase(float T);
	/** Seconds the panels take to slide apart / back together. */
	constexpr float SplitSeconds = 0.3f;
	/** Oct 22 top intro: per-piece fly-in, stagger, logo hold, minimize delay, peek length (seconds). */
	constexpr float IntroPiece = 0.45f, IntroStagger = 0.12f, LogoHold = 4.5f, MiniDelay = 5.5f, PeekSeconds = 3.0f;
	/** Ease-out with a small overshoot (0..1, slightly past 1 near the end). */
	ZOOGIROLL_API float BackEase(float T);
	/** Oct 30 (Evan): how far the avatar details (score bar, P1 / YOU, pips) are revealed after an avatar tap (0 = avatars only,
	 *  1 = shown, animated) and the bottom of that panel in viewport px - the SUPER meter slides down under it. */
	ZOOGIROLL_API float DetailsReveal(float& OutBottomPx);
	/** b116: a P5 banner is in the top band this frame - the profiles slide out of its way (the cut-in split) */
	ZOOGIROLL_API void NoteTopBanner();
	ZOOGIROLL_API bool TopBannerOn();
}

UCLASS()
class ZOOGIROLL_API UZoogiArenaHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Spawns (or keeps) the one HUD layer for this world. Called every frame by the viewport client. */
	static void Ensure(UWorld* World);
	/** b81: the MENU button's action (also reached from overlays that sit above it) */
	void ToggleMenu() { SetPaused(!bPaused); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;

private:
	AZoogiGameMode* GM() const;
	int32 LocalPlayer() const;

	// ---- build (whole tree rebuilt on orientation / size / hand changes; values refreshed every tick)
	void Build();
	UWidget* At(UWidget* W, float X, float Y, float Wd, float Ht);
	UBorder* Box(const FLinearColor& Fill, const FLinearColor& Edge, float EdgeW, float R);
	UTextBlock* Txt(const FString& S, float Size, const FLinearColor& C, float Outline, int32 Justify = 1);
	UTextBlock* Text(const FString& S, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, float Outline = 3.f, int32 Justify = 1);
	UImage* Img(UTexture2D* Tex, float X, float Y, float Wd, float Ht, float Radius = 0.f, bool bCrop = true, const FLinearColor& Tint = FLinearColor::White);
	UWidget* Stone(float X, float Y, float Wd, float Ht, float R, const FLinearColor& Fill, bool bGoldRim);
	UButton* Hit(float X, float Y, float Wd, float Ht, TFunction<void()> Fn);
	UTexture2D* Tex(const FString& Key);
	UTexture2D* FileTex(const FString& Dir, const FString& Name);
	UTexture2D* CardArt(FName CardId);
	void BuildTopBar();
	void BuildUnit(int32 Player, float Cx, float Top);
	void BuildEpisode(float X, float Y, float Wd);
	void BuildTray();
	void BuildCard(int32 CardSlot, float X, float Y, float Wd, float Ht);
	void BuildDetail(float X, float Y, float Wd, float Ht);
	void BuildPause();
	void BuildPauseP5(); // b70 (Evan): "restyle pause menu to P5 style"
	void BuildChromeCluster(float SettingsX, float SettingsY, bool bSettingsOnRight, bool bMark = true);
	/** Oct 28 (Evan): DEV / TAC / settings row centred under the round counter, shown only with the profiles (split Kind 4). */
	void BuildChromeRow(float CenterX, float Y);
	void RefreshChromeCam();
	void BuildResults();
	// ---- Board Battle (portrait, Oct 20 Evan reference Content/Data/MenuArt/board_ref.png, 941 x 1672): the reference's own
	// frames (top band with logo + DRAW..END stepper, the 4 player panels, arena / round plates, bottom tray) with every value
	// live on top; the real 3D match shows between them
	bool bBoard = false;
	void BuildBoard();
	void BuildComboBadge(float X0, float CY);
	void TickComboBadge(float Dt);
	/** Oct 22: portrait Board tray in its compact form (aim / roll): one slim row pinned to the bottom - 5 card tiles,
	 *  energy, AWAKEN - so the arena stays visible. Standby keeps the full reference tray. */
	void BuildBoardTrayCompact();
	bool BoardTrayCompact() const;
	void RefreshBoard(float Dt);
	UImage* RefCrop(float X0, float Y0, float X1, float Y1, float X, float Y, float Wd, float Ht);
	float BoardBotY = 0.f;        // design y of reference y 1255 (tray band top)
	TWeakObjectPtr<UWidget> StepGlow[5];
	/** Oct 22: widgets that slide apart while a cut-in video plays (left panels left, right panels right, top band up). */
	struct FSplitPart { TWeakObjectPtr<UWidget> W; FVector2D Dir; int32 Order = 0; uint8 Kind = 1; bool bFaded = false; };
	/** Oct 22 (Evan): top-of-screen intro + minimize. Part kinds: 0 logo/title (flies in, then leaves for good), 1 details
	 *  (fade out once minimized, back while peeking), 2 avatar (always shown), 3 shown only once the logo is gone,
	 *  4 (Oct 28) the DEV / TAC / settings row: like 1, and its buttons collapse while hidden. */
	int32 SplitOrder = 0;
	TArray<TWeakObjectPtr<UWidget>> AvatarW;
	/** Canvas children added since FirstChild are avatar pieces (kept when the HUD minimizes). Call before MarkSplit. */
	void MarkAvatar(int32 FirstChild);
	/** Tap an avatar: details come back for ZoogiArenaHUD::PeekSeconds, then minimize again. */
	void PeekDetails();
	float TopOnT = -1.f, LogoT = 0.f, MiniT = 0.f, PeekUntil = -1.f;
	bool bUserMini = false; // b67 (Evan): the profiles stay up until an avatar tap hides them (tap again = back)
	float DetailsBottomD = 0.f; // Oct 30: bottom of the avatar details in design units (set by the layout)
	uint32 TopShotsDone = 0;
	TArray<FSplitPart> SplitParts;
	int32 SplitShots = 0;         // -ZoogiShots captures of the split (max 3 per HUD)
	float SplitT = 0.f;           // 0 together .. 1 apart (linear clock, eased when applied)
	/** Canvas children added since FirstChild slide by Dir (design units) when split. */
	void MarkSplit(int32 FirstChild, const FVector2D& Dir, uint8 Kind = 1);
	void TickSplit(float Dt);
	TWeakObjectPtr<UTextBlock> StepTxt[5];
	TWeakObjectPtr<UTextBlock> ArenaTxt, DeckTxt, DiscardTxt, EnergyTxt;
	TWeakObjectPtr<UTextBlock> EpTxt[4];

	// ---- refresh
	void Refresh(float Dt);
	FString HandSignature() const;
	bool TrayWanted() const;
	void PressCard(int32 CardSlot);
	void PressActivate();
	void PressAwaken();
	void SetPaused(bool bOn);
	/** -ZoogiArenaHUDTest (with -ZoogiAuto -ZoogiShots=<dir>): plays the human P1 through this HUD's own buttons (card tap,
	 *  ACTIVATE, AWAKEN when full) plus lock-on aim, and screenshots each step - a full human 2v2 match without a person. */
	void TickTestDriver(float Dt);
	int32 TestTurnKey = -1, TestShotN = 0, TestRolls = 0;
	float TestT = 0.f;
	uint32 TestDone = 0;
	void TestShot(const TCHAR* Name);

	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas; float HudShakeLog = -10.f; // Oct 22: HUD rumble log throttle
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> TexCache;
	UPROPERTY() TArray<TObjectPtr<UObject>> Clicks; // click forwarders of the current tree
	UPROPERTY() TObjectPtr<UZoogiP5ResultsWidget> P5Results; // Oct 30: P5 results screen of this game over (kept across rebuilds)

	FVector2D BuiltSize = FVector2D::ZeroVector;
	FString BuiltHand;
	bool bBuiltTray = false, bBuiltPause = false, bBuiltOver = false;
	bool bPortrait = true;
	float S = 1.f;            // px per design unit (design = 1080 wide in portrait, 1080 tall in landscape)
	FVector2D Design = FVector2D(1080.f, 1920.f);
	float Time = 0.f;
	float MatchClock = 0.f;
	bool bPaused = false;
	int32 DetailSlot = 0;     // card shown in the detail panel (last tapped)
	float TrayT = 0.f;        // 0 collapsed .. 1 open (slides the tray)

	struct FUnitRefs { TWeakObjectPtr<UWidget> Ring, Active, Bar; TWeakObjectPtr<UImage> BarFill; TArray<TWeakObjectPtr<UWidget>> Pips; TArray<TWeakObjectPtr<UWidget>> Wolves; TWeakObjectPtr<UTextBlock> Score; TWeakObjectPtr<UTextBlock> SP; TWeakObjectPtr<UTextBlock> PipMore; float BarW = 0.f; TWeakObjectPtr<UImage> Avi; TWeakObjectPtr<UTexture2D> AviBase; bool bAviUlt = false; bool bAviClip = false; FSlateBrush AviSaved; TWeakObjectPtr<UObject> AviRes; bool bBig = false; TArray<TWeakObjectPtr<UWidget>> SpSegs, ActiveExtra; TWeakObjectPtr<UTextBlock> CrysN; };
	void BuildBigBadge(int32 Player, float X, float Y, float Wd, float Ht); // b171d: big corner badge
	void SyncBigBadge(class AZoogiGameMode* G, FUnitRefs& U, int32 i);
	static bool BigBadges();
	float BigBottom = 0.f;
	void SyncAviClip(class AZoogiGameMode* G, FUnitRefs& U, int32 i); // b88: Wolf Pack summon clip in the avatar circle
	TWeakObjectPtr<class UImage> ClipImg; TWeakObjectPtr<class UBorder> ClipFrame; TWeakObjectPtr<UObject> ClipRes; float ClipK = 0.f, ClipAsp = 1.f; int32 ClipWho = -1; // b90d: the profile-corner clip square
	void SetSPText(class AZoogiGameMode* G, FUnitRefs& U, int32 i); // b69 // build 28: Avi swaps to the ultimate face while transformed
	FUnitRefs Units[4];
	TArray<TWeakObjectPtr<UWidget>> GaugeSegs, Crystals;
	TWeakObjectPtr<UWidget> GaugeStar;
	TWeakObjectPtr<UTextBlock> RoundTxt, RoundOfTxt, ClockTxt, TacLbl, BannerTxt, ComboNum, ActivateCost, ActivateLbl;
	TWeakObjectPtr<UWidget> TacGlow, TacPlate;
	int32 LastCamKey = -1; // Oct 27: refresh the camera button when the view changes (Cinematic / Normal / Tac)
	TWeakObjectPtr<UWidget> ComboNumBox, ComboShadeBox;
	TWeakObjectPtr<UTextBlock> ComboShade;
	float ComboPopT = 9.f, ComboHoldT = 0.f;
	int32 ComboLastN = 0;
	TWeakObjectPtr<UWidget> ComboRoot, BannerFill, TrayRoot, AwakenGlow, ActivateDim, TurnTag;
	TWeakObjectPtr<UTextBlock> TurnTagTxt;
	float BannerFillW = 0.f;
	// #11 turn banner polish
	float TurnBannerPop = 0.f;
	int32 TurnBannerKey = -1; // player*10 + (mine?1:0) + stage*100 - pop when identity changes
	TArray<TWeakObjectPtr<UWidget>> AwakenPips;
	struct FCardRefs { TWeakObjectPtr<UWidget> Root, Glow, Dim; };
	TArray<FCardRefs> Cards;
	float TrayY0 = 0.f, TrayHidden = 0.f; // tray translation: 0 open, TrayHidden collapsed (design units)
	TWeakObjectPtr<UWidget> BannerRoot;
	float BannerShift = 0.f;              // legacy: unused once tray is removed (banner rests at top)
	// DRAW / turn banner: rest near top; slide up from mid on phase/turn change (~0.32s ease)
	float BannerRestY = 100.f;            // design Y of the DRAW pill (tag sits above)
	float BannerSlideT = 1.f;             // 0 = mid start .. 1 = rested at top
	float BannerSlideFromY = 0.f;         // design Y offset at slide start (positive = below rest)
};
