#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * Small procedural mesh builder shared by the Frozen Ring pieces (Oct 6). Flat-shaded, stylised (large clean facets).
 * Same winding convention as the arena kit: a triangle's front faces the "Out" hint.
 */
struct ZOOGIROLL_API FZoogiMeshBuilder
{
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	TArray<FProcMeshTangent> Tan;

	bool IsEmpty() const { return T.Num() == 0; }
	int32 Add(const FVector& P, const FVector& Nrm, const FVector2D& Uv);
	void Tri(int32 A, int32 B, int32 Cc, const FVector& Out);
	static FVector2D Planar(const FVector& P, const FVector& Nrm);
	void FlatTri(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& Out);
	void Quad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Out);
	/** Oriented box (center, half extents, rotation). */
	void Box(const FVector& Center, const FVector& Half, const FRotator& Rot = FRotator::ZeroRotator);
	/** Vertical prism with N sides between Z0 and Z1; radius can taper (R1 at the top); optional top facet "Peak" height. */
	void Prism(const FVector2D& XY, int32 Sides, float R0, float R1, float Z0, float Z1, float YawDeg = 0.f, float Peak = 0.f, bool bBottom = false);
	/** Curved slab: annulus sector RIn..ROut, angles A0..A1 (deg), Z0..Z1, with Segs segments. Top can bulge (snow cap). */
	void ArcSlab(float RIn, float ROut, float A0, float A1, float Z0, float Z1, int32 Segs, float TopBulge = 0.f);
	/** Jagged chunk: a randomised convex-ish blob (stylised ice block / rock), Size half extents. */
	void Chunk(const FVector& Center, const FVector& Half, FRandomStream& Rng, float Jag = 0.25f, float YawDeg = 0.f);
	void Upload(UProceduralMeshComponent* M, int32 Section, bool bCollision = false) const;
};
