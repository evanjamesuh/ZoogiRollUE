# b143b (Evan 12:31 "Give weapons that same zoogi roll arena back splash like baller. Also start with these heads for the ballers
# instead of the balls"): ITEM (weapon) tiles + detail get the ZOOGI ROLL ARENA burst behind their render; the BALLERS tab uses the
# new crystal heads Data/MenuArt/Front/zoogi_<ball>.png (Frosti, Hotstreak, Vex, Lumi so far - the others keep the ball until
# their heads arrive); the ZOOGIS tab always shows the balls.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n, cnt=1):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != cnt and b'\r\n' in b: o = o.replace(b'\\n', b'\r\n'); n = n.replace(b'\\n', b'\r\n')
    assert b.count(o) == cnt, (p, o[:80], b.count(o)); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '''UTexture2D* Img = !I.Ball.IsEmpty() ? ZBallerArt(I.Ball) : !I.CardId.IsEmpty()''', '''UTexture2D* Img = !I.Ball.IsEmpty() ? (I.Kind == 0 ? ZBallerArt(I.Ball) : ZBallIcon(I.Ball)) : !I.CardId.IsEmpty()''', 2)
ed1(F, '''			if (!I.Ball.IsEmpty() && !bPlain) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers")))''', '''			if ((!I.Ball.IsEmpty() || I.Kind == 2) && !bPlain) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers")))''')
ed1(F, '''		if (!I.Ball.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiPlainBallCards"))) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers_sq")))''', '''		if ((!I.Ball.IsEmpty() || I.Kind == 2) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiPlainBallCards"))) if (UTexture2D* Bg = ZLoadPng(TEXT("Data/MenuArt/Front/card_bg_ballers_sq")))''')
ed1(F, '''TEXT("Inventory: the WEAPON tab is now ITEM.") },''', '''TEXT("Inventory: the WEAPON tab is now ITEM, with the ZOOGI ROLL ARENA burst behind each item. BALLERS show their crystal heads (Frosti, Hotstreak, Vex, Lumi).") },''')
print('b143b ok')
