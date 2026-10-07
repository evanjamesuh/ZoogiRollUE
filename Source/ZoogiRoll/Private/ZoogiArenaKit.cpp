// Buzzy arena kit (Oct 6 re-skin): shared, code-built dressing for the Buzzy-style arenas.
// Spec: buzzy-arenas/README.md (K1 floor material, K3 hub core, K4 pillars (visual), K6 recovery gate (visual),
// K11 panorama, K12 stands / pagodas / waterfalls / terrace). Everything here is visual only: no colliders that touch
// balls or orbs, so turn order, physics and scoring are unchanged. Stands can block the camera probe channel only.
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	struct FKitMesh
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tan;

		int32 Add(const FVector& P, const FVector& Nrm, const FVector2D& Uv)
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
		/** Triangle wound so its front faces Out (same convention as AZoogiRock). */
		void Tri(int32 A, int32 B, int32 Cc, const FVector& Out)
		{
			const FVector Fn = FVector::CrossProduct(V[Cc] - V[A], V[B] - V[A]);
			if (FVector::DotProduct(Fn, Out) >= 0.0) { T.Add(A); T.Add(B); T.Add(Cc); }
			else { T.Add(A); T.Add(Cc); T.Add(B); }
		}
		static FVector2D Planar(const FVector& P, const FVector& Nrm)
		{
			const FVector A = Nrm.GetAbs();
			if (A.Z >= A.X && A.Z >= A.Y) return FVector2D(P.X, P.Y) * 0.01f;
			if (A.X >= A.Y) return FVector2D(P.Y, P.Z) * 0.01f;
			return FVector2D(P.X, P.Z) * 0.01f;
		}
		/** Flat-shaded quad P0..P3 (any winding), front towards Out. Degenerate halves are skipped. */
		void Quad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Out)
		{
			FlatTri(P0, P1, P2, Out);
			FlatTri(P0, P2, P3, Out);
		}
		void FlatTri(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& Out)
		{
			FVector Fn = FVector::CrossProduct(P1 - P0, P2 - P0);
			const double L = Fn.Size();
			if (L < 1e-3) return;
			Fn /= L;
			if (FVector::DotProduct(Fn, Out) < 0.0) Fn = -Fn;
			const int32 A = Add(P0, Fn, Planar(P0, Fn)), B = Add(P1, Fn, Planar(P1, Fn)), Cc = Add(P2, Fn, Planar(P2, Fn));
			Tri(A, B, Cc, Fn);
		}
		void Upload(UProceduralMeshComponent* M, int32 Section, bool bCollision) const
		{
			if (!M || T.Num() == 0) return;
			const TArray<FVector2D> NoUV;
			M->CreateMeshSection_LinearColor(Section, V, T, N, UV, NoUV, NoUV, NoUV, C, Tan, bCollision, false);
		}
	};

	UTexture* LoadArenaTex(const FString& Name)
	{
		return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	UMaterialInterface* LoadArenaMat(const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	FVector Polar(float R, float AngleRad, float Z) { return FVector(FMath::Cos(AngleRad) * R, FMath::Sin(AngleRad) * R, Z); }
}

UMaterialInterface* AZoogiArena::KitFloorMaterial(const TCHAR* Key, const FLinearColor& GlowColor, float GlowStrength, float BaseBright)
{
	UMaterialInterface* M = LoadArenaMat(TEXT("M_ZoogiArenaFloor"));
	UTexture* Base = LoadArenaTex(FString::Printf(TEXT("T_%sFloor"), Key));
	UTexture* Glow = LoadArenaTex(FString::Printf(TEXT("T_%sGlow"), Key));
	UTexture* Detail = LoadArenaTex(TEXT("T_StoneDetail"));
	if (!M || !Base || !Glow)
	{
		UE_LOG(LogZoogi, Warning, TEXT("Arena kit: floor assets for %s missing (run Scripts/arenas/make_arena_kit.py); using the old floor"), Key);
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
	MID->SetTextureParameterValue(TEXT("Base"), Base);
	MID->SetTextureParameterValue(TEXT("Glow"), Glow);
	if (Detail) MID->SetTextureParameterValue(TEXT("Detail"), Detail);
	MID->SetScalarParameterValue(TEXT("Radius"), ZoogiConst::KnockoffRadius * BuildScale); // r19: world radius at the per-map scale
	MID->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
	MID->SetScalarParameterValue(TEXT("GlowStrength"), GlowStrength);
	MID->SetScalarParameterValue(TEXT("BaseBright"), BaseBright);
	return MID;
}

bool AZoogiArena::AddPanoramaSky(const TCHAR* Key, float Bright, float NadirDim, const FLinearColor& NadirColor)
{
	UMaterialInterface* M = LoadArenaMat(TEXT("M_ArenaSky"));
	UTexture* Tex = LoadArenaTex(FString::Printf(TEXT("T_%sSky"), Key));
	if (!M || !Tex)
	{
		UE_LOG(LogZoogi, Warning, TEXT("Arena kit: sky assets for %s missing"), Key);
		return false;
	}
	UStaticMeshComponent* Sky = NewObject<UStaticMeshComponent>(this);
	Sky->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(Root);
	Sky->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sky->SetCastShadow(false);
	Sky->bAffectDistanceFieldLighting = false;
	Sky->bAffectDynamicIndirectLighting = false;
	Sky->bVisibleInRayTracing = false;
	Sky->SetReceivesDecals(false);
	Sky->SetRelativeScale3D(FVector(90000.f / 50.f));
	Sky->RegisterComponent();
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
	MID->SetTextureParameterValue(TEXT("Panorama"), Tex);
	MID->SetScalarParameterValue(TEXT("Bright"), Bright);
	MID->SetScalarParameterValue(TEXT("NadirDim"), NadirDim);
	MID->SetVectorParameterValue(TEXT("NadirColor"), NadirColor);
	Sky->SetMaterial(0, MID);
	Built.Add(Sky);
	Fog->SetFogCutoffDistance(80000.f); // the sky sphere (r 90000) stays unfogged; everything nearer is fogged as before
	return true;
}

UProceduralMeshComponent* AZoogiArena::NewProcMesh(bool bShadow, bool bCameraBlock)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCanEverAffectNavigation(false);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(bShadow);
	C->bUseComplexAsSimpleCollision = true;
	if (bCameraBlock)
	{
		C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		C->SetCollisionResponseToAllChannels(ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->RegisterComponent();
	Built.Add(C);
	return C;
}

UStaticMeshComponent* AZoogiArena::AddBoxR(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bShadow)
{
	UStaticMeshComponent* C = AddMesh(ZoogiUtil::BasicShape(TEXT("Cube")), Mat, false, bShadow);
	C->SetRelativeScale3D(Size / 100.f);
	C->SetRelativeRotation(Rot);
	C->SetRelativeLocation(Center);
	return C;
}

// ------------------------------------------------------------------ terrace

float AZoogiArena::TerraceHeight(float R, float AngleRad) const
{
	const FVector2D Q(FMath::Cos(AngleRad) * R * 0.004f, FMath::Sin(AngleRad) * R * 0.004f);
	const float Nz = FMath::PerlinNoise2D(Q);
	if (R <= TerraceFlatTo) return TerraceZ + Nz * 5.f;
	const float T = FMath::SmoothStep(0.f, 1.f, (R - TerraceFlatTo) / FMath::Max(TerraceROut - TerraceFlatTo, 1.f));
	return TerraceZ + Nz * 5.f + (TerraceRimZ - TerraceZ) * T + Nz * 35.f * T;
}

bool AZoogiArena::InTerraceGap(float AngleRad) const
{
	const float D = FMath::RadiansToDegrees(AngleRad);
	for (float G : TerraceGapYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(D, G)) < TerraceGapHalfDeg) return true;
	return false;
}

void AZoogiArena::AddTerrace(float RIn, float ROut, float Z, float FlatTo, float RimZ, UMaterialInterface* Top, UMaterialInterface* Wall, UMaterialInterface* Under)
{
	TerraceRIn = RIn; TerraceROut = ROut; TerraceZ = Z; TerraceFlatTo = FlatTo; TerraceRimZ = RimZ;
	const int32 Segs = 144;
	TArray<float> Rs;
	for (float R = RIn; R < ROut - 1.f; R += 60.f) Rs.Add(R);
	Rs.Add(ROut);
	FKitMesh TopM, WallM;
	const int32 Cols = Segs + 1;
	for (int32 j = 0; j < Rs.Num(); ++j)
	{
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = 2.f * PI * i / Segs;
			const float R = Rs[j];
			const FVector P = Polar(R, A, TerraceHeight(R, A));
			// Normal by finite differences.
			const float E = 8.f;
			const FVector Pr = Polar(R + E, A, TerraceHeight(R + E, A)) - Polar(R - E, A, TerraceHeight(R - E, A));
			const FVector Pa = Polar(R, A + E / R, TerraceHeight(R, A + E / R)) - Polar(R, A - E / R, TerraceHeight(R, A - E / R));
			FVector Nn = FVector::CrossProduct(Pr, Pa).GetSafeNormal();
			if (Nn.Z < 0.f) Nn = -Nn;
			TopM.Add(P, Nn, FVector2D(P.X, P.Y) * 0.002f);
		}
	}
	for (int32 j = 0; j < Rs.Num() - 1; ++j)
		for (int32 i = 0; i < Segs; ++i)
		{
			if (InTerraceGap(2.f * PI * (i + 0.5f) / Segs)) continue; // drop channel
			const int32 A = j * Cols + i, B = A + 1, Cc = A + Cols, D = Cc + 1;
			TopM.Tri(A, B, D, FVector::UpVector);
			TopM.Tri(A, D, Cc, FVector::UpVector);
		}
	// Inner cliff (faces the court) and outer cliff, flat-shaded rock.
	for (int32 i = 0; i < Segs; ++i)
	{
		const float A0 = 2.f * PI * i / Segs, A1 = 2.f * PI * (i + 1) / Segs, Am = (A0 + A1) * 0.5f;
		const bool bGap = InTerraceGap(Am);
		if (bGap)
		{
			// Channel cheeks: a cliff face on each side of the cut, facing into the channel.
			for (int32 e = 0; e < 2; ++e)
			{
				const float Ae = e ? A1 : A0;
				const bool bEdge = e ? !InTerraceGap(Am + 2.f * PI / Segs) : !InTerraceGap(Am - 2.f * PI / Segs);
				if (!bEdge) continue;
				const FVector Out = FVector::CrossProduct(FVector::UpVector, Polar(1.f, Ae, 0.f)) * (e ? -1.f : 1.f);
				for (int32 j = 0; j + 1 < Rs.Num(); ++j)
					WallM.Quad(Polar(Rs[j], Ae, TerraceHeight(Rs[j], Ae)), Polar(Rs[j + 1], Ae, TerraceHeight(Rs[j + 1], Ae)),
						Polar(Rs[j + 1], Ae, Z - 420.f), Polar(Rs[j], Ae, Z - 420.f), Out);
			}
			continue;
		}
		WallM.Quad(Polar(RIn, A0, TerraceHeight(RIn, A0)), Polar(RIn, A1, TerraceHeight(RIn, A1)), Polar(RIn - 20.f, A1, Z - 380.f), Polar(RIn - 20.f, A0, Z - 380.f), -Polar(1.f, Am, 0.f));
		WallM.Quad(Polar(ROut, A0, TerraceHeight(ROut, A0)), Polar(ROut, A1, TerraceHeight(ROut, A1)), Polar(ROut - 60.f, A1, RimZ - 520.f), Polar(ROut - 60.f, A0, RimZ - 520.f), Polar(1.f, Am, 0.f));
	}
	UProceduralMeshComponent* PM = NewProcMesh(true);
	TopM.Upload(PM, 0, false);
	WallM.Upload(PM, 1, false);
	PM->SetMaterial(0, Top);
	PM->SetMaterial(1, Wall);
	// One big craggy underside for the whole terrace ring.
	AddUnderCone(FVector2D::ZeroVector, ROut - 60.f, 2600.f, RimZ - 500.f, Under);
	FRandomStream Rng(808);
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * 2.f * PI / 10.f + Rng.FRandRange(-0.2f, 0.2f);
		AddUnderCone(FVector2D(Polar(Rng.FRandRange(1700.f, 2100.f), A, 0.f)), Rng.FRandRange(300.f, 520.f), Rng.FRandRange(900.f, 1600.f), RimZ - 480.f, Under);
	}
}

