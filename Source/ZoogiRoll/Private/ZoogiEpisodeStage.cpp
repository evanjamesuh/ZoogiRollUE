#include "ZoogiEpisodeStage.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/KismetMathLibrary.h"

namespace
{
	const TCHAR* BoulderPath = TEXT("/Game/Env/Incoming/tomb/SM_Boulder_A/SM_Boulder_A/StaticMeshes/SM_Boulder_A.SM_Boulder_A");

	UStaticMesh* RockMesh()
	{
		UStaticMesh* M = ZoogiUtil::LoadMesh(BoulderPath);
		return M ? M : ZoogiUtil::BasicShape(TEXT("Sphere"));
	}

	UStaticMeshComponent* NewMesh(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Mat)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->CastShadow = false;
		if (Mat) for (int32 i = 0; i < FMath::Max(1, C->GetNumMaterials()); ++i) C->SetMaterial(i, Mat);
		C->SetupAttachment(Parent);
		C->RegisterComponent();
		C->SetVisibility(false);
		return C;
	}

	float Smooth(float A, float B, float T) { return FMath::SmoothStep(A, B, T); }
}

AZoogiEpisodeStage::AZoogiEpisodeStage()
{
	PrimaryActorTick.bCanEverTick = false; // driven by the subsystem (real time, also while paused)
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

// ------------------------------------------------------------------------------------------------------------- panels

void AZoogiEpisodeStage::EnsureCaptures(int32 Num, int32 Res)
{
	while (Captures.Num() < Num)
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor::Black;
		RT->InitAutoFormat(Res, FMath::RoundToInt(Res * 9.f / 16.f));
		RT->UpdateResourceImmediate(true);
		USceneCaptureComponent2D* C = NewObject<USceneCaptureComponent2D>(this);
		C->SetupAttachment(Root);
		C->TextureTarget = RT;
		C->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->bAlwaysPersistRenderingState = true;
		C->FOVAngle = 55.f;
		C->ShowFlags.SetMotionBlur(false);
		C->RegisterComponent();
		Captures.Add(C);
		Targets.Add(RT);
	}
}

void AZoogiEpisodeStage::SetCaptureView(int32 Index, const FVector& Location, const FVector& LookAt, float FOV)
{
	if (!Captures.IsValidIndex(Index)) return;
	Captures[Index]->SetWorldLocationAndRotation(Location, UKismetMathLibrary::FindLookAtRotation(Location, LookAt));
	Captures[Index]->FOVAngle = FOV;
}

void AZoogiEpisodeStage::CaptureAll(int32 Num)
{
	for (int32 i = 0; i < FMath::Min(Num, Captures.Num()); ++i) Captures[i]->CaptureScene();
}

TArray<UTextureRenderTarget2D*> AZoogiEpisodeStage::GetTargets(int32 Num) const
{
	TArray<UTextureRenderTarget2D*> Out;
	for (int32 i = 0; i < FMath::Min(Num, Targets.Num()); ++i) Out.Add(Targets[i]);
	return Out;
}

// --------------------------------------------------------------------------------------------------------------- glow

UProceduralMeshComponent* AZoogiEpisodeStage::MakeRing(float Inner, float Outer, int32 Segments)
{
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	TArray<FVector> V; TArray<int32> Tri; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<FProcMeshTangent> Tan;
	for (int32 i = 0; i <= Segments; ++i)
	{
		const float A = 2.f * PI * i / Segments;
		const FVector D(FMath::Cos(A), FMath::Sin(A), 0.f);
		V.Add(D * Inner); V.Add(D * Outer);
		N.Add(FVector::UpVector); N.Add(FVector::UpVector);
		UV.Add(FVector2D((float)i / Segments, 0.f)); UV.Add(FVector2D((float)i / Segments, 1.f));
		if (i < Segments)
		{
			const int32 B = i * 2;
			Tri.Append({ B, B + 2, B + 1, B + 1, B + 2, B + 3 });
			Tri.Append({ B, B + 1, B + 2, B + 1, B + 3, B + 2 }); // two-sided
		}
	}
	P->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Col, Tan, false);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->CastShadow = false;
	P->SetupAttachment(Root);
	P->RegisterComponent();
	return P;
}

