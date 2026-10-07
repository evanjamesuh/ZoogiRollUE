#include "ZoogiAwakenFX.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiGameMode.h"
#include "ZoogiArtifacts.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

TOptional<FVector> AZoogiAwakenFX::ViewOverride;

namespace
{
	// Colours from the demo frames Evan picked (and the KriptoFX material tints).
	const FLinearColor FireTint(1.0f, 0.42f, 0.10f);
	const FLinearColor ShockTint(0.55f, 0.33f, 1.0f);
	const FLinearColor VoidTint(0.62f, 0.28f, 1.0f);

	template <typename T>
	T* LoadFX(const TCHAR* Name)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		const FString Path = FString(Name).StartsWith(TEXT("/")) ? FString(Name) : FString::Printf(TEXT("/Game/VFX/AwakenFX/%s%s.%s"), FString(Name).StartsWith(TEXT("M")) ? TEXT("Materials/") : TEXT(""), Name, Name);
		if (UObject* O = Cache.FindRef(Path).Get()) return Cast<T>(O);
		T* O = LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (O) Cache.Add(Path, O);
		return O;
	}
	float Ease(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Square(1.f - X); }
	float EaseBack(float X) { X = FMath::Clamp(X, 0.f, 1.f); const float C = 1.9f; return 1.f + (C + 1.f) * FMath::Pow(X - 1.f, 3.f) + C * FMath::Square(X - 1.f); }
	void Burst(UWorld* W, const TCHAR* Sys, const FVector& P, float Scale)
	{
		if (UNiagaraSystem* NS = LoadFX<UNiagaraSystem>(Sys)) UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, NS, P, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::None);
	}
	void Sfx(UWorld* W, const TCHAR* Path, const FVector& P, float Vol, float Pitch = 1.f)
	{
		if (USoundBase* Snd = LoadFX<USoundBase>(Path)) UGameplayStatics::PlaySoundAtLocation(W, Snd, P, Vol, Pitch);
	}
}

AZoogiAwakenFX::AZoogiAwakenFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Quads = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Quads"));
	Quads->SetupAttachment(RootComponent);
	Quads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quads->SetCastShadow(false);
	Quads->bUseAsyncCooking = true;
}

void AZoogiAwakenFX::OnInfuse(UWorld* World, AZoogiBall* InBall, EZoogiArtifact Artifact)
{
	if (!World || !InBall || World->GetNetMode() == NM_DedicatedServer) return;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKFX"))) return;
	const FVector G = InBall->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 2.f);
	constexpr float ZoogiScale = 0.45f; // demo character 180 cm vs a 1 m Zoogi
	SpawnFX(World, EZoogiAwakenFX::LaunchRing, G, ZoogiScale, InBall, FVector::ZeroVector);
	if (Artifact == EZoogiArtifact::Storm) SpawnFX(World, EZoogiAwakenFX::ShockChain, G, ZoogiScale, InBall, FVector::ZeroVector);
	if (Artifact == EZoogiArtifact::Void)
	{
		// Rock goes to what the shot is lined up on (aim preview contact), else the free-aim / path end, else straight ahead.
		AActor* Tgt = nullptr;
		FVector Off(500.f, 0.f, 0.f);
		if (const AZoogiGameMode* GM = World->GetAuthGameMode<AZoogiGameMode>())
		{
			if (GM->Preview.bValid && GM->Preview.ContactActor.IsValid()) Tgt = GM->Preview.ContactActor.Get();
			else if (GM->Preview.bValid && GM->Preview.Path.Num()) Off = GM->Preview.Path.Last() - G;
			else if (GM->bFreeAim) Off = GM->FreeAimPoint - G;
		}
		Off.Z = 0.f;
		SpawnFX(World, EZoogiAwakenFX::VoidPortal, G, ZoogiScale, InBall, Off, Tgt);
	}
}

AZoogiAwakenFX* AZoogiAwakenFX::SpawnFX(UWorld* World, EZoogiAwakenFX InKind, const FVector& InGround, float InScale, AZoogiBall* InBall, const FVector& InParam, AActor* InTarget)
{
	if (!World) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiAwakenFX* X = World->SpawnActor<AZoogiAwakenFX>(AZoogiAwakenFX::StaticClass(), FTransform(InGround), P);
	if (!X) return nullptr;
	X->Kind = InKind; X->Ground = InGround; X->S = InScale; X->Ball = InBall; X->Param = InParam; X->TargetActor = InTarget;
	X->Rng.Initialize(FMath::Rand());
	X->Init();
	return X;
}

FVector AZoogiAwakenFX::ViewPos() const
{
	if (ViewOverride.IsSet()) return ViewOverride.GetValue();
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)) return PCM->GetCameraLocation();
	return Ground + FVector(-600.f, 0.f, 300.f);
}

