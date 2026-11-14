// b138 (Evan 09:49: "for the 15 cards do what you did for the new effects - I love the images you chose"): the new skills' art is the
// real effect shown on the court in a close hero shot. This test-only studio does the same for the 15 old cards: -ZoogiCardArtShoot (solo
// training match) poses your Zoogi ball, plays each card's chosen effects around it, holds a close camera and takes two clean shots per
// card (cardart_<id>_a / _b, use -ZoogiNoHUD). Claude crops them into Data/CardArt/<id>.png + Icons/<id>.png.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
namespace ZoogiCardArtStudio
{
	ZOOGIROLL_API void OnMatchStart(AZoogiGameMode* G);
}
