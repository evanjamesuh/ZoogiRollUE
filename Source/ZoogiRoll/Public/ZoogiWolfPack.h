// Nov 1 (Evan: "Let's perfect Wolfgang, give me 20 improvements ..." -> "All"): the Wolf Pack super power, rebuilt.
//   1 Howl summon      : 0.2x slow-mo, P5 cut-in "WOLF PACK!", spirit burst + howl (S_Howl)
//   2 Wolf clones      : Wolfgang wolf-head marbles in the pack colour, tail streak, glowing paw prints
//   3 V formation      : the pack bursts out behind you in a V and runs with you, then peels off
//   4 Howl chord       : each wolf howls at its own pitch
//   5 PACK HUNT        : a wolf knockout gets a 1 s slow-mo kill-cam + stamp
//   6 Paint targets    : during the slow-mo tap / swipe up to N targets (CPU: auto)
//   7 Alpha            : the first target gets the Alpha (bigger, x1.5 hit, wider blast)
//   8 Pack Tactics     : 2+ wolves on the same rival within 1.5 s = PILE-ON (x2 shove)
//   9 Recall           : tap SUPERPOWER again -> the wolves dash back to you
//  10 Charge           : hold SUPERPOWER -> the wolves charge (blast up to x1.8), release = blow now
//  11 Scent trail      : auto targets are picked along the path you rolled
//  12 Parry            : a rival can pop a wolf with a timed REACTION tap (CPU: chance)
//  13 Arena synergy    : ice wolves slide (Frozen), fire wolves blast harder (Volcanic), bumper howls
//  14 Pack levels 1-5  : XP from wolf hits (+hunt time, +blast, rebound, 4th wolf)
//  15 Pack skins       : Frost / Inferno / Shadow / Golden Alpha / Cyber / Spirit Fox (SHOP > WOLF PACKS)
//  16 Alpha howl       : the round's pack-KO leader gets the ALPHA HOWL celebration
//  17 Badges           : TRIPLE HUNT, BLOOD MOON, LONE WOLF, ALPHA STRIKE (profile)
//  18 Blood Moon       : weekends (or -ZoogiBloodMoon): red sky, every pack gets a 4th wolf
//  19 Dire Wolf        : 2v2 - summon right after your partner did = one giant DIRE WOLF + 2 wolves
//  20 Pack HUD         : wolf icons light up per hit, PACK SCORE tally
// Toggles: -ZoogiOldWolfPack (the Oct 29 pack), -ZoogiBloodMoon / -ZoogiNoBloodMoon. Test: -ZoogiWolfTest (+ -ZoogiShots=<dir>).
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiWolfPack.generated.h"

class AZoogiGameMode;
class AZoogiBall;
class AZoogiClone;
class AHUD;
class UCanvas;
class ACameraActor;

namespace ZoogiWolf
{
	struct FSkin { FName Id; const TCHAR* Name; FLinearColor Tint; float Pitch; int32 Price; int32 PriceKind; }; // PriceKind 0 gold, 1 gems
	ZOOGIROLL_API const TArray<FSkin>& Skins();
	ZOOGIROLL_API const FSkin& SkinById(FName Id);
	/** Pack level 1..5 for this XP; OutBase / OutNext = XP at this level / the next (Next = Base at max). */
	ZOOGIROLL_API int32 LevelForXP(int32 XP, int32* OutBase = nullptr, int32* OutNext = nullptr);
	ZOOGIROLL_API bool BloodMoon();
	ZOOGIROLL_API bool On();
	/** Summons by the human this match (LONE WOLF badge, read by the profile at match end). */
	ZOOGIROLL_API int32 HumanSummonsThisMatch();
	ZOOGIROLL_API void AwardBadge(const UObject* Ctx, FName Badge);
	ZOOGIROLL_API FString BadgeTitle(FName Badge);
}

UCLASS()
class ZOOGIROLL_API UZoogiWolfPackSys : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiWolfPackSys, STATGROUP_Tickables); }
	static UZoogiWolfPackSys* Get(const UObject* Ctx);

	/** FireInstantPower (WolfPack): starts the howl / paint window, then releases the pack. */
	void BeginSummon(AZoogiGameMode* G, AZoogiBall* Caster);
	/** b119 (Evan: "bring the Wolf Pack effect from build 117 to PRECISION as its skill"): N purple missile orbs - the same 30 s target
	 *  pick, fired straight from the Zoogi, each blows on impact, the third hits harder. */
	void BeginPrecision(AZoogiGameMode* G, AZoogiBall* Caster, int32 N);
	bool bShot = false, bShotNext = false; // b119: this summon is a PRECISION volley (missiles), not the pack
	bool bFieldPack = false; // b119 (Evan): the pack is on the field from the summon; each picked target sends the closest wolf
	/** b119c: wolves waiting with each player (owner -> orbs that follow it until sent or the player is knocked out) */
	TMap<int32, TArray<TWeakObjectPtr<AZoogiClone>>> FieldWolves;
	/** b119c: tops the player's waiting wolves up to Want (Wolf Pack cast) */
	void SummonField(AZoogiGameMode* G, AZoogiBall* B, int32 Want);
	void TickField(AZoogiGameMode* G, float RealDt);
	/** Painting or wolves on the field. */
	bool PackActive() const { return Phase != EPhase::Idle; }
	/** The human owns the live pack (SUPERPOWER = recall / charge). */
	bool HumanPackOut() const;
	int32 PackOwnerIndex() const { return Owner; }
	bool IsPainting() const { return Phase == EPhase::Paint; }
	/** b87: a summon over someone else's live pack clears that pack first (one pack at a time; the bank was lost before). */
	void ForceClear();
	/** b87 (Evan: "if I have multiple I should be able to select targets up to how many I have"): banked wolves spent by this
	 *  summon (ZoogiSkills::WolfGo sets it) - the paint takes up to that many targets and the pack has at least that many wolves. */
	int32 BankedWolves = 0;
	int32 PaintMax = 3;
	/** Screen taps (player controller): paint targets / parry. True = used. */
	bool OnTap(const FVector2D& Screen);
	void OnDrag(const FVector2D& Screen);
	/** SUPERPOWER while the human's pack is out: press / release (pad), tap (keyboard). */
	void OnPowerPress();
	void OnPowerRelease();
	void OnPowerTap() { Recall(); }
	/** A wolf is about to hit the human's Zoogi: REACTION pops it. */
	bool ParryOpen() const;
	bool TryParry();
	// clone callbacks
	void OnCloneHit(AZoogiClone* C, AActor* Other);
	void OnCloneBump(AZoogiClone* C);
	void OnCloneGone(AZoogiClone* C, bool bDetonated);
	AActor* PickReboundTarget(AZoogiClone* C, const AActor* Exclude) const;

