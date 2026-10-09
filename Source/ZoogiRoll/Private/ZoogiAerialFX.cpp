#include "ZoogiAerialFX.h"
#include "ZoogiTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	const FLinearColor FxCyan(0.25f, 0.9f, 1.f);
	const FLinearColor FxHot(0.9f, 0.98f, 1.f);
	const FLinearColor FxOrange(1.f, 0.5f, 0.1f);

	UStaticMeshComponent* NewFxSeg(AActor* Owner, USceneComponent* Root, UStaticMesh* Mesh, UMaterialInterface* M)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetupAttachment(Root);
		C->RegisterComponent();
		if (M) C->SetMaterial(0, M);
		C->SetVisibility(false);
		return C;
	}
	void Seg(UStaticMeshComponent* C, const FVector& A, const FVector& B, float Thick)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f || Thick < 0.3f) { C->SetVisibility(false); return; }
		C->SetWorldLocationAndRotation((A + B) * 0.5f, D.Rotation());
		C->SetWorldScale3D(FVector(L / 100.f, Thick / 100.f, Thick / 100.f));
		C->SetVisibility(true);
	}
}

// ------------------------------------------------------------------------------------------------ Sky Bumper impact flash

AZoogiImpactFlash::AZoogiImpactFlash()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(1100.f);
	Light->SetCastShadows(false);
	Light->SetLightColor(FxHot);
}

AZoogiImpactFlash* AZoogiImpactFlash::Spawn(UWorld* W, const FVector& At, const FVector& Normal, int32 Seed, float InScale)
{
	if (!W || W->IsNetMode(NM_DedicatedServer)) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiImpactFlash* F = W->SpawnActor<AZoogiImpactFlash>(AZoogiImpactFlash::StaticClass(), At, FRotator::ZeroRotator, P);
	if (F) F->Setup(Normal, Seed, InScale);
	return F;
}

void AZoogiImpactFlash::Setup(const FVector& Normal, int32 Seed, float InScale)
{
	Scale = InScale;
	Origin = GetActorLocation();
	FVector N = Normal.GetSafeNormal();
	if (N.IsNearlyZero()) N = FVector::UpVector;
	FVector T1 = FVector::CrossProduct(N, FVector::UpVector);
	if (T1.SizeSquared() < 0.01f) T1 = FVector::CrossProduct(N, FVector::ForwardVector);
	T1.Normalize();
	const FVector T2 = FVector::CrossProduct(N, T1).GetSafeNormal();
	FRandomStream R(Seed * 7417 + 11); // cosmetic stream (not GameRng)
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	UStaticMesh* Cone = ZoogiUtil::BasicShape(TEXT("Cone"));
	UMaterialInstanceDynamic* Hot = ZoogiUtil::FxMID(this, FxHot, 26.f);
	UMaterialInstanceDynamic* Cy = ZoogiUtil::FxMID(this, FxCyan, 18.f);
	UMaterialInstanceDynamic* Or = ZoogiUtil::FxMID(this, FxOrange, 16.f);
	UMaterialInstanceDynamic* Gl = ZoogiUtil::FxMID(this, FxCyan, 2.4f);
	UMaterialInstanceDynamic* IceM = ZoogiUtil::FxMID(this, FLinearColor(0.7f, 0.92f, 1.f), 3.5f);
	// 16 star-burst rays: mostly in the plane across the normal (a flat burst facing out), some along it
	for (int32 i = 0; i < 16; ++i)
	{
		const float A = (i + R.FRandRange(-0.3f, 0.3f)) * 2.f * PI / 16.f;
		const FVector InPlane = T1 * FMath::Cos(A) + T2 * FMath::Sin(A);
		FRay Ray;
		Ray.Dir = (InPlane + N * R.FRandRange(0.05f, 0.7f)).GetSafeNormal();
		Ray.Len = R.FRandRange(90.f, 230.f) * ((i % 4 == 0) ? 1.35f : 1.f);
		Ray.Thick = (i % 4 == 0) ? 13.f : R.FRandRange(6.f, 9.f);
		const FVector Side = FVector::CrossProduct(Ray.Dir, N).GetSafeNormal();
		Ray.Kink1 = Side * R.FRandRange(-0.12f, 0.12f) + N * R.FRandRange(-0.05f, 0.05f);
		Ray.Kink2 = Side * R.FRandRange(-0.10f, 0.10f);
		RayDefs.Add(Ray);
		UMaterialInterface* M = (i % 3 == 0) ? (UMaterialInterface*)Or : ((i % 3 == 1) ? (UMaterialInterface*)Hot : (UMaterialInterface*)Cy);
		for (int32 s = 0; s < 3; ++s) { Rays.Add(NewFxSeg(this, RootComponent, Cube, M)); Glows.Add(NewFxSeg(this, RootComponent, Cube, Gl)); }
	}
	// 10 ice shards (thin cones) flying out of the burst
	for (int32 i = 0; i < 10; ++i)
	{
		FShard S;
		const float A = R.FRandRange(0.f, 2.f * PI);
		S.Vel = ((T1 * FMath::Cos(A) + T2 * FMath::Sin(A)) * R.FRandRange(0.6f, 1.f) + N * R.FRandRange(0.2f, 0.9f)).GetSafeNormal() * R.FRandRange(520.f, 900.f);
		S.Axis = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)).GetSafeNormal();
		S.Spin = R.FRandRange(-720.f, 720.f);
		S.Len = R.FRandRange(14.f, 30.f);
		ShardDefs.Add(S);
		Shards.Add(NewFxSeg(this, RootComponent, Cone, IceM));
	}
	ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.55f, 1.7f);
	Place(1.f);
}

