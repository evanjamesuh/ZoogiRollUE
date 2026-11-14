// BATTLE ROYALE HUD (AZoogiHUD, see ZoogiRoyale.h). Canvas only (the arena UMG HUD stays off in this mode).
// Oct 27 (Evan): laid out after the Marble Clash reference shots - LEADERBOARD top left (leader with crown, the players
// around you, your row green with the flag and your score box, ZOOGIS LEFT strip), clock + life dots top centre, menu
// bars top right, HP bar bottom centre, name / HP tags + red chevron over rivals, floating damage numbers (white, yellow
// crit, red taken), red hit marker, red damage vignette, purple YOU DESTROY banner, white-ring JUMP / TRANSFORM buttons.
#include "ZoogiClassicPad.h"
#include "ZoogiShooter.h" // b234
#include "ZoogiHUD.h"
#include "ZoogiRoyale.h"
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.h"
#include "ZoogiPhoneControls.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiArtifacts.h"
#include "ZoogiUIStyle.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Texture2D.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiTune.h"
#include "ZoogiApplePad.h"
#include "ZoogiFront.h"
#include "ZoogiP5Style.h"
#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "UObject/Package.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "ZoogiRoyaleMix.h"
static float MixLen(const FZoogiRoyaleTune& T) { return ZoogiRoyaleMix::ActionRound() ? ZoogiRoyaleMix::ActionSeconds() : T.SurgeFull; } // b149: the action round clock

namespace
{
	// Nov 1: the P5 fonts are composite fonts built from the TTFs (ZoogiP5::Font); the HUD canvas needs a font object, so each one is
	// wrapped in a runtime-cache UFont once (rooted)
	UFont* P5CanvasFont(int32 Which)
	{
		static UFont* Cache[4] = { nullptr, nullptr, nullptr, nullptr };
		Which = FMath::Clamp(Which, 0, 3);
		if (!Cache[Which])
		{
			const FSlateFontInfo Fi = ZoogiP5::Font(Which, 20.f);
			if (const FCompositeFont* CF = Fi.GetCompositeFont())
			{
				UFont* F = NewObject<UFont>(GetTransientPackage());
				F->FontCacheType = EFontCacheType::Runtime;
				F->CompositeFont = *CF;
				F->AddToRoot();
				Cache[Which] = F;
			}
		}
		return Cache[Which];
	}
	void RDisc(UCanvas* C, const FVector2D& P, float R, const FLinearColor& Col, int32 Seg = 32)
	{
		if (!C || R <= 0.5f || Col.A <= 0.f) return;
		FCanvasNGonItem It(P, FVector2D(R, R), Seg, Col);
		It.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(It);
	}
	/** Ring arc from 12 o'clock clockwise over Frac of the circle. */
	void RArc(UCanvas* C, const FVector2D& P, float R, float Thick, float Frac, const FLinearColor& Col, int32 Seg = 64)
	{
		if (!C || Frac <= 0.f || Col.A <= 0.f) return;
		const int32 N = FMath::Max(1, FMath::CeilToInt(Seg * FMath::Clamp(Frac, 0.f, 1.f)));
		for (int32 i = 0; i < N; ++i)
		{
			const float A0 = -UE_HALF_PI + UE_TWO_PI * Frac * i / N, A1 = -UE_HALF_PI + UE_TWO_PI * Frac * (i + 1) / N;
			C->K2_DrawLine(P + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, P + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, Thick, Col);
		}
	}
	/** Arc from angle A0 to A1 (radians, screen space: 0 = right, +pi/2 = down). */
	void RArcAB(UCanvas* C, const FVector2D& P, float R, float Thick, float A0, float A1, const FLinearColor& Col, int32 Seg = 20)
	{
		for (int32 i = 0; i < Seg; ++i)
		{
			const float B0 = FMath::Lerp(A0, A1, (float)i / Seg), B1 = FMath::Lerp(A0, A1, (float)(i + 1) / Seg);
			C->K2_DrawLine(P + FVector2D(FMath::Cos(B0), FMath::Sin(B0)) * R, P + FVector2D(FMath::Cos(B1), FMath::Sin(B1)) * R, Thick, Col);
		}
	}
	/** HUD colours below are picked as sRGB (from the reference shots); the canvas blends in linear space. */
	FLinearColor SRGB(float R, float G, float B, float A = 1.f)
	{
		return FLinearColor(FMath::Pow(FMath::Max(R, 0.f), 2.2f), FMath::Pow(FMath::Max(G, 0.f), 2.2f), FMath::Pow(FMath::Max(B, 0.f), 2.2f), A);
	}
	/** Marble Clash move stick: thin white ring, four white arrow heads, soft white knob. */
	void RStick(UCanvas* C, const FVector2D& P, float R, float KnobR, const FVector2D& KnobOff, float Alpha, float K)
	{
		RDisc(C, P, R, FLinearColor(1.f, 1.f, 1.f, 0.05f * Alpha), 48);
		RArc(C, P, R, 5.f * K, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.7f * Alpha), 80);
		const float AD = R * 0.78f, AS = 26.f * K;
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = k * UE_HALF_PI;
			const FVector2D D(FMath::Cos(A), FMath::Sin(A)), N(-D.Y, D.X);
			const FVector2D Tip = P + D * (AD + AS * 0.5f), Base = P + D * (AD - AS * 0.5f);
			FLinearColor TC(1.f, 1.f, 1.f, 0.55f * Alpha);
			if (!C->DefaultTexture || !C->DefaultTexture->GetResource()) continue;
			TArray<FCanvasUVTri> Tris; FCanvasUVTri Tr;
			Tr.V0_Pos = Tip; Tr.V1_Pos = Base + N * AS * 0.75f; Tr.V2_Pos = Base - N * AS * 0.75f;
			Tr.V0_Color = Tr.V1_Color = Tr.V2_Color = TC; Tris.Add(Tr);
			FCanvasTriangleItem It(Tris, C->DefaultTexture->GetResource()); It.BlendMode = SE_BLEND_Translucent; C->DrawItem(It);
		}
		const FVector2D KP = P + KnobOff;
		RDisc(C, KP, KnobR, FLinearColor(1.f, 1.f, 1.f, 0.32f * Alpha), 40);
		RDisc(C, KP - FVector2D(KnobR * 0.15f, KnobR * 0.2f), KnobR * 0.75f, FLinearColor(1.f, 1.f, 1.f, 0.08f * Alpha), 32);
		RArc(C, KP, KnobR, 2.5f * K, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.45f * Alpha), 48);
	}
	/** Round touch button in the Marble Clash look: clear centre, thick white rim. */
	void RButton(UCanvas* C, const FVector2D& P, float R, float U, float Alpha, bool bLit)
	{
		RDisc(C, P, R, FLinearColor(0.f, 0.f, 0.f, 0.08f * Alpha), 40);
		if (bLit) RDisc(C, P, R * 0.95f, FLinearColor(0.3f, 0.7f, 1.f, 0.2f * Alpha), 40);
		RArc(C, P, R, 7.f * U, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.95f * Alpha), 72);
	}
	void RTri(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& D, const FLinearColor& Col)
	{
		if (!C || Col.A <= 0.f) return;
		if (!C->DefaultTexture || !C->DefaultTexture->GetResource()) return;
		TArray<FCanvasUVTri> Tris;
		FCanvasUVTri Tr;
		Tr.V0_Pos = A; Tr.V1_Pos = B; Tr.V2_Pos = D;
		Tr.V0_Color = Tr.V1_Color = Tr.V2_Color = Col;
		Tris.Add(Tr);
		FCanvasTriangleItem It(Tris, C->DefaultTexture->GetResource());
		It.BlendMode = SE_BLEND_Translucent;
		C->DrawItem(It);
	}
	void RQuad(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& D, const FVector2D& E, const FLinearColor& Col)
	{
		RTri(C, A, B, D, Col);
		RTri(C, A, D, E, Col);
	}
	/** Gold coin (score) icon. */
	void RCoin(UCanvas* C, const FVector2D& P, float R)
	{
		RDisc(C, P, R, SRGB(0.85f, 0.5f, 0.05f, 1.f), 20);
		RDisc(C, P - FVector2D(R * 0.1f, R * 0.1f), R * 0.82f, SRGB(1.f, 0.78f, 0.15f, 1.f), 20);
		RDisc(C, P - FVector2D(R * 0.32f, R * 0.32f), R * 0.25f, SRGB(1.f, 0.95f, 0.6f, 0.9f), 10);
	}
	/** Crown over the leader's rank. */
	void RCrown(UCanvas* C, const FVector2D& P, float S)
	{
		const FLinearColor Gd = SRGB(1.f, 0.78f, 0.15f, 1.f);
		ZoogiUIStyle::CRect(C, P.X - S, P.Y, S * 2.f, S * 0.45f, Gd);
		RTri(C, FVector2D(P.X - S, P.Y + 0.05f * S), FVector2D(P.X - S * 0.45f, P.Y + 0.05f * S), FVector2D(P.X - S * 0.95f, P.Y - S * 0.75f), Gd);
		RTri(C, FVector2D(P.X - S * 0.45f, P.Y + 0.05f * S), FVector2D(P.X + S * 0.45f, P.Y + 0.05f * S), FVector2D(P.X, P.Y - S), Gd);
		RTri(C, FVector2D(P.X + S * 0.45f, P.Y + 0.05f * S), FVector2D(P.X + S, P.Y + 0.05f * S), FVector2D(P.X + S * 0.95f, P.Y - S * 0.75f), Gd);
		RDisc(C, FVector2D(P.X, P.Y - S), S * 0.16f, Gd, 10);
	}
	/** Checkered finish flag (your row). */
	void RFlag(UCanvas* C, const FVector2D& P, float S)
	{
		ZoogiUIStyle::CRect(C, P.X - S * 0.75f, P.Y - S * 0.7f, S * 0.12f, S * 1.5f, SRGB(0.85f, 0.85f, 0.85f, 1.f)); // pole
		const float Q = S * 0.3f;
		for (int32 r = 0; r < 4; ++r)
			for (int32 c = 0; c < 5; ++c)
			{
				const bool bW = (r + c) % 2 == 0;
				const float Wave = FMath::Sin(c * 0.9f) * S * 0.08f;
				ZoogiUIStyle::CRect(C, P.X - S * 0.62f + c * Q, P.Y - S * 0.7f + r * Q + Wave, Q, Q, bW ? SRGB(0.95f, 0.95f, 0.95f, 1.f) : SRGB(0.08f, 0.5f, 0.12f, 1.f));
			}
	}
	/** Little person (players-left strip). */
	void RPerson(UCanvas* C, const FVector2D& P, float S, const FLinearColor& Col)
	{
		RDisc(C, P - FVector2D(0, S * 0.45f), S * 0.32f, Col, 16);
		RArcAB(C, P + FVector2D(0, S * 0.55f), S * 0.3f, S * 0.55f, -UE_PI, 0.f, Col, 12);
		ZoogiUIStyle::CRect(C, P.X - S * 0.55f, P.Y + S * 0.25f, S * 1.1f, S * 0.35f, Col);
	}
	/** Edge vignette (damage): bands fading inward from every screen edge. */
	void RVignette(UCanvas* C, float W, float H, float Depth, const FLinearColor& Col)
	{
		if (Col.A <= 0.01f) return;
		const int32 N = 10;
		for (int32 k = 0; k < N; ++k)
		{
			FLinearColor B = Col;
			B.A = Col.A * 0.1f;
			const float Step = Depth / N;
			ZoogiUIStyle::CRect(C, 0.f, 0.f, W, Step * (N - k), B);
			ZoogiUIStyle::CRect(C, 0.f, H - Step * (N - k), W, Step * (N - k), B);
			ZoogiUIStyle::CRect(C, 0.f, 0.f, Step * (N - k), H, B);
			ZoogiUIStyle::CRect(C, W - Step * (N - k), 0.f, Step * (N - k), H, B);
		}
	}
	FString RoyaleClockText(float S)
	{
		const int32 T = FMath::Max(0, FMath::FloorToInt(S));
		return FString::Printf(TEXT("%d:%02d"), T / 60, T % 60);
	}
	FString Commas(int32 V)
	{
		FString Raw = FString::FromInt(FMath::Abs(V)), Out;
		for (int32 i = 0; i < Raw.Len(); ++i)
		{
			if (i > 0 && (Raw.Len() - i) % 3 == 0) Out.AppendChar(TEXT(','));
			Out.AppendChar(Raw[i]);
		}
		return V < 0 ? TEXT("-") + Out : Out;
	}
	FString RoyaleClock2(float S)
	{
		const int32 T = FMath::Max(0, FMath::CeilToInt(S));
		return FString::Printf(TEXT("%02d:%02d"), T / 60, T % 60);
	}
}

