// Zoogi Blue transform model on the ball + the robot's plasma bolt (see ZoogiMorph.h).
#include "ZoogiMorph.h"
#include "ZoogiBall.h"
#include "ZoogiRoyale.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "ReferenceSkeleton.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const TCHAR* GMorphDir = TEXT("/Game/ZoogiBlue/Mesh/SK_ZoogiBlue/SkeletalMeshes");
	const TCHAR* GDirNames[8] = { TEXT("F"), TEXT("FR"), TEXT("R"), TEXT("BR"), TEXT("B"), TEXT("BL"), TEXT("L"), TEXT("FL") };
	constexpr float GThrustMinSpeed = 70.f; // cm/s: slower than this the robot stands (Bot_Idle)
	constexpr float GBallCentre = 50.f;     // model origin is the floor, the ball centre is 50 cm up

	USkeletalMesh* LoadMorphMesh()
	{
		static TWeakObjectPtr<USkeletalMesh> Cached;
		static bool bTried = false;
		if (!Cached.IsValid() && !bTried)
		{
			bTried = true;
			Cached = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/SK_ZoogiBlue.SK_ZoogiBlue"), GMorphDir), nullptr, LOAD_NoWarn | LOAD_Quiet);
			UE_LOG(LogZoogi, Log, TEXT("MORPH model %s"), Cached.IsValid() ? TEXT("loaded (SK_ZoogiBlue)") : TEXT("MISSING - old static meshes stay"));
		}
		return Cached.Get();
	}
}

// ------------------------------------------------------------------ UZoogiMorphComponent

UZoogiMorphComponent::UZoogiMorphComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after the sphere moved: the model never lags a frame behind
}

bool UZoogiMorphComponent::ModelAvailable()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoMorph"));
	return !bOff && LoadMorphMesh() != nullptr;
}

bool UZoogiMorphComponent::WantsModel(const FZoogiDef& Def)
{
	return Def.Id == FName(TEXT("blue")) && ModelAvailable();
}

UAnimSequence* UZoogiMorphComponent::Clip(FName Name) const
{
	const TObjectPtr<UAnimSequence>* A = Anims.Find(Name);
	return A ? A->Get() : nullptr;
}

float UZoogiMorphComponent::ClipLength(FName Name) const
{
	const UAnimSequence* A = Clip(Name);
	return A ? A->GetPlayLength() : 0.f;
}

FName UZoogiMorphComponent::DirClip(const TCHAR* Prefix, int32 Dir) const
{
	return FName(*FString::Printf(TEXT("%s_%s"), Prefix, GDirNames[(Dir % 8 + 8) % 8]));
}

// Nov 1 (Evan: "the new characters were not there"): every Royale bot is Mecha Aurex too, coloured per Zoogi (zones R / G / B + glow,
// matched to its ball design), on the rifle clips retargeted by Scripts/royale/aurex_setup.py. -ZoogiNoAurex = the old transforming robot.
static USkeletalMesh* LoadAurexMesh()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAurex"))) return nullptr;
	return LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}
