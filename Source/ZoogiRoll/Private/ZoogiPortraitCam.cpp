#include "ZoogiPortraitCam.h"
#include "ZoogiFramedLayout.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPortrait(TEXT("zoogi.Cam.Portrait"), 1, TEXT("Portrait camera framing on narrow (aspect < 1) screens: capped vertical FOV, width-keeping boom, HUD-safe band."));
	TAutoConsoleVariable<float> CVarSafeTop(TEXT("zoogi.Cam.SafeTop"), 0.05f, TEXT("Portrait: top screen fraction kept clear of the play focus (portrait = HUD-hidden play: show-HUD / turn-score pill / ZOOM controls; 0.12 with a full HUD)."));
	TAutoConsoleVariable<float> CVarSafeTopMin(TEXT("zoogi.Cam.SafeTopMin"), 0.05f, TEXT("Portrait: the top band never shrinks below this (HUD-hidden controls live in the top ~5%)."));
	int32 GModeOverride = -1;
	TAutoConsoleVariable<int32> CVarAim(TEXT("zoogi.Cam.PortraitAim"), 1, TEXT("Portrait aim / rest framing from behind the shooter looking up the arena (0 = width-keeping pull-back only)."));
	TAutoConsoleVariable<float> CVarAimPitch(TEXT("zoogi.Cam.PortraitAimPitch"), 48.f, TEXT("Portrait aim / rest framing: camera pitch (deg)."));
	TAutoConsoleVariable<float> CVarAimAhead(TEXT("zoogi.Cam.PortraitAimAhead"), 0.5f, TEXT("Portrait aim framing: look point from the shooter toward the aim point / arena centre (fraction)."));
	TAutoConsoleVariable<float> CVarRestAhead(TEXT("zoogi.Cam.PortraitRestAhead"), 0.4f, TEXT("Portrait Draw / Awaken framing: look point from the shooter toward the arena centre (fraction)."));
	TAutoConsoleVariable<float> CVarAimWidth(TEXT("zoogi.Cam.PortraitAimWidth"), 1.4f, TEXT("Portrait aim framing: ground width at the look point, times the play radius."));
	TAutoConsoleVariable<float> CVarShooterMargin(TEXT("zoogi.Cam.PortraitShooterMargin"), 0.06f, TEXT("Portrait aim framing: the shooter stays this far (normalised) inside the safe band."));
	TAutoConsoleVariable<float> CVarSafeBottom(TEXT("zoogi.Cam.SafeBottom"), 0.10f, TEXT("Portrait: bottom screen fraction kept clear of the play focus (HUD-hidden play: thumb / drag-aim zone; 0.22 with the card hand up)."));
	TAutoConsoleVariable<float> CVarVFovMax(TEXT("zoogi.Cam.PortraitVFovMax"), 78.f, TEXT("Portrait: vertical FOV cap (deg)."));
	TAutoConsoleVariable<float> CVarBoomMax(TEXT("zoogi.Cam.PortraitBoomMax"), 2.6f, TEXT("Portrait: cap on the width-keeping boom pull-back."));
	TAutoConsoleVariable<float> CVarCourtMargin(TEXT("zoogi.Cam.CourtMargin"), 0.94f, TEXT("Portrait: whole-court views keep the court inside this fraction of the half-width."));
}

namespace ZoogiPortrait
{
	bool AimFraming() { return CVarAim.GetValueOnGameThread() != 0; }
	float AimPitch() { return FMath::Clamp(CVarAimPitch.GetValueOnGameThread(), 20.f, 75.f); }
	float AimAhead() { return FMath::Clamp(CVarAimAhead.GetValueOnGameThread(), 0.f, 1.f); }
	float RestAhead() { return FMath::Clamp(CVarRestAhead.GetValueOnGameThread(), 0.f, 1.f); }
	float AimWidth() { return FMath::Clamp(CVarAimWidth.GetValueOnGameThread(), 0.f, 3.f); }
	float ShooterMargin() { return FMath::Clamp(CVarShooterMargin.GetValueOnGameThread(), 0.f, 0.3f); }
	bool Enabled() { return CVarPortrait.GetValueOnGameThread() != 0; }
	float SafeTopMin() { return FMath::Clamp(CVarSafeTopMin.GetValueOnGameThread(), 0.f, 0.4f); }
	float SafeTop() { return FMath::Clamp(CVarSafeTop.GetValueOnGameThread(), SafeTopMin(), 0.4f); }
	float SafeBottom() { return FMath::Clamp(CVarSafeBottom.GetValueOnGameThread(), 0.f, 0.45f); }
	float VFovMax() { return FMath::Clamp(CVarVFovMax.GetValueOnGameThread(), 40.f, 120.f); }
	float BoomMax() { return FMath::Clamp(CVarBoomMax.GetValueOnGameThread(), 1.f, 4.f); }
	float CourtMargin() { return FMath::Clamp(CVarCourtMargin.GetValueOnGameThread(), 0.5f, 1.f); }

	void SetModeOverride(int32 Mode) { GModeOverride = FMath::Clamp(Mode, -1, 1); }
	int32 GetModeOverride() { return GModeOverride; }
	bool IsPortrait(float Aspect)
	{
		if (Aspect <= 0.f || GModeOverride == 0) return false;
		return GModeOverride == 1 || (Enabled() && Aspect < 1.f);
	}

	float ViewAspect(const APlayerController* PC) { return ZoogiFramed::ViewAspect(PC); }

