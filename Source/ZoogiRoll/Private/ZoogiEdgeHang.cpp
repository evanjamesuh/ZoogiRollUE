// Nov 1 b58 (Evan): edge hang mini-game - see ZoogiEdgeHang.h.
#include "ZoogiEdgeHang.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiActionFun.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCards.h"
#include "ZoogiAurexPuppet.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CollisionQueryParams.h"

namespace
{
	struct FHang
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AZoogiBall> Ball;
		TWeakObjectPtr<AZoogiAurexPuppet> Pup;
		TWeakObjectPtr<ACameraActor> Cam;
		TWeakObjectPtr<AActor> PrevView;
		FVector Ledge = FVector::ZeroVector, Out = FVector::ForwardVector, Pin = FVector::ZeroVector, DropV = FVector::ZeroVector, ClimbFrom = FVector::ZeroVector;
		ECollisionEnabled::Type Coll = ECollisionEnabled::QueryAndPhysics;
		float Imp = 0.f, AutoAcc = 0.f, AutoRate = 0.f, Age = 0.f, FloorZ = 0.f;
		bool bCards = false, bDebt = false;
		ZoogiEdgeHang::FView V;
		TMap<TWeakObjectPtr<AZoogiBall>, FVector> Safe;
		TSet<TWeakObjectPtr<AZoogiBall>> Dropped;
		int32 TestStep = 0; float TestNext = 14.f;
	};
	FHang GHangS;

	bool Flag(const TCHAR* N) { return FParse::Param(FCommandLine::Get(), N); }
	// b147a: a Zoogi braced against the boss sweep gets the b58 mini-game (free) for a few seconds
	TMap<TWeakObjectPtr<AZoogiBall>, double> GForce; bool GForceNow = false;
	bool ForcedB(const AZoogiBall* B) { const double* T = B ? GForce.Find(B) : nullptr; return T && FPlatformTime::Seconds() < *T; }
	bool HangVisualOnly() { static const bool bGame = Flag(TEXT("ZoogiEdgeHangGame")); return !bGame && !GForceNow; }

	void Shot(AZoogiGameMode* G, const TCHAR* Name)
	{
		static const bool bShots = Flag(TEXT("ZoogiHangTest"));
		if (bShots && G && !G->ShotDir.IsEmpty()) G->TakeShot(Name);
	}

	void ReleaseCam(UWorld* W)
	{
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (ACameraActor* C = GHangS.Cam.Get())
		{
			if (PC && PC->GetViewTarget() == C) PC->SetViewTargetWithBlend(GHangS.PrevView.IsValid() ? GHangS.PrevView.Get() : PC->GetPawn(), 0.25f);
			C->Destroy();
		}
		GHangS.Cam.Reset(); GHangS.PrevView.Reset();
	}

	void TapNow(AZoogiGameMode* G)
	{
		GHangS.V.Prog = FMath::Min(1.f, GHangS.V.Prog + ZoogiEdgeHang::PerTap);
		GHangS.Imp = 1.f;
		GHangS.V.LastTap = G->GetWorld()->GetRealTimeSeconds();
		ZoogiUtil::PlaySfx(G, TEXT("Pull"), 0.55f, FMath::FRandRange(0.9f, 1.1f));
		if (AZoogiFX* X = AZoogiFX::Spawn(G->GetWorld(), GHangS.Pin + FVector(0.f, 0.f, 4.f), ZoogiUtil::PlayerColor(GHangS.V.Player), 55.f, 0.2f, true, 4.f)) X->CustomTimeDilation = 1.f / ZoogiEdgeHang::Dilation; // real speed in the hang slow-mo
	}

	void Finish(AZoogiGameMode* G, bool bSaved)
	{
		if (GForceNow) { GForceNow = false; if (AZoogiBall* Fb = GHangS.Ball.Get()) GForce.Remove(Fb); } // b147a
		UWorld* W = G ? G->GetWorld() : nullptr;
		AZoogiBall* B = GHangS.Ball.Get();
		AZoogiAurexPuppet* P = GHangS.Pup.Get();
		if (B && B->Sphere)
		{
			B->Sphere->SetCollisionEnabled(GHangS.Coll);
			if (bSaved)
			{
				const FVector At = FVector(GHangS.Ledge.X, GHangS.Ledge.Y, GHangS.FloorZ) - GHangS.Out * 130.f + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 4.f);
				B->Sphere->SetWorldLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
				B->Sphere->SetSimulatePhysics(true);
				B->Sphere->SetPhysicsLinearVelocity(-GHangS.Out * 140.f);
				B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				GHangS.Safe.Add(B, At);
			}
			else
			{
				const FVector At = (P ? P->GetActorLocation() : GHangS.Ledge) + GHangS.Out * 40.f + FVector(0.f, 0.f, 80.f);
				B->Sphere->SetWorldLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
				B->Sphere->SetSimulatePhysics(true);
				B->Sphere->SetPhysicsLinearVelocity(GHangS.Out * 250.f + FVector(0.f, 0.f, -800.f));
				GHangS.Dropped.Add(B);
			}
		}
		if (P) { if (bSaved) P->Dismiss(true); else P->Destroy(); }
		ReleaseCam(W);
		GHangS.V.bOn = false; GHangS.V.Result = bSaved ? 1 : 2; GHangS.V.ResultAge = 0.f;
		if (G && B)
		{
			if (bSaved) G->PublicFeed(FString::Printf(TEXT("P%d %s  pulled back up!%s"), B->PlayerIndex + 1, *B->Def.Name, GHangS.V.bFree ? TEXT("") : *FString::Printf(TEXT("  -%d SP"), ZoogiEdgeHang::Cost)), ZoogiUtil::PlayerColor(B->PlayerIndex));
			else G->PublicFeed(FString::Printf(TEXT("P%d %s  lost its grip"), B->PlayerIndex + 1, *B->Def.Name), FLinearColor(1.f, 0.4f, 0.35f));
		}
		UE_LOG(LogZoogi, Log, TEXT("HANG P%d %s (bar %.2f after %.1f s)"), GHangS.V.Player + 1, bSaved ? TEXT("SAVED - back on the court") : TEXT("FELL - lost its grip"), GHangS.V.Prog, GHangS.Age);
		GHangS.Ball.Reset(); GHangS.Pup.Reset();
	}

	void Begin(AZoogiGameMode* G, AZoogiBall* B, const FVector& L, const FVector& Sf, bool bHuman)
	{
		UWorld* W = G->GetWorld();
		AZoogiArena* A = G->Arena;
		// the lip: binary search the court boundary between the last spot on the court and where the Zoogi is now
		FVector2D In(Sf.X, Sf.Y), Ou(L.X, L.Y);
		for (int32 k = 0; k < 14; ++k) { const FVector2D M = (In + Ou) * 0.5f; if (A && A->IsOffCourt(M)) Ou = M; else In = M; }
		FVector2D O2 = FVector2D(L.X, L.Y) - FVector2D(Sf.X, Sf.Y);
		if (O2.SizeSquared() < 1.f) O2 = In;
		O2.Normalize();
		GHangS.Out = FVector(O2.X, O2.Y, 0.f);
		GHangS.Ledge = FVector(In.X, In.Y, Sf.Z - ZoogiConst::ZoogiRadius);
		GHangS.FloorZ = GHangS.Ledge.Z;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiHang), false, B);
		{
			// the real lip height: a seat wall / rim on the boundary is what the hands hold
			FHitResult H;
			const FVector T0 = GHangS.Ledge - GHangS.Out * 8.f + FVector(0.f, 0.f, 300.f), T1 = GHangS.Ledge - GHangS.Out * 8.f - FVector(0.f, 0.f, 100.f);
			if (W->LineTraceSingleByChannel(H, T0, T1, ECC_Visibility, QP) && H.ImpactPoint.Z > GHangS.Ledge.Z - 30.f && H.ImpactPoint.Z < GHangS.Ledge.Z + 220.f) GHangS.Ledge.Z = H.ImpactPoint.Z;
		}
		GHangS.Pin = GHangS.Ledge + GHangS.Out * 6.f + FVector(0.f, 0.f, 3.f);
		GHangS.Ball = B;
		GHangS.Coll = B->Sphere ? B->Sphere->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics;
		if (B->Sphere)
		{
			B->Sphere->SetSimulatePhysics(false);
			B->Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			B->Sphere->SetWorldLocation(GHangS.Ledge + GHangS.Out * 40.f - FVector(0.f, 0.f, 140.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(W, B, GHangS.Ledge - FVector(0.f, 0.f, 170.f), (-GHangS.Out).Rotation().Yaw, EZoogiPuppetPose::Hang);
		if (P) { P->bPinHands = true; P->HandPin = GHangS.Pin; P->Pull = 0.1f; }
		GHangS.Pup = P;
		// camera: on the court, a little to the side, looking out and down at the Aurex's face on the lip
		// first clear view of the face on the lip: out over the drop looking back at it, then high on the court, then low on the court
		const FVector Side = FVector::CrossProduct(FVector::UpVector, GHangS.Out);
		const FVector Seen = GHangS.Pin + FVector(0.f, 0.f, 15.f); // the hands on the lip must be in clear view
		const FVector Body = GHangS.Pin + GHangS.Out * 20.f - FVector(0.f, 0.f, 80.f);
		const FVector Cands[6] = { GHangS.Ledge + GHangS.Out * 360.f + Side * 150.f + FVector(0.f, 0.f, 60.f),   // out over the drop, facing it
			GHangS.Ledge + GHangS.Out * 170.f + Side * 380.f + FVector(0.f, 0.f, 90.f),                           // outside, along the cliff (profile)
			GHangS.Ledge + GHangS.Out * 170.f - Side * 380.f + FVector(0.f, 0.f, 90.f),
			GHangS.Ledge - GHangS.Out * 70.f + Side * 80.f + FVector(0.f, 0.f, 250.f),                             // on the court, close, looking over the lip
			GHangS.Ledge - GHangS.Out * 120.f + Side * 90.f + FVector(0.f, 0.f, 380.f),
			GHangS.Ledge - GHangS.Out * 250.f + Side * 120.f + FVector(0.f, 0.f, 400.f) };
		if (P) QP.AddIgnoredActor(P);
		FVector CamAt = Cands[3];
		for (const FVector& Cc : Cands)
			if (!W->LineTraceTestByChannel(Cc, Seen, ECC_Visibility, QP) && !W->OverlapAnyTestByChannel(Cc, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(30.f), QP)) { CamAt = Cc; break; }
		const FVector Look = FVector::DotProduct(CamAt - GHangS.Ledge, GHangS.Out) > 0.f ? Body : (Body + Seen) * 0.5f;
		if (UZoogiEpisodeCameraDirector* CD = UZoogiEpisodeCameraDirector::Get(G)) CD->StopAllShots(true); // the edge-fall shot must not hold the view
		if (!HangVisualOnly()) // b67: no ledge zoom - the game camera stays
		{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACameraActor* C = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Look - CamAt).Rotation(), CamAt), SP))
		{
			if (C->GetCameraComponent()) { C->GetCameraComponent()->SetFieldOfView(62.f); C->GetCameraComponent()->SetConstraintAspectRatio(false); }
			GHangS.Cam = C;
			if (APlayerController* PC = W->GetFirstPlayerController()) { GHangS.PrevView = PC->GetViewTarget(); PC->SetViewTargetWithBlend(C, 0.f); }
		}
		}
		GHangS.V = ZoogiEdgeHang::FView();
		GHangS.V.bOn = true; GHangS.V.bVisual = HangVisualOnly(); GHangS.V.Player = B->PlayerIndex; GHangS.V.bHuman = bHuman && !GHangS.V.bVisual; GHangS.V.bFree = !GHangS.bCards; GHangS.V.bNextTurn = GHangS.bCards && GHangS.bDebt; GHangS.V.Name = B->Def.Name;
		GHangS.Imp = 0.f; GHangS.AutoAcc = 0.f; GHangS.Age = 0.f;
		// CPU / test grabs auto-tap: test step 1 makes it, step 2 does not
		GHangS.AutoRate = bHuman ? 0.f : (GHangS.TestStep == 2 ? 1.6f : 4.6f);
		if (bHuman && GHangS.TestStep > 0) GHangS.AutoRate = GHangS.TestStep == 2 ? 1.6f : 4.6f;
		if (AZoogiFX* X = AZoogiFX::Spawn(W, GHangS.Pin, ZoogiUtil::PlayerColor(B->PlayerIndex), 110.f, 0.3f, false, 8.f)) X->CustomTimeDilation = 1.f / ZoogiEdgeHang::Dilation;
		ZoogiUtil::PlaySfx(G, TEXT("Grab"), 0.9f); // b59: realistic grab thud
		UE_LOG(LogZoogi, Log, TEXT("HANG P%d %s grabbed the edge at (%.0f, %.0f, %.0f) - tap to pull up (%s)"), B->PlayerIndex + 1, *B->Def.Name, GHangS.Ledge.X, GHangS.Ledge.Y, GHangS.Ledge.Z, GHangS.bCards ? TEXT("2 SP") : TEXT("free, cards off"));
	}
}

