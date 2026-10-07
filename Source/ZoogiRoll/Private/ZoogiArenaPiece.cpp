// Reusable destructible stage piece (Frozen Ring, Oct 6). See ZoogiArenaPiece.h.
#include "ZoogiArenaPiece.h"
#include "ZoogiArena.h"
#include "ZoogiMeshKit.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiArtifactComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
	bool HasScalar(UMaterialInstanceDynamic* M, const TCHAR* Name)
	{
		float V = 0.f;
		return M && M->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(Name)), V);
	}
}

AZoogiArenaPiece::AZoogiArenaPiece()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

void AZoogiArenaPiece::Setup(FName InKind, FName InSurface, float InMaxHP, bool bInIndestructible)
{
	Kind = InKind;
	Surface = InSurface;
	MaxHP = HP = FMath::Max(1.f, InMaxHP);
	bIndestructible = bInIndestructible;
	Seed = (int32)(GetActorLocation().X * 3.f + GetActorLocation().Y * 7.f) ^ GetTypeHash(Kind);
}

UStaticMeshComponent* AZoogiArenaPiece::AddShape(const TCHAR* Shape, const FTransform& Local, UMaterialInterface* Mat, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadow);
	C->SetGenerateOverlapEvents(false);
	C->RegisterComponent();
	C->SetRelativeTransform(Local);
	if (Mat) C->SetMaterial(0, Mat);
	FPart P; P.C = C; Parts.Add(P);
	return C;
}

void AZoogiArenaPiece::AdoptPart(UPrimitiveComponent* C)
{
	if (!C) return;
	C->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FPart P; P.C = C; Parts.Add(P);
}

UStaticMeshComponent* AZoogiArenaPiece::AddBox(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bShadow)
{
	return AddShape(TEXT("Cube"), FTransform(Rot, Center, Size / 100.f), Mat, bShadow);
}

UProceduralMeshComponent* AZoogiArenaPiece::AddProc(const FZoogiMeshBuilder& M, UMaterialInterface* Mat, bool bShadow)
{
	UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadow);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->RegisterComponent();
	M.Upload(C, 0, false);
	if (Mat) C->SetMaterial(0, Mat);
	FPart P; P.C = C; Parts.Add(P);
	return C;
}

UStaticMeshComponent* AZoogiArenaPiece::AddColliderBox(const FVector& Center, const FVector& Size, const FRotator& Rot, FName Tag, UPhysicalMaterial* PM)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	C->SetNotifyRigidBodyCollision(true);
	if (PM) C->SetPhysMaterialOverride(PM);
	C->SetHiddenInGame(true);
	C->SetCastShadow(false);
	if (!Tag.IsNone()) C->ComponentTags.Add(Tag);
	C->RegisterComponent();
	C->SetRelativeTransform(FTransform(Rot, Center, Size / 100.f));
	Colliders.Add(C);
	return C;
}

UStaticMeshComponent* AZoogiArenaPiece::AddColliderCyl(const FVector2D& XY, float Radius, float Bottom, float Top, FName Tag, UPhysicalMaterial* PM)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetCollisionProfileName(TEXT("BlockAll"));
	C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	C->SetNotifyRigidBodyCollision(true);
	if (PM) C->SetPhysMaterialOverride(PM);
	C->SetHiddenInGame(true);
	C->SetCastShadow(false);
	if (!Tag.IsNone()) C->ComponentTags.Add(Tag);
	C->RegisterComponent();
	const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
	const FVector S(Radius / B.BoxExtent.X, Radius / B.BoxExtent.Y, (Top - Bottom) / (2.f * B.BoxExtent.Z));
	C->SetRelativeScale3D(S);
	C->SetRelativeLocation(FVector(XY.X, XY.Y, Top - (B.Origin.Z + B.BoxExtent.Z) * S.Z));
	Colliders.Add(C);
	return C;
}

