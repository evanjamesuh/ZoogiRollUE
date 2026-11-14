# Nov 11 (Claude): B238 part 2 - inventory + card style from Evan's Grok mockups
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
V = os.path.expanduser('~/mnt/dev/ios/b238')
p = os.path.join(S, 'Private/ZoogiFront.cpp'); t = io.open(p, encoding='utf-8-sig').read()
def rep(a, b):
    global t
    n = t.count(a)
    if n != 1: print('FAIL', n, a[:120]); sys.exit(1)
    t = t.replace(a, b)
st = io.open(os.path.join(V, 'inv_static.inl'), encoding='utf-8').read()
mb = io.open(os.path.join(V, 'inv_members.inl'), encoding='utf-8').read()
rep('	/** a label tag: ink or paper slab with a coloured shadow, text centred; returns its width */', st + '	/** a label tag: ink or paper slab with a coloured shadow, text centred; returns its width */')
rep('	void PaintInventory(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const\n', mb + '	void PaintInventory(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const\n')
rep('''			ZPanel(Out, G, L, FVector2D(X, 130.f), FVector2D(W, 58.f), bOn ? Mag() : Ink, bOn ? Paper : Ink.CopyWithNewOpacity(0.6f), 4.f, 6.f);''',
    '''			if (ZInvNew()) ZBox9(Out, G, L, ZInv(bOn ? TEXT("chip_on") : TEXT("chip")), FVector2D(X - 8.f, 124.f), FVector2D(W + 16.f, 70.f), 0.2f, 0.f); /* b238 */ else
			ZPanel(Out, G, L, FVector2D(X, 130.f), FVector2D(W, 58.f), bOn ? Mag() : Ink, bOn ? Paper : Ink.CopyWithNewOpacity(0.6f), 4.f, 6.f);''')
rep('''			ZPanel(Out, G, L, FVector2D(X3, Y3), FVector2D(W3, 58.f), Self->b3D ? Gold : Paper, Ink, 4.f, 6.f);
			ZTextC(Out, G, L + 3, Lb, F, FVector2D(X3 + W3 * 0.5f, Y3 + 29.f), Ink);''',
    '''			if (ZInvNew()) ZBox9(Out, G, L, ZInv(Self->b3D ? TEXT("chip_on") : TEXT("chip")), FVector2D(X3 - 8.f, Y3 - 6.f), FVector2D(W3 + 16.f, 70.f), 0.2f, 0.f); /* b238 */ else
			ZPanel(Out, G, L, FVector2D(X3, Y3), FVector2D(W3, 58.f), Self->b3D ? Gold : Paper, Ink, 4.f, 6.f);
			ZTextC(Out, G, L + 3, Lb, F, FVector2D(X3 + W3 * 0.5f, Y3 + 29.f), ZInvNew() ? Paper : Ink);''')
rep('''			ZPanel(Out, G, L, FVector2D(X + 10.f, 134.f), FVector2D(Hw, 50.f), NIn >= UZoogiFrontProfileSubsystem::DeckSize ? Mag() : Ink, Paper, 4.f, 5.f);''',
    '''			if (ZInvNew()) ZBox9(Out, G, L, ZInv(NIn >= UZoogiFrontProfileSubsystem::DeckSize ? TEXT("chip_on") : TEXT("chip")), FVector2D(X + 2.f, 124.f), FVector2D(Hw + 16.f, 70.f), 0.2f, 0.f); /* b238 */ else
			ZPanel(Out, G, L, FVector2D(X + 10.f, 134.f), FVector2D(Hw, 50.f), NIn >= UZoogiFrontProfileSubsystem::DeckSize ? Mag() : Ink, Paper, 4.f, 5.f);''')
rep('''		if (Tab == 3 && ZNewSkillsUi()) { PaintSkillsC(Out, G, L, Self, It, GX); return; } // b179''',
    '''		if (Tab == 3 && ZNewSkillsUi()) { if (ZInvNew()) PaintSkillsN(Out, G, L, Self, It, GX); else PaintSkillsC(Out, G, L, Self, It, GX); return; } // b179, b238
		if (ZInvNew() && It.Num() > 0)
		{	// b238: the grid sits on a neon panel
			const int32 Rn = (It.Num() + Cols - 1) / Cols;
			ZBox9(Out, G, L + 1, InvFrame(TEXT("panel")), FVector2D(GX - 26.f, 226.f - 26.f), FVector2D(Cols * W + (Cols - 1) * Gap + 52.f, Rn * W + (Rn - 1) * Gap + 52.f), 0.25f);
		}''')
rep('''			Tile(Out, G, L + 4, It[i], TL, W);
			if (It[i].Kind == 3)''', '''			if (ZInvNew()) TileN(Out, G, L + 4, It[i], TL, W); else Tile(Out, G, L + 4, It[i], TL, W);
			if (It[i].Kind == 3)''')
io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ALL OK')
