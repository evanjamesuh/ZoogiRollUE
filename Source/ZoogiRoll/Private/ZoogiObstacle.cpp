#include "ZoogiObstacle.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiRoll.h"
#include "ZoogiArena.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiFX.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Oct 15 dev test -ZoogiR2OrbTest (Frozen Ring 2 outer pocket orb at yaw 90): once the first turn reaches Aiming the turn flow
// is held and P2 is rolled from Ring 2 (r1900) into the pocket orb three times: (1) pickup, (2) again 3 s later while
// recharging (must not pay), (3) again after the orb is back (12 s recharge). Logs R2ORBTEST lines (energy before/after,
// measured recharge, orb visibility) and, with -ZoogiShots=<dir>, screenshots from a fixed camera on the pocket.
namespace ZoogiR2OrbTest
{
	struct FState
	{
		int32 Step = -1; float T0 = 0.f, Pick[2] = { -1.f, -1.f }, Shown = -1.f, Shot = -1.f; FString ShotName;
		float E0 = 0.f, E1 = 0.f, E2 = 0.f, E3 = 0.f; FVector Home = FVector::ZeroVector; int32 NPick = 0;
		TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> PrevView;
	};
	FState S;
	bool On() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiR2OrbTest")); return b; }
}

namespace ZoogiRelaunch
{
	struct FState { double Last = -1e9; int32 Turn = -1; int32 Count = 0; };
	static TMap<TPair<const void*, const void*>, FState> GState;
	static int32 GBlocked = 0;
	static double GLastNow = 0.0;
	static void Tune(float& Cool, int32& Max)
	{
		static float C = -1.f; static int32 M = -1;
		if (C < 0.f)
		{
			C = 1.5f; M = 2;
			if (GConfig) { GConfig->GetFloat(TEXT("ZoogiRoll.Relaunch"), TEXT("Cooldown"), C, GGameIni); GConfig->GetInt(TEXT("ZoogiRoll.Relaunch"), TEXT("MaxPerTurn"), M, GGameIni); }
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiRelaunchCool="), C);
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiRelaunchMax="), M);
			C = FMath::Max(C, 0.f); M = FMath::Max(M, 0);
			UE_LOG(LogZoogi, Log, TEXT("RELAUNCH guard cooldown=%.2fs max/turn=%d"), C, M);
		}
		Cool = C; Max = M;
	}
	bool Allow(const UObject* Pad, const AActor* Who, int32 Turn, double Now, const TCHAR* PadName)
	{
		if (Now < GLastNow) GState.Reset(); // new world / match
		GLastNow = Now;
		float Cool; int32 Max; Tune(Cool, Max);
		FState& S = GState.FindOrAdd(TPair<const void*, const void*>(Pad, Who));
		if (S.Turn != Turn) { S.Turn = Turn; S.Count = 0; }
		const bool bCool = Now - S.Last < Cool;
		const bool bCap = Max > 0 && S.Count >= Max;
		if (bCool || bCap)
		{
			++GBlocked;
			UE_LOG(LogZoogi, Log, TEXT("RELAUNCH blocked %s -> %s (%s, %d this turn, %.2fs since last)"), PadName, *GetNameSafe(Who), bCap ? TEXT("max per turn") : TEXT("cooldown"), S.Count, Now - S.Last);
			return false;
		}
		S.Last = Now; ++S.Count;
		return true;
	}
	int32 Blocked() { return GBlocked; }
}

AZoogiObstacle::AZoogiObstacle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	Tags.Add(TEXT("Unbreakable")); Tags.Add(TEXT("NonDestructible")); Tags.Add(TEXT("CrystalCollapseExclude")); Tags.Add(TEXT("ZoogiObstacle"));
}

UStaticMeshComponent* AZoogiObstacle::AddVisual(UStaticMesh* Mesh, float Scale, bool bSpent)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetupAttachment(Root);
	C->SetRelativeScale3D(FVector(Scale));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->ComponentTags.Add(TEXT("NoCamBlock"));
	C->SetVisibility(!bSpent);
	C->RegisterComponent();
	(bSpent ? Spent : Vis).Add(C);
	return C;
}

