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
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

	// Oct 15 (Evan: Frozen must be scoreable, ping-pong action): knock-out gaps in the 95 cm ice ricochet ring. The 168391e
	// exit map found only 8 one-ray slivers (~72 cm) beside the diagonal bridges. Between each pair of adjacent bridges a
	// centred section (RimGapFrac of the bridge-to-bridge arc: 45 deg stretches -> ~4 m, 90 deg stretches -> ~8 m) of the
	// wall is replaced by a low sloped ice lip (RimGapLipH cm, RimGapLipSlope deg): a slow roll stops / rolls back, anything
	// with real pace rides it and drops past the court edge into the void (out below FallOutZ; nothing below catches it).
	// The back stretch (225..315) keeps the N combo bank wall (262..300) whole: its gap sits centred in the fill arc
	// 231.5..262 with the short-stretch width. Remaining walls keep their bank role. Reversible:
	// [ZoogiRoll.Frozen] bOpenRimGaps (default True), -ZoogiFrozenRimGaps=Off|On; RimGapFrac / RimGapLipH / RimGapLipSlope,
	// -ZoogiFrozenRimGapFrac= -ZoogiFrozenRimGapLipH=. Off = the exact pre-gap build (no extra pieces, same Rng draws).
	struct FRimGapCfg { bool bOn = true; float Frac = 0.35f; float LipH = 22.f; float Slope = 30.f; };
	const FRimGapCfg& RimGapCfg()
	{
		static const FRimGapCfg C = []
		{
			FRimGapCfg R;
			if (GConfig)
			{
				GConfig->GetBool(TEXT("ZoogiRoll.Frozen"), TEXT("bOpenRimGaps"), R.bOn, GGameIni);
				GConfig->GetFloat(TEXT("ZoogiRoll.Frozen"), TEXT("RimGapFrac"), R.Frac, GGameIni);
				GConfig->GetFloat(TEXT("ZoogiRoll.Frozen"), TEXT("RimGapLipH"), R.LipH, GGameIni);
				GConfig->GetFloat(TEXT("ZoogiRoll.Frozen"), TEXT("RimGapLipSlope"), R.Slope, GGameIni);
			}
			FString V;
			if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiFrozenRimGaps="), V)) R.bOn = !(V.Equals(TEXT("Off"), ESearchCase::IgnoreCase) || V == TEXT("0") || V.Equals(TEXT("False"), ESearchCase::IgnoreCase));
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiFrozenRimGapFrac="), R.Frac);
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiFrozenRimGapLipH="), R.LipH);
			R.Frac = FMath::Clamp(R.Frac, 0.05f, 0.6f);
			R.LipH = FMath::Clamp(R.LipH, 0.f, 60.f);
			R.Slope = FMath::Clamp(R.Slope, 10.f, 60.f);
			return R;
		}();
		return C;
	}
	// Gap arcs (deg, A0 < A1, inside 0..360, never wrapping): empty when the switch is off.
	const TArray<FVector2D>& RimGaps()
	{
		static TArray<FVector2D> G;
		static bool bInit = false;
		if (bInit) return G;
		bInit = true;
		if (!RimGapCfg().bOn) return G;
		const float B[7] = { 0.f, 45.f, 135.f, 180.f, 225.f, 315.f, 360.f };
		for (int32 i = 0; i < 6; ++i)
		{
			float Mid = 0.5f * (B[i] + B[i + 1]), W = RimGapCfg().Frac * (B[i + 1] - B[i]);
			if (Mid + 0.5f * W > WallNA0 && Mid - 0.5f * W < WallNA1)
			{
				Mid = 0.5f * (B[i] + BridgeWallGapDeg + WallNA0); // centred in the fill arc before the combo bank wall
				W = RimGapCfg().Frac * 45.f;
			}
			G.Add(FVector2D(Mid - 0.5f * W, Mid + 0.5f * W));
		}
		return G;
	}
	bool InRimGap(float YawDeg, float Margin = 0.f)
	{
		const float A = FMath::Fmod(FMath::Fmod(YawDeg, 360.f) + 360.f, 360.f);
		for (const FVector2D& G : RimGaps()) if (A >= G.X - Margin && A <= G.Y + Margin) return true;
		return false;
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
	FRandomStream PRng(2026); // polish r1 additions draw from their own stream: the r3c random layout stays bit-identical
	ExtraOrbSpots.Reset();
	// Oct 15: Ring 2 on (default) = the plan's layout moves; every random draw stays in place, so Off is bit-identical
	const bool bR2 = Ring2On();
	auto InRampMouth = [&](float YawDeg, float Margin) { if (!bR2) return false; for (float Y : R2RampYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(YawDeg, Y)) <= R2RampGapDeg + Margin) return true; return false; };
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
	// Polish r3: icy blue glacier underside / cliffs with painterly vertical striations (T_FrozenStreakTile), shadows lifted
	// into blue by emissive so the hero view no longer reads as grey-blue rock. Fallback detail: the r1 ice tile.
	const bool bStreak = FTex(TEXT("T_FrozenStreakTile")) != nullptr;
	const TCHAR* StreakTex = bStreak ? TEXT("T_FrozenStreakTile") : TEXT("T_FrozenIceTile");
	UMaterialInterface* Cliff = MakeFrozenMat(TEXT("Cliff"), ZoogiUtil::Hex(TEXT("#C2E5FB")), StreakTex, 650.f, 0.5f, 0.35f, 0.55f, ZoogiUtil::Hex(TEXT("#7FCBFA")), 0.22f);
	UMaterialInterface* IceUnder = MakeFrozenMat(TEXT("IceUnder"), ZoogiUtil::Hex(TEXT("#A3D4F6")), StreakTex, 520.f, 0.5f, 0.3f, 0.6f, ZoogiUtil::Hex(TEXT("#64BCF6")), 0.24f);
	// Mountain rock: blue-grey anime cliff faces under the snow (slope-split by BuildFrozenPeaks).
	// Polish r1 (Oct 8): icy blue glacier faces like frozen_zones_aerial (was blue-grey rock).
	UMaterialInterface* Peak = MakeFrozenMat(TEXT("Peak"), ZoogiUtil::Hex(TEXT("#9CCBF0")), StreakTex, 2400.f, 0.45f, 0.55f, 0.45f, ZoogiUtil::Hex(TEXT("#6FB8F0")), 0.16f);
	UMaterialInterface* Glacier = MakeFrozenMat(TEXT("Glacier"), ZoogiUtil::Hex(TEXT("#E2F3FD")), StreakTex, 2200.f, 0.5f, 0.4f, 0.55f, ZoogiUtil::Hex(TEXT("#A8DCFF")), 0.22f);
	UMaterialInterface* GlacierDeep = MakeFrozenMat(TEXT("GlacierDeep"), ZoogiUtil::Hex(TEXT("#A6CFF0")), StreakTex, 2200.f, 0.5f, 0.35f, 0.55f, ZoogiUtil::Hex(TEXT("#6FB4EE")), 0.14f); // r2: paler (r1 read as dark blue stripes)
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
		FloorMID->SetScalarParameterValue(TEXT("GlowTint"), 0.5f); // polish r2: bluer ice (was 0.35)
		FloorMID->SetScalarParameterValue(TEXT("DetailStrength"), 0.12f);
		FloorMID->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
		FloorMID->SetScalarParameterValue(TEXT("Specular"), 0.6f);
		FloorMID->SetScalarParameterValue(TEXT("Pulse"), 0.12f);
	}
	BuildIsland(ZoogiUtil::Hex(TEXT("#CFEFFF")), ZoogiUtil::Hex(TEXT("#9ED8F5")), ZoogiUtil::Hex(TEXT("#F4FBFF")),
		ZoogiUtil::Hex(TEXT("#7FC0EE")), ZoogiUtil::Hex(TEXT("#BFE4FB")), 0.03f, FloorMat); // polish r3: glacier-blue under-cones (were grey-blue)
	// Polish r1: bright blue painterly sky with soft cumulus + aurora / rainbow flares (T_FrozenBrightSky); r1 sky fallback.
	if (!AddPanoramaSky(TEXT("FrozenBright"), 1.0f, 0.f, FLinearColor::White)) AddPanoramaSky(TEXT("Frozen"), 1.0f, 0.f, FLinearColor::White);

	// Snow lip (outside the knockout line, top below the court at the line so balls roll off as before) + ice cliff face.
	{
		FZoogiMeshBuilder SnowM, CliffM;
		const int32 Seg = 144;
		auto H = [&](float R, float A) -> float
		{
			const float N = FMath::PerlinNoise2D(FVector2D(FMath::Cos(A) * 3.1f, FMath::Sin(A) * 3.1f)) * 0.5f + 0.5f;
			if (R < 1560.f) return -4.f;
			if (bR2) return -4.f - 6.f * (R - 1553.f); // Ring 2: the rim breaks away into the crevasse (r1550-1750)
			// Round 3c: the snow lip dips under each bridge (the deck runs from the court edge down to the islet).
			if (NearIsletYaw(FMath::RadiansToDegrees(A), 5.f)) return -4.f - 0.3f * (R - 1550.f);
			if (InRimGap(FMath::RadiansToDegrees(A))) return -4.f - 0.55f * (R - 1550.f); // rim gap: the berm breaks away into a drop
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
		if (!bR2) AddFrozenProc(CliffM, Cliff, nullptr, true); // Ring 2 builds the crevasse's own cliff faces (r1555 / r1748)
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
		if (!bR2) AddFrozenProc(Drapes, Snow, nullptr, true); // r1790 drapes would poke through the Ring 2 floor
		AddFrozenProc(Under, IceUnder, nullptr, true);
		AddFrozenProc(Icicles, IcePane, nullptr, false);
		// Round 2: lumpy snow mounds along the lip (the art's thick snow rim), no collision.
		FZoogiMeshBuilder Mounds;
		for (int32 i = 0; i < 64; ++i)
		{
			const float A = i * 360.f / 64 + Rng.FRandRange(-2.5f, 2.5f);
			if (NearIsletYaw(A, 6.f)) continue;
			const float R = Rng.FRandRange(1645.f, 1715.f);
			FZoogiMeshBuilder Skip; // rim gap: same Rng draws, mound not shown (the berm is cut there)
			(InRimGap(A, 3.f) ? Skip : Mounds).Chunk(Pol(R, A, Rng.FRandRange(18.f, 30.f)), FVector(Rng.FRandRange(55.f, 85.f), Rng.FRandRange(80.f, 130.f), Rng.FRandRange(22.f, 40.f)), Rng, 0.18f, A);
		}
		if (!bR2) AddFrozenProc(Mounds, Snow, nullptr, true); // the r1645-1715 berm would sit over the crevasse
		// Polish r1: deep icy cliff underside (frozen_zones_aerial): a ridged inverted glacier cone of vertical ice columns
		// under the rim cliff, tapering to a jagged point far below (visual only, no collision, no shadow).
		{
			FZoogiMeshBuilder ColLit, ColDeep;
			const int32 NS = 56, NL = 7;
			TArray<FVector> Vt; Vt.SetNum(NS * (NL + 1));
			for (int32 l = 0; l <= NL; ++l)
				for (int32 j = 0; j < NS; ++j)
				{
					const float T = (float)l / NL;
					const float A = 2.f * PI * (j + (l > 0 ? PRng.FRandRange(-0.18f, 0.18f) : 0.f)) / NS;
					const bool bRidge = (j % 2) == 0;
					float R = FMath::Lerp(1745.f, 140.f, FMath::Pow(T, 1.25f)) * (bRidge ? PRng.FRandRange(1.0f, 1.07f) : PRng.FRandRange(0.86f, 0.93f));
					float Z = FMath::Lerp(-560.f, -3600.f, T) + (l > 0 && l < NL ? PRng.FRandRange(-120.f, 120.f) : 0.f) - (l == NL ? PRng.FRandRange(0.f, 900.f) : 0.f);
					if (l == 0) { R = 1700.f + 60.f * FMath::PerlinNoise1D(A * 5.f + 3.f); Z = -600.f; }
					Vt[l * NS + j] = FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
				}
			for (int32 l = 0; l < NL; ++l)
				for (int32 j = 0; j < NS; ++j)
				{
					const int32 J1 = (j + 1) % NS;
					const FVector& A0 = Vt[l * NS + j]; const FVector& A1 = Vt[l * NS + J1];
					const FVector& B0 = Vt[(l + 1) * NS + j]; const FVector& B1 = Vt[(l + 1) * NS + J1];
					const FVector Out = FVector(FVector2D(A0 + A1).GetSafeNormal(), -0.25f);
					FZoogiMeshBuilder& Mb = ((j % 2) == 1 || PRng.FRand() < 0.18f) ? ColDeep : ColLit;
					Mb.FlatTri(A0, B0, B1, Out);
					Mb.FlatTri(A0, B1, A1, Out);
				}
			AddFrozenProc(ColLit, Glacier, nullptr, false);
			AddFrozenProc(ColDeep, GlacierDeep, nullptr, false);
		}
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

	// ---- Polish r6: sky band (frozen_zones_aerial: open bright sky + clouds above the far mountains at the top centre).
	// Every mountain behind the arena stays under a sight line from the ref_aerial camera (0, 6997, 2690; pitch 12): about
	// 1.5 deg under the horizon at the frame centre, relaxing toward the frame edges so the peaks still frame both sides.
	const FVector2D SkyCam(0.f, 6997.f);
	const float SkyCamZ = 2690.f;
	auto SkyCapZ = [SkyCam, SkyCamZ](const FVector2D& P) -> float
	{
		const FVector2D D = P - SkyCam;
		if (D.Y > -1000.f) return 1.e6f; // not in front of that camera
		const float Az = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(D.X), -D.Y)); // 0 = frame centre, ~31 = frame edge
		const float Elev = -1.5f + FMath::Clamp((Az - 14.f) / 14.f, 0.f, 1.f) * 13.5f;
		return SkyCamZ + D.Size() * FMath::Tan(FMath::DegreesToRadians(Elev));
	};
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
		// r5: camera-side near / middle clusters are replaced by pack mountain meshes (still generated so the Rng stream is unchanged).
		const bool bPackFront = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/Arenas/FrozenMountains/SM_FrozenMtn_02.SM_FrozenMtn_02"), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
		bool bDropMtn = false;
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
				if (bDropMtn) return;
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
			H = FMath::Max(FMath::Min(H, (SkyCapZ(FVector2D(Pol(R, A))) - Z0) / 1.15f), 250.f); // r6: sky band
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
			bDropMtn = bPackFront && FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 60.f;
			Cluster(Rng.FRandRange(14000.f, 19000.f), A, Rng.FRandRange(4200.f, 6800.f) * (bFront ? 0.55f : 1.f), -5600.f);
		}
		for (int32 i = 0; i < 20; ++i)
		{
			const float A = i * 18.f + Rng.FRandRange(-5.f, 5.f);
			const bool bFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 40.f;
			bDropMtn = bPackFront && FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) < 60.f;
			Cluster(Rng.FRandRange(6800.f, 11000.f), A, Rng.FRandRange(3200.f, 5200.f) * (bFront ? 0.6f : 1.f), -5600.f);
		}
		bDropMtn = false;
		AddFrozenProc(Rock, Peak, nullptr, false);
		AddFrozenProc(Caps, Snow, nullptr, false);
	}

	// ---- Polish r1: towering icy cliffs / spires close around the arena (frozen_zones_aerial: the island floats inside a ring
	// of tall glacier peaks that rise far above it; lower on the camera side so the top-down / 3/4 views stay open). Visual only:
	// no collision, no shadows (they must never darken the court), outside every gameplay / aerial-prop zone (R >= 5600).
	{
		FZoogiMeshBuilder Lit, Deep, Cap;
		// r4: behind the arena the pack mountain meshes take over; those spires are still generated (same PRng stream) but dropped.
		const bool bPackMtns = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/Arenas/FrozenMountains/SM_FrozenMtn_02.SM_FrozenMtn_02"), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
		bool bDropSpire = false;
		auto Spire = [&](const FVector2D& Cc, float Rb, float H, float Z0)
		{
			const int32 NR = 10, NS = 14;
			TArray<FVector> Vt;
			Vt.SetNum(NR * NS + 1);
			const FVector2D Lean(PRng.FRandRange(-0.18f, 0.18f) * Rb, PRng.FRandRange(-0.18f, 0.18f) * Rb);
			Vt[0] = FVector(Cc + Lean, Z0 + H);
			const float Ph = PRng.FRandRange(0.f, 2.f * PI);
			for (int32 i = 1; i <= NR; ++i)
				for (int32 j = 0; j < NS; ++j)
				{
					const float T = (float)i / NR;
					const float A = Ph + 2.f * PI * (j + PRng.FRandRange(-0.25f, 0.25f)) / NS;
					const bool bRidge = (j % 2) == 0;
					// Steep columnar profile: radius grows fast near the top then runs almost straight down (cliff walls).
					const float Prof = FMath::Pow(T, 0.62f); // r2: more pointed peaks
					const float Rr = Rb * Prof * (bRidge ? PRng.FRandRange(1.04f, 1.16f) : PRng.FRandRange(0.74f, 0.86f));
					const float Hh = H * (1.f - T) + (i < NR ? PRng.FRandRange(-0.04f, 0.04f) * H : -0.1f * H);
					Vt[1 + (i - 1) * NS + j] = FVector(Cc + Lean * (1.f - Prof) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr, Z0 + Hh);
				}
			const FVector Ctr(Cc, Z0 + H * 0.4f);
			auto Face = [&](const FVector& P0, const FVector& P1, const FVector& P2, int32 j)
			{
				const FVector Cen = (P0 + P1 + P2) / 3.f;
				FVector Nn = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
				const FVector Out = (Cen - Ctr).GetSafeNormal2D() + FVector(0, 0, 0.2f);
				if (FVector::DotProduct(Nn, Out) < 0.f) Nn = -Nn;
				const bool bSnow = Nn.Z > 0.55f;
				if (bDropSpire) return;
				(bSnow ? Cap : ((j % 3) == 1 ? Deep : Lit)).FlatTri(P0, P1, P2, Out); // r3: fewer deep columns (r2 read as stripes)
			};
			for (int32 j = 0; j < NS; ++j)
			{
				const int32 J1 = (j + 1) % NS;
				Face(Vt[0], Vt[1 + j], Vt[1 + J1], j);
				for (int32 i = 1; i < NR; ++i)
				{
					const FVector& A0 = Vt[1 + (i - 1) * NS + j]; const FVector& A1 = Vt[1 + (i - 1) * NS + J1];
					const FVector& B0 = Vt[1 + i * NS + j]; const FVector& B1 = Vt[1 + i * NS + J1];
					Face(A0, B0, B1, j);
					Face(A0, B1, A1, j);
				}
			}
		};
		for (int32 i = 0; i < 26; ++i)
		{
			const float A = i * (360.f / 26) + PRng.FRandRange(-4.f, 4.f);
			const float DFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)); // 0 = camera side (art bottom)
			if (DFront < 40.f) continue;                                             // keep the camera side open
			const float Back = FMath::Clamp((DFront - 40.f) / 120.f, 0.f, 1.f);     // 1 = straight behind the arena (art top)
			// r2: further out and lower so the sky shows above the peaks (r1 walled the whole view).
			const float R = PRng.FRandRange(6400.f, 8800.f) + (1.f - Back) * 1800.f;
			const float Top = FMath::Lerp(-1400.f, 3200.f, FMath::Pow(Back, 1.4f)) + PRng.FRandRange(-600.f, 900.f); // r3: lower flanks, sky shows in the corners
			const float H = Top + 6500.f;
			const FVector2D C0(Pol(R, A));
			bDropSpire = bPackMtns; // r5: the camera-side corner spires are pack mountains too
			Spire(C0, H * PRng.FRandRange(0.16f, 0.22f), H, -6500.f);
			if (PRng.FRand() < 0.7f) // shoulder spire
				Spire(C0 + FVector2D(Pol(H * 0.18f, A + (PRng.FRand() < 0.5f ? 90.f : -90.f))), H * 0.13f, H * PRng.FRandRange(0.6f, 0.82f), -6500.f);
		}
		AddFrozenProc(Lit, Glacier, nullptr, false);
		AddFrozenProc(Deep, GlacierDeep, nullptr, false);
		AddFrozenProc(Cap, Snow, nullptr, false);
	}

	// ---- Polish r4: real mountain meshes behind the spire ring (free "Iceland_Environment" pack, derived copies in
	// /Game/Env/Arenas/FrozenMountains on M_FrozenMountain: icy blue + painterly vertical striations, snow on up-facing slopes,
	// deep blue toward the base). Visual only: no collision, no shadows, never inside a gameplay / aerial-prop zone (R >= 9000).
	// Each mesh is normalised from its own bounds (height / base width / bottom centre), so pack pivots and sizes do not matter.
	{
		TArray<UStaticMesh*> Mtns;
		for (int32 k = 2; k <= 9; ++k)
		{
			const FString Pth = FString::Printf(TEXT("/Game/Env/Arenas/FrozenMountains/SM_FrozenMtn_%02d.SM_FrozenMtn_%02d"), k, k);
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Pth, nullptr, LOAD_NoWarn | LOAD_Quiet)) Mtns.Add(M);
		}
		UMaterialInterface* MtnBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenMountain.M_FrozenMountain"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UE_LOG(LogZoogi, Log, TEXT("Frozen mountains: %d pack meshes, material=%s"), Mtns.Num(), MtnBase ? TEXT("ok") : TEXT("missing"));
		if (Mtns.Num() > 0 && MtnBase)
		{
			UMaterialInstanceDynamic* MtnMat = UMaterialInstanceDynamic::Create(MtnBase, this);
			// r5: paler icy blue, more snow on up-facing slopes, softer striations, bright sky haze with distance.
			MtnMat->SetVectorParameterValue(TEXT("Color"), ZoogiUtil::Hex(TEXT("#D2ECFD")));
			MtnMat->SetVectorParameterValue(TEXT("DeepColor"), ZoogiUtil::Hex(TEXT("#93C6EE")));
			MtnMat->SetVectorParameterValue(TEXT("SnowColor"), ZoogiUtil::Hex(TEXT("#F7FBFF")));
			MtnMat->SetVectorParameterValue(TEXT("HazeColor"), ZoogiUtil::Hex(TEXT("#D6EBFC")));
			MtnMat->SetScalarParameterValue(TEXT("HazeStart"), 4000.f);
			MtnMat->SetScalarParameterValue(TEXT("HazeEnd"), 26000.f);
			MtnMat->SetScalarParameterValue(TEXT("HazeMax"), 0.7f);
			MtnMat->SetScalarParameterValue(TEXT("HazeGlow"), 0.55f);
			if (UTexture* T = FTex(TEXT("T_FrozenStreakTile"))) MtnMat->SetTextureParameterValue(TEXT("Detail"), T);
			MtnMat->SetScalarParameterValue(TEXT("DetailScale"), 2600.f);
			MtnMat->SetScalarParameterValue(TEXT("DetailStrength"), 0.24f);
			MtnMat->SetScalarParameterValue(TEXT("SnowSlope"), 0.42f);
			MtnMat->SetScalarParameterValue(TEXT("SnowZ"), -4500.f);
			MtnMat->SetScalarParameterValue(TEXT("SnowZFade"), 3500.f);
			MtnMat->SetScalarParameterValue(TEXT("DeepZ"), -7000.f);
			MtnMat->SetScalarParameterValue(TEXT("DeepZFade"), 5000.f);
			MtnMat->SetVectorParameterValue(TEXT("EmissiveColor"), ZoogiUtil::Hex(TEXT("#9ED6FA")));
			MtnMat->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.26f);
			FRandomStream MRng(4242);
			const int32 NM = 22; // r5: denser ring (the flank spires are gone)
			for (int32 i = 0; i < NM; ++i)
			{
				const float A = i * (360.f / NM) + MRng.FRandRange(-6.f, 6.f);
				const float DFront = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f));
				if (DFront < 30.f) continue;                                           // camera side stays open
				const float Back = FMath::Clamp((DFront - 30.f) / 130.f, 0.f, 1.f);
				UStaticMesh* M = Mtns[(i * 3 + MRng.RandRange(0, 1)) % Mtns.Num()];
				const FBox B = M->GetBoundingBox();
				const FVector Sz = B.GetSize();
				if (Sz.Z < 1.f) continue;
				const float Z0 = -7000.f;
				const bool bFar = Back > 0.4f && (i % 2) == 1;                         // r5b: alternate far layer behind the arena (hazes out, less wall-like)
				const float Top = FMath::Lerp(-800.f, 4800.f, FMath::Pow(Back, 1.2f)) + MRng.FRandRange(-500.f, 900.f) + (bFar ? 2600.f : 0.f); // r5: low on the camera-side corners
				const float Hm = Top - Z0;
				const float Wm = Hm * MRng.FRandRange(1.1f, 1.6f);
				float Sz3 = Hm / (float)Sz.Z;
				const float Sxy = Wm / FMath::Max3((float)Sz.X, (float)Sz.Y, 1.f);
				// behind the arena (where the spires are dropped) they come in close; on the flanks they sit behind the spires
				// r5: the flanks come in to the old spire radius (6400-10600) now that the hand-built spires are gone
				const float R = (bFar ? MRng.FRandRange(13000.f, 15500.f) : Back > 0.4f ? MRng.FRandRange(7600.f, 9000.f) : MRng.FRandRange(7000.f, 9000.f)) + Wm * 0.2f;
				{ const float Cap = SkyCapZ(FVector2D(Pol(R, A))); if (Top > Cap) Sz3 = FMath::Max(Cap - Z0, 900.f) / (float)Sz.Z; } // r6: sky band (width kept: low wide ridges)
				const FRotator Rot(0.f, MRng.FRandRange(0.f, 360.f), 0.f);
				const FVector Scale(Sxy, Sxy, Sz3);
				const FVector Pivot = Rot.RotateVector(FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z) * Scale);
				if (UStaticMeshComponent* C = AddMesh(M, MtnMat, false, false))
				{
					C->SetRelativeTransform(FTransform(Rot, FVector(Pol(R, A)) + FVector(0.f, 0.f, Z0) - Pivot, Scale));
					C->SetCanEverAffectNavigation(false);
				}
			}
			// r5: camera-side ranges (yaw 90 +/-60) are pack mountains too, low and far (peaks below the court) so the views stay open.
			for (int32 i = 0; i < 9; ++i)
			{
				const float A = 90.f + (i - 4) * 13.f + MRng.FRandRange(-4.f, 4.f);
				const float Edge = FMath::Abs(FMath::FindDeltaAngleDegrees(A, 90.f)) / 60.f; // 0 = straight under the camera
				UStaticMesh* M = Mtns[(i * 5 + 1) % Mtns.Num()];
				const FBox B = M->GetBoundingBox();
				const FVector Sz = B.GetSize();
				if (Sz.Z < 1.f) continue;
				const float Z0 = -7000.f;
				const float Top = FMath::Lerp(-3200.f, -1200.f, Edge) + MRng.FRandRange(-400.f, 500.f);
				const float Hm = Top - Z0;
				const float Wm = Hm * MRng.FRandRange(1.4f, 1.9f);
				const FVector Scale(Wm / FMath::Max3((float)Sz.X, (float)Sz.Y, 1.f), Wm / FMath::Max3((float)Sz.X, (float)Sz.Y, 1.f), Hm / (float)Sz.Z);
				const FRotator Rot(0.f, MRng.FRandRange(0.f, 360.f), 0.f);
				const FVector Pivot = Rot.RotateVector(FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z) * Scale);
				const float R = MRng.FRandRange(11000.f, 16000.f) + Wm * 0.2f;
				if (UStaticMeshComponent* C = AddMesh(M, MtnMat, false, false))
				{
					C->SetRelativeTransform(FTransform(Rot, FVector(Pol(R, A)) + FVector(0.f, 0.f, Z0) - Pivot, Scale));
					C->SetCanEverAffectNavigation(false);
				}
			}
		}
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

	// ---- Polish r1: glowing blue rune seal overlay on the ice floor (frozen-assist T_FrozenSealHalo_RE, traced on the carved
	// runes; UV = WorldXY / 3100 + 0.5). Unlit translucent quad 0.9 cm above the floor, no collision; it ignites with the
	// rune floor during the stage-forming intro.
	{
		UMaterialInstanceDynamic* SealMID = nullptr;
		FlatQuad(FTex(TEXT("T_FrozenSealHalo")), FVector(0.f, 0.f, 0.9f), 0.f, 2.f * ZoogiConst::KnockoffRadius, 1.0f, 2.6f, 1.0f, FLinearColor(0.75f, 0.95f, 1.f), &SealMID); // r6: emissive 2.0 -> 2.6
		if (SealMID && Forming) { SealMID->SetScalarParameterValue(TEXT("Opacity"), 0.f); Forming->AddParamRamp(SealMID, TEXT("Opacity"), 0.f, 1.0f, 0.3f, 2.3f); }
		// Fountain basin glow under the hub crystal (bright basin rings).
		FZoogiMeshBuilder Basin;
		for (int32 s = 0; s < 64; ++s)
		{
			const float A0 = 2.f * PI * s / 64, A1 = 2.f * PI * (s + 1) / 64;
			for (const FVector2D& Rr : { FVector2D(118.f, 132.f), FVector2D(54.f, 60.f), FVector2D(176.f, 184.f), FVector2D(232.f, 236.f) }) // r6: stepped basin tiers
				Basin.Quad(FVector(FMath::Cos(A0) * Rr.X, FMath::Sin(A0) * Rr.X, 1.1f), FVector(FMath::Cos(A1) * Rr.X, FMath::Sin(A1) * Rr.X, 1.1f),
					FVector(FMath::Cos(A1) * Rr.Y, FMath::Sin(A1) * Rr.Y, 1.1f), FVector(FMath::Cos(A0) * Rr.Y, FMath::Sin(A0) * Rr.Y, 1.1f), FVector::UpVector);
		}
		if (UProceduralMeshComponent* Bc = AddFrozenProc(Basin, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 3.0f), nullptr, false)) Bc->ComponentTags.Add(TEXT("NoCamBlock"));
		// r6: glowing water in the basin (soft additive discs, brightest at the centre). Flat, 1.2-1.4 cm above the floor, no collision.
		{
			FZoogiMeshBuilder Pool0, Pool1;
			for (int32 s = 0; s < 64; ++s)
			{
				const float A0 = 2.f * PI * s / 64, A1 = 2.f * PI * (s + 1) / 64;
				Pool0.Quad(FVector(0.f, 0.f, 1.2f), FVector(0.f, 0.f, 1.2f), FVector(FMath::Cos(A1) * 176.f, FMath::Sin(A1) * 176.f, 1.2f), FVector(FMath::Cos(A0) * 176.f, FMath::Sin(A0) * 176.f, 1.2f), FVector::UpVector);
				Pool1.Quad(FVector(0.f, 0.f, 1.4f), FVector(0.f, 0.f, 1.4f), FVector(FMath::Cos(A1) * 118.f, FMath::Sin(A1) * 118.f, 1.4f), FVector(FMath::Cos(A0) * 118.f, FMath::Sin(A0) * 118.f, 1.4f), FVector::UpVector);
			}
			for (UProceduralMeshComponent* Pc : { AddFrozenProc(Pool0, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#5FD8FF")), 0.32f), nullptr, false),
				AddFrozenProc(Pool1, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#A8F2FF")), 0.42f), nullptr, false) })
				if (Pc) Pc->ComponentTags.Add(TEXT("NoCamBlock"));
		}
	}
	// ---- Polish r3: ring of 8 small glowing crystal posts on the seal's outer ring (r 1000, between the spawn pads and clear of
	// the ramp / platform / combo section). Breakable pieces with NO collider (play-floor collision unchanged); low (30 cm,
	// under a Zoogi's 50 cm radius) so a ball rolling over one hides it. They rise with the obelisks during the forming.
	{
		UMaterialInterface* PostGlow = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 1.8f);
		for (int32 k = 0; k < 8; ++k)
		{
			const float Yaw = 22.5f + 45.f * k;
			AZoogiArenaPiece* P = SpawnPiece(Pol(1000.f, Yaw), Yaw, TEXT("seal_crystal"), TEXT("Ice"), 40.f);
			if (!P) continue;
			FZoogiMeshBuilder Cr, Base;
			Cr.Prism(FVector2D::ZeroVector, 6, 9.f, 7.f, 0.f, 20.f, Yaw, 10.f, false);
			Cr.Prism(FVector2D(9.f, 5.f), 5, 5.f, 4.f, 0.f, 11.f, Yaw + 20.f, 6.f, false);
			Cr.Prism(FVector2D(-7.f, -7.f), 5, 5.f, 4.f, 0.f, 9.f, Yaw + 40.f, 5.f, false);
			Base.Prism(FVector2D::ZeroVector, 8, 20.f, 17.f, 0.f, 2.f, Yaw, 0.f, false);
			UProceduralMeshComponent* Cm = P->AddProc(Cr, PostGlow, false);
			P->MarkGlow(Cm);
			P->AddProc(Base, IceDeep, false);
			P->RubbleKeepZ = 3.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -40.f), 0.75f + 0.05f * k, 0.6f, 3.f);
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
	// Rim gaps (see RimGapCfg): each wall arc minus the gap arcs; off = the original calls.
	// Ring 2: plus a +/- R2RampGapDeg opening at each re-entry ramp (plan: the ramps aim straight in through the Ring 1 wall)
	TArray<FVector2D> CutGaps = RimGaps();
	if (bR2) for (float Y : R2RampYaw) CutGaps.Add(FVector2D(Y - R2RampGapDeg, Y + R2RampGapDeg));
	auto BuildWallCut = [&](float A0, float A1, bool bCombo)
	{
		if (CutGaps.Num() == 0) { BuildWall(A0, A1, bCombo); return; }
		float S = A0;
		TArray<FVector2D> G = CutGaps; G.Sort([](const FVector2D& a, const FVector2D& b) { return a.X < b.X; });
		for (const FVector2D& Gp : G)
		{
			if (Gp.Y <= S || Gp.X >= A1) continue;
			if (Gp.X - S > 0.5f) BuildWall(S, Gp.X, bCombo);
			S = FMath::Max(S, Gp.Y);
		}
		if (A1 - S > 0.5f) BuildWall(S, A1, bCombo);
	};
	BuildWallCut(WallNA0, WallNA1, true);
	BuildWallCut(WallWA0, WallWA1, false);
	BuildWallCut(WallEA0, WallEA1, false);
	// Bridge alignment (Evan 10:49 PM): W / E arcs and the fill arcs leave every bridge a clear gap of +/- BridgeWallGapDeg.
	// Polish r2 (Evan 9:16 PM): wall the rest of the ring edge with the same breakable ice walls (same style / height /
	// HP / collider / forming). Symmetric layout (Oct 9 12:30 AM): only the 4 bridge gaps (45 / 135 / 225 / 315 +/- 6.5)
	// stay open. Arcs: ZoogiFrozen::WallFillArcs.
	for (const FVector2D& Arc : WallFillArcs) BuildWallCut(Arc.X, Arc.Y, false);

	// ---- rim gaps: low sloped ice lip + ice-edge trim + crystal posts + a soft glow line (indestructible) ----
	if (RimGaps().Num() > 0)
	{
		const FRimGapCfg& Cfg = RimGapCfg();
		UPhysicalMaterial* LipPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.3f, false);
		UMaterialInterface* GapGlow = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#8FEFFF")), 1.3f);
		const float Th = FMath::DegreesToRadians(Cfg.Slope);
		const float Run = Cfg.LipH > 1.f ? Cfg.LipH / FMath::Tan(Th) : 0.f;
		const float RFoot = WallR - Run, RBack = WallR + WallThick;
		FString GapLog;
		int32 k = 0;
		for (const FVector2D& Gp : RimGaps())
		{
			const float Ga = Gp.X, Gb = Gp.Y;
			const int32 Segs = FMath::Max(2, FMath::CeilToInt((Gb - Ga) / 2.f));
			AZoogiArenaPiece* P = SpawnPiece(FVector::ZeroVector, 0.f, TEXT("rimgap"), TEXT("Ice"), 100.f, true);
			if (!P) continue;
			FZoogiMeshBuilder Ramp, Cap, Trim, Line, Posts, Tips;
			for (int32 s = 0; s < Segs; ++s)
			{
				const float Sa = FMath::Lerp(Ga, Gb, (float)s / Segs), Sb = FMath::Lerp(Ga, Gb, (float)(s + 1) / Segs), Sm = 0.5f * (Sa + Sb);
				if (InRampMouth(Sm, 1.f)) continue; // Ring 2: a re-entry ramp comes up here - no lip in its mouth
				if (bR2 && Cfg.LipH > 1.f)
				{
					Cap.ArcSlab(WallR - 1.f, RBack, Sa, Sb, -6.f, Cfg.LipH, 1, 3.f);
					Line.ArcSlab(WallR - 4.f, WallR + 3.f, Sa, Sb, Cfg.LipH + 1.f, Cfg.LipH + 3.f, 1, 0.f);
				}
				if (Cfg.LipH > 1.f)
				{
					// ice ramp face (visual) + its pitched collider box
					Ramp.Quad(Pol(RFoot, Sa, 0.5f), Pol(RFoot, Sb, 0.5f), Pol(WallR, Sb, Cfg.LipH), Pol(WallR, Sa, Cfg.LipH), FVector(-FMath::Sin(Th) * FMath::Cos(FMath::DegreesToRadians(Sm)), -FMath::Sin(Th) * FMath::Sin(FMath::DegreesToRadians(Sm)), FMath::Cos(Th)));
					const float L = Run / FMath::Cos(Th), T = 20.f;
					const float Rm = WallR - 0.5f * Run + FMath::Sin(Th) * T * 0.5f, Zm = 0.5f * Cfg.LipH - FMath::Cos(Th) * T * 0.5f;
					const float SegLen = (WallR - 0.5f * Run) * FMath::DegreesToRadians(Sb - Sa) * 1.04f;
					P->AddColliderBox(Pol(Rm, Sm, Zm), FVector(L, SegLen, T), FRotator(Cfg.Slope, Sm, 0.f), TEXT("ZRimLip"), LipPM);
					// flat top behind the crest (where the wall stood), down to the court floor
					const float SegLen2 = (WallR + WallThick * 0.5f) * FMath::DegreesToRadians(Sb - Sa) * 1.04f;
					P->AddColliderBox(Pol(WallR + WallThick * 0.5f, Sm, 0.5f * (Cfg.LipH - 20.f)), FVector(WallThick, SegLen2, Cfg.LipH + 20.f), FRotator(0.f, Sm, 0.f), TEXT("ZRimLip"), LipPM);
				}
			}
			if (Cfg.LipH > 1.f && !bR2)
			{
				Cap.ArcSlab(WallR - 1.f, RBack, Ga, Gb, -6.f, Cfg.LipH, Segs, 3.f);
				Line.ArcSlab(WallR - 4.f, WallR + 3.f, Ga + 0.4f, Gb - 0.4f, Cfg.LipH + 1.f, Cfg.LipH + 3.f, Segs, 0.f);
			}
			// ice-edge trim along the court's drop edge in the gap (visual)
			Trim.ArcSlab(1528.f, 1556.f, Ga, Gb, -10.f, 1.5f, Segs, 0.f);
			// crystal posts at both gap ends (on the cut wall ends) with glowing tips
			float PA = Ga, PB = Gb; // Ring 2: a ramp mouth overlapping the gap end moves that post to the opening's real end
			if (bR2) for (float Y : R2RampYaw)
			{
				if (Y - R2RampGapDeg < PA && Y + R2RampGapDeg >= PA) PA = Y - R2RampGapDeg;
				if (Y + R2RampGapDeg > PB && Y - R2RampGapDeg <= PB) PB = Y + R2RampGapDeg;
			}
			for (int32 e = 0; e < 2; ++e)
			{
				const float A = e == 0 ? PA : PB;
				const FVector2D XY(Pol(WallR + WallThick * 0.5f, A));
				Posts.Prism(XY, 6, 24.f, 13.f, -6.f, 150.f, A + 15.f, 0.f, false);
				Posts.Prism(XY + FVector2D(Pol(16.f, A + (e == 0 ? -90.f : 90.f))), 5, 11.f, 4.f, -6.f, 92.f, A, 0.f, false);
				Tips.Prism(XY, 6, 13.f, 0.f, 150.f, 186.f, A + 15.f, 0.f, false);
				P->AddColliderCyl(XY, 24.f, -6.f, 150.f, TEXT("ZWall"), WallPM);
			}
			P->AddProc(Ramp, IcePane);
			if (!Cap.IsEmpty()) P->AddProc(Cap, Snow);
			P->AddProc(Trim, IcePane);
			if (!Line.IsEmpty()) { UProceduralMeshComponent* Lc = P->AddProc(Line, GapGlow, false); P->MarkGlow(Lc); }
			P->AddProc(Posts, IcePane);
			{ UProceduralMeshComponent* Tc = P->AddProc(Tips, Rune, false); P->MarkGlow(Tc); }
			P->RubbleKeepZ = 20.f;
			P->FinishBuild();
			if (Forming) Forming->AddActor(P, EZoogiFormStyle::Rise, FVector(0, 0, -200.f), 0.75f + 0.05f * k, 0.7f);
			GapLog += FString::Printf(TEXT("%s%.1f..%.1f deg (~%.0f cm)"), GapLog.IsEmpty() ? TEXT("") : TEXT(", "), Ga, Gb, WallR * FMath::DegreesToRadians(Gb - Ga));
			++k;
		}
		UE_LOG(LogZoogi, Log, TEXT("FROZEN rim gaps ON: %d gap(s) [%s], lip %.0f cm at %.0f deg (frac %.2f); -ZoogiFrozenRimGaps=Off reverts"), k, *GapLog, Cfg.LipH, Cfg.Slope, Cfg.Frac);
	}
	else UE_LOG(LogZoogi, Log, TEXT("FROZEN rim gaps OFF (closed ice ring)"));

	// ---- rim arches (SE = Recovery Gate with a portal sheet) ----
	for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(ArchYaw); ++k)
	{
		const float Yaw = ArchYaw[k];
		// Bridge arches (Evan 10:49 PM): centred on the bridge yaw / centreline and wider so the deck + rails pass cleanly.
		const float HW = NearIsletYaw(Yaw, 1.f) ? BridgeArchHalfW : ArchHalfW;
		AZoogiArenaPiece* P = SpawnPiece(Pol(ArchR, Yaw), Yaw, TEXT("arch"), TEXT("Stone"), 160.f); // symmetric: 4 identical bridge arches (no gate portal sheet)
		if (!P) continue;
		// Local: X = outward, Y = along the rim.
		for (int32 s = -1; s <= 1; s += 2)
		{
			P->AddBox(FVector(0, s * HW, 16.f), FVector(64.f, 64.f, 32.f), FRotator::ZeroRotator, StoneDark);
			P->AddBox(FVector(0, s * HW, 32.f + (ArchH - 120.f) * 0.5f), FVector(46.f, 46.f, ArchH - 120.f), FRotator::ZeroRotator, Stone);
			P->AddBox(FVector(0, s * HW, ArchH - 116.f), FVector(54.f, 54.f, 10.f), FRotator::ZeroRotator, Snow, false);
		}
		// Semicircular top from 7 voussoirs.
		const float Rr = HW, Zs = ArchH - 120.f;
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
		for (int32 s = -1; s <= 1; s += 2)
			P->AddColliderCyl(FVector2D(0.f, s * HW), ArchPostR, 0.f, ArchH - 110.f, TEXT("ZArch"), StonePM);
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
			if (bR2) { P->SetActorHiddenInGame(true); P->SetActorEnableCollision(false); P->SetActorLocation(P->GetActorLocation() - FVector(0, 0, 5000.f)); ++n; continue; } // r1650-1720 = the crevasse
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
		UProceduralMeshComponent* CrM = AddFrozenProc(Cr, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#69E1F9")), 3.0f), Spin, false);
		CrM->SetRelativeLocation(FVector(0, 0, 215.f));
		CrM->SetRelativeScale3D(FVector(2.0f)); // polish r1: the art's big central crystal; r6: larger + brighter so it reads from the aerial views
		// Fountain: a faint glowing light column from the basin up to the crystal (visual only).
		{
			FZoogiMeshBuilder Col;
			Col.Prism(FVector2D::ZeroVector, 12, 46.f, 22.f, 2.f, 215.f, 0.f, 0.f, false);
			if (UProceduralMeshComponent* Cc = AddFrozenProc(Col, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#7FEAFF")), 0.5f), Pv, false)) Cc->ComponentTags.Add(TEXT("NoCamBlock"));
		}
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
	for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(IsletYaw); ++k) // symmetric layout: 6 identical islets
	{
		const float Yaw = IsletYaw[k];
		const FVector C = Pol(IsletD(k), Yaw, IsletTopZ);
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
			FZoogiMeshBuilder Top, Ic;
			Top.Prism(FVector2D::ZeroVector, 18, IsletR, IsletR - 12.f, -80.f, 0.f, Yaw, 10.f, false);
			// Polish r7: jagged snowy rock underside (frozen_zones_aerial) instead of the smooth 11-sided cone: an irregular ridged
			// inverted cone (snow on the rim band + up-facing facets, pale stone ridges, blue ice valleys) with hanging rock spurs.
			// Visual only (the walkable islet floor is the invisible cylinder below); own random stream so nothing else moves.
			FZoogiMeshBuilder USnow, URock, UIce;
			{
				FRandomStream URng(7100 + k);
				const int32 UNS = 18, UNL = 6;
				const float UDepth = IsletR * 1.35f;
				TArray<FVector> UPt; UPt.SetNum(UNS * UNL);
				for (int32 l = 0; l < UNL; ++l)
				{
					const float UT = (float)l / UNL;
					for (int32 j = 0; j < UNS; ++j)
					{
						const bool bURidge = (j % 3) == 0;
						const float UA = 2.f * PI * (j + URng.FRandRange(-0.25f, 0.25f)) / UNS + FMath::DegreesToRadians(Yaw);
						const float URr = l == 0 ? (IsletR - 24.f) * URng.FRandRange(0.97f, 1.02f)
							: (IsletR - 24.f) * FMath::Pow(1.f - UT, 0.8f) * (bURidge ? URng.FRandRange(1.0f, 1.08f) : URng.FRandRange(0.76f, 0.9f));
						const float UZ = l == 0 ? -80.f - URng.FRandRange(0.f, 20.f) : -80.f - UDepth * UT * URng.FRandRange(0.9f, 1.1f);
						UPt[l * UNS + j] = FVector(FMath::Cos(UA) * URr, FMath::Sin(UA) * URr, UZ);
					}
				}
				const FVector UApex(URng.FRandRange(-60.f, 60.f), URng.FRandRange(-60.f, 60.f), -80.f - UDepth * URng.FRandRange(1.05f, 1.2f));
				auto UFace = [&](const FVector& P0, const FVector& P1, const FVector& P2, int32 l, int32 j)
				{
					const FVector UCen = (P0 + P1 + P2) / 3.f;
					const FVector UOut = FVector(UCen.X, UCen.Y, 0.f).GetSafeNormal() + FVector(0.f, 0.f, -0.25f);
					FVector UNn = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
					if (FVector::DotProduct(UNn, UOut) < 0.f) UNn = -UNn;
					if (l <= 1 || UNn.Z > 0.15f) USnow.FlatTri(P0, P1, P2, UOut);
					else if ((j % 3) == 0 || ((j % 3) == 1 && (l % 2) == 1)) URock.FlatTri(P0, P1, P2, UOut);
					else UIce.FlatTri(P0, P1, P2, UOut);
				};
				for (int32 l = 0; l < UNL; ++l)
					for (int32 j = 0; j < UNS; ++j)
					{
						const int32 UJ1 = (j + 1) % UNS;
						const FVector UA0 = UPt[l * UNS + j], UA1 = UPt[l * UNS + UJ1];
						if (l + 1 < UNL)
						{
							const FVector UB0 = UPt[(l + 1) * UNS + j], UB1 = UPt[(l + 1) * UNS + UJ1];
							UFace(UA0, UB0, UB1, l, j);
							UFace(UA0, UB1, UA1, l, j);
						}
						else UFace(UA0, UApex, UA1, l, j);
					}
				for (int32 sp = 0; sp < 5; ++sp) // hanging rock spurs: broken silhouette like the art
				{
					const float UA = Yaw + sp * 72.f + URng.FRandRange(-20.f, 20.f);
					const FVector USp = Pol((IsletR - 30.f) * URng.FRandRange(0.35f, 0.6f), UA, -80.f - UDepth * URng.FRandRange(0.45f, 0.7f));
					URock.Chunk(USp, FVector(URng.FRandRange(90.f, 150.f), URng.FRandRange(90.f, 150.f), URng.FRandRange(160.f, 260.f)), URng, 0.35f, UA);
				}
			}
			for (int32 i = 0; i < 22; ++i)
			{
				const float UA = i * (360.f / 22) + Rng.FRandRange(-5.f, 5.f);
				const float L = Rng.FRandRange(70.f, 260.f);
				Ic.Prism(FVector2D(Pol(IsletR - 30.f, UA)), 5, Rng.FRandRange(16.f, 30.f), 0.f, -82.f, -82.f - L, UA, 0.f, false);
			}
			Stretch(Top); Stretch(USnow); Stretch(URock); Stretch(UIce); Stretch(Ic);
			AddFrozenProc(Top, Snow, Pv, true);
			AddFrozenProc(USnow, Snow, Pv, true);
			AddFrozenProc(URock, Glacier, Pv, true); // r7b: pale snowy rock (Stone read dark slate in shadow)
			AddFrozenProc(UIce, Cliff, Pv, true);
			AddFrozenProc(Ic, IcePane, Pv, false);
			// One small snow pine beside the bleachers (outside the side wall, never in front of the seats).
			for (int32 sd = -1; sd <= 1; sd += 2) // symmetric layout: mirrored pine pair on every islet
				if (UStaticMeshComponent* Tr = AddTree(SnowPine, ToWorld(60.f, sd * (StandHalfW + 75.f), 0.f), Rng.FRandRange(300.f, 380.f), Rng.FRandRange(0.f, 360.f)))
					Tr->AttachToComponent(Pv, FAttachmentTransformRules::KeepWorldTransform);
			// Polish r1: snowy pine pair on the islet flanks, outside the bleacher side walls (the art's frosted pines).
			for (int32 t = 0; t < 2; ++t)
				if (UStaticMeshComponent* Tr = AddTree(SnowPine, ToWorld(-IsletR * 0.35f + PRng.FRandRange(-20.f, 20.f), (t ? 1.f : -1.f) * Rb * 0.78f, 0.f), PRng.FRandRange(320.f, 440.f), PRng.FRandRange(0.f, 360.f)))
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
		// Arena Orb spot on the front apron, in front of the bleachers. Ring 2: the apron is out-zone (r > 2300), so the spot
		// moves onto Ring 2 at the bridge crossing (r1860, between span A and the deflector wings).
		if (bR2) ExtraOrbSpots.Add(Pol(1860.f, Yaw, R2Z + ZoogiConst::OrbRadius + 1.f));
		else ExtraOrbSpots.Add(ToWorld(StandX0 + (IsletR - StandX0) * 0.5f, 0.f, ZoogiConst::OrbRadius + 1.f));

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
			if (sec != 1) // symmetric layout: a lamp on both outer sections of every stand
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
		// Ring 2 (plan): span A r1535-1750 (z 0 -> -30) over the crevasse, the Ring 2 crossing is the Ring 2 floor (no rails),
		// span B r2060-2285 (z -30 -> -60) to the islet apron; deck half width 110.
		for (int32 Sp = 0; Sp < (bR2 ? 2 : 1); ++Sp)
		{
			const float RA = (bR2 && Sp == 1) ? R2Out : ZoogiConst::KnockoffRadius - 15.f;
			const float RB = (bR2 && Sp == 0) ? R2In : IsletD(k) - IsletR + 25.f;
			const float ZA = (bR2 && Sp == 1) ? R2Z : 0.f, ZB = (bR2 && Sp == 0) ? R2Z : IsletTopZ;
			const float BHW = bR2 ? R2BridgeHalfW : BridgeHalfW;
			const FVector A = Pol(RA, Yaw, ZA), B = Pol(RB, Yaw, ZB);
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
					Planks.Box(Pt, FVector(15.f, BHW, 3.f), Rot);
				}
				Under.Box(-Nn * 14.f, FVector(L * 0.5f, BHW * 0.7f, 8.f), Rot);
				for (int32 s = -1; s <= 1; s += 2)
				{
					Rails.Box(FVector(0.f, s * (BHW + 6.f), BridgeRailH * 0.5f) + FVector(0, 0, 0), FVector(L * 0.5f, 6.f, BridgeRailH * 0.5f), Rot);
					Rails.Box(FVector(0.f, s * (BHW + 6.f), BridgeRailH + 26.f), FVector(L * 0.5f, 3.f, 3.f), Rot);
					const int32 Npost = FMath::Max(2, FMath::RoundToInt(L / 160.f));
					for (int32 j = 0; j <= Npost; ++j)
					{
						const float X = -L * 0.5f + L * j / Npost;
						Rails.Box(FVector(X, s * (BHW + 6.f), Dz * (X / L) + 28.f), FVector(6.f, 6.f, 28.f));
					}
				}
				P->AddProc(Planks, StandWood);
				P->AddProc(Rails, StandRiser);
				P->AddProc(Under, StandRiser);
				// Deck collider (top runs A -> B, 24 thick) and the two low rail colliders.
				P->AddColliderBox(-Nn * 12.f, FVector(L + 30.f, BHW * 2.f, 24.f), Rot, TEXT("ZBridge"), StonePM);
				for (int32 s = -1; s <= 1; s += 2)
					P->AddColliderBox(FVector(0.f, s * (BHW + 6.f), BridgeRailH * 0.5f), FVector(L + 30.f, 12.f, BridgeRailH), Rot, TEXT("ZRail"), WallPM);
				P->RubbleKeepZ = 200.f;
				P->FinishBuild();
				if (Forming) Forming->AddActor(P, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.3f + 0.1f * k, 0.8f);
			}
		}
		{
			const int32 Before = Built.Num();
			// Symmetric layout: on the side away from the camera axis, half way to the next islet (mirrored east / west).
			const float FallA = Yaw + (FMath::Cos(FMath::DegreesToRadians(Yaw)) > 0.f ? -22.5f : 22.5f);
			AddWaterfall(Pol(IsletD(k) - 60.f, FallA, IsletTopZ - 10.f), FallA, 120.f, 2600.f, 50.f); // r6: narrower
			for (int32 b = Before; b < Built.Num(); ++b) if (USceneComponent* Wc = Cast<USceneComponent>(Built[b])) Wc->AttachToComponent(Pv, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// ---- NW snowy outcrop (terraced) with frosted trees ----
	{
		AZoogiArenaPiece* P = SpawnPiece(Pol(bR2 ? R2OutcropDist : OutcropDist, OutcropYaw, -20.f), OutcropYaw, TEXT("outcrop"), TEXT("Stone"), 400.f);
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
				if (UStaticMeshComponent* Tr = AddTree(SnowPine, Pol(bR2 ? R2OutcropDist : OutcropDist, OutcropYaw, 0.f) + FVector(Rng.FRandRange(-90.f, 90.f), Rng.FRandRange(-90.f, 90.f), 190.f), Rng.FRandRange(380.f, 520.f), Rng.FRandRange(0.f, 360.f)))
					if (Forming) Forming->AddComponent(Tr, EZoogiFormStyle::Rise, FVector(0, 0, -800.f), 0.0f, 1.4f);
		}
	}
	// Round 3: stronger falls (wider, much longer, two more) with mist clouds where they drop into the cloud sea.
	{
		// Polish r1: 6 falls (front-centre + back-right added), longer drop into the cloud sea.
		const float Falls[6] = { bR2 ? R2FallYaw[0] : FallYaw[0], bR2 ? R2FallYaw[1] : FallYaw[1], 90.f, 270.f, 258.75f, 281.25f }; // symmetric layout: front trio + back trio, never under a bridge
		const float FallR = bR2 ? 1565.f : 1775.f; // Ring 2: they pour from the Ring 1 rim into the crevasse
		const float Wid[6] = { 130.f, 130.f, 170.f, 120.f, 100.f, 100.f }; // mirrored pairs; r6: narrower ribbons like the art
		FZoogiMeshBuilder FallCore;
		UTexture* Cloud = FTex(TEXT("T_FrozenCloud"));
		for (int32 f = 0; f < 6; ++f)
		{
			AddWaterfall(Pol(FallR, Falls[f], -16.f), Falls[f], Wid[f], 3800.f, 55.f);
			{ // r6: bright white-cyan core in front of the sheet (additive, visual only)
				const FVector Sd = Pol(1.f, Falls[f] + 90.f) * (Wid[f] * 0.22f);
				const FVector T0 = Pol(FallR + 55.f + 10.f, Falls[f], -16.f - 33.f), T1 = Pol(FallR + 55.f + 46.f, Falls[f], -16.f - 33.f - 3800.f);
				FallCore.Quad(T0 - Sd, T0 + Sd, T1 + Sd * 1.25f, T1 - Sd * 1.25f, Pol(1.f, Falls[f]));
			}
			for (int32 m = 0; m < 3; ++m)
				FlatQuad(Cloud, Pol(1900.f + m * 120.f, Falls[f] + Rng.FRandRange(-4.f, 4.f), -1700.f - m * 250.f), Rng.FRandRange(0.f, 360.f), 900.f + m * 300.f, 0.9f, 0.25f, 1.1f, FLinearColor::White, nullptr);
		}
		if (UProceduralMeshComponent* Fc = AddFrozenProc(FallCore, ZoogiUtil::FxMID(this, ZoogiUtil::Hex(TEXT("#E8FBFF")), 0.32f), nullptr, false)) Fc->ComponentTags.Add(TEXT("NoCamBlock"));
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
