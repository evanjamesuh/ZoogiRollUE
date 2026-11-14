# b146a: Cosmos boss puppet hooks for ZoogiBossAttack (sweep reach, slam toward, taunt)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
C = 'Private/ZoogiCosmosBoss.cpp'
ed1(C, '''		FBoss B;
		FTSTicker::FDelegateHandle Tick;''', '''		FBoss B;
		bool GSweepOn = false; float GSweepAng = 0.f; // b146: the hand sweep drives the peek angle
		FTSTicker::FDelegateHandle Tick;''')
ed1(C, '''			const float Dd = FMath::Abs(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek));''', '''			if (GSweepOn) { B.PeekAng = B.WantPeek = GSweepAng; B.bMoving = false; B.Dip = FMath::FInterpConstantTo(B.Dip, 0.f, Dt, 5.f); } // b146
			const float Dd = FMath::Abs(FMath::FindDeltaAngleDegrees(B.PeekAng, B.WantPeek));''')
ed1(C, '''			P->bLookAt = true;
			FVector Look = CamL;''', '''			if (GSweepOn) { P->HandPin = FVector(C.X, C.Y, C.Z) + Dir * (ER * 0.8f) + FVector(0.f, 0.f, 70.f); P->Lean = 1.2f; P->Pose = EZoogiPuppetPose::Peek; P->bPinHands = true; } // b146: both hands reach in and drag across the court
			P->bLookAt = true;
			FVector Look = CamL;''')
ed1(C, '''	void OnEvent(UWorld* W, const FString& Ev)
	{''', '''	bool Present(const AZoogiGameMode* G) { return G && B.G.Get() == G && B.A.IsValid() && B.Pup.IsValid(); }
	void Sweep(bool bOn, float AngDeg)
	{
		GSweepOn = bOn; GSweepAng = AngDeg;
		if (bOn) { B.Eye = 10.f; B.Dip = FMath::Min(B.Dip, 0.4f); }
	}
	void SlamToward(float AngDeg)
	{
		B.PeekAng = B.WantPeek = AngDeg; B.bMoving = false; B.Dip = FMath::Min(B.Dip, 0.25f);
		B.Slam = 1.f; B.Cool = 2.5f; B.Eye = 10.f; B.LeanTo = 8.f;
		Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.9f);
	}
	void Taunt()
	{
		B.Eye = 9.f; B.EyeBase = FMath::Max(B.EyeBase, 4.5f);
		if (B.Pup.IsValid()) B.RoarLeft = 1.6f;
		Play(Anim(TEXT("/Game/Mecha_Aurex/Anims/Actions/GEN_cast_a_spell.GEN_cast_a_spell")), 0.6f);
		BossCam(1.6f);
	}

	void OnEvent(UWorld* W, const FString& Ev)
	{''')
print('b146a ok')