void AZoogiObstacle::BuildCollider(const TArray<TArray<FVector>>& Hulls, float Scale, UPhysicalMaterial* PM)
{
	if (Hulls.Num() == 0) return;
	Col = NewObject<UProceduralMeshComponent>(this, TEXT("ObsCollider"));
	Col->SetMobility(EComponentMobility::Movable);
	Col->SetupAttachment(Root);
	Col->bUseComplexAsSimpleCollision = false;
	Col->bUseAsyncCooking = false;
	for (const TArray<FVector>& H : Hulls)
	{
		TArray<FVector> S; S.Reserve(H.Num());
		for (const FVector& V : H) S.Add(V * Scale);
		Col->AddCollisionConvexMesh(S);
	}
	Col->SetVisibility(false);
	Col->SetHiddenInGame(true);
	Col->SetCollisionProfileName(TEXT("BlockAll"));
	Col->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); // NoCamBlock
	if (PM) Col->SetPhysMaterialOverride(PM);
	Col->SetNotifyRigidBodyCollision(true);
	Col->OnComponentHit.AddDynamic(this, &AZoogiObstacle::OnColHit);
	Col->RegisterComponent();
	NumHulls = Hulls.Num();
}

void AZoogiObstacle::OnColHit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (BounceGain <= 1.f) return;
	if (AZoogiBall* B = Cast<AZoogiBall>(Other)) PendingGain.AddUnique(B); // applied after the physics step resolved the bounce
}

bool AZoogiObstacle::Overlaps(const FZoogiObsTrigger& T, const FVector& BallW, float R) const
{
	const FVector L = GetActorTransform().InverseTransformPositionNoScale(BallW);
	switch (T.Shape)
	{
	case 0: { const FVector Q = (L - T.C).GetAbs() - T.E; return FVector(FMath::Max(Q.X, 0.f), FMath::Max(Q.Y, 0.f), FMath::Max(Q.Z, 0.f)).Size() <= R; }
	case 1: return FVector::Dist(L, T.C) <= T.E.X + R;
	case 2: return FVector2D::Distance(FVector2D(L), FVector2D(T.C)) <= T.E.X + R * 0.5f && FMath::Abs(L.Z - T.C.Z) <= T.E.Z + R;
	case 3:
	{
		const float Rho = FVector2D(L).Size();
		const float A = FMath::RadiansToDegrees(FMath::Atan2(L.Y, L.X));
		const float Rel = FMath::Fmod(A - T.D.X + 720.f, 360.f);
		return Rel <= T.D.Y - T.D.X && Rho >= T.E.X && Rho <= T.E.Y && L.Z <= T.E.Z + R + 5.f;
	}
	case 4: // world arc around the arena centre (world 0,0)
	{
		const float Rho = FVector2D(BallW).Size();
		const float A = FMath::RadiansToDegrees(FMath::Atan2(BallW.Y, BallW.X));
		const float Rel = FMath::Fmod(A - T.D.X + 720.f, 360.f);
		return Rel <= T.D.Y - T.D.X && Rho >= T.E.X && Rho <= T.E.Y && BallW.Z <= T.E.Z + R + 5.f;
	}
	}
	return false;
}

void AZoogiObstacle::SetOrbShown(bool bShown)
{
	if (Spent.Num()) { for (auto& C : Vis) if (C) C->SetVisibility(bShown); for (auto& C : Spent) if (C) C->SetVisibility(!bShown); }
	else
	{
		bool bAny = false;
		for (auto& C : Vis) if (C && C->GetStaticMesh() && C->GetStaticMesh()->GetName().Contains(TEXT("Orb")) && !C->GetStaticMesh()->GetName().Contains(TEXT("Cup"))) { C->SetVisibility(bShown); bAny = true; }
		if (!bAny || Vis.Num() == 1) for (auto& C : Vis) if (C) C->SetVisibility(bShown);
	}
}

float AZoogiObstacle::EdgeR() const { return AZoogiArena::ActiveKnockoffR; }

