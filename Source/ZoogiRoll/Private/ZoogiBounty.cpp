// b179: BOUNTY + TURN CHALLENGES - see ZoogiBounty.h
#include "ZoogiBounty.h"
#include "ZoogiAwardBanner.h" // b269
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiActionFun.h"
#include "ZoogiSkills.h"
#include "ZoogiRedirect.h"
#include "ZoogiHaptics.h"
#include "ZoogiRaid.h"
#include "ZoogiBossAttack.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCosmosBoss.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);

namespace ZoogiBounty
{
	namespace
	{
		enum class ECh : uint8 { None, TwoOrbs, TwoRivals, RivalOff, OrbThenRival, ThreeOrbs, BountyAndOrb, RaidDouble, RaidStar, RaidBank, RaidInterrupt, RaidArmor, RaidSafe };
		struct FS
		{
			TWeakObjectPtr<AZoogiGameMode> G;
			int32 Turn = -1, Shooter = -1;
			bool bArmed = false;
			TWeakObjectPtr<AZoogiBall> Bounty; bool bBountyDone = false, bFirstSeen = false;
			ECh Ch = ECh::None; bool bChDone = false;
			TArray<TWeakObjectPtr<AZoogiOrb>> OrbsIn; TArray<TWeakObjectPtr<AZoogiBall>> RivalsIn;
			TSet<TWeakObjectPtr<AZoogiBall>> RivalsHit; bool bOrbTouched = false, bOrbThenRival = false;
			int32 OrbsOut = 0, RivalsOut = 0;
			float CardAge = 0.f;
			// b200 raid challenges
			int32 RaidHits = 0; bool bStarHit = false, bBank = false, bInterrupted = false; FVector LaunchP = FVector::ZeroVector, LaunchD = FVector::ZeroVector;
		};
		FS S;
		const FLinearColor Gold(1.f, 0.78f, 0.16f), Yel(1.f, 0.84f, 0.16f), Ink(0.03f, 0.03f, 0.05f, 0.94f);
		constexpr int32 BountyPts = 50, ChallengePts = 30; constexpr float ChallengeMeter = 25.f;

		bool BountyOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBounty")); return b; }
		bool ChallengeOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTurnChallenge")); return b; }
		bool ModeOk(const AZoogiGameMode* G) { return G && !G->IsRoyaleMatch(); }

