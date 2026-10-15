// Crystal Collapse ice wall (Oct 14, Evan 9:33 PM). See ZoogiIceWall.h.
#include "ZoogiIceWall.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiFramedLayout.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	TArray<TWeakObjectPtr<AZoogiIceWall>>& Walls() { static TArray<TWeakObjectPtr<AZoogiIceWall>> W; return W; }
	constexpr int32 ShardsPerSeg = 3;
	constexpr float RiseTime = 0.35f;
	constexpr float DebrisTime = 0.75f;
}

void FZoogiIceWallTune::Load()
{
	const TCHAR* S = TEXT("ZoogiRoll.IceWall");
	if (GConfig)
	{
		GConfig->GetFloat(S, TEXT("Length"), Length, GGameIni);
		GConfig->GetFloat(S, TEXT("Height"), Height, GGameIni);
		GConfig->GetFloat(S, TEXT("Thickness"), Thickness, GGameIni);
		GConfig->GetInt(S, TEXT("Segments"), Segments, GGameIni);
		GConfig->GetFloat(S, TEXT("ShatterSpeed"), ShatterSpeed, GGameIni);
		GConfig->GetFloat(S, TEXT("PassThroughKeep"), PassThroughKeep, GGameIni);
		GConfig->GetBool(S, TEXT("bPerSegment"), bPerSegment, GGameIni);
		GConfig->GetFloat(S, TEXT("Offset"), Offset, GGameIni);
		GConfig->GetString(S, TEXT("Mesh"), Mesh, GGameIni);
		FString C;
		if (GConfig->GetString(S, TEXT("Glow"), C, GGameIni) && !C.IsEmpty()) Glow.InitFromString(C);
	}
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiIceWallShatter="), ShatterSpeed);
	Length = FMath::Clamp(Length, 100.f, 2000.f);
	Height = FMath::Clamp(Height, 30.f, 600.f);
	Thickness = FMath::Clamp(Thickness, 10.f, 200.f);
	Segments = FMath::Clamp(Segments, 1, 16);
	ShatterSpeed = FMath::Max(ShatterSpeed, 50.f);
	PassThroughKeep = FMath::Clamp(PassThroughKeep, 0.f, 1.f);
}

const FZoogiIceWallTune& AZoogiIceWall::Tune()
{
	static FZoogiIceWallTune T;
	static bool bLoaded = false;
	if (!bLoaded) { bLoaded = true; T.Load(); }
	return T;
}

AZoogiIceWall::AZoogiIceWall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics; // PrevVel = last frame's (pre-impact) velocity
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

