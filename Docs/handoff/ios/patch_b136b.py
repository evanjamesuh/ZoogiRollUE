# b136b (Evan 08:35: "Golden Apple strength or radius doesn't work in skill lab - ensure all skills can be edited in skill lab and can
# be played in actual gameplay"): the Skill Lab tunings (Saved/SkillLab.ini, used in matches too) now reach every skill:
#  - golden_apple: strength / radius / fx size / colour captured at the cast, used when the apple lands
#  - delayed effects (Later: water cannon, magic rays, fire meteors, shockwave...) keep their own skill's strength + tuning
#  - apple_bombs mines keep the caster's strength + tuning until they go off
#  - wolf_pack: the wolves' blasts (strength + radius); bolt_balls: the zap push (strength); ice_wall: radius = wall height
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
LH = 'Public/ZoogiSkillLab.h'; LC = 'Private/ZoogiSkillLab.cpp'; S = 'Private/ZoogiSkills.cpp'
ed1(LH, '''	ZOOGIROLL_API void SetCurrent(FName Skill);''', '''	ZOOGIROLL_API void SetCurrent(FName Skill);
	/** b136: the skill the tunings below read */
	ZOOGIROLL_API FName Current();
	/** b136: one skill's tunings, whatever plays now (wolves / bolts / delayed effects) */
	ZOOGIROLL_API float StrengthOf(FName Skill);
	ZOOGIROLL_API float RadiusOf(FName Skill);''')
ed1(LC, '''	void SetCurrent(FName Skill) { GCur = Skill; }''', '''	void SetCurrent(FName Skill) { GCur = Skill; }
	FName Current() { return GCur; }
	float StrengthOf(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->Strength, 0.f, 5.f) : 1.f; }
	float RadiusOf(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->Radius, 0.2f, 4.f) : 1.f; }''')
# Later(): the delayed part of a skill keeps its own strength + lab skill
ed1(S, '''		void Later(AZoogiGameMode* G, float Delay, TFunction<void()> Fn) { FEvent E; E.At = Now(G) + Delay; E.Fn = MoveTemp(Fn); Events.Add(MoveTemp(E)); }''',
'''		/** b136: GSkillPow + the lab's current skill for a block of code, restored after */
		struct FTuneScope
		{
			float SavedPow; FName SavedSkill;
			FTuneScope(float Pow, FName Skill) : SavedPow(GSkillPow), SavedSkill(ZoogiSkillLab::Current()) { GSkillPow = Pow; if (!Skill.IsNone()) ZoogiSkillLab::SetCurrent(Skill); }
			~FTuneScope() { GSkillPow = SavedPow; ZoogiSkillLab::SetCurrent(SavedSkill); }
		};
		void Later(AZoogiGameMode* G, float Delay, TFunction<void()> Fn)
		{	// b136 (Evan: the lab tunings must reach every skill): the delayed part runs with the strength + lab skill of its cast
			FEvent E; E.At = Now(G) + Delay;
			const float Pw = GSkillPow; const FName Sk = ZoogiSkillLab::Current();
			E.Fn = [Pw, Sk, Inner = MoveTemp(Fn)]() { FTuneScope T(Pw, Sk); Inner(); };
			Events.Add(MoveTemp(E));
		}''')
