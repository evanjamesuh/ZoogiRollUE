# b171d (Evan: "yes to this big corner design"): big corner badges - face, name, score, SP bar (5), crystals (4) + xN; -ZoogiOldBadges
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
if not os.path.exists(R): R='C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/'
dry='dry' in sys.argv
files={}
def get(f):
    if f not in files: files[f]=open(R+f,'rb').read().decode('utf-8')
    return files[f]
def ed(f,old,new):
    s=get(f)
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: files[f]=s.replace(o,n); print('OK',f,old[:50].strip()); return
    print('MISS',f,repr(old[:80]), s.count(old), s.count(old.replace('\n','\r\n'))); sys.exit(1)
H='Public/ZoogiArenaHUDWidget.h'; C='Private/ZoogiArenaHUDWidget.cpp'
ed(H,'bool bAviUlt = false; bool bAviClip = false; FSlateBrush AviSaved; TWeakObjectPtr<UObject> AviRes; };',
 'bool bAviUlt = false; bool bAviClip = false; FSlateBrush AviSaved; TWeakObjectPtr<UObject> AviRes; bool bBig = false; TArray<TWeakObjectPtr<UWidget>> SpSegs, ActiveExtra; TWeakObjectPtr<UTextBlock> CrysN; };\n\tvoid BuildBigBadge(int32 Player, float X, float Y, float Wd, float Ht); // b171d: big corner badge\n\tvoid SyncBigBadge(class AZoogiGameMode* G, FUnitRefs& U, int32 i);\n\tstatic bool BigBadges();\n\tfloat BigBottom = 0.f;')
ed(C,'''	for (FUnitRefs& U : Units) U = FUnitRefs();
''','''	for (FUnitRefs& U : Units) U = FUnitRefs();
	BigBottom = 0.f;
''')
# ---- landscape / non-board portrait top bar
ed(C,'''	const float PlateW = 386.f, PlateH = 238.f;
	int32 Mark = Canvas->GetChildrenCount();
	Stone(-12.f, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
	MarkSplit(Mark, FVector2D(-PlateW - 40.f, 0.f)); Mark = Canvas->GetChildrenCount();
	Stone(W - PlateW, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
	MarkSplit(Mark, FVector2D(PlateW + 40.f, 0.f)); Mark = Canvas->GetChildrenCount();
''','''	// b171d (Evan: "big corner badges ... with the crystal counter and sp"): cards replace the stone plates; -ZoogiOldBadges
	const bool bBigB = BigBadges();
	const float BigW = 200.f, BigH = 296.f;
	const FMargin SafeB = ZoogiFramed::SafeInsets(FVector2D(W * S, Design.Y * S));
	const float BigTop = FMath::Max(SafeB.Top / FMath::Max(S, 0.001f) + 12.f, 14.f);
	const float PlateW = bBigB ? 2.f * BigW + 40.f : 386.f;
	const float PlateH = bBigB ? BigTop + BigH + 44.f : 238.f;
	int32 Mark = Canvas->GetChildrenCount();
	if (!bBigB)
	{
		Stone(-12.f, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
		MarkSplit(Mark, FVector2D(-PlateW - 40.f, 0.f)); Mark = Canvas->GetChildrenCount();
		Stone(W - PlateW, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
		MarkSplit(Mark, FVector2D(PlateW + 40.f, 0.f)); Mark = Canvas->GetChildrenCount();
	}
''')
ed(C,'''		BuildUnit(i, Team == 0 ? CxA[USlot] : CxB[USlot], 14.f);
''','''		if (bBigB)
		{	// outer slot first; 1v1 / 3 players: the right side starts at its outer slot too
			const bool bLeftSide = (Team == 0) != bMirror;
			const bool bOuter = bLeftSide ? USlot == 0 : (G->Balls.Num() < 4 ? USlot == 0 : USlot == 1);
			BuildBigBadge(i, bLeftSide ? (bOuter ? 16.f : 30.f + BigW) : (bOuter ? W - 16.f - BigW : W - 30.f - 2.f * BigW), BigTop, BigW, BigH);
		}
		else BuildUnit(i, Team == 0 ? CxA[USlot] : CxB[USlot], 14.f);
''')
# ---- board (phone portrait)
ed(C,'''			const float Cx = (Team == 0 ? CxA[USlot] : CxB[USlot]);
			const float RX = Cx - RS * 0.5f, RY = RowY;''','''			if (BigBadges())
			{	// b171d: big corner badges across the top (4 fit a phone); ROUND / MENU sit under them
				const float BW = 206.f, BH = 304.f, Gp = 12.f, M0 = FMath::Max(8.f, (W - 4.f * BW - 3.f * Gp) * 0.5f);
				const FMargin SafeB = ZoogiFramed::SafeInsets(FVector2D(W * S, H * S));
				const float BT = FMath::Max(SafeB.Top / FMath::Max(S, 0.001f) + 10.f, 60.f);
				const bool bLeftSide = (Team == 0) != bMirror;
				const bool bOuter = bLeftSide ? USlot == 0 : (G->Balls.Num() < 4 ? USlot == 0 : USlot == 1);
				const float BX = bLeftSide ? (bOuter ? M0 : M0 + BW + Gp) : (bOuter ? W - M0 - BW : W - M0 - 2.f * BW - Gp);
				BuildBigBadge(i, BX, BT, BW, BH);
				MarkSplit(PMark, FVector2D(BX + BW * 0.5f < W * 0.5f ? -(BX + BW + 40.f) : (W - BX + 40.f), 0.f));
				EpTxt[i] = nullptr;
				continue;
			}
			const float Cx = (Team == 0 ? CxA[USlot] : CxB[USlot]);
			const float RX = Cx - RS * 0.5f, RY = RowY;''')
