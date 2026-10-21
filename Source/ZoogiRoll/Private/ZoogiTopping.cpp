#include "ZoogiTopping.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiFX.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

AZoogiTopping::AZoogiTopping()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while popping
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetMobility(EComponentMobility::Movable);

	// Query-only trigger on the Pawn channel (Zoogis) and nothing else: no blocking, no physics, invisible to the
	// ECC_WorldStatic aim / bank sweeps, the grounded probe (WorldStatic objects) and the camera probe.
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(30.f);
	Trigger->SetMobility(EComponentMobility::Movable);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);
}

void AZoogiTopping::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AZoogiTopping::OnTriggerOverlap);
}

UStaticMeshComponent* AZoogiTopping::MakePart(UStaticMesh* Mesh, UMaterialInterface* Mat, USceneComponent* Parent, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Parent);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(bShadow);
	if (Mat) C->SetMaterial(0, Mat);
	C->RegisterComponent();
	return C;
}

namespace
{
	/** Basic Cylinder resized like AZoogiArena::AddCylinder: radius, height, top Z (relative to its parent). */
	void ToppingFitCylinder(UStaticMeshComponent* C, const FVector2D& XY, float R, float H, float TopZ)
	{
		UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
		if (!Mesh) return;
		const FBoxSphereBounds B = Mesh->GetBounds();
		const FVector S(R / B.BoxExtent.X, R / B.BoxExtent.Y, H / (2.f * B.BoxExtent.Z));
		C->SetRelativeScale3D(S);
		C->SetRelativeLocation(FVector(XY.X, XY.Y, TopZ - (B.Origin.Z + B.BoxExtent.Z) * S.Z));
	}
}

void AZoogiTopping::InitTopping(EZoogiToppingKind InKind, float InRadius, float InYaw, int32 InSeed)
{
	Kind = InKind; Radius = InRadius; Yaw = InYaw; Seed = InSeed;
	UStaticMesh* Sphere = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	FRandomStream Rng(Seed);
	Visual->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	UMaterialInterface* BitMatA = nullptr;
	UMaterialInterface* BitMatB = nullptr;

	if (Kind == EZoogiToppingKind::Pepperoni)
	{
		// Flat slice: darker curled rim, glossy deep-red face, a few dark speckles and one pale fat fleck.
		UMaterialInterface* Face = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B3261E")), 0.38f);
		UMaterialInterface* Rim = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#6E140E")), 0.5f);
		UMaterialInterface* Speck = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#4E0B08")), 0.6f);
		UMaterialInterface* Fat = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#E58A6B")), 0.45f);
		UStaticMeshComponent* R = MakePart(Cyl, Rim, Visual, true);
		ToppingFitCylinder(R, FVector2D::ZeroVector, Radius, 5.f, 5.5f);
		Parts.Add(R);
		UStaticMeshComponent* F = MakePart(Cyl, Face, Visual, false);
		ToppingFitCylinder(F, FVector2D::ZeroVector, Radius * 0.84f, 4.6f, 5.6f);
		Parts.Add(F);
		for (int32 i = 0; i < 4; ++i)
		{
			UStaticMeshComponent* S = MakePart(Sphere, i == 3 ? Fat : Speck, Visual, false);
			const float A = Rng.FRandRange(0.f, 2.f * PI), D = Rng.FRandRange(0.1f, 0.55f) * Radius;
			const float SR = Radius * (i == 3 ? Rng.FRandRange(0.08f, 0.12f) : Rng.FRandRange(0.1f, 0.16f));
			S->SetRelativeLocation(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 5.8f));
			S->SetRelativeRotation(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
			S->SetRelativeScale3D(FVector(SR * 2.f / 100.f, SR * 1.6f / 100.f, 0.012f));
			Parts.Add(S);
		}
		BitMatA = Face; BitMatB = Rim;
	}
	else
	{
		// Melted cheese: a pale mozzarella blob with bubbles and a browned blister, or a wedge-ish chunk melting
		// into a puddle. A faint warm emissive keeps it readable under the moonlight.
		UMaterialInterface* Cheese = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FFF2C4")), 0.42f, ZoogiUtil::Hex(TEXT("#FFD25E")), 0.06f);
		UMaterialInterface* Bubble = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FFD86B")), 0.35f);
		UMaterialInterface* Brown = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C98A35")), 0.6f);
		if (Seed % 2 == 0)
		{
			UStaticMeshComponent* B = MakePart(Sphere, Cheese, Visual, true);
			B->SetRelativeScale3D(FVector(Radius * 2.f / 100.f, Radius * 1.7f / 100.f, 0.22f)); // 11 cm above the floor
			Parts.Add(B);
			for (int32 i = 0; i < 2; ++i)
			{
				UStaticMeshComponent* Bb = MakePart(Sphere, Bubble, Visual, false);
				const float A = Rng.FRandRange(0.f, 2.f * PI), D = Rng.FRandRange(0.15f, 0.45f) * Radius;
				const float BR = Radius * (i ? 0.22f : 0.3f);
				Bb->SetRelativeLocation(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D * 0.85f, 7.f));
				Bb->SetRelativeScale3D(FVector(BR * 2.f / 100.f, BR * 2.f / 100.f, BR * 1.2f / 100.f));
				Parts.Add(Bb);
			}
			UStaticMeshComponent* Bl = MakePart(Sphere, Brown, Visual, false);
			Bl->SetRelativeLocation(FVector(-Radius * 0.25f, Radius * 0.2f, 10.2f));
			Bl->SetRelativeScale3D(FVector(Radius * 0.36f / 100.f, Radius * 0.28f / 100.f, 0.02f));
			Parts.Add(Bl);
		}
		else
		{
			UStaticMeshComponent* Pd = MakePart(Sphere, Cheese, Visual, false);
			Pd->SetRelativeScale3D(FVector(Radius * 2.3f / 100.f, Radius * 1.9f / 100.f, 0.02f)); // melt puddle
			Pd->SetRelativeLocation(FVector(0.f, 0.f, 1.5f)); // above the floor's sauce / browned decals
			Parts.Add(Pd);
			UStaticMeshComponent* Ck = MakePart(Cube, Cheese, Visual, true);
			Ck->SetRelativeScale3D(FVector(Radius * 1.5f / 100.f, Radius * 1.f / 100.f, 0.2f));
			Ck->SetRelativeRotation(FRotator(8.f, 0.f, 0.f)); // one end raised: reads as a wedge
			Ck->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
			Parts.Add(Ck);
			UStaticMeshComponent* Bb = MakePart(Sphere, Bubble, Visual, false);
			Bb->SetRelativeLocation(FVector(Radius * 0.35f, -Radius * 0.15f, 19.f));
			Bb->SetRelativeScale3D(FVector(Radius * 0.4f / 100.f, Radius * 0.4f / 100.f, Radius * 0.22f / 100.f));
			Parts.Add(Bb);
		}
		BitMatA = Cheese; BitMatB = Bubble;
	}

	// Trigger at marble-centre height: a Zoogi pops it once its body is over the topping's edge.
	Trigger->SetSphereRadius(Radius * 0.75f);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, ZoogiConst::ZoogiRadius));

	// Bit pool for the pop (flat crumbs for pepperoni, nuggets for cheese), hidden until used.
	for (int32 i = 0; i < BitCount; ++i)
	{
		UStaticMesh* M = Kind == EZoogiToppingKind::Pepperoni ? Cyl : (i % 2 ? Cube : Sphere);
		UStaticMeshComponent* C = MakePart(M, i % 3 == 2 ? BitMatB : BitMatA, Root, false);
		C->SetVisibility(false);
		BitPool.Add(C);
	}
	Bits.SetNum(BitPool.Num());
	ResetTopping();
}

