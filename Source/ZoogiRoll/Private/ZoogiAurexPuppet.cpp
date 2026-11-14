// Nov 1 b58 (Evan): posed form puppet - see ZoogiAurexPuppet.h.
#include "ZoogiAurexPuppet.h"
#include "ZoogiStandForms.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "ZoogiActionFun.h"
#include "ZoogiGameMode.h"
#include "ZoogiFX.h"
#include "ZoogiMorph.h"
#include "ZoogiTypes.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ReferenceSkeleton.h"
#include "Misc/App.h"
#include "GameFramework/WorldSettings.h"

AZoogiAurexPuppet::AZoogiAurexPuppet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCastShadow(true);
	Body->bUseBoundsFromLeaderPoseComponent = false;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetRelativeLocation(FVector(120.f, 0.f, 120.f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(45.f);
	Glow->SetAttenuationRadius(520.f);
	Glow->SetCastShadows(false);
	for (int32& I : Idx) I = INDEX_NONE;
}

int32 AZoogiAurexPuppet::Find(std::initializer_list<const TCHAR*> Cands) const
{
	for (const TCHAR* C : Cands)
		for (int32 i = 0; i < Names.Num(); ++i)
			if (Names[i].ToString().Equals(C, ESearchCase::IgnoreCase)) return i;
	return INDEX_NONE;
}

bool AZoogiAurexPuppet::Setup(USkeletalMesh* SM)
{
	if (!SM || !Body) return false;
	Body->SetSkinnedAssetAndUpdate(SM, true);
	const FReferenceSkeleton& RS = SM->GetRefSkeleton();
	const int32 N = RS.GetNum();
	RefLocal = RS.GetRefBonePose();
	Parent.SetNum(N); Names.SetNum(N);
	for (int32 i = 0; i < N; ++i) { Parent[i] = RS.GetParentIndex(i); Names[i] = RS.GetBoneName(i); }
	// UE4 mannequin (Aurex) / Mixamo (Tripo ultimate forms)
	Idx[Spine3] = Find({ TEXT("spine_03"), TEXT("Spine2") });
	Idx[Neck] = Find({ TEXT("neck_01"), TEXT("Neck") });
	Idx[Head] = Find({ TEXT("head"), TEXT("Head") });
	Idx[UArmL] = Find({ TEXT("upperarm_l"), TEXT("LeftArm") });
	Idx[LArmL] = Find({ TEXT("lowerarm_l"), TEXT("LeftForeArm") });
	Idx[HandL] = Find({ TEXT("hand_l"), TEXT("LeftHand") });
	Idx[UArmR] = Find({ TEXT("upperarm_r"), TEXT("RightArm") });
	Idx[LArmR] = Find({ TEXT("lowerarm_r"), TEXT("RightForeArm") });
	Idx[HandR] = Find({ TEXT("hand_r"), TEXT("RightHand") });
	Idx[ThighL] = Find({ TEXT("thigh_l"), TEXT("LeftUpLeg") });
	Idx[CalfL] = Find({ TEXT("calf_l"), TEXT("LeftLeg") });
	Idx[FootL] = Find({ TEXT("foot_l"), TEXT("LeftFoot") });
	Idx[ThighR] = Find({ TEXT("thigh_r"), TEXT("RightUpLeg") });
	Idx[CalfR] = Find({ TEXT("calf_r"), TEXT("RightLeg") });
	Idx[FootR] = Find({ TEXT("foot_r"), TEXT("RightFoot") });
	for (int32 b : { UArmL, LArmL, HandL, UArmR, LArmR, HandR, ThighL, CalfL, FootL, ThighR, CalfR, FootR })
		if (Idx[b] == INDEX_NONE) { UE_LOG(LogZoogi, Warning, TEXT("PUPPET %s: limb bone %d missing - no poses"), *SM->GetName(), b); return false; }
	TArray<FTransform> CS; CS.SetNum(N);
	for (int32 i = 0; i < N; ++i) CS[i] = Parent[i] >= 0 ? RefLocal[i] * CS[Parent[i]] : RefLocal[i];
	// facing from the rig itself: left = shoulder L - shoulder R, forward = up x left
	Lat = CS[Idx[UArmL]].GetLocation() - CS[Idx[UArmR]].GetLocation(); Lat.Z = 0.f; Lat = Lat.GetSafeNormal();
	if (Lat.IsNearlyZero()) Lat = FVector(1, 0, 0);
	F = FVector::CrossProduct(FVector::UpVector, Lat).GetSafeNormal();
	const FBoxSphereBounds Bd = SM->GetBounds();
	BottomZ = Bd.Origin.Z - Bd.BoxExtent.Z;
	Scale = Height / FMath::Max(10.f, (float)Bd.BoxExtent.Z * 2.f);
	const FQuat ToActor = FRotationMatrix::MakeFromXZ(F, FVector::UpVector).ToQuat().Inverse(); // rig forward -> actor +X
	Body->SetRelativeRotation(ToActor);
	Body->SetRelativeScale3D(FVector(Scale));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -BottomZ * Scale));
	// smoothed aim directions start at the bind pose
	CurDir.SetNum(NumBones);
	auto Child = [this](int32 b) -> int32
	{
		switch (b)
		{
		case Spine3: return Idx[Neck]; case Neck: return Idx[Head];
		case UArmL: return Idx[LArmL]; case LArmL: return Idx[HandL]; case UArmR: return Idx[LArmR]; case LArmR: return Idx[HandR];
		case ThighL: return Idx[CalfL]; case CalfL: return Idx[FootL]; case ThighR: return Idx[CalfR]; case CalfR: return Idx[FootR];
		default: return INDEX_NONE;
		}
	};
	for (int32 b = 0; b < NumBones; ++b)
	{
		const int32 C = Child(b);
		CurDir[b] = (Idx[b] != INDEX_NONE && C != INDEX_NONE) ? (CS[C].GetLocation() - CS[Idx[b]].GetLocation()).GetSafeNormal() : FVector::UpVector;
	}
	if (Idx[Head] != INDEX_NONE) BindHeadCS = CS[Idx[Head]].GetRotation(); // b142 look-at
	bReady = true;
	UE_LOG(LogZoogi, Log, TEXT("PUPPET %s ready (%d bones, %.0f cm, scale %.2f, rig forward %s)"), *SM->GetName(), N, Height, Scale, *F.ToCompactString());
	return true;
}

