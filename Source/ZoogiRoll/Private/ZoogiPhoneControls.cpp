// ZoogiPhoneControls.cpp — procedural comic-glossy blue bubble sticks matching approved-omni-sticks.png.
#include "ZoogiPhoneControls.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "CanvasItem.h"
#include "Engine/Texture.h"

namespace ZoogiPhoneControls
{
	static FTune GTune;
	static FState GState;
	static bool bTuneLoaded = false;
	static int32 EnabledOverride = -1; // -1 unset, 0 force off, 1 force on
	static int32 SticksOverride = -1; // -1 unset, 1 force draw sticks (-ZoogiPhoneControlsSticks)

	static FOnAimAxis GAimAxis;
	static FOnPowerPull01 GPowerPull;
	static FOnFireRelease GFireRelease;
	static FOnCancelAbort GCancelAbort;
	static FOnAbilityIndex GAbilityIndex;
	static FOnAwakenPressed GAwakenPressed;
	static FOnStandbyLookAxis GStandbyLook;
	static FOnStandbyPeek GStandbyPeek;

	FOnAimAxis& AimAxisDelegate() { return GAimAxis; }
	FOnPowerPull01& PowerPullDelegate() { return GPowerPull; }
	FOnFireRelease& FireReleaseDelegate() { return GFireRelease; }
	FOnCancelAbort& CancelAbortDelegate() { return GCancelAbort; }
	FOnAbilityIndex& AbilityIndexDelegate() { return GAbilityIndex; }
	FOnAwakenPressed& AwakenPressedDelegate() { return GAwakenPressed; }
	FOnStandbyLookAxis& StandbyLookAxisDelegate() { return GStandbyLook; }
	FOnStandbyPeek& StandbyPeekDelegate() { return GStandbyPeek; }

	const FTune& Tune() { if (!bTuneLoaded) ReloadTune(); return GTune; }
	FState& State() { return GState; }

