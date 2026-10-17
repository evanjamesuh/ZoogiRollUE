// aim-arrow: see ZoogiAimArrow.h
#include "ZoogiAimArrow.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SceneView.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"

namespace ZoogiAimArrow
{
	static FTune GTune;
	static bool bTuneLoaded = false;

	static FLinearColor HexColor(const FString& Hex, const FLinearColor& Fallback)
	{
		if (Hex.Len() < 6) return Fallback;
		const FColor C = FColor::FromHex(Hex);
		return FLinearColor(C.R / 255.f, C.G / 255.f, C.B / 255.f, C.A / 255.f); // display-space values, like the rest of the HUD canvas
	}

	const FTune& Tune()
	{
		if (bTuneLoaded) return GTune;
		bTuneLoaded = true;
		if (GConfig)
		{
			const TCHAR* S = TEXT("ZoogiRoll.AimArrow");
			GConfig->GetBool(S, TEXT("bEnabled"), GTune.bEnabled, GGameIni);
			GConfig->GetFloat(S, TEXT("ShaftWidthD"), GTune.ShaftWidthD, GGameIni);
			GConfig->GetFloat(S, TEXT("HeadWidthD"), GTune.HeadWidthD, GGameIni);
			GConfig->GetFloat(S, TEXT("LengthD"), GTune.LengthD, GGameIni);
			GConfig->GetFloat(S, TEXT("HeadLengthD"), GTune.HeadLengthD, GGameIni);
			GConfig->GetFloat(S, TEXT("LiftCm"), GTune.LiftCm, GGameIni);
			FString Hex;
			if (GConfig->GetString(S, TEXT("FillHex"), Hex, GGameIni)) GTune.Fill = HexColor(Hex, GTune.Fill);
			if (GConfig->GetString(S, TEXT("StrokeHex"), Hex, GGameIni)) GTune.Stroke = HexColor(Hex, GTune.Stroke);
			GConfig->GetFloat(S, TEXT("StrokePx"), GTune.StrokePx, GGameIni);
			GConfig->GetBool(S, TEXT("bGlow"), GTune.bGlow, GGameIni);
			GConfig->GetFloat(S, TEXT("GlowPx"), GTune.GlowPx, GGameIni);
			GConfig->GetFloat(S, TEXT("GlowAlpha"), GTune.GlowAlpha, GGameIni);
			GConfig->GetFloat(S, TEXT("GrabShaftPx"), GTune.GrabShaftPx, GGameIni);
			GConfig->GetFloat(S, TEXT("GrabHeadPx"), GTune.GrabHeadPx, GGameIni);
			GConfig->GetBool(S, TEXT("bGrabOnZoogi"), GTune.bGrabOnZoogi, GGameIni);
			GConfig->GetFloat(S, TEXT("RotateStartPx"), GTune.RotateStartPx, GGameIni);
			GConfig->GetFloat(S, TEXT("DefaultAimDistCm"), GTune.DefaultAimDistCm, GGameIni);
			GConfig->GetBool(S, TEXT("bFaceArrow"), GTune.bFaceArrow, GGameIni);
			GConfig->GetFloat(S, TEXT("FaceRate"), GTune.FaceRate, GGameIni);
		}
		const TCHAR* Cmd = FCommandLine::Get();
		if (FParse::Param(Cmd, TEXT("ZoogiNoAimArrow"))) GTune.bEnabled = false;
		if (FParse::Param(Cmd, TEXT("ZoogiAimArrowGlow"))) GTune.bGlow = true;
		if (FParse::Param(Cmd, TEXT("ZoogiAimArrowNoFace"))) GTune.bFaceArrow = false;
		GTune.ShaftWidthD = FMath::Clamp(GTune.ShaftWidthD, 0.05f, 2.f);
		GTune.HeadWidthD = FMath::Clamp(GTune.HeadWidthD, GTune.ShaftWidthD, 3.f);
		GTune.LengthD = FMath::Clamp(GTune.LengthD, 0.5f, 5.f);
		GTune.HeadLengthD = FMath::Clamp(GTune.HeadLengthD, 0.1f, GTune.LengthD * 0.9f);
		UE_LOG(LogZoogi, Log, TEXT("AIMARROW tune: on=%d shaft %.2fD head %.2fD x %.2fD length %.2fD glow=%d faceArrow=%d grab shaft %.0f / head %.0f px onZoogi=%d"),
			GTune.bEnabled ? 1 : 0, GTune.ShaftWidthD, GTune.HeadWidthD, GTune.HeadLengthD, GTune.LengthD, GTune.bGlow ? 1 : 0, GTune.bFaceArrow ? 1 : 0, GTune.GrabShaftPx, GTune.GrabHeadPx, GTune.bGrabOnZoogi ? 1 : 0);
		return GTune;
	}