void AZoogiEpisodeStage::GlowSurface(UPrimitiveComponent* Surface, const FVector& At, const FVector& Normal, const FLinearColor& Color)
{
	for (const FGlow& G : Glows) if (G.Comp.IsValid() && G.Comp->GetAttachParent() == Surface) return;
	TArray<UStaticMeshComponent*> Parts;
	// A destructible piece glows on its visible parts; a plain visible wall mesh glows itself if it is wall-sized.
	if (Surface)
	{
		AActor* SurfOwner = Surface->GetOwner();
		UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(Surface);
		if (SM && SM->IsVisible() && SM->GetStaticMesh() && SM->Bounds.SphereRadius < 900.f) Parts.Add(SM);
		else if (SurfOwner && SurfOwner->GetClass()->GetName().Contains(TEXT("ArenaPiece")))
		{
			TArray<UStaticMeshComponent*> All;
			SurfOwner->GetComponents(All);
			for (UStaticMeshComponent* C : All)
				if (C && C->IsVisible() && C->GetStaticMesh() && C->Bounds.SphereRadius < 900.f && FVector::Dist(C->Bounds.Origin, At) < C->Bounds.SphereRadius + 250.f && Parts.Num() < 4) Parts.Add(C);
		}
	}
	for (UStaticMeshComponent* Src : Parts)
	{
		UMaterialInstanceDynamic* MID = ZoogiUtil::FxMID(this, Color, 6.f);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Src->GetStaticMesh());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		for (int32 i = 0; i < FMath::Max(1, C->GetNumMaterials()); ++i) C->SetMaterial(i, MID);
		C->SetupAttachment(Src);
		C->RegisterComponent();
		C->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(1.025f)));
		FGlow G; G.Comp = C; G.MID = MID; G.Life = 0.95f; G.Strength = 5.f;
		Glows.Add(G);
		GlowComps.Add(C);
		GlowMIDs.Add(MID);
	}
	if (Parts.Num() == 0)
	{
		// Invisible collider (rim, bumper volume): a glow ring on the contact point, facing out of the surface.
		UProceduralMeshComponent* R = MakeRing(70.f, 100.f, 40);
		UMaterialInstanceDynamic* MID = ZoogiUtil::FxMID(this, Color, 8.f);
		R->SetMaterial(0, MID);
		const FVector Nn = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		R->SetWorldLocationAndRotation(At + Nn * 3.f, FRotationMatrix::MakeFromZ(Nn).Rotator());
		FGlow G; G.Comp = R; G.MID = MID; G.Life = 0.7f; G.Strength = 8.f; G.bRing = true;
		Glows.Add(G);
		GlowComps.Add(R);
		GlowMIDs.Add(MID);
	}
}

void AZoogiEpisodeStage::TickStage(float Dt)
{
	for (int32 i = Glows.Num() - 1; i >= 0; --i)
	{
		FGlow& G = Glows[i];
		G.Age += Dt;
		UPrimitiveComponent* C = G.Comp.Get();
		if (!C || G.Age >= G.Life)
		{
			if (C) { GlowComps.Remove(C); C->DestroyComponent(); }
			if (G.MID.IsValid()) GlowMIDs.Remove(G.MID.Get());
			Glows.RemoveAt(i);
			continue;
		}
		const float T = G.Age / G.Life;
		// two pulses, fading
		const float Pulse = FMath::Abs(FMath::Sin(T * PI * 2.f)) * (1.f - T);
		if (G.MID.IsValid()) G.MID->SetScalarParameterValue(TEXT("Strength"), G.Strength * Pulse);
		if (G.bRing) C->SetWorldScale3D(FVector(FMath::Lerp(0.6f, 2.2f, T)));
	}
}

// ------------------------------------------------------------------------------------------------------------- meteor

