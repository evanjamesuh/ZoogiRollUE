#include "ZoogiRock.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.h"
#include "ZoogiArtifactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"

AZoogiRock::AZoogiRock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while chunks fly
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* AZoogiRock::MakePart(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(bShadow);
	if (Mat) C->SetMaterial(0, Mat);
	C->RegisterComponent();
	return C;
}

void AZoogiRock::InitRock(float InRadius, float InHeight, float InYaw, const FLinearColor& ColA, const FLinearColor& ColB, UPhysicalMaterial* PM, int32 InSeed)
{
	Radius = InRadius; Height = InHeight; Yaw = InYaw; Seed = InSeed;
	UStaticMesh* Sphere = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	MatIntact = ZoogiUtil::GroundMID(this, ColA, ColB, 0.02f, 0.85f);
	MatCracked = ZoogiUtil::GroundMID(this, ColA * 0.62f, ColB * 0.5f, 0.03f, 0.9f);
	UMaterialInterface* CrackMat = ZoogiUtil::SurfaceMID(this, FLinearColor(0.02f, 0.015f, 0.01f), 0.95f);

	// Solid collider: invisible cylinder with the footprint (same as the old static rim boulders).
	Collider = NewObject<UStaticMeshComponent>(this);
	Collider->SetStaticMesh(Cyl);
	Collider->SetMobility(EComponentMobility::Movable);
	Collider->SetupAttachment(Root);
	Collider->SetCollisionProfileName(TEXT("BlockAll"));
	Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collider->SetVisibility(false);
	Collider->SetCastShadow(false);
	if (PM) Collider->SetPhysMaterialOverride(PM);
	Collider->SetRelativeScale3D(FVector(Radius * 2.f / 100.f, Radius * 2.f / 100.f, Height / 100.f));
	Collider->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Collider->RegisterComponent();

	// Visual: a squashed boulder sunk into the turf, plus a smaller one beside it (pops off as the first chip).
	Body = MakePart(Sphere, MatIntact, true);
	BodyScale = FVector(Radius * 2.f / 100.f, Radius * 1.8f / 100.f, Height * 2.f / 100.f);
	Body->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	Side = MakePart(Sphere, MatIntact, true);
	Side->SetRelativeScale3D(FVector(Radius * 0.9f / 100.f, Radius * 0.8f / 100.f, Height * 1.1f / 100.f));
	const float YR = FMath::DegreesToRadians(Yaw);
	Side->SetRelativeLocation(FVector(-FMath::Sin(YR), FMath::Cos(YR), 0.f) * Radius * 0.85f);

	// Crack lines: thin dark slivers lying on the upper surface of the ellipsoid (hidden until cracked).
	FRandomStream Rng(Seed);
	const FVector Semi(Radius, Radius * 0.9f, Height); // ellipsoid semi-axes (before yaw)
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* L = MakePart(Cube, CrackMat, false);
		const float Az = Rng.FRandRange(0.f, 2.f * PI), El = FMath::DegreesToRadians(Rng.FRandRange(25.f, 60.f));
		FVector Pn(FMath::Cos(El) * FMath::Cos(Az) * Semi.X, FMath::Cos(El) * FMath::Sin(Az) * Semi.Y, FMath::Sin(El) * Semi.Z);
		FVector N(Pn.X / (Semi.X * Semi.X), Pn.Y / (Semi.Y * Semi.Y), Pn.Z / (Semi.Z * Semi.Z));
		N.Normalize();
		const FVector Tan = FVector::CrossProduct(N, FVector(FMath::Sin(Az), -FMath::Cos(Az), 0.f).RotateAngleAxis(Rng.FRandRange(-40.f, 40.f), N)).GetSafeNormal();
		const FRotator Rot = FRotationMatrix::MakeFromXZ(Tan, N).Rotator();
		L->SetRelativeLocation(FRotator(0.f, Yaw, 0.f).RotateVector(Pn + N * 1.5f));
		L->SetRelativeRotation(FRotator(0.f, Yaw, 0.f).Quaternion() * Rot.Quaternion());
		L->SetRelativeScale3D(FVector(Radius * Rng.FRandRange(0.55f, 0.9f) / 100.f, 0.045f, 0.03f));
		CrackLines.Add(L);
	}
	// Rubble left after shattering: low flat pebbles, no collision, so a Zoogi rolls straight over the spot.
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* R = MakePart(i % 2 ? Cube : Sphere, MatCracked, false);
		const float A = Rng.FRandRange(0.f, 2.f * PI), D = Rng.FRandRange(0.f, Radius * 0.7f);
		R->SetRelativeLocation(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 2.f));
		R->SetRelativeRotation(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
		const float S = Radius * Rng.FRandRange(0.25f, 0.45f) / 100.f;
		R->SetRelativeScale3D(FVector(S, S * Rng.FRandRange(0.6f, 1.f), 0.08f));
		Rubble.Add(R);
	}
	// Chunk pool: the shatter chunks plus three chips for the crack (one draw call each, 11 per rock).
	for (int32 i = 0; i < ChunkCount + 3; ++i)
	{
		UStaticMeshComponent* C = MakePart(i % 3 == 0 ? Sphere : Cube, MatCracked, true);
		C->SetVisibility(false);
		ChunkPool.Add(C);
	}
	Chunks.SetNum(ChunkPool.Num());
	ResetRock();
}