void AZoogiArenaPiece::FinishBuild()
{
	// Piece-local MIDs (shared source MIDs must not crack every piece at once).
	TMap<UMaterialInterface*, UMaterialInstanceDynamic*> Local;
	const float ActorZ = GetActorLocation().Z;
	for (FPart& P : Parts)
	{
		UPrimitiveComponent* C = P.C.Get();
		if (!C) continue;
		P.Base = C->GetRelativeTransform();
		P.bGlow = GlowParts.Contains(C);
		C->UpdateBounds();
		P.BottomZ = (C->Bounds.Origin.Z - C->Bounds.BoxExtent.Z) - ActorZ;
		for (int32 m = 0; m < C->GetNumMaterials(); ++m)
		{
			UMaterialInterface* Src = C->GetMaterial(m);
			if (!Src) continue;
			UMaterialInstanceDynamic*& D = Local.FindOrAdd(Src);
			if (!D)
			{
				if (UMaterialInstanceDynamic* SrcMID = Cast<UMaterialInstanceDynamic>(Src))
				{
					D = UMaterialInstanceDynamic::Create(SrcMID->Parent, this);
					D->CopyParameterOverrides(SrcMID);
				}
				else D = UMaterialInstanceDynamic::Create(Src, this);
				MIDs.Add(D);
				FLinearColor Tint = FLinearColor::White;
				D->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), Tint);
				MIDBaseTint.Add(Tint);
				MIDHasDamageParam.Add(HasScalar(D, TEXT("Damage")));
				if (!P.bGlow) ShardMats.AddUnique(D);
				else { float Sg = 1.f; if (D->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Strength")), Sg)) GlowBase.Add(D, Sg); }
			}
			C->SetMaterial(m, D);
		}
	}
	bBuilt = true;
	ResetPiece();
}

bool AZoogiArenaPiece::GetPathBlock(FVector2D& OutXY, float& OutR) const
{
	if (PathR <= 0.f || !bCollisionLive || State == EZoogiPieceState::Broken || Colliders.Num() == 0) return false;
	OutXY = PathXY;
	OutR = PathR;
	return true;
}