UMaterialInstanceDynamic* AZoogiAwakenFX::MID(const TCHAR* Path)
{
	UMaterialInterface* M = LoadFX<UMaterialInterface>(Path);
	if (!M) { UE_LOG(LogZoogi, Warning, TEXT("AwakenFX: missing %s"), Path); return nullptr; }
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(M, this);
	MIDs.Add(D);
	return D;
}

UDecalComponent* AZoogiAwakenFX::MakeDecal(const TCHAR* Mat, float Half, float Depth)
{
	UDecalComponent* C = NewObject<UDecalComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetDecalMaterial(MID(Mat));
	C->DecalSize = FVector(Depth, 100.f, 100.f); // resized later through the component scale (Y/Z = half size / 100)
	C->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	C->SetRelativeScale3D(FVector(1.f, Half / 100.f, Half / 100.f));
	C->RegisterComponent();
	return C;
}

UPointLightComponent* AZoogiAwakenFX::MakeLight(const FLinearColor& Cl, float Radius)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(RootComponent);
	L->SetUseInverseSquaredFalloff(false);
	L->SetLightFalloffExponent(2.f);
	L->SetCastShadows(false);
	L->SetLightColor(Cl);
	L->SetAttenuationRadius(Radius);
	L->SetIntensity(0.f);
	L->RegisterComponent();
	return L;
}

UStaticMeshComponent* AZoogiAwakenFX::MakeCard(const TCHAR* Mat, const TCHAR* Mesh)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(LoadFX<UStaticMesh>(Mesh ? Mesh : TEXT("/Engine/BasicShapes/Plane.Plane")));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(Mesh != nullptr);
	C->bReceivesDecals = false;
	if (Mat) C->SetMaterial(0, MID(Mat));
	C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
	C->SetVisibility(false);
	C->RegisterComponent();
	return C;
}

void AZoogiAwakenFX::Init()
{
	switch (Kind)
	{
	case EZoogiAwakenFX::LaunchRing:
		Decal2 = MakeDecal(TEXT("MI_ZAFX_RingInner"), 100.f, 80.f * S + 40.f);
		Decal = MakeDecal(TEXT("MI_ZAFX_RingDecal"), 100.f, 80.f * S + 40.f);
		Decal->SortOrder = 1;
		Light = MakeLight(FireTint, 400.f);
		Light->SetRelativeLocation(FVector(0, 0, 70.f * S));
		Quads->SetMaterial(0, MID(TEXT("MI_ZAFX_Flame")));
		if (UNiagaraSystem* NS = LoadFX<UNiagaraSystem>(TEXT("NS_ZAFX_Embers")))
			Niagara = UNiagaraFunctionLibrary::SpawnSystemAttached(NS, RootComponent, NAME_None, FVector(0, 0, 5.f), FRotator::ZeroRotator, FVector(1.f), EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None, true);
		Power = Ball.IsValid() ? 0.f : Param.X;
		Sfx(GetWorld(), TEXT("/Game/VFX/KriptoFX/Effect4/Audio/S_FireExplosion2.S_FireExplosion2"), Ground, 0.35f, 1.1f);
		break;
	case EZoogiAwakenFX::ShockChain:
		Quads->SetMaterial(0, MID(TEXT("MI_ZAFX_Bolt")));
		Quads->SetMaterial(1, MID(TEXT("MI_ZAFX_Orb")));
		Light = MakeLight(ShockTint, 1000.f * S);
		for (int32 k = 0; k < 3; ++k) HitLights.Add(MakeLight(ShockTint, 450.f * S));
		Bolts.SetNum(3);
		for (int32 k = 0; k < 3; ++k) Bolts[k].NextMove = 0.f;
		if (AZoogiBall* B = Ball.Get()) B->Sphere->OnComponentHit.AddDynamic(this, &AZoogiAwakenFX::OnBallHit);
		break;
	case EZoogiAwakenFX::ShockHit:
		Quads->SetMaterial(0, MID(TEXT("MI_ZAFX_Bolt")));
		Quads->SetMaterial(1, MID(TEXT("MI_ZAFX_Flash")));
		Light = MakeLight(ShockTint, 750.f * S);
		Light->SetRelativeLocation(FVector(0, 0, 60.f * S));
		SplatAt(Ground, 1.7f, 2.5f);
		Burst(GetWorld(), TEXT("NS_ZAFX_Sparks"), Ground + FVector(0, 0, 30.f * S), S * 1.6f);
		ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.7f, 0.8f);
		break;
	case EZoogiAwakenFX::VoidPortal:
	{
		Card2 = MakeCard(TEXT("MI_ZAFX_PortalGlow"));
		Card0 = MakeCard(TEXT("MI_ZAFX_PortalInner"));
		Card1 = MakeCard(TEXT("MI_ZAFX_PortalRim"));
		Card0->SetTranslucentSortPriority(1); Card1->SetTranslucentSortPriority(2);
		Rock = MakeCard(TEXT("MI_ZAFX_Rock"), TEXT("/Game/VFX/KriptoFX/Effect5/Meshes/SM_Rock.SM_Rock"));
		for (int32 k = 0; k < 9; ++k) Chunks.Add(MakeCard(TEXT("MI_ZAFX_Rock"), TEXT("/Game/VFX/KriptoFX/Effect5/Meshes/SM_Rock.SM_Rock")));
		Decal2 = MakeDecal(TEXT("MI_ZAFX_Spill"), 380.f * S, 400.f * S);
		Decal2->SetUsingAbsoluteLocation(true);
		Light = MakeLight(VoidTint, 800.f * S);
		Light->SetUsingAbsoluteLocation(true);
		Light2 = MakeLight(VoidTint, 600.f * S);
		Light2->SetUsingAbsoluteLocation(true);
		Quads->SetMaterial(1, MID(TEXT("MI_ZAFX_Mote")));
		FVector Dir = (TargetActor.IsValid() ? TargetActor->GetActorLocation() - Ground : Param).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
		PortalN = (Dir - FVector(0, 0, 0.42f)).GetSafeNormal();
		PortalC = Ground - Dir * 70.f * S + FVector(0, 0, 300.f * S);
		Decal2->SetWorldLocation(FVector(PortalC.X, PortalC.Y, Ground.Z) + Dir * 120.f * S);
		Light->SetWorldLocation(PortalC - FVector(0, 0, 120.f * S) + Dir * 60.f * S);
		Sfx(GetWorld(), TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_PostEffect1.S_PostEffect1"), PortalC, 0.6f);
		break;
	}
	}
}