void AZoogiArena::ScatterTerrace(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten)
{
	if (!Mesh) return;
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetStaticMesh(Mesh);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetupAttachment(Root);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCastShadow(false);
	ISM->RegisterComponent();
	if (Mat) ISM->SetMaterial(0, Mat);
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		if (InTerraceGap(A)) continue;
		const float R = FMath::Sqrt(Rng.FRandRange(RMin * RMin, RMax * RMax));
		const float Sc = Rng.FRandRange(ScaleRange.X, ScaleRange.Y);
		FTransform T;
		T.SetLocation(Polar(R, A, TerraceHeight(R, A) + ZOffset));
		T.SetRotation(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Quaternion());
		T.SetScale3D(bFlatten ? FVector(Sc, Sc, Sc * 0.5f) : FVector(Sc));
		ISM->AddInstance(T);
	}
	Built.Add(ISM);
}

void AZoogiArena::AddCurb(float ROut, float TopZ, float Depth, UMaterialInterface* Mat)
{
	// Cut-stone curb just outside the knockout line, its top a few cm under the court (balls roll off exactly as before).
	AddCylinder(FVector2D::ZeroVector, ROut, Depth, TopZ, Mat, false)->SetCastShadow(false);
}

// ------------------------------------------------------------------ stands

void AZoogiArena::AddStands(float CenterDeg, float ArcDeg, float RIn, float BaseZ, int32 Tiers, float TierDepth, float TierRise, UMaterialInterface* Seat, UMaterialInterface* SeatAlt, UMaterialInterface* Riser, UMaterialInterface* Stone)
{
	FKitMesh SeatM, AltM, RiserM, StoneM;
	const int32 Segs = FMath::Max(4, FMath::RoundToInt(ArcDeg / 2.5f));
	const float A0 = FMath::DegreesToRadians(CenterDeg - ArcDeg * 0.5f), A1 = FMath::DegreesToRadians(CenterDeg + ArcDeg * 0.5f);
	const float Bottom = BaseZ - 320.f;
	const float ROuter = RIn + Tiers * TierDepth;
	const float TopZ = BaseZ + Tiers * TierRise;
	for (int32 s = 0; s < Segs; ++s)
	{
		const float Sa = FMath::Lerp(A0, A1, (float)s / Segs), Sb = FMath::Lerp(A0, A1, (float)(s + 1) / Segs), Sm = (Sa + Sb) * 0.5f;
		const FVector In = -Polar(1.f, Sm, 0.f);
		for (int32 k = 0; k < Tiers; ++k)
		{
			const float R0 = RIn + k * TierDepth, R1 = R0 + TierDepth, Rm = R0 + TierDepth * 0.55f;
			const float Z = BaseZ + (k + 1) * TierRise;
			const float Zp = k == 0 ? Bottom + 200.f : BaseZ + k * TierRise;
			// Bench plank (front) + footway (back), alternating shades read as boards.
			SeatM.Quad(Polar(R0, Sa, Z), Polar(R0, Sb, Z), Polar(Rm, Sb, Z), Polar(Rm, Sa, Z), FVector::UpVector);
			AltM.Quad(Polar(Rm, Sa, Z), Polar(Rm, Sb, Z), Polar(R1, Sb, Z), Polar(R1, Sa, Z), FVector::UpVector);
			RiserM.Quad(Polar(R0, Sa, Zp), Polar(R0, Sb, Zp), Polar(R0, Sb, Z), Polar(R0, Sa, Z), In);
		}
		// Back wall with a low parapet, and its top.
		StoneM.Quad(Polar(ROuter, Sa, Bottom), Polar(ROuter, Sb, Bottom), Polar(ROuter, Sb, TopZ + 45.f), Polar(ROuter, Sa, TopZ + 45.f), -In);
		StoneM.Quad(Polar(ROuter - 25.f, Sa, TopZ), Polar(ROuter - 25.f, Sb, TopZ), Polar(ROuter - 25.f, Sb, TopZ + 45.f), Polar(ROuter - 25.f, Sa, TopZ + 45.f), In);
		StoneM.Quad(Polar(ROuter - 25.f, Sa, TopZ + 45.f), Polar(ROuter - 25.f, Sb, TopZ + 45.f), Polar(ROuter, Sb, TopZ + 45.f), Polar(ROuter, Sa, TopZ + 45.f), FVector::UpVector);
	}
	// End caps (stepped stone cheeks).
	for (int32 e = 0; e < 2; ++e)
	{
		const float A = e ? A1 : A0;
		const FVector Out = FVector::CrossProduct(FVector::UpVector, Polar(1.f, A, 0.f)) * (e ? 1.f : -1.f);
		for (int32 k = 0; k < Tiers; ++k)
		{
			const float R0 = RIn + k * TierDepth, R1 = R0 + TierDepth;
			const float Z = BaseZ + (k + 1) * TierRise + 18.f;
			StoneM.Quad(Polar(R0, A, Bottom), Polar(R1, A, Bottom), Polar(R1, A, Z), Polar(R0, A, Z), Out);
		}
		StoneM.Quad(Polar(ROuter - 25.f, A, TopZ), Polar(ROuter, A, TopZ), Polar(ROuter, A, TopZ + 45.f), Polar(ROuter - 25.f, A, TopZ + 45.f), Out);
	}
	UProceduralMeshComponent* PM = NewProcMesh(true, true);
	SeatM.Upload(PM, 0, true); AltM.Upload(PM, 1, true); RiserM.Upload(PM, 2, true); StoneM.Upload(PM, 3, true);
	PM->SetMaterial(0, Seat); PM->SetMaterial(1, SeatAlt); PM->SetMaterial(2, Riser); PM->SetMaterial(3, Stone);
}

