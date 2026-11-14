#include "ZoogiBall.h"
#include "ZoogiActionFun.h"
#include "ZoogiRoyale.h"
#include "ZoogiMorph.h"
#include "ZoogiIceWall.h"
#include "ZoogiIceTrail.h"
#include "ZoogiArtifacts.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiHaptics.h"
#include "ZoogiFeel.h"
#include "ZoogiAirMarker.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ZoogiOrb.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiFX.h"
#include "NiagaraComponent.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFloorScars.h"
#include "ZoogiCrackFX.h"
#include "ZoogiArena.h"
#include "ZoogiFront.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiExpressionComponent.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
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
		// Oct 21: game-ready Tripo models first (Scripts/import_tripo_characters.py: shell + 12-expression eye decal, nubs
		// separate); -ZoogiBibleModels keeps the older Bible models.
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiBibleModels")))
		{
			Mesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Tripo/%s/SM_%s.SM_%s"), *A, *A, *A));
			if (Mesh) NubMesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Tripo/%s/SM_%s_Nubs.SM_%s_Nubs"), *A, *A, *A));
		}
		if (!Mesh)
		{
			Mesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Bible/%s/SM_%s.SM_%s"), *A, *A, *A));
			NubMesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Characters/Bible/%s/SM_%s_Nubs.SM_%s_Nubs"), *A, *A, *A));
		}
	}
	if (!Mesh) Mesh = ZoogiUtil::LoadMesh(FString::Printf(TEXT("/Game/Zoogis/%s/SM_%s.SM_%s"), *Id, *Id, *Id));
	// The nubs share Visual's transform (same pivot / scale), so they roll, squash and wobble with the shell.
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNubs"))) NubMesh = nullptr; // b150a (Evan): "delete the left over arms from old models on all characters"
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
	// Nov 1 (Evan's buzzy ball forms): a Zoogi with a ball design rolls as that ball (M_ZoogiBallSkin + T_Ball_<name>, Scripts/royale/make_ball_kit.py);
	// -ZoogiOldBalls = the character models again
	bool bBallSkin = false;
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBalls")))
	{
		const FString N = Def.Name.ToLower();
		// same pairing as the Aurex colour palettes (ZoogiMorph.cpp AurexPalette): pinpoint = Frosti, nightshade = Vex, wraps = Zephyr, sera = Ember, tavi = Bolt
		const TCHAR* TN = (N == TEXT("darkzoogy") || N == TEXT("dark")) ? TEXT("T_Ball_Dark") : (N.Contains(TEXT("fros")) || N == TEXT("pinpoint")) ? TEXT("T_Ball_Frosti") : (N == TEXT("luma") || N == TEXT("lumi")) ? TEXT("T_Ball_Lumi")
			: (N == TEXT("vex") || N == TEXT("nightshade") || N.Contains(TEXT("dark"))) ? TEXT("T_Ball_Vex")
			: N == TEXT("hotstreak") ? TEXT("T_Ball_Hotstreak") : N == TEXT("blue") ? TEXT("T_Ball_Blue") : (N == TEXT("terra") || N == TEXT("moss")) ? TEXT("T_Ball_Terra")
			: (N == TEXT("zephyr") || N == TEXT("wraps")) ? TEXT("T_Ball_Zephyr") : (N == TEXT("ember") || N == TEXT("sera")) ? TEXT("T_Ball_Ember")
			: (N == TEXT("bolt") || N == TEXT("tavi")) ? TEXT("T_Ball_Bolt") : nullptr;
		// Nov 1 b52: the player's own ball rolls in the skin equipped in the new INVENTORY (BALLS tab)
		FString OverrideTN;
		if (!bInCPU && ZoogiFront::On()) if (const UZoogiFrontProfileSubsystem* FPs = UZoogiFrontProfileSubsystem::Get(this)) if (!FPs->Data().EquippedBall.IsNone()) OverrideTN = TEXT("T_Ball_") + FPs->Data().EquippedBall.ToString();
		{ FString FilmBall; if (!bInCPU && FParse::Value(FCommandLine::Get(), TEXT("ZoogiAwakenBall="), FilmBall)) OverrideTN = TEXT("T_Ball_") + FilmBall; } // b101 film
		if (!OverrideTN.IsEmpty()) TN = *OverrideTN;
		UMaterialInterface* BM = TN ? LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Zoogis/Balls/M_ZoogiBallSkin.M_ZoogiBallSkin"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		UTexture* BT = TN ? LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Balls/%s.%s"), TN, TN), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		if (BM && BT)
		{
			Visual->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
			UMaterialInstanceDynamic* BMID = UMaterialInstanceDynamic::Create(BM, this);
			BMID->SetTextureParameterValue(TEXT("Tex"), BT);
			for (int32 m = 0; m < Visual->GetNumMaterials(); ++m) Visual->SetMaterial(m, BMID);
			Nubs->SetVisibility(false);
			FitVisual();
			bBallSkin = true;
		}
	}
	if (Expression && !bBallSkin) Expression->Setup();

	const FLinearColor PC = ZoogiUtil::PlayerColor(PlayerIndex);
	Ring->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
	RingMID = ZoogiUtil::GlowMID(this, PC, 2.f);
	Ring->SetMaterial(0, RingMID);
	Ring->SetRelativeScale3D(FVector(1.25f, 1.25f, 0.01f));
	Ring->SetRelativeLocation(FVector(0, 0, -ZoogiConst::ZoogiRadius + 1.5f));
	TurnLight->SetLightColor(PC);
	// b62 (Evan: classic uses the Tripo models only; the bought Aurex stays in Battle Royale, which enables the morph itself): no Aurex body
	// on Blue's ball in classic (its arms were showing). -ZoogiClassicMorph = the old auto morph.
	if (UZoogiMorphComponent::WantsModel(Def) && FParse::Param(FCommandLine::Get(), TEXT("ZoogiClassicMorph"))) EnableMorph();
}

