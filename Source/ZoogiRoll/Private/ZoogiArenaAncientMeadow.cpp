// Nov 1 (Evan: "Build this stage for ancient meadows ... don't stop until it look photo realistic exactly like it", reference
// C:\dev\art\ancient_meadows\meadow2\meadow_ref.png): Ancient Collision Meadows v2 - the sunset meadow kingdom.
//   centre  : lava volcano on an arched stone drum (glowing doorways, Z banners), cantilevered ring walkway with gold rails, all-gold
//             spike crown holding a purple orb with a gold ring
//   plaza   : cream pavers (hub tile), purple cobble ring band with gold rims, Z medallion in front of the volcano, blue spawn rings,
//             blue-orb lamp posts on the rim (the old pillar colliders), drum bumpers alternate red/white and yellow/black
//   front   : staircase down from the plaza (collision to z -500) with the Z-banner gate, brick path with lanterns and big lamp pillars
//   islands : each floating region becomes a rock mesa standing in the meadow; landmarks on their own outcrops: crystal shrine (NW),
//             gold-dome palace with purple canopies (NE), domed temple (SW), armillary sphere (SE); gold balustrades = the old walls
//   world   : lawn valley at z -640 (below FallOutZ: a fall still drops out of play), rivers, palms, purple plants, rocks, distant mesas
//             with arches and waterfalls, painted sunset sky with the ringed planet (T_AncientDuskSky), warm low sun.
// Gameplay hulls kept from v1 (volcano + drum, rim posts, walls, star pads, crate drums). -ZoogiAncientV1 = the Oct 31 look.
#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
#include "ZoogiObstacle.h"
#include "ZoogiMeshKit.h"
#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

namespace ZoogiMeadow2
{
	constexpr float GZ = -640.f;                    // meadow ground top
	constexpr float BaseR = 1100.f, PeakZ = 2050.f, TopR = 520.f;   // visual mountain (overhangs the drum above head height); collision stays r900
	constexpr float DrumR = 960.f, DrumH = 380.f;   // the arched stone drum the volcano stands on (collision)
	float MtnR(float Z) { const float T = FMath::Clamp(Z / PeakZ, 0.f, 1.f); return FMath::Lerp(BaseR, TopR, FMath::Pow(T, 1.45f)); } // broad shoulders (ref: a wide stepped mountain)

	/** Textured quad, P0 bottom-left, P1 bottom-right, P2 top-right, P3 top-left (texture upright), both faces (back mirrored back). */
	void UVQuad(FZoogiMeshBuilder& M, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3)
	{
		const FVector Nn = FVector::CrossProduct(P3 - P0, P1 - P0).GetSafeNormal();
		for (int32 s = 0; s < 2; ++s)
		{
			const FVector Nr = s ? -Nn : Nn, Off = Nr * 0.8f;
			const float U0 = s ? 1.f : 0.f, U1 = s ? 0.f : 1.f;
			const int32 a = M.Add(P0 + Off, Nr, FVector2D(U0, 1.f)), b = M.Add(P1 + Off, Nr, FVector2D(U1, 1.f));
			const int32 c = M.Add(P2 + Off, Nr, FVector2D(U1, 0.f)), d = M.Add(P3 + Off, Nr, FVector2D(U0, 0.f));
			M.Tri(a, b, c, Nr); M.Tri(a, c, d, Nr);
		}
	}
	/** Flat textured disc facing up, image upright for the match camera (+Y looking -Y: image top = -Y). */
	void UVDisc(FZoogiMeshBuilder& M, const FVector2D& C, float R, float Z, int32 N = 48)
	{
		const int32 Ci = M.Add(FVector(C, Z), FVector::UpVector, FVector2D(0.5f, 0.5f));
		TArray<int32> Rim;
		for (int32 i = 0; i <= N; ++i)
		{
			const float A = 2.f * PI * i / N; const float Cx = FMath::Cos(A), Sy = FMath::Sin(A);
			Rim.Add(M.Add(FVector(C.X + Cx * R, C.Y + Sy * R, Z), FVector::UpVector, FVector2D(0.5f + 0.5f * Cx, 0.5f + 0.5f * Sy)));
		}
		for (int32 i = 0; i < N; ++i) M.Tri(Ci, Rim[i], Rim[i + 1], FVector::UpVector);
	}
	/** Hexagonal crystal along Axis with a pointed tip. */
	void Crystal(FZoogiMeshBuilder& M, const FVector& Base, const FVector& Axis, float R, float Len)
	{
		const FVector A = Axis.GetSafeNormal();
		FVector U = FVector::CrossProduct(A, FVector::UpVector); if (U.SizeSquared() < 1e-3f) U = FVector::ForwardVector; U.Normalize();
		const FVector Vv = FVector::CrossProduct(A, U);
		const FVector Tip = Base + A * Len, Top = Base + A * (Len * 0.76f);
		FVector B[6], T[6];
		for (int32 i = 0; i < 6; ++i) { const float Ang = PI / 3.f * i; const FVector D = U * FMath::Cos(Ang) + Vv * FMath::Sin(Ang); B[i] = Base + D * R * 0.8f; T[i] = Top + D * R; }
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 j = (i + 1) % 6;
			const FVector Out = ((B[i] + B[j]) * 0.5f - Base).GetSafeNormal();
			M.Quad(B[i], B[j], T[j], T[i], Out);
			M.FlatTri(T[i], T[j], Tip, (Out + A).GetSafeNormal());
		}
	}
	/** Two-sided quad helper (cloth, leaves). */
	void Quad2(FZoogiMeshBuilder& M, const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Nrm)
	{
		M.Quad(P0, P1, P2, P3, Nrm); M.Quad(P0 + Nrm * -0.5f, P1 + Nrm * -0.5f, P2 + Nrm * -0.5f, P3 + Nrm * -0.5f, -Nrm);
	}
	/** Palm: leaning ringed trunk + drooping folded fronds. */
	void Palm(FZoogiMeshBuilder& Tr, FZoogiMeshBuilder& Lf, FZoogiMeshBuilder& Lf2, const FVector& Base, float Ht, float LeanYaw, FRandomStream& R)
	{
		const FVector Lean = FRotator(0.f, LeanYaw, 0.f).Vector();
		auto At = [&](float T) { return Base + Lean * (Ht * 0.22f * T * T) + FVector(0, 0, Ht * T); };
		const int32 NSeg = 8, NS = 7;
		for (int32 s = 0; s < NSeg; ++s)
		{
			const float T0 = (float)s / NSeg, T1 = (float)(s + 1) / NSeg;
			const FVector P0 = At(T0), P1 = At(T1);
			const float K = Ht / 600.f, R0 = FMath::Lerp(34.f, 18.f, T0) * K, R1 = FMath::Lerp(34.f, 18.f, T1) * K * 1.14f;
			for (int32 k = 0; k < NS; ++k)
			{
				const float A0 = 2.f * PI * k / NS, A1 = 2.f * PI * (k + 1) / NS;
				const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0.f), D1(FMath::Cos(A1), FMath::Sin(A1), 0.f);
				Tr.Quad(P0 + D0 * R0, P0 + D1 * R0, P1 + D1 * R1, P1 + D0 * R1, (D0 + D1).GetSafeNormal());
			}
		}
		const FVector Top = At(1.f);
		const int32 NF = 10;
		for (int32 f = 0; f < NF; ++f)
		{
			const float Yw = f * 360.f / NF + R.FRandRange(-14.f, 14.f);
			const FVector D = FRotator(0.f, Yw, 0.f).Vector(), Sd(-D.Y, D.X, 0.f);
			const float L = Ht * R.FRandRange(0.42f, 0.56f), Up = R.FRandRange(0.35f, 0.7f);
			FVector PrevC = Top; float PrevW = 2.f;
			FZoogiMeshBuilder& Dst = (f % 3 == 0) ? Lf2 : Lf;
			for (int32 i = 1; i <= 7; ++i)
			{
				const float t = i / 7.f;
				const FVector C = Top + D * (L * t) + FVector(0, 0, L * (Up * t - 1.05f * t * t));
				const float W = L * 0.17f * FMath::Sin(FMath::Min(t * 1.25f, 1.f) * PI * 0.5f + (t > 0.8f ? (t - 0.8f) * 5.f : 0.f)) * (t > 0.85f ? (1.f - t) / 0.15f : 1.f) + 3.f;
				const FVector Nrm = FVector::CrossProduct(C - PrevC, Sd).GetSafeNormal();
				const FVector Fold0(0, 0, -PrevW * 0.35f), Fold1(0, 0, -W * 0.35f);
				Quad2(Dst, PrevC, C, C + Sd * W + Fold1, PrevC + Sd * PrevW + Fold0, Nrm);
				Quad2(Dst, PrevC - Sd * PrevW + Fold0, C - Sd * W + Fold1, C, PrevC, Nrm);
				PrevC = C; PrevW = W;
			}
		}
	}
	/** Fan of long arching blades (purple ornamental plant / fern). */
	void Plant(FZoogiMeshBuilder& M, const FVector& Base, float Size, int32 Blades, FRandomStream& R)
	{
		for (int32 b = 0; b < Blades; ++b)
		{
			const float Yw = b * 360.f / Blades + R.FRandRange(-15.f, 15.f);
			const FVector D = FRotator(0.f, Yw, 0.f).Vector(), Sd(-D.Y, D.X, 0.f);
			const float L = Size * R.FRandRange(0.75f, 1.1f), Rise = R.FRandRange(0.7f, 1.2f);
			FVector Prev = Base; float PrevW = 1.f;
			for (int32 i = 1; i <= 5; ++i)
			{
				const float t = i / 5.f;
				const FVector C = Base + D * (L * t * 0.9f) + FVector(0, 0, L * (Rise * t - 0.75f * t * t));
				const float W = L * 0.16f * FMath::Sin(t * PI) + 1.f;
				const FVector Nrm = FVector::CrossProduct(C - Prev, Sd).GetSafeNormal();
				Quad2(M, Prev - Sd * PrevW, C - Sd * W, C + Sd * W, Prev + Sd * PrevW, Nrm);
				Prev = C; PrevW = W;
			}
		}
	}
	/** Jagged rock skirt under a floor polygon: perimeter resampled every ~Step, rings stepping down to the meadow with strata ledges. */
	void CliffSkirt(FZoogiMeshBuilder& M, const TArray<FVector2D>& Poly, float ZTop, float ZBot, float Jag, float Flare, int32 Seed, float Step = 260.f)
	{
		FRandomStream R(Seed);
		FVector2D C(0, 0); for (const FVector2D& V : Poly) C += V; C /= FMath::Max(1, Poly.Num());
		TArray<FVector2D> P, Nr;
		for (int32 i = 0; i < Poly.Num(); ++i)
		{
			const FVector2D A = Poly[i], B = Poly[(i + 1) % Poly.Num()];
			const int32 N = FMath::Max(1, FMath::RoundToInt(FVector2D::Distance(A, B) / Step));
			for (int32 k = 0; k < N; ++k) P.Add(FMath::Lerp(A, B, (float)k / N));
		}
		for (const FVector2D& Q : P) Nr.Add((Q - C).GetSafeNormal());
		const int32 NL = 6;
		TArray<TArray<FVector>> Rings;
		for (int32 l = 0; l <= NL; ++l)
		{
			const float T = (float)l / NL;
			const float Z = FMath::Lerp(ZTop, ZBot, FMath::Pow(T, 0.9f));
			TArray<FVector>& Rg = Rings.AddDefaulted_GetRef();
			for (int32 i = 0; i < P.Num(); ++i)
			{
				const float Out = l == 0 ? 0.f : (Flare * T * T + R.FRandRange(-0.35f, 1.f) * Jag);
				Rg.Add(FVector(P[i] + Nr[i] * Out, Z + (l > 0 && l < NL ? R.FRandRange(-40.f, 40.f) : 0.f)));
			}
		}
		for (int32 l = 0; l < NL; ++l)
			for (int32 i = 0; i < P.Num(); ++i)
			{
				const int32 j = (i + 1) % P.Num();
				const FVector Out(((Nr[i] + Nr[j]) * 0.5f).GetSafeNormal(), 0.15f);
				M.Quad(Rings[l + 1][i], Rings[l + 1][j], Rings[l][j], Rings[l][i], Out);
			}
	}
}

