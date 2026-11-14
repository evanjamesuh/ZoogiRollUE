import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiArenaHUDWidget.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:50].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
# SetBrushColor tints the brush: build the coloured bits white so the live colour reads true
ed('U.SpSegs.Add(At(Box(ZCHex(TEXT("2D2D3C")), FLinearColor::Transparent, 0.f, 2.f * k),','U.SpSegs.Add(At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 2.f * k),')
ed('UWidget* Gm = At(Box(ZCHex(TEXT("37324A")), ZCHex(TEXT("8D9099")), 2.f * k, 3.f * k),','UWidget* Gm = At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 3.f * k),')
# the turn frame / arrow / label belong to SyncBigBadge only (the split intro must not force them visible)
ed('''	DetailsBottomD = FMath::Max(DetailsBottomD, Y + Ht + 50.f * k);
	BigBottom = FMath::Max(BigBottom, Y + Ht + 50.f * k);''','''	AvatarW.Add(U.Active); for (TWeakObjectPtr<UWidget>& Wx : U.ActiveExtra) AvatarW.Add(Wx);
	DetailsBottomD = FMath::Max(DetailsBottomD, Y + Ht + 50.f * k);
	BigBottom = FMath::Max(BigBottom, Y + Ht + 50.f * k);''')
ed('''	if (!U.bBig || !G) return;
	const bool bAct = G->ActivePlayer == i && G->Phase != EZoogiPhase::GameOver;''','''	if (!U.bBig || !G) return;
	const bool bAct = G->ActivePlayer == i && G->Phase != EZoogiPhase::GameOver;
	if (U.Active.IsValid()) { const ESlateVisibility V = bAct ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (U.Active->GetVisibility() != V) U.Active->SetVisibility(V); }''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
