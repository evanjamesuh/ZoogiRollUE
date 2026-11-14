# b178d (Evan: "make this the new meadows"): the whole Meadow v3 - floor, medallion, circuits, arches, rear gate, crystals, banners,
# flowers, trees, landscape, mountains, village, water, sky details, logs, the six Z rune rocks (solid) - over our court; the old
# decor that overlaps it (batch-1 builder, flower scatter, halls, shrines, mountains, landmarks, realism tufts) steps aside.
# Gameplay colliders (floor, seat wall with its 4 orb-exit gaps, hub dais) stay. -ZoogiMeadowV2 = the b171 look.
import os,sys
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiArenaMeadow.cpp')
s=open(R,'rb').read().decode('utf-8')
def ed(o,n,cnt=1):
    global s
    for oo,nn in ((o,n),(o.replace('\n','\r\n'),n.replace('\n','\r\n'))):
        if s.count(oo)==cnt: s=s.replace(oo,nn); return
    print('MISS',repr(o[:70]),s.count(o)); sys.exit(1)
ed('''	if (!bMeadowRef) AddCurb(FloorR + 40.f, -2.f, 55.f,''','''	static const bool bOldV2 = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMeadowV2"));
	const bool bV3 = bMeadowRef && !bOldV2 && LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/MeadowRef/SM_MeadowRef_Floor.SM_MeadowRef_Floor"), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr; // b178d
	if (!bMeadowRef) AddCurb(FloorR + 40.f, -2.f, 55.f,''')
ed('''		UE_LOG(LogZoogi, Log, TEXT("MEADOW REF: %d / 5 pieces (rim + stands + crowds), scale %.3f, rim height x%.2f"), NRef, K, RimZ);''','''		UE_LOG(LogZoogi, Log, TEXT("MEADOW REF: %d / 5 pieces (rim + stands + crowds), scale %.3f, rim height x%.2f"), NRef, K, RimZ);
		if (bV3)
		{	// b178d: the rest of Meadow v3 (decor has no collision; the Z rune rocks on the paving are solid)
			struct FV3 { const TCHAR* N; bool bCol; float Lift; };
			static const FV3 V3[] = { { TEXT("SM_MeadowRef_Floor"), false, 0.6f }, { TEXT("SM_MeadowRef_CentreMedallion"), false, 1.2f }, { TEXT("SM_MeadowRef_Circuits"), false, 1.6f },
				{ TEXT("SM_MeadowRef_FrontArches"), false, 0.f }, { TEXT("SM_MeadowRef_RearGate"), false, 0.f }, { TEXT("SM_MeadowRef_Crystals"), false, 0.f },
				{ TEXT("SM_MeadowRef_Banners"), false, 0.f }, { TEXT("SM_MeadowRef_Flowers"), false, 0.f }, { TEXT("SM_MeadowRef_Trees"), false, 0.f },
				{ TEXT("SM_MeadowRef_Landscape"), false, 0.f }, { TEXT("SM_MeadowRef_Mountains"), false, 0.f }, { TEXT("SM_MeadowRef_Village"), false, 0.f },
				{ TEXT("SM_MeadowRef_Water"), false, 0.f }, { TEXT("SM_MeadowRef_SkyDetails"), false, 0.f }, { TEXT("SM_MeadowRef_LogsStumps"), false, 0.f },
				{ TEXT("SM_MeadowRef_RuneRock_01"), true, 0.f }, { TEXT("SM_MeadowRef_RuneRock_02"), true, 0.f }, { TEXT("SM_MeadowRef_RuneRock_03"), true, 0.f },
				{ TEXT("SM_MeadowRef_RuneRock_04"), true, 0.f }, { TEXT("SM_MeadowRef_RuneRock_05"), true, 0.f }, { TEXT("SM_MeadowRef_RuneRock_06"), true, 0.f } };
			static const bool bNoRocks = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoRuneRocks"));
			int32 NV3 = 0;
			for (const FV3& Vp : V3)
			{
				if (bNoRocks && Vp.bCol) continue;
				if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/MeadowRef/%s.%s"), Vp.N, Vp.N), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					UStaticMeshComponent* C = AddMesh(M, nullptr, Vp.bCol, Vp.bCol);
					C->SetRelativeScale3D(FVector(K)); C->SetRelativeLocation(FVector(0.f, 0.f, Vp.Lift));
					if (Vp.bCol) C->SetPhysMaterialOverride(StonePM); else { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->ComponentTags.Add(TEXT("NoCamBlock")); C->SetCastShadow(false); }
					++NV3;
				}
			}
			UE_LOG(LogZoogi, Log, TEXT("MEADOW V3: %d / %d pieces%s"), NV3, (int32)UE_ARRAY_COUNT(V3), bNoRocks ? TEXT(" (no rune rocks)") : TEXT(""));
		}''')
# the old decor that overlaps v3
s2=s
n=s.count('\tScatterInstances(')
s=s.replace('\n\tScatterInstances(','\n\tif (!bV3) ScatterInstances(').replace('\r\n\tScatterInstances(','\r\n\tif (!bV3) ScatterInstances(')
print('scatter',n)
ed('''	bool bB1 = false;
	{''','''	bool bB1 = false;
	if (!bV3) // b178d
	{''')
ed('''	for (int32 i = 0; i < 2; ++i)
	{''','''	for (int32 i = 0; i < (bV3 ? 0 : 2); ++i) // b178d
	{''',2)
ed('''	// r4: crisp layered mountains (3 ridges, near green -> far blue) with tall waterfalls, replacing the pale haze.
	{''','''	// r4: crisp layered mountains (3 ridges, near green -> far blue) with tall waterfalls, replacing the pale haze.
	if (!bV3) // b178d
	{''')
ed('''	// r5: landmark buildings where the art has them, on hills past the stands (out of reach: solid scenery).
	{''','''	// r5: landmark buildings where the art has them, on hills past the stands (out of reach: solid scenery).
	if (!bV3) // b178d
	{''')
ed('''	ZoogiRealismApply(this, EZoogiArena::Meadow);''','''	if (!bV3) ZoogiRealismApply(this, EZoogiArena::Meadow); // b178d: v3 has its own flowers / trees''')
open(R,'wb').write(s.encode('utf-8')); print('meadow ok')
