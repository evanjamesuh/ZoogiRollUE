// quality items 4 + 31 Ã¢â‚¬â€ see ZoogiQualityCam.h
#include "ZoogiQualityCam.h"
#include "ZoogiPlayerController.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"


namespace
{
	static TAutoConsoleVariable<int32> CVarEdgeSlowMo(
		TEXT("zoogi.Cam.EdgeSlowMo"), 1,
		TEXT("1 = brief presentation slow-mo (camera/director MotionRate) when a Zoogi starts a ring-out fall. Never dilates physics."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarEdgeSlowMoScale(
		TEXT("zoogi.Cam.EdgeSlowMoScale"), 0.35f,
		TEXT("Presentation MotionRate target (0.05..1) for EdgeFallArena slow-look. Default 0.35."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarEdgeSlowMoHold(
		TEXT("zoogi.Cam.EdgeSlowMoHold"), 0.55f,
		TEXT("Seconds at full presentation slow-look during edge fall. Default 0.55."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarEdgeSlowMoDuration(
		TEXT("zoogi.Cam.EdgeSlowMoDuration"), 1.45f,
		TEXT("Total EdgeFallArena MainView duration (seconds). Default 1.45."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarBestShotReplay(
		TEXT("zoogi.Cam.BestShotReplay"), 1,
		TEXT("1 = play a short best-shot replay into GetReplayPipTexture / OnGameplayPipShot(REPLAY). Never takes MainView."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarBestShotHold(
		TEXT("zoogi.Cam.BestShotHold"), 1.2f,
		TEXT("Minimum seconds the best-shot REPLAY panel holds after the RenderTarget shot ends. Default 1.2."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarBestShotFps(
		TEXT("zoogi.Cam.BestShotFps"), 20.f,
		TEXT("Target capture rate hint for best-shot panel (director still drives captures). Default 20."),
		ECVF_Default);
	static TAutoConsoleVariable<float> CVarBestShotMinAir(
		TEXT("zoogi.Cam.BestShotMinAir"), 0.85f,
		TEXT("Airborne seconds that qualify as a best-shot trigger. Default 0.85."),
		ECVF_Default);

	struct FQCState
	{
		FZoogiShotHandle EdgeHandle;
		FZoogiShotHandle ReplayHandle;
		TWeakObjectPtr<UTextureRenderTarget2D> ReplayRT;
		TWeakObjectPtr<UTextureRenderTarget2D> LiveRT;
		TWeakObjectPtr<USceneCaptureComponent2D> LiveCap;
		TWeakObjectPtr<AActor> LiveRig;
		bool bGameplayPipActive = false;
		float ReplayHoldLeft = 0.f;
		float LiveCapAcc = 0.f;
		// Rising-edge bookkeeping (PlayerIndex -> was leaving / air time).
		TMap<int32, bool> WasLeaving;
		TMap<int32, float> AirTime;
		TMap<int32, int32> LastScore;
		float CooldownEdge = 0.f;
		float CooldownBest = 0.f;
		int32 BestScoreSeen = -1;
	};

	static TMap<TWeakObjectPtr<AZoogiPlayerController>, FQCState> GState;

	static FQCState& StateOf(AZoogiPlayerController* PC)
	{
		return GState.FindOrAdd(PC);
	}

	static const FQCState* StateOfConst(const AZoogiPlayerController* PC)
	{
		return GState.Find(PC);
	}

	static void BroadcastPipShot(AZoogiPlayerController* PC, FName ShotType)
	{
		if (!PC) return;
		PC->OnGameplayPipShot.Broadcast(ShotType);
		UE_LOG(LogZoogi, Log, TEXT("CAM best-shot: OnGameplayPipShot(%s)"), *ShotType.ToString());
	}

	static UTextureRenderTarget2D* EnsureLiveRT(AZoogiPlayerController* PC, FQCState& S)
	{
		if (UTextureRenderTarget2D* Existing = S.LiveRT.Get()) return Existing;
		if (!PC || !PC->GetWorld()) return nullptr;
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(PC);
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);
		RT->bAutoGenerateMips = false;
		RT->InitAutoFormat(256, 256);
		RT->UpdateResourceImmediate(true);
		S.LiveRT = RT;
		return RT;
	}

	static void EnsureLiveCapture(AZoogiPlayerController* PC, FQCState& S)
	{
		if (S.LiveCap.IsValid()) return;
		UWorld* W = PC ? PC->GetWorld() : nullptr;
		if (!W) return;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Rig = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SP);
		if (!Rig) return;
		Rig->SetActorTickEnabled(false);
		USceneComponent* Root = NewObject<USceneComponent>(Rig, TEXT("Root"));
		Rig->SetRootComponent(Root);
		Root->RegisterComponent();
		USceneCaptureComponent2D* C = NewObject<USceneCaptureComponent2D>(Rig);
		C->SetupAttachment(Root);
		C->RegisterComponent();
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		C->FOVAngle = 70.f;
		C->ShowFlags.SetMotionBlur(false);
		C->TextureTarget = EnsureLiveRT(PC, S);
		S.LiveRig = Rig;
		S.LiveCap = C;
		UE_LOG(LogZoogi, Log, TEXT("CAM gameplay-pip: live SceneCapture 256x256 ready"));
	}

	static void TickLiveCapture(AZoogiPlayerController* PC, FQCState& S, float Dt)
	{
		if (!S.bGameplayPipActive) return;
		EnsureLiveCapture(PC, S);
		USceneCaptureComponent2D* C = S.LiveCap.Get();
		if (!C || !PC || !PC->PlayerCameraManager) return;
		const float Period = 1.f / FMath::Clamp(CVarBestShotFps.GetValueOnGameThread(), 5.f, 60.f);
		S.LiveCapAcc += Dt;
		if (S.LiveCapAcc < Period) return;
		S.LiveCapAcc = 0.f;
		const FMinimalViewInfo V = PC->PlayerCameraManager->GetCameraCacheView();
		C->SetWorldLocationAndRotation(V.Location, V.Rotation);
		C->FOVAngle = V.FOV;
		if (!C->TextureTarget) C->TextureTarget = EnsureLiveRT(PC, S);
		C->CaptureScene();
	}
}

namespace ZoogiQualityCam
{
	bool EdgeSlowMoEnabled() { return CVarEdgeSlowMo.GetValueOnGameThread() != 0; }
	bool BestShotEnabled() { return CVarBestShotReplay.GetValueOnGameThread() != 0; }

	FZoogiShotSpec MakeEdgeFallSpec()
	{
		FZoogiShotSpec S = UZoogiShotLibrary::MakeDefault(EZoogiShotType::EdgeFallArena);
		S.SlowMoTarget = FMath::Clamp(CVarEdgeSlowMoScale.GetValueOnGameThread(), 0.05f, 1.f);
		S.SlowMoHold = FMath::Max(0.05f, CVarEdgeSlowMoHold.GetValueOnGameThread());
		S.Duration = FMath::Max(S.SlowMoHold + S.SlowMoEaseIn + S.SlowMoEaseOut + 0.25f,
			CVarEdgeSlowMoDuration.GetValueOnGameThread());
		return S;
	}

	bool TryEdgeSlowMo(AZoogiPlayerController* PC, AZoogiBall* Falling, AZoogiGameMode* G)
	{
		if (!PC || !Falling || !EdgeSlowMoEnabled()) return false;
		FQCState& S = StateOf(PC);
		if (S.CooldownEdge > 0.f) return false;
		UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(PC);
		if (!Dir) { UE_LOG(LogZoogi, Warning, TEXT("CAM edge-slowmo: no EpisodeCameraDirector Ã¢â‚¬â€ fail soft")); return false; }
		if (Dir->NumActiveShots() > 0) return false; // don't stomp another MainView / panel shot

		FZoogiShotSpec Spec = MakeEdgeFallSpec();
		FZoogiShotPlayParams P;
		if (G && G->Arena)
		{
			P.EventLocation = G->Arena->GetActorLocation();
			P.bHasEventLocation = true;
		}
		const FZoogiShotHandle H = Dir->PlayShot(Spec, Falling, nullptr, EZoogiShotOutput::MainView, P);
		if (!H.IsValid()) return false;
		S.EdgeHandle = H;
		S.CooldownEdge = Spec.Duration + 0.75f;
		UE_LOG(LogZoogi, Log,
			TEXT("CAM edge-slowmo: EdgeFallArena MainView id=%d scale=%.2f hold=%.2f dur=%.2f (presentation only Ã¢â‚¬â€ no world dilation)"),
			H.Id, Spec.SlowMoTarget, Spec.SlowMoHold, Spec.Duration);
		return true;
	}

	bool TryBestShotReplay(AZoogiPlayerController* PC, AZoogiBall* Focus, FName Reason)
	{
		if (!PC || !Focus || !BestShotEnabled()) return false;
		FQCState& S = StateOf(PC);
		if (S.CooldownBest > 0.f || S.ReplayHoldLeft > 0.f) return false;
		UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(PC);
		if (!Dir) { UE_LOG(LogZoogi, Warning, TEXT("CAM best-shot: no EpisodeCameraDirector Ã¢â‚¬â€ fail soft")); return false; }

		// Prefer Airborne (built-in slow-look) for the panel; fall back to Tracking.
		EZoogiShotType Type = EZoogiShotType::Airborne;
		FZoogiShotSpec Spec = UZoogiShotLibrary::MakeDefault(Type);
		Spec.Duration = FMath::Max(1.35f, CVarBestShotHold.GetValueOnGameThread() + 0.15f);
		Spec.bSlowMoEntry = true;
		Spec.SlowMoTarget = 0.4f;
		Spec.SlowMoHold = FMath::Min(0.7f, Spec.Duration * 0.45f);

		FZoogiShotPlayParams P;
		P.RenderTargetSize = FIntPoint(320, 320);
		const FZoogiShotHandle H = Dir->PlayShot(Spec, Focus, nullptr, EZoogiShotOutput::RenderTarget, P);
		if (!H.IsValid())
		{
			Type = EZoogiShotType::Tracking;
			Spec = UZoogiShotLibrary::MakeDefault(Type);
			Spec.Duration = FMath::Max(1.35f, CVarBestShotHold.GetValueOnGameThread() + 0.15f);
			const FZoogiShotHandle H2 = Dir->PlayShot(Spec, Focus, nullptr, EZoogiShotOutput::RenderTarget, P);
			if (!H2.IsValid()) { UE_LOG(LogZoogi, Warning, TEXT("CAM best-shot: PlayShot failed (%s) Ã¢â‚¬â€ fail soft"), *Reason.ToString()); return false; }
			S.ReplayHandle = H2;
		}
		else S.ReplayHandle = H;

		UTextureRenderTarget2D* RT = Dir->GetShotRenderTarget(S.ReplayHandle);
		S.ReplayRT = RT;
		S.ReplayHoldLeft = FMath::Max(1.2f, CVarBestShotHold.GetValueOnGameThread());
		S.CooldownBest = S.ReplayHoldLeft + Spec.Duration + 0.5f;
		BroadcastPipShot(PC, FName(TEXT("REPLAY")));
		UE_LOG(LogZoogi, Log,
			TEXT("CAM best-shot: REPLAY panel id=%d reason=%s rt=%s hold=%.2f (MainView untouched)"),
			S.ReplayHandle.Id, *Reason.ToString(), RT ? *RT->GetName() : TEXT("null"), S.ReplayHoldLeft);
		return true;
	}

	bool ForceEdgeSlowMo(AZoogiPlayerController* PC, AZoogiBall* Falling)
	{
		if (!PC) return false;
		StateOf(PC).CooldownEdge = 0.f;
		return TryEdgeSlowMo(PC, Falling, PC->GetWorld() ? Cast<AZoogiGameMode>(PC->GetWorld()->GetAuthGameMode()) : nullptr);
	}

	bool ForceBestShot(AZoogiPlayerController* PC, AZoogiBall* Focus, FName Reason)
	{
		if (!PC) return false;
		StateOf(PC).CooldownBest = 0.f;
		StateOf(PC).ReplayHoldLeft = 0.f;
		return TryBestShotReplay(PC, Focus, Reason);
	}

	bool IsEdgeSlowMoActive(const AZoogiPlayerController* PC)
	{
		const FQCState* S = StateOfConst(PC);
		if (!S || S->EdgeHandle.Id == 0) return false;
		UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(PC);
		return Dir && Dir->NumActiveShots() > 0;
	}

	bool IsBestShotActive(const AZoogiPlayerController* PC)
	{
		const FQCState* S = StateOfConst(PC);
		return S && (S->ReplayHoldLeft > 0.f || S->ReplayHandle.Id != 0);
	}

	UTextureRenderTarget2D* GetReplayPipTexture(const AZoogiPlayerController* PC)
	{
		const FQCState* S = StateOfConst(PC);
		if (!S || S->ReplayHoldLeft <= 0.f) return nullptr; // null when idle (contract)
		return S->ReplayRT.Get();
	}

	UTextureRenderTarget2D* GetGameplayPipTexture(const AZoogiPlayerController* PC)
	{
		const FQCState* S = StateOfConst(PC);
		return S ? S->LiveRT.Get() : nullptr;
	}

	void SetGameplayPipActive(AZoogiPlayerController* PC, bool bActive)
	{
		if (!PC) return;
		FQCState& S = StateOf(PC);
		if (S.bGameplayPipActive == bActive) return;
		S.bGameplayPipActive = bActive;
		if (bActive) EnsureLiveCapture(PC, S);
		UE_LOG(LogZoogi, Log, TEXT("CAM gameplay-pip: active=%d"), bActive ? 1 : 0);
	}

	void Tick(AZoogiPlayerController* PC, AZoogiGameMode* G, float Dt)
	{
		if (!PC) return;
		FQCState& S = StateOf(PC);
		S.CooldownEdge = FMath::Max(0.f, S.CooldownEdge - Dt);
		S.CooldownBest = FMath::Max(0.f, S.CooldownBest - Dt);
		if (S.ReplayHoldLeft > 0.f)
		{
			S.ReplayHoldLeft = FMath::Max(0.f, S.ReplayHoldLeft - Dt);
			if (S.ReplayHoldLeft <= 0.f)
			{
				S.ReplayRT = nullptr;
				S.ReplayHandle = FZoogiShotHandle();
				UE_LOG(LogZoogi, Log, TEXT("CAM best-shot: REPLAY panel idle (GetReplayPipTexture -> null)"));
			}
		}
		TickLiveCapture(PC, S, Dt);

		if (!G || G->Phase == EZoogiPhase::Menu) return;
		const float CourtZ = G->Arena ? G->Arena->GetActorLocation().Z : 0.f;

		// ---- Item 4: rising edge of leaving-play -> presentation EdgeFallArena
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bParked) continue;
			const int32 Idx = B->PlayerIndex;
			const bool bLeaving = AZoogiPlayerController::IsLeavingPlay(B->BallLocation(), B->bOut, CourtZ, G->Arena);
			const bool bWas = S.WasLeaving.FindRef(Idx);
			if (bLeaving && !bWas)
			{
				TryEdgeSlowMo(PC, B, G);
				// Opponent ring-out is also a best-shot candidate (item 31).
				if (PC->GetPawn() || true)
				{
					AZoogiBall* Local = nullptr;
					for (AZoogiBall* X : G->Balls) { if (X && !X->bCPU) { Local = X; break; } }
					const int32 LocalTeam = Local ? (Local->PlayerIndex / 2) : 0;
					const int32 BallTeam = B->PlayerIndex / 2;
					if (BallTeam != LocalTeam)
						TryBestShotReplay(PC, B, FName(TEXT("RingOutOpponent")));
				}
			}
			S.WasLeaving.Add(Idx, bLeaving);

			// ---- Item 31: longest air time heuristic (fail soft Ã¢â‚¬â€ no GameMode BallAir dependency)
			float& Air = S.AirTime.FindOrAdd(Idx);
			const FVector V = B->GetBallVelocity();
			const float Above = B->BallLocation().Z - CourtZ;
			const bool bAir = !bLeaving && !B->bOut && Above > 40.f && V.Z > -200.f;
			if (bAir) Air += Dt; else
			{
				if (Air >= CVarBestShotMinAir.GetValueOnGameThread())
					TryBestShotReplay(PC, B, FName(TEXT("LongAir")));
				Air = 0.f;
			}

			// Highest score event (name-lookup fail soft): watch Score rising by RingOutScore-ish amounts.
			const int32 Sc = B->Score;
			const int32 Prev = S.LastScore.FindRef(Idx);
			if (Prev >= 0 && Sc - Prev >= 50)
				TryBestShotReplay(PC, B, FName(TEXT("ScoreSpike")));
			S.LastScore.Add(Idx, Sc);
		}
	}
}