# mines
ed1(S, '''		struct FMine { FVector P; int32 Owner = -1; TWeakObjectPtr<AZoogiFabFX> Fx; };''', '''		struct FMine { FVector P; int32 Owner = -1; TWeakObjectPtr<AZoogiFabFX> Fx; float Pow = 1.f; }; // b136: Pow = the cast's strength''')
ed1(S, '''				FMine M; M.P = At(Sp); M.Owner = Pi;''', '''				FMine M; M.P = At(Sp); M.Owner = Pi; M.Pow = GSkillPow;''')
ed1(S, '''			Blast(G, M.P, 280.f, 650.f, 260.f, nullptr, M.Owner);''', '''			{ FTuneScope Tn(M.Pow, FName(TEXT("apple_bombs"))); Blast(G, M.P, 280.f, 650.f, 260.f, nullptr, M.Owner); } // b136: the lab tuning''')
# ice wall height
ed1(S, '''			AZoogiIceBarrier::Spawn(W, At(P.A), At(P.B), 220.f, Pi, bOldLife ? 1 : 0);''', '''			AZoogiIceBarrier::Spawn(W, At(P.A), At(P.B), 220.f * ZoogiSkillLab::Radius(), Pi, bOldLife ? 1 : 0); // b136: lab RADIUS = wall height''')
# golden apple: capture the tunings at the cast
ed1(S, '''					const double T0 = FPlatformTime::Seconds();
					const int32 PiA = Pi; const float FZA = FZ;
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, WApple, From, To, FormT, FlyT, T0, PiA, FZA](float) -> bool''',
'''					const double T0 = FPlatformTime::Seconds();
					const int32 PiA = Pi; const float FZA = FZ;
					// b136 (Evan: the lab's strength / radius must work): the tunings of this cast, used when the apple lands
					const float StrA = GSkillPow / FMath::Max(0.1f, CardPow()), RadA = ZoogiSkillLab::Radius(), FxA = ZoogiSkillLab::FxSize();
					FLinearColor GoldA(1.f, 0.8f, 0.25f); ZoogiSkillLab::Tint(GoldA);
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, WApple, From, To, FormT, FlyT, T0, PiA, FZA, StrA, RadA, FxA, GoldA](float) -> bool''')
ed1(S, '''						const float R = 650.f;
						int32 Pushed = 0;''', '''						const float R = 650.f * RadA;
						int32 Pushed = 0;''')
ed1(S, '''							Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (520.f + 1700.f * Kk) + FVector(0.f, 0.f, 200.f * Kk));''',
'''							Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + (Dir * (520.f + 1700.f * Kk) + FVector(0.f, 0.f, 200.f * Kk)) * StrA);''')
ed1(S, '''						const FLinearColor Gold(1.f, 0.8f, 0.25f);
						AZoogiFabFX::Play(Wg, FX(TEXT("NS_Apple_of_Eden")), Ground, Sc(FName(TEXT("golden_apple")), 0.3f) * 1.3f, 2.f);''', '''						const FLinearColor Gold = GoldA;
						AZoogiFabFX::Play(Wg, FX(TEXT("NS_Apple_of_Eden")), Ground, Sc(FName(TEXT("golden_apple")), 0.3f) * 1.3f * FxA, 2.f);''')
ed1(S, '''						AZoogiFX::Stylized(Wg, TEXT("Mid_Explosion"), Ground, 0.55f);
						ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 0.9f, 1.05f);
						UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: the apple lands - blast r %.0f pushed %d orbs"), R, Pushed);''',
'''						AZoogiFX::Stylized(Wg, TEXT("Mid_Explosion"), Ground, 0.55f * FxA);
						ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 0.9f, 1.05f);
						UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: the apple lands - blast r %.0f x%.2f strength, pushed %d orbs"), R, StrA, Pushed);''')
# wolves
C = 'Private/ZoogiClone.cpp'
ed1(C, '#include "Misc/CommandLine.h"\n', '#include "Misc/CommandLine.h"\n#include "ZoogiSkillLab.h"\n')
ed1(C, '''	const float R = ShockR * (ZoogiWolf::On() ? BlastMul * ChargeMul : 1.f);
	if (AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr)''', '''	const FName WolfSk(TEXT("wolf_pack"));
	const float LabStr = ZoogiSkillLab::StrengthOf(WolfSk); // b136 (Evan): the Skill Lab tunes the wolves too
	const float R = ShockR * (ZoogiWolf::On() ? BlastMul * ChargeMul : 1.f) * ZoogiSkillLab::RadiusOf(WolfSk);
	if (AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr)''')
ed1(C, '''			const float Push = (420.f + 1380.f * K) * (ZoogiWolf::On() ? FMath::Sqrt(BlastMul * ChargeMul) : 1.f);''', '''			const float Push = (420.f + 1380.f * K) * (ZoogiWolf::On() ? FMath::Sqrt(BlastMul * ChargeMul) : 1.f) * LabStr;''')
# bolt zap
RD = 'Private/ZoogiRedirect.cpp'
ed1(RD, '#include "ZoogiCosmosBoss.h"\n', '#include "ZoogiCosmosBoss.h"\n#include "ZoogiSkillLab.h"\n')
ed1(RD, '''				Push(Ts[i], (Loc(Ts[i]) - P).GetSafeNormal2D(), 900.f);''', '''				Push(Ts[i], (Loc(Ts[i]) - P).GetSafeNormal2D(), 900.f * ZoogiSkillLab::StrengthOf(FName(TEXT("bolt_balls")))); // b136: lab strength''')
print('b136b ok')
