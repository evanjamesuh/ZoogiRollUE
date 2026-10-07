#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiEpisodeWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UImage;
class UTextBlock;
class UTextureRenderTarget2D;

/**
 * Episode Engine overlay (own widget; the Canvas HUD is untouched). Pure presentation, driven every real frame by
 * UZoogiEpisodeSubsystem (works while the game is paused for a presentation freeze).
 *  - callout pop (tier 1) / trick-shot callout (tier 2)
 *  - center-frame panel compositor (tier 3+): 2-4 live SceneCapture panels slide / rotate in around the live centre
 *    view and collapse out; the centre of the screen stays the real, live game view
 *  - power escalation (tier 4): power-colour frame + title
 *  - flash / darken (Titan Meteor), result banner (only after physics confirmed it)
 */
UCLASS()
class ZOOGIROLL_API UZoogiEpisodeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ShowCallout(const FString& Main, const FString& Sub, const FLinearColor& Color, int32 Tier);
	void ShowPanels(EZoogiPanelLayout Layout, const TArray<UTextureRenderTarget2D*>& Targets, const FLinearColor& Frame, float Seconds);
	void CollapsePanels();
	void SetPower(bool bOn, const FLinearColor& Color, const FString& Title);
	void Flash(float Strength, const FLinearColor& Color);
	void SetDarken(float Alpha);
	void ShowBanner(const FString& Text, const FString& Sub, const FLinearColor& Color, float Seconds);
	void TickEpisode(float RealDt);
	bool PanelsVisible() const { return PanelState != 0; }
	bool PanelsFullyIn() const { return PanelState == 2; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UImage> DarkImg;
	UPROPERTY() TObjectPtr<UImage> TintImg;
	UPROPERTY() TArray<TObjectPtr<UImage>> EdgeBars;
	UPROPERTY() TArray<TObjectPtr<UBorder>> PanelFrames;
	UPROPERTY() TArray<TObjectPtr<UImage>> PanelImages;
	UPROPERTY() TObjectPtr<UTextBlock> CalloutText;
	UPROPERTY() TObjectPtr<UTextBlock> CalloutSub;
	UPROPERTY() TObjectPtr<UTextBlock> PowerTitle;
	UPROPERTY() TObjectPtr<UBorder> BannerBox;
	UPROPERTY() TObjectPtr<UTextBlock> BannerText;
	UPROPERTY() TObjectPtr<UTextBlock> BannerSub;
	UPROPERTY() TObjectPtr<UImage> FlashImg;

	// animation state (real seconds)
	float CalloutAge = 99.f;
	float CalloutLife = 1.2f;
	int32 CalloutTier = 1;
	FLinearColor CalloutColor = FLinearColor::White;
	int32 PanelState = 0;       // 0 hidden, 1 sliding in, 2 in, 3 collapsing
	float PanelAge = 0.f;
	float PanelHold = 1.9f;
	int32 PanelCount = 0;
	EZoogiPanelLayout Layout = EZoogiPanelLayout::None;
	TArray<FVector2D> PanelSlot;   // anchor (0..1)
	TArray<FVector2D> PanelFrom;   // slide-in offset (slate units)
	TArray<float> PanelAngle;      // entry rotation
	bool bPower = false;
	float PowerAge = 0.f;
	FLinearColor PowerColor = FLinearColor::White;
	float FlashA = 0.f;
	FLinearColor FlashColor = FLinearColor::White;
	float DarkA = 0.f;
	float BannerAge = 99.f;
	float BannerLife = 1.4f;
};
