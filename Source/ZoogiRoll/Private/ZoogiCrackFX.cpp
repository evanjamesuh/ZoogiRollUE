// Oct 22 (Evan): glowing ground cracks / craters. See ZoogiCrackFX.h.
#include "ZoogiCrackFX.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiGameMode.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	struct FCrackTune
	{
		bool bEnabled = true, bLight = true, bShot = false;
		bool bAwakenCracks = false; // Oct 22 (Evan): no element cracks per Awaken for now (craters only)
		bool bSeams = false;        // Oct 22 (Evan): no glowing crack lines in the crater material (SeamSharp pushed to SeamOffSharp)
		float ImpactMin = 12000.f;  // Zoogi-vs-Zoogi impulse for an impact crater (same bar as the floor scars)
		float LandSpeed = 1300.f;   // cm/s downward at floor contact for a landing crater (~0.85 m drop)
		float Life = 3.0f;          // s (Crystal Collapse x1.4)
		int32 MaxLive = 6;          // oldest is retired early beyond this (mobile budget)
		FCrackTune()
		{
			const TCHAR* S = TEXT("ZoogiRoll.Cracks");
			if (GConfig)
			{
				GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
				GConfig->GetBool(S, TEXT("bLight"), bLight, GGameIni);
				GConfig->GetFloat(S, TEXT("ImpactMin"), ImpactMin, GGameIni);
				GConfig->GetFloat(S, TEXT("LandSpeed"), LandSpeed, GGameIni);
				GConfig->GetFloat(S, TEXT("Life"), Life, GGameIni);
				GConfig->GetInt(S, TEXT("MaxLive"), MaxLive, GGameIni);
				GConfig->GetBool(S, TEXT("bAwakenCracks"), bAwakenCracks, GGameIni);
				GConfig->GetBool(S, TEXT("bSeams"), bSeams, GGameIni);
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCracks")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKFX"))) bEnabled = false;
			bShot = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrackShot"));
			Life = FMath::Clamp(Life, 1.f, 8.f); MaxLive = FMath::Clamp(MaxLive, 1, 24);
		}
	};
	constexpr float ZD = 2.f * ZoogiConst::ZoogiRadius; // Zoogi diameter (100 cm)
	const FCrackTune& CrackTune() { static FCrackTune T; return T; }

	struct FPal { FLinearColor Crack, Inner, Stone; float Emissive; };
	FPal Palette(EZoogiCrackStyle S) // keep in sync with Scripts/cracks/cracks_fx.py STYLES
	{
		switch (S)
		{
		case EZoogiCrackStyle::Lava:    return { FLinearColor(1.0f, 0.30f, 0.04f), FLinearColor(1.0f, 0.12f, 0.02f), FLinearColor(0.15f, 0.13f, 0.12f), 14.f };
		case EZoogiCrackStyle::Ice:     return { FLinearColor(0.10f, 0.60f, 1.0f), FLinearColor(0.20f, 0.70f, 1.0f), FLinearColor(0.10f, 0.18f, 0.30f), 10.f };
		case EZoogiCrackStyle::Fire:    return { FLinearColor(1.0f, 0.22f, 0.03f), FLinearColor(1.0f, 0.45f, 0.05f), FLinearColor(0.19f, 0.16f, 0.14f), 16.f };
		case EZoogiCrackStyle::Frost:   return { FLinearColor(0.30f, 0.75f, 1.0f), FLinearColor(0.50f, 0.85f, 1.0f), FLinearColor(0.14f, 0.22f, 0.34f), 12.f };
		case EZoogiCrackStyle::Shock:   return { FLinearColor(0.25f, 0.55f, 1.0f), FLinearColor(0.70f, 0.85f, 1.0f), FLinearColor(0.21f, 0.23f, 0.29f), 18.f };
		case EZoogiCrackStyle::Void:    return { FLinearColor(0.60f, 0.18f, 1.0f), FLinearColor(0.30f, 0.05f, 0.70f), FLinearColor(0.14f, 0.11f, 0.19f), 14.f };
		case EZoogiCrackStyle::Titan:   return { FLinearColor(1.0f, 0.48f, 0.12f), FLinearColor(1.0f, 0.30f, 0.05f), FLinearColor(0.50f, 0.50f, 0.52f), 10.f };
		case EZoogiCrackStyle::Crystal: return { FLinearColor(0.30f, 0.95f, 1.0f), FLinearColor(0.65f, 0.25f, 1.0f), FLinearColor(0.24f, 0.21f, 0.36f), 16.f };
		case EZoogiCrackStyle::Toxic:   return { FLinearColor(0.35f, 1.0f, 0.25f), FLinearColor(0.65f, 1.0f, 0.20f), FLinearColor(0.20f, 0.24f, 0.18f), 12.f };
		default:                        return { FLinearColor(1.0f, 0.55f, 0.20f), FLinearColor(1.0f, 0.40f, 0.10f), FLinearColor(0.40f, 0.38f, 0.35f), 8.f };
		}
	}
	TArray<TWeakObjectPtr<AZoogiCrackFX>>& LiveList() { static TArray<TWeakObjectPtr<AZoogiCrackFX>> L; return L; }
	template<class T> T* LoadCrack(const FString& Name) { return LoadObject<T>(nullptr, *FString::Printf(TEXT("/Game/VFX/Cracks/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet); }
	float CrackEase(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - (1.f - X) * (1.f - X) * (1.f - X); }
}

bool AZoogiCrackFX::IsEnabled() { return CrackTune().bEnabled; }

const TCHAR* AZoogiCrackFX::StyleName(EZoogiCrackStyle S)
{
	static const TCHAR* N[] = { TEXT("Lava"), TEXT("Ice"), TEXT("Warm"), TEXT("Fire"), TEXT("Frost"), TEXT("Shock"), TEXT("Void"), TEXT("Titan"), TEXT("Crystal"), TEXT("Toxic") };
	return (SIZE_T)S < UE_ARRAY_COUNT(N) ? N[(uint8)S] : TEXT("Warm");
}

EZoogiCrackStyle AZoogiCrackFX::StyleForMap(const UWorld* World)
{
	const AZoogiGameMode* G = World ? World->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const EZoogiArena A = G ? (G->Arena ? G->Arena->GetTheme() : G->MenuArena) : EZoogiArena::Meadow;
	if (A == EZoogiArena::Volcanic) return EZoogiCrackStyle::Lava;
	if (A == EZoogiArena::Frozen) return EZoogiCrackStyle::Ice;
	return EZoogiCrackStyle::Warm;
}

EZoogiCrackStyle AZoogiCrackFX::StyleForArtifact(EZoogiArtifact A)
{
	switch (A)
	{
	case EZoogiArtifact::Ember: return EZoogiCrackStyle::Fire;
	case EZoogiArtifact::Frost: return EZoogiCrackStyle::Frost;
	case EZoogiArtifact::Storm: return EZoogiCrackStyle::Shock;
	case EZoogiArtifact::Void:  return EZoogiCrackStyle::Void;
	default:                    return EZoogiCrackStyle::Titan; // Titan / neutral: warm orange on grey stone
	}
}

FVector AZoogiCrackFX::FloorUnder(const UWorld* World, const FVector& P)
{
	if (!World) return P;
	FHitResult H;
	FCollisionObjectQueryParams Q(ECC_WorldStatic);
	if (World->LineTraceSingleByObjectType(H, P + FVector(0, 0, 120.f), P - FVector(0, 0, 3000.f), Q) && H.ImpactNormal.Z > 0.5f) return H.ImpactPoint;
	return P;
}

AZoogiCrackFX::AZoogiCrackFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	Mesh->SetBoundsScale(3.f); // WPO raises the rim; keep it from culling early
}

AZoogiCrackFX* AZoogiCrackFX::Spawn(UWorld* World, const FVector& Floor, float InRadius, EZoogiCrackStyle InStyle, float InLife, float InStrength, const TCHAR* Reason)
{
	if (!World || !IsEnabled() || World->GetNetMode() == NM_DedicatedServer) return nullptr;
	TArray<TWeakObjectPtr<AZoogiCrackFX>>& L = LiveList();
	L.RemoveAll([](const TWeakObjectPtr<AZoogiCrackFX>& W) { return !W.IsValid(); });
	while (L.Num() >= CrackTune().MaxLive) { if (AZoogiCrackFX* Old = L[0].Get()) Old->Destroy(); L.RemoveAt(0); }
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiCrackFX* X = World->SpawnActor<AZoogiCrackFX>(AZoogiCrackFX::StaticClass(), FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Floor + FVector(0, 0, 1.5f)), P);
	if (!X) return nullptr;
	X->Style = InStyle; X->Radius = FMath::Clamp(InRadius, 0.4f * ZD, 0.9f * ZD) /* crater diameter 0.8..1.8x a Zoogi */; X->Life = FMath::Max(InLife, 0.8f); X->Strength = FMath::Clamp(InStrength, 0.2f, 2.f);
	X->Init();
	L.Add(X);
	UE_LOG(LogZoogi, Log, TEXT("CRACKS spawn %s r%.0f life %.1f str %.2f at (%.0f, %.0f, %.0f) [%s]"), StyleName(InStyle), X->Radius, X->Life, X->Strength, Floor.X, Floor.Y, Floor.Z, Reason ? Reason : TEXT("-"));
	return X;
}

void AZoogiCrackFX::Init()
{
	const FPal Pal = Palette(Style);
	UMaterialInterface* Base = LoadCrack<UMaterialInterface>(FString(TEXT("MI_ZoogiCrack_")) + StyleName(Style));
	if (!Base) Base = LoadCrack<UMaterialInterface>(TEXT("M_ZoogiCrack"));
	if (!Base) { UE_LOG(LogZoogi, Warning, TEXT("CRACKS: /Game/VFX/Cracks/M_ZoogiCrack missing (run Scripts/cracks/cracks_fx.py)")); }
	if (Base)
	{
		Mat = UMaterialInstanceDynamic::Create(Base, this);
		Mat->SetVectorParameterValue(TEXT("CrackColor"), Pal.Crack);
		Mat->SetVectorParameterValue(TEXT("InnerColor"), Pal.Inner);
		Mat->SetVectorParameterValue(TEXT("StoneColor"), Pal.Stone);
		Mat->SetScalarParameterValue(TEXT("Emissive"), Pal.Emissive);
		Mat->SetScalarParameterValue(TEXT("CellDensity"), 2.6f * (Style == EZoogiCrackStyle::Crystal ? 0.8f : 1.f));
		Mat->SetScalarParameterValue(TEXT("ExtrudeDepth"), Radius * 0.16f * Strength);
		Mat->SetScalarParameterValue(TEXT("Rise"), 0.f);
		Mat->SetScalarParameterValue(TEXT("RadiusMask"), 0.5f);
		if (!CrackTune().bSeams) Mat->SetScalarParameterValue(TEXT("SeamSharp"), 400.f); // seam glow -> ~0: crater bowl / rim only
	}
	if (UMaterialInterface* SM = LoadCrack<UMaterialInterface>(TEXT("M_ZoogiCrackShaft")))
	{
		ShaftMat = UMaterialInstanceDynamic::Create(SM, this);
		ShaftMat->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Pal.Crack, FLinearColor::White, 0.08f));
		ShaftMat->SetScalarParameterValue(TEXT("Intensity"), 1.5f + Pal.Emissive * 0.08f);
	}
	BuildDisc();
	// light shafts: a few vertical cards standing on random seam points
	FRandomStream R(FMath::Rand());
	const int32 NS = FMath::Clamp(FMath::RoundToInt(2.f + Strength * 3.f), 2, 7);
	for (int32 i = 0; i < NS; ++i)
	{
		const float A = R.FRandRange(0.f, 2.f * PI), D = R.FRandRange(0.1f, 0.7f) * Radius;
		Shafts.Add({ FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0.f), (float)R.FRandRange(0.10f, 0.22f) * Radius, (float)R.FRandRange(1.2f, 2.2f) * Radius * (0.6f + 0.4f * Strength), (float)R.FRandRange(0.f, 0.25f), (float)R.FRandRange(0.9f, 1.6f) });
	}
	if (UNiagaraSystem* NS2 = LoadCrack<UNiagaraSystem>(FString(TEXT("NS_ZoogiCrackSparks_")) + StyleName(Style)))
	{
		Sparks = UNiagaraFunctionLibrary::SpawnSystemAttached(NS2, RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, FVector(Radius / 100.f), EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::None, true, true);
	}
	if (CrackTune().bLight && Strength >= 0.6f)
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(RootComponent);
		Light->SetRelativeLocation(FVector(0, 0, Radius * 0.35f));
		Light->SetLightColor(Pal.Crack);
		Light->SetAttenuationRadius(Radius * 2.5f);
		Light->SetCastShadows(false);
		Light->SetIntensityUnits(ELightUnits::Unitless);
		Light->SetIntensity(0.f);
		Light->RegisterComponent();
		LightPeak = 18000.f * Strength * FMath::Clamp(Radius / 250.f, 0.2f, 1.f);
	}
	Tick(0.f);
}

