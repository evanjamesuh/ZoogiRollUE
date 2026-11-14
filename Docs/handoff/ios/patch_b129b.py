# b129b (Evan 02:59: "stop wolves from moving the player if they are called back - it causes accidental self knockouts"):
#  - a recalled wolf no longer collides with any Zoogi (it dashed at 2300 cm/s straight into its own caster)
#  - the recalled wolf's pop beside the caster no longer blasts the orbs around it (they were shoved into the caster)
#  -ZoogiOldRecall = the b95 recall.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
C = 'Private/ZoogiClone.cpp'
ed1(C, '''	Recall = To;
	Age = FMath::Min(Age, 1.0f);''', '''	Recall = To;
	static const bool bOldRecall = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRecall"));
	if (!bOldRecall && Sphere) Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // b129: runs home through Zoogis - never shoves its caster
	Age = FMath::Min(Age, 1.0f);''')
ed1(C, '''		// radial blast on the field orbs: strongest at the centre, planar push + a small hop, credited to the caster
		for (AZoogiOrb* O : G->Orbs)
		{''', '''		// radial blast on the field orbs: strongest at the centre, planar push + a small hop, credited to the caster
		// b129: a recalled wolf's pop beside its caster pushes nothing (the orbs it shoved knocked the caster off)
		const bool bQuietPop = Recall.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRecall"));
		if (!bQuietPop) for (AZoogiOrb* O : G->Orbs)
		{''')
print('b129b ok')
