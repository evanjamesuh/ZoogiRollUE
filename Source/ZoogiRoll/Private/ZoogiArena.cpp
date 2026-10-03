#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AZoogiArena::AZoogiArena()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(9.f);
	Sun->SetCastShadows(true);
	Sun->SetDynamicShadowCascades(4);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SetIntensity(1.f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->SetupAttachment(Root);
	Post->bUnbound = true;

	SkySphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkySphere"));
	SkySphere->SetupAttachment(Root);
	SkySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkySphere->SetCastShadow(false);
	SkySphere->SetVisibility(false);
}

float AZoogiArena::GetRollingDecel() const
{
	return Theme == EZoogiArena::Frozen ? ZoogiConst::IceRollingDecel : ZoogiConst::RollingDecel;
}

static const float GSpawnAngles[4] = { 180.f, 0.f, 90.f, 270.f };

FVector AZoogiArena::GetSpawnPoint(int32 PlayerIndex) const
{
	const float A = FMath::DegreesToRadians(GSpawnAngles[PlayerIndex % 4]);
	return FVector(FMath::Cos(A) * ZoogiConst::SpawnRadius, FMath::Sin(A) * ZoogiConst::SpawnRadius, ZoogiConst::ZoogiRadius + 2.f);
}

float AZoogiArena::GetSpawnYaw(int32 PlayerIndex) const
{
	return GSpawnAngles[PlayerIndex % 4] + 180.f; // face the centre
}

void AZoogiArena::Clear()
{
	for (UActorComponent* C : Built)
	{
		if (C) C->DestroyComponent();
	}
	Built.Reset();
}

UStaticMeshComponent* AZoogiArena::AddMesh(UStaticMesh* Mesh, UMaterialInterface* Mat, bool bCollide, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	if (bCollide)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->SetCastShadow(bShadow);
	C->RegisterComponent();
	if (Mat)
	{
		for (int32 i = 0; i < FMath::Max(1, C->GetNumMaterials()); ++i) C->SetMaterial(i, Mat);
	}
	Built.Add(C);
	return C;
}

UStaticMeshComponent* AZoogiArena::AddCylinder(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat, bool bCollide)
{
	UStaticMesh* Mesh = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMeshComponent* C = AddMesh(Mesh, Mat, bCollide);
	const FBoxSphereBounds B = Mesh->GetBounds();
	const FVector S(Radius / B.BoxExtent.X, Radius / B.BoxExtent.Y, Height / (2.f * B.BoxExtent.Z));
	C->SetRelativeScale3D(S);
	C->SetRelativeLocation(FVector(XY.X, XY.Y, TopZ - (B.Origin.Z + B.BoxExtent.Z) * S.Z));
	if (bCollide && FloorPM) C->SetPhysMaterialOverride(FloorPM);
	return C;
}

UStaticMeshComponent* AZoogiArena::AddUnderCone(const FVector2D& XY, float Radius, float Height, float TopZ, UMaterialInterface* Mat)
{
	UStaticMesh* Mesh = ZoogiUtil::BasicShape(TEXT("Cone"));
	UStaticMeshComponent* C = AddMesh(Mesh, Mat, false);
	const FBoxSphereBounds B = Mesh->GetBounds();
	const FVector S(Radius / B.BoxExtent.X, Radius / B.BoxExtent.Y, Height / (2.f * B.BoxExtent.Z));
	C->SetRelativeScale3D(S);
	C->SetRelativeRotation(FRotator(0.f, 0.f, 180.f));
	// Flipped: local +Z points down, so the base (local min Z) becomes the top.
	C->SetRelativeLocation(FVector(XY.X, XY.Y, TopZ - (B.BoxExtent.Z - B.Origin.Z) * S.Z));
	return C;
}

UStaticMeshComponent* AZoogiArena::AddTree(UStaticMesh* Tree, const FVector& Base, float Height, float Yaw)
{
	if (!Tree) return nullptr;
	UStaticMeshComponent* C = AddMesh(Tree, nullptr, false);
	const FBoxSphereBounds B = Tree->GetBounds();
	const float S = Height / FMath::Max(2.f * B.BoxExtent.Z, 1.f);
	C->SetRelativeScale3D(FVector(S));
	C->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	const FVector Off = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(B.Origin.X, B.Origin.Y, 0.f) * S);
	C->SetRelativeLocation(FVector(Base.X - Off.X, Base.Y - Off.Y, Base.Z - (B.Origin.Z - B.BoxExtent.Z) * S));
	return C;
}

