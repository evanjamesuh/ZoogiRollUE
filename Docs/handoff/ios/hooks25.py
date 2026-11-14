import sys, os; sys.path.insert(0,'/tmp'); from ed import ed
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-combined/Source/ZoogiRoll/')
ed(R+'Private/ZoogiGameMode.cpp',[
('			if (bRoundEnd && Fun->StartReplay(this)) return;',
 '			if ((bRoundEnd || Fun->WantsFinisher()) && Fun->StartReplay(this, bRoundEnd)) return; // build 25: + ULTIMATE FINISH replay'),
])
ed(R+'Private/ZoogiPowers.cpp',[
('#include "ZoogiFrozenFun.h"\n', '#include "ZoogiFrozenFun.h"\n#include "ZoogiActionFun.h"\n'),
('''bool AZoogiGameMode::TryPower()
{
	AZoogiBall* B = GetActiveBall();
''', '''bool AZoogiGameMode::TryPower()
{
	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) if (Fun->TryUltimate(this)) return true; // build 25: full SUPER meter -> signature ultimate
	AZoogiBall* B = GetActiveBall();
'''),
('	if (WolfPackOnly()) return CanUsePower(); // Oct 28: the button is Wolf Pack only (classic / 2v2)',
 '	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) if (Fun->UltimateReady(this, B->PlayerIndex)) return true; // build 25: ultimate ready\n	if (WolfPackOnly()) return CanUsePower(); // Oct 28: the button is Wolf Pack only (classic / 2v2)'),
])
ed(R+'Private/ZoogiPlayerController.cpp',[
('		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) bFunTap = Fun->OnTap();',
 '		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) bFunTap = Fun->OnTap(Ptr);'),
])
