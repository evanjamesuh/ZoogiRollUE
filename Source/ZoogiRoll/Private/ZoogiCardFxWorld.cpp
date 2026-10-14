// v3.0.0 Phase 3: card effects applied to the real rolling simulation (UZoogiCardSubsystem, cards on only).
// Every hook returns immediately with bCards=False, so the cards-off simulation (roll parity hash) is unchanged.
// Hooks: AZoogiGameMode shot (FxModifyShot before Launch, FxAfterShot after), AZoogiBall::OnSphereHit (FxContact),
// subsystem Tick (FxTick: prev velocities, Double Impact bursts, Orb Magnet, Frost, Ember, Recovery Roll),
// RESOLVE boundary (FxLeadActionEnd: Anchor Guard arms at settle, lead-action effects expire),
// next STANDBY (FxTurnEnd: Turn-End effects expire, Titan mass restored to its actual previous value).
// Push stays physical: guards / Titan / Ember / Bank scale the impulse the physics engine already produced
// (Prev + (Post - Prev) * K), never invent a direction. HP / damage numbers (Guard 12, Anchor 12, Momentum Steal x0.6)
// are display / log only (no HP system in Phase 3; PROPOSED).
#include "ZoogiCardSubsystem.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiCombo.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiTypes.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

static const FName NStorm(TEXT("storm")), NBank(TEXT("bank_strike")), NGuard(TEXT("guard")), NRelay(TEXT("orb_relay")), NHeavy(TEXT("heavy_roll")),
	NEmber(TEXT("ember")), NTitan(TEXT("titan")), NFrost(TEXT("frost")), NPrecision(TEXT("precision")), NSteal(TEXT("momentum_steal")),
	NCrystal(TEXT("crystal_collapse")), NMagnet(TEXT("orb_magnet")), NDouble(TEXT("double_impact")), NAnchor(TEXT("anchor_guard")), NRecovery(TEXT("recovery_roll"));

bool UZoogiCardSubsystem::IsEnemy(const AZoogiGameMode* G, const AZoogiBall* A, const AZoogiBall* B)
{
	if (!G || !A || !B || A == B) return false;
	return G->bComboMode ? A->TeamIndex != B->TeamIndex : true;
}

FVector UZoogiCardSubsystem::Prev(const AZoogiBall* B) const
{
	return B && PrevVel.IsValidIndex(B->PlayerIndex) ? PrevVel[B->PlayerIndex] : (B ? B->GetBallVelocity() : FVector::ZeroVector);
}

void UZoogiCardSubsystem::FxReset()
{
	Fx.Reset();
	PrevVel.Reset(); PendingBursts.Reset(); OrbLife.Reset(); OrbWasOut.Reset();
	MagnetOrb.Reset(); RelayOrb.Reset(); FxShooter = -1;
}

void UZoogiCardSubsystem::FxUse(const FZoogiFxArmed& F, const FString& What)
{
	FxUses.FindOrAdd(F.Card)++;
	UE_LOG(LogZoogi, Log, TEXT("CARDS fx %s effect=%lld P%d: %s"), *F.Card.ToString(), F.EffectId, F.Owner + 1, *What);
}

void UZoogiCardSubsystem::FxArm(AZoogiGameMode* G, int32 P, FName Card, int64 EffectId)
{
	const FZoogiCardDef* D = Def(Card);
	if (!D || Card == NCrystal) return; // Crystal Collapse keeps its Phase 2 path (destruction exemption)
	FZoogiFxArmed& F = Fx.Arm(Card, EffectId, P, G ? G->TurnCounter : 0, D->ParamA, D->ParamB, D->Expiry);
	UE_LOG(LogZoogi, Log, TEXT("CARDS fx armed %s effect=%lld P%d a=%.2f b=%.2f expiry=%s%s"), *Card.ToString(), EffectId, P + 1, F.A, F.B, ZoogiCards::ExpiryName(F.Expiry), F.bArmed ? TEXT("") : TEXT(" (arms at settle)"));
}

// ------------------------------------------------------------------ shot

void UZoogiCardSubsystem::FxModifyShot(AZoogiGameMode* G, AZoogiBall* Ball, FVector2D& V)
{
	if (!Tune.bCards || !G || !Ball) return;
	const int32 P = Ball->PlayerIndex;
	FxShooter = P;
	float Mul = 1.f;
	FZoogiFxArmed* S = Fx.Find(NStorm, P);
	FZoogiFxArmed* H = Fx.Find(NHeavy, P);
	FxCondIf(S); FxCondIf(H); FxCondIf(Fx.Find(NPrecision, P)); // condition: a lead shot fired with the card active
	if (S) Mul *= S->A;
	if (H) Mul *= H->A;
	FxShotIn = V.Size();
	FxShotMul = Mul;
	if (Mul != 1.f)
	{
		const FVector2D In = V;
		V = FZoogiCardEffectCore::ShotScale(V, Mul, ZoogiConst::FullLaunchSpeed * ZoogiCardFxTune::MaxShotMul);
		if (S) { S->Uses++; FxUse(*S, FString::Printf(TEXT("roll control x%.2f (%.0f -> %.0f cm/s, no auto-aim / extra shot)"), S->A, In.Size(), V.Size())); }
		if (H) { H->Uses++; FxUse(*H, FString::Printf(TEXT("force impulse x%.2f, aim error envelope x%.2f"), H->A, H->B)); }
	}
	if (FZoogiFxArmed* Pr = Fx.Find(NPrecision, P))
	{
		// The roll itself is deterministic (no aim noise in this build), so Precision's aim error x0.7 has nothing to scale;
		// its visible part is the forecast (first contact + one rebound, already drawn by the Standby aim forecast).
		Pr->Uses++; FxUse(*Pr, FString::Printf(TEXT("aim error x%.2f (deterministic aim: envelope 0), forecast first contact + %d rebound"), Pr->A, (int32)Pr->B));
	}
	FxShotOut = V.Size();
}

