# b221 (Claude): turret shots leave the BARREL and fly STRAIGHT along it; 6 shot styles (-ZoogiTurretShot=1..6); demo rig -ZoogiDemoTurret
import os, sys
SRC = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(path, pairs):
    p = os.path.join(SRC, path); s = open(p).read()
    for a, b in pairs:
        assert a in s, (path, a[:90]); s = s.replace(a, b, 1)
    open(p, 'w').write(s)

# ---------------------------------------------------------------- ZoogiSkills.cpp
patch('Private/ZoogiSkills.cpp', [
('''		struct FTurSlash { TWeakObjectPtr<AActor> Turret; TWeakObjectPtr<AActor> Carrier; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float T = 0.f, Dur = 0.3f; int32 Owner = -1; float Pow = 1.f; TWeakObjectPtr<AActor> Target; };''',
'''		struct FTurSlash { TWeakObjectPtr<AActor> Turret; TWeakObjectPtr<AActor> Carrier; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float T = 0.f, Dur = 0.3f; int32 Owner = -1; float Pow = 1.f; TWeakObjectPtr<AActor> Target;
			int32 Style = 1; float Push = 1.f; bool bShown = false; double NextTrail = 0.0; }; // b221: shot style, T < 0 = waiting (tracer bursts)
		// b221 (Evan Nov 10: turret shots went out sideways and curved after the target - "only straight", "confirm bullets shoot from the
		// barrel"): every shot leaves the barrel tip and flies straight along the barrel. -ZoogiTurretShot=1..6 picks the shot:
		// 1 CRESCENT SLASH (b202), 2 TRACER BURST, 3 PLASMA BOLT, 4 LASER BEAM, 5 ROCKET, 6 SHOCK RING. -ZoogiTurretYaw= = the model's
		// facing offset (its barrel is local +Y -> -90).
		int32 TurretStyleNow() { static int32 S = -1; if (S < 0) { S = 1; FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretShot="), S); S = FMath::Clamp(S, 1, 6); } return S; }
		float TurYawOff() { static float Y = -1000.f; if (Y < -999.f) { Y = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretYaw="), Y); } return Y; }
		FVector BarrelDir(const AActor* A) { return A ? FRotator(0.f, A->GetActorRotation().Yaw - TurYawOff(), 0.f).Vector() : FVector(1.f, 0.f, 0.f); }'''),
('''		void TurSlashTick(AZoogiGameMode* G, float Dt)
		{''',
'''		void TurSlashTick(AZoogiGameMode* G, float Dt)
		{
			static uint64 TsFrame = 0; if (TsFrame == GFrameCounter) return; TsFrame = GFrameCounter; // b221: once a frame (the game tick + the demo rig)'''),
# the old slash function -> the generic barrel shot
('''		void TurretSlash(AZoogiGameMode* G, const FTurret& T, AActor* Target, const FVector& Tp)
		{''',
'''		AStaticMeshActor* ShotCarrier(UWorld* W, const TCHAR* Shape, const FVector& At, const FRotator& Rot, const FVector& Scl, const FLinearColor& Col, float Glow)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* C = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rot, At), SP);
			if (!C) return nullptr;
			C->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* Mc = C->GetStaticMeshComponent();
			Mc->SetStaticMesh(ZoogiUtil::BasicShape(Shape)); Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mc->SetCastShadow(false);
			Mc->SetMaterial(0, ZoogiUtil::FxMID(C, Col, Glow)); C->SetActorScale3D(Scl);
			return C;
		}
		/** b221: one turret shot of Style from the barrel tip, straight along the barrel; returns the seconds until it lands */
		float BarrelShot(AZoogiGameMode* G, AActor* Tur, const FVector& Base, float TipFwd, float TipUp, const FVector& Tp, int32 Owner, float Pow, int32 Style, AActor* Target)
		{
			UWorld* W = G ? G->GetWorld() : nullptr; if (!W || !Tur) return 0.f;
			const FVector Fw = BarrelDir(Tur);
			const FVector From = FVector(Base.X, Base.Y, Base.Z) + Fw * TipFwd + FVector(0.f, 0.f, TipUp);
			const float Dist = FMath::Clamp((float)FVector::Dist2D(From, Tp), 150.f, 1400.f);
			const FVector To = From + Fw * Dist + FVector(0.f, 0.f, Tp.Z - From.Z); // straight out of the barrel (no homing)
			const FVector Dir3 = (To - From).GetSafeNormal();
			static const float Speeds[7] = { 2200.f, 2200.f, 4200.f, 1700.f, 9000.f, 1500.f, 1800.f };
			const FLinearColor Cols[7] = { FLinearColor(0.75f, 0.35f, 1.f), FLinearColor(0.75f, 0.35f, 1.f), FLinearColor(1.f, 0.82f, 0.3f), FLinearColor(1.f, 0.3f, 0.9f), FLinearColor(1.f, 0.15f, 0.2f), FLinearColor(1.f, 0.55f, 0.15f), FLinearColor(0.3f, 0.9f, 1.f) };
			Style = FMath::Clamp(Style, 1, 6);
			const FLinearColor Col = Cols[Style];
			AZoogiFX::Spawn(W, From, Col, Style == 2 ? 70.f : 120.f, 0.14f, false, 16.f); // muzzle flash at the barrel tip
			const float Dur = FMath::Clamp(FVector::Dist(From, To) / Speeds[Style], 0.06f, 0.9f);
			auto Add = [&](AActor* C, float Delay, float Push)
			{
				FTurSlash S2; S2.Turret = Tur; S2.Carrier = C; S2.From = From; S2.To = To; S2.Dur = Dur; S2.T = -Delay; S2.Owner = Owner; S2.Pow = Pow; S2.Target = Target; S2.Style = Style; S2.Push = Push;
				if (C && Delay > 0.f) C->SetActorHiddenInGame(true);
				TurSlashes.Add(S2);
			};
			switch (Style)
			{
			case 2: // TRACER BURST: three gold rounds, 0.08 s apart
				for (int32 k = 0; k < 3; ++k) Add(ShotCarrier(W, TEXT("Sphere"), From, Dir3.Rotation(), FVector(0.75f, 0.13f, 0.13f), Col, 22.f), 0.08f * k, k == 2 ? 0.7f : 0.35f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.7f, 2.2f);
				break;
			case 3: // PLASMA BOLT: a big glowing ball with a trail
				Add(ShotCarrier(W, TEXT("Sphere"), From, Dir3.Rotation(), FVector(0.55f), Col, 14.f), 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.2f);
				break;
			case 4: // LASER BEAM: a beam from the barrel tip to the end of its reach, lands at once
			{
				const float Len = FVector::Dist(From, To);
				AStaticMeshActor* Bm = ShotCarrier(W, TEXT("Cylinder"), (From + To) * 0.5f, FRotationMatrix::MakeFromZ(Dir3).Rotator(), FVector(0.14f, 0.14f, Len / 100.f), Col, 30.f);
				if (Bm) Bm->SetLifeSpan(0.28f);
				Add(nullptr, 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.9f, 0.8f);
				break;
			}
			case 5: // ROCKET: a nose cone with a smoke trail, a fire burst on impact
				Add(ShotCarrier(W, TEXT("Cone"), From, FRotationMatrix::MakeFromZ(Dir3).Rotator(), FVector(0.32f, 0.32f, 0.75f), Col, 10.f), 0.f, 1.2f);
				ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.5f, 0.7f);
				break;
			case 6: // SHOCK RING: a ring that grows as it flies
				Add(ShotCarrier(W, TEXT("Cylinder"), From, FRotationMatrix::MakeFromZ(Dir3).Rotator(), FVector(0.6f, 0.6f, 0.04f), Col, 12.f), 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.5f);
				break;
			default: // 1 CRESCENT SLASH (b202)
			{
				AStaticMeshActor* C = ShotCarrier(W, TEXT("Sphere"), From, Dir3.Rotation(), FVector(0.35f), Col, 14.f);
				AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), From, 0.42f, 2.f, C, FVector::ZeroVector, Fw.Rotation());
				Add(C, 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.7f);
				break;
			}
			}
			UE_LOG(LogZoogi, Log, TEXT("TURRET shot style %d of P%d: barrel tip (%.0f, %.0f, %.0f) -> straight %.0f cm along yaw %.0f"), Style, Owner + 1, From.X, From.Y, From.Z, Dist, Fw.Rotation().Yaw);
			return Dur + (Style == 2 ? 0.16f : 0.f);
		}
		void TurretSlash(AZoogiGameMode* G, const FTurret& T, AActor* Target, const FVector& Tp)
		{
			static const bool bOldSlash = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTurretOldSlash"));
			if (!bOldSlash) { if (AActor* A = T.A.Get()) BarrelShot(G, A, T.P, T.TipFwd, T.TipUp, Tp, T.Owner, T.Pow, TurretStyleNow(), Target); return; } // b221'''),
# TurSlashTick: straight, styles, delays
('''				FTurSlash& S2 = TurSlashes[i];
				if (const AActor* Ta = S2.Target.Get()) { const FVector Tl = Cast<AZoogiBall>(Ta) ? Cast<AZoogiBall>(Ta)->BallLocation() : Cast<AZoogiOrb>(Ta) ? Cast<AZoogiOrb>(Ta)->OrbLocation() : Ta->GetActorLocation(); S2.To = Tl; } // homes on where it is now
				S2.T += Dt;
				const float K = FMath::Clamp(S2.T / S2.Dur, 0.f, 1.f);
				if (AActor* C = S2.Carrier.Get()) C->SetActorLocation(FMath::Lerp(S2.From, S2.To, K));
				if (K < 1.f) continue;
				if (AActor* C = S2.Carrier.Get()) C->Destroy();
				const FVector Dir = (S2.To - S2.From).GetSafeNormal2D();
				AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), S2.To, 0.5f, 2.f, nullptr, FVector::ZeroVector, Dir.Rotation());
				AZoogiFX::Spawn(W, S2.To, FLinearColor(0.75f, 0.35f, 1.f), 180.f, 0.3f, true, 10.f);
				ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.6f, 1.4f);
				{ FTuneScope Tn3(S2.Pow, FName(TEXT("gattling_turret"))); Blast(G, S2.To - Dir * 70.f, 150.f, 950.f, 140.f, nullptr, S2.Owner); }
				TurSlashes.RemoveAt(i);''',
'''				FTurSlash& S2 = TurSlashes[i];
				// b221: no homing - the shot flies straight along the barrel (-ZoogiTurretOldSlash = the b202 homing slash)
				static const bool bHoming = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTurretOldSlash"));
				if (bHoming) if (const AActor* Ta = S2.Target.Get()) { const FVector Tl = Cast<AZoogiBall>(Ta) ? Cast<AZoogiBall>(Ta)->BallLocation() : Cast<AZoogiOrb>(Ta) ? Cast<AZoogiOrb>(Ta)->OrbLocation() : Ta->GetActorLocation(); S2.To = Tl; } // homes on where it is now
				S2.T += Dt;
				if (S2.T < 0.f) continue; // a tracer still in the barrel
				if (!S2.bShown) { S2.bShown = true; if (AActor* C = S2.Carrier.Get()) C->SetActorHiddenInGame(false); if (S2.Style == 2) AZoogiFX::Spawn(W, S2.From, FLinearColor(1.f, 0.82f, 0.3f), 70.f, 0.1f, false, 16.f); }
				const float K = FMath::Clamp(S2.T / S2.Dur, 0.f, 1.f);
				const FVector Pk = FMath::Lerp(S2.From, S2.To, K);
				if (AActor* C = S2.Carrier.Get())
				{
					C->SetActorLocation(Pk);
					if (S2.Style == 6) C->SetActorScale3D(FVector(0.6f + 1.1f * K, 0.6f + 1.1f * K, 0.04f));
				}
				const double Tw = FPlatformTime::Seconds();
				if ((S2.Style == 3 || S2.Style == 5) && Tw >= S2.NextTrail)
				{	// plasma glow / rocket smoke behind the shot
					S2.NextTrail = Tw + 0.025;
					AZoogiFX::SpawnTrail(W, Pk, S2.Style == 3 ? FLinearColor(1.f, 0.3f, 0.9f) : FLinearColor(0.75f, 0.75f, 0.78f), S2.Style == 3 ? 38.f : 46.f, S2.Style == 3 ? 0.3f : 0.7f, S2.Style == 3 ? 6.f : 0.6f);
				}
				if (K < 1.f) continue;
				if (AActor* C = S2.Carrier.Get()) C->Destroy();
				const FVector Dir = (S2.To - S2.From).GetSafeNormal2D();
				switch (S2.Style)
				{
				case 2: AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.82f, 0.3f), 90.f, 0.18f, false, 12.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_Gold"), S2.To, FRotator::ZeroRotator, 0.35f); break;
				case 3: AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.3f, 0.9f), 220.f, 0.35f, true, 10.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), S2.To, FRotator::ZeroRotator, 0.6f); break;
				case 4: AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.3f, 0.3f), 160.f, 0.25f, false, 14.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_Red"), S2.To, FRotator::ZeroRotator, 0.45f); break;
				case 5: AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), S2.To, 0.55f); AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.55f, 0.15f), 240.f, 0.4f, true, 10.f); break;
				case 6: AZoogiFX::Spawn(W, S2.To, FLinearColor(0.3f, 0.9f, 1.f), 300.f, 0.4f, true, 10.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), S2.To, FRotator::ZeroRotator, 0.6f); break;
				default:
					AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), S2.To, 0.5f, 2.f, nullptr, FVector::ZeroVector, Dir.Rotation());
					AZoogiFX::Spawn(W, S2.To, FLinearColor(0.75f, 0.35f, 1.f), 180.f, 0.3f, true, 10.f);
					break;
				}
				ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), S2.Style == 2 ? 0.35f : 0.6f, S2.Style == 5 ? 0.9f : 1.4f);
				{ FTuneScope Tn3(S2.Pow, FName(TEXT("gattling_turret"))); Blast(G, S2.To - Dir * 70.f, 150.f, 950.f * S2.Push, 140.f * S2.Push, nullptr, S2.Owner); }
				TurSlashes.RemoveAt(i);'''),
('''					static float TurYaw = -1000.f; if (TurYaw < -999.f) { TurYaw = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretYaw="), TurYaw); }''',
'''					const float TurYaw = TurYawOff(); // b221'''),
# public API for the demo rig
('''	void TurretStart(AZoogiGameMode* G, AZoogiBall* Caster, const FVector& Spot)
	{''',
'''	AActor* TurretDemoSpawn(AZoogiGameMode* G, const FVector& Floor, float& OutTipFwd, float& OutTipUp)
	{	// b221: the turret model alone (no owner, no auto fire) for -ZoogiDemoTurret
		UWorld* W = G ? G->GetWorld() : nullptr; if (!W) return nullptr;
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/Turret/SM_Turret.SM_Turret"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) return nullptr;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Floor), SP);
		if (!A) return nullptr;
		A->SetMobility(EComponentMobility::Movable);
		const FBox Bb = M->GetBoundingBox(); const float Hh = FMath::Max(1.f, (float)Bb.GetSize().Z), Sc = 240.f / Hh;
		A->GetStaticMeshComponent()->SetStaticMesh(M); A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); A->GetStaticMeshComponent()->SetWorldScale3D(FVector(Sc));
		A->SetActorLocation(Floor - FVector(0, 0, Bb.Min.Z * Sc));
		OutTipFwd = FMath::Max(Bb.Max.Y, Bb.Max.X) * Sc + 10.f; OutTipUp = 0.62f * Hh * Sc;
		return A;
	}
	float TurretYawOffset() { return TurYawOff(); }
	float TurretShoot(AZoogiGameMode* G, AActor* Turret, const FVector& Floor, float TipFwd, float TipUp, const FVector& Target, int32 Style)
	{
		return BarrelShot(G, Turret, Floor, TipFwd, TipUp, Target, -1, 1.f, Style, nullptr);
	}
	void TurretShotsTick(AZoogiGameMode* G, float Dt) { if (G) TurSlashTick(G, Dt); }
	void TurretStart(AZoogiGameMode* G, AZoogiBall* Caster, const FVector& Spot)
	{'''),
])

