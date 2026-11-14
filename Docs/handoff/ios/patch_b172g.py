# b172g (Evan: shop "Fill page vertical with larger buttons ... no horizontal scrolling" -> "Category menu first in p5 style" -> "Zig zag";
# "Larger back button"): phone shop opens on a zig-zag P5 category menu; a category fills the screen in wrapped rows (no side scroll).
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiFront.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new,cnt=1):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==cnt: s=s.replace(o,n); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70]), s.count(old)); sys.exit(1)
ed('	float ShopX = 0.f, ShopXVel = 0.f, ShopMax = 0.f;','	float ShopX = 0.f, ShopXVel = 0.f, ShopMax = 0.f;\n	int32 ShopCat = -1; // b172g: phone shop - -1 = the zig-zag category menu, else the open section id')
# bigger BACK on the phone; in a shop category it returns to the menu
ed('''			X += ZTag(Out, G, L, TEXT("BACK"), FVector2D(X, 22.f), 64.f, true, Mag(), 36.f) + 24.f;
			AddHit(FVector2D(10.f, 10.f), FVector2D(X, 90.f), [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Go(0); });''','''			const bool bBigBack = DH > DW; // b172g (Evan: "Larger back button")
			X += ZTag(Out, G, L, TEXT("BACK"), FVector2D(X, bBigBack ? 16.f : 22.f), bBigBack ? 104.f : 64.f, true, Mag(), bBigBack ? 64.f : 36.f) + 24.f;
			AddHit(FVector2D(0.f, 0.f), FVector2D(X + 10.f, bBigBack ? 140.f : 90.f), [Self, bBigBack]() { Self->Sfx(TEXT("UiCancel")); if (bBigBack && Self->Screen == 2 && Self->ShopCat >= 0) { Self->ShopCat = -1; return; } Self->Go(0); });''')
# entry points pick the category on the phone
ed('''[Self]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(5); });''','''[Self]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(5); Self->ShopCat = 5; });''')
ed('''[Self]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(4); });''','''[Self]() { Self->Go(2); Self->ShopX = Self->SectionX.FindRef(4); Self->ShopCat = 4; });''')
ed('''Self->Go(2); Self->ShopX = FMath::Min(Self->ShopMax, Self->SectionX.FindRef(bGems ? 4 : 5)); Self->ShopXVel = 0.f; return; }''','''Self->Go(2); Self->ShopX = FMath::Min(Self->ShopMax, Self->SectionX.FindRef(bGems ? 4 : 5)); Self->ShopXVel = 0.f; Self->ShopCat = bGems ? 4 : 5; return; }''')
ed('''[Self]() { Self->Go(2); Self->ShopX = 0.f; });''','''[Self]() { Self->Go(2); Self->ShopX = 0.f; Self->ShopCat = -1; });''',cnt=2)
ed('''Self->Pop = SZoogiFront::EPop::None; Self->Go(2); Self->ShopX = 0.f; });''','''Self->Pop = SZoogiFront::EPop::None; Self->Go(2); Self->ShopX = 0.f; Self->ShopCat = -1; });''')
# PaintShop: phone menu + wrapped category view
ed('''		UZoogiFrontProfileSubsystem* P = FP();
		if (P) P->EnsureDeals();
		// b66 (playtest): portrait used the whole screen height''','''		UZoogiFrontProfileSubsystem* P = FP();
		if (P) P->EnsureDeals();
		const bool bPhone = DH > DW;
		if (bPhone && ShopCat < 0) { PaintShopMenu(Out, G, L, Self); return; }
		// b66 (playtest): portrait used the whole screen height''')
