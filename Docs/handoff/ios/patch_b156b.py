# b156b (Evan Nov 6): FRONT
#  - 07:27 "Start the shop and level system now ... coins and gems only": coloured shop tiles (no white), real gem / coin art,
#    a BUY pop-up on every purchase (GET GEMS / GET COINS when short), level locks (UNLOCK LV badge), a LEVEL UP pop-up, COINS naming
#  - 07:33 "Top button doesn't work and scrolling glitches needs to be smooth no exceptions" + 07:34 "Bigger x button":
#    NEWS: the drag area is the list only (TOP / CLOSE / X fire like every button), finger-follow scrolling with release
#    velocity from the last 100 ms, TOP eases up; the shop strip scrolls the same way; X 180 x 150.
import os, sys
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
                import re  # mixed line endings: each newline in the anchor matches \r?\n
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b156b ok', len(EDITS))

F = 'Private/ZoogiFront.cpp'
# ---- draw helpers (before the shared settings rows namespace)
ed1(F, '// ================================================================================================ shared settings rows', '''namespace ZoogiFrontDraw
{
	// b156 (Evan: "add color to white backgrounds", the shop like his reference): tile colours
	static FLinearColor ZTile(int32 K)
	{
		switch (((K % 6) + 6) % 6)
		{
		case 0: return FLinearColor::FromSRGBColor(FColor(46, 184, 60));
		case 1: return FLinearColor::FromSRGBColor(FColor(28, 122, 240));
		case 2: return FLinearColor::FromSRGBColor(FColor(150, 48, 226));
		case 3: return FLinearColor::FromSRGBColor(FColor(246, 138, 12));
		case 4: return FLinearColor::FromSRGBColor(FColor(84, 40, 214));
		default: return FLinearColor::FromSRGBColor(FColor(0, 168, 186));
		}
	}
	// b156: the player level that opens a shop item (owned items are never locked)
	static int32 ZSkinLockLv(int32 N) { return 1 + N; }
	static int32 ZPackLockLv(int32 N, int32 Price) { return Price == 0 ? 1 : 2 + N; }
	static int32 ZChestLockLv(FName Id) { return Id == FName(TEXT("epic")) ? 5 : Id == FName(TEXT("royal")) ? 3 : 1; }
	static constexpr int32 HotstreakLockLv = 4;
	static UTexture2D* ZCoinIcon() { return ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin3d")); }
	static UTexture2D* ZGemIcon() { return ZLoadPng(TEXT("Data/MenuArt/Front/icon_gem3d")); }
}

// ================================================================================================ shared settings rows''')
# ---- popups + state
ed1(F, 'enum class EPop : uint8 { None, Item, Login, Chest, Tasks, News, Settings };', 'enum class EPop : uint8 { None, Item, Login, Chest, Tasks, News, Settings, Confirm, LevelUp };')
ed1(F, 'float NewsY = 0.f, NewsMax = 0.f, NewsVel = 0.f; FBox2D NewsBox = FBox2D(ForceInit); // b127: the NEWS list scrolls', '''float NewsY = 0.f, NewsMax = 0.f, NewsVel = 0.f; FBox2D NewsBox = FBox2D(ForceInit); // b127: the NEWS list scrolls
	// b156 (Evan: "scrolling ... needs to be smooth no exceptions"): the content follows the finger from an anchor; the release
	// velocity comes from the last 100 ms of samples (not one frame); TOP eases up
	float DragAnchorV = 0.f; FVector2D DragAnchorP = FVector2D::ZeroVector; bool bAnchored = false, bNewsToTop = false;
	TArray<TPair<double, float>> VelS;
	void AddVel(float V) { VelS.Add(TPair<double, float>(FPlatformTime::Seconds(), V)); if (VelS.Num() > 16) VelS.RemoveAt(0); }
	float RelVel() const
	{
		if (VelS.Num() < 2) return 0.f;
		const double Now = FPlatformTime::Seconds();
		const TPair<double, float>& Last = VelS.Last();
		if (Now - Last.Key > 0.08) return 0.f; // the finger stopped before letting go
		int32 i0 = VelS.Num() - 1; while (i0 > 0 && Last.Key - VelS[i0 - 1].Key <= 0.1) --i0;
		const double Dt = Last.Key - VelS[i0].Key;
		if (Dt < 0.012) return 0.f;
		return FMath::Clamp((float)((Last.Value - VelS[i0].Value) / Dt), -7000.f, 7000.f);
	}
	// b156: the BUY pop-up + LEVEL UP pop-up
	FString CfmTitle, CfmName; TWeakObjectPtr<UTexture2D> CfmIcon; FName CfmChest; FLinearColor CfmCol = FLinearColor::White; int32 CfmPrice = 0, CfmKind = 0; TFunction<void()> CfmFn;
	int32 LvShown = 0;
	void AskBuy(const FString& Title, const FString& Name, UTexture2D* Icon, const FLinearColor& Col, int32 Price, int32 Kind, TFunction<void()> Fn, FName Chest = NAME_None)
	{
		CfmTitle = Title; CfmName = Name; CfmIcon = Icon; CfmCol = Col; CfmPrice = Price; CfmKind = Kind; CfmFn = MoveTemp(Fn); CfmChest = Chest;
		Pop = EPop::Confirm; PopT = T;
		UE_LOG(LogZoogi, Log, TEXT("FRONT buy pop-up: %s - %d %s"), *Name, Price, Kind == 1 ? TEXT("gems") : TEXT("coins"));
	}''')
