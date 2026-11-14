# Nov 11 (Claude): B244 - Evan 08:38 "make ai intention knock orbs out but also block opposing player with earth bending effect to show
# case the use and urgency to use both super power in time with reaction while you have momentum so thinking quickly is of the essence"
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(rel, pairs):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
EARTH = '{ FName(TEXT("sapphire_wall")), FName(TEXT("long_rampart")), FName(TEXT("rock_rampart")), FName(TEXT("stone_barricade")), FName(TEXT("jade_wall")), FName(TEXT("obsidian_wall")), FName(TEXT("gold_wall")), FName(TEXT("rune_wall")), FName(TEXT("slab_row")), FName(TEXT("sandstone_ruins")) }'
0 and patch('Public/ZoogiCPUSmart.h', [('	ZOOGIROLL_API float TapRank(', '''	/** b244 (Evan): earth walls a CPU raises mid-roll to block a rival (not its own backstop) */
	ZOOGIROLL_API bool IsEarthBlock(FName Id);
	ZOOGIROLL_API float TapRank(''')])
0 and patch('Private/ZoogiCPUSmart.cpp', [
 ('	bool On() { static const bool bOff', '	bool IsEarthBlock(FName Id) { static const TSet<FName> L = ' + EARTH + '; static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCpuNoEarthBlock")); return !bOff && L.Contains(Id); }\n	bool On() { static const bool bOff'),
 ('		else if (K == TEXT("energy_buff")) V = 30.f + 22.f * (S.NearestRival > 1100.f ? 1.f : 0.f);',
  '''		else if (K == TEXT("energy_buff")) V = 30.f + 22.f * (S.NearestRival > 1100.f ? 1.f : 0.f);
		else if (IsEarthBlock(Id)) V = S.RivalsAlive ? Ag * (52.f + 22.f * RivalClose) + (S.OrbsIn ? 12.f : 0.f) : 0.f; // b244: knock orbs out AND wall a rival off'''),
 ('		if (K == TEXT("ice_wall")) return S.MyEdge < 900.f; // the Zoogi is heading for its edge: wall it off',
  '''		if (K == TEXT("ice_wall")) return S.MyEdge < 900.f; // the Zoogi is heading for its edge: wall it off
		if (IsEarthBlock(Id)) return S.RivalsAlive > 0 && G->PhaseTime > 0.25f; // b244: straight away, while the roll still has its momentum'''),
])
patch('Private/ZoogiSkills.cpp', [
 ('''			S.CpuA = ClampRange(Ctr + Perp * 280.f); S.CpuB = ClampRange(Ctr - Perp * 280.f);
		}
		if (D->Aim == EAim::Line && IsPackLine(Id))
		{	// b176e: attack lines run from the caster through the target''',
  '''			S.CpuA = ClampRange(Ctr + Perp * 280.f); S.CpuB = ClampRange(Ctr - Perp * 280.f);
		}
		bool bBlock = false;
		if (bSmart && D->Aim == EAim::Line && ZoogiCPUSmart::IsEarthBlock(Id))
		{	// b244 (Evan: "block opposing player with earth bending"): the wall rises across the nearest rival's way to the orbs
			AZoogiBall* Rv = nullptr; float Rd = 1e9f;
			for (AZoogiBall* X : G->Balls) if (X && X != B && !X->bOut && !G->IsAlly(B, X)) { const float Dd = FVector::Dist2D(X->BallLocation(), S.Origin); if (Dd < Rd) { Rd = Dd; Rv = X; } }
			if (Rv)
			{
				const FVector Rp = Rv->BallLocation();
				FVector Goal = G->Arena ? G->Arena->GetActorLocation() : S.Origin; float Od = 1e9f;
				for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float Dd = FVector::Dist2D(O->OrbLocation(), Rp); if (Dd < Od) { Od = Dd; Goal = O->OrbLocation(); } }
				FVector Dir = (Goal - Rp).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = (S.Origin - Rp).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
				const FVector Ctr = Rp + Dir * FMath::Clamp(Od * 0.5f, 180.f, 320.f), Perp(-Dir.Y, Dir.X, 0.f);
				S.CpuA = ClampRange(Ctr + Perp * 300.f); S.CpuB = ClampRange(Ctr - Perp * 300.f);
				bBlock = true;
				G->PublicPopup(FString::Printf(TEXT("%s: EARTH WALL - BLOCKED!"), *B->Def.Name.ToUpper()), Rp + FVector(0.f, 0.f, 150.f), FLinearColor(0.55f, 0.85f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL cpu P%d EARTH BLOCK %s in front of P%d (toward the orb at %.0f, %.0f)"), B->PlayerIndex + 1, *Id.ToString(), Rv->PlayerIndex + 1, Goal.X, Goal.Y);
			}
		}
		if (D->Aim == EAim::Line && IsPackLine(Id) && !bBlock)
		{	// b176e: attack lines run from the caster through the target'''),
 ('		S.CpuFireAt = S.OpenedAt + 1.5 + 0.35 * FMath::Max(0, N - 1) + FMath::FRandRange(0.f, 0.5f);',
  '		S.CpuFireAt = bBlock ? S.OpenedAt + 0.35 + FMath::FRandRange(0.f, 0.15f) /* b244: a quick reaction, still rolling */ : S.OpenedAt + 1.5 + 0.35 * FMath::Max(0, N - 1) + FMath::FRandRange(0.f, 0.5f);'),
])
patch('Private/ZoogiCardSubsystem.cpp', [('''				Decks[i] = Hand;
				TArray<FString> Nm; for (const FName& Id : Hand) Nm.Add(Id.ToString());''', '''				if (ZoogiCPUSmart::On() && !Hand.ContainsByPredicate([](const FName& X) { return ZoogiCPUSmart::IsEarthBlock(X); }))
				{	// b244 (Evan): every CPU carries an earth wall to block a rival with
					TArray<FName> Ew; for (const FName& Id : Pool) if (ZoogiCPUSmart::IsEarthBlock(Id)) Ew.Add(Id);
					if (Ew.Num() && Hand.Num()) Hand.Last() = Ew[Rs.RandRange(0, Ew.Num() - 1)];
				}
				Decks[i] = Hand;
				TArray<FString> Nm; for (const FName& Id : Hand) Nm.Add(Id.ToString());''')])
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B243"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B244"), TEXT("SMARTER CPU: KNOCK + BLOCK"), TEXT("CPU Zoogis now carry an earth wall and use both powers in one roll: they knock orbs out and, while still rolling fast, raise the wall across the nearest rival\'s way to the orbs (EARTH WALL - BLOCKED!). Watch them - then do it yourself: launch, then cast while you still have momentum.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
