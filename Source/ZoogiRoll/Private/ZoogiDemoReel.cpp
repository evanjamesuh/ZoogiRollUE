// b213 (Evan Nov 9: "Video demo of startup roll and tire smoke and 3d model animation fight fly smash in to rock sequences"):
// a scripted previs, recorded by the film recorder. Only with -ZoogiDemoReel (and -ZoogiFilm to record); nothing else runs it.
//  1 STARTUP ROLL  - the Zoogi spins up on the spot (burnout): tire smoke, skid marks, a wobble, then it launches
//  2 FIGHT         - it pops into its 3D Baller model, face to face with Vex's model: three lunge-and-hit exchanges
//  3 FLY           - Blue leaps, spins up into the air, hangs, and dives in with a flying punch
//  4 ROCK SMASH    - Vex is blasted across the court into a rock: the blast bursts OUT OF THE ROCK FACE, Vex bounces off, KO
// The Tripo models are static (no skeleton), so the motion is whole-body (lunge / tilt / spin / fly) - no limb animation.
#include "ZoogiDemoReel.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFilm.h"
#include "ZoogiSkills.h" // b221
#include "ZoogiEarthEvents.h" // b215 blast options
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"

namespace ZoogiDemoReel
{
	namespace
	{
		struct FReel
		{
			TWeakObjectPtr<AZoogiGameMode> G; bool bStarted = false, bDone = false; double T0 = 0.0, Last = 0.0;
			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AStaticMeshActor> Ball, MA, MB, Rock;
			FVector A0 = FVector::ZeroVector, Dir = FVector(1, 0, 0), Side = FVector(0, 1, 0), BStop = FVector::ZeroVector, VexAt = FVector::ZeroVector, RockAt = FVector::ZeroVector, Hit = FVector::ZeroVector;
			float FloorZ = 0.f, Spin = 0.f, HA = 1.f, HB = 1.f, MinA = 0.f, MinB = 0.f, Shake = 0.f;
			double NextPuff = 0.0, NextBig = 0.0, NextMark = 0.0, NextTrail = 0.0, NextSnd = 0.0; int32 Ev = 0;
		};
		FReel D;
		const float ModelH = 250.f;

