// Oct 31 (Evan: "the battle Royale environment ... I need it to be exactly like that every single detail", ref
// C:\dev\art\ancient_meadows\royale_ref\royale_ref.png): the Battle Royale reference look on top of the Ancient layout.
//   centre  : lava volcano (collision cone) crowned with a stone + gold spike crown holding a glowing orb and a light beam
//   NW / NE : blue fortress / red fortress walls (crenellated, collision) - the portals glow blue / orange (layout)
//   high    : wooden rope bridges with hanging crown banners from void towers to ledges on the volcano, bunting strings
//   pads    : star bounce pads (aimed springs) on the orange plaza (SW) and the red fortress; wooden crates
//   below   : the ruin valley (checker fields, horseshoe arch, pillars, palms, huts), lava falls, floating stepping stones
//   sky     : blimps get gold crown emblems; distant floating temple islands with columns
// Visual pieces never collide unless stated. Called at the end of BuildAncient.
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
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

namespace ZoogiAncLook
{
	constexpr float BaseR = 900.f, PeakZ = 1750.f, TopR = 340.f;
	// volcano radius at height Z (smooth profile, the visual jitter only goes inward of it)
	float MtnR(float Z) { const float T = FMath::Clamp(Z / PeakZ, 0.f, 1.f); return FMath::Lerp(BaseR, TopR, FMath::Pow(T, 0.8f)); }
	void Star(FZoogiMeshBuilder& M, const FVector& C, float ROut, float RIn, float Yaw)
	{
		TArray<FVector> P;
		for (int32 i = 0; i < 10; ++i) { const float A = FMath::DegreesToRadians(Yaw + 36.f * i + 90.f); const float R = (i % 2 == 0) ? ROut : RIn; P.Add(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f)); }
		for (int32 i = 0; i < 10; ++i) M.FlatTri(C, P[i], P[(i + 1) % 10], FVector::UpVector);
	}
}