void AZoogiImpactFlash::Place(float K)
{
	const float Grow = FMath::Clamp(Age / 0.07f, 0.f, 1.f);         // rays shoot out in 70 ms
	const float Fade = FMath::Clamp(1.f - Age / Life, 0.f, 1.f);
	for (int32 i = 0; i < RayDefs.Num(); ++i)
	{
		const FRay& Ry = RayDefs[i];
		const float L = Ry.Len * Scale * (0.35f + 0.65f * Grow) * (0.7f + 0.3f * Fade);
		const FVector A = Origin + Ry.Dir * 14.f * Scale;
		const FVector P1 = A + (Ry.Dir + Ry.Kink1) .GetSafeNormal() * L * 0.38f;
		const FVector P2 = P1 + (Ry.Dir + Ry.Kink2).GetSafeNormal() * L * 0.34f;
		const FVector P3 = P2 + Ry.Dir * L * 0.28f;
		const FVector Pts[4] = { A, P1, P2, P3 };
		for (int32 s = 0; s < 3; ++s)
		{
			const float Taper = Ry.Thick * Scale * Fade * (1.f - 0.3f * s);
			Seg(Rays[i * 3 + s], Pts[s], Pts[s + 1], Taper);
			Seg(Glows[i * 3 + s], Pts[s], Pts[s + 1], Taper * 3.f);
		}
	}
	for (int32 i = 0; i < ShardDefs.Num(); ++i)
	{
		const FShard& S = ShardDefs[i];
		UStaticMeshComponent* C = Shards[i];
		const FVector P = Origin + S.Vel * Age + FVector(0, 0, -490.f) * Age * Age;
		const FQuat Q = FQuat(S.Axis, FMath::DegreesToRadians(S.Spin * Age)) * FRotationMatrix::MakeFromZ(S.Vel).ToQuat();
		C->SetWorldLocationAndRotation(P, Q);
		const float Sz = S.Len * Scale * FMath::Clamp(Fade * 1.4f, 0.f, 1.f) / 100.f;
		C->SetWorldScale3D(FVector(Sz * 0.28f, Sz * 0.12f, Sz));
		C->SetVisibility(Sz > 0.01f);
	}
	Light->SetIntensity(Fade * Fade * 160000.f * Scale);
}

void AZoogiImpactFlash::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	if (Age >= Life) { Destroy(); return; }
	Place(1.f);
}

// ------------------------------------------------------------------------------------------------ Air Reaction reticle

AZoogiAirReticle::AZoogiAirReticle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the camera / ball moved this frame
	PrimaryActorTick.bTickEvenWhenPaused = true;    // the Air Reaction offer holds (pauses) the game
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(450.f);
	Light->SetCastShadows(false);
	Light->SetLightColor(FxCyan);
}

