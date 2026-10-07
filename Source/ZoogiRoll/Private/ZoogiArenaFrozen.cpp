// Frozen Ring (map id "ice"), rebuilt Oct 6 2026 to match Evan's Buzzy Frozen Ring art.
// Court: the Buzzy top-down rectified onto the floor (T_FrozenFloor / T_FrozenGlow, Scripts/arenas/gen_frozen.py).
// Every stage piece is an AZoogiArenaPiece (destructible, power-reactive) except the 4 Resonance Hub pillars
// (indestructible, they rise from under the floor at match start). UZoogiStageForming assembles the stage (~3 s).
// Layout constants: ZoogiFrozenLayout.h (shared with the 2v2 combo props).
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiFloorScars.h"
#include "ZoogiFrozenPlayTest.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiMeshKit.h"
#include "ZoogiRock.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

using namespace ZoogiFrozen;

namespace
{
	FVector Pol(float R, float YawDeg, float Z = 0.f)
	{
		const float A = FMath::DegreesToRadians(YawDeg);
		return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
	}
	UTexture* FTex(const TCHAR* Name)
	{
		return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

UMaterialInterface* AZoogiArena::FrozenMat(FName Key) const
{
	if (const TObjectPtr<UMaterialInterface>* M = FrozenMats.Find(Key)) return M->Get();
	return nullptr;
}

UMaterialInterface* AZoogiArena::MakeFrozenMat(FName Key, const FLinearColor& Color, const TCHAR* Detail, float DetailScale, float DetailStrength,
	float Roughness, float Specular, const FLinearColor& Emissive, float EmissiveStrength)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenPiece.M_FrozenPiece"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Out = nullptr;
	if (Base)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		if (UTexture* T = FTex(Detail)) M->SetTextureParameterValue(TEXT("Detail"), T);
		if (UTexture* C = FTex(TEXT("T_FrozenCrackTile"))) M->SetTextureParameterValue(TEXT("Crack"), C);
		M->SetScalarParameterValue(TEXT("DetailScale"), DetailScale);
		M->SetScalarParameterValue(TEXT("DetailStrength"), DetailStrength);
		M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		M->SetScalarParameterValue(TEXT("Specular"), Specular);
		M->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
		M->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
		Out = M;
	}
	else
	{
		Out = ZoogiUtil::SurfaceMID(this, Color, Roughness, Emissive, EmissiveStrength);
	}
	FrozenMats.Add(Key, Out);
	return Out;
}

AZoogiArenaPiece* AZoogiArena::SpawnPiece(const FVector& Loc, float Yaw, FName Kind, FName Surface, float HP, bool bIndestructible)
{
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiArenaPiece* P = GetWorld()->SpawnActor<AZoogiArenaPiece>(AZoogiArenaPiece::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Loc * BuildScale, FVector(BuildScale)), SP); // r19: per-map scale
	if (!P) return nullptr;
	P->Setup(Kind, Surface, HP, bIndestructible);
	Pieces.Add(P);
	return P;
}

void AZoogiArena::ResetPieces()
{
	for (AZoogiArenaPiece* P : Pieces) if (IsValid(P)) P->ResetPiece();
	if (Scars) Scars->ResetScars();
}

void AZoogiArena::BeginForming(float TimeScale)
{
	ResetPieces();
	if (Forming && Forming->Num() > 0) Forming->Begin(TimeScale);
}

bool AZoogiArena::IsForming() const
{
	return Forming && Forming->IsPlaying();
}

USceneComponent* AZoogiArena::NewPivot(const FVector& Loc)
{
	USceneComponent* P = NewObject<USceneComponent>(this);
	P->SetMobility(EComponentMobility::Movable);
	P->SetupAttachment(Root);
	P->RegisterComponent();
	P->SetRelativeLocation(Loc);
	Built.Add(P);
	return P;
}

UProceduralMeshComponent* AZoogiArena::AddFrozenProc(const FZoogiMeshBuilder& M, UMaterialInterface* Mat, USceneComponent* Parent, bool bShadow)
{
	UProceduralMeshComponent* C = NewProcMesh(bShadow, false);
	if (Parent) C->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
	M.Upload(C, 0, false);
	C->SetMaterial(0, Mat);
	return C;
}

// ------------------------------------------------------------------ build

