// Frozen Ring verification captures (Oct 6):
//  -ZoogiFormShots    frames of the stage-forming intro (slowed x4): overview sweep, then a replay framed on the hub pillars
//                     rising through the floor.
//  -ZoogiDamageShots  after forming, freezes the match and steps every destructible piece (walls, arches, stands, rim
//                     ice, ramp, platform, obelisks, crystal bumpers, ice boulders, the reactive floor) through
//                     INTACT -> DAMAGED -> CRACKED -> BROKEN, one capture per piece group and state, then restores all.
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiFloorScars.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiCombo.h"
#include "ZoogiRock.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	struct FShotView { const TCHAR* Name; FVector Look; float Yaw, Pitch, Dist, Fov; };
	FVector SPol(float R, float YawDeg, float Z = 0.f)
	{
		const float A = FMath::DegreesToRadians(YawDeg);
		return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
	}
	TArray<FShotView> DamageViews(const AZoogiArena* Arena, bool bCombo)
	{
		using namespace ZoogiFrozen;
		TArray<FShotView> V;
		V.Add({ TEXT("overview"), FVector(0.f, 0.f, 0.f), 270.f, 64.f, 6300.f, 62.f }); // = Buzzy damaged.png framing
		V.Add({ TEXT("walls"), SPol(WallR, 281.f, 50.f), 281.f, 24.f, 950.f, 70.f });
		V.Add({ TEXT("gate_arch"), SPol(ArchR, GateYaw, 110.f), GateYaw, 16.f, 1150.f, 70.f });
		V.Add({ TEXT("stands"), SPol(IsletDist[2], IsletYaw[2], 80.f), IsletYaw[2], 22.f, 1600.f, 70.f });
		V.Add({ TEXT("hub"), SPol(HubR, 45.f, 90.f), 45.f, 18.f, 800.f, 70.f });
		V.Add({ TEXT("ramp_platform"), FVector(40.f, -860.f, 30.f), 270.f, 34.f, 1800.f, 70.f });
		FVector RockAt = SPol(1400.f, RockYaw[0], 40.f);
		if (Arena && Arena->GetRocks().Num() > 0 && Arena->GetRocks()[0]) RockAt = Arena->GetRocks()[0]->GetActorLocation() + FVector(0, 0, 40.f);
		const float RockYawNow = FMath::RadiansToDegrees(FMath::Atan2(RockAt.Y, RockAt.X));
		V.Add({ TEXT("rim_rock"), RockAt, RockYawNow, 24.f, 850.f, 70.f });
		V.Add({ TEXT("floor"), FVector(0.f, 0.f, 0.f), 270.f, 62.f, 2300.f, 60.f });
		if (bCombo) V.Add({ TEXT("bumper_section"), FVector(-900.f, 600.f, 30.f), 140.f, 30.f, 1300.f, 70.f });
		return V;
	}
}

void AZoogiGameMode::TickFormShots()
{
	if (!Arena || !Arena->Forming) return;
	static const float Fracs[] = { 0.02f, 0.10f, 0.18f, 0.26f, 0.34f, 0.42f, 0.52f, 0.62f, 0.74f, 0.88f };
	const float P = Arena->Forming->GetProgress();
	const int32 N = UE_ARRAY_COUNT(Fracs);
	while (FormShotIdx < N && P >= Fracs[FormShotIdx])
	{
		const FString Name = FString::Printf(TEXT("%s_%02d_p%03d"), FormPass == 0 ? TEXT("form") : TEXT("pillar"), FormShotIdx, FMath::RoundToInt(Fracs[FormShotIdx] * 100.f));
		// The world keeps running (no freeze): the frame is whatever renders next, close to this progress.
		TakeShot(Name);
		UE_LOG(LogZoogi, Log, TEXT("Forming shot %s at progress %.2f"), *Name, P);
		++FormShotIdx;
	}
}

