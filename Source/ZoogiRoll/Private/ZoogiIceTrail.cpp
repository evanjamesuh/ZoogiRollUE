// Crystal Collapse ice-shard trail (Oct 22, Evan). See ZoogiIceTrail.h.
#include "ZoogiIceTrail.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

namespace ZoogiIceTrailPriv
{
	TArray<TWeakObjectPtr<AZoogiIceTrail>>& Trails() { static TArray<TWeakObjectPtr<AZoogiIceTrail>> T; return T; }
	int32 GFragLights = 0;
	constexpr float TrailRiseTime = 0.25f;
	float TrailEase(float X) { X = FMath::Clamp(X, 0.f, 1.f); return 1.f - FMath::Square(1.f - X); }
}

using namespace ZoogiIceTrailPriv;

void FZoogiIceTrailTune::Load()
{
	const TCHAR* S = TEXT("ZoogiRoll.IceTrail");
	if (GConfig)
	{
		auto F = [&](const TCHAR* K, float& V) { GConfig->GetFloat(S, K, V, GGameIni); };
		auto I = [&](const TCHAR* K, int32& V) { GConfig->GetInt(S, K, V, GGameIni); };
		GConfig->GetBool(S, TEXT("bOn"), bOn, GGameIni);
		F(TEXT("TrailSeconds"), TrailSeconds); F(TEXT("ArmTimeout"), ArmTimeout); F(TEXT("MinSpeed"), MinSpeed); F(TEXT("Spacing"), Spacing);
		I(TEXT("MaxShards"), MaxShards); F(TEXT("BackOffset"), BackOffset); F(TEXT("ShardRadius"), ShardRadius); F(TEXT("ShardHeight"), ShardHeight);
		F(TEXT("BurstDelay"), BurstDelay); F(TEXT("BurstStagger"), BurstStagger); F(TEXT("BurstRadius"), BurstRadius);
		F(TEXT("ZoogiPush"), ZoogiPush); F(TEXT("ZoogiUp"), ZoogiUp); F(TEXT("PhysicsImpulse"), PhysicsImpulse);
		GConfig->GetBool(S, TEXT("bBreakPieces"), bBreakPieces, GGameIni);
		I(TEXT("Fragments"), Fragments); F(TEXT("FragSpeed"), FragSpeed); F(TEXT("FragLife"), FragLife); I(TEXT("MaxFragLights"), MaxFragLights);
		F(TEXT("FragLightIntensity"), FragLightIntensity); F(TEXT("FloorGlowEvery"), FloorGlowEvery);
		GConfig->GetString(S, TEXT("Mesh"), Mesh, GGameIni); GConfig->GetString(S, TEXT("BurstKFX"), BurstKFX, GGameIni);
		FString C; if (GConfig->GetString(S, TEXT("Glow"), C, GGameIni) && !C.IsEmpty()) Glow.InitFromString(C);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallClassic"))) bOn = false;
	TrailSeconds = FMath::Clamp(TrailSeconds, 0.2f, 10.f); Spacing = FMath::Clamp(Spacing, 40.f, 600.f); MaxShards = FMath::Clamp(MaxShards, 1, 40);
	ShardRadius = FMath::Clamp(ShardRadius, 8.f, 120.f); ShardHeight = FMath::Clamp(ShardHeight, 20.f, 400.f);
	BurstDelay = FMath::Clamp(BurstDelay, 0.2f, 15.f); BurstStagger = FMath::Clamp(BurstStagger, 0.f, 2.f); BurstRadius = FMath::Clamp(BurstRadius, 50.f, 1500.f);
	Fragments = FMath::Clamp(Fragments, 0, 12); FragLife = FMath::Clamp(FragLife, 0.1f, 3.f); MaxFragLights = FMath::Clamp(MaxFragLights, 0, 16);
}

const FZoogiIceTrailTune& AZoogiIceTrail::Tune() { static FZoogiIceTrailTune T; static bool b = false; if (!b) { b = true; T.Load(); } return T; }

AZoogiIceTrail::AZoogiIceTrail()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

AZoogiIceTrail* AZoogiIceTrail::Start(UWorld* World, AZoogiBall* InBall, int64 EffectId, float InAmp)
{
	if (!World || !InBall) return nullptr;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiIceTrail* T = World->SpawnActor<AZoogiIceTrail>(AZoogiIceTrail::StaticClass(), FTransform(InBall->BallLocation()), P);
	if (!T) return nullptr;
	T->Ball = InBall; T->OwnerP = InBall->PlayerIndex; T->Effect = EffectId; T->Amp = FMath::Max(InAmp, 0.5f);
	T->LastPos = InBall->BallLocation();
	if (UStaticMesh* M = Tune().Mesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *Tune().Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet)) T->Keep.Add(M);
	Trails().RemoveAll([](const TWeakObjectPtr<AZoogiIceTrail>& X) { return !X.IsValid(); });
	Trails().Add(T);
	UE_LOG(LogZoogi, Log, TEXT("ICETRAIL armed effect=%lld P%d amp %.2f (trail %.1fs every %.0fcm, burst %.1fs+%.2f*i r%.0f)"), EffectId, T->OwnerP + 1, T->Amp,
		Tune().TrailSeconds, Tune().Spacing, Tune().BurstDelay, Tune().BurstStagger, Tune().BurstRadius);
	return T;
}

