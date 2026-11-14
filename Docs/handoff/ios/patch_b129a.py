# b129a (Evan 02:57: "if I have 3 wolves the first pick sends all three to that location - I want to be able to choose three different
# targets first"): the b120 aim-lock shortcut fired the closest wolf at the locked target and closed the pick, and every later SUPERPOWER
# press re-picked the same locked target. Now: the target tapped while aiming is pick 1 (marked), the pick view stays open for the rest,
# every pick must be a different target, and the wolves launch together once all targets are chosen (or the time runs out).
# -ZoogiWolfStrikeOnPick = the b119/b120 strike-per-pick + lock shortcut.
# + b129 NEWS portrait: the text column is a little narrower (portrait lines ran past the card edge).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
W = 'Private/ZoogiWolfPack.cpp'
ed1(W, '''			PickActor(Lk);
			UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s locked -> the closest wolf strikes it now"), Owner + 1, *Lk->GetName());
			Toast(TEXT("WOLF STRIKE!"), TEXT("YOUR TARGET"), PackTint(B), 1.4f, 1.f);
			Release();
			return;''', '''			PickActor(Lk);
			static const bool bStrikeOnPick = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfStrikeOnPick"));
			if (bStrikeOnPick || Painted.Num() >= PaintNeed())
			{
				UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s locked -> the closest wolf strikes it now"), Owner + 1, *Lk->GetName());
				Toast(TEXT("WOLF STRIKE!"), TEXT("YOUR TARGET"), PackTint(B), 1.4f, 1.f);
				Release();
				return;
			}
			// b129 (Evan: "I want to choose three different targets first"): the locked target is pick 1, pick the rest
			UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s is pick 1 of %d"), Owner + 1, *Lk->GetName(), PaintNeed());
			Toast(TEXT("TARGET 1 SET"), FString::Printf(TEXT("PICK %d MORE"), PaintNeed() - Painted.Num()), PackTint(B), 1.4f, 1.f);''')
ed1(W, '''	if (bFieldPack)
	{	// b119 (Evan): "when chosen the closest wolf strikes"''', '''	static const bool bStrikeEach = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfStrikeOnPick"));
	if (bFieldPack && bStrikeEach)
	{	// b119 (Evan): "when chosen the closest wolf strikes" (b129: only with -ZoogiWolfStrikeOnPick - the pack now waits for every pick)''')
ed1(W, '''		TArray<TWeakObjectPtr<AZoogiClone>>& F = FieldWolves.FindOrAdd(Owner);
		if (!bHumanOwner)''', '''		TArray<TWeakObjectPtr<AZoogiClone>>& F = FieldWolves.FindOrAdd(Owner);
		if (bHumanOwner && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfStrikeOnPick")))
		{	// b129: all targets chosen - each picked target gets the closest waiting wolf, all at once
			for (const TWeakObjectPtr<AActor>& Pw : Painted)
			{
				AActor* Tp = Pw.Get(); if (!Tp) continue;
				AZoogiClone* Near = nullptr; float NearD = 1e12f;
				for (const TWeakObjectPtr<AZoogiClone>& Wk : F) if (AZoogiClone* Cw = Wk.Get()) if (Cw->bWaiting) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(Tp)); if (Dd < NearD) { NearD = Dd; Near = Cw; } }
				if (Near) { F.Remove(Near); Wolves.Add(Near); WolfHits.Add(0); WolfPopped.Add(0); Near->Strike(Tp); }
			}
			UE_LOG(LogZoogi, Log, TEXT("WOLF P%d: %d target(s) chosen -> the pack runs"), Owner + 1, Wolves.Num());
		}
		if (!bHumanOwner)''')
ed1('Private/ZoogiFront.cpp', '''		const float Top = 56.f, Bot = 24.f, ViewH = Sz.Y - Top - Bot, TextW = Sz.X - 100.f;''',
'''		const float Top = 56.f, Bot = 24.f, ViewH = Sz.Y - Top - Bot, TextW = (Sz.X - 100.f) * (bPortN ? 0.9f : 1.f); // b129: portrait lines ran past the card''')
print('b129a ok')
