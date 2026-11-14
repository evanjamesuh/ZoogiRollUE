# b137d (Evan 09:34 / 09:36: "Lock all skills so I can earn them - all players begin with Bolt Balls, Ice Wall (and Energy Buff); Energy
# Buff can't be used in combos"):
#  - the profile keeps EarnedSkills; every player (existing saves too, once) starts with bolt_balls, ice_wall, energy_buff - every other
#    skill card is LOCKED (grey) until earned. -ZoogiAllSkills = the old ownership (everything the powers unlock)
#  - earning: each chest opened can unlock a new skill (Wooden 35%, Royal 70%, Epic always) - shown as a card reward + toast
#  - the match hand only uses skills you own (a deck holding locked ones is filtered; empty = your owned skills)
#  - COMBO: Energy Buff can't be ticked either (like Wood Shield)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
PH = 'Public/ZoogiFrontProfile.h'; PC = 'Private/ZoogiFrontProfile.cpp'
ed1(PH, '''	bool bItemsSeeded = false;    // b137: 3 of each weapon given once''', '''	bool bItemsSeeded = false;    // b137: 3 of each weapon given once
	TSet<FName> EarnedSkills;     // b137d: skill cards earned (Evan: "lock all skills so I can earn them")
	bool bSkillEarn = false;      // b137d: earn mode set up (starter skills given once)''')
ed1(PC, '''	J->SetBoolField(TEXT("items_seeded"), bItemsSeeded);''', '''	J->SetBoolField(TEXT("items_seeded"), bItemsSeeded);
	{ TArray<FName> Es = EarnedSkills.Array(); J->SetArrayField(TEXT("earned_skills"), ZFNames(Es)); }
	J->SetBoolField(TEXT("skill_earn"), bSkillEarn);''')
ed1(PC, '''		J->TryGetBoolField(TEXT("items_seeded"), N.bItemsSeeded);''', '''		J->TryGetBoolField(TEXT("items_seeded"), N.bItemsSeeded);
		if (J->TryGetArrayField(TEXT("earned_skills"), A) && A) for (const FName& X : ZFReadNames(*A)) N.EarnedSkills.Add(X);
		J->TryGetBoolField(TEXT("skill_earn"), N.bSkillEarn);''')
ed1(PC, '''bool UZoogiFrontProfileSubsystem::IsCardOwned(FName Id) const
{''', '''static bool ZSkillEarnOn() { static const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAllSkills")); return !bAll; }
bool UZoogiFrontProfileSubsystem::IsCardOwned(FName Id) const
{
	if (ZSkillEarnOn())
	{	// b137d (Evan): every skill is earned; the starters are given once
		if (!D.bSkillEarn)
		{
			FZoogiFrontProfileData& Dm = const_cast<FZoogiFrontProfileData&>(D);
			Dm.EarnedSkills = { FName(TEXT("bolt_balls")), FName(TEXT("ice_wall")), FName(TEXT("energy_buff")) };
			Dm.bSkillEarn = true;
			const_cast<UZoogiFrontProfileSubsystem*>(this)->Save();
			UE_LOG(LogZoogi, Log, TEXT("PROFILE skills: earn mode - starters Bolt Balls, Ice Wall, Energy Buff"));
		}
		return D.EarnedSkills.Contains(Id);
	}''')
