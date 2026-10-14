// cardui: segmented card-to-target arrow + target ring painters (Slate draw elements only, no gameplay).
// This is ONLY the card -> Zoogi selection arrow while dragging a hand card. The aim preview stays the existing
// ArrowTrail forecast ribbon (ZoogiCardSubsystem::TickForecast); nothing here replaces or touches it.
#pragma once

#include "CoreMinimal.h"

class FSlateWindowElementList;
struct FGeometry;

namespace ZoogiCardTargetArrow
{
	/** Quadratic arc from Start to End (local space of Geo), drawn as Segments chevron dashes plus an arrow head. */
	ZOOGIROLL_API void Paint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Start, const FVector2D& End,
		const FLinearColor& Color, float Time, int32 Segments = 14);
	/** Circle outline (polyline). */
	ZOOGIROLL_API void Ring(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, const FLinearColor& Color, float Thick);
}
