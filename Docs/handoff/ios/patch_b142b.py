# b142b (Evan 11:23): arena responses wired into CheckKnockoffs (ZoogiArenaReact): Cosmos zero-g fly-out, Cosmos rim opened,
# stands = not out + crowd token, KO / orb explosions at the real spot through the Skill Lab PLAYER / ORB KNOCKOUT entries;
# Skill Lab gets the two entries. Also -ZoogiTrainBotsTest (training CPU OPPONENTS check, Evan 11:39).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''#include "ZoogiEdgeHang.h"''', '''#include "ZoogiEdgeHang.h"
#include "ZoogiArenaReact.h"''')
# orbs: the burst at the orb's own spot + the lab's ORB KNOCKOUT
ed1(G, '''			O->SetOut();
			AZoogiFX::Spawn(GetWorld(), EdgePoint(L, Arena), O->bStar ? FLinearColor(1.f, 0.75f, 0.2f) : FLinearColor(0.5f, 0.85f, 1.f), 160.f, 0.6f, false, 7.f);
			AZoogiFX::Niagara(GetWorld(), O->bStar ? TEXT("NS_Burst_Gold") : TEXT("NS_Burst_Blue"), EdgePoint(L, Arena));''', '''			O->SetOut();
			// b142 (Evan): the orb's knock-out burst where the orb is (not the court edge), Skill Lab "ORB KNOCKOUT" picks the blast
			static const bool bOldOrbSpot = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldKOSpot"));
			const FVector OrbAt = bOldOrbSpot ? EdgePoint(L, Arena) : FVector(L.X, L.Y, FMath::Clamp(L.Z, -60.f, 600.f) + 30.f);
			ZoogiArenaReact::OrbOutBlast(GetWorld(), OrbAt, O->bStar);''')
ed1(G, '''		const FVector L = B->BallLocation();
		if (ZoogiEdgeHang::Track(this, B, L)) continue; // b58 (Evan): dropped off the edge -> its Aurex hangs on the lip (tap to pull up)
		// Nov 1 b53 (Evan: "when a Zoogi hits them they bounce back toward the centre naturally"): a Zoogi that reaches the solid
		// stands is sent back toward the middle (it keeps most of its speed, with a little hop) instead of resting up there
		if (AZoogiArena::StandBlockR > 0.f && B->Sphere && L.Z > -120.f && FVector2D(L.X, L.Y).Size() > AZoogiArena::StandBlockR)''', '''		const FVector L = B->BallLocation();
		// b142 (Evan: "players that hit those stands shouldn't be counted as out"): heading into / thrown back from the stands
		if (ZoogiArenaReact::StandsCheck(this, B, L)) continue;
		// b142 (Evan: "on Cosmos there's no gravity, they shouldn't be going down"): over the edge = out now, it floats away
		const bool bZeroG = ZoogiArenaReact::CosmicEdgeOut(this, B, L);
		if (!bZeroG && ZoogiEdgeHang::Track(this, B, L)) continue; // b58 (Evan): dropped off the edge -> its Aurex hangs on the lip (tap to pull up)
		// Nov 1 b53 (Evan: "when a Zoogi hits them they bounce back toward the centre naturally"): a Zoogi that reaches the solid
		// stands is sent back toward the middle (it keeps most of its speed, with a little hop) instead of resting up there
		// (b142: only with -ZoogiNoStandsSave - the stands crash + crowd token above replaces it)
		static const bool bNoStandsSave = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoStandsSave"));
		if (bNoStandsSave && AZoogiArena::StandBlockR > 0.f && B->Sphere && L.Z > -120.f && FVector2D(L.X, L.Y).Size() > AZoogiArena::StandBlockR)''')
ed1(G, '''		if (AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300
		{
			// b140''', '''		if (bZeroG || AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300
		{
			// b140''')
