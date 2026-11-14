# Nov 11 (Claude): B241 - Evan 06:46 "PUT MODES 3,4,5 IN TO A NEW VOYAGER MODE": one VOYAGER card on Mode Select opens
# HOLD THE LINE / SHOOTER ROYALE / TEAM DEATHMATCH
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
p = S + 'Private/ZoogiModeSelect.cpp'; t = io.open(p, encoding='utf-8-sig').read()
def rep(a, b):
    global t
    if t.count(a) != 1: print('FAIL', t.count(a), a[:120]); sys.exit(1)
    t = t.replace(a, b)
rep('ShooterHold, ShooterRoyale, ShooterTeam }; // b234', 'ShooterHold, ShooterRoyale, ShooterTeam, Voyager }; // b234, b241 Voyager')
rep('''		{ TEXT("ModeSelect/ms_card_royale"), TEXT("HOLD THE LINE"), TEXT("SHOOTER - SURVIVE 5 WAVES"), TEXT("Fire"), ECardId::ShooterHold },''',
    '''		{ TEXT("ModeSelect/ms_card_royale"), TEXT("VOYAGER"), TEXT("3RD PERSON SHOOTER - 3 MODES"), TEXT("Shock"), ECardId::Voyager }, // b241 (Evan: "PUT MODES 3,4,5 IN TO A NEW VOYAGER MODE")
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("HOLD THE LINE"), TEXT("VOYAGER - SURVIVE THE WAVES"), TEXT("Fire"), ECardId::ShooterHold },''')
rep('TEXT("SHOOTER ROYALE"), TEXT("SHOOTER - LAST ONE STANDING")', 'TEXT("SHOOTER ROYALE"), TEXT("VOYAGER - AI SQUAD ARENA")')
rep('TEXT("TEAM DEATHMATCH"), TEXT("SHOOTER - 2v2, FIRST TO 20")', 'TEXT("TEAM DEATHMATCH"), TEXT("VOYAGER - COVER BATTLE")')
rep('	bool GCountSel = false; // b154a: MULTIPLAYER - the 2 / 3 / 4 PLAYERS step',
    '	bool GCountSel = false; // b154a: MULTIPLAYER - the 2 / 3 / 4 PLAYERS step\n	bool GVoySel = false; // b241: the VOYAGER card\'s 3 modes\n	bool IsVoyMode(ECardId Id) { return Id == ECardId::ShooterHold || Id == ECardId::ShooterRoyale || Id == ECardId::ShooterTeam; }')
rep('	S.bMapList = bMapSelect;\n', '	S.bMapList = bMapSelect;\n	{ static bool bVT = false; if (!bVT && FParse::Param(FCommandLine::Get(), TEXT("ZoogiVoySelTest"))) { bVT = true; GVoySel = true; } } // b241 preview\n')
rep('	S.TitleA = GCountSel ? TEXT("PLAYERS") : bMapSelect ? TEXT("MAP") : TEXT("MODE");',
    '	S.TitleA = GCountSel ? TEXT("PLAYERS") : bMapSelect ? TEXT("MAP") : GVoySel ? TEXT("VOYAGER") : TEXT("MODE");')
rep('''	else
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Cards); ++i)
		{
			ZoogiP5::FItem It;''', '''	else
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Cards); ++i)
		{
			if (GVoySel != IsVoyMode(Cards[i].Id)) continue; // b241: the 3 shooter modes live under the VOYAGER card
			ZoogiP5::FItem It;''')
rep('''	S.Back = [Self]() { if (!Self.IsValid()) return; if (ZoogiModeSel::GCountSel && !Self->bMapSelect)''',
    '''	S.Back = [Self]() { if (!Self.IsValid()) return; if (ZoogiModeSel::GVoySel && !Self->bMapSelect) { ZoogiModeSel::GVoySel = false; Self->bDirty = true; return; } if (ZoogiModeSel::GCountSel && !Self->bMapSelect)''')
rep('''	if (Id == ZoogiModeSel::ECardId::Multi)
	{''', '''	if (Id == ZoogiModeSel::ECardId::Voyager)
	{	// b241: open the VOYAGER modes
		ZoogiModeSel::GVoySel = true; ZoogiModeSel::GCountSel = false; ZoogiModeSel::GRaidMaps = false; bMapSelect = false; bTrainingMaps = false; bDirty = true;
		return;
	}
	if (ZoogiModeSel::IsVoyMode(Id)) ZoogiModeSel::GVoySel = false; // b241: back on the main list next time
	if (Id == ZoogiModeSel::ECardId::Multi)
	{''')
io.open(p, 'w', encoding='utf-8', newline='').write(t)
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B240"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B241"), TEXT("VOYAGER MODE"), TEXT("Mode Select has one VOYAGER card now: tap it to pick HOLD THE LINE, SHOOTER ROYALE or TEAM DEATHMATCH (the real Voyager game). BACK returns to the modes.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
