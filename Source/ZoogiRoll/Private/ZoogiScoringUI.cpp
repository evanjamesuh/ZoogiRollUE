// Oct 22: see ZoogiScoringUI.h
#include "ZoogiScoringUI.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "ZoogiPortraitCam.h"

namespace ZoogiScoringUI
{
	struct FTune
	{
		bool bEnabled = true;
		bool bMirrorLayout = false;
		bool bPauseClockWhileRolling = true;
		bool bFirstBounce = true;
		bool bShotCallouts = true;
		bool bComboPoints = true;
		bool bOrbTally = true;
		bool bGhostMarker = true;
		bool bWeightUI = true;
		bool bLog = false;
		float GoodLo = 0.35f;
		float GreatLo = 0.58f;
		float PerfectLo = 0.72f;
		float PerfectHi = 0.88f;
		float MissHi = 0.22f;
		// Portrait scoring juice (Oct 22 clutter fix): callouts were at ClipY*0.28 fighting KO at ~0.30; tally mid-screen.
		float CalloutY = 0.28f;
		float PortraitCalloutY = 0.42f;
		float PortraitCalloutX = 0.50f; // 0.5 center; set ~0.72 for a side band
		float PortraitTallyScale = 0.85f;
		float PortraitTallyWorldZMul = 0.65f;
		float PortraitTallyYMin = 0.36f; // screen Y clamp floor (below KO banner)
		FTune()
		{
			const TCHAR* S = TEXT("ZoogiRoll.ScoringUI");
			if (GConfig)
			{
				GConfig->GetBool(S, TEXT("bEnabled"), bEnabled, GGameIni);
				GConfig->GetBool(S, TEXT("bMirrorLayout"), bMirrorLayout, GGameIni);
				GConfig->GetBool(S, TEXT("bPauseClockWhileRolling"), bPauseClockWhileRolling, GGameIni);
				GConfig->GetBool(S, TEXT("bFirstBounce"), bFirstBounce, GGameIni);
				GConfig->GetBool(S, TEXT("bShotCallouts"), bShotCallouts, GGameIni);
				GConfig->GetBool(S, TEXT("bComboPoints"), bComboPoints, GGameIni);
				GConfig->GetBool(S, TEXT("bOrbTally"), bOrbTally, GGameIni);
				GConfig->GetBool(S, TEXT("bGhostMarker"), bGhostMarker, GGameIni);
				GConfig->GetBool(S, TEXT("bWeightUI"), bWeightUI, GGameIni);
				GConfig->GetFloat(S, TEXT("GoodLo"), GoodLo, GGameIni);
				GConfig->GetFloat(S, TEXT("GreatLo"), GreatLo, GGameIni);
				GConfig->GetFloat(S, TEXT("PerfectLo"), PerfectLo, GGameIni);
				GConfig->GetFloat(S, TEXT("PerfectHi"), PerfectHi, GGameIni);
				GConfig->GetFloat(S, TEXT("MissHi"), MissHi, GGameIni);
				GConfig->GetFloat(S, TEXT("CalloutY"), CalloutY, GGameIni);
				GConfig->GetFloat(S, TEXT("PortraitCalloutY"), PortraitCalloutY, GGameIni);
				GConfig->GetFloat(S, TEXT("PortraitCalloutX"), PortraitCalloutX, GGameIni);
				GConfig->GetFloat(S, TEXT("PortraitTallyScale"), PortraitTallyScale, GGameIni);
				GConfig->GetFloat(S, TEXT("PortraitTallyWorldZMul"), PortraitTallyWorldZMul, GGameIni);
				GConfig->GetFloat(S, TEXT("PortraitTallyYMin"), PortraitTallyYMin, GGameIni);
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoScoringUI"))) bEnabled = false;
			bLog = FParse::Param(FCommandLine::Get(), TEXT("ZoogiScoringUILog"));
			int32 MirrorUser = -1;
			if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), TEXT("MirrorHUD"), MirrorUser, GGameUserSettingsIni);
			if (MirrorUser >= 0) bMirrorLayout = (MirrorUser != 0);
		}
	};

	static FTune& TuneMut() { static FTune T; return T; }
	static const FTune& Tune() { return TuneMut(); }

	static TArray<FCallout> GCallouts;
	static TArray<FTallyPop> GPops;
	static FMatchStats GStats;
	static FVector GGhost = FVector::ZeroVector;
	static bool GHasGhost = false;
	static int32 GStreak = 0;
	static int32 GLastStep = 0;

	bool Enabled() { return Tune().bEnabled; }
	bool PauseClockWhileRolling() { return Enabled() && Tune().bPauseClockWhileRolling; }
	bool MirrorLayout() { return Enabled() && Tune().bMirrorLayout; }

	void SetMirrorLayout(bool bOn)
	{
		TuneMut().bMirrorLayout = bOn;
		if (GConfig)
		{
			GConfig->SetInt(TEXT("ZoogiRoll.MenuSettings"), TEXT("MirrorHUD"), bOn ? 1 : 0, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}


	bool IsPortraitLayout(float ClipX, float ClipY)
	{
		// ForceRes / SetPortraitCameraMode: 1 = portrait, 0 = landscape, -1 = auto from viewport.
		const int32 Mode = ZoogiPortrait::GetModeOverride();
		if (Mode == 1) return true;
		if (Mode == 0) return false;
		return ClipY > ClipX;
	}

	float CalloutAnchorY(float ClipX, float ClipY)
	{
		const FTune& T = Tune();
		return IsPortraitLayout(ClipX, ClipY) ? T.PortraitCalloutY : T.CalloutY;
	}

	float CalloutAnchorX(float ClipX, float ClipY)
	{
		const FTune& T = Tune();
		return IsPortraitLayout(ClipX, ClipY) ? T.PortraitCalloutX : 0.5f;
	}

	float TallyDrawScale(float ClipX, float ClipY)
	{
		return IsPortraitLayout(ClipX, ClipY) ? Tune().PortraitTallyScale : 1.f;
	}

	float TallyWorldZMul(float ClipX, float ClipY)
	{
		return IsPortraitLayout(ClipX, ClipY) ? Tune().PortraitTallyWorldZMul : 1.f;
	}

	float TallyScreenYMin(float ClipX, float ClipY)
	{
		return IsPortraitLayout(ClipX, ClipY) ? Tune().PortraitTallyYMin : 0.f;
	}

	float Weight01(const AZoogiBall* B)
	{
		if (!B || !Tune().bWeightUI) return 0.5f;
		// MassRatio is 0.55..1.8 -> 0..1
		return FMath::Clamp((B->Def.MassRatio() - 0.55f) / (1.8f - 0.55f), 0.f, 1.f);
	}

	const TCHAR* WeightLabel(float MassRatio)
	{
		if (MassRatio < 0.85f) return TEXT("LIGHT");
		if (MassRatio > 1.2f) return TEXT("HEAVY");
		return TEXT("MED");
	}

	int32 WeightClass(float MassRatio)
	{
		if (MassRatio < 0.85f) return 1;
		if (MassRatio > 1.2f) return 3;
		return 2;
	}

	int32 CardWeightClass(FName CardId)
	{
		if (!Tune().bWeightUI || CardId.IsNone()) return 0;
		const FString S = CardId.ToString().ToLower();
		if (S.Contains(TEXT("heavy")) || S.Contains(TEXT("titan")) || S.Contains(TEXT("anchor")) || S.Contains(TEXT("boulder")))
			return 3;
		if (S.Contains(TEXT("light")) || S.Contains(TEXT("swift")) || S.Contains(TEXT("breeze")) || S.Contains(TEXT("gust")))
			return 1;
		return 0;
	}

	EShotGrade GradePull(float Frac)
	{
		const FTune& T = Tune();
		Frac = FMath::Clamp(Frac, 0.f, 1.f);
		if (Frac >= T.PerfectLo && Frac <= T.PerfectHi) return EShotGrade::Perfect;
		if (Frac < T.MissHi) return EShotGrade::Miss;
		if (Frac >= T.GreatLo) return EShotGrade::Great;
		if (Frac >= T.GoodLo) return EShotGrade::Good;
		return EShotGrade::Miss;
	}

	const TCHAR* GradeText(EShotGrade G)
	{
		switch (G)
		{
		case EShotGrade::Perfect: return TEXT("PERFECT!");
		case EShotGrade::Great: return TEXT("GREAT!");
		case EShotGrade::Good: return TEXT("GOOD!");
		default: return TEXT("MISS");
		}
	}

	FLinearColor GradeColor(EShotGrade G)
	{
		switch (G)
		{
		case EShotGrade::Perfect: return FLinearColor(1.f, 0.92f, 0.28f);
		case EShotGrade::Great: return FLinearColor(0.45f, 0.95f, 0.55f);
		case EShotGrade::Good: return FLinearColor(0.55f, 0.82f, 1.f);
		default: return FLinearColor(1.f, 0.45f, 0.4f);
		}
	}

	int32 ComboPointsForIndex(int32 Index1Based)
	{
		if (Index1Based <= 1) return 1;
		if (Index1Based == 2) return 2;
		return 4;
	}

	static void PushCallout(const FString& Text, const FLinearColor& Color, float Life = 1.15f, float Scale = 1.f)
	{
		FCallout C;
		C.Text = Text;
		C.Color = Color;
		C.Life = Life;
		C.Scale = Scale;
		GCallouts.Add(C);
		if (GCallouts.Num() > 6) GCallouts.RemoveAt(0);
	}

	void ResetMatch()
	{
		GCallouts.Reset();
		GPops.Reset();
		GStats = FMatchStats();
		GHasGhost = false;
		GGhost = FVector::ZeroVector;
		GStreak = 0;
		GLastStep = 0;
	}

	void OnAimBegin(AZoogiGameMode* G)
	{
		if (!Enabled()) return;
		GStreak = 0;
		GLastStep = 0;
		(void)G;
	}

	void OnShotLaunch(AZoogiGameMode* G, float PullFrac)
	{
		if (!Enabled() || !Tune().bShotCallouts || !G) return;
		GStreak = 0;
		GLastStep = 0;
		++GStats.Shots;
		const EShotGrade Grade = GradePull(PullFrac);
		switch (Grade)
		{
		case EShotGrade::Perfect: ++GStats.Perfects; break;
		case EShotGrade::Great: ++GStats.Greats; break;
		case EShotGrade::Good: ++GStats.Goods; break;
		default: ++GStats.Misses; break;
		}
		PushCallout(GradeText(Grade), GradeColor(Grade), Grade == EShotGrade::Miss ? 0.85f : 1.2f, Grade == EShotGrade::Perfect ? 1.25f : 1.f);
		if (AZoogiBall* B = G->GetActiveBall())
		{
			G->CardPopup(GradeText(Grade), B->BallLocation() + FVector(0, 0, 140.f), GradeColor(Grade));
		}
		if (Tune().bLog) UE_LOG(LogZoogi, Log, TEXT("SCORINGUI shot %s pull %.0f%%"), GradeText(Grade), PullFrac * 100.f);
	}

	int32 OnOrbScored(AZoogiGameMode* G, int32 Scorer, int32 BasePts, const FVector& Where, bool bStar)
	{
		if (!Enabled() || !G) return 0;
		++GStats.Orbs;
		if (bStar) ++GStats.Stars;
		int32 Bonus = 0;
		if (Tune().bComboPoints)
		{
			++GStreak;
			Bonus = ComboPointsForIndex(GStreak);
			GLastStep = Bonus;
			GStats.BestCombo = FMath::Max(GStats.BestCombo, Bonus);
			GStats.ComboBonus += Bonus;
			if (Bonus >= 2)
			{
				PushCallout(FString::Printf(TEXT("COMBO x%d  +%d"), GStreak, Bonus), FLinearColor(1.f, 0.85f, 0.35f), 1.0f, 1.1f); // Oct 29: chain count (matches the big COMBO badge) + bonus
			}
		}
		if (Tune().bOrbTally)
		{
			FTallyPop P;
			P.World = Where;
			P.Points = BasePts + Bonus;
			P.Color = bStar ? FLinearColor(1.f, 0.82f, 0.25f) : FLinearColor(0.45f, 0.85f, 1.f);
			P.ComboStep = GLastStep > 0 ? GLastStep : 1;
			GPops.Add(P);
			if (GPops.Num() > 10) GPops.RemoveAt(0);
		}
		if (Tune().bLog) UE_LOG(LogZoogi, Log, TEXT("SCORINGUI orb P%d +%d (bonus %d, streak %d)"), Scorer + 1, BasePts, Bonus, GStreak);
		(void)Scorer;
		return Bonus;
	}

	void OnKnockout(AZoogiGameMode* G, int32 Scorer, const FVector& Where)
	{
		if (!Enabled() || !G) return;
		++GStats.KOs;
		if (Tune().bComboPoints)
		{
			++GStreak;
			const int32 Bonus = ComboPointsForIndex(GStreak);
			GLastStep = Bonus;
			GStats.BestCombo = FMath::Max(GStats.BestCombo, Bonus);
			GStats.ComboBonus += Bonus;
			PushCallout(Bonus >= 2 ? FString::Printf(TEXT("KO COMBO x%d"), Bonus) : TEXT("KNOCKOUT!"),
				FLinearColor(1.f, 0.55f, 0.35f), 1.15f, 1.15f);
			if (Tune().bOrbTally)
			{
				FTallyPop P;
				P.World = Where;
				P.Points = ZoogiConst::ScorePlayer + Bonus;
				P.Color = FLinearColor(1.f, 0.5f, 0.35f);
				P.ComboStep = Bonus;
				GPops.Add(P);
			}
		}
		(void)Scorer;
	}

	void OnFall(AZoogiGameMode* G)
	{
		if (!Enabled()) return;
		++GStats.Falls;
		(void)G;
	}

	void OnShotSettled(AZoogiGameMode* G, const FVector& LandWorld)
	{
		if (!Enabled() || !Tune().bGhostMarker || !G) return;
		GGhost = LandWorld;
		GHasGhost = true;
	}

	void Tick(float Dt)
	{
		if (!Enabled()) return;
		for (int32 i = GCallouts.Num() - 1; i >= 0; --i)
		{
			GCallouts[i].Age += Dt;
			if (GCallouts[i].Age >= GCallouts[i].Life) GCallouts.RemoveAt(i);
		}
		for (int32 i = GPops.Num() - 1; i >= 0; --i)
		{
			GPops[i].Age += Dt;
			if (GPops[i].Age >= GPops[i].Life) GPops.RemoveAt(i);
		}
	}

	const TArray<FCallout>& Callouts() { return GCallouts; }
	const TArray<FTallyPop>& TallyPops() { return GPops; }
	bool HasGhost() { return Enabled() && Tune().bGhostMarker && GHasGhost; }
	FVector GhostWorld() { return GGhost; }
	int32 CurrentComboStep() { return GLastStep; }
	int32 ComboStreak() { return GStreak; }
	const FMatchStats& Stats() { return GStats; }

	bool FirstBouncePoint(const AZoogiGameMode* G, FVector& Out)
	{
		if (!Enabled() || !Tune().bFirstBounce || !G || !G->Preview.bValid) return false;
		if (G->Preview.bBank)
		{
			Out = G->Preview.BankPoint;
			return true;
		}
		if (G->Preview.Path.Num() > 0)
		{
			Out = G->Preview.Path.Last();
			return true;
		}
		return false;
	}

	int32 GetAudioPct(const TCHAR* Key, int32 DefaultPct)
	{
		int32 V = DefaultPct;
		if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), Key, V, GGameUserSettingsIni);
		return FMath::Clamp(V, 0, 100);
	}

	void SetAudioPct(const TCHAR* Key, int32 Pct)
	{
		Pct = FMath::Clamp(Pct, 0, 100);
		if (GConfig)
		{
			GConfig->SetInt(TEXT("ZoogiRoll.MenuSettings"), Key, Pct, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		ApplyAudio();
	}

	void ApplyAudio()
	{
		const int32 Master = GetAudioPct(TEXT("ClashMaster"), 50);
		const int32 Sfx = GetAudioPct(TEXT("ClashSfx"), 50);
		const int32 Music = GetAudioPct(TEXT("ClashMusic"), 50);
		if (GEngine)
			if (FAudioDeviceHandle Dev = GEngine->GetMainAudioDevice())
				Dev->SetTransientPrimaryVolume(FMath::Clamp(Master / 100.f, 0.f, 1.f));
		ZoogiUtil::SetSfxVolume(Sfx / 100.f);
		ZoogiUtil::SetMusicVolume(Music / 100.f);
	}
}
