# b178e (Evan: "make this the new ... underworld realm for episode 4 lvl 1"): THE UNDERREALM - Grok's Shadow Realm v3 dressed over the
# Cosmic court (same collider, physics, camera): basalt terraces, spires, ruins, ghostfire braziers, sigil, void. Story stop ep4_a + mission
# m4_gate use env=Underrealm. -ZoogiUnderrealm (with -ZoogiArena=3) = a capture run of it outside the story.
import os,sys
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
def patch(f,pairs):
    s=open(R+f,'rb').read().decode('utf-8')
    for o,n,*c in pairs:
        cnt=c[0] if c else 1; done=False
        for oo,nn in ((o,n),(o.replace('\n','\r\n'),n.replace('\n','\r\n'))):
            if s.count(oo)==cnt: s=s.replace(oo,nn); done=True; break
        if not done: print('MISS',f,repr(o[:70]),s.count(o)); sys.exit(1)
    open(R+f,'wb').write(s.encode('utf-8')); print('OK',f)
h=R+'Source/ZoogiRoll/Public/ZoogiArena.h'; s=open(h,'rb').read().decode('utf-8'); nl='\r\n' if '\r\n' in s else '\n'
if 'namespace ZoogiUnderrealm' not in s:
    s=s.rstrip()+nl+nl+'// b178e: THE UNDERREALM - the Cosmic court dressed as Grok\'s Shadow Realm v3 (Episode 4 stop 1)'+nl+'namespace ZoogiUnderrealm { ZOOGIROLL_API void Set(bool bOn); ZOOGIROLL_API bool On(); }'+nl
    open(h,'wb').write(s.encode('utf-8')); print('OK header')
