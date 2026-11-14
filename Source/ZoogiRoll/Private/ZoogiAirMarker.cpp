// Oct 22: see ZoogiAirMarker.h
#include "ZoogiAirMarker.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace
{
	struct FAirTune
	{
		bool bEnabled = true, bShot = false, bHop = false;
		float CoreStrength = 16.f, GoldStrength = 10.f, PulseStrength = 18.f, RingWidth = 12.f, OutlineWidth = 6.f;
		float MinGap = 22.f, Radius = 58.f, RadiusPerM = 12.f, MaxRadius = 95.f, FadeIn = 0.1f, FadeOut = 0.2f, TraceDepth = 6000.f;
		FAirTune()
		{
			const TCHAR* S = TEXT("ZoogiRoll.AirMarker");
			if (GConfig)
			{
				GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
				GConfig->GetFloat(S, TEXT("MinGap"), MinGap, GGameIni);
				GConfig->GetFloat(S, TEXT("Radius"), Radius, GGameIni);
				GConfig->GetFloat(S, TEXT("RadiusPerM"), RadiusPerM, GGameIni);
				GConfig->GetFloat(S, TEXT("MaxRadius"), MaxRadius, GGameIni);
				GConfig->GetFloat(S, TEXT("FadeIn"), FadeIn, GGameIni);
				GConfig->GetFloat(S, TEXT("FadeOut"), FadeOut, GGameIni);
				GConfig->GetFloat(S, TEXT("CoreStrength"), CoreStrength, GGameIni);
				GConfig->GetFloat(S, TEXT("GoldStrength"), GoldStrength, GGameIni);
				GConfig->GetFloat(S, TEXT("PulseStrength"), PulseStrength, GGameIni);
				GConfig->GetFloat(S, TEXT("RingWidth"), RingWidth, GGameIni);
				GConfig->GetFloat(S, TEXT("OutlineWidth"), OutlineWidth, GGameIni);
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAirMarker"))) bEnabled = false;
			bShot = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAirMarkerShot"));
			bHop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAirMarkerHop")); // test only: pops a resting Zoogi into the air every 4 s
			FadeIn = FMath::Max(FadeIn, 0.01f); FadeOut = FMath::Max(FadeOut, 0.01f);
		}
	};
	const FAirTune& AirTune() { static FAirTune T; return T; }

	constexpr int32 RingSegs = 32, Bars = 4, Brackets = 4;
	constexpr int32 SegsPerMarker = RingSegs + Bars + Brackets * 2;
	constexpr int32 CoreSegs = RingSegs + Bars, PulseRings = 2;
	const FLinearColor AirBright(1.f, 0.62f, 0.08f); // warm gold: reads on ice, sand, grass, cosmic and lava floors
}

namespace ZoogiAirMarker
{
	void Ensure(UWorld* World)
	{
		if (!World || !World->IsGameWorld() || !AirTune().bEnabled) return;
		static TWeakObjectPtr<UWorld> GAirWorld;
		static TWeakObjectPtr<AZoogiAirMarkers> GMarkers;
		if (GAirWorld.Get() == World && GMarkers.IsValid()) return;
		GAirWorld = World;
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GMarkers = World->SpawnActor<AZoogiAirMarkers>(AZoogiAirMarkers::StaticClass(), FTransform::Identity, P);
		UE_LOG(LogZoogi, Log, TEXT("AIRMARKER manager up"));
	}
}

AZoogiAirMarkers::AZoogiAirMarkers()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after the balls moved this frame
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
}

UStaticMeshComponent* AZoogiAirMarkers::NewSeg(UMaterialInterface* M, int32 SortPriority)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->bReceivesDecals = false;
	C->SetupAttachment(RootComponent);
	C->SetAbsolute(true, true, true);
	if (M) C->SetMaterial(0, M);
	C->SetTranslucentSortPriority(SortPriority);
	C->bRenderInDepthPass = SortPriority == 0; // pulses: no depth write, drawn after the floor
	C->bRenderCustomDepth = false;
	C->SetVisibility(false);
	C->RegisterComponent();
	Parts.Add(C);
	return C;
}

