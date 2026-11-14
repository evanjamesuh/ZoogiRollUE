# b150a (Evan 21:47 "Delete the left over arms from old models on all characters"): the old character models' flipper arms ("nubs",
# SM_<Name>_Nubs) rode along every Zoogi - hidden under the ball skins at spawn, but the drop-in / unhide paths switched them back on
# after launch. They are never loaded now (no mesh = nothing to show on any path). -ZoogiNubs = back.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiBall.cpp', '''	// The nubs share Visual's transform (same pivot / scale), so they roll, squash and wobble with the shell.
	Nubs->SetStaticMesh(NubMesh);''', '''	// The nubs share Visual's transform (same pivot / scale), so they roll, squash and wobble with the shell.
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNubs"))) NubMesh = nullptr; // b150a (Evan): "delete the left over arms from old models on all characters"
	Nubs->SetStaticMesh(NubMesh);''')
ed1('Private/ZoogiTypes.cpp', '''		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNubs"))) bShow = true;''', '''		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNubs"))) bShow = true;
		else bShow = false; // b150a: the old flipper arms are gone''')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B149"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B150"), TEXT("OLD ARMS REMOVED"), TEXT("The little leftover arms from the old character models no longer show up around any Zoogi (they used to reappear after launch).") },\r\n'
open(p, 'wb').write(b.replace(o, n + o))
print('b150a ok')
