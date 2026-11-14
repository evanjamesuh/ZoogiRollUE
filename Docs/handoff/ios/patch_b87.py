# b87 (Evan 13:26-13:31): Wolf Pack fixed + one painted target per banked wolf; Water Cannon = LAUNCH force; GUARD = a fixed,
# unbreakable bumper (stationary, can't roll / fly away); spire "orb generators" drop purple +2 SP orbs; Cosmic rune markers
# grow out of the floor one per round (Cosmic only).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'

# ---------------- Wolf Pack
ed(H + 'ZoogiWolfPack.h', [
 ('	bool IsPainting() const { return Phase == EPhase::Paint; }\n',
  '''	bool IsPainting() const { return Phase == EPhase::Paint; }
	/** b87: a summon over someone else's live pack clears that pack first (one pack at a time; the bank was lost before). */
	void ForceClear();
	/** b87 (Evan: "if I have multiple I should be able to select targets up to how many I have"): banked wolves spent by this
	 *  summon (ZoogiSkills::WolfGo sets it) - the paint takes up to that many targets and the pack has at least that many wolves. */
	int32 BankedWolves = 0;
	int32 PaintMax = 3;
'''),
])
ed(P + 'ZoogiWolfPack.cpp', [
 ('	NumWolves = 3 + (Level >= 5 ? 1 : 0) + (ZoogiWolf::BloodMoon() ? 1 : 0);\n',
  '''	NumWolves = 3 + (Level >= 5 ? 1 : 0) + (ZoogiWolf::BloodMoon() ? 1 : 0);
	PaintMax = BankedWolves > 0 ? BankedWolves : NumWolves; // b87: one painted target per banked wolf
	NumWolves = FMath::Max(NumWolves, BankedWolves);
	const int32 Banked = BankedWolves; BankedWolves = 0;
'''),
 ('	PaintLimit = bHumanOwner ? (PaintSeen.Contains(Owner) ? 4.f : 5.f) : 0.7f; // b55 (Evan: "more time to select targets"): was 1.6 / 2.8 s\n',
  '''	PaintLimit = bHumanOwner ? (PaintSeen.Contains(Owner) ? 4.f : 5.f) + 0.8f * FMath::Max(0, PaintMax - 3) : 0.7f; // b55 (Evan: "more time to select targets"): was 1.6 / 2.8 s; b87: more per extra target
	if (bHumanOwner) Toast(FString::Printf(TEXT("%d WOLF%s!"), NumWolves, NumWolves == 1 ? TEXT("") : TEXT("VES")), FString::Printf(TEXT("TAP UP TO %d TARGET%s"), PaintMax, PaintMax == 1 ? TEXT("") : TEXT("S")), PackTint(B), 2.f, 1.f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: %d banked -> paint up to %d, %d wolves"), Owner + 1, Banked, PaintMax, NumWolves);
'''),
 ('	if (Phase != EPhase::Paint || !bHumanOwner || Painted.Num() >= NumWolves) return;',
  '	if (Phase != EPhase::Paint || !bHumanOwner || Painted.Num() >= PaintMax) return;'),
 ('		else if (Now - PhaseT0 > PaintLimit || Painted.Num() >= NumWolves) Release();',
  '		else if (Now - PhaseT0 > PaintLimit || Painted.Num() >= PaintMax) Release();'),
 ('		Text(FString::Printf(TEXT("TAP UP TO %d TARGETS  -  FIRST ONE GETS THE ALPHA"), NumWolves),',
  '		Text(FString::Printf(TEXT("TAP UP TO %d TARGETS  -  %d / %d PICKED"), PaintMax, Painted.Num(), PaintMax),'),
 ('''void UZoogiWolfPackSys::DetonateAll()''',
  '''void UZoogiWolfPackSys::ForceClear()
{
	if (Phase == EPhase::Idle) return;
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiClone>& W : Wolves) if (AZoogiClone* C = W.Get()) { C->Pop(); ++N; }
	RestoreDil(); EndKillCam();
	UE_LOG(LogZoogi, Log, TEXT("WOLF pack of P%d cleared for a new summon (%d wolves popped, phase %d)"), Owner + 1, N, (int32)Phase);
	Phase = EPhase::Idle; Wolves.Reset(); Painted.Reset(); bHeld = bCharging = false;
}

void UZoogiWolfPackSys::DetonateAll()'''),
])

