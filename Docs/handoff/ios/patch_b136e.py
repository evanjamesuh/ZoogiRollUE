# b136e (Evan 08:56: "Change the skill meteor rain to IMPULSE - a single wolf pack blast at the player position, non-targeting, no
# slowdown, the only skill that gets a mandatory anime"): the meteor_rain card is now IMPULSE - no pick, one wolf-strength blast
# from where your Zoogi is (also mid-roll), and its anime plays on every cast (top card: Anime_Awaken_Impulse_01, Wolf Pack clip until
# Grok's own clip lands). Ember's Awaken HEARTBREAKER keeps the void-portal meteor. -ZoogiOldMeteorRain = the old skill.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Source/ZoogiRoll/Private/ZoogiSkills.cpp'
ed1(S, '''		const FDef* Find(FName Id) { for (const FDef& D : Defs) if (Id == FName(D.Id)) return &D; return nullptr; }''',
'''		bool ImpulseNow(); // b136: meteor_rain is IMPULSE (no pick) unless an Awaken power casts it
		const FDef ImpulseDef = { TEXT("meteor_rain"), EAim::Self, 0.f, 1, TEXT("") };
		const FDef* Find(FName Id)
		{
			for (const FDef& D : Defs) if (Id == FName(D.Id)) return (Id == FName(TEXT("meteor_rain")) && ImpulseNow()) ? &ImpulseDef : &D;
			return nullptr;
		}''')
ed1(S, '''		float CardPow() { static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillFullPower")); return bFull ? 1.f : 0.6f; }''',
'''		float CardPow() { static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillFullPower")); return bFull ? 1.f : 0.6f; }
		bool ImpulseNow() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeteorRain")); return !bOld && !GAwakenNext; }''')
ed1(S, '''		GSkillPow = (GAwakenNext ? 1.f : CardPow()) * ZoogiSkillLab::Strength(); GAwakenNext = false; // b100; b107 x the lab tuning''',
'''		const bool bImpulse = Id == FName(TEXT("meteor_rain")) && ImpulseNow(); // b136: read before GAwakenNext resets
		GSkillPow = (GAwakenNext ? 1.f : CardPow()) * ZoogiSkillLab::Strength(); GAwakenNext = false; // b100; b107 x the lab tuning''')
ed1(S, '''		else if (Id == FName(TEXT("meteor_rain")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeteorRain")))
		{''', '''		else if (bImpulse)
		{	// b136 (Evan): IMPULSE - one wolf-pack blast from where the Zoogi is right now, no aiming, no slow-down; its anime always plays
			const FVector Bp = Caster->BallLocation();
			WolfBlast(G, FVector(Bp.X, Bp.Y, FZ + 4.f), Pi, 1.f / FMath::Max(0.1f, CardPow())); // one wolf at lab strength 1 (WolfBlast applies GSkillPow)
			AZoogiFX::Spawn(W, Bp, FLinearColor(0.7f, 0.85f, 1.f), 260.f, 0.35f, false, 16.f);
			AZoogiFX::Shockwave(W, FVector(Bp.X, Bp.Y, FZ + 4.f), EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, nullptr, false);
			ZoogiHaptics::Kick(0.6f, W, TEXT("impulse"));
			if (AnS && !AnS->PlayPowerIntro(TEXT("Impulse"), Pi, FLinearColor(0.7f, 0.85f, 1.f))) AnS->PlayPowerIntro(TEXT("WolfPack"), Pi, FLinearColor(0.7f, 0.85f, 1.f)); // mandatory anime (placeholder: the Wolf Pack clip)
			UE_LOG(LogZoogi, Log, TEXT("SKILL impulse: P%d wolf blast at (%.0f, %.0f) + anime"), Pi + 1, Bp.X, Bp.Y);
		}
		else if (Id == FName(TEXT("meteor_rain")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeteorRain")))
		{''')
# the card
C = 'Content/Data/Cards.csv'
ed1(C, '''meteor_rain,METEOR RAIN,3,Impact,ActingZoogi,LeadActionEnd,450,7,0,true,Fire,Place a circle inside your arc range: a meteor shower rains there and every meteor knocks orbs and rivals outward.''',
'''meteor_rain,IMPULSE,3,Impact,ActingZoogi,LeadActionEnd,450,7,0,true,Fire,No aiming: a wolf-pack blast bursts out from where your Zoogi is - cast it before or during your roll. Its anime plays every time.''')
print('b136e ok')
