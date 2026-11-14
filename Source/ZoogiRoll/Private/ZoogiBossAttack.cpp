// b146: the Cosmos boss attacks in the closing rounds - see ZoogiBossAttack.h.
#include "ZoogiBossAttack.h"
#include "ZoogiAnimeCues.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiTypes.h"
#include "ZoogiHaptics.h"
#include "ZoogiSkills.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiDragonBoss.h"
#include "ZoogiGolemBoss.h"
#include "ZoogiRaid.h"
#include "ZoogiActionFun.h"
#include "ZoogiPlayerController.h"
#include "ZoogiEdgeHang.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "Camera/CameraActor.h"
#include "ZoogiCardSubsystem.h" // b190
#include "Engine/Canvas.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace ZoogiBossAttack
{
	namespace
	{
		enum class EKind : uint8 { None, Sweep, Slam };
		enum class ERun : uint8 { Idle, Sweep, Slam, Grace };
		struct FHitMark { TWeakObjectPtr<AZoogiBall> B; bool bDone = false, bOffered = false, bBraced = false, bCover = false; };
		struct FState
		{
			TWeakObjectPtr<AZoogiGameMode> G; int32 Seed = -1; TWeakObjectPtr<AZoogiBall> First;
			int32 Round = -1;          // the round this attack belongs to
			bool bPending = false;     // announced, not landed
			EKind Kind = EKind::None;
			FVector At = FVector::ZeroVector; // slam spot
			float Ang = 0.f;           // sweep centre (deg)
			float Pulse = 0.f; int32 LastTurn = -1; int32 Count = 0;
			ERun Run = ERun::Idle; float RunT = 0.f; float FxT = 0.f; bool bLanded = false; float RunAge = 0.f;
			TArray<FHitMark> Marks;
			// brace window (real time)
			TWeakObjectPtr<AZoogiBall> BraceBall; double BraceAt = -1.0; bool bPrevDown = true; bool bShotMid = false;
			// b147a: who last hit each rival / orb during the warning round (a boss ring-out scores for them); the attack camera
			TMap<TWeakObjectPtr<AZoogiBall>, int32> HitBy; TMap<TWeakObjectPtr<AZoogiOrb>, int32> OrbHitBy;
			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> CamPrev;
			// b190: invasions / grabs / anger duels / lock-on
			bool bMini = false; TArray<TWeakObjectPtr<AZoogiBall>> InZone; float Weak = 1.f; int32 BossHits = 0;
			int32 GrabRound = -1; TWeakObjectPtr<AZoogiBall> GrabB; bool bGrab = false; double GrabT0 = 0.0; float You = 0.f, BossM = 0.f; bool bTapPrev = true;
			float Anger = 0.f; int32 LastSc[4] = { 0, 0, 0, 0 }; bool bScInit = false; bool bAngerReady = false; int32 AngerBy = -1;
			bool bDuel = false; TWeakObjectPtr<AZoogiBall> DuelB; int32 DuelRound = 0, DuelWins = 0, DuelPhase = 0; double DuelNextAt = 0.0, DuelFlashAt = 0.0;
			FString Msg; double MsgAt = -10.0;
		};
		FState S;
		float HalfW() { return 35.f * ZoogiRaid::Rage(); } // sweep half-width (deg); b151: x1.3 when the raid titan is enraged
		constexpr float SweepTime = 1.25f; // game seconds for the hand to cross the slice
		constexpr float SlamTravel = 0.45f;
		const float SlamR[3] = { 300.f, 560.f, 820.f };
		const float SlamV[3] = { 1900.f, 1300.f, 700.f };
		const FLinearColor Red(1.f, 0.12f, 0.08f), Amber(1.f, 0.45f, 0.1f);
		float RingR(int32 i) { return SlamR[i] * ZoogiRaid::Rage() * (S.bMini ? 0.6f : 1.f); } // b190: an invasion slam is smaller

		bool Flag(const TCHAR* N) { return FParse::Param(FCommandLine::Get(), N); }
		bool TestMode() { static const bool b = Flag(TEXT("ZoogiBossAttackTest")); return b; }
		FVector Ctr(const AZoogiGameMode* G) { return G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector; }
		float ER(const AZoogiGameMode* G) { return G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f; }
		float AngOf(const AZoogiGameMode* G, const FVector& P) { const FVector C = Ctr(G); return FMath::RadiansToDegrees(FMath::Atan2(P.Y - C.Y, P.X - C.X)); }
		bool InBand(const AZoogiGameMode* G, const FVector& P) { const float R = FVector::Dist2D(P, Ctr(G)); return R > ER(G) * 0.5f && R < ER(G) + 80.f; }
		bool Alive(const AZoogiBall* B) { return B && !B->bOut && !B->IsParked() && !B->IsOnLaunchPad() && B->Sphere; }

		// turns left in this round, the current one included (1 = this is the round's last turn)
		int32 TurnsLeft(const AZoogiGameMode* G)
		{
			const int32 N = FMath::Max(1, G->Balls.Num());
			for (int32 k = 1; k <= N; ++k) if ((G->ActivePlayer + k) % N == G->RoundStartPlayer) return k;
			return N;
		}
		bool AttackRound(const AZoogiGameMode* G)
		{
			if (TestMode() || ZoogiRaid::Active()) return true; // b151: the raid titan attacks every round
			// b190 (Evan picked "every 3 rds"): the boss also attacks every 3rd round (3, 6, 9 ...), not only at the finale
			if (!Flag(TEXT("ZoogiBossFinaleOnly")) && G->RoundNumber >= 3 && G->RoundNumber % 3 == 0) return true;
			if (G->MenuRounds >= 4) return G->RoundNumber >= G->MenuRounds - 1;
			return G->RoundNumber >= G->MenuRounds;
		}
		AZoogiBall* Leader(AZoogiGameMode* G)
		{
			AZoogiBall* Best = nullptr; int32 Hi = -1000000;
			for (AZoogiBall* B : G->Balls) if (Alive(B)) { const int32 Sc = B->Score; if (Sc > Hi) { Hi = Sc; Best = B; } }
			return Best;
		}
		bool Covered(AZoogiGameMode* G, const AZoogiBall* B)
		{	// an obstacle between the Zoogi and the rim stops the hand
			const FVector C = Ctr(G), P = B->BallLocation() + FVector(0.f, 0.f, 15.f);
			FVector Out = FVector(P.X - C.X, P.Y - C.Y, 0.f); const float R = Out.Size(); if (R < 1.f) return false; Out /= R;
			const float To = ER(G) - 40.f - R; if (To < 40.f) return false;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiBossCover), false);
			for (AZoogiBall* Zb : G->Balls) if (Zb) Q.AddIgnoredActor(Zb);
			for (AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
			FCollisionObjectQueryParams Ob; Ob.AddObjectTypesToQuery(ECC_WorldStatic); Ob.AddObjectTypesToQuery(ECC_WorldDynamic);
			FHitResult H;
			const bool bHit = G->GetWorld()->LineTraceSingleByObjectType(H, P + Out * (ZoogiConst::ZoogiRadius + 5.f), P + Out * To, Ob, Q);
			if (bHit) UE_LOG(LogZoogi, Log, TEXT("BOSS cover: P%d is behind %s (%.0f cm out)"), B->PlayerIndex + 1, *GetNameSafe(H.GetActor()), H.Distance);
			return bHit;
		}
		void Fling(AZoogiGameMode* G, AZoogiBall* B, const FVector& Dir, float Speed, const TCHAR* What)
		{
			const int32* Hb = S.HitBy.Find(B); // b147a (Evan: "points for the brave"): whoever knocked it into the zone
			B->LastHitPlayer = (Hb && *Hb != B->PlayerIndex) ? *Hb : -1;
			const FVector D = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
			B->SetPlanarVelocity(D * Speed, Speed > 900.f ? 260.f : 120.f);
			UE_LOG(LogZoogi, Log, TEXT("BOSS %s P%d flung %.0f cm/s"), What, B->PlayerIndex + 1, Speed);
		}
		void EndCam(AZoogiGameMode* G, float Blend)
		{
			ACameraActor* C = S.Cam.Get();
			APlayerController* Pc = G && G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr;
			if (C && Pc && Pc->GetViewTarget() == C)
			{
				AActor* Back = S.CamPrev.Get();
				if (const AZoogiPlayerController* Z = Cast<AZoogiPlayerController>(Pc)) if (Z->MainCam()) Back = (AActor*)Z->MainCam();
				Pc->SetViewTargetWithBlend(Back ? Back : Pc->GetPawn(), Blend, VTBlend_Cubic);
			}
			if (C) C->SetLifeSpan(Blend + 0.5f);
			S.Cam.Reset(); S.CamPrev.Reset();
		}
		void StartCam(AZoogiGameMode* G)
		{	// b147a: frame the zone from the far side, high, with the boss behind it
			static const bool bOff = Flag(TEXT("ZoogiNoBossAttackCam"));
			UWorld* W = G->GetWorld(); APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			if (bOff || !Pc) return;
			const FVector C = Ctr(G);
			const FVector Dir = FRotator(0.f, S.Kind == EKind::Sweep ? S.Ang : AngOf(G, S.At), 0.f).Vector();
			const FVector Look = S.Kind == EKind::Sweep ? C + Dir * ER(G) * 0.8f + FVector(0, 0, 150.f) : (S.At + C + Dir * ER(G)) * 0.5f + FVector(0, 0, 150.f);
			FVector At = (S.Kind == EKind::Sweep ? C - Dir * ER(G) * 0.35f : S.At - Dir * ER(G) * 0.55f) + FVector(0, 0, ER(G) * 0.7f);
			if (S.Kind == EKind::Sweep && ZoogiDragon::Active())
			{	// b177b: ICE BREATH - lower and further back, looking out at the rim so Dracoria and its beam are both in frame
				At = C - Dir * ER(G) * 0.6f + FVector(0, 0, ER(G) * 0.32f);
				const FVector LookD = C + Dir * ER(G) * 1.1f + FVector(0, 0, ER(G) * 0.5f);
				if (UZoogiEpisodeCameraDirector* CD = UZoogiEpisodeCameraDirector::Get(G)) CD->StopAllShots(true);
				FActorSpawnParameters SPd; SPd.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (ACameraActor* Cd = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((LookD - At).Rotation(), At), SPd))
				{
					if (UCameraComponent* Cc = Cd->GetCameraComponent()) { Cc->SetFieldOfView(78.f); Cc->SetConstraintAspectRatio(false); }
					S.Cam = Cd; S.CamPrev = Pc->GetViewTarget(); Pc->SetViewTargetWithBlend(Cd, 0.35f, VTBlend_Cubic);
				}
				return;
			}
			if (UZoogiEpisodeCameraDirector* CD = UZoogiEpisodeCameraDirector::Get(G)) CD->StopAllShots(true);
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACameraActor* Cm = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Look - At).Rotation(), At), SP);
			if (!Cm) return;
			if (UCameraComponent* Cc = Cm->GetCameraComponent()) { Cc->SetFieldOfView(72.f); Cc->SetConstraintAspectRatio(false); }
			S.Cam = Cm; S.CamPrev = Pc->GetViewTarget();
			Pc->SetViewTargetWithBlend(Cm, 0.35f, VTBlend_Cubic);
		}
		void Announce(AZoogiGameMode* G, bool bMini = false)
		{
			S.HitBy.Reset(); S.OrbHitBy.Reset();
			S.bMini = bMini; S.Weak = 1.f; S.BossHits = 0; S.InZone.Reset();
			AZoogiBall* Hb = nullptr; for (AZoogiBall* B : G->Balls) if (Alive(B) && !B->bCPU) { Hb = B; break; }
			AZoogiBall* T = (TestMode() && Hb) ? Hb : (FMath::FRand() < 0.67f ? Leader(G) : nullptr);
			if (!T) { TArray<AZoogiBall*> Al; for (AZoogiBall* B : G->Balls) if (Alive(B)) Al.Add(B); if (Al.Num()) T = Al[FMath::RandRange(0, Al.Num() - 1)]; }
			if (!T) return;
			const FVector C = Ctr(G), P = T->BallLocation();
			const float R = FVector::Dist2D(P, C);
			S.Kind = bMini ? EKind::Slam : TestMode() ? (S.Count % 2 == 0 ? EKind::Sweep : EKind::Slam) : (R > ER(G) * 0.5f ? EKind::Sweep : EKind::Slam);
			S.Ang = AngOf(G, P);
			S.At = FVector(P.X, P.Y, C.Z);
			if (S.Kind == EKind::Slam && R > ER(G) * 0.7f) { const FVector2D D2 = FVector2D(P.X - C.X, P.Y - C.Y).GetSafeNormal() * ER(G) * 0.7f; S.At = FVector(C.X + D2.X, C.Y + D2.Y, C.Z); }
			S.bPending = true; S.Round = G->RoundNumber; S.Pulse = 0.f; ++S.Count;
			if (bMini)
			{	// b190 BOSS INVASION: a small fist slam - whoever stands in it now and gets clear earns a crystal
				for (AZoogiBall* B : G->Balls) if (Alive(B) && FVector::Dist2D(B->BallLocation(), S.At) <= RingR(2)) S.InZone.Add(B);
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.6f, 0.8f);
				if (ZoogiRaid::NewHud()) ZoogiRaid::HowTo(6, FString::Printf(TEXT("%s INVASION: FIST SLAM"), *ZoogiCosmosBoss::Name())); // b207 D
				else if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("%s INVASION: FIST SLAM!"), *ZoogiCosmosBoss::Name()), TEXT("ROLL OUT OF THE RINGS - DODGE IT = +1 CRYSTAL"), Amber, 2.f, 1.1f);
				G->PublicFeed(FString::Printf(TEXT("A SMALL BOSS SLAM LANDS AFTER EVERYONE'S TURN (%d IN THE RINGS)"), S.InZone.Num()), Amber);
				UE_LOG(LogZoogi, Log, TEXT("BOSS INVASION warning (round %d): small slam at (%.0f, %.0f), %d Zoogis in the rings"), G->RoundNumber, S.At.X, S.At.Y, S.InZone.Num());
				return;
			}
			ZoogiCosmosBoss::Taunt();
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 0.6f);
			ZoogiHaptics::Kick(0.5f, G->GetWorld(), TEXT("boss_warning"));
			const bool bSw = S.Kind == EKind::Sweep;
			if (ZoogiRaid::NewHud()) // b207 D: the HOW TO HURT IT card instead of the corner callout
				ZoogiRaid::HowTo(bSw ? 1 : 2, bSw ? (ZoogiDragon::Active() ? FString(TEXT("ICE BREATH")) : ZoogiGolem::Active() ? FString(TEXT("HAMMER SWEEP")) : FString(TEXT("HAND SWEEP"))) : (ZoogiGolem::Active() ? FString(TEXT("GROUND SLASH")) : FString(TEXT("GROUND SLAM"))));
			else if (UZoogiFunSys* F = UZoogiFunSys::Get(G))
				F->PublicCallout(FString::Printf(TEXT("%s: %s"), *ZoogiCosmosBoss::Name(), bSw ? (ZoogiDragon::Active() ? TEXT("ICE BREATH!") : ZoogiGolem::Active() ? TEXT("HAMMER SWEEP!") : TEXT("HAND SWEEP!")) : (ZoogiGolem::Active() ? TEXT("GROUND SLASH!") : TEXT("GROUND SLAM!"))),
					bSw ? TEXT("GET OUT OF THE RED ZONE - OR HIDE BEHIND COVER") : TEXT("GET CLEAR OF THE RINGS - OR GUARD"), Red, 2.2f, 1.2f);
			G->PublicFeed(FString::Printf(TEXT("THE %s IS COMING FOR P%d %s - IT LANDS AFTER EVERYONE'S TURN"), *ZoogiCosmosBoss::Name(), T->PlayerIndex + 1, *T->Def.Name.ToUpper()), Red);
			UE_LOG(LogZoogi, Log, TEXT("BOSS ATTACK warning (round %d): %s aimed at P%d - %s yaw %.0f / spot (%.0f, %.0f), lands in %d turns"),
				G->RoundNumber, bSw ? TEXT("HAND SWEEP") : TEXT("GROUND SLAM"), T->PlayerIndex + 1, bSw ? TEXT("slice") : TEXT("slam"), S.Ang, S.At.X, S.At.Y, TurnsLeft(G));
			if (TestMode()) G->TakeShotDelayed(FString::Printf(TEXT("boss_warn_%d"), S.Count), 1.2f);
			ZoogiAnimeCues::Fire(G->GetWorld(), TEXT("BossAttack"), T->PlayerIndex); // b164
		}
		void DrawZone(AZoogiGameMode* G)
		{	// red pulses on the floor: the sweep slice / the slam rings
			UWorld* W = G->GetWorld(); const FVector C = Ctr(G);
			if (S.Kind == EKind::Sweep)
			{
				for (int32 i = 0; i <= 6; ++i)
				{
					const float A = FMath::DegreesToRadians(S.Ang - HalfW() + i * (2.f * HalfW() / 6.f));
					for (float Rf : { 0.62f, 0.86f })
						AZoogiFX::Spawn(W, C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * ER(G) * Rf + FVector(0, 0, 6.f), Red, 190.f, 0.95f, false, 14.f);
				}
			}
			else for (int32 i = 0; i < 3; ++i) AZoogiFX::Spawn(W, S.At + FVector(0, 0, 6.f + i), i == 0 ? Red : i == 1 ? Amber : FLinearColor(1.f, 0.8f, 0.2f), RingR(i), 0.95f, true, 16.f - 3.f * i);
		}
		void Countdown(AZoogiGameMode* G)
		{
			if (ZoogiRaid::NewHud()) return; // b207 HUD A: the warning lives in the one ticker under the boss plate
			const int32 Tl = TurnsLeft(G);
			const FString Nm = S.Kind == EKind::Sweep ? ZoogiDragon::SweepName(ZoogiGolem::Active() ? TEXT("HAMMER SWEEP") : TEXT("HAND SWEEP")) : TEXT("GROUND SLAM");
			const FString Tx = Tl <= 1 ? FString::Printf(TEXT("%s AFTER THIS TURN!"), *Nm) : FString::Printf(TEXT("%s IN %d TURNS"), *Nm, Tl);
			const FVector C = Ctr(G);
			const FVector P = S.Kind == EKind::Sweep ? C + FRotator(0.f, S.Ang, 0.f).Vector() * ER(G) * 0.75f : S.At;
			G->PublicPopup(Tx, P + FVector(0, 0, 220.f), Red);
		}
		void StartRun(AZoogiGameMode* G)
		{
			S.bPending = false; S.RunT = 0.f; S.FxT = 0.f; S.bLanded = false; S.Marks.Reset(); S.bShotMid = false;
			if (TestMode() && S.Count >= 3 && S.Kind == EKind::Sweep)
				for (AZoogiBall* B : G->Balls) if (Alive(B) && !B->bCPU)
				{	// test: the human stands in the slice (and braces - the edge hang)
					const FVector P = Ctr(G) + FRotator(0.f, S.Ang, 0.f).Vector() * ER(G) * 0.86f + FVector(0, 0, B->BallLocation().Z - Ctr(G).Z);
					B->Sphere->SetWorldLocation(P, false, nullptr, ETeleportType::TeleportPhysics); B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
					UE_LOG(LogZoogi, Log, TEXT("BOSS TEST: P%d placed in the slice"), B->PlayerIndex + 1);
				}
			for (AZoogiBall* B : G->Balls) if (Alive(B)) { FHitMark M; M.B = B; S.Marks.Add(M); }
			if (S.Kind == EKind::Sweep)
			{
				S.Run = ERun::Sweep;
				for (FHitMark& M : S.Marks) if (AZoogiBall* B = M.B.Get()) if (InBand(G, B->BallLocation()) && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, AngOf(G, B->BallLocation()))) < HalfW() + 8.f) M.bCover = Covered(G, B);
				ZoogiCosmosBoss::Sweep(true, S.Ang - HalfW());
				ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 1.f, 0.6f);
			}
			else { S.Run = ERun::Slam; ZoogiCosmosBoss::SlamToward(AngOf(G, S.At)); ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 0.9f, 0.5f); }
			StartCam(G);
			UE_LOG(LogZoogi, Log, TEXT("BOSS ATTACK lands: %s (round %d, %d Zoogis in play)"), S.Kind == EKind::Sweep ? TEXT("HAND SWEEP") : TEXT("GROUND SLAM"), G->RoundNumber, S.Marks.Num());
		}
		bool TapNow(UWorld* W)
		{
			APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			if (!Pc) return false;
			float X = 0.f, Y = 0.f; bool bT = false; Pc->GetInputTouchState(ETouchIndex::Touch1, X, Y, bT);
			return bT || Pc->IsInputKeyDown(EKeys::LeftMouseButton) || Pc->IsInputKeyDown(EKeys::SpaceBar);
		}
		void StepSweep(AZoogiGameMode* G, float GDt)
		{
			UWorld* W = G->GetWorld(); const FVector C = Ctr(G);
			const float Prev = S.RunT;
			S.RunT += GDt;
			const float Lead = ZoogiDragon::SweepLead(), SwT = SweepTime * ZoogiDragon::SweepSlow(); // b177a: Dracoria dives in first, the beam crosses slower
			auto AngAt = [Lead, SwT](float T) { const float K = FMath::Clamp((T - Lead) / SwT, 0.f, 1.f); const float E = K * K * (3.f - 2.f * K); return S.Ang - HalfW() + 2.f * HalfW() * E; };
			const float A0 = AngAt(Prev), A1 = AngAt(S.RunT);
			ZoogiCosmosBoss::Sweep(true, A1);
			const FVector Tan = FRotator(0.f, A1 + 90.f, 0.f).Vector();
			S.FxT -= GDt;
			if (S.FxT <= 0.f && S.RunT >= Lead)
			{
				S.FxT = 0.05f;
				const FVector Hp = C + FRotator(0.f, A1, 0.f).Vector() * ER(G) * 0.8f + FVector(0, 0, 40.f);
				AZoogiFX::Spawn(W, Hp, ZoogiCosmosBoss::Tint(), 260.f, 0.35f, false, 9.f); // b148a: the map boss's colour
				AZoogiFX::Spawn(W, Hp + FRotator(0.f, A1, 0.f).Vector() * 220.f, Red, 200.f, 0.3f, false, 7.f);
			}
			if (TestMode() && !S.bShotMid && S.RunT > Lead + SwT * 0.5f) { S.bShotMid = true; G->TakeShot(FString::Printf(TEXT("boss_sweep_%d"), S.Count)); }
			for (FHitMark& M : S.Marks)
			{
				AZoogiBall* B = M.B.Get();
				if (M.bDone || !Alive(B)) continue;
				const FVector P = B->BallLocation();
				if (!InBand(G, P)) continue;
				const float Ab = AngOf(G, P);
				const float D0 = FMath::FindDeltaAngleDegrees(A0, Ab), D1 = FMath::FindDeltaAngleDegrees(A1, Ab);
				if (S.RunT >= Lead && !M.bOffered && !M.bCover && D1 > 0.f && D1 < 16.f && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, Ab)) <= HalfW() + 2.f)
				{	// BRACE: the hand is about to hit this Zoogi
					M.bOffered = true;
					if (B->bCPU) M.bBraced = FMath::FRand() < 0.4f;
					else if (TestMode() && S.Count >= 3) { M.bBraced = true; UE_LOG(LogZoogi, Log, TEXT("BOSS TEST: P%d braces"), B->PlayerIndex + 1); }
					else if (!S.BraceBall.IsValid())
					{
						S.BraceBall = B; S.BraceAt = FPlatformTime::Seconds(); S.bPrevDown = TapNow(W);
						if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("TAP TO BRACE!"), ZoogiDragon::Active() ? TEXT("THE ICE BEAM IS ON YOU") : ZoogiGolem::Active() ? TEXT("THE HAMMER IS COMING") : TEXT("THE HAND IS ON YOU"), FLinearColor(1.f, 0.85f, 0.2f), 1.1f, 1.3f);
						G->PublicPopup(TEXT("TAP!"), P + FVector(0, 0, 170.f), FLinearColor(1.f, 0.85f, 0.2f));
						UE_LOG(LogZoogi, Log, TEXT("BOSS brace window for P%d"), B->PlayerIndex + 1);
					}
				}
				if (D0 > 0.f && D1 <= 0.f && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, Ab)) <= HalfW() + 2.f)
				{	// the hand passes it
					M.bDone = true;
					if (S.BraceBall.Get() == B) S.BraceBall.Reset();
					if (M.bCover)
					{
						G->PublicPopup(TEXT("COVER!"), P + FVector(0, 0, 160.f), FLinearColor(0.5f, 0.9f, 1.f));
						AZoogiFX::Spawn(W, P, FLinearColor(0.5f, 0.9f, 1.f), 180.f, 0.5f, true, 8.f);
						UE_LOG(LogZoogi, Log, TEXT("BOSS sweep: P%d safe behind cover"), B->PlayerIndex + 1);
						continue;
					}
					const FVector Out = FVector(P.X - C.X, P.Y - C.Y, 0.f).GetSafeNormal();
					const FVector Dir = (Out * 1.f + Tan * 0.75f).GetSafeNormal();
					if (M.bBraced && !B->bCPU)
					{	// b147a (Evan): braced = knocked just over the rim, where it grabs on - the tap-to-pull-up mini-game
						ZoogiEdgeHang::Force(B, 4.f);
						EndCam(G, 0.2f);
						Fling(G, B, Out, 800.f, TEXT("sweep (braced - hang on!)"));
						G->PublicPopup(TEXT("BRACED - HANG ON!"), P + FVector(0, 0, 160.f), FLinearColor(1.f, 0.85f, 0.2f));
					}
					else if (M.bBraced) { Fling(G, B, Dir, 380.f, TEXT("sweep (braced)")); G->PublicPopup(TEXT("BRACED!"), P + FVector(0, 0, 160.f), FLinearColor(1.f, 0.85f, 0.2f)); }
					else { Fling(G, B, Dir, 1900.f * S.Weak, TEXT("sweep")); G->PublicPopup(TEXT("SWEPT!"), P + FVector(0, 0, 160.f), Red); G->PublicFeed(FString::Printf(TEXT("THE BOSS SWEEPS P%d %s"), B->PlayerIndex + 1, *B->Def.Name.ToUpper()), Red); }
					AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), P, FRotator::ZeroRotator, 0.9f);
					ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.7f, 0.9f);
					if (!B->bCPU) ZoogiHaptics::Kick(M.bBraced ? 0.4f : 0.9f, W, TEXT("boss_sweep_hit"));
				}
			}
			// orbs in the slice get dragged off too
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !O->Sphere) continue;
				const FVector P = O->OrbLocation(); if (!InBand(G, P)) continue;
				const float Ab = AngOf(G, P);
				if (FMath::FindDeltaAngleDegrees(A0, Ab) > 0.f && FMath::FindDeltaAngleDegrees(A1, Ab) <= 0.f && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, Ab)) <= HalfW() + 2.f)
				{
					{ const int32* Ob = S.OrbHitBy.Find(O); O->LastHitBy = Ob ? *Ob : -1; } // b147a
					const FVector Out = FVector(P.X - C.X, P.Y - C.Y, 0.f).GetSafeNormal();
					O->Sphere->SetPhysicsLinearVelocity((Out + Tan * 0.75f).GetSafeNormal() * 1500.f + FVector(0, 0, 150.f));
				}
			}
			if (S.RunT >= Lead + SwT + 0.15f) { ZoogiCosmosBoss::Sweep(false, A1); S.Run = ERun::Grace; S.RunT = 0.f; S.BraceBall.Reset(); UE_LOG(LogZoogi, Log, TEXT("BOSS sweep done")); }
		}
		void StepSlam(AZoogiGameMode* G, float GDt)
		{
			UWorld* W = G->GetWorld(); const FVector C = Ctr(G);
			S.RunT += GDt;
			const FVector Rim = C + FRotator(0.f, AngOf(G, S.At), 0.f).Vector() * ER(G);
			if (S.RunT <= GDt * 1.5f && ZoogiGolem::Active() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoGolemSlash")))
			{	// b179 (Evan: "remove ground slash as a skill and give it to a boss monster"): the Golem's hammer sends the purple
				// Ground Slash wave tearing across the court from its rim to the target
				const FVector Dir = (S.At - Rim).GetSafeNormal2D();
				const float Len = FVector::Dist2D(Rim, S.At);
				for (int32 k = 0; k < 3; ++k)
					AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_GroundSlash03.NS_GroundSlash03"), FVector(Rim.X, Rim.Y, S.At.Z) + Dir * Len * (k + 0.5f) / 3.f, 2.4f, 6.f, nullptr, FVector::ZeroVector, Dir.Rotation());
				ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 1.f, 0.7f); ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.9f, 0.8f);
			}
			S.FxT -= GDt;
			if (!S.bLanded && S.FxT <= 0.f)
			{	// the shockwave runs across the floor from the rim to the spot
				S.FxT = 0.04f;
				const float K = FMath::Clamp(S.RunT / SlamTravel, 0.f, 1.f);
				AZoogiFX::Spawn(W, FMath::Lerp(Rim, S.At, K) + FVector(0, 0, 10.f), Amber, 170.f, 0.3f, false, 9.f);
			}
			if (!S.bLanded && S.RunT >= SlamTravel)
			{
				S.bLanded = true;
				AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), S.At + FVector(0, 0, 20.f), FRotator::ZeroRotator, 2.2f);
				for (int32 i = 0; i < 3; ++i) AZoogiFX::Spawn(W, S.At + FVector(0, 0, 8.f), i == 0 ? Red : Amber, RingR(i), 0.7f, true, 14.f - 3.f * i);
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 1.f, 0.55f);
				ZoogiHaptics::Kick(1.f, W, TEXT("boss_slam"));
				if (AZoogiPlayerController* Pc = Cast<AZoogiPlayerController>(W->GetFirstPlayerController())) Pc->OnImpact(1.f);
				for (FHitMark& M : S.Marks)
				{
					AZoogiBall* B = M.B.Get(); if (!Alive(B)) continue;
					const FVector P = B->BallLocation(); const float D = FVector::Dist2D(P, S.At);
					int32 Ring = -1; for (int32 i = 0; i < 3; ++i) if (D <= RingR(i)) { Ring = i; break; }
					if (Ring < 0) continue;
					FVector Dir = FVector(P.X - S.At.X, P.Y - S.At.Y, 0.f); if (Dir.Size() < 20.f) Dir = FVector(P.X - C.X, P.Y - C.Y, 0.f); if (Dir.Size() < 1.f) Dir = FVector(1, 0, 0);
					const bool bGuard = ZoogiSkills::GuardBusy(G, B->PlayerIndex);
					const float V = SlamV[Ring] * (bGuard ? 0.15f : 1.f) * (S.bMini ? 0.55f : 1.f) * S.Weak; // b190: invasions are softer; boss lock-on weakens it
					Fling(G, B, Dir, V, bGuard ? TEXT("slam (GUARD anchored)") : TEXT("slam"));
					G->PublicPopup(bGuard ? FString(TEXT("ANCHORED!")) : FString(Ring == 0 ? TEXT("SLAMMED!") : TEXT("SHOVED!")), P + FVector(0, 0, 160.f), bGuard ? FLinearColor(0.55f, 0.85f, 1.f) : Red);
				}
				for (AZoogiOrb* O : G->Orbs)
				{
					if (!O || O->bOut || !O->Sphere) continue;
					const FVector P = O->OrbLocation(); const float D = FVector::Dist2D(P, S.At);
					int32 Ring = -1; for (int32 i = 0; i < 3; ++i) if (D <= RingR(i)) { Ring = i; break; }
					if (Ring < 0) continue;
					FVector Dir = FVector(P.X - S.At.X, P.Y - S.At.Y, 0.f); if (Dir.Size() < 20.f) Dir = FVector(P.X - C.X, P.Y - C.Y, 0.f);
					{ const int32* Ob = S.OrbHitBy.Find(O); O->LastHitBy = Ob ? *Ob : -1; } // b147a
					O->Sphere->WakeRigidBody();
					O->Sphere->SetPhysicsLinearVelocity(Dir.GetSafeNormal() * SlamV[Ring] * 0.9f + FVector(0, 0, 200.f));
				}
				if (S.bMini)
					for (const TWeakObjectPtr<AZoogiBall>& Wz : S.InZone)
						if (AZoogiBall* Bz = Wz.Get()) if (Alive(Bz) && FVector::Dist2D(Bz->BallLocation(), S.At) > RingR(2) && Bz->PlayerIndex >= 0 && Bz->PlayerIndex < 4)
						{	// b190: got clear of the invasion slam
							if (G->AwakenChargeOn()) G->AwakenCharge[Bz->PlayerIndex] += 1;
							G->PublicPopup(TEXT("DODGED! +1 CRYSTAL"), Bz->BallLocation() + FVector(0, 0, 170.f), FLinearColor(1.f, 0.85f, 0.2f));
							UE_LOG(LogZoogi, Log, TEXT("BOSS INVASION: P%d dodged (+1 crystal)"), Bz->PlayerIndex + 1);
						}
				G->PublicFeed(S.bMini ? TEXT("THE BOSS INVADES!") : TEXT("THE BOSS SLAMS THE ARENA!"), Red);
				UE_LOG(LogZoogi, Log, TEXT("BOSS slam lands at (%.0f, %.0f)"), S.At.X, S.At.Y);
				if (TestMode()) G->TakeShotDelayed(FString::Printf(TEXT("boss_slam_%d"), S.Count), 0.1f);
			}
			if (S.RunT >= SlamTravel + 0.3f) { S.Run = ERun::Grace; S.RunT = 0.f; }
		}
		// ================================================================ b190 (Evan picked: every 3 rounds, grabs, invasions, anger, lock-on)
		bool InvasionRound(const AZoogiGameMode* G);
		bool GrabRound(const AZoogiGameMode* G)
		{
			static const bool bTest = Flag(TEXT("ZoogiBossGrabTest"));
			if (bTest) return true;
			if (Flag(TEXT("ZoogiNoBossGrab")) || TestMode()) return false;
			return G->RoundNumber >= 4 && G->RoundNumber % 3 == 1 && !AttackRound(G);
		}
		bool InvasionRound(const AZoogiGameMode* G)
		{	// a small fist slam in every other round from round 2 (not on attack / grab rounds)
			if (Flag(TEXT("ZoogiNoBossInvasion")) || TestMode()) return false;
			return G->RoundNumber >= 2 && !AttackRound(G) && !GrabRound(G);
		}
		AZoogiBall* HumanBall(AZoogiGameMode* G) { for (AZoogiBall* B : G->Balls) if (B && !B->bCPU) return B; return nullptr; }
		void Say(const FString& M) { S.Msg = M; S.MsgAt = FPlatformTime::Seconds(); }
		void EndGrab(AZoogiGameMode* G, bool bWin)
		{
			AZoogiBall* B = S.GrabB.Get();
			ZoogiCosmosBoss::HoldAt(false, 0.f);
			S.bGrab = false; S.GrabRound = G->RoundNumber;
			if (!B) return;
			const FVector P = B->BallLocation();
			if (bWin)
			{
				if (UZoogiCardSubsystem* Cs = UZoogiCardSubsystem::Get(G)) Cs->AddEnergyNow(B->PlayerIndex, 3);
				G->PublicPopup(TEXT("BREAK FREE! +3 SP"), P + FVector(0, 0, 190.f), FLinearColor(1.f, 0.85f, 0.2f));
				AZoogiFX::Spawn(G->GetWorld(), P, FLinearColor(1.f, 0.85f, 0.2f), 260.f, 0.5f, true, 12.f);
				ZoogiUtil::PlaySfx(G, TEXT("Score"), 1.f, 1.2f);
				Say(TEXT("BREAK FREE!  +3 SP"));
			}
			else
			{
				const FVector C = Ctr(G); FVector Out = FVector(P.X - C.X, P.Y - C.Y, 0.f); if (Out.Size() < 1.f) Out = FVector(1, 0, 0);
				Fling(G, B, Out, 1400.f * S.Weak, TEXT("grab throw"));
				G->PublicPopup(TEXT("THROWN!"), P + FVector(0, 0, 190.f), Red);
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.7f);
				Say(TEXT("THE BOSS THROWS YOU!"));
			}
			UE_LOG(LogZoogi, Log, TEXT("BOSS GRAB P%d %s"), B->PlayerIndex + 1, bWin ? TEXT("broke free (+3 SP)") : TEXT("thrown"));
		}
		void StartGrab(AZoogiGameMode* G, AZoogiBall* B)
		{
			S.GrabRound = G->RoundNumber; S.GrabB = B;
			ZoogiCosmosBoss::Taunt(); ZoogiCosmosBoss::HoldAt(true, AngOf(G, B->BallLocation()));
			ZoogiHaptics::Kick(0.7f, G->GetWorld(), TEXT("boss_grab"));
			if (ZoogiRaid::NewHud()) { if (B->bCPU) ZoogiRaid::Ticker(FString::Printf(TEXT("%s GRABS P%d!"), *ZoogiCosmosBoss::Name(), B->PlayerIndex + 1), Red); else ZoogiRaid::HowTo(3, FString::Printf(TEXT("%s GRABS YOU!"), *ZoogiCosmosBoss::Name())); } // b207
			else if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("%s GRABS P%d!"), *ZoogiCosmosBoss::Name(), B->PlayerIndex + 1), B->bCPU ? TEXT("THE LEADER FIGHTS TO BREAK FREE") : TEXT("TAP AS FAST AS YOU CAN TO BREAK FREE!"), Red, 1.8f, 1.1f);
			UE_LOG(LogZoogi, Log, TEXT("BOSS GRAB on the leader P%d (round %d)"), B->PlayerIndex + 1, G->RoundNumber);
			if (B->bCPU) { EndGrab(G, FMath::FRand() < 0.45f); return; } // a CPU's struggle resolves at once (no pause on its turn)
			S.bGrab = true; S.GrabT0 = FPlatformTime::Seconds(); S.You = 0.08f; S.BossM = 0.15f; S.bTapPrev = TapNow(G->GetWorld()); S.FxT = 0.f;
		}
		void StepGrab(AZoogiGameMode* G, float RealDt)
		{
			AZoogiBall* B = S.GrabB.Get();
			if (!B || B->bOut) { S.bGrab = false; ZoogiCosmosBoss::HoldAt(false, 0.f); return; }
			G->PhaseTime = 0.f; // the turn waits
			const bool bDown = TapNow(G->GetWorld());
			if (bDown && !S.bTapPrev) { S.You += 0.075f; ZoogiHaptics::Kick(0.15f, G->GetWorld(), TEXT("boss_grab_tap")); }
			S.bTapPrev = bDown;
			S.BossM += RealDt * 0.21f;
			S.FxT -= RealDt;
			if (S.FxT <= 0.f) { S.FxT = 0.22f; AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), ZoogiCosmosBoss::Tint(), 150.f, 0.25f, true, 9.f); }
			if (S.You >= 1.f) EndGrab(G, true);
			else if (S.BossM >= 1.f || FPlatformTime::Seconds() - S.GrabT0 > 7.0) EndGrab(G, false);
		}
		// ---- ANGER: big scores fill it; whoever fills it duels the boss on their next turn
		void AngerFeed(AZoogiGameMode* G)
		{
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->PlayerIndex < 0 || B->PlayerIndex > 3) continue;
				int32& L = S.LastSc[B->PlayerIndex];
				const int32 D = B->Score - L; L = B->Score;
				if (D <= 0 || S.bAngerReady || G->RoundNumber < 2 || Flag(TEXT("ZoogiNoBossAnger"))) continue;
				static const bool bFast = Flag(TEXT("ZoogiBossAngerTest"));
				S.Anger = FMath::Min(1.f, S.Anger + D / (bFast ? 200.f : 1800.f));
				if (S.Anger >= 1.f)
				{
					S.bAngerReady = true; S.AngerBy = B->PlayerIndex;
					if (ZoogiRaid::NewHud()) ZoogiRaid::Ticker(FString::Printf(TEXT("%s IS FURIOUS - P%d DUELS IT ON THEIR TURN"), *ZoogiCosmosBoss::Name(), B->PlayerIndex + 1), Red); // b207
					else if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("%s IS FURIOUS!"), *ZoogiCosmosBoss::Name()), FString::Printf(TEXT("P%d FILLED ITS ANGER - A BOSS DUEL ON THEIR NEXT TURN"), B->PlayerIndex + 1), Red, 2.f, 1.1f);
					ZoogiCosmosBoss::Taunt();
					UE_LOG(LogZoogi, Log, TEXT("BOSS ANGER full - P%d duels next"), B->PlayerIndex + 1);
				}
			}
		}
		void EndDuel(AZoogiGameMode* G, bool bWin, AZoogiBall* B)
		{
			S.bDuel = false; S.bAngerReady = false; S.Anger = 0.f;
			if (!B) return;
			const FVector P = B->BallLocation();
			if (bWin) { G->PublicPopup(TEXT("YOU WIN - POINTS KEPT"), P + FVector(0, 0, 190.f), FLinearColor(1.f, 0.85f, 0.2f)); Say(FString::Printf(TEXT("P%d BEATS THE BOSS - POINTS KEPT"), B->PlayerIndex + 1)); ZoogiUtil::PlaySfx(G, TEXT("Score"), 1.f, 1.1f); }
			else
			{
				const int32 Take = FMath::Min(50, FMath::Max(0, B->Score));
				B->Score -= Take; S.LastSc[FMath::Clamp(B->PlayerIndex, 0, 3)] = B->Score;
				G->PublicPopup(FString::Printf(TEXT("-%d STOLEN"), Take), P + FVector(0, 0, 190.f), Red); Say(FString::Printf(TEXT("THE BOSS STEALS %d FROM P%d"), Take, B->PlayerIndex + 1));
				ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 0.8f);
			}
			UE_LOG(LogZoogi, Log, TEXT("BOSS DUEL P%d %s (%d of 3)"), B->PlayerIndex + 1, bWin ? TEXT("won") : TEXT("lost"), S.DuelWins);
		}
		void StartDuel(AZoogiGameMode* G, AZoogiBall* B)
		{
			S.DuelB = B;
			if (B->bCPU) { EndDuel(G, FMath::FRand() < 0.5f, B); return; }
			S.bDuel = true; S.DuelRound = 0; S.DuelWins = 0; S.DuelPhase = 0; S.DuelNextAt = FPlatformTime::Seconds() + 1.4 + FMath::FRandRange(0.f, 0.9f); S.bTapPrev = TapNow(G->GetWorld());
			if (ZoogiRaid::NewHud()) ZoogiRaid::HowTo(4); // b207 D
			else if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("BOSS DUEL!"), TEXT("TAP THE MOMENT THE FIST FLASHES - NOT BEFORE"), Red, 1.4f, 1.f);
			UE_LOG(LogZoogi, Log, TEXT("BOSS DUEL starts for P%d"), B->PlayerIndex + 1);
		}
		void StepDuel(AZoogiGameMode* G)
		{
			AZoogiBall* B = S.DuelB.Get();
			if (!B || B->bOut) { S.bDuel = false; return; }
			G->PhaseTime = 0.f;
			const double Tn = FPlatformTime::Seconds();
			const bool bDown = TapNow(G->GetWorld()); const bool bTap = bDown && !S.bTapPrev; S.bTapPrev = bDown;
			auto RoundOver = [&](bool bWon, const TCHAR* What)
			{
				if (bWon) ++S.DuelWins;
				G->PublicPopup(What, B->BallLocation() + FVector(0, 0, 170.f), bWon ? FLinearColor(0.4f, 0.9f, 1.f) : Red);
				++S.DuelRound; S.DuelPhase = 2; S.DuelNextAt = Tn + 0.7;
			};
			if (S.DuelPhase == 0)
			{
				if (bTap) RoundOver(false, TEXT("TOO EARLY!"));
				else if (Tn >= S.DuelNextAt)
				{
					S.DuelPhase = 1; S.DuelFlashAt = Tn;
					ZoogiCosmosBoss::SlamToward(AngOf(G, B->BallLocation()));
					AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor::White, 320.f, 0.3f, true, 14.f);
					ZoogiHaptics::Kick(0.6f, G->GetWorld(), TEXT("boss_duel_flash"));
					ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 1.f, 0.7f);
				}
			}
			else if (S.DuelPhase == 1)
			{
				if (bTap) { RoundOver(true, TEXT("REACT!")); AZoogiFX::Niagara(G->GetWorld(), TEXT("NS_Burst_White"), B->BallLocation() + FVector(0, 0, 60.f), FRotator::ZeroRotator, 1.f); }
				else if (Tn - S.DuelFlashAt > 0.65) RoundOver(false, TEXT("TOO SLOW!"));
			}
			else if (Tn >= S.DuelNextAt)
			{
				if (S.DuelRound >= 3 || S.DuelWins >= 2 || S.DuelRound - S.DuelWins >= 2) EndDuel(G, S.DuelWins >= 2, B);
				else { S.DuelPhase = 0; S.DuelNextAt = Tn + 0.9 + FMath::FRandRange(0.f, 0.9f); }
			}
		}
	}

	bool CamActive() { return S.Cam.IsValid(); }
	bool Pending(const AZoogiGameMode* G) { return G && S.G.Get() == G && S.bPending && !S.bMini && S.Run == ERun::Idle; }
	bool InDangerZone(const AZoogiGameMode* G, const FVector& P)
	{
		if (!Pending(G)) return false;
		if (S.Kind == EKind::Sweep) return InBand(G, P) && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, AngOf(G, P))) <= HalfW() + 8.f;
		return FVector::Dist2D(P, S.At) <= RingR(2);
	}
	void Interrupt(AZoogiGameMode* G, int32 ByPlayer)
	{
		if (!Pending(G)) return;
		S.bPending = false; S.InZone.Reset();
		ZoogiCosmosBoss::Sweep(false, 0.f);
		ZoogiCosmosBoss::Flinch();
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("INTERRUPTED!"), FString::Printf(TEXT("P%d STOPPED THE %s'S ATTACK"), ByPlayer + 1, *ZoogiCosmosBoss::Name()), FLinearColor(0.35f, 0.95f, 0.55f), 2.f, 1.2f);
		ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 1.4f);
		UE_LOG(LogZoogi, Log, TEXT("BOSS attack of round %d INTERRUPTED by P%d"), S.Round, ByPlayer + 1);
	}

	bool CpuDodge(AZoogiGameMode* G, AZoogiBall* Me, FVector2D& OutDir, float& OutRun)
	{
		if (!G || !Me || S.G.Get() != G || !S.bPending || !Alive(Me)) return false;
		const FVector C = Ctr(G), P = Me->BallLocation();
		bool bIn = false;
		if (S.Kind == EKind::Sweep) bIn = InBand(G, P) && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, AngOf(G, P))) <= HalfW() + 8.f && !Covered(G, Me);
		else bIn = FVector::Dist2D(P, S.At) <= RingR(2);
		if (!bIn) return false;
		if (!TestMode() && FMath::FRand() < 0.3f) { UE_LOG(LogZoogi, Log, TEXT("BOSS cpu P%d stays in the zone and attacks"), Me->PlayerIndex + 1); return false; }
		if (S.Kind == EKind::Slam && !ZoogiSkills::GuardBusy(G, Me->PlayerIndex))
		{
			ZoogiSkills::GuardArm(G, Me->PlayerIndex);
			if (ZoogiSkills::GuardBusy(G, Me->PlayerIndex)) { UE_LOG(LogZoogi, Log, TEXT("BOSS cpu P%d GUARDs against the slam"), Me->PlayerIndex + 1); return false; }
		}
		FVector T;
		if (S.Kind == EKind::Sweep) T = C + FVector(P.X - C.X, P.Y - C.Y, 0.f).GetSafeNormal() * ER(G) * 0.25f;
		else { FVector Aw = FVector(C.X - S.At.X, C.Y - S.At.Y, 0.f); if (Aw.Size() < 100.f) Aw = FVector(P.X - S.At.X, P.Y - S.At.Y, 0.f); T = C + Aw.GetSafeNormal() * ER(G) * 0.35f; }
		const FVector2D D(T.X - P.X, T.Y - P.Y);
		if (D.Size() < 120.f) return false;
		OutDir = D.GetSafeNormal(); OutRun = D.Size();
		UE_LOG(LogZoogi, Log, TEXT("BOSS cpu P%d dodges the %s: rolls %.0f cm to (%.0f, %.0f)"), Me->PlayerIndex + 1, S.Kind == EKind::Sweep ? TEXT("sweep") : TEXT("slam"), OutRun, T.X, T.Y);
		return true;
	}

	float SlowMo()
	{
		if (!S.BraceBall.IsValid() || S.BraceAt < 0.0) return 0.f;
		return 0.12f;
	}

	bool Hold(AZoogiGameMode* G)
	{
		if (!G || S.G.Get() != G) return false;
		if (S.Run != ERun::Idle) return true;
		if (!S.bPending || G->Phase != EZoogiPhase::Rolling || G->RoundNumber != S.Round || TurnsLeft(G) != 1) return false;
		if (G->PhaseTime < 0.25f) return true;
		if (G->AllAtRestPublic() || G->PhaseTime > 9.f) StartRun(G);
		return true;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		static const bool bOff = Flag(TEXT("ZoogiNoBossAttack"));
		if (bOff || !G || G->bRoyaleMode || G->bComboMode) return;
		AZoogiBall* First = G->Balls.Num() > 0 ? G->Balls[0].Get() : nullptr;
		if (S.G.Get() != G || S.Seed != G->GameSeed || G->RoundNumber < S.Round || S.First.Get() != First) // b149: new balls = a new match (rematch / royale-mix round)
		{	// a new match
			if (S.Run == ERun::Sweep) ZoogiCosmosBoss::Sweep(false, 0.f);
			EndCam(G, 0.f);
			S = FState(); S.G = G; S.Seed = G->GameSeed; S.First = First;
		}
		if (!ZoogiCosmosBoss::Present(G))
		{	// never leave a turn held without the boss
			if (S.Run != ERun::Idle || S.bPending) { ZoogiCosmosBoss::Sweep(false, 0.f); S.Run = ERun::Idle; S.bPending = false; S.BraceBall.Reset(); EndCam(G, 0.3f); }
			return;
		}
		UWorld* W = G->GetWorld();
		if (S.Run != ERun::Idle) { S.RunAge += RealDt; if (S.RunAge > 12.f) { UE_LOG(LogZoogi, Warning, TEXT("BOSS attack watchdog: released the turn")); EndCam(G, 0.3f); ZoogiCosmosBoss::Sweep(false, 0.f); S.Run = ERun::Idle; S.BraceBall.Reset(); return; } }
		else S.RunAge = 0.f;
		const float GDt = RealDt * (W && W->GetWorldSettings() ? W->GetWorldSettings()->GetEffectiveTimeDilation() : 1.f);
		// brace window: a fresh tap braces the Zoogi the hand is about to hit (1.1 s real)
		if (S.BraceBall.IsValid())
		{
			const bool bDown = TapNow(W);
			if (bDown && !S.bPrevDown)
			{
				for (FHitMark& M : S.Marks) if (M.B == S.BraceBall) M.bBraced = true;
				if (AZoogiBall* Bb = S.BraceBall.Get()) { G->PublicPopup(TEXT("BRACE!"), Bb->BallLocation() + FVector(0, 0, 150.f), FLinearColor(1.f, 0.85f, 0.2f)); UE_LOG(LogZoogi, Log, TEXT("BOSS brace: P%d braced in time"), Bb->PlayerIndex + 1); }
				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.3f);
				S.BraceBall.Reset();
			}
			else if (FPlatformTime::Seconds() - S.BraceAt > 1.1) { UE_LOG(LogZoogi, Log, TEXT("BOSS brace: too late")); S.BraceBall.Reset(); }
			S.bPrevDown = bDown;
		}
		// b190: anger (score watch), the grab and the duel
		if (!S.bScInit) { S.bScInit = true; for (AZoogiBall* B : G->Balls) if (B && B->PlayerIndex >= 0 && B->PlayerIndex < 4) S.LastSc[B->PlayerIndex] = B->Score; }
		AngerFeed(G);
		if (S.bGrab) { StepGrab(G, RealDt); return; }
		if (S.bDuel) { StepDuel(G); return; }
		if (S.Run == ERun::Idle && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 0.5f)
			if (AZoogiBall* Ab = G->GetActiveBall())
			{
				if (GrabRound(G) && S.GrabRound != G->RoundNumber && Ab == (Flag(TEXT("ZoogiBossGrabTest")) ? HumanBall(G) : Leader(G))) StartGrab(G, Ab);
				else if (S.bAngerReady && Ab->PlayerIndex == S.AngerBy) StartDuel(G, Ab);
			}
		switch (S.Run)
		{
		case ERun::Sweep: StepSweep(G, GDt); return;
		case ERun::Slam: StepSlam(G, GDt); return;
		case ERun::Grace:
			S.RunT += GDt;
			if (S.RunT > 1.6f) { EndCam(G, 0.5f); S.Run = ERun::Idle; if (TestMode()) G->TakeShot(FString::Printf(TEXT("boss_after_%d"), S.Count)); UE_LOG(LogZoogi, Log, TEXT("BOSS attack over - the turn resolves")); }
			return;
		default: break;
		}
		// the warning: once per attack round, at its first aim
		if (G->Phase == EZoogiPhase::Aiming && G->TurnCounter != S.LastTurn && G->PhaseTime > 0.3f)
		{
			S.LastTurn = G->TurnCounter;
			if (!S.bPending && S.Round != G->RoundNumber && AttackRound(G)) Announce(G);
			else if (!S.bPending && S.Round != G->RoundNumber && InvasionRound(G)) Announce(G, true); // b190
			else if (S.bPending) Countdown(G);
		}
		if (S.bPending && S.Round != G->RoundNumber) { UE_LOG(LogZoogi, Log, TEXT("BOSS attack of round %d dropped (the round ended early)"), S.Round); S.bPending = false; }
		if (S.bPending && G->Phase == EZoogiPhase::Rolling)
		{	// b147a: remember who hit whom this round
			for (AZoogiBall* B : G->Balls) if (B && B->LastHitPlayer >= 0 && B->LastHitPlayer != B->PlayerIndex) S.HitBy.Add(B, B->LastHitPlayer);
			for (AZoogiOrb* O : G->Orbs) if (O && O->LastHitBy >= 0) S.OrbHitBy.Add(O, O->LastHitBy);
		}
		if (S.bPending) { S.Pulse -= RealDt; if (S.Pulse <= 0.f) { S.Pulse = 0.9f; DrawZone(G); } }
	}

	bool DuelActive() { return S.bGrab || S.bDuel; }
	bool LockOnTarget(const AZoogiGameMode* G, FVector& OutP)
	{	// b190 BOSS LOCK-ON: while the boss is in play (an attack / invasion warned or landing, a grab, a duel, anger full) shots can hit it
		if (!G || S.G.Get() != G || !ZoogiCosmosBoss::Present(G)) return false;
		if (!(S.bPending || S.Run != ERun::Idle || S.bGrab || S.bDuel || S.bAngerReady)) return false;
		OutP = Ctr(G) + FRotator(0.f, ZoogiCosmosBoss::Angle(), 0.f).Vector() * ER(G) * 1.05f + FVector(0, 0, 420.f);
		// b191 (Evan: "not just Cosmos - Frozen and Volcano Pit too"): aim at Dracoria's head / the Stone Golem's body there
		if (ZoogiDragon::Active()) OutP = ZoogiDragon::HeadLoc();
		else if (ZoogiGolem::Active()) OutP = ZoogiGolem::HeadLoc();
		else { FVector Tc; float Th; if (ZoogiCosmosBoss::TitanFocus(Tc, Th)) OutP = Tc + FVector(0.f, 0.f, Th * 0.35f); } // b216: the Tripo titan's chest
		return true;
	}
	void HitBoss(AZoogiGameMode* G, int32 By)
	{
		if (!G) return;
		++S.BossHits; S.Weak = FMath::Max(0.35f, 1.f - 0.1f * S.BossHits);
		S.Anger = FMath::Max(0.f, S.Anger - 0.04f);
		if (G->Balls.IsValidIndex(By)) if (AZoogiBall* B = G->Balls[By].Get()) { B->Score += 10; G->PublicPopup(TEXT("BOSS HIT +10"), B->BallLocation() + FVector(0, 0, 150.f), FLinearColor(1.f, 0.82f, 0.25f)); }
		if (S.BossHits == 4) if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("BOSS WEAKENED!"), TEXT("ITS NEXT ATTACK HITS SOFTER"), FLinearColor(1.f, 0.82f, 0.25f), 1.4f, 1.f);
		UE_LOG(LogZoogi, Log, TEXT("BOSS LOCK-ON hit by P%d (%d hits, attack x%.2f)"), By + 1, S.BossHits, S.Weak);
	}
	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!G || !C || S.G.Get() != G || !ZoogiCosmosBoss::Present(G) || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver) return;
		const float U = FMath::Min(C->ClipX, C->ClipY) / 1080.f, Wd = C->ClipX, Ht = C->ClipY;
		const FLinearColor Ink(0.04f, 0.03f, 0.06f, 0.9f), Gold(1.f, 0.83f, 0.36f), Cy(0.24f, 0.86f, 1.f), Pu(0.6f, 0.3f, 1.f);
		// the anger meter (top centre, under SHOWTIME)
		if (G->RoundNumber >= 2 && !Flag(TEXT("ZoogiNoBossAnger")) && !ZoogiRaid::NewHud()) // b207: in a raid the boss plate has it
		{
			const float BW = 300.f * U, BH = 16.f * U, X0 = Wd * 0.5f - BW * 0.5f, Y0 = Ht * 0.135f;
			ZoogiFunCanvasRect(C, X0 - 3.f * U, Y0 - 3.f * U, BW + 6.f * U, BH + 6.f * U, Ink);
			const float Pl = S.bAngerReady ? 0.7f + 0.3f * FMath::Sin((float)FPlatformTime::Seconds() * 9.f) : 1.f;
			ZoogiFunCanvasRect(C, X0, Y0, BW * S.Anger, BH, FLinearColor(1.f * Pl, (0.6f - 0.5f * S.Anger) * Pl, 0.1f, 1.f));
			ZoogiFunCanvasText(C, S.bAngerReady ? FString::Printf(TEXT("BOSS FURIOUS - P%d DUELS"), S.AngerBy + 1) : FString(TEXT("BOSS ANGER")), Wd * 0.5f, Y0 + BH * 0.5f, 14.f * U, FLinearColor::White, 1, 0);
		}
		if (S.bGrab)
		{
			const float BW = 620.f * U, BH = 34.f * U, X0 = Wd * 0.5f - BW * 0.5f, Y0 = Ht * 0.62f;
			for (int32 k = 0; k < 2; ++k)
			{
				const float V = FMath::Clamp(k == 0 ? S.You : S.BossM, 0.f, 1.f); const float Y = Y0 + k * (BH + 16.f * U);
				ZoogiFunCanvasRect(C, X0, Y, BW, BH, Ink);
				ZoogiFunCanvasRect(C, X0, Y, BW * V, BH, k == 0 ? Cy : Pu);
				ZoogiFunCanvasText(C, k == 0 ? TEXT("YOU") : TEXT("BOSS"), X0 - 14.f * U, Y + BH * 0.5f, 26.f * U, FLinearColor::White, 2, 0);
			}
			const float Sc = 1.f + 0.08f * FMath::Sin((float)FPlatformTime::Seconds() * 20.f);
			ZoogiFunCanvasText(C, TEXT("TAP TAP TAP!"), Wd * 0.5f, Y0 - 50.f * U, 64.f * U * Sc, Gold, 1, 0);
		}
		if (S.bDuel)
		{
			const float Cx = Wd * 0.5f, Cy2 = Ht * 0.6f;
			ZoogiFunCanvasText(C, FString::Printf(TEXT("BOSS DUEL  -  ROUND %d / 3"), FMath::Min(3, S.DuelRound + 1)), Cx, Cy2 - 90.f * U, 34.f * U, FLinearColor::White, 1, 0);
			for (int32 k = 0; k < 3; ++k)
			{
				const bool bDone = k < S.DuelRound; const bool bWon = k < S.DuelWins;
				ZoogiFunCanvasRect(C, Cx - 75.f * U + k * 55.f * U, Cy2 - 50.f * U, 40.f * U, 14.f * U, bDone ? (bWon ? Cy : FLinearColor(1.f, 0.15f, 0.1f, 1.f)) : Ink);
			}
			if (S.DuelPhase == 1) ZoogiFunCanvasText(C, TEXT("TAP!"), Cx, Cy2 + 30.f * U, 120.f * U, Gold, 1, 0);
			else if (S.DuelPhase == 0) ZoogiFunCanvasText(C, TEXT("WAIT FOR THE FLASH..."), Cx, Cy2 + 30.f * U, 40.f * U, FLinearColor(0.85f, 0.85f, 0.9f, 1.f), 1, 0);
		}
		if (FPlatformTime::Seconds() - S.MsgAt < 2.0) ZoogiFunCanvasText(C, S.Msg, Wd * 0.5f, Ht * 0.45f, 44.f * U, Gold, 1, 0);
	}

	bool Engaged(const AZoogiGameMode* G)
	{	// b155a: this round is a boss-attack round (announced / running / just landed)
		if (!G) return false;
		if (S.bPending || S.Run != ERun::Idle) return true;
		return AttackRound(G) && S.Round == G->RoundNumber;
	}

	float AngerFrac(bool* bReady, int32* By) { if (bReady) *bReady = S.bAngerReady; if (By) *By = S.AngerBy; return S.Anger; }
	bool Warning(const AZoogiGameMode* G, FString& Out)
	{	// b207: the coming attack, for the raid plate's ticker
		if (!G || S.G.Get() != G || !S.bPending) return false;
		if (S.bMini) { Out = TEXT("FIST SLAM AFTER EVERYONE'S TURN - ROLL OUT OF THE RINGS"); return true; }
		const int32 Tl = TurnsLeft(G);
		const FString Nm = S.Kind == EKind::Sweep ? ZoogiDragon::SweepName(ZoogiGolem::Active() ? TEXT("HAMMER SWEEP") : TEXT("HAND SWEEP")) : (ZoogiGolem::Active() ? FString(TEXT("GROUND SLASH")) : FString(TEXT("GROUND SLAM")));
		Out = Tl <= 1 ? FString::Printf(TEXT("%s AFTER THIS TURN!"), *Nm) : FString::Printf(TEXT("%s IN %d TURNS"), *Nm, Tl);
		return true;
	}
}
