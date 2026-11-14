#include "ZoogiStandForms.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiAurexPuppet.h"
#include "ZoogiActionFun.h"
#include "ZoogiEdgeHang.h"
#include "ZoogiFX.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiStandForms
{
	namespace
	{
		struct FStand { TWeakObjectPtr<AZoogiBall> Ball; TWeakObjectPtr<AZoogiAurexPuppet> Pup; };
		TMap<TWeakObjectPtr<AZoogiBall>, float> RestT;      // b67: how long each Zoogi has been at rest
		TMap<TWeakObjectPtr<AZoogiBall>, int32> LeftTurn;   // b67: the turn it last left its stand (its own action) - no re-stand that turn
		int32 SoundTurn = -1;
		TArray<FStand> Stands;
		TWeakObjectPtr<AZoogiGameMode> ForGame;
		int32 ForSeed = 0;
		bool bDone = false, bSpawning = false;
		bool Off() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStandForms")); return b; } // b75 (Evan): no standing forms - every Zoogi stays a marble (-ZoogiStandForms = the b63-b70 stance)
	}

	bool IsStanding(const AZoogiBall* B)
	{
		for (const FStand& S : Stands) if (S.Ball.Get() == B && S.Pup.IsValid()) return true;
		return false;
	}

	void Release(AZoogiBall* B, bool bFx)
	{
		if (bSpawning) return;
		for (int32 i = Stands.Num() - 1; i >= 0; --i)
			if (Stands[i].Ball.Get() == B)
			{
				if (AZoogiAurexPuppet* P = Stands[i].Pup.Get()) P->Dismiss(bFx);
				Stands.RemoveAt(i);
				if (B && ForGame.IsValid()) LeftTurn.Add(B, ForGame->TurnCounter);
			}
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (Off() || !G || G->bRoyaleMode) return;
		if (ForGame.Get() != G || ForSeed != G->GameSeed) { Stands.Reset(); RestT.Reset(); LeftTurn.Reset(); SoundTurn = -1; ForGame = G; ForSeed = G->GameSeed; bDone = false; }
		// spawn once per match, as the first turn opens (after the forming / drop intro)
		if (!bDone && G->Phase == EZoogiPhase::Aiming && G->TurnCounter <= 1)
		{
			bDone = true;
			UWorld* W = G->GetWorld();
			const FVector C0 = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			int32 N = 0;
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut) continue;
				if (B != G->GetActiveBall() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStandAll"))) continue; // b70 (Evan): "show play stance only at turn start" - the turn player only
				const FVector L = B->BallLocation();
				const float Yaw = (C0 - L).GetSafeNormal2D().Rotation().Yaw; // facing the middle of the court
				bSpawning = true;
				AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(W, B, L - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), Yaw, EZoogiPuppetPose::Stand, 178.f, nullptr, B == G->GetActiveBall()); // b67: only the turn player's form makes a sound
				bSpawning = false;
				if (!P) continue;
				P->bFollowBall = true;
				P->FaceDir = (C0 - L).GetSafeNormal2D();
				FStand S; S.Ball = B; S.Pup = P; Stands.Add(S); ++N;
			}
			UE_LOG(LogZoogi, Log, TEXT("STANDFORMS: %d Zoogis start standing as their ultimate forms"), N);
			if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("standforms_a"), 0.6f); G->TakeShotDelayed(TEXT("standforms_b"), 1.6f); }
		}
		// back into the ball on its own action (launch) or when knocked
		for (int32 i = Stands.Num() - 1; i >= 0; --i)
		{
			AZoogiBall* B = Stands[i].Ball.Get();
			AZoogiAurexPuppet* P = Stands[i].Pup.Get();
			if (!B || !P) { Stands.RemoveAt(i); continue; }
			const bool bActed = G->GetActiveBall() == B && G->Phase == EZoogiPhase::Rolling;
			const bool bKnocked = B->GetBallVelocity().Size2D() > 140.f || B->bOut || (B != G->GetActiveBall() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStandAll"))) || (UZoogiFunSys::Get(G) && UZoogiFunSys::Get(G)->IsReplaying()); // b70: not its turn = back to the ball; replays show balls
			if (bActed || bKnocked)
			{
				UE_LOG(LogZoogi, Log, TEXT("STANDFORMS: P%d back to its ball (%s)"), B->PlayerIndex + 1, bActed ? TEXT("took its action") : TEXT("knocked"));
				P->Dismiss(bActed); // b67: knocked = quietly back into the ball (no transform flash before / at the hit)
				Stands.RemoveAt(i);
				if (bActed) LeftTurn.Add(B, G->TurnCounter);
				RestT.Remove(B);
			}
		}
		static const bool bOnce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStandOnce"));
		if (bOnce || !bDone) return;
		// b67: the turn player's form - the transform sound plays now, on its turn (and only then)
		if (G->Phase == EZoogiPhase::Aiming && SoundTurn != G->TurnCounter)
			if (AZoogiBall* A = G->GetActiveBall())
				if (IsStanding(A)) { SoundTurn = G->TurnCounter; ZoogiUtil::PlaySfx(G, TEXT("AurexIn"), 0.7f); }
		// b67: whoever has stopped rolling stands up again, quietly
		UZoogiFunSys* F = UZoogiFunSys::Get(G);
		const bool bBusy = (F && (F->ClashOn() || F->IsReplaying())) || ZoogiEdgeHang::Active() || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver;
		UWorld* W = G->GetWorld();
		const FVector C0 = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bOut || IsStanding(B)) { if (B) RestT.Remove(B); continue; }
			const bool bRest = B->GetBallVelocity().Size() < 18.f && !bBusy && !(F && F->IsFlung(B));
			float& Rt = RestT.FindOrAdd(B);
			Rt = bRest ? Rt + RealDt : 0.f;
			if (Rt < 0.6f) continue;
			if (B != G->GetActiveBall() || G->Phase != EZoogiPhase::Aiming) continue; // b70: the stance is the turn start only
			if (B == G->GetActiveBall() && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling)) { const int32* Lt = LeftTurn.Find(B); if (Lt && *Lt == G->TurnCounter) continue; } // it is acting this turn
			if (G->Arena && G->Arena->IsOffCourt(FVector2D(B->BallLocation()))) continue;
			bool bOther = false; // another form (edge hang / flung / clash) owns it
			for (TActorIterator<AZoogiAurexPuppet> It(W); It; ++It) if (It->Ball.Get() == B) { bOther = true; break; }
			if (bOther) continue;
			const FVector L = B->BallLocation();
			bSpawning = true;
			AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(W, B, L - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), (C0 - L).GetSafeNormal2D().Rotation().Yaw, EZoogiPuppetPose::Stand, 178.f, nullptr, false);
			bSpawning = false;
			RestT.Remove(B);
			if (!P) continue;
			P->bFollowBall = true; P->FaceDir = (C0 - L).GetSafeNormal2D();
			FStand S; S.Ball = B; S.Pup = P; Stands.Add(S);
			UE_LOG(LogZoogi, Log, TEXT("STANDFORMS: P%d stopped rolling -> stands (quiet)"), B->PlayerIndex + 1);
		}
	}
}