// ------------------------------------------------------------------ pagoda

void AZoogiArena::AddPagoda(const FVector& Base, float Yaw, int32 Storeys, float Size, UMaterialInterface* Body, UMaterialInterface* Panel, UMaterialInterface* Roof, UMaterialInterface* Trim, UMaterialInterface* Stone, UMaterialInterface* Gold)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	float Z = Base.Z;
	AddBoxR(FVector(Base.X, Base.Y, Z + 25.f), FVector(Size * 2.7f, Size * 2.7f, 50.f), Rot, Stone);
	Z += 50.f;
	FKitMesh RoofM, UnderM;
	auto Ring = [&](float Half, float Zr, float Lift, float Flare, TArray<FVector>& Out)
	{
		// 8 points: edge mids and corners, corners pushed out (Flare) and up (Lift) for the swept eaves.
		const FVector2D L[8] = { {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1} };
		Out.Reset();
		for (int32 k = 0; k < 8; ++k)
		{
			const bool bCorner = (k & 1) == 1;
			const FVector Lp(L[k].X * Half * (bCorner ? Flare : 1.f), L[k].Y * Half * (bCorner ? Flare : 1.f), Zr + (bCorner ? Lift : 0.f));
			Out.Add(FVector(Base.X, Base.Y, 0.f) + Rot.RotateVector(Lp));
		}
	};
	for (int32 i = 0; i < Storeys; ++i)
	{
		const float S = Size * (1.f - 0.17f * i);
		const float H = Size * (i == 0 ? 1.25f : 0.95f);
		AddBoxR(FVector(Base.X, Base.Y, Z + H * 0.5f), FVector(2.f * S, 2.f * S, H), Rot, Body);
		AddBoxR(FVector(Base.X, Base.Y, Z + H * 0.48f), FVector(2.f * S + 6.f, 2.f * S * 0.64f, H * 0.6f), Rot, Panel);
		AddBoxR(FVector(Base.X, Base.Y, Z + H * 0.48f), FVector(2.f * S * 0.64f, 2.f * S + 6.f, H * 0.6f), Rot, Panel);
		Z += H;
		// Roof: eave ring -> concave mid ring -> top ring (or apex on the last storey).
		const bool bLast = i == Storeys - 1;
		const float E = S * 1.6f, Hr = Size * (bLast ? 1.0f : 0.55f);
		TArray<FVector> R0, R1, R2, RU;
		Ring(E, Z + 10.f, Size * 0.28f, 1.1f, R0);
		Ring(E * 0.66f, Z + 10.f + Hr * 0.42f, Size * 0.08f, 1.04f, R1);
		Ring(bLast ? 1.f : S * 0.78f, Z + 10.f + Hr, 0.f, 1.f, R2);
		Ring(S, Z - 6.f, 0.f, 1.f, RU);
		const FVector Ax(Base.X, Base.Y, 0.f);
		for (int32 k = 0; k < 8; ++k)
		{
			const int32 K1 = (k + 1) % 8;
			auto Out = [&](const FVector& A, const FVector& B) { FVector O = (A + B) * 0.5f - Ax; O.Z = 0.f; return O.GetSafeNormal() + FVector(0, 0, 0.7f); };
			RoofM.Quad(R0[k], R0[K1], R1[K1], R1[k], Out(R0[k], R0[K1]));
			RoofM.Quad(R1[k], R1[K1], R2[K1], R2[k], Out(R1[k], R1[K1]));
			UnderM.Quad(R0[k], R0[K1], RU[K1], RU[k], FVector(0, 0, -1));
			// Eave fascia (thin dark edge).
			UnderM.Quad(R0[k], R0[K1], R0[K1] - FVector(0, 0, 12.f), R0[k] - FVector(0, 0, 12.f), Out(R0[k], R0[K1]) - FVector(0, 0, 0.7f));
		}
		if (!bLast)
		{
			for (int32 k = 0; k < 8; ++k) RoofM.FlatTri(FVector(Base.X, Base.Y, Z + 10.f + Hr), R2[k], R2[(k + 1) % 8], FVector::UpVector);
			Z += Hr * 0.55f;
		}
		else
		{
			// Finial.
			UStaticMeshComponent* F = AddMesh(ZoogiUtil::BasicShape(TEXT("Cone")), Gold, false);
			F->SetRelativeScale3D(FVector(0.18f, 0.18f, 1.1f) * (Size / 180.f));
			F->SetRelativeLocation(FVector(Base.X, Base.Y, Z + 10.f + Hr + 45.f * Size / 180.f));
		}
	}
	UProceduralMeshComponent* PM = NewProcMesh(true);
	RoofM.Upload(PM, 0, false);
	UnderM.Upload(PM, 1, false);
	PM->SetMaterial(0, Roof);
	PM->SetMaterial(1, Trim);
}