AZoogiIceWall* AZoogiIceWall::SpawnWall(UWorld* World, const FVector& At, const FVector& ShotDir, int32 OwnerIdx, int64 EffectId)
{
	if (!World) return nullptr;
	FVector2D D(ShotDir.X, ShotDir.Y);
	if (!D.Normalize()) D = FVector2D(1.f, 0.f);
	const FZoogiIceWallTune& T = Tune();
	FVector Center(At.X + D.X * T.Offset, At.Y + D.Y * T.Offset, At.Z);
	{
		// Near the rim a wall across the shot line would hang past the knockout line: slide it in toward the court
		// centre (keeping its orientation) until the most segments fit.
		const FVector2D Ax(-D.Y, D.X), C0(Center);
		const FVector2D In = C0.IsNearlyZero() ? -D : -C0.GetSafeNormal();
		int32 Best = -1; FVector2D BestC = C0;
		for (int32 k = 0; k <= 16 && Best < T.Segments; ++k)
		{
			const FVector2D Ck = C0 + In * 100.f * k;
			const int32 N = ClearCount(World, Ck, Ax);
			if (N > Best) { Best = N; BestC = Ck; }
		}
		if (!BestC.Equals(C0, 1.f)) UE_LOG(LogZoogi, Log, TEXT("ICEWALL effect=%lld: moved %.0f cm in from the rim (%d/%d segments fit)"), EffectId, FVector2D::Distance(BestC, C0), Best, T.Segments);
		Center = FVector(BestC.X, BestC.Y, At.Z);
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiIceWall* W = World->SpawnActor<AZoogiIceWall>(AZoogiIceWall::StaticClass(), FTransform(Center), P);
	if (!W) return nullptr;
	W->Normal = D;
	W->Axis = FVector2D(-D.Y, D.X);
	W->Build(Center, OwnerIdx, EffectId);
	if (W->LiveCount() == 0)
	{
		UE_LOG(LogZoogi, Log, TEXT("ICEWALL effect=%lld: every segment blocked (Zoogi / orb / off court) - no wall"), EffectId);
		W->Destroy();
		return nullptr;
	}
	return W;
}

void AZoogiIceWall::Build(const FVector& Center, int32 OwnerIdx, int64 EffectId)
{
	const FZoogiIceWallTune& T = Tune();
	OwnerP = OwnerIdx; Effect = EffectId; BaseZ = Center.Z;
	Walls().RemoveAll([](const TWeakObjectPtr<AZoogiIceWall>& X) { return !X.IsValid(); });
	Walls().Add(this);
	UStaticMesh* Mesh = T.Mesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *T.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const bool bCone = Mesh == nullptr;
	if (!Mesh) Mesh = ZoogiUtil::BasicShape(TEXT("Cone"));
	UMaterialInstanceDynamic* GlowM = ZoogiUtil::FxMID(this, T.Glow, 0.45f);
	UMaterialInstanceDynamic* ConeM = bCone ? ZoogiUtil::GlowMID(this, T.Glow, 1.5f) : nullptr;
	const FBoxSphereBounds MB = Mesh ? Mesh->GetBounds() : FBoxSphereBounds(FVector::ZeroVector, FVector(50.f), 50.f);
	const FVector Ext = MB.BoxExtent.ComponentMax(FVector(1.f));
	const float SegLen = T.Length / T.Segments;
	FRandomStream R((int32)(EffectId & 0x7fffffff) ^ 0x1CE); // own stream: never touches GameRng
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
	int32 Blocked = 0;
	for (int32 s = 0; s < T.Segments; ++s)
	{
		const float U = ((s + 0.5f) / T.Segments - 0.5f) * T.Length;
		const FVector2D C2 = FVector2D(Center) + Axis * U;
		const FVector2D A2 = C2 - Axis * SegLen * 0.5f, B2 = C2 + Axis * SegLen * 0.5f;
		// never raise a segment inside a Zoogi / orb or past the knockout line
		const bool bOk = SegClear(GetWorld(), A2, B2);
		SegLive.Add(bOk); SegA.Add(A2); SegB.Add(B2);
		UStaticMeshComponent* Col = NewObject<UStaticMeshComponent>(this);
		Col->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
		Col->SetMobility(EComponentMobility::Movable);
		Col->SetupAttachment(Root);
		Col->SetCollisionProfileName(TEXT("BlockAll"));
		Col->SetCollisionEnabled(bOk ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Col->SetNotifyRigidBodyCollision(true);
		Col->SetHiddenInGame(true);
		Col->SetCastShadow(false);
		Col->RegisterComponent();
		Col->SetWorldTransform(FTransform(FRotator(0.f, Yaw, 0.f), FVector(C2.X, C2.Y, BaseZ + T.Height * 0.5f), FVector(SegLen / 100.f, T.Thickness / 100.f, T.Height / 100.f)));
		Colliders.Add(Col);
		if (!bOk) { ++Blocked; continue; }
		for (int32 k = 0; k < ShardsPerSeg; ++k)
		{
			UStaticMeshComponent* Sh = NewObject<UStaticMeshComponent>(this);
			Sh->SetStaticMesh(Mesh);
			Sh->SetMobility(EComponentMobility::Movable);
			Sh->SetupAttachment(Root);
			Sh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sh->SetCastShadow(false);
			if (ConeM) Sh->SetMaterial(0, ConeM);
			else if (GlowM) Sh->SetOverlayMaterial(GlowM);
			Sh->RegisterComponent();
			const float H = T.Height * R.FRandRange(0.72f, 1.05f) * (k == 1 ? 1.f : 0.85f);
			const float Wd = SegLen * 0.48f, Th = T.Thickness * 1.35f;
			const FVector Sc(Wd / (2.f * Ext.X), Th / (2.f * Ext.Y), H / (2.f * Ext.Z));
			const FVector2D P2 = C2 + Axis * (k - 1) * SegLen * 0.32f + D2Jit(R, T.Thickness * 0.15f);
			const FRotator Rot(R.FRandRange(-9.f, 9.f), Yaw + R.FRandRange(-22.f, 22.f), R.FRandRange(-9.f, 9.f));
			const float Z0 = BaseZ - (MB.Origin.Z - Ext.Z) * Sc.Z;
			Sh->SetWorldLocationAndRotation(FVector(P2.X, P2.Y, Z0), Rot);
			Sh->SetWorldScale3D(FVector(Sc.X, Sc.Y, Sc.Z * 0.02f));
			Shards.Add(Sh); ShardSeg.Add(s); ShardScale.Add(Sc);
		}
	}
	ZoogiUtil::PlaySfx(this, TEXT("StingFrost"), 0.8f, 1.1f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Blue"), Center + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, 1.2f);
	UE_LOG(LogZoogi, Log, TEXT("ICEWALL raised effect=%lld P%d at (%.0f,%.0f) len %.0f h %.0f segs %d (blocked %d) normal (%.2f,%.2f) shatter>=%.0f cm/s"),
		EffectId, OwnerIdx + 1, Center.X, Center.Y, T.Length, T.Height, T.Segments, Blocked, Normal.X, Normal.Y, T.ShatterSpeed);
}

bool AZoogiIceWall::SegClear(UWorld* World, const FVector2D& A2, const FVector2D& B2)
{
	const FZoogiIceWallTune& T = Tune();
	AZoogiGameMode* G = World ? World->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return true;
	const float Margin = T.Thickness * 0.5f + 30.f;
	if (G->Arena)
	{
		const FVector2D Mid = (A2 + B2) * 0.5f;
		for (const FVector2D& Q : { A2, Mid, B2 })
			if (G->Arena->IsOffCourt(Q + Q.GetSafeNormal() * Margin)) return false;
	}
	auto Clear = [&](const FVector2D& Q, float Rad)
	{
		const FVector2D AB = B2 - A2;
		const float K = FMath::Clamp(FVector2D::DotProduct(Q - A2, AB) / FMath::Max(AB.SizeSquared(), 1.f), 0.f, 1.f);
		return FVector2D::Distance(A2 + AB * K, Q) > Rad + T.Thickness * 0.5f + 4.f;
	};
	for (AZoogiBall* B : G->Balls) if (B && !B->bOut && !Clear(FVector2D(B->BallLocation()), ZoogiConst::ZoogiRadius)) return false;
	for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && !Clear(FVector2D(O->OrbLocation()), ZoogiConst::OrbRadius)) return false;
	return true;
}

int32 AZoogiIceWall::ClearCount(UWorld* World, const FVector2D& Center, const FVector2D& Ax)
{
	const FZoogiIceWallTune& T = Tune();
	const float SegLen = T.Length / T.Segments;
	int32 N = 0;
	for (int32 s = 0; s < T.Segments; ++s)
	{
		const FVector2D C2 = Center + Ax * (((s + 0.5f) / T.Segments - 0.5f) * T.Length);
		N += SegClear(World, C2 - Ax * SegLen * 0.5f, C2 + Ax * SegLen * 0.5f) ? 1 : 0;
	}
	return N;
}

FVector2D AZoogiIceWall::D2Jit(FRandomStream& R, float A) { return FVector2D(R.FRandRange(-A, A), R.FRandRange(-A, A)); }

int32 AZoogiIceWall::LiveCount() const { int32 N = 0; for (bool b : SegLive) N += b ? 1 : 0; return N; }

void AZoogiIceWall::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	// rise (0.35 s ease-out) and shatter debris (the segment's own shard meshes fly out: no spawns, nothing to pool)
	const float Rise = FMath::Clamp(Age / RiseTime, 0.f, 1.f);
	const float E = 1.f - FMath::Square(1.f - Rise);
	bool bDebris = false;
	for (int32 i = 0; i < Shards.Num(); ++i)
	{
		UStaticMeshComponent* Sh = Shards[i];
		if (!Sh) continue;
		if (SegLive[ShardSeg[i]])
		{
			if (Rise < 1.f || Age - Dt < RiseTime) Sh->SetWorldScale3D(FVector(ShardScale[i].X, ShardScale[i].Y, ShardScale[i].Z * FMath::Max(0.02f, E)));
		}
		else if (Debris.IsValidIndex(i) && Debris[i].Life > 0.f)
		{
			FDebris& Db = Debris[i];
			Db.Life -= Dt;
			Db.Vel.Z -= 1800.f * Dt;
			Sh->SetWorldLocation(Sh->GetComponentLocation() + Db.Vel * Dt);
			Sh->AddWorldRotation(FRotator(Db.Spin * Dt, Db.Spin * 0.7f * Dt, 0.f));
			const float K = FMath::Clamp(Db.Life / DebrisTime, 0.f, 1.f);
			Sh->SetWorldScale3D(ShardScale[i] * 0.45f * K);
			if (Db.Life <= 0.f) Sh->SetVisibility(false);
			else bDebris = true;
		}
	}
	if (LiveCount() == 0 && !bDebris && Age > RiseTime) { Destroy(); return; }
	if (AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>())
	{
		PrevVel.SetNum(G->Balls.Num());
		for (int32 i = 0; i < G->Balls.Num(); ++i) PrevVel[i] = G->Balls[i] ? G->Balls[i]->GetBallVelocity() : FVector::ZeroVector;
	}
}

void AZoogiIceWall::OnBallHit(AZoogiBall* Ball, UPrimitiveComponent* Comp, const FVector& NormalImpulse, const FHitResult& Hit)
{
	if (!Ball) return;
	const FZoogiIceWallTune& T = Tune();
	int32 Seg = Colliders.IndexOfByKey(Cast<UStaticMeshComponent>(Comp));
	if (!SegLive.IsValidIndex(Seg) || !SegLive[Seg]) return;
	const FVector V = PrevVel.IsValidIndex(Ball->PlayerIndex) ? PrevVel[Ball->PlayerIndex] : Ball->GetBallVelocity();
	const float Speed = FMath::Abs(FVector2D::DotProduct(FVector2D(V.X, V.Y), Normal));
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHitLog > 0.2f)
	{
		LastHitLog = Now;
		UE_LOG(LogZoogi, Log, TEXT("ICEWALL hit P%d seg %d speed %.0f (threshold %.0f) -> %s"), Ball->PlayerIndex + 1, Seg, Speed, T.ShatterSpeed, Speed >= T.ShatterSpeed ? TEXT("SHATTER") : TEXT("holds"));
	}
	if (Speed < T.ShatterSpeed)
	{
		// a solid knock: a little frost puff so the hit reads
		AZoogiFX::Spawn(GetWorld(), Hit.ImpactPoint, T.Glow, 60.f, 0.2f, false, 5.f);
		return;
	}
	if (T.bPerSegment) ShatterSegment(Seg, Hit.ImpactPoint, Speed, Ball);
	else for (int32 s = 0; s < SegLive.Num(); ++s) if (SegLive[s]) ShatterSegment(s, Hit.ImpactPoint, Speed, Ball);
	// smash through: keep part of the pre-hit velocity instead of the bounce
	if (T.PassThroughKeep > 0.f && Ball->Sphere) Ball->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, 0.f) * T.PassThroughKeep);
	ZoogiFramed::FrameShake(FMath::Clamp(Speed / (T.ShatterSpeed * 2.f), 0.4f, 1.f), GetWorld());
}

