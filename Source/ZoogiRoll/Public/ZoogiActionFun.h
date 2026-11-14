// Oct 29 night (Evan, build 23): "action, fun and timing" pack for the classic turn game.
//  #2  CPU turns: tap anywhere to fast-forward (2x) the rest of the CPU turn.
//  #5  Impact timing tap: a ring tightens on the target before your Zoogi hits it - tap on contact for PERFECT / GREAT
//      (extra knock speed + impact frame).
//  #6  Defender parry: a CPU shot about to hit your Zoogi shows a shield ring - tap on contact to BLOCK (less knockback).
//  #7  Boost pickups: a glowing gem spawns mid-roll; the first Zoogi to touch it gets HEAVY HIT or DOUBLE POINTS next turn.
//  #8  On fire: three scoring turns in a row set the Zoogi on fire (flames, crowd hype, stronger hits) until it misses.
//  #9  Final rounds: the last two rounds make every orb worth double.
//  #10 Highlight replay: at the end of each round the best moment of the round replays in slow motion from a side camera.
// (#1 early turn end, #3 short cine hold and #4 cut-in throttle live in the game mode / targeting / anime code.)
// Build 25 super power pack (ZoogiSuperFun.cpp): SUPER meter -> signature ULTIMATES (fired mid-roll, aimed ones in bullet time),
// POWER CLASH tap-offs, ultimates breaking igloos / snowman, AWAKENED form, FUSION bursts, ARENA EVENTS, ULTIMATE FINISH replays.
// Toggles: -ZoogiNoClash, -ZoogiNoArenaEvents.
// Toggles: -ZoogiNoActionFun (all of the above), -ZoogiNoTimingTap, -ZoogiNoBoosts, -ZoogiNoReplay. Test: -ZoogiFunTest.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiTypes.h"
#include "ZoogiActionFun.generated.h"

class AZoogiGameMode;
class AZoogiBall;
class AHUD;
class UCanvas;
class UNiagaraComponent;
class UStaticMeshComponent;
class AZoogiIglooWall;
class USkeletalMeshComponent;
class UTexture2D;
/** b135 (Evan): the SUPER meter is the AWAKEN meter (full -> SHOWTIME at aim). False with -ZoogiOldMeters. */
ZOOGIROLL_API bool ZoogiAwakenMeter();

UCLASS()
class ZOOGIROLL_API UZoogiFunSys : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static void ParryShield(UWorld* W, AActor* Def); // b63: Hovl wood shield dome over the defender (parry / BLOCK)
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	static UZoogiFunSys* Get(const UObject* Ctx);

	/** Driven every frame by AZoogiShowTicker. */
	void FunTick(float Dt);
	/** A pointer press that is not over UI (player controller). True = used (timing tap, replay skip, fast-forward). */
	bool OnTap(const FVector2D& Screen = FVector2D(-1.f, -1.f));
	/** b120: the superpower bullet-time target pick is open / a release (tap, not a drag) picks there */
	bool UltAimOn() const { return bUltAim; }
	void UltAimRelease(const FVector2D& Screen);
	/** Every Zoogi sphere hit (AZoogiBall::OnSphereHit). */
	void OnBallContact(AZoogiBall* Self, AActor* Other);
	/** BeginTurn / SetStage(Resolve) hooks. */
	void OnTurnBegin(AZoogiGameMode* G, int32 Player);
	void OnTurnResolved(AZoogiGameMode* G, int32 Player, int32 Points, int32 KOs);
	/** Score multipliers (final rounds, DOUBLE boost). */
	int32 OrbPointMul(const AZoogiGameMode* G, int32 Scorer) const;
	int32 KOPointMul(const AZoogiGameMode* G, int32 Scorer) const;
	bool IsFinalRounds(const AZoogiGameMode* G) const;
	/** EndTurn at a round end: true = the highlight replay started (EndTurn waits until it is over). */
	bool StartReplay(AZoogiGameMode* G, bool bRoundEnd = true);
	bool IsReplaying() const { return Rep.bOn; }
	bool IsActive(const AZoogiGameMode* G) const;
	/** Build 25: the SUPERPOWER button / TryPower fire the signature ultimate when the SUPER meter is full (during the roll). */
	bool TryUltimate(AZoogiGameMode* G);
	/** b101 film / tests: fill a player's super meter. */
	void FillMeterForTest(AZoogiGameMode* G, int32 Player) { for (int32 k = 0; k < 6 && Meter(Player) < 100.f; ++k) AddMeter(G, Player, 100.f, TEXT("test fill")); }
	bool UltimateReady(const AZoogiGameMode* G, int32 Player) const;
	float Meter(int32 Player) const;
	/** b135: AWAKEN meter - full, spend (SHOWTIME), add (card casts) */
	bool AwakenFull(int32 Player) const;
	void SpendAwaken(int32 Player);
	void AddAwakenMeter(AZoogiGameMode* G, int32 Player, float Amount, const TCHAR* Why) { AddMeter(G, Player, Amount, Why); }
	/** A knockout during an ultimate this turn: EndTurn plays the ULTIMATE FINISH replay. */
	bool WantsFinisher() const { return bFinisherPending && Cur.Frames.Num() > 10; }