void AZoogiArena::ScatterInstances(UStaticMesh* Mesh, UMaterialInterface* Mat, int32 Count, float RMin, float RMax, FVector2D ScaleRange, float ZOffset, FRandomStream& Rng, bool bFlatten)
{
	if (!Mesh) return;
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetStaticMesh(Mesh);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetupAttachment(Root);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCastShadow(false);
	ISM->RegisterComponent();
	if (Mat) ISM->SetMaterial(0, Mat);
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float R = FMath::Sqrt(Rng.FRandRange(RMin * RMin, RMax * RMax));
		const float Sc = Rng.FRandRange(ScaleRange.X, ScaleRange.Y);
		FTransform T;
		T.SetLocation(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, ZOffset));
		T.SetRotation(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Quaternion());
		T.SetScale3D(bFlatten ? FVector(Sc, Sc, Sc * 0.5f) : FVector(Sc));
		ISM->AddInstance(T);
	}
	Built.Add(ISM);
}

void AZoogiArena::Build(EZoogiArena InTheme)
{
	Clear();
	Theme = InTheme;
	SetupSky(InTheme);
	switch (InTheme)
	{
	case EZoogiArena::Frozen: BuildFrozen(); break;
	case EZoogiArena::Volcanic: BuildVolcanic(); break;
	case EZoogiArena::Cosmic: BuildCosmic(); break;
	default: BuildMeadow(); break;
	}
	BuildPads();
	UE_LOG(LogZoogi, Log, TEXT("Built arena %s with %d components"), *ZoogiUtil::ArenaName(InTheme), Built.Num());
}

void AZoogiArena::SetupSky(EZoogiArena InTheme)
{
	// Planet ground sits far below the island so the cloud layer floats under it.
	Atmosphere->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtComponentTransform;
	Atmosphere->SetRelativeLocation(FVector(0, 0, -60000.f));
	Atmosphere->MarkRenderStateDirty();
	Clouds->SetRelativeLocation(FVector::ZeroVector);
	// Cloud deck from 0.05 to 0.35 km above the planet ground: well under the island.
	Clouds->SetLayerBottomAltitude(0.05f);
	Clouds->SetLayerHeight(0.3f);
	if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Clouds->SetMaterial(CloudMat);
	}

	FPostProcessSettings& P = Post->Settings;
	P.bOverride_AutoExposureMethod = true;
	P.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	P.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	P.AutoExposureApplyPhysicalCameraExposure = false;
	P.bOverride_AutoExposureBias = true;
	P.bOverride_BloomIntensity = true;
	P.BloomIntensity = 0.9f;
	P.bOverride_VignetteIntensity = true;
	P.VignetteIntensity = 0.35f;
	P.bOverride_ColorSaturation = true;
	P.bOverride_ColorContrast = true;
	P.ColorContrast = FVector4(1.05f, 1.05f, 1.05f, 1.f);
	P.bOverride_SceneFringeIntensity = true;
	P.SceneFringeIntensity = 0.f;

	Fog->SetVolumetricFog(false);
	Clouds->SetVisibility(true);
	Atmosphere->SetVisibility(true);
	Atmosphere->SetRayleighScatteringScale(0.0331f);
	Atmosphere->SetMieScatteringScale(0.003996f);

	switch (InTheme)
	{
	case EZoogiArena::Frozen:
		Sun->SetRelativeRotation(FRotator(-28.f, 60.f, 0.f));
		Sun->SetIntensity(7.f);
		Sun->SetLightColor(FLinearColor(0.85f, 0.92f, 1.f));
		Fog->SetFogDensity(0.012f);
		Fog->SetFogInscatteringColor(FLinearColor(0.35f, 0.5f, 0.75f));
		P.AutoExposureBias = -0.6f;
		P.ColorSaturation = FVector4(0.95f, 1.f, 1.08f, 1.f);
		break;
	case EZoogiArena::Volcanic:
		Sun->SetRelativeRotation(FRotator(-12.f, 20.f, 0.f));
		Sun->SetIntensity(5.f);
		Sun->SetLightColor(FLinearColor(1.f, 0.55f, 0.3f));
		Fog->SetFogDensity(0.06f);
		Fog->SetFogInscatteringColor(FLinearColor(0.6f, 0.15f, 0.05f));
		Clouds->SetVisibility(false);
		P.AutoExposureBias = 1.0f;
		P.ColorSaturation = FVector4(1.1f, 1.f, 0.95f, 1.f);
		break;
	case EZoogiArena::Cosmic:
		Sun->SetRelativeRotation(FRotator(-55.f, 140.f, 0.f));
		Sun->SetIntensity(6.f);
		Sun->SetLightColor(FLinearColor(0.8f, 0.8f, 1.f));
		Fog->SetFogDensity(0.0f);
		Clouds->SetVisibility(false);
		// Keep the atmosphere (the real-time sky light capture needs one) but make it black space.
		Atmosphere->SetRayleighScatteringScale(0.f);
		Atmosphere->SetMieScatteringScale(0.f);
		P.AutoExposureBias = 1.2f;
		P.ColorSaturation = FVector4(1.f, 1.f, 1.1f, 1.f);
		break;
	default:
		Sun->SetRelativeRotation(FRotator(-42.f, 35.f, 0.f));
		Sun->SetIntensity(9.f);
		Sun->SetLightColor(FLinearColor(1.f, 0.96f, 0.88f));
		Fog->SetFogDensity(0.004f);
		Fog->SetFogInscatteringColor(FLinearColor(0.25f, 0.45f, 0.8f));
		P.AutoExposureBias = -0.4f;
		P.ColorSaturation = FVector4(1.02f, 1.02f, 1.02f, 1.f);
		break;
	}
	Fog->SetFogHeightFalloff(0.002f);
	Fog->SetRelativeLocation(FVector(0, 0, -3000.f));
	SkyLight->RecaptureSky();
}