namespace ZoogiEdgeHang
{
	bool Active() { return GHangS.V.bOn; }
	void Force(AZoogiBall* B, float Seconds) { if (B) GForce.Add(B, FPlatformTime::Seconds() + Seconds); }
	bool Forced(const AZoogiBall* B) { return ForcedB(B); }
	const FView& View() { return GHangS.V; }
	bool WantsTaps() { return GHangS.V.bOn && GHangS.V.bHuman && !GHangS.V.bVisual && GHangS.V.Phase <= 1; }
	bool Visual() { return HangVisualOnly(); }

	bool Track(AZoogiGameMode* G, AZoogiBall* B, const FVector& L)
	{
		if (!G || !B) return false;
		if (GHangS.World.Get() != G->GetWorld())
		{
			GHangS.Safe.Reset(); GHangS.Dropped.Reset(); GHangS.V = FView(); GHangS.Ball.Reset(); GHangS.Pup.Reset(); GHangS.Cam.Reset();
			GHangS.World = G->GetWorld();
		}
		if (GHangS.V.bOn && GHangS.Ball.Get() == B) return true;
		static const bool bOff = Flag(TEXT("ZoogiNoEdgeHang"));
		if (bOff) return false;
		AZoogiArena* A = G->Arena;
		if (!A) return false;
		const FVector2D P(L.X, L.Y);
		if (!A->IsOffCourt(P) && L.Z > -200.f)
		{
			if (FMath::Abs(B->GetBallVelocity().Z) < 150.f) GHangS.Safe.Add(B, L);
			GHangS.Dropped.Remove(B);
			return false;
		}
		if (GHangS.V.bOn || GHangS.Dropped.Contains(B) || G->Phase != EZoogiPhase::Rolling) return false;
		const FVector* Sf = GHangS.Safe.Find(B);
		if (!Sf) return false;
		static const bool bAll = Flag(TEXT("ZoogiHangAll")) || Flag(TEXT("ZoogiHangTest"));
		const bool bHuman = !B->bCPU;
		if (!bHuman && !bAll && !HangVisualOnly()) return false; // b67: the visual-only hang plays for CPU Zoogis too (it is short)
		const FVector V = B->GetBallVelocity();
		if (L.Z > Sf->Z - 70.f || V.Z > 0.f)                                                // still level with the lip
		{
			static double LastLog = 0.0;
			if (G->GetWorld()->GetRealTimeSeconds() - LastLog > 0.5) { LastLog = G->GetWorld()->GetRealTimeSeconds(); UE_LOG(LogZoogi, Log, TEXT("HANG P%d off the court at z %.0f (lip %.0f, vz %.0f) - not falling yet"), B->PlayerIndex + 1, L.Z, Sf->Z, V.Z); }
			return false;
		}
		if (FVector2D::Distance(P, FVector2D(Sf->X, Sf->Y)) > 700.f) { GHangS.Dropped.Add(B); return false; } // blasted far out: nothing to grab
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (F->ClashOn() || F->IsReplaying() || F->IsFlung(B)) return false;
		UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
		GHangS.bDebt = false;
		const bool bForceB = ForcedB(B) && bHuman; // b147a
		if (bForceB) GForceNow = true;
		GHangS.bCards = G->CardsOn() && CS && !HangVisualOnly() && !bForceB; // b67: no SP - it always falls; b147a: the boss-brace hang is free
		if (GHangS.bCards)
		{
			const FZoogiCardPlayerState* PS = CS->Rules().Player(B->PlayerIndex);
			GHangS.bDebt = B->PlayerIndex != G->ActivePlayer; // off-turn: Card Energy is 0 between turns, the save is paid from the next turn
			if (!PS || (!GHangS.bDebt && PS->Energy < Cost))
			{
				GHangS.Dropped.Add(B);
				G->PublicFeed(FString::Printf(TEXT("P%d %s  no SP to hang on (needs %d)"), B->PlayerIndex + 1, *B->Def.Name, Cost), FLinearColor(1.f, 0.55f, 0.4f));
				UE_LOG(LogZoogi, Log, TEXT("HANG P%d could not grab the edge: %d SP (needs %d)"), B->PlayerIndex + 1, PS ? PS->Energy : -1, Cost);
				return false;
			}
		}
		Begin(G, B, L, *Sf, bHuman);
		return true;
	}

