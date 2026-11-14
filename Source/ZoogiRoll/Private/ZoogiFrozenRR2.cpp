// Frozen rev_realism_v2 visual-only swap (Oct 22). Layout, gameplay colliders, obstacles and debris are untouched:
// the generated visuals of the matched pieces are hidden (SetHiddenInGame, collision stays) and the pack meshes are
// attached as NoCollision visuals. Repeated static Ring 2 pieces (orb pockets, crystal bumpers) use one HISM per mesh.
// Pack meshes are used at 1.0 on top of the layout: the C++ layout already matches the pack footprints
// (pillar 435 cm, obelisk 168 cm vs ObeliskH 170, bumper r37 h117 vs collider r35 h120, pocket r142 vs back wall 127).
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiObstacle.h"
#include "ZoogiTypes.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

namespace ZoogiRR2
{
	static UStaticMesh* Load(const TCHAR* N)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Frozen/RR2/Meshes/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	static constexpr float PackScale = 1.0f;
	static constexpr float R2Z = -30.f; // ZoogiFrozenRing2.cpp
}

void AZoogiArena::ApplyFrozenRR2Visuals()
{
	using namespace ZoogiRR2;
	UWorld* W = GetWorld();
	if (!W) return;
	UStaticMesh* Pillar = Load(TEXT("SM_Frozen_ResonancePillar"));
	UStaticMesh* Obelisk = Load(TEXT("SM_Frozen_IceObelisk_s0"));
	UStaticMesh* Pocket = Load(TEXT("SM_FrozenOrbPocket"));
	UStaticMesh* Bumper = Load(TEXT("SM_Frozen_CrystalBumper"));
	int32 NPiece = 0, NPocket = 0, NBumper = 0, NHidden = 0;

	// ---- per-piece visuals (pillars / obelisks rise with Forming and obelisks break, so no HISM here)
	for (TActorIterator<AZoogiArenaPiece> It(W); It; ++It)
	{
		AZoogiArenaPiece* P = *It;
		const FString K = P->GetKind().ToString();
		UStaticMesh* M = K.StartsWith(TEXT("pillar_")) ? Pillar : (K == TEXT("obelisk") ? Obelisk : nullptr);
		if (!M) continue;
		const float BaseZ = P->GetActorLocation().Z;
		const float KeepAbove = (M == Pillar) ? 420.f : 1.0e6f; // pillar: keep the generated orb + light above the pack cup
		TArray<UPrimitiveComponent*> Cs; P->GetComponents(Cs);
		for (UPrimitiveComponent* C : Cs)
		{
			if (!C || !C->IsVisible() || C->bHiddenInGame) continue;
			if (C->Bounds.Origin.Z - BaseZ > KeepAbove) continue;
			C->SetHiddenInGame(true); ++NHidden;
		}
		UStaticMeshComponent* V = NewObject<UStaticMeshComponent>(P, NAME_None);
		V->SetStaticMesh(M);
		V->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		V->SetCanEverAffectNavigation(false);
		V->SetMobility(EComponentMobility::Movable);
		V->SetupAttachment(P->GetRootComponent());
		V->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(PackScale)));
		V->RegisterComponent();
		P->AdoptPart(V); // broken state: handled like the other low parts (debris spawning unchanged)
		++NPiece;
	}

	// ---- Ring 2 instanced visuals
	auto MakeH = [&](UStaticMesh* M) -> UHierarchicalInstancedStaticMeshComponent*
	{
		if (!M) return nullptr;
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None);
		H->SetStaticMesh(M);
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCanEverAffectNavigation(false);
		H->SetupAttachment(GetRootComponent());
		H->RegisterComponent();
		return H;
	};
	UHierarchicalInstancedStaticMeshComponent* HP = MakeH(Pocket);
	UHierarchicalInstancedStaticMeshComponent* HB = MakeH(Bumper);
	TArray<FVector> PocketC, BumperC;
	for (AZoogiObstacle* O : Obstacles)
	{
		if (!O) continue;
		const FString N = O->ObsName.ToString();
		if (HP && N.StartsWith(TEXT("Ring2_OuterOrb_")))
		{
			const FVector C = O->GetActorLocation();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(C.Y, C.X));
			const FVector Base(C.X, C.Y, R2Z);
			HP->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Base, FVector(PackScale)), true);
			PocketC.Add(Base); ++NPocket;
		}
		else if (HB && N.StartsWith(TEXT("Ring2_CrystalBumper_")))
		{
			const FVector C = O->GetActorLocation();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(C.Y, C.X));
			HB->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), C, FVector(PackScale)), true);
			BumperC.Add(C); ++NBumper;
		}
	}
	// hide the generated Ring 2 proc visuals of those pieces (pocket floor / back wall / cup, bumper crystal); collision is on the obstacles
	TArray<UProceduralMeshComponent*> Procs; GetComponents(Procs);
	for (UProceduralMeshComponent* C : Procs)
	{
		if (!C || C->bHiddenInGame) continue;
		const FVector O = C->Bounds.Origin, E = C->Bounds.BoxExtent;
		bool bHide = false;
		for (const FVector& B : BumperC) if (FVector::Dist2D(O, B) < 30.f && E.X < 60.f && E.Y < 60.f && FMath::Abs(O.Z - B.Z - 60.f) < 40.f) bHide = true;
		for (const FVector& B : PocketC) if (FVector::Dist2D(O, B) < 140.f && E.X < 180.f && E.Y < 180.f && O.Z < B.Z + 80.f && O.Z > B.Z - 40.f) bHide = true;
		if (bHide) { C->SetHiddenInGame(true); ++NHidden; }
	}
	UE_LOG(LogTemp, Log, TEXT("FrozenRR2: visuals pieces=%d pockets=%d bumpers=%d hidden=%d (pillar=%d obelisk=%d pocket=%d bumper=%d)"),
		NPiece, NPocket, NBumper, NHidden, Pillar != nullptr, Obelisk != nullptr, Pocket != nullptr, Bumper != nullptr);
}
