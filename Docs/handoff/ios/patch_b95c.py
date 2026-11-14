# b95c (Evan 17:52: "game stalls on this view after the new duel"): watchdogs (real time) so a match can never hang -
# a duel/clash over 40 s ends; a rolling shot / clash resolve stuck over 20 s resolves; a flung loser over 8 s lands; a CPU menu
# over 12 s picks ATTACK. The P5 menu also closes when the CPU's turn moved on (it stayed drawn, frozen, over a stuck phase).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiP5Command.cpp', [
 ('''			if (Cb && Cb->bCPU && G->TurnCounter == GS.CpuTurn && G->Phase == EZoogiPhase::Aiming && !ZoogiP5::ResultsOn()) { CpuTick(G, Cb, W, World, VC, Now); return true; }
			GS.bCpuMenu = false; W.bPuppet = false;''',
  '''			const bool bCpuStuck = Now - GS.CpuAt > 12.0; // b95c watchdog: a CPU menu never runs longer than 12 s
			if (Cb && Cb->bCPU && G->TurnCounter == GS.CpuTurn && G->Phase == EZoogiPhase::Aiming && !ZoogiP5::ResultsOn() && !bCpuStuck) { CpuTick(G, Cb, W, World, VC, Now); return true; }
			if (bCpuStuck) UE_LOG(LogZoogi, Warning, TEXT("P5CMD watchdog: CPU menu open %.0f s -> closed"), Now - GS.CpuAt);
			GS.bCpuMenu = false; W.bPuppet = false;
			W.bMenu = false; W.Mode = 0; W.Hover = -1; GShowing = false; // b95c: don't leave the CPU's menu drawn over a turn that moved on'''),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('''	if (Flung.Actor.IsValid()) TickFlung(G, Dt); // build 28
	if (bResolveAfterClash && !Clash.bOn && !Flung.Actor.IsValid())''',
  '''	if (Flung.Actor.IsValid()) TickFlung(G, Dt); // build 28
	{	// b95c (Evan: "game stalls after the new duel"): real-time watchdogs - nothing may hold a match up for long
		static double ClashSince = 0.0, FlungSince = 0.0, PhaseSince = 0.0; static EZoogiPhase LastPh = EZoogiPhase::Menu; static int32 LastTurn = -1;
		const double Nw = FPlatformTime::Seconds();
		if (!Clash.bOn) ClashSince = Nw; else if (Nw - ClashSince > 40.0) { UE_LOG(LogZoogi, Warning, TEXT("WATCHDOG: clash/duel on %.0f s -> ended"), Nw - ClashSince); ClashSince = Nw; if (Clash.bV2) DuelV2End(G, false); else EndClash(G); }
		if (!Flung.Actor.IsValid()) FlungSince = Nw; else if (Nw - FlungSince > 8.0) { UE_LOG(LogZoogi, Warning, TEXT("WATCHDOG: flung loser %.0f s -> landed"), Nw - FlungSince); FlungSince = Nw; EndFlung(true); }
		if (G->Phase != LastPh || G->TurnCounter != LastTurn) { LastPh = G->Phase; LastTurn = G->TurnCounter; PhaseSince = Nw; }
		else if (G->Phase == EZoogiPhase::Rolling && !Clash.bOn && Nw - PhaseSince > 20.0)
		{
			UE_LOG(LogZoogi, Warning, TEXT("WATCHDOG: P%d's shot rolling %.0f s (resolve pending %d) -> resolved"), G->ActivePlayer + 1, Nw - PhaseSince, bResolveAfterClash ? 1 : 0);
			PhaseSince = Nw; bResolveAfterClash = false; ResolveHoldT = 0.f;
			G->ForceResolveNow(TEXT("watchdog: shot stuck"));
		}
	}
	if (bResolveAfterClash && !Clash.bOn && !Flung.Actor.IsValid())'''),
])
print('b95c ok')
