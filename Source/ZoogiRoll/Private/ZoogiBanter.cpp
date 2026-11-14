// Nov 2 b67: see ZoogiBanter.h
#include "ZoogiBanter.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiP5Style.h"
#include "Engine/Canvas.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasBubble(UCanvas* C, float Wd, float Ht, float U, UTexture2D* Face, const FString& Name, const FString& Line, const FLinearColor& Col, float Age, float Life, int32 Slot);

namespace ZoogiBanter
{
	namespace
	{
		struct FLine { int32 Speaker = -1; FString Name, Text; float Delay = 0.f, Age = 0.f, Life = 2.6f; };
		TArray<FLine> Lines;
		double LastExchange = -100.0;
		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBanter")); return b; }
		double Now() { return FPlatformTime::Seconds(); }

		const TArray<const TCHAR*>& Pool(EKind K)
		{
			static const TArray<const TCHAR*> P[] = {
				/* Taunt */       { TEXT("%s, you're wide open!"), TEXT("Watch this one, %s."), TEXT("This orb is mine, %s!"), TEXT("Hope you like the edge, %s."), TEXT("Ready to fly, %s?"), TEXT("Stay still, %s - it hurts less.") },
				/* TauntReply */  { TEXT("Big talk. Prove it."), TEXT("You couldn't hit a wall."), TEXT("Go on then!"), TEXT("I've seen better rolls from a pebble."), TEXT("Bring it!"), TEXT("Funny. Now roll.") },
				/* DuelOpen */    { TEXT("You and me, %s. Right now!"), TEXT("Face me, %s!"), TEXT("No more running, %s!"), TEXT("Let's settle this, %s!") },
				/* DuelReply */   { TEXT("I was waiting for this!"), TEXT("You'll regret that!"), TEXT("Come and get it!"), TEXT("Finally - a real fight!") },
				/* WinGloat */    { TEXT("Too slow, %s!"), TEXT("Stay down, %s."), TEXT("That's how it's done!"), TEXT("Was that your best, %s?") },
				/* LoseRetort */  { TEXT("Lucky shot..."), TEXT("This isn't over!"), TEXT("Ugh - I'll be back!"), TEXT("You got lucky this time.") },
				/* KOGloat */     { TEXT("Bye bye, %s!"), TEXT("Out of the ring, %s!"), TEXT("Enjoy the view, %s!"), TEXT("And STAY out, %s!") },
				/* KOVictim */    { TEXT("Noooo!"), TEXT("I'll remember that!"), TEXT("Not fair!"), TEXT("Next turn you're mine!") },
				/* BigScore */    { TEXT("Did you see that?!"), TEXT("Count 'em up!"), TEXT("Clean sweep!"), TEXT("Now THAT'S a roll!") },
				/* BigScoreReact */ { TEXT("Show-off..."), TEXT("Okay, that was good."), TEXT("Don't get used to it."), TEXT("Hmph. My turn.") },
				/* MissMock */    { TEXT("Ha! Missed, %s!"), TEXT("Nice try, %s."), TEXT("Was that on purpose, %s?"), TEXT("The orbs are over THERE, %s.") },
				/* MissReply */   { TEXT("Just warming up!"), TEXT("Meant to do that."), TEXT("Laugh now..."), TEXT("Wait for it!") },
				/* Combo */       { TEXT("Chain reaction!"), TEXT("And again - and again!"), TEXT("Can't stop me now!"), TEXT("Combo time!") },
				/* ComboReact */  { TEXT("How is it still going?!"), TEXT("Somebody stop that ball!"), TEXT("No way!"), TEXT("Show-off!") },
				/* Awaken */      { TEXT("Feel that? I'm AWAKE!"), TEXT("Now I'm serious."), TEXT("Full power!"), TEXT("Time to shine!") },
				/* AwakenReact */ { TEXT("Uh oh..."), TEXT("That glow can't be good."), TEXT("Everyone - take cover!"), TEXT("Not again!") } };
			const int32 I = FMath::Clamp((int32)K, 0, (int32)UE_ARRAY_COUNT(P) - 1);
			return P[I];
		}
		FString Pick(EKind K, const FString& About)
		{
			const TArray<const TCHAR*>& P = Pool(K);
			FString S = P[FMath::RandRange(0, P.Num() - 1)];
			return S.Replace(TEXT("%s"), *About);
		}
		FString NameOf(AZoogiGameMode* G, int32 I) { return (G && G->Balls.IsValidIndex(I) && G->Balls[I]) ? G->Balls[I]->Def.Name : FString(); }
	}

	bool Busy() { return Lines.Num() > 0; }
	void Reset() { Lines.Reset(); LastExchange = -100.0; }

	int32 RandomRival(AZoogiGameMode* G, int32 Player)
	{
		if (!G) return -1;
		TArray<int32> R;
		for (int32 i = 0; i < G->Balls.Num(); ++i) if (i != Player && G->Balls[i] && !G->IsAlly(G->Balls[i], G->Balls.IsValidIndex(Player) ? G->Balls[Player] : nullptr)) R.Add(i);
		return R.Num() ? R[FMath::RandRange(0, R.Num() - 1)] : -1;
	}

	void Say(AZoogiGameMode* G, int32 Speaker, int32 About, EKind Kind)
	{
		if (Off() || !G || !G->Balls.IsValidIndex(Speaker) || !G->Balls[Speaker] || Lines.Num() >= 2) return;
		FLine L; L.Speaker = Speaker; L.Name = NameOf(G, Speaker); L.Text = Pick(Kind, NameOf(G, About));
		Lines.Add(L);
		UE_LOG(LogZoogi, Log, TEXT("BANTER %s: %s"), *L.Name, *L.Text);
	}

	void Exchange(AZoogiGameMode* G, int32 A, int32 B, EKind KindA, EKind KindB, bool bForce)
	{
		if (Off() || !G || !G->Balls.IsValidIndex(A) || !G->Balls.IsValidIndex(B) || A == B) return;
		if (!bForce && (Lines.Num() > 0 || Now() - LastExchange < 5.0)) return;
		if (bForce) Lines.Reset();
		LastExchange = Now();
		FLine La; La.Speaker = A; La.Name = NameOf(G, A); La.Text = Pick(KindA, NameOf(G, B));
		FLine Lb; Lb.Speaker = B; Lb.Name = NameOf(G, B); Lb.Text = Pick(KindB, NameOf(G, A)); Lb.Delay = 1.7f;
		Lines.Add(La); Lines.Add(Lb);
		UE_LOG(LogZoogi, Log, TEXT("BANTER %s: %s  /  %s: %s"), *La.Name, *La.Text, *Lb.Name, *Lb.Text);
	}

	void Tick(float RealDt)
	{
		for (FLine& L : Lines) { if (L.Delay > 0.f) L.Delay -= RealDt; else L.Age += RealDt; }
		Lines.RemoveAll([](const FLine& L) { return L.Delay <= 0.f && L.Age > L.Life + 0.4f; });
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, float Wd, float Ht, float U, bool bTopTaken)
	{
		if (Off() || !G || !C) return;
		int32 Slot = bTopTaken ? 1 : 0;
		for (const FLine& L : Lines)
		{
			if (L.Delay > 0.f) { ++Slot; continue; }
			ZoogiFunCanvasBubble(C, Wd, Ht, U, ZoogiP5::Portrait(L.Name), L.Name.ToUpper(), L.Text, ZoogiUtil::PlayerColor(L.Speaker), L.Age, L.Life, Slot);
			++Slot;
		}
	}
}
