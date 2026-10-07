#include "ZoogiUIFx.h"
#include "ZoogiGemRender.inl"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "TextureResource.h"

// ------------------------------------------------------------------ pixels / textures

namespace
{
	FColor White(float A) { return FColor(255, 255, 255, (uint8)FMath::Clamp(FMath::RoundToInt(A * 255.f), 0, 255)); }
}

ZoogiUIFx::FPixels ZoogiUIFx::SoftDot(int32 N)
{
	FPixels P; P.W = P.H = N; P.Px.SetNumUninitialized(N * N);
	const float C = (N - 1) * 0.5f;
	for (int32 y = 0; y < N; ++y)
		for (int32 x = 0; x < N; ++x)
		{
			const float R = FMath::Sqrt(FMath::Square(x - C) + FMath::Square(y - C)) / (N * 0.5f);
			const float Core = FMath::Exp(-R * R * 9.f);
			const float Halo = FMath::Exp(-R * R * 3.2f) * 0.45f;
			P.Px[y * N + x] = White(FMath::Clamp((Core + Halo) * FMath::Clamp((1.f - R) * 6.f, 0.f, 1.f), 0.f, 1.f));
		}
	return P;
}

ZoogiUIFx::FPixels ZoogiUIFx::ShimmerBand(int32 W, int32 H)
{
	FPixels P; P.W = W; P.H = H; P.Px.SetNumUninitialized(W * H);
	for (int32 y = 0; y < H; ++y)
		for (int32 x = 0; x < W; ++x)
		{
			const float U = (x + 0.5f) / W * 2.f - 1.f;
			const float V = (y + 0.5f) / H * 2.f - 1.f;
			const float A = FMath::Exp(-U * U * 5.f) * (0.65f + 0.35f * FMath::Exp(-U * U * 40.f)) * FMath::Clamp((1.f - FMath::Abs(V)) * 4.f, 0.f, 1.f);
			P.Px[y * W + x] = White(A);
		}
	return P;
}

ZoogiUIFx::FPixels ZoogiUIFx::PlayIcon(int32 N)
{
	FPixels P; P.W = P.H = N; P.Px.SetNumUninitialized(N * N);
	// Triangle pointing right, softly rounded by supersampling + a small inset.
	const FVector2D A(0.24f, 0.14f), B(0.24f, 0.86f), C(0.88f, 0.5f);
	auto Edge = [](const FVector2D& P0, const FVector2D& P1, const FVector2D& Q) { return (P1.X - P0.X) * (Q.Y - P0.Y) - (P1.Y - P0.Y) * (Q.X - P0.X); };
	const int32 SS = 4;
	for (int32 y = 0; y < N; ++y)
		for (int32 x = 0; x < N; ++x)
		{
			int32 In = 0;
			for (int32 sy = 0; sy < SS; ++sy)
				for (int32 sx = 0; sx < SS; ++sx)
				{
					const FVector2D Q((x + (sx + 0.5f) / SS) / N, (y + (sy + 0.5f) / SS) / N);
					const float e0 = Edge(A, B, Q), e1 = Edge(B, C, Q), e2 = Edge(C, A, Q);
					if ((e0 <= 0 && e1 <= 0 && e2 <= 0) || (e0 >= 0 && e1 >= 0 && e2 >= 0)) ++In;
				}
			P.Px[y * N + x] = White(In / float(SS * SS));
		}
	return P;
}

UTexture2D* ZoogiUIFx::ToTexture(const FPixels& P)
{
	if (P.W <= 0 || P.H <= 0 || P.Px.Num() != P.W * P.H) return nullptr;
	UTexture2D* T = UTexture2D::CreateTransient(P.W, P.H, PF_B8G8R8A8);
	if (!T) return nullptr;
	T->SRGB = true;
	T->Filter = TF_Bilinear;
	T->AddressX = TA_Clamp;
	T->AddressY = TA_Clamp;
	FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, P.Px.GetData(), P.Px.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	T->UpdateResource();
	return T;
}

ZoogiUIFx::FGemFrames ZoogiUIFx::RenderGem(uint8 Artifact, const FLinearColor& Core, const FLinearColor& Rim, int32 Size, float StepDeg)
{
	FGemFrames Out;
	Out.Artifact = Artifact;
	const int32 Kind = FMath::Clamp((int32)Artifact, 1, 5);
	Out.PeriodDeg = ZoogiGemCore::PeriodDeg(Kind);
	const int32 N = FMath::Max(4, FMath::RoundToInt(Out.PeriodDeg / FMath::Max(0.5f, StepDeg)));
	const float CoreC[3] = { Core.R, Core.G, Core.B }, RimC[3] = { Rim.R, Rim.G, Rim.B };
	std::vector<ZoogiGemCore::RGBA> Buf(Size * Size);
	Out.Frames.SetNum(N);
	for (int32 f = 0; f < N; ++f)
	{
		ZoogiGemCore::RenderFrame(Kind, CoreC, RimC, Size, Out.PeriodDeg * f / N, 22.f, Buf.data());
		FPixels& P = Out.Frames[f];
		P.W = P.H = Size;
		P.Px.SetNumUninitialized(Size * Size);
		for (int32 i = 0; i < Size * Size; ++i)
		{
			const ZoogiGemCore::RGBA& C = Buf[i];
			P.Px[i] = FLinearColor(FMath::Min(C.r, 1.f), FMath::Min(C.g, 1.f), FMath::Min(C.b, 1.f), C.a).ToFColor(true);
		}
	}
	return Out;
}

