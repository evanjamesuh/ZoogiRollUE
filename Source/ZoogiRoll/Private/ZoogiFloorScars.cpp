// Reactive court floor scars (Frozen Ring, Oct 6). See ZoogiFloorScars.h.
#include "ZoogiFloorScars.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UTexture* LoadTex(const TCHAR* Name)
	{
		return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Env/Arenas/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	constexpr int32 MaxScars = 10;
}

AZoogiFloorScars::AZoogiFloorScars()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AZoogiFloorScars::Init(float InMaxRadius)
{
	MaxRadius = InMaxRadius;
	TexCrack = LoadTex(TEXT("T_FrozenDmg_Crack"));
	TexGlow = LoadTex(TEXT("T_FrozenDmg_GlowCrack"));
	TexCrater = LoadTex(TEXT("T_FrozenDmg_Crater"));
	TexArtCrater = LoadTex(TEXT("T_FrozenScar_Crater"));
	TexFrost = LoadTex(TEXT("T_FrozenDmg_Frost"));
	TexShards = LoadTex(TEXT("T_FrozenDmg_Shards"));
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenDecal.M_FrozenDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	bReady = M && TexCrack && TexGlow && TexCrater;
	if (!bReady)
	{
		UE_LOG(LogZoogi, Warning, TEXT("Floor scars: M_FrozenDecal / T_FrozenDmg_* missing (run Scripts/arenas/make_frozen_kit.py); scars off"));
		return;
	}
	for (int32 i = 0; i < MaxScars * 3; ++i)
	{
		UStaticMeshComponent* C = NewLayer(0.f);
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, this);
		C->SetMaterial(0, D);
		Pool.Add(C);
		PoolMIDs.Add(D);
	}
	UE_LOG(LogZoogi, Log, TEXT("Floor scars ready (%d layers)"), Pool.Num());
}

UStaticMeshComponent* AZoogiFloorScars::NewLayer(float Z)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane")));
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetReceivesDecals(false);
	C->SetTranslucentSortPriority(2);
	C->RegisterComponent();
	C->SetVisibility(false);
	C->SetRelativeLocation(FVector(0, 0, Z));
	return C;
}

int32 AZoogiFloorScars::NumScars() const
{
	int32 N = 0;
	for (const FScar& S : Scars) if (S.Stage > 0) ++N;
	return N;
}

void AZoogiFloorScars::ResetScars()
{
	for (UStaticMeshComponent* C : Pool) if (C) C->SetVisibility(false);
	Scars.Reset();
}

int32 AZoogiFloorScars::Impact(const FVector& P, float Strength, EZoogiArtifact Element)
{
	if (!bReady) return 0;
	const FVector2D P2(P);
	if (P2.Size() > MaxRadius) return 0;
	FScar* Near = nullptr;
	float Best = 1e9f;
	for (FScar& S : Scars)
	{
		const float D = FVector2D::Distance(FVector2D(S.P), P2);
		if (D < 150.f * S.Size && D < Best) { Best = D; Near = &S; }
	}
	if (Near && Clock - Near->LastAt < 0.6f) return Near->Stage;
	if (Element == EZoogiArtifact::Frost)
	{
		if (!Near) return 0;
		Near->Stage = FMath::Max(0, Near->Stage - 1);
		Near->LastAt = Clock;
		Near->Warm = 0.f;
		Apply(*Near);
		AZoogiFX::Spawn(GetWorld(), Near->P + FVector(0, 0, 10.f), FLinearColor(0.6f, 0.9f, 1.f), 140.f * Near->Size, 0.5f, true, 5.f);
		UE_LOG(LogZoogi, Log, TEXT("Floor scar refrozen by Frost -> stage %d"), Near->Stage);
		return Near->Stage;
	}
	int32 Add = Strength >= 2.5f ? 2 : 1;
	if (Element == EZoogiArtifact::Ember || Element == EZoogiArtifact::Titan) Add += 1;
	if (!Near)
	{
		if (Clock - LastNew < 0.8f && Strength < 2.5f) return 0;
		if (NumScars() >= MaxScars)
		{
			// Recycle the oldest, shallowest scar.
			int32 Victim = 0;
			for (int32 i = 1; i < Scars.Num(); ++i) if (Scars[i].Stage < Scars[Victim].Stage || (Scars[i].Stage == Scars[Victim].Stage && Scars[i].LastAt < Scars[Victim].LastAt)) Victim = i;
			for (auto& L : Scars[Victim].Layers) if (L.IsValid()) L->SetVisibility(false);
			Scars.RemoveAt(Victim);
		}
		FScar S;
		S.P = FVector(P.X, P.Y, 0.f);
		S.Size = FMath::Clamp(0.85f + 0.2f * Strength, 0.9f, 1.6f);
		S.Yaw = FMath::FRandRange(0.f, 360.f);
		// Claim 3 free layers.
		for (int32 i = 0; i < Pool.Num() && S.Layers.Num() < 3; ++i)
		{
			bool bUsed = false;
			for (const FScar& O : Scars) for (const auto& L : O.Layers) if (L.Get() == Pool[i]) bUsed = true;
			if (!bUsed) { S.Layers.Add(Pool[i]); S.MIDs.Add(PoolMIDs[i]); }
		}
		if (S.Layers.Num() < 3) return 0;
		Scars.Add(S);
		Near = &Scars.Last();
		LastNew = Clock;
		Add -= 1; // a fresh scar starts at stage 1 and escalates by the rest
		Near->Stage = 1;
	}
	Near->Stage = FMath::Clamp(Near->Stage + Add, 1, 3);
	Near->LastAt = Clock;
	Near->Pulse = 1.f;
	if (Element == EZoogiArtifact::Ember) Near->Warm = 1.f;
	Apply(*Near);
	if (Near->Stage >= 3) AZoogiFX::Spawn(GetWorld(), Near->P + FVector(0, 0, 20.f), FLinearColor(0.1f, 0.76f, 0.9f), 200.f * Near->Size, 0.6f, true, 6.f);
	UE_LOG(LogZoogi, Log, TEXT("Floor scar at (%.0f, %.0f) -> %s (strength %.1f)"), Near->P.X, Near->P.Y,
		Near->Stage == 1 ? TEXT("DAMAGED") : (Near->Stage == 2 ? TEXT("CRACKED") : TEXT("CRATER")), Strength);
	return Near->Stage;
}

