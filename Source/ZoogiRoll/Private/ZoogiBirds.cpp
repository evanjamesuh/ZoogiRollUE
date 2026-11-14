// b179: Meadow songbirds - see ZoogiBirds.h
#include "ZoogiBirds.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiBirds
{
	namespace
	{
		enum class EB : uint8 { Perch, Hop, Flee, Gone, Arrive, Flyover };
		struct FBird
		{
			TWeakObjectPtr<AActor> A; TWeakObjectPtr<USceneComponent> WL, WR, Body;
			EB S = EB::Perch; FVector Pos = FVector::ZeroVector, Vel = FVector::ZeroVector, From = FVector::ZeroVector, To = FVector::ZeroVector;
			float Yaw = 0.f, T = 0.f, Dur = 1.f, Flap = 0.f, Next = 2.f, Wait = 0.f, Startle = -1.f; bool bFlock = false;
		};
		TArray<FBird> Birds;
		TWeakObjectPtr<AZoogiGameMode> GG; int32 MatchKey = -1; bool bFlyoverDone = false; float ChirpT = 4.f, FloorZ = 0.f, CourtR = 1500.f; FVector Ctr = FVector::ZeroVector;
		FRandomStream R(7);

		bool On(const AZoogiGameMode* G)
		{
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBirds"));
			static const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBirdsAll"));
			return !bOff && G && G->Arena && (bAll || G->Arena->GetTheme() == EZoogiArena::Meadow);
		}
		UStaticMesh* Shape(const TCHAR* N) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet); }
		UStaticMeshComponent* Part(AActor* A, USceneComponent* Parent, const TCHAR* Mesh, const FVector& Loc, const FVector& Scl, const FRotator& Rot, const FLinearColor& Col)
		{
			UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(A);
			M->SetStaticMesh(Shape(Mesh)); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetGenerateOverlapEvents(false);
			M->SetupAttachment(Parent); M->SetRelativeLocation(Loc); M->SetRelativeScale3D(Scl); M->SetRelativeRotation(Rot);
			M->SetCastShadow(true); M->bCastDynamicShadow = true;
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
				if (UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, M)) { D->SetVectorParameterValue(TEXT("Color"), Col); M->SetMaterial(0, D); }
			M->RegisterComponent();
			return M;
		}
		FBird Make(UWorld* W, const FVector& At, float Yaw)
		{
			FBird B; B.Pos = At; B.Yaw = Yaw;
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), At), Sp);
			if (!A) return B;
			USceneComponent* Root = NewObject<USceneComponent>(A); A->SetRootComponent(Root); Root->RegisterComponent(); A->SetActorLocationAndRotation(At, FRotator(0.f, Yaw, 0.f));
			USceneComponent* Body = NewObject<USceneComponent>(A); Body->SetupAttachment(Root); Body->RegisterComponent();
			// colours: bluebird / robin / goldfinch / cardinal
			static const FLinearColor Pal[4][3] = {
				{ FLinearColor(0.07f, 0.22f, 0.75f), FLinearColor(0.9f, 0.45f, 0.15f), FLinearColor(0.04f, 0.12f, 0.45f) },
				{ FLinearColor(0.25f, 0.16f, 0.1f), FLinearColor(0.9f, 0.32f, 0.08f), FLinearColor(0.16f, 0.1f, 0.06f) },
				{ FLinearColor(0.95f, 0.78f, 0.05f), FLinearColor(1.f, 0.92f, 0.5f), FLinearColor(0.05f, 0.05f, 0.05f) },
				{ FLinearColor(0.8f, 0.05f, 0.05f), FLinearColor(0.9f, 0.2f, 0.15f), FLinearColor(0.45f, 0.03f, 0.03f) } };
			const int32 Pi = R.RandRange(0, 3);
			const FLinearColor Bc = Pal[Pi][0], Be = Pal[Pi][1], Wc = Pal[Pi][2], Bk(1.f, 0.55f, 0.08f), Ey(0.01f, 0.01f, 0.01f);
			const float K = R.FRandRange(1.9f, 2.3f); // b179b: readable from the game camera
			Part(A, Body, TEXT("Sphere"), FVector(0, 0, 12) * K, FVector(0.32f, 0.2f, 0.19f) * K, FRotator(-8.f, 0, 0), Bc);
			Part(A, Body, TEXT("Sphere"), FVector(3, 0, 9) * K, FVector(0.22f, 0.16f, 0.13f) * K, FRotator::ZeroRotator, Be); // belly
			Part(A, Body, TEXT("Sphere"), FVector(15, 0, 21) * K, FVector(0.14f, 0.13f, 0.13f) * K, FRotator::ZeroRotator, Bc); // head
			Part(A, Body, TEXT("Cone"), FVector(23, 0, 20) * K, FVector(0.05f, 0.05f, 0.08f) * K, FRotator(-90.f, 0, 0), Bk); // beak
			Part(A, Body, TEXT("Sphere"), FVector(19, 5, 24) * K, FVector(0.03f) * K, FRotator::ZeroRotator, Ey);
			Part(A, Body, TEXT("Sphere"), FVector(19, -5, 24) * K, FVector(0.03f) * K, FRotator::ZeroRotator, Ey);
			Part(A, Body, TEXT("Cube"), FVector(-19, 0, 15) * K, FVector(0.13f, 0.08f, 0.02f) * K, FRotator(22.f, 0, 0), Wc); // tail
			Part(A, Body, TEXT("Cylinder"), FVector(2, 4, 3) * K, FVector(0.015f, 0.015f, 0.07f) * K, FRotator::ZeroRotator, Bk); // legs
			Part(A, Body, TEXT("Cylinder"), FVector(2, -4, 3) * K, FVector(0.015f, 0.015f, 0.07f) * K, FRotator::ZeroRotator, Bk);
			for (int32 s = -1; s <= 1; s += 2)
			{
				USceneComponent* Pv = NewObject<USceneComponent>(A); Pv->SetupAttachment(Body); Pv->SetRelativeLocation(FVector(3, 8.f * s, 16) * K); Pv->RegisterComponent();
				Part(A, Pv, TEXT("Cube"), FVector(-2, 14.f * s, 0) * K, FVector(0.16f, 0.28f, 0.02f) * K, FRotator::ZeroRotator, Wc);
				(s < 0 ? B.WL : B.WR) = Pv;
			}
			B.A = A; B.Body = Body; B.Next = R.FRandRange(1.f, 4.f);
			return B;
		}
		float GroundZ(const AZoogiGameMode* G, const FVector& P)
		{	// the court floor under P (the Meadow's look pieces have no collision; the play floor does)
			UWorld* W = G ? G->GetWorld() : nullptr; if (!W) return FloorZ;
			FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiBirds), false);
			for (const AZoogiBall* Bz : G->Balls) if (Bz) Q.AddIgnoredActor(Bz);
			for (const AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
			if (W->LineTraceSingleByChannel(H, FVector(P.X, P.Y, FloorZ + 600.f), FVector(P.X, P.Y, FloorZ - 600.f), ECC_WorldStatic, Q)) return H.ImpactPoint.Z;
			return FloorZ;
		}
		FVector RandSpot(const AZoogiGameMode* G)
		{	// an open spot on the court: away from Zoogis and orbs
			for (int32 Try = 0; Try < 30; ++Try)
			{
				const float A = R.FRandRange(0.f, 2.f * PI), Rr = CourtR * FMath::Sqrt(R.FRandRange(0.12f, 0.72f));
				FVector P(Ctr.X + FMath::Cos(A) * Rr, Ctr.Y + FMath::Sin(A) * Rr, FloorZ);
				bool bOk = true;
				for (const AZoogiBall* Bz : G->Balls) if (Bz && !Bz->bOut && FVector::Dist2D(Bz->BallLocation(), P) < 450.f) bOk = false;
				for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), P) < 90.f) bOk = false;
				for (const FBird& B : Birds) if (B.S == EB::Perch && FVector::Dist2D(B.Pos, P) < 60.f) bOk = false;
				if (bOk) { P.Z = GroundZ(G, P); return P; }
			}
			return FVector(Ctr.X, Ctr.Y, FloorZ) + FVector(CourtR * 0.6f, 0.f, 0.f);
		}
		void Chirp(UWorld* W, const FVector& At, float Vol)
		{
			// b182 FIX: the chirps were cached as raw static pointers - the garbage collector freed them after a map change and the next
			// chirp played a dead sound (PC crash in Chirp, AudioDevice "Sound->GetFName() != None"). Weak pointers, reloaded when gone.
			static TWeakObjectPtr<USoundBase> Snd[4];
			const int32 Ki = R.RandRange(0, 3);
			if (!Snd[Ki].IsValid()) Snd[Ki] = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/Sounds/Amb/SW_AmbBird_%02d.SW_AmbBird_%02d"), Ki + 1, Ki + 1), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (USoundBase* Sx = Snd[Ki].Get()) if (W) UGameplayStatics::PlaySoundAtLocation(W, Sx, At, Vol * ZoogiUtil::GetSfxVolume(), R.FRandRange(0.95f, 1.2f));
		}
		void TakeOff(FBird& B, const FVector& From)
		{
			if (B.S == EB::Flee || B.S == EB::Gone || B.S == EB::Flyover) return;
			FVector Away = (B.Pos - From).GetSafeNormal2D(); if (Away.IsNearlyZero()) Away = FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f).Vector();
			Away = Away.RotateAngleAxis(R.FRandRange(-35.f, 35.f), FVector::UpVector);
			B.S = EB::Flee; B.T = 0.f; B.Vel = Away * R.FRandRange(500.f, 750.f) + FVector(0.f, 0.f, R.FRandRange(450.f, 650.f)); B.Startle = -1.f;
			if (AZoogiGameMode* G = GG.Get()) { Chirp(G->GetWorld(), B.Pos, 0.45f); if (R.FRand() < 0.5f) ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.18f, 1.9f); }
		}
	}

	void Clear()
	{
		for (FBird& B : Birds) if (AActor* A = B.A.Get()) A->Destroy();
		Birds.Reset(); bFlyoverDone = false; MatchKey = -1;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld()) return;
		const bool bMatch = G->Phase != EZoogiPhase::Menu && G->Phase != EZoogiPhase::Forming && G->Phase != EZoogiPhase::DropIntro;
		if (GG.Get() != G || !On(G) || G->Phase == EZoogiPhase::Menu) { if (Birds.Num()) Clear(); GG = G; if (!On(G) || G->Phase == EZoogiPhase::Menu) return; }
		if (G->Phase == EZoogiPhase::Forming && MatchKey >= 0) Clear(); // a new match is forming
		if (!bMatch) return;
		UWorld* W = G->GetWorld();
		const float Dt = FMath::Min(0.05f, W->GetDeltaSeconds());
		if (MatchKey < 0)
		{	// a new match on the Meadow: the court size, the floor height, a flock on the court + a flyover
			MatchKey = 1; Ctr = G->Arena->GetActorLocation(); CourtR = G->Arena->GetKnockoffRadius();
			float Zs = 0.f; int32 N = 0; for (const AZoogiBall* Bz : G->Balls) if (Bz && !Bz->bOut) { Zs += Bz->BallLocation().Z - ZoogiConst::ZoogiRadius; ++N; }
			FloorZ = N ? Zs / N : Ctr.Z;
			R.Initialize(GetTypeHash(G->GameSeed) ^ 0x51b1);
			const int32 NP = 7;
			for (int32 k = 0; k < NP; ++k) { const FVector P = RandSpot(G); Birds.Add(Make(W, P, R.FRandRange(0.f, 360.f))); }
			UE_LOG(LogZoogi, Log, TEXT("BIRDS: %d songbirds on the Meadow court (r %.0f, floor %.0f)"), Birds.Num(), CourtR, FloorZ);
			if (!G->ShotDir.IsEmpty()) G->TakeShotDelayed(TEXT("birds_perched"), 0.5f);
		}
		if (!bFlyoverDone)
		{	// ambience: a flock crosses the sky over the arena at the start of the match
			bFlyoverDone = true;
			const float A0 = R.FRandRange(0.f, 360.f);
			const FVector Dir = FRotator(0.f, A0, 0.f).Vector(), Side(-Dir.Y, Dir.X, 0.f);
			for (int32 k = 0; k < 6; ++k)
			{
				const FVector Off = -Dir * (FMath::Abs(k - 2.5f) * 140.f) + Side * ((k - 2.5f) * 120.f);
				FBird B = Make(W, Ctr - Dir * CourtR * 2.6f + Off + FVector(0.f, 0.f, 900.f + R.FRandRange(-80.f, 80.f)), A0);
				B.S = EB::Flyover; B.Vel = Dir * R.FRandRange(620.f, 700.f); B.bFlock = true; B.Flap = R.FRandRange(0.f, 6.f);
				Birds.Add(B);
			}
			if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("birds_flyover_a"), 2.5f); G->TakeShotDelayed(TEXT("birds_flyover_b"), 4.5f); }
		}
		// danger: Zoogis rolling near (fast ones from further away)
		TArray<TPair<FVector, float>> Threats;
		for (const AZoogiBall* Bz : G->Balls) if (Bz && !Bz->bOut)
		{
			const float Sp = Bz->GetBallVelocity().Size2D();
			const float Rad = Sp > 900.f ? 650.f : Sp > 250.f ? 480.f : Sp > 60.f ? 300.f : 170.f;
			Threats.Add(TPair<FVector, float>(Bz->BallLocation(), Rad));
		}
		if ((ChirpT -= Dt) <= 0.f)
		{
			ChirpT = R.FRandRange(5.f, 12.f);
			TArray<int32> Perched; for (int32 i = 0; i < Birds.Num(); ++i) if (Birds[i].S == EB::Perch || Birds[i].S == EB::Hop) Perched.Add(i);
			if (Perched.Num()) Chirp(W, Birds[Perched[R.RandRange(0, Perched.Num() - 1)]].Pos, 0.3f);
		}
		for (int32 i = Birds.Num() - 1; i >= 0; --i)
		{
			FBird& B = Birds[i];
			AActor* A = B.A.Get(); if (!A) { Birds.RemoveAt(i); continue; }
			B.T += Dt;
			float Pitch = 0.f, Wing = 4.f, Bob = 0.f; // b179b: Wing ~0 = folded flat along the back (80 stood the wings up like flags)
			switch (B.S)
			{
			case EB::Perch:
			case EB::Hop:
			{
				for (const TPair<FVector, float>& Th : Threats) if (FVector::Dist2D(Th.Key, B.Pos) < Th.Value) { TakeOff(B, Th.Key); for (FBird& O : Birds) if (&O != &B && (O.S == EB::Perch || O.S == EB::Hop) && FVector::Dist2D(O.Pos, B.Pos) < 600.f && O.Startle < 0.f) O.Startle = R.FRandRange(0.08f, 0.45f); break; }
				if (B.S != EB::Perch && B.S != EB::Hop) break;
				if (B.Startle >= 0.f && (B.Startle -= Dt) <= 0.f) { TakeOff(B, B.Pos - FRotator(0.f, B.Yaw, 0.f).Vector() * 100.f); break; } // a neighbour flew - so do I
				if (B.S == EB::Hop)
				{
					const float K = FMath::Clamp(B.T / B.Dur, 0.f, 1.f);
					B.Pos = FMath::Lerp(B.From, B.To, K); B.Pos.Z = B.From.Z + FMath::Sin(K * PI) * 22.f; Wing = 30.f;
					if (K >= 1.f) { B.S = EB::Perch; B.T = 0.f; B.Next = R.FRandRange(1.f, 4.f); }
				}
				else
				{
					const float Pk = FMath::Fmod(B.T, 1.6f);
					if (B.Next > 0.f && Pk < 0.35f && FMath::Fmod(B.T * 0.37f, 1.f) < 0.5f) Pitch = -FMath::Sin(Pk / 0.35f * PI) * 35.f; // pecks the ground
					if ((B.Next -= Dt) <= 0.f)
					{	// hop a little way, or turn on the spot
						if (R.FRand() < 0.6f)
						{
							const float Ay = B.Yaw + R.FRandRange(-90.f, 90.f);
							B.From = B.Pos; B.To = B.Pos + FRotator(0.f, Ay, 0.f).Vector() * R.FRandRange(25.f, 60.f); B.To.Z = B.Pos.Z;
							if (FVector::Dist2D(B.To, Ctr) > CourtR * 0.85f) B.To = B.Pos;
							B.Yaw = Ay; B.S = EB::Hop; B.T = 0.f; B.Dur = 0.22f;
						}
						else { B.Yaw += R.FRandRange(-120.f, 120.f); B.Next = R.FRandRange(1.f, 3.5f); }
					}
				}
				break;
			}
			case EB::Flee:
			{	// flaps hard, climbs away, banks gently
				B.Vel.Z = FMath::Min(B.Vel.Z + 220.f * Dt, 900.f);
				B.Vel = B.Vel.RotateAngleAxis(FMath::Sin(B.T * 1.3f + i) * 12.f * Dt, FVector::UpVector) * (1.f + 0.25f * Dt);
				B.Pos += B.Vel * Dt; B.Yaw = B.Vel.Rotation().Yaw; Pitch = FMath::Clamp(B.Vel.Rotation().Pitch, -10.f, 35.f);
				B.Flap += Dt * 26.f; Wing = FMath::Sin(B.Flap) * 70.f;
				if (B.T > 5.f || B.Pos.Z > FloorZ + 2600.f) { B.S = EB::Gone; B.Wait = R.FRandRange(10.f, 22.f); A->SetActorHiddenInGame(true); }
				break;
			}
			case EB::Gone:
				if ((B.Wait -= Dt) <= 0.f)
				{	// comes back from the edge of the sky and glides in to land somewhere new
					B.To = RandSpot(G); B.From = B.To + FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f).Vector() * 2200.f + FVector(0.f, 0.f, 1300.f);
					B.Pos = B.From; B.S = EB::Arrive; B.T = 0.f; B.Dur = R.FRandRange(3.f, 4.5f); A->SetActorHiddenInGame(false);
				}
				break;
			case EB::Arrive:
			{
				const float K = FMath::Clamp(B.T / B.Dur, 0.f, 1.f), Ke = 1.f - FMath::Square(1.f - K);
				const FVector Prev = B.Pos;
				B.Pos = FMath::Lerp(B.From, B.To, Ke); B.Pos.Z = FMath::Lerp(B.From.Z, B.To.Z, FMath::Sqrt(Ke));
				if (!(Prev - B.Pos).IsNearlyZero()) B.Yaw = (B.Pos - Prev).Rotation().Yaw;
				B.Flap += Dt * (K > 0.8f ? 30.f : 9.f); Wing = K > 0.8f ? FMath::Sin(B.Flap) * 75.f : (FMath::Fmod(B.T, 1.8f) < 0.6f ? FMath::Sin(B.Flap) * 50.f : -10.f); // flap - glide - flare to land
				Pitch = K > 0.85f ? 25.f : -5.f;
				for (const TPair<FVector, float>& Th : Threats) if (FVector::Dist2D(Th.Key, B.To) < Th.Value) { B.To = RandSpot(G); break; } // the spot got busy: pick another
				if (K >= 1.f) { B.S = EB::Perch; B.T = 0.f; B.Pos = B.To; B.Next = R.FRandRange(0.5f, 2.f); Chirp(W, B.Pos, 0.25f); }
				break;
			}
			case EB::Flyover:
				B.Pos += B.Vel * Dt; B.Flap += Dt * 18.f; Wing = FMath::Fmod(B.T + i * 0.3f, 1.5f) < 0.9f ? FMath::Sin(B.Flap) * 60.f : -8.f; B.Yaw = B.Vel.Rotation().Yaw;
				if (FVector::Dist2D(B.Pos, Ctr) > CourtR * 3.f && B.T > 3.f) { A->Destroy(); Birds.RemoveAt(i); continue; }
				break;
			}
			A->SetActorLocationAndRotation(B.Pos + FVector(0.f, 0.f, Bob), FRotator(0.f, B.Yaw, 0.f));
			if (USceneComponent* Bd = B.Body.Get()) Bd->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
			if (USceneComponent* L = B.WL.Get()) L->SetRelativeRotation(FRotator(0.f, 0.f, Wing));
			if (USceneComponent* Rw = B.WR.Get()) Rw->SetRelativeRotation(FRotator(0.f, 0.f, -Wing));
		}
	}
}