void AZoogiRock::ResetRock()
{
	State = EZoogiRockState::Intact;
	LastHitTime = -10.f;
	NextChunk = 0;
	if (Collider) Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	if (Body) { Body->SetVisibility(true); Body->SetMaterial(0, MatIntact); Body->SetRelativeScale3D(BodyScale); }
	if (Side) { Side->SetVisibility(true); Side->SetMaterial(0, MatIntact); }
	for (UStaticMeshComponent* L : CrackLines) if (L) L->SetVisibility(false);
	for (UStaticMeshComponent* R : Rubble) if (R) R->SetVisibility(false);
	for (int32 i = 0; i < Chunks.Num(); ++i) { Chunks[i].bLive = false; if (ChunkPool.IsValidIndex(i) && ChunkPool[i]) ChunkPool[i]->SetVisibility(false); }
	SetActorTickEnabled(false);
}

void AZoogiRock::OnBallHit(AZoogiBall* Ball, const FVector& NormalImpulse, const FVector& ImpactPoint)
{
	if (!Ball || State == EZoogiRockState::Shattered) return;
	UWorld* W = GetWorld();
	const float Now = W ? W->GetTimeSeconds() : 0.f;
	if (Now - LastHitTime < 0.3f) return; // one contact reports for several frames
	// Contact speed: the ball's approach velocity (sampled before this frame's physics step) along the contact normal.
	// (The reported Chaos impulse proved unreliable as a speed measure: it read ~3x the launch speed.)
	FVector N2 = (GetActorLocation() - Ball->BallLocation()).GetSafeNormal2D();
	if (N2.IsNearlyZero()) N2 = -NormalImpulse.GetSafeNormal2D();
	const FVector V = Ball->PrevVelocity.SizeSquared() > 1.f ? Ball->PrevVelocity : Ball->GetBallVelocity();
	const float Vn = FMath::Max(0.f, FVector::DotProduct(FVector(V.X, V.Y, 0.f), N2));
	const float CutDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Vn / FMath::Max(1.f, V.Size2D()), -1.f, 1.f)));
	if (Vn < CrackSpeed) return;
	LastHitTime = Now;
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	// Strong hit: the active shooter while Awakened (infused), or crit-grade contact (>= 90% of its launch speed, the crit rule).
	bool bStrong = false;
	if (G && Ball->PlayerIndex == G->ActivePlayer && G->Phase == EZoogiPhase::Rolling)
	{
		const bool bAwakened = Ball->Artifact && Ball->Artifact->Infused != EZoogiArtifact::None;
		// Crit-grade = the crit rule (>= 90% of its launch speed at contact, <= 5 deg cut) on a hard hit; a soft straight tap only cracks.
		const bool bCritGrade = G->GetShotSpeed() > 1.f && V.Size2D() >= 0.9f * G->GetShotSpeed() && CutDeg <= 5.f && Vn >= ShatterSpeed;
		bStrong = bAwakened || bCritGrade;
	}
	FVector Dir = -NormalImpulse.GetSafeNormal2D(); // impulse pushes the ball away from the rock: debris flies the other way
	if (Dir.IsNearlyZero()) Dir = (GetActorLocation() - Ball->BallLocation()).GetSafeNormal2D();
	UE_LOG(LogZoogi, Log, TEXT("Rock %s hit by P%d %s at %.0f cm/s normal (%.0f cm/s, cut %.0f deg)%s (state %d)"), *GetName(), Ball->PlayerIndex + 1, *Ball->Def.Name, Vn, V.Size2D(), CutDeg, bStrong ? TEXT(" STRONG") : TEXT(""), (int32)State);
	if (bStrong || (State == EZoogiRockState::Cracked && Vn >= ShatterSpeed)) Shatter(ImpactPoint, Dir, Ball);
	else if (State == EZoogiRockState::Intact) Crack(ImpactPoint, Dir, Ball);
}

