// Meadow Arena (hand-built kit, no Aura). Spec: reskin-incoming/01-meadow-arena/
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiMeadowLayout.h"
#include "ZoogiTypes.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFloorScars.h"
#include "ZoogiStageForming.h"
#include "ZoogiMeshKit.h"
#include "ZoogiPhysics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"

using namespace ZoogiMeadow;

void AZoogiArena::BuildMeadow()
{
	const FLinearColor Green = ZoogiUtil::Hex(TEXT("#39E37A"));
	const FLinearColor Soil = ZoogiUtil::Hex(TEXT("#9C8B73"));
	const FLinearColor Wood = ZoogiUtil::Hex(TEXT("#8B5A2B"));
	const FLinearColor WoodAlt = ZoogiUtil::Hex(TEXT("#A06A38"));
	const FLinearColor Riser = ZoogiUtil::Hex(TEXT("#6E4A28"));
	const FLinearColor SeatStone = ZoogiUtil::Hex(TEXT("#B9A889"));
	const FLinearColor RockA = ZoogiUtil::Hex(TEXT("#B9A889"));
	const FLinearColor RockB = ZoogiUtil::Hex(TEXT("#8E8063"));

	UMaterialInterface* FloorMat = KitFloorMaterial(TEXT("Meadow"), Green, 1.6f, 1.0f);
	BuildIsland(ZoogiUtil::Hex(TEXT("#3F8A2C")), ZoogiUtil::Hex(TEXT("#55A336")), Soil,
		ZoogiUtil::Hex(TEXT("#6E5B4A")), ZoogiUtil::Hex(TEXT("#4A3B30")), 0.3f, FloorMat);
	FloorMID = Cast<UMaterialInstanceDynamic>(FloorMat);
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
		P->AddBox(FVector(0, 0, 14.f), FVector(120.f, 120.f, 28.f), FRotator::ZeroRotator, StoneDark);
		P->AddBox(FVector(0, 0, 40.f), FVector(96.f, 96.f, 24.f), FRotator::ZeroRotator, Stone);
		FZoogiMeshBuilder Shaft;
		Shaft.Prism(FVector2D::ZeroVector, 4, 44.f, 38.f, 50.f, PillarH - 18.f, 45.f, 0.f, false);
		P->AddProc(Shaft, Stone);
		P->AddBox(FVector(0, 0, PillarH - 10.f), FVector(78.f, 78.f, 16.f), FRotator::ZeroRotator, Stone);
		for (int32 f = 0; f < 4; ++f)
		{
			const FRotator Fr(0.f, f * 90.f, 0.f);
			for (int32 s = -1; s <= 1; s += 2)
			{
				UStaticMeshComponent* S = P->AddBox(Fr.RotateVector(FVector(34.5f, s * 12.f, 0.f)) + FVector(0, 0, PillarH * 0.5f + 10.f), FVector(2.f, 6.f, PillarH * 0.55f), Fr, Slit, false);
				P->MarkGlow(S);
			}
		}
		for (int32 v = 0; v < 6; ++v)
		{
			const float T = v / 5.f;
			const float A = FMath::DegreesToRadians(30.f + T * 250.f);
			P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(FMath::Cos(A) * 36.f, FMath::Sin(A) * 36.f, 60.f + T * (PillarH - 80.f)), FVector(0.28f, 0.28f, 0.22f)), Vine, false);
		}
		P->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 12.f), FVector(0.34f, 0.34f, 0.24f)), Bronze);
		P->AddShape(TEXT("Cone"), FTransform(FRotator(180.f, 0.f, 0.f), FVector(0, 0, PillarH + 44.f), FVector(0.98f, 0.98f, 0.42f)), Bronze);
		UStaticMeshComponent* OrbM = P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 84.f), FVector(0.88f, 0.88f, 0.98f)), ZoogiUtil::GlowMID(this, Green, 2.4f), false);
		P->MarkGlow(OrbM);
		P->AddColliderBox(FVector(0, 0, 26.f), FVector(110.f, 110.f, 52.f), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
		P->AddColliderBox(FVector(0, 0, PillarH * 0.5f), FVector(80.f, 80.f, PillarH), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
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
			UE_LOG(LogZoogi, Log, TEXT("Meadow crater slab kit: %d meshes"), Scars->SlabMeshes.Num());
		}
	}
	if (Forming && FloorMID) Forming->AddParamRamp(FloorMID, TEXT("GlowStrength"), 0.f, 1.6f, 2.6f, 0.5f);
	BuildPads();
	UE_LOG(LogZoogi, Log, TEXT("Meadow Arena built: %d pieces, %d forming, %d extra orbs, orb-exit gaps %.0f cm @ 0/90/180/270 (no Aura)"),
		Pieces.Num(), Forming ? Forming->Num() : 0, ExtraOrbSpots.Num(), OrbExitGapWidthCm);
}
