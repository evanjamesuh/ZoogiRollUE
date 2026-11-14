// b156 (Evan Nov 6 07:38 "Battle royal arena map starts with ancient meadows, for this map the zoogi king sit on the throne which
// would be me but as a 3d model I create for myself"): the ZOOGI KING's throne on Ancient Meadows' Sky Citadel, facing the plaza.
// The king himself is Evan's own model: drop it in as /Game/ZoogiKing/SK_ZoogiKing (skeletal) or /Game/ZoogiKing/SM_ZoogiKing
// (static) and it sits on the seat; until then the throne is empty (no stand-in). Visual only (no collision).
// -ZoogiNoThrone = off.
#include "ZoogiThrone.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiThrone
{
	namespace
	{
		TArray<TWeakObjectPtr<AActor>> GParts;
		TWeakObjectPtr<AZoogiArena> GArena;
		bool GBuilt = false;
		FVector GSeat = FVector::ZeroVector; float GYaw = 0.f;

		void Clear()
		{
			for (TWeakObjectPtr<AActor>& A : GParts) if (AActor* X = A.Get()) X->Destroy();
			GParts.Reset(); GBuilt = false;
		}
		AActor* Part(UWorld* W, const TCHAR* Shape, const FVector& Loc, const FRotator& Rot, const FVector& Scale, const FLinearColor& Col, float Metal = 0.f)
		{
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Shape);
			if (!M) return nullptr;
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rot, Loc, Scale), SP);
			if (!A) return nullptr;
			UStaticMeshComponent* C = A->GetStaticMeshComponent();
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(M);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, A);
				D->SetVectorParameterValue(TEXT("Color"), Col);
				C->SetMaterial(0, D);
			}
			A->SetActorEnableCollision(false);
			GParts.Add(A);
			return A;
		}
		void Build(AZoogiGameMode* G)
		{
			UWorld* W = G->GetWorld(); if (!W || !G->Arena) return;
			const FVector Ac = G->Arena->GetActorLocation();
			// the Sky Citadel (ZoogiAncientLayout v14: island_citadel at (3000, -3500), z +450): back of the island, facing the plaza
			const FVector2D Isle = ZoogiAncient::V15() ? FVector2D(4139.f, -4829.f) : FVector2D(3000.f, -3500.f); // b162: v15 citadel
			const FVector2D Out2 = Isle.GetSafeNormal();
			FVector2D P2 = FVector2D(Ac) + Isle + Out2 * 450.f;
			float Z = Ac.Z + 450.f;
			{
				FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiThrone), true);
				if (W->LineTraceSingleByChannel(H, FVector(P2.X, P2.Y, Ac.Z + 3000.f), FVector(P2.X, P2.Y, Ac.Z - 2000.f), ECC_Visibility, Q)) Z = H.ImpactPoint.Z;
			}
			GYaw = FMath::RadiansToDegrees(FMath::Atan2(-Out2.Y, -Out2.X)); // faces the plaza
			const FRotator R(0.f, GYaw, 0.f);
			const FVector Fw = R.Vector(), Rt = FRotationMatrix(R).GetUnitAxis(EAxis::Y);
			const FVector B(P2.X, P2.Y, Z);
			const FLinearColor Gold(1.f, 0.62f, 0.08f), Red(0.55f, 0.02f, 0.08f), Pink(0.85f, 0.06f, 0.42f), Stone(0.18f, 0.16f, 0.2f), Ink(0.02f, 0.02f, 0.03f);
			const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
			const TCHAR* Cyl = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
			const TCHAR* Sph = TEXT("/Engine/BasicShapes/Sphere.Sphere");
			const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
			// dais: 3 steps
			Part(W, Cyl, B + FVector(0, 0, 30.f), R, FVector(9.f, 9.f, 0.6f), Stone);
			Part(W, Cyl, B + FVector(0, 0, 90.f), R, FVector(7.4f, 7.4f, 0.6f), Ink);
			Part(W, Cyl, B + FVector(0, 0, 150.f), R, FVector(6.f, 6.f, 0.6f), Gold);
			// red carpet down the front, toward the plaza
			Part(W, Cube, B + Fw * 640.f + FVector(0, 0, 8.f), R, FVector(9.f, 2.6f, 0.12f), Red);
			// seat, back, arms (the back is tall enough to read from the plaza)
			const FVector S0 = B + FVector(0, 0, 180.f);
			Part(W, Cube, S0 + FVector(0, 0, 70.f), R, FVector(3.4f, 3.6f, 1.4f), Red);                        // seat block
			Part(W, Cube, S0 + FVector(0, 0, 146.f), R, FVector(3.2f, 3.4f, 0.16f), Pink);                     // cushion
			Part(W, Cube, S0 - Fw * 170.f + FVector(0, 0, 420.f), R, FVector(0.6f, 4.2f, 8.4f), Red);          // back
			Part(W, Cube, S0 - Fw * 150.f + FVector(0, 0, 420.f), R, FVector(0.3f, 3.4f, 7.4f), Pink);         // back panel
			for (int32 s = -1; s <= 1; s += 2)
			{
				Part(W, Cube, S0 + Rt * (s * 200.f) + FVector(0, 0, 170.f), R, FVector(3.4f, 0.5f, 0.6f), Gold); // arm rests
				Part(W, Cube, S0 - Fw * 170.f + Rt * (s * 225.f) + FVector(0, 0, 470.f), R, FVector(0.8f, 0.6f, 9.4f), Gold); // back posts
				Part(W, Sph, S0 - Fw * 170.f + Rt * (s * 225.f) + FVector(0, 0, 960.f), R, FVector(1.1f), Gold);          // post finials
				// banners either side of the dais
				Part(W, Cyl, B + Rt * (s * 620.f) + FVector(0, 0, 500.f), R, FVector(0.35f, 0.35f, 10.f), Gold);
				Part(W, Cube, B + Rt * (s * 620.f) + Fw * 70.f + FVector(0, 0, 720.f), R, FVector(0.08f, 2.2f, 4.f), Pink);
				Part(W, Sph, B + Rt * (s * 620.f) + FVector(0, 0, 1010.f), R, FVector(0.8f), Gold);
			}
			// crown on top of the back
			const FVector Cr = S0 - Fw * 170.f + FVector(0, 0, 900.f);
			Part(W, Cyl, Cr, R, FVector(2.6f, 2.6f, 0.7f), Gold);
			for (int32 k = 0; k < 5; ++k)
			{
				const float A = FMath::DegreesToRadians(GYaw + 90.f + k * 72.f);
				Part(W, Cone, Cr + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 110.f + FVector(0, 0, 70.f), FRotator::ZeroRotator, FVector(0.6f, 0.6f, 0.9f), Gold);
			}
			Part(W, Sph, Cr + FVector(0, 0, 60.f), R, FVector(0.8f), Pink);
			GSeat = S0 + FVector(0, 0, 160.f);
			// the king: Evan's own model when it exists
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (USkeletalMesh* Sk = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/ZoogiKing/SK_ZoogiKing.SK_ZoogiKing"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				if (ASkeletalMeshActor* K = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(R, GSeat), SP))
				{ K->GetSkeletalMeshComponent()->SetSkeletalMesh(Sk); K->SetActorEnableCollision(false); GParts.Add(K); UE_LOG(LogZoogi, Log, TEXT("ZOOGI KING: SK_ZoogiKing on the throne")); }
			}
			else if (UStaticMesh* Sm = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ZoogiKing/SM_ZoogiKing.SM_ZoogiKing"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				if (AStaticMeshActor* K = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(R, GSeat), SP))
				{ K->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); K->GetStaticMeshComponent()->SetStaticMesh(Sm); K->SetActorEnableCollision(false); GParts.Add(K); UE_LOG(LogZoogi, Log, TEXT("ZOOGI KING: SM_ZoogiKing on the throne")); }
			}
			GBuilt = true;
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI KING throne built on the Sky Citadel at (%.0f, %.0f, %.0f), facing yaw %.0f (%d parts)"), B.X, B.Y, B.Z, GYaw, GParts.Num());
		}
	}

	void Tick(AZoogiGameMode* G)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoThrone"));
		if (bOff || !G || !G->Arena) return;
		const bool bWant = G->Arena->GetTheme() == EZoogiArena::Ancient;
		if (GBuilt && (!bWant || GArena.Get() != G->Arena)) Clear();
		if (bWant && !GBuilt) { GArena = G->Arena; Build(G); }
	}
	FVector SeatLocation() { return GSeat; }
}
