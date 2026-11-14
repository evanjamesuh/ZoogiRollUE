# b127a (Evan 02:24: "why isnt news updating and make more and scroll on page"): NEWS lists every build from b108 to b126 (newest first),
# rows sized to their text (the B119 text ran into B115), the card fills the screen and scrolls - swipe / drag on phones, the mouse wheel
# or drag on PC, scrollbar + "SCROLL FOR MORE".
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '''	static FString ZNumS(int32 N) { return FText::AsNumber(N).ToString(); }''', '''	static int32 ZWrapLines(const FString& S, const FSlateFontInfo& F, float MaxW) // b127: how many lines ZWrap draws
	{
		TArray<FString> Words; S.ParseIntoArray(Words, TEXT(" "));
		FString Line; int32 N = 0;
		for (const FString& W : Words)
		{
			const FString Try = Line.IsEmpty() ? W : Line + TEXT(" ") + W;
			if (ZMeasure(Try, F).X > MaxW && !Line.IsEmpty()) { ++N; Line = W; }
			else Line = Try;
		}
		return N + (Line.IsEmpty() ? 0 : 1);
	}
	static FString ZNumS(int32 N) { return FText::AsNumber(N).ToString(); }''')
ed1(F, '''	int32 DragKind = 0; // 1 robot spin, 2 shop scroll''', '''	int32 DragKind = 0; // 1 robot spin, 2 shop scroll, 3 news scroll
	float NewsY = 0.f, NewsMax = 0.f, NewsVel = 0.f; FBox2D NewsBox = FBox2D(ForceInit); // b127: the NEWS list scrolls''')
ed1(F, '''		DragKind = Pop != EPop::None ? 0 : (Screen == 2''', '''		DragKind = (Pop == EPop::News && NewsBox.bIsValid && NewsBox.IsInside(DownAt)) ? 3 : Pop != EPop::None ? 0 : (Screen == 2''')
ed1(F, '''			if (DragKind == 2) { ShopX = FMath::Clamp(ShopX - Dx, 0.f, ShopMax); ShopXVel = -Dx / FMath::Max(0.008f, FSlateApplication::Get().GetDeltaTime()); }''',
'''			if (DragKind == 2) { ShopX = FMath::Clamp(ShopX - Dx, 0.f, ShopMax); ShopXVel = -Dx / FMath::Max(0.008f, FSlateApplication::Get().GetDeltaTime()); }
			if (DragKind == 3) { const float Dy = P.Y - LastAt.Y; NewsY = FMath::Clamp(NewsY - Dy, 0.f, NewsMax); NewsVel = -Dy / FMath::Max(0.008f, FSlateApplication::Get().GetDeltaTime()); }''')
ed1(F, '''		else if (DragKind != 2) ShopXVel = 0.f;''', '''		else if (DragKind != 2) ShopXVel = 0.f;
		if (!bDrag || DragKind != 3) NewsVel = 0.f;''')
ed1(F, '''	virtual FReply OnTouchEnded(const FGeometry& G, const FPointerEvent& E) override { return Up(G, E); }
''', '''	virtual FReply OnTouchEnded(const FGeometry& G, const FPointerEvent& E) override { return Up(G, E); }
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent& E) override // b127: the mouse wheel scrolls NEWS
	{
		if (Pop != EPop::News) return FReply::Unhandled();
		NewsY = FMath::Clamp(NewsY - E.GetWheelDelta() * 110.f, 0.f, NewsMax); NewsVel = 0.f;
		return FReply::Handled();
	}
''')
ed1(F, '''		if (!bDown && FMath::Abs(ShopXVel) > 1.f) {''', '''		if (!bDown && FMath::Abs(NewsVel) > 1.f) { NewsY = FMath::Clamp(NewsY + NewsVel * Step, 0.f, NewsMax); NewsVel *= FMath::Exp(-5.f * Step); } // b127: a flick keeps scrolling
		if (!bDown && FMath::Abs(ShopXVel) > 1.f) {''')