AZoogiAurexPuppet* AZoogiAurexPuppet::SpawnFor(UWorld* W, AZoogiBall* B, const FVector& Feet, float Yaw, EZoogiPuppetPose InPose, float InHeight, USkeletalMeshComponent* From, bool bPop)
{
	if (!W || !B) return nullptr;
	ZoogiStandForms::Release(B, false); // b63: a standing start form hands over to this puppet
	// b62 (Evan): classic uses the Zoogis' own Tripo forms; the bought Aurex only in Battle Royale (-ZoogiAurexClassic = old)
	const AZoogiGameMode* Gm = W->GetAuthGameMode<AZoogiGameMode>();
	const bool bTripo = !From && !(Gm && Gm->bRoyaleMode) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAurexClassic"));
	USkeletalMesh* TripoSM = bTripo ? UZoogiFunSys::FormMeshFor(B) : nullptr;
	if (TripoSM) InHeight = FMath::Min(InHeight, 160.f);
	USkeletalMesh* SM = From ? Cast<USkeletalMesh>(From->GetSkinnedAsset())
		: (TripoSM ? TripoSM : LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	if (!SM) return nullptr;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiAurexPuppet* P = W->SpawnActor<AZoogiAurexPuppet>(AZoogiAurexPuppet::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Feet), SP);
	if (!P) return nullptr;
	P->Ball = B; P->Pose = InPose; P->Height = InHeight; P->FaceDir = FRotator(0.f, Yaw, 0.f).Vector();
	P->Tint = ZoogiUtil::PlayerColor(B->PlayerIndex);
	P->PopIn = bPop ? 0.3f : 0.f;
	if (!P->Setup(SM)) { P->Destroy(); return nullptr; }
	if (From)
	{
		for (int32 m = 0; m < P->Body->GetNumMaterials(); ++m) P->Body->SetMaterial(m, From->GetMaterial(m));
	}
	else if (TripoSM) {} // its own Tripo materials
	else if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		FLinearColor Z[4];
		ZoogiAurexPalette(B->Def.Name, B->PlayerIndex, Z);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, P);
		MID->SetVectorParameterValue(TEXT("Color_Zone_R"), Z[0]); MID->SetVectorParameterValue(TEXT("Color_Zone_G"), Z[1]);
		MID->SetVectorParameterValue(TEXT("Color_Zone_B"), Z[2]); MID->SetVectorParameterValue(TEXT("Emissive_Color"), Z[3]);
		for (int32 m = 0; m < P->Body->GetNumMaterials(); ++m) P->Body->SetMaterial(m, MID);
	}
	P->Glow->SetLightColor(FMath::Lerp(FLinearColor::White, P->Tint, 0.35f));
	// the ball's own visuals hide while the form is out (same rule as the ultimate strike: visible FLAG, not the collision sphere)
	TInlineComponentArray<UPrimitiveComponent*> Prims(B);
	for (UPrimitiveComponent* C : Prims)
	{
		if (!C || C == B->Sphere.Get() || C == B->Ring.Get() || !C->GetVisibleFlag()) continue;
		C->SetVisibility(false, false);
		P->Hidden.Add(C);
	}
	if (bPop) ZoogiUtil::PlaySfx(W, TEXT("AurexIn"), 0.8f); // b59
	if (bPop) P->Fx.Add(AZoogiFX::Spawn(W, Feet + FVector(0.f, 0.f, InHeight * 0.5f), P->Tint, 130.f, 0.3f, false, 8.f));
	P->Solve(0.f, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("PUPPET P%d %s out as %s (pose %d)"), B->PlayerIndex + 1, *B->Def.Name, From ? TEXT("its clash form") : (TripoSM ? *SM->GetName() : TEXT("Aurex")), (int32)InPose);
	return P;
}

