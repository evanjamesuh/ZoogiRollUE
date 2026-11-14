// b234: SHOOTER modes - see ZoogiShooter.h
#include "ZoogiShooter.h"
#include "Components/SphereComponent.h" // b236
#include "ZoogiVoyager.h"
#include "ZoogiRoyale.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiMorph.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiShooter
{
	namespace
	{
		int32 GKind = -1;
		struct FVis { TWeakObjectPtr<ASkeletalMeshActor> A; FString Cur; float Rate = 1.f; float ActLeft = 0.f; float LastHurt = -100.f; float Yaw = 0.f; float Scale = 1.f; };
		struct FState
		{
			TWeakObjectPtr<AZoogiGameMode> G;
			int32 Wave = 1, MaxWave = 5, TeamKO[2] = { 0, 0 }, TeamGoal = 20;
			bool bOver = false;
			int32 OrbsSeen = 0, Crystals = 0; float BallLeft = 0.f; int32 Ammo = 30; float ReloadUntil = -1.f; // b237 magazine
 // b236: the human's crystals / ball dash
			TArray<FVis> Vis;
		};
		FState S;
		const TCHAR* KindName(int32 K) { return K == 1 ? TEXT("HOLD THE LINE") : K == 2 ? TEXT("SHOOTER ROYALE") : K == 3 ? TEXT("TEAM DEATHMATCH") : TEXT(""); }
		bool Bot(const AZoogiGameMode* G, int32 i) { return i != G->Royale.HumanIndex; }
		void Clear()
		{
			for (FVis& V : S.Vis) if (AActor* A = V.A.Get()) A->Destroy();
			S = FState();
		}
		void Play(FVis& V, const TCHAR* Rel, bool bLoop, float Rate)
		{
			ASkeletalMeshActor* A = V.A.Get(); if (!A) return;
			if (V.Cur != Rel) { if (UAnimSequence* Sq = ZoogiVoyager::Anim(Rel)) { A->GetSkeletalMeshComponent()->PlayAnimation(Sq, bLoop); V.Cur = Rel; } }
			if (!FMath::IsNearlyEqual(V.Rate, Rate, 0.02f)) { A->GetSkeletalMeshComponent()->SetPlayRate(Rate); V.Rate = Rate; }
		}
		void Act(FVis& V, const TCHAR* Rel, float Secs, float Rate)
		{
			V.Cur.Reset(); Play(V, Rel, false, Rate); V.ActLeft = Secs;
		}
		void ForceRobot(AZoogiGameMode* G, int32 i)
		{
			if (!G->Royale.P.IsValidIndex(i) || !G->Balls.IsValidIndex(i)) return;
			FZoogiRoyalePlayer& R = G->Royale.P[i];
			AZoogiBall* B = G->Balls[i];
			if (!B || R.bRobot) return;
			R.bRobot = true; R.MorphT = 10.f; R.MorphCD = 0.f;
			B->SetMassScale(FZoogiRoyaleTune::Get().RobotMassMul);
			if (B->Morph) { B->Morph->SetFacingYaw(R.FacingYaw); B->Morph->SetRobot(true, true); }
		}
		void Announce(AZoogiGameMode* G, const FString& Big, const FLinearColor& Col)
		{
			G->BigText = Big; G->BigColor = Col; G->BigAge = 0.f;
		}
	}

	int32 Kind()
	{
		if (GKind < 0) { GKind = 0; FParse::Value(FCommandLine::Get(), TEXT("ZoogiShooter="), GKind); GKind = FMath::Clamp(GKind, 0, 3); }
		return GKind;
	}
	void SetKind(int32 K) { Kind(); GKind = FMath::Clamp(K, 0, 3); UE_LOG(LogZoogi, Log, TEXT("SHOOTER mode: %s"), GKind ? KindName(GKind) : TEXT("off")); }
	bool On(const AZoogiGameMode* G) { return G && G->bRoyaleMode && !G->bRoyaleTraining && Kind() > 0; }

	int32 Team(const AZoogiGameMode* G, int32 Idx)
	{
		if (!On(G)) return Idx;
		if (Kind() == 1) return Idx == G->Royale.HumanIndex ? 0 : 1;
		if (Kind() == 3) return Idx < 2 ? 0 : 1;
		return Idx;
	}
	bool Allies(const AZoogiGameMode* G, int32 A, int32 B)
	{
		if (!On(G) || A == B || A < 0 || B < 0) return false;
		return Kind() != 2 && Team(G, A) == Team(G, B);
	}
	bool KeepLives(const AZoogiGameMode* G) { return On(G) && Kind() == 3; }

	void OnStart(AZoogiGameMode* G)
	{
		Clear();
		if (!On(G)) return;
		S.G = G;
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiShooterWaves="), S.MaxWave);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiTdmGoal="), S.TeamGoal);
		FZoogiRoyale& R = G->Royale;
		for (int32 i = 0; i < R.P.Num(); ++i)
		{
			if (Kind() == 1) R.P[i].Lives = Bot(G, i) ? 1 : 5; /* b236: 3 went too fast */       // a wave = 3 soldiers with one life each; you have 3
			else if (Kind() == 3) R.P[i].Lives = 99;
		}
		// the Voyager soldiers for the bots (the human is ZoogiRoyaleHero's Voyager hero)
		S.Vis.SetNum(G->Balls.Num());
		UWorld* W = G->GetWorld();
		for (int32 i = 0; i < G->Balls.Num(); ++i)
		{
			if (!Bot(G, i) || !G->Balls[i]) continue;
			// HOLD THE LINE: troopers / shadow ops / wraiths; TDM: your teammate is Murdock, the enemies Shadow Ops + Trooper
			const int32 Pick = Kind() == 1 ? (i == 1 ? 5 : i == 2 ? 1 : 3) : Kind() == 3 ? (i == 1 ? 4 : i == 2 ? 1 : 5) : (i + 2);
			USkeletalMesh* M = ZoogiVoyager::Character(TEXT("-"), Pick);
			if (!M) continue;
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASkeletalMeshActor* A = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(G->Balls[i]->BallLocation()), Sp);
			if (!A) continue;
			USkeletalMeshComponent* Sk = A->GetSkeletalMeshComponent();
			Sk->SetMobility(EComponentMobility::Movable); Sk->SetSkeletalMeshAsset(M); Sk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sk->SetAnimationMode(EAnimationMode::AnimationSingleNode); Sk->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			const float Hm = FMath::Max(10.f, (float)M->GetBounds().BoxExtent.Z * 2.f);
			S.Vis[i].Scale = 185.f / Hm;
			A->SetActorScale3D(FVector(S.Vis[i].Scale));
			ZoogiVoyager::Dress(Sk, G->Balls[i], Kind() == 1); // b235: Zoogi colours + head; HOLD THE LINE enemies are Void Hollows
			if (USkeletalMesh* Gm = ZoogiVoyager::Gun(Kind() == 1 ? TEXT("AssaultRifle") : TEXT("HeavyMachineGun")))
			{
				USkeletalMeshComponent* Gc = NewObject<USkeletalMeshComponent>(A);
				Gc->SetSkeletalMeshAsset(Gm); Gc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gc->RegisterComponent();
				Gc->AttachToComponent(Sk, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_rSocket"));
			}
			S.Vis[i].A = A; S.Vis[i].Yaw = G->Royale.P.IsValidIndex(i) ? G->Royale.P[i].FacingYaw : 0.f;
		}
		Announce(G, KindName(Kind()), FLinearColor(1.f, 0.78f, 0.2f));
		UE_LOG(LogZoogi, Log, TEXT("SHOOTER %s: %d soldiers%s"), KindName(Kind()), G->Balls.Num(), Kind() == 1 ? *FString::Printf(TEXT(", %d waves"), S.MaxWave) : Kind() == 3 ? *FString::Printf(TEXT(", first team to %d"), S.TeamGoal) : TEXT(""));
	}

	void OnFight(AZoogiGameMode* G)
	{
		if (!On(G)) return;
		for (int32 i = 0; i < G->Balls.Num(); ++i) ForceRobot(G, i);
		Announce(G, Kind() == 1 ? FString::Printf(TEXT("WAVE %d"), S.Wave) : Kind() == 3 ? FString(TEXT("TEAM DEATHMATCH\nFIRST TO 20")) : FString(TEXT("SHOOT!")), FLinearColor(1.f, 0.42f, 0.2f));
	}

	void OnRespawn(AZoogiGameMode* G, int32 Idx)
	{
		if (!On(G)) return;
		ForceRobot(G, Idx);
		if (Kind() == 1 && Bot(G, Idx) && G->Royale.P.IsValidIndex(Idx))
		{	// tougher every wave; wave 5 (the boss wave) doubles them
			const int32 Base = FZoogiRoyaleTune::Get().MaxHP;
			const float Mul = (1.f + 0.25f * (S.Wave - 1)) * (S.Wave >= S.MaxWave ? 1.6f : 1.f);
			G->Royale.P[Idx].HP = FMath::RoundToInt(Base * Mul);
		}
	}

	void OnLoseLife(AZoogiGameMode* G, int32 Victim, int32 CreditedTo)
	{
		if (!On(G) || Kind() != 3) return;
		const int32 Tv = Team(G, Victim);
		const int32 Tk = CreditedTo >= 0 && !Allies(G, Victim, CreditedTo) ? Team(G, CreditedTo) : 1 - Tv; // a fall scores for the other team
		S.TeamKO[Tk]++;
		UE_LOG(LogZoogi, Log, TEXT("SHOOTER TDM: BLUE %d - %d RED"), S.TeamKO[0], S.TeamKO[1]); // the HUD line shows the score
		if (S.TeamKO[Tk] >= S.TeamGoal && !S.bOver)
		{
			S.bOver = true;
			for (int32 i = 0; i < G->Royale.P.Num(); ++i) if (Team(G, i) != Tk) { G->Royale.P[i].Place = 3; G->Royale.P[i].Lives = 0; }
			G->RoyaleFinish();
			const bool bWe = Team(G, G->Royale.HumanIndex) == Tk;
			if (bWe) { G->Royale.Winner = G->Royale.HumanIndex; G->WinnerIndex = G->Royale.HumanIndex; } // b235: your team's win is your win (story)
			Announce(G, FString::Printf(TEXT("%s\n%d - %d"), Tk == 0 ? TEXT("BLUE TEAM WINS") : TEXT("RED TEAM WINS"), S.TeamKO[0], S.TeamKO[1]), bWe ? FLinearColor(0.3f, 0.7f, 1.f) : FLinearColor(1.f, 0.35f, 0.3f));
			UE_LOG(LogZoogi, Log, TEXT("SHOOTER TDM over: %s %d - %d"), Tk == 0 ? TEXT("BLUE") : TEXT("RED"), S.TeamKO[0], S.TeamKO[1]);
		}
	}

	bool OnEliminated(AZoogiGameMode* G, int32 Victim)
	{
		if (!On(G) || Kind() != 1) return false;
		FZoogiRoyale& R = G->Royale;
		if (Victim == R.HumanIndex)
		{	// you are out: the match ends on the wave you reached
			for (int32 i = 0; i < R.P.Num(); ++i) if (Bot(G, i)) { R.P[i].Place = 0; }
			G->RoyaleFinish();
			Announce(G, FString::Printf(TEXT("THE LINE FELL\n%d WAVE%s HELD"), S.Wave - 1, S.Wave - 1 == 1 ? TEXT("") : TEXT("S")), FLinearColor(1.f, 0.35f, 0.3f));
			UE_LOG(LogZoogi, Log, TEXT("SHOOTER HOLD THE LINE over on wave %d"), S.Wave);
			return true;
		}
		bool bAnyBot = false;
		for (int32 i = 0; i < R.P.Num(); ++i) if (Bot(G, i) && R.P[i].IsIn()) bAnyBot = true;
		if (bAnyBot) return true; // the wave goes on
		if (S.Wave >= S.MaxWave)
		{	// all waves held: you win
			G->RoyaleFinish();
			Announce(G, FString::Printf(TEXT("YOU HELD THE LINE!\n%d WAVES"), S.MaxWave), FLinearColor(1.f, 0.82f, 0.25f));
			UE_LOG(LogZoogi, Log, TEXT("SHOOTER HOLD THE LINE won (%d waves)"), S.MaxWave);
			return true;
		}
		++S.Wave;
		for (int32 i = 0; i < R.P.Num(); ++i)
			if (Bot(G, i)) { R.P[i].Place = 0; R.P[i].Lives = 1; R.P[i].RespawnIn = 2.5f + 0.6f * i; R.P[i].KOs = 0; }
		if (R.P.IsValidIndex(R.HumanIndex)) R.P[R.HumanIndex].HP = FZoogiRoyaleTune::Get().MaxHP; // a breather: full health between waves
		Announce(G, S.Wave >= S.MaxWave ? FString::Printf(TEXT("WAVE %d\nBOSS WAVE"), S.Wave) : FString::Printf(TEXT("WAVE %d"), S.Wave), FLinearColor(1.f, 0.6f, 0.15f));
		ZoogiUtil::PlaySfx(G, TEXT("EpicRiser"), 0.7f, 1.f);
		UE_LOG(LogZoogi, Log, TEXT("SHOOTER HOLD THE LINE: wave %d"), S.Wave);
		return true;
	}

	bool TryShot(AZoogiGameMode* G)
	{
		if (!On(G)) return true;
		const float Now = G->GetWorld()->GetTimeSeconds();
		if (S.ReloadUntil > 0.f) { if (Now < S.ReloadUntil) return false; S.ReloadUntil = -1.f; S.Ammo = 30; }
		if (--S.Ammo <= 0)
		{
			S.ReloadUntil = Now + 1.5f; ZoogiUtil::PlaySfx(G, TEXT("Arm"), 0.6f, 0.8f);
			const int32 Hi = G->Royale.HumanIndex;
			if (G->Balls.IsValidIndex(Hi) && G->Balls[Hi]) G->PublicPopup(TEXT("RELOADING"), G->Balls[Hi]->BallLocation() + FVector(0, 0, 170.f), FLinearColor(1.f, 0.82f, 0.3f));
		}
		return true;
	}
	bool AllowTransform(AZoogiGameMode* G, int32 Idx, bool bToBall)
	{
		if (!On(G)) return true;
		if (Idx != G->Royale.HumanIndex) return !bToBall; // bots stay on foot
		if (!bToBall) { S.BallLeft = 0.f; return true; }  // back on foot early
		static int32 Need = -1; if (Need < 0) { Need = 3; FParse::Value(FCommandLine::Get(), TEXT("ZoogiShooterCrystals="), Need); }
		if (S.Crystals < Need)
		{
			if (G->Balls.IsValidIndex(Idx) && G->Balls[Idx]) G->PublicPopup(FString::Printf(TEXT("COLLECT %d CRYSTALS TO ROLL"), Need - S.Crystals), G->Balls[Idx]->BallLocation() + FVector(0, 0, 170.f), FLinearColor(0.5f, 0.9f, 1.f));
			return false;
		}
		S.BallLeft = 2.f * S.Crystals; S.Crystals = 0;
		UE_LOG(LogZoogi, Log, TEXT("SHOOTER ball dash: %.0f s"), S.BallLeft);
		return true;
	}

	void Tick(AZoogiGameMode* G, float Dt)
	{
		if (S.G.Get() != G || !On(G)) return;
		{	// b236 (Evan: "collect crystals and transform into a ball to move fast"): every orb you pick up is a crystal; the ball dash runs out
			const int32 Hi = G->Royale.HumanIndex;
			if (G->Royale.P.IsValidIndex(Hi) && G->Balls.IsValidIndex(Hi) && G->Balls[Hi])
			{
				FZoogiRoyalePlayer& Rh = G->Royale.P[Hi];
				if (Rh.Orbs > S.OrbsSeen) { S.Crystals = FMath::Min(9, S.Crystals + Rh.Orbs - S.OrbsSeen); }
				S.OrbsSeen = Rh.Orbs;
				static const bool bDashTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShooterDashTest")); // preview: a crystal every 3 s, dash when ready
				static float TestT = 0.f;
				if (bDashTest && G->Royale.Step == EZoogiRoyaleStep::Fight) { TestT += Dt; if (TestT > 3.f) { TestT = 0.f; ++S.Crystals; } if (Rh.bRobot && S.Crystals >= 3) G->RoyaleTransform(Hi, TEXT("dash test")); }
				if (!Rh.bRobot && Rh.RespawnIn < 0.f && G->Royale.Step == EZoogiRoyaleStep::Fight)
				{
					S.BallLeft -= Dt;
					const FVector2D Mv = Rh.Move.GetSafeNormal();
					if (!Mv.IsNearlyZero() && G->Balls[Hi]->Sphere) G->Balls[Hi]->Sphere->AddForce(FVector(Mv.X, Mv.Y, 0.f) * 900.f, NAME_None, true); // the ball is the fast form
					if (S.BallLeft <= 0.f) { ForceRobot(G, Hi); AZoogiFX::Spawn(G->GetWorld(), G->Balls[Hi]->BallLocation(), FLinearColor(0.5f, 0.9f, 1.f), 160.f, 0.3f, true, 8.f); }
				}
			}
		}
		const float Now = G->GetWorld()->GetTimeSeconds();
		for (int32 i = 0; i < S.Vis.Num() && i < G->Balls.Num() && i < G->Royale.P.Num(); ++i)
		{
			FVis& V = S.Vis[i];
			ASkeletalMeshActor* A = V.A.Get(); AZoogiBall* B = G->Balls[i];
			if (!A || !B) continue;
			const FZoogiRoyalePlayer& R = G->Royale.P[i];
			if (B->Morph && B->Morph->Mesh) B->Morph->Mesh->SetHiddenInGame(true); // the old robot / ball is not drawn
			if (B->Visual) B->Visual->SetHiddenInGame(true);
			const bool bShow = !B->bOut && R.IsIn() && R.RespawnIn < 0.f && (R.ShieldLeft <= 0.f || FMath::Fmod(Now, 0.2f) < 0.12f);
			A->SetActorHiddenInGame(!bShow);
			if (!bShow) continue;
			const FVector L = B->BallLocation(), Vv = B->GetBallVelocity();
			const float Sp = FVector2D(Vv).Size();
			V.Yaw = FMath::FixedTurn(V.Yaw, R.FacingYaw, 720.f * Dt);
			A->SetActorLocationAndRotation(L - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), FRotator(0.f, V.Yaw - 90.f, 0.f));
			if (R.HurtAt > V.LastHurt + 0.01f && Now - R.HurtAt < 0.2f) { V.LastHurt = R.HurtAt; if (V.ActLeft <= 0.f) Act(V, TEXT("Soldier/MotionCC0/Anim_Hit_Chest"), 0.35f, 1.3f); }
			if (V.ActLeft > 0.f) { V.ActLeft -= Dt; continue; }
			const bool bAir = Vv.Z > 160.f || Vv.Z < -260.f;
			if (bAir) Play(V, TEXT("Soldier/ThirdPerson/Anim_ThirdPersonJump_Loop"), true, 1.f);
			else if (Sp < 60.f) Play(V, TEXT("Soldier/Anim_Idle_Rifle_Hip"), true, 1.f);
			else
			{	// the jog clip that matches the move relative to the facing
				const float Rel = FMath::FindDeltaAngleDegrees(V.Yaw, FMath::RadiansToDegrees(FMath::Atan2(Vv.Y, Vv.X)));
				const TCHAR* Cl = FMath::Abs(Rel) < 45.f ? TEXT("Soldier/Anim_Jog_Fwd_Rifle") : FMath::Abs(Rel) > 135.f ? TEXT("Soldier/Anim_Jog_Bwd_Rifle") : Rel > 0.f ? TEXT("Soldier/Anim_Jog_Rt_Rifle") : TEXT("Soldier/Anim_Jog_Lt_Rifle");
				Play(V, Cl, true, FMath::Clamp(Sp / 520.f, 0.6f, 1.6f));
			}
		}
	}

	static FString HudMode(const AZoogiGameMode* G);
	FString Hud(const AZoogiGameMode* G)
	{
		if (!On(G)) return FString();
		const FString Cr = S.BallLeft > 0.f && G->Royale.P.IsValidIndex(G->Royale.HumanIndex) && !G->Royale.P[G->Royale.HumanIndex].bRobot
			? FString::Printf(TEXT("BALL DASH %.0f s"), S.BallLeft) : (S.Crystals >= 3 ? FString::Printf(TEXT("CRYSTALS %d - TRANSFORM TO ROLL!"), S.Crystals) : FString::Printf(TEXT("CRYSTALS %d / 3"), S.Crystals));
		const FString Am = S.ReloadUntil > 0.f && G->GetWorld()->GetTimeSeconds() < S.ReloadUntil ? FString(TEXT("RELOADING")) : FString::Printf(TEXT("AMMO %d / 30"), FMath::Max(0, S.Ammo));
		return HudMode(G) + TEXT("     ") + Cr + TEXT("     ") + Am;
	}
	static FString HudMode(const AZoogiGameMode* G)
	{
		if (Kind() == 1)
		{
			int32 Left = 0; for (int32 i = 0; i < G->Royale.P.Num(); ++i) if (i != G->Royale.HumanIndex && G->Royale.P[i].IsIn()) ++Left;
			return FString::Printf(TEXT("HOLD THE LINE  -  WAVE %d / %d  -  %d LEFT"), S.Wave, S.MaxWave, Left);
		}
		if (Kind() == 3) return FString::Printf(TEXT("BLUE %d  -  %d RED   (FIRST TO %d)"), S.TeamKO[0], S.TeamKO[1], S.TeamGoal);
		return FString::Printf(TEXT("SHOOTER ROYALE  -  %d LEFT"), G->Royale.NumIn());
	}

	int32 CrewTarget(const AZoogiGameMode* G, const FVector& From)
	{
		if (!On(G) || Kind() != 1) return -1;
		int32 Best = -1; float Bd = 1e9f;
		for (int32 i = 0; i < G->Balls.Num() && i < G->Royale.P.Num(); ++i)
		{
			const AZoogiBall* B = G->Balls[i];
			if (!B || B->bOut || i == G->Royale.HumanIndex || !G->Royale.P[i].IsIn() || G->Royale.P[i].RespawnIn >= 0.f) continue;
			const float D = FVector::Dist2D(B->BallLocation(), From);
			if (D < Bd) { Bd = D; Best = i; }
		}
		return Best;
	}
}
