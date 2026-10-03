#include "ZoogiTargetMarker.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AZoogiTargetMarker::AZoogiTargetMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetCastShadows(false);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(0.f);
	Glow->SetAttenuationRadius(450.f);
}

UStaticMeshComponent* AZoogiTargetMarker::MakePart(const TCHAR* Shape, float Strength, bool bRing)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetupAttachment(Root);
	UMaterialInstanceDynamic* M = ZoogiUtil::FxMID(this, FLinearColor::White, Strength);
	M->SetScalarParameterValue(TEXT("Ring"), bRing ? 1.f : 0.f);
	C->SetMaterial(0, M);
	C->RegisterComponent();
	MIDs.Add(M);
	return C;
}

void AZoogiTargetMarker::Build()
{
	if (bBuilt) return;
	bBuilt = true;
	Ring = MakePart(TEXT("Cylinder"), 6.f, true);
	InnerRing = MakePart(TEXT("Cylinder"), 3.f, true);
	for (int32 i = 0; i < 4; ++i) Ticks.Add(MakePart(TEXT("Cube"), 8.f, false));
	Shell = MakePart(TEXT("Sphere"), 1.6f, false);
	Arrow = MakePart(TEXT("Cone"), 9.f, false);
}

void AZoogiTargetMarker::SetTarget(AActor* InTarget, EZoogiMarkerStyle InStyle, const FLinearColor& InColor)
{
	Build();
	if (!InTarget) InStyle = EZoogiMarkerStyle::Hidden;
	if (InStyle != Style || InTarget != Target.Get()) StyleAge = 0.f;
	Target = InTarget;
	Style = InStyle;
	if (!Color.Equals(InColor))
	{
		Color = InColor;
		for (UMaterialInstanceDynamic* M : MIDs) if (M) M->SetVectorParameterValue(TEXT("Color"), Color);
		Glow->SetLightColor(Color);
	}
	Tick(0.f);
}

void AZoogiTargetMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt) return;
	Age += DeltaSeconds;
	StyleAge += DeltaSeconds;
	AActor* T = Target.Get();
	const bool bShow = T && Style != EZoogiMarkerStyle::Hidden;
	SetActorHiddenInGame(!bShow);
	if (!bShow) { Glow->SetIntensity(0.f); return; }

	const bool bOrb = Cast<AZoogiOrb>(T) != nullptr;
	const FVector L = bOrb ? Cast<AZoogiOrb>(T)->OrbLocation() : (Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation());
	const float R = bOrb ? ZoogiConst::OrbRadius : ZoogiConst::ZoogiRadius;
	SetActorLocation(FVector(L.X, L.Y, L.Z - R + 3.f)); // on the floor under the target
	const bool bLock = Style == EZoogiMarkerStyle::Lock;
	const bool bSoft = Style == EZoogiMarkerStyle::SoftLock;
	// Lock-in: the ring snaps in from wide to tight over 0.25 s.
	const float In = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(StyleAge / 0.25f, 0.f, 1.f), 3.f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Age * 6.f);
	const float RingR = (R + (bLock ? 45.f : 35.f)) * FMath::Lerp(bLock ? 2.2f : 1.3f, 1.f, In) * (1.f + 0.04f * Pulse);
	const float Thick = bLock ? 0.035f : 0.02f;
	Ring->SetRelativeScale3D(FVector(RingR / 50.f, RingR / 50.f, Thick));
	Ring->SetRelativeLocation(FVector::ZeroVector);
	InnerRing->SetVisibility(bLock || bSoft);
	InnerRing->SetRelativeScale3D(FVector((RingR - 22.f) / 50.f, (RingR - 22.f) / 50.f, 0.015f));
	const float Strength = bLock ? 7.f + 3.f * Pulse : (bSoft ? 4.5f : 2.2f + Pulse);
	if (MIDs.IsValidIndex(0)) MIDs[0]->SetScalarParameterValue(TEXT("Strength"), Strength);

	// Rotating reticle brackets.
	for (int32 i = 0; i < Ticks.Num(); ++i)
	{
		UStaticMeshComponent* K = Ticks[i];
		K->SetVisibility(bLock);
		// Brackets swing in from wide with an extra half turn as the lock lands, then idle-rotate.
		const float A = FMath::DegreesToRadians(Age * 90.f + i * 90.f + (1.f - In) * 180.f);
		const float KR = RingR + 26.f + (1.f - In) * 90.f;
		K->SetRelativeLocation(FVector(FMath::Cos(A) * KR, FMath::Sin(A) * KR, 6.f));
		K->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(A), 0.f));
		K->SetRelativeScale3D(FVector(0.34f, 0.08f, 0.06f));
	}
	// Highlight shell around the target itself.
	Shell->SetVisibility(bLock || bSoft);
	Shell->SetRelativeLocation(FVector(0.f, 0.f, R - 3.f));
	Shell->SetRelativeScale3D(FVector((R + 9.f + 4.f * Pulse) / 50.f));
	if (MIDs.IsValidIndex(6)) MIDs[6]->SetScalarParameterValue(TEXT("Strength"), (bLock ? 1.2f + 0.8f * Pulse : 0.8f) + (1.f - In) * 3.f); // flash on lock
	// Bobbing arrow above.
	Arrow->SetVisibility(bLock);
	Arrow->SetRelativeLocation(FVector(0.f, 0.f, 2.f * R + 95.f + 22.f * FMath::Sin(Age * 3.5f)));
	Arrow->SetRelativeRotation(FRotator(180.f, Age * 120.f, 0.f));
	Arrow->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.55f));
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Glow->SetIntensity((bLock ? 60.f + 30.f * Pulse : (bSoft ? 25.f : 0.f)) * (1.f + (1.f - In) * 1.5f));
}
