#include "ZoogiClone.h"
#include "ZoogiSkills.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiScreenFx.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ZoogiWolfPack.h"
#include "ZoogiObstacle.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "ZoogiSkillLab.h"
#include "Misc/Parse.h"

static constexpr float CloneRadius = 38.f;
static constexpr float CloneLife = 3.2f;
// Oct 29 detonation: once it has hit something it may roll on until it settles (the turn already waits for clones)
static constexpr float CloneLifeAfterHit = 5.5f;
static constexpr float StillSpeed = 140.f;   // planar cm/s that counts as "momentum stopped"
static constexpr float ShockR = 430.f;       // orbs inside this radius are thrown out

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

void AZoogiClone::InitClone(UStaticMesh* WolfMesh, AActor* InTarget, int32 InOwnerPlayer, float InSpeed, FLinearColor InTint)
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
	Tint = InTint;
	MID = ZoogiUtil::FxMID(this, Tint, 9.f);
	for (int32 i = 0; i < Visual->GetNumMaterials(); ++i) Visual->SetMaterial(i, MID);
	// Nov 1 Wolf Pack 2.0: a glowing tail streak that trails the run
	if (ZoogiWolf::On() && !Tail)
	{
		Tail = NewObject<UStaticMeshComponent>(this, TEXT("Tail"));
		Tail->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
		Tail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Tail->SetCastShadow(false);
		Tail->SetUsingAbsoluteRotation(true);
		Tail->SetupAttachment(Sphere);
		Tail->RegisterComponent();
		Tail->SetMaterial(0, ZoogiUtil::FxMID(this, Tint, 6.f));
		Tail->SetVisibility(false);
	}
	if (ZoogiWolf::On()) SetupBody();
}

