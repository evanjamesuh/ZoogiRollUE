#include "ZoogiRock.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.h"
#include "ZoogiArtifactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

// Realism pass (Oct 4 evening). Everything here is cosmetic: no debris piece has collision or a physics body, every
// "bounce / roll / settle" is scripted against the court plane (Z = 0), and the hit logic in OnBallHit is unchanged.
namespace
{
	constexpr int32 NumCrackPieces = 5;   // pool [0] = impact flake, [1..4] = pebbles; shatter shards follow
	constexpr int32 GritPerSet = 30;      // two instanced grit sets (dark / fresh): 2 draw calls for all grit
	constexpr int32 CrackGritPerSet = 10; // crack uses [0, 10) of each set, shatter the rest
	constexpr int32 PuffCount = 14;
	constexpr int32 BodyCols = 44;        // boulder lat/long grid (even: the split uses half the columns per side)
	constexpr int32 BodyRows = 26;        // rows from BodyElMin up to the pole
	constexpr float BodyElMin = -0.2443461f; // -14 deg: the bottom edge stays under the turf

	float RowEl(int32 J, int32 Rows = BodyRows) { return FMath::Lerp(BodyElMin, PI * 0.5f, (float)J / (float)(Rows - 1)); }
	FVector DirAE(float Az, float El) { return FVector(FMath::Cos(El) * FMath::Cos(Az), FMath::Cos(El) * FMath::Sin(Az), FMath::Sin(El)); }
	float Wrap2Pi(float X) { X = FMath::Fmod(X, 2.f * PI); return X < 0.f ? X + 2.f * PI : X; }
	float SMin(float A, float B, float K)
	{
		const float H = FMath::Clamp(0.5f + 0.5f * (B - A) / K, 0.f, 1.f);
		return FMath::Lerp(B, A, H) - K * H * (1.f - H);
	}
	float ScarRadius(float Radius) { return FMath::Clamp(Radius * 0.15f, 11.f, 20.f); }
	const FTransform& HiddenXf()
	{
		static const FTransform T(FQuat::Identity, FVector(0.f, 0.f, -300.f), FVector(0.001f));
		return T;
	}

	/** Mesh section builder. UE front faces: (C - A) x (B - A) points out of the face (as in GenerateBoxMesh). */
	struct FMeshBuf
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tan;

		int32 Add(const FVector& P, const FVector& Nrm, float Alpha = 1.f)
		{
			const int32 I = V.Add(P);
			N.Add(Nrm);
			UV.Add(FVector2D(P.X, P.Y) * 0.01f);
			C.Add(FLinearColor(1.f, 1.f, 1.f, Alpha));
			FVector Tx = FVector::CrossProduct(Nrm, FVector::UpVector);
			if (Tx.SizeSquared() < 1e-4) Tx = FVector::CrossProduct(Nrm, FVector::ForwardVector);
			Tan.Add(FProcMeshTangent(Tx.GetSafeNormal(), false));
			return I;
		}
		/** Indexed triangle, wound so that its front faces Out. */
		void Tri(int32 A, int32 B, int32 Cc, const FVector& Out)
		{
			const FVector Fn = FVector::CrossProduct(V[Cc] - V[A], V[B] - V[A]);
			if (FVector::DotProduct(Fn, Out) >= 0.0) { T.Add(A); T.Add(B); T.Add(Cc); }
			else { T.Add(A); T.Add(Cc); T.Add(B); }
		}
		/** Flat-shaded triangle (own vertices, face normal towards Out). Degenerate triangles are skipped. */
		void Flat(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& Out)
		{
			FVector Fn = FVector::CrossProduct(P2 - P0, P1 - P0);
			const double L = Fn.Size();
			if (L < 1e-3) return;
			Fn /= L;
			if (FVector::DotProduct(Fn, Out) < 0.0)
			{
				Fn = -Fn;
				const int32 A = Add(P0, Fn), B = Add(P2, Fn), Cc = Add(P1, Fn);
				T.Add(A); T.Add(B); T.Add(Cc);
			}
			else
			{
				const int32 A = Add(P0, Fn), B = Add(P1, Fn), Cc = Add(P2, Fn);
				T.Add(A); T.Add(B); T.Add(Cc);
			}
		}
	};

	void Upload(UProceduralMeshComponent* M, int32 Section, const FMeshBuf& B)
	{
		if (!M) return;
		if (B.T.Num() == 0) { M->ClearMeshSection(Section); return; }
		const TArray<FVector2D> NoUV; // full overload (UV1..UV3 empty) so it matches every 5.x signature
	M->CreateMeshSection_LinearColor(Section, B.V, B.T, B.N, B.UV, NoUV, NoUV, NoUV, B.C, B.Tan, false, false);
	}

	void GridTris(FMeshBuf& B, int32 Base, int32 Cols, int32 Rows)
	{
		for (int32 j = 0; j < Rows - 1; ++j)
		{
			for (int32 i = 0; i < Cols; ++i)
			{
				const int32 A = Base + j * (Cols + 1) + i, Bb = A + 1, Cc = A + Cols + 1, D = Cc + 1;
				const FVector Out = B.N[A] + B.N[D];
				B.Tri(A, Bb, D, Out);
				B.Tri(A, D, Cc, Out);
			}
		}
	}

	/** Closed-top lat/long shell of a boulder shape (smooth normals, seam column duplicated with identical normals). */
	template <typename TShape>
	void BuildShell(const TShape& S, int32 Cols, int32 Rows, FMeshBuf& Out)
	{
		const int32 Base = Out.V.Num();
		for (int32 j = 0; j < Rows; ++j)
		{
			const float El = RowEl(j, Rows);
			for (int32 i = 0; i <= Cols; ++i)
			{
				const FVector U = DirAE(2.f * PI * (float)i / (float)Cols, El);
				Out.Add(S.Point(U), S.Normal(U));
			}
		}
		GridTris(Out, Base, Cols, Rows);
	}

	/** Irregular angular stone: a jittered hexahedron, optionally pinched into a wedge, a slab or a spire. 8 corners. */
	void MakeShardCorners(FRandomStream& Rng, int32 Type, const FVector& Size, TArray<FVector>& C)
	{
		static const float Sx[8] = { -1.f, 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f };
		static const float Sy[8] = { 1.f, 1.f, -1.f, -1.f, 1.f, 1.f, -1.f, -1.f };
		static const float Sz[8] = { 1.f, 1.f, 1.f, 1.f, -1.f, -1.f, -1.f, -1.f };
		const FVector H = Size * 0.5f;
		C.SetNum(8);
		for (int32 i = 0; i < 8; ++i)
		{
			C[i] = FVector(Sx[i] * H.X * Rng.FRandRange(0.6f, 1.25f), Sy[i] * H.Y * Rng.FRandRange(0.6f, 1.25f), Sz[i] * H.Z * Rng.FRandRange(0.6f, 1.2f))
				+ FVector(Rng.FRandRange(-0.18f, 0.18f) * H.X, Rng.FRandRange(-0.18f, 0.18f) * H.Y, Rng.FRandRange(-0.15f, 0.15f) * H.Z);
		}
		if (Type == 1) // wedge: the top face pinched into a ridge
		{
			const float Off = Rng.FRandRange(-0.3f, 0.3f) * H.Y;
			for (int32 i = 0; i < 4; ++i) C[i].Y = Off + C[i].Y * Rng.FRandRange(0.05f, 0.25f);
		}
		else if (Type == 2) // slab / flake: thin with a tilted top
		{
			for (int32 i = 0; i < 8; ++i) C[i].Z *= 0.45f;
			C[0].Z += H.Z * 0.35f;
			C[1].Z += H.Z * 0.2f;
		}
		else if (Type == 3) // spire: top face collapsed towards an off-centre tip
		{
			const FVector Tip(Rng.FRandRange(-0.4f, 0.4f) * H.X, Rng.FRandRange(-0.4f, 0.4f) * H.Y, H.Z * Rng.FRandRange(0.9f, 1.3f));
			for (int32 i = 0; i < 4; ++i) C[i] = FMath::Lerp(Tip, C[i], Rng.FRandRange(0.1f, 0.3f));
		}
		FVector Cen = FVector::ZeroVector;
		for (const FVector& P : C) Cen += P;
		Cen /= 8.f;
		for (FVector& P : C) P -= Cen; // pivot at the centroid so the tumble looks right
	}

	const int32 ShardFaces[6][4] = { { 0, 1, 2, 3 }, { 4, 7, 6, 5 }, { 0, 4, 5, 1 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 }, { 3, 7, 4, 0 } };

	/** Appends a shard as 12 flat facets; faces whose bit is set in FreshMask go to the fresh-break section. */
	void AppendShard(const TArray<FVector>& C, const FTransform& Xf, uint32 FreshMask, FMeshBuf& Outer, FMeshBuf& Fresh)
	{
		FVector P[8];
		FVector Cen = FVector::ZeroVector;
		for (int32 i = 0; i < 8; ++i) { P[i] = Xf.TransformPosition(C[i]); Cen += P[i]; }
		Cen /= 8.f;
		for (int32 f = 0; f < 6; ++f)
		{
			FMeshBuf& B = (FreshMask & (1u << f)) ? Fresh : Outer;
			const int32* Q = ShardFaces[f];
			const FVector A = P[Q[0]], Bp = P[Q[1]], Cc = P[Q[2]], D = P[Q[3]];
			// Non-planar quads split along alternating diagonals: two facets per face read as chipped stone.
			if (((f + (int32)(FreshMask & 7u)) & 1) == 0)
			{
				B.Flat(A, Bp, Cc, (A + Bp + Cc) / 3.f - Cen);
				B.Flat(A, Cc, D, (A + Cc + D) / 3.f - Cen);
			}
			else
			{
				B.Flat(A, Bp, D, (A + Bp + D) / 3.f - Cen);
				B.Flat(Bp, Cc, D, (Bp + Cc + D) / 3.f - Cen);
			}
		}
	}

	UMaterialInterface* DustMaterial()
	{
		// /Game/Materials/M_ZoogiDust (Scripts/make_dust_material.py): lit translucent, soft edge, never emissive.
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_ZoogiDust.M_ZoogiDust"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

// ---------------------------------------------------------------------------------------------------------------- shape

void AZoogiRock::FShape::Init(const FVector& InSemi, FRandomStream& Rng, float InLump)
{
	Semi = InSemi;
	Lump = InLump;
	O1 = FVector(Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f));
	O2 = FVector(Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f));
	O3 = FVector(Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f), Rng.FRandRange(-50.f, 50.f));
	for (int32 k = 0; k < 5; ++k)
	{
		// A few weathered flat faces (sides and shoulders) so it reads as a boulder, not a blob.
		const float Az = Rng.FRandRange(0.f, 2.f * PI);
		const float Z = Rng.FRandRange(-0.05f, 0.8f);
		const float Rxy = FMath::Sqrt(FMath::Max(0.f, 1.f - Z * Z));
		FacetN[k] = FVector(FMath::Cos(Az) * Rxy, FMath::Sin(Az) * Rxy, Z);
		FacetH[k] = Rng.FRandRange(0.87f, 0.96f);
	}
	DentR = 0.f;
	DentD = 0.f;
}

