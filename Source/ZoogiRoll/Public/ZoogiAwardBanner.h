// b238 (Evan Nov 11: "The award banner 1"): the hanging stadium banner. At the end of a round a purple banner with gold trim drops
// from the top of the screen, sways, and names the round's star: KNOCKOUT KING (most knockouts that round) or ORB HUNTER (most
// points that round) with the Zoogi's portrait, name and the stat. -ZoogiNoAwardBanner = off, -ZoogiAwardBannerTest = every round.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class UCanvas;

namespace ZoogiAwardBanner
{
	/** called when a round ends and another follows */
	ZOOGIROLL_API void OnRoundEnd(AZoogiGameMode* G);
	ZOOGIROLL_API bool Showing(); // b239
	/** drawn by the fun HUD (Text = centred chunky text: string, x, y, px, colour) */
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, TFunctionRef<void(const FString&, float, float, float, const FLinearColor&)> Text);
}
