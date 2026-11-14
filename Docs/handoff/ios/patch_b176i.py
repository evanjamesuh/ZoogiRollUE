# b176i (Evan: "Make all skills editable in skill lab"): the 9 pack skills in the lab list; their casts honour every lab row
# (STRENGTH via GSkillPow, RADIUS on all hit shapes, FX SIZE, COLOUR, BLAST + BLAST SIZE; PULL = lift on the pack skills)
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/')
def patch(f,pairs):
    s=open(R+f,'rb').read().decode('utf-8')
    for old,new in pairs:
        done=False
        for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
            if s.count(o)==1: s=s.replace(o,n); done=True; break
        if not done: print('MISS',f,repr(old[:80])); sys.exit(1)
    open(R+f,'wb').write(s.encode('utf-8')); print('OK',f)
patch('ZoogiSkillLab.cpp',[('''TEXT("bolt_balls"), TEXT("ko_orb"), TEXT("ko_player") };''','''TEXT("bolt_balls"),
			TEXT("frost_spikes"), TEXT("glacier_lance"), TEXT("ice_shards"), TEXT("stone_pillar"), TEXT("earthquake"), TEXT("rock_rampart"), TEXT("crescent_slash"), TEXT("cross_slash"), TEXT("ground_slash"), // b176i
			TEXT("ko_orb"), TEXT("ko_player") };''')])
patch('ZoogiSkills.cpp',[
# lab tunings inside ExecPack
('''		const float Scl = Sc(Id, D->Scale);''','''		const float Scl = Sc(Id, D->Scale) * ZoogiSkillLab::FxSize(); // b176i: lab FX SIZE'''),
('''		const FPk Dd = *D;''','''		FPk Dd = *D;
		{	// b176i: lab COLOUR / RADIUS / PULL (lift)
			FLinearColor Lc; if (ZoogiSkillLab::Tint(Lc)) Dd.Col = Lc;
			Dd.R *= ZoogiSkillLab::Radius(); Dd.Up *= ZoogiSkillLab::Pull();
		}
		const TCHAR* LabBx = ZoogiSkillLab::ChosenBlast(); const float LabBs = ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize(); // b176i: lab BLAST (DEFAULT = none)'''),
('''				AZoogiFabFX::Play(W, Dd.Fx, H, Scl, 3.f);''','''				AZoogiFabFX::Play(W, Dd.Fx, H, Scl, 3.f);
				if (LabBx && *LabBx) AZoogiFX::Stylized(W, LabBx, H, 0.6f * LabBs);'''),
('''				AZoogiFabFX::Play(W, Dd.Fx, Pk, Scl, 3.f, nullptr, FVector::ZeroVector, Rot);''','''				AZoogiFabFX::Play(W, Dd.Fx, Pk, Scl, 3.f, nullptr, FVector::ZeroVector, Rot);
				if (LabBx && *LabBx) AZoogiFX::Stylized(W, LabBx, Pk, 0.5f * LabBs);'''),
# Blast() scales R by Radius() itself: hand it the un-scaled R
('''					Credit(Gm, Hb, Dd.R);
					int32 N = Blast(Gm, Hb, Dd.R, Dd.Speed, Dd.Up, WC.Get(), Pi);
					if (N == 0) N += Assist(Gm, H, FVector::ZeroVector, Dd.R * 2.5f * ZoogiSkillLab::Radius(), Dd.Speed, Dd.Up, WC.Get(), Pi);''','''					const float R0 = Dd.R / FMath::Max(0.05f, ZoogiSkillLab::RadiusOf(FName(Dd.Id))); // Blast / Credit apply the lab RADIUS themselves
					Credit(Gm, Hb, R0);
					int32 N = Blast(Gm, Hb, R0, Dd.Speed, Dd.Up, WC.Get(), Pi);
					if (N == 0) N += Assist(Gm, H, FVector::ZeroVector, Dd.R * 2.5f, Dd.Speed, Dd.Up, WC.Get(), Pi);'''),
# lab cast: attack lines through the target
('''		for (int32 k = 0; k < D->Count; ++k) S.Spots.Add(Fwd + FVector(FMath::Cos(k * 2.1f), FMath::Sin(k * 2.1f), 0.f) * 220.f);
		Execute(G, B, Id);''','''		for (int32 k = 0; k < D->Count; ++k) S.Spots.Add(Fwd + FVector(FMath::Cos(k * 2.1f), FMath::Sin(k * 2.1f), 0.f) * 220.f);
		if (D->Aim == EAim::Line && (Id == FName(TEXT("crescent_slash")) || Id == FName(TEXT("ground_slash")) || Id == FName(TEXT("rock_rampart"))))
		{	// b176i: attack lines through the rack in the lab too
			FVector Dir = (Fwd - S.Origin).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			S.A = S.Origin + Dir * 70.f; S.B = Fwd + Dir * 160.f;
		}
		Execute(G, B, Id);'''),
])