void AZoogiFloorScars::SetScar(const FVector& P, int32 Stage, float Size)
{
	if (!bReady) return;
	Impact(P, 1.f, EZoogiArtifact::None);
	for (FScar& S : Scars)
		if (FVector2D::Distance(FVector2D(S.P), FVector2D(P)) < 5.f || FVector2D::Distance(FVector2D(S.P), FVector2D(P)) < 150.f)
		{
			S.Stage = FMath::Clamp(Stage, 0, 3);
			S.Size = Size;
			S.Pulse = 0.f;
			Apply(S);
			return;
		}
}

void AZoogiFloorScars::Apply(FScar& S)
{
	auto Layer = [&](int32 i, UTexture* Tex, float Scale, float Z, const FLinearColor& Tint, float Bright, float Opacity, float Emis, float YawOff)
	{
		if (!S.Layers.IsValidIndex(i) || !S.Layers[i].IsValid()) return;
		UStaticMeshComponent* C = S.Layers[i].Get();
		if (!Tex) { C->SetVisibility(false); return; }
		UMaterialInstanceDynamic* D = S.MIDs[i].Get();
		D->SetTextureParameterValue(TEXT("Tex"), Tex);
		D->SetVectorParameterValue(TEXT("Tint"), Tint);
		D->SetScalarParameterValue(TEXT("Bright"), Bright);
		D->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		D->SetScalarParameterValue(TEXT("Emissive"), Emis);
		C->SetWorldTransform(FTransform(FRotator(0.f, S.Yaw + YawOff, 0.f), S.P + FVector(0, 0, Z), FVector(Scale * S.Size, Scale * S.Size, 1.f)));
		C->SetVisibility(true);
	};
	for (auto& L : S.Layers) if (L.IsValid()) L->SetVisibility(false);
	const FLinearColor Warm = FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor(1.f, 0.75f, 0.6f), S.Warm);
	switch (S.Stage)
	{
	case 1: // DAMAGED: hairline cracks + frost scuff
		Layer(0, TexCrack, 2.2f, 0.6f, Warm, 0.95f, 0.9f, 0.f, 0.f);
		Layer(2, TexFrost, 1.6f, 0.8f, FLinearColor::White, 1.f, 0.6f, 0.f, 35.f);
		break;
	case 2: // CRACKED: glow-crack burst over the cracks + shards
		Layer(0, TexCrack, 2.6f, 0.6f, Warm, 0.9f, 1.f, 0.f, 0.f);
		Layer(1, TexGlow, 2.6f, 1.0f, FLinearColor(0.75f, 1.f, 1.f), 0.6f, 0.95f, 1.6f, 15.f);
		Layer(2, TexShards, 1.8f, 0.8f, FLinearColor::White, 1.f, 0.8f, 0.f, 70.f);
		break;
	case 3: // CRATER: the damaged.png crater (charcoal + cyan cracks), plus a long glow-crack network
		Layer(0, TexArtCrater ? TexArtCrater.Get() : TexCrater.Get(), 3.0f, 0.7f, Warm, 1.f, 1.f, 0.15f, 0.f);
		Layer(1, TexGlow, 3.8f, 1.1f, FLinearColor(0.75f, 1.f, 1.f), 0.6f, 1.f, 1.8f, 25.f);
		Layer(2, TexCrater, 1.9f, 0.9f, Warm, 0.9f, 0.85f, 0.f, 50.f);
		break;
	default:
		break;
	}
}

void AZoogiFloorScars::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	for (FScar& S : Scars)
	{
		if (S.Pulse <= 0.f || S.Stage < 2) continue;
		S.Pulse = FMath::Max(0.f, S.Pulse - DeltaSeconds * 0.6f);
		if (S.MIDs.IsValidIndex(1) && S.MIDs[1].IsValid()) S.MIDs[1]->SetScalarParameterValue(TEXT("Emissive"), (S.Stage == 3 ? 1.8f : 1.6f) * (1.f + 0.9f * S.Pulse));
	}
}
