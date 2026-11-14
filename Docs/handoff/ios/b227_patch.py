# b227 (Claude): Evan picked ITEMS option A - the carried count in a gold pill, top-right of every item card (+ the Golden Apple card)
import os, sys
SRC = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(path, pairs):
    p = os.path.join(SRC, path); s = open(p).read()
    for a, b in pairs:
        assert a in s, (path, a[:100]); s = s.replace(a, b, 1)
    open(p, 'w').write(s)
patch('Private/ZoogiFront.cpp', [
('int32 Kind = 0; int32 HandSlot = -1; };', 'int32 Kind = 0; int32 HandSlot = -1; int32 Count = -1; /* b227: items - how many you carry */ };'),
('''				{ TEXT("disc"), TEXT("HALO DISC"), TEXT("Throw the halo like a boomerang - it cuts through everyone on the way out and on the way back."), 2 } };''',
 '''				{ TEXT("disc"), TEXT("HALO DISC"), TEXT("Throw the halo like a boomerang - it cuts through everyone on the way out and on the way back."), 2 },
				{ TEXT("apple"), TEXT("GOLDEN APPLE"), TEXT("Tap an orb in reach: a golden apple is lobbed there and its big blast throws everything away."), 1 } }; // b227: the Golden Apple is an item too'''),
('''				I.Sub = FString::Printf(TEXT("%s ITEM  x%d"), ZRarityName(W[i].R), P && P->Data().bItemsSeeded ? Have : ((FCString::Strcmp(W[i].K, TEXT("sword")) == 0 || FCString::Strcmp(W[i].K, TEXT("staff")) == 0) ? 3 : 5)); // b137: the collection count (b143d: start 5)
				I.Icon = FString(TEXT("Data/MenuArt/Weapons/w_")) + W[i].K; I.bOwned = true;''',
 '''				I.Count = P && P->Data().bItemsSeeded ? Have : ((FCString::Strcmp(W[i].K, TEXT("sword")) == 0 || FCString::Strcmp(W[i].K, TEXT("staff")) == 0 || FCString::Strcmp(W[i].K, TEXT("apple")) == 0) ? 3 : 5); // b227
				I.Sub = FString::Printf(TEXT("%s ITEM  x%d"), ZRarityName(W[i].R), I.Count); // b137: the collection count (b143d: start 5)
				I.Icon = FCString::Strcmp(W[i].K, TEXT("apple")) == 0 ? FString(TEXT("Data/CardArt/golden_apple")) : FString(TEXT("Data/MenuArt/Weapons/w_")) + W[i].K; I.bOwned = true;'''),
('''		if (I.Kind == 3 && I.HandSlot >= 0)
		{
			// b63: hand slot badge (top right)''',
 '''		if (I.Kind == 2 && I.Count >= 0)
		{	// b227 (Evan picked ITEMS option A): how many you carry - a gold-rimmed pill, top-right
			const FString Cn = FString::Printf(TEXT("x%d"), I.Count);
			const float Fs = FMath::Max(24.f, W * 0.15f), Pw = ZMeasure(Cn, Font(0, Fs)).X + Fs * 0.7f, Ph = Fs * 1.35f;
			const FVector2D Pt = TL + FVector2D(W - Pw - 8.f, 14.f);
			ZRect(Out, G, L + 6, Pt - FVector2D(3.f, 3.f), FVector2D(Pw + 6.f, Ph + 6.f), I.Count > 0 ? FLinearColor(1.f, 0.77f, 0.24f, 1.f) : FLinearColor(0.45f, 0.45f, 0.5f, 1.f));
			ZRect(Out, G, L + 7, Pt, FVector2D(Pw, Ph), FLinearColor(0.06f, 0.05f, 0.16f, 1.f));
			ZTextC(Out, G, L + 8, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), I.Count > 0 ? Paper : FLinearColor(0.6f, 0.6f, 0.65f));
		}
		if (I.Kind == 3 && I.HandSlot >= 0)
		{
			// b63: hand slot badge (top right)'''),
('''		if (Img) ZTexFit(Out, G, L + 5, Img, TL + FVector2D(40.f, 46.f), FVector2D(230.f, 226.f));''',
 '''		if (Img) ZTexFit(Out, G, L + 5, Img, TL + FVector2D(40.f, 46.f), FVector2D(230.f, 226.f));
		if (I.Kind == 2 && I.Count >= 0)
		{	// b227: the carried count on the item card too (option A)
			const FString Cn = FString::Printf(TEXT("x%d"), I.Count);
			const float Pw = ZMeasure(Cn, Font(0, 40.f)).X + 28.f, Ph = 54.f; const FVector2D Pt = TL + FVector2D(30.f + 250.f - Pw - 10.f, 46.f);
			ZRect(Out, G, L + 6, Pt - FVector2D(3.f, 3.f), FVector2D(Pw + 6.f, Ph + 6.f), I.Count > 0 ? FLinearColor(1.f, 0.77f, 0.24f, 1.f) : FLinearColor(0.45f, 0.45f, 0.5f, 1.f));
			ZRect(Out, G, L + 7, Pt, FVector2D(Pw, Ph), FLinearColor(0.06f, 0.05f, 0.16f, 1.f));
			ZTextC(Out, G, L + 8, Cn, Font(0, 40.f), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), Paper);
		}'''),
('''			{ TEXT("B226"),''',
 '''			{ TEXT("B227"), TEXT("ITEM COUNTS"), TEXT("Every item card in the inventory shows how many you carry in a gold pill (top-right), and the Golden Apple has its own card. Skills have no counters.") },
			{ TEXT("B226"),'''),
])
print('patched b227')
