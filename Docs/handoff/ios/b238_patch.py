# Nov 11 (Claude): B238 part 1 - wall-smash cinematic wired to real moments 1-5, award banner 1
import io, os, sys, shutil
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
V = os.path.expanduser('~/mnt/dev/ios/b238')
shutil.copy(os.path.join(V, 'ZoogiAwardBanner.h'), os.path.join(S, 'Public', 'ZoogiAwardBanner.h'))
shutil.copy(os.path.join(V, 'ZoogiAwardBanner.cpp'), os.path.join(S, 'Private', 'ZoogiAwardBanner.cpp'))
def patch(rel, pairs, incs=()):
    p = os.path.join(S, rel); t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        n = t.count(a)
        if n != 1: print('FAIL', rel, n, a[:110]); sys.exit(1)
        t = t.replace(a, b)
    for inc in incs:
        if inc not in t:
            k = t.index('#include', t.index('#include') + 1)
            t = t[:k] + inc + ' // b238\n' + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)

patch('Public/ZoogiWallCine.h', [
('	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);',
 '''	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	// b238 (Evan Nov 11: "Wall smash moment 1-5"): the real triggers. An offer is queued and plays at the start of the next turn
	// (a quiet moment). Limits per match: MATCH POINT 1, AWAKEN 2, WALL SLAM 2, FIRST KO 1, MEGA HIT 2, 5 in all, never two turns
	// running (-ZoogiWallCineMax=, -ZoogiWallCineAlways = no limits).
	ZOOGIROLL_API bool Offer(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim, int32 Kind);
	/** a knockout: picks MATCH POINT (last round) / AWAKEN (the attacker awakened this turn) / FIRST KO */
	ZOOGIROLL_API void OfferKO(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim);
	ZOOGIROLL_API void NoteAwaken(AZoogiGameMode* G, int32 Player);'''),
])
patch('Private/ZoogiWallCine.cpp', [
('''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld()) return;''',
 '''	// ---- b238: the real triggers (moments 1-5)
	namespace
	{
		struct FOffer { int32 Kind = 0; TWeakObjectPtr<AZoogiBall> A, V; };
		struct FMatch { TWeakObjectPtr<AZoogiGameMode> G; int32 LastRound = 0, Total = 0, KOs = 0, LastPlayTurn = -100, AwPlayer = -1, AwTurn = -1; int32 Count[7] = { 0, 0, 0, 0, 0, 0, 0 }; FOffer Pend; };
		FMatch M;
		void Sync(AZoogiGameMode* G)
		{
			if (M.G.Get() != G || G->RoundNumber < M.LastRound) { const FMatch Fresh; M = Fresh; M.G = G; }
			M.LastRound = G->RoundNumber;
		}
		int32 Prio(int32 K) { return K == 1 ? 5 : K == 2 ? 4 : K == 4 ? 3 : K == 3 ? 2 : 1; }
	}
	void NoteAwaken(AZoogiGameMode* G, int32 Player) { if (!G) return; Sync(G); M.AwPlayer = Player; M.AwTurn = G->TurnCounter; }
	bool Offer(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim, int32 Kind)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWallCine"));
		static const bool bAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallCineAlways"));
		static int32 MaxAll = -1; if (MaxAll < 0) { MaxAll = 5; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWallCineMax="), MaxAll); }
		if (bOff || !G || !Attacker || !Victim || Attacker == Victim || Kind < 1 || Kind > 5 || G->bSoloTraining) return false;
		Sync(G);
		static const int32 MaxKind[] = { 0, 1, 2, 2, 1, 2 };
		if (!bAlways && (M.Count[Kind] >= MaxKind[Kind] || M.Total >= MaxAll || (Kind != 1 && G->TurnCounter - M.LastPlayTurn < 2))) return false;
		if (M.Pend.Kind && Prio(M.Pend.Kind) >= Prio(Kind)) return false;
		M.Pend.Kind = Kind; M.Pend.A = Attacker; M.Pend.V = Victim;
		UE_LOG(LogZoogi, Log, TEXT("WALL CINE offer kind %d: P%d -> P%d (round %d, turn %d)"), Kind, Attacker->PlayerIndex + 1, Victim->PlayerIndex + 1, G->RoundNumber, G->TurnCounter);
		return true;
	}
	void OfferKO(AZoogiGameMode* G, AZoogiBall* Attacker, AZoogiBall* Victim)
	{
		if (!G || !Attacker || !Victim) return;
		Sync(G); ++M.KOs;
		const int32 Kind = G->RoundNumber >= G->MenuRounds ? 1 : (M.AwPlayer == Attacker->PlayerIndex && M.AwTurn == G->TurnCounter) ? 2 : M.KOs == 1 ? 4 : 0;
		if (Kind) Offer(G, Attacker, Victim, Kind);
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G || !G->GetWorld()) return;
		if (M.Pend.Kind && !C.bOn && M.G.Get() == G)
		{	// b238: a queued moment plays at the start of the next turn
			if (G->Phase == EZoogiPhase::GameOver) M.Pend = FOffer();
			else if (G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 0.35f)
			{
				const FOffer Pd = M.Pend; M.Pend = FOffer();
				AZoogiBall* Pa = Pd.A.Get(); AZoogiBall* Pv = Pd.V.Get();
				if (Pa && Pv && !Pa->bOut && !Pv->bOut && Play(G, Pa, Pv, Pd.Kind))
				{
					++M.Count[Pd.Kind]; ++M.Total; M.LastPlayTurn = G->TurnCounter;
					ZoogiFilm::Begin(FString::Printf(TEXT("cine_live_%d"), Pd.Kind), Len + 1.4f);
				}
			}
		}'''),
])
patch('Private/ZoogiGameMode.cpp', [
('''				BigText = FString::Printf(TEXT("KNOCKOUT!\\n%s  >  %s"), *A->Def.Name.ToUpper(), *B->Def.Name.ToUpper());''',
 '''				ZoogiWallCine::OfferKO(this, A, B); // b238: wall-smash moments 1 (match point) / 2 (awaken) / 4 (first KO)
				BigText = FString::Printf(TEXT("KNOCKOUT!\\n%s  >  %s"), *A->Def.Name.ToUpper(), *B->Def.Name.ToUpper());'''),
('''			ZoogiStageShift::OnRoundStart(this); // Oct 31 b46: halfway -> the arena takes its second form''',
 '''			ZoogiAwardBanner::OnRoundEnd(this); // b238: the hanging award banner for the round that just ended
			ZoogiStageShift::OnRoundStart(this); // Oct 31 b46: halfway -> the arena takes its second form'''),
], ['#include "ZoogiWallCine.h"', '#include "ZoogiAwardBanner.h"'])
patch('Private/ZoogiCardGlue.cpp', [
('''	bStandbyAwaken = true;
	if (GAwakeG.Get() != this)''', '''	bStandbyAwaken = true;
	ZoogiWallCine::NoteAwaken(this, B->PlayerIndex); // b238: a knockout this turn = the AWAKEN KNOCKOUT moment
	if (GAwakeG.Get() != this)'''),
], ['#include "ZoogiWallCine.h"'])
patch('Private/ZoogiEarthEvents.cpp', [
('''				if (bBall && Vn >= 800.f) G->PublicPopup(TEXT("ROCK SLAM!"), P + FVector(0.f, 0.f, 120.f), Col);''',
 '''				if (bBall && Vn >= 800.f) G->PublicPopup(TEXT("ROCK SLAM!"), P + FVector(0.f, 0.f, 120.f), Col);
				if (bBall && Vn >= 800.f) if (AZoogiBall* Sb = const_cast<AZoogiBall*>(Cast<AZoogiBall>(Who))) if (G->Balls.IsValidIndex(Sb->LastHitPlayer) && Sb->LastHitPlayer != Sb->PlayerIndex) ZoogiWallCine::Offer(G, G->Balls[Sb->LastHitPlayer], Sb, 3); // b238: the WALL SLAM moment'''),
], ['#include "ZoogiWallCine.h"'])
patch('Private/ZoogiActionFun.cpp', [
('''		AZoogiFX::Spawn(W, At, FLinearColor(1.f, 0.9f, 0.4f), Gr == 2 ? 260.f : 180.f, 0.4f, true, 14.f);''',
 '''		AZoogiFX::Spawn(W, At, FLinearColor(1.f, 0.9f, 0.4f), Gr == 2 ? 260.f : 180.f, 0.4f, true, 14.f);
		if (Gr == 2) if (AZoogiBall* Mv = Cast<AZoogiBall>(Hit)) if (AZoogiBall* Ms = Shooter.Get()) ZoogiWallCine::Offer(G, Ms, Mv, 5); // b238: the MEGA HIT moment (a PERFECT HIT on a rival)'''),
('''	if (!IsActive(G)) return;
	APlayerController* PC = HUD->GetOwningPlayerController();
	const float Wd = C->ClipX, Ht = C->ClipY, Mn = FMath::Min(Wd, Ht);''',
 '''	if (!IsActive(G)) return;
	ZoogiAwardBanner::Draw(G, C, [C](const FString& Ts, float Tx, float Ty, float Tp, const FLinearColor& Tc) { FunTextL(C, Ts, Tx, Ty, Tp, Tc, 1); }); // b238
	APlayerController* PC = HUD->GetOwningPlayerController();
	const float Wd = C->ClipX, Ht = C->ClipY, Mn = FMath::Min(Wd, Ht);'''),
], ['#include "ZoogiAwardBanner.h"'])
print('ALL OK')
