// Oct 8 (Evan 9:16 PM): config-driven look / toughness switches (see ZoogiTypes.h). Visual-only except BreakToughness.
#include "ZoogiTypes.h"
#include "ZoogiOrb.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool ZoogiUtil::ShowZoogiSparkles()
{
	static int32 Cached = -1;
	if (Cached < 0)
	{
		bool bShow = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Characters"), TEXT("ZoogiSparkles"), bShow, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSparkles"))) bShow = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiSparkles"))) bShow = true;
		Cached = bShow ? 1 : 0;
	}
	return Cached == 1;
}

bool ZoogiUtil::ReactionPauses()
{
	// Oct 24 (Evan): no pauses - physics always runs naturally; [ZoogiRoll.Awakening] bReactionPauses / -ZoogiReactionPauses are ignored
	return false;
}

float ZoogiUtil::BreakToughness()
{
	static float Cached = -1.f;
	if (Cached < 0.f)
	{
		float T = 1.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.Arena"), TEXT("BreakToughness"), T, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiBreakToughness="), T);
		Cached = FMath::Clamp(T, 0.5f, 3.f);
	}
	return Cached;
}

bool ZoogiUtil::DestructionEnabled()
{
	static int32 Cached = -1;
	static bool bFixed = false; // command line override or a test / auto run: the ini / command-line value only (parity)
	if (Cached < 0)
	{
		bool bOn = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Arena"), TEXT("bDestruction"), bOn, GGameIni);
		FString V;
		const TCHAR* C = FCommandLine::Get();
		if (FParse::Value(C, TEXT("ZoogiDestruction="), V))
		{
			bOn = V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V.Equals(TEXT("1")) || V.Equals(TEXT("True"), ESearchCase::IgnoreCase);
			bFixed = true;
		}
		bFixed |= FParse::Param(C, TEXT("ZoogiAuto")) || FParse::Value(C, TEXT("ZoogiCardTest="), V) || FParse::Value(C, TEXT("ZoogiBattleTest="), V) || GIsAutomationTesting;
		Cached = bOn ? 1 : 0;
		UE_LOG(LogTemp, Log, TEXT("ZOOGI destruction=%s%s"), bOn ? TEXT("on") : TEXT("off"), bFixed ? TEXT(" (fixed: command line / test run)") : TEXT(" (ini [ZoogiRoll.Arena] bDestruction)"));
	}
	// Oct 22 (Evan): the Settings > GRAPHICS > DESTRUCTION toggle is gone; a value saved by older builds ([ZoogiRoll.MenuSettings]
	// Destruction in GameUserSettings.ini) is ignored. Only the ini / -ZoogiDestruction= decide (default ON, pillars never break).
	return Cached == 1;
}