float AZoogiRock::FShape::RadiusScale(const FVector& U) const
{
	float Nz = 0.075f * FMath::PerlinNoise3D(U * 1.7f + O1) + 0.035f * FMath::PerlinNoise3D(U * 3.9f + O2) + 0.014f * FMath::PerlinNoise3D(U * 8.5f + O3);
	Nz *= Lump;
	float R = 1.f + (Nz > 0.f ? Nz * 0.5f : Nz); // bumps stay mostly inside the collider footprint
	for (int32 k = 0; k < 5; ++k)
	{
		const float Cs = (float)FVector::DotProduct(U, FacetN[k]);
		if (Cs > 0.15f) R = SMin(R, FacetH[k] / Cs, 0.05f);
	}
	return FMath::Clamp(R, 0.8f, 1.04f);
}

FVector AZoogiRock::FShape::Point(const FVector& U) const
{
	FVector P = U * RadiusScale(U) * Semi;
	if (DentD > 0.f && DentR > 0.f)
	{
		const float D = (float)FVector::Dist(P, DentC);
		if (D < DentR)
		{
			const float X = 1.f - FMath::Square(D / DentR);
			P -= DentN * (DentD * X * X); // smooth crater where the flake popped off
		}
	}
	return P;
}

FVector AZoogiRock::FShape::Normal(const FVector& U) const
{
	const FVector T1 = FVector::CrossProduct(U, FMath::Abs(U.Z) < 0.95 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	const FVector T2 = FVector::CrossProduct(U, T1);
	const float E = 0.012f;
	const FVector A = Point((U + T1 * E).GetSafeNormal()) - Point((U - T1 * E).GetSafeNormal());
	const FVector B = Point((U + T2 * E).GetSafeNormal()) - Point((U - T2 * E).GetSafeNormal());
	FVector N = FVector::CrossProduct(A, B).GetSafeNormal();
	if (N.IsNearlyZero()) N = (U / Semi).GetSafeNormal();
	if (FVector::DotProduct(N, Point(U)) < 0.0) N = -N;
	return N;
}

// ---------------------------------------------------------------------------------------------------------------- setup

AZoogiRock::AZoogiRock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while debris / dust is moving
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* AZoogiRock::MakePart(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(bShadow);
	if (Mat) C->SetMaterial(0, Mat);
	C->RegisterComponent();
	return C;
}

UProceduralMeshComponent* AZoogiRock::MakeProc(bool bShadow)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(bShadow);
	C->RegisterComponent();
	return C;
}

void AZoogiRock::InitRock(float InRadius, float InHeight, float InYaw, const FLinearColor& ColA, const FLinearColor& ColB, UPhysicalMaterial* PM, int32 InSeed)
{
	Radius = InRadius; Height = InHeight; Yaw = InYaw; Seed = InSeed;
	UStaticMesh* Sphere = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));

	// Materials: weathered surface (same as before), a slightly duller cracked surface, near-black gaps, lighter chipped
	// edges, a light fresh-stone scar, dark fracture faces. Moving pieces use flat SurfaceMIDs (GroundMID's world-space
	// noise would swim on them) and the instanced grit must use M_ZoogiSurface (ISM usage flag).
	const FLinearColor Stone = ColA;
	const FLinearColor Fresh(FMath::Min(Stone.R * 1.5f + 0.02f, 0.6f), FMath::Min(Stone.G * 1.5f + 0.015f, 0.6f), FMath::Min(Stone.B * 1.5f, 0.6f), 1.f);
	MatIntact = ZoogiUtil::GroundMID(this, ColA, ColB, 0.02f, 0.85f);
	MatCracked = ZoogiUtil::GroundMID(this, ColA * 0.88f, ColB * 0.8f, 0.02f, 0.88f);
	MatGap = ZoogiUtil::SurfaceMID(this, FLinearColor(Stone.R * 0.07f, Stone.G * 0.065f, Stone.B * 0.06f), 0.95f);
	MatEdge = ZoogiUtil::SurfaceMID(this, Stone * 1.3f, 0.9f);
	MatScar = ZoogiUtil::SurfaceMID(this, Fresh, 0.88f);
	MatFracture = ZoogiUtil::SurfaceMID(this, FLinearColor(Stone.R * 0.2f, Stone.G * 0.19f, Stone.B * 0.17f), 0.95f);
	MatFreshDark = ZoogiUtil::SurfaceMID(this, FLinearColor(Stone.R * 0.6f, Stone.G * 0.56f, Stone.B * 0.5f), 0.92f);
	const FLinearColor ShardBase = FMath::Lerp(ColA, ColB, 0.18f);
	MatShard.Reset();
	MatShard.Add(ZoogiUtil::SurfaceMID(this, ShardBase, 0.9f));
	MatShard.Add(ZoogiUtil::SurfaceMID(this, ShardBase * 0.86f, 0.9f));
	MatShard.Add(ZoogiUtil::SurfaceMID(this, ShardBase * 0.74f, 0.92f));
	DustCol = FMath::Lerp(Stone, FLinearColor(0.34f, 0.31f, 0.26f), 0.55f);
	UMaterialInterface* DustMat = DustMaterial();
	bHasDust = DustMat != nullptr;
	if (!bHasDust) UE_LOG(LogZoogi, Warning, TEXT("Rock %s: /Game/Materials/M_ZoogiDust missing (run Scripts/make_dust_material.py); using NS_Dust fallback"), *GetName());

	// Solid collider: invisible cylinder with the footprint (same as the old static rim boulders).
	Collider = NewObject<UStaticMeshComponent>(this);
	Collider->SetStaticMesh(Cyl);
	Collider->SetMobility(EComponentMobility::Movable);
	Collider->SetupAttachment(Root);
	Collider->SetCollisionProfileName(TEXT("BlockAll"));
	Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collider->SetVisibility(false);
	Collider->SetCastShadow(false);
	if (PM) Collider->SetPhysMaterialOverride(PM);
	Collider->SetRelativeScale3D(FVector(Radius * 2.f / 100.f, Radius * 2.f / 100.f, Height / 100.f));
	Collider->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Collider->RegisterComponent();

	// Visual: a procedural boulder (same footprint as the old squashed sphere, semi-axes R, 0.9R, H) sunk into the turf,
	// plus a smaller stone beside it. Seeded, so every rock keeps its look across matches.
	FRandomStream Rng(Seed);
	Shape.Init(FVector(Radius, Radius * 0.9f, Height), Rng, 1.f);
	Body = MakeProc(true);
	Body->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	Side = MakeProc(true);
	BuildSide(Rng);
	CrackMesh = MakeProc(false); // decals: no shadow (they sit ~1 cm off the surface)
	CrackMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	CrackMesh->SetMaterial(0, MatGap);
	CrackMesh->SetMaterial(1, MatEdge);
	CrackMesh->SetMaterial(2, MatScar);
	CrackMesh->SetVisibility(false);
	Rubble = MakeProc(true);
	Rubble->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	BuildRubble(Rng);
	BuildChunks(Rng);

	// Grit: two instanced sets of small stone chips (dark weathered / light fresh), hidden until a hit.
	UMaterialInterface* GritMats[2] = { ZoogiUtil::SurfaceMID(this, Stone * 0.75f, 0.95f), ZoogiUtil::SurfaceMID(this, Fresh * 0.9f, 0.9f) };
	GritParts.SetNum(GritPerSet * 2);
	for (int32 s = 0; s < 2; ++s)
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetStaticMesh(Cube);
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->SetupAttachment(Root);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetGenerateOverlapEvents(false);
		ISM->SetCanEverAffectNavigation(false);
		ISM->SetCastShadow(false);
		ISM->RegisterComponent();
		ISM->SetMaterial(0, GritMats[s]);
		for (int32 k = 0; k < GritPerSet; ++k) ISM->AddInstance(HiddenXf());
		ISM->SetVisibility(false);
		GritISM.Add(ISM);
	}

	// Dust: soft translucent puffs (pool), only with the dust material.
	if (bHasDust)
	{
		PuffState.SetNum(PuffCount);
		for (int32 k = 0; k < PuffCount; ++k)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(DustMat, this);
			MID->SetVectorParameterValue(TEXT("Color"), DustCol);
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
			MID->SetScalarParameterValue(TEXT("Soft"), 1.f);
			MID->SetScalarParameterValue(TEXT("FadeDistance"), 30.f);
			UStaticMeshComponent* P = MakePart(Sphere, MID, false);
			P->SetVisibility(false);
			Puffs.Add(P);
			PuffMIDs.Add(MID);
		}
	}
	ResetRock();
}

