#pragma once
// Oct 28 (Evan): "For classic mode allow vertical mobile phone play" + "Menu in portrait should convert to fit portrait".
// The iPhone app is landscape-only in its settings; at runtime the menus and classic matches also allow portrait (the phone's
// own rotation picks), Battle Royale and 2v2 stay landscape. Applied only when the wanted set changes.
#include "CoreMinimal.h"

namespace ZoogiOrientation
{
	/** true: portrait + both landscapes (no upside down); false: landscape only. Why goes to the log. */
	ZOOGIROLL_API void Allow(bool bPortraitToo, const TCHAR* Why);
}