ed(P + 'ZoogiSkills.cpp', [
 ('#include "ZoogiAnimeSubsystem.h"\n', '#include "ZoogiAnimeSubsystem.h"\n#include "ZoogiWolfPack.h"\n#include "ZoogiClone.h"\n'),
 ('''		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && WolfArmed(G, B->PlayerIndex);
	}''',
  '''		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!(B && !B->bOut && G->Phase == EZoogiPhase::Rolling && WolfArmed(G, B->PlayerIndex))) return false;
		const UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G);
		return !(WP && (WP->IsPainting() || (WP->PackActive() && WP->PackOwnerIndex() == B->PlayerIndex))); // b87: own pack out -> SUPERPOWER recalls it
	}'''),
 ('''		AZoogiBall* B = G->GetActiveBall();
		if (int32* N = GWolfArmed.Find(B->PlayerIndex)) { if (--(*N) <= 0) GWolfArmed.Remove(B->PlayerIndex); }
		UE_LOG(LogZoogi, Log, TEXT("WOLF button: P%d summons the pack mid-roll"), B->PlayerIndex + 1);
		G->FireWolfSkill(B);''',
  '''		AZoogiBall* B = G->GetActiveBall();
		// b87 (Evan: "fix Wolf Pack ... if I have multiple I should be able to select targets up to how many I have"): one press
		// spends every banked wolf (-ZoogiWolfOnePerPress = one per press); a rival's pack still out is cleared first (it used to
		// swallow the summon - bank spent, no wolves)
		static const bool bOnePer = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOnePerPress"));
		int32 Spend = 1;
		if (int32* N = GWolfArmed.Find(B->PlayerIndex)) { Spend = bOnePer ? 1 : FMath::Max(1, *N); *N -= Spend; if (*N <= 0) GWolfArmed.Remove(B->PlayerIndex); }
		if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G)) { if (WP->PackActive()) WP->ForceClear(); WP->BankedWolves = Spend; }
		UE_LOG(LogZoogi, Log, TEXT("WOLF button: P%d summons the pack mid-roll (%d banked wolves)"), B->PlayerIndex + 1, Spend);
		G->FireWolfSkill(B);'''),
 # wolf bank self-test
 ('''		// the CPU taps its wolf a moment into its own roll, like a player would
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;''',
  '''		// b87 -ZoogiWolfBankTest: the human banks 3 wolves on its first aim and fires them 0.5 s into the roll
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfBankTest")) && G)
			if (const AZoogiBall* Bt = G->GetActiveBall())
				if (!Bt->bCPU)
				{
					static int32 BankStep = 0;
					if (BankStep == 0 && G->Phase == EZoogiPhase::Aiming) { for (int32 k = 0; k < 3; ++k) ArmWolf(G, Bt->PlayerIndex); BankStep = 1; UE_LOG(LogZoogi, Log, TEXT("WOLFBANK test: P%d banks %d"), Bt->PlayerIndex + 1, WolfCount(G, Bt->PlayerIndex)); G->TakeShotDelayed(TEXT("wolfbank_a_badge"), 0.5f); }
					else if (BankStep == 1 && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.5f)
					{
						const bool bGo = WolfGo(G); BankStep = 2;
						UE_LOG(LogZoogi, Log, TEXT("WOLFBANK test: SUPERPOWER -> WolfGo %d (left %d)"), bGo ? 1 : 0, WolfCount(G, Bt->PlayerIndex));
						G->TakeShotDelayed(TEXT("wolfbank_b_paint"), 0.4f); G->TakeShotDelayed(TEXT("wolfbank_c_out"), 1.6f); G->TakeShotDelayed(TEXT("wolfbank_d_hunt"), 2.6f);
					}
				}
		// the CPU taps its wolf a moment into its own roll, like a player would
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;'''),
 # water cannon = LAUNCH force
 ('''					if (AZoogiBall* Bb = Cast<AZoogiBall>(Tt)) Kick(Bb->Sphere, Dir * 900.f + FVector(0, 0, 120.f));
					else if (AZoogiOrb* O = Cast<AZoogiOrb>(Tt)) { Kick(O->Sphere, Dir * 820.f + FVector(0, 0, 100.f)); O->LastHitBy = Pi; }''',
  '''					// b87 (Evan: "Water Cannon force increase to equal the launch button"): the LAUNCH speed (full pull); -ZoogiOldWaterCannon = 900 / 820
					static const bool bOldWc = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWaterCannon"));
					const float Vb = bOldWc ? 900.f : ZoogiConst::LaunchSpeedForPull(1.f), Vo = bOldWc ? 820.f : ZoogiConst::LaunchSpeedForPull(1.f);
					if (AZoogiBall* Bb = Cast<AZoogiBall>(Tt)) { Kick(Bb->Sphere, Dir * Vb + FVector(0, 0, 120.f)); UE_LOG(LogZoogi, Log, TEXT("SKILL water_cannon hits P%d at %.0f"), Bb->PlayerIndex + 1, Vb); }
					else if (AZoogiOrb* O = Cast<AZoogiOrb>(Tt)) { Kick(O->Sphere, Dir * Vo + FVector(0, 0, 100.f)); O->LastHitBy = Pi; }'''),
 # guard: unbreakable, anchored bumper
 ('	struct FGuardDome { int32 ArmTurn = -1; bool bUp = false; int32 UpTurn = -1; float Dmg = 0.f; TWeakObjectPtr<AActor> A; TWeakObjectPtr<AZoogiBall> Ball; float OldMass = 0.f, OldLin = 0.f, OldAng = 0.f; };',
  '''	struct FGuardDome { int32 ArmTurn = -1; bool bUp = false; int32 UpTurn = -1; float Dmg = 0.f; TWeakObjectPtr<AActor> A; TWeakObjectPtr<AZoogiBall> Ball; float OldMass = 0.f, OldLin = 0.f, OldAng = 0.f; FVector Anchor = FVector::ZeroVector; TMap<TWeakObjectPtr<AActor>, double> BumpAt; };
	/** b87 (Evan: "Guard is now solid and can't be destroyed and becomes a bumper to others and protects the player" / "Guard form is
	 *  stationary and can not roll or fly away"): -ZoogiGuardBreakable = the b83 cracking dome on a heavy (movable) Zoogi. */
	static bool GuardBreakable() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuardBreakable")); return b; }'''),
 ('''		if (!D || !D->bUp) return;
		D->Dmg += Damage;''',
  '''		if (!D || !D->bUp) return;
		if (!GuardBreakable())
		{	// b87: unbreakable - the hit just flashes off the dome
			AZoogiFX::Spawn(G->GetWorld(), Victim->BallLocation(), FLinearColor(0.6f, 0.9f, 1.f), 1.6f * GuardR(), 0.25f, false, 10.f);
			UE_LOG(LogZoogi, Log, TEXT("GUARD P%d barrier shrugs off a %s hit (unbreakable)"), Victim->PlayerIndex + 1, Why);
			return;
		}
		D->Dmg += Damage;'''),
 ('''				D.bUp = true; D.UpTurn = G->TurnCounter; D.Ball = B;''',
  '''				D.bUp = true; D.UpTurn = G->TurnCounter; D.Ball = B; D.Anchor = B->BallLocation();
				if (B->Sphere && !GuardBreakable()) { B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); }'''),
 ('''				if (AActor* A = D.A.Get()) A->SetActorLocation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f));
				if (G->ActivePlayer == Kv.Key && G->TurnCounter > D.UpTurn) Down.Add(Kv.Key); // its next turn: the barrier drops''',
  '''				if (!GuardBreakable() && B->Sphere && B->Sphere->IsSimulatingPhysics())
				{	// b87: stationary - pinned to the spot it was guarding (nothing rolls it or launches it away)
					if (FVector::Dist(B->BallLocation(), D.Anchor) > 0.5f) B->Sphere->SetWorldLocation(D.Anchor, false, nullptr, ETeleportType::TeleportPhysics);
					B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				}
				if (AActor* A = D.A.Get()) A->SetActorLocation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f));
				if (!GuardBreakable())
				{	// b87: the dome is a solid bumper - Zoogis, orbs and wolves bounce off its surface
					const double Now = FPlatformTime::Seconds();
					const float Rd = GuardR();
					auto Bump = [&](AActor* Who, UPrimitiveComponent* Body, float Rad, bool bOrb)
					{
						if (!Who || !Body || !Body->IsSimulatingPhysics()) return;
						const FVector Pw = Body->GetComponentLocation();
						FVector N = Pw - D.Anchor; N.Z = 0.f;
						const float Dist = N.Size(), Reach = Rd + Rad - 4.f;
						if (Dist >= Reach || Pw.Z > D.Anchor.Z + Rd) return;
						N = Dist > 1.f ? N / Dist : FVector(1.f, 0.f, 0.f);
						Body->SetWorldLocation(FVector(D.Anchor.X, D.Anchor.Y, Pw.Z) + N * (Reach + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
						FVector V = Body->GetPhysicsLinearVelocity();
						const float Vn = FVector::DotProduct(V, N);
						if (Vn < 0.f) V -= 2.f * Vn * N; // reflect
						const float Out = FVector::DotProduct(V, N);
						if (Out < 650.f) V += N * (650.f - Out); // bumper kick
						Body->SetPhysicsLinearVelocity(V);
						const double* Last = D.BumpAt.Find(Who);
						if (!Last || Now - *Last > 0.3)
						{
							D.BumpAt.Add(Who, Now);
							AZoogiFX::Spawn(G->GetWorld(), D.Anchor + N * Rd, FLinearColor(0.55f, 0.9f, 1.f), 140.f, 0.3f, false, 10.f);
							ZoogiUtil::PlaySfx(G, TEXT("Clack"), 0.8f, bOrb ? 1.5f : 1.1f);
							ZoogiHaptics::Kick(0.35f, G->GetWorld(), TEXT("guard_bump"));
							UE_LOG(LogZoogi, Log, TEXT("GUARD P%d dome bumps %s (in %.0f -> out %.0f)"), Kv.Key + 1, *Who->GetName(), -Vn, V.Size2D());
						}
					};
					for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut) Bump(O, O->Sphere, ZoogiConst::ZoogiRadius, false);
					for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Bump(O, O->Sphere, ZoogiConst::OrbRadius, true);
					for (AZoogiClone* Cl : G->Clones) if (Cl && Cl->Sphere) Bump(Cl, Cl->Sphere, Cl->Sphere->GetScaledSphereRadius(), false);
				}
				if (G->ActivePlayer == Kv.Key && G->TurnCounter > D.UpTurn) Down.Add(Kv.Key); // its next turn: the barrier drops'''),
 ('''				if (B->Sphere)
				{	// solid: rivals bounce off, the guarded Zoogi does not budge''',
  '''				if (B->Sphere && GuardBreakable())
				{	// solid: rivals bounce off, the guarded Zoogi does not budge'''),
])

