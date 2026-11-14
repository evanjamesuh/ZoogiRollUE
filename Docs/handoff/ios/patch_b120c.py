# b120c (Evan 17:26: "update map selection map pictures. Resize for portrait orientation - fill screen"):
#  - portrait MAP SELECT: the selected map's picture fills the whole screen (cover crop of Content/Data/MenuArt/ModeSelect/
#    ms_map_<key>_p.jpg - new portrait arena shots), zoom-settle + flash on every change, dark gradient under the list;
#    the list sits in the lower half with taller bars; caption tag + GO! sticker above the list. Landscape keeps the framed card.
#  - FItem.ArtFull (the portrait picture); missing -> the old framed card layout
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiP5Style.h'; S = 'Private/ZoogiP5Style.cpp'; M = 'Private/ZoogiModeSelect.cpp'
ed1(H, '''		TWeakObjectPtr<UTexture2D> Art;
		FLinearColor Col = FLinearColor::White;''', '''		TWeakObjectPtr<UTexture2D> Art;
		TWeakObjectPtr<UTexture2D> ArtFull; // b120: portrait full-screen picture (map select)
		FLinearColor Col = FLinearColor::White;''')
ed1(M, '''			UTexture2D* T = Art(Maps[i].Art); It.Art = T; if (T) W->Keep.Add(T);''', '''			UTexture2D* T = Art(Maps[i].Art); It.Art = T; if (T) W->Keep.Add(T);
			UTexture2D* Tf = Art(FString(Maps[i].Art) + TEXT("_p")); It.ArtFull = Tf; if (Tf) W->Keep.Add(Tf); // b120: portrait full-screen shot''')
ed1(S, '''	static void Tiled(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL, const FVector2D& Size, float Tile, const FLinearColor& Col)''', '''	/** b120: the texture cover-cropped to Size (centred), no rotation */
	static void TexCover(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& Ctr, const FVector2D& Size, const FLinearColor& Col)
	{
		if (!T || Size.X < 1.f || Size.Y < 1.f) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = Size;
		const float Ta = (float)T->GetSizeX() / FMath::Max(1, T->GetSizeY()), Ba = Size.X / Size.Y;
		if (Ta > Ba) { const float W = Ba / Ta; B.SetUVRegion(FBox2f(FVector2f(0.5f - W * 0.5f, 0.f), FVector2f(0.5f + W * 0.5f, 1.f))); }
		else { const float Hh = Ta / Ba; B.SetUVRegion(FBox2f(FVector2f(0.f, 0.5f - Hh * 0.5f), FVector2f(1.f, 0.5f + Hh * 0.5f))); }
		Box(Out, G, L, Ctr, Size, 0.f, Col, &B);
	}
	static void Tiled(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& TL, const FVector2D& Size, float Tile, const FLinearColor& Col)''')
