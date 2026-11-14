# b85 (Evan 11:53: "remove the gameplay pause during animation"): anime cut-ins no longer pause the game, crash / skill-summon clips no longer
# freeze or slow the world. -ZoogiAnimeFreeze / -ZoogiAnimeSlow bring them back
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('	if (E.bFreeze) Freeze();', '	static const bool bAnimeFreeze = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeFreeze")); // b85 (Evan): gameplay keeps going under the anime\n	if (E.bFreeze && bAnimeFreeze) Freeze();'),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('			else if (UZoogiAnimeSubsystem* AnC = W->GetSubsystem<UZoogiAnimeSubsystem>(); AnC && AnC->CrashOn()) Want = AnC->PowerIntroOn() ? 0.0001f : 0.03f;',
  '			else if (UZoogiAnimeSubsystem* AnC = W->GetSubsystem<UZoogiAnimeSubsystem>(); AnC && AnC->CrashOn() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeSlow"))) Want = AnC->PowerIntroOn() ? 0.0001f : 0.03f; // b85 (Evan: "remove the gameplay pause during animation"): only with -ZoogiAnimeSlow;'),
])
print('b85 ok')
