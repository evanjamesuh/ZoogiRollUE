// Nov 1 (Evan: "on the volcano stage add the [Inferno World island] as shown next to the arena"): the Inferno World Free
// knight-statue lava island (Demonstration map, ZoogiInfernoIsland.inl) rebuilt beside the Volcanic court, one ISM per mesh,
// with Stylized VFX Pack fires on its braziers. -ZoogiNoInferno = off; -ZoogiInfernoAt=X,Y  -ZoogiInfernoScale=  -ZoogiInfernoYaw=
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiInfernoIsland.inl"

void AZoogiArena::AddInfernoIsland(const FVector& At0, float Scale0, float Yaw0)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoInferno"))) return;
	FVector At = At0; float Scale = Scale0, Yaw = Yaw0;
	{
		FString S; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiInfernoAt="), S)) { FString A, B; if (S.Split(TEXT(","), &A, &B)) { At.X = FCString::Atof(*A); At.Y = FCString::Atof(*B); } }
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiInfernoScale="), Scale);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiInfernoYaw="), Yaw);
	}
	const FTransform Island(FRotator(0.f, Yaw, 0.f), At, FVector(Scale));
	TMap<UStaticMesh*, UInstancedStaticMeshComponent*> Ism;
	int32 N = 0, Missing = 0;
	for (const FZoogiInfernoPiece& P : GInfernoIsland)
	{
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, P.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) { ++Missing; continue; }
		UInstancedStaticMeshComponent*& C = Ism.FindOrAdd(M);
		if (!C)
		{
			C = NewObject<UInstancedStaticMeshComponent>(this);
			C->SetStaticMesh(M);
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(M->GetBounds().SphereRadius > 400.f); // the big rock / platform / statue pieces only
			C->RegisterComponent();
			Built.Add(C);
		}
		const FTransform Local(FRotator(P.Pitch, P.Yaw, P.Roll), FVector(P.X, P.Y, P.Z), FVector(P.SX, P.SY, P.SZ));
		C->AddInstance(Local * Island, true);
		++N;
	}
	int32 Fires = 0;
	for (const FVector& F : GInfernoFires)
		if (AZoogiFX::StylizedLoop(GetWorld(), (Fires & 1) ? TEXT("Fire_v2") : TEXT("Fire"), Island.TransformPosition(F), Scale * 1.6f, Root)) ++Fires;
	// warm lava key light on the face that looks at the court (the scene sun is behind it from the match camera)
	{
		const FVector ToCourt = FVector(-At.X, -At.Y, 0.f).GetSafeNormal();
		AddPointLight(At + ToCourt * (3200.f * Scale) + FVector(0.f, 0.f, 2600.f * Scale), FLinearColor(1.f, 0.55f, 0.28f), 900.f, 9000.f * Scale);
	}
	UE_LOG(LogZoogi, Log, TEXT("INFERNO island: %d pieces in %d meshes (%d missing), %d brazier fires, at (%.0f,%.0f,%.0f) scale %.2f yaw %.0f"), N, Ism.Num(), Missing, Fires, At.X, At.Y, At.Z, Scale, Yaw);
}