void AZoogiArena::BuildIsland(const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Band, const FLinearColor& RockA, const FLinearColor& RockB, float FloorFriction)
{
	FloorPM = ZoogiPhysics::MakeMaterial(this, FloorFriction, 0.1f, true);
	const float R = ZoogiConst::KnockoffRadius;
	UMaterialInterface* Top = ZoogiUtil::GroundMID(this, TopA, TopB, 0.0025f, 0.92f);
	UMaterialInterface* BandMat = ZoogiUtil::GroundMID(this, Band, Band * 0.75f, 0.01f, 0.95f);
	UMaterialInterface* Rock = ZoogiUtil::GroundMID(this, RockA, RockB, 0.004f, 0.95f);

	// Playfield: collision floor, top at Z = 0.
	AddCylinder(FVector2D::ZeroVector, R, 60.f, 0.f, Top, true);
	// Soil band just under the lip so the edge reads.
	AddCylinder(FVector2D::ZeroVector, R - 10.f, 160.f, -45.f, BandMat, false);
	// Craggy underside.
	AddUnderCone(FVector2D::ZeroVector, R - 40.f, 1500.f, -190.f, Rock);
	FRandomStream Rng(1234);
	for (int32 i = 0; i < 9; ++i)
	{
		const float A = i * (2.f * PI / 9.f) + Rng.FRandRange(-0.2f, 0.2f);
		const float D = Rng.FRandRange(550.f, 950.f);
		AddUnderCone(FVector2D(FMath::Cos(A) * D, FMath::Sin(A) * D), Rng.FRandRange(380.f, 620.f), Rng.FRandRange(900.f, 1700.f), -150.f, Rock);
	}
}