ed1(F, '''		Pop = P; PopT = T;''', '''		Pop = P; PopT = T;
		if (P == EPop::News) { NewsY = 0.f; NewsVel = 0.f; } // b127: newest first''')
# the list + layout
b = open(R + F, 'rb').read().decode('utf-8', 'surrogateescape')
a = b.index('	void PaintNews(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const')
e = b.index('	void PaintSettings(FSlateWindowElementList& Out', a)
nl = '\r\n' if '\r\n' in b[a:e] else '\n'
NEW = r'''	void PaintNews(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		struct FN { const TCHAR* Tag; const TCHAR* Head; const TCHAR* Body; };
		static const FN News[] = {
			// b90 (Evan: "news update with what we actually changed so I can use it as a checklist - tell folks to try it out and give feedback");
			// b127 (Evan: "why isn't news updating - make more and scroll on page"): every build from b108, newest first, scrolling list
			{ TEXT("TEST"), TEXT("TRY IT - SEND FEEDBACK"), TEXT("Play a few matches and check off each item below, newest build first. Tell us what feels great and what is broken. Swipe (or use the mouse wheel) to scroll.") },
			{ TEXT("B126"), TEXT("REDIRECT HITS HARD"), TEXT("REDIRECT keeps its full speed all the way to the target you tapped - your Zoogi really travels there and smashes in.") },
			{ TEXT("B125"), TEXT("TAPS FIRE ON TOUCH, FAIR CPUS"), TEXT("REDIRECT and superpower targets fire the moment your finger touches them. A button press can never leave taps stuck. REDIRECT launches harder. CPUs play by your rules: every skill is cast after launch, one at a time, and Wooden Shield, Bolt Balls and Energy Buff never stack.") },
			{ TEXT("B124"), TEXT("SMARTER CPUS, ICE WALLS"), TEXT("CPUs pick skills that fit the moment (no cannon with nothing in reach, a shield when they're near the edge, meteors on a crowd), cast aiming skills mid-roll, go after the leader and rivals near the edge, look one move ahead, and spend a crystal to save themselves from rolling off. Ice walls you build stay the whole match until 3 hits break them.") },
			{ TEXT("B123"), TEXT("COMBO PICKS MID-ROLL"), TEXT("A COMBO cast that needs a target, a line or an area opens the normal pick from where your Zoogi is mid-roll: FIRE casts it, CANCEL puts it back in the queue. Mid-roll picks keep the roll camera angle.") },
			{ TEXT("B122"), TEXT("REACTION VS SUPERPOWER"), TEXT("REDIRECT circles can be tapped on phones again. REACTION and SUPERPOWER each do their own job at any time of the roll - REACTION closes an open REDIRECT pick, SUPERPOWER closes it and does its own job (next COMBO cast, wolves, bolts, ultimate).") },
			{ TEXT("B121"), TEXT("SHOWTIME, GUARD, COMBO CASTS"), TEXT("SHOWTIME: the phone rumbles 2 s, the ancient Frozen symbols glow, the lights dim, an energy buff flares - then tap your target. GUARD raises a 10 s force field round your Zoogi. COMBO: tick skills, CAST - launch, then each SUPERPOWER tap fires the next cast from where your Zoogi is. No target in range = no REDIRECT slow-down.") },
			{ TEXT("B120"), TEXT("WEAPONS, MAPS, BUTTON TAPS"), TEXT("WEAPON tab: Prism Grenade, Crystal Sword, Crystal Staff and Halo Disc - equip one for Battle Royale. New map pictures (portrait fills the screen), portrait inventory fills the screen. The black-and-white impact flash lasts 0.2 s. Button taps never count as a target tap on the field.") },
			{ TEXT("B119"), TEXT("WOLVES, PRECISION, FROZEN"), TEXT("Wolf Pack orbs wait on the field with you - tap WOLF while aiming or rolling, the closest wolf strikes. PRECISION fires 3 missile orbs. Frozen Ring: gap filled, no bridges or walls. No slow-mo duel ring; knockouts play anime clips every other turn. SKILL opens the command menu; wider aim arrow; landscape aim camera behind your Zoogi.") },
			{ TEXT("B115"), TEXT("ENERGY BUFF = +1 CRYSTAL"), TEXT("Energy Buff gives you +1 crystal (the circles under SP) on top of the heavier, faster roll. In the Skill Lab the Wolf Pack picks its own targets so you see the whole run.") },
			{ TEXT("B114"), TEXT("ENERGIZED WOLF PACK"), TEXT("Wolf Pack now carries the Energy Buff glow: it flares under your Zoogi as the pack is called and under every wolf as it runs. Skill Lab shows the reach ring and height arc of every PLAY.") },
			{ TEXT("B108"), TEXT("SKILL LAB BUTTON"), TEXT("SKILL LAB is a black button on the home screen now (not a skill). Just the effect and the orbs: pick with the arrows, PLAY, RESET cleans the field, swipe to turn the camera.") },
		};
		const int32 N = UE_ARRAY_COUNT(News);
		const bool bPortN = DH > DW;
		const float Ks = bPortN ? 1.3f : 1.f;
		const FSlateFontInfo BodyF = Font(1, 22.f * Ks);
		const float LineH = 30.f * Ks;
		// the card fills the screen; rows are as tall as their text, the list scrolls inside the card
		const FVector2D Sz(bPortN ? DW - 50.f : FMath::Min(1300.f, DW - 120.f), FMath::Max(420.f, DH - (bPortN ? 230.f : 200.f)));
		const FVector2D TL = PopCard(Out, G, L, Self, Sz, { TEXT("NE"), TEXT("WS") });
		const float Top = 56.f, Bot = 24.f, ViewH = Sz.Y - Top - Bot, TextW = Sz.X - 100.f;
		TArray<float> Hs; float Total = 10.f;
		for (int32 i = 0; i < N; ++i) { const float H = 56.f * Ks + ZWrapLines(News[i].Body, BodyF, TextW) * LineH + 30.f; Hs.Add(H); Total += H; }
		Self->NewsMax = FMath::Max(0.f, Total - ViewH);
		Self->NewsY = FMath::Clamp(Self->NewsY, 0.f, Self->NewsMax);
		Self->NewsBox = FBox2D(TL - FVector2D(0.f, 40.f), TL + Sz);
		Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(FVector2D(Sz.X, ViewH), FSlateLayoutTransform(TL + FVector2D(0.f, Top)))));
		float Y = TL.Y + Top + 10.f - Self->NewsY;
		for (int32 i = 0; i < N; ++i)
		{
			const float H = Hs[i];
			if (Y + H > TL.Y + Top - 10.f && Y < TL.Y + Top + ViewH + 10.f)
			{
				const FVector2D R0(TL.X + 30.f, Y);
				const float TW = ZMeasure(News[i].Tag, Font(0, 26.f * Ks)).X + 30.f;
				ZRect(Out, G, L + 4, R0, FVector2D(TW, 40.f * Ks), i <= 1 ? Mag() : Ink);
				ZTextC(Out, G, L + 5, News[i].Tag, Font(0, 26.f * Ks), R0 + FVector2D(TW * 0.5f, 20.f * Ks), Paper);
				ZTextAt(Out, G, L + 5, News[i].Head, ZFitFont(News[i].Head, 0, 36.f * Ks, Sz.X - TW - 110.f), R0 + FVector2D(TW + 20.f, -2.f), Ink);
				ZWrap(Out, G, L + 5, News[i].Body, BodyF, R0 + FVector2D(0.f, 52.f * Ks), TextW, LineH, Ink);
				if (i < N - 1) ZRect(Out, G, L + 4, R0 + FVector2D(0.f, H - 16.f), FVector2D(Sz.X - 80.f, 3.f), Ink.CopyWithNewOpacity(0.2f));
			}
			Y += H;
		}
		Out.PopClip();
		if (Self->NewsMax > 0.f)
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
		}
	}

'''.replace('\n', nl)
b = b[:a] + NEW + b[e:]
open(R + F, 'wb').write(b.encode('utf-8', 'surrogateescape'))
print('b127a ok')