void AZoogiBall::EnableMorph()
{
	if (Morph || !UZoogiMorphComponent::ModelAvailable()) return;
	Morph = NewObject<UZoogiMorphComponent>(this, TEXT("Morph"));
	Morph->RegisterComponent();
	AddInstanceComponent(Morph);
	if (!Morph->Setup(this))
	{
		Morph->DestroyComponent();
		Morph = nullptr;
	}
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
		// Tripo models are normalised to 0.97 m (97 cm after import).
		Scale = ZoogiConst::ZoogiRadius / (Mesh->GetPathName().StartsWith(TEXT("/Game/Characters/Tripo/")) ? 97.f : 95.8f);
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

void AZoogiBall::SetAimSpin(float DegPerSec, const FVector& WorldAxis)
{
	if (bOut) { AimSpinHold = 0.f; return; }
	if (FMath::Abs(DegPerSec) < 1.f || WorldAxis.IsNearlyZero())
	{
		// Blend out: keep SpinRate so TickVisualHooks exponential decay runs (Sonic curl winds down on release).
		if (FMath::Abs(AimSpinHold) > 1.f) SpinRate = AimSpinHold;
		AimSpinHold = 0.f;
		bHooksDirty = true;
		return;
	}
	// Oct 23 (Evan t781u): Sonic pullback spin is Feel mesh-only � never gate on HookGain / -ZoogiNoHooks.
	SpinAxisWorld = WorldAxis.GetSafeNormal();
	AimSpinHold = FMath::Clamp(DegPerSec, -2160.f, 2160.f);
	SpinRate = AimSpinHold;
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
	// Oct 23 (Evan t771u): kill shell expand / inflate visual (power-up / PulseCore / artifact pulse).
	(void)Strength;
	ExpandStrength = 0.f;
	ExpandAge = 10.f;
}

void AZoogiBall::SetModelScale(float Scale)
{
	// Oct 23 (Evan t771u): kill persistent grow (Titan 1.04 / Heavy Roll card model scale). Keep mesh at fit size.
	(void)Scale;
	if (FMath::IsNearlyEqual(ModelScale, 1.f)) return;
	ModelScale = 1.f;
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
	AimSpinHold = 0.f;
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
	// Aim hold (Sonic curl while pulled back) sustains rate; otherwise SpinBoost decays over ~0.35 s.
	if (FMath::Abs(AimSpinHold) > 1.f)
	{
		SpinLocal = FQuat(ToLocal.RotateVector(SpinAxisWorld), FMath::DegreesToRadians(AimSpinHold * Dt)) * SpinLocal;
		SpinLocal.Normalize();
		SpinRate = AimSpinHold;
		bActive = true;
		TireSmokeT -= Dt; // b229: burnout smoke pours out behind the spinning Zoogi while you aim
		if (TireSmokeT <= 0.f) { const float K = FMath::Clamp(FMath::Abs(AimSpinHold) / 1200.f, 0.15f, 1.f); TireSmokeT = FMath::Lerp(0.18f, 0.06f, K); TireSmoke(K, 1); }
	}
	else if (FMath::Abs(SpinRate) > 1.f)
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
	// Oct 24 (Evan, Sonic spin): the face turn solved Want with SpinLocal.Inverse(), so while aiming (face-the-arrow target
	// active) it slerped back against the curl every frame - the spin stuttered / barely turned. Hold the face while spinning.
	if (bFaceTarget && FMath::Abs(SpinRate) <= 1.f && FMath::Abs(AimSpinHold) <= 1.f)
	{
		const FQuat Want = (SpinLocal.Inverse() * ToLocal * FaceTargetWorld * BaseVisualRot.Inverse()).GetNormalized();
		FaceAlign = FQuat::Slerp(FaceAlign, Want, 1.f - FMath::Exp(-Dt * FaceRate)).GetNormalized();
		if (FaceAlign.AngularDistance(Want) > FMath::DegreesToRadians(0.5f)) bActive = true;
	}
	const FQuat Rel = WobbleLocal * SpinLocal * FaceAlign * BaseVisualRot;

	// Squash: the diagonal of the squash matrix I + a (1.5 d d^T - 0.5 I) in the model's own axes (no shear on a rotated mesh).
	FVector S = FVector::OneVector;
	SquashAge += Dt;
	if (SquashAge < 0.22f)
	{
		const float A = SquashAmount * FMath::Sin(PI * SquashAge / 0.22f);
		const FVector D = (SphereQ * Rel).Inverse().RotateVector(SquashDirWorld);
		S = FVector(1.f + A * (1.5f * D.X * D.X - 0.5f), 1.f + A * (1.5f * D.Y * D.Y - 0.5f), 1.f + A * (1.5f * D.Z * D.Z - 0.5f));
		bActive = true;
	}
	// Oct 23 (Evan t771u): no shell expand / ModelScale grow — visual stays at BaseVisualScale * squash only.
	ExpandAge += Dt;
	const float E = 1.f;
	const FVector Scale = BaseVisualScale * S * (E * 1.f);
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
	FlailHideAt = -1.f;
	bParked = false;
	SetActorHiddenInGame(false);
	// Empty-cabin DropIntro (ab1521e) hides Visual/Nubs via SetVisibility(false). Actor unhide alone
	// does not restore component visibility � restore match meshes whenever placing into play.
	if (Visual)
	{
		Visual->SetVisibility(true);
		Visual->SetHiddenInGame(false);
	}
	if (Nubs) Nubs->SetVisibility(Nubs->GetStaticMesh() != nullptr && ZoogiUtil::ShowFlipperNubs());
	Sphere->SetSimulatePhysics(false);
	AZoogiArena::RestoreAfterSink(Sphere); // b179
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorLocationAndRotation(Location, FRotator(0.f, FacingYaw, 0.f), false, nullptr, ETeleportType::ResetPhysics);
	Sphere->SetSimulatePhysics(true);
	Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	AimYaw = FacingYaw;
	ResetVisualHooks();
}

void AZoogiBall::ParkAt(const FVector& Location, float FacingYaw)
{
	bOut = false;
	FlailHideAt = -1.f;
	bOnLaunchPad = false;
	bParked = true;
	SetActorHiddenInGame(false);
	AZoogiArena::RestoreAfterSink(Sphere); // b179
	Sphere->SetSimulatePhysics(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorLocationAndRotation(Location, FRotator(0.f, FacingYaw, 0.f), false, nullptr, ETeleportType::ResetPhysics);
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
		const FVector Exit = BallLocation();
		AZoogiArenaPiece::RingOutCrash(GetWorld(), Exit, V, 50.f, this);
		AZoogiArena::SpawnRingOutRipple(GetWorld(), Exit); // Oct 22 QQ #29
		OutLocation = Exit;
		// Feel 43/27: comic flail + crowd BEFORE bOut (SpinBoost/Wobble refuse after).
		ZoogiFeel::OnKnockOff(this);
		// b140 (Evan: "When a player is knocked out the phone should vibrate for 2s")
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKOBuzz"))) ZoogiHaptics::Rumble(2.f, 1.f, GetWorld(), TEXT("knockout"));
	}
	const bool bWasOut = bOut;
	const FVector SinkV = PrevVelocity.SizeSquared() > 1.f ? PrevVelocity : GetBallVelocity();
	bOut = true;
	bParked = false;
	bOnLaunchPad = false;
	Sphere->SetSimulatePhysics(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (!bWasOut && AZoogiArena::OutsSinkThroughGround(GetWorld())) AZoogiArena::SinkThroughGround(GetWorld(), Sphere, SinkV); // b179: Meadow - falls through the plain
	const float FlailVis = ZoogiFeel::FlailVisibleSeconds();
	if (FlailVis > 0.f && GetWorld())
	{
		// Keep the marble visible briefly so the tumble/squash reads; Tick still drives visual hooks.
		SetActorHiddenInGame(false);
		FlailHideAt = GetWorld()->GetTimeSeconds() + FlailVis;
	}
	else
	{
		SetActorHiddenInGame(true);
		FlailHideAt = -1.f;
	}
}

void AZoogiBall::SetActiveTurn(bool bActive)
{
	bActiveTurn = bActive;
	TurnLight->SetIntensity(0.f); // Oct 24 (Evan): no active-turn point light - it bloomed into a blinding white glow around the Zoogi; the ring is the marker
	if (!bActive) TurnLight->SetLightColor(ZoogiUtil::PlayerColor(PlayerIndex));
	if (RingMID) RingMID->SetScalarParameterValue(TEXT("Strength"), bActive ? 2.2f : 1.2f); // subtle readable marker, under the bloom threshold
	Ring->SetRelativeScale3D(bActive ? FVector(1.35f, 1.35f, 0.01f) : FVector(1.2f, 1.2f, 0.01f));
}

void AZoogiBall::Launch(const FVector2D& PlanarVelocity)
{
	if (bOut) return;
	FlickSpinYaw = 0.f; // cleared; ApplyFlickSide may re-arm after FireShot
	Sphere->WakeRigidBody();
	const FVector V(PlanarVelocity.X, PlanarVelocity.Y, 0.f);
	Sphere->SetPhysicsLinearVelocity(V);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::CrossProduct(FVector::UpVector, V) / ZoogiConst::ZoogiRadius);
	if (!V.IsNearlyZero())
	{
		AimYaw = V.Rotation().Yaw;
		// Visual hooks: a quick stretch along the shot and a little extra spin (same sense as the roll).
		AimSpinHold = 0.f; // end Sonic aim curl; SpinBoost below blends into the launch roll
		const float K = FMath::Clamp(V.Size() / ZoogiConst::FullLaunchSpeed, 0.25f, 1.f);
		Squash(0.08f * K, V);
		SpinBoost(360.f * K, FVector::CrossProduct(FVector::UpVector, V));
		TireSmoke(K, 3 + FMath::RoundToInt(5.f * K)); // b229: the launch kicks up a burst of tire smoke
		if (HookGain() > 0.f && FParse::Param(FCommandLine::Get(), TEXT("ZoogiHookShots")))
			UE_LOG(LogZoogi, Log, TEXT("Hook launch P%d stretch %.3f spin %.0f deg/s"), PlayerIndex + 1, SquashAmount, SpinRate);
	}
}

void AZoogiBall::TireSmoke(float Amount, int32 Puffs)
{	// b229 (Evan approved the demo reel's "orb spin and tire smoke"): grey-white puffs on the floor behind the Zoogi
	static const bool bNo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTireSmoke"));
	UWorld* W = GetWorld();
	if (bNo || !W || !Sphere || bOut || HookGain() <= 0.f) return;
	FVector Fwd = FVector::CrossProduct(SpinAxisWorld, FVector::UpVector).GetSafeNormal2D();
	if (AimSpinHold < 0.f || (FMath::Abs(AimSpinHold) <= 1.f && SpinRate < 0.f)) Fwd = -Fwd;
	if (Fwd.IsNearlyZero()) Fwd = FRotator(0.f, AimYaw, 0.f).Vector();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Fwd);
	const FVector Base = Sphere->GetComponentLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.75f) - Fwd * (ZoogiConst::ZoogiRadius * 0.9f);
	for (int32 q = 0; q < Puffs; ++q)
		AZoogiFX::SpawnTrail(W, Base - Fwd * FMath::FRandRange(0.f, 40.f + 90.f * Amount) + Side * FMath::FRandRange(-30.f, 30.f) * (1.f + Amount) + FVector(0.f, 0.f, FMath::FRandRange(0.f, 35.f)),
			FLinearColor(0.84f, 0.84f, 0.87f), FMath::FRandRange(45.f, 95.f) * (0.6f + 0.6f * Amount), 1.2f + 0.6f * Amount, 0.6f);
}

