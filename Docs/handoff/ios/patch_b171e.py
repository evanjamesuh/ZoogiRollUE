import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
dry='dry' in sys.argv
f='Private/ZoogiArenaMeadow.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:50].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('''	AddCurb(FloorR + 40.f, -2.f, 55.f, ZoogiUtil::GroundMID(this, Soil, ZoogiUtil::Hex(TEXT("#7A6B55")), 0.01f, 0.92f));''',
'''	// b171e (Evan: "take the ring and the stands from the new meadows fbx and apply it to our existing meadow hollow arena"): Grok's
	// Meadow reference rim + the two purple pavilion stands with their Zoogi crowds (/Game/Env/MeadowRef); -ZoogiOldMeadowRing = the old ones
	static const bool bNoMeadowRef = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeadowRing"));
	const bool bMeadowRef = !bNoMeadowRef && LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/MeadowRef/SM_MeadowRef_Rim.SM_MeadowRef_Rim"), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
	if (!bMeadowRef) AddCurb(FloorR + 40.f, -2.f, 55.f, ZoogiUtil::GroundMID(this, Soil, ZoogiUtil::Hex(TEXT("#7A6B55")), 0.01f, 0.92f));''')
ed('''	UPhysicalMaterial* StonePM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.35f, false);
''','''	UPhysicalMaterial* StonePM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.35f, false);
	if (bMeadowRef)
	{	// Grok's court is r17.2 m: scale so its edge sits on our floor edge. The rim is a low visual lip (no collision, so a Zoogi
		// still rolls off the edge exactly as before); the stands are solid (complex collision), the crowds visual.
		const float K = FloorR / 1720.f;
		float RimZ = 0.2f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiMeadowRimZ="), RimZ);
		struct FRef { const TCHAR* N; bool bCol; bool bRim; };
		static const FRef Refs[] = { { TEXT("SM_MeadowRef_Rim"), false, true }, { TEXT("SM_MeadowRef_SpectatorStands_Left"), true, false }, { TEXT("SM_MeadowRef_SpectatorStands_Right"), true, false },
			{ TEXT("SM_MeadowRef_ZoogiSpectators_Left"), false, false }, { TEXT("SM_MeadowRef_ZoogiSpectators_Right"), false, false } };
		int32 NRef = 0;
		for (const FRef& Rf : Refs)
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/MeadowRef/%s.%s"), Rf.N, Rf.N), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UStaticMeshComponent* C = AddMesh(M, nullptr, Rf.bCol, true);
				C->SetRelativeScale3D(FVector(K, K, Rf.bRim ? K * RimZ : K));
				if (Rf.bCol) C->SetPhysMaterialOverride(WallPM); else { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->ComponentTags.Add(TEXT("NoCamBlock")); }
				++NRef;
			}
		UE_LOG(LogZoogi, Log, TEXT("MEADOW REF: %d / 5 pieces (rim + stands + crowds), scale %.3f, rim height x%.2f"), NRef, K, RimZ);
	}
''')
ed('''			const bool bStand = Name.Contains(TEXT("Stands")), bRock = Name.Contains(TEXT("Boulder")), bTree = Name.Contains(TEXT("Tree"));
''','''			const bool bStand = Name.Contains(TEXT("Stands")), bRock = Name.Contains(TEXT("Boulder")), bTree = Name.Contains(TEXT("Tree"));
			if (bStand && bMeadowRef) continue; // b171e: the Meadow reference pavilions replace the old stands
''')
ed('''	if (!bB1)
		for (int32 i = 0; i < NumStands; ++i)''','''	if (!bB1 && !bMeadowRef)
		for (int32 i = 0; i < NumStands; ++i)''')
if not dry: open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