ed(C,'''		const float EpY = FMath::Max(BandH + 8.f, 96.f);''','''		const float EpY = BigBottom > 0.f ? BigBottom + 18.f : FMath::Max(BandH + 8.f, 96.f); // b171d: under the big badges''')
ed(C,'''	BuildComboBadge(0.f, 372.f);''','''	BuildComboBadge(0.f, BigBottom > 0.f ? BigBottom + 150.f : 372.f);''')
# ---- refresh hooks
ed(C,'''			if (!Pm->GetText().ToString().Equals(Ms)) Pm->SetText(FText::FromString(Ms));
		}
	}
''','''			if (!Pm->GetText().ToString().Equals(Ms)) Pm->SetText(FText::FromString(Ms));
		}
		SyncBigBadge(G, U, i); // b171d
	}
''')
ed(C,'''		for (int32 k = 0; k < U.Pips.Num(); ++k) SetFill(U.Pips[k], k < Charge ? BRPlayer[i] : ZCHex(TEXT("232838")));
''','''		for (int32 k = 0; k < U.Pips.Num(); ++k) SetFill(U.Pips[k], k < Charge ? BRPlayer[i] : ZCHex(TEXT("232838")));
		SyncBigBadge(G, U, i); // b171d
''')
# ---- new functions
ed(C,'''// b69 (Evan: "Add SP and remaining SP to the profile above the crystal counter")''','''bool UZoogiArenaHUDWidget::BigBadges()
{
	static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBadges"));
	return b;
}

// b171d (Evan: "big corner badges but show with the crystal counter and sp" / "yes to this big corner design"): one card per
// player - face, name, score, SP bar (5 segments), crystals (4 gems + xN); gold frame + YOUR TURN under whoever is shooting
void UZoogiArenaHUDWidget::BuildBigBadge(int32 Player, float X, float Y, float Wd, float Ht)
{
	const AZoogiGameMode* G = GM();
	const AZoogiBall* B = G && G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	if (!B) return;
	FUnitRefs& U = Units[Player];
	U.bBig = true;
	const float k = Wd / 210.f;
	const FLinearColor PC = ZoogiUtil::PlayerColor(Player);
	U.Active = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFD35C")), 6.f * k, 22.f * k), X - 7.f * k, Y - 7.f * k, Wd + 14.f * k, Ht + 14.f * k);
	At(Box(ZCHex(TEXT("0E0A18")).CopyWithNewOpacity(0.93f), PC, 5.f * k, 18.f * k), X, Y, Wd, Ht);
	const float Rs = 124.f * k, RX = X + (Wd - Rs) * 0.5f, RY = Y + 10.f * k;
	const int32 AvMark = Canvas->GetChildrenCount();
	U.Ring = At(Box(PC, PC, 3.f * k, Rs * 0.5f), RX, RY, Rs, Rs);
	if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) { U.Avi = Img(P, RX + 6.f * k, RY + 6.f * k, Rs - 12.f * k, Rs - 12.f * k, (Rs - 12.f * k) * 0.5f, true); U.AviBase = P; U.bAviUlt = false; }
	else Text(B->Def.Name.Left(2).ToUpper(), RX, RY, Rs, Rs, 48.f * k, FLinearColor::White, 3.f);
	MarkAvatar(AvMark);
	Hit(X, Y, Wd, Ht, [this]() { PeekDetails(); });
	const FString Nm = B->Def.Name.ToUpper();
	Text(Nm, X + 4.f * k, Y + 136.f * k, Wd - 8.f * k, 40.f * k, (Nm.Len() > 8 ? 23.f : 30.f) * k, FLinearColor::White, 2.5f, 1);
	U.Score = Text(TEXT("0"), X, Y + 172.f * k, Wd, 48.f * k, 40.f * k, ZCHex(TEXT("FFD35C")), 3.f, 1);
	Text(TEXT("SP"), X + 12.f * k, Y + 224.f * k, 40.f * k, 24.f * k, 18.f * k, ZCHex(TEXT("78C8FF")), 1.5f, 0);
	for (int32 j = 0; j < 5; ++j) U.SpSegs.Add(At(Box(ZCHex(TEXT("2D2D3C")), FLinearColor::Transparent, 0.f, 2.f * k), X + 52.f * k + j * 29.f * k, Y + 229.f * k, 25.f * k, 15.f * k));
	const int32 NPip = AZoogiGameMode::AwakenChargeMax;
	for (int32 j = 0; j < NPip && j < 4; ++j)
	{
		UWidget* Gm = At(Box(ZCHex(TEXT("37324A")), ZCHex(TEXT("8D9099")), 2.f * k, 3.f * k), X + 16.f * k + j * 31.f * k, Y + 260.f * k, 19.f * k, 19.f * k);
		if (Gm) Gm->SetRenderTransformAngle(45.f);
		U.Pips.Add(Gm);
	}
	U.CrysN = Text(TEXT("x0"), X + Wd - 62.f * k, Y + 253.f * k, 54.f * k, 32.f * k, 20.f * k, ZCHex(TEXT("DCB4FF")), 1.5f, 0);
	{	// b78 wolf heads (banked Wolf Pack summons): a row under the card, hidden until there is one
		const float WS = 20.f * k, WG = 5.f * k, WW = 4 * WS + 3 * WG;
		for (int32 j = 0; j < 4; ++j) if (UImage* Wi = Img(Tex(TEXT("wolfhead")), X + Wd - WW - 8.f * k + j * (WS + WG), Y + Ht + 8.f * k, WS, WS, 0.f, false, FLinearColor::Transparent)) { Wi->SetVisibility(ESlateVisibility::Collapsed); U.Wolves.Add(Wi); }
	}
	{	// YOUR TURN: gold arrow (diamond) on the bottom edge + label
		UWidget* Ar = At(Box(ZCHex(TEXT("FFD35C")), FLinearColor::Transparent, 0.f, 3.f * k), X + Wd * 0.5f - 13.f * k, Y + Ht - 6.f * k, 26.f * k, 26.f * k);
		if (Ar) { Ar->SetRenderTransformAngle(45.f); Ar->SetVisibility(ESlateVisibility::Collapsed); U.ActiveExtra.Add(Ar); }
		UTextBlock* Tt = Text(B->bCPU ? TEXT("CPU TURN") : TEXT("YOUR TURN"), X - 20.f * k, Y + Ht + 22.f * k, Wd + 40.f * k, 26.f * k, 18.f * k, FLinearColor::White, 2.f, 1);
		if (Tt) { Tt->SetVisibility(ESlateVisibility::Collapsed); U.ActiveExtra.Add(Tt); }
	}
	DetailsBottomD = FMath::Max(DetailsBottomD, Y + Ht + 50.f * k);
	BigBottom = FMath::Max(BigBottom, Y + Ht + 50.f * k);
}

void UZoogiArenaHUDWidget::SyncBigBadge(AZoogiGameMode* G, FUnitRefs& U, int32 i)
{
	if (!U.bBig || !G) return;
	const bool bAct = G->ActivePlayer == i && G->Phase != EZoogiPhase::GameOver;
	for (TWeakObjectPtr<UWidget>& Wd : U.ActiveExtra)
		if (Wd.IsValid()) { const ESlateVisibility V = bAct ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (Wd->GetVisibility() != V) Wd->SetVisibility(V); }
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(i) : nullptr;
	const int32 Max = CS ? FMath::Max(1, CS->Rules().Tune().EnergyPerTurn) : 5;
	const int32 NSeg = U.SpSegs.Num();
	const int32 Lit = PS ? FMath::Clamp(FMath::RoundToInt((float)FMath::Max(0, PS->Energy) * NSeg / Max), 0, NSeg) : 0;
	for (int32 k = 0; k < NSeg; ++k) if (UBorder* Bd = Cast<UBorder>(U.SpSegs[k].Get())) Bd->SetBrushColor(k < Lit ? ZCHex(TEXT("3CAAFF")) : ZCHex(TEXT("2D2D3C")));
	const int32 Charge = G->AwakenCharge[i];
	for (int32 k = 0; k < U.Pips.Num(); ++k)
		if (UBorder* Bd = Cast<UBorder>(U.Pips[k].Get())) Bd->SetBrushColor(k < Charge ? (Charge >= AZoogiGameMode::AwakenChargeMax ? ZCHex(TEXT("FFD35C")) : ZCHex(TEXT("BE5AFF"))) : ZCHex(TEXT("37324A")));
	if (UTextBlock* T = U.CrysN.Get()) { const FString Cs = FString::Printf(TEXT("x%d"), FMath::Max(0, Charge)); if (!T->GetText().ToString().Equals(Cs)) T->SetText(FText::FromString(Cs)); }
}

// b69 (Evan: "Add SP and remaining SP to the profile above the crystal counter")''')
if not dry:
    for f,s in files.items(): open(R+f,'wb').write(s.encode('utf-8')); print('WROTE',f)