// ------------------------------------------------------------------ Micro Werewolf body
namespace
{
	const TCHAR* MWDir = TEXT("/Game/Characters/MicroWerewolf/");
	template<class T> T* MWLoad(const TCHAR* Name) { return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s%s.%s"), MWDir, Name, Name)); }
	float MWYawOffset() { static float Y = [] { float V = 180.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWolfYaw="), V); return V; }(); return Y; }
	/** b88 (Evan: "bring back the wolf from the micro unit pack and make them purple holograms - they seek your target and explode on
	 *  impact with that target"): Micro Werewolf in every mode, purple glowing hologram, explodes on its target. -ZoogiOldWolfClones = b87. */
	bool HoloWolves() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWolfClones")); return b; }
	const FLinearColor HoloPurple(0.62f, 0.3f, 1.f);
}

bool AZoogiClone::SetupBody()
{
	if (Body || FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWolfMesh"))) return Body != nullptr;
	// b62 (Evan: "Classic use the 3d models we made in Tripo, not the models I purchased - use those only in Battle Royale"): the bought
	// Micro Werewolf only runs in Battle Royale; classic keeps the original wolves (-ZoogiBoughtWolvesClassic = old b54 behaviour)
	{
		const AZoogiGameMode* Gm = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		if (Gm && !Gm->bRoyaleMode && !HoloWolves() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoughtWolvesClassic"))) return false;
	}
	USkeletalMesh* SK = MWLoad<USkeletalMesh>(TEXT("SK_MicroWerewolf"));
	if (!SK) return false;
	Body = NewObject<USkeletalMeshComponent>(this, TEXT("Body"));
	Body->SetSkeletalMesh(SK);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
	Body->SetUsingAbsoluteRotation(true);
	Body->SetupAttachment(Sphere);
	Body->RegisterComponent();
	// fit: about 1.35x the marble so it reads as a character, feet on the ground under the sphere
	const FBoxSphereBounds B = SK->GetBounds();
	const float H = FMath::Max(2.f * B.BoxExtent.Z, 1.f);
	BodyFit = (2.f * CloneRadius * 1.35f) / H;
	BodyBase = FVector(0.f, 0.f, -CloneRadius - (B.Origin.Z - B.BoxExtent.Z) * BodyFit);
	Body->SetRelativeScale3D(FVector(BodyFit * BodyScale));
	Body->SetRelativeLocation(BodyBase * BodyScale);
	if (HoloWolves()) { Tint = HoloPurple; Body->SetCastShadow(false); }
	if (UMaterialInterface* M = MWLoad<UMaterialInterface>(TEXT("M_MicroWerewolf")))
	{
		BodyMID = UMaterialInstanceDynamic::Create(M, this);
		BodyMID->SetVectorParameterValue(TEXT("Tint"), Tint);
		BodyMID->SetScalarParameterValue(TEXT("TintAmt"), bDire ? 0.55f : 0.35f);
		BodyMID->SetScalarParameterValue(TEXT("Glow"), 0.12f);
		if (HoloWolves()) { BodyMID->SetScalarParameterValue(TEXT("TintAmt"), 0.92f); BodyMID->SetScalarParameterValue(TEXT("Glow"), 1.5f); BodyMID->SetScalarParameterValue(TEXT("BaseLit"), 0.9f); }
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, BodyMID);
	}
	AnimWalk = MWLoad<UAnimSequence>(TEXT("A_MW_walk"));
	AnimBite = MWLoad<UAnimSequence>(TEXT("A_MW_bite"));
	AnimCharge = MWLoad<UAnimSequence>(TEXT("A_MW_attack2"));
	AnimAir = MWLoad<UAnimSequence>(TEXT("A_MW_fall_idle"));
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	PlayBody(0, true);
	Visual->SetVisibility(false);
	if (Tail) { Tail->DestroyComponent(); Tail = nullptr; } // the streak was for the marble; the werewolf runs with paw prints
	const FVector V = Sphere->GetPhysicsLinearVelocity();
	BodyYaw = FVector2D(V.X, V.Y).SizeSquared() > 100.f ? FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) : GetActorRotation().Yaw;
	return true;
}

void AZoogiClone::PlayBody(int32 Which, bool bForce)
{
	if (!Body || (Which == BodyAnim && !bForce)) return;
	UAnimSequence* A = Which == 1 ? AnimBite : (Which == 2 ? AnimCharge : (Which == 3 ? AnimAir : AnimWalk));
	if (!A) A = AnimWalk;
	if (!A) return;
	BodyAnim = Which;
	Body->PlayAnimation(A, Which != 1);
	if (Which == 1) BiteUntil = Age + A->GetPlayLength() / 1.4f, Body->SetPlayRate(1.4f);
}

void AZoogiClone::UpdateBody(float Dt, float Charge)
{
	if (!Body) return;
	const FVector V = Sphere->GetPhysicsLinearVelocity();
	const float Spd = FVector2D(V.X, V.Y).Size();
	// face the run (the sphere still rolls underneath; the body stays upright)
	if (Spd > 60.f)
	{
		const float Want = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
		BodyYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(BodyYaw, Want), -720.f * Dt, 720.f * Dt);
	}
	Body->SetWorldRotation(FRotator(0.f, BodyYaw + MWYawOffset(), 0.f));
	// a little run bob so the walk cycle reads as a scurry at speed
	const float Bob = Spd > 200.f ? FMath::Abs(FMath::Sin(Age * 22.f)) * 5.f * BodyScale : 0.f;
	Body->SetRelativeLocation(BodyBase * BodyScale + FVector(0.f, 0.f, Bob));
	// anim: bite after a hit, snarl-attack while charged, air pose when falling, otherwise walk (play rate follows speed)
	const bool bAir = V.Z < -350.f;
	if (BodyAnim == 1 && Age < BiteUntil) {}
	else if (ChargeMul > 1.05f) PlayBody(2);
	else if (bAir) PlayBody(3);
	else PlayBody(0);
	if (BodyAnim == 0) Body->SetPlayRate(FMath::Clamp(Spd / 520.f, 0.7f, 2.8f));
	if (BodyAnim == 2) Body->SetPlayRate(1.6f);
	if (BodyMID && HoloWolves())
	{	// hologram: a humming purple glow with the odd flicker frame
		const bool bGlitch = FMath::FRand() < 0.035f && Charge < 0.5f;
		Body->SetVisibility(!bGlitch);
		BodyMID->SetScalarParameterValue(TEXT("Glow"), 1.3f + 0.45f * FMath::Sin(Age * 11.f) + 1.4f * Charge + (bGlitch ? -1.f : 0.f));
	}
	else if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("Glow"), 0.12f + 0.9f * Charge + (ChargeMul > 1.01f ? 0.8f * (ChargeMul - 1.f) * (0.7f + 0.3f * FMath::Sin(Age * 40.f)) : 0.f));
}

void AZoogiClone::SetBodyScale(float S)
{
	BodyScale = S;
	Sphere->SetSphereRadius(CloneRadius * S);
	Visual->SetRelativeScale3D(Visual->GetRelativeScale3D() * S);
	Visual->SetRelativeLocation(Visual->GetRelativeLocation() * S);
	if (Body) { Body->SetRelativeScale3D(FVector(BodyFit * S)); Body->SetRelativeLocation(BodyBase * S); }
	if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("TintAmt"), HoloWolves() ? 0.92f : (bDire ? 0.55f : 0.35f));
	Sphere->SetMassOverrideInKg(NAME_None, 0.7f * ZoogiConst::MassKg * S * S, true);
}

