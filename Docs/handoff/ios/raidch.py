import os
os.chdir(os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll'))
def rep(p, a, b, cnt=1):
    s = open(p, encoding='utf-8').read()
    assert s.count(a) == cnt, (p, a[:70], s.count(a))
    s = s.replace(a, b); open(p, 'w', encoding='utf-8').write(s)

# ---------------------------------------------------------------- ZoogiBossAttack: pending zone / interrupt
rep('Public/ZoogiBossAttack.h', '	ZOOGIROLL_API bool LockOnTarget(const AZoogiGameMode* G, FVector& OutP);\n',
'''	ZOOGIROLL_API bool LockOnTarget(const AZoogiGameMode* G, FVector& OutP);
	/** b200 raid challenges: a big boss attack is announced and not landed yet */
	ZOOGIROLL_API bool Pending(const AZoogiGameMode* G);
	/** b200: P is inside the announced attack's red zone */
	ZOOGIROLL_API bool InDangerZone(const AZoogiGameMode* G, const FVector& P);
	/** b200 INTERRUPT: the announced attack is cancelled */
	ZOOGIROLL_API void Interrupt(AZoogiGameMode* G, int32 ByPlayer);
''')
rep('Private/ZoogiBossAttack.cpp', '	bool CamActive() { return S.Cam.IsValid(); }\n',
'''	bool CamActive() { return S.Cam.IsValid(); }
	bool Pending(const AZoogiGameMode* G) { return G && S.G.Get() == G && S.bPending && !S.bMini && S.Run == ERun::Idle; }
	bool InDangerZone(const AZoogiGameMode* G, const FVector& P)
	{
		if (!Pending(G)) return false;
		if (S.Kind == EKind::Sweep) return InBand(G, P) && FMath::Abs(FMath::FindDeltaAngleDegrees(S.Ang, AngOf(G, P))) <= HalfW() + 8.f;
		return FVector::Dist2D(P, S.At) <= RingR(2);
	}
	void Interrupt(AZoogiGameMode* G, int32 ByPlayer)
	{
		if (!Pending(G)) return;
		S.bPending = false; S.InZone.Reset();
		ZoogiCosmosBoss::Sweep(false, 0.f);
		ZoogiCosmosBoss::Flinch();
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("INTERRUPTED!"), FString::Printf(TEXT("P%d STOPPED THE %s'S ATTACK"), ByPlayer + 1, *ZoogiCosmosBoss::Name()), FLinearColor(0.35f, 0.95f, 0.55f), 2.f, 1.2f);
		ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 1.4f);
		UE_LOG(LogZoogi, Log, TEXT("BOSS attack of round %d INTERRUPTED by P%d"), S.Round, ByPlayer + 1);
	}
''')

# ---------------------------------------------------------------- ZoogiRaid: hit hook, armor shards, exposed x2, bonus damage
rep('Public/ZoogiRaid.h', '	ZOOGIROLL_API float Rage();\n',
'''	ZOOGIROLL_API float Rage();
	/** b200 raid challenges */
	ZOOGIROLL_API void BonusDamage(AZoogiGameMode* G, int32 Player, int32 Amt, const TCHAR* What);
	ZOOGIROLL_API void SpawnArmor(AZoogiGameMode* G);   // ARMOR BREAK: 2 ice shards guard the weak point this turn
	ZOOGIROLL_API void ClearArmor();
	ZOOGIROLL_API int32 ArmorBroken();
	ZOOGIROLL_API void Expose(AZoogiGameMode* G, int32 Turns); // the weak point takes x2 for the next N turns
''')
R = 'Private/ZoogiRaid.cpp'
rep(R, '#include "ZoogiGattling.h"\n', '#include "ZoogiGattling.h"\n#include "ZoogiBounty.h"\n')
rep(R, '			TSet<TWeakObjectPtr<AZoogiBall>> OutSeen;\n		};',
'''			TSet<TWeakObjectPtr<AZoogiBall>> OutSeen;
			TArray<TWeakObjectPtr<AStaticMeshActor>> Armor; int32 ArmorDown = 0; int32 ExposeUntil = -1; // b200
		};''')
rep(R, '''		void Damage(AZoogiGameMode* G, int32 Player, int32 Amt, const FVector& At, const TCHAR* What)
		{
			if (Amt <= 0 || S.bEnding) return;''', '''		void Damage(AZoogiGameMode* G, int32 Player, int32 Amt, const FVector& At, const TCHAR* What)
		{
			if (Amt <= 0 || S.bEnding) return;
			if (S.ExposeUntil >= G->TurnCounter) { Amt *= 2; G->PublicPopup(TEXT("EXPOSED x2"), At + FVector(0, 0, 260.f), FLinearColor(0.55f, 0.9f, 1.f)); } // b200 ARMOR BREAK
			ZoogiBounty::OnRaidHit(G, Player, What, At); // b200 raid challenges''')
# a hit by the shooter's own Zoogi tells the bounty it was a Zoogi slam (What == "Zoogi slam") - no change needed
rep(R, '''		void End(AZoogiGameMode* G)
		{''', '''		void DropArmor() { for (TWeakObjectPtr<AStaticMeshActor>& A : S.Armor) if (AStaticMeshActor* Ac = A.Get()) Ac->Destroy(); S.Armor.Reset(); }
		void End(AZoogiGameMode* G)
		{
			DropArmor(); S.ExposeUntil = -1;''')
rep(R, '''	void OnAwaken(AZoogiGameMode* G, AZoogiBall* B)''', '''	void BonusDamage(AZoogiGameMode* G, int32 Player, int32 Amt, const TCHAR* What) { if (S.bOn && G) Damage(G, Player, Amt, HandPos(G), What); }
	void ClearArmor() { DropArmor(); }
	int32 ArmorBroken() { return S.ArmorDown; }
	void Expose(AZoogiGameMode* G, int32 Turns) { if (S.bOn && G) { S.ExposeUntil = G->TurnCounter + Turns; UE_LOG(LogZoogi, Log, TEXT("RAID: weak point EXPOSED (x2) until turn %d"), S.ExposeUntil); } }
	void SpawnArmor(AZoogiGameMode* G)
	{	// b200 ARMOR BREAK: two ice shards on the court in front of the weak point - smash both (your Zoogi or an orb)
		DropArmor(); S.ArmorDown = 0;
		if (!S.bOn || !G || !G->GetWorld()) return;
		const FVector Hp = HandPos(G), C = Ctr(G);
		const FVector In = FVector(C.X - Hp.X, C.Y - Hp.Y, 0.f).GetSafeNormal(), Side(-In.Y, In.X, 0.f);
		UStaticMesh* Sm = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/IceAttack/IceAttack/Models/SM_IceSpikes01_Spike01.SM_IceSpikes01_Spike01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (int32 k = -1; k <= 1; k += 2)
		{
			const FVector P = Hp + In * 330.f + Side * (k * 260.f) - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f);
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = G->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(P), SP);
			if (!A) continue;
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			M->SetStaticMesh(Sm ? Sm : ZoogiUtil::BasicShape(TEXT("Cone")));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			const FBox Bb = M->GetStaticMesh() ? M->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50), FVector(50));
			A->SetActorScale3D(FVector(150.f / FMath::Max(1.f, (float)Bb.GetSize().Z))); // ~1.5 m
			M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.45f, 0.85f, 1.f), 3.f));
			S.Armor.Add(A);
		}
		UE_LOG(LogZoogi, Log, TEXT("RAID: ARMOR BREAK - %d ice shards guard the weak point"), S.Armor.Num());
	}

	void OnAwaken(AZoogiGameMode* G, AZoogiBall* B)''')
# armor hit test in Tick (next to the orb hits)
rep(R, '''		// scoreboard = damage; team lives
		for (int32 i = 0; i < G->Balls.Num(); ++i)''', '''		for (int32 k = S.Armor.Num() - 1; k >= 0; --k)
		{	// b200 ARMOR BREAK: a Zoogi or an orb rolling into a shard smashes it
			AStaticMeshActor* A = S.Armor[k].Get(); if (!A) { S.Armor.RemoveAt(k); continue; }
			const FVector Ap = A->GetActorLocation();
			bool bHit = false;
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), Ap) < 110.f && B->GetBallVelocity().Size() > 150.f) { bHit = true; break; }
			if (!bHit) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->Sphere && FVector::Dist2D(O->OrbLocation(), Ap) < 95.f && O->Sphere->GetPhysicsLinearVelocity().Size() > 150.f) { bHit = true; break; }
			if (!bHit) continue;
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), Ap + FVector(0, 0, 60.f), FRotator::ZeroRotator, 1.3f);
			AZoogiFX::Spawn(W, Ap + FVector(0, 0, 40.f), FLinearColor(0.55f, 0.9f, 1.f), 220.f, 0.4f, true, 12.f);
			ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.9f, 1.3f);
			++S.ArmorDown; A->Destroy(); S.Armor.RemoveAt(k);
			Popup(G, FString::Printf(TEXT("SHARD %d / 2"), S.ArmorDown), Ap, FLinearColor(0.55f, 0.9f, 1.f));
			UE_LOG(LogZoogi, Log, TEXT("RAID: armor shard smashed (%d)"), S.ArmorDown);
		}
		// scoreboard = damage; team lives
		for (int32 i = 0; i < G->Balls.Num(); ++i)''')

# ---------------------------------------------------------------- ZoogiBounty: the raid challenges
rep('Public/ZoogiBounty.h', '	ZOOGIROLL_API void Reset();\n', '	ZOOGIROLL_API void Reset();\n	/** b200: the raid weak point took a hit (ZoogiRaid::Damage) */\n	ZOOGIROLL_API void OnRaidHit(AZoogiGameMode* G, int32 Player, const TCHAR* What, const FVector& At);\n')
B = 'Private/ZoogiBounty.cpp'
rep(B, '#include "ZoogiHaptics.h"\n', '#include "ZoogiHaptics.h"\n#include "ZoogiRaid.h"\n#include "ZoogiBossAttack.h"\n#include "ZoogiCardSubsystem.h"\n#include "ZoogiCosmosBoss.h"\n')
rep(B, 'enum class ECh : uint8 { None, TwoOrbs, TwoRivals, RivalOff, OrbThenRival, ThreeOrbs, BountyAndOrb };',
       'enum class ECh : uint8 { None, TwoOrbs, TwoRivals, RivalOff, OrbThenRival, ThreeOrbs, BountyAndOrb, RaidDouble, RaidStar, RaidBank, RaidInterrupt, RaidArmor, RaidSafe };')
rep(B, '			float CardAge = 0.f;\n		};', '''			float CardAge = 0.f;
			// b200 raid challenges
			int32 RaidHits = 0; bool bStarHit = false, bBank = false, bInterrupted = false; FVector LaunchP = FVector::ZeroVector, LaunchD = FVector::ZeroVector;
		};''')
rep(B, '''			case ECh::BountyAndOrb: return TEXT("HIT THE BOUNTY FIRST + SCORE AN ORB");
			default: return FString();''', '''			case ECh::BountyAndOrb: return TEXT("HIT THE BOUNTY FIRST + SCORE AN ORB");
			case ECh::RaidDouble: return TEXT("DOUBLE TAP: HIT THE WEAK POINT TWICE IN ONE ROLL");
			case ECh::RaidStar: return TEXT("STAR STRIKE: KNOCK A GOLD STAR ORB INTO THE WEAK POINT");
			case ECh::RaidBank: return TEXT("BANK SHOT: BOUNCE OFF SOMETHING, THEN HIT THE WEAK POINT");
			case ECh::RaidInterrupt: return TEXT("INTERRUPT: HIT THE WEAK POINT BEFORE ITS ATTACK LANDS");
			case ECh::RaidArmor: return TEXT("ARMOR BREAK: SMASH BOTH ICE SHARDS");
			case ECh::RaidSafe: return TEXT("SAFE ZONE: HIT THE WEAK POINT + STOP OUTSIDE THE RED ZONE");
			default: return FString();''')
rep(B, '''			case ECh::BountyAndOrb: return S.bBountyDone && S.OrbsOut >= 1;
			default: return false;''', '''			case ECh::BountyAndOrb: return S.bBountyDone && S.OrbsOut >= 1;
			case ECh::RaidDouble: return S.RaidHits >= 2;
			case ECh::RaidStar: return S.bStarHit;
			case ECh::RaidBank: return S.bBank;
			case ECh::RaidInterrupt: return S.bInterrupted;
			case ECh::RaidArmor: return ZoogiRaid::ArmorBroken() >= 2;
			case ECh::RaidSafe: { const AZoogiGameMode* G = S.G.Get(); const AZoogiBall* Me = G && G->Balls.IsValidIndex(S.Shooter) ? G->Balls[S.Shooter].Get() : nullptr;
				return G && Me && !Me->bOut && S.RaidHits >= 1 && G->Phase == EZoogiPhase::Resolving && !ZoogiBossAttack::InDangerZone(G, Me->BallLocation()); }
			default: return false;''')
# raid rewards
rep(B, '''		void Finish(AZoogiGameMode* G)
		{''', '''		bool IsRaid(ECh C) { return C >= ECh::RaidDouble; }
		void RaidReward(AZoogiGameMode* G, AZoogiBall* B)
		{	// b200 (Evan: "add all those suggested raid challenges"): each one pays off in the raid itself
			if (!G || !B) return;
			const int32 Pi = B->PlayerIndex; FString Sub;
			auto Sp = [&](int32 N) { if (UZoogiCardSubsystem* Cs = UZoogiCardSubsystem::Get(G)) Cs->AddEnergyNow(Pi, N); };
			switch (S.Ch)
			{
			case ECh::RaidDouble: ZoogiRaid::BonusDamage(G, Pi, 6, TEXT("double tap bonus")); Sp(2); Sub = TEXT("+6 BONUS DAMAGE  +2 SP"); break;
			case ECh::RaidStar: { const int32 Ci = FMath::Clamp(Pi, 0, 3); if (G->AwakenChargeOn()) G->AwakenCharge[Ci] += 1; Sub = TEXT("STAR ORBS HIT x2.5  +1 CRYSTAL"); break; }
			case ECh::RaidBank: ZoogiRaid::BonusDamage(G, Pi, 8, TEXT("bank shot crit")); Sp(2); Sub = TEXT("CRIT! +8 BONUS DAMAGE  +2 SP"); break;
			case ECh::RaidInterrupt: Sp(3); Sub = TEXT("ATTACK CANCELLED  +3 SP"); break;
			case ECh::RaidArmor: ZoogiRaid::Expose(G, FMath::Max(1, G->Balls.Num())); Sub = TEXT("WEAK POINT EXPOSED - THE TEAM HITS x2 NEXT TURN"); break;
			case ECh::RaidSafe: ZoogiRaid::BonusDamage(G, Pi, 4, TEXT("safe zone bonus")); Sp(2); Sub = TEXT("DODGED!  +4 BONUS DAMAGE  +2 SP"); break;
			default: break;
			}
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) { F->PublicCallout(TEXT("RAID CHALLENGE CLEAR!"), Sub, FLinearColor(0.35f, 0.95f, 0.55f), 1.8f, 1.15f); F->AddAwakenMeter(G, Pi, ChallengeMeter, TEXT("raid challenge")); }
			ZoogiUtil::PlaySfx(G, TEXT("Score"), 1.f, 1.15f);
			ZoogiHaptics::Kick(0.5f, G->GetWorld(), TEXT("raid challenge"));
			if (!G->ShotDir.IsEmpty()) { static int32 Nr = 0; if (Nr++ < 6) G->TakeShotDelayed(FString::Printf(TEXT("raid_challenge_%d"), Nr), 0.35f); }
		}
		void Finish(AZoogiGameMode* G)
		{
			if (S.Ch == ECh::RaidArmor) ZoogiRaid::ClearArmor();''')
rep(B, '''			S.OrbsIn.Reset(); S.RivalsIn.Reset(); S.RivalsHit.Reset(); S.bOrbTouched = S.bOrbThenRival = false; S.OrbsOut = S.RivalsOut = 0; S.CardAge = 0.f;''',
'''			S.OrbsIn.Reset(); S.RivalsIn.Reset(); S.RivalsHit.Reset(); S.bOrbTouched = S.bOrbThenRival = false; S.OrbsOut = S.RivalsOut = 0; S.CardAge = 0.f;
			S.RaidHits = 0; S.bStarHit = S.bBank = S.bInterrupted = false; S.LaunchD = FVector::ZeroVector;''')
rep(B, '''			if (ChallengeOn())
			{
				TArray<ECh> Pool = { ECh::TwoOrbs, ECh::OrbThenRival };''', '''			if (ChallengeOn() && ZoogiRaid::Active() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidOldChallenges")))
			{	// b200 RAID: challenges that teach the tricks to beating the boss (not orb knockouts)
				TArray<ECh> Pool = { ECh::RaidDouble, ECh::RaidBank, ECh::RaidArmor };
				bool bStar = false; for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->bStar) bStar = true;
				if (bStar) Pool.Add(ECh::RaidStar);
				if (ZoogiBossAttack::Pending(G)) { Pool.Add(ECh::RaidInterrupt); Pool.Add(ECh::RaidInterrupt); Pool.Add(ECh::RaidSafe); Pool.Add(ECh::RaidSafe); }
				FString Force; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiRaidChallenge="), Force)) for (const ECh Cc : { ECh::RaidDouble, ECh::RaidStar, ECh::RaidBank, ECh::RaidInterrupt, ECh::RaidArmor, ECh::RaidSafe }) if (ChText(Cc).StartsWith(Force.ToUpper())) Pool = { Cc };
				S.Ch = Pool[R.RandRange(0, Pool.Num() - 1)];
				if (S.Ch == ECh::RaidArmor) ZoogiRaid::SpawnArmor(G);
			}
			else if (ChallengeOn())
			{
				TArray<ECh> Pool = { ECh::TwoOrbs, ECh::OrbThenRival };''')
# Tick: record the launch line; raid rewards
rep(B, '''		S.OrbsOut = FMath::Max(S.OrbsOut, Oo); S.RivalsOut = FMath::Max(S.RivalsOut, Ro);''', '''		S.OrbsOut = FMath::Max(S.OrbsOut, Oo); S.RivalsOut = FMath::Max(S.RivalsOut, Ro);
		if (G->Phase == EZoogiPhase::Rolling && S.LaunchD.IsNearlyZero())
			if (const AZoogiBall* Me = G->Balls.IsValidIndex(S.Shooter) ? G->Balls[S.Shooter].Get() : nullptr)
				if (Me->GetBallVelocity().Size2D() > 150.f) { S.LaunchP = Me->BallLocation(); S.LaunchD = Me->GetBallVelocity().GetSafeNormal2D(); } // b200 BANK SHOT: the launch line''')
rep(B, '''			Reward(G, B, ChallengePts, TEXT("CHALLENGE COMPLETE!")''', '''			if (IsRaid(S.Ch)) RaidReward(G, B); else
			Reward(G, B, ChallengePts, TEXT("CHALLENGE COMPLETE!")''')
# the hit hook
rep(B, '''	void OnContact(AZoogiGameMode* G, AZoogiBall* Shooter, AActor* Other)''', '''	void OnRaidHit(AZoogiGameMode* G, int32 Player, const TCHAR* What, const FVector& At)
	{
		if (!G || !S.bArmed || Player != S.Shooter || G->TurnCounter != S.Turn || !IsRaid(S.Ch)) return;
		const FString Wt(What);
		if (Wt.Contains(TEXT("bonus")) || Wt.Contains(TEXT("crit")) || Wt == TEXT("SHOWTIME")) return; // challenge payouts / Awaken are not shots
		++S.RaidHits;
		if (Wt == TEXT("star orb")) S.bStarHit = true;
		if (Wt == TEXT("Zoogi slam") && !S.LaunchD.IsNearlyZero())
		{	// not on the launch line = it bounced (rim, rock, a rival, a REACTION) on the way
			const FVector ToHit = FVector(At.X - S.LaunchP.X, At.Y - S.LaunchP.Y, 0.f).GetSafeNormal();
			const float Deg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToHit, S.LaunchD), -1.f, 1.f)));
			if (Deg > 30.f) S.bBank = true;
			UE_LOG(LogZoogi, Log, TEXT("RAID CHALLENGE: P%d hit %.0f deg off the launch line%s"), Player + 1, Deg, S.bBank ? TEXT(" - BANK SHOT") : TEXT(""));
		}
		if (S.Ch == ECh::RaidInterrupt && ZoogiBossAttack::Pending(G)) { ZoogiBossAttack::Interrupt(G, Player); S.bInterrupted = true; }
	}

	void OnContact(AZoogiGameMode* G, AZoogiBall* Shooter, AActor* Other)''')
print('patched')