	void SetTuneForTest(const FTune& T) { GTune = T; bTuneLoaded = true; }

	void Outline(const FVector& C, float R, float YawDeg, const FTune& T, FVector Out[7])
	{
		const float D = 2.f * R;
		const float Rad = FMath::DegreesToRadians(YawDeg);
		const FVector F(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);
		const FVector Rt(-F.Y, F.X, 0.f); // +90 deg yaw (right of the arrow seen from above)
		const FVector Base = FVector(C.X, C.Y, C.Z - R + T.LiftCm) + F * R; // the ball's front base, on the floor
		const float Len = T.LengthD * D, HeadLen = T.HeadLengthD * D;
		const float HW = 0.5f * T.ShaftWidthD * D, HH = 0.5f * T.HeadWidthD * D;
		const FVector ShaftEnd = Base + F * (Len - HeadLen);
		Out[0] = Base - Rt * HW;
		Out[1] = ShaftEnd - Rt * HW;
		Out[2] = ShaftEnd - Rt * HH;
		Out[3] = Base + F * Len;
		Out[4] = ShaftEnd + Rt * HH;
		Out[5] = ShaftEnd + Rt * HW;
		Out[6] = Base + Rt * HW;
	}

	bool ShouldShow(const FVisInput& In)
	{
		return In.bEnabled && In.bHumanAimStep && In.bBallInPlay && !In.bZoomMode && !In.bPinpointArmed;
	}

	FVisInput GatherVis(const AZoogiGameMode* G, const AZoogiPlayerController* PC)
	{
		FVisInput V;
		V.bEnabled = Tune().bEnabled;
		if (!G) return V;
		const AZoogiBall* B = G->GetActiveBall();
		V.bHumanAimStep = G->IsHumanAiming() && G->IsAimStep();
		V.bBallInPlay = B && !B->bOut && !B->bKnockedOut;
		V.bZoomMode = PC && PC->IsZoomMode();
		V.bPinpointArmed = B && G->bPowerArmed && B->Def.PowerKind == EZoogiPower::LockOn;
		return V;
	}

	bool IsShown(const AZoogiGameMode* G, const AZoogiPlayerController* PC) { return ShouldShow(GatherVis(G, PC)); }

	static FVector ActorPoint(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}

