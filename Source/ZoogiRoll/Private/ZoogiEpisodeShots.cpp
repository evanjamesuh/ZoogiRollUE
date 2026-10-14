// GENERATED from episode-engine/gen/shots.py (spec v1.1 section 15 + 11.4). Keep in sync with SHOT_IDS.md.
#include "ZoogiEpisodeShots.h"

namespace ZoogiEpisodeShots
{
	static const FShotDef GLibrary[] = {
		{ TEXT("char_eye_closeup"), TEXT("character"), TEXT("eye close-up") },
		{ TEXT("char_focus"), TEXT("character"), TEXT("focus") },
		{ TEXT("char_fear"), TEXT("character"), TEXT("fear") },
		{ TEXT("char_surprise"), TEXT("character"), TEXT("surprise") },
		{ TEXT("char_confidence"), TEXT("character"), TEXT("confidence") },
		{ TEXT("char_irritation"), TEXT("character"), TEXT("irritation") },
		{ TEXT("char_comedy"), TEXT("character"), TEXT("comedy") },
		{ TEXT("char_celebration"), TEXT("character"), TEXT("celebration") },
		{ TEXT("char_exhaustion"), TEXT("character"), TEXT("exhaustion") },
		{ TEXT("char_rivalry_glance"), TEXT("character"), TEXT("rivalry glance") },
		{ TEXT("motion_launch"), TEXT("motion"), TEXT("launch") },
		{ TEXT("motion_spin"), TEXT("motion"), TEXT("spin") },
		{ TEXT("motion_skid"), TEXT("motion"), TEXT("skid") },
		{ TEXT("motion_wall_impact"), TEXT("motion"), TEXT("wall impact") },
		{ TEXT("motion_floor_impact"), TEXT("motion"), TEXT("floor impact") },
		{ TEXT("motion_airborne_drift"), TEXT("motion"), TEXT("airborne drift") },
		{ TEXT("motion_rebound"), TEXT("motion"), TEXT("rebound") },
		{ TEXT("motion_guard_impact"), TEXT("motion"), TEXT("guard impact") },
		{ TEXT("motion_partner_approach"), TEXT("motion"), TEXT("partner approach") },
		{ TEXT("motion_opponent_approach"), TEXT("motion"), TEXT("opponent approach") },
		{ TEXT("world_arena_orb"), TEXT("world"), TEXT("Arena Orb insert") },
		{ TEXT("world_awaken_orb"), TEXT("world"), TEXT("Awaken Orb insert") },
		{ TEXT("world_boundary"), TEXT("world"), TEXT("boundary") },
		{ TEXT("world_ring_mark_break"), TEXT("world"), TEXT("Ring Mark break") },
		{ TEXT("world_recovery_gate"), TEXT("world"), TEXT("Recovery Gate") },
		{ TEXT("world_cracked_floor"), TEXT("world"), TEXT("cracked floor") },
		{ TEXT("world_moving_platform"), TEXT("world"), TEXT("moving platform") },
		{ TEXT("world_resonance_hub"), TEXT("world"), TEXT("Resonance Hub") },
		{ TEXT("world_resonance_gauge"), TEXT("world"), TEXT("Resonance Gauge fill") },
		{ TEXT("world_environment_shift"), TEXT("world"), TEXT("environmental shift") },
		{ TEXT("power_storm_symbol"), TEXT("power"), TEXT("Storm symbol") },
		{ TEXT("power_ember_symbol"), TEXT("power"), TEXT("Ember symbol") },
		{ TEXT("power_titan_symbol"), TEXT("power"), TEXT("Titan symbol") },
		{ TEXT("power_frost_symbol"), TEXT("power"), TEXT("Frost symbol") },
		{ TEXT("power_void_symbol"), TEXT("power"), TEXT("Phase symbol (the game's Void artifact)") },
		{ TEXT("power_infuse_buildup"), TEXT("power"), TEXT("Infuse buildup") },
		{ TEXT("power_guard_buildup"), TEXT("power"), TEXT("Guard buildup") },
		{ TEXT("power_collision"), TEXT("power"), TEXT("power collision") },
		{ TEXT("power_signature_setup"), TEXT("power"), TEXT("signature attack setup") },
		{ TEXT("arch_meteor_descent"), TEXT("archetype"), TEXT("meteor descent (Titan)") },
		{ TEXT("arch_lightning_corridor"), TEXT("archetype"), TEXT("lightning corridor (Storm)") },
		{ TEXT("arch_fire_horizon"), TEXT("archetype"), TEXT("fire horizon (Ember)") },
		{ TEXT("arch_ice_wall"), TEXT("archetype"), TEXT("ice wall (Frost)") },
		{ TEXT("arch_spatial_fracture"), TEXT("archetype"), TEXT("spatial fracture") },
		{ TEXT("arch_void_isolation"), TEXT("archetype"), TEXT("void isolation (Void)") },
		{ TEXT("arch_giant_shadow"), TEXT("archetype"), TEXT("giant shadow") },
		{ TEXT("arch_symbolic_wave"), TEXT("archetype"), TEXT("symbolic wave") },
		{ TEXT("arch_allout_finisher"), TEXT("archetype"), TEXT("all-out finisher space") },
		{ TEXT("titan_meteor_a_reaction"), TEXT("titan_meteor"), TEXT("Shot A: target looks up, concentration to disbelief") },
		{ TEXT("titan_meteor_b_eye_panel"), TEXT("titan_meteor"), TEXT("Shot B: narrow eye panel, eyes widen") },
		{ TEXT("titan_meteor_c_partner_reaction"), TEXT("titan_meteor"), TEXT("Shot C: partner / another Zoogi looks up from elsewhere") },
		{ TEXT("titan_meteor_d_silhouette"), TEXT("titan_meteor"), TEXT("Shot D: only the lower edge of the enormous rock through cloud") },
		{ TEXT("titan_meteor_e_scale_reveal"), TEXT("titan_meteor"), TEXT("Shot E: camera pulls far wide, gigantic meteor descending on the target") },
	};