void AZoogiHUD::DrawRoyale(AZoogiGameMode* G)
{
	if (!G || !Canvas) return;
	const FZoogiRoyale& R = G->Royale;
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const ZoogiRoyaleUI::FPad Pad = ZoogiRoyaleUI::Layout(FVector2D(W, H));
	const float SavedU = U;
	U = Pad.U; // this HUD scales with the short side (portrait phones too)
	const float K = U * (1080.f / 923.f); // Marble Clash reference shots were 2000 x 923: K = one reference pixel
	const bool bOver = R.Step == EZoogiRoyaleStep::Over;
	const AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(PlayerOwner);
	const FLinearColor Gold = SRGB(1.f, 0.8f, 0.25f), Red = SRGB(1.f, 0.33f, 0.3f), Soft = SRGB(0.8f, 0.88f, 1.f);
	const FLinearColor Green = SRGB(0.45f, 1.f, 0.35f), Yellow = SRGB(1.f, 0.85f, 0.15f);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// text sized by its on-screen cap height in reference pixels
	const float BigUnit = FMath::Max(1.f, TextSize(TEXT("0"), 1.f, true).Y), SmallUnit = FMath::Max(1.f, TextSize(TEXT("0"), 1.f, false).Y);
	auto Px = [&](float RefPx, bool bBig) { return RefPx * K / (bBig ? BigUnit : SmallUnit) * 1.32f; };
	auto OText = [&](const FString& S, float X, float Y, const FLinearColor& C, float RefPx, bool bCenter, bool bBig, float Outline = 2.f)
	{
		const float Sc = Px(RefPx, bBig);
		float X0 = X;
		if (bCenter) X0 -= TextSize(S, Sc, bBig).X * 0.5f;
		const float O = Outline * K;
		if (O > 0.f) for (const FVector2D& D : { FVector2D(O, 0), FVector2D(-O, 0), FVector2D(0, O), FVector2D(0, -O) }) Text(S, X0 + D.X, Y + D.Y, SRGB(0.02f, 0.03f, 0.08f, 0.85f * C.A), Sc, false, bBig);
		Text(S, X0, Y, C, Sc, false, bBig);
	};
	auto TxtH = [&](float RefPx, bool bBig) { return TextSize(TEXT("0"), Px(RefPx, bBig), bBig).Y; };
	if (ZoogiShooter::On(G)) { const FString Sh = ZoogiShooter::Hud(G); if (!Sh.IsEmpty()) OText(Sh, W * 0.5f, 118.f * K, Gold, 26.f, true, true); } // b234: wave / team score
	// ---- Nov 1 (Evan: "fix the battle royale HUD to match P5 style"): ink black / paper white / Zoogi Roll magenta, slanted collage
	// panels with offset shadows, Anton / Bangers lettering (ZoogiP5::Font). -ZoogiOldRoyaleHUD = the Marble Clash look.
	const bool bP5 = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRoyaleHUD"));
	const FLinearColor Ink = SRGB(0.02f, 0.02f, 0.03f, 1.f), Paper = FLinearColor::White, Mag = ZoogiP5::Accent(), P5Red = SRGB(0.95f, 0.08f, 0.14f);
	auto Slant = [&](float Xs, float Ys, float Wd, float Ht, float Sk, const FLinearColor& Col)
	{ RQuad(Canvas, FVector2D(Xs + Sk, Ys), FVector2D(Xs + Wd + Sk, Ys), FVector2D(Xs + Wd, Ys + Ht), FVector2D(Xs, Ys + Ht), Col); };
	auto P5Panel = [&](float Xs, float Ys, float Wd, float Ht, float Sk, const FLinearColor& Body, const FLinearColor& Rim, float Alpha)
	{
		FLinearColor Sh = Mag; Sh.A = 0.9f * Alpha;
		FLinearColor Rm = Rim; Rm.A *= Alpha;
		FLinearColor Bd = Body; Bd.A *= Alpha;
		Slant(Xs + 7.f * K, Ys + 7.f * K, Wd, Ht, Sk, Sh);
		Slant(Xs - 3.f * K, Ys - 3.f * K, Wd + 6.f * K, Ht + 6.f * K, Sk, Rm);
		Slant(Xs, Ys, Wd, Ht, Sk, Bd);
	};
	auto PFont = [&](float RefPx, int32 Which, float Outline) -> FSlateFontInfo
	{
		const float Sz = FMath::Max(6.f, RefPx * K * 0.75f);
		FSlateFontInfo Fi = ZoogiP5::Font(Which, Sz, Outline * K);
		if (UFont* CF = P5CanvasFont(Which))
		{
			FSlateFontInfo Fo(CF, Sz, FName(TEXT("Regular")));
			Fo.OutlineSettings = Fi.OutlineSettings;
			return Fo;
		}
		return Fi;
	};
	auto PSize = [&](const FString& S, float RefPx, int32 Which) -> FVector2D
	{
		if (!FSlateApplication::IsInitialized()) return FVector2D(S.Len() * RefPx * K * 0.5f, RefPx * K);
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, PFont(RefPx, Which, 0.f));
	};
	// lettering: Which 0 Anton / 3 Bangers, ink outline; Y = top of the text box
	auto PText = [&](const FString& S, float Xs, float Ys, const FLinearColor& C, float RefPx, bool bCenter, int32 Which, float Outline)
	{
		if (S.IsEmpty() || C.A <= 0.01f) return;
		float X0 = Xs;
		if (bCenter) X0 -= PSize(S, RefPx, Which).X * 0.5f;
		FCanvasTextItem It(FVector2D(X0, Ys), FText::FromString(S), PFont(RefPx, Which, Outline), C);
		Canvas->DrawItem(It);
	};
	auto PH_ = [&](float RefPx, int32 Which) { return PSize(TEXT("0"), RefPx, Which).Y; };
	// chunky rounded touch button with readable text (training controls / menu); registers its hit box
	auto TBtn = [&](const FString& Label, float X, float Y, float Wd, float Ht, FName Action, int32 Index, const FLinearColor& Col, bool bLit)
	{
		const float R = FMath::Min(14.f * K, Ht * 0.5f);
		ZoogiUIStyle::CRoundRect(Canvas, X + 2.f * K, Y + 5.f * K, Wd, Ht, R, SRGB(0.f, 0.f, 0.05f, 0.4f));
		ZoogiUIStyle::CRoundRect(Canvas, X, Y, Wd, Ht, R, SRGB(0.02f, 0.05f, 0.14f, 0.95f));
		FLinearColor F = bLit ? Col : SRGB(0.1f, 0.2f, 0.42f, 0.95f);
		ZoogiUIStyle::CRoundRect(Canvas, X + 3.f * K, Y + 3.f * K, Wd - 6.f * K, Ht - 6.f * K, FMath::Max(0.f, R - 3.f * K), F);
		ZoogiUIStyle::CRect(Canvas, X + R, Y + 6.f * K, Wd - 2.f * R, 4.f * K, SRGB(1.f, 1.f, 1.f, 0.25f));
		const float TPx = FMath::Min(Ht / K * 0.42f, 30.f);
		OText(Label, X + Wd * 0.5f, Y + Ht * 0.5f - TxtH(TPx, true) * 0.5f, FLinearColor::White, TPx, true, true, 2.f);
		FZoogiButton Bt; Bt.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + Wd, Y + Ht)); Bt.Action = Action; Bt.Index = Index;
		Buttons.Add(Bt);
	};
	const bool bHumanIn = R.P.IsValidIndex(R.HumanIndex) && R.P[R.HumanIndex].IsIn();
	const FZoogiRoyalePlayer* MeP = R.P.IsValidIndex(R.HumanIndex) ? &R.P[R.HumanIndex] : nullptr;

	// ---- damage vignette (red screen edges) when the human is hit, faint while low on HP
	if (MeP && bHumanIn && !bOver)
	{
		const float Since = Now - MeP->HurtAt;
		float A = Since < 0.7f ? 0.75f * (1.f - Since / 0.7f) : 0.f;
		const float Frac = (float)MeP->HP / FMath::Max(1, T.MaxHP);
		if (Frac < 0.3f && MeP->RespawnIn < 0.f) A = FMath::Max(A, (0.3f - Frac) * 1.4f * (0.75f + 0.25f * FMath::Sin(Time * 6.f)));
		RVignette(Canvas, W, H, FMath::Min(W, H) * 0.16f, SRGB(0.85f, 0.05f, 0.02f, A));
	}

	// ---- rival tags: name, HP bar, score tab; red chevron over the crosshair target
	for (int32 i = 0; i < G->Balls.Num() && i < R.P.Num(); ++i)
	{
		const AZoogiBall* B = G->Balls[i];
		const FZoogiRoyalePlayer& P = R.P[i];
		if (!B || B->bOut || !P.IsIn() || P.RespawnIn >= 0.f || i == R.HumanIndex) continue;
		const FVector Top = B->BallLocation() + FVector(0, 0, P.bRobot ? 175.f : 105.f);
		const FVector S = Project(Top, true);
		if (S.Z <= 0.f || S.X < -80.f || S.X > W + 80.f || S.Y < -80.f || S.Y > H + 80.f) continue;
		const FVector CamL = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		const float Sz = FMath::Clamp(900.f / FMath::Max(1.f, FVector::Dist(CamL, Top)), 0.5f, 1.f);
		const float BW = 150.f * K * Sz, BH = 14.f * K * Sz;
		const float BX = S.X - BW * 0.5f, BY = S.Y;
		if (bP5)
		{
			const float Sq = FMath::Max(Sz, 0.85f); // readable at range
			// P5 tag: name on an ink slant (Bangers), slanted HP bar (paper rim, ink back, red fill), KO count + life pips on the right
			const FString Nm = B->Def.Name.ToUpper();
			const float NH = 26.f * K * Sq, NW2 = PSize(Nm, 24.f * Sq, 3).X + 20.f * K * Sq, Sk = 8.f * K * Sq;
			Slant(S.X - NW2 * 0.5f, BY - NH - 7.f * K * Sq, NW2, NH, Sk, Ink);
			PText(Nm, S.X + 3.f * K * Sq, BY - NH * 0.5f - 7.f * K * Sq - PH_(24.f * Sq, 3) * 0.5f, Paper, 24.f * Sq, true, 3, 0.f);
			const float HF = FMath::Clamp((float)P.HP / FMath::Max(1, T.MaxHP), 0.f, 1.f);
			Slant(BX - 3.f * K * Sq, BY - 3.f * K * Sq, BW + 6.f * K * Sq, BH + 6.f * K * Sq, Sk, Paper);
			Slant(BX, BY, BW, BH, Sk, Ink);
			if (HF > 0.f) Slant(BX, BY, BW * HF, BH, Sk, Now - P.HurtAt < 0.15f ? Paper : P5Red);
			const FString KO = FString::Printf(TEXT("%d"), P.KOs);
			Slant(BX + BW + 10.f * K * Sq, BY - 6.f * K * Sq, 30.f * K * Sq, BH + 12.f * K * Sq, Sk, Mag);
			PText(KO, BX + BW + 27.f * K * Sq, BY + BH * 0.5f - PH_(20.f * Sq, 0) * 0.5f, Paper, 20.f * Sq, true, 0, 0.f);
			for (int32 l = 0; l < P.Lives; ++l)
				Slant(BX + l * 12.f * K * Sq, BY + BH + 5.f * K * Sq, 8.f * K * Sq, 6.f * K * Sq, 3.f * K * Sq, Paper);
			if (MeP && MeP->bRobot && MeP->Lock == i)
			{
				const float CW = 60.f * K * Sq, CH = 30.f * K * Sq, Th = 16.f * K * Sq;
				const float Bob = FMath::Sin(Time * 8.f) * 3.f * K;
				const FVector2D Tip(S.X, BY - NH - 64.f * K * Sq + Bob);
				for (int32 pass = 0; pass < 2; ++pass)
				{
					const float Gr = pass == 0 ? 4.f * K : 0.f;
					const FLinearColor CC = pass == 0 ? Paper : P5Red;
					RQuad(Canvas, Tip + FVector2D(0, -Gr), Tip + FVector2D(CW + Gr, CH - Gr * 0.5f), Tip + FVector2D(CW + Gr, CH + Th + Gr), Tip + FVector2D(0, Th + Gr), CC);
					RQuad(Canvas, Tip + FVector2D(0, -Gr), Tip + FVector2D(0, Th + Gr), Tip + FVector2D(-CW - Gr, CH + Th + Gr), Tip + FVector2D(-CW - Gr, CH - Gr * 0.5f), CC);
				}
			}
			continue;
		}
		// name
		OText(B->Def.Name, S.X, BY - TxtH(17.f * Sz, false) - 5.f * K * Sz, FLinearColor::White, 17.f * Sz, true, false, 1.5f);
		// HP bar: dark back, red fill, white end ticks
		const float Frac = FMath::Clamp((float)P.HP / FMath::Max(1, T.MaxHP), 0.f, 1.f);
		ZoogiUIStyle::CRect(Canvas, BX, BY, BW, BH, SRGB(0.08f, 0.12f, 0.2f, 0.85f));
		ZoogiUIStyle::CRect(Canvas, BX, BY, BW * Frac, BH, SRGB(0.92f, 0.16f, 0.2f, 1.f));
		if (Now - P.HurtAt < 0.15f) ZoogiUIStyle::CRect(Canvas, BX, BY, BW * Frac, BH, SRGB(1.f, 1.f, 1.f, 0.6f));
		ZoogiUIStyle::CRect(Canvas, BX - 2.f * K, BY - 3.f * K * Sz, 3.f * K, BH + 6.f * K * Sz, FLinearColor::White);
		ZoogiUIStyle::CRect(Canvas, BX + BW - 1.f * K, BY - 3.f * K * Sz, 3.f * K, BH + 6.f * K * Sz, FLinearColor::White);
		// score tab under the bar
		{
			const float TW = 46.f * K * Sz, TH = 22.f * K * Sz;
			ZoogiUIStyle::CRect(Canvas, S.X - TW * 0.5f, BY + BH, TW, TH, SRGB(0.08f, 0.12f, 0.2f, 0.75f));
			RCoin(Canvas, FVector2D(S.X - TW * 0.2f, BY + BH + TH * 0.5f), 7.f * K * Sz);
			OText(FString::FromInt(P.KOs), S.X + TW * 0.18f, BY + BH + TH * 0.5f - TxtH(14.f * Sz, true) * 0.5f, FLinearColor::White, 14.f * Sz, true, true, 1.f);
		}
		// lives pips
		for (int32 l = 0; l < P.Lives; ++l)
			RDisc(Canvas, FVector2D(S.X + BW * 0.5f + 12.f * K * Sz + l * 11.f * K * Sz, BY + BH * 0.5f), 4.f * K * Sz, SRGB(1.f, 0.35f, 0.4f, 0.95f), 10);
		// locked target: big red chevron above the name
		if (MeP && MeP->bRobot && MeP->Lock == i)
		{
			const float CW = 60.f * K * Sz, CH = 30.f * K * Sz, Th = 16.f * K * Sz;
			const float Bob = FMath::Sin(Time * 8.f) * 3.f * K;
			const FVector2D Tip(S.X, BY - TxtH(17.f * Sz, false) - 62.f * K * Sz + Bob);
			const FLinearColor CC = SRGB(0.95f, 0.08f, 0.1f, 0.95f);
			RQuad(Canvas, Tip, Tip + FVector2D(CW, CH), Tip + FVector2D(CW, CH + Th), Tip + FVector2D(0, Th), CC);
			RQuad(Canvas, Tip, Tip + FVector2D(0, Th), Tip + FVector2D(-CW, CH + Th), Tip + FVector2D(-CW, CH), CC);
		}
	}

	DrawPopups(G);

	// ---- floating damage numbers: white dealt, yellow crit, red taken (near your Zoogi), grey between CPUs
	for (const FZoogiRoyaleDmg& D : R.Dmg)
	{
		const float Age = Now - D.Born;
		if (Age < 0.f || Age > 1.3f) continue;
		const FVector S3 = Project(D.World, true);
		if (S3.Z <= 0.f) continue;
		FVector2D P(S3.X, S3.Y);
		float Px0 = 40.f, A = Age < 0.75f ? 1.f : FMath::Clamp(1.f - (Age - 0.75f) / 0.55f, 0.f, 1.f);
		FLinearColor C = FLinearColor::White;
		switch (D.Kind)
		{
		case 1: C = Yellow; Px0 = 58.f; break;
		case 2: C = SRGB(0.95f, 0.18f, 0.15f); Px0 = 30.f; P = FVector2D(W * 0.41f, H * 0.80f); break; // stacks left of your Zoogi
		case 3: C = SRGB(0.75f, 0.78f, 0.82f); Px0 = 24.f; A *= 0.7f; break;
		default: break;
		}
		if (D.Kind == 0 && Age > 0.35f) C = FMath::Lerp(C, SRGB(0.7f, 0.74f, 0.8f), FMath::Clamp((Age - 0.35f) / 0.5f, 0.f, 1.f)); // fades to grey like the reference
		const float Pop = 1.f + 0.6f * FMath::Max(0.f, 1.f - Age * 7.f);
		P += D.Jitter * K * (D.Kind == 2 ? 0.4f : 1.f) + FVector2D(D.Kind == 2 ? 0.f : 30.f * Age * K, -70.f * Age * K);
		C.A = A;
		if (bP5) PText(D.Kind == 2 ? FString::Printf(TEXT("-%d"), D.Amount) : FString::FromInt(D.Amount), P.X, P.Y, D.Kind == 1 ? FLinearColor(Mag.R, Mag.G, Mag.B, C.A) : C, Px0 * Pop * 1.15f, true, 0, D.Kind == 3 ? 1.5f : 3.f);
		else OText(D.Kind == 2 ? FString::Printf(TEXT("-%d"), D.Amount) : FString::FromInt(D.Amount), P.X, P.Y, C, Px0 * Pop, true, true, D.Kind == 3 ? 1.f : 2.5f);
	}

	// ---- LEADERBOARD (top left)
	if (bP5)
	{
		const int32 NB = FMath::Min(G->Balls.Num(), R.P.Num());
		TArray<int32> Order;
		for (int32 i = 0; i < NB; ++i) if (G->Balls[i]) Order.Add(i);
		Order.Sort([&R](int32 A, int32 B)
		{
			const FZoogiRoyalePlayer& PA = R.P[A]; const FZoogiRoyalePlayer& PB = R.P[B];
			if (PA.IsIn() != PB.IsIn()) return PA.IsIn();
			if (PA.KOs != PB.KOs) return PA.KOs > PB.KOs;
			if (PA.Lives != PB.Lives) return PA.Lives > PB.Lives;
			return PA.HP > PB.HP;
		});
		const float LX = 30.f * K, LY = 24.f * K;
		// ransom-note title: LEADER (paper on ink) + BOARD (ink on paper)
		P5Panel(LX, LY, 150.f * K, 40.f * K, 10.f * K, Ink, Paper, 1.f);
		PText(TEXT("LEADER"), LX + 85.f * K, LY + 20.f * K - PH_(26.f, 0) * 0.5f, Paper, 26.f, true, 0, 0.f);
		Slant(LX + 160.f * K, LY + 4.f * K, 118.f * K, 36.f * K, 10.f * K, Paper);
		PText(TEXT("BOARD"), LX + 224.f * K, LY + 22.f * K - PH_(24.f, 3) * 0.5f, Ink, 24.f, true, 3, 0.f);
		const int32 Show = FMath::Min(4, Order.Num());
		TArray<int32> Rows;
		for (int32 k = 0; k < Show; ++k) Rows.Add(k);
		const int32 MyRank = Order.IndexOfByKey(R.HumanIndex);
		if (MyRank >= Show && Show > 0) Rows[Show - 1] = MyRank;
		float RY = LY + 58.f * K;
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const int32 Rank = Rows[r], Pi = Order[Rank];
			const AZoogiBall* LB = G->Balls[Pi];
			const FZoogiRoyalePlayer& LP = R.P[Pi];
			const bool bMe = Pi == R.HumanIndex;
			const float RH = (bMe ? 46.f : 38.f) * K, RW = (bMe ? 330.f : 300.f) * K, Sk = 12.f * K, RX = LX + r * 6.f * K;
			const FLinearColor Fg = bMe ? Ink : (LP.IsIn() ? Paper : SRGB(0.45f, 0.45f, 0.5f));
			P5Panel(RX, RY, RW, RH, Sk, bMe ? Paper : Ink, bMe ? Ink : Paper, 1.f);
			Slant(RX + 6.f * K, RY + 5.f * K, 40.f * K, RH - 10.f * K, Sk * 0.7f, bMe ? Mag : (Rank == 0 ? Paper : SRGB(0.2f, 0.2f, 0.23f)));
			const float RkPx = bMe ? 30.f : 26.f, NmPx = bMe ? 26.f : 22.f;
			PText(FString::FromInt(Rank + 1), RX + 30.f * K, RY + RH * 0.5f - PH_(RkPx, 0) * 0.5f, (bMe || Rank != 0) ? Paper : Ink, RkPx, true, 0, 0.f);
			PText(LB->Def.Name.ToUpper(), RX + 62.f * K, RY + RH * 0.5f - PH_(NmPx, 0) * 0.5f, Fg, NmPx, false, 0, 0.f);
			const FString KO = FString::Printf(TEXT("%d KO"), LP.KOs);
			PText(KO, RX + RW - 18.f * K - PSize(KO, 22.f, 3).X, RY + RH * 0.5f - PH_(22.f, 3) * 0.5f, bMe ? Mag : SRGB(1.f, 0.85f, 0.2f), 22.f, false, 3, 0.f);
			if (!LP.IsIn()) Slant(RX + 58.f * K, RY + RH * 0.5f - 2.f * K, RW - 130.f * K, 4.f * K, 0.f, P5Red);
			if (Rank == 0) RCrown(Canvas, FVector2D(RX + 30.f * K, RY - 1.f * K), 10.f * K);
			RY += RH + 9.f * K;
		}
		Slant(LX, RY + 2.f * K, 230.f * K, 34.f * K, 10.f * K, Mag);
		PText(FString::Printf(TEXT("ZOOGIS LEFT  %d"), R.NumIn()), LX + 120.f * K, RY + 19.f * K - PH_(22.f, 3) * 0.5f, Paper, 22.f, true, 3, 1.5f);
	}
	else
	{
		const int32 N = FMath::Min(G->Balls.Num(), R.P.Num());
		TArray<int32> Order;
		for (int32 i = 0; i < N; ++i) if (G->Balls[i]) Order.Add(i);
		Order.Sort([&R](int32 A, int32 B)
		{
			const FZoogiRoyalePlayer& PA = R.P[A]; const FZoogiRoyalePlayer& PB = R.P[B];
			if (PA.IsIn() != PB.IsIn()) return PA.IsIn();
			if (PA.KOs != PB.KOs) return PA.KOs > PB.KOs;
			if (PA.Lives != PB.Lives) return PA.Lives > PB.Lives;
			return PA.HP > PB.HP;
		});
		const int32 MyRank = Order.IndexOfByKey(R.HumanIndex); // -1 spectating
		TArray<int32> Rows; // ranks shown: the leader, the players around you, you
		Rows.Add(0);
		if (MyRank >= 0) { for (int32 k = MyRank - 1; k <= MyRank + 1; ++k) if (k > 0 && k < Order.Num()) Rows.AddUnique(k); }
		for (int32 k = 1; Rows.Num() < FMath::Min(4, Order.Num()) && k < Order.Num(); ++k) Rows.AddUnique(k);
		Rows.Sort();
		const float X0 = 34.f * K, Y0 = 47.f * K, PW = 379.f * K;
		float RowY = Y0 + 10.f * K;
		TArray<float> RowYs;
		for (int32 k : Rows) { RowYs.Add(RowY); RowY += (Order[k] == R.HumanIndex ? 66.f : 56.f) * K; }
		const float StripY = RowY + 4.f * K, PH = StripY + 40.f * K - Y0;
		OText(TEXT("LEADERBOARD"), X0 + PW * 0.5f, 18.f * K, SRGB(0.55f, 0.66f, 0.8f, 0.95f), 20.f, true, true, 0.f);
		ZoogiUIStyle::CRect(Canvas, X0, Y0, PW, PH, SRGB(0.06f, 0.16f, 0.3f, 0.62f));
		for (int32 r = 0; r < Rows.Num(); ++r)
		{
			const int32 Rank = Rows[r], Pi = Order[Rank];
			const AZoogiBall* B = G->Balls[Pi];
			const FZoogiRoyalePlayer& P = R.P[Pi];
			const bool bMe = Pi == R.HumanIndex;
			const float Y = RowYs[r];
			const FLinearColor Box = SRGB(0.13f, 0.15f, 0.2f, 0.88f);
			const FString Name = B->Def.Name;
			if (bMe)
			{
				// your row: green rank + name, the flag, your score box sticking out to the right
				const float RH = 50.f * K;
				ZoogiUIStyle::CRect(Canvas, 15.f * K, Y, 47.f * K, RH, Box);
				OText(FString::FromInt(Rank + 1), 38.5f * K, Y + RH * 0.5f - TxtH(28.f, true) * 0.5f, Green, 28.f, true, true);
				ZoogiUIStyle::CRect(Canvas, 68.f * K, Y, 364.f * K, RH, Box);
				OText(Name.ToUpper(), 76.f * K, Y + RH * 0.5f - TxtH(24.f, false) * 0.5f, P.IsIn() ? Green : ZoogiUIStyle::MutedText, 24.f, false, false, 1.5f);
				RFlag(Canvas, FVector2D(408.f * K, Y + RH * 0.5f), 22.f * K);
				ZoogiUIStyle::CRect(Canvas, 442.f * K, Y - 4.f * K, 100.f * K, RH + 8.f * K, SRGB(0.12f, 0.22f, 0.38f, 0.9f));
				OText(FString::FromInt(P.KOs), 474.f * K, Y + RH * 0.5f - TxtH(30.f, true) * 0.5f, Yellow, 30.f, true, true);
				RCoin(Canvas, FVector2D(517.f * K, Y + RH * 0.5f), 15.f * K);
				if (Rank == 0) RCrown(Canvas, FVector2D(32.f * K, Y - 2.f * K), 13.f * K);
			}
			else
			{
				const bool bTop = Rank == 0;
				const float RH = (bTop ? 38.f : 35.f) * K;
				const float RX = bTop ? 45.f * K : 80.f * K, RW = (bTop ? 50.f : 40.f) * K, NX = bTop ? 100.f * K : 126.f * K, NW = (bTop ? 302.f : 241.f) * K;
				const float NameY = bTop ? Y + 10.f * K : Y + 8.f * K;
				ZoogiUIStyle::CRect(Canvas, RX, bTop ? Y : NameY, RW, bTop ? 51.f * K : RH, Box);
				const float RCY = bTop ? Y + 32.f * K : NameY + RH * 0.5f;
				OText(FString::FromInt(Rank + 1), RX + RW * 0.5f, RCY - TxtH(bTop ? 26.f : 22.f, true) * 0.5f, P.IsIn() ? FLinearColor::White : ZoogiUIStyle::MutedText, bTop ? 26.f : 22.f, true, true);
				if (bTop) RCrown(Canvas, FVector2D(RX + RW * 0.42f, Y + 6.f * K), 12.f * K);
				ZoogiUIStyle::CRect(Canvas, NX, NameY, NW, RH, Box);
				OText(Name, NX + 10.f * K, NameY + RH * 0.5f - TxtH(bTop ? 20.f : 17.f, false) * 0.5f, P.IsIn() ? FLinearColor::White : ZoogiUIStyle::MutedText, bTop ? 20.f : 17.f, false, false, 1.f);
				RCoin(Canvas, FVector2D(NX + NW - 62.f * K, NameY + RH * 0.5f), 10.f * K);
				OText(FString::FromInt(P.KOs), NX + NW - 27.f * K, NameY + RH * 0.5f - TxtH(20.f, true) * 0.5f, Yellow, 20.f, true, true, 1.f);
				if (!P.IsIn()) ZoogiUIStyle::CRect(Canvas, NX + 6.f * K, NameY + RH * 0.5f, NW - 90.f * K, 2.f * K, ZoogiUIStyle::MutedText); // struck out
			}
		}
		// bottom strip: Zoogis still in
		ZoogiUIStyle::CRect(Canvas, X0, StripY, PW, 40.f * K, SRGB(0.03f, 0.09f, 0.18f, 0.85f));
		const float SCY = StripY + 20.f * K;
		OText(TEXT("ZOOGIS LEFT"), X0 + 76.f * K, SCY - TxtH(19.f, true) * 0.5f, Green, 19.f, false, true, 0.f);
		RPerson(Canvas, FVector2D(X0 + 262.f * K, SCY - 2.f * K), 16.f * K, SRGB(0.85f, 0.88f, 0.95f, 1.f));
		OText(FString::FromInt(R.NumIn()), X0 + 292.f * K, SCY - TxtH(19.f, true) * 0.5f, Green, 19.f, false, true, 0.f);
	}

	// ---- top centre: clock box + life dots
	if (bP5)
	{
		const float CX = W * 0.5f;
		const bool bTrain = G->bRoyaleTraining;
		const float Left = FMath::Max(0.f, MixLen(T) - R.FightTime);
		const bool bLate = !bTrain && R.Step == EZoogiRoyaleStep::Fight && Left <= 10.f;
		const FString Clock = (bOver || bTrain) ? RoyaleClock2(R.FightTime) : RoyaleClock2(R.Step == EZoogiRoyaleStep::Countdown ? MixLen(T) : Left);
		P5Panel(CX - 96.f * K, 16.f * K, 192.f * K, 62.f * K, 14.f * K, bLate ? P5Red : Ink, Paper, 1.f);
		FLinearColor CC = Paper;
		if (bLate && Left <= 0.f) CC.A = 0.6f + 0.4f * FMath::Sin(Time * 8.f);
		PText(Clock, CX + 7.f * K, 47.f * K - PH_(46.f, 0) * 0.5f, CC, 46.f, true, 0, 0.f);
		if (bTrain)
		{
			TBtn(G->bRoyaleMarble ? TEXT("CONTROLS: MARBLE") : TEXT("CONTROLS: BRAWL"), CX - 150.f * K, 96.f * K, 300.f * K, 54.f * K, TEXT("TrainControls"), 0,
				G->bRoyaleMarble ? SRGB(1.f, 0.75f, 0.2f) : SRGB(0.3f, 0.75f, 1.f), true);
		}
		else if (MeP)
		{
			// life pips = slanted diamonds: the life in play white with its number, lost lives red with a slash, the rest ink
			const int32 NL = FMath::Clamp(T.Lives, 1, 9);
			const int32 Cur = FMath::Clamp(NL - MeP->Lives, 0, NL - 1);
			const float Sp = 54.f * K, DR = 18.f * K, DY = 104.f * K;
			const float PX0 = CX - Sp * (NL - 1) * 0.5f;
			auto Dia = [&](const FVector2D& Pd, float Rd, const FLinearColor& Col)
			{ RQuad(Canvas, Pd + FVector2D(Rd * 0.25f, -Rd), Pd + FVector2D(Rd * 0.85f, 0.f), Pd + FVector2D(-Rd * 0.25f, Rd), Pd + FVector2D(-Rd * 0.85f, 0.f), Col); };
			for (int32 k = 0; k < NL; ++k)
			{
				const FVector2D Pd(PX0 + Sp * k, DY);
				if (k == Cur && MeP->IsIn())
				{
					Dia(Pd + FVector2D(4.f, 4.f) * K, DR * 1.15f, Mag);
					Dia(Pd, DR * 1.15f, Paper);
					PText(FString::FromInt(k + 1), Pd.X, Pd.Y - PH_(24.f, 0) * 0.5f, Ink, 24.f, true, 0, 0.f);
				}
				else if (k < Cur || !MeP->IsIn())
				{
					Dia(Pd, DR, P5Red);
					Canvas->K2_DrawLine(Pd + FVector2D(-DR * 0.4f, DR * 0.4f), Pd + FVector2D(DR * 0.4f, -DR * 0.4f), 3.f * K, Paper);
				}
				else
				{
					Dia(Pd, DR + 2.5f * K, Paper);
					Dia(Pd, DR, Ink);
				}
			}
		}
	}
	else
	{
		const float CX = W * 0.5f;
		const bool bTrain = G->bRoyaleTraining;
		const float Left = FMath::Max(0.f, MixLen(T) - R.FightTime); // counts down to full surge (Marble Clash round clock look)
		const bool bLate = !bTrain && R.Step == EZoogiRoyaleStep::Fight && Left <= 10.f;
		ZoogiUIStyle::CRect(Canvas, CX - 88.f * K, 18.f * K, 176.f * K, 67.f * K, SRGB(0.06f, 0.24f, 0.45f, 0.82f));
		const FString Clock = (bOver || bTrain) ? RoyaleClock2(R.FightTime) : RoyaleClock2(R.Step == EZoogiRoyaleStep::Countdown ? MixLen(T) : Left);
		FLinearColor CC = bLate ? SRGB(1.f, 0.18f, 0.15f) : FLinearColor::White;
		if (bLate && Left <= 0.f) CC.A = 0.6f + 0.4f * FMath::Sin(Time * 8.f);
		OText(Clock, CX, 51.5f * K - TxtH(40.f, true) * 0.5f, CC, 40.f, true, true, 0.f);
		// TRAINING: a CONTROLS switch under the clock instead of the life dots (tap = BRAWL <-> MARBLE)
		if (bTrain)
		{
			TBtn(G->bRoyaleMarble ? TEXT("CONTROLS: MARBLE") : TEXT("CONTROLS: BRAWL"), CX - 150.f * K, 96.f * K, 300.f * K, 54.f * K, TEXT("TrainControls"), 0,
				G->bRoyaleMarble ? SRGB(1.f, 0.75f, 0.2f) : SRGB(0.3f, 0.75f, 1.f), true);
		}
		// one dot per life: lost lives dim, the current one white with its number, the rest dark
		else if (MeP)
		{
			const int32 NL = FMath::Clamp(T.Lives, 1, 9);
			const int32 Cur = FMath::Clamp(NL - MeP->Lives, 0, NL - 1); // 0-based life in play
			const float Sp = 77.f * K, DR = 22.f * K, DY = 118.f * K;
			const float X0 = CX - Sp * (NL - 1) * 0.5f;
			if (NL > 1) ZoogiUIStyle::CRect(Canvas, X0, DY - 3.f * K, Sp * (NL - 1), 6.f * K, SRGB(0.05f, 0.18f, 0.32f, 0.85f));
			for (int32 k = 0; k < NL; ++k)
			{
				const FVector2D P(X0 + Sp * k, DY);
				if (k == Cur && MeP->IsIn())
				{
					RDisc(Canvas, P, DR + 3.f * K, SRGB(0.55f, 0.6f, 0.68f, 1.f), 28);
					RDisc(Canvas, P, DR, SRGB(0.96f, 0.96f, 0.98f, 1.f), 28);
					OText(FString::FromInt(k + 1), P.X, P.Y - TxtH(24.f, true) * 0.5f, SRGB(0.15f, 0.18f, 0.25f), 24.f, true, true, 0.f);
				}
				else
				{
					RDisc(Canvas, P, DR, k < Cur || !MeP->IsIn() ? SRGB(0.35f, 0.08f, 0.1f, 0.85f) : SRGB(0.07f, 0.26f, 0.44f, 0.92f), 28);
				}
			}
		}
	}

	// ---- Oct 31 b46 (Evan: "royale needs a cog button to return to menu"): a settings cog top right (was three bars) -> back to the menu
	{
		// Nov 1 b52 (Evan: "slightly increase the size ... make the cog the settings button in P5 style"): a straight paper tile with a hard
		// magenta shadow and an ink cog, ~1.3x the old one; tapping opens the P5 pause / settings panel (ZoogiFront::OpenRoyalePause)
		const float CS = 1.3f;
		const FVector2D CC(W - 76.f * K, 78.f * K);
		const float Hb = 40.f * CS * K;
		Slant(CC.X - Hb + 7.f * K, CC.Y - Hb + 7.f * K, Hb * 2.f, Hb * 2.f, 0.f, Mag);
		Slant(CC.X - Hb - 3.f * K, CC.Y - Hb - 3.f * K, Hb * 2.f + 6.f * K, Hb * 2.f + 6.f * K, 0.f, Ink);
		Slant(CC.X - Hb, CC.Y - Hb, Hb * 2.f, Hb * 2.f, 0.f, Paper);
		{
			const FVector2D P = CC;
			RArc(Canvas, P, 17.f * CS * K, 9.f * CS * K, 1.f, Ink, 40);
			for (int32 t = 0; t < 8; ++t)
			{
				const float A = PI / 4.f * t; const FVector2D D(FMath::Cos(A), FMath::Sin(A)), Tn(-D.Y, D.X);
				const FVector2D B0 = P + D * 19.f * CS * K, B1 = P + D * 27.f * CS * K;
				RTri(Canvas, B0 - Tn * 6.f * CS * K, B0 + Tn * 6.f * CS * K, B1 + Tn * 4.5f * CS * K, Ink);
				RTri(Canvas, B0 - Tn * 6.f * CS * K, B1 + Tn * 4.5f * CS * K, B1 - Tn * 4.5f * CS * K, Ink);
			}
		}
		FZoogiButton MB;
		MB.Rect = Pad.Menu;
		MB.Action = G->bRoyaleTraining ? TEXT("TrainMenu") : TEXT("RoyalePause"); // training: its own menu (bots / controls / exit)
		MB.Index = 0;
		Buttons.Add(MB);
	}

	// ---- purple kill banner: YOU DESTROY / DESTROYED BY + the other Zoogi
	if (bP5)
	{
		const float Age = Now - R.KillBanner.Born;
		if (Age >= 0.f && Age < 2.2f && !R.KillBanner.Top.IsEmpty())
		{
			// slams in from the left as a slanted magenta strip over an ink shadow, the other Zoogi on a paper tag below
			const float A = FMath::Clamp((2.2f - Age) / 0.35f, 0.f, 1.f);
			const float In = FMath::Clamp(Age / 0.12f, 0.f, 1.f);
			const float CX = W * 0.5f - (1.f - In) * W * 0.6f, CY = 300.f * K, BW = 560.f * K, BH = 64.f * K;
			FLinearColor Sh = Ink; Sh.A = 0.85f * A;
			FLinearColor Mg = Mag; Mg.A = A;
			FLinearColor Pp = Paper; Pp.A = A;
			Slant(CX - BW * 0.5f + 10.f * K, CY - BH * 0.5f + 10.f * K, BW, BH, 26.f * K, Sh);
			Slant(CX - BW * 0.5f, CY - BH * 0.5f, BW, BH, 26.f * K, Mg);
			PText(R.KillBanner.Top, CX + 13.f * K, CY - PH_(48.f, 0) * 0.5f, Pp, 48.f, true, 0, 2.5f);
			const FString Nm = R.KillBanner.Name.ToUpper();
			const float NW = PSize(Nm, 28.f, 3).X + 40.f * K;
			Slant(CX - NW * 0.5f, CY + BH * 0.5f + 4.f * K, NW, 40.f * K, 12.f * K, Pp);
			PText(Nm, CX + 6.f * K, CY + BH * 0.5f + 24.f * K - PH_(28.f, 3) * 0.5f, SRGB(0.02f, 0.02f, 0.03f, A), 28.f, true, 3, 0.f);
		}
	}
	else
	{
		const float Age = Now - R.KillBanner.Born;
		if (Age >= 0.f && Age < 2.2f && !R.KillBanner.Top.IsEmpty())
		{
			const float A = FMath::Clamp((2.2f - Age) / 0.35f, 0.f, 1.f);
			const float Pop = 1.f + 0.25f * FMath::Max(0.f, 1.f - Age * 6.f);
			const float CX = W * 0.5f, CY = 307.f * K, BW = 400.f * K * Pop, BH = 56.f * K * Pop;
			const int32 Bands = 14;
			for (int32 b = 0; b < Bands; ++b) // purple band, solid in the middle, fading out to both sides
			{
				const float F0 = (float)b / Bands, F1 = (float)(b + 1) / Bands;
				const float Al = 0.9f * A * (1.f - FMath::Square(FMath::Max(0.f, F0 - 0.45f) / 0.55f));
				const FLinearColor BC = SRGB(0.42f, 0.16f, 0.95f, Al);
				ZoogiUIStyle::CRect(Canvas, CX + BW * 0.5f * F0, CY - BH * 0.5f, BW * 0.5f * (F1 - F0) + 1.f, BH, BC);
				ZoogiUIStyle::CRect(Canvas, CX - BW * 0.5f * F1, CY - BH * 0.5f, BW * 0.5f * (F1 - F0) + 1.f, BH, BC);
			}
			OText(R.KillBanner.Top, CX, CY - TxtH(46.f * Pop, true) * 0.5f, SRGB(1.f, 1.f, 1.f, A), 46.f * Pop, true, true, 2.5f);
			OText(R.KillBanner.Name.ToUpper(), CX, CY + BH * 0.5f + 6.f * K, SRGB(1.f, 1.f, 1.f, A), 26.f, true, true, 2.f);
		}
	}

	// ---- controls + crosshair + HP bar (only while the human still plays)
	if (bHumanIn && !bOver)
	{
		const FZoogiRoyalePlayer& Me = *MeP;
		const float Live = R.Step == EZoogiRoyaleStep::Fight ? 1.f : 0.55f;
		// left move circle (floats under the thumb)
		const bool bHeld = PC && PC->IsRoyaleStickHeld();
		const FVector2D Base = bHeld ? PC->RoyaleStickBase : Pad.StickHome;
		const AZoogiBall* MyBall = G->Balls.IsValidIndex(R.HumanIndex) ? G->Balls[R.HumanIndex].Get() : nullptr;
		const bool bApple = ZoogiApplePad::Active(); // Oct 31 b41: Apple on-screen controller replaces the drawn stick / JUMP / TRANSFORM
		if (bApple) {}
		else if (!G->bRoyaleMarble && (ZoogiControls::StickHide() & 1) == 0) RStick(Canvas, Base, Pad.StickR, Pad.KnobR, bHeld ? PC->RoyaleKnobOff : FVector2D::ZeroVector, (bHeld ? 1.f : 0.85f) * Live, K); // b53: hidden by the HIDE LEFT setting
		else
		{
			// TRAINING MARBLE controls: pull back on the left side and let go. While pulling: the pull line under the thumb and
			// a power arrow from the Zoogi in the shot direction (green -> yellow -> red); grey + WAIT while it still rolls.
			const bool bReady = G->RoyaleMarbleReady();
			if (bHeld && MyBall)
			{
				const FVector2D Pull = PC->RoyaleKnobOff;
				const float MaxPull = Pad.StickR * 1.2f, Dead = 22.f * U;
				const float Frac = FMath::Clamp((Pull.Size() - Dead) / (MaxPull - Dead), 0.f, 1.f);
				RDisc(Canvas, Base, 14.f * K, SRGB(1.f, 1.f, 1.f, 0.5f), 20);
				Canvas->K2_DrawLine(Base, Base + Pull, 5.f * K, SRGB(1.f, 1.f, 1.f, 0.55f));
				RDisc(Canvas, Base + Pull, 34.f * K, SRGB(1.f, 1.f, 1.f, 0.3f), 28);
				const FVector S3 = Project(MyBall->BallLocation(), true);
				if (S3.Z > 0.f && Pull.Size() > Dead)
				{
					const FVector2D From(S3.X, S3.Y), Dir = (-Pull).GetSafeNormal();
					const float Len = (90.f + 330.f * Frac) * K;
					const FLinearColor AC = !bReady ? SRGB(0.6f, 0.62f, 0.66f, 0.8f) : (Frac < 0.5f ? FMath::Lerp(SRGB(0.35f, 0.95f, 0.4f), SRGB(1.f, 0.85f, 0.2f), Frac * 2.f) : FMath::Lerp(SRGB(1.f, 0.85f, 0.2f), SRGB(1.f, 0.25f, 0.15f), (Frac - 0.5f) * 2.f));
					const FVector2D Tip = From + Dir * Len, N(-Dir.Y, Dir.X);
					Canvas->K2_DrawLine(From + Dir * 40.f * K, Tip - Dir * 24.f * K, 12.f * K, AC);
					RTri(Canvas, Tip + Dir * 10.f * K, Tip - Dir * 30.f * K + N * 24.f * K, Tip - Dir * 30.f * K - N * 24.f * K, AC);
					OText(bReady ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Frac * 100.f)) : FString(TEXT("WAIT")), Tip.X + N.X * 40.f * K, Tip.Y - 40.f * K, AC, 30.f, true, true, 2.f);
				}
			}
			else
			{
				const float A = (bReady ? 0.85f : 0.4f) * Live;
				RArc(Canvas, Pad.StickHome, Pad.StickR * 0.55f, 4.f * K, 1.f, SRGB(1.f, 1.f, 1.f, 0.5f * A), 64);
				RTri(Canvas, Pad.StickHome + FVector2D(0, Pad.StickR * 0.75f), Pad.StickHome + FVector2D(-18.f * K, Pad.StickR * 0.52f), Pad.StickHome + FVector2D(18.f * K, Pad.StickR * 0.52f), SRGB(1.f, 1.f, 1.f, 0.6f * A));
				OText(bReady ? TEXT("PULL BACK") : TEXT("ROLLING..."), Pad.StickHome.X, Pad.StickHome.Y - 14.f * K, SRGB(1.f, 1.f, 1.f, A), 24.f, true, true, 1.5f);
			}
		}
		const bool bFight = R.Step == EZoogiRoyaleStep::Fight && Me.RespawnIn < 0.f;

		// ---- centre crosshair: four small white ticks; red diagonal hit marker when your rounds land
		{
			const FVector2D C(W * 0.5f, H * 0.5f);
			const bool bLock = Me.bRobot && Me.Lock >= 0 && bFight;
			const FLinearColor XC = bLock ? SRGB(1.f, 1.f, 1.f, 0.95f) : SRGB(1.f, 1.f, 1.f, Me.bRobot ? 0.8f : 0.5f);
			const float Gap = 9.f * K, Len = 13.f * K, Th = 3.f * K;
			Canvas->K2_DrawLine(C + FVector2D(Gap, 0), C + FVector2D(Gap + Len, 0), Th, XC);
			Canvas->K2_DrawLine(C - FVector2D(Gap, 0), C - FVector2D(Gap + Len, 0), Th, XC);
			Canvas->K2_DrawLine(C + FVector2D(0, Gap), C + FVector2D(0, Gap + Len), Th, XC);
			Canvas->K2_DrawLine(C - FVector2D(0, Gap), C - FVector2D(0, Gap + Len), Th, XC);
			const float HitAge = Now - R.HumanHitAt;
			if (HitAge < 0.22f)
			{
				const float A = 1.f - HitAge / 0.22f;
				const float In = 13.f * K, Out = (34.f + 6.f * A) * K;
				for (const FVector2D& Dg : { FVector2D(0.7071f, 0.7071f), FVector2D(0.7071f, -0.7071f), FVector2D(-0.7071f, 0.7071f), FVector2D(-0.7071f, -0.7071f) })
					Canvas->K2_DrawLine(C + Dg * In, C + Dg * Out, 4.5f * K, SRGB(1.f, 0.12f, 0.1f, A));
			}
		}

		// ---- HP bar (bottom centre): green fill, red / orange flash when hit, white end ticks, + icon and the number
		if (bP5)
		{
			const float BW = 440.f * K, BH = 40.f * K, BX = W * 0.5f - BW * 0.5f + 30.f * K, BY = H - 72.f * K, Sk = 16.f * K;
			const float HF = FMath::Clamp((float)Me.HP / FMath::Max(1, T.MaxHP), 0.f, 1.f);
			const float Since = Now - Me.HurtAt;
			FLinearColor Fill = HF < 0.3f ? P5Red : SRGB(0.2f, 0.95f, 0.65f, 1.f);
			if (Since < 0.25f) Fill = Paper;
			P5Panel(BX, BY, BW, BH, Sk, Ink, Paper, 1.f);
			if (HF > 0.f) Slant(BX + 5.f * K, BY + 5.f * K, (BW - 10.f * K) * HF, BH - 10.f * K, Sk * 0.75f, Fill);
			Slant(BX - 78.f * K, BY - 7.f * K, 80.f * K, BH + 14.f * K, Sk, Mag);
			PText(TEXT("HP"), BX - 34.f * K, BY + BH * 0.5f - PH_(32.f, 0) * 0.5f, Paper, 32.f, true, 0, 2.f);
			PText(Commas(Me.HP), BX + 26.f * K, BY + BH * 0.5f - PH_(28.f, 0) * 0.5f, Paper, 28.f, false, 0, 2.5f);
		}
		else
		{
			const float BW = 424.f * K, BH = 46.f * K, BX = W * 0.5f - BW * 0.5f, BY = H - 76.f * K;
			const float Frac = FMath::Clamp((float)Me.HP / FMath::Max(1, T.MaxHP), 0.f, 1.f);
			const float Since = Now - Me.HurtAt;
			FLinearColor Fill = SRGB(0.3f, 0.78f, 0.33f, 1.f);
			if (Since < 0.3f) Fill = SRGB(0.88f, 0.16f, 0.1f, 1.f);
			else if (Since < 0.8f) Fill = FMath::Lerp(SRGB(0.78f, 0.45f, 0.14f, 1.f), Fill, (Since - 0.3f) / 0.5f);
			ZoogiUIStyle::CRect(Canvas, BX, BY, BW, BH, SRGB(0.12f, 0.13f, 0.18f, 0.72f));
			ZoogiUIStyle::CRect(Canvas, BX, BY, BW * Frac, BH, Fill);
			ZoogiUIStyle::CRect(Canvas, BX, BY, BW * Frac, BH * 0.18f, SRGB(1.f, 1.f, 1.f, 0.12f));
			ZoogiUIStyle::CRect(Canvas, BX - 2.f * K, BY - 4.f * K, 3.f * K, BH + 8.f * K, FLinearColor::White);
			ZoogiUIStyle::CRect(Canvas, BX + BW - 1.f * K, BY - 4.f * K, 3.f * K, BH + 8.f * K, FLinearColor::White);
			// green + (outlined)
			const FVector2D PC2(BX + 28.f * K, BY + BH * 0.5f);
			const float A1 = 13.f * K, A2 = 4.5f * K;
			ZoogiUIStyle::CRect(Canvas, PC2.X - A1 - 2.f * K, PC2.Y - A2 - 2.f * K, 2.f * (A1 + 2.f * K), 2.f * (A2 + 2.f * K), SRGB(0.04f, 0.18f, 0.06f, 1.f));
			ZoogiUIStyle::CRect(Canvas, PC2.X - A2 - 2.f * K, PC2.Y - A1 - 2.f * K, 2.f * (A2 + 2.f * K), 2.f * (A1 + 2.f * K), SRGB(0.04f, 0.18f, 0.06f, 1.f));
			ZoogiUIStyle::CRect(Canvas, PC2.X - A1, PC2.Y - A2, 2.f * A1, 2.f * A2, SRGB(0.35f, 0.95f, 0.3f, 1.f));
			ZoogiUIStyle::CRect(Canvas, PC2.X - A2, PC2.Y - A1, 2.f * A2, 2.f * A1, SRGB(0.35f, 0.95f, 0.3f, 1.f));
			OText(Commas(Me.HP), BX + 48.f * K, BY + BH * 0.5f - TxtH(30.f, true) * 0.5f, FLinearColor::White, 30.f, false, true, 2.5f);
		}

		// ---- JUMP: white ring, solid up arrow
		if (!bApple)
		{
			const bool bReady = bFight && Me.JumpCD <= 0.f && MyBall && !MyBall->bOut;
			const float A = (bReady ? 1.f : 0.55f) * Live;
			const FVector2D P = Pad.JumpC;
			const float Rr = Pad.JumpR;
			RButton(Canvas, P, Rr, U, A, false);
			const FLinearColor IC = SRGB(1.f, 1.f, 1.f, 0.97f * A);
			const float HW = Rr * 0.36f;
			RTri(Canvas, P + FVector2D(0, -Rr * 0.62f), P + FVector2D(HW, -Rr * 0.08f), P + FVector2D(-HW, -Rr * 0.08f), IC); // head
			RQuad(Canvas, P + FVector2D(-Rr * 0.17f, -Rr * 0.09f), P + FVector2D(Rr * 0.17f, -Rr * 0.09f), P + FVector2D(Rr * 0.2f, Rr * 0.32f), P + FVector2D(-Rr * 0.2f, Rr * 0.32f), IC); // stem
			ZoogiUIStyle::CRect(Canvas, P.X - Rr * 0.22f, P.Y + Rr * 0.4f, Rr * 0.44f, Rr * 0.12f, IC); // foot bar
			if (Me.JumpCD > 0.f) RArc(Canvas, P, Rr - 3.5f * U, 7.f * U, 1.f - Me.JumpCD / FMath::Max(0.05f, T.JumpCooldown), SRGB(0.3f, 0.75f, 1.f, 0.9f), 64);
		}

		// ---- TRANSFORM: white ring, two chasing arrows; lit while a robot
		if (!bApple)
		{
			const bool bReady = bFight && Me.MorphCD <= 0.f && MyBall && !MyBall->bOut;
			const float A = (bReady ? 1.f : 0.55f) * Live;
			const FVector2D P = Pad.MorphC;
			const float Rr = Pad.MorphR;
			RButton(Canvas, P, Rr, U, A, Me.bRobot);
			const FLinearColor IC = SRGB(1.f, 1.f, 1.f, 0.97f * A);
			const float Ar = Rr * 0.42f, Th = Rr * 0.17f;
			RArcAB(Canvas, P, Ar, Th, -UE_PI * 0.95f, -UE_PI * 0.2f, IC);
			RArcAB(Canvas, P, Ar, Th, UE_PI * 0.05f, UE_PI * 0.8f, IC);
			for (const float E : { -UE_PI * 0.2f, UE_PI * 0.8f })
			{
				const FVector2D Tip = P + FVector2D(FMath::Cos(E), FMath::Sin(E)) * Ar;
				const FVector2D Tan(-FMath::Sin(E), FMath::Cos(E)), Nrm(FMath::Cos(E), FMath::Sin(E));
				RTri(Canvas, Tip + Tan * Rr * 0.26f, Tip + Nrm * Rr * 0.2f, Tip - Nrm * Rr * 0.2f, IC);
			}
			if (Me.MorphCD > 0.f) RArc(Canvas, P, Rr - 3.5f * U, 7.f * U, 1.f - Me.MorphCD / FMath::Max(0.05f, T.MorphCooldown), SRGB(0.3f, 0.75f, 1.f, 0.9f), 64);
		}
		// edge warning: the human's Zoogi is close to the ring line (red pulse at the screen edges)
		if (MyBall)
		{
			const FVector ML = MyBall->BallLocation();
			const FVector2D M2(ML.X, ML.Y);
			if (!MyBall->bOut && G->Arena && !M2.IsNearlyZero() && G->Arena->IsOffCourt(M2 + M2.GetSafeNormal() * (T.EdgeMargin + 260.f)))
			{
				const float A = 0.35f + 0.25f * FMath::Sin(Time * 12.f);
				if (bP5) PText(TEXT("EDGE!"), W * 0.5f, H * 0.55f, FLinearColor(P5Red.R, P5Red.G, P5Red.B, 0.6f + A), 64.f, true, 0, 3.f);
				else OText(TEXT("EDGE!"), W * 0.5f, H * 0.3f, SRGB(1.f, 0.35f, 0.3f, 0.6f + A), 44.f, true, true);
			}
		}
		if (Me.RespawnIn >= 0.f && bP5)
		{
			const FString Rs = FString::Printf(TEXT("RESPAWN IN %.1f"), Me.RespawnIn);
			const float RsW = PSize(Rs, 44.f, 0).X + 60.f * K;
			P5Panel(W * 0.5f - RsW * 0.5f, H * 0.6f, RsW, 64.f * K, 18.f * K, Ink, Paper, 1.f);
			PText(Rs, W * 0.5f + 9.f * K, H * 0.6f + 32.f * K - PH_(44.f, 0) * 0.5f, Paper, 44.f, true, 0, 0.f);
		}
		else if (Me.RespawnIn >= 0.f) OText(FString::Printf(TEXT("RESPAWN IN %.1f"), Me.RespawnIn), W * 0.5f, H * 0.62f, FLinearColor::White, 40.f, true, true);
	}
	else if (!bOver && R.HumanIndex >= 0)
	{
		if (bP5) { Slant(W * 0.5f - 240.f * K, H - 100.f * K, 480.f * K, 50.f * K, 16.f * K, P5Red); PText(TEXT("ELIMINATED  -  SPECTATING"), W * 0.5f + 8.f * K, H - 75.f * K - PH_(30.f, 0) * 0.5f, Paper, 30.f, true, 0, 2.f); }
		else OText(TEXT("ELIMINATED  -  SPECTATING"), W * 0.5f, H - 90.f * K, Red, 30.f, true, true);
	}

	// ---- TRAINING menu (hamburger in training): bots, controls, reset, exit
	if (G->bRoyaleTraining && G->bRoyaleTrainPanel && !bOver)
	{
		ZoogiUIStyle::CRect(Canvas, 0.f, 0.f, W, H, SRGB(0.f, 0.02f, 0.06f, 0.55f));
		const float PW = 560.f * K, BH = 74.f * K, Gap = 16.f * K;
		const int32 NB = 6;
		const float PH = 110.f * K + NB * (BH + Gap);
		const float X = (W - PW) * 0.5f, Y = (H - PH) * 0.5f;
		ZoogiUIStyle::CPanel(Canvas, X, Y, PW, PH, U, SRGB(0.03f, 0.08f, 0.22f, 0.95f), Gold);
		OText(TEXT("TRAINING"), W * 0.5f, Y + 22.f * K, Gold, 40.f, true, true);
		float BY = Y + 96.f * K;
		const float BX = X + 40.f * K, BW = PW - 80.f * K;
		const int32 Bots = G->RoyaleTrainBots;
		TBtn(FString::Printf(TEXT("ADD BOT  (%d / 3)"), Bots), BX, BY, BW, BH, TEXT("TrainBot"), 1, SRGB(0.35f, 0.9f, 0.45f), Bots < 3); BY += BH + Gap;
		TBtn(TEXT("REMOVE BOT"), BX, BY, BW, BH, TEXT("TrainBot"), -1, SRGB(1.f, 0.45f, 0.35f), false); BY += BH + Gap;
		TBtn(G->bRoyaleMarble ? TEXT("SWITCH TO BRAWL CONTROLS") : TEXT("SWITCH TO MARBLE CONTROLS"), BX, BY, BW, BH, TEXT("TrainControls"), 0, SRGB(1.f, 0.75f, 0.2f), true); BY += BH + Gap;
		TBtn(TEXT("RESET"), BX, BY, BW, BH, TEXT("TrainReset"), 0, SRGB(0.3f, 0.6f, 1.f), false); BY += BH + Gap;
		TBtn(TEXT("EXIT TRAINING"), BX, BY, BW, BH, TEXT("TrainExit"), 0, SRGB(1.f, 0.35f, 0.3f), false); BY += BH + Gap;
		TBtn(TEXT("CLOSE"), BX, BY, BW, BH, TEXT("TrainMenu"), 0, SRGB(0.3f, 0.6f, 1.f), false);
	}

	// ---- countdown
	if (R.Step == EZoogiRoyaleStep::Countdown)
	{
		const int32 N = FMath::Max(1, FMath::CeilToInt(T.CountdownSeconds - R.StepTime - 0.3f));
		const float F = FMath::Frac(T.CountdownSeconds - R.StepTime - 0.3f);
		if (bP5)
		{
			// a jagged magenta burst behind a huge paper number, the rule on an ink strip
			const FVector2D C0(W * 0.5f, H * 0.46f);
			const float BR = (130.f + 30.f * F) * K;
			for (int32 sp = 0; sp < 12; ++sp)
			{
				const float A0 = UE_PI * 2.f / 12.f * sp + Time * 0.6f, A1 = A0 + UE_PI * 2.f / 12.f;
				RTri(Canvas, C0, C0 + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * BR * (sp % 2 ? 0.7f : 1.f), C0 + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * BR * (sp % 2 ? 1.f : 0.7f), Mag);
			}
			const FString Ns = FString::FromInt(N);
			PText(Ns, C0.X, C0.Y - PH_(170.f + 50.f * F, 0) * 0.5f, Paper, 170.f + 50.f * F, true, 0, 5.f);
			const FString Rule = ZoogiRoyaleMix::ActionRound() ? FString(TEXT("MOST POINTS WINS THE ROUND")) : FString(TEXT("LAST ZOOGI ROLLING WINS")); // b149
			const float RW = PSize(Rule, 40.f, 3).X + 60.f * K;
			P5Panel(W * 0.5f - RW * 0.5f, H * 0.62f, RW, 56.f * K, 16.f * K, Ink, Paper, 1.f);
			PText(Rule, W * 0.5f + 8.f * K, H * 0.62f + 28.f * K - PH_(40.f, 3) * 0.5f, Paper, 40.f, true, 3, 0.f);
			PText(ZoogiRoyaleMix::ActionRound() ? FString(TEXT("RING-OUT / DESTROY = + POINTS  -  FALL = - POINTS  -  TRANSFORM TO SHOOT")) : FString(TEXT("DESTROY THEM OR KNOCK THEM OFF  -  TRANSFORM TO SHOOT")), W * 0.5f, H * 0.62f + 76.f * K, Paper, 22.f, true, 0, 2.f);
		}
		else
		{
		Text(FString::FromInt(N), W * 0.5f, H * 0.38f, Gold, 3.2f + 1.2f * F, true, true);
		Text(ZoogiRoyaleMix::ActionRound() ? FString(TEXT("MOST POINTS WINS THE ROUND")) : FString(TEXT("LAST ZOOGI ROLLING WINS")), W * 0.5f, H * 0.62f, FLinearColor::White, 0.75f, true, true);
		Text(TEXT("destroy them or knock them off  -  TRANSFORM to shoot"), W * 0.5f, H * 0.62f + 46.f * U, Soft, 0.55f, true, false);
		}
	}

	// ---- big callout (BRAWL!, RING OUT!, AWAKEN!, SURGE)
	if (G->BigAge < 1.8f && !G->BigText.IsEmpty() && !(R.Step == EZoogiRoyaleStep::Countdown && G->BigAge > 1.2f))
	{
		FLinearColor C = G->BigColor + FLinearColor(0.1f, 0.1f, 0.1f, 0.f);
		C.A = FMath::Clamp((1.8f - G->BigAge) / 0.4f, 0.f, 1.f);
		const float Pop = 1.f + 0.35f * FMath::Max(0.f, 1.f - G->BigAge * 5.f);
		TArray<FString> Lines;
		G->BigText.ParseIntoArray(Lines, TEXT("\n"));
		float Y = H * (bOver ? 0.12f : 0.2f);
		for (int32 l = 0; l < Lines.Num(); ++l)
		{
			const float S = (l == 0 ? 1.9f : 0.9f) * Pop;
			if (bP5)
			{
				// ransom slam: line 1 paper Anton on a slanted magenta strip with an ink shadow, line 2 Bangers
				const float Rp = (l == 0 ? 92.f : 40.f) * Pop;
				const int32 Wh = l == 0 ? 0 : 3;
				const FVector2D Ts = PSize(Lines[l], Rp, Wh);
				if (l == 0)
				{
					FLinearColor Sh = Ink; Sh.A = 0.8f * C.A;
					FLinearColor Mg = Mag; Mg.A = 0.92f * C.A;
					Slant(W * 0.5f - Ts.X * 0.5f - 40.f * K + 10.f * K, Y + 10.f * K, Ts.X + 80.f * K, Ts.Y, 24.f * K, Sh);
					Slant(W * 0.5f - Ts.X * 0.5f - 40.f * K, Y, Ts.X + 80.f * K, Ts.Y, 24.f * K, Mg);
				}
				PText(Lines[l], W * 0.5f + (l == 0 ? 12.f * K : 0.f), Y, l == 0 ? FLinearColor(1.f, 1.f, 1.f, C.A) : C, Rp, true, Wh, l == 0 ? 3.f : 2.f);
				Y += Ts.Y + 6.f * K;
			}
			else
			{
			Text(Lines[l], W * 0.5f, Y, C, S, true, true);
			Y += TextSize(Lines[l], S, true).Y;
			}
		}
	}

	// ---- results
	if (bOver && G->PhaseTime > 1.f && bP5)
	{
		// P5 results card: ink slab with paper rim + magenta shadow, ransom title, slanted rows (you = paper), REMATCH / MENU
		const int32 NR = FMath::Min(G->Balls.Num(), R.P.Num());
		const float PW = 620.f * K, Row = 56.f * K, PH = 170.f * K + Row * NR + 100.f * K;
		const float PX = (W - PW) * 0.5f, PY = FMath::Max(H * 0.22f, (H - PH) * 0.5f);
		P5Panel(PX, PY, PW, PH, 26.f * K, Ink, Paper, 0.96f);
		const bool bWon = R.Winner == R.HumanIndex && R.HumanIndex >= 0;
		const FString Title = bWon ? FString(TEXT("VICTORY ROYALE!")) : (G->Balls.IsValidIndex(R.Winner) ? FString(TEXT("BATTLE ROYALE")) : FString(TEXT("DRAW")));
		const FVector2D Tsz = PSize(Title, 58.f, 0);
		Slant(W * 0.5f - Tsz.X * 0.5f - 30.f * K, PY + 22.f * K, Tsz.X + 60.f * K, Tsz.Y, 18.f * K, bWon ? Mag : Paper);
		PText(Title, W * 0.5f + 8.f * K, PY + 22.f * K, bWon ? Paper : Ink, 58.f, true, 0, 0.f);
		PText(FString::Printf(TEXT("%s  -  %s"), *ZoogiUtil::ArenaName(G->MenuArena).ToUpper(), *RoyaleClockText(R.FightTime)), W * 0.5f, PY + 34.f * K + Tsz.Y, SRGB(0.8f, 0.8f, 0.85f), 22.f, true, 3, 0.f);
		TArray<int32> Order;
		for (int32 i = 0; i < NR; ++i) Order.Add(i);
		Order.Sort([&R](int32 A, int32 B) { const int32 PA = R.P[A].Place > 0 ? R.P[A].Place : 99, PB = R.P[B].Place > 0 ? R.P[B].Place : 99; return PA != PB ? PA < PB : R.P[A].KOs > R.P[B].KOs; });
		for (int32 k = 0; k < Order.Num(); ++k)
		{
			const int32 i = Order[k];
			const AZoogiBall* RB = G->Balls[i];
			if (!RB) continue;
			const bool bMe = i == R.HumanIndex;
			const float RY = PY + 150.f * K + Row * k, RX = PX + 40.f * K + k * 5.f * K, RW = PW - 90.f * K;
			Slant(RX, RY, RW, Row - 10.f * K, 12.f * K, bMe ? Paper : SRGB(0.12f, 0.12f, 0.14f));
			const FString Pl = R.P[i].Place > 0 ? FString::Printf(TEXT("#%d"), R.P[i].Place) : FString(TEXT("-"));
			Slant(RX + 6.f * K, RY + 4.f * K, 62.f * K, Row - 18.f * K, 8.f * K, R.P[i].Place == 1 ? Mag : Ink);
			PText(Pl, RX + 40.f * K, RY + (Row - 10.f * K) * 0.5f - PH_(26.f, 0) * 0.5f, Paper, 26.f, true, 0, 0.f);
			PText(RB->Def.Name.ToUpper() + (bMe ? TEXT("  (YOU)") : TEXT("")), RX + 86.f * K, RY + (Row - 10.f * K) * 0.5f - PH_(26.f, 0) * 0.5f, bMe ? Ink : Paper, 26.f, false, 0, 0.f);
			const FString KO = FString::Printf(TEXT("%d KO"), R.P[i].KOs);
			PText(KO, RX + RW - 24.f * K - PSize(KO, 24.f, 3).X, RY + (Row - 10.f * K) * 0.5f - PH_(24.f, 3) * 0.5f, bMe ? Mag : SRGB(1.f, 0.85f, 0.2f), 24.f, false, 3, 0.f);
		}
		const float BY2 = PY + PH - 86.f * K;
		Button(TEXT("REMATCH"), PX + 50.f * K, BY2, 240.f * K, 64.f * K, TEXT("Rematch"), 0, Mag, true);
		Button(TEXT("MENU"), PX + PW - 290.f * K, BY2, 240.f * K, 64.f * K, TEXT("Menu"), 0, FLinearColor(0.9f, 0.9f, 0.92f));
	}
	else if (bOver && G->PhaseTime > 1.f)
	{
		const float PW = 560.f * U, Row = 58.f * U;
		const int32 N = FMath::Min(G->Balls.Num(), R.P.Num());
		const float PH = 150.f * U + Row * N + 96.f * U;
		const float X = (W - PW) * 0.5f, Y = FMath::Max(H * 0.28f, (H - PH) * 0.5f);
		ZoogiUIStyle::CPanel(Canvas, X, Y, PW, PH, U, FLinearColor(0.03f, 0.08f, 0.22f, 0.9f), Gold);
		const FString Title = R.Winner == R.HumanIndex && R.HumanIndex >= 0 ? FString(TEXT("VICTORY ROYALE!")) : (G->Balls.IsValidIndex(R.Winner) ? FString(TEXT("BATTLE ROYALE")) : FString(TEXT("DRAW")));
		Text(Title, W * 0.5f, Y + 18.f * U, Gold, 1.2f, true, true);
		Text(FString::Printf(TEXT("%s  -  %s"), *ZoogiUtil::ArenaName(G->MenuArena), *RoyaleClockText(R.FightTime)), W * 0.5f, Y + 78.f * U, Soft, 0.5f, true, false);
		TArray<int32> Order;
		for (int32 i = 0; i < N; ++i) Order.Add(i);
		Order.Sort([&R](int32 A, int32 B) { const int32 PA = R.P[A].Place > 0 ? R.P[A].Place : 99, PB = R.P[B].Place > 0 ? R.P[B].Place : 99; return PA != PB ? PA < PB : R.P[A].KOs > R.P[B].KOs; });
		for (int32 k = 0; k < Order.Num(); ++k)
		{
			const int32 i = Order[k];
			const AZoogiBall* B = G->Balls[i];
			if (!B) continue;
			const float RY = Y + 120.f * U + Row * k;
			const FLinearColor PCol = ZoogiUtil::PlayerColor(i);
			Text(R.P[i].Place > 0 ? FString::Printf(TEXT("#%d"), R.P[i].Place) : FString(TEXT("-")), X + 40.f * U, RY, R.P[i].Place == 1 ? Gold : FLinearColor::White, 0.8f, false, true);
			RDisc(Canvas, FVector2D(X + 130.f * U, RY + 22.f * U), 14.f * U, PCol, 20);
			Text(B->Def.Name.ToUpper() + (i == R.HumanIndex ? TEXT(" (YOU)") : TEXT("")), X + 158.f * U, RY + 2.f * U, FLinearColor::White, 0.7f, false, true);
			const FString KO = FString::Printf(TEXT("%d KO"), R.P[i].KOs);
			Text(KO, X + PW - 40.f * U - TextSize(KO, 0.6f, true).X, RY + 4.f * U, Gold, 0.6f, false, true);
		}
		const float BY = Y + PH - 84.f * U;
		Button(TEXT("REMATCH"), X + 40.f * U, BY, 220.f * U, 62.f * U, TEXT("Rematch"), 0, Gold, true);
		Button(TEXT("MENU"), X + PW - 260.f * U, BY, 220.f * U, 62.f * U, TEXT("Menu"), 0, FLinearColor(0.3f, 0.6f, 1.f));
	}
	DrawAnime(G);
	U = SavedU;
}

