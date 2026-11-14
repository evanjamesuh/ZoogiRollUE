// Nov 2 b66: see ZoogiRedirect.h
#include "ZoogiRedirect.h"
#include "ZoogiPauseOrbit.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiCPUSmart.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiActionFun.h"
#include "ZoogiSkills.h"
#include "ZoogiHaptics.h"
#include "ZoogiPlayerController.h"
#include "Containers/Ticker.h"
#include "ZoogiBanter.h"
#include "ZoogiAnimeSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "CanvasItem.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiSkillLab.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace ZoogiRedirect
{
	namespace
	{
		bool bPick = false;
		double PickT0 = 0.0;
		TWeakObjectPtr<AZoogiBall> PickBall;
		int32 ComboN = 0, LastTurn = -1, CpuUsed = 0;
		int32 Free[4] = { 0, 0, 0, 0 };
		bool Bolt[4] = { false, false, false, false };
		bool bBoltLaunch = false;
		TWeakObjectPtr<AActor> Homing;
		double HomingUntil = 0.0, CpuNextAt = 0.0, LastFireAt = -10.0;
		double SpdCheckAt = 0.0; TWeakObjectPtr<AZoogiBall> SpdCheckBall; float SpdFired = 0.f; // b125: speed 0.25 s after a redirect (log)
		float HomeFloor = 0.f; // b126: the redirected Zoogi never drops below this until it reaches its target
		TWeakObjectPtr<AZoogiBall> DuelShooter, DuelTarget;
		int32 TestStep = 0; double TestAt = 0.0;
		// b67 (Evan: the CPU plays as a person): the CPU pauses the roll too, its cursor glides to the target it picked, then taps it
		bool bCpuPick = false; TWeakObjectPtr<AActor> CpuGoal; double CpuTapAt = 0.0;
		TWeakObjectPtr<AActor> HitWatch; double HitWatchUntil = 0.0; // b70: the redirect's impact (feel)

		double Now() { return FPlatformTime::Seconds(); }
		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoRedirect")); return b; }
		bool TestOn() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectTest")); return b; }
		FVector Loc(AActor* A)
		{
			if (AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
			if (AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
			return A ? A->GetActorLocation() : FVector::ZeroVector;
		}
		UZoogiFunSys* Fun(AZoogiGameMode* G) { return G ? UZoogiFunSys::Get(G) : nullptr; }
		TArray<AActor*> Candidates(AZoogiGameMode* G, AZoogiBall* B)
		{
			TArray<AActor*> Out;
			if (!G || !B) return Out;
			const float R = Range(G);
			const FVector P = B->BallLocation();
			TArray<AActor*> All;
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) All.Add(O);
			for (AZoogiBall* Rb : G->Balls) if (Rb && Rb != B && !Rb->bOut && !G->IsAlly(Rb, B)) All.Add(Rb);
			for (AActor* T : All) if (T && FVector::Dist2D(Loc(T), P) <= R && FVector::Dist2D(Loc(T), P) > 60.f) Out.Add(T);
			return Out;
		}
		void Push(AActor* T, const FVector& Dir, float Speed)
		{
			if (AZoogiBall* B = Cast<AZoogiBall>(T)) { B->SetPlanarVelocity(Dir * Speed, 160.f); return; }
			if (UPrimitiveComponent* P = T ? Cast<UPrimitiveComponent>(T->GetRootComponent()) : nullptr)
				if (P->IsSimulatingPhysics()) { P->WakeAllRigidBodies(); P->SetPhysicsLinearVelocity(Dir * Speed + FVector(0, 0, 120.f)); }
		}
		// b72 (Evan: "more of the anime electricity like this, and more flicker duration"): repeat the electric shock N times over Span s
		void ElecFlicker(UWorld* W, const FVector& At, float Scale, int32 N, float Span, TWeakObjectPtr<AActor> Follow = nullptr)
		{
			if (!W) return;
			AZoogiFX::Stylized(W, TEXT("ElectricShock"), At, Scale, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
			TWeakObjectPtr<UWorld> WW = W;
			for (int32 k = 1; k < N; ++k)
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WW, At, Scale, Follow, k](float)
				{
					if (UWorld* Wd = WW.Get())
					{
						const FVector P = (Follow.IsValid() ? Loc(Follow.Get()) : At) + FVector(FMath::FRandRange(-50.f, 50.f), FMath::FRandRange(-50.f, 50.f), 0.f);
						AZoogiFX::Stylized(Wd, (k % 3 == 2) ? TEXT("ElectricShock_v2") : TEXT("ElectricShock"), P, Scale * FMath::FRandRange(0.75f, 1.3f), FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
					}
					return false;
				}), Span * k / N + FMath::FRandRange(0.f, 0.07f));
		}
		void Zap(AZoogiGameMode* G, AZoogiBall* B)
		{
			// Bolt Balls: the old electric shock (VFX_PackVol1 P_ElectricShock, the classic StaticShock power) jumps to the 3 nearest targets
			UWorld* W = G->GetWorld();
			TArray<AActor*> Ts = Candidates(G, B);
			const FVector P = B->BallLocation();
			Ts.Sort([&P](const AActor& X, const AActor& Y) { return FVector::DistSquared2D(Loc(const_cast<AActor*>(&X)), P) < FVector::DistSquared2D(Loc(const_cast<AActor*>(&Y)), P); });
			ElecFlicker(W, P, 1.3f, 6, 1.4f, B); // b72: longer, flickering
			for (int32 i = 0; i < Ts.Num() && i < 3; ++i)
			{
				ElecFlicker(W, Loc(Ts[i]), 1.1f, 5, 1.2f, Ts[i]);
				AZoogiFX::Spawn(W, Loc(Ts[i]), FLinearColor(1.f, 0.95f, 0.3f), 160.f, 0.35f, true, 12.f);
				Push(Ts[i], (Loc(Ts[i]) - P).GetSafeNormal2D(), 900.f * ZoogiSkillLab::StrengthOf(FName(TEXT("bolt_balls")))); // b136: lab strength
				if (AZoogiBall* Rb = Cast<AZoogiBall>(Ts[i])) Rb->LastHitPlayer = B->PlayerIndex;
			}
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 1.2f);
			UE_LOG(LogZoogi, Log, TEXT("REDIRECT bolt zap from P%d: %d targets"), B->PlayerIndex + 1, FMath::Min(3, Ts.Num()));
		}
		void Fire(AZoogiGameMode* G, AZoogiBall* B, AActor* T)
		{
			if (!G || !B || !T) return;
			const int32 Pi = B->PlayerIndex;
			const bool bFree = Pi >= 0 && Pi < 4 && Free[Pi] > 0;
			if (bFree) --Free[Pi];
			else if (Pi >= 0 && Pi < 4) G->AwakenCharge[Pi] = FMath::Max(0, G->AwakenCharge[Pi] - 1);
			const FVector From = B->BallLocation();
			const FVector Dir = (Loc(T) - From).GetSafeNormal2D();
			const float Cur = B->GetBallVelocity().Size2D();
			// b125 (Evan: "the zoogi barely moves in that direction"): a harder kick - 2200..3400 cm/s (was 1500..3000; most redirects came
			// out at the 1875 floor). -ZoogiRedirectSoft = the b70 speeds
			static const bool bSoft = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectSoft"));
			const float Spd = bSoft ? FMath::Clamp(FMath::Max(Cur, 1500.f) * 1.25f, 1500.f, 3000.f) : FMath::Clamp(FMath::Max(Cur, 1700.f) * 1.3f, 2200.f, 3400.f); // b70 (Evan): "powerful and swift"
			SpdCheckAt = Now() + 0.25; SpdCheckBall = B; SpdFired = Spd;
			HomeFloor = bSoft ? 0.f : Spd * 0.9f;
			// b180 (Evan: "if airborne during reaction the player must shoot down to the arena at target - it stayed airborne"): a Zoogi
			// in the air dives straight at the target (3D aim); -ZoogiRedirectFlat = the old flat shot
			static const bool bFlat = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectFlat"));
			const FVector Tl = Loc(T);
			if (!bFlat && From.Z - Tl.Z > ZoogiConst::ZoogiRadius * 1.2f)
			{
				const FVector D3 = (Tl - From).GetSafeNormal();
				B->SetPlanarVelocity(FVector(D3.X, D3.Y, 0.f) * Spd, FMath::Min(-300.f, D3.Z * Spd));
				UE_LOG(LogZoogi, Log, TEXT("REDIRECT P%d airborne (%.0f cm above the target): dives down at it"), Pi + 1, From.Z - Tl.Z);
			}
			else B->SetPlanarVelocity(Dir * Spd);
			Homing = T; HomingUntil = Now() + (FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectSoft")) ? 0.9 : 1.2);
			HitWatch = T; HitWatchUntil = Now() + 1.8;
			LastFireAt = Now();
			++ComboN;
			AZoogiBall* Rb = Cast<AZoogiBall>(T);
			if (Rb && !G->IsAlly(Rb, B)) { DuelShooter = B; DuelTarget = Rb; }
			UWorld* W = G->GetWorld();
			const FLinearColor Col = ComboN >= 4 ? FLinearColor(1.f, 0.3f, 0.9f) : ComboN >= 2 ? FLinearColor(1.f, 0.7f, 0.15f) : FLinearColor(0.35f, 0.9f, 1.f);
			AZoogiFX::Spawn(W, From, Col, 220.f + 40.f * ComboN, 0.4f, true, 12.f);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 0.9f + 0.1f * ComboN);
			if (Pi >= 0 && Pi < 4 && Bolt[Pi]) Zap(G, B);
			if (ComboN >= 3) ZoogiCosmosBoss::OnEvent(W, TEXT("Combo")); // b136: the cosmos boss leans in
			if (UZoogiFunSys* F = Fun(G))
				F->PublicCallout(FString::Printf(TEXT("COMBO x%d"), ComboN), Rb ? FString::Printf(TEXT("SMASH INTO %s!"), *Rb->Def.Name.ToUpper()) : FString::Printf(TEXT("REDIRECT  -  %d CRYSTAL%s LEFT"), Crystals(G, Pi), Crystals(G, Pi) == 1 ? TEXT("") : TEXT("S")),
					Col, 1.1f, 0.8f);
			UE_LOG(LogZoogi, Log, TEXT("REDIRECT P%d -> %s (combo %d, %s, %.0f cm/s, crystals left %d)"), Pi + 1, *T->GetName(), ComboN, bFree ? TEXT("free pick") : TEXT("1 crystal"), Spd, Crystals(G, Pi));
			if (ComboN == 3) { const int32 Rv = ZoogiBanter::RandomRival(G, Pi); if (Rv >= 0) ZoogiBanter::Exchange(G, Pi, Rv, ZoogiBanter::EKind::Combo, ZoogiBanter::EKind::ComboReact); } // b67
			if (G->ShotDir.Len()) G->TakeShot(FString::Printf(TEXT("redirect_%02d_fire"), ComboN));
		}
		void Close() { bPick = false; PickBall.Reset(); bCpuPick = false; CpuGoal.Reset(); }
	}

	float Range(AZoogiGameMode* G) { return G ? FMath::Max(800.f, G->ArcRange()) : 900.f; }
	int32 Combo() { return ComboN; }
	bool Picking() { return bPick; }
	static bool bPend = false; static FVector2D PendAt = FVector2D::ZeroVector; static double PendT = 0.0; // b284: a press waiting to become a tap or a swipe
	void CancelPick(const TCHAR* Why) { if (!bPick) return; UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick closed by %s (nothing spent)"), Why); Close(); }
	int32 Crystals(AZoogiGameMode* G, int32 P) { return (G && P >= 0 && P < 4) ? G->AwakenCharge[P] + Free[P] : 0; }
	void GrantFree(int32 P, int32 N) { if (P >= 0 && P < 4) Free[P] += N; }
	void ArmBolt(int32 P) { if (P >= 0 && P < 4) { Bolt[P] = true; bBoltLaunch = true; } }
	void Reset() { Close(); ComboN = 0; LastTurn = -1; CpuUsed = 0; for (int32 i = 0; i < 4; ++i) { Free[i] = 0; Bolt[i] = false; } Homing.Reset(); DuelShooter.Reset(); DuelTarget.Reset(); }

	bool TakeDuel(AZoogiBall* Sh, AZoogiBall* R)
	{
		if (!Sh || !R || DuelShooter.Get() != Sh || DuelTarget.Get() != R) return false;
		DuelShooter.Reset(); DuelTarget.Reset();
		return !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDuel"));
	}

	bool Available(AZoogiGameMode* G)
	{
		if (Off() || bPick || !G || G->Phase != EZoogiPhase::Rolling || ZoogiSkills::Picking()) return false;
		AZoogiBall* B = G->GetActiveBall();
		if (!B || B->bCPU || B->bOut) return false;
		UZoogiFunSys* F = Fun(G);
		if (F && F->ClashOn()) return false;
		if (UZoogiAnimeSubsystem* An = G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) if (An->CrashOn()) return false;
		if (G->PhaseTime > 14.f || B->GetBallVelocity().Size2D() < 90.f) return false;
		return Crystals(G, B->PlayerIndex) > 0 && Candidates(G, B).Num() > 0;
	}

	bool Begin(AZoogiGameMode* G)
	{
		if (!Available(G)) return false;
		bPick = true; bPend = false; PickT0 = Now(); PickBall = G->GetActiveBall();
		// the pick needs the top view: end a cut-in (its close-up form camera) that is still playing
		if (UZoogiAnimeSubsystem* An = G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) if (An->bPlaying && !An->CrashOn()) An->Skip();
		ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.9f, 0.8f);
		UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick open (P%d, %d crystals, %d targets in %.0f)"), PickBall->PlayerIndex + 1, Crystals(G, PickBall->PlayerIndex), Candidates(G, PickBall.Get()).Num(), Range(G));
		if (G->ShotDir.Len()) G->TakeShot(FString::Printf(TEXT("redirect_%02d_pick"), ComboN + 1));
		return true;
	}

	void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease)
	{
		// b125 (Evan: "it fires when I release - go back to when it worked right"): the target is picked on the PRESS again (the finger's
		// real spot - no lifted-finger position); a press away from every target can still drag the camera round. -ZoogiPickOnRelease = b120
		static const bool bOnRelease = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPickOnRelease"));
		if (!bPick || bCpuPick || !G || !PC) return;
		// b284 (Evan: "let me swipe to look around during the pauses"): the big tap window took every press as a pick, so a swipe never
		// turned the camera. Now a press only marks the spot; a swipe (> 26 px) looks around and picks nothing, a tap (lifted, or held
		// still 0.2 s) picks at the PRESS spot. -ZoogiPickOnPress = b125 (pick the instant the finger lands)
		static const bool bOnPress = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPickOnPress"));
		FVector2D UsePtr = Ptr;
		if (bOnRelease) { if (!bRelease || ZoogiPauseOrbit::WasDrag()) return; }
		else if (bOnPress) { if (!bPress) return; }
		else
		{
			if (bPress) { bPend = true; PendAt = Ptr; PendT = Now(); return; }
			if (!bPend) return;
			if (ZoogiPauseOrbit::WasDrag()) { bPend = false; UE_LOG(LogZoogi, Log, TEXT("REDIRECT swipe: looking around (no pick)")); return; }
			if (!bRelease && !(bDown && Now() - PendT > 0.2)) return;
			bPend = false; UsePtr = PendAt;
		}
		AZoogiBall* B = PickBall.Get();
		if (!B) { Close(); return; }
		AActor* Best = nullptr; float BestD = 1e9f;
		for (AActor* T : Candidates(G, B))
		{
			FVector2D S;
			if (!PC->ProjectWorldLocationToScreen(Loc(T), S, false)) continue;
			const float D = FVector2D::Distance(S, UsePtr);
			if (D < BestD) { BestD = D; Best = T; }
		}
		int32 Vx = 0, Vy = 0; PC->GetViewportSize(Vx, Vy);
		if (Best && BestD < 0.20f * FMath::Max(1, FMath::Min(Vx, Vy)) + 80.f) { Close(); Fire(G, B, Best); } // b120h: a bigger tap window (was 0.12 + 50)
		else UE_LOG(LogZoogi, Log, TEXT("REDIRECT tap missed (nearest target %.0f px away)"), BestD);
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || Off()) return;
		if (G->TurnCounter != LastTurn)
		{
			LastTurn = G->TurnCounter;
			Close(); ComboN = 0; CpuUsed = 0; Homing.Reset(); DuelShooter.Reset(); DuelTarget.Reset(); bBoltLaunch = false;
			for (int32 i = 0; i < 4; ++i) { Free[i] = 0; Bolt[i] = false; }
			// crystals are earned over time: +1 at the start of each of that player's turns
			if (AZoogiBall* A = G->GetActiveBall())
				if (A->PlayerIndex >= 0 && A->PlayerIndex < 4)
				{
					G->AwakenCharge[A->PlayerIndex] = FMath::Max(G->AwakenCharge[A->PlayerIndex], FMath::Min(AZoogiGameMode::AwakenChargeMax, G->AwakenCharge[A->PlayerIndex] + 1)); // b141: keeps an Energy Buff stack
					if (TestOn() && !A->bCPU) G->AwakenCharge[A->PlayerIndex] = FMath::Max(G->AwakenCharge[A->PlayerIndex], AZoogiGameMode::AwakenChargeMax);
					UE_LOG(LogZoogi, Log, TEXT("REDIRECT turn %d: P%d crystals %d"), LastTurn, A->PlayerIndex + 1, G->AwakenCharge[A->PlayerIndex]);
				}
			TestStep = 0; TestAt = Now() + 1.2;
		}
		AZoogiBall* A = G->GetActiveBall();
		if (bPick)
		{
			if (!PickBall.IsValid() || G->Phase != EZoogiPhase::Rolling) Close();
			else if (bCpuPick)
			{
				AActor* Gt = CpuGoal.Get();
				AZoogiBall* Pb = PickBall.Get();
				if (!Gt || (Cast<AZoogiBall>(Gt) && Cast<AZoogiBall>(Gt)->bOut)) Close();
				else if (Now() >= CpuTapAt) { Close(); ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 1.2f); Fire(G, Pb, Gt); }
			}
			else if (Candidates(G, PickBall.Get()).Num() == 0)
			{	// b120h (Evan: "if no target the game stalls"): nothing left in the arc range - close at once, play goes on
				if (AZoogiBall* Pb = PickBall.Get()) G->PublicPopup(TEXT("NO TARGET IN RANGE"), Pb->BallLocation() + FVector(0, 0, 170.f), FLinearColor(1.f, 0.6f, 0.4f));
				Close(); UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick closed: no target in range (nothing spent)"));
			}
			else if (Now() - PickT0 > 30.0) { Close(); UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick timed out (nothing spent)")); } // b67: 30 s, the game stopped
		}
		if (SpdCheckAt > 0.0 && Now() >= SpdCheckAt)
		{
			SpdCheckAt = 0.0;
			if (AZoogiBall* Sb = SpdCheckBall.Get()) UE_LOG(LogZoogi, Log, TEXT("REDIRECT check P%d: fired %.0f cm/s, 0.25 s later %.0f cm/s (time dilation %.3f)"), Sb->PlayerIndex + 1, SpdFired, Sb->GetBallVelocity().Size2D(), G->GetWorldSettings() ? G->GetWorldSettings()->TimeDilation : -1.f);
		}
		// the ball homes in on its target for a moment so the smash lands where it was aimed
		if (AActor* T = Homing.Get())
		{
			if (A && Now() < HomingUntil && G->Phase == EZoogiPhase::Rolling && !A->bOut)
			{
				const FVector V = A->GetBallVelocity();
				const float S = V.Size2D();
				const FVector Want = (Loc(T) - A->BallLocation()).GetSafeNormal2D();
				const FVector Dir = FMath::Lerp(V.GetSafeNormal2D(), Want, FMath::Clamp(RealDt * 8.f, 0.f, 1.f)).GetSafeNormal2D();
				// b126: and it keeps its redirect speed until it gets there (it used to bleed to ~25% in a quarter second)
				const float S2 = FMath::Max(S, HomeFloor);
				static const bool bFlatHome = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectFlat"));
				const FVector Al = A->BallLocation(), Tl = Loc(T);
				if (!bFlatHome && Al.Z - Tl.Z > ZoogiConst::ZoogiRadius * 1.2f && (S > 200.f || HomeFloor > 0.f))
				{	// b180: still in the air - keep diving at the target
					const FVector W3 = (Tl - Al).GetSafeNormal();
					const float Sp = FMath::Max(V.Size(), HomeFloor);
					A->SetPlanarVelocity(FVector(W3.X, W3.Y, 0.f) * Sp, FMath::Min(-300.f, W3.Z * Sp));
				}
				else if ((S > 200.f || HomeFloor > 0.f) && FVector::Dist2D(Loc(T), A->BallLocation()) > ZoogiConst::ZoogiRadius * 2.5f) A->SetPlanarVelocity(Dir * S2);
				else if (FVector::Dist2D(Loc(T), A->BallLocation()) <= ZoogiConst::ZoogiRadius * 2.5f) HomeFloor = 0.f; // arrived: physics takes over
			}
			else { Homing.Reset(); HomeFloor = 0.f; }
		}
		// b70 (Evan: "each hit of a sequence reaction should feel powerful and swift - hard vibration each hit, camera shake and a slight
		// rock-crushing sound that doesn't linger")
		if (AActor* Ht = HitWatch.Get())
		{
			if (!A || Now() > HitWatchUntil || G->Phase != EZoogiPhase::Rolling) HitWatch.Reset();
			else if (FVector::Dist2D(Loc(Ht), A->BallLocation()) < ZoogiConst::ZoogiRadius * 2.f + 30.f)
			{
				HitWatch.Reset();
				UWorld* W = G->GetWorld();
				ZoogiHaptics::Rumble(0.16f, 1.f, W, TEXT("redirect hit"));
				if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(W->GetFirstPlayerController())) PC->OnImpactFeel(0.9f);
				ZoogiUtil::PlaySfxShort(G, TEXT("RockCrack"), 0.85f, FMath::FRandRange(1.0f, 1.15f), 0.3f);
				ZoogiUtil::PlaySfx(G, TEXT("HitClick"), 0.9f, 1.15f);
				AZoogiFX::Spawn(W, (Loc(Ht) + A->BallLocation()) * 0.5f, FLinearColor(1.f, 0.9f, 0.7f), 160.f, 0.18f, true, 10.f);
				ElecFlicker(W, (Loc(Ht) + A->BallLocation()) * 0.5f, 0.9f, 4, 0.8f, Ht); // b72 (Evan): anime electricity on every redirect hit
				UE_LOG(LogZoogi, Log, TEXT("REDIRECT hit %s (combo %d)"), *Ht->GetName(), ComboN);
			}
		}
		// Bolt Balls: the launch itself zaps once the ball is moving
		if (A && G->Phase == EZoogiPhase::Rolling && bBoltLaunch && A->PlayerIndex >= 0 && A->PlayerIndex < 4 && Bolt[A->PlayerIndex] && G->PhaseTime > 0.35f) { bBoltLaunch = false; Zap(G, A); }
		// CPU: spends earned crystals mid-roll (a rival in range first, else the orb nearest the rim), at most 2 a turn
		if (A && A->bCPU && G->Phase == EZoogiPhase::Rolling && !A->bOut && Now() >= CpuNextAt)
		{
			UZoogiFunSys* F = Fun(G);
			const bool bBusy = (F && F->ClashOn()) || ZoogiSkills::Picking() || bPick;
			// b124 (Evan: "CPUs should play like a person"): about to roll off the edge? a person spends a crystal to save the Zoogi
			bool bDanger = false;
			const FVector2D Av(A->GetBallVelocity()); const float AvS = Av.Size();
			if (ZoogiCPUSmart::On() && G->Arena && AvS > 120.f)
			{
				const float Stop = AvS * AvS / (2.f * FMath::Max(1.f, G->Arena->GetRollingDecel()));
				const float Ed = G->Arena->DistanceToEdge(FVector2D(A->BallLocation()), Av / AvS);
				bDanger = Ed >= 0.f && Stop > Ed - 60.f && Ed < 1100.f;
			}
			if (!bBusy && CpuUsed < 2 && Crystals(G, A->PlayerIndex) > 0 && G->PhaseTime > (bDanger ? 0.3f : 0.8f) && G->PhaseTime < 10.f && (bDanger || A->GetBallVelocity().Size2D() < 900.f) && A->GetBallVelocity().Size2D() > 120.f)
			{
				CpuNextAt = Now() + 0.6;
				TArray<AActor*> Ts = Candidates(G, A);
				AActor* Pick = nullptr; float Best = -1e9f;
				for (AActor* T : Ts)
				{
					const bool bRival = Cast<AZoogiBall>(T) != nullptr;
					float Score = (bRival ? 4000.f : 0.f) + Loc(T).Size2D() - 0.5f * FVector::Dist2D(Loc(T), A->BallLocation()) + FMath::FRandRange(0.f, 300.f);
					if (ZoogiCPUSmart::On())
					{
						Score = -ZoogiCPUSmart::TapRank(G, A, T) + FMath::FRandRange(0.f, 200.f);
						if (bDanger && AvS > 1.f) Score += 900.f * FVector2D::DotProduct((FVector2D(Loc(T)) - FVector2D(A->BallLocation())).GetSafeNormal(), -Av / AvS); // back into the court
					}
					if (Score > Best) { Best = Score; Pick = T; }
				}
				static const bool bInstant = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCpuInstant"));
				// b124: a rival in reach is a real chance (85%), an orb sometimes (40%), a save always
				const float Want = !ZoogiCPUSmart::On() ? 0.55f : (bDanger ? 1.f : (Cast<AZoogiBall>(Pick) ? 0.85f : 0.4f));
				if (bDanger && Pick) UE_LOG(LogZoogi, Log, TEXT("REDIRECT cpu P%d heading off the edge - redirects to save itself"), A->PlayerIndex + 1);
				if (Pick && FMath::FRand() < Want)
				{
					++CpuUsed;
					if (bInstant) Fire(G, A, Pick);
					else
					{	// b67: the same pause + pick a person goes through
						bPick = true; bCpuPick = true; PickT0 = Now(); PickBall = A; CpuGoal = Pick;
						CpuTapAt = PickT0 + FMath::FRandRange(1.0f, 1.6f);
						if (UZoogiAnimeSubsystem* An = G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) if (An->bPlaying && !An->CrashOn()) An->Skip();
						ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.9f, 0.8f);
						UE_LOG(LogZoogi, Log, TEXT("REDIRECT cpu P%d pauses to pick (%d crystals)"), A->PlayerIndex + 1, Crystals(G, A->PlayerIndex));
					}
				}
			}
		}
		// proofs (-ZoogiRedirectTest): the human redirects 3 times a turn, the last into a rival when one is in range
		if (TestOn() && A && !A->bCPU && G->Phase == EZoogiPhase::Rolling && Now() >= TestAt && TestStep < 3)
		{
			UZoogiFunSys* F = Fun(G);
			if (!(F && F->ClashOn()))
			{
				if (!bPick) { if (Begin(G)) TestAt = Now() + 0.8; else TestAt = Now() + 0.3; }
				else
				{
					TArray<AActor*> Ts = Candidates(G, A);
					AActor* Pick = nullptr;
					for (AActor* T : Ts) if (Cast<AZoogiBall>(T) && TestStep == 2) Pick = T;
					if (!Pick && Ts.Num()) Pick = Ts[FMath::RandRange(0, Ts.Num() - 1)];
					Close();
					if (Pick) Fire(G, A, Pick);
					++TestStep; TestAt = Now() + 1.1;
				}
			}
		}
	}

	static void Ring(UCanvas* C, APlayerController* PC, const FVector& Ctr, float R, float Th, const FLinearColor& Col, int32 Seg, bool bDash)
	{
		FVector2D Prev; bool bPrev = false;
		for (int32 k = 0; k <= Seg; ++k)
		{
			const float A = 2.f * PI * k / Seg;
			FVector2D Sp; const bool bOk = PC->ProjectWorldLocationToScreen(Ctr + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f), Sp, true);
			if (bOk && bPrev && (!bDash || k % 2 == 0)) { FCanvasLineItem L(Prev, Sp); L.LineThickness = Th; L.SetColor(Col); C->DrawItem(L); }
			Prev = Sp; bPrev = bOk;
		}
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!G || !C || !PC || Off()) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const float Pulse = 0.5f + 0.5f * FMath::Sin((float)Now() * 7.f);
		AZoogiBall* A = G->GetActiveBall();
		// combo counter (this turn)
		static const bool bSlab = FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboSlab")); // b177j (Evan: "move the combo to the left and only keep one style"): the left HUD badge shows it
		if (bSlab && ComboN > 0 && A && G->Phase == EZoogiPhase::Rolling)
		{
			const float Age = (float)(Now() - LastFireAt);
			const float Pop = 1.f + 0.35f * FMath::Clamp(1.f - Age / 0.25f, 0.f, 1.f);
			const float Yc = Ht * (Ht > Wd ? 0.2f : 0.22f); // b131: below the turn / round header (was 0.16 - on top of ROUND n / N)
			ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Yc), 300.f * U * Pop, 70.f * U * Pop, -6.f, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.85f));
			ZoogiFunCanvasText(C, FString::Printf(TEXT("COMBO  x%d"), ComboN), Wd * 0.5f, Yc, 48.f * U * Pop, ComboN >= 4 ? FLinearColor(1.f, 0.35f, 0.9f) : FLinearColor(1.f, 0.82f, 0.2f), 1, 0);
		}
		if (!bPick && A && !A->bCPU && Available(G))
		{	// b176h (Evan: "add the same trail the CPU have to player"): the dashed REACTION reach ring rides with your marble while REACT is up
			const FVector Fh(A->BallLocation().X, A->BallLocation().Y, A->BallLocation().Z - ZoogiConst::ZoogiRadius + 3.f);
			Ring(C, PC, Fh, Range(G), 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.55f), 96, true);
			Ring(C, PC, Fh, Range(G), 5.f * U, FLinearColor(0.35f, 0.9f, 1.f, 0.95f), 96, true);
		}
		if (!bPick) return;
		AZoogiBall* B = PickBall.Get();
		if (!B) return;
		const FVector Fl(B->BallLocation().X, B->BallLocation().Y, B->BallLocation().Z - ZoogiConst::ZoogiRadius + 3.f);
		const float R = Range(G);
		Ring(C, PC, Fl, R, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.55f), 96, true);
		Ring(C, PC, Fl, R, 5.f * U, FLinearColor(0.35f, 0.9f, 1.f, 0.95f), 96, true);
		for (AActor* T : Candidates(G, B))
		{
			const bool bRival = Cast<AZoogiBall>(T) != nullptr;
			const FVector P = Loc(T);
			const float Rr = (bRival ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius) + 30.f;
			const FVector F2(P.X, P.Y, Fl.Z);
			Ring(C, PC, F2, Rr * (0.9f + 0.25f * Pulse), 8.f * U, FLinearColor(0.f, 0.f, 0.f, 0.5f), 24, false);
			Ring(C, PC, F2, Rr * (0.9f + 0.25f * Pulse), 5.f * U, bRival ? FLinearColor(1.f, 0.3f, 0.35f, 1.f) : FLinearColor(1.f, 0.88f, 0.3f, 1.f), 24, false);
			FVector2D S0, S1;
			if (PC->ProjectWorldLocationToScreen(Fl, S0, true) && PC->ProjectWorldLocationToScreen(F2, S1, true))
			{
				FCanvasLineItem Ln(S0, S1); Ln.LineThickness = 2.f * U; Ln.SetColor(FLinearColor(1.f, 1.f, 1.f, 0.25f)); C->DrawItem(Ln);
			}
		}
		const int32 Cr = Crystals(G, B->PlayerIndex);
		ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Ht * 0.085f), 760.f * U, 92.f * U, -3.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.88f));
		if (bCpuPick)
		{	// b67: whose pick it is + its cursor gliding from the ball to the target
			ZoogiFunCanvasText(C, FString::Printf(TEXT("P%d %s  -  REACTION!"), B->PlayerIndex + 1, *B->Def.Name.ToUpper()), Wd * 0.5f, Ht * 0.085f - 16.f * U, 38.f * U, ZoogiUtil::PlayerColor(B->PlayerIndex), 1, 0);
			ZoogiFunCanvasText(C, FString::Printf(TEXT("PICKING A TARGET  -  %d CRYSTAL%s"), Cr, Cr == 1 ? TEXT("") : TEXT("S")), Wd * 0.5f, Ht * 0.085f + 24.f * U, 24.f * U, FLinearColor(1.f, 0.82f, 0.25f), 1, 0);
			if (AActor* Gt = CpuGoal.Get())
			{
				const float K = FMath::Clamp((float)((Now() - PickT0 - 0.25) / FMath::Max(0.3, CpuTapAt - PickT0 - 0.45)), 0.f, 1.f);
				const FVector P = FMath::Lerp(Fl, FVector(Loc(Gt).X, Loc(Gt).Y, Fl.Z), FMath::InterpEaseInOut(0.f, 1.f, K, 2.f));
				FVector2D Sc;
				if (PC->ProjectWorldLocationToScreen(P, Sc, true))
				{
					const float Rd = (K >= 1.f ? 30.f + 10.f * Pulse : 26.f) * U;
					FCanvasTileItem Dot(Sc - FVector2D(Rd, Rd), FVector2D(Rd, Rd) * 2.f, FLinearColor(1.f, 0.85f, 0.25f, 0.8f)); Dot.BlendMode = SE_BLEND_Translucent; C->DrawItem(Dot);
					FCanvasTileItem In(Sc - FVector2D(8.f, 8.f) * U, FVector2D(16.f, 16.f) * U, FLinearColor::White); In.BlendMode = SE_BLEND_Translucent; C->DrawItem(In);
				}
			}
			return;
		}
		ZoogiFunCanvasText(C, FString::Printf(TEXT("REDIRECT  -  TAP A GLOWING TARGET   %d"), FMath::Max(0, FMath::CeilToInt(30.0 - (Now() - PickT0)))), Wd * 0.5f, Ht * 0.085f - 16.f * U, 38.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, FString::Printf(TEXT("%d CRYSTAL%s  -  RIVALS (RED) START A DUEL"), Cr, Cr == 1 ? TEXT("") : TEXT("S")), Wd * 0.5f, Ht * 0.085f + 24.f * U, 24.f * U, FLinearColor(1.f, 0.82f, 0.25f), 1, 0);
	}
}
