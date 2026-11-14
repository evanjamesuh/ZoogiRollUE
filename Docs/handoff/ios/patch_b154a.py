# b154a (Evan Nov 6 06:28 "Replace 2v2 with multiple player", 06:29 "Move raid to under classic mode on list"):
# MODE SELECT = BATTLE ROYALE, MULTIPLAYER, CLASSIC, RAID, TRAINING, STORY. MULTIPLAYER = pass-and-play on one device: pick 2 / 3 / 4
# players, then the map; every Zoogi is a human player (the 2V2 BATTLE card is gone; -ZoogiOld2v2Card keeps nothing - the 2v2 mode
# itself still exists in code). Also fixes the card subtitle / colour tables, which had one entry fewer than the cards since b151.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
M = 'Private/ZoogiModeSelect.cpp'
ed1(M, '''	struct FCard { const TCHAR* Art; const TCHAR* Name; };
	const FCard Cards[] = {
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("BATTLE ROYALE") },
		{ TEXT("ModeSelect/ms_card_battle"), TEXT("2V2 BATTLE") },
		{ TEXT("ModeSelect/ms_card_classic"), TEXT("CLASSIC") },
		{ TEXT("ModeSelect/ms_card_training"), TEXT("TRAINING") },
		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY") },
		{ TEXT("ModeSelect/ms_card_raid"), TEXT("RAID") }, // b151 (Evan): co-op vs the map's titan
	};
	bool GRaidMaps = false; // b151: the map cards were opened from RAID''', '''	// b154a (Evan): 2V2 BATTLE -> MULTIPLAYER, RAID under CLASSIC; Id drives StartModeCard (not the position)
	enum class ECardId : uint8 { Royale, Multi, Classic, Raid, Training, Story };
	struct FCard { const TCHAR* Art; const TCHAR* Name; const TCHAR* Sub; const TCHAR* Elem; ECardId Id; };
	const FCard Cards[] = {
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("BATTLE ROYALE"), TEXT("8 ARENAS  -  CLASSIC ROUNDS"), TEXT("Fire"), ECardId::Royale },
		{ TEXT("ModeSelect/ms_card_battle"), TEXT("MULTIPLAYER"), TEXT("PASS & PLAY  -  2 TO 4 PLAYERS"), TEXT("Shock"), ECardId::Multi },
		{ TEXT("ModeSelect/ms_card_classic"), TEXT("CLASSIC"), TEXT("PICK YOUR ARENA"), TEXT(""), ECardId::Classic },
		{ TEXT("ModeSelect/ms_card_raid"), TEXT("RAID"), TEXT("YOU + 2 ALLIES  VS  THE TITAN"), TEXT("Void"), ECardId::Raid }, // b151 (Evan): co-op vs the map's titan
		{ TEXT("ModeSelect/ms_card_training"), TEXT("TRAINING"), TEXT("1V1 PRACTICE"), TEXT("Frost"), ECardId::Training },
		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY"), TEXT("EPISODE 1"), TEXT("Void"), ECardId::Story },
	};
	bool GRaidMaps = false; // b151: the map cards were opened from RAID
	bool GCountSel = false; // b154a: MULTIPLAYER - the 2 / 3 / 4 PLAYERS step
	int32 GMultiN = 0;      // b154a: > 0 = the map cards start a pass-and-play match with this many human players''')
ed1(M, '''	S.bMapList = bMapSelect;
	S.TitleA = bMapSelect ? TEXT("MAP") : TEXT("MODE");
	S.TitleB = TEXT("SELECT");
	if (bMapSelect)
	{''', '''	S.bMapList = bMapSelect;
	S.TitleA = GCountSel ? TEXT("PLAYERS") : bMapSelect ? TEXT("MAP") : TEXT("MODE");
	S.TitleB = TEXT("SELECT");
	if (GCountSel && !bMapSelect)
	{	// b154a MULTIPLAYER: how many players share this device
		for (int32 n = 2; n <= 4; ++n)
		{
			ZoogiP5::FItem It;
			It.Name = FString::Printf(TEXT("%d PLAYERS"), n); It.Sub = TEXT("PASS THE PHONE - TAKE TURNS");
			UTexture2D* T = Art(TEXT("ModeSelect/ms_card_battle")); It.Art = T; if (T) W->Keep.Add(T);
			It.Col = ZoogiP5::ElementColor(TEXT("Shock"));
			It.Fn = [Self, n]() { if (!Self.IsValid()) return; GCountSel = false; GMultiN = n; Self->bMapSelect = true; Self->bTrainingMaps = false; Self->bDirty = true; UE_LOG(LogZoogi, Log, TEXT("MULTIPLAYER: %d players -> map select"), n); };
			S.Items.Add(It);
		}
	}
	else if (bMapSelect)
	{''')
