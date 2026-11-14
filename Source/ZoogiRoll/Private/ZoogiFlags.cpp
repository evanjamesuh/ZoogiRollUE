// b157 (Evan Nov 6 08:29 "every map has the iconic purple and gold 'Z' zoogi flag or banner waving"): two Zoogi flags on every
// arena - gold poles just outside the court on opposite sides, a purple flag with the gold Z (Content/Data/MenuArt/Flags/
// zoogi_flag.png) waving in the wind (a procedural cloth grid, both faces). Visual only (no collision). -ZoogiNoFlags = off.
#include "ZoogiFlags.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiFlags
{
	namespace
	{
		constexpr int32 NX = 14, NY = 9;
		constexpr float FW = 500.f, FH = 320.f, PoleH = 620.f; // b160: lower + bigger (was 420 x 270 on 950)
		struct FFlag { TWeakObjectPtr<AActor> Pole; TWeakObjectPtr<UProceduralMeshComponent> Cloth; float Phase = 0.f; };
		TArray<FFlag> GF;
		TArray<TWeakObjectPtr<AActor>> GActors;
		TWeakObjectPtr<AZoogiArena> GArena;
		EZoogiArena GTheme = EZoogiArena::MAX;
		float GT = 0.f;

		UTexture2D* FlagTex()
		{
			static UTexture2D* T = nullptr;
			if (!T)
			{
				const FString P = FPaths::ProjectContentDir() / TEXT("Data/MenuArt/Flags/zoogi_flag.png");
				if (FPaths::FileExists(P)) { T = FImageUtils::ImportFileAsTexture2D(P); if (T) T->AddToRoot(); }
				if (!T) UE_LOG(LogZoogi, Warning, TEXT("ZOOGI FLAGS: zoogi_flag.png missing"));
			}
			return T;
		}
		UMaterialInterface* ClothMat(UObject* Outer)
		{
			UTexture2D* T = FlagTex();
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_AncientDecal.M_AncientDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
				if (T) M->SetTextureParameterValue(TEXT("Tex"), T);
				M->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.6f); // b160: brighter
				return M;
			}
			if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Cosmic/M_CosmicPlanet.M_CosmicPlanet"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
				if (T) M->SetTextureParameterValue(TEXT("Tex"), T);
				M->SetScalarParameterValue(TEXT("Bright"), 1.f);
				return M;
			}
			return nullptr;
		}
		AStaticMeshActor* Shape(UWorld* W, const TCHAR* Path, const FVector& L, const FVector& S, const FLinearColor& Col)
		{
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path);
			if (!M) return nullptr;
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, L, S), SP);
			if (!A) return nullptr;
			UStaticMeshComponent* C = A->GetStaticMeshComponent();
			C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(M); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (UMaterialInterface* B = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{ UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(B, A); D->SetVectorParameterValue(TEXT("Color"), Col); C->SetMaterial(0, D); }
			A->SetActorEnableCollision(false);
			GActors.Add(A);
			return A;
		}
		void Wave(FFlag& F, bool bCreate)
		{
			UProceduralMeshComponent* PM = F.Cloth.Get(); if (!PM) return;
			TArray<FVector> V; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Cl; TArray<FProcMeshTangent> Tg; TArray<int32> Tri;
			V.Reserve(NX * NY * 2);
			for (int32 Side = 0; Side < 2; ++Side)
				for (int32 j = 0; j < NY; ++j)
					for (int32 i = 0; i < NX; ++i)
					{
						const float U = (float)i / (NX - 1), Vv = (float)j / (NY - 1);
						const float Ph = GT * 5.2f - U * 4.2f + F.Phase;
						const float Amp = U * (38.f + 10.f * FMath::Sin(GT * 1.3f + F.Phase));
						V.Add(FVector(U * FW, FMath::Sin(Ph) * Amp + FMath::Sin(Ph * 1.7f + Vv * 2.f) * Amp * 0.25f, -Vv * FH - U * U * 18.f));
						const float Dy = FMath::Cos(Ph) * Amp * -4.2f / FW;
						FVector Nn(-Dy, 1.f, 0.f); Nn.Normalize();
						N.Add(Side == 0 ? Nn : -Nn);
						UV.Add(FVector2D(Side == 0 ? U : U, Vv)); // the back reads mirrored, like real cloth
						Cl.Add(FLinearColor::White);
						Tg.Add(FProcMeshTangent(1.f, 0.f, 0.f));
					}
			if (bCreate)
			{
				for (int32 Side = 0; Side < 2; ++Side)
				{
					const int32 O = Side * NX * NY;
					for (int32 j = 0; j < NY - 1; ++j)
						for (int32 i = 0; i < NX - 1; ++i)
						{
							const int32 A = O + j * NX + i, B = A + 1, C = A + NX, D = C + 1;
							if (Side == 0) { Tri.Append({ A, C, B, B, C, D }); }
							else { Tri.Append({ A, B, C, B, D, C }); }
						}
				}
				PM->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Cl, Tg, false);
			}
			else PM->UpdateMeshSection_LinearColor(0, V, N, UV, Cl, Tg);
		}
		void Clear()
		{
			for (TWeakObjectPtr<AActor>& A : GActors) if (AActor* X = A.Get()) X->Destroy();
			GActors.Reset(); GF.Reset(); GTheme = EZoogiArena::MAX;
		}
		void Build(AZoogiGameMode* G)
		{
			UWorld* W = G->GetWorld(); if (!W || !G->Arena) return;
			const FVector C = G->Arena->GetActorLocation();
			const float Rk = G->Arena->GetKnockoffRadius();
			const FLinearColor Gold(1.f, 0.62f, 0.08f);
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiFlags), true);
			for (AZoogiBall* B : G->Balls) if (B) Q.AddIgnoredActor(B);
			for (int32 k = 0; k < 4; ++k) // b160: four flags (was 2)
			{
				const float Yaw = 45.f + 90.f * k;
				const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
				FVector Base = C + Dir * (Rk * 0.97f);
				for (float R : { Rk + 380.f, Rk + 220.f, Rk * 0.97f })
				{
					const FVector P = C + Dir * R; FHitResult H;
					if (W->LineTraceSingleByChannel(H, P + FVector(0, 0, 2500.f), P - FVector(0, 0, 1500.f), ECC_Visibility, Q) && H.ImpactPoint.Z > C.Z - 400.f && H.ImpactPoint.Z < C.Z + 1800.f && H.ImpactNormal.Z > 0.6f)
					{ Base = H.ImpactPoint; break; }
				}
				AStaticMeshActor* Pole = Shape(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Base + FVector(0, 0, PoleH * 0.5f), FVector(0.26f, 0.26f, PoleH / 100.f), Gold);
				Shape(W, TEXT("/Engine/BasicShapes/Sphere.Sphere"), Base + FVector(0, 0, PoleH + 25.f), FVector(0.55f), Gold);
				Shape(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Base + FVector(0, 0, 20.f), FVector(1.1f, 1.1f, 0.4f), FLinearColor(0.12f, 0.04f, 0.2f)); // base plinth
				if (!Pole) continue;
				UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(Pole);
				PM->SetMobility(EComponentMobility::Movable);
				PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PM->bUseAsyncCooking = false;
				PM->RegisterComponent();
				// flies along the tangent (wind sweeping round the bowl), top edge just under the finial
				PM->SetWorldLocationAndRotation(Base + FVector(0, 0, PoleH - 20.f), FRotator(0.f, Yaw + 90.f, 0.f));
				if (UMaterialInterface* M = ClothMat(Pole)) PM->SetMaterial(0, M);
				FFlag F; F.Pole = Pole; F.Cloth = PM; F.Phase = k * 1.7f;
				GF.Add(F);
				Wave(GF.Last(), true);
			}
			GTheme = G->Arena->GetTheme();
			UE_LOG(LogZoogi, Log, TEXT("ZOOGI FLAGS: %d flags on %s (court r%.0f)"), GF.Num(), *ZoogiUtil::ArenaName(GTheme), Rk);
		}
	}

	void Tick(AZoogiGameMode* G, float Dt)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFlags"));
		if (bOff || !G || !G->Arena) return;
		const EZoogiArena Th = G->Arena->GetTheme();
		if (GTheme != EZoogiArena::MAX && (GArena.Get() != G->Arena || GTheme != Th)) Clear();
		if (GTheme == EZoogiArena::MAX)
		{
			if (G->Phase == EZoogiPhase::Forming) return; // after the arena has formed
			GArena = G->Arena; Build(G);
			if (GTheme == EZoogiArena::MAX) GTheme = Th; // nothing built: don't retry every frame
			return;
		}
		GT += FMath::Min(Dt, 0.05f);
		for (FFlag& F : GF) Wave(F, false);
	}
}
