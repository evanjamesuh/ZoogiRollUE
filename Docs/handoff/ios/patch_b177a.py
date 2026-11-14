# b177a: Dracoria = the Frozen boss (ZoogiDragonBoss) + ICE BREATH (the boss sweep as a dive + mouth beam)
import sys,os,shutil
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
S=os.path.expanduser('~/mnt/dev/ios/b177/')
shutil.copy(S+'ZoogiDragonBoss.h', R+'Public/ZoogiDragonBoss.h'); shutil.copy(S+'ZoogiDragonBoss.cpp', R+'Private/ZoogiDragonBoss.cpp')
def patch(f,pairs):
    s=open(R+f,'rb').read().decode('utf-8')
    for old,new in pairs:
        done=False
        for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
            if s.count(o)==1: s=s.replace(o,n); done=True; break
        if not done: print('MISS',f,repr(old[:80])); sys.exit(1)
    open(R+f,'wb').write(s.encode('utf-8')); print('OK',f)
patch('Private/ZoogiCosmosBoss.cpp',[
('#include "ZoogiAurexPuppet.h"\n','#include "ZoogiAurexPuppet.h"\n#include "ZoogiDragonBoss.h"\n'),
('''				B.A = P; B.Pup = P; B.G = G; B.H = PeekH;''','''				B.A = P; B.Pup = P; B.G = G; B.H = PeekH;
				if (ZoogiDragon::Wanted(G) && ZoogiDragon::Spawn(G)) { P->SetActorHiddenInGame(true); UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: Frozen = DRACORIA (the titan puppet only steers)")); } // b177a'''),
('''			P->LookAt = Look;''','''			P->LookAt = Look;
			if (ZoogiDragon::Active()) ZoogiDragon::Tick(G, Dt, C, ER, Rt.X, Rt.Y, B.PeekAng, bShow, GDefeated, Look); // b177a'''),
('''			BossCollision(P, !GDefeated && !GSweepOn && B.Dip < 0.45f);''','''			BossCollision(P, !ZoogiDragon::Active() && !GDefeated && !GSweepOn && B.Dip < 0.45f);'''),
('''			const FVector HeadP = P->Body ? P->Body->GetBoneLocationByName(TEXT("head"), EBoneSpaces::WorldSpace) : P->GetActorLocation();''','''			const FVector HeadP = ZoogiDragon::Active() ? ZoogiDragon::HeadLoc() : (P->Body ? P->Body->GetBoneLocationByName(TEXT("head"), EBoneSpaces::WorldSpace) : P->GetActorLocation());'''),
('''		GSweepOn = bOn; GSweepAng = AngDeg;''','''		GSweepOn = bOn; GSweepAng = AngDeg;
		ZoogiDragon::Sweep(bOn, AngDeg); // b177a'''),
('''		B.Slam = 1.f; B.Cool = 2.5f; B.Eye = 10.f; B.LeanTo = 8.f;''','''		B.Slam = 1.f; B.Cool = 2.5f; B.Eye = 10.f; B.LeanTo = 8.f;
		ZoogiDragon::Event(TEXT("Stomp"));'''),
('''		if (B.Pup.IsValid()) B.RoarLeft = 1.6f;
		Play(''','''		if (B.Pup.IsValid()) B.RoarLeft = 1.6f;
		ZoogiDragon::Event(TEXT("Taunt"));
		Play('''),
('''	FString Name() { return GTheme.Name; }''','''	FString Name() { return ZoogiDragon::Active() ? FString(TEXT("DRACORIA")) : FString(GTheme.Name); } // b177a'''),
('''	void Flinch() { B.Eye = 10.f;''','''	void Flinch() { ZoogiDragon::Event(TEXT("Hit")); B.Eye = 10.f;'''),
('''			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: KO"));''','''			ZoogiDragon::Event(TEXT("Roar"));
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: KO"));'''),
('''			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: combo - leans in"));''','''			ZoogiDragon::Event(TEXT("Threat"));
			UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS reacts: combo - leans in"));'''),
('''		Clear();
		GHold = false; GDefeated = false; GSweepOn = false; // b151''','''		Clear();
		ZoogiDragon::Clear(); // b177a
		GHold = false; GDefeated = false; GSweepOn = false; // b151'''),
])
patch('Private/ZoogiBossAttack.cpp',[
('#include "ZoogiCosmosBoss.h"\n','#include "ZoogiCosmosBoss.h"\n#include "ZoogiDragonBoss.h"\n'),
('''			auto AngAt = [](float T) { const float K = FMath::Clamp(T / SweepTime, 0.f, 1.f);''','''			const float Lead = ZoogiDragon::SweepLead(), SwT = SweepTime * ZoogiDragon::SweepSlow(); // b177a: Dracoria dives in first, the beam crosses slower
			auto AngAt = [Lead, SwT](float T) { const float K = FMath::Clamp((T - Lead) / SwT, 0.f, 1.f);'''),
('''			if (S.FxT <= 0.f)
			{
				S.FxT = 0.05f;''','''			if (S.FxT <= 0.f && S.RunT >= Lead)
			{
				S.FxT = 0.05f;'''),
('''			if (TestMode() && !S.bShotMid && S.RunT > SweepTime * 0.5f)''','''			if (TestMode() && !S.bShotMid && S.RunT > Lead + SwT * 0.5f)'''),
('''				if (!M.bOffered && !M.bCover && D1 > 0.f''','''				if (S.RunT >= Lead && !M.bOffered && !M.bCover && D1 > 0.f'''),
('''F->PublicCallout(TEXT("TAP TO BRACE!"), TEXT("THE HAND IS ON YOU")''','''F->PublicCallout(TEXT("TAP TO BRACE!"), ZoogiDragon::Active() ? TEXT("THE ICE BEAM IS ON YOU") : TEXT("THE HAND IS ON YOU")'''),
('''			if (S.RunT >= SweepTime + 0.15f) { ZoogiCosmosBoss::Sweep(false, A1);''','''			if (S.RunT >= Lead + SwT + 0.15f) { ZoogiCosmosBoss::Sweep(false, A1);'''),
('''bSw ? TEXT("HAND SWEEP!") : TEXT("GROUND SLAM!")''','''bSw ? (ZoogiDragon::Active() ? TEXT("ICE BREATH!") : TEXT("HAND SWEEP!")) : TEXT("GROUND SLAM!")'''),
('''			const FString Nm = S.Kind == EKind::Sweep ? TEXT("HAND SWEEP") : TEXT("GROUND SLAM");''','''			const FString Nm = S.Kind == EKind::Sweep ? ZoogiDragon::SweepName(TEXT("HAND SWEEP")) : TEXT("GROUND SLAM");'''),
])
