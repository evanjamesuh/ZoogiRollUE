#pragma once
// Oct 30 (Evan, "Persona 5 Royal-style animations and videos" brief, feat/p5-style): Zoogi Roll's own take on that look -
// deep black, pure white and ONE loud accent (Zoogi Roll hot magenta), element colours in each Zoogi's moments; tilted, layered
// collage panels, jagged bursts, halftone dots; ransom-note lettering that slams in; everything moves, nothing fades.
// Style only - no Persona characters, logos, fonts, art, music or screenshots. Fonts are SIL OFL (Content/Data/Fonts):
// Anton, Archivo Black, Abril Fatface, Bangers.
//  * UZoogiP5MenuWidget: the MODE SELECT / MAP SELECT screen (UZoogiMenuRoot::BuildP5Menu, ZoogiModeSelect.cpp).
//  * ZoogiP5::PlayWipe: full-screen angled-shard transition (~0.6 s) over the game viewport.
//  * UZoogiP5ResultsWidget: the RESULTS screen (brief step 3; UZoogiArenaHUDWidget::BuildResults). -ZoogiP5ResultsTest = capture run.
//  * ZoogiP5::PlayCutIn: character cut-in for a critical (Blue first; UZoogiAnimeSubsystem::Start).
//  -ZoogiClassicMenu = the previous card-row menu.  -ZoogiP5MenuTest = scripted capture run (frames for review).
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "ZoogiP5Style.generated.h"

class UTexture2D;
class UWorld;

namespace ZoogiP5
{
	ZOOGIROLL_API bool MenuOn();
	/** Zoogi Roll's signature accent (hot magenta) and the element palette (Void / Fire / Shock / Frost / Neutral). */
	ZOOGIROLL_API FLinearColor Accent();
	ZOOGIROLL_API FLinearColor ElementColor(const FString& Element);
	/** 0 Anton, 1 Archivo Black, 2 Abril Fatface, 3 Bangers (falls back to the engine bold font if a file is missing). */
	ZOOGIROLL_API FSlateFontInfo Font(int32 Which, float Size, float Outline = 0.f);
	/** Full-screen angled-shard wipe over the game viewport; OnCovered runs once the screen is fully covered (about 0.28 s in). */
	ZOOGIROLL_API void PlayWipe(UWorld* World, TFunction<void()> OnCovered, const FLinearColor& Tint, const FString& Shout = TEXT("LET'S ROLL!"));
	ZOOGIROLL_API bool WipeBusy();

	/** Oct 30 (brief step 2): P5-style character cut-in for a critical - a slanted panel slams in over the scene with a close-up of
	 *  the Zoogi's eyes on its element colour, the name tag and ransom impact text, then snaps away (~1.1 s; repeats 0.6 s). */
	struct FCutIn
	{
		FString Name;                                   // "BLUE"
		FString Shout = TEXT("CRITICAL!");
		TWeakObjectPtr<UTexture2D> Face;                // the close-up source (menu portrait, or the ultimate face when transformed)
		FVector2D UV0 = FVector2D(0.17f, 0.53f);        // the eyes inside Face (the new head art, Oct 30)
		FVector2D UV1 = FVector2D(0.83f, 0.75f);
		FLinearColor Col = FLinearColor::White;         // element colour
		TFunction<void(const FString&)> Shot;           // -ZoogiP5CritTest captures
	};
	/** Which Zoogis use the P5 cut-in so far: Blue (Evan reviews it before the rollout); -ZoogiP5CutInAll = everyone. */
	ZOOGIROLL_API bool CutInFor(const FString& ZoogiName);
	ZOOGIROLL_API FLinearColor ZoogiElementColor(const FString& ZoogiName);
	ZOOGIROLL_API void PlayCutIn(UWorld* World, const FCutIn& C, float Length);
	ZOOGIROLL_API void StopCutIn();
	ZOOGIROLL_API bool CutInBusy();
	/** Content/Data/MenuArt/<name>.png|.jpg as a texture (cached). */
	ZOOGIROLL_API UTexture2D* Portrait(const FString& ZoogiName);

	/** Oct 30 (brief step 3): the P5-style RESULTS screen (UZoogiArenaHUDWidget::BuildResults -> UZoogiP5ResultsWidget). */
	struct FResultRow
	{
		FString Name, Line;
		int32 Score = 0, Player = 0;
		bool bWin = false, bYou = false;
		FLinearColor Col = FLinearColor::White;      // team colour (face chip)
		TWeakObjectPtr<UTexture2D> Face;
	};
	struct FResultStat { FString Label; int32 Value = 0; };
	struct FResultButton { FString Label, Shout; int32 Style = 0; TFunction<void()> Fn; }; // Style 0 magenta, 1 white, 2 black
	struct FResultsSpec
	{
		FString Head, Sub, Reason;                   // "VICTORY!" / "BLUE WINS!" / combo reason
		int32 Outcome = 0;                           // the local player's view: 0 won, 1 lost, 2 draw
		FLinearColor WinCol = FLinearColor::White;   // the winner's element colour
		FString HeroName, HeroTag;
		TWeakObjectPtr<UTexture2D> Hero;
		bool bHeroCutout = false;                    // transparent ultimate face (no frame) vs. a square portrait (torn frame)
		TArray<FResultRow> Rows;                     // in place order
		TArray<FResultStat> Stats;                   // the local player's numbers
		TArray<FResultButton> Buttons;
		TWeakObjectPtr<UObject> Ctx;
		TFunction<void(const FString&)> Shot;        // -ZoogiP5ResultsTest captures
	};
	/** true while the results screen is on screen (post-render canvases stay off so nothing draws over it) */
	ZOOGIROLL_API bool ResultsOn();

	struct FItem
	{
		FString Name;
		FString Sub;
		TWeakObjectPtr<UTexture2D> Art;
		TWeakObjectPtr<UTexture2D> ArtFull; // b120: portrait full-screen picture (map select)
		FLinearColor Col = FLinearColor::White;
		TFunction<void()> Fn;
	};
	struct FMenuSpec
	{
		FString TitleA, TitleB;      // ransom-note title, two words
		TArray<FItem> Items;
		TFunction<void()> Back;
		int32 Gems = 0, Coins = 0;
		int32 StartSel = 0;
		bool bMapList = false;
		TWeakObjectPtr<UObject> Ctx;                 // world context for sounds / the wipe
		TFunction<void(const FString&)> Shot;        // -ZoogiP5MenuTest captures
	};
}

UCLASS()
class ZOOGIROLL_API UZoogiP5MenuWidget : public UWidget
{
	GENERATED_BODY()
public:
	ZoogiP5::FMenuSpec Spec;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Keep;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	TSharedPtr<SWidget> MySlate;
};

UCLASS()
class ZOOGIROLL_API UZoogiP5ResultsWidget : public UWidget
{
	GENERATED_BODY()
public:
	ZoogiP5::FResultsSpec Spec;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Keep;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	TSharedPtr<SWidget> MySlate;
};
