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
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Realism pass (Oct 4 evening; pass 2 ~4:30 PM). Everything here is cosmetic: no debris piece has collision or a physics body, every
// "bounce / roll / settle" is scripted against the court plane (Z = 0), and the hit logic in OnBallHit is unchanged.
namespace
{
	constexpr int32 NumCrackPieces = 5;   // pool [0] = impact flake, [1..4] = pebbles; then ChunkCount shards, then small shards
	constexpr int32 NumSmallShards = 12;  // pass 2: many small chunky pieces (no shadow)
	constexpr int32 GritPerSet = 40;      // two instanced grit sets (dark / fresh): 2 draw calls for all grit
	constexpr int32 CrackGritPerSet = 14; // crack uses [0, 14) of each set, shatter the rest
	constexpr int32 PuffCount = 24;
	constexpr int32 BodyCols = 56;        // boulder lat/long grid (even: the split uses half the columns per side)
	constexpr int32 BodyRows = 40;        // rows from BodyElMin up to the pole (fine enough for a jagged fracture line)
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
	/** Multi-scale 1D noise in about [-1, 1]: big meander + medium kinks + fine grain (no regular period). */
	float Fbm1(float X, float Off)
	{
		return 0.6f * FMath::PerlinNoise1D(X + Off) + 0.28f * FMath::PerlinNoise1D(X * 2.13f + Off * 1.7f + 3.1f)
			+ 0.14f * FMath::PerlinNoise1D(X * 4.37f + Off * 2.3f + 7.7f);
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

	struct FTri3 { int32 A, B, C; };
	struct FEdge2 { int32 A, B; };

	/** Convex hull of a small point set (incremental; fine for <= ~20 points). Triangles are wound outwards. */
	void ConvexHull(const TArray<FVector>& P, TArray<FTri3>& Out)
	{
		struct FFace { int32 A, B, C; FVector N; double D; };
		Out.Reset();
		const int32 Num = P.Num();
		if (Num < 4) return;
		int32 I1 = -1, I2 = -1, I3 = -1;
		double Best = 0.0;
		for (int32 i = 1; i < Num; ++i) { const double D = (P[i] - P[0]).SizeSquared(); if (D > Best) { Best = D; I1 = i; } }
		if (I1 < 0 || Best < 1e-6) return;
		Best = 0.0;
		for (int32 i = 1; i < Num; ++i) { const double D = FVector::CrossProduct(P[i] - P[0], P[I1] - P[0]).SizeSquared(); if (D > Best) { Best = D; I2 = i; } }
		if (I2 < 0 || Best < 1e-6) return;
		const FVector N0 = FVector::CrossProduct(P[I1] - P[0], P[I2] - P[0]).GetSafeNormal();
		Best = 0.0;
		for (int32 i = 1; i < Num; ++i) { const double D = FMath::Abs(FVector::DotProduct(P[i] - P[0], N0)); if (D > Best) { Best = D; I3 = i; } }
		if (I3 < 0 || Best < 1e-3) return;
		const FVector Inside = (P[0] + P[I1] + P[I2] + P[I3]) / 4.0;
		TArray<FFace> F;
		auto AddFace = [&](int32 A, int32 B, int32 C)
		{
			FVector Nf = FVector::CrossProduct(P[B] - P[A], P[C] - P[A]);
			const double L = Nf.Size();
			if (L < 1e-9) return;
			Nf /= L;
			if (FVector::DotProduct(Nf, P[A] - Inside) < 0.0) { const int32 Tmp = B; B = C; C = Tmp; Nf = -Nf; }
			F.Add(FFace{ A, B, C, Nf, FVector::DotProduct(Nf, P[A]) });
		};
		AddFace(0, I1, I2); AddFace(0, I1, I3); AddFace(0, I2, I3); AddFace(I1, I2, I3);
		for (int32 i = 1; i < Num; ++i)
		{
			if (i == I1 || i == I2 || i == I3) continue;
			TArray<FEdge2> Edges;
			TArray<FFace> Keep;
			for (const FFace& Fc : F)
			{
				if (FVector::DotProduct(Fc.N, P[i]) - Fc.D > 1e-3)
				{
					Edges.Add(FEdge2{ Fc.A, Fc.B }); Edges.Add(FEdge2{ Fc.B, Fc.C }); Edges.Add(FEdge2{ Fc.C, Fc.A });
				}
				else Keep.Add(Fc);
			}
			if (Edges.Num() == 0) continue;
			F = Keep;
			for (const FEdge2& E : Edges)
			{
				bool bShared = false;
				for (const FEdge2& E2 : Edges) if (E2.A == E.B && E2.B == E.A) { bShared = true; break; }
				if (!bShared) AddFace(E.A, E.B, i); // horizon edge
			}
		}
		for (const FFace& Fc : F) Out.Add(FTri3{ Fc.A, Fc.B, Fc.C });
	}

	/** A chunky broken stone: random points in an ellipsoid of the given size, 1-3 of them cleaved flat (fresh breaks),
	 *  convex hull. Taper > 0 makes a tooth: base at Z = 0, narrowing to an off-centre tip at Size.Z. */
	struct FStone
	{
		TArray<FVector> Pts;
		TArray<FTri3> Tris;
		TArray<FVector> FreshN; // outward normals of the cleaved (fresh-break) planes
	};
	void MakeStone(FRandomStream& Rng, const FVector& Size, int32 NumPts, int32 NumCleave, bool bTooth, bool bCenter, FStone& S,
		const FVector* ForcedCleave = nullptr)
	{
		const FVector H = Size * 0.5f;
		S.Pts.Reset(); S.FreshN.Reset();
		const FVector Tip(Rng.FRandRange(-0.35f, 0.35f) * H.X, Rng.FRandRange(-0.35f, 0.35f) * H.Y, (float)Size.Z);
		for (int32 k = 0; k < NumPts; ++k)
		{
			if (bTooth)
			{
				const float Z01 = k == 0 ? 1.f : FMath::Pow(Rng.FRandRange(0.f, 1.f), 0.8f);
				const float Rs = FMath::Lerp(1.f, 0.12f, Z01) * Rng.FRandRange(0.65f, 1.f);
				const float A = Rng.FRandRange(0.f, 2.f * PI);
				const FVector Axis = FMath::Lerp(FVector(0.f, 0.f, 0.f), FVector(Tip.X, Tip.Y, 0.f), Z01);
				S.Pts.Add(Axis + FVector(FMath::Cos(A) * H.X * Rs, FMath::Sin(A) * H.Y * Rs, Z01 * (float)Size.Z * (k == 0 ? 1.f : Rng.FRandRange(0.9f, 1.f))));
			}
			else
			{
				const FVector D = Rng.VRand() * Rng.FRandRange(0.72f, 1.f);
				S.Pts.Add(FVector(D.X * H.X, D.Y * H.Y, D.Z * H.Z));
			}
		}
		for (int32 c = 0; c < NumCleave + (ForcedCleave ? 1 : 0); ++c)
		{
			const bool bForced = ForcedCleave && c == NumCleave;
			FVector Nc = bForced ? ForcedCleave->GetSafeNormal() : Rng.VRand();
			if (bTooth && !bForced) Nc.Z *= 0.3f; // teeth break along steep planes
			Nc = Nc.GetSafeNormal();
			if (Nc.IsNearlyZero()) continue;
			const FVector Ref = bTooth ? FVector(0.f, 0.f, Size.Z * 0.4f) : FVector::ZeroVector;
			const float Extent = (float)(FMath::Abs(Nc.X) * H.X + FMath::Abs(Nc.Y) * H.Y + FMath::Abs(Nc.Z) * H.Z);
			const float Lim = Extent * (bForced ? 0.3f : Rng.FRandRange(0.38f, 0.7f));
			for (FVector& P : S.Pts)
			{
				const float D = (float)FVector::DotProduct(P - Ref, Nc);
				if (D > Lim) P -= Nc * (D - Lim);
			}
			S.FreshN.Add(Nc);
		}
		if (bCenter)
		{
			FVector Cen = FVector::ZeroVector;
			for (const FVector& P : S.Pts) Cen += P;
			Cen /= (double)FMath::Max(1, S.Pts.Num());
			for (FVector& P : S.Pts) P -= Cen; // pivot at the centroid so the tumble looks right
		}
		ConvexHull(S.Pts, S.Tris);
		if (S.Tris.Num() < 4)
		{
			// Degenerate sample (should not happen): fall back to a box.
			S.Pts.Reset(); S.Tris.Reset();
			for (int32 i = 0; i < 8; ++i) S.Pts.Add(FVector((i & 1) ? H.X : -H.X, (i & 2) ? H.Y : -H.Y, (i & 4) ? H.Z : -H.Z));
			ConvexHull(S.Pts, S.Tris);
		}
	}

	/** Appends a stone as flat facets; facets lying on a cleave plane go to the fresh-break section. */
	void AppendStone(const FStone& S, const FTransform& Xf, FMeshBuf& Outer, FMeshBuf& Fresh)
	{
		FVector Cen = FVector::ZeroVector;
		for (const FVector& P : S.Pts) Cen += P;
		Cen /= (double)FMath::Max(1, S.Pts.Num());
		for (const FTri3& Tr : S.Tris)
		{
			const FVector A = S.Pts[Tr.A], B = S.Pts[Tr.B], C = S.Pts[Tr.C];
			FVector Nl = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
			if (FVector::DotProduct(Nl, (A + B + C) / 3.0 - Cen) < 0.0) Nl = -Nl;
			bool bFresh = false;
			for (const FVector& Fn : S.FreshN) if (FVector::DotProduct(Nl, Fn) > 0.97) { bFresh = true; break; }
			const FVector WA = Xf.TransformPosition(A), WB = Xf.TransformPosition(B), WC = Xf.TransformPosition(C);
			(bFresh ? Fresh : Outer).Flat(WA, WB, WC, Xf.TransformVector(Nl));
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

void AZoogiRock::SetCarvedStyle(float InLump, const FLinearColor& InRuneColor, float InRuneStrength)
{
	bCarved = true;
	StyleLump = InLump;
	RuneColor = InRuneColor;
	RuneStrength = InRuneStrength;
}

void AZoogiRock::BuildRunes()
{
	// A band of simple carved glyphs (bars, chevrons, ticks) around the stone at ~45% height, laid on the intact surface
	// 0.7 cm out. Rock space (the component carries the yaw like CrackMesh).
	if (!RuneMesh) return;
	FMeshBuf B;
	FRandomStream R(Seed * 31 + 5);
	const int32 Glyphs = FMath::Clamp(FMath::RoundToInt(2.f * PI * Radius / 55.f), 5, 12);
	const float El0 = FMath::Asin(0.42f);
	auto At = [&](float Az, float El, FVector& OutN) { const FVector U = DirAE(Az, El); OutN = Shape.Normal(U); return Shape.Point(U) + OutN * 0.7f; };
	auto Stroke = [&](float Az, float El, float DAz0, float DEl0, float DAz1, float DEl1)
	{
		const int32 Seg = 4;
		const float W = FMath::Clamp(Radius * 0.022f, 1.6f, 2.6f);
		TArray<FVector> L, Rr; TArray<FVector> Ns;
		for (int32 i = 0; i <= Seg; ++i)
		{
			const float T = (float)i / Seg;
			FVector N;
			const FVector P = At(Az + FMath::Lerp(DAz0, DAz1, T), El + FMath::Lerp(DEl0, DEl1, T), N);
			Ns.Add(N); L.Add(P);
		}
		for (int32 i = 0; i <= Seg; ++i)
		{
			const FVector D = (L[FMath::Min(i + 1, Seg)] - L[FMath::Max(i - 1, 0)]).GetSafeNormal();
			const FVector Side = FVector::CrossProduct(Ns[i], D).GetSafeNormal() * W * 0.5f;
			Rr.Add(L[i] - Side); L[i] += Side;
		}
		for (int32 i = 0; i < Seg; ++i)
		{
			const int32 A = B.Add(L[i], Ns[i]), Bb = B.Add(Rr[i], Ns[i]), C = B.Add(L[i + 1], Ns[i + 1]), D = B.Add(Rr[i + 1], Ns[i + 1]);
			B.Tri(A, Bb, D, Ns[i]); B.Tri(A, D, C, Ns[i]);
		}
	};
	const float GH = 0.16f; // glyph half height in elevation (rad)
	for (int32 g = 0; g < Glyphs; ++g)
	{
		const float Az = (g + 0.5f) * 2.f * PI / Glyphs + R.FRandRange(-0.05f, 0.05f);
		const float GW = 0.32f / Glyphs * 2.f * PI * 0.25f; // glyph half width in azimuth
		Stroke(Az, El0, 0.f, -GH, 0.f, GH);
		switch (R.RandRange(0, 3))
		{
		case 0: Stroke(Az, El0, -GW, GH, 0.f, 0.f); Stroke(Az, El0, GW, GH, 0.f, 0.f); break;
		case 1: Stroke(Az, El0, -GW, -GH * 0.2f, GW, -GH * 0.2f); break;
		case 2: Stroke(Az, El0, -GW, -GH, GW, GH); break;
		default: Stroke(Az, El0, 0.f, GH, GW, GH * 0.3f); Stroke(Az, El0, 0.f, -GH * 0.3f, -GW, -GH); break;
		}
	}
	// Two thin bands framing the glyphs.
	for (int32 s = 0; s < 2; ++s)
	{
		const float El = El0 + (s ? GH * 1.45f : -GH * 1.45f);
		for (int32 i = 0; i < 48; ++i) Stroke(i * 2.f * PI / 48.f, El, 0.f, 0.f, 2.f * PI / 48.f * 1.02f, 0.f);
	}
	Upload(RuneMesh, 0, B);
}

void AZoogiRock::InitRock(float InRadius, float InHeight, float InYaw, const FLinearColor& ColA, const FLinearColor& ColB, UPhysicalMaterial* PM, int32 InSeed)
{
	Radius = InRadius; Height = InHeight; Yaw = InYaw; Seed = InSeed;
	UStaticMesh* Sphere = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));

	// Materials: weathered surface (same as before), a slightly duller cracked surface, near-black gaps, lighter chipped
	// edges, a light fresh-stone scar, a pale crushed-stone powder, dark fracture faces. Moving pieces use flat SurfaceMIDs
	// (GroundMID's world-space noise would swim on them) and the instanced grit must use M_ZoogiSurface (ISM usage flag).
	const FLinearColor Stone = ColA;
	const FLinearColor Fresh(FMath::Min(Stone.R * 1.5f + 0.02f, 0.6f), FMath::Min(Stone.G * 1.5f + 0.015f, 0.6f), FMath::Min(Stone.B * 1.5f, 0.6f), 1.f);
	MatIntact = ZoogiUtil::GroundMID(this, ColA, ColB, 0.02f, 0.85f);
	MatCracked = ZoogiUtil::GroundMID(this, ColA * 0.88f, ColB * 0.8f, 0.02f, 0.88f);
	MatGap = ZoogiUtil::SurfaceMID(this, FLinearColor(Stone.R * 0.06f, Stone.G * 0.055f, Stone.B * 0.05f), 0.95f);
	MatEdge = ZoogiUtil::SurfaceMID(this, Stone * 1.3f, 0.9f);
	MatScar = ZoogiUtil::SurfaceMID(this, Fresh, 0.88f);
	MatPowder = ZoogiUtil::SurfaceMID(this, FLinearColor(FMath::Min(Fresh.R * 1.45f + 0.04f, 0.75f), FMath::Min(Fresh.G * 1.45f + 0.04f, 0.72f), FMath::Min(Fresh.B * 1.45f + 0.035f, 0.68f)), 1.f);
	MatFracture = ZoogiUtil::SurfaceMID(this, FLinearColor(Stone.R * 0.16f, Stone.G * 0.15f, Stone.B * 0.135f), 0.95f);
	MatFreshDark = ZoogiUtil::SurfaceMID(this, FLinearColor(Stone.R * 0.55f, Stone.G * 0.51f, Stone.B * 0.46f), 0.92f);
	const FLinearColor ShardBase = FMath::Lerp(ColA, ColB, 0.18f);
	MatShard.Reset();
	MatShard.Add(ZoogiUtil::SurfaceMID(this, ShardBase, 0.9f));
	MatShard.Add(ZoogiUtil::SurfaceMID(this, ShardBase * 0.86f, 0.9f));
	MatShard.Add(ZoogiUtil::SurfaceMID(this, ShardBase * 0.74f, 0.92f));
	// Dust: a light grey-brown that reads against bright grass (lit, never emissive).
	DustCol = FMath::Lerp(Stone, FLinearColor(0.46f, 0.42f, 0.36f), 0.7f);
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
	Shape.Init(FVector(Radius, Radius * 0.9f, Height), Rng, bCarved ? StyleLump : 1.f);
	if (bCarved)
	{
		// Cut-stone block: a flat dressed top and four near-vertical cut faces (no extra Rng draws, so the chip/crack
		// layout seeded above is unchanged). Faces stay inside the collider footprint (RadiusScale clamps 0.8..1.04).
		const float Az0 = (Seed % 7) * 0.37f;
		Shape.FacetN[0] = FVector(0.f, 0.f, 1.f); Shape.FacetH[0] = 0.86f;
		for (int32 k = 1; k < 5; ++k)
		{
			const float Az = Az0 + (k - 1) * PI * 0.5f + ((Seed * (k + 3)) % 5 - 2) * 0.06f;
			Shape.FacetN[k] = FVector(FMath::Cos(Az) * 0.985f, FMath::Sin(Az) * 0.985f, 0.17f).GetSafeNormal();
			Shape.FacetH[k] = 0.88f + ((Seed + k) % 3) * 0.02f;
		}
	}
	Body = MakeProc(true);
	Body->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	Side = MakeProc(true);
	BuildSide(Rng);
	CrackMesh = MakeProc(false); // decals: no shadow (they sit ~1 cm off the surface)
	CrackMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	CrackMesh->SetMaterial(0, MatGap);
	CrackMesh->SetMaterial(1, MatEdge);
	CrackMesh->SetMaterial(2, MatScar);
	CrackMesh->SetMaterial(3, MatPowder);
	CrackMesh->SetVisibility(false);
	Rubble = MakeProc(true);
	Rubble->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	BuildRubble(Rng);
	BuildChunks(Rng);
	if (bCarved && RuneStrength > 0.f)
	{
		RuneMesh = MakeProc(false);
		RuneMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
		RuneMesh->SetMaterial(0, ZoogiUtil::SurfaceMID(this, RuneColor * 0.35f, 0.5f, RuneColor, RuneStrength));
		BuildRunes();
	}

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

	// Dust: soft translucent puffs (pool), only with the dust material. Soft 0.7: less edge falloff, so the puffs read
	// as solid dust; Opacity per puff may exceed 1 (the material output saturates), which keeps the core dense.
	if (bHasDust)
	{
		PuffState.SetNum(PuffCount);
		for (int32 k = 0; k < PuffCount; ++k)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(DustMat, this);
			MID->SetVectorParameterValue(TEXT("Color"), DustCol);
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.f);
			MID->SetScalarParameterValue(TEXT("Soft"), 0.7f);
			MID->SetScalarParameterValue(TEXT("FadeDistance"), 40.f);
			UStaticMeshComponent* P = MakePart(Sphere, MID, false);
			P->SetVisibility(false);
			Puffs.Add(P);
			PuffMIDs.Add(MID);
		}
	}
	ResetRock();
}

