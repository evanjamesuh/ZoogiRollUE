#include "ZoogiObstacle.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

AZoogiObstacle::AZoogiObstacle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	Tags.Add(TEXT("Unbreakable")); Tags.Add(TEXT("NonDestructible")); Tags.Add(TEXT("CrystalCollapseExclude")); Tags.Add(TEXT("ZoogiObstacle"));
}

UStaticMeshComponent* AZoogiObstacle::AddVisual(UStaticMesh* Mesh, float Scale, bool bSpent)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetRelativeScale3D(FVector(Scale));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->ComponentTags.Add(TEXT("NoCamBlock"));
	C->SetVisibility(!bSpent);
	C->RegisterComponent();
	(bSpent ? Spent : Vis).Add(C);
	return C;
}

void AZoogiObstacle::BuildCollider(const TArray<TArray<FVector>>& Hulls, float Scale, UPhysicalMaterial* PM)
{
	if (Hulls.Num() == 0) return;
	Col = NewObject<UProceduralMeshComponent>(this, TEXT("ObsCollider"));
	Col->SetMobility(EComponentMobility::Movable);
	Col->SetupAttachment(Root);
	Col->bUseComplexAsSimpleCollision = false;
	Col->bUseAsyncCooking = false;
	for (const TArray<FVector>& H : Hulls)
	{
		TArray<FVector> S; S.Reserve(H.Num());
		for (const FVector& V : H) S.Add(V * Scale);
		Col->AddCollisionConvexMesh(S);
	}
	Col->SetVisibility(false);
	Col->SetHiddenInGame(true);
	Col->SetCollisionProfileName(TEXT("BlockAll"));
	Col->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // NoCamBlock
	if (PM) Col->SetPhysMaterialOverride(PM);
	Col->SetNotifyRigidBodyCollision(true);
	Col->OnComponentHit.AddDynamic(this, &AZoogiObstacle::OnColHit);
	Col->RegisterComponent();
	NumHulls = Hulls.Num();
}

void AZoogiObstacle::OnColHit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (BounceGain <= 1.f) return;
	if (AZoogiBall* B = Cast<AZoogiBall>(Other)) PendingGain.AddUnique(B); // applied after the physics step resolved the bounce
}

bool AZoogiObstacle::Overlaps(const FZoogiObsTrigger& T, const FVector& BallW, float R) const
{
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(BallW);
	switch (T.Shape)
	{
	case 0: { const FVector Q = (L - T.C).GetAbs() - T.E; return FVector(FMath::Max(Q.X, 0.f), FMath::Max(Q.Y, 0.f), FMath::Max(Q.Z, 0.f)).Size() <= R; }
	case 1: return FVector::Dist(L, T.C) <= T.E.X + R;
	case 2: return FVector2D::Distance(FVector2D(L), FVector2D(T.C)) <= T.E.X + R * 0.5f && FMath::Abs(L.Z - T.C.Z) <= T.E.Z + R;
	case 3:
	{
		const float Rho = FVector2D(L).Size();
		const float A = FMath::RadiansToDegrees(FMath::Atan2(L.Y, L.X));
		const float Rel = FMath::Fmod(A - T.D.X + 720.f, 360.f);
		return Rel <= T.D.Y - T.D.X && Rho >= T.E.X && Rho <= T.E.Y && L.Z <= T.E.Z + R + 5.f;
	}
	}
	return false;
}

void AZoogiObstacle::SetOrbShown(bool bShown)
{
	if (Spent.Num()) { for (auto& C : Vis) if (C) C->SetVisibility(bShown); for (auto& C : Spent) if (C) C->SetVisibility(!bShown); }
	else for (auto& C : Vis) if (C) C->SetVisibility(bShown);
}

