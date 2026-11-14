// b163 (Evan Nov 6 13:34 "Go with battle league court", 13:35 "Use the ring from ancient meadows but just the ring, everything else you
// build for the league court"): the Training arena (Episode 1 practice spar, solo training, skill lab) becomes the ZOOGI LEAGUE TRAINING
// COURT - the Ancient Meadows plaza ring (r20.15 m walkable) on a stone plinth in a dawn meadow, with bleachers and a few early fans, a
// PRACTICE scoreboard, light towers, league banners, pink target pads + cones, a coach's bench and Meadow Hollow on the horizon.
// Art: Blender build_court.py -> /Game/Env/LeagueCourt (Scripts/arenas/ancient/import_court.py). -ZoogiOldTraining = the old court.
#include "ZoogiArena.h"
#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool AZoogiArena::BuildLeagueCourt()
{
	struct FP { const TCHAR* Name; bool bCollide; bool bShadow; int32 Pm; };   // Pm 0 floor, 1 bumper, 2 prop
	static const FP CourtPieces[] = {
		{ TEXT("SM_Arena_Court_Ring"), true, true, 0 }, { TEXT("SM_Arena_Court_Curb"), true, true, 1 }, { TEXT("SM_Arena_Court_Ground"), false, true, 0 },
		{ TEXT("SM_Arena_Court_Stands"), true, true, 2 }, { TEXT("SM_Arena_Court_Scoreboard"), true, true, 2 }, { TEXT("SM_Arena_Court_Lights"), false, true, 2 },
		{ TEXT("SM_Arena_Court_Banners"), true, true, 2 }, { TEXT("SM_Arena_Court_Pads"), false, false, 0 }, { TEXT("SM_Arena_Court_Scenery"), false, true, 2 },
	};
	if (!LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/LeagueCourt/SM_Arena_Court_Ring.SM_Arena_Court_Ring"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return false;
	bLeagueCourt = true;
	BuildScale = 1.f; SetActorScale3D(FVector(1.f)); CamScale = 1.15f;
	KnockoffRadiusOverride = 2015.f; ActiveKnockoffR = KnockoffRadiusOverride;      // the ring's walkable radius (the old Training court too)
	SpawnRadiusOverride = ZoogiConst::SpawnRadius * 1.3f; OrbCountOverride = 12;
	OrbRingRadiusOverride = 780.f; OrbRingYawOffset = 15.f;                           // inside the four target pads (r11.5 m)
	FloorPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.1f, true);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);
	UPhysicalMaterial* BumpPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.75f, false);
	UPhysicalMaterial* PropPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.4f, false);
	int32 N = 0;
	for (const FP& P : CourtPieces)
	{
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/LeagueCourt/%s.%s"), P.Name, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) { UE_LOG(LogZoogi, Warning, TEXT("LEAGUE COURT: %s missing"), P.Name); continue; }
		UStaticMeshComponent* C = AddMesh(M, nullptr, P.bCollide, P.bShadow);
		if (P.bCollide) C->SetPhysMaterialOverride(P.Pm == 1 ? BumpPM : P.Pm == 2 ? PropPM : FloorPM.Get());
		if (!P.bCollide) C->ComponentTags.Add(TEXT("NoCamBlock"));
		++N;
	}
	// the PRACTICE scoreboard face (Blender: screen centre (0, -37.69 m, 6.4 m), facing the court)
	{
		auto Line = [&](const TCHAR* Txt, float Z, float Size, const FColor& Col)
		{
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
			T->SetupAttachment(GetRootComponent()); T->RegisterComponent();
			T->SetRelativeLocation(FVector(0.f, -3769.f + 14.f, Z)); T->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
			T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
			T->SetWorldSize(Size); T->SetTextRenderColor(Col); T->SetText(FText::FromString(Txt));
			T->SetCollisionEnabled(ECollisionEnabled::NoCollision); T->SetCastShadow(false);
			Built.Add(T);
		};
		Line(TEXT("ZOOGI LEAGUE"), 760.f, 95.f, FColor(242, 193, 78));
		Line(TEXT("PRACTICE"), 600.f, 150.f, FColor(229, 23, 126));
		Line(TEXT("TRAINING COURT"), 470.f, 70.f, FColor(244, 238, 248));
	}
	// dawn over Meadow Hollow (EP1: "One practice roll before we go")
	if (!AddPanoramaSky(TEXT("AncientDusk"), 0.95f, 0.f, FLinearColor::White)) AddPanoramaSky(TEXT("Training"), 0.6f, 0.45f, FLinearColor(0.36f, 0.48f, 0.64f));
	if (Sun) { Sun->SetRelativeRotation(FRotator(-24.f, 150.f, 0.f)); Sun->SetLightColor(ZoogiUtil::Hex(TEXT("#FFC890"))); Sun->SetIntensity(9.f); }
	if (SkyLight) { SkyLight->SetIntensity(1.6f); SkyLight->SetLightColor(ZoogiUtil::Hex(TEXT("#F0DCFF"))); }
	if (Fog) { Fog->SetFogInscatteringColor(ZoogiUtil::Hex(TEXT("#F6C8B8"))); Fog->SetFogDensity(0.0015f); Fog->SetStartDistance(9000.f); }
	if (Post)
	{
		FPostProcessSettings& PS = Post->Settings;
		PS.bOverride_BloomIntensity = true; PS.BloomIntensity = 1.1f;
		PS.bOverride_ColorSaturation = true; PS.ColorSaturation = FVector4(1.05f, 1.04f, 1.02f, 1.05f);
		PS.bOverride_VignetteIntensity = true; PS.VignetteIntensity = 0.22f;
	}
	UE_LOG(LogZoogi, Log, TEXT("LEAGUE COURT: %d/%d pieces, ring r%.0f, scoreboard PRACTICE"), N, (int32)UE_ARRAY_COUNT(CourtPieces), KnockoffRadiusOverride);
	return N > 0;
}
