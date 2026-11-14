# b223 (Claude): turret = 10 shots, ammo picked in the inventory (not the slash), targets nearest orb / rival / (raid) boss;
# GATTLING RAPID = 10 dashes on SUPERPOWER toward the last tap (no pause, no REACTION pick)
import os, sys
SRC = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(path, pairs):
    p = os.path.join(SRC, path); s = open(p).read()
    for a, b in pairs:
        assert a in s, (path, a[:100]); s = s.replace(a, b, 1)
    open(p, 'w').write(s)

patch('Private/ZoogiSkills.cpp', [
('#include "ZoogiHaptics.h"\n', '#include "ZoogiHaptics.h"\n#include "ZoogiFrontProfile.h" // b223: the turret AMMO pick\n#include "ZoogiClassicPad.h" // b223\n#include "ZoogiRaid.h" // b223\n'),
('struct FTurret { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector; int32 Owner = -1; int32 EndTurn = 0; double NextShot = 0.0; int32 Shots = 0;',
 'struct FTurret { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector; int32 Owner = -1; int32 EndTurn = 0; double NextShot = 0.0; int32 Shots = 0; int32 ShotsLeft = 10; /* b223 */'),
('int32 Style = 1; float Push = 1.f; bool bShown = false; double NextTrail = 0.0; };',
 'int32 Style = 1; float Push = 1.f; bool bShown = false; double NextTrail = 0.0; bool bBoss = false; /* b223 */ };'),
# ammo per owner (inventory pick for the human, rocket for CPUs)
('''		float TurYawOff() {''',
'''		// b223 (Evan: "in inventory for turret only create a separate edit for selecting ammo type ... all options except slash"): the human's
		// turret fires the AMMO picked in the inventory (profile key turret_ammo = 2..6), CPUs fire rockets; -ZoogiTurretShot= forces one
		int32 TurretAmmoFor(AZoogiGameMode* G, int32 Owner)
		{
			static int32 Forced = -2; if (Forced == -2) { Forced = -1; FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretShot="), Forced); }
			if (Forced >= 1) return FMath::Clamp(Forced, 1, 6);
			const AZoogiBall* Ob = G && G->Balls.IsValidIndex(Owner) ? G->Balls[Owner].Get() : nullptr;
			if (Ob && !Ob->bCPU) if (UZoogiFrontProfileSubsystem* Prof = UZoogiFrontProfileSubsystem::Get(G)) { const int32 Am = Prof->Data().ItemCounts.FindRef(FName(TEXT("turret_ammo"))); if (Am >= 2 && Am <= 6) return Am; }
			return 5;
		}
		float TurYawOff() {'''),
# GATTLING RAPID dashes
('''		void DropTurrets() { for (FTurret& T : Turrets) if (AActor* A = T.A.Get()) A->Destroy(); Turrets.Reset(); GatRuns.Reset();''',
'''		// b223 (Evan: "for Gatling rapid instead of pausing like reaction and choosing like reaction we transition to tapping the super power
		// button to make the player dash toward the last direction the user taps on the screen"): 10 dashes, no pause - tap the court to
		// set where, SUPERPOWER = dash there (-ZoogiRapidReaction = the b200 REACTION version)
		struct FDashRun { TWeakObjectPtr<AZoogiBall> B; int32 Left = 10; bool bCpu = false, bRolled = false, bHasTap = false, bPrevDown = false; FVector Tap = FVector::ZeroVector; double T0 = 0.0, NextCpu = 0.0; };
		TArray<FDashRun> DashRuns;
		void DashOnce(AZoogiGameMode* G, FDashRun& R, const FVector& Toward)
		{
			AZoogiBall* Db = R.B.Get(); if (!Db || !Db->Sphere) return;
			const FVector Bl = Db->BallLocation();
			FVector Dir = (Toward - Bl).GetSafeNormal2D();
			if (Dir.IsNearlyZero()) Dir = Db->GetBallVelocity().GetSafeNormal2D();
			if (Dir.IsNearlyZero()) Dir = FRotator(0.f, Db->AimYaw, 0.f).Vector();
			static float DashSpeed = -1.f; if (DashSpeed < 0.f) { DashSpeed = 2000.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRapidDashSpeed="), DashSpeed); }
			const float Sp = FMath::Max(DashSpeed, (float)Db->GetBallVelocity().Size2D());
			Db->Sphere->WakeAllRigidBodies();
			Db->Sphere->SetPhysicsLinearVelocity(Dir * Sp + FVector(0.f, 0.f, 40.f));
			--R.Left;
			UWorld* W = G->GetWorld();
			AZoogiFX::Spawn(W, Bl, GatGold, 160.f, 0.25f, true, 10.f);
			for (int32 k = 1; k <= 5; ++k) AZoogiFX::SpawnTrail(W, Bl - Dir * (k * 40.f), GatGold, 40.f, 0.35f, 6.f);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.7f, 1.6f);
			if (!R.bCpu) ZoogiHaptics::Kick(0.4f, W, TEXT("rapid dash"));
			UE_LOG(LogZoogi, Log, TEXT("RAPID DASH P%d toward (%.0f, %.0f) at %.0f cm/s - %d left"), Db->PlayerIndex + 1, Toward.X, Toward.Y, Sp, R.Left);
		}
		void DropTurrets() { DashRuns.Reset(); for (FTurret& T : Turrets) if (AActor* A = T.A.Get()) A->Destroy(); Turrets.Reset(); GatRuns.Reset();'''),
('''		// rivals first, then orbs, nearest first, inside Range
''',
 '''		void DashTick(AZoogiGameMode* G)
		{
			UWorld* W = G->GetWorld(); const double Tn = FPlatformTime::Seconds();
			for (int32 i = DashRuns.Num() - 1; i >= 0; --i)
			{
				FDashRun& R = DashRuns[i]; AZoogiBall* Db = R.B.Get();
				const bool bMine = Db && G->GetActiveBall() == Db && G->Phase == EZoogiPhase::Rolling;
				if (bMine) R.bRolled = true;
				if (!Db || Db->bOut || R.Left <= 0 || (R.bRolled && !bMine) || Tn - R.T0 > 40.0)
				{
					UE_LOG(LogZoogi, Log, TEXT("RAPID DASH run ends (%d left)"), R.Left);
					DashRuns.RemoveAt(i); continue;
				}
				if (!bMine) continue;
				if (R.bCpu)
				{	// CPU: three dashes at the nearest rival, 0.8 s apart
					if (R.Left > 7 && Tn >= R.NextCpu)
					{
						R.NextCpu = Tn + 0.8;
						const AZoogiBall* Near = nullptr; float Nd = 1e9f;
						for (AZoogiBall* Ob : G->Balls) if (Ob && Ob != Db && !Ob->bOut && !G->IsAlly(Db, Ob)) { const float Dd = FVector::Dist2D(Ob->BallLocation(), Db->BallLocation()); if (Dd < Nd) { Nd = Dd; Near = Ob; } }
						if (Near) DashOnce(G, R, Near->BallLocation());
					}
					continue;
				}
				// the human: remember the last tap on the court (not the pad buttons)
				const bool bDown = GatTapNow(W);
				if (bDown && !R.bPrevDown && !ZoogiClassicPad::PointerOwned())
				{
					FVector Tp;
					if (RapidTapPoint(G, Db->BallLocation().Z - ZoogiConst::ZoogiRadius, Tp))
					{
						R.Tap = Tp; R.bHasTap = true;
						AZoogiFX::Spawn(W, Tp + FVector(0.f, 0.f, 5.f), GatGold, 90.f, 0.35f, true, 8.f);
					}
				}
				R.bPrevDown = bDown;
			}
		}
		void DashStart(AZoogiGameMode* G, AZoogiBall* Caster)
		{
			FDashRun R; R.B = Caster; R.bCpu = Caster->bCPU; R.T0 = FPlatformTime::Seconds(); R.NextCpu = R.T0 + 0.4;
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiRapidDashes="), R.Left);
			DashRuns.RemoveAll([Caster](const FDashRun& X) { return X.B.Get() == Caster; });
			DashRuns.Add(R);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (!Caster->bCPU) F->PublicCallout(TEXT("GATTLING RAPID!"), FString::Printf(TEXT("TAP WHERE TO GO - SUPERPOWER DASHES THERE  x%d"), R.Left), GatGold, 1.2f, 0.9f);
			AZoogiFX::Spawn(G->GetWorld(), Caster->BallLocation(), GatGold, 180.f, 0.4f, true, 10.f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 0.7f);
			UE_LOG(LogZoogi, Log, TEXT("SKILL gattling rapid P%d: %d dashes on SUPERPOWER"), Caster->PlayerIndex + 1, R.Left);
		}
		// rivals first, then orbs, nearest first, inside Range
'''),
# BarrelShot: the boss as a target (raid)
('''		float BarrelShot(AZoogiGameMode* G, AActor* Tur, const FVector& Base, float TipFwd, float TipUp, const FVector& Tp, int32 Owner, float Pow, int32 Style, AActor* Target)''',
 '''		float BarrelShot(AZoogiGameMode* G, AActor* Tur, const FVector& Base, float TipFwd, float TipUp, const FVector& Tp, int32 Owner, float Pow, int32 Style, AActor* Target, bool bBoss = false)'''),
('''			const float Dist = FMath::Clamp((float)FVector::Dist2D(From, Tp), 150.f, 1400.f);''',
 '''			const float Dist = FMath::Clamp((float)FVector::Dist2D(From, Tp), 150.f, bBoss ? 3500.f : 1400.f); // b223: the raid boss can be far'''),
('''S2.Style = Style; S2.Push = Push;''', '''S2.Style = Style; S2.Push = Push; S2.bBoss = bBoss;'''),
('''		void TurretSlash(AZoogiGameMode* G, const FTurret& T, AActor* Target, const FVector& Tp)
		{''', '''		void TurretSlash(AZoogiGameMode* G, const FTurret& T, AActor* Target, const FVector& Tp, bool bBoss = false)
		{'''),
('''BarrelShot(G, A, T.P, T.TipFwd, T.TipUp, Tp, T.Owner, T.Pow, TurretStyleNow(), Target); return; } // b221''',
 '''BarrelShot(G, A, T.P, T.TipFwd, T.TipUp, Tp, T.Owner, T.Pow, TurretAmmoFor(G, T.Owner), Target, bBoss); return; } // b221; b223 ammo + boss'''),
('''				{ FTuneScope Tn3(S2.Pow, FName(TEXT("gattling_turret"))); Blast(G, S2.To - Dir * 70.f, 150.f, 950.f * S2.Push, 140.f * S2.Push, nullptr, S2.Owner); }''',
 '''				if (S2.bBoss) { if (S2.Push >= 0.7f) { ZoogiCosmosBoss::Flinch(); ZoogiBossAttack::HitBoss(G, S2.Owner); } } // b223: a raid boss hit
				else { FTuneScope Tn3(S2.Pow, FName(TEXT("gattling_turret"))); Blast(G, S2.To - Dir * 70.f, 150.f, 950.f * S2.Push, 140.f * S2.Push, nullptr, S2.Owner); }'''),
# GatTick: dashes; turret ammo count + boss target
('''			TurSlashTick(G, FMath::Min(G->GetWorld() ? G->GetWorld()->GetDeltaSeconds() : 0.016f, 0.05f)); // b202''',
 '''			TurSlashTick(G, FMath::Min(G->GetWorld() ? G->GetWorld()->GetDeltaSeconds() : 0.016f, 0.05f)); // b202
			DashTick(G); // b223'''),
('''				AActor* A = T.A.Get();
				if (!A || G->TurnCounter >= T.EndTurn)''',
 '''				AActor* A = T.A.Get();
				bool bOutOfAmmo = false; // b223 (Evan: "Gatling turrets only get 10 shots"): gone after its 10th shot lands
				if (A && T.ShotsLeft <= 0) { bool bFly = false; for (const FTurSlash& Ts : TurSlashes) if (Ts.Turret.Get() == A) bFly = true; bOutOfAmmo = !bFly && Tn >= T.NextShot + 0.6; }
				if (!A || G->TurnCounter >= T.EndTurn || bOutOfAmmo)'''),
('''					for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), T.P); if (D < Bd) { Bd = D; Best = O; } }
					if (!Best) continue;
					const FVector Tp = Cast<AZoogiBall>(Best) ? Cast<AZoogiBall>(Best)->BallLocation() : Cast<AZoogiOrb>(Best) ? Cast<AZoogiOrb>(Best)->OrbLocation() : Best->GetActorLocation();''',
 '''					for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), T.P); if (D < Bd) { Bd = D; Best = O; } }
					// b223 (Evan: "turrets also shoot at nearest orbs, opposing player and boss on raid only"): in a raid the boss is a target too
					bool bBossT = false; FVector BossP = FVector::ZeroVector;
					if (ZoogiRaid::Active() && ZoogiBossAttack::LockOnTarget(G, BossP)) { const float Dbs = FVector::Dist2D(BossP, T.P); if (Dbs < 2600.f && (!Best || Dbs < Bd)) { bBossT = true; Best = nullptr; Bd = Dbs; } }
					if (!Best && !bBossT) continue;
					const FVector Tp = bBossT ? BossP : Cast<AZoogiBall>(Best) ? Cast<AZoogiBall>(Best)->BallLocation() : Cast<AZoogiOrb>(Best) ? Cast<AZoogiOrb>(Best)->OrbLocation() : Best->GetActorLocation();'''),
('''					if (bInFlight || G->Phase != EZoogiPhase::Rolling || Tn < T.NextShot || T.Shots >= 8 || FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantYaw)) > 20.f) continue;
					T.NextShot = Tn + 0.35; ++T.Shots;
					T.KickAt = Tn; T.KickDir = (Tp - T.P).GetSafeNormal2D();
					TurretSlash(G, T, Best, Tp);''',
 '''					if (bInFlight || G->Phase != EZoogiPhase::Rolling || Tn < T.NextShot || T.ShotsLeft <= 0 || FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantYaw)) > 20.f) continue;
					T.NextShot = Tn + 0.35; ++T.Shots; --T.ShotsLeft; // b223: 10 shots in all
					T.KickAt = Tn; T.KickDir = (Tp - T.P).GetSafeNormal2D();
					TurretSlash(G, T, Best, Tp, bBossT);
					UE_LOG(LogZoogi, Log, TEXT("TURRET of P%d fires at %s - %d shots left"), T.Owner + 1, bBossT ? TEXT("the BOSS") : *GetNameSafe(Best), T.ShotsLeft);'''),
('''		T.EndTurn = G->TurnCounter + 2 * FMath::Max(1, G->Balls.Num()); // two of the owner's turns''',
 '''		T.EndTurn = G->TurnCounter + 5 * FMath::Max(1, G->Balls.Num()); // b223: it stays until its 10 shots are used (5 of the owner's turns at most)'''),
('''		G->PublicPopup(TEXT("TURRET UP - 2 TURNS"), P + FVector(0, 0, 200.f), GatGold);''',
 '''		G->PublicPopup(TEXT("TURRET UP - 10 SHOTS"), P + FVector(0, 0, 200.f), GatGold); // b223'''),
# GatStart: RAPID = dashes
('''		static const bool bRapidBolts = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidBolts"));''',
 '''		static const bool bRapidBolts = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidBolts"));
		static const bool bRapidReaction = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidReaction")); // b223: the b200 version
		if (Kind == 1 && !bRapidBolts && !bRapidReaction) { DashStart(G, Caster); return; } // b223: 10 SUPERPOWER dashes'''),
# public API
('''	bool RapidActive(int32* OutShots, float* OutLeft)''',
 '''	bool DashActive(int32* OutLeft)
	{	// b223: the human's GATTLING RAPID dashes
		for (const FDashRun& R : DashRuns) if (!R.bCpu && R.B.IsValid() && R.Left > 0) { if (OutLeft) *OutLeft = R.Left; return true; }
		return false;
	}
	bool DashGo(AZoogiGameMode* G)
	{
		for (FDashRun& R : DashRuns)
			if (!R.bCpu && R.B.IsValid() && R.Left > 0)
			{
				AZoogiBall* Db = R.B.Get();
				if (G->Phase != EZoogiPhase::Rolling || G->GetActiveBall() != Db) { G->PublicPopup(TEXT("LAUNCH, THEN SUPERPOWER TO DASH"), Db->BallLocation() + FVector(0, 0, 190.f), GatGold); return true; }
				DashOnce(G, R, R.bHasTap ? R.Tap : Db->BallLocation() + Db->GetBallVelocity().GetSafeNormal2D() * 500.f);
				return true;
			}
		return false;
	}
	bool RapidActive(int32* OutShots, float* OutLeft)'''),
])

