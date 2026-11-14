// b130: Prism Grenade pickups - see ZoogiGrenadeItem.h
#include "ZoogiGrenadeItem.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiSkills.h"
#include "ZoogiHaptics.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiGrenade
{
	namespace
	{
		struct FDrop { TWeakObjectPtr<AActor> A; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float T = 0.f; bool bLanded = false; };
		struct FLobShot { TWeakObjectPtr<AActor> A; TWeakObjectPtr<AZoogiBall> B; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float T = 0.f, Dur = 0.8f, Spark = 0.f; };
		TArray<FDrop> Drops;
		TArray<FLobShot> Lobs;
		TMap<int32, int32> Held;
		TWeakObjectPtr<AZoogiGameMode> ForG; int32 ForSeed = 0; int32 CpuTurn = -1; TWeakObjectPtr<AZoogiBall> ForFirst; // b149: a new match = new balls (rematch / royale-mix round)
		bool SameMatch(const AZoogiGameMode* G) { return G && ForG.Get() == G && ForSeed == G->GameSeed && ForFirst.Get() == (G->Balls.Num() > 0 ? G->Balls[0].Get() : nullptr); }
		FTSTicker::FDelegateHandle Handle;
		const FLinearColor Glow(0.75f, 0.45f, 1.f);
		constexpr float FlyTime = 0.9f, Size = 110.f;

		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoGrenadeDrops")); return b; }
		AZoogiGameMode* Game()
		{
			UWorld* W = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr;
			return W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		}
		void Sync(AZoogiGameMode* G)
		{
			if (!G || SameMatch(G)) return;
			for (FDrop& D : Drops) if (AActor* A = D.A.Get()) A->Destroy();
			for (FLobShot& L : Lobs) if (AActor* A = L.A.Get()) A->Destroy();
			Drops.Reset(); Lobs.Reset(); Held.Reset(); CpuTurn = -1;
			ForG = G; ForSeed = G->GameSeed; ForFirst = G->Balls.Num() > 0 ? G->Balls[0].Get() : nullptr;
		}
		AActor* MakeVis(UWorld* W, const FVector& At)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W ? W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(At), SP) : nullptr;
			if (!A) return nullptr;
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Relics/Lantern/SM_RelicLantern.SM_RelicLantern"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Mesh)
			{
				M->SetStaticMesh(Mesh);
				const FBox Bx = Mesh->GetBoundingBox();
				const float Sc = Size / FMath::Max(1.f, Bx.GetSize().GetMax());
				M->SetRelativeScale3D(FVector(Sc));
			}
			else
			{
				M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
				M->SetMaterial(0, ZoogiUtil::FxMID(A, Glow, 8.f));
				A->SetActorScale3D(FVector(0.6f));
			}
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetCastShadow(false);
			return A;
		}
		FVector Arc(const FVector& From, const FVector& To, float K, float Hgt)
		{
			FVector P = FMath::Lerp(From, To, K);
			P.Z += 4.f * Hgt * K * (1.f - K);
			return P;
		}
		void CpuThink(AZoogiGameMode* G)
		{
			AZoogiBall* B = G->GetActiveBall();
			if (!B || !B->bCPU || B->bOut || G->Phase != EZoogiPhase::Aiming || G->TurnCounter == CpuTurn || G->PhaseTime < 1.2f || ZoogiSkills::Picking()) return;
			if (Count(G, B->PlayerIndex) <= 0) return;
			CpuTurn = G->TurnCounter;
			const float Range = 2.f * FMath::Max(400.f, G->ArcRange());
			const FVector Me = B->BallLocation();
			TArray<TPair<FVector, float>> Pts;
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O) && FVector::Dist2D(O->BallLocation(), Me) < Range) Pts.Add({ O->BallLocation(), 2.f });
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), Me) < Range) Pts.Add({ O->OrbLocation(), 1.f });
			float Best = 0.f; FVector At = FVector::ZeroVector;
			for (const TPair<FVector, float>& P : Pts)
			{
				if (FVector::Dist2D(P.Key, Me) < 450.f) continue; // never on itself
				float N = 0.f; for (const TPair<FVector, float>& Q : Pts) if (FVector::Dist2D(P.Key, Q.Key) < 500.f) N += Q.Value;
				if (N > Best) { Best = N; At = P.Key; }
			}
			if (Best < 3.f) { UE_LOG(LogZoogi, Log, TEXT("GRENADE cpu P%d keeps its grenade (no crowd in reach)"), B->PlayerIndex + 1); return; }
			if (!Take(G, B->PlayerIndex)) return;
			UE_LOG(LogZoogi, Log, TEXT("GRENADE cpu P%d throws at a crowd (%.0f) %.0f cm away"), B->PlayerIndex + 1, Best, FVector::Dist2D(At, Me));
			Lob(G, B, At);
		}
		bool Step(float Dt)
		{
			AZoogiGameMode* G = Game();
			if (!G) return true;
			Sync(G);
			UWorld* W = G->GetWorld();
			const float Now = W ? W->GetTimeSeconds() : 0.f;
			// drops: fly in from the generator, then wait (spin + bob) until a Zoogi touches one
			for (int32 i = Drops.Num() - 1; i >= 0; --i)
			{
				FDrop& D = Drops[i];
				AActor* A = D.A.Get();
				if (!A) { Drops.RemoveAt(i); continue; }
				if (!D.bLanded)
				{
					D.T += Dt;
					const float K = FMath::Clamp(D.T / FlyTime, 0.f, 1.f);
					A->SetActorLocation(Arc(D.From, D.To, K, 420.f));
					A->AddActorWorldRotation(FRotator(0.f, 400.f * Dt, 0.f));
					if (K >= 1.f)
					{
						D.bLanded = true;
						AZoogiFX::Spawn(W, D.To, Glow, 180.f, 0.5f, true, 9.f);
						AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), D.To, FRotator::ZeroRotator, 0.6f);
						ZoogiUtil::PlaySfx(G, TEXT("OrbClink"), 0.8f, 1.3f);
					}
					continue;
				}
				A->SetActorLocation(D.To + FVector(0.f, 0.f, 60.f + 12.f * FMath::Sin(Now * 3.f)));
				D.T += Dt;
				if (D.T > FlyTime + 1.f) { D.T = FlyTime; AZoogiFX::Spawn(W, D.To + FVector(0, 0, 4.f), Glow, 170.f, 0.8f, true, 7.f); } // b131: a ring pulse so it reads from the match camera
				A->AddActorWorldRotation(FRotator(0.f, 70.f * Dt, 0.f));
				for (AZoogiBall* B : G->Balls)
				{
					if (!B || B->bOut || FVector::Dist2D(B->BallLocation(), D.To) > ZoogiConst::ZoogiRadius + 60.f) continue;
					++Held.FindOrAdd(B->PlayerIndex);
					AZoogiFX::Spawn(W, D.To, Glow, 220.f, 0.5f, false, 12.f);
					ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.2f);
					if (!B->bCPU) ZoogiHaptics::Kick(0.6f, W, TEXT("grenade_pickup"));
					G->PublicPopup(TEXT("GRENADE!"), B->BallLocation() + FVector(0, 0, 170.f), Glow);
					G->PublicFeed(FString::Printf(TEXT("P%d %s GRABS THE PRISM GRENADE%s"), B->PlayerIndex + 1, *B->Def.Name.ToUpper(), B->bCPU ? TEXT("") : TEXT("  -  ITEM TO THROW IT")), Glow);
					UE_LOG(LogZoogi, Log, TEXT("GRENADE picked up by P%d (%d held)"), B->PlayerIndex + 1, Held[B->PlayerIndex]);
					A->Destroy();
					Drops.RemoveAt(i);
					break;
				}
			}
			// thrown grenades: arc to the spot, then the shockwave
			for (int32 i = Lobs.Num() - 1; i >= 0; --i)
			{
				FLobShot& L = Lobs[i];
				AActor* A = L.A.Get();
				L.T += Dt;
				const float K = FMath::Clamp(L.T / L.Dur, 0.f, 1.f);
				const FVector P = Arc(L.From, L.To, K, FMath::Max(300.f, FVector::Dist2D(L.From, L.To) * 0.35f));
				if (A) { A->SetActorLocation(P); A->AddActorWorldRotation(FRotator(500.f * Dt, 300.f * Dt, 0.f)); }
				L.Spark += Dt;
				if (L.Spark > 0.04f) { L.Spark = 0.f; AZoogiFX::Spawn(W, P, Glow, 50.f, 0.25f, false, 10.f); }
				if (K >= 1.f)
				{
					if (A) A->Destroy();
					AZoogiBall* B = L.B.Get();
					UE_LOG(LogZoogi, Log, TEXT("GRENADE lands at (%.0f, %.0f) -> shockwave"), L.To.X, L.To.Y);
					if (B) ZoogiSkills::GrenadeBoom(G, B, L.To);
					Lobs.RemoveAt(i);
				}
			}
			CpuThink(G);
			return true;
		}
		void Ensure() { if (!Handle.IsValid()) Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return Step(FMath::Min(Dt, 0.05f)); })); }
	}

	void OnRound(AZoogiGameMode* G)
	{
		if (Off() || !G || !G->Arena || G->bRoyaleMode || G->bComboMode || G->RoundNumber < 2) return;
		Ensure();
		Sync(G);
		if (Drops.Num()) return; // one on the court at a time
		UWorld* W = G->GetWorld();
		const float RingR = G->Arena->OrbRingRadiusOverride > 0.f ? G->Arena->OrbRingRadiusOverride : ZoogiConst::OrbRingRadius;
		const float FloorZ = [&]() { float Z = G->Arena->GetActorLocation().Z; for (const AZoogiBall* B : G->Balls) if (B && !B->bOut) { Z = B->BallLocation().Z - ZoogiConst::ZoogiRadius; break; } return Z; }();
		for (int32 Try = 0; Try < 80; ++Try)
		{
			const float A = FMath::FRandRange(0.f, 2.f * PI), D = FMath::Sqrt(FMath::FRandRange(300.f * 300.f, RingR * RingR));
			const FVector L(FMath::Cos(A) * D + G->Arena->GetActorLocation().X, FMath::Sin(A) * D + G->Arena->GetActorLocation().Y, FloorZ);
			if (G->Arena->IsOffCourt(FVector2D(L))) continue;
			bool bBad = false;
			for (const AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), L) < 300.f) bBad = true;
			for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), L) < 120.f) bBad = true;
			if (bBad) continue;
			const FVector Src = G->Arena->GetActorLocation() + FVector(0.f, 0.f, 850.f);
			FDrop Dp; Dp.A = MakeVis(W, Src); Dp.From = Src; Dp.To = L;
			if (!Dp.A.IsValid()) return;
			Drops.Add(Dp);
			AZoogiFX::Spawn(W, Src, Glow, 160.f, 0.4f, false, 10.f);
			G->PublicFeed(FString::Printf(TEXT("ROUND %d  -  THE ORB GENERATOR DROPS A PRISM GRENADE  -  FIRST TO TOUCH IT TAKES IT"), G->RoundNumber), Glow);
			G->TakeShotDelayed(TEXT("grenade_drop"), 1.4f);
			UE_LOG(LogZoogi, Log, TEXT("GRENADE drop round %d at (%.0f, %.0f)"), G->RoundNumber, L.X, L.Y);
			return;
		}
	}

	int32 Count(const AZoogiGameMode* G, int32 Player)
	{
		if (!SameMatch(G)) return 0;
		const int32* N = Held.Find(Player); return N ? *N : 0;
	}

	bool Take(AZoogiGameMode* G, int32 Player)
	{
		if (Count(G, Player) <= 0) return false;
		int32& N = Held.FindOrAdd(Player); --N; if (N <= 0) Held.Remove(Player);
		return true;
	}

	void Lob(AZoogiGameMode* G, AZoogiBall* B, const FVector& At)
	{
		if (!G || !B) return;
		Ensure();
		Sync(G); // b144a: sync to this match NOW - the first tick's Sync() used to wipe this very throw
		const FVector From = B->BallLocation() + FVector(0.f, 0.f, 60.f);
		FLobShot L; L.A = MakeVis(G->GetWorld(), From); L.B = B; L.From = From; L.To = FVector(At.X, At.Y, B->BallLocation().Z - ZoogiConst::ZoogiRadius + 20.f);
		L.Dur = FMath::Clamp(0.45f + FVector::Dist2D(From, At) / 3200.f, 0.5f, 1.3f);
		Lobs.Add(L);
		ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.7f, 1.4f);
		if (!B->bCPU) ZoogiHaptics::Kick(0.4f, G->GetWorld(), TEXT("grenade_throw"));
		UE_LOG(LogZoogi, Log, TEXT("GRENADE P%d throws %.0f cm"), B->PlayerIndex + 1, FVector::Dist2D(From, At));
	}
}