static void AurexPalette(const FString& Name, int32 Player, FLinearColor Z[4])
{
	const FString N = Name.ToLower();
	auto Pal = [Z](FLinearColor Ra, FLinearColor Ga, FLinearColor Ba, FLinearColor Ea) { Z[0] = Ra; Z[1] = Ga; Z[2] = Ba; Z[3] = Ea; };
	if (N.Contains(TEXT("fros")) || N == TEXT("pinpoint")) Pal(FLinearColor(0.12f, 0.42f, 1.f), FLinearColor(0.86f, 0.9f, 0.96f), FLinearColor(0.02f, 0.04f, 0.12f), FLinearColor(0.3f, 0.85f, 1.f));
	else if (N == TEXT("luma") || N == TEXT("lumi")) Pal(FLinearColor(1.f, 0.62f, 0.08f), FLinearColor(0.9f, 0.88f, 0.82f), FLinearColor(0.05f, 0.04f, 0.03f), FLinearColor(1.f, 0.8f, 0.3f));
	else if (N == TEXT("darkzoogy") || N == TEXT("dark")) Pal(FLinearColor(0.05f, 0.07f, 0.32f), FLinearColor(0.22f, 0.23f, 0.27f), FLinearColor(0.02f, 0.02f, 0.03f), FLinearColor(1.f, 0.48f, 0.06f)); // b202 Dark: navy, steel, orange crystals
	else if (N == TEXT("vex") || N.Contains(TEXT("dark")) || N == TEXT("nightshade")) Pal(FLinearColor(0.36f, 0.08f, 0.78f), FLinearColor(0.03f, 0.02f, 0.05f), FLinearColor(0.15f, 0.15f, 0.18f), FLinearColor(0.62f, 0.22f, 1.f));
	else if (N == TEXT("hotstreak")) Pal(FLinearColor(0.03f, 0.02f, 0.02f), FLinearColor(1.f, 0.34f, 0.04f), FLinearColor(0.12f, 0.1f, 0.1f), FLinearColor(1.f, 0.45f, 0.05f));
	else if (N == TEXT("blue")) Pal(FLinearColor(0.05f, 0.2f, 0.78f), FLinearColor(0.26f, 0.28f, 0.33f), FLinearColor(0.02f, 0.03f, 0.1f), FLinearColor(0.2f, 0.6f, 1.f));
	else if (N == TEXT("terra") || N == TEXT("moss")) Pal(FLinearColor(0.16f, 0.2f, 0.13f), FLinearColor(0.2f, 0.55f, 0.12f), FLinearColor(0.05f, 0.05f, 0.04f), FLinearColor(0.3f, 1.f, 0.2f));
	else if (N == TEXT("zephyr") || N == TEXT("wraps")) Pal(FLinearColor(0.04f, 0.72f, 0.74f), FLinearColor(0.8f, 0.82f, 0.86f), FLinearColor(0.02f, 0.12f, 0.14f), FLinearColor(0.3f, 1.f, 0.95f));
	else if (N == TEXT("ember") || N == TEXT("sera")) Pal(FLinearColor(0.85f, 0.1f, 0.55f), FLinearColor(0.2f, 0.04f, 0.15f), FLinearColor(0.05f, 0.02f, 0.04f), FLinearColor(1.f, 0.3f, 0.7f));
	else if (N == TEXT("bolt") || N == TEXT("tavi")) Pal(FLinearColor(1.f, 0.78f, 0.06f), FLinearColor(0.03f, 0.03f, 0.035f), FLinearColor(0.2f, 0.2f, 0.22f), FLinearColor(0.2f, 0.8f, 1.f));
	else { const FLinearColor Pc = ZoogiUtil::PlayerColor(Player); Pal(Pc, FLinearColor(0.82f, 0.84f, 0.88f), FLinearColor(0.03f, 0.03f, 0.04f), Pc); }
}

void ZoogiAurexPalette(const FString& Name, int32 Player, FLinearColor Z[4]) { AurexPalette(Name, Player, Z); }

