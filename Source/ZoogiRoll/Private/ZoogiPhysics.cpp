#include "ZoogiPhysics.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiObstacle.h"
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

float ZoogiPhysics::MassFeelT(float MassRatio)
{
	return FMath::Clamp((MassRatio - Feel::MassMin) / (Feel::MassMax - Feel::MassMin), 0.f, 1.f);
}

float ZoogiPhysics::MassRollBlend(float MassRatio)
{
	return FMath::Lerp(Feel::LightRollBlend, Feel::HeavyRollBlend, MassFeelT(MassRatio));
}

float ZoogiPhysics::MassDecelMul(float MassRatio)
{
	return FMath::Lerp(Feel::LightDecelMul, Feel::HeavyDecelMul, MassFeelT(MassRatio));
}

float ZoogiPhysics::FlickSpinFromSide(float SideFrac)
{
	const float S = FMath::Clamp(SideFrac, -1.f, 1.f);
	if (FMath::Abs(S) < Feel::FlickSideDeadZone) return 0.f;
	// Remap out of the dead-zone so a just-above-threshold flick is still readable.
	const float Mag = (FMath::Abs(S) - Feel::FlickSideDeadZone) / (1.f - Feel::FlickSideDeadZone);
	return FMath::Sign(S) * Mag * Feel::FlickSpinMaxRad;
}

FVector2D ZoogiPhysics::FlickLateralAccel(const FVector2D& PlanarVel, float FlickSpinYaw)
{
	const float Speed = PlanarVel.Size();
	if (Speed < ZoogiConst::RestSpeed || FMath::Abs(FlickSpinYaw) < KINDA_SMALL_NUMBER) return FVector2D::ZeroVector;
	// Bend to the left of travel for positive yaw spin (Up x V sense): lateral = normalize(perp(V)) * spin * speed * gain.
	const FVector2D Dir = PlanarVel / Speed;
	const FVector2D Lat(-Dir.Y, Dir.X); // +Z cross in 2D
	const float A = FMath::Clamp(FlickSpinYaw * Speed * Feel::FlickCurveGain, -Feel::FlickMaxLatAccel, Feel::FlickMaxLatAccel);
	return Lat * A;
}

float ZoogiPhysics::FlickDecaySpin(float FlickSpinYaw, float DeltaSeconds)
{
	if (FMath::Abs(FlickSpinYaw) < KINDA_SMALL_NUMBER) return 0.f;
	return FlickSpinYaw * FMath::Exp(-Feel::FlickDecayPerSec * FMath::Max(0.f, DeltaSeconds));
}

ZoogiPhysics::FRailResponse ZoogiPhysics::RailResponse(float Incidence)
{
	FRailResponse R;
	const float Inc = FMath::Clamp(Incidence, 0.f, 1.f);
	if (Inc >= Feel::HeadOnIncidence)
	{
		R.NormalScale = Feel::HeadOnNormalScale;
		R.TangentScale = Feel::HeadOnTangentScale;
		return R;
	}
	if (Inc <= Feel::GlanceIncidence)
	{
		R.NormalScale = Feel::GlanceNormalScale;
		R.TangentScale = Feel::GlanceTangentScale;
		return R;
	}
	const float T = (Inc - Feel::GlanceIncidence) / (Feel::HeadOnIncidence - Feel::GlanceIncidence);
	R.NormalScale = FMath::Lerp(Feel::GlanceNormalScale, Feel::HeadOnNormalScale, T);
	R.TangentScale = FMath::Lerp(Feel::GlanceTangentScale, Feel::HeadOnTangentScale, T);
	return R;
}

bool ZoogiPhysics::IsStageHit(const AActor* OtherActor, const UPrimitiveComponent* OtherComp)
{
	if (!OtherActor) return false;
	if (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor)) return false;
	if (Cast<AZoogiRock>(OtherActor) || Cast<AZoogiArenaPiece>(OtherActor) || Cast<AZoogiObstacle>(OtherActor)) return true;
	if (OtherComp && (OtherComp->ComponentHasTag(TEXT("ZWall")) || OtherComp->ComponentHasTag(TEXT("ZBumper"))
		|| OtherComp->ComponentHasTag(TEXT("ZRamp")) || OtherComp->ComponentHasTag(TEXT("ZPlatform"))))
	{
		return true;
	}
	if (OtherActor->ActorHasTag(TEXT("ZWall")) || OtherActor->ActorHasTag(TEXT("ZBumper"))
		|| OtherActor->ActorHasTag(TEXT("ArenaPiece")) || OtherActor->ActorHasTag(TEXT("Rock")))
	{
		return true;
	}
	return false;
}

