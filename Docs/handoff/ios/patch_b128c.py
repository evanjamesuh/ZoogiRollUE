# b128c (Evan 02:41 + 02:52 "yes and yes"): when Bolt Balls / Wolf Pack is one of the COMBO casts, the player must use up every bolt / wolf
# before SUPERPOWER calls the next cast. While bolts are left SUPERPOWER fires bolts (label BOLT xN!), while wolves are banked or the
# player's pack is out it runs the pack (WOLF PACK!); then CAST n/N! again. Roll ends with bolts/wolves left -> the casts keep waiting.
# The CPU follows the same order. -ZoogiComboFirst = the b120 order (next cast first).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiSkills.h'; S = 'Private/ZoogiSkills.cpp'
ed1(H, '''	ZOOGIROLL_API bool ComboReady(const AZoogiGameMode* G);''', '''	ZOOGIROLL_API bool ComboReady(const AZoogiGameMode* G);
	/** b128: this player still has bolts / banked wolves / its own pack out - the next COMBO cast waits until they're used up */
	ZOOGIROLL_API bool PowerLeft(const AZoogiGameMode* G, int32 Player);''')
ed1(S, '''		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && !Picking() && ComboCount(G, B->PlayerIndex) > 0;
	}''', '''		static const bool bComboFirst = FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboFirst"));
		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && !Picking() && ComboCount(G, B->PlayerIndex) > 0 && (bComboFirst || !PowerLeft(G, B->PlayerIndex)); // b128: bolts / wolves first
	}
	bool PowerLeft(const AZoogiGameMode* G, int32 Player)
	{
		if (!G) return false;
		if (BoltCount(G, Player) > 0 || WolfArmed(G, Player)) return true;
		const UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G);
		return WP && WP->PackOwnerIndex() == Player && (WP->IsPainting() || WP->PackActive());
	}''')
ed1('Private/ZoogiP5Command.cpp', '''			else if (G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.45f && Now >= GS.CpuRollNextAt && !ZoogiSkills::Picking() && !ZoogiRedirect::Picking())''',
'''			else if (G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.45f && Now >= GS.CpuRollNextAt && !ZoogiSkills::Picking() && !ZoogiRedirect::Picking()
				&& (FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboFirst")) || !ZoogiSkills::PowerLeft(G, Rb->PlayerIndex))) // b128: the CPU uses up its bolts / wolves first too''')
print('b128c ok')
