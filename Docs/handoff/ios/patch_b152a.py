# b152a (Evan 23:17 "Bolt ball targeting should always be ground level so when players use it in air they shoot down to the target and
# stay in air"): a bolt fired from the air now flies DOWN at its target on the ground (a straight 3-D line to the rival / orb, or to the
# floor ahead), blasts where it lands, and the caster keeps flying (nothing touches its flight). Hits are tested along the 3-D path, so
# a bolt passing high over an orb no longer sets it off. -ZoogiFlatBolts = the old flat bolts.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''	struct FBoltShot { TWeakObjectPtr<AActor> Vis; FVector P = FVector::ZeroVector, D = FVector::ForwardVector; float Age = 0.f, Spark = 0.f; int32 Owner = -1; };''', '''	struct FBoltShot { TWeakObjectPtr<AActor> Vis; FVector P = FVector::ZeroVector, D = FVector::ForwardVector; float Age = 0.f, Spark = 0.f; int32 Owner = -1; float GroundZ = -1e9f; bool b3D = false; }; // b152a: GroundZ / b3D - aimed down at the floor''')
ed1(S, '''		FBoltShot Bs; Bs.P = bOldAim ? From + Dir * (ZoogiConst::ZoogiRadius + 20.f) : From; Bs.D = Dir; Bs.Owner = B->PlayerIndex; // b128: from the Zoogi's centre (the path check covers the first metre)''', '''		FBoltShot Bs; Bs.P = bOldAim ? From + Dir * (ZoogiConst::ZoogiRadius + 20.f) : From; Bs.D = Dir; Bs.Owner = B->PlayerIndex; // b128: from the Zoogi's centre (the path check covers the first metre)
		static const bool bFlat = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatBolts"));
		if (!bFlat && !bOldAim)
		{	// b152a (Evan): always at ground level - from the air the bolt flies down to its target on the floor
			FVector T; float Gz;
			if (Best)
			{
				T = Best->GetActorLocation();
				if (const AZoogiBall* Tb = Cast<AZoogiBall>(Best)) { T = Tb->BallLocation(); Gz = T.Z - ZoogiConst::ZoogiRadius; }
				else if (const AZoogiOrb* To = Cast<AZoogiOrb>(Best)) { T = To->OrbLocation(); Gz = T.Z - ZoogiConst::OrbRadius; }
				else Gz = T.Z - 50.f;
			}
			else
			{	// nothing to aim at: the floor ~10 m ahead
				const FVector Ahead = From + Dir * 1000.f;
				FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiBoltFloor), false); Q.AddIgnoredActor(B);
				Gz = W->LineTraceSingleByChannel(H, Ahead + FVector(0, 0, 300.f), Ahead - FVector(0, 0, 6000.f), ECC_WorldStatic, Q) ? (float)H.ImpactPoint.Z : (float)(From.Z - ZoogiConst::ZoogiRadius);
				T = FVector(Ahead.X, Ahead.Y, Gz + ZoogiConst::ZoogiRadius);
			}
			const FVector D3 = (T - From).GetSafeNormal();
			if (!D3.IsNearlyZero() && From.Z - T.Z > 60.f) { Bs.D = D3; Bs.GroundZ = Gz; Bs.b3D = true; UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d fires from %.0f cm up - down at the target on the floor"), B->PlayerIndex + 1, From.Z - Gz); }
		}''')
ed1(S, '''			auto Near = [&](const FVector& Q, float Rr) { const FVector2D A(Prev), Bp(Bs.P), Qq(Q); return FMath::PointDistToSegment(FVector(Qq, 0.f), FVector(A, 0.f), FVector(Bp, 0.f)) < Rr; };''', '''			auto Near = [&](const FVector& Q, float Rr) { if (Bs.b3D) return FMath::PointDistToSegment(Q, Prev, Bs.P) < Rr; const FVector2D A(Prev), Bp(Bs.P), Qq(Q); return FMath::PointDistToSegment(FVector(Qq, 0.f), FVector(A, 0.f), FVector(Bp, 0.f)) < Rr; }; // b152a: a downward bolt tests its real path''')
ed1(S, '''			if (!bBoom && (Bs.Age > 1.4f || FVector::Dist2D(Bs.P, Ctr) > KR + 150.f)) bBoom = true;''', '''			if (!bBoom && Bs.b3D && Bs.P.Z <= Bs.GroundZ + 15.f) { Bs.P.Z = Bs.GroundZ + 15.f; bBoom = true; } // b152a: it reached the floor - blast there
			if (!bBoom && (Bs.Age > 1.4f || FVector::Dist2D(Bs.P, Ctr) > KR + 150.f)) bBoom = true;''')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B151"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B152"), TEXT("BOLT BALLS FROM THE AIR"), TEXT("Fire Bolt Balls while you are flying and each bolt shoots DOWN at its target on the ground (or the floor ahead) and blasts where it lands - you keep flying.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o))
print('b152a ok')