# ---- input: smooth drag
ed1(F, '''		if (DragKind == 0) { bDown = false; Tap(DownAt); return FReply::Handled(); }
		bDown = true;''', '''		if (DragKind == 0) { bDown = false; Tap(DownAt); return FReply::Handled(); }
		VelS.Reset(); bAnchored = false; bNewsToTop = false;
		if (DragKind == 3) { bDrag = true; bAnchored = true; DragAnchorP = DownAt; DragAnchorV = NewsY; NewsVel = 0.f; AddVel(NewsY); } // b156: the list follows from the first pixel
		if (DragKind == 2) ShopXVel = 0.f;
		bDown = true;''')
ed1(F, 'if (DragKind == 2) { ShopX = FMath::Clamp(ShopX - Dx, 0.f, ShopMax); ShopXVel = -Dx / FMath::Max(0.008f, FSlateApplication::Get().GetDeltaTime()); }',
       'if (DragKind == 2) { if (!bAnchored) { bAnchored = true; DragAnchorP = P; DragAnchorV = ShopX; } ShopX = FMath::Clamp(DragAnchorV - (P.X - DragAnchorP.X), 0.f, ShopMax); AddVel(ShopX); } // b156: no jump when the drag starts')
ed1(F, 'if (DragKind == 3) { const float Dy = P.Y - LastAt.Y; NewsY = FMath::Clamp(NewsY - Dy, 0.f, NewsMax); NewsVel = -Dy / FMath::Max(0.008f, FSlateApplication::Get().GetDeltaTime()); }',
       'if (DragKind == 3) { NewsY = FMath::Clamp(DragAnchorV - (P.Y - DragAnchorP.Y), 0.f, NewsMax); AddVel(NewsY); }')
ed1(F, '''		if (!bDrag) Tap(P);
		else if (DragKind != 2) ShopXVel = 0.f;
		if (!bDrag || DragKind != 3) NewsVel = 0.f;''', '''		if (!bDrag) Tap(P);
		ShopXVel = (bDrag && DragKind == 2) ? RelVel() : 0.f; // b156: release velocity from the last 100 ms
		NewsVel = (bDrag && DragKind == 3) ? RelVel() : 0.f;''')
ed1(F, 'if (!bDown && FMath::Abs(NewsVel) > 1.f) { NewsY = FMath::Clamp(NewsY + NewsVel * Step, 0.f, NewsMax); NewsVel *= FMath::Exp(-5.f * Step); } // b127: a flick keeps scrolling',
       '''if (!bDown && FMath::Abs(NewsVel) > 4.f) { const float Ny = NewsY + NewsVel * Step; NewsY = FMath::Clamp(Ny, 0.f, NewsMax); NewsVel = Ny != NewsY ? 0.f : NewsVel * FMath::Exp(-3.2f * Step); } else if (!bDown) NewsVel = 0.f; // b127 / b156: a flick glides
		if (bNewsToTop && !bDown) { NewsY = FMath::FInterpTo(NewsY, 0.f, Step, 9.f); if (NewsY < 0.5f) { NewsY = 0.f; bNewsToTop = false; } } // b156: TOP eases up''')
ed1(F, 'if (!bDown && FMath::Abs(ShopXVel) > 1.f) { ShopX = FMath::Clamp(ShopX + ShopXVel * Step, 0.f, ShopMax); ShopXVel *= FMath::Exp(-5.f * Step); }',
       'if (!bDown && FMath::Abs(ShopXVel) > 4.f) { const float Nx = ShopX + ShopXVel * Step; ShopX = FMath::Clamp(Nx, 0.f, ShopMax); ShopXVel = Nx != ShopX ? 0.f : ShopXVel * FMath::Exp(-3.2f * Step); } else if (!bDown) ShopXVel = 0.f;')
# ---- LEVEL UP detection (tick)
ed1(F, '''		ZoogiFront::FView& V = ZoogiFront::View();
		V.AliveT = FPlatformTime::Seconds();''', '''		{	// b156: LEVEL UP pop-up, once per new level (after a match, a trick shot, a card upgrade)
			static int32 GSeenLv = -1;
			if (UZoogiFrontProfileSubsystem* Pl = FP())
			{
				const int32 Lv = Pl->Data().Level;
				if (GSeenLv < 0 || Lv < GSeenLv) GSeenLv = Lv;
				else if (Lv > GSeenLv && Pop == EPop::None && T > 1.f)
				{
					GSeenLv = Lv; LvShown = Lv; Pop = EPop::LevelUp; PopT = T;
					Sfx(TEXT("UiConfirm"), 1.f, 1.2f);
					if (UObject* C = Ctx.Get()) ZoogiHaptics::Kick(0.6f, C->GetWorld(), TEXT("level_up"));
					UE_LOG(LogZoogi, Log, TEXT("FRONT LEVEL UP -> %d"), Lv);
				}
			}
		}
		ZoogiFront::FView& V = ZoogiFront::View();
		V.AliveT = FPlatformTime::Seconds();''')
