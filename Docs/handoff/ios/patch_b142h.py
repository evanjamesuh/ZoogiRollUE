# b142h: the water cannon numbers from the b142c measuring pass (wc_test, WC PROBE): the system's water ball flies along its local
# side axis - yaw offset 0 sent it off to the right (bounds -880 cm sideways), +90 sends it straight down the cast (+500..+1400 cm).
# It forms for ~1.2 s and then crosses ~14 m in under 0.25 s, so the hit now lands at 1.2 s + distance / 5000 cm/s (was 0.35 s,
# before the ball had even left).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''constexpr float WcYawDefault = 0.f, WcHitDefault = 0.35f;''', '''constexpr float WcYawDefault = 90.f, WcHitDefault = 1.2f; // b142h: measured (patch_b142h)''')
ed1(S, '''				UE_LOG(LogZoogi, Log, TEXT("SKILL water_cannon: yaw offset %.0f, target %.0f cm away, hit at %.2f s"), WcYaw(), FVector::Dist2D(Tp, Me), WcHitT());
				Later(G, WcHitT(), [WG, WT, Dir, Pi]()''', '''				const float WcAt = WcHitT() + FVector::Dist2D(Tp, Me) / 5000.f; // b142h: when the water ball gets there
				UE_LOG(LogZoogi, Log, TEXT("SKILL water_cannon: yaw offset %.0f, target %.0f cm away, hit at %.2f s"), WcYaw(), FVector::Dist2D(Tp, Me), WcAt);
				Later(G, WcAt, [WG, WT, Dir, Pi]()''')
ed1(S, '''					Later(Gm, WcHitT(),''', '''					Later(Gm, WcHitT() + FVector::Dist2D(ActorPos(Tt), Mq) / 5000.f,''')
print('b142h ok')
