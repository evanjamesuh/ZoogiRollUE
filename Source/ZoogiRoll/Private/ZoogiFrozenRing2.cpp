// Frozen Ring OUTER ACTION RING (Ring 2), Oct 14 (Evan: "build the Frozen outer ring per the approved plan").
// Plan: /workspace/zoogi-unreal/frozen-polish/OUTER_RING_PLAN.md + outer_ring_plan(.png/_zoom.png); numbers in Docs/Arenas/FROZEN_RING2.md.
// Ring 1 (court r1550) is unchanged except the 6 Ring 1 wall gaps at the re-entry ramps (ZoogiFrozenLayout.h WallFillArcs / W / N arcs).
//   crevasse r1550-1750 (no floor: falling in = ring-out by the existing FallOutZ rule)
//   Ring 2 floor r1750-2060 at z -30, inner curb h15, outer bank walls r2060 h70 (gaps at the 6 bridges and 6 orb pockets)
//   6 orb pockets (outer orbs: Episode Energy pickups), 6 crystal bumpers r1905, 12 curved deflector wings, 6 re-entry ramps,
//   ice slide arcs (zones), slide / bank / runway lanes ride the existing 6 bridges (spans A/B: the bridge deck already crosses).
//   New ring-out rule: OUT past r2300 (AZoogiArena::OuterOutR, checked by the game mode) or by falling (crevasse / edges).
// Everything here is an AZoogiObstacle (unbreakable, never a Crystal Collapse piece). -ZoogiFrozenRing2=Off restores the old map.
#include "ZoogiArena.h"
#include "ZoogiObstacle.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiMeshKit.h"
#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

// Oct 15 (Evan: "Package now, outer ring next build"): Ring 2 ON by default, with the layout moves of the plan (islets
// r2900, two-span bridges, Ring 1 wall gaps at the 6 ramps, crevasse rim, outcrop r2650, rim falls 80 / 100) applied in
// BuildFrozen. [ZoogiRoll.Frozen] bRing2 (True), -ZoogiFrozenRing2=Off|On (Off = the pre-Ring-2 map exactly).
bool ZoogiFrozen::Ring2On()
{
	static const bool bOn = []
	{
		bool b = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Frozen"), TEXT("bRing2"), b, GGameIni);
		FString V;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiFrozenRing2="), V)) b = !(V.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || V == TEXT("0") || V.Equals(TEXT("False"), ESearchCase::IgnoreCase));
		UE_LOG(LogZoogi, Log, TEXT("FROZEN Ring 2 (outer action ring): %s"), b ? TEXT("ON (default; -ZoogiFrozenRing2=Off reverts)") : TEXT("OFF"));
		return b;
	}();
	return bOn;
}

