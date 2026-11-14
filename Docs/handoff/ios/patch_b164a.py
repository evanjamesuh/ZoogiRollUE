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
    print('b164a ok', len(EDITS))
# b164a (Evan Nov 6 13:45 / 14:03): the ANIME DIRECTOR (b164_files: ZoogiAnimeCues.h/.cpp, ZoogiAnimeCuesInt.h, ZoogiAnimeDirector.cpp) +
# hooks: combat events (Emit), round / final / win / lose (Tick), boss attack, trick shot 3 stars, story lines (replacing the film rows),
# time dilation while a cue plays, gameplay waits (IsBusy), Dev Mode button + F6. NEWS B164.
import shutil
EDITS.clear()
FD = os.path.dirname(os.path.abspath(__file__)) + '/b164_files/'
A = 'Private/ZoogiActionFun.cpp'
ed1(A, '''#include "ZoogiFlags.h"
''', '''#include "ZoogiFlags.h"
#include "ZoogiAnimeCues.h"
''')
ed1(A, '''	ZoogiFlags::Tick(G, RealDt); // b157: the Z flags waving on every arena
''', '''	ZoogiFlags::Tick(G, RealDt); // b157: the Z flags waving on every arena
	ZoogiAnimeCues::Tick(G, RealDt); // b164: ANIME DIRECTOR cues (round / final / win / lose, F6)
''')
ed1(A, '''			if (ZoogiEdgeHang::Active() && !ZoogiEdgeHang::View().bVisual) Want = ZoogiEdgeHang::Dilation;''', '''			if (ZoogiAnimeCues::WantDilation() > 0.f) Want = ZoogiAnimeCues::WantDilation(); // b164: a director cue pauses / slows the game
			else if (ZoogiEdgeHang::Active() && !ZoogiEdgeHang::View().bVisual) Want = ZoogiEdgeHang::Dilation;''')
H = 'Public/ZoogiAnimeSubsystem.h'
ed1(H, '''#include "ZoogiAnimePanels.h"
''', '''#include "ZoogiAnimePanels.h"
#include "ZoogiAnimeCues.h"
''')
ed1(H, '''	bool IsBusy() const { return bActive || CrashState > 0; }''', '''	bool IsBusy() const { return bActive || CrashState > 0 || ZoogiAnimeCues::Busy(); } // b164: + a director cue''')
S = 'Private/ZoogiAnimeSubsystem.cpp'
ed1(S, '''void UZoogiAnimeSubsystem::Emit(const FZoogiCombatEvent& E)
{
	PlayEventSfx(E);''', '''void UZoogiAnimeSubsystem::Emit(const FZoogiCombatEvent& E)
{
	PlayEventSfx(E);
	if (ZoogiAnimeCues::OnEvent(GetWorld(), E.Event.ToString(), E.ZoogiId.ToString(), E.Player)) return; // b164: a director cue replaces the built-in cut-in''')
B = 'Private/ZoogiBossAttack.cpp'
ed1(B, '''			if (TestMode()) G->TakeShotDelayed(FString::Printf(TEXT("boss_warn_%d"), S.Count), 1.2f);''', '''			if (TestMode()) G->TakeShotDelayed(FString::Printf(TEXT("boss_warn_%d"), S.Count), 1.2f);
			ZoogiAnimeCues::Fire(G->GetWorld(), TEXT("BossAttack"), T->PlayerIndex); // b164''')
T = 'Private/ZoogiTrickShot.cpp'
ed1(T, '''		S.LastStars = Stars;
		S.Best = FMath::Max(S.Best, Stars);''', '''		S.LastStars = Stars;
		S.Best = FMath::Max(S.Best, Stars);
		if (Stars >= 3) ZoogiAnimeCues::Fire(G->GetWorld(), TEXT("TrickShot3Star"), H); // b164''')
