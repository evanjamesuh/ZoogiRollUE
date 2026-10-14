#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ZoogiOrb.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiFX.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFloorScars.h"
#include "ZoogiArena.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiExpressionComponent.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

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

	Nubs = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Nubs"));
	Nubs->SetupAttachment(Visual);
	Nubs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Nubs->SetCastShadow(true);
	Nubs->SetVisibility(false);

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
	Expression = CreateDefaultSubobject<UZoogiExpressionComponent>(TEXT("Expression"));
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
	if (Nubs) Nubs->SetRenderCustomDepth(bOn);
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
	UStaticMesh* Mesh = nullptr;
	UStaticMesh* NubMesh = nullptr;
	if (!Def.MeshAsset.IsNone())
	{
		// Bible characters (Scripts/import_bible_characters.py): shell + eyes + trim + core in one mesh, nubs separate.
		const FString A = Def.MeshAsset.ToString();
		Mesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Bible/%s/SM_%s.SM_%s"), *A, *A, *A));
		NubMesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Bible/%s/SM_%s_Nubs.SM_%s_Nubs"), *A, *A, *A));
	}
	if (!Mesh) Mesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Zoogis/%s/SM_%s.SM_%s"), *Id, *Id, *Id));
	// The nubs share Visual's transform (same pivot / scale), so they roll, squash and wobble with the shell.
	Nubs->SetStaticMesh(NubMesh);
	Nubs->SetRelativeTransform(FTransform::Identity);
	Nubs->SetVisibility(NubMesh != nullptr && ZoogiUtil::ShowFlipperNubs());
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
	if (Expression) Expression->Setup();

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
	// Rest pose for the visual hooks (Offset == -Origin * Scale, so scaling about the bounds centre keeps the model centred).
	BaseVisualRot = FRotator(0.f, Def.ModelYaw, 0.f).Quaternion();
	BaseVisualScale = FVector(Scale);
	BaseVisualOrigin = Offset.IsNearlyZero() ? FVector::ZeroVector : B.Origin;
	ResetVisualHooks();
}

// ------------------------------------------------------------------ visual hooks (item 4)

float AZoogiBall::HookGain()
{
	static float Gain = -1.f;
	if (Gain < 0.f)
	{
		Gain = 1.f;
		const TCHAR* Cmd = FCommandLine::Get();
		FParse::Value(Cmd, TEXT("ZoogiHookGain="), Gain);
		if (FParse::Param(Cmd, TEXT("ZoogiNoHooks"))) Gain = 0.f;
		Gain = FMath::Clamp(Gain, 0.f, 4.f);
	}
	return Gain;
}

void AZoogiBall::PulseCore(float Strength)
{
	if (Artifact) Artifact->Pulse(Strength); // the artifact pulse also calls ShellExpand
	else ShellExpand(Strength);
}

void AZoogiBall::Squash(float Amount, const FVector& WorldDir)
{
	const float G = HookGain();
	if (G <= 0.f || WorldDir.IsNearlyZero() || bOut) return;
	SquashAmount = FMath::Clamp(Amount * G, -0.15f * FMath::Max(1.f, G), 0.15f * FMath::Max(1.f, G));
	SquashDirWorld = WorldDir.GetSafeNormal();
	SquashAge = 0.f;
	bHooksDirty = true;
}

void AZoogiBall::SpinBoost(float DegPerSec, const FVector& WorldAxis)
{
	const float G = HookGain();
	if (G <= 0.f || WorldAxis.IsNearlyZero() || bOut) return;
	SpinAxisWorld = WorldAxis.GetSafeNormal();
	SpinRate = FMath::Clamp(DegPerSec, -720.f, 720.f) * G;
	bHooksDirty = true;
}