void AZoogiArenaPiece::SetCollisionLive(bool bOn)
{
	bCollisionLive = bOn;
	const bool bEnable = bOn && State != EZoogiPieceState::Broken;
	for (UPrimitiveComponent* C : Colliders)
		if (C) C->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

FString AZoogiArenaPiece::StateName(EZoogiPieceState S)
{
	switch (S)
	{
	case EZoogiPieceState::Intact: return TEXT("INTACT");
	case EZoogiPieceState::Damaged: return TEXT("DAMAGED");
	case EZoogiPieceState::Cracked: return TEXT("CRACKED");
	case EZoogiPieceState::Broken: return TEXT("BROKEN");
	}
	return TEXT("?");
}

FLinearColor AZoogiArenaPiece::ElementColor(EZoogiArtifact E)
{
	switch (E)
	{
	case EZoogiArtifact::Ember: return FLinearColor(1.f, 0.42f, 0.17f);
	case EZoogiArtifact::Frost: return FLinearColor(0.41f, 0.88f, 0.98f);
	case EZoogiArtifact::Storm: return FLinearColor(0.95f, 0.97f, 1.f);
	case EZoogiArtifact::Titan: return FLinearColor(1.f, 0.78f, 0.25f);
	case EZoogiArtifact::Void: return FLinearColor(0.55f, 0.3f, 0.95f);
	default: return FLinearColor(0.75f, 0.95f, 1.f);
	}
}

float AZoogiArenaPiece::PowerMultiplier(FName InSurface, EZoogiArtifact Element)
{
	const bool bIce = InSurface == TEXT("Ice") || InSurface == TEXT("Snow") || InSurface == TEXT("Crystal");
	switch (Element)
	{
	case EZoogiArtifact::Ember: return bIce ? 2.0f : 1.2f;   // melts and cracks ice / snow
	case EZoogiArtifact::Frost: return bIce ? -1.f : 0.6f;   // refreezes ice (one state back); weak on stone / wood
	case EZoogiArtifact::Storm: return InSurface == TEXT("Crystal") ? 1.6f : 1.25f; // shock cracks crystal hardest
	case EZoogiArtifact::Titan: return InSurface == TEXT("Stone") ? 2.2f : 2.0f;    // crushing weight
	case EZoogiArtifact::Void: return 1.5f;                  // erodes anything
	default: return 1.f;                                     // future powers: 1x until they get a row here
	}
}

void AZoogiArenaPiece::OnBallHit(AZoogiBall* Ball, const FVector& NormalImpulse, const FVector& ImpactPoint)
{
	if (!Ball || bIndestructible || State == EZoogiPieceState::Broken || !bCollisionLive) return;
	if (Clock - LastHit < 0.25f) return; // one contact reports for several frames
	FVector N2 = -NormalImpulse.GetSafeNormal();
	if (N2.IsNearlyZero()) N2 = (GetActorLocation() - Ball->BallLocation()).GetSafeNormal();
	const FVector V = Ball->PrevVelocity.SizeSquared() > 1.f ? Ball->PrevVelocity : Ball->GetBallVelocity();
	const float Vn = FMath::Max(0.f, FVector::DotProduct(V, N2));
	if (Vn < HitSpeedMin) return;
	LastHit = Clock;
	const EZoogiArtifact E = Ball->Artifact ? Ball->Artifact->Infused : EZoogiArtifact::None;
	float Dmg = (Vn - HitSpeedMin) * HitDamagePerSpeed + 6.f;
	if (E != EZoogiArtifact::None) Dmg *= 1.3f; // an Awakened shooter hits harder
	UE_LOG(LogZoogi, Log, TEXT("Piece %s (%s, %s) hit by P%d %s at %.0f cm/s normal%s"), *GetName(), *Kind.ToString(), *Surface.ToString(), Ball->PlayerIndex + 1, *Ball->Def.Name, Vn,
		E != EZoogiArtifact::None ? TEXT(" (infused)") : TEXT(""));
	ApplyDamage(Dmg, ImpactPoint, N2, E, Ball);
}

bool AZoogiArenaPiece::ApplyPower(EZoogiArtifact Element, float Amount, const FVector& At, AZoogiBall* By)
{
	return ApplyDamage(Amount, At, (At - GetActorLocation()).GetSafeNormal2D() * -1.f, Element, By);
}

bool AZoogiArenaPiece::AnythingOverlaps() const
{
	UWorld* W = GetWorld();
	if (!W) return false;
	for (const UPrimitiveComponent* C : Colliders)
	{
		if (!C) continue;
		const FBox Bx = C->Bounds.GetBox().ExpandBy(70.f);
		for (TActorIterator<AZoogiBall> It(W); It; ++It) if (!It->bOut && Bx.IsInside(It->BallLocation())) return true;
		for (TActorIterator<AZoogiOrb> It(W); It; ++It) if (!It->bOut && Bx.IsInside(It->OrbLocation())) return true;
	}
	return false;
}

bool AZoogiArenaPiece::ApplyDamage(float Amount, const FVector& At, const FVector& Dir, EZoogiArtifact Element, AZoogiBall* By)
{
	if (bIndestructible || !bBuilt) return false;
	const float Mul = PowerMultiplier(Surface, Element);
	const EZoogiPieceState Before = State;
	if (Mul < 0.f)
	{
		// Frost on ice: refreeze one state (never while a Zoogi / orb is inside the collider footprint).
		Frost = 1.f;
		if (State == EZoogiPieceState::Intact) { ApplyVisualState(); return false; }
		if (State == EZoogiPieceState::Broken && Colliders.Num() > 0 && AnythingOverlaps())
		{
			UE_LOG(LogZoogi, Log, TEXT("Piece %s: Frost refreeze deferred (something is inside it)"), *GetName());
			return false;
		}
		const EZoogiPieceState NewS = (EZoogiPieceState)((uint8)State - 1);
		HP = NewS == EZoogiPieceState::Cracked ? MaxHP * 0.3f : (NewS == EZoogiPieceState::Damaged ? MaxHP * 0.6f : MaxHP);
		SetPieceState(NewS, At, Dir, true);
		AZoogiFX::Spawn(GetWorld(), At, ElementColor(Element), 140.f, 0.5f, true, 6.f);
		UE_LOG(LogZoogi, Log, TEXT("Piece %s: FROST refreezes %s -> %s"), *GetName(), *StateName(Before), *StateName(State));
		return true;
	}
	if (Element == EZoogiArtifact::Ember && (Surface == TEXT("Ice") || Surface == TEXT("Snow"))) Melt = FMath::Min(1.f, Melt + 0.5f);
	if (Element == EZoogiArtifact::Storm) AZoogiFX::Niagara(GetWorld(), TEXT("NS_Sparks"), At, Dir.Rotation(), 1.f);
	HP -= Amount * Mul;
	EZoogiPieceState NewS = State;
	if (HP <= 0.f) NewS = EZoogiPieceState::Broken;
	else if (HP < MaxHP * 0.4f) NewS = FMath::Max(State, EZoogiPieceState::Cracked);
	else if (HP < MaxHP * 0.78f) NewS = FMath::Max(State, EZoogiPieceState::Damaged);
	if (Element != EZoogiArtifact::None) Flash = 1.f;
	if (NewS != State)
	{
		SetPieceState(NewS, At, Dir, true);
		UE_LOG(LogZoogi, Log, TEXT("Piece %s (%s): %s -> %s (hp %.0f / %.0f%s%s)"), *GetName(), *Kind.ToString(), *StateName(Before), *StateName(State), FMath::Max(0.f, HP), MaxHP,
			Element != EZoogiArtifact::None ? TEXT(", element ") : TEXT(""), Element != EZoogiArtifact::None ? *UEnum::GetValueAsString(Element) : TEXT(""));
		return true;
	}
	ApplyVisualState();
	return false;
}

void AZoogiArenaPiece::SetPieceState(EZoogiPieceState S, const FVector& At, const FVector& Dir, bool bFx)
{
	if (bIndestructible) return;
	const EZoogiPieceState Before = State;
	State = S;
	if (S == EZoogiPieceState::Intact) HP = MaxHP;
	else if (S == EZoogiPieceState::Broken) HP = 0.f;
	else if (bFx == false) HP = S == EZoogiPieceState::Damaged ? MaxHP * 0.6f : MaxHP * 0.3f;
	FRandomStream R(Seed + (int32)S * 31);
	for (FPart& P : Parts)
	{
		if (S >= EZoogiPieceState::Cracked && !P.bGlow)
		{
			P.Shift = FVector(R.FRandRange(-5.f, 5.f), R.FRandRange(-5.f, 5.f), R.FRandRange(-3.f, 0.f));
			P.Tilt = FRotator(R.FRandRange(-2.5f, 2.5f), R.FRandRange(-2.f, 2.f), R.FRandRange(-2.5f, 2.5f));
		}
		else { P.Shift = FVector::ZeroVector; P.Tilt = FRotator::ZeroRotator; }
	}
	SetCollisionLive(bCollisionLive);
	ApplyVisualState();
	if (!bFx || S <= Before) return;
	const FVector D = Dir.IsNearlyZero() ? FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() : Dir.GetSafeNormal();
	if (S == EZoogiPieceState::Damaged)
	{
		SpawnShards(3, At, D, 260.f, 4.f, 8.f);
		ZoogiUtil::PlaySfx(this, TEXT("Clack"), 0.5f, 1.5f);
	}
	else if (S == EZoogiPieceState::Cracked)
	{
		SpawnShards(6, At, D, 360.f, 5.f, 12.f);
		AZoogiFX::Spawn(GetWorld(), At, CrackColor, 120.f, 0.35f, false, 5.f);
		ZoogiUtil::PlaySfx(this, TEXT("HitCrack"), 0.6f, 1.25f);
	}
	else if (S == EZoogiPieceState::Broken)
	{
		FVector Ctr = GetActorLocation();
		FBox Bx(ForceInit);
		for (const FPart& P : Parts) if (P.C.IsValid()) Bx += P.C->Bounds.GetBox();
		if (Bx.IsValid) Ctr = Bx.GetCenter();
		const float Size = Bx.IsValid ? Bx.GetExtent().Size() : 100.f;
		SpawnShards(FMath::Clamp((int32)(Size / 18.f), 8, 16), Ctr, D, 520.f, FMath::Clamp(Size * 0.05f, 6.f, 22.f), FMath::Clamp(Size * 0.12f, 12.f, 40.f));
		AZoogiFX::Spawn(GetWorld(), Ctr, CrackColor, FMath::Clamp(Size * 1.2f, 160.f, 420.f), 0.6f, true, 6.f);
		AZoogiFX::Spawn(GetWorld(), Ctr, FLinearColor(0.85f, 0.92f, 1.f), FMath::Clamp(Size, 120.f, 360.f), 0.8f, false, 3.f);
		ZoogiUtil::PlaySfx(this, TEXT("HitCrack"), 0.9f, 0.85f);
	}
}

void AZoogiArenaPiece::ResetPiece()
{
	State = EZoogiPieceState::Intact;
	HP = MaxHP;
	Melt = Frost = Flash = 0.f;
	for (FPart& P : Parts) { P.Shift = FVector::ZeroVector; P.Tilt = FRotator::ZeroRotator; }
	for (FShard& S : Shards) if (S.C.IsValid()) S.C->SetVisibility(false);
	Shards.Reset();
	SetCollisionLive(bCollisionLive);
	ApplyVisualState();
}

void AZoogiArenaPiece::ApplyVisualState()
{
	const float DamageV = State == EZoogiPieceState::Intact ? 0.f : (State == EZoogiPieceState::Damaged ? 0.42f : (State == EZoogiPieceState::Cracked ? 0.8f : 1.f));
	for (int32 i = 0; i < MIDs.Num(); ++i)
	{
		UMaterialInstanceDynamic* M = MIDs[i];
		if (!M) continue;
		if (MIDHasDamageParam[i])
		{
			M->SetScalarParameterValue(TEXT("Damage"), DamageV);
			M->SetScalarParameterValue(TEXT("Melt"), Melt);
			M->SetScalarParameterValue(TEXT("Frost"), Frost);
			M->SetScalarParameterValue(TEXT("Flash"), Flash);
			M->SetVectorParameterValue(TEXT("CrackColor"), CrackColor);
		}
		else
		{
			// Fallback look: darker, slightly bluer as it degrades (and warmer while melted).
			const float K = 1.f - 0.18f * DamageV;
			FLinearColor T = MIDBaseTint[i] * K;
			T = FLinearColor::LerpUsingHSV(T, FLinearColor(0.45f, 0.3f, 0.25f), Melt * 0.25f);
			M->SetVectorParameterValue(TEXT("Color"), T);
			M->SetVectorParameterValue(TEXT("ColorA"), T);
		}
	}
	for (FPart& P : Parts)
	{
		UPrimitiveComponent* C = P.C.Get();
		if (!C) continue;
		if (State == EZoogiPieceState::Broken)
		{
			// Rubble: low parts stay, flattened onto their base; tall / glowing parts are gone.
			const bool bKeep = !P.bGlow && P.BottomZ < RubbleKeepZ;
			C->SetVisibility(bKeep);
			if (bKeep)
			{
				FTransform T = P.Base;
				const float H = FMath::Max(1.f, C->Bounds.BoxExtent.Z * 2.f);
				const float F = FMath::Clamp(RubbleKeepZ * 0.35f / H, 0.08f, 1.f);
				FVector S = T.GetScale3D();
				S.Z *= F;
				T.SetScale3D(S);
				if (Cast<UProceduralMeshComponent>(C)) T.SetLocation(T.GetLocation() * FVector(1.f, 1.f, F));
				else T.SetLocation(FVector(T.GetLocation().X, T.GetLocation().Y, P.BottomZ + (T.GetLocation().Z - P.BottomZ) * F));
				T.ConcatenateRotation(P.Tilt.Quaternion());
				C->SetRelativeTransform(T);
			}
			continue;
		}
		C->SetVisibility(true);
		FTransform T = P.Base;
		T.AddToTranslation(P.Shift);
		T.ConcatenateRotation(P.Tilt.Quaternion());
		C->SetRelativeTransform(T);
		if (P.bGlow)
		{
			const float G = State == EZoogiPieceState::Intact ? 1.f : (State == EZoogiPieceState::Damaged ? 0.75f : 0.4f);
			C->SetVisibility(G > 0.f);
			for (int32 m = 0; m < C->GetNumMaterials(); ++m)
				if (UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(C->GetMaterial(m)))
					if (const float* Base = GlowBase.Find(D)) D->SetScalarParameterValue(TEXT("Strength"), *Base * G);
		}
	}
}

UMaterialInterface* AZoogiArenaPiece::PickShardMat() const
{
	return ShardMats.Num() > 0 ? ShardMats[FMath::RandRange(0, ShardMats.Num() - 1)].Get() : nullptr;
}

void AZoogiArenaPiece::SpawnShards(int32 Count, const FVector& At, const FVector& Dir, float Speed, float SizeMin, float SizeMax)
{
	FRandomStream R(Seed * 13 + (int32)(Clock * 100.f));
	const float FloorZ = FVector2D(At).Size() < AZoogiArena::ActiveKnockoffR + 40.f ? 0.f : GetActorLocation().Z;
	for (int32 i = 0; i < Count; ++i)
	{
		UStaticMeshComponent* C = nullptr;
		for (UStaticMeshComponent* P : ShardPool) if (P && !P->IsVisible()) { C = P; break; }
		if (!C)
		{
			if (ShardPool.Num() >= 24) break;
			C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(ZoogiUtil::BasicShape(i % 3 == 0 ? TEXT("Cone") : TEXT("Cube")));
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(Root);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->RegisterComponent();
			ShardPool.Add(C);
		}
		C->SetMaterial(0, PickShardMat());
		C->SetVisibility(true);
		FShard S;
		S.C = C;
		S.Pos = At + FVector(R.FRandRange(-15.f, 15.f), R.FRandRange(-15.f, 15.f), R.FRandRange(5.f, 30.f));
		const FVector Spread = (Dir + FVector(R.FRandRange(-0.8f, 0.8f), R.FRandRange(-0.8f, 0.8f), 0.f)).GetSafeNormal2D();
		S.Vel = Spread * Speed * R.FRandRange(0.4f, 1.f) + FVector(0, 0, Speed * R.FRandRange(0.5f, 1.1f));
		S.W = FVector(R.FRandRange(-400.f, 400.f), R.FRandRange(-400.f, 400.f), R.FRandRange(-400.f, 400.f));
		const float Sz = R.FRandRange(SizeMin, SizeMax);
		S.Scale = FVector(Sz, Sz * R.FRandRange(0.5f, 1.2f), Sz * R.FRandRange(0.4f, 1.f)) / 100.f;
		S.Rot = FRotator(R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f), 0.f);
		S.Life = R.FRandRange(1.6f, 2.6f);
		S.FloorZ = FloorZ;
		Shards.Add(S);
	}
}

