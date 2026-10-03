#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ZoogiOrb.h"
#include "ZoogiGameMode.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AZoogiBall::AZoogiBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(ZoogiConst::ZoogiRadius);
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	Sphere->SetSimulatePhysics(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->BodyInstance.bUseCCD = true;
	Sphere->SetLinearDamping(0.02f);
	Sphere->SetAngularDamping(0.4f);
	Sphere->SetShouldUpdatePhysicsVolume(false);
	RootComponent = Sphere;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Sphere);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(true);

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Sphere);
	Ring->SetUsingAbsoluteRotation(true);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);

	TurnLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TurnLight"));
	TurnLight->SetupAttachment(Sphere);
	TurnLight->SetUsingAbsoluteRotation(true);
	TurnLight->SetRelativeLocation(FVector(0, 0, 120));
	TurnLight->SetIntensityUnits(ELightUnits::Candelas);
	TurnLight->SetIntensity(0.f);
	TurnLight->SetAttenuationRadius(450.f);
	TurnLight->SetCastShadows(false);
}

void AZoogiBall::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentHit.AddDynamic(this, &AZoogiBall::OnSphereHit);
}

void AZoogiBall::InitZoogi(const FZoogiDef& InDef, int32 InPlayerIndex, bool bInCPU, UPhysicalMaterial* PhysMat, float InRollingDecel)
{
	Def = InDef;
	PlayerIndex = InPlayerIndex;
	bCPU = bInCPU;
	RollingDecel = InRollingDecel;
	if (PhysMat) Sphere->SetPhysMaterialOverride(PhysMat);
	Sphere->SetMassOverrideInKg(NAME_None, Def.MassRatio() * ZoogiConst::MassKg, true);

	const FString Id = Def.Id.ToString();
	UStaticMesh* Mesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Zoogis/%s/SM_%s.SM_%s"), *Id, *Id, *Id));
	if (Mesh)
	{
		Visual->SetStaticMesh(Mesh);
	}
	else
	{
		UE_LOG(LogZoogi, Warning, TEXT("No imported model for %s, using a coloured ball"), *Id);
		Visual->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
		Visual->SetMaterial(0, ZoogiUtil::SurfaceMID(this, Def.Color, 0.35f));
	}
	FitVisual();

	const FLinearColor PC = ZoogiUtil::PlayerColor(PlayerIndex);
	Ring->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
	RingMID = ZoogiUtil::GlowMID(this, PC, 2.f);
	Ring->SetMaterial(0, RingMID);
	Ring->SetRelativeScale3D(FVector(1.25f, 1.25f, 0.01f));
	Ring->SetRelativeLocation(FVector(0, 0, -ZoogiConst::ZoogiRadius + 1.5f));
	TurnLight->SetLightColor(PC);
}

void AZoogiBall::FitVisual()
{
	UStaticMesh* Mesh = Visual->GetStaticMesh();
	if (!Mesh) return;
	const FBoxSphereBounds B = Mesh->GetBounds();
	float Scale;
	FVector Offset;
	if (Def.bPivotFit && Mesh != ZoogiUtil::BasicShape(TEXT("Sphere")))
	{
		// zoogiModels.ts: marbleRadius / ROSTER_REFERENCE_RADIUS (0.958 m in the glb = 95.8 cm after import).
		Scale = ZoogiConst::ZoogiRadius / 95.8f;
		Offset = FVector::ZeroVector;
	}
	else
	{
		const float MaxDim = FMath::Max(2.f * B.BoxExtent.GetMax(), 1.f);
		Scale = (2.f * ZoogiConst::ZoogiRadius) / MaxDim;
		Offset = -B.Origin * Scale;
	}
	Visual->SetRelativeScale3D(FVector(Scale));
	Visual->SetRelativeRotation(FRotator(0.f, Def.ModelYaw, 0.f));
	Visual->SetRelativeLocation(FRotator(0.f, Def.ModelYaw, 0.f).RotateVector(Offset));
}

void AZoogiBall::PlaceAt(const FVector& Location, float FacingYaw)
{
	bOut = false;
	SetActorHiddenInGame(false);
	Sphere->SetSimulatePhysics(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorLocationAndRotation(Location, FRotator(0.f, FacingYaw, 0.f), false, nullptr, ETeleportType::ResetPhysics);
	Sphere->SetSimulatePhysics(true);
	Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	AimYaw = FacingYaw;
}

void AZoogiBall::SetOut()
{
	bOut = true;
	Sphere->SetSimulatePhysics(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
}

void AZoogiBall::SetActiveTurn(bool bActive)
{
	bActiveTurn = bActive;
	TurnLight->SetIntensity(bActive ? 60.f : 0.f);
	if (RingMID) RingMID->SetScalarParameterValue(TEXT("Strength"), bActive ? 6.f : 1.5f);
	Ring->SetRelativeScale3D(bActive ? FVector(1.45f, 1.45f, 0.01f) : FVector(1.2f, 1.2f, 0.01f));
}

void AZoogiBall::Launch(const FVector2D& PlanarVelocity)
{
	if (bOut) return;
	Sphere->WakeRigidBody();
	const FVector V(PlanarVelocity.X, PlanarVelocity.Y, 0.f);
	Sphere->SetPhysicsLinearVelocity(V);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::CrossProduct(FVector::UpVector, V) / ZoogiConst::ZoogiRadius);
	if (!V.IsNearlyZero()) AimYaw = V.Rotation().Yaw;
}

bool AZoogiBall::IsAtRest() const
{
	if (bOut) return true;
	const FVector V = Sphere->GetPhysicsLinearVelocity();
	return V.Size() < ZoogiConst::RestSpeed && ZoogiPhysics::IsGrounded(Sphere, ZoogiConst::ZoogiRadius);
}

float AZoogiBall::PlanarSpeed() const
{
	const FVector V = Sphere->GetPhysicsLinearVelocity();
	return FVector2D(V.X, V.Y).Size();
}

FVector AZoogiBall::GetBallVelocity() const
{
	return bOut ? FVector::ZeroVector : Sphere->GetPhysicsLinearVelocity();
}

FVector AZoogiBall::BallLocation() const
{
	return Sphere->GetComponentLocation();
}

void AZoogiBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOut) return;
	ZoogiPhysics::ApplyRolling(Sphere, ZoogiConst::ZoogiRadius, RollingDecel, DeltaSeconds, ZoogiConst::FullLaunchSpeed);

	// Keep the ring flat on the floor under the marble.
	Ring->SetWorldLocation(BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 1.5f));
	TurnLight->SetWorldLocation(BallLocation() + FVector(0, 0, 120.f));
	if (bActiveTurn && RingMID)
	{
		RingPulse += DeltaSeconds;
		RingMID->SetScalarParameterValue(TEXT("Strength"), 4.f + 3.f * FMath::Sin(RingPulse * 4.f));
	}
}

void AZoogiBall::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Knock-off credit goes to the shooter, also through chains (shooter -> Zoogi -> orb).
	const AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const int32 Shooter = G ? G->ActivePlayer : -1;
	const int32 Credit = (PlayerIndex == Shooter) ? PlayerIndex : LastHitPlayer;
	if (AZoogiBall* Other = Cast<AZoogiBall>(OtherActor))
	{
		if (Credit >= 0 && Other->PlayerIndex != Credit) Other->LastHitPlayer = Credit;
	}
	else if (AZoogiOrb* Orb = Cast<AZoogiOrb>(OtherActor))
	{
		if (Credit >= 0) Orb->LastHitBy = Credit;
	}
}