void AZoogiArena::BuildRim(const TArray<FVector4>& Marks, const FLinearColor& A, const FLinearColor& B, float Restitution)
{
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, Restitution, false);
	UMaterialInterface* Mat = ZoogiUtil::GroundMID(this, A, B, 0.02f, 0.85f);
	UStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));
	for (const FVector4& M : Marks)
	{
		// X = angle (deg), Y = distance, Z = footprint radius, W = height (web units)
		const float Ang = FMath::DegreesToRadians(M.X);
		const FVector2D P(FMath::Cos(Ang) * M.Y * 100.f, FMath::Sin(Ang) * M.Y * 100.f);
		const float Rad = M.Z * 100.f;
		const float H = M.W * 100.f;
		// Collider: an invisible cylinder with the boulder footprint.
		UStaticMeshComponent* Col = AddCylinder(P, Rad, H, H, nullptr, true);
		Col->SetVisibility(false);
		Col->SetPhysMaterialOverride(RockPM);
		// Visual: a squashed boulder sunk into the turf, plus a smaller one beside it.
		UStaticMeshComponent* V = AddMesh(SphereMesh, Mat, false);
		V->SetRelativeScale3D(FVector(Rad * 2.f / 100.f, Rad * 1.8f / 100.f, H * 2.f / 100.f));
		V->SetRelativeLocation(FVector(P.X, P.Y, 0.f));
		V->SetRelativeRotation(FRotator(0.f, M.X, 0.f));
		UStaticMeshComponent* V2 = AddMesh(SphereMesh, Mat, false);
		V2->SetRelativeScale3D(FVector(Rad * 0.9f / 100.f, Rad * 0.8f / 100.f, H * 1.1f / 100.f));
		const FVector2D Side = P + FVector2D(-FMath::Sin(Ang), FMath::Cos(Ang)) * Rad * 0.85f;
		V2->SetRelativeLocation(FVector(Side.X, Side.Y, 0.f));
	}
}

void AZoogiArena::BuildPads()
{
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector S = GetSpawnPoint(i);
		UMaterialInterface* Mat = ZoogiUtil::GlowMID(this, ZoogiUtil::PlayerColor(i), 0.8f);
		AddCylinder(FVector2D(S.X, S.Y), 85.f, 3.f, 1.f, Mat, false)->SetCastShadow(false);
	}
}

void AZoogiArena::BuildIslet(const FVector& Center, float Radius, const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& RockC, UStaticMesh* Tree, int32 TreeCount, float TreeHeight, FRandomStream& Rng)
{
	UMaterialInterface* Top = ZoogiUtil::GroundMID(this, TopA, TopB, 0.003f, 0.92f);
	UMaterialInterface* Rock = ZoogiUtil::GroundMID(this, RockC, RockC * 0.7f, 0.006f, 0.95f);
	AddCylinder(FVector2D(Center), Radius, 80.f, Center.Z, Top, false);
	AddUnderCone(FVector2D(Center), Radius * 0.97f, Radius * 1.6f, Center.Z - 60.f, Rock);
	for (int32 t = 0; t < TreeCount; ++t)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float D = Rng.FRandRange(0.f, Radius * 0.65f);
		AddTree(Tree, FVector(Center.X + FMath::Cos(A) * D, Center.Y + FMath::Sin(A) * D, Center.Z), TreeHeight * Rng.FRandRange(0.75f, 1.25f), Rng.FRandRange(0.f, 360.f));
	}
}

