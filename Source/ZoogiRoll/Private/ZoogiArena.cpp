#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
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

// Night Circuit spawn pads (NEON_SPAWN_UNITS), web (x, z) -> UE (x, y).
static const FVector2D GNeonSpawns[4] = { FVector2D(-640.f, -110.f), FVector2D(660.f, 105.f), FVector2D(-115.f, 415.f), FVector2D(120.f, -405.f) };
static constexpr float GNeonHalfX = 1200.f, GNeonHalfY = 800.f;

bool AZoogiArena::IsOffCourt(const FVector2D& P) const
{
	if (Theme == EZoogiArena::Neon) return FMath::Abs(P.X) > GNeonHalfX || FMath::Abs(P.Y) > GNeonHalfY;
	return P.Size() > ZoogiConst::KnockoffRadius;
}

float AZoogiArena::DistanceToEdge(const FVector2D& P, const FVector2D& Dir) const
{
	if (Theme == EZoogiArena::Neon)
	{
		const float TX = Dir.X > 0.f ? (GNeonHalfX - P.X) / Dir.X : (Dir.X < 0.f ? (-GNeonHalfX - P.X) / Dir.X : 1e6f);
		const float TY = Dir.Y > 0.f ? (GNeonHalfY - P.Y) / Dir.Y : (Dir.Y < 0.f ? (-GNeonHalfY - P.Y) / Dir.Y : 1e6f);
		return FMath::Max(0.f, FMath::Min(TX, TY));
	}
	const float R = ZoogiConst::KnockoffRadius;
	const float PD = FVector2D::DotProduct(P, Dir);
	const float Disc = PD * PD - (P.SizeSquared() - R * R);
	return -PD + FMath::Sqrt(FMath::Max(0.f, Disc));
}

bool AZoogiArena::IsSpotClear(const FVector2D& P, float Radius) const
{
	for (const FBox2D& B : SolidBoxes)
	{
		const FVector2D C(FMath::Clamp(P.X, B.Min.X, B.Max.X), FMath::Clamp(P.Y, B.Min.Y, B.Max.Y));
		if (FVector2D::Distance(C, P) < Radius + 15.f) return false;
	}
	for (const FVector2D& Q : SolidPosts) if (FVector2D::Distance(Q, P) < Radius + 95.5f + 15.f) return false;
	return true;
}

FVector AZoogiArena::GetSpawnPoint(int32 PlayerIndex) const
{
	if (Theme == EZoogiArena::Neon)
	{
		const FVector2D S = GNeonSpawns[PlayerIndex % 4];
		return FVector(S.X, S.Y, ZoogiConst::ZoogiRadius + 2.f);
	}
	const float A = FMath::DegreesToRadians(GSpawnAngles[PlayerIndex % 4]);
	return FVector(FMath::Cos(A) * ZoogiConst::SpawnRadius, FMath::Sin(A) * ZoogiConst::SpawnRadius, ZoogiConst::ZoogiRadius + 2.f);
}

float AZoogiArena::GetSpawnYaw(int32 PlayerIndex) const
{
	if (Theme == EZoogiArena::Neon)
	{
		const FVector2D S = GNeonSpawns[PlayerIndex % 4];
		return FMath::RadiansToDegrees(FMath::Atan2(-S.Y, -S.X));
	}
	return GSpawnAngles[PlayerIndex % 4] + 180.f; // face the centre
}

