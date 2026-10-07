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
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Am, PipeYaw[0])) < 6.f || FMath::Abs(FMath::FindDeltaAngleDegrees(Am, PipeYaw[1])) < 6.f) continue; // r5: quarter-pipes run out over the rim
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

	// ---- r5 ramps (measured off the Buzzy top-down; all wood/stone pieces stay breakable) ----
	auto Pol = [](float R, float Deg, float Z = 0.f) { const float A = FMath::DegreesToRadians(Deg); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); };
	UMaterialInterface* RampWood = Seat;
	UMaterialInterface* RampEdge = SeatRiser;
	// Central spiral ramp: 4 breakable stone quarters climbing from the floor (yaw 125) to the raised disc (yaw 270).
	{
		const int32 Quarters = 4, SegsPer = 10;
		const float Wd = SpiralROut - SpiralRIn, Rm = (SpiralRIn + SpiralROut) * 0.5f;
		auto Zt = [](float T) { return SpiralH * (T * T * (3.f - 2.f * T)); }; // flat entry, flat into the disc
		for (int32 q = 0; q < Quarters; ++q)
		{
			AZoogiArenaPiece* P = SpawnPiece(FVector::ZeroVector, 0.f, TEXT("spiralramp"), TEXT("Stone"), 200.f, false);
			if (!P) continue;
			FZoogiMeshBuilder Top, Sides, Rail;
			for (int32 i = 0; i < SegsPer; ++i)
			{
				const float Ta = (q * SegsPer + i) / float(Quarters * SegsPer), Tb = (q * SegsPer + i + 1) / float(Quarters * SegsPer);
				const float Aa = FMath::Lerp(SpiralA0, SpiralA1, Ta), Ab = FMath::Lerp(SpiralA0, SpiralA1, Tb);
				const float Za = Zt(Ta), Zb = Zt(Tb);
				Top.Quad(Pol(SpiralRIn, Aa, Za), Pol(SpiralROut, Aa, Za), Pol(SpiralROut, Ab, Zb), Pol(SpiralRIn, Ab, Zb), FVector::UpVector);
				const FVector Oa = Pol(1.f, Aa, 0.f), Ob = Pol(1.f, Ab, 0.f);
				Sides.Quad(Pol(SpiralRIn, Aa, -2.f), Pol(SpiralRIn, Ab, -2.f), Pol(SpiralRIn, Ab, Zb), Pol(SpiralRIn, Aa, Za), -(Oa + Ob));
				Sides.Quad(Pol(SpiralROut, Aa, -2.f), Pol(SpiralROut, Ab, -2.f), Pol(SpiralROut, Ab, Zb + 30.f), Pol(SpiralROut, Aa, Za + 30.f), Oa + Ob);
				Rail.Quad(Pol(SpiralROut - 18.f, Aa, Za), Pol(SpiralROut - 18.f, Ab, Zb), Pol(SpiralROut - 18.f, Ab, Zb + 30.f), Pol(SpiralROut - 18.f, Aa, Za + 30.f), -(Oa + Ob));
				Rail.Quad(Pol(SpiralROut - 18.f, Aa, Za + 30.f), Pol(SpiralROut - 18.f, Ab, Zb + 30.f), Pol(SpiralROut, Ab, Zb + 30.f), Pol(SpiralROut, Aa, Za + 30.f), FVector::UpVector);
				// Collider: solid tangent block whose top follows the ramp, plus the outer curb.
				const float Am = (Aa + Ab) * 0.5f, Zm = (Za + Zb) * 0.5f;
				const float L = Rm * FMath::DegreesToRadians(FMath::Abs(Ab - Aa));
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Za - Zb, L)); // +X = increasing angle = downhill side
				const float H = FMath::Max(Zm + 20.f, 30.f);
				const FRotator R(Pitch, Am + 90.f, 0.f);
				P->AddColliderBox(Pol(Rm, Am, Zm - H * 0.5f), FVector(L * 1.08f, Wd, H), R, TEXT("ZRamp"), StonePM);
				P->AddColliderBox(Pol(SpiralROut - 9.f, Am, Zm + 9.f), FVector(L * 1.08f, 18.f, 38.f), R, TEXT("ZWall"), WallPM);
			}
			P->AddProc(Top, Stone); P->AddProc(Sides, StoneDark); P->AddProc(Rail, Stone);
			P->RubbleKeepZ = 40.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(SpiralH + 80.f)), 0.9f + 0.1f * q, 0.8f);
		}
		// Raised round disc at the top of the spiral (breakable stone platform).
		AZoogiArenaPiece* D = SpawnPiece(Pol(DiscR, DiscYaw, 0.f), 0.f, TEXT("platform"), TEXT("Stone"), 180.f, false);
		if (D)
		{
			FZoogiMeshBuilder Top;
			Top.Prism(FVector2D::ZeroVector, 16, DiscRad, DiscRad - 4.f, -2.f, SpiralH, 0.f, 0.f, false);
			D->AddProc(Top, Stone);
			UStaticMeshComponent* G = D->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, SpiralH + 0.6f), FVector((DiscRad * 0.7f) / 50.f, (DiscRad * 0.7f) / 50.f, 0.01f)), GlowTop, false);
			D->MarkGlow(G);
			D->AddColliderCyl(FVector2D::ZeroVector, DiscRad, -2.f, SpiralH, TEXT("ZPlatform"), StonePM);
			D->FinishBuild();
			if (Forming) Forming->AddActor(D, EZoogiFormStyle::Rise, FVector(0, 0, -(SpiralH + 80.f)), 1.3f, 0.8f, 4.f);
		}
	}
	// Two tall wooden quarter-pipes flanking the north gap: roll out, ride up the curve, launch, fall back in.
	for (int32 k = 0; k < 2; ++k)
	{
		AZoogiArenaPiece* P = SpawnPiece(Pol(PipeR0, PipeYaw[k], 0.f), PipeYaw[k], TEXT("woodramp"), TEXT("Wood"), 160.f, false);
		if (!P) continue;
		const int32 N = 16; const float Hw = PipeWid * 0.5f;
		FZoogiMeshBuilder Top, Sides, Rails;
		auto Pt = [&](int32 i) { const float Th = FMath::DegreesToRadians(PipeEndDeg * i / N); return FVector(PipeRq * FMath::Sin(Th), 0.f, PipeRq * (1.f - FMath::Cos(Th))); };
		for (int32 i = 0; i < N; ++i)
		{
			const FVector A = Pt(i), B = Pt(i + 1), M = (A + B) * 0.5f;
			const float ThM = FMath::DegreesToRadians(PipeEndDeg * (i + 0.5f) / N);
			const FVector Nn(-FMath::Sin(ThM), 0.f, FMath::Cos(ThM));
			Top.Quad(A + FVector(0, -Hw, 0), B + FVector(0, -Hw, 0), B + FVector(0, Hw, 0), A + FVector(0, Hw, 0), Nn);
			for (int32 s = -1; s <= 1; s += 2)
				Sides.Quad(FVector(A.X, s * Hw, -170.f), FVector(B.X, s * Hw, -170.f), B + FVector(0, s * Hw, 0), A + FVector(0, s * Hw, 0), FVector(0, s, 0));
			const float L = FVector::Dist(A, B);
			const FRotator R(FMath::RadiansToDegrees(ThM), 0.f, 0.f);
			for (int32 s = -1; s <= 1; s += 2)
			{
				Rails.Box(M + FVector(0, s * (Hw + 7.f), 0) + Nn * 16.f, FVector(L * 0.53f, 7.f, 16.f), R);
				P->AddColliderBox(M + FVector(0, s * (Hw + 7.f), 0) + Nn * 16.f, FVector(L * 1.06f, 14.f, 44.f), R, TEXT("ZWall"), WallPM);
			}
			P->AddColliderBox(M - Nn * 20.f, FVector(L * 1.08f, PipeWid, 40.f), R, TEXT("ZRamp"), StonePM);
			if (i % 4 == 2) { UStaticMeshComponent* Ch = P->AddBox(M + Nn * 1.2f, FVector(10.f, PipeWid * 0.6f, 1.5f), R, GlowTop, false); P->MarkGlow(Ch); }
		}
		const FVector E = Pt(N);
		Sides.Quad(FVector(E.X, -Hw, -170.f), FVector(E.X, Hw, -170.f), E + FVector(0, Hw, 0), E + FVector(0, -Hw, 0), FVector::ForwardVector);
		Rails.Box(E + FVector(6.f, 0, 8.f), FVector(12.f, Hw + 14.f, 12.f)); // coping lip
		P->AddProc(Top, RampWood); P->AddProc(Sides, RampEdge); P->AddProc(Rails, StoneDark);
		// Back stop over the coping so an overshoot drops back into the pipe instead of off the map.
		P->AddColliderBox(E + FVector(14.f, 0, 90.f), FVector(20.f, PipeWid + 30.f, 180.f), FRotator::ZeroRotator, TEXT("ZWall"), WallPM);
		P->RubbleKeepZ = 60.f;
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(PipeRq + 120.f)), 1.0f + 0.15f * k, 0.9f);
	}
	// Two low curved lips at the front stand ends: rise toward the stand end, curling inward.
	for (int32 k = 0; k < 2; ++k)
	{
		AZoogiArenaPiece* P = SpawnPiece(FVector::ZeroVector, 0.f, TEXT("woodramp"), TEXT("Wood"), 140.f, false);
		if (!P) continue;
		const int32 N = 16; const float Hw = LipWid * 0.5f;
		auto C = [&](int32 i) { const float T = float(i) / N; return Pol(FMath::Lerp(LipRLow, LipRHigh, T), FMath::Lerp(LipYawLow[k], LipYawHigh[k], T), LipH * FMath::Pow(T, 2.2f)); };
		FZoogiMeshBuilder Top, Sides, Rails;
		for (int32 i = 0; i < N; ++i)
		{
			const FVector A = C(i), B = C(i + 1), M = (A + B) * 0.5f;
			const FVector D = (B - A).GetSafeNormal();
			const FVector Sd = FVector::CrossProduct(FVector::UpVector, D).GetSafeNormal2D();
			const FVector Nn = FVector::CrossProduct(D, Sd).GetSafeNormal() * -1.f;
			const FVector Up = Nn.Z > 0.f ? Nn : -Nn;
			Top.Quad(A - Sd * Hw, B - Sd * Hw, B + Sd * Hw, A + Sd * Hw, Up);
			for (int32 s = -1; s <= 1; s += 2)
				Sides.Quad(A + Sd * (s * Hw) - FVector(0, 0, A.Z + 2.f), B + Sd * (s * Hw) - FVector(0, 0, B.Z + 2.f), B + Sd * (s * Hw), A + Sd * (s * Hw), Sd * s);
			const FRotator R = D.Rotation();
			const float L = FVector::Dist(A, B);
			for (int32 s = -1; s <= 1; s += 2)
			{
				Rails.Box(M + Sd * (s * (Hw + 6.f)) + Up * 12.f, FVector(L * 0.53f, 6.f, 12.f), R);
				P->AddColliderBox(M + Sd * (s * (Hw + 6.f)) + Up * 12.f, FVector(L * 1.06f, 12.f, 36.f), R, TEXT("ZWall"), WallPM);
			}
			const float H = FMath::Max(M.Z + 20.f, 30.f);
			P->AddColliderBox(M - Up * (H * 0.5f - 0.f) , FVector(L * 1.08f, LipWid, H), R, TEXT("ZRamp"), StonePM);
		}
		const FVector E = C(N), Ep = C(N - 1);
		const FVector Sd = FVector::CrossProduct(FVector::UpVector, (E - Ep).GetSafeNormal()).GetSafeNormal2D();
		Sides.Quad(E - Sd * Hw - FVector(0, 0, E.Z + 2.f), E + Sd * Hw - FVector(0, 0, E.Z + 2.f), E + Sd * Hw, E - Sd * Hw, (E - Ep).GetSafeNormal2D());
		Rails.Box(E + FVector(0, 0, 4.f), FVector(10.f, Hw + 12.f, 8.f), (E - Ep).Rotation()); // lip edge
		P->AddProc(Top, RampWood); P->AddProc(Sides, RampEdge); P->AddProc(Rails, StoneDark);
		P->RubbleKeepZ = 40.f;
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(LipH + 80.f)), 1.1f + 0.1f * k, 0.8f);
	}
	// Wooden bench blocks (breakable), tangential like the art.
	for (int32 b = 0; b < NumBenches; ++b)
	{
		AZoogiArenaPiece* P = SpawnPiece(Pol(BenchR, BenchYaw[b], 0.f), BenchYaw[b] + 90.f, TEXT("bench"), TEXT("Wood"), 70.f, false);
		if (!P) continue;
		P->AddBox(FVector(0, 0, 38.f), FVector(130.f, 52.f, 14.f), FRotator::ZeroRotator, SeatAlt);
		for (int32 s = -1; s <= 1; s += 2) P->AddBox(FVector(s * 48.f, 0, 16.f), FVector(22.f, 46.f, 32.f), FRotator::ZeroRotator, SeatRiser);
		P->AddColliderBox(FVector(0, 0, 23.f), FVector(130.f, 52.f, 46.f), FRotator::ZeroRotator, TEXT("ZWall"), WallPM);
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -90.f), 1.2f + 0.05f * b, 0.6f);
	}

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
		AddStands(StandCenters[i], StandArcs[i], StandRIn, -40.f, StandTiers, 72.f, 50.f, Seat, SeatAlt, SeatRiser, Stone);

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

	// r5: the two small red shrines behind the NE stand (art), at the stand's outer edge.
	for (int32 i = 0; i < 2; ++i)
	{
		const float Yw = i ? 343.f : 326.f;
		AddPagoda(Pol(2120.f, Yw, -160.f), Yw + 180.f, 1, 62.f, Body, Panel, Roof, Trim, Stone, Bronze);
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
				// r5: near/mid ridges are rounded rolling hills (the art's soft green downs); only the far ridge is peaked.
				const bool bHill = r < 2;
				UStaticMeshComponent* C = AddMesh(bHill ? ZoogiUtil::BasicShape(TEXT("Sphere")) : Cone, (i & 1) ? MA : MB, false, false);
				C->SetRelativeLocationAndRotation(bHill ? FVector(P.X, P.Y, -2400.f) : P, FRotator(0.f, MR.FRandRange(0.f, 360.f), 0.f));
				C->SetRelativeScale3D(bHill ? FVector(W * 1.3f / 100.f, W * 1.3f / 100.f * MR.FRandRange(0.8f, 1.2f), 2.f * H / 100.f) : FVector(W / 100.f, W / 100.f * MR.FRandRange(0.8f, 1.2f), H / 100.f));
				if (r == 2 && H > 9000.f) // snow caps on the tallest far peaks
				{
					UStaticMeshComponent* Cap = AddMesh(Cone, Snow, false, false);
					Cap->SetRelativeLocation(P + FVector(0, 0, H * 0.35f));
					Cap->SetRelativeScale3D(FVector(W * 0.3f / 100.f, W * 0.3f / 100.f, H * 0.3f / 100.f));
				}
			}
		}
		// r5: three big waterfalls where the art has them (north-west, north, north-east valley), each falling down a
		// dark cliff face so it reads against the hills.
		UMaterialInterface* CliffDark = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#6B5A4A")), ZoogiUtil::Hex(TEXT("#4A3B30")), 0.004f, 0.95f);
		const float FallYaw[3] = { 238.f, 266.f, 302.f }, FallR[3] = { 6400.f, 8200.f, 6900.f };
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Top = Pol(FallR[i], FallYaw[i], 700.f + i * 150.f);
			AddBoxR(Pol(FallR[i] + 260.f, FallYaw[i], -900.f + i * 75.f), FVector(500.f, 1600.f, 3400.f + i * 150.f), FRotator(0.f, FallYaw[i], 0.f), CliffDark, false);
			AddBoxR(Pol(FallR[i] + 900.f, FallYaw[i], 760.f + i * 150.f), FVector(1600.f, 2200.f, 120.f), FRotator(0.f, FallYaw[i], 0.f), GrassTop, false);
			AddWaterfall(Top, FallYaw[i] + 180.f, 520.f, 3300.f, 120.f);
			AddWaterfall(Top + Pol(1.f, FallYaw[i] + 90.f, 0.f) * 260.f, FallYaw[i] + 180.f, 220.f, 3300.f, 90.f);
		}
	}

	// r5: landmark buildings where the art has them, on hills past the stands (out of reach: solid scenery).
	{
		UMaterialInterface* HillTop = GrassTop;
		UMaterialInterface* Red = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A8302A")), 0.65f);
		UMaterialInterface* RoofGrey = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#5C6470")), 0.6f);
		// 5-storey pagoda tower on the NW hill with two small halls (art: top-left cluster).
		{
			const FVector2D P(Pol(2550.f, 226.f, 0.f));
			AddCylinder(P, 520.f, 900.f, 300.f, HillTop, false);
			AddUnderCone(P, 520.f, 1300.f, -600.f, Cliff);
			AddPagoda(FVector(P.X, P.Y, 300.f), 46.f, 5, 105.f, Body, Panel, Roof, Trim, Stone, Bronze);
			for (int32 j = 0; j < 2; ++j)
			{
				const FVector2D Q(Pol(2650.f, j ? 238.f : 212.f, 0.f));
				AddCylinder(Q, 330.f, 700.f, 180.f, HillTop, false);
				AddPagoda(FVector(Q.X, Q.Y, 180.f), 46.f, 2, 70.f, Body, Panel, Roof, Trim, Stone, Bronze);
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
			const float Zc = 310.f, Hh = 380.f, Lw = 1400.f, Dp = 520.f;
			AddBoxR(At(0, 0, Zc + Hh * 0.5f), FVector(Dp, Lw, Hh), Rot, Body);
			for (int32 c = 0; c < 9; ++c)
				AddBoxR(At(-Dp * 0.5f - 30.f, -Lw * 0.5f + c * (Lw / 8.f), Zc + Hh * 0.5f), FVector(40.f, 40.f, Hh), Rot, Red);
			AddBoxR(At(-Dp * 0.5f - 6.f, 0, Zc + 150.f), FVector(8.f, 300.f, 300.f), Rot, Portal);       // gate doors
			AddBoxR(At(-Dp * 0.5f - 30.f, 0, Zc + Hh + 10.f), FVector(70.f, Lw + 80.f, 30.f), Rot, Red);   // lintel beam
			for (int32 w = -1; w <= 1; w += 2) AddBoxR(At(-Dp * 0.5f - 8.f, w * 450.f, Zc + 230.f), FVector(6.f, 220.f, 90.f), Rot, Panel); // windows
			// Broad hip roof: two pitched slabs meeting at a ridge, wide eaves, plus a ridge beam and end hips.
			const float Ez = Zc + Hh + 30.f, Pit = 26.f, Run = Dp * 0.5f + 180.f;
			for (int32 s2 = -1; s2 <= 1; s2 += 2)
				AddBoxR(At(s2 * Run * 0.5f, 0, Ez + Run * 0.5f * FMath::Tan(FMath::DegreesToRadians(Pit))), FVector(Run / FMath::Cos(FMath::DegreesToRadians(Pit)) + 10.f, Lw + 360.f, 26.f), FRotator(-s2 * Pit, Yaw, 0.f), RoofGrey);
			const float RidgeZ = Ez + Run * FMath::Tan(FMath::DegreesToRadians(Pit));
			AddBoxR(At(0, 0, RidgeZ + 8.f), FVector(40.f, Lw + 300.f, 30.f), Rot, Trim);
			for (int32 s2 = -1; s2 <= 1; s2 += 2)
				AddBoxR(At(0, s2 * (Lw * 0.5f + 140.f), Ez + 80.f), FVector(Run * 1.6f, 120.f, 160.f), FRotator(0.f, Yaw, s2 * 30.f), RoofGrey);
		}
		// Stone golem statue behind the north-west quarter-pipe (art).
		{
			UMaterialInterface* GolemStone = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#8E8A78")), ZoogiUtil::Hex(TEXT("#6D6A5C")), 0.01f, 0.9f);
			UMaterialInterface* Moss = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#5E9A3A")), 0.9f);
			const FVector G = Pol(2250.f, 246.f, -160.f);
			const FRotator Rg(0.f, 246.f + 180.f, 0.f);
			auto GA2 = [&](float X, float Y, float Z) { return G + Rg.RotateVector(FVector(X, Y, Z)); };
			for (int32 s2 = -1; s2 <= 1; s2 += 2) AddBoxR(GA2(0, s2 * 70.f, 110.f), FVector(90.f, 80.f, 220.f), Rg, GolemStone);
			AddBoxR(GA2(0, 0, 330.f), FVector(150.f, 260.f, 220.f), Rg, GolemStone);
			AddBoxR(GA2(0, 0, 330.f + 140.f), FVector(160.f, 280.f, 40.f), Rg, Moss);
			for (int32 s2 = -1; s2 <= 1; s2 += 2) AddBoxR(GA2(10.f, s2 * 175.f, 280.f), FVector(80.f, 70.f, 260.f), FRotator(0.f, 246.f + 180.f, s2 * 8.f), GolemStone);
			AddBoxR(GA2(-10.f, 0, 520.f), FVector(110.f, 120.f, 100.f), Rg, GolemStone);
			UStaticMeshComponent* Eye = AddBoxR(GA2(-66.f, 0, 530.f), FVector(4.f, 70.f, 12.f), Rg, Portal, false);
			(void)Eye;
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
	// (must stay in), normal Zoogi shots that must crack the seat wall and a bench, and ramp roll-and-launch checks.
	if (AZoogiArenaPlayTest* PT = AZoogiArenaPlayTest::MaybeSpawn(this))
	{
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
		{ FZoogiArenaCheck C; C.Name = TEXT("bench_crack_65"); C.Kind = FZoogiArenaCheck::ZoogiDamages; C.From = Pol(480.f, 65.f); C.Vel = FVector2D(Pol(NormalShot, 65.f));
		  C.PieceKind = TEXT("bench"); C.Near = Pol(BenchR, 65.f); C.Radius = 150.f; C.Wait = 1.6f; PT->Checks.Add(C); }
		// Ramp roll-and-launch: a normal-power Zoogi must ride each ramp and get airborne (rise >= Radius cm).
		for (int32 k = 0; k < 2; ++k)
		{
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("pipe_launch_%03.0f"), PipeYaw[k]); C.Kind = FZoogiArenaCheck::ZoogiLaunch;
			C.From = Pol(850.f, PipeYaw[k]); C.Vel = FVector2D(Pol(NormalShot, PipeYaw[k])); C.Radius = 150.f; C.Wait = 3.0f;
			C.PieceKind = TEXT("woodramp"); C.Near = Pol(PipeR0 + 250.f, PipeYaw[k]); PT->Checks.Add(C);
		}
		for (int32 k = 0; k < 2; ++k)
		{
			const FVector L0 = Pol(LipRLow, LipYawLow[k]), L1 = Pol(FMath::Lerp(LipRLow, LipRHigh, 0.08f), FMath::Lerp(LipYawLow[k], LipYawHigh[k], 0.08f));
			const FVector D = (L1 - L0).GetSafeNormal2D();
			FZoogiArenaCheck C; C.Name = FString::Printf(TEXT("lip_launch_%d"), k); C.Kind = FZoogiArenaCheck::ZoogiLaunch;
			C.From = L0 - D * 260.f; C.Vel = FVector2D(D * NormalShot); C.Radius = 60.f; C.Wait = 2.5f;
			C.PieceKind = TEXT("woodramp"); C.Near = L0; PT->Checks.Add(C);
		}
		{
			const FVector L0 = Pol(310.f, SpiralA0), L1 = Pol(310.f, SpiralA0 - 6.f);
			const FVector D = (L1 - L0).GetSafeNormal2D();
			FZoogiArenaCheck C; C.Name = TEXT("spiral_ride"); C.Kind = FZoogiArenaCheck::ZoogiLaunch;
			C.From = L0 - D * 150.f; C.Vel = FVector2D(D * NormalShot * 0.8f); C.Radius = 30.f; C.Wait = 2.5f;
			C.PieceKind = TEXT("spiralramp"); C.Near = L0; PT->Checks.Add(C);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("Meadow Arena built: %d pieces, %d forming, %d extra orbs, orb-exit gaps %.0f cm @ 0/90/180/270 (no Aura)"),
		Pieces.Num(), Forming ? Forming->Num() : 0, ExtraOrbSpots.Num(), OrbExitGapWidthCm);
}
