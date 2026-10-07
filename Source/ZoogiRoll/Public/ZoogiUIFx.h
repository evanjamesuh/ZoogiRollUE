#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
#include "ZoogiUIFx.generated.h"

class UTexture2D;

/**
 * UI polish helpers (Oct 6 "ui-polish"): procedural UI textures, the ray-traced rotating artifact stones, easing curves and a
 * drifting light-particle layer. Everything is generated in code (no assets), in Zoogi Roll's own palette.
 */
namespace ZoogiUIFx
{
	/** CPU-side BGRA8 image (sRGB). Safe to build on a worker thread; turned into a texture on the game thread. */
	struct FPixels
	{
		int32 W = 0, H = 0;
		TArray<FColor> Px;
	};

	/** Soft round light (white, gaussian alpha): glows, particles. */
	FPixels SoftDot(int32 N);
	/** Vertical light band (white, gaussian alpha across X): shimmer sweeps. */
	FPixels ShimmerBand(int32 W, int32 H);
	/** Rounded "play" triangle (white). */
	FPixels PlayIcon(int32 N);
	/** Transient texture from pixels (game thread). */
	UTexture2D* ToTexture(const FPixels& P);

	/** A turn of an artifact stone: ray-traced faceted gem (one frame per StepDeg over the shape's symmetry period). */
	struct FGemFrames
	{
		uint8 Artifact = 0;
		float PeriodDeg = 360.f;
		TArray<FPixels> Frames;
	};
	/** Thread-safe (pure CPU). Shapes: Ember flame shard, Storm octahedron "eye", Frost quartz column, Titan brilliant core,
	 *  Void dodecahedron with an accretion ring. */
	FGemFrames RenderGem(uint8 Artifact, const FLinearColor& Core, const FLinearColor& Rim, int32 Size, float StepDeg);

	float EaseOutCubic(float T);
	float EaseOutBack(float T);
	float EaseInOutSine(float T);
	/** 0..1 sweep position for a shimmer that crosses every Period seconds and rests the remaining time. */
	float SweepPhase(float Time, float Period, float Offset, float CrossFraction = 0.35f);
}

/** Slate leaf that paints drifting, twinkling light motes (a handful of large soft bokeh plus small sparks). */
class ZOOGIROLL_API SZoogiSparkleField : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiSparkleField) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetDot(UTexture2D* Tex);
	void SetTime(float InTime);
	void SetDensity(int32 InCount, float InOpacity) { Count = InCount; Opacity = InOpacity; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(8.f, 8.f); }
	virtual bool ComputeVolatility() const override { return true; }

private:
	FSlateBrush Brush;
	float Time = 0.f;
	int32 Count = 72;
	float Opacity = 1.f;
};

/** UMG wrapper for SZoogiSparkleField (menu background layer). Hit-test invisible. */
UCLASS()
class ZOOGIROLL_API UZoogiSparkleField : public UWidget
{
	GENERATED_BODY()
public:
	void SetDot(UTexture2D* Tex);
	void SetTime(float InTime);
	void SetDensity(int32 InCount, float InOpacity);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY() TObjectPtr<UTexture2D> Dot;
	TSharedPtr<SZoogiSparkleField> Field;
	int32 Count = 72;
	float Opacity = 1.f;
};
