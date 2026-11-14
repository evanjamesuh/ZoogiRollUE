// cardui: one card face in the drag-to-assign hand (-ZoogiCardHandUI). Display + pointer forwarding only:
// every gameplay decision goes back through UZoogiCardHandWidget -> AZoogiGameMode::MenuAction (the same calls the
// Phase 4 Standby panel uses), so this widget never touches Card Energy or the card rules.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiCardWidget.generated.h"

class UZoogiCardHandWidget;
class UTexture2D;

UCLASS()
class ZOOGIROLL_API UZoogiCardWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	static constexpr float CardW = 150.f;
	static constexpr float CardH = 236.f; // card art aspect 245 x 386

	UPROPERTY() TObjectPtr<UZoogiCardHandWidget> Hand;
	UPROPERTY() TObjectPtr<UTexture2D> Art;
	int32 InstanceId = -1;
	FName CardId;
	FString Name;
	FString Rule;
	int32 Cost = 0;
	FLinearColor Color = FLinearColor::White;
	const TCHAR* Tablet = nullptr; // hud-v3: element tablet (Evan's element_tablets.png) from the card's element
	bool bAffordable = true;   // CanSelect == Ok (dimmed otherwise)
	bool bHighlight = false;   // hovered / dragged

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual void NativeOnMouseEnter(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& Ev) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Ev) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Ev) override;
};