void AZoogiCrackFX::BuildDisc()
{
	constexpr int32 G = 48; // 49x49 verts / 4.6k tris: enough for the WPO cell walls, cheap on mobile
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<FProcMeshTangent> Tg;
	V.Reserve((G + 1) * (G + 1));
	for (int32 y = 0; y <= G; ++y) for (int32 x = 0; x <= G; ++x)
	{
		const float u = (float)x / G, v = (float)y / G;
		V.Add(FVector((u - 0.5f) * 2.f * Radius, (v - 0.5f) * 2.f * Radius, 0.f));
		UV.Add(FVector2D(u, v)); N.Add(FVector::UpVector); C.Add(FLinearColor::White); Tg.Add(FProcMeshTangent(1, 0, 0));
	}
	for (int32 y = 0; y < G; ++y) for (int32 x = 0; x < G; ++x)
	{
		const float cx = ((x + 0.5f) / G - 0.5f) * 2.f, cy = ((y + 0.5f) / G - 0.5f) * 2.f;
		if (cx * cx + cy * cy > 1.06f) continue; // round disc; the material masks the ragged edge
		const int32 a = y * (G + 1) + x, b = a + 1, c = a + G + 1, d = c + 1;
		T.Append({ a, c, b, b, c, d });
	}
	Mesh->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tg, false);
	if (Mat) Mesh->SetMaterial(0, Mat);
}