void ZoogiUtil::ApplyBlueGlassOrb(AZoogiOrb* O)
{
	if (!O || !O->Visual || !O->Core || !O->Glow) return;
	UStaticMesh* SphereMesh = BasicShape(TEXT("Sphere"));
	UMaterialInterface* BlueShell = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Shell.MI_Blue_Shell"));
	UMaterialInterface* BlueCore = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Core.MI_Blue_Core"));
	const FLinearColor Body = Hex(TEXT("#87CEEB"));
	O->Visual->SetStaticMesh(SphereMesh);
	O->Visual->SetRelativeLocation(FVector::ZeroVector);
	O->Visual->SetRelativeScale3D(FVector(2.f * ZoogiConst::OrbRadius / 100.f));
	for (int32 i = 0; i < O->Visual->GetNumMaterials(); ++i) O->Visual->SetMaterial(i, nullptr);
	O->Visual->SetMaterial(0, BlueShell ? BlueShell : static_cast<UMaterialInterface*>(GlowMID(O, Body, 3.f)));
	O->Core->SetVisibility(true);
	O->Core->SetStaticMesh(SphereMesh);
	O->Core->SetRelativeLocation(FVector::ZeroVector);
	O->Core->SetRelativeScale3D(FVector((BlueCore ? 1.25f : 1.2f) * ZoogiConst::OrbRadius / 100.f));
	O->Core->SetMaterial(0, BlueCore ? BlueCore : static_cast<UMaterialInterface*>(GlowMID(O, FLinearColor(0.75f, 0.95f, 1.f), 12.f)));
	O->Glow->SetLightColor(BlueShell ? Hex(TEXT("#3FD8F2")) : Body);
	if (O->bStar)
	{
		// Oct 7 MakeStar: gold core + gold light (glass shell kept).
		const FLinearColor Gold(1.f, 0.72f, 0.12f);
		if (!BlueShell) O->Visual->SetMaterial(0, GlowMID(O, Gold, 0.9f));
		for (int32 i = 0; i < O->Core->GetNumMaterials(); ++i) O->Core->SetMaterial(i, GlowMID(O, FLinearColor(1.f, 0.45f, 0.02f), 3.f));
		O->Glow->SetLightColor(Gold);
		O->Glow->SetAttenuationRadius(320.f);
	}
}
void ZoogiUtil::ApplyBlueMonkeyOrb(AZoogiOrb* O)
{
	// Oct 24 (Evan, "Blue monkey face orbs"): monkey-face mesh /Game/Orbs/SM_OrbMonkey (35d5616) at OrbRadius, wearing the
	// glossy translucent Blue glass shell MI from ApplyBlueGlassOrb (1ede391/63dbd45) on every slot, Blue glowing core inside.
	if (!O || !O->Visual || !O->Core || !O->Glow) return;
	UStaticMesh* OrbMesh = LoadMesh(TEXT("/Game/Orbs/SM_OrbMonkey.SM_OrbMonkey"));
	if (!OrbMesh) { ApplyBlueGlassOrb(O); return; }
	UMaterialInterface* BlueShell = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Shell.MI_Blue_Shell"));
	UMaterialInterface* BlueCore = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/Bible/Blue/Materials/MI_Blue_Core.MI_Blue_Core"));
	const FLinearColor Body = Hex(TEXT("#87CEEB"));
	O->Visual->SetStaticMesh(OrbMesh);
	const FBoxSphereBounds B = OrbMesh->GetBounds();
	const float S = ZoogiConst::OrbRadius / FMath::Max(B.BoxExtent.GetMax(), 1.f);
	O->Visual->SetRelativeScale3D(FVector(S));
	O->Visual->SetRelativeLocation(-B.Origin * S);
	// Oct 24 (Evan 6:51 AM): the stock MI_Blue_Shell is nearly clear, so the monkey silhouette vanished and only the inner
	// core read (looked like a sphere with a core). Tint a per-orb copy deeper blue + stronger rim so the glass monkey shape reads.
	UMaterialInterface* Shell = static_cast<UMaterialInterface*>(GlowMID(O, Body, 3.f));
	if (BlueShell)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BlueShell, O);
		M->SetScalarParameterValue(TEXT("Opacity"), 0.72f);
		M->SetVectorParameterValue(TEXT("RimColor"), FLinearColor(0.25f, 0.65f, 1.f));
		M->SetScalarParameterValue(TEXT("RimStrength"), 4.f);
		M->SetScalarParameterValue(TEXT("RimPower"), 2.f);
		Shell = M;
	}
	for (int32 i = 0; i < O->Visual->GetNumMaterials(); ++i) O->Visual->SetMaterial(i, Shell);
	UStaticMesh* SphereMesh = BasicShape(TEXT("Sphere"));
	// Inner body = the same monkey mesh at 0.9x in solid deep blue, so the glass reads as a blue monkey (not a sphere + core).
	(void)SphereMesh; (void)BlueCore;
	O->Core->SetVisibility(true);
	O->Core->SetStaticMesh(OrbMesh);
	O->Core->SetRelativeScale3D(FVector(0.9f * S));
	O->Core->SetRelativeLocation(-B.Origin * 0.9f * S);
	for (int32 i = 0; i < O->Core->GetNumMaterials(); ++i) O->Core->SetMaterial(i, GlowMID(O, Hex(TEXT("#0341CA")), 2.f));
	O->Glow->SetLightColor(BlueShell ? Hex(TEXT("#3FD8F2")) : Body);
	if (O->bStar)
	{
		const FLinearColor Gold(1.f, 0.72f, 0.12f);
		for (int32 i = 0; i < O->Core->GetNumMaterials(); ++i) O->Core->SetMaterial(i, GlowMID(O, FLinearColor(1.f, 0.45f, 0.02f), 3.f));
		O->Glow->SetLightColor(Gold);
		O->Glow->SetAttenuationRadius(320.f);
	}
}
void ZoogiUtil::ApplyOrbStyle(AZoogiOrb* O)
{
	static int32 bOriginal = -1;
	if (bOriginal < 0)
	{
		FString Style = TEXT("Original");
		if (GConfig) GConfig->GetString(TEXT("ZoogiRoll.Characters"), TEXT("OrbStyle"), Style, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiOrbStyle="), Style);
		bOriginal = Style.Equals(TEXT("Original"), ESearchCase::IgnoreCase) ? 1 : 0;
	}
	if (!bOriginal || !O || !O->Visual || !O->Core || !O->Glow) return;
	// Oct 24 (Evan): OrbStyle=Original = the Oct 7 Blue-shell glass orb (restored from cfc9388).
	ApplyBlueMonkeyOrb(O); // Oct 24 (Evan): Blue monkey face orb
}