void AZoogiIceTrail::DestroyAll(UWorld* World, const TCHAR* Why)
{
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiIceTrail>& W : TArray<TWeakObjectPtr<AZoogiIceTrail>>(Trails())) if (AZoogiIceTrail* T = W.Get()) if (T->GetWorld() == World) { T->Destroy(); ++N; }
	if (N) UE_LOG(LogZoogi, Log, TEXT("ICETRAIL cleared %d (%s)"), N, Why);
}

bool AZoogiIceTrail::SpotClear(const FVector& Floor) const
{
	AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (!G) return true;
	const FVector2D Q(Floor);
	const float R = Tune().ShardRadius * 1.42f; // box corner
	if (G->Arena && G->Arena->IsOffCourt(Q + Q.GetSafeNormal() * R)) return false;
	for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector2D::Distance(FVector2D(B->BallLocation()), Q) < ZoogiConst::ZoogiRadius + R + 4.f) return false;
	for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector2D::Distance(FVector2D(O->OrbLocation()), Q) < ZoogiConst::OrbRadius + R + 4.f) return false;
	return true;
}

int32 AZoogiIceTrail::AddShard(const FVector& Floor, float Delay)
{
	const FZoogiIceTrailTune& T = Tune();
	if (Shards.Num() >= T.MaxShards * 2 || !SpotClear(Floor)) return INDEX_NONE;
	FRandomStream R((int32)(Effect & 0x7fffffff) ^ (Shards.Num() * 7919 + 0x1CE));
	const float Yaw = R.FRandRange(0.f, 360.f);
	FShard S; S.Floor = Floor; S.BurstAt = Delay;
	S.Col = NewObject<UStaticMeshComponent>(this);
	S.Col->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
	S.Col->SetMobility(EComponentMobility::Movable);
	S.Col->SetupAttachment(Root);
	S.Col->SetCollisionProfileName(TEXT("BlockAll"));
	S.Col->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	S.Col->SetHiddenInGame(true); S.Col->SetCastShadow(false);
	S.Col->RegisterComponent();
	S.Col->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 0.f), Floor + FVector(0, 0, T.ShardHeight * 0.5f), FVector(T.ShardRadius / 50.f, T.ShardRadius / 50.f, T.ShardHeight / 100.f)));
	UStaticMesh* M = Keep.Num() ? Cast<UStaticMesh>(Keep[0]) : nullptr;
	const bool bCone = M == nullptr;
	if (!M) M = ZoogiUtil::BasicShape(TEXT("Cone"));
	S.Vis = NewObject<UStaticMeshComponent>(this);
	S.Vis->SetStaticMesh(M);
	S.Vis->SetMobility(EComponentMobility::Movable);
	S.Vis->SetupAttachment(Root);
	S.Vis->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	S.Vis->SetCastShadow(false);
	if (bCone) S.Vis->SetMaterial(0, ZoogiUtil::GlowMID(this, T.Glow, 1.5f));
	else if (UMaterialInstanceDynamic* GM = ZoogiUtil::FxMID(this, T.Glow, 0.45f)) S.Vis->SetOverlayMaterial(GM);
	S.Vis->RegisterComponent();
	const FBoxSphereBounds MB = M->GetBounds();
	const FVector Ext = MB.BoxExtent.ComponentMax(FVector(1.f));
	S.Scale = FVector(T.ShardRadius * 1.5f / (2.f * Ext.X), T.ShardRadius * 1.5f / (2.f * Ext.Y), T.ShardHeight * R.FRandRange(0.9f, 1.1f) / (2.f * Ext.Z));
	S.Vis->SetWorldLocationAndRotation(FVector(Floor.X, Floor.Y, Floor.Z - (MB.Origin.Z - Ext.Z) * S.Scale.Z), FRotator(R.FRandRange(-8.f, 8.f), Yaw, R.FRandRange(-8.f, 8.f)));
	S.Vis->SetWorldScale3D(FVector(S.Scale.X, S.Scale.Y, S.Scale.Z * 0.02f));
	Shards.Add(S);
	AZoogiFX::Spawn(GetWorld(), Floor + FVector(0, 0, 4.f), T.Glow, T.ShardRadius * 2.f, 0.3f, true, 5.f);
	if (Shards.Num() == 1) ZoogiUtil::PlaySfx(this, TEXT("StingFrost"), 0.6f, 1.25f);
	UE_LOG(LogZoogi, Log, TEXT("ICETRAIL shard %d effect=%lld at (%.0f,%.0f) bursts in %.2fs"), Shards.Num() - 1, Effect, Floor.X, Floor.Y, Delay);
	return Shards.Num() - 1;
}