# ---- popup dispatch + outside tap
ed1(F, 'if (Self->Pop == SZoogiFront::EPop::Item || Self->Pop == SZoogiFront::EPop::Tasks', 'if (Self->Pop == SZoogiFront::EPop::Item || Self->Pop == SZoogiFront::EPop::Confirm || Self->Pop == SZoogiFront::EPop::LevelUp || Self->Pop == SZoogiFront::EPop::Tasks')
ed1(F, '''			else if (Pop == EPop::Settings) PaintSettings(Out, G, L, Self);
			else PaintChest(Out, G, L, Self);''', '''			else if (Pop == EPop::Settings) PaintSettings(Out, G, L, Self);
			else if (Pop == EPop::Confirm) PaintConfirm(Out, G, L, Self);
			else if (Pop == EPop::LevelUp) PaintLevelUp(Out, G, L, Self);
			else PaintChest(Out, G, L, Self);''')
# ---- bigger X, NEWS drag area = the list, TOP eases
ed1(F, 'const FVector2D CT(TL.X + Sz.X - 104.f, TL.Y - 58.f), CS(130.f, 104.f); // b142m (Evan): a bigger X', 'const FVector2D CT(TL.X + Sz.X - 150.f, TL.Y - 86.f), CS(180.f, 150.f); // b142m / b156 (Evan): a bigger X')
ed1(F, 'ZTextC(Out, G, L + 5, TEXT("X"), Font(0, 70.f), CT + CS * 0.5f, Paper);', 'ZTextC(Out, G, L + 5, TEXT("X"), Font(0, 104.f), CT + CS * 0.5f, Paper);')
ed1(F, 'Self->NewsBox = FBox2D(TL - FVector2D(0.f, 40.f), TL + Sz);', 'Self->NewsBox = FBox2D(TL + FVector2D(0.f, Top), TL + FVector2D(Sz.X, Top + ViewH)); // b156: only the list scrolls (TOP / CLOSE / X are buttons)')
ed1(F, '[Self]() { Self->NewsY = 0.f; Self->NewsVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.7f, 1.2f); });', '[Self]() { Self->bNewsToTop = true; Self->NewsVel = 0.f; Self->Sfx(TEXT("UiClick"), 0.7f, 1.2f); });')
# ---- top bar
ed1(F, 'TEXT("GOLD  %s  +")', 'TEXT("COINS  %s  +")')
# ---- shop
ed1(F, '		const float Y0 = Top + 80.f, HalfH = (H - 20.f) * 0.5f;', '''		const float Y0 = Top + 80.f, HalfH = (H - 20.f) * 0.5f;
		const int32 PLv = P ? P->Data().Level : 1;
		auto Lock = [&](const FVector2D& TL, float W, float Hh, int32 Lv) -> bool
		{	// b156: dimmed + a round UNLOCK / LV badge; a tap says when it opens
			if (PLv >= Lv) return false;
			ZRect(Out, G, L + 7, TL, FVector2D(W, Hh), FLinearColor(0.02f, 0.01f, 0.05f, 0.55f));
			const float R = FMath::Min(W, Hh) * 0.30f;
			const FVector2D C(TL.X + W - R * 0.85f, TL.Y + Hh - 46.f - R * 0.6f);
			ZTex(Out, G, L + 8, ZDiscTex(), C - FVector2D(R + 6.f, R + 6.f), FVector2D(R + 6.f, R + 6.f) * 2.f, Paper);
			ZTex(Out, G, L + 9, ZDiscTex(), C - FVector2D(R, R), FVector2D(R, R) * 2.f, FLinearColor::FromSRGBColor(FColor(42, 36, 112)));
			if (UTexture2D* Lk = ZLoadPng(TEXT("Data/MenuArt/icon_lock"))) ZTex(Out, G, L + 10, Lk, C + FVector2D(-R * 0.3f, -R * 0.82f), FVector2D(R * 0.6f, R * 0.6f));
			ZTextC(Out, G, L + 10, TEXT("UNLOCK"), Font(0, FMath::Max(16.f, R * 0.3f)), C + FVector2D(0.f, R * 0.12f), Paper);
			ZTextC(Out, G, L + 10, FString::Printf(TEXT("LV %d"), Lv), Font(0, FMath::Max(20.f, R * 0.42f)), C + FVector2D(0.f, R * 0.52f), Paper);
			AddHit(TL, FVector2D(W, Hh), [Self, Lv]() { Self->Say(FString::Printf(TEXT("REACH LEVEL %d TO UNLOCK"), Lv)); Self->Sfx(TEXT("UiCancel"), 0.7f); });
			return true;
		};''')
