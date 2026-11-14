// Active bumpers for Dev Mode objects (see ZoogiBumpers.h).
#include "ZoogiBumpers.h"
#include "ZoogiArena.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiAerialFX.h"
#include "ZoogiRoll.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

bool UZoogiBumperSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

TStatId UZoogiBumperSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiBumperSubsystem, STATGROUP_Tickables);
}

void UZoogiBumperSubsystem::SetBumper(UPrimitiveComponent* C, int32 Level)
{
	if (!C) return;
	if (Level <= 0) { Levels.Remove(C); return; }
	Levels.Add(C, FMath::Clamp(Level, 1, 10));
	if (!C->BodyInstance.bNotifyRigidBodyCollision) C->SetNotifyRigidBodyCollision(true);
	C->OnComponentHit.AddUniqueDynamic(this, &UZoogiBumperSubsystem::OnBumperHit);
}

void UZoogiBumperSubsystem::Tick(float DeltaTime)
{
	if (Levels.Num() == 0) { LastVel.Reset(); return; }
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return;
	// end-of-frame velocities = what each body carried into the next step's contacts
	for (AZoogiBall* B : G->Balls) if (B && B->Sphere) LastVel.Add(B->Sphere.Get(), B->Sphere->GetPhysicsLinearVelocity());
	for (AZoogiOrb* O : G->Orbs) if (O && O->Sphere) LastVel.Add(O->Sphere.Get(), O->Sphere->GetPhysicsLinearVelocity());
}

void UZoogiBumperSubsystem::OnBumperHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const int32* Lv = HitComp ? Levels.Find(HitComp) : nullptr;
	if (!Lv || *Lv <= 0) return;
	UPrimitiveComponent* Body = nullptr;
	float Rad = ZoogiConst::ZoogiRadius;
	FString Who;
	if (AZoogiBall* B = Cast<AZoogiBall>(OtherActor)) { if (B->bOut) return; Body = B->Sphere.Get(); Who = FString::Printf(TEXT("P%d"), B->PlayerIndex + 1); }
	else if (AZoogiOrb* O = Cast<AZoogiOrb>(OtherActor)) { if (O->bOut) return; Body = O->Sphere.Get(); Rad = ZoogiConst::OrbRadius; Who = TEXT("orb"); }
	if (!Body || (OtherComp && OtherComp != Body) || !Body->IsSimulatingPhysics()) return;
	UWorld* W = GetWorld();
	const double Now = W ? W->GetTimeSeconds() : 0.0;
	if (const double* C = Cool.Find(OtherActor)) if (Now - *C < 0.2) return;
	const FVector BL = Body->GetComponentLocation();
	FVector CP = FVector::ZeroVector;
	FVector N = (HitComp->GetClosestPointOnCollision(BL, CP) > 0.f) ? BL - CP : BL - HitComp->Bounds.Origin;
	if (FMath::Abs(N.GetSafeNormal().Z) < 0.7f) N.Z = 0.f; // side hits rebound flat along the floor
	N = N.GetSafeNormal();
	if (N.IsNearlyZero()) return;
	FVector VIn = LastVel.FindRef(Body);
	if (VIn.SizeSquared() < 100.f) VIn = Body->GetPhysicsLinearVelocity();
	FVector R = VIn - 2.f * FVector::DotProduct(VIn, N) * N;
	if (FVector::DotProduct(R, N) <= 0.f) R = N * FMath::Max(VIn.Size(), 1.f);
	const float Speed = FMath::Min(FMath::Max(VIn.Size() * Mul(*Lv), Kick(*Lv)), MaxOut);
	FVector Out = R.GetSafeNormal() * Speed;
	if (N.Z == 0.f) Out.Z = FMath::Clamp(Body->GetPhysicsLinearVelocity().Z, 0.f, 300.f);
	Body->SetPhysicsLinearVelocity(Out);
	LastVel.Add(Body, Out);
	Cool.Add(OtherActor, Now);
	++Rebounds;
	AZoogiImpactFlash::Spawn(W, BL - N * Rad, N, Rebounds * 131 + *Lv, FMath::Clamp(0.6f + 0.06f * *Lv, 0.6f, 1.3f));
	ZoogiUtil::PlaySfx(HitComp, TEXT("Clack"), 0.9f, 0.9f + 0.04f * *Lv);
	UE_LOG(LogZoogi, Log, TEXT("BUMPER %s %s level %d in=%.0f out=%.0f n=(%.2f,%.2f,%.2f)"), *GetNameSafe(HitComp->GetOwner()), *Who, *Lv, VIn.Size(), Speed, N.X, N.Y, N.Z);
}

void ZoogiBumpers::Sync(AZoogiArena* Arena, int32 Index)
{
	if (!Arena || !Arena->DevProps.IsValidIndex(Index)) return;
	UWorld* W = Arena->GetWorld();
	UZoogiBumperSubsystem* S = W ? W->GetSubsystem<UZoogiBumperSubsystem>() : nullptr;
	if (!S) return;
	const AZoogiArena::FZoogiDevProp& P = Arena->DevProps[Index];
	const int32 Level = P.bBumper ? FMath::Clamp(P.Bounce, 1, 10) : 0;
	TArray<UPrimitiveComponent*> Solids;
	if (AActor* A = P.Actor.Get())
	{
		TArray<UPrimitiveComponent*> All;
		A->GetComponents<UPrimitiveComponent>(All);
		for (UPrimitiveComponent* C : All)
			if (C && C->IsCollisionEnabled() && C->GetCollisionEnabled() != ECollisionEnabled::QueryOnly && C->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block) Solids.Add(C);
		if (Solids.Num() == 0) for (UPrimitiveComponent* C : All) if (C && C->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics) Solids.Add(C);
	}
	else
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& C : P.Comps) if (C.IsValid() && C->IsCollisionEnabled() && C->GetCollisionEnabled() != ECollisionEnabled::QueryOnly) Solids.Add(C.Get());
	}
	for (UPrimitiveComponent* C : Solids) S->SetBumper(C, Level);
	if (Level > 0) UE_LOG(LogZoogi, Log, TEXT("BUMPER %s on: level %d (x%.1f, kick %.0f) on %d solid part(s)"), *P.Id, Level, UZoogiBumperSubsystem::Mul(Level), UZoogiBumperSubsystem::Kick(Level), Solids.Num());
}
