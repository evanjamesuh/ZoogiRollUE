// Awaken anime cut-in as FOUR comic panels (Evan, Oct 14 6:28 AM): the Infuse clip is not drawn as a big centre card any more;
// it plays in four bordered panels along the left / right screen edges (landscape mobile safe area), revealed in sequence,
// each showing a different crop of the clip, so the centre of the screen and the Zoogi's real in-world transformation stay
// visible. Pure Slate overlay on the game viewport (not ZoogiHUD); owned by UZoogiAnimeSubsystem.
#pragma once

#include "CoreMinimal.h"

class UGameViewportClient;
class UTexture;
class SWidget;

class ZOOGIROLL_API FZoogiAnimePanels
{
public:
	~FZoogiAnimePanels() { Hide(); }
	void Show(UGameViewportClient* Viewport, UTexture* Movie, const FLinearColor& Accent, float Length);
	void Hide();
	void SetAge(float Age);
	bool IsShown() const { return Root.IsValid(); }

	/** Normalized screen rects (x0, y0, x1, y1) of the four panels; the band x 0.25..0.75 stays clear. */
	static FBox2f PanelRect(int32 Index);

private:
	struct FState
	{
		float Age = 0.f;
		float Length = 2.f;
		FLinearColor Accent = FLinearColor::White;
		TArray<TSharedPtr<struct FSlateBrush>> Brushes;
	};
	TSharedPtr<FState> State;
	TSharedPtr<SWidget> Root;
	TWeakObjectPtr<UGameViewportClient> VC;
};
