# b172e (Evan: play guide = a hands-free AI trainer that guides AND performs each step, pausing at each; story box at the bottom with the
# crystal fox as Trainer Frosti; steps: Bolt Balls knocks orbs off, Ice Wall stops a self knockout / blocks a shot). -ZoogiOldGuide = b66.
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiGuide.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    if s.count(old)==1: s=s.replace(old,new); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('#include "Misc/Parse.h"\n','''#include "Misc/Parse.h"
#include "ZoogiSkills.h"
#include "ZoogiOrb.h"
#include "ZoogiP5Style.h"
''')
ed('''	bool DoneBefore() { return GuideAlreadyDone(); }''','''	namespace
	{	// ---- b172e trainer guide (v2)
		enum EG2 : int32 { G2Off = 0, G2Intro, G2BoltTalk, G2BoltDo, G2BoltSee, G2WallTalk, G2WallDo, G2WallSee, G2BlockTalk, G2ShotTalk, G2ShotDo, G2Done };
		int32 S2 = G2Off; double S2At = 0.0, S2NextFire = 0.0; int32 S2Fired = 0;
		bool OldGuide() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGuide")); return b; }
		void Set2(int32 V) { if (V != S2) { S2 = V; S2At = Now(); UE_LOG(LogZoogi, Log, TEXT("GUIDE2 step %d"), V); } }
		float Hold2(int32 V) { switch (V) { case G2Intro: return 4.5f; case G2BoltTalk: return 5.f; case G2BoltSee: return 3.5f; case G2WallTalk: return 5.f; case G2WallSee: return 3.f; case G2BlockTalk: return 5.5f; case G2ShotTalk: return 3.5f; default: return 0.f; } }
	}

	bool DoneBefore() { return GuideAlreadyDone(); }''')
ed('''	bool Active() { return Step > Off && Step < Done; }
	bool HideMenus() { return Step == Aim; }
	bool WantSlow() { return Step == Rolling && Now() < SlowUntil; }''','''	bool Active() { return OldGuide() ? (Step > Off && Step < Done) : (S2 > G2Off && S2 < G2Done); }
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
		switch (S2)
		{
		case G2Intro: if (T > Hold2(S2)) Set2(G2BoltTalk); break;
		case G2BoltTalk: if (T > Hold2(S2)) { ZoogiSkills::ArmBolts(G, HumanIdx, 3); S2Fired = 0; S2NextFire = Now() + 0.3; Set2(G2BoltDo); } break;
		case G2BoltDo:
			if (Now() >= S2NextFire && S2Fired < 3) { if (ZoogiSkills::BoltGo(G)) ++S2Fired; S2NextFire = Now() + 0.8; }
			if (S2Fired >= 3 || T > 6.f) Set2(G2BoltSee);
			break;
		case G2BoltSee: if (T > Hold2(S2)) Set2(G2WallTalk); break;
		case G2WallTalk: if (T > Hold2(S2)) { ZoogiSkills::CpuCast(G, FName(TEXT("ice_wall"))); Set2(G2WallDo); } break;
		case G2WallDo: if ((T > 1.2f && !ZoogiSkills::CpuPicking() && !ZoogiSkills::Picking() && !ZoogiSkills::EffectBusy()) || T > 7.f) Set2(G2WallSee); break;
		case G2WallSee: if (T > Hold2(S2)) Set2(G2BlockTalk); break;
		case G2BlockTalk: if (T > Hold2(S2)) Set2(G2ShotTalk); break;
		case G2ShotTalk:
			if (T > Hold2(S2) && A && !A->bCPU)
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
		const float M = 16.f * U, BoxH = (bPort ? 300.f : 250.f) * U, BoxY = Ht - BoxH - M - (bPort ? 30.f * U : 10.f * U);
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
		if (Hold > 0.f)
		{
			const float K = FMath::Clamp(T / Hold, 0.f, 1.f);
			Rect(TX, BoxY + BoxH - 26.f * U, TW, 8.f * U, FLinearColor(1.f, 1.f, 1.f, 0.15f));
			Rect(TX, BoxY + BoxH - 26.f * U, TW * K, 8.f * U, FLinearColor(0.47f, 0.78f, 1.f, 0.95f));
			ZoogiFunCanvasText(C, TEXT("NEXT STEP"), TX + TW, BoxY + BoxH - 46.f * U, 22.f * U, FLinearColor(0.47f, 0.78f, 1.f, 1.f), 2, 0);
		}
	}''')
ed('''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;''','''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		if (!OldGuide()) { Tick2(G); return; } // b172e''')
ed('''	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!Active() || !G || !C) return;''','''	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!Active() || !G || !C) return;
		if (!OldGuide()) { Draw2(G, C); return; } // b172e''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
