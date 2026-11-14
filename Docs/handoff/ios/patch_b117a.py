# b117a (Evan: "Time slows and the camera goes to an angled wide view. You get 30 seconds to tap the targets, up to the number of
# wolves. Wolves fire straight towards that target like missiles with smoke trails like meteors. The wolves shoot out in a straight
# line from the Zoogi and each one blasts its target on impact. The alpha (third) hits harder."). -ZoogiWolfRun = the b96-b116 run.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:70]); open(p, 'wb').write(b.replace(o, n))
ed1('Public/ZoogiClone.h', '	bool bImpactBlow = false; // b88: hit its target -> blows next tick',
    '	bool bImpactBlow = false; // b88: hit its target -> blows next tick\n	bool bMissile = false; float PuffT = 0.f; // b117: straight-line missile run with a smoke trail, blows on impact')
C = 'Private/ZoogiClone.cpp'
ed1(C, '''	if (bImpactBlow) { Detonate(TEXT("impact")); return; } // b88''', '''	if (bImpactBlow) { Detonate(TEXT("impact")); return; } // b88
	if (bMissile && !bHit)
	{	// b117: a missile - dead straight at its target (re-aimed every frame so it connects), smoke + embers off the back
		AActor* Tg = Target.Get();
		if (!Tg || Tg->IsHidden()) if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(this)) if (AActor* Nt = Wp->PickReboundTarget(this, nullptr)) { Target = Nt; Tg = Nt; }
		if (!Tg || Tg->IsHidden() || Age > 4.f + ExtraLife || GetActorLocation().Z < ZoogiConst::FallOutZ) { Detonate(TEXT("missile spent")); return; }
		const AZoogiBall* Tb = Cast<AZoogiBall>(Tg); const AZoogiOrb* To = Cast<AZoogiOrb>(Tg);
		const FVector Tp = Tb ? Tb->BallLocation() : (To ? To->OrbLocation() : Tg->GetActorLocation());
		FVector Fly = Tp - GetActorLocation(); Fly.Z = 0.f;
		const float Dist = Fly.Size();
		if (Dist < CloneRadius * BodyScale + (Tb ? ZoogiConst::ZoogiRadius : ZoogiConst::OrbRadius) + 12.f) { bHit = true; bImpactBlow = true; UE_LOG(LogZoogi, Log, TEXT("WOLF missile P%d reaches %s -> boom"), OwnerPlayer + 1, *Tg->GetName()); return; }
		const FVector Dir = Fly / FMath::Max(Dist, 1.f);
		const FVector V0 = Sphere->GetPhysicsLinearVelocity();
		Sphere->SetPhysicsLinearVelocity(Dir * Speed + FVector(0.f, 0.f, FMath::Min(0.f, V0.Z)));
		BodyYaw = Dir.Rotation().Yaw;
		UpdateBody(DeltaSeconds, 0.f);
		PuffT += DeltaSeconds;
		while (PuffT >= 0.045f)
		{
			PuffT -= 0.045f;
			const FVector Back = GetActorLocation() - Dir * CloneRadius * BodyScale * 1.4f + FVector(0.f, 0.f, 10.f);
			AZoogiFX::Stylized(GetWorld(), TEXT("Smoke_v2"), Back + FMath::VRand() * 8.f, (bAlpha || bDire ? 0.16f : 0.11f) * FMath::FRandRange(0.85f, 1.2f));
			AZoogiFX::Spawn(GetWorld(), Back, FLinearColor(1.f, 0.45f, 0.12f), (bAlpha || bDire ? 46.f : 32.f), 0.28f, false, 6.f); // the meteor's ember glow
		}
		return;
	}''')
ed1(C, '''		// Nov 1: a wolf bouncing off a bumper / wall howls (side impacts only, not the floor)''', '''		if (bMissile && !bHit && Hit.ImpactNormal.Z < 0.5f) { bHit = true; bImpactBlow = true; return; } // b117: a missile blows on a wall too
		// Nov 1: a wolf bouncing off a bumper / wall howls (side impacts only, not the floor)''')
ed1(C, '''	PlayBody(1, true); // the werewolf bites whatever it hits''', '''	if (bMissile) { bHit = true; bImpactBlow = true; return; } // b117: impact = boom (whatever it hits first)
	PlayBody(1, true); // the werewolf bites whatever it hits''')
W = 'Private/ZoogiWolfPack.cpp'
ed1(W, '''	if (bBirdPick) { PaintLimit = 12.f; StartBird(); Toast(FString::Printf(TEXT("%d %s!"), NumWolves, NumWolves == 1 ? TEXT("WOLF") : TEXT("WOLVES")), FString::Printf(TEXT("PICK ALL %d TARGET%s - 12 SECONDS"), PaintNeed(), PaintNeed() == 1 ? TEXT("") : TEXT("S")), PackTint(B), 2.f, 1.f); }''',
'''	if (bBirdPick) { PaintLimit = 30.f; SetDil(0.05f); StartBird(); Toast(FString::Printf(TEXT("%d %s!"), NumWolves, NumWolves == 1 ? TEXT("WOLF") : TEXT("WOLVES")), FString::Printf(TEXT("PICK ALL %d TARGET%s - 30 SECONDS"), PaintNeed(), PaintNeed() == 1 ? TEXT("") : TEXT("S")), PackTint(B), 2.f, 1.f); } // b117 (Evan): 30 s, the world crawls meanwhile''')
ed1(W, '''	const bool bAlpha = Painted.Num() == 1;''', '''	static const bool bRunPaint = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfRun"));
	const bool bAlpha = bRunPaint ? Painted.Num() == 1 : Painted.Num() == 3; // b117 (Evan): the alpha is the third''')
ed1(W, '''		const FVector SpawnL = L + Off + FVector(0, 0, bThisDire ? 30.f : 8.f);''', '''		static const bool bRun = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfRun"));
		AActor* Aim = Assign.IsValidIndex(i) ? Assign[i] : (Assign.Num() ? Assign[0] : nullptr);
		FVector AimDir = Aim ? (WolfPos(Aim) - L).GetSafeNormal2D() : Fwd; if (AimDir.IsNearlyZero()) AimDir = Fwd;
		// b117: missiles leave from the Zoogi itself, each on the straight line to its own target (a little side offset so they never overlap)
		const FVector SpawnL = bRun ? L + Off + FVector(0, 0, bThisDire ? 30.f : 8.f)
			: L + AimDir * (ZoogiConst::ZoogiRadius + 70.f) + FVector::CrossProduct(FVector::UpVector, AimDir) * (Side * 30.f) + FVector(0, 0, bThisDire ? 30.f : 8.f);''')
ed1(W, '''		Cl->bAlpha = (i == 0 && !bDire);''', '''		Cl->bAlpha = bRun ? (i == 0 && !bDire) : (i == 2 && !bDire); // b117 (Evan): "the alpha (third) hits harder"''')
ed1(W, '''		Cl->Sphere->SetPhysicsLinearVelocity(Fwd * Cl->FormSpeed + Right * (Side * 260.f));''', '''		if (bRun) Cl->Sphere->SetPhysicsLinearVelocity(Fwd * Cl->FormSpeed + Right * (Side * 260.f));
		else
		{	// b117: fired - no V run, straight out at missile speed
			Cl->bMissile = true; Cl->FormUntil = 0.f; Cl->Speed = bThisDire ? 3000.f : 2600.f;
			Cl->Sphere->SetPhysicsLinearVelocity(AimDir * Cl->Speed);
		}''')
print('b117a ok')