// ------------------------------------------------------------------ shrine gate (Recovery Gate, K6 visual)

void AZoogiArena::AddShrineGate(const FVector& Base, float Yaw, float Scale, UMaterialInterface* PostMat, UMaterialInterface* Beam, UMaterialInterface* Emblem, UMaterialInterface* Portal, UMaterialInterface* Stone)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	const FVector Across = Rot.RotateVector(FVector(0, 1, 0));
	const float W = 240.f * Scale, H = 290.f * Scale;
	auto At = [&](float Y, float Z) { return Base + Across * Y + FVector(0, 0, Z); };
	for (int32 s = -1; s <= 1; s += 2)
	{
		AddBoxR(At(s * W * 0.5f, 18.f * Scale), FVector(46.f, 46.f, 36.f) * Scale, Rot, Stone);
		const FVector P = At(s * W * 0.5f, 0.f);
		AddCylinder(FVector2D(P), 13.f * Scale, H - 34.f * Scale, P.Z + H - 30.f * Scale, PostMat, false);
	}
	AddBoxR(At(0.f, H - 44.f * Scale), FVector(26.f * Scale, W * 1.22f, 20.f * Scale), Rot, PostMat);             // shimaki
	AddBoxR(At(0.f, H - 22.f * Scale), FVector(32.f * Scale, W * 1.42f, 22.f * Scale), Rot, Beam);             // kasagi
	for (int32 s = -1; s <= 1; s += 2)                                                                           // upturned tips
		AddBoxR(At(s * W * 0.74f, H - 14.f * Scale), FVector(32.f * Scale, 34.f * Scale, 18.f * Scale), FRotator(0.f, Yaw, s * -14.f), Beam);
	AddBoxR(At(0.f, H * 0.68f), FVector(18.f * Scale, W * 1.12f, 16.f * Scale), Rot, PostMat);                      // nuki
	// Plaque with the Zoogy "Z" emblem between the beams.
	AddBoxR(At(0.f, H * 0.68f + (H - 44.f * Scale - H * 0.68f) * 0.5f), FVector(8.f, 56.f * Scale, 50.f * Scale), Rot, Beam);
	const FVector Fwd = Rot.RotateVector(FVector(1, 0, 0));
	const float Zc = H * 0.68f + (H - 44.f * Scale - H * 0.68f) * 0.5f;
	for (float Fs : { -1.f, 1.f })
	{
		const FVector Off = Fwd * Fs * 5.f;
		AddBoxR(At(0.f, Zc + 15.f * Scale) + Off, FVector(2.f, 34.f * Scale, 6.f * Scale), Rot, Emblem, false);
		AddBoxR(At(0.f, Zc - 15.f * Scale) + Off, FVector(2.f, 34.f * Scale, 6.f * Scale), Rot, Emblem, false);
		AddBoxR(At(0.f, Zc) + Off, FVector(2.f, 40.f * Scale, 6.f * Scale), FRotator(0.f, Yaw, Fs * 41.f), Emblem, false);
	}
	// Portal sheet (additive glow, no shadow).
	AddBoxR(At(0.f, H * 0.34f), FVector(2.f, W - 30.f * Scale, H * 0.62f), Rot, Portal, false);
}

