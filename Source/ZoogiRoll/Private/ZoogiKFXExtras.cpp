#include "ZoogiKFXExtras.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	struct FKFXDecalDef { const TCHAR* Mat; float X, Y, Half, Delay, Grow, Life; };
	struct FKFXLightDef { float R, G, B, Intensity, Radius, X, Y, Z, Delay, Life; };
	struct FKFXMeshDef { const TCHAR* Mesh; const TCHAR* Mat; float X, Y, Z, SX, SY, SZ, Yaw, Pitch, Roll, Delay, Grow, Life, Spin, Rise; int32 Burst; int32 FadeMat; };
	struct FKFXSystemDef { const TCHAR* System; float X, Y, Z, Delay, Life; };
	struct FKFXExtrasDef
	{
		const TCHAR* Prefab;
		const FKFXDecalDef* Decals; int32 NumDecals;
		const FKFXLightDef* Lights; int32 NumLights;
		const FKFXMeshDef* Meshes; int32 NumMeshes;
		const FKFXSystemDef* Systems; int32 NumSystems;
	};
#include "ZoogiKFXExtrasData.inl"

	const FKFXExtrasDef* FindDef(const FString& Prefab)
	{
		for (const FKFXExtrasDef& D : GKFXExtras) if (Prefab.Equals(D.Prefab)) return &D;
		return nullptr;
	}
	template <typename T>
	T* LoadCached(const TCHAR* Path)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (UObject* O = Cache.FindRef(Path).Get()) return Cast<T>(O);
		T* O = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn);
		if (O) Cache.Add(Path, O);
		return O;
	}
}

AZoogiKFXExtras::AZoogiKFXExtras()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AZoogiKFXExtras* AZoogiKFXExtras::Spawn(UWorld* World, const FString& Prefab, const FVector& Location, float InScale, float MaxLife, AActor* Attach)
{
	const FKFXExtrasDef* D = FindDef(Prefab);
	if (!World || !D) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiKFXExtras* X = World->SpawnActor<AZoogiKFXExtras>(AZoogiKFXExtras::StaticClass(), FTransform(Attach ? Attach->GetActorLocation() : Location), P);
	if (!X) return nullptr;
	X->Scale = InScale;
	X->Follow = Attach;
	const float S = InScale;
	float End = 0.f;
	for (int32 i = 0; i < D->NumDecals; ++i)
	{
		const FKFXDecalDef& Def = D->Decals[i];
		UMaterialInterface* M = LoadCached<UMaterialInterface>(Def.Mat);
		if (!M) continue;
		UDecalComponent* C = NewObject<UDecalComponent>(X);
		C->SetupAttachment(X->RootComponent);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, X);
		C->SetDecalMaterial(MID);
		C->DecalSize = FVector(120.f * S, Def.Half * S, Def.Half * S);
		C->SetRelativeLocation(FVector(Def.X * S, Def.Y * S, 0.f));
		C->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // project straight down
		C->SortOrder = i;
		C->RegisterComponent();
		FItem It; It.Kind = 0; It.Comp = C; It.MID = MID; It.Delay = Def.Delay; It.Grow = Def.Grow; It.Life = Def.Life; It.bFadeMat = true;
		X->Items.Add(It);
		End = FMath::Max(End, Def.Delay + Def.Life);
	}
	for (int32 i = 0; i < D->NumLights; ++i)
	{
		const FKFXLightDef& Def = D->Lights[i];
		UPointLightComponent* L = NewObject<UPointLightComponent>(X);
		L->SetupAttachment(X->RootComponent);
		L->SetUseInverseSquaredFalloff(false);
		L->SetLightFalloffExponent(2.f);
		L->SetCastShadows(false);
		L->SetLightColor(FLinearColor(Def.R, Def.G, Def.B));
		L->SetAttenuationRadius(Def.Radius * S);
		L->SetIntensity(0.f);
		L->SetRelativeLocation(FVector(Def.X, Def.Y, Def.Z) * S);
		L->RegisterComponent();
		FItem It; It.Kind = 1; It.Comp = L; It.Base = Def.Intensity; It.Delay = Def.Delay; It.Life = Def.Life; It.Grow = 0.08f;
		X->Items.Add(It);
		End = FMath::Max(End, Def.Delay + Def.Life);
	}
	for (int32 i = 0; i < D->NumMeshes; ++i)
	{
		const FKFXMeshDef& Def = D->Meshes[i];
		UStaticMesh* SM = LoadCached<UStaticMesh>(Def.Mesh);
		UMaterialInterface* M = LoadCached<UMaterialInterface>(Def.Mat);
		if (!SM) continue;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(X);
		C->SetupAttachment(X->RootComponent);
		C->SetStaticMesh(SM);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(!Def.FadeMat);
		C->SetGenerateOverlapEvents(false);
		UMaterialInstanceDynamic* MID = nullptr;
		if (M)
		{
			MID = UMaterialInstanceDynamic::Create(M, X);
			for (int32 k = 0; k < FMath::Max(1, SM->GetStaticMaterials().Num()); ++k) C->SetMaterial(k, MID);
		}
		C->SetVisibility(false);
		C->RegisterComponent();
		FItem It; It.Kind = 2; It.Comp = C; It.MID = MID;
		It.Pos = FVector(Def.X, Def.Y, Def.Z); It.Scale3 = FVector(Def.SX, Def.SY, Def.SZ); It.Rot = FRotator(Def.Pitch, Def.Yaw, Def.Roll);
		It.Delay = Def.Delay; It.Grow = Def.Grow; It.Life = Def.Life; It.Spin = Def.Spin; It.Rise = Def.Rise; It.bBurst = Def.Burst != 0; It.bFadeMat = Def.FadeMat != 0;
		X->Items.Add(It);
		End = FMath::Max(End, Def.Delay + Def.Life);
	}
	for (int32 i = 0; i < D->NumSystems; ++i)
	{
		// ring-shaped emitters (fire rings, flames on a rotating arm): one small Niagara system per point on the ring
		const FKFXSystemDef& Def = D->Systems[i];
		UNiagaraSystem* NS = LoadCached<UNiagaraSystem>(Def.System);
		if (!NS) continue;
		UNiagaraComponent* N = UNiagaraFunctionLibrary::SpawnSystemAttached(NS, X->RootComponent, NAME_None, FVector(Def.X, Def.Y, Def.Z) * S, FRotator::ZeroRotator, FVector(S), EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None, true);
		if (!N) continue;
		FItem It; It.Kind = 3; It.Comp = N; It.Delay = Def.Delay; It.Life = (MaxLife > 0.f ? MaxLife : Def.Life);
		X->Items.Add(It);
		End = FMath::Max(End, Def.Delay + It.Life + 1.5f);
	}
	X->EndAge = (MaxLife > 0.f ? FMath::Min(End, MaxLife + 1.5f) : FMath::Min(End, 3.1f)) + 0.05f;
	for (FItem& It : X->Items) X->Apply(It, 0.f);
	return X;
}