void AZoogiArena::BuildMeadow()
{
	BuildIsland(ZoogiUtil::Hex(TEXT("#3F8A2C")), ZoogiUtil::Hex(TEXT("#55A336")), ZoogiUtil::Hex(TEXT("#6B4A2B")),
		ZoogiUtil::Hex(TEXT("#6E5B4A")), ZoogiUtil::Hex(TEXT("#4A3B30")), 0.3f);

	// roundRim.ts GRASS_RIM: angle, distance, radius, height
	TArray<FVector4> Rim = {
		FVector4(32, 13.35, 1.15, 0.85), FVector4(70, 13.5, 1.05, 0.72), FVector4(152, 13.25, 1.2, 0.9),
		FVector4(196, 13.45, 1.1, 0.78), FVector4(250, 13.3, 1.22, 0.88), FVector4(332, 13.4, 1.08, 0.7) };
	BuildRim(Rim, ZoogiUtil::Hex(TEXT("#7C7F73")), ZoogiUtil::Hex(TEXT("#4F7A3A")), 0.55f);

	FRandomStream Rng(77);
	UStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* ConeMesh = ZoogiUtil::BasicShape(TEXT("Cone"));
	// Grass tufts and flowers (visual only).
	ScatterInstances(ConeMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#7BC843")), 0.9f), 260, 300.f, 1500.f, FVector2D(0.07f, 0.12f), 0.f, Rng);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FDE047")), 0.6f, ZoogiUtil::Hex(TEXT("#FDE047")), 0.4f), 120, 700.f, 1480.f, FVector2D(0.08f, 0.12f), 4.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FFFFFF")), 0.6f), 120, 700.f, 1480.f, FVector2D(0.07f, 0.11f), 4.f, Rng, true);
	ScatterInstances(SphereMesh, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#F472B6")), 0.6f, ZoogiUtil::Hex(TEXT("#F472B6")), 0.3f), 90, 700.f, 1480.f, FVector2D(0.08f, 0.12f), 4.f, Rng, true);

	// Floating forest islets around the meadow.
	UStaticMesh* Oak = ZoogiUtil::LoadMesh(TEXT("/Game/Env/OakTree/SM_OakTree.SM_OakTree"));
	UStaticMesh* Pine = ZoogiUtil::LoadMesh(TEXT("/Game/Env/PineTree/SM_PineTree.SM_PineTree"));
	const FLinearColor GA = ZoogiUtil::Hex(TEXT("#3F8A2C")), GB = ZoogiUtil::Hex(TEXT("#55A336")), RockC = ZoogiUtil::Hex(TEXT("#5E4C3D"));
	for (int32 i = 0; i < 14; ++i)
	{
		const float A = i * (2.f * PI / 14.f) + Rng.FRandRange(-0.15f, 0.15f);
		const float D = Rng.FRandRange(2600.f, 5200.f) + (i % 3) * 900.f;
		const float Z = Rng.FRandRange(-900.f, 200.f);
		const float Rad = Rng.FRandRange(320.f, 700.f);
		BuildIslet(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Z), Rad, GA, GB, RockC, (i % 2) ? Oak : Pine, FMath::RoundToInt(Rad / 160.f), Rng.FRandRange(500.f, 900.f), Rng);
	}
}

void AZoogiArena::BuildFrozen()
{
	BuildIsland(ZoogiUtil::Hex(TEXT("#CFEFFF")), ZoogiUtil::Hex(TEXT("#9ED8F5")), ZoogiUtil::Hex(TEXT("#F4FBFF")),
		ZoogiUtil::Hex(TEXT("#16324A")), ZoogiUtil::Hex(TEXT("#2D5675")), 0.03f);
	TArray<FVector4> Rim;
	const float Angles[] = { 32, 70, 152, 196, 250, 332 };
	for (float A : Angles) Rim.Add(FVector4(A, 14.15, 0.55, 1.7));
	BuildRim(Rim, ZoogiUtil::Hex(TEXT("#FFFFFF")), ZoogiUtil::Hex(TEXT("#DDEEFF")), 0.55f);
	FRandomStream Rng(88);
	UStaticMesh* SnowPine = ZoogiUtil::LoadMesh(TEXT("/Game/Env/SnowPine/SM_SnowPine.SM_SnowPine"));
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = i * (2.f * PI / 12.f) + Rng.FRandRange(-0.15f, 0.15f);
		const float D = Rng.FRandRange(2600.f, 5600.f);
		BuildIslet(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Rng.FRandRange(-900.f, 200.f)), Rng.FRandRange(320.f, 650.f),
			ZoogiUtil::Hex(TEXT("#FFFFFF")), ZoogiUtil::Hex(TEXT("#E3F2FD")), ZoogiUtil::Hex(TEXT("#2D5675")), SnowPine, 3, 700.f, Rng);
	}
}