void UZoogiCardSubsystem::FxAfterShot(AZoogiGameMode* G, AZoogiBall* Ball, const FVector2D& V)
{
	if (Tune.bCards && Ball) { const int32 Pc = Ball->PlayerIndex; FxCondIf(Fx.Find(NTitan, Pc)); FxCondIf(Fx.Find(NFrost, Pc)); FxCondIf(Fx.Find(NMagnet, Pc)); }
	if (!Tune.bCards || !G || !Ball || !Ball->Sphere) return;
	const int32 P = Ball->PlayerIndex;
	FxRingMarksAtShot = Ball->RingMarks;
	if (FZoogiFxArmed* T = Fx.Find(NTitan, P))
	{
		const float Actual = Ball->Sphere->GetMass();
		if (Fx.SaveMass(P, Actual))
		{
			Ball->Sphere->SetMassOverrideInKg(NAME_None, Actual * T->A, true);
			T->Uses++; FxUse(*T, FString::Printf(TEXT("mass %.3f -> %.3f kg (prior cached), received push x%.2f"), Actual, Ball->Sphere->GetMass(), T->B));
			FxTitanPrev = Actual; FxTitanSet = Ball->Sphere->GetMass();
		}
	}
	if (FZoogiFxArmed* F = Fx.Find(NFrost, P))
	{
		const int32 N = Fx.AddFrostLane(Ball->BallLocation(), FVector(V.X, V.Y, 0.f), ZoogiCardFxTune::FrostLaneCells);
		F->Uses++; FxUse(*F, FString::Printf(TEXT("lane +%d cells (shared total %d, never stacks), friction x%.2f"), N, Fx.FrostCells(), F->A));
		F->bConsumed = true; // the lane persists until locally cleared (Ember) or match reset
	}
	MagnetOrb.Reset();
	if (FZoogiFxArmed* M = Fx.Find(NMagnet, P))
	{
		float Best = ZoogiCardFxTune::MagnetRange;
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), Ball->BallLocation()); if (D < Best) { Best = D; MagnetOrb = O; } }
		if (AZoogiOrb* O = MagnetOrb.Get())
		{
			MagnetStart = O->OrbLocation(); MagnetAdded = FVector::ZeroVector; MagnetT = 0.f; MagnetPullDisp = 0.f; MagnetEffect = M->EffectId; MagnetOwner = P;
			UE_LOG(LogZoogi, Log, TEXT("CARDS fx orb_magnet effect=%lld P%d: pulling %s (%.0f cm) for %.1f s, max %.0f cm"), M->EffectId, P + 1, *O->GetName(), Best, M->A, M->B * 2.f * ZoogiConst::OrbRadius);
		}
		else UE_LOG(LogZoogi, Log, TEXT("CARDS fx orb_magnet effect=%lld P%d: no neutral orb within %.0f cm"), M->EffectId, P + 1, ZoogiCardFxTune::MagnetRange);
	}
	RelayOrb.Reset();
	if (TestCase == TEXT("effects") && !bTestDone) { TestEffectsOnShot(G, Ball); TestStageContact(G, Ball, V); }
	if (TestCase == TEXT("followup") && !bTestDone && !G->bInFollowUp) TestStageContact(G, Ball, V);
}

// ------------------------------------------------------------------ contacts

