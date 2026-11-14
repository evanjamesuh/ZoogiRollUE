# b100a (Evan 19:51): "Name balls ZOOGIS, name ZOOGIS BALLERS. This list now called Zoogis is the player's Awaken collection - a unique
# power for each: Hotstreak = Meteor Strike; the others get our current skills at their current strength as their Awaken powers (renamed).
# Then tune the skills down (too many combos off a skill) and tone the skill explosions down to smaller yellow-orange billow blasts;
# the big blast is kept for the big ones."
#  - Inventory: tab 0 BALLERS (the robots), tab 1 ZOOGIS (the balls); a Zoogi's detail = its Awaken power + description; equipping it
#    sets your Awaken power. CPUs use the Zoogi of their roster pick.
#  - SUPERPOWER (full meter) fires the Zoogi's Awaken power at full (current) strength: Hotstreak METEOR STRIKE (the ultimate),
#    Vex VOID PACK (3 wolves), Blue TIDAL CANNON, Frosti GLACIER WALL, Lumi SOLAR BOLTS, Terra IRONWOOD DOME, Zephyr CYCLONE PULSE,
#    Ember HEARTBREAKER (the void meteor). -ZoogiOldUltimates = the old signature ultimates.
#  - card skills cast at 0.6 strength (every skill push / wolf / blast), and their blasts use Mid_Explosion instead of Big_Explosion_v2.
#    -ZoogiSkillFullPower = 1.0.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiFront.h', [
 ('	ZOOGIROLL_API const FHero* HeroByName(const FString& Name);',
  '''	ZOOGIROLL_API const FHero* HeroByName(const FString& Name);
	/** b100 (Evan: "balls are now ZOOGIS - the player's Awaken collection, a unique power each"): Skill empty = METEOR STRIKE (the ultimate). */
	struct FAwaken { const TCHAR* Ball; const TCHAR* Name; const TCHAR* Skill; const TCHAR* Desc; };
	ZOOGIROLL_API const FAwaken* AwakenForBall(const FString& Ball);
	/** The Awaken power this Zoogi ball rolls with: the human's equipped Zoogi, a CPU's roster pick. */
	ZOOGIROLL_API const FAwaken* AwakenFor(const class AZoogiBall* B, const UObject* Ctx);'''),
])
ed(P + 'ZoogiFront.cpp', [
 ('''	const FHero* HeroByName(const FString& Name)
	{''', '''	const FAwaken* AwakenForBall(const FString& Ball)
	{
		static const FAwaken A[] = {
			{ TEXT("Hotstreak"), TEXT("METEOR STRIKE"), TEXT(""), TEXT("A huge fire blast where you roll, then a meteor shower rains across the whole arena.") },
			{ TEXT("Vex"), TEXT("VOID PACK"), TEXT("wolf_pack"), TEXT("Three hologram wolves at full power: time slows, pick 3 targets and they hunt them down.") },
			{ TEXT("Blue"), TEXT("TIDAL CANNON"), TEXT("water_cannon"), TEXT("A full-power water jet blasts the nearest rival or orb straight away from you.") },
			{ TEXT("Frosti"), TEXT("GLACIER WALL"), TEXT("ice_wall"), TEXT("A solid wall of ice rises across the arena - it cracks and bursts into ice blocks.") },
			{ TEXT("Lumi"), TEXT("SOLAR BOLTS"), TEXT("bolt_balls"), TEXT("Your ball crackles with sun lightning - every redirect shocks the 3 nearest away.") },
			{ TEXT("Terra"), TEXT("IRONWOOD DOME"), TEXT("wood_shield"), TEXT("An unbreakable wooden dome: you will not move and everything bounces off you.") },
			{ TEXT("Zephyr"), TEXT("CYCLONE PULSE"), TEXT("energy_shockwave"), TEXT("A cyclone sucks the orbs together, then blows them out at double wolf force.") },
			{ TEXT("Ember"), TEXT("HEARTBREAKER"), TEXT("meteor_rain"), TEXT("A void portal tears open and drops a boulder of purple lightning on your target.") },
		};
		for (const FAwaken& X : A) if (Ball.Equals(X.Ball, ESearchCase::IgnoreCase)) return &X;
		return nullptr;
	}
	const FAwaken* AwakenFor(const AZoogiBall* B, const UObject* Ctx)
	{
		if (!B) return nullptr;
		if (!B->bCPU)
		{
			const UZoogiFrontProfileSubsystem* Fp = UZoogiFrontProfileSubsystem::Get(Ctx);
			if (Fp && !Fp->Data().EquippedBall.IsNone()) if (const FAwaken* X = AwakenForBall(Fp->Data().EquippedBall.ToString())) return X;
		}
		for (const FHero& H : Heroes()) if (B->Def.Id == FName(H.RosterId)) return AwakenForBall(H.Ball);
		return nullptr;
	}
	const FHero* HeroByName(const FString& Name)
	{'''),
 ('		static const TCHAR* Tabs[] = { TEXT("ZOOGIS"), TEXT("BALLS"), TEXT("WEAPON"), TEXT("SKILLS") }; // b63: CARDS -> SKILLS (the hand loadout)',
  '		static const TCHAR* Tabs[] = { TEXT("BALLERS"), TEXT("ZOOGIS"), TEXT("WEAPON"), TEXT("SKILLS") }; // b63: CARDS -> SKILLS (the hand loadout); b100 (Evan): ZOOGIS -> BALLERS, BALLS -> ZOOGIS'),
 ('''				FItem I; I.Kind = 1; I.Name = FString(H.Ball) + TEXT(" BALL"); I.Sub = FString::Printf(TEXT("%s BALL SKIN"), ZRarityName(H.Rarity)); I.Ball = H.Ball; I.Rarity = H.Rarity;
				I.Desc = TEXT("Roll in this ball in every mode. Your Zoogi still stands up in its own Aurex colours.");''',
  '''				FItem I; I.Kind = 1; I.Name = FString(H.Ball).ToUpper(); I.Ball = H.Ball; I.Rarity = H.Rarity;
				// b100 (Evan): the balls are the ZOOGIS - your Awaken collection; each carries its own Awaken power
				const ZoogiFront::FAwaken* Aw = ZoogiFront::AwakenForBall(H.Ball);
				I.Sub = Aw ? FString::Printf(TEXT("AWAKEN: %s"), Aw->Name) : FString::Printf(TEXT("%s ZOOGI"), ZRarityName(H.Rarity));
				I.Desc = Aw ? FString::Printf(TEXT("%s ZOOGI. %s"), ZRarityName(H.Rarity), Aw->Desc) : FString(TEXT("Roll in this Zoogi in every mode."));'''),
 ('''				if (P->BuySkin(FName(*I.Ball), Why)) { Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s BALL UNLOCKED!"), *I.Ball.ToUpper())); }''',
  '''				if (P->BuySkin(FName(*I.Ball), Why)) { Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s UNLOCKED!"), *I.Ball.ToUpper())); }'''),
 ('''			Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s BALL EQUIPPED!"), *I.Ball.ToUpper())); Pop = EPop::None;''',
  '''			const ZoogiFront::FAwaken* AwE = ZoogiFront::AwakenForBall(I.Ball);
			Sfx(TEXT("UiConfirm")); Say(AwE ? FString::Printf(TEXT("%s EQUIPPED - AWAKEN: %s"), *I.Ball.ToUpper(), AwE->Name) : FString::Printf(TEXT("%s EQUIPPED!"), *I.Ball.ToUpper())); Pop = EPop::None;'''),
])
ed('Source/ZoogiRoll/Public/ZoogiSkills.h', [
 ('	ZOOGIROLL_API bool EffectBusy();', '''	ZOOGIROLL_API bool EffectBusy();
	/** b100 (Evan: "tune the skills down"): the strength of the skill playing now - 0.6 for a card skill, 1 for an Awaken power. */
	ZOOGIROLL_API float SkillPow();
	/** b100: the next pushes run at full (Awaken / ultimate) strength. */
	ZOOGIROLL_API void SetAwakenStrength();
	/** b100: a Zoogi's Awaken power = one of the skills at its full (pre-b100) strength, auto-aimed, no SP. */
	ZOOGIROLL_API void AwakenCast(AZoogiGameMode* G, AZoogiBall* B, FName Id);'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('		float Now(const AZoogiGameMode* G) { return G && G->GetWorld() ? G->GetWorld()->GetTimeSeconds() : 0.f; }',
  '''		float GSkillPow = 1.f; bool GAwakenNext = false; // b100: card skills 0.6, Awaken powers 1
		float CardPow() { static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillFullPower")); return bFull ? 1.f : 0.6f; }
		const TCHAR* SkillBlastFx() { return GSkillPow >= 0.99f ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion"); } // b100 (Evan): smaller billow blasts for skills
		float SkillBlastSc() { return GSkillPow >= 0.99f ? 1.f : 0.8f; }
		float Now(const AZoogiGameMode* G) { return G && G->GetWorld() ? G->GetWorld()->GetTimeSeconds() : 0.f; }'''),
 ('''			Sp->SetPhysicsLinearVelocity(Sp->GetPhysicsLinearVelocity() + DV);
		}''', '''			Sp->SetPhysicsLinearVelocity(Sp->GetPhysicsLinearVelocity() + DV * GSkillPow);
		}'''),
 ('''	void Execute(AZoogiGameMode* G, AZoogiBall* Caster, FName Id)
	{
		if (!G || !Caster) { Reset(); return; }''', '''	void Execute(AZoogiGameMode* G, AZoogiBall* Caster, FName Id)
	{
		if (!G || !Caster) { Reset(); return; }
		GSkillPow = GAwakenNext ? 1.f : CardPow(); GAwakenNext = false; // b100
		UE_LOG(LogZoogi, Log, TEXT("SKILL strength x%.2f (%s)"), GSkillPow, GSkillPow >= 0.99f ? TEXT("awaken power") : TEXT("card skill"));'''),
 ('					AZoogiFX::Stylized(Wm, TEXT("Big_Explosion_v2"), Hit, 0.6f);', '					AZoogiFX::Stylized(Wm, SkillBlastFx(), Hit, 0.6f * SkillBlastSc());'),
 ('						Body->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * 2.f * (420.f + 1380.f * K) + FVector(0.f, 0.f, 320.f * K));',
  '						Body->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + (Dir * 2.f * (420.f + 1380.f * K) + FVector(0.f, 0.f, 320.f * K)) * GSkillPow);'),
 ('					AZoogiFX::Stylized(Wb, TEXT("Big_Explosion_v2"), Pc, 0.7f);', '					AZoogiFX::Stylized(Wb, SkillBlastFx(), Pc, 0.7f * SkillBlastSc());'),
 ('''			Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (420.f + 1380.f * K) + FVector(0.f, 0.f, 160.f * K));
			Ob->LastHitBy = Owner;''', '''			Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + (Dir * (420.f + 1380.f * K) + FVector(0.f, 0.f, 160.f * K)) * GSkillPow);
			Ob->LastHitBy = Owner;'''),
 ('			Struck->SetPlanarVelocity(Struck->GetBallVelocity() + D * 900.f, 120.f);', '			Struck->SetPlanarVelocity(Struck->GetBallVelocity() + D * 900.f * GSkillPow, 120.f * GSkillPow);'),
 ('''		if (!G) return;
		UWorld* W = G->GetWorld();
		const float R = 430.f * FMath::Sqrt(FMath::Max(Mul, 0.1f)); // a wolf's ShockR''', '''		if (!G) return;
		UWorld* W = G->GetWorld();
		Mul *= GSkillPow; // b100: a card skill's blasts are 0.6
		const float R = 430.f * FMath::Sqrt(FMath::Max(Mul, 0.1f)); // a wolf's ShockR'''),
 ('		AZoogiFX::Stylized(W, TEXT("Big_Explosion_v2"), At, 0.45f * Mul);', '		AZoogiFX::Stylized(W, SkillBlastFx(), At, 0.45f * FMath::Max(Mul, 0.6f) * SkillBlastSc());'),
 ('''	bool AutoCast(AZoogiGameMode* G, FName Id)
	{''', '''	float SkillPow() { return GSkillPow; }
	void SetAwakenStrength() { GSkillPow = 1.f; }
	void AwakenCast(AZoogiGameMode* G, AZoogiBall* B, FName Id)
	{
		if (!G || !B) return;
		UE_LOG(LogZoogi, Log, TEXT("AWAKEN power P%d: %s at full strength"), B->PlayerIndex + 1, *Id.ToString());
		if (Id == FName(TEXT("wolf_pack")))
		{	// three wolves straight out (no level, no button): the pick, then the hunt
			GSkillPow = 1.f;
			for (int32 k = 0; k < 3; ++k) ArmWolf(G, B->PlayerIndex);
			if (!WolfGo(G)) UE_LOG(LogZoogi, Log, TEXT("AWAKEN void pack: wolves banked (SUPERPOWER releases them)"));
			return;
		}
		GAwakenNext = true;
		if (!AutoCast(G, Id)) GAwakenNext = false;
	}
	bool AutoCast(AZoogiGameMode* G, FName Id)
	{'''),
])
ed(P + 'ZoogiClone.cpp', [
 ('	AZoogiFX::Stylized(W, bFire ? TEXT("Fire_v2") : ((bDire || ChargeMul > 1.05f) ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion")), Ground, (bDire ? 0.75f : 0.42f) * FMath::Max(1.f, ChargeMul));',
  '	AZoogiFX::Stylized(W, bFire ? TEXT("Fire_v2") : ((bDire || ChargeMul > 1.05f) && BlastMul >= 0.99f ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion")), Ground, (bDire ? 0.75f : 0.42f) * FMath::Max(1.f, ChargeMul)); // b100: card wolves (0.6) keep the small blast'),
 ('#include "ZoogiClone.h"\n', '#include "ZoogiClone.h"\n#include "ZoogiSkills.h"\n'),
])
ed(P + 'ZoogiWolfPack.cpp', [
 ('		Cl->Rebounds = Level >= 4 ? 1 : 0;', '		Cl->Rebounds = Level >= 4 ? 1 : 0;\n		Cl->BlastMul *= ZoogiSkills::SkillPow(); Cl->HitMul *= ZoogiSkills::SkillPow(); // b100: a card Wolf Pack is 0.6, the VOID PACK Awaken power 1'),
 ('#include "ZoogiWolfPack.h"\n', '#include "ZoogiWolfPack.h"\n#include "ZoogiSkills.h"\n'),
])
ed(P + 'ZoogiSuperFun.cpp', [
 ('''	const EZoogiPower P = SignaturePower(B);
	Players[B->PlayerIndex].Meter = 0.f;
	bUltThisShot = true; bUltThisTurn = true; UltPower = P;''', '''	EZoogiPower P = SignaturePower(B);
	// b100 (Evan): the Zoogi (ball) carries the Awaken power - Hotstreak METEOR STRIKE, the others one of the skills at full strength
	static const bool bOldUlts = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldUltimates"));
	if (const ZoogiFront::FAwaken* Aw = bOldUlts ? nullptr : ZoogiFront::AwakenFor(B, this))
	{
		if (Aw->Skill && *Aw->Skill)
		{
			Players[B->PlayerIndex].Meter = 0.f;
			bUltThisShot = true; bUltThisTurn = true; UltPower = P;
			const FLinearColor Ac(1.f, 0.78f, 0.25f);
			Callout(Aw->Name, FString::Printf(TEXT("%s  AWAKEN"), *B->Def.Name.ToUpper()), Ac, 1.5f, 1.05f, B->PlayerIndex, FString::Printf(TEXT("Awaken - %s!"), Aw->Name));
			ZoogiShow::ImpactFrame(GetWorld(), B->BallLocation(), 9);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 0.85f);
			ZoogiSkills::AwakenCast(G, B, FName(Aw->Skill));
			return true;
		}
		P = EZoogiPower::Explosion; // METEOR STRIKE
	}
	Players[B->PlayerIndex].Meter = 0.f;
	bUltThisShot = true; bUltThisTurn = true; UltPower = P;'''),
 ('''	UE_LOG(LogZoogi, Log, TEXT("FUN ULTIMATE P%d %s: %s (target %s)"), B->PlayerIndex + 1, *B->Def.Name, UltName(P), Target ? *Target->GetName() : TEXT("auto"));''',
  '''	UE_LOG(LogZoogi, Log, TEXT("FUN ULTIMATE P%d %s: %s (target %s)"), B->PlayerIndex + 1, *B->Def.Name, UltName(P), Target ? *Target->GetName() : TEXT("auto"));
	ZoogiSkills::SetAwakenStrength(); // b100: the ultimate's meteors / blasts run at full strength'''),
 ('''		const EZoogiPower Sig = SignaturePower(B);
		// b63: the Zoogi says it in its speech bubble at the top
		const FString Nm = FString(UltName(Sig));''', '''		const EZoogiPower Sig = SignaturePower(B);
		// b63: the Zoogi says it in its speech bubble at the top
		const ZoogiFront::FAwaken* AwR = ZoogiFront::AwakenFor(B, this); // b100: the Zoogi's Awaken power
		const FString Nm = AwR ? FString(AwR->Name) : FString(UltName(Sig));'''),
 ('''		if (!B->bCPU) Callout(TEXT("ULTIMATE READY!"), FString::Printf(TEXT("%s  -  TAP SUPERPOWER WHILE ROLLING"), UltName(Sig)), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);
		else Callout(FString::Printf(TEXT("%s'S ULTIMATE IS READY"), *B->Def.Name.ToUpper()), UltName(Sig), UltColor(Sig), 1.3f, 0.7f, Player, LineReady);''',
  '''		if (!B->bCPU) Callout(AwR ? TEXT("AWAKEN READY!") : TEXT("ULTIMATE READY!"), FString::Printf(TEXT("%s  -  TAP SUPERPOWER WHILE ROLLING"), *Nm), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);
		else Callout(FString::Printf(TEXT("%s'S %s IS READY"), *B->Def.Name.ToUpper(), AwR ? TEXT("AWAKEN") : TEXT("ULTIMATE")), Nm, UltColor(Sig), 1.3f, 0.7f, Player, LineReady);'''),
 ('#include "ZoogiSkills.h"\n', '#include "ZoogiSkills.h"\n#include "ZoogiFront.h"\n'),
])
print('b100a ok')
