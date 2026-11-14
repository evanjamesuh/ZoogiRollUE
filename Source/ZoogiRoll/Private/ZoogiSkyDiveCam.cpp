// b144c: SKY DIVE camera - see ZoogiSkyDiveCam.h.
#include "ZoogiSkyDiveCam.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiPhysics.h"
#include "ZoogiArcCam.h"
#include "ZoogiArena.h"
#include "ZoogiSkills.h"
#include "ZoogiEdgeHang.h"
#include "ZoogiActionFun.h"
#include "ZoogiHaptics.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	enum class EPh : uint8 { Off, Swoop, Dive, Land };
	struct FSky
	{
		TWeakObjectPtr<ACameraActor> Cam;
		TWeakObjectPtr<AActor> Prev;
		TWeakObjectPtr<AZoogiBall> Ball;
		EPh Ph = EPh::Off;
		float Age = 0.f, PhAge = 0.f, GroundZ = 0.f, LastZ = 0.f, Fov = 72.f;
		FVector Dir = FVector::ForwardVector, LandAt = FVector::ZeroVector;
		int32 Shots = 0;
	};
	FSky GSky;
	// per ball: the last floor height it rolled on, when it last sat on a launch pad, the turn it last dived
	TMap<TWeakObjectPtr<AZoogiBall>, float> GFloorZ;
	TMap<TWeakObjectPtr<AZoogiBall>, double> GPadAt;
	TMap<TWeakObjectPtr<AZoogiBall>, int32> GDoneTurn;
	int32 GTestTurn = -1; bool bTestDone = false;

	float TriggerH() { static float H = -1.f; if (H < 0.f) { H = 420.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiSkyDiveH="), H); } return H; }

	void Stop(UWorld* W, const TCHAR* Why)
	{
		if (ACameraActor* C = GSky.Cam.Get())
		{
			if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
				if (PC->GetViewTarget() == C) PC->SetViewTargetWithBlend(GSky.Prev.IsValid() ? GSky.Prev.Get() : PC->GetPawn(), 0.5f, VTBlend_Cubic);
			C->Destroy();
		}
		if (GSky.Ph != EPh::Off) UE_LOG(LogZoogi, Log, TEXT("SKYDIVE cam off (%s) after %.2f s"), Why, GSky.Age);
		GSky = FSky();
	}

	// where the fall ends: the ball's floor height, along its current flight under gravity
	FVector Landing(UWorld* W, const AZoogiBall* B)
	{
		const FVector P = B->BallLocation(), V = B->GetBallVelocity();
		const float Gz = FMath::Min(-200.f, W ? W->GetGravityZ() : -980.f);
		const float H = P.Z - (GSky.GroundZ + ZoogiConst::ZoogiRadius);
		// z(t) = H + Vz t + Gz/2 t^2 = 0
		const float A = 0.5f * Gz, Bq = V.Z, Cq = H;
		const float Disc = Bq * Bq - 4.f * A * Cq;
		float T = Disc > 0.f ? (-Bq - FMath::Sqrt(Disc)) / (2.f * A) : 0.5f;
		T = FMath::Clamp(T, 0.f, 3.f);
		return FVector(P.X + V.X * T, P.Y + V.Y * T, GSky.GroundZ + ZoogiConst::ZoogiRadius);
	}

	void Start(AZoogiGameMode* G, AZoogiBall* B)
	{
		UWorld* W = G->GetWorld();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC) return;
		GSky = FSky();
		GSky.Ball = B; GSky.Ph = EPh::Swoop; GSky.GroundZ = GFloorZ.FindRef(B); GSky.LastZ = B->BallLocation().Z;
		const FVector V = B->GetBallVelocity();
		GSky.Dir = FVector(V.X, V.Y, 0.f).GetSafeNormal();
		if (GSky.Dir.IsNearlyZero()) GSky.Dir = FRotator(0.f, B->AimYaw, 0.f).Vector();
		GSky.LandAt = Landing(W, B);
		GDoneTurn.Add(B, G->TurnCounter);
		if (UZoogiEpisodeCameraDirector* CD = UZoogiEpisodeCameraDirector::Get(G)) CD->StopAllShots(true);
		const FVector From = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : B->BallLocation() - GSky.Dir * 800.f + FVector(0, 0, 400.f);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* C = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((B->BallLocation() - From).Rotation(), From), SP);
		if (!C) { GSky = FSky(); return; }
		if (UCameraComponent* CC = C->GetCameraComponent()) { CC->SetFieldOfView(GSky.Fov); CC->SetConstraintAspectRatio(false); }
		GSky.Cam = C; GSky.Prev = PC->GetViewTarget();
		PC->SetViewTargetWithBlend(C, 0.2f, VTBlend_EaseOut);
		ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.9f, 0.85f);
		if (!B->bCPU) ZoogiHaptics::Kick(0.5f, W, TEXT("sky high"));
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("SKY HIGH!"), FString::Printf(TEXT("P%d %s DIVES BACK IN"), B->PlayerIndex + 1, *B->Def.Name.ToUpper()), ZoogiUtil::PlayerColor(B->PlayerIndex), 1.5f, 1.25f);
		UE_LOG(LogZoogi, Log, TEXT("SKYDIVE P%d is %.0f cm over its floor (vz %.0f) - swoop up, slow-mo, dive to (%.0f, %.0f)"), B->PlayerIndex + 1, B->BallLocation().Z - GSky.GroundZ, V.Z, GSky.LandAt.X, GSky.LandAt.Y);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoltAirTest")))
		{	// b152a test: bolts fired from the air
			ZoogiSkills::ArmBolts(G, B->PlayerIndex, 3);
			const FVector V0 = B->GetBallVelocity();
			const bool bOk = ZoogiSkills::BoltGo(G);
			UE_LOG(LogZoogi, Log, TEXT("BOLT AIR TEST: fired %d, caster vz %.0f -> %.0f"), bOk ? 1 : 0, V0.Z, B->GetBallVelocity().Z);
			G->TakeShotDelayed(TEXT("bolt_air_a"), 0.15f); G->TakeShotDelayed(TEXT("bolt_air_b"), 0.5f);
		}
	}
}