void AZoogiIceTrail::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	const FZoogiIceTrailTune& T = Tune();
	// lay shards behind the rolling Zoogi
	AZoogiBall* B = Ball.Get();
	if (!bDoneLaying)
	{
		if (!B || B->bOut || (RollT < 0.f && Age > T.ArmTimeout)) bDoneLaying = true;
		else
		{
			const FVector P = B->BallLocation(), V = B->GetBallVelocity();
			const float Sp = FVector2D(V.X, V.Y).Size();
			if (RollT < 0.f && Sp > T.MinSpeed) { RollT = 0.f; LastPos = P; Travel = T.Spacing * 0.6f; UE_LOG(LogZoogi, Log, TEXT("ICETRAIL effect=%lld P%d rolling: laying shards"), Effect, OwnerP + 1); }
			if (RollT >= 0.f)
			{
				RollT += Dt;
				Travel += FVector::Dist2D(P, LastPos); LastPos = P;
				int32 Laid = 0; for (const FShard& S : Shards) Laid += 1; // shards from this trail
				if (Travel >= T.Spacing && Sp > T.MinSpeed * 0.5f && Laid < T.MaxShards)
				{
					const FVector2D D = FVector2D(V.X, V.Y).GetSafeNormal();
					const FVector2D At = FVector2D(P) - D * (ZoogiConst::ZoogiRadius + T.ShardRadius * 1.42f + T.BackOffset);
					const FVector Floor(At.X, At.Y, P.Z - ZoogiConst::ZoogiRadius);
					if (AddShard(Floor, T.BurstDelay + T.BurstStagger * Shards.Num()) != INDEX_NONE) Travel = 0.f;
				}
				if (RollT >= T.TrailSeconds || Laid >= T.MaxShards) { bDoneLaying = true; UE_LOG(LogZoogi, Log, TEXT("ICETRAIL effect=%lld trail done: %d shards"), Effect, Shards.Num()); }
			}
		}
	}
	bool bLive = !bDoneLaying;
	for (int32 i = 0; i < Shards.Num(); ++i)
	{
		FShard& S = Shards[i];
		if (S.bBurst) continue;
		bLive = true;
		S.Age += Dt;
		if (S.Vis && S.Age - Dt < TrailRiseTime + 0.05f) S.Vis->SetWorldScale3D(FVector(S.Scale.X, S.Scale.Y, S.Scale.Z * FMath::Max(0.02f, TrailEase(S.Age / TrailRiseTime))));
		// tell: the shard trembles and glows harder in its last 0.5 s
		if (S.Vis && S.BurstAt - S.Age < 0.5f) S.Vis->SetWorldScale3D(S.Scale * (1.f + 0.06f * FMath::Sin(S.Age * 60.f)));
		if (S.Age >= S.BurstAt) Burst(i);
	}
	TickFrags(Dt);
	if (!bLive && Frags.Num() == 0) Destroy();
}

