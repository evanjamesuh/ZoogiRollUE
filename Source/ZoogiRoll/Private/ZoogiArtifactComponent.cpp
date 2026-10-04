#include "ZoogiArtifactComponent.h"
#include "ZoogiBall.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 NumOrbiters = 6;
	constexpr int32 NumPlates = 12;

	UMaterialInterface* ArtifactBaseMaterial()
	{
		// Procedural artifact material (Scripts/make_artifact_material.py). Falls back to M_ZoogiFX via FxMID.
		static TWeakObjectPtr<UMaterialInterface> M;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_ZoogiArtifact.M_ZoogiArtifact"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return M.Get();
	}

	UMaterialInstanceDynamic* MakeOverlayMID(UObject* Outer, const FLinearColor& C, float Strength)
	{
		if (UMaterialInterface* Base = ArtifactBaseMaterial())
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Outer);
			MID->SetVectorParameterValue(TEXT("Color"), C);
			MID->SetScalarParameterValue(TEXT("Strength"), Strength);
			return MID;
		}
		UMaterialInstanceDynamic* MID = ZoogiUtil::FxMID(Outer, C, Strength);
		MID->SetScalarParameterValue(TEXT("Ring"), 0.f);
		return MID;
	}
}

UZoogiArtifactComponent::UZoogiArtifactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

AZoogiBall* UZoogiArtifactComponent::Ball() const
{
	return Cast<AZoogiBall>(GetOwner());
}

void UZoogiArtifactComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildVisuals();
}

void UZoogiArtifactComponent::BuildVisuals()
{
	AZoogiBall* B = Ball();
	if (bBuilt || !B || !B->Sphere) return;
	bBuilt = true;
	auto MakeMesh = [B, this](const TCHAR* Shape, FName Name, bool bAbsRot) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(B, Name);
		C->SetupAttachment(B->Sphere);
		C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetUsingAbsoluteRotation(bAbsRot);
		C->SetVisibility(false);
		C->RegisterComponent();
		B->AddInstanceComponent(C);
		return C;
	};
	Shell = MakeMesh(TEXT("Sphere"), TEXT("ArtifactShell"), true);
	ShellMID = MakeOverlayMID(this, FLinearColor::White, 1.f);
	Shell->SetMaterial(0, ShellMID);
	GuardShell = MakeMesh(TEXT("Sphere"), TEXT("ArtifactGuard"), true);
	GuardMID = ZoogiUtil::FxMID(this, FLinearColor::White, 1.f);
	GuardMID->SetScalarParameterValue(TEXT("Ring"), 0.f);
	GuardShell->SetMaterial(0, GuardMID);
	for (int32 i = 0; i < NumOrbiters; ++i)
	{
		UStaticMeshComponent* O = MakeMesh(TEXT("Sphere"), FName(*FString::Printf(TEXT("ArtifactOrbit%d"), i)), true);
		UMaterialInstanceDynamic* MID = ZoogiUtil::FxMID(this, FLinearColor::White, 4.f);
		MID->SetScalarParameterValue(TEXT("Ring"), 0.f);
		O->SetMaterial(0, MID);
		Orbiters.Add(O);
		OrbitMIDs.Add(MID);
	}
	// Plates / cracks / shards ride on the rolling body (attached to the physics sphere, so they roll with it).
	for (int32 i = 0; i < NumPlates; ++i)
	{
		UStaticMeshComponent* P = MakeMesh(TEXT("Cube"), FName(*FString::Printf(TEXT("ArtifactPlate%d"), i)), false);
		UMaterialInstanceDynamic* MID = ZoogiUtil::FxMID(this, FLinearColor::White, 4.f);
		MID->SetScalarParameterValue(TEXT("Ring"), 0.f);
		P->SetMaterial(0, MID);
		Plates.Add(P);
		PlateMIDs.Add(MID);
	}
}

void UZoogiArtifactComponent::SetInfused(EZoogiArtifact A)
{
	Infused = A;
	LookAge = 0.f;
	ApplyLook();
	Pulse(1.f);
}

void UZoogiArtifactComponent::SetGuard(EZoogiArtifact A, int32 Turn)
{
	Guard = A;
	GuardSetTurn = Turn;
	GuardFlashAge = 0.f;
	ApplyLook();
}

void UZoogiArtifactComponent::ClearInfused()
{
	if (Infused == EZoogiArtifact::None) return;
	Infused = EZoogiArtifact::None;
	ApplyLook();
}

void UZoogiArtifactComponent::ClearGuard()
{
	if (Guard == EZoogiArtifact::None) return;
	Guard = EZoogiArtifact::None;
	GuardSetTurn = -1;
	ApplyLook();
}

