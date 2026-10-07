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
	constexpr int32 NumLayers = 4;
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
	TexS1 = LoadTex(TEXT("T_FrozenDisp_S1"));
	TexS2 = LoadTex(TEXT("T_FrozenDisp_S2"));
	TexS3 = LoadTex(TEXT("T_FrozenDisp_S3"));
	TexTitan = LoadTex(TEXT("T_FrozenDisp_Titan"));
	TexPackCrater = LoadTex(TEXT("T_FrozenDisp_Crater"));
	TexPowEmber = LoadTex(TEXT("T_FrozenDisp_Ember"));
	TexPowFrost = LoadTex(TEXT("T_FrozenDisp_Frost"));
	TexPowStorm = LoadTex(TEXT("T_FrozenDisp_Storm"));
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenDecal.M_FrozenDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	bReady = M && TexCrack && TexGlow && TexCrater;
	if (!bReady)
	{
		UE_LOG(LogZoogi, Warning, TEXT("Floor scars: M_FrozenDecal / T_FrozenDmg_* missing (run Scripts/arenas/make_frozen_kit.py); scars off"));
		return;
	}
	for (int32 i = 0; i < MaxScars * NumLayers; ++i)
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
	ClearCraterSlabs();
}

void AZoogiFloorScars::ClearCraterSlabs()
{
	for (UStaticMeshComponent* C : SlabPool) if (C) C->SetVisibility(false);
	for (FSlab& Sl : Slabs) Sl.Born = -10.f;
	NextSlab = 0;
}

