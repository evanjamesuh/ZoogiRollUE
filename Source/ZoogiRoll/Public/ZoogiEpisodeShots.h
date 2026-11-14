#pragma once

#include "CoreMinimal.h"

/** Spec v1.1 section 15 shot library + section 11.4 Titan Meteor shots A-E (table generated from gen/shots.py). */
namespace ZoogiEpisodeShots
{
	struct FShotDef
	{
		const TCHAR* Id;      // shot_id (anime manifest key)
		const TCHAR* Family;  // character | motion | world | power | archetype | titan_meteor
		const TCHAR* Desc;    // spec meaning
	};
	ZOOGIROLL_API TConstArrayView<FShotDef> Library();
	ZOOGIROLL_API const FShotDef* Find(FName Id);

	/** Comic's delivered keyframes / clips (anime/_src/<Key>/), mapped onto library shot_ids. */
	struct FAssetDef
	{
		const TCHAR* ShotId;
		const TCHAR* Character;  // "" = any
		uint8 Power;             // EZoogiArtifact, 0 = any
		const TCHAR* Key;
	};
	ZOOGIROLL_API TConstArrayView<FAssetDef> Assets();
	/** Asset key for a pick (exact character / power match preferred over "any"), NAME_None if Comic has none yet. */
	ZOOGIROLL_API FName FindAsset(FName ShotId, const FString& Character, uint8 Power);
}