// ------------------------------------------------------------------ resonance pillar (K4 visual)

void AZoogiArena::AddResonancePillar(const FVector& Base, float Yaw, float Height, UMaterialInterface* Stone, UMaterialInterface* Slit, UMaterialInterface* Cup, UMaterialInterface* Orb, UMaterialInterface* Vine)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	AddBoxR(Base + FVector(0, 0, 15.f), FVector(120.f, 120.f, 30.f), Rot, Stone);
	AddBoxR(Base + FVector(0, 0, 42.f), FVector(96.f, 96.f, 24.f), Rot, Stone);
	const float ShaftH = Height - 54.f - 16.f;
	const FVector ShaftC = Base + FVector(0, 0, 54.f + ShaftH * 0.5f);
	AddBoxR(ShaftC, FVector(64.f, 64.f, ShaftH), Rot, Stone);
	for (int32 f = 0; f < 4; ++f)
	{
		const FRotator Fr(0.f, Yaw + f * 90.f, 0.f);
		const FVector Out = Fr.RotateVector(FVector(1, 0, 0)), Side = Fr.RotateVector(FVector(0, 1, 0));
		for (int32 s = -1; s <= 1; s += 2)
			AddBoxR(ShaftC + Out * 32.5f + Side * (s * 12.f) + FVector(0, 0, 6.f), FVector(2.f, 6.f, ShaftH * 0.72f), Fr, Slit, false);
	}
	AddBoxR(Base + FVector(0, 0, Height - 8.f), FVector(78.f, 78.f, 16.f), Rot, Stone);
	// Gold cup: stem + flared bowl, holding the Awaken Orb crystal.
	AddCylinder(FVector2D(Base), 12.f, 18.f, Base.Z + Height + 18.f, Cup, false);
	AddUnderCone(FVector2D(Base), 32.f, 26.f, Base.Z + Height + 42.f, Cup);
	AddGem(Root, Base + FVector(0, 0, Height + 66.f), 26.f, 17.f, 6, Orb, false);
	// Vines: leafy clumps spiralling up two sides of the shaft and around the plinth.
	FRandomStream R((int32)(Base.X * 7.f + Base.Y * 13.f));
	UStaticMesh* Sph = ZoogiUtil::BasicShape(TEXT("Sphere"));
	for (int32 k = 0; k < 9; ++k)
	{
		const float T = k / 8.f;
		const float A = FMath::DegreesToRadians(Yaw + 30.f + T * 260.f);
		const FVector P = Base + FVector(FMath::Cos(A) * 36.f, FMath::Sin(A) * 36.f, 60.f + T * (ShaftH - 10.f));
		UStaticMeshComponent* V = AddMesh(Sph, Vine, false, false);
		V->SetRelativeScale3D(FVector(R.FRandRange(0.22f, 0.34f), R.FRandRange(0.22f, 0.34f), R.FRandRange(0.16f, 0.26f)));
		V->SetRelativeLocation(P);
	}
	for (int32 k = 0; k < 4; ++k)
	{
		const float A = FMath::DegreesToRadians(Yaw + k * 90.f + 45.f);
		UStaticMeshComponent* V = AddMesh(Sph, Vine, false, false);
		V->SetRelativeScale3D(FVector(0.5f, 0.42f, 0.3f));
		V->SetRelativeLocation(Base + FVector(FMath::Cos(A) * 62.f, FMath::Sin(A) * 62.f, 26.f));
	}
}

// ------------------------------------------------------------------ hub core (K3)

void AZoogiArena::AddGem(USceneComponent* Parent, const FVector& Center, float HalfHeight, float Radius, int32 Sides, UMaterialInterface* Mat, bool bShadow)
{
	FKitMesh G;
	const FVector Top(0, 0, HalfHeight), Bot(0, 0, -HalfHeight * 0.85f);
	for (int32 k = 0; k < Sides; ++k)
	{
		const float A0 = 2.f * PI * k / Sides, A1 = 2.f * PI * (k + 1) / Sides;
		const FVector E0(FMath::Cos(A0) * Radius, FMath::Sin(A0) * Radius, HalfHeight * 0.15f), E1(FMath::Cos(A1) * Radius, FMath::Sin(A1) * Radius, HalfHeight * 0.15f);
		const FVector Mid = (E0 + E1) * 0.5f;
		G.FlatTri(Top, E0, E1, Mid + FVector(0, 0, Radius));
		G.FlatTri(Bot, E0, E1, Mid - FVector(0, 0, Radius));
	}
	UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
	PM->SetMobility(EComponentMobility::Movable);
	PM->SetupAttachment(Parent ? Parent : Root.Get());
	PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PM->SetCastShadow(bShadow);
	PM->ComponentTags.Add(TEXT("NoCamBlock"));
	PM->RegisterComponent();
	PM->SetWorldLocation(Center);
	G.Upload(PM, 0, false);
	PM->SetMaterial(0, Mat);
	Built.Add(PM);
}

void AZoogiArena::AddHubCore(const FVector& Center, float HalfHeight, UMaterialInterface* Crystal)
{
	USceneComponent* Pivot = NewObject<USceneComponent>(this);
	Pivot->SetMobility(EComponentMobility::Movable);
	Pivot->SetupAttachment(Root);
	Pivot->RegisterComponent();
	Pivot->SetRelativeLocation(Center);
	Built.Add(Pivot);
	AddGem(Pivot, Center, HalfHeight, HalfHeight * 0.55f, 8, Crystal, false);
	URotatingMovementComponent* Spin = NewObject<URotatingMovementComponent>(this);
	Spin->bAutoRegisterUpdatedComponent = false;
	Spin->RotationRate = FRotator(0.f, 12.f, 0.f);
	Spin->RegisterComponent();
	Spin->SetUpdatedComponent(Pivot);
	Built.Add(Spin);
}