void AZoogiGameMode::TickDamageShots(float Dt)
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!W || !PC || !Arena || ShotDir.IsEmpty()) return;
	const double Now = FPlatformTime::Seconds();
	const bool bCombo = ComboArena != nullptr;
	const TArray<FShotView> Views = DamageViews(Arena, bCombo);
	const int32 NV = Views.Num();
	const int32 Total = NV * 4;

	auto ApplyState = [this, bCombo](int32 S)
	{
		const EZoogiPieceState PS = (EZoogiPieceState)FMath::Clamp(S, 0, 3);
		int32 Count = 0;
		auto Step = [&](AZoogiArenaPiece* P)
		{
			if (!P || P->IsIndestructible()) return;
			P->ResetPiece();
			if (S > 0) P->SetPieceState(PS, P->GetActorLocation() + FVector(0, 0, 60.f), P->GetActorLocation().GetSafeNormal2D(), S >= 2);
			++Count;
		};
		for (AZoogiArenaPiece* P : Arena->GetPieces()) Step(P);
		if (bCombo && ComboArena)
		{
			for (AZoogiArenaPiece* P : ComboArena->GetIceBumpers()) Step(P);
			ComboArena->ForceFloorState(S == 0 ? EZoogiFloorState::Normal : (S == 1 ? EZoogiFloorState::Damaged : EZoogiFloorState::Cracked));
		}
		for (AZoogiRock* R : Arena->GetRocks()) if (R) R->ForceStage(S);
		if (AZoogiFloorScars* Sc = Arena->GetScars())
		{
			Sc->ResetScars();
			if (S > 0)
			{
				Sc->SetScar(FVector(330.f, -260.f, 0.f), S, 1.2f);
				Sc->SetScar(FVector(-420.f, 230.f, 0.f), S, 1.f);
				Sc->SetScar(FVector(120.f, 470.f, 0.f), FMath::Max(1, S - 1), 0.9f);
				Sc->SetScar(FVector(-150.f, -520.f, 0.f), S, 1.4f);
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("Damage shots: %d pieces -> %s (rocks stage %d, scars stage %d); the 4 hub pillars stay indestructible"), Count, *AZoogiArenaPiece::StateName(PS), S, S);
	};

	if (DamageShotStep == 0)
	{
		if (Phase == EZoogiPhase::Menu || Phase == EZoogiPhase::Forming) { DamageShotReal = Now; return; }
		if (Now - DamageShotReal < 1.5) return;
		W->GetWorldSettings()->SetTimeDilation(0.0001f);
		ShotCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		if (ShotCam)
		{
			UCameraComponent* C = ShotCam->GetCameraComponent();
			C->bConstrainAspectRatio = false;
			// Time is dilated to ~0 while capturing, and eye adaptation runs on world time: make it adapt instantly instead
			// of staying stuck on the exposure of the previous view (the first captures came out blown out).
			C->PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
			C->PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
			C->PostProcessSettings.AutoExposureSpeedUp = 2.0e6f;
			C->PostProcessSettings.AutoExposureSpeedDown = 2.0e6f;
			C->PostProcessBlendWeight = 1.f;
		}
		if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
		DamageShotStep = 1;
		DamageShotReal = Now;
		ApplyState(0);
		UE_LOG(LogZoogi, Log, TEXT("Damage shots: %d views x 4 states"), NV);
		return;
	}
	const int32 Idx = DamageShotStep - 1;
	if (Idx >= Total)
	{
		ApplyState(0);
		Arena->ResetPieces();
		if (ComboArena) ComboArena->ForceFloorState(EZoogiFloorState::Normal);
		for (AZoogiRock* R : Arena->GetRocks()) if (R) R->ResetRock();
		W->GetWorldSettings()->SetTimeDilation(1.f);
		if (ShotCam) { ShotCam->Destroy(); ShotCam = nullptr; }
		if (APawn* Pw = PC->GetPawn()) PC->SetViewTarget(Pw);
		if (PC->MyHUD) PC->MyHUD->bShowHUD = true;
		bDamageShots = false;
		UE_LOG(LogZoogi, Log, TEXT("Damage shots: done, everything restored"));
		return;
	}
	const int32 S = Idx / NV, Vi = Idx % NV;
	const FShotView& V = Views[Vi];
	if (ShotCam)
	{
		const FRotator Heading(0.f, V.Yaw, 0.f);
		const float Pr = FMath::DegreesToRadians(V.Pitch);
		const FVector Loc = V.Look - Heading.Vector() * V.Dist * FMath::Cos(Pr) + FVector(0, 0, V.Dist * FMath::Sin(Pr));
		ShotCam->SetActorLocation(Loc);
		ShotCam->SetActorRotation((V.Look - Loc).Rotation());
		ShotCam->GetCameraComponent()->SetFieldOfView(V.Fov);
		PC->SetViewTarget(ShotCam);
	}
	const double T = Now - DamageShotReal;
	if (T > 1.6 && !ShotsTaken.Contains(FString::Printf(TEXT("dmg_%d_%d"), S, Vi)))
	{
		ShotsTaken.Add(FString::Printf(TEXT("dmg_%d_%d"), S, Vi));
		const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("%02d_dmg_%d_%s_%s.png"), ShotSeq++, S, *AZoogiArenaPiece::StateName((EZoogiPieceState)S).ToLower(), V.Name));
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		UE_LOG(LogZoogi, Log, TEXT("Screenshot requested: %s"), *Path);
	}
	if (T > 2.2)
	{
		++DamageShotStep;
		DamageShotReal = Now;
		if ((DamageShotStep - 1) < Total && (DamageShotStep - 1) % NV == 0) ApplyState((DamageShotStep - 1) / NV);
	}
}
