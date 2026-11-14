#include "ZoogiEpisodeTypes.h"
#include "Misc/ConfigCacheIni.h"

void FZoogiEpisodeTuning::Load()
{
	if (!GConfig) return;
	const TCHAR* S = TEXT("ZoogiRoll.Episode");
	auto F = [S](const TCHAR* Key, float& V) { GConfig->GetFloat(S, Key, V, GGameIni); };
	auto I = [S](const TCHAR* Key, int32& V) { GConfig->GetInt(S, Key, V, GGameIni); };
	F(TEXT("MeaningfulSpeed"), MeaningfulSpeed); F(TEXT("FullWeightSpeed"), FullWeightSpeed); F(TEXT("DedupeSeconds"), DedupeSeconds);
	F(TEXT("AirborneGap"), AirborneGap); F(TEXT("AirborneMinTime"), AirborneMinTime); F(TEXT("AirborneMinPeak"), AirborneMinPeak);
	F(TEXT("EdgeNearDist"), EdgeNearDist); F(TEXT("EdgeNearSpeed"), EdgeNearSpeed);
	F(TEXT("PBank"), PBank); F(TEXT("PDoubleBank"), PDoubleBank); F(TEXT("PAllyBump"), PAllyBump); F(TEXT("POrbRelay"), POrbRelay);
	F(TEXT("PWallRedirect"), PWallRedirect); F(TEXT("PBoundarySave"), PBoundarySave); F(TEXT("PAirborne"), PAirborne);
	F(TEXT("PPowerHit"), PPowerHit); F(TEXT("PCrit"), PCrit); F(TEXT("PGuardBreak"), PGuardBreak); F(TEXT("POrbOut"), POrbOut);
	F(TEXT("PRingOut"), PRingOut); F(TEXT("PTerrain"), PTerrain); F(TEXT("PTerrainExtra"), PTerrainExtra); F(TEXT("PPartnerFollowUp"), PPartnerFollowUp);
	F(TEXT("RepeatFalloff"), RepeatFalloff); F(TEXT("SynergyPer"), SynergyPer); F(TEXT("ChainBonus"), ChainBonus); F(TEXT("ChainBonusMax"), ChainBonusMax);
	F(TEXT("ScoreSwingBonus"), ScoreSwingBonus); I(TEXT("ScoreSwingPoints"), ScoreSwingPoints);
	F(TEXT("T1"), T1); F(TEXT("T2"), T2); F(TEXT("T3"), T3); F(TEXT("T4"), T4); F(TEXT("T5"), T5); I(TEXT("T5MinCategories"), T5MinCategories);
	F(TEXT("FullScoreMul"), FullScoreMul);
	F(TEXT("CalloutGap"), CalloutGap); F(TEXT("SlowMoScale"), SlowMoScale); F(TEXT("SlowMoSeconds"), SlowMoSeconds); F(TEXT("PanelSeconds"), PanelSeconds);
	F(TEXT("MeteorSeconds"), MeteorSeconds); F(TEXT("MeteorSecondsShort"), MeteorSecondsShort); F(TEXT("MeteorSecondsFull"), MeteorSecondsFull);
	F(TEXT("MeteorMaxDelay"), MeteorMaxDelay); I(TEXT("PanelRes"), PanelRes);
	SlowMoScale = FMath::Clamp(SlowMoScale, 0.05f, 1.f);
	PanelRes = FMath::Clamp(PanelRes, 128, 1024);
}

namespace ZoogiEpisode
{
	const TCHAR* TagName(EZoogiEpisodeTag T)
	{
		switch (T)
		{
		case EZoogiEpisodeTag::Bank: return TEXT("BANK");
		case EZoogiEpisodeTag::DoubleBank: return TEXT("DOUBLE_BANK");
		case EZoogiEpisodeTag::AllyBump: return TEXT("ALLY_BUMP");
		case EZoogiEpisodeTag::OrbRelay: return TEXT("ORB_RELAY");
		case EZoogiEpisodeTag::WallRedirect: return TEXT("WALL_REDIRECT");
		case EZoogiEpisodeTag::BoundarySave: return TEXT("BOUNDARY_SAVE");
		case EZoogiEpisodeTag::Airborne: return TEXT("AIRBORNE");
		case EZoogiEpisodeTag::PowerHit: return TEXT("POWER_HIT");
		case EZoogiEpisodeTag::GuardBreak: return TEXT("GUARD_BREAK");
		case EZoogiEpisodeTag::OrbOut: return TEXT("ORB_OUT");
		case EZoogiEpisodeTag::RingOut: return TEXT("RING_OUT");
		case EZoogiEpisodeTag::Terrain: return TEXT("TERRAIN");
		case EZoogiEpisodeTag::PartnerFollowUp: return TEXT("PARTNER_FOLLOW_UP");
		case EZoogiEpisodeTag::NearMiss: return TEXT("NEAR_MISS");
		case EZoogiEpisodeTag::FunnyBounce: return TEXT("FUNNY_BOUNCE");
		case EZoogiEpisodeTag::OrbAlmostOut: return TEXT("ORB_ALMOST_OUT");
		default: return TEXT("?");
		}
	}

