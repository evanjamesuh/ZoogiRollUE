#include "ZoogiAerialProps.h"
#include "ZoogiAerialFX.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiMeshKit.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ProceduralMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

AZoogiAerialProp::AZoogiAerialProp()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FString AZoogiAerialProp::Label() const
{
	const TCHAR* K = Kind == EZoogiPropKind::SkyBumper ? TEXT("sky_bumper") : (Kind == EZoogiPropKind::ResonanceHalo ? TEXT("resonance_halo") : (Kind == EZoogiPropKind::Gate ? TEXT("recovery_gate") : TEXT("launch_rail")));
	return FString::Printf(TEXT("%s_%d"), K, PropId);
}

void AZoogiAerialProp::Build(EZoogiPropKind InKind, int32 Id, const FVector& Location, const FVector& InForward, float InSize)
{
	Kind = InKind;
	PropId = Id;
	Forward = InForward.GetSafeNormal();
	Size = InSize;
	SetActorLocation(Location);
	auto AddSolid = [this](const FVector& Rel, float R, FName CompTag)
	{
		USphereComponent* S = NewObject<USphereComponent>(this);
		S->SetupAttachment(RootComponent);
		S->SetSphereRadius(R);
		S->SetRelativeLocation(Rel);
		S->SetCollisionProfileName(TEXT("BlockAll"));
		S->SetNotifyRigidBodyCollision(true);
		S->SetGenerateOverlapEvents(false);
		S->ComponentTags.Add(CompTag);
		S->RegisterComponent();
		S->OnComponentHit.AddDynamic(this, &AZoogiAerialProp::OnSolidHit);
		Solids.Add(S);
	};
	if (Kind == EZoogiPropKind::SkyBumper)
	{
		AddSolid(FVector::ZeroVector, Size, TEXT("ZBumper"));
	}
	else if (Kind == EZoogiPropKind::ResonanceHalo && bAerialRing)
	{
		// hanging Zone 2 halo: full vertical ring centred on Location, pass trigger at the ring centre
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		for (int32 a = 0; a < 360; a += 20)
		{
			const float Rad = FMath::DegreesToRadians((float)a);
			AddSolid(Side * FMath::Cos(Rad) * Size + FVector(0, 0, FMath::Sin(Rad) * Size), 15.f, TEXT("ZHaloRim"));
		}
		Trigger = NewObject<UBoxComponent>(this);
		Trigger->SetupAttachment(RootComponent);
		Trigger->SetBoxExtent(FVector(10.f, Size * 0.6f, Size * 0.6f));
		Trigger->SetRelativeLocation(FVector::ZeroVector);
		Trigger->SetRelativeRotation(Forward.Rotation());
	}
	else if (Kind == EZoogiPropKind::Gate)
	{
		// Zone 3 recovery gate: trigger only (no solids), a plane across the bridge deck
		Trigger = NewObject<UBoxComponent>(this);
		Trigger->SetupAttachment(RootComponent);
		Trigger->SetBoxExtent(FVector(12.f, Size * 0.8f, Size));
		Trigger->SetRelativeLocation(FVector(0, 0, Size * 0.5f));
		Trigger->SetRelativeRotation(Forward.Rotation());
	}
	else if (Kind == EZoogiPropKind::ResonanceHalo)
	{
		// vertical ring in the plane (Side, Up), open at the bottom; the ring centre sits Size above Location
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		for (int32 a = -50; a <= 230; a += 20)
		{
			const float Rad = FMath::DegreesToRadians((float)a);
			AddSolid(Side * FMath::Cos(Rad) * Size + FVector(0, 0, Size + FMath::Sin(Rad) * Size), 15.f, TEXT("ZHaloRim"));
		}
		Trigger = NewObject<UBoxComponent>(this);
		Trigger->SetupAttachment(RootComponent);
		Trigger->SetBoxExtent(FVector(10.f, Size * 0.45f, 45.f));
		Trigger->SetRelativeLocation(FVector(0, 0, 55.f));
		Trigger->SetRelativeRotation(Forward.Rotation());
	}
	else
	{
		Trigger = NewObject<UBoxComponent>(this);
		Trigger->SetupAttachment(RootComponent);
		Trigger->SetBoxExtent(FVector(Size, 70.f, 70.f));
		Trigger->SetRelativeLocation(FVector(0, 0, 55.f));
		Trigger->SetRelativeRotation(Forward.Rotation());
	}
	if (Trigger)
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision); // a debug volume: never touches the simulation
		Trigger->SetGenerateOverlapEvents(false);
		Trigger->RegisterComponent();
	}
	UE_LOG(LogZoogi, Log, TEXT("BATTLE prop spawn %s at (%.0f,%.0f,%.0f) fwd=(%.2f,%.2f) size=%.0f solids=%d%s"), *Label(), Location.X, Location.Y, Location.Z, Forward.X, Forward.Y, Size, Solids.Num(),
		bAerialRing ? TEXT(" aerial_ring") : TEXT(""));
}

bool AZoogiAerialProp::Cooldown(AActor* A, float Seconds, int32 Channel)
{
	TMap<TWeakObjectPtr<AActor>, float>& M = LastEvent[Channel ? 1 : 0];
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (const float* L = M.Find(A)) if (Now - *L < Seconds) return false;
	M.Add(A, Now);
	return true;
}

void AZoogiAerialProp::OnSolidHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AZoogiBall* B = Cast<AZoogiBall>(OtherActor);
	if (!B || !HitComp || !Cooldown(B, 0.25f)) return;
	UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	if (OtherComp && B->Sphere && OtherComp != B->Sphere) return;
	if (Kind == EZoogiPropKind::SkyBumper && B->Sphere)
	{
		// deterministic rebound: normal from the bumper centre to the ball centre (2D for a floor bumper), reflect, x ReboundMul
		const FVector C = HitComp->GetComponentLocation();
		FVector N = B->BallLocation() - C;
		const bool bFloor = C.Z < 150.f;
		if (bFloor) N.Z = 0.f;
		N = N.GetSafeNormal();
		const FVector VIn = B->PrevVelocity.SizeSquared() > 1.f ? B->PrevVelocity : B->Sphere->GetPhysicsLinearVelocity();
		FVector R = VIn - 2.f * FVector::DotProduct(VIn, N) * N;
		if (FVector::DotProduct(R, N) <= 0.f) R = N * VIn.Size();
		if (bFloor) R.Z = 0.f;
		const float Speed = FMath::Min(VIn.Size() * ReboundMul, ReboundMaxSpeed);
		const FVector Out = R.GetSafeNormal() * Speed;
		B->Sphere->SetPhysicsLinearVelocity(Out);
		LastIn = VIn; LastOut = Out; LastNormal = N; ++Rebounds;
		// Oct 9: cosmetic star-burst + ice shards at the contact (visual only, own random stream)
		AZoogiImpactFlash::Spawn(GetWorld(), C + N * Size, N, PropId * 131 + Rebounds, FMath::Clamp(Size / 70.f, 0.8f, 1.4f));
		UE_LOG(LogZoogi, Log, TEXT("BATTLE skybumper %s P%d in=(%.0f,%.0f,%.0f)|%.0f| n=(%.2f,%.2f,%.2f) out=(%.0f,%.0f,%.0f)|%.0f| mul=%.2f"), *Label(), B->PlayerIndex + 1,
			VIn.X, VIn.Y, VIn.Z, VIn.Size(), N.X, N.Y, N.Z, Out.X, Out.Y, Out.Z, Out.Size(), ReboundMul);
		if (SM) SM->OnPropEvent(this, B, TEXT("bumper"), B->BallLocation(), VIn.Size(), Out.Size());
	}
	else if (Kind == EZoogiPropKind::ResonanceHalo)
	{
		UE_LOG(LogZoogi, Log, TEXT("BATTLE halo %s rim P%d at (%.0f,%.0f,%.0f)"), *Label(), B->PlayerIndex + 1, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
		if (SM) SM->OnPropEvent(this, B, TEXT("rim"), FVector(Hit.ImpactPoint), 0.f, 0.f);
	}
}

