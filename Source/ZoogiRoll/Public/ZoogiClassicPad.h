#pragma once
// Oct 28 (Evan): "restyle the reaction and power buttons and fix the issue of when pressed doesn't activate when tapping
// slightly off center ... ensure button activate always". REACTION and SUPERPOWER are now real touch widgets on top of the
// game view (not Canvas drawings hit-tested by the controller): they fire on touch-down, anywhere inside a generous box
// around the circle and its label, and the touch never leaks into aiming. Look: MODE SELECTION style round plates (thick dark
// rim, lit top, Lilita One labels). Placement = ZoogiRoyaleUI::Layout (REACTION on the right edge, SUPERPOWER under it).
#include "CoreMinimal.h"

class UWorld;

namespace ZoogiClassicPad
{
	/** Adds the overlay to this world's game viewport once (call every frame from the HUD; cheap). */
	ZOOGIROLL_API void Ensure(UWorld* World);
	/** b120: a pad button (REACTION / SUPERPOWER / WOLF) owns the pointer right now - never a field tap */
	ZOOGIROLL_API bool PointerOwned();
	/** b276: the SUPERPOWER button's centre in viewport pixels (the command menu is centred on it) */
	ZOOGIROLL_API FVector2D PowerCenterPx(const FVector2D& ViewportPx);
	/** b278: the left edge (viewport pixels) of the REACTION starburst, so overlays never cover it */
	ZOOGIROLL_API float ReactLeftPx(const FVector2D& ViewportPx);
}
