#include "ZoogiArena.h"
#include "ZoogiRock.h"
#include "ZoogiTopping.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ZoogiArenaShowcase.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiFloorScars.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
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

	Forming = CreateDefaultSubobject<UZoogiStageForming>(TEXT("Forming"));
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
		const float T = FMath::Max(0.f, FMath::Min(TX, TY));
		// The rails cover each side except the corner mouths (BuildNeon: Mouth 315): a line that meets a rail
		// bounces back instead of leaving the court, so for planning there is no edge that way.
		const FVector2D E = P + Dir * T;
		const float Mouth = 315.f, Slack = 40.f; // a ball centre within ~a radius of the rail end still clips it
		const bool bRail = TX < TY ? FMath::Abs(E.Y) < GNeonHalfY - Mouth + Slack : FMath::Abs(E.X) < GNeonHalfX - Mouth + Slack;
		return bRail ? T + 3000.f : T;
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

bool AZoogiArena::IsPathBlocked(const FVector2D& From, const FVector2D& To, float Radius) const
{
	const FVector2D D = To - From;
	const float Len = D.Size();
	if (Len < 1.f) return false;
	// Frozen Ring stage pieces with a live collider (pillars, obelisks, walls, bumpers ...).
	for (const AZoogiArenaPiece* Pc : Pieces)
	{
		FVector2D Q; float PR = 0.f;
		if (!Pc || !Pc->GetPathBlock(Q, PR)) continue;
		const float T = FMath::Clamp(FVector2D::DotProduct(Q - From, D) / (Len * Len), 0.f, 1.f);
		if (FVector2D::Distance(From + D * T, Q) < Radius + PR) return true;
	}
	for (const FVector2D& Q : SolidPosts)
	{
		const float T = FMath::Clamp(FVector2D::DotProduct(Q - From, D) / (Len * Len), 0.f, 1.f);
		if (FVector2D::Distance(From + D * T, Q) < Radius + 95.5f) return true;
	}
	for (const FBox2D& B : SolidBoxes)
	{
		// Slab test against the box grown by the ball radius.
		const FVector2D Mn = B.Min - FVector2D(Radius, Radius), Mx = B.Max + FVector2D(Radius, Radius);
		float T0 = 0.f, T1 = 1.f;
		bool bMiss = false;
		for (int32 a = 0; a < 2 && !bMiss; ++a)
		{
			const float O = a ? From.Y : From.X, Dd = a ? D.Y : D.X, Lo = a ? Mn.Y : Mn.X, Hi = a ? Mx.Y : Mx.X;
			if (FMath::Abs(Dd) < 1e-4f) { if (O < Lo || O > Hi) bMiss = true; continue; }
			float Ta = (Lo - O) / Dd, Tb = (Hi - O) / Dd;
			if (Ta > Tb) Swap(Ta, Tb);
			T0 = FMath::Max(T0, Ta); T1 = FMath::Min(T1, Tb);
			if (T0 > T1) bMiss = true;
		}
		if (!bMiss) return true;
	}
	return false;
}

