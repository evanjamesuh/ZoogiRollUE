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
    print('b166d ok', len(EDITS))
# b166d (Evan Nov 6 18:35 "Add episode 1 lvl 1 arena as training level"): TRAINING's map select gets a LEAGUE COURT card first
#  (the EP1 stop-1 court, EZoogiArena::Training -> BuildLeagueCourt); StartMapCard(100) = you alone on the court.
MS = 'Private/ZoogiModeSelect.cpp'; FR = 'Private/ZoogiFront.cpp'
ed1(MS, '''	else if (bMapSelect)
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Maps); ++i)
		{
			ZoogiP5::FItem It;''', '''	else if (bMapSelect)
	{
		if (bTrainingMaps)
		{	// b166 (Evan): the EP1 stop-1 LEAGUE COURT is a training level
			ZoogiP5::FItem It;
			It.Name = TEXT("LEAGUE COURT"); It.Sub = TEXT("TRAINING  -  EP1 PRACTICE COURT");
			UTexture2D* T = Art(TEXT("ModeSelect/ms_map_training")); It.Art = T; if (T) W->Keep.Add(T);
			UTexture2D* Tf = Art(TEXT("ModeSelect/ms_map_training_p")); It.ArtFull = Tf; if (Tf) W->Keep.Add(Tf);
			It.Col = FLinearColor::FromSRGBColor(FColor(122, 58, 203));
			It.Fn = [Self]() { if (Self.IsValid()) Self->StartMapCard(100); };
			S.Items.Add(It);
		}
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Maps); ++i)
		{
			ZoogiP5::FItem It;''')
ed1(MS, '''	if (bMapSelect)
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Maps); ++i)
			List.Add(''', '''	if (bMapSelect)
	{
		if (bTrainingMaps) List.Add({ FString(TEXT("ModeSelect/ms_map_training")), FString(TEXT("LEAGUE COURT")), FLinearColor::FromSRGBColor(FColor(122, 58, 203)), [this]() { StartMapCard(100); } }); // b166
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Maps); ++i)
			List.Add(''')
ed1(MS, '''{
	AZoogiGameMode* G = GM();
	if (!G || Index < 0 || Index >= (int32)UE_ARRAY_COUNT(ZoogiModeSel::Maps)) return;''', '''{
	if (Index == 100)
	{	// b166 (Evan "Add episode 1 lvl 1 arena as training level"): TRAINING on the LEAGUE COURT
		AZoogiGameMode* G2 = GM();
		if (!G2) return;
		bMapSelect = false; bTrainingMaps = false;
		Fire("Mode", 0);
		if (G2->MenuArena != EZoogiArena::Training) Fire("Arena", (int32)EZoogiArena::Training);
		UE_LOG(LogZoogi, Log, TEXT("MAP SELECT training: LEAGUE COURT"));
		G2->ClassicStartTraining();
		return;
	}
	AZoogiGameMode* G = GM();
	if (!G || Index < 0 || Index >= (int32)UE_ARRAY_COUNT(ZoogiModeSel::Maps)) return;''')
ed1(FR, '\t\t\t{ TEXT("B166"), TEXT("HARDER HITS"),', '\t\t\t{ TEXT("B166"), TEXT("TRAINING: LEAGUE COURT"), TEXT("The Zoogi League Court from Episode 1 is now a Training level - pick it first in TRAINING\'s map list.") },\n\t\t\t{ TEXT("B166"), TEXT("HARDER HITS"),')
run()
