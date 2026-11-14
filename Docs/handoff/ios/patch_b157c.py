import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
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
    print('b157c ok', len(EDITS))
# b157c (Evan Nov 6):
#  08:25 "for trick shot build a version of ancient meadows without the most centralize bumpers" / 08:26 "leave enough to do bank
#   trick shots": in a Daily Trick Shot on Ancient Meadows the plaza props mesh is swapped for SM_Arena_PlazaPropsTrick (the ring
#   of 10 drums around the Z removed; curb ring, hazard drums, bollards, banner posts stay to bank off). Swapped back afterwards.
#  08:28 "you can bank off the map boss never phase threw it": the titan gets solid colliders on its hands, forearms, upper arms
#   and head while it is up on the rim (off while it hides, sweeps or sinks). -ZoogiBossNoCollide = old.
# b157c (Evan Nov 6 08:46 "daily trick shot isn't a game mode, it's a pop up that happens at start up main menu"):
#  - DAILY TRICK SHOT leaves the mode list; the main menu opens a DAILY TRICK SHOT pop-up once a day at start-up (after the login
#    reward): today's arena, the goals, rewards, streak and today's best - PLAY! starts it, LATER closes it.
EDITS.clear()
TH = 'Public/ZoogiTrickShot.h'
ed1(TH, '	ZOOGIROLL_API int32 BestToday();', '''	ZOOGIROLL_API int32 BestToday();
	/** b157: the start-up pop-up - due once a day; mark it shown */
	ZOOGIROLL_API bool PopupDue();
	ZOOGIROLL_API void MarkPopupShown();''')
T = 'Private/ZoogiTrickShot.cpp'
ed1(T, '	int32 BestToday() { return GetI(*FString::Printf(TEXT("Best_%d"), DayIndex()), 0); }', '''	int32 BestToday() { return GetI(*FString::Printf(TEXT("Best_%d"), DayIndex()), 0); }
	bool PopupDue() { return !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTrickPopup")) && GetI(TEXT("PopupDay"), -1) != DayIndex(); }
	void MarkPopupShown() { SetI(TEXT("PopupDay"), DayIndex()); if (GConfig) GConfig->Flush(false, GGameUserSettingsIni); }''')
M = 'Private/ZoogiModeSelect.cpp'
ed1(M, '''		{ TEXT("ModeSelect/ms_card_trickshot"), TEXT("DAILY TRICK SHOT"), TEXT("ONE SHOT  -  3 TRIES  -  NEW EVERY DAY"), TEXT("Shock"), ECardId::TrickShot }, // b156\n''', '')
ed1(M, '''	{	// b156 DAILY TRICK SHOT: today's arena, you + 3 rivals that never shoot, one shot per try
		ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; bMapSelect = false; bTrainingMaps = false;''', '''	{	// b156 DAILY TRICK SHOT (b157: started from the main-menu pop-up, StartTrickShot)
		StartTrickShot();
		return;
	}
	if (false)
	{
		ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; bMapSelect = false; bTrainingMaps = false;''')
ed1(M, 'void UZoogiMenuRoot::StartSkillLab()', '''void UZoogiMenuRoot::StartTrickShot()
{	// b157 (Evan): the DAILY TRICK SHOT pop-up's PLAY - today's arena, you + 3 rivals that never shoot, one shot per try
	AZoogiGameMode* G = GM();
	if (!G) return;
	ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; ZoogiModeSel::GCountSel = false;
	bMapSelect = false; bTrainingMaps = false; SetFrontBattle(false);
	UE_LOG(LogZoogi, Log, TEXT("DAILY TRICK SHOT -> today's shot"));
	Fire("Mode", 0);
	Fire("Players", 4);
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0;
	const EZoogiArena Ta = ZoogiTrickShot::TodayArena();
	if (G->MenuArena != Ta) Fire("Arena", (int32)Ta);
	ZoogiTrickShot::Arm(G);
	Fire("Start", 0);
}

void UZoogiMenuRoot::StartSkillLab()''')
MH = 'Public/ZoogiMenuRoot.h'
ed1(MH, '	void StartSkillLab();', '	void StartSkillLab();\n	void StartTrickShot(); // b157: the DAILY TRICK SHOT pop-up')
F = 'Private/ZoogiFront.cpp'
ed1(F, '#include "ZoogiHaptics.h"', '#include "ZoogiHaptics.h"\n#include "ZoogiTrickShot.h"')
ed1(F, 'enum class EPop : uint8 { None, Item, Login, Chest, Tasks, News, Settings, Confirm, LevelUp };', 'enum class EPop : uint8 { None, Item, Login, Chest, Tasks, News, Settings, Confirm, LevelUp, Trick };')
ed1(F, '	bool bLoginChecked = false;', '	bool bLoginChecked = false;\n	bool bTrickChecked = false; // b157: the DAILY TRICK SHOT pop-up (once a day)')
ed1(F, 'Screen = K.Screen; Tab = K.Tab; SkillDetail = K.SkillDetail; ShopX = K.ShopX; bLoginChecked = true;', 'Screen = K.Screen; Tab = K.Tab; SkillDetail = K.SkillDetail; ShopX = K.ShopX; bLoginChecked = true; bTrickChecked = true;')
ed1(F, '''		{	// b156: LEVEL UP pop-up, once per new level''', '''		if (!bTrickChecked && T > 1.4f && Pop == EPop::None && Screen == 0)
		{	// b157 (Evan: "daily trick shot ... is a pop up that happens at start up main menu")
			bTrickChecked = true;
			if (ZoogiTrickShot::PopupDue()) { ZoogiTrickShot::MarkPopupShown(); Pop = EPop::Trick; PopT = T; Sfx(TEXT("UiConfirm"), 0.9f, 1.1f); UE_LOG(LogZoogi, Log, TEXT("FRONT daily trick shot pop-up #%d"), ZoogiTrickShot::TodayNumber()); }
		}
		{	// b156: LEVEL UP pop-up, once per new level''')