// Oct 27 Phase 3 (Evan's spec, build 9): the royale buttons in classic, same spots and look. TRANSFORM's place = SUPERPOWER
// (star; the Zoogi's Awaken when charged, else its classic power), JUMP's place = REACTION (up arrow; a hop while the shot rolls).
void AZoogiHUD::DrawClassicButtons(AZoogiGameMode* G)
{
	ZoogiClassicPad::Ensure(GetWorld()); // Oct 28: REACTION / SUPERPOWER are touch widgets now (ZoogiClassicPad), drawn + hit there
	if (!G || !Canvas || !G->ClassicButtonsOn()) return;
	const ZoogiRoyaleUI::FPad Pad = ZoogiRoyaleUI::Layout(FVector2D(Canvas->ClipX, Canvas->ClipY));
	const float BU = Pad.U;
	// Oct 27 Frozen Ring: two visible sticks (left roll / right pull-back launch); SUPERPOWER sits above the right stick and
	// REACTION takes the right stick's place while the shot rolls
	const bool bFR = G->FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn();
	FVector2D FrLH, FrRH; float FrLR = Pad.StickR, FrRR = Pad.StickR;
	if (bFR) AZoogiPlayerController::FrozenStickLayout(FVector2D(Canvas->ClipX, Canvas->ClipY), FrLH, FrLR, FrRH, FrRR);
	const FVector2D FrSuper = FrRH - FVector2D(0.f, FrRR + Pad.MorphR + 34.f * BU);
	if (bFR && G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle)
	{
		const AZoogiPlayerController* FPC = Cast<AZoogiPlayerController>(PlayerOwner);
		const float K = BU * (1080.f / 923.f);
		const bool bRoam = G->FrozenRoamActive() && (G->RoamState == EZoogiRoamState::FreeRoam || G->RoamState == EZoogiRoamState::BridgeApproach);
		const bool bPrep = G->FrozenPrepReady();
		const bool bCan = G->FrozenCanLaunch();
		// left stick
		{
			const bool bHeld = FPC && FPC->FrMoveSlot >= 0;
			const FVector2D Base = bHeld ? FPC->FrMoveBase : FrLH;
			const FVector2D Knob = bHeld ? (FPC->FrMoveNow - FPC->FrMoveBase).GetClampedToMaxSize(FrLR * 0.75f) : FVector2D::ZeroVector;
			const float A = ZoogiTune::Get(TEXT("lstick.alpha")) * ((bRoam || bPrep) ? 1.f : 0.45f);
			RStick(Canvas, Base, FrLR, Pad.KnobR * (FrLR / FMath::Max(Pad.StickR, 1.f)), Knob, A, K);
			const TCHAR* LL = bRoam ? TEXT("ROLL") : (bPrep ? TEXT("TURN") : nullptr);
			if (LL) Text(LL, Base.X, Base.Y + FrLR + 8.f * BU, FLinearColor(1.f, 1.f, 1.f, 0.9f * A), 0.85f, true, true);
		}
		// right stick: ring, knob following the pull, power arc
		{
			const bool bPull = FPC && FPC->FrPullSlot >= 0;
			const float A = ZoogiTune::Get(TEXT("rstick.alpha")) * (bCan ? 1.f : 0.3f);
			const FVector2D Knob = bPull ? (FPC->FrPullNow - FPC->FrPullBase).GetClampedToMaxSize(FrRR * 0.8f) : FVector2D::ZeroVector;
			RDisc(Canvas, FrRH, FrRR, FLinearColor(1.f, 1.f, 1.f, 0.05f * A), 48);
			RArc(Canvas, FrRH, FrRR, 5.f * K, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.7f * A), 80);
			const float KR = Pad.KnobR * (FrRR / FMath::Max(Pad.StickR, 1.f));
			RDisc(Canvas, FrRH + Knob, KR, FLinearColor(1.f, 0.9f, 0.5f, 0.35f * A), 40);
			RArc(Canvas, FrRH + Knob, KR, 2.5f * K, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.5f * A), 48);
			if (bPull && FPC->FrPullFrac > 0.f)
				RArc(Canvas, FrRH, FrRR + 10.f * K, 9.f * K, FPC->FrPullFrac, FLinearColor::LerpUsingHSV(SRGB(0.4f, 0.9f, 1.f), SRGB(1.f, 0.35f, 0.2f), FPC->FrPullFrac), 80);
			const TCHAR* RL = !bCan ? (G->FrozenRoamActive() ? TEXT("ON A BRIDGE") : nullptr) : (bPull ? (FPC->FrPullFrac > 0.f ? TEXT("LET GO!") : TEXT("PULL BACK")) : TEXT("PULL BACK"));
			if (RL) Text(RL, FrRH.X, FrRH.Y + FrRR + 8.f * BU, FLinearColor(1.f, 1.f, 1.f, 0.9f * FMath::Max(A, 0.6f)), 0.85f, true, true);
		}
	}
	// capture runs (-ZoogiClassicButtonsTest): press REACTION a moment into every human roll
	static const bool bBtnTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClassicButtonsTest"));
	if (bBtnTest && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.3f && G->ClassicReactionReady()) G->MenuAction(TEXT("ClassicReaction"), 0);
	// ... and SUPERPOWER (Wolf Pack) a little later in the roll
	if (bBtnTest && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.5f && G->ClassicSuperpowerReady()) G->MenuAction(TEXT("ClassicPower"), 0);
	// ... and shoot like a phone player: free-aim drag toward the arena centre and release (launch on release, no hold)
	if (bBtnTest && G->IsHumanAiming() && G->IsAimStep() && G->StepTime > 1.2f)
	{
		if (const AZoogiBall* TB = G->GetActiveBall())
		{
			if (G->ClassicAimBar() && !(G->FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn()))
			{
				// Oct 28: lock a rival / orb, then alternate shots between a bar preset (DRIVE) and a drag-back slingshot release
				const TArray<AActor*> Cand = G->TargetCandidates();
				if (Cand.Num()) G->LockOn(Cand[0], false); else G->SetFreeAim(TB->BallLocation() * 0.25f);
				static int32 TestShotN = 0;
				if (TestShotN++ % 2 == 0) G->MenuAction(TEXT("AimPreset"), 2);
				else G->HumanDrag(0.7f, true); // drag-back release at 70%
			}
			else if (!G->FrozenRoamOn()) { G->SetFreeAim(TB->BallLocation() * 0.25f); G->HumanDrag(0.6f, true); }
			else if (!G->FrozenNeedsEntry(G->ActivePlayer)) { const FVector2D D = -FVector2D(TB->BallLocation()); G->FrozenHumanLaunch(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.6f); } // right-stick launch in the arena
		}
	}
	auto Label = [&](const TCHAR* S, const FVector2D& P, float Rr, float A)
	{
		Text(S, P.X, P.Y + Rr + 6.f * BU, FLinearColor(1.f, 1.f, 1.f, 0.95f * A), 0.85f, true, true);
	};
	auto Hit = [&](const FVector2D& P, float Rr, FName Action)
	{
		FZoogiButton Bt; Bt.Rect = FBox2D(P - FVector2D(Rr * 1.4f, Rr * 1.4f), P + FVector2D(Rr * 1.4f, Rr * 1.4f)); Bt.Action = Action; Bt.Index = 0;
		Buttons.Add(Bt);
	};
	// ---- SUPERPOWER (TRANSFORM's spot): white ring, gold star; lit when ready. Oct 28: only for the Frozen bridge flow now -
	// otherwise ZoogiClassicPad draws REACTION / SUPERPOWER as real touch buttons (fire on touch-down, generous box)
	if (bFR)
	{
		const bool bReady = G->ClassicSuperpowerReady();
		const float A = bReady ? 1.f : 0.5f;
		// Oct 28 (Evan): straight underneath REACTION (same column), landscape and portrait
		const FVector2D P = bFR ? FrSuper : FVector2D(Pad.JumpC.X, Pad.JumpC.Y + Pad.JumpR + Pad.MorphR + 76.f * BU);
		const float Rr = Pad.MorphR;
		RButton(Canvas, P, Rr, BU, A, bReady);
		const FLinearColor IC = bReady ? SRGB(1.f, 0.85f, 0.25f, 0.98f) : SRGB(1.f, 1.f, 1.f, 0.8f * A);
		const float Ro = Rr * 0.5f, Ri = Rr * 0.22f;
		for (int32 k = 0; k < 10; ++k)
		{
			const float A0 = -UE_HALF_PI + UE_TWO_PI * k / 10.f, A1 = -UE_HALF_PI + UE_TWO_PI * (k + 1) / 10.f;
			const float R0 = (k % 2 == 0) ? Ro : Ri, R1 = (k % 2 == 0) ? Ri : Ro;
			RTri(Canvas, P, P + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R0, P + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R1, IC);
		}
		Label(TEXT("SUPERPOWER"), P, Rr, A);
		// Oct 28: Wolf Pack charges left (a gold star orb adds one)
		if (G->WolfPackOnly()) if (const AZoogiBall* AB = G->GetActiveBall()) Text(FString::Printf(TEXT("x%d"), AB->Charges), P.X + Rr * 0.85f, P.Y - Rr * 1.05f, AB->Charges > 0 ? SRGB(1.f, 0.85f, 0.25f, 0.98f) : SRGB(1.f, 1.f, 1.f, 0.6f), 0.8f, true, true);
		Hit(P, Rr, TEXT("ClassicPower"));
	}
	// ---- REACTION (JUMP's spot): white ring, solid up arrow; usable while your shot rolls
	if (bFR && G->Phase == EZoogiPhase::Rolling)
	{
		const bool bReady = G->ClassicReactionReady();
		const float A = bReady ? 1.f : 0.5f;
		const FVector2D P = bFR ? FrRH : Pad.JumpC;
		const float Rr = Pad.JumpR;
		RButton(Canvas, P, Rr, BU, A, bReady);
		const FLinearColor IC = SRGB(1.f, 1.f, 1.f, 0.97f * A);
		const float HW = Rr * 0.36f;
		RTri(Canvas, P + FVector2D(0, -Rr * 0.62f), P + FVector2D(HW, -Rr * 0.08f), P + FVector2D(-HW, -Rr * 0.08f), IC);
		RQuad(Canvas, P + FVector2D(-Rr * 0.17f, -Rr * 0.09f), P + FVector2D(Rr * 0.17f, -Rr * 0.09f), P + FVector2D(Rr * 0.2f, Rr * 0.32f), P + FVector2D(-Rr * 0.2f, Rr * 0.32f), IC);
		ZoogiUIStyle::CRect(Canvas, P.X - Rr * 0.22f, P.Y + Rr * 0.4f, Rr * 0.44f, Rr * 0.12f, IC);
		Label(TEXT("REACTION"), P, Rr, A);
		Hit(P, Rr, TEXT("ClassicReaction"));
	}
}