private:
	enum class EBoost : uint8 { None, Heavy, Double };
	struct FRecFrame { float T = 0.f; TArray<FTransform> X; TArray<uint8> Vis; };
	struct FRec
	{
		TArray<TWeakObjectPtr<AActor>> Actors;
		TArray<FRecFrame> Frames;
		float ImpactT = -1.f;
		FVector Dir = FVector::ForwardVector;
		TWeakObjectPtr<AActor> Shooter, Other;
		int32 Player = -1;
		float Score = 0.f;
		bool bAction = false;
		FString Title;
		void Reset() { Actors.Reset(); Frames.Reset(); ImpactT = -1.f; Dir = FVector::ForwardVector; Shooter.Reset(); Other.Reset(); Player = -1; Score = 0.f; bAction = false; Title.Reset(); }
	};
	struct FSnap { TWeakObjectPtr<AActor> A; FTransform X; bool bHidden = false, bSim = false; ECollisionEnabled::Type Coll = ECollisionEnabled::NoCollision; FVector V = FVector::ZeroVector, W = FVector::ZeroVector; };
	struct FReplay
	{
		bool bOn = false;
		FRec Clip;
		float T = 0.f, T0 = 0.f, T1 = 0.f, Age = 0.f;
		TArray<FSnap> Snaps;
		TWeakObjectPtr<AActor> Cam, PrevView;
		FVector CamP = FVector::ZeroVector, Focus = FVector::ZeroVector;
		bool bCamInit = false;
		bool bFinisher = false;
		float SideSign = 1.f;
		int32 Shots = 0;
	};
	struct FCallout { FString Text, Sub; FLinearColor Col; float Age = 0.f, Life = 1.4f; float Size = 1.f; int32 Speaker = -1; FString Line; int32 Band = 0; };
	struct FPendMul { TWeakObjectPtr<AActor> A; float Mul = 1.f; };
	struct FPlayerFun { int32 Streak = 0; bool bFire = false; EBoost Pending = EBoost::None; TWeakObjectPtr<UNiagaraComponent> FireTrail; float Meter = 0.f; bool bAwake = false; FVector VisualScale = FVector::ZeroVector; int32 ClashWins = 0; float HP = 100.f; int32 DuelReserve = 2; }; // b90: duel HP (kept for the match); b96: 2 SP duel reserve
	// build 27: the clash is a fist fight - both Zoogis burst into their ultimate forms and trade punches while the world is frozen
	struct FFighter { TWeakObjectPtr<AActor> Actor; TWeakObjectPtr<USkeletalMeshComponent> Mesh; int32 Form = -1; float T = 0.f, Dur = 1.f, HitAt = -1.f; int32 Punch = 0; FString Move; TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden; };
	struct FClash
	{
		bool bOn = false; float Bar = 0.f; bool bGo = false; float Age = 0.f, FxT = 0.f, RateA = 0.f, RateD = 0.f, AccA = 0.f, AccD = 0.f, TestAcc = 0.f; int32 TapsA = 0, TapsD = 0; bool bHumanAtt = false, bHumanDef = false; FVector At = FVector::ZeroVector; TWeakObjectPtr<AZoogiBall> Att, Def;
		FFighter F[2]; FVector Axis = FVector::ForwardVector, Center = FVector::ZeroVector; float AxisYaw = 0.f, Ratio = 0.5f, Gap = 60.f, Height = 180.f, Hit = 0.f, TapA = -10.f, TapD = -10.f, FinishAge = 0.f;
		bool bField = false; // b104: classic duel played on the field (no forms / duel camera)
		bool bFinish = false, bAttWins = true, bFinishHit = false, bCamFlip = false, bMeasured = false, bCamHigh = false; int32 Hits = 0; FVector2D HitScreen = FVector2D(-1.f, -1.f); TWeakObjectPtr<AActor> Cam, PrevView;
		// b66 (Evan): DUEL - a redirect into a rival = RPG-stance cutscene: intro, action menu (ATTACK / SKILL / GUARD / ITEM),
		// the move + a REACTION timing ring (on time = add-on), the rival's response, then the normal finisher / loser crash
		bool bDuel = false, bDPreReact = false, bDQteDone = false; int32 DStage = 0, DChoice = -1, DGrade = -1; float DAge = 0.f, DQteAt = 1.6f; FString DMove, DMoveSub;
		bool bSplit = false; // b68: the split-screen awakening clips are up
		bool bDCounter = false; // b67 (Evan): the rival countered on time -> its anime, then a classic clash
		int32 DHover = -1, DCpuWant = 0, DAttG = -1, DDefG = -1; float DCpuPickAt = 2.f, DCpuTapAt = -1.f, DDefTapAt = -1.f; // b67: the CPU goes through the duel menu + timing taps
		bool bV2 = false, bVGraded = false, bVDefend = false, bVCounter = false, bVLanded = false, bVCountered = false; int32 VPick = -1, VRivalWant = 3; float VGradeAt = 0.f; float VDmgA = 0.f, VDmgD = 0.f; // b90 duel v2 (b94: damage taken each side)
	};