void AZoogiBall::ApplyFlickSide(float SideFrac)
{
	// Item 21: gameplay flick curve (not visual SpinBoost). Pure back-pull stays ~0 via dead-zone.
	FlickSpinYaw = ZoogiPhysics::FlickSpinFromSide(SideFrac);
	if (FMath::Abs(FlickSpinYaw) > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogZoogi, Log, TEXT("FEEL flick P%d side %.2f -> spin %.2f rad/s"), PlayerIndex + 1, SideFrac, FlickSpinYaw);
	}
}

bool AZoogiBall::IsAtRest() const
{
	if (bOut || bParked) return true;
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
	if (OrbDampT > 0.f && Sphere && Sphere->IsSimulatingPhysics() && !bOut)
	{
		OrbDampT -= DeltaSeconds;
		FVector V = Sphere->GetPhysicsLinearVelocity();
		bool bSet = false;
		if (V.Z > 60.f) { V.Z = 60.f; bSet = true; }
		const float S2 = FVector2D(V.X, V.Y).Size(), Cap = OrbDampSpeed * 0.85f;
		if (S2 > Cap && S2 > 1.f) { const float K = Cap / S2; V.X *= K; V.Y *= K; bSet = true; }
		if (bSet) Sphere->SetPhysicsLinearVelocity(V);
		if (OrbDampT <= 0.f) OrbDampSpeed = 0.f;
	}
	TickRollSound();
	ZoogiAirMarker::Ensure(GetWorld()); AZoogiCrackFX::EnsureShowcase(GetWorld()); AZoogiIceTrail::EnsureTest(GetWorld()); // Oct 22 cracks: -ZoogiCrackShot only // Oct 22: floor crosshair under airborne Zoogis (one manager per world)
	if (bOut)
	{
		// Feel 43: finish comic flail visual, then hide.
		if (FlailHideAt > 0.f)
		{
			TickVisualHooks(DeltaSeconds);
			if (GetWorld() && GetWorld()->GetTimeSeconds() >= FlailHideAt)
			{
				SetActorHiddenInGame(true);
				FlailHideAt = -1.f;
			}
		}
		return;
	}
	PrevVelocity = GetBallVelocity();
	// Oct 15: a floor on rolling resistance - no zone / artifact stacking may leave a Zoogi sliding forever
	// (Arabian carpet lane decel 0 -> 12 s resolve timeout). [ZoogiRoll.Physics] MinRollingDecel (40 cm/s^2), -ZoogiMinDecel=
	static const float MinDecel = []
	{
		float V = 40.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.Physics"), TEXT("MinRollingDecel"), V, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiMinDecel="), V);
		UE_LOG(LogZoogi, Log, TEXT("ROLLING floor: min rolling decel %.0f cm/s^2"), V);
		return FMath::Max(0.f, V);
	}();
	if (RollingDecel < MinDecel)
	{
		UE_LOG(LogZoogi, Warning, TEXT("ROLLING floor: P%d decel %.1f -> %.0f"), PlayerIndex + 1, RollingDecel, MinDecel);
		RollingDecel = MinDecel;
	}
	// Battle Royale drive (ZoogiRoyale.h): accelerate along the stick, brake harder against the roll, a little grip for
	// steering under the speed cap; never pushes past DriveMaxSpeed (knockback still can). Rolling resistance below still applies.
	if (DriveAccel > 0.f && !DriveInput.IsNearlyZero(0.02f) && Sphere->IsSimulatingPhysics())
	{
		const FZoogiRoyaleTune& RT = FZoogiRoyaleTune::Get();
		const FVector V = Sphere->GetPhysicsLinearVelocity();
		FVector2D P(V.X, V.Y);
		const float Before = P.Size();
		const float Mag = FMath::Min(DriveInput.Size(), 1.f);
		const FVector2D Dir = DriveInput.GetSafeNormal();
		const bool bGround = ZoogiPhysics::IsGrounded(Sphere, ZoogiConst::ZoogiRadius);
		float Acc = (DriveAccel + RollingDecel) * Mag * (bGround ? 1.f : RT.AirControl);
		if (FVector2D::DotProduct(P, Dir) < 0.f) Acc *= RT.BrakeMul;
		if (Before > DriveMaxSpeed * 1.15f) Acc *= RT.OverSpeedControl; // knocked back faster than the cap: only a little steering
		P += Dir * Acc * DeltaSeconds;
		if (bGround && Before < DriveMaxSpeed * 1.1f)
		{
			const FVector2D Side = P - Dir * FVector2D::DotProduct(P, Dir);
			P -= Side * FMath::Min(1.f, RT.Grip * Mag * DeltaSeconds);
		}
		const float After = P.Size();
		const float Cap = FMath::Max(DriveMaxSpeed, Before);
		if (After > Cap && After > 1.f) P *= Cap / After;
		if (P.Size() < ZoogiConst::RestSpeed * 2.f && bGround) P = Dir * ZoogiConst::RestSpeed * 2.f; // get off the rest snap
		Sphere->WakeRigidBody();
		Sphere->SetPhysicsLinearVelocity(FVector(P.X, P.Y, V.Z));
	}
	ZoogiPhysics::ApplyRolling(Sphere, ZoogiConst::ZoogiRadius, RollingDecel, DeltaSeconds, ZoogiConst::FullLaunchSpeed, Def.MassRatio(), &FlickSpinYaw);
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
	// Shot trail: Free Arrow Trail ribbon (Fab) while post-launch high speed when affinity trail is off.
	// Animations & Powers keeps SpawnTrail element affinity on ArtifactComponent â€” do not double ribbons.
	if (bActiveTurn && !bOut)
	{
		AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		const float Spd = PlanarSpeed();
		TrailClock += DeltaSeconds;
		const bool bInfused = Artifact && Artifact->Infused != EZoogiArtifact::None; // artifact trail replaces the player puffs
		const bool bWantRibbon = GM && GM->Phase == EZoogiPhase::Rolling && Spd > 400.f && !bInfused;
		if (bWantRibbon)
		{
			if (!FastRollRibbon.IsValid())
			{
				// Scale tracks speed; MaxLife 0 so Ball Tick owns lifecycle (pool slot freed via ReleaseArrowTrail).
				const float Scale = FMath::Lerp(0.35f, 0.55f, FMath::Clamp(Spd / ZoogiConst::FullLaunchSpeed, 0.f, 1.f));
				FastRollRibbon = AZoogiFX::ArrowTrail(GetWorld(), this, TEXT("Basic"), Scale, 0.f);
			}
			else if (UNiagaraComponent* R = FastRollRibbon.Get())
			{
				if (!R->IsActive()) R->Activate(true);
				const float Scale = FMath::Lerp(0.35f, 0.55f, FMath::Clamp(Spd / ZoogiConst::FullLaunchSpeed, 0.f, 1.f));
				R->SetWorldScale3D(FVector(Scale));
			}
		}
		else if (FastRollRibbon.IsValid())
		{
			AZoogiFX::ReleaseArrowTrail(FastRollRibbon.Get());
			FastRollRibbon.Reset();
		}
		// Fallback ground puffs only when ribbon unavailable (pack missing / pool full / -ZoogiNoArrowTrail).
		if (!bWantRibbon || !FastRollRibbon.IsValid())
		{
			if (GM && GM->Phase == EZoogiPhase::Rolling && Spd > 250.f && TrailClock > 0.04f && !bInfused)
			{
				TrailClock = 0.f;
				const FVector L = BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f);
				const float K = FMath::Clamp(Spd / 2000.f, 0.f, 1.f);
				AZoogiFX::SpawnTrail(GetWorld(), L, ZoogiUtil::PlayerColor(PlayerIndex), 26.f + 22.f * K, 0.55f, 3.f + 4.f * K);
			}
		}
	}
	else if (FastRollRibbon.IsValid())
	{
		AZoogiFX::ReleaseArrowTrail(FastRollRibbon.Get());
		FastRollRibbon.Reset();
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
		RingMID->SetScalarParameterValue(TEXT("Strength"), 2.f + 0.6f * FMath::Sin(RingPulse * 4.f)); // Oct 24: was 4+-3 (peaked at 7 -> blinding bloom halo)
	}
	// Feel 47: teeter wobble + gasp near the arena rim (tunable TeeterDistCm).
	ZoogiFeel::TickNearRim(this, DeltaSeconds);
}