	void Band(const APlayerController* PC, float Aspect, float& OutTop, float& OutBottom)
	{
		const bool bOff = !IsPortrait(Aspect);
		const bool bFramed = PC && ZoogiFramed::IsActive(PC->GetWorld());
		// framed HUD: the arena window already excludes the HUD, but the top HUD-hidden controls band still applies
		OutTop = bOff ? 0.f : (bFramed ? SafeTopMin() : SafeTop());
		OutBottom = (bOff || bFramed) ? 0.f : SafeBottom();
	}

	void AdaptFov(float LandscapeHFov, float Aspect, float& OutHFov, float& OutBoomMul)
	{
		OutHFov = LandscapeHFov; OutBoomMul = 1.f;
		if (!IsPortrait(Aspect)) return;
		const float TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(LandscapeHFov, 5.f, 170.f) * 0.5f));
		const float TanVCap = FMath::Tan(FMath::DegreesToRadians(VFovMax() * 0.5f));
		const float TanHCap = TanVCap * Aspect;
		if (TanH <= TanHCap) return;
		OutHFov = 2.f * FMath::RadiansToDegrees(FMath::Atan(TanHCap));
		OutBoomMul = FMath::Min(TanH / TanHCap, BoomMax());
	}

	float BandPitch(float HFov, float Aspect, float Top, float Bottom)
	{
		// band in normalised y: top edge 1 - 2*Top, bottom edge -1 + 2*Bottom; centre = Bottom - Top
		const float C = Bottom - Top;
		if (FMath::IsNearlyZero(C) || Aspect <= 0.f) return 0.f;
		const float TanV = FMath::Tan(FMath::DegreesToRadians(HFov * 0.5f)) / Aspect;
		return FMath::RadiansToDegrees(FMath::Atan(C * TanV)); // look point this far above the optical axis -> pitch down
	}

	bool Project(const FVector& CamLoc, const FRotator& Rot, float HFov, float Aspect, const FVector& P, FVector2D& Out)
	{
		const FVector L = Rot.UnrotateVector(P - CamLoc);
		if (L.X < 1.f) { Out = FVector2D(10.f, 10.f); return false; }
		const float TanH = FMath::Tan(FMath::DegreesToRadians(HFov * 0.5f));
		const float TanV = TanH / FMath::Max(Aspect, 0.01f);
		Out = FVector2D((L.Y / L.X) / TanH, (L.Z / L.X) / TanV);
		return true;
	}

	FVector Boom(const FVector& Pivot, float Yaw, float Pitch, float Dist)
	{
		const float Pr = FMath::DegreesToRadians(Pitch);
		return Pivot - FRotator(0.f, Yaw, 0.f).Vector() * Dist * FMath::Cos(Pr) + FVector(0.f, 0.f, Dist * FMath::Sin(Pr));
	}

	FRotator BandRotation(const FVector& Loc, const FVector& Look, float HFov, float Aspect, float Top, float Bottom)
	{
		FRotator R = (Look - Loc).Rotation();
		R.Pitch -= BandPitch(HFov, Aspect, Top, Bottom);
		return R;
	}

	float FitPointsDist(TArrayView<const FVector> Pts, const FVector& Pivot, float Yaw, float Pitch, float HFov, float Aspect, float Top, float Bottom, float YMargin, float MinDist, float MaxDist)
	{
		const float XM = CourtMargin();
		const float Y1 = 1.f - 2.f * Top - YMargin, Y0 = -1.f + 2.f * Bottom + YMargin;
		auto Fits = [&](float D)
		{
			const FVector Loc = Boom(Pivot, Yaw, Pitch, D);
			const FRotator Rot = BandRotation(Loc, Pivot, HFov, Aspect, Top, Bottom);
			for (const FVector& P : Pts)
			{
				FVector2D S;
				if (!Project(Loc, Rot, HFov, Aspect, P, S)) return false;
				if (FMath::Abs(S.X) > XM || S.Y > Y1 || S.Y < Y0) return false;
			}
			return true;
		};
		if (Fits(MinDist)) return MinDist;
		if (!Fits(MaxDist)) return MaxDist;
		float Lo = MinDist, Hi = MaxDist;
		for (int32 i = 0; i < 24; ++i) { const float M = 0.5f * (Lo + Hi); if (Fits(M)) Hi = M; else Lo = M; }
		return Hi;
	}

	float FitCircleDist(const FVector& C, float R, float Yaw, float Pitch, float HFov, float Aspect, float Top, float Bottom, float MinDist, float MaxDist)
	{
		const float XM = CourtMargin();
		const float Y1 = 1.f - 2.f * Top, Y0 = -1.f + 2.f * Bottom;
		auto Fits = [&](float D)
		{
			const FVector Loc = Boom(C, Yaw, Pitch, D);
			const FRotator Rot = BandRotation(Loc, C, HFov, Aspect, Top, Bottom);
			for (int32 k = 0; k < 48; ++k)
			{
				const float A = FMath::DegreesToRadians(7.5f * k);
				FVector2D S;
				if (!Project(Loc, Rot, HFov, Aspect, C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f), S)) return false;
				if (FMath::Abs(S.X) > XM || S.Y > Y1 || S.Y < Y0) return false;
			}
			return true;
		};
		if (Fits(MinDist)) return MinDist;
		if (!Fits(MaxDist)) return MaxDist;
		float Lo = MinDist, Hi = MaxDist;
		for (int32 i = 0; i < 24; ++i) { const float M = 0.5f * (Lo + Hi); if (Fits(M)) Hi = M; else Lo = M; }
		return Hi;
	}
}
