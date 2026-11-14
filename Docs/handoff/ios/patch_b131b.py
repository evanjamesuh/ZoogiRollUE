# b131b: the bottom-band corner banner (ARENA EVENT ...) sat on the bottom-left note stack -> the notes step aside while it shows;
# the grenade pickup is bigger and pulses a purple ring on the floor so it reads from the match camera.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
AF = 'Private/ZoogiActionFun.cpp'
ed1(AF, '''		const float Rt = GetWorld()->GetRealTimeSeconds();
		for (auto It = NoteSeen.CreateIterator(); It; ++It)''', '''		{	// b131 (Evan: "fix overlapping text and banners"): a bottom-band corner banner owns the bottom-left while it shows
			bool bBandLow = false;
			for (const FCallout& Co : Callouts) if (Co.Band == 1 && Co.Age < Co.Life) bBandLow = true;
			if (bBandLow && P5BannersOn()) Notes.Reset();
		}
		const float Rt = GetWorld()->GetRealTimeSeconds();
		for (auto It = NoteSeen.CreateIterator(); It; ++It)''')
GI = 'Private/ZoogiGrenadeItem.cpp'
ed1(GI, '''		constexpr float FlyTime = 0.9f, Size = 75.f;''', '''		constexpr float FlyTime = 0.9f, Size = 110.f;''')
ed1(GI, '''				A->SetActorLocation(D.To + FVector(0.f, 0.f, 45.f + 10.f * FMath::Sin(Now * 3.f)));''', '''				A->SetActorLocation(D.To + FVector(0.f, 0.f, 60.f + 12.f * FMath::Sin(Now * 3.f)));
				D.T += Dt;
				if (D.T > FlyTime + 1.f) { D.T = FlyTime; AZoogiFX::Spawn(W, D.To + FVector(0, 0, 4.f), Glow, 170.f, 0.8f, true, 7.f); } // b131: a ring pulse so it reads from the match camera''')
print('b131b ok')
