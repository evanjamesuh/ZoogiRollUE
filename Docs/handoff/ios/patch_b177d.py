# b177d (Evan: "remove this bleacher and have the dragon sit in its place"): Frozen islet 0 (yaw 180) loses its bleacher + crowd;
# Dracoria lives there - sits, stands + roars when the boss is in play, flies out for ICE BREATH and comes home.
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(f,pairs):
    s=open(R+f,'rb').read().decode('utf-8')
    for old,new in pairs:
        done=False
        for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
            if s.count(o)==1: s=s.replace(o,n); done=True; break
        if not done: print('MISS',f,repr(old[:80])); sys.exit(1)
    open(R+f,'wb').write(s.encode('utf-8')); print('OK',f)
patch('Public/ZoogiDragonBoss.h',[('''	ZOOGIROLL_API bool Wanted(const AZoogiGameMode* G);''','''	ZOOGIROLL_API bool Wanted(const AZoogiGameMode* G);
	/** b177d: the Frozen islet the dragon sits on instead of a bleacher (-1 = none, -ZoogiOldFrostBoss) */
	ZOOGIROLL_API int32 SeatIslet();''')])
patch('Private/ZoogiArenaFrozen.cpp',[
('#include "ZoogiArena.h"\n','#include "ZoogiArena.h"\n#include "ZoogiDragonBoss.h"\n'),
('''		for (int32 sec = 0; sec < 3; ++sec)''','''		for (int32 sec = 0; sec < (k == ZoogiDragon::SeatIslet() ? 0 : 3); ++sec) // b177d: Dracoria's seat - no bleacher'''),
('''		for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(IsletYaw); ++k)
		{
			const float Yaw = IsletYaw[k];
			const FVector C = Pol(IsletD(k), Yaw, IsletTopZ);
			const FRotator Face(0.f, Yaw + 180.f, 0.f);
			for (int32 t = 0; t < StandRows; ++t)''','''		for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(IsletYaw); ++k)
		{
			if (k == ZoogiDragon::SeatIslet()) continue; // b177d: no crowd on the dragon's islet
			const float Yaw = IsletYaw[k];
			const FVector C = Pol(IsletD(k), Yaw, IsletTopZ);
			const FRotator Face(0.f, Yaw + 180.f, 0.f);
			for (int32 t = 0; t < StandRows; ++t)'''),
])
# ---- dragon: home seat state machine
f='Private/ZoogiDragonBoss.cpp'; s=open(R+f,'rb').read().decode('utf-8')
s=s.replace('#include "ZoogiHaptics.h"\n','#include "ZoogiHaptics.h"\n#include "ZoogiFrozenLayout.h"\n',1)
s=s.replace('enum class ES : uint8 { Away, Arrive, Perch, Leave, Dive, Breath, Return, Dead };','enum class ES : uint8 { Away, Arrive, Perch, Leave, Dive, Breath, Return, Dead, Seated };',1)
s=s.replace('static const TCHAR* Nm[] = { TEXT("away"), TEXT("arrive"), TEXT("perch"), TEXT("leave"), TEXT("dive"), TEXT("breath"), TEXT("return"), TEXT("dead") };','static const TCHAR* Nm[] = { TEXT("away"), TEXT("arrive"), TEXT("perch"), TEXT("leave"), TEXT("dive"), TEXT("breath"), TEXT("return"), TEXT("dead"), TEXT("seated") };',1)
a=s.index('	void Tick(AZoogiGameMode* G, float Dt,'); b=s.index('	void Sweep(bool bOn, float AngDeg)')
tick='''	int32 SeatIslet() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldFrostBoss")); return bOld ? -1 : 0; }

	void Tick(AZoogiGameMode* G, float Dt, const FVector& Ctr, float ER, float RimR, float RimZ, float PeekAng, bool bShow, bool bDefeated, const FVector& LookAt)
	{	// b177d: home = the islet that lost its bleacher; it sits there, stands + roars when the boss is in play
		AActor* A = D.A.Get(); USkeletalMeshComponent* Mc = D.Mc.Get();
		if (!A || !Mc || !G) return;
		D.Ctr = Ctr; D.ER = ER; D.RimR = RimR; D.RimZ = RimZ;
		D.T += Dt;
		const float K = Ease(D.T / D.Dur);
		if (!D.bOriented && D.T > 0.1f && Mc->GetBoneIndex(FName(TEXT("head"))) != INDEX_NONE && Mc->GetBoneIndex(FName(TEXT("pelvis"))) != INDEX_NONE)
		{	// face the mesh's head down the actor's +X, whatever axis it was modelled on
			const FVector H = A->GetActorTransform().InverseTransformPosition(Mc->GetBoneLocation(FName(TEXT("head"))));
			const FVector P = A->GetActorTransform().InverseTransformPosition(Mc->GetBoneLocation(FName(TEXT("pelvis"))));
			const FVector2D F(H.X - P.X, H.Y - P.Y);
			if (F.Size() > 1.f) { const float Off = FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X)); Mc->SetRelativeRotation(FRotator(0.f, -Off, 0.f)); D.bOriented = true; UE_LOG(LogZoogi, Log, TEXT("DRAGON mesh faces %.0f deg - turned to +X"), Off); }
		}
		if (D.OneLeft > 0.f) { D.OneLeft -= Dt; if (D.OneLeft <= 0.f) { const FString B = D.Base; D.Cur.Reset(); Loop(*B); } }
		const int32 Si = FMath::Max(0, SeatIslet());
		const float HomeYaw = ZoogiFrozen::IsletYaw[Si];
		const FVector Home = FVector(Ctr.X, Ctr.Y, Ctr.Z + ZoogiFrozen::IsletTopZ) + Dir(HomeYaw) * (ZoogiFrozen::IsletD(Si) + D.R * 0.1f);
		const float FaceIn = HomeYaw + 180.f;
		float WantYaw = FaceIn;
		const bool bForming = G->Arena && G->Arena->IsForming();
		A->SetActorHiddenInGame(bForming && (D.S == ES::Away || D.S == ES::Seated));
		if (bDefeated && D.S != ES::Dead) { BreathOn(false); Go(ES::Dead, 3.f); Once(TEXT("Death"), 0.8f); }
		if (D.bSweep && D.S != ES::Dive && D.S != ES::Breath && D.S != ES::Dead)
		{	// ICE BREATH: takes off from its seat and dives in over the rim
			A->SetActorHiddenInGame(false);
			D.HoverP = FVector(Ctr.X, Ctr.Y, RimZ) + Dir(D.SwMid) * (RimR + D.R * 0.35f) + FVector(0.f, 0.f, D.R * 0.42f);
			Go(ES::Dive, FMath::Max(0.6f, SweepLead() - 0.35f)); Loop(TEXT("Dive"), 1.1f);
			ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 1.f, 0.45f);
		}
		switch (D.S)
		{
		case ES::Away: // first tick: straight onto the seat
			D.Pos = Home; D.Yaw = FaceIn; Go(ES::Seated, 1.f); Loop(TEXT("Sit_Idle"), 0.8f);
			break;
		case ES::Seated:
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 3.f);
			if (bShow && !bForming) { Go(ES::Perch, 1.f); Once(TEXT("Sit_To_Stand")); D.Base = TEXT("Idle"); D.IdleT = 0.f; }
			else if ((D.IdleT += Dt) > 11.f && D.OneLeft <= 0.f) { D.IdleT = 0.f; Once(TEXT("Idle_LookAround"), 0.7f); D.Base = TEXT("Sit_Idle"); }
			break;
		case ES::Perch: // standing on its islet, watching the court
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 3.f);
			WantYaw = (FVector(LookAt.X, LookAt.Y, 0.f) - FVector(D.Pos.X, D.Pos.Y, 0.f)).Rotation().Yaw;
			WantYaw = FaceIn + FMath::Clamp(FMath::FindDeltaAngleDegrees(FaceIn, WantYaw), -40.f, 40.f);
			if (D.T > 1.2f && D.T < 1.3f && D.OneLeft <= 0.f) { Once(TEXT("Roar"), 0.9f); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 0.45f); ZoogiHaptics::Kick(0.5f, G->GetWorld(), TEXT("dragon_roar")); }
			if (!bShow && D.T > 4.f && D.OneLeft <= 0.f) { Go(ES::Seated, 1.f); Once(TEXT("Sit_Down")); D.Base = TEXT("Sit_Idle"); }
			else if ((D.IdleT += Dt) > 9.f && D.OneLeft <= 0.f) { D.IdleT = 0.f; Once(TEXT("Idle_LookAround"), 0.8f); }
			break;
		case ES::Dive:
			D.Pos = FMath::Lerp(D.From, D.HoverP, K) + FVector(0.f, 0.f, FMath::Sin(K * PI) * D.R * 0.5f); // up off the islet, then down to the hover
			WantYaw = (BeamP(D.SwAng) - D.Pos).Rotation().Yaw;
			if (D.T >= D.Dur) { Go(ES::Breath, 10.f); Loop(TEXT("Fly_Breath_Loop")); BreathOn(true); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.4f); }
			break;
		case ES::Breath:
		{
			D.Pos = FMath::VInterpTo(D.Pos, D.HoverP, Dt, 2.f) + FVector(0.f, 0.f, FMath::Sin(D.T * 3.f) * 6.f);
			const FVector Bp = BeamP(D.SwAng);
			WantYaw = (Bp - D.Pos).Rotation().Yaw;
			const FVector M = Mouth();
			if (UNiagaraComponent* Nc = D.Breath.Get()) Nc->SetWorldLocationAndRotation(M, (Bp - M).Rotation());
			LaserStep(G->GetWorld(), Dt, M, Bp);
			if (!D.bSweep) { BreathOn(false); Go(ES::Return, 2.f); Loop(TEXT("Glide")); }
			break;
		}
		case ES::Return: // glides home and lands on its islet
			D.Pos = FMath::Lerp(D.From, Home, K) + FVector(0.f, 0.f, FMath::Sin(K * PI) * D.R * 0.6f);
			WantYaw = K < 0.75f ? (Home - D.From).Rotation().Yaw : FaceIn;
			if (D.T > D.Dur * 0.7f && D.Cur != TEXT("Land")) Once(TEXT("Land"));
			if (D.T >= D.Dur) { Go(ES::Perch, 1.f); D.Base = TEXT("Idle"); if (D.OneLeft <= 0.f) Loop(TEXT("Idle"), 0.8f); }
			break;
		case ES::Dead:
			D.Pos = FMath::VInterpTo(D.Pos, Home, Dt, 1.f);
			break;
		default: Go(ES::Seated, 1.f); Loop(TEXT("Sit_Idle"), 0.8f); break;
		}
		D.Yaw = FMath::FixedTurn(D.Yaw, WantYaw, 140.f * Dt);
		A->SetActorLocationAndRotation(D.Pos, FRotator(0.f, D.Yaw, 0.f));
	}

'''
s=s[:a]+tick+s[b:]
# events: from the seat too (stands up first)
s=s.replace('''		if (!Active() || D.S != ES::Perch) return;''','''		if (!Active() || (D.S != ES::Perch && D.S != ES::Seated)) return;
		if (D.S == ES::Seated) { Go(ES::Perch, 1.f); Once(TEXT("Sit_To_Stand")); D.Base = TEXT("Idle"); return; } // b177d: stands up, then roars (Perch)''',1)
open(R+f,'wb').write(s.encode('utf-8')); print('OK dragon')
