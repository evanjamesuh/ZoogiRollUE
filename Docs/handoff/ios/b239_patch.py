# Nov 11 (Claude): B239 - award banner on a clean screen + after the cine; inventory skill grid picks 3/4 columns; text tweaks
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(rel, pairs, allow_multi=False):
    p = os.path.join(S, rel); t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        n = t.count(a)
        if n < 1 or (n != 1 and not allow_multi): print('FAIL', rel, n, a[:110]); sys.exit(1)
        t = t.replace(a, b)
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
patch('Public/ZoogiWallCine.h', [('	ZOOGIROLL_API bool Active();', '	ZOOGIROLL_API bool Active();\n	/** b239: the round award banner is up (the P5 HUD layers stay off under it) */\n	ZOOGIROLL_API bool BannerUp();')])
patch('Private/ZoogiWallCine.cpp', [('	bool Active() { return C.bOn; }', '	bool Active() { return C.bOn; }\n	bool BannerUp() { return ZoogiAwardBanner::Showing(); } // b239'),
    ('#include "ZoogiWallCine.h"\n', '#include "ZoogiWallCine.h"\n#include "ZoogiAwardBanner.h" // b239\n')])
patch('Public/ZoogiAwardBanner.h', [('	ZOOGIROLL_API void OnRoundEnd(AZoogiGameMode* G);', '	ZOOGIROLL_API void OnRoundEnd(AZoogiGameMode* G);\n	ZOOGIROLL_API bool Showing(); // b239')])
patch('Private/ZoogiAwardBanner.cpp', [
 ('			S.ShowAt = FPlatformTime::Seconds();', '			S.ShowAt = FPlatformTime::Seconds() + 0.7; // b239: after the turn change (a queued wall-smash cinematic goes first)'),
 ('	void Draw(AZoogiGameMode* G,', '	bool Showing() { const double t = FPlatformTime::Seconds() - S.ShowAt; return S.G.IsValid() && t >= 0.0 && t <= Dur; }\n\n	void Draw(AZoogiGameMode* G,'),
 ('		if (!G || !C || S.G.Get() != G) return;\n', '		if (!G || !C || S.G.Get() != G) return;\n		if (ZoogiWallCine::Active()) { if (FPlatformTime::Seconds() - S.ShowAt < Dur) S.ShowAt = FPlatformTime::Seconds() + 0.3; return; } // b239: the banner waits for the cinematic\n'),
 ('#include "ZoogiAwardBanner.h"\n', '#include "ZoogiAwardBanner.h"\n#include "ZoogiWallCine.h" // b239\n'),
])
for f in ('Private/ZoogiP5Style.cpp', 'Private/ZoogiAnimeCues.cpp'):
    patch(f, [('if (ZoogiWallCine::Active())', 'if (ZoogiWallCine::Active() || ZoogiWallCine::BannerUp())')], True)
patch('Private/ZoogiFront.cpp', [
 ('''		const int32 Cols = bPort ? 2 : 4, Rows = bPort ? 3 : 2;
		const float RowGap = Gap + 6.f;
		const float ByW = (LX1 - LX0 - (Cols - 1) * Gap) / Cols;
		const float ByH = ((LY1 - LY0 - ChipH - 26.f) - (Rows - 1) * RowGap) / Rows / 0.92f;
		const float W = FMath::Max(80.f, FMath::Min(ByW, ByH));''',
  '''		const int32 Rows = bPort ? 3 : 2;
		const float RowGap = Gap + 6.f;
		const float ByH = ((LY1 - LY0 - ChipH - 26.f) - (Rows - 1) * RowGap) / Rows / 0.92f;
		auto ColW = [&](int32 Cn) { return FMath::Min((LX1 - LX0 - (Cn - 1) * Gap) / Cn, ByH); };
		const int32 Cols = bPort ? 2 : (ColW(4) >= ColW(3) - 1.f ? 4 : 3); // b239: 4 across when the screen is wide enough (phones), else 3 bigger cards
		const float W = FMath::Max(80.f, ColW(Cols));'''),
 ('ZFitFont(Lv, 0, LSz.Y * 0.62f, LSz.X - 26.f, 2.f)', 'ZFitFont(Lv, 0, LSz.Y * 0.5f, LSz.X - 30.f, 2.f)'),
 ('ITL + FVector2D(16.f, 60.f) * Fk, FVector2D(100.f, 38.f) * Fk);', 'ITL + FVector2D(16.f, 72.f) * Fk, FVector2D(100.f, 38.f) * Fk);'),
 ('ITL + FVector2D(63.f, 78.f) * Fk, Paper);', 'ITL + FVector2D(63.f, 90.f) * Fk, Paper);'),
 ('ITL + FVector2D(126.f, 65.f) * Fk, FLinearColor(1.f, 0.9f, 0.25f, 1.f));', 'ITL + FVector2D(126.f, 77.f) * Fk, FLinearColor(1.f, 0.9f, 0.25f, 1.f));'),
 ('ITL + FVector2D(18.f, 110.f) * Fk, ISz.X - 36.f * Fk, 27.f * Fk, Paper);', 'ITL + FVector2D(18.f, 124.f) * Fk, ISz.X - 36.f * Fk, 27.f * Fk, Paper);'),
 ('			{ TEXT("B238"),', '			{ TEXT("B239"), TEXT("BANNER + INVENTORY POLISH"), TEXT("The round award banner now drops on a clean screen (the turn cards step aside) and waits for a wall-smash cinematic to finish. INVENTORY skills: bigger cards - 4 across on phones, 3 on narrower screens.") },\n			{ TEXT("B238"),'),
])
print('ALL OK')