void AZoogiArenaPiece::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	Clock += DeltaSeconds;
	bool bDirty = false;
	if (Flash > 0.f) { Flash = FMath::Max(0.f, Flash - Dt * 1.5f); bDirty = true; }
	if (Frost > 0.f) { Frost = FMath::Max(0.f, Frost - Dt * 0.25f); bDirty = true; }
	if (bDirty) ApplyVisualState();
	for (int32 i = Shards.Num() - 1; i >= 0; --i)
	{
		FShard& S = Shards[i];
		UStaticMeshComponent* C = S.C.Get();
		S.Age += Dt;
		if (!C || S.Age > S.Life)
		{
			if (C) C->SetVisibility(false);
			Shards.RemoveAtSwap(i);
			continue;
		}
		if (!S.bRest)
		{
			S.Vel.Z -= 980.f * Dt;
			S.Pos += S.Vel * Dt;
			S.Rot += FRotator(S.W.X * Dt, S.W.Y * Dt, S.W.Z * Dt);
			const bool bOverFloor = S.FloorZ < -1.f || FVector2D(S.Pos).Size() < AZoogiArena::ActiveKnockoffR + 60.f;
			if (bOverFloor && S.Pos.Z < S.FloorZ + 2.f && S.Vel.Z < 0.f)
			{
				S.Pos.Z = S.FloorZ + 2.f;
				if (FMath::Abs(S.Vel.Z) < 120.f) { S.bRest = true; }
				else { S.Vel.Z *= -0.3f; S.Vel.X *= 0.5f; S.Vel.Y *= 0.5f; S.W *= 0.5f; }
			}
		}
		const float Fade = FMath::Clamp((S.Life - S.Age) / 0.5f, 0.f, 1.f);
		C->SetWorldTransform(FTransform(S.Rot, S.Pos, S.Scale * Fade));
	}
}

