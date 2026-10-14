// See ZoogiSpawnElevator.h.
#include "ZoogiSpawnElevator.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiGameMode.h"
#include "ZoogiFX.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace
{
	const TCHAR* MapDir(EZoogiArena A)
	{
		switch (A)
		{
		case EZoogiArena::Frozen: return TEXT("Frozen");
		case EZoogiArena::Volcanic: return TEXT("Volcanic");
		case EZoogiArena::Cosmic: return TEXT("Cosmic");
		case EZoogiArena::Arabian: return TEXT("Arabian");
		case EZoogiArena::Tomb: return TEXT("Tomb");
		case EZoogiArena::Neon: return TEXT("Neon");
		default: return TEXT("Meadow"); // Meadow + Training (Meadow layout)
		}
	}
	// accent colours from each pack's PLACEMENT.json (M_Spawn_Accent, linear), used for the arrival light
	FLinearColor MapAccent(const FString& M)
	{
		if (M == TEXT("Meadow")) return FLinearColor(0.0409f, 0.7682f, 0.1946f);
		if (M == TEXT("Volcanic")) return FLinearColor(1.f, 0.1022f, 0.007f);
		return FLinearColor(0.55f, 0.85f, 1.f);
	}
	UStaticMesh* FindMesh(const FString& Map, const TCHAR* PartDir)
	{
		static TMap<FString, TWeakObjectPtr<UStaticMesh>> Cache;
		const FString Key = Map / PartDir;
		if (UStaticMesh* M = Cache.FindRef(Key).Get()) return M;
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> As;
		AR.GetAssetsByPath(FName(*(FString(TEXT("/Game/Env/SpawnPlatform/")) / Key)), As, true);
		// Volcanic's telescoping lift imports as 3 nested stage meshes: the top stage (_03) is the one that reaches the floor
		As.Sort([](const FAssetData& X, const FAssetData& Y) { return X.AssetName.ToString().Contains(TEXT("Stage_03")) > Y.AssetName.ToString().Contains(TEXT("Stage_03")); });
		for (const FAssetData& A : As)
			if (A.AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName() && !A.AssetName.ToString().StartsWith(TEXT("UCX")))
				if (UStaticMesh* M = Cast<UStaticMesh>(A.GetAsset())) { Cache.Add(Key, M); return M; }
		return nullptr;
	}
	float Ease(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T) * (1.f - T); }
}

AZoogiSpawnElevator::AZoogiSpawnElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	Pad = CreateDefaultSubobject<USceneComponent>(TEXT("Pad"));
	RootComponent = Pad;
}

bool AZoogiSpawnElevator::IsEnabled()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ZoogiNoSpawnElevator"))) return false;
	if (FParse::Param(Cmd, TEXT("ZoogiSpawnElevator"))) return true;
	bool b = true;
	if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.SpawnElevator"), TEXT("bEnabled"), b, GGameIni);
	FString V;
	// auto / test / parity runs never see it (gameplay timing and hashes untouched)
	if (FParse::Param(Cmd, TEXT("ZoogiAuto")) || FParse::Value(Cmd, TEXT("ZoogiBattleTest="), V) || FParse::Param(Cmd, TEXT("ZoogiNoForm")) || GIsAutomationTesting) return false;
	return b;
}

void AZoogiSpawnElevator::PlayAll(AZoogiGameMode* GM)
{
	if (!GM || !GM->Arena || !IsEnabled() || GM->GetNetMode() == NM_DedicatedServer) return;
	const FString Map = MapDir(GM->Arena->GetTheme());
	int32 N = 0;
	for (AZoogiBall* B : GM->Balls)
	{
		if (!B || B->bOut) continue;
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector L = B->BallLocation();
		const FVector Floor(L.X, L.Y, L.Z - ZoogiConst::ZoogiRadius);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-L.Y, -L.X)); // local +X faces the arena centre
		if (AZoogiSpawnElevator* E = GM->GetWorld()->SpawnActor<AZoogiSpawnElevator>(AZoogiSpawnElevator::StaticClass(), FTransform(FRotator(0, Yaw, 0), Floor), P))
		{
			E->Init(B, Map, 0.12f * N++);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("SpawnElevator: %d pads (%s pack)"), N, *Map);
}