void AZoogiClone::SetMissile(float InSpeed, bool bOrb)
{
	bMissile = true; FormUntil = 0.f; Speed = InSpeed;
	if (bOrb) MakeOrb();
}

void AZoogiClone::Hold()
{
	bWaiting = true; FormUntil = 0.f; Age = 0.f;
	if (Sphere) { Sphere->SetSimulatePhysics(false); Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); } // b119c: it floats with the player
}

void AZoogiClone::Strike(AActor* InTarget)
{
	if (InTarget) Target = InTarget;
	bWaiting = false; Age = 0.f; FormUntil = 0.f;
	if (Sphere) { Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Sphere->SetSimulatePhysics(true); }
	AActor* Tg = Target.Get();
	if (Sphere && Tg)
	{
		FVector D = Tg->GetActorLocation() - GetActorLocation(); D.Z = 0.f;
		Sphere->WakeAllRigidBodies();
		Sphere->SetPhysicsLinearVelocity(D.GetSafeNormal() * Speed);
	}
	AZoogiFX::Spawn(GetWorld(), GetActorLocation(), Tint, bAlpha ? 130.f : 95.f, 0.35f, false, 8.f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF P%d strikes %s"), OwnerPlayer + 1, Tg ? *Tg->GetName() : TEXT("-"));
}

void AZoogiClone::MakeOrb()
{
	// b117 (Evan: "make them purple orbs instead - remove the micro wolf")
	if (Body) { Body->DestroyComponent(); Body = nullptr; BodyMID = nullptr; }
	if (UStaticMesh* Sm = ZoogiUtil::BasicShape(TEXT("Sphere")))
	{
		Visual->SetStaticMesh(Sm);
		const FBoxSphereBounds Bn = Sm->GetBounds();
		const float Sc = (2.f * CloneRadius * BodyScale) / FMath::Max(2.f * Bn.BoxExtent.GetMax(), 1.f);
		Visual->SetRelativeScale3D(FVector(Sc)); Visual->SetRelativeLocation(-Bn.Origin * Sc);
	}
	Tint = FLinearColor(0.62f, 0.3f, 1.f);
	MID = ZoogiUtil::FxMID(this, Tint, 14.f);
	for (int32 i = 0; i < Visual->GetNumMaterials(); ++i) Visual->SetMaterial(i, MID);
	Visual->SetVisibility(true);
	if (Tail) Tail->SetVisibility(false);
}

void AZoogiClone::StartRecall(AZoogiBall* To)
{
	if (bDetonated || !To) return;
	if (bLatched || bLatchPending)
	{	// b95: called off its target - physics back on, it runs home
		bLatched = bLatchPending = false;
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Sphere->SetSimulatePhysics(true);
	}
	Recall = To;
	static const bool bOldRecall = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRecall"));
	if (!bOldRecall && Sphere) Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // b129: runs home through Zoogis - never shoves its caster
	Age = FMath::Min(Age, 1.0f);
	SlowT = 0.f;
}

void AZoogiClone::Pop()
{
	if (bDetonated) return;
	bDetonated = true;
	AZoogiFX::Spawn(GetWorld(), GetActorLocation(), FLinearColor(0.6f, 0.95f, 1.f), 110.f, 0.3f, false, 10.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_White"), GetActorLocation(), FRotator::ZeroRotator, 0.5f);
	if (UZoogiWolfPackSys* W = UZoogiWolfPackSys::Get(this)) W->OnCloneGone(this, false);
	Destroy();
}

void AZoogiClone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (bDetonated) return;
	if (bImpactBlow) { Detonate(TEXT("impact")); return; } // b88
	if (bWaiting)
	{	// b119: parked on the field, humming, until a tap sends it (Strike)
		Age = 0.f;
		if (MID) MID->SetScalarParameterValue(TEXT("Strength"), 12.f + 3.f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 6.f));
		if (Tail) Tail->SetVisibility(false);
		return;
	}
	if (bMissile && !bHit)
	{	// b117: a missile - dead straight at its target (re-aimed every frame so it connects), smoke + embers off the back
		AActor* Tg = Target.Get();
		if (!Tg || Tg->IsHidden()) if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(this)) if (AActor* Nt = Wp->PickReboundTarget(this, nullptr)) { Target = Nt; Tg = Nt; }
		if (!Tg || Tg->IsHidden() || Age > 4.f + ExtraLife || GetActorLocation().Z < ZoogiConst::FallOutZ) { Detonate(TEXT("missile spent")); return; }
		const AZoogiBall* Tb = Cast<AZoogiBall>(Tg); const AZoogiOrb* To = Cast<AZoogiOrb>(Tg);
		const FVector Tp = Tb ? Tb->BallLocation() : (To ? To->OrbLocation() : Tg->GetActorLocation());
		FVector Fly = Tp - GetActorLocation(); Fly.Z = 0.f;
		const float Dist = Fly.Size();
		if (Dist < CloneRadius * BodyScale + (Tb ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius) + 12.f) { bHit = true; bImpactBlow = true; UE_LOG(LogZoogi, Log, TEXT("WOLF missile P%d reaches %s -> boom"), OwnerPlayer + 1, *Tg->GetName()); return; }
		const FVector Dir = Fly / FMath::Max(Dist, 1.f);
		const FVector V0 = Sphere->GetPhysicsLinearVelocity();
		Sphere->SetPhysicsLinearVelocity(Dir * Speed + FVector(0.f, 0.f, FMath::Min(0.f, V0.Z)));
		BodyYaw = Dir.Rotation().Yaw;
		UpdateBody(DeltaSeconds, 0.f);
		PuffT += DeltaSeconds;
		while (PuffT >= 0.045f)
		{
			PuffT -= 0.045f;
			const FVector Back = GetActorLocation() - Dir * CloneRadius * BodyScale * 1.4f + FVector(0.f, 0.f, 10.f);
			AZoogiFX::Spawn(GetWorld(), Back, FLinearColor(1.f, 0.45f, 0.12f), (bAlpha || bDire ? 46.f : 32.f), 0.28f, false, 6.f); // the meteor's ember glow
		}
		return;
	}
	if (bLatchPending || bLatched)
	{	// b95 (Evan: "wolves stay at the target unless called"): parked on its target, biting, until SUPERPOWER calls it
		AActor* Tg = Target.Get();
		const AZoogiBall* Tb = Cast<AZoogiBall>(Tg); const AZoogiOrb* To = Cast<AZoogiOrb>(Tg);
		const bool bGone = !Tg || Tg->IsHidden() || (Tb && Tb->bOut) || (To && To->bOut);
		const FVector Tp = Tb ? Tb->BallLocation() : (To ? To->OrbLocation() : (Tg ? Tg->GetActorLocation() : GetActorLocation()));
		if (bLatchPending && !bGone)
		{
			bLatchPending = false; bLatched = true; LatchT = 0.f;
			FVector Off = GetActorLocation() - Tp; Off.Z = 0.f;
			LatchOff = Off.GetSafeNormal() * ((Tb ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius) + CloneRadius * BodyScale + 6.f);
			if (LatchOff.IsNearlyZero()) LatchOff = FVector(1.f, 0.f, 0.f) * 90.f;
			Sphere->SetSimulatePhysics(false);
			Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UE_LOG(LogZoogi, Log, TEXT("WOLF hologram P%d latches onto %s"), OwnerPlayer + 1, *Tg->GetName());
		}
		LatchT += DeltaSeconds;
		if (bGone || LatchT > 6.f) { bLatched = false; Detonate(bGone ? TEXT("target gone") : TEXT("latched time up")); return; }
		SetActorLocation(Tp + LatchOff + FVector(0.f, 0.f, (CloneRadius - (Tb ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius)) + 3.f * FMath::Sin(Age * 9.f)));
		BodyYaw = FMath::RadiansToDegrees(FMath::Atan2(-LatchOff.Y, -LatchOff.X));
		if (BodyAnim != 1 || Age >= BiteUntil) PlayBody(1, true); // keeps biting
		UpdateBody(DeltaSeconds, 0.f);
		if (Body) Body->SetWorldRotation(FRotator(0.f, BodyYaw + MWYawOffset(), 0.f));
		return;
	}
	const float Life = (bHit ? CloneLifeAfterHit : CloneLife) + ExtraLife;
	if (GetActorLocation().Z < ZoogiConst::FallOutZ) { Destroy(); return; } // fell off: no blast
	{
		const FVector V = Sphere->GetPhysicsLinearVelocity();
		SlowT = (Age > 0.8f && FVector2D(V.X, V.Y).Size() < StillSpeed) ? SlowT + FMath::Min(DeltaSeconds, 0.05f) : 0.f; // frame-rate safe (a hitch frame cannot blow it early)
	}
	// charging glow: brightens and flickers faster as it comes to rest, then it blows
	const float Charge = FMath::Clamp(SlowT / 0.18f, 0.f, 1.f);
	if (MID) MID->SetScalarParameterValue(TEXT("Strength"), (9.f + 14.f * Charge + (1.5f + 4.f * Charge) * FMath::Sin(Age * (18.f + 30.f * Charge))) * (ChargeMul > 1.01f ? 1.f + 1.6f * (ChargeMul - 1.f) + 0.4f * FMath::Sin(Age * 40.f) : 1.f));
	UpdateBody(DeltaSeconds, Charge);
	if (ZoogiWolf::On())
	{
		const FVector V = Sphere->GetPhysicsLinearVelocity();
		const FVector Planar(V.X, V.Y, 0.f);
		const float Spd = Planar.Size();
		// tail streak along the run + glowing paw prints
		if (Tail)
		{
			Tail->SetVisibility(Spd > 300.f);
			if (Spd > 300.f)
			{
				const FVector Back = -Planar / Spd;
				const float Rr = CloneRadius * BodyScale;
				Tail->SetWorldLocation(GetActorLocation() + Back * Rr * 1.6f);
				Tail->SetWorldRotation(FRotationMatrix::MakeFromZ(Back).Rotator());
				Tail->SetWorldScale3D(FVector(Rr * 0.012f, Rr * 0.012f, Rr * 0.012f * FMath::Clamp(Spd / 900.f, 1.f, 3.2f)));
			}
		}
		PawT += DeltaSeconds;
		if (Spd > 350.f && PawT > (bDire ? 0.09f : 0.12f))
		{
			PawT = 0.f; PawSide = -PawSide;
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Planar / Spd);
			const FVector Gp(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z - CloneRadius * BodyScale + 3.f);
			AZoogiFX::Spawn(GetWorld(), Gp + Side * (PawSide * 16.f * BodyScale), Tint, 16.f * BodyScale, 0.7f, true, 4.f);
		}
		// recall: dash back to the caster, then settle beside it (it blows when it stops, like any wolf)
		if (AZoogiBall* Rc = Recall.Get())
		{
			FVector To = Rc->BallLocation() - GetActorLocation(); To.Z = 0.f;
			const float D = To.Size();
			const FVector Desired = D > 170.f ? To / D * 2300.f : FVector::ZeroVector;
			const FVector NewV = FMath::VInterpTo(Planar, Desired, DeltaSeconds, D > 170.f ? 8.f : 5.f);
			Sphere->SetPhysicsLinearVelocity(FVector(NewV.X, NewV.Y, V.Z));
			if (SlowT >= 0.18f) { Detonate(TEXT("recalled")); return; }
			if (Age > Life) { Detonate(TEXT("life")); return; }
			return;
		}
		// V formation run before peeling off to the target
		if (Age < FormUntil)
		{
			const FVector NewV = FMath::VInterpTo(Planar, FormDir * FormSpeed, DeltaSeconds, 10.f);
			Sphere->SetPhysicsLinearVelocity(FVector(NewV.X, NewV.Y, V.Z));
			return;
		}
	}
	if (SlowT >= 0.18f) { Detonate(TEXT("stopped")); return; }
	if (Age > Life) { Detonate(TEXT("life")); return; }
	// Steer toward the target until the first hit, then roll freely.
	AActor* T = Target.Get();
	bool bTargetGone = !T || T->IsHidden();
	if (HoloWolves() && bTargetGone && !bHit) // b88: the hunt goes on - next target
		if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(this)) if (AActor* Nt = Wp->PickReboundTarget(this, nullptr)) { Target = Nt; T = Nt; bTargetGone = false; }
	if (!bHit && !bTargetGone && Age < CloneLife - 0.8f && SlowT <= 0.f)
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
	if (bWaiting) return; // b119: a parked wolf is not hunting yet
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
		// b117: a missile blows on a wall too. b118: not on a sibling missile / in its first 0.15 s (b117 blew them up at the Zoogi)
		if (bMissile && !bHit && Hit.ImpactNormal.Z < 0.5f && !Cast<AZoogiClone>(OtherActor) && Age > 0.15f) { bHit = true; bImpactBlow = true; UE_LOG(LogZoogi, Log, TEXT("WOLF missile P%d hits %s -> boom"), OwnerPlayer + 1, OtherActor ? *OtherActor->GetName() : TEXT("?")); return; }
		if (bMissile) return;
		// Nov 1: a wolf bouncing off a bumper / wall howls (side impacts only, not the floor)
		if (ZoogiWolf::On() && Cast<AZoogiObstacle>(OtherActor) && Hit.ImpactNormal.Z < 0.5f && Age - BumpT > 0.5f && Sphere->GetPhysicsLinearVelocity().Size2D() > 400.f)
		{
			BumpT = Age;
			if (UZoogiWolfPackSys* W = UZoogiWolfPackSys::Get(this)) W->OnCloneBump(this);
		}
		return;
	}
	if (!bHit) AZoogiFX::Spawn(GetWorld(), GetActorLocation(), bAlpha ? FLinearColor(1.f, 0.82f, 0.25f) : FLinearColor(0.5f, 0.7f, 1.f), bAlpha || bDire ? 140.f : 90.f, 0.35f, false, 5.f);
	if (ZoogiWolf::On() && !Shoved.Contains(OtherActor) && !Recall.IsValid())
	{
		Shoved.Add(OtherActor);
		// the bite: an extra shove on a rival Zoogi along the run (Alpha x1.5, Dire x2.5)
		if (AZoogiBall* Rv = Cast<AZoogiBall>(OtherActor))
		{
			const FVector D = (Rv->BallLocation() - GetActorLocation()).GetSafeNormal2D();
			Rv->SetPlanarVelocity(Rv->GetBallVelocity() + D * 380.f * HitMul, bDire ? 220.f : 90.f);
		}
		if (UZoogiWolfPackSys* W = UZoogiWolfPackSys::Get(this)) W->OnCloneHit(this, OtherActor);
		// pack level 4: one rebound to a new target
		if (Rebounds > 0 && !bHit)
		{
			--Rebounds;
			if (UZoogiWolfPackSys* W = UZoogiWolfPackSys::Get(this)) if (AActor* N = W->PickReboundTarget(this, OtherActor)) { Target = N; Age = FMath::Min(Age, 0.9f); AZoogiFX::Spawn(GetWorld(), GetActorLocation(), Tint, 100.f, 0.3f, true, 6.f); return; }
		}
	}
	if (bMissile) { if (OtherActor == Target.Get() || Age > 0.12f) { bHit = true; bImpactBlow = true; UE_LOG(LogZoogi, Log, TEXT("WOLF missile P%d hits %s -> boom"), OwnerPlayer + 1, *OtherActor->GetName()); } return; } // b117: impact = boom; b118: not a brush at launch
	PlayBody(1, true); // the werewolf bites whatever it hits
	if (HoloWolves() && !Recall.IsValid())
	{	// b88: it explodes on impact with its own target; anything else it brushes past keeps it hunting
		AActor* Tg = Target.Get();
		static const bool bImpactBlowOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfImpactBlow"));
		if (OtherActor == Tg || !Tg || Tg->IsHidden())
		{
			bHit = true;
			if (bImpactBlowOld || !Tg || Tg->IsHidden()) { bImpactBlow = true; UE_LOG(LogZoogi, Log, TEXT("WOLF hologram P%d hits its target %s -> boom"), OwnerPlayer + 1, *OtherActor->GetName()); }
			else bLatchPending = true; // b95: it stays on the target until called
		}
		return;
	}
	bHit = true;
}