void AZoogiIceWall::ShatterSegment(int32 Seg, const FVector& At, float Speed, AZoogiBall* By)
{
	if (!SegLive.IsValidIndex(Seg) || !SegLive[Seg]) return;
	SegLive[Seg] = false;
	if (UStaticMeshComponent* C = Colliders[Seg]) C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Debris.SetNum(Shards.Num());
	FRandomStream R((int32)(Effect & 0x7fffffff) ^ (Seg * 7919));
	const FVector2D Push = By ? FVector2D(By->GetBallVelocity()).GetSafeNormal() : Normal;
	for (int32 i = 0; i < Shards.Num(); ++i)
	{
		if (ShardSeg[i] != Seg || !Shards[i]) continue;
		const FVector2D Out = (Push * 0.7f + FVector2D(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f)) * 0.6f).GetSafeNormal();
		Debris[i].Vel = FVector(Out.X, Out.Y, 0.f) * R.FRandRange(350.f, 700.f) + FVector(0.f, 0.f, R.FRandRange(450.f, 800.f));
		Debris[i].Spin = R.FRandRange(-540.f, 540.f);
		Debris[i].Life = DebrisTime;
	}
	const FVector2D M = (SegA[Seg] + SegB[Seg]) * 0.5f;
	const FVector P(M.X, M.Y, BaseZ + Tune().Height * 0.45f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_White"), P, FRotator::ZeroRotator, 1.3f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Blue"), P, FRotator::ZeroRotator, 1.f);
	AZoogiFX::Spawn(GetWorld(), FVector(M.X, M.Y, BaseZ + 5.f), Tune().Glow, 150.f, 0.35f, true, 8.f);
	ZoogiUtil::PlaySfx(this, TEXT("RockShatter"), 1.f, FMath::FRandRange(1.05f, 1.2f));
	UE_LOG(LogZoogi, Log, TEXT("ICEWALL shatter effect=%lld seg %d by P%d at %.0f cm/s: collision removed (%d live)"), Effect, Seg, By ? By->PlayerIndex + 1 : 0, Speed, LiveCount());
}