ed1(F, '''const FVector2D T2 = Card(340.f, H, Y0, Paper, bHot ? FString(TEXT("OWNED")) : FString(TEXT("GEMS 120")), [Self]() { FString W; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuySkin(FName(TEXT("Hotstreak")), W)) Self->Say(TEXT("HOTSTREAK ZOOGI UNLOCKED!")); else Self->Say(W.ToUpper()); });''',
       '''const FVector2D T2 = Card(340.f, H, Y0, ZTile(3), bHot ? FString(TEXT("OWNED")) : FString(TEXT("GEMS 120")), [Self, bHot]() { if (bHot) { Self->Say(TEXT("OWNED - EQUIP IT IN INVENTORY")); return; } Self->AskBuy(TEXT("UNLOCK ZOOGI?"), TEXT("HOTSTREAK"), ZBallIcon(TEXT("Hotstreak")), ZTile(3), 120, 1, [Self]() { FString W; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuySkin(FName(TEXT("Hotstreak")), W)) Self->Say(TEXT("HOTSTREAK ZOOGI UNLOCKED!")); else Self->Say(W.ToUpper()); }); });''')
ed1(F, 'ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Hotstreak")), T2 + FVector2D(40.f, 80.f), FVector2D(260.f, H - 190.f));', 'ZTexFit(Out, G, L + 5, ZBallIcon(TEXT("Hotstreak")), T2 + FVector2D(40.f, 80.f), FVector2D(260.f, H - 190.f));\n\t\t\tif (!bHot) Lock(T2, 340.f, H, HotstreakLockLv);')
ed1(F, '''const FVector2D TL = Card(W, HalfH, Sy, Paper, bOwn ? FString(TEXT("OWNED")) : FString::Printf(TEXT("%s %s"), Kind == 1 ? TEXT("GEMS") : TEXT("GOLD"), *ZNumS(Price)),''',
       '''const FVector2D TL = Card(W, HalfH, Sy, ZTile(Kind == 1 ? 2 : (n % 2 ? 1 : 0)), bOwn ? FString(TEXT("OWNED")) : FString::Printf(TEXT("%s %s"), Kind == 1 ? TEXT("GEMS") : TEXT("COINS"), *ZNumS(Price)),''')
ed1(F, '''[Self, Ball, bOwn]() { if (bOwn) { Self->Say(TEXT("OWNED - EQUIP IT IN INVENTORY")); return; } FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuySkin(FName(*Ball), Wy)) Self->Say(FString::Printf(TEXT("%s BALL UNLOCKED!"), *Ball.ToUpper())); else Self->Say(Wy.ToUpper()); });''',
       '''[Self, Ball, bOwn, Price, Kind]() { if (bOwn) { Self->Say(TEXT("OWNED - EQUIP IT IN INVENTORY")); return; } Self->AskBuy(TEXT("UNLOCK SKIN?"), Ball.ToUpper() + TEXT(" BALL"), ZBallIcon(Ball), ZTile(Kind == 1 ? 2 : 3), Price, Kind, [Self, Ball]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuySkin(FName(*Ball), Wy)) Self->Say(FString::Printf(TEXT("%s BALL UNLOCKED!"), *Ball.ToUpper())); else Self->Say(Wy.ToUpper()); }); });''')
ed1(F, 'ZTexFit(Out, G, L + 5, ZBallIcon(Ball), TL + FVector2D(16.f, 12.f), FVector2D(W - 32.f, HalfH - 70.f), bOwn ? FLinearColor(1.f, 1.f, 1.f, 0.5f) : FLinearColor::White);',
       'ZTexFit(Out, G, L + 5, ZBallIcon(Ball), TL + FVector2D(16.f, 12.f), FVector2D(W - 32.f, HalfH - 70.f), bOwn ? FLinearColor(1.f, 1.f, 1.f, 0.5f) : FLinearColor::White);\n\t\t\t\tif (!bOwn) Lock(TL, W, HalfH, ZSkinLockLv(n));')
ed1(F, '''FString::Printf(TEXT("%s %s"), Kind == 1 ? TEXT("GEMS") : TEXT("GOLD"), *ZNumS(Price)));''', '''FString::Printf(TEXT("%s %s"), Kind == 1 ? TEXT("GEMS") : TEXT("COINS"), *ZNumS(Price)));''')
ed1(F, 'const FVector2D TL = Card(W, HalfH, Sy, Paper, Lbl, [Self, Id, Price, Kind, Nm, bOwn]()', 'const FVector2D TL = Card(W, HalfH, Sy, ZTile(1), Lbl, [Self, Id, Price, Kind, Nm, bOwn]()')
ed1(F, 'ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 22.f, W - 16.f), TL + FVector2D(W * 0.5f, 20.f), Ink);',
       '''ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 22.f, W - 16.f), TL + FVector2D(W * 0.5f, 20.f), Paper);
				if (!bOwn && !Lock(TL, W, HalfH, ZPackLockLv(n, Price))) // b156: buying goes through the BUY pop-up
					AddHit(TL, FVector2D(W, HalfH), [Self, Id, Price, Kind, Nm]() { Self->AskBuy(TEXT("UNLOCK PACK?"), Nm, nullptr, ZTile(1), Price, Kind, [Self, Id, Price, Kind, Nm]()
					{
						UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (!Pp) return;
						if (Kind == 1) { if (!Pp->SpendGems(Price, FName(TEXT("pack_skin")))) { Self->Say(TEXT("NOT ENOUGH GEMS")); return; } }
						else { if (Pp->Data().Gold < Price) { Self->Say(TEXT("NOT ENOUGH COINS")); return; } Pp->Mutable().Gold -= Price; }
						Pp->Mutable().OwnedPackSkins.Add(Id); Pp->Mutable().EquippedPackSkin = Id; Pp->Save();
						Self->Say(FString::Printf(TEXT("%s UNLOCKED!"), *Nm));
					}); });''')