// ------------------------------------------------------------------ quad batch
void AZoogiAwakenFX::BeginQuads() { V.Reset(); Tri.Reset(); UV0.Reset(); UV1.Reset(); Col.Reset(); Nrm.Reset(); }

void AZoogiAwakenFX::Billboard(const FVector& C, float W, float H, const FLinearColor& Color, float Rand, bool bUpright)
{
	const FVector Base = C - GetActorLocation();
	const FVector ToCam = (ViewPos() - C).GetSafeNormal();
	FVector Right = FVector::CrossProduct(FVector::UpVector, ToCam).GetSafeNormal();
	if (Right.IsNearlyZero()) Right = FVector::RightVector;
	const FVector Up = bUpright ? FVector::UpVector : FVector::CrossProduct(ToCam, Right).GetSafeNormal();
	const FVector B0 = bUpright ? Base : Base - Up * H * 0.5f;
	const int32 I = V.Num();
	V.Add(B0 - Right * W * 0.5f); V.Add(B0 + Right * W * 0.5f); V.Add(B0 + Right * W * 0.5f + Up * H); V.Add(B0 - Right * W * 0.5f + Up * H);
	UV0.Add(FVector2D(0, 1)); UV0.Add(FVector2D(1, 1)); UV0.Add(FVector2D(1, 0)); UV0.Add(FVector2D(0, 0));
	for (int32 k = 0; k < 4; ++k) { UV1.Add(FVector2D(Rand, 0)); Col.Add(Color); Nrm.Add(ToCam); }
	Tri.Append({ I, I + 2, I + 1, I, I + 3, I + 2 });
}

void AZoogiAwakenFX::Ribbon(const TArray<FVector>& Pts, float Width, const FLinearColor& Color)
{
	if (Pts.Num() < 2) return;
	const FVector Cam = ViewPos();
	const int32 I0 = V.Num();
	for (int32 i = 0; i < Pts.Num(); ++i)
	{
		const FVector T = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal();
		const FVector ToCam = (Cam - Pts[i]).GetSafeNormal();
		FVector Side = FVector::CrossProduct(T, ToCam).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector::RightVector;
		const float Taper = (i == Pts.Num() - 1) ? 0.6f : 1.f;
		const FVector P = Pts[i] - GetActorLocation();
		V.Add(P - Side * Width * 0.5f * Taper); V.Add(P + Side * Width * 0.5f * Taper);
		const float Vv = float(i) / float(Pts.Num() - 1);
		UV0.Add(FVector2D(0, Vv)); UV0.Add(FVector2D(1, Vv));
		UV1.Add(FVector2D(0, 0)); UV1.Add(FVector2D(0, 0));
		Col.Add(Color); Col.Add(Color); Nrm.Add(ToCam); Nrm.Add(ToCam);
		if (i > 0) { const int32 a = I0 + (i - 1) * 2; Tri.Append({ a, a + 2, a + 1, a + 1, a + 2, a + 3 }); }
	}
}

void AZoogiAwakenFX::EndQuads(int32 Section)
{
	if (V.Num() == 0) { Quads->ClearMeshSection(Section); return; }
	static const TArray<FVector2D> NoUV;
	static const TArray<FProcMeshTangent> NoTan;
	Quads->CreateMeshSection_LinearColor(Section, V, Tri, Nrm, UV0, UV1, NoUV, NoUV, Col, NoTan, false);
}