	bool CurrentYaw(const AZoogiGameMode* G, float& OutYaw)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B) return false;
		const FVector2D S(B->BallLocation());
		FVector2D D = FVector2D::ZeroVector;
		if (G->HasAim())
		{
			float Fr = 0.f, Imp = 0.f;
			D = G->ComputeShotVelocity(Fr, Imp);
		}
		if (D.IsNearlyZero() && G->TargetLock.IsValid()) D = FVector2D(ActorPoint(G->TargetLock.Get())) - S;
		if (D.IsNearlyZero() && S.Size() > 150.f) D = -S; // toward the arena centre (where the tactical camera looks)
		OutYaw = D.IsNearlyZero() ? B->AimYaw : FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		return true;
	}

	bool FaceYaw(const AZoogiGameMode* G, const AZoogiBall* B, float& OutYaw)
	{
		if (!Tune().bFaceArrow || !G || !B || G->GetActiveBall() != B) return false;
		const UWorld* W = G->GetWorld();
		const AZoogiPlayerController* PC = W ? Cast<AZoogiPlayerController>(W->GetFirstPlayerController()) : nullptr;
		if (!IsShown(G, PC)) return false;
		return CurrentYaw(G, OutYaw);
	}

	static float DistToSeg(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float L2 = AB.SizeSquared();
		const float T = L2 > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + AB * T);
	}

	static bool InConvex(const FVector2D* Pts, int32 N, const FVector2D& P)
	{
		float Sign = 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& A = Pts[i];
			const FVector2D& B = Pts[(i + 1) % N];
			const float Cr = FVector2D::CrossProduct(B - A, P - A);
			if (FMath::Abs(Cr) < KINDA_SMALL_NUMBER) continue;
			if (Sign == 0.f) Sign = FMath::Sign(Cr);
			else if (FMath::Sign(Cr) != Sign) return false;
		}
		return true;
	}

	static float DistToPoly(const FVector2D* Pts, int32 N, const FVector2D& P)
	{
		if (InConvex(Pts, N, P)) return 0.f;
		float Best = TNumericLimits<float>::Max();
		for (int32 i = 0; i < N; ++i) Best = FMath::Min(Best, DistToSeg(P, Pts[i], Pts[(i + 1) % N]));
		return Best;
	}

	bool HitTest(const FVector2D S[7], const FVector2D& BallS, float BallRpx, const FVector2D& P, float Px, const FTune& T)
	{
		if (!T.bGrabOnZoogi && FVector2D::Distance(P, BallS) < BallRpx) return false; // the ball itself keeps the old press
		const FVector2D Shaft[4] = { S[0], S[1], S[5], S[6] };
		const FVector2D Head[3] = { S[2], S[3], S[4] };
		if (DistToPoly(Head, 3, P) <= T.GrabHeadPx * Px) return true;
		if (DistToPoly(Shaft, 4, P) <= T.GrabShaftPx * Px) return true;
		return T.bGrabOnZoogi && FVector2D::Distance(P, BallS) < BallRpx * 1.15f;
	}

	float SlingshotPull(float BackPx, float UPx, float MaxDragDistancePx, float MaxDragForce)
	{
		return FMath::Min(BackPx / (MaxDragDistancePx * UPx), 1.f) * MaxDragForce;
	}

	void FGesture::Begin(float CurYaw, const FVector2D& FingerDir, const FVector2D& PressScreen, float Dist)
	{
		bActive = true; bRotating = false;
		Yaw0 = Yaw = CurYaw;
		Dir0 = FingerDir.IsNearlyZero() ? FVector2D(FMath::Cos(FMath::DegreesToRadians(CurYaw)), FMath::Sin(FMath::DegreesToRadians(CurYaw))) : FingerDir.GetSafeNormal();
		Press = PressScreen;
		AimDist = Dist;
	}

	bool FGesture::Move(const FVector2D& FingerDir, const FVector2D& Screen, float StartPx)
	{
		if (!bActive) return false;
		if (!bRotating && FVector2D::Distance(Screen, Press) > StartPx) bRotating = true;
		if (bRotating && !FingerDir.IsNearlyZero())
		{
			const FVector2D Dn = FingerDir.GetSafeNormal();
			const float Delta = FMath::RadiansToDegrees(FMath::Atan2(FVector2D::CrossProduct(Dir0, Dn), FVector2D::DotProduct(Dir0, Dn)));
			Yaw = FRotator::NormalizeAxis(Yaw0 + Delta);
		}
		return bRotating;
	}

	bool FGesture::End()
	{
		const bool bTap = bActive && !bRotating;
		bActive = bRotating = false;
		return bTap;
	}

	// ------------------------------------------------------------------ drawing

	struct FProj
	{
		const FSceneView* View = nullptr;
		UCanvas* Canvas = nullptr;
		bool P(const FVector& W, FVector2D& Out) const
		{
			if (FVector::DotProduct(W - View->ViewMatrices.GetViewOrigin(), View->GetViewDirection()) < 1.f) return false;
			const FVector S = Canvas->Project(W, false);
			Out = FVector2D(S.X, S.Y);
			return true;
		}
	};

	/** Point W on the floor is hidden by the ball (sphere C, R) seen from the camera O. */
	static bool BallHides(const FVector& O, const FVector& W, const FVector& C, float R)
	{
		const FVector D = W - O;
		const float Len = D.Size();
		if (Len < 1.f) return false;
		const FVector U = D / Len;
		const FVector OC = C - O;
		const float B = FVector::DotProduct(OC, U);
		const float Disc = B * B - (OC.SizeSquared() - R * R);
		if (Disc < 0.f) return false;
		const float T0 = B - FMath::Sqrt(Disc);
		return T0 > 0.f && T0 < Len - 0.5f;
	}

	static void Tri(TArray<FCanvasUVTri>& Out, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Col)
	{
		FCanvasUVTri T;
		T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
		T.V0_Color = T.V1_Color = T.V2_Color = Col;
		Out.Add(T);
	}

	static void Line(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Col, float Thick)
	{
		// extend by half the thickness so the corners close
		const FVector2D Dir = (B - A).GetSafeNormal();
		FCanvasLineItem L(A - Dir * Thick * 0.5f, B + Dir * Thick * 0.5f);
		L.SetColor(Col);
		L.LineThickness = Thick;
		L.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(L);
	}

	void Draw(UCanvas* Canvas, AZoogiGameMode* G, APlayerController* InPC)
	{
		AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(InPC);
		if (!Canvas || !Canvas->SceneView || !G || !IsShown(G, PC)) return;
		AZoogiBall* B = G->GetActiveBall();
		float Yaw = 0.f;
		if (!B || !CurrentYaw(G, Yaw)) return;
		const FTune& T = Tune();
		const float R = ZoogiConst::ZoogiRadius;
		const FVector C = B->BallLocation();
		FVector W[7];
		Outline(C, R, Yaw, T, W);
		FProj Pr; Pr.View = Canvas->SceneView; Pr.Canvas = Canvas;
		const FVector Cam = Pr.View->ViewMatrices.GetViewOrigin();
		const float Px = FMath::Max(FMath::Min(Canvas->ClipX, Canvas->ClipY), 1.f) / 1080.f; // aspect independent

		// shaft: NL lines across its width, each clipped where it leaves the ball (the ball renders in front of the base)
		constexpr int32 NL = 9;
		FVector2D LS[NL], LE[NL];
		bool bLineOk[NL];
		float Tc[NL];
		for (int32 k = 0; k < NL; ++k)
		{
			const float A = (float)k / (NL - 1);
			const FVector WB = FMath::Lerp(W[0], W[6], A), WE = FMath::Lerp(W[1], W[5], A);
			float Lo = 0.f, Hi = 1.f;
			if (!BallHides(Cam, WB, C, R)) Hi = 0.f;
			else if (BallHides(Cam, WE, C, R)) Lo = 1.f;
			else for (int32 i = 0; i < 14; ++i) { const float M = 0.5f * (Lo + Hi); if (BallHides(Cam, FMath::Lerp(WB, WE, M), C, R)) Lo = M; else Hi = M; }
			Tc[k] = Hi;
			bLineOk[k] = Tc[k] < 0.999f && Pr.P(FMath::Lerp(WB, WE, Tc[k]), LS[k]) && Pr.P(WE, LE[k]);
		}
		FVector2D S[7];
		for (int32 i = 0; i < 7; ++i) if (!Pr.P(W[i], S[i])) return; // behind the camera: skip (never partial garbage)

		auto Shape = [&](const FLinearColor& Col, TArray<FCanvasUVTri>& Tris)
		{
			for (int32 k = 0; k + 1 < NL; ++k)
				if (bLineOk[k] && bLineOk[k + 1]) { Tri(Tris, LS[k], LE[k], LE[k + 1], Col); Tri(Tris, LS[k], LE[k + 1], LS[k + 1], Col); }
			Tri(Tris, S[2], S[3], S[4], Col);
		};
		auto Outline2D = [&](const FLinearColor& Col, float Thick)
		{
			if (bLineOk[0]) Line(Canvas, LS[0], S[1], Col, Thick);
			if (bLineOk[NL - 1]) Line(Canvas, LS[NL - 1], S[5], Col, Thick);
			Line(Canvas, S[1], S[2], Col, Thick);
			Line(Canvas, S[2], S[3], Col, Thick);
			Line(Canvas, S[3], S[4], Col, Thick);
			Line(Canvas, S[4], S[5], Col, Thick);
			for (int32 k = 0; k + 1 < NL; ++k) // the base edge only where the ball does not cover it
				if (bLineOk[k] && bLineOk[k + 1] && Tc[k] <= 0.f && Tc[k + 1] <= 0.f) Line(Canvas, LS[k], LS[k + 1], Col, Thick);
		};

		if (T.bGlow)
			for (int32 g = 3; g >= 1; --g)
				Outline2D(FLinearColor(T.Fill.R, T.Fill.G, T.Fill.B, T.GlowAlpha / 3.f), (T.StrokePx + T.GlowPx * g / 3.f) * 2.f * Px);

		TArray<FCanvasUVTri> Tris;
		Shape(T.Fill, Tris);
		if (Tris.Num())
		{
			FCanvasTriangleItem Item(Tris, Canvas->DefaultTexture ? Canvas->DefaultTexture->GetResource() : nullptr);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		Outline2D(T.Stroke, T.StrokePx * Px);
	}
}
