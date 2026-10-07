#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "ZoogiKFXExtras.h"

void AZoogiFX::Niagara(UWorld* World, const TCHAR* SystemName, const FVector& Location, const FRotator& Rotation, float Scale)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara"));
	if (!World || bDisabled || World->GetNetMode() == NM_DedicatedServer) return;
	static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> Cache;
	const FName Key(SystemName);
	UNiagaraSystem* Sys = Cache.FindRef(Key).Get();
	if (!Sys)
	{
		const FString Path = FString::Printf(TEXT("/Game/FX/%s.%s"), SystemName, SystemName);
		Sys = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (!Sys) return;
		Cache.Add(Key, Sys);
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, Rotation, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
}

UNiagaraComponent* AZoogiFX::KFX(UWorld* World, const TCHAR* Prefab, const FVector& Location, float Scale, float MaxLife, AActor* Attach)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKFX"));
	if (!World || bDisabled || World->GetNetMode() == NM_DedicatedServer) return nullptr;
	static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> Cache;
	const FName Key(Prefab);
	UNiagaraSystem* Sys = Cache.FindRef(Key).Get();
	if (!Sys)
	{
		const FString P(Prefab);
		int32 U = INDEX_NONE;
		const FString Folder = P.FindChar(TEXT('_'), U) ? P.Left(U) : P;
		const FString Path = FString::Printf(TEXT("/Game/VFX/KriptoFX/%s/NS_KFX_%s.NS_KFX_%s"), *Folder, *P, *P);
		Sys = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (!Sys) return nullptr;
		Cache.Add(Key, Sys);
	}
	UNiagaraComponent* C = nullptr;
	if (Attach && Attach->GetRootComponent())
	{
		C = UNiagaraFunctionLibrary::SpawnSystemAttached(Sys, Attach->GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::None);
		if (C)
		{
			// Marbles roll: keep the effect upright and at its own scale.
			C->SetUsingAbsoluteRotation(true);
			C->SetUsingAbsoluteScale(true);
			C->SetWorldRotation(FRotator::ZeroRotator);
			C->SetWorldScale3D(FVector(Scale));
		}
	}
	else
	{
		C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::None);
	}
	if (C)
	{
		// Decals / lights / placed meshes of the prefab (ground cracks, icicles, rock spikes, tornado, portal disc).
		AZoogiKFXExtras::Spawn(World, FString(Prefab), Location, Scale, MaxLife, Attach);
	}
	if (C && MaxLife > 0.f)
	{
		// Some ported emitters loop (fire skins, fog): stop spawning after MaxLife, hard-remove a few seconds later.
		TWeakObjectPtr<UNiagaraComponent> W(C);
		FTimerHandle H1, H2;
		World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([W]() { if (W.IsValid()) W->Deactivate(); }), MaxLife, false);
		World->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([W]() { if (W.IsValid()) W->DestroyComponent(); }), MaxLife + 4.f, false);
	}
	return C;
}

const TCHAR* AZoogiFX::PowerBurst(EZoogiPower Power)
{
	switch (Power)
	{
	case EZoogiPower::WolfPack: return TEXT("NS_Burst_White");
	case EZoogiPower::Explosion: return TEXT("NS_Explosion");
	case EZoogiPower::Ricochet: return TEXT("NS_Burst_Green");
	case EZoogiPower::LockOn: return TEXT("NS_Burst_Red");
	case EZoogiPower::StaticShock: return TEXT("NS_Burst_Yellow");
	case EZoogiPower::BandageBind: return TEXT("NS_Burst_Gold");
	case EZoogiPower::ShadowStun: return TEXT("NS_Burst_Purple");
	case EZoogiPower::RuneSeal: return TEXT("NS_Burst_Gold");
	}
	return TEXT("NS_Burst_White");
}

AZoogiFX::AZoogiFX()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;
	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Mesh);
	Flash->SetUsingAbsoluteScale(true);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	Flash->SetCastShadows(false);
	Flash->SetIntensity(0.f);
}

AZoogiFX* AZoogiFX::Spawn(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, bool bRing, float Strength, AActor* FollowActor)
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiFX* FX = World->SpawnActor<AZoogiFX>(AZoogiFX::StaticClass(), FTransform(Location), P);
	if (!FX) return nullptr;
	FX->Life = FMath::Max(Duration, 0.05f);
	FX->EndRadius = Radius;
	FX->BaseStrength = Strength;
	FX->bIsRing = bRing;
	FX->Follow = FollowActor;
	FX->Mesh->SetStaticMesh(ZoogiUtil::BasicShape(bRing ? TEXT("Cylinder") : TEXT("Sphere")));
	FX->MID = ZoogiUtil::FxMID(FX, Color, Strength);
	FX->MID->SetScalarParameterValue(TEXT("Ring"), bRing ? 1.f : 0.f);
	FX->Mesh->SetMaterial(0, FX->MID);
	FX->BaseLight = FMath::Clamp(Radius * 0.25f, 20.f, 200.f);
	FX->Flash->SetLightColor(Color);
	FX->Flash->SetAttenuationRadius(Radius * 2.5f);
	FX->Flash->SetIntensity(FX->BaseLight);
	FX->Tick(0.f);
	return FX;
}

AZoogiFX* AZoogiFX::SpawnTrail(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, float Strength)
{
	AZoogiFX* FX = Spawn(World, Location, Color, Radius, Duration, true, Strength);
	if (FX)
	{
		FX->BaseLight = 0.f;
		FX->Flash->SetIntensity(0.f);
		FX->Flash->SetVisibility(false);
	}
	return FX;
}

void AZoogiFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	if (Follow.IsValid()) SetActorLocation(Follow->GetActorLocation());
	const float Grow = FMath::InterpEaseOut(0.15f, 1.f, T, 2.5f);
	const float R = EndRadius * Grow;
	if (bIsRing) Mesh->SetWorldScale3D(FVector(R / 50.f, R / 50.f, 0.04f)); // engine cylinder: radius 50, height 100
	else Mesh->SetWorldScale3D(FVector(R / 50.f));
	const float Fade = 1.f - T * T;
	if (MID) MID->SetScalarParameterValue(TEXT("Strength"), BaseStrength * Fade);
	Flash->SetIntensity(BaseLight * (1.f - T));
	if (Age >= Life) Destroy();
}