void AZoogiAwakenFX::SplatAt(const FVector& P, float Size, float Glow)
{
	UDecalComponent* C = nullptr;
	if (Splats.Num() >= 10)
	{
		int32 Old = 0;
		for (int32 i = 1; i < SplatBorn.Num(); ++i) if (SplatBorn[i] < SplatBorn[Old]) Old = i;
		C = Splats[Old]; SplatBorn[Old] = Age;
	}
	else
	{
		C = NewObject<UDecalComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true);
		C->SetDecalMaterial(MID(TEXT("MI_ZAFX_Splat")));
		C->DecalSize = FVector(60.f * S + 30.f, 100.f, 100.f);
		C->RegisterComponent();
		Splats.Add(C); SplatBorn.Add(Age);
	}
	const float H = 95.f * S * Size;
	C->SetWorldLocationAndRotation(P, FRotator(-90.f, Rng.FRandRange(0.f, 360.f), 0.f));
	C->SetWorldScale3D(FVector(1.f, H / 100.f, H / 100.f));
	if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(C->GetDecalMaterial())) { M->SetScalarParameterValue(TEXT("Glow"), Glow); M->SetScalarParameterValue(TEXT("Fade"), 1.f); }
}

// ------------------------------------------------------------------ tick
void AZoogiAwakenFX::Finish(float FadeTime)
{
	if (EndAt >= 0.f) return;
	EndAt = Age;
	Param.Z = FadeTime; // reuse: fade length once finished
	if (Niagara) Niagara->Deactivate();
	if (Ball.IsValid() && Kind == EZoogiAwakenFX::ShockChain) Ball->Sphere->OnComponentHit.RemoveDynamic(this, &AZoogiAwakenFX::OnBallHit);
}

void AZoogiAwakenFX::OnBallHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (EndAt >= 0.f || Kind != EZoogiAwakenFX::ShockChain) return;
	if (Hit.ImpactNormal.Z > 0.65f) return; // rolling on the floor is not a collision
	const FVector P(Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Ground.Z);
	SpawnFX(GetWorld(), EZoogiAwakenFX::ShockHit, P, S, nullptr, FVector::ZeroVector);
	Finish(0.15f);
}

void AZoogiAwakenFX::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	float Fade = 1.f;
	if (EndAt >= 0.f)
	{
		const float FL = FMath::Max(0.05f, Param.Z);
		Fade = 1.f - FMath::Clamp((Age - EndAt) / FL, 0.f, 1.f);
		const float Linger = Kind == EZoogiAwakenFX::ShockChain ? 2.8f : 1.2f; // scorch splats cool down after the lightning stops
		if (Age - EndAt > FL + Linger) { Destroy(); return; }
	}
	switch (Kind)
	{
	case EZoogiAwakenFX::LaunchRing: TickRing(Dt, Fade); break;
	case EZoogiAwakenFX::ShockChain: TickShock(Dt, Fade); break;
	case EZoogiAwakenFX::ShockHit: TickShockHit(Dt); break;
	case EZoogiAwakenFX::VoidPortal: TickVoid(Dt); break;
	}
}

