#include "ZoogiMeshKit.h"

int32 FZoogiMeshBuilder::Add(const FVector& P, const FVector& Nrm, const FVector2D& Uv)
{
	const int32 I = V.Add(P);
	N.Add(Nrm);
	UV.Add(Uv);
	C.Add(FLinearColor::White);
	FVector Tx = FVector::CrossProduct(Nrm, FVector::UpVector);
	if (Tx.SizeSquared() < 1e-4) Tx = FVector::CrossProduct(Nrm, FVector::ForwardVector);
	Tan.Add(FProcMeshTangent(Tx.GetSafeNormal(), false));
	return I;
}

void FZoogiMeshBuilder::Tri(int32 A, int32 B, int32 Cc, const FVector& Out)
{
	const FVector Fn = FVector::CrossProduct(V[Cc] - V[A], V[B] - V[A]);
	if (FVector::DotProduct(Fn, Out) >= 0.0) { T.Add(A); T.Add(B); T.Add(Cc); }
	else { T.Add(A); T.Add(Cc); T.Add(B); }
}

FVector2D FZoogiMeshBuilder::Planar(const FVector& P, const FVector& Nrm)
{
	const FVector A = Nrm.GetAbs();
	if (A.Z >= A.X && A.Z >= A.Y) return FVector2D(P.X, P.Y) * 0.01f;
	if (A.X >= A.Y) return FVector2D(P.Y, P.Z) * 0.01f;
	return FVector2D(P.X, P.Z) * 0.01f;
}

void FZoogiMeshBuilder::FlatTri(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& Out)
{
	FVector Fn = FVector::CrossProduct(P1 - P0, P2 - P0);
	const double L = Fn.Size();
	if (L < 1e-3) return;
	Fn /= L;
	if (FVector::DotProduct(Fn, Out) < 0.0) Fn = -Fn;
	const int32 A = Add(P0, Fn, Planar(P0, Fn)), B = Add(P1, Fn, Planar(P1, Fn)), Cc = Add(P2, Fn, Planar(P2, Fn));
	Tri(A, B, Cc, Fn);
}

void FZoogiMeshBuilder::Quad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Out)
{
	FlatTri(P0, P1, P2, Out);
	FlatTri(P0, P2, P3, Out);
}

void FZoogiMeshBuilder::Box(const FVector& Center, const FVector& Half, const FRotator& Rot)
{
	const FQuat Q = Rot.Quaternion();
	auto P = [&](float X, float Y, float Z) { return Center + Q.RotateVector(FVector(X * Half.X, Y * Half.Y, Z * Half.Z)); };
	const FVector Ax = Q.RotateVector(FVector::ForwardVector), Ay = Q.RotateVector(FVector::RightVector), Az = Q.RotateVector(FVector::UpVector);
	Quad(P(1, -1, -1), P(1, 1, -1), P(1, 1, 1), P(1, -1, 1), Ax);
	Quad(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), -Ax);
	Quad(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), Ay);
	Quad(P(-1, -1, -1), P(1, -1, -1), P(1, -1, 1), P(-1, -1, 1), -Ay);
	Quad(P(-1, -1, 1), P(1, -1, 1), P(1, 1, 1), P(-1, 1, 1), Az);
	Quad(P(-1, -1, -1), P(1, -1, -1), P(1, 1, -1), P(-1, 1, -1), -Az);
}

void FZoogiMeshBuilder::Prism(const FVector2D& XY, int32 Sides, float R0, float R1, float Z0, float Z1, float YawDeg, float Peak, bool bBottom)
{
	const FVector C0(XY.X, XY.Y, Z0), C1(XY.X, XY.Y, Z1 + Peak);
	for (int32 k = 0; k < Sides; ++k)
	{
		const float A0 = FMath::DegreesToRadians(YawDeg) + 2.f * PI * k / Sides, A1 = FMath::DegreesToRadians(YawDeg) + 2.f * PI * (k + 1) / Sides;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
		const FVector B0 = FVector(XY, Z0) + D0 * R0, B1 = FVector(XY, Z0) + D1 * R0;
		const FVector T0 = FVector(XY, Z1) + D0 * R1, T1 = FVector(XY, Z1) + D1 * R1;
		const FVector Out = (D0 + D1).GetSafeNormal();
		Quad(B0, B1, T1, T0, Out);
		if (R1 > 0.5f) FlatTri(T0, T1, C1, FVector::UpVector + Out * 0.2f);
		if (bBottom) FlatTri(B0, B1, C0, -FVector::UpVector);
	}
}

