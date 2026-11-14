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
    print('b167a ok', len(EDITS))
# b167a (Evan Nov 6 21:11 "Fix dev mode" -> "Won't open"): the pause menu's DEV MODE row only flipped ZoogiLayout's flag (the
#  label said DEV MODE: ON, nothing opened). It now opens the real dev panel (the PIN pad first on a locked device) and a second
#  tap closes it; the PIN pad makes sure the overlay exists (it was only created when the DEV button overlay was). -ZoogiDevMenuTest
#  = the auto smoke script opens through that same path.
H = 'Public/ZoogiDevMode.h'; D = 'Private/ZoogiDevMode.cpp'; W = 'Private/ZoogiArenaHUDWidget.cpp'; FR = 'Private/ZoogiFront.cpp'
ed1(H, '\tstatic AZoogiDevMode* Ensure(UWorld* World);\n', '\tstatic AZoogiDevMode* Ensure(UWorld* World);\n\t/** b167: the pause menu DEV MODE row - open the panel (PIN pad when locked) or close it */\n\tstatic void ToggleFromMenu(UWorld* World);\n')
ed1(D, '''void AZoogiDevMode::OnDevButton()
{
	if (ZoogiDevGate::IsUnlocked()) OpenPanel();
	else { Panel = EZoogiDevPanel::Pin; PinEntry.Reset(); PinMsg.Reset(); UGameplayStatics::SetGamePaused(this, true); }
}
''', '''void AZoogiDevMode::OnDevButton()
{
	if (ZoogiDevGate::IsUnlocked()) OpenPanel();
	else
	{
		if (!Overlay.IsValid()) ShowOverlay(true); // b167: the PIN pad lives in the overlay
		Panel = EZoogiDevPanel::Pin; PinEntry.Reset(); PinMsg.Reset(); UGameplayStatics::SetGamePaused(this, true);
		if (APlayerController* P = PC()) P->bShowMouseCursor = true;
		if (Overlay.IsValid()) FSlateApplication::Get().SetAllUserFocus(Overlay, EFocusCause::SetDirectly);
		UE_LOG(LogZoogi, Log, TEXT("DEVMODE: PIN pad"));
	}
}

void AZoogiDevMode::ToggleFromMenu(UWorld* World)
{
	AZoogiDevMode* Dm = Ensure(World);
	if (!Dm) { UE_LOG(LogZoogi, Warning, TEXT("DEVMODE: menu toggle - no game world")); return; }
	if (Dm->Panel == EZoogiDevPanel::Closed) Dm->OnDevButton(); else Dm->ClosePanel();
	UE_LOG(LogZoogi, Log, TEXT("DEVMODE: menu toggle -> panel %d"), (int32)Dm->Panel);
}
''')
ed1(D, '''	if (AutoStep == 0 && AutoT > Start)
	{
		OpenPanel();''', '''	if (AutoStep == 0 && AutoT > Start)
	{
		static const bool bMenuPath = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevMenuTest"));
		if (bMenuPath) ToggleFromMenu(GetWorld()); else OpenPanel(); // b167: the pause menu path''')
ed1(W, 'Rows.Add({ ZoogiLayout::IsDevMode() ? TEXT("DEV MODE: ON") : TEXT("DEV MODE"), 1, [this]() { SetPaused(false); ZoogiLayout::SetDevMode(!ZoogiLayout::IsDevMode()); } });',
       'Rows.Add({ ZoogiLayout::IsDevMode() ? TEXT("CLOSE DEV MODE") : TEXT("DEV MODE"), 1, [this]() { SetPaused(false); AZoogiDevMode::ToggleFromMenu(GetWorld()); } }); // b167: opens the real panel (was: only a flag)')
ed1(FR, '\t\t\t{ TEXT("B166"), TEXT("VOID CALLING: NEW LOOK"),', '\t\t\t{ TEXT("B167"), TEXT("DEV MODE OPENS AGAIN"), TEXT("DEV MODE in the pause menu opens the dev panel again (PIN first on a new device); tap it again to close.") },\n\t\t\t{ TEXT("B166"), TEXT("VOID CALLING: NEW LOOK"),')
run()
