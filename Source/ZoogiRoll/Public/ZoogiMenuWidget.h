#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ZoogiMenuWidget.generated.h"

class UZoogiMenuWidget;
class UButton;
class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UPanelWidget;
class UTextBlock;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;

/** Menu button that never takes keyboard focus (so SPACE / Esc keep reaching the game). */
UCLASS()
class ZOOGIROLL_API UZoogiButton : public UButton
{
	GENERATED_BODY()
public:
	UZoogiButton() { InitIsFocusable(false); }
};

/** One per menu button: forwards the click to the game mode's MenuAction. */
UCLASS()
class ZOOGIROLL_API UZoogiMenuClick : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UZoogiMenuWidget> Owner;
	FName Action;
	int32 Index = 0;
	UFUNCTION() void Click();
	UFUNCTION() void Hover();
};

/**
 * Main menu built entirely in C++ with UMG (no Blueprint asset needed):
 * title, players / rounds, one card per player (Zoogi, power, stat bars, CPU/HUMAN),
 * arena picker, START MATCH and QUIT. Rebuilds itself whenever AZoogiGameMode::MenuRevision changes.
 */
UCLASS()
class ZOOGIROLL_API UZoogiMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Fire(FName Action, int32 Index);
	/** Called by the HUD every frame while the menu is up: rebuilds after a menu change, pulses the title. */
	void Refresh(float DeltaSeconds);
	void MarkDirty() { BuiltRevision = -1; }
	/** Menu backdrop video (Content/Anime/Movies/Menu_Background.mp4, Evan's main-bg-video): play while the menu is up. */
	void SetBackdropPlaying(bool bPlay);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildContents();
	UButton* MakeButton(UPanelWidget* Parent, const FString& Label, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected, float Width, float Height, int32 FontSize = 22);
	UTextBlock* MakeText(UPanelWidget* Parent, const FString& S, int32 Size, const FLinearColor& C, bool bBold = true);
	void BuildPlayerCard(UHorizontalBox* Row, int32 PlayerIndex);

	UPROPERTY() TObjectPtr<UOverlay> Root;
	UPROPERTY() TObjectPtr<UTextBlock> Title;
	UPROPERTY() TArray<TObjectPtr<UZoogiMenuClick>> Clicks;
	int32 BuiltRevision = -1;
	bool EnsureBackdrop();
	UPROPERTY() TObjectPtr<UMediaPlayer> BackdropPlayer;
	UPROPERTY() TObjectPtr<UMediaTexture> BackdropTexture;
	UPROPERTY() TObjectPtr<UFileMediaSource> BackdropSource;
	int32 BackdropState = 0; // 0 untried, 1 ok, -1 unavailable
	float Time = 0.f;
	bool bAnimeView = false; // WATCH ANIME player (Oct 7)
	FString AnimeClip;
};
