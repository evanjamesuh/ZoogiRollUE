# b140b (Evan 10:50 + screenshot 10:59: "Remove the unfinished aerial ramps circled here and the pole floating in air"): the two
# curved ice slide tracks in the Frozen Zone 2 sky cluster (AZoogiAerialDecor::Build Slide) and their hanger poles are no longer
# built. Visual only (no collision ever). -ZoogiAerialSlides = back.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiAerialProps.cpp', '''	Slide(244.f, 300.f, 1880.f, 1760.f, 830.f, 600.f, 0.f);
	Slide(296.f, 254.f, 2150.f, 2100.f, 470.f, 360.f, 90.f);''', '''	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiAerialSlides"))) // b140 (Evan: "remove the unfinished aerial ramps ... and the pole floating in air")
	{
		Slide(244.f, 300.f, 1880.f, 1760.f, 830.f, 600.f, 0.f);
		Slide(296.f, 254.f, 2150.f, 2100.f, 470.f, 360.f, 90.f);
	}
	else UE_LOG(LogZoogi, Log, TEXT("BATTLE aerial decor: unfinished slide tracks + hanger poles removed (b140; -ZoogiAerialSlides = back)"));''')
F = 'Private/ZoogiFront.cpp'
ed1(F, '''			{ TEXT("B139"), TEXT("THE VOLCANO ERUPTS")''', '''			{ TEXT("B140"), TEXT("KO BUZZ, KO BLAST ON THE SPOT"), TEXT("Every knock-out now buzzes the phone for 2 seconds. The knock-out blast goes off right where the Zoogi was knocked out (it used to pop on the court edge). Frozen: the unfinished curved sky ramps and their floating poles are gone.") },
			{ TEXT("B139"), TEXT("THE VOLCANO ERUPTS")''')
print('b140b ok')