void AZoogiArena::BuildVolcanic()
{
	BuildIsland(ZoogiUtil::Hex(TEXT("#3A2A26")), ZoogiUtil::Hex(TEXT("#5A3A2E")), ZoogiUtil::Hex(TEXT("#FF5722")),
		ZoogiUtil::Hex(TEXT("#1E1513")), ZoogiUtil::Hex(TEXT("#3A2622")), 0.3f);
	TArray<FVector4> Rim = {
		FVector4(32, 13.35, 1.15, 1.2), FVector4(70, 13.5, 1.05, 1.0), FVector4(152, 13.25, 1.2, 1.3),
		FVector4(196, 13.45, 1.1, 1.1), FVector4(250, 13.3, 1.22, 1.25), FVector4(332, 13.4, 1.08, 1.0) };
	BuildRim(Rim, ZoogiUtil::Hex(TEXT("#2B1D1A")), ZoogiUtil::Hex(TEXT("#4A2A20")), 0.55f);
	// Glowing lava sea far below.
	UStaticMesh* Plane = ZoogiUtil::BasicShape(TEXT("Plane"));
	UStaticMeshComponent* Lava = AddMesh(Plane, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FF4500")), 4.f), false, false);
	Lava->SetRelativeScale3D(FVector(1500.f, 1500.f, 1.f));
	Lava->SetRelativeLocation(FVector(0, 0, -2400.f));
	FRandomStream Rng(99);
	UStaticMesh* Hoodoo = ZoogiUtil::LoadMesh(TEXT("/Game/Env/DesertHoodoo/SM_DesertHoodoo.SM_DesertHoodoo"));
	for (int32 i = 0; i < 12; ++i)
	{
		const float A = i * (2.f * PI / 12.f) + Rng.FRandRange(-0.15f, 0.15f);
		const float D = Rng.FRandRange(2600.f, 5600.f);
		BuildIslet(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Rng.FRandRange(-1200.f, 0.f)), Rng.FRandRange(320.f, 650.f),
			ZoogiUtil::Hex(TEXT("#2B1D1A")), ZoogiUtil::Hex(TEXT("#5A2A1E")), ZoogiUtil::Hex(TEXT("#1E1513")), Hoodoo, 2, 900.f, Rng);
	}
}

void AZoogiArena::BuildCosmic()
{
	BuildIsland(ZoogiUtil::Hex(TEXT("#2A2550")), ZoogiUtil::Hex(TEXT("#3F3A7A")), ZoogiUtil::Hex(TEXT("#7C4DFF")),
		ZoogiUtil::Hex(TEXT("#15122B")), ZoogiUtil::Hex(TEXT("#29224F")), 0.3f);
	TArray<FVector4> Rim = {
		FVector4(32, 13.35, 0.9, 1.2), FVector4(70, 13.5, 0.9, 1.2), FVector4(152, 13.25, 0.9, 1.2),
		FVector4(196, 13.45, 0.9, 1.2), FVector4(250, 13.3, 0.9, 1.2), FVector4(332, 13.4, 0.9, 1.2) };
	BuildRim(Rim, ZoogiUtil::Hex(TEXT("#6D28D9")), ZoogiUtil::Hex(TEXT("#A78BFA")), 0.8f);
	// Glowing lip ring.
	AddCylinder(FVector2D::ZeroVector, ZoogiConst::KnockoffRadius + 15.f, 20.f, -2.f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7C4DFF")), 5.f), false);
	// Star field: tiny emissive spheres on a huge shell.
	FRandomStream Rng(42);
	UStaticMesh* SphereMesh = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UInstancedStaticMeshComponent* Stars = NewObject<UInstancedStaticMeshComponent>(this);
	Stars->SetStaticMesh(SphereMesh);
	Stars->SetMobility(EComponentMobility::Movable);
	Stars->SetupAttachment(Root);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	Stars->RegisterComponent();
	Stars->SetMaterial(0, ZoogiUtil::GlowMID(this, FLinearColor(0.9f, 0.9f, 1.f), 30.f));
	for (int32 i = 0; i < 1500; ++i)
	{
		const FVector Dir = Rng.VRand();
		Stars->AddInstance(FTransform(FRotator::ZeroRotator, Dir * Rng.FRandRange(60000.f, 90000.f), FVector(Rng.FRandRange(2.5f, 7.f))));
	}
	Built.Add(Stars);
	UStaticMesh* Crystal = ZoogiUtil::LoadMesh(TEXT("/Game/Env/AlienCrystal/SM_AlienCrystal.SM_AlienCrystal"));
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * (2.f * PI / 10.f) + Rng.FRandRange(-0.15f, 0.15f);
		const float D = Rng.FRandRange(2800.f, 6000.f);
		BuildIslet(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Rng.FRandRange(-1500.f, 800.f)), Rng.FRandRange(300.f, 600.f),
			ZoogiUtil::Hex(TEXT("#2A2550")), ZoogiUtil::Hex(TEXT("#3F3A7A")), ZoogiUtil::Hex(TEXT("#15122B")), Crystal, 2, 500.f, Rng);
	}
}