void AZoogiRock::BuildSide(FRandomStream& Rng)
{
	FShape S;
	S.Init(FVector(Radius * 0.45f, Radius * 0.4f, Height * 0.55f), Rng, 1.2f);
	FMeshBuf B;
	BuildShell(S, 24, 14, B);
	Upload(Side, 0, B);
	Side->SetMaterial(0, MatIntact);
	const float YR = FMath::DegreesToRadians(Yaw);
	Side->SetRelativeLocation(FVector(-FMath::Sin(YR), FMath::Cos(YR), 0.f) * Radius * 0.85f);
	Side->SetRelativeRotation(FRotator(0.f, Yaw + Rng.FRandRange(0.f, 360.f), 0.f));
}

void AZoogiRock::BuildChunks(FRandomStream& Rng)
{
	// Pool: [0] flake (its underside is fresh stone), [1..4] pebbles, then ChunkCount shards in three size classes
	// (2 big, 3 medium, 3 small per 8) with darker fresh-break faces. One component each, built once per rock.
	const int32 N = NumCrackPieces + FMath::Max(0, ChunkCount);
	Chunks.SetNum(N);
	const float Rs = ScarRadius(Radius);
	for (int32 i = 0; i < N; ++i)
	{
		FVector Sz;
		int32 Type = 0;
		uint32 Mask = 0;
		bool bLightFresh = true;
		if (i == 0)
		{
			Sz = FVector(Rs * 1.5f, Rs * 1.15f, Rs * 0.5f);
			Type = 2;
			Mask = 1u << 1;
		}
		else if (i < NumCrackPieces)
		{
			const float S = FMath::Clamp(Radius * Rng.FRandRange(0.035f, 0.065f), 3.f, 8.f);
			Sz = FVector(S * Rng.FRandRange(0.8f, 1.3f), S * Rng.FRandRange(0.7f, 1.1f), S * Rng.FRandRange(0.5f, 0.9f));
			Type = Rng.RandRange(0, 3);
			Mask = 1u << Rng.RandRange(0, 5);
		}
		else
		{
			const int32 Cls = (i - NumCrackPieces) % 8;
			const float F = Cls < 2 ? Rng.FRandRange(0.3f, 0.42f) : (Cls < 5 ? Rng.FRandRange(0.17f, 0.26f) : Rng.FRandRange(0.09f, 0.15f));
			const float S = Radius * F;
			Sz = FVector(S * Rng.FRandRange(0.9f, 1.3f), S * Rng.FRandRange(0.6f, 1.f), S * Rng.FRandRange(0.4f, 0.8f));
			Type = Rng.RandRange(0, 3);
			Mask = (1u << Rng.RandRange(0, 5)) | (1u << Rng.RandRange(0, 5));
			bLightFresh = false;
		}
		FChunk& C = Chunks[i];
		MakeShardCorners(Rng, Type, Sz, C.Corners);
		C.Size = (float)Sz.GetMax();
		FMeshBuf Outer, FreshB;
		AppendShard(C.Corners, FTransform::Identity, Mask, Outer, FreshB);
		UProceduralMeshComponent* M = MakeProc(i == 0 || i >= NumCrackPieces); // pebbles are too small to need shadows
		Upload(M, 0, Outer);
		Upload(M, 1, FreshB);
		M->SetMaterial(0, MatShard[i % MatShard.Num()]);
		M->SetMaterial(1, bLightFresh ? MatScar.Get() : MatFreshDark.Get());
		M->SetVisibility(false);
		ChunkPool.Add(M);
	}
}

