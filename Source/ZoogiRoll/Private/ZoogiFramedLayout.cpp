// framed: see ZoogiFramedLayout.h
#include "ZoogiFramedLayout.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiRoll.h"
#include "ZoogiPlayerController.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "UnrealClient.h"

namespace ZoogiFramed
{
	static FLayout GCur;
	static float GPulse = 0.f;
	static FLinearColor GPulseColor = FLinearColor::White;

	bool IsEnabled()
	{
		static const bool b = []()
		{
			bool bIni = false;
			if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.UI"), TEXT("bFramedHUD"), bIni, GGameIni);
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFramedHUD"))) return false;
			return bIni || FParse::Param(FCommandLine::Get(), TEXT("ZoogiFramedHUD"));
		}();
		return b;
	}

	bool IsActive(const UWorld* World)
	{
		if (!IsEnabled() || !World) return false;
		static const bool bNoHUD = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHUD"));
		if (bNoHUD) return false;
		const AZoogiGameMode* G = World->GetAuthGameMode<AZoogiGameMode>();
		return G && G->Phase != EZoogiPhase::Menu;
	}

	// ---- Oct 17 (Evan 12:10 PM): HUD hidden = portrait play
	static int32 GHudHidden = -1; // -1 = not decided yet (first framed frame decides by aspect)
	bool IsHudHidden() { return GHudHidden == 1; }
	void SetHudHidden(bool bHidden)
	{
		if (GHudHidden == (bHidden ? 1 : 0)) return;
		GHudHidden = bHidden ? 1 : 0;
		UE_LOG(LogZoogi, Log, TEXT("FRAMED HUD %s"), bHidden ? TEXT("HIDDEN -> portrait play (arena full screen, edge controls)") : TEXT("SHOWN -> landscape framed hud-v3"));
	}
	static void DecideHudHidden(const FVector2D& V)
	{
		if (GHudHidden >= 0) return;
		const TCHAR* Cmd = FCommandLine::Get();
		bool bHide = V.Y > V.X; // follow the device / window orientation at the first match frame
		if (FParse::Param(Cmd, TEXT("ZoogiHudHidden"))) bHide = true;
		if (FParse::Param(Cmd, TEXT("ZoogiHudShown"))) bHide = false;
		SetHudHidden(bHide);
	}
	static TArray<FBox2D> GUiRects;
	void SetUiRects(const TArray<FBox2D>& Rects) { GUiRects = Rects; }

	// battle view mode hook (Game Camera owns the portrait framing and listens here)
	static int32 GModeHidden = -1, GModePortrait = -1;
	FZoogiBattleViewModeChanged& OnBattleViewModeChanged() { static FZoogiBattleViewModeChanged D; return D; }
	void GetBattleViewMode(bool& bOutHudHidden, bool& bOutPortrait) { bOutHudHidden = GModeHidden == 1; bOutPortrait = GModePortrait == 1; }
	bool BroadcastBattleViewMode(bool bHudHidden, bool bPortrait)
	{
		if (GModeHidden == (bHudHidden ? 1 : 0) && GModePortrait == (bPortrait ? 1 : 0)) return false;
		GModeHidden = bHudHidden ? 1 : 0;
		GModePortrait = bPortrait ? 1 : 0;
		UE_LOG(LogZoogi, Log, TEXT("FRAMED OnBattleViewModeChanged hudHidden=%d portrait=%d"), GModeHidden, GModePortrait);
		OnBattleViewModeChanged().Broadcast(bHudHidden, bPortrait);
		return true;
	}

