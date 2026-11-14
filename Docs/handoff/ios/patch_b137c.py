# b137c (Evan 08:42 "mimic the chest and coin icons"): Grok's original glossy 3D icon set - chests (common / rare / epic) replace the
# flat chest boxes; the GOLD / GEMS chips get the 3D coin / gem. Data/MenuArt/Front/icon_*.png (256 px). -ZoogiFlatIcons = old.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiFront.cpp'
ed1(F, '''	static void ZChest(FSlateWindowElementList& Out, const FGeometry& G, int32 L, FName Type, const FVector2D& Ctr, float W)
	{''', '''	static void ZChest(FSlateWindowElementList& Out, const FGeometry& G, int32 L, FName Type, const FVector2D& Ctr, float W)
	{
		static const bool bFlat = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons"));
		if (!bFlat)
		{	// b137 (Evan): the glossy 3D Zoogi chests
			const TCHAR* Key = Type == FName(TEXT("epic")) ? TEXT("epic") : Type == FName(TEXT("royal")) ? TEXT("rare") : TEXT("common");
			if (UTexture2D* Tx = ZLoadPng(FString(TEXT("Data/MenuArt/Front/icon_chest_")) + Key)) { const float S2 = W * 1.25f; ZTex(Out, G, L + 1, Tx, Ctr - FVector2D(S2, S2) * 0.5f, FVector2D(S2, S2)); return; }
		}''')
ed1(F, '''		ZTag(Out, G, L, Coins, FVector2D(R - Wc, 22.f), 60.f, true, Gold, 30.f);''', '''		ZTag(Out, G, L, Coins, FVector2D(R - Wc, 22.f), 60.f, true, Gold, 30.f);
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons"))) if (UTexture2D* Ic = ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin3d"))) ZTex(Out, G, L + 3, Ic, FVector2D(R - Wc - 46.f, 12.f), FVector2D(80.f, 80.f)); // b137''')
ed1(F, '''		ZTag(Out, G, L, Gems, FVector2D(R - Wg, 22.f), 60.f, true, Mag(), 30.f);''', '''		ZTag(Out, G, L, Gems, FVector2D(R - Wg, 22.f), 60.f, true, Mag(), 30.f);
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons"))) if (UTexture2D* Ic = ZLoadPng(TEXT("Data/MenuArt/Front/icon_gem3d"))) ZTex(Out, G, L + 3, Ic, FVector2D(R - Wg - 46.f, 12.f), FVector2D(80.f, 80.f)); // b137''')
ed1(F, '''		R -= Wc + 26.f;''', '''		R -= Wc + (FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatIcons")) ? 26.f : 72.f); // b137: room for the coin icon''')
print('b137c ok')
