# b125a (Evan 22:04-22:08: "tap to target still doesn't work", "it fires when I release - fuck that, go back to when it worked",
# "the redirect - the zoogi barely moves in that direction", "how do wooden shield and bolt ball end up overlaying" (CPU))
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
D = 'Private/ZoogiRedirect.cpp'
# 1. REDIRECT: the target is picked the moment the finger touches it (as before b120); a drag elsewhere still turns the camera
ed1(D, '''		// b120: picks on release (a tap) - a drag turns the camera round the Zoogi instead (ZoogiPauseOrbit)
		if (!bPick || bCpuPick || !bRelease || ZoogiPauseOrbit::WasDrag() || !G || !PC) return;''',
'''		// b125 (Evan: "it fires when I release - go back to when it worked right"): the target is picked on the PRESS again (the finger's
		// real spot - no lifted-finger position); a press away from every target can still drag the camera round. -ZoogiPickOnRelease = b120
		static const bool bOnRelease = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPickOnRelease"));
		if (!bPick || bCpuPick || !G || !PC) return;
		if (bOnRelease ? (!bRelease || ZoogiPauseOrbit::WasDrag()) : !bPress) return;''')
# 2. REDIRECT launch: stronger and it stays that strong (homing keeps the speed, a slow roll gets the full kick)
ed1(D, '''			const float Spd = FMath::Clamp(FMath::Max(Cur, 1500.f) * 1.25f, 1500.f, 3000.f); // b70 (Evan): "powerful and swift"''',
'''			// b125 (Evan: "the zoogi barely moves in that direction"): a harder kick - 2200..3400 cm/s (was 1500..3000; most redirects came
			// out at the 1875 floor). -ZoogiRedirectSoft = the b70 speeds
			static const bool bSoft = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectSoft"));
			const float Spd = bSoft ? FMath::Clamp(FMath::Max(Cur, 1500.f) * 1.25f, 1500.f, 3000.f) : FMath::Clamp(FMath::Max(Cur, 1700.f) * 1.3f, 2200.f, 3400.f); // b70 (Evan): "powerful and swift"
			SpdCheckAt = Now() + 0.25; SpdCheckBall = B; SpdFired = Spd;''')
ed1(D, '''		double HomingUntil = 0.0, CpuNextAt = 0.0, LastFireAt = -10.0;''', '''		double HomingUntil = 0.0, CpuNextAt = 0.0, LastFireAt = -10.0;
		double SpdCheckAt = 0.0; TWeakObjectPtr<AZoogiBall> SpdCheckBall; float SpdFired = 0.f; // b125: speed 0.25 s after a redirect (log)''')
ed1(D, '''		// the ball homes in on its target for a moment so the smash lands where it was aimed''', '''		if (SpdCheckAt > 0.0 && Now() >= SpdCheckAt)
		{
			SpdCheckAt = 0.0;
			if (AZoogiBall* Sb = SpdCheckBall.Get()) UE_LOG(LogZoogi, Log, TEXT("REDIRECT check P%d: fired %.0f cm/s, 0.25 s later %.0f cm/s (time dilation %.3f)"), Sb->PlayerIndex + 1, SpdFired, Sb->GetBallVelocity().Size2D(), G->GetWorldSettings() ? G->GetWorldSettings()->TimeDilation : -1.f);
		}
		// the ball homes in on its target for a moment so the smash lands where it was aimed''')
# 3. the superpower bullet-time pick: on the press too, bigger tap window
P = 'Private/ZoogiPlayerController.cpp'
ed1(P, '''	if (bPtrRelease && !ZoogiPauseOrbit::WasDrag() && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick()))) // b120: the bullet-time pick on release (a drag turns the camera)''',
'''	static const bool bUltOnRelease = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPickOnRelease"));
	if ((bUltOnRelease ? (bPtrRelease && !ZoogiPauseOrbit::WasDrag()) : bPtrPress) && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick()))) // b125: the bullet-time pick on the press again (b120: on release)''')
