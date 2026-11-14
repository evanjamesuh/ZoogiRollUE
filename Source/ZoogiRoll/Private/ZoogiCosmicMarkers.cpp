// Nov 3 b87: Cosmic rune markers (see ZoogiCosmicMarkers.h).
#include "ZoogiCosmicMarkers.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiHaptics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

namespace ZoogiCosmicMarkers
{
	namespace
	{
		struct FMarker { TWeakObjectPtr<AActor> A; FVector Up = FVector::ZeroVector; float Drop = 0.f, T = 0.f, Dur = 1.6f;
			float BaseSc = 1.f, Grow = 1.f, GrowFrom = 1.f, GrowTo = 1.f, GrowT = -1.f, GrowDelay = 0.f; }; // b129: grows each round
		const FLinearColor MarkerPurple(0.55f, 0.2f, 1.f);
		bool Old() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldCosmicMarkers")); return b; }
		// b129: sit the (scaled) marker's base on its floor spot
		void Seat(FMarker& M, AActor* A, const FVector& At)
		{
			A->SetActorLocation(At);
			if (UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(A->GetRootComponent())) { const float MinZ = Sm->Bounds.GetBox().Min.Z; A->SetActorLocation(At + FVector(0, 0, M.Up.Z - MinZ)); }
		}
		TArray<FMarker> Markers;
		FTSTicker::FDelegateHandle MarkerTick;
		constexpr float MarkerH = 300.f; // cm tall once grown (about a Zoogi and a half)

		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCosmicMarkers")); return b; }

		bool Step(float Dt)
		{
			bool bAny = false;
			for (FMarker& M : Markers)
			{
				AActor* A = M.A.Get();
				if (A && M.GrowT >= 0.f)
				{	// b129: the round's growth, one marker after another
					bAny = true;
					if (M.GrowDelay > 0.f) { M.GrowDelay -= Dt; continue; }
					M.GrowT = FMath::Min(M.GrowT + Dt, 0.8f);
					const float K = M.GrowT / 0.8f, E = 1.f - FMath::Pow(1.f - K, 3.f);
					M.Grow = FMath::Lerp(M.GrowFrom, M.GrowTo, E);
					if (UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(A->GetRootComponent())) Sm->SetRelativeScale3D(FVector(M.BaseSc * M.Grow));
					Seat(M, A, FVector(M.Up.X, M.Up.Y, A->GetActorLocation().Z));
					if (M.GrowT >= 0.8f) { M.GrowT = -1.f; AZoogiFX::Spawn(A->GetWorld(), M.Up + FVector(0, 0, 8.f), MarkerPurple, 220.f * M.Grow, 0.5f, true, 8.f); }
					continue;
				}
				if (!A || M.T >= M.Dur) continue;
				bAny = true;
				M.T = FMath::Min(M.T + Dt, M.Dur);
				const float K = M.T / M.Dur, E = 1.f - FMath::Pow(1.f - K, 3.f); // ease out
				const float Shake = (1.f - K) * 4.f;
				A->SetActorLocation(M.Up - FVector(FMath::FRandRange(-Shake, Shake), FMath::FRandRange(-Shake, Shake), M.Drop * (1.f - E)));
				if (M.T >= M.Dur) AZoogiFX::Spawn(A->GetWorld(), M.Up + FVector(0, 0, 8.f), Old() ? FLinearColor(1.f, 0.25f, 0.15f) : MarkerPurple, 260.f, 0.6f, true, 8.f);
			}
			if (!bAny) { MarkerTick.Reset(); return false; }
			return true;
		}
	}

