#include "ZoogiLaunchPad.h"
#include "ZoogiRoll.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/CommandLine.h"

AZoogiLaunchPad::AZoogiLaunchPad()
{
	PrimaryActorTick.bCanEverTick = false; // static dressing: no per-frame cost
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	LaunchPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaunchPoint"));
	LaunchPoint->SetupAttachment(Root);
	LaunchPoint->SetRelativeLocation(FVector(0, 0, ZoogiConst::ZoogiRadius + 2.f));
}

FTransform AZoogiLaunchPad::GetLaunchTransform() const
{
	return FTransform(LaunchDirection.Rotation(), LaunchPoint->GetComponentLocation());
}

AActor* AZoogiLaunchPad::GetOccupant() const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	TArray<FOverlapResult> Hits;
	FCollisionQueryParams Q(TEXT("LaunchPadOccupant"), false, this);
	const FVector C = GetActorLocation() + FVector(0, 0, 90.f); // box from the deck top up ~180 cm
	const FCollisionShape Box = FCollisionShape::MakeBox(FVector(ZoogiLaunch::DeckRadius, ZoogiLaunch::DeckRadius, 90.f));
	W->OverlapMultiByObjectType(Hits, C, GetActorQuat(), FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllDynamicObjects), Box, Q);
	for (const FOverlapResult& H : Hits)
	{
		AActor* A = H.GetActor();
		if (A && A != this && A != GetOwner()) return A;
	}
	return nullptr;
}

void AZoogiLaunchPad::Setup(EZoogiArena InTheme, const FVector& InLandingTarget)
{
	Theme = InTheme;
	LandingTarget = InLandingTarget;
	const FVector From = LaunchPoint->GetComponentLocation();
	const FVector Flat = FVector(InLandingTarget.X - From.X, InLandingTarget.Y - From.Y, 0.f);
	const float D = Flat.Size();
	const float Drop = From.Z - InLandingTarget.Z; // positive: target below
	const float G = FMath::Abs(GetWorld() ? GetWorld()->GetGravityZ() : -980.f);
	const float Th = FMath::DegreesToRadians(ZoogiLaunch::LaunchAngleDeg);
	const float C = FMath::Cos(Th), T = FMath::Tan(Th);
	const float Den = 2.f * C * C * FMath::Max(1.f, D * T + Drop);
	LaunchSpeed = FMath::Sqrt(G * D * D / Den);
	LaunchDirection = (Flat.GetSafeNormal() * C + FVector::UpVector * FMath::Sin(Th)).GetSafeNormal();
	LandingSpeed = FMath::Sqrt(LaunchSpeed * LaunchSpeed + 2.f * G * Drop);
	BuildDressing();
	UE_LOG(LogZoogi, Log, TEXT("LaunchPad %s at (%.0f, %.0f, %.0f) yaw %.0f -> land (%.0f, %.0f) D %.0f speed %.0f cm/s landing %.0f cm/s"),
		*ZoogiUtil::ArenaName(InTheme), From.X, From.Y, From.Z, GetActorRotation().Yaw, InLandingTarget.X, InLandingTarget.Y, D, LaunchSpeed, LandingSpeed);
}

UStaticMeshComponent* AZoogiLaunchPad::Part(const TCHAR* Shape, UMaterialInterface* Mat, const FVector& Loc, const FRotator& Rot, const FVector& Scale, bool bDeck)
{
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
	M->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
	M->SetMaterial(0, Mat);
	M->SetupAttachment(Root);
	M->SetRelativeTransform(FTransform(Rot, Loc, Scale));
	M->SetCastShadow(bDeck);
	M->ComponentTags.Add(TEXT("NoCamBlock"));
	if (bDeck) // the deck is standable; never blocks the camera
	{
		M->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		M->SetCollisionResponseToAllChannels(ECR_Block);
		M->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	M->RegisterComponent();
	Parts.Add(M);
	return M;
}

void AZoogiLaunchPad::AddLight(const FLinearColor& Color, float Intensity, const FVector& Loc)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetRelativeLocation(Loc);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(650.f);
	L->SetCastShadows(false); // one cheap unshadowed light per pad
	L->RegisterComponent();
	Parts.Add(L);
}

