// Frozen Ring round 3c gameplay checks (-ZoogiFrozenPlayTest). See ZoogiFrozenPlayTest.h.
#include "ZoogiFrozenPlayTest.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiBall.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	FVector TPol(float R, float YawDeg, float Z) { const float A = FMath::DegreesToRadians(YawDeg); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); }
	constexpr float NormalShot = 0.55f * ZoogiConst::FullLaunchSpeed; // ~1980 cm/s
	constexpr float BridgeShot = NormalShot;                           // bridge_k: a normal shot must carry a Zoogi to the islet orb
}

AZoogiFrozenPlayTest::AZoogiFrozenPlayTest()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

int32 AZoogiFrozenPlayTest::CountDamaged(FName Kind, const FVector& Near, float Radius) const
{
	int32 N = 0;
	if (!Arena.IsValid()) return 0;
	for (const AZoogiArenaPiece* P : Arena->GetPieces())
		if (P && P->GetKind() == Kind && P->GetPieceState() != EZoogiPieceState::Intact && FVector::Dist2D(P->GetActorLocation(), Near) < Radius) ++N;
	return N;
}

void AZoogiFrozenPlayTest::View(const FVector& Look, float Yaw, float Pitch, float Dist)
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	if (!Cam) Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
	if (!Cam) return;
	const FRotator H(0.f, Yaw, 0.f);
	const float Pr = FMath::DegreesToRadians(Pitch);
	const FVector Loc = Look - H.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation((Look - Loc).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(70.f);
	PC->SetViewTarget(Cam);
}

void AZoogiFrozenPlayTest::Shot(const TCHAR* Name)
{
	if (ShotDir.IsEmpty()) return;
	const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("playtest_%d_%s.png"), Step, Name));
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	UE_LOG(LogZoogi, Log, TEXT("Screenshot requested: %s"), *Path);
}

void AZoogiFrozenPlayTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	using namespace ZoogiFrozen;
	UWorld* W = GetWorld();
	if (!W || !Arena.IsValid() || Step > 11) return;
	const double Now = W->GetTimeSeconds();
	if (Step == 0)
	{
		const bool bFormed = !Arena->Forming || Arena->Forming->GetProgress() >= 1.f;
		if (!bFormed) { StepStart = Now; return; }
		if (Now - StepStart < 2.0) return;
		for (TActorIterator<AZoogiBall> It(W); It; ++It) if (It->PlayerIndex == 3) TestBall = *It;
		if (!TestBall.IsValid()) for (TActorIterator<AZoogiBall> It(W); It; ++It) TestBall = *It;
		if (!TestBall.IsValid()) { UE_LOG(LogZoogi, Warning, TEXT("PLAYTEST: no Zoogi found")); Step = 99; return; }
		Step = 1; StepStart = Now; bShot = false; Before = -1;
	}
	AZoogiBall* B = TestBall.Get();
	if (!B) { Step = 99; return; }
	const double T = Now - StepStart;
	const float R = ZoogiConst::ZoogiRadius + 1.f;
	auto Next = [&](const FString& Res) { Results.Add(Res); UE_LOG(LogZoogi, Log, TEXT("PLAYTEST %s"), *Res); ++Step; StepStart = Now; bShot = false; Before = -1; };
	auto IsletLocal = [&](int32 k, float Lx, float Ly, float Lz) { return TPol(IsletD(k), IsletYaw[k], IsletTopZ) + FRotator(0.f, IsletYaw[k] + 180.f, 0.f).RotateVector(FVector(Lx, Ly, Lz)); };
	switch (Step)
	{
	case 1: // wall: normal shot into the N ricochet wall
	{
		const float A = (WallNA0 + WallNA1) * 0.5f;
		if (Before < 0)
		{
			Before = CountDamaged(TEXT("icewall"), TPol(WallR, A, 0.f), 400.f);
			B->PlaceAt(TPol(WallR - 420.f, A, R), A);
			B->Launch(FVector2D(TPol(1.f, A, 0.f)) * NormalShot);
			View(TPol(WallR, A, 40.f), A, 22.f, 1100.f);
		}
		if (T > 1.0 && !bShot) { bShot = true; Shot(TEXT("wall")); }
		if (T > 1.6)
		{
			const int32 N = CountDamaged(TEXT("icewall"), TPol(WallR, A, 0.f), 400.f);
			Next(FString::Printf(TEXT("wall: normal shot %.0f cm/s -> %d wall segment(s) damaged or worse (before %d) %s"), NormalShot, N, Before, N > Before ? TEXT("PASS") : TEXT("FAIL")));
		}
		break;
	}
	case 2: // stand: normal shot from islet 0's apron into the bleachers
	{
		const FVector Ctr = TPol(IsletD(0), IsletYaw[0], 0.f);
		if (Before < 0)
		{
			Before = CountDamaged(TEXT("stand"), Ctr, 900.f);
			B->PlaceAt(IsletLocal(0, IsletR - 150.f, 0.f, R), IsletYaw[0]);
			B->Launch(FVector2D(TPol(1.f, IsletYaw[0], 0.f)) * NormalShot);
			View(IsletLocal(0, 100.f, 0.f, 150.f), IsletYaw[0], 24.f, 1500.f);
		}
		if (T > 0.9 && !bShot) { bShot = true; Shot(TEXT("stand")); }
		if (T > 1.6)
		{
			const int32 N = CountDamaged(TEXT("stand"), Ctr, 900.f);
			Next(FString::Printf(TEXT("stand: normal shot %.0f cm/s -> %d bleacher section(s) damaged or worse (before %d) %s"), NormalShot, N, Before, N > Before ? TEXT("PASS") : TEXT("FAIL")));
		}
		break;
	}
	case 3: // bridge: medium roll from the court out to islet 1
	{
		if (Before < 0)
		{
			Before = 0;
			B->PlaceAt(TPol(1150.f, IsletYaw[1], R), IsletYaw[1]);
			B->Launch(FVector2D(TPol(1.f, IsletYaw[1], 0.f)) * 1750.f);
			View(TPol(IsletD(1) - 500.f, IsletYaw[1], 0.f), IsletYaw[1] + 90.f, 30.f, 1900.f);
		}
		if (T > 2.2 && !bShot) { bShot = true; Shot(TEXT("bridge")); }
		if (T > 4.5)
		{
			const FVector L = B->BallLocation();
			const float D = FVector::Dist2D(L, TPol(IsletD(1), IsletYaw[1], 0.f));
			const bool bOk = !B->bOut && L.Z > IsletTopZ - 100.f && D < IsletR * IsletStretch;
			Next(FString::Printf(TEXT("bridge: 1750 cm/s roll from r1150 -> ends at (%.0f, %.0f, %.0f), %.0f cm from the islet centre, out=%d %s"), L.X, L.Y, L.Z, D, B->bOut ? 1 : 0, bOk ? TEXT("PASS") : TEXT("FAIL")));
		}
		break;
	}
	case 4: // lip: medium roll across islet 3 into its lip
	{
		if (Before < 0)
		{
			Before = 0;
			B->PlaceAt(IsletLocal(3, 400.f, 0.f, R), 0.f);
			B->Launch(FVector2D(FRotator(0.f, IsletYaw[3] + 270.f, 0.f).Vector()) * 1300.f);
			View(IsletLocal(3, 300.f, 0.f, 0.f), IsletYaw[3] + 180.f, 40.f, 1900.f);
		}
		if (T > 1.2 && !bShot) { bShot = true; Shot(TEXT("lip")); }
		if (T > 3.5)
		{
			const FVector L = B->BallLocation();
			const bool bOk = !B->bOut && L.Z > IsletTopZ - 100.f;
			Next(FString::Printf(TEXT("lip: 1300 cm/s roll into the islet lip -> ends at Z %.0f, out=%d %s"), L.Z, B->bOut ? 1 : 0, bOk ? TEXT("PASS") : TEXT("FAIL")));
		}
		break;
	}
	case 5: // ringout: full-power roll across islet 2 -> over the lip and off
	{
		if (Before < 0)
		{
			Before = 0;
			B->PlaceAt(IsletLocal(2, 420.f, 0.f, R), 0.f);
			B->Launch(FVector2D(FRotator(0.f, IsletYaw[2] + 270.f, 0.f).Vector()) * ZoogiConst::FullLaunchSpeed);
			View(IsletLocal(2, 300.f, 0.f, 0.f), IsletYaw[2] + 180.f, 35.f, 2200.f);
		}
		if (T > 0.6 && !bShot) { bShot = true; Shot(TEXT("ringout")); }
		if (T > 3.5)
		{
			const FVector L = B->BallLocation();
			const bool bOk = B->bOut || L.Z < IsletTopZ - 300.f;
			Next(FString::Printf(TEXT("ringout: full-power %.0f cm/s roll across the islet -> Z %.0f, out=%d %s"), ZoogiConst::FullLaunchSpeed, L.Z, B->bOut ? 1 : 0, bOk ? TEXT("PASS") : TEXT("FAIL")));
		}
		break;
	}
	case 6: case 7: case 8: case 9: case 10: case 11: // bridge_k (Evan 10:50 PM; 6 bridges since the Oct 9 symmetric layout): straight down each bridge centreline -> through its arch -> the islet orb
	{
		const int32 k = Step - 6;
		const float Yaw = IsletYaw[k];
		// The islet's orb spot (ExtraOrbSpots holds one per bridged islet; take the one on this islet).
		FVector Spot = TPol(IsletD(k), Yaw, IsletTopZ);
		float Best = 1e9f;
		for (const FVector& S : Arena->ExtraOrbSpots)
		{
			const float D = FVector::Dist2D(S, TPol(IsletD(k), Yaw, 0.f));
			if (D < Best) { Best = D; Spot = S; }
		}
		if (Before < 0)
		{
			Before = 0;
			BridgeMin = 1e9f;
			BridgeMaxR = -1.f;
			B->PlaceAt(TPol(1150.f, Yaw, R), Yaw);
			B->Launch(FVector2D(TPol(1.f, Yaw, 0.f)) * BridgeShot);
			View(TPol(ArchR + 150.f, Yaw, 0.f), Yaw + 90.f, 38.f, 2100.f);
		}
		// Judged the moment it happens (auto battle flow may move the Zoogi again afterwards): the Zoogi reaches the orb
		// spot (touching distance) while still in play and on the islet surface.
		const FVector L = B->BallLocation();
		const float Dn = FVector::Dist2D(L, Spot);
		BridgeMin = FMath::Min(BridgeMin, Dn);
		if (BridgeMaxR < 0.f && Dn < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 10.f && !B->bOut && L.Z > IsletTopZ - 100.f)
		{
			BridgeMaxR = (float)T; // reach time
			BridgeAt = L;
		}
		if (!bShot && (BridgeMaxR >= 0.f || T > 1.6)) { bShot = true; Shot(*FString::Printf(TEXT("bridge_%d"), FMath::RoundToInt(Yaw))); }
		if ((BridgeMaxR >= 0.f && T > BridgeMaxR + 0.3) || T > 4.0)
		{
			const bool bOk = BridgeMaxR >= 0.f;
			Next(FString::Printf(TEXT("bridge_%d: %.0f cm/s roll along the bridge centreline from r1150 -> %s the islet orb spot (%.0f, %.0f) (closest %.0f cm%s) %s"),
				FMath::RoundToInt(Yaw), BridgeShot, bOk ? TEXT("reached") : TEXT("did NOT reach"), Spot.X, Spot.Y, BridgeMin,
				bOk ? *FString::Printf(TEXT(", at t=%.2f s, Z %.0f"), BridgeMaxR, BridgeAt.Z) : TEXT(""), bOk ? TEXT("PASS") : TEXT("FAIL")));
			if (Step > 11)
			{
				UE_LOG(LogZoogi, Log, TEXT("PLAYTEST SUMMARY: %s"), *FString::Join(Results, TEXT(" | ")));
				if (APlayerController* PC = W->GetFirstPlayerController()) if (APawn* Pw = PC->GetPawn()) PC->SetViewTarget(Pw);
			}
		}
		break;
	}
	default: break;
	}
}
