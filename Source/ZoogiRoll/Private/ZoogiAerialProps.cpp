#include "ZoogiAerialProps.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AZoogiAerialProp::AZoogiAerialProp()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FString AZoogiAerialProp::Label() const
{
	const TCHAR* K = Kind == EZoogiPropKind::SkyBumper ? TEXT("sky_bumper") : (Kind == EZoogiPropKind::ResonanceHalo ? TEXT("resonance_halo") : TEXT("launch_rail"));
	return FString::Printf(TEXT("%s_%d"), K, PropId);
}

void AZoogiAerialProp::Build(EZoogiPropKind InKind, int32 Id, const FVector& Location, const FVector& InForward, float InSize)
{
	Kind = InKind;
	PropId = Id;
	Forward = InForward.GetSafeNormal();
	Size = InSize;
	SetActorLocation(Location);
	auto AddSolid = [this](const FVector& Rel, float R, FName Tag)
	{
		USphereComponent* S = NewObject<USphereComponent>(this);
		S->SetupAttachment(RootComponent);
		S->SetSphereRadius(R);
		S->SetRelativeLocation(Rel);
		S->SetCollisionProfileName(TEXT("BlockAll"));
		S->SetNotifyRigidBodyCollision(true);
		S->SetGenerateOverlapEvents(false);
		S->ComponentTags.Add(Tag);
		S->RegisterComponent();
		S->OnComponentHit.AddDynamic(this, &AZoogiAerialProp::OnSolidHit);
		Solids.Add(S);
	};
	if (Kind == EZoogiPropKind::SkyBumper)
	{
		AddSolid(FVector::ZeroVector, Size, TEXT("ZBumper"));
	}
	else if (Kind == EZoogiPropKind::ResonanceHalo)
	{
		// vertical ring in the plane (Side, Up), open at the bottom; the ring centre sits Size above Location
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		for (int32 a = -50; a <= 230; a += 20)
		{
			const float Rad = FMath::DegreesToRadians((float)a);
			AddSolid(Side * FMath::Cos(Rad) * Size + FVector(0, 0, Size + FMath::Sin(Rad) * Size), 15.f, TEXT("ZHaloRim"));
		}
		Trigger = NewObject<UBoxComponent>(this);
		Trigger->SetupAttachment(RootComponent);
		Trigger->SetBoxExtent(FVector(10.f, Size * 0.45f, 45.f));
		Trigger->SetRelativeLocation(FVector(0, 0, 55.f));
		Trigger->SetRelativeRotation(Forward.Rotation());
	}
	else
	{
		Trigger = NewObject<UBoxComponent>(this);
		Trigger->SetupAttachment(RootComponent);
		Trigger->SetBoxExtent(FVector(Size, 70.f, 70.f));
		Trigger->SetRelativeLocation(FVector(0, 0, 55.f));
		Trigger->SetRelativeRotation(Forward.Rotation());
	}
	if (Trigger)
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision); // a debug volume: never touches the simulation
		Trigger->SetGenerateOverlapEvents(false);
		Trigger->RegisterComponent();
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE prop spawn %s at (%.0f,%.0f,%.0f) fwd=(%.2f,%.2f) size=%.0f solids=%d"), *Label(), Location.X, Location.Y, Location.Z, Forward.X, Forward.Y, Size, Solids.Num());
}

bool AZoogiAerialProp::Cooldown(AActor* A, float Seconds, int32 Channel)
{
	TMap<TWeakObjectPtr<AActor>, float>& M = LastEvent[Channel ? 1 : 0];
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (const float* L = M.Find(A)) if (Now - *L < Seconds) return false;
	M.Add(A, Now);
	return true;
}

void AZoogiAerialProp::OnSolidHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AZoogiBall* B = Cast<AZoogiBall>(OtherActor);
	if (!B || !HitComp || !Cooldown(B, 0.25f)) return;
	UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	if (OtherComp && B->Sphere && OtherComp != B->Sphere) return;
	if (Kind == EZoogiPropKind::SkyBumper && B->Sphere)
	{
		// deterministic rebound: normal from the bumper centre to the ball centre (2D for a floor bumper), reflect, x ReboundMul
		const FVector C = HitComp->GetComponentLocation();
		FVector N = B->BallLocation() - C;
		const bool bFloor = C.Z < 150.f;
		if (bFloor) N.Z = 0.f;
		N = N.GetSafeNormal();
		const FVector VIn = B->PrevVelocity.SizeSquared() > 1.f ? B->PrevVelocity : B->Sphere->GetPhysicsLinearVelocity();
		FVector R = VIn - 2.f * FVector::DotProduct(VIn, N) * N;
		if (FVector::DotProduct(R, N) <= 0.f) R = N * VIn.Size();
		if (bFloor) R.Z = 0.f;
		const float Speed = FMath::Min(VIn.Size() * ReboundMul, ReboundMaxSpeed);
		const FVector Out = R.GetSafeNormal() * Speed;
		B->Sphere->SetPhysicsLinearVelocity(Out);
		LastIn = VIn; LastOut = Out; LastNormal = N; ++Rebounds;
		UE_LOG(LogZoogi, Log, TEXT("BATTLE skybumper %s P%d in=(%.0f,%.0f,%.0f)|%.0f| n=(%.2f,%.2f,%.2f) out=(%.0f,%.0f,%.0f)|%.0f| mul=%.2f"), *Label(), B->PlayerIndex + 1,
			VIn.X, VIn.Y, VIn.Z, VIn.Size(), N.X, N.Y, N.Z, Out.X, Out.Y, Out.Z, Out.Size(), ReboundMul);
		if (SM) SM->OnPropEvent(this, B, TEXT("bumper"), B->BallLocation(), VIn.Size(), Out.Size());
	}
	else if (Kind == EZoogiPropKind::ResonanceHalo)
	{
		UE_LOG(LogZoogi, Log, TEXT("BATTLE halo %s rim P%d at (%.0f,%.0f,%.0f)"), *Label(), B->PlayerIndex + 1, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
		if (SM) SM->OnPropEvent(this, B, TEXT("rim"), FVector(Hit.ImpactPoint), 0.f, 0.f);
	}
}

bool AZoogiAerialProp::InTrigger(const FVector& Local) const
{
	if (!Trigger) return false;
	const FVector E = Trigger->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

void AZoogiAerialProp::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* W = GetWorld();
	if (!W) return;
	const FColor C = Kind == EZoogiPropKind::SkyBumper ? FColor::Orange : (Kind == EZoogiPropKind::ResonanceHalo ? FColor::Cyan : FColor::Green);
	for (const USphereComponent* S : Solids) if (S) DrawDebugSphere(W, S->GetComponentLocation(), S->GetScaledSphereRadius(), 10, C, false, -1.f, 0, 1.5f);
	if (Trigger) DrawDebugBox(W, Trigger->GetComponentLocation(), Trigger->GetScaledBoxExtent(), Trigger->GetComponentQuat(), C, false, -1.f, 0, 1.f);
	if (Kind == EZoogiPropKind::LaunchRail) DrawDebugDirectionalArrow(W, GetActorLocation() + FVector(0, 0, 20), GetActorLocation() + FVector(0, 0, 20) + Forward * Size * 2.f, 40.f, C, false, -1.f, 0, 3.f);
#endif
}
