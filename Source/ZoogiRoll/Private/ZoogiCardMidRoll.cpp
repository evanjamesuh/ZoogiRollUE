// Oct 22 (Evan): cards played AFTER launch. Launch-time effects (FxModifyShot / FxAfterShot) are re-applied to the moving
// Zoogi right away through the same gameplay hooks the card layer already uses (velocity / mass override on the ball's
// sphere, the Frost lane store, the Orb Magnet pull) - no physics engine changes. Contact / tick / settle effects need
// nothing (they already run during the roll). Precision is Blocked mid-roll (greyed out by AZoogiGameMode::CardsSlotPlayableNow).
#include "ZoogiCardSubsystem.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "Components/SphereComponent.h"

void UZoogiCardSubsystem::FxApplyMidRoll(AZoogiGameMode* G, int32 P, FName Card)
{
	if (!Tune.bCards || !G) return;
	AZoogiBall* Ball = G->Balls.IsValidIndex(P) ? G->Balls[P] : nullptr;
	FZoogiFxArmed* F = Fx.Find(Card, P);
	if (!Ball || !Ball->Sphere || Ball->bOut || !F) return;
	if (ZoogiCards::MidRollMode(Card) != ZoogiCards::EMidRoll::Immediate) return;
	FxShooter = P; // tick effects (Ember / Recovery) follow this Zoogi
	static const FName MRNStorm(TEXT("storm")), MRNHeavy(TEXT("heavy_roll")), MRNTitan(TEXT("titan")), MRNFrost(TEXT("frost")), MRNMagnet(TEXT("orb_magnet"));
	const FVector V = Ball->GetBallVelocity();
	FxCondIf(F);
	if (Card == MRNStorm || Card == MRNHeavy)
	{
		const FVector Out = ZoogiCards::MidRollVelocity(V, F->A, ZoogiConst::FullLaunchSpeed * ZoogiCardFxTune::MaxShotMul);
		Ball->Sphere->SetPhysicsLinearVelocity(Out);
		F->Uses++; FxUse(*F, FString::Printf(TEXT("mid-roll speed x%.2f (%.0f -> %.0f cm/s)"), F->A, FVector2D(V).Size(), FVector2D(Out).Size()));
	}
	else if (Card == MRNTitan)
	{
		const float Actual = Ball->Sphere->GetMass();
		if (Fx.SaveMass(P, Actual))
		{
			Ball->Sphere->SetMassOverrideInKg(NAME_None, Actual * F->A, true);
			F->Uses++; FxUse(*F, FString::Printf(TEXT("mid-roll mass %.3f -> %.3f kg (restored at turn end), received push x%.2f"), Actual, Ball->Sphere->GetMass(), F->B));
		}
	}
	else if (Card == MRNFrost)
	{
		const FVector Dir = FVector(V.X, V.Y, 0.f).SizeSquared() > 1.f ? FVector(V.X, V.Y, 0.f) : FVector(1, 0, 0);
		const int32 N = Fx.AddFrostLane(Ball->BallLocation(), Dir, ZoogiCardFxTune::FrostLaneCells * (PowerRider(Ball, MRNFrost) ? 2 : 1));
		F->Uses++; F->bConsumed = true;
		FxUse(*F, FString::Printf(TEXT("mid-roll lane +%d cells ahead of the Zoogi (total %d)"), N, Fx.FrostCells()));
	}
	else if (Card == MRNMagnet)
	{
		float Best = ZoogiCardFxTune::MagnetRange; AZoogiOrb* Pick = nullptr;
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), Ball->BallLocation()); if (D < Best) { Best = D; Pick = O; } }
		if (Pick)
		{
			MagnetOrb = Pick; MagnetStart = Pick->OrbLocation(); MagnetAdded = FVector::ZeroVector; MagnetT = 0.f; MagnetPullDisp = 0.f; MagnetEffect = F->EffectId; MagnetOwner = P;
			UE_LOG(LogZoogi, Log, TEXT("CARDS fx orb_magnet effect=%lld P%d: mid-roll pull on %s (%.0f cm)"), F->EffectId, P + 1, *Pick->GetName(), Best);
		}
		else { RefundMagnet(G, P, F->EffectId); return; } // never waste it: energy back, card back in hand
	}
	++MidRollApplied;
}
bool UZoogiCardSubsystem::MagnetTargetNow(const AZoogiGameMode* G, int32 P) const
{
	const AZoogiBall* Ball = G && G->Balls.IsValidIndex(P) ? G->Balls[P].Get() : nullptr;
	if (!Ball) return false;
	TArray<FVector> Orbs;
	for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Orbs.Add(O->OrbLocation());
	return ZoogiCards::MagnetHasTarget(Ball->BallLocation(), Orbs, ZoogiCardFxTune::MagnetRange);
}

void UZoogiCardSubsystem::RefundMagnet(AZoogiGameMode* G, int32 P, int64 EffectId)
{
	Fx.Remove(EffectId);
	const bool bOk = RulesV.RefundCommitted(P, EffectId);
	if (bOk) ++MagnetRefunds;
	UE_LOG(LogZoogi, Log, TEXT("CARDS fx orb_magnet effect=%lld P%d: no orb within %.0f cm -> %s"), EffectId, P + 1, ZoogiCardFxTune::MagnetRange, bOk ? TEXT("refunded, card back in hand") : TEXT("refund failed (not committed)"));

}