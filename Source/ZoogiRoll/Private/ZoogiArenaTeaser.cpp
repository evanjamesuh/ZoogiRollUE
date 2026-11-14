// Teaser Arena (Oct 23, Arena Maps): the Zoogi Roll Teaser star-bumper plateau, built 1:1 from
// reskin-incoming/08-teaser-original (LAYOUT.md / PLACEMENT.json / DIFFS.md). Pack court A 1.3 (knockout r2015).
// Layout yaw (theta): 0 = far rock side, 180 = elevator / camera side, + = camera-right. The showcase / match55
// camera sits on +Y looking -Y, so theta maps to world angle 270 + theta. All numbers below are WORLD cm (as in
// PLACEMENT.json) and are divided by the actor scale when placed. Physics unchanged: the four pads are the existing
// AddBumper (invisible collider + visual), everything else is visual only (no collision, NoCamBlock).
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiMeshKit.h"
#include "ZoogiTeaserRef.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"

namespace ZoogiTeaser
{
	constexpr float TA = 1.3f;
	// world (r, theta) -> builder-local (unscaled) position
	FVector Pol(float RWorld, float Theta, float ZWorld)
	{
		const float A = FMath::DegreesToRadians(270.f + Theta);
		return FVector(FMath::Cos(A) * RWorld, FMath::Sin(A) * RWorld, ZWorld) / TA;
	}
	float WorldYaw(float Theta) { return 270.f + Theta; }

	// flat 5-point star (local XY, tip on +X), extruded Z0..Z1
	void Star(FZoogiMeshBuilder& M, float ROut, float RIn, float Z0, float Z1)
	{
		TArray<FVector2D> P;
		for (int32 i = 0; i < 10; ++i)
		{
			const float A = FMath::DegreesToRadians(36.f * i);
			const float R = (i % 2 == 0) ? ROut : RIn;
			P.Add(FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R));
		}
		const FVector C(0, 0, Z1);
		for (int32 i = 0; i < 10; ++i)
		{
			const FVector2D& A = P[i]; const FVector2D& B = P[(i + 1) % 10];
			M.FlatTri(C, FVector(A, Z1), FVector(B, Z1), FVector::UpVector);
			const FVector2D Mid = (A + B) * 0.5f;
			M.Quad(FVector(A, Z0), FVector(B, Z0), FVector(B, Z1), FVector(A, Z1), FVector(Mid.X, Mid.Y, 0.f).GetSafeNormal());
		}
	}
}

