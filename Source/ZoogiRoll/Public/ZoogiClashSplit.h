// Nov 2 b68 (Evan: "During the clash imagine two separate anime videos playing, one on each side, with a continuous tear down the
// middle like a clash - the video of our character awakening their inner power; the winner's video takes over more until full screen
// from either side"). Two looping awakening clips (one media player each), split by a jagged, crackling seam that follows the tap
// tug; on the finish the winner's side sweeps across to full screen. Drawn by UZoogiFunSys::DrawClash under the tug UI.
// -ZoogiNoClashSplit = the old energy-wash only clash screen.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class UCanvas;
class AZoogiBall;

namespace ZoogiClashSplit
{
	ZOOGIROLL_API bool Enabled();
	/** Opens both clips (Left / Right = screen sides). */
	ZOOGIROLL_API void Begin(UWorld* W, const AZoogiBall* Left, const AZoogiBall* Right);
	ZOOGIROLL_API void End();
	ZOOGIROLL_API bool On();
	/** Split = the left side's share of the width (0..1), Alpha = overall fade, T = real time (seam motion), colours = the sides' form colours. */
	ZOOGIROLL_API void Draw(UCanvas* C, float Split, float Alpha, float T, const FLinearColor& LCol, const FLinearColor& RCol);
}