	void ReloadTune()
	{
		bTuneLoaded = true;
		FTune T;
		GConfig->GetBool(TEXT("ZoogiRoll.PhoneControls"), TEXT("bEnabled"), T.bEnabled, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("LeftStickCX"), T.LeftStickCX, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("LeftStickCY"), T.LeftStickCY, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("LeftStickR"), T.LeftStickR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("LeftKnobR"), T.LeftKnobR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("RightStickCX"), T.RightStickCX, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("RightStickCY"), T.RightStickCY, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("RightStickR"), T.RightStickR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("RightKnobR"), T.RightKnobR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AbilityArcR"), T.AbilityArcR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AbilityBubbleR"), T.AbilityBubbleR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AbilityAngle0"), T.AbilityAngle0, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AbilityAngle1"), T.AbilityAngle1, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AbilityAngle2"), T.AbilityAngle2, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AbilityAngle3"), T.AbilityAngle3, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AwakenAngle"), T.AwakenAngle, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AwakenR"), T.AwakenR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("AwakenBubbleR"), T.AwakenBubbleR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("CancelRingR"), T.CancelRingR, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("CancelRingThick"), T.CancelRingThick, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("HitPad"), T.HitPad, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("Alpha"), T.Alpha, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("GlowMul"), T.GlowMul, GGameIni);
		GConfig->GetBool(TEXT("ZoogiRoll.PhoneControls"), TEXT("bDrawOmniSticks"), T.bDrawOmniSticks, GGameIni);
		GConfig->GetBool(TEXT("ZoogiRoll.PhoneControls"), TEXT("bDrawAbilityOrbs"), T.bDrawAbilityOrbs, GGameIni);
		GConfig->GetBool(TEXT("ZoogiRoll.PhoneControls"), TEXT("bDrawAwakenOrb"), T.bDrawAwakenOrb, GGameIni);
		GConfig->GetBool(TEXT("ZoogiRoll.PhoneControls"), TEXT("bHideArenaAwakenWithOrbs"), T.bHideArenaAwakenWithOrbs, GGameIni);
		GConfig->GetBool(TEXT("ZoogiRoll.PhoneControls"), TEXT("bUseOrbScreenAnchor"), T.bUseOrbScreenAnchor, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("OrbAnchorXPct"), T.OrbAnchorXPct, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.PhoneControls"), TEXT("OrbAnchorYPct"), T.OrbAnchorYPct, GGameIni);
		GTune = T;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPhoneControls"))) EnabledOverride = 0;
		else if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiPhoneControls"))) EnabledOverride = 1;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiPhoneControlsSticks"))) SticksOverride = 1;
	}

	bool IsEnabled()
	{
		if (!bTuneLoaded) ReloadTune();
		if (EnabledOverride == 0) return false;
		if (EnabledOverride == 1) return true;
		return GTune.bEnabled;
	}

	static bool ShouldDrawSticks()
	{
		if (!bTuneLoaded) ReloadTune();
		if (SticksOverride == 1) return true;
		return GTune.bDrawOmniSticks;
	}

	bool ShouldHideArenaAwakenButton()
	{
		if (!IsEnabled()) return false;
		const FTune& T = Tune();
		// One Awaken path: Phone orb drawn => Arena large gold AWAKEN button hides. Charge pips untouched.
		return T.bHideArenaAwakenWithOrbs; // Oct 24: keep the arena AWAKEN button hidden now the phone bubbles are gone too
	}

	FLayout ComputeLayout(float CanvasW, float CanvasH)
	{
		const FTune& T = Tune();
		FLayout L;
		L.U = CanvasH / 1080.f;
		const float U = L.U;
		L.LeftC = FVector2D(T.LeftStickCX * U, CanvasH - T.LeftStickCY * U);
		// Orb arc pivots around RightC. Default: where the right stick WAS (1080p from-right/from-bottom).
		// Optional: bUseOrbScreenAnchor + OrbAnchorXPct/YPct so orbs sit near existing tablet power control without drawing our stick.
		if (T.bUseOrbScreenAnchor)
		{
			L.RightC = FVector2D(CanvasW * FMath::Clamp(T.OrbAnchorXPct, 0.f, 1.f), CanvasH * FMath::Clamp(T.OrbAnchorYPct, 0.f, 1.f));
		}
		else
		{
			L.RightC = FVector2D(CanvasW - T.RightStickCX * U, CanvasH - T.RightStickCY * U);
		}
		L.LeftR = T.LeftStickR * U;
		L.RightR = T.RightStickR * U;
		L.LeftKnobR = T.LeftKnobR * U;
		L.RightKnobR = T.RightKnobR * U;
		L.AbilityR = T.AbilityBubbleR * U;
		L.AwakenR = T.AwakenBubbleR * U;
		L.CancelR = T.CancelRingR * U;
		const float Ang[4] = { T.AbilityAngle0, T.AbilityAngle1, T.AbilityAngle2, T.AbilityAngle3 };
		for (int32 i = 0; i < 4; ++i)
		{
			const float Rad = FMath::DegreesToRadians(Ang[i]);
			L.AbilityC[i] = L.RightC + FVector2D(FMath::Cos(Rad), -FMath::Sin(Rad)) * (T.AbilityArcR * U);
		}
		{
			const float Rad = FMath::DegreesToRadians(T.AwakenAngle);
			L.AwakenC = L.RightC + FVector2D(FMath::Cos(Rad), -FMath::Sin(Rad)) * (T.AwakenR * U);
		}
		return L;
	}

	// ---- Canvas primitives (same triangle-fan style as ZoogiUIStyle / Gem) ----
	static void FillDisc(UCanvas* C, const FVector2D& P, float R, const FLinearColor& Col, int32 Seg = 28)
	{
		if (!C || R <= 0.5f || Col.A <= 0.f) return;
		FCanvasNGonItem It(P, FVector2D(R, R), Seg, Col);
		It.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(It);
	}

	static void StrokeRing(UCanvas* C, const FVector2D& P, float R, float Thick, const FLinearColor& Col, int32 Seg = 48)
	{
		if (!C || R <= 0.5f || Col.A <= 0.f) return;
		for (int32 i = 0; i < Seg; ++i)
		{
			const float A0 = UE_TWO_PI * i / Seg, A1 = UE_TWO_PI * (i + 1) / Seg;
			C->K2_DrawLine(P + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, P + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, Thick, Col);
		}
	}

	static void FillTri(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& D, const FLinearColor& Col)
	{
		if (!C || Col.A <= 0.f) return;
		TArray<FCanvasUVTri> Tris;
		FCanvasUVTri T;
		T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = D;
		T.V0_Color = T.V1_Color = T.V2_Color = Col;
		Tris.Add(T);
		FTexture* Tex = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
		if (!Tex) return;
		FCanvasTriangleItem TI(Tris, Tex);
		TI.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(TI);
	}

	static void DrawLine2(UCanvas* C, const FVector2D& A, const FVector2D& B, float Thick, const FLinearColor& Col)
	{
		if (!C) return;
		C->K2_DrawLine(A, B, Thick, Col);
	}

	/** Glossy electric-blue bubble: outer glow + bright rim + dark core + specular highlight. */
	static void DrawBubble(UCanvas* C, const FVector2D& P, float R, const FLinearColor& Rim, const FLinearColor& Core, float Alpha, float GlowMul, float Time)
	{
		const float Breath = 0.04f * FMath::Sin(Time * 2.2f);
		const float RR = R * (1.f + Breath * 0.15f);
		FillDisc(C, P, RR * 1.42f, FLinearColor(Rim.R, Rim.G, Rim.B, 0.14f * Alpha * GlowMul), 32);
		FillDisc(C, P, RR * 1.18f, FLinearColor(Rim.R, Rim.G, Rim.B, 0.28f * Alpha * GlowMul), 32);
		StrokeRing(C, P, RR * 1.02f, FMath::Max(2.5f, RR * 0.07f), FLinearColor(Rim.R, Rim.G, Rim.B, 0.95f * Alpha), 48);
		FillDisc(C, P, RR * 0.92f, FLinearColor(Core.R, Core.G, Core.B, 0.88f * Alpha), 28);
		FillDisc(C, P, RR * 0.72f, FLinearColor(Core.R * 0.55f, Core.G * 0.6f, Core.B * 0.75f, 0.9f * Alpha), 24);
		// specular (top-left)
		FillDisc(C, P + FVector2D(-RR * 0.28f, -RR * 0.32f), RR * 0.22f, FLinearColor(1.f, 1.f, 1.f, 0.42f * Alpha), 16);
		FillDisc(C, P + FVector2D(-RR * 0.12f, -RR * 0.18f), RR * 0.10f, FLinearColor(1.f, 1.f, 1.f, 0.55f * Alpha), 12);
	}

	/** Center crosshair (revised mockup): bright + on both sticks. */
	static void DrawCrosshair(UCanvas* C, const FVector2D& P, float R, float Alpha, float U)
	{
		const FLinearColor Soft(0.55f, 0.9f, 1.f, 0.35f * Alpha);
		const FLinearColor Hot(0.85f, 0.98f, 1.f, 0.95f * Alpha);
		const float Arm = R * 0.55f;
		const float Thick = FMath::Max(1.4f, 1.6f * U);
		DrawLine2(C, P + FVector2D(-Arm, 0.f), P + FVector2D(Arm, 0.f), Thick, Soft);
		DrawLine2(C, P + FVector2D(0.f, -Arm), P + FVector2D(0.f, Arm), Thick, Soft);
		FillDisc(C, P, FMath::Max(2.5f, 3.2f * U), Hot, 12);
		StrokeRing(C, P, FMath::Max(4.f, 5.5f * U), FMath::Max(1.2f, 1.4f * U), FLinearColor(0.7f, 0.95f, 1.f, 0.7f * Alpha), 20);
	}

	/** Four light-blue D-pad triangles inside the left ring (U/D/L/R pointing outward). */
	static void DrawDPad(UCanvas* C, const FVector2D& P, float RingR, float Alpha, float U)
	{
		const float Mid = RingR * 0.72f;
		const float HalfW = RingR * 0.14f;
		const float Tip = RingR * 0.18f;
		const FLinearColor Tri(0.45f, 0.85f, 1.f, 0.75f * Alpha);
		const FLinearColor TriGlow(0.55f, 0.95f, 1.f, 0.35f * Alpha);
		struct FTri { FVector2D Tip, A, B; };
		const FTri Ts[4] = {
			{ P + FVector2D(0.f, -Mid - Tip * 0.2f), P + FVector2D(-HalfW, -Mid + Tip), P + FVector2D(HalfW, -Mid + Tip) }, // U
			{ P + FVector2D(0.f,  Mid + Tip * 0.2f), P + FVector2D(HalfW,  Mid - Tip), P + FVector2D(-HalfW, Mid - Tip) }, // D
			{ P + FVector2D(-Mid - Tip * 0.2f, 0.f), P + FVector2D(-Mid + Tip, HalfW), P + FVector2D(-Mid + Tip, -HalfW) }, // L
			{ P + FVector2D( Mid + Tip * 0.2f, 0.f), P + FVector2D( Mid - Tip, -HalfW), P + FVector2D( Mid - Tip, HalfW) }, // R
		};
		for (const FTri& T : Ts)
		{
			FillTri(C, T.Tip, T.A, T.B, TriGlow);
			FillTri(C, T.Tip, T.A, T.B, Tri);
		}
		(void)U;
	}

	// ---- Ability icons (white on blue bubble) ----
	static void IconShield(UCanvas* C, const FVector2D& P, float R, float Alpha)
	{
		const FLinearColor W(1.f, 1.f, 1.f, 0.95f * Alpha);
		const float H = R * 0.78f, Ww = R * 0.55f;
		const FVector2D Top(P.X, P.Y - H * 0.55f);
		const FVector2D BL(P.X - Ww, P.Y - H * 0.05f), BR(P.X + Ww, P.Y - H * 0.05f);
		const FVector2D Bot(P.X, P.Y + H * 0.65f);
		FillTri(C, Top, BL, BR, W);
		FillTri(C, BL, Bot, BR, W);
		StrokeRing(C, P + FVector2D(0.f, -H * 0.05f), R * 0.22f, 1.5f, FLinearColor(0.6f, 0.9f, 1.f, 0.5f * Alpha), 16);
	}

	static void IconTarget(UCanvas* C, const FVector2D& P, float R, float Alpha)
	{
		const FLinearColor W(1.f, 1.f, 1.f, 0.95f * Alpha);
		StrokeRing(C, P, R * 0.62f, FMath::Max(1.6f, R * 0.08f), W, 28);
		StrokeRing(C, P, R * 0.36f, FMath::Max(1.4f, R * 0.07f), W, 24);
		FillDisc(C, P, R * 0.12f, W, 12);
		const float Arm = R * 0.78f;
		DrawLine2(C, P + FVector2D(-Arm, 0), P + FVector2D(-R * 0.45f, 0), 1.8f, W);
		DrawLine2(C, P + FVector2D(Arm, 0), P + FVector2D(R * 0.45f, 0), 1.8f, W);
		DrawLine2(C, P + FVector2D(0, -Arm), P + FVector2D(0, -R * 0.45f), 1.8f, W);
		DrawLine2(C, P + FVector2D(0, Arm), P + FVector2D(0, R * 0.45f), 1.8f, W);
	}

	/** Crystalline Z / bolt (revised mockup — replaces sword/dagger). */
	static void IconCrystalZ(UCanvas* C, const FVector2D& P, float R, float Alpha)
	{
		const FLinearColor W(1.f, 1.f, 1.f, 0.96f * Alpha);
		const FLinearColor Glint(0.75f, 0.95f, 1.f, 0.85f * Alpha);
		const float S = R * 0.72f;
		// jagged Z: top bar, diagonal bolt, bottom bar
		const FVector2D T0(P.X - S * 0.55f, P.Y - S * 0.55f), T1(P.X + S * 0.6f, P.Y - S * 0.55f);
		const FVector2D T2(P.X + S * 0.45f, P.Y - S * 0.28f), T3(P.X - S * 0.15f, P.Y - S * 0.28f);
		FillTri(C, T0, T1, T2, W); FillTri(C, T0, T2, T3, W);
		const FVector2D M0(P.X + S * 0.35f, P.Y - S * 0.25f), M1(P.X + S * 0.55f, P.Y - S * 0.05f);
		const FVector2D M2(P.X - S * 0.35f, P.Y + S * 0.25f), M3(P.X - S * 0.55f, P.Y + S * 0.05f);
		FillTri(C, M0, M1, M2, W); FillTri(C, M0, M2, M3, W);
		const FVector2D B0(P.X + S * 0.55f, P.Y + S * 0.55f), B1(P.X - S * 0.6f, P.Y + S * 0.55f);
		const FVector2D B2(P.X - S * 0.45f, P.Y + S * 0.28f), B3(P.X + S * 0.15f, P.Y + S * 0.28f);
		FillTri(C, B0, B1, B2, W); FillTri(C, B0, B2, B3, W);
		FillDisc(C, P + FVector2D(S * 0.15f, S * 0.55f), R * 0.08f, Glint, 10); // bottom glint
	}

	static void IconStar(UCanvas* C, const FVector2D& P, float R, float Alpha)
	{
		const FLinearColor W(1.f, 1.f, 1.f, 0.96f * Alpha);
		const float Outer = R * 0.78f, Inner = R * 0.32f;
		for (int32 i = 0; i < 8; ++i)
		{
			const float A0 = -UE_HALF_PI + i * UE_TWO_PI / 8.f;
			const float A1 = A0 + UE_TWO_PI / 16.f;
			const float A2 = A0 - UE_TWO_PI / 16.f;
			const FVector2D Tip(P.X + FMath::Cos(A0) * Outer, P.Y + FMath::Sin(A0) * Outer);
			const FVector2D L(P.X + FMath::Cos(A2) * Inner, P.Y + FMath::Sin(A2) * Inner);
			const FVector2D Rr(P.X + FMath::Cos(A1) * Inner, P.Y + FMath::Sin(A1) * Inner);
			FillTri(C, Tip, L, Rr, W);
		}
		FillDisc(C, P, R * 0.18f, W, 12);
	}

	static void DrawAbilityIcon(UCanvas* C, int32 Index, const FVector2D& P, float R, float Alpha)
	{
		switch (Index)
		{
		case 0: IconShield(C, P, R, Alpha); break;
		case 1: IconTarget(C, P, R, Alpha); break;
		case 2: IconCrystalZ(C, P, R, Alpha); break;
		default: IconStar(C, P, R, Alpha); break;
		}
	}

	/** Orange/gold Awaken sun with fiery ring (wisps + embers). */
	static void DrawAwakenSun(UCanvas* C, const FVector2D& P, float R, float Alpha, float Time)
	{
		const FLinearColor Fire(1.f, 0.45f, 0.08f, 0.55f * Alpha);
		const FLinearColor Gold(1.f, 0.78f, 0.15f, 0.95f * Alpha);
		const FLinearColor Core(1.f, 0.92f, 0.35f, 0.98f * Alpha);
		// fiery outer wisps
		for (int32 i = 0; i < 10; ++i)
		{
			const float A = Time * 1.7f + i * UE_TWO_PI / 10.f;
			const float J = 0.85f + 0.2f * FMath::Sin(Time * 4.f + i);
			const FVector2D Tip = P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R * 1.35f * J);
			const FVector2D L = P + FVector2D(FMath::Cos(A - 0.22f), FMath::Sin(A - 0.22f)) * (R * 0.95f);
			const FVector2D Rr = P + FVector2D(FMath::Cos(A + 0.22f), FMath::Sin(A + 0.22f)) * (R * 0.95f);
			FillTri(C, Tip, L, Rr, Fire);
		}
		FillDisc(C, P, R * 1.15f, FLinearColor(1.f, 0.4f, 0.05f, 0.25f * Alpha), 28);
		StrokeRing(C, P, R * 1.02f, FMath::Max(2.5f, R * 0.1f), Gold, 36);
		FillDisc(C, P, R * 0.78f, FLinearColor(0.55f, 0.18f, 0.02f, 0.85f * Alpha), 24);
		// sun rays
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = -UE_HALF_PI + i * UE_TWO_PI / 8.f + Time * 0.15f;
			const FVector2D Tip = P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (R * 0.62f);
			const FVector2D L = P + FVector2D(FMath::Cos(A - 0.28f), FMath::Sin(A - 0.28f)) * (R * 0.28f);
			const FVector2D Rr = P + FVector2D(FMath::Cos(A + 0.28f), FMath::Sin(A + 0.28f)) * (R * 0.28f);
			FillTri(C, Tip, L, Rr, Core);
		}
		FillDisc(C, P, R * 0.22f, Core, 14);
		// swirl center
		StrokeRing(C, P, R * 0.14f, 1.6f, FLinearColor(1.f, 0.55f, 0.1f, 0.9f * Alpha), 16);
		FillDisc(C, P + FVector2D(-R * 0.22f, -R * 0.25f), R * 0.12f, FLinearColor(1.f, 1.f, 0.85f, 0.4f * Alpha), 10);
		// ember sparks
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = Time * 2.4f + i * 1.3f;
			const FVector2D E = P + FVector2D(FMath::Cos(A), FMath::Sin(A * 1.3f)) * (R * (1.15f + 0.15f * FMath::Sin(Time * 3.f + i)));
			FillDisc(C, E, FMath::Max(1.5f, R * 0.06f), FLinearColor(1.f, 0.7f, 0.2f, 0.7f * Alpha), 8);
		}
	}

	static void DrawStick(UCanvas* C, const FVector2D& Center, float RingR, float KnobR, const FVector2D& KnobOff,
		bool bDPad, float Alpha, float GlowMul, float Time, float U, bool bDim)
	{
		const float A = Alpha * (bDim ? 0.45f : 1.f);
		const FLinearColor Rim(0.15f, 0.75f, 1.f, 1.f);
		const FLinearColor Core(0.05f, 0.18f, 0.42f, 1.f);
		// soft outer plate
		FillDisc(C, Center, RingR * 1.08f, FLinearColor(0.05f, 0.25f, 0.45f, 0.22f * A), 36);
		StrokeRing(C, Center, RingR, FMath::Max(3.f, RingR * 0.045f), FLinearColor(Rim.R, Rim.G, Rim.B, 0.95f * A * GlowMul), 56);
		StrokeRing(C, Center, RingR * 0.92f, FMath::Max(1.5f, RingR * 0.02f), FLinearColor(0.4f, 0.85f, 1.f, 0.35f * A), 48);
		if (bDPad) DrawDPad(C, Center, RingR, A, U);
		const FVector2D Knob = Center + KnobOff;
		DrawBubble(C, Knob, KnobR, Rim, Core, A, GlowMul, Time);
		DrawCrosshair(C, Knob, KnobR, A, U);
	}

	void Draw(UCanvas* Canvas, float /*UHint*/, float TimeSeconds, AZoogiGameMode* G)
	{
		if (!Canvas || !IsEnabled()) return;
		const float W = Canvas->ClipX, H = Canvas->ClipY;
		if (W < 8.f || H < 8.f) return;

		FState& S = State();
		S.PulseT = TimeSeconds;
		// Your-turn heuristic (don't break turn flow): human active ball + aiming / awaken stage
		bool bYours = true;
		if (G)
		{
			if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming || G->Phase == EZoogiPhase::GameOver) return;
			bYours = G->IsHumanAiming() || (G->Phase == EZoogiPhase::Rolling && G->GetActiveBall() && !G->GetActiveBall()->bCPU);
		}
		S.bYourTurn = bYours;
		S.bStandbyLook = !bYours;
		S.bStandbyPeekHand = !bYours;

		const FTune& T = Tune();
		const FLayout L = ComputeLayout(W, H);
		const float A = T.Alpha;
		const bool bDim = !bYours;

		const bool bSticks = ShouldDrawSticks();

		// Cancel ring + omni sticks only when explicitly enabled (PC mockup / -ZoogiPhoneControlsSticks).
		// Default tablet path: existing aim/power widgets stay; we only overlay power orbs.
		if (bSticks)
		{
			const FLinearColor CR = S.bCancelHot
				? FLinearColor(1.f, 0.32f, 0.22f, 0.85f * A)
				: FLinearColor(1.f, 0.4f, 0.3f, (bDim ? 0.12f : 0.28f) * A);
			StrokeRing(Canvas, L.RightC, L.CancelR, T.CancelRingThick * L.U, CR, 56);

			FVector2D AimOff = S.AimKnobOff;
			if (AimOff.IsNearlyZero()) AimOff = S.AimAxis * (L.LeftR - L.LeftKnobR) * 0.72f;
			FVector2D PowerOff = S.PowerKnobOff;
			if (PowerOff.IsNearlyZero()) PowerOff = FVector2D(0.f, S.PowerPull01 * (L.RightR - L.RightKnobR) * 0.75f);
			DrawStick(Canvas, L.LeftC, L.LeftR, L.LeftKnobR, AimOff, /*bDPad*/true, A, T.GlowMul, TimeSeconds, L.U, bDim);
			DrawStick(Canvas, L.RightC, L.RightR, L.RightKnobR, PowerOff, /*bDPad*/false, A, T.GlowMul, TimeSeconds, L.U, bDim);
		}

		if (T.bDrawAbilityOrbs)
		{
			const FLinearColor AbilityRim(0.2f, 0.8f, 1.f, 1.f);
			const FLinearColor AbilityCore(0.06f, 0.22f, 0.48f, 1.f);
			for (int32 i = 0; i < 4; ++i)
			{
				const bool bOn = S.AbilityEnabled[i];
				const float AA = A * (bOn ? 1.f : 0.35f) * (bDim ? 0.55f : 1.f);
				const float Pop = (S.HoverAbility == i) ? 1.08f : 1.f;
				DrawBubble(Canvas, L.AbilityC[i], L.AbilityR * Pop, AbilityRim, AbilityCore, AA, T.GlowMul, TimeSeconds + i * 0.4f);
				DrawAbilityIcon(Canvas, i, L.AbilityC[i], L.AbilityR * 0.62f * Pop, AA);
			}
		}

		if (T.bDrawAwakenOrb)
		{
			const float AA = A * (S.bAwakenReady ? 1.f : 0.4f) * (bDim ? 0.55f : 1.f);
			const float Pop = S.bHoverAwaken ? 1.1f : 1.f;
			DrawAwakenSun(Canvas, L.AwakenC, L.AwakenR * Pop, AA, TimeSeconds);
		}

		// Standby caption stubs (non-interactive labels; Mobile wires look/peek later)
		if (bDim && GEngine)
		{
			// Intentionally quiet — no big banners; sticks stay visible but dimmed.
		}
	}

	bool HitTest(float CanvasW, float CanvasH, const FVector2D& Pos, FName& OutAction, int32& OutIndex)
	{
		OutAction = NAME_None; OutIndex = 0;
		if (!IsEnabled()) return false;
		const FTune& T = Tune();
		const FLayout L = ComputeLayout(CanvasW, CanvasH);
		const float Pad = T.HitPad * L.U;
		auto Inside = [&](const FVector2D& C, float R) { return FVector2D::DistSquared(Pos, C) <= FMath::Square(R + Pad); };

		// Orbs first (always claimable when drawn) — these are the "power orbs" overlay.
		if (T.bDrawAwakenOrb && Inside(L.AwakenC, L.AwakenR)) { OutAction = FName(TEXT("PhoneAwaken")); OutIndex = 0; return true; }
		if (T.bDrawAbilityOrbs)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				if (Inside(L.AbilityC[i], L.AbilityR)) { OutAction = FName(TEXT("PhoneAbility")); OutIndex = i; return true; }
			}
		}
		// Stick / cancel hit zones only when omni sticks are drawn. When hidden, do NOT eat touches
		// meant for existing tablet aim/power widgets outside orb rects.
		if (ShouldDrawSticks())
		{
			if (Inside(L.RightC, L.RightR)) { OutAction = FName(TEXT("PhonePower")); OutIndex = 0; return true; }
			if (Inside(L.LeftC, L.LeftR)) { OutAction = FName(TEXT("PhoneAim")); OutIndex = 0; return true; }
			const float D = FVector2D::Distance(Pos, L.RightC);
			if (D > L.CancelR && D < L.CancelR + 48.f * L.U)
			{
				OutAction = FName(TEXT("PhoneCancel")); OutIndex = 0; return true;
			}
		}
		return false;
	}

	void NotifyAimAxis(const FVector2D& Axis)
	{
		FState& S = State();
		S.AimAxis = FVector2D(FMath::Clamp(Axis.X, -1.f, 1.f), FMath::Clamp(Axis.Y, -1.f, 1.f));
		S.AimKnobOff = FVector2D::ZeroVector; // Draw derives px from AimAxis; Mobile may overwrite AimKnobOff in px
		if (S.bYourTurn) GAimAxis.Broadcast(S.AimAxis);
		else GStandbyLook.Broadcast(S.AimAxis);
	}

	void NotifyPowerPull01(float Pull01)
	{
		FState& S = State();
		S.PowerPull01 = FMath::Clamp(Pull01, 0.f, 1.f);
		S.PowerKnobOff = FVector2D::ZeroVector; // Draw derives default down-pull; Mobile may overwrite in px
		if (S.bYourTurn) GPowerPull.Broadcast(S.PowerPull01);
		else GStandbyPeek.Broadcast(FVector2D(0.f, S.PowerPull01));
	}

	void NotifyFireRelease(float Pull01, const FVector2D& FlickDelta)
	{
		FState& S = State();
		if (!S.bYourTurn) { S.PowerPull01 = 0.f; S.PowerKnobOff = FVector2D::ZeroVector; return; }
		GFireRelease.Broadcast(FMath::Clamp(Pull01, 0.f, 1.f), FlickDelta);
		S.PowerPull01 = 0.f;
		S.PowerKnobOff = FVector2D::ZeroVector;
		S.bCancelHot = false;
	}

	void NotifyCancelAbort()
	{
		FState& S = State();
		S.bCancelHot = true;
		S.PowerPull01 = 0.f;
		S.PowerKnobOff = FVector2D::ZeroVector;
		if (S.bYourTurn) GCancelAbort.Broadcast();
		S.bCancelHot = false;
	}

	void NotifyAbilityIndex(int32 Index0to3)
	{
		if (Index0to3 < 0 || Index0to3 > 3) return;
		FState& S = State();
		if (!S.bYourTurn || !S.AbilityEnabled[Index0to3]) return;
		GAbilityIndex.Broadcast(Index0to3);
	}

	void NotifyAwakenPressed()
	{
		FState& S = State();
		if (!S.bYourTurn || !S.bAwakenReady) return;
		GAwakenPressed.Broadcast();
	}

	void DrawStickAt(UCanvas* Canvas, const FVector2D& Center, float RingR, float KnobR, const FVector2D& KnobOff, bool bDPad, float Alpha, float TimeSeconds, float U)
	{
		DrawStick(Canvas, Center, RingR, KnobR, KnobOff, bDPad, Alpha, Tune().GlowMul, TimeSeconds, U, false);
	}

	void DrawAwakenAt(UCanvas* Canvas, const FVector2D& Center, float R, float Alpha, float TimeSeconds)
	{
		DrawAwakenSun(Canvas, Center, R, Alpha, TimeSeconds);
	}

	void DrawRingAt(UCanvas* Canvas, const FVector2D& Center, float R, float Thick, const FLinearColor& Color)
	{
		StrokeRing(Canvas, Center, R, Thick, Color, 56);
	}
}
