#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ZoogiOrb.h"
#include "ZoogiGameMode.h"
#include "ZoogiFX.h"
#include "ZoogiRock.h"
#include "ZoogiArtifactComponent.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

AZoogiBall::AZoogiBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(ZoogiConst::ZoogiRadius);
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	// Zoogis live on the Pawn channel so Bolt can phase through them while still hitting orbs.
	Sphere->SetCollisionObjectType(ECC_Pawn);
	Sphere->SetGenerateOverlapEvents(true);
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

	Artifact = CreateDefaultSubobject<UZoogiArtifactComponent>(TEXT("Artifact"));
}

void AZoogiBall::SetMassScale(float Mul)
{
	Sphere->SetMassOverrideInKg(NAME_None, BaseMassKg * FMath::Max(0.1f, Mul), true);
}

void AZoogiBall::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentHit.AddDynamic(this, &AZoogiBall::OnSphereHit);
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AZoogiBall::OnSphereOverlap);
}

void AZoogiBall::SetPhasing(bool bOn)
{
	bPhasing = bOn;
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, bOn ? ECR_Overlap : ECR_Block);
	Visual->SetRenderCustomDepth(bOn);
}

void AZoogiBall::SetPlanarVelocity(const FVector& V, float Up)
{
	if (bOut) return;
	Sphere->WakeRigidBody();
	const FVector Cur = Sphere->GetPhysicsLinearVelocity();
	Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, Up != 0.f ? Up : Cur.Z));
	const FVector P(V.X, V.Y, 0.f);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::CrossProduct(FVector::UpVector, P) / ZoogiConst::ZoogiRadius);
}

void AZoogiBall::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bPhasing) return;
	AZoogiBall* Other = Cast<AZoogiBall>(OtherActor);
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (Other && G && !Other->bOut) G->OnShooterContact(this, Other);
}

void AZoogiBall::InitZoogi(const FZoogiDef& InDef, int32 InPlayerIndex, bool bInCPU, UPhysicalMaterial* PhysMat, float InRollingDecel)
{
	Def = InDef;
	PlayerIndex = InPlayerIndex;
	bCPU = bInCPU;
	RollingDecel = InRollingDecel;
	if (PhysMat) Sphere->SetPhysMaterialOverride(PhysMat);
	BaseMassKg = Def.MassRatio() * ZoogiConst::MassKg;
	TeamIndex = InPlayerIndex;
	Sphere->SetMassOverrideInKg(NAME_None, BaseMassKg, true);

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
	if (!bActive) TurnLight->SetLightColor(ZoogiUtil::PlayerColor(PlayerIndex));
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
	TickRollSound();
	if (bOut) return;
	PrevVelocity = GetBallVelocity();
	ZoogiPhysics::ApplyRolling(Sphere, ZoogiConst::ZoogiRadius, RollingDecel, DeltaSeconds, ZoogiConst::FullLaunchSpeed);

	// Keep the ring flat on the floor under the marble.
	Ring->SetWorldLocation(BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 1.5f));
	TurnLight->SetWorldLocation(BallLocation() + FVector(0, 0, 120.f));
	StatusTime += DeltaSeconds;
	if (!bActiveTurn && RingMID && (StunTurns > 0 || BoundShots > 0))
	{
		RingMID->SetVectorParameterValue(TEXT("Color"), StunTurns > 0 ? FLinearColor(0.7f, 0.3f, 1.f) : FLinearColor(0.85f, 0.7f, 0.45f));
		RingMID->SetScalarParameterValue(TEXT("Strength"), 3.f + 2.5f * FMath::Sin(StatusTime * 9.f));
	}
	else if (RingMID && !bActiveTurn)
	{
		RingMID->SetVectorParameterValue(TEXT("Color"), ZoogiUtil::PlayerColor(PlayerIndex));
	}
	// Shot trail: lightless ground puffs in the player colour while this Zoogi is the rolling shooter.
	if (bActiveTurn && !bOut)
	{
		AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		const float Spd = PlanarSpeed();
		TrailClock += DeltaSeconds;
		const bool bInfused = Artifact && Artifact->Infused != EZoogiArtifact::None; // artifact trail replaces the player puffs
		if (GM && GM->Phase == EZoogiPhase::Rolling && Spd > 250.f && TrailClock > 0.04f && !bInfused)
		{
			TrailClock = 0.f;
			const FVector L = BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f);
			const float K = FMath::Clamp(Spd / 2000.f, 0.f, 1.f);
			AZoogiFX::SpawnTrail(GetWorld(), L, ZoogiUtil::PlayerColor(PlayerIndex), 26.f + 22.f * K, 0.55f, 3.f + 4.f * K);
		}
	}
	// Niagara sparkle trail: every Zoogi moving fast during a shot sheds a little burst of sparks in its player colour.
	if (!bOut)
	{
		const float Spd = PlanarSpeed();
		SparkClock += DeltaSeconds;
		const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		if (GM && GM->Phase == EZoogiPhase::Rolling && Spd > 520.f && SparkClock > FMath::Lerp(0.11f, 0.05f, FMath::Clamp((Spd - 520.f) / 1800.f, 0.f, 1.f)))
		{
			SparkClock = 0.f;
			static const TCHAR* Bursts[4] = { TEXT("NS_Burst_Blue"), TEXT("NS_Burst_Red"), TEXT("NS_Burst_Yellow"), TEXT("NS_Burst_Green") };
			const FVector V = GetBallVelocity();
			const FVector Back = -FVector(V.X, V.Y, 0.f).GetSafeNormal() * (ZoogiConst::ZoogiRadius * 0.8f);
			const float K = FMath::Clamp(Spd / 2400.f, 0.f, 1.f);
			AZoogiFX::Niagara(GetWorld(), Bursts[FMath::Clamp(PlayerIndex, 0, 3)], BallLocation() + Back - FVector(0, 0, ZoogiConst::ZoogiRadius * 0.5f), FRotator::ZeroRotator, 0.16f + 0.16f * K);
		}
	}
	if (bActiveTurn && RingMID)
	{
		RingMID->SetVectorParameterValue(TEXT("Color"), bPhasing ? FLinearColor(1.f, 0.9f, 0.2f) : ZoogiUtil::PlayerColor(PlayerIndex));
		RingPulse += DeltaSeconds;
		RingMID->SetScalarParameterValue(TEXT("Strength"), 4.f + 3.f * FMath::Sin(RingPulse * 4.f));
	}
}

