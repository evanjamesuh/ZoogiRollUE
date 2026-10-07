// Meadow Arena (hand-built kit, no Aura). Spec: reskin-incoming/01-meadow-arena/
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiMeadowLayout.h"
#include "ZoogiTypes.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFloorScars.h"
#include "ZoogiArenaPlayTest.h"
#include "ZoogiStageForming.h"
#include "ZoogiMeshKit.h"
#include "ZoogiPhysics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "ZoogiMeadowBatch1.inl"

void AZoogiArena::BuildMeadow()
{
	using namespace ZoogiMeadow; // function scope: unity builds also pull in ZoogiFrozen's WallR/GateYaw/...
	const FLinearColor Green = ZoogiUtil::Hex(TEXT("#39E37A"));
	// r19 (Evan 7:16): whole Meadow 1.3x larger (actor + every piece scale together), 6 orbs.
	BuildScale = MeadowScale; SetActorScale3D(FVector(MeadowScale));
	KnockoffRadiusOverride = FloorR * MeadowScale; ActiveKnockoffR = KnockoffRadiusOverride;
	SpawnRadiusOverride = ZoogiConst::SpawnRadius * MeadowScale; OrbCountOverride = MeadowOrbs;
	constexpr float SealGlow = 7.0f; // r8: lower strength + deeper green so the tonemapper keeps it saturated // r4: bold seal (T_MeadowGlow rebaked: thick core + soft halo)
	const FLinearColor Soil = ZoogiUtil::Hex(TEXT("#9C8B73"));
	const FLinearColor Wood = ZoogiUtil::Hex(TEXT("#CDA28B"));
	const FLinearColor WoodAlt = ZoogiUtil::Hex(TEXT("#D9B29A"));
	const FLinearColor Riser = ZoogiUtil::Hex(TEXT("#A0705A"));
	const FLinearColor SeatStone = ZoogiUtil::Hex(TEXT("#A8926E")); // r11: warm sandstone, not white
	const FLinearColor RockA = ZoogiUtil::Hex(TEXT("#B9A889"));
	const FLinearColor RockB = ZoogiUtil::Hex(TEXT("#8A7458"));

	UMaterialInterface* FloorMat = KitFloorMaterial(TEXT("Meadow"), ZoogiUtil::Hex(TEXT("#00B83A")), SealGlow, 0.80f); // r10: warm tan floor like the art (was washed cream)
	BuildIsland(ZoogiUtil::Hex(TEXT("#93A644")), ZoogiUtil::Hex(TEXT("#A6B656")), Soil,
		ZoogiUtil::Hex(TEXT("#6E5B4A")), ZoogiUtil::Hex(TEXT("#4A3B30")), 0.3f, FloorMat);
	FloorMID = Cast<UMaterialInstanceDynamic>(FloorMat);
	if (FloorMID) FloorMID->SetScalarParameterValue(TEXT("GlowTint"), 1.0f); // r4: seal lines paint near-solid green into the base, not a faint tint
	AddPanoramaSky(TEXT("Meadow"), 1.0f, 0.f, FLinearColor::White);
	AddCurb(FloorR + 40.f, -2.f, 55.f, ZoogiUtil::GroundMID(this, Soil, ZoogiUtil::Hex(TEXT("#7A6B55")), 0.01f, 0.92f));

	UMaterialInterface* Stone = ZoogiUtil::GroundMID(this, SeatStone, RockB, 0.01f, 0.9f);
	UMaterialInterface* StoneDark = ZoogiUtil::GroundMID(this, RockB, ZoogiUtil::Hex(TEXT("#6A5C48")), 0.01f, 0.92f);
	UMaterialInterface* Slit = ZoogiUtil::GlowMID(this, Green, 1.4f);
	UMaterialInterface* Bronze = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#D9A93A")), 0.35f, ZoogiUtil::Hex(TEXT("#D9A93A")), 0.25f, 0.85f);
	UMaterialInterface* Vine = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#4E8F3A")), 0.85f);
	UMaterialInterface* Crystal = ZoogiUtil::GlowMID(this, Green, 2.6f);
	UMaterialInterface* Seat = ZoogiUtil::SurfaceMID(this, Wood, 0.8f);
	UMaterialInterface* SeatAlt = ZoogiUtil::SurfaceMID(this, WoodAlt, 0.8f);
	UMaterialInterface* SeatRiser = ZoogiUtil::SurfaceMID(this, Riser, 0.85f);
	UMaterialInterface* GrassTop = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#93A644")), ZoogiUtil::Hex(TEXT("#A6B656")), 0.004f, 0.92f);
	UMaterialInterface* Cliff = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#A08A6C")), ZoogiUtil::Hex(TEXT("#8A7458")), 0.01f, 0.95f); // r11: sunlit sandstone cliffs like the art
	UMaterialInterface* GlowTop = ZoogiUtil::GlowMID(this, Green, 1.8f);
	UPhysicalMaterial* PillarPM = ZoogiPhysics::MakeMaterial(this, 0.25f, 0.6f, false);
	UPhysicalMaterial* WallPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.85f, false);
	UPhysicalMaterial* StonePM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.35f, false);

	AddHubCore(FVector(0, 0, 175.f), 46.f, Crystal); // r20: sits in the hub cup
	OrbRingRadiusOverride = OrbRingR * MeadowScale; OrbRingYawOffset = OrbRingYaw;

	// 4 Resonance pillars (unbreakable), rise at match start.
	UMaterialInterface* PilStone = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A89A86")), 0.9f);
	UMaterialInterface* PilDark = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#7E7262")), 0.9f);
	for (int32 k = 0; k < 4; ++k)
	{
		const float Yaw = PillarYaw[k];
		const float Rad = FMath::DegreesToRadians(Yaw);
		const FVector Loc(FMath::Cos(Rad) * PillarR, FMath::Sin(Rad) * PillarR, 0.f);
		AZoogiArenaPiece* P = SpawnPiece(Loc, Yaw, *FString::Printf(TEXT("pillar_%d"), k), TEXT("Stone"), 1000.f, true);
		if (!P) continue;
		const float S = PillarS;
		P->AddBox(FVector(0, 0, 18.f), FVector(150.f * S, 150.f * S, 36.f), FRotator::ZeroRotator, PilDark);
		P->AddBox(FVector(0, 0, 50.f), FVector(118.f * S, 118.f * S, 30.f), FRotator::ZeroRotator, PilStone);
		FZoogiMeshBuilder Shaft;
		Shaft.Prism(FVector2D::ZeroVector, 4, 44.f * S, 34.f * S, 62.f, PillarH - 22.f, 45.f, 0.f, false);
		P->AddProc(Shaft, PilStone);
		P->AddBox(FVector(0, 0, PillarH - 12.f), FVector(82.f * S, 82.f * S, 20.f), FRotator::ZeroRotator, PilStone);
		for (int32 f = 0; f < 4; ++f)
		{
			const FRotator Fr(0.f, f * 90.f, 0.f);
			for (int32 s = -1; s <= 1; s += 2)
			{
				UStaticMeshComponent* Sl = P->AddBox(Fr.RotateVector(FVector(30.f * S + 3.f, s * 12.f * S, 0.f)) + FVector(0, 0, PillarH * 0.52f + 20.f), FVector(3.f, 8.f, PillarH * 0.58f), Fr, Slit, false);
				P->MarkGlow(Sl);
			}
			// glyph band near the base
			UStaticMeshComponent* G = P->AddBox(Fr.RotateVector(FVector(40.f * S + 2.f, 0.f, 0.f)) + FVector(0, 0, 92.f), FVector(3.f, 54.f * S, 8.f), Fr, Slit, false);
			P->MarkGlow(G);
		}
		for (int32 v = 0; v < 8; ++v)
		{
			const float T = v / 7.f;
			const float A = FMath::DegreesToRadians(30.f + T * 300.f);
			P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(FMath::Cos(A) * 40.f * S, FMath::Sin(A) * 40.f * S, 70.f + T * (PillarH - 100.f)), FVector(0.34f, 0.34f, 0.26f)), Vine, false);
		}
		P->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 14.f), FVector(0.42f * S, 0.42f * S, 0.28f)), Bronze);
		P->AddShape(TEXT("Cone"), FTransform(FRotator(180.f, 0.f, 0.f), FVector(0, 0, PillarH + 52.f), FVector(1.2f * S, 1.2f * S, 0.5f)), Bronze);
		UStaticMeshComponent* OrbM = P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 100.f), FVector(0.62f * S, 0.62f * S, 0.8f * S)), ZoogiUtil::GlowMID(this, Green, 2.4f), false);
		P->MarkGlow(OrbM);
		P->AddColliderBox(FVector(0, 0, 33.f), FVector(140.f * S, 140.f * S, 66.f), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
		P->AddColliderBox(FVector(0, 0, PillarH * 0.5f), FVector(84.f * S, 84.f * S, PillarH), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(PillarH + 120.f)), 1.30f + 0.10f * k, 1.0f, 14.f);
		// Arena Orb apron in front of each pillar (toward centre), clear of the seat-wall gaps.
		const float ApronR = PillarR + 160.f; // r9: outside the pillar (the inside is the spiral / orb ring)
		(void)ApronR; // r19: Meadow starts with exactly 6 orbs (no apron orbs)
	}

	// Stadium-seat wall with 4 orb-exit gaps (MEADOW ONLY). No VFX/debris in gaps.
	{
		const float R = FloorR + 18.f;
		const int32 Segs = 48;
		for (int32 i = 0; i < Segs; ++i)
		{
			const float Am = i * (360.f / Segs) + 180.f / Segs;
			if (NearOrbExitGap(Am, 0.5f)) continue;
			// r20: no quarter-pipes, the seat wall runs unbroken between the 4 gaps
			const float Rad = FMath::DegreesToRadians(Am);
			const float SegLen = R * FMath::DegreesToRadians(360.f / Segs) * 1.05f;
			const FVector Loc(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, 0.f);
			AZoogiArenaPiece* P = SpawnPiece(Loc, Am, TEXT("seatwall"), TEXT("Wood"), 420.f, false); // r19: soft hits crack it; a normal shot no longer breaks it in one go
			if (!P) continue;
			for (int32 t = 0; t < 2; ++t)
			{
				const float Z = (t + 1) * (SeatWallH * 0.5f);
				const float D = SeatWallThick + t * 18.f;
				P->AddBox(FVector(t * 10.f, 0.f, Z * 0.5f), FVector(D, SegLen * 0.96f, SeatWallH * 0.48f), FRotator::ZeroRotator, t ? SeatAlt : Seat);
			}
			P->AddColliderBox(FVector(8.f, 0.f, SeatWallH * 0.55f), FVector(SeatWallThick + 20.f, SegLen, SeatWallH + 8.f), FRotator::ZeroRotator, TEXT("ZWall"), WallPM);
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(SeatWallH + 40.f)), 0.4f + Am / 360.f * 0.9f, 0.7f);
		}
		UE_LOG(LogZoogi, Log, TEXT("Meadow orb-exit gaps: width %.0f cm, centers %.0f/%.0f/%.0f/%.0f deg (half-gap %.1f deg)"),
			OrbExitGapWidthCm, OrbExitGapYaw[0], OrbExitGapYaw[1], OrbExitGapYaw[2], OrbExitGapYaw[3], OrbExitGapHalfDeg);
	}

	// ---- r5 ramps (measured off the Buzzy top-down; all wood/stone pieces stay breakable) ----
	auto Pol = [](float R, float Deg, float Z = 0.f) { const float A = FMath::DegreesToRadians(Deg); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); };
	UMaterialInterface* RampWood = Seat;
	UMaterialInterface* RampEdge = SeatRiser;
	(void)RampWood; (void)RampEdge;
	// r20 (Evan 7:39, Hero v4): no centre spiral / raised disc. Flat stepped hub dais with the glowing green orb.
	{
		UMaterialInterface* HubStone = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C9B898")), 0.9f);
		UMaterialInterface* HubStoneDark = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A08C70")), 0.9f);
		AZoogiArenaPiece* H = SpawnPiece(FVector::ZeroVector, 0.f, TEXT("hub"), TEXT("Stone"), 600.f, false);
		if (H)
		{
			FZoogiMeshBuilder S1, S2, Rim;
			S1.Prism(FVector2D::ZeroVector, 32, 270.f, 262.f, -2.f, 12.f, 0.f, 0.f, false);
			S2.Prism(FVector2D::ZeroVector, 32, 175.f, 168.f, 10.f, 26.f, 0.f, 0.f, false);
			H->AddProc(S1, HubStone); H->AddProc(S2, HubStoneDark);
			for (int32 g = 0; g < 2; ++g)
			{
				const float Rr = g ? 150.f : 240.f, Zr = g ? 26.6f : 12.6f;
				UStaticMeshComponent* Gl = H->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, Zr), FVector(Rr / 50.f, Rr / 50.f, 0.004f)), GlowTop, false);
				H->MarkGlow(Gl);
				H->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, Zr + 0.3f), FVector((Rr - 14.f) / 50.f, (Rr - 14.f) / 50.f, 0.006f)), g ? HubStoneDark : HubStone, false);
			}
			// carved pedestal + bronze cup holding the orb
			FZoogiMeshBuilder Ped; Ped.Prism(FVector2D::ZeroVector, 8, 54.f, 38.f, 26.f, 96.f, 22.5f, 0.f, false);
			H->AddProc(Ped, HubStone);
			H->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, 104.f), FVector(0.9f, 0.9f, 0.16f)), Bronze);
			H->AddShape(TEXT("Cone"), FTransform(FRotator(180.f, 0.f, 0.f), FVector(0, 0, 128.f), FVector(1.1f, 1.1f, 0.4f)), Bronze);
			for (int32 r = 0; r < 8; ++r) { const FRotator Fr(0.f, r * 45.f + 22.5f, 0.f); H->MarkGlow(H->AddBox(Fr.RotateVector(FVector(46.f, 0, 0)) + FVector(0, 0, 62.f), FVector(3.f, 10.f, 46.f), Fr, Slit, false)); }
			H->AddColliderCyl(FVector2D::ZeroVector, 268.f, -2.f, 12.f, TEXT("ZPlatform"), StonePM);
			H->AddColliderCyl(FVector2D::ZeroVector, 172.f, -2.f, 26.f, TEXT("ZPlatform"), StonePM);
			H->AddColliderCyl(FVector2D::ZeroVector, 56.f, -2.f, 130.f, TEXT("ZPillar"), PillarPM);
			H->FinishBuild();
			H->HitSpeedMin = 900.f; // rolling up the steps never damages it
		}
	}
	// r20 (Evan 7:42): no ramps at all (spiral, disc, quarter-pipes and lips removed).

	// r22: A&P Meadow kit meshes (/Game/Env/MeadowBuildings, front = -Y). Slots get SurfaceMIDs so piece damage tints work.
	TMap<FName, UMaterialInterface*> KitMats;
	auto KitMat = [&](FName Slot) -> UMaterialInterface*
	{
		if (UMaterialInterface** F = KitMats.Find(Slot)) return *F;
		const FString S = Slot.ToString();
		const TCHAR* Hx = TEXT("#A8A08A"); float Ro = 0.85f; bool bGlow = false;
		if (S.StartsWith(TEXT("M_TileGray"))) Hx = TEXT("#8E9090");
		else if (S.StartsWith(TEXT("M_Tile"))) { Hx = TEXT("#B8452E"); Ro = 0.55f; }
		else if (S.StartsWith(TEXT("M_Ridge"))) Hx = TEXT("#6E2A1E");
		else if (S.StartsWith(TEXT("M_WoodRed"))) { Hx = TEXT("#9A3526"); Ro = 0.6f; }
		else if (S.StartsWith(TEXT("M_Wall"))) Hx = TEXT("#E6D8BC");
		else if (S.StartsWith(TEXT("M_Gold"))) { Hx = TEXT("#C9A13B"); Ro = 0.4f; }
		else if (S.StartsWith(TEXT("M_Lattice"))) Hx = TEXT("#4A2E1C");
		else if (S.StartsWith(TEXT("M_CrateWood"))) { Hx = TEXT("#A97A4E"); Ro = 0.9f; }
		else if (S.StartsWith(TEXT("M_Iron"))) { Hx = TEXT("#3A3530"); Ro = 0.6f; }
		else if (S.StartsWith(TEXT("M_Moss"))) Hx = TEXT("#5E7A3A");
		else if (S.StartsWith(TEXT("M_RuneGlow")) || S.StartsWith(TEXT("M_Gem"))) bGlow = true;
		UMaterialInterface* M = bGlow ? ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#39E37A")), 2.f) : ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(Hx), Ro);
		KitMats.Add(Slot, M);
		return M;
	};
	auto KitPart = [&](AZoogiArenaPiece* P, const TCHAR* Name, const FVector& Loc, float Yaw, float Scale) -> UStaticMeshComponent*
	{
		UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/MeadowBuildings/%s.%s"), Name, Name));
		if (!SM || !P) return nullptr;
		UStaticMeshComponent* C = P->AddShape(TEXT("Cube"), FTransform(FRotator(0.f, Yaw - 90.f, 0.f), Loc, FVector(Scale)), nullptr);
		C->SetStaticMesh(SM);
		for (int32 m = 0; m < SM->GetStaticMaterials().Num(); ++m) C->SetMaterial(m, KitMat(SM->GetStaticMaterials()[m].MaterialSlotName));
		return C;
	};

	// r19: weathered wooden crates and plank benches (art), breakable, tangential like the art.
	{
		UMaterialInterface* CrateWood = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A87C52")), 0.9f);
		UMaterialInterface* CrateEdge = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#5E4030")), 0.9f);
		UMaterialInterface* Plank = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B58A60")), 0.88f);
		int32 CrateN = 0;
		auto Crate = [&](AZoogiArenaPiece* P, const FVector& C, float E, float Yw)
		{
			{ // r22: A&P weathered crate mesh (A 100 / B 120x80 / C 60 cm), sized to E
				const int32 k = CrateN++ % 3;
				const TCHAR* N = k == 0 ? TEXT("SM_MeadowCrate_A") : (k == 1 ? TEXT("SM_MeadowCrate_C") : TEXT("SM_MeadowCrate_B"));
				const float Base = k == 0 ? 100.f : (k == 1 ? 60.f : 100.f);
				if (KitPart(P, N, C - FVector(0, 0, E * 0.5f), Yw + 90.f, 1.45f * E / Base)) return; // r23: v4 crates are bigger
			}
			const FRotator Rr(0.f, Yw, 0.f);
			P->AddBox(C, FVector(E - 4.f), Rr, CrateWood);
			const float H = E * 0.5f, T = FMath::Max(7.f, E * 0.09f);
			for (int32 a = 0; a < 3; ++a) for (int32 u = -1; u <= 1; u += 2) for (int32 v = -1; v <= 1; v += 2)
			{
				FVector Off, Sz;
				if (a == 0) { Off = FVector(0, u * H, v * H); Sz = FVector(E, T, T); }
				else if (a == 1) { Off = FVector(u * H, 0, v * H); Sz = FVector(T, E, T); }
				else { Off = FVector(u * H, v * H, 0); Sz = FVector(T, T, E); }
				P->AddBox(C + Rr.RotateVector(Off), Sz, Rr, CrateEdge);
			}
			for (int32 u = -1; u <= 1; u += 2) // diagonal braces on two faces
			{
				P->AddBox(C + Rr.RotateVector(FVector(u * (H + 1.f), 0, 0)), FVector(4.f, E * 1.25f, T * 0.8f), FRotator(0.f, Yw, 45.f * u), CrateEdge);
				P->AddBox(C + Rr.RotateVector(FVector(0, u * (H + 1.f), 0)), FVector(E * 1.25f, 4.f, T * 0.8f), FRotator(45.f * u, Yw, 0.f), CrateEdge);
			}
		};
		for (int32 b = 0; b < NumBenches; ++b)
		{
			AZoogiArenaPiece* P = SpawnPiece(Pol(BenchR, BenchYaw[b], 0.f), BenchYaw[b] + 90.f, TEXT("bench"), TEXT("Wood"), 70.f, false);
			if (!P) continue;
			if (true) // r20: v4 has crates all round the middle ring
			{
				Crate(P, FVector(-20.f, 0, 40.f), 80.f, 0.f);
				Crate(P, FVector(48.f, 4.f, 28.f), 56.f, 14.f);
				if (b % 4 == 0) Crate(P, FVector(-16.f, 2.f, 104.f), 48.f, -10.f);
				P->AddColliderBox(FVector(8.f, 0, 52.f), FVector(190.f, 116.f, 104.f), FRotator::ZeroRotator, TEXT("ZWall"), WallPM);
				P->SetPathBlock(FVector2D(P->GetActorLocation()), 85.f * MeadowScale); // r22: CPU planning + IsSpotClear
			}
			else
			{
				for (int32 k = -1; k <= 1; ++k) P->AddBox(FVector(0, k * 17.f, 40.f), FVector(140.f, 15.f, 8.f), FRotator(0.f, 0.f, 0.f), Plank);
				for (int32 sx = -1; sx <= 1; sx += 2)
				{
					for (int32 sy = -1; sy <= 1; sy += 2) P->AddBox(FVector(sx * 52.f, sy * 18.f, 18.f), FVector(10.f, 10.f, 36.f), FRotator::ZeroRotator, CrateEdge);
					P->AddBox(FVector(sx * 52.f, 0, 22.f), FVector(8.f, 44.f, 6.f), FRotator::ZeroRotator, CrateEdge);
				}
				P->AddBox(FVector(0, 0, 14.f), FVector(110.f, 6.f, 6.f), FRotator::ZeroRotator, CrateEdge);
				P->AddColliderBox(FVector(0, 0, 23.f), FVector(140.f, 54.f, 46.f), FRotator::ZeroRotator, TEXT("ZWall"), WallPM);
			}
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -150.f), 1.2f + 0.05f * b, 0.6f);
		}
		const float InnerYaw[3] = { 45.f, 165.f, 285.f }; // r22: A&P layout v2 -> court crates at r620-1250
		for (int32 b = 0; b < 3; ++b)
		{
			AZoogiArenaPiece* P = SpawnPiece(Pol(1150.f, InnerYaw[b], 0.f), InnerYaw[b] + 90.f, TEXT("bench"), TEXT("Wood"), 70.f, false);
			if (!P) continue;
			Crate(P, FVector(-14.f, 0, 36.f), 72.f, 6.f);
			Crate(P, FVector(40.f, 6.f, 26.f), 52.f, -12.f);
			P->AddColliderBox(FVector(6.f, 0, 48.f), FVector(170.f, 110.f, 96.f), FRotator::ZeroRotator, TEXT("ZWall"), WallPM);
			P->SetPathBlock(FVector2D(P->GetActorLocation()), 75.f * MeadowScale);
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -150.f), 1.5f + 0.05f * b, 0.6f);
		}
	}

	// r3: drop channels cut through the terrace at the 4 orb-exit gaps so an orb visibly drops off (no clipping).
	TerraceGapYaw = { OrbExitGapYaw[0], OrbExitGapYaw[1], OrbExitGapYaw[2], OrbExitGapYaw[3] };
	TerraceGapHalfDeg = OrbExitGapHalfDeg + 1.2f;
	AddTerrace(1590.f, 2400.f, -40.f, 1650.f, -20.f, GrassTop, GrassTop, GrassTop); // r14: grass meets the stone like the art (was a 160 cm moat step)
	FRandomStream Rng(77);
	UStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* ConeMesh = ZoogiUtil::BasicShape(TEXT("Cone"));
	ScatterInstances(ConeMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#9AAE4E")), 0.9f), 700, 1700.f, 4200.f, FVector2D(0.28f, 0.45f), -44.f, Rng);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FDE047")), 0.6f, ZoogiUtil::Hex(TEXT("#FDE047")), 0.4f), 420, 1700.f, 4200.f, FVector2D(0.28f, 0.45f), -38.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FFFFFF")), 0.6f), 360, 1700.f, 4200.f, FVector2D(0.28f, 0.45f), -38.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#F472B6")), 0.6f, ZoogiUtil::Hex(TEXT("#F472B6")), 0.3f), 320, 1700.f, 4200.f, FVector2D(0.28f, 0.45f), -38.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B4A68C")), 0.95f), 120, 1750.f, 4200.f, FVector2D(0.35f, 0.9f), -60.f, Rng); // r15: field rocks
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C4A0E8")), 0.6f), 240, 1700.f, 4200.f, FVector2D(0.28f, 0.45f), -38.f, Rng, true); // r15: lavender flowers

	// r23: Meadow Arena Builder batch 1 (tall stands, boulders, trees, houses, hills, shrine gates, path ribbons).
	// Placement is world cm (PLACEMENT_batch1_cm.json, R 2015); the arena is actor-scaled by MeadowScale, so /A here.
	// Reachable pieces (stands, boulders, trees inside the kill zone) are breakable pieces with colliders; the rest is visual.
	bool bB1 = false;
	{
		const int32 NM = UE_ARRAY_COUNT(ZoogiMeadowB1::Meshes);
		TArray<TArray<UStaticMesh*>> Sets; Sets.SetNum(NM);
		for (const ZoogiMeadowB1::FPart& Pt : ZoogiMeadowB1::Parts)
			if (UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, Pt.Path)) Sets[Pt.Mesh].Add(SM);
		bB1 = Sets[0].Num() > 0;
		TMap<FName, UMaterialInterface*> BMats;
		auto BMat = [&](FName Slot) -> UMaterialInterface*
		{
			if (UMaterialInterface** F = BMats.Find(Slot)) return *F;
			const FString S = Slot.ToString();
			UMaterialInterface* M = nullptr;
			auto Sf = [&](const TCHAR* H, float R) { return ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(H), R); };
			if (S.Contains(TEXT("Glow"))) M = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#39E37A")), 2.5f);
			else if (S.Contains(TEXT("Foliage2"))) M = Sf(TEXT("#7DB24A"), 0.9f);
			else if (S.Contains(TEXT("Foliage"))) M = Sf(TEXT("#5E9E3C"), 0.9f);
			else if (S.Contains(TEXT("Bark"))) M = Sf(TEXT("#6B4A2E"), 0.9f);
			else if (S.Contains(TEXT("Moss"))) M = Sf(TEXT("#6E9A3E"), 0.95f);
			else if (S.Contains(TEXT("Rock"))) M = Sf(TEXT("#9C9480"), 0.9f);
			else if (S.Contains(TEXT("Grass"))) M = GrassTop;
			else if (S.Contains(TEXT("Path"))) M = Sf(TEXT("#C9B581"), 0.9f);
			else if (S.Contains(TEXT("Plaster"))) M = Sf(TEXT("#EFE4CC"), 0.85f);
			else if (S.Contains(TEXT("Roof"))) M = Sf(TEXT("#D0582F"), 0.6f);
			else if (S.Contains(TEXT("Trim"))) M = Sf(TEXT("#A8322A"), 0.6f);
			else if (S.Contains(TEXT("Wood"))) M = Sf(TEXT("#A87A4C"), 0.8f);
			else M = Sf(TEXT("#B4A88E"), 0.85f);
			BMats.Add(Slot, M);
			return M;
		};
		auto Dress = [&](UStaticMeshComponent* C, UStaticMesh* SM)
		{
			C->SetStaticMesh(SM);
			for (int32 k = 0; k < SM->GetStaticMaterials().Num(); ++k) C->SetMaterial(k, BMat(SM->GetStaticMaterials()[k].MaterialSlotName));
		};
		const float A = MeadowScale, KillR = 2700.f * A;
		int32 NPieces = 0, NVis = 0;
		for (const ZoogiMeadowB1::FPlace& Pl : ZoogiMeadowB1::Places)
		{
			const TArray<UStaticMesh*>& Set = Sets[Pl.Mesh];
			if (Set.Num() == 0) continue;
			const FString Name = ZoogiMeadowB1::Meshes[Pl.Mesh];
			const bool bStand = Name.Contains(TEXT("Stands")), bRock = Name.Contains(TEXT("Boulder")), bTree = Name.Contains(TEXT("Tree"));
			float RW = FMath::Sqrt(Pl.X * Pl.X + Pl.Y * Pl.Y);
			if (bRock)
			{
				const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Pl.Y, Pl.X));
				bool bBlocked = false;
				for (const ZoogiMeadowB1::FPlace& O : ZoogiMeadowB1::Places)
					if (FCString::Strstr(ZoogiMeadowB1::Meshes[O.Mesh], TEXT("Stands")) && FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, O.Yaw)) < 22.f) bBlocked = true;
				for (float Gy : GateYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(Ang, Gy)) < 12.f) bBlocked = true;
				if (bBlocked) continue; // r23: no boulder inside a stand arc or against a shrine
			}
			const float Push = (bRock && RW < 2240.f) ? 2240.f / RW : 1.f; // r23: boulders clear of the seat wall (outer r 2062)
			RW *= Push;
			const FVector L(Pl.X * Push / A, Pl.Y * Push / A, Pl.Z / A - 40.f);
			const float Sc = Pl.Scale / A;
			if (bStand || bRock || (bTree && RW < KillR))
			{
				AZoogiArenaPiece* P = SpawnPiece(L, Pl.Yaw, bStand ? TEXT("stand") : (bRock ? TEXT("boulder") : TEXT("tree")), bRock ? TEXT("Stone") : TEXT("Wood"), bStand ? 520.f : 160.f, false);
				if (!P) continue;
				TMap<int32, FBox> Bays; FBox All(ForceInit);
				for (UStaticMesh* SM : Set)
				{
					Dress(P->AddShape(TEXT("Cube"), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(Sc)), nullptr), SM);
					const FBox B = SM->GetBoundingBox();
					const FString MN = SM->GetName();
					if (bTree && !MN.Contains(TEXT("Trunk"))) continue; // trees: only the trunk is solid
					const int32 Bay = MN.Contains(TEXT("Bay0")) ? 0 : (MN.Contains(TEXT("Bay1")) ? 1 : (MN.Contains(TEXT("Bay2")) ? 2 : -1));
					if (bStand && Bay >= 0) Bays.FindOrAdd(Bay, FBox(ForceInit)) += B;
					All += B;
				}
				if (bStand) for (auto& KV : Bays)
				{
					bool bShared = false;
					for (const ZoogiMeadowB1::FPlace& O : ZoogiMeadowB1::Places)
						if (&O != &Pl && FCString::Strstr(ZoogiMeadowB1::Meshes[O.Mesh], TEXT("Stands")))
						{
							const float D = FMath::FindDeltaAngleDegrees(Pl.Yaw, O.Yaw);
							if ((KV.Key == 0 && D > 0.f && D < 29.f) || (KV.Key == 2 && D < 0.f && D > -29.f)) bShared = true;
						}
					if (bShared) continue; // r23: adjoining stands share this end; the neighbour's bay covers it
					// r23: each 10-deg bay gets a box along its own radial (arc centre 2140 behind the pivot), behind the seat wall.
					const float Rc = 2140.f, Ab = (1 - KV.Key) * 10.f, Rad = FMath::DegreesToRadians(Ab);
					const float D0 = 80.f, D1 = KV.Value.Max.X + 20.f, Wd = 2.f * Rc * FMath::Sin(FMath::DegreesToRadians(5.f)) * 0.45f;
					const FVector Ctr(FMath::Cos(Rad) * (Rc + (D0 + D1) * 0.5f) - Rc, FMath::Sin(Rad) * (Rc + (D0 + D1) * 0.5f), KV.Value.Max.Z * 0.5f);
					P->AddColliderBox(Ctr * Sc, FVector(D1 - D0, Wd, KV.Value.Max.Z) * Sc, FRotator(0.f, Ab, 0.f), TEXT("ZWall"), WallPM);
				}
				else if (All.IsValid) P->AddColliderBox(All.GetCenter() * Sc, All.GetSize() * Sc * 0.8f, FRotator::ZeroRotator, TEXT("ZWall"), bRock ? StonePM : WallPM);
				if (bRock && RW < 2015.f + 300.f) P->SetPathBlock(FVector2D(P->GetActorLocation()), All.GetExtent().X * Pl.Scale);
				P->RubbleKeepZ = bStand ? 60.f : 25.f;
				P->FinishBuild();
				++NPieces;
			}
			else
			{
				for (UStaticMesh* SM : Set)
				{
					UStaticMeshComponent* C = AddMesh(SM, nullptr, false, RW < 9000.f);
					Dress(C, SM);
					C->SetRelativeLocationAndRotation(L, FRotator(0.f, Pl.Yaw, 0.f));
					C->SetRelativeScale3D(FVector(Sc));
				}
				++NVis;
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("Meadow batch1: %s, %d pieces, %d visual"), bB1 ? TEXT("loaded") : TEXT("MISSING"), NPieces, NVis);
	}
	if (!bB1)
		for (int32 i = 0; i < NumStands; ++i)
			AddStands(StandCenters[i], StandArcs[i], StandRIn, -40.f, StandTiers, 72.f, 50.f, Seat, SeatAlt, SeatRiser, Stone);

	// Recovery Gate shrines (visual portal + return pad near south rim; Combo uses its own gate for Meadow).
	UMaterialInterface* Trim = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B8352B")), 0.7f);
	UMaterialInterface* Roof = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C8452E")), 0.55f);
	UMaterialInterface* Body = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#CDB89A")), 0.85f);
	UMaterialInterface* Panel = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#E8D7B8")), 0.8f);
	UMaterialInterface* Portal = ZoogiUtil::GlowMID(this, Green, 2.2f);
	// r19 (Evan 7:18/7:14): art-style buildings -- stone base, cream walls, dark wood frame, curved red tile roofs with
	// upturned eaves, green glowing doors -- and every building is a breakable piece.
	UMaterialInterface* PStone = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B4A284")), 0.9f);
	UMaterialInterface* PStoneDark = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8C7A62")), 0.9f);
	UMaterialInterface* DarkWood = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#4A3326")), 0.85f);
	UMaterialInterface* TileRoof = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C2432E")), 0.6f);
	UMaterialInterface* Cream = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#EADBC0")), 0.85f);
	auto HipRoof = [](FZoogiMeshBuilder& Rf, FZoogiMeshBuilder& Ud, float HX, float HY, float Z, float Hr, float Lift)
	{
		auto Ring = [](float hx, float hy, float z, float lift, float flare, TArray<FVector>& O)
		{
			const FVector2D L[8] = { {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1} };
			O.Reset();
			for (int32 k = 0; k < 8; ++k) { const bool c = (k & 1) == 1; O.Add(FVector(L[k].X * hx * (c ? flare : 1.f), L[k].Y * hy * (c ? flare : 1.f), z + (c ? lift : 0.f))); }
		};
		TArray<FVector> R0, R1, R2, RU;
		Ring(HX, HY, Z, Lift, 1.1f, R0);                          // upturned eave corners
		Ring(HX * 0.66f, HY * 0.78f, Z + Hr * 0.36f, Lift * 0.2f, 1.03f, R1); // concave sweep
		Ring(HX * 0.1f, HY * 0.6f, Z + Hr, 0.f, 1.f, R2);           // ridge
		Ring(HX * 0.7f, HY * 0.75f, Z - 10.f, 0.f, 1.f, RU);
		auto Nrm = [](const FVector& A, const FVector& B) { FVector O = (A + B) * 0.5f; O.Z = 0.f; return O.GetSafeNormal() + FVector(0, 0, 0.8f); };
		for (int32 k = 0; k < 8; ++k)
		{
			const int32 K1 = (k + 1) % 8;
			Rf.Quad(R0[k], R0[K1], R1[K1], R1[k], Nrm(R0[k], R0[K1]));
			Rf.Quad(R1[k], R1[K1], R2[K1], R2[k], Nrm(R1[k], R1[K1]));
			Ud.Quad(R0[k], R0[K1], RU[K1], RU[k], FVector(0, 0, -1));
			Ud.Quad(R0[k], R0[K1], R0[K1] - FVector(0, 0, 14.f), R0[k] - FVector(0, 0, 14.f), Nrm(R0[k], R0[K1]) - FVector(0, 0, 0.8f));
			Rf.FlatTri(FVector(0, 0, Z + Hr), R2[k], R2[K1], FVector::UpVector);
		}
	};
	// Hall in piece-local space (front = -X, faces the arena).
	auto Hall = [&](AZoogiArenaPiece* P, float HX, float HY, float WallH, bool bTwoTier, int32 Doors)
	{
		const FRotator R0r = FRotator::ZeroRotator;
		P->AddBox(FVector(0, 0, 18.f), FVector(2.f * HX + 90.f, 2.f * HY + 90.f, 36.f), R0r, PStone);
		P->AddBox(FVector(0, 0, 46.f), FVector(2.f * HX + 46.f, 2.f * HY + 46.f, 20.f), R0r, PStoneDark);
		const float Z0 = 56.f;
		P->AddBox(FVector(0, 0, Z0 + WallH * 0.5f), FVector(2.f * HX, 2.f * HY, WallH), R0r, Cream);
		const int32 NP = FMath::Max(2, FMath::RoundToInt(2.f * HY / 170.f)) + 1;
		for (int32 i = 0; i < NP; ++i)
		{
			const float Y = -HY + i * (2.f * HY / (NP - 1));
			for (int32 sx = -1; sx <= 1; sx += 2) P->AddBox(FVector(sx * (HX + 8.f), Y, Z0 + WallH * 0.5f), FVector(22.f, 22.f, WallH), R0r, i == 0 || i == NP - 1 ? DarkWood : Trim);
		}
		for (int32 sy = -1; sy <= 1; sy += 2) P->AddBox(FVector(0, sy * (HY + 8.f), Z0 + WallH * 0.5f), FVector(2.f * HX, 18.f, WallH), R0r, DarkWood);
		for (int32 sx = -1; sx <= 1; sx += 2)
		{
			P->AddBox(FVector(sx * (HX + 12.f), 0, Z0 + WallH - 12.f), FVector(18.f, 2.f * HY + 40.f, 24.f), R0r, DarkWood);
			P->AddBox(FVector(sx * (HX + 12.f), 0, Z0 + 12.f), FVector(16.f, 2.f * HY + 20.f, 16.f), R0r, DarkWood);
		}
		const float DoorW = FMath::Min(120.f, 2.f * HY / (Doors + 1)), DoorH = WallH * 0.72f;
		for (int32 d = 0; d < Doors; ++d)
		{
			const float Y = (d - (Doors - 1) * 0.5f) * (2.f * HY / FMath::Max(1, Doors));
			P->AddBox(FVector(-HX - 4.f, Y, Z0 + DoorH * 0.5f + 6.f), FVector(6.f, DoorW + 26.f, DoorH + 20.f), R0r, DarkWood);
			P->MarkGlow(P->AddBox(FVector(-HX - 9.f, Y, Z0 + DoorH * 0.5f + 4.f), FVector(4.f, DoorW, DoorH), R0r, Portal, false));
			P->AddBox(FVector(-HX - 12.f, Y, Z0 + DoorH * 0.5f + 4.f), FVector(3.f, 6.f, DoorH), R0r, DarkWood);
		}
		// lattice windows between the doors
		for (int32 w = 0; w <= Doors; ++w)
		{
			const float Y = (w - Doors * 0.5f) * (2.f * HY / FMath::Max(1, Doors));
			if (FMath::Abs(Y) > HY - 50.f) continue;
			P->AddBox(FVector(-HX - 3.f, Y, Z0 + WallH * 0.62f), FVector(4.f, 60.f, 56.f), R0r, Panel);
			for (int32 l = -1; l <= 1; ++l) P->AddBox(FVector(-HX - 6.f, Y + l * 18.f, Z0 + WallH * 0.62f), FVector(3.f, 4.f, 56.f), R0r, DarkWood);
		}
		auto Roofs = [&](float hx, float hy, float Z, float Hr, float Lift)
		{
			FZoogiMeshBuilder Rf, Ud;
			HipRoof(Rf, Ud, hx, hy, Z, Hr, Lift);
			P->AddProc(Rf, TileRoof); P->AddProc(Ud, DarkWood);
			P->AddBox(FVector(0, 0, Z + Hr + 8.f), FVector(24.f, 2.f * hy * 0.6f + 30.f, 22.f), R0r, DarkWood); // ridge beam
			for (int32 sy = -1; sy <= 1; sy += 2) P->AddBox(FVector(0, sy * (hy * 0.6f + 22.f), Z + Hr + 26.f), FVector(20.f, 30.f, 46.f), FRotator(0.f, 0.f, sy * 25.f), DarkWood); // ridge horns
		};
		const float RoofZ = Z0 + WallH + 4.f;
		Roofs(HX + 110.f, HY + 120.f, RoofZ, FMath::Max(90.f, HX * 0.75f), 46.f);
		if (bTwoTier)
		{
			const float Zu = RoofZ + HX * 0.36f;
			P->AddBox(FVector(0, 0, Zu + WallH * 0.25f), FVector(HX * 1.1f, HY * 1.3f, WallH * 0.5f), R0r, Cream);
			for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sy = -1; sy <= 1; sy += 2) P->AddBox(FVector(sx * HX * 0.55f, sy * HY * 0.65f, Zu + WallH * 0.25f), FVector(18.f, 18.f, WallH * 0.5f), R0r, Trim);
			Roofs(HX * 0.55f + 80.f, HY * 0.65f + 90.f, Zu + WallH * 0.5f, FMath::Max(70.f, HX * 0.6f), 36.f);
		}
		const float TopZ = RoofZ + (bTwoTier ? HX * 0.36f + WallH * 0.5f + HX * 0.6f : HX * 0.75f);
		P->AddColliderBox(FVector(0, 0, TopZ * 0.5f), FVector(2.f * HX + 90.f, 2.f * HY + 90.f, TopZ), R0r, TEXT("ZWall"), StonePM);
	};
	auto Adopt = [&](int32 Mark, const FVector& Base, float Yaw, const TCHAR* Kind, float HP) -> AZoogiArenaPiece*
	{
		AZoogiArenaPiece* P = SpawnPiece(Base, Yaw, Kind, TEXT("Stone"), HP, false);
		if (!P) return nullptr;
		for (int32 i = Mark; i < Built.Num(); ++i) if (UPrimitiveComponent* C = Cast<UPrimitiveComponent>(Built[i].Get())) P->AdoptPart(C);
		P->RubbleKeepZ = 40.f;
		return P;
	};
	auto HallPiece = [&](const FVector& Base, float Yaw, const TCHAR* Kind, float HX, float HY, float WallH, bool bTwo, int32 Doors) -> AZoogiArenaPiece*
	{
		AZoogiArenaPiece* P = SpawnPiece(Base, Yaw, Kind, TEXT("Wood"), 220.f, false);
		if (!P) return nullptr;
		// r22: A&P kit hall/shrine when imported (front faces the arena = local -X), hand-built fallback.
		const bool bGate = FCString::Strcmp(Kind, TEXT("gatehouse")) == 0;
		const float Sc = bGate ? (2.f * HY + 40.f) / 1125.f : (2.f * HY + 200.f) / 507.f;
		if (KitPart(P, bGate ? TEXT("SM_MeadowGatehouse") : TEXT("SM_MeadowShrine"), FVector::ZeroVector, 0.f, Sc))
		{
			const float HxK = (bGate ? 390.f : 254.f) * Sc, HyK = (bGate ? 563.f : 254.f) * Sc, HzK = (bGate ? 1205.f : 520.f) * Sc;
			P->AddColliderBox(FVector(0, 0, HzK * 0.5f), FVector(2.f * HxK, 2.f * HyK, HzK), FRotator::ZeroRotator, TEXT("ZWall"), StonePM);
		}
		else Hall(P, HX, HY, WallH, bTwo, Doors);
		P->RubbleKeepZ = 45.f;
		P->FinishBuild();
		return P;
	};
	for (int32 i = 0; i < 2; ++i)
	{
		const float A = FMath::DegreesToRadians(GateYaw[i]);
		const FVector Base(FMath::Cos(A) * GateR, FMath::Sin(A) * GateR, -40.f);
		HallPiece(Base, GateYaw[i], TEXT("shrine"), 95.f, 120.f, 190.f, false, 1); // r19: art shrine (red roof, green door), breakable
		AddCylinder(FVector2D(Base.X * 0.92f, Base.Y * 0.92f), 70.f, 2.f, 2.f, ZoogiUtil::GlowMID(this, Green, 1.4f), false);
	}

	// r5: the two small red shrines behind the NE stand (art), at the stand's outer edge.
	for (int32 i = 0; i < 2; ++i)
	{
		const float Yw = i ? 353.f : 338.f; // r23: clear of the gatehouse
		HallPiece(Pol(2420.f, Yw, -40.f), Yw, TEXT("shrine"), 80.f, 100.f, 170.f, false, 1);
	}

	// r4: crisp layered mountains (3 ridges, near green -> far blue) with tall waterfalls, replacing the pale haze.
	{
		UStaticMesh* Cone = ZoogiUtil::BasicShape(TEXT("Cone"));
		FRandomStream MR(4242);
		struct FRidge { float R; float HMin, HMax; FLinearColor A, B; int32 N; };
		const FRidge Ridges[3] = {
			{ 17000.f, 700.f, 1300.f, ZoogiUtil::Hex(TEXT("#98AE52")), ZoogiUtil::Hex(TEXT("#A8BA62")), 44 },   // r18: low near downs (horizon open)
			{ 28000.f, 3000.f, 5500.f, ZoogiUtil::Hex(TEXT("#968A78")), ZoogiUtil::Hex(TEXT("#AA9E8A")), 40 },  // r18: rocky mountains
			{ 46000.f, 5000.f, 8200.f, ZoogiUtil::Hex(TEXT("#A9C0D6")), ZoogiUtil::Hex(TEXT("#BDD0E2")), 48 } }; // far blue peaks
		UMaterialInterface* Snow = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#EEF4F8")), 0.8f);
		for (int32 r = 0; r < 3; ++r)
		{
			const FRidge& Rg = Ridges[r];
			UMaterialInterface* MA = ZoogiUtil::SurfaceMID(this, Rg.A, 0.95f);
			UMaterialInterface* MB = ZoogiUtil::SurfaceMID(this, Rg.B, 0.95f);
			for (int32 i = 0; i < Rg.N; ++i)
			{
				const float A = (i + MR.FRandRange(-0.3f, 0.3f)) * 2.f * PI / Rg.N;
				const float D = Rg.R * MR.FRandRange(0.92f, 1.1f);
				const float H = MR.FRandRange(Rg.HMin, Rg.HMax), W = (r == 0 ? 5000.f : H) * MR.FRandRange(2.2f, 3.2f);
				const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, -1000.f + H * 0.5f);
				// r5: near/mid ridges are rounded rolling hills (the art's soft green downs); only the far ridge is peaked.
				const bool bHill = r == 0;
				UStaticMeshComponent* C = AddMesh(bHill ? ZoogiUtil::BasicShape(TEXT("Sphere")) : Cone, (i & 1) ? MA : MB, false, false);
				C->SetRelativeLocationAndRotation(bHill ? FVector(P.X, P.Y, -1000.f) : P, FRotator(0.f, MR.FRandRange(0.f, 360.f), 0.f));
				C->SetRelativeScale3D(bHill ? FVector(W * 0.75f / 100.f, W * 0.75f / 100.f * MR.FRandRange(0.8f, 1.2f), 2.f * H / 100.f) : FVector(W / 100.f, W / 100.f * MR.FRandRange(0.8f, 1.2f), H / 100.f));
				if (r >= 1 && H > (r == 1 ? 4800.f : 7000.f)) // snow caps on the tallest far peaks
				{
					UStaticMeshComponent* Cap = AddMesh(Cone, Snow, false, false);
					Cap->SetRelativeLocation(P + FVector(0, 0, H * 0.35f));
					Cap->SetRelativeScale3D(FVector(W * 0.3f / 100.f, W * 0.3f / 100.f, H * 0.3f / 100.f));
				}
			}
		}
		// r5: three big waterfalls where the art has them (north-west, north, north-east valley), each falling down a
		// dark cliff face so it reads against the hills.
		UMaterialInterface* CliffDark = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#B0A08A")), ZoogiUtil::Hex(TEXT("#9A8A74")), 0.004f, 0.95f); // r11: pale distant cliffs
		// r6: valley floor under the island (below FallOutZ, no collision) so the view reads as rolling meadow, not void.
		AddCylinder(FVector2D::ZeroVector, 40000.f, 60.f, -1000.f, ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#9CAF6A")), ZoogiUtil::Hex(TEXT("#AEB878")), 0.0005f, 0.95f), false)->SetCastShadow(false);
		// r12: meadow plain at terrace level out to the hills (visual only, no collision), cut at the 4 orb-exit gaps
		// so the drop channels continue as ravines and an orb still falls out of play.
		{
			FZoogiMeshBuilder Plain;
			for (int32 g = 0; g < 4; ++g)
				Plain.ArcSlab(2380.f, 14000.f, OrbExitGapYaw[g] + 11.f, OrbExitGapYaw[(g + 1) % 4] + (g == 3 ? 360.f : 0.f) - 11.f, -80.f, -45.f, 24);
			for (int32 g = 0; g < 4; ++g) // r13: the ravine only runs to r3600 (orbs are well below the plain by then)
				Plain.ArcSlab(3600.f, 14000.f, OrbExitGapYaw[g] - 11.5f, OrbExitGapYaw[g] + 11.5f, -80.f, -45.f, 4);
			AddFrozenProc(Plain, GrassTop, nullptr, false);
			FZoogiMeshBuilder Ravine;
			for (int32 g = 0; g < 4; ++g)
			{
				const float A0 = OrbExitGapYaw[g] - 11.f, A1 = OrbExitGapYaw[g] + 11.f;
				for (int32 sd = 0; sd < 2; ++sd)
				{
					const float A = FMath::DegreesToRadians(sd ? A1 : A0);
					const FVector Dn(FMath::Cos(A), FMath::Sin(A), 0.f);
					Ravine.Quad(Dn * 2380.f + FVector(0, 0, -45.f), Dn * 3600.f + FVector(0, 0, -45.f), Dn * 3600.f + FVector(0, 0, -1000.f), Dn * 2380.f + FVector(0, 0, -1000.f), FVector(-Dn.Y, Dn.X, 0.f) * (sd ? -1.f : 1.f));
				}
			}
			for (int32 g = 0; g < 4; ++g)
				Ravine.ArcSlab(3560.f, 3600.f, OrbExitGapYaw[g] - 11.f, OrbExitGapYaw[g] + 11.f, -1000.f, -46.f, 4);
			AddFrozenProc(Ravine, GrassTop, nullptr, false); // r15: grassy banks, no hard cliff cut
		}
		// r6: three waterfalls spilling off grassy cliff mesas in the valley (art: NW, N, NE), facing the arena.
		const float FallYaw[5] = { 222.f, 244.f, 268.f, 292.f, 316.f }, FallR[5] = { 10500.f, 9200.f, 12500.f, 9600.f, 11000.f }, MesaTop[5] = { 1100.f, 800.f, 1500.f, 900.f, 1200.f }; // r18: 5 tall falls
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector2D M(Pol(FallR[i] + 900.f, FallYaw[i]));
			{ // r21: rounded grassy hill with a rock face, not a drum
				UStaticMeshComponent* Hl = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), GrassTop, false, false);
				Hl->SetRelativeLocation(FVector(M.X, M.Y, -1000.f));
				Hl->SetRelativeScale3D(FVector(30.f, 34.f, 2.f * (MesaTop[i] + 1000.f) / 100.f));
				UStaticMeshComponent* Rk = AddMesh(ZoogiUtil::BasicShape(TEXT("Sphere")), CliffDark, false, false);
				Rk->SetRelativeLocation(FVector(M.X, M.Y, -1000.f) - FVector(Pol(1.f, FallYaw[i])) * 260.f);
				Rk->SetRelativeScale3D(FVector(26.f, 18.f, 1.9f * (MesaTop[i] + 1000.f) / 100.f));
				Rk->SetRelativeRotation(FRotator(0.f, FallYaw[i], 0.f));
			}
			const FVector Lip = Pol(FallR[i] + 160.f, FallYaw[i], -1000.f + 0.86f * (MesaTop[i] + 1000.f));
			AddWaterfall(Lip, FallYaw[i] + 180.f, 300.f, Lip.Z + 1000.f, 80.f);
			AddWaterfall(Lip + Pol(1.f, FallYaw[i] + 90.f) * 300.f, FallYaw[i] + 180.f, 160.f, Lip.Z + 1000.f, 60.f);
		}
	}

	// r5: landmark buildings where the art has them, on hills past the stands (out of reach: solid scenery).
	{
		UMaterialInterface* HillTop = GrassTop;
		UMaterialInterface* Red = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A8302A")), 0.65f);
		UMaterialInterface* RoofGrey = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B8402C")), 0.6f);
		// 5-storey pagoda tower on the NW hill with two small halls (art: top-left cluster).
		{
			const FVector2D P(Pol(2550.f, 226.f, 0.f));
			AddCylinder(P, 720.f, 900.f, 300.f, HillTop, false);
			AddUnderCone(P, 720.f, 1500.f, -600.f, Cliff);
			int32 Mk = Built.Num();
			UStaticMesh* KitPag = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/MeadowBuildings/SM_MeadowPagoda5.SM_MeadowPagoda5"));
			if (!KitPag) AddPagoda(FVector(P.X, P.Y, 300.f), 46.f, 6, 150.f, Cream, DarkWood, TileRoof, DarkWood, PStone, Bronze);
			if (AZoogiArenaPiece* TP = Adopt(Mk, FVector(P.X, P.Y, 300.f), 46.f, TEXT("pagoda"), 300.f))
			{
				if (KitPag) KitPart(TP, TEXT("SM_MeadowPagoda5"), FVector::ZeroVector, 180.f, 0.85f); // r22: A&P 5-tier pagoda
				TP->AddColliderBox(FVector(0, 0, 450.f), FVector(320.f, 320.f, 900.f), FRotator::ZeroRotator, TEXT("ZWall"), StonePM); TP->RubbleKeepZ = 120.f; TP->FinishBuild(); }
			for (int32 j = 0; j < 2; ++j)
			{
				const FVector2D Q(Pol(2800.f, j ? 232.f : 210.f, 0.f));
				AddCylinder(Q, 330.f, 700.f, 180.f, HillTop, false);
				const int32 Mq = Built.Num();
				AddPagoda(FVector(Q.X, Q.Y, 180.f), 46.f, 2, 100.f, Cream, DarkWood, TileRoof, DarkWood, PStone, Bronze);
				if (AZoogiArenaPiece* QP = Adopt(Mq, FVector(Q.X, Q.Y, 180.f), 46.f, TEXT("pagoda"), 220.f))
				{ QP->AddColliderBox(FVector(0, 0, 200.f), FVector(250.f, 250.f, 400.f), FRotator::ZeroRotator, TEXT("ZWall"), StonePM); QP->FinishBuild(); }
			}
		}
		// Wide gatehouse on the NE hill (art: top-right): long hall, red colonnade, broad grey hip roof, gate doors.
		{
			const float Yaw = 314.f;
			const FVector2D P(Pol(2800.f, Yaw, 0.f));
			const FRotator Rot(0.f, Yaw, 0.f); // local X = radial (outward), Y = tangential (the long side)
			auto At = [&](float X, float Y, float Z) { const FVector L = Rot.RotateVector(FVector(X, Y, 0.f)); return FVector(P.X + L.X, P.Y + L.Y, Z); };
			AddCylinder(P, 950.f, 1100.f, 200.f, HillTop, false);
			AddUnderCone(P, 950.f, 1400.f, -900.f, Cliff);
			for (int32 s2 = 0; s2 < 3; ++s2)
				AddBoxR(At(0, 0, 220.f + s2 * 36.f), FVector(720.f - s2 * 60.f, 1700.f - s2 * 80.f, 36.f), Rot, Stone);
			HallPiece(FVector(P.X, P.Y, 310.f), Yaw, TEXT("gatehouse"), 240.f, 640.f, 300.f, true, 3); // r19: art gatehouse hall, breakable
			(void)Red; (void)RoofGrey; (void)At;
		}
		// Stone golem statue behind the north-west quarter-pipe (art).
		{
			UMaterialInterface* GolemStone = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8E8A78")), 0.9f);
			UMaterialInterface* Moss = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#5E9A3A")), 0.9f);
			const int32 GolemMark = Built.Num();
			const float GK = 1.7f; // r18: art golem towers over the stand
			const FVector G = Pol(2750.f, 246.f, -40.f);
			const FRotator Rg(0.f, 246.f + 180.f, 0.f);
			auto GA2 = [&](float X, float Y, float Z) { return G + Rg.RotateVector(FVector(X, Y, Z) * GK); };
			for (int32 s2 = -1; s2 <= 1; s2 += 2) AddBoxR(GA2(0, s2 * 70.f, 110.f), FVector(90.f, 80.f, 220.f) * GK, Rg, GolemStone);
			AddBoxR(GA2(0, 0, 330.f), FVector(150.f, 260.f, 220.f) * GK, Rg, GolemStone);
			AddBoxR(GA2(0, 0, 330.f + 140.f), FVector(160.f, 280.f, 40.f) * GK, Rg, Moss);
			for (int32 s2 = -1; s2 <= 1; s2 += 2) AddBoxR(GA2(10.f, s2 * 175.f, 280.f), FVector(80.f, 70.f, 260.f) * GK, FRotator(0.f, 246.f + 180.f, s2 * 8.f), GolemStone);
			AddBoxR(GA2(-10.f, 0, 520.f), FVector(110.f, 120.f, 100.f) * GK, Rg, GolemStone);
			// r13: rounder silhouette like the art's mossy golem: boulder shoulders, fists, head and a moss cap.
			UStaticMesh* Sph = ZoogiUtil::BasicShape(TEXT("Sphere"));
			auto Blob = [&](const FVector& At, const FVector& Sc, UMaterialInterface* Mt) { UStaticMeshComponent* Bc = AddMesh(Sph, Mt, false, true); Bc->SetRelativeLocationAndRotation(At, Rg); Bc->SetRelativeScale3D(Sc * GK); };
			for (int32 s2 = -1; s2 <= 1; s2 += 2)
			{
				Blob(GA2(0, s2 * 165.f, 420.f), FVector(1.5f, 1.4f, 1.3f), GolemStone);
				Blob(GA2(-20.f, s2 * 190.f, 150.f), FVector(1.2f, 1.1f, 1.1f), GolemStone);
				Blob(GA2(0, s2 * 165.f, 470.f), FVector(1.2f, 1.1f, 0.5f), Moss);
			}
			Blob(GA2(-10.f, 0, 560.f), FVector(1.3f, 1.3f, 1.1f), GolemStone);
			Blob(GA2(-10.f, 0, 610.f), FVector(1.0f, 1.0f, 0.35f), Moss);
			for (int32 m2 = 0; m2 < 6; ++m2) Blob(GA2(-60.f + 20.f * (m2 % 3), -120.f + 48.f * m2, 330.f + 60.f * (m2 & 1)), FVector(0.45f, 0.45f, 0.3f), Moss); // r18: moss patches
			AddBoxR(GA2(0, 0, -10.f), FVector(320.f, 520.f, 40.f) * GK, Rg, PStone); // plinth
			UStaticMeshComponent* Eye = AddBoxR(GA2(-70.f, 0, 560.f), FVector(4.f, 70.f, 12.f) * GK, Rg, Portal, false);
			(void)Eye;
			if (AZoogiArenaPiece* GP = Adopt(GolemMark, G, 246.f + 180.f, TEXT("golem"), 320.f))
			{ GP->AddColliderBox(FVector(0, 0, 330.f * GK), FVector(260.f, 460.f, 660.f) * GK, FRotator::ZeroRotator, TEXT("ZWall"), StonePM); GP->RubbleKeepZ = 90.f; GP->FinishBuild(); }
		}
	}

	UStaticMesh* Oak = ZoogiUtil::LoadMesh(TEXT("/Game/Env/OakTree/SM_OakTree.SM_OakTree"));
	UStaticMesh* Pine = ZoogiUtil::LoadMesh(TEXT("/Game/Env/PineTree/SM_PineTree.SM_PineTree"));
	const FLinearColor GA = ZoogiUtil::Hex(TEXT("#93A644")), GB = ZoogiUtil::Hex(TEXT("#A6B656")), RockC = ZoogiUtil::Hex(TEXT("#5E4C3D"));
	// r11: no floating islets (the art has none).
	(void)Oak; (void)Pine; (void)GA; (void)GB; (void)RockC;

	{
		FActorSpawnParameters SP; SP.Owner = this;
		Scars = GetWorld()->SpawnActor<AZoogiFloorScars>(AZoogiFloorScars::StaticClass(), FTransform::Identity, SP);
		if (Scars)
		{
			Scars->Init((ZoogiConst::KnockoffRadius - 60.f) * MeadowScale);
			Scars->SlabMat = Stone; // SpawnCraterSlabs hook uses sandstone
			// A&P CraterSlab kit (SM_MeadowCraterSlab_A..D); falls back to the placeholder ring if not imported.
			for (const TCHAR* Nm : { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") })
				if (UStaticMesh* SM = ZoogiUtil::LoadMesh(*FString::Printf(TEXT("/Game/Env/Arenas/Meadow/SM_MeadowCraterSlab_%s.SM_MeadowCraterSlab_%s"), Nm, Nm)))
					Scars->SlabMeshes.Add(SM);
			Scars->SlabSideMat = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#4A3A2C")), 0.95f, ZoogiUtil::Hex(TEXT("#4CD964")), 0.15f);
			Scars->NoSlabYaw = { OrbExitGapYaw[0], OrbExitGapYaw[1], OrbExitGapYaw[2], OrbExitGapYaw[3] };
			Scars->NoSlabHalfDeg = OrbExitGapHalfDeg;
			Scars->SetTheme(TEXT("Meadow"), ZoogiUtil::Hex(TEXT("#4CD964"))); // r3: earthy brown/green cracks + dark crater (A&P Meadow decals)
			UE_LOG(LogZoogi, Log, TEXT("Meadow crater slab kit: %d meshes"), Scars->SlabMeshes.Num());
		}
	}
	if (Forming && FloorMID) Forming->AddParamRamp(FloorMID, TEXT("GlowStrength"), 0.f, SealGlow, 2.6f, 0.5f);
	// -ZoogiArenaPlayTest: orb out through each orb-exit gap (must count as ORB OUT), a control shot into the seat wall
	// (must stay in), normal Zoogi shots that must crack the seat wall and a bench, and ramp roll-and-launch checks.
	// r19: riding a ramp must never damage it (soft-bump threshold is 125 cm/s for everything else).
	for (AZoogiArenaPiece* Pc : Pieces)
		if (IsValid(Pc) && (Pc->GetKind() == TEXT("hub"))) Pc->HitSpeedMin = 900.f;
	if (AZoogiArenaPlayTest* PT = AZoogiArenaPlayTest::MaybeSpawn(this))
	{
		{ FZoogiArenaCheck C; C.Name = TEXT("piece_overlap"); C.Kind = FZoogiArenaCheck::PieceOverlap; C.Wait = 0.2f; PT->Checks.Add(C); }
		const float NormalShot = 0.55f * ZoogiConst::FullLaunchSpeed;
		for (int32 g = 0; g < 4; ++g)
		{
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("gap_%03.0f"), OrbExitGapYaw[g]); C.Kind = FZoogiArenaCheck::OrbOut;
			C.From = Pol(1250.f, OrbExitGapYaw[g]); C.Vel = FVector2D(Pol(1500.f, OrbExitGapYaw[g])); C.Wait = 3.0f;
			PT->Checks.Add(C);
		}
		{ FZoogiArenaCheck C; C.Name = TEXT("seatwall_control_225"); C.Kind = FZoogiArenaCheck::OrbStays; C.From = Pol(1250.f, 225.f); C.Vel = FVector2D(Pol(1500.f, 225.f)); C.Wait = 3.0f; PT->Checks.Add(C); }
		{ FZoogiArenaCheck C; C.Name = TEXT("seatwall_crack_225"); C.Kind = FZoogiArenaCheck::ZoogiDamages; C.From = Pol(1050.f, 225.f); C.Vel = FVector2D(Pol(NormalShot, 225.f));
		  C.PieceKind = TEXT("seatwall"); C.Near = Pol(FloorR + 18.f, 225.f); C.Radius = 350.f; C.Wait = 1.6f; PT->Checks.Add(C); }
		{ FZoogiArenaCheck C; C.Name = TEXT("bench_crack_67"); C.Kind = FZoogiArenaCheck::ZoogiDamages; C.From = Pol(1050.f, 67.f); C.Vel = FVector2D(Pol(-NormalShot, 67.f));
		  C.PieceKind = TEXT("bench"); C.Near = Pol(BenchR, 67.f); C.Radius = 150.f; C.Wait = 1.6f; PT->Checks.Add(C); }
		// r19: normal shots into each stand section must hit the (taller) seat wall, damage it and roll back onto the court.
		const float BounceYaw[6] = { 25.f, 155.f, 205.f, 231.f, 309.f, 335.f }; // r23: the batch-1 tall stands
		for (float Sy : BounceYaw)
		{
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("stand_bounce_%03.0f"), Sy); C.Kind = FZoogiArenaCheck::ZoogiBounceBack;
			C.From = Pol(1150.f, Sy); C.Vel = FVector2D(Pol(NormalShot, Sy));
			C.PieceKind = TEXT("seatwall"); C.Near = Pol(FloorR + 18.f, Sy); C.Radius = 350.f; C.Wait = 3.5f; PT->Checks.Add(C);
		}
		// r19: the arena is built at MeadowScale; checks are authored in layout units.
		for (FZoogiArenaCheck& C : PT->Checks)
		{
			C.From *= MeadowScale; C.Near *= MeadowScale;
			if (C.Kind == FZoogiArenaCheck::ZoogiDamages || C.Kind == FZoogiArenaCheck::ZoogiBounceBack) C.Radius *= MeadowScale;
		}
	}
	{
		// r9: orb ring clearance vs spiral band/disc, pillars, benches (edge-to-edge, cm).
		float MinSp = 1e9f, MinPi = 1e9f, MinBe = 1e9f;
		for (int32 i = 0; i < MeadowOrbs; ++i)
		{
			const FVector O = Pol(OrbRingR, OrbRingYaw + i * 360.f / MeadowOrbs);
			MinSp = FMath::Min(MinSp, O.Size2D() - 270.f - ZoogiConst::OrbRadius); // r20: hub dais
			for (float Y : PillarYaw) MinPi = FMath::Min(MinPi, FVector::Dist2D(O, Pol(PillarR, Y)) - 70.f * PillarS - ZoogiConst::OrbRadius);
			for (float Y : BenchYaw) MinBe = FMath::Min(MinBe, FVector::Dist2D(O, Pol(BenchR, Y)) - 65.f - ZoogiConst::OrbRadius);
		}
		UE_LOG(LogZoogi, Log, TEXT("Meadow orb ring r%.0f +%.0f deg: clearance hub %.0f, pillars %.0f, benches %.0f cm %s"), OrbRingR, OrbRingYaw, MinSp, MinPi, MinBe,
			(MinSp > 0.f && MinPi > 0.f && MinBe > 0.f) ? TEXT("CLEAR") : TEXT("OVERLAP"));
	}
	UE_LOG(LogZoogi, Log, TEXT("Meadow Arena built: %d pieces, %d forming, %d extra orbs, orb-exit gaps %.0f cm @ 0/90/180/270 (no Aura)"),
		Pieces.Num(), Forming ? Forming->Num() : 0, ExtraOrbSpots.Num(), OrbExitGapWidthCm);
}