ed1(F, 'if (Self->Pop == SZoogiFront::EPop::Item || Self->Pop == SZoogiFront::EPop::Confirm', 'if (Self->Pop == SZoogiFront::EPop::Item || Self->Pop == SZoogiFront::EPop::Trick || Self->Pop == SZoogiFront::EPop::Confirm')
ed1(F, '			else if (Pop == EPop::LevelUp) PaintLevelUp(Out, G, L, Self);', '			else if (Pop == EPop::LevelUp) PaintLevelUp(Out, G, L, Self);\n			else if (Pop == EPop::Trick) PaintTrick(Out, G, L, Self);')
ed1(F, '	void PaintLevelUp(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const', r'''	void PaintTrick(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const
	{	// b157: the DAILY TRICK SHOT pop-up (start-up, once a day)
		const float K = FMath::Clamp((T - PopT) / 0.22f, 0.f, 1.f), Pk = 0.88f + 0.12f * K;
		const FVector2D Sz = FVector2D(FMath::Min(1180.f, DW - 80.f), 640.f) * Pk;
		const FVector2D TL(DW * 0.5f - Sz.X * 0.5f, DH * 0.5f - Sz.Y * 0.5f + 30.f);
		ZRect(Out, G, L, TL + FVector2D(20.f, 20.f), Sz, Mag());
		ZRect(Out, G, L + 1, TL - FVector2D(6.f, 6.f), Sz + FVector2D(12.f, 12.f), Paper);
		ZRect(Out, G, L + 2, TL, Sz, Ink);
		AddHit(TL, Sz, nullptr);
		ZRansom(Out, G, L + 3, { TEXT("DAILY"), TEXT("TRICK"), TEXT("SHOT!") }, TL + FVector2D(30.f, -58.f), 70.f * Pk);
		const EZoogiArena Ar = ZoogiTrickShot::TodayArena();
		const FString ArName = ZoogiUtil::ArenaName(Ar).ToUpper();
		const TCHAR* Key = Ar == EZoogiArena::Ancient ? TEXT("ancient") : Ar == EZoogiArena::Volcanic ? TEXT("volcanic") : Ar == EZoogiArena::Arabian ? TEXT("arabian") : Ar == EZoogiArena::Tomb ? TEXT("tomb") : Ar == EZoogiArena::Neon ? TEXT("neon") : TEXT("meadow");
		const FVector2D At = TL + FVector2D(36.f, 70.f) * Pk, As = FVector2D(Sz.X * 0.46f, Sz.Y - 210.f * Pk);
		ZRect(Out, G, L + 3, At - FVector2D(6.f, 6.f), As + FVector2D(12.f, 12.f), Paper);
		if (UTexture2D* Img = ZLoadPng(FString(TEXT("Data/MenuArt/ModeSelect/ms_map_")) + Key)) ZTexFit(Out, G, L + 4, Img, At, As);
		ZRect(Out, G, L + 5, At + FVector2D(0.f, As.Y - 50.f * Pk), FVector2D(As.X, 50.f * Pk), Ink.CopyWithNewOpacity(0.85f));
		ZTextC(Out, G, L + 6, FString::Printf(TEXT("#%d  -  %s"), ZoogiTrickShot::TodayNumber(), *ArName), ZFitFont(ArName + TEXT("  #00"), 0, 30.f * Pk, As.X - 20.f), At + FVector2D(As.X * 0.5f, As.Y - 25.f * Pk), Paper);
		const float Rx = At.X + As.X + 40.f * Pk, Rw = TL.X + Sz.X - 30.f - Rx;
		float Y = At.Y;
		ZTextAt(Out, G, L + 4, TEXT("ONE SHOT  -  3 TRIES  -  YOUR BEST COUNTS"), ZFitFont(TEXT("ONE SHOT  -  3 TRIES  -  YOUR BEST COUNTS"), 0, 30.f * Pk, Rw), FVector2D(Rx, Y), Gold); Y += 58.f * Pk;
		const TCHAR* Goals[] = { TEXT("1 STAR    KNOCK 1 RIVAL OFF"), TEXT("2 STARS   2 RIVALS IN ONE SHOT"), TEXT("3 STARS   ALL 3 IN ONE SHOT") };
		for (int32 g = 0; g < 3; ++g)
		{
			ZRect(Out, G, L + 3, FVector2D(Rx, Y), FVector2D(Rw, 54.f * Pk), FLinearColor(0.12f, 0.1f, 0.14f));
			ZRect(Out, G, L + 4, FVector2D(Rx, Y), FVector2D(8.f, 54.f * Pk), Mag());
			ZTextAt(Out, G, L + 5, Goals[g], ZFitFont(Goals[g], 0, 28.f * Pk, Rw - 30.f), FVector2D(Rx + 22.f, Y + 10.f * Pk), Paper);
			Y += 64.f * Pk;
		}
		Y += 6.f * Pk;
		if (UTexture2D* Ci = ZCoinIcon()) ZTex(Out, G, L + 4, Ci, FVector2D(Rx, Y), FVector2D(46.f, 46.f) * Pk);
		ZTextAt(Out, G, L + 4, TEXT("+20 PER NEW STAR"), Font(0, 28.f * Pk), FVector2D(Rx + 54.f * Pk, Y + 6.f * Pk), Paper);
		if (UTexture2D* Gi = ZGemIcon()) ZTex(Out, G, L + 4, Gi, FVector2D(Rx + Rw * 0.55f, Y), FVector2D(46.f, 46.f) * Pk);
		ZTextAt(Out, G, L + 4, TEXT("+5 FOR 3 STARS"), Font(0, 28.f * Pk), FVector2D(Rx + Rw * 0.55f + 54.f * Pk, Y + 6.f * Pk), Paper);
		Y += 62.f * Pk;
		const int32 Best = ZoogiTrickShot::BestToday(), Streak = ZoogiTrickShot::Streak();
		const FString Info = FString::Printf(TEXT("STREAK %d DAY%s   -   TODAY'S BEST %d STAR%s"), Streak, Streak == 1 ? TEXT("") : TEXT("S"), Best, Best == 1 ? TEXT("") : TEXT("S"));
		ZTextAt(Out, G, L + 4, Info, ZFitFont(Info, 0, 28.f * Pk, Rw), FVector2D(Rx, Y), Mag());
		const FVector2D Bs = FVector2D(320.f, 100.f) * Pk;
		const FVector2D Lp(TL.X + Sz.X - 2.f * Bs.X - 70.f, TL.Y + Sz.Y - Bs.Y - 30.f * Pk), Pp(TL.X + Sz.X - Bs.X - 36.f, Lp.Y);
		ZPanel(Out, G, L + 3, Lp, Bs, Paper, Ink, 4.f, 8.f);
		ZTextC(Out, G, L + 6, TEXT("LATER"), Font(0, 46.f * Pk), Lp + Bs * 0.5f, Ink);
		AddHit(Lp, Bs, [Self]() { Self->Sfx(TEXT("UiCancel")); Self->Pop = SZoogiFront::EPop::None; });
		ZPanel(Out, G, L + 3, Pp, Bs, Mag(), Paper, 4.f, 8.f);
		ZTextC(Out, G, L + 6, Best >= 3 ? TEXT("AGAIN!") : TEXT("PLAY!"), Font(0, 52.f * Pk), Pp + Bs * 0.5f, Paper);
		AddHit(Pp, Bs, [Self]()
		{
			Self->Pop = SZoogiFront::EPop::None;
			Self->Sfx(TEXT("UiConfirm"), 0.9f);
			TWeakObjectPtr<UZoogiMenuRoot> M = Self->Menu;
			UWorld* W = Self->Ctx.IsValid() ? Self->Ctx->GetWorld() : nullptr;
			ZoogiP5::PlayWipe(W, [M]() { if (M.IsValid()) M->StartTrickShot(); }, Mag(), TEXT("TRICK SHOT!"));
		});
	}

	void PaintLevelUp(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const''')
ed1(F, 'Every arena now flies two purple-and-gold Z flags waving in the wind. The ON FIRE circles are thinner.") },', 'Every arena now flies two purple-and-gold Z flags waving in the wind. The ON FIRE circles are thinner. DAILY TRICK SHOT is no longer in the mode list: it pops up on the main menu once a day at start-up (PLAY! or LATER).") },')
run()