ed1(F, '''const FVector2D TL = Card(250.f, H, Y0, Paper, Left == 0 ? FString(TEXT("SOLD OUT")) : FString::Printf(TEXT("GEMS %s"), *ZNumS(Cd.GemCost)),
				[Self, Id, Name]() { FString Wy; TArray<FZoogiFrontReward> R; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyChest(Id, R, Wy)) Self->ShowRewards(Name, Id, R); else Self->Say(Wy.ToUpper()); });''',
       '''const FVector2D TL = Card(250.f, H, Y0, ZTile(Id == FName(TEXT("epic")) ? 3 : Id == FName(TEXT("royal")) ? 2 : 0), Left == 0 ? FString(TEXT("SOLD OUT")) : FString::Printf(TEXT("GEMS %s"), *ZNumS(Cd.GemCost)),
				[Self, Id, Name, Cost = Cd.GemCost, Left]() { if (Left == 0) { Self->Say(TEXT("SOLD OUT - BACK TOMORROW")); return; } Self->AskBuy(TEXT("OPEN CHEST?"), Name, nullptr, ZTile(Id == FName(TEXT("epic")) ? 3 : Id == FName(TEXT("royal")) ? 2 : 0), Cost, 1, [Self, Id, Name]() { FString Wy; TArray<FZoogiFrontReward> R; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyChest(Id, R, Wy)) Self->ShowRewards(Name, Id, R); else Self->Say(Wy.ToUpper()); }, Id); });''')
ed1(F, 'ZTextC(Out, G, L + 5, Name, ZFitFont(Name, 0, 30.f, 230.f), TL + FVector2D(125.f, 40.f), Ink);', 'ZTextC(Out, G, L + 5, Name, ZFitFont(Name, 0, 30.f, 230.f), TL + FVector2D(125.f, 40.f), Paper);')
ed1(F, 'ZChest(Out, G, L + 5, Id, TL + FVector2D(125.f, H * 0.45f), 140.f);', 'ZChest(Out, G, L + 5, Id, TL + FVector2D(125.f, H * 0.45f), 140.f);\n\t\t\tLock(TL, 250.f, H, ZChestLockLv(Id));')
ed1(F, 'FString::Printf(TEXT("%d LEFT TODAY"), Left), Font(0, 22.f), TL + FVector2D(125.f, H - 80.f), Mag());', 'FString::Printf(TEXT("%d LEFT TODAY"), Left), Font(0, 22.f), TL + FVector2D(125.f, H - 80.f), Gold);')
ed1(F, '''Paper, Dl.bBought ? FString(TEXT("BOUGHT")) : Dl.bFree ? FString(TEXT("FREE!")) : FString::Printf(TEXT("GOLD %s"), *ZNumS(Dl.Price)),
					[Self, i]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyDeal(i, Wy)) Self->Say(TEXT("ADDED TO YOUR CARDS!")); else Self->Say(Wy.ToUpper()); });''',
       '''ZTile(Dl.bFree ? 0 : 5), Dl.bBought ? FString(TEXT("BOUGHT")) : Dl.bFree ? FString(TEXT("FREE!")) : FString::Printf(TEXT("COINS %s"), *ZNumS(Dl.Price)),
					[Self, i, Free = Dl.bFree, Bought = Dl.bBought, Price = Dl.Price, Nm2 = (Cd ? Cd->Name.ToUpper() : Dl.CardId.ToString()), Art = Dl.CardId]()
					{
						auto Do = [Self, i]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyDeal(i, Wy)) Self->Say(TEXT("ADDED TO YOUR CARDS!")); else Self->Say(Wy.ToUpper()); };
						if (Free || Bought) { Do(); return; }
						Self->AskBuy(TEXT("BUY CARD?"), Nm2, ZLoadPng(TEXT("Data/CardArt/") + Art.ToString()), ZTile(5), Price, 0, Do);
					});''')