	void OnMatchStart(AZoogiGameMode* G)
	{
		for (FMarker& M : Markers) if (AActor* A = M.A.Get()) A->Destroy();
		Markers.Reset();
		// b89c: the first marker rises a few seconds into round 1
		if (Off() || !G) return;
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		const double At = FPlatformTime::Seconds() + 4.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, At](float) -> bool
		{
			AZoogiGameMode* Gm = WG.Get();
			if (!Gm) return false;
			if (FPlatformTime::Seconds() < At) return true;
			if (Gm->Arena && Gm->Arena->IsForming()) return true;
			if (Old() && Gm->RoundNumber <= 1 && Gm->Phase != EZoogiPhase::Menu) OnRoundStart(Gm); // b129: none in round 1 (the first rises with round 2)
			return false;
		}));
	}

	void OnRoundStart(AZoogiGameMode* G)
	{
		if (Off() || !G || !G->Arena || G->Arena->GetTheme() != EZoogiArena::Cosmic || G->bRoyaleMode) return;
		if (!Old() && G->RoundNumber < 2) return; // b129 (Evan): "only start to appear on round 2"
		UWorld* W = G->GetWorld(); if (!W) return;
		Markers.RemoveAll([](const FMarker& M) { return !M.A.IsValid(); });
		float NewDelay = 0.f;
		if (!Old())
		{	// b129 (Evan: "every one of those that spawn increase in size the next round in order they summoned"): +20% each (max 2x)
			for (FMarker& M : Markers)
			{
				if (M.Grow >= 2.f) continue;
				M.GrowFrom = M.Grow; M.GrowTo = FMath::Min(2.f, M.Grow * 1.2f); M.GrowT = 0.f; M.GrowDelay = NewDelay; NewDelay += 0.55f;
			}
			if (NewDelay > 0.f && !MarkerTick.IsValid()) MarkerTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return Step(Dt); }));
			if (NewDelay > 0.f) UE_LOG(LogZoogi, Log, TEXT("COSMIC MARKER: %d marker(s) grow for round %d, in the order they rose"), Markers.Num(), G->RoundNumber);
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/CosmicMarker/SM_CosmicMarker.SM_CosmicMarker"));
		if (!Mesh) { UE_LOG(LogZoogi, Warning, TEXT("COSMIC MARKER: SM_CosmicMarker missing")); return; }
		AZoogiArena* Ar = G->Arena;
		const float KR = Ar->GetKnockoffRadius();
		const float Centre = 360.f * Ar->GetActorScale3D().X; // keep clear of the spire / centre lane
		// b89c: the court floor from Zoogis standing on it (not one parked under a pad); each spot is traced below
		float FloorZ = Ar->GetActorLocation().Z;
		{
			TArray<float> Zs;
			for (const AZoogiBall* B : G->Balls) if (B && !B->bOut && !B->bParked && !B->bOnLaunchPad) Zs.Add(B->BallLocation().Z - ZoogiConst::ZoogiRadius);
			if (Zs.Num()) { Zs.Sort(); FloorZ = Zs[Zs.Num() / 2]; }
		}
		FCollisionQueryParams Qp(TEXT("ZoogiCosmicMarker"), false);
		for (AZoogiBall* B : G->Balls) if (B) Qp.AddIgnoredActor(B);
		for (AZoogiOrb* O : G->Orbs) if (O) Qp.AddIgnoredActor(O);
		for (const FMarker& Mq : Markers) if (AActor* Am = Mq.A.Get()) Qp.AddIgnoredActor(Am);
		auto FloorAt = [&](const FVector2D& Xy)
		{
			FHitResult Hr;
			if (W->LineTraceSingleByChannel(Hr, FVector(Xy, FloorZ + 600.f), FVector(Xy, FloorZ - 600.f), ECC_Visibility, Qp)) return (float)Hr.ImpactPoint.Z;
			return FloorZ;
		};
		FVector Spot = FVector::ZeroVector; bool bFound = false;
		for (int32 Try = 0; Try < 200 && !bFound; ++Try)
		{
			const float Ang = FMath::FRandRange(0.f, 2.f * PI), D = FMath::FRandRange(FMath::Max(Centre, KR * 0.28f), KR * 0.68f);
			const FVector P(FMath::Cos(Ang) * D, FMath::Sin(Ang) * D, FloorZ);
			if (Ar->IsOffCourt(FVector2D(P)) || Ar->IsOffCourt(FVector2D(P * 1.25f))) continue;
			bool bClear = true;
			for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), P) < 200.f) bClear = false;
			for (const AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), P) < 300.f) bClear = false;
			for (const FMarker& M : Markers) if (M.A.IsValid() && FVector::Dist2D(M.Up, P) < 380.f) bClear = false;
			if (bClear) { Spot = P; Spot.Z = FloorAt(FVector2D(P)); bFound = true; }
		}
		if (!bFound) { UE_LOG(LogZoogi, Log, TEXT("COSMIC MARKER: no free spot for round %d"), G->RoundNumber); return; }
		const FBox Bx = Mesh->GetBoundingBox();
		const FVector Ext = Bx.GetSize();
		const float Sc = MarkerH / FMath::Max(Ext.Z, 1.f);
		const float Yaw = FMath::FRandRange(0.f, 360.f);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Spot - FVector(0, 0, MarkerH + 20.f)), SP);
		if (!A) return;
		A->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* M = A->GetStaticMeshComponent();
		M->SetStaticMesh(Mesh);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetRelativeScale3D(FVector(Sc));
		if (!Old()) for (int32 s = 0; s < M->GetNumMaterials(); ++s) M->SetMaterial(s, ZoogiUtil::SurfaceMID(A, FLinearColor(0.3f, 0.05f, 0.8f), 0.35f, FLinearColor(0.4f, 0.08f, 1.f), 0.35f, 0.25f)); // b129: purple (b130: deeper - b129 read pale lavender)
		M->SetRelativeLocation(FVector(-Bx.GetCenter().X * Sc, -Bx.GetCenter().Y * Sc, -Bx.Min.Z * Sc)); // base on the floor, centred
		// the solid part: a capsule a little slimmer than the widest ring (the spindle tapers to its tips)
		UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(A, TEXT("MarkerBody"));
		const float Rad = FMath::Clamp(0.42f * FMath::Max(Ext.X, Ext.Y) * Sc, 40.f, 120.f);
		Cap->InitCapsuleSize(Rad, MarkerH * 0.5f);
		Cap->SetupAttachment(A->GetRootComponent());
		Cap->SetRelativeLocation(FVector(0, 0, MarkerH * 0.5f));
		Cap->SetCollisionProfileName(TEXT("BlockAll"));
		Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Cap->SetHiddenInGame(true);
		Cap->RegisterComponent();
		A->Tags.Add(TEXT("CosmicMarker"));
		FMarker Mk; Mk.A = A; Mk.Up = Spot; Mk.Drop = MarkerH + 20.f; Mk.BaseSc = Sc;
		if (!Old()) { Mk.T = -NewDelay; } // b129: the new marker rises after the old ones have grown
		Markers.Add(Mk);
		if (!MarkerTick.IsValid()) MarkerTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return Step(Dt); }));
		AZoogiFX::Spawn(W, Spot + FVector(0, 0, 4.f), Old() ? FLinearColor(1.f, 0.2f, 0.1f) : MarkerPurple, 320.f, 1.2f, true, 10.f);
		if (Old()) AZoogiFX::Stylized(W, TEXT("Smoke_v2"), Spot, 0.6f); // b129: no smoke
		ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.9f, 0.7f);
		ZoogiHaptics::Kick(0.5f, W, TEXT("cosmic_marker"));
		G->PublicFeed(FString::Printf(TEXT("ROUND %d  -  A RUNE MARKER RISES"), G->RoundNumber), FLinearColor(1.f, 0.35f, 0.25f));
		G->TakeShotDelayed(FString::Printf(TEXT("cosmic_marker_r%d"), G->RoundNumber), 2.2f);
		UE_LOG(LogZoogi, Log, TEXT("COSMIC MARKER %d rises for round %d at (%.0f, %.0f), scale %.2f, body r %.0f"), Markers.Num(), G->RoundNumber, Spot.X, Spot.Y, Sc, Rad);
	}
}
