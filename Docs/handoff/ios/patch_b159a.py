import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n): EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            if b.count(o2) == 1: ob, nb = o2, n2
            else:
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b159a ok', len(EDITS))
# b159a (Evan Nov 6 11:12 "I tap hold to scroll please fix cause it stutters now on news and shop"): the finger only sets a
# target; the NEWS list and the SHOP strip glide to it every frame (28 ms smoothing), so uneven touch events (120 Hz touch on a
# 30/60 fps frame) and a held finger's jitter no longer make steps. A flick still glides from where the list is drawn.
EDITS.clear()
F = 'Private/ZoogiFront.cpp'
ed1(F, '''	TArray<TPair<double, float>> VelS;
''', '''	TArray<TPair<double, float>> VelS;
	float NewsTgt = 0.f, ShopTgt = 0.f; // b159: the finger sets the target, Tick glides the list to it
''')
ed1(F, '''		if (DragKind == 3) { bDrag = true; bAnchored = true; DragAnchorP = DownAt; DragAnchorV = NewsY; NewsVel = 0.f; AddVel(NewsY); } // b156: the list follows from the first pixel
		if (DragKind == 2) ShopXVel = 0.f;''', '''		if (DragKind == 3) { bDrag = true; bAnchored = true; DragAnchorP = DownAt; DragAnchorV = NewsY; NewsVel = 0.f; NewsTgt = NewsY; AddVel(NewsY); } // b156: the list follows from the first pixel
		if (DragKind == 2) { ShopXVel = 0.f; ShopTgt = ShopX; }''')
ed1(F, '''			if (DragKind == 2) { if (!bAnchored) { bAnchored = true; DragAnchorP = P; DragAnchorV = ShopX; } ShopX = FMath::Clamp(DragAnchorV - (P.X - DragAnchorP.X), 0.f, ShopMax); AddVel(ShopX); } // b156: no jump when the drag starts
			if (DragKind == 3) { NewsY = FMath::Clamp(DragAnchorV - (P.Y - DragAnchorP.Y), 0.f, NewsMax); AddVel(NewsY); }''', '''			if (DragKind == 2) { if (!bAnchored) { bAnchored = true; DragAnchorP = P; DragAnchorV = ShopX; } ShopTgt = FMath::Clamp(DragAnchorV - (P.X - DragAnchorP.X), 0.f, ShopMax); AddVel(ShopTgt); } // b156: no jump when the drag starts (b159: target)
			if (DragKind == 3) { NewsTgt = FMath::Clamp(DragAnchorV - (P.Y - DragAnchorP.Y), 0.f, NewsMax); AddVel(NewsTgt); }''')
ed1(F, '''		if (PendingTap && T >= PendingTapAt) { TFunction<void()> Fn = MoveTemp(PendingTap); PendingTap = nullptr; Fn(); } // b136: after the squash
''', '''		if (PendingTap && T >= PendingTapAt) { TFunction<void()> Fn = MoveTemp(PendingTap); PendingTap = nullptr; Fn(); } // b136: after the squash
		if (bDown && bAnchored && (DragKind == 2 || DragKind == 3))
		{	// b159: glide to the finger (frame-rate independent); snaps when within half a pixel
			static const bool bRaw = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRawScroll"));
			const float A = bRaw ? 1.f : 1.f - FMath::Exp(-FMath::Min(Dt, 0.1f) / 0.028f);
			float& Cur = DragKind == 3 ? NewsY : ShopX; const float Tg = DragKind == 3 ? NewsTgt : ShopTgt;
			Cur = FMath::Abs(Tg - Cur) < 0.5f ? Tg : Cur + (Tg - Cur) * A;
		}
''')
run()