void AZoogiIceTrail::Burst(int32 i)
{
	FShard& S = Shards[i];
	if (S.bBurst) return;
	S.bBurst = true; ++Bursts;
	const FZoogiIceTrailTune& T = Tune();
	UWorld* W = GetWorld();
	if (S.Col) S.Col->DestroyComponent();
	const FVector C = S.Floor + FVector(0, 0, T.ShardHeight * 0.4f);
	const float Rad = T.BurstRadius * Amp;
	// VFX + sound (no crater / cracks: Evan, Oct 22)
	if (!T.BurstKFX.IsEmpty() && !AZoogiFX::KFX(W, *T.BurstKFX, S.Floor, 0.45f, 1.6f)) AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), C, FRotator::ZeroRotator, 1.1f);
	AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), C, FRotator::ZeroRotator, 1.0f);
	AZoogiFX::Spawn(W, S.Floor + FVector(0, 0, 5.f), T.Glow, Rad * 0.6f, 0.35f, true, 7.f);
	ZoogiUtil::PlaySfx(this, TEXT("RockShatter"), 0.9f, FMath::FRandRange(1.15f, 1.35f));
	// radial push: Zoogis
	AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>();
	int32 NZ = 0, NP = 0, NB = 0;
	TSet<const AActor*> Done;
	if (G) for (AZoogiBall* O : G->Balls)
	{
		if (!O || O->bOut || O->bKnockedOut || !O->Sphere) continue;
		Done.Add(O);
		const FVector2D D = FVector2D(O->BallLocation() - S.Floor); const float Dist = D.Size();
		if (Dist > Rad + ZoogiConst::ZoogiRadius) continue;
		const FVector2D Dir = Dist > 1.f ? D / Dist : FVector2D(1.f, 0.f);
		const float K = 0.4f + 0.6f * FMath::Clamp(1.f - Dist / Rad, 0.f, 1.f);
		O->Sphere->SetPhysicsLinearVelocity(O->GetBallVelocity() + FVector(Dir * T.ZoogiPush * Amp * K, T.ZoogiUp * K));
		if (O->PlayerIndex != OwnerP) O->LastHitPlayer = OwnerP;
		++NZ;
	}
	// orbs / loose simulated physics objects (anything movable that simulates; static pillars never move)
	TArray<FOverlapResult> Ov;
	FCollisionObjectQueryParams Q; Q.AddObjectTypesToQuery(ECC_PhysicsBody); Q.AddObjectTypesToQuery(ECC_WorldDynamic); Q.AddObjectTypesToQuery(ECC_Pawn);
	W->OverlapMultiByObjectType(Ov, C, FQuat::Identity, Q, FCollisionShape::MakeSphere(Rad));
	TSet<UPrimitiveComponent*> Seen;
	for (const FOverlapResult& R : Ov)
	{
		UPrimitiveComponent* P = R.GetComponent();
		if (!P || Seen.Contains(P) || Done.Contains(P->GetOwner()) || !P->IsSimulatingPhysics() || P->Mobility != EComponentMobility::Movable) continue;
		Seen.Add(P);
		P->AddRadialImpulse(C, Rad, T.PhysicsImpulse * Amp, ERadialImpulseFalloff::RIF_Linear, false);
		++NP;
	}
	// Crystal Collapse = the card-scoped destruction exemption: one break state on eligible pieces (pillars / indestructible never)
	if (T.bBreakPieces) for (TActorIterator<AZoogiArenaPiece> It(W); It; ++It)
	{
		AZoogiArenaPiece* Pc = *It;
		if (!UZoogiCardSubsystem::IsCollapseEligible(Pc) || FVector::Dist2D(Pc->GetActorLocation(), S.Floor) > Rad) continue;
		if (Pc->ApplyCardCollapse(Effect * 100 + 50 + i, Pc->GetActorLocation(), (Pc->GetActorLocation() - S.Floor).GetSafeNormal2D())) ++NB;
	}
	PushedZoogis += NZ; PushedPhysics += NP; BrokenPieces += NB;
	// fragments: the shard mesh flies apart in a few pieces, each lighting the floor under it
	FRandomStream R((int32)(Effect & 0x7fffffff) ^ (i * 104729));
	for (int32 k = 0; k < T.Fragments && S.Vis; ++k)
	{
		FFrag F;
		F.Mesh = NewObject<UStaticMeshComponent>(this);
		F.Mesh->SetStaticMesh(S.Vis->GetStaticMesh());
		F.Mesh->SetMobility(EComponentMobility::Movable);
		F.Mesh->SetupAttachment(Root);
		F.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		F.Mesh->SetCastShadow(false);
		for (int32 m = 0; m < S.Vis->GetNumMaterials(); ++m) F.Mesh->SetMaterial(m, S.Vis->GetMaterial(m));
		F.Mesh->SetOverlayMaterial(S.Vis->GetOverlayMaterial());
		F.Mesh->RegisterComponent();
		F.Scale = S.Scale * R.FRandRange(0.22f, 0.32f);
		F.Mesh->SetWorldLocationAndRotation(C + FVector(R.FRandRange(-15.f, 15.f), R.FRandRange(-15.f, 15.f), R.FRandRange(-20.f, 20.f)), FRotator(R.FRandRange(0.f, 360.f), R.FRandRange(0.f, 360.f), 0.f));
		F.Mesh->SetWorldScale3D(F.Scale);
		const FVector2D Dir = FVector2D(1.f, 0.f).GetRotated(360.f * (k + R.FRandRange(-0.3f, 0.3f)) / FMath::Max(T.Fragments, 1));
		F.Vel = FVector(Dir * T.FragSpeed * R.FRandRange(0.75f, 1.15f), R.FRandRange(250.f, 450.f));
		F.Spin = R.FRandRange(-720.f, 720.f);
		F.Life = T.FragLife * R.FRandRange(0.85f, 1.1f);
		if (GFragLights < T.MaxFragLights)
		{
			F.Light = NewObject<UPointLightComponent>(this);
			F.Light->SetupAttachment(Root);
			F.Light->SetLightColor(T.Glow);
			F.Light->SetIntensityUnits(ELightUnits::Unitless);
			F.Light->SetIntensity(T.FragLightIntensity);
			F.Light->SetAttenuationRadius(220.f);
			F.Light->SetCastShadows(false);
			F.Light->RegisterComponent();
			F.Light->SetWorldLocation(F.Mesh->GetComponentLocation());
			++GFragLights;
		}
		Frags.Add(F);
	}
	if (S.Vis) S.Vis->DestroyComponent();
	UE_LOG(LogZoogi, Log, TEXT("ICETRAIL burst shard %d effect=%lld at (%.0f,%.0f) r%.0f: pushed %d Zoogi(s), %d physics object(s), broke %d piece(s), %d fragments (no crater)"),
		i, Effect, S.Floor.X, S.Floor.Y, Rad, NZ, NP, NB, T.Fragments);
}

