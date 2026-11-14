# b147a (Evan 17:20, the boss design confirmed in full): what b146 left out
#  - BRACE -> EDGE HANG: a human Zoogi braced against the sweep is knocked just over the rim and grabs it - the b58 tap-to-pull-up
#    mini-game, free (ZoogiEdgeHang::Force; the Cosmos zero-g fly-out waits for it)
#  - POINTS FOR THE BRAVE: a boss ring-out of a rival / orb scores for whoever last hit it during the warning round
#  - CPUs DODGE: in a pending zone a CPU rolls out of it (toward the centre / away from the slam) or GUARDs the slam; 3 in 10 go
#    on the attack instead
#  - readability: a boss-attack camera frames the zone while it lands; brighter zone pulses
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
# ---- edge hang force
EH = 'Private/ZoogiEdgeHang.cpp'
ed1(EH, '''	bool HangVisualOnly() { static const bool bGame = Flag(TEXT("ZoogiEdgeHangGame")); return !bGame; }''', '''	// b147a: a Zoogi braced against the boss sweep gets the b58 mini-game (free) for a few seconds
	TMap<TWeakObjectPtr<AZoogiBall>, double> GForce; bool GForceNow = false;
	bool ForcedB(const AZoogiBall* B) { const double* T = B ? GForce.Find(B) : nullptr; return T && FPlatformTime::Seconds() < *T; }
	bool HangVisualOnly() { static const bool bGame = Flag(TEXT("ZoogiEdgeHangGame")); return !bGame && !GForceNow; }''')
ed1(EH, '''		GHangS.bCards = G->CardsOn() && CS && !HangVisualOnly(); // b67: no SP - it always falls''', '''		const bool bForceB = ForcedB(B) && bHuman; // b147a
		if (bForceB) GForceNow = true;
		GHangS.bCards = G->CardsOn() && CS && !HangVisualOnly() && !bForceB; // b67: no SP - it always falls; b147a: the boss-brace hang is free''')
ed1(EH, '''	void Finish(AZoogiGameMode* G, bool bSaved)
	{''', '''	void Finish(AZoogiGameMode* G, bool bSaved)
	{
		if (GForceNow) { GForceNow = false; if (AZoogiBall* Fb = GHangS.Ball.Get()) GForce.Remove(Fb); } // b147a''')
ed1(EH, '''	bool Active() { return GHangS.V.bOn; }''', '''	bool Active() { return GHangS.V.bOn; }
	void Force(AZoogiBall* B, float Seconds) { if (B) GForce.Add(B, FPlatformTime::Seconds() + Seconds); }
	bool Forced(const AZoogiBall* B) { return ForcedB(B); }''')
ed1('Public/ZoogiEdgeHang.h', '''	ZOOGIROLL_API bool Track(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);''', '''	ZOOGIROLL_API bool Track(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);
	/** b147a: this Zoogi gets the tap-to-pull-up mini-game (free) if it drops off the edge in the next Seconds (the boss-sweep BRACE). */
	ZOOGIROLL_API void Force(AZoogiBall* B, float Seconds);
	ZOOGIROLL_API bool Forced(const AZoogiBall* B);''')
GM = 'Private/ZoogiGameMode.cpp'
ed1(GM, '''		const bool bZeroG = ZoogiArenaReact::CosmicEdgeOut(this, B, L);''', '''		const bool bZeroG = !ZoogiEdgeHang::Forced(B) && ZoogiArenaReact::CosmicEdgeOut(this, B, L); // b147a: a braced Zoogi grabs the rim first''')
# ---- CPU dodge hook
ed1(GM, '''	if (IsOnLaunchPad(Me)) { const AZoogiLaunchPad* LP = Arena ? Arena->GetLaunchPad() : nullptr;''', '''	{	// b147a (Evan: "CPU players dodge with the same rules"): out of the boss's red zone
		FVector2D DodgeDir; float DodgeRun = 0.f;
		if (!IsOnLaunchPad(Me) && ZoogiBossAttack::CpuDodge(this, Me, DodgeDir, DodgeRun))
		{
			BestDir = DodgeDir; CPULock = nullptr;
			BestFrac = FMath::Clamp(InvertLaunch(FMath::Sqrt(2.f * Decel * FMath::Max(0.f, DodgeRun))), 0.25f, 0.9f);
		}
	}
	if (IsOnLaunchPad(Me)) { const AZoogiLaunchPad* LP = Arena ? Arena->GetLaunchPad() : nullptr;''')
ed1('Public/ZoogiBossAttack.h', '''	ZOOGIROLL_API float SlowMo();''', '''	ZOOGIROLL_API float SlowMo();
	/** b147a: a CPU in a pending zone - true = roll OutRun cm along OutDir instead of its plan (it may GUARD the slam and keep its plan). */
	ZOOGIROLL_API bool CpuDodge(AZoogiGameMode* G, class AZoogiBall* Me, FVector2D& OutDir, float& OutRun);
	/** b147a: the boss-attack camera is on */
	ZOOGIROLL_API bool CamActive();''')
PC = 'Private/ZoogiPlayerController.cpp'
ed1(PC, '''|| ZoogiSkyDiveCam::Active() || G->bCinematicCam''', '''|| ZoogiSkyDiveCam::Active() || ZoogiBossAttack::CamActive() || G->bCinematicCam''')
ed1(PC, '#include "ZoogiSkyDiveCam.h"', '#include "ZoogiSkyDiveCam.h"\n#include "ZoogiBossAttack.h"')
print('b147a ok')