patch('Source/ZoogiRoll/Private/ZoogiArena.cpp',[
('''void AZoogiArena::BuildCosmic()
{''','''namespace ZoogiUnderrealm
{
	static bool GOn = false;
	void Set(bool bOn) { if (GOn != bOn) UE_LOG(LogZoogi, Log, TEXT("UNDERREALM %s"), bOn ? TEXT("on") : TEXT("off")); GOn = bOn; }
	bool On() { static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUnderrealm")); return GOn || bTest; }
}

void AZoogiArena::BuildCosmic()
{
	if (ZoogiUnderrealm::On() && LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/ShadowRealm/SM_Shadow_Floor.SM_Shadow_Floor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{	// b178e: THE UNDERREALM - the court collider of the Cosmic build, Grok's Shadow Realm v3 on top (scaled so its 17.2 m court
		// sits on our court edge). Decor never collides; the 0.6 m rim is a low visual lip (balls roll off as on every map); the mist
		// proxy stays hidden (no native fog volume yet).
		const int32 FloorIdxU = Built.Num();
		BuildIsland(ZoogiUtil::Hex(TEXT("#16121F")), ZoogiUtil::Hex(TEXT("#221A30")), ZoogiUtil::Hex(TEXT("#5B3DA8")),
			ZoogiUtil::Hex(TEXT("#0C0A12")), ZoogiUtil::Hex(TEXT("#181226")), 0.3f, KitFloorMaterial(TEXT("Cosmic"), ZoogiUtil::Hex(TEXT("#6B4FD8")), 1.0f, 0.2f));
		if (Built.IsValidIndex(FloorIdxU)) if (UStaticMeshComponent* SC = Cast<UStaticMeshComponent>(Built[FloorIdxU])) SC->SetVisibility(false); // collision unchanged
		const float K = ZoogiConst::KnockoffRadius / 1720.f;
		float RimZ = 0.35f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiUnderRimZ="), RimZ);
		struct FU { const TCHAR* N; float Lift; bool bRim; };
		static const FU Us[] = { { TEXT("SM_Shadow_Floor"), 0.f, false }, { TEXT("SM_Shadow_Emblem"), 0.6f, false }, { TEXT("SM_Shadow_Rim"), 0.f, true },
			{ TEXT("SM_Shadow_Foundation"), 0.f, false }, { TEXT("SM_Shadow_Trim"), 0.f, true }, { TEXT("SM_Shadow_Rocks"), 0.f, false },
			{ TEXT("SM_Shadow_Energy"), 0.f, false }, { TEXT("SM_Shadow_Environment"), 0.f, false }, { TEXT("SM_Shadow_Architecture"), 0.f, false },
			{ TEXT("SM_Shadow_Ghostfire"), 0.f, false }, { TEXT("SM_Shadow_Void"), 0.f, false } };
		int32 Nu = 0;
		for (const FU& U : Us)
			if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/ShadowRealm/%s.%s"), U.N, U.N), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UStaticMeshComponent* C = AddMesh(M, nullptr, false, false);
				C->SetRelativeScale3D(FVector(K, K, U.bRim ? K * RimZ : K)); C->SetRelativeLocation(FVector(0.f, 0.f, U.Lift));
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->ComponentTags.Add(TEXT("NoCamBlock"));
				++Nu;
			}
		AddCylinder(FVector2D::ZeroVector, ZoogiConst::KnockoffRadius + 15.f, 20.f, -2.f, ZoogiUtil::GlowMID(this, ZoogiUtil::Hex(TEXT("#7C4DFF")), 0.9f), false); // the violet lip
		AddPointLight(FVector(0, 0, 420.f), ZoogiUtil::Hex(TEXT("#8FB8FF")), 90.f, 2200.f);   // silver-blue key
		AddPointLight(FVector(0, 0, -900.f), ZoogiUtil::Hex(TEXT("#7A4DFF")), 650.f, 2800.f); // violet underlight
		for (int32 b = 0; b < 4; ++b) AddPointLight(FVector(FMath::Cos(b * 1.5708f + 0.785f), FMath::Sin(b * 1.5708f + 0.785f), 0.f) * (ZoogiConst::KnockoffRadius + 600.f) + FVector(0, 0, 160.f),
			b % 2 ? ZoogiUtil::Hex(TEXT("#B07CFF")) : ZoogiUtil::Hex(TEXT("#FFB45A")), 60.f, 900.f); // ghostfire glow
		AddPanoramaSky(TEXT("Cosmic"), 0.25f, 0.f, FLinearColor::Black);
		UE_LOG(LogZoogi, Log, TEXT("UNDERREALM built: %d / %d Shadow Realm pieces, scale %.3f, rim height x%.2f"), Nu, (int32)UE_ARRAY_COUNT(Us), K, RimZ);
		return;
	}'''),
])
# ---- story routing
patch('Source/ZoogiRoll/Private/ZoogiStoryMode.inl',[
('''	struct FSpot { FString Chapter, Id, Label, Sub, Scene, Mission; EZoogiArena Env = EZoogiArena::Meadow; };''','''	struct FSpot { FString Chapter, Id, Label, Sub, Scene, Mission; EZoogiArena Env = EZoogiArena::Meadow; bool bUnder = false; }; // b178e: env=Underrealm'''),
('''			S.Env = ArenaOf(X.FindRef(TEXT("env")));''','''			S.Env = ArenaOf(X.FindRef(TEXT("env"))); S.bUnder = X.FindRef(TEXT("env")).Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase);'''),
('''M.Env = ArenaOf(X.FindRef(TEXT("env")));''','''M.Env = ArenaOf(X.FindRef(TEXT("env"))); M.bUnder = X.FindRef(TEXT("env")).Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase);'''),
('''	static EZoogiArena ArenaOf(const FString& S)
	{''','''	static EZoogiArena ArenaOf(const FString& S)
	{
		if (S.Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase)) return EZoogiArena::Cosmic; // b178e: the Cosmic court, Shadow Realm dressing'''),
('''	static void SetEnv(EZoogiArena A)
	{
		AZoogiGameMode* G = GM();
		if (G && G->Phase == EZoogiPhase::Menu && G->MenuArena != A)''','''	static void SetEnv(EZoogiArena A, bool bUnder = false)
	{
		AZoogiGameMode* G = GM();
		const bool bFlip = ZoogiUnderrealm::On() != bUnder;
		ZoogiUnderrealm::Set(bUnder); // b178e
		if (G && G->Phase == EZoogiPhase::Menu && (G->MenuArena != A || bFlip))'''),
('''SetEnv(GDB.Spots[At].Env)''','''SetEnv(GDB.Spots[At].Env, GDB.Spots[At].bUnder)'''),
('''SetEnv(GDB.Spots[GS.Spot].Env)''','''SetEnv(GDB.Spots[GS.Spot].Env, GDB.Spots[GS.Spot].bUnder)''',3),
('''SetEnv(GDB.Spots[Si].Env)''','''SetEnv(GDB.Spots[Si].Env, GDB.Spots[Si].bUnder)'''),
('''G->MenuArena != GDB.Spots[Si].Env''','''(G->MenuArena != GDB.Spots[Si].Env || ZoogiUnderrealm::On() != GDB.Spots[Si].bUnder)'''),
('''SetEnv(M->Env)''','''SetEnv(M->Env, M->bUnder)'''),
('''SetEnv(M2->Env)''','''SetEnv(M2->Env, M2->bUnder)'''),
('''SetEnv(ZoogiStoryData::ArenaOf(F.Mid(4)))''','''SetEnv(ZoogiStoryData::ArenaOf(F.Mid(4)), F.Mid(4).Equals(TEXT("Underrealm"), ESearchCase::IgnoreCase))'''),
])
