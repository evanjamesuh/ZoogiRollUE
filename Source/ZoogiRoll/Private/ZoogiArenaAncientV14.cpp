// Nov 1 (Evan: "v14 is done ... Import"): Grok's Blender diorama (C:\dev\art\ancient_meadows\grok_design\blender\arena\pieces_v14,
// imported by Scripts/arenas/ancient/import_v14.py into /Game/Env/AncientV14) IS the Ancient Meadows playfield.
// - The model is placed at y -16 m so its plaza centre sits on the arena origin (spawns / camera / orb ring are hub-centric).
// - Collision = the meshes themselves (complex as simple, set at import) for every walk / bump surface: base, plaza, plaza props,
//   gate, volcano + ramp, islands, ramps, viaducts, islets. No collision: water, nature, gate portal swirl (arena_report_v14.txt).
// - The rules layout (off-court, regions, orb floors) is ZoogiAncient::MakeV14 (same surfaces). -ZoogiAncientV2 = the old arena.
#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiAncientStartTest(AZoogiArena* Arena);

namespace ZoogiV14
{
	struct FPiece { const TCHAR* Name; bool bCollide; bool bShadow; };
	static const FPiece Pieces[] = {
		{ TEXT("SM_Arena_Base"), true, true }, { TEXT("SM_Arena_Water"), false, false }, { TEXT("SM_Arena_Islets"), true, true },
		{ TEXT("SM_Arena_Plaza"), true, true }, { TEXT("SM_Arena_PlazaProps"), true, true }, { TEXT("SM_Arena_Gate"), true, true },
		{ TEXT("SM_Arena_GatePortal"), false, false }, { TEXT("SM_Arena_Volcano"), true, true }, { TEXT("SM_Arena_VolcanoRamp"), true, true },
		{ TEXT("SM_Arena_Island_Ruins"), true, true }, { TEXT("SM_Arena_Island_Citadel"), true, true }, { TEXT("SM_Arena_Island_Emerald"), true, true },
		{ TEXT("SM_Arena_Island_Sunstone"), true, true }, { TEXT("SM_Arena_Ramps"), true, true }, { TEXT("SM_Arena_Viaducts"), true, true },
		{ TEXT("SM_Arena_Nature"), false, true },
	};
	constexpr float ShiftY = -1600.f;
}

void AZoogiArena::BuildAncientV14()
{
	using namespace ZoogiAncient;
	using ZoogiUtil::Hex;
	const FLayout& L = Get();
	bAncientActive = true;
	AncMovers.Reset(); AncMoverPath.Reset(); AncClock = 0.f;
	BuildScale = 1.f; SetActorScale3D(FVector(1.f)); CamScale = 1.6f;
	KnockoffRadiusOverride = L.HubR; ActiveKnockoffR = L.HubR;   // the plaza is the "court" for the round-court code paths
	SpawnRadiusOverride = ZoogiAncient::V15() ? 1900.f : 950.f;   // Grok's plaza spawns (r9.5 m; b162 v15 r19 m)
	OrbCountOverride = 14; OrbRingRadiusOverride = 1150.f; OrbRingYawOffset = 13.f; // between the drum ring (r7.2 m) and the bollards (r14.6 m)
	if (ZoogiAncient::V15()) { OrbCountOverride = 20; OrbRingRadiusOverride = 2100.f; OrbRingYawOffset = 9.f; } // b162: the 2x plaza (bollards r29 m)
	FloorPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.1f, true);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);
	UPhysicalMaterial* PropPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.75f, false);

	int32 N = 0, Missing = 0;
	for (const ZoogiV14::FPiece& P : ZoogiV14::Pieces)
	{
		UStaticMesh* M = nullptr;
		if (FCString::Strcmp(P.Name, TEXT("SM_Arena_PlazaProps")) == 0 && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientDrums"))) // b161: no centre drums
			M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/AncientV14/SM_Arena_PlazaPropsTrick.SM_Arena_PlazaPropsTrick"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/AncientV14/%s.%s"), P.Name, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) { ++Missing; UE_LOG(LogZoogi, Warning, TEXT("ANCIENT v14: %s missing"), P.Name); continue; }
		if (ZoogiAncient::V15())
		{	// b162: the v15 pieces (2x plaza ring); falls back to v14 if an asset is missing
			if (UStaticMesh* M15 = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/AncientV15/%s.%s"), P.Name, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet)) M = M15;
			else UE_LOG(LogZoogi, Warning, TEXT("ANCIENT v15: %s missing - v14 piece used"), P.Name);
		}
		UStaticMeshComponent* C = AddMesh(M, nullptr, P.bCollide, P.bShadow);
		C->SetRelativeLocation(FVector(0.f, ZoogiV14::ShiftY, 0.f));
		if (P.bCollide)
		{
			const bool bProp = FCString::Strstr(P.Name, TEXT("Props")) || FCString::Strstr(P.Name, TEXT("Gate")) || (FCString::Strstr(P.Name, TEXT("Volcano")) && !FCString::Strstr(P.Name, TEXT("Ramp")));
			C->SetPhysMaterialOverride(bProp ? PropPM : FloorPM.Get());
		}
		++N;
	}
	// island orbs (the plaza ring comes from the game mode); only where the rules floor agrees
	for (const FVector& S : L.OrbSpots)
		if (FMath::Abs(FloorZ(FVector2D(S)) - (S.Z - 41.f)) <= 5.f) ExtraOrbSpots.Add(S);

	// sky, sun, fog: the v14 renders' golden afternoon (palette.md: sun 18 deg #FFC27A, skylight #B9A6FF)
	if (!AddPanoramaSky(TEXT("AncientDusk"), 1.0f, 0.f, FLinearColor::White)) AddPanoramaSky(TEXT("Ancient"), 1.0f, 0.f, FLinearColor::White);
	if (Sun) { Sun->SetRelativeRotation(FRotator(-30.f, 205.f, 0.f)); Sun->SetLightColor(Hex(TEXT("#FFD2A0"))); Sun->SetIntensity(11.f); }
	if (SkyLight) { SkyLight->SetIntensity(1.7f); SkyLight->SetLightColor(Hex(TEXT("#E4DCFF"))); }
	if (Fog) { Fog->SetFogInscatteringColor(Hex(TEXT("#D9CCF2"))); Fog->SetFogDensity(0.0012f); Fog->SetStartDistance(15000.f); }
	if (Post)
	{
		FPostProcessSettings& PS = Post->Settings;
		PS.bOverride_AutoExposureBias = true; PS.AutoExposureBias += 0.3f;
		PS.bOverride_BloomIntensity = true; PS.BloomIntensity = 1.4f;      // Grok: "bloomier lava"
		PS.bOverride_ColorSaturation = true; PS.ColorSaturation = FVector4(1.05f, 1.05f, 1.03f, 1.06f);
		PS.bOverride_VignetteIntensity = true; PS.VignetteIntensity = 0.25f;
	}
	UE_LOG(LogZoogi, Log, TEXT("ANCIENT v14: %d/%d pieces (%d missing) at y %.0f, %d island orbs, rules: %d floors %d ramps"),
		N, (int32)UE_ARRAY_COUNT(ZoogiV14::Pieces), Missing, ZoogiV14::ShiftY, ExtraOrbSpots.Num(), L.Plats.Num(), L.Ramps.Num());
	ZoogiAncientStartTest(this);
}