ed1(T, '''#include "Misc/Parse.h"
''', '''#include "Misc/Parse.h"
#include "ZoogiAnimeCues.h"
''')
ST = 'Private/ZoogiStoryMode.inl'
ed1(ST, '''		if (L->Who == TEXT("film")) { Shown = L->Text.Len(); StartFilm(L->Text, false); return; } // a missing part is skipped next tick''', '''		ZoogiAnimeCues::OnStoryLine(GS.World.Get(), GS.Scene, GS.Line); // b164: a director cue placed before this line
		if (L->Who == TEXT("film") && ZoogiAnimeCues::StoryOwnsFilms()) { Shown = L->Text.Len(); return; } // b164: the director's cues replace the film rows
		if (L->Who == TEXT("film")) { Shown = L->Text.Len(); StartFilm(L->Text, false); return; } // a missing part is skipped next tick''')
ed1(ST, '''		LineT += Dt;
		if (L->Who == TEXT("film")) { if (!ZoogiStoryDir::GFilm.bOn''', '''		if (ZoogiAnimeCues::Busy()) return; // b164: the story waits while a director cue plays
		LineT += Dt;
		if (L->Who == TEXT("film") && ZoogiAnimeCues::StoryOwnsFilms()) { Advance(); return; } // b164
		if (L->Who == TEXT("film")) { if (!ZoogiStoryDir::GFilm.bOn''')
D = 'Private/ZoogiDevMode.cpp'
ed1(D, '''						+ SScrollBox::Slot()[Row(Btn(T(TEXT("PLAY TEST")), [](AZoogiDevMode* M) { M->ClosePanel(); }, Green), Btn(T(TEXT("HIDE PANEL")), [](AZoogiDevMode* M) { M->bPanelHidden = true; }, Grey))]''', '''						+ SScrollBox::Slot()[Row(Btn(T(TEXT("PLAY TEST")), [](AZoogiDevMode* M) { M->ClosePanel(); }, Green), Btn(T(TEXT("HIDE PANEL")), [](AZoogiDevMode* M) { M->bPanelHidden = true; }, Grey))]
						+ SScrollBox::Slot()[Row(Btn(T(TEXT("ANIME DIRECTOR")), [](AZoogiDevMode* M) { ZoogiAnimeCues::ToggleDirector(); }, Gold), Btn(T(TEXT("(F6 ANYWHERE)")), [](AZoogiDevMode* M) { ZoogiAnimeCues::ToggleDirector(); }, Grey))] // b164''')
ed1(D, '''#include "ZoogiArena.h"
''', '''#include "ZoogiArena.h"
#include "ZoogiAnimeCues.h"
''')
ed1(B, '''#include "ZoogiBossAttack.h"
''', '''#include "ZoogiBossAttack.h"
#include "ZoogiAnimeCues.h"
''')
ed1('Private/ZoogiP5Style.cpp', '''#include "ZoogiGuide.h"
''', '''#include "ZoogiGuide.h"
#include "ZoogiAnimeCues.h"
''')
N = 'Private/ZoogiFront.cpp'
ed1(N, '\t\t\t{ TEXT("B163"), TEXT("LEAGUE TRAINING COURT")', \
'''\t\t\t{ TEXT("B164"), TEXT("ANIME DIRECTOR (PC)"), TEXT("Evan can now place every anime clip himself on the PC: when it plays, on which modes and maps, full screen or in a window (landscape and portrait), intro and exit animations, pause or keep playing. Story scenes use the same tool.") },
\t\t\t{ TEXT("B163"), TEXT("LEAGUE TRAINING COURT")''')
run()
if not DRY:
    shutil.copy(FD + 'ZoogiAnimeCues.h', R + 'Public/ZoogiAnimeCues.h')
    for f in ('ZoogiAnimeCues.cpp', 'ZoogiAnimeCuesInt.h', 'ZoogiAnimeDirector.cpp'): shutil.copy(FD + f, R + 'Private/' + f)
    print('director files copied')
