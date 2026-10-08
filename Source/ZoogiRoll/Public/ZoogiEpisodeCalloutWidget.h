#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiEpisodeCalloutWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;

/**
 * Minimal fallback text overlay of the Episode core: the tier 1-2 callout pop and the confirmed-result epilogue banner.
 * Used ONLY while nobody is bound to UZoogiEpisodeSubsystem::OnCallout / OnEpilogue (the real presentation pieces,
 * panels / camera / power visuals, belong to their owners and plug into the subsystem's cue delegates).
 * Own widget; the Canvas HUD is untouched. Driven every real frame (works while paused).
 */
UCLASS()
class ZOOGIROLL_API UZoogiEpisodeCalloutWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ShowCallout(const FString& Main, const FString& Sub, const FLinearColor& Color, int32 Tier);
	void ShowBanner(const FString& Text, const FString& Sub, const FLinearColor& Color, float Seconds);
	void TickEpisode(float RealDt);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UTextBlock> CalloutText;
	UPROPERTY() TObjectPtr<UTextBlock> CalloutSub;
	UPROPERTY() TObjectPtr<UBorder> BannerBox;
	UPROPERTY() TObjectPtr<UTextBlock> BannerText;
	UPROPERTY() TObjectPtr<UTextBlock> BannerSub;

	float CalloutAge = 99.f;
	float CalloutLife = 1.2f;
	int32 CalloutTier = 1;
	FLinearColor CalloutColor = FLinearColor::White;
	float BannerAge = 99.f;
	float BannerLife = 1.4f;
};
