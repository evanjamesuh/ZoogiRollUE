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

void AZoogiArena::BuildMeadow()
{
	using namespace ZoogiMeadow; // function scope: unity builds also pull in ZoogiFrozen's WallR/GateYaw/...
	const FLinearColor Green = ZoogiUtil::Hex(TEXT("#39E37A"));
	constexpr float SealGlow = 9.0f; // r4: bold seal (T_MeadowGlow rebaked: thick core + soft halo)
	const FLinearColor Soil = ZoogiUtil::Hex(TEXT("#9C8B73"));
	const FLinearColor Wood = ZoogiUtil::Hex(TEXT("#8B5A2B"));
	const FLinearColor WoodAlt = ZoogiUtil::Hex(TEXT("#A06A38"));
	const FLinearColor Riser = ZoogiUtil::Hex(TEXT("#6E4A28"));
	const FLinearColor SeatStone = ZoogiUtil::Hex(TEXT("#B9A889"));
	const FLinearColor RockA = ZoogiUtil::Hex(TEXT("#B9A889"));
	const FLinearColor RockB = ZoogiUtil::Hex(TEXT("#8E8063"));

	UMaterialInterface* FloorMat = KitFloorMaterial(TEXT("Meadow"), ZoogiUtil::Hex(TEXT("#12E04A")), SealGlow, 0.98f);
	BuildIsland(ZoogiUtil::Hex(TEXT("#3F8A2C")), ZoogiUtil::Hex(TEXT("#55A336")), Soil,
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
	UMaterialInterface* GrassTop = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#3F8A2C")), ZoogiUtil::Hex(TEXT("#55A336")), 0.004f, 0.92f);
	UMaterialInterface* Cliff = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#6E5B4A")), ZoogiUtil::Hex(TEXT("#4A3B30")), 0.01f, 0.95f);
	UMaterialInterface* GlowTop = ZoogiUtil::GlowMID(this, Green, 1.8f);
	UPhysicalMaterial* PillarPM = ZoogiPhysics::MakeMaterial(this, 0.25f, 0.6f, false);
	UPhysicalMaterial* WallPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.85f, false);
	UPhysicalMaterial* StonePM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.35f, false);

	AddHubCore(FVector(0, 0, 290.f), 55.f, Crystal);

	// 4 Resonance pillars (unbreakable), rise at match start.
	for (int32 k = 0; k < 4; ++k)
	{
		const float Yaw = PillarYaw[k];
		const float Rad = FMath::DegreesToRadians(Yaw);
		const FVector Loc(FMath::Cos(Rad) * PillarR, FMath::Sin(Rad) * PillarR, 0.f);
		AZoogiArenaPiece* P = SpawnPiece(Loc, Yaw, *FString::Printf(TEXT("pillar_%d"), k), TEXT("Stone"), 1000.f, true);
		if (!P) continue;
		const float S = PillarS;
		P->AddBox(FVector(0, 0, 18.f), FVector(150.f * S, 150.f * S, 36.f), FRotator::ZeroRotator, StoneDark);
		P->AddBox(FVector(0, 0, 50.f), FVector(118.f * S, 118.f * S, 30.f), FRotator::ZeroRotator, Stone);
		FZoogiMeshBuilder Shaft;
		Shaft.Prism(FVector2D::ZeroVector, 4, 44.f * S, 34.f * S, 62.f, PillarH - 22.f, 45.f, 0.f, false);
		P->AddProc(Shaft, Stone);
		P->AddBox(FVector(0, 0, PillarH - 12.f), FVector(82.f * S, 82.f * S, 20.f), FRotator::ZeroRotator, Stone);
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
		UStaticMeshComponent* OrbM = P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 100.f), FVector(1.05f * S, 1.05f * S, 1.15f * S)), ZoogiUtil::GlowMID(this, Green, 2.4f), false);
		P->MarkGlow(OrbM);
		P->AddColliderBox(FVector(0, 0, 33.f), FVector(140.f * S, 140.f * S, 66.f), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
		P->AddColliderBox(FVector(0, 0, PillarH * 0.5f), FVector(84.f * S, 84.f * S, PillarH), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(PillarH + 120.f)), 1.30f + 0.10f * k, 1.0f, 14.f);
		// Arena Orb apron in front of each pillar (toward centre), clear of the seat-wall gaps.
		const float ApronR = PillarR - 180.f;
		ExtraOrbSpots.Add(FVector(FMath::Cos(Rad) * ApronR, FMath::Sin(Rad) * ApronR, ZoogiConst::OrbRadius + 1.f));
	}

	// Stadium-seat wall with 4 orb-exit gaps (MEADOW ONLY). No VFX/debris in gaps.
	{
		const float R = FloorR + 18.f;
		const int32 Segs = 48;
		for (int32 i = 0; i < Segs; ++i)
		{
			const float Am = i * (360.f / Segs) + 180.f / Segs;
			if (NearOrbExitGap(Am, 0.5f)) continue;
			const float Rad = FMath::DegreesToRadians(Am);
			const float SegLen = R * FMath::DegreesToRadians(360.f / Segs) * 1.05f;
			const FVector Loc(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, 0.f);
			AZoogiArenaPiece* P = SpawnPiece(Loc, Am, TEXT("seatwall"), TEXT("Wood"), 110.f, false);
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

	// Curved ricochet walls (stone blocks + glow strip) — arcs avoid orb-exit gaps.
	for (int32 a = 0; a < 2; ++a)
	{
		const int32 N = 10;
		AZoogiArenaPiece* Wall = SpawnPiece(FVector::ZeroVector, 0.f, TEXT("stonewall"), TEXT("Stone"), 140.f, false);
		if (!Wall) continue;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = FMath::Lerp(WallA0[a], WallA1[a], (i + 0.5f) / N);
			if (NearOrbExitGap(A, 12.f)) continue;
			const float Rad = FMath::DegreesToRadians(A);
			const float SegLen = WallR * FMath::DegreesToRadians((WallA1[a] - WallA0[a]) / N) * 1.08f;
			const FVector C(FMath::Cos(Rad) * (WallR + 20.f), FMath::Sin(Rad) * (WallR + 20.f), WallH * 0.5f - 10.f);
			Wall->AddBox(C, FVector(40.f, SegLen, WallH + 20.f), FRotator(0.f, A, 0.f), Stone);
			Wall->AddBox(FVector(C.X, C.Y, WallH + 1.f), FVector(44.f, SegLen, 4.f), FRotator(0.f, A, 0.f), GlowTop, false);
			Wall->AddColliderBox(C, FVector(40.f, SegLen, WallH + 20.f), FRotator(0.f, A, 0.f), TEXT("ZWall"), WallPM);
		}
		Wall->FinishBuild();
		if (Forming) Forming->AddActor(Wall, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.0f + 0.1f * a, 0.7f);
	}

	// Launch ramp (wood) toward +X
	{
		AZoogiArenaPiece* P = SpawnPiece(FVector(RampCX, 0.f, 0.f), RampYaw, TEXT("woodramp"), TEXT("Wood"), 140.f, false);
		if (P)
		{
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(RampH, RampLen));
			P->AddBox(FVector(0.f, 0.f, RampH * 0.5f), FVector(RampLen, RampWid, 28.f), FRotator(Pitch, 0.f, 0.f), Seat);
			P->AddColliderBox(FVector(0.f, 0.f, RampH * 0.5f), FVector(RampLen + 20.f, RampWid, 36.f), FRotator(Pitch, 0.f, 0.f), TEXT("ZRamp"), StonePM);
			for (int32 k = 0; k < 3; ++k)
			{
				const float T = 0.25f + 0.25f * k;
				P->AddBox(FVector((T - 0.5f) * RampLen, 0.f, T * RampH + 2.f), FVector(12.f, RampWid * 0.7f, 2.f), FRotator(Pitch, 0.f, 0.f), GlowTop, false);
			}
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(RampH + 70.f)), 1.0f, 0.8f);
		}
	}

	// Raised stone platform
	{
		AZoogiArenaPiece* P = SpawnPiece(FVector(PlatX, PlatY, 0.f), 0.f, TEXT("platform"), TEXT("Stone"), 180.f, false);
		if (P)
		{
			FZoogiMeshBuilder Top;
			Top.Prism(FVector2D::ZeroVector, 8, PlatR, PlatR * 0.92f, PlatR, PlatH, 0.f, 0.f, true);
			P->AddProc(Top, Stone);
			P->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PlatH + 1.f), FVector((PlatR + 6.f) / 50.f, (PlatR + 6.f) / 50.f, 0.04f)), GlowTop, false);
			P->AddColliderBox(FVector(0, 0, PlatH * 0.5f), FVector(PlatR * 1.7f, PlatR * 1.7f, PlatH + 10.f), FRotator::ZeroRotator, TEXT("ZPlatform"), StonePM);
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(PlatH + 70.f)), 1.1f, 0.8f, 4.f);
		}
	}

	TArray<FVector4> Rim = {
		FVector4(32, 13.35, 1.15, 0.85), FVector4(70, 13.5, 1.05, 0.72), FVector4(152, 13.25, 1.2, 0.9),
		FVector4(196, 13.45, 1.1, 0.78), FVector4(250, 13.3, 1.22, 0.88), FVector4(332, 13.4, 1.08, 0.7),
		FVector4(111, 13.3, 0.8, 0.6), FVector4(291, 13.3, 0.8, 0.6) };
	BuildRim(Rim, RockA, ZoogiUtil::Hex(TEXT("#4F7A3A")), 0.55f, true);

	// r3: drop channels cut through the terrace at the 4 orb-exit gaps so an orb visibly drops off (no clipping).
	TerraceGapYaw = { OrbExitGapYaw[0], OrbExitGapYaw[1], OrbExitGapYaw[2], OrbExitGapYaw[3] };
	TerraceGapHalfDeg = OrbExitGapHalfDeg + 1.2f;
	AddTerrace(1680.f, 2400.f, -160.f, 1750.f, -100.f, GrassTop, Cliff, Cliff);
	FRandomStream Rng(77);
	UStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* ConeMesh = ZoogiUtil::BasicShape(TEXT("Cone"));
	ScatterInstances(ConeMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#7BC843")), 0.9f), 260, 1700.f, 2350.f, FVector2D(0.07f, 0.12f), 0.f, Rng);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FDE047")), 0.6f, ZoogiUtil::Hex(TEXT("#FDE047")), 0.4f), 140, 1700.f, 2350.f, FVector2D(0.08f, 0.12f), 4.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FFFFFF")), 0.6f), 120, 1700.f, 2350.f, FVector2D(0.07f, 0.11f), 4.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#F472B6")), 0.6f, ZoogiUtil::Hex(TEXT("#F472B6")), 0.3f), 100, 1700.f, 2350.f, FVector2D(0.08f, 0.12f), 4.f, Rng, true);

	for (int32 i = 0; i < NumStands; ++i)
		AddStands(StandCenters[i], StandArcDeg, StandRIn, -40.f, StandTiers, 110.f, 55.f, Seat, SeatAlt, SeatRiser, Stone);

	// Recovery Gate shrines (visual portal + return pad near south rim; Combo uses its own gate for Meadow).
	UMaterialInterface* Trim = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B8352B")), 0.7f);
	UMaterialInterface* Roof = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C8452E")), 0.55f);
	UMaterialInterface* Body = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#CDB89A")), 0.85f);
	UMaterialInterface* Panel = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#E8D7B8")), 0.8f);
	UMaterialInterface* Portal = ZoogiUtil::GlowMID(this, Green, 2.2f);
	for (int32 i = 0; i < 2; ++i)
	{
		const float A = FMath::DegreesToRadians(GateYaw[i]);
		const FVector Base(FMath::Cos(A) * GateR, FMath::Sin(A) * GateR, -40.f);
		AddShrineGate(Base, GateYaw[i] + 180.f, 1.0f, Stone, Trim, Bronze, Portal, Stone);
		AddCylinder(FVector2D(Base.X * 0.92f, Base.Y * 0.92f), 70.f, 2.f, 2.f, ZoogiUtil::GlowMID(this, Green, 1.4f), false);
	}

	const float PagodaYawA[5] = { 40.f, 100.f, 160.f, 300.f, 340.f };
	const float PagodaRA[5] = { 2800.f, 3200.f, 3000.f, 3100.f, 2700.f };
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = FMath::DegreesToRadians(PagodaYawA[i]);
		AddPagoda(FVector(FMath::Cos(A) * PagodaRA[i], FMath::Sin(A) * PagodaRA[i], -200.f - (i % 3) * 80.f),
			PagodaYawA[i] + 180.f, 2 + (i % 2), 70.f + i * 8.f, Body, Panel, Roof, Trim, Stone, Bronze);
	}

	// Waterfalls off the terrace (far, non-destructible)
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = 25.f + i * 55.f;
		const float Rad = FMath::DegreesToRadians(A);
		AddWaterfall(FVector(FMath::Cos(Rad) * 2100.f, FMath::Sin(Rad) * 2100.f, -80.f), A + 180.f, 140.f + (i % 3) * 30.f, 900.f + (i % 2) * 400.f, 40.f);
	}

	// r4: crisp layered mountains (3 ridges, near green -> far blue) with tall waterfalls, replacing the pale haze.
	{
		UStaticMesh* Cone = ZoogiUtil::BasicShape(TEXT("Cone"));
		FRandomStream MR(4242);
		struct FRidge { float R; float HMin, HMax; FLinearColor A, B; int32 N; };
		const FRidge Ridges[3] = {
			{ 20000.f, 1800.f, 3200.f, ZoogiUtil::Hex(TEXT("#3E7F3A")), ZoogiUtil::Hex(TEXT("#5C9A45")), 40 },
			{ 32000.f, 4200.f, 6200.f, ZoogiUtil::Hex(TEXT("#4F8C7A")), ZoogiUtil::Hex(TEXT("#6FA48E")), 44 },
			{ 50000.f, 7000.f, 10500.f, ZoogiUtil::Hex(TEXT("#86ABC8")), ZoogiUtil::Hex(TEXT("#AFC8DE")), 48 } };
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
				const float H = MR.FRandRange(Rg.HMin, Rg.HMax), W = H * MR.FRandRange(2.6f, 3.6f);
				const FVector P(FMath::Cos(A) * D, FMath::Sin(A) * D, -2400.f + H * 0.5f);
				UStaticMeshComponent* C = AddMesh(Cone, (i & 1) ? MA : MB, false, false);
				C->SetRelativeLocationAndRotation(P, FRotator(0.f, MR.FRandRange(0.f, 360.f), 0.f));
				C->SetRelativeScale3D(FVector(W / 100.f, W / 100.f * MR.FRandRange(0.8f, 1.2f), H / 100.f));
				if (r == 2 && H > 9000.f) // snow caps on the tallest far peaks
				{
					UStaticMeshComponent* Cap = AddMesh(Cone, Snow, false, false);
					Cap->SetRelativeLocation(P + FVector(0, 0, H * 0.35f));
					Cap->SetRelativeScale3D(FVector(W * 0.3f / 100.f, W * 0.3f / 100.f, H * 0.3f / 100.f));
				}
			}
		}
		// Visible waterfalls down the near ridge, facing the arena (hero/low34 views).
		const float FallYaw[6] = { 5.f, 28.f, 52.f, 128.f, 152.f, 200.f };
		for (int32 i = 0; i < 6; ++i)
		{
			const float Rad = FMath::DegreesToRadians(FallYaw[i]);
			const float R = 17500.f + (i % 2) * 900.f;
			AddWaterfall(FVector(FMath::Cos(Rad) * R, FMath::Sin(Rad) * R, -700.f + (i % 3) * 250.f), FallYaw[i] + 180.f, 520.f + (i % 2) * 160.f, 2400.f, 220.f);
		}
	}

	// r4: hand-built landmark buildings from the art. Both sit far past the knock-off rim on their own hills, where no
	// Zoogi or power can reach, so they are solid scenery (no collision, not breakable).
	{
		UMaterialInterface* HillTop = GrassTop;
		UMaterialInterface* Red = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A8302A")), 0.65f);
		// Tall 5-storey pagoda tower on a hill (art: back-left).
		{
			const float Rad = FMath::DegreesToRadians(-8.f);
			const FVector2D P(FMath::Cos(Rad) * 4300.f, FMath::Sin(Rad) * 4300.f);
			AddCylinder(P, 900.f, 1400.f, 120.f, HillTop, false);
			AddUnderCone(P, 900.f, 1600.f, -1280.f, Cliff);
			AddPagoda(FVector(P.X, P.Y, 120.f), 180.f - 8.f, 5, 150.f, Body, Panel, Roof, Trim, Stone, Bronze);
		}
		// Gatehouse hall (art: right): stepped plinth, red colonnade, double hip roof, glowing door.
		{
			const float Yaw = 48.f, Rad = FMath::DegreesToRadians(Yaw);
			const FVector2D P(FMath::Cos(Rad) * 4150.f, FMath::Sin(Rad) * 4150.f);
			const FRotator Rot(0.f, Yaw + 180.f, 0.f);
			AddCylinder(P, 1000.f, 1300.f, 0.f, HillTop, false);
			AddUnderCone(P, 1000.f, 1500.f, -1300.f, Cliff);
			for (int32 s = 0; s < 3; ++s)
				AddBoxR(FVector(P.X, P.Y, 20.f + s * 40.f), FVector(1100.f - s * 90.f, 760.f - s * 70.f, 40.f), Rot, Stone);
			const float Zc = 140.f;
			for (int32 c = 0; c < 6; ++c)
			{
				const FVector L = Rot.RotateVector(FVector(-360.f, -400.f + c * 160.f, 0.f));
				AddBoxR(FVector(P.X + L.X, P.Y + L.Y, Zc + 210.f), FVector(46.f, 46.f, 420.f), Rot, Red);
			}
			AddBoxR(FVector(P.X, P.Y, Zc + 210.f), FVector(560.f, 900.f, 420.f), Rot, Body);
			const FVector Door = Rot.RotateVector(FVector(-282.f, 0.f, 0.f));
			AddBoxR(FVector(P.X + Door.X, P.Y + Door.Y, Zc + 150.f), FVector(8.f, 200.f, 300.f), Rot, Portal);
			AddBoxR(FVector(P.X, P.Y, Zc + 440.f), FVector(880.f, 1120.f, 40.f), Rot, Trim);
			AddPagoda(FVector(P.X, P.Y, Zc + 400.f), Yaw + 180.f, 2, 230.f, Body, Panel, Roof, Trim, Stone, Bronze);
		}
	}

	UStaticMesh* Oak = ZoogiUtil::LoadMesh(TEXT("/Game/Env/OakTree/SM_OakTree.SM_OakTree"));
	UStaticMesh* Pine = ZoogiUtil::LoadMesh(TEXT("/Game/Env/PineTree/SM_PineTree.SM_PineTree"));
	const FLinearColor GA = ZoogiUtil::Hex(TEXT("#3F8A2C")), GB = ZoogiUtil::Hex(TEXT("#55A336")), RockC = ZoogiUtil::Hex(TEXT("#5E4C3D"));
	for (int32 i = 0; i < 14; ++i)
	{
		const float A = i * (2.f * PI / 14.f) + Rng.FRandRange(-0.15f, 0.15f);
		const float D = Rng.FRandRange(3200.f, 5600.f) + (i % 3) * 700.f;
		const float Z = Rng.FRandRange(-900.f, 100.f);
		const float RadI = Rng.FRandRange(320.f, 700.f);
		BuildIslet(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Z), RadI, GA, GB, RockC, (i % 2) ? Oak : Pine, FMath::RoundToInt(RadI / 160.f), Rng.FRandRange(500.f, 900.f), Rng);
	}

	{
		FActorSpawnParameters SP; SP.Owner = this;
		Scars = GetWorld()->SpawnActor<AZoogiFloorScars>(AZoogiFloorScars::StaticClass(), FTransform::Identity, SP);
		if (Scars)
		{
			Scars->Init(ZoogiConst::KnockoffRadius - 60.f);
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
	// (must stay in), and normal Zoogi shots that must crack the seat wall and a ricochet wall.
	if (AZoogiArenaPlayTest* PT = AZoogiArenaPlayTest::MaybeSpawn(this))
	{
		auto Pol = [](float R, float Deg) { const float A = FMath::DegreesToRadians(Deg); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f); };
		const float NormalShot = 0.55f * ZoogiConst::FullLaunchSpeed;
		for (int32 g = 0; g < 4; ++g)
		{
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("gap_%03.0f"), OrbExitGapYaw[g]); C.Kind = FZoogiArenaCheck::OrbOut;
			C.From = Pol(1250.f, OrbExitGapYaw[g]); C.Vel = FVector2D(Pol(1500.f, OrbExitGapYaw[g])); C.Wait = 3.0f;
			PT->Checks.Add(C);
		}
		{ FZoogiArenaCheck C; C.Name = TEXT("seatwall_control_120"); C.Kind = FZoogiArenaCheck::OrbStays; C.From = Pol(1250.f, 120.f); C.Vel = FVector2D(Pol(1500.f, 120.f)); C.Wait = 3.0f; PT->Checks.Add(C); }
		{ FZoogiArenaCheck C; C.Name = TEXT("seatwall_crack_120"); C.Kind = FZoogiArenaCheck::ZoogiDamages; C.From = Pol(1050.f, 120.f); C.Vel = FVector2D(Pol(NormalShot, 120.f));
		  C.PieceKind = TEXT("seatwall"); C.Near = Pol(FloorR + 18.f, 120.f); C.Radius = 350.f; C.Wait = 1.6f; PT->Checks.Add(C); }
		{ FZoogiArenaCheck C; C.Name = TEXT("ricochet_crack_30"); C.Kind = FZoogiArenaCheck::ZoogiDamages; C.From = Pol(650.f, 30.f); C.Vel = FVector2D(Pol(NormalShot, 30.f));
		  C.PieceKind = TEXT("stonewall"); C.Near = FVector::ZeroVector; C.Radius = 100.f; C.Wait = 1.6f; PT->Checks.Add(C); }
	}
	UE_LOG(LogZoogi, Log, TEXT("Meadow Arena built: %d pieces, %d forming, %d extra orbs, orb-exit gaps %.0f cm @ 0/90/180/270 (no Aura)"),
		Pieces.Num(), Forming ? Forming->Num() : 0, ExtraOrbSpots.Num(), OrbExitGapWidthCm);
}
