#include "ZoogiClone.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

static constexpr float CloneRadius = 38.f;
static constexpr float CloneLife = 3.2f;

AZoogiClone::AZoogiClone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(CloneRadius);
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	Sphere->SetSimulatePhysics(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->BodyInstance.bUseCCD = true;
	Sphere->SetEnableGravity(true);
	RootComponent = Sphere;
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Sphere);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
}

void AZoogiClone::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentHit.AddDynamic(this, &AZoogiClone::OnSphereHit);
}

void AZoogiClone::InitClone(UStaticMesh* WolfMesh, AActor* InTarget, int32 InOwnerPlayer, float InSpeed, FLinearColor Tint)
{
	Target = InTarget;
	OwnerPlayer = InOwnerPlayer;
	Speed = InSpeed;
	Sphere->SetMassOverrideInKg(NAME_None, 0.7f * ZoogiConst::MassKg, true);
	UStaticMesh* M = WolfMesh ? WolfMesh : ZoogiUtil::BasicShape(TEXT("Sphere"));
	Visual->SetStaticMesh(M);
	const FBoxSphereBounds B = M->GetBounds();
	const float S = (2.f * CloneRadius) / FMath::Max(2.f * B.BoxExtent.GetMax(), 1.f);
	Visual->SetRelativeScale3D(FVector(S));
	Visual->SetRelativeLocation(-B.Origin * S);
	MID = ZoogiUtil::FxMID(this, Tint, 9.f);
	for (int32 i = 0; i < Visual->GetNumMaterials(); ++i) Visual->SetMaterial(i, MID);
}

void AZoogiClone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (MID) MID->SetScalarParameterValue(TEXT("Strength"), 9.f * FMath::Clamp((CloneLife - Age) / 0.6f, 0.f, 1.f) + 1.5f * FMath::Sin(Age * 18.f));
	if (Age > CloneLife || GetActorLocation().Z < ZoogiConst::FallOutZ)
	{
		Destroy();
		return;
	}
	// Steer toward the target until the first hit, then roll freely.
	AActor* T = Target.Get();
	bool bTargetGone = !T || T->IsHidden();
	if (!bHit && !bTargetGone && Age < CloneLife - 0.8f)
	{
		const FVector V = Sphere->GetPhysicsLinearVelocity();
		FVector To = T->GetActorLocation() - GetActorLocation();
		To.Z = 0.f;
		const FVector Desired = To.GetSafeNormal() * Speed;
		const FVector Planar(V.X, V.Y, 0.f);
		const FVector NewV = FMath::VInterpTo(Planar, Desired, DeltaSeconds, 6.f);
		Sphere->SetPhysicsLinearVelocity(FVector(NewV.X, NewV.Y, V.Z));
	}
}

void AZoogiClone::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (AZoogiOrb* Orb = Cast<AZoogiOrb>(OtherActor))
	{
		Orb->LastHitBy = OwnerPlayer;
	}
	else if (AZoogiBall* Ball = Cast<AZoogiBall>(OtherActor))
	{
		if (Ball->PlayerIndex == OwnerPlayer) return;
		Ball->LastHitPlayer = OwnerPlayer;
	}
	else
	{
		return;
	}
	if (!bHit) AZoogiFX::Spawn(GetWorld(), GetActorLocation(), FLinearColor(0.5f, 0.7f, 1.f), 90.f, 0.35f, false, 5.f);
	bHit = true;
}
