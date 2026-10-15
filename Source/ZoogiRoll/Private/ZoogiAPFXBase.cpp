#include "ZoogiAPFXBase.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"

TOptional<FVector> AZoogiAPFXBase::ViewOverride;

AZoogiAPFXBase::AZoogiAPFXBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;   // archetypes play inside the Episode core's paused hold
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Quads = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Quads"));
	Quads->SetupAttachment(RootComponent);
	Quads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quads->SetCastShadow(false);
	Quads->bReceivesDecals = false;
	Quads->SetUsingAbsoluteLocation(true);
	Quads->SetUsingAbsoluteRotation(true);
	Quads->bUseAsyncCooking = true;
	Rng.Initialize(0x7A11);
}

void AZoogiAPFXBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying) return;
	// real, undilated frame time: slow-mo / pause holds never stretch the presentation
	const float Dt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.1f);
	Age += Dt;
	if (!TickFX(Dt, GetProgress())) { bPlaying = false; Destroy(); }
}

UObject* AZoogiAPFXBase::LoadAsset(const TCHAR* Path)
{
	static TMap<FString, TWeakObjectPtr<UObject>> Cache;
	FString P(Path);
	if (!P.StartsWith(TEXT("/"))) P = FString::Printf(TEXT("/Game/VFX/AwakenFX/Materials/%s.%s"), Path, Path);
	else if (!P.Contains(TEXT("."))) { FString L, R; P.Split(TEXT("/"), &L, &R, ESearchCase::IgnoreCase, ESearchDir::FromEnd); P = P + TEXT(".") + R; }
	if (UObject* O = Cache.FindRef(P).Get()) return O;
	UObject* O = LoadObject<UObject>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (O) Cache.Add(P, O);
	else UE_LOG(LogZoogi, Warning, TEXT("APFX: missing asset %s"), *P);
	return O;
}

UMaterialInstanceDynamic* AZoogiAPFXBase::MID(const TCHAR* Path)
{
	UMaterialInterface* M = Cast<UMaterialInterface>(LoadAsset(Path));
	if (!M) return nullptr;
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, this);
	MIDs.Add(D);
	return D;
}

UStaticMeshComponent* AZoogiAPFXBase::MakeMesh(const TCHAR* MeshPath, UMaterialInterface* Mat, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(Cast<UStaticMesh>(LoadAsset(MeshPath)));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(bShadow);
	C->bReceivesDecals = false;
	if (Mat && C->GetStaticMesh()) for (int32 i = 0; i < C->GetNumMaterials(); ++i) C->SetMaterial(i, Mat);
	C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
	C->SetVisibility(false);
	C->RegisterComponent();
	return C;
}

UDecalComponent* AZoogiAPFXBase::MakeDecal(UMaterialInterface* Mat, float Depth)
{
	UDecalComponent* D = NewObject<UDecalComponent>(this);
	D->SetupAttachment(RootComponent);
	D->SetDecalMaterial(Mat);
	D->DecalSize = FVector(Depth, 100.f, 100.f); // half extents; Y/Z resized through the scale (half size / 100)
	D->SetUsingAbsoluteLocation(true); D->SetUsingAbsoluteRotation(true); D->SetUsingAbsoluteScale(true);
	D->SetVisibility(false);
	D->RegisterComponent();
	return D;
}

UPointLightComponent* AZoogiAPFXBase::MakeLight(const FLinearColor& Color, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(RootComponent);
	L->SetUsingAbsoluteLocation(true);
	L->SetUseInverseSquaredFalloff(false);
	L->SetLightFalloffExponent(2.f);
	L->SetCastShadows(false);
	L->SetLightColor(Color);
	L->SetAttenuationRadius(Radius);
	L->SetIntensity(0.f);
	L->RegisterComponent();
	return L;
}

