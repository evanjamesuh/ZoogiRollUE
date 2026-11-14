# b86c (Evan 13:10: "unlock all skills, fix stuck precision"): every power / skill card unlocked (-ZoogiPowerLocks = progression locks);
# PRECISION homing lets go after 2.2 s or once it is close (it could circle its target and keep the roll going forever)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiPowerUnlockSubsystem.cpp', [
 ('	bUnlockAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUnlockAll"));',
  '	bUnlockAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUnlockAll")) || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerLocks")); // b86 (Evan): every skill unlocked'),
])
ed(P + 'ZoogiCardFantasy.cpp', [
 ('''		const FVector2D To = Flat(Goal - SL);
		if (Sp > 60.f && To.Size() > 30.f)
		{''',
  '''		const FVector2D To = Flat(Goal - SL);
		// b86 (Evan: "fix stuck precision"): the homing lets go after 2.2 s of roll or inside 110 cm, so it can never circle its target
		static float HomingT = 0.f; static int32 HomingTurn = -1;
		if (G->TurnCounter != HomingTurn) { HomingTurn = G->TurnCounter; HomingT = 0.f; }
		HomingT += Dt;
		const bool bLetGo = HomingT > 2.2f || To.Size() < 110.f;
		if (bLetGo) { UE_LOG(LogZoogi, Log, TEXT("CARDS fantasy precision: homing released (%.1f s, %.0f cm)"), HomingT, To.Size()); Fan.HomingTarget.Reset(); Fan.bHomingPoint = false; }
		if (!bLetGo && Sp > 60.f && To.Size() > 30.f)
		{'''),
])
print('b86c ok')