ed1(PC, '''	Got.ValueSort([](int32 A, int32 B) { return A > B; });
	for (const TPair<FName, int32>& P : Got) { AddCopies(P.Key, P.Value); Out.Add({ FName(TEXT("card")), P.Key, P.Value }); }
	return Out;''', '''	Got.ValueSort([](int32 A, int32 B) { return A > B; });
	for (const TPair<FName, int32>& P : Got) { AddCopies(P.Key, P.Value); Out.Add({ FName(TEXT("card")), P.Key, P.Value }); }
	if (ZSkillEarnOn())
	{	// b137d (Evan: "lock all skills so I can earn them"): a chest can unlock a new skill
		IsCardOwned(NAME_None); // make sure the starters exist
		const float Chance = Type == FName(TEXT("epic")) ? 1.f : Type == FName(TEXT("royal")) ? 0.7f : 0.35f;
		TArray<FName> Locked;
		for (const FZoogiCardDef& Cd : CardDefs()) if (!D.EarnedSkills.Contains(Cd.Id) && Cd.Id != FName(TEXT("bank_strike")) && Cd.Id != FName(TEXT("orb_relay")) && Cd.Id != FName(TEXT("recovery_roll"))) Locked.Add(Cd.Id);
		if (Locked.Num() && R.FRand() < Chance)
		{
			const FName New = Locked[R.RandRange(0, Locked.Num() - 1)];
			D.EarnedSkills.Add(New);
			Out.Insert({ FName(TEXT("card")), New, 1 }, 0);
			UE_LOG(LogZoogi, Log, TEXT("PROFILE skills: %s chest unlocked %s (%d earned)"), *Type.ToString(), *New.ToString(), D.EarnedSkills.Num());
		}
	}
	return Out;''')
# match hand: owned skills only
CS = 'Private/ZoogiCardSubsystem.cpp'
ed1(CS, '#include "Misc/CommandLine.h"\n', '#include "Misc/CommandLine.h"\n#include "ZoogiFrontProfile.h"\n')
ed1(CS, '''	{	// b111 test: -ZoogiTestHand=a,b,c = the human's inventory hand (capture runs never read the saved profile)''',
'''	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiAllSkills")))
		if (const UZoogiFrontProfileSubsystem* Fp = UZoogiFrontProfileSubsystem::Get(this))
		{	// b137d (Evan: skills are earned): the hand holds only owned skills; nothing left = the owned ones
			const int32 Was = HumanDeck.Num();
			HumanDeck.RemoveAll([Fp](const FName& Id) { return !Fp->IsCardOwned(Id); });
			if (HumanDeck.Num() == 0) { for (const FName& Id : Fp->X.Num() < Tune.DeckSize) HumanDeck.Add(Id); bHumanLoadout = true; }
			LogS += FString::Printf(TEXT(" owned-skills hand %d -> %d;"), Was, HumanDeck.Num());
		}
	{	// b111 test: -ZoogiTestHand=a,b,c = the human's inventory hand (capture runs never read the saved profile)''')
# combo: energy buff too
P5 = 'Private/ZoogiP5Command.cpp'
ed1(P5, '''				if (PSw && PSw->Hand.IsValidIndex(W.Skills[R].Slot) && PSw->Hand[W.Skills[R].Slot].CardId == FName(TEXT("wood_shield")))
				{
					ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f);
					if (UZoogiFunSys* Fw = UZoogiFunSys::Get(G)) Fw->PublicCallout(TEXT("NOT IN A COMBO"), TEXT("WOOD SHIELD ENDS YOUR TURN - CAST IT ON ITS OWN"), FLinearColor(0.55f, 0.85f, 0.35f), 1.2f, 0.8f);
					return;
				}''', '''				const FName Cw = PSw && PSw->Hand.IsValidIndex(W.Skills[R].Slot) ? PSw->Hand[W.Skills[R].Slot].CardId : NAME_None;
				if (Cw == FName(TEXT("wood_shield")) || Cw == FName(TEXT("energy_buff"))) // b137d (Evan): Energy Buff can't be in a combo either
				{
					ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f);
					if (UZoogiFunSys* Fw = UZoogiFunSys::Get(G)) Fw->PublicCallout(TEXT("NOT IN A COMBO"), Cw == FName(TEXT("wood_shield")) ? TEXT("WOOD SHIELD ENDS YOUR TURN - CAST IT ON ITS OWN") : TEXT("ENERGY BUFF IS CAST ON ITS OWN"), FLinearColor(0.55f, 0.85f, 0.35f), 1.2f, 0.8f);
					return;
				}''')
print('b137d ok')