private:
	enum class EPhase : uint8 { Idle, Paint, Out };
	struct FToast { FString Text, Sub; FLinearColor Col; float Age = 0.f, Life = 1.6f, Size = 1.f; };
	struct FTouch { float At = -100.f; int32 Owner = -1; };
	struct FPile { int32 Hits = 0; float First = -100.f; bool bDone = false; };

	AZoogiGameMode* GM() const;
	float RealNow() const;
	void SetDil(float D);
	void RestoreDil();
	void Release();
	void SpawnPack();
	void Recall();
	void DetonateAll();
	void EndPack();
	void PackKO(AZoogiBall* Victim, int32 Owner);
	void StartKillCam(const FVector& At);
	void StartBird();   // b96d: top-down pick view
	void EndBird();
	int32 PaintNeed() const;
	void EndKillCam();
	void Toast(const FString& T, const FString& S, const FLinearColor& C, float Life = 1.6f, float Size = 1.f);
	void Howl(float Pitch, float Vol = 1.f);
	TArray<AActor*> Targets(const AZoogiBall* For) const;
	FVector2D Project(const FVector& W, bool& bOk) const;
	void PaintPick(const FVector2D& Screen, float MaxPx);
	void PickActor(AActor* Best); // b120: one target painted (tap or the aim's locked target)
	void DrawHud(AHUD* HUD, UCanvas* Canvas);
	void DrawWolfIcon(UCanvas* C, const FVector2D& At, float Size, const FLinearColor& Col, bool bCrown, bool bDim) const;
	void TestTick(AZoogiGameMode* G);
	void ApplyBloodMoon(AZoogiGameMode* G);
	FLinearColor PackTint(const AZoogiBall* B) const;

	EPhase Phase = EPhase::Idle;
	float PhaseT0 = 0.f, PaintLimit = 1.5f;
	TWeakObjectPtr<AZoogiBall> Caster;
	int32 Owner = -1;
	bool bHumanOwner = false;
	int32 NumWolves = 3;
	bool bDire = false;
	TArray<TWeakObjectPtr<AActor>> Painted;
	TArray<TWeakObjectPtr<AZoogiClone>> Wolves;
	TArray<int32> WolfHits;      // per wolf: things hit
	TArray<uint8> WolfPopped;    // per wolf: parried
	int32 SummonHits = 0, SummonKOs = 0;
	TMap<TWeakObjectPtr<AZoogiBall>, FPile> Pile;
	TMap<TWeakObjectPtr<AZoogiBall>, FTouch> LastTouch;
	TSet<TWeakObjectPtr<AZoogiBall>> WasOut;
	TSet<TWeakObjectPtr<AZoogiClone>> CpuParryRolled;
	TSet<int32> PaintSeen;       // players who already had the long first paint window
	// time
	float SavedDil = 1.f; bool bDilSet = false; float DilUntil = -1.f; float DilValue = 1.f;
	// SUPERPOWER hold
	bool bHeld = false, bCharging = false; float PressAt = 0.f;
	// howl chord queue
	TArray<TPair<float, float>> HowlQueue; // real time, pitch
	// kill-cam
	TWeakObjectPtr<ACameraActor> KillCam; TWeakObjectPtr<AActor> PrevView; float KillCamEnd = -1.f;
	TWeakObjectPtr<ACameraActor> BirdCam; TWeakObjectPtr<AActor> BirdPrev; bool bBirdPick = false;
	// 2v2 Dire Wolf
	int32 LastSummoner = -1; float LastSummonAt = -100.f;
	// rounds / alpha howl
	int32 SeenRound = -1; bool bSeenGameOver = false; TMap<int32, int32> RoundPackKOs;
	// scent trail
	TArray<FVector> Trail; TWeakObjectPtr<AZoogiBall> TrailBall;
	// HUD
	float CutInAge = 10.f; FString CutInTitle, CutInSub; FLinearColor CutInCol = FLinearColor::White;
	TArray<FToast> Toasts;
	float TallyAge = 10.f; int32 TallyScore = 0; FLinearColor TallyCol = FLinearColor::White;
	bool bBloodDone = false;
	FDelegateHandle HudHandle;
	float LastBumpHowl = -10.f;
	float ParryFocusT0 = -1.f;   // b55: incoming wolf close-up (slow-mo, 5 s to decide)
	TWeakObjectPtr<class AZoogiAurexPuppet> ParryPup; // b58: the victim's Aurex guard during the parry slow cut
	float LastReal = 0.f;
	// test
	bool bTest = false; int32 TestStep = 0; float TestT = 0.f; FString ShotDir; int32 Shots = 0;
};