void AZoogiRock::BuildChunks(FRandomStream& Rng)
{
	// Pool: [0] flake (fresh underside), [1..4] pebbles, then ChunkCount shards in three size classes (2 big, 3 medium,
	// 3 smaller per 8), then NumSmallShards small pieces. All are chunky convex hulls with 1-3 flat fresh-break faces.
	const int32 NumMain = FMath::Max(0, ChunkCount);
	const int32 N = NumCrackPieces + NumMain + NumSmallShards;
	Chunks.SetNum(N);
	const float Rs = ScarRadius(Radius);
	for (int32 i = 0; i < N; ++i)
	{
		FVector Sz;
		int32 NumPts = 12, NumCleave = 2;
		bool bLightFresh = false;
		const FVector Down(0.f, 0.f, -1.f);
		const FVector* Forced = nullptr;
		if (i == 0)
		{
			Sz = FVector(Rs * 1.5f, Rs * 1.2f, Rs * 0.5f);
			NumCleave = 1;
			Forced = &Down; // flat fresh underside
			bLightFresh = true;
		}
		else if (i < NumCrackPieces)
		{
			const float S = FMath::Clamp(Radius * Rng.FRandRange(0.035f, 0.07f), 3.f, 9.f);
			Sz = FVector(S * Rng.FRandRange(0.85f, 1.3f), S * Rng.FRandRange(0.7f, 1.1f), S * Rng.FRandRange(0.6f, 0.95f));
			NumPts = 9; NumCleave = 1;
			bLightFresh = true;
		}
		else if (i < NumCrackPieces + NumMain)
		{
			const int32 Cls = (i - NumCrackPieces) % 8;
			const float F = Cls < 2 ? Rng.FRandRange(0.32f, 0.45f) : (Cls < 5 ? Rng.FRandRange(0.18f, 0.28f) : Rng.FRandRange(0.1f, 0.16f));
			const float S = Radius * F;
			Sz = FVector(S * Rng.FRandRange(0.85f, 1.2f), S * Rng.FRandRange(0.65f, 1.f), S * Rng.FRandRange(0.55f, 0.9f));
			NumPts = Cls < 5 ? 14 : 11; NumCleave = Cls < 2 ? 3 : 2;
		}
		else
		{
			const float F = 0.035f + 0.06f * FMath::Pow(Rng.FRandRange(0.f, 1.f), 1.5f); // skewed towards small
			const float S = FMath::Max(Radius * F, 2.5f);
			Sz = FVector(S * Rng.FRandRange(0.8f, 1.2f), S * Rng.FRandRange(0.65f, 1.f), S * Rng.FRandRange(0.55f, 0.9f));
			NumPts = 9; NumCleave = 1;
		}
		FStone St;
		MakeStone(Rng, Sz, NumPts, NumCleave, false, true, St, Forced);
		FChunk& C = Chunks[i];
		C.Corners = St.Pts;
		C.Size = (float)Sz.GetMax();
		FMeshBuf Outer, FreshB;
		AppendStone(St, FTransform::Identity, Outer, FreshB);
		const bool bShadow = i == 0 || (i >= NumCrackPieces && i < NumCrackPieces + NumMain); // small bits: no shadow
		UProceduralMeshComponent* M = MakeProc(bShadow);
		Upload(M, 0, Outer);
		Upload(M, 1, FreshB);
		M->SetMaterial(0, MatShard[i % MatShard.Num()]);
		M->SetMaterial(1, bLightFresh ? MatScar.Get() : MatFreshDark.Get());
		M->SetVisibility(false);
		ChunkPool.Add(M);
	}
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

void AZoogiRock::BuildRubble(FRandomStream& Rng)
{
	// What's left after a shatter (no collision, low enough to roll over): an irregular broken base made of Voronoi
	// cells (each a facet with its own height and tilt, fresh-break dark on top, cliffs between cells), two tall jagged
	// teeth, fragments leaning on the base, loose stones around it and a pale stone-powder stain. No radial structure.
	FMeshBuf Outer, FreshB, Powder;
	const float Floor = -(float)GetActorLocation().Z; // rock-space court height
	const FVector Up = FVector::UpVector;
	const float NoiseOff = Rng.FRandRange(0.f, 100.f);

	// Footprint of the base: ~70% of the boulder's foot, with a multi-scale irregular outline.
	const int32 NB = 30;
	TArray<FVector> Bnd;
	for (int32 i = 0; i < NB; ++i)
	{
		const float Az = 2.f * PI * ((float)i + Rng.FRandRange(-0.35f, 0.35f)) / (float)NB;
		const FVector Foot = Shape.Point(DirAE(Az, 0.f));
		const float K = 0.7f * (1.f + 0.13f * Fbm1(Az * 1.6f, NoiseOff) + Rng.FRandRange(-0.03f, 0.03f));
		Bnd.Add(FVector(Foot.X * K, Foot.Y * K, 0.f));
	}
	// Cell seeds (jittered, minimum spacing) with a plane each: height above the turf + tilt.
	const float Rb = Radius * 0.7f;
	struct FCellSeed { FVector P; float H; FVector G; };
	TArray<FCellSeed> Seeds;
	const int32 WantSeeds = 11 + Rng.RandRange(0, 4);
	for (int32 Try = 0; Try < 200 && Seeds.Num() < WantSeeds; ++Try)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI), D = Rb * FMath::Sqrt(Rng.FRandRange(0.f, 1.f)) * 0.95f;
		const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.9f, 0.f);
		bool bOk = true;
		for (const FCellSeed& S : Seeds) if (FVector::Dist(S.P, P) < Rb * 0.33f) { bOk = false; break; }
		if (!bOk) continue;
		const float Gm = Rng.FRandRange(0.08f, 0.6f), Ga = Rng.FRandRange(0.f, 2.f * PI);
		// Lower towards the rim on average, with big random variation (a broken surface, not a dome).
		const float H = FMath::Max(2.5f, Rng.FRandRange(3.f, 13.f) * (1.05f - 0.45f * D / Rb));
		Seeds.Add(FCellSeed{ P, H, FVector(FMath::Cos(Ga) * Gm, FMath::Sin(Ga) * Gm, 0.f) });
	}
	auto TopZ = [&](int32 c, const FVector& X)
	{
		const FCellSeed& S = Seeds[c];
		return Floor + FMath::Clamp(S.H + (float)FVector::DotProduct(S.G, X - S.P), 1.2f, 20.f);
	};
	struct FCV { FVector P; int32 Tag; }; // Tag: -1 = outline edge, otherwise the neighbour cell across this edge
	for (int32 c = 0; c < Seeds.Num(); ++c)
	{
		TArray<FCV> Poly;
		for (const FVector& B : Bnd) Poly.Add(FCV{ B, -1 });
		for (int32 o = 0; o < Seeds.Num() && Poly.Num() >= 3; ++o)
		{
			if (o == c) continue;
			const FVector Dir = Seeds[o].P - Seeds[c].P;
			const FVector Mid = (Seeds[o].P + Seeds[c].P) * 0.5;
			TArray<FCV> Clip;
			for (int32 k = 0; k < Poly.Num(); ++k)
			{
				const FCV& A = Poly[k];
				const FCV& B = Poly[(k + 1) % Poly.Num()];
				const double Fa = FVector::DotProduct(A.P - Mid, Dir), Fb = FVector::DotProduct(B.P - Mid, Dir);
				if (Fa <= 0.0) Clip.Add(A);
				if ((Fa <= 0.0) != (Fb <= 0.0))
				{
					const FVector X = A.P + (B.P - A.P) * (Fa / (Fa - Fb));
					Clip.Add(FCV{ X, Fa <= 0.0 ? o : A.Tag });
				}
			}
			Poly = Clip;
		}
		if (Poly.Num() < 3) continue;
		FVector Cen = FVector::ZeroVector;
		for (const FCV& V : Poly) Cen += V.P;
		Cen /= (double)Poly.Num();
		const FVector CenTop(Cen.X, Cen.Y, TopZ(c, Cen));
		for (int32 k = 0; k < Poly.Num(); ++k)
		{
			const FCV& A = Poly[k];
			const FCV& B = Poly[(k + 1) % Poly.Num()];
			const FVector At(A.P.X, A.P.Y, TopZ(c, A.P)), Bt(B.P.X, B.P.Y, TopZ(c, B.P));
			FreshB.Flat(CenTop, At, Bt, Up); // the cell's broken top facet
			const FVector EdgeOut = FVector::CrossProduct(B.P - A.P, Up).GetSafeNormal() * -1.0; // outwards for a CCW cell
			const FVector Out = FVector::DotProduct(EdgeOut, (A.P + B.P) * 0.5 - Cen) >= 0.0 ? EdgeOut : -EdgeOut;
			if (A.Tag < 0)
			{
				// Outer wall: weathered boulder surface, flaring a little into the turf.
				const FVector Ab(A.P.X * 1.06, A.P.Y * 1.06, Floor - 4.f), Bb(B.P.X * 1.06, B.P.Y * 1.06, Floor - 4.f);
				Outer.Flat(At, Bt, Bb, Out);
				Outer.Flat(At, Bb, Ab, Out);
			}
			else if (A.Tag < Seeds.Num())
			{
				// Cliff between cells: only the part above the neighbour's facet, fresh stone.
				const float Na = TopZ(A.Tag, A.P), Nb = TopZ(A.Tag, B.P);
				if (At.Z > Na + 0.3f || Bt.Z > Nb + 0.3f)
				{
					const FVector Ab(A.P.X, A.P.Y, FMath::Min((float)At.Z, Na) - 0.3f), Bb(B.P.X, B.P.Y, FMath::Min((float)Bt.Z, Nb) - 0.3f);
					FreshB.Flat(At, Bt, Bb, Out);
					FreshB.Flat(At, Bb, Ab, Out);
				}
			}
		}
	}

	// Two tall jagged teeth on the rim (well apart), leaning out a little; their inner faces are fresh breaks.
	const float ToothAz0 = Rng.FRandRange(0.f, 2.f * PI);
	for (int32 t = 0; t < 2; ++t)
	{
		const float Az = ToothAz0 + (t == 0 ? 0.f : Rng.FRandRange(0.65f, 1.35f) * PI);
		const FVector Rim = Shape.Point(DirAE(Az, 0.f)) * 0.6f;
		const float Ht = FMath::Clamp(Height * Rng.FRandRange(0.36f, 0.55f), 16.f, 45.f) * (t == 0 ? 1.f : 0.75f);
		const FVector Sz(Radius * Rng.FRandRange(0.16f, 0.24f), Radius * Rng.FRandRange(0.12f, 0.2f), Ht);
		const FVector Inward(-1.f, 0.f, 0.25f); // tooth frame: X = radial outward
		FStone St;
		MakeStone(Rng, Sz, 13, 1, true, false, St, &Inward);
		const FQuat Q = FRotator(0.f, FMath::RadiansToDegrees(Az), 0.f).Quaternion() * FRotator(-Rng.FRandRange(5.f, 16.f), Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(-8.f, 8.f)).Quaternion();
		AppendStone(St, FTransform(Q, FVector(Rim.X, Rim.Y, Floor - 3.f)), Outer, FreshB);
	}
	// Fragments that broke off and lean against the base.
	const int32 NumLean = 4 + Rng.RandRange(0, 2);
	for (int32 k = 0; k < NumLean; ++k)
	{
		const float Az = Rng.FRandRange(0.f, 2.f * PI);
		const float S = Radius * Rng.FRandRange(0.11f, 0.2f);
		const FVector Sz(S * Rng.FRandRange(0.9f, 1.3f), S * Rng.FRandRange(0.6f, 1.f), S * Rng.FRandRange(0.4f, 0.7f));
		FStone St;
		MakeStone(Rng, Sz, 11, 2, false, true, St);
		const FQuat Q = FRotator(0.f, FMath::RadiansToDegrees(Az), 0.f).Quaternion() * FRotator(Rng.FRandRange(25.f, 50.f), Rng.FRandRange(-25.f, 25.f), Rng.FRandRange(-15.f, 15.f)).Quaternion();
		float Low = 1e9f;
		for (const FVector& P : St.Pts) Low = FMath::Min(Low, (float)Q.RotateVector(P).Z);
		const FVector Foot = Shape.Point(DirAE(Az, 0.f)) * (0.72f + S * 0.35f / Radius);
		AppendStone(St, FTransform(Q, FVector(Foot.X, Foot.Y, Floor - 1.5f - Low)), Outer, FreshB);
	}
	// Loose stones scattered around (low, half-buried).
	for (int32 k = 0; k < 7; ++k)
	{
		const float Az = Rng.FRandRange(0.f, 2.f * PI);
		const float D = Radius * Rng.FRandRange(0.85f, 1.35f);
		const FVector Sz(Radius * Rng.FRandRange(0.07f, 0.16f), Radius * Rng.FRandRange(0.06f, 0.12f), FMath::Min(Radius * Rng.FRandRange(0.05f, 0.08f), 9.f));
		FStone St;
		MakeStone(Rng, Sz, 10, 1, false, true, St);
		const FQuat Q = FRotator(Rng.FRandRange(-15.f, 15.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-15.f, 15.f)).Quaternion();
		float Low = 1e9f;
		for (const FVector& P : St.Pts) Low = FMath::Min(Low, (float)Q.RotateVector(P).Z);
		AppendStone(St, FTransform(Q, FVector(FMath::Cos(Az) * D, FMath::Sin(Az) * D, Floor - 1.5f - Low)), Outer, FreshB);
	}
	// Powder stain: irregular outline, fades out at the edge (vertex alpha).
	if (bHasDust)
	{
		const int32 NP = 28;
		const int32 C0 = Powder.Add(FVector(0.f, 0.f, Floor + 1.f), Up, 0.75f);
		TArray<int32> In, Out;
		for (int32 i = 0; i < NP; ++i)
		{
			const float Az = 2.f * PI * (float)i / (float)NP;
			const FVector D(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			const float Wob = 1.f + 0.25f * Fbm1(Az * 1.9f, NoiseOff + 40.f);
			In.Add(Powder.Add(D * (Radius * 0.8f * Wob) + FVector(0.f, 0.f, Floor + 0.9f), Up, 0.65f));
			Out.Add(Powder.Add(D * (Radius * 1.6f * Wob * Rng.FRandRange(0.85f, 1.15f)) + FVector(0.f, 0.f, Floor + 0.6f), Up, 0.f));
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
			MID->SetScalarParameterValue(TEXT("Opacity"), 0.75f);
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
	// The main fracture starts at the impact and runs down to the turf and up over the crown, bending as it climbs.
	// Half B (the -SplitN side) swings open about a vertical axis through the far foot, so the gap is widest at the
	// impact face (dark fracture faces visible inside) and closes to a hairline on the far side. The fracture line is
	// multi-scale noise (meander + kinks + grain), pinned to pass exactly through the impact point.
	bSplit = true;
	SplitAz = FMath::Atan2((float)U.Y, (float)U.X);
	const FVector G = FVector(Shape.Semi.X * FMath::Cos(SplitAz), Shape.Semi.Y * FMath::Sin(SplitAz), 0.f).GetSafeNormal();
	SplitN = FVector(-G.Y, G.X, 0.f); // = Up x G: half A is on the +SplitN side
	const FVector Far = Shape.Point(DirAE(SplitAz + PI, 0.f));
	SplitPivot = FVector(Far.X, Far.Y, 0.f);
	const FVector Imp = Shape.Point(U);
	const float Reach = FMath::Max((float)FVector::Dist(FVector(Imp.X, Imp.Y, 0.f), SplitPivot), Radius);
	SplitOpen = FMath::Clamp(Radius * 0.04f, 3.f, 5.5f) * Rng.FRandRange(0.85f, 1.15f);
	SplitQ = FQuat(FVector::UpVector, -SplitOpen / Reach); // negative: the impact end of half B moves towards -SplitN
	SplitSep = 0.25f; // hairline at the hinge
	JagA.SetNum(BodyRows);
	JagB.SetNum(BodyRows);
	const float Amp1 = FMath::Clamp(Radius * 0.05f, 3.f, 6.5f);   // meander, cm
	const float Amp2 = FMath::Clamp(Radius * 0.022f, 1.4f, 3.f);  // kinks
	const float Amp3 = FMath::Clamp(Radius * 0.01f, 0.6f, 1.4f);  // grain (per row)
	const float Curve = Rng.FRandRange(0.25f, 0.6f) * (Rng.FRand() < 0.5f ? 1.f : -1.f); // azimuth bend per radian climbed
	const float OffA = Rng.FRandRange(0.f, 100.f), OffB = Rng.FRandRange(0.f, 100.f);
	const float ImpEl = FMath::Asin(FMath::Clamp((float)U.Z, -1.f, 1.f));
	auto Smooth = [&](float El, float Off, float Scale)
	{
		const float Arc = El * Radius; // cm along the meridian
		return Scale * (Amp1 * Fbm1(Arc / 38.f, Off) + Amp2 * Fbm1(Arc / 12.f, Off + 31.f));
	};
	const float SA0 = Smooth(ImpEl, OffA, 1.f);
	for (int32 j = 0; j < BodyRows; ++j)
	{
		const float El = RowEl(j);
		const float Grain = Rng.FRandRange(-1.f, 1.f) * Amp3 * (Rng.FRand() < 0.3f ? 1.8f : 1.f);
		const float Near = FMath::Clamp(FMath::Abs(El - ImpEl) / 0.12f, 0.f, 1.f); // pinned through the impact point
		const float SA = (Smooth(El, OffA, 1.f) - SA0 + Grain) * Near;
		const float SB = Smooth(El, OffB, 0.7f) + Rng.FRandRange(-1.f, 1.f) * Amp3 * 0.7f;
		const float Fade = 1.f - FMath::SmoothStep(FMath::DegreesToRadians(72.f), FMath::DegreesToRadians(88.f), El); // meet at the crown
		const float Rr = Radius * FMath::Max(FMath::Cos(El), 0.25f);
		JagA[j] = SA * Fade / Rr + Curve * (El - ImpEl) * FMath::Lerp(1.f, 0.6f, 1.f - Fade);
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
	return (bSplit && InSide == 1) ? SplitPivot + SplitQ.RotateVector(P - SplitPivot) - SplitN * SplitSep : P;
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

			// Fracture face of this half (seen through the gap): rough, flat-shaded, dark. Fan from inside the rock
			// through two jittered rings to the jagged boundary.
			TArray<FVector> Ring;
			for (int32 j = 0; j < BodyRows; ++j) Ring.Add(Shape.Point(DirAE(s == 0 ? PhiA(j) : PhiB(j), RowEl(j))));
			for (int32 j = BodyRows - 2; j >= 0; --j) Ring.Add(Shape.Point(DirAE(s == 0 ? PhiB(j) : PhiA(j), RowEl(j))));
			FRandomStream FR(Seed * 131 + 7 + s);
			const FVector Cen(0.f, 0.f, FMath::Max(Height * 0.2f, 8.f));
			TArray<FVector> MidA, MidB;
			for (const FVector& R : Ring)
			{
				MidA.Add(FMath::Lerp(Cen, R, 0.45f) + SplitN * FR.FRandRange(-2.2f, 2.2f));
				MidB.Add(FMath::Lerp(Cen, R, 0.8f) + SplitN * FR.FRandRange(-1.3f, 1.3f));
			}
			const FVector Out = SplitNrm(s == 0 ? -SplitN : SplitN, s);
			const FVector CenS = SplitPos(Cen, s);
			for (int32 k = 0; k < Ring.Num(); ++k)
			{
				const int32 K1 = (k + 1) % Ring.Num();
				const FVector A0 = SplitPos(MidA[k], s), A1 = SplitPos(MidA[K1], s);
				const FVector B0 = SplitPos(MidB[k], s), B1 = SplitPos(MidB[K1], s);
				const FVector R0 = SplitPos(Ring[k], s), R1 = SplitPos(Ring[K1], s);
				Faces.Flat(CenS, A0, A1, Out);
				Faces.Flat(A0, B0, B1, Out);
				Faces.Flat(A0, B1, A1, Out);
				Faces.Flat(B0, R0, R1, Out);
				Faces.Flat(B0, R1, B1, Out);
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
	// Crack network on the (dented, split) surface, all radiating from the impact: crushed powdery spot + fresh scar,
	// rims along the main fracture (wide where it gapes, hairline at the hinge), 3-6 irregular branches with forks,
	// and fine hairlines around the impact. Widths and kinks use multi-scale noise, never a fixed period.
	FMeshBuf Gap, Edge, Scar, Powder;
	const float FloorLocal = -(float)GetActorLocation().Z + 2.f;
	FVector N0;
	const FVector P0 = Surf(Shape.Point(U0), 0.f, &N0); // on the dented surface
	const float Rs = ScarRadius(Radius);
	const float Rc = FMath::Clamp(Radius * 0.085f, 6.f, 11.f); // crushed spot radius
	const float NoiseO = Rng.FRandRange(0.f, 100.f);
	FVector T1 = FVector::CrossProduct(N0, FVector::UpVector).GetSafeNormal();
	if (T1.IsNearlyZero()) T1 = FVector::ForwardVector;
	const FVector T2 = FVector::CrossProduct(N0, T1);

	// 1) Irregular radial patches around the impact: fresh scar (dented, normals leaning in) and, on top, the crushed
	//    powder spot. Each vertex takes its half's offset; triangles straddling the fracture are dropped.
	auto Patch = [&](FMeshBuf& B, float R0, int32 K, int32 NumRings, float Lift, float Wobble, float Off, float Concave)
	{
		int32 C0[2];
		for (int32 s = 0; s < 2; ++s) C0[s] = B.Add(SplitPos(P0 + N0 * Lift, s), SplitNrm(N0, s));
		TArray<int32> Prev, PrevS;
		for (int32 r = 1; r <= NumRings; ++r)
		{
			TArray<int32> Cur, CurS;
			const float Fr = (float)r / (float)NumRings;
			for (int32 k = 0; k < K; ++k)
			{
				const float A = 2.f * PI * (float)k / (float)K + Rng.FRandRange(-0.12f, 0.12f);
				const float F = Fr * (1.f + Wobble * Fbm1(A * 1.3f + (float)r * 0.7f, NoiseO + Off)) * Rng.FRandRange(0.9f, 1.1f);
				const FVector Dk = T1 * FMath::Cos(A) + T2 * FMath::Sin(A);
				FVector Nk;
				const FVector P = Surf(P0 + Dk * (R0 * F), Lift, &Nk);
				const int32 Sd = SideOf(Shape.ToU(P));
				Cur.Add(B.Add(SplitPos(P, Sd), SplitNrm((Nk - Dk * (Concave * Fr)).GetSafeNormal(), Sd)));
				CurS.Add(Sd);
			}
			for (int32 k = 0; k < K; ++k)
			{
				const int32 K1 = (k + 1) % K;
				if (r == 1)
				{
					if (CurS[k] == CurS[K1]) B.Tri(C0[CurS[k]], Cur[k], Cur[K1], SplitNrm(N0, CurS[k]));
				}
				else if (CurS[k] == CurS[K1] && PrevS[k] == PrevS[K1] && CurS[k] == PrevS[k])
				{
					const FVector Out = SplitNrm(N0, CurS[k]);
					B.Tri(Prev[k], Cur[k], Cur[K1], Out);
					B.Tri(Prev[k], Cur[K1], Prev[K1], Out);
				}
			}
			Prev = Cur;
			PrevS = CurS;
		}
	};
	Patch(Scar, Rs, 14, 2, 0.45f, 0.3f, 0.f, 0.5f);
	Patch(Powder, Rc, 12, 2, 0.65f, 0.35f, 17.f, 0.3f);
	// Crushed texture: dark pits inside the powder spot, light flecks scattered around it.
	auto Speck = [&](FMeshBuf& B, float MaxD, float Lift, float SzMin, float SzMax)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI), D = MaxD * FMath::Sqrt(Rng.FRandRange(0.f, 1.f));
		const FVector Dk = T1 * FMath::Cos(A) + T2 * FMath::Sin(A);
		FVector Nk;
		const FVector C = Surf(P0 + Dk * D, Lift, &Nk);
		const int32 Sd = SideOf(Shape.ToU(C));
		FVector Ta = FVector::CrossProduct(Nk, T2).GetSafeNormal();
		if (Ta.IsNearlyZero()) Ta = T1;
		const FVector Tb = FVector::CrossProduct(Nk, Ta);
		const float Sz = Rng.FRandRange(SzMin, SzMax), B0 = Rng.FRandRange(0.f, 2.f * PI);
		FVector V[3];
		for (int32 v = 0; v < 3; ++v)
		{
			const float Bv = B0 + (float)v * 2.1f + Rng.FRandRange(-0.4f, 0.4f);
			V[v] = SplitPos(C + (Ta * FMath::Cos(Bv) + Tb * FMath::Sin(Bv)) * (Sz * Rng.FRandRange(0.6f, 1.2f)), Sd);
		}
		B.Flat(V[0], V[1], V[2], SplitNrm(Nk, Sd));
	};
	for (int32 k = 0; k < 24; ++k) Speck(Gap, Rc * 1.2f, 0.8f, 0.5f, 1.5f);
	for (int32 k = 0; k < 16; ++k) Speck(Powder, Rs * 1.7f, 0.55f, 0.5f, 1.2f);

	// 2) Main fracture rims: on both halves, a dark lip at the break and a lighter chipped band behind it, both scaled
	//    by how far the fracture gapes there (wide at the impact face, hairline at the far-side hinge).
	const float OpenMax = FMath::Max(SplitOpen, 1.f);
	const float LipK = FMath::Clamp(Radius * 0.011f, 0.8f, 1.5f), EdgeK = FMath::Clamp(Radius * 0.02f, 1.4f, 2.6f);
	for (int32 s = 0; s < 2; ++s)
	{
		for (int32 m = 0; m < 2; ++m)
		{
			const bool bOnA = (s == 0) == (m == 0); // this rim follows boundary meridian A (the impact one)
			const float InSign = m == 0 ? 1.f : -1.f; // towards the inside of half s
			const float RimOff = NoiseO + 13.f * (float)(s * 2 + m);
			const int32 GBase = Gap.V.Num(), EBase = Edge.V.Num();
			int32 Rows = 0;
			for (int32 j = 0; j < BodyRows; ++j)
			{
				const float El = RowEl(j);
				if (El < FMath::DegreesToRadians(-3.f) || El > FMath::DegreesToRadians(86.f)) continue;
				const float PhiBd = bOnA ? SplitAz + JagA[j] : SplitAz + PI + JagB[j];
				const float CosEl = FMath::Max(FMath::Cos(El), 0.2f);
				const FVector Pb0 = Shape.Point(DirAE(PhiBd, El));
				const float G01 = FMath::Clamp((float)(SplitPos(Pb0, 1) - Pb0).Size() / OpenMax, 0.f, 1.f);
				const float Wn = 0.6f + 0.8f * (0.5f + 0.5f * Fbm1(El * Radius / 9.f, RimOff));
				const float Lip = LipK * (0.3f + 1.4f * G01) * Wn * Rng.FRandRange(0.8f, 1.2f);
				const float EdgeW = EdgeK * (0.25f + 1.6f * G01 * G01) * (0.6f + 0.8f * (0.5f + 0.5f * Fbm1(El * Radius / 6.f, RimOff + 5.f))) * Rng.FRandRange(0.7f, 1.3f);
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

	// 3) Ribbons: a dark gap (walls lean in) with uneven left/right widths, optionally with lighter chipped bevels.
	auto Ribbon = [&](const TArray<FVector>& Pts, const TArray<FVector>& Nrm, const TArray<float>& Ws, int32 Sd, bool bEdges, float Lift)
	{
		const int32 Num = Pts.Num();
		const int32 G0 = Gap.V.Num(), E0 = Edge.V.Num();
		for (int32 k = 0; k < Num; ++k)
		{
			const FVector N = Nrm[k];
			const FVector Tg = (Pts[FMath::Min(k + 1, Num - 1)] - Pts[FMath::Max(k - 1, 0)]).GetSafeNormal();
			FVector B = FVector::CrossProduct(N, Tg).GetSafeNormal();
			if (B.IsNearlyZero()) B = T1;
			const float WL = Ws[k] * 0.5f * Rng.FRandRange(0.65f, 1.35f);
			const float WR = Ws[k] * 0.5f * Rng.FRandRange(0.65f, 1.35f);
			FVector NL1, NR1;
			const FVector L1 = Surf(Pts[k] - B * WL, Lift, &NL1);
			const FVector R1 = Surf(Pts[k] + B * WR, Lift, &NR1);
			Gap.Add(SplitPos(L1, Sd), SplitNrm((NL1 + B * 0.8f).GetSafeNormal(), Sd));
			Gap.Add(SplitPos(Pts[k] + N * (Lift + 0.1f), Sd), SplitNrm(N, Sd));
			Gap.Add(SplitPos(R1, Sd), SplitNrm((NR1 - B * 0.8f).GetSafeNormal(), Sd));
			if (bEdges)
			{
				const float Tip = Ws[k] > 0.f ? 1.f : 0.3f;
				const float EL = (Ws[k] * 0.45f + 0.6f) * Rng.FRandRange(0.45f, 1.6f) * Tip; // chipped, uneven edge
				const float ER = (Ws[k] * 0.45f + 0.6f) * Rng.FRandRange(0.45f, 1.6f) * Tip;
				FVector NL2, NR2;
				const FVector L2 = Surf(Pts[k] - B * (WL + EL), Lift - 0.3f, &NL2);
				const FVector R2 = Surf(Pts[k] + B * (WR + ER), Lift - 0.3f, &NR2);
				Edge.Add(SplitPos(L2, Sd), SplitNrm(NL2, Sd));
				Edge.Add(SplitPos(L1, Sd), SplitNrm((NL1 + B * 0.45f).GetSafeNormal(), Sd));
				Edge.Add(SplitPos(R1, Sd), SplitNrm((NR1 - B * 0.45f).GetSafeNormal(), Sd));
				Edge.Add(SplitPos(R2, Sd), SplitNrm(NR2, Sd));
			}
		}
		for (int32 k = 0; k + 1 < Num; ++k)
		{
			const FVector Out = SplitNrm(Nrm[k], Sd);
			const int32 g = G0 + k * 3, g2 = g + 3;
			Gap.Tri(g, g + 1, g2 + 1, Out); Gap.Tri(g, g2 + 1, g2, Out);
			Gap.Tri(g + 1, g + 2, g2 + 2, Out); Gap.Tri(g + 1, g2 + 2, g2 + 1, Out);
			if (bEdges)
			{
				const int32 e = E0 + k * 4, e2 = e + 4;
				Edge.Tri(e, e + 1, e2 + 1, Out); Edge.Tri(e, e2 + 1, e2, Out);
				Edge.Tri(e + 2, e + 3, e2 + 3, Out); Edge.Tri(e + 2, e2 + 3, e2 + 2, Out);
			}
		}
	};

	// 4) Branches and hairlines: noisy zigzag walks over the surface, stopping at the turf or at the main fracture.
	struct FSeedB { FVector P; FVector H; float W0; float Len; int32 Depth; int32 Side; bool bHair; };
	TArray<FSeedB> Queue;
	const float Wm = FMath::Clamp(Radius * 0.045f, 3.5f, 6.f);
	const float Step = FMath::Clamp(Radius * 0.06f, 4.5f, 8.f);
	FVector Tu = (FVector::UpVector - N0 * FVector::DotProduct(FVector::UpVector, N0)).GetSafeNormal();
	if (Tu.IsNearlyZero()) Tu = T2;
	const int32 NumPrim = Rng.RandRange(3, 6);
	float Ang = Rng.FRandRange(20.f, 70.f) * (Rng.FRand() < 0.5f ? 1.f : -1.f);
	for (int32 k = 0; k < NumPrim; ++k)
	{
		// Irregular spacing; keep clear of the main fracture, which leaves the impact straight up and down (0 / 180 deg).
		float A = FMath::Fmod(Ang + 540.f, 360.f) - 180.f;
		if (FMath::Abs(A) < 24.f) A = A < 0.f ? -24.f : 24.f;
		if (FMath::Abs(A) > 156.f) A = A < 0.f ? -156.f : 156.f;
		const float Len = Radius * (k == 0 ? Rng.FRandRange(0.65f, 0.95f) : Rng.FRandRange(0.28f, 0.75f));
		Queue.Add(FSeedB{ P0, Tu.RotateAngleAxis(A, N0), float(Wm * Rng.FRandRange(0.5f, 1.f) * (k == 0 ? 1.1f : 1.f)), Len, 0, -1, false });
		Ang += Rng.FRandRange(45.f, 110.f);
	}
	const int32 NumHair = 8 + Rng.RandRange(0, 5);
	for (int32 k = 0; k < NumHair; ++k)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const FVector Dk = T1 * FMath::Cos(A) + T2 * FMath::Sin(A);
		const FVector Start = P0 + Dk * (Rc * Rng.FRandRange(0.6f, 2.2f));
		Queue.Add(FSeedB{ Start, Dk.RotateAngleAxis(Rng.FRandRange(-70.f, 70.f), N0), float(Rng.FRandRange(0.35f, 0.8f)), float(Rng.FRandRange(5.f, 20.f)), 2, -1, true });
	}
	int32 Budget = 420;
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
		const float WOff = Rng.FRandRange(0.f, 100.f);
		Pts.Add(P); Nrm.Add(N); Ws.Add(S.W0);
		while (Done < S.Len && Pts.Num() < 48 && Budget > 0)
		{
			const float St = S.bHair ? Rng.FRandRange(1.8f, 3.2f) : Step * Rng.FRandRange(0.45f, 1.5f) * (S.Depth > 0 ? 0.85f : 1.f);
			float Turn = 14.f * FMath::PerlinNoise1D(Done / 15.f + WOff); // slow meander
			if (Rng.FRand() < 0.55f)
			{
				Turn += Zig * Rng.FRandRange(10.f, S.bHair ? 50.f : 42.f); // a kink; mostly alternating, not always
				if (Rng.FRand() < 0.62f) Zig = -Zig;
			}
			else Turn += Rng.FRandRange(-7.f, 7.f);
			const FVector Hs = H.RotateAngleAxis(Turn, N);
			FVector N2;
			const FVector P2 = Surf(P + Hs * St, 0.f, &N2);
			if (P2.Z < FloorLocal) break;
			const int32 S2 = SideOf(Shape.ToU(P2));
			if (Sd < 0) Sd = S2;
			else if (S2 != Sd) break; // cracks end at the main fracture
			Done += St;
			float W = S.W0 * FMath::Pow(FMath::Max(0.f, 1.f - Done / S.Len), 0.8f) * (0.55f + 0.9f * (0.5f + 0.5f * Fbm1(Done / 8.f, WOff)));
			if (!S.bHair && Rng.FRand() < 0.1f) W *= 0.4f; // pinch
			H = H.RotateAngleAxis(Rng.FRandRange(-6.f, 6.f), N2);
			H = (H - N2 * FVector::DotProduct(H, N2)).GetSafeNormal();
			if (H.IsNearlyZero()) break;
			Pts.Add(P2); Nrm.Add(N2); Ws.Add(W);
			--Budget;
			if (!S.bHair && S.Depth < 2 && Pts.Num() > 2 && Done < S.Len * 0.8f && Rng.FRand() < 0.24f)
			{
				const float Fa = (Rng.FRand() < 0.5f ? 1.f : -1.f) * Rng.FRandRange(22.f, 68.f);
				Queue.Add(FSeedB{ P2, H.RotateAngleAxis(Fa, N2), float(W * Rng.FRandRange(0.5f, 0.8f)), float((S.Len - Done) * Rng.FRandRange(0.4f, 0.8f) + St), S.Depth + 1, Sd, false });
			}
			P = P2; N = N2;
		}
		if (Pts.Num() < 2 || Sd < 0) continue;
		Ws.Last() = 0.f;
		if (Ws.Num() > 2) Ws[Ws.Num() - 2] *= 0.6f;
		Ribbon(Pts, Nrm, Ws, Sd, !S.bHair, S.bHair ? 0.8f : 1.f);
	}

	if (CrackMesh)
	{
		Upload(CrackMesh, 0, Gap);
		Upload(CrackMesh, 1, Edge);
		Upload(CrackMesh, 2, Scar);
		Upload(CrackMesh, 3, Powder);
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
	bDamagedStage = false;
	if (Body && MatIntact) Body->SetMaterial(0, MatIntact);
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
	if (RuneMesh) RuneMesh->SetVisibility(true);
	if (Side) Side->SetVisibility(true);
	if (CrackMesh) CrackMesh->SetVisibility(false);
	if (Rubble) Rubble->SetVisibility(false);
	HideAllDebris();
	if (bTestCamOn) SetTestCam(false);
	TestUntil = -1.f;
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
	const float Tough = ZoogiUtil::BreakToughness(); // Oct 8: [ZoogiRoll.Arena] BreakToughness scales both speeds
	const float CrackV = CrackSpeed * Tough, ShatterV = ShatterSpeed * Tough;
	if (Vn < CrackV) return;
	LastHitTime = Now;
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	// Strong hit: the active shooter while Awakened (infused), or crit-grade contact (>= 90% of its launch speed, the crit rule).
	bool bStrong = false;
	if (G && Ball->PlayerIndex == G->ActivePlayer && G->Phase == EZoogiPhase::Rolling)
	{
		const bool bAwakened = Ball->Artifact && Ball->Artifact->Infused != EZoogiArtifact::None;
		// Crit-grade = the crit rule (>= 90% of its launch speed at contact, <= 5 deg cut) on a hard hit; a soft straight tap only cracks.
		const bool bCritGrade = G->GetShotSpeed() > 1.f && V.Size2D() >= 0.9f * G->GetShotSpeed() && CutDeg <= 5.f && Vn >= ShatterV;
		bStrong = bAwakened || bCritGrade;
	}
	FVector Dir = -NormalImpulse.GetSafeNormal2D(); // impulse pushes the ball away from the rock: debris flies the other way
	if (Dir.IsNearlyZero()) Dir = (GetActorLocation() - Ball->BallLocation()).GetSafeNormal2D();
	UE_LOG(LogZoogi, Log, TEXT("Rock %s hit by P%d %s at %.0f cm/s normal (%.0f cm/s, cut %.0f deg)%s (state %d)"), *GetName(), Ball->PlayerIndex + 1, *Ball->Def.Name, Vn, V.Size2D(), CutDeg, bStrong ? TEXT(" STRONG") : TEXT(""), (int32)State);
	if (bStrong || (State == EZoogiRockState::Cracked && Vn >= ShatterV)) Shatter(ImpactPoint, Dir, Ball);
	else if (State == EZoogiRockState::Intact && bFourStage && !bDamagedStage) MarkDamaged(ImpactPoint);
	else if (State == EZoogiRockState::Intact) Crack(ImpactPoint, Dir, Ball);
}

void AZoogiRock::MarkDamaged(const FVector& P)
{
	bDamagedStage = true;
	if (Body && MatCracked) Body->SetMaterial(0, MatCracked);
	AZoogiFX::Spawn(GetWorld(), P, FLinearColor(0.75f, 0.92f, 1.f), FMath::Max(80.f, Radius * 0.9f), 0.35f, false, 4.f);
	ZoogiUtil::PlaySfx(this, TEXT("Clack"), 0.6f, 1.4f);
	UE_LOG(LogZoogi, Log, TEXT("Rock %s: INTACT -> DAMAGED (four-stage)"), *GetName());
}

void AZoogiRock::Escalate(const FVector& P, const FVector& Dir)
{
	const FVector D = Dir.IsNearlyZero() ? (GetActorLocation() - P).GetSafeNormal2D() : Dir;
	if (State == EZoogiRockState::Shattered) return;
	if (State == EZoogiRockState::Cracked) { Shatter(P, D, nullptr); return; }
	if (bFourStage && !bDamagedStage) { MarkDamaged(P); return; }
	Crack(P, D, nullptr);
}

void AZoogiRock::ForceStage(int32 Stage)
{
	ResetRock();
	const FVector P = GetActorLocation() + FVector(-Radius, 0.f, Height * 0.5f);
	const FVector D(1.f, 0.f, 0.f);
	if (Stage >= 1 && bFourStage) MarkDamaged(P);
	if (Stage >= 2) Crack(P, D, nullptr);
	if (Stage >= 3) Shatter(P, D, nullptr);
}

void AZoogiRock::Crack(const FVector& P, const FVector& Dir, AZoogiBall* By)
{
	State = EZoogiRockState::Cracked;
	FRandomStream Rng(Seed * 7919 + 17);

	// Geometry: dent where the flake pops off, split the boulder along a jagged fracture through the impact point
	// (gaping at the impact face), then lay the crushed spot, scar, fracture rims, branches and hairlines.
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
	if (RuneMesh) RuneMesh->SetVisibility(false); // the runes go dark once the stone cracks (damage cue)

	// Debris off the impact face (actor space): the flake, a few pebbles, stone grit and a grey-brown dust burst.
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
		// Dense burst off the face (opaque at first, billows, drifts, thins out over ~2 s) plus low puffs at the foot.
		const float K = FMath::Clamp(Radius / 110.f, 0.75f, 1.3f);
		for (int32 k = 0; k < 8; ++k)
		{
			SpawnPuff(PA + NA * Rng.FRandRange(4.f, 14.f) + TA * (Rng.FRandRange(-0.6f, 0.6f) * Rs) + FVector(0.f, 0.f, Rng.FRandRange(-6.f, 10.f)),
				NA * Rng.FRandRange(70.f, 170.f) + TA * Rng.FRandRange(-60.f, 60.f) + FVector(0.f, 0.f, Rng.FRandRange(15.f, 60.f)),
				Rng.FRandRange(12.f, 20.f) * K, Rng.FRandRange(55.f, 95.f) * K, Rng.FRandRange(1.8f, 2.6f), Rng.FRandRange(1.1f, 1.45f), 2.2f, 7.f, 1.f,
				DustCol * Rng.FRandRange(0.92f, 1.08f));
		}
		const FVector NH = FVector(NA.X, NA.Y, 0.f).GetSafeNormal();
		const FVector Foot(PA.X, PA.Y, -(float)GetActorLocation().Z);
		for (int32 k = 0; k < 3; ++k)
		{
			SpawnPuff(Foot + NH * Rng.FRandRange(15.f, 35.f) + TA * Rng.FRandRange(-25.f, 25.f) + FVector(0.f, 0.f, 10.f * K),
				NH * Rng.FRandRange(80.f, 160.f) + TA * Rng.FRandRange(-70.f, 70.f),
				18.f * K, Rng.FRandRange(70.f, 110.f) * K, Rng.FRandRange(2.f, 2.6f), Rng.FRandRange(0.9f, 1.2f), 1.6f, 2.f, 0.5f,
				DustCol * Rng.FRandRange(0.85f, 0.97f));
		}
	}
	else
	{
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), GetActorTransform().TransformPosition(PA), FRotator::ZeroRotator, 0.35f);
	}
	QuietSparks(By); // visual only: the stone grit and dust read instead of the shooter's bright speed sparks

	ZoogiUtil::PlaySfx(this, TEXT("RockCrack"), 0.9f, FMath::FRandRange(0.95f, 1.05f));
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		G->OnRockEvent(this, false, By, GetActorLocation() + FVector(0, 0, Height + 90.f));
	}
	StartTestCam(false, Dir);
	UE_LOG(LogZoogi, Log, TEXT("Rock %s CRACKED"), *GetName());
}