void AZoogiClone::Detonate(const TCHAR* Why)
{
	if (bDetonated) return;
	bDetonated = true;
	UWorld* W = GetWorld();
	const FVector C = GetActorLocation();
	int32 Pushed = 0;
	const FName WolfSk(TEXT("wolf_pack"));
	const float LabStr = ZoogiSkillLab::StrengthOf(WolfSk); // b136 (Evan): the Skill Lab tunes the wolves too
	const float R = ShockR * (ZoogiWolf::On() ? BlastMul * ChargeMul : 1.f) * ZoogiSkillLab::RadiusOf(WolfSk);
	if (AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr)
	{
		// radial blast on the field orbs: strongest at the centre, planar push + a small hop, credited to the caster
		// b129: a recalled wolf's pop beside its caster pushes nothing (the orbs it shoved knocked the caster off)
		const bool bQuietPop = Recall.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRecall"));
		if (!bQuietPop) for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut || !O->Sphere || !O->Sphere->IsSimulatingPhysics()) continue;
			FVector D = O->OrbLocation() - C;
			D.Z = 0.f;
			const float Dist = D.Size();
			if (Dist > R) continue;
			const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
			const float K = 1.f - Dist / R;
			const float Push = (420.f + 1380.f * K) * (ZoogiWolf::On() ? FMath::Sqrt(BlastMul * ChargeMul) : 1.f) * LabStr;
			const FVector V = O->Sphere->GetPhysicsLinearVelocity();
			O->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * Push + FVector(0.f, 0.f, 160.f * K));
			O->LastHitBy = OwnerPlayer;
			++Pushed;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("POWER wolf clone P%d detonates (%s) at (%.0f,%.0f,%.0f) age %.2f: %d orbs pushed"), OwnerPlayer + 1, Why, C.X, C.Y, C.Z, Age, Pushed);
	const FVector Ground(C.X, C.Y, C.Z - CloneRadius + 4.f);
	{	// b139b (Evan): the Skill Lab BLAST / BLAST SIZE picked for wolf_pack goes off where a wolf blows
		const FName Was = ZoogiSkillLab::Current(); ZoogiSkillLab::SetCurrent(FName(TEXT("wolf_pack")));
		if (const TCHAR* Bx = ZoogiSkillLab::ChosenBlast()) if (*Bx) AZoogiFX::Stylized(W, Bx, Ground, 0.35f * ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize());
		ZoogiSkillLab::SetCurrent(Was);
	}
	AZoogiFX::Spawn(W, C, FLinearColor(0.75f, 0.9f, 1.f), 120.f, 0.22f, false, 14.f);
	AZoogiFX::Spawn(W, Ground, Tint, R, 0.55f, true, 10.f);
	AZoogiFX::Shockwave(W, Ground, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Tint, false); // Vefects (PC; no-op where not cooked)
	UZoogiScreenFx::Shockwave(W, Ground, R + 60.f, Tint);
	ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.8f, bDire ? 0.8f : 1.1f);
	AZoogiFX::Stylized(W, bFire ? TEXT("Fire_v2") : ((bDire || ChargeMul > 1.05f) && BlastMul >= 0.99f ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion")), Ground, (bDire ? 0.75f : 0.42f) * FMath::Max(1.f, ChargeMul)); // b100: card wolves (0.6) keep the small blast
	if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(this)) WP->OnCloneGone(this, true);
	Destroy();
}