	static FLayout ComputePortrait(const FVector2D& V, const FMargin& S)
	{
		// Portrait play (HUD hidden): the 3D arena fills the whole screen (behind the notch too); the controls stay inside the
		// safe insets. Top edge: HUD toggle (left), turn / score indicator (centre), ZOOM (right), all >= 58 design px, plus the
		// prompt line - everything inside the top 5% band Game Camera reserves (96 px of 1920; below the safe-area top inset).
		// Bottom: the tray rects (hand recess over the deck | discard + orb row) for the Standby / Awaken overlay. Design is
		// 1080 px wide (1080x1920 .. 1080x2340; the PC's 540x1080 window is the same at P 0.5).
		FLayout L;
		L.Viewport = V;
		L.Safe = S;
		L.bPortrait = true;
		L.bCompact = true;
		L.TrayOpen = 1.f;
		const float X0 = S.Left, X1 = V.X - S.Right, Y0 = S.Top, Y1 = V.Y - S.Bottom;
		const float P = FMath::Max(0.1f, FMath::Min((X1 - X0) / 1080.f, (Y1 - Y0) / 1920.f)); // 1080-wide design (a landscape window without the turn still fits)
		L.P = P;
		L.U = P;
		L.ColT = 220.f;
		auto Box = [](float Ax, float Ay, float Bx, float By) { return FBox2D(FVector2D(Ax, Ay), FVector2D(Bx, By)); };
		const float BH = PortraitTap * P, Ty = Y0 + 6.f * P;
		L.HudBtn = Box(X0 + 12.f * P, Ty, X0 + 12.f * P + 150.f * P, Ty + BH);
		L.ZoomBtn = Box(X1 - 12.f * P - 170.f * P, Ty, X1 - 12.f * P, Ty + BH);
		L.Indicator = Box(L.HudBtn.Max.X + 14.f * P, Ty, L.ZoomBtn.Min.X - 14.f * P, Ty + BH);
		L.Top = Box(X0, Y0, X1, Ty + BH + 28.f * P); // + one prompt line under the strip (ends at Y0 + 92 P, inside the 96 P band)
		L.Arena = Box(0.f, 0.f, V.X, V.Y);
		L.Left = L.Right = Box(X0, Y0, X0, Y0);
		// tray overlay (Standby / Awaken only), bottom up
		L.TrayT = 1.45f * P;
		L.T = L.TrayT;
		const float RowTop = Y1 - 190.f * L.TrayT;
		L.BottomRow = Box(X0, RowTop, X1, Y1);
		L.TrayL = FVector2D(X0 + 6.f * P, RowTop);
		L.TrayR = FVector2D(X1 - 398.f * L.TrayT, RowTop);
		L.TrayLeftX1 = L.TrayL.X + 200.f * L.TrayT; // deck slab crop of tray_left
		L.TrayRightX0 = L.TrayR.X;
		const float RecTop = RowTop - 272.f * P;
		L.Tray = Box(X0, RecTop, X1, Y1);
		L.Hand = Box(X0 + 12.f * P, RecTop + 18.f * P, X1 - 12.f * P, RowTop - 10.f * P);
		L.Deck = L.TrayL + FVector2D(97.f, 90.f) * L.TrayT;
		L.Discard = L.TrayR + FVector2D(72.f, 90.f) * L.TrayT;
		L.Energy = L.TrayR + FVector2D(328.f, 123.f) * L.TrayT;
		L.Prompt = Box(X0 + 20.f * P, Ty + BH + 2.f * P, X1 - 20.f * P, Ty + BH + 28.f * P);
		L.bValid = true;
		return L;
	}