void AZoogiBall::Wobble(float Degrees, const FVector& WorldDir)
{
	const float G = HookGain();
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, FVector(WorldDir.X, WorldDir.Y, 0.f)).GetSafeNormal();
	if (G <= 0.f || Axis.IsNearlyZero() || bOut) return;
	WobbleDeg = FMath::Clamp(Degrees, 0.f, 6.f) * G;
	WobbleAxisWorld = Axis;
	WobbleAge = 0.f;
	bHooksDirty = true;
}

void AZoogiBall::ShellExpand(float Strength)
{
	const float G = HookGain();
	if (G <= 0.f || bOut) return;
	ExpandStrength = FMath::Clamp(Strength, 0.f, 1.5f) * G;
	ExpandAge = 0.f;
	bHooksDirty = true;
}

void AZoogiBall::SetModelScale(float Scale)
{
	Scale = FMath::Clamp(Scale, 0.5f, 1.5f);
	if (FMath::IsNearlyEqual(Scale, ModelScale)) return;
	ModelScale = Scale;
	bHooksDirty = true;
	TickVisualHooks(0.f);
}

void AZoogiBall::SetFaceTarget(bool bActive, const FQuat& WorldFace, float Rate)
{
	bFaceTarget = bActive;
	FaceTargetWorld = WorldFace;
	FaceRate = FMath::Max(Rate, 0.1f);
	if (bActive) bHooksDirty = true;
}

void AZoogiBall::ResetVisualHooks()
{
	SpinLocal = FQuat::Identity;
	SpinRate = 0.f;
	SquashAge = WobbleAge = ExpandAge = 10.f;
	FaceAlign = FQuat::Identity;
	bHooksDirty = true;
	TickVisualHooks(0.f);
}

void AZoogiBall::TickVisualHooks(float Dt)
{
	if (!bHooksDirty || !Visual) return;
	Dt = FMath::Min(Dt, 0.05f);
	const FQuat SphereQ = Sphere->GetComponentQuat();
	const FQuat ToLocal = SphereQ.Inverse();
	bool bActive = false;

	// Spin: extra rotation about the world axis, accumulated in the sphere's frame so later rolling stays about the true axis.
	if (FMath::Abs(SpinRate) > 1.f)
	{
		SpinLocal = FQuat(ToLocal.RotateVector(SpinAxisWorld), FMath::DegreesToRadians(SpinRate * Dt)) * SpinLocal;
		SpinLocal.Normalize();
		SpinRate *= FMath::Exp(-Dt / 0.35f);
		bActive = true;
	}
	else SpinRate = 0.f;

	// Wobble: decaying ~6 Hz tilt.
	FQuat WobbleLocal = FQuat::Identity;
	WobbleAge += Dt;
	if (WobbleAge < 0.6f)
	{
		const float Ang = WobbleDeg * FMath::Exp(-WobbleAge / 0.18f) * FMath::Sin(UE_TWO_PI * 6.f * WobbleAge);
		WobbleLocal = FQuat(ToLocal.RotateVector(WobbleAxisWorld), FMath::DegreesToRadians(Ang));
		bActive = true;
	}
	// Expression face turn: settle so that SphereQ * SpinLocal * FaceAlign * BaseVisualRot == FaceTargetWorld (wobble aside).
	if (bFaceTarget)
	{
		const FQuat Want = (SpinLocal.Inverse() * ToLocal * FaceTargetWorld * BaseVisualRot.Inverse()).GetNormalized();
		FaceAlign = FQuat::Slerp(FaceAlign, Want, 1.f - FMath::Exp(-Dt * FaceRate)).GetNormalized();
		if (FaceAlign.AngularDistance(Want) > FMath::DegreesToRadians(0.5f)) bActive = true;
	}
	const FQuat Rel = WobbleLocal * SpinLocal * FaceAlign * BaseVisualRot;

	// Squash: the diagonal of the squash matrix I + a (1.5 d d^T - 0.5 I) in the model's own axes (no shear on a rotated mesh).
	FVector S = FVector::OneVector;
	SquashAge += Dt;
	if (SquashAge < 0.15f)
	{
		const float A = SquashAmount * FMath::Sin(PI * SquashAge / 0.15f);
		const FVector D = (SphereQ * Rel).Inverse().RotateVector(SquashDirWorld);
		S = FVector(1.f + A * (1.5f * D.X * D.X - 0.5f), 1.f + A * (1.5f * D.Y * D.Y - 0.5f), 1.f + A * (1.5f * D.Z * D.Z - 0.5f));
		bActive = true;
	}
	// Shell expansion during power-up.
	ExpandAge += Dt;
	float E = 1.f;
	if (ExpandAge < 0.5f)
	{
		E = 1.f + 0.06f * ExpandStrength * FMath::Sin(PI * ExpandAge / 0.5f);
		bActive = true;
	}
	const FVector Scale = BaseVisualScale * S * (E * ModelScale);
	Visual->SetRelativeTransform(FTransform(Rel, -Rel.RotateVector(Scale * BaseVisualOrigin), Scale));
	bHooksDirty = bActive;
	// -ZoogiHookShots: a few frames at the peak of each hook for review (with -ZoogiShots=<dir>).
	static const bool bHookShots = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHookShots"));
	if (bHookShots && Dt > 0.f)
	{
		static int32 NSquash = 0, NExpand = 0, NSpin = 0;
		AZoogiGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		if (GM && SquashAge >= 0.075f && SquashAge - Dt < 0.075f && NSquash < 8)
			GM->TakeShot(FString::Printf(TEXT("hook_%s%d_P%d"), SquashAmount < 0.f ? TEXT("squash") : TEXT("stretch"), ++NSquash, PlayerIndex + 1));
		if (GM && ExpandAge >= 0.25f && ExpandAge - Dt < 0.25f && NExpand < 4) GM->TakeShot(FString::Printf(TEXT("hook_expand%d_P%d"), ++NExpand, PlayerIndex + 1));
		if (GM && SpinRate > 200.f && NSpin < 3 && SquashAge > 0.15f) { GM->TakeShot(FString::Printf(TEXT("hook_spin%d_P%d"), ++NSpin, PlayerIndex + 1)); }
	}
}

