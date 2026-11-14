// b273: PARTY MODES - see ZoogiParty.h
#include "ZoogiParty.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiRaid.h"
#include "ZoogiActionFun.h"
#include "ZoogiIceWall.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiParty
{
	namespace
	{
		EKind GKind = EKind::None;
		TWeakObjectPtr<AZoogiGameMode> GG;
		bool bSeenPlay = false, bIntro = false;
		int32 LastRound = -1, LastTurn = -1, PrevPhase = -1;
		double Next = 0.0, T0 = -1.0;
		// HOT ORB
		TWeakObjectPtr<AZoogiOrb> Bomb; int32 Fuse = 0; int32 Toucher = -1; // b274: tracked here (orbs only remember skill hits)
		// ORB STORM
		bool bTimeUp = false; int32 LastCall = -1;
		// BOSS RUSH
		int32 Stage = 0, Carry = -1;
		const EZoogiArena RushMaps[3] = { EZoogiArena::Volcanic, EZoogiArena::Frozen, EZoogiArena::Tomb };
		// TRICK COURSE
		TWeakObjectPtr<AZoogiOrb> Target; int32 Hole = -1, Shots = 0, TotalShots = 0; FVector HoleP = FVector::ZeroVector, StartP = FVector::ZeroVector; double HoleDoneAt = -1.0;

		// KING OF THE HILL - b286 (Evan: "that mode should be about building barriers and breaking through them amongst the normal chaos"):
		// the centre ring starts walled in (a FORT of ice walls - 3 knocks shatter a wall), every 3rd round the broken fort walls rise again,
		// and a Zoogi that ends its turn in or near the ring raises its own wall between itself and the nearest rival (2 walls each).
		// Breaking a wall someone else built (or the fort) = +5. Off: -ZoogiKothNoFort
		struct FKWall { TWeakObjectPtr<AZoogiIceWall> W; int32 Owner = -1; int32 Slot = -1; bool bPaid = false; };
		TArray<FKWall> KWalls; int32 KLastTurn = -1, KTurnPlayer = -1, KFortRound = -1; int64 KEff = 860000;
		const FLinearColor Gold(1.f, 0.8f, 0.25f), Red(1.f, 0.25f, 0.15f);
		FVector Ctr(AZoogiGameMode* G) { return G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector; }
		float KR(AZoogiGameMode* G) { return G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f; }
		void Callout(AZoogiGameMode* G, const FString& A, const FString& B, const FLinearColor& C) { if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(A, B, C, 2.2f, 1.1f); }
		void Kick(UPrimitiveComponent* Body, const FVector& V) { if (Body && Body->IsSimulatingPhysics()) { Body->WakeAllRigidBodies(); Body->SetPhysicsLinearVelocity(V); } }
		void Teleport(AZoogiOrb* O, const FVector& P) { if (O && O->Sphere) { O->Sphere->SetWorldLocation(P, false, nullptr, ETeleportType::TeleportPhysics); O->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); } }

		AZoogiOrb* PickBomb(AZoogiGameMode* G, AZoogiOrb* Not)
		{
			const FVector C = Ctr(G); AZoogiOrb* Best = nullptr; float Bd = 1e9f;
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O != Not && !O->bStar) { const float D = FVector::Dist2D(O->OrbLocation(), C); if (D < Bd) { Bd = D; Best = O; } }
			return Best;
		}
		void TrickHole(AZoogiGameMode* G)
		{	// the marked orb starts on one side, the hole is on the other
			static const FVector2D Holes[3] = { FVector2D(0.55f, 0.f), FVector2D(-0.35f, 0.5f), FVector2D(0.2f, -0.6f) };
			static const FVector2D Starts[3] = { FVector2D(-0.1f, 0.25f), FVector2D(0.25f, -0.15f), FVector2D(-0.3f, 0.1f) };
			const FVector C = Ctr(G); const float R = KR(G);
			HoleP = C + FVector(Holes[Hole].X * R, Holes[Hole].Y * R, 0.f);
			StartP = C + FVector(Starts[Hole].X * R, Starts[Hole].Y * R, ZoogiConst::OrbRadius + 2.f);
			if (!Target.IsValid() || Target->bOut) { AZoogiOrb* O = PickBomb(G, nullptr); Target = O; }
			Teleport(Target.Get(), StartP);
			Shots = 0; HoleDoneAt = -1.0;
			Callout(G, FString::Printf(TEXT("HOLE %d / 3"), Hole + 1), TEXT("KNOCK THE GLOWING ORB INTO THE GOLD HOLE"), Gold);
			UE_LOG(LogZoogi, Log, TEXT("PARTY trick course: hole %d at (%.0f, %.0f)"), Hole + 1, HoleP.X, HoleP.Y);
		}
	}

	void Arm(AZoogiGameMode* G, EKind K)
	{
		GKind = K; GG = G; bSeenPlay = false; bIntro = false; LastRound = -1; LastTurn = -1; PrevPhase = -1; T0 = -1.0; Next = 0.0;
		Bomb.Reset(); Fuse = 0; bTimeUp = false; LastCall = -1; Target.Reset(); Hole = -1; Shots = 0; TotalShots = 0;
		Stage = 0; Carry = -1; // a new pick from the menu always starts at titan 1
		KWalls.Reset(); KLastTurn = -1; KTurnPlayer = -1; KFortRound = -1;
		UE_LOG(LogZoogi, Log, TEXT("PARTY armed: mode %d"), (int32)K);
	}
	EKind Kind() { return GKind; }
	int32 CarryLives() { return GKind == EKind::BossRush ? Carry : -1; }

	bool BossRushNext(AZoogiGameMode* G, bool bWin, int32 LivesLeft)
	{
		if (GKind != EKind::BossRush || !G) return false;
		if (!bWin || Stage >= 2)
		{
			Callout(G, bWin ? TEXT("BOSS RUSH CLEARED!") : TEXT("BOSS RUSH OVER"), FString::Printf(TEXT("%d OF 3 TITANS BEATEN"), bWin ? 3 : Stage), bWin ? Gold : Red);
			UE_LOG(LogZoogi, Log, TEXT("PARTY boss rush ends: %s at stage %d"), bWin ? TEXT("cleared") : TEXT("failed"), Stage + 1);
			GKind = EKind::None; Stage = 0; Carry = -1;
			return false;
		}
		++Stage; Carry = FMath::Max(1, LivesLeft);
		UE_LOG(LogZoogi, Log, TEXT("PARTY boss rush: titan %d / 3 on arena %d, %d team lives carried"), Stage + 1, (int32)RushMaps[Stage], Carry);
		G->MenuAction(FName(TEXT("Arena")), (int32)RushMaps[Stage]);
		ZoogiRaid::Arm(G);
		G->MenuAction(FName(TEXT("Start")), 0);
		Callout(G, FString::Printf(TEXT("BOSS RUSH: TITAN %d / 3"), Stage + 1), FString::Printf(TEXT("%d TEAM LIVES LEFT"), Carry), Gold);
		return true;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		{ static bool bCl = false; if (!bCl) { bCl = true; int32 Pk = 0; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiParty="), Pk) && Pk > 0 && Pk <= 5) Arm(G, (EKind)Pk); } } // preview: -ZoogiParty=1..5
		if (GKind == EKind::None) return;
		UWorld* W = G->GetWorld(); if (!W) return;
		const EZoogiPhase Ph = G->Phase;
		const bool bPlay = Ph == EZoogiPhase::Aiming || Ph == EZoogiPhase::Rolling || Ph == EZoogiPhase::Resolving;
		if (bPlay) bSeenPlay = true;
		if (bSeenPlay && Ph == EZoogiPhase::Menu && GKind != EKind::BossRush) { UE_LOG(LogZoogi, Log, TEXT("PARTY mode %d over (back to the menu)"), (int32)GKind); GKind = EKind::None; return; }
		if (!bPlay) { PrevPhase = (int32)Ph; return; }
		const double Now = FPlatformTime::Seconds();
		const FVector C = Ctr(G); const float R = KR(G);
		if (!bIntro)
		{
			bIntro = true; T0 = Now; LastRound = G->RoundNumber; LastTurn = G->TurnCounter;
			if (GKind == EKind::Koth) Callout(G, TEXT("KING OF THE HILL"), TEXT("BREAK INTO THE FORT - HOLD THE RING = +10 - BUILD WALLS"), Gold);
			else if (GKind == EKind::HotOrb) Callout(G, TEXT("HOT ORB"), TEXT("THE RED ORB BLOWS UP IN 4 TURNS - DON'T TOUCH IT LAST"), Red);
			else if (GKind == EKind::OrbStorm) Callout(G, TEXT("ORB STORM"), TEXT("90 SECONDS - ORBS KEEP RAINING IN"), FLinearColor(0.5f, 0.85f, 1.f));
			else if (GKind == EKind::BossRush && Stage == 0) Callout(G, TEXT("BOSS RUSH: TITAN 1 / 3"), TEXT("THREE TITANS, ONE POOL OF TEAM LIVES"), Gold);
			else if (GKind == EKind::TrickCourse) { Hole = 0; TrickHole(G); }
		}
		if (GKind == EKind::Koth)
		{
			const float ZoneR = R * 0.3f;
			if (Now > Next) { Next = Now + 1.4; AZoogiFX::SpawnMark(W, C + FVector(0, 0, 5.f), Gold, ZoneR, 1.3f); }
			static const bool bNoFort = FParse::Param(FCommandLine::Get(), TEXT("ZoogiKothNoFort"));
			if (!bNoFort)
			{
				const float FortR = ZoneR + 90.f, Len = AZoogiIceWall::Tune().Length;
				const int32 NF = FMath::Clamp(FMath::CeilToInt(2.f * PI * FortR / (Len * 0.92f)), 5, 12);
				// the FORT: raised at the start, broken slots rise again every 3rd round
				if (KFortRound < 0 || (G->RoundNumber != KFortRound && (G->RoundNumber - 1) % 3 == 0 && Ph == EZoogiPhase::Aiming))
				{
					const bool bFirst = KFortRound < 0; KFortRound = G->RoundNumber; int32 Raised = 0;
					for (int32 k = 0; k < NF; ++k)
					{
						bool bLive = false;
						for (const FKWall& Kw : KWalls) if (Kw.Slot == k && Kw.W.IsValid() && Kw.W->LiveCount() > 0) bLive = true;
						if (bLive) continue;
						const float A = 2.f * PI * k / NF;
						const FVector Rad(FMath::Cos(A), FMath::Sin(A), 0.f);
						if (AZoogiIceWall* Nw = AZoogiIceWall::SpawnWall(W, C + Rad * FortR + FVector(0, 0, 2.f), Rad, -1, ++KEff)) { KWalls.Add({ Nw, -1, k, false }); ++Raised; }
					}
					if (Raised > 0) { Callout(G, bFirst ? TEXT("THE FORT IS UP") : TEXT("THE FORT REBUILDS"), TEXT("3 KNOCKS SHATTER A WALL - BREAK IN!"), FLinearColor(0.5f, 0.85f, 1.f)); ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.6f, 0.7f); }
					UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH fort: %d / %d walls raised (round %d, r%.0f)"), Raised, NF, G->RoundNumber, FortR);
				}
				// BUILD: a Zoogi that ends its turn in or near the ring raises its own wall toward the nearest rival
				if (Ph == EZoogiPhase::Rolling && G->GetActiveBall()) KTurnPlayer = G->GetActiveBall()->PlayerIndex;
				if (G->TurnCounter != KLastTurn)
				{
					const int32 Was = KTurnPlayer; KLastTurn = G->TurnCounter; KTurnPlayer = -1;
					AZoogiBall* Bb = G->Balls.IsValidIndex(Was) ? G->Balls[Was] : nullptr;
					if (Bb && !Bb->bOut && FVector::Dist2D(Bb->BallLocation(), C) < ZoneR * 1.6f)
					{
						AZoogiBall* Rv = nullptr; float Rd = 1e9f;
						for (AZoogiBall* O : G->Balls) if (O && O != Bb && !O->bOut) { const float D = FVector::Dist2D(O->BallLocation(), Bb->BallLocation()); if (D < Rd) { Rd = D; Rv = O; } }
						FVector Dir = Rv ? Rv->BallLocation() - Bb->BallLocation() : Bb->BallLocation() - C; Dir.Z = 0.f; if (!Dir.Normalize()) Dir = FVector::ForwardVector;
						int32 Mine = 0; for (const FKWall& Kw : KWalls) if (Kw.Owner == Was && Kw.W.IsValid() && Kw.W->LiveCount() > 0) ++Mine;
						if (Mine >= 2) for (FKWall& Kw : KWalls) if (Kw.Owner == Was && Kw.W.IsValid() && Kw.W->LiveCount() > 0) { Kw.bPaid = true; Kw.W->Destroy(); break; } // the oldest goes
						if (AZoogiIceWall* Nw = AZoogiIceWall::SpawnWall(W, Bb->BallLocation() + Dir * 210.f, Dir, Was, ++KEff))
						{
							KWalls.Add({ Nw, Was, -1, false });
							G->PublicPopup(TEXT("BARRIER UP!"), Bb->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.5f, 0.85f, 1.f));
							UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH: P%d built a wall toward %s"), Was + 1, Rv ? *Rv->Def.Name : TEXT("the edge"));
						}
					}
				}
				// BREAK: +5 for shattering a wall you didn't build
				for (FKWall& Kw : KWalls)
				{
					if (Kw.bPaid) continue;
					AZoogiIceWall* Iw = Kw.W.Get();
					if (Iw && Iw->LiveCount() > 0) continue;
					Kw.bPaid = true;
					const int32 By = Iw ? Iw->LastHitP : -1;
					if (By >= 0 && By != Kw.Owner && G->Balls.IsValidIndex(By) && G->Balls[By])
					{
						G->Balls[By]->Score += 5;
						G->PublicPopup(Kw.Owner < 0 ? TEXT("FORT BREACHED +5") : TEXT("WALL SMASHED +5"), G->Balls[By]->BallLocation() + FVector(0, 0, 190.f), Gold);
						UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH: P%d broke %s (+5, score %d)"), By + 1, Kw.Owner < 0 ? TEXT("a fort wall") : *FString::Printf(TEXT("P%d's wall"), Kw.Owner + 1), G->Balls[By]->Score);
					}
				}
			}
			if (G->RoundNumber != LastRound)
			{	// a round just ended: everyone in the centre ring scores
				LastRound = G->RoundNumber;
				for (AZoogiBall* B : G->Balls)
				{
					if (!B || B->bOut || FVector::Dist2D(B->BallLocation(), C) > ZoneR) continue;
					B->Score += 10;
					G->PublicPopup(TEXT("+10 KING OF THE HILL"), B->BallLocation() + FVector(0, 0, 190.f), Gold);
					UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH: P%d holds the centre (+10, score %d)"), B->PlayerIndex + 1, B->Score);
				}
			}
		}
		else if (GKind == EKind::HotOrb)
		{
			if (!Bomb.IsValid()) { Bomb = PickBomb(G, nullptr); Fuse = 4; Toucher = -1; }
			AZoogiOrb* O = Bomb.Get(); if (!O) return;
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), O->OrbLocation()) < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 25.f) Toucher = B->PlayerIndex; // b274: who touched it last
			if (G->TurnCounter != LastTurn)
			{
				LastTurn = G->TurnCounter; --Fuse;
				if (Fuse > 0) G->PublicPopup(FString::Printf(TEXT("BOMB: %d"), Fuse), O->OrbLocation() + FVector(0, 0, 170.f), Red);
			}
			if (Now > Next) { Next = Now + (Fuse <= 1 ? 0.18 : 0.45); AZoogiFX::Spawn(W, O->OrbLocation(), Red, 80.f, 0.3f, false, 12.f); }
			if (O->bOut)
			{	// knocked out of the arena: the one who did it gets the points, a new bomb lights up
				const int32 By = Toucher >= 0 ? Toucher : O->LastHitBy;
				if (G->Balls.IsValidIndex(By) && G->Balls[By]) { G->Balls[By]->Score += 20; G->PublicPopup(TEXT("BOMB CLEARED +20"), G->Balls[By]->BallLocation() + FVector(0, 0, 190.f), Gold); }
				Bomb = PickBomb(G, O); Fuse = 4; Toucher = -1;
			}
			else if (Fuse <= 0)
			{
				const FVector P = O->OrbLocation();
				for (AZoogiBall* B : G->Balls) if (B && !B->bOut) { FVector D = B->BallLocation() - P; D.Z = 0.f; const float Ds = D.Size(); if (Ds < 600.f) Kick(B->Sphere, D.GetSafeNormal() * (1600.f * (1.f - Ds / 600.f) + 400.f) + FVector(0, 0, 380.f)); }
				for (AZoogiOrb* Q : G->Orbs) if (Q && !Q->bOut && Q != O) { FVector D = Q->OrbLocation() - P; D.Z = 0.f; const float Ds = D.Size(); if (Ds < 600.f) Kick(Q->Sphere, D.GetSafeNormal() * (1400.f * (1.f - Ds / 600.f) + 300.f) + FVector(0, 0, 300.f)); }
				const int32 Last = Toucher >= 0 ? Toucher : O->LastHitBy;
				if (G->Balls.IsValidIndex(Last) && G->Balls[Last] && !G->Balls[Last]->bOut)
				{	// whoever touched it last is blasted toward the nearest edge
					AZoogiBall* L = G->Balls[Last]; FVector Out = L->BallLocation() - C; Out.Z = 0.f; if (!Out.Normalize()) Out = FVector::ForwardVector;
					Kick(L->Sphere, Out * 2600.f + FVector(0, 0, 320.f));
					Callout(G, TEXT("BOOM!"), FString::Printf(TEXT("P%d %s TOUCHED IT LAST"), Last + 1, *L->Def.Name.ToUpper()), Red);
				}
				AZoogiFX::Spawn(W, P, Red, 600.f, 0.6f, true, 16.f);
				AZoogiFX::Spawn(W, P, FLinearColor(1.f, 0.9f, 0.6f), 220.f, 0.3f, false, 18.f);
				ZoogiUtil::PlaySfx(G, TEXT("Explosion"), 1.f, 0.8f);
				UE_LOG(LogZoogi, Log, TEXT("PARTY hot orb: boom - P%d touched it last"), Last + 1);
				Bomb = PickBomb(G, O); Fuse = 4; Toucher = -1;
			}
		}
		else if (GKind == EKind::OrbStorm)
		{
			const float Left = 90.f - (float)(Now - T0);
			const int32 Mark = Left <= 10.f ? 10 : Left <= 30.f ? 30 : Left <= 60.f ? 60 : 90;
			if (Mark != LastCall && Mark < 90) { LastCall = Mark; Callout(G, FString::Printf(TEXT("%d SECONDS LEFT"), Mark), TEXT("ORB STORM"), FLinearColor(0.5f, 0.85f, 1.f)); }
			if (Left <= 0.f && !bTimeUp)
			{
				bTimeUp = true; Callout(G, TEXT("TIME!"), TEXT("ORB STORM OVER"), Gold);
				UE_LOG(LogZoogi, Log, TEXT("PARTY orb storm: time up"));
				TWeakObjectPtr<AZoogiGameMode> WG = G;
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG](float) -> bool { if (AZoogiGameMode* Gm = WG.Get()) Gm->PublicFinishGame(); return false; }), 2.f);
				return;
			}
			int32 Alive = 0; for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) ++Alive;
			if (!bTimeUp && Now > Next && Alive < 60)
			{
				Next = Now + 2.5;
				const float A = FMath::FRandRange(0.f, 2.f * PI), Rr = FMath::FRandRange(0.2f, 0.75f) * R;
				const FVector P = C + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 900.f);
				if (G->PublicSpawnOrb(P, FMath::FRand() < 0.2f)) AZoogiFX::SpawnMark(W, FVector(P.X, P.Y, C.Z + 5.f), FLinearColor(0.5f, 0.85f, 1.f), 90.f, 1.2f);
			}
		}
		else if (GKind == EKind::TrickCourse && Hole >= 0)
		{
			AZoogiOrb* O = Target.Get();
			if (Now > Next) { Next = Now + 0.6; AZoogiFX::SpawnMark(W, HoleP + FVector(0, 0, 5.f), Gold, 170.f, 0.7f); if (O) AZoogiFX::Spawn(W, O->OrbLocation(), FLinearColor(0.5f, 1.f, 0.6f), 70.f, 0.3f, false, 10.f); }
			if (PrevPhase == (int32)EZoogiPhase::Aiming && Ph == EZoogiPhase::Rolling) { ++Shots; ++TotalShots; }
			if (O && O->bOut) { ++Shots; ++TotalShots; Target = G->PublicSpawnOrb(StartP, false); O = Target.Get(); G->PublicPopup(TEXT("OUT! +1 SHOT"), StartP + FVector(0, 0, 160.f), Red); } // a fresh marked orb at the start
			if (O && HoleDoneAt < 0.0 && FVector::Dist2D(O->OrbLocation(), HoleP) < 170.f && O->Sphere && O->Sphere->GetPhysicsLinearVelocity().Size() < 80.f)
			{
				HoleDoneAt = Now;
				Callout(G, Shots <= 1 ? TEXT("HOLE IN ONE!") : FString::Printf(TEXT("HOLE IN %d"), Shots), FString::Printf(TEXT("%d SHOTS SO FAR"), TotalShots), Gold);
				AZoogiFX::Spawn(W, HoleP, Gold, 300.f, 0.6f, true, 14.f); ZoogiUtil::PlaySfx(G, TEXT("Win"), 0.7f, 1.2f);
				UE_LOG(LogZoogi, Log, TEXT("PARTY trick course: hole %d in %d (total %d)"), Hole + 1, Shots, TotalShots);
			}
			if (HoleDoneAt > 0.0 && Now - HoleDoneAt > 2.5)
			{
				if (Hole >= 2)
				{
					Callout(G, FString::Printf(TEXT("COURSE DONE: %d SHOTS"), TotalShots), TotalShots <= 5 ? TEXT("PAR OR BETTER!") : TEXT("PAR IS 5 - TRY AGAIN"), Gold);
					UE_LOG(LogZoogi, Log, TEXT("PARTY trick course done in %d shots"), TotalShots);
					Hole = -1;
					TWeakObjectPtr<AZoogiGameMode> WG = G;
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG](float) -> bool { if (AZoogiGameMode* Gm = WG.Get()) Gm->PublicFinishGame(); return false; }), 3.f);
				}
				else { ++Hole; TrickHole(G); }
			}
		}
		PrevPhase = (int32)Ph;
	}
}
