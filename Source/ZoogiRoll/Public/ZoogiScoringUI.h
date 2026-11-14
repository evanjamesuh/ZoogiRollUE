// Oct 22 (Evan / Gameplay & UI): scoring + HUD juice queue — first-bounce preview, shot callouts, combo 1/2/4,
// orb tally pops, timer pause while rolling, ghost marker, weight rating/bar, audio slider hooks, L/R mirror, personal stats.
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class AZoogiHUD;
class UWorld;

/** Coherent scoring / HUD juice layer. Visual + light ledger bonus only: never rewrites physics, aim math, camera,
 *  Aura/stash, Meadow/ZoogiOrb uassets, or A&P transition / fx-queue files.
 *  Tunables: [ZoogiRoll.ScoringUI] in DefaultGame.ini. -ZoogiNoScoringUI / -ZoogiScoringUILog. */
namespace ZoogiScoringUI
{
	enum class EShotGrade : uint8 { Miss = 0, Good, Great, Perfect };

	struct FCallout
	{
		FString Text;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.f;
		float Life = 1.1f;
		float Scale = 1.f;
	};

	struct FTallyPop
	{
		FVector World = FVector::ZeroVector;
		int32 Points = 0;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.f;
		float Life = 1.15f;
		int32 ComboStep = 1; // 1 / 2 / 4
	};

	struct FMatchStats
	{
		int32 Orbs = 0;
		int32 Stars = 0;
		int32 KOs = 0;
		int32 Falls = 0;
		int32 BestCombo = 0;
		int32 ComboBonus = 0;
		int32 Shots = 0;
		int32 Perfects = 0;
		int32 Greats = 0;
		int32 Goods = 0;
		int32 Misses = 0;
	};

	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API bool MirrorLayout(); // battle HUD L/R mirror (ini + GameUserSettings)
	ZOOGIROLL_API void SetMirrorLayout(bool bOn);
	ZOOGIROLL_API bool PauseClockWhileRolling();

	/** MassRatio -> LIGHT / MED / HEAVY (and 0..1 bar fill). */
	ZOOGIROLL_API float Weight01(const AZoogiBall* B);
	ZOOGIROLL_API const TCHAR* WeightLabel(float MassRatio);
	ZOOGIROLL_API int32 WeightClass(float MassRatio); // 1 Light, 2 Med, 3 Heavy
	/** Card id heuristic for Heavy Roll / Titan-style mass cards; else 0. */
	ZOOGIROLL_API int32 CardWeightClass(FName CardId);

	ZOOGIROLL_API EShotGrade GradePull(float Frac);
	ZOOGIROLL_API const TCHAR* GradeText(EShotGrade G);
	ZOOGIROLL_API FLinearColor GradeColor(EShotGrade G);

	/** Combo step for the Nth score event in the current shot window: 1, 2, then 4 thereafter. */
	ZOOGIROLL_API int32 ComboPointsForIndex(int32 Index1Based);

	ZOOGIROLL_API void ResetMatch();
	ZOOGIROLL_API void OnAimBegin(AZoogiGameMode* G);
	ZOOGIROLL_API void OnShotLaunch(AZoogiGameMode* G, float PullFrac);
	/** Orb / star scored: returns bonus points (1/2/4) to add on top of BasePts; fires tally pop + combo juice. */
	ZOOGIROLL_API int32 OnOrbScored(AZoogiGameMode* G, int32 Scorer, int32 BasePts, const FVector& Where, bool bStar);
	ZOOGIROLL_API void OnKnockout(AZoogiGameMode* G, int32 Scorer, const FVector& Where);
	ZOOGIROLL_API void OnFall(AZoogiGameMode* G);
	/** Capture ghost land position when the active shot settles (UI marker only). */
	ZOOGIROLL_API void OnShotSettled(AZoogiGameMode* G, const FVector& LandWorld);
	ZOOGIROLL_API void Tick(float Dt);

	ZOOGIROLL_API const TArray<FCallout>& Callouts();
	ZOOGIROLL_API const TArray<FTallyPop>& TallyPops();

	/** Portrait layout helpers for DrawScoringJuice (ClipY>ClipX or ForceRes / mode override). Tunables under [ZoogiRoll.ScoringUI]. */
	ZOOGIROLL_API bool IsPortraitLayout(float ClipX, float ClipY);
	ZOOGIROLL_API float CalloutAnchorY(float ClipX, float ClipY); // screen Y fraction for callout stack
	ZOOGIROLL_API float CalloutAnchorX(float ClipX, float ClipY); // screen X fraction (0.5 = center; side band via PortraitCalloutX)
	ZOOGIROLL_API float TallyDrawScale(float ClipX, float ClipY);
	ZOOGIROLL_API float TallyWorldZMul(float ClipX, float ClipY); // portrait: lower lift to clear mid-chrome
	ZOOGIROLL_API float TallyScreenYMin(float ClipX, float ClipY); // clamp pops below KO / top chrome (0 = off)
	ZOOGIROLL_API bool HasGhost();
	ZOOGIROLL_API FVector GhostWorld();
	ZOOGIROLL_API int32 CurrentComboStep(); // last awarded 1/2/4 (0 if none this shot)
	ZOOGIROLL_API int32 ComboStreak();      // count of score events this shot
	ZOOGIROLL_API const FMatchStats& Stats();

	/** First-bounce world point from existing aim preview (bank point, else path end). */
	ZOOGIROLL_API bool FirstBouncePoint(const AZoogiGameMode* G, FVector& Out);

	/** Audio slider helpers (persist under ZoogiRoll.MenuSettings like Clash settings). */
	ZOOGIROLL_API int32 GetAudioPct(const TCHAR* Key, int32 DefaultPct);
	ZOOGIROLL_API void SetAudioPct(const TCHAR* Key, int32 Pct);
	ZOOGIROLL_API void ApplyAudio();
}