ed1(M, '''		static const TCHAR* Subs[] = { TEXT("ANCIENT COLLISION MEADOWS"), TEXT("YOU + ALLY  VS  TWO"), TEXT("PICK YOUR ARENA"), TEXT("1V1 PRACTICE"), TEXT("EPISODE 1") };
		static const TCHAR* Elem[] = { TEXT("Fire"), TEXT("Shock"), TEXT(""), TEXT("Frost"), TEXT("Void") };
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Cards); ++i)
		{
			ZoogiP5::FItem It;
			It.Name = Cards[i].Name; It.Sub = Subs[i];
			UTexture2D* T = Art(Cards[i].Art); It.Art = T; if (T) W->Keep.Add(T);
			It.Col = Elem[i][0] ? ZoogiP5::ElementColor(Elem[i]) : ZoogiP5::Accent();''', '''		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Cards); ++i)
		{
			ZoogiP5::FItem It;
			It.Name = Cards[i].Name; It.Sub = Cards[i].Sub; // b154a: per-card (the old tables were one short once RAID was added)
			UTexture2D* T = Art(Cards[i].Art); It.Art = T; if (T) W->Keep.Add(T);
			It.Col = Cards[i].Elem[0] ? ZoogiP5::ElementColor(Cards[i].Elem) : ZoogiP5::Accent();''')
ed1(M, '''	S.Back = [Self]() { if (!Self.IsValid()) return; if (Self->bMapSelect) {''', '''	S.Back = [Self]() { if (!Self.IsValid()) return; if (ZoogiModeSel::GCountSel && !Self->bMapSelect) { ZoogiModeSel::GCountSel = false; Self->bDirty = true; return; } if (Self->bMapSelect) { ZoogiModeSel::GMultiN = 0; ZoogiModeSel::GRaidMaps = false;''')
# StartModeCard by id
ed1(M, '''	switch (Card)
	{
	case 0: // BATTLE ROYALE''', '''	const ZoogiModeSel::ECardId Id = Card >= 0 && Card < (int32)UE_ARRAY_COUNT(ZoogiModeSel::Cards) ? ZoogiModeSel::Cards[Card].Id : ZoogiModeSel::ECardId::Story;
	if (Id == ZoogiModeSel::ECardId::Multi)
	{	// b154a MULTIPLAYER: players step (P5 menu) -> map select; the older card menu goes straight to the maps with 2 players
		ZoogiModeSel::GRaidMaps = false;
		if (ZoogiP5::MenuOn()) { ZoogiModeSel::GCountSel = true; ZoogiModeSel::GMultiN = 0; }
		else { ZoogiModeSel::GMultiN = 2; bMapSelect = true; }
		bTrainingMaps = false; bDirty = true;
		return;
	}
	Card = Id == ZoogiModeSel::ECardId::Royale ? 0 : Id == ZoogiModeSel::ECardId::Classic ? 2 : Id == ZoogiModeSel::ECardId::Training ? 3 : Id == ZoogiModeSel::ECardId::Story ? 4 : 5; // the b151 numbering below
	ZoogiModeSel::GMultiN = 0;
	switch (Card)
	{
	case 0: // BATTLE ROYALE''')
ed1(M, '''	Fire("Mode", 0);
	const bool bRaid = ZoogiModeSel::GRaidMaps; ZoogiModeSel::GRaidMaps = false;
	Fire("Players", bRaid ? 3 : 4); // b151 RAID: you + 2 CPU allies
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0;''', '''	Fire("Mode", 0);
	const bool bRaid = ZoogiModeSel::GRaidMaps; ZoogiModeSel::GRaidMaps = false;
	const int32 MultiN = ZoogiModeSel::GMultiN; ZoogiModeSel::GMultiN = 0;
	Fire("Players", MultiN > 0 ? MultiN : bRaid ? 3 : 4); // b151 RAID: you + 2 CPU allies; b154a MULTIPLAYER: N humans
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = MultiN > 0 ? false : i > 0;
	if (MultiN > 0) UE_LOG(LogZoogi, Log, TEXT("MULTIPLAYER pass & play: %d human players on %s"), MultiN, M.Name);''')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B153"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B154"), TEXT("MULTIPLAYER + NEW MODE LIST"), TEXT("BATTLE -> MULTIPLAYER replaces 2V2: pick 2, 3 or 4 players, pick the map, and pass the phone - everyone plays classic on one device. RAID now sits right under CLASSIC.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o))
print('b154a ok')
