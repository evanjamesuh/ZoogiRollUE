# b156a (Evan Nov 6 07:27 "daily shot goes in", 07:36 "Story mode at the top of the list"):
#  DAILY TRICK SHOT mode (ZoogiTrickShot.h/.cpp) wired into the turn flow + the mode list; STORY first in the mode list.
import os, shutil, sys
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n):
    EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            assert b.count(o2) == 1, (p, b.count(ob), b.count(o2), o[:100]); ob, nb = o2, n2
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    here = os.path.dirname(os.path.abspath(__file__))
    shutil.copy(here + '/b156_files/ZoogiTrickShot.h', R + 'Public/ZoogiTrickShot.h')
    shutil.copy(here + '/b156_files/ZoogiTrickShot.cpp', R + 'Private/ZoogiTrickShot.cpp')
    print('b156a ok', len(EDITS))

G = 'Private/ZoogiGameMode.cpp'
ed1(G, '#include "ZoogiRaid.h"', '#include "ZoogiRaid.h"\n#include "ZoogiTrickShot.h"')
ed1(G, '''void AZoogiGameMode::BeginTurn(int32 PlayerIdx)
{''', '''void AZoogiGameMode::BeginTurn(int32 PlayerIdx)
{
	PlayerIdx = ZoogiTrickShot::FixTurn(this, PlayerIdx); // b156: DAILY TRICK SHOT - the human always shoots, the layout is placed''')
ed1(G, '''void AZoogiGameMode::EndTurn()
{''', '''void AZoogiGameMode::EndTurn()
{
	if (ZoogiTrickShot::OnEndTurn(this)) return; // b156: DAILY TRICK SHOT scores the try (next try / the end)''')
ed1(G, '	ZoogiRaid::OnMenu(this); // b151', '	ZoogiRaid::OnMenu(this); // b151\n	ZoogiTrickShot::OnMenu(this); // b156')
ed1(G, '	ZoogiRaid::OnFinished(this); // b151: RAID CLEARED / FAILED banner', '	ZoogiRaid::OnFinished(this); // b151: RAID CLEARED / FAILED banner\n	ZoogiTrickShot::OnFinished(this); // b156: TRICK SHOT stars + rewards banner')
H = 'Public/ZoogiGameMode.h'
ed1(H, '	void PublicFinishGame() { FinishGame(); }', '	void PublicBeginTurn(int32 P) { BeginTurn(P); } // b156: DAILY TRICK SHOT - the next try\n	void PublicFinishGame() { FinishGame(); }')
A = 'Private/ZoogiActionFun.cpp'
ed1(A, '#include "ZoogiRaid.h"', '#include "ZoogiRaid.h"\n#include "ZoogiTrickShot.h"')
ed1(A, '	ZoogiRaid::Tick(G, RealDt); // b151: RAID mode', '	ZoogiTrickShot::Tick(G, RealDt); // b156: DAILY TRICK SHOT\n	ZoogiRaid::Tick(G, RealDt); // b151: RAID mode')
ed1(A, '	ZoogiRaid::Draw(G, C); // b151: the raid boss health bar + team lives', '	ZoogiRaid::Draw(G, C); // b151: the raid boss health bar + team lives\n	ZoogiTrickShot::Draw(G, C); // b156: try / best stars')
C = 'Private/ZoogiCosmosBoss.cpp'
ed1(C, '#include "ZoogiRaid.h"', '#include "ZoogiRaid.h"\n#include "ZoogiTrickShot.h"')
ed1(C, '			if (A == EZoogiArena::Ancient) return false;', '			if (A == EZoogiArena::Ancient || ZoogiTrickShot::Active()) return false; // b156: no titan in the trick shot')
M = 'Private/ZoogiModeSelect.cpp'
ed1(M, '#include "ZoogiRaid.h"', '#include "ZoogiRaid.h"\n#include "ZoogiTrickShot.h"')
ed1(M, '''	enum class ECardId : uint8 { Royale, Multi, Classic, Raid, Training, Story };''', '''	enum class ECardId : uint8 { Royale, Multi, Classic, Raid, Training, Story, TrickShot };''')
ed1(M, '''		{ TEXT("ModeSelect/ms_card_royale"), TEXT("BATTLE ROYALE"), TEXT("8 ARENAS  -  CLASSIC ROUNDS"), TEXT("Fire"), ECardId::Royale },''', '''		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY"), TEXT("EPISODES 1 - 3"), TEXT("Void"), ECardId::Story }, // b156 (Evan): STORY at the top
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("BATTLE ROYALE"), TEXT("8 ARENAS  -  CLASSIC ROUNDS"), TEXT("Fire"), ECardId::Royale },''')
ed1(M, '''		{ TEXT("ModeSelect/ms_card_training"), TEXT("TRAINING"), TEXT("1V1 PRACTICE"), TEXT("Frost"), ECardId::Training },
		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY"), TEXT("EPISODE 1"), TEXT("Void"), ECardId::Story },''', '''		{ TEXT("ModeSelect/ms_card_trickshot"), TEXT("DAILY TRICK SHOT"), TEXT("ONE SHOT  -  3 TRIES  -  NEW EVERY DAY"), TEXT("Shock"), ECardId::TrickShot }, // b156
		{ TEXT("ModeSelect/ms_card_training"), TEXT("TRAINING"), TEXT("1V1 PRACTICE"), TEXT("Frost"), ECardId::Training },''')
ed1(M, '''	Card = Id == ZoogiModeSel::ECardId::Royale ? 0''', '''	if (Id == ZoogiModeSel::ECardId::TrickShot)
	{	// b156 DAILY TRICK SHOT: today's arena, you + 3 rivals that never shoot, one shot per try
		ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; bMapSelect = false; bTrainingMaps = false;
		Fire("Mode", 0);
		Fire("Players", 4);
		CPUs();
		const EZoogiArena Ta = ZoogiTrickShot::TodayArena();
		if (G->MenuArena != Ta) Fire("Arena", (int32)Ta);
		ZoogiTrickShot::Arm(G);
		Fire("Start", 0);
		return;
	}
	Card = Id == ZoogiModeSel::ECardId::Royale ? 0''')
F = 'Private/ZoogiFront.cpp'
ed1(F, '''			{ TEXT("B155"),''', '''			{ TEXT("B156"), TEXT("DAILY TRICK SHOT + SHOP & LEVELS"), TEXT("BATTLE -> DAILY TRICK SHOT: one set-up shot a day, the same for everyone. 3 tries, your best counts: 1 star per rival you knock off in ONE shot (all 3 = 3 stars). New stars pay coins, gems and XP; play daily for a streak (every 7th day = +20 gems). STORY is now first in the mode list. SHOP: coloured tiles, real gem / coin art, a BUY pop-up on every purchase, and items that unlock at a player level (LV badge). A LEVEL UP pop-up shows what you just unlocked. Every button vibrates and squashes when you press it. NEWS: TOP works, smooth scrolling, a bigger X.") },
			{ TEXT("B155"),''')
run()