	const TCHAR* TagLabel(EZoogiEpisodeTag T)
	{
		switch (T)
		{
		case EZoogiEpisodeTag::Bank: return TEXT("BANK");
		case EZoogiEpisodeTag::DoubleBank: return TEXT("DOUBLE BANK");
		case EZoogiEpisodeTag::AllyBump: return TEXT("ALLY BUMP");
		case EZoogiEpisodeTag::OrbRelay: return TEXT("ORB RELAY");
		case EZoogiEpisodeTag::WallRedirect: return TEXT("WALL REDIRECT");
		case EZoogiEpisodeTag::BoundarySave: return TEXT("BOUNDARY SAVE");
		case EZoogiEpisodeTag::Airborne: return TEXT("AIRBORNE");
		case EZoogiEpisodeTag::PowerHit: return TEXT("POWER HIT");
		case EZoogiEpisodeTag::GuardBreak: return TEXT("GUARD BREAK");
		case EZoogiEpisodeTag::OrbOut: return TEXT("ORB OUT");
		case EZoogiEpisodeTag::RingOut: return TEXT("RING OUT");
		case EZoogiEpisodeTag::Terrain: return TEXT("TERRAIN");
		case EZoogiEpisodeTag::PartnerFollowUp: return TEXT("PARTNER FOLLOW-UP");
		case EZoogiEpisodeTag::NearMiss: return TEXT("NEAR MISS");
		case EZoogiEpisodeTag::FunnyBounce: return TEXT("FUNNY BOUNCE");
		case EZoogiEpisodeTag::OrbAlmostOut: return TEXT("ORB ALMOST OUT");
		default: return TEXT("?");
		}
	}

	const TCHAR* ModeName(EZoogiEpisodeMode M)
	{
		switch (M)
		{
		case EZoogiEpisodeMode::Off: return TEXT("Off");
		case EZoogiEpisodeMode::Competitive: return TEXT("Competitive");
		case EZoogiEpisodeMode::Full: return TEXT("Full");
		default: return TEXT("Balanced");
		}
	}

	const TCHAR* LayoutName(EZoogiPanelLayout L)
	{
		switch (L)
		{
		case EZoogiPanelLayout::CornerPair: return TEXT("CornerPair");
		case EZoogiPanelLayout::SidePair: return TEXT("SidePair");
		case EZoogiPanelLayout::Trio: return TEXT("Trio");
		case EZoogiPanelLayout::Quad: return TEXT("Quad");
		default: return TEXT("None");
		}
	}

	const TCHAR* ArchetypeName(EZoogiEpisodeArchetype A)
	{
		return A == EZoogiEpisodeArchetype::TitanMeteor ? TEXT("TitanMeteor") : TEXT("None");
	}

	const TCHAR* KindName(EZoogiContactKind K)
	{
		switch (K)
		{
		case EZoogiContactKind::Zoogi: return TEXT("zoogi");
		case EZoogiContactKind::Ally: return TEXT("ally");
		case EZoogiContactKind::Orb: return TEXT("orb");
		case EZoogiContactKind::Wall: return TEXT("wall");
		case EZoogiContactKind::Bumper: return TEXT("bumper");
		case EZoogiContactKind::Ramp: return TEXT("ramp");
		case EZoogiContactKind::Piece: return TEXT("piece");
		case EZoogiContactKind::Rock: return TEXT("rock");
		default: return TEXT("floor");
		}
	}

	FLinearColor PowerColor(EZoogiArtifact A)
	{
		switch (A)
		{
		case EZoogiArtifact::Storm: return FLinearColor(0.62f, 0.38f, 1.f);   // Storm / Shock: violet-electric
		case EZoogiArtifact::Void: return FLinearColor(0.42f, 0.12f, 0.75f);   // Void: deep violet
		case EZoogiArtifact::Ember: return FLinearColor(1.f, 0.42f, 0.1f);     // Ember: fire orange
		case EZoogiArtifact::Titan: return FLinearColor(0.95f, 0.66f, 0.28f);  // Titan: earth amber
		case EZoogiArtifact::Frost: return FLinearColor(0.55f, 0.88f, 1.f);    // Frost: ice blue
		default: return FLinearColor(1.f, 0.92f, 0.7f);
		}
	}

	FString PowerTitle(EZoogiArtifact A)
	{
		switch (A)
		{
		case EZoogiArtifact::Storm: return TEXT("STORM SURGE");
		case EZoogiArtifact::Void: return TEXT("VOID BREACH");
		case EZoogiArtifact::Ember: return TEXT("EMBER BLAZE");
		case EZoogiArtifact::Titan: return TEXT("TITAN FORCE");
		case EZoogiArtifact::Frost: return TEXT("FROST LOCK");
		default: return TEXT("POWER");
		}
	}
}
