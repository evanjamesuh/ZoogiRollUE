// b137 (Evan 08:24 / 09:15): "selected weapons appear in ITEM as a list in the skill menu to use", "until the collection is depleted,
// once per turn", "items collected during gameplay not used get saved and stored into inventory with a counter" (carried over):
//  - the profile keeps an item collection (FZoogiFrontProfileData::ItemCounts, seeded with 3 of each weapon) and the WEAPON tab picks
//    which weapons ride along (SelWeapons, up to 3)
//  - in a classic match the human's ITEM list shows the selected weapons with their counts (+ PRISM GRENADEs picked up); one item per
//    turn; each use spends one from the collection
//  - match over / back to menu: what is left (pickups included) is written back to the profile
// Weapon effects (no SP): PRISM GRENADE lob (2x arc range shockwave), CRYSTAL SWORD slash ring round you, CRYSTAL STAFF heavy bolt at a
// tapped target, GATLING 3 rapid shots at tapped targets, HALO DISC cuts along a swiped line.
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;

namespace ZoogiItems
{
	ZOOGIROLL_API const TArray<FName>& WeaponIds();      // gatling, grenade, sword, staff, disc
	ZOOGIROLL_API FString WeaponName(FName Id);
	ZOOGIROLL_API FString WeaponRule(FName Id);
	/** the human's selected weapons for this match, in order */
	ZOOGIROLL_API TArray<FName> Selected(AZoogiGameMode* G);
	/** items left for this player (grenade = saved + picked up this match) */
	ZOOGIROLL_API int32 Count(AZoogiGameMode* G, int32 Player, FName Id);
	/** spends one (false if none, or an item was already used this turn) */
	ZOOGIROLL_API bool Take(AZoogiGameMode* G, int32 Player, FName Id);
	ZOOGIROLL_API bool UsedThisTurn(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void MarkUsed(AZoogiGameMode* G, int32 Player);
	/** writes the human's remaining items back to the profile (match over / menu) */
	ZOOGIROLL_API void SaveBack(AZoogiGameMode* G);
}