void AZoogiAwakenFX::TickRing(float Dt, float Fade)
{
	// Launch power (0..1): the aim preview's launch fraction, read-only; preview captures ramp Param.X -> Param.Y over Param.Z s.
	float Want = Power;
	if (AZoogiBall* B = Ball.Get())
	{
		const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		if (!GM || GM->Phase != EZoogiPhase::Aiming || GM->GetActiveBall() != B || B->bOut) Finish(0.4f);
		else if (GM->Preview.bValid) Want = FMath::Clamp(GM->Preview.Fraction, 0.f, 1.f);
		Ground = B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 2.f);
		SetActorLocation(Ground);
	}
	else if (EndAt < 0.f) Want = FMath::Lerp(Param.X, Param.Y, Param.Z > 0.f ? FMath::Clamp(Age / Param.Z, 0.f, 1.f) : 1.f);
	Power = FMath::FInterpTo(Power, Want, Dt, 6.f);

	const float Sweep = FMath::Clamp(Age / 0.45f, 0.f, 1.f);      // the ring draws itself round (demo 1:00.8 - 1:01.3)
	const float Ignite = Ease(Age / 0.7f);
	const float R = S * FMath::Lerp(190.f, 325.f, Power);           // demo: ~6.5 m across around a 1.8 m character
	const float I = FMath::Lerp(0.45f, 1.f, Power) * Fade;
	const float Half = R / 0.86f;                                   // ring line sits at 0.86 of the texture radius
	if (Decal)
	{
		Decal->SetRelativeScale3D(FVector(1.f, Half / 100.f, Half / 100.f));
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial()))
		{
			M->SetScalarParameterValue(TEXT("Reveal"), Sweep);
			M->SetScalarParameterValue(TEXT("Fade"), I * (0.85f + 0.15f * FMath::Sin(Age * 23.f) * FMath::Sin(Age * 9.f)));
		}
	}
	if (Decal2)
	{
		Decal2->SetRelativeScale3D(FVector(1.f, Half / 100.f, Half / 100.f));
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Decal2->GetDecalMaterial())) M->SetScalarParameterValue(TEXT("Fade"), I * Ignite);
	}
	if (Light)
	{
		Light->SetAttenuationRadius(R * 2.3f);
		Light->SetIntensity(28.f * I * Ignite * (0.9f + 0.1f * FMath::Sin(Age * 17.f)));
	}
	if (Niagara) Niagara->SetRelativeScale3D(FVector(R / 100.f, R / 100.f, 1.f));

	// flame licks round the ring (camera-facing, base on the floor), only where the sweep has passed
	BeginQuads();
	constexpr int32 N = 110;
	for (int32 k = 0; k < N; ++k)
	{
		FRandomStream FR(k * 7919 + 13);
		const float A01 = (k + FR.FRandRange(-0.35f, 0.35f)) / N;
		if (A01 > Sweep) continue;
		const float A = A01 * 2.f * PI;
		const float Ph = FR.FRand() * 20.f;
		const float Fl = 0.72f + 0.28f * FMath::Sin(Age * (9.f + FR.FRand() * 6.f) + Ph);
		const float Born = FMath::Clamp((Sweep - A01) * 6.f, 0.f, 1.f);
		const float H = S * FR.FRandRange(30.f, 85.f) * (0.55f + 0.45f * Power) * Fl * Born;
		const FVector P = Ground + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R * FR.FRandRange(0.985f, 1.03f) + FVector(0, 0, -2.f);
		Billboard(P, H * 0.8f, H, FLinearColor(I, I, I, 1.f), FR.FRand(), true);
	}
	EndQuads(0);
	if (EndAt >= 0.f && Fade <= 0.f) { BeginQuads(); EndQuads(0); }
}