void AZoogiRock::BuildRubble(FRandomStream& Rng)
{
	// What's left after a shatter (no collision, low enough to roll over): the sheared-off base of the boulder with a
	// rough fresh break on top, loose angular stones, and a pale stone-powder stain on the turf.
	FMeshBuf Outer, FreshB, Powder;
	const float Floor = -(float)GetActorLocation().Z; // rock-space court height
	const FVector Up = FVector::UpVector;
	const int32 NS = 26;
	TArray<FVector> Bot, Top, Mid;
	for (int32 i = 0; i < NS; ++i)
	{
		const float Az = 2.f * PI * (float)i / (float)NS + Rng.FRandRange(-0.08f, 0.08f);
		const FVector Foot = Shape.Point(DirAE(Az, 0.f));
		const float K = 0.7f * Rng.FRandRange(0.82f, 1.08f);
		Bot.Add(FVector(Foot.X * K * 1.04f, Foot.Y * K * 1.04f, Floor - 4.f));
		Top.Add(FVector(Foot.X * K, Foot.Y * K, Floor + Rng.FRandRange(3.5f, 8.5f)));
	}
	for (int32 i = 0; i < NS; ++i)
	{
		const float M = Rng.FRandRange(0.4f, 0.62f);
		Mid.Add(FVector(Top[i].X * M, Top[i].Y * M, Floor + Rng.FRandRange(5.f, 11.f)));
	}
	const FVector Cen(0.f, 0.f, Floor + Rng.FRandRange(6.f, 10.f));
	for (int32 i = 0; i < NS; ++i)
	{
		const int32 I1 = (i + 1) % NS;
		const FVector OutW = FVector(Top[i].X + Top[I1].X, Top[i].Y + Top[I1].Y, 0.f);
		Outer.Flat(Bot[i], Bot[I1], Top[I1], OutW);
		Outer.Flat(Bot[i], Top[I1], Top[i], OutW);
		FMeshBuf& Rim = Rng.FRand() < 0.3f ? Outer : FreshB;
		Rim.Flat(Top[i], Top[I1], Mid[I1], Up);
		Rim.Flat(Top[i], Mid[I1], Mid[i], Up);
		FreshB.Flat(Mid[i], Mid[I1], Cen, Up);
	}
	for (int32 k = 0; k < 9; ++k)
	{
		const float Az = Rng.FRandRange(0.f, 2.f * PI);
		const float D = Radius * Rng.FRandRange(0.55f, 1.2f);
		const FVector Sz(Radius * Rng.FRandRange(0.09f, 0.2f), Radius * Rng.FRandRange(0.07f, 0.15f), FMath::Min(Radius * Rng.FRandRange(0.05f, 0.09f), 10.f));
		TArray<FVector> C;
		MakeShardCorners(Rng, Rng.RandRange(0, 3), Sz, C);
		const FQuat Q = FRotator(Rng.FRandRange(-18.f, 18.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-18.f, 18.f)).Quaternion();
		float Low = 1e9f;
		for (const FVector& P : C) Low = FMath::Min(Low, (float)Q.RotateVector(P).Z);
		const float Base = Floor + (D < Radius * 0.66f ? 6.f : 0.f) - 1.5f; // half-buried in the turf (or on the stump)
		const FVector Pos(FMath::Cos(Az) * D, FMath::Sin(Az) * D, Base - Low);
		const uint32 Mask = (1u << Rng.RandRange(0, 5)) | (Rng.FRand() < 0.5f ? (1u << Rng.RandRange(0, 5)) : 0u);
		AppendShard(C, FTransform(Q, Pos), Mask, Outer, FreshB);
	}
	if (bHasDust)
	{
		const int32 NP = 24;
		const int32 C0 = Powder.Add(FVector(0.f, 0.f, Floor + 1.f), Up, 0.7f);
		TArray<int32> In, Out;
		for (int32 i = 0; i < NP; ++i)
		{
			const float Az = 2.f * PI * (float)i / (float)NP;
			const FVector D(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			In.Add(Powder.Add(D * (Radius * 0.75f * Rng.FRandRange(0.85f, 1.1f)) + FVector(0.f, 0.f, Floor + 0.9f), Up, 0.6f));
			Out.Add(Powder.Add(D * (Radius * 1.45f * Rng.FRandRange(0.8f, 1.15f)) + FVector(0.f, 0.f, Floor + 0.6f), Up, 0.f));
		}
		for (int32 i = 0; i < NP; ++i)
		{
			const int32 I1 = (i + 1) % NP;
			Powder.Tri(C0, In[i], In[I1], Up);
			Powder.Tri(In[i], Out[i], Out[I1], Up);
			Powder.Tri(In[i], Out[I1], In[I1], Up);
		}
	}
	Upload(Rubble, 0, Outer);
	Upload(Rubble, 1, FreshB);
	Upload(Rubble, 2, Powder);
	Rubble->SetMaterial(0, MatCracked);
	Rubble->SetMaterial(1, MatFreshDark);
	if (bHasDust)
	{
		if (UMaterialInterface* DustMat = DustMaterial())
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(DustMat, this);
			MID->SetVectorParameterValue(TEXT("Color"), DustCol * 1.15f);
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.55f);
			MID->SetScalarParameterValue(TEXT("Soft"), 0.f);          // flat stain: no fresnel edge
			MID->SetScalarParameterValue(TEXT("FadeDistance"), 0.05f); // lies on the turf: no depth fade
			Rubble->SetMaterial(2, MID);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------- fracture

FVector AZoogiRock::RockToActor(const FVector& V) const
{
	return FRotator(0.f, Yaw, 0.f).RotateVector(V);
}

FVector AZoogiRock::Surf(const FVector& RockP, float Offset, FVector* OutN) const
{
	const FVector U = Shape.ToU(RockP);
	const FVector N = Shape.Normal(U);
	if (OutN) *OutN = N;
	return Shape.Point(U) + N * Offset;
}

FVector AZoogiRock::ImpactU(const FVector& WorldP, const FVector& Dir) const
{
	FVector L = Body ? Body->GetComponentTransform().InverseTransformPosition(WorldP) : WorldP - GetActorLocation();
	const float D2 = (float)FVector2D(L.X, L.Y).Size();
	if (WorldP.ContainsNaN() || D2 < Radius * 0.3f || D2 > Radius * 2.5f)
	{
		// Unusable contact point: take the side the ball came from, at marble-centre height.
		const FVector DL = Body ? Body->GetComponentTransform().InverseTransformVectorNoScale(Dir) : Dir;
		L = -DL.GetSafeNormal2D() * Radius + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius);
	}
	const FVector U = Shape.ToU(FVector(L.X, L.Y, FMath::Max((float)L.Z, 1.f)));
	const float Az = FMath::Atan2((float)U.Y, (float)U.X);
	const float El = FMath::Clamp(FMath::Asin(FMath::Clamp((float)U.Z, -1.f, 1.f)), FMath::DegreesToRadians(10.f), FMath::DegreesToRadians(48.f));
	return DirAE(Az, El);
}

void AZoogiRock::PrepareSplit(const FVector& U, FRandomStream& Rng)
{
	// The main fracture runs through the impact point, up over the crown and down the far side (a vertical plane through
	// the rock's axis). Half A (SplitAz .. SplitAz + PI) stays; half B tilts away a few degrees about the ground line
	// and slides ~1.5 cm, so the gap is widest at the top. The boundary zigzags (per-row jag) and meanders (drift).
	bSplit = true;
	SplitAz = FMath::Atan2((float)U.Y, (float)U.X);
	const FVector G = FVector(Shape.Semi.X * FMath::Cos(SplitAz), Shape.Semi.Y * FMath::Sin(SplitAz), 0.f).GetSafeNormal();
	SplitN = FVector(-G.Y, G.X, 0.f); // = Up x G: half A is on the +SplitN side
	SplitQ = FQuat(G, FMath::DegreesToRadians(Rng.FRandRange(2.4f, 3.4f))); // +angle about G tilts the top towards -SplitN
	SplitSep = FMath::Clamp(Radius * 0.012f, 1.f, 1.8f);
	JagA.SetNum(BodyRows);
	JagB.SetNum(BodyRows);
	const float Amp = FMath::Clamp(Radius * 0.03f, 2.2f, 3.8f);
	const float ImpEl = FMath::Asin(FMath::Clamp((float)U.Z, -1.f, 1.f));
	const float RowStep = (PI * 0.5f - BodyElMin) / (float)(BodyRows - 1);
	float DriftA = 0.f, DriftB = 0.f, SignA = 1.f, SignB = -1.f;
	for (int32 j = 0; j < BodyRows; ++j)
	{
		const float El = RowEl(j);
		DriftA = FMath::Clamp(DriftA + Rng.FRandRange(-0.9f, 0.9f), -4.f, 4.f);
		DriftB = FMath::Clamp(DriftB + Rng.FRandRange(-0.9f, 0.9f), -4.f, 4.f);
		if (Rng.FRand() > 0.2f) SignA = -SignA;
		if (Rng.FRand() > 0.2f) SignB = -SignB;
		float SA = SignA * Amp * Rng.FRandRange(0.35f, 1.f) + DriftA;
		const float SB = SignB * Amp * Rng.FRandRange(0.35f, 1.f) + DriftB;
		if (FMath::Abs(El - ImpEl) < RowStep * 0.75f) SA *= 0.15f; // pass (almost) through the impact point
		const float Fade = 1.f - FMath::SmoothStep(FMath::DegreesToRadians(70.f), FMath::DegreesToRadians(88.f), El); // meet at the crown
		const float Rr = Radius * FMath::Max(FMath::Cos(El), 0.25f);
		JagA[j] = SA * Fade / Rr;
		JagB[j] = SB * Fade / Rr;
	}
}

int32 AZoogiRock::SideOf(const FVector& U) const
{
	if (!bSplit || JagA.Num() != BodyRows || JagB.Num() != BodyRows) return 0;
	const float El = FMath::Asin(FMath::Clamp((float)U.Z, -1.f, 1.f));
	const float F = FMath::Clamp((El - BodyElMin) / (PI * 0.5f - BodyElMin), 0.f, 1.f) * (float)(BodyRows - 1);
	const int32 J0 = FMath::Clamp((int32)F, 0, BodyRows - 2);
	const float T = F - (float)J0;
	const float JA = FMath::Lerp(JagA[J0], JagA[J0 + 1], T);
	const float JB = FMath::Lerp(JagB[J0], JagB[J0 + 1], T);
	const float Phi = FMath::Atan2((float)U.Y, (float)U.X);
	const float A = SplitAz + JA, B = SplitAz + PI + JB;
	return Wrap2Pi(Phi - A) < Wrap2Pi(B - A) ? 0 : 1;
}

FVector AZoogiRock::SplitPos(const FVector& P, int32 InSide) const
{
	return (bSplit && InSide == 1) ? SplitQ.RotateVector(P) - SplitN * SplitSep : P;
}

FVector AZoogiRock::SplitNrm(const FVector& N, int32 InSide) const
{
	return (bSplit && InSide == 1) ? SplitQ.RotateVector(N) : N;
}

void AZoogiRock::BuildBody()
{
	if (!Body) return;
	FMeshBuf Outer, Faces;
	if (!bSplit)
	{
		BuildShell(Shape, BodyCols, BodyRows, Outer);
	}
	else
	{
		const int32 HC = BodyCols / 2;
		auto PhiA = [this](int32 j) { return SplitAz + JagA[j]; };
		auto PhiB = [this](int32 j) { return SplitAz + PI + JagB[j]; };
		for (int32 s = 0; s < 2; ++s)
		{
			// Half s spans [start, end] per row; boundary vertices are shared positions (duplicated per half).
			const int32 Base = Outer.V.Num();
			for (int32 j = 0; j < BodyRows; ++j)
			{
				const float El = RowEl(j);
				const float P0 = s == 0 ? PhiA(j) : PhiB(j);
				const float P1 = s == 0 ? PhiB(j) : PhiA(j) + 2.f * PI;
				for (int32 i = 0; i <= HC; ++i)
				{
					const FVector U = DirAE(FMath::Lerp(P0, P1, (float)i / (float)HC), El);
					Outer.Add(SplitPos(Shape.Point(U), s), SplitNrm(Shape.Normal(U), s));
				}
			}
			GridTris(Outer, Base, HC, BodyRows);

			// Fracture face of this half (seen through the gap): fan from inside the rock to the jagged boundary, with a
			// roughened middle ring, flat-shaded.
			TArray<FVector> Ring;
			for (int32 j = 0; j < BodyRows; ++j) Ring.Add(Shape.Point(DirAE(s == 0 ? PhiA(j) : PhiB(j), RowEl(j))));
			for (int32 j = BodyRows - 2; j >= 0; --j) Ring.Add(Shape.Point(DirAE(s == 0 ? PhiB(j) : PhiA(j), RowEl(j))));
			FRandomStream FR(Seed * 131 + 7 + s);
			const FVector Cen(0.f, 0.f, FMath::Max(Height * 0.2f, 8.f));
			TArray<FVector> MidR;
			for (const FVector& R : Ring) MidR.Add(FMath::Lerp(Cen, R, 0.62f) + SplitN * FR.FRandRange(-1.2f, 1.2f));
			const FVector Out = SplitNrm(s == 0 ? -SplitN : SplitN, s);
			const FVector CenS = SplitPos(Cen, s);
			for (int32 k = 0; k < Ring.Num(); ++k)
			{
				const int32 K1 = (k + 1) % Ring.Num();
				const FVector M0 = SplitPos(MidR[k], s), M1 = SplitPos(MidR[K1], s), R0 = SplitPos(Ring[k], s), R1 = SplitPos(Ring[K1], s);
				Faces.Flat(CenS, M0, M1, Out);
				Faces.Flat(M0, R0, R1, Out);
				Faces.Flat(M0, R1, M1, Out);
			}
		}
	}
	Upload(Body, 0, Outer);
	Upload(Body, 1, Faces);
	Body->SetMaterial(0, bSplit ? MatCracked.Get() : MatIntact.Get());
	Body->SetMaterial(1, MatFracture);
}

void AZoogiRock::BuildCracks(const FVector& U0, FRandomStream& Rng)
{
	FMeshBuf Gap, Edge, Scar;
	const float FloorLocal = -(float)GetActorLocation().Z + 2.f;
	FVector N0;
	const FVector P0 = Surf(Shape.Point(U0), 0.f, &N0); // on the dented surface
	const float Rs = ScarRadius(Radius);
	FVector T1 = FVector::CrossProduct(N0, FVector::UpVector).GetSafeNormal();
	if (T1.IsNearlyZero()) T1 = FVector::ForwardVector;
	const FVector T2 = FVector::CrossProduct(N0, T1);

	// 1) Fresh-stone scar where the flake came off: jagged two-ring patch in the dent, normals leaning in (concave).
	//    Each vertex takes its half's offset; triangles straddling the fracture are dropped so the split shows through.
	{
		const int32 K = 12;
		int32 CenterIdx[2];
		for (int32 s = 0; s < 2; ++s) CenterIdx[s] = Scar.Add(SplitPos(P0 + N0 * 0.45f, s), SplitNrm(N0, s));
		TArray<int32> InI, RimI, InS, RimS;
		for (int32 k = 0; k < K; ++k)
		{
			const float A = 2.f * PI * (float)k / (float)K + Rng.FRandRange(-0.15f, 0.15f);
			const float Rr = Rs * Rng.FRandRange(0.7f, 1.1f) * ((k & 1) ? 1.f : 0.78f);
			const FVector Dk = T1 * FMath::Cos(A) + T2 * FMath::Sin(A);
			FVector Ni, Nr;
			const FVector Pi = Surf(P0 + Dk * (Rr * 0.5f), 0.45f, &Ni);
			const FVector Pr = Surf(P0 + Dk * Rr, 0.45f, &Nr);
			const int32 Si = SideOf(Shape.ToU(Pi)), Sr = SideOf(Shape.ToU(Pr));
			InS.Add(Si);
			RimS.Add(Sr);
			InI.Add(Scar.Add(SplitPos(Pi, Si), SplitNrm((Ni - Dk * 0.25f).GetSafeNormal(), Si)));
			RimI.Add(Scar.Add(SplitPos(Pr, Sr), SplitNrm((Nr - Dk * 0.55f).GetSafeNormal(), Sr)));
		}
		for (int32 k = 0; k < K; ++k)
		{
			const int32 K1 = (k + 1) % K;
			if (InS[k] == InS[K1]) Scar.Tri(CenterIdx[InS[k]], InI[k], InI[K1], SplitNrm(N0, InS[k]));
			if (InS[k] == InS[K1] && RimS[k] == RimS[K1] && InS[k] == RimS[k])
			{
				const FVector Out = SplitNrm(N0, InS[k]);
				Scar.Tri(InI[k], RimI[k], RimI[K1], Out);
				Scar.Tri(InI[k], RimI[K1], InI[K1], Out);
			}
		}
	}

	// 2) Main fracture rims: on both halves, a dark lip right at the break and a lighter chipped band behind it, wider
	//    near the impact.
	const float ImpEl = FMath::Asin(FMath::Clamp((float)U0.Z, -1.f, 1.f));
	for (int32 s = 0; s < 2; ++s)
	{
		for (int32 m = 0; m < 2; ++m)
		{
			const bool bOnA = (s == 0) == (m == 0); // this rim follows boundary meridian A (the impact one)
			const float InSign = m == 0 ? 1.f : -1.f; // towards the inside of half s
			const int32 GBase = Gap.V.Num(), EBase = Edge.V.Num();
			int32 Rows = 0;
			for (int32 j = 0; j < BodyRows; ++j)
			{
				const float El = RowEl(j);
				if (El < FMath::DegreesToRadians(-3.f) || El > FMath::DegreesToRadians(84.f)) continue;
				const float PhiBd = bOnA ? SplitAz + JagA[j] : SplitAz + PI + JagB[j];
				const float CosEl = FMath::Max(FMath::Cos(El), 0.2f);
				const float Near = bOnA ? 1.f - FMath::Clamp(FMath::Abs(El - ImpEl) / 0.9f, 0.f, 1.f) : 0.f;
				const float Lip = FMath::Clamp(Radius * 0.012f, 0.9f, 1.6f) * (1.f + Near) * Rng.FRandRange(0.7f, 1.3f);
				const float EdgeW = FMath::Clamp(Radius * 0.02f, 1.4f, 2.6f) * (1.f + 1.2f * Near) * Rng.FRandRange(0.6f, 1.4f);
				auto At = [&](float D) { return DirAE(PhiBd + InSign * D / (Radius * CosEl), El); };
				const FVector Ua = At(0.f), Ub = At(Lip), Uc = At(Lip + EdgeW);
				const FVector Pa = Shape.Point(Ua), Pb = Shape.Point(Ub), Pc = Shape.Point(Uc);
				const FVector Na = Shape.Normal(Ua), Nb = Shape.Normal(Ub), Nc = Shape.Normal(Uc);
				const FVector In = (Pc - Pa).GetSafeNormal();
				Gap.Add(SplitPos(Pa + Na * 0.9f, s), SplitNrm((Na - In * 0.9f).GetSafeNormal(), s));
				Gap.Add(SplitPos(Pb + Nb * 1.f, s), SplitNrm((Nb - In * 0.5f).GetSafeNormal(), s));
				Edge.Add(SplitPos(Pb + Nb * 1.f, s), SplitNrm((Nb - In * 0.35f).GetSafeNormal(), s));
				Edge.Add(SplitPos(Pc + Nc * 0.7f, s), SplitNrm(Nc, s));
				++Rows;
			}
			for (int32 r = 0; r + 1 < Rows; ++r)
			{
				const FVector OutG = Gap.N[GBase + r * 2] + Gap.N[GBase + r * 2 + 1];
				Gap.Tri(GBase + r * 2, GBase + r * 2 + 1, GBase + r * 2 + 3, OutG);
				Gap.Tri(GBase + r * 2, GBase + r * 2 + 3, GBase + r * 2 + 2, OutG);
				const FVector OutE = Edge.N[EBase + r * 2] + Edge.N[EBase + r * 2 + 1];
				Edge.Tri(EBase + r * 2, EBase + r * 2 + 1, EBase + r * 2 + 3, OutE);
				Edge.Tri(EBase + r * 2, EBase + r * 2 + 3, EBase + r * 2 + 2, OutE);
			}
		}
	}

	// 3) Branching surface cracks from the impact: zigzag walks over the surface, thick near the impact and tapering,
	//    forking up to two levels deep, stopping at the turf or at the main fracture (cracks end at an existing break).
	auto Ribbon = [&](const TArray<FVector>& Pts, const TArray<FVector>& Nrm, const TArray<float>& Ws, int32 Sd)
	{
		const int32 Num = Pts.Num();
		const int32 G0 = Gap.V.Num(), E0 = Edge.V.Num();
		for (int32 k = 0; k < Num; ++k)
		{
			const FVector N = Nrm[k];
			const FVector Tg = (Pts[FMath::Min(k + 1, Num - 1)] - Pts[FMath::Max(k - 1, 0)]).GetSafeNormal();
			FVector B = FVector::CrossProduct(N, Tg).GetSafeNormal();
			if (B.IsNearlyZero()) B = T1;
			const float W = Ws[k] * 0.5f;
			const float Tip = Ws[k] > 0.f ? 1.f : 0.3f;
			const float EL = (Ws[k] * 0.45f + 0.7f) * Rng.FRandRange(0.55f, 1.5f) * Tip; // chipped, uneven edge
			const float ER = (Ws[k] * 0.45f + 0.7f) * Rng.FRandRange(0.55f, 1.5f) * Tip;
			FVector NL1, NR1, NL2, NR2;
			const FVector L1 = Surf(Pts[k] - B * W, 1.0f, &NL1);
			const FVector R1 = Surf(Pts[k] + B * W, 1.0f, &NR1);
			const FVector L2 = Surf(Pts[k] - B * (W + EL), 0.7f, &NL2);
			const FVector R2 = Surf(Pts[k] + B * (W + ER), 0.7f, &NR2);
			// Dark gap (walls lean in towards the centre line) ...
			Gap.Add(SplitPos(L1, Sd), SplitNrm((NL1 + B * 0.8f).GetSafeNormal(), Sd));
			Gap.Add(SplitPos(Pts[k] + N * 1.1f, Sd), SplitNrm(N, Sd));
			Gap.Add(SplitPos(R1, Sd), SplitNrm((NR1 - B * 0.8f).GetSafeNormal(), Sd));
			// ... and lighter chipped bevels either side (outer edge blends into the surface).
			Edge.Add(SplitPos(L2, Sd), SplitNrm(NL2, Sd));
			Edge.Add(SplitPos(L1, Sd), SplitNrm((NL1 + B * 0.45f).GetSafeNormal(), Sd));
			Edge.Add(SplitPos(R1, Sd), SplitNrm((NR1 - B * 0.45f).GetSafeNormal(), Sd));
			Edge.Add(SplitPos(R2, Sd), SplitNrm(NR2, Sd));
		}
		for (int32 k = 0; k + 1 < Num; ++k)
		{
			const FVector Out = SplitNrm(Nrm[k], Sd);
			const int32 g = G0 + k * 3, g2 = g + 3;
			Gap.Tri(g, g + 1, g2 + 1, Out); Gap.Tri(g, g2 + 1, g2, Out);
			Gap.Tri(g + 1, g + 2, g2 + 2, Out); Gap.Tri(g + 1, g2 + 2, g2 + 1, Out);
			const int32 e = E0 + k * 4, e2 = e + 4;
			Edge.Tri(e, e + 1, e2 + 1, Out); Edge.Tri(e, e2 + 1, e2, Out);
			Edge.Tri(e + 2, e + 3, e2 + 3, Out); Edge.Tri(e + 2, e2 + 3, e2 + 2, Out);
		}
	};

	struct FSeedB { FVector P; FVector H; float W0; float Len; int32 Depth; int32 Side; };
	TArray<FSeedB> Queue;
	const float Wm = FMath::Clamp(Radius * 0.05f, 4.f, 6.5f);
	const float Step = FMath::Clamp(Radius * 0.065f, 5.f, 8.5f);
	FVector Tu = (FVector::UpVector - N0 * FVector::DotProduct(FVector::UpVector, N0)).GetSafeNormal();
	if (Tu.IsNearlyZero()) Tu = T2;
	static const float Angles[5] = { 38.f, -42.f, 102.f, -112.f, 176.f }; // relative to "up the rock"; the fracture itself runs straight up
	const int32 NumPrim = 4 + (Rng.FRand() < 0.5f ? 1 : 0);
	for (int32 k = 0; k < NumPrim; ++k)
	{
		const float A = Angles[k] + Rng.FRandRange(-12.f, 12.f);
		const float Len = Radius * (k == 4 ? Rng.FRandRange(0.22f, 0.32f) : Rng.FRandRange(0.5f, 0.85f));
		Queue.Add(FSeedB{ P0, Tu.RotateAngleAxis(A, N0), float(Wm * Rng.FRandRange(0.7f, 1.f)), Len, 0, -1 });
	}
	int32 Budget = 260;
	for (int32 q = 0; q < Queue.Num() && Budget > 0; ++q)
	{
		const FSeedB S = Queue[q]; // copy: forks append to Queue
		TArray<FVector> Pts, Nrm;
		TArray<float> Ws;
		FVector N;
		FVector P = Surf(S.P, 0.f, &N);
		FVector H = (S.H - N * FVector::DotProduct(S.H, N)).GetSafeNormal();
		if (H.IsNearlyZero()) continue;
		int32 Sd = S.Side;
		float Zig = Rng.FRand() < 0.5f ? 1.f : -1.f;
		float Done = 0.f;
		Pts.Add(P); Nrm.Add(N); Ws.Add(S.W0);
		while (Done < S.Len && Pts.Num() < 40 && Budget > 0)
		{
			const float St = Step * Rng.FRandRange(0.6f, 1.35f) * (S.Depth > 0 ? 0.85f : 1.f);
			const FVector Hs = H.RotateAngleAxis(Zig * Rng.FRandRange(14.f, 38.f), N);
			if (Rng.FRand() < 0.8f) Zig = -Zig; // mostly alternating, sometimes two kinks the same way
			FVector N2;
			const FVector P2 = Surf(P + Hs * St, 0.f, &N2);
			if (P2.Z < FloorLocal) break;
			const int32 S2 = SideOf(Shape.ToU(P2));
			if (Sd < 0) Sd = S2;
			else if (S2 != Sd) break;
			Done += St;
			const float W = S.W0 * FMath::Pow(FMath::Max(0.f, 1.f - Done / S.Len), 0.85f);
			H = H.RotateAngleAxis(Rng.FRandRange(-9.f, 9.f), N2);
			H = (H - N2 * FVector::DotProduct(H, N2)).GetSafeNormal();
			if (H.IsNearlyZero()) break;
			Pts.Add(P2); Nrm.Add(N2); Ws.Add(W);
			--Budget;
			if (S.Depth < 2 && Pts.Num() > 2 && Done < S.Len * 0.8f && Rng.FRand() < 0.22f)
			{
				const float Fa = (Rng.FRand() < 0.5f ? 1.f : -1.f) * Rng.FRandRange(25.f, 55.f);
				Queue.Add(FSeedB{ P2, H.RotateAngleAxis(Fa, N2), float(W * 0.7f), float((S.Len - Done) * Rng.FRandRange(0.45f, 0.8f) + St), S.Depth + 1, Sd });
			}
			P = P2; N = N2;
		}
		if (Pts.Num() < 2 || Sd < 0) continue;
		Ws.Last() = 0.f;
		if (Ws.Num() > 2) Ws[Ws.Num() - 2] *= 0.6f;
		Ribbon(Pts, Nrm, Ws, Sd);
	}

	if (CrackMesh)
	{
		Upload(CrackMesh, 0, Gap);
		Upload(CrackMesh, 1, Edge);
		Upload(CrackMesh, 2, Scar);
	}
}

// ---------------------------------------------------------------------------------------------------------------- state

void AZoogiRock::HideAllDebris()
{
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		Chunks[i].bLive = false;
		Chunks[i].bRest = false;
		Chunks[i].bSinking = false;
		if (ChunkPool.IsValidIndex(i) && ChunkPool[i]) ChunkPool[i]->SetVisibility(false);
	}
	for (FGrit& G : GritParts) { G.bLive = false; G.bRest = false; }
	for (UInstancedStaticMeshComponent* ISM : GritISM)
	{
		if (!ISM) continue;
		for (int32 k = 0; k < GritPerSet; ++k) ISM->UpdateInstanceTransform(k, HiddenXf(), false, false, true);
		ISM->MarkRenderStateDirty();
		ISM->SetVisibility(false);
	}
	for (int32 i = 0; i < PuffState.Num(); ++i)
	{
		PuffState[i].bLive = false;
		if (Puffs.IsValidIndex(i) && Puffs[i]) Puffs[i]->SetVisibility(false);
	}
}

