# Nov 11 (Claude): B248 - auto cover finds Voyager cover by its collision object channel "CoverPlane" (ECC_GameTraceChannel8),
# not by actor class (B247 found 0 on LevelCover).
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
p = S + 'Private/ZoogiVoyagerLevels.cpp'; t = io.open(p, encoding='utf-8-sig').read()
a = '''			const FVector P = Me->GetActorLocation();
			for (const TWeakObjectPtr<AActor>& Cv : G.Covers)
				if (AActor* C = Cv.Get()) { const FVector D = C->GetActorLocation() - P; if (FVector2D(D.X, D.Y).Size() < 260.f && FMath::Abs(D.Z) < 220.f) { G.bCoverNear = true; break; } }'''
b = '''			const FVector P = Me->GetActorLocation();
			for (const TWeakObjectPtr<AActor>& Cv : G.Covers)
				if (AActor* C = Cv.Get()) { const FVector D = C->GetActorLocation() - P; if (FVector2D(D.X, D.Y).Size() < 260.f && FMath::Abs(D.Z) < 220.f) { G.bCoverNear = true; break; } }
			if (!G.bCoverNear)
			{	// b248: Voyager cover = components on the "CoverPlane" object channel
				TArray<FOverlapResult> Hits; FCollisionQueryParams Qp(TEXT("ZoogiCover"), false, Me);
				W->OverlapMultiByObjectType(Hits, P, FQuat::Identity, FCollisionObjectQueryParams(ECC_GameTraceChannel8), FCollisionShape::MakeSphere(230.f), Qp);
				G.bCoverNear = Hits.Num() > 0;
				static int32 Logged = 0; if (G.bCoverNear && Logged < 3) { ++Logged; UE_LOG(LogZoogi, Log, TEXT("VOYPAD auto cover: cover plane near (%s)"), *GetNameSafe(Hits[0].GetActor())); }
			}'''
assert t.count(a) == 1; t = t.replace(a, b)
for inc in ('Engine/OverlapResult.h', 'CollisionQueryParams.h'):
    if '#include "%s"' % inc not in t:
        k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b248\n' % inc + t[k:]
io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok')
p2 = S + 'Private/ZoogiP5Style.cpp'; t2 = io.open(p2, encoding='utf-8-sig').read()
a2 = """				const FSlateFontInfo NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f), 0.f);
				const FString Nm = Spec.Items[i].Name;
				const FVector2D NS = Measure(Nm, NF);"""
b2 = """				FSlateFontInfo NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f), 0.f);
				const FString Nm = Spec.Items[i].Name;
				FVector2D NS = Measure(Nm, NF);
				if (NS.X > BW - 140.f && NS.X > 1.f) { NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f) * (BW - 140.f) / NS.X, 0.f); NS = Measure(Nm, NF); } // b248: long names fit the ribbon"""
assert t2.count(a2) == 1; t2 = t2.replace(a2, b2); io.open(p2, 'w', encoding='utf-8', newline='').write(t2); print('ok p5')
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B247"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B248"), TEXT("AUTO COVER FIX"), TEXT("Auto cover now finds the real Voyager cover spots (walls, crates, barriers) on every Voyager map.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f); print('ALL OK')