AZoogiAurexPuppet* AZoogiAurexPuppet::SpawnBoss(UWorld* W, USkeletalMesh* SM, const FVector& Feet, float Yaw, float InHeight)
{
	if (!W || !SM) return nullptr;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiAurexPuppet* P = W->SpawnActor<AZoogiAurexPuppet>(AZoogiAurexPuppet::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Feet), SP);
	if (!P) return nullptr;
	P->Pose = EZoogiPuppetPose::Peek; P->Height = InHeight; P->FaceDir = FRotator(0.f, Yaw, 0.f).Vector(); P->PopIn = 0.f;
	P->Tint = FLinearColor(0.6f, 0.25f, 1.f);
	if (!P->Setup(SM)) { P->Destroy(); return nullptr; }
	P->Body->SetCastShadow(false);
	P->Body->SetBoundsScale(3.f);
	P->Glow->SetVisibility(false);
	P->Solve(0.f, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("PUPPET boss %s out (%.0f cm)"), *SM->GetName(), InHeight);
	return P;
}

void AZoogiAurexPuppet::Dismiss(bool bFx)
{
	if (OutAge >= 0.f) return;
	OutAge = 0.f;
	if (bFx && GetWorld()) if (AZoogiFX* X = AZoogiFX::Spawn(GetWorld(), GetActorLocation() + GetActorUpVector() * Height * 0.5f, Tint, 120.f, 0.25f, false, 8.f)) Fx.Add(X);
}

void AZoogiAurexPuppet::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : Hidden) if (C.IsValid()) C->SetVisibility(true, false);
	Hidden.Reset();
	Super::EndPlay(Reason);
}

void AZoogiAurexPuppet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float RealDt = FMath::Min((float)FApp::GetDeltaTime(), 0.05f); // real time: the poses play at full speed in slow-mo
	Age += RealDt;
	const float Gd = FMath::Max(0.01f, GetWorldSettings() ? GetWorldSettings()->TimeDilation : 1.f);
	for (const TWeakObjectPtr<AActor>& X : Fx) if (X.IsValid()) X->CustomTimeDilation = 1.f / Gd;
	if (OutAge >= 0.f)
	{
		OutAge += RealDt;
		if (OutAge >= 0.22f) { Destroy(); return; }
	}
	if (bReady) Solve(Age, RealDt);
}

