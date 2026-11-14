# b136d (Evan 08:42: "shrink buttons when they get pressed and vibration buzz to know you tapped it"): every front-menu button
# (home, inventory, shop, popups) squashes to 90% and springs back over 0.16 s when tapped, with a short haptic buzz; the button's
# action runs 0.1 s later so the squash is seen. -ZoogiNoPressFx = old instant taps.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '#include "Misc/Paths.h"\n', '#include "Misc/Paths.h"\n#include "ZoogiHaptics.h"\n')
ed1(F, '''	static const FSlateBrush* ZWhite() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	static void ZRect(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& TL, const FVector2D& Sz, const FLinearColor& C, const FSlateBrush* B = nullptr)
	{
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(TL)), B ? B : ZWhite(), ESlateDrawEffect::None, C);
	}''', '''	static const FSlateBrush* ZWhite() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	// b136 (Evan: "shrink buttons when they get pressed"): the pressed button's box; everything drawn inside it squashes about its centre
	static FBox2D GPressBox(ForceInit); static double GPressAt = -10.0;
	static float ZPressK()
	{
		const float Tp = (float)(FPlatformTime::Seconds() - GPressAt);
		return (Tp < 0.f || Tp > 0.16f || !GPressBox.bIsValid) ? 1.f : 1.f - 0.1f * FMath::Sin(PI * Tp / 0.16f);
	}
	static bool ZPress(FVector2D& TL, FVector2D& Sz, float* OutK = nullptr)
	{
		const float K = ZPressK();
		if (K >= 0.999f) return false;
		const FBox2D Bx = GPressBox.ExpandBy(14.f);
		if (TL.X < Bx.Min.X || TL.Y < Bx.Min.Y || TL.X + Sz.X > Bx.Max.X || TL.Y + Sz.Y > Bx.Max.Y) return false; // only what sits inside the button
		const FVector2D C = GPressBox.GetCenter();
		TL = C + (TL - C) * K; Sz *= K;
		if (OutK) *OutK = K;
		return true;
	}
	static void ZRect(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& TL0, const FVector2D& Sz0, const FLinearColor& C, const FSlateBrush* B = nullptr)
	{
		FVector2D TL = TL0, Sz = Sz0; ZPress(TL, Sz);
		FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(TL)), B ? B : ZWhite(), ESlateDrawEffect::None, C);
	}''')
ed1(F, '''	static void ZTextAt(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& TL, const FLinearColor& C)
	{
		FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(ZMeasure(S, F), FSlateLayoutTransform(TL)), S, F, ESlateDrawEffect::None, C);
	}''', '''	static void ZTextAt(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& TL0, const FLinearColor& C)
	{
		FVector2D TL = TL0, Sz = ZMeasure(S, F); float K = 1.f;
		if (ZPress(TL, Sz, &K)) { FSlateFontInfo Fk = F; Fk.Size = FMath::Max(1.f, F.Size * K); Fk.OutlineSettings.OutlineSize = FMath::RoundToInt(F.OutlineSettings.OutlineSize * K); FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(ZMeasure(S, Fk), FSlateLayoutTransform(TL)), S, Fk, ESlateDrawEffect::None, C); return; } // b136
		FSlateDrawElement::MakeText(Out, L, G.ToPaintGeometry(ZMeasure(S, F), FSlateLayoutTransform(TL)), S, F, ESlateDrawEffect::None, C);
	}''')
ed1(F, '''	void Tap(const FVector2D& P)
	{
		for (int32 i = Hits.Num() - 1; i >= 0; --i)
			if (Hits[i].R.IsInside(P)) { TFunction<void()> Fn = Hits[i].Fn; if (Fn) Fn(); return; }''', '''	TFunction<void()> PendingTap; float PendingTapAt = -1.f; // b136: the button squashes first, its action follows
	void Tap(const FVector2D& P)
	{
		static const bool bNoPressFx = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPressFx"));
		for (int32 i = Hits.Num() - 1; i >= 0; --i)
			if (Hits[i].R.IsInside(P))
			{
				TFunction<void()> Fn = Hits[i].Fn;
				if (!Fn) return;
				const FVector2D Bs = Hits[i].R.GetSize();
				if (bNoPressFx || bTest || Bs.X * Bs.Y > DW * DH * 0.5f) { Fn(); return; } // a full-screen dismiss area just runs
				if (PendingTap) { TFunction<void()> Prev = MoveTemp(PendingTap); PendingTap = nullptr; Prev(); } // a quick double tap: the first one runs now
				GPressBox = Hits[i].R; GPressAt = FPlatformTime::Seconds();
				PendingTap = MoveTemp(Fn); PendingTapAt = T + 0.1f;
				if (UObject* C = Ctx.Get()) ZoogiHaptics::Kick(0.25f, C->GetWorld(), TEXT("ui_tap"));
				return;
			}''')
ed1(F, '''		const float Step = bTest ? 1.f / 30.f : FMath::Min(Dt, 0.05f);
		T += Step;''', '''		const float Step = bTest ? 1.f / 30.f : FMath::Min(Dt, 0.05f);
		T += Step;
		if (PendingTap && T >= PendingTapAt) { TFunction<void()> Fn = MoveTemp(PendingTap); PendingTap = nullptr; Fn(); } // b136: after the squash''')
print('b136d ok')