		AStaticMeshActor* NewMeshActor(UWorld* W, UStaticMesh* M, const FVector& At)
		{
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(At), Sp);
			if (!A) return nullptr;
			A->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(M);
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			return A;
		}
		AStaticMeshActor* Model(UWorld* W, const TCHAR* Name, float& OutScale, float& OutMinZ)
		{
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ballers/%s/SM_%s.SM_%s"), Name, Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!M) { UE_LOG(LogZoogi, Warning, TEXT("DEMO no model %s"), Name); return nullptr; }
			const FBox Bb = M->GetBoundingBox();
			OutScale = ModelH / FMath::Max(1.f, (float)Bb.GetSize().Z); OutMinZ = Bb.Min.Z;
			AStaticMeshActor* A = NewMeshActor(W, M, FVector(0, 0, -5000.f));
			if (A) { A->GetStaticMeshComponent()->SetWorldScale3D(FVector(OutScale)); A->SetActorHiddenInGame(true); }
			return A;
		}
		// the Tripo models face their local +Y: turn to face Dir
		FRotator Face(const FVector& Dir, float Pitch = 0.f, float Roll = 0.f) { return FRotator(Pitch, Dir.Rotation().Yaw - 90.f, Roll); }
		void PlaceModel(AStaticMeshActor* A, const FVector& Feet, const FRotator& Rot, float Scale, float MinZ, float SquashZ = 1.f)
		{
			if (!A) return;
			A->SetActorHiddenInGame(false);
			A->GetStaticMeshComponent()->SetWorldScale3D(FVector(Scale, Scale, Scale * SquashZ));
			A->SetActorLocationAndRotation(Feet - Rot.RotateVector(FVector(0, 0, MinZ * Scale * SquashZ)), Rot);
		}
		float Ease(float x) { x = FMath::Clamp(x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
		float Seg(float t, float a, float b) { return FMath::Clamp((t - a) / FMath::Max(0.001f, b - a), 0.f, 1.f); }
		void Smoke(UWorld* W, const FVector& Pos, float Rad, float Life) { AZoogiFX::SpawnTrail(W, Pos, FLinearColor(0.82f, 0.82f, 0.85f), Rad, Life, 0.6f); }
		void Look(ACameraActor* Cm, const FVector& Eye, const FVector& At, float ShakeAmt)
		{
			if (!Cm) return;
			const FVector J = ShakeAmt > 0.f ? FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * 14.f * ShakeAmt : FVector::ZeroVector;
			Cm->SetActorLocationAndRotation(Eye + J, (At - Eye).Rotation());
		}
		void Blast(UWorld* W, const TCHAR* Name, const FVector& Pos, float Scl, const FVector& Out = FVector::ZeroVector)
		{
			const FRotator Rot = Out.IsNearlyZero() ? FRotator::ZeroRotator : FRotationMatrix::MakeFromZ((Out.GetSafeNormal2D() + FVector(0, 0, 0.22f)).GetSafeNormal()).Rotator();
			AZoogiFX::Stylized(W, Name, Pos, Scl, Rot);
		}

		bool Setup(AZoogiGameMode* G)
		{
			UWorld* W = G->GetWorld();
			TArray<AZoogiBall*> In; for (AZoogiBall* B : G->Balls) if (B && !B->bOut) In.Add(B);
			if (In.Num() < 1 || !G->Arena) return false;
			AZoogiBall* B0 = In[0];
			const FVector Ctr = G->Arena->GetActorLocation();
			D.A0 = B0->BallLocation(); D.FloorZ = D.A0.Z - ZoogiConst::ZoogiRadius;
			D.Dir = FVector(Ctr.X - D.A0.X, Ctr.Y - D.A0.Y, 0.f).GetSafeNormal(); if (D.Dir.IsNearlyZero()) D.Dir = FVector(1, 0, 0);
			D.Side = FVector(-D.Dir.Y, D.Dir.X, 0.f);
			D.BStop = D.A0 + D.Dir * 650.f; D.VexAt = D.BStop + D.Dir * 330.f; D.RockAt = D.VexAt + D.Dir * 700.f;
			D.BStop.Z = D.VexAt.Z = D.RockAt.Z = D.FloorZ; D.A0.Z = D.FloorZ + ZoogiConst::ZoogiRadius;
			// the court is cleared for the film: balls and orbs hidden, frozen
			for (AZoogiBall* B : G->Balls) if (B) { if (B->Sphere) B->Sphere->SetSimulatePhysics(false); B->SetActorHiddenInGame(true); }
			for (AZoogiOrb* O : G->Orbs) if (O) { if (O->Sphere) O->Sphere->SetSimulatePhysics(false); O->SetActorHiddenInGame(true); }
			{	// the film set: everything standing near the path or between it and the side cameras is hidden (eggs, rocks, arches, the tower)
				const FVector S0 = D.A0 - D.Dir * 500.f, S1 = D.RockAt + D.Dir * 350.f;
				int32 NHid = 0;
				for (TActorIterator<AActor> It(W); It; ++It)
				{
					AActor* Ac = *It;
					if (!Ac || Ac == G->Arena || Ac->IsHidden() || Ac->IsA<AZoogiBall>() || Ac->IsA<AZoogiOrb>() || Ac->IsA<ACameraActor>() || Ac->IsA<APlayerController>() || Ac->FindComponentByClass<ULightComponent>()) continue;
					FVector Org, Ext; Ac->GetActorBounds(false, Org, Ext);
					if (Ext.IsNearlyZero() || FMath::Max(Ext.X, Ext.Y) > 450.f) continue;
					if (Org.Z + Ext.Z < D.FloorZ + 30.f || Org.Z - Ext.Z > D.FloorZ + 700.f) continue;
					const FVector Cp = FMath::ClosestPointOnSegment(FVector(Org.X, Org.Y, 0.f), FVector(S0.X, S0.Y, 0.f), FVector(S1.X, S1.Y, 0.f));
					if (FVector::Dist2D(Cp, Org) > 1250.f) continue;
					Ac->SetActorHiddenInGame(true); ++NHid;
				}
				UE_LOG(LogZoogi, Log, TEXT("DEMO set: %d props hidden along the path"), NHid);
			}
			// a stand-in for the rolling Zoogi (its own ball look)
			if (B0->Visual && B0->Visual->GetStaticMesh())
			{
				if (AStaticMeshActor* Sa = NewMeshActor(W, B0->Visual->GetStaticMesh(), D.A0))
				{
					Sa->GetStaticMeshComponent()->SetWorldScale3D(B0->Visual->GetComponentScale());
					for (int32 m = 0; m < B0->Visual->GetNumMaterials(); ++m) Sa->GetStaticMeshComponent()->SetMaterial(m, B0->Visual->GetMaterial(m));
					D.Ball = Sa;
				}
			}
			D.MA = Model(W, TEXT("Blue"), D.HA, D.MinA);
			D.MB = Model(W, TEXT("Vex"), D.HB, D.MinB);
			// the rock Vex is smashed into
			const int32 Rn = 3;
			if (UStaticMesh* Rm = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/EarthbenderV1/Models/SM_Free_Rock_Model_%d.SM_Free_Rock_Model_%d"), Rn, Rn), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				const FBox Bb = Rm->GetBoundingBox(); const float K = 340.f / FMath::Max(1.f, (float)Bb.GetSize().Z);
				if (AStaticMeshActor* Ra = NewMeshActor(W, Rm, D.RockAt))
				{
					Ra->GetStaticMeshComponent()->SetWorldScale3D(FVector(K * 1.25f, K * 1.25f, K));
					Ra->SetActorLocation(D.RockAt - FVector(Bb.GetCenter().X * K * 1.25f, Bb.GetCenter().Y * K * 1.25f, Bb.Min.Z * K + 8.f));
					if (UMaterialInstanceDynamic* Mt = ZoogiUtil::SurfaceMID(Ra, ZoogiUtil::Hex(TEXT("#8A8F7A")), 0.85f, ZoogiUtil::Hex(TEXT("#8A8F7A")), 0.4f))
						for (int32 m = 0; m < FMath::Max(1, Ra->GetStaticMeshComponent()->GetNumMaterials()); ++m) Ra->GetStaticMeshComponent()->SetMaterial(m, Mt);
					D.Rock = Ra;
				}
			}
			D.Hit = D.RockAt - D.Dir * 150.f + FVector(0, 0, 130.f);
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACameraActor* Ca = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(D.A0), Sp)) { FVector2D Vp(16.f, 9.f); if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Vp); Ca->GetCameraComponent()->SetFieldOfView(Vp.Y > Vp.X ? 50.f : 70.f); Ca->GetCameraComponent()->bConstrainAspectRatio = false; D.Cam = Ca; }
			if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->MyHUD) PC->MyHUD->bShowHUD = false;
			if (UGameViewportClient* Vc = W->GetGameViewport()) Vc->RemoveAllViewportWidgets(); // a clean film (-ZoogiFilmNoUI drops the rest)
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoBlasts")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoTurret"))) ZoogiFilm::Begin(TEXT("demo"), 19.5f);
			UE_LOG(LogZoogi, Log, TEXT("DEMO reel start: from (%.0f, %.0f) dir (%.2f, %.2f), rock at (%.0f, %.0f), models %s/%s"), D.A0.X, D.A0.Y, D.Dir.X, D.Dir.Y, D.RockAt.X, D.RockAt.Y, D.MA.IsValid() ? TEXT("ok") : TEXT("-"), D.MB.IsValid() ? TEXT("ok") : TEXT("-"));
			return true;
		}
		void Once(int32 Id, TFunctionRef<void()> Fn) { if (!(D.Ev & (1 << Id))) { D.Ev |= (1 << Id); Fn(); } }
	}

	namespace
	{
		// b215 (-ZoogiDemoReel -ZoogiDemoBlasts; Evan: "I wanted the blast effect on impact when colliding with earth bending rock, show me
		// options"): the court cleared, a rock, the Zoogi rolled into its face 8 times - one blast style each, a film clip each (NN_rock)
		struct FBl { bool bOn = false; int32 K = 0; double T0 = 0.0; FVector Start, HitAt, Back; bool bHit = false; };
		FBl GBl;
		void TickBlasts(AZoogiGameMode* G, UWorld* W, double Tn)
		{
			if (!GBl.bOn)
			{
				GBl.bOn = true; GBl.K = 0; GBl.T0 = Tn + 1.0;
				// the stand-in ball starts 650 cm out from the rock face
				const FVector Face = D.RockAt - D.Dir * 150.f;
				GBl.HitAt = FVector(Face.X, Face.Y, D.FloorZ + ZoogiConst::ZoogiRadius);
				GBl.Start = GBl.HitAt - D.Dir * 650.f; GBl.Back = GBl.HitAt - D.Dir * 380.f;
				if (AStaticMeshActor* Ma = D.MA.Get()) Ma->SetActorHiddenInGame(true);
				if (AStaticMeshActor* Mb = D.MB.Get()) Mb->SetActorHiddenInGame(true);
			}
			const FVector Eye = GBl.HitAt - D.Dir * 360.f + D.Side * 640.f + FVector(0, 0, 150.f);
			Look(D.Cam.Get(), Eye, GBl.HitAt + D.Dir * 30.f + FVector(0, 0, 40.f), D.Shake);
			D.Shake = FMath::Max(0.f, D.Shake - 0.03f);
			const float t = (float)(Tn - GBl.T0);
			if (t < 0.f) return;
			if (t > 3.4f) { GBl.T0 = Tn; GBl.bHit = false; if (++GBl.K >= 8) { D.bDone = true; UE_LOG(LogZoogi, Log, TEXT("DEMO blasts done")); return; } }
			const float u = (float)(Tn - GBl.T0);
			if (u < 0.05f && !GBl.bHit) { ZoogiFilm::Begin(TEXT("rock"), 3.0f); ZoogiFilm::Mark(FString::Printf(TEXT("option %d"), GBl.K + 1)); }
			FVector P; float Spin = 0.f;
			if (u < 0.35f) { P = GBl.Start; }
			else if (u < 0.70f) { const float k = (u - 0.35f) / 0.35f; P = FMath::Lerp(GBl.Start, GBl.HitAt, k); Spin = 1500.f * (u - 0.35f); }
			else
			{
				if (!GBl.bHit)
				{
					GBl.bHit = true; D.Shake = 1.f;
					ZoogiEarthEvents::BlastStyleAt(G, GBl.HitAt + FVector(0, 0, 10.f), -D.Dir, 1400.f, GBl.K + 1);
					UE_LOG(LogZoogi, Log, TEXT("DEMO blast option %d"), GBl.K + 1);
				}
				const float k = FMath::Clamp((u - 0.70f) / 0.9f, 0.f, 1.f);
				P = FMath::Lerp(GBl.HitAt, GBl.Back, 1.f - FMath::Pow(1.f - k, 2.f)) + FVector(0, 0, 90.f * FMath::Sin(k * PI) * (1.f - k));
				Spin = 525.f - 600.f * k;
			}
			if (AStaticMeshActor* B = D.Ball.Get()) { B->SetActorHiddenInGame(false); B->SetActorLocationAndRotation(P, FQuat(D.Side, FMath::DegreesToRadians(Spin))); }
		}
	}

	namespace
	{
		// b221 (-ZoogiDemoReel -ZoogiDemoTurret; Evan: "show me 6 top view options ... and confirm bullets shoot from the barrel"): the turret
		// on the cleared court seen from straight above, a Zoogi stand-in at 6 spots around it - it turns, fires 2 shots of one style per
		// spot, the target is knocked back when a shot lands. A film clip per option (NN_turret).
		struct FTu { bool bOn = false; int32 K = 0; double T0 = 0.0; TWeakObjectPtr<AActor> Tur; FVector At = FVector::ZeroVector; float TipF = 60.f, TipU = 110.f;
			float Yaw = 0.f; int32 Fired = 0; double HitAt[2] = { 1e9, 1e9 }; FVector TgtAt = FVector::ZeroVector, Knock = FVector::ZeroVector; double KnockT = -10.0; double LastT = 0.0; };
		FTu GTu;
		void TickTurret(AZoogiGameMode* G, UWorld* W, double Tn)
		{
			if (!GTu.bOn)
			{
				GTu.bOn = true; GTu.K = 0; GTu.T0 = Tn + 1.0; GTu.LastT = Tn;
				GTu.At = D.A0 + D.Dir * 750.f; GTu.At.Z = D.FloorZ;
				GTu.Tur = ZoogiSkills::TurretDemoSpawn(G, GTu.At, GTu.TipF, GTu.TipU);
				GTu.Yaw = D.Dir.Rotation().Yaw;
				if (AStaticMeshActor* Ma = D.MA.Get()) Ma->SetActorHiddenInGame(true);
				if (AStaticMeshActor* Mb = D.MB.Get()) Mb->SetActorHiddenInGame(true);
				if (AStaticMeshActor* Ro = D.Rock.Get()) Ro->SetActorHiddenInGame(true);
				UE_LOG(LogZoogi, Log, TEXT("DEMO turret at (%.0f, %.0f) tip fwd %.0f up %.0f, yaw offset %.0f"), GTu.At.X, GTu.At.Y, GTu.TipF, GTu.TipU, ZoogiSkills::TurretYawOffset());
			}
			const float Dt = FMath::Clamp((float)(Tn - GTu.LastT), 0.f, 0.1f); GTu.LastT = Tn;
			ZoogiSkills::TurretShotsTick(G, Dt);
			if (ACameraActor* Cm = D.Cam.Get())
			{	// straight down, the court's forward at the top of the screen
				static float CamH = -1.f; if (CamH < 0.f) { CamH = 1250.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiDemoTurretCamH="), CamH); }
				Cm->SetActorLocationAndRotation(GTu.At + FVector(0, 0, CamH), FRotator(-89.9f, D.Dir.Rotation().Yaw, 0.f));
			}
			const float t = (float)(Tn - GTu.T0);
			if (t < 0.f) return;
			if (t > 3.6f) { GTu.T0 = Tn; GTu.Fired = 0; GTu.HitAt[0] = GTu.HitAt[1] = 1e9; if (++GTu.K >= 6) { D.bDone = true; UE_LOG(LogZoogi, Log, TEXT("DEMO turret done")); return; } }
			const float u = (float)(Tn - GTu.T0);
			const float Ang = D.Dir.Rotation().Yaw + 35.f + 62.f * GTu.K; // a new spot for every option, so the turret turns each time
			GTu.TgtAt = GTu.At + FRotator(0.f, Ang, 0.f).Vector() * 640.f; GTu.TgtAt.Z = D.FloorZ + ZoogiConst::ZoogiRadius;
			if (u < 0.05f && GTu.Fired == 0) { ZoogiFilm::Begin(TEXT("turret"), 3.4f); ZoogiFilm::Mark(FString::Printf(TEXT("option %d"), GTu.K + 1)); }
			// turn the turret toward the target (~360 deg/s), its barrel = actor yaw - the model offset
			const float Want = Ang;
			GTu.Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(GTu.Yaw, Want), -360.f * Dt, 360.f * Dt);
			if (AActor* Tu = GTu.Tur.Get()) Tu->SetActorRotation(FRotator(0.f, GTu.Yaw + ZoogiSkills::TurretYawOffset(), 0.f));
			const float FireT[2] = { 0.8f, 1.9f };
			for (int32 s = 0; s < 2; ++s)
				if (GTu.Fired == s && u >= FireT[s] && FMath::Abs(FMath::FindDeltaAngleDegrees(GTu.Yaw, Want)) < 3.f)
				{
					const float Land = ZoogiSkills::TurretShoot(G, GTu.Tur.Get(), GTu.At, GTu.TipF, GTu.TipU, GTu.TgtAt, GTu.K + 1);
					GTu.HitAt[s] = Tn + Land; ++GTu.Fired;
					UE_LOG(LogZoogi, Log, TEXT("DEMO turret option %d shot %d (lands in %.2f s)"), GTu.K + 1, s + 1, Land);
				}
			for (int32 s = 0; s < 2; ++s) if (Tn >= GTu.HitAt[s]) { GTu.HitAt[s] = 1e9; GTu.KnockT = Tn; GTu.Knock = FRotator(0.f, Ang, 0.f).Vector(); }
			// the target: knocked back along the shot, then it rolls back to its spot
			const float Kt = (float)(Tn - GTu.KnockT);
			const float Kb = Kt >= 0.f && Kt < 0.9f ? FMath::Sin(FMath::Clamp(Kt / 0.9f, 0.f, 1.f) * PI) * 120.f : 0.f;
			if (AStaticMeshActor* B = D.Ball.Get()) { B->SetActorHiddenInGame(false); B->SetActorLocation(GTu.TgtAt + GTu.Knock * Kb + FVector(0, 0, Kb * 0.3f)); }
		}
	}

	bool Holding() { static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoReel")); return bOn && !D.bDone; }

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoReel"));
		if (!bOn || !G || !G->GetWorld() || D.bDone) return;
		UWorld* W = G->GetWorld();
		const double Tn = FPlatformTime::Seconds();
		if (!D.bStarted)
		{
			if (G->Phase != EZoogiPhase::Aiming || G->PhaseTime < 1.2f) return;
			D = FReel(); D.G = G;
			if (!Setup(G)) return;
			D.bStarted = true; D.T0 = Tn; D.Last = Tn;
		}
		static const bool bBlasts = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoBlasts"));
		static const bool bTurretRig = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoTurret")); // b221
		if (bTurretRig) { if (APlayerController* Pc = W->GetFirstPlayerController()) if (D.Cam.IsValid() && Pc->GetViewTarget() != D.Cam.Get()) Pc->SetViewTarget(D.Cam.Get()); TickTurret(G, W, Tn); return; }
		if (bBlasts) { if (APlayerController* Pc = W->GetFirstPlayerController()) if (D.Cam.IsValid() && Pc->GetViewTarget() != D.Cam.Get()) Pc->SetViewTarget(D.Cam.Get()); TickBlasts(G, W, Tn); return; }
		const float Dt = FMath::Clamp((float)(Tn - D.Last), 0.f, 0.1f); D.Last = Tn;
		const float t = (float)(Tn - D.T0);
		ACameraActor* Cm = D.Cam.Get();
		if (APlayerController* PC = W->GetFirstPlayerController()) if (Cm && PC->GetViewTarget() != Cm) PC->SetViewTarget(Cm);
		D.Shake = FMath::Max(0.f, D.Shake - Dt * 2.2f);
		AStaticMeshActor* Bl = D.Ball.Get(); AStaticMeshActor* MA = D.MA.Get(); AStaticMeshActor* MB = D.MB.Get();
		const FVector Mid = (D.BStop + D.VexAt) * 0.5f;

		// ---- 1 STARTUP ROLL (0 - 4.0): burnout on the spot, then the launch
		if (t < 4.0f)
		{
			const float Rate = t < 3.1f ? FMath::Lerp(200.f, 1700.f, Ease(t / 2.4f)) : 1700.f;
			D.Spin += Rate * Dt;
			FVector Pos = D.A0;
			if (t < 3.1f)
			{
				Pos += FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 3.5f * Ease(t / 2.f); // the wobble
				if (t > 2.5f) Pos -= D.Dir * 18.f * Ease(Seg(t, 2.5f, 3.1f)); // squats back, about to go
				if (Tn >= D.NextPuff)
				{	// tire smoke pouring out behind the spinning Zoogi
					D.NextPuff = Tn + 0.045;
					const FVector Rear = D.A0 - D.Dir * 60.f + D.Side * FMath::FRandRange(-35.f, 35.f) + FVector(0, 0, -30.f);
					Smoke(W, Rear - D.Dir * FMath::FRandRange(0.f, 60.f) + FVector(0, 0, FMath::FRandRange(0.f, 40.f)), FMath::FRandRange(55.f, 120.f) * (0.6f + 0.5f * Ease(t / 2.f)), 1.6f);
				}
				if (Tn >= D.NextBig && t > 0.6f) { D.NextBig = Tn + 0.12; Smoke(W, D.A0 - D.Dir * FMath::FRandRange(90.f, 200.f) + D.Side * FMath::FRandRange(-60.f, 60.f) + FVector(0, 0, FMath::FRandRange(-20.f, 50.f)), FMath::FRandRange(90.f, 160.f), 2.0f); }
				if (Tn >= D.NextSnd) { D.NextSnd = Tn + 0.32; ZoogiUtil::PlaySfx(G, TEXT("Clack"), 0.35f, 0.6f + 0.6f * Ease(t / 3.f)); } // b214: the tyres screech / grind
				Once(20, [&]() { ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.8f, 1.1f); ZoogiUtil::PlaySfx(G, TEXT("EpicRiser"), 0.6f, 1.f); });
				if (Tn >= D.NextMark) { D.NextMark = Tn + 0.25; AZoogiFX::SpawnMark(W, FVector(D.A0.X, D.A0.Y, D.FloorZ + 1.f) - D.Dir * FMath::FRandRange(0.f, 40.f), FLinearColor(0.05f, 0.05f, 0.06f), 45.f, 6.f); }
			}
			else
			{	// LAUNCH
				Once(0, [&]() { ZoogiUtil::PlaySfx(G, TEXT("Dash"), 1.f, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("EpicImpact"), 0.7f, 1.2f); for (int32 q = 0; q < 10; ++q) Smoke(W, D.A0 - D.Dir * FMath::FRandRange(40.f, 160.f) + D.Side * FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(110.f, 190.f), 2.2f); AZoogiFX::Spawn(W, FVector(D.A0.X, D.A0.Y, D.FloorZ + 4.f), FLinearColor(1.f, 0.9f, 0.6f), 260.f, 0.45f, true, 8.f); D.Shake = 0.8f; ZoogiFilm::Mark(TEXT("launch")); });
				const float k = Ease(Seg(t, 3.1f, 3.9f));
				Pos = FMath::Lerp(D.A0, FVector(D.BStop.X, D.BStop.Y, D.A0.Z), 1.f - FMath::Pow(1.f - k, 2.f));
				if (Tn >= D.NextTrail) { D.NextTrail = Tn + 0.03; AZoogiFX::SpawnTrail(W, Pos - D.Dir * 40.f - FVector(0, 0, 35.f), FLinearColor(0.8f, 0.8f, 0.85f), 70.f, 0.9f, 1.f); }
			}
			if (Bl) Bl->SetActorLocationAndRotation(Pos, FQuat(D.Side, FMath::DegreesToRadians(D.Spin)));
			// camera: low, behind and to the side; it whips round to track the launch
			if (t < 3.1f) Look(Cm, D.A0 + D.Dir * (330.f - 50.f * Ease(t / 3.f)) + D.Side * 300.f + FVector(0, 0, 60.f), D.A0 - D.Dir * 60.f + FVector(0, 0, 10.f), D.Shake);
			else Look(Cm, FMath::Lerp(D.A0 + D.Dir * 280.f + D.Side * 300.f, Mid - D.Dir * 200.f + D.Side * 520.f, Ease(Seg(t, 3.1f, 4.f))) + FVector(0, 0, 90.f), Pos, D.Shake);
			return;
		}
		Once(1, [&]()
		{	// the ball pops into its 3D Baller model, and Vex appears
			if (Bl) Bl->SetActorHiddenInGame(true);
			Blast(W, TEXT("Mid_Explosion"), D.BStop + FVector(0, 0, 90.f), 0.25f);
			AZoogiFX::Spawn(W, D.BStop + FVector(0, 0, 100.f), FLinearColor(0.5f, 0.85f, 1.f), 90.f, 0.25f, false, 6.f);
			AZoogiFX::Spawn(W, D.VexAt + FVector(0, 0, 100.f), FLinearColor(1.f, 0.4f, 0.15f), 90.f, 0.25f, false, 6.f);
			ZoogiFilm::Mark(TEXT("fight")); ZoogiUtil::PlaySfx(G, TEXT("AurexIn"), 0.9f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.6f, 1.f);
		});
		// ---- 2 FIGHT (4.0 - 9.0): three lunge-and-hit exchanges (Blue, Vex, Blue)
		FVector PA = D.BStop, PB = D.VexAt; float TiltA = 0.f, TiltB = 0.f, SqA = 1.f, SqB = 1.f, YawA = 0.f, YawB = 0.f;
		const float Bob = 6.f * FMath::Sin(t * 7.f);
		PA.Z += FMath::Max(0.f, Bob); PB.Z += FMath::Max(0.f, -Bob);
		if (t < 9.0f)
		{
			struct FX3 { float At; bool bBlue; };
			const FX3 Ex[] = { { 4.7f, true }, { 5.9f, false }, { 7.1f, true }, { 8.0f, true } };
			for (int32 i = 0; i < 4; ++i)
			{
				const float a = Ex[i].At; const float u = Seg(t, a, a + 0.55f);
				if (u <= 0.f || u >= 1.f) continue;
				const float Lunge = u < 0.4f ? Ease(u / 0.4f) : 1.f - Ease((u - 0.4f) / 0.6f);
				if (Ex[i].bBlue) { PA += D.Dir * 170.f * Lunge; TiltA = 18.f * Lunge; PB += D.Dir * 45.f * (u > 0.4f ? 1.f - Ease((u - 0.4f) / 0.6f) : 0.f); TiltB = u > 0.4f ? -22.f * (1.f - Ease((u - 0.4f) / 0.6f)) : 0.f; }
				else { PB -= D.Dir * 170.f * Lunge; TiltB = 18.f * Lunge; PA -= D.Dir * 45.f * (u > 0.4f ? 1.f - Ease((u - 0.4f) / 0.6f) : 0.f); TiltA = u > 0.4f ? -22.f * (1.f - Ease((u - 0.4f) / 0.6f)) : 0.f; }
				if (u >= 0.4f) Once(8 + i, [&]()
				{
					const FVector Hp = (Ex[i].bBlue ? D.VexAt - D.Dir * 70.f : D.BStop + D.Dir * 70.f) + FVector(0, 0, 140.f);
					Blast(W, TEXT("Mid_Explosion_v2"), Hp, i == 3 ? 0.3f : 0.22f);
					AZoogiFX::Spawn(W, Hp, Ex[i].bBlue ? FLinearColor(0.5f, 0.85f, 1.f) : FLinearColor(1.f, 0.45f, 0.1f), 60.f, 0.2f, false, 6.f);
					D.Shake = 0.6f;
					ZoogiUtil::PlaySfx(G, i == 3 ? TEXT("HitCrush") : TEXT("HitCrack"), 1.f, Ex[i].bBlue ? 1.05f : 0.9f); // b214
				});
			}
			SqA = 1.f + 0.03f * FMath::Sin(t * 5.f); SqB = 1.f + 0.03f * FMath::Cos(t * 5.f);
			PlaceModel(MA, PA, Face(D.Dir, -TiltA), D.HA, D.MinA, SqA);
			PlaceModel(MB, PB, Face(-D.Dir, -TiltB), D.HB, D.MinB, SqB);
			const float Ang = FMath::DegreesToRadians(FMath::Lerp(35.f, 115.f, Ease(Seg(t, 4.f, 9.f))));
			const FVector Orbit = (D.Side * FMath::Cos(Ang) - D.Dir * FMath::Sin(Ang) * 0.6f).GetSafeNormal();
			Look(Cm, Mid + Orbit * 640.f + FVector(0, 0, 170.f), Mid + FVector(0, 0, 120.f), D.Shake);
			return;
		}
		// ---- 3 FLY (9.0 - 11.6): crouch, leap, spin up into the air, hang, dive in with a flying punch
		const FVector Apex = D.BStop + D.Dir * 90.f + FVector(0, 0, 540.f);
		const FVector PunchAt = D.VexAt - D.Dir * 90.f + FVector(0, 0, 40.f);
		bool bVexFlying = false;
		if (t < 11.6f)
		{
			Once(12, [&]() { ZoogiFilm::Mark(TEXT("fly")); });
			if (t < 9.3f) { SqA = 1.f - 0.18f * Ease(Seg(t, 9.0f, 9.3f)); YawA = 0.f; }
			else if (t < 10.5f)
			{
				Once(13, [&]() { ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 1.f, 0.8f); ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.8f, 1.2f); AZoogiFX::Spawn(W, D.BStop + FVector(0, 0, 6.f), FLinearColor(0.5f, 0.85f, 1.f), 300.f, 0.5f, true, 8.f); for (int32 q = 0; q < 6; ++q) Smoke(W, D.BStop + FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), 10.f), 120.f, 1.6f); });
				const float k = Ease(Seg(t, 9.3f, 10.5f));
				PA = FMath::Lerp(D.BStop, Apex, k); PA.Z = D.FloorZ + 540.f * FMath::Sin(k * PI * 0.5f);
				YawA = 720.f * k; SqA = 1.f + 0.12f * (1.f - k);
			}
			else if (t < 11.15f) { PA = Apex + FVector(0, 0, 12.f * FMath::Sin(t * 9.f)); YawA = 0.f; }
			else
			{
				const float k = FMath::Pow(Seg(t, 11.15f, 11.6f), 1.6f);
				PA = FMath::Lerp(Apex, PunchAt, k); TiltA = 55.f * k;
			}
			if (t > 9.3f && Tn >= D.NextTrail) { D.NextTrail = Tn + 0.035; AZoogiFX::Spawn(W, PA + FVector(0, 0, 110.f), FLinearColor(0.45f, 0.85f, 1.f), 70.f, 0.5f, false, 7.f); }
			PlaceModel(MA, PA, Face(D.Dir, -TiltA) + FRotator(0.f, YawA, 0.f), D.HA, D.MinA, SqA);
			PlaceModel(MB, D.VexAt + FVector(0, 0, FMath::Max(0.f, -Bob)), Face(-D.Dir, 8.f * FMath::Sin(t * 3.f)), D.HB, D.MinB, 1.f);
			// camera low on the side, looking up at the flight
			Look(Cm, Mid - D.Side * 760.f - D.Dir * 120.f + FVector(0, 0, 60.f), FMath::Lerp(Mid + FVector(0, 0, 150.f), PA + FVector(0, 0, 120.f), 0.75f), D.Shake);
			return;
		}
		// ---- 4 ROCK SMASH (11.6 - 19): the punch blasts Vex across the court into the rock; the blast bursts OUT of the rock face
		Once(14, [&]()
		{
			Blast(W, TEXT("Mid_Explosion"), D.VexAt + FVector(0, 0, 120.f), 0.35f);
			AZoogiFX::Spawn(W, D.VexAt + FVector(0, 0, 6.f), FLinearColor(1.f, 0.85f, 0.4f), 380.f, 0.5f, true, 9.f);
			D.Shake = 1.f; ZoogiFilm::Mark(TEXT("punch")); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 1.f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.8f, 1.f);
		});
		const float tHit = 12.25f;
		FVector PVex; FRotator RVex;
		if (t < tHit)
		{
			const float k = Seg(t, 11.6f, tHit);
			PVex = FMath::Lerp(D.VexAt, D.Hit - FVector(0, 0, 100.f), k) + FVector(0, 0, 160.f * FMath::Sin(k * PI) * 0.6f + 100.f * k);
			RVex = Face(-D.Dir, -40.f - 400.f * k);
			bVexFlying = true;
			if (Tn >= D.NextTrail) { D.NextTrail = Tn + 0.03; AZoogiFX::Spawn(W, PVex + FVector(0, 0, 110.f), FLinearColor(1.f, 0.5f, 0.15f), 80.f, 0.4f, false, 7.f); }
		}
		else
		{
			Once(15, [&]()
			{	// the side blast out of the rock face + a second burst, dust, the rock shakes
				Blast(W, TEXT("Mid_Explosion"), D.Hit - D.Dir * 20.f, 0.4f, -D.Dir);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), D.Hit - D.Dir * 30.f, FRotationMatrix::MakeFromZ(-D.Dir).Rotator(), 0.6f);
				AZoogiFX::Spawn(W, D.Hit, FLinearColor(0.75f, 0.7f, 0.6f), 260.f, 0.6f, false, 6.f);
				D.Shake = 1.4f; ZoogiFilm::Mark(TEXT("rock_smash"));
				ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 1.f, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.9f, 1.f); ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 0.7f, 1.f);
			});
			if (t > tHit + 0.18f) Once(17, [&]() { Blast(W, TEXT("Mid_Explosion_v2"), D.Hit - D.Dir * 60.f + D.Side * 60.f + FVector(0, 0, 60.f), 0.3f, -D.Dir + D.Side * 0.5f); });
			if (t > tHit + 0.36f) Once(18, [&]() { Blast(W, TEXT("Mid_Explosion_v3"), D.Hit - D.Dir * 50.f - D.Side * 70.f - FVector(0, 0, 40.f), 0.3f, -D.Dir - D.Side * 0.5f); });
			// Vex bounces off the rock and lands flat - KO
			const float k = Ease(Seg(t, tHit, tHit + 0.9f));
			const FVector Land = D.RockAt - D.Dir * 420.f;
			PVex = FMath::Lerp(D.Hit - FVector(0, 0, 100.f), Land, k) + FVector(0, 0, 140.f * FMath::Sin(k * PI) * (1.f - k));
			PVex.Z = FMath::Max(PVex.Z, D.FloorZ);
			RVex = Face(-D.Dir, FMath::Lerp(-440.f, -800.f, k));
			if (k >= 1.f) RVex = Face(-D.Dir, -80.f);
			if (D.Rock.IsValid() && t < tHit + 0.6f) D.Rock->AddActorWorldOffset(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 4.f * (1.f - Seg(t, tHit, tHit + 0.6f)));
			if (t > tHit + 0.35f) Once(21, [&]() { ZoogiUtil::PlaySfx(G, TEXT("Debris"), 0.9f, 1.f); });
			if (k >= 1.f) Once(19, [&]() { ZoogiUtil::PlaySfx(G, TEXT("Land"), 1.f, 0.8f); ZoogiUtil::PlaySfx(G, TEXT("Win"), 0.6f, 1.f); AZoogiFX::Spawn(W, FVector(Land.X, Land.Y, D.FloorZ + 4.f), FLinearColor(0.75f, 0.7f, 0.6f), 200.f, 0.5f, true, 6.f); D.Shake = 0.5f; });
		}
		PlaceModel(MB, PVex, RVex, D.HB, D.MinB, 1.f);
		// Blue lands after the punch and celebrates (a hop and a spin)
		{
			const float k = Ease(Seg(t, 11.6f, 12.1f));
			FVector Pl = FMath::Lerp(PunchAt, D.VexAt - D.Dir * 60.f, k); Pl.Z = FMath::Lerp(PunchAt.Z, D.FloorZ, k);
			float Yaw = 0.f;
			const float c = Seg(t, 14.0f, 14.8f);
			if (c > 0.f && c < 1.f) { Pl.Z += 120.f * FMath::Sin(c * PI); Yaw = 360.f * Ease(c); }
			PlaceModel(MA, Pl, Face(D.Dir, -50.f * (1.f - k)) + FRotator(0.f, Yaw, 0.f), D.HA, D.MinA, 1.f);
		}
		// camera: wide side view of the flight into the rock, then a slow push in on the hero with the cracked rock behind
		const FVector SideMid = (D.VexAt + D.RockAt) * 0.5f;
		if (t < 14.f) Look(Cm, SideMid + D.Side * 1050.f - D.Dir * 150.f + FVector(0, 0, 230.f), SideMid + FVector(0, 0, 110.f) + D.Dir * (bVexFlying ? 0.f : 60.f), D.Shake);
		else
		{
			const float k = Ease(Seg(t, 14.f, 18.f));
			const FVector Hero = D.VexAt - D.Dir * 60.f + FVector(0, 0, 140.f);
			Look(Cm, FMath::Lerp(SideMid + D.Side * 1050.f - D.Dir * 150.f + FVector(0, 0, 230.f), Hero - D.Dir * 420.f + D.Side * 240.f + FVector(0, 0, 40.f), k), FMath::Lerp(SideMid + FVector(0, 0, 110.f), Hero, k), D.Shake);
		}
		if (t > 19.0f) { D.bDone = true; UE_LOG(LogZoogi, Log, TEXT("DEMO reel done")); }
	}
}
