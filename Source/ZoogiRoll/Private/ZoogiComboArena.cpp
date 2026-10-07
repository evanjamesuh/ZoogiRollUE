// 2v2 Combo prototype props on the Meadow arena (Part 4 "Vertical Arena + Reactive Terrain").
#include "ZoogiCombo.h"
#include "ZoogiMeadowLayout.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiPhysics.h"
#include "ZoogiFX.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiStageForming.h"
#include "ZoogiFloorScars.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiMeshKit.h"

AZoogiComboArena::AZoogiComboArena()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

// Layout statics (Meadow prototype defaults; SetLayout switches them per theme).
float AZoogiComboArena::WallR = 1150.f;
float AZoogiComboArena::WallA0 = 195.f;
float AZoogiComboArena::WallA1 = 245.f;
float AZoogiComboArena::WallH = 85.f;
float AZoogiComboArena::RampH = 45.f;
float AZoogiComboArena::PlatformR = 200.f;
float AZoogiComboArena::PlatformH = 55.f;
FVector2D AZoogiComboArena::LRampMin(-760.f, 290.f);
FVector2D AZoogiComboArena::LRampMax(-540.f, 510.f);
FVector2D AZoogiComboArena::LSecMin(-150.f, 200.f);
FVector2D AZoogiComboArena::LSecMax(250.f, 600.f);
FVector2D AZoogiComboArena::LPlat(750.f, 900.f);
FVector2D AZoogiComboArena::LGate(0.f, -1450.f);
FVector AZoogiComboArena::LGateReturn(0.f, -1180.f, 52.f);
bool AZoogiComboArena::bFrozenLayout = false;

namespace
{
	TArray<FVector2D>& MutableBumpers()
	{
		static TArray<FVector2D> B = { FVector2D(420.f, -420.f), FVector2D(-500.f, 950.f), FVector2D(850.f, -850.f) };
		return B;
	}
	FVector FPol(float R, float YawDeg, float Z = 0.f)
	{
		const float A = FMath::DegreesToRadians(YawDeg);
		return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z);
	}
}

const TArray<FVector2D>& AZoogiComboArena::BumperXY()
{
	return MutableBumpers();
}

void AZoogiComboArena::SetLayout(EZoogiArena Theme)
{
	using namespace ZoogiFrozen;
	bFrozenLayout = Theme == EZoogiArena::Frozen;
	TArray<FVector2D>& B = MutableBumpers();
	if (bFrozenLayout)
	{
		WallR = ZoogiFrozen::WallR; WallA0 = WallNA0; WallA1 = WallNA1; WallH = ZoogiFrozen::WallH;
		RampH = ZoogiFrozen::RampH;
		LRampMin = FVector2D(RampCX - 110.f, RampCY - 110.f); LRampMax = FVector2D(RampCX + 110.f, RampCY + 110.f);
		LSecMin = FVector2D(SecX0, SecY0); LSecMax = FVector2D(SecX1, SecY1);
		LPlat = FVector2D(PlatX, PlatY); PlatformR = PlatR; PlatformH = PlatH;
		LGate = FVector2D(FPol(ArchR, GateYaw));
		LGateReturn = FPol(GateReturnR, GateYaw, 52.f);
		B = { FVector2D(BumpX[0], BumpY[0]), FVector2D(BumpX[1], BumpY[1]), FVector2D(BumpX[2], BumpY[2]) };
	}
	else
	{
		WallR = 1150.f; WallA0 = 195.f; WallA1 = 245.f; WallH = 85.f; RampH = 45.f;
		LRampMin = FVector2D(-760.f, 290.f); LRampMax = FVector2D(-540.f, 510.f);
		LSecMin = FVector2D(-150.f, 200.f); LSecMax = FVector2D(250.f, 600.f);
		LPlat = FVector2D(750.f, 900.f); PlatformR = 200.f; PlatformH = 55.f;
		LGate = FVector2D(0.f, -1450.f); LGateReturn = FVector(0.f, -1180.f, 52.f);
		B = { FVector2D(575.f, -996.f), FVector2D(-500.f, 950.f), FVector2D(850.f, -850.f) }; // r19: bumper off the spiral
		LPlat = FVector2D(1111.f, -298.f); // r19: platform clear of the curved lip (was 750,900)
		// r19: Meadow is built 1.3x larger; scale the prop layout with it.
		const float K = ZoogiMeadow::MeadowScale;
		WallR *= K; LRampMin *= K; LRampMax *= K; LSecMin *= K; LSecMax *= K; LPlat *= K; LGate *= K;
		LGateReturn = FVector(LGateReturn.X * K, LGateReturn.Y * K, LGateReturn.Z);
		for (FVector2D& Xy : B) Xy *= K;
	}
}