float ZoogiUIFx::EaseOutCubic(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - FMath::Pow(1.f - T, 3.f); }
float ZoogiUIFx::EaseOutBack(float T)
{
	T = FMath::Clamp(T, 0.f, 1.f);
	const float C1 = 1.70158f, C3 = C1 + 1.f;
	return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
}
float ZoogiUIFx::EaseInOutSine(float T) { T = FMath::Clamp(T, 0.f, 1.f); return -(FMath::Cos(UE_PI * T) - 1.f) * 0.5f; }
float ZoogiUIFx::SweepPhase(float Time, float Period, float Offset, float CrossFraction)
{
	const float Ph = FMath::Fmod(Time + Offset, Period) / Period; // 0..1
	return Ph < CrossFraction ? EaseInOutSine(Ph / CrossFraction) : -1.f; // -1: resting (hidden)
}

// ------------------------------------------------------------------ sparkle field

void SZoogiSparkleField::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(64.f, 64.f);
}

void SZoogiSparkleField::SetDot(UTexture2D* Tex)
{
	Brush.SetResourceObject(Tex);
}

void SZoogiSparkleField::SetTime(float InTime)
{
	Time = InTime;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SZoogiSparkleField::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!Brush.GetResourceObject()) return LayerId;
	const FVector2D Sz = AllottedGeometry.GetLocalSize();
	if (Sz.X < 4.f || Sz.Y < 4.f) return LayerId;
	static const FLinearColor Cols[5] = {
		FLinearColor(1.f, 0.84f, 0.45f), FLinearColor(0.55f, 0.9f, 1.f), FLinearColor(1.f, 0.62f, 0.86f), FLinearColor(1.f, 1.f, 1.f), FLinearColor(0.7f, 1.f, 0.8f) };
	auto Rnd = [](uint32 I, uint32 K) { return (ZoogiGemCore::Hash(I * 7919u + K * 104729u + 13u) & 0xFFFF) / 65535.f; };
	const float Scale = Sz.Y / 1080.f;
	for (int32 i = 0; i < Count; ++i)
	{
		const bool bBokeh = i < 9;
		const float Speed = bBokeh ? FMath::Lerp(0.006f, 0.014f, Rnd(i, 1)) : FMath::Lerp(0.018f, 0.06f, Rnd(i, 1));
		const float Ph = Rnd(i, 2) * 100.f;
		float Y = FMath::Frac(Rnd(i, 3) - Time * Speed);                       // 0..1, drifting upward
		const float Sway = FMath::Sin(Time * FMath::Lerp(0.25f, 0.7f, Rnd(i, 4)) + Ph) * (bBokeh ? 0.03f : 0.018f);
		const float X = FMath::Frac(Rnd(i, 5) + Sway + Time * 0.003f * (Rnd(i, 6) - 0.5f));
		const float Size = (bBokeh ? FMath::Lerp(90.f, 220.f, Rnd(i, 7)) : FMath::Lerp(5.f, 16.f, FMath::Square(Rnd(i, 7)))) * Scale;
		const float Tw = bBokeh ? 0.5f + 0.5f * FMath::Sin(Time * 0.4f + Ph) : 0.35f + 0.65f * FMath::Square(0.5f + 0.5f * FMath::Sin(Time * FMath::Lerp(1.2f, 3.4f, Rnd(i, 8)) + Ph));
		const float EdgeFade = FMath::Clamp(Y * 6.f, 0.f, 1.f) * FMath::Clamp((1.f - Y) * 4.f, 0.f, 1.f);
		FLinearColor C = Cols[(int32)(Rnd(i, 9) * 4.99f)];
		C.A = (bBokeh ? 0.075f : 0.8f) * Tw * EdgeFade * Opacity * InWidgetStyle.GetColorAndOpacityTint().A;
		if (C.A < 0.004f) continue;
		const FVector2D Pos(X * Sz.X - Size * 0.5f, (Y * 1.1f - 0.05f) * Sz.Y - Size * 0.5f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(Size, Size), FSlateLayoutTransform(1.f, Pos)), &Brush, ESlateDrawEffect::None, C);
	}
	return LayerId;
}

void UZoogiSparkleField::SetDot(UTexture2D* Tex)
{
	Dot = Tex;
	if (Field.IsValid()) Field->SetDot(Tex);
}

void UZoogiSparkleField::SetTime(float InTime)
{
	if (Field.IsValid()) Field->SetTime(InTime);
}

void UZoogiSparkleField::SetDensity(int32 InCount, float InOpacity)
{
	Count = InCount; Opacity = InOpacity;
	if (Field.IsValid()) Field->SetDensity(Count, Opacity);
}

TSharedRef<SWidget> UZoogiSparkleField::RebuildWidget()
{
	Field = SNew(SZoogiSparkleField);
	Field->SetDot(Dot);
	Field->SetDensity(Count, Opacity);
	return Field.ToSharedRef();
}

void UZoogiSparkleField::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Field.Reset();
}
