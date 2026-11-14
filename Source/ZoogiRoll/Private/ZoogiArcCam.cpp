// Nov 1 b59 (Evan): ARC launch flight camera - see ZoogiArcCam.h.
#include "ZoogiArcCam.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "ZoogiTypes.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	struct FArc
	{
		TWeakObjectPtr<ACameraActor> Cam;
		TWeakObjectPtr<AActor> Prev;
		TWeakObjectPtr<AZoogiBall> Ball;
		FVector Dir = FVector::ForwardVector;
		float Side = 1.f, Age = 0.f, Flight = 1.f, Hold = 0.f;
		bool bOn = false;
		int32 Shots = 0;
	};
	FArc GArcS;

	void Stop(UWorld* W)
	{
		if (ACameraActor* C = GArcS.Cam.Get())
		{
			if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
				if (PC->GetViewTarget() == C) PC->SetViewTargetWithBlend(GArcS.Prev.IsValid() ? GArcS.Prev.Get() : PC->GetPawn(), 0.45f, VTBlend_Cubic);
			C->Destroy();
		}
		GArcS = FArc();
	}

	// camera spot for sweep fraction K (0 = behind the Zoogi, 1 = 160 deg round, in front of it)
	FVector Spot(const FVector& At, float K)
	{
		const float E = K * K * (3.f - 2.f * K); // smoothstep
		const float Ang = FMath::DegreesToRadians(180.f - 160.f * E) * GArcS.Side;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, GArcS.Dir);
		const FVector Off = (GArcS.Dir * FMath::Cos(Ang) + Right * FMath::Sin(Ang)).GetSafeNormal2D();
		const float Dist = FMath::Lerp(980.f, 430.f, E), Up = FMath::Lerp(400.f, 130.f, E);
		return At + Off * Dist + FVector(0.f, 0.f, Up);
	}
}

namespace ZoogiArcCam
{
	bool Active() { return GArcS.bOn; }

	void Start(AZoogiGameMode* G, AZoogiBall* B, const FVector2D& Dir, float Flight)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoArcCam"));
		if (bOff || !G || !B) return;
		UWorld* W = G->GetWorld();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (!PC) return;
		if (GArcS.bOn) Stop(W);
		GArcS.Dir = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
		if (GArcS.Dir.IsNearlyZero()) return;
		GArcS.Ball = B; GArcS.Flight = FMath::Clamp(Flight, 0.5f, 3.f); GArcS.Age = 0.f; GArcS.Hold = 0.f;
		// swing round the side the current view is already on (no flip through the Zoogi)
		const FVector View = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : B->BallLocation() - GArcS.Dir * 500.f;
		GArcS.Side = FVector::CrossProduct(GArcS.Dir, View - B->BallLocation()).Z >= 0.f ? 1.f : -1.f;
		if (UZoogiEpisodeCameraDirector* CD = UZoogiEpisodeCameraDirector::Get(G)) CD->StopAllShots(true);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector P0 = Spot(B->BallLocation(), 0.f);
		ACameraActor* C = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((B->BallLocation() + GArcS.Dir * 300.f - P0).Rotation(), P0), SP);
		if (!C) return;
		if (C->GetCameraComponent()) { C->GetCameraComponent()->SetFieldOfView(70.f); C->GetCameraComponent()->SetConstraintAspectRatio(false); }
		GArcS.Cam = C; GArcS.Prev = PC->GetViewTarget(); GArcS.bOn = true;
		PC->SetViewTargetWithBlend(C, 0.18f, VTBlend_EaseOut);
		ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.8f);
		UE_LOG(LogZoogi, Log, TEXT("ARC cam: P%d flight %.2f s - 160 deg sweep from behind to the front (side %+.0f)"), B->PlayerIndex + 1, GArcS.Flight, GArcS.Side);
	}

	void Tick(AZoogiGameMode* G, float Dt)
	{
		if (!GArcS.bOn || !G) return;
		UWorld* W = G->GetWorld();
		AZoogiBall* B = GArcS.Ball.Get();
		ACameraActor* C = GArcS.Cam.Get();
		if (!B || !C || B->bOut || G->Phase != EZoogiPhase::Rolling) { Stop(W); return; }
		GArcS.Age += Dt;
		const float Total = GArcS.Flight + 0.25f;
		const float K = FMath::Clamp(GArcS.Age / Total, 0.f, 1.f);
		const FVector At = B->BallLocation();
		const FVector Want = Spot(At, K);
		C->SetActorLocation(FMath::VInterpTo(C->GetActorLocation(), Want, Dt, 10.f));
		const FVector Look = At + GArcS.Dir * FMath::Lerp(260.f, 0.f, K) + FVector(0.f, 0.f, 20.f);
		C->SetActorRotation(FMath::RInterpTo(C->GetActorRotation(), (Look - C->GetActorLocation()).Rotation(), Dt, 12.f));
		if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetViewTarget() != C && GArcS.Age > 0.25f) PC->SetViewTarget(C); // hold it against the other camera systems
		if (!G->ShotDir.IsEmpty() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiArcTest")))
		{
			static const float Marks[3] = { 0.15f, 0.5f, 0.9f };
			if (GArcS.Shots < 3 && K >= Marks[GArcS.Shots]) G->TakeShot(FString::Printf(TEXT("arccam_%d"), GArcS.Shots++));
		}
		if (K >= 1.f) { if (GArcS.Hold <= 0.f) ZoogiUtil::PlaySfx(G, TEXT("Land"), 0.9f); GArcS.Hold += Dt; if (GArcS.Hold > 0.35f) Stop(W); }
	}
}
