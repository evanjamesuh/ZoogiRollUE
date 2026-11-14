// b149: BATTLE ROYALE mix (classic / action rounds, rotating arenas, points per round) - see ZoogiRoyaleMix.h.
#include "ZoogiRoyaleMix.h"
#include "ZoogiShooter.h" // b234
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoyale.h"
#include "ZoogiTypes.h"
#include "ZoogiActionFun.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiRoyaleMix
{
	namespace
	{
		struct FMix
		{
			bool bOn = false;
			int32 Seg = 0, N = 0, SavedRounds = 8;
			EZoogiArena SavedArena = EZoogiArena::Meadow;
			bool bAction = false, bEnding = false, bPlayed = false, bSwitching = false; // bSwitching: the next round starts next frame
			TArray<int32> Totals;
			TWeakObjectPtr<AZoogiGameMode> G;
		};
		FMix M;
		// one lap of every arena; Ancient (royale only) always lands on an action round
		const EZoogiArena Order[8] = { EZoogiArena::Ancient, EZoogiArena::Meadow, EZoogiArena::Frozen, EZoogiArena::Volcanic, EZoogiArena::Cosmic, EZoogiArena::Arabian, EZoogiArena::Tomb, EZoogiArena::Neon };
		float ActionSecondsI() { static float S = -1.f; if (S < 0.f) { S = 45.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiMixActionSec="), S); S = FMath::Clamp(S, 10.f, 300.f); } return S; }
		// the last round is always classic: round s is classic when (N-1-s) is even
		bool IsActionSeg(int32 S)
		{	// b153 (Evan: "Classic-only"): every round is a classic round; -ZoogiRoyaleActionRounds = the b149 alternating mix
			static const bool bAct = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleActionRounds"));
			return bAct && ((M.N - 1 - S) % 2) != 0;
		}
		EZoogiArena ArenaFor(int32 S)
		{
			static const bool bAct = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleActionRounds"));
			if (!bAct) { static const EZoogiArena Classic[7] = { EZoogiArena::Frozen, EZoogiArena::Volcanic, EZoogiArena::Cosmic, EZoogiArena::Arabian, EZoogiArena::Tomb, EZoogiArena::Neon, EZoogiArena::Meadow }; return Classic[S % 7]; } // b265 (Evan: "Remove first map that appears in battle royale"): Ancient is out of the rotation // b202: Teaser deleted -> Meadow // b153: classic-only lap; b156 (Evan 08:02 "Replace meadows with ancient meadows"): ANCIENT MEADOWS opens the lap in place of the Meadow (the Zoogi King's throne)
			const int32 Off = (M.N % 2 == 1) ? 1 : 0; // keeps Ancient on an action round
			EZoogiArena A = Order[(S + Off) % 8];
			if (!IsActionSeg(S) && A == EZoogiArena::Ancient) A = EZoogiArena::Meadow; // never a classic round on Ancient (b202: Teaser deleted)
			return A;
		}
		FString TotalsText(AZoogiGameMode* G)
		{
			FString T;
			for (int32 i = 0; i < M.Totals.Num(); ++i)
				if (G->Balls.IsValidIndex(i) && G->Balls[i]) T += FString::Printf(TEXT("%sP%d %s %d"), T.IsEmpty() ? TEXT("") : TEXT("   "), i + 1, *G->Balls[i]->Def.Name.ToUpper(), M.Totals[i]);
			return T;
		}
		void StartSeg(AZoogiGameMode* G)
		{
			M.bAction = IsActionSeg(M.Seg);
			M.bEnding = false; M.bSwitching = false;
			G->MenuArena = ArenaFor(M.Seg);
			const FString ArenaNm = ZoogiUtil::ArenaName(G->MenuArena).ToUpper();
			if (M.bAction)
			{
				G->RoyaleSetMode(true);
				G->RoyaleStartMatch();
			}
			else
			{
				G->RoyaleSetMode(false);
				G->MenuRounds = 1; // one classic round: every Zoogi takes one turn
				G->StartMatch();
				for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i] && M.Totals.IsValidIndex(i)) G->Balls[i]->Score = M.Totals[i]; // the running totals
			}
			if (M.Totals.Num() < G->Balls.Num()) M.Totals.SetNumZeroed(G->Balls.Num());
			const bool bLast = M.Seg == M.N - 1;
			G->BigText = FString::Printf(TEXT("ROUND %d / %d%s\n%s ROUND - %s"), M.Seg + 1, M.N, bLast ? TEXT("  FINAL") : TEXT(""), M.bAction ? TEXT("ACTION") : TEXT("CLASSIC"), *ArenaNm);
			G->BigColor = M.bAction ? FLinearColor(1.f, 0.45f, 0.2f) : FLinearColor(0.45f, 0.85f, 1.f);
			G->BigAge = 0.f;
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G))
				F->PublicCallout(M.bAction ? TEXT("ACTION ROUND!") : TEXT("CLASSIC ROUND!"),
					M.bAction ? FString::Printf(TEXT("%s - DRIVE, JUMP, TRANSFORM & SHOOT - %.0f SEC"), *ArenaNm, ActionSecondsI()) : FString::Printf(TEXT("%s - ONE SHOT EACH, TAKE TURNS"), *ArenaNm),
					G->BigColor, 2.4f, 1.25f);
			G->TakeShotDelayed(FString::Printf(TEXT("mix_round_%d_%s"), M.Seg + 1, M.bAction ? TEXT("action") : TEXT("classic")), 3.f);
			UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX round %d/%d: %s on %s | totals: %s"), M.Seg + 1, M.N, M.bAction ? TEXT("ACTION") : TEXT("CLASSIC"), *ArenaNm, *TotalsText(G));
		}
		void Next(AZoogiGameMode* G)
		{
			++M.Seg; M.bSwitching = true; // the finishing round may call FinishGame / RoyaleFinish again before then
			TWeakObjectPtr<AZoogiGameMode> Wg = G;
			// next frame (never from inside the finishing round's own call stack)
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Wg](float) -> bool
			{
				if (AZoogiGameMode* Gg = Wg.Get()) if (M.bOn) StartSeg(Gg);
				return false;
			}), 0.05f);
		}
	}

	bool Enabled() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleOld")); return !bOld && ZoogiShooter::Kind() == 0; } // b234: no classic rounds in a SHOOTER match
	bool Active() { return M.bOn; }
	bool ActionRound() { return M.bOn && M.bAction; }
	float ActionSeconds() { return ActionSecondsI(); }

	void Begin(AZoogiGameMode* G)
	{
		if (!G) return;
		M = FMix();
		M.bOn = true; M.bPlayed = true; M.G = G;
		M.SavedRounds = G->MenuRounds; M.SavedArena = G->MenuArena;
		M.N = FMath::Clamp(G->MenuRounds, 2, 16);
		M.Totals.SetNumZeroed(FMath::Max(4, G->MenuNumPlayers));
		UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX begins: %d rounds (classic / action alternate, last round classic), action rounds %.0f s"), M.N, ActionSecondsI());
		StartSeg(G);
	}

	bool OnClassicFinished(AZoogiGameMode* G)
	{
		if (!M.bOn || !G) return false;
		if (M.bSwitching) return true;
		if (M.bAction) return false;
		for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i] && M.Totals.IsValidIndex(i)) M.Totals[i] = G->Balls[i]->Score;
		if (M.Seg >= M.N - 1)
		{	// the final round: its results screen is the match result (the scores are the totals)
			UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX over: %s"), *TotalsText(G));
			G->MenuRounds = M.SavedRounds;
			M.bOn = false;
			G->RoyaleSetMode(false); // the PLAY screen keeps the royale card selected via its own state; the results are classic
			return false;
		}
		UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX classic round %d done: %s"), M.Seg + 1, *TotalsText(G));
		Next(G);
		return true;
	}

	static void EndAction(AZoogiGameMode* G, const TCHAR* Why)
	{
		if (M.bEnding) return;
		M.bEnding = true;
		const int32 Ko = ZoogiConst::ScorePlayer, Fl = ZoogiConst::PenaltyFall;
		FString Line;
		for (int32 i = 0; i < G->Royale.P.Num() && i < G->Balls.Num(); ++i)
		{
			const FZoogiRoyalePlayer& R = G->Royale.P[i];
			const int32 Pts = R.KOs * Ko - R.Falls * Fl;
			if (M.Totals.IsValidIndex(i)) M.Totals[i] += Pts;
			if (G->Balls[i]) Line += FString::Printf(TEXT("P%d %+d (%d KO, %d falls)  "), i + 1, Pts, R.KOs, R.Falls);
		}
		G->PublicFeed(FString::Printf(TEXT("ACTION ROUND OVER  -  %s"), *TotalsText(G)), FLinearColor(1.f, 0.8f, 0.3f));
		UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX action round %d over (%s): %s| totals: %s"), M.Seg + 1, Why, *Line, *TotalsText(G));
		Next(G);
	}

	bool OnRoyaleFinished(AZoogiGameMode* G)
	{
		if (!M.bOn || !G) return false;
		if (M.bSwitching) return true;
		if (!M.bAction) return false;
		EndAction(G, TEXT("one left"));
		return true;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!M.bOn || !G || M.G.Get() != G || !M.bAction || M.bEnding || M.bSwitching || !G->bRoyaleMode) return;
		if (G->Royale.Step == EZoogiRoyaleStep::Fight && G->Royale.FightTime >= ActionSecondsI()) EndAction(G, TEXT("time"));
	}

	void OnMenu(AZoogiGameMode* G)
	{
		if (!M.bOn && !M.bPlayed) return;
		if (G) { G->MenuRounds = M.SavedRounds; G->MenuArena = M.SavedArena; if (!G->bRoyaleMode) G->RoyaleSetMode(true); }
		UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX: back to the menu (BATTLE ROYALE stays selected)"));
		M = FMix();
	}

	void Stop(AZoogiGameMode* G)
	{
		if (!M.bOn) return;
		if (G) G->MenuRounds = M.SavedRounds;
		M.bOn = false;
		UE_LOG(LogZoogi, Log, TEXT("ROYALE MIX stopped"));
	}
}