	bool Tap(AZoogiGameMode* G)
	{
		if (!GHangS.V.bOn || !G) return false;
		if (GHangS.V.Phase == 1 && GHangS.V.bHuman && GHangS.AutoRate <= 0.f) TapNow(G);
		return true;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		UWorld* W = G->GetWorld();
		// -ZoogiHangTest: roll a Zoogi off the edge mid-shot (step 1 makes it back up, step 2 lets go)
		{
			static const bool bTest = Flag(TEXT("ZoogiHangTest"));
			if (bTest && !GHangS.V.bOn && GHangS.TestStep < 2 && W->GetRealTimeSeconds() > GHangS.TestNext && G->Phase == EZoogiPhase::Rolling && G->Arena)
			{
				for (AZoogiBall* B : G->Balls)
				{
					if (!B || B->bOut || !B->Sphere || !B->Sphere->IsSimulatingPhysics() || B == G->GetActiveBall() || B->GetBallVelocity().Size() > 60.f) continue; // a resting non-shooter (the launch would override it)
					// drop it just past the lip (open edge or not - the grab is what is being shown)
					const float Z0 = B->BallLocation().Z;
					FVector2D D(B->BallLocation().X, B->BallLocation().Y);
					if (D.SizeSquared() < 1.f) D = FVector2D(1.f, 0.f);
					D.Normalize();
					float R = 300.f; while (R < 6000.f && !G->Arena->IsOffCourt(D * (R + 10.f))) R += 20.f; // walk out to the lip
					GHangS.Safe.Add(B, FVector(D.X * (R - 80.f), D.Y * (R - 80.f), Z0));
					B->Sphere->SetWorldLocation(FVector(D.X * (R + 60.f), D.Y * (R + 60.f), Z0 - 110.f), false, nullptr, ETeleportType::TeleportPhysics);
					B->Sphere->SetPhysicsLinearVelocity(FVector(D.X * 100.f, D.Y * 100.f, -300.f));
					++GHangS.TestStep; GHangS.TestNext = W->GetRealTimeSeconds() + 22.f;
					UE_LOG(LogZoogi, Log, TEXT("HANG test %d: P%d rolled toward the edge (r %.0f)"), GHangS.TestStep, B->PlayerIndex + 1, R);
					break;
				}
			}
		}
		if (!GHangS.V.bOn) { if (GHangS.V.ResultAge >= 0.f) GHangS.V.ResultAge += RealDt; return; }
		AZoogiBall* B = GHangS.Ball.Get();
		AZoogiAurexPuppet* P = GHangS.Pup.Get();
		if (!B || !P) { Finish(G, false); return; }
		GHangS.Age += RealDt; GHangS.V.PhaseAge += RealDt;
		GHangS.V.At = P->GetActorLocation(); // b175: the camera drifts over to the hanging Zoogi
		if (ACameraActor* C = GHangS.Cam.Get()) if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetViewTarget() != C) PC->SetViewTarget(C); // hold the ledge camera
		GHangS.Imp = FMath::Max(0.f, GHangS.Imp - RealDt * 4.f);
		switch (GHangS.V.Phase)
		{
		case 0: // grab: the Aurex pops out on the lip
			P->Pull = 0.1f;
			GHangS.V.Left = Window;
			if (GHangS.V.PhaseAge > 0.35f) Shot(G, GHangS.TestStep == 2 ? TEXT("hang2_grab") : TEXT("hang_grab"));
			if (GHangS.V.PhaseAge > 0.5f) { GHangS.V.Phase = 1; GHangS.V.PhaseAge = 0.f; }
			break;
		case 1: // struggle: tap to fill the bar
		{
			if (GHangS.V.bVisual)
			{	// b67: a short struggle, then it loses its grip (always - a knockout)
				P->Pull = FMath::Clamp(0.25f + 0.2f * FMath::Sin(GHangS.V.PhaseAge * 11.f), 0.f, 1.f);
				GHangS.V.Left = FMath::Max(0.f, 0.9f - GHangS.V.PhaseAge);
				if (GHangS.V.PhaseAge > 0.9f)
				{
					GHangS.V.Phase = 3; GHangS.V.PhaseAge = 0.f;
					P->Pose = EZoogiPuppetPose::Drop; P->bPinHands = false;
					GHangS.DropV = GHangS.Out * 120.f + FVector(0.f, 0.f, 160.f);
				}
				break;
			}
			GHangS.V.Prog = FMath::Max(0.f, GHangS.V.Prog - Slip * RealDt);
			if (GHangS.AutoRate > 0.f) { GHangS.AutoAcc += RealDt * GHangS.AutoRate; while (GHangS.AutoAcc >= 1.f) { GHangS.AutoAcc -= 1.f; TapNow(G); } }
			GHangS.V.Left = FMath::Max(0.f, Window - GHangS.V.PhaseAge);
			P->Pull = FMath::Clamp(0.15f + GHangS.V.Prog * 0.75f + GHangS.Imp * 0.15f, 0.f, 1.f);
			if (GHangS.V.PhaseAge > 1.6f) Shot(G, GHangS.TestStep == 2 ? TEXT("hang2_struggle") : TEXT("hang_struggle"));
			if (GHangS.V.Prog >= 1.f)
			{
				if (GHangS.bCards) if (UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G)) { if (GHangS.bDebt || !CS->SpendEnergy(B->PlayerIndex, Cost)) CS->AddEnergyDebt(B->PlayerIndex, Cost); }
				GHangS.V.Phase = 2; GHangS.V.PhaseAge = 0.f;
				P->Pose = EZoogiPuppetPose::Climb;
				if (AZoogiFX* X = AZoogiFX::Spawn(W, GHangS.Pin, ZoogiUtil::PlayerColor(GHangS.V.Player), 120.f, 0.3f, false, 8.f)) X->CustomTimeDilation = 1.f / ZoogiEdgeHang::Dilation;
				UE_LOG(LogZoogi, Log, TEXT("HANG P%d made it - climbing back up%s"), GHangS.V.Player + 1, GHangS.bCards ? TEXT(" (-2 SP)") : TEXT(""));
			}
			else if (GHangS.V.Left <= 0.f)
			{
				GHangS.V.Phase = 3; GHangS.V.PhaseAge = 0.f;
				P->Pose = EZoogiPuppetPose::Drop; P->bPinHands = false;
				GHangS.DropV = GHangS.Out * 120.f + FVector(0.f, 0.f, 160.f);
			}
			break;
		}
		case 2: // climb: hands planted, then up onto the court
		{
			const float K = GHangS.V.PhaseAge / 0.8f;
			if (K > 0.5f && P->bPinHands)
			{
				P->bPinHands = false; P->Pose = EZoogiPuppetPose::Cheer; GHangS.ClimbFrom = P->GetActorLocation();
			}
			if (!P->bPinHands)
			{
				const float E = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp((K - 0.5f) / 0.5f, 0.f, 1.f), 2.f);
				P->SetActorLocation(FMath::Lerp(GHangS.ClimbFrom, FVector(GHangS.Ledge.X, GHangS.Ledge.Y, GHangS.FloorZ) - GHangS.Out * 110.f, E) + FVector(0.f, 0.f, FMath::Sin(E * PI) * 35.f));
			}
			if (K > 0.45f) Shot(G, TEXT("hang_climb"));
			if (K >= 1.f) { GHangS.V.Phase = 4; GHangS.V.PhaseAge = 0.f; }
			break;
		}
		case 3: // let go
			GHangS.DropV.Z -= 1900.f * RealDt;
			P->SetActorLocation(P->GetActorLocation() + GHangS.DropV * RealDt);
			if (ACameraActor* C = GHangS.Cam.Get()) C->SetActorRotation((P->GetActorLocation() + FVector(0.f, 0.f, 90.f) - C->GetActorLocation()).Rotation());
			if (GHangS.V.PhaseAge > 0.3f) Shot(G, TEXT("hang2_drop"));
			if (GHangS.V.PhaseAge > 0.85f) Finish(G, false);
			break;
		default: // cheer on the court, then back into the ball
			if (GHangS.V.PhaseAge > 0.15f) Shot(G, TEXT("hang_cheer"));
			if (GHangS.V.PhaseAge > 0.55f) Finish(G, true);
			break;
		}
	}
}