namespace ZoogiRing2
{
	constexpr float CrevIn = 1550.f, R2In = 1750.f, R2Out = 2060.f, R2Z = -30.f, OutR = 2300.f;
	constexpr float CurbW = 15.f, CurbH = 15.f, WallThick = 30.f, WallH = 70.f;
	constexpr float BridgeYaw[6] = { 0.f, 45.f, 135.f, 180.f, 225.f, 315.f };
	constexpr float RampYaw[6] = { 30.f, 67.5f, 112.5f, 150.f, 240.f, 300.f };
	constexpr float PocketYaw[6] = { 22.5f, 90.f, 157.5f, 202.5f, 270.f, 337.5f };
	constexpr float BumperYaw[6] = { 78.f, 102.f, 258.f, 282.f, 191.25f, 348.75f };
	constexpr float PocketR = 2150.f, PocketBackR = 112.f;
	bool Enabled() { return ZoogiFrozen::Ring2On(); }
	FVector P(float R, float Yaw, float Z) { const float A = FMath::DegreesToRadians(Yaw); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z); }
	bool NearAny(float A, const float* Ys, int32 N, float Tol) { for (int32 i = 0; i < N; ++i) if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, Ys[i])) < Tol) return true; return false; }
	// Oriented box hull (8 corners) in world cm.
	void BoxHull(TArray<TArray<FVector>>& H, const FVector& C, const FVector& Half, const FRotator& R)
	{
		TArray<FVector>& Pts = H.AddDefaulted_GetRef();
		for (int32 i = 0; i < 8; ++i) Pts.Add(C + R.RotateVector(FVector((i & 1) ? Half.X : -Half.X, (i & 2) ? Half.Y : -Half.Y, (i & 4) ? Half.Z : -Half.Z)));
	}
	// Arc of radial-box hulls between yaws A0..A1 (deg), radii R0..R1, z Z0..Z1.
	void ArcHulls(TArray<TArray<FVector>>& H, float R0, float R1, float A0, float A1, float Z0, float Z1, float StepDeg = 4.f)
	{
		const int32 N = FMath::Max(1, FMath::CeilToInt((A1 - A0) / StepDeg));
		for (int32 i = 0; i < N; ++i)
		{
			const float Aa = A0 + (A1 - A0) * i / N, Ab = A0 + (A1 - A0) * (i + 1) / N, Am = 0.5f * (Aa + Ab);
			const float HalfT = FMath::Tan(FMath::DegreesToRadians(0.5f * (Ab - Aa))) * R1 + 1.f;
			BoxHull(H, P(0.5f * (R0 + R1), Am, 0.5f * (Z0 + Z1)), FVector(0.5f * (R1 - R0), HalfT, 0.5f * (Z1 - Z0)), FRotator(0.f, Am, 0.f));
		}
	}
	// Open arcs of [0,360) minus gaps (centre yaw, half width).
	TArray<FVector2D> OpenArcs(const TArray<FVector2D>& Gaps)
	{
		TArray<FVector2D> G = Gaps; G.Sort([](const FVector2D& a, const FVector2D& b) { return a.X < b.X; });
		TArray<FVector2D> Out;
		for (int32 i = 0; i < G.Num(); ++i)
		{
			const float S = G[i].X + G[i].Y;
			const float E = (i + 1 < G.Num() ? G[i + 1].X - G[i + 1].Y : G[0].X - G[0].Y + 360.f);
			if (E > S + 0.5f) Out.Add(FVector2D(S, E));
		}
		return Out;
	}
}

