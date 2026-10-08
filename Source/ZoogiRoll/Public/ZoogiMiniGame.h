// Battle Episode Engine (T3): constrained challenges ("mini-games"). Engine only: objective + evaluation from real
// physics truth; no art. A failed / timed-out challenge shatters its pending segment (UZoogiBattleStateMachine).
//
//   HaloBank   : bank off a surface (wall / bumper / ramp / piece / rock), THEN the first Zoogi / orb contact is the
//                declared target (lock or aim-preview contact at launch).
//   OrbLine    : the orbs on the launch route are committed at launch (aim-preview path corridor, else the straight
//                launch line); success if the shooter touches every committed orb.
//   AirReaction: offered only to an Awakened Zoogi while it is airborne (gameplay hold, also in Episode Off). Choices
//                Brace (horizontal speed x BraceDamp) / Redirect (turn toward the centre <= RedirectDeg), applied on
//                the resume frame. Success = the reacting Zoogi is not rung out. CPU decides deterministically; human
//                input times out (-> shatter, then ordinary resolution).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiMiniGame.generated.h"

class UZoogiBattleStateMachine;

UCLASS(Abstract)
class ZOOGIROLL_API UZoogiMiniGame : public UObject
{
	GENERATED_BODY()
public:
	EZoogiChallenge Kind = EZoogiChallenge::None;
	int32 Owner = -1;
	bool bInputOpen = false;
	float InputLeft = 0.f;
	EZoogiBattleState ReturnTo = EZoogiBattleState::PhysicsResolve;

	virtual bool CanEnter(const UZoogiBattleStateMachine* SM, int32 Player) const { return true; }
	virtual void Enter(UZoogiBattleStateMachine* SM, int32 Player, FZoogiPendingSegment& Seg);
	/** Returns true if the input was accepted. */
	virtual bool SubmitInput(UZoogiBattleStateMachine* SM, int32 Choice, const FString& Source) { return false; }
	/** Pending while waiting for input; Timeout when the input window ran out. */
	virtual EZoogiChallengeResult TickInput(UZoogiBattleStateMachine* SM, float RealDt);
	virtual EZoogiChallengeResult Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const;
	virtual void OnShatter(UZoogiBattleStateMachine* SM) {}
	EZoogiBattleState ReturnState() const { return ReturnTo; }

	/** Route challenges (HaloBank / OrbLine) evaluated from truth at Resolve. */
	static EZoogiChallengeResult EvaluateRoute(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail);
	/** OrbLine: commit the orbs on the launch route (preview path corridor, else the straight launch line). */
	static void CommitOrbLine(const UZoogiBattleStateMachine* SM, FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T);
	static UZoogiMiniGame* Create(UObject* Outer, EZoogiChallenge Kind);
};

UCLASS()
class ZOOGIROLL_API UZoogiMiniGameHaloBank : public UZoogiMiniGame
{
	GENERATED_BODY()
public:
	UZoogiMiniGameHaloBank() { Kind = EZoogiChallenge::HaloBank; }
	virtual EZoogiChallengeResult Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const override;
};

UCLASS()
class ZOOGIROLL_API UZoogiMiniGameOrbLine : public UZoogiMiniGame
{
	GENERATED_BODY()
public:
	UZoogiMiniGameOrbLine() { Kind = EZoogiChallenge::OrbLine; }
	virtual void Enter(UZoogiBattleStateMachine* SM, int32 Player, FZoogiPendingSegment& Seg) override;
	virtual EZoogiChallengeResult Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const override;
};

UCLASS()
class ZOOGIROLL_API UZoogiMiniGameAirReaction : public UZoogiMiniGame
{
	GENERATED_BODY()
public:
	UZoogiMiniGameAirReaction() { Kind = EZoogiChallenge::AirReaction; }
	int32 Body = -1;
	EZoogiAirChoice Choice = EZoogiAirChoice::None;
	bool bCPU = false;
	float CPUDelay = 0.f;
	virtual bool CanEnter(const UZoogiBattleStateMachine* SM, int32 Player) const override;
	virtual void Enter(UZoogiBattleStateMachine* SM, int32 Player, FZoogiPendingSegment& Seg) override;
	virtual bool SubmitInput(UZoogiBattleStateMachine* SM, int32 InChoice, const FString& Source) override;
	virtual EZoogiChallengeResult TickInput(UZoogiBattleStateMachine* SM, float RealDt) override;
	virtual EZoogiChallengeResult Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const override;
	/** Deterministic CPU choice from the ball's position / velocity. */
	EZoogiAirChoice DecideCPU(const UZoogiBattleStateMachine* SM) const;
	/** Applies the choice to the live body (call on the first unpaused frame). Returns a description. */
	FString Apply(UZoogiBattleStateMachine* SM) const;
};
