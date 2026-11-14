#include "ZoogiFX.h"
#include "ZoogiSkills.h"
#include "GameFramework/WorldSettings.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "ZoogiKFXExtras.h"
#include "ZoogiPlayerController.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	TMap<FName, TWeakObjectPtr<UNiagaraSystem>>& NSCache() { static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> C; return C; }
	UNiagaraSystem* FindNS(const TCHAR* SystemName)
	{
		const FName Key(SystemName);
		if (UNiagaraSystem* Hit = NSCache().FindRef(Key).Get()) return Hit;
		const FString Path = FString::Printf(TEXT("/Game/FX/%s.%s"), SystemName, SystemName);
		UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (Sys) NSCache().Add(Key, Sys);
		return Sys;
	}
	TArray<TWeakObjectPtr<AZoogiFX>>& FXPool() { static TArray<TWeakObjectPtr<AZoogiFX>> P; return P; }
	constexpr int32 MaxFXPool = 48;
}

int32 AZoogiFX::Reused = 0;

void AZoogiFX::Niagara(UWorld* World, const TCHAR* SystemName, const FVector& Location, const FRotator& Rotation, float Scale)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara"));
	if (!World || bDisabled || bMuted || World->GetNetMode() == NM_DedicatedServer) return;
	UNiagaraSystem* Sys = FindNS(SystemName);
	if (!Sys) return;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, Rotation, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
}

namespace
{
	UNiagaraSystem* FindSty(const TCHAR* Name)
	{
		static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> C;
		const FName Key(Name);
		if (UNiagaraSystem* Hit = C.FindRef(Key).Get()) return Hit;
		const FString Path = FString::Printf(TEXT("/Game/VFX_PackVol1/Particles/P_%s_Converted.P_%s_Converted"), Name, Name);
		UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Sys) { C.Add(Key, Sys); Sys->AddToRoot(); } // small pack: keep loaded (no hitch on the next blast)
		else UE_LOG(LogTemp, Warning, TEXT("ZOOGI stylized fx missing: %s"), *Path);
		return Sys;
	}
	bool StyOff() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoStyFX")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara")); return b; }
}

void AZoogiFX::Stylized(UWorld* World, const TCHAR* Name, const FVector& Location, float Scale, const FRotator& Rotation)
{
	if (!World || StyOff() || bMuted || World->GetNetMode() == NM_DedicatedServer) return;
	UNiagaraSystem* Sys = FindSty(Name);
	if (!Sys) return;
	// Nov 1 b55 (Evan: "they keep going and don't turn off"): the converted Cascade emitters loop forever, so every one-shot
	// gets a lifetime: Deactivate (the last particles finish), then the component is destroyed.
	UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, Rotation, FVector(Scale), false, true, ENCPoolMethod::None);
	if (!NC) return;
	const FString N(Name);
	if (N.Contains(TEXT("Big_Explosion"))) BigBlastShake(World, Scale); // b171 (Evan): the screen shakes on every big blast
	const float Life = N.Contains(TEXT("Smoke")) ? 1.8f : (N.Contains(TEXT("Fire")) ? 1.2f : (N.Contains(TEXT("Electric")) ? 0.8f : (N.Contains(TEXT("Big")) ? 1.3f : 1.0f)));
	TWeakObjectPtr<UNiagaraComponent> WC(NC);
	FTimerHandle H1, H2;
	World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WC]() { if (WC.IsValid()) WC->Deactivate(); }), Life, false);
	World->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WC]() { if (WC.IsValid()) WC->DestroyComponent(); }), Life + 2.5f, false);
}

void AZoogiFX::BigBlastShake(UWorld* World, float Scale)
{
	if (!World) return;
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(World->GetFirstPlayerController()))
		PC->OnBigBlast(FMath::Clamp(1.1f + 0.6f * Scale, 1.2f, 2.f));
}

