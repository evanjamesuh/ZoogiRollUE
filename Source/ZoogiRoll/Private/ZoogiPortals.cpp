// Oct 8 (Evan 11:48 PM): Boost Portals (see ZoogiPortals.h).
#include "ZoogiPortals.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "UnrealClient.h"

namespace
{
	const FLinearColor PortalCyan(0.25f, 0.9f, 1.f);
	const FLinearColor PortalHot(0.85f, 0.97f, 1.f);

	UStaticMeshComponent* NewSeg(AActor* Owner, USceneComponent* Root, UStaticMesh* Cube)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Cube);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetGenerateOverlapEvents(false);
		C->SetupAttachment(Root);
		C->RegisterComponent();
		C->SetVisibility(false);
		return C;
	}
	/** A thin stretched cube from A to B (world space), Thick cm across. */
	void PlaceSeg(UStaticMeshComponent* C, const FVector& A, const FVector& B, float Thick)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f) { C->SetVisibility(false); return; }
		C->SetWorldLocationAndRotation((A + B) * 0.5f, D.Rotation());
		C->SetWorldScale3D(FVector(L / 100.f, Thick / 100.f, Thick / 100.f));
		C->SetVisibility(true);
	}
	/** Jagged polyline from A to B: N points with lateral jitter (perpendicular to AB) of up to Amp. */
	void Bolt(TArray<FVector>& Out, const FVector& A, const FVector& B, int32 N, float Amp, const FVector& Up, FRandomStream& R)
	{
		Out.Reset();
		const FVector D = B - A;
		FVector S1 = FVector::CrossProduct(D.GetSafeNormal(), Up).GetSafeNormal();
		if (S1.IsNearlyZero()) S1 = FVector::RightVector;
		const FVector S2 = FVector::CrossProduct(D.GetSafeNormal(), S1).GetSafeNormal();
		Out.Add(A);
		for (int32 i = 1; i < N; ++i)
		{
			const float T = (float)i / N;
			const float Env = FMath::Sin(T * PI); // pinned at both ends
			Out.Add(A + D * T + (S1 * R.FRandRange(-1.f, 1.f) + S2 * R.FRandRange(-0.6f, 0.6f)) * Amp * Env);
		}
		Out.Add(B);
	}
}

// ------------------------------------------------------------------------------------------------ surge FX

AZoogiPortalFX::AZoogiPortalFX()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetCastShadows(false);
	Light->SetLightColor(PortalCyan);
}

void AZoogiPortalFX::Setup(float InHalfW, float InTop)
{
	HalfW = InHalfW;
	Top = InTop;
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	MatHot = ZoogiUtil::FxMID(this, PortalHot, 14.f);
	MatCyan = ZoogiUtil::FxMID(this, PortalCyan, 9.f);
	MatSheet = ZoogiUtil::FxMID(this, PortalCyan, 0.18f);
	for (int32 i = 0; i < 44; ++i)
	{
		UStaticMeshComponent* C = NewSeg(this, RootComponent, Cube);
		C->SetMaterial(0, (i % 3 == 0) ? MatHot : MatCyan);
		Segs.Add(C);
	}
	// faint idle sheet in the opening so the portal reads as one (no collision)
	Sheet = NewSeg(this, RootComponent, Cube);
	Sheet->SetMaterial(0, MatSheet);
	Sheet->SetRelativeLocation(FVector(0.f, 0.f, Top * 0.5f));
	Sheet->SetRelativeScale3D(FVector(0.02f, HalfW * 2.f / 100.f, Top / 100.f));
	Sheet->SetVisibility(true);
	Light->SetRelativeLocation(FVector(0.f, 0.f, Top * 0.5f));
}

void AZoogiPortalFX::Surge(const FVector& Dir)
{
	SurgeT = 0.45f;
	ReboltT = 0.f;
	++Serial;
	if (MatSheet) MatSheet->SetScalarParameterValue(TEXT("Strength"), 1.6f);
	ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.9f, 1.15f);
	Rebolt();
}