patch('Public/ZoogiSkills.h', [
('''	/** b130: ITEM -> PRISM GRENADE''',
'''	/** b221: -ZoogiDemoTurret rig - the turret model alone, a barrel shot of Style (1-6, straight from the barrel tip), its flight tick */
	ZOOGIROLL_API AActor* TurretDemoSpawn(AZoogiGameMode* G, const FVector& Floor, float& OutTipFwd, float& OutTipUp);
	ZOOGIROLL_API float TurretYawOffset();
	ZOOGIROLL_API float TurretShoot(AZoogiGameMode* G, AActor* Turret, const FVector& Floor, float TipFwd, float TipUp, const FVector& Target, int32 Style);
	ZOOGIROLL_API void TurretShotsTick(AZoogiGameMode* G, float Dt);
	/** b130: ITEM -> PRISM GRENADE'''),
])

# ---------------------------------------------------------------- ZoogiDemoReel.cpp: -ZoogiDemoTurret (top view, 6 options)
patch('Private/ZoogiDemoReel.cpp', [
('#include "ZoogiFilm.h"\n', '#include "ZoogiFilm.h"\n#include "ZoogiSkills.h" // b221\n'),
('''	bool Holding() { static const bool bOn''',
'''	namespace
	{
		// b221 (-ZoogiDemoReel -ZoogiDemoTurret; Evan: "show me 6 top view options ... and confirm bullets shoot from the barrel"): the turret
		// on the cleared court seen from straight above, a Zoogi stand-in at 6 spots around it - it turns, fires 2 shots of one style per
		// spot, the target is knocked back when a shot lands. A film clip per option (NN_turret).
		struct FTu { bool bOn = false; int32 K = 0; double T0 = 0.0; TWeakObjectPtr<AActor> Tur; FVector At = FVector::ZeroVector; float TipF = 60.f, TipU = 110.f;
			float Yaw = 0.f; int32 Fired = 0; double HitAt[2] = { 1e9, 1e9 }; FVector TgtAt = FVector::ZeroVector, Knock = FVector::ZeroVector; double KnockT = -10.0; double LastT = 0.0; };
		FTu GTu;
		void TickTurret(AZoogiGameMode* G, UWorld* W, double Tn)
		{
			if (!GTu.bOn)
			{
				GTu.bOn = true; GTu.K = 0; GTu.T0 = Tn + 1.0; GTu.LastT = Tn;
				GTu.At = D.A0 + D.Dir * 750.f; GTu.At.Z = D.FloorZ;
				GTu.Tur = ZoogiSkills::TurretDemoSpawn(G, GTu.At, GTu.TipF, GTu.TipU);
				GTu.Yaw = D.Dir.Rotation().Yaw;
				if (AStaticMeshActor* Ma = D.MA.Get()) Ma->SetActorHiddenInGame(true);
				if (AStaticMeshActor* Mb = D.MB.Get()) Mb->SetActorHiddenInGame(true);
				if (AStaticMeshActor* Ro = D.Rock.Get()) Ro->SetActorHiddenInGame(true);
				UE_LOG(LogZoogi, Log, TEXT("DEMO turret at (%.0f, %.0f) tip fwd %.0f up %.0f, yaw offset %.0f"), GTu.At.X, GTu.At.Y, GTu.TipF, GTu.TipU, ZoogiSkills::TurretYawOffset());
			}
			const float Dt = FMath::Clamp((float)(Tn - GTu.LastT), 0.f, 0.1f); GTu.LastT = Tn;
			ZoogiSkills::TurretShotsTick(G, Dt);
			if (ACameraActor* Cm = D.Cam.Get())
			{	// straight down, the court's forward at the top of the screen
				static float CamH = -1.f; if (CamH < 0.f) { CamH = 1250.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiDemoTurretCamH="), CamH); }
				Cm->SetActorLocationAndRotation(GTu.At + FVector(0, 0, CamH), FRotator(-89.9f, D.Dir.Rotation().Yaw, 0.f));
			}
			const float t = (float)(Tn - GTu.T0);
			if (t < 0.f) return;
			if (t > 3.6f) { GTu.T0 = Tn; GTu.Fired = 0; GTu.HitAt[0] = GTu.HitAt[1] = 1e9; if (++GTu.K >= 6) { D.bDone = true; UE_LOG(LogZoogi, Log, TEXT("DEMO turret done")); return; } }
			const float u = (float)(Tn - GTu.T0);
			const float Ang = D.Dir.Rotation().Yaw + 35.f + 62.f * GTu.K; // a new spot for every option, so the turret turns each time
			GTu.TgtAt = GTu.At + FRotator(0.f, Ang, 0.f).Vector() * 640.f; GTu.TgtAt.Z = D.FloorZ + ZoogiConst::ZoogiRadius;
			if (u < 0.05f && GTu.Fired == 0) { ZoogiFilm::Begin(TEXT("turret"), 3.4f); ZoogiFilm::Mark(FString::Printf(TEXT("option %d"), GTu.K + 1)); }
			// turn the turret toward the target (~360 deg/s), its barrel = actor yaw - the model offset
			const float Want = Ang;
			GTu.Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(GTu.Yaw, Want), -360.f * Dt, 360.f * Dt);
			if (AActor* Tu = GTu.Tur.Get()) Tu->SetActorRotation(FRotator(0.f, GTu.Yaw + ZoogiSkills::TurretYawOffset(), 0.f));
			const float FireT[2] = { 0.8f, 1.9f };
			for (int32 s = 0; s < 2; ++s)
				if (GTu.Fired == s && u >= FireT[s] && FMath::Abs(FMath::FindDeltaAngleDegrees(GTu.Yaw, Want)) < 3.f)
				{
					const float Land = ZoogiSkills::TurretShoot(G, GTu.Tur.Get(), GTu.At, GTu.TipF, GTu.TipU, GTu.TgtAt, GTu.K + 1);
					GTu.HitAt[s] = Tn + Land; ++GTu.Fired;
					UE_LOG(LogZoogi, Log, TEXT("DEMO turret option %d shot %d (lands in %.2f s)"), GTu.K + 1, s + 1, Land);
				}
			for (int32 s = 0; s < 2; ++s) if (Tn >= GTu.HitAt[s]) { GTu.HitAt[s] = 1e9; GTu.KnockT = Tn; GTu.Knock = FRotator(0.f, Ang, 0.f).Vector(); }
			// the target: knocked back along the shot, then it rolls back to its spot
			const float Kt = (float)(Tn - GTu.KnockT);
			const float Kb = Kt >= 0.f && Kt < 0.9f ? FMath::Sin(FMath::Clamp(Kt / 0.9f, 0.f, 1.f) * PI) * 120.f : 0.f;
			if (AStaticMeshActor* B = D.Ball.Get()) { B->SetActorHiddenInGame(false); B->SetActorLocation(GTu.TgtAt + GTu.Knock * Kb + FVector(0, 0, Kb * 0.3f)); }
		}
	}

	bool Holding() { static const bool bOn'''),
('''		if (bBlasts) { if (APlayerController* Pc = W->GetFirstPlayerController())''',
'''		static const bool bTurretRig = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoTurret")); // b221
		if (bTurretRig) { if (APlayerController* Pc = W->GetFirstPlayerController()) if (D.Cam.IsValid() && Pc->GetViewTarget() != D.Cam.Get()) Pc->SetViewTarget(D.Cam.Get()); TickTurret(G, W, Tn); return; }
		if (bBlasts) { if (APlayerController* Pc = W->GetFirstPlayerController())'''),
('''			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoBlasts"))) ZoogiFilm::Begin(TEXT("demo"), 19.5f);''',
'''			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoBlasts")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDemoTurret"))) ZoogiFilm::Begin(TEXT("demo"), 19.5f);'''),
])
patch('Private/ZoogiGameMode.cpp', [
("""	if (bComboMode) { ComboCheckKnockoffs(); return; }
	for (AZoogiOrb* O : Orbs)""",
"""	if (bComboMode) { ComboCheckKnockoffs(); return; }
	{	// b221 preview only (-ZoogiItemKOTest): on P1's 2nd roll P2 is thrown off the edge as if P1 hit it, to show the item-KO crystals
		static const bool bKOTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiItemKOTest"));
		static bool bKOTestDone = false;
		if (bKOTest && !bKOTestDone && Phase == EZoogiPhase::Rolling && ActivePlayer == 0 && TurnCounter >= 3 && PhaseTime > 0.8f && Balls.IsValidIndex(1) && Balls[1] && !Balls[1]->bOut && Balls[1]->Sphere)
		{
			bKOTestDone = true;
			AZoogiBall* Kb = Balls[1];
			const FVector Lk = Kb->BallLocation(); const FVector Out = FVector(Lk.X, Lk.Y, 0.f).GetSafeNormal();
			Kb->LastHitPlayer = 0;
			Kb->Sphere->SetWorldLocation(FVector(0.f, 0.f, Lk.Z) + (Out.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Out) * 4200.f - FVector(0.f, 0.f, 1500.f), false, nullptr, ETeleportType::TeleportPhysics);
			Kb->Sphere->SetPhysicsLinearVelocity(FVector(0.f, 0.f, -600.f));
			UE_LOG(LogZoogi, Log, TEXT("ITEM KO TEST: P2 thrown off as if hit by P1"));
		}
	}
	for (AZoogiOrb* O : Orbs)"""),
])
print('patched')