UNiagaraComponent* AZoogiFX::StylizedLoop(UWorld* World, const TCHAR* Name, const FVector& Location, float Scale, USceneComponent* Attach)
{
	if (!World || StyOff() || World->GetNetMode() == NM_DedicatedServer) return nullptr;
	UNiagaraSystem* Sys = FindSty(Name);
	if (!Sys) return nullptr;
	if (Attach)
		return UNiagaraFunctionLibrary::SpawnSystemAttached(Sys, Attach, NAME_None, Location, FRotator::ZeroRotator, FVector(Scale), EAttachLocation::KeepWorldPosition, false, ENCPoolMethod::None);
	return UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, FRotator::ZeroRotator, FVector(Scale), false, true, ENCPoolMethod::None);
}

void AZoogiFX::Prewarm(UWorld* World, const TArray<const TCHAR*>& Systems, TArray<TObjectPtr<UObject>>& Keep, int32 PoolActors)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;
	int32 NS = 0;
	for (const TCHAR* S : Systems) if (UNiagaraSystem* Sys = FindNS(S)) { ++NS; Keep.AddUnique(Sys); } // the caller's UPROPERTY keeps them loaded
	int32 Made = 0;
	for (int32 i = FXPool().Num(); i < PoolActors; ++i)
	{
		if (AZoogiFX* FX = Spawn(World, FVector(0.f, 0.f, -10000.f), FLinearColor::Black, 1.f, 0.05f, false, 0.f)) { FX->Release(); ++Made; }
	}
	UE_LOG(LogTemp, Log, TEXT("ZOOGI fx prewarm: %d/%d Niagara systems loaded, %d pooled glow actors (pool %d)"), NS, Systems.Num(), Made, FXPool().Num());
}

void AZoogiFX::Release()
{
	Follow.Reset();
	if (Flash) Flash->SetIntensity(0.f);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	FXPool().RemoveAll([](const TWeakObjectPtr<AZoogiFX>& X) { return !X.IsValid(); });
	if (FXPool().Num() < MaxFXPool) FXPool().Add(this);
	else Destroy();
}

UNiagaraComponent* AZoogiFX::KFX(UWorld* World, const TCHAR* Prefab, const FVector& Location, float Scale, float MaxLife, AActor* Attach)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKFX"));
	if (!World || bDisabled || bMuted || World->GetNetMode() == NM_DedicatedServer) return nullptr;
	static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> Cache;
	const FName Key(Prefab);
	UNiagaraSystem* Sys = Cache.FindRef(Key).Get();
	if (!Sys)
	{
		const FString P(Prefab);
		int32 U = INDEX_NONE;
		const FString Folder = P.FindChar(TEXT('_'), U) ? P.Left(U) : P;
		const FString Path = FString::Printf(TEXT("/Game/VFX/KriptoFX/%s/NS_KFX_%s.NS_KFX_%s"), *Folder, *P, *P);
		Sys = LoadObject<UNiagaraSystem>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (!Sys) return nullptr;
		Cache.Add(Key, Sys);
	}
	UNiagaraComponent* C = nullptr;
	if (Attach && Attach->GetRootComponent())
	{
		C = UNiagaraFunctionLibrary::SpawnSystemAttached(Sys, Attach->GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::None);
		if (C)
		{
			// Marbles roll: keep the effect upright and at its own scale.
			C->SetUsingAbsoluteRotation(true);
			C->SetUsingAbsoluteScale(true);
			C->SetWorldRotation(FRotator::ZeroRotator);
			C->SetWorldScale3D(FVector(Scale));
		}
	}
	else
	{
		C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::None);
	}
	if (C)
	{
		// Decals / lights / placed meshes of the prefab (ground cracks, icicles, rock spikes, tornado, portal disc).
		AZoogiKFXExtras::Spawn(World, FString(Prefab), Location, Scale, MaxLife, Attach);
	}
	if (C && MaxLife > 0.f)
	{
		// Some ported emitters loop (fire skins, fog): stop spawning after MaxLife, hard-remove a few seconds later.
		TWeakObjectPtr<UNiagaraComponent> W(C);
		FTimerHandle H1, H2;
		World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([W]() { if (W.IsValid()) W->Deactivate(); }), MaxLife, false);
		World->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([W]() { if (W.IsValid()) W->DestroyComponent(); }), MaxLife + 4.f, false);
	}
	return C;
}

