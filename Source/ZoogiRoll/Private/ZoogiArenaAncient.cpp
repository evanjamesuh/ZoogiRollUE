// Oct 31 (Evan brief "Ancient Collision Meadows"): Phase 2 PLAYABLE BLOCKOUT of the multi-island arena (EZoogiArena::Ancient, id
// 'ancient'). Layout numbers live in ZoogiAncientLayout.h. Collision = AZoogiObstacle convex hulls (the Frozen Ring 2 method:
// floors, decks, ramps, curbs, bumpers); visuals = simple colour-coded procedural slabs (no collision, the art pass replaces
// them). Ring-out stays the shared rule (below FallOutZ -600). Nothing here touches the other arenas.
//   -ZoogiAncientTest : scripted traversal test (every route + recovery springs + a fall-out control), logs
//                       "ANCIENT route <name> ... PASS/FAIL" and "ANCIENT SUMMARY", chase-cam captures to -ZoogiShots=<dir>.
#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
#include "ZoogiObstacle.h"
#include "ZoogiMeshKit.h"
#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiGameMode.h"
#include "ZoogiRock.h"
#include "Engine/Texture.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Containers/Ticker.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"

namespace ZoogiAncientBuild
{
	using namespace ZoogiAncient;
	// convex slab hull + visual: polygon top at Z, bottom at Z - Thick
	void PolyHull(TArray<TArray<FVector>>& H, const TArray<FVector2D>& P, float Z, float Thick)
	{
		TArray<FVector>& Pts = H.AddDefaulted_GetRef();
		for (const FVector2D& V : P) { Pts.Add(FVector(V, Z)); Pts.Add(FVector(V, Z - Thick)); }
	}
	void PolyMesh(FZoogiMeshBuilder& M, const TArray<FVector2D>& P, float Z, float Thick, bool bTop = true, bool bSides = true)
	{
		FVector2D C(0, 0); for (const FVector2D& V : P) C += V; C /= FMath::Max(1, P.Num());
		for (int32 i = 0; i < P.Num(); ++i)
		{
			const FVector2D A = P[i], B = P[(i + 1) % P.Num()];
			if (bTop) M.FlatTri(FVector(C, Z), FVector(A, Z), FVector(B, Z), FVector::UpVector);
			if (bSides)
			{
				const FVector2D Mid = (A + B) * 0.5f;
				M.Quad(FVector(A, Z - Thick), FVector(B, Z - Thick), FVector(B, Z), FVector(A, Z), FVector(FVector2D(Mid - C).GetSafeNormal(), 0.f));
			}
		}
	}
	// ramp deck: 8 corners (top quad along the slope + parallel bottom; solid ramps get a flat bottom)
	void RampCorners(const FRamp& R, FVector Out[8], float BotOverride = 1e9f)
	{
		const FVector2D A2(R.A), B2(R.B), D = (B2 - A2).GetSafeNormal(), GAncS(-D.Y, D.X);
		const float Len = FVector2D::Distance(A2, B2);
		const float Slope = Len > 1.f ? (R.B.Z - R.A.Z) / Len : 0.f;
		const FVector2D PA = A2 - D * R.ExtendA, PB = B2 + D * R.ExtendB;
		const float ZA = R.A.Z - Slope * R.ExtendA, ZB = R.B.Z + Slope * R.ExtendB;
		const bool bSolid = R.Thick >= 200.f;
		const float Bot = BotOverride < 1e8f ? BotOverride : FMath::Min(R.A.Z, R.B.Z) - 20.f;
		const FVector2D C[4] = { PA + GAncS * R.HalfW, PB + GAncS * R.HalfW, PB - GAncS * R.HalfW, PA - GAncS * R.HalfW };
		const float Zt[4] = { ZA, ZB, ZB, ZA };
		for (int32 i = 0; i < 4; ++i) { Out[i] = FVector(C[i], Zt[i]); Out[i + 4] = FVector(C[i], bSolid ? FMath::Min(Bot, Zt[i] - 20.f) : Zt[i] - R.Thick); }
	}
	void RampMesh(FZoogiMeshBuilder& M, const FVector Cn[8])
	{
		M.Quad(Cn[0], Cn[1], Cn[2], Cn[3], FVector::UpVector);
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 j = (i + 1) % 4;
			const FVector Mid = (Cn[i] + Cn[j]) * 0.5f, Ctr = (Cn[0] + Cn[2]) * 0.5f;
			M.Quad(Cn[i + 4], Cn[j + 4], Cn[j], Cn[i], FVector(FVector2D(Mid - Ctr).GetSafeNormal(), 0.f));
		}
	}
	// side curbs along a ramp: thin slabs riding the deck edges
	void CurbHulls(const FRamp& R, TArray<TArray<FVector>>& H, FZoogiMeshBuilder& M)
	{
		if (R.Curb <= 0.f) return;
		const FVector2D A2(R.A), B2(R.B), D = (B2 - A2).GetSafeNormal(), GAncS(-D.Y, D.X);
		for (int32 s = -1; s <= 1; s += 2)
		{
			TArray<FVector>& Pts = H.AddDefaulted_GetRef();
			FVector C[8];
			const float W0 = R.HalfW * s, W1 = (R.HalfW + 22.f) * s;
			const FVector2D P[4] = { A2 + GAncS * W0, B2 + GAncS * W0, B2 + GAncS * W1, A2 + GAncS * W1 };
			const float Z[4] = { R.A.Z, R.B.Z, R.B.Z, R.A.Z };
			for (int32 i = 0; i < 4; ++i) { C[i] = FVector(P[i], Z[i] + R.Curb); C[i + 4] = FVector(P[i], Z[i] - 25.f); Pts.Add(C[i]); Pts.Add(C[i + 4]); }
			RampMesh(M, C);
		}
	}
	FVector2D Centroid(const TArray<FVector2D>& P) { FVector2D C(0, 0); for (const FVector2D& V : P) C += V; return C / FMath::Max(1, P.Num()); }
	float Extent(const TArray<FVector2D>& P) { const FVector2D C = Centroid(P); float R = 0.f; for (const FVector2D& V : P) R = FMath::Max(R, (float)FVector2D::Distance(V, C)); return R; }
}

void ZoogiAncientStartTest(AZoogiArena* Arena);

