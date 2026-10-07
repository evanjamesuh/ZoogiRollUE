// Oct 7 (Evan 10:20 "upload all the other maps"): places the reskin-incoming builder packs (intact meshes, own materials)
// on Volcanic / Cosmic / Arabian / Tomb / Neon after the theme's own build. Court items, pillars and floors are skipped
// (the C++ court stays authoritative). Solid props outside the court become breakable pieces with a bounds collider;
// a prop whose bounds hit an existing blocking part is placed visual-only. Everything else is visual, no collision.
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiArenaPlayTest.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "ZoogiIncoming.inl"

void AZoogiArena::PlaceIncoming()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoIncoming"))) return;
	const int32 Th = (int32)Theme;
	const int32 NM = UE_ARRAY_COUNT(ZoogiInc::Meshes);
	TArray<TArray<UStaticMesh*>> Sets; Sets.SetNum(NM);
	for (const ZoogiInc::FPart& Pt : ZoogiInc::Parts)
		if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Pt.Path)) Sets[Pt.Mesh].Add(SM);
	TArray<FBox> Blocking;
	for (UActorComponent* C : Built)
		if (UPrimitiveComponent* PC = Cast<UPrimitiveComponent>(C))
			if (PC->GetCollisionEnabled() != ECollisionEnabled::NoCollision) Blocking.Add(PC->Bounds.GetBox());
	for (AZoogiArenaPiece* P : Pieces) if (P) Blocking.Add(P->GetComponentsBoundingBox(true));
	int32 NPieces = 0, NVis = 0, NMiss = 0;
	for (const ZoogiInc::FPlace& Pl : ZoogiInc::Places)
	{
		if (Pl.Theme != Th) continue;
		const TArray<UStaticMesh*>& Set = Sets[Pl.Mesh];
		if (Set.Num() == 0) { ++NMiss; continue; }
		const FVector L(Pl.X, Pl.Y, Pl.Z);
		const FRotator R(0.f, Pl.Yaw, 0.f);
		FBox Local(ForceInit);
		for (UStaticMesh* SM : Set) Local += SM->GetBoundingBox();
		bool bPiece = Pl.bPiece != 0 && Local.IsValid;
		if (bPiece)
		{
			const FBox W = Local.TransformBy(FTransform(R, L, FVector(Pl.Scale))).ExpandBy(-4.f);
			for (const FBox& B : Blocking) if (B.Intersect(W)) { bPiece = false; break; }
		}
		if (bPiece)
		{
			AZoogiArenaPiece* P = SpawnPiece(L, Pl.Yaw, TEXT("prop"), TEXT("Stone"), 200.f, false);
			if (!P) continue;
			for (UStaticMesh* SM : Set)
			{
				UStaticMeshComponent* C = P->AddShape(TEXT("Cube"), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(Pl.Scale)), nullptr);
				C->SetStaticMesh(SM);
			}
			P->AddColliderBox(Local.GetCenter() * Pl.Scale, Local.GetSize() * Pl.Scale * 0.85f, FRotator::ZeroRotator, TEXT("ZWall"), RockPM);
			P->RubbleKeepZ = 25.f;
			P->FinishBuild();
			Blocking.Add(Local.TransformBy(FTransform(R, L, FVector(Pl.Scale))));
			++NPieces;
		}
		else
		{
			for (UStaticMesh* SM : Set)
			{
				UStaticMeshComponent* C = AddMesh(SM, nullptr, false, true);
				C->SetStaticMesh(SM);
				for (int32 k = 0; k < SM->GetStaticMaterials().Num(); ++k) C->SetMaterial(k, SM->GetMaterial(k));
				C->SetRelativeLocationAndRotation(L, R);
				C->SetRelativeScale3D(FVector(Pl.Scale));
			}
			++NVis;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("Incoming pack (theme %d): %d pieces, %d visual, %d missing meshes"), Th, NPieces, NVis, NMiss);

	// -ZoogiArenaPlayTest on the non-Meadow maps: piece overlap + an orb launched out on 4 headings must still go ORB OUT.
	if (AZoogiArenaPlayTest* PT = AZoogiArenaPlayTest::MaybeSpawn(this))
	{
		{ FZoogiArenaCheck C; C.Name = TEXT("piece_overlap"); C.Kind = FZoogiArenaCheck::PieceOverlap; C.Wait = 0.2f; PT->Checks.Add(C); }
		const float Base = Theme == EZoogiArena::Neon ? 45.f : 0.f; // Neon's knockout mouths are the corners
		for (int32 g = 0; g < 4; ++g)
		{
			const float Yw = Base + 90.f * g, Rd = FMath::DegreesToRadians(Yw);
			const FVector2D Dir(FMath::Cos(Rd), FMath::Sin(Rd));
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("gap_%03.0f"), Yw); C.Kind = FZoogiArenaCheck::OrbOut;
			C.From = FVector(Dir * 300.f, 0.f); C.Vel = Dir * 2200.f; C.Wait = 3.0f;
			PT->Checks.Add(C);
		}
	}
}
