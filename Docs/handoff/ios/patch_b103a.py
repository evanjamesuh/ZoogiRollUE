# b103a: EPIC pass 1 - war drums, rock / debris sounds, physics debris (see ZoogiEpic.h)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P='Source/ZoogiRoll/Private/'
def first_inc(rel):
    s=open(os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/'+rel),encoding='utf-8-sig').read()
    return [l for l in s.splitlines() if l.startswith('#include')][0]
ed(P+'ZoogiTypes.cpp', [
 ('''		Add(TEXT("RockCrack"), Seq(TEXT("Impacts"), TEXT("Lyra_ImpactPlaster"), 9), 0.9f);''',
  '''		Add(TEXT("RockCrack"), Seq(TEXT("Impacts"), TEXT("Lyra_ImpactPlaster"), 9), 0.9f, 0.f, Seq(TEXT("Epic"), TEXT("SW_RockSmash"), 4), 0.75f); // b103: + a real rock smash
		// b103 (Evan: "epic devastating war drums, realistic rock and debris") - Scripts/synth_epic_sfx.py
		Add(TEXT("WarDon"), Seq(TEXT("Epic"), TEXT("SW_WarDon"), 4), 1.0f);
		Add(TEXT("WarKa"), Seq(TEXT("Epic"), TEXT("SW_WarKa"), 3), 0.8f);
		Add(TEXT("WarBoom"), Seq(TEXT("Epic"), TEXT("SW_WarBoom"), 2), 1.0f, 0.f, Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.6f);
		Add(TEXT("WarRoll"), Seq(TEXT("Epic"), TEXT("SW_WarRoll"), 1), 1.0f);
		Add(TEXT("Debris"), Seq(TEXT("Epic"), TEXT("SW_Debris"), 4), 0.9f);
		Add(TEXT("RockSmash"), Seq(TEXT("Epic"), TEXT("SW_RockSmash"), 4), 1.0f, 0.f, Seq(TEXT("Epic"), TEXT("SW_Debris"), 4), 0.55f);
		Add(TEXT("Rumble"), Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.9f);'''),
 ('''		Add(TEXT("Explosion"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Close"), 6), 0.9f);
		Add(TEXT("CritBoom"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Close"), 6), 0.8f, 0.45f);
		Add(TEXT("Fall"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Distant"), 4), 0.8f, 0.35f);''',
  '''		Add(TEXT("Explosion"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Close"), 6), 0.9f, 0.f, Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.7f); // b103: + a sub rumble
		Add(TEXT("CritBoom"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Close"), 6), 0.8f, 0.45f, Seq(TEXT("Epic"), TEXT("SW_Debris"), 4), 0.6f); // b103: + falling debris
		Add(TEXT("Fall"), Seq(TEXT("Explosions"), TEXT("Explosions_Grenade_Punch-Distant"), 4), 0.8f, 0.35f, Seq(TEXT("Epic"), TEXT("SW_Rumble"), 2), 0.6f);'''),
])
ed(P+'ZoogiActionFun.cpp', [
 ('	TickUltimate(G, Dt, RealDt);\n', '	TickUltimate(G, Dt, RealDt);\n	ZoogiEpic::Tick(G, RealDt); // b103: the war drums\n'),
 (first_inc(P+'ZoogiActionFun.cpp')+'\n', first_inc(P+'ZoogiActionFun.cpp')+'\n#include "ZoogiEpic.h"\n'),
])
ed(P+'ZoogiMusicSubsystem.cpp', [
 ('	OnMatchEvent.Broadcast(E, Who, Mag);\n', '	OnMatchEvent.Broadcast(E, Who, Mag);\n	ZoogiEpic::OnMatchEvent(this, E, Who, Mag); // b103: war drum hits on the big moments\n'),
 (first_inc(P+'ZoogiMusicSubsystem.cpp')+'\n', first_inc(P+'ZoogiMusicSubsystem.cpp')+'\n#include "ZoogiEpic.h"\n'),
])
ed(P+'ZoogiSuperFun.cpp', [
 ('''			ZoogiSkills::AwakenCast(G, B, FName(Aw->Skill));
			return true;''', '''			ZoogiEpic::Hit(G, ZoogiEpic::EHit::Boom); ZoogiEpic::Surge(1.f, 6.f); // b103
			ZoogiSkills::AwakenCast(G, B, FName(Aw->Skill));
			return true;'''),
 ('''		Blast(G, L, 720.f, 650.f, 1500.f, 320.f, B, true, true);
		DamageArena(G, L, 800.f, B, 3);''', '''		Blast(G, L, 720.f, 650.f, 1500.f, 320.f, B, true, true);
		DamageArena(G, L, 800.f, B, 3);
		ZoogiEpic::DebrisBurst(W, L, 1.5f); ZoogiEpic::Hit(G, ZoogiEpic::EHit::Boom); ZoogiEpic::Surge(1.f, 6.f); // b103'''),
 ('#include "ZoogiFront.h"\n', '#include "ZoogiFront.h"\n#include "ZoogiEpic.h"\n'),
])
ed(P+'ZoogiDuel.cpp', [
 ('	if (ZoogiFilm::On() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmDuels"))) ZoogiFilm::Begin(TEXT("duel"), 16.f); // b101',
  '	if (ZoogiFilm::On() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmDuels"))) ZoogiFilm::Begin(TEXT("duel"), 16.f); // b101\n	ZoogiEpic::Hit(G, ZoogiEpic::EHit::Roll); // b103: the war drums roll into the duel'),
 ('#include "ZoogiFilm.h"\n', '#include "ZoogiFilm.h"\n#include "ZoogiEpic.h"\n'),
])
ed(P+'ZoogiSkills.cpp', [
 ('''		AZoogiFX::Stylized(W, SkillBlastFx(), At, 0.45f * FMath::Max(Mul, 0.6f) * SkillBlastSc());''', '''		AZoogiFX::Stylized(W, SkillBlastFx(), At, 0.45f * FMath::Max(Mul, 0.6f) * SkillBlastSc());
		ZoogiEpic::DebrisBurst(W, At, 0.45f * Mul); // b103: rock chunks out of the floor'''),
 ('''					WolfBlast(Gm, Hit, Pi, 1.f);
					if (AActor* Rk = V->Rock.Get()) Rk->SetLifeSpan(1.4f);''', '''					WolfBlast(Gm, Hit, Pi, 1.f);
					ZoogiEpic::Hit(Gm, ZoogiEpic::EHit::Boom); // b103
					if (AActor* Rk = V->Rock.Get()) Rk->SetLifeSpan(1.4f);'''),
 ('#include "ZoogiFilm.h"\n', '#include "ZoogiFilm.h"\n#include "ZoogiEpic.h"\n'),
])
ed(P+'ZoogiFeel.cpp', [
 ('''		if (W > 1.35f && Impulse > 9000.f)
		{''', '''		if (Impulse > 15000.f && Cast<AZoogiBall>(Other)) ZoogiEpic::DebrisBurst(World, (Self->BallLocation() + Cast<AZoogiBall>(Other)->BallLocation()) * 0.5f - FVector(0, 0, ZoogiConst::ZoogiRadius - 5.f), 0.3f); // b103: the hardest hits chip the floor
		if (W > 1.35f && Impulse > 9000.f)
		{'''),
 (first_inc(P+'ZoogiFeel.cpp')+'\n', first_inc(P+'ZoogiFeel.cpp')+'\n#include "ZoogiEpic.h"\n'),
])
print('b103a ok')