void AZoogiArena::BuildAncientLook()
{
	using namespace ZoogiAncLook;
	using ZoogiUtil::Hex;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientOldLook"))) return;
	if (ZoogiAncient::Meadow2()) { BuildAncientMeadow(); return; } // Nov 1: v2 sunset meadow look
	UWorld* W = GetWorld();
	if (!W) return;
	UMaterialInterface* TileBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_AncientTile.M_AncientTile"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto TexA = [](const TCHAR* N) { return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet); };
	auto Tile = [&](const TCHAR* Top, const TCHAR* Side, float TS, float SS, const TCHAR* Fallback, const TCHAR* Tint = nullptr, float Bright = 1.f) -> UMaterialInterface*
	{
		UTexture* T = TexA(Top); UTexture* S = TexA(Side);
		if (!TileBase || !T || !S) return ZoogiUtil::SurfaceMID(this, Hex(Fallback), 0.85f);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TileBase, this);
		M->SetTextureParameterValue(TEXT("TopTex"), T); M->SetTextureParameterValue(TEXT("SideTex"), S);
		M->SetScalarParameterValue(TEXT("TopScale"), TS); M->SetScalarParameterValue(TEXT("SideScale"), SS); M->SetScalarParameterValue(TEXT("Bright"), Bright);
		if (Tint) { M->SetVectorParameterValue(TEXT("TopTint"), Hex(Tint)); M->SetVectorParameterValue(TEXT("SideTint"), Hex(Tint)); }
		return M;
	};
	UMaterialInterface* RockM = Tile(TEXT("T_AncRockTile"), TEXT("T_AncRockTile"), 700.f, 520.f, TEXT("#9A5A3C"), TEXT("#F0A27A"), 1.45f); // ref volcano: warm red-brown rock, not dark
	UMaterialInterface* BlueWallM = Tile(TEXT("T_AncWallBlueTile"), TEXT("T_AncWallBlueTile"), 300.f, 260.f, TEXT("#4F6FC0"));
	UMaterialInterface* RedWallM = Tile(TEXT("T_AncWallTile"), TEXT("T_AncWallTile"), 460.f, 360.f, TEXT("#B85A42"), TEXT("#E58A6A"));
	UMaterialInterface* StoneM = Tile(TEXT("T_AncWallTile"), TEXT("T_AncWallTile"), 460.f, 360.f, TEXT("#A99273"));
	UMaterialInterface* GrassM = Tile(TEXT("T_AncEmeraldTile"), TEXT("T_AncRockTile"), 1600.f, 900.f, TEXT("#5DB33A"));
	UMaterialInterface* OrangeM = Tile(TEXT("T_AncNorthTile"), TEXT("T_AncWallTile"), 1400.f, 460.f, TEXT("#E07A3A"));
	UMaterialInterface* DirtM = Tile(TEXT("T_AncSunstoneTile"), TEXT("T_AncRockTile"), 900.f, 700.f, TEXT("#D9A56A"));
	UMaterialInterface* GoldM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F2C230")), 0.3f, Hex(TEXT("#F2C230")), 0.45f, 0.8f);
	UMaterialInterface* IvoryM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#EFE4C8")), 0.5f);
	UMaterialInterface* WoodM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#9A6634")), 0.8f);
	UMaterialInterface* WoodDarkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#5C3A1C")), 0.85f);
	UMaterialInterface* RopeM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#C9A36A")), 0.9f);
	UMaterialInterface* LeafA = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#2FA03A")), 0.7f);
	UMaterialInterface* LeafB = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#46BE3E")), 0.7f);
	UMaterialInterface* MossM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#5DAE38")), 0.8f);
	UMaterialInterface* OrbM = ZoogiUtil::GlowMID(this, Hex(TEXT("#7E6BFF")), 2.4f);
	UMaterialInterface* OrbCoreM = ZoogiUtil::GlowMID(this, Hex(TEXT("#BFE8FF")), 3.0f);
	UMaterialInterface* BeamM = ZoogiUtil::GlowMID(this, Hex(TEXT("#D8ECFF")), 1.6f);
	UMaterialInterface* StarBlueM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#2F5FD0")), 0.35f, Hex(TEXT("#3F8CFF")), 0.25f, 0.3f);
	UMaterialInterface* StarYelM = ZoogiUtil::GlowMID(this, Hex(TEXT("#FFD23F")), 1.4f);
	UMaterialInterface* LavaM = KitLavaMaterial(1.f, 5.f, 0.f, -0.25f, 3.0f, 0.2f);
	if (!LavaM) LavaM = ZoogiUtil::GlowMID(this, Hex(TEXT("#FF7A1A")), 2.5f);
	UStaticMesh* SphereM = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* CylM = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* ConeM = ZoogiUtil::BasicShape(TEXT("Cone"));
	UPhysicalMaterial* RockPMat = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.45f, false);
	UPhysicalMaterial* WallPMat = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.6f, false);

	FActorSpawnParameters SpP; SpP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto Spawn = [&](const FString& Name, const FTransform& X = FTransform::Identity) -> AZoogiObstacle*
	{
		AZoogiObstacle* O = W->SpawnActor<AZoogiObstacle>(AZoogiObstacle::StaticClass(), X, SpP);
		if (O) { O->ObsName = FName(*Name); Obstacles.Add(O); }
		return O;
	};
	auto Vis = [&](const FZoogiMeshBuilder& M, UMaterialInterface* Mat, bool bShadow = true, USceneComponent* Parent = nullptr)
	{
		if (M.V.Num() == 0) return;
		if (UProceduralMeshComponent* C = AddFrozenProc(M, Mat, Parent, bShadow)) C->ComponentTags.Add(TEXT("NoCamBlock"));
	};
	auto MeshC = [&](UStaticMesh* M, UMaterialInterface* Mat, const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true, USceneComponent* Parent = nullptr)
	{
		UStaticMeshComponent* C = AddMesh(M, Mat, false, bShadow);
		if (!C) return C;
		if (Parent) C->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeLocation(Loc); C->SetRelativeRotation(Rot); C->SetRelativeScale3D(Scale);
		C->ComponentTags.Add(TEXT("NoCamBlock"));
		return C;
	};
	auto BoxHull = [](TArray<TArray<FVector>>& H, const FVector& C, const FVector& Half, float Yaw)
	{
		TArray<FVector>& P = H.AddDefaulted_GetRef(); const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
		for (int32 i = 0; i < 8; ++i) P.Add(C + Q.RotateVector(FVector((i & 1) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 4) ? Half.Z : -Half.Z)));
	};
	auto Banner = [&](const FVector& Top, float Yaw, float BW, float BH, const TCHAR* Col)
	{
		const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion(); const FVector Xa = Q.RotateVector(FVector::RightVector), Na = Q.RotateVector(FVector::ForwardVector);
		FZoogiMeshBuilder Cl, Mk;
		const FVector A = Top - Xa * BW * 0.5f, B = Top + Xa * BW * 0.5f, Tip = Top - FVector(0, 0, BH * 1.18f);
		for (int32 s = -1; s <= 1; s += 2)
		{
			Cl.Quad(A, B, B - FVector(0, 0, BH), A - FVector(0, 0, BH), Na * s);
			Cl.FlatTri(A - FVector(0, 0, BH), B - FVector(0, 0, BH), Tip, Na * s);
			const FVector Mc = Top - FVector(0, 0, BH * 0.42f) + Na * (2.f * s); const float K = BW * 0.28f;
			Mk.Quad(Mc - Xa * K - FVector(0, 0, K * 0.35f), Mc + Xa * K - FVector(0, 0, K * 0.35f), Mc + Xa * K + FVector(0, 0, K * 0.05f), Mc - Xa * K + FVector(0, 0, K * 0.05f), Na * s);
			for (int32 p = -1; p <= 1; ++p) { const FVector Bp = Mc + Xa * (p * K * 0.8f) + FVector(0, 0, K * 0.05f); Mk.FlatTri(Bp - Xa * K * 0.28f, Bp + Xa * K * 0.28f, Bp + FVector(0, 0, K * (p == 0 ? 0.75f : 0.55f)), Na * s); }
		}
		Vis(Cl, ZoogiUtil::SurfaceMID(this, Hex(Col), 0.75f), false); Vis(Mk, GoldM, false);
	};
	auto Palm = [&](const FVector& Base, float Ht)
	{
		FZoogiMeshBuilder Tr;
		for (int32 s = 0; s < 5; ++s) { const float Z0 = Base.Z + s * Ht / 5.f; Tr.Prism(FVector2D(Base) + FVector2D(s * 10.f, s * 4.f), 7, 34.f - 3.f * s, 31.f - 3.f * s, Z0, Z0 + Ht / 5.f + 4.f, s * 13.f); }
		Vis(Tr, WoodM, false);
		const FVector Cr(Base.X + 50.f, Base.Y + 20.f, Base.Z + Ht);
		for (int32 f = 0; f < 7; ++f) { const float Yw = f * 360.f / 7.f + 10.f; MeshC(ConeM, f % 2 ? LeafA : LeafB, Cr + FRotator(0.f, Yw, 0.f).RotateVector(FVector(110.f, 0.f, -20.f)), FVector(0.55f, 2.6f, 0.12f), FRotator(-18.f, Yw, 90.f), false); }
	};
	// stacked-block tower (the reference's chunky blue / red masonry): square blocks, slight per-course offsets, crenellated top
	auto Tower = [&](const FVector2D& C, float Z0, float Z1, float Half, UMaterialInterface* Mat, int32 SeedT)
	{
		FRandomStream R(SeedT);
		FZoogiMeshBuilder Bk;
		for (float Z = Z0; Z < Z1; Z += 110.f)
		{
			const FVector2D J(R.FRandRange(-8.f, 8.f), R.FRandRange(-8.f, 8.f));
			Bk.Box(FVector(C + J, Z + 55.f), FVector(Half + R.FRandRange(-6.f, 6.f), Half + R.FRandRange(-6.f, 6.f), 53.f));
		}
		for (int32 m = 0; m < 4; ++m) for (int32 n = -1; n <= 1; n += 2)
		{
			const FVector2D O = FVector2D(m < 2 ? (m == 0 ? Half : -Half) : n * Half * 0.55f, m >= 2 ? (m == 2 ? Half : -Half) : n * Half * 0.55f) * 0.86f;
			Bk.Box(FVector(C + O, Z1 + 40.f), FVector(Half * 0.22f, Half * 0.22f, 40.f));
		}
		Vis(Bk, Mat);
	};

	// ================================================================== VOLCANO + CROWN ORB (the centrepiece)
	{
		FRandomStream R(907);
		constexpr int32 NR = 9, NS = 24;
		TArray<TArray<FVector>> Rings;
		for (int32 r = 0; r <= NR; ++r)
		{
			TArray<FVector>& Rg = Rings.AddDefaulted_GetRef();
			const float Z = PeakZ * r / NR;
			for (int32 s = 0; s < NS; ++s)
			{
				const float A = 2.f * PI * s / NS + (r % 2) * PI / NS;
				const float J = r == NR ? 0.f : R.FRandRange(-0.13f, 0.02f) - 0.05f * FMath::Abs(FMath::Sin(A * 3.f + r));
				Rg.Add(FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * MtnR(Z) * (1.f + J) + FVector(0, 0, Z + (r > 0 && r < NR ? R.FRandRange(-25.f, 25.f) : 0.f)));
			}
		}
		FZoogiMeshBuilder Mtn;
		for (int32 r = 0; r < NR; ++r)
			for (int32 s = 0; s < NS; ++s)
			{
				const FVector& A0 = Rings[r][s]; const FVector& A1 = Rings[r][(s + 1) % NS]; const FVector& B0 = Rings[r + 1][s]; const FVector& B1 = Rings[r + 1][(s + 1) % NS];
				const FVector Out = (FVector((A0 + A1) * 0.5f).GetSafeNormal2D() + FVector(0, 0, 0.5f)).GetSafeNormal();
				Mtn.Quad(A0, A1, B1, B0, Out);
			}
		// crater lip down to a lava pool
		FZoogiMeshBuilder Lip, Pool;
		for (int32 s = 0; s < NS; ++s)
		{
			const FVector& A0 = Rings[NR][s]; const FVector& A1 = Rings[NR][(s + 1) % NS];
			const FVector I0 = FVector(FVector2D(A0) * 0.72f, PeakZ - 70.f), I1 = FVector(FVector2D(A1) * 0.72f, PeakZ - 70.f);
			Lip.Quad(A0, A1, I1, I0, FVector(0, 0, 1));
			Pool.FlatTri(FVector(0, 0, PeakZ - 60.f), I0, I1, FVector::UpVector);
		}
		Vis(Mtn, RockM); Vis(Lip, RockM); Vis(Pool, LavaM, false);
		// lava rivers down the slopes (strips hugging the smooth profile, a little proud of it)
		FZoogiMeshBuilder Lava;
		for (int32 k = 0; k < 7; ++k)
		{
			const float A0 = 360.f / 7.f * k + 12.f;
			FVector PrevL, PrevR; bool bHave = false;
			for (int32 i = 0; i <= 16; ++i)
			{
				const float T = 1.f - i / 16.f; // top -> bottom
				const float Z = PeakZ * T - 20.f;
				const float A = FMath::DegreesToRadians(A0 + 9.f * FMath::Sin(i * 0.9f + k));
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f), Tan(-Dir.Y, Dir.X, 0.f);
				const float Wd = FMath::Lerp(14.f, 58.f, 1.f - T) * (0.75f + 0.25f * FMath::Sin(i * 1.7f + k * 2.f));
				const FVector C = Dir * (MtnR(Z) * 1.035f + 6.f) + FVector(0, 0, Z);
				const FVector Lp = C - Tan * Wd, Rp = C + Tan * Wd;
				if (bHave) Lava.Quad(PrevL, PrevR, Rp, Lp, (Dir + FVector(0, 0, 0.6f)).GetSafeNormal());
				PrevL = Lp; PrevR = Rp; bHave = true;
			}
		}
		Vis(Lava, ZoogiUtil::GlowMID(this, Hex(TEXT("#FF5C14")), 2.2f), false); // ref: deep orange glowing rivers (the kit lava read pale yellow on strips)
		// green tufts on the lower slopes (the ref volcano is half jungle)
		for (int32 k = 0; k < 14; ++k)
		{
			const float A = 2.f * PI * k / 14.f + R.FRandRange(-0.12f, 0.12f) + 0.2f;
			const float Z = R.FRandRange(60.f, 520.f);
			const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector C = D * (MtnR(Z) * 0.97f) + FVector(0, 0, Z);
			for (int32 j = 0; j < 3; ++j)
				MeshC(SphereM, j % 2 ? LeafA : LeafB, C + FVector(R.FRandRange(-60.f, 60.f), R.FRandRange(-60.f, 60.f), R.FRandRange(0.f, 50.f)), FVector(R.FRandRange(0.9f, 1.5f), R.FRandRange(0.9f, 1.5f), R.FRandRange(0.6f, 0.9f)), FRotator::ZeroRotator, false);
		}
		// collision: a convex cone just outside the visual jitter
		TArray<TArray<FVector>> H; TArray<FVector>& Pts = H.AddDefaulted_GetRef();
		for (int32 s = 0; s < 16; ++s) { const float A = 2.f * PI * s / 16; Pts.Add(FVector(FMath::Cos(A) * BaseR * 1.03f, FMath::Sin(A) * BaseR * 1.03f, -10.f)); Pts.Add(FVector(FMath::Cos(A) * TopR * 1.05f, FMath::Sin(A) * TopR * 1.05f, PeakZ + 150.f)); }
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_volcano"))) O->BuildCollider(H, 1.f, RockPMat);
		SolidPosts.Add(FVector2D::ZeroVector);
		// crown: stone ring + gold rim + ivory / gold spikes leaning out, banners around it
		FZoogiMeshBuilder Ring, Rim, SpG, SpI;
		Ring.Prism(FVector2D::ZeroVector, 14, TopR + 30.f, TopR + 70.f, PeakZ - 40.f, PeakZ + 140.f, 0.f);
		Rim.Prism(FVector2D::ZeroVector, 14, TopR + 78.f, TopR + 78.f, PeakZ + 120.f, PeakZ + 150.f, 0.f);
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = 2.f * PI * k / 10.f; const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f), T2(-D.Y, D.X, 0.f);
			const FVector B = D * (TopR + 55.f) + FVector(0, 0, PeakZ + 145.f);
			const float Hs = k % 2 ? 260.f : 340.f, Ws = 48.f;
			const FVector Tip = B + D * (Hs * 0.32f) + FVector(0, 0, Hs);
			FZoogiMeshBuilder& S = k % 2 ? SpG : SpI;
			const FVector P0 = B - T2 * Ws - D * 20.f, P1 = B + T2 * Ws - D * 20.f, P2 = B + T2 * Ws + D * 28.f, P3 = B - T2 * Ws + D * 28.f;
			S.FlatTri(P0, P1, Tip, -D); S.FlatTri(P1, P2, Tip, T2); S.FlatTri(P2, P3, Tip, D); S.FlatTri(P3, P0, Tip, -T2);
			if (k % 2 == 0) Banner(B + D * 30.f - FVector(0, 0, 10.f), FMath::RadiansToDegrees(A) + 90.f, 120.f, 300.f, (k / 2) % 2 ? TEXT("#B8333F") : TEXT("#3346B8"));
		}
		Vis(Ring, StoneM); Vis(Rim, GoldM); Vis(SpG, GoldM); Vis(SpI, IvoryM);
		// the orb: glowing sphere + bright core + two gold bands, and the light beam into the sky
		const float OZ = PeakZ + 420.f;
		MeshC(SphereM, OrbM, FVector(0, 0, OZ), FVector(3.6f), FRotator::ZeroRotator, false);
		MeshC(SphereM, OrbCoreM, FVector(0, 0, OZ), FVector(2.2f), FRotator::ZeroRotator, false);
		for (int32 b = 0; b < 2; ++b)
		{
			USceneComponent* Pv = NewPivot(FVector(0, 0, OZ)); Pv->SetRelativeRotation(FRotator(b ? 65.f : -20.f, b ? 30.f : 120.f, 0.f));
			FZoogiMeshBuilder Band; Band.ArcSlab(196.f, 214.f, 0.f, 360.f, -9.f, 9.f, 48);
			Vis(Band, GoldM, false, Pv);
		}
		// (Oct 31 b41: the solid beam cylinder read as a pole in the captures - now a short crown of light rays above the orb)
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = 45.f * k;
			MeshC(ConeM, BeamM, FVector(0, 0, OZ + 330.f) + FRotator(0.f, A, 0.f).Vector() * 40.f, FVector(0.22f, 0.22f, k % 2 ? 4.5f : 6.5f), FRotator(-8.f, A, 0.f), false);
		}
		// rock ledges on the volcano flanks for the rope bridges
		for (int32 s = -1; s <= 1; s += 2)
		{
			FZoogiMeshBuilder Ld; const FVector2D C(s * 700.f, -430.f);
			Ld.Prism(C, 7, 120.f, 230.f, 1000.f, 1110.f, 20.f * s); Ld.Prism(C, 7, 30.f, 120.f, 760.f, 1000.f, 20.f * s);
			Vis(Ld, RockM);
		}
	}

	// ================================================================== FORTRESS WALLS (collision) - NW blue, NE red
	{
		struct FWall { FVector2D A, B; float Z, H; bool bBlue; };
		const FWall Ws[] = {
			{ { -4680.f, -2350.f }, { -4680.f, -3350.f }, 150.f, 150.f, true },   // blue fortress west edge (south of the terrace)
			{ { -3550.f, -4480.f }, { -2350.f, -4480.f }, 150.f, 150.f, true },   // blue fortress north edge (east of the terrace)
			{ { -4680.f, -3550.f }, { -4680.f, -4480.f }, 450.f, 90.f, true },    // terrace parapets
			{ { -4600.f, -4480.f }, { -3800.f, -4480.f }, 450.f, 90.f, true },
			{ { 4680.f, -2350.f }, { 4680.f, -3700.f }, 450.f, 150.f, false },    // red fortress east edge (gap at the portal corner)
			{ { 2350.f, -4480.f }, { 3700.f, -4480.f }, 450.f, 150.f, false },    // red fortress north edge
		};
		TArray<TArray<FVector>> WH;
		FZoogiMeshBuilder BlueV, RedV;
		for (const FWall& Wl : Ws)
		{
			const FVector2D D = (Wl.B - Wl.A).GetSafeNormal(); const float Len = FVector2D::Distance(Wl.A, Wl.B);
			const float Yw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			const FVector Mid(FVector2D((Wl.A + Wl.B) * 0.5f), Wl.Z + Wl.H * 0.5f);
			BoxHull(WH, Mid, FVector(Len * 0.5f, 28.f, Wl.H * 0.5f), Yw);
			FZoogiMeshBuilder& V = Wl.bBlue ? BlueV : RedV;
			for (float T = 0.f; T < Len - 10.f; T += 105.f) // chunky blocks + merlons
			{
				const float Seg = FMath::Min(100.f, Len - T);
				V.Box(FVector(Wl.A + D * (T + Seg * 0.5f), Wl.Z + Wl.H * 0.5f), FVector(Seg * 0.5f, 34.f, Wl.H * 0.5f), FRotator(0.f, Yw, 0.f));
				if (FMath::RoundToInt(T / 105.f) % 2 == 0) V.Box(FVector(Wl.A + D * (T + Seg * 0.5f), Wl.Z + Wl.H + 30.f), FVector(Seg * 0.42f, 30.f, 30.f), FRotator(0.f, Yw, 0.f));
			}
			if (Len > 800.f) Banner(FVector(Wl.A + D * Len * 0.5f + FVector2D(-D.Y, D.X) * 36.f * (Wl.bBlue ? 1.f : -1.f), Wl.Z + Wl.H - 5.f), Yw + 90.f, 110.f, 220.f, Wl.bBlue ? TEXT("#3346B8") : TEXT("#B8333F"));
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_fortress_walls"))) { O->BuildCollider(WH, 1.f, WallPMat); O->BounceGain = 1.1f; O->MaxOut = 1600.f; O->CenterBias = 0.5f; }
		Vis(BlueV, BlueWallM); Vis(RedV, RedWallM);
		// corner towers (visual, inside the wall line corners)
		Tower(FVector2D(-4620.f, -2300.f), 150.f, 700.f, 75.f, BlueWallM, 11);
		Tower(FVector2D(4620.f, -2300.f), 450.f, 1050.f, 75.f, RedWallM, 12);
		Tower(FVector2D(2300.f, -4430.f), 450.f, 1000.f, 70.f, RedWallM, 13);
	}

	// ================================================================== HUB RIM PILLARS: carved drum posts with gold Greek-key bands (rolling ref)
	{
		TArray<TArray<FVector>> PH;
		FZoogiMeshBuilder Dr, Bd, Cp;
		for (int32 k = 0; k < 8; ++k)
		{
			const float A = FMath::DegreesToRadians(22.5f + 45.f * k); const FVector2D C(FMath::Cos(A) * 1790.f, FMath::Sin(A) * 1790.f);
			TArray<FVector>& P = PH.AddDefaulted_GetRef();
			for (int32 j = 0; j < 10; ++j) { const float B = j * PI / 5.f; P.Add(FVector(C.X + FMath::Cos(B) * 72.f, C.Y + FMath::Sin(B) * 72.f, 0.f)); P.Add(FVector(C.X + FMath::Cos(B) * 72.f, C.Y + FMath::Sin(B) * 72.f, 230.f)); }
			SolidPosts.Add(C);
			Dr.Prism(C, 14, 78.f, 74.f, 0.f, 230.f, 0.f);
			Bd.Prism(C, 14, 81.f, 81.f, 40.f, 72.f, 0.f); Bd.Prism(C, 14, 81.f, 81.f, 170.f, 200.f, 0.f);
			Cp.Prism(C, 14, 88.f, 84.f, 230.f, 262.f, 0.f);
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_hub_pillars"))) { O->BuildCollider(PH, 1.f, WallPMat); O->BounceGain = 1.15f; O->MaxOut = 1800.f; O->CenterBias = 0.65f; } // b46: bounce back toward the middle
		Vis(Dr, StoneM); Vis(Bd, GoldM); Vis(Cp, StoneM);
	}
	// ================================================================== VOID TOWERS + ROPE BRIDGES + BANNERS + BUNTING
	{
		Tower(FVector2D(-2500.f, -1500.f), -1900.f, 950.f, 120.f, BlueWallM, 21);  // blue pillar tower in the void (left)
		Tower(FVector2D(2500.f, -1500.f), -1900.f, 950.f, 120.f, RedWallM, 22);    // red pillar tower (right)
		Tower(FVector2D(-1700.f, 2600.f), -1900.f, 260.f, 95.f, BlueWallM, 23);    // foreground blue block pillar
		Tower(FVector2D(1750.f, 2650.f), -1900.f, -120.f, 90.f, BlueWallM, 24);
		struct FBr { FVector A, B; };
		const FBr Bs[] = { { { -2500.f, -1500.f, 1030.f }, { -760.f, -470.f, 1110.f } }, { { 760.f, -470.f, 1110.f }, { 2500.f, -1500.f, 1030.f } },
			{ { -2560.f, -1600.f, 1030.f }, { -3760.f, -3560.f, 460.f } }, { { 2560.f, -1600.f, 1030.f }, { 3000.f, -2850.f, 460.f } } };
		int32 Bi = 0;
		for (const FBr& Bg : Bs)
		{
			AddBridge(Bg.A, Bg.B, 190.f, 140.f, WoodM, RopeM);
			const FVector D = Bg.B - Bg.A; const float Yw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			const int32 NB = FMath::Max(2, FMath::RoundToInt(FVector2D(D).Size() / 380.f));
			for (int32 k = 1; k < NB; ++k)
			{
				const float T = (float)k / NB; const FVector P = Bg.A + D * T - FVector(0, 0, 140.f * 4.f * T * (1.f - T) + 12.f);
				Banner(P, Yw + 90.f, 95.f, 230.f, (k + Bi) % 2 ? TEXT("#B8333F") : TEXT("#3346B8"));
			}
			++Bi;
		}
		// bunting strings: towers <-> crown, alternating triangles
		const TCHAR* FlagC[] = { TEXT("#E8463C"), TEXT("#3D63D6"), TEXT("#F2C230"), TEXT("#F4F0E6") };
		FZoogiMeshBuilder Fl[4], Line;
		const FVector Strings[][2] = { { { -2500.f, -1500.f, 1230.f }, { -300.f, -200.f, PeakZ + 260.f } }, { { 300.f, -200.f, PeakZ + 260.f }, { 2500.f, -1500.f, 1230.f } },
			{ { -4620.f, -2300.f, 900.f }, { -2500.f, -1500.f, 1230.f } }, { { 2500.f, -1500.f, 1230.f }, { 4620.f, -2300.f, 1250.f } } };
		for (const auto& Sg : Strings)
		{
			const FVector A = Sg[0], B = Sg[1], D = B - A; const FVector Side = FVector(-D.Y, D.X, 0.f).GetSafeNormal();
			const int32 N = FMath::Max(6, FMath::RoundToInt(D.Size() / 90.f));
			auto At = [&](float T) { return A + D * T - FVector(0, 0, 220.f * 4.f * T * (1.f - T)); };
			for (int32 i = 0; i < N; ++i)
			{
				const FVector P0 = At((float)i / N), P1 = At((float)(i + 1) / N);
				Line.Quad(P0 - FVector(0, 0, 2.f), P1 - FVector(0, 0, 2.f), P1 + FVector(0, 0, 2.f), P0 + FVector(0, 0, 2.f), Side);
				const FVector M0 = FMath::Lerp(P0, P1, 0.15f), M1 = FMath::Lerp(P0, P1, 0.85f), Tp = FMath::Lerp(P0, P1, 0.5f) - FVector(0, 0, 70.f);
				Fl[i % 4].FlatTri(M0, M1, Tp, Side); Fl[i % 4].FlatTri(M1, M0, Tp, -Side);
			}
		}
		Vis(Line, RopeM, false);
		for (int32 c = 0; c < 4; ++c) Vis(Fl[c], ZoogiUtil::SurfaceMID(this, Hex(FlagC[c]), 0.7f), false);
		// rope rails + posts along the walkable high bridge (visual)
		FZoogiMeshBuilder Posts;
		for (float X = -3600.f; X <= 2000.f; X += 400.f) for (int32 s = -1; s <= 1; s += 2) Posts.Box(FVector(X, -4000.f + s * 190.f, 510.f), FVector(8.f, 8.f, 60.f));
		Vis(Posts, WoodDarkM, false);
		for (int32 s = -1; s <= 1; s += 2) AddBoxR(FVector(-800.f, -4000.f + s * 190.f, 565.f), FVector(5600.f, 5.f, 5.f), FRotator::ZeroRotator, RopeM, false);
	}

	// ================================================================== STAR BOUNCE PADS (aimed springs) + CRATES
	{
		struct FStar { FVector C; FVector Target; };
		const FStar Ss[] = { { { -4200.f, 2450.f, 0.f }, { -1050.f, 1050.f, 0.f } }, { { 4450.f, -2350.f, 450.f }, { 1050.f, -1050.f, 0.f } } };
		int32 Si = 0;
		for (const FStar& S : Ss)
		{
			FZoogiMeshBuilder Base, Top, St;
			Base.Prism(FVector2D(S.C), 24, 150.f, 140.f, S.C.Z, S.C.Z + 26.f);
			Top.Prism(FVector2D(S.C), 24, 118.f, 118.f, S.C.Z + 26.f, S.C.Z + 30.f);
			ZoogiAncLook::Star(St, S.C + FVector(0, 0, 31.f), 100.f, 44.f, 0.f);
			Vis(Base, StarBlueM); Vis(Top, GoldM, false); Vis(St, StarYelM, false);
			if (AZoogiObstacle* O = Spawn(FString::Printf(TEXT("Ancient_starpad_%d"), Si++), FTransform(S.C)))
			{
				FZoogiObsTrigger& T = O->Triggers.AddDefaulted_GetRef();
				T.Kind = 8; T.Shape = 2; T.C = FVector(0, 0, 50.f); T.E = FVector(120.f, 0.f, 70.f); T.P[2] = 1.0f; T.Target = S.Target;
			}
		}
		const FVector Crates[] = { { -4550.f, -3300.f, 150.f }, { -2300.f, -3900.f, 150.f }, { 4500.f, -3600.f, 450.f }, { 2400.f, -4400.f, 450.f },
			{ -4500.f, 2300.f, 0.f }, { -2400.f, 4300.f, 0.f }, { 4550.f, 4450.f, 0.f }, { 2250.f, 2900.f, 0.f } };
		TArray<TArray<FVector>> CH;
		FZoogiMeshBuilder Cb, Cf;
		int32 Ci = 0;
		for (const FVector& C : Crates)
		{
			const float Yw = 17.f * Ci++; const float Hf = 48.f;
			BoxHull(CH, C + FVector(0, 0, Hf), FVector(Hf), Yw);
			Cb.Box(C + FVector(0, 0, Hf), FVector(Hf - 2.f), FRotator(0.f, Yw, 0.f));
			const FQuat Q = FRotator(0.f, Yw, 0.f).Quaternion();
			for (int32 e = 0; e < 4; ++e) // dark frame edges
			{
				const FVector Ox = Q.RotateVector(FVector((e & 1) ? Hf : -Hf, (e & 2) ? Hf : -Hf, 0.f));
				Cf.Box(C + FVector(0, 0, Hf) + Ox, FVector(7.f, 7.f, Hf + 1.f), FRotator(0.f, Yw, 0.f));
			}
			Cf.Box(C + FVector(0, 0, Hf * 2.f), FVector(Hf + 1.f, 7.f, 4.f), FRotator(0.f, Yw + 45.f, 0.f));
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_crates"))) { O->BuildCollider(CH, 1.f, WallPMat); O->BounceGain = 1.15f; O->MaxOut = 1800.f; O->CenterBias = 0.65f; }
		Vis(Cb, WoodM); Vis(Cf, WoodDarkM);
	}

	// ================================================================== THE RUIN VALLEY BELOW (visual)
	{
		const float VZ = -1900.f;
		FZoogiMeshBuilder G, Pa, Dr;
		auto Disc = [](FZoogiMeshBuilder& M, const FVector2D& C, float R, float Z, int32 N)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N;
				M.FlatTri(FVector(C, Z), FVector(C + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, Z), FVector(C + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, Z), FVector::UpVector);
			}
		};
		Disc(G, FVector2D::ZeroVector, 14000.f, VZ, 64);
		FRandomStream R(4411);
		for (int32 i = 0; i < 26; ++i) // orange checker plazas + dirt paths
		{
			const FVector2D C(R.FRandRange(-9000.f, 9000.f), R.FRandRange(-9000.f, 9000.f));
			const float Hx = R.FRandRange(500.f, 1300.f), Hy = R.FRandRange(500.f, 1300.f);
			FZoogiMeshBuilder& M = i % 3 == 0 ? Dr : Pa;
			M.Quad(FVector(C + FVector2D(-Hx, -Hy), VZ + 3.f), FVector(C + FVector2D(Hx, -Hy), VZ + 3.f), FVector(C + FVector2D(Hx, Hy), VZ + 3.f), FVector(C + FVector2D(-Hx, Hy), VZ + 3.f), FVector::UpVector);
		}
		// b41: softer, deeper-reading valley floor (the 1600-scale emerald checker read as a floor right under the islands)
		UMaterialInterface* VGrassM = Tile(TEXT("T_AncGrassTile"), TEXT("T_AncRockTile"), 2600.f, 900.f, TEXT("#4E8A3E"), TEXT("#8CC468"), 0.78f);
		UMaterialInterface* VOrangeM = Tile(TEXT("T_AncNorthTile"), TEXT("T_AncWallTile"), 2200.f, 460.f, TEXT("#C8703A"), TEXT("#E0A07A"), 0.72f);
		Vis(G, VGrassM, false); Vis(Pa, VOrangeM, false); Vis(Dr, DirtM, false);
		// horseshoe ruin arch, stone pillars, little huts, palms
		{
			FZoogiMeshBuilder Ar;
			const FVector2D C(-300.f, -2900.f);
			for (int32 k = 0; k <= 12; ++k)
			{
				const float A = FMath::DegreesToRadians(-20.f + 220.f * k / 12.f);
				const FVector P(C.X + FMath::Cos(A) * 420.f, C.Y, VZ + 60.f + FMath::Sin(A) * 520.f + 520.f * 0.f);
				Ar.Box(FVector(C.X + FMath::Cos(A) * 420.f, C.Y, VZ + 420.f + FMath::Sin(A) * 420.f), FVector(70.f, 90.f, 70.f), FRotator(FMath::RadiansToDegrees(A), 0.f, 0.f));
				(void)P;
			}
			Ar.Prism(C, 10, 260.f, 240.f, VZ, VZ + 120.f, 0.f);
			Vis(Ar, StoneM);
		}
		FZoogiMeshBuilder Pil, Hut, Roof;
		const FVector2D PilP[] = { { -1500.f, -2600.f }, { 1500.f, -2600.f }, { 900.f, -3500.f }, { -2600.f, 900.f }, { 2700.f, 600.f }, { 600.f, 3000.f }, { -900.f, 3900.f } };
		int32 Pi = 0;
		for (const FVector2D& P : PilP) { const float Ht = 600.f + 220.f * (Pi % 3); Pil.Prism(P, 4, 110.f, 95.f, VZ, VZ + Ht, 45.f); Pil.Prism(P, 4, 150.f, 150.f, VZ + Ht, VZ + Ht + 50.f, 45.f); ++Pi; }
		const FVector2D HutP[] = { { 2100.f, -2300.f }, { -2300.f, -2700.f }, { -3000.f, 1300.f }, { 3200.f, 1500.f }, { 200.f, 4300.f } };
		for (const FVector2D& P : HutP) { Hut.Box(FVector(P, VZ + 110.f), FVector(140.f, 120.f, 110.f)); Roof.Prism(P, 4, 200.f, 10.f, VZ + 220.f, VZ + 330.f, 45.f); }
		Vis(Pil, StoneM); Vis(Hut, StoneM); Vis(Roof, ZoogiUtil::SurfaceMID(this, Hex(TEXT("#C2563A")), 0.7f));
		for (int32 i = 0; i < 34; ++i)
		{
			const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(2300.f, 9000.f);
			Palm(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, VZ), R.FRandRange(420.f, 640.f));
		}
		// lava falls off the red fortress and the hub rim (existing kit waterfall with the lava material)
		AddWaterfall(FVector(4000.f, -4512.f, 435.f), -90.f, 300.f, 2350.f, 70.f, LavaM);
		AddWaterfall(FVector(4712.f, -2700.f, 435.f), 0.f, 260.f, 2350.f, 70.f, LavaM);
		AddWaterfall(FVector(1640.f, -950.f, -15.f), -30.f, 220.f, 1900.f, 60.f, LavaM);
		AddWaterfall(FVector(-1640.f, -950.f, -15.f), -150.f, 260.f, 1900.f, 60.f);
		// floating stepping stones (below the play line: never land on them)
		FZoogiMeshBuilder StnB, StnM, StnL;
		const FVector Stones[] = { { -900.f, -2900.f, -950.f }, { 300.f, -2400.f, -780.f }, { 1200.f, -3100.f, -1100.f }, { -2000.f, -400.f, -900.f },
			{ 2100.f, 300.f, -1000.f }, { -700.f, 2600.f, -820.f }, { 900.f, 3600.f, -1050.f }, { 2900.f, 1800.f, -900.f }, { -2900.f, 1700.f, -1150.f }, { 0.f, -1950.f, -1300.f } };
		int32 Sk = 0;
		for (const FVector& S : Stones)
		{
			const float Hx = 160.f + 40.f * (Sk % 3), Hy = 140.f + 30.f * (Sk % 2);
			StnB.Box(S, FVector(Hx, Hy, 70.f), FRotator(0.f, 13.f * Sk, 0.f));
			(Sk % 4 == 1 ? StnL : StnM).Box(S + FVector(0, 0, 76.f), FVector(Hx + 6.f, Hy + 6.f, 8.f), FRotator(0.f, 13.f * Sk, 0.f));
			++Sk;
		}
		Vis(StnB, RedWallM); Vis(StnM, MossM); Vis(StnL, LavaM, false);
	}

	// ================================================================== SKY: blimp crowns + distant temple islands
	for (const TObjectPtr<USceneComponent>& Pv : AncMovers)
	{
		if (!Pv) continue;
		for (int32 s = -1; s <= 1; s += 2)
		{
			FZoogiMeshBuilder Cw;
			const float Yo = s * 172.f;
			Cw.Box(FVector(0.f, Yo, -10.f), FVector(70.f, 3.f, 18.f));
			for (int32 p = -1; p <= 1; ++p) { const FVector Bp(p * 50.f, Yo, 8.f); Cw.FlatTri(Bp + FVector(-22.f, 0, 0), Bp + FVector(22.f, 0, 0), Bp + FVector(0, 0, p == 0 ? 70.f : 48.f), FVector(0, (float)s, 0)); Cw.FlatTri(Bp + FVector(22.f, 0, 0), Bp + FVector(-22.f, 0, 0), Bp + FVector(0, 0, p == 0 ? 70.f : 48.f), FVector(0, (float)-s, 0)); }
			Vis(Cw, GoldM, false, Pv.Get());
		}
	}
	{
		FRandomStream R(919);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = FMath::DegreesToRadians(200.f + i * 22.f + R.FRandRange(-6.f, 6.f)), D = R.FRandRange(10500.f, 15500.f);
			const FVector C(FMath::Cos(A) * D, FMath::Sin(A) * D, R.FRandRange(-200.f, 1400.f));
			FZoogiMeshBuilder Isl, Col, Rf;
			Isl.Prism(FVector2D(C), 9, 120.f, 900.f, C.Z - 1300.f, C.Z, 7.f);
			Isl.Prism(FVector2D(C), 9, 900.f, 880.f, C.Z, C.Z + 60.f, 7.f);
			for (int32 c = 0; c < 6; ++c) { const FVector2D Q = FVector2D(C) + FVector2D(-450.f + 180.f * c, 0.f); Col.Prism(Q, 8, 55.f, 48.f, C.Z + 60.f, C.Z + 760.f, 0.f); }
			if (i % 2 == 0) Rf.Box(FVector(C.X, C.Y, C.Z + 800.f), FVector(560.f, 160.f, 40.f));
			Vis(Isl, RockM, false); Vis(Col, IvoryM, false); Vis(Rf, IvoryM, false);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT LOOK: Battle Royale reference look built (volcano + crown orb, fortress walls, rope bridges, star pads, crates, ruin valley)"));
}
