#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

bool ZoogiPhysics::IsGrounded(const UPrimitiveComponent* Body, float Radius)
{
	if (!Body || !Body->GetWorld()) return false;
	const FVector Start = Body->GetComponentLocation();
	const FVector End = Start - FVector(0, 0, Radius + 6.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZoogiGround), false, Body->GetOwner());
	FHitResult Hit;
	// Only static world geometry counts as ground (not other marbles or orbs).
	return Body->GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

ZoogiPhysics::FRollResult ZoogiPhysics::ApplyRolling(UPrimitiveComponent* Body, float Radius, float Decel, float DeltaSeconds, float MaxPlanarSpeed)
{
	FRollResult R;
	if (!Body || !Body->IsSimulatingPhysics()) return R;

	const FVector V = Body->GetPhysicsLinearVelocity();
	FVector Planar(V.X, V.Y, 0.f);
	const float Speed = Planar.Size();
	R.PlanarSpeed = Speed;
	R.bGrounded = IsGrounded(Body, Radius);
	if (!R.bGrounded)
	{
		if (Speed > MaxPlanarSpeed)
		{
			Planar *= MaxPlanarSpeed / Speed;
			Body->SetPhysicsLinearVelocity(FVector(Planar.X, Planar.Y, V.Z));
		}
		return R;
	}

	if (Speed < 1.f && FMath::Abs(V.Z) < 5.f)
	{
		// Already at rest: let Chaos sleep the body instead of touching it every frame.
		return R;
	}

	float NewSpeed = FMath::Max(0.f, Speed - Decel * DeltaSeconds);
	NewSpeed = FMath::Min(NewSpeed, MaxPlanarSpeed);
	if (NewSpeed < ZoogiConst::RestSpeed)
	{
		NewSpeed = 0.f;
	}
	const FVector NewPlanar = Speed > KINDA_SMALL_NUMBER ? Planar * (NewSpeed / Speed) : FVector::ZeroVector;
	// Small upward bounces are allowed to die out; once it is resting, drop the residual vertical jitter.
	const float Vz = (NewSpeed == 0.f && FMath::Abs(V.Z) < 40.f) ? 0.f : V.Z;
	Body->SetPhysicsLinearVelocity(FVector(NewPlanar.X, NewPlanar.Y, Vz));
	// Rolling without slipping: w = (Up x v) / r.
	Body->SetPhysicsAngularVelocityInRadians(FVector::CrossProduct(FVector::UpVector, NewPlanar) / Radius);
	if (NewSpeed == 0.f && Vz == 0.f)
	{
		Body->PutRigidBodyToSleep();
	}
	R.PlanarSpeed = NewSpeed;
	return R;
}

UPhysicalMaterial* ZoogiPhysics::MakeMaterial(UObject* Outer, float Friction, float Restitution, bool bOverrideRestitutionMin)
{
	UPhysicalMaterial* PM = NewObject<UPhysicalMaterial>(Outer);
	PM->Friction = Friction;
	PM->StaticFriction = Friction;
	PM->Restitution = Restitution;
	if (bOverrideRestitutionMin)
	{
		PM->bOverrideRestitutionCombineMode = true;
		PM->RestitutionCombineMode = EFrictionCombineMode::Min;
	}
	return PM;
}