const TCHAR* AZoogiFX::PowerBurst(EZoogiPower Power)
{
	switch (Power)
	{
	case EZoogiPower::WolfPack: return TEXT("NS_Burst_White");
	case EZoogiPower::Explosion: return TEXT("NS_Explosion");
	case EZoogiPower::Ricochet: return TEXT("NS_Burst_Green");
	case EZoogiPower::LockOn: return TEXT("NS_Burst_Red");
	case EZoogiPower::StaticShock: return TEXT("NS_Burst_Yellow");
	case EZoogiPower::BandageBind: return TEXT("NS_Burst_Gold");
	case EZoogiPower::ShadowStun: return TEXT("NS_Burst_Purple");
	case EZoogiPower::RuneSeal: return TEXT("NS_Burst_Gold");
	}
	return TEXT("NS_Burst_White");
}

AZoogiFX::AZoogiFX()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;
	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Mesh);
	Flash->SetUsingAbsoluteScale(true);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	Flash->SetCastShadows(false);
	Flash->SetIntensity(0.f);
}

AZoogiFX* AZoogiFX::Spawn(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, bool bRing, float Strength, AActor* FollowActor)
{
	// Evan 8:59 AM merge41: no translucent sphere dome over a Zoogi (card guard shield / ice / shadow / storm bubbles followed the ball and covered the face). Rings stay.
	if (!bRing && FollowActor && FollowActor->IsA<AZoogiBall>()) return nullptr;
	if (!World || bMuted) return nullptr;
	// Oct 14 polish: reuse a pooled effect actor (no spawn / MID allocation per hit)
	AZoogiFX* FX = nullptr;
	while (FXPool().Num() && !FX)
	{
		AZoogiFX* C = FXPool().Pop(EAllowShrinking::No).Get();
		if (C && C->GetWorld() == World && !C->IsActorBeingDestroyed()) FX = C;
	}
	if (FX)
	{
		++Reused;
		FX->SetActorLocation(Location);
		FX->SetActorHiddenInGame(false);
		FX->SetActorTickEnabled(true);
	}
	else
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FX = World->SpawnActor<AZoogiFX>(AZoogiFX::StaticClass(), FTransform(Location), P);
	}
	if (!FX) return nullptr;
	FX->Age = 0.f;
	{	// b200: GATTLING RAPID holds the world still - its bolt sparks and blasts still play in real time
		const AWorldSettings* Ws = World->GetWorldSettings(); const float Gd = Ws ? Ws->GetEffectiveTimeDilation() : 1.f;
		FX->CustomTimeDilation = (Gd < 0.01f && ZoogiSkills::RapidActive()) ? 1.f / FMath::Max(Gd, 0.0001f) : 1.f;
	}
	FX->Flash->SetVisibility(true);
	FX->Life = FMath::Max(Duration, 0.05f);
	FX->EndRadius = Radius;
	FX->BaseStrength = Strength;
	FX->bIsRing = bRing;
	FX->Follow = FollowActor;
	FX->Mesh->SetStaticMesh(ZoogiUtil::BasicShape(bRing ? TEXT("Cylinder") : TEXT("Sphere")));
	if (!FX->MID) FX->MID = ZoogiUtil::FxMID(FX, Color, Strength);
	else { FX->MID->SetVectorParameterValue(TEXT("Color"), Color); FX->MID->SetScalarParameterValue(TEXT("Strength"), Strength); }
	FX->MID->SetScalarParameterValue(TEXT("Ring"), bRing ? 1.f : 0.f);
	FX->Mesh->SetMaterial(0, FX->MID);
	FX->BaseLight = FMath::Clamp(Radius * 0.25f, 20.f, 200.f);
	FX->Flash->SetLightColor(Color);
	FX->Flash->SetAttenuationRadius(Radius * 2.5f);
	FX->Flash->SetIntensity(FX->BaseLight);
	FX->Tick(0.f);
	return FX;
}