void AZoogiComboArena::Destroyed()
{
	for (AZoogiArenaPiece* P : IceBumpers) if (IsValid(P)) P->Destroy();
	IceBumpers.Reset();
	Super::Destroyed();
}

FString AZoogiComboArena::FloorStateName(EZoogiFloorState S)
{
	switch (S)
	{
	case EZoogiFloorState::Normal: return TEXT("NORMAL");
	case EZoogiFloorState::Damaged: return TEXT("DAMAGED");
	case EZoogiFloorState::Cracked: return TEXT("CRACKED");
	}
	return TEXT("");
}

UStaticMeshComponent* AZoogiComboArena::AddPiece(const TCHAR* Shape, UMaterialInterface* Mat, bool bCollide, FName Tag, UPhysicalMaterial* PM)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	if (bCollide)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (PM) C->SetPhysMaterialOverride(PM);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (!Tag.IsNone()) C->ComponentTags.Add(Tag);
	C->RegisterComponent();
	if (Mat) C->SetMaterial(0, Mat);
	return C;
}

UStaticMeshComponent* AZoogiComboArena::AddBoxAt(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, FName Tag, UPhysicalMaterial* PM)
{
	UStaticMeshComponent* C = AddPiece(TEXT("Cube"), Mat, bCollide, Tag, PM);
	C->SetWorldTransform(FTransform(Rot, Center, Size / 100.f));
	return C;
}

UStaticMeshComponent* AZoogiComboArena::AddCyl(const FVector2D& XY, float Radius, float Bottom, float Top, UMaterialInterface* Mat, bool bCollide, FName Tag, UPhysicalMaterial* PM)
{
	UStaticMeshComponent* C = AddPiece(TEXT("Cylinder"), Mat, bCollide, Tag, PM);
	const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
	const FVector S(Radius / B.BoxExtent.X, Radius / B.BoxExtent.Y, (Top - Bottom) / (2.f * B.BoxExtent.Z));
	C->SetWorldScale3D(S);
	C->SetWorldLocation(FVector(XY.X, XY.Y, Top - (B.Origin.Z + B.BoxExtent.Z) * S.Z));
	return C;
}