void AZoogiBall::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Knock-off credit goes to the shooter, also through chains (shooter -> Zoogi -> orb).
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const int32 Shooter = G ? G->ActivePlayer : -1;
	if (G && PlayerIndex == Shooter && (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor))) G->OnShooterContact(this, OtherActor);
	// Guard: the active attacker hit this guarded Zoogi.
	if (G && PlayerIndex != Shooter && Artifact && Artifact->Guard != EZoogiArtifact::None)
	{
		if (AZoogiBall* Att = Cast<AZoogiBall>(OtherActor)) if (Att->PlayerIndex == Shooter) G->OnGuardedHit(this, Att);
	}
	// Destructible rocks take the hit (crack / shatter thresholds live in AZoogiRock).
	if (AZoogiRock* Rock = Cast<AZoogiRock>(OtherActor)) Rock->OnBallHit(this, NormalImpulse, Hit.ImpactPoint);
	// Impact spark on hard hits between marbles/orbs.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if ((Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor)) && NormalImpulse.Size() > 3500.f && Now - LastImpactFx > 0.15f)
	{
		LastImpactFx = Now;
		const float R = FMath::Clamp(45.f + NormalImpulse.Size() * 0.004f, 50.f, 150.f);
		AZoogiFX::Spawn(GetWorld(), Hit.ImpactPoint, FLinearColor(0.85f, 0.95f, 1.f), R, 0.25f, false, 10.f);
		const FVector N = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
		if (!N.IsNearlyZero())
		{
			// Two spark sprays, one off each marble.
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Sparks"), Hit.ImpactPoint, FRotationMatrix::MakeFromX(N).Rotator(), 1.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Sparks"), Hit.ImpactPoint, FRotationMatrix::MakeFromX(-N).Rotator(), 1.f);
		}
		// Zoogi vs Zoogi: the sharp anime "crack" (once per pair, from the lower index); orbs keep the clack.
		if (AZoogiBall* OB = Cast<AZoogiBall>(OtherActor))
		{
			if (PlayerIndex < OB->PlayerIndex || Now - OB->LastImpactFx > 0.15f)
				ZoogiUtil::PlaySfx(this, TEXT("HitCrack"), FMath::Clamp(NormalImpulse.Size() / 14000.f, 0.35f, 1.f), FMath::FRandRange(0.94f, 1.08f));
		}
		else ZoogiUtil::PlaySfx(this, TEXT("Clack"), FMath::Clamp(NormalImpulse.Size() / 15000.f, 0.25f, 1.f), FMath::FRandRange(0.9f, 1.15f) * 1.25f);
	}
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

void AZoogiBall::TickRollSound()
{
	if (bRollSoundMissing) return;
	const float Speed = bOut ? 0.f : GetBallVelocity().Size2D();
	const bool bWant = Speed > 60.f && ZoogiUtil::GetSfxVolume() > 0.001f;
	if (!RollAudio)
	{
		if (!bWant) return;
		USoundBase* S = ZoogiUtil::LoadSound(TEXT("RollLoop"));
		if (!S) { bRollSoundMissing = true; return; }
		RollAudio = NewObject<UAudioComponent>(this);
		RollAudio->SetSound(S);
		RollAudio->bAutoActivate = false;
		RollAudio->bAllowSpatialization = false;
		RollAudio->bIsUISound = false; // pauses with the game (anime freezes)
		RollAudio->SetupAttachment(Sphere);
		RollAudio->RegisterComponent();
	}
	if (!bWant)
	{
		if (RollAudio->IsPlaying() && Speed < 40.f) RollAudio->FadeOut(0.15f, 0.f);
		return;
	}
	// Louder / higher with speed; heavier Zoogis (MassRatio from Defense) are deeper and a touch louder.
	const float U = FMath::Clamp(Speed / ZoogiConst::FullLaunchSpeed, 0.f, 1.f);
	const float Mass = Def.MassRatio();
	const float Vol = ZoogiUtil::GetSfxVolume() * 0.3f * FMath::Pow(FMath::Clamp(Speed / 1800.f, 0.f, 1.f), 0.7f) * (0.85f + 0.15f * Mass);
	const float Pitch = FMath::Clamp(FMath::Lerp(0.75f, 1.25f, U) / FMath::Sqrt(Mass), 0.55f, 1.6f);
	RollAudio->SetVolumeMultiplier(Vol);
	if (!RollAudio->IsPlaying()) RollAudio->FadeIn(0.08f, 1.f);
	RollAudio->SetPitchMultiplier(Pitch);
}