AZoogiFX* AZoogiFX::SpawnTrail(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration, float Strength)
{
	AZoogiFX* FX = Spawn(World, Location, Color, Radius, Duration, true, Strength);
	if (FX)
	{
		FX->BaseLight = 0.f;
		FX->Flash->SetIntensity(0.f);
		FX->Flash->SetVisibility(false);
	}
	return FX;
}


void AZoogiFX::SpawnConfetti(UWorld* World, const FVector& Location, const FLinearColor& Accent, int32 Count)
{
	if (!World) return;
	Count = FMath::Clamp(Count, 8, 48);
	static const FLinearColor Extra[] = {
		FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(1.f, 0.35f, 0.45f), FLinearColor(0.35f, 0.85f, 1.f),
		FLinearColor(0.55f, 1.f, 0.4f), FLinearColor(1.f, 1.f, 1.f), FLinearColor(0.9f, 0.5f, 1.f)
	};
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Off(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(40.f, 220.f));
		const FLinearColor C = (i % 3 == 0) ? Accent : Extra[i % UE_ARRAY_COUNT(Extra)];
		SpawnTrail(World, Location + Off, C, FMath::FRandRange(12.f, 28.f), FMath::FRandRange(0.55f, 1.1f), FMath::FRandRange(2.5f, 5.f));
	}
}

AZoogiFX* AZoogiFX::SpawnMark(UWorld* World, const FVector& Location, const FLinearColor& Color, float Radius, float Duration)
{
	// Soft dark scuff: ring mesh, no flash light, low strength so it reads as a mark not a glow burst.
	AZoogiFX* FX = Spawn(World, Location, Color, Radius, FMath::Max(Duration, 0.2f), true, 1.6f);
	if (FX)
	{
		FX->BaseLight = 0.f;
		FX->Flash->SetIntensity(0.f);
		FX->Flash->SetVisibility(false);
		// Flatten tighter to the floor and keep a soft opaque look via GlowMID when available.
		if (UMaterialInstanceDynamic* M = ZoogiUtil::GlowMID(FX, Color, 0.55f))
		{
			FX->MID = M;
			FX->Mesh->SetMaterial(0, M);
		}
		FX->Mesh->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.02f));
	}
	return FX;
}

void AZoogiFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = FMath::Clamp(Age / Life, 0.f, 1.f);
	if (Follow.IsValid()) SetActorLocation(Follow->GetActorLocation());
	const float Grow = FMath::InterpEaseOut(0.15f, 1.f, T, 2.5f);
	const float R = EndRadius * Grow;
	if (bIsRing) Mesh->SetWorldScale3D(FVector(R / 50.f, R / 50.f, 0.04f)); // engine cylinder: radius 50, height 100
	else Mesh->SetWorldScale3D(FVector(R / 50.f));
	const float Fade = 1.f - T * T;
	if (MID) MID->SetScalarParameterValue(TEXT("Strength"), BaseStrength * Fade);
	Flash->SetIntensity(BaseLight * (1.f - T));
	if (Age >= Life) Release();
}


// ---------------------------------------------------------------------------
// Easy Shockwaves VFX (Fab / Vefects) â€” collision / ring-out / pad
// ---------------------------------------------------------------------------
namespace ZoogiShockwaveNS
{
	constexpr int32 MaxConcurrent = 6;
	static int32 Live = 0;

	TMap<FName, TWeakObjectPtr<UNiagaraSystem>>& Cache()
	{
		static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> C;
		return C;
	}

