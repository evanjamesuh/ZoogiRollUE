# b124a (Evan 21:44: "Fix cpu ai they should play like a person would and be smarter"; 21:45: "The ice walls built from players stay
# to the end of the match unless destroyed after three hits")
import os, shutil
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
IOS = os.path.expanduser('~/mnt/dev/ios/')
shutil.copy(IOS + 'ZoogiCPUSmart.h', R + 'Public/ZoogiCPUSmart.h')
shutil.copy(IOS + 'ZoogiCPUSmart.cpp', R + 'Private/ZoogiCPUSmart.cpp')

# ---------------------------------------------------------------- ice walls: whole match, three hits
ed1('Private/ZoogiSkills.cpp', '''			AZoogiIceBarrier::Spawn(W, At(P.A), At(P.B), 220.f, Pi, 1); // b71 (Grok): taller spires like the Fab demo (was 150)''',
'''			// b124 (Evan: "ice walls built from players stay to the end of the match unless destroyed after three hits"): no round limit
			static const bool bOldLife = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallOneRound"));
			AZoogiIceBarrier::Spawn(W, At(P.A), At(P.B), 220.f, Pi, bOldLife ? 1 : 0); // b71 (Grok): taller spires like the Fab demo (was 150)''')
ed1('Private/ZoogiIceBarrier.cpp', '''	const float St = (bBall ? 1.f : 0.4f) * (Imp > 1500.f ? 2.f : 1.f);''',
'''	float St = (bBall ? 1.f : 0.4f) * (Imp > 1500.f ? 2.f : 1.f);
	// b124 (Evan: "ice walls built from players ... destroyed after three hits"): a player's wall takes exactly three Zoogi hits
	// (soft or hard - each hit is one crack stage); orbs bounce off it. -ZoogiIceWallOldDamage = the old speed-based damage
	static const bool bOldDmg = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallOldDamage"));
	if (Owner >= 0 && !bOldDmg) { if (!bBall) return; St = 1.f; }''')

ed1('Public/ZoogiIceWall.h', '''	float LastHitLog = -10.f;''', '''	float LastHitLog = -10.f;
	int32 HitCount = 0; float LastCountT = -10.f; // b124: three hits break the wall''')
ed1('Private/ZoogiIceWall.cpp', '''	if (Speed < T.ShatterSpeed)
	{
		// a solid knock''', '''	// b124 (Evan: "the ice walls built from players stay to the end of the match unless destroyed after three hits"): every Zoogi
	// hit counts once (a knock, not a resting touch); the third shatters the whole wall. -ZoogiIceWallSpeedShatter = the old rule
	static const bool bSpeedRule = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallSpeedShatter"));
	if (!bSpeedRule)
	{
		if (Speed < 150.f || Now - LastCountT < 0.3f) return;
		LastCountT = Now; ++HitCount;
		UE_LOG(LogZoogi, Log, TEXT("ICEWALL effect=%lld hit %d/3 by P%d (%.0f cm/s)"), Effect, HitCount, Ball->PlayerIndex + 1, Speed);
		if (HitCount < 3)
		{
			AZoogiFX::Spawn(GetWorld(), Hit.ImpactPoint, T.Glow, 90.f + 40.f * HitCount, 0.25f, false, 6.f);
			ZoogiUtil::PlaySfx(this, TEXT("OrbClink"), 0.9f, 0.6f + 0.15f * HitCount);
			return;
		}
		for (int32 s = 0; s < SegLive.Num(); ++s) if (SegLive[s]) ShatterSegment(s, Hit.ImpactPoint, Speed, Ball);
		if (T.PassThroughKeep > 0.f && Ball->Sphere) Ball->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, 0.f) * T.PassThroughKeep);
		ZoogiFramed::FrameShake(0.7f, GetWorld());
		return;
	}
	if (Speed < T.ShatterSpeed)
	{
		// a solid knock''')