void AZoogiPortalFX::Rebolt()
{
	// 4 jagged bolts post to post + 2 floor-to-lintel arcs, regenerated every 50 ms while the surge lasts (cosmetic stream)
	FRandomStream R(Serial * 7919 + (int32)(SurgeT * 1000.f));
	const FTransform X = GetActorTransform();
	TArray<FVector> P;
	int32 S = 0;
	auto Emit = [&](const FVector& A, const FVector& B, int32 N, float Amp, float Thick)
	{
		Bolt(P, X.TransformPosition(A), X.TransformPosition(B), N, Amp, X.GetUnitAxis(EAxis::X), R);
		for (int32 i = 0; i + 1 < P.Num() && S < Segs.Num(); ++i) PlaceSeg(Segs[S++], P[i], P[i + 1], Thick);
	};
	for (int32 b = 0; b < 4; ++b)
	{
		const float Za = R.FRandRange(0.12f, 0.92f) * Top, Zb = R.FRandRange(0.12f, 0.92f) * Top;
		Emit(FVector(0.f, -HalfW, Za), FVector(0.f, HalfW, Zb), 7, 38.f, b == 0 ? 7.f : 4.f);
	}
	for (int32 b = 0; b < 2; ++b)
	{
		const float Ya = R.FRandRange(-0.7f, 0.7f) * HalfW;
		Emit(FVector(0.f, Ya, 6.f), FVector(0.f, R.FRandRange(-0.7f, 0.7f) * HalfW, Top), 7, 30.f, 3.f);
	}
	for (; S < Segs.Num(); ++S) Segs[S]->SetVisibility(false);
}

void AZoogiPortalFX::Tick(float Dt)
{
	Super::Tick(Dt);
	if (SurgeT <= 0.f) return;
	SurgeT -= Dt;
	ReboltT += Dt;
	const float K = FMath::Clamp(SurgeT / 0.45f, 0.f, 1.f);
	Light->SetIntensity(K * K * 90000.f);
	if (MatSheet) MatSheet->SetScalarParameterValue(TEXT("Strength"), 0.18f + 1.4f * K);
	if (SurgeT <= 0.f)
	{
		for (UStaticMeshComponent* C : Segs) C->SetVisibility(false);
		Light->SetIntensity(0.f);
		if (MatSheet) MatSheet->SetScalarParameterValue(TEXT("Strength"), 0.18f);
		return;
	}
	if (ReboltT >= 0.05f) { ReboltT = 0.f; Rebolt(); }
}

// ------------------------------------------------------------------------------------------------ trail FX

AZoogiPortalTrail::AZoogiPortalTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(20000.f);
	Light->SetAttenuationRadius(400.f);
	Light->SetCastShadows(false);
	Light->SetLightColor(PortalCyan);
}

void AZoogiPortalTrail::Follow(AActor* InTarget, float InRadius, int32 Seed)
{
	Target = InTarget;
	Radius = InRadius;
	Serial = Seed;
	UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
	UMaterialInstanceDynamic* Hot = ZoogiUtil::FxMID(this, PortalHot, 12.f);
	UMaterialInstanceDynamic* Cy = ZoogiUtil::FxMID(this, PortalCyan, 8.f);
	for (int32 i = 0; i < 18; ++i)
	{
		UStaticMeshComponent* C = NewSeg(this, RootComponent, Cube);
		C->SetMaterial(0, i < 6 ? Hot : Cy);
		Segs.Add(C);
	}
	if (InTarget) LastPos = InTarget->GetActorLocation();
}

void AZoogiPortalTrail::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	ReboltT += Dt;
	AActor* T = Target.Get();
	const float Life = 0.7f;
	if (!T || Age > Life || T->IsHidden()) { Destroy(); return; }
	// follow the object's visual centre (ball / orb sphere component)
	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(T->GetRootComponent());
	const FVector C = Prim ? Prim->GetComponentLocation() : T->GetActorLocation();
	FVector V = Prim ? Prim->GetPhysicsLinearVelocity() : (C - LastPos) / FMath::Max(Dt, 1e-3f);
	LastPos = C;
	SetActorLocation(C);
	if (ReboltT < 0.04f) return;
	ReboltT = 0.f;
	++Serial;
	const float K = 1.f - Age / Life;
	Light->SetIntensity(K * 25000.f);
	FVector Back = -V.GetSafeNormal();
	if (Back.IsNearlyZero()) Back = -T->GetActorForwardVector();
	FRandomStream R(Serial * 104729 + 3);
	TArray<FVector> P;
	int32 S = 0;
	// 3 jagged streaks streaming back from the rim of the object
	for (int32 b = 0; b < 3; ++b)
	{
		const FVector Side = FVector::CrossProduct(Back, FVector::UpVector).GetSafeNormal();
		const FVector A = C + (Side * R.FRandRange(-0.8f, 0.8f) + FVector::UpVector * R.FRandRange(-0.4f, 0.8f)) * Radius;
		const FVector B = A + Back * (Radius * 3.4f * K + 40.f) + FVector::UpVector * R.FRandRange(-20.f, 20.f);
		Bolt(P, A, B, 6, Radius * 0.35f, FVector::UpVector, R);
		for (int32 i = 0; i + 1 < P.Num() && S < Segs.Num(); ++i) PlaceSeg(Segs[S++], P[i], P[i + 1], (b == 0 ? 5.f : 3.f) * (0.4f + 0.6f * K));
	}
	for (; S < Segs.Num(); ++S) Segs[S]->SetVisibility(false);
}

