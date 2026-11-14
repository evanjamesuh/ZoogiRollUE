// b182: SHOWTIME / GATTLING - see ZoogiGattling.h
#include "ZoogiGattling.h"
#include "ZoogiRaid.h" // b207
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiHaptics.h"
#include "ZoogiActionFun.h"
#include "ZoogiMusicSubsystem.h"
#include "ZoogiSkills.h"
#include "ZoogiRedirect.h"
#include "ZoogiFrozenFun.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);

namespace ZoogiGattling
{
	namespace
	{
		const FLinearColor Gold(1.f, 0.83f, 0.36f), Mag(1.f, 0.24f, 0.75f), Cyan(0.24f, 0.86f, 1.f), Ink(0.04f, 0.03f, 0.06f, 0.92f);
		const FLinearColor Fret[4] = { FLinearColor(0.94f, 0.24f, 0.24f), FLinearColor(1.f, 0.82f, 0.24f), FLinearColor(0.24f, 0.82f, 1.f), FLinearColor(0.9f, 0.35f, 1.f) };
		constexpr int32 NumBeats = 32;
		constexpr float DashSec = 0.16f;

		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoGattling")); return b; }
		bool TestOn() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGattlingTest")); return b; }
		// b187 (Evan: "make SHOWTIME activate automatic" / "I have it full and it's not activating"): a full meter starts the show by
		// itself on your next aim - no button, and you keep it even if you lose the lead (-ZoogiShowtimeButton = the b182 gold button)
		// b194 (Evan: "don't make SHOWTIME activate automatically - players must know whose show it is and activate it"): the P5 menu
		// SHOWTIME command starts it (-ZoogiShowtimeAuto = the b187 auto start)
		bool GMidRollOk = false; // b195: StartFromMenu(G, true) - a COMBO cast while rolling
		bool AutoMode() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeAuto")); return b; }
		bool ButtonMode() { return !AutoMode(); }
		float FillMul() { static const float F = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGattlingFast")) ? 4.f : 1.f; return F; }
		double Now() { return FPlatformTime::Seconds(); }

		struct FTarget { double Beat = 0.0; int32 Tile = 0; TWeakObjectPtr<AZoogiBall> Rival; int32 Res = 0; /*0 open, 1 hit, 2 miss*/ FString Grade; TWeakObjectPtr<AStaticMeshActor> Orb; };
		struct FPop { FVector At; FString Text; FLinearColor Col; double T0; };
		struct FState
		{
			TWeakObjectPtr<AZoogiGameMode> G;
			float Meter[4] = { 0.f, 0.f, 0.f, 0.f };
			int32 Turn = -1, TurnHits = 0; TSet<TWeakObjectPtr<AActor>> Touched;
			bool bActive = false, bFinale = false, bTestDone = false;
			int32 Player = -1; TWeakObjectPtr<AZoogiBall> Me;
			FVector C = FVector::ZeroVector; float R = 1500.f, TileR = 220.f, FloorZ = 0.f;
			TArray<FVector> Tiles; TArray<FTarget> T; TArray<FVector> Landed; TArray<FPop> Pops;
			int32 Next = 0, Combo = 0, BestCombo = 0, Shield = 2, Pts = 0, RivalHits = 0, Climb = 0;
			double T0 = 0.0, FinaleAt = 0.0, EndAt = 0.0, BeatLen = 60.0 / 158.45;
			FVector DashFrom = FVector::ZeroVector, DashTo = FVector::ZeroVector; double DashT0 = -10.0; int32 DashTarget = -1;
			FVector Pos = FVector::ZeroVector;
			TWeakObjectPtr<ACameraActor> Cam; TWeakObjectPtr<AActor> PrevView; float CamDist = 0.f, CamYaw = 0.f; bool bPortrait = false; // b195
			bool bHold = false; FBox2D Button = FBox2D(ForceInit);
		};
		FState S;

		const TArray<double>& BeatsFor(int32 Idx)
		{
			static TArray<double> B[3]; static bool bLoaded[3] = { false, false, false };
			Idx = FMath::Clamp(Idx, 1, 2);
			if (!bLoaded[Idx])
			{
				bLoaded[Idx] = true;
				TArray<FString> Lines;
				if (FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectContentDir() / FString::Printf(TEXT("Data/Music/Theme_Arena%d_beats.txt"), Idx))))
					for (const FString& L : Lines) if (L.Len() && !L.StartsWith(TEXT("#"))) B[Idx].Add(FCString::Atod(*L));
				UE_LOG(LogZoogi, Log, TEXT("GATTLING beat map Theme_Arena%d: %d beats"), Idx, B[Idx].Num());
			}
			return B[Idx];
		}

		bool Leading(const AZoogiGameMode* G, const AZoogiBall* Me)
		{
			if (!G || !Me) return false;
			for (const AZoogiBall* B : G->Balls) if (B && B != Me && !G->IsAlly(B, Me) && B->Score >= Me->Score) return false;
			return true;
		}
		bool ModeOk(const AZoogiGameMode* G) { return G && G->Arena && !G->bRoyaleMode && !G->bComboMode; }

		AZoogiBall* Human(const AZoogiGameMode* G) { for (AZoogiBall* B : G->Balls) if (B && !B->bCPU) return B; return nullptr; }

		bool CanStart(const AZoogiGameMode* G, bool bForce)
		{
			if (Off() || S.bActive || !ModeOk(G)) return false;
			AZoogiBall* Me = G->GetActiveBall();
			if (!Me || Me->bCPU || Me->bOut || Me->PlayerIndex < 0 || Me->PlayerIndex > 3) return false;
			if ((G->Phase != EZoogiPhase::Aiming && !(GMidRollOk && G->Phase == EZoogiPhase::Rolling)) || ZoogiSkills::Picking() || ZoogiRedirect::Picking()) return false; // b195: + a COMBO mid-roll
			if (bForce) return true;
			return S.Meter[Me->PlayerIndex] >= 0.999f; // b194: earned = yours (no lead check at start)
		}

