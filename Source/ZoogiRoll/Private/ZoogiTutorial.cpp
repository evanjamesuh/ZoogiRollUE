// b211: the play-to-learn first match - see ZoogiTutorial.h
#include "ZoogiTutorial.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiFront.h"
#include "ZoogiGuide.h"
#include "ZoogiSkills.h"
#include "ZoogiActionFun.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace ZoogiTutorial
{
	namespace
	{
		struct FTut
		{
			bool bOn = false, bHooked = false, bTapPrev = true, bAutoChecked = false, bEnding = false;
			TWeakObjectPtr<AZoogiGameMode> G;
			double T0 = 0.0, EndAt = -1.0;
			int32 Orbs0 = -1, Score0 = -1, HumanTurns = 0, LastTurnSeen = -1;
			bool bStep[3] = { false, false, false };
			int32 ShotN = 0;
		};
		FTut S;
		UTexture2D* GHead = nullptr; bool bHeadTried = false;
		bool Flag(const TCHAR* F) { return FParse::Param(FCommandLine::Get(), F); }
		double Now() { return FPlatformTime::Seconds(); }
		UTexture2D* Head()
		{
			if (!bHeadTried) { bHeadTried = true; const FString P = FPaths::ProjectContentDir() / TEXT("Data/MenuArt/Front/zoogi_blue.png"); if (FPaths::FileExists(P)) GHead = FImageUtils::ImportFileAsTexture2D(P); if (GHead) GHead->AddToRoot(); }
			return GHead;
		}
		void MarkDone() { ZoogiControls::Set(TEXT("TutorialDone"), 1.f); }
		void Finish(AZoogiGameMode* G, bool bSkipped)
		{
			MarkDone();
			S.bOn = false; S.bEnding = false; S.EndAt = -1.0;
			UE_LOG(LogZoogi, Log, TEXT("TUTORIAL %s - back to the home screen"), bSkipped ? TEXT("SKIPPED") : TEXT("COMPLETE"));
			if (G)
			{
				if (!G->ShotDir.IsEmpty()) G->TakeShotDelayed(TEXT("tut_9_home_buddy"), 4.0f);
				G->ReturnToMenu();
			}
		}
		const TCHAR* Hint()
		{
			if (!S.bStep[0]) return TEXT("Drag back, let go to ROLL!");
			if (!S.bStep[1]) return TEXT("Knock an orb off the edge!");
			if (!S.bStep[2]) return TEXT("Tap SKILL to use a power!");
			return TEXT("YOU'RE READY!");
		}
		FBox2D SkipBox(float Wd, float Ht, float U) { const FVector2D TL(Wd - 280.f * U, Ht * 0.25f + 290.f * U); return FBox2D(TL, TL + FVector2D(240.f * U, 62.f * U)); } // under the FIRST MATCH panel

		void Logic(AZoogiGameMode* G, float Wd, float Ht, float U)
		{
			if (G->Phase == EZoogiPhase::Menu) { if (Now() - S.T0 > 5.0) S.bOn = false; return; } // the match never started / was left
			if (G->Phase == EZoogiPhase::GameOver) { MarkDone(); S.bOn = false; UE_LOG(LogZoogi, Log, TEXT("TUTORIAL: the match ended - done")); return; }
			AZoogiBall* Hu = nullptr; for (AZoogiBall* B : G->Balls) if (B && !B->bCPU) { Hu = B; break; }
			if (S.Orbs0 < 0 && G->Phase == EZoogiPhase::Aiming) S.Orbs0 = G->OrbsLeft();
			// SKIP: a press inside the button
			{
				APlayerController* Pc = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr;
				bool bDown = false; FVector2D P(-1.f, -1.f);
				if (Pc)
				{
					float X = 0.f, Y = 0.f; bool bT = false; Pc->GetInputTouchState(ETouchIndex::Touch1, X, Y, bT);
					if (bT) { bDown = true; P = FVector2D(X, Y); }
					else if (Pc->IsInputKeyDown(EKeys::LeftMouseButton)) { bDown = true; Pc->GetMousePosition(P.X, P.Y); }
				}
				if (bDown && !S.bTapPrev && SkipBox(Wd, Ht, U).IsInside(P)) { S.bTapPrev = bDown; Finish(G, true); return; }
				S.bTapPrev = bDown;
			}
			if (!Hu || S.bEnding) { if (S.bEnding && Now() >= S.EndAt) Finish(G, false); return; }
			// 1 aim + roll: the human's Zoogi rolls
			if (!S.bStep[0] && G->Phase == EZoogiPhase::Rolling && G->GetActiveBall() == Hu) { S.bStep[0] = true; UE_LOG(LogZoogi, Log, TEXT("TUTORIAL step 1 done (roll)")); }
			// 2 knock out 2 orbs
			if (S.Score0 < 0 && G->Phase == EZoogiPhase::Aiming) S.Score0 = Hu->Score;
			if (!S.bStep[1] && ((S.Orbs0 >= 0 && S.Orbs0 - G->OrbsLeft() >= 1) || (S.Score0 >= 0 && Hu->Score > S.Score0) || S.HumanTurns >= 4)) { S.bStep[1] = true; UE_LOG(LogZoogi, Log, TEXT("TUTORIAL step 2 done (first points)")); }
			// 3 a skill / power of the human's (or, if there is never SP for one, after its 3rd turn)
			if (G->GetActiveBall() == Hu && G->TurnCounter != S.LastTurnSeen && G->Phase == EZoogiPhase::Aiming) { S.LastTurnSeen = G->TurnCounter; ++S.HumanTurns; }
			if (!S.bStep[2])
			{
				FName Id; int32 Pl = -1; double At = 0.0;
				if ((ZoogiSkills::LastCast(Id, Pl, At) && Pl == Hu->PlayerIndex && At > S.T0) || S.HumanTurns >= 4) { S.bStep[2] = true; UE_LOG(LogZoogi, Log, TEXT("TUTORIAL step 3 done (power)")); }
			}
			if (S.bStep[0] && S.bStep[1] && S.bStep[2] && !S.bEnding)
			{
				S.bEnding = true; S.EndAt = Now() + 3.5;
				if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("YOU'RE READY!"), TEXT("TUTORIAL COMPLETE - LET'S ROLL"), FLinearColor(1.f, 0.82f, 0.25f), 3.f, 1.4f);
				ZoogiUtil::PlaySfx(G, TEXT("Win"), 0.8f, 1.1f);
			}
		}
		void DrawCb(AHUD* H, UCanvas* C)
		{
			if (!S.bOn || !H || !C || !H->GetWorld()) return;
			AZoogiGameMode* G = H->GetWorld()->GetAuthGameMode<AZoogiGameMode>();
			if (!G || G != S.G.Get()) return;
			const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
			Logic(G, Wd, Ht, U);
			if (!S.bOn || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming || G->Phase == EZoogiPhase::GameOver) return;
			if (!G->ShotDir.IsEmpty())
			{	// capture runs: one frame per step
				const int32 K = (S.bStep[0] ? 1 : 0) + (S.bStep[1] ? 1 : 0) + (S.bStep[2] ? 1 : 0);
				if (K + 1 > S.ShotN) { S.ShotN = K + 1; G->TakeShotDelayed(FString::Printf(TEXT("tut_%d_step"), K + 1), 0.8f); }
			}
			// the FIRST MATCH checklist (right side, under the player cards) with Blue's head + the current hint
			const float PW = 460.f * U, PH = 270.f * U, X0 = Wd - PW - 40.f * U, Y0 = Ht * 0.25f;
			ZoogiFunCanvasRect(C, X0 + 8.f * U, Y0 + 8.f * U, PW, PH, FLinearColor(0.85f, 0.05f, 0.4f, 0.95f));
			ZoogiFunCanvasRect(C, X0 - 4.f * U, Y0 - 4.f * U, PW + 8.f * U, PH + 8.f * U, FLinearColor::White);
			ZoogiFunCanvasRect(C, X0, Y0, PW, PH, FLinearColor(0.03f, 0.025f, 0.08f, 0.95f));
			ZoogiFunCanvasRect(C, X0, Y0, PW, 44.f * U, FLinearColor(0.16f, 0.1f, 0.4f, 1.f));
			ZoogiFunCanvasText(C, TEXT("FIRST MATCH"), X0 + 16.f * U, Y0 + 22.f * U, 28.f * U, FLinearColor(1.f, 0.82f, 0.25f), 0, 0);
			const int32 Nd = (S.bStep[0] ? 1 : 0) + (S.bStep[1] ? 1 : 0) + (S.bStep[2] ? 1 : 0);
			ZoogiFunCanvasText(C, FString::Printf(TEXT("%d / 3"), Nd), X0 + PW - 16.f * U, Y0 + 22.f * U, 26.f * U, FLinearColor::White, 2, 0);
			static const TCHAR* Names[3] = { TEXT("Aim + roll"), TEXT("Knock out an orb"), TEXT("Use a power") };
			for (int32 i = 0; i < 3; ++i)
			{
				const float Y = Y0 + 58.f * U + i * 44.f * U;
				ZoogiFunCanvasRect(C, X0 + 16.f * U, Y, 28.f * U, 28.f * U, S.bStep[i] ? FLinearColor(0.24f, 0.8f, 0.35f, 1.f) : FLinearColor(0.25f, 0.27f, 0.42f, 1.f));
				if (S.bStep[i]) ZoogiFunCanvasText(C, TEXT("V"), X0 + 30.f * U, Y + 14.f * U, 22.f * U, FLinearColor::White, 1, 0);
				ZoogiFunCanvasText(C, Names[i], X0 + 58.f * U, Y + 14.f * U, 26.f * U, S.bStep[i] ? FLinearColor(0.55f, 0.95f, 0.6f, 1.f) : FLinearColor::White, 0, 0);
			}
			// Blue says the next thing to do
			const float By = Y0 + PH - 58.f * U;
			ZoogiFunCanvasRect(C, X0 + 10.f * U, By - 4.f * U, PW - 20.f * U, 48.f * U, FLinearColor::White);
			if (UTexture2D* Hd = Head(); Hd && Hd->GetResource()) { FCanvasTileItem It(FVector2D(X0 + 12.f * U, By - 6.f * U), Hd->GetResource(), FVector2D(54.f * U, 54.f * U), FLinearColor::White); It.BlendMode = SE_BLEND_Translucent; C->DrawItem(It); }
			ZoogiFunCanvasText(C, Hint(), X0 + 74.f * U, By + 20.f * U, 22.f * U, FLinearColor(0.05f, 0.04f, 0.1f, 1.f), 0, 0);
			// SKIP
			const FBox2D Sb = SkipBox(Wd, Ht, U);
			ZoogiFunCanvasRect(C, Sb.Min.X + 6.f * U, Sb.Min.Y + 6.f * U, Sb.GetSize().X, Sb.GetSize().Y, FLinearColor(0.f, 0.f, 0.f, 0.6f));
			ZoogiFunCanvasRect(C, Sb.Min.X, Sb.Min.Y, Sb.GetSize().X, Sb.GetSize().Y, FLinearColor(0.02f, 0.02f, 0.05f, 0.85f));
			ZoogiFunCanvasRect(C, Sb.Min.X, Sb.Min.Y, Sb.GetSize().X, 4.f * U, FLinearColor::White);
			ZoogiFunCanvasText(C, TEXT("SKIP TUTORIAL  >"), Sb.GetCenter().X, Sb.GetCenter().Y, 26.f * U, FLinearColor::White, 1, 0);
		}
	}

	bool Done()
	{
		float V = 0.f; if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.Controls"), TEXT("TutorialDone"), V, GGameUserSettingsIni);
		return V > 0.5f;
	}
	bool Active() { return S.bOn; }
	bool ShouldAutoRun()
	{
		if (S.bAutoChecked) return false;
		S.bAutoChecked = true;
		if (Flag(TEXT("ZoogiNoTutorial"))) return false;
		if (Flag(TEXT("ZoogiTutorialTest"))) return true;
		if (Flag(TEXT("ZoogiAuto")) || Flag(TEXT("ZoogiFrontTest")) || Flag(TEXT("ZoogiHumanTest"))) return false; // capture runs
		return !Done();
	}
	void Begin(AZoogiGameMode* G)
	{
		if (!G) return;
		S = FTut(); S.bAutoChecked = true; // never auto-run again this launch (the menu comes back after it)
		static bool bHook = false;
		if (!bHook) { bHook = true; AHUD::OnHUDPostRender.AddStatic(&DrawCb); }
		S.bOn = true; S.G = G; S.T0 = Now();
		// a short classic match: you + 1 CPU, 2 rounds, the guided first turn armed
		ZoogiGuide::Arm(true);
		G->MenuAction(FName(TEXT("Mode")), 0);
		G->MenuAction(FName(TEXT("Players")), 2);
		for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0;
		G->MenuRounds = 2;
		UE_LOG(LogZoogi, Log, TEXT("TUTORIAL begins: guided first match (you + 1 CPU, 2 rounds)"));
		G->MenuAction(FName(TEXT("Start")), 0);
	}
}