// Launch tuning (Oct 14): a launch must land on the arena. Ballistic landing (g 980, back to the start height); if it would land past
// 70% of the ring-out radius the horizontal speed is scaled down until it lands there. Returns the landing radius.
float AZoogiObstacle::SafeLaunch(const FVector& P, FVector& H, float Up) const
{
	const float T = 2.f * FMath::Max(Up, 0.f) / 980.f;
	const float Lim = EdgeR() * 0.7f;
	auto LandR = [&](float K) { return FVector2D(P.X + H.X * K * T, P.Y + H.Y * K * T).Size(); };
	float K = 1.f;
	if (LandR(1.f) > Lim) { float Lo = 0.f, Hi = 1.f; for (int32 i = 0; i < 20; ++i) { const float M = 0.5f * (Lo + Hi); (LandR(M) > Lim ? Hi : Lo) = M; } K = Lo; }
	H *= K;
	return LandR(1.f);
}

void AZoogiObstacle::Fire(FZoogiObsTrigger& T, AZoogiBall* B, float Now)
{
	const float Yaw = GetActorRotation().Yaw;
	const FVector DirW = FRotator(0.f, Yaw, 0.f).RotateVector(FVector(T.D.X, T.D.Y, 0.f)).GetSafeNormal();
	const FVector V = B->GetBallVelocity();
	const FVector Pl(V.X, V.Y, 0.f);
	switch (T.Kind)
	{
	case 1: // Boost
	{
		if (T.P[1] > 0.f) // multiplier along the axis, only moving with it
		{
			if (FVector::DotProduct(Pl, DirW) <= 0.f) return;
			B->SetPlanarVelocity(DirW * FMath::Min(Pl.Size() * T.P[1], MaxBoost));
		}
		else { FVector NV = Pl + DirW * T.P[0]; B->SetPlanarVelocity(NV.GetClampedToMaxSize(MaxBoost)); }
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s boost P%d %.0f -> %.0f cm/s"), *ObsName.ToString(), B->PlayerIndex + 1, Pl.Size(), B->GetBallVelocity().Size2D());
		break;
	}
	case 2: // Launch (P0 < 0 = keep the horizontal velocity)
	{
		if (Pl.Size() < T.P[3]) return;
		FVector H = T.P[0] < 0.f ? Pl : DirW * T.P[0];
		const float Land = SafeLaunch(B->BallLocation(), H, T.P[1]);
		B->SetPlanarVelocity(H, T.P[1]);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s launch P%d fwd %.0f up %.0f -> lands r%.0f (edge r%.0f)"), *ObsName.ToString(), B->PlayerIndex + 1, H.Size(), T.P[1], Land, EdgeR());
		break;
	}
	case 5: // heading snap (world yaw), speed kept
	{
		const FVector Dir = FRotator(0.f, T.P[0], 0.f).Vector();
		if (FVector::DotProduct(Pl, Dir) <= 0.f) return;
		B->SetPlanarVelocity(Dir * Pl.Size());
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s snap P%d heading %.0f"), *ObsName.ToString(), B->PlayerIndex + 1, T.P[0]);
		break;
	}
	case 6: // set a minimum speed along the axis (one-way when P1 > 0)
	{
		if (T.P[1] > 0.f && FVector::DotProduct(Pl, DirW) <= 0.f) return;
		const FVector NewV = DirW * FMath::Min(FMath::Max(Pl.Size(), T.P[0]), MaxBoost);
		B->SetPlanarVelocity(NewV);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s boost P%d %.0f -> %.0f cm/s"), *ObsName.ToString(), B->PlayerIndex + 1, Pl.Size(), NewV.Size());
		break;
	}
	case 7: // two-way multiplier along the axis with a minimum exit speed
	{
		const float Sg = FVector::DotProduct(Pl, DirW) >= 0.f ? 1.f : -1.f;
		const FVector NewV = DirW * Sg * FMath::Min(FMath::Max(Pl.Size() * T.P[0], T.P[1]), MaxBoost);
		B->SetPlanarVelocity(NewV);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s boost P%d %.0f -> %.0f cm/s"), *ObsName.ToString(), B->PlayerIndex + 1, Pl.Size(), NewV.Size());
		break;
	}
	case 3: // Energy orb
	{
		if (T.RechargeAt > Now) return;
		UZoogiEpisodeEnergyComponent* En = B->FindComponentByClass<UZoogiEpisodeEnergyComponent>();
		const float Got = En ? En->Gain(T.P[0], FName(TEXT("obstacle_orb")), 0) : 0.f;
		T.RechargeAt = Now + T.P[1];
		SetOrbShown(false);
		// Oct 15: pickup flash (energy-blue sphere + ground ring) - was only the orb hiding
		AZoogiFX::Spawn(GetWorld(), GetActorLocation(), FLinearColor(0.45f, 0.9f, 1.f), 150.f, 0.55f, false, 8.f);
		AZoogiFX::Spawn(GetWorld(), GetActorLocation() - FVector(0, 0, 30.f), FLinearColor(0.45f, 0.9f, 1.f), 190.f, 0.7f, true, 6.f);
		if (ZoogiR2OrbTest::On() && ZoogiR2OrbTest::S.Step >= 0 && ZoogiR2OrbTest::S.NPick < 2) ZoogiR2OrbTest::S.Pick[ZoogiR2OrbTest::S.NPick++] = Now;
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s energy P%d +%.0f (%s), recharge %.0f s"), *ObsName.ToString(), B->PlayerIndex + 1, Got, En ? TEXT("episode energy") : TEXT("no energy component"), T.P[1]);
		break;
	}
	case 4: // Zone entry: speed bonus (the rolling-decel change is applied while inside)
		if (T.P[2] > 0.f && Pl.Size() > 1.f) B->SetPlanarVelocity(Pl * T.P[2]);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s zone P%d decel %.0f -> %.0f"), *ObsName.ToString(), B->PlayerIndex + 1, T.Inside.FindRef(B), B->RollingDecel);
		break;
	}
}