void AZoogiLaunchPad::BuildDressing()
{
	for (UActorComponent* C : Parts) if (C) C->DestroyComponent();
	Parts.Reset();
	bShip = BuildShip(); // Oct 24 (Evan): the LerpzShip is the launch pad on every map
	if (bShip) return;
	using namespace ZoogiLaunch;
	const float R = DeckRadius, S = R / 50.f; // cylinder diameter 100 -> deck diameter
	// Evan Oct 22 6:57 PM CT: SharedPlatform GLB is the outside hover launch pad (not the arena floor).
	// Prefer /Game/Env/LaunchPad/SM_SharedPlatform when imported; fall back to themed BasicShapes.
	static const TCHAR* SharedPath = TEXT("/Game/Env/LaunchPad/SM_SharedPlatform.SM_SharedPlatform");
	if (UStaticMesh* SharedMesh = LoadObject<UStaticMesh>(nullptr, SharedPath))
	{
		const float MeshR = FMath::Max(SharedMesh->GetBounds().SphereRadius, 1.f);
		const float Uni = R / MeshR;
		UStaticMeshComponent* Deck = NewObject<UStaticMeshComponent>(this);
		Deck->SetStaticMesh(SharedMesh);
		Deck->SetupAttachment(Root);
		Deck->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0, 0, -15), FVector(Uni)));
		Deck->SetCastShadow(true);
		Deck->ComponentTags.Add(TEXT("NoCamBlock"));
		Deck->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Deck->SetCollisionResponseToAllChannels(ECR_Block);
		Deck->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Deck->RegisterComponent();
		Parts.Add(Deck);
		// Soft hover ring + light so the pad reads as outside/unobstructed aim platform on every map.
		auto Glow = [this](const TCHAR* Hex, float Str) { return ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(Hex), FMath::Min(Str, MaxGlow)); };
		Part(TEXT("Cylinder"), Glow(TEXT("#7FE8FF"), 0.55f), FVector(0, 0, 1), FRotator::ZeroRotator, FVector(S * 0.85f, S * 0.85f, 0.02f));
		Part(TEXT("Cylinder"), Glow(TEXT("#A070FF"), 0.35f), FVector(0, 0, -40), FRotator::ZeroRotator, FVector(S * 0.55f, S * 0.55f, 0.03f));
		AddLight(ZoogiUtil::Hex(TEXT("#7FE8FF")), 10.f, FVector(0, 0, 120));
		UE_LOG(LogZoogi, Log, TEXT("LaunchPad using SharedPlatform mesh (scale %.3f for DeckRadius %.0f)"), Uni, R);
		return;
	}
	auto Surf = [this](const TCHAR* Hex, float Rough = 0.7f, float Metal = 0.f) { return ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(Hex), Rough, FLinearColor::Black, 0.f, Metal); };
	auto Glow = [this](const TCHAR* Hex, float Str) { return ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(Hex), FMath::Min(Str, MaxGlow)); };
	const FVector Disk(S, S, 0.3f);
	const float Down = PadHeight + 1500.f; // supports run well below the court skirt
	switch (Theme)
	{
	case EZoogiArena::Meadow: // tree-branch perch
	{
		UMaterialInterface* Bark = Surf(TEXT("#6B4A2E"), 0.9f);
		UMaterialInterface* Leaf = Surf(TEXT("#5E9E3A"), 0.8f);
		Part(TEXT("Cylinder"), Surf(TEXT("#9A7448"), 0.85f), FVector(0, 0, -15), FRotator::ZeroRotator, Disk, true);
		Part(TEXT("Cylinder"), Bark, FVector(-160, 0, -70), FRotator(90, 0, 0), FVector(0.9f, 0.9f, 5.2f));       // branch along X
		Part(TEXT("Cylinder"), Bark, FVector(-420, 0, -Down * 0.5f), FRotator::ZeroRotator, FVector(1.6f, 1.6f, Down / 100.f)); // trunk
		for (int32 i = 0; i < 5; ++i)
			Part(TEXT("Sphere"), Leaf, FVector(-420 + 90 * FMath::Cos(i * 1.3f), 120 * FMath::Sin(i * 1.3f), 180 + 40 * (i % 2)), FRotator::ZeroRotator, FVector(2.6f, 2.4f, 1.8f));
		Part(TEXT("Cylinder"), Glow(TEXT("#FFD98A"), 0.5f), FVector(0, 0, 1), FRotator::ZeroRotator, FVector(S * 0.8f, S * 0.8f, 0.02f));
		AddLight(ZoogiUtil::Hex(TEXT("#FFD98A")), 10.f, FVector(0, 0, 120));
		break;
	}
	case EZoogiArena::Frozen: // ice-spire ledge
	{
		UMaterialInterface* Ice = Surf(TEXT("#BFE9F5"), 0.15f);
		Part(TEXT("Cylinder"), Ice, FVector(0, 0, -15), FRotator::ZeroRotator, Disk, true);
		Part(TEXT("Cylinder"), Surf(TEXT("#7FB8D0"), 0.3f), FVector(-60, 0, -Down * 0.5f), FRotator::ZeroRotator, FVector(2.2f, 2.2f, Down / 100.f));
		Part(TEXT("Cone"), Ice, FVector(-150, 0, 260), FRotator::ZeroRotator, FVector(1.4f, 1.4f, 5.6f)); // spire behind the deck
		for (int32 i = 0; i < 6; ++i)
			Part(TEXT("Cone"), Ice, FVector(R * 0.85f * FMath::Cos(i * PI / 3), R * 0.85f * FMath::Sin(i * PI / 3), -90), FRotator(180, 0, 0), FVector(0.35f, 0.35f, 1.2f)); // icicles
		Part(TEXT("Cylinder"), Glow(TEXT("#7FE8FF"), 0.6f), FVector(0, 0, 1), FRotator::ZeroRotator, FVector(S * 0.8f, S * 0.8f, 0.02f));
		AddLight(ZoogiUtil::Hex(TEXT("#7FE8FF")), 10.f, FVector(0, 0, 120));
		break;
	}
	case EZoogiArena::Volcanic: // crater rim + steam vent
	{
		UMaterialInterface* Rock = Surf(TEXT("#2E2522"), 0.95f);
		Part(TEXT("Cylinder"), Rock, FVector(0, 0, -15), FRotator::ZeroRotator, Disk, true);
		Part(TEXT("Cone"), Rock, FVector(-40, 0, -Down * 0.5f - 15), FRotator::ZeroRotator, FVector(S * 2.4f, S * 2.4f, Down / 100.f));
		Part(TEXT("Cylinder"), Glow(TEXT("#FF7A2E"), 0.8f), FVector(0, 0, -34), FRotator::ZeroRotator, FVector(S * 1.08f, S * 1.08f, 0.06f)); // lava lip
		UMaterialInterface* Steam = Surf(TEXT("#C9C2BC"), 1.f);
		Part(TEXT("Cylinder"), Rock, FVector(-130, 70, 30), FRotator::ZeroRotator, FVector(0.7f, 0.7f, 0.9f)); // vent
		for (int32 i = 0; i < 3; ++i) Part(TEXT("Sphere"), Steam, FVector(-135 - 10 * i, 75 + 15 * i, 110 + 75 * i), FRotator::ZeroRotator, FVector(0.8f + 0.4f * i));
		AddLight(ZoogiUtil::Hex(TEXT("#FF7A2E")), 12.f, FVector(0, 0, 60));
		break;
	}
	case EZoogiArena::Cosmic: // floating satellite dish
	{
		UMaterialInterface* Metal = Surf(TEXT("#B8BCC8"), 0.35f, 0.8f);
		Part(TEXT("Cylinder"), Surf(TEXT("#5A5F70"), 0.4f, 0.6f), FVector(0, 0, -15), FRotator::ZeroRotator, Disk, true);
		Part(TEXT("Sphere"), Metal, FVector(0, 0, -70), FRotator::ZeroRotator, FVector(S * 1.5f, S * 1.5f, 0.9f)); // dish bowl under the deck
		Part(TEXT("Cylinder"), Metal, FVector(-120, 0, 110), FRotator(-25, 0, 0), FVector(0.12f, 0.12f, 2.4f));    // feed arm
		Part(TEXT("Sphere"), Glow(TEXT("#A070FF"), 0.8f), FVector(-170, 0, 215), FRotator::ZeroRotator, FVector(0.4f));
		Part(TEXT("Cylinder"), Glow(TEXT("#A070FF"), 0.5f), FVector(0, 0, -130), FRotator::ZeroRotator, FVector(S * 0.6f, S * 0.6f, 0.04f)); // hover ring
		AddLight(ZoogiUtil::Hex(TEXT("#A070FF")), 8.f, FVector(0, 0, 120));
		break;
	}
	case EZoogiArena::Arabian: // flying carpet
	{
		Part(TEXT("Cube"), Surf(TEXT("#8E1F3A"), 0.9f), FVector(0, 0, -5), FRotator::ZeroRotator, FVector(3.8f, 2.4f, 0.1f), true);
		UMaterialInterface* Gold = Surf(TEXT("#D9A441"), 0.4f, 0.7f);
		Part(TEXT("Cube"), Gold, FVector(185, 0, -3), FRotator::ZeroRotator, FVector(0.12f, 2.4f, 0.12f));
		Part(TEXT("Cube"), Gold, FVector(-185, 0, -3), FRotator::ZeroRotator, FVector(0.12f, 2.4f, 0.12f));
		Part(TEXT("Cube"), Gold, FVector(0, 115, -3), FRotator::ZeroRotator, FVector(3.8f, 0.12f, 0.12f));
		Part(TEXT("Cube"), Gold, FVector(0, -115, -3), FRotator::ZeroRotator, FVector(3.8f, 0.12f, 0.12f));
		for (int32 i = 0; i < 4; ++i) Part(TEXT("Sphere"), Gold, FVector((i & 1) ? 195 : -195, (i & 2) ? 125 : -125, -20), FRotator::ZeroRotator, FVector(0.18f));
		Part(TEXT("Cube"), Glow(TEXT("#3FD8F2"), 0.6f), FVector(0, 0, 1), FRotator::ZeroRotator, FVector(2.2f, 1.2f, 0.01f)); // woven medallion
		AddLight(ZoogiUtil::Hex(TEXT("#3FD8F2")), 9.f, FVector(0, 0, -120)); // under-glow sells the float
		break;
	}
	case EZoogiArena::Neon: // neon tower + glowing hoop
	{
		Part(TEXT("Cube"), Surf(TEXT("#1A1830"), 0.4f, 0.5f), FVector(0, 0, -15), FRotator::ZeroRotator, FVector(3.4f, 3.4f, 0.3f), true);
		Part(TEXT("Cube"), Surf(TEXT("#12101F"), 0.5f), FVector(-40, 0, -Down * 0.5f - 30), FRotator::ZeroRotator, FVector(2.6f, 2.6f, Down / 100.f));
		UMaterialInterface* Mag = Glow(TEXT("#FF3FD0"), 0.8f);
		for (int32 c = 0; c < 4; ++c) Part(TEXT("Cube"), Mag, FVector(-40 + ((c & 1) ? 131 : -131), (c & 2) ? 131 : -131, -Down * 0.5f), FRotator::ZeroRotator, FVector(0.06f, 0.06f, Down / 100.f));
		UMaterialInterface* Cy = Glow(TEXT("#3FE6FF"), 0.8f);
		const int32 N = 16; const float HR = 140.f;
		for (int32 i = 0; i < N; ++i) // hoop at the deck's front edge, upright, the launch passes through it
		{
			const float A = 2.f * PI * i / N;
			Part(TEXT("Cube"), Cy, FVector(R, HR * FMath::Cos(A), 160 + HR * FMath::Sin(A)), FRotator(0, 0, -FMath::RadiansToDegrees(A)), FVector(0.1f, 0.12f, 0.58f));
		}
		AddLight(ZoogiUtil::Hex(TEXT("#FF3FD0")), 8.f, FVector(0, 0, 140));
		break;
	}
	case EZoogiArena::Tomb: // pyramid tip + golden stair ramp
	{
		UMaterialInterface* Sand = Surf(TEXT("#C9A46A"), 0.9f);
		for (int32 t = 0; t < 6; ++t)
		{
			const float W = 3.6f + 1.6f * t;
			Part(TEXT("Cube"), Sand, FVector(0, 0, -15 - 80 * t), FRotator::ZeroRotator, FVector(W, W, 0.8f), t == 0);
		}
		Part(TEXT("Cube"), Sand, FVector(0, 0, -480 - Down * 0.5f), FRotator::ZeroRotator, FVector(13.2f, 13.2f, Down / 100.f));
		UMaterialInterface* Gold = Surf(TEXT("#E3B04B"), 0.3f, 0.9f);
		for (int32 s = 0; s < 6; ++s) // stair down the back face (away from the court)
			Part(TEXT("Cube"), Gold, FVector(-190 - 80 * s, 0, -10 - 80 * s), FRotator::ZeroRotator, FVector(0.8f, 1.2f, 0.25f));
		Part(TEXT("Cube"), Glow(TEXT("#FFB347"), 0.6f), FVector(0, 0, 26), FRotator::ZeroRotator, FVector(1.2f, 1.2f, 0.02f));
		AddLight(ZoogiUtil::Hex(TEXT("#FFB347")), 10.f, FVector(0, 0, 140));
		break;
	}
	default: // Training: stone pillar ledge
	{
		UMaterialInterface* Stone = Surf(TEXT("#8C8F94"), 0.85f);
		Part(TEXT("Cylinder"), Stone, FVector(0, 0, -15), FRotator::ZeroRotator, Disk, true);
		Part(TEXT("Cylinder"), Surf(TEXT("#6E7178"), 0.9f), FVector(0, 0, -Down * 0.5f - 30), FRotator::ZeroRotator, FVector(S * 0.75f, S * 0.75f, Down / 100.f));
		Part(TEXT("Cylinder"), Glow(TEXT("#3FD9C0"), 0.6f), FVector(0, 0, -36), FRotator::ZeroRotator, FVector(S * 1.04f, S * 1.04f, 0.05f));
		AddLight(ZoogiUtil::Hex(TEXT("#3FD9C0")), 10.f, FVector(0, 0, 120));
		break;
	}
	}
}

