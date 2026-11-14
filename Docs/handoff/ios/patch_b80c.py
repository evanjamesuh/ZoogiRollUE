# b80c (Evan 01:45: "ultimate ready banner should appear on the left side under the super meter")
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiActionFun.cpp', [
 ('			else if (!A->bCPU && Meter(A->PlayerIndex) >= 100.f && !bUltThisShot && bPlay) Notes.Add(',
  '			else if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiUltNote")) && !A->bCPU && Meter(A->PlayerIndex) >= 100.f && !bUltThisShot && bPlay) Notes.Add('),
 ('''		FunText(C, M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER"), X, Y + Bh + 2.f * U, 16.f * U, Mc, false);
		break;''',
  '''		FunText(C, M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER"), X, Y + Bh + 2.f * U, 16.f * U, Mc, false);
		// b80 (Evan): the ULTIMATE READY banner sits on the left, right under this meter (was a lower-left P5 note)
		if (M >= 1.f && !bUltThisShot && G->ActivePlayer == i && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && !Clash.bOn)
		{
			const FString Rd = FString::Printf(TEXT("%s READY  -  TAP SUPERPOWER WHILE ROLLING"), UltName(Sig));
			float Px = 20.f * U;
			FVector2D TS = P5Measure(Rd, FunFontP(0, Px));
			const float MaxL = Wd * 0.9f;
			if (TS.X + 50.f * U > MaxL) { Px *= (MaxL - 50.f * U) / FMath::Max(1.f, TS.X); TS = P5Measure(Rd, FunFontP(0, Px)); }
			const float Len = TS.X + 50.f * U, Hg = Px * 1.8f;
			const FVector2D Ct(X + Len * 0.5f, Y + Bh + 26.f * U + Hg * 0.5f);
			P5Quad(C, Ct + FVector2D(6.f, 6.f) * U, Len, Hg, -4.f, 10.f * U, FLinearColor(Mc.R, Mc.G, Mc.B, 0.95f));
			P5Quad(C, Ct, Len, Hg, -4.f, 10.f * U, FLinearColor(0.02f, 0.02f, 0.05f, 0.95f));
			FunTextF(C, Rd, Ct.X, Ct.Y, Px, FLinearColor::White, 1, 0);
		}
		break;'''),
])
print('b80c ok')
