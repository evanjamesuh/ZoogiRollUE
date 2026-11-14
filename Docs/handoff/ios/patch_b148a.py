# b148a (Evan 17:48 "for each map use the characters we bought as placeholders for now just tailor them for each map"):
# every classic arena gets its own boss on the rim - the Mecha Aurex pack meshes (SK_Mecha_Aurex / _02 / _03) in that map's colours,
# eye glow and name; they watch, react and attack exactly like the Cosmos boss. Not in Battle Royale, Combo, solo Training or the
# Ancient (royale) map. -ZoogiBossCosmicOnly = the b147 Cosmos-only boss.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
C = 'Private/ZoogiCosmosBoss.cpp'
ed1(C, '''		const FLinearColor EyeRed(1.f, 0.12f, 0.08f);''', '''		FLinearColor EyeRed(1.f, 0.12f, 0.08f); // b148a: the map boss's eye colour (set by Spawn)
		// b148a: one boss per map - the bought Mecha Aurex meshes, tailored (placeholders until the real bosses)
		struct FTheme { const TCHAR* Name; const TCHAR* Mesh; FLinearColor R, G, B, Eye, Tint; };
		FTheme GTheme = { TEXT("VOID TITAN"), TEXT("SK_Mecha_Aurex_03"), FLinearColor(0.22f, 0.07f, 0.42f), FLinearColor(0.04f, 0.02f, 0.08f), FLinearColor(0.45f, 0.12f, 0.7f), FLinearColor(1.f, 0.12f, 0.08f), FLinearColor(0.6f, 0.25f, 1.f) };
		FTheme ThemeFor(EZoogiArena A)
		{	// armour (R zone), joints (G zone), trim (B zone), eyes, the puppet / FX tint
			switch (A)
			{
			case EZoogiArena::Meadow:   return { TEXT("GROVE TITAN"),    TEXT("SK_Mecha_Aurex"),    FLinearColor(0.10f, 0.32f, 0.08f), FLinearColor(0.12f, 0.07f, 0.03f), FLinearColor(0.55f, 0.42f, 0.12f), FLinearColor(1.f, 0.7f, 0.1f),   FLinearColor(0.45f, 0.9f, 0.3f) };
			case EZoogiArena::Frozen:   return { TEXT("FROST TITAN"),    TEXT("SK_Mecha_Aurex_02"), FLinearColor(0.75f, 0.88f, 1.0f),  FLinearColor(0.08f, 0.16f, 0.3f), FLinearColor(0.2f, 0.6f, 1.0f),   FLinearColor(0.2f, 0.9f, 1.f),   FLinearColor(0.5f, 0.85f, 1.f) };
			case EZoogiArena::Volcanic: return { TEXT("MAGMA TITAN"),    TEXT("SK_Mecha_Aurex_03"), FLinearColor(0.07f, 0.05f, 0.04f), FLinearColor(0.02f, 0.01f, 0.01f), FLinearColor(1.0f, 0.32f, 0.04f), FLinearColor(1.f, 0.45f, 0.05f), FLinearColor(1.f, 0.4f, 0.1f) };
			case EZoogiArena::Arabian:  return { TEXT("DJINN TITAN"),    TEXT("SK_Mecha_Aurex_02"), FLinearColor(0.8f, 0.6f, 0.18f),  FLinearColor(0.05f, 0.18f, 0.22f), FLinearColor(0.1f, 0.75f, 0.7f), FLinearColor(0.15f, 1.f, 0.85f), FLinearColor(0.2f, 0.9f, 0.8f) };
			case EZoogiArena::Tomb:     return { TEXT("PHARAOH TITAN"),  TEXT("SK_Mecha_Aurex"),    FLinearColor(0.62f, 0.5f, 0.3f),  FLinearColor(0.1f, 0.07f, 0.04f), FLinearColor(0.9f, 0.7f, 0.15f),  FLinearColor(0.3f, 1.f, 0.35f),  FLinearColor(0.95f, 0.8f, 0.3f) };
			case EZoogiArena::Neon:     return { TEXT("NEON TITAN"),     TEXT("SK_Mecha_Aurex_02"), FLinearColor(0.03f, 0.03f, 0.05f), FLinearColor(0.01f, 0.01f, 0.02f), FLinearColor(1.0f, 0.1f, 0.6f),  FLinearColor(0.1f, 0.95f, 1.f),  FLinearColor(1.f, 0.2f, 0.75f) };
			case EZoogiArena::Teaser:   return { TEXT("STAR TITAN"),     TEXT("SK_Mecha_Aurex"),    FLinearColor(0.9f, 0.75f, 0.2f),  FLinearColor(0.1f, 0.1f, 0.25f),  FLinearColor(1.0f, 1.0f, 0.9f),   FLinearColor(1.f, 0.95f, 0.5f),  FLinearColor(1.f, 0.9f, 0.4f) };
			case EZoogiArena::Training: return { TEXT("TRAINING TITAN"), TEXT("SK_Mecha_Aurex"),    FLinearColor(0.35f, 0.38f, 0.42f), FLinearColor(0.05f, 0.05f, 0.06f), FLinearColor(0.2f, 0.85f, 0.4f),  FLinearColor(0.3f, 1.f, 0.4f),   FLinearColor(0.4f, 1.f, 0.5f) };
			default:                    return { TEXT("VOID TITAN"),     TEXT("SK_Mecha_Aurex_03"), FLinearColor(0.22f, 0.07f, 0.42f), FLinearColor(0.04f, 0.02f, 0.08f), FLinearColor(0.45f, 0.12f, 0.7f), FLinearColor(1.f, 0.12f, 0.08f), FLinearColor(0.6f, 0.25f, 1.f) };
			}
		}
		bool ArenaHasBoss(const AZoogiGameMode* G)
		{
			if (!G || !G->Arena || G->bRoyaleMode || G->bComboMode || G->bSoloTraining) return false;
			const EZoogiArena A = G->Arena->GetTheme();
			if (A == EZoogiArena::Ancient) return false;
			static const bool bCosmicOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossCosmicOnly"));
			return A == EZoogiArena::Cosmic || !bCosmicOnly || FParse::Param(FCommandLine::Get(), TEXT("ZoogiCosmosBossAll"));
		}''')