	UNiagaraSystem* LoadNS(const FString& Path)
	{
		const FName Key(*Path);
		if (UNiagaraSystem* Hit = Cache().FindRef(Key).Get()) return Hit;
		// Soft path: /Game/.../Asset.Asset
		const FString Soft = Path.Contains(TEXT(".")) ? Path : FString::Printf(TEXT("%s.%s"), *Path, *FPaths::GetBaseFilename(Path));
		UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Soft, nullptr, LOAD_NoWarn);
		if (Sys) Cache().Add(Key, Sys);
		return Sys;
	}

	const TCHAR* SizeToken(EZoogiShockwaveSize Size)
	{
		switch (Size)
		{
		case EZoogiShockwaveSize::Small: return TEXT("_Small");
		case EZoogiShockwaveSize::Big: return TEXT("_Big");
		default: return TEXT("");
		}
	}

	/** Nearest pack Color_Variation name for a Zoogi player tint (white if none). */
	const TCHAR* ColorName(const FLinearColor* Tint)
	{
		if (!Tint) return nullptr;
		const float R = Tint->R, G = Tint->G, B = Tint->B;
		// Dominant channel / hue buckets matching pack: Red Green Yellow Teal Purple Pink Orange Lavender Black
		if (R < 0.2f && G < 0.2f && B < 0.2f) return TEXT("Black");
		if (R > G && R > B && R - G > 0.15f) return (G > 0.35f) ? TEXT("Orange") : TEXT("Red");
		if (G > R && G > B) return (B > 0.35f) ? TEXT("Teal") : TEXT("Green");
		if (B > R && B > G) return (R > 0.35f) ? TEXT("Purple") : TEXT("Teal");
		if (R > 0.7f && G > 0.7f && B < 0.45f) return TEXT("Yellow");
		if (R > 0.7f && B > 0.4f && G < 0.55f) return TEXT("Pink");
		if (R > 0.45f && G > 0.45f && B > 0.7f) return TEXT("Lavender");
		return TEXT("Teal");
	}

	FString FlatPath(EZoogiShockwaveSize Size, const TCHAR* Color)
	{
		const TCHAR* Sz = SizeToken(Size);
		if (Color)
		{
			return FString::Printf(
				TEXT("/Game/Vefects/Easy_Shockwaves_VFX/VFX/Shockwaves/Particles/Color_Variation/VFX_Shockwave_01_%s%s_1s"),
				Color, Sz);
		}
		return FString::Printf(
			TEXT("/Game/Vefects/Easy_Shockwaves_VFX/VFX/Shockwaves/Particles/VFX_Shockwave_01_White%s_1s"), Sz);
	}

	FString VerticalPath(EZoogiShockwaveSize Size)
	{
		return FString::Printf(
			TEXT("/Game/Vefects/Easy_Shockwaves_VFX/VFX/Shockwaves/Particles/Vertical/VFX_Shockwave_Vertical_01_White%s_1s"),
			SizeToken(Size));
	}

	FString DistortionPath(EZoogiShockwaveSize Size, bool bVertical)
	{
		const TCHAR* Sz = SizeToken(Size);
		if (bVertical)
		{
			return FString::Printf(
				TEXT("/Game/Vefects/Easy_Shockwaves_VFX/VFX/Shockwaves_Distortion/Particles/Vertical/VFX_Shockwave_Vertical_Distortion_01%s_1s"),
				Sz);
		}
		return FString::Printf(
			TEXT("/Game/Vefects/Easy_Shockwaves_VFX/VFX/Shockwaves_Distortion/Particles/VFX_Shockwave_Distortion_01%s_1s"),
			Sz);
	}

	/** light=500ms, heavy=1s, ring-out=2s */
	FString SfxPath(EZoogiShockwaveSize Size, bool bRingOut)
	{
		const TCHAR* Dur = bRingOut ? TEXT("2s") : (Size == EZoogiShockwaveSize::Small ? TEXT("500ms") : TEXT("1s"));
		return FString::Printf(
			TEXT("/Game/Vefects/Easy_Shockwaves_VFX/Audio/Cues/SFX_Vefects_Shockwave_01_%s_Cue.SFX_Vefects_Shockwave_01_%s_Cue"),
			Dur, Dur);
	}

	bool WantDistortion()
	{
		if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			// 0 Low, 1 Medium, 2 High, 3 Epic â€” distortion on High+
			return S->GetOverallScalabilityLevel() >= 2;
		}
		return true;
	}

	void ReleaseSlot(UWorld* World)
	{
		if (!World) { Live = FMath::Max(0, Live - 1); return; }
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([]() { Live = FMath::Max(0, Live - 1); }), 1.2f, false);
	}
}

