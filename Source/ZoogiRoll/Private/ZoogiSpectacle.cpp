// b268: match spectacle - see ZoogiSpectacle.h
#include "ZoogiSpectacle.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiScoringUI.h"
#include "ZoogiRoyaleMix.h"
#include "ZoogiTrickShot.h"
#include "ZoogiMusicSubsystem.h"
#include "ZoogiRedirect.h"
#include "ZoogiSkills.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformTime.h"

namespace ZoogiSpectacle
{
	namespace
	{
		bool Flag(const TCHAR* N) { return FParse::Param(FCommandLine::Get(), N) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSpectacle")); }
		double Now() { return FPlatformTime::Seconds(); }

		TWeakObjectPtr<AZoogiGameMode> GG;
		TArray<bool> GWasOut;
		int32 GPrevPhase = -1, GPrevStreak = 0;
		float GAimFrac = 0.f;
		double GSlowEnd = -1.0, GLastKO = -100.0;

		// one spectacle camera at a time (the KO moment can take over from the shot cam)
		enum class ECam : uint8 { None, KO, Shot };
		ECam GCam = ECam::None;
		TWeakObjectPtr<ACameraActor> GCamActor;
		TWeakObjectPtr<AActor> GCamPrev;
		TWeakObjectPtr<AZoogiBall> GCamBall;
		double GCamT0 = 0.0; float GCamLen = 0.f;
		bool GRet = false; double GRetT0 = 0.0; FVector GRetPos = FVector::ZeroVector; FRotator GRetRot = FRotator::ZeroRotator; float GRetFov = 82.f; // b282: real-time glide back to the match view
		FVector GCamPos = FVector::ZeroVector, GKODir = FVector::ForwardVector, GKOLook = FVector::ZeroVector;

		// final round lights
		bool GFinal = false;
		TArray<TPair<TWeakObjectPtr<UDirectionalLightComponent>, float>> GLights;
		TArray<TPair<TWeakObjectPtr<USkyLightComponent>, float>> GSkies;
		float GDim = 0.f, GBeatGlow = 0.f; int64 GLastBeat = -1; double GClock0 = 0.0;

		APlayerController* PC(AZoogiGameMode* G) { UWorld* W = G ? G->GetWorld() : nullptr; return W ? W->GetFirstPlayerController() : nullptr; }

		void EndCam(float Blend)
		{
			ACameraActor* C = GCamActor.Get();
			if (C && GG.IsValid())
				if (APlayerController* P = PC(GG.Get()))
					if (P->GetViewTarget() == C) P->SetViewTargetWithBlend(GCamPrev.IsValid() ? GCamPrev.Get() : P->GetPawn(), Blend, VTBlend_Cubic);
			if (C) C->SetLifeSpan(Blend + 0.5f);
			GCamActor.Reset(); GCam = ECam::None; GRet = false;
		}

		bool StartCam(AZoogiGameMode* G, ECam Kind, AZoogiBall* B, const FVector& Pos, const FVector& Look, float Len)
		{
			APlayerController* P = PC(G);
			UWorld* W = G->GetWorld();
			if (!P || !W) return false;
			AActor* Prev = GCamActor.IsValid() ? GCamPrev.Get() : P->GetViewTarget();
			if (GCamActor.IsValid()) { if (ACameraActor* Old = GCamActor.Get()) Old->SetLifeSpan(0.5f); GCamActor.Reset(); }
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACameraActor* Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Look - Pos).Rotation(), Pos), SP);
			if (!Cam) return false;
			Cam->GetCameraComponent()->SetFieldOfView(Kind == ECam::KO ? 70.f : 82.f);
			Cam->GetCameraComponent()->bConstrainAspectRatio = false;
			GCamPrev = Prev; GCamActor = Cam; GCam = Kind; GCamBall = B; GCamT0 = Now(); GCamLen = Len; GCamPos = Pos;
			P->SetViewTargetWithBlend(Cam, Kind == ECam::KO ? 0.f : 0.25f);
			return true;
		}

		bool WorldHeld()
		{	// b282: the game is paused / slowed for a REACTION (redirect pick, skill aim, clash / timing tap slow-mo) - a view-target blend would freeze
			UWorld* W = GG.IsValid() ? GG->GetWorld() : nullptr;
			const float Td = (W && W->GetWorldSettings()) ? W->GetWorldSettings()->TimeDilation : 1.f;
			return ZoogiRedirect::Picking() || ZoogiSkills::Picking() || Td < 0.15f;
		}
		void BeginReturn()
		{	// b282 (Evan: "automatically begin transition to normal view after launch the moment the gameplay is paused for reaction"):
			// glide the ride camera back to the match camera in REAL time (0.45 s), then hand the view back with no blend
			ACameraActor* C = GCamActor.Get();
			if (!C || GRet) return;
			GRet = true; GRetT0 = Now(); GRetPos = C->GetActorLocation(); GRetRot = C->GetActorRotation(); GRetFov = C->GetCameraComponent()->FieldOfView;
			UE_LOG(LogZoogi, Log, TEXT("SPECTACLE shot cam: game held for a reaction - gliding back to the match view"));
		}
		void TickCam()
		{
			ACameraActor* C = GCamActor.Get();
			if (!C || GCam == ECam::None) { GCam = ECam::None; GRet = false; return; }
			if (GRet)
			{
				AActor* Prev = GCamPrev.Get();
				APlayerController* P = GG.IsValid() ? PC(GG.Get()) : nullptr;
				FMinimalViewInfo Vi; bool bHave = false;
				if (Prev && Prev != C) { Prev->CalcCamera(0.f, Vi); bHave = true; }
				const float A = FMath::Clamp((float)(Now() - GRetT0) / 0.45f, 0.f, 1.f), E = A * A * (3.f - 2.f * A);
				if (bHave)
				{
					C->SetActorLocationAndRotation(FMath::Lerp(GRetPos, Vi.Location, E), FQuat::Slerp(GRetRot.Quaternion(), Vi.Rotation.Quaternion(), E).Rotator());
					C->GetCameraComponent()->SetFieldOfView(FMath::Lerp(GRetFov, Vi.FOV, E));
				}
				if (A >= 1.f || !bHave)
				{
					if (P && P->GetViewTarget() == C) P->SetViewTarget(Prev ? Prev : P->GetPawn());
					C->SetLifeSpan(0.5f); GCamActor.Reset(); GCam = ECam::None; GRet = false;
				}
				return;
			}
			static const bool bNoReturn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactCamReturn"));
			if (GCam == ECam::Shot && !bNoReturn && WorldHeld()) { BeginReturn(); return; }
			const float Ts = (float)(Now() - GCamT0);
			AZoogiBall* B = GCamBall.Get();
			if (!B || Ts >= GCamLen) { if (GCam == ECam::Shot && !bNoReturn) BeginReturn(); else EndCam(0.45f); return; }
			const FVector L = B->BallLocation();
			if (GCam == ECam::KO)
			{	// stays on the court, low, turning to watch the Zoogi fly out over the edge
				C->SetActorLocation(GCamPos + (GKOLook - GCamPos).GetSafeNormal() * 140.f * FMath::Clamp(Ts / GCamLen, 0.f, 1.f)); (void)L; // a slow push-in, fixed look (b269)
			}
			else
			{	// rides behind the ball
				FVector V = B->GetBallVelocity(); V.Z = 0.f;
				if (V.Size() < 120.f && Ts > 0.4f) { EndCam(0.45f); return; } // it stopped: back to the match
				const FVector D = V.GetSafeNormal().IsNearlyZero() ? C->GetActorForwardVector().GetSafeNormal2D() : V.GetSafeNormal();
				const FVector Want = L - D * 430.f + FVector(0, 0, 175.f);
				GCamPos = FMath::VInterpTo(GCamPos, Want, 1.f / 60.f, 7.f);
				C->SetActorLocationAndRotation(GCamPos, (L + D * 320.f - GCamPos).Rotation());
			}
		}

		void Confetti(AZoogiGameMode* G, const FLinearColor& Col)
		{
			UWorld* W = G->GetWorld();
			UZoogiShowSys* Sh = W ? W->GetSubsystem<UZoogiShowSys>() : nullptr;
			AZoogiFrozenCrowd* Cr = Sh ? Sh->Crowd.Get() : nullptr;
			if (Cr) { Cr->Cheer = 1.f; Cr->Hype = 1.f; } // the fans jump up
			const FVector Ctr = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			const float KR = G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f;
			for (int32 k = 0; k < 6; ++k)
			{
				FVector P;
				if (!(Cr && Cr->Num() > 0 && Cr->SeatPos(FMath::RandRange(0, Cr->Num() - 1), P)))
				{	// no crowd on this map: bursts around the rim
					const float A = FMath::FRandRange(0.f, 2.f * PI);
					P = Ctr + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * KR * 1.15f + FVector(0, 0, 150.f);
				}
				AZoogiFX::SpawnConfetti(W, P + FVector(0, 0, 80.f), Col, 26);
			}
		}

		void FinalLights(AZoogiGameMode* G, bool bOn)
		{
			if (bOn == GFinal) return;
			GFinal = bOn;
			if (bOn)
			{
				GLights.Reset(); GSkies.Reset();
				for (TActorIterator<AActor> It(G->GetWorld()); It; ++It)
				{
					TInlineComponentArray<UDirectionalLightComponent*> Dl(*It); for (UDirectionalLightComponent* L : Dl) if (L) GLights.Add({ L, L->Intensity });
					TInlineComponentArray<USkyLightComponent*> Sl(*It); for (USkyLightComponent* L : Sl) if (L) GSkies.Add({ L, L->Intensity });
				}
				GDim = 0.f; GLastBeat = -1; GClock0 = Now();
				UE_LOG(LogZoogi, Log, TEXT("SPECTACLE final round: lights dim, the arena pulses on the beat (%d lights)"), GLights.Num() + GSkies.Num());
			}
			else
			{
				for (auto& Pr : GLights) if (UDirectionalLightComponent* L = Pr.Key.Get()) L->SetIntensity(Pr.Value);
				for (auto& Pr : GSkies) if (USkyLightComponent* L = Pr.Key.Get()) L->SetIntensity(Pr.Value);
				GLights.Reset(); GSkies.Reset();
			}
		}
	}

	float SlowMo() { return GSlowEnd > 0.0 && Now() < GSlowEnd ? 0.22f : 0.f; }

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld()) return;
		if (GG.Get() != G)
		{	// a new match world
			GG = G; GWasOut.Reset(); GPrevPhase = -1; GPrevStreak = 0; GSlowEnd = -1.0; GCam = ECam::None; GCamActor.Reset();
			GFinal = false; GLights.Reset(); GSkies.Reset();
		}
		UWorld* W = G->GetWorld();
		const EZoogiPhase Ph = G->Phase;
		const bool bPlay = Ph == EZoogiPhase::Aiming || Ph == EZoogiPhase::Rolling || Ph == EZoogiPhase::Resolving;
		const bool bClassic = !G->bRoyaleMode && !ZoogiTrickShot::Active();
		const FVector Ctr = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
		const float KR = G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f;
		AZoogiBall* Act = G->GetActiveBall();

		// ---- 1 KNOCKOUT MOMENT
		static const bool bNoKO = Flag(TEXT("ZoogiNoKOMoment"));
		if (GWasOut.Num() != G->Balls.Num()) { GWasOut.SetNum(G->Balls.Num()); for (int32 i = 0; i < G->Balls.Num(); ++i) GWasOut[i] = G->Balls[i] && G->Balls[i]->bOut; }
		for (int32 i = 0; i < G->Balls.Num(); ++i)
		{
			AZoogiBall* B = G->Balls[i];
			const bool bOut = B && B->bOut;
			if (bOut && !GWasOut[i] && !bNoKO && bClassic && (Ph == EZoogiPhase::Rolling || Ph == EZoogiPhase::Resolving) && Now() - GLastKO > 3.0)
			{
				GLastKO = Now();
				GSlowEnd = Now() + 0.38; // a 0.3 s freeze-frame feel (real time) at 0.22 speed
				const FVector L = B->BallLocation();
				FVector Dir = L - Ctr; Dir.Z = 0.f; Dir = Dir.GetSafeNormal(); if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
				// b269 (B268 preview: the ball had already dropped below the rim, so the camera stared at the grass): low on the court at
				// floor level + 2 m, looking out over the rim at where it went - the stands behind - and it never tilts down after the ball
				const float FloorZ = Ctr.Z;
				FVector Edge = Ctr + Dir * KR; Edge.Z = FloorZ;
				const FVector Pos = Edge - Dir * 1500.f + FVector(0.f, 0.f, 480.f); // b270: further back and higher - the ring-out burst at the edge filled the whole screen at 9 m
				GKOLook = Edge + Dir * 700.f + FVector(0.f, 0.f, 260.f);
				StartCam(G, ECam::KO, B, Pos, GKOLook, 1.25f);
				Confetti(G, ZoogiUtil::PlayerColor(Act && Act != B ? Act->PlayerIndex : i));
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.55f, 0.85f);
				UE_LOG(LogZoogi, Log, TEXT("SPECTACLE knockout moment: P%d %s out (slow-mo, low camera, crowd up, confetti)"), i + 1, *B->Def.Name);
			}
			if (GWasOut.IsValidIndex(i)) GWasOut[i] = bOut;
		}

		// ---- 2 SHOT CAM
		static const bool bNoShot = Flag(TEXT("ZoogiNoShotCam"));
		if (Ph == EZoogiPhase::Aiming && G->Preview.bValid) GAimFrac = G->Preview.Fraction;
		if (GPrevPhase == (int32)EZoogiPhase::Aiming && Ph == EZoogiPhase::Rolling && !bNoShot && bClassic && Act && !Act->bCPU && GAimFrac >= 0.7f && GCam == ECam::None)
		{
			FVector V = Act->GetBallVelocity(); V.Z = 0.f;
			const FVector D = V.GetSafeNormal().IsNearlyZero() ? (Act->BallLocation() - Ctr).GetSafeNormal2D() : V.GetSafeNormal();
			const FVector L = Act->BallLocation();
			if (StartCam(G, ECam::Shot, Act, L - D * 430.f + FVector(0, 0, 175.f), L + D * 320.f, 1.4f))
				UE_LOG(LogZoogi, Log, TEXT("SPECTACLE shot cam: riding behind %s (power %.0f%%)"), *Act->Def.Name, GAimFrac * 100.f);
		}
		if (Ph == EZoogiPhase::Aiming && GCam == ECam::Shot) EndCam(0.3f); // a new turn: never keep the ride cam
		TickCam();

		// ---- 3 CHAIN LIGHTS
		static const bool bNoChain = Flag(TEXT("ZoogiNoChainLights"));
		const int32 Streak = ZoogiScoringUI::ComboStreak();
		if (!bNoChain && bPlay && Streak >= 2 && Streak > GPrevStreak)
		{
			for (int32 s = FMath::Max(2, GPrevStreak + 1); s <= Streak; ++s)
			{
				const int32 k = s - 2;
				const float R = KR * FMath::Min(1.f, 0.3f + 0.17f * k);
				AZoogiFX::Spawn(W, Ctr + FVector(0, 0, 6.f), FLinearColor(1.f, 0.78f, 0.2f), R, 0.5f, true, 9.f + 2.f * k);
				ZoogiUtil::PlaySfx(G, TEXT("Score"), 0.5f, FMath::Min(2.f, 1.f + 0.12f * k));
			}
		}
		GPrevStreak = Streak;

		// ---- 4 FINAL ROUND
		static const bool bNoFinal = Flag(TEXT("ZoogiNoFinalPulse"));
		const bool bFinalNow = !bNoFinal && bClassic && !ZoogiRoyaleMix::Active() && G->MenuRounds > 1 && G->RoundNumber >= G->MenuRounds && bPlay;
		FinalLights(G, bFinalNow);
		if (GFinal)
		{
			GDim = FMath::Min(1.f, GDim + RealDt / 1.2f);
			int32 Th = 0; double E = 0.0;
			const UZoogiMusicSubsystem* Mu = W->GetSubsystem<UZoogiMusicSubsystem>();
			const double Period = 60.0 / 158.45; // the arena themes' tempo (b182 beat maps)
			if (!(Mu && Mu->ThemeClock(Th, E))) E = Now() - GClock0;
			const int64 Beat = (int64)FMath::FloorToDouble(E / Period);
			if (Beat != GLastBeat)
			{
				GLastBeat = Beat;
				GBeatGlow = 1.f;
				const bool bBar = (Beat % 4) == 0;
				AZoogiFX::Spawn(W, Ctr + FVector(0, 0, 6.f), FLinearColor(1.f, 0.72f, 0.2f), KR * (bBar ? 1.02f : 0.97f), 0.32f, true, bBar ? 7.f : 4.f);
			}
			GBeatGlow = FMath::Max(0.f, GBeatGlow - RealDt * 4.f);
			const float K = FMath::Lerp(1.f, 0.5f, GDim) * (1.f + 0.25f * GBeatGlow * GDim);
			for (auto& Pr : GLights) if (UDirectionalLightComponent* L = Pr.Key.Get()) L->SetIntensity(Pr.Value * K);
			for (auto& Pr : GSkies) if (USkyLightComponent* L = Pr.Key.Get()) L->SetIntensity(Pr.Value * FMath::Lerp(1.f, 0.6f, GDim));
		}
		GPrevPhase = (int32)Ph;
	}
}
