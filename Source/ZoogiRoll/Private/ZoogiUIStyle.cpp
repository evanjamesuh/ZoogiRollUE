// uistyle: see ZoogiUIStyle.h
#include "ZoogiUIStyle.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Rendering/DrawElements.h"
#include "Layout/Geometry.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace ZoogiUIStyle
{
	const FLinearColor Outline(0.012f, 0.025f, 0.11f, 1.f);
	const FLinearColor PanelNavy(0.03f, 0.10f, 0.36f, 0.86f);
	const FLinearColor PanelTeal(0.0f, 0.30f, 0.40f, 0.86f);
	const FLinearColor Track(0.02f, 0.05f, 0.22f, 0.95f);
	const FLinearColor ButtonNavy(0.05f, 0.16f, 0.50f, 0.95f);
	const FLinearColor Muted(0.22f, 0.32f, 0.62f, 1.f);
	const FLinearColor MutedText(0.62f, 0.75f, 1.f, 1.f);
	const FLinearColor EnergyA(0.15f, 0.95f, 1.f, 1.f), EnergyB(0.10f, 0.35f, 1.f, 1.f);
	const FLinearColor CardEnergyA(0.85f, 0.45f, 1.f, 1.f), CardEnergyB(0.50f, 0.15f, 1.f, 1.f);
	const FLinearColor Gold(1.f, 0.82f, 0.25f, 1.f);
	const FLinearColor LossRed(1.f, 0.15f, 0.2f, 1.f);

	FLinearColor HealthColor(float Frac)
	{
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		const FLinearColor G(0.25f, 1.f, 0.3f), Y(1.f, 0.88f, 0.15f), R(1.f, 0.2f, 0.18f);
		return Frac > 0.5f ? FMath::Lerp(Y, G, (Frac - 0.5f) * 2.f) : FMath::Lerp(R, Y, Frac * 2.f);
	}

	void FBarJuice::Update(float Target, float Dt)
	{
		if (Shown < 0.f) { Shown = Trail = Last = Target; return; }
		if (Target > Last + KINDA_SMALL_NUMBER) Flash = 1.f;
		else if (Target < Last - KINDA_SMALL_NUMBER) { Shake = 1.f; Trail = FMath::Max(Trail, Last); }
		Last = Target;
		Shown = FMath::FInterpTo(Shown, Target, Dt, 12.f);
		if (FMath::Abs(Shown - Target) < 0.002f) Shown = Target;
		// the red trail holds briefly, then drains down to the value
		if (Trail > Shown) Trail = FMath::Max(Shown, Trail - Dt * FMath::Max(0.6f, (Trail - Shown) * 2.5f) * (Shake > 0.6f ? 0.f : 1.f));
		else Trail = Shown;
		Flash = FMath::Max(0.f, Flash - Dt * 3.5f);
		Shake = FMath::Max(0.f, Shake - Dt * 3.f);
	}

	FVector2D FBarJuice::ShakeOffset(float Amp, float Time) const
	{
		if (Shake <= 0.f) return FVector2D::ZeroVector;
		const float A = Amp * Shake * Shake;
		return FVector2D(FMath::Sin(Time * 71.f) * A, FMath::Cos(Time * 53.f) * A * 0.5f);
	}

	void AddOutline(FSlateFontInfo& F, float OutlinePx)
	{
		F.OutlineSettings.OutlineSize = FMath::RoundToInt(OutlinePx);
		F.OutlineSettings.OutlineColor = Outline;
	}

	FSlateFontInfo BoldFont(int32 Size, float OutlinePx)
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", Size);
		AddOutline(F, OutlinePx);
		return F;
	}

	FSlateFontInfo CanvasFont(int32 Size, float OutlinePx)
	{
		// Canvas text needs a font object (the legacy engine font's Slate info), not just a composite style font
		UFont* F = GEngine ? GEngine->GetLargeFont() : nullptr;
		FSlateFontInfo Fi = F ? F->GetLegacySlateFontInfo() : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Fi.Size = Size;
		Fi.TypefaceFontName = FName(TEXT("Bold"));
		AddOutline(Fi, OutlinePx);
		return Fi;
	}

	// ------------------------------------------------------------------ Canvas
	void CRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col)
	{
		if (!C || W <= 0.f || H <= 0.f || Col.A <= 0.f) return;
		FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), Col);
		T.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(T);
	}

	void CRoundRect(UCanvas* C, float X, float Y, float W, float H, float R, const FLinearColor& Col)
	{
		if (!C || W <= 0.f || H <= 0.f || Col.A <= 0.f) return;
		R = FMath::Clamp(R, 0.f, FMath::Min(W, H) * 0.5f);
		if (R < 1.f) { CRect(C, X, Y, W, H, Col); return; }
		// non-overlapping pieces so translucent fills stay even: middle column, side columns, 4 quarter-disc fans
		CRect(C, X + R, Y, W - 2.f * R, H, Col);
		CRect(C, X, Y + R, R, H - 2.f * R, Col);
		CRect(C, X + W - R, Y + R, R, H - 2.f * R, Col);
		TArray<FCanvasUVTri> Tris;
		const int32 Seg = 6;
		const FVector2D Ctr[4] = { FVector2D(X + R, Y + R), FVector2D(X + W - R, Y + R), FVector2D(X + W - R, Y + H - R), FVector2D(X + R, Y + H - R) };
		const float Start[4] = { PI, 1.5f * PI, 0.f, 0.5f * PI };
		for (int32 k = 0; k < 4; ++k)
			for (int32 i = 0; i < Seg; ++i)
			{
				const float A0 = Start[k] + 0.5f * PI * i / Seg, A1 = Start[k] + 0.5f * PI * (i + 1) / Seg;
				FCanvasUVTri T;
				T.V0_Pos = FVector2D(Ctr[k]); T.V1_Pos = FVector2D(Ctr[k] + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R); T.V2_Pos = FVector2D(Ctr[k] + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R);
				T.V0_Color = T.V1_Color = T.V2_Color = Col;
				Tris.Add(T);
			}
		FCanvasTriangleItem TI(Tris, C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr); if (!TI.Texture) return;
		TI.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(TI);
	}

	void CPanel(UCanvas* C, float X, float Y, float W, float H, float U, const FLinearColor& Fill, const FLinearColor& Accent)
	{
		const float O = 3.5f * U, R = FMath::Min(14.f * U, H * 0.5f);
		CRoundRect(C, X + 3.f * U, Y + 4.f * U, W, H, R, FLinearColor(0.f, 0.f, 0.05f, 0.35f * Fill.A)); // drop shadow
		CRoundRect(C, X, Y, W, H, R, FLinearColor(Outline.R, Outline.G, Outline.B, FMath::Min(1.f, Fill.A + 0.12f)));
		CRoundRect(C, X + O, Y + O, W - 2.f * O, H - 2.f * O, FMath::Max(0.f, R - O), Fill);
		CRect(C, X + O + R * 0.5f, Y + O + 2.f * U, W - 2.f * O - R, FMath::Min(4.f * U, H * 0.1f), FLinearColor(1.f, 1.f, 1.f, 0.10f * Fill.A)); // sheen
		if (Accent.A > 0.f) CRect(C, X + R, Y + O * 0.5f, W - 2.f * R, 3.f * U, Accent);
	}

	void CBar(UCanvas* C, float X, float Y, float W, float H, float U, float Frac, const FLinearColor& A, const FLinearColor& B, const FBarJuice* J, int32 Ticks, float Time)
	{
		if (!C) return;
		if (J) { const FVector2D S = J->ShakeOffset(3.f * U, Time); X += S.X; Y += S.Y; Frac = J->Shown >= 0.f ? J->Shown : Frac; }
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		const float O = FMath::Max(2.f, 3.f * U), R = H * 0.5f;
		CRoundRect(C, X - O, Y - O, W + 2.f * O, H + 2.f * O, R + O, Outline);
		CRoundRect(C, X, Y, W, H, R, Track);
		// red loss trail
		if (J && J->Trail > Frac + 0.001f) CRoundRect(C, X, Y, W * FMath::Clamp(J->Trail, 0.f, 1.f), H, R, LossRed);
		// gradient fill: rounded cap at each end + stepped colour along the bar
		const float FW = W * Frac;
		if (FW > 0.5f)
		{
			const int32 N = FMath::Clamp(FMath::RoundToInt(FW / (6.f * U)), 1, 48);
			const float Cap = FMath::Min(R, FW * 0.5f);
			CRoundRect(C, X, Y, FMath::Min(FW, 2.f * Cap), H, Cap, A);
			CRoundRect(C, X + FW - FMath::Min(FW, 2.f * Cap), Y, FMath::Min(FW, 2.f * Cap), H, Cap, FMath::Lerp(A, B, Frac));
			const float X0 = X + Cap, X1 = X + FW - Cap;
			for (int32 i = 0; i < N && X1 > X0; ++i)
			{
				const float S0 = X0 + (X1 - X0) * i / N, S1 = X0 + (X1 - X0) * (i + 1) / N;
				CRect(C, S0, Y, S1 - S0 + 0.5f, H, FMath::Lerp(A, B, ((S0 + S1) * 0.5f - X) / W));
			}
			// darker underside + white gloss strip along the top
			CRect(C, X + Cap, Y + H * 0.72f, FMath::Max(0.f, FW - 2.f * Cap), H * 0.28f, FLinearColor(0.f, 0.02f, 0.15f, 0.25f));
			CRoundRect(C, X + FMath::Min(Cap, FW * 0.5f) * 0.6f, Y + H * 0.12f, FMath::Max(0.f, FW - FMath::Min(Cap, FW * 0.5f) * 1.2f), FMath::Max(1.f, H * 0.26f), H * 0.13f, FLinearColor(1.f, 1.f, 1.f, 0.55f));
		}
		// inner tick marks (tech)
		for (int32 t = 1; t < Ticks; ++t)
		{
			const float TX = X + W * t / Ticks;
			CRect(C, TX - 1.f * U, Y + H * 0.55f, 2.f * U, H * 0.45f, FLinearColor(Outline.R, Outline.G, Outline.B, 0.55f));
		}
		// hex notch accents at both ends of the frame
		auto Hex = [&](float CX, float CY, float Rr, const FLinearColor& Col)
		{
			TArray<FCanvasUVTri> Tris;
			for (int32 i = 0; i < 6; ++i)
			{
				const float A0 = PI / 6.f + i * PI / 3.f, A1 = PI / 6.f + (i + 1) * PI / 3.f;
				FCanvasUVTri T;
				T.V0_Pos = FVector2D(CX, CY); T.V1_Pos = FVector2D(CX + FMath::Cos(A0) * Rr, CY + FMath::Sin(A0) * Rr); T.V2_Pos = FVector2D(CX + FMath::Cos(A1) * Rr, CY + FMath::Sin(A1) * Rr);
				T.V0_Color = T.V1_Color = T.V2_Color = Col;
				Tris.Add(T);
			}
			FCanvasTriangleItem TI(Tris, C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr); if (!TI.Texture) return; TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI);
		};
		const float HR = FMath::Max(3.f * U, H * 0.32f);
		Hex(X + W + O + HR * 0.9f, Y + H * 0.5f, HR + 1.5f * U, Outline);
		Hex(X + W + O + HR * 0.9f, Y + H * 0.5f, HR - 0.5f * U, FMath::Lerp(A, B, 1.f));
		// white flash on gain
		if (J && J->Flash > 0.f) CRoundRect(C, X, Y, W, H, R, FLinearColor(1.f, 1.f, 1.f, 0.65f * J->Flash));
	}

	void CText(UCanvas* C, const FString& S, float X, float Y, const FSlateFontInfo& Font, const FLinearColor& Col, float U)
	{
		if (!C) return;
		FCanvasTextItem I(FVector2D(X, Y), FText::FromString(S), Font, Col);
		I.EnableShadow(FLinearColor(0.f, 0.f, 0.06f, 0.8f * Col.A), FVector2D(2.f * U, 3.f * U));
		I.bOutlined = true;
		I.OutlineColor = FLinearColor(Outline.R, Outline.G, Outline.B, Col.A);
		C->DrawItem(I);
	}

	// ------------------------------------------------------------------ Slate
	void SBox(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& Fill, const FLinearColor& Edge, float R, float EdgeW)
	{
		// Tint-driven (white rounded brush * colour): an outline pass then the inset fill, so fills never depend on the
		// brush's own colour field.
		auto Draw = [&](const FVector2D& PP, const FVector2D& SS, float RR, const FLinearColor& Col)
		{
			if (SS.X <= 0.f || SS.Y <= 0.f || Col.A <= 0.f) return;
			const FSlateRoundedBoxBrush B(FLinearColor::White, FMath::Max(0.f, RR));
			FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2f(SS), FSlateLayoutTransform(FVector2f(PP))), &B, ESlateDrawEffect::None, Col);
		};
		if (EdgeW > 0.f && Edge.A > 0.f)
		{
			Draw(P, S, R, Edge);
			Draw(P + FVector2D(EdgeW, EdgeW), S - FVector2D(2.f * EdgeW, 2.f * EdgeW), R - EdgeW, Fill);
		}
		else Draw(P, S, R, Fill);
	}

	void SPanel(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FLinearColor& Fill, const FLinearColor& Accent, float R)
	{
		SBox(Out, L, G, P + FVector2D(3.f, 5.f), S, FLinearColor(0.f, 0.f, 0.05f, 0.35f), FLinearColor::Transparent, R, 0.f);
		SBox(Out, L, G, P, S, Fill, Outline, R, 3.5f);
		SBox(Out, L, G, P + FVector2D(R * 0.6f, 5.f), FVector2D(FMath::Max(0.f, S.X - R * 1.2f), 4.f), FLinearColor(1.f, 1.f, 1.f, 0.12f), FLinearColor::Transparent, 2.f, 0.f);
		if (Accent.A > 0.f) SBox(Out, L, G, P + FVector2D(R, 1.5f), FVector2D(FMath::Max(0.f, S.X - 2.f * R), 3.f), Accent, FLinearColor::Transparent, 1.5f, 0.f);
	}

	void SBar(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& InP, const FVector2D& S, float Frac, const FLinearColor& A, const FLinearColor& B, const FBarJuice* J, int32 Ticks)
	{
		FVector2D P = InP;
		if (J) { P += J->ShakeOffset(3.f, (float)FPlatformTime::Seconds()); Frac = J->Shown >= 0.f ? J->Shown : Frac; }
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		const float R = S.Y * 0.5f;
		SBox(Out, L, G, P - FVector2D(3.5f, 3.5f), S + FVector2D(7.f, 7.f), Outline, FLinearColor::Transparent, R + 3.5f, 0.f);
		SBox(Out, L, G, P, S, Track, FLinearColor::Transparent, R, 0.f);
		if (J && J->Trail > Frac + 0.001f) SBox(Out, L + 1, G, P, FVector2D(S.X * FMath::Clamp(J->Trail, 0.f, 1.f), S.Y), LossRed, FLinearColor::Transparent, R, 0.f);
		const float FW = S.X * Frac;
		if (FW > 0.5f)
		{
			// stepped gradient (each step a rounded box clipped by width; steps overlap so the joins are invisible)
			const int32 N = FMath::Clamp(FMath::RoundToInt(FW / 6.f), 1, 48);
			for (int32 i = N - 1; i >= 0; --i)
			{
				const float W = FW * (i + 1) / N;
				SBox(Out, L + 2, G, P, FVector2D(W, S.Y), FMath::Lerp(A, B, W / S.X), FLinearColor::Transparent, FMath::Min(R, W * 0.5f), 0.f);
			}
			SBox(Out, L + 2, G, P + FVector2D(FMath::Min(R, FW * 0.5f) * 0.6f, S.Y * 0.12f), FVector2D(FMath::Max(0.f, FW - FMath::Min(R, FW * 0.5f) * 1.2f), FMath::Max(1.f, S.Y * 0.26f)),
				FLinearColor(1.f, 1.f, 1.f, 0.55f), FLinearColor::Transparent, S.Y * 0.13f, 0.f);
		}
		for (int32 t = 1; t < Ticks; ++t)
			SBox(Out, L + 3, G, P + FVector2D(S.X * t / Ticks - 1.f, S.Y * 0.55f), FVector2D(2.f, S.Y * 0.45f), FLinearColor(Outline.R, Outline.G, Outline.B, 0.55f), FLinearColor::Transparent, 0.f, 0.f);
		// hex-ish end notch (a rotated rounded square reads as a tech bolt)
		const float HR = FMath::Max(4.f, S.Y * 0.36f);
		SBox(Out, L + 3, G, P + FVector2D(S.X + 4.f, S.Y * 0.5f - HR), FVector2D(HR * 2.f, HR * 2.f), B, Outline, HR * 0.35f, 2.5f);
		if (J && J->Flash > 0.f) SBox(Out, L + 3, G, P, S, FLinearColor(1.f, 1.f, 1.f, 0.65f * J->Flash), FLinearColor::Transparent, R, 0.f);
	}

	void SText(FSlateWindowElementList& Out, int32 L, const FGeometry& G, const FVector2D& P, const FString& T, const FSlateFontInfo& F, const FLinearColor& C)
	{
		FSlateFontInfo Sh = F; Sh.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.06f, 0.6f * C.A);
		FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(FVector2f(800.f, 400.f), FSlateLayoutTransform(FVector2f(P + FVector2D(2.f, 3.f)))), T, Sh, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.06f, 0.6f * C.A));
		FSlateDrawElement::MakeText(Out, L + 1, G.ToPaintGeometry(FVector2f(800.f, 400.f), FSlateLayoutTransform(FVector2f(P))), T, F, ESlateDrawEffect::None, C);
	}

	FSlateBrush RoundBrush(const FLinearColor& Fill, const FLinearColor& Edge, float EdgeW, float R)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(R, R, R, R), FSlateColor(Edge), EdgeW);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	FButtonStyle ButtonStyle(const FLinearColor& Accent, bool bFilled)
	{
		const FLinearColor Base = bFilled ? FLinearColor(Accent.R * 0.85f, Accent.G * 0.85f, Accent.B * 0.85f, 0.97f) : ButtonNavy;
		const FLinearColor Hover = bFilled ? FLinearColor(FMath::Min(1.f, Accent.R * 1.05f), FMath::Min(1.f, Accent.G * 1.05f), FMath::Min(1.f, Accent.B * 1.05f), 1.f) : FLinearColor(0.08f, 0.26f, 0.72f, 0.98f);
		FButtonStyle S;
		S.SetNormal(RoundBrush(Base, Outline, 3.5f, 12.f));
		S.SetHovered(RoundBrush(Hover, Outline, 3.5f, 12.f));
		S.SetPressed(RoundBrush(Accent * 0.6f + FLinearColor(0.f, 0.f, 0.f, 0.4f), FLinearColor::White, 3.5f, 12.f));
		S.SetDisabled(RoundBrush(FLinearColor(0.12f, 0.18f, 0.42f, 0.8f), Outline, 3.5f, 12.f));
		S.SetNormalPadding(FMargin(6.f, 2.f));
		S.SetPressedPadding(FMargin(6.f, 4.f, 6.f, 0.f));
		return S;
	}
}