void AZoogiFX::Shockwave(UWorld* World, const FVector& Location, EZoogiShockwaveSize Size, EZoogiShockwaveKind Kind,
	const FLinearColor* Tint, bool bRingOutSfx)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara"))
		|| FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShockwave"));
	if (bMuted) return; // b57: no sparks during a power clash
	if (!World || bDisabled || World->GetNetMode() == NM_DedicatedServer) return;
	if (ZoogiShockwaveNS::Live >= ZoogiShockwaveNS::MaxConcurrent) return;

	const bool bVert = (Kind == EZoogiShockwaveKind::Vertical);
	const TCHAR* Color = (!bVert) ? ZoogiShockwaveNS::ColorName(Tint) : nullptr;
	FString Path = bVert ? ZoogiShockwaveNS::VerticalPath(Size) : ZoogiShockwaveNS::FlatPath(Size, Color);
	UNiagaraSystem* Sys = ZoogiShockwaveNS::LoadNS(Path);
	if (!Sys && Color)
	{
		// Colour variant missing -> white
		Path = ZoogiShockwaveNS::FlatPath(Size, nullptr);
		Sys = ZoogiShockwaveNS::LoadNS(Path);
	}
	if (!Sys) return;

	++ZoogiShockwaveNS::Live;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Location, FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);

	if (ZoogiShockwaveNS::WantDistortion())
	{
		if (UNiagaraSystem* Dist = ZoogiShockwaveNS::LoadNS(ZoogiShockwaveNS::DistortionPath(Size, bVert)))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Dist, Location, FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
		}
	}

	if (USoundBase* Cue = LoadObject<USoundBase>(nullptr, *ZoogiShockwaveNS::SfxPath(Size, bRingOutSfx), nullptr, LOAD_NoWarn))
	{
		UGameplayStatics::PlaySoundAtLocation(World, Cue, Location);
	}

	ZoogiShockwaveNS::ReleaseSlot(World);
}

void AZoogiFX::ShockwaveByMag(UWorld* World, const FVector& Location, float Mag, const FLinearColor* Tint, bool bVertical, bool bRingOutSfx)
{
	EZoogiShockwaveSize Size = EZoogiShockwaveSize::Small;
	if (Mag >= 22000.f) Size = EZoogiShockwaveSize::Big;
	else if (Mag >= 12000.f) Size = EZoogiShockwaveSize::Normal;
	else if (Mag < 3500.f && !bRingOutSfx) return; // below spark threshold, skip (ring-out always fires)
	const EZoogiShockwaveKind Kind = bVertical ? EZoogiShockwaveKind::Vertical : EZoogiShockwaveKind::Flat;
	Shockwave(World, Location, Size, Kind, Tint, bRingOutSfx);
}

void AZoogiFX::PrewarmShockwaves(UWorld* World, TArray<TObjectPtr<UObject>>& Keep)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;
	const FString Paths[] = {
		ZoogiShockwaveNS::FlatPath(EZoogiShockwaveSize::Small, nullptr),
		ZoogiShockwaveNS::FlatPath(EZoogiShockwaveSize::Normal, nullptr),
		ZoogiShockwaveNS::FlatPath(EZoogiShockwaveSize::Big, nullptr),
		ZoogiShockwaveNS::VerticalPath(EZoogiShockwaveSize::Small),
		ZoogiShockwaveNS::VerticalPath(EZoogiShockwaveSize::Big),
		ZoogiShockwaveNS::DistortionPath(EZoogiShockwaveSize::Small, false),
		ZoogiShockwaveNS::DistortionPath(EZoogiShockwaveSize::Big, true),
	};
	int32 N = 0;
	for (const FString& P : Paths)
	{
		if (UNiagaraSystem* Sys = ZoogiShockwaveNS::LoadNS(P)) { Keep.AddUnique(Sys); ++N; }
	}
	UE_LOG(LogTemp, Log, TEXT("ZOOGI fx prewarm shockwaves: %d/%d Vefects systems loaded"), N, UE_ARRAY_COUNT(Paths));
}

// ---------------------------------------------------------------------------
// Free Arrow Trail (Fab) � fast-roll ribbon + blue orb ribbon
// ---------------------------------------------------------------------------
namespace ZoogiArrowTrailNS
{
	constexpr int32 MaxConcurrent = 6;
	static int32 Live = 0;

