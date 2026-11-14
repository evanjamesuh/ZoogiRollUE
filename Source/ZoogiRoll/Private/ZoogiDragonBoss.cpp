// b177: DRACORIA, the Frozen boss - see ZoogiDragonBoss.h
#include "ZoogiDragonBoss.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiScreenFx.h"
#include "ZoogiTypes.h"
#include "ZoogiHaptics.h"
#include "ZoogiFrozenLayout.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ZoogiActionFun.h"
#include "ZoogiEpic.h"

namespace ZoogiDragon
{
	namespace
	{
		enum class ES : uint8 { Away, Arrive, Perch, Leave, Dive, Breath, Return, Dead, Seated, FlyIn, Landing, IntroRoar }; // b178h: + the fly-in intro
		struct FD
		{
			TWeakObjectPtr<AActor> A; TWeakObjectPtr<USkeletalMeshComponent> Mc; TWeakObjectPtr<AZoogiGameMode> G;
			TWeakObjectPtr<UNiagaraComponent> Breath;
			// b177c (Evan: "use the beam for its dragon breath" - the Stylized Laser Beams look, built from our Unreal parts):
			// a white-hot core, an icy glow, a faint haze, crackling lightning arcs along it, flares at the mouth and on the court
			TArray<TWeakObjectPtr<UStaticMeshComponent>> Laser; TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> LaserMID;
			TArray<TWeakObjectPtr<UStaticMeshComponent>> Arcs; float LaserT = 0.f, ArcT = 0.f, FlareT = 0.f;
			TArray<TWeakObjectPtr<UStaticMeshComponent>> Orbs, Pulses; float SparkT = 0.f; // b177f
			ES S = ES::Away; float T = 0.f, Dur = 1.f; FVector From = FVector::ZeroVector, Pos = FVector::ZeroVector; float Yaw = 0.f;
			float R = 800.f; bool bOriented = false; float MeshOff = 0.f; // b177e: the mesh yaw fix, added to every actor rotation (the mesh IS the root)
			FString Base, Cur; float OneLeft = 0.f, IdleT = 0.f, PuffT = 0.f, PerchAng = 0.f, SwAng = 0.f, SwMid = 0.f; bool bSweep = false, bLanded = false;
			FVector HoverP = FVector::ZeroVector, Ctr = FVector::ZeroVector; float ER = 1800.f, RimR = 1800.f, RimZ = 0.f;
			// b178h (Evan: "I didn't see a fly in intro for the frozen dragon, I need an epic intro"): the match opens on Dracoria diving out
			// of the clouds, sweeping over the court, landing on its islet and roaring at the camera under a DRACORIA title card
			bool bIntro = false, bIntroPlayed = false, bFlyRoar = false, bTouch = false, bTitle = false; float IntroAll = 0.f, Shake = 0.f, CamFov = 55.f; int32 CamShot = -1;
			FVector IP0 = FVector::ZeroVector, IP1 = FVector::ZeroVector, IP2 = FVector::ZeroVector, HomeP = FVector::ZeroVector, CamP = FVector::ZeroVector; float HomeYawS = 180.f;
			TWeakObjectPtr<ACameraActor> ICam; TWeakObjectPtr<AActor> IPrev;
		};
		FD D;
		const FLinearColor Ice(0.55f, 0.88f, 1.f);
		UAnimSequence* Anim(const FString& N)
		{
			static TMap<FString, TWeakObjectPtr<UAnimSequence>> Cache;
			if (TWeakObjectPtr<UAnimSequence>* C = Cache.Find(N)) if (C->IsValid()) return C->Get();
			UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/Dracoria/Animations/A_Dracoria_%s.A_Dracoria_%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!S) UE_LOG(LogZoogi, Warning, TEXT("DRAGON: no anim %s"), *N);
			Cache.Add(N, S);
			return S;
		}
		void Loop(const TCHAR* N, float Rate = 1.f)
		{
			D.Base = N; D.OneLeft = 0.f;
			USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc || D.Cur == N) return;
			if (UAnimSequence* S = Anim(N)) { Mc->PlayAnimation(S, true); Mc->SetPlayRate(Rate); D.Cur = N; }
		}
		void Once(const TCHAR* N, float Rate = 1.f)
		{	// a one-shot, then back to the state's loop
			USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc) return;
			if (UAnimSequence* S = Anim(N)) { Mc->PlayAnimation(S, false); Mc->SetPlayRate(Rate); D.Cur = N; D.OneLeft = S->GetPlayLength() / FMath::Max(0.1f, Rate) - 0.1f; }
		}
		void Go(ES S, float Dur)
		{
			D.S = S; D.T = 0.f; D.Dur = FMath::Max(0.05f, Dur); D.From = D.Pos;
			static const TCHAR* Nm[] = { TEXT("away"), TEXT("arrive"), TEXT("perch"), TEXT("leave"), TEXT("dive"), TEXT("breath"), TEXT("return"), TEXT("dead"), TEXT("seated"), TEXT("flyin"), TEXT("landing"), TEXT("introroar") };
			UE_LOG(LogZoogi, Log, TEXT("DRAGON -> %s (%.1f s)"), Nm[(int32)S], Dur);
			if (AZoogiGameMode* G = D.G.Get()) if (!G->ShotDir.IsEmpty() && S != ES::Away) { G->TakeShotDelayed(FString::Printf(TEXT("dragon_%s_a"), Nm[(int32)S]), 0.35f); G->TakeShotDelayed(FString::Printf(TEXT("dragon_%s_b"), Nm[(int32)S]), FMath::Min(Dur * 0.8f, 2.f)); }
			if (AZoogiGameMode* G = D.G.Get()) if (!G->ShotDir.IsEmpty() && S == ES::Breath) for (float St : { 0.6f, 0.9f, 1.3f, 1.7f }) G->TakeShotDelayed(FString::Printf(TEXT("dragon_beam_%03d"), FMath::RoundToInt(St * 100.f)), St); // b177f: beam play-test frames
		}
		// b179 (Evan: "a dragon roar sfx when it roars"): an original synthesized roar (SourceArt/Audio/Dragon, S_DragonRoar / S_DragonRoarShort)
		void RoarSfx(AZoogiGameMode* G, bool bLong, float Vol = 1.f)
		{
			if (!G) return;
			const TCHAR* N = bLong ? TEXT("DragonRoar") : TEXT("DragonRoarShort");
			if (ZoogiUtil::LoadSound(N)) ZoogiUtil::PlaySfx(G, N, Vol, FMath::FRandRange(0.94f, 1.04f));
			else ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f * Vol, 0.45f); // not imported yet
		}
		float Ease(float K) { K = FMath::Clamp(K, 0.f, 1.f); return K * K * (3.f - 2.f * K); }
		FVector Dir(float Ang) { return FRotator(0.f, Ang, 0.f).Vector(); }
		FVector BeamP(float Ang) { return FVector(D.Ctr.X, D.Ctr.Y, D.Ctr.Z + 40.f) + Dir(Ang) * D.ER * 0.8f; }
		FVector Mouth()
		{
			USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc) return D.Pos;
			for (const TCHAR* Bn : { TEXT("jaw"), TEXT("JAW"), TEXT("head") }) if (Mc->GetBoneIndex(FName(Bn)) != INDEX_NONE) return Mc->GetBoneLocation(FName(Bn));
			return D.Pos + FVector(0.f, 0.f, D.R);
		}
		UStaticMeshComponent* LaserPiece(AActor* A, UStaticMesh* M, UMaterialInterface* Mat)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
			C->SetStaticMesh(M); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
			C->SetMobility(EComponentMobility::Movable); C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
			if (Mat) C->SetMaterial(0, Mat);
			C->SetupAttachment(A->GetRootComponent()); C->RegisterComponent();
			return C;
		}
		void BreathOn(bool bOn)
		{
			AActor* A = D.A.Get();
			if (bOn && D.Laser.Num() == 0 && A)
			{
				UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				UStaticMesh* Arc1 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vefects/Easy_Impact_Frames/VFX/Extras/Meshes/SM_VFX_Lightning_Arc_01.SM_VFX_Lightning_Arc_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				UStaticMesh* Arc2 = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vefects/Easy_Impact_Frames/VFX/Extras/Meshes/SM_VFX_Lightning_Arc_02.SM_VFX_Lightning_Arc_02"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				UMaterialInterface* Zap = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Vefects/Easy_Impact_Frames/VFX/Extras/Materials/Zap/MI_Zap_Lightning_01.MI_Zap_Lightning_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (Cyl)
				{	// core (white-hot), glow (ice blue), haze (faint, wide)
					// b177f (Evan: "more beam - look like the stylized beam"): white-hot core, saturated blue glow, wide haze
					// b177h: was blowing out to plain white - a thin hot core, saturated blue edges, a faint wide haze
					const FLinearColor Cols[] = { FLinearColor(0.9f, 0.97f, 1.f), FLinearColor(0.1f, 0.55f, 1.f), FLinearColor(0.02f, 0.25f, 1.f), FLinearColor(0.05f, 0.2f, 0.9f) };
					const float Str[] = { 9.f, 2.2f, 0.9f, 0.25f };
					for (int32 k = 0; k < 4; ++k)
					{
						UMaterialInstanceDynamic* M = ZoogiUtil::FxMID(A, Cols[k], Str[k]);
						D.Laser.Add(LaserPiece(A, Cyl, M)); D.LaserMID.Add(M);
					}
					if (UStaticMesh* Sph = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet))
					{	// [4] mouth orb, [5] impact orb, [6] impact halo
						D.Orbs.Add(LaserPiece(A, Sph, ZoogiUtil::FxMID(A, FLinearColor(0.6f, 0.9f, 1.f), 6.f)));
						D.Orbs.Add(LaserPiece(A, Sph, ZoogiUtil::FxMID(A, FLinearColor(0.55f, 0.88f, 1.f), 5.f)));
						D.Orbs.Add(LaserPiece(A, Sph, ZoogiUtil::FxMID(A, FLinearColor(0.05f, 0.35f, 1.f), 0.8f)));
					}
					for (int32 k = 0; k < 4; ++k) D.Pulses.Add(LaserPiece(A, Cyl, ZoogiUtil::FxMID(A, FLinearColor(0.3f, 0.8f, 1.f), 3.f))); // energy pulses streaming down the beam
				}
				for (int32 k = 0; k < 6; ++k) if (UStaticMesh* Am = (k % 2) ? Arc2 : Arc1) D.Arcs.Add(LaserPiece(A, Am, Zap));
				D.LaserT = 0.f; D.ArcT = 0.f; D.FlareT = 0.f;
				if (UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Dracoria/VFX/Niagara/NS_Dracoria_Fly_Breath_Loop_head.NS_Dracoria_Fly_Breath_Loop_head"), nullptr, LOAD_NoWarn | LOAD_Quiet))
					if (UNiagaraComponent* Nc = UNiagaraFunctionLibrary::SpawnSystemAtLocation(A->GetWorld(), Sys, Mouth(), FRotator::ZeroRotator, FVector(D.R / 800.f), false, true, ENCPoolMethod::None)) D.Breath = Nc;
				UE_LOG(LogZoogi, Log, TEXT("DRAGON laser breath on: %d beam layers, %d lightning arcs%s"), D.Laser.Num(), D.Arcs.Num(), Zap ? TEXT("") : TEXT(" (no zap material)"));
			}
			else if (!bOn)
			{
				for (TWeakObjectPtr<UStaticMeshComponent>& C : D.Laser) if (UStaticMeshComponent* Cc = C.Get()) Cc->DestroyComponent();
				for (TWeakObjectPtr<UStaticMeshComponent>& C : D.Arcs) if (UStaticMeshComponent* Cc = C.Get()) Cc->DestroyComponent();
				for (TWeakObjectPtr<UStaticMeshComponent>& C : D.Orbs) if (UStaticMeshComponent* Cc = C.Get()) Cc->DestroyComponent();
				for (TWeakObjectPtr<UStaticMeshComponent>& C : D.Pulses) if (UStaticMeshComponent* Cc = C.Get()) Cc->DestroyComponent();
				D.Laser.Reset(); D.LaserMID.Reset(); D.Arcs.Reset(); D.Orbs.Reset(); D.Pulses.Reset();
				if (UNiagaraComponent* Nc = D.Breath.Get()) { Nc->Deactivate(); Nc->SetAutoDestroy(true); } D.Breath.Reset();
			}
		}
		void LaserStep(UWorld* W, float Dt, const FVector& M, const FVector& Bp)
		{	// the beam reaches out from the mouth in 0.25 s, then crackles
			D.LaserT += Dt;
			const FVector Dv = Bp - M; const float FullL = Dv.Size(); if (FullL < 1.f) return;
			const FVector Dir = Dv / FullL;
			const float L = FullL * FMath::Clamp(D.LaserT / 0.25f, 0.f, 1.f);
			const FRotator Rot = FRotationMatrix::MakeFromZ(Dir).Rotator();
			const float Base = D.R * 0.028f; // core radius (~40 cm)
			const float Rad[] = { Base, Base * 2.f, Base * 3.4f, Base * 6.f };
			const float Fl = 0.85f + 0.15f * FMath::Sin(D.LaserT * 47.f) + FMath::FRandRange(-0.08f, 0.08f);
			for (int32 k = 0; k < D.Laser.Num(); ++k) if (UStaticMeshComponent* C = D.Laser[k].Get())
			{
				const float R = Rad[FMath::Min(k, 3)] * (k == 0 ? 1.f : Fl) * (D.LaserT < 0.35f ? 1.6f - D.LaserT / 0.35f * 0.6f : 1.f); // a fat burst as it fires
				C->SetWorldLocationAndRotation(M + Dir * L * 0.5f, Rot);
				C->SetWorldScale3D(FVector(R / 50.f, R / 50.f, L / 100.f)); // the engine cylinder: 100 tall, radius 50
			}
			{	// b177f: the orbs at both ends + energy pulses streaming mouth -> court
				const float Pz = 1.f + 0.25f * FMath::Sin(D.LaserT * 31.f) + FMath::FRandRange(-0.1f, 0.1f);
				const float OrbR[] = { Base * 3.f * Pz, Base * 3.6f * Pz, Base * 10.f * (0.9f + 0.1f * Pz) };
				const FVector OrbAt[] = { M, M + Dir * L, M + Dir * L };
				for (int32 k = 0; k < D.Orbs.Num(); ++k) if (UStaticMeshComponent* O = D.Orbs[k].Get())
				{
					O->SetWorldLocation(OrbAt[k]);
					O->SetWorldScale3D(k == 2 ? FVector(OrbR[k] / 50.f, OrbR[k] / 50.f, OrbR[k] / 150.f) : FVector(OrbR[k] / 50.f)); // the halo is a flat splash on the court
					O->SetVisibility(k == 0 || D.LaserT > 0.2f);
				}
				for (int32 k = 0; k < D.Pulses.Num(); ++k) if (UStaticMeshComponent* Pc = D.Pulses[k].Get())
				{
					const float Ph = FMath::Fmod(D.LaserT * 2.2f + k / (float)D.Pulses.Num(), 1.f);
					const float Pr = Base * (2.3f + 0.7f * FMath::Sin(Ph * PI));
					Pc->SetWorldLocationAndRotation(M + Dir * L * Ph, Rot);
					Pc->SetWorldScale3D(FVector(Pr / 50.f, Pr / 50.f, FMath::Max(1.f, L * 0.06f) / 100.f));
					Pc->SetVisibility(Ph * FullL < L);
				}
				if ((D.SparkT -= Dt) <= 0.f && D.LaserT > 0.2f)
				{	// sparks spraying off the strike point
					D.SparkT = 0.09f;
					AZoogiFX::Niagara(W, FMath::FRand() < 0.5f ? TEXT("NS_Burst_Blue") : TEXT("NS_Burst_White"), M + Dir * L, FRotator::ZeroRotator, 0.7f);
				}
			}
			if ((D.ArcT -= Dt) <= 0.f)
			{	// lightning arcs jump to new spots along the beam every 0.06 s
				D.ArcT = 0.06f;
				for (int32 k = 0; k < D.Arcs.Num(); ++k) if (UStaticMeshComponent* C = D.Arcs[k].Get())
				{
					const float At = FMath::FRandRange(0.05f, 0.95f);
					C->SetVisibility(FMath::FRand() < 0.8f);
					C->SetWorldLocationAndRotation(M + Dir * L * At, FRotator(FMath::FRandRange(-180.f, 180.f), Rot.Yaw + FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-180.f, 180.f)));
					C->SetWorldScale3D(FVector(D.R * FMath::FRandRange(0.0025f, 0.005f)));
				}
			}
			if ((D.FlareT -= Dt) <= 0.f && D.LaserT > 0.2f)
			{	// flares: the mouth glow and the strike on the court
				D.FlareT = 0.12f;
				AZoogiFX::Spawn(W, M, FLinearColor(0.75f, 0.95f, 1.f), Base * 5.f, 0.18f, false, 14.f);
				AZoogiFX::Spawn(W, Bp, FLinearColor(0.6f, 0.9f, 1.f), 260.f, 0.3f, true, 12.f);
				AZoogiFX::Stylized(W, TEXT("ElectricShock"), Bp, 0.55f);
			}
		}
	}

	namespace
	{	// ---- b178h: the fly-in intro
		FVector Bez(float K) { const float A = 1.f - K; return D.IP0 * (A * A) + D.IP1 * (2.f * A * K) + D.IP2 * (K * K); }
		APlayerController* IntroPc() { AZoogiGameMode* G = D.G.Get(); return G && G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr; }
		void IntroCamEnd(float Blend)
		{
			APlayerController* Pc = IntroPc();
			if (Pc && D.ICam.IsValid() && Pc->GetViewTarget() == D.ICam.Get())
			{
				AActor* Back = D.IPrev.IsValid() ? D.IPrev.Get() : Pc->GetPawn();
				if (Back) Pc->SetViewTargetWithBlend(Back, Blend, VTBlend_Cubic);
			}
			if (ACameraActor* Cm = D.ICam.Get()) Cm->SetLifeSpan(Blend + 0.5f);
			D.ICam.Reset(); D.IPrev.Reset();
		}
		void IntroStart(AZoogiGameMode* G)
		{
			AActor* A = D.A.Get(); if (!A || !G) return;
			const float HY = D.HomeYawS;
			D.IP2 = D.HomeP + FVector(0.f, 0.f, D.R * 0.55f);
			D.IP0 = D.Ctr + Dir(HY + 140.f) * D.ER * 6.5f + FVector(0.f, 0.f, D.ER * 3.f);  // high in the clouds, far out
			D.IP1 = D.Ctr + Dir(HY - 75.f) * D.ER * 1.6f + FVector(0.f, 0.f, D.ER * 1.1f);  // banks over the far side of the court
			D.Pos = D.IP0; D.Yaw = (D.IP1 - D.IP0).Rotation().Yaw;
			D.IntroAll = 0.f; D.bFlyRoar = D.bTouch = D.bTitle = false; D.CamShot = -1; D.Shake = 0.f;
			A->SetActorHiddenInGame(false);
			Go(ES::FlyIn, 4.6f); Loop(TEXT("Glide"), 1.f);
			ZoogiUtil::PlaySfx(G, TEXT("EpicRiser"), 0.9f, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.8f, 0.6f);
			if (APlayerController* Pc = IntroPc())
			{
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ACameraActor* Cm = G->GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator::ZeroRotator, D.Ctr + FVector(0, 0, 400.f)), SP))
				{
					Cm->GetCameraComponent()->bConstrainAspectRatio = false; Cm->GetCameraComponent()->SetFieldOfView(50.f);
					D.ICam = Cm; D.IPrev = Pc->GetViewTarget(); Pc->SetViewTarget(Cm);
				}
			}
			if (!G->ShotDir.IsEmpty()) for (float St : { 0.8f, 1.8f, 2.8f, 3.8f, 4.9f, 5.6f, 6.4f, 7.4f, 8.6f }) G->TakeShotDelayed(FString::Printf(TEXT("dragon_intro_%03d"), FMath::RoundToInt(St * 10.f)), St);
			UE_LOG(LogZoogi, Log, TEXT("DRAGON INTRO: Dracoria dives in from the clouds"));
		}
		void IntroFinish(bool bSkipped)
		{
			D.Pos = D.HomeP; D.Yaw = D.HomeYawS + 180.f;
			Go(ES::Seated, 1.f); Once(TEXT("Sit_Down")); D.Base = TEXT("Sit_Idle");
			if (bSkipped) Loop(TEXT("Sit_Idle"), 0.8f);
			IntroCamEnd(bSkipped ? 0.25f : 0.8f);
			D.bIntro = false; D.bIntroPlayed = true;
			UE_LOG(LogZoogi, Log, TEXT("DRAGON INTRO: %s"), bSkipped ? TEXT("skipped") : TEXT("done"));
		}
		void IntroCamStep(float Dt)
		{
			ACameraActor* Cm = D.ICam.Get(); if (!Cm) return;
			const FVector Head = HeadLoc();
			const float HY = D.HomeYawS, FaceIn = HY + 180.f;
			int32 Shot = 0; FVector Want = Cm->GetActorLocation(); float Fov = 55.f;
			if (D.S == ES::FlyIn && D.T < D.Dur * 0.42f)
			{	// A: low on the court, across from where it comes from, looking up as it breaks out of the clouds
				Shot = 0; Want = D.Ctr + Dir(HY + 140.f + 180.f + 40.f) * D.ER * 1.05f + FVector(0.f, 0.f, D.ER * 0.4f); // off to the side + higher: the centre spire / orbs blocked it Fov = FMath::Lerp(42.f, 58.f, D.T / (D.Dur * 0.42f));
			}
			else if (D.S == ES::FlyIn)
			{	// B: ahead of it and off to the side, looking back into its face as it sweeps over the court
				Shot = 1; Want = D.Pos + Dir(D.Yaw + 28.f) * D.R * 2.6f + FVector(0.f, 0.f, D.R * 0.2f); Fov = 62.f;
			}
			else
			{	// C: in front of the islet, low, as it lands and roars - a slow push in
				Shot = 2; Want = D.HomeP + Dir(FaceIn) * D.R * 2.1f + FVector(0.f, 0.f, D.R * 0.35f);
				Fov = D.S == ES::IntroRoar ? FMath::Lerp(56.f, 38.f, FMath::Clamp(D.T / 2.6f, 0.f, 1.f)) : 56.f;
			}
			bool bCut = false;
			if (Shot != D.CamShot) { if (D.CamShot < 1) { D.CamP = Want; bCut = true; } D.CamShot = Shot; } // A -> B is a cut, B -> C glides
			D.CamP = FMath::VInterpTo(D.CamP, Want, Dt, Shot == 2 ? 2.5f : 5.f);
			D.CamFov = FMath::FInterpTo(D.CamFov, Fov, Dt, 4.f);
			D.Shake = FMath::Max(0.f, D.Shake - Dt * 1.4f);
			const FVector Jit = D.Shake > 0.f ? FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * D.Shake * D.R * 0.018f : FVector::ZeroVector;
			Cm->SetActorLocation(D.CamP + Jit);
			Cm->SetActorRotation(FMath::RInterpTo(Cm->GetActorRotation(), (Head - D.CamP).Rotation(), Dt, Shot == 0 ? 3.f : 7.f));
			if (bCut) Cm->SetActorRotation((Head - D.CamP).Rotation());
			Cm->GetCameraComponent()->SetFieldOfView(D.CamFov);
		}
	}

	bool IntroHold() { return Active() && D.bIntro && D.IntroAll < 16.f; }
	bool IntroPlayed() { return D.bIntroPlayed; }
	bool SkipIntro()
	{
		if (!Active() || !D.bIntro) return false;
		if (D.S == ES::Away) return false; // tapped during the forming: the forming skip runs, the intro still plays after it
		if (D.IntroAll < 0.6f) return true; // the tap that skipped the forming
		IntroFinish(true); return true;
	}

	bool Wanted(const AZoogiGameMode* G)
	{
		static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldFrostBoss"));
		return !bOld && G && G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen;
	}
	bool Active() { return D.A.IsValid() && D.Mc.IsValid(); }
	void Clear() { BreathOn(false); IntroCamEnd(0.f); if (AActor* A = D.A.Get()) A->Destroy(); D = FD(); }

	bool Spawn(AZoogiGameMode* G)
	{
		Clear();
		UWorld* W = G ? G->GetWorld() : nullptr; if (!W || !G->Arena) return false;
		USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Dracoria/Meshes/SK_Dracoria.SK_Dracoria"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SM) { UE_LOG(LogZoogi, Warning, TEXT("DRAGON: SK_Dracoria missing - Frost Titan stays")); return false; }
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* A = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(G->Arena->GetActorLocation() + FVector(0.f, 0.f, 20000.f)), SP);
		if (!A) return false;
		USkeletalMeshComponent* Mc = A->GetSkeletalMeshComponent();
		Mc->SetMobility(EComponentMobility::Movable);
		Mc->SetSkeletalMeshAsset(SM);
		Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mc->SetCastShadow(true);
		Mc->SetBoundsScale(3.f);
		Mc->bEnableUpdateRateOptimizations = false;
		Mc->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mc->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		D.A = A; D.Mc = Mc; D.G = G;
		D.ER = G->Arena->GetKnockoffRadius();
		const float Sr = FMath::Max(50.f, SM->GetBounds().SphereRadius);
		static float Want = -1.f; if (Want < 0.f) { Want = 0.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiDragonSize="), Want); }
		D.R = (Want > 0.f ? Want : 0.9f) * D.ER; // b177b: big enough to read (0.42 was a speck) // the dragon's reach (bounds radius) vs the court
		A->SetActorScale3D(FVector(D.R / Sr));
		A->SetActorHiddenInGame(true);
		Loop(TEXT("Glide"));
		D.S = ES::Away;
		D.bIntro = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDragonIntro")); // b178h
		UE_LOG(LogZoogi, Log, TEXT("DRAGON spawned: Dracoria, bounds %.0f cm -> scale %.2f (reach %.0f cm, court %.0f cm)"), Sr, D.R / Sr, D.R, D.ER);
		return true;
	}

	int32 SeatIslet() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldFrostBoss")); return bOld ? -1 : 0; }

	void Tick(AZoogiGameMode* G, float Dt, const FVector& Ctr, float ER, float RimR, float RimZ, float PeekAng, bool bShow, bool bDefeated, const FVector& LookAt)
	{	// b177d: home = the islet that lost its bleacher; it sits there, stands + roars when the boss is in play
		AActor* A = D.A.Get(); USkeletalMeshComponent* Mc = D.Mc.Get();
		if (!A || !Mc || !G) return;
		D.Ctr = Ctr; D.ER = ER; D.RimR = RimR; D.RimZ = RimZ;
		D.T += Dt;
		const float K = Ease(D.T / D.Dur);
		if (!D.bOriented && D.T > 0.1f && Mc->GetBoneIndex(FName(TEXT("head"))) != INDEX_NONE && Mc->GetBoneIndex(FName(TEXT("pelvis"))) != INDEX_NONE)
		{	// face the mesh's head down the actor's +X, whatever axis it was modelled on
			const FVector H = A->GetActorTransform().InverseTransformPosition(Mc->GetBoneLocation(FName(TEXT("head"))));
			const FVector P = A->GetActorTransform().InverseTransformPosition(Mc->GetBoneLocation(FName(TEXT("pelvis"))));
			const FVector2D F(H.X - P.X, H.Y - P.Y);
			if (F.Size() > 1.f) { const float Off = FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X)) - D.MeshOff; D.MeshOff = -Off; D.bOriented = true; UE_LOG(LogZoogi, Log, TEXT("DRAGON mesh faces %.0f deg - turned to +X"), Off); }
		}
		if (D.OneLeft > 0.f) { D.OneLeft -= Dt; if (D.OneLeft <= 0.f) { const FString B = D.Base; D.Cur.Reset(); Loop(*B); } }
		const int32 Si = FMath::Max(0, SeatIslet());
		const float HomeYaw = ZoogiFrozen::IsletYaw[Si];
		const FVector Home = FVector(Ctr.X, Ctr.Y, Ctr.Z + ZoogiFrozen::IsletTopZ) + Dir(HomeYaw) * (ZoogiFrozen::IsletD(Si) + D.R * 0.1f);
		const float FaceIn = HomeYaw + 180.f;
		float WantYaw = FaceIn;
		D.HomeP = Home; D.HomeYawS = HomeYaw; // b178h
		if (D.bIntro && D.S != ES::Away) D.IntroAll += Dt;
		const bool bForming = G->Arena && G->Arena->IsForming();
		float CamYaw = FaceIn; // b177e (Evan: "I want to see the front not the side"): it turns its face to the viewer
		if (APlayerController* Pc = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr) if (Pc->PlayerCameraManager)
		{
			const FVector Cl = Pc->PlayerCameraManager->GetCameraLocation();
			CamYaw = FaceIn + FMath::Clamp(FMath::FindDeltaAngleDegrees(FaceIn, (FVector(Cl.X, Cl.Y, 0.f) - FVector(D.Pos.X, D.Pos.Y, 0.f)).Rotation().Yaw), -80.f, 80.f);
		}
		A->SetActorHiddenInGame((bForming && (D.S == ES::Away || D.S == ES::Seated)) || (D.bIntro && D.S == ES::Away));
		if (bDefeated && D.S != ES::Dead) { BreathOn(false); Go(ES::Dead, 3.f); Once(TEXT("Death"), 0.8f); }
		if (D.bSweep && D.S != ES::Dive && D.S != ES::Breath && D.S != ES::Dead && !D.bIntro)
		{	// ICE BREATH: takes off from its seat and dives in over the rim
			A->SetActorHiddenInGame(false);
			D.HoverP = FVector(Ctr.X, Ctr.Y, RimZ) + Dir(D.SwMid) * (RimR + D.R * 0.35f) + FVector(0.f, 0.f, D.R * 0.42f);
			Go(ES::Dive, FMath::Max(0.6f, SweepLead() - 0.35f)); Loop(TEXT("Dive"), 1.1f);
			ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 1.f, 0.45f);
		}
		switch (D.S)
		{
		case ES::Away: // first tick: straight onto the seat (b178h: or, once the arena has formed, the fly-in)
			if (D.bIntro) { D.Pos = Home; if (!bForming) IntroStart(G); else break; break; }
			D.Pos = Home; D.Yaw = FaceIn; Go(ES::Seated, 1.f); Loop(TEXT("Sit_Idle"), 0.8f);
			break;
		case ES::FlyIn:
		{
			const float U = FMath::Clamp(D.T / D.Dur, 0.f, 1.f), Kc = 1.f - FMath::Pow(1.f - U, 1.5f);
			D.Pos = Bez(Kc);
			const FVector Tan = Bez(FMath::Min(1.f, Kc + 0.02f)) - Bez(FMath::Max(0.f, Kc - 0.02f));
			WantYaw = Tan.IsNearlyZero() ? D.Yaw : Tan.Rotation().Yaw;
			D.Yaw = FMath::FixedTurn(D.Yaw, WantYaw, 260.f * Dt);
			if (U > 0.12f && D.Cur == TEXT("Glide") && !D.bFlyRoar) Loop(TEXT("Fly_Flap"), 1.1f);
			if (!D.bFlyRoar && U > 0.45f) { D.bFlyRoar = true; Once(TEXT("Fly_Roar"), 0.9f); D.Base = TEXT("Fly_Flap"); RoarSfx(G, false, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.7f, 0.8f); D.Shake = 0.5f; ZoogiHaptics::Kick(0.4f, G->GetWorld(), TEXT("dragon_flyby")); }
			if (D.T >= D.Dur) { Go(ES::Landing, 1.4f); Once(TEXT("Land"), 1.f); D.Base = TEXT("Idle"); }
			IntroCamStep(Dt);
			A->SetActorLocationAndRotation(D.Pos, FRotator(0.f, D.Yaw + D.MeshOff, 0.f));
			return;
		}
		case ES::Landing:
			D.Pos = FMath::Lerp(D.From, Home, Ease(D.T / (D.Dur * 0.6f)));
			WantYaw = CamYaw;
			if (!D.bTouch && D.T > D.Dur * 0.55f)
			{	// touchdown: the islet shakes, ice and rock burst off it
				D.bTouch = true; D.Shake = 1.f;
				ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 1.f, 0.7f); ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.9f, 0.7f);
				UZoogiScreenFx::Shockwave(G->GetWorld(), Home, D.R * 0.9f, FLinearColor(0.55f, 0.88f, 1.f)); // an ice shockwave ring (rocks / frost spikes hid its face)
				ZoogiHaptics::Kick(0.7f, G->GetWorld(), TEXT("dragon_land"));
			}
			if (D.T >= D.Dur)
			{
				Go(ES::IntroRoar, 3.4f); Once(TEXT("Roar"), 0.85f); D.Base = TEXT("Idle");
				D.Shake = 1.2f; RoarSfx(G, true, 1.f); ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 0.7f, 0.85f);
				ZoogiHaptics::Kick(0.9f, G->GetWorld(), TEXT("dragon_intro_roar"));
				ZoogiEpic::Hit(G, ZoogiEpic::EHit::Big);
			}
			break;
		case ES::IntroRoar:
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 3.f); WantYaw = CamYaw;
			if (!D.bTitle && D.T > 0.45f)
			{
				D.bTitle = true;
				if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("DRACORIA"), TEXT("QUEEN OF THE FROZEN RING"), FLinearColor(0.55f, 0.88f, 1.f), 2.8f, 1.6f);
			}
			if (D.T >= D.Dur) { IntroFinish(false); break; }
			break;
		case ES::Seated:
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 3.f); WantYaw = CamYaw;
			{	// b179 (Evan: "a frontal view showing the arena" for the Frozen map picture): -ZoogiDragonPoster - from behind the far
				// side of the court, looking across the arena at Dracoria's face; a few heights / widths, then back to the game camera
				static const bool bPoster = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDragonPoster"));
				static float PosterT = -1.f; static int32 PosterShot = 0; static TWeakObjectPtr<ACameraActor> PCam;
				if (bPoster && !G->ShotDir.IsEmpty() && D.bIntroPlayed && PosterShot < 6)
				{
					PosterT += Dt;
					if (PosterT > 1.5f)
					{
						PosterT = 0.f;
						static const float Ht[6] = { 0.22f, 0.35f, 0.5f, 0.3f, 0.45f, 0.6f }, Bk[6] = { 1.0f, 1.0f, 1.0f, 0.55f, 0.55f, 1.35f };
						const FVector Far = FVector(Ctr.X, Ctr.Y, Ctr.Z) + Dir(HomeYaw + 180.f) * ER * Bk[PosterShot] + FVector(0.f, 0.f, ER * Ht[PosterShot]);
						APlayerController* Pc = G->GetWorld()->GetFirstPlayerController();
						if (!PCam.IsValid()) { FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; PCam = G->GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Far), SP); if (PCam.IsValid()) PCam->GetCameraComponent()->bConstrainAspectRatio = false; }
						if (ACameraActor* Cm = PCam.Get())
						{
							const FVector Look = HeadLoc() - FVector(0.f, 0.f, D.R * 0.35f);
							Cm->SetActorLocationAndRotation(Far, (Look - Far).Rotation());
							Cm->GetCameraComponent()->SetFieldOfView(PosterShot >= 3 ? 50.f : 62.f);
							if (Pc) Pc->SetViewTarget(Cm);
							G->TakeShotDelayed(FString::Printf(TEXT("dragon_poster_%d"), PosterShot), 0.5f);
							UE_LOG(LogZoogi, Log, TEXT("DRAGON poster shot %d"), PosterShot);
						}
						if (++PosterShot >= 6 && Pc) { if (ACameraActor* Cm = PCam.Get()) Cm->SetLifeSpan(3.f); }
					}
				}
			}
			if (bShow && !bForming) { Go(ES::Perch, 1.f); Once(TEXT("Sit_To_Stand")); D.Base = TEXT("Idle"); D.IdleT = 0.f; }
			else if ((D.IdleT += Dt) > 11.f && D.OneLeft <= 0.f) { D.IdleT = 0.f; Once(TEXT("Idle_LookAround"), 0.7f); D.Base = TEXT("Sit_Idle"); }
			break;
		case ES::Perch: // standing on its islet, watching the court
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 3.f);
			WantYaw = CamYaw;
			if (D.T > 1.2f && D.T < 1.3f && D.OneLeft <= 0.f) { Once(TEXT("Roar"), 0.9f); RoarSfx(G, true); ZoogiHaptics::Kick(0.5f, G->GetWorld(), TEXT("dragon_roar")); }
			if (!bShow && D.T > 4.f && D.OneLeft <= 0.f) { Go(ES::Seated, 1.f); Once(TEXT("Sit_Down")); D.Base = TEXT("Sit_Idle"); }
			else if ((D.IdleT += Dt) > 9.f && D.OneLeft <= 0.f) { D.IdleT = 0.f; Once(TEXT("Idle_LookAround"), 0.8f); }
			break;
		case ES::Dive:
			D.Pos = FMath::Lerp(D.From, D.HoverP, K) + FVector(0.f, 0.f, FMath::Sin(K * PI) * D.R * 0.5f); // up off the islet, then down to the hover
			WantYaw = (BeamP(D.SwAng) - D.Pos).Rotation().Yaw;
			if (D.T >= D.Dur) { Go(ES::Breath, 10.f); Loop(TEXT("Fly_Breath_Loop")); BreathOn(true); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.4f); RoarSfx(G, false, 0.8f); ZoogiHaptics::Kick(1.f, G->GetWorld(), TEXT("dragon_breath_start")); }
			break;
		case ES::Breath:
		{
			D.Pos = FMath::VInterpTo(D.Pos, D.HoverP, Dt, 2.f) + FVector(0.f, 0.f, FMath::Sin(D.T * 3.f) * 6.f);
			const FVector Bp = BeamP(D.SwAng);
			WantYaw = (Bp - D.Pos).Rotation().Yaw;
			const FVector M = Mouth();
			if (UNiagaraComponent* Nc = D.Breath.Get()) Nc->SetWorldLocationAndRotation(M, (Bp - M).Rotation());
			LaserStep(G->GetWorld(), Dt, M, Bp);
			{	// b179 (Evan: "the phone to vibrate during dragon breath"): a steady rumble pulse for as long as the beam burns
				static float BuzzT = 0.f;
				if ((BuzzT -= Dt) <= 0.f) { BuzzT = 0.22f; ZoogiHaptics::Kick(0.55f, G->GetWorld(), TEXT("dragon_breath")); }
			}
			if (!D.bSweep) { BreathOn(false); Go(ES::Return, 2.f); Loop(TEXT("Glide")); }
			break;
		}
		case ES::Return: // glides home and lands on its islet
			D.Pos = FMath::Lerp(D.From, Home, K) + FVector(0.f, 0.f, FMath::Sin(K * PI) * D.R * 0.6f);
			WantYaw = K < 0.75f ? (Home - D.From).Rotation().Yaw : CamYaw;
			if (D.T > D.Dur * 0.7f && D.Cur != TEXT("Land")) Once(TEXT("Land"));
			if (D.T >= D.Dur) { Go(ES::Perch, 1.f); D.Base = TEXT("Idle"); if (D.OneLeft <= 0.f) Loop(TEXT("Idle"), 0.8f); }
			break;
		case ES::Dead:
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 1.f);
			break;
		default: Go(ES::Seated, 1.f); Loop(TEXT("Sit_Idle"), 0.8f); break;
		}
		D.Yaw = FMath::FixedTurn(D.Yaw, WantYaw, 140.f * Dt);
		A->SetActorLocationAndRotation(D.Pos, FRotator(0.f, D.Yaw + D.MeshOff, 0.f));
		if (D.S == ES::Landing || D.S == ES::IntroRoar) IntroCamStep(Dt); // b178h
	}

	void Sweep(bool bOn, float AngDeg)
	{
		if (!Active()) return;
		if (bOn && !D.bSweep) { D.SwMid = AngDeg + 35.f; UE_LOG(LogZoogi, Log, TEXT("DRAGON ICE BREATH: dives in at yaw %.0f"), D.SwMid); }
		D.bSweep = bOn; if (bOn) D.SwAng = AngDeg;
	}
	void Event(const TCHAR* What)
	{
		if (!Active() || (D.S != ES::Perch && D.S != ES::Seated)) return;
		if (D.S == ES::Seated) { Go(ES::Perch, 1.f); Once(TEXT("Sit_To_Stand")); D.Base = TEXT("Idle"); return; } // b177d: stands up, then roars (Perch)
		const FString E(What);
		if (E == TEXT("Roar") || E == TEXT("Taunt")) { Once(TEXT("Roar"), 0.9f); if (AZoogiGameMode* Gr = D.G.Get()) { RoarSfx(Gr, true); ZoogiHaptics::Kick(0.5f, Gr->GetWorld(), TEXT("dragon_roar")); } }
		else if (E == TEXT("Stomp")) Once(TEXT("Attack_Stomp"));
		else if (E == TEXT("Hit")) Once(TEXT("Hit_Heavy"));
		else if (E == TEXT("Threat")) Once(TEXT("Threat_Display"));
	}
	bool CamSpot(FVector& OutPos, FVector& OutLook)
	{	// b177e: the boss cam stands in front of Dracoria's face (front view, not side)
		if (!Active()) return false;
		OutLook = HeadLoc();
		OutPos = D.Pos + Dir(D.Yaw) * (D.R * 1.5f) + FVector(0.f, 0.f, D.R * 0.3f);
		return true;
	}
	FVector HeadLoc()
	{
		USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc) return FVector::ZeroVector;
		return Mc->GetBoneIndex(FName(TEXT("head"))) != INDEX_NONE ? Mc->GetBoneLocation(FName(TEXT("head"))) : D.Pos;
	}
	float SweepLead() { return Active() ? 2.2f : 0.f; }
	float SweepSlow() { return Active() ? 1.7f : 1.f; }
	const TCHAR* SweepName(const TCHAR* Def) { return Active() ? TEXT("ICE BREATH") : Def; }
}
