#include "ZoogiIceBarrier.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFabFX.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	UStaticMesh* IceMesh() { static UStaticMesh* M = []() { UStaticMesh* X = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StylizedToonVFX3/Models/SM_Icewall.SM_Icewall"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (X) X->AddToRoot(); return X; }(); return M; }
	UStaticMesh* ChunkMesh() { static UStaticMesh* M = []() { UStaticMesh* X = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StylizedToonVFX3/Models/SM_HOVLCrystalPiese.SM_HOVLCrystalPiese"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (X) X->AddToRoot(); return X; }(); return M; } // b117: rooted - a GC'd cached pointer crashed Burst
	UMaterialInterface* IceMat() { static UMaterialInterface* M = []() { UMaterialInterface* X = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StylizedToonVFX3/Materials/M_Ice3.M_Ice3"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (X) X->AddToRoot(); return X; }(); return M; } // b117: rooted (see ChunkMesh)
}

AZoogiIceBarrier::AZoogiIceBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Wall"));
	RootComponent = Box;
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Box->SetNotifyRigidBodyCollision(true);
	Box->SetGenerateOverlapEvents(false);
}

AZoogiIceBarrier* AZoogiIceBarrier::Spawn(UWorld* W, const FVector& A, const FVector& B, float Height, int32 OwnerIdx, int32 Rounds)
{
	if (!W) return nullptr;
	const FVector Mid = (A + B) * 0.5f;
	const float L = FMath::Max(120.f, FVector::Dist2D(A, B));
	const float Yaw = (B - A).Rotation().Yaw;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiIceBarrier* X = W->SpawnActor<AZoogiIceBarrier>(AZoogiIceBarrier::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Mid + FVector(0, 0, Height * 0.5f)), P);
	if (!X) return nullptr;
	X->Len = L; X->Hgt = Height; X->Owner = OwnerIdx; X->LifeRounds = Rounds;
	if (const AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>()) X->BornRound = G->RoundNumber;
	X->Box->SetBoxExtent(FVector(L * 0.5f, 26.f, Height * 0.5f));
	X->Box->OnComponentHit.AddDynamic(X, &AZoogiIceBarrier::OnHit);
	// the Hovl ice spires along the wall (mesh scaled to fit), rising out of the floor
	if (UStaticMesh* M = IceMesh())
	{
		const FBox Bb = M->GetBoundingBox();
		const FVector Sz = Bb.GetSize();
		const int32 N = FMath::Max(1, FMath::RoundToInt(L / 170.f));
		for (int32 k = 0; k < N; ++k)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(X);
			C->SetStaticMesh(M);
			if (UMaterialInterface* Mi = IceMat()) for (int32 s = 0; s < C->GetNumMaterials(); ++s) C->SetMaterial(s, Mi);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			C->SetupAttachment(X->Box);
			// fit one piece to (L / N) long, Height tall; the mesh's long horizontal axis follows the wall
			const bool bXLong = Sz.X >= Sz.Y;
			const float Long = FMath::Max(1.f, bXLong ? Sz.X : Sz.Y), Up = FMath::Max(1.f, Sz.Z);
			const float SL = (L / N) * 1.05f / Long, SH = Height * 1.1f / Up;
			C->SetRelativeRotation(FRotator(0.f, bXLong ? 0.f : 90.f, 0.f));
			C->SetRelativeScale3D(FVector(SL, SL * 0.9f, SH));
			const float X0 = -L * 0.5f + (k + 0.5f) * L / N;
			C->SetRelativeLocation(FVector(X0, 0.f, -Height * 0.5f - Bb.Min.Z * SH - Height)); // starts under the floor (Rise lifts it)
			C->RegisterComponent();
			X->Ice.Add(C);
		}
	}
	AZoogiFabFX::Play(W, TEXT("/Game/StylizedToonVFX3/Effects/NS_Ice_wall.NS_Ice_wall"), Mid, FMath::Clamp(L / 1200.f, 0.3f, 0.6f), 3.f, /* b71 (Grok): was 0.2-0.45 */ nullptr, FVector::ZeroVector, FRotator(0.f, Yaw + 90.f, 0.f));
	AZoogiFabFX::Sound(W, TEXT("/Game/StylizedToonVFX3/Audio/Cues/S_Ice_wall_Cue.S_Ice_wall_Cue"), Mid, 0.9f);
	UE_LOG(LogZoogi, Log, TEXT("ICEWALL up: %.0f cm long, %.0f tall at (%.0f, %.0f) yaw %.0f (%d spires%s)"), L, Height, Mid.X, Mid.Y, Yaw, X->Ice.Num(), IceMesh() ? TEXT("") : TEXT(", SM_Icewall missing"));
	return X;
}

void AZoogiIceBarrier::DestroyAll(UWorld* W)
{
	if (!W) return;
	for (TActorIterator<AZoogiIceBarrier> It(W); It; ++It) It->Destroy();
}

