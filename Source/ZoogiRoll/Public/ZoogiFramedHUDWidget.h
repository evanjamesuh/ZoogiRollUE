// framed: the UMG layer of the framed match layout (-ZoogiFramedHUD). Paints the deep-blue backdrop in the margins,
// the decorated arena frame + map title (light pulse on major events), the top header (round, orbs, active player,
// DRAW/AWAKEN/BATTLE/RESOLVE/END stepper, status), the two team columns (UZoogiTeamColumnWidget) and the bottom tray
// + prompt bar; routes the existing Awaken / aim UMG panels and the card hand into the tray; nav buttons (camera,
// pause/menu with resume, settings, restart, main menu, exit). Never draws over the arena rect except the pause overlay.
// Everything calls the same AZoogiGameMode::MenuAction paths as the old HUD buttons: no rule changes.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiFramedHUDWidget.generated.h"

class UCanvasPanel;
class UButton;
class UTextBlock;
class UBorder;
class UZoogiTeamColumnWidget;
class AZoogiGameMode;

/** Backdrop / frame / header / tray painter: a child placed first so buttons and columns draw above it. */
UCLASS()
class ZOOGIROLL_API UZoogiFramedPainter : public UUserWidget
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<class UZoogiFramedHUDWidget> Owner;
protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
};

UCLASS()
class ZOOGIROLL_API UZoogiFramedHUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	int32 PaintBack(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer) const;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
	UFUNCTION() void OnCam();
	UFUNCTION() void OnPause();
	UFUNCTION() void OnResume();
	UFUNCTION() void OnRestart();
	UFUNCTION() void OnSettings();
	UFUNCTION() void OnMainMenu();
	UFUNCTION() void OnExit();
	UFUNCTION() void OnVolDown();
	UFUNCTION() void OnVolUp();

private:
	AZoogiGameMode* GM() const;
	UButton* MakeButton(const FString& Label, const FLinearColor& Accent, bool bFilled, UTextBlock*& OutText, int32 FontSize = 16);
	void PlaceW(UWidget* W, const FVector2D& Pos, const FVector2D& Size, bool bShow);
	void SetPaused(bool bPause);
	void RouteTray(AZoogiGameMode* G, float s);
	FString PromptText(const AZoogiGameMode* G) const;
	FString HintText(const AZoogiGameMode* G) const;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UZoogiFramedPainter> Painter;
	UPROPERTY() TObjectPtr<UBorder> PausePanel;
	UPROPERTY() TObjectPtr<UTextBlock> PauseTitle;
	UPROPERTY() TObjectPtr<UZoogiTeamColumnWidget> ColL;
	UPROPERTY() TObjectPtr<UZoogiTeamColumnWidget> ColR;
	UPROPERTY() TObjectPtr<UButton> CamBtn;
	UPROPERTY() TObjectPtr<UTextBlock> CamText;
	UPROPERTY() TObjectPtr<UButton> MenuBtn;
	UPROPERTY() TObjectPtr<UBorder> PauseShade;
	UPROPERTY() TArray<TObjectPtr<UButton>> PauseBtns; // resume, settings, restart, main menu, exit
	UPROPERTY() TObjectPtr<UButton> VolDown;
	UPROPERTY() TObjectPtr<UButton> VolUp;
	UPROPERTY() TObjectPtr<UTextBlock> VolText;
	UPROPERTY() TObjectPtr<UButton> SetCamBtn;
	UPROPERTY() TObjectPtr<UTextBlock> SetCamText;

	bool bPaused = false;
	bool bSettings = false;
	bool bActive = false;
	bool bTrayScaled = false;
	float Time = 0.f;
	float LocalScale = 1.f;   // local units per viewport pixel
	// feedback animations
	int32 LastStage = -1, LastActive = -1, LastPhase = -1;
	float StageAnim = 1.f, TurnAnim = 1.f, EndAnim = 0.f;
	float LastBigAge = 99.f;
	FString LastPrompt;
	float PromptAnim = 1.f;
};
