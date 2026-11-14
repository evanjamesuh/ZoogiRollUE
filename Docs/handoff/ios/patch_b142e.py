# b142e: the Cosmos boss peek (see patch_b142d) - ZoogiCosmosBoss drives the puppet.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
B = 'Private/ZoogiCosmosBoss.cpp'
ed1(B, '''#include "Misc/Parse.h"''', '''#include "Misc/Parse.h"
#include "ZoogiAurexPuppet.h"
#include "Components/PoseableMeshComponent.h"''')
ed1(B, '''			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> CamPrev; float CamLeft = 0.f, CamT = 0.f, CamWant = 0.f, CamWait = 0.f; // b136 v6: the boss cam''', '''			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> CamPrev; float CamLeft = 0.f, CamT = 0.f, CamWant = 0.f, CamWait = 0.f; // b136 v6: the boss cam
			// b142 peek: the puppet on the rim
			TWeakObjectPtr<AZoogiAurexPuppet> Pup; float PeekAng = 0.f, WantPeek = 0.f, Dip = 1.f, Slam = 0.f, RoarLeft = 0.f, PeekLog = 0.f; bool bMoving = false; int32 PeekTurn = -1;''')
ed1(B, '''		bool AllArenas() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCosmosBossAll")); return b; }''', '''		bool AllArenas() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCosmosBossAll")); return b; }
		bool Titan() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossTitan")); return b; } // b142: the b136 far titan
		constexpr float PeekH = 1800.f; // b142: 18 m - head + shoulders over the far rim in the gameplay camera''')
