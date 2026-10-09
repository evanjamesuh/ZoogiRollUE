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
//   RailRelease: (T3, ref rail_release_timing) a Zoogi entering a Launch Rail rides it; the release window decides the
//                launch vector: EARLY high launch / CENTER horizontal / LATE downward bank toward the centre. Game time,
//                no hold, no dilation: a real velocity change in the normal simulation. Window sizes in [ZoogiRoll.Aerial].
//   SkyBumper  : route challenge; with aerial props, a Sky Bumper rebound (reflection x SkyBumperRebound) then a body
//                contact. HaloBank with props: a Resonance Halo pass or rim rebound counts as the bank.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiMiniGame.generated.h"

class UZoogiBattleStateMachine;
class AZoogiAerialProp;

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

UCLASS()
class ZOOGIROLL_API UZoogiMiniGameRailRelease : public UZoogiMiniGame
{
	GENERATED_BODY()
public:
	UZoogiMiniGameRailRelease() { Kind = EZoogiChallenge::RailRelease; }
	int32 Body = -1;
	TWeakObjectPtr<AZoogiAerialProp> Rail;
	FVector RailForward = FVector::ForwardVector;
	float Elapsed = 0.f;
	float EntrySpeed = 0.f;
	bool bCPU = false;
	bool bSegment = false;              // a rail_release combo is pending for this Zoogi (timeout shatters it)
	bool bReleaseRequested = false;
	EZoogiRailWindow Window = EZoogiRailWindow::None;
	EZoogiRailWindow CPUWindow = EZoogiRailWindow::Center; // the window a CPU releases in (center unless a map prop plan chose one)
	static EZoogiRailWindow WindowAt(const FZoogiBattleTuning& T, float Time);
	/** Deterministic launch velocity for a window (rail forward, the Zoogi position for the bank side, entry speed). */
	static FVector LaunchVelocity(const FZoogiBattleTuning& T, EZoogiRailWindow W, const FVector& Forward, const FVector& Pos, float Speed);
	virtual bool SubmitInput(UZoogiBattleStateMachine* SM, int32 Choice, const FString& Source) override;
	/** Game-time tick (GameDt = 0 while paused). Success once released, Timeout after the late window. */
	EZoogiChallengeResult TickGame(UZoogiBattleStateMachine* SM, float GameDt);
	FString Apply(UZoogiBattleStateMachine* SM) const;
	virtual EZoogiChallengeResult Evaluate(const UZoogiBattleStateMachine* SM, const FZoogiPendingSegment& Seg, const FZoogiGameplayTruthRecord& T, FString& OutDetail) const override;
};