public:
	static constexpr float ClashDilation = 0.01f; // world speed during the clash fight (the forms run at 1 / this)
	static constexpr float ClashReadySecs = 0.8f;  // Oct 30 (Evan): "READY..." - taps count only after TAP!
	static constexpr float ClashFightSecs = 3.8f;  // real seconds incl. the READY beat (3.0 s of punch-for-punch tapping)
	static constexpr float ClashFinishSecs = 1.5f; // then the finisher (winner signature move, loser hit + fall)
	static constexpr int32 ClashPoints = 50; // scoreboard points for the clash winner
	static FLinearColor FormColor(int32 Form);
	static FString FormSignature(int32 Form);
	/** Build 27: what the REACTION button does right now: 0 = nothing for FunSys (hop), 1 = clash taps, 2 = timing tap (strike), 3 = timing tap (block). */
	int32 TapButtonMode(const AZoogiGameMode* G) const;
	bool ClashOn() const { return Clash.bOn; }
	bool SpecialCamOn() const { return Clash.bOn || Flung.Actor.IsValid() || Rep.bOn || ReactCam.IsValid() || DefWin.bOn; } // b98b; b106 + the defend window
	bool DefendWindowOn() const { return DefWin.bOn; }
	/** b106: classic matches - the defend window instead of duels (story missions keep the duel scene). */
	static bool ClassicDefendMode(const AZoogiGameMode* G);
	double LastDuelEndAt = 0.0; // b98b: FPlatformTime seconds of the last duel / flung ending
	/** b90: duel HP (100 per Zoogi, kept for the match); b90c: critical hits take 10. 0 HP = KNOCKOUT (blasted off, back at 100). */
	float PlayerHP(int32 P) const { return Players.IsValidIndex(P) ? Players[P].HP : 100.f; }
	void HpDamage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* Why);
	bool DuelOn() const { return Clash.bOn && Clash.bDuel; }
	float AwakenGlowK = 0.f; TWeakObjectPtr<class APostProcessVolume> AwakenGlowVol; // b67: awaken = brighter arena glow
	/** b66: ZoogiRedirect starts a duel when its redirect meets the targeted rival. */
	void StartDuel(AZoogiGameMode* G, AZoogiBall* Att, AZoogiBall* Def);
	/** b66: callouts from outside FunSys (redirect combo). */
	void PublicCallout(const FString& Text, const FString& Sub, const FLinearColor& Col, float Life = 1.4f, float Size = 1.f) { Callout(Text, Sub, Col, Life, Size); }
	/** b58: the clash loser flying off in its form (the edge hang leaves it alone). */
	bool IsFlung(const AZoogiBall* B) const { return B && Flung.Ball.Get() == B; }
	int32 ClashWins(int32 Player) const { return Players.IsValidIndex(Player) ? Players[Player].ClashWins : 0; }
	bool IsClashFighter(int32 Player) const;
	/** Build 28: Evan's ultimate-form face art for this Zoogi (frost / white-gold fox / ember / lightning); null if missing. */
	UTexture2D* UltAvatar(const AZoogiBall* B);
	static int32 AvatarIndex(const AZoogiBall* B);
	/** b62: the Zoogi's own Tripo ultimate-form mesh (classic uses these, not the bought Aurex). */
	static class USkeletalMesh* FormMeshFor(const AZoogiBall* B);
	/** True while that player's Zoogi is out of the ball in its ultimate form (clash fighter / ultimate strike). */
	bool IsTransformed(int32 Player) const;
