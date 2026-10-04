#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "ZoogiAwakenWidget.generated.h"

class UZoogiAwakenWidget;
class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;
class AZoogiGameMode;

/** One per Awaken / aim control: forwards the click to the game mode's MenuAction. */
UCLASS()
class ZOOGIROLL_API UZoogiAwakenClick : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UZoogiAwakenWidget> Owner;
	FName Action;
	int32 Index = 0;
	UFUNCTION() void Click();
	UFUNCTION() void Press();
};

/**
 * In-match Awaken controls as real UMG (replaces the Canvas versions of these controls only):
 *  - the Awaken panel: drawn card + saved hand cards (click = pick), INFUSE / GUARD / SAVE, texts, guards line, hints;
 *  - the aim bar: << < > >>, AUTO / SOFT / MED / FULL, NEXT, CONFIRM, CANCEL and the live info line.
 * Built in C++ (no Blueprint asset). Panels swallow mouse presses so a click on a panel never reaches world picking;
 * keyboard shortcuts are untouched (buttons never take focus). -ZoogiCanvasAwaken falls back to the Canvas versions.
 */
UCLASS()
class ZOOGIROLL_API UZoogiAwakenWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Called by the HUD every frame during a match: shows / hides / rebuilds the panels. */
	void Refresh(float DeltaSeconds, bool bHideAll);
	bool ShowsAwaken() const { return bAwakenShown; }
	bool ShowsAim() const { return bAimShown; }

	void Fire(FName Action, int32 Index);
	void NotePress();
	/** True for the frame a UMG control / panel took a mouse press (and the next one), so the controller skips world picking. */
	bool ConsumedClickRecently() const;
	/** Viewport pixel position over a visible UMG panel? */
	bool IsOverPanels(const FVector2D& ViewportPos) const;

	// -ZoogiUiTest helpers: real Slate mouse events (move / down / up) synthesized at a control's centre.
	bool FindControl(FName Action, int32 Index, FVector2D& OutAbs) const;
	bool PanelPoint(bool bAimBar, const FVector2D& Frac, FVector2D& OutAbs) const;
	bool SynthClick(const FVector2D& Abs);
	/** Queue a click at Abs: performed (move + press + release in one go) on a later frame once Slate's hit test
	 *  finds Expect there (waits up to ~30 frames, e.g. while a panel is being rebuilt or the window resizes). */
	void QueueClick(const FVector2D& Abs, const FString& Expect);
	void TickQueuedClick();
	/** Same for a registered control; its position is looked up again every frame. */
	void QueueControlClick(FName Action, int32 Index);
	bool IsClickQueued() const { return QueuedStage > 0; }
	/** Which registered control (if any) Slate's hit test finds at Abs, for the test log. */
	FString ControlUnder(const FVector2D& Abs) const;
	bool SynthHover(const FVector2D& Abs);
	bool AbsToViewport(const FVector2D& Abs, FVector2D& OutViewport) const;
	int32 NumRebuilds = 0;
	/** Last control whose OnClicked fired (test log / checks). */
	FName LastFiredAction = NAME_None;
	int32 LastFiredIndex = -1;
	uint64 LastFiredFrame = 0;
	/** Frame the last synthesized click was performed (0 = none yet). */
	uint64 LastSynthFrame = 0;
	bool FiredSince(uint64 Frame, FName Action, int32 Index) const { return LastFiredFrame >= Frame && LastFiredAction == Action && LastFiredIndex == Index; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void BuildAwaken(AZoogiGameMode* G);
	void BuildAim(AZoogiGameMode* G);
	UWidget* MakeButton(UCanvasPanel* Parent, const FString& Label, FVector2D Pos, FVector2D Size, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected, bool bEnabled = true);
	UTextBlock* MakeText(const FString& S, float Scale, const FLinearColor& C, bool bBig);
	UTextBlock* PlaceText(UCanvasPanel* P, const FString& S, FVector2D Pos, float Scale, const FLinearColor& C, bool bBig, float AlignX = 0.f);
	UBorder* MakePanel(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineW = 0.f);
	UTexture2D* GemTexture(uint8 Artifact);
	void RegisterControl(FName Action, int32 Index, UWidget* W);
	bool bBuildingAim = false;
	FString AwakenSignature(AZoogiGameMode* G) const;
	FString AimSignature(AZoogiGameMode* G) const;

	UFUNCTION() FEventReply OnPanelMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	struct FControl { FName Action; int32 Index = 0; bool bAim = false; TWeakObjectPtr<UWidget> Widget; };
	TArray<FControl> Controls;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UBorder> AwakenPanel;
	UPROPERTY() TObjectPtr<UBorder> AimPanel;
	UPROPERTY() TObjectPtr<UTextBlock> AimInfo;
	UPROPERTY() TObjectPtr<UImage> SelGem;
	UPROPERTY() TArray<TObjectPtr<UZoogiAwakenClick>> AwakenClicks;
	UPROPERTY() TArray<TObjectPtr<UZoogiAwakenClick>> AimClicks;
	UPROPERTY() TMap<uint8, TObjectPtr<UTexture2D>> Gems;

	FString AwakenSig, AimSig;
	bool bAwakenShown = false, bAimShown = false;
	uint64 PressFrame = 0;
	FVector2D QueuedAbs = FVector2D::ZeroVector;
	int32 QueuedStage = 0;
	uint64 QueuedFrame = 0;
	FString QueuedExpect;
	FName QueuedAction = NAME_None;
	int32 QueuedIndex = 0;
	float Time = 0.f;
};