void UZoogiCardSubsystem::FxContact(AZoogiBall* Self, AActor* Other, const FVector& NormalImpulse, const FHitResult& Hit)
{
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!Tune.bCards || !G || !Self || G->Stage != EZoogiStage::Battle) return;
	const int32 Shooter = G->ActivePlayer;
	const float Now = GetWorld()->GetTimeSeconds();
	if (AZoogiBall* OB = Cast<AZoogiBall>(Other))
	{
		const int32 Ep = Fx.ContactEpisode(Self->PlayerIndex, OB->PlayerIndex, Now);
		// handled once, from the victim's callback (Self = victim, Other = acting Zoogi)
		if (OB->PlayerIndex != Shooter || Self->PlayerIndex == Shooter || !IsEnemy(G, Self, OB)) return;
		if (NormalImpulse.Size() < ZoogiCardFxTune::QualifyImpulse) return;
		// condition: a qualifying enemy contact by the acting Zoogi (attacker cards) / on the victim (defender cards)
		for (FName Nc : { NEmber, NSteal, NDouble }) FxCondIf(Fx.Find(Nc, OB->PlayerIndex));
		if (FZoogiFxArmed* Bk = Fx.Find(NBank, OB->PlayerIndex)) if (Bk->bBankSetup) FxCondIf(Bk);
		for (FName Nc : { NGuard, NAnchor, NTitan }) FxCondIf(Fx.Find(Nc, Self->PlayerIndex));
		const uint64 Cid = FZoogiCardEffectCore::ContactId(Self->PlayerIndex, OB->PlayerIndex, Ep);
		AZoogiBall* Victim = Self; AZoogiBall* Att = OB;
		const FVector VPrev = Prev(Victim), VPost = Victim->GetBallVelocity();
		float K = 1.f;
		FString Why;
		auto Once = [&](FZoogiFxArmed* F) { return F && F->bArmed && !F->bConsumed && Fx.ApplyOnce(F->EffectId, Cid); };
		if (FZoogiFxArmed* E = Fx.Find(NEmber, Att->PlayerIndex)) if (Once(E)) { K *= E->A; E->Uses++; FxUse(*E, FString::Printf(TEXT("enemy push x%.2f on P%d contact=%llx"), E->A, Victim->PlayerIndex + 1, Cid)); }
		if (FZoogiFxArmed* B = Fx.Find(NBank, Att->PlayerIndex)) if (B->bBankSetup && B->Uses == 0 && Once(B)) { K *= ZoogiCardFxTune::BankBonus; B->Uses++; B->bConsumed = true; FxUse(*B, FString::Printf(TEXT("bank-setup strike x%.2f (PROPOSED) on P%d"), ZoogiCardFxTune::BankBonus, Victim->PlayerIndex + 1)); }
		FZoogiFxArmed* Gd = Fx.Find(NGuard, Victim->PlayerIndex);
		FZoogiFxArmed* An = Fx.Find(NAnchor, Victim->PlayerIndex);
		if (Once(Gd)) { K *= Gd->B; Gd->Uses++; Gd->bConsumed = true; FxUse(*Gd, FString::Printf(TEXT("consumed: absorb %.0f (display), push x%.2f from P%d (still physical)"), Gd->A, Gd->B, Att->PlayerIndex + 1)); }
		else if (Once(An)) { K *= An->B; An->Uses++; An->bConsumed = true; FxUse(*An, FString::Printf(TEXT("consumed: absorb %.0f (display), push x%.2f from P%d"), An->A, An->B, Att->PlayerIndex + 1)); }
		if (FZoogiFxArmed* T = Fx.Find(NTitan, Victim->PlayerIndex)) if (Once(T)) { K *= T->B; T->Uses++; FxUse(*T, FString::Printf(TEXT("received push x%.2f"), T->B)); }
		if (K != 1.f && Victim->Sphere)
		{
			Victim->Sphere->SetPhysicsLinearVelocity(FZoogiCardEffectCore::ScaleDelta(VPrev, VPost, K));
			UE_LOG(LogZoogi, Log, TEXT("CARDS fx push P%d->P%d contact=%llx K=%.3f |dv| %.0f -> %.0f"), Att->PlayerIndex + 1, Victim->PlayerIndex + 1, Cid, K, FVector::Dist2D(VPost, VPrev), FVector::Dist2D(VPost, VPrev) * K);
		}
		if (FZoogiFxArmed* S = Fx.Find(NSteal, Att->PlayerIndex)) if (Once(S))
		{
			S->bConsumed = true; // first qualified contact consumes it, glancing or not
			FVector VA = Prev(Att), VV = Victim->GetBallVelocity();
			const FVector VA0 = VA;
			if (FZoogiCardEffectCore::MomentumSteal(VA, VV, Hit.ImpactNormal, S->A))
			{
				FVector CurA = Att->GetBallVelocity(); CurA += (VA - VA0);
				Att->Sphere->SetPhysicsLinearVelocity(CurA);
				Victim->Sphere->SetPhysicsLinearVelocity(VV);
				S->Uses++; FxUse(*S, FString::Printf(TEXT("glancing: %.0f cm/s tangential moved to P%d, damage x%.2f (display)"), (VA - VA0).Size2D(), Victim->PlayerIndex + 1, S->B));
			}
			else UE_LOG(LogZoogi, Log, TEXT("CARDS fx momentum_steal effect=%lld consumed by a head-on contact (no transfer)"), S->EffectId);
		}
		if (FZoogiFxArmed* D = Fx.Find(NDouble, Att->PlayerIndex)) if (D->bArmed && Fx.ApplyOnce(D->EffectId, Cid))
		{
			Fx.TryBurst(Cid); // burst 1: the physical contact
			FBurst B; B.Victim = Victim; B.Dv = (Victim->GetBallVelocity() - VPrev) * D->B; B.Dv.Z = 0.f; B.At = Now + D->A; B.Effect = D->EffectId; B.Contact = Cid; B.Owner = Att->PlayerIndex;
			PendingBursts.Add(B);
		}
		return;
	}
	if (AZoogiOrb* O = Cast<AZoogiOrb>(Other))
	{
		const int32 Oi = G->Orbs.IndexOfByKey(O);
		const int32 Life = OrbLife.IsValidIndex(Oi) ? OrbLife[Oi] : 0;
		// Orb Relay (card book: "physical Orb contact creates a relay candidate; partner + 30 Combo required"): the acting
		// Zoogi's REAL orb contact earns the Orb Relay setup once per orb life. It grants no Combo: the follow-up offer still
		// validates the partner and charges the 30 Combo once (AZoogiGameMode::ComboResolve / ComboAcceptFollowUp).
		if (Self->PlayerIndex == Shooter)
			if (FZoogiFxArmed* R = Fx.Find(NRelay, Shooter)) if (!RelayOrb.IsValid())
			{
				FxCondIf(R); // condition: the acting Zoogi's real orb contact
				RelayOrb = O; RelayLife = Life;
				if (CardSetup(G, ZoogiComboTrigger::OrbRelay, O->OrbLocation(), Oi, Life))
				{
					R->Uses++; R->bConsumed = true;
					FxUse(*R, FString::Printf(TEXT("real orb contact (orb %d life %d) -> ORB RELAY setup (partner + %.0f Combo still required)"), Oi, Life, R->A));
				}
				else UE_LOG(LogZoogi, Log, TEXT("CARDS fx orb_relay effect=%lld P%d: orb %d life %d contact gives no setup (follow-up / scored / not 2v2)"), R->EffectId, Shooter + 1, Oi, Life);
			}
		return;
	}
	// static geometry: Bank Strike needs a real wall / bumper contact (not the floor)
	if (Self->PlayerIndex == Shooter && FMath::Abs(Hit.ImpactNormal.Z) < 0.5f)
		if (FZoogiFxArmed* B = Fx.Find(NBank, Shooter)) if (!B->bBankSetup && Fx.ApplyOnce(B->EffectId, 1))
		{
			FxCondIf(B); // condition: a real bank (wall / bumper) contact
			B->bBankSetup = true;
			FxUse(*B, FString::Printf(TEXT("BankSetup: real bank contact with %s"), *GetNameSafe(Other)));
			CardSetup(G, ZoogiComboTrigger::BankSetup, Hit.ImpactPoint, -1, 0);
		}
}

