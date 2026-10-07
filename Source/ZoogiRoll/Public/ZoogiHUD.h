#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ZoogiHUD.generated.h"

class AZoogiGameMode;
class UFont;
class UZoogiMenuWidget;
class UZoogiAwakenWidget;

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
	/** UMG Awaken controls (null with -ZoogiCanvasAwaken or before the first match frame). */
	UZoogiAwakenWidget* GetAwakenWidget() const { return AwakenWidget; }
	/** A UMG control / panel took a mouse press this frame (or the last): the controller must not world-pick it. */
	bool UIConsumedClick() const;
	/** Viewport pixel position over a visible UMG panel (no hover / pick through it). */
	bool IsOverUI(const FVector2D& ViewportPos) const;

protected:
	void DrawForming(class AZoogiGameMode* G);
	void DrawMenu(AZoogiGameMode* G);
	void DrawMatch(AZoogiGameMode* G);
	void DrawScoreboard(AZoogiGameMode* G);
	void DrawAim(AZoogiGameMode* G);
	void DrawPopups(AZoogiGameMode* G);
	void DrawFlyers(AZoogiGameMode* G);
	void DrawPower(AZoogiGameMode* G);
	void DrawResults(AZoogiGameMode* G);
	void DrawStages(AZoogiGameMode* G);
	// Milestone "Awaken" (Canvas, like the rest of the in-match HUD)
	void DrawAwaken(AZoogiGameMode* G);
	void DrawAimControls(AZoogiGameMode* G);
	void DrawPreview(AZoogiGameMode* G);
	void DrawArtifacts(AZoogiGameMode* G);
	void DrawAnime(AZoogiGameMode* G);
	void DrawResolve(AZoogiGameMode* G);
	/** 2v2 Combo: team gauges, Resonance / Arena Control, floor state, callouts, Partner Follow-Up prompt. */
	void DrawCombo(AZoogiGameMode* G);
	void RingMarks(int32 Marks, float X, float Y, float S, bool bKO);
	/** Procedural gem icon per artifact (placeholder until Aura's icons land). */
	void Gem(uint8 Artifact, float CX, float CY, float R, float Alpha = 1.f);
	void WorldCircle(const FVector& C, float Radius, const FLinearColor& Col, float Thick);
	void WorldArrow(const FVector& From, const FVector& To, const FLinearColor& Col, float Thick);
	/** Aim arrows drawn as glowing golden swords (pommel, wrapped grip, swept crossguard, bevelled blade with a fuller,
	 *  travelling shimmer, sparkle at the point). Scale ~1 for the main arrow; Seed de-syncs the shimmer. */
	void SwordArrow(const FVector& From, const FVector& To, float Scale, float Alpha, float Seed);
	/** -ZoogiClipFrames -ZoogiShots=<dir>: dump Awaken panel frames for a short review clip. */
	void TickClipFrames(AZoogiGameMode* G);
	/** -ZoogiUIPolish: hold human Awaken for stone spin shot/clip, Infuse, then sword-aim shot. */
	void TickUIPolish(AZoogiGameMode* G);
	int32 ClipAwakenFrames = 0;
	float ClipAwakenT = -1.f;
	float PolishAwakenT = 0.f;
	int32 PolishStep = 0;

	void Panel(float X, float Y, float W, float H, const FLinearColor& C);
	void Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale, bool bCenter = false, bool bBig = true);
	bool Button(const FString& Label, float X, float Y, float W, float H, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected = false);
	FVector2D TextSize(const FString& S, float Scale, bool bBig = true);

	TArray<FZoogiButton> Buttons;
	float U = 1.f; // UI scale (1 at 1080p)
	float Time = 0.f;
	float Letterbox = 0.f; // Battle cinematic bars (0..1)

	/** UMG main menu (the canvas menu is kept as a fallback behind -ZoogiCanvasMenu). */
	UPROPERTY() TObjectPtr<UZoogiMenuWidget> MenuWidget;
	int32 CanvasMenuMode = -1;
	/** UMG Awaken panel + aim bar (Canvas versions kept behind -ZoogiCanvasAwaken). */
	UPROPERTY() TObjectPtr<UZoogiAwakenWidget> AwakenWidget;
	int32 CanvasAwakenMode = -1;
	void UpdateAwakenWidget(AZoogiGameMode* G, bool bHide);
};