void AZoogiLaunchPad::Relocate(const FVector& Loc, float Yaw, float InShipScale, const FVector& InLandingTarget)
{
	ShipScale = FMath::Clamp(InShipScale, 0.3f, 3.f);
	SetActorLocationAndRotation(Loc, FRotator(0.f, Yaw, 0.f));
	Setup(Theme, InLandingTarget); // launch speed/direction + dressing follow the ship
}

bool AZoogiLaunchPad::BuildShip()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShipPad"))) return false;
	UStaticMesh* Ship = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Legacy012/SM_LegacyShip.SM_LegacyShip"));
	if (!Ship) { UE_LOG(LogZoogi, Warning, TEXT("LaunchPad: SM_LegacyShip missing, using the platform")); return false; }
	using namespace ZoogiLaunch;
	// Invisible standable deck: same footprint/height as the old platform, so the seat, launch math and physics are unchanged.
	UStaticMeshComponent* Deck = Part(TEXT("Cylinder"), nullptr, FVector(0, 0, -15), FRotator::ZeroRotator, FVector(DeckRadius / 50.f, DeckRadius / 50.f, 0.3f), true);
	Deck->SetVisibility(false);
	Deck->SetCastShadow(false);
	// Ship visual: ~2.8x the deck diameter long, hull top just above the deck so the Zoogi sits IN the ship. Visual only.
	const FBox B = Ship->GetBoundingBox();
	const FVector Sz = B.GetSize();
	const float Sc = DeckRadius * 2.8f / FMath::Max(FMath::Max(Sz.X, Sz.Y), 1.f) * ShipScale;
	static const float ShipYawOffset = 90.f; // mesh nose -> actor +X (the court)
	const FRotator Rot(0.f, ShipYawOffset, 0.f);
	const FVector Off = Rot.RotateVector(FVector(-B.GetCenter().X * Sc, -B.GetCenter().Y * Sc, 0.f));
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
	M->SetStaticMesh(Ship);
	M->SetupAttachment(Root);
	M->SetRelativeTransform(FTransform(Rot, FVector(Off.X, Off.Y, -B.Max.Z * Sc + 35.f), FVector(Sc)));
	M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M->SetCastShadow(true);
	M->ComponentTags.Add(TEXT("NoCamBlock"));
	M->ComponentTags.Add(TEXT("ZoogiShipPad"));
	M->RegisterComponent();
	Parts.Add(M);
	AddLight(ZoogiUtil::Hex(TEXT("#FFD27F")), 14.f, FVector(0, 0, 160));
	UE_LOG(LogZoogi, Log, TEXT("LaunchPad: ship pad (scale %.3f, dev scale %.2f)"), Sc, ShipScale);
	return true;
}