void AZoogiRock::Crack(const FVector& P, const FVector& Dir, AZoogiBall* By)
{
	State = EZoogiRockState::Cracked;
	Body->SetMaterial(0, MatCracked);
	Body->SetRelativeScale3D(BodyScale * FVector(0.95f, 0.95f, 0.93f));
	Side->SetVisibility(false); // the side stone breaks off as chips
	for (UStaticMeshComponent* L : CrackLines) L->SetVisibility(true);
	Launch(3, Side->GetComponentLocation() + FVector(0, 0, Height * 0.4f), Dir, 380.f, 0.55f);
	AZoogiFX::Spawn(GetWorld(), P, FLinearColor(0.55f, 0.45f, 0.32f), 110.f, 0.35f, false, 3.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Debris"), P, FRotationMatrix::MakeFromX(Dir).Rotator(), 0.6f);
	ZoogiUtil::PlaySfx(this, TEXT("RockCrack"), 0.9f, FMath::FRandRange(0.95f, 1.05f));
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		G->OnRockEvent(this, false, By, GetActorLocation() + FVector(0, 0, Height + 90.f));
	}
	UE_LOG(LogZoogi, Log, TEXT("Rock %s CRACKED"), *GetName());
}

void AZoogiRock::Shatter(const FVector& P, const FVector& Dir, AZoogiBall* By)
{
	State = EZoogiRockState::Shattered;
	Collider->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the lane opens; nothing left to trap a Zoogi
	Body->SetVisibility(false);
	Side->SetVisibility(false);
	for (UStaticMeshComponent* L : CrackLines) L->SetVisibility(false);
	for (UStaticMeshComponent* R : Rubble) R->SetVisibility(true);
	Launch(ChunkCount, GetActorLocation() + FVector(0, 0, Height * 0.55f), Dir, 520.f, 1.f);
	const FVector Base = GetActorLocation() + FVector(0, 0, 10.f);
	AZoogiFX::Spawn(GetWorld(), Base, FLinearColor(0.6f, 0.5f, 0.36f), Radius * 2.4f, 0.8f, true, 2.5f);
	AZoogiFX::Spawn(GetWorld(), Base + FVector(0, 0, Height * 0.5f), FLinearColor(0.75f, 0.62f, 0.45f), Radius * 1.6f, 0.45f, false, 3.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Dust"), Base, FRotator::ZeroRotator, FMath::Clamp(Radius / 100.f, 0.8f, 1.6f));
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Debris"), Base + FVector(0, 0, Height * 0.4f), FRotationMatrix::MakeFromX(Dir + FVector(0, 0, 0.6f)).Rotator(), 1.f);
	ZoogiUtil::PlaySfx(this, TEXT("RockShatter"), 1.f, FMath::FRandRange(0.95f, 1.05f));
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		G->OnRockEvent(this, true, By, GetActorLocation() + FVector(0, 0, Height + 110.f));
	}
	// Small emphasis only: the tactical camera gets a light kick, no hit-stop.
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(0.35f);
	UE_LOG(LogZoogi, Log, TEXT("Rock %s SHATTERED (%d chunks)"), *GetName(), ChunkCount);
}