AZoogiAirMarkers::FEntry& AZoogiAirMarkers::EntryFor(AZoogiBall* B)
{
	for (FEntry& E : Entries) if (E.Ball.Get() == B) return E;
	const FAirTune& T = AirTune();
	if (!MatBright) MatBright = ZoogiUtil::GlowMID(this, AirBright, T.GoldStrength); // opaque emissive: no additive wash-out on bright floors
	if (!MatCore) MatCore = ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.97f, 0.9f), T.CoreStrength);
	if (!MatDark)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		UMaterialInstanceDynamic* D = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
		if (D) D->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.004f, 0.004f, 0.006f));
		MatDark = D ? (UMaterialInterface*)D : Base;
	}
	FEntry& E = Entries.AddDefaulted_GetRef();
	E.Ball = B;
	for (int32 i = 0; i < SegsPerMarker; ++i) { E.Dark.Add(NewSeg(MatDark)); E.Bright.Add(NewSeg(MatBright)); }
	for (int32 i = 0; i < CoreSegs; ++i) E.Core.Add(NewSeg(MatCore));
	const FLinearColor Tint = FMath::Lerp(AirBright, ZoogiUtil::PlayerColor(B ? B->PlayerIndex : 0), 0.45f);
	for (int32 k = 0; k < PulseRings; ++k)
	{
		E.PulseMat[k] = ZoogiUtil::FxMID(this, Tint, T.PulseStrength);
		PulseMats.Add(E.PulseMat[k]);
		for (int32 i = 0; i < RingSegs; ++i) E.Pulse.Add(NewSeg(E.PulseMat[k], 100));
	}
	return E;
}

// lay one stroke (cube 100 cm) from A to B on the plane, width W, height H
static void Stroke(UStaticMeshComponent* C, const FVector& A, const FVector& B, const FVector& N, float W, float H)
{
	const FVector D = B - A;
	const float L = D.Size();
	if (L < 0.1f || W <= 0.01f) { C->SetVisibility(false); return; }
	const FRotator R = FRotationMatrix::MakeFromXZ(D / L, N).Rotator();
	C->SetWorldLocationAndRotation((A + B) * 0.5f, R);
	C->SetWorldScale3D(FVector(L / 100.f, W / 100.f, H / 100.f));
	C->SetVisibility(true);
}

