// Frame trail (Evan, Oct 14 5:28 PM): a white light glint racing around the framed battle HUD's arena view frame in a loop.
// Separate Slate overlay (one leaf widget, 3 antialiased line passes per frame: no material, no texture, no render target),
// traced on ZoogiFramed::Current().Arena, so it follows the real view rect on every aspect / safe area. Draws nothing while
// the framed layout is not active. Tuning: [ZoogiRoll.FrameTrail] in DefaultGame.ini.
// HUD hook (owned by Gameplay & UI):  UZoogiFrameTrail::Show(GetWorld());   (and UZoogiFrameTrail::Hide(GetWorld()); to stop)
// Test: -ZoogiFrameTrail shows it on world begin play; with -ZoogiShots=<dir> it also writes frametrail_* captures.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiFrameTrail.generated.h"

class SWidget;

struct FZoogiFrameTrailTune
{
	float LapSeconds = 3.0f;   // one lap of the arena frame
	float TailFrac = 0.18f;    // tail length as a fraction of the perimeter
	float CoreWidth = 3.0f;    // 1080p units
	float GlowWidth = 16.0f;   // soft bloom halo width (1080p units)
	float GlowAlpha = 0.35f;
	float Brightness = 1.0f;   // overall alpha multiplier
	float Inset = 0.0f;        // 1080p units inward from the arena rect edge (negative = outward onto the frame)
	FLinearColor Color = FLinearColor::White;
};

UCLASS()
class ZOOGIROLL_API UZoogiFrameTrail : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static void Show(UWorld* World);
	static void Hide(UWorld* World);
	static FZoogiFrameTrailTune GetTune();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }

private:
	void DoShow();
	void DoHide();
	void ShotTick();
	TSharedPtr<SWidget> Widget;
	FTimerHandle ShotTimer;
	int32 Shots = 0;
};