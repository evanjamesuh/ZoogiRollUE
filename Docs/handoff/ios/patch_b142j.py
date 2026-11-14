# b142j: the boss was hidden - the Cosmos court is ringed by the R2 bowl wall (stone stands ring) and the hands were on the court
# lip behind it. The peek now finds the top of whatever stands beyond the court edge on that heading (vertical traces 0-15 m out,
# the highest top, its inner edge) and puts both hands there; 12 m tall, leaning far over so the head hangs just above the wall.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
B = 'Private/ZoogiCosmosBoss.cpp'
ed1(B, '''		constexpr float PeekH = 1800.f; // b142: 18 m - head + shoulders over the far rim in the gameplay camera''', '''		constexpr float PeekH = 1200.f; // b142j: 12 m - hands on the bowl wall top, the head just above it in the gameplay camera
		// b142j: the top of the wall / stands beyond the court edge on a heading: (distance from the centre, height); cached per heading
		FVector2D RimTop(AZoogiGameMode* G, float Yaw, float ER)
		{
			static float LastYaw = -999.f; static FVector2D Last(0.f, 0.f); static TWeakObjectPtr<AZoogiGameMode> LastG;
			if (LastG.Get() == G && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, LastYaw)) < 1.5f) return Last;
			LastG = G; LastYaw = Yaw;
			const FVector C = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiBossRim), true);
			for (AZoogiBall* Zb : G->Balls) if (Zb) Q.AddIgnoredActor(Zb);
			if (AActor* Pa = B.A.Get()) Q.AddIgnoredActor(Pa);
			float BestZ = C.Z, BestR = ER;
			TArray<FVector2D> Hits;
			for (float Rr = ER + 20.f; Rr <= ER + 1500.f; Rr += 40.f)
			{
				FHitResult H; const FVector P = C + Dir * Rr;
				if (G->GetWorld()->LineTraceSingleByChannel(H, P + FVector(0.f, 0.f, 4000.f), P - FVector(0.f, 0.f, 600.f), ECC_Visibility, Q)) Hits.Add(FVector2D(Rr, H.ImpactPoint.Z));
			}
			for (const FVector2D& Hh : Hits) if (Hh.Y > BestZ + 1.f && Hh.Y < C.Z + 2500.f) BestZ = Hh.Y;
			for (const FVector2D& Hh : Hits) if (Hh.Y >= BestZ - 40.f) { BestR = Hh.X; break; } // the inner edge of the top
			if (BestZ < C.Z + 60.f) { BestZ = C.Z; BestR = ER; } // nothing stands there: the court lip
			Last = FVector2D(BestR, BestZ);
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS rim at yaw %.0f: wall top r%.0f z%.0f (court edge r%.0f, %d hits)"), Yaw, BestR, BestZ, ER, Hits.Num());
			return Last;
		}''')
ed1(B, '''			P->HandPin = C + Dir * (ER + 25.f) + FVector(0.f, 0.f, 12.f - B.Dip * PeekH * 0.85f + B.Rise * 30.f);''', '''			const FVector2D Rt = RimTop(G, B.PeekAng, ER); // b142j: over the bowl wall, not behind it
			P->HandPin = FVector(C.X, C.Y, 0.f) + Dir * (Rt.X + 15.f) + FVector(0.f, 0.f, Rt.Y + 10.f - B.Dip * PeekH * 0.85f + B.Rise * 30.f);''')
ed1(B, '''			P->Lean = B.Lean / 25.f;''', '''			P->Lean = 0.35f + B.Lean / 25.f; // b142j: well over the edge''')
print('b142j ok')
