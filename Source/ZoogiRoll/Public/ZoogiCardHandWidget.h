// cardui: drag-to-assign card hand for the cards-mode Standby (opt-in: -ZoogiCardHandUI, needs cards mode on).
// Bottom-center fanned hand; hover = 1.3x, lift, straighten, neighbors spread, tooltip; drag = the card stays in the hand,
// a segmented arrow follows the cursor and the acting Zoogi glows in the card color with an effect preview; release on
// it = AZoogiGameMode::MenuAction("CardToggle", slot) (the same select call the Phase 4 Standby panel uses: the card
// system validates, nothing is spent at select); CONFIRM = MenuAction("CardCommit") (validates again, then spends once);
// CLEAR = MenuAction("CardClear"). Draw / discard / assign are animations of the card system's own piles.
// With the flag on, the Phase 4 Canvas card panel is hidden (UZoogiAwakenWidget asks IsEnabled); keys 1-5 / A / Enter /
// Backspace keep working. The ArrowTrail forecast ribbon stays the aim preview; the drag arrow is only card -> target.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiUIStyle.h"
#include "ZoogiCardHandWidget.generated.h"

class AZoogiGameMode;
class UZoogiCardWidget;
class UCanvasPanel;
class UButton;
class UTextBlock;
class UTexture2D;
struct FZoogiCardPlayerState;

UCLASS()
class ZOOGIROLL_API UZoogiCardHandWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** True when -ZoogiCardHandUI is on the command line and the card system is active this match. */
	static bool IsEnabled(const AZoogiGameMode* G);
	/** GameMode hook: creates the widget on the first local player controller (retries until one exists). */
	static void Spawn(AZoogiGameMode* G);

	// card widget -> hand
	void CardHover(UZoogiCardWidget* C, bool bOn);
	bool CardPress(UZoogiCardWidget* C, const FPointerEvent& Ev);
	void CardDragMove(UZoogiCardWidget* C, const FPointerEvent& Ev);
	void CardRelease(UZoogiCardWidget* C, const FPointerEvent& Ev, bool bCancel);
	void CancelDrag(const TCHAR* Why);
	bool IsDragging(const UZoogiCardWidget* C) const { return C && Dragging.Get() == C; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	enum class EVis : uint8 { Hand, ToTarget, Assigned, ToDiscard };
	struct FVis
	{
		TWeakObjectPtr<UZoogiCardWidget> W;
		EVis State = EVis::Hand;
		FVector2D Pos = FVector2D::ZeroVector;
		float Angle = 0.f, Scale = 1.f, Opacity = 1.f;
		// active flight (draw / to target / to discard / back to hand)
		bool bFly = false;
		float T = 0.f, Delay = 0.f, Dur = 0.3f;
		FVector2D From = FVector2D::ZeroVector, To = FVector2D::ZeroVector;
		float FromAngle = 0.f, FromScale = 1.f, ToScale = 1.f, ToAngle = 0.f, Lift = 120.f;
		bool bToLayout = false; // flight ends at the live hand slot
	};
	struct FReact { FVector2D Pos; float T = 0.f; FLinearColor Color; FString Label; };

	UFUNCTION() void OnConfirm();
	UFUNCTION() void OnClear();
	UZoogiCardWidget* MakeCard(int32 InstanceId, FName CardId);
	UTexture2D* ArtFor(FName CardId);
	void Fly(FVis& V, const FVector2D& To, float Dur, float Delay, float ToScale, float ToAngle, float Lift, bool bToLayout);
	void DiscardAll(const FZoogiCardPlayerState* S);
	void UpdateTarget(AZoogiGameMode* G);
	bool OverTarget() const;
	FVector2D Local(const FPointerEvent& Ev) const;
	static FLinearColor TypeColor(uint8 Type);

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UButton> ConfirmBtn;
	UPROPERTY() TObjectPtr<UButton> ClearBtn;
	UPROPERTY() TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY() TArray<TObjectPtr<UZoogiCardWidget>> AllCards;
	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> ArtCache;
	TSet<FName> ArtTried;

	TMap<int32, FVis> Vis;        // by card instance id
	TArray<int32> HandOrder;      // instance ids in hand-slot order (Hand state only)
	int32 KeyPlayer = -1, KeyTurn = -1;
	bool bShown = false, bInteractive = false, bShownPlayedLog = false;
	TWeakObjectPtr<UZoogiCardWidget> Hovered, Dragging;
	FVector2D Cursor = FVector2D::ZeroVector;
	FGeometry CachedGeo;
	FVector2D ViewSize = FVector2D(1920.f, 1080.f);
	float Time = 0.f;
	// framed HUD (v2): the fan sits fully inside the bottom tray, bigger; tooltip goes in the tray beside the fan
	float FramedFanL = -1.f, FramedTrayTop = 0.f;
	TArray<FReact> Reacts;

	// display copies (refreshed each tick, painted in NativePaint)
	int32 EnergyShown = 0, EnergyMax = 5, DrawCount = 0, DiscardCount = 0, LastLoggedEnergy = -999;
	bool bEnergyVisible = false;
	ZoogiUIStyle::FBarJuice EnergyJuice; // uistyle: Card Energy counter juice (display only)
	FVector2D TargetPos = FVector2D::ZeroVector;
	float TargetR = 60.f;
	bool bTargetOnScreen = false;
	FString TargetName, DragPreview, HoverTip, HoverTitle;
	bool bDragValid = false;
	FLinearColor DragColor = FLinearColor::White;
};