void ZoogiPhysics::ApplyRailBumperFeel(UPrimitiveComponent* Body, const FVector& HitNormal, const FVector& ApproachVel, const AActor* OtherActor, const UPrimitiveComponent* OtherComp)
{
	if (!Body || !Body->IsSimulatingPhysics()) return;
	if (!IsStageHit(OtherActor, OtherComp)) return;

	// Prefer planar wall normals; skip floor/ceiling contacts (landings).
	FVector N = HitNormal;
	if (FMath::Abs(N.Z) > 0.65f) return;
	N.Z = 0.f;
	if (!N.Normalize()) return;

	FVector Approach = ApproachVel;
	Approach.Z = 0.f;
	const float AppSpeed = Approach.Size();
	if (AppSpeed < ZoogiConst::RestSpeed) return;
	const float Incidence = FMath::Abs(FVector::DotProduct(N, Approach / AppSpeed));
	const FRailResponse R = RailResponse(Incidence);

	FVector V = Body->GetPhysicsLinearVelocity();
	const float Vz = V.Z;
	V.Z = 0.f;
	const float Vn = FVector::DotProduct(V, N);
	FVector Vt = V - N * Vn;
	// Outward (post-bounce) normal component: scale for head-on snap vs glancing damp.
	float NewVn = Vn;
	if (Vn > 0.f) NewVn = Vn * R.NormalScale;
	Vt *= R.TangentScale;
	Body->SetPhysicsLinearVelocity(FVector(N * NewVn + Vt) + FVector(0, 0, Vz));
}

ZoogiPhysics::FRollResult ZoogiPhysics::ApplyRolling(UPrimitiveComponent* Body, float Radius, float Decel, float DeltaSeconds, float MaxPlanarSpeed, float MassRatio, float* InOutFlickSpinYaw)
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
		// Airborne: flick spin still decays so a lofted flick does not resume curving forever on landing.
		if (InOutFlickSpinYaw) *InOutFlickSpinYaw = FlickDecaySpin(*InOutFlickSpinYaw, DeltaSeconds);
		return R;
	}

	if (Speed < 1.f && FMath::Abs(V.Z) < 5.f)
	{
		// Already at rest: let Chaos sleep the body instead of touching it every frame.
		if (InOutFlickSpinYaw) *InOutFlickSpinYaw = 0.f;
		return R;
	}

	const float DecelMul = MassDecelMul(MassRatio);
	float NewSpeed = FMath::Max(0.f, Speed - Decel * DecelMul * DeltaSeconds);
	NewSpeed = FMath::Min(NewSpeed, MaxPlanarSpeed);
	FVector NewPlanar = Speed > KINDA_SMALL_NUMBER ? Planar * (NewSpeed / Speed) : FVector::ZeroVector;

	// Item 21: short-lived lateral bend from gameplay flick spin (not visual SpinBoost).
	if (InOutFlickSpinYaw && FMath::Abs(*InOutFlickSpinYaw) > KINDA_SMALL_NUMBER && NewSpeed >= ZoogiConst::RestSpeed)
	{
		const FVector2D Acc = FlickLateralAccel(FVector2D(NewPlanar.X, NewPlanar.Y), *InOutFlickSpinYaw);
		NewPlanar.X += Acc.X * DeltaSeconds;
		NewPlanar.Y += Acc.Y * DeltaSeconds;
		const float Bent = NewPlanar.Size();
		if (Bent > MaxPlanarSpeed) NewPlanar *= MaxPlanarSpeed / Bent;
		NewSpeed = NewPlanar.Size();
		*InOutFlickSpinYaw = FlickDecaySpin(*InOutFlickSpinYaw, DeltaSeconds);
	}
	else if (InOutFlickSpinYaw)
	{
		*InOutFlickSpinYaw = FlickDecaySpin(*InOutFlickSpinYaw, DeltaSeconds);
	}

	if (NewSpeed < ZoogiConst::RestSpeed)
	{
		NewSpeed = 0.f;
		NewPlanar = FVector::ZeroVector;
		if (InOutFlickSpinYaw) *InOutFlickSpinYaw = 0.f;
	}

	// Small upward bounces are allowed to die out; once it is resting, drop the residual vertical jitter.
	const float Vz = (NewSpeed == 0.f && FMath::Abs(V.Z) < 40.f) ? 0.f : V.Z;
	Body->SetPhysicsLinearVelocity(FVector(NewPlanar.X, NewPlanar.Y, Vz));

	// Item 25: blend toward rolling-without-slipping. Heavy -> perfect roll; light -> longer skid.
	const FVector TargetW = FVector::CrossProduct(FVector::UpVector, NewPlanar) / Radius;
	const float Blend = FMath::Clamp(MassRollBlend(MassRatio), 0.f, 1.f);
	if (Blend >= 0.999f || NewSpeed == 0.f)
	{
		Body->SetPhysicsAngularVelocityInRadians(TargetW);
	}
	else
	{
		const FVector CurW = Body->GetPhysicsAngularVelocityInRadians();
		// Frame-rate aware catch-up: Blend is the heavy/light feel at ~60 fps; scale with dt.
		const float Alpha = 1.f - FMath::Pow(1.f - Blend, FMath::Clamp(DeltaSeconds * 60.f, 0.f, 4.f));
		Body->SetPhysicsAngularVelocityInRadians(FMath::Lerp(CurW, TargetW, Alpha));
	}

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