bool UZoogiMorphComponent::Setup(AZoogiBall* InBall)
{
	Ball = InBall;
	USkeletalMesh* AurexSM = LoadAurexMesh();
	bAurex = AurexSM != nullptr;
	USkeletalMesh* SM = bAurex ? AurexSM : LoadMorphMesh();
	if (!Ball || !SM || !Ball->Sphere) return false;
	if (bAurex)
	{
		// clip names stay the morph model's; each points at an Aurex rifle clip
		auto LoadA = [this](const TCHAR* Key, const TCHAR* Rifle)
		{
			const FString An = FString(TEXT("R_")) + Rifle;
			if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Mecha_Aurex/Anims/Rifle/%s.%s"), *An, *An), nullptr, LOAD_NoWarn | LOAD_Quiet)) Anims.Add(FName(Key), A);
		};
		LoadA(TEXT("Ball_Idle"), TEXT("MM_Rifle_Idle_Hipfire")); LoadA(TEXT("Ball_Roll"), TEXT("MM_Rifle_Idle_Hipfire"));
		LoadA(TEXT("ToBot"), TEXT("MM_Rifle_Jump_Fall_Land")); LoadA(TEXT("ToBall"), TEXT("MM_Rifle_Jump_Start"));
		LoadA(TEXT("Bot_Idle"), TEXT("MM_Rifle_Idle_Hipfire")); LoadA(TEXT("Bot_Aim"), TEXT("MM_Rifle_Idle_ADS")); LoadA(TEXT("Bot_Fire"), TEXT("MM_Rifle_Fire"));
		for (int32 d = 0; d < 8; ++d)
		{
			const TCHAR* Jog = (d == 0 || d == 1 || d == 7) ? TEXT("MM_Rifle_Jog_Fwd") : (d == 2) ? TEXT("MM_Rifle_Jog_Right") : (d == 6) ? TEXT("MM_Rifle_Jog_Left") : TEXT("MM_Rifle_Jog_Bwd");
			for (const TCHAR* Pf : { TEXT("Bot_ThrustStart"), TEXT("Bot_Thrust"), TEXT("Bot_ThrustStop"), TEXT("Bot_ThrustFire") })
				LoadA(*FString::Printf(TEXT("%s_%s"), Pf, GDirNames[d]), Jog);
		}
	}
	static const TCHAR* Names[] = { TEXT("Ball_Idle"), TEXT("Ball_Roll"), TEXT("ToBot"), TEXT("Bot_Idle"), TEXT("Bot_Aim"), TEXT("Bot_Fire"), TEXT("ToBall") };
	auto Load = [this](const FString& N)
	{
		if (UAnimSequence* A = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), GMorphDir, *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet)) Anims.Add(FName(*N), A);
	};
	if (!bAurex)
	{
		for (const TCHAR* N : Names) Load(N);
		for (int32 d = 0; d < 8; ++d)
			for (const TCHAR* P : { TEXT("Bot_ThrustStart"), TEXT("Bot_Thrust"), TEXT("Bot_ThrustStop"), TEXT("Bot_ThrustFire") })
				Load(FString::Printf(TEXT("%s_%s"), P, GDirNames[d]));
	}

	Mesh = NewObject<USkeletalMeshComponent>(Ball, TEXT("MorphMesh"));
	Mesh->SetSkeletalMeshAsset(SM);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(true);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh->SetupAttachment(Ball->Sphere);
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->RegisterComponent();
	Ball->AddInstanceComponent(Mesh);

	if (bAurex)
	{
		// UE4-mannequin rig faces +Y: -90 deg yaw turns it to +X; ~178 cm tall; Zoogi colours on the colour-zone material
		Calib = FRotator(0.f, -90.f, 0.f).Quaternion();
		const float Hm = FMath::Max(10.f, (float)SM->GetBounds().BoxExtent.Z * 2.f);
		Mesh->SetWorldScale3D(FVector(178.f / Hm));
		if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			FLinearColor Z[4];
			AurexPalette(Ball->Def.Name, Ball->PlayerIndex, Z);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, Ball);
			MID->SetVectorParameterValue(TEXT("Color_Zone_R"), Z[0]); MID->SetVectorParameterValue(TEXT("Color_Zone_G"), Z[1]);
			MID->SetVectorParameterValue(TEXT("Color_Zone_B"), Z[2]); MID->SetVectorParameterValue(TEXT("Emissive_Color"), Z[3]);
			for (int32 m = 0; m < Mesh->GetNumMaterials(); ++m) Mesh->SetMaterial(m, MID);
		}
		UE_LOG(LogZoogi, Log, TEXT("MORPH Aurex body for %s (%d clips)"), *Ball->Def.Name, Anims.Num());
	}
	// model forward: the cannons point from the shoulders to the muzzles in the bind pose (robot form)
	else
	{
		const FReferenceSkeleton& RS = SM->GetRefSkeleton();
		auto CS = [&RS](const TCHAR* Bone)
		{
			int32 i = RS.FindBoneIndex(FName(Bone));
			FTransform T = FTransform::Identity;
			while (i != INDEX_NONE) { T = T * RS.GetRefBonePose()[i]; i = RS.GetParentIndex(i); }
			return T.GetLocation();
		};
		const FVector Muz = (CS(TEXT("muzzle_R")) + CS(TEXT("muzzle_L"))) * 0.5f;
		const FVector Sho = (CS(TEXT("shoulder_R")) + CS(TEXT("shoulder_L"))) * 0.5f;
		FVector F = Muz - Sho;
		F.Z = 0.f;
		if (!F.Normalize()) F = FVector::ForwardVector;
		const float FYaw = FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X));
		Calib = FRotator(0.f, -FYaw, 0.f).Quaternion();
		const FVector RightArm = CS(TEXT("muzzle_R")) - CS(TEXT("muzzle_L"));
		const float Side = FVector::DotProduct(RightArm, FVector::CrossProduct(FVector::UpVector, F));
		static bool bLogged = false;
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(LogZoogi, Log, TEXT("MORPH calib: model forward yaw %.1f, right arm on the %s, muzzle height %.0f cm, %d clips"), FYaw,
				Side >= 0.f ? TEXT("right (ok)") : TEXT("LEFT (mirrored)"), Muz.Z, Anims.Num());
		}
	}
	FacingYaw = Ball->AimYaw;
	BallOffset = FRotator(0.f, Ball->Def.ModelYaw, 0.f).Quaternion().Inverse() * Calib; // the Visual carries the def's model yaw
	State = EState::Ball;
	bRobot = false;
	UprightBlend = 0.f;
	Current = NAME_None;
	Play(TEXT("Ball_Idle"), true);
	Place();
	if (Ball->Visual) Ball->Visual->SetHiddenInGame(!bAurex); // Aurex: the ball skin stays as the ball form
	if (Ball->Nubs) Ball->Nubs->SetHiddenInGame(true);
	return true;
}