UStaticMeshComponent* AZoogiSpawnElevator::Part(UStaticMesh* M)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Pad);
	C->SetStaticMesh(M);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false); // cheap: the sun shadow of a 2 cm pad adds nothing
	C->SetGenerateOverlapEvents(false);
	C->RegisterComponent();
	return C;
}

void AZoogiSpawnElevator::Init(AZoogiBall* InBall, const FString& Map, float InDelay)
{
	Ball = InBall; Delay = InDelay; Accent = MapAccent(Map);
	LiftRest = Map == TEXT("Volcanic") ? -90.f : -122.f; // Volcanic: short pit, telescoping lift (collapsed top at -90)
	UStaticMesh* MB = FindMesh(Map, TEXT("SM_SpawnPlatform_Base"));
	UStaticMesh* MP = FindMesh(Map, TEXT("SM_SpawnPlatform_Petal"));
	UStaticMesh* ML = FindMesh(Map, TEXT("SM_SpawnPlatform_Lift"));
	if (!MB || !MP || !ML) { UE_LOG(LogZoogi, Warning, TEXT("SpawnElevator: %s meshes missing (base %d petal %d lift %d)"), *Map, !!MB, !!MP, !!ML); Destroy(); return; }
	Base = Part(MB);
	for (int32 i = 0; i < 6; ++i) Petals.Add(Part(MP));
	Lift = Part(ML);
	// fake pit mouth (no real floor hole): a near-black disc just above the floor, visible while the core is open
	if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		Mouth = Part(Cyl);
		Mouth->SetRelativeScale3D(FVector(1.40f, 1.40f, 0.004f));
		Mouth->SetRelativeLocation(FVector(0, 0, 0.0f));
		if (UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(BM, this);
			D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.004f, 0.006f, 0.01f));
			Mouth->SetMaterial(0, D);
		}
		Mouth->SetVisibility(false);
	}
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(Pad);
	Light->SetRelativeLocation(FVector(0, 0, 60.f));
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(420.f);
	Light->SetLightColor(Accent);
	Light->SetIntensity(0.f);
	Light->RegisterComponent();
	Tick(0.f);
}