void AZoogiRock::ResetRock()
{
	State = EZoogiRockState::Intact;
	LastHitTime = -10.f;
	if (Collider) Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Shape.DentD = 0.f;
	Shape.DentR = 0.f;
	if (bSplit || !bBodyBuiltIntact)
	{
		bSplit = false;
		BuildBody();
		bBodyBuiltIntact = Body != nullptr;
	}
	if (Body) Body->SetVisibility(true);
	if (Side) Side->SetVisibility(true);
	if (CrackMesh) CrackMesh->SetVisibility(false);
	if (Rubble) Rubble->SetVisibility(false);
	HideAllDebris();
	SetActorTickEnabled(false);
}

void AZoogiRock::OnBallHit(AZoogiBall* Ball, const FVector& NormalImpulse, const FVector& ImpactPoint)
{
	if (!Ball || State == EZoogiRockState::Shattered) return;
	UWorld* W = GetWorld();
	const float Now = W ? W->GetTimeSeconds() : 0.f;
	if (Now - LastHitTime < 0.3f) return; // one contact reports for several frames
	// Contact speed: the ball's approach velocity (sampled before this frame's physics step) along the contact normal.
	// (The reported Chaos impulse proved unreliable as a speed measure: it read ~3x the launch speed.)
	FVector N2 = (GetActorLocation() - Ball->BallLocation()).GetSafeNormal2D();
	if (N2.IsNearlyZero()) N2 = -NormalImpulse.GetSafeNormal2D();
	const FVector V = Ball->PrevVelocity.SizeSquared() > 1.f ? Ball->PrevVelocity : Ball->GetBallVelocity();
	const float Vn = FMath::Max(0.f, FVector::DotProduct(FVector(V.X, V.Y, 0.f), N2));
	const float CutDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Vn / FMath::Max(1.f, V.Size2D()), -1.f, 1.f)));
	if (Vn < CrackSpeed) return;
	LastHitTime = Now;
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	// Strong hit: the active shooter while Awakened (infused), or crit-grade contact (>= 90% of its launch speed, the crit rule).
	bool bStrong = false;
	if (G && Ball->PlayerIndex == G->ActivePlayer && G->Phase == EZoogiPhase::Rolling)
	{
		const bool bAwakened = Ball->Artifact && Ball->Artifact->Infused != EZoogiArtifact::None;
		// Crit-grade = the crit rule (>= 90% of its launch speed at contact, <= 5 deg cut) on a hard hit; a soft straight tap only cracks.
		const bool bCritGrade = G->GetShotSpeed() > 1.f && V.Size2D() >= 0.9f * G->GetShotSpeed() && CutDeg <= 5.f && Vn >= ShatterSpeed;
		bStrong = bAwakened || bCritGrade;
	}
	FVector Dir = -NormalImpulse.GetSafeNormal2D(); // impulse pushes the ball away from the rock: debris flies the other way
	if (Dir.IsNearlyZero()) Dir = (GetActorLocation() - Ball->BallLocation()).GetSafeNormal2D();
	UE_LOG(LogZoogi, Log, TEXT("Rock %s hit by P%d %s at %.0f cm/s normal (%.0f cm/s, cut %.0f deg)%s (state %d)"), *GetName(), Ball->PlayerIndex + 1, *Ball->Def.Name, Vn, V.Size2D(), CutDeg, bStrong ? TEXT(" STRONG") : TEXT(""), (int32)State);
	if (bStrong || (State == EZoogiRockState::Cracked && Vn >= ShatterSpeed)) Shatter(ImpactPoint, Dir, Ball);
	else if (State == EZoogiRockState::Intact) Crack(ImpactPoint, Dir, Ball);
}

