# b95 (Evan 17:46-17:47): hologram wolves STAY on their target (latched, biting) until called - SUPERPOWER tap = recall, hold = charged
# blast; uncalled they blow on the target after 6 s (so the turn can end). -ZoogiWolfImpactBlow = b88 (explode on contact).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiClone.h', [
 ('	bool bImpactBlow = false; // b88: hit its target -> blows next tick\n',
  '	bool bImpactBlow = false; // b88: hit its target -> blows next tick\n	bool bLatchPending = false, bLatched = false; float LatchT = 0.f; FVector LatchOff = FVector::ZeroVector; // b95: stays on its target until called\n'),
])
ed(P + 'ZoogiClone.cpp', [
 ('''	if (bDetonated || !To) return;
	Recall = To;''', '''	if (bDetonated || !To) return;
	if (bLatched || bLatchPending)
	{	// b95: called off its target - physics back on, it runs home
		bLatched = bLatchPending = false;
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Sphere->SetSimulatePhysics(true);
	}
	Recall = To;'''),
 ('''	if (bImpactBlow) { Detonate(TEXT("impact")); return; } // b88''',
  '''	if (bImpactBlow) { Detonate(TEXT("impact")); return; } // b88
	if (bLatchPending || bLatched)
	{	// b95 (Evan: "wolves stay at the target unless called"): parked on its target, biting, until SUPERPOWER calls it
		AActor* Tg = Target.Get();
		const AZoogiBall* Tb = Cast<AZoogiBall>(Tg); const AZoogiOrb* To = Cast<AZoogiOrb>(Tg);
		const bool bGone = !Tg || Tg->IsHidden() || (Tb && Tb->bOut) || (To && To->bOut);
		const FVector Tp = Tb ? Tb->BallLocation() : (To ? To->OrbLocation() : (Tg ? Tg->GetActorLocation() : GetActorLocation()));
		if (bLatchPending && !bGone)
		{
			bLatchPending = false; bLatched = true; LatchT = 0.f;
			FVector Off = GetActorLocation() - Tp; Off.Z = 0.f;
			LatchOff = Off.GetSafeNormal() * ((Tb ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius) + CloneRadius * BodyScale + 6.f);
			if (LatchOff.IsNearlyZero()) LatchOff = FVector(1.f, 0.f, 0.f) * 90.f;
			Sphere->SetSimulatePhysics(false);
			Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UE_LOG(LogZoogi, Log, TEXT("WOLF hologram P%d latches onto %s"), OwnerPlayer + 1, *Tg->GetName());
		}
		LatchT += DeltaSeconds;
		if (bGone || LatchT > 6.f) { bLatched = false; Detonate(bGone ? TEXT("target gone") : TEXT("latched time up")); return; }
		SetActorLocation(Tp + LatchOff + FVector(0.f, 0.f, (CloneRadius - (Tb ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius)) + 3.f * FMath::Sin(Age * 9.f)));
		BodyYaw = FMath::RadiansToDegrees(FMath::Atan2(-LatchOff.Y, -LatchOff.X));
		if (BodyAnim != 1 || Age >= BiteUntil) PlayBody(1, true); // keeps biting
		UpdateBody(DeltaSeconds, 0.f);
		if (Body) Body->SetWorldRotation(FRotator(0.f, BodyYaw + MWYawOffset(), 0.f));
		return;
	}'''),
 ('''		if (OtherActor == Tg || !Tg || Tg->IsHidden()) { bImpactBlow = true; bHit = true; UE_LOG(LogZoogi, Log, TEXT("WOLF hologram P%d hits its target %s -> boom"), OwnerPlayer + 1, *OtherActor->GetName()); }''',
  '''		static const bool bImpactBlowOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfImpactBlow"));
		if (OtherActor == Tg || !Tg || Tg->IsHidden())
		{
			bHit = true;
			if (bImpactBlowOld || !Tg || Tg->IsHidden()) { bImpactBlow = true; UE_LOG(LogZoogi, Log, TEXT("WOLF hologram P%d hits its target %s -> boom"), OwnerPlayer + 1, *OtherActor->GetName()); }
			else bLatchPending = true; // b95: it stays on the target until called
		}'''),
])
print('b95 ok')