ed('''		const float HRaw = DH - 110.f - 150.f - 56.f, H = FMath::Min(HRaw, 780.f);
		const float Top = DH > DW ? 150.f + (HRaw - H) * 0.5f : 150.f, Bot = Top + H + 56.f;
		float X = 50.f - ShopX;
		auto Section = [&](int32 Id, const TCHAR* Title)
		{
			Self->SectionX.Add(Id, FMath::Max(0.f, X + ShopX - 50.f));
			ZTag(Out, G, L, Title, FVector2D(X, Top), 56.f, false, Mag(), 32.f);
		};
		auto Card = [&](float W, float Hh, float Y, const FLinearColor& Body, const FString& Price, TFunction<void()> Fn) -> FVector2D
		{
			const FVector2D TL(X, Y);''','''		const float HRaw = DH - 110.f - 150.f - 56.f, H = bPhone ? FMath::Clamp((DH - 420.f) / 3.f - 40.f, 420.f, 600.f) : FMath::Min(HRaw, 780.f);
		const float Top = bPhone ? 170.f : (DH > DW ? 150.f + (HRaw - H) * 0.5f : 150.f), Bot = Top + H + 56.f;
		float X = bPhone ? 50.f : 50.f - ShopX;
		// b172g phone: only the open section is drawn; its cards wrap into rows (each original column keeps its slot)
		int32 CurSec = -1; float SecX = 0.f, WrapX = 0.f, WrapW = 0.f; int32 WrapRow = 0; TMap<int32, FVector2D> WrapAt;
		auto Section = [&](int32 Id, const TCHAR* Title)
		{
			Self->SectionX.Add(Id, FMath::Max(0.f, X + ShopX - 50.f));
			CurSec = Id; SecX = X; WrapX = 0.f; WrapW = 0.f; WrapRow = 0; WrapAt.Reset();
			if (!bPhone) ZTag(Out, G, L, Title, FVector2D(X, Top), 56.f, false, Mag(), 32.f);
			else if (Id == ShopCat) ZTag(Out, G, L, Title, FVector2D(50.f, Top), 70.f, false, Mag(), 44.f);
		};
		auto Place = [&](const FVector2D& Raw, float W) -> FVector2D
		{
			if (!bPhone) return Raw;
			if (CurSec != ShopCat) return FVector2D(Raw.X, Raw.Y + 100000.f); // not this category: off screen
			const int32 Key = FMath::RoundToInt(Raw.X - SecX);
			if (!WrapAt.Contains(Key))
			{
				if (WrapX > 0.f && 50.f + WrapX + W > DW - 30.f) { WrapX = 0.f; ++WrapRow; }
				WrapAt.Add(Key, FVector2D(50.f + WrapX, (float)WrapRow));
				WrapX += W + 22.f;
			}
			const FVector2D Slot = WrapAt[Key];
			return FVector2D(Slot.X, Raw.Y + Slot.Y * (H + 40.f) + 20.f);
		};
		auto Card = [&](float W, float Hh, float Y, const FLinearColor& Body, const FString& Price, TFunction<void()> Fn) -> FVector2D
		{
			const FVector2D TL = Place(FVector2D(X, Y), W);''')
ed('''		Self->ShopMax = FMath::Max(0.f, X + ShopX - DW + 50.f);
		// bottom tab bar''','''		Self->ShopMax = bPhone ? 0.f : FMath::Max(0.f, X + ShopX - DW + 50.f);
		if (bPhone) return; // b172g: no side-scrolling tab bar on the phone - BACK returns to the category menu
		// bottom tab bar''')