void AZoogiSpawnElevator::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Base) return;
	Age += Dt;
	const float T = Age - Delay;
	AZoogiBall* B = Ball.Get();
	// timeline (s): pad surfaces -0.15..0 | petals open 0..0.35 | lift rises 0.15..0.95 (+6 overshoot), settles 0.95..1.1
	// | arrival burst 0.95 | Zoogi handed back 1.15 | petals close + pad sinks under the floor 1.6..2.1 | destroy 2.2
	const float Surface = T < 0.f ? Ease((T + 0.15f) / 0.15f) : (T < 1.6f ? 1.f : 1.f - Ease((T - 1.8f) / 0.3f));
	const float PadZ = -8.f * (1.f - Surface) + 0.4f;   // 0.4 cm above the floor: no z-fight with the arena floor
	const float Open = T < 1.6f ? EaseOut(T / 0.35f) : 1.f - Ease((T - 1.6f) / 0.25f);
	float LiftZ = LiftRest;
	if (T > 0.15f && T < 1.6f)
	{
		const float R = (T - 0.15f) / 0.8f;
		LiftZ = R < 1.f ? FMath::Lerp(LiftRest, 6.f, EaseOut(R)) : FMath::Lerp(6.f, 0.f, Ease((T - 0.95f) / 0.15f));
	}
	else if (T >= 1.6f) LiftZ = FMath::Lerp(0.f, LiftRest, Ease((T - 1.6f) / 0.25f));
	Base->SetRelativeLocation(FVector(0, 0, PadZ));
	for (int32 i = 0; i < Petals.Num(); ++i)
	{
		const float Yaw = 30.f + 60.f * i;
		Petals[i]->SetRelativeLocationAndRotation(FRotator(0, Yaw, 0).RotateVector(FVector(62.35f, 0, 0)) + FVector(0, 0, PadZ - 6.f), FRotator(95.f * Open, Yaw, 0));
	}
	Lift->SetRelativeLocation(FVector(0, 0, LiftZ + PadZ));
	if (Mouth) Mouth->SetVisibility(Open > 0.05f && LiftZ < -1.5f);

	// the Zoogi: hide the real one, ride a visual copy of its meshes on the lift (centre z = lift top + radius)
	if (B && !bHid && T > -0.2f)
	{
		bHid = true;
		B->SetActorHiddenInGame(true);
		TArray<UStaticMeshComponent*> Src; B->GetComponents<UStaticMeshComponent>(Src);
		for (UStaticMeshComponent* S : Src)
		{
			if (!S || !S->GetStaticMesh() || !S->IsVisible() || S == B->Ring) continue;
			UStaticMeshComponent* D = NewObject<UStaticMeshComponent>(this);
			D->SetStaticMesh(S->GetStaticMesh());
			for (int32 m = 0; m < S->GetNumMaterials(); ++m) D->SetMaterial(m, S->GetMaterial(m));
			D->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			D->SetUsingAbsoluteLocation(true); D->SetUsingAbsoluteRotation(true); D->SetUsingAbsoluteScale(true);
			D->SetupAttachment(Pad);
			D->RegisterComponent();
			Proxies.Add({ S, D }); ProxyComps.Add(D);
		}
	}
	const float Off = (LiftZ + PadZ) - 0.4f; // proxy offset vs the real (resting) Zoogi, whose centre is at floor + radius
	for (FProxy& P : Proxies)
		if (UStaticMeshComponent* S = P.Src.Get())
		{
			FTransform X = S->GetComponentTransform();
			X.AddToTranslation(FVector(0, 0, FMath::Min(0.f, Off) + FMath::Max(0.f, Off)));
			P.Dst->SetWorldTransform(X);
		}
	if (!bBurst && T >= 0.95f)
	{
		bBurst = true;
		const FVector G = GetActorLocation();
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), G, FRotator::ZeroRotator, 0.8f);
		AZoogiFX::Spawn(GetWorld(), G + FVector(0, 0, 1.f), Accent, 150.f, 0.45f, true, 6.f);
	}
	if (Light) Light->SetIntensity(T >= 0.95f ? 9000.f * FMath::Exp(-(T - 0.95f) * 6.f) : 0.f);
	if (!bShown && T >= 1.15f)
	{
		bShown = true;
		if (B) B->SetActorHiddenInGame(false);
		for (UStaticMeshComponent* D : ProxyComps) if (D) D->DestroyComponent();
		Proxies.Reset(); ProxyComps.Reset();
	}
	if (AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) // capture runs (-ZoogiShots): first pad only
		if (Delay == 0.f) { static const float ShotT[] = { 0.1f, 0.35f, 0.6f, 0.85f, 1.0f, 1.2f }; if (Shot < 6 && T >= ShotT[Shot]) GM->TakeShot(FString::Printf(TEXT("elevator_%d_t%03d"), Shot, FMath::RoundToInt(ShotT[Shot++] * 100.f))); }
	if (T > 2.2f) Destroy();
}

void AZoogiSpawnElevator::EndPlay(const EEndPlayReason::Type Reason)
{
	if (!bShown) if (AZoogiBall* B = Ball.Get()) B->SetActorHiddenInGame(false); // never leave a Zoogi invisible
	Super::EndPlay(Reason);
}