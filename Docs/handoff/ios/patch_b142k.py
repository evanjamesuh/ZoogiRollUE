# b142k: self-tests for b142 - -ZoogiShowtimeTest (P5 test turn 3: fill the AWAKEN meter, press SHOWTIME, then SUPERPOWER 0.8 s
# into the roll) and -ZoogiStandsTest (Frozen: P1's first roll after 8 s is sent flying at the nearest grandstand).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiP5Command.cpp', '''			Step(0, 0.7f, [&]() { Shot(FString::Printf(TEXT("t%d_open"), Tn + 1)); });''', '''			Step(0, 0.7f, [&]() { Shot(FString::Printf(TEXT("t%d_open"), Tn + 1)); if (Tn >= 2 && FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest"))) if (UZoogiFunSys* Ft = UZoogiFunSys::Get(G)) Ft->FillMeterForTest(G, B->PlayerIndex); });''')
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '''				AZoogiGameMode* Ga = GArmG.Get();
				if (GArmP < 0) return true;
				if (!Ga) { GArmP = -1; return true; }''', '''				AZoogiGameMode* Ga = GArmG.Get();
				if (GArmP < 0) return true;
				if (!Ga) { GArmP = -1; return true; }
				static const bool bAutoFire = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest"));
				if (bAutoFire && Ga->Phase == EZoogiPhase::Rolling && Ga->PhaseTime > 0.8f && Ga->TurnCounter == GArmTurn) { UE_LOG(LogZoogi, Log, TEXT("SHOWTIME TEST: SUPERPOWER press")); Ga->TryPower(); Ga->TakeShotDelayed(TEXT("awaken_fire"), 0.9f); Ga->TakeShotDelayed(TEXT("awaken_anime"), 2.6f); return true; }''')
A = 'Private/ZoogiArenaReact.cpp'
ed1(A, '''		static const bool bOff = ReactFlag(TEXT("ZoogiNoStandsSave"));
		if (bOff || !G || !B || !G->Arena || G->bRoyaleMode || !B->Sphere) return false;
		UWorld* W = G->GetWorld();
		const double Now = W->GetTimeSeconds();''', '''		static const bool bOff = ReactFlag(TEXT("ZoogiNoStandsSave"));
		if (bOff || !G || !B || !G->Arena || G->bRoyaleMode || !B->Sphere) return false;
		UWorld* W = G->GetWorld();
		const double Now = W->GetTimeSeconds();
		static bool bStandsTestDone = false;
		if (!bStandsTestDone && B->PlayerIndex == 0 && Now > 8.0 && G->Phase == EZoogiPhase::Rolling && G->ActivePlayer == 0 && ReactFlag(TEXT("ZoogiStandsTest")) && G->Arena->GetTheme() == EZoogiArena::Frozen)
		{	// b142 test: fling P1 at the nearest grandstand
			bStandsTestDone = true;
			ReactCollectStands(G);
			float BestD = 1e9f; FVector Tg = FVector::ZeroVector;
			for (const FBox& Bx : GReactStandBoxes) { const float D = FVector::Dist2D(Bx.GetCenter(), L); if (D < BestD) { BestD = D; Tg = Bx.GetCenter(); } }
			if (BestD < 1e8f)
			{
				const FVector Dir = FVector(Tg.X - L.X, Tg.Y - L.Y, 0.f).GetSafeNormal();
				B->Sphere->SetPhysicsLinearVelocity(Dir * 2600.f + FVector(0.f, 0.f, 380.f));
				UE_LOG(LogZoogi, Log, TEXT("STANDS TEST: P1 flung at the grandstand %.0f cm away"), BestD);
				G->TakeShotDelayed(TEXT("stands_fling"), 0.5f);
			}
		}''')
print('b142k ok')