void AZoogiIceBarrier::OnHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (BurstAge >= 0.f || !OtherComp) return;
	const float T = GetWorld()->GetTimeSeconds();
	if (T - LastHit < 0.25f || Rise < 1.f) return; // b64: no damage while it rises (a ball it pushed aside is not a hit)
	const float Speed = OtherComp->GetPhysicsLinearVelocity().Size();
	const bool bBall = Cast<AZoogiBall>(Other) != nullptr, bOrb = Cast<AZoogiOrb>(Other) != nullptr;
	if (!bBall && !bOrb) return;
	const float Imp = FMath::Max(Speed, NormalImpulse.Size() / FMath::Max(1.f, OtherComp->GetMass()));
	if (Imp < 260.f) return;
	LastHit = T;
	float St = (bBall ? 1.f : 0.4f) * (Imp > 1500.f ? 2.f : 1.f);
	// b124 (Evan: "ice walls built from players ... destroyed after three hits"): a player's wall takes exactly three Zoogi hits
	// (soft or hard - each hit is one crack stage); orbs bounce off it. -ZoogiIceWallOldDamage = the old speed-based damage
	static const bool bOldDmg = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallOldDamage"));
	if (Owner >= 0 && !bOldDmg) { if (!bBall) return; St = 1.f; }
	Damage(St, Hit.ImpactPoint, -Hit.ImpactNormal);
}

void AZoogiIceBarrier::Damage(float Stages, const FVector& At, const FVector& Dir)
{
	if (BurstAge >= 0.f) return;
	const int32 Was = Stage;
	Dmg += Stages;
	Stage = FMath::Min(3, FMath::FloorToInt(Dmg));
	UE_LOG(LogZoogi, Log, TEXT("ICEWALL hit +%.1f -> %.1f (stage %d)"), Stages, Dmg, Stage);
	ZoogiUtil::PlaySfx(this, TEXT("OrbClink"), 0.9f, 0.6f + 0.1f * Stage);
	if (Stage >= 3) { Burst(Dir); return; }
	if (Stage == Was) return;
	if (const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) if (!G->ShotDir.IsEmpty()) const_cast<AZoogiGameMode*>(G)->TakeShotDelayed(FString::Printf(TEXT("icewall_crack_%d"), Stage), 0.05f);
	// crack lines: jagged bright-blue streaks on both faces, spreading from the hit point (more per stage)
	const FTransform Tr = Box->GetComponentTransform();
	const FVector Loc = Tr.InverseTransformPosition(At);
	FRandomStream R(FMath::Rand());
	const int32 NL = Stage == 1 ? 5 : 9;
	for (int32 Face = -1; Face <= 1; Face += 2)
		for (int32 i = 0; i < NL; ++i)
		{
			FVector P(FMath::Clamp(Loc.X, -Len * 0.45f, Len * 0.45f), Face * 27.f, FMath::Clamp(Loc.Z, -Hgt * 0.4f, Hgt * 0.4f));
			float Ang = R.FRandRange(0.f, 360.f);
			const int32 Kinks = Stage == 1 ? 2 : 3;
			for (int32 k = 0; k < Kinks; ++k)
			{
				const float Seg = R.FRandRange(14.f, 34.f) * (Stage == 1 ? 1.f : 1.5f);
				const FVector D(FMath::Cos(FMath::DegreesToRadians(Ang)), 0.f, FMath::Sin(FMath::DegreesToRadians(Ang)));
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
				C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
				C->SetMaterial(0, ZoogiUtil::GlowMID(this, FLinearColor(0.75f, 0.95f, 1.f), 6.f));
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetCastShadow(false);
				C->SetupAttachment(Box);
				C->SetRelativeLocation(P + D * Seg * 0.5f);
				C->SetRelativeRotation(FRotator(-Ang, 0.f, 0.f));
				C->SetRelativeScale3D(FVector(Seg / 100.f, 0.015f, 0.025f));
				C->RegisterComponent();
				Cracks.Add(C);
				P += D * Seg; Ang += R.FRandRange(-40.f, 40.f);
			}
		}
	// chips fly off the hit
	AZoogiFabFX::Play(GetWorld(), TEXT("/Game/StylizedToonVFX3/Effects/NS_Ice_wall_2.NS_Ice_wall_2"), FVector(At.X, At.Y, GetActorLocation().Z - Hgt * 0.5f), 0.25f, 1.2f); // b71 (Grok): burst 0.12 -> 0.25
	for (UStaticMeshComponent* C : Ice) if (C) C->SetRelativeLocation(C->GetRelativeLocation() + FVector(0.f, R.FRandRange(-3.f, 3.f), 0.f)); // a shudder
}