// ------------------------------------------------------------------ Phase 4: card setups feed the combo system

bool UZoogiCardSubsystem::CardSetup(AZoogiGameMode* G, uint32 Flag, const FVector& Where, int32 Orb, int32 Life)
{
	// Only from a real event of the lead action, never during the partner follow-up (no setup farming / third action),
	// once per orb life for orbs, and through ComboTrigger (one flag per shot). No Combo is granted here.
	if (!G || !G->bComboMode) return false;
	if (G->bInFollowUp) { ++FxSetupRefused; UE_LOG(LogZoogi, Log, TEXT("CARDS setup %u refused: partner follow-up in progress"), Flag); return false; }
	if (Orb >= 0 && !Fx.ScoreOrbOnce(Orb, Life)) { ++FxSetupRefused; return false; }
	const bool bNew = (G->ShotTriggers & Flag) == 0;
	G->ComboTrigger(Flag, 0.f, Where);
	++FxCardSetups;
	UE_LOG(LogZoogi, Log, TEXT("CARDS setup %s from a real contact (%s)"), Flag == ZoogiComboTrigger::BankSetup ? TEXT("BANK SETUP") : TEXT("ORB RELAY"), bNew ? TEXT("new") : TEXT("already earned this shot"));
	return true;
}

// ------------------------------------------------------------------ tick

