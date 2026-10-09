#include "ZoogiBattleTypes.h"
#include "ZoogiRoll.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

namespace ZoogiBattle
{
	const TCHAR* StateName(EZoogiBattleState S)
	{
		switch (S)
		{
		case EZoogiBattleState::WaitForCommand: return TEXT("WAIT_FOR_COMMAND");
		case EZoogiBattleState::ActionPreview: return TEXT("ACTION_PREVIEW");
		case EZoogiBattleState::MiniGame: return TEXT("MINIGAME");
		case EZoogiBattleState::PhysicsResolve: return TEXT("PHYSICS_RESOLVE");
		case EZoogiBattleState::ShatterFailure: return TEXT("SHATTER_FAILURE");
		case EZoogiBattleState::Result: return TEXT("RESULT");
		case EZoogiBattleState::EpisodeEvent: return TEXT("EPISODE_EVENT");
		case EZoogiBattleState::NextTurn: return TEXT("NEXT_TURN");
		case EZoogiBattleState::MatchEnd: return TEXT("MATCH_END");
		default: return TEXT("NONE");
		}
	}
	const TCHAR* ChallengeName(EZoogiChallenge C)
	{
		switch (C)
		{
		case EZoogiChallenge::Strike: return TEXT("Strike");
		case EZoogiChallenge::HaloBank: return TEXT("HaloBank");
		case EZoogiChallenge::OrbLine: return TEXT("OrbLine");
		case EZoogiChallenge::AirReaction: return TEXT("AirReaction");
		case EZoogiChallenge::SkyBumper: return TEXT("SkyBumper");
		case EZoogiChallenge::PartnerIntercept: return TEXT("PartnerIntercept");
		case EZoogiChallenge::RailRelease: return TEXT("RailRelease");
		default: return TEXT("None");
		}
	}
	const TCHAR* ResultName(EZoogiChallengeResult R)
	{
		switch (R)
		{
		case EZoogiChallengeResult::Pending: return TEXT("PENDING");
		case EZoogiChallengeResult::Success: return TEXT("SUCCESS");
		case EZoogiChallengeResult::Failed: return TEXT("FAILED");
		case EZoogiChallengeResult::Timeout: return TEXT("TIMEOUT");
		case EZoogiChallengeResult::Cancelled: return TEXT("CANCELLED");
		default: return TEXT("NONE");
		}
	}
	const TCHAR* ComicName(EZoogiComicState S)
	{
		switch (S)
		{
		case EZoogiComicState::Pending: return TEXT("PENDING");
		case EZoogiComicState::Completed: return TEXT("COMPLETED");
		case EZoogiComicState::Shattered: return TEXT("SHATTERED");
		default: return TEXT("NONE");
		}
	}
	const TCHAR* RailWindowName(EZoogiRailWindow W)
	{
		switch (W)
		{
		case EZoogiRailWindow::Early: return TEXT("EARLY");
		case EZoogiRailWindow::Center: return TEXT("CENTER");
		case EZoogiRailWindow::Late: return TEXT("LATE");
		default: return TEXT("NONE");
		}
	}
	const TCHAR* AirChoiceName(EZoogiAirChoice C)
	{
		switch (C)
		{
		case EZoogiAirChoice::Brace: return TEXT("BRACE");
		case EZoogiAirChoice::Redirect: return TEXT("REDIRECT");
		default: return TEXT("NONE");
		}
	}
	EZoogiChallenge ParseChallenge(const FString& S)
	{
		if (S.Equals(TEXT("Strike"), ESearchCase::IgnoreCase)) return EZoogiChallenge::Strike;
		if (S.Equals(TEXT("HaloBank"), ESearchCase::IgnoreCase)) return EZoogiChallenge::HaloBank;
		if (S.Equals(TEXT("OrbLine"), ESearchCase::IgnoreCase)) return EZoogiChallenge::OrbLine;
		if (S.Equals(TEXT("AirReaction"), ESearchCase::IgnoreCase)) return EZoogiChallenge::AirReaction;
		if (S.Equals(TEXT("SkyBumper"), ESearchCase::IgnoreCase)) return EZoogiChallenge::SkyBumper;
		if (S.Equals(TEXT("PartnerIntercept"), ESearchCase::IgnoreCase)) return EZoogiChallenge::PartnerIntercept;
		if (S.Equals(TEXT("RailRelease"), ESearchCase::IgnoreCase)) return EZoogiChallenge::RailRelease;
		return EZoogiChallenge::None;
	}
	bool LoadCsv(const FString& Name, TArray<TArray<FString>>& OutRows, FString& OutPath)
	{
		OutRows.Reset();
		OutPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), Name);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *OutPath)) return false;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (int32 i = 1; i < Lines.Num(); ++i) // row 0 = header
		{
			FString L = Lines[i].TrimStartAndEnd();
			if (L.IsEmpty() || L.StartsWith(TEXT("#"))) continue;
			TArray<FString> Cells;
			L.ParseIntoArray(Cells, TEXT(","), false);
			for (FString& C : Cells) C.TrimStartAndEndInline();
			OutRows.Add(Cells);
		}
		return true;
	}
}

