# b155a (Evan Nov 6 06:54 "Show arena boss only in raids after the classic arena form shot, in classic start with arena forming
# shot, show boss at point throughout the match where it makes sense"): the titan stays below the rim (out of sight) unless
#  - RAID: always, once the arena-forming intro is done (it rises and roars = the reveal)
#  - CLASSIC: its attack rounds (warning -> attack, the closing rounds), and a 5 s cameo after a knockout / combo
#  Never during the arena-forming shot. -ZoogiBossAlways = the b148 always-peeking boss.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
A = 'Private/ZoogiBossAttack.cpp'
ed1(A, '''		if (S.bPending) { S.Pulse -= RealDt; if (S.Pulse <= 0.f) { S.Pulse = 0.9f; DrawZone(G); } }
	}
}''', '''		if (S.bPending) { S.Pulse -= RealDt; if (S.Pulse <= 0.f) { S.Pulse = 0.9f; DrawZone(G); } }
	}

	bool Engaged(const AZoogiGameMode* G)
	{	// b155a: this round is a boss-attack round (announced / running / just landed)
		if (!G) return false;
		if (S.bPending || S.Run != ERun::Idle) return true;
		return AttackRound(G) && S.Round == G->RoundNumber;
	}
}''')
ed1('Public/ZoogiBossAttack.h' if os.path.exists(R + 'Public/ZoogiBossAttack.h') else 'Private/ZoogiBossAttack.h', '''	ZOOGIROLL_API bool CamActive();''', '''	ZOOGIROLL_API bool CamActive();
	/** b155a: the current round is one of the boss's attack rounds (the boss should be in view) */
	ZOOGIROLL_API bool Engaged(const AZoogiGameMode* G);''')
C = 'Private/ZoogiCosmosBoss.cpp'
ed1(C, '''#include "ZoogiAurexPuppet.h"''', '''#include "ZoogiAurexPuppet.h"
#include "ZoogiBossAttack.h"
#include "ZoogiRaid.h"''')
ed1(C, '''		FLinearColor EyeRed(1.f, 0.12f, 0.08f); // b148a: the map boss's eye colour (set by Spawn)''', '''		FLinearColor EyeRed(1.f, 0.12f, 0.08f); // b148a: the map boss's eye colour (set by Spawn)
		float GCameo = 0.f; bool GWasShown = false; // b155a: a short appearance after a KO / combo
		bool Shown(const AZoogiGameMode* G)
		{	// b155a (Evan): out of sight during the arena-forming shot; RAID = always after it; CLASSIC = attack rounds + cameos
			static const bool bAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossAlways"));
			if (!G) return false;
			if (G->Arena && G->Arena->IsForming()) return false;
			if (bAlways || ZoogiRaid::Active()) return true;
			return ZoogiBossAttack::Engaged(G) || GCameo > 0.f;
		}''')
ed1(C, '''			if (GDefeated) { B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 1.4f, Dt, 0.35f); B.EyeBase = 0.f; B.Eye = FMath::Min(B.Eye, 0.3f); } // b151: beaten - sinks below the rim, eyes out''', '''			GCameo = FMath::Max(0.f, GCameo - Dt);
			const bool bShow = GSweepOn || Shown(G);
			if (!GDefeated && !bShow) { B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 1.4f, Dt, 0.8f); B.CamWant = 0.f; B.RoarLeft = 0.f; } // b155a: below the rim, out of sight
			else if (!GDefeated && !GWasShown && B.Dip > 0.9f) { B.RoarLeft = 1.8f; B.Eye = 8.f; UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS shows itself (%s)"), ZoogiRaid::Active() ? TEXT("raid") : (GCameo > 0.f ? TEXT("cameo") : TEXT("attack round"))); }
			if (!GDefeated && bShow && B.Dip > 0.3f && !B.bMoving) B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 1.6f); // rises quickly into view
			GWasShown = bShow;
			if (GDefeated) { B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 1.4f, Dt, 0.35f); B.EyeBase = 0.f; B.Eye = FMath::Min(B.Eye, 0.3f); } // b151: beaten - sinks below the rim, eyes out''')
ed1(C, '''		if (Ev == TEXT("KO"))
		{	// a slow power gesture, eyes blaze, rumble''', '''		if (Ev == TEXT("KO") || Ev == TEXT("Combo")) GCameo = FMath::Max(GCameo, 5.f); // b155a: comes up to watch
		if (Ev == TEXT("KO"))
		{	// a slow power gesture, eyes blaze, rumble''')
ed1(C, '''		GHold = false; GDefeated = false; GSweepOn = false; // b151''', '''		GHold = false; GDefeated = false; GSweepOn = false; // b151
		GCameo = 0.f; GWasShown = false; B.Dip = 1.4f; // b155a: starts out of sight (the arena forms first)''')
print('b155a ok')
