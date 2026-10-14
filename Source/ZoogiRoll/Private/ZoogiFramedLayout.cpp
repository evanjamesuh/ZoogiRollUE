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

	FLayout Compute(const FVector2D& V, const FMargin& S, float TrayOpen)
	{
		FLayout L;
		L.Viewport = V;
		L.Safe = S;
		const float W = V.X, H = V.Y;
		const float U = H / 1080.f;
		L.U = U;
		const float A = W / FMath::Max(H, 1.f);
		L.bCompact = A > 2.0f || A < 1.5f || H < 700.f;
		L.TrayOpen = L.bCompact ? FMath::Clamp(TrayOpen, 0.f, 1.f) : 1.f;
		const float Gap = 14.f * U;
		const float TopH = (L.bCompact ? 58.f : 70.f) * U;
		// Side columns: 300 units at 16:9, shrink on narrow and ultra-wide (mobile landscape) screens; the arena always keeps >= 52% of the width.
		float ColW = (A < 1.5f ? 220.f : (A > 2.0f ? 250.f : 300.f)) * U;
		const float Avail = W - S.Left - S.Right;
		ColW = FMath::Min(ColW, (Avail - 4.f * Gap) * 0.24f);
		// v2 (Evan, Oct 14 9:49): the player prompt lives in the active Zoogi's team-column action box, so the tray is all
		// card hand / Awaken controls (taller, cards fully visible); collapsed it is a thin strip for system text only.
		const float PromptH = 30.f * U;
		const float TrayFull = 272.f * U, TrayMin = PromptH + 6.f * U;
		const float TrayH = FMath::Lerp(TrayMin, TrayFull, L.TrayOpen);
		const float X0 = S.Left, X1 = W - S.Right, Y0 = S.Top, Y1 = H - S.Bottom;
		L.Top = FBox2D(FVector2D(X0 + Gap, Y0 + Gap * 0.5f), FVector2D(X1 - Gap, Y0 + Gap * 0.5f + TopH));
		const float AY0 = L.Top.Max.Y + Gap, AY1 = Y1 - TrayH - Gap * 0.5f;
		L.Left = FBox2D(FVector2D(X0 + Gap, AY0), FVector2D(X0 + Gap + ColW, AY1));
		L.Right = FBox2D(FVector2D(X1 - Gap - ColW, AY0), FVector2D(X1 - Gap, AY1));
		L.Arena = FBox2D(FVector2D(FMath::RoundToFloat(L.Left.Max.X + Gap), FMath::RoundToFloat(AY0)), FVector2D(FMath::RoundToFloat(L.Right.Min.X - Gap), FMath::RoundToFloat(AY1)));
		L.Tray = FBox2D(FVector2D(X0 + Gap, AY1 + Gap * 0.5f), FVector2D(X1 - Gap, Y1 - Gap * 0.4f));
		L.Prompt = FBox2D(FVector2D(L.Arena.Min.X, L.Tray.Min.Y + 3.f * U), FVector2D(L.Arena.Max.X, L.Tray.Min.Y + 3.f * U + PromptH)); // system text only (forming / match over)
		L.bValid = true;
		return L;
	}

	const FLayout& Current() { return GCur; }
	void SetCurrent(const FLayout& L) { GCur = L; }
	void Clear() { GCur.bValid = false; }

	FMargin SafeInsets(const FVector2D& V)
	{
		const float U = V.Y / 1080.f;
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

	bool IsOutsideArena(const UWorld* World, const FVector2D& P)
	{
		if (!GCur.bValid || !IsActive(World)) return false;
		return !GCur.Arena.IsInside(P);
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