FVector AZoogiArena::GetSpawnPoint(int32 PlayerIndex) const
{
	if (Theme == EZoogiArena::Neon)
	{
		const FVector2D S = GNeonSpawns[PlayerIndex % 4];
		return FVector(S.X, S.Y, ZoogiConst::ZoogiRadius + 2.f);
	}
	const float A = FMath::DegreesToRadians(GSpawnAngles[PlayerIndex % 4]);
	const float SR = SpawnRadiusOverride > 0.f ? SpawnRadiusOverride : ZoogiConst::SpawnRadius;
	return FVector(FMath::Cos(A) * SR, FMath::Sin(A) * SR, ZoogiConst::ZoogiRadius + 2.f);
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
	for (AZoogiRock* R : Rocks) if (IsValid(R)) R->Destroy();
	Rocks.Reset();
	for (AZoogiTopping* T : Toppings) if (IsValid(T)) T->Destroy();
	Toppings.Reset();
	if (Forming) Forming->Clear();
	for (AZoogiArenaPiece* P : Pieces) if (IsValid(P)) P->Destroy();
	Pieces.Reset();
	if (IsValid(Scars)) Scars->Destroy();
	Scars = nullptr;
	FloorMID = nullptr;
	FrozenMats.Reset();
	SpawnRadiusOverride = 0.f;
	OrbRingRadiusOverride = 0.f;
	OrbRingYawOffset = 0.f;
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
		if (InTerraceGap(A)) continue;
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
	ExtraOrbSpots.Reset();
	TerraceGapYaw.Reset(); TerraceGapHalfDeg = 0.f;
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
	// Camera collision: decorative props near the court (trees, torches, pots, domes...) block only the
	// camera probe channel so the action camera never clips through them. Physics is unaffected.
	int32 CamBlockers = 0;
	for (UActorComponent* C : Built)
	{
		UStaticMeshComponent* M = Cast<UStaticMeshComponent>(C);
		if (!M || Cast<UInstancedStaticMeshComponent>(M) || M->GetCollisionEnabled() != ECollisionEnabled::NoCollision) continue;
		if (M->ComponentHasTag(TEXT("NoCamBlock"))) continue; // hub core / flush kit pieces (Buzzy kit K3)
		const FBoxSphereBounds Bd = M->Bounds;
		const FVector O = Bd.Origin;
		if (Bd.SphereRadius > 900.f || Bd.SphereRadius < 25.f || FVector2D(O.X, O.Y).Size() > 4000.f || O.Z > 2000.f || O.Z < -1500.f) continue;
		if (FVector2D(O.X, O.Y).Size() < 400.f && Bd.SphereRadius > 300.f) continue; // centre-court pieces
		M->SetCollisionResponseToAllChannels(ECR_Ignore);
		M->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		++CamBlockers;
	}
	UE_LOG(LogZoogi, Log, TEXT("Built arena %s with %d components (%d camera blockers)"), *ZoogiUtil::ArenaName(InTheme), Built.Num(), CamBlockers);
	// -ZoogiArenaShots: fixed-camera A/B views of the arena (test tooling, see ZoogiArenaShowcase.h).
	static const TCHAR* Slugs[] = { TEXT("meadow"), TEXT("frozen"), TEXT("volcanic"), TEXT("cosmic"), TEXT("arabian"), TEXT("tomb"), TEXT("neon") };
	const int32 SlugIdx = (int32)InTheme;
	AZoogiArenaShowcase::MaybeStart(GetWorld(), SlugIdx >= 0 && SlugIdx < UE_ARRAY_COUNT(Slugs) ? FString(Slugs[SlugIdx]) : FString::FromInt(SlugIdx));
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
	SkyLight->SetIntensity(1.f); // per-theme overrides below (Frozen)

	Fog->SetVolumetricFog(false);
	Fog->SetStartDistance(0.f);
	Fog->SetFogCutoffDistance(0.f); // AddPanoramaSky raises it so the painted sky sphere is not fogged
	Clouds->SetVisibility(true);
	Atmosphere->SetVisibility(true);
	SkyLight->SetIntensity(1.f);
	Atmosphere->SetRayleighScatteringScale(0.0331f);
	Atmosphere->SetMieScatteringScale(0.003996f);

	Sun->SetEnableLightShaftBloom(false);
	switch (InTheme)
	{
	case EZoogiArena::Frozen:
		// Buzzy Frozen Ring (Oct 6): bright crisp anime daylight, cool key from high up, pale blue haze, aurora sky panorama.
		// Round 3c: sun rays (light-shaft bloom from the key light when the sun side is in frame).
		Sun->SetEnableLightShaftBloom(true);
		Sun->SetBloomScale(0.35f);
		Sun->SetBloomThreshold(0.55f);
		Sun->SetBloomTint(FColor(235, 245, 255));
		Sun->SetRelativeRotation(FRotator(-38.f, 125.f, 0.f));
		Sun->SetIntensity(8.f);
		Sun->SetLightColor(FLinearColor(0.9f, 0.95f, 1.f));
		Fog->SetFogDensity(0.02f);
		Fog->SetStartDistance(9000.f); // court stays crisp; far mountains fade into the pale haze like the art
		Fog->SetFogInscatteringColor(FLinearColor(0.72f, 0.84f, 0.98f));
		Clouds->SetVisibility(false);
		SkyLight->SetIntensity(1.35f);
		P.AutoExposureBias = -0.8f;
		P.BloomIntensity = 1.1f;
		P.ColorSaturation = FVector4(0.95f, 1.f, 1.1f, 1.f);
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
	case EZoogiArena::Meadow:
		// r3 (Buzzy Meadow): warm high sun, saturated greens, light haze so the painted hills/waterfalls read.
		Sun->SetRelativeRotation(FRotator(-48.f, 215.f, 0.f));
		Sun->SetIntensity(9.5f);
		Sun->SetLightColor(FLinearColor(1.f, 0.9f, 0.74f)); // r10: warmer golden afternoon
		Fog->SetFogDensity(0.0012f);
		Fog->SetStartDistance(16000.f);
		Fog->SetFogInscatteringColor(FLinearColor(0.55f, 0.72f, 0.95f));
		Clouds->SetVisibility(false);
		SkyLight->SetIntensity(1.25f);
		P.AutoExposureBias = -0.3f;
		P.BloomIntensity = 1.0f;
		P.ColorSaturation = FVector4(0.95f, 0.95f, 0.95f, 1.f); // r10: art palette is soft/muted
		P.ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.f);
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

void AZoogiArena::BuildIsland(const FLinearColor& TopA, const FLinearColor& TopB, const FLinearColor& Band, const FLinearColor& RockA, const FLinearColor& RockB, float FloorFriction, UMaterialInterface* TopOverride)
{
	FloorPM = ZoogiPhysics::MakeMaterial(this, FloorFriction, 0.1f, true);
	const float R = ZoogiConst::KnockoffRadius;
	UMaterialInterface* Top = TopOverride ? TopOverride : ZoogiUtil::GroundMID(this, TopA, TopB, 0.0025f, 0.92f);
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

void AZoogiArena::ResetRocks()
{
	for (AZoogiRock* R : Rocks) if (IsValid(R)) R->ResetRock();
	for (AZoogiTopping* T : Toppings) if (IsValid(T)) T->ResetTopping();
}

AZoogiRock* AZoogiArena::NearestStandingRock(const FVector2D& From) const
{
	AZoogiRock* Best = nullptr;
	float BestD = TNumericLimits<float>::Max();
	for (AZoogiRock* R : Rocks)
	{
		if (!IsValid(R) || !R->IsStanding()) continue;
		const float D = FVector2D::Distance(From, FVector2D(R->GetActorLocation()));
		if (D < BestD) { BestD = D; Best = R; }
	}
	return Best;
}

void AZoogiArena::BuildRim(const TArray<FVector4>& Marks, const FLinearColor& A, const FLinearColor& B, float Restitution, bool bDestructible,
	float CarvedLump, const FLinearColor& RuneColor, float RuneStrength)
{
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, Restitution, false);
	if (bDestructible)
	{
		// Destructible boulders (AZoogiRock): same footprint / look as the static ones below.
		int32 Seed = 11;
		for (const FVector4& M : Marks)
		{
			const float Ang = FMath::DegreesToRadians(M.X);
			const FVector P(FMath::Cos(Ang) * M.Y * 100.f, FMath::Sin(Ang) * M.Y * 100.f, 0.f);
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AZoogiRock* R = GetWorld()->SpawnActor<AZoogiRock>(AZoogiRock::StaticClass(), FTransform(P), SP);
			if (!R) continue;
			if (CarvedLump > 0.f) R->SetCarvedStyle(CarvedLump, RuneColor, RuneStrength); // Buzzy rune-stone look (visual only)
			R->InitRock(M.Z * 100.f, M.W * 100.f, M.X, A, B, RockPM, Seed++);
			Rocks.Add(R);
		}
		return;
	}
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
	// Animated "space journey" backdrop (nebula sky, star streaks, drifting planets). Falls back to the
	// old static star shell if the /Game/Env/Cosmic assets are missing (or with -ZoogiNoSpaceBackdrop, for A/B tests).
	FRandomStream Rng(42);
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSpaceBackdrop")) || !AddSpaceBackdrop())
	{
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
	}
	UStaticMesh* Crystal = ZoogiUtil::LoadMesh(TEXT("/Game/Env/AlienCrystal/SM_AlienCrystal.SM_AlienCrystal"));
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * (2.f * PI / 10.f) + Rng.FRandRange(-0.15f, 0.15f);
		const float D = Rng.FRandRange(2800.f, 6000.f);
		BuildIslet(FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, Rng.FRandRange(-1500.f, 800.f)), Rng.FRandRange(300.f, 600.f),
			ZoogiUtil::Hex(TEXT("#2A2550")), ZoogiUtil::Hex(TEXT("#3F3A7A")), ZoogiUtil::Hex(TEXT("#15122B")), Crystal, 2, 500.f, Rng);
	}
}

