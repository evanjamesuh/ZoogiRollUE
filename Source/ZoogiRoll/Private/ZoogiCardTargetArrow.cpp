// cardui: see ZoogiCardTargetArrow.h
#include "ZoogiCardTargetArrow.h"
#include "Rendering/DrawElements.h"
#include "Layout/Geometry.h"

namespace ZoogiCardTargetArrow
{
	static FVector2D Bez(const FVector2D& A, const FVector2D& C, const FVector2D& B, float T)
	{
		const float U = 1.f - T;
		return A * (U * U) + C * (2.f * U * T) + B * (T * T);
	}

	void Paint(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Start, const FVector2D& End, const FLinearColor& Color, float Time, int32 Segments)
	{
		const FVector2D Mid = (Start + End) * 0.5f;
		const float Len = FVector2D::Distance(Start, End);
		const FVector2D Ctrl = Mid + FVector2D(0.f, -FMath::Clamp(Len * 0.45f, 40.f, 260.f));
		Segments = FMath::Max(Segments, 4);
		const float Crawl = FMath::Fmod(Time * 1.6f, 1.f); // dashes crawl toward the target
		for (int32 i = 0; i < Segments; ++i)
		{
			const float T0 = FMath::Clamp((i + Crawl) / (Segments + 1.f), 0.f, 1.f);
			const float T1 = FMath::Clamp((i + Crawl + 0.55f) / (Segments + 1.f), 0.f, 1.f);
			const FVector2D P0 = Bez(Start, Ctrl, End, T0), P1 = Bez(Start, Ctrl, End, T1);
			const float Th = FMath::Lerp(5.f, 11.f, T0);
			FLinearColor C = Color; C.A *= FMath::Lerp(0.55f, 1.f, T0);
			TArray<FVector2f> Pts = { FVector2f(P0), FVector2f(P1) };
			FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, C, true, Th);
		}
		// head: two strokes along the tangent at the end
		FVector2D Dir = (End - Bez(Start, Ctrl, End, 0.93f)).GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = FVector2D(0.f, -1.f);
		const FVector2D N(-Dir.Y, Dir.X);
		const FVector2D A = End - Dir * 26.f + N * 16.f, B = End - Dir * 26.f - N * 16.f;
		TArray<FVector2f> H = { FVector2f(A), FVector2f(End), FVector2f(B) };
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), H, ESlateDrawEffect::None, Color, true, 9.f);
	}

	void Ring(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& C, float R, const FLinearColor& Color, float Thick)
	{
		TArray<FVector2f> Pts;
		const int32 N = 40;
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = 2.f * PI * i / N;
			Pts.Add(FVector2f(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R));
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thick);
	}
}