void AZoogiIceWall::EndPlay(const EEndPlayReason::Type Reason)
{
	Walls().RemoveAll([this](const TWeakObjectPtr<AZoogiIceWall>& X) { return !X.IsValid() || X.Get() == this; });
	Super::EndPlay(Reason);
}

void AZoogiIceWall::LiveSegments(const UWorld* World, TArray<FSeg2D>& Out)
{
	for (const TWeakObjectPtr<AZoogiIceWall>& WP : Walls())
	{
		const AZoogiIceWall* W = WP.Get();
		if (!W || W->GetWorld() != World || W->IsActorBeingDestroyed()) continue;
		for (int32 s = 0; s < W->SegLive.Num(); ++s)
			if (W->SegLive[s]) { FSeg2D S; S.A = W->SegA[s]; S.B = W->SegB[s]; S.N = W->Normal; S.Half = Tune().Thickness * 0.5f; Out.Add(S); }
	}
}

int32 AZoogiIceWall::NumWalls(const UWorld* World)
{
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiIceWall>& WP : Walls()) if (WP.IsValid() && WP->GetWorld() == World && !WP->IsActorBeingDestroyed()) ++N;
	return N;
}

void AZoogiIceWall::DestroyAll(UWorld* World, const TCHAR* Why)
{
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiIceWall>& WP : TArray<TWeakObjectPtr<AZoogiIceWall>>(Walls()))
		if (AZoogiIceWall* W = WP.Get()) if (W->GetWorld() == World) { W->Destroy(); ++N; }
	if (N) UE_LOG(LogZoogi, Log, TEXT("ICEWALL cleared %d wall(s) (%s)"), N, Why);
}