ed1(F, 'ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 22.f, W - 12.f), TL + FVector2D(W * 0.5f, HalfH - 64.f), Ink);', 'ZTextC(Out, G, L + 6, Nm, ZFitFont(Nm, 0, 22.f, W - 12.f), TL + FVector2D(W * 0.5f, HalfH - 64.f), Paper);')
ed1(F, 'Section(4, TEXT("CRYSTALS"));', 'Section(4, TEXT("GEMS"));')
ed1(F, '''Paper, FString::Printf(TEXT("$%.2f"), Gp[i].USD), [Self]() { Self->Say(TEXT("THE STORE OPENS IN A LATER BUILD")); });''', '''ZTile(2), FString::Printf(TEXT("$%.2f"), Gp[i].USD), [Self]() { Self->Say(TEXT("GEM PACKS OPEN ONCE THE APP STORE IS SET UP")); });''')
ed1(F, 'ZTex(Out, G, L + 5, ZBurstTex(), TL + FVector2D(W * 0.5f - 50.f, 18.f), FVector2D(100.f, 100.f), Mag());', 'ZTexFit(Out, G, L + 5, ZLoadPng(i < 2 ? TEXT("Data/MenuArt/Front/icon_gem3d") : TEXT("Data/MenuArt/Front/icon_gem_pile")), TL + FVector2D(14.f, 8.f), FVector2D(W - 28.f, HalfH - 128.f)); // b156: the real gem art')
ed1(F, 'ZTextC(Out, G, L + 6, ZNumS(Gp[i].Gems), ZFitFont(ZNumS(Gp[i].Gems), 0, 40.f, W - 20.f), TL + FVector2D(W * 0.5f, HalfH - 74.f), Ink);', 'ZTextC(Out, G, L + 6, ZNumS(Gp[i].Gems), ZFitFont(ZNumS(Gp[i].Gems), 0, 40.f, W - 20.f), TL + FVector2D(W * 0.5f, HalfH - 74.f), Paper);')
ed1(F, '''Card(250.f, H, Y0, Paper, FString::Printf(TEXT("GEMS %s"), *ZNumS(Gp[i].Gems)), [Self, i]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyGold(i, Wy)) Self->Say(TEXT("GOLD ADDED!")); else Self->Say(Wy.ToUpper()); });''',
       '''Card(250.f, H, Y0, ZTile(3), FString::Printf(TEXT("GEMS %s"), *ZNumS(Gp[i].Gems)), [Self, i, Gm = Gp[i].Gems, Gd = Gp[i].Gold]() { Self->AskBuy(TEXT("BUY COINS?"), ZNumS(Gd) + TEXT(" COINS"), ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin_stack")), ZTile(3), Gm, 1, [Self, i]() { FString Wy; UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (Pp && Pp->BuyGold(i, Wy)) Self->Say(TEXT("COINS ADDED!")); else Self->Say(Wy.ToUpper()); }); });''')