void AZoogiComboArena::PlaceSlab(UStaticMeshComponent* C, float X0, float Z0, float X1, float Z1, float Y0, float Y1, float Thickness)
{
	if (!C) return;
	const float DX = X1 - X0, DZ = Z1 - Z0;
	const float L = FMath::Sqrt(DX * DX + DZ * DZ);
	const float Th = FMath::Atan2(DZ, DX);
	const FVector Mid((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, (Z0 + Z1) * 0.5f);
	const FVector N(-FMath::Sin(Th), 0.f, FMath::Cos(Th)); // top-face normal
	const FVector Center = Mid - N * (Thickness * 0.5f);
	C->SetWorldTransform(FTransform(FRotator(FMath::RadiansToDegrees(Th), 0.f, 0.f), Center, FVector(L, Y1 - Y0, Thickness) / 100.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void AZoogiComboArena::Build()
{
	WallPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.9f, false);
	BumperPM = ZoogiPhysics::MakeMaterial(this, 0.2f, 0.95f, false);
	StonePM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.1f, true);
	if (bFrozenLayout && Arena && Arena->GetTheme() == EZoogiArena::Frozen) { BuildFrozen(); return; }
	const FLinearColor Teal = ZoogiUtil::Hex(TEXT("#2DD4BF"));
	UMaterialInterface* Stone = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#8C8577")), ZoogiUtil::Hex(TEXT("#6E685C")), 0.01f, 0.85f);
	UMaterialInterface* Glow = ZoogiUtil::GlowMID(this, Teal, 4.f);

	// 1) Curved ricochet wall: an arc of stone blocks (inner face on R = WallR) with a glowing top strip.
	{
		const int32 N = 12;
		for (int32 i = 0; i < N; ++i)
		{
			const float A = FMath::Lerp(WallA0, WallA1, (i + 0.5f) / N);
			const float Rad = FMath::DegreesToRadians(A);
			const float SegLen = WallR * FMath::DegreesToRadians((WallA1 - WallA0) / N) * 1.08f;
			const FVector C(FMath::Cos(Rad) * (WallR + 20.f), FMath::Sin(Rad) * (WallR + 20.f), WallH * 0.5f - 10.f);
			AddBoxAt(C, FVector(40.f, SegLen, WallH + 20.f), FRotator(0.f, A, 0.f), Stone, true, TEXT("ZWall"), WallPM);
			AddBoxAt(FVector(C.X, C.Y, WallH + 1.f), FVector(44.f, SegLen, 4.f), FRotator(0.f, A, 0.f), Glow, false, NAME_None, nullptr);
		}
	}

	// 2) Launch ramp (rises toward +X, lip RampH high at RampMax().X). r20: not on Meadow.
	if (HasRamp())
	{
		UStaticMeshComponent* R = AddPiece(TEXT("Cube"), ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#C08A4A")), 0.7f), true, TEXT("ZRamp"), StonePM);
		PlaceSlab(R, RampMin().X, 0.f, RampMax().X, RampH, RampMin().Y, RampMax().Y, 60.f);
		// Chevrons on the ramp face (visual).
		for (int32 k = 0; k < 3; ++k)
		{
			const float X = FMath::Lerp(RampMin().X, RampMax().X, 0.25f + 0.25f * k);
			const float Z = RampH * (X - RampMin().X) / (RampMax().X - RampMin().X) + 1.5f;
			AddBoxAt(FVector(X, (RampMin().Y + RampMax().Y) * 0.5f, Z), FVector(12.f, 160.f, 2.f), FRotator(FMath::RadiansToDegrees(FMath::Atan2(RampH, RampMax().X - RampMin().X)), 0.f, 0.f), ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.75f, 0.2f), 3.f), false, NAME_None, nullptr);
		}
	}

	// 3) Raised platform (only reachable airborne; its side is a rebound face).
	AddCyl(PlatformXY(), PlatformR, -10.f, PlatformH, Stone, true, TEXT("ZPlatform"), StonePM);
	AddCyl(PlatformXY(), PlatformR + 6.f, PlatformH - 4.f, PlatformH + 1.f, Glow, false, NAME_None, nullptr);
	AddCyl(PlatformXY(), PlatformR - 6.f, PlatformH, PlatformH + 1.5f, ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#55A336")), ZoogiUtil::Hex(TEXT("#3F8A2C")), 0.004f, 0.9f), false, NAME_None, nullptr);

	// 4) Bumpers: lively round posts that kick a Zoogi back out (the kick is applied by the game mode on hit).
	for (const FVector2D& XY : BumperXY())
	{
		UStaticMeshComponent* B = AddCyl(XY, 70.f, -10.f, 90.f, ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.3f, 0.75f), 2.5f), true, TEXT("ZBumper"), BumperPM);
		Bumpers.Add(B);
		UStaticMeshComponent* Cap = AddPiece(TEXT("Sphere"), ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.85f, 0.95f), 5.f), false, NAME_None, nullptr);
		Cap->SetWorldTransform(FTransform(FRotator::ZeroRotator, FVector(XY.X, XY.Y, 90.f), FVector(1.2f, 1.2f, 0.35f)));
		BumperCaps.Add(Cap);
	}

	// 5) Recovery Gate: an arch at the south rim (visual only, ring-outs still fly under it) and a glowing return pad.
	{
		UMaterialInterface* GateMat = ZoogiUtil::GlowMID(this, FLinearColor(0.3f, 1.f, 0.6f), 4.f);
		const FVector2D G = GateXY();
		AddCyl(FVector2D(G.X - 170.f, G.Y), 16.f, 0.f, 230.f, GateMat, false, NAME_None, nullptr);
		AddCyl(FVector2D(G.X + 170.f, G.Y), 16.f, 0.f, 230.f, GateMat, false, NAME_None, nullptr);
		AddBoxAt(FVector(G.X, G.Y, 236.f), FVector(28.f, 372.f, 24.f), FRotator(0.f, 90.f, 0.f), GateMat, false, NAME_None, nullptr);
		GatePad = AddCyl(FVector2D(GateReturn().X, GateReturn().Y), 85.f, 0.f, 1.2f, ZoogiUtil::GlowMID(this, FLinearColor(0.3f, 1.f, 0.6f), 1.5f), false, NAME_None, nullptr);
		GateLight = NewObject<UPointLightComponent>(this);
		GateLight->SetupAttachment(Root);
		GateLight->RegisterComponent();
		GateLight->SetWorldLocation(FVector(G.X, G.Y + 120.f, 180.f));
		GateLight->SetIntensityUnits(ELightUnits::Candelas);
		GateLight->SetIntensity(40.f);
		GateLight->SetLightColor(FLinearColor(0.3f, 1.f, 0.6f));
		GateLight->SetAttenuationRadius(600.f);
		GateLight->SetCastShadows(false);
	}

	// 6) Damageable floor section (top flush with the floor while NORMAL).
	SlabMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#9A8F78")), ZoogiUtil::Hex(TEXT("#857A64")), 0.008f, 0.85f);
	SlabDamagedMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#7A6750")), ZoogiUtil::Hex(TEXT("#5F4E3B")), 0.01f, 0.9f);
	SlabCrackedMat = ZoogiUtil::GroundMID(this, ZoogiUtil::Hex(TEXT("#5C4530")), ZoogiUtil::Hex(TEXT("#3E2E20")), 0.012f, 0.95f);
	SlabA = AddPiece(TEXT("Cube"), SlabMat, true, TEXT("ZFloor"), StonePM);
	SlabB = AddPiece(TEXT("Cube"), SlabMat, true, TEXT("ZFloor"), StonePM);
	for (int32 i = 0; i < 6; ++i) Cracks.Add(AddPiece(TEXT("Cube"), ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.35f, 0.05f), 2.f), false, NAME_None, nullptr));
	// Outline so the section reads as a distinct floor tile.
	{
		UMaterialInterface* Edge = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#4B4033")), 0.8f);
		const FVector2D A = SectionMin(), B = SectionMax();
		AddBoxAt(FVector((A.X + B.X) * 0.5f, A.Y - 4.f, 0.3f), FVector(B.X - A.X + 16.f, 8.f, 1.f), FRotator::ZeroRotator, Edge, false, NAME_None, nullptr);
		AddBoxAt(FVector((A.X + B.X) * 0.5f, B.Y + 4.f, 0.3f), FVector(B.X - A.X + 16.f, 8.f, 1.f), FRotator::ZeroRotator, Edge, false, NAME_None, nullptr);
		AddBoxAt(FVector(A.X - 4.f, (A.Y + B.Y) * 0.5f, 0.3f), FVector(8.f, B.Y - A.Y, 1.f), FRotator::ZeroRotator, Edge, false, NAME_None, nullptr);
		AddBoxAt(FVector(B.X + 4.f, (A.Y + B.Y) * 0.5f, 0.3f), FVector(8.f, B.Y - A.Y, 1.f), FRotator::ZeroRotator, Edge, false, NAME_None, nullptr);
	}
	FloorState = EZoogiFloorState::Normal;
	ApplyFloorState();
	UE_LOG(LogZoogi, Log, TEXT("Combo arena props built: curved wall %.0f-%.0f deg, ramp, platform, %d bumpers, Recovery Gate, floor section %s"), WallA0, WallA1, Bumpers.Num(), *FloorStateName(FloorState));
}