ed1('Private/ZoogiActionFun.cpp', '''	if (PickA && BestD < 0.12f * FMath::Max(1, FMath::Min(Vx, Vy)) + 40.f) { bUltAim = false; FireUltimate(G, B, UltPower, PickA); }''',
'''	if (PickA && BestD < 0.20f * FMath::Max(1, FMath::Min(Vx, Vy)) + 80.f) { bUltAim = false; FireUltimate(G, B, UltPower, PickA); } // b125: bigger window (was 0.12 + 40)
	else UE_LOG(LogZoogi, Log, TEXT("ULT tap missed (nearest target %.0f px away)"), BestD);''')
# 4. a button press that lost its touch-end never blocks the field for more than 1.5 s (was: the menu forever, the pad 6 s)
ed1('Private/ZoogiClassicPad.cpp', '''	return (GPadHeld && Dt < 6.0) || Dt < 0.15;''', '''	return (GPadHeld && Dt < 1.5) || Dt < 0.15; // b125: 1.5 s cap (was 6 s)''')
ed1('Private/ZoogiP5Command.cpp', '''	bool InputBlocked() { return GP5Held || FPlatformTime::Seconds() - GP5TookAt < 0.3; }''',
'''	bool InputBlocked() { return (GP5Held && FPlatformTime::Seconds() - GP5TookAt < 1.5) || FPlatformTime::Seconds() - GP5TookAt < 0.3; } // b125: a lost touch-end never blocks taps for good''')
# 5. CPU plays by the player's rules: no skill before launch - every skill is cast while rolling, one at a time (like COMBO), and only one
#    aura on the Zoogi per turn (wood shield / bolt balls / energy buff never stack)
S = 'Private/ZoogiCPUSmart.cpp'
ed1(S, '''	bool CastMidRoll(FName Id)
	{
		static const FName M[] = { TEXT("meteor_rain"), TEXT("fire_meteors"), TEXT("water_cannon"), TEXT("magic_rays"), TEXT("sword_slash"), TEXT("energy_shockwave"), TEXT("apple_bombs"), TEXT("golden_apple"), TEXT("ice_wall") };
		for (const FName& N : M) if (N == Id) return true;
		return false;
	}''', '''	bool CastMidRoll(FName) { return true; } // b125 (Evan: the CPU did things a player can't): every skill after launch, like the player's COMBO
	bool IsAura(FName Id) { return Id == FName(TEXT("wood_shield")) || Id == FName(TEXT("bolt_balls")) || Id == FName(TEXT("energy_buff")); }''')
ed1(S, '''		if (K == TEXT("ice_wall")) return S.MyEdge < 900.f; // the Zoogi is heading for its edge: wall it off''', '''		if (K == TEXT("ice_wall")) return S.MyEdge < 900.f; // the Zoogi is heading for its edge: wall it off
		if (K == TEXT("energy_buff") || K == TEXT("wolf_pack")) return true; // straight after launch
		if (K == TEXT("wood_shield")) return S.NearestRival < 1000.f || G->PhaseTime > 1.2f;
		if (K == TEXT("bolt_balls")) return S.Near > 0 || G->PhaseTime > 1.0f;''')
ed1('Public/ZoogiCPUSmart.h', '''	ZOOGIROLL_API bool CastMidRoll(FName Id);''', '''	ZOOGIROLL_API bool CastMidRoll(FName Id);
	/** b125: shield / bolt / buff - only one of these on the Zoogi per turn */
	ZOOGIROLL_API bool IsAura(FName Id);''')
C = 'Private/ZoogiCardSubsystem.cpp'
ed1(C, '''			if (X.V < 22.f || X.C > E || Took.Contains(X.Id) || Smart.Num() >= 3) continue;''', '''			if (X.V < 22.f || X.C > E || Took.Contains(X.Id) || Smart.Num() >= 3) continue;
			if (ZoogiCPUSmart::IsAura(X.Id) && Took.ContainsByPredicate([](const FName& T) { return ZoogiCPUSmart::IsAura(T); })) continue; // b125: one aura a turn''')
print('b125a ok')
