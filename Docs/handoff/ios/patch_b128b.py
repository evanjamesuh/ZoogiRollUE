# b128b (Evan 02:50: "if an orb is right next to the player bolt ball doesn't see it or hit it - optimize bolt balls"):
#  - target = the closest thing in reach, rivals favoured (distance x0.7), not "any rival anywhere first" (a far rival beat an orb touching you)
#  - the shot starts at the Zoogi's centre and checks its whole path each frame (swept), so an orb right beside the Zoogi is hit, never skipped
#  -ZoogiOldBoltAim = the b80 aim (nearest rival first). A hard target lock still wins.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::DistSquared2D(O->BallLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }''',
'''		static const bool bOldAim = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoltAim"));
		if (bOldAim)
		{
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::DistSquared2D(O->BallLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
			if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		}
		else
		{	// b128 (Evan: "an orb right next to the player - bolt balls doesn't see it"): the closest thing in reach, rivals favoured
			const float Reach = FMath::Max(900.f, G->ArcRange());
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::Dist2D(O->BallLocation(), From); if (D < Reach && D * 0.7f < BestD) { BestD = D * 0.7f; Best = O; } }
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), From); if (D < Reach && D < BestD) { BestD = D; Best = O; } }
			if (!Best) for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::Dist2D(O->BallLocation(), From); if (D < BestD) { BestD = D; Best = O; } } // nothing in reach: the nearest rival
		}''')
ed1(S, '''		FBoltShot Bs; Bs.P = From + Dir * (ZoogiConst::ZoogiRadius + 20.f); Bs.D = Dir; Bs.Owner = B->PlayerIndex;''',
'''		FBoltShot Bs; Bs.P = bOldAim ? From + Dir * (ZoogiConst::ZoogiRadius + 20.f) : From; Bs.D = Dir; Bs.Owner = B->PlayerIndex; // b128: from the Zoogi's centre (the path check covers the first metre)''')
ed1(S, '''			Bs.P += Bs.D * 2600.f * Dt;
			if (AActor* V = Bs.Vis.Get()) V->SetActorLocation(Bs.P);''', '''			const FVector Prev = Bs.P;
			Bs.P += Bs.D * 2600.f * Dt;
			if (AActor* V = Bs.Vis.Get()) V->SetActorLocation(Bs.P);
			// b128: swept - anything the bolt passed this frame counts (an orb touching the Zoogi was skipped by the start offset / a long frame)
			auto Near = [&](const FVector& Q, float Rr) { const FVector2D A(Prev), Bp(Bs.P), Qq(Q); return FMath::PointDistToSegment(FVector(Qq, 0.f), FVector(A, 0.f), FVector(Bp, 0.f)) < Rr; };''')
ed1(S, '''			for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex != Bs.Owner && !O->bOut && FVector::Dist2D(O->BallLocation(), Bs.P) < ZoogiConst::ZoogiRadius + 40.f) { Hit = O; bBoom = true; break; }
			if (!bBoom) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), Bs.P) < 70.f) { bBoom = true; break; }''',
'''			for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex != Bs.Owner && !O->bOut && Near(O->BallLocation(), ZoogiConst::ZoogiRadius + 40.f)) { Hit = O; bBoom = true; Bs.P = O->BallLocation(); break; }
			if (!bBoom) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Near(O->OrbLocation(), ZoogiConst::OrbRadius + 45.f)) { bBoom = true; Bs.P = FVector(O->OrbLocation().X, O->OrbLocation().Y, Bs.P.Z); break; }''')
print('b128b ok')