void AZoogiRock::Crack(const FVector& P, const FVector& Dir, AZoogiBall* By)
{
	State = EZoogiRockState::Cracked;
	FRandomStream Rng(Seed * 7919 + 17);

	// Geometry: dent where the flake pops off, split the boulder along a jagged fracture through the impact point,
	// then lay the scar, fracture rims and branching cracks on the (dented, split) surface.
	const FVector U = ImpactU(P, Dir);
	FVector N0;
	const FVector P0 = Surf(Shape.Point(U), 0.f, &N0);
	const float Rs = ScarRadius(Radius);
	Shape.DentC = P0;
	Shape.DentN = N0;
	Shape.DentR = Rs * 1.05f;
	Shape.DentD = FMath::Clamp(Radius * 0.022f, 1.6f, 2.8f);
	PrepareSplit(U, Rng);
	BuildBody();
	bBodyBuiltIntact = false;
	BuildCracks(U, Rng);
	if (CrackMesh) CrackMesh->SetVisibility(true);

	// Debris off the impact face (actor space): the flake, a few pebbles, stone grit and a slow grey-brown dust puff.
	const FVector PA = RockToActor(P0), NA = RockToActor(N0);
	FVector TA = FVector::CrossProduct(NA, FVector::UpVector).GetSafeNormal();
	if (TA.IsNearlyZero()) TA = FVector::RightVector;
	LaunchChunk(0, PA + NA * 3.f, NA * Rng.FRandRange(200.f, 300.f) + TA * Rng.FRandRange(-60.f, 60.f) + FVector(0.f, 0.f, Rng.FRandRange(120.f, 210.f)),
		Rng.VRand() * Rng.FRandRange(9.f, 16.f), true);
	for (int32 i = 1; i < NumCrackPieces; ++i)
	{
		LaunchChunk(i, PA + NA * 2.f + TA * (Rng.FRandRange(-0.5f, 0.5f) * Rs) + FVector(0.f, 0.f, Rng.FRandRange(-0.3f, 0.3f) * Rs),
			NA * Rng.FRandRange(80.f, 210.f) + TA * Rng.FRandRange(-90.f, 90.f) + FVector(0.f, 0.f, Rng.FRandRange(30.f, 150.f)),
			Rng.VRand() * Rng.FRandRange(8.f, 20.f), true);
	}
	SpawnGrit(0, CrackGritPerSet, PA + NA * 2.f, Rs * 0.5f, (NA + FVector(0.f, 0.f, 0.35f)).GetSafeNormal(), 50.f, 180.f, 560.f, Rng);
	if (bHasDust)
	{
		const float K = FMath::Clamp(Radius / 110.f, 0.7f, 1.3f);
		for (int32 k = 0; k < 4; ++k)
		{
			SpawnPuff(PA + NA * Rng.FRandRange(6.f, 16.f) + TA * (Rng.FRandRange(-0.6f, 0.6f) * Rs) + FVector(0.f, 0.f, Rng.FRandRange(-4.f, 8.f)),
				NA * Rng.FRandRange(60.f, 140.f) + TA * Rng.FRandRange(-40.f, 40.f) + FVector(0.f, 0.f, Rng.FRandRange(10.f, 45.f)),
				Rng.FRandRange(6.f, 11.f), Rng.FRandRange(26.f, 48.f) * K, Rng.FRandRange(2.f, 2.8f), Rng.FRandRange(0.3f, 0.46f), 2.4f, 9.f, 1.f,
				DustCol * Rng.FRandRange(0.92f, 1.08f));
		}
	}
	else
	{
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), GetActorTransform().TransformPosition(PA), FRotator::ZeroRotator, 0.35f);
	}
	// Hold the shooter's bright speed sparks for a moment so the stone grit and dust read (visual only).
	if (By) By->SparkClock = -0.6f;

	ZoogiUtil::PlaySfx(this, TEXT("RockCrack"), 0.9f, FMath::FRandRange(0.95f, 1.05f));
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		G->OnRockEvent(this, false, By, GetActorLocation() + FVector(0, 0, Height + 90.f));
	}
	UE_LOG(LogZoogi, Log, TEXT("Rock %s CRACKED"), *GetName());
}

