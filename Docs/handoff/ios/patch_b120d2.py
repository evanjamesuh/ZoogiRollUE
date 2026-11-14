# b120d (Evan 17:50, screenshot WOLF PACK button: "Button taps should never count as a target tap on field. If target selecting
# during aiming that counts as a target and if I push superpower it activates the effect."):
#  - ZoogiClassicPad::PointerOwned(): a SUPERPOWER / REACTION / WOLF press owns the pointer (held, or 0.35 s after the press);
#    AZoogiHUD::IsOverUI counts it, so no field tap / aim / target lock sees that touch
#  - Wolf Pack paint drags only count when the press began on the field (the held SUPERPOWER finger used to paint the orb under it)
#  - skill / redirect target picks ignore presses that belong to a button (pad, P5 command menu, AUTO / ARC bar)
#  - a target tapped while aiming (hard lock) + SUPERPOWER: the Wolf Pack sends the closest wolf straight at it (no pick step);
#    Bolt Balls fire at it
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
W = 'Private/ZoogiWolfPack.cpp'
ed1(W, '''	if (!Best) return;
	Painted.Add(Best);''', '''	if (!Best) return;
	PickActor(Best);
}

void UZoogiWolfPackSys::PickActor(AActor* Best)
{
	if (Phase != EPhase::Paint || !Best || Painted.Num() >= PaintMax) return;
	Painted.Add(Best);''')
ed1(W, '''	if (bDire) Toast(TEXT("PACK COMBO!"), TEXT("YOUR PARTNER'S PACK JOINS - DIRE WOLF"), PackGold, 2.f, 1.1f);''', '''	// b120 (Evan: "if target selecting during aiming that counts as a target and if I push superpower it activates the effect"):
	// the target tapped while aiming (hard lock) gets the closest wolf at once - no pick step; the rest stay with the player
	if (bFieldPack && bHumanOwner && G->Phase == EZoogiPhase::Aiming && G->TargetLock.IsValid() && !G->bSoftLock && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfNoLockFire")))
	{
		AActor* Lk = G->TargetLock.Get();
		if (Targets(B).Contains(Lk))
		{
			PickActor(Lk);
			UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s locked -> the closest wolf strikes it now"), Owner + 1, *Lk->GetName());
			Toast(TEXT("WOLF STRIKE!"), TEXT("YOUR TARGET"), PackTint(B), 1.4f, 1.f);
			Release();
			return;
		}
	}
	if (bDire) Toast(TEXT("PACK COMBO!"), TEXT("YOUR PARTNER'S PACK JOINS - DIRE WOLF"), PackGold, 2.f, 1.1f);''')
# bolt balls: the locked target first
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		FVector Dir = Best ?''', '''		if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		if (G->TargetLock.IsValid() && !G->bSoftLock && G->TargetLock.Get() != B) Best = G->TargetLock.Get(); // b120: the target picked while aiming
		FVector Dir = Best ?''')
print('b120d ok')
