# b176c (Evan: "unlock all skills and use them to replace the very old original skills used before we started buying"):
# the 15 card-book originals (storm .. recovery_roll) are retired everywhere (deck, hand, CPU pool, shop, chests, collection);
# every other skill is owned from the start. -ZoogiOldSkills = the originals come back; -ZoogiLockSkills = the b137d earn mode.
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/')
f='ZoogiFrontProfile.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:50].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('''const TArray<FZoogiCardDef>& UZoogiFrontProfileSubsystem::CardDefs()
{''','''// b176c: the retired card-book originals, and every other skill open from the start
static bool ZRetiredSkill(FName Id)
{
	static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSkills"));
	if (bOld) return false;
	static const TSet<FName> Old = { FName(TEXT("storm")), FName(TEXT("bank_strike")), FName(TEXT("guard")), FName(TEXT("orb_relay")), FName(TEXT("heavy_roll")),
		FName(TEXT("ember")), FName(TEXT("titan")), FName(TEXT("frost")), FName(TEXT("precision")), FName(TEXT("momentum_steal")), FName(TEXT("crystal_collapse")),
		FName(TEXT("orb_magnet")), FName(TEXT("double_impact")), FName(TEXT("anchor_guard")), FName(TEXT("recovery_roll")) };
	return Old.Contains(Id);
}
static bool ZAllSkillsOpen() { static const bool bLock = FParse::Param(FCommandLine::Get(), TEXT("ZoogiLockSkills")); return !bLock; }

const TArray<FZoogiCardDef>& UZoogiFrontProfileSubsystem::CardDefs()
{''')
ed('''		FString Log; ZoogiCards::LinkRequiredPowersFromData(R, Log);
		return R;''','''		FString Log; ZoogiCards::LinkRequiredPowersFromData(R, Log);
		const int32 Was = R.Num();
		R.RemoveAll([](const FZoogiCardDef& C) { return ZRetiredSkill(C.Id); }); // b176c
		UE_LOG(LogZoogi, Log, TEXT("PROFILE skills: %d cards, %d originals retired"), R.Num(), Was - R.Num());
		return R;''')
ed('''bool UZoogiFrontProfileSubsystem::IsCardOwned(FName Id) const
{''','''bool UZoogiFrontProfileSubsystem::IsCardOwned(FName Id) const
{
	if (ZRetiredSkill(Id)) return false; // b176c
	if (ZAllSkillsOpen()) return true;   // b176c (Evan): every skill unlocked''')
ed('''	if (ZSkillEarnOn())
	{	// b137d (Evan: "lock all skills so I can earn them"): a chest can unlock a new skill''','''	if (ZSkillEarnOn() && !ZAllSkillsOpen())
	{	// b137d (Evan: "lock all skills so I can earn them"): a chest can unlock a new skill''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
