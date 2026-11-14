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
    print('b162a ok', len(EDITS))
# b162a (Evan Nov 6 12:57 "make that ring twice as big ... increase the overall map"; picked "Grow ring, push islands out"): Ancient
# Collision Meadows v15 = Blender build_arena_v15.py (plaza ring r17.5 -> r35 m, islands / volcano / viaduct / moat / islets / base pushed
# out 17.5 m, props keep their size, no centre drums) imported to /Game/Env/AncientV15. Rules layout, spawns (r19 m), orb ring (r21 m, 20
# orbs) and the throne follow. -ZoogiAncientV14 = the b161 arena.
EDITS.clear()
L = 'Private/ZoogiAncientLayout.h'
ed1(L, '''	#include "ZoogiAncientV14Data.inl"
''', '''	#include "ZoogiAncientV14Data.inl"
	#include "ZoogiAncientV15Data.inl"
	/** b162: the v15 arena (2x plaza ring) is the default; -ZoogiAncientV14 = the v14 diorama. */
	inline bool V15() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV14")); return b; }
''')
ed1(L, '''		FLayout O;
		O.HubR = 1720.f; O.SpireR = 0.f;
		auto Circle''', '''		FLayout O;
		const bool b15 = V15(); // b162
		O.HubR = b15 ? 3470.f : 1720.f; O.SpireR = 0.f;
		auto Circle''')
ed1(L, '''		Circle(TEXT("hub"), Hub, FVector2D(0.f, 0.f), 1720.f, 0.f, 32, 5.625f);
		Circle(TEXT("volcano_platform"), Hub, FVector2D(0.f, -3700.f), 1900.f, -180.f, 32, 5.625f);
		Circle(TEXT("island_ruins"), North, FVector2D(-3000.f, -3500.f), 950.f, 450.f, 8, 22.5f);
		Circle(TEXT("island_citadel"), Citadel, FVector2D(3000.f, -3500.f), 950.f, 450.f, 8, 22.5f);
		Circle(TEXT("island_emerald"), Emerald, FVector2D(-3300.f, 300.f), 750.f, 0.f, 8, 22.5f);
		Circle(TEXT("island_sunstone"), Sunstone, FVector2D(3450.f, -750.f), 750.f, 0.f, 8, 22.5f);''', '''		if (b15)
		{	// b162: layout_v15.json (x 100, y (y - 16) x 100)
			Circle(TEXT("hub"), Hub, FVector2D(0.f, 0.f), 3470.f, 0.f, 48, 3.75f);
			Circle(TEXT("volcano_platform"), Hub, FVector2D(0.f, -5450.f), 1900.f, -180.f, 32, 5.625f);
			Circle(TEXT("island_ruins"), North, FVector2D(-4139.f, -4829.f), 950.f, 450.f, 8, 22.5f);
			Circle(TEXT("island_citadel"), Citadel, FVector2D(4139.f, -4829.f), 950.f, 450.f, 8, 22.5f);
			Circle(TEXT("island_emerald"), Emerald, FVector2D(-5043.f, 458.f), 750.f, 0.f, 8, 22.5f);
			Circle(TEXT("island_sunstone"), Sunstone, FVector2D(5160.f, -1122.f), 750.f, 0.f, 8, 22.5f);
		}
		else
		{
		Circle(TEXT("hub"), Hub, FVector2D(0.f, 0.f), 1720.f, 0.f, 32, 5.625f);
		Circle(TEXT("volcano_platform"), Hub, FVector2D(0.f, -3700.f), 1900.f, -180.f, 32, 5.625f);
		Circle(TEXT("island_ruins"), North, FVector2D(-3000.f, -3500.f), 950.f, 450.f, 8, 22.5f);
		Circle(TEXT("island_citadel"), Citadel, FVector2D(3000.f, -3500.f), 950.f, 450.f, 8, 22.5f);
		Circle(TEXT("island_emerald"), Emerald, FVector2D(-3300.f, 300.f), 750.f, 0.f, 8, 22.5f);
		Circle(TEXT("island_sunstone"), Sunstone, FVector2D(3450.f, -750.f), 750.f, 0.f, 8, 22.5f);
		}''')
