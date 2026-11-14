# b138b (Evan 10:25: "for BALLERS switch the balls to the front views of the Zoogi, not the models we bought used in Battle Royale"):
# a BALLERS tile / its popup shows Data/MenuArt/Front/zoogi_<ball>.png (the Zoogi's front view) when it exists, else the ball.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '''	static UTexture2D* ZBallIcon(const FString& Ball) { return ZLoadPng(TEXT("Data/MenuArt/Front/ball_") + Ball.ToLower()); }''',
'''	static UTexture2D* ZBallIcon(const FString& Ball) { return ZLoadPng(TEXT("Data/MenuArt/Front/ball_") + Ball.ToLower()); }
	/** b138 (Evan): the BALLERS tiles show the Zoogi itself (front view) when its art is in, else the ball */
	static UTexture2D* ZBallerArt(const FString& Ball)
	{
		static const bool bBalls = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBallerBalls"));
		if (!bBalls) if (UTexture2D* Z = ZLoadPng(TEXT("Data/MenuArt/Front/zoogi_") + Ball.ToLower())) return Z;
		return ZBallIcon(Ball);
	}''')
b = open(R + F, 'rb').read()
o = b'UTexture2D* Img = !I.Ball.IsEmpty() ? ZBallIcon(I.Ball) : '
n = b'UTexture2D* Img = !I.Ball.IsEmpty() ? ZBallerArt(I.Ball) : '
assert b.count(o) == 2, b.count(o)
open(R + F, 'wb').write(b.replace(o, n))
print('b138b ok')
