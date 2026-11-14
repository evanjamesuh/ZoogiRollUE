# b144c (Evan 13:31 "Camera gets stuck at high elevations how can we fix this and make it a fun part of the game action" -> SKY DIVE cam)
# 1) the stuck camera: CourtZ (the floor the match camera keeps above) followed the turn Zoogi up into the air; on the way down the Zoogi
#    read as "dropped below the floor", so CourtZ froze at the top of the flight - and every Zoogi on the real floor after that read as
#    fallen too, so it never came down. CourtZ now only follows a Zoogi that is ON a floor, and a grounded Zoogi on the court resets it.
# 2) SKY DIVE: new ZoogiSkyDiveCam (swoop above + slow-mo + SKY HIGH!, ride the fall down, landing thump)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
PC = 'Private/ZoogiPlayerController.cpp'
ed1(PC, '''	const bool bShooterLeaving = !Shooter || (!bPadView && !bOnLaunchPad && IsLeavingPlay(SL, Shooter->bOut, CourtZ, G->Arena));''', '''	// b144c (Evan: "Camera gets stuck at high elevations"): CourtZ follows the turn Zoogi only while it is ON a floor (never up into the
	// air - the fall back read as "below the floor" and froze CourtZ at the top of the flight); a grounded Zoogi on the court resets it
	const bool bShooterOnFloor = Shooter && !Shooter->bOut && Shooter->Sphere && SL.Z > -200.f && ZoogiPhysics::IsGrounded(Shooter->Sphere, ZoogiConst::ZoogiRadius);
	if (bShooterOnFloor && !bPadView && !bOnLaunchPad && SL.Z < CourtZ - 1.5f * ZoogiConst::ZoogiRadius && !IsOffPlayArea(G->Arena, FVector2D(SL.X, SL.Y)))
	{
		UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH court height %.0f was stuck over P%d on the floor at %.0f -> reset"), CourtZ, Shooter->PlayerIndex + 1, SL.Z);
		CourtZ = SL.Z; ++CamWatchFixes;
	}
	const bool bShooterLeaving = !Shooter || (!bPadView && !bOnLaunchPad && IsLeavingPlay(SL, Shooter->bOut, CourtZ, G->Arena));''')
ed1(PC, '''	if (!bShooterLeaving && SL.Z > -200.f) CourtZ = SL.Z;''', '''	if (!bShooterLeaving && SL.Z > -200.f && (bShooterOnFloor || bPadView || bOnLaunchPad || SL.Z < CourtZ)) CourtZ = SL.Z; // b144c''')
ed1(PC, '''		const bool bOwner = bSpecialV || ZoogiEdgeHang::Active() || ZoogiArcCam::Active() || G->bCinematicCam''', '''		const bool bOwner = bSpecialV || ZoogiEdgeHang::Active() || ZoogiArcCam::Active() || ZoogiSkyDiveCam::Active() || G->bCinematicCam''')
ed1(PC, '#include "ZoogiArcCam.h"\n', '#include "ZoogiArcCam.h"\n#include "ZoogiSkyDiveCam.h"\n#include "ZoogiPhysics.h"\n#include "Components/SphereComponent.h"\n')
AF = 'Private/ZoogiActionFun.cpp'
ed1(AF, '''	ZoogiArcCam::Tick(G, Dt); // b59: ARC launch flight camera
''', '''	ZoogiArcCam::Tick(G, Dt); // b59: ARC launch flight camera
	ZoogiSkyDiveCam::Tick(G, RealDt); // b144c (Evan): SKY DIVE - a Zoogi thrown high gets a swoop + slow-mo + ride-down camera
''')
ed1(AF, '''			else if (bFast && A && A->bCPU''', '''			else if (ZoogiSkyDiveCam::SlowMo() > 0.f) Want = ZoogiSkyDiveCam::SlowMo(); // b144c: SKY HIGH! swoop
			else if (bFast && A && A->bCPU''')
b = open(R + AF, 'rb').read()
inc = b'#include "ZoogiArcCam.h"'
assert b.count(inc) >= 1
i = b.index(inc); j = b.index(b'\n', i)
b = b[:j+1] + b'#include "ZoogiSkyDiveCam.h"' + (b'\r\n' if b[j-1:j] == b'\r' else b'\n') + b[j+1:]
open(R + AF, 'wb').write(b)
p = os.path.expanduser('~/mnt/dev/ios/p5run.py'); s = open(p).read()
o = "            ('item_test',"
assert s.count(o) == 1
s = s.replace(o, "            ('skydive_test', 1600, 900, 70, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiSkyDiveTest -ZoogiArena=3 -ZoogiPlayers=2 -ZoogiRounds=2'),\n" + o)
open(p, 'w').write(s)
print('b144c ok')
