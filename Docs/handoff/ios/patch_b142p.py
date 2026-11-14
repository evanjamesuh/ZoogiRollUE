# b142p: the bowl wall has no Visibility collision (it is a camera blocker: QueryOnly, Camera = Block) - the rim traces use the
# Camera channel; fallback = the SM_BowlRing bounds top, 2.5 m past the court edge.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
B = 'Private/ZoogiCosmosBoss.cpp'
ed1(B, '''P + FVector(0.f, 0.f, 4000.f), P - FVector(0.f, 0.f, 600.f), ECC_Visibility, Q)) Hits.Add''', '''P + FVector(0.f, 0.f, 4000.f), P - FVector(0.f, 0.f, 600.f), ECC_Camera, Q)) Hits.Add''')
ed1(B, '''			if (BestZ < C.Z + 60.f) { BestZ = C.Z; BestR = ER; } // nothing stands there: the court lip''', '''			if (BestZ < C.Z + 60.f && G->Arena)
			{	// fallback: the R2 bowl wall's bounds
				TInlineComponentArray<UStaticMeshComponent*> Sms(G->Arena);
				for (UStaticMeshComponent* Sm : Sms) if (Sm && Sm->GetStaticMesh() && Sm->GetStaticMesh()->GetName().Contains(TEXT("BowlRing"))) { BestZ = FMath::Max(BestZ, (float)Sm->Bounds.GetBox().Max.Z); BestR = ER + 250.f; }
			}
			if (BestZ < C.Z + 60.f) { BestZ = C.Z; BestR = ER; } // nothing stands there: the court lip''')
print('b142p ok')