namespace ZoogiSkyDiveCam
{
	bool Active() { return GSky.Ph != EPh::Off; }

	float SlowMo()
	{
		if (GSky.Ph != EPh::Swoop && GSky.Ph != EPh::Dive) return 0.f;
		// 0.9 s of real time at 0.3x, then back up to full speed over 0.35 s
		if (GSky.Age < 0.9f) return 0.3f;
		if (GSky.Age < 1.25f) return FMath::Lerp(0.3f, 1.f, (GSky.Age - 0.9f) / 0.35f);
		return 0.f;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSkyDive")) || (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkyDive")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkyDiveTest")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoltAirTest"))); // b154 (Evan: "remove the zoom in camera that happens during air flight"): off unless -ZoogiSkyDive
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkyDiveTest"));
		if (!G) return;
		UWorld* W = G->GetWorld();
		const bool bPlay = G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving;
		// floor heights + pad times for every Zoogi in play
		const double Now = W ? W->GetRealTimeSeconds() : 0.0;
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bOut || !B->Sphere) continue;
			if (B->IsOnLaunchPad()) GPadAt.Add(B, Now);
			if (ZoogiPhysics::IsGrounded(B->Sphere, ZoogiConst::ZoogiRadius) && B->BallLocation().Z > -200.f) GFloorZ.Add(B, B->BallLocation().Z - ZoogiConst::ZoogiRadius);
		}
		if (GSky.Ph == EPh::Off)
		{
			if (bOff || !bPlay || G->bRoyaleMode || ZoogiArcCam::Active() || ZoogiEdgeHang::Active()) return;
			AZoogiBall* B = G->GetActiveBall();
			if (!B || B->bOut || !B->Sphere || !GFloorZ.Contains(B)) return;
			if (bTest && !bTestDone && !B->bCPU && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.35f)
			{	// -ZoogiSkyDiveTest: pop the human's first roll up into the air
				bTestDone = true;
				const FVector V = B->GetBallVelocity();
				B->SetPlanarVelocity(FVector(V.X, V.Y, 0.f) * 0.6f, 1500.f);
				UE_LOG(LogZoogi, Log, TEXT("SKYDIVE TEST: P%d popped up (vz 1500)"), B->PlayerIndex + 1);
			}
			if (const int32* T = GDoneTurn.Find(B)) if (*T == G->TurnCounter) return;
			if (const double* P = GPadAt.Find(B)) if (Now - *P < 3.0) return; // a launch pad flight has its own camera
			const float Over = B->BallLocation().Z - GFloorZ.FindRef(B) - ZoogiConst::ZoogiRadius;
			if (Over > TriggerH() && !ZoogiPhysics::IsGrounded(B->Sphere, ZoogiConst::ZoogiRadius) && G->Arena && !G->Arena->IsOffCourt(FVector2D(B->BallLocation().X, B->BallLocation().Y))) Start(G, B); // b149b: only over the arena
			return;
		}
		AZoogiBall* B = GSky.Ball.Get();
		ACameraActor* C = GSky.Cam.Get();
		if (!B || !C) { Stop(W, TEXT("gone")); return; }
		if (B->bOut) { Stop(W, TEXT("out")); return; }
		if (G->Arena && G->Arena->IsOffCourt(FVector2D(B->BallLocation().X, B->BallLocation().Y))) { Stop(W, TEXT("left the arena")); return; } // b149b (Evan): the aerial cam only inside the arena
		if (!bPlay && GSky.Ph != EPh::Land) { Stop(W, TEXT("phase")); return; }
		if (ZoogiArcCam::Active() || ZoogiEdgeHang::Active()) { Stop(W, TEXT("other cam")); return; }
		GSky.Age += RealDt; GSky.PhAge += RealDt;
		if (GSky.Age > 7.f) { Stop(W, TEXT("timeout")); return; }
		const FVector P = B->BallLocation();
		const float GDt = RealDt * (W->GetWorldSettings() ? W->GetWorldSettings()->GetEffectiveTimeDilation() : 1.f);
		const float Vz = GDt > 1e-5f ? (P.Z - GSky.LastZ) / GDt : 0.f; // game-time vertical speed (scripted lobs have no physics velocity)
		GSky.LastZ = P.Z;
		const bool bGround = ZoogiPhysics::IsGrounded(B->Sphere, ZoogiConst::ZoogiRadius) || P.Z < GSky.GroundZ + ZoogiConst::ZoogiRadius + 30.f;
		{ const FVector V = B->GetBallVelocity(); const FVector D(V.X, V.Y, 0.f); if (D.SizeSquared() > 100.f * 100.f) GSky.Dir = FMath::Lerp(GSky.Dir, D.GetSafeNormal(), FMath::Min(1.f, RealDt * 3.f)).GetSafeNormal(); }
		if (GSky.Ph != EPh::Land) GSky.LandAt = Landing(W, B);
		FVector Want, Look; float WantFov = 72.f;
		if (GSky.Ph == EPh::Swoop)
		{	// up above and behind it, looking down on the Zoogi with the arena below
			Want = P - GSky.Dir * 520.f + FVector(0.f, 0.f, 560.f);
			Look = FMath::Lerp(P, GSky.LandAt, 0.2f);
			WantFov = 76.f;
			if ((Vz < -60.f && GSky.PhAge > 0.35f) || GSky.PhAge > 2.2f) { GSky.Ph = EPh::Dive; GSky.PhAge = 0.f; ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.8f, 1.2f); UE_LOG(LogZoogi, Log, TEXT("SKYDIVE dive: P%d falls toward (%.0f, %.0f)"), B->PlayerIndex + 1, GSky.LandAt.X, GSky.LandAt.Y); }
		}
		else if (GSky.Ph == EPh::Dive)
		{	// ride it down from just above, looking past it at the landing spot - the FOV opens with the speed
			Want = P - GSky.Dir * 340.f + FVector(0.f, 0.f, 300.f);
			Look = FMath::Lerp(P, GSky.LandAt, 0.55f);
			WantFov = FMath::Lerp(76.f, 92.f, FMath::Clamp(-Vz / 1600.f, 0.f, 1.f));
			if (bGround)
			{
				GSky.Ph = EPh::Land; GSky.PhAge = 0.f; GSky.LandAt = FVector(P.X, P.Y, GSky.GroundZ);
				ZoogiUtil::PlaySfx(G, TEXT("Land"), 1.f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), GSky.LandAt + FVector(0, 0, 10.f), FRotator::ZeroRotator, 1.2f);
				AZoogiFX::Spawn(W, GSky.LandAt + FVector(0, 0, 6.f), ZoogiUtil::PlayerColor(B->PlayerIndex), 320.f, 0.6f, true, 10.f);
				if (!B->bCPU) ZoogiHaptics::Kick(0.8f, W, TEXT("sky dive landing"));
				UE_LOG(LogZoogi, Log, TEXT("SKYDIVE landed: P%d at (%.0f, %.0f) after %.2f s"), B->PlayerIndex + 1, P.X, P.Y, GSky.Age);
			}
		}
		else
		{	// landing thump: a short shake, then back to the match camera
			const float Sh = FMath::Max(0.f, 1.f - GSky.PhAge / 0.4f) * 22.f;
			Want = P - GSky.Dir * 420.f + FVector(0.f, 0.f, 340.f) + FVector(FMath::FRandRange(-Sh, Sh), FMath::FRandRange(-Sh, Sh), FMath::FRandRange(-Sh, Sh));
			Look = P;
			WantFov = 80.f;
			if (GSky.PhAge > 0.5f) { Stop(W, TEXT("landed")); return; }
		}
		const float Rate = GSky.Ph == EPh::Land ? 30.f : GSky.Ph == EPh::Dive ? 9.f : 6.f;
		C->SetActorLocation(FMath::VInterpTo(C->GetActorLocation(), Want, RealDt, Rate));
		C->SetActorRotation(FMath::RInterpTo(C->GetActorRotation(), (Look - C->GetActorLocation()).Rotation(), RealDt, 10.f));
		GSky.Fov = FMath::FInterpTo(GSky.Fov, WantFov, RealDt, 4.f);
		if (UCameraComponent* CC = C->GetCameraComponent()) CC->SetFieldOfView(GSky.Fov);
		if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetViewTarget() != C && GSky.Age > 0.25f) PC->SetViewTarget(C); // hold it against the other camera systems
		if (bTest && !G->ShotDir.IsEmpty())
		{
			const bool bShot = (GSky.Shots == 0 && GSky.Age > 0.5f) || (GSky.Shots == 1 && GSky.Ph == EPh::Dive && GSky.PhAge > 0.3f) || (GSky.Shots == 2 && GSky.Ph == EPh::Land);
			if (bShot) G->TakeShot(FString::Printf(TEXT("skydive_%d"), GSky.Shots++));
		}
	}
}