patch('Public/ZoogiSkills.h', [
('''	ZOOGIROLL_API bool RapidActive(''',
 '''	/** b223: GATTLING RAPID = dashes - the human has some left (OutLeft); DashGo = SUPERPOWER: dash toward the last court tap */
	ZOOGIROLL_API bool DashActive(int32* OutLeft = nullptr);
	ZOOGIROLL_API bool DashGo(AZoogiGameMode* G);
	ZOOGIROLL_API bool RapidActive('''),
])

patch('Private/ZoogiClassicPad.cpp', [
('''				if (ZoogiSkills::RapidActive()) return FReply::Handled().CaptureMouse(SharedThis(this)); // b199: during GATTLING RAPID a press is just a bolt''',
 '''				if (bPower && ZoogiSkills::DashActive()) { ZoogiSkills::DashGo(G); return FReply::Handled().CaptureMouse(SharedThis(this)); } // b223: GATTLING RAPID dash
				if (ZoogiSkills::RapidActive()) return FReply::Handled().CaptureMouse(SharedThis(this)); // b199: during GATTLING RAPID a press is just a bolt'''),
('''			int32 RapidN = 0; const bool bRapidNow = bPower && ZoogiSkills::RapidActive(&RapidN); // b199
			if (bRapidNow) Label(''',
 '''			int32 RapidN = 0; const bool bRapidNow = bPower && ZoogiSkills::RapidActive(&RapidN); // b199
			int32 DashN = 0; const bool bDashNow = bPower && ZoogiSkills::DashActive(&DashN); // b223
			if (bDashNow) Label(FString::Printf(TEXT("DASH x%d!"), DashN), FMath::Max(8.f, R0 * 0.3f), FVector2D(Sz.X * 0.5f, C.Y + R0 * 1.3f), FLinearColor::FromSRGBColor(FColor(255, 214, 74)), Layer + 5);
			else if (bRapidNow) Label('''),
('''				if (ZoogiSkills::RapidActive(&Rn, &Rl))
				{
					const FLinearColor Yel''',
 '''				if (ZoogiSkills::RapidActive(&Rn, &Rl) || ZoogiSkills::DashActive(&Rn)) // b223: + the dash count
				{
					const FLinearColor Yel'''),
])

