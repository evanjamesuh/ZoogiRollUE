// Frozen Ring (map id "ice"), rebuilt Oct 6 2026 to match Evan's Buzzy Frozen Ring art.
// Court: the Buzzy top-down rectified onto the floor (T_FrozenFloor / T_FrozenGlow, Scripts/arenas/gen_frozen.py).
// Every stage piece is an AZoogiArenaPiece (destructible, power-reactive) except the 4 Resonance Hub pillars
// (indestructible, they rise from under the floor at match start). UZoogiStageForming assembles the stage (~3 s).
// Layout constants: ZoogiFrozenLayout.h (shared with the 2v2 combo props).
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiFloorScars.h"
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
	AZoogiArenaPiece* P = GetWorld()->SpawnActor<AZoogiArenaPiece>(AZoogiArenaPiece::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Loc), SP);
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
	UMaterialInterface* Cliff = MakeFrozenMat(TEXT("Cliff"), ZoogiUtil::Hex(TEXT("#B7DCF4")), TEXT("T_FrozenIceTile"), 420.f, 0.6f, 0.4f, 0.5f, ZoogiUtil::Hex(TEXT("#5FA8DA")), 0.06f);
	UMaterialInterface* Peak = MakeFrozenMat(TEXT("Peak"), ZoogiUtil::Hex(TEXT("#A7C3E2")), TEXT("T_FrozenSnowTile"), 2400.f, 0.5f, 0.8f, 0.3f, FLinearColor(0.5f, 0.65f, 0.85f), 0.08f);
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
	FloorGlowTarget = 1.6f;
	if (FloorMID)
	{
		FloorMID->SetScalarParameterValue(TEXT("GlowTint"), 0.25f);
		FloorMID->SetScalarParameterValue(TEXT("DetailStrength"), 0.12f);
		FloorMID->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
		FloorMID->SetScalarParameterValue(TEXT("Specular"), 0.6f);
		FloorMID->SetScalarParameterValue(TEXT("Pulse"), 0.12f);
	}
	BuildIsland(ZoogiUtil::Hex(TEXT("#CFEFFF")), ZoogiUtil::Hex(TEXT("#9ED8F5")), ZoogiUtil::Hex(TEXT("#F4FBFF")),
		ZoogiUtil::Hex(TEXT("#7FB3DE")), ZoogiUtil::Hex(TEXT("#A9D4F0")), 0.03f, FloorMat);
	AddPanoramaSky(TEXT("Frozen"), 1.0f, 0.f, FLinearColor::White);

	// Snow lip (outside the knockout line, top below the court at the line so balls roll off as before) + ice cliff face.
	{
		FZoogiMeshBuilder SnowM, CliffM;
		const int32 Seg = 144;
		auto H = [&](float R, float A) -> float
		{
			const float N = FMath::PerlinNoise2D(FVector2D(FMath::Cos(A) * 3.1f, FMath::Sin(A) * 3.1f)) * 0.5f + 0.5f;
			if (R < 1560.f) return -4.f;
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
		for (int32 i = 0; i < 70; ++i)
		{
			const float A = i * 360.f / 70 + Rng.FRandRange(-2.f, 2.f);
			const float R = Rng.FRandRange(1690.f, 1770.f);
			const float L = Rng.FRandRange(90.f, 340.f);
			const FVector2D XY(Pol(R, A));
			Icicles.Prism(XY, 5, Rng.FRandRange(22.f, 42.f), 0.f, -600.f, -600.f - L * 1.4f, A, 0.f, false);
		}
		AddFrozenProc(Under, IceDeep, nullptr, true);
		AddFrozenProc(Icicles, Ice, nullptr, false);
		// Round 2: lumpy snow mounds along the lip (the art's thick snow rim), no collision.
		FZoogiMeshBuilder Mounds;
		for (int32 i = 0; i < 64; ++i)
		{
			const float A = i * 360.f / 64 + Rng.FRandRange(-2.5f, 2.5f);
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
		Ring(Bound, 1526.f, 1544.f, 0.9f, 192);
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
		// Mountains: each a cluster (main peak + two shoulders), mostly snow-covered, far enough to sit on the horizon.
		FZoogiMeshBuilder Rock, Caps;
		for (int32 i = 0; i < 40; ++i)
		{
			const float A = i * 360.f / 40 + Rng.FRandRange(-4.f, 4.f);
			const bool bFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 50.f; // camera side (art bottom): lower
			const float R = Rng.FRandRange(24000.f, 38000.f) * (bFront ? 1.15f : 1.f);
			const float Hh = Rng.FRandRange(3200.f, 6200.f) * (bFront ? 0.5f : 1.f);
			for (int32 c = 0; c < 3; ++c)
			{
				const float Hc = Hh * (c == 0 ? 1.f : Rng.FRandRange(0.5f, 0.75f));
				const float Rb = Hc * Rng.FRandRange(0.45f, 0.62f);
				const FVector2D XY = FVector2D(Pol(R, A)) + (c == 0 ? FVector2D::ZeroVector : FVector2D(Pol(Hh * Rng.FRandRange(0.35f, 0.6f), A + (c == 1 ? 90.f : -90.f) + Rng.FRandRange(-20.f, 20.f))));
				const int32 Sides = Rng.RandRange(5, 8);
				const float Yw = Rng.FRandRange(0.f, 360.f);
				const float Z0 = -5600.f, Z1 = Z0 + Hc * 0.42f;
				Rock.Prism(XY, Sides, Rb, Rb * 0.58f, Z0, Z1, Yw, 0.f, false);
				Caps.Prism(XY, Sides, Rb * 0.60f, 0.f, Z1, Z0 + Hc, Yw, 0.f, false);
			}
		}
		AddFrozenProc(Rock, Peak, nullptr, false);
		AddFrozenProc(Caps, Snow, nullptr, false);
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

	// ---- stand islets: snowy floating islands with bleachers, pavilions, trees and bridges ----
	UStaticMesh* SnowPine = ZoogiUtil::LoadMesh(TEXT("/Game/Env/SnowPine/SM_SnowPine.SM_SnowPine"));
	for (int32 k = 0; k < 4; ++k)
	{
		const float Yaw = IsletYaw[k];
		const FVector C = Pol(IsletDist[k], Yaw, -60.f);
		USceneComponent* Pv = NewPivot(C);
		{
			FZoogiMeshBuilder Top, Under, Ic;
			Top.Prism(FVector2D::ZeroVector, 14, IsletR, IsletR - 10.f, -70.f, 0.f, Yaw, 18.f, false);
			Under.Prism(FVector2D::ZeroVector, 9, IsletR - 20.f, 0.f, -70.f, -70.f - IsletR * 1.5f, Yaw + 7.f, 0.f, false);
			for (int32 i = 0; i < 16; ++i)
			{
				const float A = i * 22.5f + Rng.FRandRange(-6.f, 6.f);
				const float L = Rng.FRandRange(60.f, 220.f);
				Ic.Prism(FVector2D(Pol(IsletR - 25.f, A)), 5, Rng.FRandRange(14.f, 26.f), 0.f, -72.f, -72.f - L, A, 0.f, false);
			}
			UProceduralMeshComponent* T = AddFrozenProc(Top, Snow, Pv, true);
			AddFrozenProc(Under, Cliff, Pv, true);
			AddFrozenProc(Ic, Ice, Pv, false);
			(void)T;
			for (int32 t = 0; t < 2; ++t)
			{
				const FVector TP = Pol(IsletR * 0.62f, Yaw + (t ? 70.f : -70.f), 0.f);
				if (UStaticMeshComponent* Tr = AddTree(SnowPine, C + TP, Rng.FRandRange(420.f, 620.f), Rng.FRandRange(0.f, 360.f))) Tr->AttachToComponent(Pv, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
		if (Forming) Forming->AddComponent(Pv, EZoogiFormStyle::Rise, FVector(0, 0, -700.f), 0.1f + 0.12f * k, 1.3f);
		// Bleachers (destructible piece) facing the court.
		{
			AZoogiArenaPiece* P = SpawnPiece(C, Yaw + 180.f, TEXT("stand"), TEXT("Wood"), 160.f);
			if (P)
			{
				FZoogiMeshBuilder Seats, Risers, Sides;
				const int32 Tiers = 5;
				const float Wd = 560.f, Dp = 58.f, Rise = 36.f, X0 = 200.f;
				for (int32 t = 0; t < Tiers; ++t)
				{
					const float X = X0 - t * Dp, Z = (t + 1) * Rise;
					Seats.Box(FVector(X - Dp * 0.5f, 0.f, Z - 4.f), FVector(Dp * 0.5f, Wd * 0.5f, 4.f));
					Risers.Box(FVector(X - 2.f, 0.f, Z * 0.5f), FVector(3.f, Wd * 0.5f, Z * 0.5f));
				}
				for (int32 s = -1; s <= 1; s += 2)
					Sides.Box(FVector(X0 - Tiers * Dp * 0.5f, s * (Wd * 0.5f + 10.f), Tiers * Rise * 0.5f + 8.f), FVector(Tiers * Dp * 0.5f + 6.f, 10.f, Tiers * Rise * 0.5f + 8.f));
				Sides.Box(FVector(X0 - Tiers * Dp - 8.f, 0.f, Tiers * Rise * 0.5f + 40.f), FVector(10.f, Wd * 0.5f + 20.f, Tiers * Rise * 0.5f + 40.f));
				P->AddProc(Seats, Wood);
				P->AddProc(Risers, WoodDark);
				P->AddProc(Sides, Stone);
				FZoogiMeshBuilder SnowTop;
				SnowTop.Box(FVector(X0 - Tiers * Dp - 8.f, 0.f, Tiers * Rise + 82.f), FVector(14.f, Wd * 0.5f + 24.f, 5.f));
				P->AddProc(SnowTop, Snow);
				P->RubbleKeepZ = 50.f;
				P->FinishBuild();
				if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -700.f), 0.1f + 0.12f * k, 1.3f);
			}
		}
		if (IsletPavilion[k])
		{
			AZoogiArenaPiece* P = SpawnPiece(C + Pol(IsletR * 0.62f, Yaw, 0.f), Yaw + 180.f, TEXT("pavilion"), TEXT("Stone"), 200.f);
			if (P)
			{
				FZoogiMeshBuilder Walls, Roof, RoofSnow, Trim;
				Walls.Box(FVector(0, 0, 110.f), FVector(120.f, 170.f, 110.f));
				// Gabled roof along Y.
				const float Rz = 220.f, Rh = 110.f, Rx = 150.f, Ry = 195.f;
				Roof.Quad(FVector(-Rx, -Ry, Rz), FVector(-Rx, Ry, Rz), FVector(0, Ry, Rz + Rh), FVector(0, -Ry, Rz + Rh), FVector(-1, 0, 1));
				Roof.Quad(FVector(Rx, -Ry, Rz), FVector(Rx, Ry, Rz), FVector(0, Ry, Rz + Rh), FVector(0, -Ry, Rz + Rh), FVector(1, 0, 1));
				Roof.FlatTri(FVector(-Rx * 0.8f, -Ry * 0.86f, Rz), FVector(Rx * 0.8f, -Ry * 0.86f, Rz), FVector(0, -Ry * 0.86f, Rz + Rh * 0.8f), FVector(0, -1, 0));
				Roof.FlatTri(FVector(-Rx * 0.8f, Ry * 0.86f, Rz), FVector(Rx * 0.8f, Ry * 0.86f, Rz), FVector(0, Ry * 0.86f, Rz + Rh * 0.8f), FVector(0, 1, 0));
				RoofSnow.Quad(FVector(-Rx + 8.f, -Ry, Rz + 7.f), FVector(-Rx + 8.f, Ry, Rz + 7.f), FVector(0, Ry, Rz + Rh + 9.f), FVector(0, -Ry, Rz + Rh + 9.f), FVector(-1, 0, 1));
				RoofSnow.Quad(FVector(Rx - 8.f, -Ry, Rz + 7.f), FVector(Rx - 8.f, Ry, Rz + 7.f), FVector(0, Ry, Rz + Rh + 9.f), FVector(0, -Ry, Rz + Rh + 9.f), FVector(1, 0, 1));
				for (int32 c = 0; c < 4; ++c) Trim.Box(FVector((c & 1 ? 1 : -1) * 118.f, (c & 2 ? 1 : -1) * 168.f, 110.f), FVector(10.f, 10.f, 112.f));
				P->AddProc(Walls, Stone);
				P->AddProc(Roof, StoneDark);
				P->AddProc(RoofSnow, Snow);
				P->AddProc(Trim, WoodDark);
				UStaticMeshComponent* Emb = P->AddBox(FVector(122.f, 0.f, 150.f), FVector(3.f, 46.f, 46.f), FRotator::ZeroRotator, Rune, false);
				P->MarkGlow(Emb);
				P->RubbleKeepZ = 60.f;
				P->FinishBuild();
				if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -700.f), 0.1f + 0.12f * k, 1.3f);
			}
		}
		// Bridge from the island's snow lip to the islet (destructible).
		{
			const FVector A = Pol(1760.f, Yaw, -18.f), B = Pol(IsletDist[k] - IsletR + 30.f, Yaw, -60.f);
			AZoogiArenaPiece* P = SpawnPiece((A + B) * 0.5f, Yaw, TEXT("bridge"), TEXT("Wood"), 90.f);
			if (P)
			{
				const float L = FVector::Dist2D(A, B);
				FZoogiMeshBuilder Planks, Rails;
				const int32 Np = FMath::RoundToInt(L / 40.f);
				for (int32 i = 0; i < Np; ++i)
				{
					const float T = (i + 0.5f) / Np;
					const float X = -L * 0.5f + L * T;
					const float Z = FMath::Lerp(A.Z, B.Z, T) - (A.Z + B.Z) * 0.5f - 40.f * 4.f * T * (1.f - T);
					Planks.Box(FVector(X, 0.f, Z), FVector(17.f, (i % 3) ? 70.f : 64.f, 4.f));
				}
				for (int32 s = -1; s <= 1; s += 2)
				{
					Rails.Box(FVector(-L * 0.5f, s * 72.f, 30.f), FVector(6.f, 6.f, 50.f));
					Rails.Box(FVector(L * 0.5f, s * 72.f, -10.f), FVector(6.f, 6.f, 50.f));
					Rails.Box(FVector(0.f, s * 72.f, 26.f), FVector(L * 0.5f, 2.5f, 2.5f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(B.Z - A.Z, L)), 0.f, 0.f));
				}
				P->AddProc(Planks, Wood);
				P->AddProc(Rails, WoodDark);
				P->RubbleKeepZ = 200.f;
				P->FinishBuild();
				if (Forming) Forming->AddActor(P, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.3f + 0.1f * k, 0.8f);
			}
		}
		{
			const int32 Before = Built.Num();
			AddWaterfall(Pol(IsletDist[k] - 60.f, Yaw + 25.f, -70.f), Yaw + 25.f, 90.f, 1200.f, 40.f);
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
	for (float FY : FallYaw) AddWaterfall(Pol(1775.f, FY, -16.f), FY, 120.f, 1500.f, 45.f);

	// ---- reactive floor scars ----
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Scars = GetWorld()->SpawnActor<AZoogiFloorScars>(AZoogiFloorScars::StaticClass(), FTransform::Identity, SP);
		if (Scars) Scars->Init(ZoogiConst::KnockoffRadius - 60.f);
	}

	// ---- forming: rune floor lights up while the stage assembles ----
	if (Forming && FloorMID) Forming->AddParamRamp(FloorMID, TEXT("GlowStrength"), 0.f, FloorGlowTarget, 0.0f, 2.4f);
	UE_LOG(LogZoogi, Log, TEXT("Frozen Ring built: %d pieces (4 indestructible pillars), %d forming entries (%.2f s), floor %s"), Pieces.Num(), Forming ? Forming->Num() : 0,
		Forming ? Forming->GetLength() : 0.f, FloorMID ? TEXT("art-derived") : TEXT("FALLBACK"));
}
