#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ZoogiHUD.generated.h"

class AZoogiGameMode;
class UFont;
class UZoogiMenuWidget;

struct FZoogiButton
{
	FBox2D Rect;
	FName Action;
	int32 Index = 0;
};

/** Canvas HUD: menu, scoreboard, turn banner, slingshot trajectory + power meter, score popups, results. */
UCLASS()
class ZOOGIROLL_API AZoogiHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	bool HitTest(const FVector2D& Mouse, FName& OutAction, int32& OutIndex) const;

protected:
	void DrawMenu(AZoogiGameMode* G);
	void DrawMatch(AZoogiGameMode* G);
	void DrawScoreboard(AZoogiGameMode* G);
	void DrawAim(AZoogiGameMode* G);
	void DrawPopups(AZoogiGameMode* G);
	void DrawPower(AZoogiGameMode* G);
	void DrawResults(AZoogiGameMode* G);

	void Panel(float X, float Y, float W, float H, const FLinearColor& C);
	void Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter = false, bool bBig = true);
	bool Button(const FString& Label, float X, float Y, float W, float H, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected = false);
	FVector2D TextSize(const FString& S, float Scale, bool bBig = true);

	TArray<FZoogiButton> Buttons;
	float U = 1.f; // UI scale (1 at 1080p)
	float Time = 0.f;

	/** UMG main menu (the canvas menu is kept as a fallback behind -ZoogiCanvasMenu). */
	UPROPERTY() TObjectPtr<UZoogiMenuWidget> MenuWidget;
	int32 CanvasMenuMode = -1;
};