namespace ZoogiUIStyle
{
	float EaseOutBack(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		const float C1 = 1.70158f, C3 = C1 + 1.f;
		return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
	}

	void FButtonAnim::Tick(bool bHovered, bool bPressed, float Dt, float Time)
	{
		Dt = FMath::Min(Dt, 1.f / 20.f);
		Age += Dt;
		if (bDisabled) { OutScale = 1.f; OutOpacity = 1.f; OutGloss = 0.f; return; }
		if (bPressed)
		{
			// squash fast (~60 ms), no spring while held
			Scale = FMath::FInterpTo(Scale, 0.94f, Dt, 35.f);
			Vel = 0.f;
		}
		else
		{
			// under-damped spring towards 1.05 (hover) / 1.0: bouncy release, settles in ~150-180 ms
			const float Target = bHovered ? 1.05f : 1.f;
			const float K = bWasPressed ? 1400.f : 900.f, C = bWasPressed ? 26.f : 42.f;
			if (bWasPressed) Vel += 2.2f; // release kick
			Vel += (K * (Target - Scale) - C * Vel) * Dt;
			Scale += Vel * Dt;
		}
		bWasPressed = bPressed;
		Gloss = FMath::FInterpTo(Gloss, (bHovered || bPressed) ? 1.f : 0.f, Dt, 18.f);
		float S = Scale, G = Gloss, Op = 1.f;
		if (bEnter)
		{
			const float T = (Age - Delay) / 0.18f;
			S *= T <= 0.f ? 0.6f : 0.6f + 0.4f * EaseOutBack(T);
			Op = FMath::Clamp(T * 2.5f, 0.f, 1.f);
		}
		if (bPrimary)
		{
			// idle: gentle 1.6 s breathing + a gloss shine flash every 2.4 s
			const float P = 0.5f + 0.5f * FMath::Sin(Time * 2.f * PI / 1.6f);
			S *= 1.f + 0.02f * P;
			const float Ph = FMath::Fmod(Time, 2.4f) / 0.5f;
			if (Ph < 1.f) G = FMath::Max(G, FMath::Sin(Ph * PI));
		}
		OutScale = S; OutOpacity = Op; OutGloss = G;
	}
}
