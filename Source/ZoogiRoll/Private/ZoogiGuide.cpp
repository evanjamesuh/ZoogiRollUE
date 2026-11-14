// Nov 2 b66: see ZoogiGuide.h
#include "ZoogiGuide.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiFront.h"
#include "ZoogiRedirect.h"
#include "ZoogiActionFun.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "ZoogiSkills.h"
#include "ZoogiOrb.h"
#include "ZoogiP5Style.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);

namespace ZoogiGuide
{
	namespace
	{
		enum EStep : int32 { Off = 0, Aim, Rolling, Picked, Duel, Done };
		int32 Step = Off;
		int32 GuideTurn = -1, HumanIdx = -1;
		double StepAt = 0.0, SlowUntil = 0.0;
		bool bChecked = false, bWanted = false;
		double Now() { return FPlatformTime::Seconds(); }
		bool GuideAlreadyDone()
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoGuide"))) return true;
			float V = 0.f; if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.Controls"), TEXT("GuideDone"), V, GGameUserSettingsIni);
			return V > 0.5f;
		}
		void SetStep(int32 S) { if (S != Step) { Step = S; StepAt = Now(); UE_LOG(LogZoogi, Log, TEXT("GUIDE step %d"), S); } }
	}

	namespace
	{	// ---- b172e trainer guide (v2)
		enum EG2 : int32 { G2Off = 0, G2Intro, G2BoltTalk, G2BoltDo, G2BoltSee, G2WallTalk, G2WallDo, G2WallSee, G2BlockTalk, G2ShotTalk, G2ShotDo, G2Done };
		int32 S2 = G2Off; double S2At = 0.0, S2NextFire = 0.0; int32 S2Fired = 0;
		bool OldGuide() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGuide")); return b; }
		void Set2(int32 V) { if (V != S2) { S2 = V; S2At = Now(); UE_LOG(LogZoogi, Log, TEXT("GUIDE2 step %d"), V); } }
		float Hold2(int32 V) { switch (V) { case G2Intro: return 4.5f; case G2BoltTalk: return 5.f; case G2BoltSee: return 3.5f; case G2WallTalk: return 5.f; case G2WallSee: return 3.f; case G2BlockTalk: return 5.5f; case G2ShotTalk: return 3.5f; default: return 0.f; } }
	}

	bool DoneBefore() { return GuideAlreadyDone(); }
	void Arm(bool bOn) { bChecked = true; bWanted = bOn; Step = Off; GuideTurn = -1; UE_LOG(LogZoogi, Log, TEXT("GUIDE %s for the next match"), bOn ? TEXT("armed") : TEXT("off")); }
	bool Active() { return OldGuide() ? (Step > Off && Step < Done) : (S2 > G2Off && S2 < G2Done); }
	bool HideMenus() { return OldGuide() ? Step == Aim : (S2 > G2Off && S2 < G2ShotDo); }
	bool WantSlow() { return OldGuide() && Step == Rolling && Now() < SlowUntil; }

	static void Tick2(AZoogiGameMode* G)
	{
		if (G->Phase == EZoogiPhase::Menu || G->bRoyaleMode || G->bComboMode || G->bSoloTraining) { if (S2 != G2Done) S2 = G2Off; GuideTurn = -1; return; }
		AZoogiBall* A = G->GetActiveBall();
		if (S2 == G2Off)
		{
			if (!bChecked) { bChecked = true; bWanted = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideTest")); }
			if (!bWanted || !A || A->bCPU || G->Phase != EZoogiPhase::Aiming || G->Stage != EZoogiStage::Battle) return;
			GuideTurn = G->TurnCounter; HumanIdx = A->PlayerIndex;
			Set2(G2Intro);
			return;
		}
		{ static int32 GShot2 = -1; if (S2 != GShot2 && G->ShotDir.Len()) { GShot2 = S2; G->TakeShotDelayed(FString::Printf(TEXT("guide2_%02d"), S2), S2 == G2BoltDo || S2 == G2WallDo ? 1.4f : 0.8f); } }
		if (S2 == G2Done) return;
		if (G->TurnCounter != GuideTurn)
		{
			Set2(G2Done); bWanted = false;
			ZoogiControls::Set(TEXT("GuideDone"), 1.f);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("YOUR TURN!"), TEXT("BOLT BALLS FOR ORBS - ICE WALL TO STAY SAFE"), FLinearColor(0.55f, 0.85f, 1.f), 2.6f, 1.f);
			return;
		}
		if (S2 < G2ShotDo && G->Phase == EZoogiPhase::Rolling) { Set2(G2ShotDo); return; } // the player launched on their own: fine
		const float T = (float)(Now() - S2At);
		// b176d (Evan: "should not auto switch to the next ... game waits until screen is tapped"): a story step stays up until a tap;
		// capture runs (-ZoogiAuto / -ZoogiGuideAuto) keep the timed steps
		static const bool bAutoAdv = (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAuto")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideAuto"))) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideTap"));
		bool bTap = false;
		{
			static bool bWasDown = false;
			bool bDown = false;
			if (APlayerController* PC = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr)
			{
				float Tx = 0.f, Ty = 0.f; bool bT = false;
				PC->GetInputTouchState(ETouchIndex::Touch1, Tx, Ty, bT);
				bDown = bT || PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Enter);
			}
			bTap = bDown && !bWasDown; bWasDown = bDown;
		}
		auto Ready = [&]() { return bAutoAdv ? T > Hold2(S2) : (T > 0.5f && bTap); };
		switch (S2)
		{
		case G2Intro: if (Ready()) Set2(G2BoltTalk); break;
		case G2BoltTalk: if (Ready()) { ZoogiSkills::ArmBolts(G, HumanIdx, 3); S2Fired = 0; S2NextFire = Now() + 0.3; Set2(G2BoltDo); } break;
		case G2BoltDo:
			if (Now() >= S2NextFire && S2Fired < 3) { if (ZoogiSkills::BoltGo(G)) ++S2Fired; S2NextFire = Now() + 0.8; }
			if (S2Fired >= 3 || T > 6.f) Set2(G2BoltSee);
			break;
		case G2BoltSee: if (Ready()) Set2(G2WallTalk); break;
		case G2WallTalk: if (Ready()) { ZoogiSkills::CpuCast(G, FName(TEXT("ice_wall"))); Set2(G2WallDo); } break;
		case G2WallDo: if ((T > 1.2f && !ZoogiSkills::CpuPicking() && !ZoogiSkills::Picking() && !ZoogiSkills::EffectBusy()) || T > 7.f) Set2(G2WallSee); break;
		case G2WallSee: if (Ready()) Set2(G2BlockTalk); break;
		case G2BlockTalk: if (Ready()) Set2(G2ShotTalk); break;
		case G2ShotTalk:
			if (Ready() && A && !A->bCPU)
			{	// the trainer launches at the middle of the orbs
				FVector Sum = FVector::ZeroVector; int32 N = 0;
				for (AZoogiOrb* O : G->Orbs) if (O && !O->IsHidden()) { Sum += O->GetActorLocation(); ++N; }
				const FVector Tg = N ? Sum / N : FVector::ZeroVector;
				const FVector D = Tg - A->BallLocation();
				if (D.Size2D() > 50.f) A->AimYaw = D.Rotation().Yaw;
				G->HumanDrag(0.78f, false);
				const bool bShot = G->HumanDrag(0.78f, true);
				UE_LOG(LogZoogi, Log, TEXT("GUIDE2 trainer launch toward (%.0f, %.0f): %s"), Tg.X, Tg.Y, bShot ? TEXT("fired") : TEXT("not fired"));
				Set2(G2ShotDo);
			}
			break;
		default: break;
		}
	}

	static void Draw2(AZoogiGameMode* G, UCanvas* C)
	{
		if (S2 <= G2Off || S2 >= G2Done) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const bool bPort = Ht > Wd;
		const float T = (float)(Now() - S2At);
		const TCHAR* Head = TEXT(""); FString Body;
		switch (S2)
		{
		case G2Intro: Head = TEXT("PLAY GUIDE"); Body = TEXT("Hi! I'm Frosti, your trainer. Sit back - I'll play this turn and show you two big moves."); break;
		case G2BoltTalk: Head = TEXT("STEP 1 / 3  -  BOLT BALLS"); Body = TEXT("I tap SUPERPOWER to fire Bolt Balls. Each bolt smashes into the nearest target - knock orbs off the edge for points!"); break;
		case G2BoltDo: Head = TEXT("STEP 1 / 3  -  BOLT BALLS"); Body = TEXT("Zap! Zap! Zap!"); break;
		case G2BoltSee: Head = TEXT("STEP 1 / 3  -  BOLT BALLS"); Body = TEXT("See them fly? Every orb that goes off the edge scores."); break;
		case G2WallTalk: Head = TEXT("STEP 2 / 3  -  ICE WALL"); Body = TEXT("My marble is near the edge. An Ice Wall behind me stops a big hit from knocking me out."); break;
		case G2WallDo: case G2WallSee: Head = TEXT("STEP 2 / 3  -  ICE WALL"); Body = TEXT("Safe! Nothing can push me off that side now."); break;
		case G2BlockTalk: Head = TEXT("STEP 3 / 3  -  BLOCK A SHOT"); Body = TEXT("Ice Wall blocks shots too: drop it in front of a rival and they can't shoot that way."); break;
		case G2ShotTalk: Head = TEXT("WATCH ME LAUNCH"); Body = TEXT("Now I pull back and let go - right into the orbs!"); break;
		case G2ShotDo: Head = TEXT("NICE!"); Body = TEXT("Your turn is next - you've got this!"); break;
		default: break;
		}
		const float In = FMath::Clamp(T / 0.2f, 0.f, 1.f);
		const float M = 16.f * U, BoxH = (bPort ? 360.f : 260.f) * U, BoxY = Ht - BoxH - M - (bPort ? 30.f * U : 10.f * U);
		const float ImgH = BoxH * (bPort ? 1.55f : 1.45f), ImgW = ImgH * 1.08f;
		auto Rect = [&](float X, float Y, float W, float H, const FLinearColor& Cc) { FCanvasTileItem It(FVector2D(X, Y), FVector2D(W, H), Cc); It.BlendMode = SE_BLEND_Translucent; C->DrawItem(It); };
		Rect(M - 4.f * U, BoxY - 4.f * U, Wd - 2.f * M + 8.f * U, BoxH + 8.f * U, FLinearColor(0.47f, 0.78f, 1.f, 0.95f * In));
		Rect(M, BoxY, Wd - 2.f * M, BoxH, FLinearColor(0.012f, 0.01f, 0.035f, 0.94f * In));
		if (UTexture2D* Tr = ZoogiP5::Portrait(TEXT("trainer_frosti")))
		{
			const float Bob = 5.f * U * FMath::Sin((float)Now() * 2.2f);
			FCanvasTileItem Im(FVector2D(M - 30.f * U, BoxY + BoxH - ImgH + Bob), Tr->GetResource(), FVector2D(ImgW, ImgH), FLinearColor(1.f, 1.f, 1.f, In)); Im.BlendMode = SE_BLEND_Translucent; C->DrawItem(Im);
		}
		const float TX = M + ImgW * 0.78f, TW = Wd - M - 24.f * U - TX;
		Rect(TX - 6.f * U, BoxY - 30.f * U, 330.f * U, 48.f * U, FLinearColor(0.24f, 0.59f, 1.f, In));
		ZoogiFunCanvasText(C, TEXT("TRAINER FROSTI"), TX + 8.f * U, BoxY - 6.f * U, 34.f * U, FLinearColor::White, 0, 0);
		ZoogiFunCanvasText(C, Head, TX, BoxY + 42.f * U, (bPort ? 34.f : 36.f) * U, FLinearColor(1.f, 0.84f, 0.f, In), 0, 0);
		// word wrap to the box
		const float Px = (bPort ? 30.f : 30.f) * U;
		TArray<FString> Words; Body.ParseIntoArray(Words, TEXT(" "));
		TArray<FString> Lines; FString Cur;
		for (const FString& Wd2 : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd2 : Cur + TEXT(" ") + Wd2;
			if (ZoogiFunCanvasMeasure(Try, Px, 1).X > TW && !Cur.IsEmpty()) { Lines.Add(Cur); Cur = Wd2; } else Cur = Try;
		}
		if (!Cur.IsEmpty()) Lines.Add(Cur);
		for (int32 k = 0; k < Lines.Num() && k < 5; ++k) ZoogiFunCanvasText(C, Lines[k], TX, BoxY + (92.f + k * 40.f) * U, Px, FLinearColor(1.f, 1.f, 1.f, In), 0, 1);
		// hands free: the next step comes on its own - a thin bar shows when
		const float Hold = Hold2(S2);
		static const bool bAutoDraw = (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAuto")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideAuto"))) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideTap"));
		if (Hold > 0.f && !bAutoDraw && T > 0.5f)
		{	// b176d: waits for a tap
			const float Bl = 0.55f + 0.45f * FMath::Sin((float)Now() * 5.f);
			ZoogiFunCanvasText(C, TEXT("TAP TO CONTINUE  >"), TX + TW, BoxY + BoxH - 40.f * U, 30.f * U, FLinearColor(0.47f, 0.78f, 1.f, Bl), 2, 0);
		}
		else if (Hold > 0.f && bAutoDraw)
		{
			const float K = FMath::Clamp(T / Hold, 0.f, 1.f);
			Rect(TX, BoxY + BoxH - 26.f * U, TW, 8.f * U, FLinearColor(1.f, 1.f, 1.f, 0.15f));
			Rect(TX, BoxY + BoxH - 26.f * U, TW * K, 8.f * U, FLinearColor(0.47f, 0.78f, 1.f, 0.95f));
			ZoogiFunCanvasText(C, TEXT("NEXT STEP"), TX + TW, BoxY + BoxH - 46.f * U, 22.f * U, FLinearColor(0.47f, 0.78f, 1.f, 1.f), 2, 0);
		}
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		if (!OldGuide()) { Tick2(G); return; } // b172e
		if (G->Phase == EZoogiPhase::Menu || G->bRoyaleMode || G->bComboMode || G->bSoloTraining) { if (Step != Done) Step = Off; GuideTurn = -1; return; }
		AZoogiBall* A = G->GetActiveBall();
		if (Step == Off)
		{
			if (!bChecked) { bChecked = true; bWanted = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideTest")); } // b67: armed by the first story mission (or the test flag)
			if (!bWanted || !A || A->bCPU || G->Phase != EZoogiPhase::Aiming) return;
			GuideTurn = G->TurnCounter; HumanIdx = A->PlayerIndex;
			if (HumanIdx >= 0 && HumanIdx < 4) G->AwakenCharge[HumanIdx] = AZoogiGameMode::AwakenChargeMax; // the demo combo: 4 crystals
			SetStep(Aim);
			return;
		}
		if (Step == Done) return;
		if (G->TurnCounter != GuideTurn)
		{
			SetStep(Done); bWanted = false;
			ZoogiControls::Set(TEXT("GuideDone"), 1.f);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("YOU'VE GOT IT!"), TEXT("CRYSTALS REFILL +1 EVERY TURN - SAVE THEM FOR COMBOS"), FLinearColor(1.f, 0.82f, 0.25f), 2.4f, 1.f);
			return;
		}
		UZoogiFunSys* F = UZoogiFunSys::Get(G);
		if (F && F->DuelOn()) { SetStep(Duel); return; }
		if (Step == Aim && G->Phase == EZoogiPhase::Rolling) { SetStep(Rolling); SlowUntil = Now() + 5.0; }
		if ((Step == Rolling || Step == Picked) && ZoogiRedirect::Combo() > 0) SetStep(Picked);
		if (Step == Rolling && ZoogiRedirect::Picking()) SlowUntil = 0.0;
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!Active() || !G || !C) return;
		if (!OldGuide()) { Draw2(G, C); return; } // b172e
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const float T = (float)(Now() - StepAt);
		const bool bPort = Ht > Wd;
		auto Card = [&](const TCHAR* Num, const FString& L1, const FString& L2, float Y)
		{
			const float In = FMath::Clamp(T / 0.25f, 0.f, 1.f);
			const FVector2D Ctr(Wd * 0.5f, Y);
			ZoogiFunCanvasQuad(C, Ctr + FVector2D(10.f, 10.f) * U, FMath::Min(Wd * 0.94f, 1000.f * U) * In, 120.f * U, -3.f, 16.f * U, FLinearColor(0.85f, 0.05f, 0.12f, 0.9f));
			ZoogiFunCanvasQuad(C, Ctr, FMath::Min(Wd * 0.94f, 1000.f * U) * In, 120.f * U, -3.f, 16.f * U, FLinearColor(0.f, 0.f, 0.f, 0.92f));
			if (In < 1.f) return;
			ZoogiFunCanvasText(C, FString(Num) + TEXT("  ") + L1, Wd * 0.5f, Y - 22.f * U, (bPort ? 34.f : 42.f) * U, FLinearColor::White, 1, 0);
			ZoogiFunCanvasText(C, L2, Wd * 0.5f, Y + 26.f * U, (bPort ? 22.f : 26.f) * U, FLinearColor(1.f, 0.82f, 0.25f), 1, 0);
		};
		AZoogiBall* A = G->GetActiveBall();
		const float CardY = Ht * (bPort ? 0.2f : 0.24f);
		if (Step == Aim)
		{
			Card(TEXT("1"), TEXT("DRAG BACK, AIM, LET GO"), TEXT("Pull away from where you want to roll - further back = harder"), CardY);
			// ghost hand: a finger dot pulls back from the ball and snaps forward
			FVector2D S;
			if (A && PC && PC->ProjectWorldLocationToScreen(A->BallLocation(), S, true))
			{
				const float Ph = FMath::Fmod(T, 1.8f) / 1.8f;
				const float Pull = Ph < 0.7f ? Ph / 0.7f : 1.f - (Ph - 0.7f) / 0.3f;
				const FVector2D Back = FVector2D(0.f, 1.f) * 260.f * U * Pull;
				const FVector2D P = S + Back;
				for (int32 k = 0; k < 10; ++k)
				{
					const FVector2D Q = S + Back * (k / 10.f);
					FCanvasTileItem D(Q - FVector2D(4.f, 4.f) * U, FVector2D(8.f, 8.f) * U, FLinearColor(1.f, 1.f, 1.f, 0.6f)); D.BlendMode = SE_BLEND_Translucent; C->DrawItem(D);
				}
				FCanvasTileItem Dot(P - FVector2D(26.f, 26.f) * U, FVector2D(52.f, 52.f) * U, FLinearColor(1.f, 0.82f, 0.25f, 0.85f)); Dot.BlendMode = SE_BLEND_Translucent; C->DrawItem(Dot);
				ZoogiFunCanvasText(C, Ph < 0.7f ? TEXT("PULL") : TEXT("LET GO!"), P.X, P.Y + 34.f * U, 30.f * U, FLinearColor::White, 1, 0);
			}
		}
		else if (Step == Rolling)
		{
			if (T < 1.4f) Card(TEXT("2"), TEXT("EVERY LAUNCH HITS BOOSTED"), TEXT("No timing tap needed - your first hit always lands +35%"), CardY);
			else
			{
				Card(TEXT("3"), TEXT("TAP REACTION TO REDIRECT"), FString::Printf(TEXT("Pick a target in range - 1 crystal each (%d left)"), A ? ZoogiRedirect::Crystals(G, A->PlayerIndex) : 0), CardY);
				// arrow to the REACTION button (right edge, 60 % down)
				const FVector2D To(Wd - 150.f * U, Ht * 0.6f), From(Wd * 0.62f, Ht * 0.6f);
				const float Wob = 18.f * U * FMath::Sin(T * 8.f);
				FCanvasLineItem L(From, To - FVector2D(40.f * U - Wob, 0.f)); L.LineThickness = 10.f * U; L.SetColor(FLinearColor(1.f, 0.82f, 0.25f, 1.f)); C->DrawItem(L);
				FCanvasLineItem H1(To - FVector2D(40.f * U - Wob, 0.f), To - FVector2D(80.f * U - Wob, 30.f * U)); H1.LineThickness = 10.f * U; H1.SetColor(FLinearColor(1.f, 0.82f, 0.25f, 1.f)); C->DrawItem(H1);
				FCanvasLineItem H2(To - FVector2D(40.f * U - Wob, 0.f), To - FVector2D(80.f * U - Wob, -30.f * U)); H2.LineThickness = 10.f * U; H2.SetColor(FLinearColor(1.f, 0.82f, 0.25f, 1.f)); C->DrawItem(H2);
			}
		}
		else if (Step == Picked && !ZoogiRedirect::Picking())
			Card(TEXT("4"), TEXT("CHAIN IT INTO A COMBO!"), FString::Printf(TEXT("Tap REACTION again - %d crystal%s left. Pick a red rival to start a DUEL"), A ? ZoogiRedirect::Crystals(G, A->PlayerIndex) : 0, (A && ZoogiRedirect::Crystals(G, A->PlayerIndex) == 1) ? TEXT("") : TEXT("s")), CardY);
		else if (Step == Duel)
		{
			UZoogiFunSys* F = UZoogiFunSys::Get(G);
			(void)F;
			Card(TEXT("5"), TEXT("DUEL!"), TEXT("Pick SKILL, then tap REACTION when the rings meet for the add-on"), Ht * (bPort ? 0.83f : 0.8f));
		}
	}
}
