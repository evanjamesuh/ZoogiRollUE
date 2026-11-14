# b140a (Evan 10:47 "When a player is knocked out the phone should vibrate for 2s"; 10:58 "The blast that happens when a player
# is knocked out should happen at that players knockout location"):
#  - AZoogiBall::SetOut: 2 s haptic rumble on every Zoogi knock-out (-ZoogiNoKOBuzz = off)
#  - CheckKnockoffs: the KO blast / shockwave / burst / FELL OFF popup at the Zoogi's own knock-out spot (was the court edge point
#    on the same radial - on Frozen Ring 2 that put the blast ~800 cm inside the court). -ZoogiOldKOSpot = edge point.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiBall.cpp', '''		// Feel 43/27: comic flail + crowd BEFORE bOut (SpinBoost/Wobble refuse after).
		ZoogiFeel::OnKnockOff(this);
	}''', '''		// Feel 43/27: comic flail + crowd BEFORE bOut (SpinBoost/Wobble refuse after).
		ZoogiFeel::OnKnockOff(this);
		// b140 (Evan: "When a player is knocked out the phone should vibrate for 2s")
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoKOBuzz"))) ZoogiHaptics::Rumble(2.f, 1.f, GetWorld(), TEXT("knockout"));
	}''')
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''		if (AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300
		{
			AZoogiFX::Spawn(GetWorld(), EdgePoint(L, Arena), FLinearColor(1.f, 0.3f, 0.25f), 220.f, 0.7f, false, 7.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), EdgePoint(L, Arena), FRotator::ZeroRotator, 1.4f);
			// Oct 23: Easy Shockwaves vertical + ring-out 2s SFX (attacker tint when known).
			{
				const FVector At = EdgePoint(L, Arena);''', '''		if (AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300
		{
			// b140 (Evan: "The blast that happens when a player is knocked out should happen at that players knockout location"):
			// the Zoogi's own spot (a fall keeps its x / y, lifted to near the floor so the blast reads); -ZoogiOldKOSpot = court edge point
			static const bool bOldKOSpot = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldKOSpot"));
			const FVector KOAt = bOldKOSpot ? EdgePoint(L, Arena) : FVector(L.X, L.Y, FMath::Clamp(L.Z, -60.f, 600.f) + 40.f);
			UE_LOG(LogZoogi, Log, TEXT("KO blast P%d at (%.0f, %.0f, %.0f) r%.0f (edge point r%.0f)"), B->PlayerIndex + 1, KOAt.X, KOAt.Y, KOAt.Z, FVector2D(L.X, L.Y).Size(), FVector2D(EdgePoint(L, Arena)).Size());
			AZoogiFX::Spawn(GetWorld(), KOAt, FLinearColor(1.f, 0.3f, 0.25f), 220.f, 0.7f, false, 7.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), KOAt, FRotator::ZeroRotator, 1.4f);
			// Oct 23: Easy Shockwaves vertical + ring-out 2s SFX (attacker tint when known).
			{
				const FVector At = KOAt;''')
ed1(G, '''			AddPopup(FString::Printf(TEXT("-%d FELL OFF  P%d"), ZoogiConst::PenaltyFall, B->PlayerIndex + 1), EdgePoint(L, Arena), FLinearColor(1.f, 0.3f, 0.25f));''',
'''			AddPopup(FString::Printf(TEXT("-%d FELL OFF  P%d"), ZoogiConst::PenaltyFall, B->PlayerIndex + 1), KOAt, FLinearColor(1.f, 0.3f, 0.25f));''')
print('b140a ok')
