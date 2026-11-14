#include "ZoogiVortexRings.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiObstacle.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiVortexRings
{
	namespace
	{
		struct FRing { FVector C; };
		struct FTransit { TWeakObjectPtr<AActor> A; TWeakObjectPtr<UPrimitiveComponent> Body; int32 From = 0, To = 0; float T = 0.f; bool bOut = false; };
		TArray<FRing> Rings;
		TArray<FTransit> Transits;
		TMap<TWeakObjectPtr<AActor>, float> Cool;
		TWeakObjectPtr<AZoogiArena> ForArena;
		int32 ForSeed = 0;
		float Clock = 0.f;
		bool bTestSent = false;
		constexpr float Reach = 200.f, PullTime = 0.35f, ExitSpeed = 1150.f, ExitUp = 260.f, CoolTime = 2.f;

		void Gather(AZoogiArena* Ar)
		{
			Rings.Reset();
			for (AZoogiArenaPiece* P : Ar->GetPieces()) if (P && P->GetKind().ToString().StartsWith(TEXT("pillar_"))) { FRing R; R.C = P->GetActorLocation() + FVector(0, 0, 60.f); Rings.Add(R); }
			for (AZoogiObstacle* O : Ar->GetObstacles()) if (O && O->ObsName.ToString().StartsWith(TEXT("Ring2_OuterOrb_"))) { FRing R; R.C = O->GetActorLocation() + FVector(0, 0, 40.f); Rings.Add(R); }
			UE_LOG(LogZoogi, Log, TEXT("VORTEX: %d rings on the Frozen Ring"), Rings.Num());
		}
		void Fx(UWorld* W, const FVector& At, bool bIn)
		{
			AZoogiFX::Spawn(W, At, FLinearColor(0.35f, 0.9f, 1.f), bIn ? 220.f : 260.f, 0.5f, true, 12.f);
			AZoogiFX::Spawn(W, At, FLinearColor(0.75f, 0.95f, 1.f), 120.f, 0.35f, false, 10.f);
			AZoogiFabFX::Play(W, TEXT("/Game/StylizedToonVFX3/Effects/NS_Water_hit.NS_Water_hit"), At - FVector(0, 0, 40.f), 0.18f, 1.2f);
			ZoogiUtil::PlaySfx(W->GetAuthGameMode(), bIn ? TEXT("Inhale") : TEXT("Dash"), 0.7f, bIn ? 1.3f : 1.1f);
		}
	}

	void Tick(AZoogiGameMode* G, float Dt)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVortexRings"));
		if (bOff || !G || !G->Arena || G->Arena->GetTheme() != EZoogiArena::Frozen || G->bRoyaleMode) return;
		UWorld* W = G->GetWorld();
		if (ForArena.Get() != G->Arena || ForSeed != G->GameSeed) { ForArena = G->Arena; ForSeed = G->GameSeed; Transits.Reset(); Cool.Reset(); bTestSent = false; Clock = 0.f; Gather(G->Arena); }
		Clock += Dt;
		if (Rings.Num() < 2) return;
		const FVector Mid = G->Arena->GetActorLocation();
		// capture
		auto Try = [&](AActor* A, UPrimitiveComponent* Body)
		{
			if (!A || !Body || !Body->IsSimulatingPhysics()) return;
			if (const float* Cd = Cool.Find(A)) if (Clock < *Cd) return;
			for (const FTransit& Tr : Transits) if (Tr.A.Get() == A) return;
			const FVector V = Body->GetPhysicsLinearVelocity();
			if (V.Size2D() < 260.f && FMath::Abs(V.Z) < 200.f) return; // only things on the move
			const FVector P = A->GetActorLocation();
			for (int32 i = 0; i < Rings.Num(); ++i)
			{
				if (FVector::Dist2D(P, Rings[i].C) > Reach || FMath::Abs(P.Z - Rings[i].C.Z) > 260.f) continue;
				FTransit Tr; Tr.A = A; Tr.Body = Body; Tr.From = i;
				do { Tr.To = FMath::RandRange(0, Rings.Num() - 1); } while (Tr.To == i);
				Transits.Add(Tr);
				Fx(W, Rings[i].C, true);
				UE_LOG(LogZoogi, Log, TEXT("VORTEX: %s sucked into ring %d -> out of ring %d"), *A->GetName(), i, Tr.To);
				if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("vortex_in"), 0.1f); G->TakeShotDelayed(TEXT("vortex_out"), PullTime + 0.25f); }
				return;
			}
		};
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Try(B, B->Sphere);
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Try(O, O->Sphere);
		// pull in, then fire out of the other ring toward the middle
		for (int32 k = Transits.Num() - 1; k >= 0; --k)
		{
			FTransit& Tr = Transits[k];
			AActor* A = Tr.A.Get(); UPrimitiveComponent* Body = Tr.Body.Get();
			if (!A || !Body) { Transits.RemoveAt(k); continue; }
			Tr.T += Dt;
			const FVector C = Rings[Tr.From].C;
			if (Tr.T < PullTime)
			{
				const FVector To = C - A->GetActorLocation();
				const FVector Tan = FVector::CrossProduct(FVector::UpVector, To.GetSafeNormal2D());
				Body->SetPhysicsLinearVelocity(To * 9.f + Tan * 520.f); // spiral into the ring
				continue;
			}
			const FVector Out = Rings[Tr.To].C;
			const FVector Dir = (Mid - Out).GetSafeNormal2D();
			A->SetActorLocation(Out + Dir * (Reach + 40.f) + FVector(0, 0, 30.f), false, nullptr, ETeleportType::TeleportPhysics);
			Body->SetPhysicsLinearVelocity(Dir * ExitSpeed + FVector(0, 0, ExitUp));
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Fx(W, Out, false);
			Cool.Add(A, Clock + CoolTime);
			Transits.RemoveAt(k);
		}
		// test: one orb is sent at ring 0 after 8 s
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVortexTest"));
		if (bTest && !bTestSent && Clock > 8.f)
			for (AZoogiOrb* O : G->Orbs)
				if (O && !O->bOut && O->Sphere)
				{
					bTestSent = true;
					const FVector R0 = Rings[0].C, Dir = (R0 - Mid).GetSafeNormal2D();
					O->SetActorLocation(R0 - Dir * 450.f + FVector(0, 0, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
					O->Sphere->SetPhysicsLinearVelocity(Dir * 700.f);
					UE_LOG(LogZoogi, Log, TEXT("VORTEX test: an orb sent at ring 0"));
					break;
				}
	}
}