void UZoogiCardSubsystem::FxTick(AZoogiGameMode* G, float Dt)
{
	// orb lives: a respawned orb is a new life (Orb Relay scores once per life)
	OrbLife.SetNum(G->Orbs.Num()); OrbWasOut.SetNum(G->Orbs.Num());
	for (int32 i = 0; i < G->Orbs.Num(); ++i)
	{
		const bool bOut = !G->Orbs[i] || G->Orbs[i]->bOut;
		if (OrbWasOut[i] && !bOut) ++OrbLife[i];
		OrbWasOut[i] = bOut;
	}
	const bool bRolling = G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::Rolling;
	const float Now = GetWorld()->GetTimeSeconds();
	// Double Impact second burst (max 2 per contact episode)
	for (int32 i = PendingBursts.Num() - 1; i >= 0; --i)
	{
		FBurst& B = PendingBursts[i];
		if (Now < B.At) continue;
		AZoogiBall* V = B.Victim.Get();
		if (V && V->Sphere && !V->bOut && Fx.TryBurst(B.Contact))
		{
			V->Sphere->SetPhysicsLinearVelocity(V->GetBallVelocity() + B.Dv);
			if (FZoogiFxArmed* D = Fx.Find(NDouble, B.Owner)) { D->Uses++; FxUse(*D, FString::Printf(TEXT("burst 2 on P%d contact=%llx |dv| %.0f (bursts %d)"), V->PlayerIndex + 1, B.Contact, B.Dv.Size(), Fx.BurstsFor(B.Contact))); }
		}
		FxMaxBursts = FMath::Max(FxMaxBursts, Fx.BurstsFor(B.Contact));
		PendingBursts.RemoveAt(i);
	}
	if (bRolling && G->Balls.IsValidIndex(FxShooter))
	{
		AZoogiBall* S = G->Balls[FxShooter];
		// Orb Magnet: bounded pull (time, displacement, geometry)
		if (AZoogiOrb* O = MagnetOrb.Get())
		{
			const FZoogiFxArmed* M = Fx.Find(NMagnet, MagnetOwner);
			const float MaxMove = M ? M->B * 2.f * ZoogiConst::OrbRadius : 0.f;
			MagnetT += Dt;
			FHitResult H;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiOrbMagnet), false, O); Q.AddIgnoredActor(S);
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(H, O->OrbLocation(), S->BallLocation(), ECC_WorldStatic, Q);
			// bound on the pull-attributable displacement (integral of the velocity the pull added), so collisions /
			// the orb's own roll never count against it and the pull can never move it more than MaxMove
			const FVector Dv = (!M || O->bOut || bBlocked || MagnetT > M->A || MagnetPullDisp >= MaxMove) ? FVector::ZeroVector
				: FZoogiCardEffectCore::MagnetStep(O->OrbLocation(), O->OrbLocation(), S->BallLocation(), MaxMove, ZoogiCardFxTune::MagnetAccel, Dt);
			if (!Dv.IsZero()) { O->Sphere->SetPhysicsLinearVelocity(O->Sphere->GetPhysicsLinearVelocity() + Dv); MagnetAdded += Dv; MagnetPullDisp += MagnetAdded.Size2D() * Dt; }
			else
			{
				// pull over: take back exactly the velocity it added so the bound holds (no residual momentum)
				if (!O->bOut && !MagnetAdded.IsZero()) O->Sphere->SetPhysicsLinearVelocity(O->Sphere->GetPhysicsLinearVelocity() - MagnetAdded);
				const float Moved = MagnetPullDisp;
				FxMagnetMoved = FMath::Max(FxMagnetMoved, Moved); FxMagnetMax = MaxMove;
				if (FZoogiFxArmed* MM = Fx.Find(NMagnet, MagnetOwner)) { MM->Uses++; MM->bConsumed = true; FxUse(*MM, FString::Printf(TEXT("pull ended after %.2f s%s: orb moved %.0f cm (bound %.0f)"), MagnetT, bBlocked ? TEXT(" (blocked by geometry)") : TEXT(""), Moved, MaxMove)); }
				MagnetOrb.Reset();
			}
		}
		// Ember clears only Frost cells it overlaps
		if (const FZoogiFxArmed* E = Fx.Find(NEmber, FxShooter)) if (!S->bOut)
		{
			const int32 N = Fx.ClearFrostNear(S->BallLocation(), ZoogiCardFxTune::EmberClearR);
			if (N > 0) { FxUses.FindOrAdd(NEmber)++; UE_LOG(LogZoogi, Log, TEXT("CARDS fx ember effect=%lld P%d: cleared %d overlapping Frost cells (%d left)"), E->EffectId, FxShooter + 1, N, Fx.FrostCells()); }
		}
		// Recovery Roll: extra deceleration near the safe zone; never heals, never touches Ring Marks
		if (FZoogiFxArmed* R = Fx.Find(NRecovery, FxShooter)) if (!S->bOut && S->Sphere)
		{
			const FVector Zone = G->bComboMode ? AZoogiComboArena::GateReturn() : FVector::ZeroVector;
			const FVector V = S->GetBallVelocity();
			const float Sp = FVector(V.X, V.Y, 0.f).Size();
			if (FVector::Dist2D(S->BallLocation(), Zone) < ZoogiCardFxTune::RecoveryZoneR && Sp > 1.f)
			{
				FxCondIf(R); // condition: rolling inside the safe zone
				const float Cut = FMath::Min(Sp, S->RollingDecel * (R->A - 1.f) * Dt);
				S->Sphere->SetPhysicsLinearVelocity(V - FVector(V.X, V.Y, 0.f) / Sp * Cut);
				if (R->Uses++ == 0) FxUse(*R, FString::Printf(TEXT("deceleration x%.2f inside the safe zone (no heal, Ring Marks %d)"), R->A, S->RingMarks));
			}
		}
	}
	// Frost: shared lane, every rolling Zoogi on a cell keeps (1 - friction) of the rolling deceleration
	if (bRolling && Fx.FrostCells() > 0)
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bOut || !B->Sphere || !Fx.InFrost(B->BallLocation())) continue;
			const FVector V = B->GetBallVelocity();
			const float Sp = FVector(V.X, V.Y, 0.f).Size();
			if (Sp < 5.f) continue;
			B->Sphere->SetPhysicsLinearVelocity(V + FVector(V.X, V.Y, 0.f) / Sp * (B->RollingDecel * (1.f - 0.35f) * Dt));
			FxFrostTicks++;
		}
	// previous-frame velocities (this tick runs after physics: the next contact's pre-impact velocity)
	PrevVel.SetNum(G->Balls.Num());
	for (int32 i = 0; i < G->Balls.Num(); ++i) PrevVel[i] = G->Balls[i] ? G->Balls[i]->GetBallVelocity() : FVector::ZeroVector;
}

// ------------------------------------------------------------------ expiry