int32 AZoogiArenaPiece::DamageArea(UWorld* World, const FVector& P, float Radius, float Amount, EZoogiArtifact Element, AZoogiBall* By, const AZoogiArenaPiece* Except)
{
	if (!World) return 0;
	int32 N = 0;
	for (TActorIterator<AZoogiArenaPiece> It(World); It; ++It)
	{
		AZoogiArenaPiece* A = *It;
		if (A == Except || A->bIndestructible) continue;
		FBox Bx(ForceInit);
		for (const FPart& Pt : A->Parts) if (Pt.C.IsValid() && Pt.C->IsVisible()) Bx += Pt.C->Bounds.GetBox();
		if (!Bx.IsValid) continue;
		const float D = FMath::Sqrt(Bx.ComputeSquaredDistanceToPoint(P));
		if (D > Radius) continue;
		const float Fall = 1.f - 0.5f * D / FMath::Max(1.f, Radius);
		if (A->ApplyDamage(Amount * Fall, Bx.GetClosestPointTo(P), (Bx.GetCenter() - P).GetSafeNormal2D(), Element, By)) ++N;
	}
	return N;
}

AZoogiArenaPiece* AZoogiArenaPiece::RingOutCrash(UWorld* World, const FVector& ExitPoint, const FVector& Dir, float Amount, AZoogiBall* By)
{
	if (!World) return nullptr;
	// Mostly vertical fall (the ball is already well below the rim when it is called out): use the radial direction.
	const FVector2D Hz(Dir);
	const FVector2D D2 = (Hz.Size() > 0.3f * Dir.Size() && Hz.Size() > 20.f) ? Hz.GetSafeNormal() : FVector2D(ExitPoint).GetSafeNormal();
	const FVector2D E2 = FVector2D(ExitPoint).GetSafeNormal() * FMath::Min(FVector2D(ExitPoint).Size(), AZoogiArena::ActiveKnockoffR + 60.f);
	AZoogiArenaPiece* Best = nullptr;
	float BestScore = 1e9f;
	for (TActorIterator<AZoogiArenaPiece> It(World); It; ++It)
	{
		AZoogiArenaPiece* A = *It;
		if (A->bIndestructible || A->State == EZoogiPieceState::Broken) continue;
		const FVector2D P(A->GetActorLocation());
		if (P.Size() < AZoogiArena::ActiveKnockoffR - 20.f) continue;
		const FVector2D To = P - E2;
		const float Dist = To.Size();
		if (Dist > 3200.f) continue;
		const float Cos = D2.IsNearlyZero() ? FVector2D::DotProduct(To / FMath::Max(1.f, Dist), E2.GetSafeNormal()) : FVector2D::DotProduct(To / FMath::Max(1.f, Dist), D2);
		if (Cos < 0.82f) continue; // ~35 degree cone
		const float Score = Dist * (2.f - Cos);
		if (Score < BestScore) { BestScore = Score; Best = A; }
	}
	if (!Best)
	{
		// Nothing in the flight cone: the nearest rim piece to the exit point takes it (within ~9 m).
		for (TActorIterator<AZoogiArenaPiece> It(World); It; ++It)
		{
			AZoogiArenaPiece* A = *It;
			if (A->bIndestructible || A->State == EZoogiPieceState::Broken) continue;
			const FVector2D P(A->GetActorLocation());
			if (P.Size() < AZoogiArena::ActiveKnockoffR - 20.f) continue;
			const float Dist = FVector2D::Distance(P, E2);
			if (Dist < 900.f && Dist < BestScore) { BestScore = Dist; Best = A; }
		}
	}
	if (Best)
	{
		UE_LOG(LogZoogi, Log, TEXT("Ring-out crash: %s (%s) takes %.0f"), *Best->GetName(), *Best->Kind.ToString(), Amount);
		Best->ApplyDamage(Amount, Best->GetActorLocation() + FVector(0, 0, 40.f), FVector(D2, 0.f), By && By->Artifact ? By->Artifact->Infused : EZoogiArtifact::None, By);
	}
	return Best;
}
