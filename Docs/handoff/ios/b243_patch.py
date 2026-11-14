# Nov 11 (Claude): B243 - Evan 08:22 "Remove the soldier still appearing on the classic, raid, battle royale arenas"
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(rel, pairs, inc=None):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    if inc and '#include "%s"' % inc not in t:
        k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b243\n' % inc + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
# 1 no Voyager soldier model outside a SHOOTER match (any caller: royale hero, wall cine, crew, story)
patch('Private/ZoogiVoyager.cpp', [('''	USkeletalMesh* Character(const FString& ZoogiName, int32 Fallback, FString* OutName)
	{
		if (!On()) return nullptr;''', '''	USkeletalMesh* Character(const FString& ZoogiName, int32 Fallback, FString* OutName)
	{
		if (!On()) return nullptr;
		static const bool bAny = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCineVoyager")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidCrew"));
		if (ZoogiShooter::Kind() == 0 && !bAny) return nullptr; // b243 (Evan): no soldiers in classic / raid / battle royale''')], inc='ZoogiShooter.h')
# 2 Battle Royale (not a shooter mode): no two-legged robot form - everyone stays a ball (-ZoogiRoyaleRobot = the old transform)
patch('Private/ZoogiRoyale.cpp', [('''	if (ZoogiShooter::On(this) && !ZoogiShooter::AllowTransform(this, Idx, R.bRobot)) return false; // b236: crystals buy the ball dash''',
 '''	if (ZoogiShooter::On(this) && !ZoogiShooter::AllowTransform(this, Idx, R.bRobot)) return false; // b236: crystals buy the ball dash
	static const bool bRobotOk = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleRobot"));
	if (!ZoogiShooter::On(this) && !R.bRobot && !bRobotOk) return false; // b243 (Evan): two-legged forms only in the shooter modes''')])
# 3 every non-shooter Mode Select card clears a leftover shooter mode
patch('Private/ZoogiModeSelect.cpp', [('	if (Id == ZoogiModeSel::ECardId::Royale) ZoogiShooter::SetKind(0); // b234',
 '	if (!ZoogiModeSel::IsVoyMode(Id)) ZoogiShooter::SetKind(0); // b234, b243: any non-shooter card clears a leftover shooter mode')])
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B242"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B243"), TEXT("NO SOLDIERS OUTSIDE VOYAGER"), TEXT("Removed the two-legged soldier / robot from Classic, Raid and Battle Royale: in Battle Royale everyone stays a ball, and the Voyager soldier models only appear in the VOYAGER modes.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
