# b156c (Evan Nov 6 07:35 "Vibrations on all button taps and shrink animation for all buttons, people need to know they pressed
# the buttons"): every UZoogiButton (all UMG menus / HUD buttons) squashes to 90% with a haptic tick and pops back (1.04 -> 1);
# the P5 mode / map list, its BACK tag, the results buttons and the in-match command menu (ATTACK / SKILL / ITEM / GUARD,
# skill rows) squash + vibrate too. The front menu already did (b136). -ZoogiNoPressFx = off.
import os, sys, re
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
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
    print('b156c ok', len(EDITS))

H = 'Public/ZoogiMenuWidget.h'
ed1(H, '	UZoogiButton() { InitIsFocusable(false); }', '''	UZoogiButton() { InitIsFocusable(false); }
	// b156 (Evan: "Vibrations on all button taps and shrink animation for all buttons"): press = 90% + haptic tick, release = pop back
	virtual void SynchronizeProperties() override;
	UFUNCTION() void ZPressFx();
	UFUNCTION() void ZReleaseFx();
	FWidgetTransform ZRest; FVector2D ZRestPivot = FVector2D(0.5f, 0.5f); bool bZPressed = false; double ZPressAt = 0.0;''')
M = 'Private/ZoogiMenuWidget.cpp'
ed1(M, '#include "ZoogiP5Style.h"', '''#include "ZoogiP5Style.h"
#include "ZoogiHaptics.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// b156: press feedback for every UZoogiButton
void UZoogiButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPressFx"));
	if (bOff) return;
	if (!OnPressed.IsAlreadyBound(this, &UZoogiButton::ZPressFx)) OnPressed.AddDynamic(this, &UZoogiButton::ZPressFx);
	if (!OnReleased.IsAlreadyBound(this, &UZoogiButton::ZReleaseFx)) OnReleased.AddDynamic(this, &UZoogiButton::ZReleaseFx);
}
void UZoogiButton::ZPressFx()
{
	if (!bZPressed) { ZRest = GetRenderTransform(); ZRestPivot = GetRenderTransformPivot(); bZPressed = true; }
	FWidgetTransform Tr = ZRest; Tr.Scale = ZRest.Scale * 0.9f;
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderTransform(Tr);
	ZoogiHaptics::Kick(0.25f, GetWorld(), TEXT("ui_tap"));
	ZPressAt = FPlatformTime::Seconds();
	TWeakObjectPtr<UZoogiButton> W = this; // springs back even when the release never comes (the press rebuilt / hid the menu)
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([W](float) -> bool
	{
		UZoogiButton* B = W.Get(); if (!B || !B->bZPressed) return false;
		if (FPlatformTime::Seconds() - B->ZPressAt > 0.6) { B->ZReleaseFx(); return false; }
		return true;
	}), 0.1f);
}
void UZoogiButton::ZReleaseFx()
{
	if (!bZPressed) return;
	bZPressed = false;
	FWidgetTransform Tr = ZRest; Tr.Scale = ZRest.Scale * 1.04f; SetRenderTransform(Tr); // a little pop past rest
	TWeakObjectPtr<UZoogiButton> W = this; const FWidgetTransform Rest = ZRest; const FVector2D Pv = ZRestPivot;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([W, Rest, Pv](float) -> bool
	{
		if (UZoogiButton* B = W.Get()) if (!B->bZPressed) { B->SetRenderTransform(Rest); B->SetRenderTransformPivot(Pv); }
		return false;
	}), 0.08f);
}''')

S = 'Private/ZoogiP5Style.cpp'
ed1(S, '#include "ZoogiUIFx.h"', '#include "ZoogiUIFx.h"\n#include "ZoogiHaptics.h"')
ed1(S, '	mutable FBox2D ArtRect = FBox2D(ForceInit), BackRect = FBox2D(ForceInit);', '	mutable FBox2D ArtRect = FBox2D(ForceInit), BackRect = FBox2D(ForceInit);\n	int32 PrIdx = -3; float PrT = -10.f; // b156: the pressed bar squashes')
ed1(S, '''		if (BackRect.bIsValid && BackRect.IsInside(D)) { Sfx(TEXT("UiCancel"), 0.8f);''', '''		auto Buzz = [this]() { if (UObject* C = Spec.Ctx.Get()) ZoogiHaptics::Kick(0.25f, C->GetWorld(), TEXT("ui_tap")); }; // b156
		if (BackRect.bIsValid && BackRect.IsInside(D)) { PrIdx = -2; PrT = T; Buzz(); Sfx(TEXT("UiCancel"), 0.8f);''')
ed1(S, '''				if (i == Sel && T - SelT > 0.12f) Confirm(); else Select(i);''', '''				PrIdx = i; PrT = T; Buzz();
				if (i == Sel && T - SelT > 0.12f) Confirm(); else Select(i);''')
ed1(S, '''				const bool bSel = i == Sel;
				const float In = EaseOutBack((T - 0.15f - 0.055f * i) / 0.3f);''', '''				const bool bSel = i == Sel;
				const float Kp = PrIdx == i ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PrT) / 0.22f, 0.f, 1.f) : 1.f; // b156: press squash
				const float In = EaseOutBack((T - 0.15f - 0.055f * i) / 0.3f);''')