void AZoogiObstacle::Fire(FZoogiObsTrigger& T, AZoogiBall* B, float Now)
{
	const float Yaw = GetActorRotation().Yaw;
	const FVector DirW = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(T.D.X, T.D.Y, 0.f)).GetSafeNormal();
	const FVector V = B->GetBallVelocity();
	const FVector Pl(V.X, V.Y, 0.f);
	switch (T.Kind)
	{
	case 1: // Boost
	{
		if (T.P[1] > 0.f) // multiplier along the axis, only moving with it
		{
			if (FVector::DotProduct(Pl, DirW) <= 0.f) return;
			B->SetPlanarVelocity(DirW * Pl.Size() * T.P[1]);
		}
		else B->SetPlanarVelocity(Pl + DirW * T.P[0]);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s boost P%d %.0f -> %.0f cm/s"), *ObsName.ToString(), B->PlayerIndex + 1, Pl.Size(), B->GetBallVelocity().Size2D());
		break;
	}
	case 2: // Launch
		if (Pl.Size() < T.P[3]) return;
		B->SetPlanarVelocity(DirW * T.P[0], T.P[1]);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s launch P%d fwd %.0f up %.0f"), *ObsName.ToString(), B->PlayerIndex + 1, T.P[0], T.P[1]);
		break;
	case 3: // Energy orb
	{
		if (T.RechargeAt > Now) return;
		UZoogiEpisodeEnergyComponent* En = B->FindComponentByClass<UZoogiEpisodeEnergyComponent>();
		const float Got = En ? En->Gain(T.P[0], FName(TEXT("obstacle_orb")), 0) : 0.f;
		T.RechargeAt = Now + T.P[1];
		SetOrbShown(false);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s energy P%d +%.0f (%s), recharge %.0f s"), *ObsName.ToString(), B->PlayerIndex + 1, Got, En ? TEXT("episode energy") : TEXT("no energy component"), T.P[1]);
		break;
	}
	case 4: // Zone entry: speed bonus (the rolling-decel change is applied while inside)
		if (T.P[2] > 0.f && Pl.Size() > 1.f) B->SetPlanarVelocity(Pl * T.P[2]);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s zone P%d decel %.0f -> %.0f"), *ObsName.ToString(), B->PlayerIndex + 1, T.Inside.FindRef(B), B->RollingDecel);
		break;
	}
}

void AZoogiObstacle::Tick(float Dt)
{
	Super::Tick(Dt);
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return;
	const float Now = W->GetTimeSeconds();
	for (FZoogiObsTrigger& T : Triggers)
		if (T.Kind == 3 && T.RechargeAt > 0.f && Now >= T.RechargeAt) { T.RechargeAt = -1.f; SetOrbShown(true); }
	for (TWeakObjectPtr<AZoogiBall>& WB : PendingGain)
	{
		AZoogiBall* B = WB.Get();
		if (!B || B->bOut || GainCool.FindRef(WB) > Now) continue;
		const FVector V = B->GetBallVelocity(); const FVector Pl(V.X, V.Y, 0.f);
		const FVector Away = FVector(B->BallLocation() - GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(Pl, Away) <= 0.f && Col) continue; // still inbound: nothing to amplify yet
		const float NewS = FMath::Min(Pl.Size() * BounceGain, FMath::Max(MaxOut, Pl.Size()));
		if (Pl.Size() > 1.f) B->SetPlanarVelocity(Pl.GetSafeNormal() * NewS);
		GainCool.Add(WB, Now + 0.25f);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s bounce P%d %.0f -> %.0f cm/s (gain %.2f)"), *ObsName.ToString(), B->PlayerIndex + 1, Pl.Size(), NewS, BounceGain);
	}
	PendingGain.Reset();
	if (Triggers.Num() == 0) return;
	for (AZoogiBall* B : G->Balls)
	{
		if (!B || B->bOut || B->bKnockedOut) continue;
		const float R = B->Sphere ? B->Sphere->GetScaledSphereRadius() : 50.f;
		const FVector P = B->BallLocation();
		for (FZoogiObsTrigger& T : Triggers)
		{
			const bool bIn = Overlaps(T, P, R);
			const bool bWas = T.Inside.Contains(B);
			if (bIn && !bWas)
			{
				T.Inside.Add(B, B->RollingDecel);
				if (T.Kind == 4) { if (T.P[0] > 0.f) B->RollingDecel = T.P[0]; else if (T.P[1] > 0.f) B->RollingDecel *= T.P[1]; }
				if (T.Cool.FindRef(B) <= Now) { Fire(T, B, Now); T.Cool.Add(B, Now + FMath::Max(T.Kind == 3 ? 0.f : (T.Kind == 4 ? T.P[3] : T.P[2]), 0.05f)); }
			}
			else if (!bIn && bWas)
			{
				const float Base = T.Inside.FindRef(B);
				if (T.Kind == 4) B->RollingDecel = Base; // leave the zone: back to the decel it had
				T.Inside.Remove(B);
			}
		}
	}
}