void UZoogiMorphComponent::Play(FName Name, bool bLoop)
{
	if (!Mesh) return;
	UAnimSequence* A = Clip(Name);
	if (!A) return;
	if (Name == Current && bLoop) return;
	Current = Name;
	Mesh->PlayAnimation(A, bLoop);
	// Oct 27 (Evan, build 9): no delay on TRANSFORM - the ball <-> robot clips play fast (ZoogiRoyaleFeel::MorphSeconds)
	const bool bMorphClip = Name == FName(TEXT("ToBot")) || Name == FName(TEXT("ToBall"));
	const float Rate = bMorphClip ? FMath::Max(1.f, A->GetPlayLength() / ZoogiRoyaleFeel::MorphSeconds) : 1.f;
	Mesh->SetPlayRate(Rate);
	StateLen = A->GetPlayLength() / Rate;
}

void UZoogiMorphComponent::SetRobot(bool bOn, bool bInstant)
{
	if (!Mesh) return;
	if (bInstant)
	{
		bRobot = bOn;
		State = bOn ? EState::BotIdle : EState::Ball;
		StateTime = 0.f;
		UprightBlend = bOn ? 1.f : 0.f;
		FireHold = 0.f;
		Play(bOn ? TEXT("Bot_Idle") : TEXT("Ball_Idle"), true);
		return;
	}
	if (bOn == bRobot) return;
	bRobot = bOn;
	StateTime = 0.f;
	FireHold = 0.f;
	if (bOn) { State = EState::ToBot; Play(TEXT("ToBot"), false); }
	else { State = EState::ToBall; Play(TEXT("ToBall"), false); }
}

FVector UZoogiMorphComponent::GetMuzzle(bool bLeft) const
{
	if (!Mesh) return Ball ? Ball->BallLocation() : FVector::ZeroVector;
	if (bAurex) return Mesh->GetBoneLocation(FName(TEXT("hand_r"))) + FRotator(0.f, FacingYaw, 0.f).Vector() * 45.f;
	return Mesh->GetSocketLocation(bLeft ? FName(TEXT("muzzle_L")) : FName(TEXT("muzzle_R")));
}