void AZoogiArena::BuildFrozen()
{
	FRandomStream Rng(88);
	ExtraOrbSpots.Reset();
	// ---- materials (Buzzy palette: ice #7FC4EA / #BFE6F7, snow #F4FBFF, stone #CACFDC, glow #19C3E6) ----
	const FLinearColor Cyan = ZoogiUtil::Hex(TEXT("#19C3E6"));
	UMaterialInterface* Ice = MakeFrozenMat(TEXT("Ice"), ZoogiUtil::Hex(TEXT("#8FD3F4")), TEXT("T_FrozenIceTile"), 180.f, 0.5f, 0.18f, 0.75f, ZoogiUtil::Hex(TEXT("#4FC9F0")), 0.12f);
	UMaterialInterface* IceDeep = MakeFrozenMat(TEXT("IceDeep"), ZoogiUtil::Hex(TEXT("#74B4E4")), TEXT("T_FrozenIceTile"), 260.f, 0.5f, 0.25f, 0.7f, ZoogiUtil::Hex(TEXT("#2A7FC0")), 0.08f);
	UMaterialInterface* IcePane = MakeFrozenMat(TEXT("IcePane"), ZoogiUtil::Hex(TEXT("#A9E2FA")), TEXT("T_FrozenIceTile"), 140.f, 0.35f, 0.08f, 0.9f, ZoogiUtil::Hex(TEXT("#6FDcff")), 0.3f);
	UMaterialInterface* Snow = MakeFrozenMat(TEXT("Snow"), ZoogiUtil::Hex(TEXT("#F2F8FF")), TEXT("T_FrozenSnowTile"), 240.f, 0.35f, 0.8f, 0.35f, FLinearColor(0.85f, 0.93f, 1.f), 0.04f);
	UMaterialInterface* SnowShade = MakeFrozenMat(TEXT("SnowShade"), ZoogiUtil::Hex(TEXT("#C9DDEE")), TEXT("T_FrozenSnowTile"), 240.f, 0.35f, 0.85f, 0.3f, FLinearColor::Black, 0.f);
	UMaterialInterface* Stone = MakeFrozenMat(TEXT("Stone"), ZoogiUtil::Hex(TEXT("#D5DEEA")), TEXT("T_FrozenStoneTile"), 120.f, 0.55f, 0.6f, 0.45f, FLinearColor::Black, 0.f);
	UMaterialInterface* StoneDark = MakeFrozenMat(TEXT("StoneDark"), ZoogiUtil::Hex(TEXT("#8E9DB4")), TEXT("T_FrozenStoneTile"), 120.f, 0.55f, 0.7f, 0.4f, FLinearColor::Black, 0.f);
	UMaterialInterface* Wood = MakeFrozenMat(TEXT("Wood"), ZoogiUtil::Hex(TEXT("#CACFDC")), TEXT("T_FrozenStoneTile"), 60.f, 0.6f, 0.75f, 0.3f, FLinearColor::Black, 0.f);
	UMaterialInterface* WoodDark = MakeFrozenMat(TEXT("WoodDark"), ZoogiUtil::Hex(TEXT("#8E95A8")), TEXT("T_FrozenStoneTile"), 60.f, 0.6f, 0.8f, 0.3f, FLinearColor::Black, 0.f);
	// Round 3: paler icy cliffs / underside (the art's white-cyan ice, not dark saturated blue).
	UMaterialInterface* Cliff = MakeFrozenMat(TEXT("Cliff"), ZoogiUtil::Hex(TEXT("#D3ECFA")), TEXT("T_FrozenIceTile"), 420.f, 0.55f, 0.4f, 0.5f, ZoogiUtil::Hex(TEXT("#8FD0F2")), 0.10f);
	UMaterialInterface* IceUnder = MakeFrozenMat(TEXT("IceUnder"), ZoogiUtil::Hex(TEXT("#C6E6F8")), TEXT("T_FrozenIceTile"), 300.f, 0.45f, 0.3f, 0.6f, ZoogiUtil::Hex(TEXT("#9FDCFF")), 0.12f);
	// Mountain rock: blue-grey anime cliff faces under the snow (slope-split by BuildFrozenPeaks).
	UMaterialInterface* Peak = MakeFrozenMat(TEXT("Peak"), ZoogiUtil::Hex(TEXT("#8FA9D2")), TEXT("T_FrozenStoneTile"), 1800.f, 0.35f, 0.85f, 0.25f, FLinearColor(0.45f, 0.6f, 0.85f), 0.10f);
	UMaterialInterface* Rune = ZoogiUtil::GlowMID(this, Cyan, 3.f);
	UMaterialInterface* RuneSoft = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.6f);
	FrozenMats.Add(TEXT("Rune"), Rune);
	UMaterialInterface* Bronze = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8A5A2E")), 0.35f, FLinearColor::Black, 0.f, 0.85f);
	UPhysicalMaterial* WallPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.9f, false);
	UPhysicalMaterial* StonePM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.35f, false);
	UPhysicalMaterial* PillarPM = ZoogiPhysics::MakeMaterial(this, 0.25f, 0.6f, false);
	SpawnRadiusOverride = SpawnR;

	// ---- floor (art-derived) + island ----
	UMaterialInterface* FloorMat = KitFloorMaterial(TEXT("Frozen"), Cyan, 1.6f, 1.0f);
	FloorMID = Cast<UMaterialInstanceDynamic>(FloorMat);
	FloorGlowTarget = 2.5f; // round 3: clean rune geometry in T_FrozenGlow, brighter (the art's glowing swirl / rune floor)
	if (FloorMID)
	{
		FloorMID->SetScalarParameterValue(TEXT("GlowTint"), 0.35f);
		FloorMID->SetScalarParameterValue(TEXT("DetailStrength"), 0.12f);
		FloorMID->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
		FloorMID->SetScalarParameterValue(TEXT("Specular"), 0.6f);
		FloorMID->SetScalarParameterValue(TEXT("Pulse"), 0.12f);
	}
	BuildIsland(ZoogiUtil::Hex(TEXT("#CFEFFF")), ZoogiUtil::Hex(TEXT("#9ED8F5")), ZoogiUtil::Hex(TEXT("#F4FBFF")),
		ZoogiUtil::Hex(TEXT("#C4E2F5")), ZoogiUtil::Hex(TEXT("#E6F4FC")), 0.03f, FloorMat);
	AddPanoramaSky(TEXT("Frozen"), 1.0f, 0.f, FLinearColor::White);

	// Snow lip (outside the knockout line, top below the court at the line so balls roll off as before) + ice cliff face.
	{
		FZoogiMeshBuilder SnowM, CliffM;
		const int32 Seg = 144;
		auto H = [&](float R, float A) -> float
		{
			const float N = FMath::PerlinNoise2D(FVector2D(FMath::Cos(A) * 3.1f, FMath::Sin(A) * 3.1f)) * 0.5f + 0.5f;
			if (R < 1560.f) return -4.f;
			// Round 3c: the snow lip dips under each bridge (the deck runs from the court edge down to the islet).
			if (NearIsletYaw(FMath::RadiansToDegrees(A), 5.f)) return -4.f - 0.3f * (R - 1550.f);
			if (R < 1650.f) return FMath::Lerp(-4.f, 34.f + 26.f * N, FMath::SmoothStep(0.f, 1.f, (R - 1560.f) / 90.f));
			return FMath::Lerp(34.f + 26.f * N, -18.f, FMath::SmoothStep(0.f, 1.f, (R - 1650.f) / 130.f));
		};
		const float Rs[5] = { 1553.f, 1600.f, 1650.f, 1720.f, 1780.f };
		for (int32 s = 0; s < Seg; ++s)
		{
			const float A0 = 2.f * PI * s / Seg, A1 = 2.f * PI * (s + 1) / Seg;
			for (int32 j = 0; j < 4; ++j)
			{
				const FVector P00(FMath::Cos(A0) * Rs[j], FMath::Sin(A0) * Rs[j], H(Rs[j], A0)), P01(FMath::Cos(A1) * Rs[j], FMath::Sin(A1) * Rs[j], H(Rs[j], A1));
				const FVector P10(FMath::Cos(A0) * Rs[j + 1], FMath::Sin(A0) * Rs[j + 1], H(Rs[j + 1], A0)), P11(FMath::Cos(A1) * Rs[j + 1], FMath::Sin(A1) * Rs[j + 1], H(Rs[j + 1], A1));
				SnowM.Quad(P00, P01, P11, P10, FVector::UpVector);
			}
			// Cliff: jagged ice face from the lip down to -320.
			const float J0 = 1780.f + 30.f * FMath::PerlinNoise1D(A0 * 9.f), J1 = 1780.f + 30.f * FMath::PerlinNoise1D(A1 * 9.f);
			const float B0 = 1700.f + 60.f * FMath::PerlinNoise1D(A0 * 5.f + 3.f), B1 = 1700.f + 60.f * FMath::PerlinNoise1D(A1 * 5.f + 3.f);
			const FVector T0(FMath::Cos(A0) * 1780.f, FMath::Sin(A0) * 1780.f, H(1780.f, A0)), T1(FMath::Cos(A1) * 1780.f, FMath::Sin(A1) * 1780.f, H(1780.f, A1));
			const FVector M0(FMath::Cos(A0) * J0, FMath::Sin(A0) * J0, -260.f), M1(FMath::Cos(A1) * J1, FMath::Sin(A1) * J1, -260.f);
			const FVector D0(FMath::Cos(A0) * B0, FMath::Sin(A0) * B0, -640.f), D1(FMath::Cos(A1) * B1, FMath::Sin(A1) * B1, -640.f);
			const FVector Out(FMath::Cos((A0 + A1) * 0.5f), FMath::Sin((A0 + A1) * 0.5f), 0.f);
			CliffM.Quad(T0, T1, M1, M0, Out);
			CliffM.Quad(M0, M1, D1, D0, Out - FVector(0, 0, 0.3f));
		}
		AddFrozenProc(SnowM, Snow, nullptr, true);
		AddFrozenProc(CliffM, Cliff, nullptr, true);
		// Faceted ice underside chunks + icicles hanging from the rim.
		FZoogiMeshBuilder Under, Icicles;
		for (int32 i = 0; i < 34; ++i)
		{
			const float A = i * 360.f / 34 + Rng.FRandRange(-5.f, 5.f);
			const float R = Rng.FRandRange(1350.f, 1700.f);
			Under.Chunk(Pol(R, A, Rng.FRandRange(-820.f, -520.f)), FVector(Rng.FRandRange(220.f, 380.f), Rng.FRandRange(220.f, 380.f), Rng.FRandRange(260.f, 420.f)), Rng, 0.3f, A);
		}
		for (int32 i = 0; i < 150; ++i)
		{
			// Round 3: denser, longer icicle fringe under the cliff lip + a second row under the faceted underside.
			const bool bInner = i >= 100;
			const float A = (bInner ? (i - 100) * 360.f / 50 : i * 360.f / 100) + Rng.FRandRange(-2.f, 2.f);
			const float R = bInner ? Rng.FRandRange(1250.f, 1550.f) : Rng.FRandRange(1690.f, 1775.f);
			const float L = bInner ? Rng.FRandRange(260.f, 620.f) : Rng.FRandRange(120.f, 480.f);
			const float Z0 = bInner ? -900.f - Rng.FRandRange(0.f, 120.f) : -600.f;
			const FVector2D XY(Pol(R, A));
			Icicles.Prism(XY, 5, Rng.FRandRange(26.f, 52.f) * (bInner ? 1.5f : 1.f), 0.f, Z0, Z0 - L * 1.4f, A, 0.f, false);
		}
		// Snow drapes on the cliff face (white tongues over the ice, the art's snow-covered rim).
		FZoogiMeshBuilder Drapes;
		for (int32 i = 0; i < 40; ++i)
		{
			const float A = i * 9.f + Rng.FRandRange(-3.f, 3.f);
			if (NearIsletYaw(A, 6.f)) continue;
			Drapes.Chunk(Pol(1790.f, A, -60.f - Rng.FRandRange(0.f, 60.f)), FVector(Rng.FRandRange(25.f, 40.f), Rng.FRandRange(70.f, 140.f), Rng.FRandRange(60.f, 130.f)), Rng, 0.15f, A);
		}
		AddFrozenProc(Drapes, Snow, nullptr, true);
		AddFrozenProc(Under, IceUnder, nullptr, true);
		AddFrozenProc(Icicles, IcePane, nullptr, false);
		// Round 2: lumpy snow mounds along the lip (the art's thick snow rim), no collision.
		FZoogiMeshBuilder Mounds;
		for (int32 i = 0; i < 64; ++i)
		{
			const float A = i * 360.f / 64 + Rng.FRandRange(-2.5f, 2.5f);
			if (NearIsletYaw(A, 6.f)) continue;
			const float R = Rng.FRandRange(1645.f, 1715.f);
			Mounds.Chunk(Pol(R, A, Rng.FRandRange(18.f, 30.f)), FVector(Rng.FRandRange(55.f, 85.f), Rng.FRandRange(80.f, 130.f), Rng.FRandRange(22.f, 40.f)), Rng, 0.18f, A);
		}
		AddFrozenProc(Mounds, Snow, nullptr, true);
	}

	// ---- Round 2: glowing boundary ring + hub rune circles (flat emissive strips just above the floor, no collision) ----
	{
		auto Ring = [](FZoogiMeshBuilder& M, float R0, float R1, float Z, int32 Seg)
		{
			for (int32 s = 0; s < Seg; ++s)
			{
				const float A0 = 2.f * PI * s / Seg, A1 = 2.f * PI * (s + 1) / Seg;
				const FVector P00(FMath::Cos(A0) * R0, FMath::Sin(A0) * R0, Z), P01(FMath::Cos(A1) * R0, FMath::Sin(A1) * R0, Z);
				const FVector P10(FMath::Cos(A0) * R1, FMath::Sin(A0) * R1, Z), P11(FMath::Cos(A1) * R1, FMath::Sin(A1) * R1, Z);
				M.Quad(P00, P01, P11, P10, FVector::UpVector);
			}
		};
		FZoogiMeshBuilder Bound, BoundSoft, HubRunes;
		Ring(Bound, 1514.f, 1556.f, 0.9f, 192);
		// Round 3: soft additive halo around the boundary (the art's bright glowing ice ring), stepped gradient.
		FZoogiMeshBuilder Halo0, Halo1;
		Ring(Halo0, 1480.f, 1600.f, 1.1f, 192);
		Ring(Halo1, 1440.f, 1650.f, 1.2f, 192);
		for (UProceduralMeshComponent* Hc : { AddFrozenProc(Halo0, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 0.55f), nullptr, false),
			AddFrozenProc(Halo1, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#5FD8FF")), 0.25f), nullptr, false) })
			if (Hc) Hc->ComponentTags.Add(TEXT("NoCamBlock"));
		Ring(BoundSoft, 1490.f, 1497.f, 0.7f, 192);
		Ring(HubRunes, 146.f, 154.f, 0.6f, 64);
		Ring(HubRunes, 326.f, 336.f, 0.6f, 96);
		Ring(HubRunes, 596.f, 604.f, 0.6f, 128);
		for (int32 k = 0; k < 4; ++k)
		{
			// Four short spokes linking the rune circles (between the pillars and the obelisks).
			const float A = FMath::DegreesToRadians(k * 90.f);
			const FVector Dv(FMath::Cos(A), FMath::Sin(A), 0.f), Nv(-FMath::Sin(A), FMath::Cos(A), 0.f);
			const FVector P0 = Dv * 336.f + FVector(0, 0, 0.6f), P1 = Dv * 596.f + FVector(0, 0, 0.6f);
			HubRunes.Quad(P0 - Nv * 4.f, P1 - Nv * 4.f, P1 + Nv * 4.f, P0 + Nv * 4.f, FVector::UpVector);
		}
		UProceduralMeshComponent* B0 = AddFrozenProc(Bound, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 3.4f), nullptr, false);
		UProceduralMeshComponent* B1 = AddFrozenProc(BoundSoft, RuneSoft, nullptr, false);
		UProceduralMeshComponent* B2 = AddFrozenProc(HubRunes, ZoogiUtil::GlowMID(this, Cyan, 2.4f), nullptr, false);
		for (UProceduralMeshComponent* C : { B0, B1, B2 }) if (C) C->ComponentTags.Add(TEXT("NoCamBlock"));
	}

	// ---- distant snow valley + mountain ranges (Buzzy: white peaks with blue shading, cloud sea under the island) ----
	{
		// Snow valley far below (fills the view under the island with bright snow instead of void).
		FZoogiMeshBuilder Valley;
		const int32 NR = 14, NA = 48;
		auto VZ = [](float R, float A) { return -5200.f + 900.f * FMath::PerlinNoise2D(FVector2D(FMath::Cos(A) * R * 0.00012f, FMath::Sin(A) * R * 0.00012f)) + (R > 16000.f ? (R - 16000.f) * 0.12f : 0.f); };
		for (int32 r = 0; r < NR; ++r)
			for (int32 a = 0; a < NA; ++a)
			{
				const float R0 = 2000.f + r * 3000.f, R1 = R0 + 3000.f;
				const float A0 = 2.f * PI * a / NA, A1 = 2.f * PI * (a + 1) / NA;
				const FVector P00(FMath::Cos(A0) * R0, FMath::Sin(A0) * R0, VZ(R0, A0)), P01(FMath::Cos(A1) * R0, FMath::Sin(A1) * R0, VZ(R0, A1));
				const FVector P10(FMath::Cos(A0) * R1, FMath::Sin(A0) * R1, VZ(R1, A0)), P11(FMath::Cos(A1) * R1, FMath::Sin(A1) * R1, VZ(R1, A1));
				Valley.Quad(P00, P01, P11, P10, FVector::UpVector);
			}
		AddFrozenProc(Valley, SnowShade, nullptr, false);
		// Round 3: jagged anime snow peaks (ridged faceted cones, slope-split snow / blue-grey rock), a far horizon range and a
		// nearer range rising out of the valley around the island (the art's mountain ring under / behind the arena).
		FZoogiMeshBuilder Rock, Caps;
		auto Mountain = [&](const FVector2D& Cc, float Rb, float H, float Z0)
		{
			const int32 NR = 9, NS = 17; // round 3c: finer, more jagged ridges (no pyramid look)
			TArray<FVector> Vt;
			Vt.SetNum(NR * NS + 1);
			const FVector2D Lean(Rng.FRandRange(-0.12f, 0.12f) * Rb, Rng.FRandRange(-0.12f, 0.12f) * Rb);
			Vt[0] = FVector(Cc + Lean, Z0 + H * Rng.FRandRange(1.f, 1.08f));
			const float Ph = Rng.FRandRange(0.f, 2.f * PI);
			for (int32 i = 1; i <= NR; ++i)
				for (int32 j = 0; j < NS; ++j)
				{
					const float T = (float)i / NR;
					const float A = Ph + 2.f * PI * (j + Rng.FRandRange(-0.3f, 0.3f)) / NS;
					const bool bRidge = (j % 3) == 0 || ((j % 3) == 2 && (j * 7 + i) % 4 == 0);
					const float Rr = Rb * T * (bRidge ? Rng.FRandRange(1.05f, 1.18f) : Rng.FRandRange(0.68f, 0.9f));
					float Hh = H * FMath::Pow(1.f - T, 1.6f) * (bRidge ? 1.14f : Rng.FRandRange(0.66f, 0.9f)) + Rng.FRandRange(-0.09f, 0.09f) * H * (1.f - T) * T;
					if (i == NR) Hh = -0.25f * H;
					Vt[1 + (i - 1) * NS + j] = FVector(Cc + Lean * (1.f - T) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr, Z0 + Hh);
				}
			const FVector Ctr(Cc, Z0 + H * 0.3f);
			auto Face = [&](const FVector& P0, const FVector& P1, const FVector& P2)
			{
				const FVector Cen = (P0 + P1 + P2) / 3.f;
				FVector Nn = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
				const FVector Out = (Cen - Ctr).GetSafeNormal() + FVector(0, 0, 0.3f);
				if (FVector::DotProduct(Nn, Out) < 0.f) Nn = -Nn;
				const float Rel = (Cen.Z - Z0) / H;
				const bool bSnow = Nn.Z > 0.62f || (Rel > 0.55f && Nn.Z > 0.32f) || (Rel > 0.82f);
				(bSnow ? Caps : Rock).FlatTri(P0, P1, P2, Out);
			};
			for (int32 j = 0; j < NS; ++j)
			{
				const int32 J1 = (j + 1) % NS;
				Face(Vt[0], Vt[1 + j], Vt[1 + J1]);
				for (int32 i = 1; i < NR; ++i)
				{
					const FVector& A0 = Vt[1 + (i - 1) * NS + j]; const FVector& A1 = Vt[1 + (i - 1) * NS + J1];
					const FVector& B0 = Vt[1 + i * NS + j]; const FVector& B1 = Vt[1 + i * NS + J1];
					Face(A0, B0, B1);
					Face(A0, B1, A1);
				}
			}
		};
		auto Cluster = [&](float R, float A, float H, float Z0)
		{
			const FVector2D C0(Pol(R, A));
			Mountain(C0, H * Rng.FRandRange(0.5f, 0.62f), H, Z0);
			const int32 Ns = Rng.RandRange(2, 4);
			for (int32 c = 0; c < Ns; ++c)
			{
				const float Hc = H * Rng.FRandRange(0.45f, 0.75f);
				const FVector2D O = C0 + FVector2D(Pol(H * Rng.FRandRange(0.4f, 0.75f), A + Rng.FRandRange(50.f, 130.f) * (c % 2 ? 1.f : -1.f)));
				Mountain(O, Hc * Rng.FRandRange(0.55f, 0.7f), Hc, Z0);
			}
		};
		for (int32 i = 0; i < 34; ++i)
		{
			const float A = i * 360.f / 34 + Rng.FRandRange(-4.f, 4.f);
			const bool bFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 50.f; // camera side (art bottom): lower
			Cluster(Rng.FRandRange(22000.f, 34000.f) * (bFront ? 1.15f : 1.f), A, Rng.FRandRange(5200.f, 8800.f) * (bFront ? 0.55f : 1.f), -5600.f);
		}
		// Round 3c: a middle range between the far horizon and the near ring (layered ranges like the art).
		for (int32 i = 0; i < 26; ++i)
		{
			const float A = i * 360.f / 26 + Rng.FRandRange(-5.f, 5.f);
			const bool bFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 45.f;
			Cluster(Rng.FRandRange(14000.f, 19000.f), A, Rng.FRandRange(4200.f, 6800.f) * (bFront ? 0.55f : 1.f), -5600.f);
		}
		for (int32 i = 0; i < 20; ++i)
		{
			const float A = i * 18.f + Rng.FRandRange(-5.f, 5.f);
			const bool bFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 40.f;
			Cluster(Rng.FRandRange(6800.f, 11000.f), A, Rng.FRandRange(3200.f, 5200.f) * (bFront ? 0.6f : 1.f), -5600.f);
		}
		AddFrozenProc(Rock, Peak, nullptr, false);
		AddFrozenProc(Caps, Snow, nullptr, false);
	}

	// ---- Round 3: cloud sea (soft anime cloud puffs drifting below the island and around the peaks) ----
	UMaterialInterface* DecalBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenDecal.M_FrozenDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto FlatQuad = [&](UTexture* Tex, const FVector& Loc, float Yaw, float Size, float Opacity, float Emis, float Bright, const FLinearColor& Tint, UMaterialInstanceDynamic** OutMID) -> UStaticMeshComponent*
	{
		if (!DecalBase || !Tex) return nullptr;
		UStaticMeshComponent* Q = NewObject<UStaticMeshComponent>(this);
		Q->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane")));
		Q->SetMobility(EComponentMobility::Movable);
		Q->SetupAttachment(Root);
		Q->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Q->SetCastShadow(false);
		Q->SetReceivesDecals(false);
		Q->ComponentTags.Add(TEXT("NoCamBlock"));
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(DecalBase, this);
		D->SetTextureParameterValue(TEXT("Tex"), Tex);
		D->SetVectorParameterValue(TEXT("Tint"), Tint);
		D->SetScalarParameterValue(TEXT("Bright"), Bright);
		D->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		D->SetScalarParameterValue(TEXT("Emissive"), Emis);
		Q->SetMaterial(0, D);
		Q->RegisterComponent();
		Q->SetRelativeTransform(FTransform(FRotator(0.f, Yaw, 0.f), Loc, FVector(Size / 100.f, Size / 100.f, 1.f)));
		Built.Add(Q);
		if (OutMID) *OutMID = D;
		return Q;
	};
	if (UTexture* Cloud = FTex(TEXT("T_FrozenCloud")))
	{
		for (int32 i = 0; i < 90; ++i)
		{
			const bool bNear = i < 24;
			const float A = Rng.FRandRange(0.f, 360.f);
			const float R = bNear ? Rng.FRandRange(1500.f, 3200.f) : Rng.FRandRange(3000.f, 17000.f);
			const float Z = bNear ? Rng.FRandRange(-1700.f, -1100.f) : Rng.FRandRange(-3600.f, -2300.f);
			const float Sz = bNear ? Rng.FRandRange(1600.f, 2600.f) : Rng.FRandRange(3000.f, 7500.f);
			FlatQuad(Cloud, Pol(R, A, Z), Rng.FRandRange(0.f, 360.f), Sz, bNear ? 0.85f : 0.95f, 0.18f, 1.05f, FLinearColor::White, nullptr);
		}
	}

	// ---- Resonance Hub: 4 indestructible pillars with Awaken Orbs (rise from below the floor) ----
	{
		const FLinearColor OrbCol[4] = { FLinearColor(1.f, 0.16f, 0.02f), FLinearColor(0.05f, 0.9f, 0.75f), FLinearColor(0.7f, 0.85f, 1.f), FLinearColor(1.f, 0.55f, 0.05f) };
		const TCHAR* OrbName[4] = { TEXT("Ember"), TEXT("Frost"), TEXT("Storm"), TEXT("Titan") };
		for (int32 k = 0; k < 4; ++k)
		{
			const float Yaw = PillarYaw0 + 90.f * k;
			AZoogiArenaPiece* P = SpawnPiece(Pol(HubR, Yaw), Yaw, *FString::Printf(TEXT("pillar_%s"), OrbName[k]), TEXT("Stone"), 1000.f, true);
			if (!P) continue;
			P->AddBox(FVector(0, 0, 14.f), FVector(122.f, 122.f, 28.f), FRotator::ZeroRotator, StoneDark);
			P->AddBox(FVector(0, 0, 40.f), FVector(98.f, 98.f, 24.f), FRotator::ZeroRotator, Stone);
			FZoogiMeshBuilder Shaft;
			Shaft.Prism(FVector2D::ZeroVector, 4, 46.f, 39.f, 52.f, PillarH - 18.f, 45.f, 0.f, false);
			P->AddProc(Shaft, Stone);
			P->AddBox(FVector(0, 0, PillarH - 10.f), FVector(80.f, 80.f, 18.f), FRotator::ZeroRotator, Stone);
			// Snow on the plinth top and cap.
			P->AddBox(FVector(0, 0, 53.f), FVector(90.f, 90.f, 4.f), FRotator(0.f, 0.f, 0.f), Snow, false);
			for (int32 f = 0; f < 4; ++f)
			{
				const FRotator Fr(0.f, f * 90.f, 0.f);
				for (int32 s = -1; s <= 1; s += 2)
				{
					UStaticMeshComponent* Slit = P->AddBox(Fr.RotateVector(FVector(35.5f, s * 12.f, 0.f)) + FVector(0, 0, PillarH * 0.5f + 10.f), FVector(2.f, 7.f, PillarH * 0.56f), Fr, Rune, false);
					P->MarkGlow(Slit);
				}
				P->AddBox(Fr.RotateVector(FVector(37.f, 0.f, 0.f)) + FVector(0, 0, PillarH - 40.f), FVector(1.5f, 18.f, 14.f), Fr, StoneDark, false); // glyph
			}
			// Bronze claw cup + Awaken Orb.
			P->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 12.f), FVector(0.34f, 0.34f, 0.24f)), Bronze);
			P->AddShape(TEXT("Cone"), FTransform(FRotator(180.f, 0.f, 0.f), FVector(0, 0, PillarH + 44.f), FVector(0.98f, 0.98f, 0.42f)), Bronze);
			UStaticMeshComponent* Orb = P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(0, 0, PillarH + 84.f), FVector(0.88f, 0.88f, 0.98f)), ZoogiUtil::GlowMID(this, OrbCol[k], k == 2 ? 2.2f : 3.2f), false);
			P->MarkGlow(Orb);
			UPointLightComponent* L = NewObject<UPointLightComponent>(P);
			L->SetupAttachment(P->Root);
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(140.f);
			L->SetLightColor(OrbCol[k]);
			L->SetAttenuationRadius(420.f);
			L->SetCastShadows(false);
			L->RegisterComponent();
			L->SetRelativeLocation(FVector(0, 0, PillarH + 84.f));
			P->AddColliderBox(FVector(0, 0, 26.f), FVector(110.f, 110.f, 52.f), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
			P->AddColliderBox(FVector(0, 0, PillarH * 0.5f), FVector(PillarHalf * 2.f, PillarHalf * 2.f, PillarH), FRotator::ZeroRotator, TEXT("ZPillar"), PillarPM);
			P->SetPathBlock(FVector2D(P->GetActorLocation()), 60.f);
			P->FinishBuild();
			// Climax of the intro (animations pack forming_spec "Resonance pillars"): break-throughs staggered 0.1 s from 1.3 s,
			// +14 cm overshoot, settled by ~2.6 s, just before the rune floor ignites.
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(PillarH + 120.f)), 1.30f + 0.10f * k, 1.0f, 14.f);
			// Round 3 (Animations & Powers pack): floor-break crack ring, ice / snow burst and rune glow ring at the break-through
			// (1.45 / 1.55 / 1.65 / 1.75 s). Flat, no collision, invisible outside the forming (opacity pulses end at 0).
			if (Forming)
			{
				const float Bt = 1.45f + 0.10f * k;
				UMaterialInstanceDynamic* Mb = nullptr; UMaterialInstanceDynamic* Mu = nullptr; UMaterialInstanceDynamic* Mr = nullptr;
				FlatQuad(FTex(TEXT("T_FrozenDisp_Break")), Pol(HubR, Yaw, 1.5f), Yaw, 360.f, 0.f, 1.2f, 1.f, FLinearColor::White, &Mb);
				UStaticMeshComponent* Bq = FlatQuad(FTex(TEXT("T_FrozenDisp_Burst")), Pol(HubR, Yaw, 2.5f), Yaw * 3.f, 620.f, 0.f, 0.25f, 1.f, FLinearColor::White, &Mu);
				FlatQuad(FTex(TEXT("T_FrozenDisp_RuneRing")), Pol(HubR, Yaw, 2.0f), 0.f, 420.f, 0.f, 1.5f, 0.6f, FLinearColor::White, &Mr);
				Forming->AddParamPulse(Mb, TEXT("Opacity"), 1.f, Bt - 0.05f, 0.08f, 0.55f, 0.8f);
				Forming->AddParamPulse(Mu, TEXT("Opacity"), 0.95f, Bt, 0.05f, 0.18f, 0.5f);
				Forming->AddParamPulse(Mr, TEXT("Opacity"), 1.f, Bt + 0.1f, 0.15f, 0.25f, 0.5f);
				if (Bq) Forming->AddComponent(Bq, EZoogiFormStyle::Grow, FVector::ZeroVector, Bt, 0.4f);
			}
		}
	}

	// ---- 4 ice obelisks (destructible) ----
	for (int32 k = 0; k < 4; ++k)
	{
		const float Yaw = ObeliskYaw0 + 90.f * k;
		AZoogiArenaPiece* P = SpawnPiece(Pol(HubR, Yaw), Yaw + 45.f, TEXT("obelisk"), TEXT("Ice"), 100.f);
		if (!P) continue;
		P->AddBox(FVector(0, 0, 10.f), FVector(84.f, 84.f, 20.f), FRotator::ZeroRotator, StoneDark);
		FZoogiMeshBuilder Body;
		Body.Prism(FVector2D::ZeroVector, 4, ObeliskR + 6.f, ObeliskR - 8.f, 20.f, ObeliskH - 30.f, 45.f, 30.f, false);
		P->AddProc(Body, Ice);
		for (int32 f = 0; f < 4; ++f)
		{
			const FRotator Fr(0.f, f * 90.f, 0.f);
			UStaticMeshComponent* Slit = P->AddBox(Fr.RotateVector(FVector(ObeliskR - 2.f, 0.f, 0.f)) + FVector(0, 0, 80.f), FVector(2.f, 6.f, 90.f), Fr, RuneSoft, false);
			P->MarkGlow(Slit);
		}
		P->AddColliderCyl(FVector2D::ZeroVector, ObeliskR, 0.f, ObeliskH, TEXT("ZObelisk"), StonePM);
		P->SetPathBlock(FVector2D(P->GetActorLocation()), ObeliskR + 6.f);
		P->RubbleKeepZ = 25.f;
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(ObeliskH + 60.f)), 0.65f + 0.1f * k, 0.9f, 6.f);
	}

	// ---- curved ice ricochet walls (segments are separate destructible pieces) ----
	auto BuildWall = [&](float A0, float A1, bool bCombo)
	{
		const int32 N = FMath::Max(3, FMath::RoundToInt((A1 - A0) / 6.5f));
		for (int32 i = 0; i < N; ++i)
		{
			const float Sa = FMath::Lerp(A0, A1, (float)i / N), Sb = FMath::Lerp(A0, A1, (float)(i + 1) / N), Sm = (Sa + Sb) * 0.5f;
			AZoogiArenaPiece* P = SpawnPiece(FVector::ZeroVector, 0.f, TEXT("icewall"), TEXT("Ice"), 120.f);
			if (!P) continue;
			FZoogiMeshBuilder Pane, Cap, Base;
			Pane.ArcSlab(WallR, WallR + WallThick - 6.f, Sa + 0.15f, Sb - 0.15f, -6.f, WallH + 30.f + 14.f * FMath::Sin(Sm * 0.21f), 3, 0.f);
			Cap.ArcSlab(WallR - 3.f, WallR + WallThick - 3.f, Sa + 0.1f, Sb - 0.1f, WallH + 30.f + 14.f * FMath::Sin(Sm * 0.21f), WallH + 36.f + 14.f * FMath::Sin(Sm * 0.21f), 3, 10.f);
			Base.ArcSlab(WallR - 14.f, WallR + WallThick + 18.f, Sa, Sb, -6.f, 14.f, 3, 8.f);
			P->AddProc(Pane, IcePane);
			P->AddProc(Cap, Snow);
			P->AddProc(Base, SnowShade);
			const float Rad = FMath::DegreesToRadians(Sm);
			const float SegLen = (WallR + WallThick * 0.5f) * FMath::DegreesToRadians(Sb - Sa) * 1.04f;
			const FVector C(FMath::Cos(Rad) * (WallR + WallThick * 0.5f), FMath::Sin(Rad) * (WallR + WallThick * 0.5f), WallH * 0.5f - 10.f);
			P->AddColliderBox(C, FVector(WallThick, SegLen, WallH + 20.f), FRotator(0.f, Sm, 0.f), TEXT("ZWall"), WallPM);
			P->SetPathBlock(FVector2D(C), 40.f);
			P->RubbleKeepZ = 20.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Grow, FVector::ZeroVector, 0.55f + 0.07f * i + (bCombo ? 0.f : 0.2f), 0.7f);
		}
	};
	BuildWall(WallNA0, WallNA1, true);
	BuildWall(WallWA0, WallWA1, false);
	BuildWall(WallEA0, WallEA1, false);

	// ---- rim arches (SE = Recovery Gate with a portal sheet) ----
	for (int32 k = 0; k < 4; ++k)
	{
		const float Yaw = ArchYaw[k];
		AZoogiArenaPiece* P = SpawnPiece(Pol(ArchR, Yaw), Yaw, k == 0 ? TEXT("gate_arch") : TEXT("arch"), TEXT("Stone"), 160.f);
		if (!P) continue;
		// Local: X = outward, Y = along the rim.
		for (int32 s = -1; s <= 1; s += 2)
		{
			P->AddBox(FVector(0, s * ArchHalfW, 16.f), FVector(64.f, 64.f, 32.f), FRotator::ZeroRotator, StoneDark);
			P->AddBox(FVector(0, s * ArchHalfW, 32.f + (ArchH - 120.f) * 0.5f), FVector(46.f, 46.f, ArchH - 120.f), FRotator::ZeroRotator, Stone);
			P->AddBox(FVector(0, s * ArchHalfW, ArchH - 116.f), FVector(54.f, 54.f, 10.f), FRotator::ZeroRotator, Snow, false);
		}
		// Semicircular top from 7 voussoirs.
		const float Rr = ArchHalfW, Zs = ArchH - 120.f;
		for (int32 v = 0; v < 7; ++v)
		{
			const float T0 = PI * v / 7.f, T1 = PI * (v + 1) / 7.f, Tm = (T0 + T1) * 0.5f;
			const FVector Cv(0.f, FMath::Cos(Tm) * Rr, Zs + FMath::Sin(Tm) * Rr);
			const float Len = Rr * (T1 - T0) * 1.06f;
			P->AddBox(Cv, FVector(48.f, Len, 40.f), FRotator(0.f, 0.f, -FMath::RadiansToDegrees(Tm) + 90.f), v == 3 ? StoneDark : Stone);
			if (v >= 1 && v <= 5) P->AddBox(Cv + FVector(0, 0, 24.f), FVector(52.f, Len * 0.9f, 9.f), FRotator(0.f, 0.f, -FMath::RadiansToDegrees(Tm) + 90.f), Snow, false);
		}
		UStaticMeshComponent* Key = P->AddBox(FVector(-25.f, 0.f, Zs + Rr - 6.f), FVector(4.f, 22.f, 26.f), FRotator::ZeroRotator, Rune, false);
		P->MarkGlow(Key);
		if (k == 0)
		{
			UStaticMeshComponent* Portal = P->AddBox(FVector(0.f, 0.f, (Zs + Rr) * 0.5f - 10.f), FVector(3.f, ArchHalfW * 2.f - 40.f, Zs + Rr * 0.7f), FRotator::ZeroRotator, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 0.6f), false);
			P->MarkGlow(Portal);
		}
		for (int32 s = -1; s <= 1; s += 2)
			P->AddColliderCyl(FVector2D(0.f, s * ArchHalfW), ArchPostR, 0.f, ArchH - 110.f, TEXT("ZArch"), StonePM);
		P->RubbleKeepZ = 40.f;
		P->FinishBuild();
		if (Forming) Forming->AddActor(P, EZoogiFormStyle::Drop, FVector(0, 0, 900.f), 0.3f + 0.2f * k, 0.95f);
	}

	// ---- snow chute launch ramp (smooth: top face starts flush with the court) ----
	{
		AZoogiArenaPiece* P = SpawnPiece(FVector(RampCX, RampCY, 0.f), RampYaw, TEXT("ramp"), TEXT("Snow"), 140.f);
		if (P)
		{
			const float X0 = -RampLen * 0.5f, X1 = RampLen * 0.5f, Hw = RampWid * 0.5f;
			FZoogiMeshBuilder W, Rails;
			const FVector A(X0, -Hw, 0.f), B(X1, -Hw, RampH), Cc(X1, Hw, RampH), D(X0, Hw, 0.f);
			W.Quad(A, B, Cc, D, FVector(-0.15f, 0.f, 1.f));                                  // top (snow)
			W.Quad(FVector(X1, -Hw, -2.f), FVector(X1, Hw, -2.f), Cc, B, FVector::ForwardVector); // lip face
			W.FlatTri(FVector(X0, -Hw, -2.f), FVector(X1, -Hw, -2.f), B, -FVector::RightVector);
			W.FlatTri(FVector(X0, Hw, -2.f), FVector(X1, Hw, -2.f), Cc, FVector::RightVector);
			P->AddProc(W, Snow);
			for (int32 s = -1; s <= 1; s += 2)
				Rails.Box(FVector(0.f, s * (Hw + 6.f), RampH * 0.5f + 3.f), FVector(RampLen * 0.5f, 7.f, 6.f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(RampH, RampLen)), 0.f, 0.f));
			P->AddProc(Rails, Ice);
			for (int32 k = 0; k < 3; ++k)
			{
				const float X = FMath::Lerp(X0, X1, 0.25f + 0.25f * k);
				const float Z = RampH * (X - X0) / RampLen + 1.2f;
				UStaticMeshComponent* Chev = P->AddBox(FVector(X, 0.f, Z), FVector(10.f, RampWid * 0.7f, 1.5f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(RampH, RampLen)), 0.f, 0.f), RuneSoft, false);
				P->MarkGlow(Chev);
			}
			// Collider: slab whose top runs (X0, 0) -> (X1, RampH), 60 thick.
			const float L = FMath::Sqrt(RampLen * RampLen + RampH * RampH);
			const float Th = FMath::Atan2(RampH, RampLen);
			const FVector Nn(-FMath::Sin(Th), 0.f, FMath::Cos(Th));
			P->AddColliderBox(FVector(0.f, 0.f, RampH * 0.5f) - Nn * 30.f, FVector(L, RampWid, 60.f), FRotator(FMath::RadiansToDegrees(Th), 0.f, 0.f), TEXT("ZRamp"), StonePM);
			P->RubbleKeepZ = 60.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(RampH + 70.f)), 1.0f, 0.8f);
		}
	}

	// ---- NE raised hex-ice platform ----
	{
		AZoogiArenaPiece* P = SpawnPiece(FVector(PlatX, PlatY, 0.f), 0.f, TEXT("platform"), TEXT("Ice"), 180.f);
		if (P)
		{
			const float Rc = PlatR / FMath::Cos(PI / 6.f); // hexagon circumradius whose inradius = PlatR
			FZoogiMeshBuilder Body, Plates;
			Body.Prism(FVector2D::ZeroVector, 6, Rc, Rc - 4.f, -8.f, PlatH - 3.f, 30.f, 0.f, false);
			P->AddProc(Body, IceDeep);
			// Hex plates on top (slightly different heights read as the art's cracked ice sheet).
			const FVector2D Cells[7] = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0.5f, 0.866f), FVector2D(-0.5f, 0.866f), FVector2D(0.5f, -0.866f), FVector2D(-0.5f, -0.866f) };
			for (const FVector2D& Cl : Cells)
				Plates.Prism(Cl * Rc * 0.56f, 6, Rc * 0.31f, Rc * 0.29f, PlatH - 4.f, PlatH + Rng.FRandRange(-0.5f, 1.2f), 30.f, 0.f, false);
			P->AddProc(Plates, Ice);
			for (int32 k = 0; k < 3; ++k)
				P->AddColliderBox(FVector(0, 0, (PlatH - 8.f) * 0.5f - 4.f), FVector(PlatR * 2.f, Rc, PlatH + 8.f), FRotator(0.f, 60.f * k, 0.f), TEXT("ZPlatform"), StonePM);
			P->SetPathBlock(FVector2D(PlatX, PlatY), PlatR + 10.f);
			P->RubbleKeepZ = 60.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -(PlatH + 70.f)), 1.1f, 0.8f, 4.f);
		}
	}

	// ---- rim ice-block clusters + snow drifts (no collision, destructible by ring-outs / powers) ----
	{
		int32 n = 0;
		for (float A = 8.f; A < 360.f; A += 21.f)
		{
			bool bSkip = false;
			for (float Ay : ArchYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(A, Ay)) < 12.f) bSkip = true;
			if (NearIsletYaw(A, 8.f)) bSkip = true; // bridge corridors
			if (bSkip) continue;
			const float R = Rng.FRandRange(1650.f, 1720.f);
			AZoogiArenaPiece* P = SpawnPiece(Pol(R, A), A, TEXT("rim_ice"), TEXT("Ice"), 80.f);
			if (!P) continue;
			FZoogiMeshBuilder Blocks, Drift;
			const int32 Nb = Rng.RandRange(2, 4);
			for (int32 b = 0; b < Nb; ++b)
				Blocks.Chunk(FVector(Rng.FRandRange(-30.f, 40.f), Rng.FRandRange(-90.f, 90.f), Rng.FRandRange(20.f, 45.f)), FVector(Rng.FRandRange(30.f, 55.f), Rng.FRandRange(35.f, 60.f), Rng.FRandRange(28.f, 55.f)), Rng, 0.25f, Rng.FRandRange(0.f, 90.f));
			Drift.Chunk(FVector(10.f, 0.f, 4.f), FVector(70.f, 120.f, 22.f), Rng, 0.2f, 0.f);
			P->AddProc(Blocks, (n % 3 == 0) ? IceDeep : Ice);
			P->AddProc(Drift, Snow);
			P->RubbleKeepZ = 30.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -180.f), 0.05f + A / 360.f * 1.3f, 0.7f);
			++n;
		}
	}

	// ---- ice boulders (reuse AZoogiRock: crack / shatter, 4-stage, cyan rune carvings) ----
	{
		TArray<FVector4> Rim;
		for (float A : RockYaw) Rim.Add(FVector4(A, RockDist, RockRad, RockH));
		BuildRim(Rim, ZoogiUtil::Hex(TEXT("#BFE6F7")), ZoogiUtil::Hex(TEXT("#7FC4EA")), 0.55f, true, 0.55f, Cyan, 2.2f);
		for (AZoogiRock* R : Rocks)
		{
			if (!R) continue;
			R->bFourStage = true;
			if (Forming) Forming->AddActor(R, EZoogiFormStyle::Rise, FVector(0, 0, -200.f), 1.4f, 0.7f);
		}
	}

	// ---- hub crystal (floating, no collider; stays above the 3 centre orbs) ----
	{
		USceneComponent* Pv = NewPivot(FVector(0, 0, 0));
		USceneComponent* Spin = NewObject<USceneComponent>(this);
		Spin->SetMobility(EComponentMobility::Movable);
		Spin->SetupAttachment(Pv);
		Spin->RegisterComponent();
		Built.Add(Spin);
		FZoogiMeshBuilder Cr;
		const float Hs[5] = { 95.f, 60.f, 55.f, 50.f, 42.f };
		for (int32 c = 0; c < 5; ++c)
		{
			const FVector2D O = c == 0 ? FVector2D::ZeroVector : FVector2D(Pol(22.f, c * 90.f + 20.f));
			const float Tilt = c == 0 ? 0.f : 1.f;
			Cr.Prism(O, 6, 13.f - c * 0.8f, 9.f, 0.f, Hs[c] * 0.6f, c * 17.f, Hs[c] * 0.4f, true);
			(void)Tilt;
		}
		UProceduralMeshComponent* CrM = AddFrozenProc(Cr, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 2.2f), Spin, false);
		CrM->SetRelativeLocation(FVector(0, 0, 225.f));
		CrM->ComponentTags.Add(TEXT("NoCamBlock"));
		URotatingMovementComponent* Rot = NewObject<URotatingMovementComponent>(this);
		Rot->bAutoRegisterUpdatedComponent = false;
		Rot->RotationRate = FRotator(0.f, 14.f, 0.f);
		Rot->RegisterComponent();
		Rot->SetUpdatedComponent(Spin);
		Built.Add(Rot);
		AddFillLight(FVector(0, 0, 260.f), ZoogiUtil::Hex(TEXT("#69E1F9")), 80.f, 700.f);
		if (Forming) Forming->AddComponent(Pv, EZoogiFormStyle::Rise, FVector(0, 0, -320.f), 1.7f, 0.9f, 8.f);
	}

	// ---- stand islets (round 3c, Evan 10:54 PM: the Buzzy hero / 3/4 bleachers): each islet is a snowy floating
	// ellipse almost filled by OPEN curved wooden bleachers (6 stepped rows, no roof, snow on the walls / tops, low side
	// walls) facing the ring, split into 3 breakable sections with real stepped colliders. A rollable bridge (deck +
	// low rails, collision) links the court edge to the islet's front apron, which holds an Arena Orb spot; a low snow lip
	// rail keeps normal rolls on the islet (hard hits still ring out over it). One small pine behind each stand.
	UStaticMesh* SnowPine = ZoogiUtil::LoadMesh(TEXT("/Game/Env/SnowPine/SM_SnowPine.SM_SnowPine"));
	UMaterialInterface* StandWood = MakeFrozenMat(TEXT("StandWood"), ZoogiUtil::Hex(TEXT("#E3E6EE")), TEXT("T_FrozenStoneTile"), 50.f, 0.6f, 0.75f, 0.3f, FLinearColor::Black, 0.f);
	UMaterialInterface* StandRiser = MakeFrozenMat(TEXT("StandRiser"), ZoogiUtil::Hex(TEXT("#A9B3C6")), TEXT("T_FrozenStoneTile"), 50.f, 0.6f, 0.8f, 0.3f, FLinearColor::Black, 0.f);
	UMaterialInterface* StandWall = MakeFrozenMat(TEXT("StandWall"), ZoogiUtil::Hex(TEXT("#C3CCDA")), TEXT("T_FrozenStoneTile"), 110.f, 0.55f, 0.65f, 0.4f, FLinearColor::Black, 0.f);
	UMaterialInterface* Window = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#5D6F8C")), 0.3f, FLinearColor::Black, 0.f, 0.6f);
	const float Rb = IsletR * IsletStretch; // tangential semi-axis
	for (int32 k = 0; k < 4; ++k)
	{
		const float Yaw = IsletYaw[k];
		const FVector C = Pol(IsletDist[k], Yaw, IsletTopZ);
		const FVector2D U(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw))), Vt(-U.Y, U.X);
		// Islet local (piece frame, yaw Yaw + 180): +X toward the court.
		const FRotator Face(0.f, Yaw + 180.f, 0.f);
		auto ToWorld = [&](float Lx, float Ly, float Lz) { return C + Face.RotateVector(FVector(Lx, Ly, Lz)); };
		auto Stretch = [&](FZoogiMeshBuilder& M)
		{
			for (FVector& P : M.V) { const FVector2D Q(P.X, P.Y); const float Pr = FVector2D::DotProduct(Q, U), Pt = FVector2D::DotProduct(Q, Vt); const FVector2D Q2 = U * Pr + Vt * Pt * IsletStretch; P.X = Q2.X; P.Y = Q2.Y; }
			for (FVector& Nn : M.N) { const FVector2D Q(Nn.X, Nn.Y); const FVector2D Q2 = U * FVector2D::DotProduct(Q, U) + Vt * (FVector2D::DotProduct(Q, Vt) / IsletStretch); Nn = FVector(Q2.X, Q2.Y, Nn.Z).GetSafeNormal(); }
		};
		USceneComponent* Pv = NewPivot(C);
		{
			FZoogiMeshBuilder Top, Under, Ic;
			Top.Prism(FVector2D::ZeroVector, 18, IsletR, IsletR - 12.f, -80.f, 0.f, Yaw, 10.f, false);
			Under.Prism(FVector2D::ZeroVector, 11, IsletR - 30.f, 0.f, -80.f, -80.f - IsletR * 1.35f, Yaw + 7.f, 0.f, false);
			for (int32 i = 0; i < 22; ++i)
			{
				const float A = i * (360.f / 22) + Rng.FRandRange(-5.f, 5.f);
				const float L = Rng.FRandRange(70.f, 260.f);
				Ic.Prism(FVector2D(Pol(IsletR - 30.f, A)), 5, Rng.FRandRange(16.f, 30.f), 0.f, -82.f, -82.f - L, A, 0.f, false);
			}
			Stretch(Top); Stretch(Under); Stretch(Ic);
			AddFrozenProc(Top, Snow, Pv, true);
			AddFrozenProc(Under, Cliff, Pv, true);
			AddFrozenProc(Ic, IcePane, Pv, false);
			// One small snow pine beside the bleachers (outside the side wall, never in front of the seats).
			if (UStaticMeshComponent* Tr = AddTree(SnowPine, ToWorld(60.f, (k % 2 ? 1.f : -1.f) * (StandHalfW + 75.f), 0.f), Rng.FRandRange(300.f, 380.f), Rng.FRandRange(0.f, 360.f)))
				Tr->AttachToComponent(Pv, FAttachmentTransformRules::KeepWorldTransform);
		}
		if (Forming) Forming->AddComponent(Pv, EZoogiFormStyle::Rise, FVector(0, 0, -700.f), 0.1f + 0.12f * k, 1.3f);
		// Walkable islet floor (invisible elliptic collider, top flush with the islet snow).
		if (UStaticMeshComponent* Fl = AddCylinder(FVector2D(C.X, C.Y), IsletR - 8.f, 80.f, C.Z, nullptr, true))
		{
			Fl->SetRelativeScale3D(Fl->GetRelativeScale3D() * FVector(1.f, IsletStretch, 1.f));
			Fl->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
			Fl->SetVisibility(false);
			Fl->ComponentTags.Add(TEXT("NoCamBlock"));
		}
		// Arena Orb spot on the front apron, in front of the bleachers.
		ExtraOrbSpots.Add(ToWorld(StandX0 + (IsletR - StandX0) * 0.5f, 0.f, ZoogiConst::OrbRadius + 1.f));

		// Bleachers: rows are arcs around a focus point out toward the court (gently curved like the art).
		const float Fx = StandX0 + StandFocus;
		auto Arc = [&](float R, float A) { return FVector(Fx - R * FMath::Cos(A), R * FMath::Sin(A), 0.f); };
		auto HalfAng = [&](int32 t, float R) { return FMath::Asin(FMath::Min(0.95f, (StandHalfW - 14.f * t) / R)); };
		for (int32 sec = 0; sec < 3; ++sec)
		{
			AZoogiArenaPiece* P = SpawnPiece(C, Yaw + 180.f, TEXT("stand"), TEXT("Wood"), 160.f);
			if (!P) continue;
			FZoogiMeshBuilder Tops, Risers, Walls, SnowB, Win, Lamp;
			const float S0 = -1.f + sec * (2.f / 3.f), S1 = S0 + 2.f / 3.f; // fraction of each row's half angle
			const int32 Segs = 4;
			// Curved block between radii R0..R1, angle fractions F0..F1 of Phi, Z0..Z1; faces into the given builders.
			auto Block = [&](FZoogiMeshBuilder* TopB, FZoogiMeshBuilder* FrontB, FZoogiMeshBuilder* BackB, FZoogiMeshBuilder* EndB, float R0, float R1, float A0, float A1, float Z0, float Z1)
			{
				for (int32 s = 0; s < Segs; ++s)
				{
					const float Aa = FMath::Lerp(A0, A1, (float)s / Segs), Ab = FMath::Lerp(A0, A1, (float)(s + 1) / Segs);
					const FVector I0 = Arc(R0, Aa), I1 = Arc(R0, Ab), O0 = Arc(R1, Aa), O1 = Arc(R1, Ab);
					const FVector Z0v(0, 0, Z0), Z1v(0, 0, Z1);
					if (TopB) TopB->Quad(I0 + Z1v, I1 + Z1v, O1 + Z1v, O0 + Z1v, FVector::UpVector);
					const FVector InN = (Arc(R0 - 10.f, (Aa + Ab) * 0.5f) - Arc(R0, (Aa + Ab) * 0.5f)).GetSafeNormal();
					if (FrontB) FrontB->Quad(I0 + Z0v, I1 + Z0v, I1 + Z1v, I0 + Z1v, InN);
					if (BackB) BackB->Quad(O0 + Z0v, O1 + Z0v, O1 + Z1v, O0 + Z1v, -InN);
				}
				if (EndB)
					for (float A : { A0, A1 })
					{
						const FVector I = Arc(R0, A), O = Arc(R1, A);
						const FVector Sn = (A == A0 ? -1.f : 1.f) * FVector(FMath::Sin(A), FMath::Cos(A), 0.f);
						EndB->Quad(I + FVector(0, 0, Z0), O + FVector(0, 0, Z0), O + FVector(0, 0, Z1), I + FVector(0, 0, Z1), Sn);
					}
			};
			auto Collider = [&](float R0, float R1, float A0, float A1, float Z1, FName Tag)
			{
				const float Am = (A0 + A1) * 0.5f, Rm = (R0 + R1) * 0.5f;
				const FVector Cm = Arc(Rm, Am);
				const float Chord = 2.f * R1 * FMath::Sin(FMath::Abs(A1 - A0) * 0.5f) * 1.02f;
				const float YawL = FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(Am), -FMath::Cos(Am)));
				P->AddColliderBox(FVector(Cm.X, Cm.Y, Z1 * 0.5f), FVector(R1 - R0, Chord, Z1), FRotator(0.f, YawL, 0.f), Tag, StonePM);
			};
			for (int32 t = 0; t < StandRows; ++t)
			{
				const float R0 = StandFocus + t * StandDepth, R1 = R0 + StandDepth, Zt = (t + 1) * StandRise;
				const float Ph = HalfAng(t, R1);
				const float A0 = S0 * Ph, A1 = S1 * Ph;
				Block(&Tops, &Risers, nullptr, nullptr, R0, R1, A0, A1, 0.f, Zt);
				// Plank seams: a thin darker board line at the back of each seat.
				Block(&Risers, nullptr, nullptr, nullptr, R1 - 10.f, R1, A0, A1, Zt, Zt + 1.5f);
				Collider(R0, R1, A0, A1, Zt, TEXT("ZStand"));
				// Low side walls on the outer ends (stepped with the rows) + snow caps and row-end drifts.
				for (int32 e = 0; e < 2; ++e)
				{
					if ((e == 0 && sec != 0) || (e == 1 && sec != 2)) continue;
					const float Ae = e == 0 ? A0 : A1, Dw = (e == 0 ? -1.f : 1.f) * 32.f / R1;
					const float Wa0 = FMath::Min(Ae, Ae + Dw), Wa1 = FMath::Max(Ae, Ae + Dw);
					Block(nullptr, &Walls, &Walls, &Walls, R0, R1, Wa0, Wa1, 0.f, Zt + 34.f);
					Block(&SnowB, &SnowB, &SnowB, &SnowB, R0 - 3.f, R1 + 3.f, Wa0 - 0.004f, Wa1 + 0.004f, Zt + 34.f, Zt + 44.f);
					Collider(R0, R1, Wa0, Wa1, Zt + 34.f, TEXT("ZStand"));
					const FVector Dp = Arc(R0 + StandDepth * 0.5f, Ae - Dw * 0.8f);
					SnowB.Chunk(Dp + FVector(0, 0, Zt + 6.f), FVector(28.f, 34.f, 10.f), Rng, 0.2f, 0.f);
				}
			}
			// Back wall with snow cap, railing posts, and the art's facade (windows / door) on its outer face.
			{
				const float Rw0 = StandFocus + StandRows * StandDepth, Rw1 = Rw0 + 34.f, Zw = StandRows * StandRise + 72.f;
				const float Ph = HalfAng(StandRows - 1, Rw1) + 32.f / Rw1;
				const float A0 = S0 * Ph, A1 = S1 * Ph;
				Block(&Walls, &Walls, &Walls, (sec != 1) ? &Walls : nullptr, Rw0, Rw1, A0, A1, 0.f, Zw);
				Block(&SnowB, &SnowB, &SnowB, &SnowB, Rw0 - 4.f, Rw1 + 4.f, A0, A1, Zw, Zw + 12.f);
				Collider(Rw0, Rw1, A0, A1, Zw, TEXT("ZStand"));
				const int32 Nw = 4;
				for (int32 w = 0; w < Nw; ++w)
				{
					const float Aw = FMath::Lerp(A0, A1, (w + 0.5f) / Nw);
					const FVector Wp = Arc(Rw1 + 2.f, Aw);
					const float YawL = FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(Aw), -FMath::Cos(Aw)));
					const bool bDoor = sec == 1 && (w == 1 || w == 2) && false;
					Win.Box(Wp + FVector(0, 0, bDoor ? 70.f : 210.f), FVector(3.f, 26.f, bDoor ? 70.f : 30.f), FRotator(0.f, YawL, 0.f));
					Win.Box(Wp + FVector(0, 0, 110.f), FVector(3.f, 26.f, 26.f), FRotator(0.f, YawL, 0.f));
				}
			}
			// Lamp post at the front corner of the outer sections (the art's street lamp).
			if (sec != 1 && ((k + sec) % 2 == 0))
			{
				const float Ph = HalfAng(0, StandFocus + StandDepth);
				const FVector Lp = Arc(StandFocus - 30.f, (sec == 0 ? -1.f : 1.f) * Ph * 1.02f);
				Walls.Box(Lp + FVector(0, 0, 160.f), FVector(5.f, 5.f, 160.f));
				Walls.Box(Lp + FVector(0, 22.f, 318.f), FVector(4.f, 24.f, 3.f));
				Lamp.Box(Lp + FVector(0, 40.f, 300.f), FVector(10.f, 10.f, 14.f));
			}
			P->AddProc(Tops, StandWood);
			P->AddProc(Risers, StandRiser);
			P->AddProc(Walls, StandWall);
			P->AddProc(SnowB, Snow);
			if (!Win.IsEmpty()) P->AddProc(Win, Window, false);
			if (!Lamp.IsEmpty()) { UProceduralMeshComponent* Lm = P->AddProc(Lamp, RuneSoft, false); P->MarkGlow(Lm); }
			P->RubbleKeepZ = 40.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -700.f), 0.1f + 0.12f * k + 0.08f * sec, 1.3f);
		}
		// Low snow lip rail around the islet edge (breakable), open at the bridge landing.
		{
			AZoogiArenaPiece* P = SpawnPiece(C, Yaw + 180.f, TEXT("islet_rail"), TEXT("Snow"), 90.f);
			if (P)
			{
				FZoogiMeshBuilder Lip;
				const int32 Nl = 28;
				for (int32 i = 0; i < Nl; ++i)
				{
					const float A0 = 2.f * PI * i / Nl, A1 = 2.f * PI * (i + 1) / Nl, Am = (A0 + A1) * 0.5f;
					if (FMath::Abs(FMath::UnwindRadians(Am)) < FMath::DegreesToRadians(BridgeGapDeg)) continue; // bridge landing (local +X)
					const FVector Pa(FMath::Cos(A0) * (IsletR - 22.f), FMath::Sin(A0) * (Rb - 22.f), 0.f), Pb(FMath::Cos(A1) * (IsletR - 22.f), FMath::Sin(A1) * (Rb - 22.f), 0.f);
					const FVector Mid = (Pa + Pb) * 0.5f;
					const float Len = FVector::Dist(Pa, Pb) + 6.f, YawL = FMath::RadiansToDegrees(FMath::Atan2(Pb.Y - Pa.Y, Pb.X - Pa.X));
					Lip.Chunk(Mid + FVector(0, 0, IsletLipH * 0.5f), FVector(Len * 0.55f, 20.f, IsletLipH * 0.6f), Rng, 0.15f, YawL);
					P->AddColliderBox(Mid + FVector(0, 0, IsletLipH * 0.5f), FVector(Len, 26.f, IsletLipH), FRotator(0.f, YawL, 0.f), TEXT("ZRail"), WallPM);
				}
				P->AddProc(Lip, Snow);
				P->RubbleKeepZ = 10.f;
				P->FinishBuild();
				if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -700.f), 0.1f + 0.12f * k, 1.3f);
			}
		}
		// Rollable bridge from the court edge to the islet apron: deck + low rails with collision (breakable).
		{
			const float RA = ZoogiConst::KnockoffRadius - 15.f, RB = IsletDist[k] - IsletR + 25.f;
			const FVector A = Pol(RA, Yaw, 0.f), B = Pol(RB, Yaw, IsletTopZ);
			AZoogiArenaPiece* P = SpawnPiece((A + B) * 0.5f, Yaw, TEXT("bridge"), TEXT("Wood"), 110.f);
			if (P)
			{
				const float L = RB - RA, Dz = B.Z - A.Z;
				const float Th = FMath::Atan2(Dz, L), Pitch = FMath::RadiansToDegrees(Th);
				const FRotator Rot(Pitch, 0.f, 0.f);
				const FVector Nn(-FMath::Sin(Th), 0.f, FMath::Cos(Th));
				FZoogiMeshBuilder Planks, Rails, Under;
				const int32 Np = FMath::Max(6, FMath::RoundToInt(L / 34.f));
				for (int32 i = 0; i < Np; ++i)
				{
					const float X = -L * 0.5f + L * (i + 0.5f) / Np;
					const FVector Pt = FVector(X, 0.f, Dz * (X / L)) - Nn * 3.f;
					Planks.Box(Pt, FVector(15.f, BridgeHalfW, 3.f), Rot);
				}
				Under.Box(-Nn * 14.f, FVector(L * 0.5f, BridgeHalfW * 0.7f, 8.f), Rot);
				for (int32 s = -1; s <= 1; s += 2)
				{
					Rails.Box(FVector(0.f, s * (BridgeHalfW + 6.f), BridgeRailH * 0.5f) + FVector(0, 0, 0), FVector(L * 0.5f, 6.f, BridgeRailH * 0.5f), Rot);
					Rails.Box(FVector(0.f, s * (BridgeHalfW + 6.f), BridgeRailH + 26.f), FVector(L * 0.5f, 3.f, 3.f), Rot);
					const int32 Npost = FMath::Max(2, FMath::RoundToInt(L / 160.f));
					for (int32 j = 0; j <= Npost; ++j)
					{
						const float X = -L * 0.5f + L * j / Npost;
						Rails.Box(FVector(X, s * (BridgeHalfW + 6.f), Dz * (X / L) + 28.f), FVector(6.f, 6.f, 28.f));
					}
				}
				P->AddProc(Planks, StandWood);
				P->AddProc(Rails, StandRiser);
				P->AddProc(Under, StandRiser);
				// Deck collider (top runs A -> B, 24 thick) and the two low rail colliders.
				P->AddColliderBox(-Nn * 12.f, FVector(L + 30.f, BridgeHalfW * 2.f, 24.f), Rot, TEXT("ZBridge"), StonePM);
				for (int32 s = -1; s <= 1; s += 2)
					P->AddColliderBox(FVector(0.f, s * (BridgeHalfW + 6.f), BridgeRailH * 0.5f), FVector(L + 30.f, 12.f, BridgeRailH), Rot, TEXT("ZRail"), WallPM);
				P->RubbleKeepZ = 200.f;
				P->FinishBuild();
				if (Forming) Forming->AddActor(P, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.3f + 0.1f * k, 0.8f);
			}
		}
		{
			const int32 Before = Built.Num();
			AddWaterfall(Pol(IsletDist[k] - 60.f, Yaw + 28.f, IsletTopZ - 10.f), Yaw + 28.f, 170.f, 2600.f, 50.f);
			for (int32 b = Before; b < Built.Num(); ++b) if (USceneComponent* Wc = Cast<USceneComponent>(Built[b])) Wc->AttachToComponent(Pv, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// ---- NW snowy outcrop (terraced) with frosted trees ----
	{
		AZoogiArenaPiece* P = SpawnPiece(Pol(OutcropDist, OutcropYaw, -20.f), OutcropYaw, TEXT("outcrop"), TEXT("Stone"), 400.f);
		if (P)
		{
			FZoogiMeshBuilder Rock, SnowT;
			const float Rs[3] = { 300.f, 230.f, 150.f };
			float Z = -260.f;
			for (int32 t = 0; t < 3; ++t)
			{
				const float H = t == 0 ? 330.f : 110.f;
				Rock.Prism(FVector2D(Rng.FRandRange(-25.f, 25.f), Rng.FRandRange(-25.f, 25.f)), 7, Rs[t], Rs[t] * 0.94f, Z, Z + H, t * 23.f, 0.f, false);
				SnowT.Prism(FVector2D::ZeroVector, 7, Rs[t] * 0.97f, Rs[t] * 0.9f, Z + H, Z + H + 12.f, t * 23.f, 6.f, false);
				Z += H;
			}
			P->AddProc(Rock, Cliff);
			P->AddProc(SnowT, Snow);
			P->RubbleKeepZ = -100.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -800.f), 0.0f, 1.4f);
			for (int32 t = 0; t < 3; ++t)
				if (UStaticMeshComponent* Tr = AddTree(SnowPine, Pol(OutcropDist, OutcropYaw, 0.f) + FVector(Rng.FRandRange(-90.f, 90.f), Rng.FRandRange(-90.f, 90.f), 190.f), Rng.FRandRange(380.f, 520.f), Rng.FRandRange(0.f, 360.f)))
					if (Forming) Forming->AddComponent(Tr, EZoogiFormStyle::Rise, FVector(0, 0, -800.f), 0.0f, 1.4f);
		}
	}
	// Round 3: stronger falls (wider, much longer, two more) with mist clouds where they drop into the cloud sea.
	{
		const float Falls[4] = { FallYaw[0], FallYaw[1], 205.f, 292.f };
		const float Wid[4] = { 230.f, 200.f, 160.f, 170.f };
		UTexture* Cloud = FTex(TEXT("T_FrozenCloud"));
		for (int32 f = 0; f < 4; ++f)
		{
			AddWaterfall(Pol(1775.f, Falls[f], -16.f), Falls[f], Wid[f], 3000.f, 55.f);
			for (int32 m = 0; m < 3; ++m)
				FlatQuad(Cloud, Pol(1900.f + m * 120.f, Falls[f] + Rng.FRandRange(-4.f, 4.f), -1700.f - m * 250.f), Rng.FRandRange(0.f, 360.f), 900.f + m * 300.f, 0.9f, 0.25f, 1.1f, FLinearColor::White, nullptr);
		}
	}

	// ---- round 3c gameplay checks (-ZoogiFrozenPlayTest) ----
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrozenPlayTest")))
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AZoogiFrozenPlayTest* PT = GetWorld()->SpawnActor<AZoogiFrozenPlayTest>(AZoogiFrozenPlayTest::StaticClass(), FTransform::Identity, SP))
		{
			PT->Arena = this;
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), PT->ShotDir);
		}
	}

	// ---- reactive floor scars ----
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Scars = GetWorld()->SpawnActor<AZoogiFloorScars>(AZoogiFloorScars::StaticClass(), FTransform::Identity, SP);
		if (Scars) { Scars->Init(ZoogiConst::KnockoffRadius - 60.f); Scars->SlabMat = Ice; }
	}

	// ---- forming: rune floor lights up while the stage assembles ----
	if (Forming && FloorMID) Forming->AddParamRamp(FloorMID, TEXT("GlowStrength"), 0.f, FloorGlowTarget, 0.0f, 2.4f);
	UE_LOG(LogZoogi, Log, TEXT("Frozen Ring built: %d pieces (4 indestructible pillars), %d forming entries (%.2f s), floor %s"), Pieces.Num(), Forming ? Forming->Num() : 0,
		Forming ? Forming->GetLength() : 0.f, FloorMID ? TEXT("art-derived") : TEXT("FALLBACK"));
}