# the peek step + spawn, placed before Step
ed1(B, '''		bool Step(float Dt)
		{''', '''		bool PeekStep(AZoogiGameMode* G, float Dt)
		{
			AZoogiAurexPuppet* P = B.Pup.Get();
			UWorld* W = G->GetWorld();
			const FVector C = Centre(G);
			const float ER = G->Arena->GetKnockoffRadius();
			APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			const AZoogiBall* Act = G->GetActiveBall();
			const bool bHumanAim = Act && !Act->bCPU && G->Phase == EZoogiPhase::Aiming;
			FVector CamL = C + FVector(0.f, 0.f, 1500.f);
			if (Pc && Pc->PlayerCameraManager)
			{
				CamL = Pc->PlayerCameraManager->GetCameraLocation();
				if (bHumanAim)
				{	// in front of the player: where the camera's forward ray meets the far rim
					const FVector Fw = Pc->PlayerCameraManager->GetCameraRotation().Vector();
					FVector2D O(CamL.X - C.X, CamL.Y - C.Y), D(Fw.X, Fw.Y);
					if (D.Normalize())
					{
						const float Bq = FVector2D::DotProduct(O, D), Cq = O.SizeSquared() - ER * ER, Disc = Bq * Bq - Cq;
						if (Disc > 0.f) { const FVector2D Hp = O + D * (-Bq + FMath::Sqrt(Disc)); B.WantPeek = FMath::RadiansToDegrees(FMath::Atan2(Hp.Y, Hp.X)); }
					}
					if (G->TurnCounter != B.PeekTurn) { B.PeekTurn = G->TurnCounter; if (FMath::Abs(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek)) > 20.f) B.bMoving = true; }
				}
			}
			const float Dd = FMath::Abs(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek));
			if (!B.bMoving && Dd > 40.f) B.bMoving = true; // the aim turned a lot: duck and come up in front again
			if (B.bMoving)
			{	// ducks under the rim, moves, comes back up
				B.Dip = FMath::FInterpConstantTo(B.Dip, 1.f, Dt, 2.4f);
				if (B.Dip >= 0.99f) { B.PeekAng = B.WantPeek; B.bMoving = false; UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peek: comes up at yaw %.0f (in front of the camera)"), B.PeekAng); }
			}
			else
			{
				B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 0.9f);
				B.PeekAng += FMath::Clamp(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek), -30.f * Dt, 30.f * Dt); // slides along the rim
			}
			const FVector Dir = FRotator(0.f, B.PeekAng, 0.f).Vector();
			B.Slam = FMath::Max(0.f, B.Slam - Dt * 1.6f);
			B.Lean = FMath::FInterpTo(B.Lean, B.LeanTo, Dt, 1.6f);
			if (B.LeanTo > 0.f && (B.Cool <= 1.5f)) B.LeanTo = 0.f;
			if (B.RoarLeft > 0.f) { B.RoarLeft -= Dt; P->Pose = EZoogiPuppetPose::Cheer; P->bPinHands = false; }
			else { P->Pose = EZoogiPuppetPose::Peek; P->bPinHands = true; }
			const float Slam01 = B.Slam > 0.5f ? (1.f - B.Slam) * 2.f : B.Slam * 2.f; // up, then down on the rim
			P->Pull = Slam01;
			P->Lean = B.Lean / 25.f;
			P->FaceDir = -Dir;
			P->HandPin = C + Dir * (ER + 25.f) + FVector(0.f, 0.f, 12.f - B.Dip * PeekH * 0.85f + B.Rise * 30.f);
			P->bLookAt = true;
			FVector Look = CamL;
			if (!bHumanAim && Act && G->Phase == EZoogiPhase::Rolling) Look = Act->BallLocation(); // eyes on a CPU / rolling shot
			P->LookAt = Look;
			B.Rise = FMath::FInterpTo(B.Rise, B.RiseTo, Dt, 0.6f);
			// eyes
			B.Eye = FMath::FInterpTo(B.Eye, B.EyeBase, Dt, 1.2f);
			float K = B.Eye * (0.85f + 0.15f * FMath::Sin(B.T * 1.7f));
			if (B.Flicker > 0.f) { B.Flicker -= Dt; K *= (FMath::Fmod(B.T * 14.f, 1.f) < 0.5f) ? 0.15f : 1.f; }
			SetEyes(K);
			B.Cool = FMath::Max(0.f, B.Cool - Dt);
			// the boss cam: from the court, low, looking up at its face
			const FVector HeadP = P->Body ? P->Body->GetBoneLocationByName(TEXT("head"), EBoneSpaces::WorldSpace) : P->GetActorLocation();
			if (B.CamWant > 0.f && !B.Cam.IsValid() && Pc && B.Dip < 0.2f)
			{
				AActor* Vt = Pc->GetViewTarget();
				const AZoogiPlayerController* Zpc = Cast<AZoogiPlayerController>(Pc);
				if (Vt && (Zpc ? Vt == Zpc->MainCam() : !Cast<ACameraActor>(Vt)))
				{
					const FVector CamP = C + Dir * (ER * 0.15f) + FVector(-Dir.Y, Dir.X, 0.f) * (ER * 0.3f) + FVector(0.f, 0.f, 230.f);
					FActorSpawnParameters SPc; SPc.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (ACameraActor* Cm = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((HeadP - CamP).Rotation(), CamP), SPc))
					{
						Cm->GetCameraComponent()->SetFieldOfView(60.f); Cm->GetCameraComponent()->bConstrainAspectRatio = false;
						B.Cam = Cm; B.CamPrev = Vt; B.CamLeft = B.CamWant; B.CamT = 0.f; B.CamWait = 0.f;
						Pc->SetViewTargetWithBlend(Cm, 0.35f, VTBlend_Cubic);
						UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS cam %.1f s (peek)"), B.CamWant);
					}
				}
				if (B.Cam.IsValid()) B.CamWant = 0.f;
				else if ((B.CamWait += Dt) > 15.f) { B.CamWant = 0.f; B.CamWait = 0.f; }
			}
			if (ACameraActor* Cm = B.Cam.Get())
			{
				B.CamT += Dt; B.CamLeft -= Dt;
				if (Pc && Pc->GetViewTarget() != Cm && B.CamT > 0.4f && B.CamT < 0.9f) Pc->SetViewTarget(Cm);
				Cm->SetActorRotation(FMath::RInterpTo(Cm->GetActorRotation(), (HeadP - Cm->GetActorLocation()).Rotation(), Dt, 6.f));
				Cm->GetCameraComponent()->SetFieldOfView(FMath::Lerp(60.f, 42.f, FMath::Clamp(B.CamT / 2.5f, 0.f, 1.f)));
				if (B.CamLeft <= 0.f) EndBossCam(0.5f);
			}
			if ((B.PeekLog -= Dt) <= 0.f)
			{
				B.PeekLog = 3.f;
				FVector2D Sp(-1.f, -1.f); int32 Vx = 0, Vy = 0; if (Pc) { Pc->GetViewportSize(Vx, Vy); Pc->ProjectWorldLocationToScreen(HeadP, Sp, true); }
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peek yaw %.0f (want %.0f) dip %.2f head (%.0f,%.0f,%.0f) on screen (%.2f, %.2f) human aim %d"), B.PeekAng, B.WantPeek, B.Dip, HeadP.X, HeadP.Y, HeadP.Z, Vx ? Sp.X / Vx : -1.f, Vy ? Sp.Y / Vy : -1.f, bHumanAim ? 1 : 0);
			}
			return true;
		}

		bool Step(float Dt)
		{''')