void AZoogiAirMarkers::Layout(FEntry& E)
{
	const FAirTune& T = AirTune();
	const bool bShow = E.bHasPoint && E.Alpha > 0.001f;
	if (!bShow) { for (UStaticMeshComponent* C : E.Bright) C->SetVisibility(false); for (UStaticMeshComponent* C : E.Dark) C->SetVisibility(false); for (UStaticMeshComponent* C : E.Core) C->SetVisibility(false); for (UStaticMeshComponent* C : E.Pulse) C->SetVisibility(false); return; }
	const FVector N = E.Normal;
	FVector X = FVector::CrossProduct(N, FVector::RightVector);
	if (X.SizeSquared() < 0.01f) X = FVector::CrossProduct(N, FVector::ForwardVector);
	X.Normalize();
	const FVector Y = FVector::CrossProduct(N, X).GetSafeNormal();
	const float A = E.Alpha;
	const float Ease = FMath::InterpEaseOut(0.f, 1.f, A, 2.f);
	const float R = FMath::Min(T.Radius + T.RadiusPerM * E.Gap / 100.f, T.MaxRadius) * FMath::Lerp(1.35f, 1.f, Ease);
	const float Spin = E.Age * 1.2f;
	const FVector OD = E.Point + N * 1.0f; // stacked layers, each well clear of the floor and the one below (no z-fight)
	const FVector O = E.Point + N * 2.0f;
	const FVector OC = E.Point + N * 3.0f;
	const FVector OP = E.Point + N * 3.6f;
	const float W = T.RingWidth * A, WD = W + 2.f * T.OutlineWidth * A, WC = W * 0.38f; // gold stroke, dark outline each side, white core
	auto P = [&](const FVector& C, float Rad, float Ang) { return C + (X * FMath::Cos(Ang) + Y * FMath::Sin(Ang)) * Rad; };
	int32 S = 0;
	for (int32 i = 0; i < RingSegs; ++i, ++S)
	{
		const float A0 = i * 2.f * PI / RingSegs, A1 = (i + 1) * 2.f * PI / RingSegs + 0.02f;
		Stroke(E.Bright[S], P(O, R, A0), P(O, R, A1), N, W, 1.f);
		Stroke(E.Dark[S], P(OD, R, A0), P(OD, R, A1), N, WD, 1.f);
		Stroke(E.Core[S], P(OC, R, A0), P(OC, R, A1), N, WC, 1.f);
	}
	for (int32 k = 0; k < Bars; ++k, ++S) // crosshair bars: gap at the centre, run out past the ring
	{
		const float Ang = Spin + k * PI * 0.5f;
		Stroke(E.Bright[S], P(O, R * 0.28f, Ang), P(O, R * 1.25f, Ang), N, W * 0.85f, 1.f);
		Stroke(E.Dark[S], P(OD, R * 0.28f - 4.f, Ang), P(OD, R * 1.25f + 4.f, Ang), N, WD * 0.9f, 1.f);
		Stroke(E.Core[S], P(OC, R * 0.3f, Ang), P(OC, R * 1.22f, Ang), N, WC * 0.85f, 1.f);
	}
	for (int32 k = 0; k < Brackets; ++k) // small inner ticks between the bars (two strokes, a chevron)
	{
		const float Ang = Spin + PI * 0.25f + k * PI * 0.5f;
		const FVector Tip = P(O, R * 0.55f, Ang), TipD = P(OD, R * 0.55f, Ang);
		for (int32 s = -1; s <= 1; s += 2, ++S)
		{
			Stroke(E.Bright[S], Tip, P(O, R * 0.75f, Ang + s * 0.22f), N, W * 0.7f, 1.f);
			Stroke(E.Dark[S], TipD, P(OD, R * 0.75f, Ang + s * 0.22f), N, WD * 0.75f, 1.f);
		}
	}
	// pulse rings: expand out of the main ring and fade; higher = faster and wider (two staggered rings)
	const float Hgt = FMath::Clamp(E.Gap / 400.f, 0.f, 1.f);
	const float Rate = 0.9f + 1.8f * Hgt, Reach = 0.55f + 0.95f * Hgt;
	for (int32 k = 0; k < PulseRings; ++k)
	{
		const float Ph = FMath::Frac(E.Age * Rate + k * 0.5f);
		const float PR = R * (1.f + Ph * Reach), PW = (5.f + 6.f * Hgt) * (1.f - Ph) * A;
		if (E.PulseMat[k]) E.PulseMat[k]->SetScalarParameterValue(TEXT("Strength"), T.PulseStrength * (1.f - Ph * Ph) * A);
		for (int32 i = 0; i < RingSegs; ++i)
		{
			const float A0 = i * 2.f * PI / RingSegs, A1 = (i + 1) * 2.f * PI / RingSegs + 0.02f;
			Stroke(E.Pulse[k * RingSegs + i], P(OP, PR, A0), P(OP, PR, A1), N, PW, 1.f);
		}
	}
}

