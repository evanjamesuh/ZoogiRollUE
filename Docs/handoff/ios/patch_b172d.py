# b172d (Evan: "I fell below the map got stuck and couldn't launch, ensure no this can happen"): a Zoogi under the court is put back
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiGameMode.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('''	if (Ball->bOut) { if (bLaunchFlow) { LaunchFlow.CompleteCarry(Ball->PlayerIndex); PlaceOnPad(Ball, true); } else RespawnBall(Ball); } // never wait on the crane at turn start''',
'''	{	// b172d: never start a turn under the court / inside the scenery - put the Zoogi back on a free spot first
		const FVector Lb = Ball->BallLocation(); const float Fz = Arena ? Arena->GetActorLocation().Z : 0.f;
		if (!Ball->bOut && !Ball->bParked && Lb.Z < Fz - 120.f)
		{
			UE_LOG(LogZoogi, Warning, TEXT("RESCUE P%d started its turn under the court at (%.0f, %.0f, %.0f) - back on the court"), Ball->PlayerIndex + 1, Lb.X, Lb.Y, Lb.Z);
			RespawnBall(Ball);
		}
	}
	if (Ball->bOut) { if (bLaunchFlow) { LaunchFlow.CompleteCarry(Ball->PlayerIndex); PlaceOnPad(Ball, true); } else RespawnBall(Ball); } // never wait on the crane at turn start''')
ed('''	ZoogiArenaReact::KOShowTick(this); // b171: -ZoogiKOLine / -ZoogiKOShow test switches
''','''	ZoogiArenaReact::KOShowTick(this); // b171: -ZoogiKOLine / -ZoogiKOShow test switches
	{	// b172d: a Zoogi wedged under the court (above the fall-out depth, barely moving) for 1.5 s is put back on the court
		static float GStuck[8] = { 0.f };
		const float Fz = Arena ? Arena->GetActorLocation().Z : 0.f;
		for (int32 i = 0; i < Balls.Num() && i < 8; ++i)
		{
			AZoogiBall* Bs = Balls[i];
			if (!Bs || Bs->bOut || Bs->bParked || Phase == EZoogiPhase::Menu || Phase == EZoogiPhase::GameOver) { GStuck[i] = 0.f; continue; }
			const FVector Ls = Bs->BallLocation();
			const bool bUnder = Ls.Z < Fz - 150.f && Ls.Z > ZoogiConst::FallOutZ + 50.f && Bs->GetBallVelocity().Size() < 80.f;
			GStuck[i] = bUnder ? GStuck[i] + DeltaSeconds : 0.f;
			if (GStuck[i] > 1.5f)
			{
				GStuck[i] = 0.f;
				UE_LOG(LogZoogi, Warning, TEXT("RESCUE P%d stuck under the court at (%.0f, %.0f, %.0f) - back on the court"), i + 1, Ls.X, Ls.Y, Ls.Z);
				if (Bs->Sphere) Bs->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
				RespawnBall(Bs);
			}
		}
	}
''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
