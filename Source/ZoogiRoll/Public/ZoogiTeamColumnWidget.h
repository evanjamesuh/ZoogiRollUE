// framed: one side column of the framed HUD (left = friendly / team A / P1 side in cyan, right = opponents in coral).
// Per Zoogi: portrait, name, HUMAN/CPU tag, score, orbs / KOs / hand, status effects (KO, OUT, STUN, BOUND, GUARD,
// artifact status), power indicator (infused artifact / committed cards), ring marks (2v2 lives), Episode Energy bar
// with juice + READY state, active highlight. Team Combo gauge when 2v2 Combo is on. READ-ONLY: never writes game state.
// There is no HP stat in Zoogi Roll, so no health bar is drawn (ring marks are the only "lives" stat and only exist in 2v2).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZoogiUIStyle.h"
#include "ZoogiTeamColumnWidget.generated.h"

class AZoogiGameMode;
class AZoogiBall;
class UTexture2D;

UCLASS()
class ZOOGIROLL_API UZoogiTeamColumnWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	int32 Side = 0;          // 0 left, 1 right
	float UScale = 1.f;      // local units per 1080p unit
	bool bCompact = false;
	/** Which balls sit in this column: team in 2v2, else even / odd player index. */
	static int32 SideOf(const AZoogiGameMode* G, const AZoogiBall* B);
	static FLinearColor SideColor(int32 Side);

protected:
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	const FSlateBrush* Portrait(FName Id) const;
	UPROPERTY() mutable TMap<FName, TObjectPtr<UTexture2D>> PortraitTex;
	mutable TMap<FName, FSlateBrush> PortraitBrush;
	TMap<int32, ZoogiUIStyle::FBarJuice> EnergyJuice;
	ZoogiUIStyle::FBarJuice ComboJuice;
	TMap<int32, int32> LastScore;
	TMap<int32, float> ScorePop;
	int32 LastActive = -1;
	float TurnAnim = 1.f;
	float Time = 0.f;
};