void AZoogiEpisodeStage::BuildMeteorParts()
{
	if (Rock) return;
	UStaticMesh* RM = RockMesh();
	Rock = NewMesh(this, Root, RM, ZoogiUtil::SurfaceMID(this, FLinearColor(0.035f, 0.03f, 0.028f), 0.95f, FLinearColor(1.f, 0.55f, 0.2f), 0.15f));
	HeatMID = ZoogiUtil::FxMID(this, FLinearColor(1.f, 0.5f, 0.15f), 0.f);
	RockHeat = NewMesh(this, Rock, RM, HeatMID);
	RockHeat->SetRelativeScale3D(FVector(1.06f));
	for (int32 i = 0; i < 12; ++i)
	{
		UStaticMeshComponent* P = NewMesh(this, Root, RM, ZoogiUtil::SurfaceMID(this, FLinearColor(0.23f, 0.19f, 0.15f) * (0.8f + 0.04f * i), 0.9f));
		Pebbles.Add(P);
	}
	UMaterialInterface* DustMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_ZoogiDust.M_ZoogiDust"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (int32 i = 0; i < 10; ++i)
	{
		UMaterialInstanceDynamic* MID = DustMat ? UMaterialInstanceDynamic::Create(DustMat, this) : ZoogiUtil::SurfaceMID(this, FLinearColor(0.5f, 0.42f, 0.32f), 1.f);
		if (DustMat) { MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.47f, 0.36f)); MID->SetScalarParameterValue(TEXT("Opacity"), 0.f); }
		Dust.Add(NewMesh(this, Root, ZoogiUtil::BasicShape(TEXT("Sphere")), MID));
		DustMIDs.Add(MID);
	}
	Shock = MakeRing(88.f, 100.f, 64);
	ShockMID = ZoogiUtil::FxMID(this, FLinearColor(1.f, 0.65f, 0.3f), 0.f);
	Shock->SetMaterial(0, ShockMID);
	Shock->SetVisibility(false);
	ImpactLight = NewObject<UPointLightComponent>(this);
	ImpactLight->SetupAttachment(Root);
	ImpactLight->SetAttenuationRadius(2600.f);
	ImpactLight->SetIntensity(0.f);
	ImpactLight->SetCastShadows(false);
	ImpactLight->RegisterComponent();
	Post = NewObject<UPostProcessComponent>(this);
	Post->bUnbound = true;
	Post->Priority = 50.f;
	Post->BlendWeight = 0.f;
	Post->Settings.bOverride_AutoExposureBias = true;
	Post->Settings.AutoExposureBias = -1.6f;
	Post->Settings.bOverride_ColorSaturation = true;
	Post->Settings.ColorSaturation = FVector4(0.72f, 0.72f, 0.72f, 1.f);
	Post->Settings.bOverride_VignetteIntensity = true;
	Post->Settings.VignetteIntensity = 0.9f;
	Post->SetupAttachment(Root);
	Post->RegisterComponent();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Cam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP);
	if (Cam)
	{
		Cam->GetCameraComponent()->SetFieldOfView(62.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
}

void AZoogiEpisodeStage::BeginMeteor(const FVector& Target, const FVector& Shooter, const FLinearColor& Color)
{
	BuildMeteorParts();
	bMeteor = true;
	bImpacted = false;
	MTarget = Target;
	MFloor = Target - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - 2.f);
	ShooterAt = Shooter;
	MColor = Color;
	FVector2D Away = FVector2D(Target - Shooter).GetSafeNormal();
	if (Away.IsNearlyZero()) Away = FVector2D(1.f, 0.f);
	// The rock comes in high from behind the camera side, a steep perspective drop onto the target.
	const FVector Side = FVector(-Away.Y, Away.X, 0.f);
	MFrom = MFloor + FVector(Away, 0.f) * 900.f + Side * 500.f + FVector(0.f, 0.f, 4200.f);
	MCamBase = MFloor - FVector(Away, 0.f) * 900.f + Side * 520.f + FVector(0.f, 0.f, 260.f);
	const float Diameter = 520.f;
	if (Rock && Rock->GetStaticMesh())
	{
		const float MeshD = FMath::Max(1.f, Rock->GetStaticMesh()->GetBounds().SphereRadius * 2.f);
		RockScale = Diameter / MeshD;
	}
	FRandomStream Rng(1234);
	PebbleBase.Reset();
	for (int32 i = 0; i < Pebbles.Num(); ++i)
	{
		const float A = 2.f * PI * i / Pebbles.Num() + Rng.FRandRange(-0.2f, 0.2f);
		const float R = Rng.FRandRange(130.f, 380.f);
		PebbleBase.Add(MFloor + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f));
		const float MeshD = FMath::Max(1.f, Pebbles[i]->GetStaticMesh()->GetBounds().SphereRadius * 2.f);
		Pebbles[i]->SetWorldScale3D(FVector(Rng.FRandRange(14.f, 34.f) / MeshD));
		Pebbles[i]->SetWorldRotation(FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f));
	}
	DustBase.Reset();
	for (int32 i = 0; i < Dust.Num(); ++i)
	{
		const float A = 2.f * PI * i / Dust.Num() + 0.3f;
		const float R = Rng.FRandRange(90.f, 260.f);
		DustBase.Add(MFloor + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 10.f));
	}
	if (Cam) Cam->SetActorLocationAndRotation(MCamBase, UKismetMathLibrary::FindLookAtRotation(MCamBase, MTarget + FVector(0, 0, 150.f)));
	if (ShockMID) ShockMID->SetVectorParameterValue(TEXT("Color"), Color);
	if (HeatMID) HeatMID->SetVectorParameterValue(TEXT("Color"), Color);
	if (ImpactLight) ImpactLight->SetLightColor(Color);
	UpdateMeteor(0.f);
}