// Oct 31: MSVC C1001 (internal compiler error in the p2 optimizer) on this long one-shot builder -> build it unoptimized on
// Windows (runs once per arena load; clang / iOS unaffected).
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", off)
#endif
void AZoogiArena::BuildAncient()
{
	if (ZoogiAncient::V14()) { BuildAncientV14(); return; }
	using namespace ZoogiAncient;
	using namespace ZoogiAncientBuild;
	using ZoogiUtil::Hex;
	const FLayout& L = Get();
	const bool bV2 = Meadow2(); // Nov 1: v2 sunset meadow look (visual swaps below; collision unchanged)
	bAncientActive = true;
	AncMovers.Reset(); AncMoverPath.Reset(); AncClock = 0.f;
	BuildScale = 1.f; SetActorScale3D(FVector(1.f)); CamScale = 1.6f;
	KnockoffRadiusOverride = L.HubR; ActiveKnockoffR = L.HubR;       // the hub is the "court" for the round-court code paths
	SpawnRadiusOverride = ZoogiConst::SpawnRadius;                   // spawns on the hub (r1100, cardinals: clear of the diagonal bridges)
	OrbCountOverride = 20; OrbRingRadiusOverride = 1500.f; OrbRingYawOffset = 15.f; // Oct 31: ring around the volcano (base r900)
	FloorPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.1f, true);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);
	UPhysicalMaterial* DeckPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.2f, false);
	UPhysicalMaterial* CurbPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.5f, false);
	UPhysicalMaterial* BumpPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 1.0f, false);
	UPhysicalMaterial* StonePM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.6f, false);
	UStaticMesh* SphereM = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* CylM = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* ConeM = ZoogiUtil::BasicShape(TEXT("Cone"));
	UStaticMesh* CubeM = ZoogiUtil::BasicShape(TEXT("Cube"));

	// ---- art pass materials: M_AncientTile (Scripts/arenas/ancient/make_ancient_kit.py) = world-space top tile + side masonry.
	//      Tiles (gen_ancient_tiles.py): Marble-Clash-style chunky bevelled squares in the refs' palette. Falls back to the blockout colours.
	auto Ground = [&](const TCHAR* A, const TCHAR* B, float N = 0.003f) { return (UMaterialInterface*)ZoogiUtil::GroundMID(this, Hex(A), Hex(B), N, 0.9f); };
	UMaterialInterface* TileBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_AncientTile.M_AncientTile"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto TexA = [](const TCHAR* N) { return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), N, N), nullptr, LOAD_NoWarn | LOAD_Quiet); };
	auto Tile = [&](const TCHAR* Top, const TCHAR* Side, float TS, float SS, const TCHAR* FA, const TCHAR* FB, float Bright = 1.f, const TCHAR* Tint = nullptr) -> UMaterialInterface*
	{
		UTexture* T = TexA(Top); UTexture* GAncS = TexA(Side);
		if (!TileBase || !T || !GAncS) return Ground(FA, FB);
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(TileBase, this);
		M->SetTextureParameterValue(TEXT("TopTex"), T); M->SetTextureParameterValue(TEXT("SideTex"), GAncS);
		M->SetScalarParameterValue(TEXT("TopScale"), TS); M->SetScalarParameterValue(TEXT("SideScale"), SS); M->SetScalarParameterValue(TEXT("Bright"), Bright);
		if (Tint) { M->SetVectorParameterValue(TEXT("TopTint"), Hex(Tint)); M->SetVectorParameterValue(TEXT("SideTint"), Hex(Tint)); }
		return M;
	};
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT: art kit %s"), TileBase ? TEXT("M_AncientTile + T_Anc* tiles") : TEXT("MISSING (blockout colours)"));
	UMaterialInterface* RegM[NumRegions] = {};
	RegM[Hub] = Tile(TEXT("T_AncHubTile"), TEXT("T_AncWallTile"), 700.f, 460.f, TEXT("#D9C9A3"), TEXT("#BFAE86"));
	RegM[North] = Tile(TEXT("T_AncCitadelTile"), TEXT("T_AncWallBlueTile"), 1000.f, 320.f, TEXT("#C8573E"), TEXT("#A7452F"));
	RegM[Citadel] = Tile(TEXT("T_AncNorthTile"), TEXT("T_AncWallTile"), 1000.f, 460.f, TEXT("#5B86D6"), TEXT("#4069B4"));
	RegM[Emerald] = Tile(TEXT("T_AncNorthTile"), TEXT("T_AncWallTile"), 1100.f, 460.f, TEXT("#66BE3E"), TEXT("#4E9E2C"));
	RegM[Sunstone] = Tile(TEXT("T_AncEmeraldTile"), TEXT("T_AncWallBlueTile"), 1000.f, 320.f, TEXT("#E8A456"), TEXT("#CC8642"));
	RegM[Bridge] = Tile(TEXT("T_AncWallBlueTile"), TEXT("T_AncWallBlueTile"), 300.f, 260.f, TEXT("#4F6FC0"), TEXT("#3D5AA6")); // ref: blue brick bridges with gold arrows
	RegM[Recovery] = Tile(TEXT("T_AncGrassTile"), TEXT("T_AncRockTile"), 500.f, 600.f, TEXT("#4E8A3E"), TEXT("#3A6D2E"));
	RegM[Terrace] = Tile(TEXT("T_AncCitadelTile"), TEXT("T_AncWallBlueTile"), 1000.f, 320.f, TEXT("#B04A35"), TEXT("#8E3A28"), 0.92f);
	UMaterialInterface* WallM = Tile(TEXT("T_AncWallTile"), TEXT("T_AncWallTile"), 460.f, 460.f, TEXT("#A99273"), TEXT("#8E7859"));
	UMaterialInterface* WallBlueM = Tile(TEXT("T_AncWallBlueTile"), TEXT("T_AncWallBlueTile"), 300.f, 260.f, TEXT("#5B78B8"), TEXT("#46619E"));
	UMaterialInterface* RockM = Tile(TEXT("T_AncRockTile"), TEXT("T_AncRockTile"), 900.f, 700.f, TEXT("#7A6A56"), TEXT("#55483A"));
	UMaterialInterface* PolishM = Tile(TEXT("T_AncPolishTile"), TEXT("T_AncWallTile"), 260.f, 300.f, TEXT("#EFE4C6"), TEXT("#E2D5B2"));
	UMaterialInterface* CurbM = Tile(TEXT("T_AncWallBlueTile"), TEXT("T_AncWallBlueTile"), 260.f, 260.f, TEXT("#3D5AA6"), TEXT("#2F4A8E"), 0.85f);
	UMaterialInterface* GoldM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F2C230")), 0.32f, Hex(TEXT("#F2C230")), 0.45f, 0.75f);
	UMaterialInterface* BlueGlow = ZoogiUtil::GlowMID(this, Hex(TEXT("#3FB4FF")), 1.4f);
	UMaterialInterface* SpringM = ZoogiUtil::GlowMID(this, Hex(TEXT("#FFD23F")), 1.1f);
	UMaterialInterface* BumperM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#2F6BE0")), 0.35f, Hex(TEXT("#3FB4FF")), 0.3f, 0.2f);
	UMaterialInterface* BumperTopM = ZoogiUtil::GlowMID(this, Hex(TEXT("#E84A3A")), 0.9f);
	UMaterialInterface* WoodM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#8A5A32")), 0.8f);
	UMaterialInterface* LeafM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#2FA03A")), 0.7f);
	UMaterialInterface* LeafM2 = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#43B83E")), 0.7f);
	UMaterialInterface* CloudM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#FFFFFF")), 0.95f, Hex(TEXT("#DCE9FF")), 0.35f);
	UMaterialInterface* StoneV2M = WallM;
	if (bV2) // Nov 1 v2 (ref meadow_ref.png): cream pavers on every island, purple decks with gold rails, red sandstone cliffs
	{
		RegM[Hub] = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncPaverTile"), 420.f, 300.f, TEXT("#E2CFA6"), TEXT("#D2BE94"));
		RegM[North] = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncCliffTile"), 380.f, 520.f, TEXT("#E2CFA6"), TEXT("#D2BE94"), 1.0f, TEXT("#ECE6E0"));
		RegM[Citadel] = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncCliffTile"), 380.f, 520.f, TEXT("#E2CFA6"), TEXT("#D2BE94"), 1.0f, TEXT("#ECE6E0"));
		RegM[Emerald] = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncCliffTile"), 380.f, 520.f, TEXT("#E2CFA6"), TEXT("#D2BE94"), 1.0f, TEXT("#ECE6E0"));
		RegM[Sunstone] = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncCliffTile"), 380.f, 520.f, TEXT("#E2CFA6"), TEXT("#D2BE94"), 1.0f, TEXT("#ECE6E0"));
		RegM[Terrace] = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncCliffTile"), 380.f, 520.f, TEXT("#E2CFA6"), TEXT("#D2BE94"), 0.95f, TEXT("#F4E6D2"));
		RegM[Bridge] = Tile(TEXT("T_AncPurpleTile"), TEXT("T_AncPaverTile"), 320.f, 260.f, TEXT("#5B2FB0"), TEXT("#4A2490"), 1.1f);
		RegM[Recovery] = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#5DBB3C")), 0.95f);
		PolishM = Tile(TEXT("T_AncPurpleTile"), TEXT("T_AncPaverTile"), 320.f, 260.f, TEXT("#6A3CC8"), TEXT("#5A30B0"), 1.3f);
		WallM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#76584E")), 0.92f); // low-poly (Evan poly ref): flat colours
		WallBlueM = WallM;
		RockM = WallM;
		StoneV2M = Tile(TEXT("T_AncPaverTile"), TEXT("T_AncPaverTile"), 260.f, 220.f, TEXT("#D8C29A"), TEXT("#C8B288"), 1.0f, TEXT("#F0E2C8"));
		CurbM = GoldM;
	}

	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto Spawn = [&](const FString& Name, const FTransform& X = FTransform::Identity) -> AZoogiObstacle*
	{
		AZoogiObstacle* O = GetWorld()->SpawnActor<AZoogiObstacle>(AZoogiObstacle::StaticClass(), X, SP);
		if (O) { O->ObsName = FName(*Name); Obstacles.Add(O); }
		return O;
	};
	auto Vis = [&](const FZoogiMeshBuilder& M, UMaterialInterface* Mat, bool bShadow = true)
	{
		if (M.V.Num() == 0) return;
		if (UProceduralMeshComponent* C = AddFrozenProc(M, Mat, nullptr, bShadow)) C->ComponentTags.Add(TEXT("NoCamBlock"));
	};
	auto BoxHullW = [](TArray<TArray<FVector>>& H, const FVector& C, const FVector& Half, const FRotator& R)
	{
		TArray<FVector>& P = H.AddDefaulted_GetRef(); const FQuat Q = R.Quaternion();
		for (int32 i = 0; i < 8; ++i) P.Add(C + Q.RotateVector(FVector((i & 1) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 4) ? Half.Z : -Half.Z)));
	};
	auto CylHull = [](TArray<TArray<FVector>>& H, const FVector& Base, float R, float Ht)
	{
		TArray<FVector>& P = H.AddDefaulted_GetRef();
		for (int32 j = 0; j < 12; ++j) { const float B = j * PI / 6.f; for (float Z : { 0.f, Ht }) P.Add(Base + FVector(FMath::Cos(B) * R, FMath::Sin(B) * R, Z)); }
	};
	auto Mesh = [&](UStaticMesh* M, UMaterialInterface* Mat, const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator, bool bShadow = true, USceneComponent* Parent = nullptr) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = AddMesh(M, Mat, false, bShadow);
		if (!C) return nullptr;
		if (Parent) C->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeLocation(Loc); C->SetRelativeRotation(Rot); C->SetRelativeScale3D(Scale);
		C->ComponentTags.Add(TEXT("NoCamBlock"));
		return C;
	};
	// banner: two-sided cloth panel with a gold crown mark, hanging from Top (centre of the top edge), facing Yaw
	auto Banner = [&](const FVector& Top, float Yaw, float W, float H, const TCHAR* Col)
	{
		const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion(); const FVector X = Q.RotateVector(FVector::RightVector), N = Q.RotateVector(FVector::ForwardVector);
		FZoogiMeshBuilder Cl, Mk;
		const FVector A = Top - X * W * 0.5f, B = Top + X * W * 0.5f, Cb = Top - FVector(0, 0, H), Tip = Cb - FVector(0, 0, H * 0.18f);
		for (int32 s = -1; s <= 1; s += 2)
		{
			Cl.Quad(A, B, B - FVector(0, 0, H), A - FVector(0, 0, H), N * s);
			Cl.FlatTri(A - FVector(0, 0, H), B - FVector(0, 0, H), Tip, N * s);
			const FVector O = N * (2.f * s), Mc = Top - FVector(0, 0, H * 0.42f) + O; const float K = W * 0.28f;
			Mk.Quad(Mc - X * K + FVector(0, 0, -K * 0.35f), Mc + X * K + FVector(0, 0, -K * 0.35f), Mc + X * K + FVector(0, 0, K * 0.05f), Mc - X * K + FVector(0, 0, K * 0.05f), N * s);
			for (int32 p = -1; p <= 1; ++p) { const FVector Bp = Mc + X * (p * K * 0.8f) + FVector(0, 0, K * 0.05f); Mk.FlatTri(Bp - X * K * 0.28f, Bp + X * K * 0.28f, Bp + FVector(0, 0, K * (p == 0 ? 0.75f : 0.55f)), N * s); }
		}
		Vis(Cl, ZoogiUtil::SurfaceMID(this, Hex(bV2 ? TEXT("#6A28C0") : Col), 0.75f), false); Vis(Mk, GoldM, false);
	};

	// ---- floors (hub, regions, terrace): collision slab + tiled top + masonry sides + kerb + craggy rock underside
	for (const FPlat& P : L.Plats)
	{
		TArray<TArray<FVector>> H; PolyHull(H, P.P, P.Z, P.Thick);
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_") + P.Name.ToString())) O->BuildCollider(H, 1.f, FloorPM);
		FZoogiMeshBuilder Top, Side;
		PolyMesh(Top, P.P, P.Z, P.Thick, true, false);
		PolyMesh(Side, P.P, P.Z, P.Thick, false, true);
		Vis(Top, RegM[P.Region]); Vis(Side, (P.Region == North || P.Region == Terrace || P.Region == Sunstone) ? WallBlueM : WallM);
		if (P.Region != Terrace && !bV2) // v2: rock mesas down to the meadow (BuildAncientMeadow)
		{
			FZoogiMeshBuilder U, U2;
			const FVector2D C = Centroid(P.P); const float R = Extent(P.P);
			U.Prism(C, 9, R * 0.12f, R * 0.88f, P.Z - P.Thick - 1600.f, P.Z - P.Thick + 2.f, 11.f);
			FRandomStream UR(P.Name.GetNumber() + (int32)P.Z + 77);
			for (int32 k = 0; k < 5; ++k) { const float A = UR.FRandRange(0.f, 2.f * PI); const FVector2D Q = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * UR.FRandRange(0.35f, 0.6f); U2.Prism(Q, 7, 20.f, R * UR.FRandRange(0.2f, 0.32f), P.Z - P.Thick - UR.FRandRange(700.f, 1300.f), P.Z - P.Thick + 1.f, UR.FRandRange(0.f, 60.f)); }
			Vis(U, RockM, false); Vis(U2, RockM, false);
		}
		// raised stone kerb along every edge (reads the island edge like the refs; 2 cm high, no gameplay effect)
		if (P.Region != Hub && !bV2)
		{
			FZoogiMeshBuilder K; const FVector2D Cc = Centroid(P.P);
			for (int32 i = 0; i < P.P.Num(); ++i)
			{
				const FVector2D A = P.P[i], B = P.P[(i + 1) % P.P.Num()];
				const FVector2D In0 = A + (Cc - A).GetSafeNormal() * 45.f, In1 = B + (Cc - B).GetSafeNormal() * 45.f;
				K.Quad(FVector(A, P.Z + 2.f), FVector(B, P.Z + 2.f), FVector(In1, P.Z + 2.f), FVector(In0, P.Z + 2.f), FVector::UpVector);
			}
			Vis(K, (P.Region == North || P.Region == Terrace || P.Region == Sunstone) ? WallBlueM : CurbM, false);
		}
	}
	// ---- bridges / links / ramps: eased deck segments (RampProfile) + curbs (collision) + visuals; momentum lanes polished
	for (const FRamp& R : L.Ramps)
	{
		const bool bSlope = FMath::Abs(R.B.Z - R.A.Z) > 1.f;
		const int32 NS = bSlope ? 8 : 1;
		const float BotZ = FMath::Min(R.A.Z, R.B.Z) - 20.f;
		TArray<TArray<FVector>> H, CH; FZoogiMeshBuilder Deck, Curbs;
		for (int32 k = 0; k < NS; ++k)
		{
			const float T0 = (float)k / NS, T1 = (float)(k + 1) / NS;
			FRamp GAncS = R;
			GAncS.A = FVector(FMath::Lerp(FVector2D(R.A), FVector2D(R.B), T0), FMath::Lerp(R.A.Z, R.B.Z, RampProfile(T0)));
			GAncS.B = FVector(FMath::Lerp(FVector2D(R.A), FVector2D(R.B), T1), FMath::Lerp(R.A.Z, R.B.Z, RampProfile(T1)));
			GAncS.ExtendA = k == 0 ? R.ExtendA : 0.f; GAncS.ExtendB = k == NS - 1 ? R.ExtendB : 0.f;
			FVector Cn[8]; RampCorners(GAncS, Cn, BotZ);
			TArray<FVector>& Pts = H.AddDefaulted_GetRef(); for (const FVector& V : Cn) Pts.Add(V);
			RampMesh(Deck, Cn);
			CurbHulls(GAncS, CH, Curbs);
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_") + R.Name.ToString())) O->BuildCollider(H, 1.f, DeckPM);
		if (CH.Num()) if (AZoogiObstacle* O = Spawn(TEXT("Ancient_") + R.Name.ToString() + TEXT("_curbs"))) O->BuildCollider(CH, 1.f, CurbPM);
		Vis(Deck, R.Region == Terrace ? RegM[Terrace] : (R.bMomentum ? PolishM : RegM[Bridge]));
		Vis(Curbs, CurbM);
		const FVector2D A2(R.A), B2(R.B), D = (B2 - A2).GetSafeNormal(), Sd(-D.Y, D.X);
		const float Len = FVector2D::Distance(A2, B2);
		// gold chevrons down the deck (direction markings, visual)
		{
			FZoogiMeshBuilder Ch;
			for (float t = 250.f; t < Len - 150.f; t += 500.f)
			{
				const float Z = FMath::Lerp(R.A.Z, R.B.Z, RampProfile(t / Len)) + 1.5f;
				const FVector2D C = A2 + D * t;
				for (int32 s = -1; s <= 1; s += 2)
				{
					const FVector2D Tip = C + D * 60.f, Back = C - D * 40.f + Sd * (s * R.HalfW * 0.45f), In = Back + D * 45.f;
					Ch.Quad(FVector(Back, Z), FVector(In, Z), FVector(Tip + D * 45.f, Z), FVector(Tip, Z), FVector::UpVector);
				}
			}
			Vis(Ch, GoldM, false);
		}
		// Oct 31: arched stone underside (the refs' bridges are stone arches; long piers into the clouds read as stilts): a keel
		// under the deck, deepest between the ends (40 cm at the ends, +180 cm mid-span per arch), one arch per ~14 m. Visual only.
		if (R.Thick < 200.f && R.Region != Terrace)
		{
			FZoogiMeshBuilder Keel;
			const FVector2D KSide(-D.Y, D.X);
			const int32 KArch = FMath::Max(1, FMath::RoundToInt(Len / 1400.f)), KN = 10 * KArch;
			auto Under = [&](float t) { const float Arch = FMath::Abs(FMath::Sin(t * KArch * PI)); return 40.f + 180.f * (1.f - Arch); };
			for (int32 k = 0; k < KN; ++k)
			{
				const float t0 = (float)k / KN, t1 = (float)(k + 1) / KN;
				const FVector2D C0 = A2 + D * Len * t0, C1 = A2 + D * Len * t1;
				const float Z0 = FMath::Lerp(R.A.Z, R.B.Z, RampProfile(t0)) - R.Thick, Z1 = FMath::Lerp(R.A.Z, R.B.Z, RampProfile(t1)) - R.Thick;
				const float W = R.HalfW * 0.92f;
				const FVector a0(C0 + KSide * W, Z0), b0(C0 - KSide * W, Z0), a1(C1 + KSide * W, Z1), b1(C1 - KSide * W, Z1);
				const FVector a0d = a0 - FVector(0, 0, Under(t0)), b0d = b0 - FVector(0, 0, Under(t0)), a1d = a1 - FVector(0, 0, Under(t1)), b1d = b1 - FVector(0, 0, Under(t1));
				Keel.Quad(a0d, a1d, a1, a0, FVector(KSide, 0.f));
				Keel.Quad(b0, b1, b1d, b0d, FVector(-KSide, 0.f));
				Keel.Quad(a0d, b0d, b1d, a1d, -FVector::UpVector);
			}
			Vis(Keel, StoneV2M, false);
		}
		if (R.bMomentum) // polished stone: longer momentum - existing Zone trigger (rolling decel = Polish)
		{
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B2.Y - A2.Y, B2.X - A2.X));
			const FVector Mid((A2 + B2) * 0.5f, (R.A.Z + R.B.Z) * 0.5f);
			if (AZoogiObstacle* Z = Spawn(TEXT("Ancient_") + R.Name.ToString() + TEXT("_polish"), FTransform(FRotator(0.f, Yaw, 0.f), Mid)))
			{
				FZoogiObsTrigger& T = Z->Triggers.AddDefaulted_GetRef();
				T.Kind = 4; T.Shape = 0; T.C = FVector(0, 0, 60.f); T.E = FVector(Len * 0.5f, R.HalfW, FMath::Abs(R.B.Z - R.A.Z) * 0.5f + 90.f); T.P[0] = R.Polish;
			}
		}
	}
	// ---- hub bridge boost pads (gold chevron pads, one-way minimum speed toward the region; Citadel climb pad 1750)
	for (const FRamp& R : L.Ramps)
	{
		if (!R.Name.ToString().StartsWith(TEXT("bridge_hub_"))) continue;
		const FVector2D D = (FVector2D(R.B) - FVector2D(R.A)).GetSafeNormal();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		const FVector C(D * (L.HubR - 230.f), 0.f);
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_boost_") + R.Name.ToString(), FTransform(FRotator(0.f, Yaw, 0.f), C)))
		{
			FZoogiObsTrigger& T = O->Triggers.AddDefaulted_GetRef();
			T.Kind = 6; T.Shape = 0; T.C = FVector(0, 0, 40.f); T.E = FVector(110.f, 230.f, 60.f); T.D = FVector2D(1.f, 0.f);
			T.P[0] = R.B.Z > 300.f ? 1750.f : (R.B.Z > 100.f ? 1300.f : 1100.f); T.P[1] = 1.f; T.P[2] = 0.6f;
		}
		FZoogiMeshBuilder Pad, Ch;
		const FRotator Rt(0.f, Yaw, 0.f);
		Pad.Box(FVector(C.X, C.Y, 0.8f), FVector(115.f, 235.f, 1.f), Rt);
		for (int32 k = -1; k <= 1; ++k)
		{
			const FVector O = C + Rt.RotateVector(FVector(k * 70.f, 0.f, 2.2f));
			Ch.FlatTri(O + Rt.RotateVector(FVector(45.f, 0.f, 0.f)), O + Rt.RotateVector(FVector(-15.f, 150.f, 0.f)), O + Rt.RotateVector(FVector(15.f, 0.f, 0.f)), FVector::UpVector);
			Ch.FlatTri(O + Rt.RotateVector(FVector(45.f, 0.f, 0.f)), O + Rt.RotateVector(FVector(15.f, 0.f, 0.f)), O + Rt.RotateVector(FVector(-15.f, -150.f, 0.f)), FVector::UpVector);
		}
		Vis(Pad, ZoogiUtil::SurfaceMID(this, Hex(TEXT("#2A2A30")), 0.6f), false); Vis(Ch, SpringM, false);
	}
	// ---- recovery islets: grass octagon + rock underside + the emergency spring (trigger kind 8: ballistic throw onto the target;
	//      shared ZoogiRelaunch guard = no endless loops). Spring look: gold rim, red drum, glowing top.
	for (const FIslet& I : L.Islets)
	{
		TArray<FVector2D> Oct; for (int32 k = 0; k < 8; ++k) { const float A = (k + 0.5f) * PI / 4.f; Oct.Add(FVector2D(I.C) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (I.R / FMath::Cos(PI / 8.f))); }
		TArray<TArray<FVector>> H; PolyHull(H, Oct, I.C.Z, 60.f);
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_") + I.Name.ToString())) O->BuildCollider(H, 1.f, FloorPM);
		FZoogiMeshBuilder Top, Side, U, Rim, Drum, Glow;
		PolyMesh(Top, Oct, I.C.Z, 60.f, true, false); PolyMesh(Side, Oct, I.C.Z, 60.f, false, true);
		U.Prism(FVector2D(I.C), 8, 40.f, I.R * 0.9f, I.C.Z - 1000.f, I.C.Z - 58.f, 22.5f);
		Rim.ArcSlab(I.R - 150.f, I.R - 115.f, 0.f, 360.f, I.C.Z, I.C.Z + 8.f, 32);
		Drum.Prism(FVector2D(I.C), 24, I.R - 160.f, I.R - 160.f, I.C.Z, I.C.Z + 7.f);
		Glow.Prism(FVector2D(I.C), 24, I.R - 230.f, I.R - 230.f, I.C.Z + 7.f, I.C.Z + 8.f);
		Vis(Top, RegM[Recovery]); Vis(Side, RockM); Vis(U, RockM, false); Vis(Rim, GoldM, false); Vis(Drum, bV2 ? ZoogiUtil::SurfaceMID(this, Hex(TEXT("#4A2490")), 0.4f) : BumperTopM, false); Vis(Glow, bV2 ? BlueGlow : SpringM, false); // v2: blue rune springs (no red/yellow targets)
		if (AZoogiObstacle* GAncS = Spawn(TEXT("Ancient_spring_") + I.Name.ToString(), FTransform(I.C)))
		{
			FZoogiObsTrigger& T = GAncS->Triggers.AddDefaulted_GetRef();
			T.Kind = 8; T.Shape = 2; T.C = FVector(0, 0, 50.f); T.E = FVector(I.R - 60.f, 0.f, 70.f); T.P[2] = 1.0f;
			T.Target = I.Target;
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientPedestal"))) // Oct 31: replaced by the lava volcano + crown orb (BuildAncientLook) to match the Battle Royale reference
	// ---- central spire: stone pedestal (collision, 2.5 m) carrying the dual-mode HOLOGRAPHIC SPIRE (ZoogiHoloSpire, it stands on
	//      whatever is at the centre up to 2.5 m: the pedestal top) + six stone buttresses with gold caps, blue channels and banners
	{
		TArray<TArray<FVector>> H; TArray<FVector>& Pts = H.AddDefaulted_GetRef();
		for (int32 k = 0; k < 8; ++k) { const float A = (k + 0.5f) * PI / 4.f; for (float Z : { -10.f, 250.f }) Pts.Add(FVector(FMath::Cos(A) * 470.f, FMath::Sin(A) * 470.f, Z)); }
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_spire"))) O->BuildCollider(H, 1.f, StonePM);
		SolidPosts.Add(FVector2D::ZeroVector);
		FZoogiMeshBuilder Step, Ped, Trim, Butt, Caps, Chan;
		Step.Prism(FVector2D::ZeroVector, 8, 560.f, 540.f, 0.f, 40.f, 22.5f);
		Ped.Prism(FVector2D::ZeroVector, 8, 500.f, 470.f, 40.f, 250.f, 22.5f);
		Trim.Prism(FVector2D::ZeroVector, 8, 482.f, 482.f, 228.f, 252.f, 22.5f);
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = FMath::DegreesToRadians(30.f + 60.f * k); const FVector2D C(FMath::Cos(A) * 420.f, FMath::Sin(A) * 420.f);
			Butt.Prism(C, 4, 75.f, 52.f, 250.f, 760.f, 30.f + 60.f * k + 45.f);
			Caps.Prism(C, 4, 64.f, 20.f, 760.f, 830.f, 30.f + 60.f * k + 45.f, 30.f);
			Chan.Box(FVector(C * 0.9f, 500.f), FVector(6.f, 14.f, 210.f), FRotator(0.f, 30.f + 60.f * k, 0.f));
			Banner(FVector(C * 1.12f, 700.f), 30.f + 60.f * k + 90.f, 120.f, 300.f, k % 2 ? TEXT("#3346B8") : TEXT("#B8333F"));
		}
		Vis(Step, WallM); Vis(Ped, WallM); Vis(Trim, GoldM); Vis(Butt, WallM); Vis(Caps, GoldM); Vis(Chan, BlueGlow, false);
		FZoogiMeshBuilder Ring, ERing;
		Ring.ArcSlab(L.HubR - 70.f, L.HubR - 30.f, 0.f, 360.f, 0.f, 1.5f, 96);
		ERing.ArcSlab(720.f, 760.f, 0.f, 360.f, 0.f, 1.5f, 72);
		Vis(Ring, GoldM, false); Vis(ERing, BlueGlow, false);
	}
	// ---- hub energy bumpers: 8 blue drums with red glowing caps and gold bands at r1350 (bounce gain like the Frozen crystals)
	for (int32 k = 0; k < 8; ++k)
	{
		const float Yaw = 22.5f + 45.f * k; const float A = FMath::DegreesToRadians(Yaw);
		const FVector C(FMath::Cos(A) * 1350.f, FMath::Sin(A) * 1350.f, 0.f);
		if (AZoogiObstacle* O = Spawn(FString::Printf(TEXT("Ancient_bumper_%d"), k), FTransform(C)))
		{
			TArray<TArray<FVector>> H; CylHull(H, FVector::ZeroVector, 80.f, 95.f);
			O->BuildCollider(H, 1.f, BumpPM); O->BounceGain = 1.35f; O->MaxOut = 2600.f;
		}
		SolidPosts.Add(FVector2D(C));
		// Oct 31 (Battle Royale rolling ref): red drum, cream cap, white double chevrons around the body
		FZoogiMeshBuilder Body, Top, Chev;
		Body.Prism(FVector2D(C), 24, 86.f, 84.f, 0.f, 78.f); Top.Prism(FVector2D(C), 24, 82.f, 72.f, 78.f, 98.f);
		for (int32 c = 0; c < 6; ++c)
			for (int32 d = 0; d < 2; ++d)
			{
				const float Ac = A + FMath::DegreesToRadians(60.f * c + (d ? 7.f : -7.f));
				const FVector Rd(FMath::Cos(Ac), FMath::Sin(Ac), 0.f), Tg(-Rd.Y, Rd.X, 0.f);
				const FVector Pc = C + Rd * 86.6f;
				for (int32 h = -1; h <= 1; h += 2) // a '<' made of two slanted bars
				{
					const FVector P0 = Pc + Tg * 6.f + FVector(0, 0, 39.f), P1 = Pc - Tg * 9.f + FVector(0, 0, 39.f + h * 22.f);
					Chev.Quad(P0 + FVector(0, 0, -4.f), P1 + FVector(0, 0, -4.f), P1 + FVector(0, 0, 4.f), P0 + FVector(0, 0, 4.f), Rd);
				}
			}
		const bool bYel = bV2 && (k % 2 == 1); // Nov 1 v2 (ref): alternate red/white and yellow/black drums
		Vis(Body, ZoogiUtil::SurfaceMID(this, Hex(bYel ? TEXT("#F2C230") : TEXT("#D9342B")), 0.4f)); Vis(Top, ZoogiUtil::SurfaceMID(this, Hex(bYel ? TEXT("#1C1A22") : TEXT("#F1E4C6")), 0.55f));
		Vis(Chev, bYel ? ZoogiUtil::SurfaceMID(this, Hex(TEXT("#1C1A22")), 0.5f) : ZoogiUtil::GlowMID(this, FLinearColor::White, 1.2f), false);
		(void)BumperM; (void)BumperTopM;
	}
	// ---- Emerald edge rebounders (Recovery type A): angled red / gold spring blocks on the outer edges that bat a Zoogi back in
	{
		struct FReb { FVector2D C; float Yaw; };
		const FReb Rb[] = { { { -4640.f, 3000.f }, 70.f }, { { -4640.f, 3900.f }, 110.f }, { { -3000.f, 4440.f }, -20.f }, { { -3900.f, 4440.f }, 20.f } };
		int32 n = 0;
		for (const FReb& R : Rb)
		{
			const FVector C(R.C, 0.f);
			if (AZoogiObstacle* O = Spawn(FString::Printf(TEXT("Ancient_rebounder_%d"), n++), FTransform(C)))
			{
				TArray<TArray<FVector>> H; BoxHullW(H, FVector(0, 0, 35.f), FVector(170.f, 22.f, 35.f), FRotator(0.f, R.Yaw, 0.f));
				O->BuildCollider(H, 1.f, BumpPM); O->BounceGain = 1.3f; O->MaxOut = 1800.f;
			}
			FZoogiMeshBuilder B, G;
			B.Box(C + FVector(0, 0, 35.f), FVector(170.f, 22.f, 35.f), FRotator(0.f, R.Yaw, 0.f));
			G.Box(C + FVector(0, 0, 72.f), FVector(172.f, 24.f, 4.f), FRotator(0.f, R.Yaw, 0.f));
			Vis(B, BumperTopM); Vis(G, GoldM);
		}
	}
	// ---- scoring portals: stone arch (pillars collide), glowing gate, gold crown on the lintel, banners, steps
	for (const FPortal& P : L.Portals)
	{
		const FRotator R(0.f, P.Yaw, 0.f);
		const bool bBlue = P.C.X > 0.f && P.C.Y < 0.f;
		TArray<TArray<FVector>> H;
		for (int32 s = -1; s <= 1; s += 2) BoxHullW(H, P.C + R.RotateVector(FVector(s * PortalPillarX, 0.f, 225.f)), FVector(50.f, 55.f, 225.f), R);
		if (AZoogiObstacle* O = Spawn(TEXT("Ancient_") + P.Name.ToString(), FTransform::Identity)) O->BuildCollider(H, 1.f, StonePM);
		FZoogiMeshBuilder St, Gd, Gl, Cr;
		for (int32 s = -1; s <= 1; s += 2)
		{
			St.Box(P.C + R.RotateVector(FVector(s * PortalPillarX, 0.f, 225.f)), FVector(50.f, 55.f, 225.f), R);
			Gd.Box(P.C + R.RotateVector(FVector(s * PortalPillarX, 0.f, 455.f)), FVector(58.f, 62.f, 10.f), R);
			Gd.Box(P.C + R.RotateVector(FVector(s * PortalPillarX, 0.f, 12.f)), FVector(60.f, 64.f, 12.f), R);
			Banner(P.C + R.RotateVector(FVector(s * (PortalPillarX + 95.f), 0.f, 430.f)), P.Yaw + 90.f, 90.f, 260.f, bBlue ? TEXT("#3346B8") : TEXT("#B8333F"));
		}
		St.Box(P.C + FVector(0, 0, 500.f), FVector(305.f, 62.f, 40.f), R);
		Gd.Box(P.C + FVector(0, 0, 545.f), FVector(310.f, 66.f, 6.f), R);
		Gl.Box(P.C + FVector(0, 0, 230.f), FVector(PortalHalfW + 4.f, 6.f, 228.f), R);
		{ // gold crown emblem on the lintel (both faces)
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector N = R.RotateVector(FVector(0.f, s * 64.f, 0.f)), X = R.RotateVector(FVector(1.f, 0.f, 0.f)), Mc = P.C + FVector(0, 0, 500.f) + N;
				Cr.Quad(Mc - X * 70.f - FVector(0, 0, 22.f), Mc + X * 70.f - FVector(0, 0, 22.f), Mc + X * 70.f + FVector(0, 0, 4.f), Mc - X * 70.f + FVector(0, 0, 4.f), N);
				for (int32 p = -1; p <= 1; ++p) { const FVector B = Mc + X * (p * 50.f) + FVector(0, 0, 4.f); Cr.FlatTri(B - X * 22.f, B + X * 22.f, B + FVector(0, 0, p == 0 ? 48.f : 34.f), N); }
			}
		}
		Vis(St, bV2 ? StoneV2M : (bBlue ? WallBlueM : WallM)); Vis(Gd, GoldM); Vis(Gl, ZoogiUtil::GlowMID(this, Hex(P.Glow), 1.25f), false); Vis(Cr, GoldM, false);
	}
	// ---- set pieces: ruin columns, Citadel battlements, breakable ruins (AZoogiRock: crack, then shatter; reset every match), palms, trees
	{
		TArray<TArray<FVector>> PostH, WallH;
		FZoogiMeshBuilder ColM, ColCap, WallV, WallVB, Merl, Trunk;
		int32 Seed = 3100;
		for (const FPiece& Pc : L.Pieces)
		{
			const FVector2D C(Pc.A);
			switch (Pc.Kind)
			{
			case 0: // fluted ruin column with base, broken top on the short ones
				CylHull(PostH, Pc.A, Pc.R, Pc.H); SolidPosts.Add(C);
				ColM.Prism(C, 10, Pc.R + 18.f, Pc.R + 14.f, Pc.A.Z, Pc.A.Z + 30.f, 0.f);
				ColM.Prism(C, 10, Pc.R, Pc.R * 0.9f, Pc.A.Z + 30.f, Pc.A.Z + Pc.H - (Pc.H < 330.f ? 0.f : 30.f), 9.f);
				if (Pc.H >= 330.f) ColCap.Prism(C, 4, Pc.R + 24.f, Pc.R + 24.f, Pc.A.Z + Pc.H - 30.f, Pc.A.Z + Pc.H, 45.f);
				else ColM.Prism(C, 5, Pc.R * 0.9f, Pc.R * 0.45f, Pc.A.Z + Pc.H, Pc.A.Z + Pc.H + 40.f, 17.f, 0.f);
				break;
			case 1: case 2: // palm / round tree: trunk post collides, canopy is visual
			{
				CylHull(PostH, Pc.A, Pc.R, 260.f); SolidPosts.Add(C);
				if (bV2) {} // v2: palms drawn by BuildAncientMeadow (trunk collision stays)
				else if (Pc.Kind == 1)
				{
					for (int32 s = 0; s < 5; ++s) { const float z0 = Pc.A.Z + s * Pc.H / 5.f; Trunk.Prism(C + FVector2D(s * 9.f, s * 4.f), 7, Pc.R * (1.f - 0.08f * s), Pc.R * (0.9f - 0.08f * s), z0, z0 + Pc.H / 5.f + 4.f, s * 13.f); }
					const FVector Crown(C.X + 45.f, C.Y + 20.f, Pc.A.Z + Pc.H);
					for (int32 f = 0; f < 7; ++f)
					{
						const float Yw = f * 360.f / 7.f + 10.f;
						Mesh(ConeM, f % 2 ? LeafM : LeafM2, Crown + FRotator(0.f, Yw, 0.f).RotateVector(FVector(110.f, 0.f, -20.f)), FVector(0.55f, 2.6f, 0.12f), FRotator(-18.f, Yw, 90.f), true);
					}
					Mesh(SphereM, WoodM, Crown, FVector(0.6f), FRotator::ZeroRotator, false);
				}
				else
				{
					Trunk.Prism(C, 8, Pc.R * 1.3f, Pc.R, Pc.A.Z, Pc.A.Z + Pc.H * 0.55f, 0.f);
					Mesh(SphereM, LeafM, FVector(C, Pc.A.Z + Pc.H * 0.72f), FVector(Pc.H / 160.f, Pc.H / 160.f, Pc.H / 210.f));
					Mesh(SphereM, LeafM2, FVector(C + FVector2D(60.f, -40.f), Pc.A.Z + Pc.H * 0.86f), FVector(Pc.H / 230.f));
				}
				break;
			}
			case 3: // battlement run A->B: collision wall + masonry + merlons
			{
				const FVector2D B2(Pc.B), D = (B2 - C).GetSafeNormal(); const float Len = FVector2D::Distance(C, B2);
				const float Yw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
				const FVector Mid((C + B2) * 0.5f, Pc.A.Z + Pc.H * 0.5f);
				BoxHullW(WallH, Mid, FVector(Len * 0.5f, Pc.R, Pc.H * 0.5f), FRotator(0.f, Yw, 0.f));
				WallVB.Box(Mid, FVector(Len * 0.5f, Pc.R, Pc.H * 0.5f), FRotator(0.f, Yw, 0.f));
				for (float t = 40.f; t < Len - 30.f; t += 110.f) Merl.Box(FVector(C + D * t, Pc.A.Z + Pc.H + 25.f), FVector(32.f, Pc.R + 4.f, 25.f), FRotator(0.f, Yw, 0.f));
				if (!bV2) Banner(FVector(C + D * (Len * 0.5f) - FVector2D(-D.Y, D.X) * (Pc.R + 3.f) * (Pc.A.X > 4000.f ? -1.f : 1.f), Pc.A.Z + Pc.H + 10.f), Yw + 90.f, 110.f, 200.f, TEXT("#3346B8"));
				break;
			}
			case 4: // breakable ancient ruin block (existing destruction framework)
			{
				AZoogiRock* Rk = GetWorld()->SpawnActor<AZoogiRock>(AZoogiRock::StaticClass(), FTransform(Pc.A), SP);
				if (!Rk) break;
				Rk->SetCarvedStyle(0.55f, Hex(TEXT("#F2C230")), 0.9f);
				const bool bNorth = Pc.Region == North;
				Rk->InitRock(Pc.R, Pc.H, (float)(Seed % 90), bNorth ? Hex(TEXT("#C46A4E")) : Hex(TEXT("#D9A56A")), bNorth ? Hex(TEXT("#9E4E36")) : Hex(TEXT("#B8844E")), RockPM, Seed);
				++Seed;
				Rocks.Add(Rk);
				break;
			}
			}
		}
		if (PostH.Num()) if (AZoogiObstacle* O = Spawn(TEXT("Ancient_posts"))) { O->BuildCollider(PostH, 1.f, StonePM); O->BounceGain = 1.15f; O->MaxOut = 1800.f; O->CenterBias = 0.65f; } // b46: columns / trunks bounce players back in
		if (WallH.Num()) if (AZoogiObstacle* O = Spawn(TEXT("Ancient_battlements"))) O->BuildCollider(WallH, 1.f, CurbPM);
		Vis(ColM, StoneV2M); Vis(ColCap, GoldM); if (!bV2) { Vis(WallVB, WallM); Vis(Merl, WallM); } Vis(Trunk, WoodM); // v2: balustrades (BuildAncientMeadow) // Oct 31: NE = the red fortress now
	}
	// ---- waterfalls pouring off the island sides into the clouds (existing kit waterfall)
	{
		struct FFall { FVector Top; float Yaw, W, H; };
		const FFall Fs[] = { { { -4712.f, 3300.f, -15.f }, 180.f, 520.f, 2300.f }, { { -3400.f, 4512.f, -15.f }, 90.f, 420.f, 2300.f },
			{ { -4712.f, -2950.f, 135.f }, 180.f, 380.f, 2400.f }, { { 4712.f, -3300.f, 435.f }, 0.f, 520.f, 2600.f },
			{ { 3400.f, -4512.f, 435.f }, -90.f, 380.f, 2600.f }, { { 3400.f, 4512.f, -15.f }, 90.f, 360.f, 2300.f } };
		if (!bV2) for (const FFall& F : Fs) AddWaterfall(F.Top, F.Yaw, F.W, F.H, 70.f);
	}
	// ---- orbs on the islands (the hub ring comes from the game mode). Skip any spot a ramp / terrace covers higher.
	for (const FVector& GAncS : L.OrbSpots)
	{
		const float Floor = GAncS.Z - 41.f;
		if (FMath::Abs(FloorZ(FVector2D(GAncS)) - Floor) > 5.f) continue;
		ExtraOrbSpots.Add(GAncS);
	}
	// Oct 31: painted 360 sky (Grok art, T_AncientSky via make_ancient_kit.py): blue sky, cloud sea, distant temple islands + waterfalls
	if (!bV2) UE_LOG(LogZoogi, Log, TEXT("ANCIENT: panorama sky %s"), AddPanoramaSky(TEXT("Ancient"), 1.0f, 0.f, FLinearColor::White) ? TEXT("T_AncientSky") : TEXT("missing (atmosphere sky)"));
	// ---- sky: cloud sea + puffy cloud banks under the islands, distant ruin islets, festival blimps (animated in TickAncient)
	if (!bV2) // v2: meadow valley, mesas and the painted sunset instead
	{
		FZoogiMeshBuilder Sea;
		const float Z = -2600.f, R = 60000.f;
		for (int32 i = 0; i < 48; ++i)
		{
			const float A0 = 2.f * PI * i / 48, A1 = 2.f * PI * (i + 1) / 48;
			Sea.FlatTri(FVector(0, 0, Z), FVector(FMath::Cos(A0) * R, FMath::Sin(A0) * R, Z), FVector(FMath::Cos(A1) * R, FMath::Sin(A1) * R, Z), FVector::UpVector);
		}
		(void)Sea; // Oct 31: the ruin valley replaces the cloud sea (BuildAncientLook)
		FRandomStream CR(1031);
		for (int32 i = 0; i < 26; ++i) // Oct 31: only far cloud banks (the valley is below the islands)
		{
			const float A = CR.FRandRange(0.f, 2.f * PI), D = CR.FRandRange(11000.f, 16000.f);
			const float GAncS = CR.FRandRange(10.f, 26.f) * (D > 7000.f ? 1.6f : 1.f);
			Mesh(SphereM, CloudM, FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, CR.FRandRange(-2400.f, -1500.f)), FVector(GAncS, GAncS * CR.FRandRange(0.7f, 1.1f), GAncS * 0.38f), FRotator(0.f, CR.FRandRange(0.f, 180.f), 0.f), false);
		}
		FRandomStream IR(731);
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = 2.f * PI * i / 14 + IR.FRandRange(-0.15f, 0.15f), D = IR.FRandRange(9500.f, 15000.f);
			const FVector C(FMath::Cos(A) * D, FMath::Sin(A) * D, IR.FRandRange(-900.f, 900.f));
			const float Rad = IR.FRandRange(500.f, 1100.f);
			BuildIslet(C, Rad, Hex(TEXT("#66BE3E")), Hex(TEXT("#4E9E2C")), Hex(TEXT("#7A6A56")), nullptr, 0, 0.f, IR);
			FZoogiMeshBuilder Tw;
			const int32 NT = 1 + IR.RandRange(0, 2);
			for (int32 t = 0; t < NT; ++t) { const FVector2D Q = FVector2D(C) + FVector2D(IR.FRandRange(-0.4f, 0.4f), IR.FRandRange(-0.4f, 0.4f)) * Rad; const float Ht = IR.FRandRange(500.f, 1400.f); Tw.Prism(Q, 6, IR.FRandRange(90.f, 160.f), IR.FRandRange(70.f, 130.f), C.Z, C.Z + Ht, IR.FRandRange(0.f, 60.f)); }
			Vis(Tw, i % 3 == 0 ? WallBlueM : WallM, false);
		}
		const TCHAR* BodyC[] = { TEXT("#3D63D6"), TEXT("#D6453D"), TEXT("#7A4FD6") };
		for (int32 b = 0; b < 3; ++b)
		{
			USceneComponent* Pv = NewPivot(FVector::ZeroVector);
			UMaterialInterface* BodyM = ZoogiUtil::SurfaceMID(this, Hex(BodyC[b]), 0.45f);
			Mesh(SphereM, BodyM, FVector::ZeroVector, FVector(9.f, 3.4f, 3.4f), FRotator::ZeroRotator, false, Pv);
			Mesh(CylM, GoldM, FVector::ZeroVector, FVector(3.48f, 3.48f, 0.55f), FRotator(90.f, 0.f, 0.f), false, Pv);
			Mesh(SphereM, ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F4F0E6")), 0.5f), FVector(160.f, 0.f, 0.f), FVector(1.2f, 3.43f, 3.43f), FRotator::ZeroRotator, false, Pv);
			for (int32 f = 0; f < 4; ++f) Mesh(CubeM, BodyM, FRotator(f * 90.f, 0.f, 0.f).RotateVector(FVector(-380.f, 0.f, 150.f)), FVector(1.4f, 0.12f, 1.2f), FRotator(f * 90.f, 0.f, 0.f), false, Pv);
			Mesh(CubeM, WoodM, FVector(0.f, 0.f, -205.f), FVector(2.4f, 0.9f, 0.6f), FRotator::ZeroRotator, false, Pv);
			Mesh(SphereM, GoldM, FVector(0.f, 170.f, 30.f), FVector(1.1f, 0.05f, 1.1f), FRotator::ZeroRotator, false, Pv);
			Mesh(SphereM, GoldM, FVector(0.f, -170.f, 30.f), FVector(1.1f, 0.05f, 1.1f), FRotator::ZeroRotator, false, Pv);
			AncMovers.Add(Pv);
			AncMoverPath.Add(FVector4(8600.f + b * 1700.f, 2300.f + b * 650.f, (b % 2 ? -1.f : 1.f) * (2.2f - b * 0.4f), b * 120.f));
		}
		TickAncient(0.f);
	}
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT: built - %d floors, %d bridges/ramps, %d recovery islets, %d portals, %d set pieces, %d breakables, %d island orbs, %d obstacles"),
		L.Plats.Num(), L.Ramps.Num(), L.Islets.Num(), L.Portals.Num(), L.Pieces.Num(), Rocks.Num(), ExtraOrbSpots.Num(), Obstacles.Num());
	BuildAncientLook(); // Oct 31: Battle Royale reference look (volcano, fortresses, rope bridges, ruin valley, star pads, crates)
	ZoogiAncientStartTest(this);
}