void UZoogiArtifactComponent::ResetAll()
{
	Hand.Reset();
	Drawn = EZoogiArtifact::None;
	Infused = EZoogiArtifact::None;
	Guard = EZoogiArtifact::None;
	GuardSetTurn = -1;
	Status = EZoogiStatus::None;
	StatusTurns = 0;
	BonusDraws = 0;
	bGuardHitPending = false;
	ApplyLook();
}

void UZoogiArtifactComponent::Pulse(float Strength) { PulseAge = 0.f; PulseStrength = FMath::Clamp(Strength, 0.f, 1.5f); }
void UZoogiArtifactComponent::ImpactPulse(float Strength) { ImpactAge = 0.f; ImpactStrength = FMath::Clamp(Strength, 0.f, 1.5f); }
void UZoogiArtifactComponent::GuardFlash() { GuardFlashAge = 0.f; }

FString UZoogiArtifactComponent::StatusLabel() const
{
	return (Status != EZoogiStatus::None && StatusTurns > 0) ? FString(ZoogiArtifacts::StatusName(Status)) : FString();
}

void UZoogiArtifactComponent::ApplyLook()
{
	BuildVisuals();
	AZoogiBall* B = Ball();
	if (!B || !Shell) return;
	const EZoogiArtifact A = Infused;
	const bool bOn = A != EZoogiArtifact::None;
	const FZoogiArtifactRow& R = ZoogiArtifacts::Get(A);
	const FLinearColor C = ZoogiArtifacts::Color(A);

	// Shell change: translucent glowing shell + overlay rim on the character model (GLB materials untouched).
	Shell->SetVisibility(bOn);
	if (bOn)
	{
		ShellMID->SetVectorParameterValue(TEXT("Color"), C);
		ShellMID->SetScalarParameterValue(TEXT("Pattern"), (float)(int32)A);
		if (!OverlayMID) OverlayMID = MakeOverlayMID(this, C, 2.f);
		OverlayMID->SetVectorParameterValue(TEXT("Color"), A == EZoogiArtifact::Void ? R.RimColor : C);
		OverlayMID->SetScalarParameterValue(TEXT("Pattern"), (float)(int32)A);
		if (B->Visual) B->Visual->SetOverlayMaterial(OverlayMID);
	}
	else
	{
		if (B->Visual) B->Visual->SetOverlayMaterial(nullptr);
		if (B->TurnLight)
		{
			B->TurnLight->SetLightColor(ZoogiUtil::PlayerColor(B->PlayerIndex));
			B->TurnLight->SetIntensity(B->IsActiveTurn() ? 60.f : 0.f);
		}
	}

	// Orbiters: Storm arcs (stretched sparks), Void rings (flat ring cylinders), Ember motes.
	const int32 NOrb = A == EZoogiArtifact::Storm ? 3 : (A == EZoogiArtifact::Void ? 2 : (A == EZoogiArtifact::Ember ? 5 : (A == EZoogiArtifact::Frost ? 4 : 0)));
	for (int32 i = 0; i < Orbiters.Num(); ++i)
	{
		UStaticMeshComponent* O = Orbiters[i];
		const bool bVis = bOn && i < NOrb;
		O->SetVisibility(bVis);
		if (!bVis) continue;
		const bool bRing = A == EZoogiArtifact::Void;
		O->SetStaticMesh(ZoogiUtil::BasicShape(bRing ? TEXT("Cylinder") : TEXT("Sphere")));
		OrbitMIDs[i]->SetVectorParameterValue(TEXT("Color"), A == EZoogiArtifact::Storm ? FLinearColor(0.85f, 0.95f, 1.f) : C);
		OrbitMIDs[i]->SetScalarParameterValue(TEXT("Ring"), bRing ? 1.f : 0.f);
		OrbitMIDs[i]->SetScalarParameterValue(TEXT("Strength"), bRing ? 6.f : 8.f);
	}

	// Plates on the shell: Ember cracks (thin glowing slivers), Storm veins, Frost shards (cones), Titan stone plates.
	const int32 NPl = A == EZoogiArtifact::Void ? 0 : (A == EZoogiArtifact::Titan ? 8 : NumPlates);
	for (int32 i = 0; i < Plates.Num(); ++i)
	{
		UStaticMeshComponent* P = Plates[i];
		const bool bVis = bOn && i < NPl;
		P->SetVisibility(bVis);
		if (!bVis) continue;
		// Fibonacci sphere directions for an even spread.
		const float Y = 1.f - 2.f * (i + 0.5f) / NPl;
		const float Rad = FMath::Sqrt(FMath::Max(0.f, 1.f - Y * Y));
		const float Th = i * 2.39996f;
		const FVector Dir(FMath::Cos(Th) * Rad, FMath::Sin(Th) * Rad, Y);
		const FRotator Rot = FRotationMatrix::MakeFromZ(Dir).Rotator();
		FLinearColor PC = C;
		float Str = 6.f;
		const TCHAR* Shape = TEXT("Cube");
		FVector Scale(0.03f, 0.34f, 0.03f);
		float Out = ZoogiConst::ZoogiRadius * 0.98f;
		switch (A)
		{
		case EZoogiArtifact::Ember: PC = FLinearColor(1.f, 0.55f, 0.1f); Str = 14.f; Scale = FVector(0.025f, 0.38f + 0.1f * (i % 3), 0.025f); break;
		case EZoogiArtifact::Storm: PC = FLinearColor(0.75f, 0.92f, 1.f); Str = 12.f; Scale = FVector(0.018f, 0.45f, 0.018f); break;
		case EZoogiArtifact::Frost: PC = FLinearColor(0.75f, 0.97f, 1.f); Str = 3.f; Shape = TEXT("Cone"); Scale = FVector(0.12f, 0.12f, 0.22f); Out = ZoogiConst::ZoogiRadius * 0.92f; break;
		case EZoogiArtifact::Titan: PC = FLinearColor(0.35f, 0.27f, 0.18f); Str = 1.2f; Scale = FVector(0.5f, 0.5f, 0.1f); Out = ZoogiConst::ZoogiRadius * 1.0f; break;
		default: break;
		}
		P->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
		P->SetRelativeLocationAndRotation(Dir * Out, Rot);
		P->SetRelativeScale3D(Scale);
		P->ComponentTags.Reset();
		P->ComponentTags.Add(FName(*FString::Printf(TEXT("%.4f,%.4f,%.4f"), Scale.X, Scale.Y, Scale.Z)));
		PlateMIDs[i]->SetVectorParameterValue(TEXT("Color"), PC);
		PlateMIDs[i]->SetScalarParameterValue(TEXT("Strength"), Str);
	}

	// Guard bubble.
	const bool bGuard = Guard != EZoogiArtifact::None;
	GuardShell->SetVisibility(bGuard);
	if (bGuard) GuardMID->SetVectorParameterValue(TEXT("Color"), ZoogiArtifacts::Color(Guard));
}

void UZoogiArtifactComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickVisuals(FMath::Min(DeltaTime, 0.05f));
}

void UZoogiArtifactComponent::TickVisuals(float Dt)
{
	AZoogiBall* B = Ball();
	if (!B || !bBuilt) return;
	Clock += Dt;
	LookAge += Dt;
	PulseAge += Dt;
	ImpactAge += Dt;
	GuardFlashAge += Dt;
	if (B->bOut)
	{
		if (Shell->IsVisible()) Shell->SetVisibility(false);
		return;
	}
	const FVector L = B->BallLocation();
	const FVector V = B->GetBallVelocity();
	const float Spd = FVector2D(V.X, V.Y).Size();
	const EZoogiArtifact A = Infused;
	const FLinearColor C = ZoogiArtifacts::Color(A);
	const float Spread = FMath::Clamp(LookAge / 0.6f, 0.f, 1.f); // shell change spreads in over 0.6 s
	const float PulseK = PulseAge < 0.5f ? FMath::Sin(PI * PulseAge / 0.5f) * PulseStrength : 0.f;
	const float ImpK = ImpactAge < 0.22f ? FMath::Sin(PI * ImpactAge / 0.22f) * ImpactStrength : 0.f;

	if (A != EZoogiArtifact::None)
	{
		// Core glow: per-element rhythm.
		float Beat = 0.5f + 0.5f * FMath::Sin(Clock * 3.f);
		switch (A)
		{
		case EZoogiArtifact::Ember: Beat = 0.6f + 0.4f * FMath::Sin(Clock * 2.2f) * FMath::Sin(Clock * 7.3f); break;           // heartbeat flicker
		case EZoogiArtifact::Storm: Beat = (FMath::Frac(Clock * 5.3f) < 0.18f) ? 1.f : 0.45f; break;                          // sharp flicker
		case EZoogiArtifact::Frost: Beat = 0.55f + 0.25f * FMath::Sin(Clock * 1.4f); break;                                   // slow breathing
		case EZoogiArtifact::Titan: Beat = 0.5f + 0.15f * FMath::Sin(Clock * 0.9f); break;                                    // very slow
		case EZoogiArtifact::Void: Beat = 0.35f + 0.2f * FMath::Sin(Clock * 1.7f); break;
		default: break;
		}
		const float Strength = (0.6f + 1.4f * Beat) * Spread + 4.f * PulseK + 5.f * ImpK;
		ShellMID->SetScalarParameterValue(TEXT("Strength"), A == EZoogiArtifact::Void ? Strength * 0.6f : Strength);
		ShellMID->SetScalarParameterValue(TEXT("Mask"), Spread);
		if (OverlayMID)
		{
			OverlayMID->SetScalarParameterValue(TEXT("Strength"), (1.5f + 2.5f * Beat) * Spread + 6.f * PulseK + 6.f * ImpK);
			OverlayMID->SetScalarParameterValue(TEXT("Mask"), Spread);
		}
		if (B->TurnLight)
		{
			B->TurnLight->SetLightColor(C);
			B->TurnLight->SetIntensity(30.f + 90.f * Beat * Spread + 300.f * PulseK + 400.f * ImpK);
		}

		// Shell: slight expansion on pulses, squash and stretch along the velocity, compression on impact.
		const FVector Fwd = Spd > 60.f ? FVector(V.X, V.Y, 0.f).GetSafeNormal() : FVector(1, 0, 0);
		const float Stretch = FMath::Clamp(Spd / 3000.f, 0.f, 0.18f) - 0.22f * ImpK;
		const float Base = (A == EZoogiArtifact::Titan ? 1.09f : 1.12f) + 0.1f * PulseK;
		Shell->SetWorldRotation(Fwd.Rotation());
		Shell->SetWorldScale3D(FVector(Base * (1.f + Stretch), Base * (1.f - 0.5f * Stretch), Base * (1.f - 0.5f * Stretch)));
		Shell->SetWorldLocation(L);

		// Orbiters.
		for (int32 i = 0; i < Orbiters.Num(); ++i)
		{
			UStaticMeshComponent* O = Orbiters[i];
			if (!O->IsVisible()) continue;
			const float Ph = i * 2.f * PI / 3.f;
			FVector P = L;
			FRotator Rot = FRotator::ZeroRotator;
			FVector S(0.08f);
			switch (A)
			{
			case EZoogiArtifact::Storm:
			{
				// Orbiting arcs: three tilted orbits, fast, stretched along the tangent and flickering.
				const float T = Clock * (7.f + i) + Ph;
				const FVector Axis = FRotator(35.f * (i - 1), i * 60.f, 0.f).Vector();
				const FVector U0 = FVector::CrossProduct(Axis, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, 0));
				const FVector U1 = FVector::CrossProduct(Axis, U0);
				const FVector Off = (U0 * FMath::Cos(T) + U1 * FMath::Sin(T)) * 78.f;
				const FVector Tan = (-U0 * FMath::Sin(T) + U1 * FMath::Cos(T));
				P = L + Off;
				Rot = Tan.Rotation();
				const bool bFlick = FMath::Frac(Clock * 9.f + i * 0.37f) < 0.75f;
				S = bFlick ? FVector(0.5f, 0.035f, 0.035f) : FVector(0.01f);
				break;
			}
			case EZoogiArtifact::Void:
			{
				// Two dark orbiting rings, tilted, slowly precessing (flat cylinders drawn as rings).
				P = L;
				Rot = FRotator(62.f * (i == 0 ? 1.f : -1.f), Clock * (i == 0 ? 40.f : -55.f), 0.f);
				const float R = (i == 0 ? 1.65f : 1.95f) * (1.f + 0.15f * PulseK - 0.25f * ImpK);
				S = FVector(R, R, 0.015f);
				break;
			}
			case EZoogiArtifact::Ember:
			{
				// Embers rising around the shell, recycling every 1.2 s.
				const float T = FMath::Frac(Clock * 0.8f + i * 0.21f);
				const float Ang = Ph + Clock * 0.6f + i;
				P = L + FVector(FMath::Cos(Ang) * 62.f, FMath::Sin(Ang) * 62.f, -20.f + 130.f * T);
				S = FVector(0.06f * (1.f - T) + 0.01f);
				break;
			}
			case EZoogiArtifact::Frost:
			{
				// Cold mist motes hugging the floor.
				const float Ang = Ph + Clock * 0.4f + i * 1.3f;
				P = L + FVector(FMath::Cos(Ang) * 80.f, FMath::Sin(Ang) * 80.f, -ZoogiConst::ZoogiRadius + 12.f);
				S = FVector(0.35f, 0.35f, 0.05f);
				break;
			}
			default: break;
			}
			O->SetWorldLocationAndRotation(P, Rot);
			O->SetWorldScale3D(S * (0.2f + 0.8f * Spread));
		}

		// Plates: grow in (spread), Storm veins flicker.
		for (int32 i = 0; i < Plates.Num(); ++i)
		{
			UStaticMeshComponent* P = Plates[i];
			if (!P->IsVisible()) continue;
			FVector S0(0.03f, 0.34f, 0.03f);
			if (P->ComponentTags.Num() > 0)
			{
				TArray<FString> Parts;
				P->ComponentTags[0].ToString().ParseIntoArray(Parts, TEXT(","));
				if (Parts.Num() == 3) S0 = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			}
			// Frost spreads from the bottom up: stagger each plate by its height on the shell.
			const float Z = P->GetRelativeLocation().Z / ZoogiConst::ZoogiRadius; // -1..1 (rolls, but fine for a spread-in)
			const float K = A == EZoogiArtifact::Frost ? FMath::Clamp(Spread * 2.f - (Z + 1.f) * 0.5f, 0.f, 1.f) : Spread;
			float Vis = 1.f;
			if (A == EZoogiArtifact::Storm) Vis = FMath::Frac(Clock * 6.f + i * 0.23f) < 0.5f ? 1.f : 0.05f;
			P->SetRelativeScale3D(S0 * FMath::Max(0.01f, K * Vis) * (1.f + 0.25f * PulseK));
			if (A == EZoogiArtifact::Ember) PlateMIDs[i]->SetScalarParameterValue(TEXT("Strength"), 8.f + 8.f * FMath::Abs(FMath::Sin(Clock * 3.f + i)) + 20.f * ImpK);
		}

		// Aura particles (Niagara) and the movement trail.
		AuraClock += Dt;
		const FZoogiArtifactRow& R = ZoogiArtifacts::Get(A);
		if (AuraClock > (Spd > 300.f ? 0.12f : 0.32f) && !R.AuraNiagara.IsNone())
		{
			AuraClock = 0.f;
			const FVector Jit = FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), A == EZoogiArtifact::Frost || A == EZoogiArtifact::Titan ? -35.f : FMath::FRandRange(0.f, 50.f));
			AZoogiFX::Niagara(GetWorld(), *R.AuraNiagara.ToString(), L + Jit, FRotator::ZeroRotator, A == EZoogiArtifact::Titan ? 0.12f : 0.16f);
		}
		TrailClock += Dt;
		if (Spd > 220.f && TrailClock > 0.045f)
		{
			TrailClock = 0.f;
			const FVector G = L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f);
			const float K = FMath::Clamp(Spd / 2000.f, 0.f, 1.f);
			switch (A)
			{
			case EZoogiArtifact::Ember: AZoogiFX::SpawnTrail(GetWorld(), G, FLinearColor(1.f, 0.4f, 0.05f), 34.f + 26.f * K, 0.7f, 8.f + 6.f * K); break;            // fire trail
			case EZoogiArtifact::Storm: AZoogiFX::SpawnTrail(GetWorld(), G + FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), 0.f), FLinearColor(0.7f, 0.9f, 1.f), 18.f + 20.f * K, 0.3f, 14.f); break; // jagged sparks
			case EZoogiArtifact::Frost: AZoogiFX::SpawnTrail(GetWorld(), G, FLinearColor(0.75f, 0.96f, 1.f), 48.f, 2.2f, 2.5f); break;                               // temporary ice trail (fades ~2 s)
			case EZoogiArtifact::Titan: AZoogiFX::SpawnTrail(GetWorld(), G, FLinearColor(0.55f, 0.42f, 0.28f), 40.f + 20.f * K, 0.9f, 1.5f); break;                 // dust / ground cracks
			case EZoogiArtifact::Void: AZoogiFX::SpawnTrail(GetWorld(), G, FLinearColor(0.42f, 0.1f, 0.85f), 30.f + 24.f * K, 0.8f, 5.f); break;                     // dark smear
			default: break;
			}
		}
	}

	// Guard bubble: low idle shimmer, bright flash when it absorbs a hit.
	if (Guard != EZoogiArtifact::None && GuardShell)
	{
		const float Flash = GuardFlashAge < 0.45f ? (1.f - GuardFlashAge / 0.45f) : 0.f;
		GuardMID->SetScalarParameterValue(TEXT("Strength"), 0.7f + 0.3f * FMath::Sin(Clock * 2.5f) + 9.f * Flash);
		const float S = 1.38f + 0.04f * FMath::Sin(Clock * 2.f) + 0.25f * Flash;
		GuardShell->SetWorldLocation(L);
		GuardShell->SetWorldScale3D(FVector(S));
	}
}