// Frozen Ring combo props (Oct 6). The curved bank wall (N arc), the snow-chute ramp, the hex-ice platform and the
// Recovery Gate arch are arena stage pieces (ZoogiArenaFrozen.cpp, tagged ZWall / ZRamp / ZPlatform). Here: the three
// ice-crystal bumpers (destructible pieces, ZBumper), the gate return pad, the SW hex-ice reactive section and the
// rune sockets of the Arena Orb homes. Everything joins the arena's stage-forming intro.
void AZoogiComboArena::BuildFrozen()
{
	UZoogiStageForming* Form = Arena->Forming;
	UMaterialInterface* Ice = Arena->FrozenMat(TEXT("Ice"));
	UMaterialInterface* IceDeep = Arena->FrozenMat(TEXT("IceDeep"));
	UMaterialInterface* IcePane = Arena->FrozenMat(TEXT("IcePane"));
	UMaterialInterface* StoneDark = Arena->FrozenMat(TEXT("StoneDark"));
	UMaterialInterface* Snow = Arena->FrozenMat(TEXT("Snow"));
	UMaterialInterface* Rune = Arena->FrozenMat(TEXT("Rune"));
	const FLinearColor Cyan = ZoogiUtil::Hex(TEXT("#19C3E6"));
	if (!Rune) Rune = ZoogiUtil::GlowMID(this, Cyan, 5.f);
	if (!Ice) Ice = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8FD3F4")), 0.2f);
	if (!IceDeep) IceDeep = Ice;
	if (!IcePane) IcePane = Ice;
	if (!StoneDark) StoneDark = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#8E9DB4")), 0.7f);
	if (!Snow) Snow = ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#F2F8FF")), 0.8f);

	// 1) Ice-crystal bumpers: a stone socket ring with a cluster of glowing crystals (collider: the r70 kick post).
	int32 Bi = 0;
	for (const FVector2D& XY : BumperXY())
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AZoogiArenaPiece* P = GetWorld()->SpawnActor<AZoogiArenaPiece>(AZoogiArenaPiece::StaticClass(), FTransform(FRotator(0.f, 30.f * Bi, 0.f), FVector(XY.X, XY.Y, 0.f)), SP);
		if (!P) continue;
		P->Setup(TEXT("crystal_bumper"), TEXT("Crystal"), 140.f);
		FRandomStream R(300 + Bi);
		FZoogiMeshBuilder Socket, Main, Small;
		Socket.Prism(FVector2D::ZeroVector, 8, 86.f, 78.f, -4.f, 14.f, 22.5f, 0.f, false);
		Main.Prism(FVector2D::ZeroVector, 6, 46.f, 34.f, 10.f, 92.f, 0.f, 40.f, false);
		for (int32 c = 0; c < 4; ++c)
		{
			const float A = 45.f + 90.f * c + R.FRandRange(-15.f, 15.f);
			const FVector2D O(FPol(R.FRandRange(42.f, 52.f), A));
			Small.Prism(O, 5, R.FRandRange(14.f, 20.f), R.FRandRange(8.f, 11.f), 8.f, R.FRandRange(40.f, 62.f), A, R.FRandRange(10.f, 18.f), false);
		}
		P->AddProc(Socket, StoneDark);
		P->AddProc(Main, IcePane);
		P->AddProc(Small, Ice);
		UStaticMeshComponent* Ring = P->AddShape(TEXT("Cylinder"), FTransform(FRotator::ZeroRotator, FVector(0, 0, 14.5f), FVector(1.74f, 1.74f, 0.012f)), Rune, false);
		P->MarkGlow(Ring);
		UStaticMeshComponent* Core = P->AddShape(TEXT("Sphere"), FTransform(FRotator::ZeroRotator, FVector(0, 0, 70.f), FVector(0.42f, 0.42f, 0.7f)), Rune, false);
		P->MarkGlow(Core);
		P->AddColliderCyl(FVector2D::ZeroVector, 70.f, -10.f, 90.f, TEXT("ZBumper"), BumperPM);
		P->SetPathBlock(XY, 75.f);
		P->RubbleKeepZ = 16.f;
		P->FinishBuild();
		IceBumpers.Add(P);
		if (Form) Form->AddActor(P, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.55f + 0.15f * Bi, 0.75f, 0.f);
		++Bi;
	}

	// 2) Recovery Gate return pad (the gate itself is the SE rune arch, an arena piece).
	{
		GatePad = AddCyl(FVector2D(GateReturn().X, GateReturn().Y), 85.f, 0.f, 1.2f, ZoogiUtil::GlowMID(this, FLinearColor(0.3f, 1.f, 0.85f), 1.5f), false, NAME_None, nullptr);
		UStaticMeshComponent* Ring = AddCyl(FVector2D(GateReturn().X, GateReturn().Y), 100.f, 0.f, 0.8f, Rune, false, NAME_None, nullptr);
		GateLight = NewObject<UPointLightComponent>(this);
		GateLight->SetupAttachment(Root);
		GateLight->RegisterComponent();
		GateLight->SetWorldLocation(FVector(GateXY().X, GateXY().Y, 0.f) * 0.92f + FVector(0, 0, 180.f));
		GateLight->SetIntensityUnits(ELightUnits::Candelas);
		GateLight->SetIntensity(40.f);
		GateLight->SetLightColor(FLinearColor(0.3f, 1.f, 0.85f));
		GateLight->SetAttenuationRadius(600.f);
		GateLight->SetCastShadows(false);
		if (Form) { Form->AddComponent(GatePad, EZoogiFormStyle::Grow, FVector::ZeroVector, 2.0f, 0.4f); Form->AddComponent(Ring, EZoogiFormStyle::Grow, FVector::ZeroVector, 2.0f, 0.4f); }
	}

	// 3) Reactive hex-ice section (NORMAL -> DAMAGED -> CRACKED): ice slab, deep ice once damaged, dark cracked crater ice.
	{
		auto DamageMID = [this](UMaterialInterface* Src, float Dmg) -> UMaterialInterface*
		{
			// Src is itself a MID (arena FrozenMat): a MID parented to a MID renders black, so re-parent to its base material.
			UMaterialInstanceDynamic* SrcMID = Cast<UMaterialInstanceDynamic>(Src);
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SrcMID ? SrcMID->Parent.Get() : Src, this);
			if (SrcMID) M->CopyParameterOverrides(SrcMID);
			M->SetScalarParameterValue(TEXT("Damage"), Dmg);
			M->SetVectorParameterValue(TEXT("CrackColor"), ZoogiUtil::Hex(TEXT("#19C3E6")));
			return M;
		};
		SlabMat = Ice;
		SlabDamagedMat = DamageMID(Ice, 0.42f);
		SlabCrackedMat = DamageMID(Ice, 0.8f);
		SlabA = AddPiece(TEXT("Cube"), SlabMat, true, TEXT("ZFloor"), StonePM);
		SlabB = AddPiece(TEXT("Cube"), SlabMat, true, TEXT("ZFloor"), StonePM);
		for (int32 i = 0; i < 6; ++i) Cracks.Add(AddPiece(TEXT("Cube"), Rune, false, NAME_None, nullptr));
		const FVector2D A = SectionMin(), B = SectionMax();
		TArray<UStaticMeshComponent*> Edge;
		Edge.Add(AddBoxAt(FVector((A.X + B.X) * 0.5f, A.Y - 5.f, 0.4f), FVector(B.X - A.X + 20.f, 10.f, 1.2f), FRotator::ZeroRotator, Rune, false, NAME_None, nullptr));
		Edge.Add(AddBoxAt(FVector((A.X + B.X) * 0.5f, B.Y + 5.f, 0.4f), FVector(B.X - A.X + 20.f, 10.f, 1.2f), FRotator::ZeroRotator, Rune, false, NAME_None, nullptr));
		Edge.Add(AddBoxAt(FVector(A.X - 5.f, (A.Y + B.Y) * 0.5f, 0.4f), FVector(10.f, B.Y - A.Y, 1.2f), FRotator::ZeroRotator, Rune, false, NAME_None, nullptr));
		Edge.Add(AddBoxAt(FVector(B.X + 5.f, (A.Y + B.Y) * 0.5f, 0.4f), FVector(10.f, B.Y - A.Y, 1.2f), FRotator::ZeroRotator, Rune, false, NAME_None, nullptr));
		FloorState = EZoogiFloorState::Normal;
		ApplyFloorState();
		if (Form)
		{
			Form->AddComponent(SlabA, EZoogiFormStyle::Rise, FVector(0, 0, -45.f), 1.3f, 0.6f);
			for (UStaticMeshComponent* E : Edge) Form->AddComponent(E, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.7f, 0.4f);
		}
	}

	// 4) Arena Orb homes: small rune sockets in the ice (where the six orbs spawn and are re-seeded).
	for (int32 i = 0; i < OrbHomes.Num(); ++i)
	{
		const FVector2D H = OrbHomes[i];
		UStaticMeshComponent* Disc = AddCyl(H, 62.f, 0.f, 0.7f, IceDeep, false, NAME_None, nullptr);
		UStaticMeshComponent* Ring = AddCyl(H, 70.f, 0.f, 0.5f, Rune, false, NAME_None, nullptr);
		Disc->SetCastShadow(false); Ring->SetCastShadow(false);
		if (Form) { Form->AddComponent(Disc, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.9f + 0.05f * i, 0.35f); Form->AddComponent(Ring, EZoogiFormStyle::Grow, FVector::ZeroVector, 1.9f + 0.05f * i, 0.35f); }
	}
	UE_LOG(LogZoogi, Log, TEXT("Combo arena props built (FROZEN RING): bank wall %.0f-%.0f deg r%.0f (stage pieces), ramp, hex platform, %d ice-crystal bumpers, Recovery Gate (SE arch) pad at (%.0f, %.0f), hex-ice section, %d orb sockets"),
		WallA0, WallA1, WallR, IceBumpers.Num(), GateReturn().X, GateReturn().Y, OrbHomes.Num());
}

