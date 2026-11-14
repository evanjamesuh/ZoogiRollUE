# b139a (Evan 10:37: "Fire Meteor is now a world change effect that happens on the Volcano map randomly and increases in numbers every
# two rounds - a Zoogi / P5 style circle appears on the arena floor shortly before it activates to notify players"):
#  - Volcanic Pit (classic): when a roll starts the volcano may erupt (25% in rounds 1-2, +15% every two rounds, max 85%):
#    5 meteor strikes at random spots (Evan 10:37: "when the event happens, 5 in random spots"). Each spot gets a P5 warning circle (black / red ring, a shrinking countdown ring, "!" plate) 1.6 s before its 3 heavy
#    meteors slam it (the old FIRE METEORS effect, nobody's credit).
#  - FIRE METEORS is no longer a skill card: out of the CPU hands, LOCKED in SKILLS ("now a Volcano event"), never earned.
#  -ZoogiNoVolcanoMeteors = off; -ZoogiVolcanoMeteorsTest = every roll, any arena.
import os
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
R = ROOT + 'Source/ZoogiRoll/'
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		struct FGoldApple {''', '''		// b139a: the Volcano's meteor strikes (warning circle -> meteors)
		struct FVolcWarn { FVector P = FVector::ZeroVector; float R = 260.f; double T0 = 0.0, FireAt = 0.0; bool bFired = false; };
		TArray<FVolcWarn> VolcWarns; int32 VolcTurn = -1;
		struct FGoldApple {''')
ed1(S, '''	TArray<FName> AllSkillIds() { TArray<FName> Out; for (const FDef& D : Defs) if (FCString::Strcmp(D.Id, TEXT("skill_lab")) != 0 && FCString::Strcmp(D.Id, TEXT("halo_disc")) != 0) Out.Add(FName(D.Id)); return Out; }''',
'''	TArray<FName> AllSkillIds() { TArray<FName> Out; for (const FDef& D : Defs) if (FCString::Strcmp(D.Id, TEXT("skill_lab")) != 0 && FCString::Strcmp(D.Id, TEXT("halo_disc")) != 0 && FCString::Strcmp(D.Id, TEXT("fire_meteors")) != 0) Out.Add(FName(D.Id)); return Out; } // b139a: fire_meteors is a Volcano event now''')
# tick: schedule + fire
ed1(S, '''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		WolfTick(G);''', '''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (G && G->Arena && !G->bRoyaleMode)
		{	// b139a (Evan): the Volcano fires meteors at random rolls - more every two rounds - a warning circle first
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVolcanoMeteors"));
			static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVolcanoMeteorsTest"));
			if (!bOff && (bTest || G->Arena->GetTheme() == EZoogiArena::Volcanic) && G->Phase == EZoogiPhase::Rolling && VolcTurn != G->TurnCounter)
			{
				VolcTurn = G->TurnCounter;
				FRandomStream Rv(HashCombine(GetTypeHash(G->GameSeed), GetTypeHash(G->TurnCounter * 7919)));
				const float Chance = FMath::Min(0.85f, 0.25f + 0.15f * ((G->RoundNumber - 1) / 2)); // b139a (Evan): it happens more often every two rounds
				if (bTest || Rv.FRand() < Chance)
				{
					const int32 N = 5; // b139a (Evan 10:37: "when the event happens, 5 in random spots in the arena")
					const FVector C0 = G->Arena->GetActorLocation();
					const float ArR = 1350.f * FMath::Max(1.f, G->Arena->GetActorScale3D().X);
					float Fz = C0.Z;
					if (const AZoogiBall* Ab = G->GetActiveBall()) Fz = Ab->BallLocation().Z - ZoogiConst::ZoogiRadius;
					for (int32 k = 0; k < N; ++k)
					{
						FVolcWarn Wv;
						for (int32 Try = 0; Try < 12; ++Try)
						{
							const float A = Rv.FRandRange(0.f, 2.f * PI), Rr = ArR * FMath::Sqrt(Rv.FRandRange(0.05f, 0.75f));
							Wv.P = FVector(C0.X + FMath::Cos(A) * Rr, C0.Y + FMath::Sin(A) * Rr, Fz + 2.f);
							bool bClose = false; for (const FVolcWarn& O : VolcWarns) if (FVector::Dist2D(O.P, Wv.P) < 420.f) bClose = true;
							if (!bClose) break;
						}
						Wv.T0 = FPlatformTime::Seconds() + 0.25 * k; Wv.FireAt = Wv.T0 + 1.6;
						VolcWarns.Add(Wv);
					}
					ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.5f, 0.6f);
					UE_LOG(LogZoogi, Log, TEXT("VOLCANO meteors: %d strike(s) this roll (round %d)"), N, G->RoundNumber);
				}
			}
			for (int32 i = VolcWarns.Num() - 1; i >= 0; --i)
			{
				FVolcWarn& Wv = VolcWarns[i];
				if (!Wv.bFired && FPlatformTime::Seconds() >= Wv.FireAt) { Wv.bFired = true; WorldFireMeteors(G, Wv.P, Wv.R); }
				if (FPlatformTime::Seconds() > Wv.FireAt + 1.6) VolcWarns.RemoveAt(i);
			}
		}
		WolfTick(G);''')
# draw the warning circles (before the pick check)
ed1(S, '''	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!Picking() || !G || !C || !PC) return;''', '''	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (G && C && PC && VolcWarns.Num())
		{	// b139a: the P5 warning circle on the floor before a Volcano meteor strike
			const double Nw = FPlatformTime::Seconds();
			const float Uw = FMath::Min(C->ClipX, C->ClipY) / 1080.f;
			for (const FVolcWarn& Wv : VolcWarns)
			{
				if (Wv.bFired || Nw < Wv.T0) continue;
				const float K = FMath::Clamp((float)((Nw - Wv.T0) / (Wv.FireAt - Wv.T0)), 0.f, 1.f);
				const float Pulse = 0.6f + 0.4f * FMath::Abs(FMath::Sin((float)Nw * 14.f));
				Ring(C, PC, Wv.P, Wv.R + 18.f, 16.f * Uw, FLinearColor(0.02f, 0.02f, 0.03f, 0.9f), 40, false);
				Ring(C, PC, Wv.P, Wv.R, 9.f * Uw, FLinearColor(1.f, 0.12f, 0.08f, Pulse), 40, false);
				Ring(C, PC, Wv.P, Wv.R * (1.f - K) + 20.f, 6.f * Uw, FLinearColor(1.f, 0.85f, 0.2f, 0.95f), 32, true);
				FVector2D Sp;
				if (PC->ProjectWorldLocationToScreen(Wv.P + FVector(0.f, 0.f, 40.f), Sp, true))
				{
					ZoogiFunCanvasText(C, TEXT("!"), Sp.X, Sp.Y - 30.f * Uw, 64.f * Uw, FLinearColor(1.f, 0.15f, 0.1f), 1, 0);
					ZoogiFunCanvasText(C, TEXT("METEOR!"), Sp.X, Sp.Y + 26.f * Uw, 26.f * Uw, FLinearColor::White, 1, 0);
				}
			}
		}
		if (!Picking() || !G || !C || !PC) return;''')
# the strike itself (exported)
ed1(S, '''	bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName W)
	{''', '''	void WorldFireMeteors(AZoogiGameMode* G, const FVector& At, float R)
	{	// b139a: the old FIRE METEORS effect as a world event - nobody's credit, full strength
		if (!G || !G->GetWorld()) return;
		UWorld* W = G->GetWorld();
		AZoogiFabFX::Play(W, FX(TEXT("NS_Fire_meteors")), At, Sc(FName(TEXT("fire_meteors")), 0.3f), 3.5f);
		AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), At, 1.f);
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		for (int32 k = 0; k < 3; ++k)
		{
			const float A = 2.f * PI * k / 3.f + 0.4f;
			const FVector Hit = At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R * 0.45f;
			Later(G, 0.6f + 0.22f * k, [WG, Hit, k]() { if (AZoogiGameMode* Gm = WG.Get()) { FTuneScope Tn(1.f, FName(TEXT("fire_meteors"))); Blast(Gm, Hit, 260.f, 820.f, 300.f, nullptr, -1); AZoogiFabFX::Play(Gm->GetWorld(), FX(k % 2 ? TEXT("NS_Meteor_hit") : TEXT("NS_Meteor_hit_2")), Hit, Sc(FName(TEXT("meteor_hit")), 0.22f), 2.5f); ZoogiHaptics::Kick(0.4f, Gm->GetWorld(), TEXT("volcano_meteor")); } });
		}
		UE_LOG(LogZoogi, Log, TEXT("VOLCANO meteor strike at (%.0f, %.0f)"), At.X, At.Y);
	}
	bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName W)
	{''')
ed1('Public/ZoogiSkills.h', '''	ZOOGIROLL_API bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName Weapon);''', '''	ZOOGIROLL_API bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName Weapon);
	/** b139a: the Volcano's world meteor strike (the old FIRE METEORS effect) at At */
	ZOOGIROLL_API void WorldFireMeteors(AZoogiGameMode* G, const FVector& At, float R);''')
# lock the card everywhere
F = 'Private/ZoogiFront.cpp'
ed1(F, '''	static bool ZoogiCardLockedInClassic(FName Id) { return Id == FName(TEXT("bank_strike")) || Id == FName(TEXT("orb_relay")) || Id == FName(TEXT("recovery_roll")); }''',
'''	static bool ZoogiCardLockedInClassic(FName Id) { return Id == FName(TEXT("bank_strike")) || Id == FName(TEXT("orb_relay")) || Id == FName(TEXT("recovery_roll")) || Id == FName(TEXT("fire_meteors")); } // b139a: fire meteors = Volcano event''')
P5 = 'Private/ZoogiP5Command.cpp'
ed1(P5, '''					if (Ck == FName(TEXT("bank_strike")) || Ck == FName(TEXT("orb_relay")) || Ck == FName(TEXT("recovery_roll"))) { R.bOn = false; R.Name += TEXT("  - LOCKED"); }''',
'''					if (Ck == FName(TEXT("bank_strike")) || Ck == FName(TEXT("orb_relay")) || Ck == FName(TEXT("recovery_roll")) || Ck == FName(TEXT("fire_meteors"))) { R.bOn = false; R.Name += TEXT("  - LOCKED"); }''')
PC = 'Private/ZoogiFrontProfile.cpp'
ed1(PC, '''Cd.Id != FName(TEXT("bank_strike")) && Cd.Id != FName(TEXT("orb_relay")) && Cd.Id != FName(TEXT("recovery_roll"))''',
'''Cd.Id != FName(TEXT("bank_strike")) && Cd.Id != FName(TEXT("orb_relay")) && Cd.Id != FName(TEXT("recovery_roll")) && Cd.Id != FName(TEXT("fire_meteors"))''')
# card text
p = ROOT + 'Content/Data/Cards.csv'; b = open(p, 'rb').read()
i = b.index(b'fire_meteors,FIRE METEORS,'); j = b.index(b'\n', i); line = b[i:j]; cr = line.endswith(b'\r')
parts = line.rstrip(b'\r').split(b',', 11); parts[11] = b'LOCKED - now a Volcano arena event: meteors rain on the Volcanic Pit (more every two rounds) - watch for the red circles.'
open(p, 'wb').write(b[:i] + b','.join(parts) + (b'\r' if cr else b'') + b[j:])
print('b139a ok')