// ------------------------------------------------------------------------------------------------ portals (game logic)

bool UZoogiPortalSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

TStatId UZoogiPortalSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiPortalSubsystem, STATGROUP_Tickables);
}

bool UZoogiPortalSubsystem::Enabled()
{
	static int32 Cached = -1;
	if (Cached < 0)
	{
		bool bOn = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Portals"), TEXT("bPortals"), bOn, GGameIni);
		FString V;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiPortals="), V))
			bOn = V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V.Equals(TEXT("1")) || V.Equals(TEXT("True"), ESearchCase::IgnoreCase);
		Cached = bOn ? 1 : 0;
		UE_LOG(LogZoogi, Log, TEXT("PORTAL boost portals=%s"), bOn ? TEXT("on") : TEXT("off"));
	}
	return Cached == 1;
}

void UZoogiPortalSubsystem::Clear()
{
	for (FPortal& P : Portals) if (AZoogiPortalFX* F = P.FX.Get()) F->Destroy();
	Portals.Reset();
	for (auto& M : Prev) M.Reset();
	LastFire.Reset();
	BuiltFor = nullptr;
}

void UZoogiPortalSubsystem::Build(AZoogiArena* Arena)
{
	Clear();
	BuiltFor = Arena;
	if (!bLoaded)
	{
		bLoaded = true;
		if (GConfig)
		{
			GConfig->GetFloat(TEXT("ZoogiRoll.Portals"), TEXT("BoostAdd"), BoostAdd, GGameIni);
			GConfig->GetFloat(TEXT("ZoogiRoll.Portals"), TEXT("MaxSpeed"), MaxSpeed, GGameIni);
			GConfig->GetFloat(TEXT("ZoogiRoll.Portals"), TEXT("MinSpeed"), MinSpeed, GGameIni);
			GConfig->GetFloat(TEXT("ZoogiRoll.Portals"), TEXT("Cooldown"), Cooldown, GGameIni);
		}
	}
	UWorld* W = GetWorld();
	if (!W || !Arena) return;
	using namespace ZoogiFrozen;
	for (AZoogiArenaPiece* Pc : Arena->GetPieces())
	{
		if (!Pc || Pc->GetKind() != FName(TEXT("arch")) || Portals.Num() >= UE_ARRAY_COUNT(Prev)) continue;
		FPortal P;
		P.Arch = Pc;
		const FTransform T = Pc->GetActorTransform();
		const float S = T.GetScale3D().Z;
		P.X = FTransform(T.GetRotation(), T.GetLocation());
		P.HalfW = (ArchHalfW - ArchPostR) * T.GetScale3D().Y;
		P.Top = (ArchH - 120.f + ArchHalfW) * S;
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.GetLocation().Y, T.GetLocation().X));
		P.Name = FString::Printf(TEXT("arch_%03.0f"), FRotator::ClampAxis(Yaw));
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AZoogiPortalFX* F = W->SpawnActor<AZoogiPortalFX>(AZoogiPortalFX::StaticClass(), P.X, SP)) { F->Setup(P.HalfW, P.Top); P.FX = F; }
		Portals.Add(P);
	}
	FString L;
	for (const FPortal& P : Portals) L += FString::Printf(TEXT(" %s(r%.0f half_w=%.0f top=%.0f)"), *P.Name, P.X.GetLocation().Size2D(), P.HalfW, P.Top);
	UE_LOG(LogZoogi, Log, TEXT("PORTAL built %d boost=+%.0f cap=%.0f cooldown=%.2f:%s"), Portals.Num(), BoostAdd, MaxSpeed, Cooldown, *L);
}

int32 UZoogiPortalSubsystem::FireCount(const FString& Who, double Since) const
{
	int32 N = 0;
	for (const FZoogiPortalFire& F : Fires) if (F.Who == Who && F.Time >= Since) ++N;
	return N;
}

const FZoogiPortalFire* UZoogiPortalSubsystem::FirstFire(const FString& Who, double Since) const
{
	for (const FZoogiPortalFire& F : Fires) if (F.Who == Who && F.Time >= Since) return &F;
	return nullptr;
}

