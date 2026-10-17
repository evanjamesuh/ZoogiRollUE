// clash-menu DETAILED PASS 5: end of a real match -> "Winner!" overlay (winner's name, crowns, OK), then a short logo
// loading screen, then back to Home. Front-end only: created by UZoogiFrontRewardBridge on MatchOver, drawn above the
// battle (the battle HUD itself is untouched). Scales to fit the viewport in either orientation (landscape battle or
// portrait), so it works with the portrait-hud orientation switch.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiWinnerOverlay.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

UCLASS()
class ZOOGIROLL_API UZoogiWinnerOverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Shows the overlay in World's viewport. Outcome: 0 the player won, 1 the opponent won, 2 draw. Scores are the real match points. */
	static UZoogiWinnerOverlay* Show(UWorld* World, int32 Outcome, const FString& WinnerName, int32 MyScore, int32 OppScore);
	/** Capture tour: screenshot path prefix (dir/clash_WxH_NN); empty = no screenshots, wait for a real tap. */
	static FString TourPrefix;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void HandleOk();
	void Build();
	UWidget* At(UWidget* W, float X, float Y, float Wd, float Ht);
	UTextBlock* Txt(const FString& S, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, float Outline = 5.f);
	UImage* Img(UTexture2D* Tex, float X, float Y, float Wd, float Ht, const FLinearColor& Tint = FLinearColor::White);
	UTexture2D* LoadArt(const FString& Name);
	UTexture2D* Crown();

	UPROPERTY() TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY() TObjectPtr<UWidget> WinnerGroup;
	UPROPERTY() TObjectPtr<UWidget> LoadingGroup;
	UPROPERTY() TObjectPtr<UWidget> LoadingBg;
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> Keep;
	TWeakObjectPtr<UTextBlock> PctText;
	TWeakObjectPtr<UWidget> BarFill;

	int32 Outcome = 0;
	FString WinnerName;
	int32 MyScore = 0, OppScore = 0;
	float T = 0.f;          // since shown
	float LoadT = -1.f;     // since OK (loading screen)
	bool bReturned = false;
	int32 TourShots = 0;
};