#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", off)
#endif
void AZoogiArena::BuildAncientMeadow()
{
	using namespace ZoogiMeadow2;
	using namespace ZoogiAncient;
	using ZoogiUtil::Hex;
	UWorld* W = GetWorld();
	if (!W) return;
	const FLayout& L = Get();
	// ------------------------------------------------------------------ materials
	UMaterialInterface* TileBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_AncientTile.M_AncientTile"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* DecalBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_AncientDecal.M_AncientDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto TexA = [](const TCHAR* N) { return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet); };
	auto Tile = [&](const TCHAR* Top, const TCHAR* Side, float TS, float SS, const TCHAR* Fallback, const TCHAR* Tint = nullptr, float Bright = 1.f, float Rough = 0.8f) -> UMaterialInterface*
	{
		UTexture* T = TexA(Top); UTexture* S = TexA(Side);
		if (!TileBase || !T || !S) return ZoogiUtil::SurfaceMID(this, Hex(Fallback), Rough);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TileBase, this);
		M->SetTextureParameterValue(TEXT("TopTex"), T); M->SetTextureParameterValue(TEXT("SideTex"), S);
		M->SetScalarParameterValue(TEXT("TopScale"), TS); M->SetScalarParameterValue(TEXT("SideScale"), SS); M->SetScalarParameterValue(TEXT("Bright"), Bright);
		M->SetScalarParameterValue(TEXT("Roughness"), Rough);
		if (Tint) { M->SetVectorParameterValue(TEXT("TopTint"), Hex(Tint)); M->SetVectorParameterValue(TEXT("SideTint"), Hex(Tint)); }
		return M;
	};
	auto Decal = [&](const TCHAR* TexName, float Emis) -> UMaterialInterface*
	{
		UTexture* T = TexA(TexName);
		if (!DecalBase || !T) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(DecalBase, this);
		M->SetTextureParameterValue(TEXT("Tex"), T); M->SetScalarParameterValue(TEXT("EmissiveStrength"), Emis);
		return M;
	};
	UMaterialInterface* CliffM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#76584E")), 0.92f);   // low-poly: flat colours, the facets do the shading
	UMaterialInterface* CliffGreyM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#8E8C98")), 0.9f);
	UMaterialInterface* LavaRockM = Tile(TEXT("T_AncCliffTile"), TEXT("T_AncLavaRockTile"), 600.f, 420.f, TEXT("#6A3A28"), TEXT("#FFC0A0"), 1.25f);
	UMaterialInterface* VolcRockM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#6E4A3E")), 0.9f);
	UMaterialInterface* MossM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#6CC24A")), 0.9f);
	UMaterialInterface* PaverM = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncPaverTile"), 420.f, 300.f, TEXT("#E2CFA6"));
	UMaterialInterface* StoneM = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncPaverTile"), 260.f, 220.f, TEXT("#D8C29A"), TEXT("#F0E2C8"), 1.0f);
	UMaterialInterface* BrickM = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncPaverTile"), 230.f, 230.f, TEXT("#CDB48E"), TEXT("#D8C4A8"), 0.95f);
	UMaterialInterface* LawnM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#5DBB3C")), 0.95f);
	UMaterialInterface* LawnFarM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#55B23A")), 0.95f);
	UMaterialInterface* CobbleM = Tile(TEXT("T_AncCobbleTile"), TEXT("T_AncCobbleTile"), 260.f, 260.f, TEXT("#5A3A8A"), TEXT("#C8A8FF"), 1.15f, 0.55f);
	UMaterialInterface* PurpleM = Tile(TEXT("T_AncPurpleTile"), TEXT("T_AncPurpleTile"), 320.f, 260.f, TEXT("#5B2FB0"), nullptr, 1.1f, 0.4f);
	UMaterialInterface* GoldM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F4C23A")), 0.28f, Hex(TEXT("#F2B830")), 0.35f, 0.9f);
	UMaterialInterface* GoldDarkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#C98E1E")), 0.35f, Hex(TEXT("#C98E1E")), 0.15f, 0.9f);
	UMaterialInterface* ClothM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#6A28C0")), 0.65f, Hex(TEXT("#6A28C0")), 0.15f);
	UMaterialInterface* ClothPinkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#B03AD8")), 0.6f, Hex(TEXT("#B03AD8")), 0.12f);
	UMaterialInterface* BlueGlowM = ZoogiUtil::GlowMID(this, Hex(TEXT("#47C8FF")), 2.2f);
	UMaterialInterface* BlueOrbM = ZoogiUtil::GlowMID(this, Hex(TEXT("#7FDcFF")), 2.6f);
	UMaterialInterface* CrystalM = ZoogiUtil::GlowMID(this, Hex(TEXT("#3FA8FF")), 1.6f);
	UMaterialInterface* CrystalLightM = ZoogiUtil::GlowMID(this, Hex(TEXT("#A8E6FF")), 2.2f);
	UMaterialInterface* LavaHotM = ZoogiUtil::GlowMID(this, Hex(TEXT("#FF9A1E")), 4.5f);
	UMaterialInterface* LavaEdgeM = ZoogiUtil::GlowMID(this, Hex(TEXT("#E0400A")), 2.5f);
	UMaterialInterface* DoorGlowM = ZoogiUtil::GlowMID(this, Hex(TEXT("#FF9A2A")), 2.4f);
	UMaterialInterface* PortalGlowM = ZoogiUtil::GlowMID(this, Hex(TEXT("#5FD8FF")), 2.6f);
	UMaterialInterface* OrbM = ZoogiUtil::GlowMID(this, Hex(TEXT("#8A3CFF")), 2.2f);
	UMaterialInterface* OrbCoreM = ZoogiUtil::GlowMID(this, Hex(TEXT("#D49CFF")), 2.2f);
	UMaterialInterface* WaterM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#38B6F2")), 0.12f, Hex(TEXT("#4FC8FF")), 0.3f);
	UMaterialInterface* SandM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#D8B27C")), 0.9f);
	UMaterialInterface* BarkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#8A6A44")), 0.85f);
	UMaterialInterface* LeafM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#3FA834")), 0.6f, Hex(TEXT("#3FA834")), 0.08f);
	UMaterialInterface* LeafM2 = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#62C23A")), 0.6f, Hex(TEXT("#62C23A")), 0.08f);
	UMaterialInterface* LeafBlueM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#3CC8E8")), 0.55f, Hex(TEXT("#3CC8E8")), 0.25f);
	UMaterialInterface* PlantPurpleM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#B02CD0")), 0.55f, Hex(TEXT("#C040E0")), 0.18f);
	UMaterialInterface* PlantTealM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#1FA88A")), 0.6f);
	UMaterialInterface* CreamM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F1E4C6")), 0.55f);
	UMaterialInterface* YellowM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F2C230")), 0.4f);
	UMaterialInterface* InkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#1C1A22")), 0.5f);
	UMaterialInterface* BannerDM = Decal(TEXT("T_AncBannerZ"), 0.15f);
	UMaterialInterface* MedalDM = Decal(TEXT("T_AncMedallionZ"), 0.1f);
	UStaticMesh* SphereM = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UPhysicalMaterial* RockPMat = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.45f, false);
	UPhysicalMaterial* WallPMat = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.6f, false);
	UPhysicalMaterial* BumpPMat = ZoogiPhysics::MakeMaterial(this, 0.2f, 1.0f, false);
	UPhysicalMaterial* StairPMat = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.2f, false);
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT v2: kit %s, decals %s"), TileBase ? TEXT("ok") : TEXT("MISSING"), (BannerDM && MedalDM) ? TEXT("ok") : TEXT("MISSING (geometry fallback)"));

	FActorSpawnParameters SpP; SpP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto Spawn = [&](const FString& Name, const FTransform& X = FTransform::Identity) -> AZoogiObstacle*
	{
		AZoogiObstacle* O = W->SpawnActor<AZoogiObstacle>(AZoogiObstacle::StaticClass(), X, SpP);
		if (O) { O->ObsName = FName(*Name); Obstacles.Add(O); }
		return O;
	};
	auto Vis = [&](const FZoogiMeshBuilder& M, UMaterialInterface* Mat, bool bShadow = true, USceneComponent* Parent = nullptr)
	{
		if (M.V.Num() == 0 || !Mat) return;
		if (UProceduralMeshComponent* C = AddFrozenProc(M, Mat, Parent, bShadow)) C->ComponentTags.Add(TEXT("NoCamBlock"));
	};
	auto MeshC = [&](UStaticMesh* M, UMaterialInterface* Mat, const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true, USceneComponent* Parent = nullptr)
	{
		if (!M) return (UStaticMeshComponent*)nullptr;
		UStaticMeshComponent* C = AddMesh(M, Mat, false, bShadow);
		if (!C) return C;
		if (Parent) C->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeLocation(Loc); C->SetRelativeRotation(Rot); C->SetRelativeScale3D(Scale);
		C->ComponentTags.Add(TEXT("NoCamBlock"));
		return C;
	};
	auto CylHull = [](TArray<TArray<FVector>>& H, const FVector& Base, float R, float Ht)
	{
		TArray<FVector>& P = H.AddDefaulted_GetRef();
		for (int32 j = 0; j < 12; ++j) { const float B = j * PI / 6.f; for (float Z : { 0.f, Ht }) P.Add(Base + FVector(FMath::Cos(B) * R, FMath::Sin(B) * R, Z)); }
	};
	auto BoxHull = [](TArray<TArray<FVector>>& H, const FVector& C, const FVector& Half, float Yaw)
	{
		TArray<FVector>& P = H.AddDefaulted_GetRef(); const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
		for (int32 i = 0; i < 8; ++i) P.Add(C + Q.RotateVector(FVector((i & 1) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 4) ? Half.Z : -Half.Z)));
	};
	// Z banner (texture), hanging from Top (centre of the rod), facing Yaw (+X of the yaw = the side the front faces)
	FZoogiMeshBuilder BannerFallback, BannerFallbackGold, BannerTex;
	auto Banner = [&](const FVector& Top, float Yaw, float Wd)
	{
		const float Ht = Wd * (1024.f / 768.f);
		const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion(); const FVector X = Q.RotateVector(FVector::RightVector);
		const FVector A = Top - X * Wd * 0.5f, B = Top + X * Wd * 0.5f;
		if (BannerDM) { UVQuad(BannerTex, A - FVector(0, 0, Ht), B - FVector(0, 0, Ht), B, A); return; }
		const FVector N = Q.RotateVector(FVector::ForwardVector);
		Quad2(BannerFallback, A - FVector(0, 0, Ht * 0.85f), B - FVector(0, 0, Ht * 0.85f), B, A, N);
		BannerFallbackGold.Box(Top, FVector(4.f, Wd * 0.55f, 4.f), FRotator(0.f, Yaw, 0.f));
	};
	auto Balustrade = [&](FZoogiMeshBuilder& Stone, FZoogiMeshBuilder& Gold, const FVector2D& A, const FVector2D& B, float Z, float H)
	{
		const FVector2D D = (B - A).GetSafeNormal(); const float Len = FVector2D::Distance(A, B);
		const float Yw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		const FVector2D Mid = (A + B) * 0.5f;
		Stone.Box(FVector(Mid, Z + 14.f), FVector(Len * 0.5f, 22.f, 14.f), FRotator(0.f, Yw, 0.f));                  // plinth
		Gold.Box(FVector(Mid, Z + H - 7.f), FVector(Len * 0.5f + 6.f, 16.f, 7.f), FRotator(0.f, Yw, 0.f));          // top rail
		for (float t = 0.f; t <= Len + 1.f; t += 55.f)
		{
			const FVector2D P = A + D * FMath::Min(t, Len);
			const bool bPost = FMath::RoundToInt(t / 55.f) % 6 == 0;
			if (bPost) { Stone.Box(FVector(P, Z + H * 0.5f), FVector(16.f, 16.f, H * 0.5f + 4.f), FRotator(0.f, Yw, 0.f)); Gold.Box(FVector(P, Z + H + 8.f), FVector(12.f, 12.f, 8.f), FRotator(0.f, Yw, 0.f)); }
			else Gold.Prism(P, 6, 6.f, 5.f, Z + 28.f, Z + H - 14.f, 0.f);
		}
	};
	auto Lamp = [&](FZoogiMeshBuilder& Stone, FZoogiMeshBuilder& Gold, const FVector& Base, float H, float OrbR)
	{
		Stone.Prism(FVector2D(Base), 8, 58.f, 50.f, Base.Z, Base.Z + H * 0.35f, 22.5f);
		Gold.Prism(FVector2D(Base), 8, 62.f, 62.f, Base.Z + H * 0.35f, Base.Z + H * 0.35f + 10.f, 22.5f);
		Gold.Prism(FVector2D(Base), 8, 16.f, 12.f, Base.Z + H * 0.35f + 10.f, Base.Z + H - OrbR * 0.9f, 22.5f);
		Gold.Prism(FVector2D(Base), 10, OrbR * 0.55f, OrbR * 0.95f, Base.Z + H - OrbR * 1.2f, Base.Z + H - OrbR * 0.55f, 0.f);
		MeshC(SphereM, BlueOrbM, Base + FVector(0, 0, H), FVector(OrbR / 50.f), FRotator::ZeroRotator, false);
	};

	// ------------------------------------------------------------------ sky, sun, fog
	if (AddPanoramaSky(TEXT("AncientDusk"), 1.0f, 0.f, FLinearColor::White))
	{
		if (Built.Num()) if (USceneComponent* Sky = Cast<USceneComponent>(Built.Last().Get())) Sky->SetRelativeRotation(FRotator(0.f, 0.f, 0.f));
	}
	else AddPanoramaSky(TEXT("Ancient"), 1.0f, 0.f, FLinearColor::White);
	if (Sun)
	{
		Sun->SetRelativeRotation(FRotator(-24.f, 200.f, 0.f));    // golden-hour sun behind-right of the match camera: warm front light (Grok review)
		Sun->SetLightColor(Hex(TEXT("#FFD9A8")));
		Sun->SetIntensity(13.f);
	}
	if (SkyLight) { SkyLight->SetIntensity(1.6f); SkyLight->SetLightColor(Hex(TEXT("#E4DCFF"))); }
	if (Fog)
	{
		Fog->SetFogInscatteringColor(Hex(TEXT("#D9CCF2")));
		Fog->SetFogDensity(0.0015f);
		Fog->SetStartDistance(15000.f);
	}

	if (Post) // golden-hour grade: a touch brighter, warmer, more saturated, softer bloom (ref is a vivid sunset render)
	{
		FPostProcessSettings& P = Post->Settings;
		P.bOverride_AutoExposureBias = true; P.AutoExposureBias += 0.35f;
		P.bOverride_BloomIntensity = true; P.BloomIntensity = 1.25f;
		P.bOverride_ColorSaturation = true; P.ColorSaturation = FVector4(1.04f, 1.04f, 1.02f, 1.06f);
		P.bOverride_ColorGain = true; P.ColorGain = FVector4(1.0f, 1.0f, 0.98f, 1.f);
		P.bOverride_VignetteIntensity = true; P.VignetteIntensity = 0.25f;
	}

	// ------------------------------------------------------------------ MEADOW GROUND + RIVERS
	{
		FZoogiMeshBuilder G, GF;
		auto Disc = [](FZoogiMeshBuilder& M, float R0, float R1, float Z, int32 N)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N;
				const FVector a0(FMath::Cos(A0) * R0, FMath::Sin(A0) * R0, Z), a1(FMath::Cos(A1) * R0, FMath::Sin(A1) * R0, Z);
				const FVector b0(FMath::Cos(A0) * R1, FMath::Sin(A0) * R1, Z), b1(FMath::Cos(A1) * R1, FMath::Sin(A1) * R1, Z);
				if (R0 <= 1.f) M.FlatTri(FVector(0, 0, Z), b0, b1, FVector::UpVector); else M.Quad(a0, a1, b1, b0, FVector::UpVector);
			}
		};
		Disc(G, 0.f, 9000.f, GZ, 96);
		Disc(GF, 9000.f, 45000.f, GZ - 2.f, 96);
		Vis(G, LawnM, false); Vis(GF, LawnFarM, false);
		// rivers: winding ribbons with sandy banks (ref: blue streams in the foreground + around the islands)
		auto River = [&](const TArray<FVector2D>& Pts, float Wd)
		{
			FZoogiMeshBuilder Wt, Bk;
			TArray<FVector2D> S;    // Catmull-Rom resample
			for (int32 i = 0; i + 1 < Pts.Num(); ++i)
			{
				const FVector2D P0 = Pts[FMath::Max(0, i - 1)], P1 = Pts[i], P2 = Pts[i + 1], P3 = Pts[FMath::Min(Pts.Num() - 1, i + 2)];
				for (int32 k = 0; k < 8; ++k)
				{
					const float t = k / 8.f, t2 = t * t, t3 = t2 * t;
					S.Add(0.5f * ((2.f * P1) + (-P0 + P2) * t + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * t2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * t3));
				}
			}
			S.Add(Pts.Last());
			for (int32 i = 0; i + 1 < S.Num(); ++i)
			{
				const FVector2D D0 = (S[FMath::Min(i + 1, S.Num() - 1)] - S[FMath::Max(i - 1, 0)]).GetSafeNormal();
				const FVector2D D1 = (S[FMath::Min(i + 2, S.Num() - 1)] - S[i]).GetSafeNormal();
				const FVector2D N0(-D0.Y, D0.X), N1(-D1.Y, D1.X);
				const float W0 = Wd * (0.85f + 0.2f * FMath::Sin(i * 0.37f)), W1 = Wd * (0.85f + 0.2f * FMath::Sin((i + 1) * 0.37f));
				Wt.Quad(FVector(S[i] - N0 * W0, GZ + 10.f), FVector(S[i + 1] - N1 * W1, GZ + 10.f), FVector(S[i + 1] + N1 * W1, GZ + 10.f), FVector(S[i] + N0 * W0, GZ + 10.f), FVector::UpVector);
				Bk.Quad(FVector(S[i] - N0 * (W0 + 70.f), GZ + 8.f), FVector(S[i + 1] - N1 * (W1 + 70.f), GZ + 8.f), FVector(S[i + 1] + N1 * (W1 + 70.f), GZ + 8.f), FVector(S[i] + N0 * (W0 + 70.f), GZ + 8.f), FVector::UpVector);
			}
			Vis(Bk, SandM, false); Vis(Wt, WaterM, false);
		};
		River({ { -9000.f, 900.f }, { -6200.f, 1700.f }, { -4300.f, 2600.f }, { -2900.f, 3100.f }, { -1900.f, 3900.f }, { -1500.f, 6000.f }, { -2400.f, 9000.f } }, 230.f);
		River({ { 9000.f, 700.f }, { 6300.f, 1500.f }, { 4300.f, 2500.f }, { 2900.f, 3100.f }, { 1900.f, 3900.f }, { 1500.f, 6000.f }, { 2600.f, 9000.f } }, 230.f);
		River({ { -16000.f, 6200.f }, { -9000.f, 5600.f }, { -6200.f, 6400.f }, { -3600.f, 7600.f }, { -1500.f, 9800.f }, { -800.f, 16000.f } }, 260.f);
		River({ { 16000.f, 5200.f }, { 9500.f, 5900.f }, { 6600.f, 6900.f }, { 4200.f, 7800.f }, { 2200.f, 9400.f }, { 1200.f, 16000.f } }, 240.f);
		River({ { -6400.f, -9000.f }, { -6900.f, -4000.f }, { -6300.f, 0.f }, { -7400.f, 3500.f }, { -9000.f, 5600.f } }, 200.f);
		River({ { 6500.f, -9500.f }, { 7000.f, -4500.f }, { 6400.f, 0.f }, { 7600.f, 3000.f }, { 9500.f, 5900.f } }, 200.f);
	}

	// ------------------------------------------------------------------ LOW-POLY tree clumps on the plain (the mesas / arches / waterfalls are in the low-poly sky)
	{
		FRandomStream R(77);
		// low-poly tree clumps across the plain
		FZoogiMeshBuilder TrK, TrC;
		for (int32 i = 0; i < 120; ++i)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(8000.f, 15000.f);
			const FVector2D C(FMath::Cos(A) * D, FMath::Sin(A) * D);
			const float S = R.FRandRange(160.f, 300.f);
			TrK.Prism(C, 5, S * 0.12f, S * 0.08f, GZ, GZ + S * 1.1f, 0.f);
			TrC.Prism(C, 6, S, S * 0.15f, GZ + S * 0.8f, GZ + S * 2.4f, R.FRandRange(0.f, 60.f));
		}
		Vis(TrK, BarkM, false); Vis(TrC, LeafM, false);
	}

	// ------------------------------------------------------------------ WATER CHANNELS around the plaza and islands (poly ref: blue water between everything)
	{
		FZoogiMeshBuilder Wt, Gr, Dirt;
		// water sheet r 2100 .. 8000 just above the ground; grass skirts / front lawn sit above it
		for (int32 i = 0; i < 64; ++i)
		{
			const float A0 = 2.f * PI * i / 64, A1 = 2.f * PI * (i + 1) / 64;
			const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
			Wt.Quad(FVector(D0 * 2100.f, GZ + 3.f), FVector(D1 * 2100.f, GZ + 3.f), FVector(D1 * 8000.f, GZ + 3.f), FVector(D0 * 8000.f, GZ + 3.f), FVector::UpVector);
		}
		// grass ring round the plaza foot
		for (int32 i = 0; i < 48; ++i)
		{
			const float A0 = 2.f * PI * i / 48, A1 = 2.f * PI * (i + 1) / 48;
			const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
			const float R0 = 2500.f + 120.f * FMath::Sin(i * 1.3f), R1 = 2500.f + 120.f * FMath::Sin((i + 1) * 1.3f);
			Gr.Quad(FVector(D0 * 1900.f, GZ + 6.f), FVector(D1 * 1900.f, GZ + 6.f), FVector(D1 * R1, GZ + 6.f), FVector(D0 * R0, GZ + 6.f), FVector::UpVector);
		}
		// grass skirts round every island foot + the front lawn (the camera side) with its dirt paths
		for (const FPlat& P : L.Plats)
		{
			if (P.Region == Hub) continue;
			FVector2D C(0, 0); for (const FVector2D& V : P.P) C += V; C /= P.P.Num();
			for (int32 k = 0; k < P.P.Num(); ++k)
			{
				const FVector2D A = P.P[k], B = P.P[(k + 1) % P.P.Num()];
				Gr.FlatTri(FVector(C, GZ + 6.f), FVector(C + (A - C) * 1.28f, GZ + 6.f), FVector(C + (B - C) * 1.28f, GZ + 6.f), FVector::UpVector);
			}
		}
		{
			const FVector2D F[] = { { -2600.f, 1800.f }, { 2600.f, 1800.f }, { 3600.f, 5200.f }, { 2200.f, 9000.f }, { -2200.f, 9000.f }, { -3600.f, 5200.f } };
			for (int32 k = 0; k < 6; ++k) Gr.FlatTri(FVector(0.f, 4500.f, GZ + 6.f), FVector(F[k], GZ + 6.f), FVector(F[(k + 1) % 6], GZ + 6.f), FVector::UpVector);
			// dirt paths curling left and right from the brick path (poly ref foreground)
			for (int32 s = -1; s <= 1; s += 2)
			{
				FVector2D Prev(s * 300.f, 4200.f);
				for (int32 k = 1; k <= 10; ++k)
				{
					const float t = k / 10.f;
					const FVector2D Q(s * (300.f + 3600.f * t), 4200.f + 900.f * FMath::Sin(t * PI * 0.8f) + 600.f * t);
					const FVector2D D = (Q - Prev).GetSafeNormal(), N(-D.Y, D.X);
					Dirt.Quad(FVector(Prev - N * 150.f, GZ + 12.f), FVector(Q - N * 150.f, GZ + 12.f), FVector(Q + N * 150.f, GZ + 12.f), FVector(Prev + N * 150.f, GZ + 12.f), FVector::UpVector);
					Prev = Q;
				}
			}
		}
		Vis(Wt, WaterM, false); Vis(Gr, LawnM, false); Vis(Dirt, SandM, false);
	}

	// ------------------------------------------------------------------ ROCK MESAS under every floor (the islands stand in the meadow)
	{
		FZoogiMeshBuilder Cl, Hb;
		int32 Seed = 500;
		for (const FPlat& P : L.Plats)
		{
			if (P.Region == Hub)
			{
				// the plaza: dressed masonry drum with a gold band, then a rubble foot into the grass
				Hb.Prism(FVector2D::ZeroVector, 48, L.HubR / FMath::Cos(PI / 32.f) + 6.f, L.HubR / FMath::Cos(PI / 32.f) + 30.f, GZ - 10.f, P.Z - P.Thick + 2.f, 0.f);
				TArray<FVector2D> Ring; for (int32 i = 0; i < 32; ++i) { const float A = 2.f * PI * (i + 0.5f) / 32; Ring.Add(FVector2D(FMath::Cos(A), FMath::Sin(A)) * (L.HubR + 40.f)); }
				CliffSkirt(Cl, Ring, GZ + 140.f, GZ - 20.f, 60.f, 160.f, Seed++, 300.f);
				continue;
			}
			CliffSkirt(Cl, P.P, P.Z - P.Thick + 2.f, GZ - 20.f, 70.f, P.Z > 300.f ? 260.f : 180.f, Seed++);
		}
		for (const FIslet& I : L.Islets)
		{
			TArray<FVector2D> Oct; for (int32 k = 0; k < 8; ++k) { const float A = (k + 0.5f) * PI / 4.f; Oct.Add(FVector2D(I.C) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (I.R / FMath::Cos(PI / 8.f))); }
			CliffSkirt(Cl, Oct, I.C.Z - 58.f, GZ - 20.f, 40.f, 90.f, Seed++, 200.f);
		}
		Vis(Cl, CliffM); Vis(Hb, StoneM);
		// gold band + dark trim on the plaza rim face
		FZoogiMeshBuilder Bd; Bd.ArcSlab(L.HubR + 2.f, L.HubR + 12.f, 0.f, 360.f, -34.f, -14.f, 96);
		Vis(Bd, GoldM, false);
	}

	// ------------------------------------------------------------------ VOLCANO on the arched drum, crown + orb (centrepiece)
	{
		FRandomStream R(907);
		constexpr int32 NR = 6, NS = 14;
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r <= NR; ++r)
		{
			TArray<FVector>& Rg = Rings.AddDefaulted_GetRef();
			const float Z = DrumH - 20.f + (PeakZ - DrumH + 20.f) * r / NR;
			for (int32 s = 0; s < NS; ++s)
			{
				const float A = 2.f * PI * s / NS + (r % 2) * PI / NS;
				const float J = r == NR ? 0.f : R.FRandRange(-0.16f, 0.06f) - 0.05f * FMath::Abs(FMath::Sin(A * 3.f + r));
				const float Rad = r == 0 ? BaseR * 0.98f : MtnR(Z) * (1.f + J);
				Rg.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rad + FVector(0, 0, Z + (r > 0 && r < NR ? R.FRandRange(-70.f, 70.f) : 0.f)));
			}
		}
		FZoogiMeshBuilder Mtn, Lip, Pool;
		for (int32 r = 0; r < NR; ++r)
			for (int32 s = 0; s < NS; ++s)
			{
				const FVector& A0 = Rings[r][s]; const FVector& A1 = Rings[r][(s + 1) % NS]; const FVector& B0 = Rings[r + 1][s]; const FVector& B1 = Rings[r + 1][(s + 1) % NS];
				Mtn.Quad(A0, A1, B1, B0, (FVector((A0 + A1) * 0.5f).GetSafeNormal2D() + FVector(0, 0, 0.45f)).GetSafeNormal());
			}
		for (int32 s = 0; s < NS; ++s)
		{
			const FVector& A0 = Rings[NR][s]; const FVector& A1 = Rings[NR][(s + 1) % NS];
			const FVector I0 = FVector(FVector2D(A0) * 0.72f, PeakZ - 70.f), I1 = FVector(FVector2D(A1) * 0.72f, PeakZ - 70.f);
			Lip.Quad(A0, A1, I1, I0, FVector(0, 0, 1)); Pool.FlatTri(FVector(0, 0, PeakZ - 60.f), I0, I1, FVector::UpVector);
		}
		Vis(Mtn, VolcRockM); Vis(Lip, VolcRockM); Vis(Pool, LavaHotM, false);
		// lava channels: wide glowing cracks with a hot core, branching (ref: bright yellow-orange rivers down every face)
		FZoogiMeshBuilder Hot, Edge;
		for (int32 k = 0; k < 9; ++k)
		{
			const float A0 = 360.f / 9.f * k + 8.f + (k % 2) * 9.f;
			const float TStart = k % 3 == 0 ? 1.f : R.FRandRange(0.55f, 0.85f);
			FVector PrevL, PrevR, PrevLe, PrevRe; bool bHave = false;
			for (int32 i = 0; i <= 9; ++i)
			{
				const float T = TStart * (1.f - i / 9.f);
				const float Z = DrumH + (PeakZ - DrumH) * T - 20.f;
				const float A = FMath::DegreesToRadians(A0 + ((i + k) % 2 ? 2.f : -2.f) + 3.f * FMath::Sin(k * 1.7f)); // lightning zig-zag (poly ref)
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f), Tan(-Dir.Y, Dir.X, 0.f);
				const float Wd = FMath::Lerp(12.f, 46.f, 1.f - T) * (0.85f + 0.15f * FMath::Sin(i * 0.9f + k * 2.f));
				const FVector C = Dir * (MtnR(Z) * 1.03f + 8.f) + FVector(0, 0, Z);
				const FVector Lp = C - Tan * Wd, Rp = C + Tan * Wd, Le = C - Tan * (Wd * 2.2f + 16.f) - Dir * 3.f, Re = C + Tan * (Wd * 2.2f + 16.f) - Dir * 3.f;
				const FVector Nrm = (Dir + FVector(0, 0, 0.5f)).GetSafeNormal();
				if (bHave) { Hot.Quad(PrevL, PrevR, Rp, Lp, Nrm); Edge.Quad(PrevLe, PrevRe, Re, Le, Nrm); }
				PrevL = Lp; PrevR = Rp; PrevLe = Le; PrevRe = Re; bHave = true;
			}
		}
		Vis(Edge, LavaEdgeM, false); Vis(Hot, LavaHotM, false);
		// jungle ledges on the flanks: green tufts + acacia trees (low-poly)
		FZoogiMeshBuilder Tufts;
		for (int32 k = 0; k < 16; ++k)
		{
			const float A = 2.f * PI * k / 16.f + R.FRandRange(-0.12f, 0.12f) + 0.2f;
			const float Z = R.FRandRange(DrumH + 40.f, 980.f);
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector C = D * (MtnR(Z) * 0.98f) + FVector(0, 0, Z);
			for (int32 j = 0; j < 3; ++j) { const FVector Q = C + FVector(R.FRandRange(-60.f, 60.f), R.FRandRange(-60.f, 60.f), R.FRandRange(0.f, 40.f)); Tufts.Prism(FVector2D(Q), 6, R.FRandRange(60.f, 95.f), 20.f, Q.Z, Q.Z + R.FRandRange(50.f, 90.f), R.FRandRange(0.f, 60.f)); }
		}
		{
			FZoogiMeshBuilder Ld, Tk, Canopy;
			const float LedgeZ[] = { 900.f, 900.f, 1350.f, 1350.f };
			const float LedgeA[] = { 55.f, 125.f, 15.f, 165.f };
			FVector2D LedgeC[4];
			for (int32 i = 0; i < 4; ++i) LedgeC[i] = FVector2D(FMath::Cos(FMath::DegreesToRadians(LedgeA[i])), FMath::Sin(FMath::DegreesToRadians(LedgeA[i]))) * (MtnR(LedgeZ[i]) * 0.95f);
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector2D C = LedgeC[i]; const float Z = LedgeZ[i];
				Ld.Prism(C, 7, 120.f, 210.f, Z - 90.f, Z, 17.f * i); Ld.Prism(C, 7, 30.f, 120.f, Z - 300.f, Z - 90.f, 17.f * i);
				Tk.Prism(C, 7, 18.f, 13.f, Z, Z + 230.f, 0.f);
				UMaterialInterface* Can = i < 2 ? LeafBlueM : LeafM;
				(void)Can;
				Canopy.Prism(C, 7, 300.f, 250.f, Z + 225.f, Z + 270.f, 10.f * i); Canopy.Prism(C + FVector2D(30.f, -20.f), 7, 210.f, 120.f, Z + 270.f, Z + 320.f, 20.f + 10.f * i);
			}
			Vis(Ld, VolcRockM); Vis(Tk, BarkM); Vis(Canopy, LeafM2); Vis(Tufts, LeafM);
		}
		// the drum: dressed stone wall with arched glowing doorways, gold trims, Z banners
		{
			FZoogiMeshBuilder Dr, Tr, Arch, ArchF, Door;
			Dr.Prism(FVector2D::ZeroVector, 48, DrumR, DrumR - 10.f, -2.f, DrumH, 0.f);
			Tr.ArcSlab(DrumR - 2.f, DrumR + 14.f, 0.f, 360.f, -2.f, 22.f, 96);
			const int32 ND = 8;
			for (int32 d = 0; d < ND; ++d)
			{
				const float Ad = 90.f + 360.f / ND * d;     // door 0 faces +Y (the match camera)
				const FVector Dir = FRotator(0.f, Ad, 0.f).Vector(), Tg(-Dir.Y, Dir.X, 0.f);
				const float Wd = d == 0 ? 150.f : 100.f, Hd = d == 0 ? 250.f : 190.f;
				const FVector Base = Dir * (DrumR + 2.f);
				// arch outline: rectangle + semicircle, as a fan
				TArray<FVector> Pts;
				Pts.Add(Base - Tg * Wd + FVector(0, 0, 0.f));
				for (int32 a = 0; a <= 12; ++a) { const float An = PI * a / 12.f; Pts.Add(Base - Tg * (Wd * FMath::Cos(An)) + FVector(0, 0, Hd - Wd + Wd * FMath::Sin(An))); }
				Pts.Add(Base + Tg * Wd);
				for (int32 a = 0; a + 1 < Pts.Num(); ++a) Door.FlatTri(Base + FVector(0, 0, 4.f), Pts[a], Pts[a + 1], Dir);
				for (int32 a = 0; a + 1 < Pts.Num(); ++a) // gold frame
				{
					const FVector O0 = Pts[a] + (Pts[a] - (Base + FVector(0, 0, Hd * 0.5f))).GetSafeNormal() * 18.f + Dir * 4.f;
					const FVector O1 = Pts[a + 1] + (Pts[a + 1] - (Base + FVector(0, 0, Hd * 0.5f))).GetSafeNormal() * 18.f + Dir * 4.f;
					ArchF.Quad(Pts[a] + Dir * 4.f, Pts[a + 1] + Dir * 4.f, O1, O0, Dir);
				}
				if (d != 0) Banner(Base + Dir * 8.f + Tg * (Wd + 70.f) + FVector(0, 0, DrumH - 30.f), Ad, 90.f);
				else { Banner(Base + Dir * 8.f + Tg * (Wd + 85.f) + FVector(0, 0, DrumH - 20.f), Ad, 100.f); Banner(Base + Dir * 8.f - Tg * (Wd + 85.f) + FVector(0, 0, DrumH - 20.f), Ad, 100.f); }
				Arch.Box(Base + FVector(0, 0, Hd + 40.f) + Dir * 10.f, FVector(14.f, 40.f, 18.f), FRotator(0.f, Ad, 0.f));   // keystone
			}
			Vis(Dr, StoneM); Vis(Tr, GoldM, false); Vis(ArchF, GoldM, false); Vis(Arch, GoldM, false);
			// doors: front one cyan portal glow, others warm lava glow
			FZoogiMeshBuilder DoorFront, DoorRest;
			{
				// rebuild split by door index for two materials (cheap: re-run the fan for door 0 only)
				const FVector Dir = FRotator(0.f, 90.f, 0.f).Vector(), Tg(-Dir.Y, Dir.X, 0.f); const FVector Base = Dir * (DrumR + 5.f);
				TArray<FVector> Pts; Pts.Add(Base - Tg * 150.f);
				for (int32 a = 0; a <= 12; ++a) { const float An = PI * a / 12.f; Pts.Add(Base - Tg * (150.f * FMath::Cos(An)) + FVector(0, 0, 100.f + 150.f * FMath::Sin(An))); }
				Pts.Add(Base + Tg * 150.f);
				for (int32 a = 0; a + 1 < Pts.Num(); ++a) DoorFront.FlatTri(Base + FVector(0, 0, 4.f), Pts[a], Pts[a + 1], Dir);
			}
			Vis(Door, DoorGlowM, false); Vis(DoorFront, PortalGlowM, false);
			// cantilevered ring walkway on the drum (visual; underside at z ~260 clears every body on the plaza)
			FZoogiMeshBuilder Wk, WkF;
			Wk.ArcSlab(DrumR - 60.f, 1250.f, 0.f, 360.f, DrumH - 30.f, DrumH + 6.f, 96);
			WkF.ArcSlab(1236.f, 1252.f, 0.f, 360.f, DrumH - 120.f, DrumH - 26.f, 96);
			Vis(Wk, PaverM); Vis(WkF, StoneM);
			FZoogiMeshBuilder BS, BG;
			for (int32 k = 0; k < 48; ++k)
			{
				const float A0 = 2.f * PI * k / 48.f, A1 = 2.f * PI * (k + 1) / 48.f;
				Balustrade(BS, BG, FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * 1225.f, FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * 1225.f, DrumH + 6.f, 85.f);
			}
			// corbels under the walkway
			for (int32 k = 0; k < 32; ++k)
			{
				const float A = 2.f * PI * (k + 0.5f) / 32.f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
				BS.Box(D * 1110.f + FVector(0, 0, DrumH - 70.f), FVector(130.f, 18.f, 40.f), FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
			}
			Vis(BS, StoneM); Vis(BG, GoldM, false);
		}
		// collision: the cone + the drum
		TArray<TArray<FVector>> H; TArray<FVector>& Pts = H.AddDefaulted_GetRef();
		for (int32 s = 0; s < 16; ++s) { const float A = 2.f * PI * s / 16; Pts.Add(FVector(FMath::Cos(A) * 927.f, FMath::Sin(A) * 927.f, -10.f)); Pts.Add(FVector(FMath::Cos(A) * TopR * 1.05f, FMath::Sin(A) * TopR * 1.05f, PeakZ + 150.f)); }
		CylHull(H, FVector(0, 0, -10.f), DrumR + 8.f, DrumH + 10.f);
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_volcano"))) { O->BuildCollider(H, 1.f, RockPMat); O->BounceGain = 1.1f; O->MaxOut = 1800.f; }
		SolidPosts.Add(FVector2D::ZeroVector);
		// crown: gold band + tall gold spikes (front ones tallest) + jewels; Z banners hang off the band
		FZoogiMeshBuilder Band, Sp, Jw;
		Band.Prism(FVector2D::ZeroVector, 20, TopR + 40.f, TopR + 70.f, PeakZ - 50.f, PeakZ + 150.f, 0.f);
		Band.Prism(FVector2D::ZeroVector, 20, TopR + 80.f, TopR + 80.f, PeakZ + 120.f, PeakZ + 160.f, 0.f);
		for (int32 k = 0; k < 9; ++k)
		{
			const float A = 2.f * PI * k / 9.f + PI * 0.5f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f), T2(-D.Y, D.X, 0.f);
			const FVector B = D * (TopR + 62.f) + FVector(0, 0, PeakZ + 150.f);
			const float Front = FMath::Max(0.f, D.Y);                       // +Y = toward the camera
			const float Hs = (k % 2 ? 300.f : 430.f) + 160.f * Front, Ws = 62.f;
			const FVector Tip = B + D * (Hs * 0.36f) + FVector(0, 0, Hs);
			const FVector P0 = B - T2 * Ws - D * 22.f, P1 = B + T2 * Ws - D * 22.f, P2 = B + T2 * Ws + D * 30.f, P3 = B - T2 * Ws + D * 30.f;
			Sp.FlatTri(P0, P1, Tip, -D); Sp.FlatTri(P1, P2, Tip, T2); Sp.FlatTri(P2, P3, Tip, D); Sp.FlatTri(P3, P0, Tip, -T2);
			Jw.Box(B + D * 36.f + FVector(0, 0, -60.f), FVector(10.f, 22.f, 22.f), FRotator(45.f, FMath::RadiansToDegrees(A), 0.f));
			if (k % 3 == 0) Banner(D * (TopR + 92.f) + FVector(0, 0, PeakZ + 110.f), FMath::RadiansToDegrees(A), 110.f);
		}
		Vis(Band, GoldM); Vis(Sp, GoldM); Vis(Jw, ZoogiUtil::GlowMID(this, Hex(TEXT("#C050FF")), 1.6f), false);
		const float OZ = PeakZ + 470.f;
		MeshC(SphereM, OrbM, FVector(0, 0, OZ), FVector(5.4f), FRotator::ZeroRotator, false);
		MeshC(SphereM, OrbCoreM, FVector(0, 0, OZ), FVector(3.3f), FRotator::ZeroRotator, false);
		{
			USceneComponent* Pv = NewPivot(FVector(0, 0, OZ)); Pv->SetRelativeRotation(FRotator(-18.f, 20.f, 8.f));
			FZoogiMeshBuilder Rg; Rg.ArcSlab(330.f, 362.f, 0.f, 360.f, -12.f, 12.f, 64);
			Vis(Rg, GoldM, false, Pv);
		}
		FZoogiMeshBuilder Stem; Stem.Prism(FVector2D::ZeroVector, 10, 60.f, 30.f, PeakZ - 60.f, OZ - 250.f, 0.f);
		Vis(Stem, GoldDarkM, false);
	}

	// ------------------------------------------------------------------ PLAZA (hub) decor
	{
		FZoogiMeshBuilder Cob, Gd, Spw;
		Cob.ArcSlab(1560.f, 1770.f, 0.f, 360.f, 0.f, 1.2f, 128);
		Gd.ArcSlab(1535.f, 1560.f, 0.f, 360.f, 0.f, 1.6f, 128); Gd.ArcSlab(1770.f, 1795.f, 0.f, 360.f, 0.f, 1.6f, 128);
		Gd.ArcSlab(1268.f, 1290.f, 0.f, 360.f, 0.f, 1.6f, 128);
		Vis(Cob, CobbleM, false); Vis(Gd, GoldM, false);
		// blue spawn rings (cardinals r1100) + a few small glow discs like the ref
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = FMath::DegreesToRadians(90.f * k);
			const FVector2D C(FMath::Cos(A) * 1100.f, FMath::Sin(A) * 1100.f);
			for (int32 i = 0; i < 24; ++i)
			{
				const float B0 = 2.f * PI * i / 24, B1 = 2.f * PI * (i + 1) / 24;
				const FVector2D D0(FMath::Cos(B0), FMath::Sin(B0)), D1(FMath::Cos(B1), FMath::Sin(B1));
				Spw.Quad(FVector(C + D0 * 92.f, 1.8f), FVector(C + D1 * 92.f, 1.8f), FVector(C + D1 * 112.f, 1.8f), FVector(C + D0 * 112.f, 1.8f), FVector::UpVector);
				Spw.Quad(FVector(C + D0 * 40.f, 1.8f), FVector(C + D1 * 40.f, 1.8f), FVector(C + D1 * 52.f, 1.8f), FVector(C + D0 * 52.f, 1.8f), FVector::UpVector);
			}
		}
		Vis(Spw, BlueGlowM, false);
		// Z medallion in front of the volcano (toward the camera)
		FZoogiMeshBuilder Md;
		if (MedalDM) UVDisc(Md, FVector2D(0.f, 1420.f), 450.f, 2.2f);
		else { Md.Prism(FVector2D(0.f, 1450.f), 32, 340.f, 340.f, 0.f, 2.2f); }
		Vis(Md, MedalDM ? MedalDM : ClothM, false);
		// rim lamp posts (the old drum-pillar colliders, r1790 at 22.5 + 45k): stone pedestal (collision), gold post, blue orb
		TArray<TArray<FVector>> PH;
		FZoogiMeshBuilder LS, LG;
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = FMath::DegreesToRadians(22.5f + 45.f * k); const FVector2D C(FMath::Cos(A) * 1790.f, FMath::Sin(A) * 1790.f);
			CylHull(PH, FVector(C, 0.f), 62.f, 150.f);
			SolidPosts.Add(C);
			Lamp(LS, LG, FVector(C, 0.f), 330.f, 30.f);
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_hub_lamps"))) { O->BuildCollider(PH, 1.f, WallPMat); O->BounceGain = 1.15f; O->MaxOut = 1800.f; O->CenterBias = 0.65f; }
		Vis(LS, StoneM); Vis(LG, GoldM);
	}

	// ------------------------------------------------------------------ FRONT GATE + STAIRS + PATH (toward the match camera, +Y)
	{
		const float Y0 = L.HubR - 20.f, YLand = 2150.f, YBot = 3250.f;
		const float HalfW = 300.f;
		// collision: landing + stair slope down to z -500 (a ball that rolls down is out a moment later)
		TArray<TArray<FVector>> SH;
		BoxHull(SH, FVector(0.f, (Y0 + YLand) * 0.5f, -20.f), FVector(HalfW + 40.f, (YLand - Y0) * 0.5f, 20.f), 0.f);
		{
			const float YCut = YLand + (YBot - YLand) * (500.f / 640.f);
			TArray<FVector>& P = SH.AddDefaulted_GetRef();
			for (int32 s = -1; s <= 1; s += 2)
			{
				P.Add(FVector(s * HalfW, YLand, 0.f)); P.Add(FVector(s * HalfW, YLand, -40.f));
				P.Add(FVector(s * HalfW, YCut, -500.f)); P.Add(FVector(s * HalfW, YCut, -540.f));
			}
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_front_stairs"))) O->BuildCollider(SH, 1.f, StairPMat);
		FZoogiMeshBuilder St, StS, Gd, GS;
		St.Box(FVector(0.f, (Y0 + YLand) * 0.5f, -20.f), FVector(HalfW + 40.f, (YLand - Y0) * 0.5f, 20.f));
		const int32 NStep = 16; const float StepD = (YBot - YLand) / NStep, StepH = 640.f / NStep;
		for (int32 i = 0; i < NStep; ++i)
		{
			const float Yc = YLand + StepD * (i + 0.5f), Zt = -StepH * (i + 1) + 2.f;
			St.Box(FVector(0.f, Yc, (Zt + GZ) * 0.5f), FVector(HalfW, StepD * 0.5f + 1.f, (Zt - GZ) * 0.5f));
		}
		// side walls of the stair with gold copings
		for (int32 s = -1; s <= 1; s += 2)
		{
			StS.Quad(FVector(s * (HalfW + 40.f), YLand, 30.f), FVector(s * (HalfW + 40.f), YBot, GZ + 70.f), FVector(s * (HalfW + 40.f), YBot, GZ), FVector(s * (HalfW + 40.f), YLand, GZ), FVector((float)s, 0.f, 0.f));
		}
		Vis(St, BrickM); Vis(StS, StoneM);
		// sloped gold coping along each side
		for (int32 s = -1; s <= 1; s += 2)
		{
			FZoogiMeshBuilder Cp;
			const FVector A(s * (HalfW + 20.f), YLand, 34.f), B(s * (HalfW + 20.f), YBot, GZ + 74.f);
			const FVector Sd(1.f, 0.f, 0.f);
			Cp.Quad(A - Sd * 26.f, B - Sd * 26.f, B + Sd * 26.f, A + Sd * 26.f, FVector::UpVector);
			Cp.Quad(A - Sd * 26.f - FVector(0, 0, 10.f), B - Sd * 26.f - FVector(0, 0, 10.f), B - Sd * 26.f, A - Sd * 26.f, -Sd);
			Cp.Quad(A + Sd * 26.f, B + Sd * 26.f, B + Sd * 26.f - FVector(0, 0, 10.f), A + Sd * 26.f - FVector(0, 0, 10.f), Sd);
			Vis(Cp, GoldM, false);
		}
		// the gate: two banner pillars + a purple arch with gold trim, at the landing (pillar colliders)
		TArray<TArray<FVector>> GH;
		const float GY = 2050.f, GX = 380.f, GH0 = 0.f, GTop = 430.f;
		for (int32 s = -1; s <= 1; s += 2)
		{
			BoxHull(GH, FVector(s * GX, GY, (GH0 + GTop) * 0.5f - 200.f), FVector(55.f, 55.f, (GTop - GH0) * 0.5f + 200.f), 0.f);
			GS.Box(FVector(s * GX, GY, (GTop + GZ) * 0.5f), FVector(55.f, 55.f, (GTop - GZ) * 0.5f));
			Gd.Box(FVector(s * GX, GY, GTop + 10.f), FVector(64.f, 64.f, 10.f)); Gd.Box(FVector(s * GX, GY, 14.f), FVector(64.f, 64.f, 14.f));
			Banner(FVector(s * GX, GY + 60.f, GTop - 40.f), 90.f, 95.f);
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_front_gate"))) O->BuildCollider(GH, 1.f, WallPMat);
		{
			USceneComponent* Pv = NewPivot(FVector(0.f, GY, GTop + 20.f)); Pv->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
			FZoogiMeshBuilder Ar, ArG;
			Ar.ArcSlab(GX - 40.f, GX + 40.f, 0.f, 180.f, -45.f, 45.f, 24);
			ArG.ArcSlab(GX + 40.f, GX + 54.f, 0.f, 180.f, -50.f, 50.f, 24); ArG.ArcSlab(GX - 54.f, GX - 40.f, 0.f, 180.f, -50.f, 50.f, 24);
			Vis(Ar, PurpleM, true, Pv); Vis(ArG, GoldM, false, Pv);
		}
		MeshC(SphereM, BlueOrbM, FVector(0.f, GY, GTop + 20.f + GX + 60.f), FVector(0.9f), FRotator::ZeroRotator, false);
		Vis(GS, StoneM); Vis(Gd, GoldM, false);
		// the brick path out to the camera + gold edges + lanterns + the two big lamp pillars
		FZoogiMeshBuilder Pa, PaG, LS, LG;
		Pa.Quad(FVector(-HalfW, YBot - 20.f, GZ + 14.f), FVector(HalfW, YBot - 20.f, GZ + 14.f), FVector(HalfW, 12000.f, GZ + 14.f), FVector(-HalfW, 12000.f, GZ + 14.f), FVector::UpVector);
		for (int32 s = -1; s <= 1; s += 2) PaG.Quad(FVector(s * HalfW, YBot, GZ + 15.f), FVector(s * (HalfW + 24.f), YBot, GZ + 15.f), FVector(s * (HalfW + 24.f), 12000.f, GZ + 15.f), FVector(s * HalfW, 12000.f, GZ + 15.f), FVector::UpVector);
		for (float Y = YBot + 500.f; Y < 9000.f; Y += 700.f) for (int32 s = -1; s <= 1; s += 2) Lamp(LS, LG, FVector(s * (HalfW + 80.f), Y, GZ), 140.f, 18.f);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector2D C(s * 640.f, 2750.f);
			LS.Box(FVector(C, GZ + 260.f), FVector(70.f, 70.f, 260.f)); LG.Box(FVector(C, GZ + 530.f), FVector(80.f, 80.f, 12.f)); LG.Box(FVector(C, GZ + 16.f), FVector(82.f, 82.f, 16.f));
			LG.Prism(C, 10, 40.f, 60.f, GZ + 540.f, GZ + 600.f, 0.f);
			{ FZoogiMeshBuilder Gm; Crystal(Gm, FVector(C, GZ + 590.f), FVector(0, 0, 1), 55.f, 200.f); Vis(Gm, CrystalLightM, false); }
			Banner(FVector(C.X, C.Y + 74.f, GZ + 500.f), 90.f, 120.f);
		}
		Vis(Pa, BrickM, false); Vis(PaG, GoldM, false); Vis(LS, StoneM); Vis(LG, GoldM);
	}

	// ------------------------------------------------------------------ BALUSTRADES = the old fortress walls (same hulls) + layout battlements
	{
		struct FWall { FVector2D A, B; float Z, H; };
		const FWall Ws[] = {
			{ { -4680.f, -2350.f }, { -4680.f, -3350.f }, 150.f, 110.f }, { { -3550.f, -4480.f }, { -2350.f, -4480.f }, 150.f, 110.f },
			{ { -4680.f, -3550.f }, { -4680.f, -4480.f }, 450.f, 90.f }, { { -4600.f, -4480.f }, { -3800.f, -4480.f }, 450.f, 90.f },
			{ { 4680.f, -2350.f }, { 4680.f, -3700.f }, 450.f, 110.f }, { { 2350.f, -4480.f }, { 3700.f, -4480.f }, 450.f, 110.f },
			// open fronts of the front islands (ref: gold railings on the island rims), short runs leaving the routes open
			{ { -4680.f, 2600.f }, { -4680.f, 3700.f }, 0.f, 90.f }, { { 4680.f, 2600.f }, { 4680.f, 3700.f }, 0.f, 90.f },
		};
		TArray<TArray<FVector>> WH;
		FZoogiMeshBuilder BS, BG;
		for (const FWall& Wl : Ws)
		{
			const FVector2D D = (Wl.B - Wl.A).GetSafeNormal(); const float Len = FVector2D::Distance(Wl.A, Wl.B);
			BoxHull(WH, FVector((Wl.A + Wl.B) * 0.5f, Wl.Z + Wl.H * 0.5f), FVector(Len * 0.5f, 26.f, Wl.H * 0.5f), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
			Balustrade(BS, BG, Wl.A, Wl.B, Wl.Z, Wl.H);
		}
		for (const FPiece& Pc : L.Pieces) if (Pc.Kind == 3) Balustrade(BS, BG, FVector2D(Pc.A), FVector2D(Pc.B), Pc.A.Z, Pc.H);
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_balustrades"))) { O->BuildCollider(WH, 1.f, WallPMat); O->BounceGain = 1.1f; O->MaxOut = 1600.f; O->CenterBias = 0.5f; }
		Vis(BS, StoneM); Vis(BG, GoldM);
		// gold edge strip on every island top rim (reads the ref's gold-trimmed edges; flush, no gameplay effect)
		FZoogiMeshBuilder Edge, Moss;
		for (const FPlat& P : L.Plats)
		{
			if (P.Region == Hub) continue;
			FVector2D C(0, 0); for (const FVector2D& V : P.P) C += V; C /= P.P.Num();
			for (int32 i = 0; i < P.P.Num(); ++i)
			{
				const FVector2D A = P.P[i], B = P.P[(i + 1) % P.P.Num()];
				const FVector2D A1 = A + (C - A).GetSafeNormal() * 34.f, B1 = B + (C - B).GetSafeNormal() * 34.f;
				Edge.Quad(FVector(A, P.Z + 1.5f), FVector(B, P.Z + 1.5f), FVector(B1, P.Z + 1.5f), FVector(A1, P.Z + 1.5f), FVector::UpVector);
				const FVector2D Mid = (A + B) * 0.5f; const FVector2D On = (Mid - C).GetSafeNormal();
				Moss.Quad(FVector(A + On * 8.f, P.Z - 24.f), FVector(B + On * 8.f, P.Z - 24.f), FVector(B + On * 12.f, P.Z - 78.f), FVector(A + On * 12.f, P.Z - 78.f), FVector(On, 0.f));
				Edge.Quad(FVector(A, P.Z + 1.5f), FVector(B, P.Z + 1.5f), FVector(B, P.Z - 24.f), FVector(A, P.Z - 24.f), FVector(On, 0.f));
				// big Z banners on the cliff faces that look toward the plaza / the camera (ref: banners on every island face)
				const float Len = FVector2D::Distance(A, B);
				if (Len > 900.f && (On.Y > 0.5f || FVector2D::DotProduct(On, -C.GetSafeNormal()) > 0.6f))
				{
					const int32 NB = Len > 2000.f ? 2 : 1;
					for (int32 b = 0; b < NB; ++b)
					{
						const FVector2D Q = FMath::Lerp(A, B, NB == 1 ? 0.5f : (b ? 0.7f : 0.3f)) + On * 75.f;
						Banner(FVector(Q, P.Z - 40.f), FMath::RadiansToDegrees(FMath::Atan2(On.Y, On.X)), 150.f);
					}
				}
			}
		}
		Vis(Edge, GoldM, false); Vis(Moss, MossM, false);
	}

	// ------------------------------------------------------------------ warm lanterns along the hub bridges' gold rails (ref: lights on every rail)
	{
		FZoogiMeshBuilder LPost, Lan;
		UMaterialInterface* WarmM = ZoogiUtil::GlowMID(this, Hex(TEXT("#FFD27A")), 3.2f);
		for (const FRamp& Rp : L.Ramps)
		{
			if (!Rp.Name.ToString().StartsWith(TEXT("bridge_hub_"))) continue;
			const FVector2D A2(Rp.A), B2(Rp.B), D = (B2 - A2).GetSafeNormal(), Sd(-D.Y, D.X);
			const float Len = FVector2D::Distance(A2, B2);
			for (float t = 120.f; t < Len - 60.f; t += 280.f)
				for (int32 s = -1; s <= 1; s += 2)
				{
					const float Z = FMath::Lerp(Rp.A.Z, Rp.B.Z, RampProfile(t / Len)) + Rp.Curb;
					const FVector2D P = A2 + D * t + Sd * (s * (Rp.HalfW + 11.f));
					LPost.Box(FVector(P, Z + 28.f), FVector(4.f, 4.f, 28.f));
					Lan.Box(FVector(P, Z + 66.f), FVector(10.f, 10.f, 12.f), FRotator(0.f, 45.f, 0.f));
				}
		}
		Vis(LPost, GoldDarkM, false); Vis(Lan, WarmM, false);
		// glowing blue arch windows in the bridge keels near the piers (ref: lit arches under every ramp)
		FZoogiMeshBuilder Win;
		for (const FRamp& Rp : L.Ramps)
		{
			if (!Rp.Name.ToString().StartsWith(TEXT("bridge_hub_"))) continue;
			const FVector2D A2(Rp.A), B2(Rp.B), D = (B2 - A2).GetSafeNormal(), Sd(-D.Y, D.X);
			const float Len = FVector2D::Distance(A2, B2);
			for (float t : { 0.16f, 0.84f })
				for (int32 s = -1; s <= 1; s += 2)
				{
					const float Zb = FMath::Lerp(Rp.A.Z, Rp.B.Z, RampProfile(t)) - Rp.Thick - 22.f;
					const FVector C(A2 + D * (Len * t) + Sd * (s * (Rp.HalfW * 0.92f + 2.f)), Zb);
					const FVector X(D, 0.f), N(Sd * (float)s, 0.f);
					TArray<FVector> Pts; const float Wd = 38.f, Ht = 95.f;
					Pts.Add(C + X * Wd - FVector(0, 0, Ht));
					for (int32 a = 0; a <= 8; ++a) { const float An = PI * a / 8.f; Pts.Add(C + X * (Wd * FMath::Cos(An)) - FVector(0, 0, Wd - Wd * FMath::Sin(An))); }
					Pts.Add(C - X * Wd - FVector(0, 0, Ht));
					const FVector Mid = C - FVector(0, 0, Ht * 0.6f);
					for (int32 a = 0; a + 1 < Pts.Num(); ++a) Win.FlatTri(Mid, Pts[a], Pts[a + 1], N);
					Win.FlatTri(Mid, Pts.Last(), Pts[0], N);
				}
		}
		Vis(Win, PortalGlowM, false);
	}

	// ------------------------------------------------------------------ STAR PADS (aimed springs, same triggers) as blue glowing rune pads
	{
		struct FStar { FVector C; FVector Target; };
		const FStar Ss[] = { { { -4200.f, 2450.f, 0.f }, { -1050.f, 1050.f, 0.f } }, { { 4450.f, -2350.f, 450.f }, { 1050.f, -1050.f, 0.f } } };
		int32 Si = 0;
		FZoogiMeshBuilder Base, Rg;
		for (const FStar& S : Ss)
		{
			Base.Prism(FVector2D(S.C), 24, 150.f, 140.f, S.C.Z, S.C.Z + 22.f);
			for (int32 i = 0; i < 32; ++i)
			{
				const float B0 = 2.f * PI * i / 32, B1 = 2.f * PI * (i + 1) / 32;
				const FVector2D D0(FMath::Cos(B0), FMath::Sin(B0)), D1(FMath::Cos(B1), FMath::Sin(B1)), C(S.C);
				const float Z = S.C.Z + 23.f;
				Rg.Quad(FVector(C + D0 * 100.f, Z), FVector(C + D1 * 100.f, Z), FVector(C + D1 * 124.f, Z), FVector(C + D0 * 124.f, Z), FVector::UpVector);
				Rg.Quad(FVector(C + D0 * 40.f, Z), FVector(C + D1 * 40.f, Z), FVector(C + D1 * 58.f, Z), FVector(C + D0 * 58.f, Z), FVector::UpVector);
			}
			if (AZoogiObstacle* O = Spawn(FString::Printf(TEXT("Ancient_starpad_%d"), Si++), FTransform(S.C)))
			{
				FZoogiObsTrigger& T = O->Triggers.AddDefaulted_GetRef();
				T.Kind = 8; T.Shape = 2; T.C = FVector(0, 0, 50.f); T.E = FVector(120.f, 0.f, 70.f); T.P[2] = 1.0f; T.Target = S.Target;
			}
		}
		Vis(Base, PurpleM); Vis(Rg, BlueGlowM, false);
	}

	// ------------------------------------------------------------------ DRUM BUMPERS at the old crate spots (yellow / black chevron drums)
	{
		const FVector Spots[] = { { -4550.f, -3300.f, 150.f }, { -2300.f, -3900.f, 150.f }, { 4500.f, -3600.f, 450.f }, { 2400.f, -4400.f, 450.f },
			{ -4500.f, 2300.f, 0.f }, { -2400.f, 4300.f, 0.f }, { 4550.f, 4450.f, 0.f }, { 2250.f, 2900.f, 0.f } };
		TArray<TArray<FVector>> CH;
		FZoogiMeshBuilder Bd, Tp, Ch;
		for (const FVector& C : Spots)
		{
			CylHull(CH, C, 62.f, 105.f);
			Bd.Prism(FVector2D(C), 20, 62.f, 60.f, C.Z, C.Z + 90.f); Tp.Prism(FVector2D(C), 20, 60.f, 54.f, C.Z + 90.f, C.Z + 106.f);
			for (int32 c = 0; c < 6; ++c)
			{
				const float Ac = 2.f * PI * c / 6.f; const FVector Rd(FMath::Cos(Ac), FMath::Sin(Ac), 0.f), Tg(-Rd.Y, Rd.X, 0.f);
				const FVector Pc = C + Rd * 62.5f + FVector(0, 0, 45.f);
				for (int32 h = -1; h <= 1; h += 2) { const FVector P0 = Pc + Tg * 10.f, P1 = Pc - Tg * 12.f + FVector(0, 0, h * 24.f); Ch.Quad(P0 - FVector(0, 0, 6.f), P1 - FVector(0, 0, 6.f), P1 + FVector(0, 0, 6.f), P0 + FVector(0, 0, 6.f), Rd); }
			}
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_drums"))) { O->BuildCollider(CH, 1.f, BumpPMat); O->BounceGain = 1.3f; O->MaxOut = 2200.f; O->CenterBias = 0.65f; }
		Vis(Bd, YellowM); Vis(Tp, InkM); Vis(Ch, InkM, false);
	}

	// ------------------------------------------------------------------ LANDMARK OUTCROPS (visual, beyond the island edges)
	auto Outcrop = [&](const FVector2D& C, float R, float TopZ, int32 Seed) -> FVector
	{
		TArray<FVector2D> Poly; FRandomStream Rr(Seed);
		for (int32 k = 0; k < 10; ++k) { const float A = 2.f * PI * k / 10.f; Poly.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * Rr.FRandRange(0.9f, 1.05f)); }
		FZoogiMeshBuilder Top, Cl, Gd;
		for (int32 k = 0; k < 10; ++k) Top.FlatTri(FVector(C, TopZ), FVector(Poly[k], TopZ), FVector(Poly[(k + 1) % 10], TopZ), FVector::UpVector);
		for (int32 k = 0; k < 10; ++k) { const FVector2D A = Poly[k], B = Poly[(k + 1) % 10]; Gd.Quad(FVector(A, TopZ + 1.f), FVector(B, TopZ + 1.f), FVector(B, TopZ - 26.f), FVector(A, TopZ - 26.f), FVector(((A + B) * 0.5f - C).GetSafeNormal(), 0.f)); }
		CliffSkirt(Cl, Poly, TopZ - 26.f, GZ - 20.f, 60.f, 180.f, Seed);
		Vis(Top, PaverM); Vis(Cl, CliffM); Vis(Gd, GoldM, false);
		return FVector(C, TopZ);
	};
	// NW crystal shrine
	{
		const FVector C = Outcrop(FVector2D(-3300.f, -5350.f), 760.f, 520.f, 71);
		FZoogiMeshBuilder Sh, ShG, Cr, CrL;
		Sh.Box(C + FVector(0, 0, 50.f), FVector(420.f, 300.f, 50.f)); Sh.Box(C + FVector(0, 0, 150.f), FVector(340.f, 240.f, 50.f));
		for (int32 s = -1; s <= 1; s += 2) { Sh.Box(C + FVector(s * 300.f, 180.f, 330.f), FVector(40.f, 40.f, 130.f)); ShG.Box(C + FVector(s * 300.f, 180.f, 470.f), FVector(48.f, 48.f, 10.f)); }
		ShG.Box(C + FVector(0, 0, 202.f), FVector(345.f, 245.f, 4.f));
		FRandomStream Rr(33);
		const FVector Cc = C + FVector(0, -40.f, 200.f);
		Crystal(Cr, Cc, FVector(0, 0, 1), 150.f, 1150.f);
		for (int32 k = 0; k < 9; ++k)
		{
			const float A = 2.f * PI * k / 9.f + 0.3f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
			Crystal(k % 2 ? Cr : CrL, Cc + D * Rr.FRandRange(60.f, 160.f), (D * Rr.FRandRange(0.35f, 0.8f) + FVector(0, 0, 1)).GetSafeNormal(), Rr.FRandRange(60.f, 110.f), Rr.FRandRange(380.f, 780.f));
		}
		Vis(Sh, StoneM); Vis(ShG, GoldM, false); Vis(Cr, CrystalM, false); Vis(CrL, CrystalLightM, false);
		Banner(C + FVector(-180.f, 245.f, 190.f), 90.f, 90.f); Banner(C + FVector(180.f, 245.f, 190.f), 90.f, 90.f);
		FZoogiMeshBuilder P1, P2, Tr; FRandomStream Pr(5);
		Palm(Tr, P1, P2, C + FVector(-560.f, 120.f, 0.f), 780.f, 200.f, Pr); Palm(Tr, P1, P2, C + FVector(560.f, 60.f, 0.f), 700.f, -20.f, Pr);
		Vis(Tr, BarkM); Vis(P1, LeafM); Vis(P2, LeafM2);
		AddWaterfall(FVector(-3300.f - 800.f, -5350.f, 480.f), 180.f, 260.f, 1140.f, 40.f);
	}
	// NE gold-dome palace with purple canopies
	{
		const FVector C = Outcrop(FVector2D(3300.f, -5400.f), 820.f, 520.f, 72);
		FZoogiMeshBuilder Bs, BsG, Can, Col;
		Bs.Box(C + FVector(0, 0, 160.f), FVector(420.f, 320.f, 160.f));
		BsG.Box(C + FVector(0, 0, 326.f), FVector(430.f, 330.f, 8.f));
		for (int32 i = -3; i <= 3; ++i) { Col.Prism(FVector2D(C) + FVector2D(i * 120.f, 340.f), 10, 20.f, 18.f, C.Z, C.Z + 320.f, 0.f); }
		Bs.Prism(FVector2D(C), 16, 240.f, 230.f, C.Z + 330.f, C.Z + 470.f, 0.f);
		MeshC(SphereM, GoldM, C + FVector(0, 0, 470.f), FVector(5.6f, 5.6f, 5.2f), FRotator::ZeroRotator, true);
		BsG.Prism(FVector2D(C), 10, 22.f, 4.f, C.Z + 670.f, C.Z + 760.f, 0.f);
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector Tc = C + FVector(s * 360.f, 120.f, 330.f);
			Bs.Prism(FVector2D(Tc), 10, 80.f, 76.f, Tc.Z, Tc.Z + 200.f, 0.f);
			MeshC(SphereM, GoldM, Tc + FVector(0, 0, 200.f), FVector(1.6f, 1.6f, 1.5f), FRotator::ZeroRotator, false);
			// purple canopies on gold poles
			const FVector Pc = C + FVector(s * 620.f, 260.f, 0.f);
			for (int32 a = -1; a <= 1; a += 2) for (int32 b = -1; b <= 1; b += 2) BsG.Prism(FVector2D(Pc) + FVector2D(a * 130.f, b * 110.f), 6, 7.f, 7.f, C.Z, C.Z + 230.f, 0.f);
			Can.Prism(FVector2D(Pc), 4, 230.f, 30.f, C.Z + 230.f, C.Z + 330.f, 45.f);
		}
		Vis(Bs, StoneM); Vis(BsG, GoldM); Vis(Can, ClothM); Vis(Col, CreamM);
		Banner(C + FVector(-200.f, 345.f, 300.f), 90.f, 100.f); Banner(C + FVector(200.f, 345.f, 300.f), 90.f, 100.f);
		FZoogiMeshBuilder Fl; Fl.Box(C + FVector(0, 0, 1040.f), FVector(4.f, 4.f, 260.f)); Vis(Fl, GoldM, false);
		FZoogiMeshBuilder Fc; Quad2(Fc, C + FVector(0, 0, 1180.f), C + FVector(130.f, 0, 1170.f), C + FVector(130.f, 0, 1260.f), C + FVector(0, 0, 1290.f), FVector(0, 1, 0)); Vis(Fc, ClothM, false);
		FZoogiMeshBuilder P1, P2, Tr; FRandomStream Pr(6);
		Palm(Tr, P1, P2, C + FVector(-650.f, -120.f, 0.f), 800.f, 160.f, Pr); Palm(Tr, P1, P2, C + FVector(660.f, -200.f, 0.f), 740.f, 20.f, Pr);
		Vis(Tr, BarkM); Vis(P1, LeafM); Vis(P2, LeafM2);
		AddWaterfall(FVector(3300.f + 860.f, -5400.f, 480.f), 0.f, 280.f, 1140.f, 40.f);
	}
	// SW domed temple
	{
		const FVector C = Outcrop(FVector2D(-2650.f, 1150.f), 450.f, -150.f, 73);
		FZoogiMeshBuilder Bs, G, Col, Dr;
		Bs.Prism(FVector2D(C), 16, 330.f, 320.f, C.Z, C.Z + 40.f, 0.f);
		for (int32 k = 0; k < 10; ++k) { const float A = 2.f * PI * k / 10.f; Col.Prism(FVector2D(C) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 260.f, 10, 24.f, 22.f, C.Z + 40.f, C.Z + 300.f, 0.f); }
		Bs.Prism(FVector2D(C), 16, 300.f, 300.f, C.Z + 300.f, C.Z + 360.f, 0.f);
		G.ArcSlab(296.f, 308.f, 0.f, 360.f, C.Z + 300.f, C.Z + 312.f, 48); G.ArcSlab(296.f, 308.f, 0.f, 360.f, C.Z + 352.f, C.Z + 364.f, 48);
		MeshC(SphereM, ZoogiUtil::GlowMID(this, Hex(TEXT("#3FC8E0")), 0.9f), C + FVector(0, 0, 360.f), FVector(5.4f, 5.4f, 4.4f), FRotator::ZeroRotator, true);
		G.Prism(FVector2D(C), 8, 30.f, 8.f, C.Z + 560.f, C.Z + 640.f, 0.f);
		for (int32 k = 0; k < 10; k += 2) { const float A = 2.f * PI * k / 10.f + PI / 10.f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f); Quad2(Dr, C + D * 300.f + FVector(0, 0, 300.f) - FVector(-D.Y, D.X, 0.f) * 70.f, C + D * 300.f + FVector(0, 0, 300.f) + FVector(-D.Y, D.X, 0.f) * 70.f, C + D * 310.f + FVector(0, 0, 150.f), C + D * 310.f + FVector(0, 0, 150.f), D); }
		Vis(Bs, StoneM); Vis(G, GoldM); Vis(Col, CreamM); Vis(Dr, ClothPinkM, false);
		FZoogiMeshBuilder P1, P2, Tr; FRandomStream Pr(7);
		Palm(Tr, P1, P2, C + FVector(-330.f, -260.f, 0.f), 760.f, 225.f, Pr); Palm(Tr, P1, P2, C + FVector(-300.f, 300.f, 0.f), 640.f, 150.f, Pr);
		Vis(Tr, BarkM); Vis(P1, LeafM); Vis(P2, LeafM2);
		Banner(C + FVector(0.f, 330.f, 290.f), 90.f, 90.f);
	}
	// SE armillary sphere
	{
		const FVector C = Outcrop(FVector2D(2650.f, 1150.f), 450.f, -150.f, 74);
		FZoogiMeshBuilder Bs, G;
		Bs.Prism(FVector2D(C), 12, 260.f, 250.f, C.Z, C.Z + 50.f, 0.f); Bs.Prism(FVector2D(C), 12, 120.f, 90.f, C.Z + 50.f, C.Z + 200.f, 0.f);
		G.Prism(FVector2D(C), 12, 130.f, 130.f, C.Z + 200.f, C.Z + 220.f, 0.f); G.Prism(FVector2D(C), 8, 30.f, 24.f, C.Z + 220.f, C.Z + 330.f, 0.f);
		Vis(Bs, StoneM); Vis(G, GoldM);
		const FVector Sc = C + FVector(0, 0, 520.f);
		MeshC(SphereM, BlueOrbM, Sc, FVector(2.6f), FRotator::ZeroRotator, false);
		const FRotator Rots[] = { FRotator(90.f, 0.f, 0.f), FRotator(90.f, 60.f, 0.f), FRotator(90.f, 120.f, 0.f), FRotator(0.f, 0.f, 23.f), FRotator(0.f, 0.f, 0.f) };
		for (int32 i = 0; i < 5; ++i)
		{
			USceneComponent* Pv = NewPivot(Sc); Pv->SetRelativeRotation(Rots[i]);
			FZoogiMeshBuilder Rg; Rg.ArcSlab(i < 3 ? 250.f : 285.f, i < 3 ? 268.f : 305.f, 0.f, 360.f, -10.f, 10.f, 56);
			Vis(Rg, i == 3 ? GoldDarkM : GoldM, false, Pv);
		}
		FZoogiMeshBuilder P1, P2, Tr; FRandomStream Pr(8);
		Palm(Tr, P1, P2, C + FVector(330.f, -260.f, 0.f), 760.f, -45.f, Pr); Palm(Tr, P1, P2, C + FVector(300.f, 300.f, 0.f), 640.f, 30.f, Pr);
		Vis(Tr, BarkM); Vis(P1, LeafM); Vis(P2, LeafM2);
		Banner(C + FVector(0.f, 270.f, 230.f), 90.f, 90.f);
	}

	// ------------------------------------------------------------------ PALMS / TREES at the layout set-piece spots (their trunks collide in BuildAncient)
	{
		FZoogiMeshBuilder Tr, P1, P2; FRandomStream Pr(41);
		for (const FPiece& Pc : L.Pieces)
		{
			if (Pc.Kind != 1 && Pc.Kind != 2) continue;
			Palm(Tr, P1, P2, Pc.A, Pc.H * 1.5f, Pr.FRandRange(0.f, 360.f), Pr);
		}
		Vis(Tr, BarkM); Vis(P1, LeafM); Vis(P2, LeafM2);
	}

	// ------------------------------------------------------------------ MEADOW DRESSING: palms, purple plants, rocks, boulders
	{
		FRandomStream R(2026);
		auto Clear = [&](const FVector2D& P)
		{
			if (FMath::Abs(P.X) < 520.f && P.Y > 1900.f) return false;                    // the path
			if (P.Size() < L.HubR + 250.f) return false;
			for (const FPlat& Pl : L.Plats) if (InConvex(Pl.P, P, -260.f)) return false;  // under the islands
			return true;
		};
		FZoogiMeshBuilder Tr, P1, P2, Pp, Pt, Rk;
		int32 NPalm = 0;
		for (int32 i = 0; i < 900 && NPalm < 140; ++i)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(2300.f, 15500.f);
			const FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
			if (!Clear(P)) continue;
			Palm(Tr, P1, P2, FVector(P, GZ), R.FRandRange(650.f, 1050.f), R.FRandRange(0.f, 360.f), R); ++NPalm;
		}
		for (int32 i = 0; i < 260; ++i)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(2100.f, 9000.f);
			FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
			if (i < 90) P = FVector2D(R.FRandRange(-2600.f, 2600.f), R.FRandRange(2050.f, 4200.f));   // dense foreground (ref)
			if (!Clear(P)) continue;
			const float Big = i < 90 ? 1.7f : 1.f;
			if (i % 3 == 0) Plant(Pt, FVector(P, GZ), R.FRandRange(90.f, 160.f) * Big, 9, R);
			else Plant(Pp, FVector(P, GZ), R.FRandRange(80.f, 160.f) * Big, 8, R);
		}
		for (int32 i = 0; i < 120; ++i)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(2000.f, 9000.f);
			FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
			if (i < 40) P = FVector2D(R.FRandRange(-2600.f, 2600.f), R.FRandRange(2050.f, 4200.f));
			if (!Clear(P)) continue;
			const float S = R.FRandRange(60.f, 190.f);
			Rk.Chunk(FVector(P, GZ + S * 0.35f), FVector(S * R.FRandRange(1.f, 1.5f), S * R.FRandRange(0.9f, 1.3f), S * 0.7f), R, 0.28f, R.FRandRange(0.f, 180.f));
		}
		// boulders at the island feet
		for (const FPlat& Pl : L.Plats)
		{
			FVector2D C(0, 0); for (const FVector2D& V : Pl.P) C += V; C /= Pl.P.Num();
			for (int32 k = 0; k < 10; ++k)
			{
				const FVector2D V = Pl.P[R.RandRange(0, Pl.P.Num() - 1)];
				const FVector2D Q = FMath::Lerp(V, Pl.P[(R.RandRange(0, Pl.P.Num() - 1))], R.FRandRange(0.f, 1.f));
				const FVector2D P = Q + (Q - C).GetSafeNormal() * R.FRandRange(180.f, 420.f);
				if (FMath::Abs(P.X) < 520.f && P.Y > 1900.f) continue;
				const float S = R.FRandRange(90.f, 220.f);
				Rk.Chunk(FVector(P, GZ + S * 0.3f), FVector(S * 1.3f, S, S * 0.75f), R, 0.3f, R.FRandRange(0.f, 180.f));
			}
		}
		Vis(Tr, BarkM); Vis(P1, LeafM); Vis(P2, LeafM2); Vis(Pp, PlantPurpleM, false); Vis(Pt, PlantTealM, false); Vis(Rk, CliffGreyM);
	}

	if (BannerDM) Vis(BannerTex, BannerDM, false);
	Vis(BannerFallback, ClothM, false); Vis(BannerFallbackGold, GoldM, false);
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT v2: meadow look built (volcano drum + crown orb, plaza, stairs + gate, balustrades, 4 landmarks, meadow, rivers, mesas)"));
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", on)
#endif