ed1(S, 'Box(Out, G, L, Ctr + J + FVector2D(18.f, 14.f), FVector2D(BW + 60.f, BH + 34.f), Ang - 3.f, SelCol);', 'Box(Out, G, L, Ctr + J + FVector2D(18.f, 14.f), FVector2D(BW + 60.f, BH + 34.f) * Kp, Ang - 3.f, SelCol);')
ed1(S, '''					Box(Out, G, L + 1, Ctr + FVector2D(10.f, 10.f), FVector2D(BW, BH), Ang, Blk);
					Box(Out, G, L + 2, Ctr, FVector2D(BW, BH), Ang, Wht);''', '''					Box(Out, G, L + 1, Ctr + FVector2D(10.f, 10.f), FVector2D(BW, BH) * Kp, Ang, Blk);
					Box(Out, G, L + 2, Ctr, FVector2D(BW, BH) * Kp, Ang, Wht);''')
ed1(S, '''					Box(Out, G, L + 1, Ctr + FVector2D(9.f, 9.f), FVector2D(BW, BH), Ang, Acc.CopyWithNewOpacity(0.85f));
					Box(Out, G, L + 2, Ctr, FVector2D(BW, BH), Ang, Blk);''', '''					Box(Out, G, L + 1, Ctr + FVector2D(9.f, 9.f), FVector2D(BW, BH) * Kp, Ang, Acc.CopyWithNewOpacity(0.85f));
					Box(Out, G, L + 2, Ctr, FVector2D(BW, BH) * Kp, Ang, Blk);''')
ed1(S, '''			Box(Out, G, L, BC + FVector2D(8.f, 8.f), FVector2D(230.f, 78.f), 7.f, Acc);
			Box(Out, G, L + 1, BC, FVector2D(230.f, 78.f), 7.f, Wht);''', '''			const float Kb = PrIdx == -2 ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PrT) / 0.22f, 0.f, 1.f) : 1.f; // b156
			Box(Out, G, L, BC + FVector2D(8.f, 8.f), FVector2D(230.f, 78.f) * Kb, 7.f, Acc);
			Box(Out, G, L + 1, BC, FVector2D(230.f, 78.f) * Kb, 7.f, Wht);''')
# results buttons
ed1(S, 'for (int32 i = 0; i < BtnRects.Num(); ++i) if (BtnRects[i].bIsValid && BtnRects[i].IsInside(D)) { Confirm(i); break; }',
       'for (int32 i = 0; i < BtnRects.Num(); ++i) if (BtnRects[i].bIsValid && BtnRects[i].IsInside(D)) { if (UObject* C = Spec.Ctx.Get()) ZoogiHaptics::Kick(0.3f, C->GetWorld(), TEXT("ui_tap")); Confirm(i); break; } // b156: haptic')
ed1(S, '''				Box(Out, G, L + 2, C, FVector2D(BtW, BtH), Ang, Face);
				Text(Out, G, L + 3, Bt.Label''', '''				const float Kq = b == Pressed ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PressT) / 0.22f, 0.f, 1.f) : 1.f; // b156: press squash
				Box(Out, G, L + 2, C, FVector2D(BtW, BtH) * Kq, Ang, Face);
				Text(Out, G, L + 3, Bt.Label''')

C = 'Private/ZoogiP5Command.cpp'
ed1(C, '#include "ZoogiSkillLab.h"', '#include "ZoogiSkillLab.h"\n#include "ZoogiHaptics.h"')
ed1(C, '		int32 Hover = -1;', '		int32 Hover = -1;\n		int32 PrKind = -1, PrIdx = -9; float PrT = -10.f; // b156: the pressed plate squashes\n		float PressK(int32 K, int32 I) const { return (PrKind == K && PrIdx == I) ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PrT) / 0.22f, 0.f, 1.f) : 1.f; }')
ed1(C, '''			if (bMini) { if (Kind == 0 && OnMini) { OnMini(); return Took(); } return FReply::Unhandled(); } // b73: anything else is the aim drag''', '''			if (Kind >= 0) { PrKind = Kind; PrIdx = Idx; PrT = T; if (UWorld* Wd = GWorld) ZoogiHaptics::Kick(0.25f, Wd, TEXT("ui_tap")); } // b156: squash + haptic
			if (bMini) { if (Kind == 0 && OnMini) { OnMini(); return Took(); } return FReply::Unhandled(); } // b73: anything else is the aim drag''')
ed1(C, '				const float W = 250.f * S, H = 70.f * S, Ang = -8.f;', '				const float W = 250.f * S * PressK(0, Skill), H = 70.f * S * PressK(0, Skill), Ang = -8.f;')
ed1(C, '					const float W = En.W * S, H = 74.f * S;', '					const float W = En.W * S * PressK(0, i), H = 74.f * S * PressK(0, i);')
ed1(C, 'Plate(Out, Geo, L + 1, C, PW - 30.f * S, RowH - 10.f * S, -2.f, 10.f * S, bHov ? Red', 'Plate(Out, Geo, L + 1, C, (PW - 30.f * S) * PressK(1, r), (RowH - 10.f * S) * PressK(1, r), -2.f, 10.f * S, bHov ? Red')
run()