void AZoogiObstacle::Tick(float Dt)
{
	Super::Tick(Dt);
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return;
	const float Now = W->GetTimeSeconds();
	for (FZoogiObsTrigger& T : Triggers)
		if (T.Kind == 3 && T.RechargeAt > 0.f && Now >= T.RechargeAt)
		{
			T.RechargeAt = -1.f; SetOrbShown(true);
			AZoogiFX::Spawn(GetWorld(), GetActorLocation() - FVector(0, 0, 30.f), FLinearColor(0.45f, 0.9f, 1.f), 110.f, 0.5f, true, 4.f); // recharged: small ring
			if (ZoogiR2OrbTest::On() && ZoogiR2OrbTest::S.Step >= 0 && ZoogiR2OrbTest::S.Shown < 0.f) ZoogiR2OrbTest::S.Shown = Now;
		}
	if (ZoogiR2OrbTest::On() && ObsName == FName(TEXT("Ring2_OuterOrb_90"))) TickOrbTest(G, Now);
	for (TWeakObjectPtr<AZoogiBall>& WB : PendingGain)
	{
		AZoogiBall* B = WB.Get();
		if (!B || B->bOut || GainCool.FindRef(WB) > Now) continue;
		const FVector V = B->GetBallVelocity(); const FVector Pl(V.X, V.Y, 0.f);
		const FVector Away = FVector(B->BallLocation() - GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(Pl, Away) <= 0.f && Col) continue; // still inbound: nothing to amplify yet
		const float NewS = FMath::Min(Pl.Size() * BounceGain, FMath::Max(MaxOut, Pl.Size()));
		if (Pl.Size() > 1.f) B->SetPlanarVelocity(Pl.GetSafeNormal() * NewS);
		GainCool.Add(WB, Now + 0.25f);
		UE_LOG(LogZoogi, Log, TEXT("OBSTACLE %s bounce P%d %.0f -> %.0f cm/s (gain %.2f)"), *ObsName.ToString(), B->PlayerIndex + 1, Pl.Size(), NewS, BounceGain);
	}
	PendingGain.Reset();
	if (Triggers.Num() == 0) return;
	for (AZoogiBall* B : G->Balls)
	{
		if (!B || B->bOut || B->bKnockedOut) continue;
		const float R = B->Sphere ? B->Sphere->GetScaledSphereRadius() : 50.f;
		const FVector P = B->BallLocation();
		for (FZoogiObsTrigger& T : Triggers)
		{
			const bool bIn = Overlaps(T, P, R);
			const bool bWas = T.Inside.Contains(B);
			if (bIn && !bWas)
			{
				// Oct 15: one obstacle's zone pieces (Arabian carpet lanes = 4 overlapping segments, x0.4 decel, x1.15 speed each)
				// act once: the first piece entered owns the saved decel, later pieces are no-ops (was 450 -> 1 -> 0 compounding)
				bool bOtherPiece = false;
				if (T.Kind == 4)
					for (const FZoogiObsTrigger& O : Triggers)
						if (&O != &T && O.Kind == 4 && O.Inside.Contains(B)) { bOtherPiece = true; break; }
				T.Inside.Add(B, bOtherPiece ? -1.f : B->RollingDecel);
				if (bOtherPiece) continue;
				if (T.Kind == 4) { if (T.P[0] > 0.f) B->RollingDecel = T.P[0]; else if (T.P[1] > 0.f) B->RollingDecel *= T.P[1]; }
				const bool bLauncher = T.Kind == 1 || T.Kind == 2 || T.Kind == 5 || T.Kind == 6 || T.Kind == 7;
				if (T.Cool.FindRef(B) <= Now && (!bLauncher || ZoogiRelaunch::Allow(this, B, G->TurnCounter, Now, *ObsName.ToString()))) { Fire(T, B, Now); T.Cool.Add(B, Now + FMath::Max(T.Kind == 3 ? 0.f : (T.Kind == 4 ? T.P[3] : T.P[2]), 0.05f)); }
			}
			else if (!bIn && bWas)
			{
				const float Base = T.Inside.FindRef(B);
				T.Inside.Remove(B);
				if (T.Kind == 4 && Base >= 0.f)
				{
					// leave the zone: back to the decel it had - unless still on another piece of it (hand the saved decel over)
					FZoogiObsTrigger* Next = nullptr;
					for (FZoogiObsTrigger& O : Triggers)
						if (O.Kind == 4 && O.Inside.Contains(B)) { Next = &O; break; }
					if (Next) Next->Inside.Add(B, Base);
					else B->RollingDecel = Base;
				}
			}
		}
	}
}