FVector AZoogiEpisodeStage::GetMeteorRockLocation() const { return Rock ? Rock->GetComponentLocation() : MTarget; }

bool AZoogiEpisodeStage::UpdateMeteor(float T)
{
	if (!bMeteor) return false;
	T = FMath::Clamp(T, 0.f, 1.f);
	// Timeline (normalized): 0-0.15 darken + cut, 0.08-0.6 dust / pebbles lift, 0.32-0.66 rock drop, 0.66 impact,
	// 0.66-0.9 shockwave, 0.8-1 everything returns to the real scene.
	const float ImpactT = 0.66f;
	const float Dark = Smooth(0.f, 0.14f, T) * (1.f - Smooth(0.8f, 1.f, T));
	if (Post) Post->BlendWeight = Dark;
	// camera: slow push in, tilting up toward the sky while the rock comes, shake at impact
	if (Cam)
	{
		const FVector ToT = (MTarget - MCamBase).GetSafeNormal();
		FVector L = MCamBase + ToT * 160.f * Smooth(0.f, 0.66f, T);
		const float Up = Smooth(0.2f, 0.45f, T) * (1.f - Smooth(0.55f, 0.66f, T));
		FVector Look = MTarget + FVector(0, 0, 150.f + 1400.f * Up);
		if (T > ImpactT && T < ImpactT + 0.12f)
		{
			const float K = 1.f - (T - ImpactT) / 0.12f;
			L += FVector(FMath::Sin(T * 300.f), FMath::Cos(T * 260.f), FMath::Sin(T * 340.f)) * 22.f * K;
		}
		Cam->SetActorLocationAndRotation(L, UKismetMathLibrary::FindLookAtRotation(L, Look));
	}
	// pebbles and dust lift off the floor around the target (the Titan's pull), drop at impact
	const float Lift = Smooth(0.08f, 0.55f, T) * (1.f - Smooth(ImpactT, ImpactT + 0.05f, T));
	for (int32 i = 0; i < Pebbles.Num(); ++i)
	{
		const bool bVis = T > 0.06f && T < 0.95f;
		Pebbles[i]->SetVisibility(bVis);
		FVector P = PebbleBase[i] + FVector(0, 0, 8.f + Lift * (40.f + 9.f * i));
		if (T > ImpactT) P += (PebbleBase[i] - MFloor).GetSafeNormal() * 600.f * (T - ImpactT) + FVector(0, 0, 400.f * (T - ImpactT));
		Pebbles[i]->SetWorldLocation(P);
		Pebbles[i]->AddLocalRotation(FRotator(1.5f, 2.f, 0.f));
	}
	for (int32 i = 0; i < Dust.Num(); ++i)
	{
		const float Grow = Smooth(0.05f, 0.6f, T);
		const float Burst = Smooth(ImpactT, 0.95f, T);
		const bool bVis = T > 0.04f && T < 0.99f;
		Dust[i]->SetVisibility(bVis);
		const FVector Out = (DustBase[i] - MFloor).GetSafeNormal2D();
		Dust[i]->SetWorldLocation(DustBase[i] + FVector(0, 0, 30.f * Grow) + Out * (60.f * Grow + 700.f * Burst));
		Dust[i]->SetWorldScale3D(FVector(0.9f + 1.6f * Grow + 2.5f * Burst, 0.9f + 1.6f * Grow + 2.5f * Burst, 0.5f + 0.8f * Grow));
		if (DustMIDs.IsValidIndex(i) && DustMIDs[i]) DustMIDs[i]->SetScalarParameterValue(TEXT("Opacity"), 0.45f * Smooth(0.04f, 0.3f, T) * (1.f - Smooth(0.75f, 0.99f, T)));
	}
	// the rock: perception-space silhouette falling onto the target (no collision, nothing physical)
	bool bImpactNow = false;
	if (Rock)
	{
		const float D = FMath::Clamp((T - 0.32f) / (ImpactT - 0.32f), 0.f, 1.f);
		const bool bVis = T >= 0.3f && T < ImpactT + 0.05f;
		Rock->SetVisibility(bVis);
		RockHeat->SetVisibility(bVis);
		const float Fall = D * D; // accelerating
		const FVector P = FMath::Lerp(MFrom, MFloor + FVector(0, 0, 150.f), Fall);
		Rock->SetWorldLocation(P);
		Rock->SetWorldRotation(FRotator(25.f * D, 40.f + 70.f * D, 15.f * D));
		Rock->SetWorldScale3D(FVector(RockScale * (T > ImpactT ? FMath::Lerp(1.f, 0.6f, (T - ImpactT) / 0.05f) : 1.f)));
		if (HeatMID) HeatMID->SetScalarParameterValue(TEXT("Strength"), 2.f + 10.f * D);
		if (!bImpacted && T >= ImpactT) { bImpacted = true; bImpactNow = true; }
	}
	// impact light + shockwave ring along the floor
	if (ImpactLight)
	{
		ImpactLight->SetWorldLocation(MFloor + FVector(0, 0, 180.f));
		const float L = T >= ImpactT ? FMath::Max(0.f, 1.f - (T - ImpactT) / 0.18f) : 0.f;
		ImpactLight->SetIntensity(400000.f * L);
	}
	if (Shock)
	{
		const bool bVis = T >= ImpactT && T < 0.92f;
		Shock->SetVisibility(bVis);
		if (bVis)
		{
			const float S = (T - ImpactT) / (0.92f - ImpactT);
			Shock->SetWorldLocation(MFloor + FVector(0, 0, 4.f));
			Shock->SetWorldRotation(FRotator::ZeroRotator);
			Shock->SetWorldScale3D(FVector(FMath::Lerp(0.8f, 11.f, FMath::Sqrt(S)), FMath::Lerp(0.8f, 11.f, FMath::Sqrt(S)), 1.f));
			if (ShockMID) ShockMID->SetScalarParameterValue(TEXT("Strength"), 12.f * (1.f - S));
		}
	}
	return bImpactNow;
}

void AZoogiEpisodeStage::EndMeteor()
{
	bMeteor = false;
	if (Post) Post->BlendWeight = 0.f;
	if (Rock) { Rock->SetVisibility(false); RockHeat->SetVisibility(false); }
	for (UStaticMeshComponent* P : Pebbles) if (P) P->SetVisibility(false);
	for (UStaticMeshComponent* D : Dust) if (D) D->SetVisibility(false);
	if (Shock) Shock->SetVisibility(false);
	if (ImpactLight) ImpactLight->SetIntensity(0.f);
}