ed1(G, '''			AZoogiFX::Spawn(GetWorld(), KOAt, FLinearColor(1.f, 0.3f, 0.25f), 220.f, 0.7f, false, 7.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), KOAt, FRotator::ZeroRotator, 1.4f);
			// Oct 23: Easy Shockwaves vertical + ring-out 2s SFX (attacker tint when known).
			{
				const FVector At = KOAt;
				const FLinearColor Tint = (Balls.IsValidIndex(B->LastHitPlayer) && B->LastHitPlayer != B->PlayerIndex)
					? ZoogiUtil::PlayerColor(B->LastHitPlayer) : FLinearColor(1.f, 0.35f, 0.3f);
				AZoogiFX::ShockwaveByMag(GetWorld(), At, 30000.f, &Tint, true, true);
				// b83 (Evan: "I want this for the knockout explosion"): the pack's purple cartoon blast (dark purple puffs, magenta core)
				// with the purple ray streaks; -ZoogiOldKOBlast = the b55 v3 blast
				if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldKOBlast"))) AZoogiFX::Stylized(GetWorld(), TEXT("Big_Explosion_v3"), At, 0.7f);
				else { AZoogiFX::Stylized(GetWorld(), TEXT("Big_Explosion_v2"), At, 1.0f); AZoogiFX::Stylized(GetWorld(), TEXT("Mid_Explosion_v2"), At, 0.9f); }
			}
			B->SetOut();''', '''			// Oct 23 / b83 / b142: burst + vertical shockwave (attacker tint when known) + the KO blast (Skill Lab "PLAYER KNOCKOUT");
			// on Cosmos the blast waits until the Zoogi has floated out (zero-g)
			const FLinearColor KOTint = (Balls.IsValidIndex(B->LastHitPlayer) && B->LastHitPlayer != B->PlayerIndex)
				? ZoogiUtil::PlayerColor(B->LastHitPlayer) : FLinearColor(1.f, 0.35f, 0.3f);
			const FVector KOVel = B->GetBallVelocity();
			if (!bZeroG) ZoogiArenaReact::PlayerKOBlast(GetWorld(), KOAt, KOTint);
			B->SetOut();
			if (bZeroG) ZoogiArenaReact::StartFlyOut(this, B, KOVel, KOTint);''')
ed1(G, '''				ZoogiScoringUI::OnKnockout(this, A->PlayerIndex, EdgePoint(L, Arena));
				AddPopup(FString::Printf(TEXT("+%d KNOCKOUT  P%d"), KOPts, A->PlayerIndex + 1), EdgePoint(L, Arena) + FVector(0, 0, 90.f), ZoogiUtil::PlayerColor(A->PlayerIndex));''', '''				ZoogiScoringUI::OnKnockout(this, A->PlayerIndex, KOAt);
				AddPopup(FString::Printf(TEXT("+%d KNOCKOUT  P%d"), KOPts, A->PlayerIndex + 1), KOAt + FVector(0, 0, 90.f), ZoogiUtil::PlayerColor(A->PlayerIndex));''')
ed1(G, '''				AZoogiFX::Spawn(GetWorld(), EdgePoint(L, Arena) - FVector(0, 0, 20.f), BigColor, 420.f, 0.8f, true, 12.f);''', '''				AZoogiFX::Spawn(GetWorld(), KOAt - FVector(0, 0, 20.f), BigColor, 420.f, 0.8f, true, 12.f);''')
# training CPU OPPONENTS self-test
ed1(G, '''		UE_LOG(LogZoogi, Log, TEXT("TRAINING: you + %d bot(s)"), Bots);''', '''		UE_LOG(LogZoogi, Log, TEXT("TRAINING: you + %d bot(s)"), Bots);
		static bool bTrainBotsTest = false;
		if (!bTrainBotsTest && FParse::Param(FCommandLine::Get(), TEXT("ZoogiTrainBotsTest")))
		{	// b142 (Evan 11:39 "Opponent button doesn't work in training"): press it from code 6 s in, then count the Zoogis
			bTrainBotsTest = true;
			TWeakObjectPtr<AZoogiGameMode> WTb = this; const double Tb0 = FPlatformTime::Seconds(); int32 TbStep = 0;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WTb, Tb0, TbStep](float) mutable -> bool
			{
				AZoogiGameMode* Gt = WTb.Get(); if (!Gt) return false;
				const double Tt = FPlatformTime::Seconds() - Tb0;
				if (TbStep == 0 && Tt > 6.0) { TbStep = 1; UE_LOG(LogZoogi, Log, TEXT("TRAINBOTS TEST: CPU OPPONENTS -> 2 (balls now %d, players %d)"), Gt->Balls.Num(), Gt->MenuNumPlayers); Gt->MenuAction(TEXT("TrainBots"), 2); }
				if (TbStep == 1 && Tt > 14.0) { int32 Cpu = 0; for (AZoogiBall* Bq : Gt->Balls) if (Bq && Bq->bCPU) ++Cpu; UE_LOG(LogZoogi, Log, TEXT("TRAINBOTS TEST: after the restart %d Zoogis, %d CPU, training %d"), Gt->Balls.Num(), Cpu, Gt->bSoloTraining ? 1 : 0); Gt->TakeShotDelayed(TEXT("trainbots_after"), 0.f); return false; }
				return true;
			}));
		}''')