bool AZoogiAerialProp::InTrigger(const FVector& Local) const
{
	if (!Trigger) return false;
	const FVector E = Trigger->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

void AZoogiAerialProp::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* W = GetWorld();
	if (!W) return;
	const FColor C = Kind == EZoogiPropKind::SkyBumper ? FColor::Orange : (Kind == EZoogiPropKind::ResonanceHalo ? FColor::Cyan : (Kind == EZoogiPropKind::Gate ? FColor::Magenta : FColor::Green));
	for (const USphereComponent* S : Solids) if (S) DrawDebugSphere(W, S->GetComponentLocation(), S->GetScaledSphereRadius(), 10, C, false, -1.f, 0, 1.5f);
	if (Trigger) DrawDebugBox(W, Trigger->GetComponentLocation(), Trigger->GetScaledBoxExtent(), Trigger->GetComponentQuat(), C, false, -1.f, 0, 1.f);
	if (Kind == EZoogiPropKind::LaunchRail) DrawDebugDirectionalArrow(W, GetActorLocation() + FVector(0, 0, 20), GetActorLocation() + FVector(0, 0, 20) + Forward * Size * 2.f, 40.f, C, false, -1.f, 0, 3.f);
#endif
}

// ------------------------------------------------------------------------------------------------ placeholder visuals
// Oct 8 (Frozen map layout): simple clean geometry with the arena's Frozen materials. Every visual mesh is NoCollision,
// no overlaps, no navigation: the gameplay shapes stay the solids / triggers above (RULE ZERO: visuals never touch physics).

