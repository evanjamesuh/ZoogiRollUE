# b151a: Cosmos/map boss hooks for RAID mode (hold position, flinch, defeat)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
C = 'Private/ZoogiCosmosBoss.cpp'
ed1(C, '''		bool GSweepOn = false; float GSweepAng = 0.f; // b146: the hand sweep drives the peek angle''', '''		bool GSweepOn = false; float GSweepAng = 0.f; // b146: the hand sweep drives the peek angle
		bool GHold = false; float GHoldAng = 0.f; bool GDefeated = false; // b151 RAID: the boss stays where the raid puts it / sinks when beaten''')
ed1(C, '''			if (GSweepOn) { B.PeekAng = B.WantPeek = GSweepAng; B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 5.f); } // b146''', '''			if (GHold && !GSweepOn) B.WantPeek = GHoldAng; // b151 RAID: it holds its spot (moves when the raid says)
			if (GSweepOn) { B.PeekAng = B.WantPeek = GSweepAng; B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 5.f); } // b146''')
ed1(C, '''			const FVector2D Rt = RimTop(G, B.PeekAng, ER); // b142j: over the bowl wall, not behind it''', '''			if (GDefeated) { B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 1.4f, Dt, 0.35f); B.EyeBase = 0.f; B.Eye = FMath::Min(B.Eye, 0.3f); } // b151: beaten - sinks below the rim, eyes out
			const FVector2D Rt = RimTop(G, B.PeekAng, ER); // b142j: over the bowl wall, not behind it''')
ed1(C, '''	FString Name() { return GTheme.Name; }''', '''	FString Name() { return GTheme.Name; }
	float Angle() { return B.PeekAng; }
	void HoldAt(bool bOn, float AngDeg) { GHold = bOn; GHoldAng = AngDeg; if (!bOn) GDefeated = false; }
	void Flinch() { B.Eye = 10.f; B.Flicker = 0.35f; B.Slam = FMath::Max(B.Slam, 0.6f); }
	void Defeat() { GDefeated = true; B.RoarLeft = 0.f; Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.4f); }''')
ed1('Public/ZoogiCosmosBoss.h', '''	ZOOGIROLL_API FString Name();''', '''	ZOOGIROLL_API FString Name();
	/** b151 RAID */
	ZOOGIROLL_API float Angle();
	ZOOGIROLL_API void HoldAt(bool bOn, float AngDeg);
	ZOOGIROLL_API void Flinch();
	ZOOGIROLL_API void Defeat();''')
print('b151a ok')