void AZoogiTopping::ResetTopping()
{
	bPopped = false;
	bPopping = false;
	PopAge = 0.f;
	if (Visual) { Visual->SetRelativeScale3D(FVector::OneVector); Visual->SetVisibility(true, true); }
	if (Trigger) Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	for (int32 i = 0; i < Bits.Num(); ++i) { Bits[i].bLive = false; if (BitPool.IsValidIndex(i) && BitPool[i]) BitPool[i]->SetVisibility(false); }
	SetActorTickEnabled(false);
}

void AZoogiTopping::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPopped) return;
	AZoogiBall* Ball = Cast<AZoogiBall>(OtherActor);
	if (!Ball || Ball->bOut) return;
	Pop(Ball->BallLocation(), Ball->GetBallVelocity());
}

void AZoogiTopping::Pop(const FVector& From, const FVector& Vel)
{
	bPopped = true; // stays gone for the rest of the match (ResetTopping at match start)
	bPopping = true;
	PopAge = 0.f;
	++PopCount;
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Bits fly mostly along the marble's travel (or away from it), fanned out, popping upward.
	FVector Dir = Vel.Size2D() > 50.f ? Vel.GetSafeNormal2D() : (GetActorLocation() - From).GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
	FRandomStream Rng(Seed * 37 + PopCount * 11);
	const bool bPep = Kind == EZoogiToppingKind::Pepperoni;
	for (int32 i = 0; i < Bits.Num(); ++i)
	{
		FBit& B = Bits[i];
		B.bLive = true;
		B.Age = 0.f;
		const float S = Radius * Rng.FRandRange(0.3f, 0.5f) / 100.f;
		B.Scale = bPep ? FVector(S, S, S * 0.25f) : FVector(S, S * Rng.FRandRange(0.7f, 1.f), S * Rng.FRandRange(0.6f, 0.9f));
		const float A = Rng.FRandRange(0.f, 2.f * PI), D = Rng.FRandRange(0.f, 0.5f) * Radius;
		B.Pos = FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 8.f);
		const FVector Fan = Dir.RotateAngleAxis(Rng.FRandRange(-110.f, 110.f), FVector::UpVector);
		B.Vel = Fan * Rng.FRandRange(160.f, 380.f) + FVector(0.f, 0.f, Rng.FRandRange(260.f, 460.f));
		B.Rot = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f));
		B.Spin = FRotator(Rng.FRandRange(-720.f, 720.f), Rng.FRandRange(-720.f, 720.f), Rng.FRandRange(-720.f, 720.f));
		UStaticMeshComponent* M = BitPool[i];
		M->SetRelativeLocationAndRotation(B.Pos, B.Rot);
		M->SetRelativeScale3D(B.Scale);
		M->SetVisibility(true);
	}
	// Cheap existing hooks: one small additive ring puff (AZoogiFX) and the short HitClick tick pitched up as a "pop".
	const FLinearColor PuffColor = bPep ? FLinearColor(1.f, 0.35f, 0.2f) : FLinearColor(1.f, 0.85f, 0.35f);
	AZoogiFX::Spawn(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 8.f), PuffColor, Radius * 2.2f, 0.3f, true, 2.5f);
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.55f, bPep ? Rng.FRandRange(1.7f, 2.f) : Rng.FRandRange(1.4f, 1.6f));
	SetActorTickEnabled(true);
	UE_LOG(LogZoogi, Log, TEXT("Topping %s (%s) popped"), *GetName(), bPep ? TEXT("pepperoni") : TEXT("cheese"));
	// -ZoogiPopShots (screenshot runs): capture the first pops mid-air (bits rising / at the top of their arc).
	static const bool bPopShots = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPopShots"));
	static int32 PopShotCount = 0;
	if (bPopShots && PopShotCount < 8)
	{
		if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
		{
			++PopShotCount;
			G->TakeShotDelayed(FString::Printf(TEXT("pop%d_%s_a"), PopShotCount, bPep ? TEXT("pep") : TEXT("cheese")), 0.1f);
			G->TakeShotDelayed(FString::Printf(TEXT("pop%d_%s_b"), PopShotCount, bPep ? TEXT("pep") : TEXT("cheese")), 0.25f);
		}
	}
}