void AZoogiRock::Launch(int32 N, const FVector& P, const FVector& Dir, float Speed, float SizeMul)
{
	FRandomStream Rng(Seed * 31 + NextChunk * 7 + (int32)State);
	const FVector Local = P - GetActorLocation();
	for (int32 k = 0; k < N && Chunks.Num(); ++k)
	{
		const int32 i = NextChunk % Chunks.Num();
		++NextChunk;
		FChunk& C = Chunks[i];
		C.Comp = i;
		C.bLive = true;
		C.Age = 0.f;
		const float S = Radius * Rng.FRandRange(0.22f, 0.4f) * SizeMul / 100.f;
		C.Scale = FVector(S * Rng.FRandRange(0.7f, 1.3f), S * Rng.FRandRange(0.7f, 1.3f), S * Rng.FRandRange(0.5f, 1.f));
		C.Half = 50.f * C.Scale.Z;
		C.Pos = Local + FVector(Rng.FRandRange(-0.4f, 0.4f) * Radius, Rng.FRandRange(-0.4f, 0.4f) * Radius, Rng.FRandRange(-0.2f, 0.3f) * Height);
		// Mostly along the hit direction, fanned out, popping upward.
		const FVector Fan = Dir.RotateAngleAxis(Rng.FRandRange(-70.f, 70.f), FVector::UpVector);
		C.Vel = Fan * Speed * Rng.FRandRange(0.4f, 1.f) + FVector(0, 0, Speed * Rng.FRandRange(0.5f, 1.1f));
		C.Rot = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f));
		C.Spin = FRotator(Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(-500.f, 500.f));
		UStaticMeshComponent* M = ChunkPool[i];
		M->SetRelativeLocationAndRotation(C.Pos, C.Rot);
		M->SetRelativeScale3D(C.Scale);
		M->SetVisibility(true);
	}
	SetActorTickEnabled(true);
}

void AZoogiRock::Tick(float Dt)
{
	Super::Tick(Dt);
	// Scripted debris (cosmetic): gravity, floor bounce with damping, spin, fade (shrink) after ~2.2 s, gone by ChunkLife.
	bool bAny = false;
	const float FloorZ = -GetActorLocation().Z; // court top is Z = 0
	for (FChunk& C : Chunks)
	{
		if (!C.bLive) continue;
		C.Age += Dt;
		UStaticMeshComponent* M = ChunkPool[C.Comp];
		if (C.Age >= ChunkLife) { C.bLive = false; M->SetVisibility(false); continue; }
		bAny = true;
		C.Vel.Z -= 980.f * Dt;
		C.Pos += C.Vel * Dt;
		if (C.Pos.Z < FloorZ + C.Half && C.Vel.Z < 0.f)
		{
			C.Pos.Z = FloorZ + C.Half;
			C.Vel.Z *= -0.32f;
			C.Vel.X *= 0.6f; C.Vel.Y *= 0.6f;
			C.Spin *= 0.6f;
		}
		C.Rot += C.Spin * Dt;
		const float Fade = C.Age < ChunkLife - 0.8f ? 1.f : FMath::Max(0.f, (ChunkLife - C.Age) / 0.8f);
		M->SetRelativeLocationAndRotation(C.Pos, C.Rot);
		M->SetRelativeScale3D(C.Scale * Fade);
	}
	if (!bAny) SetActorTickEnabled(false);
}