void AZoogiArena::BuildTeaser()
{
	using namespace ZoogiTeaser;
	using ZoogiUtil::Hex;
	BuildScale = TA; SetActorScale3D(FVector(TA)); CamScale = 1.15f;
	KnockoffRadiusOverride = ZoogiConst::KnockoffRadius * TA; ActiveKnockoffR = KnockoffRadiusOverride; // r2015
	SpawnRadiusOverride = ZoogiConst::SpawnRadius * TA;
	// Orbs (world cm): 4 on r900 at the diagonals (theta 45/135/225/315 = world 315/45/135/225) + 1 centre orb.
	OrbCountOverride = 4; OrbRingRadiusOverride = 900.f; OrbRingYawOffset = 45.f;
	ExtraOrbSpots.Add(FVector(0.f, 0.f, ZoogiConst::OrbRadius + 1.f));

	// Palette (PLACEMENT.json existing_game_objects.palette)
	const FLinearColor Grass = Hex(TEXT("#5EAC00")), GrassDark = Hex(TEXT("#4C9400")); // v4: teaser lighter yellow-green (f_017/f_018 #5EAC00); 0.1.2 Ground-grass01 avg #448001
	const FLinearColor Soil = Hex(TEXT("#B1946C")), SoilDark = Hex(TEXT("#897463"));
	const FLinearColor Under = Hex(TEXT("#3A2A1A")), UnderDark = Hex(TEXT("#0B0602"));
	const FLinearColor Rock = Hex(TEXT("#7D7262")), RockDark = Hex(TEXT("#544A3F")); // v3: darker grey-brown (teaser f_017/f_024 rock #7B6B51 lit / #534131 shade), 0.1.2 Rock01 #5F513E
	const FLinearColor PadRed = Hex(TEXT("#E8241C")), PadSide = Hex(TEXT("#D13F38")); // 0.1.2 Bumper_Texture avg #D13F38
	const FLinearColor StarY = Hex(TEXT("#F5F920")), TreeG = Hex(TEXT("#198C13")), Mush = Hex(TEXT("#CD2662"));

	// Plateau: round collision floor r2015 (BuildIsland, physics unchanged) under a VISUAL 20-sided grass top + beige soil
	// band (Phase 3). 20 sides = the Unity built-in cylinder 0.1.2 uses for "Island Top" (scale 45 -> r2015); the polygon is
	// circumscribed (apothem = r2015) so the round floor never shows past a facet. Palette: 0.1.2 texture averages.
	BuildIsland(Grass, GrassDark, Soil, Under, UnderDark, 0.3f, nullptr);
	UMaterialInterface* SoilM = ZoogiUtil::GroundMID(this, Soil, SoilDark, 0.01f, 0.95f);
	UMaterialInterface* UnderM = ZoogiUtil::GroundMID(this, Under, UnderDark, 0.006f, 0.97f);
	UMaterialInterface* TopM = ZoogiUtil::GroundMID(this, Grass, GrassDark, 0.004f, 0.9f);
	const float R = ZoogiConst::KnockoffRadius;
	{
		constexpr int32 NS = 20;
		const float Rc = R / FMath::Cos(PI / NS), Zt = 0.6f, Zb = -50.f / TA, Zu = -320.f / TA;
		FZoogiMeshBuilder Top, Band, Side;
		for (int32 i = 0; i < NS; ++i)
		{
			const float A0 = 2.f * PI * i / NS, A1 = 2.f * PI * (i + 1) / NS;
			const FVector2D P0(FMath::Cos(A0) * Rc, FMath::Sin(A0) * Rc), P1(FMath::Cos(A1) * Rc, FMath::Sin(A1) * Rc);
			const FVector N(FVector2D((P0 + P1) * 0.5f).GetSafeNormal(), 0.f);
			Top.FlatTri(FVector(0, 0, Zt), FVector(P0, Zt), FVector(P1, Zt), FVector::UpVector);
			Band.Quad(FVector(P0, Zb), FVector(P1, Zb), FVector(P1, Zt), FVector(P0, Zt), N);
			Side.Quad(FVector(P0 * 0.985f, Zu), FVector(P1 * 0.985f, Zu), FVector(P1 * 0.985f, Zb), FVector(P0 * 0.985f, Zb), N);
		}
		if (UProceduralMeshComponent* C = AddFrozenProc(Top, TopM, nullptr, false)) C->ComponentTags.Add(TEXT("NoCamBlock"));
		if (UProceduralMeshComponent* C = AddFrozenProc(Band, SoilM, nullptr, true)) C->ComponentTags.Add(TEXT("NoCamBlock"));
		if (UProceduralMeshComponent* C = AddFrozenProc(Side, UnderM, nullptr, true)) C->ComponentTags.Add(TEXT("NoCamBlock"));
	}
	AddUnderCone(FVector2D::ZeroVector, R * 0.96f, 1080.f / TA, -320.f / TA, UnderM);                                            // cone to ~ -1400

	// Four red star bumpers (existing AddBumper, restitution as the other maps): top r322, height 140 (world).
	UMaterialInterface* PadM = ZoogiUtil::SurfaceMID(this, PadRed, 0.45f);
	UMaterialInterface* PadSideM = ZoogiUtil::SurfaceMID(this, PadSide, 0.5f);
	UMaterialInterface* StarM = ZoogiUtil::SurfaceMID(this, StarY, 0.4f);
	struct FPad { float R, Theta; };
	const FPad Pads[] = { { 1300.f, 4.f }, { 480.f, -4.f }, { 1150.f, -80.f }, { 1050.f, 102.f } }; // Phase 3: staggered to f_017 (back, centre, left mid, right nearer)
	const float PadR = 322.f / TA, PadH = 140.f / TA;
	// v4: visual = the 0.1.2 Star Bumper mesh (/Game/Legacy012/SM_StarBumper, asset import from 0eab718), fitted to the SAME
	// collider footprint (PadR x PadH, unchanged AddBumper collider + restitution). Falls back to the v3 cylinder+star look.
	UStaticMesh* StarBumper = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Legacy012/SM_StarBumper.SM_StarBumper"));
	UE_LOG(LogZoogi, Log, TEXT("TEASER: star bumper mesh %s"), StarBumper ? TEXT("SM_StarBumper") : TEXT("MISSING (fallback cylinder)"));
	for (const FPad& Pd : Pads)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiStarProps"))) break; // b62 (Evan): no red star bumpers anywhere
		const FVector C = Pol(Pd.R, Pd.Theta, 0.f);
		if (StarBumper)
		{
			const FBox B = StarBumper->GetBoundingBox();
			const FVector Ext = B.GetExtent();
			const float SXY = PadR / FMath::Max(FMath::Max(Ext.X, Ext.Y), 1.f), SZ = PadH / FMath::Max(Ext.Z * 2.f, 1.f);
			(void)SXY; (void)SZ; // Oct 24: same fit, now via the shared dev-mode prop helper (ZoogiArenaProps.cpp)
			AddStarBumperProp(GetActorTransform().TransformPosition(C), 0.f, 1.f);
			continue;
		}
		AddBumper(FVector2D(C), PadR, PadH, ZoogiUtil::BasicShape(TEXT("Cylinder")), PadSideM, FVector(PadR / 50.f, PadR / 50.f, PadH / 100.f), PadH * 0.5f, 0.55f);
		AddCylinder(FVector2D(C), PadR - 1.f, 2.f, PadH + 0.5f, PadM, false)->ComponentTags.Add(TEXT("NoCamBlock")); // bright red top face
		FZoogiMeshBuilder S;
		Star(S, PadR * 0.82f, PadR * 0.34f, 0.f, 1.5f);
		USceneComponent* Pv = NewPivot(FVector(C.X, C.Y, PadH + 1.f));
		Pv->SetRelativeRotation(FRotator(0.f, WorldYaw(180.f), 0.f)); // star tip faces the camera / elevator side
		if (UProceduralMeshComponent* Sc = AddFrozenProc(S, StarM, Pv, false)) Sc->ComponentTags.Add(TEXT("NoCamBlock"));
	}

	// Phase 3: big pale craggy rock walls + dark beam / slab = the real Zoogi Roll 0.1.2 meshes (crumbling_tower pPipe4
	// polySurface1/2, Trearnch Ark01 + Column01, WallBreak03), embedded in ZoogiTeaserRef.h at 0.1.2 size, each turned to
	// where the teaser frames show it. Visual only: no collision, NoCamBlock, two-sided (the 0.1.2 shells are open).
	auto RefMesh = [this](const float* V, int32 N, UMaterialInterface* Mat, bool bShadow)
	{
		// r5: split each shell into ~500 cm (world) XY chunks so every piece has a tight box. The occlusion fade skips any
		// mesh whose box holds both the camera and the Zoogi, and a whole rim wall's box does in the low Aim view, so the
		// near wall stayed solid. Same triangles, same layout - only the component granularity changes.
		const float Cell = 500.f / BuildScale;
		TMap<FIntPoint, FZoogiMeshBuilder> Chunks;
		for (int32 i = 0; i + 2 < N; i += 3)
		{
			const FVector A(V[i * 3], V[i * 3 + 1], V[i * 3 + 2]), B(V[i * 3 + 3], V[i * 3 + 4], V[i * 3 + 5]), C(V[i * 3 + 6], V[i * 3 + 7], V[i * 3 + 8]);
			const FVector Nn = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
			if (Nn.IsNearlyZero()) continue;
			const FVector Ct = (A + B + C) / 3.f;
			FZoogiMeshBuilder& M = Chunks.FindOrAdd(FIntPoint(FMath::FloorToInt(Ct.X / Cell), FMath::FloorToInt(Ct.Y / Cell)));
			M.FlatTri(A, B, C, Nn);
			M.FlatTri(A, C, B, -Nn);
		}
		for (TPair<FIntPoint, FZoogiMeshBuilder>& K : Chunks)
			if (UProceduralMeshComponent* P = AddFrozenProc(K.Value, Mat, nullptr, bShadow)) { P->ComponentTags.Add(TEXT("NoCamBlock")); P->ComponentTags.Add(TEXT("CamFadeAlways")); P->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); P->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); }
	};
	UMaterialInterface* RockM = ZoogiUtil::GroundMID(this, Rock, RockDark, 0.004f, 0.95f);
	UMaterialInterface* DarkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#3E3E33")), 0.85f); // 0.1.2 Column01 / Wall01 texture avg
	// v3: the walls / beam cast no shadow - Wall1's shadow fell across the theta -80 pad and turned it dark red (the
	// teaser pads are all evenly lit). Walls lean inward (overhang sheared in gen_ref.py); the long dark Bar is a stretched
	// 0.1.2 Column01 lying from the left wall top across the near-left rim (f_017 / f_018 foreground beam).
	const bool bDbgCol = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTeaserDbgCol")); // TEMP diagnostic
	auto DbgM = [&](UMaterialInterface* M, const TCHAR* H) { return bDbgCol ? ZoogiUtil::SurfaceMID(this, Hex(H), 0.9f) : M; };
	RefMesh(ZoogiTeaserRef::Wall1V, ZoogiTeaserRef::Wall1N, DbgM(RockM, TEXT("#FF00FF")), false);
	RefMesh(ZoogiTeaserRef::Wall2V, ZoogiTeaserRef::Wall2N, DbgM(RockM, TEXT("#00FFFF")), false);
	RefMesh(ZoogiTeaserRef::BeamV, ZoogiTeaserRef::BeamN, DbgM(DarkM, TEXT("#0000FF")), false);
	RefMesh(ZoogiTeaserRef::SlabV, ZoogiTeaserRef::SlabN, DbgM(DarkM, TEXT("#FFFFFF")), false);
	RefMesh(ZoogiTeaserRef::BarV, ZoogiTeaserRef::BarN, DbgM(DarkM, TEXT("#FF8000")), false);
	// Red-spotted mushrooms on the lip (f_017 left, f_024 right).
	UMaterialInterface* MushM = ZoogiUtil::SurfaceMID(this, Mush, 0.5f);
	UMaterialInterface* StemM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#F2E8D5")), 0.7f);
	UMaterialInterface* DotM = ZoogiUtil::SurfaceMID(this, FLinearColor::White, 0.6f);
	const float MushT[] = { -150.f, -100.f, 125.f };
	for (float T : MushT)
	{
		const FVector C = Pol(1975.f, T, 0.f);
		AddCylinder(FVector2D(C), 18.f / TA, 70.f / TA, 70.f / TA, StemM, false)->ComponentTags.Add(TEXT("NoCamBlock"));
		UStaticMeshComponent* Cap = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), MushM, false);
		Cap->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.6f) / TA); Cap->SetRelativeLocation(FVector(C.X, C.Y, 75.f / TA)); Cap->ComponentTags.Add(TEXT("NoCamBlock"));
		for (int32 d = 0; d < 4; ++d)
		{
			const float A = d * PI * 0.5f + 0.4f;
			UStaticMeshComponent* Dt = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), DotM, false, false);
			Dt->SetRelativeScale3D(FVector(0.16f) / TA); Dt->SetRelativeLocation(FVector(C.X + FMath::Cos(A) * 30.f / TA, C.Y + FMath::Sin(A) * 30.f / TA, 98.f / TA)); Dt->ComponentTags.Add(TEXT("NoCamBlock"));
		}
	}

	// Grass tufts (40, r200..1900): flat dark cones, no collision, no camera block.
	UMaterialInterface* TuftM = ZoogiUtil::SurfaceMID(this, GrassDark, 0.9f);
	FRandomStream TR(17);
	for (int32 i = 0; i < 40; ++i)
	{
		const float Rr = FMath::Sqrt(TR.FRandRange(200.f * 200.f, 1900.f * 1900.f)), T = TR.FRandRange(-180.f, 180.f);
		const FVector C = Pol(Rr, T, 0.f);
		bool bOnPad = false;
		for (const FPad& Pd : Pads) if (FVector2D::Distance(FVector2D(C), FVector2D(Pol(Pd.R, Pd.Theta, 0.f))) < (340.f / TA)) bOnPad = true;
		if (bOnPad) continue;
		UStaticMeshComponent* Tf = AddMesh(ZoogiUtil::BasicShape(TEXT("Cone")), TuftM, false, false);
		Tf->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.16f) / TA); Tf->SetRelativeLocation(FVector(C.X, C.Y, 8.f / TA)); Tf->ComponentTags.Add(TEXT("NoCamBlock"));
	}

	// Floating islands (8, r5200..6600, Z -600 / -200): grass top, rock cone, round trees, a mushroom.
	UMaterialInterface* TrunkM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#6B4A2B")), 0.8f);
	UMaterialInterface* LeafM = ZoogiUtil::SurfaceMID(this, TreeG, 0.75f);
	struct FIsl { float R, Theta, Z, S; };
	const FIsl Isl[] = { { 5200, 20, -600, 0.9f }, { 5900, 65, -200, 1.2f }, { 6600, 110, -600, 1.5f }, { 5200, 155, -200, 0.9f },
		{ 5900, 200, -600, 1.2f }, { 6600, 245, -200, 1.5f }, { 5200, 290, -600, 0.9f }, { 5900, 335, -200, 1.2f } };
	FRandomStream IR(4242);
	for (const FIsl& I : Isl)
	{
		const FVector C = Pol(I.R, I.Theta, I.Z);
		const float Rad = 520.f * I.S / TA;
		BuildIslet(C, Rad, Grass, GrassDark, Rock, nullptr, 0, 0.f, IR);
		const int32 NT = 1 + IR.RandRange(0, 2);
		for (int32 t = 0; t < NT; ++t)
		{
			const float A = IR.FRandRange(0.f, 2.f * PI), D = IR.FRandRange(0.f, Rad * 0.55f), H = 260.f * I.S / TA;
			const FVector B(C.X + FMath::Cos(A) * D, C.Y + FMath::Sin(A) * D, C.Z);
			AddCylinder(FVector2D(B), 18.f * I.S / TA, H * 0.5f, C.Z + H * 0.5f, TrunkM, false);
			UStaticMeshComponent* Lf = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), LeafM, false);
			Lf->SetRelativeScale3D(FVector(H / 100.f * 0.75f)); Lf->SetRelativeLocation(FVector(B.X, B.Y, C.Z + H * 0.75f));
		}
		UStaticMeshComponent* Cap = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), MushM, false);
		Cap->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.5f) * I.S / TA); Cap->SetRelativeLocation(FVector(C.X + Rad * 0.5f, C.Y, C.Z + 60.f * I.S / TA));
	}

	// Phase 3 sky panorama: open ocean far below + a dark treeline on the horizon (f_017 / f_018), under the existing
	// gradient sky (ApplyTheme). Colours graded from the frames and the 0.1.2 camera clear colour (0.19, 0.30, 0.48).
	{
		UMaterialInterface* SeaM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#3A7CC0")), 0.25f); // v3 lighter (f_024 low sky #387CC0)
		UMaterialInterface* WoodM = ZoogiUtil::SurfaceMID(this, Hex(TEXT("#1E4A22")), 0.95f);
		const float SeaZ = -9000.f / TA;
		FZoogiMeshBuilder Sea, Wood;
		constexpr int32 NS = 48;
		const float SeaR = 400000.f / TA, WoodR = 120000.f / TA;
		FRandomStream WR(2015);
		for (int32 i = 0; i < NS; ++i)
		{
			const float A0 = 2.f * PI * i / NS, A1 = 2.f * PI * (i + 1) / NS;
			Sea.FlatTri(FVector(0, 0, SeaZ), FVector(FMath::Cos(A0) * SeaR, FMath::Sin(A0) * SeaR, SeaZ), FVector(FMath::Cos(A1) * SeaR, FMath::Sin(A1) * SeaR, SeaZ), FVector::UpVector);
		}
		for (int32 i = 0; i < 160; ++i)
		{
			const float A = 2.f * PI * i / 160.f + WR.FRandRange(-0.01f, 0.01f);
			const float Rr = WoodR * WR.FRandRange(0.95f, 1.05f), H = WR.FRandRange(3500.f, 7000.f) / TA, W = WR.FRandRange(2500.f, 4500.f) / TA;
			const FVector C(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, SeaZ);
			const FVector T(-FMath::Sin(A), FMath::Cos(A), 0.f), In = -FVector(C.X, C.Y, 0.f).GetSafeNormal();
			Wood.FlatTri(C - T * W, C + T * W, C + FVector(0, 0, H), In);
			Wood.FlatTri(C - T * W, C + T * W, C + FVector(0, 0, H), -In);
		}
		if (UProceduralMeshComponent* P = AddFrozenProc(Sea, SeaM, nullptr, false)) P->ComponentTags.Add(TEXT("NoCamBlock"));
		if (UProceduralMeshComponent* P = AddFrozenProc(Wood, WoodM, nullptr, false)) P->ComponentTags.Add(TEXT("NoCamBlock"));
	}
}
