# b139c: the lab BLAST at the hit points of the skills that have no blast of their own (b139b part 2)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
S = R + 'Private/ZoogiSkills.cpp'
b = open(S, 'rb').read().decode('utf-8')
def add_after(o, extra):
    global b
    assert b.count(o) == 1, o[:90]
    b = b.replace(o, o + extra)
add_after('''					AZoogiFabFX::Play(Gm->GetWorld(), FX(TEXT("NS_Water_hit")), ActorPos(Tt) - FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_hit")), 0.4f), 3.f); // b71 (Grok / Evan): from the target's floor, longer (size: FxTune)''',
          '''
					LabImpact(Gm->GetWorld(), ActorPos(Tt)); // b139b''')
add_after('''						AZoogiFabFX::Play(Gn->GetWorld(), FX(TEXT("NS_Water_hit")), ActorPos(Tu) - FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_hit")), 0.4f), 3.f);''',
          '''
						LabImpact(Gn->GetWorld(), ActorPos(Tu)); // b139b''')
add_after('''					AZoogiFabFX::Play(Gm->GetWorld(), FX(TEXT("NS_Magic_ray")), Tp, Sc(FName(TEXT("magic_ray")), 0.15f), 1.6f);''',
          '''
					LabImpact(Gm->GetWorld(), Tp); // b139b''')
add_after('''		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Bs.P, 0.6f);''',
          '''
		{ FTuneScope Tb(GSkillPow, FName(TEXT("bolt_balls"))); LabImpact(W, Bs.P); } // b139b''')
add_after('''		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Ground, 1.2f);''',
          '''
		{ FTuneScope Tb(GSkillPow, FName(TEXT("bolt_balls"))); LabImpact(W, Ground, 0.45f); } // b139b''')
open(S, 'wb').write(b.encode('utf-8'))
# wolves: the clone's blast
C = R + 'Private/ZoogiClone.cpp'
c = open(C, 'rb').read().decode('utf-8')
o = '''	const FVector Ground(C.X, C.Y, C.Z - CloneRadius + 4.f);'''
assert c.count(o) == 1
c = c.replace(o, o + '''
	{	// b139b (Evan): the Skill Lab BLAST / BLAST SIZE picked for wolf_pack goes off where a wolf blows
		const FName Was = ZoogiSkillLab::Current(); ZoogiSkillLab::SetCurrent(FName(TEXT("wolf_pack")));
		if (const TCHAR* Bx = ZoogiSkillLab::ChosenBlast()) if (*Bx) AZoogiFX::Stylized(W, Bx, Ground, 0.35f * ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize());
		ZoogiSkillLab::SetCurrent(Was);
	}''')
open(C, 'wb').write(c.encode('utf-8'))
print('b139c ok')
