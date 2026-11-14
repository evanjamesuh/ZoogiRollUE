# b138c (Evan 10:30: "Golden Apple leaves an actual golden apple afterwards - a medium sized blast when a player touches it"):
# the lobbed apple stays where it landed (spinning gold, glow); the first Zoogi to touch it (after 0.6 s) sets off a medium blast
# (one wolf blast, 430 cm) credited to the caster. It stays until touched; cleared with the field / a new match.
# -ZoogiNoAppleLeft = the apple vanishes on landing (b134).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		struct FMine { FVector P; int32 Owner = -1; TWeakObjectPtr<AZoogiFabFX> Fx; float Pow = 1.f; }; // b136: Pow = the cast's strength''',
'''		struct FMine { FVector P; int32 Owner = -1; TWeakObjectPtr<AZoogiFabFX> Fx; float Pow = 1.f; }; // b136: Pow = the cast's strength
		struct FGoldApple { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector; int32 Owner = -1; double ArmAt = 0.0; float Spin = 0.f; }; // b138c: the apple left on the court
		TArray<FGoldApple> GoldApples;
		void DropGoldApples() { for (FGoldApple& Ga : GoldApples) if (AActor* A = Ga.A.Get()) A->Destroy(); GoldApples.Reset(); }''')
ed1(S, '''						if (K < 1.f) return true;
						if (Ap) Ap->Destroy();''', '''						if (K < 1.f) return true;
						static const bool bNoLeft = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAppleLeft"));
						if (Ap && !bNoLeft)
						{	// b138c (Evan): the golden apple stays where it landed - touching it sets off a medium blast
							Ap->SetActorLocation(FVector(To.X, To.Y, FZA + 36.f));
							FGoldApple Ga; Ga.A = Ap; Ga.P = FVector(To.X, To.Y, FZA + 36.f); Ga.Owner = PiA; Ga.ArmAt = FPlatformTime::Seconds() + 0.6;
							GoldApples.Add(Ga);
							UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: the apple stays at (%.0f, %.0f) - touch = blast"), To.X, To.Y);
						}
						else if (Ap) Ap->Destroy();''')
ed1(S, '''			for (FMine& M : Mines) if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
			AZoogiIceBarrier::DestroyAll(G->GetWorld());''', '''			for (FMine& M : Mines) if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
			DropGoldApples(); // b138c
			AZoogiIceBarrier::DestroyAll(G->GetWorld());''')
ed1(S, '''		for (FMine& M : Mines) if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
		for (FBuff& Bf : Buffs)''', '''		for (FMine& M : Mines) if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
		DropGoldApples(); // b138c
		for (FBuff& Bf : Buffs)''')
ed1(S, '''		for (int32 i = Mines.Num() - 1; i >= 0; --i)''', '''		for (int32 i = GoldApples.Num() - 1; i >= 0; --i)
		{	// b138c: a Zoogi touching a golden apple left on the court -> a medium blast
			FGoldApple& Ga = GoldApples[i];
			AActor* Aa = Ga.A.Get();
			if (!Aa) { GoldApples.RemoveAt(i); continue; }
			Ga.Spin += 90.f * G->GetWorld()->GetDeltaSeconds();
			Aa->SetActorLocationAndRotation(Ga.P + FVector(0.f, 0.f, 8.f * FMath::Sin(Ga.Spin * 0.05f)), FRotator(0.f, Ga.Spin, 0.f));
			if (FPlatformTime::Seconds() < Ga.ArmAt) continue;
			AZoogiBall* Toucher = nullptr;
			for (AZoogiBall* Bt : G->Balls) if (Bt && !Bt->bOut && FVector::Dist2D(Bt->BallLocation(), Ga.P) < ZoogiConst::ZoogiRadius + 45.f) { Toucher = Bt; break; }
			if (!Toucher) continue;
			const FVector Gp(Ga.P.X, Ga.P.Y, Ga.P.Z - 32.f);
			{ FTuneScope Tn(1.f, FName(TEXT("golden_apple"))); Blast(G, Gp, 430.f, 1000.f, 260.f, nullptr, Ga.Owner); } // one wolf-size blast (lab radius / strength apply)
			const FLinearColor Gold(1.f, 0.8f, 0.25f);
			AZoogiFX::Spawn(G->GetWorld(), Gp, Gold, 430.f, 0.5f, true, 10.f);
			AZoogiFX::Shockwave(G->GetWorld(), Gp, EZoogiShockwaveSize::Normal, EZoogiShockwaveKind::Flat, &Gold, false);
			AZoogiFX::Stylized(G->GetWorld(), TEXT("Mid_Explosion"), Gp, 0.4f);
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.2f);
			UE_LOG(LogZoogi, Log, TEXT("SKILL golden apple touched by P%d - blast (owner P%d)"), Toucher->PlayerIndex + 1, Ga.Owner + 1);
			Aa->Destroy();
			GoldApples.RemoveAt(i);
		}
		for (int32 i = Mines.Num() - 1; i >= 0; --i)''')
print('b138c ok')