void AZoogiAirMarkers::Tick(float Dt)
{
	Super::Tick(Dt);
	const FAirTune& T = AirTune();
	UWorld* W = GetWorld();
	if (!W) return;
	TArray<AZoogiBall*> Live;
	for (TActorIterator<AZoogiBall> It(W); It; ++It) if (!It->bOut && !It->bKnockedOut && !It->IsHidden()) Live.Add(*It);
	if (T.bHop && Live.Num() > 0)
	{
		HopT += Dt;
		if (HopT > 4.f)
		{
			HopT = 0.f;
			AZoogiBall* B = Live[HopN++ % Live.Num()];
			if (B && B->Sphere && B->Sphere->IsSimulatingPhysics())
			{
				B->Sphere->WakeRigidBody();
				B->Sphere->SetPhysicsLinearVelocity(B->Sphere->GetPhysicsLinearVelocity() + FVector(0, 0, 620.f));
				UE_LOG(LogZoogi, Log, TEXT("AIRMARKER test hop P%d"), B->PlayerIndex + 1);
			}
		}
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiAirMarker), false);
	for (AZoogiBall* B : Live) Q.AddIgnoredActor(B);
	for (AZoogiBall* B : Live)
	{
		FEntry& E = EntryFor(B);
		E.Age += Dt;
		const FVector C = B->BallLocation();
		FHitResult H;
		const bool bHit = W->LineTraceSingleByChannel(H, C, C - FVector(0, 0, T.TraceDepth), ECC_Visibility, Q);
		bool bAir = false;
		if (bHit)
		{
			E.Gap = FMath::Max(0.f, (C.Z - H.ImpactPoint.Z) - ZoogiConst::ZoogiRadius);
			bAir = E.Gap > T.MinGap && H.ImpactNormal.Z > 0.3f;
			if (bAir) { E.Point = H.ImpactPoint; E.Normal = H.ImpactNormal; E.bHasPoint = true; }
		}
		if (bAir && !E.bAir) { E.bLogged = false; UE_LOG(LogZoogi, Log, TEXT("AIRMARKER show P%d gap %.0f cm on %s"), B->PlayerIndex + 1, E.Gap, H.GetActor() ? *H.GetActor()->GetName() : TEXT("?")); }
		if (!bAir && E.bAir) UE_LOG(LogZoogi, Log, TEXT("AIRMARKER landed P%d (fade %.2f s)"), B->PlayerIndex + 1, T.FadeOut);
		E.bAir = bAir;
		E.Alpha = FMath::Clamp(E.Alpha + (bAir ? Dt / T.FadeIn : -Dt / T.FadeOut), 0.f, 1.f);
		Layout(E);
		if (bAir && E.Alpha >= 1.f && !E.bLogged)
		{
			E.bLogged = true;
			UE_LOG(LogZoogi, Log, TEXT("AIRMARKER visible P%d gap %.0f cm at (%.0f, %.0f, %.0f) normal (%.2f, %.2f, %.2f)"), B->PlayerIndex + 1, E.Gap, E.Point.X, E.Point.Y, E.Point.Z, E.Normal.X, E.Normal.Y, E.Normal.Z);
		}
		if (T.bShot && ShotN < 3 && W->GetRealTimeSeconds() - LastShotAt > 2.5f && bAir && E.Alpha >= 1.f && E.Gap > 45.f)
		{
			const AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>();
			if (G && !G->ShotDir.IsEmpty())
			{
				++ShotN; LastShotAt = W->GetRealTimeSeconds();
				const FString Pth = FPaths::Combine(G->ShotDir, FString::Printf(TEXT("airmarker_%d_P%d.png"), ShotN, B->PlayerIndex + 1));
				FScreenshotRequest::RequestScreenshot(Pth, false, false); // test capture: scene only, no HUD / comic overlays
				UE_LOG(LogZoogi, Log, TEXT("AIRMARKER shot %s"), *Pth);
			}
		}
	}
	// Zoogis gone (ring-out, round reset): fade + hide
	for (FEntry& E : Entries) if (!E.Ball.IsValid() || !Live.Contains(E.Ball.Get())) { E.bAir = false; E.Alpha = FMath::Max(0.f, E.Alpha - Dt / T.FadeOut); Layout(E); }
}