void AZoogiCrackFX::UpdateShafts(float Fade)
{
	if (!ShaftMat || Shafts.Num() == 0) return;
	FVector Cam = GetActorLocation() + FVector(-600.f, 0.f, 300.f);
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0)) Cam = PCM->GetCameraLocation();
	const FTransform Xf = GetActorTransform();
	TArray<FVector> V; TArray<int32> T; TArray<FVector> N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<FProcMeshTangent> Tg;
	for (const FShaft& S : Shafts)
	{
		const float k = FMath::Clamp((Age - S.Delay) / 0.12f, 0.f, 1.f) * FMath::Clamp(1.f - (Age - S.Delay - 0.15f) / S.Len, 0.f, 1.f) * Fade;
		if (k <= 0.001f) continue;
		const FVector WP = Xf.TransformPosition(S.P);
		FVector Side = FVector::CrossProduct(FVector::UpVector, (Cam - WP).GetSafeNormal2D());
		if (Side.IsNearlyZero()) Side = FVector::RightVector;
		const FVector Ls = Xf.InverseTransformVectorNoScale(Side) * S.W * 0.5f;
		const FVector Up(0, 0, S.H * (0.7f + 0.3f * k));
		const int32 i0 = V.Num();
		V.Append({ S.P - Ls + Up, S.P + Ls + Up, S.P - Ls, S.P + Ls });
		UV.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1) });
		for (int32 j = 0; j < 4; ++j) { N.Add(FVector::UpVector); C.Add(FLinearColor(1, 1, 1, k)); Tg.Add(FProcMeshTangent(1, 0, 0)); }
		T.Append({ i0, i0 + 2, i0 + 1, i0 + 1, i0 + 2, i0 + 3 });
	}
	if (V.Num() == 0) { Mesh->ClearMeshSection(1); return; }
	Mesh->CreateMeshSection_LinearColor(1, V, T, N, UV, C, Tg, false);
	Mesh->SetMaterial(1, ShaftMat);
}