void AZoogiRock::Shatter(const FVector& P, const FVector& Dir, AZoogiBall* By)
{
	State = EZoogiRockState::Shattered;
	Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the lane opens; nothing left to trap a Zoogi
	if (Body) Body->SetVisibility(false);
	if (RuneMesh) RuneMesh->SetVisibility(false);
	if (Side) Side->SetVisibility(false);
	if (CrackMesh) CrackMesh->SetVisibility(false);
	if (Rubble) Rubble->SetVisibility(true);
	FRandomStream Rng(Seed * 7919 + 29);
	FVector DirA = GetActorTransform().InverseTransformVectorNoScale(Dir).GetSafeNormal2D();
	if (DirA.IsNearlyZero()) DirA = FVector::ForwardVector;

	// Chunky shards of mixed size burst from inside the boulder: outwards, along the hit and up; big ones slower.
	const int32 NumMain = FMath::Max(0, Chunks.Num() - NumCrackPieces - NumSmallShards);
	for (int32 k = 0; k < NumMain; ++k)
	{
		const int32 i = NumCrackPieces + k;
		const float Az = 2.f * PI * (float)k / (float)FMath::Max(1, NumMain) + Rng.FRandRange(-0.35f, 0.35f);
		const float El = FMath::DegreesToRadians(Rng.FRandRange(12.f, 62.f));
		const FVector Pos = RockToActor(Shape.Point(DirAE(Az, El)) * Rng.FRandRange(0.4f, 0.72f));
		const FVector Rad = FVector(Pos.X, Pos.Y, 0.f).GetSafeNormal();
		const float Kv = FMath::Clamp(1.f / FMath::Sqrt(FMath::Max(Chunks[i].Size / (Radius * 0.3f), 0.1f)), 0.8f, 1.25f);
		const FVector Vel = (Rad * Rng.FRandRange(90.f, 220.f) + DirA * Rng.FRandRange(120.f, 330.f)) * Kv + FVector(0.f, 0.f, Rng.FRandRange(170.f, 380.f) * Kv);
		LaunchChunk(i, Pos, Vel, Rng.VRand() * (Rng.FRandRange(4.f, 12.f) * Kv), Chunks[i].Size <= Radius * 0.17f);
	}
	// Many small pieces: faster, wider spread; they stay as rubble when they settle near the base.
	for (int32 k = 0; k < NumSmallShards; ++k)
	{
		const int32 i = NumCrackPieces + NumMain + k;
		if (!Chunks.IsValidIndex(i)) break;
		const float Az = Rng.FRandRange(0.f, 2.f * PI);
		const float El = FMath::DegreesToRadians(Rng.FRandRange(5.f, 70.f));
		const FVector Pos = RockToActor(Shape.Point(DirAE(Az, El)) * Rng.FRandRange(0.45f, 0.85f));
		const FVector Rad = FVector(Pos.X, Pos.Y, 0.f).GetSafeNormal();
		const FVector Vel = (Rad * Rng.FRandRange(120.f, 320.f) + DirA * Rng.FRandRange(100.f, 360.f)) * 1.15f + FVector(0.f, 0.f, Rng.FRandRange(150.f, 420.f));
		LaunchChunk(i, Pos, Vel, Rng.VRand() * Rng.FRandRange(8.f, 20.f), true);
	}
	SpawnGrit(CrackGritPerSet, GritPerSet - CrackGritPerSet, RockToActor(FVector(0.f, 0.f, Height * 0.45f)), Radius * 0.45f,
		(DirA * 0.6f + FVector::UpVector).GetSafeNormal(), 70.f, 220.f, 700.f, Rng);

	if (bHasDust)
	{
		// Heavy cloud: ten burst puffs that billow out, hang and sink, plus a low ground-hugging ring rolling outwards.
		for (int32 k = 0; k < 10; ++k)
		{
			const float Az = 2.f * PI * (float)k / 10.f + Rng.FRandRange(-0.3f, 0.3f);
			const FVector Rad(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			SpawnPuff(Rad * (Radius * Rng.FRandRange(0.1f, 0.5f)) + FVector(0.f, 0.f, Height * Rng.FRandRange(0.25f, 0.75f)),
				Rad * Rng.FRandRange(120.f, 260.f) + DirA * Rng.FRandRange(40.f, 140.f) + FVector(0.f, 0.f, Rng.FRandRange(40.f, 150.f)),
				Radius * Rng.FRandRange(0.3f, 0.45f), Radius * Rng.FRandRange(1.1f, 1.7f), Rng.FRandRange(2.f, 2.6f), Rng.FRandRange(1.2f, 1.5f), 1.6f, 12.f, 1.f,
				DustCol * Rng.FRandRange(0.9f, 1.1f));
		}
		for (int32 k = 0; k < 10; ++k)
		{
			const float Az = 2.f * PI * (float)k / 10.f + PI * 0.1f + Rng.FRandRange(-0.25f, 0.25f);
			const FVector Rad(FMath::Cos(Az), FMath::Sin(Az), 0.f);
			SpawnPuff(Rad * (Radius * 0.75f) + FVector(0.f, 0.f, Radius * 0.1f), Rad * Rng.FRandRange(220.f, 360.f) + DirA * Rng.FRandRange(20.f, 80.f),
				Radius * 0.35f, Radius * Rng.FRandRange(1.f, 1.4f), Rng.FRandRange(2.f, 2.6f), Rng.FRandRange(1.f, 1.3f), 1.4f, 2.f, 0.4f,
				DustCol * Rng.FRandRange(0.82f, 0.95f));
		}
	}
	else
	{
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), GetActorLocation() + FVector(0, 0, 10.f), FRotator::ZeroRotator, FMath::Clamp(Radius / 100.f, 0.8f, 1.6f));
	}
	QuietSparks(By); // visual only, see Crack()

	ZoogiUtil::PlaySfx(this, TEXT("RockShatter"), 1.f, FMath::FRandRange(0.95f, 1.05f));
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		G->OnRockEvent(this, true, By, GetActorLocation() + FVector(0, 0, Height + 110.f));
	}
	// Small emphasis only: the tactical camera gets a light kick, no hit-stop.
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(0.35f);
	StartTestCam(true, Dir);
	UE_LOG(LogZoogi, Log, TEXT("Rock %s SHATTERED (%d chunks)"), *GetName(), ChunkCount);
}