void AZoogiObstacle::TickOrbTest(AZoogiGameMode* G, float Now)
{
	using namespace ZoogiR2OrbTest;
	if (S.Step >= 99 || G->Balls.Num() < 2 || !G->Balls[1]) return;
	AZoogiBall* B = G->Balls[1];
	UZoogiEpisodeEnergyComponent* En = B->FindComponentByClass<UZoogiEpisodeEnergyComponent>();
	const float E = En ? En->Value : -1.f;
	const FZoogiObsTrigger* Tr = Triggers.Num() ? &Triggers[0] : nullptr;
	const bool bShown = Tr && Tr->RechargeAt < 0.f;
	auto Roll = [&](float R, float Speed)
	{
		const float Z = -30.f + (B->Sphere ? B->Sphere->GetScaledSphereRadius() : 50.f) + 3.f;
		B->Sphere->SetWorldLocation(FVector(0.f, R, Z), false, nullptr, ETeleportType::TeleportPhysics);
		B->Sphere->SetPhysicsLinearVelocity(FVector(0.f, Speed, 0.f));
		B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	};
	FString Dir; FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), Dir);
	auto Shot = [&](const TCHAR* Name)
	{
		if (Dir.IsEmpty()) return;
		const FString P = FPaths::Combine(Dir, FString::Printf(TEXT("r2orb_%s.png"), Name));
		FScreenshotRequest::RequestScreenshot(P, true, false);
		UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST screenshot %s (energy P2 %.0f, orb %s)"), *P, E, bShown ? TEXT("shown") : TEXT("hidden"));
	};
	if (S.Cam.IsValid())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->GetViewTarget() != S.Cam.Get()) PC->SetViewTarget(S.Cam.Get());
	if (S.Step < 0)
	{
		if (G->Phase != EZoogiPhase::Aiming) return;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) S.PrevView = PC->GetViewTarget();
		S.Step = 0; S.T0 = Now; bOrbTestHold = true; S.Home = B->BallLocation(); S.E0 = E;
		const FVector Look(0.f, 2120.f, -30.f), Loc = Look + FVector(0.f, -900.f, 620.f);
		if (ACameraActor* C = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Look - Loc).Rotation(), Loc)))
		{ C->GetCameraComponent()->bConstrainAspectRatio = false; C->GetCameraComponent()->SetFieldOfView(70.f); S.Cam = C; }
		Roll(1900.f, 0.f);
		UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST start: P2 energy %.0f, orb %s, turn flow held"), E, bShown ? TEXT("shown") : TEXT("hidden"));
		return;
	}
	const float T = Now - S.T0;
	if (S.Shot >= 0.f && Now >= S.Shot) { Shot(*S.ShotName); S.Shot = -1.f; }
	switch (S.Step)
	{
	case 0: if (T > 1.5f) { Shot(TEXT("0_before")); Roll(1900.f, 420.f); UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST run 1: P2 rolled r1900 -> pocket at 420 cm/s")); S.Step = 1; } break;
	case 1:
		if (S.NPick >= 1 && S.Shot < 0.f && S.ShotName != TEXT("1_pickup_flash")) { S.Shot = S.Pick[0] + 0.12f; S.ShotName = TEXT("1_pickup_flash"); }
		if (T > 4.0f) { S.E1 = E; Shot(TEXT("2_after_pickup")); UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST run 1 result: pickups %d, energy %.0f -> %.0f, orb %s"), S.NPick, S.E0, S.E1, bShown ? TEXT("shown") : TEXT("hidden")); S.Step = 2; }
		break;
	case 2: if (T > 5.0f) { Roll(1900.f, 420.f); UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST run 2 (recharging): P2 rolled into the pocket again")); S.Step = 3; } break;
	case 3:
		if (T > 8.0f) { S.E2 = E; UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST run 2 result: pickups %d, energy %.0f -> %.0f (expect no gain while recharging)"), S.NPick, S.E1, S.E2); Roll(1900.f, 0.f); S.Step = 4; }
		break;
	case 4:
		if (S.Shown > 0.f && S.Shot < 0.f && S.ShotName != TEXT("3_recharged")) { S.Shot = S.Shown + 0.3f; S.ShotName = TEXT("3_recharged"); }
		if (S.Shown > 0.f && Now > S.Shown + 1.0f) { Roll(1900.f, 420.f); UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST run 3: orb back after %.2f s, P2 rolled in again"), S.Shown - S.Pick[0]); S.Step = 5; }
		if (T > 25.f) S.Step = 5;
		break;
	case 5:
		if (S.NPick >= 2 && S.Shot < 0.f && S.ShotName != TEXT("4_second_pickup")) { S.Shot = S.Pick[1] + 0.12f; S.ShotName = TEXT("4_second_pickup"); }
		if (T > 30.f || (S.NPick >= 2 && Now > S.Pick[1] + 2.f))
		{
			S.E3 = E;
			const float Rech = S.Shown > 0.f ? S.Shown - S.Pick[0] : -1.f;
			const bool bPass = S.NPick == 2 && FMath::IsNearlyEqual(S.E1 - S.E0, 10.f, 0.5f) && FMath::IsNearlyEqual(S.E2, S.E1, 0.5f) && FMath::IsNearlyEqual(S.E3 - S.E2, 10.f, 0.5f) && FMath::Abs(Rech - 12.f) < 0.25f;
			UE_LOG(LogZoogi, Log, TEXT("R2ORBTEST result=%s pickups=%d energy %.0f -> %.0f (run1 %+.0f) -> %.0f (run2 while recharging %+.0f) -> %.0f (run3 %+.0f) recharge measured %.2f s (expect 12)"),
				bPass ? TEXT("PASS") : TEXT("FAIL"), S.NPick, S.E0, S.E1, S.E1 - S.E0, S.E2, S.E2 - S.E1, S.E3, S.E3 - S.E2, Rech);
			B->Sphere->SetWorldLocation(S.Home, false, nullptr, ETeleportType::TeleportPhysics); B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (S.PrevView.IsValid()) PC->SetViewTarget(S.PrevView.Get());
			if (S.Cam.IsValid()) S.Cam->Destroy();
			bOrbTestHold = false; S.Step = 99;
		}
		break;
	}
}