void AZoogiFloorScars::SpawnCraterSlabs(const FVector& Loc, const FVector& Normal, float Power, UMaterialInterface* Mat)
{
	constexpr int32 MaxSlabs = 60;
	UMaterialInterface* M = Mat ? Mat : SlabMat.Get();
	auto Claim = [&]() -> int32
	{
		if (SlabPool.Num() < MaxSlabs)
		{
			UStaticMeshComponent* NewC = NewObject<UStaticMeshComponent>(this);
			NewC->SetMobility(EComponentMobility::Movable);
			NewC->SetupAttachment(Root);
			NewC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			NewC->SetCastShadow(true);
			NewC->RegisterComponent();
			SlabPool.Add(NewC);
			Slabs.AddDefaulted();
		}
		const int32 I = NextSlab % SlabPool.Num();
		NextSlab = (NextSlab + 1) % MaxSlabs;
		return I;
	};
	if (SlabMeshes.Num() > 0)
	{
		// A&P CraterSlab tiers (destruction_states.json craterSlab.tiers): Cracked / Shattered / Titan.
		int32 Cnt; float T0, T1, Rad, Sc, PopS;
		if (Power < 1.6f) { Cnt = 6; T0 = 18.f; T1 = 28.f; Rad = 70.f; Sc = 0.55f; PopS = 0.12f; }
		else if (Power < 2.6f) { Cnt = 9; T0 = 26.f; T1 = 45.f; Rad = 120.f; Sc = 1.f; PopS = 0.10f; }
		else { Cnt = 12; T0 = 30.f; T1 = 60.f; Rad = 170.f; Sc = 1.6f; PopS = 0.08f; }
		const float Base0 = FMath::FRandRange(0.f, 360.f);
		int32 Made = 0, Culled = 0;
		for (int32 i = 0; i < Cnt; ++i)
		{
			const float Yd = Base0 + i * 360.f / Cnt + FMath::FRandRange(-8.f, 8.f);
			const FVector Out(FMath::Cos(FMath::DegreesToRadians(Yd)), FMath::Sin(FMath::DegreesToRadians(Yd)), 0.f);
			const FVector Hinge = Loc + Out * Rad * FMath::FRandRange(0.9f, 1.1f);
			// Orb-exit gap rule: no slab whose hinge falls inside a gap volume.
			bool bCull = false;
			if (NoSlabHalfDeg > 0.f && FVector2D(Hinge).Size() > NoSlabMinR)
			{
				const float HY = FMath::RadiansToDegrees(FMath::Atan2(Hinge.Y, Hinge.X));
				for (float G : NoSlabYaw) if (FMath::Abs(FMath::FindDeltaAngleDegrees(HY, G)) < NoSlabHalfDeg) bCull = true;
			}
			if (bCull) { ++Culled; continue; }
			const int32 Idx = Claim();
			UStaticMeshComponent* C = SlabPool[Idx];
			if (!C) continue;
			C->SetStaticMesh(SlabMeshes[i % SlabMeshes.Num()]);
			if (M) C->SetMaterial(0, M);
			if (SlabSideMat) C->SetMaterial(1, SlabSideMat);
			// Pivot = outer top edge, local +X to the crater centre, tilt about local Y (inner edge rises).
			C->SetWorldRotation(FRotator(FMath::FRandRange(T0, T1), Yd + 180.f, FMath::FRandRange(-4.f, 4.f)));
			FSlab& Sl = Slabs[Idx];
			Sl.Scale = FVector(Sc * FMath::FRandRange(0.85f, 1.15f));
			Sl.Base = Hinge;
			Sl.Lift = FVector(0, 0, 18.f * Sc);
			Sl.Born = Clock + FMath::FRandRange(0.f, 0.03f) - (0.12f - PopS);
			C->SetWorldLocation(Sl.Base - Sl.Lift);
			C->SetWorldScale3D(Sl.Scale * 0.05f);
			C->SetVisibility(true);
			++Made;
		}
		UE_LOG(LogZoogi, Log, TEXT("Crater slabs (kit): %d at (%.0f, %.0f) power %.1f, %d culled in orb-exit gaps"), Made, Loc.X, Loc.Y, Power, Culled);
		return;
	}
	const float Pw = FMath::Clamp((Power - 1.f) / 2.f, 0.f, 1.f);
	const int32 N = FMath::Clamp(FMath::RoundToInt(6.f + 6.f * Pw), 6, 12);
	const float Tilt = FMath::Lerp(20.f, 55.f, Pw);
	const float Ring = FMath::Lerp(85.f, 135.f, Pw);
	const FVector Up = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Ax = FVector::CrossProduct(Up, FMath::Abs(Up.Z) > 0.9f ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
	const FVector Ay = FVector::CrossProduct(Up, Ax);
	const float A0 = FMath::FRandRange(0.f, 360.f);
	for (int32 i = 0; i < N; ++i)
	{
		if (SlabPool.Num() < MaxSlabs)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			C->RegisterComponent();
			SlabPool.Add(C);
			Slabs.AddDefaulted();
		}
		const int32 Idx = NextSlab % SlabPool.Num();
		NextSlab = (NextSlab + 1) % MaxSlabs;
		UStaticMeshComponent* C = SlabPool[Idx];
		if (!C) continue;
		C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
		const float A = FMath::DegreesToRadians(A0 + i * 360.f / N + FMath::FRandRange(-9.f, 9.f));
		const FVector Out = Ax * FMath::Cos(A) + Ay * FMath::Sin(A);
		const float Rr = Ring * FMath::FRandRange(0.88f, 1.12f);
		const float Tl = Tilt * FMath::FRandRange(0.8f, 1.15f);
		// Slab basis: X = radial (outer edge buried, inner edge heaved up), Y = tangent, Z = slab normal.
		const FVector Xs = (Out * FMath::Cos(FMath::DegreesToRadians(Tl)) - Up * FMath::Sin(FMath::DegreesToRadians(Tl))).GetSafeNormal();
		const FVector Ys = FVector::CrossProduct(Up, Out).GetSafeNormal();
		const FVector Zs = FVector::CrossProduct(Xs, Ys);
		C->SetWorldRotation(FMatrix(Xs, Ys, Zs, FVector::ZeroVector).Rotator());
		FSlab& Sl = Slabs[Idx];
		Sl.Scale = FVector(FMath::FRandRange(0.6f, 0.9f), FMath::FRandRange(0.45f, 0.7f), 0.13f) * FMath::Lerp(0.9f, 1.35f, Pw);
		Sl.Base = Loc + Out * Rr + Up * (Sl.Scale.X * 50.f * FMath::Sin(FMath::DegreesToRadians(Tl)) * 0.45f);
		Sl.Lift = Up * 40.f * (0.5f + Pw);
		Sl.Born = Clock;
		C->SetWorldLocation(Sl.Base - Sl.Lift);
		C->SetWorldScale3D(Sl.Scale * 0.05f);
		if (M) C->SetMaterial(0, M);
		C->SetVisibility(true);
	}
	UE_LOG(LogZoogi, Log, TEXT("Crater slabs: %d at (%.0f, %.0f) power %.1f tilt %.0f"), N, Loc.X, Loc.Y, Power, Tilt);
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
		if (D < 240.f * S.Size && D < Best) { Best = D; Near = &S; }
	}
	if (Near && Clock - Near->LastAt < 0.6f) return Near->Stage;
	if (Element == EZoogiArtifact::Frost)
	{
		if (!Near) return 0;
		Near->Stage = FMath::Max(0, Near->Stage - 1);
		Near->LastAt = Clock;
		Near->Warm = 0.f;
		Near->Elem = EZoogiArtifact::Frost;
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
		// Claim the free layers.
		for (int32 i = 0; i < Pool.Num() && S.Layers.Num() < NumLayers; ++i)
		{
			bool bUsed = false;
			for (const FScar& O : Scars) for (const auto& L : O.Layers) if (L.Get() == Pool[i]) bUsed = true;
			if (!bUsed) { S.Layers.Add(Pool[i]); S.MIDs.Add(PoolMIDs[i]); }
		}
		if (S.Layers.Num() < NumLayers) return 0;
		Scars.Add(S);
		Near = &Scars.Last();
		LastNew = Clock;
		Add -= 1; // a fresh scar starts at stage 1 and escalates by the rest
		Near->Stage = 1;
	}
	const int32 PrevStage = Near->Stage;
	Near->Stage = FMath::Clamp(Near->Stage + Add, 1, 3);
	Near->LastAt = Clock;
	if (Near->Stage >= 3 && PrevStage < 3) SpawnCraterSlabs(Near->P, FVector::UpVector, FMath::Max(Strength, 1.f) * Near->Size);
	Near->Pulse = 1.f;
	if (Element == EZoogiArtifact::Ember) Near->Warm = 1.f;
	if (Element != EZoogiArtifact::None) Near->Elem = Element;
	Apply(*Near);
	if (Near->Stage >= 3) AZoogiFX::Spawn(GetWorld(), Near->P + FVector(0, 0, 20.f), FLinearColor(0.1f, 0.76f, 0.9f), 200.f * Near->Size, 0.6f, true, 6.f);
	UE_LOG(LogZoogi, Log, TEXT("Floor scar at (%.0f, %.0f) -> %s (strength %.1f)"), Near->P.X, Near->P.Y,
		Near->Stage == 1 ? TEXT("DAMAGED") : (Near->Stage == 2 ? TEXT("CRACKED") : TEXT("CRATER")), Strength);
	return Near->Stage;
}

