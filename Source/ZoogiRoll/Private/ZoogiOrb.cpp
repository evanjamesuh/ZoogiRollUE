#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiPhysics.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AZoogiOrb::AZoogiOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(ZoogiConst::OrbRadius);
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	Sphere->SetSimulatePhysics(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->BodyInstance.bUseCCD = true;
	Sphere->SetLinearDamping(0.02f);
	Sphere->SetAngularDamping(0.4f);
	RootComponent = Sphere;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Sphere);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Sphere);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Sphere);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(8.f);
	Glow->SetAttenuationRadius(260.f);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(ZoogiUtil::Hex(TEXT("#87CEEB")));
}

void AZoogiOrb::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentHit.AddDynamic(this, &AZoogiOrb::OnSphereHit);
	Phase = FMath::FRand() * 6.28f;
}

void AZoogiOrb::InitOrb(UPhysicalMaterial* PhysMat, float InRollingDecel)
{
	RollingDecel = InRollingDecel;
	if (PhysMat) Sphere->SetPhysMaterialOverride(PhysMat);
	Sphere->SetMassOverrideInKg(NAME_None, ZoogiConst::OrbMassRatio * ZoogiConst::MassKg, true);

	// Imported orb.glb if present, else an engine sphere. Both get the glowing light-blue look.
	UStaticMesh* OrbMesh = ZoogiUtil::LoadMesh(TEXT("/Game/Orbs/SM_Orb.SM_Orb"));
	UStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));
	const FLinearColor Body = ZoogiUtil::Hex(TEXT("#87CEEB"));
	const FLinearColor Deep = ZoogiUtil::Hex(TEXT("#0341CA"));

	Visual->SetStaticMesh(SphereMesh);
	Visual->SetRelativeScale3D(FVector(2.f * ZoogiConst::OrbRadius / 100.f));
	Visual->SetMaterial(0, ZoogiUtil::GlowMID(this, Body, 3.f));

	if (OrbMesh)
	{
		Core->SetStaticMesh(OrbMesh);
		const FBoxSphereBounds B = OrbMesh->GetBounds();
		const float S = (1.5f * ZoogiConst::OrbRadius) / FMath::Max(B.BoxExtent.GetMax(), 1.f);
		Core->SetRelativeScale3D(FVector(S));
		Core->SetRelativeLocation(-B.Origin * S);
		for (int32 i = 0; i < Core->GetNumMaterials(); ++i)
		{
			Core->SetMaterial(i, ZoogiUtil::GlowMID(this, Deep, 6.f));
		}
	}
	else
	{
		Core->SetStaticMesh(SphereMesh);
		Core->SetRelativeScale3D(FVector(1.2f * ZoogiConst::OrbRadius / 100.f));
		Core->SetMaterial(0, ZoogiUtil::GlowMID(this, FLinearColor(0.75f, 0.95f, 1.f), 12.f));
	}
}

void AZoogiOrb::MakeStar()
{
	bStar = true;
	const FLinearColor Gold(1.f, 0.72f, 0.12f);
	Visual->SetMaterial(0, ZoogiUtil::GlowMID(this, Gold, 0.9f));
	for (int32 i = 0; i < Core->GetNumMaterials(); ++i) Core->SetMaterial(i, ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.45f, 0.02f), 3.f));
	Glow->SetLightColor(Gold);
	Glow->SetAttenuationRadius(320.f);
}

void AZoogiOrb::SetOut()
{
	bOut = true;
	Sphere->SetSimulatePhysics(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	Glow->SetIntensity(0.f);
}

void AZoogiOrb::Respawn(const FVector& Location)
{
	bOut = false;
	LastHitBy = -1;
	SetActorHiddenInGame(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
	Sphere->SetSimulatePhysics(true);
	Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Glow->SetIntensity(bStar ? 12.f : 7.f);
}

bool AZoogiOrb::IsAtRest() const
{
	if (bOut) return true;
	return Sphere->GetPhysicsLinearVelocity().Size() < ZoogiConst::RestSpeed && ZoogiPhysics::IsGrounded(Sphere, ZoogiConst::OrbRadius);
}

FVector AZoogiOrb::OrbLocation() const
{
	return Sphere->GetComponentLocation();
}

void AZoogiOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOut) return;
	ZoogiPhysics::ApplyRolling(Sphere, ZoogiConst::OrbRadius, RollingDecel, DeltaSeconds, ZoogiConst::FullLaunchSpeed);
	Phase += DeltaSeconds;
	Glow->SetIntensity((bStar ? 12.f : 7.f) + (bStar ? 5.f : 2.5f) * FMath::Sin(Phase * (bStar ? 4.f : 2.2f)));
}

void AZoogiOrb::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Chain credit: an orb pushed by an orb inherits that orb's hitter.
	if (AZoogiOrb* Other = Cast<AZoogiOrb>(OtherActor))
	{
		if (Other->LastHitBy >= 0 && LastHitBy < 0) LastHitBy = Other->LastHitBy;
		else if (LastHitBy >= 0 && Other->LastHitBy < 0) Other->LastHitBy = LastHitBy;
	}
}