void AZoogiKFXExtras::Apply(FItem& It, float A)
{
	USceneComponent* C = It.Comp.Get();
	if (!C) return;
	const float L = A - It.Delay;
	if (It.Kind == 3)
	{
		if (UNiagaraComponent* N = Cast<UNiagaraComponent>(C)) { if (L >= It.Life) { if (N->IsActive()) N->Deactivate(); } }
		return;
	}
	const bool bOn = L >= 0.f && L <= It.Life;
	C->SetVisibility(bOn);
	if (!bOn) return;
	const float G = It.Grow > 0.f ? FMath::Clamp(L / It.Grow, 0.f, 1.f) : 1.f;
	const float GE = 1.f - FMath::Square(1.f - G);                 // ease out
	const float FadeT = FMath::Clamp((It.Life - L) / FMath::Max(0.05f, It.Life * 0.35f), 0.f, 1.f); // last 35% fades
	switch (It.Kind)
	{
	case 0:
		if (UMaterialInstanceDynamic* M = It.MID.Get()) { M->SetScalarParameterValue(TEXT("Fade"), FadeT); M->SetScalarParameterValue(TEXT("Reveal"), GE); }
		break;
	case 1:
		if (UPointLightComponent* P = Cast<UPointLightComponent>(C)) P->SetIntensity(It.Base * GE * FadeT);
		break;
	default:
	{
		FVector P = It.Pos * Scale;
		if (It.bBurst) P *= GE;                                           // debris flies out from the centre
		const float Sink = It.bFadeMat ? 0.f : (1.f - FadeT);            // opaque rocks sink back into the ground
		P.Z -= It.Rise * Scale * ((It.Rise > 0.f ? (1.f - GE) : 0.f) + Sink);
		C->SetRelativeLocation(P);
		FRotator R = It.Rot; R.Yaw += It.Spin * L;
		C->SetRelativeRotation(R);
		const FVector S3 = It.Scale3 * Scale * (It.bBurst ? 1.f : FMath::Max(0.02f, GE));
		C->SetRelativeScale3D(S3);
		if (UMaterialInstanceDynamic* M = It.MID.Get()) M->SetScalarParameterValue(TEXT("Fade"), FadeT);
	}
	}
}

void AZoogiKFXExtras::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Follow.IsValid()) SetActorLocation(Follow->GetActorLocation());
	for (FItem& It : Items) Apply(It, Age);
	if (Age >= EndAge) Destroy();
}