// ------------------------------------------------------------------ waterfall

void AZoogiArena::AddWaterfall(const FVector& Top, float Yaw, float Width, float Height, float Lip, UMaterialInterface* MatOverride)
{
	UMaterialInterface* Base = MatOverride ? MatOverride : LoadArenaMat(TEXT("M_ArenaWaterfall"));
	if (!Base) return;
	const FVector Fwd = FRotator(0.f, Yaw, 0.f).Vector(), Side = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
	// Path: short lip arc forward, then straight down.
	TArray<FVector> Path;
	for (int32 i = 0; i <= 4; ++i) { const float T = i / 4.f; Path.Add(Top + Fwd * (Lip * FMath::Sin(T * PI * 0.5f)) - FVector(0, 0, Lip * 0.6f * T * T)); }
	for (int32 i = 1; i <= 6; ++i) Path.Add(Path[4] + Fwd * (i * 6.f) - FVector(0, 0, Height * i / 6.f));
	FKitMesh M;
	float Len = 0.f;
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		if (i > 0) Len += FVector::Dist(Path[i], Path[i - 1]);
		const FVector D = (Path[FMath::Min(i + 1, Path.Num() - 1)] - Path[FMath::Max(i - 1, 0)]).GetSafeNormal();
		const FVector Nn = FVector::CrossProduct(Side, D).GetSafeNormal();
		const float Wd = Width * (1.f + 0.25f * (float)i / Path.Num());
		M.Add(Path[i] - Side * Wd * 0.5f, Nn, FVector2D(0.f, Len / 200.f));
		M.Add(Path[i] + Side * Wd * 0.5f, Nn, FVector2D(1.f, Len / 200.f));
	}
	for (int32 i = 0; i < Path.Num() - 1; ++i)
	{
		const int32 A = i * 2;
		M.Tri(A, A + 1, A + 3, M.N[A]);
		M.Tri(A, A + 3, A + 2, M.N[A]);
	}
	UProceduralMeshComponent* PM = NewProcMesh(false);
	M.Upload(PM, 0, false);
	if (MatOverride) { PM->SetMaterial(0, MatOverride); return; } // lava falls (one M_ArenaLava MID shared by all falls)
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
	MID->SetScalarParameterValue(TEXT("VMax"), Len / 200.f);
	PM->SetMaterial(0, MID);
}

// ------------------------------------------------------------------ Volcanic kit (Oct 6): lava, obsidian, fortress

UMaterialInterface* AZoogiArena::KitLavaMaterial(float TileU, float TileV, float SpeedU, float SpeedV, float Bright, float CrustAmount)
{
	UMaterialInterface* M = LoadArenaMat(TEXT("M_ArenaLava"));
	if (!M) return nullptr;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
	MID->SetScalarParameterValue(TEXT("TileU"), TileU);
	MID->SetScalarParameterValue(TEXT("TileV"), TileV);
	MID->SetScalarParameterValue(TEXT("SpeedU"), SpeedU);
	MID->SetScalarParameterValue(TEXT("SpeedV"), SpeedV);
	MID->SetScalarParameterValue(TEXT("Bright"), Bright);
	MID->SetScalarParameterValue(TEXT("CrustAmount"), CrustAmount);
	return MID;
}

void AZoogiArena::AddObsidianOutcrop(const FVector2D& P, float Rad, float H, float Yaw, int32 Seed, UMaterialInterface* Rock, UMaterialInterface* Crack)
{
	// Jagged obsidian shards inside the old boulder footprint; the invisible collider cylinder stays the gameplay shape.
	FRandomStream Rs(Seed);
	FKitMesh M, C;
	const int32 N = 4 + Rs.RandRange(0, 2);
	const float Base0 = FMath::DegreesToRadians(Yaw);
	for (int32 s = 0; s < N; ++s)
	{
		const bool bMain = s == 0;
		const float A = bMain ? Base0 : Base0 + (2.f * PI * s) / N + Rs.FRandRange(-0.3f, 0.3f);
		const float D = bMain ? Rad * 0.08f : Rad * Rs.FRandRange(0.38f, 0.62f);
		const FVector B(P.X + FMath::Cos(A) * D, P.Y + FMath::Sin(A) * D, -25.f);
		const float Sr = bMain ? Rad * 0.5f : Rad * Rs.FRandRange(0.24f, 0.36f);
		const float Sh = (bMain ? H * 1.25f : H * Rs.FRandRange(0.5f, 0.9f)) + 25.f;
		const float Lean = FMath::DegreesToRadians(bMain ? Rs.FRandRange(4.f, 10.f) : Rs.FRandRange(12.f, 26.f));
		const FVector Dir(FMath::Cos(A) * FMath::Sin(Lean), FMath::Sin(A) * FMath::Sin(Lean), FMath::Cos(Lean));
		const FVector Tip = B + Dir * Sh;
		const FVector Sh0 = B + Dir * Sh * Rs.FRandRange(0.55f, 0.72f);
		const int32 K = 5;
		const float Rot0 = Rs.FRandRange(0.f, 2.f * PI);
		TArray<FVector> Lo, Hi;
		for (int32 k = 0; k < K; ++k)
		{
			const float Ak = Rot0 + 2.f * PI * k / K + Rs.FRandRange(-0.25f, 0.25f);
			const FVector Rv(FMath::Cos(Ak), FMath::Sin(Ak), 0.f);
			Lo.Add(B + Rv * Sr * Rs.FRandRange(0.85f, 1.15f));
			Hi.Add(Sh0 + Rv * Sr * Rs.FRandRange(0.45f, 0.7f));
		}
		for (int32 k = 0; k < K; ++k)
		{
			const int32 K1 = (k + 1) % K;
			const FVector OutLo = (Lo[k] + Lo[K1] + Hi[k] + Hi[K1]) * 0.25f - (B + Sh0) * 0.5f;
			M.Quad(Lo[k], Lo[K1], Hi[K1], Hi[k], OutLo);
			const FVector OutHi = (Hi[k] + Hi[K1] + Tip) / 3.f - (Sh0 + Tip) * 0.5f;
			M.FlatTri(Hi[k], Hi[K1], Tip, OutHi);
			// Thin glowing fissures (spec: obstacles must read on the near-black floor).
			if ((bMain && (k == 0 || k == 2)) || (!bMain && k == 1))
			{
				FVector Fn = FVector::CrossProduct(Lo[K1] - Lo[k], Hi[k] - Lo[k]).GetSafeNormal();
				if (FVector::DotProduct(Fn, OutLo) < 0.f) Fn = -Fn;
				const FVector Tg = (Lo[K1] - Lo[k]).GetSafeNormal();
				const FVector MidLo = FMath::Lerp(Lo[k], Lo[K1], 0.5f), MidHi = FMath::Lerp(Hi[k], Hi[K1], 0.5f);
				FVector Prev = FMath::Lerp(MidLo, MidHi, 0.18f);
				const FVector End = FMath::Lerp(Hi[k], Hi[K1], Rs.FRandRange(0.35f, 0.65f));
				const int32 Steps = 4;
				for (int32 i = 1; i <= Steps; ++i)
				{
					FVector Q = FMath::Lerp(Prev, End, 1.f / (Steps - i + 1));
					if (i < Steps) Q += Tg * Rs.FRandRange(-0.16f, 0.16f) * Sr;
					const float W = FMath::Lerp(3.4f, 1.4f, (float)i / Steps);
					const FVector Off = Fn * 1.5f;
					C.Quad(Prev - Tg * W + Off, Prev + Tg * W + Off, Q + Tg * W * 0.8f + Off, Q - Tg * W * 0.8f + Off, Fn);
					Prev = Q;
				}
			}
		}
	}
	UProceduralMeshComponent* PM = NewProcMesh(true);
	M.Upload(PM, 0, false);
	PM->SetMaterial(0, Rock);
	if (Crack)
	{
		UProceduralMeshComponent* PC = NewProcMesh(false);
		C.Upload(PC, 0, false);
		PC->SetMaterial(0, Crack);
	}
}