#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", on)
#endif

void AZoogiArena::TickAncient(float Dt)
{
	if (Theme != EZoogiArena::Ancient || AncMovers.Num() == 0) return;
	AncClock += Dt;
	for (int32 i = 0; i < AncMovers.Num() && i < AncMoverPath.Num(); ++i)
	{
		USceneComponent* P = AncMovers[i].Get();
		if (!IsValid(P)) continue;
		const FVector4& Pa = AncMoverPath[i];
		const float A = FMath::DegreesToRadians(Pa.W + Pa.Z * AncClock);
		const FVector Loc(FMath::Cos(A) * Pa.X, FMath::Sin(A) * Pa.X, Pa.Y + 120.f * FMath::Sin(AncClock * 0.35f + i));
		const float Yaw = FMath::RadiansToDegrees(A) + (Pa.Z >= 0.f ? 90.f : -90.f);
		P->SetRelativeLocationAndRotation(Loc, FRotator(1.5f * FMath::Sin(AncClock * 0.5f + i), Yaw, 0.f));
	}
}

bool AZoogiArena::IsPortalGoal(const FVector& L)
{
	return bAncientActive && ZoogiAncient::InPortal(L);
}

// ------------------------------------------------------------------ -ZoogiAncientTest (traversal validation)
namespace ZoogiAncientTest
{
	struct FRoute { const TCHAR* Name; FVector From; FVector2D Vel; const TCHAR* Expect; float Wait; const TCHAR* Alt; bool bOrb = false; };
	// From = ground point (Z = floor top); the ball is placed one radius above it.
	const FRoute Routes[] = {
		{ TEXT("hub_to_emerald"),        { -950.f,  950.f,   0.f }, { -1450.f,  1450.f }, TEXT("emerald_meadows"), 6.f, nullptr },
		{ TEXT("hub_to_sunstone"),       {  950.f,  950.f,   0.f }, {  1450.f,  1450.f }, TEXT("sunstone_ruins"), 6.f, nullptr },
		{ TEXT("hub_to_north_+150"),     { -950.f, -950.f,   0.f }, { -1300.f, -1300.f }, TEXT("north_ruins"), 6.f, nullptr },
		{ TEXT("hub_to_citadel_+450"),   {  950.f, -950.f,   0.f }, {  1500.f, -1500.f }, TEXT("sky_citadel"), 7.f, nullptr },
		{ TEXT("citadel_momentum_down"), { 3400.f,-2700.f, 450.f }, {     0.f,  1100.f }, TEXT("sunstone_ruins"), 12.f, nullptr },
		{ TEXT("emerald_to_sunstone"),   {-2700.f, 3300.f,   0.f }, {  2000.f,     0.f }, TEXT("sunstone_ruins"), 8.f, nullptr },
		{ TEXT("north_to_emerald"),      {-3400.f,-2700.f, 150.f }, {     0.f,  1850.f }, TEXT("emerald_meadows"), 8.f, nullptr },
		{ TEXT("north_up_terrace_+450"), {-4200.f,-2200.f, 150.f }, {     0.f, -1500.f }, TEXT("north_terrace"), 6.f, nullptr },
		{ TEXT("high_bridge_to_citadel"),{-4300.f,-4000.f, 450.f }, {  3100.f,     0.f }, TEXT("sky_citadel"), 9.f, nullptr },
		{ TEXT("recover_hub_north"),     {    0.f,-1650.f,   0.f }, {     0.f,  -800.f }, TEXT("hub"), 7.f, nullptr },
		{ TEXT("recover_emerald_west"),  {-3600.f, 3450.f,   0.f }, { -1450.f,     0.f }, TEXT("emerald_meadows"), 8.f, nullptr },
		{ TEXT("recover_sunstone_east"), { 3900.f, 3250.f,   0.f }, {  1300.f,     0.f }, TEXT("sunstone_ruins"), 8.f, nullptr },
		{ TEXT("curb_holds_link_east"),  { 3400.f,  100.f, 214.f }, {   900.f,     0.f }, TEXT("link_east_momentum"), 6.f, TEXT("sunstone_ruins") },
		{ TEXT("bumper_rebound"),        {  550.f,  517.f,   0.f }, {  1100.f,     0.f }, TEXT("hub"), 5.f, nullptr },
		{ TEXT("orb_portal_emerald"),    {-3700.f, 3700.f,   0.f }, {  -950.f,   950.f }, TEXT("PORTAL"), 5.f, nullptr, true },
		{ TEXT("orb_portal_north_terrace"),{-3850.f,-3850.f, 450.f }, {  -950.f,  -950.f }, TEXT("PORTAL"), 5.f, nullptr, true },
		{ TEXT("orb_portal_citadel"),    { 3700.f,-3700.f, 450.f }, {   950.f,  -950.f }, TEXT("PORTAL"), 5.f, nullptr, true },
		{ TEXT("orb_portal_sunstone"),   { 3615.f, 3785.f,   0.f }, {   950.f,   950.f }, TEXT("PORTAL"), 5.f, nullptr, true },
		{ TEXT("fallout_control_OUT"),   {    0.f,-1500.f,   0.f }, {     0.f, -2600.f }, TEXT("OUT"), 4.f, nullptr },
	};
	struct FState
	{
		TWeakObjectPtr<AZoogiArena> Arena;
		TWeakObjectPtr<AZoogiBall> Ball;
		TWeakObjectPtr<AZoogiOrb> Orb;
		bool bSawPortal = false;
		TWeakObjectPtr<ACameraActor> Cam;
		FString Dir;
		int32 Step = -2;
		double T0 = 0.0, StepT = 0.0;
		float MaxZ = -1e6f, MinZ = 1e6f;
		int32 Shots = 0;
		TArray<FString> Results;
		FTSTicker::FDelegateHandle H;
	};
	FState GAncS;
	bool Tick(float Dt);
}