void UZoogiCardSubsystem::FxLeadActionEnd(AZoogiGameMode* G)
{
	if (!Tune.bCards || !G) return;
	const int32 P = FxShooter;
	// Anchor Guard arms after the acting Zoogi settles
	if (FZoogiFxArmed* A = Fx.Find(NAnchor, P)) if (!A->bArmed && G->Balls.IsValidIndex(P) && !G->Balls[P]->bOut)
	{ A->bArmed = true; UE_LOG(LogZoogi, Log, TEXT("CARDS fx anchor_guard effect=%lld P%d armed at settle"), A->EffectId, P + 1); }
	if (G->Balls.IsValidIndex(P) && FxRingMarksAtShot >= 0)
		FxNoHealOk &= G->Balls[P]->RingMarks <= FxRingMarksAtShot; // Recovery Roll (and every card) never heals Ring Marks
	if (MagnetOrb.IsValid() && !MagnetAdded.IsZero() && !MagnetOrb->bOut) MagnetOrb->Sphere->SetPhysicsLinearVelocity(MagnetOrb->Sphere->GetPhysicsLinearVelocity() - MagnetAdded);
	MagnetOrb.Reset(); RelayOrb.Reset(); PendingBursts.Reset();
	for (const FZoogiFxArmed& F : Fx.Expire(EZoogiCardExpiry::LeadActionEnd, P))
		UE_LOG(LogZoogi, Log, TEXT("CARDS fx %s effect=%lld P%d expired (lead action end, uses %d)"), *F.Card.ToString(), F.EffectId, F.Owner + 1, F.Uses);
	Fx.Active.RemoveAll([](const FZoogiFxArmed& F) { return F.bConsumed; });
	FxRingMarksAtShot = -1;
}

void UZoogiCardSubsystem::FxTurnEnd(AZoogiGameMode* G)
{
	if (!Tune.bCards || !G) return;
	for (const FZoogiFxArmed& F : Fx.Expire(EZoogiCardExpiry::TurnEnd, -1))
	{
		if (F.Card == NTitan && G->Balls.IsValidIndex(F.Owner))
		{
			float Prev = 0.f;
			AZoogiBall* B = G->Balls[F.Owner];
			if (Fx.RestoreMass(F.Owner, Prev) && B && B->Sphere)
			{
				B->Sphere->SetMassOverrideInKg(NAME_None, Prev, true);
				// an out Zoogi does not simulate (GetMass() reads 0): verify the stored override instead
				const FBodyInstance* BI = B->Sphere->GetBodyInstance();
				const float Now = B->Sphere->IsSimulatingPhysics() ? B->Sphere->GetMass() : (BI ? BI->GetMassOverride() : Prev);
				FxTitanRestoreOk &= FMath::IsNearlyEqual(Now, Prev, 1e-3f);
				++FxTitanRestores;
				UE_LOG(LogZoogi, Log, TEXT("CARDS fx titan effect=%lld P%d mass restored %.3f kg (actual previous %.3f)"), F.EffectId, F.Owner + 1, Now, Prev);
			}
		}
		UE_LOG(LogZoogi, Log, TEXT("CARDS fx %s effect=%lld P%d expired (turn end, uses %d)"), *F.Card.ToString(), F.EffectId, F.Owner + 1, F.Uses);
	}
	// guards whose owner was knocked out leave with it
	Fx.Active.RemoveAll([G](const FZoogiFxArmed& F) { return F.Owner >= 0 && G->Balls.IsValidIndex(F.Owner) && G->Balls[F.Owner] && G->Balls[F.Owner]->bKnockedOut; });
}

// ------------------------------------------------------------------ runtime tests (effects / mutual_ko)

void UZoogiCardSubsystem::TestEffectsOnShot(AZoogiGameMode* G, AZoogiBall* Ball)
{
	const float Cap = ZoogiConst::FullLaunchSpeed * ZoogiCardFxTune::MaxShotMul;
	const float Want = FxShotIn <= 0.f ? 0.f : FMath::Min(FxShotIn * FxShotMul, FMath::Max(FxShotIn, Cap));
	TestCheck(FMath::IsNearlyEqual(FxShotOut, Want, 1.f), FString::Printf(TEXT("P%d shot %.0f x%.3f -> %.0f (want %.0f)"), Ball->PlayerIndex + 1, FxShotIn, FxShotMul, FxShotOut, Want));
	if (Fx.Find(FName(TEXT("titan")), Ball->PlayerIndex) && FxTitanPrev > 0.f)
		TestCheck(FMath::IsNearlyEqual(FxTitanSet, FxTitanPrev * 1.35f, 0.01f), FString::Printf(TEXT("titan mass %.3f == %.3f x1.35"), FxTitanSet, FxTitanPrev));
	FxTitanPrev = 0.f;
	TestCheck(Fx.AppliedCount() >= 0 && FxMaxBursts <= 2, TEXT("burst cap holds mid-match"));
}