FVector UZoogiMorphComponent::Fire(bool bLeft)
{
	if (Mesh && bRobot && !IsMorphing() && !bLeft) // the clips fire right, then left: one clip per pair of shots
	{
		const bool bHover = State == EState::Thrust || State == EState::ThrustStart;
		const FName F = bHover ? DirClip(TEXT("Bot_ThrustFire"), ThrustDir) : FName(TEXT("Bot_Fire"));
		if (UAnimSequence* A = Clip(F))
		{
			Current = F;
			Mesh->PlayAnimation(A, false);
			FireHold = A->GetPlayLength();
		}
	}
	return GetMuzzle(bLeft);
}

void UZoogiMorphComponent::Place()
{
	if (!Mesh || !Ball || !Ball->Sphere) return;
	const FVector C = Ball->Sphere->GetComponentLocation();
	const FQuat Up = FRotator(0.f, FacingYaw, 0.f).Quaternion() * Calib;
	FQuat Q = Up;
	if (UprightBlend < 1.f)
	{
		// ball form: the model rolls with the Visual (sphere + the squash / spin / wobble hooks' rotation)
		const FQuat VisualQ = Ball->Visual ? Ball->Visual->GetComponentQuat() : Ball->Sphere->GetComponentQuat();
		const FQuat Roll = VisualQ * BallOffset;
		Q = FQuat::Slerp(Roll, Up, UprightBlend).GetNormalized();
	}
	const FVector Loc = UprightBlend >= 1.f ? C - FVector(0.f, 0.f, GBallCentre) : C + Q.RotateVector(FVector(0.f, 0.f, -GBallCentre));
	Mesh->SetWorldLocationAndRotation(Loc, Q);
}

void UZoogiMorphComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	if (!Mesh || !Ball) return;
	// the old static mesh stays hidden; its visibility flag (blinks, flails, hides) drives the model
	if (Ball->Visual && bAurex)
	{
		// Aurex: robot = the body, ball = the ball skin on the Visual (the hero's own model hides this body: leave its Visual alone too)
		const bool bBallForm = State == EState::Ball || (State == EState::ToBot && StateTime < 0.06f);
		if (!Mesh->bHiddenInGame) Ball->Visual->SetHiddenInGame(!bBallForm);
		Mesh->SetVisibility(Ball->Visual->GetVisibleFlag() && !bBallForm);
		if (Mesh->bRenderCustomDepth != Ball->Visual->bRenderCustomDepth) Mesh->SetRenderCustomDepth(Ball->Visual->bRenderCustomDepth);
	}
	else if (Ball->Visual)
	{
		Mesh->SetVisibility(Ball->Visual->GetVisibleFlag());
		Ball->Visual->SetHiddenInGame(true); // no-op when already hidden
		if (Mesh->bRenderCustomDepth != Ball->Visual->bRenderCustomDepth) Mesh->SetRenderCustomDepth(Ball->Visual->bRenderCustomDepth);
	}
	if (Ball->Nubs) Ball->Nubs->SetHiddenInGame(true);
	StateTime += Dt;
	if (FireHold > 0.f) FireHold -= Dt;

	const FVector V = Ball->GetBallVelocity();
	const FVector2D V2(V.X, V.Y);
	const bool bMoving = V2.Size() > GThrustMinSpeed;
	auto PickDir = [this, &V2]()
	{
		const float Rel = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(V2.Y, V2.X)) - FacingYaw);
		const int32 D = (FMath::RoundToInt(Rel / 45.f) % 8 + 8) % 8;
		// 8 deg deadband around the current sector edge: diagonals do not flicker
		const float Off = FMath::Abs(FRotator::NormalizeAxis(Rel - ThrustDir * 45.f));
		return Off <= 22.5f + 8.f ? ThrustDir : D;
	};

	switch (State)
	{
	case EState::Ball:
		UprightBlend = 0.f;
		Play(TEXT("Ball_Idle"), true);
		break;
	case EState::ToBot:
		UprightBlend = FMath::Min(1.f, UprightBlend + Dt / 0.12f); // the shell rights itself, then splits open
		if (StateTime >= StateLen) { State = EState::BotIdle; StateTime = 0.f; Play(TEXT("Bot_Idle"), true); }
		break;
	case EState::ToBall:
		UprightBlend = 1.f;
		if (StateTime >= StateLen)
		{
			// closed: from here it rolls with the sphere, starting from the upright pose (no pop)
			const FQuat VisualQ = Ball->Visual ? Ball->Visual->GetComponentQuat() : Ball->Sphere->GetComponentQuat();
			BallOffset = VisualQ.Inverse() * (FRotator(0.f, FacingYaw, 0.f).Quaternion() * Calib);
			UprightBlend = 0.f;
			State = EState::Ball;
			StateTime = 0.f;
			Play(TEXT("Ball_Idle"), true);
		}
		break;
	case EState::BotIdle:
		UprightBlend = 1.f;
		if (bMoving)
		{
			ThrustDir = (FMath::RoundToInt(FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(V2.Y, V2.X)) - FacingYaw) / 45.f) % 8 + 8) % 8;
			State = EState::ThrustStart;
			StateTime = 0.f;
			if (FireHold <= 0.f) Play(DirClip(TEXT("Bot_ThrustStart"), ThrustDir), false);
		}
		else if (FireHold <= 0.f) Play(TEXT("Bot_Idle"), true);
		break;
	case EState::ThrustStart:
		UprightBlend = 1.f;
		if (StateTime >= FMath::Max(StateLen, 0.1f) || FireHold > 0.f) { State = EState::Thrust; StateTime = 0.f; }
		break;
	case EState::Thrust:
		UprightBlend = 1.f;
		if (!bMoving)
		{
			State = EState::ThrustStop;
			StateTime = 0.f;
			if (FireHold <= 0.f) Play(DirClip(TEXT("Bot_ThrustStop"), ThrustDir), false);
			break;
		}
		ThrustDir = PickDir(); // a new direction is a hard cut, like the reference
		if (FireHold <= 0.f) Play(DirClip(TEXT("Bot_Thrust"), ThrustDir), true);
		break;
	case EState::ThrustStop:
		UprightBlend = 1.f;
		if (bMoving) { State = EState::Thrust; StateTime = 0.f; break; }
		if (StateTime >= FMath::Max(StateLen, 0.1f)) { State = EState::BotIdle; StateTime = 0.f; if (FireHold <= 0.f) Play(TEXT("Bot_Idle"), true); }
		break;
	}
	Place();
}