void AZoogiAwakenFX::TickShock(float Dt, float Fade)
{
	AZoogiBall* B = Ball.Get();
	if (B)
	{
		Ground = B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 2.f);
		SetActorLocation(Ground);
		const AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		const bool bTurnOver = !GM || B->bOut || (GM->GetActiveBall() != B && GM->Phase == EZoogiPhase::Aiming);
		if (bTurnOver || Age > 40.f) Finish(0.25f);
	}
	else if (Param.X > 0.f && Age >= Param.X && EndAt < 0.f)
	{
		SpawnFX(GetWorld(), EZoogiAwakenFX::ShockHit, Ground + FVector(120.f * S, 0, 0), S, nullptr, FVector::ZeroVector);
		Finish(0.15f);
	}
	const FVector Src = Ground + FVector(0, 0, B ? ZoogiConst::ZoogiRadius * 2.f + 45.f : 230.f * S);
	const float Run = EndAt < 0.f ? 1.f : Fade;

	// re-aim each bolt now and then (new scorch + spark burst), re-strike the jagged paths ~18 times a second
	for (int32 k = 0; k < Bolts.Num() && EndAt < 0.f; ++k)
	{
		FBolt& Bo = Bolts[k];
		const bool bTooFar = FVector::Dist2D(Bo.End, Ground) > 480.f * S * 1.4f;
		if (Age >= Bo.NextMove || bTooFar)
		{
			const float A = FMath::DegreesToRadians(k * 120.f + Rng.FRandRange(-40.f, 40.f) + Age * 25.f);
			Bo.End = Ground + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * S * Rng.FRandRange(280.f, 460.f);
			Bo.NextMove = Age + Rng.FRandRange(0.8f, 1.4f);
			SplatAt(Bo.End, Rng.FRandRange(1.2f, 1.6f), 1.6f);
			Burst(GetWorld(), TEXT("NS_ZAFX_Sparks"), Bo.End + FVector(0, 0, 10.f), S);
			if (B && k == 0) ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.12f, Rng.FRandRange(1.2f, 1.6f));
		}
	}
	if (Age >= NextStrike) { NextStrike = Age + 0.055f; ++StrikeSeed; for (FBolt& Bo : Bolts) Bo.Flick = Rng.FRand() < 0.08f ? 0.f : Rng.FRandRange(0.7f, 1.15f); }

	BeginQuads();
	for (int32 k = 0; k < Bolts.Num() && Run > 0.f; ++k)
	{
		const FBolt& Bo = Bolts[k];
		FRandomStream R(StrikeSeed * 131 + k * 17);
		const FVector D = Bo.End - Src;
		const float Len = D.Size();
		const FVector Fw = D / FMath::Max(1.f, Len);
		FVector Side = FVector::CrossProduct(Fw, FVector::UpVector).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector::RightVector;
		const FVector Up2 = FVector::CrossProduct(Side, Fw);
		TArray<FVector> Pts;
		constexpr int32 NP = 30;
		float O1 = 0.f, O2 = 0.f;
		const float Arc = 0.30f * Len + 25.f * S;
		for (int32 i = 0; i <= NP; ++i)
		{
			const float T = float(i) / NP;
			const float Env = FMath::Sin(T * PI);
			O1 = O1 * 0.45f + R.FRandRange(-1.f, 1.f); O2 = O2 * 0.45f + R.FRandRange(-1.f, 1.f);
			Pts.Add(Src + D * T + FVector(0, 0, Arc * 4.f * T * (1.f - T)) + (Side * O1 + Up2 * O2) * 0.045f * Len * FMath::Max(Env, 0.15f) * (i == 0 || i == NP ? 0.f : 1.f));
		}
		const float Iv = Bo.Flick * Run;
		Ribbon(Pts, 130.f * S, FLinearColor(0.22f * Iv, 0.22f * Iv, 0.22f * Iv, 1.f)); // soft glow sheath (the demo's bloom)
		Ribbon(Pts, 42.f * S, FLinearColor(Iv, Iv, Iv, 1.f));
		for (int32 b = 0; b < 3; ++b)
		{
			const int32 i0 = R.RandRange(5, NP - 6);
			FVector Dir = (Pts[i0 + 1] - Pts[i0]).GetSafeNormal().RotateAngleAxis(R.FRandRange(-55.f, 55.f), FVector::UpVector);
			Dir = (Dir + FVector(0, 0, -0.35f)).GetSafeNormal();
			const float BL = Len * R.FRandRange(0.12f, 0.3f);
			TArray<FVector> Br; Br.Add(Pts[i0]);
			FVector P = Pts[i0];
			for (int32 i = 1; i <= 7; ++i) { P += (Dir + R.GetUnitVector() * 0.55f).GetSafeNormal() * BL / 7.f; Br.Add(P); }
			Ribbon(Br, 15.f * S, FLinearColor(Iv * 0.7f, Iv * 0.7f, Iv * 0.7f, 1.f));
		}
		if (UPointLightComponent* L = HitLights.IsValidIndex(k) ? HitLights[k].Get() : nullptr)
		{
			L->SetWorldLocation(Bo.End + FVector(0, 0, 45.f * S));
			L->SetIntensity(20.f * Iv);
		}
	}
	EndQuads(0);
	BeginQuads();
	if (Run > 0.f)
	{
		const float Pulse = 0.8f + 0.2f * FMath::Sin(Age * 31.f);
		Billboard(Src, 120.f * S * Pulse, 120.f * S * Pulse, FLinearColor(Run, Run, Run, 1.f), 0.f, false);
		Billboard(Src, 45.f * S, 45.f * S, FLinearColor(2.f * Run, 2.f * Run, 2.f * Run, 1.f), 0.5f, false);
	}
	EndQuads(1);
	if (Light) { Light->SetWorldLocation(Src); Light->SetIntensity(35.f * Run * (0.85f + 0.15f * FMath::Sin(Age * 40.f))); }
	for (int32 i = 0; i < Splats.Num(); ++i)
	{
		const float A = Age - SplatBorn[i];
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Splats[i]->GetDecalMaterial()))
		{
			M->SetScalarParameterValue(TEXT("Fade"), 1.f - FMath::Clamp((A - 0.8f) / 2.f, 0.f, 1.f));
			M->SetScalarParameterValue(TEXT("Glow"), 1.6f * FMath::Exp(-A * 2.2f) * Run + 0.15f);
		}
	}
}

void AZoogiAwakenFX::TickShockHit(float Dt)
{
	if (EndAt < 0.f && Age > 0.5f) Finish(0.1f);
	const float K = FMath::Exp(-Age * 6.f);
	if (Light) Light->SetIntensity(30.f * K);
	const FVector C = Ground + FVector(0, 0, 40.f * S);
	if (Age >= NextStrike) { NextStrike = Age + 0.05f; ++StrikeSeed; }
	BeginQuads();
	if (Age < 0.45f)
	{
		for (int32 k = 0; k < 6; ++k)
		{
			FRandomStream R(StrikeSeed * 97 + k);
			FRandomStream FixedR(k * 31 + 5);
			const float A = FixedR.FRandRange(0.f, 2.f * PI);
			const FVector E = Ground + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * S * FixedR.FRandRange(90.f, 210.f);
			TArray<FVector> Pts;
			for (int32 i = 0; i <= 10; ++i)
			{
				const float T = i / 10.f;
				Pts.Add(FMath::Lerp(C, E, T) + FVector(0, 0, 40.f * S * 4.f * T * (1 - T)) + (i > 0 && i < 10 ? R.GetUnitVector() * 14.f * S : FVector::ZeroVector));
			}
			const float Iv = (1.f - Age / 0.45f) * R.FRandRange(0.6f, 1.2f);
			Ribbon(Pts, 22.f * S, FLinearColor(Iv, Iv, Iv, 1.f));
		}
	}
	EndQuads(0);
	BeginQuads();
	if (Age < 0.35f) { const float F = 1.f - Age / 0.35f; Billboard(C, 300.f * S * (0.6f + Age), 300.f * S * (0.6f + Age), FLinearColor(F, F, F, 1.f), 0.f, false); }
	EndQuads(1);
	for (int32 i = 0; i < Splats.Num(); ++i)
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Splats[i]->GetDecalMaterial()))
		{
			M->SetScalarParameterValue(TEXT("Fade"), 1.f - FMath::Clamp((Age - 1.0f) / 2.f, 0.f, 1.f));
			M->SetScalarParameterValue(TEXT("Glow"), 2.5f * FMath::Exp(-Age * 1.8f) + 0.15f);
		}
}