void AZoogiCrackFX::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	const float Out = FMath::Clamp((Life - Age) / 1.2f, 0.f, 1.f);
	const float Rise = CrackEase(Age / 0.14f) * FMath::Clamp((Life - Age) / 1.0f, 0.f, 1.f);
	const float Glow = (1.f + 1.5f * FMath::Exp(-Age * 6.f)) * Out;
	const float Mask = FMath::Lerp(0.55f, 1.f, CrackEase(Age / 0.16f)) * (0.3f + 0.7f * FMath::Clamp((Life - Age) / 0.5f, 0.f, 1.f));
	if (Mat)
	{
		Mat->SetScalarParameterValue(TEXT("Rise"), Rise);
		Mat->SetScalarParameterValue(TEXT("Glow"), Glow);
		Mat->SetScalarParameterValue(TEXT("RadiusMask"), Mask);
	}
	if (Light) Light->SetIntensity(LightPeak * FMath::Clamp(Age / 0.08f, 0.f, 1.f) * FMath::Exp(-Age * 1.6f) * Out);
	UpdateShafts(Out);
	if (Age >= Life) Destroy();
}

// ---------------------------------------------------------------- hooks

void AZoogiCrackFX::OnImpact(UWorld* World, const FVector& At, float Impulse, EZoogiArtifact Element)
{
	if (!IsEnabled() || Impulse < CrackTune().ImpactMin) return;
	const float K = FMath::Clamp((Impulse - CrackTune().ImpactMin) / 30000.f, 0.f, 1.f);
	const EZoogiCrackStyle S = Element != EZoogiArtifact::None ? StyleForArtifact(Element) : StyleForMap(World);
	Spawn(World, FloorUnder(World, At), ZD * FMath::Lerp(0.5f, 0.72f, K), S, CrackTune().Life * FMath::Lerp(0.8f, 1.2f, K), FMath::Lerp(0.6f, 1.3f, K), TEXT("A impact"));
}