void UZoogiCardSubsystem::TestMutualKO(AZoogiGameMode* G)
{
	bMutualTested = true;
	TestCheck(G->bComboMode, TEXT("mutual_ko runs in combo mode (-ZoogiCombo)"));
	if (!G->bComboMode || G->Balls.Num() < 4) return;
	for (const AZoogiBall* B : G->Balls) TestCheck(B && B->RingMarks == 2, FString::Printf(TEXT("P%d starts with 2 Ring Marks"), B ? B->PlayerIndex + 1 : 0));
	auto Drop = [](AZoogiBall* B) { B->Sphere->SetWorldLocation(FVector(0.f, 0.f, ZoogiConst::FallOutZ - 400.f), false, nullptr, ETeleportType::TeleportPhysics); };
	// team 0 = P1,P2  team 1 = P3,P4
	AZoogiBall* P1 = G->Balls[0]; AZoogiBall* P2 = G->Balls[1]; AZoogiBall* P3 = G->Balls[2]; AZoogiBall* P4 = G->Balls[3];
	// 1) ring out with 2 marks -> 1 mark, Recovery Gate pending (not knocked out)
	Drop(P2);
	G->ComboCheckKnockoffs();
	TestCheck(P2->RingMarks == 1 && P2->bPendingRecovery && !P2->bKnockedOut && !G->bComboMatchOver, FString::Printf(TEXT("ring out: marks %d pending=%d ko=%d"), P2->RingMarks, P2->bPendingRecovery ? 1 : 0, P2->bKnockedOut ? 1 : 0));
	// 2) the second ring out of the same Zoogi knocks it out
	P2->bPendingRecovery = false; P2->PlaceAt(FVector(300.f, 300.f, ZoogiConst::ZoogiRadius + 1.f), 0.f); P2->bOut = false;
	Drop(P2);
	G->ComboCheckKnockoffs();
	TestCheck(P2->RingMarks == 0 && P2->bKnockedOut && !G->bComboMatchOver, TEXT("second ring out: knocked out, match continues (partner alive)"));
	// 3) one team-B Zoogi already out; then the last Zoogi of each team falls in the same check -> draw
	P4->RingMarks = 1; Drop(P4); G->ComboCheckKnockoffs();
	TestCheck(P4->bKnockedOut && !G->bComboMatchOver, TEXT("P4 knocked out, P3 still alive"));
	P1->RingMarks = 1; P3->RingMarks = 1;
	Drop(P1); Drop(P3);
	G->ComboCheckKnockoffs();
	UE_LOG(LogZoogi, Log, TEXT("CARD_TEST mutual_ko: over=%d winner=%d reason=%s"), G->bComboMatchOver ? 1 : 0, G->ComboWinner, *G->ComboOverReason);
	TestCheck(P1->bKnockedOut && P3->bKnockedOut, TEXT("both final Zoogis knocked out"));
	TestCheck(G->bComboMatchOver && G->ComboWinner == -1, FString::Printf(TEXT("mutual final KO = draw (winner %d)"), G->ComboWinner));
}

void UZoogiCardSubsystem::TestStageContact(AZoogiGameMode* G, AZoogiBall* Ball, const FVector2D& V)
{
	// Test only: the CPU rarely aims at a Zoogi, so contact cards would seldom see a real contact. Put one enemy on the
	// launch line 250 cm ahead (prefer a guarded one) so the REAL physics contact + hit callbacks run the effect code.
	const FVector D = FVector(V.X, V.Y, 0.f).GetSafeNormal();
	if (D.IsNearlyZero()) return;
	const FVector Side(-D.Y, D.X, 0.f);
	const float Z = Ball->BallLocation().Z;
	// Orb Relay: orb on the line, partner behind it (shooter -> orb -> partner)
	if (Fx.Find(FName(TEXT("orb_relay")), Ball->PlayerIndex) && G->bComboMode)
	{
		AZoogiBall* Partner = G->PartnerOf(Ball);
		AZoogiOrb* Orb = nullptr;
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { Orb = O; break; }
		if (Partner && !Partner->bOut && !Partner->bKnockedOut && Orb)
		{
			const FVector Op = Ball->BallLocation() + D * 200.f, Pp = Ball->BallLocation() + D * 420.f;
			Orb->Respawn(FVector(Op.X, Op.Y, ZoogiConst::OrbRadius + 1.f));
			Partner->PlaceAt(FVector(Pp.X, Pp.Y, Z), 0.f);
			UE_LOG(LogZoogi, Log, TEXT("CARD_TEST effects: staged orb + partner P%d on P%d's launch line"), Partner->PlayerIndex + 1, Ball->PlayerIndex + 1);
			return;
		}
	}
	// Momentum Steal needs a glancing contact: offset the target sideways (0.75 of the contact distance)
	const float Lateral = Fx.Find(FName(TEXT("momentum_steal")), Ball->PlayerIndex) ? 0.75f * 2.f * ZoogiConst::ZoogiRadius : 0.f;
	AZoogiBall* Pick = nullptr;
	for (AZoogiBall* B : G->Balls)
	{
		if (!B || B->bOut || B->bKnockedOut || !IsEnemy(G, Ball, B)) continue;
		const bool bGuarded = Fx.Find(FName(TEXT("guard")), B->PlayerIndex) || Fx.Find(FName(TEXT("anchor_guard")), B->PlayerIndex);
		if (!Pick || bGuarded) Pick = B;
		if (bGuarded) break;
	}
	if (!Pick) return;
	const FVector At = Ball->BallLocation() + D * 250.f + Side * Lateral;
	Pick->PlaceAt(FVector(At.X, At.Y, Z), 0.f);
	UE_LOG(LogZoogi, Log, TEXT("CARD_TEST effects: staged P%d on P%d's launch line at (%.0f, %.0f)"), Pick->PlayerIndex + 1, Ball->PlayerIndex + 1, At.X, At.Y);
}