// Cosmic "space journey" backdrop. Assets: /Game/Env/Cosmic (Scripts/make_cosmic_backdrop.py, cooked via /Game/Env).
// Cost: one sky sphere draw (unlit, 2 texture reads), one ISM draw of small additive streak quads whose motion is
// all World Position Offset on the GPU (no CPU/tick cost), two translucent planet quads on a slowly rotating pivot.
// Readability: the sky is dimmed straight under the board (NadirDim) and the streaks fade out within ~28-63 m of
// the arena centre, so nothing moves directly behind the play area.
namespace ZoogiCosmic
{
	static constexpr int32 StreakCount = 600;
	static constexpr float StreakRange = 26000.f;   // must match M_CosmicStreak "Range"
	static constexpr float SkyRadius = 90000.f;
	static constexpr float PlanetYawRate = 0.5f;    // deg/s (sky: far nebula ~0.22, near wisps ~0.43; streaks fastest)
}

bool AZoogiArena::AddSpaceBackdrop()
{
	UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Cosmic/M_CosmicSky.M_CosmicSky"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* StreakMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Cosmic/M_CosmicStreak.M_CosmicStreak"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* PlanetMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Cosmic/M_CosmicPlanet.M_CosmicPlanet"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UTexture* PlanetA = LoadObject<UTexture>(nullptr, TEXT("/Game/Env/Cosmic/T_CosmicPlanetA.T_CosmicPlanetA"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UTexture* PlanetB = LoadObject<UTexture>(nullptr, TEXT("/Game/Env/Cosmic/T_CosmicPlanetB.T_CosmicPlanetB"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!SkyMat)
	{
		UE_LOG(LogZoogi, Warning, TEXT("Cosmic backdrop: /Game/Env/Cosmic/M_CosmicSky missing, using the static star shell"));
		return false;
	}
	auto NoGI = [](UPrimitiveComponent* C)
	{
		C->SetCastShadow(false);
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->bVisibleInRayTracing = false;
		C->SetReceivesDecals(false);
	};

	// 1) Nebula sky sphere (view-direction mapped, so it reads as infinitely far).
	UStaticMeshComponent* Sky = NewObject<UStaticMeshComponent>(this);
	Sky->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(Root);
	Sky->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NoGI(Sky);
	Sky->SetRelativeScale3D(FVector(ZoogiCosmic::SkyRadius / 50.f));
	Sky->RegisterComponent();
	Sky->SetMaterial(0, SkyMat);
	Built.Add(Sky);

	// 2) Star streaks flowing past the arena (crossed quads so they read from high and low cameras).
	if (StreakMat)
	{
		UInstancedStaticMeshComponent* S = NewObject<UInstancedStaticMeshComponent>(this);
		S->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane")));
		S->SetMobility(EComponentMobility::Movable);
		S->SetupAttachment(Root);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NoGI(S);
		S->SetBoundsScale(2.5f); // WPO slides instances up to Range/2 along the travel axis
		S->RegisterComponent();
		S->SetMaterial(0, StreakMat);
		FRandomStream R(97);
		const float Half = ZoogiCosmic::StreakRange * 0.5f;
		for (int32 i = 0; i < ZoogiCosmic::StreakCount; ++i)
		{
			FVector P;
			do
			{
				// Weighted below the horizon: the Tactical camera looks down ~55 deg, so that is the visible backdrop.
				P = FVector(R.FRandRange(-Half, Half), R.FRandRange(-16000.f, 16000.f), R.FRandRange(-12000.f, 3500.f));
			} while (FVector(P.X * 0.35f, P.Y, P.Z + 400.f).Size() < 3400.f);
			const FVector Scale(R.FRandRange(1.5f, 4.5f), R.FRandRange(0.3f, 0.7f), 1.f); // plane is 100 cm
			S->AddInstance(FTransform(FRotator(0.f, 0.f, (i & 1) ? 90.f : 0.f), P, Scale));
		}
		Built.Add(S);
	}

	// 3) Distant planets on a slowly turning pivot (moves faster than the nebula -> parallax).
	if (PlanetMat)
	{
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetMobility(EComponentMobility::Movable);
		Pivot->SetupAttachment(Root);
		Pivot->RegisterComponent();
		Built.Add(Pivot);
		auto AddPlanet = [&](UTexture* Tex, float YawDeg, float ElevDeg, float Dist, float Size, float Bright)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane")));
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(Pivot);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			NoGI(C);
			const FVector N = FRotator(ElevDeg, YawDeg, 0.f).Vector();
			C->SetRelativeLocation(N * Dist);
			C->SetRelativeRotation(FRotationMatrix::MakeFromZX(-N, FVector::UpVector).Rotator());
			C->SetRelativeScale3D(FVector(Size / 100.f));
			C->RegisterComponent();
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PlanetMat, this);
			if (Tex) MID->SetTextureParameterValue(TEXT("Tex"), Tex);
			MID->SetScalarParameterValue(TEXT("Bright"), Bright);
			C->SetMaterial(0, MID);
			Built.Add(C);
		};
		AddPlanet(PlanetA, 10.f, -36.f, 52000.f, 13000.f, 0.7f); // low enough to drift through the Tactical view
		AddPlanet(PlanetB, 190.f, -42.f, 46000.f, 4000.f, 0.7f);
		URotatingMovementComponent* Spin = NewObject<URotatingMovementComponent>(this);
		Spin->bAutoRegisterUpdatedComponent = false;
		Spin->RotationRate = FRotator(0.f, ZoogiCosmic::PlanetYawRate, 0.f);
		Spin->RegisterComponent();
		Spin->SetUpdatedComponent(Pivot);
		Built.Add(Spin);
	}
	UE_LOG(LogZoogi, Log, TEXT("Cosmic backdrop: sky %s, streaks %s, planets %s"), *GetNameSafe(SkyMat), *GetNameSafe(StreakMat), *GetNameSafe(PlanetMat));
	return true;
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
	// RoundIsland kind="saturn", dressed as a PIZZA (Oct 4, Evan): melted-cheese floor over tomato sauce, a golden baked
	// crust instead of the glowing gold lip, same dark underside. Floor collider and knockoff line are unchanged.
	BuildIsland(ZoogiUtil::Hex(TEXT("#F2B544")), ZoogiUtil::Hex(TEXT("#E39A2E")), ZoogiUtil::Hex(TEXT("#B9773A")),
		ZoogiUtil::Hex(TEXT("#1A120C")), ZoogiUtil::Hex(TEXT("#2E2016")), 0.3f);
	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);
	{
		const float KR = ZoogiConst::KnockoffRadius;
		UStaticMesh* PizzaSph = ZoogiUtil::BasicShape(TEXT("Sphere"));
		// Visual-only instanced layers (no collision, no shadows; skipped by the camera-blocker pass in Build()).
		auto MakeISM = [&](UMaterialInterface* Mat) -> UInstancedStaticMeshComponent*
		{
			UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
			ISM->SetStaticMesh(PizzaSph);
			ISM->SetMobility(EComponentMobility::Movable);
			ISM->SetupAttachment(Root);
			ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ISM->SetCastShadow(false);
			ISM->RegisterComponent();
			if (Mat) ISM->SetMaterial(0, Mat);
			Built.Add(ISM);
			return ISM;
		};
		FRandomStream PizzaRng(2024);

		// Crust: a toasted band just under the floor edge plus a ring of puffy ellipsoids. Each puff's inner edge is at
		// the knockoff line and stays below Z = 0 there (about -2.5 cm), so nothing rises inside the court; the crust
		// peaks ~+15 cm about 48 cm outside the line. No colliders: marbles fall off exactly where they did.
		AddCylinder(FVector2D::ZeroVector, KR + 95.f, 50.f, -6.f, ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#B8742F")), ZoogiUtil::Hex(TEXT("#8E5222")), 0.01f, 0.9f), false)->SetCastShadow(false);
		// The puffs are ISMs, so they must use M_ZoogiSurface (SurfaceMID): M_ZoogiGround is not flagged "Used with
		// Instanced Static Meshes" (Scripts/fix_material_usage.py only flags Surface/Glow/FX), so a GroundMID on an ISM
		// falls back to the grey default material, which the purple night light turned slate grey. Two toasted shades
		// (golden / deeper bake) stand in for the ground noise; a faint warm self-lit tint keeps them golden at night.
		UInstancedStaticMeshComponent* Crust = MakeISM(ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#D9963F")), 0.55f, ZoogiUtil::Hex(TEXT("#FF9A40")), 0.12f));
		UInstancedStaticMeshComponent* CrustDeep = MakeISM(ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#B8702A")), 0.6f, ZoogiUtil::Hex(TEXT("#FF8A30")), 0.09f));
		UInstancedStaticMeshComponent* CrustChar = MakeISM(ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#6B3414")), 0.75f, ZoogiUtil::Hex(TEXT("#8A4A1E")), 0.03f));
		const int32 Puffs = 40;
		const float PuffR = KR + 48.f;
		for (int32 i = 0; i < Puffs; ++i)
		{
			const float Deg = i * 360.f / Puffs + PizzaRng.FRandRange(-2.f, 2.f);
			const float A = FMath::DegreesToRadians(Deg);
			const float Len = 2.f * PI * PuffR / Puffs * PizzaRng.FRandRange(1.3f, 1.5f);
			const float H = PizzaRng.FRandRange(44.f, 54.f);
			const float Semi = PizzaRng.FRandRange(48.f, 50.f); // radial half-width
			// Shade by index (no extra PizzaRng draws, so the floor dressing layout stays identical).
			(((i * 7) % 5) < 2 ? CrustDeep : Crust)->AddInstance(FTransform(FRotator(0.f, Deg + 90.f, 0.f), FVector(FMath::Cos(A) * PuffR, FMath::Sin(A) * PuffR, -10.f), FVector(Len / 100.f, Semi / 50.f, H / 100.f)));
			// Browned blister near the top of most puffs.
			if (PizzaRng.FRand() < 0.75f)
			{
				const float BDeg = Deg + PizzaRng.FRandRange(-1.5f, 1.5f), BA = FMath::DegreesToRadians(BDeg);
				const float BD = PuffR + PizzaRng.FRandRange(-12.f, 8.f);
				const float S = PizzaRng.FRandRange(0.22f, 0.45f);
				CrustChar->AddInstance(FTransform(FRotator(0.f, BDeg + 90.f + PizzaRng.FRandRange(-25.f, 25.f), 0.f), FVector(FMath::Cos(BA) * BD, FMath::Sin(BA) * BD, -10.f + H * 0.5f - 2.5f), FVector(S * 1.5f, S, 0.1f)));
			}
		}

		// Floor dressing: sauce showing through the cheese, browned spots and low cheese bubbles, clear of the spawn pads.
		auto FloorSpot = [&](float RMin, float RMax) -> FVector2D
		{
			FVector2D P = FVector2D::ZeroVector;
			for (int32 Try = 0; Try < 12; ++Try)
			{
				const float A = PizzaRng.FRandRange(0.f, 2.f * PI);
				const float D = FMath::Sqrt(PizzaRng.FRandRange(RMin * RMin, RMax * RMax));
				P = FVector2D(FMath::Cos(A) * D, FMath::Sin(A) * D);
				bool bClear = true;
				for (int32 s = 0; s < 4 && bClear; ++s) bClear = FVector2D::Distance(P, FVector2D(GetSpawnPoint(s))) > 140.f;
				if (bClear) break;
			}
			return P;
		};
		UInstancedStaticMeshComponent* Sauce = MakeISM(ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C2461F")), 0.45f));
		for (int32 i = 0; i < 22; ++i)
		{
			const FVector2D P = FloorSpot(150.f, 1380.f);
			const float S = PizzaRng.FRandRange(0.6f, 1.4f);
			Sauce->AddInstance(FTransform(FRotator(0.f, PizzaRng.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.35f), FVector(S * PizzaRng.FRandRange(1.f, 1.6f), S, 0.012f)));
		}
		UInstancedStaticMeshComponent* Browned = MakeISM(ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#A5612A")), 0.85f));
		for (int32 i = 0; i < 36; ++i)
		{
			const FVector2D P = FloorSpot(100.f, 1460.f);
			const float S = PizzaRng.FRandRange(0.25f, 0.7f);
			Browned->AddInstance(FTransform(FRotator(0.f, PizzaRng.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 1.1f), FVector(S * PizzaRng.FRandRange(1.f, 1.3f), S, 0.012f)));
		}
		UInstancedStaticMeshComponent* Bubbles = MakeISM(ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#FFD978")), 0.4f));
		for (int32 i = 0; i < 40; ++i)
		{
			const FVector2D P = FloorSpot(100.f, 1470.f);
			const float S = PizzaRng.FRandRange(0.18f, 0.36f);
			Bubbles->AddInstance(FTransform(FRotator(0.f, PizzaRng.FRandRange(0.f, 360.f), 0.f), FVector(P.X, P.Y, 0.f), FVector(S, S * PizzaRng.FRandRange(0.8f, 1.f), 0.05f)));
		}
		// Slice cuts: four thin sauce-dark lines through the centre (eight slices), angled clear of the spawn lanes
		// (0/90/180/270 deg) and the planters (8 deg minimum gap).
		UMaterialInterface* CutMat = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#9A3A1A")), 0.6f);
		UStaticMesh* PizzaCube = ZoogiUtil::BasicShape(TEXT("Cube"));
		for (int32 k = 0; k < 4; ++k)
		{
			UStaticMeshComponent* Cut = AddMesh(PizzaCube, CutMat, false, false);
			Cut->SetRelativeScale3D(FVector((KR - 15.f) * 2.f / 100.f, 0.06f, 0.01f)); // 6 cm wide, 1 cm thick
			Cut->SetRelativeRotation(FRotator(0.f, 8.f + k * 45.f, 0.f));
			Cut->SetRelativeLocation(FVector(0.f, 0.f, 0.8f));
		}
	}

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

	// Poppable toppings (AZoogiTopping): flat, overlap-only, never block or deflect a marble. Deterministic layout between
	// r 250 and 1350, clear of the spawn pads, the tomato planters and the orbs (AZoogiGameMode::SpawnOrbs layout).
	{
		TArray<FVector4> Keep; // x, y, clearance added to the topping radius
		for (int32 s = 0; s < 4; ++s) { const FVector S = GetSpawnPoint(s); Keep.Add(FVector4(S.X, S.Y, 150.f, 0.f)); }
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = FMath::DegreesToRadians(Angles[i]);
			const float D = (13.55f + Jitter[i]) * 100.f;
			Keep.Add(FVector4(FMath::Cos(A) * D, FMath::Sin(A) * D, 72.f + 120.f, 0.f));
		}
		for (int32 i = 0; i < ZoogiConst::OrbCount; ++i)
		{
			const float A = (float)i / ZoogiConst::OrbCount * 2.f * PI;
			Keep.Add(FVector4(FMath::Cos(A) * ZoogiConst::OrbRingRadius, FMath::Sin(A) * ZoogiConst::OrbRingRadius, ZoogiConst::OrbRadius + 25.f, 0.f));
		}
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = i * 2.f * PI / 3.f + PI / 6.f;
			Keep.Add(FVector4(FMath::Cos(A) * 90.f, FMath::Sin(A) * 90.f, ZoogiConst::OrbRadius + 25.f, 0.f));
		}
		FRandomStream TopRng(4242);
		TArray<FVector> Placed; // x, y, radius
		const int32 NumSlices = 21; // even = pepperoni (11), odd = cheese (10)
		for (int32 i = 0; i < NumSlices; ++i)
		{
			const EZoogiToppingKind Kind = (i % 2 == 0) ? EZoogiToppingKind::Pepperoni : EZoogiToppingKind::Cheese;
			const float TR = Kind == EZoogiToppingKind::Pepperoni ? TopRng.FRandRange(35.f, 45.f) : TopRng.FRandRange(20.f, 30.f);
			const float TYaw = TopRng.FRandRange(0.f, 360.f);
			bool bFound = false;
			FVector2D P = FVector2D::ZeroVector;
			for (int32 Try = 0; Try < 80 && !bFound; ++Try)
			{
				const float A = TopRng.FRandRange(0.f, 2.f * PI);
				const float D = FMath::Sqrt(TopRng.FRandRange(250.f * 250.f, 1350.f * 1350.f));
				P = FVector2D(FMath::Cos(A) * D, FMath::Sin(A) * D);
				bFound = true;
				for (const FVector4& K : Keep) if (FVector2D::Distance(P, FVector2D(K.X, K.Y)) < K.Z + TR) { bFound = false; break; }
				for (const FVector& Q : Placed) if (bFound && FVector2D::Distance(P, FVector2D(Q.X, Q.Y)) < Q.Z + TR + 60.f) { bFound = false; break; }
			}
			if (!bFound) continue;
			Placed.Add(FVector(P.X, P.Y, TR));
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AZoogiTopping* T = GetWorld()->SpawnActor<AZoogiTopping>(AZoogiTopping::StaticClass(), FTransform(FVector(P.X, P.Y, 0.f)), SP);
			if (!T) continue;
			T->InitTopping(Kind, TR, TYaw, 500 + i / 2); // cheese alternates blob / wedge by seed parity
			Toppings.Add(T);
		}
		UE_LOG(LogZoogi, Log, TEXT("Pizza court: %d toppings"), Toppings.Num());
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
