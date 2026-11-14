# b176e: the CPU swipes the attack-line skills THROUGH its target (not a backstop wall like Ice Wall)
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiSkills.cpp')
s=open(R,'rb').read().decode('utf-8')
old='''			S.CpuA = ClampRange(Ctr + Perp * 280.f); S.CpuB = ClampRange(Ctr - Perp * 280.f);
		}'''
new='''			S.CpuA = ClampRange(Ctr + Perp * 280.f); S.CpuB = ClampRange(Ctr - Perp * 280.f);
		}
		if (D->Aim == EAim::Line && (Id == FName(TEXT("crescent_slash")) || Id == FName(TEXT("ground_slash")) || Id == FName(TEXT("rock_rampart"))))
		{	// b176e: attack lines run from the caster through the target
			FVector Dir = (Fwd - S.Origin).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			S.CpuA = ClampRange(S.Origin + Dir * 70.f); S.CpuB = ClampRange(Fwd + Dir * 160.f);
		}'''
for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
    if s.count(o)==1: s=s.replace(o,n); open(R,'wb').write(s.encode('utf-8')); print('OK'); break
else: print('MISS'); sys.exit(1)