void ZoogiAncientStartTest(AZoogiArena* Arena)
{
	using namespace ZoogiAncientTest;
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientTest")) || GAncS.H.IsValid()) return;
	GAncS.Arena = Arena; GAncS.Step = -2;
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), GAncS.Dir);
	GAncS.H = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&ZoogiAncientTest::Tick), 0.f);
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT test armed: %d routes"), (int32)UE_ARRAY_COUNT(Routes));
}

bool ZoogiAncientTest::Tick(float Dt)
{
	AZoogiArena* A = GAncS.Arena.Get();
	UWorld* W = A ? A->GetWorld() : nullptr;
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || !A) return true;
	const double Now = FPlatformTime::Seconds();
	APlayerController* PC = W->GetFirstPlayerController();
	if (GAncS.Step == -2)
	{
		if (A->IsForming() || G->Balls.Num() == 0 || G->Phase != EZoogiPhase::Aiming) { GAncS.T0 = Now; return true; }
		if (Now - GAncS.T0 < 3.0) return true;
		AZoogiObstacle::bOrbTestHold = true; // hold the turn flow (no CPU shots) while the routes run
		GAncS.Ball = G->Balls[0];
		GAncS.Step = 0; GAncS.StepT = -1.0;
		UE_LOG(LogZoogi, Log, TEXT("ANCIENT test: start (ball P1)"));
	}
	if (GAncS.Step >= (int32)UE_ARRAY_COUNT(Routes)) return true;
	AZoogiBall* B = GAncS.Ball.Get();
	if (!B) { UE_LOG(LogZoogi, Warning, TEXT("ANCIENT test: no ball")); GAncS.Step = 999; AZoogiObstacle::bOrbTestHold = false; return true; }
	const FRoute& R = Routes[GAncS.Step];
	if (R.bOrb) // orb through a portal: the knock-off rules run each frame (the turn flow is held), PASS = the orb scored via the portal
	{
		if (GAncS.StepT < 0.0)
		{
			GAncS.Orb = nullptr;
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && !O->bStar) { GAncS.Orb = O; break; }
			if (AZoogiOrb* O = GAncS.Orb.Get()) { O->LastHitBy = 0; O->Respawn(R.From + FVector(0, 0, ZoogiConst::OrbRadius + 2.f)); O->Sphere->SetPhysicsLinearVelocity(FVector(R.Vel, 0.f)); }
			GAncS.StepT = Now; GAncS.bSawPortal = false; GAncS.Shots = 0;
			UE_LOG(LogZoogi, Log, TEXT("ANCIENT route %s: orb launch %.0f cm/s from (%.0f,%.0f,%.0f)"), R.Name, R.Vel.Size(), R.From.X, R.From.Y, R.From.Z);
			return true;
		}
		const double T = Now - GAncS.StepT;
		AZoogiOrb* O = GAncS.Orb.Get();
		const FVector P = O ? O->OrbLocation() : FVector::ZeroVector;
		if (O && !O->bOut && ZoogiAncient::InPortal(P)) GAncS.bSawPortal = true;
		if (O && !O->bOut) G->TestCheckKnockoffs();
		if (ACameraActor* C = GAncS.Cam.Get())
		{
			const FVector D = FVector(R.Vel.GetSafeNormal(), 0.f);
			const FVector Loc = R.From - D * 500.f + FVector(0, 0, 260.f);
			C->SetActorLocation(Loc); C->SetActorRotation((R.From + D * 700.f + FVector(0, 0, 120.f) - Loc).Rotation());
			if (PC) PC->SetViewTarget(C);
		}
		if (!GAncS.Dir.IsEmpty() && GAncS.Shots < 2 && T > 0.4 + GAncS.Shots * 0.5) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(GAncS.Dir, FString::Printf(TEXT("ancient_%02d_%s_%d.png"), GAncS.Step, R.Name, GAncS.Shots)), false, false); ++GAncS.Shots; }
		if (!O || O->bOut || T > R.Wait)
		{
			const bool bPass = O && O->bOut && GAncS.bSawPortal;
			const FString Res = FString::Printf(TEXT("%s: orb %s at (%.0f,%.0f,%.0f), in portal %d, %.1fs %s"), R.Name, O && O->bOut ? TEXT("SCORED") : TEXT("not scored"), P.X, P.Y, P.Z, GAncS.bSawPortal ? 1 : 0, T, bPass ? TEXT("PASS") : TEXT("FAIL"));
			GAncS.Results.Add(Res); UE_LOG(LogZoogi, Log, TEXT("ANCIENT route %s"), *Res);
			++GAncS.Step; GAncS.StepT = -1.0;
		}
		return true;
	}
	if (GAncS.StepT < 0.0)
	{
		B->bOut = false; B->bKnockedOut = false;
		B->PlaceAt(R.From + FVector(0, 0, ZoogiConst::ZoogiRadius + 2.f), FMath::RadiansToDegrees(FMath::Atan2(R.Vel.Y, R.Vel.X)));
		B->Launch(R.Vel);
		GAncS.StepT = Now; GAncS.MaxZ = -1e6f; GAncS.MinZ = 1e6f; GAncS.Shots = 0;
		if (!GAncS.Cam.IsValid()) GAncS.Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		UE_LOG(LogZoogi, Log, TEXT("ANCIENT route %s: launch %.0f cm/s from (%.0f,%.0f,%.0f)"), R.Name, R.Vel.Size(), R.From.X, R.From.Y, R.From.Z);
		return true;
	}
	const double T = Now - GAncS.StepT;
	const FVector P = B->BallLocation();
	GAncS.MaxZ = FMath::Max(GAncS.MaxZ, (float)P.Z); GAncS.MinZ = FMath::Min(GAncS.MinZ, (float)P.Z);
	// chase camera: low behind the ball along its motion (reference image 2 framing)
	if (ACameraActor* C = GAncS.Cam.Get())
	{
		FVector V = B->GetBallVelocity(); V.Z = 0.f;
		static FVector2D LastDir(1, 0);
		if (V.Size() > 120.f) LastDir = FMath::Lerp(LastDir, FVector2D(V).GetSafeNormal(), 0.15f).GetSafeNormal();
		const FVector D(LastDir, 0.f);
		const FVector Loc = P - D * 620.f + FVector(0, 0, 210.f);
		C->SetActorLocation(Loc); C->SetActorRotation((P + D * 700.f + FVector(0, 0, 40.f) - Loc).Rotation());
		C->GetCameraComponent()->SetFieldOfView(80.f);
		if (PC) PC->SetViewTarget(C);
	}
	if (!GAncS.Dir.IsEmpty() && GAncS.Shots < 3 && T > 0.6 + GAncS.Shots * 1.4)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(GAncS.Dir, FString::Printf(TEXT("ancient_%02d_%s_%d.png"), GAncS.Step, R.Name, GAncS.Shots)), false, false);
		++GAncS.Shots;
	}
	const bool bRest = B->GetBallVelocity().Size() < 25.f && T > 1.5;
	if (T > R.Wait || (bRest && T > 3.0) || P.Z < -900.f)
	{
		const FString Where = ZoogiAncient::Classify(P, ZoogiConst::ZoogiRadius);
		const bool bPass = Where == R.Expect || (R.Alt && Where == R.Alt);
		const FString Res = FString::Printf(TEXT("%s: ends (%.0f,%.0f,%.0f) on %s (expect %s) zmax %.0f zmin %.0f %.1fs %s"), R.Name, P.X, P.Y, P.Z, *Where, R.Expect, GAncS.MaxZ, GAncS.MinZ, T, bPass ? TEXT("PASS") : TEXT("FAIL"));
		GAncS.Results.Add(Res);
		UE_LOG(LogZoogi, Log, TEXT("ANCIENT route %s"), *Res);
		++GAncS.Step; GAncS.StepT = -1.0;
		if (GAncS.Step >= (int32)UE_ARRAY_COUNT(Routes))
		{
			int32 NP = 0; for (const FString& X : GAncS.Results) NP += X.EndsWith(TEXT("PASS")) ? 1 : 0;
			UE_LOG(LogZoogi, Log, TEXT("ANCIENT SUMMARY: %d/%d PASS | %s"), NP, GAncS.Results.Num(), *FString::Join(GAncS.Results, TEXT(" | ")));
			AZoogiObstacle::bOrbTestHold = false;
			B->bOut = false; B->PlaceAt(B->SpawnLocation, 0.f);
			if (PC) if (APawn* Pw = PC->GetPawn()) PC->SetViewTarget(Pw);
		}
	}
	return true;
}