ed1(F, 'ZTex(Out, G, L + 5, ZDiscTex(), TL + FVector2D(55.f, 60.f), FVector2D(140.f, 140.f), Gold);', 'ZTexFit(Out, G, L + 5, ZLoadPng(TEXT("Data/MenuArt/Front/icon_coin_stack")), TL + FVector2D(25.f, 30.f), FVector2D(200.f, 200.f)); // b156: the real coin art')
ed1(F, 'ZTextC(Out, G, L + 6, ZNumS(Gp[i].Gold), Font(0, 44.f), TL + FVector2D(125.f, H * 0.62f), Ink);', 'ZTextC(Out, G, L + 6, ZNumS(Gp[i].Gold), Font(0, 44.f), TL + FVector2D(125.f, H * 0.62f), Paper);')
ed1(F, 'TL + FVector2D(125.f, H * 0.62f + 50.f), Mag());', 'TL + FVector2D(125.f, H * 0.62f + 50.f), Ink);')
ed1(F, 'TEXT("EQUIPMENT"), TEXT("CRYSTALS"), TEXT("COINS") };', 'TEXT("EQUIPMENT"), TEXT("GEMS"), TEXT("COINS") };')
# ---- the two new popups
ed1(F, '	void PaintSettings(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const', r'''	void PaintConfirm(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{	// b156 (Evan's reference): the BUY pop-up - item tile, price, CANCEL / BUY (short of it -> GET GEMS / GET COINS)
		UZoogiFrontProfileSubsystem* P = FP();
		const int32 Have = P ? (CfmKind == 1 ? P->Gems() : P->Data().Gold) : 0;
		const bool bShort = Have < CfmPrice;
		const float K = FMath::Clamp((T - PopT) / 0.16f, 0.f, 1.f), Pk = 0.86f + 0.14f * K;
		const FVector2D Sz = FVector2D(840.f, 480.f) * Pk;
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f);
		ZRect(Out, G, L, TL + FVector2D(18.f, 18.f), Sz, Mag());
		ZRect(Out, G, L + 1, TL - FVector2D(6.f, 6.f), Sz + FVector2D(12.f, 12.f), Paper);
		ZRect(Out, G, L + 2, TL, Sz, Ink);
		AddHit(TL, Sz, nullptr);
		ZRect(Out, G, L + 3, TL, FVector2D(Sz.X, 94.f * Pk), Mag());
		ZTextC(Out, G, L + 4, CfmTitle, Font(0, 56.f * Pk), TL + FVector2D(Sz.X * 0.5f, 47.f * Pk), Paper);
		const FVector2D It = TL + FVector2D(40.f, 126.f) * Pk, Is = FVector2D(220.f, 220.f) * Pk;
		ZPanel(Out, G, L + 3, It, Is, CfmCol, Mag(), 4.f, 8.f);
		if (!CfmChest.IsNone()) ZChest(Out, G, L + 6, CfmChest, It + Is * 0.5f, Is.X * 0.6f);
		else if (UTexture2D* Ic = CfmIcon.Get()) ZTexFit(Out, G, L + 6, Ic, It + FVector2D(14.f, 14.f), Is - FVector2D(28.f, 28.f));
		const float Tx = It.X + Is.X + 40.f * Pk, Tw = TL.X + Sz.X - 30.f - Tx;
		ZTextAt(Out, G, L + 4, CfmName, ZFitFont(CfmName, 0, 50.f * Pk, Tw), FVector2D(Tx, It.Y + 4.f), Paper);
		if (UTexture2D* Cur = CfmKind == 1 ? ZGemIcon() : ZCoinIcon()) ZTex(Out, G, L + 4, Cur, FVector2D(Tx, It.Y + 80.f * Pk), FVector2D(66.f, 66.f) * Pk);
		ZTextAt(Out, G, L + 4, FString::Printf(TEXT("%s %s"), *ZNumS(CfmPrice), CfmKind == 1 ? TEXT("GEMS") : TEXT("COINS")), Font(0, 46.f * Pk), FVector2D(Tx + 80.f * Pk, It.Y + 86.f * Pk), Gold);
		const FString Hv = bShort ? FString::Printf(TEXT("YOU HAVE %s - NEED %s MORE"), *ZNumS(Have), *ZNumS(CfmPrice - Have)) : FString::Printf(TEXT("YOU HAVE %s"), *ZNumS(Have));
		ZTextAt(Out, G, L + 4, Hv, ZFitFont(Hv, 1, 26.f * Pk, Tw), FVector2D(Tx, It.Y + 170.f * Pk), bShort ? Red : FLinearColor(0.82f, 0.82f, 0.86f));
		const FVector2D Bs = FVector2D(300.f, 96.f) * Pk;
		const FVector2D Cp(TL.X + Sz.X * 0.5f - Bs.X - 20.f, TL.Y + Sz.Y - Bs.Y - 30.f * Pk), Bp(TL.X + Sz.X * 0.5f + 20.f, Cp.Y);
		ZPanel(Out, G, L + 3, Cp, Bs, Paper, Ink, 4.f, 8.f);
		ZTextC(Out, G, L + 6, TEXT("CANCEL"), Font(0, 44.f * Pk), Cp + Bs * 0.5f, Ink);
		AddHit(Cp, Bs, [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; Self->CfmFn = nullptr; });
		ZPanel(Out, G, L + 3, Bp, Bs, bShort ? Mag() : Green, Ink, 4.f, 8.f);
		const FString BuyTxt = bShort ? FString(CfmKind == 1 ? TEXT("GET GEMS") : TEXT("GET COINS")) : FString(TEXT("BUY"));
		ZTextC(Out, G, L + 6, BuyTxt, Font(0, 46.f * Pk), Bp + Bs * 0.5f, Paper);
		const bool bGems = CfmKind == 1;
		AddHit(Bp, Bs, [Self, bShort, bGems]()
		{
			Self->Pop = SZoogiFront::EPop::None;
			if (bShort) { Self->CfmFn = nullptr; Self->Go(2); Self->ShopX = FMath::Min(Self->ShopMax, Self->SectionX.FindRef(bGems ? 4 : 5)); Self->ShopXVel = 0.f; return; }
			TFunction<void()> Fn = MoveTemp(Self->CfmFn); Self->CfmFn = nullptr;
			if (Fn) Fn();
			Self->Sfx(TEXT("UiConfirm"), 1.f, 1.15f);
			if (UObject* C = Self->Ctx.Get()) ZoogiHaptics::Kick(0.55f, C->GetWorld(), TEXT("buy"));
		});
	}

	void PaintLevelUp(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{	// b156: LEVEL UP! - the XP bar and what this level opened in the shop
		const UZoogiFrontProfileSubsystem* P = FP();
		const float K = FMath::Clamp((T - PopT) / 0.25f, 0.f, 1.f);
		const FVector2D Sz(900.f, 580.f);
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f + 30.f);
		ZTex(Out, G, L, ZBurstTex(), FVector2D(DW * 0.5f - 560.f, DH * 0.5f - 530.f), FVector2D(1120.f, 1120.f), Mag().CopyWithNewOpacity(0.35f * K));
		ZRect(Out, G, L + 1, TL + FVector2D(18.f, 18.f), Sz, Mag());
		ZRect(Out, G, L + 2, TL - FVector2D(6.f, 6.f), Sz + FVector2D(12.f, 12.f), Paper);
		ZRect(Out, G, L + 3, TL, Sz, Ink);
		AddHit(TL, Sz, nullptr);
		ZRansom(Out, G, L + 4, { TEXT("LEVEL"), FString::FromInt(LvShown), TEXT("!") }, TL + FVector2D(Sz.X * 0.5f - 240.f, -64.f), 76.f * (0.75f + 0.25f * K));
		const int32 Xp = P ? P->Data().XP : 0, Need = FMath::Max(1, UZoogiFrontProfileSubsystem::XPForLevel(LvShown));
		const FVector2D Bt = TL + FVector2D(60.f, 74.f), Bz(Sz.X - 120.f, 36.f);
		ZRect(Out, G, L + 4, Bt - FVector2D(4.f, 4.f), Bz + FVector2D(8.f, 8.f), Paper);
		ZRect(Out, G, L + 5, Bt, Bz, Ink);
		ZRect(Out, G, L + 6, Bt, FVector2D(Bz.X * FMath::Clamp((float)Xp / Need, 0.f, 1.f), Bz.Y), Mag());
		ZTextC(Out, G, L + 7, FString::Printf(TEXT("%s / %s XP TO LEVEL %d"), *ZNumS(Xp), *ZNumS(Need), LvShown + 1), Font(0, 24.f), Bt + Bz * 0.5f, Paper);
		struct FUn { UTexture2D* Ic; FName Chest; FString Name; };
		TArray<FUn> Un;
		int32 n = 0;
		for (const ZoogiFront::FHero& Hh : ZoogiFront::Heroes()) { if (FString(Hh.Ball) == TEXT("Hotstreak")) continue; if (ZSkinLockLv(n) == LvShown) Un.Add({ ZBallIcon(Hh.Ball), NAME_None, FString(Hh.Ball).ToUpper() + TEXT(" BALL") }); ++n; }
		if (HotstreakLockLv == LvShown) Un.Add({ ZBallIcon(TEXT("Hotstreak")), NAME_None, TEXT("HOTSTREAK") });
		n = 0; for (const ZoogiWolf::FSkin& Sk : ZoogiWolf::Skins()) { if (Sk.Price > 0 && ZPackLockLv(n, Sk.Price) == LvShown) Un.Add({ nullptr, NAME_None, FString(Sk.Name).ToUpper() }); ++n; }
		if (ZChestLockLv(FName(TEXT("royal"))) == LvShown) Un.Add({ nullptr, FName(TEXT("royal")), TEXT("RARE CHEST") });
		if (ZChestLockLv(FName(TEXT("epic"))) == LvShown) Un.Add({ nullptr, FName(TEXT("epic")), TEXT("EPIC CHEST") });
		ZTextC(Out, G, L + 4, Un.Num() ? FString(TEXT("NEW IN THE SHOP")) : FString(TEXT("KEEP ROLLING - MORE UNLOCKS NEXT LEVEL")), Font(0, 34.f), TL + FVector2D(Sz.X * 0.5f, 152.f), Gold);
		const int32 Nu = FMath::Min(Un.Num(), 3);
		const float Cw = 200.f; float X = TL.X + Sz.X * 0.5f - (Nu * (Cw + 24.f) - 24.f) * 0.5f;
		for (int32 i = 0; i < Nu; ++i)
		{
			const FVector2D Ct(X, TL.Y + 196.f);
			ZPanel(Out, G, L + 4, Ct, FVector2D(Cw, Cw), ZTile(i + 2), Mag(), 4.f, 8.f);
			if (!Un[i].Chest.IsNone()) ZChest(Out, G, L + 8, Un[i].Chest, Ct + FVector2D(Cw, Cw - 30.f) * 0.5f, Cw * 0.55f);
			else if (Un[i].Ic) ZTexFit(Out, G, L + 8, Un[i].Ic, Ct + FVector2D(16.f, 10.f), FVector2D(Cw - 32.f, Cw - 54.f));
			ZTextC(Out, G, L + 9, Un[i].Name, ZFitFont(Un[i].Name, 0, 24.f, Cw - 12.f), Ct + FVector2D(Cw * 0.5f, Cw - 22.f), Paper);
			X += Cw + 24.f;
		}
		const FVector2D Bs(300.f, 92.f);
		const FVector2D Sp(TL.X + Sz.X * 0.5f - Bs.X - 20.f, TL.Y + Sz.Y - Bs.Y - 26.f), Op(TL.X + Sz.X * 0.5f + 20.f, Sp.Y);
		ZPanel(Out, G, L + 4, Sp, Bs, Mag(), Paper, 4.f, 8.f);
		ZTextC(Out, G, L + 8, TEXT("SHOP"), Font(0, 46.f), Sp + Bs * 0.5f, Paper);
		AddHit(Sp, Bs, [Self]() { Self->Pop = SZoogiFront::EPop::None; Self->Go(2); Self->ShopX = 0.f; });
		ZPanel(Out, G, L + 4, Op, Bs, Green, Paper, 4.f, 8.f);
		ZTextC(Out, G, L + 8, TEXT("NICE!"), Font(0, 46.f), Op + Bs * 0.5f, Paper);
		AddHit(Op, Bs, [Self]() { Self->Sfx(TEXT("UiConfirm")); Self->Pop = SZoogiFront::EPop::None; });
	}

	void PaintSettings(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const''')
run()