namespace ZoogiAerialVis
{
	/** Tube ring (radius R, tube r) around C in the plane (Ax, Ay), arc A0..A1 deg, smooth normals. */
	void Torus(FZoogiMeshBuilder& M, const FVector& C, const FVector& Ax, const FVector& Ay, float R, float r, float A0, float A1, int32 Seg, int32 Tube)
	{
		const FVector Ax2 = FVector::CrossProduct(Ax, Ay).GetSafeNormal();
		TArray<int32> Prev, Cur;
		for (int32 i = 0; i <= Seg; ++i)
		{
			const float A = FMath::DegreesToRadians(FMath::Lerp(A0, A1, (float)i / Seg));
			const FVector D = Ax * FMath::Cos(A) + Ay * FMath::Sin(A);
			Cur.Reset();
			for (int32 j = 0; j < Tube; ++j)
			{
				const float B = 2.f * PI * j / Tube;
				const FVector Nn = D * FMath::Cos(B) + Ax2 * FMath::Sin(B);
				Cur.Add(M.Add(C + D * R + Nn * r, Nn, FVector2D((float)i / Seg * 8.f, (float)j / Tube)));
			}
			if (Prev.Num())
				for (int32 j = 0; j < Tube; ++j)
				{
					const int32 j2 = (j + 1) % Tube;
					const FVector Out = (M.N[Prev[j]] + M.N[Cur[j2]]).GetSafeNormal();
					M.Tri(Prev[j], Cur[j], Cur[j2], Out);
					M.Tri(Prev[j], Cur[j2], Prev[j2], Out);
				}
			Prev = Cur;
		}
	}
	void Sphere(FZoogiMeshBuilder& M, const FVector& C, float R, int32 Seg = 16)
	{
		TArray<int32> Prev, Cur;
		const int32 Rings = FMath::Max(4, Seg / 2);
		for (int32 i = 0; i <= Rings; ++i)
		{
			const float Ph = PI * i / Rings - PI * 0.5f;
			Cur.Reset();
			for (int32 j = 0; j <= Seg; ++j)
			{
				const float Th = 2.f * PI * j / Seg;
				const FVector Nn(FMath::Cos(Ph) * FMath::Cos(Th), FMath::Cos(Ph) * FMath::Sin(Th), FMath::Sin(Ph));
				Cur.Add(M.Add(C + Nn * R, Nn, FVector2D((float)j / Seg, (float)i / Rings)));
			}
			if (Prev.Num())
				for (int32 j = 0; j < Seg; ++j)
				{
					const FVector Out = (M.N[Prev[j]] + M.N[Cur[j + 1]]).GetSafeNormal();
					M.Tri(Prev[j], Cur[j], Cur[j + 1], Out);
					M.Tri(Prev[j], Cur[j + 1], Prev[j + 1], Out);
				}
			Prev = Cur;
		}
	}
	/** Cylinder side between two points (cables, posts). */
	void Tube(FZoogiMeshBuilder& M, const FVector& P0, const FVector& P1, float R, int32 Sides = 8)
	{
		const FVector Ax = (P1 - P0).GetSafeNormal();
		FVector U = FVector::CrossProduct(Ax, FVector::UpVector);
		if (U.SizeSquared() < 1e-4) U = FVector::CrossProduct(Ax, FVector::ForwardVector);
		U.Normalize();
		const FVector V = FVector::CrossProduct(Ax, U);
		TArray<int32> A, B;
		for (int32 k = 0; k <= Sides; ++k)
		{
			const float T = 2.f * PI * k / Sides;
			const FVector Nn = U * FMath::Cos(T) + V * FMath::Sin(T);
			A.Add(M.Add(P0 + Nn * R, Nn, FVector2D((float)k / Sides, 0.f)));
			B.Add(M.Add(P1 + Nn * R, Nn, FVector2D((float)k / Sides, 1.f)));
		}
		for (int32 k = 0; k < Sides; ++k)
		{
			const FVector Out = (M.N[A[k]] + M.N[A[k + 1]]).GetSafeNormal();
			M.Tri(A[k], B[k], B[k + 1], Out);
			M.Tri(A[k], B[k + 1], A[k + 1], Out);
		}
	}
	/** Floating disc: stone slab (top Z, thickness H) as a 24-sided prism with a bottom. */
	void Disc(FZoogiMeshBuilder& M, const FVector& C, float R, float H)
	{
		M.Prism(FVector2D(C.X, C.Y), 24, R, R, C.Z - H, C.Z, 0.f, 0.f, true);
	}
	/** Flat ribbon along a polyline (local points), half width Hw, at height Z above each point. */
	void Ribbon(FZoogiMeshBuilder& M, const TArray<FVector>& Pts, float Off0, float Off1, float Z)
	{
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			const FVector D0 = (Pts[FMath::Min(i + 1, Pts.Num() - 1)] - Pts[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			const FVector D1 = (Pts[FMath::Min(i + 2, Pts.Num() - 1)] - Pts[i]).GetSafeNormal2D();
			const FVector S0(-D0.Y, D0.X, 0.f), S1(-D1.Y, D1.X, 0.f);
			const FVector Zv(0, 0, Z);
			M.Quad(Pts[i] + S0 * Off0 + Zv, Pts[i + 1] + S1 * Off0 + Zv, Pts[i + 1] + S1 * Off1 + Zv, Pts[i] + S0 * Off1 + Zv, FVector::UpVector);
		}
	}
	UMaterialInterface* Mat(AZoogiArena* Arena, UObject* Outer, FName Key, const FLinearColor& Fallback)
	{
		if (Arena) if (UMaterialInterface* M = Arena->FrozenMat(Key)) return M;
		return ZoogiUtil::SurfaceMID(Outer, Fallback, 0.6f);
	}
}

UProceduralMeshComponent* AZoogiAerialProp::NewVisual(UMaterialInterface* M, const FZoogiMeshBuilder& B)
{
	if (B.IsEmpty()) return nullptr;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(RootComponent);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetCollisionResponseToAllChannels(ECR_Ignore);
	P->SetGenerateOverlapEvents(false);
	P->SetCanEverAffectNavigation(false);
	P->bUseComplexAsSimpleCollision = false;
	P->RegisterComponent();
	B.Upload(P, 0, false);
	if (M) P->SetMaterial(0, M);
	Visuals.Add(P);
	return P;
}

FVector AZoogiAerialProp::UsePoint() const
{
	return GetActorLocation();
}

void AZoogiAerialProp::BuildVisuals(AZoogiArena* Arena)
{
	using namespace ZoogiAerialVis;
	for (UProceduralMeshComponent* V : Visuals) if (V) V->DestroyComponent();
	Visuals.Reset();
	const FLinearColor Cyan = ZoogiUtil::Hex(TEXT("#19C3E6"));
	const FLinearColor Orange(1.f, 0.45f, 0.08f);
	UMaterialInterface* Stone = Mat(Arena, this, TEXT("Stone"), FLinearColor(0.82f, 0.85f, 0.9f));
	UMaterialInterface* StoneDark = Mat(Arena, this, TEXT("StoneDark"), FLinearColor(0.55f, 0.6f, 0.7f));
	UMaterialInterface* Rune = Mat(Arena, this, TEXT("Rune"), Cyan);
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Up = FVector::UpVector;
	const FVector Base = GetActorLocation();
	FZoogiMeshBuilder SStone, SDark, SGlow;
	auto Hang = [&](const FVector& Top, float DiscZ)
	{
		// a floating anchor disc above the prop with two thin cables (Zone 2 dressing); local coordinates
		const FVector D(Top.X, Top.Y, DiscZ);
		Disc(SStone, D, 95.f, 18.f);
		Torus(SGlow, D + FVector(0, 0, -9.f), FVector::ForwardVector, FVector::RightVector, 95.f, 4.f, 0.f, 360.f, 40, 6);
		Tube(SDark, Top + Side * 40.f, D + Side * 40.f - FVector(0, 0, 18.f), 2.5f);
		Tube(SDark, Top - Side * 40.f, D - Side * 40.f - FVector(0, 0, 18.f), 2.5f);
	};
	// Oct 9 (Evan, refs resonance_halo_detail / sky_bumper_detail / frozen_zones_aerial): ref-style dressing. Visual only
	// (every mesh NoCollision, RULE ZERO): the solids / triggers built in Build() are unchanged.
	auto HexIce = [&](FZoogiMeshBuilder& M, const FVector& P, float S, int32 Seed)
	{
		FRandomStream R(PropId * 977 + Seed);
		M.Chunk(P, FVector(S, S * 0.8f, S * 0.6f), R, 0.35f, R.FRandRange(0.f, 360.f));
	};
	auto Icicle = [&](FZoogiMeshBuilder& M, const FVector& P, float Len, float W)
	{
		// hanging spike (world down), 5-sided cone
		M.Prism(FVector2D(P.X, P.Y), 5, W, 0.5f, P.Z - Len, P.Z, 0.f, 0.f, false);
	};
	auto Chain = [&](FZoogiMeshBuilder& M, const FVector& A, const FVector& B, float LinkR)
	{
		// alternating links along A -> B
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f) return;
		const FVector Dir = D / L;
		FVector S1 = FVector::CrossProduct(Dir, FVector::ForwardVector);
		if (S1.SizeSquared() < 0.01f) S1 = FVector::CrossProduct(Dir, FVector::RightVector);
		S1.Normalize();
		const FVector S2 = FVector::CrossProduct(Dir, S1).GetSafeNormal();
		const float Step = LinkR * 2.6f;
		int32 i = 0;
		for (float T = LinkR; T < L - LinkR * 0.5f; T += Step, ++i)
			Torus(M, A + Dir * T, Dir, (i % 2) ? S1 : S2, LinkR, LinkR * 0.28f, 0.f, 360.f, 10, 4);
	};
	auto Coil = [&](FZoogiMeshBuilder& M, const FVector& A, const FVector& B, float CoilR, float WireR, int32 Turns)
	{
		const FVector D = B - A;
		const FVector Dir = D.GetSafeNormal();
		FVector S1 = FVector::CrossProduct(Dir, FVector::UpVector);
		if (S1.SizeSquared() < 0.01f) S1 = FVector::CrossProduct(Dir, FVector::ForwardVector);
		S1.Normalize();
		const FVector S2 = FVector::CrossProduct(Dir, S1).GetSafeNormal();
		const int32 N = Turns * 10;
		FVector Prev = A + S1 * CoilR;
		for (int32 k = 1; k <= N; ++k)
		{
			const float T = (float)k / N, Ang = T * Turns * 2.f * PI;
			const FVector P = A + D * T + (S1 * FMath::Cos(Ang) + S2 * FMath::Sin(Ang)) * CoilR;
			Tube(M, Prev, P, WireR, 5);
			Prev = P;
		}
	};
	auto StoneBlocks = [&](FZoogiMeshBuilder& M, const FVector& C, const FVector& Ax, const FVector& Ay, const FVector& N, float R, float Half, float A0, float A1, int32 Count)
	{
		// carved stone ring as separate blocks with thin gaps (the art's ancient frame)
		const float Span = (A1 - A0) / Count;
		for (int32 k = 0; k < Count; ++k)
		{
			const float A = FMath::DegreesToRadians(A0 + Span * (k + 0.5f));
			const FVector Rad = Ax * FMath::Cos(A) + Ay * FMath::Sin(A);
			const FVector Tan = -Ax * FMath::Sin(A) + Ay * FMath::Cos(A);
			const FVector P = C + Rad * R;
			const float Arc = FMath::DegreesToRadians(Span) * R * 0.5f - 2.5f;
			const FRotator Rot = FRotationMatrix::MakeFromXY(Tan, Rad).Rotator();
			M.Box(P, FVector(FMath::Max(Arc, 4.f), Half, Half * 1.05f), Rot);
		}
	};
	if (Kind == EZoogiPropKind::ResonanceHalo)
	{
		// carved stone blocks around a bright cyan inner rim, stone hubs on the sides, support posts / hanging stack
		const FVector C = bAerialRing ? FVector::ZeroVector : FVector(0, 0, Size);
		const float A0 = bAerialRing ? 0.f : -50.f, A1 = bAerialRing ? 360.f : 230.f;
		Torus(SStone, C, Side, Up, Size, 15.f, A0, A1, 56, 10);
		StoneBlocks(SDark, C, Side, Up, Forward, Size + 6.f, 13.f, A0, A1, bAerialRing ? 18 : 14);
		UMaterialInterface* RimHot = ZoogiUtil::GlowMID(this, FLinearColor(0.35f, 0.95f, 1.f), 6.f);
		FZoogiMeshBuilder SRim;
		Torus(SRim, C + Forward * 10.f, Side, Up, Size - 17.f, 4.5f, A0, A1, 64, 6);
		Torus(SRim, C - Forward * 10.f, Side, Up, Size - 17.f, 4.5f, A0, A1, 64, 6);
		Torus(SGlow, C + Forward * 17.f, Side, Up, Size + 4.f, 2.5f, A0, A1, 64, 5);
		Torus(SGlow, C - Forward * 17.f, Side, Up, Size + 4.f, 2.5f, A0, A1, 64, 5);
		NewVisual(RimHot, SRim);
		// side hubs (3 / 9 o'clock): stone drums with a glowing rune cap
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector H = C + Side * s * (Size + 20.f);
			SDark.Box(H, FVector(26.f, 14.f, 26.f), Forward.Rotation());
			Torus(SGlow, H + Side * s * 15.f, Forward, Up, 11.f, 2.5f, 0.f, 360.f, 20, 5);
		}
		if (!bAerialRing)
		{
			// support posts with plinths at the open ends
			for (float A : { -50.f, 230.f })
			{
				const float R = FMath::DegreesToRadians(A);
				const FVector P = C + Side * FMath::Cos(R) * Size + Up * FMath::Sin(R) * Size;
				SDark.Box(FVector(P.X, P.Y, P.Z * 0.5f), FVector(22.f, 22.f, P.Z * 0.5f + 2.f), Forward.Rotation());
				SStone.Box(FVector(P.X, P.Y, 6.f), FVector(32.f, 32.f, 7.f), Forward.Rotation());
				Torus(SGlow, FVector(P.X, P.Y, 14.f), FVector::ForwardVector, FVector::RightVector, 26.f, 2.f, 0.f, 360.f, 20, 4);
				HexIce(SStone, FVector(P.X, P.Y, 4.f) + Forward * 30.f, 12.f, (int32)A);
			}
		}
		else
		{
			// hanging stack: a horizontal glowing ring (stone hub) above the halo, cables down to the halo's hubs
			const float TopZ = Size + 150.f;
			Torus(SStone, FVector(0, 0, TopZ), FVector::ForwardVector, FVector::RightVector, Size * 0.75f, 10.f, 0.f, 360.f, 48, 8);
			FZoogiMeshBuilder SStack;
			Torus(SStack, FVector(0, 0, TopZ - 8.f), FVector::ForwardVector, FVector::RightVector, Size * 0.75f - 12.f, 3.5f, 0.f, 360.f, 48, 6);
			Torus(SStack, FVector(0, 0, TopZ + 70.f), FVector::ForwardVector, FVector::RightVector, Size * 0.5f, 3.f, 0.f, 360.f, 40, 6);
			NewVisual(RimHot, SStack);
			SDark.Box(FVector(0, 0, TopZ + 70.f), FVector(16.f, 16.f, 10.f));
			for (int32 s = -1; s <= 1; s += 2)
			{
				Tube(SDark, C + Side * s * (Size + 20.f) + FVector(0, 0, 26.f), FVector(0, 0, TopZ) + Side * s * Size * 0.75f, 2.5f);
				Tube(SDark, FVector(0, 0, TopZ) + Side * s * Size * 0.75f, FVector(0, 0, TopZ + 70.f) + Side * s * Size * 0.5f, 2.f);
			}
			Hang(FVector(0, 0, TopZ + 80.f), Size + 330.f);
		}
	}
	else if (Kind == EZoogiPropKind::SkyBumper)
	{
		// spring-loaded resonance node (cyan core, orange coil springs) in an ancient icy stone frame
		UMaterialInterface* Cap = ZoogiUtil::FxMID(this, FLinearColor(0.3f, 0.75f, 1.f), 0.55f); // see-through forcefield cap
		UMaterialInterface* Spring = ZoogiUtil::GlowMID(this, Orange, 2.6f);
		UMaterialInterface* Core = ZoogiUtil::GlowMID(this, FLinearColor(0.45f, 0.95f, 1.f), 7.f);
		UMaterialInterface* Ice = Mat(Arena, this, TEXT("Ice"), FLinearColor(0.75f, 0.9f, 1.f));
		FZoogiMeshBuilder SCap, SSpring, SCore, SIce;
		Sphere(SCap, FVector::ZeroVector, Size * 0.9f, 20);
		Sphere(SCore, FVector::ZeroVector, Size * 0.55f, 16);
		Torus(SSpring, FVector::ZeroVector, FVector::ForwardVector, FVector::UpVector, Size * 0.62f, 2.5f, 0.f, 360.f, 32, 5); // orange node bands
		Torus(SSpring, FVector::ZeroVector, FVector::RightVector, FVector::UpVector, Size * 0.62f, 2.5f, 0.f, 360.f, 32, 5);
		Torus(SSpring, FVector(0, 0, 8.f), FVector::ForwardVector, FVector::RightVector, Size + 3.f, 5.f, 0.f, 360.f, 40, 6);
		Torus(SSpring, FVector(0, 0, -14.f), FVector::ForwardVector, FVector::RightVector, Size - 2.f, 4.f, 0.f, 360.f, 40, 6);
		const bool bFloor = Base.Z < 150.f && Tag != TEXT("chute");
		if (bFloor)
		{
			// stone base ring on the floor + four short posts; horizontal coil springs from the posts to the node
			const float Zf = -Base.Z;
			Torus(SDark, FVector(0, 0, Zf + 10.f), FVector::ForwardVector, FVector::RightVector, Size + 22.f, 12.f, 0.f, 360.f, 40, 8);
			StoneBlocks(SStone, FVector(0, 0, Zf + 12.f), FVector::ForwardVector, FVector::RightVector, FVector::UpVector, Size + 34.f, 9.f, 0.f, 360.f, 12);
			for (int32 k = 0; k < 4; ++k)
			{
				const float A = FMath::DegreesToRadians(45.f + 90.f * k);
				const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
				const FVector P = Dir * (Size + 22.f);
				Tube(SStone, P + FVector(0, 0, Zf), P + FVector(0, 0, 18.f), 8.f);
				Coil(SSpring, P + FVector(0, 0, 4.f), Dir * (Size * 0.92f) + FVector(0, 0, 4.f), 6.f, 1.6f, 4);
				HexIce(SIce, P + FVector(0, 0, Zf + 18.f) + Dir * 14.f, 9.f, k);
			}
		}
		else
		{
			// vertical ancient stone frame facing the court, radial coil springs to the node, icy chunks + icicles, chains up
			const FVector In = FVector(-Base.X, -Base.Y, 0.f).GetSafeNormal();
			const FVector FS = FVector::CrossProduct(Up, In).GetSafeNormal();
			const float FR = Size + 46.f;
			Torus(SDark, FVector::ZeroVector, FS, Up, FR, 12.f, 0.f, 360.f, 48, 8);
			StoneBlocks(SStone, FVector::ZeroVector, FS, Up, In, FR + 4.f, 17.f, 0.f, 360.f, 12);
			Torus(SGlow, In * 16.f, FS, Up, FR - 14.f, 3.f, 0.f, 360.f, 56, 5);
			Torus(SSpring, In * 20.f, FS, Up, FR - 22.f, 2.5f, 0.f, 360.f, 56, 5);
			for (int32 k = 0; k < 6; ++k)
			{
				const float A = FMath::DegreesToRadians(30.f + 60.f * k);
				const FVector Rd = FS * FMath::Cos(A) + Up * FMath::Sin(A);
				Coil(SSpring, Rd * (FR - 12.f), Rd * (Size * 0.92f), 7.f, 1.8f, 5);
			}
			// ice on the upper blocks + icicles under them
			FRandomStream R(PropId * 31 + 7);
			for (int32 k = 0; k < 7; ++k)
			{
				const float A = FMath::DegreesToRadians(R.FRandRange(20.f, 160.f));
				const FVector P = (FS * FMath::Cos(A) + Up * FMath::Sin(A)) * (FR + 18.f);
				HexIce(SIce, P + Up * 8.f, R.FRandRange(8.f, 14.f), k);
			}
			for (int32 k = 0; k < 9; ++k)
			{
				const float A = FMath::DegreesToRadians(R.FRandRange(-20.f, 200.f));
				const FVector P = (FS * FMath::Cos(A) + Up * FMath::Sin(A)) * (FR - 6.f);
				if (P.Z < -Size * 0.4f) continue;
				Icicle(SIce, P - Up * 14.f, R.FRandRange(18.f, 42.f), R.FRandRange(3.5f, 6.f));
			}
			// hanging chains to an anchor disc
			const float DiscZ = Size + 380.f;
			Disc(SStone, FVector(0, 0, DiscZ), 95.f, 18.f);
			Torus(SGlow, FVector(0, 0, DiscZ - 9.f), FVector::ForwardVector, FVector::RightVector, 95.f, 4.f, 0.f, 360.f, 40, 6);
			for (int32 s = -1; s <= 1; s += 2)
				Chain(SDark, FS * s * FR * 0.55f + Up * FR * 0.85f, FVector(0, 0, DiscZ - 18.f) + FS * s * 55.f, 7.f);
		}
		NewVisual(Cap, SCap);
		NewVisual(Core, SCore);
		NewVisual(Spring, SSpring);
		NewVisual(Ice, SIce);
	}
	else if (Kind == EZoogiPropKind::LaunchRail)
	{
		// curved approach track (VisualPath) into the straight release lane with the three timing bands
		const FVector F2 = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
		TArray<FVector> Path;
		for (const FVector& P : VisualPath) Path.Add(FVector(P.X - Base.X, P.Y - Base.Y, 0.f));
		const float Lane0 = -Size, Lane1 = 560.f;
		Path.Add(F2 * Lane0);
		Path.Add(F2 * Lane1);
		Ribbon(SStone, Path, -70.f, 70.f, 1.2f);
		Ribbon(SGlow, Path, 64.f, 74.f, 2.0f);
		Ribbon(SGlow, Path, -74.f, -64.f, 2.0f);
		Ribbon(SDark, Path, 74.f, 88.f, 1.6f);
		Ribbon(SDark, Path, -88.f, -74.f, 1.6f);
		// timing bands at a nominal 800 cm/s entry: EARLY (high) / CENTER (flat) / LATE (bank)
		const float Nom = 800.f;
		const float E0 = Lane0 + 10.f;
		const float E1 = 0.25f * Nom, C1 = 0.45f * Nom, L1 = FMath::Min(0.70f * Nom, Lane1 - 10.f);
		const struct { float A, B; FLinearColor Col; } Bands[] = {
			{ E0, E1 - 6.f, FLinearColor(1.f, 0.8f, 0.15f) }, { E1 + 6.f, C1 - 6.f, FLinearColor(0.25f, 1.f, 0.4f) }, { C1 + 6.f, L1, FLinearColor(1.f, 0.32f, 0.12f) } };
		for (const auto& Bd : Bands)
		{
			FZoogiMeshBuilder SB;
			Ribbon(SB, { F2 * Bd.A, F2 * Bd.B }, -46.f, 46.f, 2.4f);
			NewVisual(ZoogiUtil::GlowMID(this, Bd.Col, 1.8f), SB);
		}
		// Oct 9 pass 2 (ref rail_release_timing): raised stone curbs with cyan inner glow lines, timing pylons at the band
		// edges, exit chevrons and support piers. Visual only (NoCollision); the rail trigger / impulse are unchanged.
		auto Curb = [&](float Off, float W, float H)
		{
			for (int32 i = 0; i + 1 < Path.Num(); ++i)
			{
				const FVector D = (Path[i + 1] - Path[i]).GetSafeNormal2D();
				if (D.IsNearlyZero()) continue;
				const FVector Sd = FVector::CrossProduct(Up, D).GetSafeNormal();
				const FVector A0 = Path[i] + Sd * (Off - W), A1 = Path[i + 1] + Sd * (Off - W);
				const FVector B0 = Path[i] + Sd * (Off + W), B1 = Path[i + 1] + Sd * (Off + W);
				SStone.Quad(A0 + Up * H, A1 + Up * H, B1 + Up * H, B0 + Up * H, Up);
				SStone.Quad(A0, A1, A1 + Up * H, A0 + Up * H, -Sd);
				SStone.Quad(B0, B1, B1 + Up * H, B0 + Up * H, Sd);
				if (i % 3 == 0) SDark.Box(B0 + Sd * 6.f + Up * (H * 0.5f), FVector(10.f, 6.f, H * 0.5f), D.Rotation());
			}
		};
		Curb(96.f, 9.f, 24.f);
		Curb(-96.f, 9.f, 24.f);
		Ribbon(SGlow, Path, 84.f, 87.f, 24.5f);
		Ribbon(SGlow, Path, -87.f, -84.f, 24.5f);
		FZoogiMeshBuilder SPyl[3];
		const float Edges[] = { E0, E1, C1, L1 };
		for (int32 e = 0; e < 4; ++e)
			for (float Sg : { -1.f, 1.f })
			{
				const FVector P = F2 * Edges[e] + FVector(-F2.Y, F2.X, 0.f) * (Sg * 120.f);
				SDark.Box(P + Up * 6.f, FVector(18.f, 18.f, 6.f), F2.Rotation());
				SStone.Box(P + Up * 40.f, FVector(9.f, 9.f, 30.f), F2.Rotation());
				Sphere(SPyl[FMath::Min(e, 2)], P + Up * 82.f, 11.f, 10);
			}
		NewVisual(ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.8f, 0.15f), 4.f), SPyl[0]);
		NewVisual(ZoogiUtil::GlowMID(this, FLinearColor(0.25f, 1.f, 0.4f), 4.f), SPyl[1]);
		NewVisual(ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.32f, 0.12f), 4.f), SPyl[2]);
		{
			FZoogiMeshBuilder SChev;
			const FVector R2(-F2.Y, F2.X, 0.f);
			for (float T = L1 + 40.f; T < Lane1 - 30.f; T += 70.f)
			{
				const FVector Tip = F2 * (T + 30.f) + Up * 2.6f;
				for (float Sg : { -1.f, 1.f })
				{
					const FVector Back = F2 * T + R2 * (Sg * 34.f) + Up * 2.6f;
					const FVector N = FVector::CrossProduct(Up, (Tip - Back).GetSafeNormal()) * 6.f;
					SChev.Quad(Back - N, Tip - N, Tip + N, Back + N, Up);
				}
			}
			NewVisual(ZoogiUtil::GlowMID(this, Cyan, 3.5f), SChev);
		}
		FZoogiMeshBuilder SRing;
		Torus(SRing, FVector(0, 0, 3.f), FVector::ForwardVector, FVector::RightVector, 84.f, 5.f, 0.f, 360.f, 40, 6);
		NewVisual(ZoogiUtil::GlowMID(this, Orange, 2.4f), SRing);
	}
	else if (Kind == EZoogiPropKind::Gate)
	{
		// red / blue horseshoe gate (open at the bottom) standing across the bridge deck
		// Oct 9 pass 2 (refs frozen_zones_aerial / minigame_layout_frozen): a thick glowing horseshoe with a hot core line,
		// carved stone blocks behind, ice-crusted footings. Visual only; the gate trigger is unchanged.
		const bool bRed = Tag == TEXT("red");
		const FLinearColor GC = bRed ? FLinearColor(1.f, 0.12f, 0.1f) : FLinearColor(0.15f, 0.45f, 1.f);
		const FLinearColor GHot = bRed ? FLinearColor(1.f, 0.55f, 0.45f) : FLinearColor(0.6f, 0.85f, 1.f);
		const FVector GC0(0, 0, Size * 0.55f);
		FZoogiMeshBuilder SG, SGH;
		Torus(SG, GC0, Side, Up, Size, 17.f, -25.f, 205.f, 64, 10);
		NewVisual(ZoogiUtil::GlowMID(this, GC, 3.4f), SG);
		Torus(SGH, GC0 + Forward * 15.f, Side, Up, Size, 4.f, -25.f, 205.f, 64, 6);
		Torus(SGH, GC0 - Forward * 15.f, Side, Up, Size, 4.f, -25.f, 205.f, 64, 6);
		NewVisual(ZoogiUtil::GlowMID(this, GHot, 6.f), SGH);
		FZoogiMeshBuilder SGO;
		Torus(SGO, GC0, Side, Up, Size + 44.f, 3.f, -10.f, 190.f, 56, 5);
		NewVisual(ZoogiUtil::FxMID(this, GC, 2.5f), SGO);
		StoneBlocks(SStone, GC0 - Forward * 6.f, Side, Up, Forward, Size + 26.f, 13.f, -25.f, 205.f, 14);
		for (float A : { -25.f, 205.f })
		{
			const float R = FMath::DegreesToRadians(A);
			const FVector P = GC0 + Side * FMath::Cos(R) * Size + Up * FMath::Sin(R) * Size;
			SDark.Box(FVector(P.X, P.Y, (P.Z - 40.f) * 0.5f), FVector(24.f, 24.f, FMath::Max(10.f, (P.Z + 40.f) * 0.5f)), Forward.Rotation());
			SStone.Box(FVector(P.X, P.Y, 8.f), FVector(36.f, 36.f, 8.f), Forward.Rotation());
			for (int32 k = 0; k < 3; ++k) HexIce(SStone, FVector(P.X, P.Y, 14.f) + Side * (k - 1) * 26.f + Forward * ((k % 2) ? 22.f : -22.f), 16.f, 40 + k + (A > 0.f ? 10 : 0));
		}
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = FMath::DegreesToRadians(30.f + k * 20.f);
			const FVector P = GC0 + Side * FMath::Cos(A) * (Size + 26.f) + Up * FMath::Sin(A) * (Size + 26.f) - Up * 14.f;
			if (FMath::Sin(A) > 0.5f) Icicle(SStone, P, 26.f + 10.f * (k % 3), 6.f);
		}
	}
	NewVisual(Stone, SStone);
	NewVisual(StoneDark, SDark);
	NewVisual(Rune, SGlow);
}

