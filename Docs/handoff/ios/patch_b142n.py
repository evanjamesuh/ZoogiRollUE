# b142n (Evan 12:13 "Volcano meteor event blast only effects players and doesn't contribute to anyone's combo or score"): the
# volcano strikes push Zoogis only (orbs untouched) and clear the struck Zoogi's last hitter, so a fall it causes is nobody's
# knockout (no KO points, no combo for anyone). -ZoogiVolcanoHitsOrbs = the b139 blast.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		FVector Floor(const FVector& P) { return FVector(P.X, P.Y, S.FloorZ + 2.f); }''', '''		/** b142n: the volcano event - Zoogis only, nobody's credit (the struck Zoogi's last hitter is cleared) */
		int32 VolcanoBlast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up)
		{
			int32 N = 0;
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut) continue;
				const FVector D = B->BallLocation() - P; const float Dd = D.Size2D();
				if (Dd > R) continue;
				Kick(B->Sphere, D.GetSafeNormal2D() * Speed * (1.f - 0.5f * Dd / R) + FVector(0, 0, Up));
				B->LastHitPlayer = -1;
				++N;
			}
			if (N) UE_LOG(LogZoogi, Log, TEXT("VOLCANO meteor hit %d Zoogi(s) - no credit, orbs untouched"), N);
			return N;
		}
		FVector Floor(const FVector& P) { return FVector(P.X, P.Y, S.FloorZ + 2.f); }''')
ed1(S, '''FTuneScope Tn(1.f, FName(TEXT("fire_meteors"))); Blast(Gm, Hit, 260.f, 820.f, 300.f, nullptr, -1);''', '''FTuneScope Tn(1.f, FName(TEXT("fire_meteors"))); static const bool bOrbsToo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVolcanoHitsOrbs")); if (bOrbsToo) Blast(Gm, Hit, 260.f, 820.f, 300.f, nullptr, -1); else VolcanoBlast(Gm, Hit, 260.f * ZoogiSkillLab::Radius(), 820.f, 300.f);''')
print('b142n ok')
