import os,sys
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
I=os.path.expanduser('~/mnt/dev/ios/')
defs=open(I+'b178_defs.txt').read(); pks=open(I+'b178_pks.txt').read(); csv=open(I+'b178_csv.txt').read()
lines=open(I+'b178_lines.txt').read().split(); ids=open(I+'b178_ids.txt').read().split()
f=R+'Source/ZoogiRoll/Private/ZoogiSkills.cpp'; s=open(f,'rb').read().decode('utf-8')
def ed(o,n,cnt=1):
    global s
    assert s.count(o)==cnt,(o[:70],s.count(o)); s=s.replace(o,n)
ed('''			{ TEXT("ground_slash"), EAim::Line, 0.f, 1, TEXT("SWIPE THE WAVE'S PATH") } };''','''			{ TEXT("ground_slash"), EAim::Line, 0.f, 1, TEXT("SWIPE THE WAVE'S PATH") },
			// b178a (Evan: "add all earth bending effects as skills"): the rest of the Earthbender pack
'''+defs+' };')
ed('''NS_GroundSlash03.NS_GroundSlash03"), 2.0f, 4, 170.f, 1100.f, 600.f, 0.15f, FLinearColor(0.65f, 0.35f, 1.f) } };''','''NS_GroundSlash03.NS_GroundSlash03"), 2.0f, 4, 170.f, 1100.f, 600.f, 0.15f, FLinearColor(0.65f, 0.35f, 1.f) },
			// b178a: the rest of the Earthbender pack
'''+pks+' };')
L=['crescent_slash','ground_slash','rock_rampart']+lines
ed('''	static bool ExecPack(AZoogiGameMode* G, AZoogiBall* Caster, FName Id, const FPick& P, float FZ)''','''	// b178a: the swipe skills that are attack lines (cast from the caster through the target, not a backstop wall)
	bool IsPackLine(FName Id)
	{
		static const TSet<FName> L = { '''+', '.join('FName(TEXT("%s"))'%x for x in L)+''' };
		return L.Contains(Id);
	}
	static bool ExecPack(AZoogiGameMode* G, AZoogiBall* Caster, FName Id, const FPick& P, float FZ)''')
ed('''(Id == FName(TEXT("crescent_slash")) || Id == FName(TEXT("ground_slash")) || Id == FName(TEXT("rock_rampart")))''','''IsPackLine(Id)''',3)
open(f,'wb').write(s.encode('utf-8')); print('skills ok')
f=R+'Source/ZoogiRoll/Private/ZoogiSkillLab.cpp'; s=open(f,'rb').read().decode('utf-8')
o='''TEXT("ground_slash"), // b176i'''
assert s.count(o)==1
s=s.replace(o,o+'\n\t\t\t'+', '.join('TEXT("%s")'%x for x in ids)+', // b178a Earthbender')
open(f,'wb').write(s.encode('utf-8')); print('lab ok')
f=R+'Content/Data/Cards.csv'; s=open(f,'rb').read().decode('utf-8'); nl='\r\n' if '\r\n' in s else '\n'
if 'boulder_block,' not in s:
    s=s.rstrip('\r\n')+nl+csv.replace('\n',nl)+nl; open(f,'wb').write(s.encode('utf-8')); print('csv ok')