void AZoogiFloorScars::SetScar(const FVector& P, int32 Stage, float Size)
{
	if (!bReady) return;
	// Forced placement (shot harness / scripted damage): bypass the new-scar rate limit and the per-scar cooldown.
	LastNew = -100.f;
	for (FScar& S : Scars) S.LastAt = -100.f;
	Impact(P, 1.f, EZoogiArtifact::None);
	for (FScar& S : Scars)
		if (FVector2D::Distance(FVector2D(S.P), FVector2D(P)) < 5.f || FVector2D::Distance(FVector2D(S.P), FVector2D(P)) < 150.f)
		{
			const bool bNewCrater = Stage >= 3 && S.Stage < 3;
			S.Stage = FMath::Clamp(Stage, 0, 3);
			S.Size = Size;
			if (bNewCrater) SpawnCraterSlabs(S.P, FVector::UpVector, 1.5f * Size);
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
	const FLinearColor Cy(0.35f, 0.88f, 1.f);
	const bool bPack = TexS1 && TexS2 && TexS3 && TexPackCrater;
	if (bPack)
	{
		// Round 3 (Animations & Powers pack, pass 2 sizes 450 / 560 / 640 cm, Buzzy damaged.png scale for craters):
		// DAMAGED = 4 short glowing cracks, CRACKED = spreading glowing web, CRATER = dark charcoal bowl + full glowing web.
		switch (S.Stage)
		{
		case 1:
			Layer(0, TexS1, 4.6f, 0.6f, Warm, 1.f, 1.f, 0.9f, 0.f);
			Layer(2, TexFrost, 2.6f, 0.8f, FLinearColor::White, 1.f, 0.55f, 0.f, 35.f);
			break;
		case 2:
			Layer(0, TexS2, 6.4f, 0.6f, Warm, 1.f, 1.f, 1.3f, 0.f);
			Layer(2, TexShards, 3.0f, 0.8f, FLinearColor::White, 1.f, 0.8f, 0.f, 70.f);
			break;
		case 3:
			Layer(0, TexS3, 10.f, 0.6f, Warm, 1.f, 1.f, 1.45f, 0.f);
			Layer(1, TexPackCrater, 6.8f, 1.0f, Warm, 0.42f, 1.f, 0.55f, 25.f);
			Layer(2, TexArtCrater ? TexArtCrater.Get() : TexCrater.Get(), 4.6f, 1.2f, Warm, 0.35f, 0.9f, 0.15f, 50.f);
			break;
		default:
			break;
		}
		// Layer 3: power overlay (Ember melt, Frost rime / refreeze, Storm arcs, Titan heavy web).
		UTexture* Pow = nullptr; float PowScale = 4.f, PowEmis = 0.6f, PowOp = 0.8f;
		switch (S.Elem)
		{
		case EZoogiArtifact::Ember: Pow = TexPowEmber; PowScale = 3.6f; PowEmis = 1.1f; PowOp = 0.7f; break;
		case EZoogiArtifact::Frost: Pow = TexPowFrost; PowScale = 4.4f; PowEmis = 0.2f; PowOp = 0.85f; break;
		case EZoogiArtifact::Storm: Pow = TexPowStorm; PowScale = 4.8f; PowEmis = 1.6f; PowOp = 0.9f; break;
		case EZoogiArtifact::Titan: Pow = TexTitan; PowScale = 9.f; PowEmis = 1.5f; PowOp = 0.95f; break;
		default: break;
		}
		if (Pow && (S.Stage > 0 || S.Elem == EZoogiArtifact::Frost)) Layer(3, Pow, PowScale, 1.4f, FLinearColor::White, 1.f, PowOp, PowEmis, 10.f);
		return;
	}
	switch (S.Stage)
	{
	case 1: // DAMAGED: hairline cracks + frost scuff
		Layer(0, TexCrack, 3.4f, 0.6f, Warm, 0.85f, 1.f, 0.f, 0.f);
		Layer(2, TexFrost, 2.4f, 0.8f, FLinearColor::White, 1.f, 0.6f, 0.f, 35.f);
		break;
	case 2: // CRACKED: glow-crack burst over the cracks + shards
		Layer(0, TexCrack, 4.2f, 0.6f, Warm, 0.75f, 1.f, 0.f, 0.f);
		Layer(1, TexGlow, 4.4f, 1.0f, Cy, 0.5f, 0.95f, 0.75f, 15.f);
		Layer(2, TexShards, 2.8f, 0.8f, FLinearColor::White, 1.f, 0.8f, 0.f, 70.f);
		break;
	case 3: // CRATER: the damaged.png crater (charcoal + cyan cracks), plus a long glow-crack network
		Layer(0, TexArtCrater ? TexArtCrater.Get() : TexCrater.Get(), 5.6f, 0.7f, Warm, 0.55f, 1.f, 0.05f, 0.f);
		Layer(1, TexGlow, 6.0f, 1.1f, Cy, 0.5f, 1.f, 0.85f, 25.f);
		Layer(2, TexCrater, 3.2f, 0.9f, Warm, 0.45f, 0.95f, 0.f, 50.f);
		break;
	default:
		break;
	}
}

void AZoogiFloorScars::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	// Crater slab pop -> overshoot -> settle (placeholder curve until the A&P curve CSV lands).
	for (int32 i = 0; i < Slabs.Num() && i < SlabPool.Num(); ++i)
	{
		const float Age = Clock - Slabs[i].Born;
		if (Age < 0.f || Age > 0.6f || !SlabPool[i]) continue;
		const float T = FMath::Clamp(Age / 0.6f, 0.f, 1.f);
		const float Pop = T < 0.25f ? FMath::InterpEaseOut(0.f, 1.18f, T / 0.25f, 2.f) : FMath::Lerp(1.18f, 1.f, FMath::InterpEaseInOut(0.f, 1.f, (T - 0.25f) / 0.75f, 2.f));
		SlabPool[i]->SetWorldLocation(Slabs[i].Base - Slabs[i].Lift * (1.f - FMath::Min(Pop, 1.f)) + Slabs[i].Lift * 0.25f * FMath::Max(0.f, Pop - 1.f) / 0.18f);
		SlabPool[i]->SetWorldScale3D(Slabs[i].Scale * FMath::Max(0.05f, FMath::Min(Pop, 1.06f)));
	}
	for (FScar& S : Scars)
	{
		if (S.Pulse <= 0.f || S.Stage < 2) continue;
		S.Pulse = FMath::Max(0.f, S.Pulse - DeltaSeconds * 0.6f);
		if (TexS1 && TexS3) { if (S.MIDs.IsValidIndex(0) && S.MIDs[0].IsValid()) S.MIDs[0]->SetScalarParameterValue(TEXT("Emissive"), (S.Stage == 3 ? 1.45f : 1.3f) * (1.f + 0.7f * S.Pulse)); }
		else if (S.MIDs.IsValidIndex(1) && S.MIDs[1].IsValid()) S.MIDs[1]->SetScalarParameterValue(TEXT("Emissive"), (S.Stage == 3 ? 1.8f : 1.6f) * (1.f + 0.9f * S.Pulse));
	}
}
