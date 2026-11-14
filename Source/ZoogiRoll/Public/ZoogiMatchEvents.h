#pragma once

#include "CoreMinimal.h"

/**
 * Match events (Oct 7, music + announcer branch). AZoogiGameMode::NotifyMatchEvent broadcasts them on
 * AZoogiGameMode::OnMatchEvent. The hooks in the game code are one line each; everything else (state polling,
 * derived events such as FormingBegin / RollOut / FinalRound / MatchPoint / GaugeFull) lives in
 * ZoogiMusicSubsystem.cpp and ZoogiAnnouncerSubsystem.cpp.
 * Who = player index, team index or -1 (see each event); Mag = event magnitude (arena control, gauge, seconds...).
 */
enum class EZoogiMatchEvent : uint8
{
	MatchStart,      // Who = 1 for 2v2 combo, 0 classic
	FormingBegin,    // derived (Phase -> Forming)
	FormingDone,     // derived
	RollOut,         // derived: first turn of the match is about to start
	StageChanged,    // derived: Who = (int32)EZoogiStage
	OrbOut,          // Who = team, Mag = arena control
	RingOut,         // Who = team of the Zoogi that fell, Mag = its player index
	KnockedOut,      // Who = team of the Zoogi that fell, Mag = its player index
	RecoveryGate,    // Who = team
	ResonanceSurge,  // Who = team
	ResonanceShift,  // Who = team, Mag = arena control
	FollowUpOffered, // Who = team, Mag = gauge
	FollowUpStart,   // Who = team
	GaugeChanged,    // Who = team, Mag = new gauge value
	GaugeFull,       // derived from GaugeChanged
	GuardBreak,      // Who = attacker team
	FloorChanged,    // Mag = (float)EZoogiFloorState
	AnimeCombo,      // classic anime banners (ZoogiAnimeSubsystem)
	AnimeKO,
	AnimeCritical,
	ClimaxBegin,
	ClimaxLand,
	Duck,            // Mag = seconds
	FinalRound,      // derived
	MatchPoint,      // derived, Who = team on its last Ring Mark
	MatchOver,       // classic: Who = winner player index (-1 draw); combo: Who = winning team (-1 draw)
	ReturnToMenu,
	Count
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnZoogiMatchEvent, EZoogiMatchEvent /*Event*/, int32 /*Who*/, float /*Mag*/);

namespace ZoogiMatch
{
	ZOOGIROLL_API const TCHAR* EventName(EZoogiMatchEvent E);
}