private:
	void SpawnFighter(int32 Side, AZoogiBall* B, int32 Form);
	void PlayFighter(int32 Side, const FString& Move, float Rate, bool bLoop, float Start = 0.f);
	void TickFighters(AZoogiGameMode* G, float RealDt);
	void FinishClash(AZoogiGameMode* G);
	void EndFighters();
	void DrawClash(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);
	void TickDuel(AZoogiGameMode* G, float RealDt);
	void DuelChoose(AZoogiGameMode* G, int32 Choice);
	void DuelTap(AZoogiGameMode* G);
	void DrawDuel(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);
	// b90 duel v2 (ZoogiDuelV2.cpp): circle match -> SP -> move -> the rival's SP answer -> HP / knockout
	void TickDuelV2(AZoogiGameMode* G, float RealDt);
	void DrawDuelV2(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);
	void DuelV2Tap(AZoogiGameMode* G);
	void DuelV2Pick(AZoogiGameMode* G, int32 i);
	void DuelV2Damage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* What);
	void DuelV2End(AZoogiGameMode* G, bool bFought);
	void HpKnockout(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn);
	void DuelV2GuardForm(AZoogiGameMode* G, AZoogiBall* Def);
	// b106 defend window (ZoogiDefendWin.cpp)
	struct FDefWin { bool bOn = false; TWeakObjectPtr<AZoogiBall> A, R; double T0 = 0.0; bool bDecided = false, bDefend = false, bContact = false, bCamPlaced = false; double ContactAt = 0.0, GameT0 = 0.0; float SavedMass = 0.f, CpuAt = -1.f, GameLimit = 0.6f; FVector CamSide = FVector::RightVector; TWeakObjectPtr<AActor> Cam;
		// b107b: the timing ring, black and white, DIRECT / AROUND, the loser's fly-back
		int32 Stage = 0, Grade = -1, Choice = -1; float CpuTapAt = -1.f, CpuChoiceAt = -1.f; double GradeAt = 0.0, BwAt = 0.0; bool bBw = false, bAroundOk = false, bLoserDone = false;
		FBox2D Btn[2] = { FBox2D(ForceInit), FBox2D(ForceInit) }; FBox2D GuardBtn = FBox2D(ForceInit); };
	FDefWin DefWin;
	bool bDefWinThisShot = false;
	void TickDefendWindow(AZoogiGameMode* G, float RealDt);
	void DefendNow(AZoogiGameMode* G);
	void EndDefendWindow(AZoogiGameMode* G, const TCHAR* Why);
	float DefWinDilation() const;
	void DefWinBW(bool bOn);
	AActor* DefWinAroundTarget(AZoogiGameMode* G) const;
	void DefWinRingTap(AZoogiGameMode* G);
	void DefWinChoose(AZoogiGameMode* G, int32 Choice);
	void DefWinFlyLoser(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn);
	void DefWinTap(AZoogiGameMode* G, const FVector2D& Screen);
	void DrawDefendWindow(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U);
	int32 DuelSp(AZoogiGameMode* G, int32 P) const;
	bool DuelSpend(AZoogiGameMode* G, int32 P, int32 N);
	float DuelHP(int32 P) const;
	FBox2D DuelBtn[4];
	bool bCpuStrikeRolled = false, bCpuStrikeYes = false, bClashTestOn = false;
	// build 28 (Evan): after a power clash the loser flies off in its ultimate form (the body, not the ball) until it settles
	struct FFlung { TWeakObjectPtr<AActor> Actor; TWeakObjectPtr<USkeletalMeshComponent> Mesh; TWeakObjectPtr<AZoogiBall> Ball; TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden; int32 Form = -1; float Age = 0.f, Spin = 0.f, Calm = 0.f, Half = 90.f, Yaw = 0.f; TWeakObjectPtr<AActor> Cam, PrevView; FVector CamOff = FVector::ZeroVector;
		TWeakObjectPtr<AActor> Pup; FVector Away = FVector::ForwardVector; bool bCrash = false;
		bool bSplat = false; float SplatAge = 0.f; TWeakObjectPtr<AActor> SplatPup; FVector SplatAt = FVector::ZeroVector, SplatN = FVector::ForwardVector; }; // b67: smashed flat on the stands // b58: Pup = the backstroke puppet (AZoogiAurexPuppet) of the same form
	void ReleaseFlungCam();
	FFlung Flung;
	TMap<FString, float> NoteSeen; // Oct 30: lower-left Persona-style note panels, first-seen time (slam-in)
	TArray<TWeakObjectPtr<USceneComponent>> ClashHiddenFx; // build 29: other FX hidden while the forms fight (the frozen bursts glared)
	bool bCutInStrike = false;                              // build 29: the form shown during an anime cut-in
	void HideFxAround(const FVector& At, float Radius, const AActor* Keep0 = nullptr, const AActor* Keep1 = nullptr);
	void RestoreHiddenFx();
	static void AddFormLight(AActor* A, const FLinearColor& Tint);
	void StartFlung(int32 Side);
	void TickFlung(AZoogiGameMode* G, float Dt);
	void EndFlung(bool bFx);
	UPROPERTY() TArray<TObjectPtr<UTexture2D>> UltAvis;
	struct FDelayedHit { TWeakObjectPtr<AZoogiIglooWall> Piece; float At = 0.f; FVector Dir = FVector::ForwardVector; TWeakObjectPtr<AZoogiBall> By; };
	static EZoogiPower SignaturePower(const AZoogiBall* B);
	static const TCHAR* UltName(EZoogiPower P);
	static FLinearColor UltColor(EZoogiPower P);
	void AddMeter(AZoogiGameMode* G, int32 Player, float Amount, const TCHAR* Why);
	AActor* PickUltTarget(AZoogiGameMode* G, AZoogiBall* B, const AActor* Exclude, bool bPreferBalls) const;
	void Blast(AZoogiGameMode* G, const FVector& At, float R, float Base, float Extra, float Up, AZoogiBall* By, bool bBalls, bool bOrbs);
	void DamageArena(AZoogiGameMode* G, const FVector& At, float R, AZoogiBall* By, int32 SnowmanHits);
	void UltBanner(AZoogiGameMode* G, AZoogiBall* B, EZoogiPower P);
	void FireUltimate(AZoogiGameMode* G, AZoogiBall* B, EZoogiPower P, AActor* Target);
	void UltContact(AZoogiGameMode* G, AZoogiBall* Sh, AActor* Hit);
	void TickUltimate(AZoogiGameMode* G, float Dt, float RealDt);
	void StartClash(AZoogiGameMode* G, AZoogiBall* Att, AZoogiBall* Def);
	void TickClash(AZoogiGameMode* G, float RealDt);
	void EndClash(AZoogiGameMode* G);
	void SuperContact(AZoogiGameMode* G, AZoogiBall* Sh, AActor* Hit, bool bFirst);
	void SetAwakened(AZoogiGameMode* G, int32 Player, bool bOn);
	void StartArenaEvent(AZoogiGameMode* G);
	void EndArenaEvent();
	void TickArenaEvent(AZoogiGameMode* G, float Dt);
	// ultimates (per shot / turn)
	bool bUltThisShot = false, bUltThisTurn = false, bUltAim = false, bFinisherPending = false;
	float UltAimAge = 0.f;
	float UltAimLimit = 3.f;     // Oct 30 (Evan): bullet-time pick - 5 s the first time a player uses it in a match, then 3 s
	TSet<int32> UltAimSeen;
	/** Oct 30 (Evan, clash easier to win when behind): one punch on the tug-of-war bar (Clash.Bar, + = attacker side). */
	void ClashPush(bool bAtt);
	EZoogiPower UltPower = EZoogiPower::WolfPack;
	int32 UltBounces = 0;
	float UltRedirectAt = -1.f, UltLastBounce = -10.f, UltHomingUntil = 0.f, UltHitMul = 1.f;
	TWeakObjectPtr<AActor> UltLastHit, UltHoming;
	float UltBannerAge = 10.f, BlackoutAge = 10.f;
	FString UltBannerText, UltBannerSub;
	FLinearColor UltBannerCol = FLinearColor::White;
	TArray<FDelayedHit> DelayedHits;
	FClash Clash;
	bool bClashThisShot = false, bFusedThisShot = false;
	float ResolveHoldT = 0.f;
	bool bResolveAfterClash = false; // b62: next turn once the clash, the loser's flight and its crash clip are over
	// arena events
	int32 EventKind = 0, EventSerial = 0, EventRound = -1;
	FString EventName;
	float MeteorNext = 0.f, SavedGravity = 0.f;
	bool bSavedWorldGravitySet = false, bGravityChanged = false;
	float CurDil = 1.f;
	// build 26: ULTIMATE STRIKE (ZoogiStrikeFun.cpp) - the ball bursts into its Tripo ultimate form right before a hit
	struct FStrike { bool bOn = false; TWeakObjectPtr<AActor> Actor; TWeakObjectPtr<USkeletalMeshComponent> Mesh; TWeakObjectPtr<AZoogiBall> Ball; TWeakObjectPtr<AActor> Target; float Age = 0.f, Life = 1.f, Yaw = 0.f; bool bStand = false; int32 Form = -1; FString Move; TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden; TWeakObjectPtr<AActor> TestCam, PrevView; };
	FStrike Strike;
	bool bStrikeThisShot = false, bStrikeTest = false;
	int32 StrikeShots = 0;
	static int32 FormIndex(const AZoogiBall* B);
	FString PickMove(AZoogiGameMode* G, AZoogiBall* B, AActor* Target, bool bHeavy) const;
	void StartStrike(AZoogiGameMode* G, AZoogiBall* B, AActor* Target, float TimeToHit, const FString& Move, bool bStand, float Life);
	void TickStrike(AZoogiGameMode* G, float Dt);
	void EndStrike(bool bFx);
	void MaybeStrike(AZoogiGameMode* G);
	void PreloadForms();
	UPROPERTY() TArray<TObjectPtr<UObject>> FormKeep;
	bool bFormsLoaded = false;
	bool bStrikeLogged = false;

	void DrawHud(AHUD* HUD, UCanvas* Canvas);
	void Callout(const FString& Text, const FString& Sub, const FLinearColor& Col, float Life = 1.4f, float Size = 1.f, int32 Speaker = -1, const FString& Line = FString()); // b63: Speaker / Line = that Zoogi's speech bubble
	void ResetMatch(AZoogiGameMode* G);
	void NewShot(AZoogiGameMode* G);
	void Predict(AZoogiGameMode* G);
	void Grade(AZoogiGameMode* G, float DTap);
	void Ignite(AZoogiGameMode* G, int32 Player);
	void Extinguish(AZoogiGameMode* G, int32 Player, bool bQuiet);
	void TickBoostPickup(AZoogiGameMode* G, float Dt);
	void SpawnBoostPickup(AZoogiGameMode* G);
	void KillPickup(bool bPoof);
	void Record(AZoogiGameMode* G);
	void CommitRec();
	void TickReplay(AZoogiGameMode* G, float RealDt);
	void EndReplay(AZoogiGameMode* G);
	void SetFast(bool bOn);
	void Shot(AZoogiGameMode* G, const FString& Name, float Delay = 0.f);
	float Now() const;

	FDelegateHandle HudHandle;
	TArray<FPlayerFun> Players;
	TArray<FCallout> Callouts;
	TArray<FPendMul> PendMul;

	// timing tap (per shot)
	int32 Mode = 0;             // 0 none, 1 strike (human shooter), 2 parry (CPU shot heading for a human Zoogi)
	float PredT = -1.f;         // seconds to the predicted first contact
	TWeakObjectPtr<AActor> PredTarget;
	TWeakObjectPtr<AZoogiBall> Shooter;
	float TapT = -1.f, ContactT = -1.f;
	TWeakObjectPtr<AActor> ContactOther;
	bool bTapUsed = false, bGraded = false, bContactBoosted = false;
	int32 LastGrade = 0;        // 0 none, 1 good, 2 perfect (for the replay score)
	int32 PrevPhase = -1;
	float ShotAge = 0.f;
	int32 RingShots = 0, GradeShots = 0;
	// b55: incoming hit close-up (Mode 2 BLOCK!): slow-mo scaled so the contact lands ~5 real s after the prompt, side camera on the hit
	float ReactT0 = -1.f, ReactDil = 1.f;
	bool ReactCloseShot[2] = { false, false }; // b181: the close-up frames were shot (test)
	TWeakObjectPtr<class ACameraActor> ReactCam;
	TWeakObjectPtr<AActor> ReactPrev;
	TWeakObjectPtr<class AZoogiAurexPuppet> ReactPup; // b58: the victim's Aurex in a guard during the slow cut
	void UpdateReact(AZoogiGameMode* G, float RealDt);

	// boosts
	int32 ActiveBoostPlayer = -1;
	EBoost ActiveBoost = EBoost::None;
	UPROPERTY() TObjectPtr<AActor> Pickup;
	EBoost PickupKind = EBoost::None;
	FVector PickupAt = FVector::ZeroVector;
	float PickupAge = 0.f, PickupSpawnAt = -1.f;
	int32 BoostShots = 0;

	// fire
	float FireFxT = 0.f;

	// final rounds
	bool bFinalAnnounced = false; bool bRound1Announced = false; // b66
	int32 LastRound = 0;

	// fast-forward (CPU turns)
	bool bFast = false, bDilApplied = false;
	int32 FastLevel = 1; // b192 (Evan): a second tap on a CPU turn = 3x
	int32 CpuTurnsSeen = 0;

	// replay
	FRec Cur, Best;
	bool bRecording = false;
	float RecT = 0.f, RecNext = 0.f;
	FReplay Rep;

	bool bTest = false;
	int32 TestCpuTaps = 0;
};