const FZoogiEpisodeComboRow* FZoogiBattleTuning::FindCombo(FName Id) const
{
	for (const FZoogiEpisodeComboRow& R : Combos) if (R.Id == Id) return &R;
	return nullptr;
}

void FZoogiBattleTuning::Load()
{
	// ---- built-in defaults (balance variables: tune in the CSV / ini, not here)
	Gains.Reset();
	Gains.Add(TEXT("Hit"), 4.f);          // per meaningful Zoogi contact by the actor
	Gains.Add(TEXT("StrongHit"), 6.f);    // extra when the relative speed >= StrongHitSpeed
	Gains.Add(TEXT("Impact"), 1.f);       // per meaningful surface impact (capped by ImpactCap)
	Gains.Add(TEXT("Crack"), 5.f);        // per arena piece / rock damage step
	Gains.Add(TEXT("OrbOut"), 6.f);
	Gains.Add(TEXT("RingOut"), 12.f);     // per rival knocked out
	Gains.Add(TEXT("PowerHit"), 6.f);     // infused / special contact
	Gains.Add(TEXT("Feat"), 3.f);         // x recognized tier, for tier >= 2 (bank, airborne, relay, ...)
	Gains.Add(TEXT("GuardBlock"), 5.f);   // to a guarded Zoogi that absorbs a hit
	Gains.Add(TEXT("OrbLineOrb"), 2.f);   // per committed orb touched in an Orb Line
	Gains.Add(TEXT("Reaction"), 4.f);     // a successful Air Reaction
	Gains.Add(TEXT("RecoveryGate"), 3.f); // a Zoogi rolls through a Zone 3 red / blue recovery gate (Frozen map layout)
	Combos.Reset();
	auto AddC = [this](const TCHAR* Id, const TCHAR* Label, float Cost, EZoogiChallenge Ch, int32 MinTier, const TCHAR* Tag, const TCHAR* Clip, bool bCPU)
	{
		FZoogiEpisodeComboRow R; R.Id = Id; R.Label = Label; R.Cost = Cost; R.Challenge = Ch; R.MinTier = MinTier; R.RequiredTag = Tag ? FName(Tag) : NAME_None; R.ClipId = Clip; R.bCPU = bCPU;
		Combos.Add(R);
	};
	AddC(TEXT("signature_strike"), TEXT("SIGNATURE STRIKE"), 20.f, EZoogiChallenge::Strike, 0, nullptr, TEXT("clip_{zoogi}_signature_strike"), true);
	AddC(TEXT("halo_bank"), TEXT("HALO BANK"), 25.f, EZoogiChallenge::HaloBank, 0, nullptr, TEXT("clip_{zoogi}_halo_bank"), true);   // CPU: only with a halo-pass prop plan
	AddC(TEXT("orb_line"), TEXT("ORB LINE"), 15.f, EZoogiChallenge::OrbLine, 0, nullptr, TEXT("clip_{zoogi}_orb_line"), false);
	AddC(TEXT("sky_bumper"), TEXT("SKY BUMPER"), 25.f, EZoogiChallenge::SkyBumper, 0, nullptr, TEXT("clip_{zoogi}_sky_bumper"), true); // CPU: only with a bumper-bank prop plan
	AddC(TEXT("partner_intercept"), TEXT("PARTNER INTERCEPT"), 30.f, EZoogiChallenge::PartnerIntercept, 0, nullptr, TEXT("clip_{zoogi}_partner_intercept"), false);
	AddC(TEXT("rail_release"), TEXT("RAIL RELEASE"), 20.f, EZoogiChallenge::RailRelease, 0, nullptr, TEXT("clip_{zoogi}_rail_release"), true); // CPU: only with a rail prop plan
	AddC(TEXT("power_finisher"), TEXT("POWER FINISHER"), 40.f, EZoogiChallenge::Strike, 2, TEXT("POWER_HIT"), TEXT("clip_{zoogi}_power_finisher"), true);
	GainsSource = CombosSource = TEXT("built-in");

	// ---- CSV tables override the defaults
	TArray<TArray<FString>> Rows;
	FString Path;
	if (ZoogiBattle::LoadCsv(TEXT("EpisodeEnergyGains.csv"), Rows, Path))
	{
		for (const TArray<FString>& R : Rows) if (R.Num() >= 2) Gains.Add(FName(*R[0]), FCString::Atof(*R[1]));
		GainsSource = Path;
	}
	if (ZoogiBattle::LoadCsv(TEXT("EpisodeCombos.csv"), Rows, Path) && Rows.Num() > 0)
	{
		Combos.Reset();
		// Id,Label,Cost,Challenge,MinTier,RequiredTag,ClipId,CPU
		for (const TArray<FString>& R : Rows)
		{
			if (R.Num() < 4) continue;
			FZoogiEpisodeComboRow C;
			C.Id = FName(*R[0]); C.Label = R[1]; C.Cost = FCString::Atof(*R[2]); C.Challenge = ZoogiBattle::ParseChallenge(R[3]);
			C.MinTier = R.IsValidIndex(4) ? FCString::Atoi(*R[4]) : 0;
			C.RequiredTag = R.IsValidIndex(5) && !R[5].IsEmpty() && R[5] != TEXT("-") ? FName(*R[5]) : NAME_None;
			C.ClipId = R.IsValidIndex(6) ? FName(*R[6]) : NAME_None;
			C.bCPU = !R.IsValidIndex(7) || R[7] != TEXT("0");
			Combos.Add(C);
		}
		CombosSource = Path;
	}

	// ---- ini scalars
	if (!GConfig) return;
	auto B = [](const TCHAR* S, const TCHAR* K, bool& V) { GConfig->GetBool(S, K, V, GGameIni); };
	auto F = [](const TCHAR* S, const TCHAR* K, float& V) { GConfig->GetFloat(S, K, V, GGameIni); };
	auto I = [](const TCHAR* S, const TCHAR* K, int32& V) { GConfig->GetInt(S, K, V, GGameIni); };
	auto Str = [](const TCHAR* S, const TCHAR* K, FString& V) { GConfig->GetString(S, K, V, GGameIni); };
	const TCHAR* SB = TEXT("ZoogiRoll.Battle");
	B(SB, TEXT("Enabled"), bEnabled); B(SB, TEXT("bMissConsolationCard"), bMissConsolationCard); F(SB, TEXT("FollowUpOfferTimeout"), FollowUpOfferTimeout);
	F(SB, TEXT("MiniGameInputTimeout"), MiniGameInputTimeout); F(SB, TEXT("ShatterScorePenalty"), ShatterScorePenalty); F(SB, TEXT("ShatterRefundFrac"), ShatterRefundFrac);
	B(SB, TEXT("bSpendOnSelect"), bSpendOnSelect); B(SB, TEXT("bCPUCombos"), bCPUCombos); B(SB, TEXT("bDebugOverlay"), bDebugOverlay);
	const TCHAR* SE = TEXT("ZoogiRoll.EpisodeEnergy");
	F(SE, TEXT("Cap"), EnergyCap); F(SE, TEXT("StrongHitSpeed"), StrongHitSpeed); I(SE, TEXT("ImpactCap"), ImpactCap); Str(SE, TEXT("FollowUpCostSource"), FollowUpCostSource);
	B(SE, TEXT("bResonanceSurgeDraws"), bResonanceSurgeDraws); B(SE, TEXT("bArenaControlGauge"), bArenaControlGauge); B(SE, TEXT("bStarOrbDraws"), bStarOrbDraws);
	const TCHAR* SA = TEXT("ZoogiRoll.Awakening");
	Str(SA, TEXT("ActivationRule"), ActivationRule); I(SA, TEXT("AwakenTurns"), AwakenTurns); I(SA, TEXT("ReactionsPerAwakening"), ReactionsPerAwakening);
	B(SA, TEXT("bAirReactionOwnTurn"), bAirReactionOwnTurn); F(SA, TEXT("AirReactionMaxWait"), AirReactionMaxWait); F(SA, TEXT("AirReactionHoldCPU"), AirReactionHoldCPU);
	F(SA, TEXT("BraceDamp"), BraceDamp); F(SA, TEXT("RedirectDeg"), RedirectDeg);
	const TCHAR* SZ = TEXT("ZoogiRoll.Aerial");
	B(SZ, TEXT("bAerialProps"), bAerialProps); F(SZ, TEXT("SkyBumperRebound"), SkyBumperRebound); F(SZ, TEXT("SkyBumperMaxSpeed"), SkyBumperMaxSpeed); F(SZ, TEXT("SkyBumperRadius"), SkyBumperRadius);
	F(SZ, TEXT("HaloRadius"), HaloRadius); F(SZ, TEXT("RailEarlyWindow"), RailEarlyWindow); F(SZ, TEXT("RailCenterWindow"), RailCenterWindow); F(SZ, TEXT("RailLateWindow"), RailLateWindow);
	F(SZ, TEXT("RailHighPitch"), RailHighPitch); F(SZ, TEXT("RailCenterPitch"), RailCenterPitch); F(SZ, TEXT("RailLatePitch"), RailLatePitch); F(SZ, TEXT("RailLateYaw"), RailLateYaw);
	F(SZ, TEXT("RailSpeedMul"), RailSpeedMul); F(SZ, TEXT("RailMinSpeed"), RailMinSpeed); F(SZ, TEXT("ZoneAerialZ"), ZoneAerialZ); F(SZ, TEXT("ZoneRecoveryFrac"), ZoneRecoveryFrac);
	Str(SZ, TEXT("AerialArenas"), AerialArenas); F(SZ, TEXT("MapRailMinSpeed"), MapRailMinSpeed); F(SZ, TEXT("CPUPropChance"), CPUPropChance); F(SZ, TEXT("CPUPropAimErrDiv"), CPUPropAimErrDiv);
	Str(SZ, TEXT("ReachPeaks"), ReachPeaks); F(SZ, TEXT("ReachMargin"), ReachMargin); F(SZ, TEXT("AerialHaloAlong"), AerialHaloAlong); F(SZ, TEXT("AerialHaloZ"), AerialHaloZ);
	B(SB, TEXT("bBattleHUD"), bBattleHUD);
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiBattleProps"))) { bAerialProps = true; AerialArenas = TEXT("All"); }
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiBattleNoProps"))) bAerialProps = false;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiBattleDebug"))) bDebugOverlay = true;
	FParse::Value(FCommandLine::Get(), TEXT("-ZoogiCPUPropChance="), CPUPropChance);
	FParse::Value(FCommandLine::Get(), TEXT("-ZoogiReachPeaks="), ReachPeaks); // measurement runs: -ZoogiReachPeaks=none (no cap)
	if (ReachPeaks.Equals(TEXT("none"), ESearchCase::IgnoreCase)) ReachPeaks.Empty();
	FParse::Value(FCommandLine::Get(), TEXT("-ZoogiSkyBumperRebound="), SkyBumperRebound);
	const TCHAR* SU = TEXT("ZoogiRoll.Unlocks");
	F(SU, TEXT("PerfWeightScore"), PerfWeightScore); F(SU, TEXT("PerfWeightKO"), PerfWeightKO); F(SU, TEXT("PerfWeightSegment"), PerfWeightSegment);
	F(SU, TEXT("PerfWeightSpotlight"), PerfWeightSpotlight); F(SU, TEXT("PerfWeightShatter"), PerfWeightShatter); F(SU, TEXT("FullEpisodePerf"), FullEpisodePerf);
	I(SU, TEXT("FullEpisodeMinSegments"), FullEpisodeMinSegments); I(SU, TEXT("FullEpisodeMaxShatters"), FullEpisodeMaxShatters); F(SU, TEXT("FragmentPerf"), FragmentPerf);
	// per-gain ini overrides: [ZoogiRoll.EpisodeEnergy] Gain_Hit=5
	for (TPair<FName, float>& G : Gains) F(SE, *FString::Printf(TEXT("Gain_%s"), *G.Key.ToString()), G.Value);
}