ed1(L, '''R.A = GV14Ramp[r][i]; R.B = GV14Ramp[r][i + 1];''', '''R.A = b15 ? GV15Ramp[r][i] : GV14Ramp[r][i]; R.B = b15 ? GV15Ramp[r][i + 1] : GV14Ramp[r][i + 1];''')
ed1(L, '''		{ FRamp R; R.Name = TEXT("volcano_ramp"); R.A = FVector(0.f, -1720.f, 0.f); R.B = FVector(0.f, -2600.f, -180.f); R.HalfW = 300.f; R.Region = Bridge; O.Ramps.Add(R); }
		{ FRamp R; R.Name = TEXT("front_stairs"); R.A = FVector(0.f, 1750.f, 0.f); R.B = FVector(0.f, 3730.f, -640.f); R.HalfW = 275.f; R.Region = Hub; O.Ramps.Add(R); }
		const FVector Via[4] = { FVector(-2400.f, -4200.f, 450.f), FVector(-1200.f, -5850.f, 450.f), FVector(1200.f, -5850.f, 450.f), FVector(2400.f, -4200.f, 450.f) };''', '''		{ FRamp R; R.Name = TEXT("volcano_ramp"); R.A = b15 ? FVector(0.f, -3470.f, 0.f) : FVector(0.f, -1720.f, 0.f); R.B = b15 ? FVector(0.f, -4350.f, -180.f) : FVector(0.f, -2600.f, -180.f); R.HalfW = 300.f; R.Region = Bridge; O.Ramps.Add(R); }
		{ FRamp R; R.Name = TEXT("front_stairs"); R.A = b15 ? FVector(0.f, 3500.f, 0.f) : FVector(0.f, 1750.f, 0.f); R.B = b15 ? FVector(0.f, 5480.f, -640.f) : FVector(0.f, 3730.f, -640.f); R.HalfW = 275.f; R.Region = Hub; O.Ramps.Add(R); }
		const FVector Via14[4] = { FVector(-2400.f, -4200.f, 450.f), FVector(-1200.f, -5850.f, 450.f), FVector(1200.f, -5850.f, 450.f), FVector(2400.f, -4200.f, 450.f) };
		const FVector Via15[4] = { FVector(-3539.f, -5529.f, 450.f), FVector(-1200.f, -7600.f, 450.f), FVector(1200.f, -7600.f, 450.f), FVector(3539.f, -5529.f, 450.f) };
		const FVector* Via = b15 ? Via15 : Via14;''')
ed1(L, '''			const FVector S = GV14Spawns[k];''', '''			const FVector S = b15 ? GV15Spawns[k] : GV14Spawns[k];''')
A = 'Private/ZoogiArenaAncientV14.cpp'
ed1(A, '''		UStaticMeshComponent* C = AddMesh(M, nullptr, P.bCollide, P.bShadow);''', '''		if (ZoogiAncient::V15())
		{	// b162: the v15 pieces (2x plaza ring); falls back to v14 if an asset is missing
			if (UStaticMesh* M15 = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/AncientV15/%s.%s"), P.Name, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet)) M = M15;
			else UE_LOG(LogZoogi, Warning, TEXT("ANCIENT v15: %s missing - v14 piece used"), P.Name);
		}
		UStaticMeshComponent* C = AddMesh(M, nullptr, P.bCollide, P.bShadow);''')
ed1(A, '''	SpawnRadiusOverride = 950.f;                                  // Grok's plaza spawns (r9.5 m)
	OrbCountOverride = 14; OrbRingRadiusOverride = 1150.f; OrbRingYawOffset = 13.f; // between the drum ring (r7.2 m) and the bollards (r14.6 m)''', '''	SpawnRadiusOverride = ZoogiAncient::V15() ? 1900.f : 950.f;   // Grok's plaza spawns (r9.5 m; b162 v15 r19 m)
	OrbCountOverride = 14; OrbRingRadiusOverride = 1150.f; OrbRingYawOffset = 13.f; // between the drum ring (r7.2 m) and the bollards (r14.6 m)
	if (ZoogiAncient::V15()) { OrbCountOverride = 20; OrbRingRadiusOverride = 2100.f; OrbRingYawOffset = 9.f; } // b162: the 2x plaza (bollards r29 m)''')
T = 'Private/ZoogiThrone.cpp'
ed1(T, '''			const FVector2D Isle(3000.f, -3500.f);''', '''			const FVector2D Isle = ZoogiAncient::V15() ? FVector2D(4139.f, -4829.f) : FVector2D(3000.f, -3500.f); // b162: v15 citadel''')
ed1(T, '''#include "ZoogiArena.h"
''', '''#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
''')
F2 = 'Private/ZoogiFront.cpp'
ed1(F2, '\t\t\t{ TEXT("B161"),', '''\t\t\t{ TEXT("B162"), TEXT("ANCIENT MEADOWS: TWICE THE RING"), TEXT("The battle ring on Ancient Collision Meadows is now twice as wide. The islands, volcano and bridges moved out with it, so the whole map is bigger.") },
\t\t\t{ TEXT("B161"),''')
run()