void AZoogiBall::TickHookDemo(float Dt)
{
	static const bool bDemo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHookDemo"));
	if (!bDemo || PlayerIndex != 0 || HookDemoStep < 0) return;
	if (HookDemoClock < 0.f)
	{
		if (!bActiveTurn || !IsAtRest()) return;
		HookDemoClock = 0.f;
	}
	HookDemoClock += Dt;
	AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!GM || !PC) return;
	// Camera: 7 radii away, slightly above, looking at the ball centre; follows the ball while the demo runs.
	const FVector Side = FRotator(0.f, AimYaw + 90.f, 0.f).Vector();
	const FVector Eye = BallLocation() + FRotator(0.f, AimYaw + 180.f, 0.f).Vector() * (ZoogiConst::ZoogiRadius * 7.f) + FVector(0, 0, ZoogiConst::ZoogiRadius * 1.6f);
	if (HookDemoCam) HookDemoCam->SetActorLocationAndRotation(Eye, (BallLocation() - Eye).Rotation());
	struct FStep { float T; int32 Kind; const TCHAR* Shot; };
	// Kind: 0 camera on, 1 shot, 2 squash, 3 stretch, 4 expand, 5 spin, 6 wobble, 9 camera off.
	// A screenshot frame takes ~0.2 s, so every trigger sits well after the previous shot, and only one step runs per frame:
	// the shot right after a squash/stretch/wobble trigger lands on the next (normal-length) frame, near the peak.
	static const FStep Steps[] = {
		{ 0.05f, 0, nullptr }, { 1.0f, 1, TEXT("hookdemo_0_rest") },
		{ 1.5f, 2, nullptr }, { 1.53f, 1, TEXT("hookdemo_1_squash_peak") },
		{ 2.0f, 3, nullptr }, { 2.03f, 1, TEXT("hookdemo_2_stretch_peak") },
		{ 2.4f, 4, nullptr }, { 2.65f, 1, TEXT("hookdemo_3_expand_peak") },
		{ 3.0f, 1, TEXT("hookdemo_4_spin_before") }, { 3.3f, 5, nullptr }, { 3.6f, 1, TEXT("hookdemo_5_spin_after") },
		{ 3.9f, 6, nullptr }, { 3.94f, 1, TEXT("hookdemo_6_wobble") },
		{ 4.3f, 9, nullptr } };
	if (HookDemoStep < (int32)UE_ARRAY_COUNT(Steps) && HookDemoClock >= Steps[HookDemoStep].T)
	{
		const FStep& St = Steps[HookDemoStep++];
		switch (St.Kind)
		{
		case 0:
			HookDemoPrevView = PC->GetViewTarget();
			HookDemoCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Eye));
			if (HookDemoCam)
			{
				if (ACameraActor* PrevCam = Cast<ACameraActor>(HookDemoPrevView.Get()))
				{
					HookDemoCam->GetCameraComponent()->PostProcessSettings = PrevCam->GetCameraComponent()->PostProcessSettings; // same look as the game camera
					HookDemoCam->GetCameraComponent()->PostProcessBlendWeight = PrevCam->GetCameraComponent()->PostProcessBlendWeight;
				}
				HookDemoCam->GetCameraComponent()->SetFieldOfView(30.f);
				HookDemoCam->GetCameraComponent()->bConstrainAspectRatio = false;
				HookDemoCam->SetActorLocationAndRotation(Eye, (BallLocation() - Eye).Rotation());
				PC->SetViewTarget(HookDemoCam);
			}
			break;
		case 1: GM->TakeShot(St.Shot); break;
		case 2: Squash(-0.12f, Side); break;
		case 3: Squash(0.08f, Side); break;
		case 4: ShellExpand(1.f); break;
		case 5: SpinBoost(360.f, FVector::CrossProduct(FVector::UpVector, Side)); break;
		case 6: Wobble(5.f, Side); break;
		case 9:
			if (HookDemoPrevView.IsValid()) PC->SetViewTarget(HookDemoPrevView.Get());
			if (HookDemoCam) HookDemoCam->Destroy();
			HookDemoCam = nullptr;
			HookDemoStep = -1;
			UE_LOG(LogZoogi, Log, TEXT("HookDemo done (P1 %s, gain %.2f)"), *Def.Name, HookGain());
			return;
		}
		UE_LOG(LogZoogi, Log, TEXT("HookDemo t=%.2f step %d kind %d"), HookDemoClock, HookDemoStep, St.Kind);
	}
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
	ResetVisualHooks();
}