bool AZoogiComboArena::IsInSection(const FVector2D& P, float Pad) const
{
	return P.X >= SectionMin().X - Pad && P.X <= SectionMax().X + Pad && P.Y >= SectionMin().Y - Pad && P.Y <= SectionMax().Y + Pad;
}

void AZoogiComboArena::ApplyFloorState()
{
	const float X0 = SectionMin().X, X1 = SectionMax().X, Y0 = SectionMin().Y, Y1 = SectionMax().Y, XM = (X0 + X1) * 0.5f;
	const float T = 40.f;
	auto Hide = [](UStaticMeshComponent* C) { C->SetVisibility(false); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); };
	auto Show = [](UStaticMeshComponent* C) { C->SetVisibility(true); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); };
	for (UStaticMeshComponent* C : Cracks) C->SetVisibility(false);
	// Heights along the travel axis; LipDir flips which side rises.
	auto H = [this, X0, X1](float X, float ZLow, float ZHigh) { const float U = (X - X0) / (X1 - X0); return LipDir > 0.f ? FMath::Lerp(ZLow, ZHigh, U) : FMath::Lerp(ZHigh, ZLow, U); };
	switch (FloorState)
	{
	case EZoogiFloorState::Normal:
		Show(SlabA); Hide(SlabB);
		PlaceSlab(SlabA, X0, 0.5f, X1, 0.5f, Y0, Y1, T);
		SlabA->SetMaterial(0, SlabMat);
		break;
	case EZoogiFloorState::Damaged:
		// Tilted slab: a slope (rolling Zoogis / orbs drift off it) with a 28 cm raised edge (a bank face).
		Show(SlabA); Hide(SlabB);
		PlaceSlab(SlabA, X0, H(X0, 0.5f, 28.f), X1, H(X1, 0.5f, 28.f), Y0, Y1, T);
		SlabA->SetMaterial(0, SlabDamagedMat);
		break;
	case EZoogiFloorState::Cracked:
	{
		// Split: the near half is a gentle slope, the far half a steep raised lip (launch angle / bank face).
		Show(SlabA); Show(SlabB);
		const float ZMid = 22.f, ZLip = 70.f;
		if (LipDir > 0.f) { PlaceSlab(SlabA, X0, 0.5f, XM, ZMid, Y0, Y1, T); PlaceSlab(SlabB, XM, ZMid, X1, ZLip, Y0, Y1, T); }
		else { PlaceSlab(SlabA, XM, ZMid, X1, 0.5f, Y0, Y1, T); PlaceSlab(SlabB, X0, ZLip, XM, ZMid, Y0, Y1, T); }
		SlabA->SetMaterial(0, SlabCrackedMat);
		SlabB->SetMaterial(0, SlabCrackedMat);
		break;
	}
	}
	// Glowing cracks on the tile.
	if (FloorState != EZoogiFloorState::Normal)
	{
		const int32 N = FloorState == EZoogiFloorState::Damaged ? 3 : 6;
		FRandomStream R(11);
		for (int32 i = 0; i < N && i < Cracks.Num(); ++i)
		{
			const float X = FMath::Lerp(X0 + 40.f, X1 - 40.f, R.FRand());
			const float Y = FMath::Lerp(Y0 + 40.f, Y1 - 40.f, R.FRand());
			float Z;
			if (FloorState == EZoogiFloorState::Damaged) Z = H(X, 0.5f, 28.f);
			else Z = (LipDir > 0.f ? X > XM : X < XM) ? FMath::Lerp(22.f, 70.f, FMath::Abs(X - XM) / (XM - X0)) : FMath::Lerp(0.5f, 22.f, 1.f - FMath::Abs(X - XM) / (XM - X0));
			Cracks[i]->SetWorldTransform(FTransform(FRotator(0.f, R.FRandRange(0.f, 180.f), 0.f), FVector(X, Y, Z + 1.5f), FVector(R.FRandRange(0.9f, 1.8f), 0.06f, 0.03f)));
			Cracks[i]->SetVisibility(true);
		}
	}
}