void AZoogiAPFXBase::SetDecal(UDecalComponent* D, const FVector& At, float Half, float Yaw)
{
	if (!D) return;
	D->SetVisibility(Half > 1.f);
	D->SetWorldLocationAndRotation(At, FRotator(-90.f, Yaw, 0.f));
	D->SetWorldScale3D(FVector(1.f, FMath::Max(Half, 1.f) / 100.f, FMath::Max(Half, 1.f) / 100.f));
}

void AZoogiAPFXBase::SetScalar(UPrimitiveComponent* C, FName Name, float V, int32 Slot)
{
	if (C) if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetMaterial(Slot))) M->SetScalarParameterValue(Name, V);
}

void AZoogiAPFXBase::SetDecalScalar(UDecalComponent* D, FName Name, float V)
{
	if (D) if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(D->GetDecalMaterial())) M->SetScalarParameterValue(Name, V);
}

FVector AZoogiAPFXBase::ViewPos() const
{
	if (ViewOverride.IsSet()) return ViewOverride.GetValue();
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)) return PCM->GetCameraLocation();
	return GetActorLocation() + FVector(-1500.f, 0.f, 900.f);
}

float AZoogiAPFXBase::FloorZ(const FVector& P, float Fallback) const
{
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(APFXFloor), false, this);
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(H, P + FVector(0, 0, 150.f), P - FVector(0, 0, 600.f), ECC_Visibility, Q))
	{
		// ignore Zoogis / orbs (Pawns and physics bodies): we want the stage floor under them
		if (H.GetComponent() && H.GetComponent()->IsSimulatingPhysics()) return Fallback;
		return H.ImpactPoint.Z;
	}
	return Fallback;
}

void AZoogiAPFXBase::Billboard(FBatch& B, const FVector& C, float W, float H, const FLinearColor& Color, float Rand, bool bUpright, float Roll) const
{
	const FVector ToCam = (ViewPos() - C).GetSafeNormal();
	FVector Right = FVector::CrossProduct(FVector::UpVector, ToCam).GetSafeNormal();
	if (Right.IsNearlyZero()) Right = FVector::RightVector;
	FVector Up = bUpright ? FVector::UpVector : FVector::CrossProduct(ToCam, Right).GetSafeNormal();
	if (Roll != 0.f) { const FVector R2 = Right.RotateAngleAxis(Roll, ToCam); Up = Up.RotateAngleAxis(Roll, ToCam); Right = R2; }
	const FVector B0 = bUpright ? C : C - Up * H * 0.5f;
	const int32 I = B.V.Num();
	B.V.Add(B0 - Right * W * 0.5f); B.V.Add(B0 + Right * W * 0.5f); B.V.Add(B0 + Right * W * 0.5f + Up * H); B.V.Add(B0 - Right * W * 0.5f + Up * H);
	B.UV0.Add(FVector2D(0, 1)); B.UV0.Add(FVector2D(1, 1)); B.UV0.Add(FVector2D(1, 0)); B.UV0.Add(FVector2D(0, 0));
	for (int32 k = 0; k < 4; ++k) { B.UV1.Add(FVector2D(Rand, 0)); B.Col.Add(Color); B.Nrm.Add(ToCam); }
	B.Tri.Append({ I, I + 2, I + 1, I, I + 3, I + 2 });
}

void AZoogiAPFXBase::FlatQuad(FBatch& B, const FVector& C, const FVector& N, float Half, const FLinearColor& Color, float Yaw, float Rand) const
{
	FVector A1 = FVector::CrossProduct(N, FMath::Abs(N.Z) > 0.9f ? FVector::ForwardVector : FVector::UpVector).GetSafeNormal();
	A1 = A1.RotateAngleAxis(Yaw, N);
	const FVector A2 = FVector::CrossProduct(N, A1);
	const int32 I = B.V.Num();
	B.V.Add(C - A1 * Half - A2 * Half); B.V.Add(C + A1 * Half - A2 * Half); B.V.Add(C + A1 * Half + A2 * Half); B.V.Add(C - A1 * Half + A2 * Half);
	B.UV0.Add(FVector2D(0, 1)); B.UV0.Add(FVector2D(1, 1)); B.UV0.Add(FVector2D(1, 0)); B.UV0.Add(FVector2D(0, 0));
	for (int32 k = 0; k < 4; ++k) { B.UV1.Add(FVector2D(Rand, 0)); B.Col.Add(Color); B.Nrm.Add(N); }
	B.Tri.Append({ I, I + 2, I + 1, I, I + 3, I + 2 });
}