ed1(C, '''			USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex_03.SK_Mecha_Aurex_03"), nullptr, LOAD_NoWarn | LOAD_Quiet);''', '''			GTheme = ThemeFor(G->Arena->GetTheme()); EyeRed = GTheme.Eye; // b148a
			USkeletalMesh* SM = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Mecha_Aurex/Meshes/%s.%s"), GTheme.Mesh, GTheme.Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!SM) SM = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex_03.SK_Mecha_Aurex_03"), nullptr, LOAD_NoWarn | LOAD_Quiet);''')
ed1(C, '''				if (!P) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: peek puppet failed")); return; }
				if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, P);
					MID->SetVectorParameterValue(TEXT("Color_Zone_R"), FLinearColor(0.22f, 0.07f, 0.42f));
					MID->SetVectorParameterValue(TEXT("Color_Zone_G"), FLinearColor(0.04f, 0.02f, 0.08f));
					MID->SetVectorParameterValue(TEXT("Color_Zone_B"), FLinearColor(0.45f, 0.12f, 0.7f));''', '''				if (!P) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: peek puppet failed")); return; }
				P->Tint = GTheme.Tint;
				if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, P);
					MID->SetVectorParameterValue(TEXT("Color_Zone_R"), GTheme.R); // b148a: the map's colours
					MID->SetVectorParameterValue(TEXT("Color_Zone_G"), GTheme.G);
					MID->SetVectorParameterValue(TEXT("Color_Zone_B"), GTheme.B);''')
ed1(C, '''				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peeks: %s, %.0f m tall, hands on the rim at yaw %.0f"), *SM->GetName(), PeekH / 100.f, Ang0);''', '''				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS peeks: %s (%s), %.0f m tall, hands on the rim at yaw %.0f"), *SM->GetName(), GTheme.Name, PeekH / 100.f, Ang0);''')
ed1(C, '''		if (G->Arena->GetTheme() != EZoogiArena::Cosmic && !AllArenas()) return;''', '''		if (!ArenaHasBoss(G)) return; // b148a: every classic map''')
ed1(C, '''		if (!G || !B.A.IsValid()) { if (G && !B.A.IsValid() && !Off() && G->Arena && (G->Arena->GetTheme() == EZoogiArena::Cosmic || AllArenas())) Spawn(G); return; }''', '''		if (!G || !B.A.IsValid()) { if (G && !B.A.IsValid() && !Off() && ArenaHasBoss(G)) Spawn(G); return; }''')
ed1(C, '''	bool Present(const AZoogiGameMode* G) { return G && B.G.Get() == G && B.A.IsValid() && B.Pup.IsValid(); }''', '''	bool Present(const AZoogiGameMode* G) { return G && B.G.Get() == G && B.A.IsValid() && B.Pup.IsValid(); }
	FString Name() { return GTheme.Name; }
	FLinearColor Tint() { return GTheme.Tint; }''')
ed1('Public/ZoogiCosmosBoss.h', '''	ZOOGIROLL_API bool Present(const AZoogiGameMode* G);''', '''	ZOOGIROLL_API bool Present(const AZoogiGameMode* G);
	/** b148a: this map's boss ("FROST TITAN" ...) and its colour */
	ZOOGIROLL_API FString Name();
	ZOOGIROLL_API FLinearColor Tint();''')
# boss attack: the map boss's name + colour
BA = 'Private/ZoogiBossAttack.cpp'
ed1(BA, '''				F->PublicCallout(bSw ? TEXT("BOSS: HAND SWEEP!") : TEXT("BOSS: GROUND SLAM!"),''', '''				F->PublicCallout(FString::Printf(TEXT("%s: %s"), *ZoogiCosmosBoss::Name(), bSw ? TEXT("HAND SWEEP!") : TEXT("GROUND SLAM!")),''')
ed1(BA, '''			G->PublicFeed(FString::Printf(TEXT("THE BOSS IS COMING FOR P%d %s - IT LANDS AFTER EVERYONE'S TURN"), T->PlayerIndex + 1, *T->Def.Name.ToUpper()), Red);''', '''			G->PublicFeed(FString::Printf(TEXT("THE %s IS COMING FOR P%d %s - IT LANDS AFTER EVERYONE'S TURN"), *ZoogiCosmosBoss::Name(), T->PlayerIndex + 1, *T->Def.Name.ToUpper()), Red);''')
ed1(BA, '''				AZoogiFX::Spawn(W, Hp, Violet, 260.f, 0.35f, false, 9.f);''', '''				AZoogiFX::Spawn(W, Hp, ZoogiCosmosBoss::Tint(), 260.f, 0.35f, false, 9.f); // b148a: the map boss's colour''')
print('b148a ok')