# ---------------------------------------------------------------- shot choice: the lookahead pick for the default CPU too
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''	// Pick one of the three cheapest shots so CPU players vary their targets.
	Plans.Sort(''', '''	// b124 (Evan: "CPUs should play like a person would and be smarter"): a person goes after the score leader
	if (ZoogiCPUSmart::On())
	{
		int32 Top = -1; for (const AZoogiBall* O : Balls) if (O && O != Me && !O->bOut && !IsAlly(Me, O)) Top = FMath::Max(Top, O->Score);
		for (FPlan& Pl : Plans) if (const AZoogiBall* Tb = Cast<AZoogiBall>(Pl.Actor)) if (Tb->Score >= Top && Tb->Score > Me->Score) Pl.Cost -= 150.f;
	}
	// Pick one of the three cheapest shots so CPU players vary their targets.
	Plans.Sort(''')
ed1(G, '''		if (CPUTier == EZoogiCPUTier::Normal)
		{
			int32 NPick = 1;''', '''		if (CPUTier == EZoogiCPUTier::Normal && !ZoogiCPUSmart::On())
		{
			int32 NPick = 1;''')
ed1(G, '''			PickIdx = ZoogiCPU::PickShot(Opts, ZoogiCPU::TierRow(CPUTier), GameRng);''', '''			// b124: the default (Normal) CPU also looks one move ahead - where its Zoogi ends up and how open it is to the reply
			FZoogiTierRow Row = ZoogiCPU::TierRow(CPUTier);
			if (CPUTier == EZoogiCPUTier::Normal) { Row.Lookahead = 1; Row.Candidates = 3; Row.PickSpread = 220.f; }
			PickIdx = ZoogiCPU::PickShot(Opts, Row, GameRng);''')
b = open(R + G, 'rb').read()
if b'#include "ZoogiCPUSmart.h"' not in b:
    ed1(G, '#include "ZoogiGameMode.h"\n', '#include "ZoogiGameMode.h"\n#include "ZoogiCPUSmart.h"\n')

# ---------------------------------------------------------------- cards / skills: chosen for the moment
C = 'Private/ZoogiCardSubsystem.cpp'
ed1(C, '#include "ZoogiCPUBrain.h"\n', '#include "ZoogiCPUBrain.h"\n#include "ZoogiCPUSmart.h"\n')
ed1(C, '''	const FZoogiCardContext Ctx = BuildContext(G);
	// Oct 14 playtest fix: Crystal Collapse first''', '''	// b124 (Evan: "CPUs should play like a person would and be smarter"): skills picked for what's on the table right now (a cannon
	// only with a target in reach, a shield when exposed, meteors on a bunch...), each CPU with its own temperament, best value per
	// SP first, at most 3, nothing pointless. Energy refreshes every turn, so a good moment spends it all. -ZoogiCPULegacy = old order
	if (ZoogiCPUSmart::On() && TestCase.IsEmpty() && ZoogiForcedCard() == NAME_None && G)
	{
		const AZoogiBall* Me = G->GetActiveBall();
		FRandomStream Nr(HashCombine(GetTypeHash(G->GameSeed ^ 0x3A11), GetTypeHash(P * 977 + S->Turn)));
		struct FV { int32 I; float V; int32 C; FName Id; };
		TArray<FV> Vs;
		for (int32 i = 0; i < Hand.Num(); ++i)
		{
			const FZoogiCardDef* D = Def(Hand[i].CardId);
			float V = ZoogiCPUSmart::CardValue(G, Me, Hand[i].CardId, Nr.FRandRange(-10.f, 10.f));
			if (V < 0.f) V = D ? D->CPUWeight * 18.f + Nr.FRandRange(-6.f, 6.f) : 0.f;
			Vs.Add({ i, V, D ? FMath::Max(1, D->Cost) : 99, Hand[i].CardId });
		}
		Vs.Sort([](const FV& A, const FV& Bv) { return A.V - 8.f * A.C > Bv.V - 8.f * Bv.C; });
		TArray<int32> Smart; TArray<FName> Took; int32 E = S->Energy; FString Why;
		for (const FV& X : Vs)
		{
			Why += FString::Printf(TEXT(" %s=%.0f"), *X.Id.ToString(), X.V);
			if (X.V < 22.f || X.C > E || Took.Contains(X.Id) || Smart.Num() >= 3) continue;
			Smart.Add(X.I); Took.Add(X.Id); E -= X.C;
		}
		Order = Smart;
		UE_LOG(LogZoogi, Log, TEXT("CARDS cpu smart P%d (temper %.2f, %d SP): plays %d -%s"), P + 1, ZoogiCPUSmart::Aggression(G, P), S->Energy, Smart.Num(), *Why);
	}
	const FZoogiCardContext Ctx = BuildContext(G);
	// Oct 14 playtest fix: Crystal Collapse first''')

# ---------------------------------------------------------------- skill aim: smarter targets
S = 'Private/ZoogiSkills.cpp'
b = open(R + S, 'rb').read()
if b'#include "ZoogiCPUSmart.h"' not in b:
    ed1(S, '#include "ZoogiIceBarrier.h"\n', '#include "ZoogiIceBarrier.h"\n#include "ZoogiCPUSmart.h"\n')
ed1(S, '''		const FVector Fwd = Cand.Num() ? ActorPos(Cand[0]) : S.Origin + FVector(S.Range * 0.5f, 0, 0);
		S.CpuCtr = ClampRange(Fwd);
		S.CpuA = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f + FVector(0, 250.f, 0)); S.CpuB = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f - FVector(0, 250.f, 0));''',
'''		const bool bSmart = ZoogiCPUSmart::On();
		if (bSmart)
		{	// b124: never a teammate; rivals near their edge / the leader first
			Cand.RemoveAll([&](AActor* X) { const AZoogiBall* Xb = Cast<AZoogiBall>(X); return Xb && G->IsAlly(B, Xb); });
			Cand.Sort([&](const AActor& X, const AActor& Y) { return ZoogiCPUSmart::TapRank(G, B, const_cast<AActor*>(&X)) < ZoogiCPUSmart::TapRank(G, B, const_cast<AActor*>(&Y)); });
		}
		FVector Fwd = Cand.Num() ? ActorPos(Cand[0]) : S.Origin + FVector(S.Range * 0.5f, 0, 0);
		if (bSmart && (D->Aim == EAim::Area || D->Aim == EAim::Spots) && Cand.Num() > 1)
		{	// b124: area skills land on the biggest bunch (rivals count double)
			float BestN = -1.f;
			for (AActor* X : Cand)
			{
				FVector Sum = FVector::ZeroVector; float N = 0.f, W = 0.f;
				for (AActor* Y : Cand) if (FVector::Dist2D(ActorPos(X), ActorPos(Y)) < FMath::Max(250.f, D->R * 0.8f)) { const float Wy = Cast<AZoogiBall>(Y) ? 2.f : 1.f; N += Wy; Sum += ActorPos(Y) * Wy; W += Wy; }
				if (N > BestN) { BestN = N; Fwd = Sum / FMath::Max(W, 1.f); }
			}
		}
		S.CpuCtr = ClampRange(Fwd);
		S.CpuA = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f + FVector(0, 250.f, 0)); S.CpuB = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f - FVector(0, 250.f, 0));
		if (bSmart && D->Aim == EAim::Line)
		{	// b124: a person walls off their OWN nearest edge (a backstop), not the line of their own shot
			FVector2D Ed(1.f, 0.f);
			const float E = ZoogiCPUSmart::EdgeDist(G, FVector2D(S.Origin), &Ed);
			const float Off = FMath::Clamp(E * 0.65f, 160.f, FMath::Min(S.Range * 0.8f, 650.f));
			const FVector Ctr = S.Origin + FVector(Ed.X, Ed.Y, 0.f) * Off, Perp(-Ed.Y, Ed.X, 0.f);
			S.CpuA = ClampRange(Ctr + Perp * 280.f); S.CpuB = ClampRange(Ctr - Perp * 280.f);
		}''')

# ---------------------------------------------------------------- aiming skills cast mid-roll, like the player's COMBO
P = 'Private/ZoogiP5Command.cpp'
ed1(P, '#include "ZoogiShowtime.h"\n', '#include "ZoogiShowtime.h"\n#include "ZoogiCPUSmart.h"\n')
ed1(P, '''		TArray<FCpuStep> CpuSteps; TArray<FSkillRow> CpuRows; TArray<FName> CpuCasts;''', '''		TArray<FCpuStep> CpuSteps; TArray<FSkillRow> CpuRows; TArray<FName> CpuCasts;
		TArray<FName> CpuRollCasts; double CpuRollNextAt = 0.0; // b124: skills the CPU casts while rolling''')
ed1(P, '''		GS.CpuRows.Reset(); GS.CpuCasts.Reset(); GS.CpuSteps.Reset();''', '''		GS.CpuRows.Reset(); GS.CpuCasts.Reset(); GS.CpuSteps.Reset(); GS.CpuRollCasts.Reset();''')
ed1(P, '''				if (ZoogiSkills::IsSkill(Cm.Card.CardId)) GS.CpuCasts.Add(Cm.Card.CardId);''', '''				// b124: aiming skills wait for the roll (cast from where the Zoogi is then, like the player's COMBO); buffs before launch
				if (ZoogiSkills::IsSkill(Cm.Card.CardId)) (ZoogiCPUSmart::On() && ZoogiCPUSmart::CastMidRoll(Cm.Card.CardId) ? GS.CpuRollCasts : GS.CpuCasts).Add(Cm.Card.CardId);''')
ed1(P, '''		if (UGameplayStatics::IsGamePaused(World) || ZoogiSkillLab::Open()) { W.SetVisibility(EVisibility::Collapsed); GShowing = false; return true; } // b79: nothing over the pause menu; b108: or the lab
		const double Now = FPlatformTime::Seconds();''', '''		if (UGameplayStatics::IsGamePaused(World) || ZoogiSkillLab::Open()) { W.SetVisibility(EVisibility::Collapsed); GShowing = false; return true; } // b79: nothing over the pause menu; b108: or the lab
		const double Now = FPlatformTime::Seconds();
		if (GS.CpuRollCasts.Num())
		{	// b124 (Evan: "CPUs should play like a person"): the CPU casts its aiming skills mid-roll when a target is in reach
			AZoogiBall* Rb = G->GetActiveBall();
			if (!Rb || !Rb->bCPU || Rb->bOut || G->TurnCounter != GS.CpuTurn || (G->Phase != EZoogiPhase::Aiming && G->Phase != EZoogiPhase::Rolling))
			{
				UE_LOG(LogZoogi, Log, TEXT("P5CMD cpu roll casts: %d left unused (the roll ended)"), GS.CpuRollCasts.Num());
				GS.CpuRollCasts.Reset();
			}
			else if (G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.45f && Now >= GS.CpuRollNextAt && !ZoogiSkills::Picking() && !ZoogiRedirect::Picking())
			{
				bool bLast = false;
				const FName Id = GS.CpuRollCasts[0];
				if (ZoogiCPUSmart::WantCastNow(G, Rb, Id, bLast))
				{
					GS.CpuRollCasts.RemoveAt(0);
					GS.CpuRollNextAt = Now + 0.9 + FMath::FRandRange(0.f, 0.4f);
					UE_LOG(LogZoogi, Log, TEXT("P5CMD cpu P%d casts %s mid-roll (%s, %.0f cm/s)"), Rb->PlayerIndex + 1, *Id.ToString(), bLast ? TEXT("last chance") : TEXT("target in reach"), Rb->GetBallVelocity().Size2D());
					ZoogiSkills::CpuCast(G, Id);
				}
			}
		}''')

# ---------------------------------------------------------------- REDIRECT: a person saves themselves, and picks real chances
D = 'Private/ZoogiRedirect.cpp'
ed1(D, '#include "ZoogiGameMode.h"\n', '#include "ZoogiGameMode.h"\n#include "ZoogiArena.h"\n#include "ZoogiCPUSmart.h"\n')
ed1(D, '''			if (!bBusy && CpuUsed < 2 && Crystals(G, A->PlayerIndex) > 0 && G->PhaseTime > 0.8f && G->PhaseTime < 10.f && A->GetBallVelocity().Size2D() < 900.f && A->GetBallVelocity().Size2D() > 120.f)
			{
				CpuNextAt = Now() + 0.6;
				TArray<AActor*> Ts = Candidates(G, A);
				AActor* Pick = nullptr; float Best = -1e9f;
				for (AActor* T : Ts)
				{
					const bool bRival = Cast<AZoogiBall>(T) != nullptr;
					const float Score = (bRival ? 4000.f : 0.f) + Loc(T).Size2D() - 0.5f * FVector::Dist2D(Loc(T), A->BallLocation()) + FMath::FRandRange(0.f, 300.f);
					if (Score > Best) { Best = Score; Pick = T; }
				}
				static const bool bInstant = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCpuInstant"));
				if (Pick && FMath::FRand() < 0.55f)''', '''			// b124 (Evan: "CPUs should play like a person"): about to roll off the edge? a person spends a crystal to save the Zoogi
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
				if (Pick && FMath::FRand() < Want)''')
print('b124a ok')