void AZoogiArena::Clear()
{
	for (UActorComponent* C : Built)
	{
		if (C) C->DestroyComponent();
	}
	Built.Reset();
	SolidBoxes.Reset();
	SolidPosts.Reset();
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
	case EZoogiArena::Arabian: BuildArabian(); break;
	case EZoogiArena::Tomb: BuildTomb(); break;
	case EZoogiArena::Neon: BuildNeon(); break;
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
	SkyLight->SetIntensity(1.f);
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
	case EZoogiArena::Neon:
		Sun->SetRelativeRotation(FRotator(-60.f, 200.f, 0.f));
		Sun->SetIntensity(2.f);
		Sun->SetLightColor(ZoogiUtil::Hex(TEXT("#9AA8FF")));
		Fog->SetFogDensity(0.006f);
		Fog->SetFogInscatteringColor(FLinearColor(0.04f, 0.02f, 0.1f));
		Clouds->SetVisibility(false);
		Atmosphere->SetRayleighScatteringScale(0.f);
		Atmosphere->SetMieScatteringScale(0.f);
		P.AutoExposureBias = 0.6f;
		P.BloomIntensity = 1.6f;
		P.ColorSaturation = FVector4(1.f, 1.f, 1.12f, 1.f);
		break;
	case EZoogiArena::Arabian:
	case EZoogiArena::Tomb:
	{
		// Comic night: moonlight key, purple horizon haze, black starry sky (Lights.tsx saturn / tomb).
		const bool bTomb = InTheme == EZoogiArena::Tomb;
		Sun->SetRelativeRotation(bTomb ? FRotator(-40.f, 35.f, 0.f) : FRotator(-62.f, 150.f, 0.f));
		Sun->SetIntensity(bTomb ? 4.5f : 3.f);
		Sun->SetLightColor(bTomb ? ZoogiUtil::Hex(TEXT("#FFD2A8")) : ZoogiUtil::Hex(TEXT("#D4D0FF")));
		Fog->SetFogDensity(0.008f);
		Fog->SetFogInscatteringColor(bTomb ? FLinearColor(0.07f, 0.03f, 0.11f) : FLinearColor(0.06f, 0.025f, 0.13f));
		Clouds->SetVisibility(false);
		Atmosphere->SetRayleighScatteringScale(0.f);
		Atmosphere->SetMieScatteringScale(0.f);
		SkyLight->SetIntensity(bTomb ? 1.6f : 1.8f);
		P.AutoExposureBias = bTomb ? 0.2f : 0.35f;
		P.ColorSaturation = FVector4(1.05f, 1.f, 1.08f, 1.f);
		break;
	}
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

// ------------------------------------------------------------------ shared helpers for the night arenas

void AZoogiArena::AddStars(int32 Count, float SizeMin, float SizeMax, const FLinearColor& Color, int32 Seed)
{
	FRandomStream Rng(Seed);
	UInstancedStaticMeshComponent* Stars = NewObject<UInstancedStaticMeshComponent>(this);
	Stars->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
	Stars->SetMobility(EComponentMobility::Movable);
	Stars->SetupAttachment(Root);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	Stars->RegisterComponent();
	Stars->SetMaterial(0, ZoogiUtil::GlowMID(this, Color, 30.f));
	for (int32 i = 0; i < Count; ++i)
	{
		FVector Dir = Rng.VRand();
		Dir.Z = FMath::Abs(Dir.Z) * 0.9f + 0.05f; // keep them above the horizon
		Stars->AddInstance(FTransform(FRotator::ZeroRotator, Dir.GetSafeNormal() * Rng.FRandRange(60000.f, 90000.f), FVector(Rng.FRandRange(SizeMin, SizeMax))));
	}
	Built.Add(Stars);
}

UPointLightComponent* AZoogiArena::AddPointLight(const FVector& Location, const FLinearColor& Color, float Candelas, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Root);
	L->SetMobility(EComponentMobility::Movable);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Candelas);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetCastShadows(false);
	L->SetRelativeLocation(Location);
	L->RegisterComponent();
	Built.Add(L);
	return L;
}

void AZoogiArena::AddBumper(const FVector2D& XY, float Radius, float Height, UStaticMesh* Mesh, UMaterialInterface* Mat, const FVector& VisualScale, float VisualZ, float Restitution)
{
	if (!RockPM) RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, Restitution, false);
	UStaticMeshComponent* Col = AddCylinder(XY, Radius, Height, Height, nullptr, true);
	Col->SetVisibility(false);
	Col->SetPhysMaterialOverride(RockPM);
	if (Mesh)
	{
		UStaticMeshComponent* V = AddMesh(Mesh, Mat, false);
		V->SetRelativeScale3D(VisualScale);
		V->SetRelativeLocation(FVector(XY.X, XY.Y, VisualZ));
	}
}

// ------------------------------------------------------------------ Arabian Nights ("saturn")