# full-bleed picture right after the background
ed1(S, '''		L += 6;

		// 2) the selected item's art: tilted torn-edge frame slamming in on every change, accent burst behind it
		{''', '''		L += 6;

		// b120 (Evan: "map pictures - resize for portrait, fill screen"): portrait map select = the selected map fills the screen
		UTexture2D* FullArt = (bPortrait && Spec.bMapList && Spec.Items.IsValidIndex(Sel)) ? Spec.Items[Sel].ArtFull.Get() : nullptr;
		const int32 NFull = Spec.Items.Num();
		const float StepFull = FMath::Clamp(DH * 0.5f / FMath::Max(1, NFull), 84.f, 122.f);
		const float ListBottomFull = DH - 300.f, Y0Full = ListBottomFull - (NFull - 1) * StepFull;
		if (FullArt)
		{
			const float K = FMath::Clamp((T - SelT) / 0.28f, 0.f, 1.f);
			const float Zm = 1.f + 0.07f * FMath::Square(1.f - K);
			TexCover(Out, G, L, FullArt, FVector2D(DW * 0.5f, DH * 0.5f), FVector2D(DW, DH) * Zm, Wht);
			if (K < 1.f) Box(Out, G, L + 1, FVector2D(DW, DH) * 0.5f, FVector2D(DW, DH), 0.f, Wht.CopyWithNewOpacity(0.45f * FMath::Square(1.f - K))); // change flash
			// top band (title / tags) and a gradient under the list
			for (int32 s = 0; s < 8; ++s) Box(Out, G, L + 1, FVector2D(DW * 0.5f, s * 40.f + 20.f), FVector2D(DW, 40.f), 0.f, Blk.CopyWithNewOpacity(0.5f * (1.f - s / 8.f)));
			const float G0 = Y0Full - 260.f;
			for (int32 s = 0; s < 16; ++s) { const float Y = G0 + (DH - G0) * s / 16.f; Box(Out, G, L + 1, FVector2D(DW * 0.5f, Y + (DH - G0) / 32.f), FVector2D(DW, (DH - G0) / 16.f + 2.f), 0.f, Blk.CopyWithNewOpacity(FMath::Min(0.86f, 0.06f + 0.06f * s))); }
			// accent edge stripes (P5 frame)
			Box(Out, G, L + 2, FVector2D(10.f, DH * 0.5f), FVector2D(20.f, DH), 0.f, SelCol);
			Box(Out, G, L + 2, FVector2D(DW - 10.f, DH * 0.5f), FVector2D(20.f, DH), 0.f, SelCol);
			ArtRect = FBox2D(FVector2D(0.f, 300.f), FVector2D(DW, G0));
			if (!Spec.Items[Sel].Sub.IsEmpty())
			{
				const FSlateFontInfo SF = Font(0, 36.f);
				const FVector2D SS = Measure(Spec.Items[Sel].Sub, SF);
				const FVector2D Cc(70.f + SS.X * 0.5f + 20.f, Y0Full - 150.f);
				Box(Out, G, L + 3, Cc + FVector2D(7.f, 7.f), SS + FVector2D(40.f, 16.f), -4.f, SelCol);
				Box(Out, G, L + 4, Cc, SS + FVector2D(40.f, 16.f), -4.f, Blk);
				Text(Out, G, L + 5, Spec.Items[Sel].Sub, SF, Cc, -4.f, Wht);
			}
			if (!bConfirmed)
			{
				const float P = 1.f + 0.08f * FMath::Sin(T * 10.f);
				const FVector2D Gc(DW - 170.f, Y0Full - 190.f);
				Tex(Out, G, L + 3, BurstTex(), Gc, FVector2D(190.f, 190.f) * P, T * 40.f, Blk);
				Tex(Out, G, L + 4, BurstTex(), Gc, FVector2D(170.f, 170.f) * P, T * 40.f, Acc);
				Text(Out, G, L + 5, TEXT("GO!"), Font(3, 70.f * P, 3.f), Gc, -12.f, Wht);
			}
			L += 6;
		}

		// 2) the selected item's art: tilted torn-edge frame slamming in on every change, accent burst behind it
		if (!FullArt)
		{''')
ed1(S, '''			const float Step = bPortrait ? FMath::Min(150.f, (DH - 1080.f - 260.f) / FMath::Max(1, N)) : FMath::Min(132.f, (N > 8 ? 620.f : 680.f) / FMath::Max(1, N)); // Oct 31: 9-map list leaves room for the hint
			const float X0 = bPortrait ? 90.f : DW * 0.535f, Y0 = bPortrait ? 1010.f : 240.f;''', '''			const float Step = FullArt ? StepFull : bPortrait ? FMath::Min(150.f, (DH - 1080.f - 260.f) / FMath::Max(1, N)) : FMath::Min(132.f, (N > 8 ? 620.f : 680.f) / FMath::Max(1, N)); // Oct 31: 9-map list leaves room for the hint
			const float X0 = bPortrait ? 90.f : DW * 0.535f, Y0 = FullArt ? Y0Full : bPortrait ? 1010.f : 240.f; // b120: full-screen picture -> the list in the lower half''')
print('b120c ok')