		FVector TargetPos(const FTarget& Tg)
		{
			if (AZoogiBall* Rv = Tg.Rival.Get()) { FVector P = Rv->BallLocation(); P.Z = S.FloorZ; return P; }
			return S.Tiles.IsValidIndex(Tg.Tile) ? S.Tiles[Tg.Tile] : S.C;
		}

		AStaticMeshActor* SpawnOrb(UWorld* W, const FVector& At, const FLinearColor& Col)
		{
			FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(At), Sp);
			if (!A) return nullptr;
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetWorldScale3D(FVector(0.9f));
			M->SetMaterial(0, ZoogiUtil::GlowMID(A, Col, 4.f));
			M->SetCastShadow(false);
			return A;
		}

		void Begin(AZoogiGameMode* G, bool bForce)
		{
			if (!CanStart(G, bForce)) return;
			UWorld* W = G->GetWorld();
			AZoogiBall* Me = G->GetActiveBall();
			S.bActive = true; S.bFinale = false; S.Player = Me->PlayerIndex; S.Me = Me;
			S.C = G->Arena->GetActorLocation(); S.R = G->Arena->GetKnockoffRadius();
			S.FloorZ = Me->BallLocation().Z - ZoogiConst::ZoogiRadius; S.C.Z = S.FloorZ;
			S.TileR = S.R * 0.15f;
			S.Tiles.Reset();
			S.Tiles.Add(S.C);
			for (int32 i = 0; i < 6; ++i) { const float A = i * PI / 3.f + 0.3f; S.Tiles.Add(S.C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * S.R * 0.37f); }
			for (int32 i = 0; i < 12; ++i) { const float A = i * PI / 6.f + 0.05f; S.Tiles.Add(S.C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * S.R * 0.72f); }
			// the beats: from the theme's beat map when a theme plays, else a 158 bpm clock
			TArray<double> Wall;
			const double N0 = Now();
			int32 Idx = 0; double El = 0.0;
			UZoogiMusicSubsystem* Mu = UZoogiMusicSubsystem::Get(G);
			if (Mu && Mu->ThemeClock(Idx, El))
			{
				const TArray<double>& BM = BeatsFor(Idx);
				const double Len = Mu->ThemeLen > 1.0 ? Mu->ThemeLen : 1e9;
				for (int32 Loop = 0; Loop < 2 && Wall.Num() < NumBeats; ++Loop)
					for (double B : BM) { const double Bt = B + Loop * Len; if (Bt > El + 1.3) { Wall.Add(N0 + (Bt - El)); if (Wall.Num() >= NumBeats) break; } }
			}
			// b186 FIX: the beat map must start within a few seconds (B185: the theme had looped - with a 10000 s "length" the first
			// beat landed hours away and SHOWTIME sat frozen with no targets); otherwise the 158 bpm clock
			if (Wall.Num() && (Wall[0] < N0 + 0.3 || Wall[0] > N0 + 4.0)) { UE_LOG(LogZoogi, Warning, TEXT("GATTLING beat map off (first beat in %.1f s) -> clock"), Wall[0] - N0); Wall.Reset(); }
			if (Wall.Num() < NumBeats) { Wall.Reset(); for (int32 k = 0; k < NumBeats; ++k) Wall.Add(N0 + 1.3 + k * (60.0 / 158.45)); Idx = 0; }
			S.BeatLen = (Wall.Last() - Wall[0]) / FMath::Max(1, Wall.Num() - 1);
			// targets: hop tile to tile (neighbours), every 6th a frozen rival
			TArray<AZoogiBall*> Rivals; for (AZoogiBall* B : G->Balls) if (B && B != Me && !B->bOut && !G->IsAlly(B, Me)) Rivals.Add(B);
			S.T.Reset(); FRandomStream Rng(G->TurnCounter * 7919 + 11);
			int32 Cur = 0; float Best = 1e9f;
			for (int32 i = 0; i < S.Tiles.Num(); ++i) { const float D = FVector::Dist2D(S.Tiles[i], Me->BallLocation()); if (D < Best) { Best = D; Cur = i; } }
			int32 Prev = -1;
			for (int32 k = 0; k < Wall.Num(); ++k)
			{
				FTarget Tg; Tg.Beat = Wall[k];
				if (k % 6 == 5 && Rivals.Num()) Tg.Rival = Rivals[(k / 6) % Rivals.Num()];
				TArray<int32> Nb; for (int32 i = 0; i < S.Tiles.Num(); ++i) if (i != Cur && i != Prev && FVector::Dist2D(S.Tiles[i], S.Tiles[Cur]) < S.R * 0.75f) Nb.Add(i);
				Tg.Tile = Nb.Num() ? Nb[Rng.RandRange(0, Nb.Num() - 1)] : (Cur + 1) % S.Tiles.Num();
				Prev = Cur; Cur = Tg.Tile;
				S.T.Add(Tg);
			}
			S.Next = 0; S.Combo = S.BestCombo = 0; S.Shield = 2; S.Pts = 0; S.RivalHits = 0; S.Landed.Reset(); S.Pops.Reset();
			S.T0 = N0; S.FinaleAt = Wall.Last() + 0.35; S.EndAt = S.FinaleAt + 2.6; S.Climb = 0; // b183: +1 s to watch the blast S.DashT0 = -10.0; S.DashTarget = -1;
			S.Pos = Me->BallLocation(); S.Landed.Add(S.Pos);
			if (Me->Sphere) { Me->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); Me->Sphere->SetSimulatePhysics(false); }
			// camera: high over the court so the whole floor shows (portrait pulls back)
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				ACameraActor* Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Sp);
				if (Cam)
				{
					int32 Vx = 16, Vy = 9; PC->GetViewportSize(Vx, Vy);
					const float Fov = 70.f, HalfH = FMath::DegreesToRadians(Fov * 0.5f);
					const float HalfV = FMath::Atan(FMath::Tan(HalfH) * (float)FMath::Max(1, Vy) / (float)FMath::Max(1, Vx));
					const float Dist = S.R * 1.12f / FMath::Tan(FMath::Min(HalfH, HalfV));
					const float Yaw = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : 90.f;
					// b195 (Evan: "in portrait the SHOWTIME camera doesn't show the top view"): portrait looks straight down
					S.bPortrait = Vy > Vx;
					const FRotator Rot(S.bPortrait ? -86.f : -68.f, Yaw, 0.f);
					Cam->SetActorLocationAndRotation(S.C - Rot.Vector() * Dist, Rot);
					Cam->GetCameraComponent()->SetFieldOfView(Fov);
					Cam->GetCameraComponent()->bConstrainAspectRatio = false;
					S.Cam = Cam; S.PrevView = PC->GetViewTarget();
					PC->SetViewTargetWithBlend(Cam, 0.45f, VTBlend_Cubic);
				}
			}
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("SHOWTIME!"), TEXT("TAP TO THE BEAT - CHAIN THE DASHES"), Gold, 1.4f, 1.3f);
			if (ZoogiRaid::NewHud()) ZoogiRaid::HowTo(5); // b207 D: in a raid it's the BOSS ATTACK - the grade is the damage
			ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 0.8f, 1.1f);
			ZoogiHaptics::Kick(0.8f, W, TEXT("showtime_start"));
			ZoogiShow::Event(W, ZoogiShow::EEvent::BigScore, S.C, 3.f);
			UE_LOG(LogZoogi, Log, TEXT("GATTLING SHOWTIME P%d: %d beats (%s, beat %.3f s), %d tiles, %d rivals frozen"), S.Player + 1, S.T.Num(), Idx ? *FString::Printf(TEXT("Theme_Arena%d beat map"), Idx) : TEXT("158 bpm clock"), S.BeatLen, S.Tiles.Num(), Rivals.Num());
			if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("gattling_start"), 1.0f); G->TakeShotDelayed(TEXT("gattling_mid"), 5.5f); G->TakeShotDelayed(TEXT("gattling_late"), 10.f); }
		}

		void Pop(const FVector& At, const FString& Tx, const FLinearColor& Col) { S.Pops.Add({ At, Tx, Col, Now() }); }
		void Land(AZoogiGameMode* G, int32 k);

		void Resolve(AZoogiGameMode* G, int32 k, double TapAt, bool bAuto)
		{
			if (!S.T.IsValidIndex(k) || S.T[k].Res != 0) return;
			if (S.DashTarget >= 0) { const int32 Kt = S.DashTarget; S.DashTarget = -1; S.Pos = S.DashTo; Land(G, Kt); } // the last dash lands first
			FTarget& Tg = S.T[k];
			const double Dt = FMath::Abs(TapAt - Tg.Beat);
			Tg.Res = 1;
			Tg.Grade = bAuto ? TEXT("GOOD") : (Dt <= 0.10 ? TEXT("PERFECT") : (Dt <= 0.19 ? TEXT("GOOD") : TEXT("OK")));
			++S.Combo; S.BestCombo = FMath::Max(S.BestCombo, S.Combo);
			S.DashFrom = S.Pos; S.DashTo = TargetPos(Tg); S.DashT0 = Now(); S.DashTarget = k;
			if (Tg.Rival.IsValid()) { const FVector Dir = (S.DashTo - S.DashFrom).GetSafeNormal2D(); S.DashTo -= Dir * (ZoogiConst::ZoogiRadius * 2.2f); }
			ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.7f, 1.1f + 0.01f * FMath::Min(S.Combo, 30));
		}

		void Land(AZoogiGameMode* G, int32 k)
		{
			UWorld* W = G->GetWorld();
			FTarget& Tg = S.T[k];
			const FVector At = S.DashTo;
			S.Landed.Add(At); if (S.Landed.Num() > 7) S.Landed.RemoveAt(0);
			if (AStaticMeshActor* O = Tg.Orb.Get()) O->Destroy();
			const bool bPerfect = Tg.Grade == TEXT("PERFECT");
			const int32 Base = bPerfect ? 3 : (Tg.Grade == TEXT("GOOD") ? 2 : 1); // b183: ~250 for a great run (was ~950)
			int32 P = FMath::RoundToInt(Base * (1.f + S.Combo / 40.f));
			if (Tg.Rival.IsValid()) { P += 8; ++S.RivalHits; AZoogiFX::Spawn(W, TargetPos(Tg) + FVector(0, 0, 60.f), Mag, 260.f, 0.45f, true, 10.f); }
			S.Pts += P;
			if (AZoogiBall* Me = S.Me.Get()) Me->Score += P;
			AZoogiFX::Spawn(W, At + FVector(0, 0, 8.f), bPerfect ? Gold : Cyan, S.TileR * 1.4f, 0.4f, true, 9.f);
			if (S.Combo % 8 == 0) AZoogiFX::Stylized(W, TEXT("Mid_Explosion_v2"), At, 0.6f);
			ZoogiUtil::PlaySfx(G, Tg.Rival.IsValid() ? TEXT("HitCrush") : TEXT("RockSmash"), 0.65f, 1.0f + 0.15f * bPerfect);
			ZoogiHaptics::Kick(bPerfect ? 0.55f : 0.35f, W, TEXT("showtime_hit"));
			// real orbs near the landing scatter
			for (AZoogiOrb* O : G->Orbs)
				if (O && !O->bOut && O->Sphere && O->Sphere->IsSimulatingPhysics() && FVector::Dist2D(O->OrbLocation(), At) < S.TileR * 1.3f)
				{ O->Sphere->WakeAllRigidBodies(); O->Sphere->SetPhysicsLinearVelocity((O->OrbLocation() - At).GetSafeNormal2D() * 650.f + FVector(0, 0, 250.f)); }
			Pop(At, Tg.Grade, bPerfect ? FLinearColor::White : (Tg.Grade == TEXT("GOOD") ? Gold : Cyan));
			Pop(At + FVector(0, 0, 120.f), FString::Printf(TEXT("+%d"), P), Gold);
		}

		void Miss(AZoogiGameMode* G, int32 k)
		{
			FTarget& Tg = S.T[k];
			Tg.Res = 2;
			if (AStaticMeshActor* O = Tg.Orb.Get()) O->Destroy();
			if (S.Shield > 0) { --S.Shield; Tg.Grade = TEXT("SHIELD"); Pop(TargetPos(Tg), TEXT("SHIELD"), Cyan); }
			else { Tg.Grade = TEXT("MISS"); S.Combo = 0; Pop(TargetPos(Tg), TEXT("MISS"), FLinearColor(1.f, 0.35f, 0.3f)); }
		}

		void Finale(AZoogiGameMode* G)
		{
			S.bFinale = true;
			UWorld* W = G->GetWorld();
			AZoogiBall* Me = S.Me.Get();
			const float K = FMath::Clamp(S.BestCombo / 24.f, 0.f, 1.f);
			int32 N = 0;
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B == Me || B->bOut || !B->Sphere || (Me && G->IsAlly(B, Me))) continue;
				FVector Dir = (B->BallLocation() - S.C).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
				if (!B->Sphere->IsSimulatingPhysics()) continue;
				B->Sphere->WakeAllRigidBodies();
				B->Sphere->SetPhysicsLinearVelocity(Dir * FMath::Lerp(600.f, 2400.f, K) + FVector(0, 0, 320.f + 200.f * K));
				if (Me) B->LastHitPlayer = Me->PlayerIndex;
				++N;
			}
			const int32 Bonus = S.BestCombo * 2;
			S.Pts += Bonus; if (Me) Me->Score += Bonus;
			AZoogiFX::BigBlastShake(W, 1.f + K);
			AZoogiFX::Stylized(W, TEXT("Big_Explosion_v2"), S.C + FVector(0, 0, 40.f), 1.0f + 0.6f * K);
			AZoogiFX::Spawn(W, S.C + FVector(0, 0, 10.f), Gold, S.R * 1.1f, 0.7f, true, 12.f);
			AZoogiFX::SpawnConfetti(W, S.C + FVector(0, 0, 200.f), Gold, 60);
			ZoogiUtil::PlaySfx(G, TEXT("EpicBig"), 1.f, 0.85f); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.8f);
			ZoogiHaptics::Kick(1.f, W, TEXT("showtime_showdown"));
			ZoogiShow::Event(W, ZoogiShow::EEvent::Win, S.C, 3.f);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("SHOWDOWN!"), FString::Printf(TEXT("%d BEAT COMBO   +%d"), S.BestCombo, S.Pts), Gold, 1.8f, 1.4f);
			UE_LOG(LogZoogi, Log, TEXT("GATTLING SHOWDOWN P%d: best combo %d, %d rivals hit, +%d pts, %d rivals blasted (power %.2f)"), S.Player + 1, S.BestCombo, S.RivalHits, S.Pts, N, K);
			ZoogiRaid::ShowtimeHit(G, S.Player, K); // b207 BOSS ATTACK: the combo grade hits the raid boss
			if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("gattling_showdown_a"), 0.1f); G->TakeShotDelayed(TEXT("gattling_showdown_b"), 0.6f); }
		}

		void End(AZoogiGameMode* G)
		{
			UWorld* W = G ? G->GetWorld() : nullptr;
			if (AZoogiBall* Me = S.Me.Get())
				if (Me->Sphere)
				{
					Me->Sphere->SetWorldLocation(FVector(S.Pos.X, S.Pos.Y, S.FloorZ + ZoogiConst::ZoogiRadius + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
					Me->Sphere->SetSimulatePhysics(true); Me->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); Me->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				}
			for (FTarget& Tg : S.T) if (AStaticMeshActor* O = Tg.Orb.Get()) O->Destroy();
			if (W) if (APlayerController* PC = W->GetFirstPlayerController())
				if (S.Cam.IsValid() && PC->GetViewTarget() == S.Cam.Get()) PC->SetViewTargetWithBlend(S.PrevView.IsValid() ? S.PrevView.Get() : PC->GetPawn(), 0.45f, VTBlend_Cubic);
			if (ACameraActor* Cam = S.Cam.Get()) Cam->SetLifeSpan(1.f);
			S.Cam.Reset(); S.CamDist = 0.f;
			if (S.Player >= 0 && S.Player < 4) S.Meter[S.Player] = 0.f;
			S.bActive = false; S.bFinale = false;
			if (G) G->PhaseTime = 0.f; // a fresh aim after the show
			UE_LOG(LogZoogi, Log, TEXT("GATTLING SHOWTIME over - the match resumes"));
		}
	}

	bool Active() { return S.bActive; }
	float Meter(int32 P) { return (P >= 0 && P < 4) ? S.Meter[P] : 0.f; }
	void Reset() { const TWeakObjectPtr<AZoogiGameMode> Gw = S.G; S = FState(); S.G = Gw; }

	void OnContact(AZoogiGameMode* G, AZoogiBall* Shooter, AActor* Other)
	{
		if (Off() || !G || !Shooter || !Other || Shooter->bCPU || S.bActive || !ModeOk(G) || G->Phase != EZoogiPhase::Rolling) return;
		if (Shooter->PlayerIndex < 0 || Shooter->PlayerIndex > 3 || G->ActivePlayer != Shooter->PlayerIndex) return;
		if (G->TurnCounter != S.Turn) { S.Turn = G->TurnCounter; S.TurnHits = 0; S.Touched.Reset(); }
		if (S.Touched.Contains(Other)) return;
		const bool bOrb = Cast<AZoogiOrb>(Other) != nullptr;
		AZoogiBall* Rv = Cast<AZoogiBall>(Other);
		if (!bOrb && !(Rv && !G->IsAlly(Rv, Shooter))) return;
		S.Touched.Add(Other);
		if (!Leading(G, Shooter) && !TestOn()) return; // only the leader builds SHOWTIME
		++S.TurnHits;
		const float Add = (bOrb ? 0.08f : 0.15f) * (1.f + 0.25f * (S.TurnHits - 1)) * FillMul();
		float& M = S.Meter[Shooter->PlayerIndex];
		const bool bWasFull = M >= 0.999f;
		M = FMath::Min(1.f, M + Add);
		if (!bWasFull && M >= 0.999f)
		{
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("P%d %s: SHOWTIME READY"), Shooter->PlayerIndex + 1, *Shooter->Def.Name.ToUpper()), ButtonMode() ? TEXT("YOUR SHOW - TAP SHOWTIME IN YOUR MENU") : TEXT("IT STARTS ON YOUR NEXT AIM"), Gold, 1.5f, 1.1f);
			ZoogiUtil::PlaySfx(G, TEXT("Score"), 1.f, 1.3f);
		}
		UE_LOG(LogZoogi, Log, TEXT("GATTLING meter P%d +%.2f (%s, hit %d this turn) = %.2f"), Shooter->PlayerIndex + 1, Add, bOrb ? TEXT("orb") : TEXT("rival"), S.TurnHits, M);
	}

	bool Ready(AZoogiGameMode* G) { return CanStart(G, false); }
	bool StartFromMenu(AZoogiGameMode* G, bool bMidRoll)
	{
		GMidRollOk = bMidRoll;
		const bool bCan = CanStart(G, false);
		if (bCan) Begin(G, false);
		GMidRollOk = false;
		return bCan && S.bActive;
	}
	bool TryButton(AZoogiGameMode* G, const FVector2D& Ptr)
	{
		if (!CanStart(G, false) || !S.Button.bIsValid || !S.Button.IsInside(Ptr)) return false;
		Begin(G, false);
		return S.bActive;
	}

	void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease)
	{
		if (!S.bActive || S.bFinale) return;
		S.bHold = bDown;
		if (!bPress) return;
		// assisted: a tap anywhere resolves the open beat nearest in time (within half a beat)
		const double Tn = Now();
		int32 Kq = S.Next; while (Kq < S.T.Num() && S.T[Kq].Res != 0) ++Kq;
		if (Kq < S.T.Num() && FMath::Abs(Tn - S.T[Kq].Beat) <= S.BeatLen * 0.5) Resolve(G, Kq, Tn, false);
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || Off()) return;
		if (S.G.Get() != G || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming) { if (S.bActive) End(G); const bool bT = S.bTestDone; Reset(); S.G = G; S.bTestDone = bT && S.G.Get() == G; if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming) return; }
		// test: fill + start on the first human aim (round 1+), and tap along automatically
		if (TestOn() && !S.bTestDone && !S.bActive && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 1.5f)
			if (AZoogiBall* Me = G->GetActiveBall()) if (!Me->bCPU) { S.bTestDone = true; S.Meter[FMath::Clamp(Me->PlayerIndex, 0, 3)] = 1.f; Begin(G, true); }
		// b187: auto start - a full meter fires the show on the human's aim (a beat after the aim opens)
		if (!S.bActive && AutoMode() && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 0.6f && CanStart(G, false))
		{	UE_LOG(LogZoogi, Log, TEXT("GATTLING auto start (meter full)")); Begin(G, false); }
		if (!S.bActive) return;
		G->PhaseTime = 0.f; // the match is paused: no aim clock, no auto-shot
		const double Tn = Now();
		if (Tn - S.T0 > 32.0) { UE_LOG(LogZoogi, Warning, TEXT("GATTLING safety: 32 s - the show ends")); if (!S.bFinale) Finale(G); End(G); return; } // b186: never stuck
		UWorld* W = G->GetWorld();
		if (TestOn())
		{
			int32 Kq = S.Next; while (Kq < S.T.Num() && S.T[Kq].Res != 0) ++Kq;
			if (Kq < S.T.Num())
			{
				const double Jit = (Kq % 7 == 3) ? 0.14 : ((Kq % 11 == 6) ? 1.0 : 0.03); // mostly perfect, a few good, one miss (shield)
				if (Jit < 0.5 && Tn >= S.T[Kq].Beat + Jit) Resolve(G, Kq, Tn, false);
			}
		}
		// hold = auto-fire on the beat
		if (S.bHold)
		{ int32 Kq = S.Next; while (Kq < S.T.Num() && S.T[Kq].Res != 0) ++Kq; if (Kq < S.T.Num() && Tn >= S.T[Kq].Beat + 0.03) Resolve(G, Kq, Tn, true); }
		// misses
		for (int32 k = S.Next; k < S.T.Num(); ++k) { if (S.T[k].Res == 0 && Tn > S.T[k].Beat + S.BeatLen * 0.5) Miss(G, k); else break; }
		while (S.Next < S.T.Num() && S.T[S.Next].Res != 0 && !(S.DashTarget == S.Next && Tn < S.DashT0 + DashSec)) ++S.Next;
		// orbs gush from the generator one beat ahead and arc onto their tile
		for (int32 k = 0; k < S.T.Num(); ++k)
		{
			FTarget& Tg = S.T[k];
			if (Tg.Res != 0 || Tg.Rival.IsValid()) continue;
			const double Ls = Tg.Beat - 2.0 * S.BeatLen;
			if (Tn < Ls) continue;
			if (!Tg.Orb.IsValid()) Tg.Orb = SpawnOrb(W, S.C + FVector(0, 0, 120.f), (k % 4 == 3) ? Gold : FLinearColor(0.3f, 0.6f, 1.f));
			if (AStaticMeshActor* O = Tg.Orb.Get())
			{
				const float Kf = FMath::Clamp((float)((Tn - Ls) / S.BeatLen), 0.f, 1.f);
				const FVector To = S.Tiles[Tg.Tile] + FVector(0, 0, 55.f);
				const FVector P = FMath::Lerp(S.C + FVector(0, 0, 120.f), To, Kf) + FVector(0, 0, 420.f * FMath::Sin(PI * Kf));
				O->SetActorLocation(P + FVector(0, 0, Kf >= 1.f ? 10.f * FMath::Sin((float)Tn * 12.f) : 0.f));
			}
		}
		// the dash
		if (S.DashTarget >= 0)
		{
			const float Kd = FMath::Clamp((float)((Tn - S.DashT0) / DashSec), 0.f, 1.f);
			S.Pos = FMath::Lerp(S.DashFrom, S.DashTo, 1.f - FMath::Square(1.f - Kd));
			if (Kd >= 1.f) { const int32 Kt = S.DashTarget; S.DashTarget = -1; S.Pos = S.DashTo; Land(G, Kt); }
		}
		if (AZoogiBall* Me = S.Me.Get())
			if (Me->Sphere)
			{
				const float Hop = S.DashTarget >= 0 ? FMath::Sin(PI * FMath::Clamp((float)((Tn - S.DashT0) / DashSec), 0.f, 1.f)) : 0.f;
				Me->Sphere->SetWorldLocation(FVector(S.Pos.X, S.Pos.Y, S.FloorZ + ZoogiConst::ZoogiRadius + 2.f + 110.f * Hop), false, nullptr, ETeleportType::TeleportPhysics);
			}
		// b183: the camera holds on the floor for the whole show (it drifts a little so the frozen-view guard leaves it alone)
		if (ACameraActor* Cam = S.Cam.Get())
		{
			const float Age = (float)(Tn - S.T0);
			if (S.CamDist <= 0.f) { S.CamDist = FVector::Dist(Cam->GetActorLocation(), S.C); S.CamYaw = Cam->GetActorRotation().Yaw; }
			FRotator Rot((S.bPortrait ? -86.f : -68.f) + 2.f * FMath::Sin(Age * 0.7f), S.CamYaw + 6.f * FMath::Sin(Age * 0.35f), 0.f);
			float Dist = S.CamDist * (1.f - 0.04f * FMath::Min(Age / 10.f, 1.f));
			FVector Look = S.C;
			if (S.bFinale)
			{	// b183 (Evan: "with the big explosion at the end the camera tilts up and views the explosion for a moment"): the camera drops
				// in and tilts up to watch the blast climb over the court, holds, then the show ends and the normal camera comes back
				const float Kf = FMath::Clamp((float)((Tn - S.FinaleAt) / 0.7), 0.f, 1.f), E = 1.f - FMath::Pow(1.f - Kf, 3.f);
				Rot.Pitch = FMath::Lerp(Rot.Pitch, -14.f, E);
				Dist = FMath::Lerp(Dist, S.CamDist * 0.62f, E);
				Look = S.C + FVector(0.f, 0.f, 520.f * E);
			}
			Cam->SetActorLocationAndRotation(Look - Rot.Vector() * Dist, Rot);
			if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetViewTarget() != Cam) PC->SetViewTargetWithBlend(Cam, 0.15f);
		}
		if (!S.bFinale && Tn >= S.FinaleAt) Finale(G);
		if (S.bFinale && S.Climb < 2 && Tn >= S.FinaleAt + 0.35 + 0.35 * S.Climb)
		{
			const float K = FMath::Clamp(S.BestCombo / 24.f, 0.f, 1.f);
			const FVector Up = S.C + FVector(0.f, 0.f, 300.f + 320.f * S.Climb);
			AZoogiFX::Stylized(W, TEXT("Big_Explosion_v2"), Up, 1.1f + 0.5f * K - 0.2f * S.Climb);
			AZoogiFX::Spawn(W, Up, Gold, 500.f + 300.f * K, 0.6f, false, 10.f);
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 0.9f - 0.1f * S.Climb);
			ZoogiHaptics::Kick(0.7f, W, TEXT("showtime_climb"));
			if (S.Climb == 1 && !G->ShotDir.IsEmpty()) G->TakeShotDelayed(TEXT("gattling_tiltup"), 0.15f);
			++S.Climb;
		}
		if (Tn >= S.EndAt) End(G);
		for (int32 i = S.Pops.Num() - 1; i >= 0; --i) if (Tn - S.Pops[i].T0 > 0.7) S.Pops.RemoveAt(i);
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!G || !C || !PC || Off()) return;
		const float U = FMath::Min(C->ClipX, C->ClipY) / 1080.f;
		const float Wd = C->ClipX, Ht = C->ClipY;
		const double Tn = Now();
		auto Rect = [&](float X, float Y, float W0, float H0, const FLinearColor& Col) { FCanvasTileItem T(FVector2D(X, Y), FVector2D(W0, H0), Col); T.BlendMode = SE_BLEND_Translucent; C->DrawItem(T); };
		if (!S.bActive)
		{	// the meter (top centre) for the human, and the button when it's ready
			S.Button = FBox2D(ForceInit);
			if (ZoogiRaid::NewHud() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeCanvasBtn"))) return; // b207: the raid boss plate shows the meter
			AZoogiBall* Hu = Human(G);
			if (!Hu || !ModeOk(G) || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming || G->Phase == EZoogiPhase::GameOver) return;
			const float M = Meter(Hu->PlayerIndex);
			const bool bLead = Leading(G, Hu) || M >= 0.999f; // b187: the bar is always up in a match
			const float BW = 340.f * U, BH = 22.f * U, X0 = Wd * 0.5f - BW * 0.5f, Y0 = Ht * 0.085f;
			Rect(X0 - 4.f * U, Y0 - 4.f * U, BW + 8.f * U, BH + 8.f * U, Ink);
			const float Pulse = M >= 0.999f ? 0.75f + 0.25f * FMath::Sin((float)Tn * 8.f) : 1.f;
			if (!bLead) Rect(X0, Y0, BW, BH, FLinearColor(0.25f, 0.25f, 0.28f, 1.f));
			Rect(X0, Y0, BW * M, BH, bLead ? FLinearColor(Gold.R * Pulse, Gold.G * Pulse, Gold.B * Pulse, 1.f) : FLinearColor(0.55f, 0.52f, 0.4f, 1.f));
			ZoogiFunCanvasText(C, M >= 0.999f ? TEXT("SHOWTIME READY") : (bLead ? TEXT("SHOWTIME") : TEXT("SHOWTIME - TAKE THE LEAD")), Wd * 0.5f, Y0 + BH * 0.5f, 18.f * U, FLinearColor::White, 1, 0);
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeCanvasBtn")) && CanStart(G, false)) // b194: the button lives in the P5 menu now
			{
				const float Bw = 320.f * U, Bh = 86.f * U, Cx = Wd * 0.5f, Cy = Y0 + BH + 70.f * U;
				const float Sc = 1.f + 0.05f * FMath::Sin((float)Tn * 7.f);
				Rect(Cx - Bw * 0.5f * Sc + 9.f * U, Cy - Bh * 0.5f * Sc + 9.f * U, Bw * Sc, Bh * Sc, FLinearColor(Mag.R, Mag.G, Mag.B, 1.f));
				Rect(Cx - Bw * 0.5f * Sc - 5.f * U, Cy - Bh * 0.5f * Sc - 5.f * U, Bw * Sc + 10.f * U, Bh * Sc + 10.f * U, FLinearColor::White);
				Rect(Cx - Bw * 0.5f * Sc, Cy - Bh * 0.5f * Sc, Bw * Sc, Bh * Sc, FLinearColor(Gold.R, Gold.G, Gold.B, 1.f));
				ZoogiFunCanvasText(C, TEXT("SHOWTIME!"), Cx, Cy, 48.f * U * Sc, Ink, 1, 0);
				S.Button = FBox2D(FVector2D(Cx - Bw * 0.6f, Cy - Bh * 0.7f), FVector2D(Cx + Bw * 0.6f, Cy + Bh * 0.7f));
			}
			return;
		}
		// ---- the show
		int32 Kb = 0; float Fl = 0.f;
		for (int32 k = S.T.Num() - 1; k >= 0; --k) if (Tn >= S.T[k].Beat) { Kb = k; Fl = FMath::Exp(-(float)(Tn - S.T[k].Beat) * 9.f); break; }
		if (Tn < S.T[0].Beat) Fl = 0.3f;
		// screen-edge lights flash to the beat
		const FLinearColor Lc = (Kb % 3 == 0) ? Gold : ((Kb % 3 == 1) ? Mag : Cyan);
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = 0.16f * Fl * (1.f - i / 5.f), Th = 26.f * U * (i + 1);
			Rect(0, 0, Wd, Th, FLinearColor(Lc.R, Lc.G, Lc.B, A)); Rect(0, Ht - Th, Wd, Th, FLinearColor(Lc.R, Lc.G, Lc.B, A));
			Rect(0, 0, Th, Ht, FLinearColor(Lc.R, Lc.G, Lc.B, A)); Rect(Wd - Th, 0, Th, Ht, FLinearColor(Lc.R, Lc.G, Lc.B, A));
		}
		auto Proj = [&](const FVector& P, FVector2D& Out) { return PC->ProjectWorldLocationToScreen(P, Out, true); };
		// rim ring
		{
			FVector2D Prev; bool bHave = false;
			for (int32 k = 0; k <= 64; ++k)
			{
				const float A = 2.f * PI * k / 64.f; FVector2D Sp;
				if (Proj(S.C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (S.R + 30.f) + FVector(0, 0, 6.f), Sp))
				{ if (bHave) { FCanvasLineItem L(Prev, Sp); L.LineThickness = (6.f + 10.f * Fl) * U; L.SetColor(FLinearColor(Lc.R, Lc.G, Lc.B, 0.5f + 0.5f * Fl)); C->DrawItem(L); } Prev = Sp; bHave = true; }
				else bHave = false;
			}
		}
		// hex tiles
		const int32 NextT = S.Next < S.T.Num() ? S.T[S.Next].Tile : -1, NextT2 = S.Next + 1 < S.T.Num() ? S.T[S.Next + 1].Tile : -1;
		const bool bNextRival = S.Next < S.T.Num() && S.T[S.Next].Rival.IsValid();
		TArray<FCanvasUVTri> Tris;
		for (int32 i = 0; i < S.Tiles.Num(); ++i)
		{
			FLinearColor Cl = Fret[(i + Kb) % 4]; float A = ((i + Kb) % 2 == 0) ? 0.25f + 0.45f * Fl : 0.18f;
			if (i == NextT && !bNextRival) { Cl = FLinearColor::White; A = 0.85f; }
			else if (i == NextT2) { Cl = FLinearColor::White; A = 0.38f; }
			Cl.A = A;
			FVector2D Cs; if (!Proj(S.Tiles[i] + FVector(0, 0, 4.f), Cs)) continue;
			FVector2D V[6]; bool bOk = true;
			for (int32 j = 0; j < 6; ++j) { const float Aj = PI / 6.f + j * PI / 3.f; if (!Proj(S.Tiles[i] + FVector(FMath::Cos(Aj), FMath::Sin(Aj), 0.f) * S.TileR + FVector(0, 0, 4.f), V[j])) bOk = false; }
			if (!bOk) continue;
			for (int32 j = 0; j < 6; ++j)
			{
				FCanvasUVTri T; T.V0_Pos = Cs; T.V1_Pos = V[j]; T.V2_Pos = V[(j + 1) % 6];
				T.V0_UV = T.V1_UV = T.V2_UV = FVector2D(0.5f, 0.5f); T.V0_Color = T.V1_Color = T.V2_Color = Cl; Tris.Add(T);
			}
		}
		if (Tris.Num()) { FCanvasTriangleItem It(Tris, C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr); It.BlendMode = SE_BLEND_Translucent; C->DrawItem(It); }
		// closing ring on the next target (one beat ahead)
		if (S.Next < S.T.Num())
		{
			const FTarget& Tg = S.T[S.Next];
			const float Kk = FMath::Clamp(1.f - (float)((Tg.Beat - Tn) / S.BeatLen), 0.f, 1.f);
			const FVector P = TargetPos(Tg);
			FVector2D Prev; bool bHave = false; const float Rr = S.TileR * (0.55f + 1.1f * (1.f - Kk));
			for (int32 k = 0; k <= 40; ++k)
			{
				const float A = 2.f * PI * k / 40.f; FVector2D Sp;
				if (Proj(P + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Rr + FVector(0, 0, 10.f), Sp))
				{ if (bHave) { FCanvasLineItem L(Prev, Sp); L.LineThickness = 6.f * U; L.SetColor(Tg.Rival.IsValid() ? Mag : FLinearColor(1.f, 1.f, 1.f, 0.4f + 0.6f * Kk)); C->DrawItem(L); } Prev = Sp; bHave = true; }
				else bHave = false;
			}
		}
		// lightning chain through the last landings
		for (int32 i = 0; i + 1 < S.Landed.Num(); ++i)
		{
			FVector2D A0, B0; if (!Proj(S.Landed[i] + FVector(0, 0, 30.f), A0) || !Proj(S.Landed[i + 1] + FVector(0, 0, 30.f), B0)) continue;
			FRandomStream Rr(i * 31 + (int32)(Tn * 20.0));
			FVector2D Pp = A0;
			for (int32 s = 1; s <= 6; ++s)
			{
				FVector2D Q = FMath::Lerp(A0, B0, s / 6.f); if (s < 6) Q += FVector2D(Rr.FRandRange(-10.f, 10.f), Rr.FRandRange(-10.f, 10.f)) * U;
				FCanvasLineItem L(Pp, Q); L.LineThickness = 5.f * U; L.SetColor(FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.35f + 0.65f * (i + 1) / S.Landed.Num())); C->DrawItem(L); Pp = Q;
			}
		}
		// popups
		for (const FPop& Pp : S.Pops)
		{
			FVector2D Sp; if (!Proj(Pp.At + FVector(0, 0, 80.f), Sp)) continue;
			const float K = (float)((Tn - Pp.T0) / 0.7);
			FLinearColor Cl = Pp.Col; Cl.A = 1.f - K * K * K;
			ZoogiFunCanvasText(C, Pp.Text + (Pp.Text.StartsWith(TEXT("+")) ? TEXT("") : TEXT("!")), Sp.X, Sp.Y - 60.f * U * K, (Pp.Text.StartsWith(TEXT("+")) ? 30.f : 44.f) * U, Cl, 1, 0);
		}
		// HUD: combo (top right), chain (bottom left), beats left (top bar)
		if (!S.bFinale)
		{
			ZoogiFunCanvasText(C, FString::Printf(TEXT("x%d"), S.Combo), Wd - 140.f * U, 130.f * U, 84.f * U, Gold, 1, 0);
			ZoogiFunCanvasText(C, TEXT("COMBO"), Wd - 140.f * U, 190.f * U, 28.f * U, FLinearColor::White, 1, 0);
			ZoogiFunCanvasText(C, FString::Printf(TEXT("CHAIN x%d"), S.Next), 170.f * U, Ht - 70.f * U, 44.f * U, Cyan, 1, 0);
			const float Left = FMath::Clamp((float)((S.FinaleAt - Tn) / FMath::Max(0.1, S.FinaleAt - S.T0)), 0.f, 1.f);
			const float BW = 340.f * U, BH = 20.f * U, X0 = Wd * 0.5f - BW * 0.5f, Y0 = Ht * 0.06f;
			Rect(X0 - 4.f * U, Y0 - 4.f * U, BW + 8.f * U, BH + 8.f * U, Ink);
			Rect(X0, Y0, BW * Left, BH, FLinearColor(Gold.R, Gold.G, Gold.B, 1.f));
			ZoogiFunCanvasText(C, TEXT("SHOWTIME"), Wd * 0.5f, Y0 + BH * 0.5f, 18.f * U, FLinearColor::White, 1, 0);
			if (S.Next < 3) ZoogiFunCanvasText(C, TEXT("TAP ANYWHERE ON THE BEAT  -  HOLD TO AUTO-FIRE"), Wd * 0.5f, Ht - 60.f * U, 30.f * U, FLinearColor::White, 1, 0);
		}
	}
}
