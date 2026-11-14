# b136f (Evan 08:59: "I'll take the center version of zoogi roll arena"): every ball (BALLERS) tile and the ball's item popup get
# the white-to-orange burst with ZOOGI ROLL ARENA behind the ball (Data/MenuArt/Front/card_bg_ballers*.png). -ZoogiPlainBallCards = white.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '''		else if (Img) ZTexFit(Out, G, L + 3, Img, TL + FVector2D(W * 0.12f, 14.f), FVector2D(W * 0.76f, W - NB - 22.f), Dim);''',
'''		else if (Img)
		{
			static const bool bPlain = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPlainBallCards"));
			if (!I.Ball.IsEmpty() && !bPlain) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers"))) ZTex(Out, G, L + 2, Bg, TL + FVector2D(0.f, 8.f), FVector2D(W, W - NB - 8.f), Dim); // b136 (Evan): ZOOGI ROLL ARENA burst
			ZTexFit(Out, G, L + 3, Img, TL + FVector2D(W * 0.12f, 14.f), FVector2D(W * 0.76f, W - NB - 22.f), Dim);
		}''')
ed1(F, '''		if (Img) ZTexFit(Out, G, L + 5, Img, TL + FVector2D(40.f, 46.f), FVector2D(230.f, 226.f));''',
'''		if (!I.Ball.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiPlainBallCards"))) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers_sq"))) ZTex(Out, G, L + 4, Bg, TL + FVector2D(30.f, 40.f), FVector2D(250.f, 240.f)); // b136
		if (Img) ZTexFit(Out, G, L + 5, Img, TL + FVector2D(40.f, 46.f), FVector2D(230.f, 226.f));''')
print('b136f ok')