void AZoogiCrackFX::OnLanding(AZoogiBall* Ball, const FVector& At, float DownSpeed)
{
	if (!Ball || !IsEnabled() || DownSpeed < CrackTune().LandSpeed) return;
	static TMap<TWeakObjectPtr<AZoogiBall>, double> Last;
	UWorld* W = Ball->GetWorld();
	const double Now = W ? W->GetTimeSeconds() : 0.0;
	if (double* L = Last.Find(Ball)) if (Now - *L < 0.6 && Now >= *L) return;
	Last.Add(Ball, Now);
	const float K = FMath::Clamp((DownSpeed - CrackTune().LandSpeed) / 1800.f, 0.f, 1.f);
	UE_LOG(LogZoogi, Log, TEXT("CRACKS landing P%d %.0f cm/s"), Ball->PlayerIndex + 1, DownSpeed);
	Spawn(W, At, ZD * FMath::Lerp(0.48f, 0.62f, K), StyleForMap(W), CrackTune().Life * FMath::Lerp(0.7f, 1.0f, K), FMath::Lerp(0.5f, 1.1f, K), TEXT("A landing"));
}

void AZoogiCrackFX::OnAwaken(UWorld* World, const FVector& Floor, EZoogiArtifact Element, bool bHit)
{
	if (!IsEnabled() || !CrackTune().bAwakenCracks || Element == EZoogiArtifact::None) return;
	Spawn(World, FloorUnder(World, Floor), ZD * (bHit ? 0.7f : 0.55f), StyleForArtifact(Element), CrackTune().Life, bHit ? 1.15f : 0.75f, bHit ? TEXT("B awaken hit") : TEXT("B awaken infuse"));
}

void AZoogiCrackFX::OnCrystalCollapse(UWorld* World, const FVector& At, bool bBig)
{
	if (!IsEnabled()) return;
	Spawn(World, FloorUnder(World, At), ZD * (bBig ? 0.9f : 0.75f), EZoogiCrackStyle::Crystal, CrackTune().Life * 1.4f, bBig ? 1.6f : 1.2f, TEXT("C crystal collapse"));
}