	TMap<FName, TWeakObjectPtr<UNiagaraSystem>>& Cache()
	{
		static TMap<FName, TWeakObjectPtr<UNiagaraSystem>> C;
		return C;
	}

	FString SoftPath(const TCHAR* Which)
	{
		const FString W(Which && *Which ? Which : TEXT("Basic"));
		// Pack assets: NS_ArrowTrail_Basic, _Basic_02, _Water, _Fire, ...
		const FString Name = W.StartsWith(TEXT("NS_")) ? W : FString::Printf(TEXT("NS_ArrowTrail_%s"), *W);
		return FString::Printf(TEXT("/Game/ArrowTrail/FX/%s.%s"), *Name, *Name);
	}

	UNiagaraSystem* LoadNS(const TCHAR* Which)
	{
		const FString Soft = SoftPath(Which);
		const FName Key(*Soft);
		if (UNiagaraSystem* Hit = Cache().FindRef(Key).Get()) return Hit;
		UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Soft, nullptr, LOAD_NoWarn);
		if (Sys) Cache().Add(Key, Sys);
		return Sys;
	}

	void FreeSlot()
	{
		Live = FMath::Max(0, Live - 1);
	}
}

UNiagaraComponent* AZoogiFX::ArrowTrail(UWorld* World, AActor* Attach, const TCHAR* Which, float Scale, float MaxLife)
{
	static const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoNiagara"))
		|| FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoArrowTrail"));
	if (!World || bDisabled || World->GetNetMode() == NM_DedicatedServer) return nullptr;
	if (!Attach || !Attach->GetRootComponent()) return nullptr;
	if (ZoogiArrowTrailNS::Live >= ZoogiArrowTrailNS::MaxConcurrent) return nullptr;

	UNiagaraSystem* Sys = ZoogiArrowTrailNS::LoadNS(Which);
	if (!Sys && Which && FCString::Stricmp(Which, TEXT("Basic")) != 0)
	{
		Sys = ZoogiArrowTrailNS::LoadNS(TEXT("Basic")); // fallback to clean white
	}
	if (!Sys) return nullptr;

	UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys, Attach->GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::None);
	if (!C) return nullptr;

	// Marbles / orbs roll: keep ribbon upright at a stable world scale (mobile-friendly size).
	const float S = FMath::Clamp(Scale, 0.2f, 1.25f);
	C->SetUsingAbsoluteRotation(true);
	C->SetUsingAbsoluteScale(true);
	C->SetWorldRotation(FRotator::ZeroRotator);
	C->SetWorldScale3D(FVector(S));

	++ZoogiArrowTrailNS::Live;

	if (MaxLife > 0.f)
	{
		TWeakObjectPtr<UNiagaraComponent> W(C);
		FTimerHandle H1, H2;
		World->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([W]()
		{
			if (W.IsValid()) { W->Deactivate(); }
		}), MaxLife, false);
		World->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([W]()
		{
			if (W.IsValid()) { W->DestroyComponent(); }
			ZoogiArrowTrailNS::FreeSlot();
		}), MaxLife + 2.5f, false);
	}
	return C;
}

void AZoogiFX::ReleaseArrowTrail(UNiagaraComponent* C)
{
	if (!C) return;
	C->Deactivate();
	C->DestroyComponent();
	ZoogiArrowTrailNS::FreeSlot();
}

void AZoogiFX::PrewarmArrowTrails(UWorld* World, TArray<TObjectPtr<UObject>>& Keep)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;
	const TCHAR* Which[] = { TEXT("Basic"), TEXT("Basic_02"), TEXT("Water") };
	int32 N = 0;
	for (const TCHAR* W : Which)
	{
		if (UNiagaraSystem* Sys = ZoogiArrowTrailNS::LoadNS(W)) { Keep.AddUnique(Sys); ++N; }
	}
	UE_LOG(LogTemp, Log, TEXT("ZOOGI fx prewarm arrow trails: %d/%d Free Arrow Trail systems loaded"), N, UE_ARRAY_COUNT(Which));
}