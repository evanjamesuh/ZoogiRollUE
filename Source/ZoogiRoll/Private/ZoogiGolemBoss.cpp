// b177g: the Stone Golem MAGMA TITAN, full body + hammer - see ZoogiGolemBoss.h
#include "ZoogiGolemBoss.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiHaptics.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiGolem
{
	namespace
	{
		struct FGo
		{
			TWeakObjectPtr<AActor> A; TWeakObjectPtr<USkeletalMeshComponent> Mc; TWeakObjectPtr<AZoogiGameMode> G;
			float H = 1400.f, T = 0.f, Yaw = 0.f, MeshOff = 0.f, OneLeft = 0.f, IdleT = 0.f, SwAng = 0.f; bool bOriented = false, bSweep = false, bDead = false;
			float WAng = 0.f, WTarget = 0.f, WWait = 3.f, SmashAt = -1.f; bool bWalking = false, bAngInit = false; // b177l: wanders the rim, smashing random spots
			FString Base, Cur; FVector Pos = FVector::ZeroVector, UpPos = FVector::ZeroVector; // UpPos = its spot standing on the rim (the cam frames that)
		};
		FGo D;
		UAnimSequence* Anim(const FString& N)
		{
			static TMap<FString, TWeakObjectPtr<UAnimSequence>> Cache;
			if (TWeakObjectPtr<UAnimSequence>* C = Cache.Find(N)) if (C->IsValid()) return C->Get();
			UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/StoneGolem/Animations/AS_StoneGolem_%s.AS_StoneGolem_%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!S) UE_LOG(LogZoogi, Warning, TEXT("GOLEM: no anim %s"), *N);
			Cache.Add(N, S); return S;
		}
		void Loop(const TCHAR* N, float Rate = 1.f)
		{
			D.Base = N; D.OneLeft = 0.f;
			USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc || D.Cur == N) return;
			if (UAnimSequence* S = Anim(N)) { Mc->PlayAnimation(S, true); Mc->SetPlayRate(Rate); D.Cur = N; }
		}
		void Once(const TCHAR* N, float Rate = 1.f)
		{
			USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc) return;
			if (UAnimSequence* S = Anim(N)) { Mc->PlayAnimation(S, false); Mc->SetPlayRate(Rate); D.Cur = N; D.OneLeft = S->GetPlayLength() / FMath::Max(0.1f, Rate) - 0.05f; }
		}
		FVector Dir(float Ang) { return FRotator(0.f, Ang, 0.f).Vector(); }
	}

	bool Wanted(const AZoogiGameMode* G)
	{
		static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMagmaBoss"));
		return !bOld && G && G->Arena && G->Arena->GetTheme() == EZoogiArena::Volcanic;
	}
	bool Active() { return D.A.IsValid() && D.Mc.IsValid(); }
	void Clear() { if (AActor* A = D.A.Get()) A->Destroy(); D = FGo(); }

	bool Spawn(AZoogiGameMode* G)
	{
		Clear();
		UWorld* W = G ? G->GetWorld() : nullptr; if (!W || !G->Arena) return false;
		USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/StoneGolem/Mesh/SKM_StoneGolem.SKM_StoneGolem"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SM) return false;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* A = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(G->Arena->GetActorLocation() - FVector(0.f, 0.f, 5000.f)), SP);
		if (!A) return false;
		USkeletalMeshComponent* Mc = A->GetSkeletalMeshComponent();
		Mc->SetMobility(EComponentMobility::Movable); Mc->SetSkeletalMeshAsset(SM);
		Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mc->SetCastShadow(true); Mc->SetBoundsScale(3.f);
		Mc->bEnableUpdateRateOptimizations = false; Mc->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mc->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		struct FW { const TCHAR* Mesh; const TCHAR* Sock; const TCHAR* Bone; };
		static const FW Ws[] = { { TEXT("SM_StoneGolem_Hammer"), TEXT("weapon_rSocket"), TEXT("weapon_r") }, { TEXT("SM_StoneGolem_Shield"), TEXT("weapon_lSocket"), TEXT("weapon_l") } };
		for (const FW& Wp : Ws)
			if (UStaticMesh* Wm = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/StoneGolem/Mesh/%s.%s"), Wp.Mesh, Wp.Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UStaticMeshComponent* Wc = NewObject<UStaticMeshComponent>(A);
				Wc->SetStaticMesh(Wm); Wc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Wc->SetBoundsScale(3.f);
				Wc->SetupAttachment(Mc, Mc->DoesSocketExist(FName(Wp.Sock)) ? FName(Wp.Sock) : FName(Wp.Bone)); Wc->RegisterComponent();
			}
		D.A = A; D.Mc = Mc; D.G = G;
		const float Hm = FMath::Max(50.f, SM->GetBounds().BoxExtent.Z * 2.f);
		static float Want = -1.f; if (Want < 0.f) { Want = 0.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGolemSize="), Want); }
		D.H = Want > 0.f ? Want : 900.f; // b177n (Evan: "no taller than the flag"): was 6.2 m; b194 (Evan: "make the volcano titan bigger"): 9 m
		A->SetActorScale3D(FVector(D.H / Hm));
		Loop(TEXT("idle1"), 0.8f);
		UE_LOG(LogZoogi, Log, TEXT("GOLEM spawned: Stone Golem full body, %.0f cm model -> %.0f m tall, hammer + shield"), Hm, D.H / 100.f);
		return true;
	}

	void Tick(AZoogiGameMode* G, float Dt, const FVector& Ctr, float RimR, float RimZ, float Ang, float Dip, bool bDefeated)
	{
		AActor* A = D.A.Get(); USkeletalMeshComponent* Mc = D.Mc.Get(); if (!A || !Mc || !G) return;
		D.T += Dt;
		if (!D.bOriented)
		{	// b177h: a UE-mannequin rig faces +Y in mesh space (the hand-based guess read 137 deg); -ZoogiGolemYaw= overrides
			float Yo = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiGolemYaw="), Yo);
			D.MeshOff = Yo; D.bOriented = true;
		}
		if (D.OneLeft > 0.f) { D.OneLeft -= Dt; if (D.OneLeft <= 0.f) { const FString B = D.Base; D.Cur.Reset(); Loop(*B, 0.8f); } }
		if (bDefeated && !D.bDead) { D.bDead = true; Once(TEXT("death1"), 0.8f); D.Base = TEXT("death1"); }
		// b177l (Evan: "keep the volcano golem in view ... walks around the arena randomly hitting spots"): always up on the rim (no
		// dipping out of sight), wandering the rim inside the camera's view (+/-50 deg of the peek angle), hammering random spots
		if (!D.bAngInit) { D.bAngInit = true; D.WAng = D.WTarget = Ang; }
		float WantYaw = 0.f;
		if (D.bSweep) { D.WAng = FMath::FInterpTo(D.WAng, D.WAng + FMath::FindDeltaAngleDegrees(D.WAng, D.SwAng), Dt, 6.f); D.bWalking = false; }
		else if (!D.bDead)
		{
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(D.WTarget, Ang)) > 55.f) { D.WTarget = Ang + FMath::FRandRange(-30.f, 30.f); D.WWait = 0.f; } // the view turned: come back into it
			const float Dd = FMath::FindDeltaAngleDegrees(D.WAng, D.WTarget);
			if (FMath::Abs(Dd) > 1.5f && D.OneLeft <= 0.f)
			{	// walks along the rim (~2.4 m/s)
				const float Step = FMath::RadiansToDegrees(240.f / FMath::Max(500.f, RimR)) * Dt;
				D.WAng += FMath::Clamp(Dd, -Step, Step);
				if (!D.bWalking) { D.bWalking = true; Loop(TEXT("walkF"), 0.9f); }
			}
			else
			{
				if (D.bWalking) { D.bWalking = false; Loop(TEXT("idle1"), 0.8f); D.WWait = FMath::FRandRange(1.f, 2.5f); }
				if ((D.WWait -= Dt) <= 0.f && D.OneLeft <= 0.f)
				{	// a hammer smash on a spot in front of it, then off to the next spot
					Once(TEXT("attack1"), 0.85f); D.Base = TEXT("idle1"); D.SmashAt = 0.55f;
					D.WTarget = Ang + FMath::FRandRange(-50.f, 50.f); D.WWait = FMath::FRandRange(4.f, 7.f);
				}
			}
			if (D.SmashAt > 0.f && (D.SmashAt -= Dt) <= 0.f)
			{	// visual only: dust, rocks, a small shake - nothing on the court is pushed
				UWorld* W = G->GetWorld();
				const FVector Hit = FVector(Ctr.X, Ctr.Y, RimZ + 10.f) + Dir(D.WAng) * (RimR - D.H * 0.25f);
				AZoogiFX::Stylized(W, TEXT("Mid_Explosion_v2"), Hit, 0.7f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Red"), Hit, FRotator::ZeroRotator, 0.9f);
				ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.8f, 0.6f);
				ZoogiHaptics::Kick(0.25f, W, TEXT("golem_smash"));
			}
		}
		const float A0 = D.WAng;
		// stands on the rim top, full body in view, a little in front of the wall
		const FVector Want = FVector(Ctr.X, Ctr.Y, RimZ) + Dir(A0) * (RimR + D.H * 0.12f);
		D.Pos = D.T < 0.2f ? Want : FMath::VInterpTo(D.Pos, Want, Dt, 6.f);
		D.UpPos = Want;
		WantYaw = A0 + 180.f; // faces the court
		if (D.bWalking) WantYaw = A0 + (FMath::FindDeltaAngleDegrees(D.WAng, D.WTarget) > 0.f ? 90.f : -90.f); // walks along the rim
		else if (!D.bSweep)
			if (APlayerController* Pc = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr) if (Pc->PlayerCameraManager)
			{	// front, not side: turns its face to the viewer (within 70 deg of the court)
				const FVector Cl = Pc->PlayerCameraManager->GetCameraLocation();
				WantYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(WantYaw, (FVector(Cl.X, Cl.Y, 0.f) - FVector(D.Pos.X, D.Pos.Y, 0.f)).Rotation().Yaw), -70.f, 70.f);
			}
		if (!D.bDead && !D.bWalking && D.OneLeft <= 0.f && (D.IdleT += Dt) > 9.f) { D.IdleT = 0.f; static int32 Ii = 0; const TCHAR* Ids[] = { TEXT("idle2"), TEXT("idle3"), TEXT("idle4") }; Once(Ids[(Ii++) % 3], 0.8f); }
		D.Yaw = FMath::FixedTurn(D.Yaw, WantYaw, 120.f * Dt);
		A->SetActorLocationAndRotation(D.Pos, FRotator(0.f, D.Yaw + D.MeshOff, 0.f));
		A->SetActorHiddenInGame(false); (void)Dip;
	}

	void Sweep(bool bOn, float AngDeg)
	{
		if (!Active()) return;
		if (bOn && !D.bSweep) { D.SwAng = AngDeg + 35.f; Once(TEXT("attack2"), 0.75f); D.Base = TEXT("idle1"); UE_LOG(LogZoogi, Log, TEXT("GOLEM HAMMER SWEEP at yaw %.0f"), D.SwAng); } // stands mid-slice and swings
		D.bSweep = bOn;
	}
	void GoTo(float AngDeg) { if (!Active()) return; D.WAng = D.WTarget = AngDeg; D.bWalking = false; D.WWait = 5.f; }
	void Event(const TCHAR* What)
	{
		if (!Active() || D.bDead) return;
		const FString E(What);
		if (E == TEXT("Slam")) { Once(TEXT("attack1"), 0.8f); D.bWalking = false; D.SmashAt = -1.f; }       // the overhead hammer smash
		else if (E == TEXT("Roar") || E == TEXT("Taunt")) Once(TEXT("attack4"), 0.7f);
		else if (E == TEXT("Threat")) Once(TEXT("attack3"), 0.8f);
		else if (E == TEXT("Hit")) Once(TEXT("gethit1"));
		D.Base = TEXT("idle1");
	}
	FVector HeadLoc()
	{
		USkeletalMeshComponent* Mc = D.Mc.Get(); if (!Mc) return FVector::ZeroVector;
		return Mc->GetBoneIndex(FName(TEXT("head"))) != INDEX_NONE ? Mc->GetBoneLocation(FName(TEXT("head"))) : D.Pos + FVector(0.f, 0.f, D.H * 0.85f);
	}
	bool CamSpot(FVector& OutPos, FVector& OutLook)
	{	// in front of it, far enough back for the whole body, looking at its chest
		if (!Active()) return false;
		OutLook = D.UpPos + FVector(0.f, 0.f, D.H * 0.5f); // b177h: framed where it stands, never where it is mid-rise (was under the ground = black)
		OutPos = D.UpPos + Dir(D.Yaw) * (D.H * 2.6f) + FVector(0.f, 0.f, D.H * 0.45f); // b177l: the whole body in frame
		return true;
	}
}