void AZoogiAurexPuppet::Solve(float T, float Dt)
{
	if (!bReady || !Body) return;
	const FVector U = FVector::UpVector;
	FVector Want[NumBones];
	bool bHas[NumBones] = {};
	auto Set = [&](int32 b, const FVector& D) { Want[b] = D.GetSafeNormal(); bHas[b] = !Want[b].IsNearlyZero(); };
	auto S = [this](int32 Side) { return Side == 0 ? Lat : -Lat; };
	auto Legs = [&](float KickA, float KickF, float Bend, float Spread)
	{
		for (int32 s = 0; s < 2; ++s)
		{
			const float K = KickA * FMath::Sin(T * KickF + s * PI);
			Set(s ? ThighR : ThighL, -U + F * K + S(s) * Spread);
			Set(s ? CalfR : CalfL, -U - F * (Bend + 0.5f * KickA * FMath::Sin(T * KickF + s * PI - 0.7f)));
		}
	};
	float Rate = 14.f;
	switch (Pose)
	{
	case EZoogiPuppetPose::Guard:
		for (int32 s = 0; s < 2; ++s)
		{
			const float Br = 0.06f * FMath::Sin(T * 5.f + s);
			Set(s ? UArmR : UArmL, F * (0.5f + Br) - U * 0.6f + S(s) * 0.55f);   // elbows down in front
			Set(s ? LArmR : LArmL, U * 0.9f + F * 0.45f - S(s) * 0.3f);          // forearms up: fists by the chin
		}
		Legs(0.12f, 2.5f, 0.18f, 0.22f);
		Set(Spine3, U + F * 0.14f);
		break;
	case EZoogiPuppetPose::Hang:
	{
		const float P = FMath::Clamp(Pull, 0.f, 1.f);
		for (int32 s = 0; s < 2; ++s)
		{
			const float Jit = 0.08f * FMath::Sin(T * 17.f + s * 2.1f) * P; // shaking with effort
			Set(s ? UArmR : UArmL, FMath::Lerp(U + F * 0.2f + S(s) * 0.25f, U * 0.25f + F * (0.55f + Jit) + S(s) * 0.8f, P));
			Set(s ? LArmR : LArmL, FMath::Lerp(U + F * 0.35f, U * 0.55f + F * 0.8f - S(s) * 0.3f, P));
		}
		Legs(0.35f + 0.25f * P, 8.f + 4.f * P, 0.25f, 0.12f); // feet scrabbling at the cliff
		Set(Spine3, U + F * 0.12f);
		break;
	}
	case EZoogiPuppetPose::Climb:
		for (int32 s = 0; s < 2; ++s)
		{
			Set(s ? UArmR : UArmL, -U * 0.25f + F * 0.6f + S(s) * 0.5f); // pressing down on the lip
			Set(s ? LArmR : LArmL, -U * 0.9f + F * 0.25f);
		}
		Set(ThighL, F * 0.85f - U * 0.25f + Lat * 0.1f); Set(CalfL, -U);           // knee up onto the ledge
		Set(ThighR, -U + F * 0.1f - Lat * 0.1f); Set(CalfR, -U - F * 0.45f);
		Set(Spine3, U + F * 0.35f);
		break;
	case EZoogiPuppetPose::Drop:
		for (int32 s = 0; s < 2; ++s)
		{
			Set(s ? UArmR : UArmL, U * 0.7f + S(s) * 0.7f + F * 0.35f * FMath::Sin(T * 13.f + s * 1.3f));
			Set(s ? LArmR : LArmL, U * 0.6f + S(s) * 0.6f * FMath::Cos(T * 15.f + s) + F * 0.2f);
		}
		Legs(0.55f, 11.f, 0.3f, 0.2f);
		Rate = 20.f;
		break;
	case EZoogiPuppetPose::Cheer:
		for (int32 s = 0; s < 2; ++s)
		{
			Set(s ? UArmR : UArmL, U * 0.9f + S(s) * 0.55f);
			Set(s ? LArmR : LArmL, U + S(s) * 0.15f - F * 0.1f + F * 0.15f * FMath::Sin(T * 12.f + s * PI));
		}
		Legs(0.f, 0.f, 0.05f, 0.15f);
		break;
	case EZoogiPuppetPose::Backstroke:
	{
		// windmill: each arm circles head-ward through the air (chest side) and back down along the body, the other half a turn behind
		for (int32 s = 0; s < 2; ++s)
		{
			const float Ph = T * 9.f + s * PI;
			const FVector D = U * FMath::Cos(Ph) + F * FMath::Sin(Ph) * 0.95f + S(s) * 0.35f;
			const FVector D2 = U * FMath::Cos(Ph + 0.35f) + F * FMath::Sin(Ph + 0.35f) * 0.95f + S(s) * 0.25f;
			Set(s ? UArmR : UArmL, D);
			Set(s ? LArmR : LArmL, D2);
		}
		Legs(0.3f, 15.f, 0.12f, 0.12f); // flutter kick
		Rate = 30.f;
		break;
	}
	case EZoogiPuppetPose::Splat: // b67 (Evan): smashed into the stands - star limbs, flat on the wall, a dazed twitch
		for (int32 s = 0; s < 2; ++s)
		{
			const float Tw = 0.05f * FMath::Sin(T * 23.f + s * 1.7f);
			Set(s ? UArmR : UArmL, S(s) * 0.85f + U * (0.55f + Tw) - F * 0.12f);
			Set(s ? LArmR : LArmL, S(s) * 0.7f + U * 0.75f - F * 0.1f);
			Set(s ? ThighR : ThighL, -U + S(s) * 0.62f - F * 0.08f);
			Set(s ? CalfR : CalfL, -U + S(s) * 0.5f - F * 0.05f);
		}
		Set(Spine3, U - F * 0.1f);
		Rate = 40.f;
		break;
	case EZoogiPuppetPose::Peek: // b142: the Cosmos boss - hands on the rim in front of it, chest over the lip, fingers drumming
	{
		const float Sl = FMath::Clamp(Pull, 0.f, 1.f); // a slam: the arms rise
		for (int32 s = 0; s < 2; ++s)
		{
			const float Tap = FMath::Max(0.f, FMath::Sin(T * 3.1f + s * 2.4f)) * 0.08f;
			Set(s ? UArmR : UArmL, F * 0.8f + U * (0.22f + 0.8f * Sl) + S(s) * 0.62f);
			Set(s ? LArmR : LArmL, F * 0.55f - U * (0.85f - Tap - 1.2f * Sl) - S(s) * 0.12f);
		}
		Legs(0.06f, 1.3f, 0.2f, 0.16f);
		Set(Spine3, U + F * (0.45f + Lean));
		Set(Neck, U + F * (0.3f + Lean * 0.5f));
		Rate = 5.f;
		break;
	}
	default: // Stand
		for (int32 s = 0; s < 2; ++s)
		{
			Set(s ? UArmR : UArmL, -U + S(s) * 0.3f + F * 0.05f);
			Set(s ? LArmR : LArmL, -U + F * 0.3f);
		}
		Legs(0.f, 0.f, 0.05f, 0.12f);
		break;
	}
	const float A = bFirst ? 1.f : 1.f - FMath::Exp(-Dt * Rate);
	bFirst = false;
	for (int32 b = 0; b < NumBones; ++b)
		if (bHas[b]) CurDir[b] = (CurDir[b] + (Want[b] - CurDir[b]) * A).GetSafeNormal();

	// aim solve (parents before children - the reference skeleton is ordered that way)
	const int32 N = RefLocal.Num();
	TArray<FTransform> Local = RefLocal;
	TArray<FTransform> CS; CS.SetNum(N);
	TArray<int32, TInlineAllocator<16>> Mod;
	int32 ChildOf[NumBones];
	ChildOf[Spine3] = Idx[Neck]; ChildOf[Neck] = Idx[Head]; ChildOf[Head] = INDEX_NONE;
	ChildOf[UArmL] = Idx[LArmL]; ChildOf[LArmL] = Idx[HandL]; ChildOf[HandL] = INDEX_NONE;
	ChildOf[UArmR] = Idx[LArmR]; ChildOf[LArmR] = Idx[HandR]; ChildOf[HandR] = INDEX_NONE;
	ChildOf[ThighL] = Idx[CalfL]; ChildOf[CalfL] = Idx[FootL]; ChildOf[FootL] = INDEX_NONE;
	ChildOf[ThighR] = Idx[CalfR]; ChildOf[CalfR] = Idx[FootR]; ChildOf[FootR] = INDEX_NONE;
	for (int32 i = 0; i < N; ++i)
	{
		const FTransform PT = Parent[i] >= 0 ? CS[Parent[i]] : FTransform::Identity;
		CS[i] = Local[i] * PT;
		for (int32 b = 0; b < NumBones; ++b)
		{
			if (Idx[b] != i || !bHas[b] || ChildOf[b] == INDEX_NONE) continue;
			const FVector Cur = (CS[i].TransformPosition(RefLocal[ChildOf[b]].GetTranslation()) - CS[i].GetLocation()).GetSafeNormal();
			if (Cur.IsNearlyZero()) break;
			const FQuat NewQ = FQuat::FindBetweenNormals(Cur, CurDir[b]) * CS[i].GetRotation();
			Local[i].SetRotation((PT.GetRotation().Inverse() * NewQ).GetNormalized());
			CS[i] = Local[i] * PT;
			Mod.Add(i);
			break;
		}
	}
	for (int32 i : Mod) Body->SetBoneTransformByName(Names[i], CS[i], EBoneSpaces::ComponentSpace);
	HandMidCS = (CS[Idx[HandL]].GetLocation() + CS[Idx[HandR]].GetLocation()) * 0.5f;
	if (bLookAt && Idx[Head] != INDEX_NONE)
	{	// b142: the head turns to look at LookAt (the camera / the shot), at most 60 deg off the body, smoothed
		const int32 Hb = Idx[Head];
		const FVector LookCS = Body->GetComponentTransform().InverseTransformPosition(LookAt);
		const FVector WantL = (LookCS - CS[Hb].GetLocation()).GetSafeNormal();
		const FVector Cur = (CS[Hb].GetRotation() * BindHeadCS.Inverse()).RotateVector(F).GetSafeNormal();
		if (!WantL.IsNearlyZero() && !Cur.IsNearlyZero())
		{
			FQuat Turn = FQuat::FindBetweenNormals(Cur, WantL);
			FVector Ax; float Ang; Turn.ToAxisAndAngle(Ax, Ang);
			Turn = FQuat(Ax, FMath::Min(Ang, FMath::DegreesToRadians(60.f)));
			HeadTurn = FQuat::Slerp(HeadTurn, Turn, FMath::Clamp(Dt * 4.f, 0.f, 1.f)).GetNormalized();
			FTransform Hx = CS[Hb]; Hx.SetRotation((HeadTurn * CS[Hb].GetRotation()).GetNormalized());
			Body->SetBoneTransformByName(Names[Hb], Hx, EBoneSpaces::ComponentSpace);
		}
	}

	// pop in / out (scale), placement
	float K = 1.f;
	if (PopIn > 0.f && T < PopIn) { const float X = T / PopIn; K = 1.f + 2.70158f * FMath::Pow(X - 1.f, 3.f) + 1.70158f * FMath::Pow(X - 1.f, 2.f); } // ease-out-back
	if (OutAge >= 0.f) K *= FMath::Clamp(1.f - OutAge / 0.22f, 0.02f, 1.f);
	K = FMath::Max(K, 0.02f);
	SetActorScale3D(FVector(K));
	Glow->SetIntensity(30.f + 160.f * FMath::Clamp(1.f - T / 0.4f, 0.f, 1.f));
	const float Yaw = FaceDir.Rotation().Yaw;
	AZoogiBall* B = Ball.Get();
	if (Pose == EZoogiPuppetPose::Backstroke)
	{
		const FVector Tr = FVector(Travel.X, Travel.Y, 0.f).GetSafeNormal();
		const FVector Chest = (U * 0.8f - Tr * 0.6f).GetSafeNormal(), HeadW = (Tr * 0.8f + U * 0.6f).GetSafeNormal();
		const FQuat Roll(HeadW, FMath::DegreesToRadians(12.f * FMath::Sin(T * 3.f)));
		const FQuat Q = Roll * FRotationMatrix::MakeFromXZ(Chest, HeadW).ToQuat();
		SetActorLocationAndRotation(Center - Q.GetUpVector() * Height * 0.5f * K, Q);
	}
	else if (bPinHands)
	{
		const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
		const FVector HandA = Body->GetRelativeTransform().TransformPosition(HandMidCS) * K;
		SetActorLocationAndRotation(HandPin - Q.RotateVector(HandA), Q);
	}
	else if (bFollowBall && B)
	{
		const float Spin = (PopIn > 0.f && T < PopIn) ? (1.f - T / PopIn) * 360.f : 0.f; // the transform spin
		const float Hop = Pose == EZoogiPuppetPose::Cheer ? FMath::Abs(FMath::Sin(T * 7.f)) * 14.f : 0.f;
		SetActorLocationAndRotation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - Hop), FRotator(0.f, Yaw + Spin, 0.f));
	}
	else if (Pose != EZoogiPuppetPose::Drop && Pose != EZoogiPuppetPose::Splat)
	{
		SetActorRotation(FRotator(0.f, Yaw, 0.f));
	}
}
