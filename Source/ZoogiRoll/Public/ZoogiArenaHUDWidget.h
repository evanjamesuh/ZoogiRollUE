// arena-hud (Oct 20, Evan "complete gameplay integration"): the full-screen in-match HUD in the approved Zoogy Roll HUD style
// (carved stone, metallic gold, sapphire / arcane gems). The live 3D arena fills the whole screen; this layer draws over it:
// top bar (4 Zoogi portraits with element badge, score bar and Awaken pips; Episode panel with the team gauge, round and
// card-energy crystals), combo callout, pause + match clock, the phase banner with its progress bar, the card row, the
// selected-card detail panel, ACTIVATE (commit) and AWAKEN (spend a full charge). Portrait and landscape layouts; the tray
// collapses to the banner while the arena needs the space (aiming, rolling, CPU turns) so the HUD never covers the shot.
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
class UImage;
class UTextBlock;
class UTexture2D;

namespace ZoogiArenaHUD
{
	/** HudStyle=Arena (default) and not forced off. Read once. */
	ZOOGIROLL_API bool IsEnabled();
	/** Enabled and a match is on screen (not the main menu). */
	ZOOGIROLL_API bool IsActive(const UWorld* World);
}

UCLASS()
class ZOOGIROLL_API UZoogiArenaHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Spawns (or keeps) the one HUD layer for this world. Called every frame by the viewport client. */
	static void Ensure(UWorld* World);

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
	void BuildResults();

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

	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TMap<FString, TObjectPtr<UTexture2D>> TexCache;
	UPROPERTY() TArray<TObjectPtr<UObject>> Clicks; // click forwarders of the current tree

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

	struct FUnitRefs { TWeakObjectPtr<UWidget> Ring, Active, Bar; TWeakObjectPtr<UImage> BarFill; TArray<TWeakObjectPtr<UWidget>> Pips; TWeakObjectPtr<UTextBlock> Score; float BarW = 0.f; };
	FUnitRefs Units[4];
	TArray<TWeakObjectPtr<UWidget>> GaugeSegs, Crystals;
	TWeakObjectPtr<UWidget> GaugeStar;
	TWeakObjectPtr<UTextBlock> RoundTxt, RoundOfTxt, ClockTxt, BannerTxt, ComboNum, ActivateCost, ActivateLbl;
	TWeakObjectPtr<UWidget> ComboRoot, BannerFill, TrayRoot, AwakenGlow, ActivateDim, TurnTag;
	TWeakObjectPtr<UTextBlock> TurnTagTxt;
	float BannerFillW = 0.f;
	TArray<TWeakObjectPtr<UWidget>> AwakenPips;
	struct FCardRefs { TWeakObjectPtr<UWidget> Root, Glow, Dim; };
	TArray<FCardRefs> Cards;
	float TrayY0 = 0.f, TrayHidden = 0.f; // tray translation: 0 open, TrayHidden collapsed (design units)
	TWeakObjectPtr<UWidget> BannerRoot;
	float BannerShift = 0.f;              // banner translation when the tray is collapsed (design units, negative = up)
};