void AZoogiRock::QuietSparks(AZoogiBall* By)
{
	UWorld* W = GetWorld();
	if (!W) return;
	const FVector C = GetActorLocation();
	const double R2 = FMath::Square((double)Radius + 300.0);
	// Balls at the rock (and the shooter): no new speed sparks for 1.2 s (AZoogiBall::Tick waits for SparkClock > ~0.05-0.11 s).
	for (TActorIterator<AZoogiBall> It(W); It; ++It)
	{
		AZoogiBall* B = *It;
		if (B && (B == By || FVector::DistSquared2D(B->BallLocation(), C) < R2)) B->SparkClock = FMath::Min(B->SparkClock, -1.2f);
	}
	// Speed sparks already in the air around the rock (shed on the approach, ~0.6 s life) are cut at once.
	static const FName SparkNames[4] = { FName(TEXT("NS_Burst_Blue")), FName(TEXT("NS_Burst_Red")), FName(TEXT("NS_Burst_Yellow")), FName(TEXT("NS_Burst_Green")) };
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* NC = *It;
		if (!NC || NC->GetWorld() != W || !NC->IsActive()) continue;
		const UNiagaraSystem* Sys = NC->GetAsset();
		if (!Sys) continue;
		const FName SysName = Sys->GetFName();
		if (SysName != SparkNames[0] && SysName != SparkNames[1] && SysName != SparkNames[2] && SysName != SparkNames[3]) continue;
		if (FVector::DistSquared2D(NC->GetComponentLocation(), C) < R2) NC->DeactivateImmediate();
	}
}