void UZoogiPortalSubsystem::Tick(float DeltaTime)
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!bLoaded) FParse::Value(FCommandLine::Get(), TEXT("ZoogiPortalShots="), ShotDir);
	for (int32 i = ShotsLater.Num() - 1; i >= 0; --i)
		if (FPlatformTime::Seconds() >= ShotsLater[i].Value)
		{
			FScreenshotRequest::RequestScreenshot(ShotsLater[i].Key, true, false);
			UE_LOG(LogZoogi, Log, TEXT("PORTAL screenshot requested: %s"), *ShotsLater[i].Key);
			ShotsLater.RemoveAt(i);
			break; // one request per frame
		}
	const bool bWant = Enabled() && G && G->Arena && !G->bComboMode && G->Arena->GetTheme() == EZoogiArena::Frozen;
	if (!bWant) { if (Portals.Num() || BuiltFor.IsValid()) Clear(); return; }
	if (BuiltFor.Get() != G->Arena.Get() || Portals.Num() == 0)
	{
		// the arches are spawned with the arena; build once they exist
		bool bAny = false;
		for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces()) if (Pc && Pc->GetKind() == FName(TEXT("arch"))) { bAny = true; break; }
		if (!bAny) return;
		Build(G->Arena);
	}
	const bool bLive = G->Phase == EZoogiPhase::Rolling;
	const double Now = W->GetTimeSeconds();
	struct FObj { AActor* A; UPrimitiveComponent* Body; FVector L; float R; bool bOrb; int32 Idx; };
	TArray<FObj> Objs;
	for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->Sphere) Objs.Add({ B, B->Sphere, B->BallLocation(), ZoogiConst::ZoogiRadius, false, B->PlayerIndex });
	for (int32 i = 0; i < G->Orbs.Num(); ++i) if (AZoogiOrb* O = G->Orbs[i]) if (!O->bOut && O->Sphere) Objs.Add({ O, O->Sphere, O->OrbLocation(), ZoogiConst::OrbRadius, true, i });
	for (int32 p = 0; p < Portals.Num(); ++p)
	{
		FPortal& P = Portals[p];
		for (const FObj& Ob : Objs)
		{
			const FVector L = P.X.InverseTransformPosition(Ob.L);
			FVector* Pv = Prev[p].Find(Ob.A);
			const bool bCross = Pv && ((Pv->X < 0.f) != (L.X < 0.f)) && FVector::DistSquared(*Pv, L) < FMath::Square(600.f); // no teleports
			const FVector Mid = Pv ? (*Pv + L) * 0.5f : L;
			Prev[p].Add(Ob.A, L);
			if (!bCross || !bLive || FMath::Abs(Mid.Y) > P.HalfW || Mid.Z < -20.f || Mid.Z > P.Top) continue;
			if (const double* Lf = LastFire.Find(Ob.A)) if (Now - *Lf < Cooldown) continue;
			const FVector V = Ob.Body->GetPhysicsLinearVelocity();
			const FVector2D V2(V.X, V.Y);
			const float S = V2.Size();
			if (S < MinSpeed || S >= MaxSpeed) continue;
			const float NewS = FMath::Min(S + BoostAdd, MaxSpeed);
			const FVector2D D = V2 / S;
			const FVector VOut(D.X * NewS, D.Y * NewS, V.Z);
			Ob.Body->SetPhysicsLinearVelocity(VOut);
			LastFire.Add(Ob.A, Now);
			FZoogiPortalFire F;
			F.Portal = p; F.bOrb = Ob.bOrb; F.Index = Ob.Idx; F.Who = FString::Printf(TEXT("%s%d"), Ob.bOrb ? TEXT("O") : TEXT("P"), Ob.Idx + 1);
			F.VIn = V; F.VOut = VOut; F.Time = Now;
			Fires.Add(F);
			UE_LOG(LogZoogi, Log, TEXT("PORTAL fire %s %s in=%.0f out=%.0f dir=(%.3f,%.3f) vz=%.0f at=(%.0f,%.0f,%.0f) t=%.2f"), *P.Name, *F.Who, S, NewS, D.X, D.Y, V.Z, Ob.L.X, Ob.L.Y, Ob.L.Z, Now);
			if (AZoogiPortalFX* FX = P.FX.Get()) FX->Surge(FVector(D, 0.f));
			if (!ShotDir.IsEmpty() && ShotCount < 6)
				for (const double Dl : { 0.05, 0.16 })
					ShotsLater.Add(TPair<FString, double>(FPaths::Combine(ShotDir, FString::Printf(TEXT("portal_%02d_%s_%s.png"), ++ShotCount, *P.Name, *F.Who)), FPlatformTime::Seconds() + Dl));
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AZoogiPortalTrail* Tr = W->SpawnActor<AZoogiPortalTrail>(AZoogiPortalTrail::StaticClass(), FTransform(Ob.L), SP)) Tr->Follow(Ob.A, Ob.R, Fires.Num() * 31 + p);
		}
	}
}
