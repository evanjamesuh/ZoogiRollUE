# b139d (Evan 10:46: "AI should only be able to use unlocked skills"): the CPUs' random hands come only from the skills the player has
# unlocked (the same earned set). -ZoogiAllSkills = every skill again.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
CS = 'Private/ZoogiCardSubsystem.cpp'
ed1(CS, '''		for (const FName& Id : ZoogiSkills::AllSkillIds()) if (Backend->FindDef(Id)) Pool.Add(Id);
		FString ForcedC; FParse::Value(FCommandLine::Get(), TEXT("ZoogiForceCard="), ForcedC);
		if (!bOld && TestCase.IsEmpty() && ForcedC.IsEmpty() && Pool.Num() >= 2)''', '''		for (const FName& Id : ZoogiSkills::AllSkillIds()) if (Backend->FindDef(Id)) Pool.Add(Id);
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiAllSkills")))
			if (const UZoogiFrontProfileSubsystem* Fpc = UZoogiFrontProfileSubsystem::Get(this))
			{	// b139d (Evan): the AI only uses unlocked skills
				Pool.RemoveAll([Fpc](const FName& Id) { return !Fpc->IsCardOwned(Id); });
				LogS += FString::Printf(TEXT(" cpu pool = %d unlocked skills;"), Pool.Num());
			}
		FString ForcedC; FParse::Value(FCommandLine::Get(), TEXT("ZoogiForceCard="), ForcedC);
		if (!bOld && TestCase.IsEmpty() && ForcedC.IsEmpty() && Pool.Num() >= 1)''')
print('b139d ok')