void AZoogiAPFXBase::Ribbon(FBatch& B, const TArray<FVector>& Pts, float Width, const FLinearColor& Color, bool bFlat, const FVector& FlatN) const
{
	if (Pts.Num() < 2) return;
	const FVector Cam = ViewPos();
	const int32 I0 = B.V.Num();
	for (int32 i = 0; i < Pts.Num(); ++i)
	{
		const FVector T = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal();
		const FVector Facing = bFlat ? FlatN : (Cam - Pts[i]).GetSafeNormal();
		FVector Side = FVector::CrossProduct(T, Facing).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector::RightVector;
		const float Taper = (i == Pts.Num() - 1) ? 0.25f : 1.f;
		B.V.Add(Pts[i] - Side * Width * 0.5f * Taper); B.V.Add(Pts[i] + Side * Width * 0.5f * Taper);
		const float Vv = float(i) / float(Pts.Num() - 1);
		B.UV0.Add(FVector2D(0, Vv)); B.UV0.Add(FVector2D(1, Vv));
		B.UV1.Add(FVector2D(0, 0)); B.UV1.Add(FVector2D(0, 0));
		B.Col.Add(Color); B.Col.Add(Color); B.Nrm.Add(Facing); B.Nrm.Add(Facing);
		if (i > 0) { const int32 a = I0 + (i - 1) * 2; B.Tri.Append({ a, a + 2, a + 1, a + 1, a + 2, a + 3 }); }
	}
}

void AZoogiAPFXBase::Disc(FBatch& B, const FVector& C, float Radius, const FLinearColor& Center, const FLinearColor& Rim, int32 Segments) const
{
	const int32 I0 = B.V.Num();
	B.V.Add(C); B.UV0.Add(FVector2D(0.5f, 0.5f)); B.UV1.Add(FVector2D(0, 0)); B.Col.Add(Center); B.Nrm.Add(FVector::UpVector);
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A = 2.f * PI * i / Segments;
		B.V.Add(C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius);
		B.UV0.Add(FVector2D(0.5f + 0.5f * FMath::Cos(A), 0.5f + 0.5f * FMath::Sin(A))); B.UV1.Add(FVector2D(1, 0)); B.Col.Add(Rim); B.Nrm.Add(FVector::UpVector);
	}
	for (int32 i = 0; i < Segments; ++i)
	{
		const int32 a = I0 + 1 + i, b = I0 + 1 + (i + 1) % Segments;
		B.Tri.Append({ I0, a, b, I0, b, a });
	}
}

void AZoogiAPFXBase::DoubleSide(FBatch& B)
{
	const int32 N = B.Tri.Num();
	for (int32 i = 0; i + 2 < N; i += 3) B.Tri.Append({ B.Tri[i], B.Tri[i + 2], B.Tri[i + 1] });
}

void AZoogiAPFXBase::Commit(int32 Section, const FBatch& B)
{
	if (B.V.Num() == 0) { Quads->ClearMeshSection(Section); return; }
	static const TArray<FVector2D> NoUV;
	static const TArray<FProcMeshTangent> NoTan;
	Quads->CreateMeshSection_LinearColor(Section, B.V, B.Tri, B.Nrm, B.UV0, B.UV1, NoUV, NoUV, B.Col, NoTan, false);
}
