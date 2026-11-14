# b142l: the armed Awaken follows the equipped ZOOGI (ball) like the AWAKEN power does (ZoogiFront::AwakenFor) - the Hotstreak ball =
# METEOR STRIKE rain + the MeteorStrike anime, whatever robot rolls it.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '''	const ZoogiFront::FAwaken* Aw = ZoogiFront::AwakenFor(B, G);
	if (GArmKey == TEXT("MeteorStrike") || !Aw || !Aw->Skill || !*Aw->Skill)''', '''	const ZoogiFront::FAwaken* Aw = ZoogiFront::AwakenFor(B, G);
	if (Aw) GArmKey = (!Aw->Skill || !*Aw->Skill) ? FString(TEXT("MeteorStrike")) : FCString::Strcmp(Aw->Skill, TEXT("wolf_pack")) == 0 ? FString(TEXT("WolfPack")) : FCString::Strcmp(Aw->Skill, TEXT("bolt_balls")) == 0 ? FString(TEXT("BoltBalls")) : FString(Aw->Skill);
	if (GArmKey == TEXT("MeteorStrike") || !Aw || !Aw->Skill || !*Aw->Skill)''')
print('b142l ok')