void AZoogiIceTrail::TickFrags(float Dt)
{
	const FZoogiIceTrailTune& T = Tune();
	for (int32 k = Frags.Num() - 1; k >= 0; --k)
	{
		FFrag& F = Frags[k];
		F.Life -= Dt;
		if (F.Life <= 0.f || !F.Mesh)
		{
			if (F.Mesh) F.Mesh->DestroyComponent();
			if (F.Light) { F.Light->DestroyComponent(); --GFragLights; }
			Frags.RemoveAtSwap(k);
			continue;
		}
		FVector P = F.Mesh->GetComponentLocation() + F.Vel * Dt;
		F.Vel.Z -= 1500.f * Dt;
		const float FloorZ = Shards.Num() ? Shards[0].Floor.Z : P.Z - 100.f;
		if (P.Z < FloorZ + 8.f) { P.Z = FloorZ + 8.f; F.Vel.Z = FMath::Abs(F.Vel.Z) * 0.35f; F.Vel.X *= 0.8f; F.Vel.Y *= 0.8f; } // skips across the floor
		F.Mesh->SetWorldLocation(P);
		F.Mesh->AddWorldRotation(FRotator(F.Spin * Dt, F.Spin * 0.6f * Dt, 0.f));
		const float Fade = FMath::Clamp(F.Life / (T.FragLife * 0.5f), 0.f, 1.f);
		F.Mesh->SetWorldScale3D(F.Scale * (0.4f + 0.6f * Fade));
		if (F.Light) { F.Light->SetWorldLocation(P + FVector(0, 0, 20.f)); F.Light->SetIntensity(T.FragLightIntensity * Fade); }
		F.NextGlow -= Dt;
		if (F.NextGlow <= 0.f) { F.NextGlow = T.FloorGlowEvery; AZoogiFX::SpawnTrail(GetWorld(), FVector(P.X, P.Y, FloorZ + 3.f), T.Glow, 38.f * (0.5f + 0.5f * Fade), 0.35f, 6.f * Fade); }
	}
}