void AZoogiRock::Shatter(const FVector& P, const FVector& Dir, AZoogiBall* By)
{
	State = EZoogiRockState::Shattered;
	Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the lane opens; nothing left to trap a Zoogi
	if (Body) Body->SetVisibility(false);
	if (Side) Side->SetVisibility(false);
	if (CrackMesh) CrackMesh->SetVisibility(false);
	if (Rubble) Rubble->SetVisibility(true);
	FRandomStream Rng(Seed * 7919 + 29);
	FVector DirA = GetActorTransform().InverseTransformVectorNoScale(Dir).GetSafeNormal2D();
	if (DirA.IsNearlyZero()) DirA = FVector::ForwardVector;

	// Angular shards of mixed size burst from inside the boulder: outwards, along the hit and up; big ones slower.
	const int32 NumShards = Chunks.Num() - NumCrackPieces;
	for (int32 k = 0; k < NumShards; ++k)
	{
		const int32 i = NumCrackPieces + k;
		const float Az = 2.f * PI * (float)k / (float)FMath::Max(1, NumShards) + Rng.FRandRange(-0.35f, 0.35f);
		const float El = FMath::DegreesToRadians(Rng.FRandRange(12.f, 62.f));
		const FVector Pos = RockToActor(Shape.Point(DirAE(Az, El)) * Rng.FRandRange(0.4f, 0.72f));
		const FVector Rad = FVector(Pos.X, Pos.Y, 0.f).GetSafeNormal();
		const float Kv = FMath::Clamp(1.f / FMath::Sqrt(FMath::Max(Chunks[i].Size / (Radius * 0.3f), 0.1f)), 0.8f, 1.25f);
		const FVector Vel = (Rad * Rng.FRandRange(90.f, 220.f) + DirA * Rng.FRandRange(120.f, 330.f)) * Kv + FVector(0.f, 0.f, Rng.FRandRange(170.f, 380.f) * Kv);
		LaunchChunk(i, Pos, Vel, Rng.VRand() * (Rng.FRandRange(4.f, 12.f) * Kv), Chunks[i].Size <= Radius * 0.17f);
	}
	SpawnGrit(CrackGritPerSet, GritPerSet - CrackGritPerSet, RockToActor(FVector(0.f, 0.f, Height * 0.45f)), Radius * 0.45f,
		(DirA * 0.6f + FVector::UpVector).GetSafeNormal(), 70.f, 220.f, 700.f, Rng);

	if (bHasDust)
	{
		// Heavy cloud: six burst puffs that billow out, hang and sink, plus four low puffs rolling along the turf.
		for (int32 k = 0; k < 6; ++k)
		{
			const float Az = 2.f * PI * (float)k / 6.f + Rng.FRandRange(-0.4f, 0.4f);
			const FVector Rad(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			SpawnPuff(Rad * (Radius * Rng.FRandRange(0.15f, 0.45f)) + FVector(0.f, 0.f, Height * Rng.FRandRange(0.3f, 0.7f)),
				Rad * Rng.FRandRange(110.f, 240.f) + DirA * Rng.FRandRange(40.f, 120.f) + FVector(0.f, 0.f, Rng.FRandRange(50.f, 150.f)),
				Radius * Rng.FRandRange(0.25f, 0.4f), Radius * Rng.FRandRange(0.9f, 1.4f), Rng.FRandRange(3.4f, 4.4f), Rng.FRandRange(0.48f, 0.62f), 1.7f, 13.f, 1.f,
				DustCol * Rng.FRandRange(0.9f, 1.1f));
		}
		for (int32 k = 0; k < 4; ++k)
		{
			const float Az = 2.f * PI * (float)k / 4.f + PI * 0.25f + Rng.FRandRange(-0.3f, 0.3f);
			const FVector Rad(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			SpawnPuff(Rad * (Radius * 0.6f) + FVector(0.f, 0.f, Radius * 0.12f), Rad * Rng.FRandRange(150.f, 260.f) + DirA * Rng.FRandRange(30.f, 90.f),
				Radius * 0.35f, Radius * Rng.FRandRange(1.f, 1.5f), Rng.FRandRange(3.6f, 4.6f), Rng.FRandRange(0.32f, 0.44f), 1.5f, 4.f, 0.55f,
				DustCol * Rng.FRandRange(0.82f, 0.95f));
		}
	}
	else
	{
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), GetActorLocation() + FVector(0, 0, 10.f), FRotator::ZeroRotator, FMath::Clamp(Radius / 100.f, 0.8f, 1.6f));
	}
	if (By) By->SparkClock = -0.6f; // visual only, see Crack()

	ZoogiUtil::PlaySfx(this, TEXT("RockShatter"), 1.f, FMath::FRandRange(0.95f, 1.05f));
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		G->OnRockEvent(this, true, By, GetActorLocation() + FVector(0, 0, Height + 110.f));
	}
	// Small emphasis only: the tactical camera gets a light kick, no hit-stop.
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(0.35f);
	UE_LOG(LogZoogi, Log, TEXT("Rock %s SHATTERED (%d chunks)"), *GetName(), ChunkCount);
}

// ---------------------------------------------------------------------------------------------------------------- debris

void AZoogiRock::LaunchChunk(int32 Index, const FVector& Pos, const FVector& Vel, const FVector& AngVel, bool bMayStay)
{
	if (!Chunks.IsValidIndex(Index) || !ChunkPool.IsValidIndex(Index) || !ChunkPool[Index]) return;
	FRandomStream Rng(Seed * 31 + Index * 7 + (int32)State);
	FChunk& C = Chunks[Index];
	C.Pos = Pos;
	C.Vel = Vel;
	C.W = AngVel;
	C.Rot = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f)).Quaternion();
	C.Age = 0.f; C.RestAge = 0.f; C.Sink = 0.f;
	C.bLive = true; C.bRest = false; C.bMayStay = bMayStay; C.bSinking = false; C.bGroundChecked = false; C.bNoGround = false;
	ChunkPool[Index]->SetRelativeLocationAndRotation(C.Pos, C.Rot);
	ChunkPool[Index]->SetVisibility(true);
	SetActorTickEnabled(true);
}

void AZoogiRock::SpawnGrit(int32 First, int32 Count, const FVector& Origin, float Spread, const FVector& MainDir, float Cone, float SpeedMin, float SpeedMax, FRandomStream& Rng)
{
	const float K = FMath::Clamp(Radius / 110.f, 0.8f, 1.2f);
	for (int32 s = 0; s < GritISM.Num() && s < 2; ++s)
	{
		UInstancedStaticMeshComponent* ISM = GritISM[s];
		if (!ISM) continue;
		for (int32 k = First; k < First + Count && k < GritPerSet; ++k)
		{
			FGrit& G = GritParts[s * GritPerSet + k];
			G.Pos = Origin + Rng.VRand() * Rng.FRandRange(0.f, Spread);
			G.Vel = Rng.VRandCone(MainDir, FMath::DegreesToRadians(Cone)) * Rng.FRandRange(SpeedMin, SpeedMax);
			const float Sz = Rng.FRandRange(0.8f, 2.f) * K; // half extent, cm (gravel-sized chips)
			G.Half = FVector(Sz * Rng.FRandRange(0.7f, 1.3f), Sz * Rng.FRandRange(0.6f, 1.1f), Sz * Rng.FRandRange(0.4f, 0.9f));
			G.Rot = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f)).Quaternion();
			G.W = Rng.VRand() * Rng.FRandRange(10.f, 30.f);
			G.Age = 0.f;
			G.bLive = true; G.bRest = false; G.bGroundChecked = false; G.bNoGround = false;
			ISM->UpdateInstanceTransform(k, FTransform(G.Rot, G.Pos, G.Half / 50.f), false, false, true);
		}
		ISM->MarkRenderStateDirty();
		ISM->SetVisibility(true);
	}
	SetActorTickEnabled(true);
}

void AZoogiRock::SpawnPuff(const FVector& Pos, const FVector& Vel, float R0, float R1, float Life, float Peak, float Drag, float Settle, float Flat, const FLinearColor& Col)
{
	if (!bHasDust || Puffs.Num() == 0 || PuffState.Num() != Puffs.Num()) return;
	int32 Idx = -1;
	for (int32 n = 0; n < Puffs.Num(); ++n)
	{
		const int32 c = (NextPuff + n) % Puffs.Num();
		if (!PuffState[c].bLive) { Idx = c; break; }
	}
	if (Idx < 0) Idx = NextPuff % Puffs.Num(); // all busy: recycle round-robin
	NextPuff = Idx + 1;
	FPuff& F = PuffState[Idx];
	F.Pos = Pos; F.Vel = Vel; F.R0 = R0; F.R1 = R1; F.Age = 0.f; F.Life = FMath::Max(Life, 0.2f);
	F.Peak = Peak; F.Drag = Drag; F.Settle = Settle; F.Flat = Flat; F.bLive = true;
	if (PuffMIDs.IsValidIndex(Idx) && PuffMIDs[Idx])
	{
		PuffMIDs[Idx]->SetVectorParameterValue(TEXT("Color"), Col);
		PuffMIDs[Idx]->SetScalarParameterValue(TEXT("Opacity"), 0.f);
	}
	if (Puffs[Idx])
	{
		Puffs[Idx]->SetRelativeLocation(Pos);
		Puffs[Idx]->SetRelativeScale3D(FVector(R0, R0, R0 * Flat) / 50.f);
		Puffs[Idx]->SetVisibility(true);
	}
	SetActorTickEnabled(true);
}

