# b172b (Evan picked marble finder A + B + C): floor ring + gold arrow + YOU, face pin, colour trail - for the human marbles
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiActionFun.cpp'; s=open(R+f,'rb').read().decode('utf-8')
old='''		FCanvasTileItem Tip(FVector2D(Sp.X - 3.f * U, Sp.Y + Sz * 0.5f + 4.f * U), FVector2D(6.f * U, 12.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.9f)); Tip.BlendMode = SE_BLEND_Translucent; C->DrawItem(Tip);
	}
}
'''
new='''		FCanvasTileItem Tip(FVector2D(Sp.X - 3.f * U, Sp.Y + Sz * 0.5f + 4.f * U), FVector2D(6.f * U, 12.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.9f)); Tip.BlendMode = SE_BLEND_Translucent; C->DrawItem(Tip);
	}
	// b172b (Evan: "better ways to identify your marble from top view in the midst of chaos" -> picked A + B + C): each human marble gets
	// a pulsing ring on the floor in its colour (A), its face on a pin with a gold arrow and YOU / P# (A + B) and a comet trail while it
	// rolls (C). -ZoogiNoMarbleFinder = off.
	static const bool bNoFinder = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoMarbleFinder"));
	if (bNoFinder) return;
	static TArray<FVector> GTrail[4];
	int32 NHuman = 0; for (const AZoogiBall* Bh : G->Balls) if (Bh && !Bh->bCPU) ++NHuman;
	const double Tn = FPlatformTime::Seconds();
	for (const AZoogiBall* Bh : G->Balls)
	{
		if (!Bh || Bh->bCPU || Bh->PlayerIndex < 0 || Bh->PlayerIndex > 3) continue;
		TArray<FVector>& Tr = GTrail[Bh->PlayerIndex];
		if (Bh->bOut || Bh->IsHidden()) { Tr.Reset(); continue; }
		const FVector BL = Bh->BallLocation();
		const FLinearColor Pc = ZoogiUtil::PlayerColor(Bh->PlayerIndex);
		// C: the trail (last ~24 spots, one every 18 cm), only while it really moves
		const bool bMoving = Bh->GetBallVelocity().Size2D() > 120.f;
		if (bMoving) { if (Tr.Num() == 0 || FVector::Dist(Tr.Last(), BL) > 18.f) { Tr.Add(BL); if (Tr.Num() > 24) Tr.RemoveAt(0); } }
		else if (Tr.Num() > 0) Tr.RemoveAt(0);
		for (int32 k = 1; k < Tr.Num(); ++k)
		{
			FVector2D A0, A1;
			if (!PC->ProjectWorldLocationToScreen(Tr[k - 1], A0, true) || !PC->ProjectWorldLocationToScreen(Tr[k], A1, true)) continue;
			const float F = (float)k / Tr.Num();
			FCanvasLineItem Ln(A0, A1); Ln.LineThickness = (4.f + 18.f * F) * U; Ln.SetColor(FLinearColor(Pc.R, Pc.G, Pc.B, 0.15f + 0.6f * F)); Ln.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ln);
		}
		// A: a pulsing ring on the floor around the marble (projected circle)
		{
			const float Pulse = 1.f + 0.08f * FMath::Sin((float)Tn * 6.f);
			const float Rr = ZoogiConst::ZoogiRadius * 1.9f * Pulse;
			const FVector Floor = BL - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - 4.f);
			FVector2D Prev; bool bPrev = false;
			for (int32 k = 0; k <= 28; ++k)
			{
				const float Ang = k * 2.f * PI / 28.f; FVector2D Pk;
				const bool bOk = PC->ProjectWorldLocationToScreen(Floor + FVector(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, 0.f), Pk, true);
				if (bOk && bPrev) { FCanvasLineItem Ln(Prev, Pk); Ln.LineThickness = 5.f * U; Ln.SetColor(FLinearColor(Pc.R, Pc.G, Pc.B, 0.95f)); Ln.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ln); }
				Prev = Pk; bPrev = bOk;
			}
		}
		// A + B: the face pin above the marble, a gold arrow down to it, YOU (one human) or P# over the face
		FVector2D Sp;
		if (!PC->ProjectWorldLocationToScreen(BL + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 110.f), Sp, true)) continue;
		if (Sp.X < -50.f || Sp.Y < -50.f || Sp.X > Wd + 50.f || Sp.Y > Ht + 50.f || Sp.Y < Ht * 0.17f) continue;
		const float Sz = 58.f * U * (Ht > Wd ? 1.25f : 1.f);
		const FLinearColor Gold(1.f, 0.75f, 0.05f, 1.f);
		FCanvasTileItem Bk(Sp - FVector2D(Sz * 0.5f + 5.f * U, Sz * 0.5f + 5.f * U), FVector2D(Sz + 10.f * U, Sz + 10.f * U), FLinearColor(1.f, 1.f, 1.f, 0.95f)); Bk.BlendMode = SE_BLEND_Translucent; C->DrawItem(Bk);
		FCanvasTileItem Fr(Sp - FVector2D(Sz * 0.5f + 2.f * U, Sz * 0.5f + 2.f * U), FVector2D(Sz + 4.f * U, Sz + 4.f * U), FLinearColor(Pc.R, Pc.G, Pc.B, 1.f)); Fr.BlendMode = SE_BLEND_Translucent; C->DrawItem(Fr);
		if (UTexture2D* Tx = ZoogiP5::Portrait(Bh->Def.Name)) { FCanvasTileItem Ic(Sp - FVector2D(Sz, Sz) * 0.5f, Tx->GetResource(), FVector2D(Sz, Sz), FLinearColor::White); Ic.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ic); }
		const float Bob = 4.f * U * FMath::Sin((float)Tn * 5.f);
		const FVector2D At(Sp.X, Sp.Y + Sz * 0.5f + 10.f * U + Bob);
		FCanvasTriangleItem Tri(FVector2D(At.X - 14.f * U, At.Y), FVector2D(At.X + 14.f * U, At.Y), FVector2D(At.X, At.Y + 18.f * U), GWhiteTexture); Tri.SetColor(Gold); Tri.BlendMode = SE_BLEND_Translucent; C->DrawItem(Tri);
		FunText(C, NHuman <= 1 ? FString(TEXT("YOU")) : FString::Printf(TEXT("P%d"), Bh->PlayerIndex + 1), Sp.X, Sp.Y - Sz * 0.5f - 34.f * U, 30.f * U, FLinearColor::White);
	}
}
'''
for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
    if s.count(o)==1: s=s.replace(o,n); open(R+f,'wb').write(s.encode('utf-8')); print('WROTE'); break
else: print('MISS', s.count(old))