// ------------------------------------------------------------------ -ZoogiCrystalTest

void AZoogiIceTrail::EnsureTest(UWorld* World)
{
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrystalTest"));
	if (!bTest || !World || !World->IsGameWorld()) return;
	static TWeakObjectPtr<UWorld> Done;
	if (Done.Get() == World) return;
	Done = World;
	World->SpawnActor<AZoogiCrystalTest>();
	UE_LOG(LogZoogi, Log, TEXT("CRYSTAL_TEST armed"));
}

AZoogiCrystalTest::AZoogiCrystalTest() { PrimaryActorTick.bCanEverTick = true; }

void AZoogiCrystalTest::Finish(const TCHAR* Why)
{
	const bool bPass = bSpawn && bBlock && bBurst && bPush;
	UE_LOG(LogZoogi, Log, TEXT("CRYSTAL_TEST result=%s spawn=%d(%d shards) block=%d burst=%d push=%d (%s)"), bPass ? TEXT("PASS") : TEXT("FAIL"), bSpawn, ShardsLaid, bBlock, bBurst, bPush, Why);
	Step = 99;
	FPlatformMisc::RequestExit(false);
}

void AZoogiCrystalTest::Tick(float Dt)
{
	Super::Tick(Dt);
	T += Dt; StepT += Dt;
	if (Step == 99) return;
	AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (T > 90.f) { Finish(TEXT("timeout")); return; }
	if (!G || G->Balls.Num() < 2 || !G->Balls[0] || !G->Balls[1]) return;
	AZoogiBall* A = G->Balls[0]; AZoogiBall* B = G->Balls[1];
	const FZoogiIceTrailTune& Tu = AZoogiIceTrail::Tune();
	switch (Step)
	{
	case 0:
		if (T < 8.f || A->bOut || !A->Sphere || !A->Sphere->IsSimulatingPhysics()) return;
		Trail = AZoogiIceTrail::Start(GetWorld(), A, 990001);
		{ const FVector2D D = (-FVector2D(A->BallLocation())).GetSafeNormal(); A->Sphere->SetPhysicsLinearVelocity(FVector(D * 1100.f, 0.f)); }
		Step = 1; StepT = 0.f; return;
	case 1:
		if (Trail.IsValid() && StepT < 1.0f) { const FVector V = A->GetBallVelocity(); if (FVector2D(V.X, V.Y).Size() < 600.f) { const FVector2D D = FVector2D(V.X, V.Y).GetSafeNormal(); A->Sphere->SetPhysicsLinearVelocity(FVector((D.IsNearlyZero() ? (-FVector2D(A->BallLocation())).GetSafeNormal() : D) * 900.f, V.Z)); } }
		if (StepT < 1.6f) return;
		if (!Trail.IsValid()) { Finish(TEXT("trail gone")); return; }
		ShardsLaid = Trail->Shards.Num();
		bSpawn = ShardsLaid >= 2;
		if (ShardsLaid > 0 && Trail->Shards[0].Col)
		{
			const FVector F = Trail->Shards[0].Floor + FVector(0, 0, ZoogiConst::ZoogiRadius + 2.f);
			FHitResult H; FCollisionQueryParams QP(TEXT("CrystalTest"), false); QP.AddIgnoredActor(A); QP.AddIgnoredActor(B);
			for (const float Ang : { 0.f, 90.f, 180.f, 270.f })
			{
				const FVector Off = FVector(1, 0, 0).RotateAngleAxis(Ang, FVector::UpVector) * 260.f;
				if (GetWorld()->SweepSingleByChannel(H, F + Off, F, FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius), QP) && H.GetComponent() == Trail->Shards[0].Col)
				{ bBlock = true; break; }
			}
			UE_LOG(LogZoogi, Log, TEXT("CRYSTAL_TEST block sweep -> %s"), bBlock ? TEXT("blocked by shard 0") : *GetNameSafe(H.GetComponent()));
		}
		// push probe: one shard next to P2, bursting in 1 s
		PushTrail = AZoogiIceTrail::Start(GetWorld(), A, 990002); if (PushTrail.IsValid()) PushTrail->bDoneLaying = true;
		for (int32 a = 0; a < 8 && PushTrail.IsValid(); ++a)
		{
			const FVector2D D = (FVector2D(B->BallLocation()).GetSafeNormal()).GetRotated(45.f * a);
			const FVector Floor(B->BallLocation().X - D.X * 130.f, B->BallLocation().Y - D.Y * 130.f, B->BallLocation().Z - ZoogiConst::ZoogiRadius);
			if (PushTrail->AddShard(Floor, 1.0f) != INDEX_NONE) { PushDir = FVector(D, 0.f); break; }
		}
		if (PushDir.IsZero()) { Finish(TEXT("no clear spot near P2")); return; }
		Step = 2; StepT = 0.f; return;
	case 2:
		if (StepT < 0.9f) { VelBefore = FVector::DotProduct(B->GetBallVelocity(), PushDir); return; }
		if (StepT < 1.25f) return;
		{
			const float Gain = FVector::DotProduct(B->GetBallVelocity(), PushDir) - VelBefore;
			bPush = PushTrail.IsValid() && PushTrail->Bursts >= 1 && PushTrail->PushedZoogis >= 1 && Gain > 200.f;
			UE_LOG(LogZoogi, Log, TEXT("CRYSTAL_TEST push: P2 outward speed gain %.0f cm/s (bursts %d pushed %d)"), Gain, PushTrail.IsValid() ? PushTrail->Bursts : -1, PushTrail.IsValid() ? PushTrail->PushedZoogis : -1);
		}
		Step = 3; StepT = 0.f; return;
	case 3:
	{
		const int32 N = Trail.IsValid() ? Trail->Bursts : ShardsLaid; // a destroyed trail has burst everything
		if (N < ShardsLaid && StepT < Tu.BurstDelay + Tu.BurstStagger * Tu.MaxShards + 3.f) return;
		bBurst = ShardsLaid > 0 && N >= ShardsLaid;
		UE_LOG(LogZoogi, Log, TEXT("CRYSTAL_TEST bursts %d/%d"), N, ShardsLaid);
		Finish(TEXT("done"));
		return;
	}
	default: return;
	}
}
