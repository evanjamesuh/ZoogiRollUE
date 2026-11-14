# b137b (Evan 09:26: "precision functions like water cannon - remove the slow motion camera movement - you pick up to three targets and
# it shoots like water cannon"): PRECISION is a normal skill pick (tap up to 3 rivals / orbs, FIRE) - no bullet time, no missile camera;
# each target gets a water-cannon blast (same force), one after another. -ZoogiOldPrecision = the b117 missile volley.
import os
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
R = ROOT + 'Source/ZoogiRoll/'
def ed1(p, o, n, root=R):
    p = root + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''			{ TEXT("halo_disc"), EAim::Line, 0.f, 1, TEXT("SWIPE THE DISC'S PATH") } };''', '''			{ TEXT("halo_disc"), EAim::Line, 0.f, 1, TEXT("SWIPE THE DISC'S PATH") },
			{ TEXT("precision"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 TARGETS, THEN FIRE") } }; // b137b: PRECISION = 3 water-cannon shots''')
ed1(S, '''		const FDef* Find(FName Id)
		{''', '''		bool OldPrecision() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPrecision")); return b; }
		const FDef* Find(FName Id)
		{
			if (Id == FName(TEXT("precision")) && OldPrecision()) return nullptr; // b117 missile volley (not a skill pick)''')
ed1(S, '''		if (Id == FName(TEXT("precision"))) { if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(G)) { Wp->BeginPrecision(G, B, 3); bOk = true; } }''',
'''		if (Id == FName(TEXT("precision")) && OldPrecision()) { if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(G)) { Wp->BeginPrecision(G, B, 3); bOk = true; } }''')
ed1(S, '''		else if (Id == FName(TEXT("magic_rays")))''', '''		else if (Id == FName(TEXT("precision")))
		{	// b137b (Evan): up to 3 targets, a water-cannon blast at each (the water cannon's launch force), 0.2 s apart
			int32 k = 0;
			for (const TWeakObjectPtr<AActor>& WT : P.Targets)
			{
				if (!WT.IsValid()) continue;
				TWeakObjectPtr<AZoogiBall> WC = Caster;
				Later(G, 0.2f * k++, [WG, WT, WC, Pi]()
				{
					AZoogiGameMode* Gm = WG.Get(); AActor* Tt = WT.Get(); AZoogiBall* Cb = WC.Get(); if (!Gm || !Tt || !Cb) return;
					UWorld* Wq = Gm->GetWorld();
					const FVector Mq = Cb->BallLocation();
					const FVector Dir = (ActorPos(Tt) - Mq).GetSafeNormal2D();
					AZoogiFabFX::Play(Wq, FX(TEXT("NS_Water_cannon")), Mq + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_cannon")), 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation());
					AZoogiFabFX::Sound(Wq, SFX(TEXT("S_Water_cannon")), Mq, 0.8f);
					Later(Gm, 0.35f, [WG = TWeakObjectPtr<AZoogiGameMode>(Gm), WT, Dir, Pi]()
					{
						AZoogiGameMode* Gn = WG.Get(); AActor* Tu = WT.Get(); if (!Gn || !Tu) return;
						AZoogiFabFX::Play(Gn->GetWorld(), FX(TEXT("NS_Water_hit")), ActorPos(Tu) - FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_hit")), 0.4f), 3.f);
						AZoogiFabFX::Sound(Gn->GetWorld(), SFX(TEXT("S_Water_cannon_hit")), ActorPos(Tu), 0.9f);
						const float V = ZoogiConst::LaunchSpeedForPull(1.f), Undo = 1.f / FMath::Max(0.05f, GSkillPow);
						if (AZoogiBall* Bb = Cast<AZoogiBall>(Tu)) { Kick(Bb->Sphere, (Dir * V + FVector(0, 0, 120.f)) * Undo); Bb->LastHitPlayer = Pi; }
						else if (AZoogiOrb* O = Cast<AZoogiOrb>(Tu)) { Kick(O->Sphere, (Dir * V + FVector(0, 0, 100.f)) * Undo); O->LastHitBy = Pi; }
					});
				});
			}
			UE_LOG(LogZoogi, Log, TEXT("SKILL precision: P%d water cannon x%d"), Pi + 1, k);
		}
		else if (Id == FName(TEXT("magic_rays")))''')
CS = 'Private/ZoogiCardSubsystem.cpp'
ed1(CS, '''		if (K.Card.CardId == FName(TEXT("precision")) && !GZoogiComboArming && G->Balls.IsValidIndex(P) && G->Balls[P]) // b119 (Evan): PRECISION = the b117 missile volley''',
'''		if (K.Card.CardId == FName(TEXT("precision")) && !GZoogiComboArming && G->Balls.IsValidIndex(P) && G->Balls[P] && FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPrecision"))) // b119 (Evan): PRECISION = the b117 missile volley; b137b: only with -ZoogiOldPrecision''')
# the card text
p = ROOT + 'Content/Data/Cards.csv'; b = open(p, 'rb').read()
o = b'precision,PRECISION,1,Control,ActingZoogi,LeadActionEnd,0.7,1,3,true,'
i = b.index(o); j = b.index(b'\n', i); line = b[i:j]; cr = line.endswith(b'\r')
parts = line.rstrip(b'\r').split(b',', 11); parts[11] = b'Tap up to 3 rivals or orbs inside your arc range then FIRE: a water-cannon blast hits each one.'
b = b[:i] + b','.join(parts) + (b'\r' if cr else b'') + b[j:]; open(p, 'wb').write(b)
print('b137b ok')