O = 'Private/ZoogiArenaObstacles.cpp'
ed1(O, '''#include "ZoogiRoll.h"''', '''#include "ZoogiRoll.h"
#include "ZoogiArenaReact.h"''')
ed1(O, '''	if (N) UE_LOG(LogZoogi, Log, TEXT("Obstacles (theme %d): %d placed''', '''	ZoogiArenaReact::OpenCosmicRim(this); // b142 (Evan: "orbs and players get knocked back in" on Cosmos)
	if (N) UE_LOG(LogZoogi, Log, TEXT("Obstacles (theme %d): %d placed''')
# Skill Lab: ORB / PLAYER KNOCKOUT
L = 'Private/ZoogiSkillLab.cpp'
b = open(R + L, 'rb').read().decode()
i = b.index('const TCHAR* Skills[] = {'); j = b.index('};', i)
assert 'ko_orb' not in b[i:j]
b = b[:j].rstrip() + ' TEXT("ko_orb"), TEXT("ko_player") ' + b[j:] if b[:j].rstrip().endswith(',') else b[:j].rstrip() + ', TEXT("ko_orb"), TEXT("ko_player") ' + b[j:]
open(R + L, 'wb').write(b.encode())
ed1(L, '''		FString SkillName(int32 i) { return FString(Skills[FMath::Clamp(i, 0, NumSkills - 1)]).Replace(TEXT("_"), TEXT(" ")).ToUpper(); }''', '''		FString SkillName(int32 i)
		{
			const FString Id = Skills[FMath::Clamp(i, 0, NumSkills - 1)];
			if (Id == TEXT("ko_orb")) return TEXT("ORB KNOCKOUT"); // b142 (Evan: "in Skills Lab also add orb and player ... swap out explosion types")
			if (Id == TEXT("ko_player")) return TEXT("PLAYER KNOCKOUT");
			return Id.Replace(TEXT("_"), TEXT(" ")).ToUpper();
		}''')
ed1(L, '''			const FName Id(Skills[GSel]);
			AZoogiBall* Me = Caster(G);
			if (Id == FName(TEXT("wolf_pack")))''', '''			const FName Id(Skills[GSel]);
			AZoogiBall* Me = Caster(G);
			if (Id == FName(TEXT("ko_orb")) || Id == FName(TEXT("ko_player")))
			{	// b142: the knock-out explosion on the rack spot (what plays when an orb / a Zoogi is knocked out in a match)
				const FVector At = GSpot + FVector(0.f, 0.f, (GOrbZ > -1e8f ? GOrbZ : 40.f) + 10.f);
				if (Id == FName(TEXT("ko_orb"))) ZoogiArenaReact::OrbOutBlast(G->GetWorld(), At, false);
				else ZoogiArenaReact::PlayerKOBlast(G->GetWorld(), At, FLinearColor(1.f, 0.35f, 0.3f));
			}
			else if (Id == FName(TEXT("wolf_pack")))''')
ed1(L, '''#include "ZoogiWolfPack.h"''', '''#include "ZoogiWolfPack.h"
#include "ZoogiArenaReact.h"''')
ed1(L, '''	const TCHAR* BlastFx(const TCHAR* DefaultFx) {''', '''	const TCHAR* BlastFor(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); if (!T || T->Blast <= 0 || T->Blast >= NumBlasts) return nullptr; return BlastFxs[T->Blast]; }
	float BlastSizeFor(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->BlastSz * T->Size, 0.2f, 6.f) : 1.f; }
	bool TintFor(FName Skill, FLinearColor& Out) { Load(); const FTune* T = GTunes.Find(Skill); if (!T || T->Color <= 0 || T->Color >= NumColors) return false; Out = Colors[T->Color]; return true; }
	const TCHAR* BlastFx(const TCHAR* DefaultFx) {''')
H = 'Public/ZoogiSkillLab.h'
ed1(H, '''	ZOOGIROLL_API const TCHAR* ChosenBlast();''', '''	ZOOGIROLL_API const TCHAR* ChosenBlast();
	/** b142: any entry's BLAST pick (nullptr = DEFAULT, "" = NONE), its BLAST SIZE x FX SIZE, its COLOUR - "ko_orb" / "ko_player" = the knock-out explosions */
	ZOOGIROLL_API const TCHAR* BlastFor(FName Skill);
	ZOOGIROLL_API float BlastSizeFor(FName Skill);
	ZOOGIROLL_API bool TintFor(FName Skill, FLinearColor& Out);''')
print('b142b ok')
