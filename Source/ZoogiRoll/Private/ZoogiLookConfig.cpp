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
	static int32 Cached = -1;
	if (Cached < 0)
	{
		bool b = false;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Awakening"), TEXT("bReactionPauses"), b, GGameIni);
		FString V;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiReactionPauses="), V)) b = V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V == TEXT("1") || V.Equals(TEXT("True"), ESearchCase::IgnoreCase);
		Cached = b ? 1 : 0;
	}
	return Cached == 1;
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
		UE_LOG(LogTemp, Log, TEXT("ZOOGI destruction=%s%s"), bOn ? TEXT("on") : TEXT("off"), bFixed ? TEXT(" (fixed: command line / test run)") : TEXT(" (default; Settings > GRAPHICS > DESTRUCTION overrides)"));
	}
	if (bFixed) return Cached == 1;
	// Oct 14 (Evan): Settings > GRAPHICS > DESTRUCTION, saved with the player's settings ([ZoogiRoll.MenuSettings] Destruction in
	// GameUserSettings.ini). Default = the ini value (OFF). Pillars stay indestructible; Crystal Collapse keeps its card exemption.
	int32 S = Cached;
	if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), TEXT("Destruction"), S, GGameUserSettingsIni);
	static int32 Logged = -1;
	if (S != Logged) { Logged = S; UE_LOG(LogTemp, Log, TEXT("ZOOGI destruction=%s (Settings)"), S != 0 ? TEXT("on") : TEXT("off")); }
	return S != 0;
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
	// The original look (AZoogiOrb::InitOrb / MakeStar as committed through Oct 7): imported orb.glb core if present,
	// else an engine sphere, both inside the glowing light-blue sphere.
	UStaticMesh* OrbMesh = LoadMesh(TEXT("/Game/Orbs/SM_Orb.SM_Orb"));
	UStaticMesh* SphereMesh = BasicShape(TEXT("Sphere"));
	const FLinearColor Body = Hex(TEXT("#87CEEB"));
	const FLinearColor Deep = Hex(TEXT("#0341CA"));
	O->Visual->SetStaticMesh(SphereMesh);
	O->Visual->SetRelativeScale3D(FVector(2.f * ZoogiConst::OrbRadius / 100.f));
	O->Visual->SetMaterial(0, GlowMID(O, Body, 3.f));
	if (OrbMesh)
	{
		O->Core->SetStaticMesh(OrbMesh);
		const FBoxSphereBounds B = OrbMesh->GetBounds();
		const float S = (1.5f * ZoogiConst::OrbRadius) / FMath::Max(B.BoxExtent.GetMax(), 1.f);
		O->Core->SetRelativeScale3D(FVector(S));
		O->Core->SetRelativeLocation(-B.Origin * S);
		for (int32 i = 0; i < O->Core->GetNumMaterials(); ++i) O->Core->SetMaterial(i, GlowMID(O, Deep, 6.f));
	}
	else
	{
		O->Core->SetStaticMesh(SphereMesh);
		O->Core->SetRelativeLocation(FVector::ZeroVector);
		O->Core->SetRelativeScale3D(FVector(1.2f * ZoogiConst::OrbRadius / 100.f));
		O->Core->SetMaterial(0, GlowMID(O, FLinearColor(0.75f, 0.95f, 1.f), 12.f));
	}
	O->Glow->SetLightColor(Body);
	if (O->bStar)
	{
		const FLinearColor Gold(1.f, 0.72f, 0.12f);
		O->Visual->SetMaterial(0, GlowMID(O, Gold, 0.9f));
		for (int32 i = 0; i < O->Core->GetNumMaterials(); ++i) O->Core->SetMaterial(i, GlowMID(O, FLinearColor(1.f, 0.45f, 0.02f), 3.f));
		O->Glow->SetLightColor(Gold);
	}
}