void AZoogiArena::AddFortressWall(float CenterDeg, float ArcDeg, float RIn, float Thick, float Height, float GateHalfWidth, UMaterialInterface* Stone, UMaterialInterface* Trim, UMaterialInterface* Slit)
{
	FKitMesh W, T;
	const float ROut = RIn + Thick, Bot = TerraceZ - 300.f, Top = TerraceZ + Height;
	const float Ac = FMath::DegreesToRadians(CenterDeg), Half = FMath::DegreesToRadians(ArcDeg * 0.5f);
	const float Gap = GateHalfWidth / RIn;
	const float Ranges[2][2] = { { Ac - Half, Ac - Gap }, { Ac + Gap, Ac + Half } };
	int32 SlitIdx = 0;
	for (int32 r = 0; r < 2; ++r)
	{
		const float A0 = Ranges[r][0], A1 = Ranges[r][1];
		const int32 Segs = FMath::Max(3, FMath::RoundToInt(FMath::RadiansToDegrees(A1 - A0) / 2.f));
		for (int32 s = 0; s < Segs; ++s)
		{
			const float Sa = FMath::Lerp(A0, A1, (float)s / Segs), Sb = FMath::Lerp(A0, A1, (float)(s + 1) / Segs), Sm = (Sa + Sb) * 0.5f;
			const FVector Out = Polar(1.f, Sm, 0.f);
			W.Quad(Polar(RIn, Sa, Bot), Polar(RIn, Sb, Bot), Polar(RIn, Sb, Top), Polar(RIn, Sa, Top), -Out);
			W.Quad(Polar(ROut, Sa, Bot), Polar(ROut, Sb, Bot), Polar(ROut, Sb, Top), Polar(ROut, Sa, Top), Out);
			T.Quad(Polar(RIn, Sa, Top), Polar(RIn, Sb, Top), Polar(ROut, Sb, Top), Polar(ROut, Sa, Top), FVector::UpVector);
			if ((s & 1) == 0)
			{
				// Merlon on the court-side edge.
				const float R0 = RIn, R1 = RIn + Thick * 0.4f, Z1 = Top + 60.f;
				const FVector Sd = FVector::CrossProduct(FVector::UpVector, Out);
				W.Quad(Polar(R0, Sa, Top), Polar(R0, Sb, Top), Polar(R0, Sb, Z1), Polar(R0, Sa, Z1), -Out);
				W.Quad(Polar(R1, Sa, Top), Polar(R1, Sb, Top), Polar(R1, Sb, Z1), Polar(R1, Sa, Z1), Out);
				W.Quad(Polar(R0, Sa, Top), Polar(R1, Sa, Top), Polar(R1, Sa, Z1), Polar(R0, Sa, Z1), -Sd);
				W.Quad(Polar(R0, Sb, Top), Polar(R1, Sb, Top), Polar(R1, Sb, Z1), Polar(R0, Sb, Z1), Sd);
				T.Quad(Polar(R0, Sa, Z1), Polar(R0, Sb, Z1), Polar(R1, Sb, Z1), Polar(R1, Sa, Z1), FVector::UpVector);
			}
			if ((SlitIdx++ % 3) == 1)
				AddBoxR(Polar(RIn - 1.5f, Sm, TerraceZ + Height * 0.55f), FVector(3.f, 12.f, 80.f), FRotator(0.f, FMath::RadiansToDegrees(Sm), 0.f), Slit, false);
		}
		for (int32 e = 0; e < 2; ++e)
		{
			const float A = e ? A1 : A0;
			const FVector Sd = FVector::CrossProduct(FVector::UpVector, Polar(1.f, A, 0.f)) * (e ? 1.f : -1.f);
			W.Quad(Polar(RIn, A, Bot), Polar(ROut, A, Bot), Polar(ROut, A, Top), Polar(RIn, A, Top), Sd);
		}
	}
	UProceduralMeshComponent* PM = NewProcMesh(true);
	W.Upload(PM, 0, false);
	T.Upload(PM, 1, false);
	PM->SetMaterial(0, Stone);
	PM->SetMaterial(1, Trim);
}

