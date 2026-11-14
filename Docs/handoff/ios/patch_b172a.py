# b172a (Evan picked "C colour by state" for the home chest slots): OPEN! = pink, START = teal, timer = deep teal, EMPTY = dashed outline
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiFront.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new,cnt=1):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==cnt: s=s.replace(o,n); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
helper='''			const FLinearColor SlotC = bReady ? Mag() : Left < 0 ? FLinearColor::FromSRGBColor(FColor(30, 170, 200)) : FLinearColor::FromSRGBColor(FColor(16, 104, 128));
			auto Dash = [&](const FVector2D& P0, const FVector2D& Sz, float Th)
			{	// b172a: an empty slot is just a dashed outline (the room shows through)
				const FLinearColor Dc = FLinearColor::FromSRGBColor(FColor(200, 190, 230)); const float Dl = 14.f, Gp = 9.f;
				for (float X = 0.f; X < Sz.X; X += Dl + Gp) { const float Ln = FMath::Min(Dl, Sz.X - X); ZRect(Out, G, L + 3, P0 + FVector2D(X, 0.f), FVector2D(Ln, Th), Dc); ZRect(Out, G, L + 3, P0 + FVector2D(X, Sz.Y - Th), FVector2D(Ln, Th), Dc); }
				for (float Y = 0.f; Y < Sz.Y; Y += Dl + Gp) { const float Ln = FMath::Min(Dl, Sz.Y - Y); ZRect(Out, G, L + 3, P0 + FVector2D(0.f, Y), FVector2D(Th, Ln), Dc); ZRect(Out, G, L + 3, P0 + FVector2D(Sz.X - Th, Y), FVector2D(Th, Ln), Dc); }
			};
'''
ed('''			ZPanel(Out, G, L + 3, TL, FVector2D(CW, RH), bEmpty ? FLinearColor(1.f, 1.f, 1.f, 0.6f) : Paper, Mag(), 5.f, 9.f);
			const FString Head = bEmpty ? TEXT("EMPTY") : bReady ? TEXT("OPEN!") : Left < 0 ? TEXT("START") : ZClock(Left);
			ZRect(Out, G, L + 6, TL, FVector2D(CW, 64.f), bReady || Left >= 0 ? Mag() : Ink);''',helper+'''			if (bEmpty) { ZRect(Out, G, L + 2, TL, FVector2D(CW, RH), FLinearColor(0.02f, 0.015f, 0.05f, 0.25f)); Dash(TL, FVector2D(CW, RH), 4.f); }
			else ZPanel(Out, G, L + 3, TL, FVector2D(CW, RH), SlotC, FLinearColor::White, 5.f, 9.f);
			const FString Head = bEmpty ? TEXT("EMPTY") : bReady ? TEXT("OPEN!") : Left < 0 ? TEXT("START") : ZClock(Left);
			ZRect(Out, G, L + 6, TL, FVector2D(CW, 64.f), FLinearColor(0.012f, 0.008f, 0.03f, 0.92f));''')
ed('''			ZPanel(Out, G, L + 3, TL, FVector2D(CW, 150.f), bEmpty ? FLinearColor(1.f, 1.f, 1.f, 0.55f) : Paper, Mag(), 4.f, 8.f);
			const FString Head = bEmpty ? TEXT("EMPTY") : bReady ? TEXT("OPEN!") : Left < 0 ? TEXT("START") : ZClock(Left);
			ZRect(Out, G, L + 6, TL, FVector2D(CW, 38.f), bReady ? Mag() : Left >= 0 ? Mag() : Ink);''',helper+'''			if (bEmpty) { ZRect(Out, G, L + 2, TL, FVector2D(CW, 150.f), FLinearColor(0.02f, 0.015f, 0.05f, 0.25f)); Dash(TL, FVector2D(CW, 150.f), 3.f); }
			else ZPanel(Out, G, L + 3, TL, FVector2D(CW, 150.f), SlotC, FLinearColor::White, 4.f, 8.f);
			const FString Head = bEmpty ? TEXT("EMPTY") : bReady ? TEXT("OPEN!") : Left < 0 ? TEXT("START") : ZClock(Left);
			ZRect(Out, G, L + 6, TL, FVector2D(CW, 38.f), FLinearColor(0.012f, 0.008f, 0.03f, 0.92f));''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