// ---------------------------------------------------------------- showcase (-ZoogiCrackShot)

void AZoogiCrackFX::EnsureShowcase(UWorld* World)
{
	if (!World || !CrackTune().bShot || !IsEnabled() || !World->IsGameWorld()) return;
	static TWeakObjectPtr<UWorld> Done;
	if (Done.Get() == World) return;
	Done = World;
	World->SpawnActor<AZoogiCrackShowcase>();
	UE_LOG(LogZoogi, Log, TEXT("CRACKS showcase armed"));
}

AZoogiCrackShowcase::AZoogiCrackShowcase() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bTickEvenWhenPaused = true; }

void AZoogiCrackShowcase::Tick(float Dt)
{
	Super::Tick(Dt);
	T += FApp::GetDeltaTime(); // real time (slow-mo / pause safe)
	struct FCase { const TCHAR* Name; int32 Kind; EZoogiCrackStyle Style; float R, Str; };
	const EZoogiCrackStyle MapS = AZoogiCrackFX::StyleForMap(GetWorld());
	const FCase Cases[] = {
		{ TEXT("A_impact"), 0, MapS, 72.f, 1.2f }, { TEXT("A_landing"), 0, MapS, 55.f, 0.8f },
		{ TEXT("B_fire"), 1, EZoogiCrackStyle::Fire, 70.f, 1.15f }, { TEXT("B_frost"), 1, EZoogiCrackStyle::Frost, 70.f, 1.15f },
		{ TEXT("B_shock"), 1, EZoogiCrackStyle::Shock, 70.f, 1.15f }, { TEXT("B_void"), 1, EZoogiCrackStyle::Void, 70.f, 1.15f },
		{ TEXT("B_titan"), 1, EZoogiCrackStyle::Titan, 70.f, 1.15f }, { TEXT("C_crystal"), 2, EZoogiCrackStyle::Crystal, 90.f, 1.6f } };
	constexpr float Start = 9.f, Period = 3.2f, ShotAt = 0.5f;
	if (T < Start) return;
	const int32 S = FMath::FloorToInt((T - Start) / Period);
	const float In = (T - Start) - S * Period;
	if (S >= (int32)UE_ARRAY_COUNT(Cases)) { if (Step != 99) { Step = 99; UE_LOG(LogZoogi, Log, TEXT("CRACKS SHOWCASE done (%d cases)"), (int32)UE_ARRAY_COUNT(Cases)); } return; }
	const FCase& C = Cases[S];
	if (S != Step)
	{
		Step = S; bShot = false;
		if (Cur.IsValid()) Cur->Destroy();
		FVector P = FVector::ZeroVector;
		if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			const FVector From = PCM->GetCameraLocation(), Dir = PCM->GetCameraRotation().Vector();
			FHitResult H;
			FCollisionObjectQueryParams Q(ECC_WorldStatic);
			if (GetWorld()->LineTraceSingleByObjectType(H, From, From + Dir * 30000.f, Q)) P = H.ImpactPoint;
			else P = AZoogiCrackFX::FloorUnder(GetWorld(), From + Dir * 1500.f);
		}
		Cur = AZoogiCrackFX::Spawn(GetWorld(), P, C.R, C.Style, Period + 0.5f, C.Str, *FString::Printf(TEXT("showcase %s"), C.Name));
	}
	if (!bShot && In >= ShotAt)
	{
		bShot = true;
		const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
		const FString Dir = G && !G->ShotDir.IsEmpty() ? G->ShotDir : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CrackShots"));
		const EZoogiArena A = G ? (G->Arena ? G->Arena->GetTheme() : G->MenuArena) : EZoogiArena::Meadow;
		const FString Pth = FPaths::Combine(Dir, FString::Printf(TEXT("cracks_%s_%d_%s.png"), *UEnum::GetValueAsString(A).Replace(TEXT("EZoogiArena::"), TEXT("")), S, C.Name));
		FScreenshotRequest::RequestScreenshot(Pth, false, false); // viewport only, no UI
		UE_LOG(LogZoogi, Log, TEXT("CRACKS shot %s"), *Pth);
	}
}