patch('Private/ZoogiPlayerController.cpp', [
('''	if (ZoogiSkills::RapidActive()) { HoverTarget.Reset(); return; } // b197: GATTLING RAPID owns the taps''',
 '''	if (ZoogiSkills::RapidActive() || ZoogiSkills::DashActive()) { HoverTarget.Reset(); return; } // b197: GATTLING RAPID owns the taps (b223: the dash taps too)'''),
])

patch('Private/ZoogiFront.cpp', [
('''TEXT("Drop your turret anywhere in reach: it turns to the nearest rival or orb and fires slashes from its barrel, one at a time, for 2 of your turns."), 3 },''',
 '''TEXT("Drop your turret anywhere in reach: it turns to the nearest orb or rival (and the boss in a RAID) and fires 10 shots straight from its barrel. Pick its AMMO below."), 3 },'''),
('''		// buttons
		const float BY = TL.Y + Sz.Y - 100.f;''',
 '''		if (I.Kind == 2 && I.WeaponKey == TEXT("gatling"))
		{	// b223 (Evan: "in inventory for turret only create a separate edit for selecting ammo type with the option to choose from all
			// options above except slash"): the turret's AMMO - saved in the profile (turret_ammo), used in every match
			struct FAm { int32 S; const TCHAR* N; };
			static const FAm Am[] = { { 5, TEXT("ROCKET") }, { 2, TEXT("TRACERS") }, { 3, TEXT("PLASMA") }, { 4, TEXT("LASER") }, { 6, TEXT("RING") } };
			const UZoogiFrontProfileSubsystem* Pa = FP();
			int32 CurAm = Pa ? Pa->Data().ItemCounts.FindRef(FName(TEXT("turret_ammo"))) : 0; if (CurAm < 2 || CurAm > 6) CurAm = 5;
			const FVector2D A0 = TL + FVector2D(30.f, 420.f);
			ZTextAt(Out, G, L + 3, TEXT("AMMO"), Font(0, 30.f), A0, Ink);
			for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(Am); ++k)
			{
				const bool bOn = Am[k].S == CurAm; const int32 Sv = Am[k].S; const FString Nm = Am[k].N;
				Slab(Out, G, L + 5, Nm, FVector2D(TL.X + 200.f + k * 160.f, A0.Y - 6.f), FVector2D(148.f, 60.f), bOn ? Mag() : Ink, bOn ? Ink : Ink.CopyWithNewOpacity(0.4f), 26.f,
					[Self, Sv, Nm]() { if (UZoogiFrontProfileSubsystem* Pw = Self->FP()) { Pw->Mutable().ItemCounts.FindOrAdd(FName(TEXT("turret_ammo"))) = Sv; Pw->Save(); } Self->Sfx(TEXT("UiConfirm"), 0.8f, 1.2f); Self->Say(FString::Printf(TEXT("TURRET AMMO: %s"), *Nm)); });
			}
		}
		// buttons
		const float BY = TL.Y + Sz.Y - 100.f;'''),
('''			{ TEXT("B222"),''',
 '''			{ TEXT("B223"), TEXT("TURRET AMMO + RAPID DASH"), TEXT("The Gatling Turret gets 10 shots and goes after the nearest orb or rival, and the boss in a RAID. Pick its AMMO in the inventory (tap the Gatling Turret): ROCKET, TRACERS, PLASMA, LASER or RING. GATTLING RAPID no longer pauses the game: tap where you want to go, then press SUPERPOWER to dash there, 10 times.") },
			{ TEXT("B222"),'''),
])
print('patched b223')
