# b142m (Evan 12:08 "Make button larger and page functionality better" - NEWS screenshot): every popup's X is bigger (130 x 104,
# a wider tap area); NEWS gets a footer that the list never runs under: [TOP] - "SCROLL FOR MORE" / "THAT'S EVERY BUILD" - [CLOSE],
# a thicker scrollbar, and the newest build is marked NEW.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '''		const FVector2D CT(TL.X + Sz.X - 70.f, TL.Y - 26.f), CS(84.f, 64.f);
		ZRect(Out, G, L + 3, CT + FVector2D(6.f, 6.f), CS, Mag());
		ZRect(Out, G, L + 4, CT, CS, Ink);
		ZTextC(Out, G, L + 5, TEXT("X"), Font(0, 44.f), CT + CS * 0.5f, Paper);
		AddHit(CT - FVector2D(10.f, 10.f), CS + FVector2D(20.f, 20.f), [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });''', '''		const FVector2D CT(TL.X + Sz.X - 104.f, TL.Y - 58.f), CS(130.f, 104.f); // b142m (Evan): a bigger X
		ZRect(Out, G, L + 3, CT + FVector2D(8.f, 8.f), CS, Mag());
		ZRect(Out, G, L + 4, CT, CS, Ink);
		ZTextC(Out, G, L + 5, TEXT("X"), Font(0, 70.f), CT + CS * 0.5f, Paper);
		AddHit(CT - FVector2D(26.f, 26.f), CS + FVector2D(52.f, 52.f), [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });''')
ed1(F, '''		const float Top = 56.f, Bot = 24.f, ViewH = Sz.Y - Top - Bot, TextW''', '''		const float FootH = 110.f * Ks; // b142m: a footer the list never runs under
		const float Top = 64.f, Bot = 24.f + FootH, ViewH = Sz.Y - Top - Bot, TextW''')
ed1(F, '''				ZRect(Out, G, L + 4, R0, FVector2D(TW, 40.f * Ks), i <= 1 ? Mag() : Ink);''', '''				ZRect(Out, G, L + 4, R0, FVector2D(TW, 40.f * Ks), i <= 1 ? Mag() : Ink);
				if (i == 1) { const FVector2D Nb(TL.X + Sz.X - 150.f * Ks, Y + 4.f); ZRect(Out, G, L + 4, Nb, FVector2D(84.f * Ks, 34.f * Ks), Mag()); ZTextC(Out, G, L + 5, TEXT("NEW"), Font(0, 24.f * Ks), Nb + FVector2D(42.f, 17.f) * Ks, Paper); } // b142m''')
ed1(F, '''		if (Self->NewsMax > 0.f)
		{	// scrollbar + hint
			const float BarX = TL.X + Sz.X - 26.f, BarY = TL.Y + Top;
			ZRect(Out, G, L + 4, FVector2D(BarX, BarY), FVector2D(10.f, ViewH), Ink.CopyWithNewOpacity(0.12f));
			const float Th = FMath::Max(60.f, ViewH * ViewH / Total);
			ZRect(Out, G, L + 5, FVector2D(BarX, BarY + (ViewH - Th) * Self->NewsY / Self->NewsMax), FVector2D(10.f, Th), Mag());
			if (Self->NewsY < Self->NewsMax - 4.f)
			{
				const FString Hint = TEXT("SCROLL FOR MORE");
				const FSlateFontInfo HF = Font(0, 22.f * Ks);
				const FVector2D HS = ZMeasure(Hint, HF) + FVector2D(28.f, 10.f);
				const FVector2D HP(TL.X + Sz.X * 0.5f - HS.X * 0.5f, TL.Y + Sz.Y - HS.Y - 6.f);
				ZRect(Out, G, L + 6, HP, HS, Ink);
				ZTextC(Out, G, L + 7, Hint, HF, HP + HS * 0.5f, Paper);
			}
		}''', '''		if (Self->NewsMax > 0.f)
		{	// scrollbar (b142m: thicker)
			const float BarX = TL.X + Sz.X - 30.f, BarY = TL.Y + Top;
			ZRect(Out, G, L + 4, FVector2D(BarX, BarY), FVector2D(16.f, ViewH), Ink.CopyWithNewOpacity(0.12f));
			const float Th = FMath::Max(80.f, ViewH * ViewH / Total);
			ZRect(Out, G, L + 5, FVector2D(BarX, BarY + (ViewH - Th) * Self->NewsY / Self->NewsMax), FVector2D(16.f, Th), Mag());
		}
		{	// b142m footer: [TOP]  hint  [CLOSE]
			const float Fy = TL.Y + Sz.Y - FootH - 12.f;
			ZRect(Out, G, L + 4, FVector2D(TL.X + 20.f, Fy - 8.f), FVector2D(Sz.X - 40.f, 4.f), Ink);
			const FVector2D BS(FMath::Min(260.f * Ks, Sz.X * 0.3f), FootH - 18.f);
			const FVector2D TopP(TL.X + 30.f, Fy + 6.f), CloseP(TL.X + Sz.X - 30.f - BS.X, Fy + 6.f);
			const bool bAtTop = Self->NewsY < 4.f;
			ZRect(Out, G, L + 5, TopP + FVector2D(6.f, 6.f), BS, Mag().CopyWithNewOpacity(0.5f));
			ZRect(Out, G, L + 6, TopP, BS, bAtTop ? FLinearColor(0.55f, 0.55f, 0.58f) : Ink);
			ZTextC(Out, G, L + 7, TEXT("TOP"), Font(0, 40.f * Ks), TopP + BS * 0.5f, Paper);
			AddHit(TopP - FVector2D(10.f, 10.f), BS + FVector2D(20.f, 20.f), [Self]() { Self->NewsY = 0.f; Self->NewsVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.7f, 1.2f); });
			ZRect(Out, G, L + 5, CloseP + FVector2D(6.f, 6.f), BS, Ink);
			ZRect(Out, G, L + 6, CloseP, BS, Mag());
			ZTextC(Out, G, L + 7, TEXT("CLOSE"), Font(0, 40.f * Ks), CloseP + BS * 0.5f, Paper);
			AddHit(CloseP - FVector2D(10.f, 10.f), BS + FVector2D(20.f, 20.f), [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });
			const FString Hint = Self->NewsMax <= 0.f || Self->NewsY >= Self->NewsMax - 4.f ? FString(TEXT("THAT'S EVERY BUILD")) : FString(TEXT("SWIPE UP FOR MORE"));
			const float HwMax = CloseP.X - (TopP.X + BS.X) - 30.f;
			ZTextC(Out, G, L + 6, Hint, ZFitFont(Hint, 0, 26.f * Ks, HwMax), FVector2D((TopP.X + BS.X + CloseP.X) * 0.5f, Fy + 6.f + BS.Y * 0.5f), Ink);
		}''')
print('b142m ok')
