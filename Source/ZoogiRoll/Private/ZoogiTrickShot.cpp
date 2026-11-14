// b156: DAILY TRICK SHOT (see ZoogiTrickShot.h)
#include "ZoogiTrickShot.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiActionFun.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiProgressionSubsystem.h"
#include "ZoogiProgression.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiAnimeCues.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace ZoogiTrickShot
{
	namespace
	{
		const TCHAR* Sec = TEXT("ZoogiRoll.TrickShot");
		constexpr int32 Tries = 3;
		const FLinearColor Pink(0.85f, 0.06f, 0.42f), Gold(1.f, 0.78f, 0.1f);
		struct FState
		{
			bool bOn = false, bPlaced = false, bWaiting = false, bDone = false, bLayout = false, bResult = false;
			TWeakObjectPtr<AZoogiGameMode> G;
			int32 Day = 0, Try = 0, Best = 0, LastStars = -1, SavedRounds = 0, EvalTurn = -1;
			float WaitT = 0.f;
			FVector Me = FVector::ZeroVector; float MeYaw = 0.f;
			TArray<FVector> Rivals;
			int32 GainCoins = 0, GainGems = 0, GainXP = 0, NewLevel = 0, StreakNow = 0;
		};
		FState S;

		int32 DayIndex()
		{	// a new shot every day at 4 am local
			int32 Forced = -1;
			if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiTrickDay="), Forced) && Forced >= 0) return Forced;
			const FDateTime Now = FDateTime::Now() - FTimespan::FromHours(4.0);
			return (int32)(Now.GetDate().GetTicks() / ETimespan::TicksPerDay);
		}
		int32 FirstDay() { return (int32)(FDateTime(2026, 11, 6).GetTicks() / ETimespan::TicksPerDay); }
		int32 GetI(const TCHAR* K, int32 Def) { int32 V = Def; if (GConfig) GConfig->GetInt(Sec, K, V, GGameUserSettingsIni); return V; }
		void SetI(const TCHAR* K, int32 V) { if (GConfig) GConfig->SetInt(Sec, K, V, GGameUserSettingsIni); }
		EZoogiArena ArenaFor(int32 Day)
		{	// classic courts with a field start (no Cosmic launch pad, no Frozen roam bridges)
			static const EZoogiArena A[] = { EZoogiArena::Ancient, EZoogiArena::Volcanic, // b157 (Evan): Ancient Meadows replaces the Meadow
			  EZoogiArena::Arabian, EZoogiArena::Tomb, EZoogiArena::Neon };
			return A[((Day % 5) + 5) % 5];
		}
		int32 Human(const AZoogiGameMode* G) { for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i] && !G->Balls[i]->bCPU) return i; return 0; }

		bool Floor(AZoogiGameMode* G, const FVector2D& P, float Zc, FVector& Out)
		{
			UWorld* W = G->GetWorld(); if (!W) return false;
			FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
			FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiTrickFloor), false);
			for (AZoogiBall* B : G->Balls) if (B) QP.AddIgnoredActor(B);
			for (AZoogiOrb* O : G->Orbs) if (O) QP.AddIgnoredActor(O);
			FHitResult H;
			if (!W->LineTraceSingleByObjectType(H, FVector(P.X, P.Y, Zc + 1200.f), FVector(P.X, P.Y, Zc - 1500.f), OQ, QP)) return false;
			if (H.ImpactNormal.Z < 0.8f || FMath::Abs(H.ImpactPoint.Z - Zc) > 400.f) return false; // a prop top / a deep hole
			const FVector C(P.X, P.Y, H.ImpactPoint.Z + ZoogiConst::ZoogiRadius + 2.f);
			FCollisionQueryParams Q2(SCENE_QUERY_STAT(ZoogiTrickRoom), false);
			for (AZoogiBall* B : G->Balls) if (B) Q2.AddIgnoredActor(B);
			for (AZoogiOrb* O : G->Orbs) if (O) Q2.AddIgnoredActor(O);
			if (W->OverlapAnyTestByObjectType(C + FVector(0.f, 0.f, 6.f), FQuat::Identity, OQ, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius), Q2)) return false; // inside a prop
			Out = C; return true;
		}

		void MakeLayout(AZoogiGameMode* G)
		{	// one layout per day: the same for every try (and every player)
			S.Rivals.Reset();
			const FVector C = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			const float Zc = G->Balls.Num() > 0 && G->Balls[0] ? G->Balls[0]->SpawnLocation.Z - ZoogiConst::ZoogiRadius : C.Z;
			const float Rk = G->Arena ? G->Arena->GetKnockoffRadius() : 1500.f;
			FRandomStream Rs(S.Day * 7919 + 1301);
			const int32 H = Human(G);
			auto OnCourt = [&](const FVector2D& P) { return !G->Arena || !G->Arena->IsOffCourt(P); };
			// you: a third of the way out
			const float Ah = Rs.FRandRange(0.f, 360.f);
			bool bMe = false;
			for (int32 t = 0; t < 40 && !bMe; ++t)
			{
				const float A = Ah + Rs.FRandRange(-25.f, 25.f) * (t > 0 ? 1.f : 0.f), R = Rk * Rs.FRandRange(0.22f, 0.4f);
				const FVector2D P = FVector2D(C) + FVector2D(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A))) * R;
				bMe = OnCourt(P) && Floor(G, P, Zc, S.Me);
			}
			if (!bMe && G->Balls.IsValidIndex(H) && G->Balls[H]) S.Me = G->Balls[H]->SpawnLocation;
			// rivals: a cluster across the court, near the edge (a clean shot can take all three)
			const float Ac = Ah + 180.f + Rs.FRandRange(-30.f, 30.f);
			for (int32 k = 0; k < 3; ++k)
			{
				FVector Got = FVector::ZeroVector; bool bOk = false;
				for (int32 t = 0; t < 60 && !bOk; ++t)
				{
					const float A = Ac + Rs.FRandRange(-26.f, 26.f), R = Rk * Rs.FRandRange(0.5f, 0.8f);
					const FVector2D P = FVector2D(C) + FVector2D(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A))) * R;
					if (!OnCourt(P) || FVector2D::Distance(P, FVector2D(S.Me)) < 600.f) continue;
					bool bSpace = true; for (const FVector& O : S.Rivals) if (FVector2D::Distance(P, FVector2D(O)) < ZoogiConst::ZoogiRadius * 2.8f) bSpace = false;
					if (bSpace && Floor(G, P, Zc, Got)) bOk = true;
				}
				if (!bOk) { int32 n = 0; for (int32 i = 0; i < G->Balls.Num(); ++i) if (i != H && G->Balls[i] && n++ == k) Got = G->Balls[i]->SpawnLocation; }
				S.Rivals.Add(Got);
			}
			FVector Mid = FVector::ZeroVector; for (const FVector& R : S.Rivals) Mid += R; Mid /= FMath::Max(1, S.Rivals.Num());
			S.MeYaw = (Mid - S.Me).Rotation().Yaw;
			S.bLayout = true;
			UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT #%d layout on %s: you at (%.0f, %.0f), rivals %d, court r%.0f"), TodayNumber(), *ZoogiUtil::ArenaName(G->MenuArena), S.Me.X, S.Me.Y, S.Rivals.Num(), Rk);
		}

		void PlaceAll(AZoogiGameMode* G)
		{
			if (!S.bLayout) MakeLayout(G);
			const int32 H = Human(G);
			int32 k = 0;
			for (int32 i = 0; i < G->Balls.Num(); ++i)
			{
				AZoogiBall* B = G->Balls[i]; if (!B) continue;
				if (i == H) { B->PlaceAt(S.Me, S.MeYaw); continue; }
				if (k < S.Rivals.Num()) B->PlaceAt(S.Rivals[k], (S.Me - S.Rivals[k]).Rotation().Yaw);
				B->StunTurns = 0;
				++k;
			}
			for (AZoogiBall* B : G->Balls) if (B) B->Score = 0;
			S.bPlaced = true;
		}

		void Award(AZoogiGameMode* G)
		{
			const int32 Prev = GetI(*FString::Printf(TEXT("Best_%d"), S.Day), 0);
			const int32 New = FMath::Max(0, S.Best - Prev);
			S.GainCoins = 20 * New; S.GainXP = 20 * New; S.GainGems = (S.Best >= 3 && Prev < 3) ? 5 : 0;
			// streak: consecutive days played
			const int32 Last = GetI(TEXT("LastDay"), -100);
			int32 Streak0 = GetI(TEXT("Streak"), 0);
			if (Last != S.Day)
			{
				Streak0 = Last == S.Day - 1 ? Streak0 + 1 : 1;
				if (Streak0 % 7 == 0) S.GainGems += 20;
				SetI(TEXT("Streak"), Streak0); SetI(TEXT("LastDay"), S.Day);
			}
			S.StreakNow = Streak0;
			if (S.Best > Prev) SetI(*FString::Printf(TEXT("Best_%d"), S.Day), S.Best);
			if (GConfig) GConfig->Flush(false, GGameUserSettingsIni);
			S.NewLevel = 0;
			if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(G))
			{
				FZoogiFrontProfileData& D = P->Mutable();
				D.Gold += S.GainCoins;
				D.XP += S.GainXP;
				while (D.XP >= UZoogiFrontProfileSubsystem::XPForLevel(D.Level)) { D.XP -= UZoogiFrontProfileSubsystem::XPForLevel(D.Level); ++D.Level; S.NewLevel = D.Level; }
				P->Save();
			}
			if (S.GainGems > 0)
				if (UGameInstance* GI = G->GetGameInstance())
					if (UZoogiProgressionSubsystem* PS = GI->GetSubsystem<UZoogiProgressionSubsystem>())
					{
						PS->Core().GrantGems(FGuid::NewGuid(), EZoogiGemBucket::Earned, S.GainGems, FName(TEXT("trickshot")), FDateTime::UtcNow());
						PS->SaveProfile();
					}
			UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT #%d done: best %d (before %d) -> +%d coins +%d gems +%d XP, streak %d%s"), TodayNumber(), S.Best, Prev, S.GainCoins, S.GainGems, S.GainXP, S.StreakNow,
				S.NewLevel > 0 ? *FString::Printf(TEXT(", LEVEL UP %d"), S.NewLevel) : TEXT(""));
		}

		bool GPropsSwapped = false;
		void SwapProps(AZoogiGameMode* G, bool bTrick)
		{	// b157 (Evan): Ancient Meadows without the central drum ring for the trick shot
			if (!G || !G->Arena) return;
			static UStaticMesh* Orig = nullptr; static UStaticMesh* Trick = nullptr;
			if (!Orig) { Orig = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/AncientV14/SM_Arena_PlazaProps.SM_Arena_PlazaProps"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (Orig) Orig->AddToRoot(); }
			if (!Trick) { Trick = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/AncientV14/SM_Arena_PlazaPropsTrick.SM_Arena_PlazaPropsTrick"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (Trick) Trick->AddToRoot(); }
			if (!Orig || !Trick) { if (bTrick) UE_LOG(LogZoogi, Warning, TEXT("TRICK SHOT: plaza props swap unavailable (orig %d, trick %d)"), Orig ? 1 : 0, Trick ? 1 : 0); GPropsSwapped = bTrick; return; }
			TInlineComponentArray<UStaticMeshComponent*> Cs(G->Arena);
			int32 N = 0;
			for (UStaticMeshComponent* C : Cs)
			{
				if (!C) continue;
				if (bTrick && C->GetStaticMesh() == Orig) { C->SetStaticMesh(Trick); ++N; }
				else if (!bTrick && C->GetStaticMesh() == Trick) { C->SetStaticMesh(Orig); ++N; }
			}
			GPropsSwapped = bTrick;
			if (N) UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT: Ancient plaza props -> %s"), bTrick ? TEXT("no central drums (bank posts stay)") : TEXT("original"));
		}
		void End(AZoogiGameMode* G)
		{
			if (GPropsSwapped) SwapProps(G, false);
			if (G && S.SavedRounds > 0) G->MenuRounds = S.SavedRounds;
			const bool bResult = S.bResult; const int32 Best = S.Best;
			S = FState(); S.bResult = bResult; S.Best = Best;
		}
	}

	bool Active() { return S.bOn; }
	int32 TodayNumber() { return FMath::Max(1, DayIndex() - FirstDay() + 1); }
	EZoogiArena TodayArena() { return ArenaFor(DayIndex()); }
	int32 Streak() { const int32 Last = GetI(TEXT("LastDay"), -100); const int32 D = DayIndex(); return (Last == D || Last == D - 1) ? GetI(TEXT("Streak"), 0) : 0; }
	int32 BestToday() { return GetI(*FString::Printf(TEXT("Best_%d"), DayIndex()), 0); }
	bool PopupDue() { return !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTrickPopup")) && (true || GetI(TEXT("PopupDay"), -1) != DayIndex()); } // b159 (Evan 11:10 "for now ensure pops every time I open the app"): every launch; restore the once-a-day check later
	void MarkPopupShown() { SetI(TEXT("PopupDay"), DayIndex()); if (GConfig) GConfig->Flush(false, GGameUserSettingsIni); }

	void Arm(AZoogiGameMode* G)
	{
		if (!G) return;
		S = FState();
		S.bOn = true; S.G = G; S.Day = DayIndex();
		S.SavedRounds = G->MenuRounds;
		G->MenuRounds = 1;
		UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT #%d armed (day %d) on %s"), TodayNumber(), S.Day, *ZoogiUtil::ArenaName(ArenaFor(S.Day)));
	}

	int32 FixTurn(AZoogiGameMode* G, int32 PlayerIdx)
	{
		if (!S.bOn || !G || G->Balls.Num() == 0) return PlayerIdx;
		G->bLaunchFlow = false; G->LaunchFlow.bOpening = false;
		const int32 H = Human(G);
		if (!S.bPlaced)
		{
			PlaceAll(G);
			++S.Try;
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G))
				F->PublicCallout(FString::Printf(TEXT("TRY %d OF %d"), S.Try, Tries), S.Try == 1 ? FString(TEXT("ONE SHOT - KNOCK ALL 3 RIVALS OFF")) : FString::Printf(TEXT("BEST SO FAR: %d STAR%s"), S.Best, S.Best == 1 ? TEXT("") : TEXT("S")), Pink, 2.2f, 1.2f);
			UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT try %d / %d"), S.Try, Tries);
		}
		return H;
	}

	bool OnEndTurn(AZoogiGameMode* G)
	{
		if (!S.bOn || !G) return false;
		if (S.bWaiting || S.bDone) return true;
		if (G->TurnCounter == S.EvalTurn) return true;
		S.EvalTurn = G->TurnCounter;
		if (S.Try == 0) { S.bWaiting = true; S.WaitT = 0.4f; S.bDone = false; UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT: a turn ended before the layout - placing it now")); return true; } // b157
		const int32 H = Human(G);
		int32 Kos = 0; for (int32 i = 0; i < G->Balls.Num(); ++i) if (i != H && G->Balls[i] && G->Balls[i]->bOut) ++Kos;
		const bool bFell = G->Balls.IsValidIndex(H) && G->Balls[H] && G->Balls[H]->bOut;
		const int32 Stars = bFell ? 0 : FMath::Min(3, Kos);
		S.LastStars = Stars;
		S.Best = FMath::Max(S.Best, Stars);
		if (Stars >= 3) ZoogiAnimeCues::Fire(G->GetWorld(), TEXT("TrickShot3Star"), H); // b164
		UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT try %d: %d KO%s%s -> %d star(s), best %d"), S.Try, Kos, Kos == 1 ? TEXT("") : TEXT("s"), bFell ? TEXT(", YOU FELL") : TEXT(""), Stars, S.Best);
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G))
		{
			const FString Head = bFell ? FString(TEXT("YOU FELL OFF!")) : Stars == 3 ? FString(TEXT("TRICK SHOT!")) : Kos > 0 ? FString::Printf(TEXT("%d KNOCKOUT%s"), Kos, Kos == 1 ? TEXT("") : TEXT("S")) : FString(TEXT("MISSED!"));
			F->PublicCallout(Head, FString::Printf(TEXT("%d STAR%s THIS TRY"), Stars, Stars == 1 ? TEXT("") : TEXT("S")), Stars == 3 ? Gold : Pink, 2.4f, 1.4f);
		}
		S.bWaiting = true; S.WaitT = 2.6f;
		S.bDone = S.Try >= Tries || S.Best >= 3;
		return true;
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		static bool bCmd = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTrickShot"));
		if (bCmd) { bCmd = false; Arm(G); } // test: the command-line match (-ZoogiArena / -ZoogiPlayers=4 / -ZoogiRounds=1) is today's shot
		if (!S.bOn) return;
		if (G->Phase == EZoogiPhase::Menu) { End(G); return; }
		if (!GPropsSwapped && G->Arena && G->Arena->GetTheme() == EZoogiArena::Ancient && !S.bPlaced) SwapProps(G, true); // before the layout is placed
		if (!S.bWaiting) return;
		S.WaitT -= RealDt;
		if (S.WaitT > 0.f) return;
		S.bWaiting = false;
		if (S.bDone)
		{
			Award(G);
			const int32 H = Human(G);
			for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i]) G->Balls[i]->Score = i == H ? S.Best * 100 : 0;
			G->PublicFinishGame();
			return;
		}
		S.bPlaced = false;
		G->PublicBeginTurn(Human(G));
	}

	void Draw(AZoogiGameMode* G, UCanvas* C)
	{
		if (!S.bOn || !G || !C || G->Phase == EZoogiPhase::GameOver) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const float Y = Ht * 0.17f;
		FString Stars; for (int32 i = 0; i < 3; ++i) Stars += i < S.Best ? TEXT("O") : TEXT("-");
		const FString Line = FString::Printf(TEXT("DAILY TRICK SHOT #%d   TRY %d / %d   BEST [%s]"), TodayNumber(), FMath::Max(1, S.Try), Tries, *Stars);
		ZoogiFunCanvasRect(C, Wd * 0.5f - 430.f * U, Y - 26.f * U, 860.f * U, 52.f * U, FLinearColor(0.f, 0.f, 0.f, 0.8f));
		ZoogiFunCanvasRect(C, Wd * 0.5f - 430.f * U, Y + 26.f * U, 860.f * U, 6.f * U, Pink);
		ZoogiFunCanvasText(C, Line, Wd * 0.5f, Y, 30.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, TEXT("1 STAR = 1 KO   2 STARS = 2 IN ONE SHOT   3 STARS = ALL 3"), Wd * 0.5f, Y + 52.f * U, 22.f * U, Gold, 1, 0);
	}

	void OnFinished(AZoogiGameMode* G)
	{
		if (!S.bOn || !G) return;
		FString Gain;
		if (S.GainCoins) Gain += FString::Printf(TEXT("  +%d COINS"), S.GainCoins);
		if (S.GainGems) Gain += FString::Printf(TEXT("  +%d GEMS"), S.GainGems);
		if (S.GainXP) Gain += FString::Printf(TEXT("  +%d XP"), S.GainXP);
		G->Banner = FString::Printf(TEXT("TRICK SHOT #%d: %d STAR%s%s   STREAK %d"), TodayNumber(), S.Best, S.Best == 1 ? TEXT("") : TEXT("S"), *Gain, S.StreakNow);
		S.bResult = true;
		End(G);
	}

	void OnMenu(AZoogiGameMode* G) { if (S.bOn) End(G); }
}