bool AZoogiComboArena::FloorImpact(const FVector& Where, const FVector& TravelDir, float Strength)
{
	if (FloorState == EZoogiFloorState::Cracked) return false;
	if (Clock - LastImpact < 0.6f) return false;
	LastImpact = Clock;
	if (FloorState == EZoogiFloorState::Normal && FMath::Abs(TravelDir.X) > 0.2f) LipDir = TravelDir.X >= 0.f ? 1.f : -1.f; // material is shoved forward
	FloorState = FloorState == EZoogiFloorState::Normal ? EZoogiFloorState::Damaged : EZoogiFloorState::Cracked;
	ApplyFloorState();
	const FVector C((SectionMin().X + SectionMax().X) * 0.5f, (SectionMin().Y + SectionMax().Y) * 0.5f, 10.f);
	if (bFrozenLayout)
	{
		AZoogiFX::Spawn(GetWorld(), C, ZoogiUtil::Hex(TEXT("#19C3E6")), 320.f, 0.7f, true, 9.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Blue"), Where, FRotator::ZeroRotator, 1.2f);
	}
	else
	{
		AZoogiFX::Spawn(GetWorld(), C, FLinearColor(1.f, 0.55f, 0.2f), 320.f, 0.7f, true, 9.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), Where, FRotator::ZeroRotator, 1.2f);
	}
	ZoogiUtil::PlaySfx(this, TEXT("HitCrack"), 1.f, 0.6f);
	UE_LOG(LogZoogi, Log, TEXT("Floor section impact (strength %.0f) -> %s (raised edge on the %s side)"), Strength, *FloorStateName(FloorState), LipDir > 0.f ? TEXT("+X") : TEXT("-X"));
	return true;
}

