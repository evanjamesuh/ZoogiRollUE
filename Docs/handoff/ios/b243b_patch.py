# Nov 11 (Claude): B243 part 2 - Evan 08:31 "REMOVE ALL of these arches from the game" (the green hedge arch + every arch / gate obstacle)
# and 08:34 "make the effect where the lights get dark a toggle in settings to make permanent" (SHOWTIME dim -> Settings DARK ARENA)
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(rel, pairs, inc=None):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    if inc and '#include "%s"' % inc not in t:
        k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b243\n' % inc + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
patch('Private/ZoogiArenaObstacles.cpp', [('''		const ZoogiObs::FInst& In = ZoogiObs::Insts[i];
		if (In.Theme != Th) continue;''', '''		const ZoogiObs::FInst& In = ZoogiObs::Insts[i];
		if (In.Theme != Th) continue;
		{	// b243 (Evan: "REMOVE ALL of these arches from the game"): no hedge arches, boost arches, boost gates or portal arches (-ZoogiArches = back)
			static const bool bArches = FParse::Param(FCommandLine::Get(), TEXT("ZoogiArches"));
			const FString Nm = FString(In.Name).ToLower(), Mn = FString(ZoogiObs::Meshes[In.Mesh].Name).ToLower();
			if (!bArches && (Nm.Contains(TEXT("arch")) || Nm.Contains(TEXT("gate")) || Mn.Contains(TEXT("arch")) || Mn.Contains(TEXT("gate")))) continue;
		}''')])
patch('Public/ZoogiShowtime.h', [('	ZOOGIROLL_API bool FireArmed(AZoogiGameMode* G);', '''	ZOOGIROLL_API bool FireArmed(AZoogiGameMode* G);
	/** b243 (Evan): Settings > DARK ARENA keeps the SHOWTIME light dim on for the whole match. Call every frame from the match tick. */
	ZOOGIROLL_API void DarkTick(AZoogiGameMode* G);''')])
patch('Private/ZoogiShowtime.cpp', [('void ZoogiShowtime::Begin(AZoogiGameMode* G, AZoogiBall* B)\n{', '''namespace { bool GDarkOn = false; TWeakObjectPtr<UWorld> GDarkW; }
void ZoogiShowtime::DarkTick(AZoogiGameMode* G)
{	// b243: DARK ARENA - the showtime dim, permanent (SHOWTIME itself takes the lights while it runs)
	UWorld* W = G ? G->GetWorld() : nullptr;
	if (!W || GStage != EStage::Off) return;
	const bool bWant = ZoogiControls::DarkArena() && G->Phase != EZoogiPhase::Menu;
	if (bWant)
	{
		if (!GDarkOn || GDarkW.Get() != W || (GLights.Num() == 0 && GSkies.Num() == 0 && !GPP.IsValid())) { DimLights(W, true); GDarkW = W; GDarkOn = true; UE_LOG(LogZoogi, Log, TEXT("DARK ARENA on (%d lights)"), GLights.Num()); }
		SetDim(1.f);
	}
	else if (GDarkOn) { EndAll(); GDarkOn = false; UE_LOG(LogZoogi, Log, TEXT("DARK ARENA off")); }
}

void ZoogiShowtime::Begin(AZoogiGameMode* G, AZoogiBall* B)
{
	if (GDarkOn) { SetDim(0.f); if (APostProcessVolume* P = GPP.Get()) P->Destroy(); GLights.Reset(); GSkies.Reset(); GDarkOn = false; } // b243: hand the lights back first (no double dim)''')], inc='ZoogiFront.h')
patch('Public/ZoogiFront.h', [('	ZOOGIROLL_API bool ReplayCam();    // b70', '	ZOOGIROLL_API bool DarkArena();    // b243 (Evan): Settings > DARK ARENA - the showtime dim for the whole match\n	ZOOGIROLL_API bool ReplayCam();    // b70')])
patch('Private/ZoogiFront.cpp', [
 ('	bool ReplayCam() { return FParse::Param', '	bool DarkArena() { return FParse::Param(FCommandLine::Get(), TEXT("ZoogiDarkArena")) || Get(TEXT("DarkArena"), 0.f, 0.f, 1.f) > 0.5f; } // b243\n	bool ReplayCam() { return FParse::Param'),
 ('''		Toggle(TEXT("REPLAY CAM"), bRep ? TEXT("ON") : TEXT("OFF"), bRep, [Click, bRep]() { ZoogiControls::Set(TEXT("ReplayCam"), bRep ? 0.f : 1.f); if (Click) Click(1.f); });''',
  '''		Toggle(TEXT("REPLAY CAM"), bRep ? TEXT("ON") : TEXT("OFF"), bRep, [Click, bRep]() { ZoogiControls::Set(TEXT("ReplayCam"), bRep ? 0.f : 1.f); if (Click) Click(1.f); });
		const bool bDark = ZoogiControls::DarkArena(); // b243 (Evan: "make the effect where the lights get dark a toggle in settings to make permanent")
		Toggle(TEXT("DARK ARENA"), bDark ? TEXT("ON") : TEXT("OFF"), bDark, [Click, bDark]() { ZoogiControls::Set(TEXT("DarkArena"), bDark ? 0.f : 1.f); if (Click) Click(1.f); });'''),
 ('TEXT("NO SOLDIERS OUTSIDE VOYAGER"), TEXT("Removed the two-legged soldier / robot from Classic, Raid and Battle Royale: in Battle Royale everyone stays a ball, and the Voyager soldier models only appear in the VOYAGER modes.")',
  'TEXT("NO SOLDIERS, NO ARCHES, DARK ARENA"), TEXT("Removed the two-legged soldier / robot from Classic, Raid and Battle Royale: in Battle Royale everyone stays a ball, and the Voyager soldier models only appear in the VOYAGER modes. Every arch and gate obstacle is gone from all arenas. NEW: Settings > DARK ARENA keeps the SHOWTIME dimmed lights on for the whole match.")'),
])
patch('Private/ZoogiActionFun.cpp', [('	ZoogiWallCine::Tick(G, RealDt); // b230: the wall-smash cinematic', '	ZoogiWallCine::Tick(G, RealDt); // b230: the wall-smash cinematic\n	ZoogiShowtime::DarkTick(G); // b243: Settings > DARK ARENA')], inc='ZoogiShowtime.h')
print('ALL OK')