void AZoogiTopping::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bPopping)
	{
		// Squash: widen and flatten for the first 45%, then shrink to nothing.
		PopAge += Dt;
		const float T = PopAge / FMath::Max(PopTime, 0.01f);
		if (T >= 1.f)
		{
			bPopping = false;
			Visual->SetVisibility(false, true);
			Visual->SetRelativeScale3D(FVector::OneVector);
		}
		else if (T < 0.45f)
		{
			const float A = T / 0.45f;
			Visual->SetRelativeScale3D(FVector(FMath::Lerp(1.f, 1.35f, A), FMath::Lerp(1.f, 1.35f, A), FMath::Lerp(1.f, 0.55f, A)));
		}
		else
		{
			const float K = 1.f - (T - 0.45f) / 0.55f;
			Visual->SetRelativeScale3D(FVector(1.35f, 1.35f, 0.55f) * FMath::Max(K, 0.01f));
		}
	}
	// Scripted bits (cosmetic, no collision): gravity, one damped floor bounce, spin, shrink over the last 0.3 s.
	bool bAny = false;
	const float FloorZ = -GetActorLocation().Z; // court top is Z = 0
	for (int32 i = 0; i < Bits.Num(); ++i)
	{
		FBit& B = Bits[i];
		if (!B.bLive) continue;
		B.Age += Dt;
		UStaticMeshComponent* M = BitPool[i];
		if (B.Age >= BitLife) { B.bLive = false; M->SetVisibility(false); continue; }
		bAny = true;
		B.Vel.Z -= 980.f * Dt;
		B.Pos += B.Vel * Dt;
		const float Half = 50.f * B.Scale.Z;
		if (B.Pos.Z < FloorZ + Half && B.Vel.Z < 0.f)
		{
			B.Pos.Z = FloorZ + Half;
			B.Vel.Z *= -0.3f;
			B.Vel.X *= 0.6f; B.Vel.Y *= 0.6f;
			B.Spin *= 0.6f;
		}
		B.Rot += B.Spin * Dt;
		const float Fade = B.Age < BitLife - 0.3f ? 1.f : FMath::Max(0.f, (BitLife - B.Age) / 0.3f);
		M->SetRelativeLocationAndRotation(B.Pos, B.Rot);
		M->SetRelativeScale3D(B.Scale * Fade);
	}
	if (!bAny && !bPopping) SetActorTickEnabled(false);
}