// ------------------------------------------------------------------------------------------------ set dressing

AZoogiAerialDecor::AZoogiAerialDecor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AZoogiAerialDecor::Add(UMaterialInterface* M, const FZoogiMeshBuilder& B)
{
	if (B.IsEmpty()) return;
	UProceduralMeshComponent* P = NewObject<UProceduralMeshComponent>(this);
	P->SetupAttachment(RootComponent);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->SetCollisionResponseToAllChannels(ECR_Ignore);
	P->SetGenerateOverlapEvents(false);
	P->SetCanEverAffectNavigation(false);
	P->RegisterComponent();
	B.Upload(P, 0, false);
	if (M) P->SetMaterial(0, M);
	Meshes.Add(P);
}

void AZoogiAerialDecor::Build(AZoogiArena* Arena, const TArray<AZoogiAerialProp*>& Props)
{
	using namespace ZoogiAerialVis;
	SetActorLocation(FVector::ZeroVector);
	const FLinearColor Cyan = ZoogiUtil::Hex(TEXT("#19C3E6"));
	const FLinearColor Orange(1.f, 0.5f, 0.12f);
	UMaterialInterface* Stone = Mat(Arena, this, TEXT("Stone"), FLinearColor(0.82f, 0.85f, 0.9f));
	UMaterialInterface* StoneDark = Mat(Arena, this, TEXT("StoneDark"), FLinearColor(0.55f, 0.6f, 0.7f));
	UMaterialInterface* IcePane = Mat(Arena, this, TEXT("IcePane"), FLinearColor(0.66f, 0.88f, 0.98f));
	UMaterialInterface* Rune = Mat(Arena, this, TEXT("Rune"), Cyan);
	FZoogiMeshBuilder SStone, SDark, SIce, SGlow, SOrange, SNodeC, SNodeO;
	const FVector X = FVector::ForwardVector, Y = FVector::RightVector, Z = FVector::UpVector;
	// ---- Zone 3: recovery orbit ring (cyan) + outer orange trace, glowing nodes (ref frozen_zones_aerial)
	Torus(SGlow, FVector(0, 0, -90.f), X, Y, 2000.f, 6.f, 0.f, 360.f, 160, 6);
	FZoogiMeshBuilder SHot, SRim, SGlass, SCoil;
	Torus(SHot, FVector(0, 0, -90.f), X, Y, 2000.f, 2.2f, 0.f, 360.f, 160, 4); // hot core line (pass 2)
	Torus(SOrange, FVector(0, 0, -115.f), X, Y, 2075.f, 4.f, 0.f, 360.f, 160, 6);
	for (int32 k = 0; k < 12; ++k)
	{
		const float A = FMath::DegreesToRadians(15.f + 30.f * k);
		const bool bC = k % 2 == 0;
		const float R = bC ? 2000.f : 2075.f;
		Sphere(bC ? SNodeC : SNodeO, FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, bC ? -90.f : -115.f), 24.f, 12);
		{
			const FVector NP(FMath::Cos(A) * R, FMath::Sin(A) * R, bC ? -90.f : -115.f);
			Sphere(SGlass, NP, 40.f, 14); // glass shell around the glowing node (ref orb nodes)
			Torus(bC ? SNodeO : SNodeC, NP, X, Y, 42.f, 3.f, 0.f, 360.f, 24, 4);
		}
	}
	// ---- Zone 2: the aerial structure behind the arena (art top): floating discs, hanging rings, a big halo, cables
	// Oct 8 (Evan 9:26 PM): heights inside the measured reachable envelope (the art top was z 1250; K maps it onto MaxZ)
	const float K = MaxZ > 0.f ? FMath::Min(1.f, MaxZ / 1250.f) : 1.f;
	auto Pol = [](float R, float Yaw, float Zz) { const float A = FMath::DegreesToRadians(Yaw); return FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Zz); };
	auto Chain = [&](FZoogiMeshBuilder& M, const FVector& A, const FVector& B, float LinkR)
	{
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 1.f) return;
		const FVector Dir = D / L;
		FVector S1 = FVector::CrossProduct(Dir, X);
		if (S1.SizeSquared() < 0.01f) S1 = FVector::CrossProduct(Dir, Y);
		S1.Normalize();
		const FVector S2 = FVector::CrossProduct(Dir, S1).GetSafeNormal();
		int32 i = 0;
		for (float T = LinkR; T < L - LinkR * 0.5f; T += LinkR * 2.6f, ++i)
			Torus(M, A + Dir * T, Dir, (i % 2) ? S1 : S2, LinkR, LinkR * 0.28f, 0.f, 360.f, 10, 4);
	};
	auto Coil = [&](FZoogiMeshBuilder& M, const FVector& A, const FVector& B, float CoilR, float WireR, int32 Turns)
	{
		const FVector D = B - A;
		const FVector Dir = D.GetSafeNormal();
		FVector S1 = FVector::CrossProduct(Dir, Z);
		if (S1.SizeSquared() < 0.01f) S1 = FVector::CrossProduct(Dir, X);
		S1.Normalize();
		const FVector S2 = FVector::CrossProduct(Dir, S1).GetSafeNormal();
		const int32 N = Turns * 10;
		FVector Prev = A + S1 * CoilR;
		for (int32 k = 1; k <= N; ++k)
		{
			const float T = (float)k / N, Ang = T * Turns * 2.f * PI;
			const FVector P = A + D * T + (S1 * FMath::Cos(Ang) + S2 * FMath::Sin(Ang)) * CoilR;
			Tube(M, Prev, P, WireR, 5);
			Prev = P;
		}
	};
	struct FDiscDef { float R, Yaw, Zz, Rad; };
	const FDiscDef Discs[] = { { 1850.f, 252.f, 620.f, 130.f }, { 2050.f, 272.f, 780.f, 110.f }, { 1800.f, 292.f, 690.f, 120.f }, { 2250.f, 262.f, 960.f, 90.f }, { 2150.f, 300.f, 1020.f, 100.f }, { 1950.f, 236.f, 900.f, 85.f } };
	for (const FDiscDef& D : Discs)
	{
		const FVector C = Pol(D.R, D.Yaw, FMath::Max(40.f, D.Zz * K));
		Disc(SStone, C, D.Rad, 22.f);
		Disc(SIce, C + FVector(0, 0, 3.f), D.Rad * 0.82f, 2.f);
		Torus(SGlow, C - FVector(0, 0, 11.f), X, Y, D.Rad, 4.5f, 0.f, 360.f, 40, 6);
		// pass 2 (ref spring pucks): bright rim on both edges, an orange coil spring under the slab ending in a glowing node,
		// and the hanger as a chain near the disc + a thin cable above
		Torus(SRim, C + FVector(0, 0, 1.f), X, Y, D.Rad + 2.f, 3.f, 0.f, 360.f, 48, 5);
		Torus(SRim, C - FVector(0, 0, 21.f), X, Y, D.Rad + 2.f, 3.f, 0.f, 360.f, 48, 5);
		Coil(SCoil, C - FVector(0, 0, 22.f), C - FVector(0, 0, 92.f), 16.f, 2.6f, 5);
		Sphere(SNodeO, C - FVector(0, 0, 104.f), 13.f, 10);
		Chain(SDark, C + FVector(0, 0, 2.f), C + FVector(0, 0, 320.f), 7.f);
		Tube(SDark, C + FVector(0, 0, 320.f), FVector(C.X, C.Y, 2600.f), 3.f, 6);
	}
	// hanging vertical rings (facing the court) and one big horizontal halo over the cluster
	struct FRingDef { float R, Yaw, Zz, Rad; };
	const FRingDef Rings[] = { { 1900.f, 262.f, 900.f, 170.f }, { 1980.f, 284.f, 560.f, 120.f } };
	for (const FRingDef& Rg : Rings)
	{
		const FVector C = Pol(Rg.R, Rg.Yaw, FMath::Max(Rg.Rad + 30.f, Rg.Zz * K));
		const FVector In = FVector(-C.X, -C.Y, 0.f).GetSafeNormal();
		const FVector S = FVector::CrossProduct(Z, In).GetSafeNormal();
		Torus(SStone, C, S, Z, Rg.Rad, 16.f, 0.f, 360.f, 56, 10);
		Torus(SGlow, C + In * 8.f, S, Z, Rg.Rad - 14.f, 3.5f, 0.f, 360.f, 56, 6);
		Torus(SRim, C - In * 8.f, S, Z, Rg.Rad - 14.f, 3.5f, 0.f, 360.f, 56, 6);
		Torus(SRim, C, S, Z, Rg.Rad + 15.f, 2.5f, 0.f, 360.f, 56, 5);
		Tube(SDark, C + Z * (Rg.Rad + 14.f), FVector(C.X, C.Y, 2600.f), 3.f, 6);
	}
	{
		const FVector C = Pol(2000.f, 270.f, FMath::Max(60.f, 1250.f * K));
		Torus(SStone, C, X, Y, 340.f, 20.f, 0.f, 360.f, 72, 10);
		Torus(SGlow, C - FVector(0, 0, 18.f), X, Y, 340.f, 5.f, 0.f, 360.f, 72, 6);
		for (int32 k = 0; k < 3; ++k)
		{
			const float A = FMath::DegreesToRadians(30.f + 120.f * k);
			const FVector P = C + FVector(FMath::Cos(A) * 340.f, FMath::Sin(A) * 340.f, 18.f);
			Tube(SDark, P, FVector(P.X, P.Y, 2800.f), 3.f, 6);
		}
		// pass 2 (ref Halo Bank): a stacked bank of glowing rings under the big halo, tied with cables, plus spring hubs
		const float Lv[] = { 210.f, 400.f };
		const float Rr[] = { 250.f, 170.f };
		for (int32 j = 0; j < 2; ++j)
		{
			const FVector Cj = C - FVector(0, 0, Lv[j] * K);
			if (Cj.Z < 120.f) continue;
			Torus(SStone, Cj, X, Y, Rr[j], 13.f, 0.f, 360.f, 56, 8);
			Torus(SRim, Cj - FVector(0, 0, 12.f), X, Y, Rr[j], 4.f, 0.f, 360.f, 56, 5);
			Torus(SRim, Cj + FVector(0, 0, 12.f), X, Y, Rr[j] - 6.f, 2.5f, 0.f, 360.f, 56, 5);
			for (int32 k = 0; k < 3; ++k)
			{
				const float A = FMath::DegreesToRadians(30.f + 120.f * k + 60.f * j);
				const FVector P = Cj + FVector(FMath::Cos(A) * Rr[j], FMath::Sin(A) * Rr[j], 0.f);
				Tube(SDark, P, FVector(P.X, P.Y, C.Z), 2.5f, 6);
			}
		}
		Torus(SRim, C + FVector(0, 0, 21.f), X, Y, 340.f, 3.f, 0.f, 360.f, 72, 5);
		for (int32 k = 0; k < 3; ++k)
		{
			// small hanging spring hubs (disc + orange coil + node) around the bank
			const FVector H = Pol(2000.f + 420.f * FMath::Cos(k * 2.1f), 270.f + (k - 1) * 13.f, FMath::Max(140.f, (980.f - 140.f * k) * K));
			Disc(SStone, H, 55.f, 12.f);
			Torus(SRim, H - FVector(0, 0, 6.f), X, Y, 57.f, 3.f, 0.f, 360.f, 32, 5);
			Coil(SCoil, H - FVector(0, 0, 12.f), H - FVector(0, 0, 62.f), 11.f, 2.2f, 4);
			Sphere(SNodeO, H - FVector(0, 0, 72.f), 10.f, 10);
			Tube(SDark, H, FVector(H.X, H.Y, 2600.f), 2.5f, 6);
		}
	}
	// pass 2: floating glass orbs with cyan cores drifting through the aerial cluster (ref frozen_zones_aerial)
	{
		const FVector Orbs[] = { Pol(1760.f, 248.f, 760.f), Pol(1700.f, 278.f, 470.f), Pol(2300.f, 286.f, 820.f), Pol(1850.f, 306.f, 980.f), Pol(2200.f, 244.f, 1100.f) };
		for (const FVector& O0 : Orbs)
		{
			const FVector O(O0.X, O0.Y, FMath::Max(120.f, O0.Z * K));
			Sphere(SGlass, O, 30.f, 14);
			Sphere(SNodeC, O, 13.f, 10);
		}
	}
	// curved ice slide segments between the discs (the art's aerial tracks); pass 2 adds a second, lower S-curve
	auto Slide = [&](float Yaw0, float Yaw1, float R0, float R1, float Z0, float Z1, float Bow)
	{
		TArray<FVector> Pts;
		for (int32 i = 0; i <= 16; ++i)
		{
			const float T = (float)i / 16.f;
			const float Yaw = FMath::Lerp(Yaw0, Yaw1, T);
			Pts.Add(Pol(FMath::Lerp(R0, R1, T) + Bow * FMath::Sin(T * 2.f * PI), Yaw, FMath::Max(40.f, (FMath::Lerp(Z0, Z1, T) + 60.f * FMath::Sin(T * PI)) * K)));
		}
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			const FVector D = (Pts[i + 1] - Pts[i]).GetSafeNormal();
			const FVector S = FVector::CrossProduct(Z, D).GetSafeNormal();
			SIce.Quad(Pts[i] - S * 55.f, Pts[i + 1] - S * 55.f, Pts[i + 1] + S * 55.f, Pts[i] + S * 55.f, Z);
			SIce.Quad(Pts[i] - S * 55.f - Z * 14.f, Pts[i + 1] - S * 55.f - Z * 14.f, Pts[i + 1] + S * 55.f - Z * 14.f, Pts[i] + S * 55.f - Z * 14.f, -Z);
			SDark.Quad(Pts[i] + S * 55.f, Pts[i + 1] + S * 55.f, Pts[i + 1] + S * 55.f - Z * 14.f, Pts[i] + S * 55.f - Z * 14.f, S);
			SDark.Quad(Pts[i] - S * 55.f, Pts[i + 1] - S * 55.f, Pts[i + 1] - S * 55.f - Z * 14.f, Pts[i] - S * 55.f - Z * 14.f, -S);
			SRim.Quad(Pts[i] + S * 50.f + Z * 1.f, Pts[i + 1] + S * 50.f + Z * 1.f, Pts[i + 1] + S * 56.f + Z * 1.f, Pts[i] + S * 56.f + Z * 1.f, Z);
			SRim.Quad(Pts[i] - S * 56.f + Z * 1.f, Pts[i + 1] - S * 56.f + Z * 1.f, Pts[i + 1] - S * 50.f + Z * 1.f, Pts[i] - S * 50.f + Z * 1.f, Z);
			if (i % 4 == 2) Tube(SDark, Pts[i], FVector(Pts[i].X, Pts[i].Y, 2600.f), 2.f, 5);
		}
	};
	Slide(244.f, 300.f, 1880.f, 1760.f, 830.f, 600.f, 0.f);
	Slide(296.f, 254.f, 2150.f, 2100.f, 470.f, 360.f, 90.f);
	Add(Stone, SStone);
	Add(StoneDark, SDark);
	Add(IcePane, SIce);
	Add(Rune, SGlow);
	Add(ZoogiUtil::GlowMID(this, Orange, 2.2f), SOrange);
	Add(ZoogiUtil::GlowMID(this, Cyan, 3.f), SNodeC);
	Add(ZoogiUtil::GlowMID(this, Cyan, 6.f), SRim);
	Add(ZoogiUtil::GlowMID(this, FLinearColor(0.7f, 0.95f, 1.f), 8.f), SHot);
	Add(ZoogiUtil::GlowMID(this, Orange, 2.6f), SCoil);
	Add(ZoogiUtil::FxMID(this, FLinearColor(0.35f, 0.75f, 0.95f), 0.5f), SGlass);
	Add(ZoogiUtil::GlowMID(this, Orange, 3.f), SNodeO);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE aerial decor built: meshes=%d (visual only, no collision) props=%d max_z=%.0f k=%.2f"), Meshes.Num(), Props.Num(), MaxZ, K);
}