void AZoogiBall::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Knock-off credit goes to the shooter, also through chains (shooter -> Zoogi -> orb).
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const int32 Shooter = (G && !G->IsRoyaleMatch()) ? G->ActivePlayer : -1; // Battle Royale: nobody is "the shooter"
	// v3 Phase 3: card effects on real contacts (cards on only; nothing runs with bCards=False)
	if (G && G->CardsOn()) if (UZoogiCardSubsystem* Cards = UZoogiCardSubsystem::Get(this)) Cards->FxContact(this, OtherActor, NormalImpulse, Hit);
	if (G && PlayerIndex == Shooter && (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor))) G->OnShooterContact(this, OtherActor);
	if (G) if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) Fun->OnBallContact(this, OtherActor); // build 23: timing tap / parry / boosts
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
	// Item 23: angle-sensitive rails/bumpers (stage only). Head-on snappier, glancing grind. Zoogi-vs-Zoogi untouched.
	ZoogiPhysics::ApplyRailBumperFeel(Sphere, Hit.ImpactNormal, PrevVelocity, OtherActor, OtherComp);
	// Oct 14: Crystal Collapse ice walls (cards on only; a hard enough hit shatters the segment)
	if (AZoogiIceWall* IceWall = Cast<AZoogiIceWall>(OtherActor)) IceWall->OnBallHit(this, OtherComp, NormalImpulse, Hit);
	// Oct 14: HUD frame shake + rumble on hard impacts (a Zoogi pair once, from the lower index). Visual / haptic only.
	// Oct 22: device vibration on every hit, scaled by impulse and throttled (ZoogiHaptics); runs first so the frame-shake
	// rumble of the same contact is absorbed by the throttle (no double buzz).
	if (G && (!Cast<AZoogiBall>(OtherActor) || PlayerIndex < Cast<AZoogiBall>(OtherActor)->PlayerIndex) && FMath::Abs(Hit.ImpactNormal.Z) < 0.6f)
	{
		ZoogiHaptics::Hit(NormalImpulse.Size(), GetWorld(), Cast<AZoogiBall>(OtherActor) ? TEXT("hit zoogi") : TEXT("hit stage"));
		ZoogiFramed::ImpactShake(NormalImpulse.Size(), GetWorld());
		// Oct 22 quality feel: hit-stop + impulse camera shake + per-weight collision sounds
		ZoogiFeel::OnHardContact(this, OtherActor, NormalImpulse.Size(), Hit);
	}
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
			AZoogiCrackFX::OnImpact(GetWorld(), Hit.ImpactPoint, Imp, E); // Oct 22 cracks: A impact crater (map tint) / B element tint when Awakened; visual only
			if (E != EZoogiArtifact::None) AZoogiArenaPiece::DamageArea(GetWorld(), P, 260.f, 22.f, E, this); // Ember melts, Frost refreezes, Storm shocks, Titan crushes
		}
	}
	// Oct 22 cracks (A): hard landing on the floor after being airborne -> map-tinted crater (visual only, destruction untouched)
	if (!Cast<AZoogiBall>(OtherActor) && !Cast<AZoogiOrb>(OtherActor) && Hit.ImpactNormal.Z > 0.7f && PrevVelocity.Z < 0.f) AZoogiCrackFX::OnLanding(this, Hit.ImpactPoint, -PrevVelocity.Z);
	// Impact spark on hard hits between marbles/orbs (Oct 22 QQ #13/#26: map-tinted particles + fading mark).
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if ((Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor)) && NormalImpulse.Size() > 3500.f && Now - LastImpactFx > 0.15f)
	{
		LastImpactFx = Now;
		AZoogiArena::PlayMapImpact(GetWorld(), Hit.ImpactPoint, Hit.ImpactNormal, NormalImpulse.Size());
		// Oct 22: per-weight collision sounds live in ZoogiFeel::PlayWeightCollision (OnHardContact). Keep spark FX above.
		// Oct 23: Easy Shockwaves VFX â€” Small/Normal/Big by impact Mag, tinted with this Zoogi's colour.
		{
			const FLinearColor Tint = ZoogiUtil::PlayerColor(PlayerIndex);
			AZoogiFX::ShockwaveByMag(GetWorld(), Hit.ImpactPoint, NormalImpulse.Size(), &Tint, false, false);
		}
	}
	// Visual hooks: compress along the contact normal and wobble on hard hits (marbles, orbs, rocks, walls, hard landings).
	if (NormalImpulse.Size() > 2200.f && Now - LastSquashTime > 0.10f && !Hit.ImpactNormal.IsNearlyZero())
	{
		LastSquashTime = Now;
		const float K = FMath::Clamp((NormalImpulse.Size() - 2200.f) / 22000.f, 0.f, 1.f);
		Squash(-(0.07f + 0.12f * K), Hit.ImpactNormal); // Oct 22 queue #15: clearer squash/stretch on impacts
		Wobble(1.5f + 3.f * K, Hit.ImpactNormal);
		if (Expression) Expression->OnImpact(NormalImpulse.Size(), OtherActor);
		if (HookGain() > 0.f && FParse::Param(FCommandLine::Get(), TEXT("ZoogiHookShots")))
			UE_LOG(LogZoogi, Log, TEXT("Hook impact P%d squash %.3f (impulse %.0f, other %s)"), PlayerIndex + 1, SquashAmount, NormalImpulse.Size(), OtherActor ? *OtherActor->GetName() : TEXT("-"));
	}
	if (G && G->IsRoyaleMatch()) { G->RoyaleOnBallHit(this, OtherActor, NormalImpulse, Hit); return; } // knockback boost + ring-out credit
	const int32 Credit = (PlayerIndex == Shooter) ? PlayerIndex : LastHitPlayer;
	if (AZoogiBall* Other = Cast<AZoogiBall>(OtherActor))
	{
		if (Credit >= 0 && Other->PlayerIndex != Credit) Other->LastHitPlayer = Credit;
	}
	else if (AZoogiOrb* Orb = Cast<AZoogiOrb>(OtherActor))
	{
		if (Credit >= 0) Orb->LastHitBy = Credit;
		// b62 (Evan: "After a hit the players shouldn't fly away like they are now after clashing with an orb"): the heavier Zoogi follows
		// through - for a moment it can't come out faster than it went in, and it doesn't hop off the floor
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOrbBounceOld"));
		if (!bOff) { OrbDampT = 0.15f; OrbDampSpeed = FMath::Max(OrbDampSpeed * (OrbDampT > 0.f ? 1.f : 0.f), PrevVelocity.Size2D()); }
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
		// b59 (Evan: realistic sounds): the modelled rolling rumble first (-ZoogiSynthSfx = the old synth loop)
		static const bool bSynth = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSynthSfx"));
		USoundBase* S = bSynth ? nullptr : LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Sounds/Real/SW_RollLoop_Real.SW_RollLoop_Real"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!S) S = ZoogiUtil::LoadSound(TEXT("RollLoop"));
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