void AZoogiComboArena::ResonanceShift()
{
	if (bShifted) return;
	bShifted = true;
	ShiftAnim = 0.f;
	for (UStaticMeshComponent* B : Bumpers) B->SetMaterial(0, ZoogiUtil::GlowMID(this, FLinearColor(0.6f, 0.3f, 1.f), 6.f));
	for (const FVector2D& XY : BumperXY()) AZoogiFX::Spawn(GetWorld(), FVector(XY.X, XY.Y, 40.f), FLinearColor(0.6f, 0.3f, 1.f), 260.f, 0.8f, true, 10.f);
	UE_LOG(LogZoogi, Log, TEXT("RESONANCE SHIFT: bumpers swell (radius 70 -> 105 cm) and kick %.0f cm/s"), BumperKickSpeed());
}

void AZoogiComboArena::GateFlash()
{
	GatePulse = 1.f;
	AZoogiFX::Spawn(GetWorld(), GateReturn(), FLinearColor(0.3f, 1.f, 0.6f), 220.f, 0.7f, true, 10.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Green"), GateReturn());
}

void AZoogiComboArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	if (bShifted && ShiftAnim < 1.f)
	{
		ShiftAnim = FMath::Min(1.f, ShiftAnim + DeltaSeconds / 0.8f);
		const float R = FMath::Lerp(70.f, 105.f, ShiftAnim), Top = FMath::Lerp(90.f, 120.f, ShiftAnim);
		for (int32 i = 0; i < Bumpers.Num(); ++i)
		{
			UStaticMeshComponent* B = Bumpers[i];
			const FBoxSphereBounds Bo = B->GetStaticMesh()->GetBounds();
			const FVector S(R / Bo.BoxExtent.X, R / Bo.BoxExtent.Y, (Top + 10.f) / (2.f * Bo.BoxExtent.Z));
			const FVector2D XY = BumperXY()[i];
			B->SetWorldScale3D(S);
			B->SetWorldLocation(FVector(XY.X, XY.Y, Top - (Bo.Origin.Z + Bo.BoxExtent.Z) * S.Z));
			if (BumperCaps.IsValidIndex(i)) BumperCaps[i]->SetWorldTransform(FTransform(FRotator::ZeroRotator, FVector(XY.X, XY.Y, Top), FVector(R / 58.f, R / 58.f, 0.35f)));
		}
		// Frozen: the crystal bumpers swell (actor scale: the r70 kick post becomes r105, 90 -> 120 cm tall).
		for (AZoogiArenaPiece* P : IceBumpers) if (IsValid(P)) P->SetActorScale3D(FVector(R / 70.f, R / 70.f, Top / 90.f));
	}
	GatePulse = FMath::Max(0.f, GatePulse - DeltaSeconds * 0.8f);
	if (GateLight) GateLight->SetIntensity(40.f + 400.f * GatePulse + 10.f * FMath::Sin(Clock * 3.f));
}