# PackOwnerIndex accessor
ed(H + 'ZoogiWolfPack.h', [
 ('	bool HumanPackOut() const;\n', '	bool HumanPackOut() const;\n	int32 PackOwnerIndex() const { return Owner; }\n'),
])

# ---------------- Cosmic markers hook
ed(P + 'ZoogiStageShift.cpp', [
 ('#include "ZoogiAncientLayout.h"\n', '#include "ZoogiAncientLayout.h"\n#include "ZoogiCosmicMarkers.h"\n'),
 ('''	void OnMatchStart(AZoogiGameMode* G)
	{
		Restore();''', '''	void OnMatchStart(AZoogiGameMode* G)
	{
		ZoogiCosmicMarkers::OnMatchStart(G); // b87
		Restore();'''),
 ('''	void OnRoundStart(AZoogiGameMode* G)
	{''', '''	void OnRoundStart(AZoogiGameMode* G)
	{
		ZoogiCosmicMarkers::OnRoundStart(G); // b87 (Evan): Cosmic grows one rune marker per round'''),
])

# ---------------- SP orbs
ed(H + 'ZoogiCards.h', [
 ('	void AddEnergyDebt(int32 P, int32 Amount) { if (Players.IsValidIndex(P) && Amount > 0) EnergyDebt.FindOrAdd(P) += Amount; }',
  '''	void AddEnergyDebt(int32 P, int32 Amount) { if (Players.IsValidIndex(P) && Amount > 0) EnergyDebt.FindOrAdd(P) += Amount; }
	/** b87 (Evan: purple spire orbs "+2 SP"): extra Card Energy - straight in during P's Standby, else on top of P's next refresh. */
	void AddEnergyBonus(int32 P, int32 Amount) { if (!Players.IsValidIndex(P) || Amount <= 0) return; if (Players[P].bStandbyOpen) Players[P].Energy += Amount; else EnergyBonus.FindOrAdd(P) += Amount; }'''),
 ('	TMap<int32, int32> EnergyDebt; // b58 edge hang saves made off-turn', '	TMap<int32, int32> EnergyDebt; // b58 edge hang saves made off-turn\n	TMap<int32, int32> EnergyBonus; // b87 purple SP orbs'),
])
ed(P + 'ZoogiCards.cpp', [
 ('''EnergyDebt.Remove(P); } // b58: edge hang save paid from this turn
	Log(P, TEXT("standby")''', '''EnergyDebt.Remove(P); } // b58: edge hang save paid from this turn
	if (int32* Bonus = EnergyBonus.Find(P)) { S.Energy += *Bonus; EnergyBonus.Remove(P); } // b87: purple SP orbs collected since
	Log(P, TEXT("standby")'''),
])
ed(H + 'ZoogiCardSubsystem.h', [
 ('	void AddEnergyDebt(int32 P, int32 Amount) { RulesV.AddEnergyDebt(P, Amount); }',
  '	void AddEnergyDebt(int32 P, int32 Amount) { RulesV.AddEnergyDebt(P, Amount); }\n	void AddEnergyBonus(int32 P, int32 Amount) { RulesV.AddEnergyBonus(P, Amount); } // b87'),
])
ed(H + 'ZoogiHoloSpire.h', [
 ('	ZOOGIROLL_API bool Emitting();\n', '''	ZOOGIROLL_API bool Emitting();
	/** b87 (Evan: "new purple orbs appear to increase SP by 2, randomly provided by the spire - orb generators"): a purple SP orb;
	 *  the first Zoogi to touch it absorbs it (+2 Card Energy, ZoogiCardSubsystem::AddEnergyBonus). -ZoogiNoSPOrbs = none. */
	ZOOGIROLL_API void MarkSP(AZoogiOrb* O);
	ZOOGIROLL_API bool IsSP(const AZoogiOrb* O);
'''),
])
ed(P + 'ZoogiHoloSpire.cpp', [
 ('#include "ZoogiActionFun.h"\n', '#include "ZoogiActionFun.h"\n#include "ZoogiCardSubsystem.h"\n#include "ZoogiHaptics.h"\n'),
 ('''	void Pulse() { GEmitUntil = FMath::Max(GEmitUntil, FPlatformTime::Seconds() + 1.6); }''',
  '''	void Pulse() { GEmitUntil = FMath::Max(GEmitUntil, FPlatformTime::Seconds() + 1.6); }
	static TArray<TWeakObjectPtr<AZoogiOrb>> GSP;
	void MarkSP(AZoogiOrb* O) { if (O) GSP.AddUnique(O); }
	bool IsSP(const AZoogiOrb* O) { for (const TWeakObjectPtr<AZoogiOrb>& W : GSP) if (W.Get() == O) return true; return false; }
	static void TickSP(AZoogiGameMode* G)
	{
		for (int32 i = GSP.Num() - 1; i >= 0; --i)
		{
			AZoogiOrb* O = GSP[i].Get();
			if (!O || O->bOut || !G) { GSP.RemoveAt(i); continue; }
			if (O->IsHidden()) continue; // still flying in from the spire
			const FVector L = O->OrbLocation();
			AZoogiBall* Who = nullptr;
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist(B->BallLocation(), L) < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 14.f) { Who = B; break; }
			if (!Who) continue;
			UWorld* W = G->GetWorld();
			const FLinearColor Pu(0.7f, 0.3f, 1.f);
			if (UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G)) CS->AddEnergyBonus(Who->PlayerIndex, 2);
			AZoogiFX::Spawn(W, L, Pu, 220.f, 0.5f, false, 12.f);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), L, FRotator::ZeroRotator, 0.8f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.5f);
			if (!Who->bCPU) ZoogiHaptics::Kick(0.5f, W, TEXT("sp_orb"));
			G->PublicPopup(TEXT("+2 SP"), Who->BallLocation() + FVector(0, 0, 170.f), Pu);
			G->PublicFeed(FString::Printf(TEXT("P%d %s ABSORBS A PURPLE ORB  +2 SP"), Who->PlayerIndex + 1, *Who->Def.Name), Pu);
			UE_LOG(LogZoogi, Log, TEXT("SPIRE SP orb absorbed by P%d (+2 SP)"), Who->PlayerIndex + 1);
			GSP.RemoveAt(i);
			G->Orbs.Remove(O);
			O->Destroy();
		}
	}'''),
 ('''	using namespace ZoogiSpireOrbs;
	if (GEms.Num() == 0) return;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;''',
  '''	using namespace ZoogiSpireOrbs;
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (GSP.Num()) TickSP(G);
	if (GEms.Num() == 0) return;'''),
 ('		const FLinearColor Col = O->bStar ? Gd : Cy;', '		const FLinearColor Col = IsSP(O) ? FLinearColor(0.7f, 0.3f, 1.f) : (O->bStar ? Gd : Cy);'),
])
ed(P + 'ZoogiGameMode.cpp', [
 ('	if (Want <= 0) { ZoogiSpireOrbs::Pulse(); return; }', '	if (Want <= 0) ZoogiSpireOrbs::Pulse(); // b87: the purple SP orbs below still come'),
 ('''	if (Made) PublicFeed(FString::Printf(TEXT("ROUND %d  -  THE SPIRE SENDS %d NEW ORBS"), RoundNumber, Made), FLinearColor(0.5f, 0.9f, 1.f));''',
  '''	// b87 (Evan: "new purple orbs appear to increase SP by 2, randomly provided by the spire - orb generators"): 0-2 purple SP orbs
	static const bool bNoSP = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSPOrbs"));
	const float Roll = FMath::FRand();
	const int32 WantSP = bNoSP ? 0 : (FParse::Param(FCommandLine::Get(), TEXT("ZoogiSPOrbTest")) ? 2 : (Roll < 0.35f ? 0 : (Roll < 0.8f ? 1 : 2)));
	int32 MadeSP = 0;
	for (int32 Try = 0; Try < 120 && MadeSP < WantSP; ++Try)
	{
		const float A = FMath::FRandRange(0.f, 2.f * PI), D = FMath::Sqrt(FMath::FRandRange(260.f * 260.f, RingR * RingR));
		FVector L(FMath::Cos(A) * D, FMath::Sin(A) * D, ZoogiConst::OrbRadius + 1.f);
		if (!OrbSpot(L)) continue;
		bool bBad = false;
		for (const AZoogiOrb* E : Orbs) if (E && !E->bOut && FVector::Dist2D(E->GetActorLocation(), L) < ZoogiConst::OrbRadius * 3.f) bBad = true;
		for (const AZoogiBall* B : Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), L) < 180.f) bBad = true;
		if (bBad) continue;
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (!O) continue;
		O->InitOrb(OrbPM, Decel);
		ZoogiUtil::ApplyOrbStyle(O);
		O->MakePower(FLinearColor(0.7f, 0.3f, 1.f));
		Orbs.Add(O);
		ZoogiSpireOrbs::MarkSP(O);
		ZoogiSpireOrbs::Emit(O, 0.3f + 0.2f * (Made + MadeSP));
		++MadeSP;
	}
	if (MadeSP) { PublicFeed(FString::Printf(TEXT("THE ORB GENERATOR DROPS %d PURPLE ORB%s  -  TOUCH ONE FOR +2 SP"), MadeSP, MadeSP > 1 ? TEXT("S") : TEXT("")), FLinearColor(0.75f, 0.4f, 1.f)); TakeShotDelayed(TEXT("sp_orbs"), 2.4f); }
	UE_LOG(LogZoogi, Log, TEXT("SPIRE SP orbs round %d: %d purple"), RoundNumber, MadeSP);
	if (Made) PublicFeed(FString::Printf(TEXT("ROUND %d  -  THE SPIRE SENDS %d NEW ORBS"), RoundNumber, Made), FLinearColor(0.5f, 0.9f, 1.f));'''),
])
print('b87 ok')