// ---------------------------------------------------------------------------------------------------------------- test framing

void AZoogiRock::StartTestCam(bool bShattered, const FVector& WorldDir)
{
	static const bool bRockTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRockTest"));
	if (!bRockTest) return; // normal play: never touches the camera
	TestDir = FVector(WorldDir.X, WorldDir.Y, 0.f).GetSafeNormal();
	if (TestDir.IsNearlyZero()) TestDir = FVector::ForwardVector;
	bTestShatter = bShattered;
	TestClock = 0.f;
	// GameMode hooks: crack_after at 0.9 s; shatter_after at 1.1 s and rubble at 3.4 s. The 0.1 s shots keep the game camera.
	TestFrom = bShattered ? 0.85f : 0.65f;
	TestUntil = bShattered ? 3.65f : 1.15f;
	SetActorTickEnabled(true);
}

bool AZoogiRock::TickTestCam(float RawDt)
{
	if (TestUntil < 0.f) return false;
	TestClock += RawDt;
	if (TestClock >= TestUntil)
	{
		if (bTestCamOn) SetTestCam(false);
		TestUntil = -1.f;
		return false;
	}
	if (!bTestCamOn && TestClock >= TestFrom) SetTestCam(true);
	return true;
}

void AZoogiRock::SetTestCam(bool bOn)
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		bTestCamOn = false;
		return;
	}
	if (!bOn)
	{
		if (TestCam && PC->GetViewTarget() == TestCam.Get() && TestPrevView.IsValid()) PC->SetViewTarget(TestPrevView.Get());
		bTestCamOn = false;
		return;
	}
	if (!TestCam)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TestCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP);
		if (!TestCam) return;
	}
	AActor* Prev = PC->GetViewTarget();
	if (Prev == TestCam.Get()) return;
	TestPrevView = Prev;
	UCameraComponent* CC = TestCam->GetCameraComponent();
	if (ACameraActor* PrevCam = Cast<ACameraActor>(Prev))
	{
		CC->PostProcessSettings = PrevCam->GetCameraComponent()->PostProcessSettings; // same look as the game camera
		CC->PostProcessBlendWeight = PrevCam->GetCameraComponent()->PostProcessBlendWeight;
	}
	CC->bConstrainAspectRatio = false;
	// From the side the ball came from (looking into the gaping fracture), turned 25 deg, high enough for the debris.
	const FVector Rock = GetActorLocation();
	const FVector Look = Rock + (bTestShatter ? TestDir * (Radius * 0.8f) + FVector(0.f, 0.f, 10.f) : FVector(0.f, 0.f, Height * 0.45f));
	const FVector From = TestDir.RotateAngleAxis(25.f, FVector::UpVector);
	const float Dist = bTestShatter ? Radius * 3.f + 480.f : Radius * 2.6f + 260.f;
	const float Lift = bTestShatter ? Radius * 2.f + 400.f : Radius * 1.5f + 200.f;
	const FVector Loc = Rock - From * Dist + FVector(0.f, 0.f, Lift);
	CC->SetFieldOfView(bTestShatter ? 65.f : 55.f);
	TestCam->SetActorLocationAndRotation(Loc, (Look - Loc).Rotation());
	PC->SetViewTarget(TestCam.Get());
	bTestCamOn = true;
	UE_LOG(LogZoogi, Log, TEXT("Rock %s: -ZoogiRockTest camera on (%s)"), *GetName(), bTestShatter ? TEXT("shatter") : TEXT("crack"));
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
	const FVector Wind(FMath::Cos(WindA) * 22.f, FMath::Sin(WindA) * 22.f, 0.f); // gentle, seeded drift
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
		const float Grow = 0.7f * (1.f - FMath::Exp(-3.2f * F.Age)) + 0.3f * T; // fast billow, then a slow spread
		const float R = FMath::Lerp(F.R0, F.R1, Grow);
		F.Pos.Z = FMath::Max((float)F.Pos.Z, FloorZ + R * F.Flat * 0.35f);
		// Dense at once, hangs, then thins out over the last ~65% of its life as it spreads and settles.
		const float Op = F.Peak * FMath::Clamp(F.Age / 0.06f, 0.f, 1.f) * (1.f - FMath::SmoothStep(0.35f, 1.f, T)) * FMath::Lerp(1.f, 0.75f, Grow);
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
	const bool bCam = TickTestCam(DeltaSeconds); // -ZoogiRockTest only (same clock as the GameMode's delayed shots)
	if (!bChunks && !bGrit && !bPuffs && !bCam) SetActorTickEnabled(false);
}