// Phase 4 follow-up test: watches the real combo flow (offer -> continue / pass -> one follow-up -> end of team turn).
void UZoogiCardSubsystem::TestFollowUpTick(AZoogiGameMode* G)
{
	if (!G->bComboMode) { TestCheck(false, TEXT("followup runs in combo mode (-ZoogiCombo)")); TestFinish(G); return; }
	const int32 T = G->ComboTeam;
	if (T != FuPrevTeam) { FuPrevTeam = T; FuThisTeamTurn = 0; }
	if (G->bInFollowUp && G->bFollowUpOffered) TestCheck(false, TEXT("offer during a follow-up (third action)"));
	if (G->bFollowUpOffered && !bFuPrevOffered)
	{
		++FuOffers;
		AZoogiBall* S = G->GetActiveBall();
		AZoogiBall* P = S ? G->PartnerOf(S) : nullptr;
		TestCheck(P && !P->bOut && !P->bKnockedOut, TEXT("offer only with an eligible partner"));
		TestCheck(!G->bInFollowUp && !G->bFollowUpUsed, TEXT("offer only for the lead action, once per team turn"));
		TestCheck(G->ShotTriggers != 0, TEXT("offer only after a real setup"));
		TestCheck(P && UZoogiBattleStateMachine::CanAffordFollowUp(G, P->PlayerIndex, G->ComboEnergy[T], ZoogiComboTune::FollowUpCost), TEXT("offer only when the 30 Combo is affordable"));
		if (FuOffers % 2 == 0)
		{
			const float Before = G->ComboEnergy[T];
			const bool bPass = G->ComboDeclineFollowUp(); // PASS (the human button / timeout path)
			++FuDeclines;
			TestCheck(bPass && G->ComboEnergy[T] == Before && !G->bInFollowUp, FString::Printf(TEXT("pass charges nothing (%.0f -> %.0f)"), Before, G->ComboEnergy[T]));
			UE_LOG(LogZoogi, Log, TEXT("CARD_TEST followup: offer %d PASSED (gauge %.0f kept)"), FuOffers, G->ComboEnergy[T]);
		}
	}
	if (G->bInFollowUp && !bFuPrevIn)
	{
		++FuAccepts; ++FuThisTeamTurn;
		const float Paid = FuPrevGauge[T] - G->ComboEnergy[T];
		TestCheck(FuThisTeamTurn == 1, FString::Printf(TEXT("exactly one follow-up per team turn (%d)"), FuThisTeamTurn));
		TestCheck(FMath::IsNearlyEqual(Paid, ZoogiComboTune::FollowUpCost, 0.5f) || FMath::IsNearlyZero(Paid, 0.5f), FString::Printf(TEXT("follow-up charged once: gauge -%.0f (30 Combo, or Episode Energy when that is the cost source)"), Paid));
		UE_LOG(LogZoogi, Log, TEXT("CARD_TEST followup: CONTINUE accepted, gauge %.0f -> %.0f"), FuPrevGauge[T], G->ComboEnergy[T]);
	}
	bFuPrevOffered = G->bFollowUpOffered; bFuPrevIn = G->bInFollowUp;
	FuPrevGauge[0] = G->ComboEnergy[0]; FuPrevGauge[1] = G->ComboEnergy[1];
	if (G->Phase == EZoogiPhase::GameOver) TestFinish(G);
}

// ------------------------------------------------------------------ v3 P6 coverage
void UZoogiCardSubsystem::FxCondIf(const FZoogiFxArmed* F) { if (F) CovCond.FindOrAdd(F->Card).Add(F->EffectId); }
void UZoogiCardSubsystem::FxCond(FName Card, int32 Owner) { FxCondIf(Fx.Find(Card, Owner)); }

void UZoogiCardSubsystem::DumpCoverage(const FString& Tag) const
{
	TMap<FName, int32> Committed;
	for (const FZoogiCardEvent& E : RulesV.Events()) if (E.Kind == FName(TEXT("commit_card"))) Committed.FindOrAdd(E.CardId)++;
	for (const FZoogiCardDef& D : ZoogiCards::BuiltInDefs())
	{
		const int32* Sel = CovSelected.Find(D.Id); const int32* Com = Committed.Find(D.Id); const TSet<int64>* Cnd = CovCond.Find(D.Id); const int32* Trg = FxUses.Find(D.Id);
		UE_LOG(LogZoogi, Log, TEXT("CARD_COV tag=%s card=%s selected=%d committed=%d condition=%d triggered=%d"), *Tag, *D.Id.ToString(), Sel ? *Sel : 0, Com ? *Com : 0, Cnd ? Cnd->Num() : 0, Trg ? *Trg : 0);
	}
}
