// Oct 31 b41 (Evan: "use the default apple mobile controls"): Apple's own on-screen game controller (GameController framework
// GCVirtualController, iOS 15+) for Battle Royale. The engine's iOS input layer sees it as a normal extended gamepad, so the
// Royale gamepad mapping drives it: left stick move, right stick look, A jump, B attack (fire / relic), X transform, Y awaken.
// Elsewhere (desktop, Android, iOS < 15, -ZoogiNoApplePad) Active() stays false and the drawn Royale stick + buttons are used.
#pragma once
#include "CoreMinimal.h"

namespace ZoogiApplePad
{
	/** Call every frame: true while Royale wants the Apple controller on screen (fight running, human playing). */
	ZOOGIROLL_API void Want(bool bOn);
	/** The Apple controller is on screen (our own stick / JUMP / TRANSFORM art and hit zones step aside). */
	ZOOGIROLL_API bool Active();
	/** Nov 11 voyager port: the full Apple controller (both sticks, A B X Y, shoulders, triggers) for the Voyager SHOOTER levels -
	 *  Voyager's own gamepad mapping drives it (left stick move, right stick look, LT aim, RT fire ...). */
	ZOOGIROLL_API void WantFull(bool bOn);
}