void AZoogiIceBarrier::Burst(const FVector& Dir)
{
	if (BurstAge >= 0.f) return;
	BurstAge = 0.f;
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); // gone as a wall the moment it bursts
	for (UStaticMeshComponent* C : Ice) if (C) C->SetVisibility(false);
	for (UStaticMeshComponent* C : Cracks) if (C) C->SetVisibility(false);
	// ice blocks blast away with physics (they hit the world but never push Zoogis / orbs)
	FRandomStream R(FMath::Rand());
	const int32 NB = FMath::Clamp(FMath::RoundToInt(Len / 28.f), 8, 20);
	UStaticMesh* CM = ChunkMesh() ? ChunkMesh() : ZoogiUtil::BasicShape(TEXT("Cube"));
	const FVector Push = Dir.IsNearlyZero() ? GetActorRightVector() : Dir.GetSafeNormal2D();
	for (int32 i = 0; i < NB; ++i)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(CM);
		if (UMaterialInterface* Mi = IceMat()) C->SetMaterial(0, Mi);
		const FVector L(R.FRandRange(-Len * 0.45f, Len * 0.45f), R.FRandRange(-15.f, 15.f), R.FRandRange(-Hgt * 0.4f, Hgt * 0.4f));
		C->SetWorldLocation(GetActorTransform().TransformPosition(L));
		C->SetWorldRotation(FRotator(R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f)));
		const float S = R.FRandRange(0.6f, 1.2f);
		const FBox Bb = CM->GetBoundingBox();
		C->SetWorldScale3D(FVector(S * 26.f / FMath::Max(1.f, Bb.GetSize().GetMax())));
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->SetCollisionObjectType(ECC_WorldDynamic);
		C->RegisterComponent();
		C->SetSimulatePhysics(true);
		C->SetMassOverrideInKg(NAME_None, 2.f, true);
		const FVector V = Push * R.FRandRange(250.f, 700.f) + GetActorForwardVector() * R.FRandRange(-250.f, 250.f) + FVector(0, 0, R.FRandRange(300.f, 650.f));
		C->SetPhysicsLinearVelocity(V);
		C->SetPhysicsAngularVelocityInDegrees(FVector(R.FRandRange(-540.f, 540.f), R.FRandRange(-540.f, 540.f), R.FRandRange(-540.f, 540.f)));
		Blocks.Add(C);
	}
	AZoogiFabFX::Play(GetWorld(), TEXT("/Game/StylizedToonVFX3/Effects/NS_Ice_wall_2.NS_Ice_wall_2"), GetActorLocation() - FVector(0, 0, Hgt * 0.5f), FMath::Clamp(Len / 1200.f, 0.2f, 0.45f), 2.f, nullptr, FVector::ZeroVector, GetActorRotation() + FRotator(0.f, 90.f, 0.f));
	ZoogiUtil::PlaySfx(this, TEXT("Explosion"), 0.5f, 1.6f);
	AZoogiFabFX::Sound(GetWorld(), TEXT("/Game/StylizedToonVFX3/Audio/Cues/S_Ice_wall_Cue.S_Ice_wall_Cue"), GetActorLocation(), 1.f);
	UE_LOG(LogZoogi, Log, TEXT("ICEWALL burst into %d ice blocks"), NB);
	if (const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>()) if (!G->ShotDir.IsEmpty()) { const_cast<AZoogiGameMode*>(G)->TakeShotDelayed(TEXT("icewall_burst_a"), 0.15f); const_cast<AZoogiGameMode*>(G)->TakeShotDelayed(TEXT("icewall_burst_b"), 0.6f); }
}

void AZoogiIceBarrier::Tick(float Dt)
{
	Super::Tick(Dt);
	if (BurstAge >= 0.f)
	{
		BurstAge += Dt;
		if (BurstAge > 2.2f) // blocks shrink away
		{
			const float K = FMath::Clamp(1.f - (BurstAge - 2.2f) / 0.6f, 0.f, 1.f);
			for (UStaticMeshComponent* C : Blocks) if (C) C->SetWorldScale3D(C->GetComponentScale() * FMath::Max(K, 0.9f));
		}
		if (BurstAge > 2.9f) Destroy();
		return;
	}
	if (Rise < 1.f)
	{
		const float Was = Rise;
		Rise = FMath::Min(1.f, Rise + Dt / 0.45f);
		const float E = 1.f - FMath::Pow(1.f - Rise, 3.f), E0 = 1.f - FMath::Pow(1.f - Was, 3.f);
		for (UStaticMeshComponent* C : Ice) if (C) C->SetRelativeLocation(C->GetRelativeLocation() + FVector(0.f, 0.f, Hgt * (E - E0)));
	}
	if (LifeRounds > 0)
		if (const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
			if (G->RoundNumber >= BornRound + LifeRounds) Burst(FVector::ZeroVector); // melts / bursts on its own eventually
}