void AZoogiAwakenFX::TickVoid(float Dt)
{
	// timeline (demo 1:18.1 ->): portal opens 0-0.5 s, rock pushes out 0.35-0.85 s, flies to the target, impact; then the
	// portal stays open pulling orbs in (PullTime), and closes in 0.35 s.
	constexpr float PullTime = 1.4f, PullRadius = 650.f, PullAccel = 1100.f;
	const float OpenK = EaseBack(Age / 0.5f);
	const float CloseAt = HitAt >= 0.f ? HitAt + PullTime + 0.1f : 99.f;
	const float CloseK = 1.f - FMath::Clamp((Age - CloseAt) / 0.35f, 0.f, 1.f);
	const float K = FMath::Max(0.f, OpenK) * CloseK;
	if (Age > CloseAt + 0.35f && EndAt < 0.f) Finish(0.05f);

	const float D = 235.f * S; // ring diameter ~2.35 m in the demo
	const FRotator Rot = FRotationMatrix::MakeFromZ(PortalN).Rotator();
	const float Wob = 1.f + 0.03f * FMath::Sin(Age * 7.f);
	if (Card0) { Card0->SetVisibility(K > 0.01f); Card0->SetWorldLocationAndRotation(PortalC, Rot); Card0->SetWorldScale3D(FVector(D / 0.8f / 100.f * K)); }
	if (Card1) { Card1->SetVisibility(K > 0.01f); Card1->SetWorldLocationAndRotation(PortalC + PortalN * 2.f, Rot); Card1->SetWorldScale3D(FVector(D / 0.8f / 100.f * K * Wob, D / 0.8f / 100.f * K, 1.f)); }
	if (Card2) { Card2->SetVisibility(K > 0.01f); Card2->SetWorldLocationAndRotation(PortalC - PortalN * 2.f, Rot); Card2->SetWorldScale3D(FVector(D * 1.9f / 100.f * K)); }
	if (Light) Light->SetIntensity(25.f * K);
	if (Decal2) if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Decal2->GetDecalMaterial())) M->SetScalarParameterValue(TEXT("Fade"), K);

	// rock
	const FVector Out = PortalC + PortalN * 95.f * S;
	FVector To = TargetActor.IsValid() ? TargetActor->GetActorLocation() : Ground + Param;
	if (!TargetActor.IsValid()) To.Z = Ground.Z + 55.f * S;
	const float FlyT = FMath::Clamp(FVector::Dist(Out, To) / 1500.f, 0.35f, 0.75f);
	const float RockSize = 150.f * S;
	float RockScale = 0.f;
	FVector RockP = PortalC;
	FRotator RockR(Age * 40.f, Age * 70.f, 0.f);
	if (Rock && Rock->GetStaticMesh())
	{
		const FBoxSphereBounds Bd = Rock->GetStaticMesh()->GetBounds();
		const float Unit = RockSize / FMath::Max(1.f, 2.f * Bd.BoxExtent.GetMax());
		if (Age >= 0.35f && Age < 0.85f)
		{
			const float U = Ease((Age - 0.35f) / 0.5f);
			RockP = FMath::Lerp(PortalC - PortalN * 30.f * S, Out, U); RockScale = Unit * FMath::Lerp(0.45f, 1.f, U);
		}
		else if (Age >= 0.85f && HitAt < 0.f)
		{
			const float U = FMath::Clamp((Age - 0.85f) / FlyT, 0.f, 1.f);
			RockP = FMath::Lerp(Out, To, U) + FVector(0, 0, 60.f * S * 4.f * U * (1.f - U)); RockScale = Unit;
			RockR = FRotator(Age * 220.f, Age * 160.f, 0.f);
			if (U >= 1.f)
			{
				HitAt = Age; RockTo = To;
				Burst(GetWorld(), TEXT("NS_ZAFX_Sparks"), To, S * 1.5f);
				Sfx(GetWorld(), TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_RockImpact.S_RockImpact"), To, 0.8f);
				ChunkVel.SetNum(Chunks.Num());
				for (int32 c = 0; c < Chunks.Num(); ++c)
				{
					const FVector Dir = (Rng.GetUnitVector() * FVector(1, 1, 0.3f) + FVector(0, 0, 0.8f)).GetSafeNormal();
					ChunkVel[c] = Dir * Rng.FRandRange(250.f, 600.f) * S / 0.45f * 0.45f;
					Chunks[c]->SetWorldLocation(To);
					Chunks[c]->SetWorldScale3D(FVector(Unit * Rng.FRandRange(0.14f, 0.3f)));
					Chunks[c]->SetWorldRotation(FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f));
					Chunks[c]->SetVisibility(true);
				}
			}
		}
		if (HitAt >= 0.f)
		{
			const float A = Age - HitAt;
			RockP = RockTo + FVector(0, 0, FMath::Max(0.f, 40.f * S * FMath::Sin(A * 6.f) * FMath::Exp(-A * 4.f)));
			RockScale = Unit * (1.f - FMath::Clamp((A - 0.7f) / 0.6f, 0.f, 1.f));
		}
		Rock->SetVisibility(RockScale > 0.01f);
		Rock->SetWorldLocationAndRotation(RockP, RockR);
		Rock->SetWorldScale3D(FVector(FMath::Max(RockScale, 0.001f)));
	}
	// impact debris, flash light
	if (HitAt >= 0.f)
	{
		const float A = Age - HitAt;
		for (int32 c = 0; c < Chunks.Num() && c < ChunkVel.Num(); ++c)
		{
			ChunkVel[c].Z -= 980.f * Dt;
			FVector P = Chunks[c]->GetComponentLocation() + ChunkVel[c] * Dt;
			if (P.Z < Ground.Z + 5.f) { P.Z = Ground.Z + 5.f; ChunkVel[c] *= FVector(0.6f, 0.6f, -0.3f); }
			Chunks[c]->SetWorldLocation(P);
			Chunks[c]->AddWorldRotation(FRotator(200.f * Dt, 150.f * Dt, 0.f));
		}
		if (Light2) { Light2->SetWorldLocation(RockTo + FVector(0, 0, 60.f * S)); Light2->SetIntensity(25.f * FMath::Exp(-A * 4.f)); }

		// suction: nearby orbs drawn toward the portal's floor point while it stays open
		if (A < PullTime && GetWorld()->GetAuthGameMode<AZoogiGameMode>())
		{
			const FVector Sink(PortalC.X, PortalC.Y, Ground.Z);
			for (TActorIterator<AZoogiOrb> It(GetWorld()); It; ++It)
			{
				AZoogiOrb* O = *It;
				if (!O || O->bOut || !O->Sphere) continue;
				const FVector Dv = Sink - O->OrbLocation();
				const float Dist = FVector2D(Dv.X, Dv.Y).Size();
				if (Dist > PullRadius || Dist < 40.f) continue;
				const float Kp = 0.3f + 0.7f * (1.f - Dist / PullRadius);
				O->Sphere->WakeRigidBody();
				const FVector Vel = O->Sphere->GetPhysicsLinearVelocity();
				O->Sphere->SetPhysicsLinearVelocity(Vel + FVector(Dv.X, Dv.Y, 0.f).GetSafeNormal() * PullAccel * Kp * Dt);
			}
		}
	}
	// suction motes spiralling into the portal (and a few drifting sparkles while it opens)
	const bool bPull = HitAt >= 0.f && Age - HitAt < PullTime;
	const float Rate = bPull ? 45.f : (K > 0.5f ? 10.f : 0.f);
	MoteAcc += Rate * Dt;
	while (MoteAcc >= 1.f) { MoteAcc -= 1.f; Motes.Add({ Age, (float)Rng.FRandRange(0.f, 2.f * PI), (float)Rng.FRandRange(1.1f, 2.2f), (float)Rng.FRandRange(-0.3f, 0.3f) }); }
	BeginQuads();
	const FVector Ax1 = FVector::CrossProduct(PortalN, FVector::UpVector).GetSafeNormal();
	const FVector Ax2 = FVector::CrossProduct(Ax1, PortalN);
	for (int32 i = Motes.Num() - 1; i >= 0; --i)
	{
		const float T = (Age - Motes[i].Born) / 0.7f;
		if (T >= 1.f || K <= 0.01f) { Motes.RemoveAtSwap(i); continue; }
		const float Rr = Motes[i].Rad * (1.f - Ease(T)) * D * 0.5f;
		const float An = Motes[i].Ang + T * 3.f;
		const FVector P = PortalC + (Ax1 * FMath::Cos(An) + Ax2 * FMath::Sin(An)) * Rr + PortalN * (Motes[i].H * D * (1.f - T) + 10.f);
		const float B = FMath::Sin(T * PI) * 1.5f;
		Billboard(P, 18.f * S, 18.f * S, FLinearColor(B, B, B, 1.f), 0.f, false);
	}
	EndQuads(1);
}
