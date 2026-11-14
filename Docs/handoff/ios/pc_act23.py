import sys; sys.path.insert(0,'/tmp'); from ed import ed
P='/sessions/'  # placeholder
import os
p=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-combined/Source/ZoogiRoll/Private/ZoogiPlayerController.cpp')
ed(p,[
('static const TCHAR* ShotNames[] = { TEXT("side"), TEXT("chase"), TEXT("impact"), TEXT("follow_hit"), TEXT("wide_side"), TEXT("orbit") };',
 '// Oct 29 night (Evan, build 23): every action shot stays out at the after-launch overhead distance (Follow profile ~2400 cm,\n\t\t\t// high pitch) and only the ANGLE changes - no low chase / impact close-ups.\n\t\t\tstatic const TCHAR* ShotNames[] = { TEXT("overhead"), TEXT("high_side"), TEXT("impact_34"), TEXT("follow_hit"), TEXT("top_down"), TEXT("orbit") };'),
('ActShot = S; ActFocus = F; ActT0 = Now; ActLen = Len; bActCut = true; ActYawInit = false; ++ActCuts;',
 'ActShot = S; ActFocus = F; ActT0 = Now; ActLen = Len; bActCut = ActCuts > 0; ActYawInit = false; ++ActCuts;'),
('if (ActShot < 0) { ActSeenImpact = Ser; Pick(0, Shooter, 1.5f); }',
 'if (ActShot < 0) { ActSeenImpact = Ser; ActCuts = 0; ActYawInit = false; Pick(0, Shooter, 1.6f); }'),
('''			float CYaw = Y0, CPitch = 20.f, CDist = 900.f, CFov = 70.f;
			FVector CPiv = FP, CLook = FP;
			switch (ActShot)
			{
			case 0: case 3:
				CDist = ActShot == 0 ? 780.f : 720.f; CPitch = ActShot == 0 ? 15.f : 17.f; CFov = 68.f;
				if (!ActYawInit) ActSideSign = PickSide(Y0, 90.f, FP, CDist);
				CYaw = Y0 + 90.f * ActSideSign; CPiv = FP + D * 90.f; CLook = FP + D * 170.f;
				break;
			case 1:
				CDist = 560.f; CPitch = 11.f; CFov = 78.f; CYaw = Y0; CLook = FP + D * 420.f;
				break;
			case 2:
				if (!ActYawInit) ActSideSign = PickSide(Y0, 75.f, FP, 640.f);
				CDist = 640.f * (1.f - 0.12f * FMath::Clamp((Now - ActT0) / ActLen, 0.f, 1.f)); CPitch = 19.f; CFov = 64.f; CYaw = Y0 + 75.f * ActSideSign;
				break;
			case 4:
			{
				CPiv = FMath::Lerp(FP, C0, 0.45f); CLook = CPiv;
				const FVector Rad = FVector(FP.X, FP.Y, 0.f).GetSafeNormal();
				const float Ry = Rad.IsNearlyZero() ? Y0 : FMath::RadiansToDegrees(FMath::Atan2(Rad.Y, Rad.X));
				if (!ActYawInit) { ActSideSign = PickSide(Ry, 90.f, CPiv, 2350.f); ActWideYaw = Ry + 90.f * ActSideSign; }
				CYaw = ActWideYaw; CDist = 2350.f; CPitch = 21.f; CFov = 70.f;
				break;
			}
			default:
				CPiv = C0; CLook = C0; CYaw = ActWideYaw + 14.f * (Now - ActT0); CDist = 2700.f; CPitch = 30.f; CFov = 72.f;
				break;
			}''',
'''			const float FD = Profiles[PFollow].Dist, FPit = Profiles[PFollow].Pitch; // the after-launch overhead (2400 cm, 55 deg)
			const float FH = FD * FMath::Cos(FMath::DegreesToRadians(FPit)); // ground distance for the side pick
			float CYaw = Y0, CPitch = FPit, CDist = FD, CFov = 84.f;
			FVector CPiv = FP, CLook = FP;
			const float U = FMath::Clamp((Now - ActT0) / FMath::Max(ActLen, 0.1f), 0.f, 1.f);
			switch (ActShot)
			{
			case 0: // the overhead follow Evan likes: behind the roll, high
				if (!ActYawInit) ActWideYaw = ActCuts <= 1 ? Yaw : Y0;
				ActWideYaw = FRotator::NormalizeAxis(ActWideYaw + FRotator::NormalizeAxis(Y0 - ActWideYaw) * (1.f - FMath::Exp(-1.5f * Dt)));
				CYaw = ActWideYaw; CPitch = FPit; CDist = FD; CFov = 86.f; CPiv = FP + D * 120.f; CLook = CPiv;
				break;
			case 1: case 3: // high side angle at the same distance (on the shooter / on whatever got hit hardest)
				if (!ActYawInit) ActSideSign = PickSide(Y0, 90.f, FP, FH);
				CYaw = Y0 + (90.f - 12.f * U) * ActSideSign; CPitch = FPit - 7.f; CDist = FD * 0.96f; CFov = 82.f;
				CPiv = FP + D * 160.f; CLook = CPiv;
				break;
			case 2: // first impact: high three-quarter angle on the hit, same distance
				if (!ActYawInit) ActSideSign = PickSide(Y0, 50.f, FP, FH);
				CYaw = Y0 + (50.f + 10.f * U) * ActSideSign; CPitch = FPit - 3.f; CDist = FD * (0.97f - 0.04f * U); CFov = 80.f;
				break;
			case 4: // near top-down, slowly turning, between the action and the centre
			{
				CPiv = FMath::Lerp(FP, C0, 0.35f); CLook = CPiv;
				if (!ActYawInit) ActWideYaw = Yaw;
				CYaw = ActWideYaw + 10.f * (Now - ActT0); CPitch = 72.f; CDist = FD * 1.08f; CFov = 84.f;
				break;
			}
			default: // wide orbit of the whole court
				if (!ActYawInit) ActWideYaw = Yaw;
				CPiv = C0; CLook = C0; CYaw = ActWideYaw + 12.f * (Now - ActT0); CDist = FMath::Max(FD * 1.15f, 2700.f); CPitch = 50.f; CFov = 80.f;
				break;
			}'''),
])