void FZoogiMeshBuilder::ArcSlab(float RIn, float ROut, float A0, float A1, float Z0, float Z1, int32 Segs, float TopBulge)
{
	auto P = [](float R, float Ad, float Z) { const float A = FMath::DegreesToRadians(Ad); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); };
	const float RM = (RIn + ROut) * 0.5f;
	for (int32 s = 0; s < Segs; ++s)
	{
		const float Sa = FMath::Lerp(A0, A1, (float)s / Segs), Sb = FMath::Lerp(A0, A1, (float)(s + 1) / Segs), Sm = (Sa + Sb) * 0.5f;
		const FVector Out = P(1.f, Sm, 0.f);
		Quad(P(ROut, Sa, Z0), P(ROut, Sb, Z0), P(ROut, Sb, Z1), P(ROut, Sa, Z1), Out);
		Quad(P(RIn, Sa, Z0), P(RIn, Sb, Z0), P(RIn, Sb, Z1), P(RIn, Sa, Z1), -Out);
		if (TopBulge > 0.f)
		{
			Quad(P(RIn, Sa, Z1), P(RIn, Sb, Z1), P(RM, Sb, Z1 + TopBulge), P(RM, Sa, Z1 + TopBulge), FVector::UpVector - Out);
			Quad(P(RM, Sa, Z1 + TopBulge), P(RM, Sb, Z1 + TopBulge), P(ROut, Sb, Z1), P(ROut, Sa, Z1), FVector::UpVector + Out);
		}
		else Quad(P(RIn, Sa, Z1), P(RIn, Sb, Z1), P(ROut, Sb, Z1), P(ROut, Sa, Z1), FVector::UpVector);
	}
	for (int32 e = 0; e < 2; ++e)
	{
		const float A = e ? A1 : A0;
		const FVector Tn = FVector::CrossProduct(FVector::UpVector, P(1.f, A, 0.f)) * (e ? 1.f : -1.f);
		Quad(P(RIn, A, Z0), P(ROut, A, Z0), P(ROut, A, Z1), P(RIn, A, Z1), Tn);
		if (TopBulge > 0.f) FlatTri(P(RIn, A, Z1), P(ROut, A, Z1), P(RM, A, Z1 + TopBulge), Tn);
	}
}

void FZoogiMeshBuilder::Chunk(const FVector& Center, const FVector& Half, FRandomStream& Rng, float Jag, float YawDeg)
{
	// Octahedron-ish blob: 6 axis points + 8 corner points pulled in, triangulated as a fixed convex hull pattern.
	const FQuat Q = FRotator(0.f, YawDeg, 0.f).Quaternion();
	auto J = [&](float S) { return S * (1.f + Rng.FRandRange(-Jag, Jag)); };
	TArray<FVector> P;
	const FVector Dirs[14] = {
		FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, 1), FVector(0, 0, -1),
		FVector(1, 1, 1), FVector(-1, 1, 1), FVector(-1, -1, 1), FVector(1, -1, 1), FVector(1, 1, -1), FVector(-1, 1, -1), FVector(-1, -1, -1), FVector(1, -1, -1) };
	for (int32 i = 0; i < 14; ++i)
	{
		const float K = i < 6 ? 1.f : 0.72f;
		P.Add(Center + Q.RotateVector(FVector(J(Dirs[i].X * Half.X * K), J(Dirs[i].Y * Half.Y * K), Dirs[i].Z * Half.Z * K * (1.f + Rng.FRandRange(-Jag, Jag) * 0.5f))));
	}
	// Each axis point fans to its 4 neighbouring corners.
	const int32 Fan[6][4] = { {6, 9, 13, 10}, {7, 11, 12, 8}, {6, 10, 11, 7}, {9, 8, 12, 13}, {6, 7, 8, 9}, {10, 13, 12, 11} };
	for (int32 a = 0; a < 6; ++a)
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector& A = P[a];
			const FVector& B = P[Fan[a][k]];
			const FVector& Cc = P[Fan[a][(k + 1) % 4]];
			FlatTri(A, B, Cc, (A + B + Cc) / 3.f - Center);
		}
	// Corner-to-corner faces between adjacent axis fans (fills the 8 gaps on the edges).
	const int32 Edge[12][4] = {
		{0, 2, 6, 10}, {2, 1, 7, 11}, {1, 3, 8, 12}, {3, 0, 9, 13},
		{0, 4, 6, 9}, {4, 1, 7, 8}, {2, 4, 6, 7}, {4, 3, 8, 9},
		{0, 5, 10, 13}, {5, 1, 11, 12}, {2, 5, 10, 11}, {5, 3, 12, 13} };
	(void)Edge; // the fan already closes the hull for this point layout
}

void FZoogiMeshBuilder::Upload(UProceduralMeshComponent* M, int32 Section, bool bCollision) const
{
	if (!M || T.Num() == 0) return;
	const TArray<FVector2D> NoUV;
	M->CreateMeshSection_LinearColor(Section, V, T, N, UV, NoUV, NoUV, NoUV, C, Tan, bCollision, false);
}