// ------------------------------------------------------------------ AZoogiRoyaleBolt

AZoogiRoyaleBolt::AZoogiRoyaleBolt()
{
	PrimaryActorTick.bCanEverTick = false;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Body);
	for (UStaticMeshComponent* M : { Body.Get(), Core.Get() })
	{
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetGenerateOverlapEvents(false);
		M->SetCastShadow(false);
	}
}

void AZoogiRoyaleBolt::Init(const FLinearColor& Color)
{
	UStaticMesh* S = ZoogiUtil::BasicShape(TEXT("Sphere"));
	Body->SetStaticMesh(S);
	Core->SetStaticMesh(S);
	// Oct 27 (Marble Clash): a long, very thin tracer streak along +X (SetStreak sets the length); a white-hot core inside
	Body->SetWorldScale3D(FVector(0.3f, 0.11f, 0.11f));
	Core->SetRelativeScale3D(FVector(0.92f, 0.45f, 0.45f));
	Body->SetMaterial(0, ZoogiUtil::GlowMID(this, Color, 16.f));
	Core->SetMaterial(0, ZoogiUtil::GlowMID(this, FLinearColor(1.f, 1.f, 1.f), 28.f));
}

void AZoogiRoyaleBolt::SetStreak(float LengthCm, const FVector& Head, const FVector& Dir)
{
	const FVector D = Dir.IsNearlyZero() ? GetActorForwardVector() : Dir.GetSafeNormal();
	const float L = FMath::Max(LengthCm, 10.f);
	SetActorLocationAndRotation(Head - D * (L * 0.5f), D.Rotation()); // the basic sphere is 100 cm across: centre half a length back
	if (Body) Body->SetWorldScale3D(FVector(L / 100.f, 0.11f, 0.11f));
}
