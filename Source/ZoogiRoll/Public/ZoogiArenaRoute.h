#pragma once

// Oct 22 (Evan): every mode loads the arena the player picked. One place decides which arena a match is built on, per
// entry point, so the menus, the web UI and the game mode can't drift apart again (ZoogiRoll.Arena.RouteEveryMode).
#include "CoreMinimal.h"
#include "ZoogiTypes.h"

/** How a match was started. */
enum class EZoogiMatchRoute : uint8
{
	HomeBattle,   // Home PLAY / BATTLE -> matchmaking -> 2v2 card match on the current arena
	CustomClassic,// PLAY setup (Custom), classic turns
	Custom2v2,    // PLAY setup (Custom), 2v2 combo
	TrophyRoad,   // Arenas / Trophy Road gate tap selects that arena, then BATTLE
	Story,        // web story: LoadingComplete carries the episode's arena
	WebArena,     // web arena screen: LoadingComplete carries the picked arena
	Training,     // TRAINING tile: always the Teaser island (Evan Oct 23: Training loads the teaser island, no separate tile)
	MAX
};

namespace ZoogiArenaRoute
{
	inline EZoogiArena Valid(EZoogiArena A) { return (uint8)A < (uint8)EZoogiArena::MAX ? A : EZoogiArena::Meadow; }

	/** Combo (2v2) arena for a pick: the pick itself. The scripted -ZoogiComboDemo still runs on Meadow. */
	inline EZoogiArena ComboArena(EZoogiArena Pick, int32 Demo) { return (Demo > 0 || Pick == EZoogiArena::Ancient) ? EZoogiArena::Meadow : Valid(Pick); } // Oct 31: Ancient = Battle Royale field only

	/** Web UI arena id -> game arena. Skyfall Isles has no level yet: it plays on the Frozen Ring. */
	inline EZoogiArena ForWebId(const FString& Id)
	{
		if (Id == TEXT("meadow")) return EZoogiArena::Meadow;
		if (Id == TEXT("volcanic")) return EZoogiArena::Volcanic;
		if (Id == TEXT("cosmic")) return EZoogiArena::Cosmic;
		if (Id == TEXT("arabian")) return EZoogiArena::Arabian;
		if (Id == TEXT("pharaoh") || Id == TEXT("tomb")) return EZoogiArena::Tomb;
		if (Id == TEXT("night") || Id == TEXT("neon")) return EZoogiArena::Neon;
		if (Id == TEXT("training")) return EZoogiArena::Training;
		if (Id == TEXT("teaser")) return EZoogiArena::Teaser;
		if (Id == TEXT("ancient")) return EZoogiArena::Ancient;
		return EZoogiArena::Frozen; // frozen, skyfall
	}

	/** Web id for a game arena (inverse of ForWebId; Arabian has no web page, it keeps its own id). */
	inline const TCHAR* WebId(EZoogiArena A)
	{
		switch (A)
		{
		case EZoogiArena::Meadow: return TEXT("meadow");
		case EZoogiArena::Frozen: return TEXT("frozen");
		case EZoogiArena::Volcanic: return TEXT("volcanic");
		case EZoogiArena::Cosmic: return TEXT("cosmic");
		case EZoogiArena::Arabian: return TEXT("arabian");
		case EZoogiArena::Tomb: return TEXT("pharaoh");
		case EZoogiArena::Neon: return TEXT("night");
		case EZoogiArena::Training: return TEXT("training");
		case EZoogiArena::Teaser: return TEXT("teaser");
		case EZoogiArena::Ancient: return TEXT("ancient");
		default: return TEXT("meadow");
		}
	}

	/** Trophy Road / Arenas gate index -> game arena (gate N is EZoogiArena N). */
	inline EZoogiArena ForTrophyGate(int32 Gate) { return Valid((EZoogiArena)FMath::Clamp(Gate, 0, (int32)EZoogiArena::MAX - 1)); }

	/** The arena a match started from Route is built on, given the player's current pick. */
	inline EZoogiArena Resolve(EZoogiMatchRoute Route, EZoogiArena Pick, int32 Demo = 0)
	{
		switch (Route)
		{
		case EZoogiMatchRoute::Training: return EZoogiArena::Teaser; // Evan Oct 23 9:59 PM: Training mode loads the teaser island
		case EZoogiMatchRoute::HomeBattle:
		case EZoogiMatchRoute::Custom2v2:
		case EZoogiMatchRoute::TrophyRoad:
		case EZoogiMatchRoute::Story:
		case EZoogiMatchRoute::WebArena: return ComboArena(Pick, Demo);
		default: return Valid(Pick);
		}
	}
}