void AZoogiArena::BuildArabian()
{
	// RoundIsland kind="saturn": warm sand floor, glowing gold lip, dark underside.
	BuildIsland(ZoogiUtil::Hex(TEXT("#E8C99A")), ZoogiUtil::Hex(TEXT("#D6B07A")), ZoogiUtil::Hex(TEXT("#C4A074")),
		ZoogiUtil::Hex(TEXT("#1A120C")), ZoogiUtil::Hex(TEXT("#2E2016")), 0.3f);
	AddCylinder(FVector2D::ZeroVector, ZoogiConst::KnockoffRadius + 12.f, 18.f, -3.f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFB03A")), 3.f), false)->SetCastShadow(false);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);

	UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
	UStaticMesh* Sph = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* Cone = ZoogiUtil::BasicShape(TEXT("Cone"));
	UMaterialInterface* Pot = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8B4A2B")), 0.7f);
	UMaterialInterface* PotBand = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#E0A83A")), 0.35f, FLinearColor::Black, 0.f, 0.8f);
	UMaterialInterface* Leaf = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#2F8F4A")), 0.8f);

	// ARABIAN_RIM: six planters (distance 13.55, radius 0.72, height 0.95) with jitter.
	const float Angles[] = { 32, 70, 152, 196, 250, 332 };
	const float Jitter[] = { 0, 0.06f, -0.04f, 0.05f, -0.06f, 0.03f };
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = FMath::DegreesToRadians(Angles[i]);
		const float D = (13.55f + Jitter[i]) * 100.f;
		const FVector2D P(FMath::Cos(A) * D, FMath::Sin(A) * D);
		const float R = 72.f, H = 95.f;
		AddBumper(P, R, H, Sph, Pot, FVector(R * 2.f / 100.f, R * 2.f / 100.f, H * 1.6f / 100.f), H * 0.45f, 0.5f);
		AddCylinder(P, R * 0.78f, 12.f, H * 0.92f, PotBand, false);
		for (int32 k = 0; k < 5; ++k)
		{
			UStaticMeshComponent* L = AddMesh(Cone, Leaf, false);
			const float Yaw = k * 72.f + i * 13.f;
			L->SetRelativeScale3D(FVector(0.28f, 0.28f, 1.1f));
			L->SetRelativeRotation(FRotator(0.f, Yaw, 28.f));
			L->SetRelativeLocation(FVector(P.X, P.Y, H + 45.f));
		}
	}

	// Floating palace islets: domes, minarets, glowing lanterns.
	FRandomStream Rng(321);
	UMaterialInterface* Wall = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#E9D3B0")), 0.75f);
	UMaterialInterface* Dome = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#D4A63A")), 0.3f, ZoogiUtil::Hex(TEXT("#FFB03A")), 0.15f, 0.9f);
	UMaterialInterface* DomeTeal = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#2BA59A")), 0.35f, FLinearColor::Black, 0.f, 0.5f);
	UMaterialInterface* Lantern = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFB347")), 8.f);
	for (int32 i = 0; i < 9; ++i)
	{
		const float A = i * (2.f * PI / 9.f) + Rng.FRandRange(-0.12f, 0.12f);
		const float D = Rng.FRandRange(3000.f, 5200.f);
		const FVector C(FMath::Cos(A) * D, FMath::Sin(A) * D, Rng.FRandRange(-700.f, 300.f));
		const float IR = Rng.FRandRange(450.f, 750.f);
		BuildIslet(C, IR, ZoogiUtil::Hex(TEXT("#D9B98A")), ZoogiUtil::Hex(TEXT("#C9A472")), ZoogiUtil::Hex(TEXT("#2E2016")), nullptr, 0, 0.f, Rng);
		// Palace block + dome.
		const float BR = IR * 0.45f, BH = Rng.FRandRange(300.f, 600.f);
		AddCylinder(FVector2D(C), BR, BH, C.Z + BH, Wall, false);
		UStaticMeshComponent* Dm = AddMesh(Sph, (i % 3 == 0) ? DomeTeal : Dome, false);
		Dm->SetRelativeScale3D(FVector(BR * 2.f / 100.f, BR * 2.f / 100.f, BR * 2.4f / 100.f));
		Dm->SetRelativeLocation(FVector(C.X, C.Y, C.Z + BH));
		UStaticMeshComponent* Tip = AddMesh(Cone, Dome, false);
		Tip->SetRelativeScale3D(FVector(0.25f, 0.25f, 1.2f));
		Tip->SetRelativeLocation(FVector(C.X, C.Y, C.Z + BH + BR * 1.15f + 50.f));
		// Two minarets.
		for (int32 m = 0; m < 2; ++m)
		{
			const float MA = A + (m ? 1.f : -1.f) * 1.2f;
			const FVector2D MP = FVector2D(C) + FVector2D(FMath::Cos(MA), FMath::Sin(MA)) * IR * 0.72f;
			const float MH = BH * Rng.FRandRange(1.5f, 2.1f);
			AddCylinder(MP, 38.f, MH, C.Z + MH, Wall, false);
			UStaticMeshComponent* MD = AddMesh(Sph, Dome, false);
			MD->SetRelativeScale3D(FVector(1.f, 1.f, 1.2f));
			MD->SetRelativeLocation(FVector(MP.X, MP.Y, C.Z + MH + 20.f));
			UStaticMeshComponent* Lt = AddMesh(Sph, Lantern, false, false);
			Lt->SetRelativeScale3D(FVector(0.35f));
			Lt->SetRelativeLocation(FVector(MP.X, MP.Y, C.Z + MH - 60.f));
		}
		AddPointLight(FVector(C.X, C.Y, C.Z + BH + 200.f), ZoogiUtil::Hex(TEXT("#FFB347")), 120.f, 2500.f);
	}
	// Hanging lanterns over the court rim (warm pools of light like the web point lights).
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = FMath::DegreesToRadians(Angles[i] + 21.f);
		const FVector P(FMath::Cos(A) * 2450.f, FMath::Sin(A) * 2450.f, 950.f); // outside the chase-camera orbit
		UStaticMeshComponent* Lt = AddMesh(Sph, Lantern, false, false);
		Lt->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.6f));
		Lt->SetRelativeLocation(P);
		AddPointLight(P, ZoogiUtil::Hex(TEXT("#FF9A3C")), 90.f, 2600.f);
	}
	// Crescent moon: bright disc with a sky-coloured disc offset in front of it.
	UStaticMeshComponent* Moon = AddMesh(Sph, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFF1C9")), 12.f), false, false);
	const FVector MoonDir = FVector(-0.55f, 0.6f, 0.58f).GetSafeNormal();
	Moon->SetRelativeScale3D(FVector(40.f));
	Moon->SetRelativeLocation(MoonDir * 60000.f);
	UStaticMeshComponent* Bite = AddMesh(Sph, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#070314")), 1.f), false, false);
	Bite->SetRelativeScale3D(FVector(38.f));
	Bite->SetRelativeLocation(MoonDir * 58500.f + FVector(900.f, 700.f, 500.f));
	AddStars(1200, 2.5f, 6.f, FLinearColor(1.f, 0.92f, 0.8f), 7);
}

// ------------------------------------------------------------------ Pharaoh's Tomb

void AZoogiArena::BuildTomb()
{
	// Sandstone disc (TOMB_STAGE r 15.5) with a darker lip ring.
	BuildIsland(ZoogiUtil::Hex(TEXT("#E0B88A")), ZoogiUtil::Hex(TEXT("#D2A776")), ZoogiUtil::Hex(TEXT("#A87848")),
		ZoogiUtil::Hex(TEXT("#5A4232")), ZoogiUtil::Hex(TEXT("#3B2A20")), 0.3f);
	const float R = ZoogiConst::KnockoffRadius;
	AddCylinder(FVector2D::ZeroVector, R - 8.f, 2.f, 0.8f, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A87848")), 0.9f), false)->SetCastShadow(false);
	AddCylinder(FVector2D::ZeroVector, R - 70.f, 2.f, 1.4f, ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#E0B88A")), ZoogiUtil::Hex(TEXT("#D2A776")), 0.0025f, 0.88f), false)->SetCastShadow(false);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.55f, false);

	UStaticMesh* Sph = ZoogiUtil::BasicShape(TEXT("Sphere"));
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	UMaterialInterface* RimMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#E7C296")), ZoogiUtil::Hex(TEXT("#CFA676")), 0.02f, 0.9f);
	UMaterialInterface* Shaft = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#E4C092")), ZoogiUtil::Hex(TEXT("#CDA478")), 0.01f, 0.88f);
	UMaterialInterface* Cap = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#EFD0A4")), 0.88f);
	UMaterialInterface* Base = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C99668")), 0.88f);
	UMaterialInterface* Rubble = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#6A5344")), ZoogiUtil::Hex(TEXT("#4E3C31")), 0.01f, 1.f);
	UMaterialInterface* Glyph = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#4A301C")), 0.8f);
	UMaterialInterface* Rock = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#5A4232")), ZoogiUtil::Hex(TEXT("#3B2A20")), 0.006f, 0.95f);

	// getTombBlocks: rounded sandstone bumpers just inside the rim (angle, distance, radius, height).
	const FVector4 Blocks[] = { FVector4(55, 13.4, 1.2, 1.05), FVector4(150, 13.3, 1.15, 0.98), FVector4(238, 13.45, 1.2, 1.08), FVector4(328, 13.2, 1.1, 0.95) };
	for (const FVector4& B : Blocks)
	{
		const float A = FMath::DegreesToRadians(B.X);
		const FVector2D P(FMath::Cos(A) * B.Y * 100.f, FMath::Sin(A) * B.Y * 100.f);
		const float Rad = B.Z * 100.f;
		AddBumper(P, Rad, B.W * 100.f, Sph, RimMat, FVector(Rad * 2.f / 100.f, Rad * 2.f / 100.f, Rad * 2.f * 0.62f / 100.f), Rad * 0.62f * 0.72f, 0.55f);
	}

	// getTombBackdrop: ruin walls, pillars, boulders, braziers (outside the knockoff line, on floating rock bases).
	auto RockBase = [&](const FVector2D& P, float Radius)
	{
		AddCylinder(P, Radius, 60.f, 0.f, Rock, false);
		AddUnderCone(P, Radius * 0.95f, Radius * 2.2f, -55.f, Rock);
	};
	const FVector2D Walls[] = { {200, 5.6f}, {252, 3.4f}, {308, 6.2f}, {18, 4.6f}, {68, 5.2f}, {122, 3.8f}, {162, 5.8f} };
	for (const FVector2D& W : Walls)
	{
		const float A = FMath::DegreesToRadians(W.X);
		const FVector2D P(FMath::Cos(A) * 2320.f, FMath::Sin(A) * 2320.f);
		const float H = W.Y * 100.f, Width = 640.f, Depth = 140.f;
		RockBase(P, 380.f);
		const FRotator Rot(0.f, W.X + 90.f, 0.f); // long side tangent to the ring
		UStaticMeshComponent* Wl = AddMesh(Cube, Shaft, false);
		Wl->SetRelativeScale3D(FVector(Width / 100.f, Depth / 100.f, H / 100.f));
		Wl->SetRelativeRotation(Rot);
		Wl->SetRelativeLocation(FVector(P.X, P.Y, H * 0.5f));
		// Carved glyph panel facing the court.
		const FVector Inward = FVector(-FMath::Cos(A), -FMath::Sin(A), 0.f);
		for (int32 g = 0; g < 4; ++g)
		{
			UStaticMeshComponent* Gl = AddMesh(Cube, Glyph, false, false);
			const float Off = (-1.5f + g) * (Width * 0.22f);
			const FVector Tangent = Rot.RotateVector(FVector(1, 0, 0));
			Gl->SetRelativeScale3D(FVector(0.5f, 0.04f, (g % 2 ? 0.9f : 1.3f)));
			Gl->SetRelativeRotation(Rot);
			Gl->SetRelativeLocation(FVector(P.X, P.Y, H * 0.55f) + Tangent * Off + Inward * (Depth * 0.5f + 2.f));
		}
		// Merlons on top.
		for (int32 m = 0; m < 5; ++m)
		{
			UStaticMeshComponent* Mn = AddMesh(Cube, Cap, false);
			const FVector Tangent = Rot.RotateVector(FVector(1, 0, 0));
			Mn->SetRelativeScale3D(FVector(Width / 5.f * 0.62f / 100.f, Depth * 0.92f / 100.f, 0.56f));
			Mn->SetRelativeRotation(Rot);
			Mn->SetRelativeLocation(FVector(P.X, P.Y, H + 28.f) + Tangent * (-Width * 0.5f + (m + 0.5f) * Width / 5.f));
		}
	}
	const FVector2D Pillars[] = { {186, 6.6f}, {236, 4.1f}, {286, 7.0f}, {346, 5.2f}, {42, 6.1f}, {96, 3.5f}, {146, 5.7f} };
	for (const FVector2D& Pl : Pillars)
	{
		const float A = FMath::DegreesToRadians(Pl.X);
		const FVector2D P(FMath::Cos(A) * 1895.f, FMath::Sin(A) * 1895.f);
		const float H = Pl.Y * 100.f, Rd = 70.f;
		RockBase(P, 170.f);
		AddCylinder(P, Rd * 1.3f, 36.f, 36.f, Base, false);
		AddCylinder(P, Rd * 0.9f, H - 50.f, H - 14.f, Shaft, false);
		AddCylinder(P, Rd * 1.1f, 38.f, H + 3.f, Cap, false);
	}
	const FVector4 Boulders[] = { FVector4(214, 20.5, 1.5, 1.65), FVector4(268, 20.1, 1.3, 1.4), FVector4(332, 20.8, 1.55, 1.75),
		FVector4(12, 20.3, 1.2, 1.25), FVector4(84, 21.1, 1.65, 1.85), FVector4(138, 20.2, 1.35, 1.45) };
	for (const FVector4& B : Boulders)
	{
		const float A = FMath::DegreesToRadians(B.X);
		const FVector2D P(FMath::Cos(A) * B.Y * 100.f, FMath::Sin(A) * B.Y * 100.f);
		const float Rd = B.Z * 100.f;
		RockBase(P, Rd * 1.3f);
		UStaticMeshComponent* Bd = AddMesh(Sph, Rubble, false);
		Bd->SetRelativeScale3D(FVector(Rd * 2.4f / 100.f, Rd * 1.9f / 100.f, Rd * 1.24f / 100.f));
		Bd->SetRelativeRotation(FRotator(12.f, -B.X + 34.f, 8.f));
		Bd->SetRelativeLocation(FVector(P.X, P.Y, Rd * 0.2f));
	}
	// Braziers at 90 / 270 deg, distance 17.5, with warm flames and lights.
	UMaterialInterface* Stem = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B88860")), 0.9f);
	UMaterialInterface* Bowl = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8A5A3A")), 0.85f);
	UMaterialInterface* Flame = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FF9A32")), 10.f);
	for (float Deg : { 90.f, 270.f, 0.f, 180.f })
	{
		const float A = FMath::DegreesToRadians(Deg + (Deg == 0.f || Deg == 180.f ? 45.f : 0.f));
		const FVector2D P(FMath::Cos(A) * 1750.f, FMath::Sin(A) * 1750.f);
		RockBase(P, 110.f);
		AddCylinder(P, 33.f, 90.f, 90.f, Stem, false);
		AddCylinder(P, 42.f, 22.f, 106.f, Bowl, false);
		UStaticMeshComponent* Fl = AddMesh(Sph, Flame, false, false);
		Fl->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.55f));
		Fl->SetRelativeLocation(FVector(P.X, P.Y, 122.f));
		AddPointLight(FVector(P.X, P.Y, 160.f), ZoogiUtil::Hex(TEXT("#FF9A3C")), 60.f, 1400.f);
	}
	// Pyramids on the horizon.
	UStaticMesh* Cone = ZoogiUtil::BasicShape(TEXT("Cone"));
	UMaterialInterface* PyrMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#C99A66")), ZoogiUtil::Hex(TEXT("#A87A4E")), 0.0008f, 0.95f);
	const FVector Pyr[] = { FVector(-9000, 6000, 3200), FVector(-11000, 1500, 2400), FVector(-7600, 9500, 1800) };
	for (const FVector& Pp : Pyr)
	{
		UStaticMeshComponent* Py = AddMesh(Cone, PyrMat, false, false);
		Py->SetRelativeScale3D(FVector(Pp.Z / 50.f, Pp.Z / 50.f, Pp.Z / 100.f));
		Py->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
		Py->SetRelativeLocation(FVector(Pp.X, Pp.Y, -1500.f + Pp.Z * 0.5f));
	}
	UStaticMeshComponent* Moon = AddMesh(Sph, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFE8C0")), 10.f), false, false);
	Moon->SetRelativeScale3D(FVector(45.f));
	Moon->SetRelativeLocation(FVector(0.45f, -0.7f, 0.55f).GetSafeNormal() * 60000.f);
	AddStars(1000, 2.5f, 6.f, FLinearColor(1.f, 0.9f, 0.85f), 11);
}

// ------------------------------------------------------------------ Night Circuit (neonCourt.ts)

UStaticMeshComponent* AZoogiArena::AddBox(const FVector2D& Min, const FVector2D& Max, float Bottom, float Top, UMaterialInterface* Mat, bool bCollide)
{
	UStaticMeshComponent* C = AddMesh(ZoogiUtil::BasicShape(TEXT("Cube")), Mat, bCollide);
	const FVector Size(Max.X - Min.X, Max.Y - Min.Y, Top - Bottom);
	C->SetRelativeScale3D(Size / 100.f); // engine cube is 100 cm, centred
	C->SetRelativeLocation(FVector((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, (Bottom + Top) * 0.5f));
	return C;
}

void AZoogiArena::BuildNeon()
{
	// 24 x 16 unit court, rails 0.7 deep that stop 3.15 short of each corner (open knockout mouths).
	const float HX = GNeonHalfX, HY = GNeonHalfY, Depth = 70.f, Mouth = 315.f;
	FloorPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.1f, true);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.8f, false); // rails/posts: lively bounce

	UMaterialInterface* FloorMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#14161F")), ZoogiUtil::Hex(TEXT("#1B1E2B")), 0.004f, 0.35f);
	AddBox(FVector2D(-HX, -HY), FVector2D(HX, HY), -60.f, 0.f, FloorMat, true)->SetPhysMaterialOverride(FloorPM);
	// Underside slab + glow lip (the knockout line).
	AddBox(FVector2D(-HX + 20.f, -HY + 20.f), FVector2D(HX - 20.f, HY - 20.f), -400.f, -60.f, ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#0B0C14")), 0.6f), false);
	UMaterialInterface* LipPink = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FF3EC8")), 6.f);
	UMaterialInterface* LipCyan = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#22E7FF")), 6.f);
	AddBox(FVector2D(-HX - 8.f, HY), FVector2D(HX + 8.f, HY + 8.f), -20.f, 0.f, LipPink, false);
	AddBox(FVector2D(-HX - 8.f, -HY - 8.f), FVector2D(HX + 8.f, -HY), -20.f, 0.f, LipPink, false);
	AddBox(FVector2D(HX, -HY), FVector2D(HX + 8.f, HY), -20.f, 0.f, LipCyan, false);
	AddBox(FVector2D(-HX - 8.f, -HY), FVector2D(-HX, HY), -20.f, 0.f, LipCyan, false);

	// Floor grid lines (visual).
	UMaterialInterface* Grid = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#3B2D8F")), 1.2f);
	for (float X = -HX + 200.f; X < HX; X += 200.f) AddBox(FVector2D(X - 1.5f, -HY), FVector2D(X + 1.5f, HY), 0.f, 0.6f, Grid, false)->SetCastShadow(false);
	for (float Y = -HY + 200.f; Y < HY; Y += 200.f) AddBox(FVector2D(-HX, Y - 1.5f), FVector2D(HX, Y + 1.5f), 0.f, 0.6f, Grid, false)->SetCastShadow(false);
	// Centre circle.
	AddCylinder(FVector2D::ZeroVector, 260.f, 0.8f, 0.8f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#22E7FF")), 2.f), false)->SetCastShadow(false);
	AddCylinder(FVector2D::ZeroVector, 250.f, 1.0f, 1.0f, FloorMat, false)->SetCastShadow(false);

	// Rails: dark body with a bright neon top strip.
	struct FRail { FVector2D Min, Max; const TCHAR* Color; };
	const float GX = HX - Mouth, GY = HY - Mouth;
	const FRail Rails[] = {
		{ FVector2D(-GX, HY - Depth), FVector2D(GX, HY), TEXT("#B026FF") },
		{ FVector2D(-GX, -HY), FVector2D(GX, -HY + Depth), TEXT("#B026FF") },
		{ FVector2D(HX - Depth, -GY), FVector2D(HX, GY), TEXT("#FF2BD6") },
		{ FVector2D(-HX, -GY), FVector2D(-HX + Depth, GY), TEXT("#22E7FF") } };
	UMaterialInterface* RailBody = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#1A1C2A")), 0.3f, FLinearColor::Black, 0.f, 0.6f);
	for (const FRail& R : Rails)
	{
		UStaticMeshComponent* Col = AddBox(R.Min, R.Max, 0.f, 70.f, RailBody, true);
		Col->SetPhysMaterialOverride(RockPM);
		AddBox(R.Min + FVector2D(8.f, 8.f), R.Max - FVector2D(8.f, 8.f), 70.f, 78.f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(R.Color), 9.f), false)->SetCastShadow(false);
		const FVector C((R.Min.X + R.Max.X) * 0.5f, (R.Min.Y + R.Max.Y) * 0.5f, 120.f);
		AddPointLight(C, ZoogiUtil::Hex(R.Color), 60.f, 900.f);
	}
	// Pads and the broken centre channel (axis-aligned solids).
	struct FPad { float MinX, MaxX, MinZ, MaxZ; float H; };
	const FPad Pads[] = {
		{ -11.f, -9.3f, -1.55f, 1.55f, 45.f }, { 7.7f, 9.2f, -1.55f, 1.55f, 45.f }, { -2.4f, 0.4f, 5.55f, 6.7f, 45.f },
		{ 1.6f, 3.8f, -6.7f, -5.55f, 45.f }, { -2.35f, 2.35f, 0.72f, 1.08f, 35.f }, { -2.35f, 2.35f, -1.08f, -0.72f, 35.f } };
	UMaterialInterface* PadMat = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#222540")), 0.4f, ZoogiUtil::Hex(TEXT("#5B3BFF")), 0.15f, 0.4f);
	UMaterialInterface* PadEdge = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#22E7FF")), 4.f);
	for (const FPad& Pd : Pads)
	{
		const FVector2D Mn(Pd.MinX * 100.f, Pd.MinZ * 100.f), Mx(Pd.MaxX * 100.f, Pd.MaxZ * 100.f);
		AddBox(Mn, Mx, 0.f, Pd.H, PadMat, true)->SetPhysMaterialOverride(RockPM);
		AddBox(Mn + FVector2D(5.f, 5.f), Mx - FVector2D(5.f, 5.f), Pd.H, Pd.H + 3.f, PadEdge, false)->SetCastShadow(false);
		SolidBoxes.Add(FBox2D(Mn, Mx));
	}
	// Posts and pylons (BUMPER_RADIUS 0.955).
	const FVector2D Posts[] = { {-510.f, 270.f}, {420.f, 340.f}, {-340.f, -460.f}, {-660.f, 560.f}, {580.f, -260.f} };
	UMaterialInterface* PostMat = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#1C1F33")), 0.25f, FLinearColor::Black, 0.f, 0.8f);
	for (int32 i = 0; i < 5; ++i)
	{
		const bool bPylon = i >= 3;
		const float H = bPylon ? 220.f : 110.f;
		AddCylinder(Posts[i], 95.5f, H, H, PostMat, true)->SetPhysMaterialOverride(RockPM);
		AddCylinder(Posts[i], 98.f, 10.f, H * 0.5f + 5.f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(i % 2 ? TEXT("#FF3EC8") : TEXT("#22E7FF")), 7.f), false)->SetCastShadow(false);
		AddCylinder(Posts[i], 60.f, 6.f, H + 6.f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#FFFFFF")), 4.f), false)->SetCastShadow(false);
		SolidPosts.Add(Posts[i]);
	}

	// City skyline: dark towers with glowing window bands, far below and around the court.
	FRandomStream Rng(555);
	UMaterialInterface* Tower = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#0E1020")), 0.5f);
	const TCHAR* WinCols[] = { TEXT("#22E7FF"), TEXT("#FF3EC8"), TEXT("#B026FF"), TEXT("#FFD166") };
	for (int32 i = 0; i < 70; ++i)
	{
		const float A = Rng.FRandRange(0.f, 2.f * PI);
		const float D = Rng.FRandRange(4000.f, 11000.f);
		const FVector2D C(FMath::Cos(A) * D, FMath::Sin(A) * D);
		const float W = Rng.FRandRange(300.f, 900.f), H = Rng.FRandRange(1500.f, 6000.f), Base = -4500.f;
		AddBox(C - FVector2D(W, W) * 0.5f, C + FVector2D(W, W) * 0.5f, Base, Base + H, Tower, false)->SetCastShadow(false);
		UMaterialInterface* Win = ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(WinCols[i % 4]), Rng.FRandRange(2.f, 6.f));
		const int32 Bands = Rng.RandRange(2, 6);
		for (int32 b = 0; b < Bands; ++b)
		{
			const float Z = Base + H * Rng.FRandRange(0.35f, 0.97f);
			AddBox(C - FVector2D(W, W) * 0.5f - FVector2D(4.f, 4.f), C + FVector2D(W, W) * 0.5f + FVector2D(4.f, 4.f), Z, Z + Rng.FRandRange(15.f, 50.f), Win, false)->SetCastShadow(false);
		}
	}
	AddStars(900, 2.f, 5.f, FLinearColor(0.85f, 0.9f, 1.f), 21);
}