	FLayout Compute(const FVector2D& V, const FMargin& S, float TrayOpen)
	{
		// Oct 17 (Evan): HUD hidden = portrait play layout, HUD shown = hud-v3 landscape (unchanged)
		DecideHudHidden(V);
		if (IsHudHidden()) return ComputePortrait(V, S);
		// hud-v3 (Evan's 3:53 PM target): every piece sits where it sits in the 1376x768 target, scaled by T; the arena
		// takes whatever is left between the columns (16:9 at 16:9, wider on phones up to 20:9), columns widen a little on
		// ultra-wide screens and shrink on narrow ones. One layout for PC and phones; the tray is always open.
		FLayout L;
		L.Viewport = V;
		L.Safe = S;
		const float W = V.X, H = V.Y;
		L.U = H / 1080.f;
		const float A = W / FMath::Max(H, 1.f);
		L.bCompact = A > 2.0f || A < 1.5f || H < 700.f;
		L.TrayOpen = 1.f;
		const float X0 = S.Left, X1 = W - S.Right, Y0 = S.Top, Y1 = H - S.Bottom;
		float T = FMath::Max(0.1f, (Y1 - Y0) / 768.f);
		if ((X1 - X0) / T < 1180.f) T = (X1 - X0) / 1180.f; // narrower than ~14:9: width-limited, the arena gets taller
		L.T = T;
		const float WT = (X1 - X0) / T;
		const float ColT = 220.f; (void)WT; // hud-v3 overlay: columns keep the target width (220) from 16:9 to 20:9; only the arena widens
		L.ColT = ColT;
		const float TrayTop = Y1 - 202.f * T;
		L.Top = FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y0 + 56.f * T));
		L.Left = FBox2D(FVector2D(X0 + 8.f * T, Y0 + 62.f * T), FVector2D(X0 + (8.f + ColT) * T, TrayTop));
		L.Right = FBox2D(FVector2D(X1 - (8.f + ColT) * T, Y0 + 62.f * T), FVector2D(X1 - 8.f * T, TrayTop));
		L.Arena = FBox2D(FVector2D(FMath::RoundToFloat(L.Left.Max.X + 14.f * T), FMath::RoundToFloat(Y0 + 68.f * T)),
			FVector2D(FMath::RoundToFloat(L.Right.Min.X - 14.f * T), FMath::RoundToFloat(TrayTop - 6.f * T)));
		L.Tray = FBox2D(FVector2D(X0, TrayTop), FVector2D(X1, Y1));
		L.TrayLeftX1 = X0 + 362.f * T;
		L.TrayRightX0 = X1 - 398.f * T;
		L.Hand = FBox2D(FVector2D(L.TrayLeftX1 + 20.f * T, TrayTop + 22.f * T), FVector2D(L.TrayRightX0 - 30.f * T, Y1));
		L.Deck = FVector2D(X0 + 97.f * T, TrayTop + 102.f * T);
		L.Discard = FVector2D(X1 - 326.f * T, TrayTop + 102.f * T);
		L.Energy = FVector2D(X1 - 70.f * T, TrayTop + 135.f * T);
		L.Prompt = FBox2D(FVector2D(L.Hand.Min.X + 20.f * T, L.Hand.Min.Y + 70.f * T), FVector2D(L.Hand.Max.X - 20.f * T, L.Hand.Min.Y + 100.f * T)); // system text only (forming / match over)
		L.TrayL = FVector2D(X0, TrayTop + 12.f * T);
		L.TrayR = FVector2D(L.TrayRightX0, TrayTop + 12.f * T);
		L.TrayT = T;
		L.bValid = true;
		return L;
	}

	static FTrayInfo GTray;
	void SetTrayInfo(const FTrayInfo& I) { GTray = I; }
	const FTrayInfo& TrayInfo() { return GTray; }

	// ---- Oct 14: HUD frame shake + rumble
	struct FShakeTune
	{
		bool bEnabled = true, bRumble = true;
		float ImpulseThreshold = 9000.f, ImpulseFull = 32000.f, MaxOffset = 9.f, Duration = 0.28f, RumbleScale = 0.8f;
		FShakeTune()
		{
			const TCHAR* S = TEXT("ZoogiRoll.FrameShake");
			if (GConfig)
			{
				GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
				GConfig->GetBool(S, TEXT("bRumble"), bRumble, GGameIni);
				GConfig->GetFloat(S, TEXT("ImpulseThreshold"), ImpulseThreshold, GGameIni);
				GConfig->GetFloat(S, TEXT("ImpulseFull"), ImpulseFull, GGameIni);
				GConfig->GetFloat(S, TEXT("MaxOffset"), MaxOffset, GGameIni);
				GConfig->GetFloat(S, TEXT("Duration"), Duration, GGameIni);
				GConfig->GetFloat(S, TEXT("RumbleScale"), RumbleScale, GGameIni);
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFrameShake"))) bEnabled = false;
			ImpulseFull = FMath::Max(ImpulseFull, ImpulseThreshold + 1.f);
			Duration = FMath::Max(Duration, 0.05f);
		}
	};
	static const FShakeTune& ShakeTune() { static FShakeTune T; return T; }
	static float GShakeAmp = 0.f, GShakeT = 0.f, GShakeLastLog = -10.f;
	static int32 GShakeKicks = 0;

	static bool MenuSetting(const TCHAR* Key, int32 Def)
	{
		int32 V = Def;
		if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), Key, V, GGameUserSettingsIni);
		return V != 0;
	}

	bool FrameShakeEnabled() { return ShakeTune().bEnabled && MenuSetting(TEXT("FrameShake"), 1); }
	int32 FrameShakeKicks() { return GShakeKicks; }

	void FrameShake(float Strength, UWorld* World)
	{
		const FShakeTune& T = ShakeTune();
		const float K = FMath::Clamp(Strength, 0.f, 1.f);
		if (K <= 0.f) return;
		const bool bShake = FrameShakeEnabled();
		if (bShake) { GShakeAmp = FMath::Max(GShakeAmp, 0.35f + 0.65f * K); ++GShakeKicks; }
		bool bRumbled = false;
		if (T.bRumble && MenuSetting(TEXT("Haptics"), 1) && World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				// gamepad motors / phone vibrator (Android + iOS map force feedback to the device haptics)
				PC->PlayDynamicForceFeedback(FMath::Clamp((0.3f + 0.7f * K) * T.RumbleScale, 0.f, 1.f), 0.12f + 0.16f * K, true, true, true, true);
				bRumbled = true;
			}
		}
		const float Now = World ? World->GetRealTimeSeconds() : 0.f;
		if (Now - GShakeLastLog > 0.25f)
		{
			GShakeLastLog = Now;
			UE_LOG(LogZoogi, Log, TEXT("FRAMESHAKE kick %.2f shake=%d rumble=%d (kicks %d)"), K, bShake ? 1 : 0, bRumbled ? 1 : 0, GShakeKicks);
		}
	}

	void ImpactShake(float Impulse, UWorld* World)
	{
		const FShakeTune& T = ShakeTune();
		if (Impulse < T.ImpulseThreshold) return;
		FrameShake((Impulse - T.ImpulseThreshold) / (T.ImpulseFull - T.ImpulseThreshold), World);
	}

	FVector2D FrameShakeOffset(float Dt)
	{
		if (GShakeAmp <= 0.f) return FVector2D::ZeroVector;
		const FShakeTune& T = ShakeTune();
		GShakeT += Dt;
		GShakeAmp = FMath::Max(0.f, GShakeAmp - Dt / T.Duration);
		if (!FrameShakeEnabled()) { GShakeAmp = 0.f; return FVector2D::ZeroVector; }
		const float A = T.MaxOffset * GShakeAmp * GShakeAmp;
		return FVector2D(FMath::Sin(GShakeT * 71.f), FMath::Cos(GShakeT * 57.f) * 0.7f) * A;
	}

	// ---- Oct 14: profile card rects for the comic strip
	struct FProfRect { FVector2D Min = FVector2D::ZeroVector, Size = FVector2D::ZeroVector; double At = -100.0; };
	static TArray<FProfRect> GProf;
	void SetProfileRect(int32 Player, const FVector2D& AbsMin, const FVector2D& AbsSize)
	{
		if (Player < 0 || Player > 15) return;
		if (GProf.Num() <= Player) GProf.SetNum(Player + 1);
		GProf[Player].Min = AbsMin; GProf[Player].Size = AbsSize; GProf[Player].At = FPlatformTime::Seconds();
	}
	bool ProfileRect(int32 Player, FVector2D& OutAbsMin, FVector2D& OutAbsSize)
	{
		if (!GProf.IsValidIndex(Player) || FPlatformTime::Seconds() - GProf[Player].At > 1.0 || GProf[Player].Size.X < 4.f) return false;
		OutAbsMin = GProf[Player].Min; OutAbsSize = GProf[Player].Size;
		return true;
	}

	const FLayout& Current() { return GCur; }
	void SetCurrent(const FLayout& L) { GCur = L; }
	void Clear() { GCur.bValid = false; }

	FMargin SafeInsets(const FVector2D& V)
	{
		const float U = FMath::Min(V.X, V.Y) / 1080.f; // portrait: design px (1080 wide); landscape: 1080p units as before
		FString Ov;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiSafeArea="), Ov, false))
		{
			TArray<FString> P; Ov.ParseIntoArray(P, TEXT(","));
			if (P.Num() == 4) return FMargin(FCString::Atof(*P[0]) * U, FCString::Atof(*P[1]) * U, FCString::Atof(*P[2]) * U, FCString::Atof(*P[3]) * U);
		}
		if (FSlateApplication::IsInitialized())
		{
			FDisplayMetrics DM;
			FSlateApplication::Get().GetCachedDisplayMetrics(DM);
			const FVector4 Pd = DM.TitleSafePaddingSize;
			return FMargin(Pd.X, Pd.Y, Pd.Z, Pd.W);
		}
		return FMargin(0.f);
	}

	bool WantTrayOpen(const AZoogiGameMode* G)
	{
		if (!G) return false;
		if (G->Phase == EZoogiPhase::GameOver) return false;
		if (G->Phase != EZoogiPhase::Aiming) return false;
		if (G->Stage == EZoogiStage::Awaken && !G->bAwakenDone) return true; // hand / Awaken controls
		return G->IsHumanAiming() && G->IsAimStep();                         // battle controls
	}

	float ViewAspect(const APlayerController* PC)
	{
		const ULocalPlayer* LP = PC ? Cast<ULocalPlayer>(PC->Player) : nullptr;
		if (LP && LP->ViewportClient && LP->ViewportClient->Viewport)
		{
			const FIntPoint S = LP->ViewportClient->Viewport->GetSizeXY();
			const float W = S.X * LP->Size.X, H = S.Y * LP->Size.Y;
			if (W > 1.f && H > 1.f) return W / H;
		}
		int32 VX = 16, VY = 9;
		if (PC) PC->GetViewportSize(VX, VY);
		return VY > 0 ? (float)VX / (float)VY : 16.f / 9.f;
	}

	static FBox2D GZoomBtn(ForceInit); static bool bGZoomBtn = false;
	void SetZoomButtonRect(const FBox2D& R, bool bVisible) { GZoomBtn = R; bGZoomBtn = bVisible && R.bIsValid; }
	bool InZoomButton(const FVector2D& P) { return bGZoomBtn && GZoomBtn.ExpandBy(6.f).IsInside(P); }
	bool ZoomButtonRect(FBox2D& Out) { Out = GZoomBtn; return bGZoomBtn; }

	bool IsOutsideArena(const UWorld* World, const FVector2D& P)
	{
		if (!GCur.bValid || !IsActive(World)) return false;
		if (!GCur.Arena.IsInside(P) || InZoomButton(P)) return true;
		for (const FBox2D& R : GUiRects) if (R.bIsValid && R.IsInside(P)) return true; // Oct 17: portrait edge controls / tray overlay
		return false;
	}

	void Pulse(const FLinearColor& C) { GPulse = 1.f; GPulseColor = C; }
	float PulseAmount(FLinearColor& Out) { Out = GPulseColor; return GPulse; }
	void TickPulse(float Dt) { GPulse = FMath::Max(0.f, GPulse - Dt * 1.6f); }

	// ------------------------------------------------------------------ review captures
	void TickShots(UWorld* World)
	{
		static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFramedShots"));
		if (!bOn || !World) return;
		AZoogiGameMode* G = World->GetAuthGameMode<AZoogiGameMode>();
		if (!G || G->ShotDir.IsEmpty() || G->Phase != EZoogiPhase::Aiming) return;
		const FString Pre = IsActive(World) ? TEXT("framed") : TEXT("classic");
		static int32 StandbyN = 0, AimN = 0;
		static int32 LastStandbyRound = -1, LastAimKey = -1;
		const int32 Key = G->RoundNumber * 10 + G->ActivePlayer;
		if (G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && G->StageTime > 1.6f && LastStandbyRound != Key && StandbyN < 3)
		{
			LastStandbyRound = Key;
			G->TakeShot(FString::Printf(TEXT("%s_standby_%d_P%d"), *Pre, ++StandbyN, G->ActivePlayer + 1));
		}
		if (G->IsAimStep() && G->HasAim() && LastAimKey != Key && AimN < 3)
		{
			LastAimKey = Key;
			G->TakeShot(FString::Printf(TEXT("%s_battle_aim_%d_P%d"), *Pre, ++AimN, G->ActivePlayer + 1));
		}
	}

	// ------------------------------------------------------------------ input-mapping self check
	void TickAimCheck(UWorld* World)
	{
		if (!World || !IsActive(World) || !GCur.bValid) return;
		AZoogiGameMode* G = World->GetAuthGameMode<AZoogiGameMode>();
		AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(World->GetFirstPlayerController());
		if (!G || !PC || G->Phase != EZoogiPhase::Aiming || !G->IsAimStep()) return;
		static TWeakObjectPtr<AActor> LastLock;
		static FVector LastFree = FVector::ZeroVector;
		static int32 N = 0;
		if (N > 40) return;
		AActor* Lock = G->TargetLock.Get();
		const bool bFree = G->bFreeAim && !G->FreeAimPoint.Equals(LastFree, 1.f);
		const bool bNewLock = Lock && Lock != LastLock.Get();
		if (!bFree && !bNewLock) return;
		LastLock = Lock;
		if (G->bFreeAim) LastFree = G->FreeAimPoint;
		FVector W = G->bFreeAim ? G->FreeAimPoint : (Cast<AZoogiBall>(Lock) ? Cast<AZoogiBall>(Lock)->BallLocation() : (Cast<AZoogiOrb>(Lock) ? Cast<AZoogiOrb>(Lock)->OrbLocation() : Lock->GetActorLocation()));
		FVector2D S;
		if (!PC->ProjectWorldLocationToScreen(W, S)) return;
		FVector O, D;
		float Err = -1.f;
		if (PC->DeprojectScreenPositionToWorld(S.X, S.Y, O, D) && FMath::Abs(D.Z) > 1e-3f)
		{
			const float T = (W.Z - O.Z) / D.Z;
			Err = FVector::Dist(O + D * T, W);
		}
		const FVector2D Cur = PC->GetCursor();
		const ULocalPlayer* LP = Cast<ULocalPlayer>(PC->Player);
		UE_LOG(LogZoogi, Log, TEXT("FRAMED aimcheck #%d P%d %s %s: viewport %.0fx%.0f arena rect [%.0f,%.0f - %.0f,%.0f] LP origin (%.3f,%.3f) size (%.3f,%.3f) | target screen (%.1f,%.1f) inside=%d | cursor (%.1f,%.1f) cursor->target %.1f px | project->deproject err %.2f cm"),
			++N, G->ActivePlayer + 1, G->IsHumanAiming() ? TEXT("HUMAN") : TEXT("CPU"), G->bFreeAim ? TEXT("floor") : *G->TargetName(Lock),
			GCur.Viewport.X, GCur.Viewport.Y, GCur.Arena.Min.X, GCur.Arena.Min.Y, GCur.Arena.Max.X, GCur.Arena.Max.Y,
			LP ? LP->Origin.X : 0.f, LP ? LP->Origin.Y : 0.f, LP ? LP->Size.X : 0.f, LP ? LP->Size.Y : 0.f,
			S.X, S.Y, GCur.Arena.IsInside(S) ? 1 : 0, Cur.X, Cur.Y, FVector2D::Distance(Cur, S), Err);
	}
}
