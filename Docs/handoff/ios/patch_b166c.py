import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n): EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            if b.count(o2) == 1: ob, nb = o2, n2
            else:
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b166c ok', len(EDITS))
# b166c (Evan Nov 6 18:29 "I want more crushing sounds hard hitting"): Zoogi-on-Zoogi hits get a crush layer that grows with the
#  impulse - HitCrush (Lyra melee body impact + SW_RockSmash crunch), and the hardest hits add a low EpicImpact boom. The HitCrack
#  body thump is louder and kicks in on more hits. -ZoogiSoftHits = the b165 sounds.
TY = 'Private/ZoogiTypes.cpp'; FE = 'Private/ZoogiFeel.cpp'; FR = 'Private/ZoogiFront.cpp'
ed1(TY, '''		Add(TEXT("HitCrack"), Seq(TEXT("Real"), TEXT("SW_BallClack"), 6), 1.0f, 0.f, Seq(TEXT("SFX"), TEXT("Lyra_MeleeImpact"), 3), 0.45f, 0.55f); // heavy hits get a body thump
''', '''		static const bool bSoftHits = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSoftHits"));
		Add(TEXT("HitCrack"), Seq(TEXT("Real"), TEXT("SW_BallClack"), 6), 1.0f, 0.f, Seq(TEXT("SFX"), TEXT("Lyra_MeleeImpact"), 3), bSoftHits ? 0.45f : 0.8f, bSoftHits ? 0.55f : 0.35f); // heavy hits get a body thump (b166: louder, on more hits)
		Add(TEXT("HitCrush"), Seq(TEXT("SFX"), TEXT("Lyra_MeleeImpact"), 3), 1.0f, 0.f, Seq(TEXT("Epic"), TEXT("SW_RockSmash"), 4), 0.7f); // b166 (Evan: "more crushing sounds hard hitting")
''')
ed1(FE, '''		ZoogiUtil::PlaySfx(Self, Name, Vol, Pitch);
		if (Impulse > 15000.f && Cast<AZoogiBall>(Other))''', '''		ZoogiUtil::PlaySfx(Self, Name, Vol, Pitch);
		static const bool bSoftHits = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSoftHits"));
		if (!bSoftHits && Cast<AZoogiBall>(Other) && Impulse > 5000.f)
		{	// b166 (Evan: "I want more crushing sounds hard hitting"): a crunch that grows with the hit, a low boom on the hardest
			const float Kc = FMath::Clamp((Impulse - 5000.f) / 11000.f, 0.f, 1.f);
			ZoogiUtil::PlaySfx(Self, TEXT("HitCrush"), FMath::Lerp(0.45f, 1.f, Kc), FMath::Lerp(0.95f, 0.72f, Kc) * FMath::FRandRange(0.96f, 1.04f));
			if (Impulse > 11000.f) ZoogiUtil::PlaySfx(Self, TEXT("EpicImpact"), FMath::Lerp(0.35f, 0.75f, FMath::Clamp((Impulse - 11000.f) / 9000.f, 0.f, 1.f)), FMath::FRandRange(0.78f, 0.88f));
		}
		if (Impulse > 15000.f && Cast<AZoogiBall>(Other))''')
ed1(FR, '\t\t\t{ TEXT("B166"), TEXT("STORY 2V2 FIGHTS GET THE NEW CONTROLS"),', '\t\t\t{ TEXT("B166"), TEXT("HARDER HITS"), TEXT("Zoogi-on-Zoogi hits now crunch: a heavy body thump and a rock-smash crack that grow with the hit, and a deep boom on the biggest slams.") },\n\t\t\t{ TEXT("B166"), TEXT("STORY 2V2 FIGHTS GET THE NEW CONTROLS"),')
run()