	static const FAssetDef GAssets[] = {
		{ TEXT("titan_meteor_a_reaction"), TEXT(""), 0, TEXT("titan_A") },
		{ TEXT("titan_meteor_b_eye_panel"), TEXT(""), 0, TEXT("titan_B") },
		{ TEXT("titan_meteor_c_partner_reaction"), TEXT(""), 0, TEXT("titan_C") },
		{ TEXT("titan_meteor_d_silhouette"), TEXT(""), 0, TEXT("titan_D") },
		{ TEXT("titan_meteor_e_scale_reveal"), TEXT(""), 0, TEXT("titan_E") },
		{ TEXT("char_surprise"), TEXT("blue"), 0, TEXT("react_surprise_blue") },
		{ TEXT("char_irritation"), TEXT("vex"), 0, TEXT("react_irritation_vex") },
		{ TEXT("char_celebration"), TEXT("tavi"), 0, TEXT("react_celebrate_tavi") },
		{ TEXT("char_comedy"), TEXT("moss"), 0, TEXT("react_comedy_moss") },
		{ TEXT("power_frost_symbol"), TEXT(""), 0, TEXT("frost_symbol") },
		{ TEXT("power_infuse_buildup"), TEXT(""), 3, TEXT("frost_infuse") },
		{ TEXT("motion_skid"), TEXT(""), 0, TEXT("frost_skid") },
		{ TEXT("arch_ice_wall"), TEXT(""), 0, TEXT("frost_icewall") },
	};

	TConstArrayView<FAssetDef> Assets() { return MakeArrayView(GAssets, UE_ARRAY_COUNT(GAssets)); }

	FName FindAsset(FName ShotId, const FString& Character, uint8 Power)
	{
		const FAssetDef* Best = nullptr;
		int32 BestScore = -1;
		for (const FAssetDef& A : GAssets)
		{
			if (ShotId != FName(A.ShotId)) continue;
			const bool bChar = A.Character[0] == 0, bCharHit = !bChar && Character.Equals(A.Character, ESearchCase::IgnoreCase);
			if (!bChar && !bCharHit) continue;
			if (A.Power != 0 && A.Power != Power) continue;
			const int32 Score = (bCharHit ? 2 : 0) + (A.Power != 0 ? 1 : 0);
			if (Score > BestScore) { BestScore = Score; Best = &A; }
		}
		return Best ? FName(Best->Key) : NAME_None;
	}

	TConstArrayView<FShotDef> Library() { return MakeArrayView(GLibrary, UE_ARRAY_COUNT(GLibrary)); }

	const FShotDef* Find(FName Id)
	{
		for (const FShotDef& D : GLibrary) if (Id == FName(D.Id)) return &D;
		return nullptr;
	}
}