void AZoogiArena::AddFortressGate(const FVector& Base, float Yaw, float Height, UMaterialInterface* Stone, UMaterialInterface* Trim, UMaterialInterface* Emblem, UMaterialInterface* Portal)
{
	const FRotator Rot(0.f, Yaw, 0.f);
	const FVector Fwd = Rot.Vector(), Across = Rot.RotateVector(FVector(0, 1, 0));
	const float OpenW = 260.f, TW = 150.f, TD = 230.f, TH = Height + 140.f, Bury = 300.f;
	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector C = Base + Across * (s * (OpenW * 0.5f + TW * 0.5f));
		AddBoxR(C + FVector(0, 0, (TH - Bury) * 0.5f), FVector(TD, TW, TH + Bury), Rot, Stone);
		AddBoxR(C + FVector(0, 0, TH + 15.f), FVector(TD + 24.f, TW + 24.f, 30.f), Rot, Trim);
		AddBoxR(C + FVector(0, 0, TH + 42.f), FVector(70.f, 70.f, 24.f), Rot, Trim);
		AddBoxR(C + FVector(0, 0, TH + 57.f), FVector(54.f, 54.f, 8.f), Rot, Emblem, false);
	}
	AddBoxR(Base + FVector(0, 0, Height - 30.f), FVector(TD * 0.9f, OpenW + 40.f, 60.f), Rot, Trim);
	AddBoxR(Base + FVector(0, 0, Height + 70.f), FVector(TD * 0.7f, OpenW + 20.f, 140.f), Rot, Stone);
	// Zoogy "Z" plaque facing the court (-Fwd).
	const FVector Face = Base - Fwd * (TD * 0.35f + 2.f) + FVector(0, 0, Height + 70.f);
	AddBoxR(Face, FVector(4.f, 110.f, 110.f), Rot, Trim, false);
	const FVector E = Face - Fwd * 3.f;
	AddBoxR(E + FVector(0, 0, 32.f), FVector(2.f, 70.f, 12.f), Rot, Emblem, false);
	AddBoxR(E - FVector(0, 0, 32.f), FVector(2.f, 70.f, 12.f), Rot, Emblem, false);
	AddBoxR(E, FVector(2.f, 84.f, 12.f), FRotator(0.f, Yaw, -42.f), Emblem, false);
	// Portal sheet in the opening (additive glow).
	AddBoxR(Base + FVector(0, 0, (Height - 60.f) * 0.5f), FVector(2.f, OpenW - 10.f, Height - 60.f), Rot, Portal, false);
}

void AZoogiArena::AddLavaChannel(float Deg, float R0, float R1, float Width, UMaterialInterface* Mat)
{
	FKitMesh M;
	const float A = FMath::DegreesToRadians(Deg);
	const FVector Side = Polar(1.f, A + PI * 0.5f, 0.f);
	const int32 N = FMath::Max(2, FMath::CeilToInt((R1 - R0) / 30.f));
	for (int32 i = 0; i <= N; ++i)
	{
		const float R = FMath::Lerp(R0, R1, (float)i / N);
		const float Wd = Width * (0.85f + 0.25f * FMath::Sin(i * 1.3f));
		const FVector Cc = Polar(R, A, 0.f);
		for (int32 s = 0; s < 2; ++s)
		{
			const FVector P = Cc + Side * Wd * (s ? 0.5f : -0.5f);
			const float Ang = FMath::Atan2(P.Y, P.X), Rr = FVector2D(P.X, P.Y).Size();
			M.Add(FVector(P.X, P.Y, TerraceHeight(Rr, Ang) + 4.f), FVector::UpVector, FVector2D((float)s, (R - R0) / 200.f));
		}
	}
	for (int32 i = 0; i < N; ++i)
	{
		const int32 a = i * 2;
		M.Tri(a, a + 1, a + 3, FVector::UpVector);
		M.Tri(a, a + 3, a + 2, FVector::UpVector);
	}
	UProceduralMeshComponent* PM = NewProcMesh(false);
	M.Upload(PM, 0, false);
	PM->SetMaterial(0, Mat);
}

void AZoogiArena::AddBridge(const FVector& A, const FVector& B, float Width, float Sag, UMaterialInterface* Plank, UMaterialInterface* Rope)
{
	const FVector D = B - A;
	const float L = FVector2D(D.X, D.Y).Size();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	const FVector Side = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
	auto At = [&](float T) { return A + D * T - FVector(0, 0, Sag * 4.f * T * (1.f - T)); };
	auto Seg = [&](const FVector& P0, const FVector& P1, float W, float Th, UMaterialInterface* Mat, bool bShadow)
	{
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(P1.Z - P0.Z, FVector2D(P1.X - P0.X, P1.Y - P0.Y).Size()));
		AddBoxR((P0 + P1) * 0.5f, FVector(FVector::Dist(P0, P1), W, Th), FRotator(Pitch, Yaw, 0.f), Mat, bShadow);
	};
	const int32 N = FMath::Max(6, FMath::RoundToInt(L / 42.f));
	for (int32 i = 0; i < N; ++i)
		Seg(At((i + 0.08f) / N), At((i + 0.92f) / N), Width * ((i % 3) ? 1.f : 0.92f), 7.f, Plank, true);
	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector O = Side * (s * Width * 0.5f);
		AddBoxR(A + O + FVector(0, 0, 45.f), FVector(12.f, 12.f, 110.f), FRotator(0.f, Yaw, 0.f), Rope);
		AddBoxR(B + O + FVector(0, 0, 45.f), FVector(12.f, 12.f, 110.f), FRotator(0.f, Yaw, 0.f), Rope);
		const int32 M = 6;
		for (int32 i = 0; i < M; ++i)
			Seg(At((float)i / M) + O + FVector(0, 0, 70.f), At((float)(i + 1) / M) + O + FVector(0, 0, 70.f), 4.f, 4.f, Rope, false);
	}
}

void AZoogiArena::AddFillLight(const FVector& Pos, const FLinearColor& Color, float Candela, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Root);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Candela);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->RegisterComponent();
	L->SetRelativeLocation(Pos);
	Built.Add(L);
}

void AZoogiArena::AddMagmaCore(const FVector& Center, float Radius, UMaterialInterface* Mat, const FLinearColor& LightColor, float Candela, float LightRadius)
{
	// Floating magma orb over the molten core (K3): small so it never hides the 3 centre orbs, no collider / shadow.
	UStaticMeshComponent* S = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), Mat, false, false);
	S->SetRelativeScale3D(FVector(Radius / 50.f));
	S->SetRelativeLocation(Center);
	S->ComponentTags.Add(TEXT("NoCamBlock"));
	AddFillLight(Center, LightColor, Candela, LightRadius);
}