void AZoogiBall::SetOut()
{
	// Ring-out: whatever stands where the Zoogi flew out (rim ice, wall, arch, stands) takes the crash.
	if (!bOut && GetWorld())
	{
		const FVector V = PrevVelocity.SizeSquared() > 1.f ? PrevVelocity : GetBallVelocity();
		AZoogiArenaPiece::RingOutCrash(GetWorld(), BallLocation(), V, 50.f, this);
	}
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
	if (!V.IsNearlyZero())
	{
		AimYaw = V.Rotation().Yaw;
		// Visual hooks: a quick stretch along the shot and a little extra spin (same sense as the roll).
		const float K = FMath::Clamp(V.Size() / ZoogiConst::FullLaunchSpeed, 0.25f, 1.f);
		Squash(0.08f * K, V);
		SpinBoost(360.f * K, FVector::CrossProduct(FVector::UpVector, V));
		if (HookGain() > 0.f && FParse::Param(FCommandLine::Get(), TEXT("ZoogiHookShots")))
			UE_LOG(LogZoogi, Log, TEXT("Hook launch P%d stretch %.3f spin %.0f deg/s"), PlayerIndex + 1, SquashAmount, SpinRate);
	}
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
	TickHookDemo(DeltaSeconds);
	TickVisualHooks(DeltaSeconds);

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
	if (!bOut && ZoogiUtil::ShowZoogiSparkles()) // Oct 8: [ZoogiRoll.Characters] ZoogiSparkles
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
	// v3 Phase 3: card effects on real contacts (cards on only; nothing runs with bCards=False)
	if (G && G->CardsOn()) if (UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(this)) Cards->FxContact(this, OtherActor, NormalImpulse, Hit);
	if (G && PlayerIndex == Shooter && (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor))) G->OnShooterContact(this, OtherActor);
	if (G && G->bComboMode) G->ComboOnBallHit(this, OtherActor, OtherComp, NormalImpulse, Hit);
	// Guard: the active attacker hit this guarded Zoogi.
	if (G && PlayerIndex != Shooter && Artifact && Artifact->Guard != EZoogiArtifact::None)
	{
		if (AZoogiBall* Att = Cast<AZoogiBall>(OtherActor)) if (Att->PlayerIndex == Shooter) G->OnGuardedHit(this, Att);
	}
	// Destructible rocks take the hit (crack / shatter thresholds live in AZoogiRock).
	if (AZoogiRock* Rock = Cast<AZoogiRock>(OtherActor)) Rock->OnBallHit(this, NormalImpulse, Hit.ImpactPoint);
	// Frozen Ring stage pieces (walls, arches, stands, ramp, platform, obelisks, crystal bumpers ...) take the hit too.
	if (AZoogiArenaPiece* Piece = Cast<AZoogiArenaPiece>(OtherActor)) Piece->OnBallHit(this, NormalImpulse, Hit.ImpactPoint);
	// Reactive floor: hard Zoogi-vs-Zoogi impacts scar the ice (visual only: cracks -> glowing cracks -> cyan crater),
	// an Ember / Titan infused clash also hits the stage pieces around it (power-reactive hook).
	if (AZoogiBall* OB = Cast<AZoogiBall>(OtherActor))
	{
		const float Imp = NormalImpulse.Size();
		const float NowS = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (G && G->Arena && Imp > 12000.f && PlayerIndex < OB->PlayerIndex && NowS - LastScarAt > 0.3f)
		{
			LastScarAt = NowS;
			const EZoogiArtifact E = (Artifact && Artifact->Infused != EZoogiArtifact::None) ? Artifact->Infused : ((OB->Artifact && OB->Artifact->Infused != EZoogiArtifact::None) ? OB->Artifact->Infused : EZoogiArtifact::None);
			const FVector P(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, 0.f);
			if (AZoogiFloorScars* Sc = G->Arena->GetScars()) Sc->Impact(P, Imp / 12000.f, E);
			if (E != EZoogiArtifact::None) AZoogiArenaPiece::DamageArea(GetWorld(), P, 260.f, 22.f, E, this); // Ember melts, Frost refreezes, Storm shocks, Titan crushes
		}
	}
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
	// Visual hooks: compress along the contact normal and wobble on hard hits (marbles, orbs, rocks, walls, hard landings).
	if (NormalImpulse.Size() > 3000.f && Now - LastSquashTime > 0.12f && !Hit.ImpactNormal.IsNearlyZero())
	{
		LastSquashTime = Now;
		const float K = FMath::Clamp((NormalImpulse.Size() - 3000.f) / 25000.f, 0.f, 1.f);
		Squash(-(0.04f + 0.08f * K), Hit.ImpactNormal);
		Wobble(1.5f + 3.f * K, Hit.ImpactNormal);
		if (Expression) Expression->OnImpact(NormalImpulse.Size(), OtherActor);
		if (HookGain() > 0.f && FParse::Param(FCommandLine::Get(), TEXT("ZoogiHookShots")))
			UE_LOG(LogZoogi, Log, TEXT("Hook impact P%d squash %.3f (impulse %.0f, other %s)"), PlayerIndex + 1, SquashAmount, NormalImpulse.Size(), OtherActor ? *OtherActor->GetName() : TEXT("-"));
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