ed1(B, '''			if (!G || !A || !IsValid(A)) { Tick.Reset(); B = FBoss(); return false; }
			Dt = FMath::Min(Dt, 0.1f);
			B.T += Dt;''', '''			if (!G || !A || !IsValid(A)) { Tick.Reset(); B = FBoss(); return false; }
			Dt = FMath::Min(Dt, 0.1f);
			B.T += Dt;
			if (B.Pup.IsValid()) return PeekStep(G, Dt); // b142''')
ed1(B, '''			USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex_03.SK_Mecha_Aurex_03"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: no Aurex mesh")); return; }''', '''			USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex_03.SK_Mecha_Aurex_03"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: no Aurex mesh")); return; }
			if (!Titan())
			{	// b142: the peek boss on the rim
				float Ang0 = 0.f;
				if (APlayerController* Pc0 = W->GetFirstPlayerController()) if (Pc0->PlayerCameraManager) Ang0 = Pc0->PlayerCameraManager->GetCameraRotation().Yaw;
				AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnBoss(W, SM, Centre(G) + FVector(0.f, 0.f, -3000.f), Ang0 + 180.f, PeekH);
				if (!P) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: peek puppet failed")); return; }
				if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, P);
					MID->SetVectorParameterValue(TEXT("Color_Zone_R"), FLinearColor(0.22f, 0.07f, 0.42f));
					MID->SetVectorParameterValue(TEXT("Color_Zone_G"), FLinearColor(0.04f, 0.02f, 0.08f));
					MID->SetVectorParameterValue(TEXT("Color_Zone_B"), FLinearColor(0.45f, 0.12f, 0.7f));
					MID->SetVectorParameterValue(TEXT("Emissive_Color"), EyeRed);
					for (int32 m = 0; m < P->Body->GetNumMaterials(); ++m) P->Body->SetMaterial(m, MID);
					B.MID = MID;
				}
				B.A = P; B.Pup = P; B.G = G; B.H = PeekH;
				B.PeekAng = B.WantPeek = Ang0; B.Dip = 1.f; B.bMoving = false; B.PeekTurn = -1;
				B.CamWant = 3.f; B.Eye = 0.f; B.EyeBase = 2.5f; B.Rise = 0.f; B.RiseTo = 0.f;
				if (!Tick.IsValid()) Tick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Step));
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peeks: %s, %.0f m tall, hands on the rim at yaw %.0f"), *SM->GetName(), PeekH / 100.f, Ang0);
				return;
			}''')
ed1(B, '''		if (bFinal && !B.bFinal)
		{	// the final round: it rises higher and roars, the eyes stay hot
			B.bFinal = true; B.RiseTo = 3.f; B.EyeBase = 4.5f; B.Eye = 9.f;''', '''		if (bFinal && !B.bFinal)
		{	// the final round: it rises higher and roars, the eyes stay hot
			B.bFinal = true; B.RiseTo = 3.f; B.EyeBase = 4.5f; B.Eye = 9.f;
			if (B.Pup.IsValid()) B.RoarLeft = 2.4f; // b142: rears up, arms high''')
ed1(B, '''			B.Eye = 10.f; B.LeanTo = 6.f;
			Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.7f);''', '''			B.Eye = 10.f; B.LeanTo = 6.f;
			if (B.Pup.IsValid()) { B.Slam = 1.f; B.Cool = 2.5f; } // b142: slams the rim
			Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.7f);''')
print('b142e ok')
