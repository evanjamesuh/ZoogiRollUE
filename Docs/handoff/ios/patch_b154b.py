# b154b (Evan Nov 6 06:40 "raid ai teammates should focus on the boss not knocking me out"):
# RAID: every CPU shot goes at the titan's weak point. The line is checked for teammates (a corridor two Zoogis wide);
# if one is in the way the CPU cuts up to 40 deg either side (the weak point is a 150 cm sphere, so a cut still lands),
# and if every line is crowded it only nudges itself to an open spot. Power = just enough to reach the hand at full-damage
# speed (~1700 cm/s) instead of the old "push it past the edge" full blast. The boss-attack dodge still overrides.
# -ZoogiRaidOldAI = the b151 planner.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''	{	// b147a (Evan: "CPU players dodge with the same rules"): out of the boss's red zone''', '''	if (AActor* RH = ZoogiRaid::HandActor(); RH && ZoogiRaid::Active() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidOldAI")))
	{	// b154b (Evan: "raid ai teammates should focus on the boss not knocking me out")
		const FVector2D H(RH->GetActorLocation());
		const float HD = FVector2D::Distance(B, H);
		auto AllyInLine = [&](const FVector2D& Dir, float Len) -> const AZoogiBall*
		{
			for (const AZoogiBall* A : Balls)
			{
				if (!A || A == Me || A->bOut || !IsAlly(Me, A)) continue;
				const FVector2D Q = FVector2D(A->BallLocation()) - B;
				const float T = FVector2D::DotProduct(Q, Dir);
				if (T < -20.f || T > Len + 150.f) continue;
				if ((Q - Dir * T).Size() < 2.f * ZoogiConst::ZoogiRadius + 30.f) return A;
			}
			return nullptr;
		};
		const FVector2D D0 = HD > 1.f ? (H - B) / HD : FVector2D(1.f, 0.f);
		const float MaxCut = HD > 1.f ? FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(140.f / HD, 0.f, 1.f))) : 0.f; // still on the 150 cm weak point
		float UseCut = 1e9f;
		for (float Cut : { 0.f, 5.f, -5.f, 10.f, -10.f, 16.f, -16.f, 24.f, -24.f, 32.f, -32.f, 40.f, -40.f })
		{
			if (FMath::Abs(Cut) > FMath::Max(MaxCut, 4.f) && Cut != 0.f) break;
			if (!AllyInLine(D0.GetRotated(Cut), HD)) { UseCut = Cut; break; }
		}
		if (UseCut < 1e8f)
		{
			BestDir = D0.GetRotated(UseCut); CPULock = RH;
			const float VHit = 1700.f; // full raid damage: min(20, 6 + v/120)
			BestFrac = FMath::Clamp(InvertLaunch(FMath::Sqrt(VHit * VHit + 2.f * Decel * FMath::Max(0.f, HD - 150.f))), 0.35f, 0.85f);
			UE_LOG(LogZoogi, Log, TEXT("RAID CPU P%d -> titan weak point (%.0f cm, cut %+.0f deg, %.0f%%)"), ActivePlayer + 1, HD, UseCut, BestFrac * 100.f);
		}
		else
		{	// every line to the hand runs through a teammate: a soft reposition to the clearest open side
			float BestClear = -1.f; FVector2D BestD = D0;
			for (int32 k = 0; k < 12; ++k)
			{
				const FVector2D Dk = D0.GetRotated(30.f * k);
				if (AllyInLine(Dk, 450.f)) continue;
				const float Clear = Arena ? Arena->DistanceToEdge(B, Dk) : 0.f;
				if (Clear > BestClear) { BestClear = Clear; BestD = Dk; }
			}
			BestDir = BestD; CPULock = nullptr;
			BestFrac = FMath::Clamp(InvertLaunch(FMath::Sqrt(2.f * Decel * FMath::Clamp(BestClear - 500.f, 150.f, 400.f))), 0.2f, 0.45f);
			UE_LOG(LogZoogi, Log, TEXT("RAID CPU P%d: teammates block the titan - repositions (%.0f%%)"), ActivePlayer + 1, BestFrac * 100.f);
		}
		bCPUUsePower = false; // no area powers next to teammates
	}
	{	// b147a (Evan: "CPU players dodge with the same rules"): out of the boss's red zone''')
print('b154b ok')