		FString ChText(ECh C)
		{
			switch (C)
			{
			case ECh::TwoOrbs: return TEXT("KNOCK 2 ORBS OUT IN ONE ROLL");
			case ECh::ThreeOrbs: return TEXT("KNOCK 3 ORBS OUT IN ONE ROLL");
			case ECh::TwoRivals: return TEXT("HIT 2 RIVALS IN ONE ROLL");
			case ECh::RivalOff: return TEXT("KNOCK A RIVAL OFF THE ARENA");
			case ECh::OrbThenRival: return TEXT("HIT AN ORB, THEN A RIVAL");
			case ECh::BountyAndOrb: return TEXT("HIT THE BOUNTY FIRST + SCORE AN ORB");
			case ECh::RaidDouble: return TEXT("DOUBLE TAP: HIT THE WEAK POINT TWICE IN ONE ROLL");
			case ECh::RaidStar: return TEXT("STAR STRIKE: KNOCK A GOLD STAR ORB INTO THE WEAK POINT");
			case ECh::RaidBank: return TEXT("BANK SHOT: BOUNCE OFF SOMETHING, THEN HIT THE WEAK POINT");
			case ECh::RaidInterrupt: return TEXT("INTERRUPT: HIT THE WEAK POINT BEFORE ITS ATTACK LANDS");
			case ECh::RaidArmor: return TEXT("ARMOR BREAK: SMASH BOTH ICE SHARDS");
			case ECh::RaidSafe: return TEXT("SAFE ZONE: HIT THE WEAK POINT + STOP OUTSIDE THE RED ZONE");
			default: return FString();
			}
		}
		bool Done(ECh C)
		{
			switch (C)
			{
			case ECh::TwoOrbs: return S.OrbsOut >= 2;
			case ECh::ThreeOrbs: return S.OrbsOut >= 3;
			case ECh::TwoRivals: return S.RivalsHit.Num() >= 2;
			case ECh::RivalOff: return S.RivalsOut >= 1;
			case ECh::OrbThenRival: return S.bOrbThenRival;
			case ECh::BountyAndOrb: return S.bBountyDone && S.OrbsOut >= 1;
			case ECh::RaidDouble: return S.RaidHits >= 2;
			case ECh::RaidStar: return S.bStarHit;
			case ECh::RaidBank: return S.bBank;
			case ECh::RaidInterrupt: return S.bInterrupted;
			case ECh::RaidArmor: return ZoogiRaid::ArmorBroken() >= 2;
			case ECh::RaidSafe: { const AZoogiGameMode* G = S.G.Get(); const AZoogiBall* Me = G && G->Balls.IsValidIndex(S.Shooter) ? G->Balls[S.Shooter].Get() : nullptr;
				return G && Me && !Me->bOut && S.RaidHits >= 1 && G->Phase == EZoogiPhase::Resolving && !ZoogiBossAttack::InDangerZone(G, Me->BallLocation()); }
			default: return false;
			}
		}
		void Reward(AZoogiGameMode* G, AZoogiBall* B, int32 Pts, const FString& Big, const FString& Sub, float Meter, const FLinearColor& Col)
		{
			if (!G || !B) return;
			B->Score += Pts;
			if (Meter > 0.f) if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->AddAwakenMeter(G, B->PlayerIndex, Meter, TEXT("turn challenge"));
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(Big, Sub, Col, 1.6f, 1.15f);
			G->PublicPopup(FString::Printf(TEXT("+%d"), Pts), B->BallLocation() + FVector(0.f, 0.f, 190.f), Col);
			ZoogiUtil::PlaySfx(G, TEXT("Score"), 1.f, 1.15f);
			ZoogiHaptics::Kick(0.45f, G->GetWorld(), TEXT("bounty"));
			if (!G->ShotDir.IsEmpty()) { static int32 Nr = 0; if (Nr++ < 6) G->TakeShotDelayed(FString::Printf(TEXT("bounty_reward_%d"), Nr), 0.35f); }
		}
		bool IsRaid(ECh C) { return C >= ECh::RaidDouble; }
		void RaidReward(AZoogiGameMode* G, AZoogiBall* B)
		{	// b200 (Evan: "add all those suggested raid challenges"): each one pays off in the raid itself
			if (!G || !B) return;
			const int32 Pi = B->PlayerIndex; FString Sub;
			auto Sp = [&](int32 N) { if (UZoogiCardSubsystem* Cs = UZoogiCardSubsystem::Get(G)) Cs->AddEnergyNow(Pi, N); };
			switch (S.Ch)
			{
			case ECh::RaidDouble: ZoogiRaid::BonusDamage(G, Pi, 6, TEXT("double tap bonus")); Sp(2); Sub = TEXT("+6 BONUS DAMAGE  +2 SP"); break;
			case ECh::RaidStar: { const int32 Ci = FMath::Clamp(Pi, 0, 3); if (G->AwakenChargeOn()) G->AwakenCharge[Ci] += 1; Sub = TEXT("STAR ORBS HIT x2.5  +1 CRYSTAL"); break; }
			case ECh::RaidBank: ZoogiRaid::BonusDamage(G, Pi, 8, TEXT("bank shot crit")); Sp(2); Sub = TEXT("CRIT! +8 BONUS DAMAGE  +2 SP"); break;
			case ECh::RaidInterrupt: Sp(3); Sub = TEXT("ATTACK CANCELLED  +3 SP"); break;
			case ECh::RaidArmor: ZoogiRaid::Expose(G, FMath::Max(1, G->Balls.Num())); Sub = TEXT("WEAK POINT EXPOSED - THE TEAM HITS x2 NEXT TURN"); break;
			case ECh::RaidSafe: ZoogiRaid::BonusDamage(G, Pi, 4, TEXT("safe zone bonus")); Sp(2); Sub = TEXT("DODGED!  +4 BONUS DAMAGE  +2 SP"); break;
			default: break;
			}
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) { F->PublicCallout(TEXT("RAID CHALLENGE CLEAR!"), Sub, FLinearColor(0.35f, 0.95f, 0.55f), 1.8f, 1.15f); F->AddAwakenMeter(G, Pi, ChallengeMeter, TEXT("raid challenge")); }
			ZoogiUtil::PlaySfx(G, TEXT("Score"), 1.f, 1.15f);
			ZoogiHaptics::Kick(0.5f, G->GetWorld(), TEXT("raid challenge"));
			if (!G->ShotDir.IsEmpty()) { static int32 Nr = 0; if (Nr++ < 6) G->TakeShotDelayed(FString::Printf(TEXT("raid_challenge_%d"), Nr), 0.35f); }
		}
		void Finish(AZoogiGameMode* G)
		{
			if (S.Ch == ECh::RaidArmor) ZoogiRaid::ClearArmor();	// the turn is over: was the challenge done? (success only - a miss says nothing)
			if (!S.bArmed) return;
			S.bArmed = false;
			AZoogiBall* B = G && G->Balls.IsValidIndex(S.Shooter) ? G->Balls[S.Shooter] : nullptr;
			UE_LOG(LogZoogi, Log, TEXT("TURN CHALLENGE P%d \"%s\": %s (orbs %d, rivals hit %d, rivals off %d)"), S.Shooter + 1, *ChText(S.Ch), S.bChDone ? TEXT("done") : TEXT("missed"), S.OrbsOut, S.RivalsHit.Num(), S.RivalsOut);
			(void)B;
		}
		void Arm(AZoogiGameMode* G)
		{
			S.Turn = G->TurnCounter; S.Shooter = G->ActivePlayer; S.bArmed = false;
			S.Bounty.Reset(); S.bBountyDone = S.bFirstSeen = false; S.Ch = ECh::None; S.bChDone = false;
			S.OrbsIn.Reset(); S.RivalsIn.Reset(); S.RivalsHit.Reset(); S.bOrbTouched = S.bOrbThenRival = false; S.OrbsOut = S.RivalsOut = 0; S.CardAge = 0.f;
			S.RaidHits = 0; S.bStarHit = S.bBank = S.bInterrupted = false; S.LaunchD = FVector::ZeroVector;
			AZoogiBall* Me = G->GetActiveBall();
			if (!Me || Me->bCPU || !ModeOk(G)) return; // human turns only (single player): the CPUs play without it
			for (AZoogiBall* Bz : G->Balls) if (Bz && Bz != Me && !Bz->bOut && !G->IsAlly(Me, Bz)) S.RivalsIn.Add(Bz);
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) S.OrbsIn.Add(O);
			FRandomStream R(HashCombine(GetTypeHash(G->TurnCounter * 7717), GetTypeHash(G->RoundNumber)));
			if (BountyOn() && S.RivalsIn.Num() > 0)
			{	// the bounty is on the score leader (ties: a random one of them)
				int32 Top = -1; TArray<AZoogiBall*> Best;
				for (const TWeakObjectPtr<AZoogiBall>& W : S.RivalsIn) if (AZoogiBall* Bz = W.Get()) { if (Bz->Score > Top) { Top = Bz->Score; Best.Reset(); } if (Bz->Score == Top) Best.Add(Bz); }
				if (Best.Num()) S.Bounty = Best[R.RandRange(0, Best.Num() - 1)];
			}
			if (ChallengeOn() && ZoogiRaid::Active() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidOldChallenges")))
			{	// b200 RAID: challenges that teach the tricks to beating the boss (not orb knockouts)
				TArray<ECh> Pool = { ECh::RaidDouble, ECh::RaidBank, ECh::RaidArmor };
				bool bStar = false; for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->bStar) bStar = true;
				if (bStar) Pool.Add(ECh::RaidStar);
				if (ZoogiBossAttack::Pending(G)) { Pool.Add(ECh::RaidInterrupt); Pool.Add(ECh::RaidInterrupt); Pool.Add(ECh::RaidSafe); Pool.Add(ECh::RaidSafe); }
				FString Force; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiRaidChallenge="), Force)) for (const ECh Cc : { ECh::RaidDouble, ECh::RaidStar, ECh::RaidBank, ECh::RaidInterrupt, ECh::RaidArmor, ECh::RaidSafe }) if (ChText(Cc).StartsWith(Force.ToUpper())) Pool = { Cc };
				S.Ch = Pool[R.RandRange(0, Pool.Num() - 1)];
				if (S.Ch == ECh::RaidArmor) ZoogiRaid::SpawnArmor(G);
			}
			else if (ChallengeOn())
			{
				TArray<ECh> Pool = { ECh::TwoOrbs, ECh::OrbThenRival };
				if (S.RivalsIn.Num() >= 1) Pool.Add(ECh::RivalOff);
				if (S.RivalsIn.Num() >= 2) Pool.Add(ECh::TwoRivals);
				if (G->RoundNumber >= 2 && S.OrbsIn.Num() >= 6) Pool.Add(ECh::ThreeOrbs);
				if (S.Bounty.IsValid()) Pool.Add(ECh::BountyAndOrb);
				S.Ch = Pool[R.RandRange(0, Pool.Num() - 1)];
			}
			S.bArmed = S.Bounty.IsValid() || S.Ch != ECh::None;
			if (S.bArmed) UE_LOG(LogZoogi, Log, TEXT("BOUNTY P%d turn %d: bounty on %s, challenge \"%s\""), S.Shooter + 1, S.Turn, S.Bounty.IsValid() ? *S.Bounty->Def.Name : TEXT("-"), *ChText(S.Ch));
			if (S.bArmed && !G->ShotDir.IsEmpty()) { static int32 Na = 0; if (Na++ < 4) G->TakeShotDelayed(FString::Printf(TEXT("bounty_turn_%d"), Na), 0.9f); }
		}
	}

	void Reset() { S = FS(); }

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		if (S.G.Get() != G) { Reset(); S.G = G; }
		if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver) { if (S.bArmed) Finish(G); S.Turn = -1; return; }
		if (G->Phase == EZoogiPhase::Aiming && G->TurnCounter != S.Turn) { Finish(G); Arm(G); }
		if (!S.bArmed) return;
		S.CardAge += RealDt;
		// only this turn's own play counts (a round reset clears the orbs - that is not a 2-orb roll)
		const bool bLive = G->TurnCounter == S.Turn && G->ActivePlayer == S.Shooter && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || (G->Phase == EZoogiPhase::Resolving && G->PhaseTime < 1.f));
		if (!bLive) return;
		// orbs / rivals that left the arena during this turn
		int32 Oo = 0; for (const TWeakObjectPtr<AZoogiOrb>& W : S.OrbsIn) if (const AZoogiOrb* O = W.Get()) if (O->bOut) ++Oo;
		int32 Ro = 0; for (const TWeakObjectPtr<AZoogiBall>& W : S.RivalsIn) if (const AZoogiBall* Bz = W.Get()) if (Bz->bOut) ++Ro;
		S.OrbsOut = FMath::Max(S.OrbsOut, Oo); S.RivalsOut = FMath::Max(S.RivalsOut, Ro);
		if (G->Phase == EZoogiPhase::Rolling && S.LaunchD.IsNearlyZero())
			if (const AZoogiBall* Me = G->Balls.IsValidIndex(S.Shooter) ? G->Balls[S.Shooter].Get() : nullptr)
				if (Me->GetBallVelocity().Size2D() > 150.f) { S.LaunchP = Me->BallLocation(); S.LaunchD = Me->GetBallVelocity().GetSafeNormal2D(); } // b200 BANK SHOT: the launch line
		if (!S.bChDone && S.Ch != ECh::None && Done(S.Ch))
		{
			S.bChDone = true;
			AZoogiBall* B = G->Balls.IsValidIndex(S.Shooter) ? G->Balls[S.Shooter] : nullptr;
			if (IsRaid(S.Ch)) RaidReward(G, B); else
			Reward(G, B, ChallengePts, TEXT("CHALLENGE COMPLETE!"), FString::Printf(TEXT("%s  -  +%d  +%d%% AWAKEN"), *ChText(S.Ch), ChallengePts, FMath::RoundToInt(ChallengeMeter)), ChallengeMeter, FLinearColor(0.35f, 0.95f, 0.55f));
			UE_LOG(LogZoogi, Log, TEXT("TURN CHALLENGE P%d done: %s"), S.Shooter + 1, *ChText(S.Ch));
		}
	}

	void OnRaidHit(AZoogiGameMode* G, int32 Player, const TCHAR* What, const FVector& At)
	{
		if (!G || !S.bArmed || Player != S.Shooter || G->TurnCounter != S.Turn || !IsRaid(S.Ch)) return;
		const FString Wt(What);
		if (Wt.Contains(TEXT("bonus")) || Wt.Contains(TEXT("crit")) || Wt == TEXT("SHOWTIME")) return; // challenge payouts / Awaken are not shots
		++S.RaidHits;
		if (Wt == TEXT("star orb")) S.bStarHit = true;
		if (Wt == TEXT("Zoogi slam") && !S.LaunchD.IsNearlyZero())
		{	// not on the launch line = it bounced (rim, rock, a rival, a REACTION) on the way
			const FVector ToHit = FVector(At.X - S.LaunchP.X, At.Y - S.LaunchP.Y, 0.f).GetSafeNormal();
			const float Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToHit, S.LaunchD), -1.f, 1.f)));
			if (Deg > 30.f) S.bBank = true;
			UE_LOG(LogZoogi, Log, TEXT("RAID CHALLENGE: P%d hit %.0f deg off the launch line%s"), Player + 1, Deg, S.bBank ? TEXT(" - BANK SHOT") : TEXT(""));
		}
		if (S.Ch == ECh::RaidInterrupt && ZoogiBossAttack::Pending(G)) { ZoogiBossAttack::Interrupt(G, Player); S.bInterrupted = true; }
	}

	void OnContact(AZoogiGameMode* G, AZoogiBall* Shooter, AActor* Other)
	{
		if (!G || !S.bArmed || !Shooter || Shooter->PlayerIndex != S.Shooter || G->Phase != EZoogiPhase::Rolling || !Other) return;
		AZoogiBall* Ob = Cast<AZoogiBall>(Other);
		const bool bOrb = Cast<AZoogiOrb>(Other) != nullptr;
		if (Ob && G->IsAlly(Shooter, Ob)) return;
		if (!S.bFirstSeen)
		{
			S.bFirstSeen = true;
			if (Ob && S.Bounty.IsValid() && Ob == S.Bounty.Get() && !S.bBountyDone)
			{
				S.bBountyDone = true;
				Reward(G, Shooter, BountyPts, TEXT("BOUNTY CLAIMED!"), FString::Printf(TEXT("FIRST HIT ON %s  -  +%d"), *Ob->Def.Name.ToUpper(), BountyPts), 0.f, Gold);
				UE_LOG(LogZoogi, Log, TEXT("BOUNTY P%d claimed on %s"), Shooter->PlayerIndex + 1, *Ob->Def.Name);
			}
		}
		if (bOrb) S.bOrbTouched = true;
		if (Ob) { S.RivalsHit.Add(Ob); if (S.bOrbTouched) S.bOrbThenRival = true; }
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (ZoogiAwardBanner::Showing()) return; // b269 (Evan: nothing covered): the TURN CHALLENGE waits until the round award card is gone
		if (!G || !C || !PC || !S.bArmed || S.G.Get() != G) return;
		if (G->Phase != EZoogiPhase::Aiming && G->Phase != EZoogiPhase::Rolling) return;
		if (ZoogiSkills::Picking() || ZoogiRedirect::Picking()) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const bool bPort = Ht > Wd;
		const float Tm = G->GetWorld() ? G->GetWorld()->GetRealTimeSeconds() : 0.f;
		// the crown over the bounty
		if (AZoogiBall* Bt = S.Bounty.Get()) if (!Bt->bOut && !S.bBountyDone)
		{
			FVector2D Sp;
			if (PC->ProjectWorldLocationToScreen(Bt->BallLocation() + FVector(0.f, 0.f, 175.f), Sp, true))
			{
				const float Bob = FMath::Sin(Tm * 4.f) * 6.f * U, K = 1.f + 0.06f * FMath::Sin(Tm * 7.f);
				const FVector2D Cr = Sp + FVector2D(0.f, Bob);
				const float Cw = 46.f * U * K, Chh = 30.f * U * K;
				const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
				auto Tri = [&](const FVector2D& A, const FVector2D& B2, const FVector2D& Cc, const FLinearColor& Col) { FCanvasTriangleItem T(A, B2, Cc, WhiteT); T.SetColor(Col); C->DrawItem(T); };
				for (int32 Pass = 0; Pass < 2; ++Pass)
				{
					const float O = Pass == 0 ? 5.f * U : 0.f; const FLinearColor Col = Pass == 0 ? FLinearColor(0.f, 0.f, 0.f, 0.85f) : Gold;
					const FVector2D L(Cr.X - Cw - O, Cr.Y + Chh + O), Rr(Cr.X + Cw + O, Cr.Y + Chh + O), Lm(Cr.X - Cw - O, Cr.Y), Rm(Cr.X + Cw + O, Cr.Y);
					Tri(L, Rr, Rm, Col); Tri(L, Rm, Lm, Col); // band
					Tri(FVector2D(Cr.X - Cw - O, Cr.Y + 2.f), FVector2D(Cr.X - Cw * 0.35f, Cr.Y + 2.f), FVector2D(Cr.X - Cw - O * 0.5f, Cr.Y - Chh - O), Col);
					Tri(FVector2D(Cr.X - Cw * 0.45f, Cr.Y + 2.f), FVector2D(Cr.X + Cw * 0.45f, Cr.Y + 2.f), FVector2D(Cr.X, Cr.Y - Chh * 1.35f - O), Col);
					Tri(FVector2D(Cr.X + Cw * 0.35f, Cr.Y + 2.f), FVector2D(Cr.X + Cw + O, Cr.Y + 2.f), FVector2D(Cr.X + Cw + O * 0.5f, Cr.Y - Chh - O), Col);
				}
				const FString Bl = FString::Printf(TEXT("BOUNTY +%d"), BountyPts);
				const float Px = 24.f * U * (bPort ? 1.2f : 1.f);
				const FVector2D Ts = ZoogiFunCanvasMeasure(Bl, Px, 0);
				const FVector2D Tc(Cr.X, Cr.Y - Chh * 1.35f - Px * 1.1f);
				ZoogiFunCanvasQuad(C, Tc + FVector2D(5.f, 5.f) * U, Ts.X + 30.f * U, Px * 1.5f, -4.f, 6.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
				ZoogiFunCanvasQuad(C, Tc, Ts.X + 30.f * U, Px * 1.5f, -4.f, 6.f * U, Ink);
				ZoogiFunCanvasText(C, Bl, Tc.X, Tc.Y, Px, Gold, 1, 0);
			}
		}
		// the turn challenge card (top, under the turn banner) - slides in, shows DONE when it's done
		static float DoneShownAt = -1.f;
		if (S.bChDone && DoneShownAt < 0.f) DoneShownAt = S.CardAge;
		if (!S.bChDone) DoneShownAt = -1.f;
		const bool bShowCard = (G->Phase == EZoogiPhase::Aiming && !G->bAimActive) || (S.bChDone && S.CardAge - DoneShownAt < 2.5f); // b179g (Evan): hides once you touch to aim; DONE! flashes for 2.5 s
		if (S.Ch != ECh::None && bShowCard)
		{
			const FString Tx = ChText(S.Ch);
			float Px = 28.f * U * (bPort ? 1.15f : 1.f);
			FVector2D Ts = ZoogiFunCanvasMeasure(Tx, Px, 1);
			const float MaxW = Wd * 0.9f;
			if (Ts.X + 120.f * U > MaxW) { Px *= (MaxW - 120.f * U) / FMath::Max(1.f, Ts.X); Ts = ZoogiFunCanvasMeasure(Tx, Px, 1); }
			const float In = FMath::Clamp(S.CardAge / 0.25f, 0.f, 1.f);
			const float Bw = Ts.X + 120.f * U, Bh = Px * 2.9f;
			const FVector2D Cc(Wd * 0.5f + (1.f - In) * -Wd * 0.6f, Ht * (bPort ? 0.27f : 0.33f)); // b179: under the raid health bar / turn header
			ZoogiFunCanvasQuad(C, Cc + FVector2D(8.f, 8.f) * U, Bw, Bh, -3.f, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.65f));
			ZoogiFunCanvasQuad(C, Cc, Bw, Bh, -3.f, 12.f * U, S.bChDone ? FLinearColor(0.08f, 0.35f, 0.15f, 0.95f) : Ink);
			const float TagPx = 22.f * U * (bPort ? 1.15f : 1.f);
			const FString Tag = S.bChDone ? FString(TEXT("DONE!")) : FString(TEXT("TURN CHALLENGE"));
			const FVector2D TagC(Cc.X - Bw * 0.5f + 30.f * U + ZoogiFunCanvasMeasure(Tag, TagPx, 0).X * 0.5f + 20.f * U, Cc.Y - Bh * 0.5f);
			ZoogiFunCanvasQuad(C, TagC, ZoogiFunCanvasMeasure(Tag, TagPx, 0).X + 36.f * U, TagPx * 1.5f, -4.f, 6.f * U, S.bChDone ? FLinearColor(0.35f, 0.95f, 0.55f) : Yel);
			ZoogiFunCanvasText(C, Tag, TagC.X, TagC.Y, TagPx, FLinearColor(0.02f, 0.02f, 0.05f), 1, 0);
			ZoogiFunCanvasText(C, Tx, Cc.X, Cc.Y - Px * 0.15f, Px, FLinearColor::White, 1, 1);
			{
				const TCHAR* Rw = S.Ch == ECh::RaidDouble ? TEXT("+6 BONUS DAMAGE  +2 SP") : S.Ch == ECh::RaidStar ? TEXT("+1 CRYSTAL") : S.Ch == ECh::RaidBank ? TEXT("CRIT +8 DAMAGE  +2 SP") :
					S.Ch == ECh::RaidInterrupt ? TEXT("CANCELS THE ATTACK  +3 SP") : S.Ch == ECh::RaidArmor ? TEXT("TEAM HITS x2 NEXT TURN") : S.Ch == ECh::RaidSafe ? TEXT("+4 BONUS DAMAGE  +2 SP") : nullptr; // b201
				ZoogiFunCanvasText(C, Rw ? FString::Printf(TEXT("%s  +%d%% AWAKEN  -  optional"), Rw, FMath::RoundToInt(ChallengeMeter)) : FString::Printf(TEXT("+%d  +%d%% AWAKEN  -  optional"), ChallengePts, FMath::RoundToInt(ChallengeMeter)), Cc.X, Cc.Y + Px * 0.9f, Px * 0.6f, Yel, 1, 1);
			}
		}
	}
}