void AZoogiAirReticle::Follow(AActor* InTarget, float InRadius)
{
	Target = InTarget;
	Radius = InRadius;
	bActive = true;
	if (Segs.Num() == 0)
	{
		UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
		MatRing = ZoogiUtil::FxMID(this, FxCyan, 10.f);
		MatHot = ZoogiUtil::FxMID(this, FxHot, 14.f);
		// 48 outer ring + 24 dashed inner + 16 ticks + 8 brackets
		for (int32 i = 0; i < 96; ++i) Segs.Add(NewFxSeg(this, RootComponent, Cube, (i >= 88 || (i >= 72 && i % 4 == 0)) ? MatHot : MatRing));
	}
}

void AZoogiAirReticle::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	Alpha = FMath::Clamp(Alpha + (bActive ? Dt / 0.18f : -Dt / 0.15f), 0.f, 1.f);
	AActor* T = Target.Get();
	if (!T || (!bActive && Alpha <= 0.f)) { Destroy(); return; }
	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(T->GetRootComponent());
	const FVector C = Prim ? Prim->GetComponentLocation() : T->GetActorLocation();
	SetActorLocation(C);
	// face the camera
	FVector Cam = C + FVector(0, -1000, 600);
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(this, 0)) Cam = PCM->GetCameraLocation();
	const FVector V = (Cam - C).GetSafeNormal();
	FVector X = FVector::CrossProduct(FVector::UpVector, V);
	if (X.SizeSquared() < 0.01f) X = FVector::ForwardVector;
	X.Normalize();
	const FVector Y = FVector::CrossProduct(V, X).GetSafeNormal();
	const FVector O = C + V * (Radius * 0.6f); // a little toward the camera so the ball never hides the ring
	const float Pulse = 1.f + 0.06f * FMath::Sin(Age * 9.f);
	const float In = 1.f + 0.6f * (1.f - Alpha);        // snaps in from wider
	const float R0 = Radius * 2.1f * Pulse * In, R1 = Radius * 1.62f * In;
	const float Th = 3.2f * Alpha;
	auto P = [&](float R, float A) { return O + (X * FMath::Cos(A) + Y * FMath::Sin(A)) * R; };
	int32 S = 0;
	const float Spin = Age * 0.9f, Counter = -Age * 1.6f;
	for (int32 i = 0; i < 48; ++i) { const float A = i * 2.f * PI / 48.f; Seg(Segs[S++], P(R0, A), P(R0, A + 2.f * PI / 48.f), Th); }
	for (int32 i = 0; i < 24; ++i) { const float A = Counter + i * 2.f * PI / 24.f; Seg(Segs[S++], P(R1, A), P(R1, A + PI / 24.f * 0.9f), Th * 0.8f); }
	for (int32 i = 0; i < 16; ++i)
	{
		const float A = Spin + i * 2.f * PI / 16.f;
		const float L = (i % 4 == 0) ? Radius * 0.42f : Radius * 0.22f;
		Seg(Segs[S++], P(R0 + 4.f, A), P(R0 + 4.f + L, A), (i % 4 == 0) ? Th * 1.5f : Th);
	}
	// 4 corner brackets (two strokes each) outside the ring
	const float B = R0 * 1.22f, Arm = Radius * 0.55f;
	for (int32 k = 0; k < 4; ++k)
	{
		const float A = PI * 0.25f + k * PI * 0.5f;
		const FVector Corner = O + (X * FMath::Cos(A) * B + Y * FMath::Sin(A) * B);
		const FVector Dx = X * (FMath::Cos(A) > 0.f ? -1.f : 1.f) * Arm, Dy = Y * (FMath::Sin(A) > 0.f ? -1.f : 1.f) * Arm;
		Seg(Segs[S++], Corner, Corner + Dx, Th * 1.4f);
		Seg(Segs[S++], Corner, Corner + Dy, Th * 1.4f);
	}
	for (; S < Segs.Num(); ++S) Segs[S]->SetVisibility(false);
	Light->SetIntensity(Alpha * 9000.f);
}