ed('''	void PaintShop(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{''','''	// b172g (Evan picked "C zig-zag"): the phone shop opens on one P5 row per category - alternating black / white slabs with a red
	// shadow, sheared, the picture on the opposite side; a tap opens that category
	void PaintShopMenu(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{
		struct FRow { int32 Id; const TCHAR* Title; const TCHAR* Sub; };
		static const FRow Rows[] = { { 0, TEXT("SPECIAL OFFERS"), TEXT("LIMITED DEALS") }, { 1, TEXT("SKINS"), TEXT("NEW LOOKS FOR YOUR BALL") }, { 7, TEXT("WOLF PACKS"), TEXT("PACK SKINS + PACK LEVEL") },
			{ 2, TEXT("CHESTS"), TEXT("COMMON - RARE - EPIC") }, { 3, TEXT("EQUIPMENT"), TEXT("TODAY'S CARD DEALS") }, { 4, TEXT("GEMS"), TEXT("GEM PACKS") }, { 5, TEXT("COINS"), TEXT("COINS FOR GEMS") } };
		const int32 N = UE_ARRAY_COUNT(Rows);
		const float Y0 = 170.f, Gap = 26.f, RH = FMath::Min(300.f, (DH - Y0 - 60.f - Gap * (N - 1)) / N), RW = DW * 0.88f;
		const FLinearColor Red = FLinearColor::FromSRGBColor(FColor(230, 20, 50));
		auto Slab = [&](int32 Lay, const FVector2D& TL, const FVector2D& Sz, float Shear, const FLinearColor& C)
		{
			FSlateDrawElement::MakeBox(Out, Lay, G.ToPaintGeometry(Sz, FSlateLayoutTransform(TL), FSlateRenderTransform(FShear2D(Shear, 0.f)), FVector2D(0.5f, 0.5f)), ZWhite(), ESlateDrawEffect::None, C);
		};
		for (int32 i = 0; i < N; ++i)
		{
			const FRow& R = Rows[i];
			const bool bLeft = i % 2 == 0, bDark = i % 2 == 0;
			const float Y = Y0 + i * (RH + Gap), X0 = bLeft ? 24.f : DW - 24.f - RW, Sh = bLeft ? -0.18f : 0.18f;
			Slab(L + 2, FVector2D(X0 + 14.f, Y + 12.f), FVector2D(RW, RH), Sh, Red);
			Slab(L + 3, FVector2D(X0, Y), FVector2D(RW, RH), Sh, bDark ? Ink : Paper);
			// the picture, on the far side
			const float Pic = RH * 1.02f, PX = bLeft ? X0 + RW - Pic * 0.92f : X0 - Pic * 0.08f;
			const FVector2D PTL(PX, Y + (RH - Pic) * 0.5f);
			if (R.Id == 0) ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Hotstreak")), PTL, FVector2D(Pic, Pic));
			else if (R.Id == 1) ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Blue")), PTL, FVector2D(Pic, Pic));
			else if (R.Id == 7) ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/CardArt/wolf_pack")), PTL + FVector2D(Pic * 0.1f, Pic * 0.1f), FVector2D(Pic * 0.8f, Pic * 0.8f));
			else if (R.Id == 2) ZChest(Out, G, L + 5, FName(TEXT("epic")), PTL + FVector2D(Pic * 0.5f, Pic * 0.55f), Pic * 0.8f);
			else if (R.Id == 3) ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/CardArt/ice_wall")), PTL + FVector2D(Pic * 0.1f, Pic * 0.1f), FVector2D(Pic * 0.8f, Pic * 0.8f));
			else if (R.Id == 4) ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/MenuArt/Front/icon_gem_pile")), PTL, FVector2D(Pic, Pic));
			else ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin_stack")), PTL, FVector2D(Pic, Pic));
			const float TX = bLeft ? X0 + 60.f : X0 + Pic + 10.f, TW = RW - Pic - 70.f;
			ZTextAt(Out, G, L + 6, R.Title, ZFitFont(R.Title, 0, FMath::Min(110.f, RH * 0.42f), TW), FVector2D(TX, Y + RH * 0.1f), bDark ? Paper : Ink);
			ZTextAt(Out, G, L + 6, R.Sub, Font(1, FMath::Min(34.f, RH * 0.13f)), FVector2D(TX + 4.f, Y + RH * 0.68f), Red);
			const int32 Id = R.Id;
			AddHit(FVector2D(X0, Y), FVector2D(RW, RH), [Self, Id]() { Self->ShopCat = Id; Self->ShopX = 0.f; Self->ShopXVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.7f, 1.1f); });
		}
		Self->ShopMax = 0.f;
	}

	void PaintShop(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