bool AZoogiRock::HasGround(const FVector& LocalPos) const
{
	// One trace per piece at its first touchdown: past the island edge there is no turf, so it keeps falling.
	UWorld* W = GetWorld();
	if (!W) return true;
	const FVector P = GetActorTransform().TransformPosition(FVector(LocalPos.X, LocalPos.Y, -GetActorLocation().Z));
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiRockDebris), false, this);
	return W->LineTraceTestByObjectType(P + FVector(0.f, 0.f, 40.f), P - FVector(0.f, 0.f, 40.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
}

bool AZoogiRock::TickChunks(float Dt, float FloorZ)
{
	bool bAny = false;
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		FChunk& C = Chunks[i];
		if (!C.bLive) continue;
		UProceduralMeshComponent* M = ChunkPool.IsValidIndex(i) ? ChunkPool[i].Get() : nullptr;
		if (!M) { C.bLive = false; continue; }
		if (C.bRest && !C.bSinking)
		{
			if (C.bMayStay) continue; // settled small pieces stay as part of the rubble until ResetRock
			C.RestAge += Dt;
			C.Age += Dt;
			bAny = true;
			if (C.RestAge > 0.5f || C.Age > ChunkLife - 0.8f) C.bSinking = true;
			continue;
		}
		C.Age += Dt;
		bAny = true;
		if (C.bSinking)
		{
			// Big or far pieces settle into the turf and are gone by ChunkLife (no shrinking).
			C.Sink += Dt;
			C.Pos.Z -= C.Size * 1.1f * Dt / 0.8f;
			M->SetRelativeLocation(C.Pos);
			if (C.Sink >= 0.8f) { C.bLive = false; M->SetVisibility(false); }
			continue;
		}
		C.Vel.Z -= 980.f * Dt;
		C.Vel *= FMath::Max(0.f, 1.f - 0.1f * Dt);
		C.Pos += C.Vel * Dt;
		const float WMag = (float)C.W.Size();
		if (WMag > 1e-3f) C.Rot = FQuat(C.W / WMag, WMag * Dt) * C.Rot;
		C.Rot.Normalize();
		float Low = 0.f;
		for (int32 k = 0; k < C.Corners.Num(); ++k) Low = FMath::Min(Low, (float)C.Rot.RotateVector(C.Corners[k]).Z);
		if (!C.bNoGround && C.Pos.Z + Low < FloorZ)
		{
			if (!C.bGroundChecked) { C.bGroundChecked = true; C.bNoGround = !HasGround(C.Pos); }
			if (!C.bNoGround)
			{
				C.Pos.Z = FloorZ - Low;
				const FVector Hv(C.Vel.X, C.Vel.Y, 0.f);
				const FVector RollW = FVector::CrossProduct(FVector::UpVector, Hv) / FMath::Max(C.Size * 0.4f, 1.f);
				if (C.Vel.Z < -60.f)
				{
					// Bounce: lose most of the vertical speed, scrub some slide, spin picks up the roll.
					C.Vel.Z = -C.Vel.Z * 0.3f;
					C.Vel.X *= 0.62f; C.Vel.Y *= 0.62f;
					C.W = C.W * 0.55f + RollW * 0.45f;
				}
				else
				{
					// On the turf: tumble/roll to a stop (angular stones stop quickly).
					C.Vel.Z = 0.f;
					const float Sp = (float)Hv.Size();
					const float NewSp = FMath::Max(0.f, Sp - 520.f * Dt);
					C.Vel = Sp > 1e-3f ? Hv * (NewSp / Sp) : FVector::ZeroVector;
					C.W = FMath::Lerp(C.W, FVector::CrossProduct(FVector::UpVector, C.Vel) / FMath::Max(C.Size * 0.4f, 1.f), FMath::Min(1.f, 6.f * Dt));
					if (NewSp < 4.f)
					{
						C.bRest = true;
						C.Vel = FVector::ZeroVector;
						C.W = FVector::ZeroVector;
						C.Pos.Z -= FMath::Min(C.Size * 0.12f, 2.5f); // nestle into the grass
						C.bMayStay = C.bMayStay && FVector2D(C.Pos.X, C.Pos.Y).Size() <= Radius * 2.2f;
					}
				}
			}
		}
		if (C.bNoGround && (C.Pos.Z < FloorZ - 1500.f || C.Age > ChunkLife + 1.f)) { C.bLive = false; M->SetVisibility(false); continue; }
		if (!C.bMayStay && !C.bRest && !C.bNoGround && C.Age > ChunkLife - 0.8f) C.bSinking = true;
		if (C.bMayStay && !C.bRest && C.Age > ChunkLife + 2.f) { C.bRest = true; C.Vel = FVector::ZeroVector; C.W = FVector::ZeroVector; }
		M->SetRelativeLocationAndRotation(C.Pos, C.Rot);
	}
	return bAny;
}

bool AZoogiRock::TickGrit(float Dt, float FloorZ)
{
	bool bAny = false;
	for (int32 s = 0; s < GritISM.Num() && s < 2; ++s)
	{
		UInstancedStaticMeshComponent* ISM = GritISM[s];
		if (!ISM) continue;
		bool bDirty = false;
		for (int32 k = 0; k < GritPerSet; ++k)
		{
			FGrit& G = GritParts[s * GritPerSet + k];
			if (!G.bLive || G.bRest) continue;
			bAny = true;
			bDirty = true;
			G.Age += Dt;
			G.Vel.Z -= 980.f * Dt;
			G.Vel *= FMath::Max(0.f, 1.f - 0.7f * Dt);
			G.Pos += G.Vel * Dt;
			const float WMag = (float)G.W.Size();
			if (WMag > 1e-3f) G.Rot = FQuat(G.W / WMag, WMag * Dt) * G.Rot;
			G.Rot.Normalize();
			const FVector Ax = G.Rot.GetAxisX(), Ay = G.Rot.GetAxisY(), Az = G.Rot.GetAxisZ();
			const float Low = -(float)(FMath::Abs(Ax.Z) * G.Half.X + FMath::Abs(Ay.Z) * G.Half.Y + FMath::Abs(Az.Z) * G.Half.Z);
			if (!G.bNoGround && G.Pos.Z + Low < FloorZ)
			{
				if (!G.bGroundChecked) { G.bGroundChecked = true; G.bNoGround = !HasGround(G.Pos); }
				if (!G.bNoGround)
				{
					G.Pos.Z = FloorZ - Low;
					if (G.Vel.Z < -50.f)
					{
						G.Vel.Z *= -0.25f;
						G.Vel.X *= 0.5f; G.Vel.Y *= 0.5f;
						G.W *= 0.5f;
					}
					else
					{
						G.Vel.Z = 0.f;
						const FVector Hv(G.Vel.X, G.Vel.Y, 0.f);
						const float Sp = (float)Hv.Size();
						const float NewSp = FMath::Max(0.f, Sp - 900.f * Dt);
						G.Vel = Sp > 1e-3f ? Hv * (NewSp / Sp) : FVector::ZeroVector;
						G.W *= FMath::Max(0.f, 1.f - 8.f * Dt);
						if (NewSp < 6.f)
						{
							G.bRest = true; // stays on the turf until ResetRock
							G.Vel = FVector::ZeroVector;
							G.W = FVector::ZeroVector;
							G.Pos.Z -= G.Half.GetMin() * 0.5f;
						}
					}
				}
			}
			if (G.bNoGround && (G.Pos.Z < FloorZ - 1500.f || G.Age > 4.f)) G.bLive = false;
			ISM->UpdateInstanceTransform(k, G.bLive ? FTransform(G.Rot, G.Pos, G.Half / 50.f) : HiddenXf(), false, false, true);
		}
		if (bDirty) ISM->MarkRenderStateDirty();
	}
	return bAny;
}

bool AZoogiRock::TickPuffs(float Dt, float FloorZ)
{
	bool bAny = false;
	const float WindA = (float)Seed * 2.399f;
	const FVector Wind(FMath::Cos(WindA) * 12.f, FMath::Sin(WindA) * 12.f, 0.f); // faint, seeded drift
	for (int32 i = 0; i < PuffState.Num(); ++i)
	{
		FPuff& F = PuffState[i];
		if (!F.bLive) continue;
		UStaticMeshComponent* P = Puffs.IsValidIndex(i) ? Puffs[i].Get() : nullptr;
		F.Age += Dt;
		if (F.Age >= F.Life || !P) { F.bLive = false; if (P) P->SetVisibility(false); continue; }
		bAny = true;
		const float T = F.Age / F.Life;
		F.Vel *= FMath::Exp(-F.Drag * Dt);
		if (T > 0.3f) F.Vel.Z = FMath::FInterpTo((float)F.Vel.Z, -F.Settle, Dt, 1.5f); // after the burst the dust sinks
		F.Pos += (F.Vel + Wind) * Dt;
		const float Grow = 0.75f * (1.f - FMath::Exp(-2.8f * F.Age)) + 0.25f * T; // fast billow, then a slow spread
		const float R = FMath::Lerp(F.R0, F.R1, Grow);
		F.Pos.Z = FMath::Max((float)F.Pos.Z, FloorZ + R * F.Flat * 0.35f);
		// Fade in fast, hang, then thin out as it spreads and settles.
		const float Op = F.Peak * FMath::Clamp(F.Age / 0.08f, 0.f, 1.f) * (1.f - FMath::SmoothStep(0.3f, 1.f, T)) * FMath::Lerp(1.f, 0.6f, Grow);
		P->SetRelativeLocation(F.Pos);
		P->SetRelativeScale3D(FVector(R, R, R * F.Flat) / 50.f);
		if (PuffMIDs.IsValidIndex(i) && PuffMIDs[i]) PuffMIDs[i]->SetScalarParameterValue(TEXT("Opacity"), Op);
	}
	return bAny;
}

void AZoogiRock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Scripted cosmetic debris only: shards / pebbles (bounce, tumble, rest or sink), grit, dust. Off when all settle.
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	const float FloorZ = -(float)GetActorLocation().Z; // court top is Z = 0
	const bool bChunks = TickChunks(Dt, FloorZ);
	const bool bGrit = TickGrit(Dt, FloorZ);
	const bool bPuffs = TickPuffs(Dt, FloorZ);
	if (!bChunks && !bGrit && !bPuffs) SetActorTickEnabled(false);
}