void AZoogiArena::BuildFrozenRing2()
{
	using namespace ZoogiRing2;
	OuterOutR = 0.f;
	if (!Enabled()) return;
	OuterOutR = OutR;
	UMaterialInterface* Snow = FrozenMat(TEXT("Snow"));
	UMaterialInterface* Ice = FrozenMat(TEXT("Ice"));
	UMaterialInterface* IcePane = FrozenMat(TEXT("IcePane"));
	UMaterialInterface* Stone = FrozenMat(TEXT("Stone"));
	UMaterialInterface* StoneDark = FrozenMat(TEXT("StoneDark"));
	UMaterialInterface* Cliff = FrozenMat(TEXT("Cliff"));
	UMaterialInterface* OrbGlow = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#8FE8FF")), 1.2f);
	UMaterialInterface* CrystalGlow = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#C9A8FF")), 1.0f);
	UPhysicalMaterial* FloorPMr = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.35f, false);
	UPhysicalMaterial* WallPMr = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.9f, false);   // Ring 1 wall restitution: predictable rebounds
	UPhysicalMaterial* DeflPM = ZoogiPhysics::MakeMaterial(this, 0.1f, 0.6f, false);
	UPhysicalMaterial* BumpPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 1.0f, false);
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto Spawn = [&](const TCHAR* Name, const FVector& Loc = FVector::ZeroVector) -> AZoogiObstacle*
	{
		AZoogiObstacle* O = GetWorld()->SpawnActor<AZoogiObstacle>(AZoogiObstacle::StaticClass(), FTransform(Loc), SP);
		if (O) { O->ObsName = FName(Name); Obstacles.Add(O); }
		return O;
	};
	auto Vis = [&](const FZoogiMeshBuilder& M, UMaterialInterface* Mat) { AddFrozenProc(M, Mat, nullptr, true); };

	// ---- crevasse faces (visual): inner ice cliff r1555 and outer r1748 down to z -500
	{
		FZoogiMeshBuilder F;
		F.ArcSlab(CrevIn - 10.f, CrevIn + 5.f, 0.f, 360.f, -500.f, -4.f, 120);
		F.ArcSlab(R2In - 2.f, R2In + 10.f, 0.f, 360.f, -500.f, R2Z - 2.f, 120);
		Vis(F, Cliff);
	}
	// ---- Ring 2 floor (collision) + ice slide arcs (visual + zones)
	TArray<TArray<FVector>> FloorH, WallH_, CurbH_;
	ArcHulls(FloorH, R2In, R2Out + 40.f, 0.f, 360.f, R2Z - 30.f, R2Z, 3.f);
	{
		FZoogiMeshBuilder F, I;
		F.ArcSlab(R2In, R2Out + 40.f, 0.f, 360.f, R2Z - 30.f, R2Z, 144);
		I.ArcSlab(R2In + 2.f, R2Out - 2.f, -25.f, 25.f, R2Z + 0.6f, R2Z + 1.2f, 24);
		I.ArcSlab(R2In + 2.f, R2Out - 2.f, 155.f, 205.f, R2Z + 0.6f, R2Z + 1.2f, 24);
		I.ArcSlab(R2In + 15.f, 1905.f, 240.f, 300.f, R2Z + 0.6f, R2Z + 1.2f, 24);
		Vis(F, Snow); Vis(I, IcePane);
	}
	// ---- inner curb (open at bridge and ramp mouths), outer bank walls (open at span B and pocket mouths)
	TArray<FVector2D> CurbGaps, WallGaps;
	for (float Y : BridgeYaw) { CurbGaps.Add(FVector2D(Y, 3.6f)); WallGaps.Add(FVector2D(Y, 3.5f)); }
	for (float Y : RampYaw) CurbGaps.Add(FVector2D(Y, 3.3f));
	for (float Y : PocketYaw) WallGaps.Add(FVector2D(Y, 3.1f));
	{
		FZoogiMeshBuilder C, W, Wc;
		for (const FVector2D& A : OpenArcs(CurbGaps))
		{
			ArcHulls(CurbH_, R2In, R2In + CurbW, A.X, A.Y, R2Z, R2Z + CurbH);
			C.ArcSlab(R2In, R2In + CurbW, A.X, A.Y, R2Z, R2Z + CurbH, FMath::Max(2, FMath::CeilToInt((A.Y - A.X) / 3.f)));
		}
		for (const FVector2D& A : OpenArcs(WallGaps))
		{
			ArcHulls(WallH_, R2Out, R2Out + WallThick, A.X, A.Y, R2Z, R2Z + WallH);
			W.ArcSlab(R2Out, R2Out + WallThick, A.X, A.Y, R2Z, R2Z + WallH, FMath::Max(2, FMath::CeilToInt((A.Y - A.X) / 3.f)));
			Wc.ArcSlab(R2Out - 3.f, R2Out + WallThick + 3.f, A.X, A.Y, R2Z + WallH, R2Z + WallH + 6.f, FMath::Max(2, FMath::CeilToInt((A.Y - A.X) / 3.f)));
		}
		Vis(C, Snow); Vis(W, Stone); Vis(Wc, Snow);
	}
	if (AZoogiObstacle* O = Spawn(TEXT("Ring2_Floor")))
	{
		O->BuildCollider(FloorH, 1.f, FloorPMr);
		// ice slide arcs (IceRollingDecel 115) and the inner-half far arc
		auto AddIce = [&](float A0, float A1, float R0, float R1)
		{
			FZoogiObsTrigger& T = O->Triggers.AddDefaulted_GetRef();
			T.Kind = 4; T.Shape = 4; T.E = FVector(R0, R1, R2Z + 20.f); T.D = FVector2D(A0, A1); T.P[0] = 115.f;
		};
		AddIce(335.f, 385.f, R2In, R2Out); AddIce(155.f, 205.f, R2In, R2Out); AddIce(240.f, 300.f, R2In + 15.f, 1905.f);
	}
	if (AZoogiObstacle* O = Spawn(TEXT("Ring2_Curb"))) O->BuildCollider(CurbH_, 1.f, FloorPMr);
	if (AZoogiObstacle* O = Spawn(TEXT("Ring2_BankWalls"))) O->BuildCollider(WallH_, 1.f, WallPMr);

	// ---- 6 orb pockets: alcove floor + concave back wall (r112 around r2150, h70), outer orb in an 8 cm cup -> Episode Energy
	UStaticMesh* SphereM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* ConeM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	for (float Y : PocketYaw)
	{
		TArray<TArray<FVector>> H;
		FZoogiMeshBuilder Fl, Bw, Cup;
		const FVector C = P(PocketR, Y, 0.f);
		BoxHull(H, C + FVector(0, 0, R2Z - 15.f), FVector(130.f, 130.f, 15.f), FRotator(0.f, Y, 0.f));
		Fl.Box(C + FVector(0, 0, R2Z - 15.f), FVector(130.f, 128.f, 15.f), FRotator(0.f, Y, 0.f));
		for (int32 k = -6; k <= 6; ++k) // back wall: semicircle facing the ring
		{
			const float A = Y + k * 15.f; const float Rad = FMath::DegreesToRadians(A);
			const FVector Pt = C + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.f) * (PocketBackR + 15.f) + FVector(0, 0, R2Z + WallH * 0.5f);
			if (FMath::Abs(k) > 5) continue;
			BoxHull(H, Pt, FVector(15.f, 16.f, WallH * 0.5f), FRotator(0.f, A, 0.f));
			Bw.Box(Pt, FVector(15.f, 16.f, WallH * 0.5f), FRotator(0.f, A, 0.f));
		}
		Cup.Prism(FVector2D(C), 16, 52.f, 46.f, R2Z, R2Z + 2.f, 0.f);
		Vis(Fl, Snow); Vis(Bw, Stone); Vis(Cup, Ice);
		if (AZoogiObstacle* O = Spawn(*FString::Printf(TEXT("Ring2_Pocket_%g"), Y)))
		{
			O->BuildCollider(H, 1.f, WallPMr);
		}
		if (AZoogiObstacle* Orb = Spawn(*FString::Printf(TEXT("Ring2_OuterOrb_%g"), Y), C + FVector(0, 0, R2Z - 8.f + 40.f)))
		{
			if (SphereM) if (UStaticMeshComponent* V = Orb->AddVisual(SphereM, 0.8f, false)) V->SetMaterial(0, OrbGlow);
			FZoogiObsTrigger& T = Orb->Triggers.AddDefaulted_GetRef();
			T.Kind = 3; T.Shape = 1; T.E = FVector(40.f, 0.f, 0.f); T.P[0] = 10.f; T.P[1] = 12.f; // Episode Energy +10, recharge 12 s
		}
	}
	// ---- 6 crystal bumpers r1905 (collider r35, crystal h120; bounce gain like the pack bumpers)
	for (float Y : BumperYaw)
	{
		const FVector C = P(1905.f, Y, R2Z);
		if (AZoogiObstacle* O = Spawn(*FString::Printf(TEXT("Ring2_CrystalBumper_%g"), Y), C))
		{
			TArray<TArray<FVector>> H; TArray<FVector>& Pts = H.AddDefaulted_GetRef();
			for (int32 k = 0; k < 12; ++k) { const float A = k * PI / 6.f; for (float Z : { 0.f, 120.f }) Pts.Add(FVector(FMath::Cos(A) * 35.f, FMath::Sin(A) * 35.f, Z)); }
			O->BuildCollider(H, 1.f, BumpPM);
			O->BounceGain = 1.2f; O->MaxOut = 1400.f;
			FZoogiMeshBuilder Cr; Cr.Prism(FVector2D(C), 6, 38.f, 8.f, R2Z, R2Z + 120.f, Y, 0.f, false);
			Vis(Cr, CrystalGlow);
			SolidPosts.Add(FVector2D(C));
		}
	}
	// ---- 12 curved deflector wings: r1995, +-150 lateral from each bridge axis, face 35 deg to the axis, 150 long, h45
	{
		TArray<TArray<FVector>> H; FZoogiMeshBuilder D;
		for (float Y : BridgeYaw)
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Rad = P(1.f, Y, 0.f), Tan = P(1.f, Y + 90.f, 0.f);
				const FVector C = Rad * 1995.f + Tan * (150.f * s) + FVector(0, 0, R2Z + 22.5f);
				const FRotator R(0.f, Y + 35.f * s, 0.f);
				for (int32 k = -1; k <= 1; ++k) // 3 facets bent along a 200 radius
				{
					const FRotator Rk(0.f, R.Yaw + k * 14.f * s, 0.f);
					const FVector Ck = C + R.RotateVector(FVector(k * 50.f, (k != 0 ? 6.f : 0.f) * s, 0.f));
					BoxHull(H, Ck, FVector(26.f, 10.f, 22.5f), Rk);
					D.Box(Ck, FVector(26.f, 10.f, 22.5f), Rk);
				}
			}
		if (AZoogiObstacle* O = Spawn(TEXT("Ring2_DeflectorWings"))) O->BuildCollider(H, 1.f, DeflPM);
		Vis(D, IcePane);
	}
	// ---- 6 re-entry ramps: Ring 2 inner edge r1765 (z -30) -> Ring 1 edge r1520 (z 0), half width 85, side curbs h20
	{
		TArray<TArray<FVector>> H; FZoogiMeshBuilder Rm, Cb;
		for (float Y : RampYaw)
		{
			const float L = 1765.f - 1520.f, Pitch = FMath::RadiansToDegrees(FMath::Atan2(30.f, L));
			const FVector C = P(0.5f * (1765.f + 1520.f), Y, -15.f - 6.f);
			const FRotator R(-Pitch, Y, 0.f); // nose down outward (+X = outward, higher inside)
			BoxHull(H, C, FVector(L * 0.5f + 10.f, 85.f, 6.f), R);
			Rm.Box(C, FVector(L * 0.5f + 10.f, 85.f, 6.f), R);
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector Cc = C + R.RotateVector(FVector(0.f, (85.f + 5.f) * s, 16.f));
				BoxHull(H, Cc, FVector(L * 0.5f, 5.f, 10.f), R);
				Cb.Box(Cc, FVector(L * 0.5f, 5.f, 10.f), R);
			}
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ring2_ReentryRamps"))) O->BuildCollider(H, 1.f, FloorPMr);
		Vis(Rm, Ice); Vis(Cb, Snow);
	}
	UE_LOG(LogZoogi, Log, TEXT("Frozen Ring 2: floor r%.0f-%.0f z%.0f, crevasse r%.0f-%.0f, 6 pockets/orbs, 6 crystal bumpers, 12 deflector wings, 6 ramps; ring-out r>%.0f or fall"),
		R2In, R2Out, R2Z, CrevIn, R2In, OutR);
}

bool AZoogiArena::IsRingOut(const FVector& L)
{
	if (L.Z < ZoogiConst::FallOutZ || FVector2D(L.X, L.Y).Size() > 8000.f) return true;
	return OuterOutR > 0.f && FVector2D(L.X, L.Y).Size() > OuterOutR;
}
