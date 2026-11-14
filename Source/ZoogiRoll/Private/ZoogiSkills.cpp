#include "ZoogiSkills.h"
#include "ZoogiVoyager.h" // b233
#include "Misc/App.h"
#include "ZoogiEarthEvents.h"
#include "ZoogiHaptics.h"
#include "ZoogiFrontProfile.h" // b223: the turret AMMO pick
#include "ZoogiClassicPad.h" // b223
#include "ZoogiRaid.h" // b223
#include "HAL/PlatformTime.h"
#include "Engine/StaticMeshActor.h"
#include "ZoogiBossAttack.h" // b190
#include "ZoogiShowtime.h" // b195
#include "ZoogiGattling.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiMusicSubsystem.h"
#include "ZoogiScreenFx.h"
#include "Containers/Ticker.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiIceBarrier.h"
#include "ZoogiCPUSmart.h"
#include "ZoogiGrenadeItem.h"
#include "ZoogiItems.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiRedirect.h"
#include "ZoogiP5Command.h"
#include "ZoogiActionFun.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiWolfPack.h"
#include "ZoogiClone.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ZoogiKFXExtras.h"
#include "ZoogiFilm.h"
#include "ZoogiSkillLab.h"
#include "ZoogiEpic.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

float GZoogiWcYawTest = -999.f; // b142c: the Skill Lab measuring pass sets the water cannon yaw offset
namespace ZoogiSkills
{
	namespace
	{
		// b142c: the water cannon system's own facing / its water ball's arrival (measured in the lab: see patch_b142c)
		constexpr float WcYawDefault = 90.f, WcHitDefault = 1.2f; // b142h: measured (patch_b142h)
		float WcYaw() { if (GZoogiWcYawTest > -900.f) return GZoogiWcYawTest; static float Y = WcYawDefault; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWaterCannonYaw="), Y); } return Y; }
		float WcHitT() { static float T = WcHitDefault; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWaterCannonHitT="), T); } return T; }
		void WcProbe(UWorld* W, AZoogiFabFX* Fx, const FVector& From, const FVector& Dir)
		{	// b142c measuring: where the system's bounds are (along the cast / to its side) every 0.1 s
			if (!Fx || !W || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWcYawTest"))) return;
			TWeakObjectPtr<AZoogiFabFX> WF = Fx; const double T0 = FPlatformTime::Seconds(); const float Yw = WcYaw();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WF, T0, From, Dir, Yw](float) -> bool
			{
				AZoogiFabFX* F = WF.Get(); const double T = FPlatformTime::Seconds() - T0;
				if (!F || !F->NC || T > 1.7) return false;
				const FBoxSphereBounds Bd = F->NC->Bounds; const FVector D = Bd.Origin - From; const FVector Side(-Dir.Y, Dir.X, 0.f);
				UE_LOG(LogZoogi, Log, TEXT("WC PROBE yaw %+.0f t %.2f: bounds along %.0f side %.0f up %.0f r %.0f ext (%.0f,%.0f,%.0f)"), Yw, T, FVector::DotProduct(D, Dir), FVector::DotProduct(D, Side), D.Z, Bd.SphereRadius, Bd.BoxExtent.X, Bd.BoxExtent.Y, Bd.BoxExtent.Z);
				return true;
			}), 0.1f);
		}
		const TCHAR* FX(const TCHAR* N) { static TMap<FString, FString> M; FString& S = M.FindOrAdd(N); if (S.IsEmpty()) S = FString::Printf(TEXT("/Game/StylizedToonVFX3/Effects/%s.%s"), N, N); return *S; }
		const TCHAR* SFX(const TCHAR* N) { static TMap<FString, FString> M; FString& S = M.FindOrAdd(N); if (S.IsEmpty()) S = FString::Printf(TEXT("/Game/StylizedToonVFX3/Audio/Cues/%s_Cue.%s_Cue"), N, N); return *S; }

		struct FDef { const TCHAR* Id; EAim Aim; float R; int32 Count; const TCHAR* Hint; };
		const FDef Defs[] = {
			{ TEXT("meteor_rain"), EAim::Area, 430.f, 1, TEXT("DRAG THE CIRCLE INSIDE YOUR RANGE") },
			{ TEXT("fire_meteors"), EAim::Area, 230.f, 1, TEXT("DRAG THE CIRCLE INSIDE YOUR RANGE") },
			{ TEXT("ice_wall"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("water_cannon"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("magic_rays"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 TARGETS") },
			{ TEXT("sword_slash"), EAim::Self, 380.f, 1, TEXT("") },
			// b272 (review "New skills")
			{ TEXT("grapple_hook"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB - YOU ARE YANKED TO IT") },
			{ TEXT("edge_save"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("bounce_pad"), EAim::Spots, 0.f, 1, TEXT("TAP WHERE THE BOUNCE PAD GOES") },
			{ TEXT("ricochet"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("decoy"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("tether"), EAim::MultiTap, 0.f, 2, TEXT("TAP TWO RIVALS TO CHAIN TOGETHER") },
			{ TEXT("tide_shift"), EAim::Area, 300.f, 1, TEXT("DRAG THE CIRCLE - THE ARENA TILTS THAT WAY") },
			{ TEXT("hex"), EAim::Tap, 0.f, 1, TEXT("TAP THE RIVAL TO HEX") },
			{ TEXT("star_magnet"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("energy_shockwave"), EAim::Area, 450.f, 1, TEXT("DRAG THE PULSE WHERE THE ORBS GATHER") }, // b271 (review: too strong for 1 SP) was 600
			{ TEXT("energy_buff"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("wood_shield"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("apple_bombs"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS FOR THE APPLE MINES") },
			{ TEXT("golden_apple"), EAim::Tap, 0.f, 1, TEXT("TAP AN ORB OR A WOOD SHIELD") }, // b188: an ITEM now (Evan), breaks wood shields
			// b66 (Evan): "wolf pack is a skill and can only be activated as such" + Bolt Balls (the old electric effect)
			{ TEXT("wolf_pack"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("bolt_balls"), EAim::Self, 0.f, 1, TEXT("") },
			// b190 (Evan): the GATTLING skills
			{ TEXT("gattling_burst"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("gattling_beat"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("gattling_turret"), EAim::Spots, 0.f, 1, TEXT("TAP WHERE THE TURRET GOES") },
			{ TEXT("halo_disc"), EAim::Line, 0.f, 1, TEXT("SWIPE THE DISC'S PATH") },
			{ TEXT("precision"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 TARGETS, THEN FIRE") },
			// b176a: the bought Fab packs (Ice Attacks, Earthbender, Stylized Slashes)
			{ TEXT("frost_spikes"), EAim::Area, 360.f, 1, TEXT("DRAG THE CIRCLE - ICE SPIKES BURST THERE") },
			{ TEXT("glacier_lance"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("ice_shards"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS FOR THE ICE SHARDS") },
			{ TEXT("stone_pillar"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 RIVALS OR ORBS - A PILLAR BURSTS UNDER EACH") }, // b178k: was 1 target
			{ TEXT("earthquake"), EAim::Area, 560.f, 1, TEXT("DRAG THE CIRCLE - THE GROUND CRACKS THERE") },
			{ TEXT("rock_rampart"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE ROCK WALL BURSTS UP") },
			{ TEXT("crescent_slash"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 RIVALS OR ORBS - A SLASH FLIES AT EACH") }, // b179f (Evan): was a swipe
			{ TEXT("cross_slash"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("ground_slash"), EAim::Line, 0.f, 1, TEXT("SWIPE THE WAVE'S PATH") },
			// b178a (Evan: "add all earth bending effects as skills"): the rest of the Earthbender pack
			{ TEXT("boulder_block"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("sandstone_spire"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("stone_barricade"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("desert_rubble"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS") },
			{ TEXT("jade_boulder"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("rock_burst"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("boulder_fountain"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("slab_row"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("jade_wall"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("snowcap_pillar"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("granite_block"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("moss_tower"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("frostrock_grove"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("obsidian_spike"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("obsidian_wall"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("obsidian_cluster"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("obsidian_burst"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("sapphire_pillar"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("sapphire_burst"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("sapphire_block"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("gold_rush"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("gold_nugget"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("gold_wall"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("rune_pillar"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("rune_wall"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("rune_monolith"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("stone_teeth"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS") },
			{ TEXT("toppled_columns"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS") },
			{ TEXT("landslide"), EAim::Line, 0.f, 1, TEXT("SWIPE THE FAULT LINE") },
			{ TEXT("starburst_spikes"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("rock_geyser"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("crater_ring"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("gold_column"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("jade_columns"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS") },
			{ TEXT("sapphire_wall"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("sandstone_ruins"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("obsidian_star"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("earth_flash"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("rubble_storm"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("stone_cage"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("pillar_field"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS") },
			{ TEXT("fault_line"), EAim::Line, 0.f, 1, TEXT("SWIPE THE FAULT LINE") },
			{ TEXT("shatter_star"), EAim::Area, 380.f, 1, TEXT("DRAG THE CIRCLE - THE EARTH ERUPTS THERE") },
			{ TEXT("slab_slam"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("rift_crack"), EAim::Line, 0.f, 1, TEXT("SWIPE THE FAULT LINE") },
			{ TEXT("long_rampart"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE WALL RISES") },
			{ TEXT("boulder_pile"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") } }; // b137b: PRECISION = 3 water-cannon shots // b108: SKILL LAB is a home button now, not a skill; b137: halo_disc = the HALO DISC weapon (not a card)
		bool ImpulseNow(); // b136: meteor_rain is IMPULSE (no pick) unless an Awaken power casts it
		const FDef ImpulseDef = { TEXT("meteor_rain"), EAim::Self, 0.f, 1, TEXT("") };
		bool OldPrecision() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPrecision")); return b; }
		const FDef* Find(FName Id)
		{
			if (Id == FName(TEXT("precision")) && OldPrecision()) return nullptr; // b117 missile volley (not a skill pick)
			for (const FDef& D : Defs) if (Id == FName(D.Id)) return (Id == FName(TEXT("meteor_rain")) && ImpulseNow()) ? &ImpulseDef : &D;
			return nullptr;
		}

		struct FPick
		{
			bool bOn = false, bConfirmed = false, bDrag = false;
			int32 Slot = -1; FName Id; EAim Aim = EAim::Self; float R = 0.f; int32 Count = 1;
			TWeakObjectPtr<AZoogiBall> Caster;
			FVector Origin = FVector::ZeroVector; float Range = 900.f, FloorZ = 0.f;
			FVector Ctr = FVector::ZeroVector, A = FVector::ZeroVector, B = FVector::ZeroVector;
			TArray<TWeakObjectPtr<AActor>> Targets;
			TArray<FVector> Spots;
			FBox2D FireBtn = FBox2D(ForceInit), CancelBtn = FBox2D(ForceInit);
			double OpenedAt = 0.0;
			// b67: the CPU goes through the pick (cursor glides to its choice, then FIRE)
			bool bCpu = false; double CpuFireAt = 0.0;
			bool bGrenade = false; // b130: the PRISM GRENADE throw pick (no card, no SP)
			bool bWeapon = false; FName WeaponId; // b137: a weapon from the item collection (no card, no SP)
			bool bComboCast = false; // b122c: a COMBO cast's pick - FIRE casts it (already paid), CANCEL puts it back
			FVector CpuCtr = FVector::ZeroVector, CpuA = FVector::ZeroVector, CpuB = FVector::ZeroVector;
			TArray<TWeakObjectPtr<AActor>> CpuTargets; TArray<FVector> CpuSpots;
		} S;

		struct FEvent { float At = 0.f; TFunction<void()> Fn; };
		TArray<FEvent> Events;
		double GLastCastAt = -100.0; // b76: real time of the last skill cast
		struct FWall { TWeakObjectPtr<AActor> Collider; TArray<TWeakObjectPtr<AZoogiFabFX>> Fx; float Until = 0.f; };
		TArray<FWall> Walls;
		struct FMine { FVector P; int32 Owner = -1; TWeakObjectPtr<AZoogiFabFX> Fx; float Pow = 1.f; }; // b136: Pow = the cast's strength
		// b139a: the Volcano's meteor strikes (warning circle -> meteors)
		struct FVolcWarn { FVector P = FVector::ZeroVector; float R = 260.f; double T0 = 0.0, FireAt = 0.0; bool bFired = false; };
		TArray<FVolcWarn> VolcWarns; int32 VolcTurn = -1;
		struct FGoldApple { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector; int32 Owner = -1; double ArmAt = 0.0; float Spin = 0.f; }; // b138c: the apple left on the court
		TArray<FGoldApple> GoldApples;
		void DropGoldApples() { for (FGoldApple& Ga : GoldApples) if (AActor* A = Ga.A.Get()) A->Destroy(); GoldApples.Reset(); }
		TArray<FMine> Mines;
		// b271 (Evan: "build everything as mentioned in the review doc"): skill review fixes
		struct FPlusWall { FVector A, B; int32 Owner = -1; int32 Until = 0; }; // wall payoff: own ball +25% off it, a rival that hits it loses 1 SP
		TArray<FPlusWall> PlusWalls;
		struct FPatch { FVector P; float R = 260.f; int32 Owner = -1; int32 Until = 0; TWeakObjectPtr<AZoogiFabFX> Fx; }; // Ice Shards: slippery ice patches
		TArray<FPatch> Patches;
		struct FTrap { FVector P; float R = 200.f; int32 Owner = -1; int32 Until = 0; float Pow = 1.f; double ArmAt = 0.0; }; // Frost Spikes: a hidden trap
		TArray<FTrap> Traps;
		struct FCounter { TWeakObjectPtr<AZoogiBall> B; int32 Until = 0; float Pow = 1.f; }; // Cross Slash: the next rival who hits you is slashed away
		TArray<FCounter> Counters;
		struct FSpin { TWeakObjectPtr<AZoogiBall> B; double End = 0.0, Next = 0.0; float Pow = 1.f; int32 Pi = -1; }; // Sword Slash: a spin dash
		TArray<FSpin> Spins;
		TMap<TWeakObjectPtr<AZoogiBall>, FVector> PlusPrevVel;
		TMap<TWeakObjectPtr<AZoogiBall>, double> PlusWallCool;
		// b272 new skills
		struct FNewFx { int32 Kind = 0; TWeakObjectPtr<AZoogiBall> B, B2; FVector P = FVector::ZeroVector, Dir = FVector::ZeroVector, PrevV = FVector::ZeroVector; int32 Owner = -1, Until = 0; double NextFx = 0.0; float Sp1 = 0.f, Sp2 = 0.f; bool bArmed = false; };
		enum { NK_EdgeSave = 1, NK_Pad, NK_Rico, NK_Decoy, NK_Tether, NK_Tide, NK_Hex, NK_Magnet, NK_Fault }; // b273 NK_Fault
		TArray<FNewFx> NewFx;
		TMap<TWeakObjectPtr<AActor>, double> PadCool;
		// b271 (review): ELEMENT BONUS - two skills of one element in your deck: Fire +10% skill knockback, Frost: orbs you hit slide further,
		// Shock: +1 free redirect crystal at the start, Void: +1 crystal per knockout. -ZoogiNoElementBonus turns it off.
		TWeakObjectPtr<AZoogiGameMode> ElemG; int32 ElemTurn = -1; int32 ElemOf[4] = { 0, 0, 0, 0 }; bool bElemDone = false; bool ElemOut[8] = {};
		int32 ElemBonusOf(int32 Pi) { return (Pi >= 0 && Pi < 4) ? ElemOf[Pi] : 0; } // (int)EZoogiCardElement, 0 = none
		struct FCrater { FVector P; float R = 300.f; int32 Turn = -1; };
		TArray<FCrater> Craters;
		struct FBuff { TWeakObjectPtr<AZoogiBall> B; float SavedMass = 0.f; int32 Turn = -1; bool bVelPending = false; };
		TArray<FBuff> Buffs;
		struct FShield { TWeakObjectPtr<AZoogiBall> B; float SavedMass = 0.f; bool bHeavy = false; float HitAt = -1.f; int32 Turn = -1; float NextFx = 0.f; TWeakObjectPtr<AZoogiFabFX> Fx; double CastAt = 0.0; bool bFrozen = false; bool bLocked = false; FVector Anchor = FVector::ZeroVector; };
		static void DropShieldFx(FShield& Sh)
		{
			if (AZoogiFabFX* F = Sh.Fx.Get()) F->Destroy(); Sh.Fx.Reset();
			if (Sh.bLocked) if (AZoogiBall* B = Sh.B.Get()) if (B->Sphere) { B->Sphere->SetSimulatePhysics(true); B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); } // b177k: free again
			Sh.bLocked = false;
		}
		static bool WoodLock() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWoodShieldHeavy")); return b; } // b177k: -ZoogiWoodShieldHeavy = the old x6 mass
		bool WoodStays() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWoodShield")); return b; }
		TArray<FShield> Shields;
		bool HasShield(const AZoogiBall* B) { return B && Shields.ContainsByPredicate([B](const FShield& X) { return X.B.Get() == B; }); }
		// b188 (Evan: "golden apple ... can break a wooden guard"): a golden apple blast shatters every wood shield it reaches -
		// the dome goes, the Zoogi is light again and is thrown out of the blast
		int32 BreakShieldsNear(AZoogiGameMode* G, const FVector& P, float R)
		{
			int32 N = 0;
			for (int32 i = Shields.Num() - 1; i >= 0; --i)
			{
				FShield& Sh = Shields[i];
				AZoogiBall* B = Sh.B.Get();
				if (!B || B->bOut || FVector::Dist2D(B->BallLocation(), P) > R + ZoogiConst::ZoogiRadius) continue;
				DropShieldFx(Sh); // physics back on
				if (B->Sphere)
				{
					if (Sh.SavedMass > 0.f) B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true);
					if (B->Sphere->IsSimulatingPhysics())
					{
						FVector D = B->BallLocation() - P; D.Z = 0.f;
						const FVector Dir = D.Size() > 1.f ? D.GetSafeNormal() : FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
						B->Sphere->SetPhysicsLinearVelocity(Dir * 950.f + FVector(0.f, 0.f, 240.f));
					}
				}
				if (G)
				{
					const FLinearColor Wood(0.62f, 0.42f, 0.2f);
					AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), Wood, 260.f, 0.5f, true, 12.f);
					AZoogiFX::Shockwave(G->GetWorld(), B->BallLocation(), EZoogiShockwaveSize::Normal, EZoogiShockwaveKind::Flat, &Wood, false);
					ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.8f);
					if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("SHIELD BROKEN!"), FString::Printf(TEXT("THE GOLDEN APPLE SHATTERS P%d'S WOOD SHIELD"), B->PlayerIndex + 1), FLinearColor(1.f, 0.8f, 0.25f), 1.4f, 1.f);
				}
				UE_LOG(LogZoogi, Log, TEXT("SKILL golden apple broke P%d's wood shield"), B->PlayerIndex + 1);
				Shields.RemoveAt(i); ++N;
			}
			return N;
		}
		TWeakObjectPtr<AZoogiGameMode> ForGame; int32 ForSeed = 0, LastTurn = -1;

		float GSkillPow = 1.f; bool GAwakenNext = false; // b100: card skills 0.6, Awaken powers 1
		float CardPow() { static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillFullPower")); return bFull ? 1.f : 0.6f; }
		bool ImpulseNow() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeteorRain")); return !bOld && !GAwakenNext; }
		const TCHAR* SkillBlastFx() { return ZoogiSkillLab::BlastFx(GSkillPow >= 0.99f ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion")); } // b100 (Evan): smaller billow blasts for skills; b107: the lab's pick
		float SkillBlastSc() { return (GSkillPow >= 0.99f ? 1.f : 0.8f) * ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize(); } // b139b: x BLAST SIZE
		/** b139b: where a skill hits, the lab's chosen BLAST goes off (only when one is picked - DEFAULT adds nothing) */
		void LabImpact(UWorld* W, const FVector& At, float Base = 0.35f)
		{
			const TCHAR* Bx = ZoogiSkillLab::ChosenBlast();
			if (!W || !Bx || !*Bx) return;
			AZoogiFX::Stylized(W, Bx, At, Base * ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize());
		}
		/** b139b: a skill's own fixed blast honours the lab pick + BLAST SIZE */
		void SkillStyl(UWorld* W, const TCHAR* Def, const FVector& At, float Sc0) { const TCHAR* Bx = ZoogiSkillLab::BlastFx(Def); if (W && Bx && *Bx) AZoogiFX::Stylized(W, Bx, At, Sc0 * ZoogiSkillLab::BlastSize()); }
		float Now(const AZoogiGameMode* G) { return G && G->GetWorld() ? G->GetWorld()->GetTimeSeconds() : 0.f; }
		/** b136: GSkillPow + the lab's current skill for a block of code, restored after */
		struct FTuneScope
		{
			float SavedPow; FName SavedSkill;
			FTuneScope(float Pow, FName Skill) : SavedPow(GSkillPow), SavedSkill(ZoogiSkillLab::Current()) { GSkillPow = Pow; if (!Skill.IsNone()) ZoogiSkillLab::SetCurrent(Skill); }
			~FTuneScope() { GSkillPow = SavedPow; ZoogiSkillLab::SetCurrent(SavedSkill); }
		};
		// b190 FAST FX (Evan picked "fast fx"): every skill's delayed part lands sooner - short waits x0.6, long ones about half
		// (-ZoogiSlowFx = the old timing)
		float FastD(float D)
		{
			static const bool bSlow = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSlowFx"));
			if (bSlow || D <= 0.f) return D;
			return D <= 0.25f ? D * 0.6f : 0.15f + (D - 0.25f) * 0.55f;
		}
		void Later(AZoogiGameMode* G, float Delay, TFunction<void()> Fn)
		{
			Delay = FastD(Delay);	// b136 (Evan: the lab tunings must reach every skill): the delayed part runs with the strength + lab skill of its cast
			FEvent E; E.At = Now(G) + Delay;
			const float Pw = GSkillPow; const FName Sk = ZoogiSkillLab::Current();
			E.Fn = [Pw, Sk, Inner = MoveTemp(Fn)]() { FTuneScope T(Pw, Sk); Inner(); };
			Events.Add(MoveTemp(E));
		}
		// b178k: a real-time delay for staggered bursts (game-time Laters bunched up after a slow-mo frame: 2 of 3 pillars landed together)
		void LaterReal(float Delay, TFunction<void()> Fn)
		{
			Delay = FastD(Delay);
			const float Pw = GSkillPow; const FName Sk = ZoogiSkillLab::Current();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Pw, Sk, Inner = MoveTemp(Fn)](float) { FTuneScope T(Pw, Sk); Inner(); return false; }), FMath::Max(0.01f, Delay));
		}
		float Sc(FName Id, float Def)
		{
			// -ZoogiSkillScale=meteor_rain:0.8,ice_wall:0.3 (tuning without a rebuild)
			FString All; if (!FParse::Value(FCommandLine::Get(), TEXT("ZoogiSkillScale="), All)) return Def;
			TArray<FString> Parts; All.ParseIntoArray(Parts, TEXT(","));
			for (const FString& P : Parts) { FString K, V; if (P.Split(TEXT(":"), &K, &V) && FName(*K) == Id) return FCString::Atof(*V); }
			return Def;
		}
		void Kick(UPrimitiveComponent* Sp, const FVector& DV)
		{
			if (!Sp || !Sp->IsSimulatingPhysics()) return;
			Sp->WakeAllRigidBodies();
			// b179 (Evan: "knockouts should count even if it was with an item or skill"): a Zoogi a skill / item throws is credited to
			// the player whose turn it is (the caster), so a knock-off scores as their KNOCKOUT
			if (AZoogiBall* Kb = Cast<AZoogiBall>(Sp->GetOwner()))
				if (UWorld* Kw = Sp->GetWorld()) if (AZoogiGameMode* Kg = Kw->GetAuthGameMode<AZoogiGameMode>())
					if (!Kg->IsRoyaleMatch() && Kg->Balls.IsValidIndex(Kg->ActivePlayer) && Kg->ActivePlayer != Kb->PlayerIndex && !Kg->IsAlly(Kg->Balls[Kg->ActivePlayer], Kb)) Kb->LastHitPlayer = Kg->ActivePlayer;
			FVector D = DV * GSkillPow; D.Z *= ZoogiSkillLab::Height(); // b177o: lab HEIGHT
			Sp->SetPhysicsLinearVelocity(Sp->GetPhysicsLinearVelocity() + D);
		}
		/** Radial knock from P: balls + orbs within R (Caster spared when bSpare). */
		int32 Blast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up, const AZoogiBall* Spare, int32 Credit)
		{
			int32 N = 0;
			LabImpact(G ? G->GetWorld() : nullptr, P, 0.3f * FMath::Max(0.6f, R / 300.f)); // b139b
			R *= ZoogiSkillLab::Radius(); // b107
			ZoogiEarthEvents::BlastAt(G, P, R, Credit); // b180: skill blasts smash the event rocks
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut || B == Spare) continue;
				const FVector D = B->BallLocation() - P; const float Dd = D.Size2D();
				if (Dd > R) continue;
				Kick(B->Sphere, D.GetSafeNormal2D() * Speed * (1.f - 0.5f * Dd / R) + FVector(0, 0, Up)); ++N;
				if (Credit >= 0 && B->PlayerIndex != Credit) B->LastHitPlayer = Credit; // b179: the blast's owner gets the KO
			}
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !O->Sphere) continue;
				const FVector D = O->OrbLocation() - P; const float Dd = D.Size2D();
				if (Dd > R) continue;
				Kick(O->Sphere, D.GetSafeNormal2D() * Speed * 0.8f * (1.f - 0.5f * Dd / R) + FVector(0, 0, Up * 0.7f));
				if (Credit >= 0) O->LastHitBy = Credit;
				++N;
			}
			return N;
		}
		/** b142n: the volcano event - Zoogis only, nobody's credit (the struck Zoogi's last hitter is cleared) */
		int32 VolcanoBlast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up)
		{
			int32 N = 0;
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut) continue;
				const FVector D = B->BallLocation() - P; const float Dd = D.Size2D();
				if (Dd > R) continue;
				Kick(B->Sphere, D.GetSafeNormal2D() * Speed * (1.f - 0.5f * Dd / R) + FVector(0, 0, Up));
				B->LastHitPlayer = -1;
				++N;
			}
			if (N) UE_LOG(LogZoogi, Log, TEXT("VOLCANO meteor hit %d Zoogi(s) - no credit, orbs untouched"), N);
			return N;
		}
		FVector Floor(const FVector& P) { return FVector(P.X, P.Y, S.FloorZ + 2.f); }
		bool ScreenToFloor(APlayerController* PC, const FVector2D& Ptr, FVector& Out)
		{
			FVector O, D;
			if (!PC || !PC->DeprojectScreenPositionToWorld(Ptr.X, Ptr.Y, O, D) || FMath::Abs(D.Z) < 1e-3f) return false;
			const float T = (S.FloorZ - O.Z) / D.Z;
			if (T <= 0.f) return false;
			Out = O + D * T;
			return true;
		}
		FVector ClampRange(const FVector& P, float Inset = 0.f)
		{
			FVector2D D(P.X - S.Origin.X, P.Y - S.Origin.Y);
			const float Mx = FMath::Max(50.f, S.Range - Inset);
			if (D.Size() > Mx) D = D.GetSafeNormal() * Mx;
			return FVector(S.Origin.X + D.X, S.Origin.Y + D.Y, S.FloorZ);
		}
		// b178k (Evan: Stone Pillar's arc "increased to the red line"): some skills reach further than the launch arc
		float RangeMul(FName Id) { static float Mul = -1.f; if (Mul < 0.f) { Mul = 2.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiPillarRange="), Mul); } return Id == FName(TEXT("stone_pillar")) ? Mul : 1.f; }
		void Candidates(AZoogiGameMode* G, TArray<AActor*>& Out)
		{
			const AZoogiBall* Cb = S.Caster.Get();
			const bool bOrbsOnly = S.Id == FName(TEXT("golden_apple"));
			for (AZoogiBall* B : G->Balls) if (B && B != Cb && !B->bOut && (!bOrbsOnly || HasShield(B)) && FVector::Dist2D(B->BallLocation(), S.Origin) <= S.Range) Out.Add(B); // b188: the apple can target a wood shield
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && !(bOrbsOnly && O->bStar) && FVector::Dist2D(O->OrbLocation(), S.Origin) <= S.Range) Out.Add(O);
		}
		FVector ActorPos(AActor* A)
		{
			if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
			if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
			return A ? A->GetActorLocation() : FVector::ZeroVector;
		}
		bool CanFire()
		{
			switch (S.Aim)
			{
			case EAim::Line: return FVector::Dist2D(S.A, S.B) >= 150.f;
			case EAim::Tap: case EAim::MultiTap: return S.Targets.Num() > 0;
			case EAim::Spots: return S.Spots.Num() > 0;
			default: return true;
			}
		}
		void Reset() { S = FPick(); }
	}

	bool IsSkill(FName Id) { return Find(Id) != nullptr; }
	bool IsEventOnlyEarth(FName Id)
	{
		static const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAllEarthSkills"));
		if (bAll) return false;
		static const TSet<FName> Ev = { FName(TEXT("boulder_block")), FName(TEXT("sandstone_spire")), FName(TEXT("stone_barricade")), FName(TEXT("desert_rubble")), FName(TEXT("jade_boulder")), FName(TEXT("rock_burst")), FName(TEXT("boulder_fountain")), FName(TEXT("slab_row")), FName(TEXT("jade_wall")), FName(TEXT("snowcap_pillar")), FName(TEXT("granite_block")), FName(TEXT("moss_tower")), FName(TEXT("frostrock_grove")), FName(TEXT("obsidian_spike")), FName(TEXT("obsidian_wall")), FName(TEXT("obsidian_cluster")), FName(TEXT("obsidian_burst")), FName(TEXT("sapphire_pillar")), FName(TEXT("sapphire_burst")), FName(TEXT("sapphire_block")), FName(TEXT("gold_rush")), FName(TEXT("gold_nugget")), FName(TEXT("gold_wall")), FName(TEXT("rune_pillar")), FName(TEXT("rune_wall")), FName(TEXT("rune_monolith")), FName(TEXT("stone_teeth")), FName(TEXT("toppled_columns")), FName(TEXT("landslide")), FName(TEXT("starburst_spikes")), FName(TEXT("rock_geyser")), FName(TEXT("crater_ring")), FName(TEXT("gold_column")), FName(TEXT("jade_columns")), FName(TEXT("sandstone_ruins")), FName(TEXT("obsidian_star")), FName(TEXT("earth_flash")), FName(TEXT("rubble_storm")), FName(TEXT("stone_cage")), FName(TEXT("shatter_star")), FName(TEXT("slab_slam")), FName(TEXT("rift_crack")), FName(TEXT("boulder_pile")) };
		return Ev.Contains(Id) || Id == FName(TEXT("ground_slash")); // b179: Ground Slash belongs to the Volcano Golem now
	}
	TArray<FName> AllSkillIds() { TArray<FName> Out; for (const FDef& D : Defs) if (FCString::Strcmp(D.Id, TEXT("skill_lab")) != 0 && FCString::Strcmp(D.Id, TEXT("halo_disc")) != 0 && FCString::Strcmp(D.Id, TEXT("long_rampart")) != 0 && FCString::Strcmp(D.Id, TEXT("magic_rays")) != 0 /* b266 */ && FCString::Strcmp(D.Id, TEXT("fire_meteors")) != 0 && FCString::Strcmp(D.Id, TEXT("golden_apple")) != 0 && FCString::Strcmp(D.Id, TEXT("gattling_turret")) != 0 && FCString::Strcmp(D.Id, TEXT("stone_pillar")) != 0 && FCString::Strcmp(D.Id, TEXT("earthquake")) != 0 && FCString::Strcmp(D.Id, TEXT("rock_rampart")) != 0 && !IsEventOnlyEarth(FName(D.Id))) Out.Add(FName(D.Id)); return Out; } // b203: turret = item; 3 earth skills removed // b179: + the event-only Earthbender effects stay out // b139a: fire_meteors is a Volcano event now
	EAim AimOf(FName Id) { const FDef* D = Find(Id); return D ? D->Aim : EAim::Self; }
	bool Picking() { return S.bOn && !S.bConfirmed; }
	void Cancel(const TCHAR* Why)
	{
		if (S.bOn) UE_LOG(LogZoogi, Log, TEXT("SKILL pick %s cancelled (%s)"), *S.Id.ToString(), Why);
		if (S.bOn && !S.bConfirmed && S.bComboCast) if (AZoogiBall* Cb = S.Caster.Get()) if (AZoogiGameMode* Gc = Cb->GetWorld()->GetAuthGameMode<AZoogiGameMode>()) ComboPutBack(Gc, Cb->PlayerIndex, S.Id); // b122c
		Reset();
	}

	bool WantPick(AZoogiGameMode* G, int32 Slot, FName Id)
	{
		const FDef* D = Find(Id);
		if (!D || !G) return false;
		if (S.bOn && S.bConfirmed && S.Slot == Slot && S.Id == Id) return false; // FIRE pressed: play it now
		AZoogiBall* B = G->GetActiveBall();
		if (!B) return false;
		Reset();
		S.Slot = Slot; S.Id = Id; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
		S.Range = FMath::Max(400.f, G->ArcRange()) * RangeMul(Id); // b178k
		if (D->Aim == EAim::Self) { S.bOn = true; S.bConfirmed = true; return false; }
		S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
		// a sensible first spot: towards the nearest rival, else straight ahead
		FVector Aim = S.Origin + FVector(S.Range * 0.5f, 0.f, 0.f);
		float Best = 1e9f;
		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < Best) { Best = Dd; Aim = O->BallLocation(); } }
		S.Ctr = ClampRange(Aim, 0.f); S.A = S.B = S.Ctr;
		UE_LOG(LogZoogi, Log, TEXT("SKILL pick %s (slot %d): aim %d, range %.0f"), *Id.ToString(), Slot, (int32)D->Aim, S.Range);
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.3f);
		return true;
	}

	void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease)
	{
		if (!Picking() || !G) return;
		if (S.bCpu) return; // b67: the CPU's own pick - hands off
		if (bPress && S.CancelBtn.bIsValid && S.CancelBtn.IsInside(Ptr)) { const bool bWasCombo = S.bComboCast || S.bGrenade || S.bWeapon; ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f); Cancel(TEXT("CANCEL")); if (!bWasCombo) ZoogiP5Cmd::BackToSkills(); return; }
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bGrenade)
		{	// b130: throw the grenade at the circle
			AZoogiBall* Cb = S.Caster.Get(); const FVector At = S.Ctr;
			S.bConfirmed = true; Reset();
			if (Cb && ZoogiItems::Take(G, Cb->PlayerIndex, FName(TEXT("grenade")))) { ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.1f); ZoogiGrenade::Lob(G, Cb, At); } // b137: pickups first, then the saved ones
			return;
		}
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bWeapon)
		{	// b137: fire the weapon - one from the collection, full strength, no SP
			if (!CanFire()) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			AZoogiBall* Cb = S.Caster.Get();
			if (!Cb || !ZoogiItems::Take(G, Cb->PlayerIndex, S.WeaponId)) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); Cancel(TEXT("no item")); return; }
			S.bConfirmed = true; GAwakenNext = true;
			UE_LOG(LogZoogi, Log, TEXT("ITEM %s fired (%s)"), *S.WeaponId.ToString(), *S.Id.ToString());
			Execute(G, Cb, S.Id);
			return;
		}
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bComboCast)
		{	// b122c: a COMBO cast - already paid when armed, cast it now and play resumes
			if (!CanFire()) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			S.bConfirmed = true;
			if (AZoogiBall* Cb = S.Caster.Get()) { UE_LOG(LogZoogi, Log, TEXT("COMBO cast %s fired from its pick"), *S.Id.ToString()); Execute(G, Cb, S.Id); }
			return;
		}
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr))
		{
			if (!CanFire()) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			S.bConfirmed = true;
			const int32 Slot = S.Slot;
			if (!G->CardsTapPlay(Slot)) Reset(); // rejected (energy / window): nothing spent
			ZoogiP5Cmd::ToSkillButton(); // b79 (Evan): fold to the SKILL button so the effect shows (b73 reopened the list)
			return;
		}
		FVector P;
		const bool bHit = ScreenToFloor(PC, Ptr, P);
		switch (S.Aim)
		{
		case EAim::Area:
			if (bHit && (bPress || (bDown && S.bDrag))) { S.Ctr = ClampRange(P); S.bDrag = true; }
			if (bRelease) S.bDrag = false;
			break;
		case EAim::Line:
			if (bHit && bPress) { S.A = S.B = ClampRange(P); S.bDrag = true; }
			else if (bHit && bDown && S.bDrag)
			{
				FVector Bp = ClampRange(P);
				const FVector2D D(Bp.X - S.A.X, Bp.Y - S.A.Y);
				if (D.Size() > 650.f) Bp = FVector(S.A.X + D.GetSafeNormal().X * 650.f, S.A.Y + D.GetSafeNormal().Y * 650.f, S.FloorZ);
				S.B = Bp;
			}
			if (bRelease) S.bDrag = false;
			break;
		case EAim::Tap: case EAim::MultiTap:
			if (bPress)
			{
				TArray<AActor*> Cand; Candidates(G, Cand);
				int32 VpX = 0, VpY = 0; if (PC) PC->GetViewportSize(VpX, VpY);
				AActor* Best = nullptr; float BestD = FMath::Max(110.f, 0.11f * (float)FMath::Min(VpX, VpY)); // b108 (Grok, tablet): a finger-sized radius
				for (AActor* A : Cand) { FVector2D Sp; if (PC->ProjectWorldLocationToScreen(ActorPos(A), Sp, true)) { const float Dd = FVector2D::Distance(Sp, Ptr); if (Dd < BestD) { BestD = Dd; Best = A; } } }
				if (!Best) break;
				const int32 Was = S.Targets.IndexOfByPredicate([Best](const TWeakObjectPtr<AActor>& T) { return T.Get() == Best; });
				if (Was != INDEX_NONE) S.Targets.RemoveAt(Was);
				else { if (S.Aim == EAim::Tap) S.Targets.Reset(); if (S.Targets.Num() < S.Count) S.Targets.Add(Best); }
				ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.7f, 1.2f);
			}
			break;
		case EAim::Spots:
			if (bPress && bHit)
			{
				const FVector Q = ClampRange(P, 40.f);
				const int32 Was = S.Spots.IndexOfByPredicate([&Q](const FVector& V) { return FVector::Dist2D(V, Q) < 90.f; });
				if (Was != INDEX_NONE) S.Spots.RemoveAt(Was);
				else if (S.Spots.Num() < S.Count) S.Spots.Add(Q);
				ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.7f, 1.2f);
			}
			break;
		default: break;
		}
	}

	// ------------------------------------------------------------------ drawing
	static void Ring(UCanvas* C, APlayerController* PC, const FVector& Ctr, float R, float Th, const FLinearColor& Col, int32 Seg, bool bDash)
	{
		FVector2D Prev; bool bPrev = false;
		for (int32 k = 0; k <= Seg; ++k)
		{
			const float A = 2.f * PI * k / Seg;
			FVector2D Sp; const bool bOk = PC->ProjectWorldLocationToScreen(Ctr + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f), Sp, true);
			if (bOk && bPrev && (!bDash || k % 2 == 0)) { FCanvasLineItem L(Prev, Sp); L.LineThickness = Th; L.SetColor(Col); C->DrawItem(L); }
			Prev = Sp; bPrev = bOk;
		}
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (G && C && PC && VolcWarns.Num())
		{	// b139a: the P5 warning circle on the floor before a Volcano meteor strike
			const double Nw = Now(G), Nr = FPlatformTime::Seconds(); // b178i: the countdown runs on game time (frozen while a pick waits)
			const float Uw = FMath::Min(C->ClipX, C->ClipY) / 1080.f;
			for (const FVolcWarn& Wv : VolcWarns)
			{
				if (Wv.bFired || Nw < Wv.T0) continue;
				const float K = FMath::Clamp((float)((Nw - Wv.T0) / (Wv.FireAt - Wv.T0)), 0.f, 1.f);
				const float Pulse = 0.6f + 0.4f * FMath::Abs(FMath::Sin((float)Nr * 14.f));
				Ring(C, PC, Wv.P, Wv.R + 18.f, 16.f * Uw, FLinearColor(0.02f, 0.02f, 0.03f, 0.9f), 40, false);
				Ring(C, PC, Wv.P, Wv.R, 9.f * Uw, FLinearColor(1.f, 0.12f, 0.08f, Pulse), 40, false);
				Ring(C, PC, Wv.P, Wv.R * (1.f - K) + 20.f, 6.f * Uw, FLinearColor(1.f, 0.85f, 0.2f, 0.95f), 32, true);
				FVector2D Sp;
				if (PC->ProjectWorldLocationToScreen(Wv.P + FVector(0.f, 0.f, 40.f), Sp, true))
				{
					ZoogiFunCanvasText(C, TEXT("!"), Sp.X, Sp.Y - 30.f * Uw, 64.f * Uw, FLinearColor(1.f, 0.15f, 0.1f), 1, 0);
					ZoogiFunCanvasText(C, TEXT("METEOR!"), Sp.X, Sp.Y + 26.f * Uw, 26.f * Uw, FLinearColor::White, 1, 0);
				}
			}
		}
		if (G && C && PC && GoldApples.Num() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldAppleLook")))
		{	// b142 (Evan: "put an icon above the golden apple"): the card art in a black P5 diamond, bobbing over each apple left on the court
			static UTexture2D* AppleIcon = nullptr; static bool bTriedIcon = false;
			if (!bTriedIcon)
			{
				bTriedIcon = true;
				const FString Pth = FPaths::ProjectContentDir() / TEXT("Data/CardArt/Icons/golden_apple.png");
				if (FPaths::FileExists(Pth)) AppleIcon = FImageUtils::ImportFileAsTexture2D(Pth);
				if (AppleIcon) AppleIcon->AddToRoot();
			}
			const float Ua = FMath::Min(C->ClipX, C->ClipY) / 1080.f;
			const float Tn = (float)FPlatformTime::Seconds();
			for (const FGoldApple& Ga : GoldApples)
			{
				if (!Ga.A.IsValid()) continue;
				FVector2D Sp, Sa;
				if (!PC->ProjectWorldLocationToScreen(Ga.P + FVector(0.f, 0.f, 150.f), Sp, true) || !PC->ProjectWorldLocationToScreen(Ga.P + FVector(0.f, 0.f, 40.f), Sa, true)) continue;
				Sp.Y = FMath::Min(Sp.Y, Sa.Y - 70.f * Ua) + 6.f * Ua * FMath::Sin(Tn * 3.2f);
				const float Sz = 74.f * Ua, Pulse = 0.85f + 0.15f * FMath::Sin(Tn * 5.f);
				ZoogiFunCanvasQuad(C, Sp + FVector2D(5.f, 5.f) * Ua, Sz * 1.25f, Sz * 1.25f, 45.f, 0.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
				ZoogiFunCanvasQuad(C, Sp, Sz * 1.3f, Sz * 1.3f, 45.f, 0.f, FLinearColor(1.f, 0.78f, 0.2f, Pulse));
				ZoogiFunCanvasQuad(C, Sp, Sz * 1.12f, Sz * 1.12f, 45.f, 0.f, FLinearColor(0.03f, 0.02f, 0.04f, 0.95f));
				if (AppleIcon)
				{
					FCanvasTileItem Ti(Sp - FVector2D(Sz, Sz * 0.833f) * 0.5f, AppleIcon->GetResource(), FVector2D(Sz, Sz * 0.833f), FLinearColor::White);
					Ti.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ti);
				}
				else ZoogiFunCanvasText(C, TEXT("G"), Sp.X, Sp.Y - 26.f * Ua, 48.f * Ua, FLinearColor(1.f, 0.8f, 0.25f), 1, 1);
				// pointer down to the apple
				ZoogiFunCanvasQuad(C, FVector2D(Sp.X, Sp.Y + Sz * 0.95f), 18.f * Ua, 18.f * Ua, 45.f, 0.f, FLinearColor(1.f, 0.78f, 0.2f, Pulse));
			}
		}
		if (!Picking() || !G || !C || !PC) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const float T = (float)(FPlatformTime::Seconds() - S.OpenedAt);
		const FLinearColor Acc(1.f, 0.16f, 0.32f), Wh(1.f, 1.f, 1.f, 0.95f), Ink(0.02f, 0.02f, 0.05f, 0.92f);
		// the cast range = the ARC ring around the Zoogi
		Ring(C, PC, Floor(S.Origin), S.Range, 9.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f), 96, true);
		Ring(C, PC, Floor(S.Origin), S.Range, 5.f * U, FLinearColor(0.35f, 0.9f, 1.f, 0.95f), 96, true);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(T * 7.f);
		switch (S.Aim)
		{
		case EAim::Area:
			if (S.bGrenade)
			{	// b130: the throw arc from the Zoogi to the circle
				const FVector A0 = S.Origin + FVector(0, 0, 60.f), A1 = Floor(S.Ctr);
				const float Hh = FMath::Max(300.f, FVector::Dist2D(A0, A1) * 0.35f);
				FVector2D Prev; bool bPrev = false;
				for (int32 k = 0; k <= 24; ++k)
				{
					const float Kk = k / 24.f; FVector P = FMath::Lerp(A0, A1, Kk); P.Z += 4.f * Hh * Kk * (1.f - Kk);
					FVector2D Sp;
					if (PC->ProjectWorldLocationToScreen(P, Sp, true)) { if (bPrev && (k % 2 == 1)) { FCanvasLineItem Li(Prev, Sp); Li.LineThickness = 6.f * U; Li.SetColor(FLinearColor(0.85f, 0.6f, 1.f, 0.95f)); C->DrawItem(Li); } Prev = Sp; bPrev = true; }
					else bPrev = false;
				}
			}
			Ring(C, PC, Floor(S.Ctr), S.R, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f), 48, false);
			Ring(C, PC, Floor(S.Ctr), S.R, 6.f * U, Acc, 48, false);
			Ring(C, PC, Floor(S.Ctr), S.R * (0.35f + 0.25f * Pulse), 4.f * U, FLinearColor(1.f, 0.85f, 0.3f, 0.9f), 32, false);
			break;
		case EAim::Line:
		{
			FVector2D A2, B2;
			if (PC->ProjectWorldLocationToScreen(Floor(S.A), A2, true) && PC->ProjectWorldLocationToScreen(Floor(S.B), B2, true))
			{
				FCanvasLineItem L0(A2, B2); L0.LineThickness = 22.f * U; L0.SetColor(FLinearColor(0.f, 0.f, 0.f, 0.6f)); C->DrawItem(L0);
				FCanvasLineItem L1(A2, B2); L1.LineThickness = 14.f * U; L1.SetColor(FLinearColor(0.55f, 0.9f, 1.f, 0.95f)); C->DrawItem(L1);
				const FVector2D Dv = (B2 - A2).GetSafeNormal();
				if (!Dv.IsNearlyZero())
				{	// b178j (Evan: "add arrow to tip of drawn line"): an arrowhead shows which way the swipe skill goes
					const FVector2D Pv(-Dv.Y, Dv.X), Tip = B2 + Dv * 30.f * U, Bs = B2 - Dv * 34.f * U;
					const FVector2D Lft = Bs + Pv * 34.f * U, Rgt = Bs - Pv * 34.f * U;
					for (int32 Pass = 0; Pass < 2; ++Pass)
					{
						const FLinearColor Ac = Pass == 0 ? FLinearColor(0.f, 0.f, 0.f, 0.6f) : FLinearColor(0.55f, 0.9f, 1.f, 0.95f);
						const float Th = (Pass == 0 ? 20.f : 12.f) * U;
						for (int32 k = 0; k <= 6; ++k) { FCanvasLineItem Fl(Tip, FMath::Lerp(Lft, Rgt, k / 6.f)); Fl.LineThickness = Th; Fl.SetColor(Ac); C->DrawItem(Fl); }
					}
				}
			}
			break;
		}
		case EAim::Tap: case EAim::MultiTap:
		{
			TArray<AActor*> Cand; Candidates(G, Cand);
			for (AActor* A : Cand)
			{
				const bool bSel = S.Targets.ContainsByPredicate([A](const TWeakObjectPtr<AActor>& X) { return X.Get() == A; });
				const float R = Cast<AZoogiBall>(A) ? 80.f : 45.f;
				Ring(C, PC, FVector(ActorPos(A).X, ActorPos(A).Y, S.FloorZ + 2.f), R * (bSel ? 1.f : 0.85f + 0.15f * Pulse), (bSel ? 8.f : 4.f) * U, bSel ? Acc : FLinearColor(1.f, 1.f, 1.f, 0.75f), 24, !bSel);
			}
			break;
		}
		case EAim::Spots:
			for (const FVector& P : S.Spots) { Ring(C, PC, Floor(P), 70.f, 8.f * U, Acc, 24, false); Ring(C, PC, Floor(P), 30.f, 6.f * U, FLinearColor(1.f, 0.85f, 0.3f), 16, false); }
			break;
		default: break;
		}
		// title plate (top) + hint, FIRE / CANCEL (bottom)
		const FDef* D = Find(S.Id);
		FString Title = S.bGrenade ? FString(TEXT("PRISM GRENADE")) : S.bWeapon ? ZoogiItems::WeaponName(S.WeaponId) : S.Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper(); // b130; b137
		const float TPx = 46.f * U * (Ht > Wd ? 1.3f : 1.f);
		const FVector2D TS = ZoogiFunCanvasMeasure(Title, TPx, 0);
		const FVector2D TC(Wd * 0.5f, Ht * (Ht > Wd ? 0.2f : 0.17f));
		ZoogiFunCanvasQuad(C, TC + FVector2D(10.f * U, 10.f * U), TS.X + 90.f * U, TPx * 1.4f, -4.f, 12.f * U, Acc);
		ZoogiFunCanvasQuad(C, TC, TS.X + 90.f * U, TPx * 1.4f, -4.f, 12.f * U, Ink);
		ZoogiFunCanvasText(C, Title, TC.X, TC.Y, TPx, Wh, 1, 0);
		FString Hint = S.bGrenade ? FString(TEXT("DRAG WHERE IT LANDS - 2X THE ARC RANGE")) : FString(D ? D->Hint : TEXT(""));
		if (S.Aim == EAim::MultiTap || S.Aim == EAim::Spots) Hint += FString::Printf(TEXT("   %d / %d"), S.Aim == EAim::Spots ? S.Spots.Num() : S.Targets.Num(), S.Count);
		static const bool bOldHint = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSmallSkillHint")); // b178g: the old small white strip
		const float HPx = (bOldHint ? 22.f : 0.f) * U * (Ht > Wd ? 1.3f : 1.f);
		if (bOldHint)
		{
			const FVector2D HS = ZoogiFunCanvasMeasure(Hint, HPx, 1) * 1.15f; // b131: the plate covers the whole hint
			ZoogiFunCanvasQuad(C, TC + FVector2D(0.f, TPx * 1.25f), HS.X + 60.f * U, HPx * 1.6f, -4.f, 8.f * U, Wh);
			ZoogiFunCanvasText(C, Hint, TC.X, TC.Y + TPx * 1.25f, HPx, FLinearColor(0.02f, 0.02f, 0.05f), 1, 1);
		}
		const float BW = (Ht > Wd ? 300.f : 260.f) * U, BH = (Ht > Wd ? 120.f : 96.f) * U, By = Ht - BH - (Ht > Wd ? 210.f : 120.f) * U;
		const bool bCan = CanFire();
		const FVector2D FC(Wd * 0.5f + BW * 0.6f, By + BH * 0.5f), CC(Wd * 0.5f - BW * 0.6f, By + BH * 0.5f);
		ZoogiFunCanvasQuad(C, FC + FVector2D(8.f * U, 8.f * U), BW, BH, -4.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		ZoogiFunCanvasQuad(C, FC, BW, BH, -4.f, 14.f * U, bCan ? Acc : FLinearColor(0.35f, 0.35f, 0.4f, 0.9f));
		ZoogiFunCanvasText(C, TEXT("FIRE!"), FC.X, FC.Y, 50.f * U * (Ht > Wd ? 1.2f : 1.f), Wh, 1, 0);
		ZoogiFunCanvasQuad(C, CC + FVector2D(8.f * U, 8.f * U), BW * 0.8f, BH * 0.8f, -4.f, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		ZoogiFunCanvasQuad(C, CC, BW * 0.8f, BH * 0.8f, -4.f, 12.f * U, Ink);
		ZoogiFunCanvasText(C, TEXT("CANCEL"), CC.X, CC.Y, 32.f * U * (Ht > Wd ? 1.2f : 1.f), Wh, 1, 0);
		S.FireBtn = FBox2D(FC - FVector2D(BW, BH) * 0.55f, FC + FVector2D(BW, BH) * 0.55f);
		S.CancelBtn = FBox2D(CC - FVector2D(BW * 0.8f, BH * 0.8f) * 0.55f, CC + FVector2D(BW * 0.8f, BH * 0.8f) * 0.55f);
		if (!bOldHint && !Hint.IsEmpty())
		{	// b178g (Evan picked option C): a big HOW TO USE card just above FIRE / CANCEL - a gesture icon + the hint at about 2x
			const bool bPort = Ht > Wd;
			float CPx = 44.f * U * (bPort ? 1.15f : 1.f);
			const float IconW = 150.f * U, MaxW = Wd * 0.94f;
			TArray<FString> Lines; Lines.Add(Hint);
			if (ZoogiFunCanvasMeasure(Hint, CPx, 1).X + IconW + 90.f * U > MaxW)
			{	// two lines, split at the space nearest the middle
				int32 Best = -1;
				for (int32 k = 0; k < Hint.Len(); ++k) if (Hint[k] == TCHAR(' ') && (Best < 0 || FMath::Abs(k - Hint.Len() / 2) < FMath::Abs(Best - Hint.Len() / 2))) Best = k;
				if (Best > 0) { Lines.Reset(); Lines.Add(Hint.Left(Best)); Lines.Add(Hint.Mid(Best + 1)); }
			}
			float TxW = 0.f; for (const FString& L : Lines) TxW = FMath::Max(TxW, ZoogiFunCanvasMeasure(L, CPx, 1).X);
			if (TxW + IconW + 90.f * U > MaxW) { CPx *= (MaxW - IconW - 90.f * U) / FMath::Max(1.f, TxW); TxW = MaxW - IconW - 90.f * U; }
			const float CW = TxW + IconW + 90.f * U, CH = FMath::Max(150.f * U, CPx * 1.35f * Lines.Num() + 70.f * U);
			const FVector2D Cc(Wd * 0.5f, By - CH * 0.5f - (bPort ? 60.f : 34.f) * U);
			const FLinearColor Yel(1.f, 0.84f, 0.16f);
			ZoogiFunCanvasQuad(C, Cc + FVector2D(10.f * U, 10.f * U), CW, CH, -3.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.75f));
			ZoogiFunCanvasQuad(C, Cc, CW, CH, -3.f, 14.f * U, FLinearColor(0.05f, 0.05f, 0.08f, 0.96f));
			const float TagPx = 30.f * U * (bPort ? 1.15f : 1.f);
			const FVector2D TagC(Cc.X - CW * 0.5f + 40.f * U + 95.f * U, Cc.Y - CH * 0.5f);
			ZoogiFunCanvasQuad(C, TagC, ZoogiFunCanvasMeasure(TEXT("HOW TO USE"), TagPx, 0).X + 40.f * U, TagPx * 1.45f, -4.f, 6.f * U, Yel);
			ZoogiFunCanvasText(C, TEXT("HOW TO USE"), TagC.X, TagC.Y, TagPx, FLinearColor(0.02f, 0.02f, 0.05f), 1, 0);
			// the gesture icon: swipe = an arrow, tap = a target dot, spots = three dots, area / self = a burst ring
			const FVector2D Ic(Cc.X - CW * 0.5f + 40.f * U + IconW * 0.5f, Cc.Y + 6.f * U);
			const float IR = 46.f * U;
			auto Seg = [&](const FVector2D& A2, const FVector2D& B2, float Th) { FCanvasLineItem Li(A2, B2); Li.SetColor(Yel); Li.LineThickness = Th; C->DrawItem(Li); };
			auto Circle = [&](const FVector2D& O, float R2, float Th) { for (int32 k = 0; k < 28; ++k) { const float A0 = 2.f * PI * k / 28.f, A1 = 2.f * PI * (k + 1) / 28.f; Seg(O + R2 * FVector2D(FMath::Cos(A0), FMath::Sin(A0)), O + R2 * FVector2D(FMath::Cos(A1), FMath::Sin(A1)), Th); } };
			auto Dot = [&](const FVector2D& O, float R2) { FCanvasTileItem T(O - FVector2D(R2, R2), FVector2D(R2 * 2.f, R2 * 2.f), Yel); T.BlendMode = SE_BLEND_Translucent; C->DrawItem(T); };
			Circle(Ic, IR, 5.f * U);
			if (S.bGrenade || S.Aim == EAim::Line) { Seg(Ic - FVector2D(IR * 0.62f, 0.f), Ic + FVector2D(IR * 0.5f, 0.f), 8.f * U); Seg(Ic + FVector2D(IR * 0.62f, 0.f), Ic + FVector2D(IR * 0.2f, -IR * 0.36f), 8.f * U); Seg(Ic + FVector2D(IR * 0.62f, 0.f), Ic + FVector2D(IR * 0.2f, IR * 0.36f), 8.f * U); }
			else if (S.Aim == EAim::Tap || S.Aim == EAim::MultiTap) { Circle(Ic, IR * 0.5f, 4.f * U); Dot(Ic, IR * 0.18f); }
			else if (S.Aim == EAim::Spots) { Dot(Ic + FVector2D(-IR * 0.42f, IR * 0.2f), IR * 0.16f); Dot(Ic + FVector2D(0.f, -IR * 0.3f), IR * 0.16f); Dot(Ic + FVector2D(IR * 0.42f, IR * 0.2f), IR * 0.16f); }
			else { Circle(Ic, IR * 0.55f, 4.f * U); for (int32 k = 0; k < 8; ++k) { const float A0 = 2.f * PI * k / 8.f; const FVector2D Dv(FMath::Cos(A0), FMath::Sin(A0)); Seg(Ic + Dv * IR * 0.68f, Ic + Dv * IR * 0.9f, 4.f * U); } }
			const float Tx = Cc.X - CW * 0.5f + 40.f * U + IconW + 20.f * U + TxW * 0.5f;
			for (int32 k = 0; k < Lines.Num(); ++k)
				ZoogiFunCanvasText(C, Lines[k], Tx, Cc.Y + 6.f * U + (k - (Lines.Num() - 1) * 0.5f) * CPx * 1.35f, CPx, Wh, 1, 1);
		}
		if (S.bCpu)
		{	// b67: whose pick it is, the CPU's cursor, and FIRE lighting up just before it "presses" it
			S.FireBtn = S.CancelBtn = FBox2D(ForceInit);
			if (const AZoogiBall* Cb = S.Caster.Get())
				ZoogiFunCanvasText(C, FString::Printf(TEXT("P%d %s IS TARGETING..."), Cb->PlayerIndex + 1, *Cb->Def.Name.ToUpper()), TC.X, TC.Y + TPx * 1.25f + (HPx > 0.f ? HPx * 1.6f : 0.f), 28.f * U, ZoogiUtil::PlayerColor(Cb->PlayerIndex), 1, 0); // b131: under the hint (was over the turn header)
			FVector2D Cur;
			const FVector CurW = S.Aim == EAim::Line ? S.B : (S.Aim == EAim::Tap || S.Aim == EAim::MultiTap) && S.Targets.Num() && S.Targets.Last().IsValid() ? ActorPos(S.Targets.Last().Get()) : (S.Aim == EAim::Spots && S.Spots.Num() ? S.Spots.Last() : S.Ctr);
			if (PC->ProjectWorldLocationToScreen(Floor(CurW), Cur, true))
			{
				FCanvasTileItem Dot(Cur - FVector2D(22.f, 22.f) * U, FVector2D(44.f, 44.f) * U, FLinearColor(1.f, 0.85f, 0.25f, 0.85f)); Dot.BlendMode = SE_BLEND_Translucent; C->DrawItem(Dot);
				FCanvasTileItem In(Cur - FVector2D(8.f, 8.f) * U, FVector2D(16.f, 16.f) * U, FLinearColor::White); In.BlendMode = SE_BLEND_Translucent; C->DrawItem(In);
			}
			const double Left = S.CpuFireAt - FPlatformTime::Seconds();
			if (Left < 0.35) { ZoogiFunCanvasQuad(C, FC, BW * 1.15f, BH * 1.15f, -4.f, 14.f * U, FLinearColor(1.f, 0.9f, 0.4f, 0.55f)); ZoogiFunCanvasText(C, TEXT("FIRE!"), FC.X, FC.Y, 56.f * U, Wh, 1, 0); }
		}
	}

	// ------------------------------------------------------------------ casting
	// b176a: the Fab-pack skills - one Niagara effect per hit spot (or along the swipe), then a knock: 0 area, 1 tap, 2 spots,
	// 3 line (side push), 4 line (forward sweep)
	// b176g: nothing in the effect's reach - the nearest rival (first) or orb within MaxR is still knocked (away from At, or along Push)
	static int32 Assist(AZoogiGameMode* Gm, const FVector& At, const FVector& Push, float MaxR, float Speed, float Up, const AZoogiBall* Spare, int32 Pi)
	{
		UPrimitiveComponent* Best = nullptr; FVector BestL = FVector::ZeroVector; float BestD = MaxR; AZoogiBall* BestB = nullptr; AZoogiOrb* BestO = nullptr;
		for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz != Spare && Bz->Sphere) { const float D = FVector::Dist2D(Bz->BallLocation(), At) * 0.7f; if (D < BestD) { BestD = D; Best = Bz->Sphere; BestL = Bz->BallLocation(); BestB = Bz; BestO = nullptr; } } // rivals count closer
		for (AZoogiOrb* O : Gm->Orbs) if (O && !O->bOut && O->Sphere) { const float D = FVector::Dist2D(O->OrbLocation(), At); if (D < BestD) { BestD = D; Best = O->Sphere; BestL = O->OrbLocation(); BestO = O; BestB = nullptr; } }
		if (!Best) return 0;
		FVector Dir = Push.IsNearlyZero() ? (BestL - At).GetSafeNormal2D() : Push.GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
		Kick(Best, Dir * Speed * 0.85f + FVector(0, 0, Up));
		if (BestB) BestB->LastHitPlayer = Pi; if (BestO) BestO->LastHitBy = Pi;
		AZoogiFX::Spawn(Gm->GetWorld(), BestL, FLinearColor(1.f, 0.9f, 0.5f), 140.f, 0.35f, true, 10.f);
		return 1;
	}

	// b178a: the swipe skills that are attack lines (cast from the caster through the target, not a backstop wall)
	bool IsPackLine(FName Id)
	{
		static const TSet<FName> L = { FName(TEXT("ground_slash")), FName(TEXT("rock_rampart")), FName(TEXT("stone_barricade")), FName(TEXT("slab_row")), FName(TEXT("jade_wall")), FName(TEXT("obsidian_wall")), FName(TEXT("gold_wall")), FName(TEXT("rune_wall")), FName(TEXT("landslide")), FName(TEXT("sapphire_wall")), FName(TEXT("sandstone_ruins")), FName(TEXT("fault_line")), FName(TEXT("rift_crack")), FName(TEXT("long_rampart")) };
		return L.Contains(Id);
	}
	// b178f (Evan: "add appropriate sound effects to earth bending skills"): every pack skill gets its own sound - a ground rumble +
	// crack as it is cast, then the rock burst on the hit (crystal rock adds a glassy shatter, gold a metal ring, ice a frost sting,
	// slashes a blade whoosh). Phase 0 = cast, 1 = the hit lands. -ZoogiPackSfxOld = the old single boom.
	static void PackSfx(AZoogiGameMode* G, const TCHAR* Id, const TCHAR* Fx, int32 Kind, int32 Phase)
	{
		if (!G) return;
		static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPackSfxOld"));
		const FString Sid(Id);
		const bool bEarth = FCString::Strstr(Fx, TEXT("Earthbender")) != nullptr, bIce = FCString::Strstr(Fx, TEXT("IceAttack")) != nullptr, bSlash = FCString::Strstr(Fx, TEXT("Slash")) != nullptr;
		if (bOld) { if (Phase == 1) ZoogiUtil::PlaySfx(G, Kind >= 3 ? TEXT("Whoosh") : TEXT("CritBoom"), Kind >= 3 ? 1.f : 0.8f, Kind >= 3 ? 0.9f : 1.f); return; }
		const bool bCrystal = Sid.Contains(TEXT("obsidian")) || Sid.Contains(TEXT("sapphire")) || Sid.Contains(TEXT("rune")) || Sid.Contains(TEXT("jade")) || Sid.Contains(TEXT("frostrock")) || Sid.Contains(TEXT("snowcap")) || Sid.Contains(TEXT("star"));
		const bool bGold = Sid.Contains(TEXT("gold"));
		const bool bSand = Sid.Contains(TEXT("sand")) || Sid.Contains(TEXT("desert"));
		const float Pt = bSand ? 1.15f : (Kind == 0 ? 0.85f : 1.f); // big area bursts sit lower, sand is drier / higher
		if (Phase == 0)
		{
			if (bEarth) { ZoogiUtil::PlaySfx(G, TEXT("Rumble"), Kind == 0 ? 0.9f : 0.7f, Pt); ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.7f, Pt * 1.1f); }
			else if (bIce) { ZoogiUtil::PlaySfx(G, TEXT("StingFrost"), 0.8f, 1.1f); ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 0.5f, 1.4f); }
			else if (bSlash) ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.9f, 1.f);
			return;
		}
		if (bEarth)
		{
			ZoogiUtil::PlaySfx(G, TEXT("RockSmash"), 1.f, Pt);
			if (Kind == 0) ZoogiUtil::PlaySfx(G, TEXT("EpicImpact"), 0.8f, 1.f);
			else if (Kind >= 3) ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.6f, 0.7f); // the wall shoves
			else ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.45f, 1.f);
			if (bCrystal) ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.7f, 1.15f);
			if (bGold) ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.55f, 1.35f);
			TWeakObjectPtr<AZoogiGameMode> WG = G;
			Later(G, 0.35f, [WG, Pt]() { if (AZoogiGameMode* Gm = WG.Get()) ZoogiUtil::PlaySfx(Gm, TEXT("Debris"), 0.65f, Pt); }); // rubble rains back down
		}
		else if (bIce) { ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.9f, 1.25f); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.55f, 1.1f); }
		else if (bSlash) { ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 1.f, 0.9f); ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.7f, 1.f); }
		else ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.f);
	}

	static bool ExecPack(AZoogiGameMode* G, AZoogiBall* Caster, FName Id, const FPick& P, float FZ)
	{
		struct FPk { const TCHAR* Id; const TCHAR* Fx; float Scale; int32 Kind; float R; float Speed; float Up; float Delay; FLinearColor Col; };
		static const FPk Pks[] = {
			{ TEXT("frost_spikes"), TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV06.NS_IceAttackV06"), 0.8f, 0, 360.f, 1100.f, 650.f, 0.25f, FLinearColor(0.5f, 0.85f, 1.f) },
			{ TEXT("glacier_lance"), TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV08.NS_IceAttackV08"), 0.55f, 1, 190.f, 1500.f, 900.f, 0.3f, FLinearColor(0.45f, 0.8f, 1.f) },
			{ TEXT("ice_shards"), TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV02.NS_IceAttackV02"), 0.25f, 2, 230.f, 700.f, 420.f, 0.2f, FLinearColor(0.6f, 0.9f, 1.f) },
			{ TEXT("stone_pillar"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V01.NS_Earthbender_V01"), 1.4f, 1, 190.f, 1400.f, 850.f, 0.25f, FLinearColor(0.55f, 0.6f, 0.8f) },
			{ TEXT("earthquake"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V41.NS_Earthbender_V41"), 1.6f, 0, 560.f, 900.f, 380.f, 0.35f, FLinearColor(0.8f, 0.7f, 0.55f) },
			{ TEXT("rock_rampart"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V30.NS_Earthbender_V30"), 0.9f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.55f, 0.6f, 0.8f) },
			{ TEXT("crescent_slash"), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), 2.0f, 1, 170.f, 1100.f, 260.f, 0.1f, FLinearColor(0.3f, 0.6f, 1.f) }, // b179f: a target skill (up to 3)
			{ TEXT("cross_slash"), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_CrossSlash08.NS_CrossSlash08"), 1.8f, 1, 210.f, 1200.f, 300.f, 0.1f, FLinearColor(1.f, 0.55f, 0.15f) },
			{ TEXT("ground_slash"), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_GroundSlash03.NS_GroundSlash03"), 2.0f, 4, 170.f, 1100.f, 600.f, 0.15f, FLinearColor(0.65f, 0.35f, 1.f) },
			// b178a: the rest of the Earthbender pack
			{ TEXT("boulder_block"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V02.NS_Earthbender_V02"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("sandstone_spire"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V03.NS_Earthbender_V03"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.95f, 0.78f, 0.50f) },
			{ TEXT("stone_barricade"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V04.NS_Earthbender_V04"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("desert_rubble"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V05.NS_Earthbender_V05"), 1.00f, 2, 220.f, 760.f, 450.f, 0.25f, FLinearColor(0.95f, 0.78f, 0.50f) },
			{ TEXT("jade_boulder"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V06.NS_Earthbender_V06"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.40f, 0.90f, 0.75f) },
			{ TEXT("rock_burst"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V07.NS_Earthbender_V07"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("boulder_fountain"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V08.NS_Earthbender_V08"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("slab_row"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V09.NS_Earthbender_V09"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("jade_wall"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V10.NS_Earthbender_V10"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.40f, 0.90f, 0.75f) },
			{ TEXT("snowcap_pillar"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V11.NS_Earthbender_V11"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.85f, 0.95f, 1.00f) },
			{ TEXT("granite_block"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V12.NS_Earthbender_V12"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.65f, 0.70f, 0.55f) },
			{ TEXT("moss_tower"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V13.NS_Earthbender_V13"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.65f, 0.70f, 0.55f) },
			{ TEXT("frostrock_grove"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V14.NS_Earthbender_V14"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.85f, 0.95f, 1.00f) },
			{ TEXT("obsidian_spike"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V15.NS_Earthbender_V15"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.55f, 0.40f, 0.75f) },
			{ TEXT("obsidian_wall"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V16.NS_Earthbender_V16"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.55f, 0.40f, 0.75f) },
			{ TEXT("obsidian_cluster"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V17.NS_Earthbender_V17"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.40f, 0.75f) },
			{ TEXT("obsidian_burst"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V18.NS_Earthbender_V18"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.40f, 0.75f) },
			{ TEXT("sapphire_pillar"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V19.NS_Earthbender_V19"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.25f, 0.45f, 1.00f) },
			{ TEXT("sapphire_burst"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V20.NS_Earthbender_V20"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.25f, 0.45f, 1.00f) },
			{ TEXT("sapphire_block"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V21.NS_Earthbender_V21"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.25f, 0.45f, 1.00f) },
			{ TEXT("gold_rush"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V22.NS_Earthbender_V22"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(1.00f, 0.80f, 0.25f) },
			{ TEXT("gold_nugget"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V23.NS_Earthbender_V23"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(1.00f, 0.80f, 0.25f) },
			{ TEXT("gold_wall"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V24.NS_Earthbender_V24"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(1.00f, 0.80f, 0.25f) },
			{ TEXT("rune_pillar"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V25.NS_Earthbender_V25"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.30f, 0.75f, 1.00f) },
			{ TEXT("rune_wall"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V26.NS_Earthbender_V26"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.30f, 0.75f, 1.00f) },
			{ TEXT("rune_monolith"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V27.NS_Earthbender_V27"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.30f, 0.75f, 1.00f) },
			{ TEXT("stone_teeth"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V28.NS_Earthbender_V28"), 1.00f, 2, 220.f, 760.f, 450.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("toppled_columns"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V29.NS_Earthbender_V29"), 1.00f, 2, 220.f, 760.f, 450.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("landslide"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V31.NS_Earthbender_V31"), 0.90f, 4, 170.f, 1000.f, 450.f, 0.15f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("starburst_spikes"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V32.NS_Earthbender_V32"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("rock_geyser"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V33.NS_Earthbender_V33"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("crater_ring"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V34.NS_Earthbender_V34"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("gold_column"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V35.NS_Earthbender_V35"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(1.00f, 0.80f, 0.25f) },
			{ TEXT("jade_columns"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V36.NS_Earthbender_V36"), 1.00f, 2, 220.f, 760.f, 450.f, 0.25f, FLinearColor(0.40f, 0.90f, 0.75f) },
			{ TEXT("sapphire_wall"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V37.NS_Earthbender_V37"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.25f, 0.45f, 1.00f) },
			{ TEXT("sandstone_ruins"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V38.NS_Earthbender_V38"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.95f, 0.78f, 0.50f) },
			{ TEXT("obsidian_star"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V39.NS_Earthbender_V39"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.40f, 0.75f) },
			{ TEXT("earth_flash"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V40.NS_Earthbender_V40"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(1.00f, 1.00f, 1.00f) },
			{ TEXT("rubble_storm"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V42.NS_Earthbender_V42"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("stone_cage"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V43.NS_Earthbender_V43"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("pillar_field"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V44.NS_Earthbender_V44"), 1.00f, 2, 220.f, 760.f, 450.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("fault_line"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V45.NS_Earthbender_V45"), 0.90f, 4, 170.f, 1000.f, 450.f, 0.15f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("shatter_star"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V46.NS_Earthbender_V46"), 1.30f, 0, 380.f, 1000.f, 520.f, 0.30f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("slab_slam"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V47.NS_Earthbender_V47"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("rift_crack"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V48.NS_Earthbender_V48"), 0.90f, 4, 170.f, 1000.f, 450.f, 0.15f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("long_rampart"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V49.NS_Earthbender_V49"), 0.90f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) },
			{ TEXT("boulder_pile"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V50.NS_Earthbender_V50"), 1.20f, 1, 190.f, 1300.f, 800.f, 0.25f, FLinearColor(0.55f, 0.60f, 0.80f) } };
		const FPk* D = nullptr;
		for (const FPk& K : Pks) if (Id == FName(K.Id)) D = &K;
		if (!D || !G || !Caster) return false;
		UWorld* W = G->GetWorld();
		const int32 Pi = Caster->PlayerIndex;
		PackSfx(G, D->Id, D->Fx, D->Kind, 0); // b178f
		auto At = [FZ](const FVector& V) { return FVector(V.X, V.Y, FZ); };
		const float Scl = Sc(Id, D->Scale) * ZoogiSkillLab::FxSize(); // b176i: lab FX SIZE
		TWeakObjectPtr<AZoogiGameMode> WG = G; TWeakObjectPtr<AZoogiBall> WC = Caster;
		FPk Dd = *D;
		{	// b176i: lab COLOUR / RADIUS / PULL (lift)
			FLinearColor Lc; if (ZoogiSkillLab::Tint(Lc)) Dd.Col = Lc;
			Dd.R *= ZoogiSkillLab::Radius(); // b177o: lift is the HEIGHT row now (in Kick)
		}
		const TCHAR* LabBx = ZoogiSkillLab::ChosenBlast(); const float LabBs = ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize(); // b176i: lab BLAST (DEFAULT = none)
		auto Credit = [Pi](AZoogiGameMode* Gm, const FVector& Ctr, float Rr)
		{
			for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz->PlayerIndex != Pi && FVector::Dist2D(Bz->BallLocation(), Ctr) <= Rr * ZoogiSkillLab::Radius()) Bz->LastHitPlayer = Pi;
		};
		if (Dd.Kind <= 2)
		{
			TArray<FVector> Hits;
			if (Dd.Kind == 0) Hits.Add(At(P.Ctr));
			else if (Dd.Kind == 1) { for (const TWeakObjectPtr<AActor>& T : P.Targets) if (AActor* A = T.Get()) Hits.Add(At(A->GetActorLocation())); if (Hits.Num() == 0) Hits.Add(At(P.Ctr)); }
			else { for (const FVector& V : P.Spots) Hits.Add(At(V)); if (Hits.Num() == 0) Hits.Add(At(P.Ctr)); }
			// b178k (Evan: Stone Pillar "selects up to three targets, each gets a pillar, that start 0.2 after each other"): a tap skill with
			// several targets bursts them one by one, 0.2 s apart
			auto OneHit = [WG, WC, Dd, Scl, LabBx, LabBs, Credit, Pi, FZ](const FVector& H)
			{
				AZoogiGameMode* Gh = WG.Get(); UWorld* Wh = Gh ? Gh->GetWorld() : nullptr; if (!Wh) return;
				const FVector Cl = WC.IsValid() ? WC->BallLocation() : H - FVector(100.f, 0.f, 0.f);
				const bool bAimedSlash = FCString::Strcmp(Dd.Id, TEXT("crescent_slash")) == 0;
				const FVector From(Cl.X, Cl.Y, H.Z);
				const FRotator SlashRot = (H - From).GetSafeNormal2D().IsNearlyZero() ? FRotator::ZeroRotator : (H - From).GetSafeNormal2D().Rotation();
				if (bAimedSlash) AZoogiFabFX::Play(Wh, Dd.Fx, FMath::Lerp(From, H, 0.5f), Scl, 8.f, nullptr, FVector::ZeroVector, SlashRot); // b179f: the slash flies from you to the target
				AZoogiFabFX* Pf = bAimedSlash ? AZoogiFabFX::Play(Wh, Dd.Fx, H, Scl, 8.f, nullptr, FVector::ZeroVector, SlashRot) : AZoogiFabFX::Play(Wh, Dd.Fx, H, Scl, 8.f); // b177m: was cut at 3 s mid-effect (Evan: "too choppy, not like the demo")
				if (Pf && Pf->NC && FCString::Strstr(Dd.Fx, TEXT("IceAttack")))
				{	// b177p (Evan: "remove the snow on the ground for this effect, I can't see anything"): no smoke / ground snow, spikes + cracks stay
					static const TCHAR* Snow[] = { TEXT("NE_Smoke"), TEXT("NE_Smoke01"), TEXT("NE_SmokeBurst"), TEXT("NE_GroundIce"), TEXT("NE_GroundIce01"), TEXT("NE_GroundIce02"), TEXT("NE_GroundIce02NEW"), TEXT("NE_GroundIce03"), TEXT("NE_GroundIce03NEW"),
						TEXT("NE_GroundParticle"), TEXT("NE_GroundParticle01"), TEXT("NE_GroundParticle02"), TEXT("NE_GroundParticle02NEW"), TEXT("NE_GroundParticle03"), TEXT("NE_GroundParticle03NEW"), TEXT("NE_FloatingParticles"), TEXT("NE_FloatingParticles01") };
					static const bool bKeep = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceSnow"));
					if (!bKeep) for (const TCHAR* E : Snow) Pf->NC->SetEmitterEnable(FName(E), false);
				}
				if (LabBx && *LabBx) AZoogiFX::Stylized(Wh, LabBx, H, 0.6f * LabBs);
				AZoogiFX::Spawn(Wh, H + FVector(0, 0, 6.f), Dd.Col, Dd.R, 0.45f, true, 9.f);
				// b213 (Evan: "I want them to be solid while they're there"): an Earthbender pillar / rock stands solid while it shows
				if ((Dd.Kind == 1 || Dd.Kind == 2) && FCString::Strstr(Dd.Fx, TEXT("Earthbender"))) ZoogiEarthEvents::AddSolid(Gh, H, 0.f, Dd.Kind == 1 ? 95.f : 80.f, 0.f, 230.f, 7.f, Pf);
				// b176g: a tap skill's blast sits just behind the target (caster side) so the target flies AWAY from the caster
				const FVector Hb = Dd.Kind == 1 ? H - (H - FVector(Cl.X, Cl.Y, FZ)).GetSafeNormal2D() * Dd.R * 0.55f : H;
				Later(Gh, Dd.Delay, [WG, WC, H, Hb, Dd, Credit, Pi]()
				{
					AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
					const float R0 = Dd.R / FMath::Max(0.05f, ZoogiSkillLab::RadiusOf(FName(Dd.Id))); // Blast / Credit apply the lab RADIUS themselves
					Credit(Gm, Hb, R0);
					int32 N = Blast(Gm, Hb, R0, Dd.Speed, Dd.Up, WC.Get(), Pi);
					if (N == 0) N += Assist(Gm, H, FVector::ZeroVector, Dd.R * 2.5f, Dd.Speed, Dd.Up, WC.Get(), Pi); // b176g (Evan: "every effect is meant to hit")
					UZoogiScreenFx::Shockwave(Gm->GetWorld(), H, Dd.R + 60.f, Dd.Col);
					PackSfx(Gm, Dd.Id, Dd.Fx, Dd.Kind, 1); // b178f (was one CritBoom)
					ZoogiHaptics::Kick(0.6f, Gm->GetWorld(), TEXT("pack_skill"));
					UE_LOG(LogZoogi, Log, TEXT("SKILL %s P%d at (%.0f, %.0f): %d pushed"), Dd.Id, Pi + 1, H.X, H.Y, N);
				});
			};
			for (int32 Hk = 0; Hk < Hits.Num(); ++Hk)
			{
				const FVector H = Hits[Hk];
				if (Hk == 0 || Dd.Kind != 1) { OneHit(H); continue; }
				TWeakObjectPtr<AActor> Tg = P.Targets.IsValidIndex(Hk) ? P.Targets[Hk] : TWeakObjectPtr<AActor>();
				LaterReal(0.2f * Hk, [OneHit, H, Tg, FZ]() { const AActor* Ta = Tg.Get(); OneHit(Ta ? FVector(Ta->GetActorLocation().X, Ta->GetActorLocation().Y, FZ) : H); }); // where the target is NOW
			}
		}
		else
		{	// a swipe: the effect plays along the line, then everything within R of it is thrown (to the side, or forward)
			const FVector A = At(P.A), B = At(P.B);
			FVector Dir = (B - A).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			const float Len = FMath::Max(100.f, FVector::Dist2D(A, B));
			const FRotator Rot = Dir.Rotation();
			const int32 NFx = FMath::Clamp(FMath::RoundToInt(Len / 260.f), 1, 4);
			// b178j (Evan: "align the effect with that direction, currently it unfolds perpendicularly"): the Earthbender walls are modelled
			// across their X, so they turn 90 deg to run ALONG the swipe, and burst up one after another from the start to the arrow tip.
			// -ZoogiWallYaw=<deg> overrides the turn (test), -ZoogiWallAllAtOnce = the old all-at-once burst.
			static float WallYaw = -999.f; if (WallYaw < -998.f) { WallYaw = 90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWallYaw="), WallYaw); }
			static const bool bAtOnce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallAllAtOnce"));
			const bool bEarthLine = FCString::Strstr(Dd.Fx, TEXT("Earthbender")) != nullptr;
			const FRotator FxRot = bEarthLine ? Rot + FRotator(0.f, WallYaw, 0.f) : Rot;
			TWeakObjectPtr<AZoogiGameMode> WGf = G;
			for (int32 k = 0; k < NFx; ++k)
			{
				const FVector Pk = A + Dir * (Len * (k + 0.5f) / NFx);
				const float Sx = Scl; const FString FxPath(Dd.Fx); const TCHAR* Bx = LabBx; const float Bs = LabBs;
				const bool bSolidWall = bEarthLine && Dd.Kind == 3 && k == 0; // b213: the wall stands solid while it shows
				const FVector WallMid = A + Dir * Len * 0.5f; const float WallLen = Len; const float WallYawDeg = Rot.Yaw;
				auto SpawnWall = [WGf, Pk, Sx, FxPath, FxRot, Bx, Bs, bSolidWall, WallMid, WallLen, WallYawDeg]()
				{
					AZoogiGameMode* Gm = WGf.Get(); UWorld* Wl = Gm ? Gm->GetWorld() : nullptr; if (!Wl) return;
					AZoogiFabFX* WFx = AZoogiFabFX::Play(Wl, *FxPath, Pk, Sx, 8.f, nullptr, FVector::ZeroVector, FxRot);
					if (Bx && *Bx) AZoogiFX::Stylized(Wl, Bx, Pk, 0.5f * Bs);
					if (bSolidWall) ZoogiEarthEvents::AddSolid(Gm, WallMid, WallYawDeg, FMath::Max(WallLen * 0.5f + 40.f, 160.f), 55.f, 200.f, 7.f, WFx);
				};
				static const bool bStagger = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWallStagger")); // b179c (Evan: "I didn't ask for the effect to change"): all at once again, like before
				if (k == 0 || bAtOnce || !bEarthLine || !bStagger) SpawnWall(); else LaterReal(0.11f * k, SpawnWall);
			}
			Later(G, Dd.Delay, [WG, WC, A, Dir, Len, Dd, Pi]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				int32 N = 0;
				const FVector Side(-Dir.Y, Dir.X, 0.f);
				auto Hit = [&](UPrimitiveComponent* Body, const FVector& L) -> bool
				{
					if (!Body) return false;
					const FVector Rel = L - A; const float Along = FVector::DotProduct(Rel, Dir), Across = FVector::DotProduct(Rel, Side);
					if (Along < -60.f || Along > Len + 60.f || FMath::Abs(Across) > Dd.R) return false;
					const FVector Push = Dd.Kind == 3 ? Side * (Across >= 0.f ? 1.f : -1.f) : Dir;
					Kick(Body, Push * Dd.Speed * (1.f - 0.4f * FMath::Abs(Across) / Dd.R) + FVector(0, 0, Dd.Up)); ++N; return true;
				};
				for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz != WC.Get() && Hit(Bz->Sphere, Bz->BallLocation())) Bz->LastHitPlayer = Pi;
				for (AZoogiOrb* O : Gm->Orbs) if (O && !O->bOut && O->Sphere && Hit(O->Sphere, O->OrbLocation())) O->LastHitBy = Pi;
				if (N == 0) N += Assist(Gm, A + Dir * Len * 0.5f, Dd.Kind == 4 ? Dir : FVector::ZeroVector, Len * 0.5f + Dd.R * 2.5f, Dd.Speed, Dd.Up, WC.Get(), Pi); // b176g
				UZoogiScreenFx::Shockwave(Gm->GetWorld(), A + Dir * Len * 0.5f, Len * 0.5f + 80.f, Dd.Col);
				PackSfx(Gm, Dd.Id, Dd.Fx, Dd.Kind, 1); // b178f (was one Whoosh)
				ZoogiHaptics::Kick(0.6f, Gm->GetWorld(), TEXT("pack_skill"));
				UE_LOG(LogZoogi, Log, TEXT("SKILL %s P%d line %.0f cm: %d pushed"), Dd.Id, Pi + 1, Len, N);
			});
		}
		if (!G->ShotDir.IsEmpty()) { const FString Nn = FString::Printf(TEXT("skill_%s"), *Id.ToString()); G->TakeShotDelayed(Nn + TEXT("_a"), 0.45f); G->TakeShotDelayed(Nn + TEXT("_b"), 1.1f); }
		return true;
	}

	bool BoltFireAt(AZoogiGameMode* G, AZoogiBall* B, const FVector& From, AActor* Prefer, const FVector* AimPt = nullptr); // b195: below (Bolt Balls); b200: AimPt = a tapped circle
	// ================================================================ b190 GATTLING skills (Evan picked: 6-shot burst, beat burst, turret)
	namespace
	{
		struct FGatRun { TWeakObjectPtr<AZoogiBall> B; int32 Kind = 0; int32 Pi = -1; double T0 = 0.0, NextAt = 0.0; int32 Fired = 0, Total = 0; float Gap = 0.17f; float Pow = 1.f; double BeatLen = 0.0; bool bPrevDown = true; double LastTapAt = -10.0; int32 Perfect = 0; bool bCpu = false; };
		TArray<FGatRun> GatRuns;
		struct FRapidMark { FVector P = FVector::ZeroVector; double T = 0.0, PopAt = 0.0; };
		TArray<FRapidMark> RapidMarkList; // b200: the tapped circles
		// b200: the floor point under a screen tap (false = not on the court)
		bool RapidTapPoint(AZoogiGameMode* G, float FloorZ, FVector& Out)
		{
			APlayerController* Pc = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr; if (!Pc) return false;
			float X = 0.f, Y = 0.f; bool bT = false; Pc->GetInputTouchState(ETouchIndex::Touch1, X, Y, bT);
			if (!bT && !Pc->GetMousePosition(X, Y)) return false;
			FVector O, D; if (!Pc->DeprojectScreenPositionToWorld(X, Y, O, D) || D.Z > -0.02f) return false;
			const float Tt = (FloorZ - O.Z) / D.Z; if (Tt <= 0.f) return false;
			Out = O + D * Tt;
			const float KR = ZoogiConst::KnockoffRadius * (G->Arena ? G->Arena->GetActorScale3D().X : 1.f);
			return FVector::Dist2D(Out, G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector) < KR + 100.f;
		}
		struct FTurret { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector; int32 Owner = -1; int32 EndTurn = 0; double NextShot = 0.0; int32 Shots = 0; int32 ShotsLeft = 10; /* b223 */ int32 Turn = -1; float Pow = 1.f; FVector Home = FVector::ZeroVector; double KickAt = -10.0; FVector KickDir = FVector::ZeroVector; float TipFwd = 60.f, TipUp = 110.f; };
		TArray<FTurret> Turrets;
		struct FTurSlash { TWeakObjectPtr<AActor> Turret; TWeakObjectPtr<AActor> Carrier; FVector From = FVector::ZeroVector, To = FVector::ZeroVector; float T = 0.f, Dur = 0.3f; int32 Owner = -1; float Pow = 1.f; TWeakObjectPtr<AActor> Target;
			int32 Style = 1; float Push = 1.f; bool bShown = false; double NextTrail = 0.0; bool bBoss = false; /* b223 */ }; // b221: shot style, T < 0 = waiting (tracer bursts)
		// b221 (Evan Nov 10: turret shots went out sideways and curved after the target - "only straight", "confirm bullets shoot from the
		// barrel"): every shot leaves the barrel tip and flies straight along the barrel. -ZoogiTurretShot=1..6 picks the shot:
		// 1 CRESCENT SLASH (b202), 2 TRACER BURST, 3 PLASMA BOLT, 4 LASER BEAM, 5 ROCKET, 6 SHOCK RING. -ZoogiTurretYaw= = the model's
		// facing offset (its barrel is local +Y -> -90).
		int32 TurretStyleNow() { static int32 TsStyle = -1; if (TsStyle < 0) { TsStyle = 5; /* b222 (Evan: "turret = rocket") */ FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretShot="), TsStyle); TsStyle = FMath::Clamp(TsStyle, 1, 6); } return TsStyle; }
		// b223 (Evan: "in inventory for turret only create a separate edit for selecting ammo type ... all options except slash"): the human's
		// turret fires the AMMO picked in the inventory (profile key turret_ammo = 2..6), CPUs fire rockets; -ZoogiTurretShot= forces one
		int32 TurretAmmoFor(AZoogiGameMode* G, int32 Owner)
		{
			static int32 Forced = -2; if (Forced == -2) { Forced = -1; FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretShot="), Forced); }
			if (Forced >= 1) return FMath::Clamp(Forced, 1, 6);
			if (const int32 La = ZoogiSkillLab::TurretAmmo()) return La; // b225: the Skill Lab's AMMO row (not INVENTORY)
			const AZoogiBall* Ob = G && G->Balls.IsValidIndex(Owner) ? G->Balls[Owner].Get() : nullptr;
			if (Ob && !Ob->bCPU) if (UZoogiFrontProfileSubsystem* Prof = UZoogiFrontProfileSubsystem::Get(G)) { const int32 Am = Prof->Data().ItemCounts.FindRef(FName(TEXT("turret_ammo"))); if (Am >= 2 && Am <= 6) return Am; }
			return 5;
		}
		float TurYawOff() { static float TyOff = -1000.f; if (TyOff < -999.f) { TyOff = -90.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiTurretYaw="), TyOff); } return TyOff; }
		FVector BarrelDir(const AActor* A) { return A ? FRotator(0.f, A->GetActorRotation().Yaw - TurYawOff(), 0.f).Vector() : FVector(1.f, 0.f, 0.f); }
		TArray<FTurSlash> TurSlashes; // b202: the turret's flying slashes
		float GTurTipFwd = 60.f, GTurTipUp = 110.f;
		const FLinearColor GatGold(1.f, 0.82f, 0.25f);
		// b223 (Evan: "for Gatling rapid instead of pausing like reaction and choosing like reaction we transition to tapping the super power
		// button to make the player dash toward the last direction the user taps on the screen"): 10 dashes, no pause - tap the court to
		// set where, SUPERPOWER = dash there (-ZoogiRapidReaction = the b200 REACTION version)
		struct FDashRun { TWeakObjectPtr<AZoogiBall> B; int32 Left = 10, MaxLeft = 10, Combo = 0; bool bCpu = false, bRolled = false, bHasTap = false, bPrevDown = false, bDashHit = true; FVector Tap = FVector::ZeroVector; double T0 = 0.0, NextCpu = 0.0, LastDashAt = -10.0; }; // b228: combo refund
		TArray<FDashRun> DashRuns;
		void DashOnce(AZoogiGameMode* G, FDashRun& R, const FVector& Toward)
		{
			AZoogiBall* Db = R.B.Get(); if (!Db || !Db->Sphere) return;
			const FVector Bl = Db->BallLocation();
			FVector Dir = (Toward - Bl).GetSafeNormal2D();
			if (Dir.IsNearlyZero()) Dir = Db->GetBallVelocity().GetSafeNormal2D();
			if (Dir.IsNearlyZero()) Dir = FRotator(0.f, Db->AimYaw, 0.f).Vector();
			static float DashSpeed = -1.f; if (DashSpeed < 0.f) { DashSpeed = 2000.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRapidDashSpeed="), DashSpeed); }
			const float Sp = FMath::Max(DashSpeed, (float)Db->GetBallVelocity().Size2D());
			Db->Sphere->WakeAllRigidBodies();
			Db->Sphere->SetPhysicsLinearVelocity(Dir * Sp + FVector(0.f, 0.f, 40.f));
			--R.Left;
			if (!R.bDashHit && FPlatformTime::Seconds() - R.LastDashAt < 1.2) R.Combo = 0; // b228: the last dash missed
			R.LastDashAt = FPlatformTime::Seconds(); R.bDashHit = false;
			UWorld* W = G->GetWorld();
			AZoogiFX::Spawn(W, Bl, GatGold, 160.f, 0.25f, true, 10.f);
			for (int32 k = 1; k <= 5; ++k) AZoogiFX::SpawnTrail(W, Bl - Dir * (k * 40.f), GatGold, 40.f, 0.35f, 6.f);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.7f, 1.6f);
			if (!R.bCpu) ZoogiHaptics::Kick(0.4f, W, TEXT("rapid dash"));
			UE_LOG(LogZoogi, Log, TEXT("RAPID DASH P%d toward (%.0f, %.0f) at %.0f cm/s - %d left"), Db->PlayerIndex + 1, Toward.X, Toward.Y, Sp, R.Left);
		}
		void DropTurrets() { DashRuns.Reset(); for (FTurret& T : Turrets) if (AActor* A = T.A.Get()) A->Destroy(); Turrets.Reset(); GatRuns.Reset(); for (FTurSlash& Ts : TurSlashes) if (AActor* C = Ts.Carrier.Get()) C->Destroy(); TurSlashes.Reset(); }
		bool GatTapNow(UWorld* W)
		{
			APlayerController* Pc = W ? W->GetFirstPlayerController() : nullptr;
			if (!Pc) return false;
			float X = 0.f, Y = 0.f; bool bT = false; Pc->GetInputTouchState(ETouchIndex::Touch1, X, Y, bT);
			return bT || Pc->IsInputKeyDown(EKeys::LeftMouseButton) || Pc->IsInputKeyDown(EKeys::SpaceBar);
		}
		void DashTick(AZoogiGameMode* G)
		{
			UWorld* W = G->GetWorld(); const double Tn = FPlatformTime::Seconds();
			for (int32 i = DashRuns.Num() - 1; i >= 0; --i)
			{
				FDashRun& R = DashRuns[i]; AZoogiBall* Db = R.B.Get();
				const bool bMine = Db && G->GetActiveBall() == Db && G->Phase == EZoogiPhase::Rolling;
				if (bMine) R.bRolled = true;
				if (!Db || Db->bOut || (R.Left <= 0 && Tn - R.LastDashAt > 1.0) || (R.bRolled && !bMine) || Tn - R.T0 > 40.0) // b228: the last dash still has 1 s to hit (refund)
				{
					UE_LOG(LogZoogi, Log, TEXT("RAPID DASH run ends (%d left)"), R.Left);
					DashRuns.RemoveAt(i); continue;
				}
				if (!bMine) continue;
				if (!R.bDashHit && Tn - R.LastDashAt < 1.0)
				{	// b228 (Evan picked "combo refund" + "raid damage"): a dash that hits an orb, a rival or (raid) the boss's weak point within 1 s
					// gives the dash back and builds the RELAY combo; the weak point also takes a hit
					const FVector Bp = Db->BallLocation(); bool bHit = false, bBossHit = false;
					for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist(O->OrbLocation(), Bp) < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 18.f) { bHit = true; break; }
					if (!bHit) for (const AZoogiBall* Ob : G->Balls) if (Ob && Ob != Db && !Ob->bOut && !G->IsAlly(Db, Ob) && FVector::Dist(Ob->BallLocation(), Bp) < 2.f * ZoogiConst::ZoogiRadius + 18.f) { bHit = true; break; }
					if (ZoogiRaid::Active()) if (const AActor* Hand = ZoogiRaid::HandActor()) if (FVector::Dist2D(Hand->GetActorLocation(), Bp) < 150.f + ZoogiConst::ZoogiRadius + 40.f) { bHit = true; bBossHit = true; }
					if (bHit)
					{
						R.bDashHit = true; ++R.Combo; R.Left = FMath::Min(R.MaxLeft, R.Left + 1);
						if (bBossHit) { ZoogiCosmosBoss::Flinch(); ZoogiBossAttack::HitBoss(G, Db->PlayerIndex); if (R.Combo >= 3) ZoogiBossAttack::HitBoss(G, Db->PlayerIndex); }
						G->PublicPopup(bBossHit ? FString::Printf(TEXT("RELAY HIT x%d!"), R.Combo) : FString::Printf(TEXT("RELAY x%d  +1 DASH"), R.Combo), Bp + FVector(0, 0, 170.f), bBossHit ? FLinearColor(1.f, 0.35f, 0.3f) : GatGold);
						ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.6f, 1.2f + 0.08f * FMath::Min(R.Combo, 6));
						UE_LOG(LogZoogi, Log, TEXT("ORB RELAY P%d: %s hit - combo %d, %d dashes"), Db->PlayerIndex + 1, bBossHit ? TEXT("BOSS") : TEXT("dash"), R.Combo, R.Left);
					}
				}
				if (R.bCpu)
				{	// CPU: three dashes at the nearest rival, 0.8 s apart
					if (R.Left > 7 && Tn >= R.NextCpu)
					{
						R.NextCpu = Tn + 0.8;
						const AZoogiBall* Near = nullptr; float Nd = 1e9f;
						for (AZoogiBall* Ob : G->Balls) if (Ob && Ob != Db && !Ob->bOut && !G->IsAlly(Db, Ob)) { const float Dd = FVector::Dist2D(Ob->BallLocation(), Db->BallLocation()); if (Dd < Nd) { Nd = Dd; Near = Ob; } }
						if (Near) DashOnce(G, R, Near->BallLocation());
					}
					continue;
				}
				static const bool bDashTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidDashTest")); // preview: a dash every 0.7 s toward a random spot
				if (bDashTest && R.Left > 0 && Tn >= R.NextCpu)
				{	// b229: toward the nearest orb (shows the refund), else a random spot
					const AZoogiOrb* No = nullptr; float Nd = 1e9f;
					for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float Dd = FVector::Dist2D(O->OrbLocation(), Db->BallLocation()); if (Dd < Nd) { Nd = Dd; No = O; } }
					R.NextCpu = Tn + 0.7; R.Tap = No ? No->OrbLocation() : Db->BallLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 500.f; R.bHasTap = true; AZoogiFX::Spawn(W, R.Tap, GatGold, 90.f, 0.35f, true, 8.f); DashOnce(G, R, R.Tap); continue;
				}
				if (bDashTest) continue;
				// the human: remember the last tap on the court (not the pad buttons)
				const bool bDown = GatTapNow(W);
				if (bDown && !R.bPrevDown && !ZoogiClassicPad::PointerOwned())
				{
					FVector Tp;
					if (RapidTapPoint(G, Db->BallLocation().Z - ZoogiConst::ZoogiRadius, Tp))
					{
						R.Tap = Tp; R.bHasTap = true;
						AZoogiFX::Spawn(W, Tp + FVector(0.f, 0.f, 5.f), GatGold, 90.f, 0.35f, true, 8.f);
						static const bool bSuperOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRelaySuperpower"));
						if (!bSuperOnly && R.Left > 0) DashOnce(G, R, Tp); // b228 (Evan picked "tap to dash"): the tap IS the dash (-ZoogiRelaySuperpower = b223)
					}
				}
				R.bPrevDown = bDown;
			}
		}
		void DashStart(AZoogiGameMode* G, AZoogiBall* Caster)
		{
			FDashRun R; R.B = Caster; R.bCpu = Caster->bCPU; R.T0 = FPlatformTime::Seconds(); R.NextCpu = R.T0 + 0.4;
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiRapidDashes="), R.Left);
			DashRuns.RemoveAll([Caster](const FDashRun& X) { return X.B.Get() == Caster; });
			R.MaxLeft = R.Left; // b229: set before the copy goes in (refunds capped at the real count)
			DashRuns.Add(R);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (!Caster->bCPU) F->PublicCallout(TEXT("ORB RELAY!"), FString::Printf(TEXT("TAP THE COURT TO DASH THERE  x%d  -  HIT SOMETHING = THE DASH COMES BACK"), R.Left), GatGold, 1.2f, 0.9f);
			AZoogiFX::Spawn(G->GetWorld(), Caster->BallLocation(), GatGold, 180.f, 0.4f, true, 10.f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 0.7f);
			UE_LOG(LogZoogi, Log, TEXT("SKILL gattling rapid P%d: %d dashes on SUPERPOWER"), Caster->PlayerIndex + 1, R.Left);
		}
		// rivals first, then orbs, nearest first, inside Range
		void GatTargets(AZoogiGameMode* G, const AZoogiBall* Me, const FVector& From, float Range, TArray<AActor*>& Out)
		{
			TArray<TPair<float, AActor*>> Rv, Ob;
			for (AZoogiBall* B : G->Balls) if (B && B != Me && !B->bOut && !(Me && G->IsAlly(Me, B))) { const float D = FVector::Dist2D(B->BallLocation(), From); if (D <= Range) Rv.Add({ D, B }); }
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), From); if (D <= Range) Ob.Add({ D, O }); }
			Rv.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key < B.Key; });
			Ob.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key < B.Key; });
			for (auto& P : Rv) Out.Add(P.Value);
			for (auto& P : Ob) Out.Add(P.Value);
		}
		// one shot: a gold tracer, then a small directed knock at the target (or a hit on the boss)
		void GatShot(AZoogiGameMode* G, const AZoogiBall* Spare, int32 Pi, const FVector& From, const FVector& To, float Mul, bool bBoss)
		{
			UWorld* W = G->GetWorld(); if (!W) return;
			for (int32 k = 1; k <= 6; ++k) AZoogiFX::Spawn(W, FMath::Lerp(From, To, k / 6.f) + FVector(0, 0, 30.f), GatGold, 34.f + 6.f * Mul, 0.16f, false, 9.f);
			AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), To + FVector(0, 0, 30.f), FRotator::ZeroRotator, 0.45f * Mul);
			ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.45f, 1.5f + FMath::FRandRange(-0.1f, 0.1f));
			if (bBoss) { ZoogiCosmosBoss::Flinch(); ZoogiBossAttack::HitBoss(G, Pi); return; }
			const FVector Dir = (To - From).GetSafeNormal2D();
			Blast(G, To - Dir * 70.f, 150.f, 900.f * Mul, 140.f * Mul, Spare, Pi);
		}
		void GatFire(AZoogiGameMode* G, FGatRun& R, float Mul)
		{
			AZoogiBall* B = R.B.Get(); if (!B) return;
			const FVector From = B->BallLocation();
			FVector BossP;
			const bool bBoss = (R.Fired % 2 == 1) && ZoogiBossAttack::LockOnTarget(G, BossP); // b190 BOSS LOCK-ON: every other shot at the boss
			if (bBoss) { GatShot(G, B, R.Pi, From, BossP, Mul, true); return; }
			TArray<AActor*> Tg; GatTargets(G, B, From, FMath::Max(900.f, G->ArcRange() * 1.4f), Tg);
			if (Tg.Num() == 0) { BoltFireAt(G, B, From, nullptr); return; } // b195
			AActor* T = Tg[R.Fired % FMath::Min(Tg.Num(), 4)];
			BoltFireAt(G, B, From, T); // b195: a Bolt Balls bolt (looks + blast)
		}
		float GShotFx = 1.f; // b225
		AStaticMeshActor* ShotCarrier(UWorld* W, const TCHAR* Shape, const FVector& At, const FRotator& Rot, const FVector& Scl, const FLinearColor& Col, float Glow)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* C = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rot, At), SP);
			if (!C) return nullptr;
			C->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* Mc = C->GetStaticMeshComponent();
			Mc->SetStaticMesh(ZoogiUtil::BasicShape(Shape)); Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mc->SetCastShadow(false);
			Mc->SetMaterial(0, ZoogiUtil::FxMID(C, Col, Glow));
			const bool bCyl = FCString::Strcmp(Shape, TEXT("Cylinder")) == 0; // the beam keeps its length
			C->SetActorScale3D(bCyl ? FVector(Scl.X * GShotFx, Scl.Y * GShotFx, Scl.Z) : Scl * GShotFx);
			return C;
		}
		/** b221: one turret shot of Style from the barrel tip, straight along the barrel; returns the seconds until it lands */
		float BarrelShot(AZoogiGameMode* G, AActor* Tur, const FVector& Base, float TipFwd, float TipUp, const FVector& Tp, int32 Owner, float Pow, int32 Style, AActor* Target, bool bBoss = false)
		{
			UWorld* W = G ? G->GetWorld() : nullptr; if (!W || !Tur) return 0.f;
			const FVector Fw = BarrelDir(Tur);
			const FVector From = FVector(Base.X, Base.Y, Base.Z) + Fw * TipFwd + FVector(0.f, 0.f, TipUp);
			const float Dist = FMath::Clamp((float)FVector::Dist2D(From, Tp), 150.f, bBoss ? 3500.f : 1400.f); // b223: the raid boss can be far
			const FVector To = From + Fw * Dist + FVector(0.f, 0.f, Tp.Z - From.Z); // straight out of the barrel (no homing)
			const FVector Dir3 = (To - From).GetSafeNormal();
			static const float Speeds[7] = { 2200.f, 2200.f, 4200.f, 1700.f, 9000.f, 1500.f, 1800.f };
			const FLinearColor Cols[7] = { FLinearColor(0.75f, 0.35f, 1.f), FLinearColor(0.75f, 0.35f, 1.f), FLinearColor(1.f, 0.82f, 0.3f), FLinearColor(1.f, 0.3f, 0.9f), FLinearColor(1.f, 0.15f, 0.2f), FLinearColor(1.f, 0.55f, 0.15f), FLinearColor(0.3f, 0.9f, 1.f) };
			Style = FMath::Clamp(Style, 1, 6);
			FLinearColor Col = Cols[Style];
			{ FLinearColor Lt; if (ZoogiSkillLab::TintFor(FName(TEXT("gattling_turret")), Lt)) Col = Lt; } // b225: the lab COLOUR
			GShotFx = ZoogiSkillLab::SizeOf(FName(TEXT("gattling_turret"))); // b225: the lab FX SIZE
			AZoogiFX::Spawn(W, From, Col, Style == 2 ? 70.f : 120.f, 0.14f, false, 16.f); // muzzle flash at the barrel tip
			const float Dur = FMath::Clamp(FVector::Dist(From, To) / Speeds[Style], 0.06f, 0.9f);
			auto Add = [&](AActor* C, float Delay, float Push)
			{
				FTurSlash S2; S2.Turret = Tur; S2.Carrier = C; S2.From = From; S2.To = To; S2.Dur = Dur; S2.T = -Delay; S2.Owner = Owner; S2.Pow = Pow; S2.Target = Target; S2.Style = Style; S2.Push = Push; S2.bBoss = bBoss;
				if (C && Delay > 0.f) C->SetActorHiddenInGame(true);
				TurSlashes.Add(S2);
			};
			switch (Style)
			{
			case 2: // TRACER BURST: three gold rounds, 0.08 s apart
				for (int32 k = 0; k < 3; ++k) { AStaticMeshActor* Tc = ShotCarrier(W, TEXT("Sphere"), From, Dir3.Rotation(), FVector(0.75f, 0.13f, 0.13f), Col, 22.f); Add(Tc, 0.08f * k, k == 2 ? 0.7f : 0.35f); if (Tc) ZoogiVoyager::Fx(W, TEXT("MachinegunTrail"), From, 1.f, Dur + 0.9f, Tc, Dir3.Rotation()); } // b233: Voyager tracer trails
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.7f, 2.2f);
				break;
			case 3: // PLASMA BOLT: a big glowing ball with a trail
				{ AStaticMeshActor* Pc = ShotCarrier(W, TEXT("Sphere"), From, Dir3.Rotation(), FVector(0.55f), Col, 14.f); Add(Pc, 0.f, 1.f); if (Pc) ZoogiVoyager::Fx(W, TEXT("PlasmaTrail"), From, 1.f, Dur + 0.9f, Pc, Dir3.Rotation()); } // b233
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.2f);
				break;
			case 4: // LASER BEAM: a beam from the barrel tip to the end of its reach, lands at once
			{
				const float Len = FVector::Dist(From, To);
				AStaticMeshActor* Bm = ShotCarrier(W, TEXT("Cylinder"), (From + To) * 0.5f, FRotationMatrix::MakeFromZ(Dir3).Rotator(), FVector(0.14f, 0.14f, Len / 100.f), Col, 30.f);
				if (Bm) Bm->SetLifeSpan(0.28f);
				ZoogiVoyager::Fx(W, TEXT("Tracer"), From, 1.f, 0.8f, nullptr, Dir3.Rotation()); // b233
				Add(nullptr, 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.9f, 0.8f);
				break;
			}
			case 5: // ROCKET: a nose cone with a smoke trail, a fire burst on impact
				{ AStaticMeshActor* Rc = ShotCarrier(W, TEXT("Cone"), From, FRotationMatrix::MakeFromZ(Dir3).Rotator(), FVector(0.32f, 0.32f, 0.75f), Col, 10.f); Add(Rc, 0.f, 1.2f); if (Rc) ZoogiVoyager::Fx(W, TEXT("RocketTrail"), From, 1.f, Dur + 1.2f, Rc, (-Dir3).Rotation()); } // b233: Voyager rocket smoke
				ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.5f, 0.7f);
				break;
			case 6: // SHOCK RING: a ring that grows as it flies
				Add(ShotCarrier(W, TEXT("Cylinder"), From, FRotationMatrix::MakeFromZ(Dir3).Rotator(), FVector(0.6f, 0.6f, 0.04f), Col, 12.f), 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.5f);
				break;
			default: // 1 CRESCENT SLASH (b202)
			{
				AStaticMeshActor* C = ShotCarrier(W, TEXT("Sphere"), From, Dir3.Rotation(), FVector(0.35f), Col, 14.f);
				AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), From, 0.42f, 2.f, C, FVector::ZeroVector, Fw.Rotation());
				Add(C, 0.f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.7f);
				break;
			}
			}
			UE_LOG(LogZoogi, Log, TEXT("TURRET shot style %d of P%d: barrel tip (%.0f, %.0f, %.0f) -> straight %.0f cm along yaw %.0f"), Style, Owner + 1, From.X, From.Y, From.Z, Dist, Fw.Rotation().Yaw);
			return Dur + (Style == 2 ? 0.16f : 0.f);
		}
		void TurretSlash(AZoogiGameMode* G, const FTurret& T, AActor* Target, const FVector& Tp, bool bBoss = false)
		{
			static const bool bOldSlash = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTurretOldSlash"));
			if (!bOldSlash) { if (AActor* A = T.A.Get()) BarrelShot(G, A, T.P, T.TipFwd, T.TipUp, Tp, T.Owner, T.Pow, TurretAmmoFor(G, T.Owner), Target, bBoss); return; } // b221; b223 ammo + boss	// b202: a crescent slash flies from the turret's gem to the target (~2200 cm/s), then cuts it away from the turret
			UWorld* W = G->GetWorld(); if (!W) return;
			const FVector Fw = (Tp - T.P).GetSafeNormal2D();
			const FVector From = T.P + Fw * T.TipFwd + FVector(0, 0, T.TipUp), To(Tp.X, Tp.Y, Tp.Z); // b202 (Evan: "from the tip of the turret barrel end")
			AZoogiFX::Spawn(W, From, FLinearColor(1.f, 0.85f, 0.45f), 120.f, 0.15f, false, 16.f); // muzzle flash
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* C = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform((To - From).Rotation(), From), SP);
			if (!C) return;
			C->SetMobility(EComponentMobility::Movable);
			C->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); C->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->GetStaticMeshComponent()->SetCastShadow(false); C->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(C, FLinearColor(0.75f, 0.35f, 1.f), 14.f)); C->SetActorScale3D(FVector(0.35f));
			const FRotator Rot((To - From).GetSafeNormal2D().Rotation());
			AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), From, 0.42f, 2.f, C, FVector::ZeroVector, Rot); // b202: 0.9 filled the screen
			AZoogiFX::Spawn(W, From, FLinearColor(0.75f, 0.35f, 1.f), 90.f, 0.2f, false, 12.f);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.7f);
			FTurSlash S2; S2.Turret = T.A; S2.Carrier = C; S2.From = From; S2.To = To; S2.Dur = FMath::Clamp(FVector::Dist(From, To) / 2200.f, 0.12f, 0.6f); S2.Owner = T.Owner; S2.Pow = T.Pow; S2.Target = Target;
			TurSlashes.Add(S2);
			UE_LOG(LogZoogi, Log, TEXT("SKILL gattling turret of P%d slashes %s (%.0f cm)"), T.Owner + 1, *GetNameSafe(Target), FVector::Dist2D(From, To));
		}
		void TurSlashTick(AZoogiGameMode* G, float Dt)
		{
			static uint64 TsFrame = 0; if (TsFrame == GFrameCounter) return; TsFrame = GFrameCounter; // b221: once a frame (the game tick + the demo rig)
			UWorld* W = G->GetWorld();
			for (int32 i = TurSlashes.Num() - 1; i >= 0; --i)
			{
				FTurSlash& S2 = TurSlashes[i];
				// b221: no homing - the shot flies straight along the barrel (-ZoogiTurretOldSlash = the b202 homing slash)
				static const bool bHoming = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTurretOldSlash"));
				if (bHoming) if (const AActor* Ta = S2.Target.Get()) { const FVector Tl = Cast<AZoogiBall>(Ta) ? Cast<AZoogiBall>(Ta)->BallLocation() : Cast<AZoogiOrb>(Ta) ? Cast<AZoogiOrb>(Ta)->OrbLocation() : Ta->GetActorLocation(); S2.To = Tl; } // homes on where it is now
				S2.T += Dt;
				if (S2.T < 0.f) continue; // a tracer still in the barrel
				if (!S2.bShown) { S2.bShown = true; if (AActor* C = S2.Carrier.Get()) C->SetActorHiddenInGame(false); if (S2.Style == 2) AZoogiFX::Spawn(W, S2.From, FLinearColor(1.f, 0.82f, 0.3f), 70.f, 0.1f, false, 16.f); }
				const float K = FMath::Clamp(S2.T / S2.Dur, 0.f, 1.f);
				const FVector Pk = FMath::Lerp(S2.From, S2.To, K);
				if (AActor* C = S2.Carrier.Get())
				{
					C->SetActorLocation(Pk);
					if (S2.Style == 6) C->SetActorScale3D(FVector(0.6f + 1.1f * K, 0.6f + 1.1f * K, 0.04f));
				}
				const double Tw = FPlatformTime::Seconds();
				if ((S2.Style == 3 || S2.Style == 5) && Tw >= S2.NextTrail)
				{	// plasma glow / rocket smoke behind the shot
					S2.NextTrail = Tw + 0.025;
					AZoogiFX::SpawnTrail(W, Pk, S2.Style == 3 ? FLinearColor(1.f, 0.3f, 0.9f) : FLinearColor(0.75f, 0.75f, 0.78f), S2.Style == 3 ? 38.f : 46.f, S2.Style == 3 ? 0.3f : 0.7f, S2.Style == 3 ? 6.f : 0.6f);
				}
				if (K < 1.f) continue;
				if (AActor* C = S2.Carrier.Get()) C->Destroy();
				const FVector Dir = (S2.To - S2.From).GetSafeNormal2D();
				switch (S2.Style)
				{
				case 2: AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.82f, 0.3f), 90.f, 0.18f, false, 12.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_Gold"), S2.To, FRotator::ZeroRotator, 0.35f); ZoogiVoyager::Fx(W, TEXT("ImpactConcrete"), S2.To, 1.f, 1.5f); break;
				case 3: AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.3f, 0.9f), 220.f, 0.35f, true, 10.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_Purple"), S2.To, FRotator::ZeroRotator, 0.6f); ZoogiVoyager::Fx(W, TEXT("PlasmaBoom"), S2.To, 0.7f, 2.5f); break;
				case 4: AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.3f, 0.3f), 160.f, 0.25f, false, 14.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_Red"), S2.To, FRotator::ZeroRotator, 0.45f); ZoogiVoyager::Fx(W, TEXT("ImpactMetal"), S2.To, 1.f, 1.5f); break;
				case 5: if (ZoogiVoyager::On()) ZoogiVoyager::Fx(W, TEXT("RocketBoom"), S2.To, 0.7f, 3.f); else AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), S2.To, 0.55f); AZoogiFX::Spawn(W, S2.To, FLinearColor(1.f, 0.55f, 0.15f), 240.f, 0.4f, true, 10.f); break; // b233: Voyager rocket explosion
				case 6: AZoogiFX::Spawn(W, S2.To, FLinearColor(0.3f, 0.9f, 1.f), 300.f, 0.4f, true, 10.f); AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), S2.To, FRotator::ZeroRotator, 0.6f); break;
				default:
					AZoogiFabFX::Play(W, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), S2.To, 0.5f, 2.f, nullptr, FVector::ZeroVector, Dir.Rotation());
					AZoogiFX::Spawn(W, S2.To, FLinearColor(0.75f, 0.35f, 1.f), 180.f, 0.3f, true, 10.f);
					break;
				}
				ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), S2.Style == 2 ? 0.35f : 0.6f, S2.Style == 5 ? 0.9f : 1.4f);
				if (S2.bBoss) { if (S2.Push >= 0.7f) { ZoogiCosmosBoss::Flinch(); ZoogiBossAttack::HitBoss(G, S2.Owner); } } // b223: a raid boss hit
				else { FTuneScope Tn3(S2.Pow, FName(TEXT("gattling_turret"))); Blast(G, S2.To - Dir * 70.f, 150.f, 950.f * S2.Push, 140.f * S2.Push, nullptr, S2.Owner); }
				TurSlashes.RemoveAt(i);
			}
		}
		void GatTick(AZoogiGameMode* G)
		{
			TurSlashTick(G, FMath::Min(G->GetWorld() ? G->GetWorld()->GetDeltaSeconds() : 0.016f, 0.05f)); // b202
			{	// b224 preview (-ZoogiRapidDashTest): the human gets a dash run as soon as it rolls (no skill pick needed)
				static const bool bDashT = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidDashTest"));
				AZoogiBall* Hb = G->GetActiveBall();
				static TWeakObjectPtr<AZoogiBall> DashTDone; static int32 DashTRound = -1; // b229: once per roll (no restart spam)
				if (bDashT && Hb && !Hb->bCPU && !Hb->bOut && (DashTDone.Get() != Hb || DashTRound != G->RoundNumber) && G->Phase == EZoogiPhase::Rolling && !DashRuns.ContainsByPredicate([Hb](const FDashRun& X) { return X.B.Get() == Hb; })) { DashStart(G, Hb); DashTDone = Hb; DashTRound = G->RoundNumber; }
			}
			DashTick(G); // b223
			UWorld* W = G->GetWorld();
			const double Tn = FPlatformTime::Seconds();
			const bool bDown = GatTapNow(W);
			for (int32 i = GatRuns.Num() - 1; i >= 0; --i)
			{
				FGatRun& R = GatRuns[i];
				AZoogiBall* B = R.B.Get();
				if (!B || B->bOut || Tn - R.T0 > 12.0) { GatRuns.RemoveAt(i); continue; }
				const bool bTapEdge = bDown && !R.bPrevDown;
				if (bTapEdge) R.LastTapAt = Tn;
				R.bPrevDown = bDown;
				if (R.Kind == 1)
				{	// b194 (Evan: "rapid fire as much and as fast as I can tap for 3 s - the orb generator must keep up"): every tap is a shot
					const double End = R.T0 + 0.3 + 6.0; // b197 (Evan): 6 s, the world paused like a REACTION
					const bool bShoot = Tn >= R.T0 + 0.3 && Tn < End && Tn - R.NextAt >= 0.06 && (R.bCpu ? Tn - R.NextAt >= 0.14 : bTapEdge);
					if (bShoot)
					{
						R.NextAt = Tn;
						FVector Tp; static const bool bOldRapid = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidAuto"));
						if (!R.bCpu && !bOldRapid && RapidTapPoint(G, B->BallLocation().Z - ZoogiConst::ZoogiRadius, Tp))
						{	// b200: a circle where you tapped, a Bolt Balls bolt flies straight to it (2600 cm/s)
							FTuneScope Tn2(R.Pow, FName(TEXT("gattling_beat")));
							BoltFireAt(G, B, B->BallLocation(), nullptr, &Tp);
							FRapidMark Mk; Mk.P = Tp; Mk.T = Tn; Mk.PopAt = Tn + FVector::Dist2D(Tp, B->BallLocation()) / 2600.0; RapidMarkList.Add(Mk);
						}
						else { FTuneScope Tn2(R.Pow, FName(TEXT("gattling_beat"))); GatFire(G, R, 0.8f); }
						++R.Fired;
					}
					if (Tn - R.Perfect * 0.3 >= R.T0 + 0.3 && Tn < End) { ++R.Perfect; G->SpireRefillOrbs(2, false); } // the generator keeps up (every 0.3 s)
					if (Tn >= End)
					{
						G->PublicPopup(FString::Printf(TEXT("%d SHOTS!"), R.Fired), B->BallLocation() + FVector(0, 0, 210.f), GatGold);
						UE_LOG(LogZoogi, Log, TEXT("SKILL gattling rapid P%d done (%d shots in 6 s)"), R.Pi + 1, R.Fired);
						GatRuns.RemoveAt(i);
					}
					continue;
				}
				else if (Tn < R.NextAt)
				{	// BURST spin-up: gold sparks circle the caster
					const float A = (float)(Tn - R.T0) * 14.f;
					AZoogiFX::Spawn(W, B->BallLocation() + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * 70.f + FVector(0, 0, 25.f), GatGold, 22.f, 0.12f, false, 9.f);
				}
				if (Tn < R.NextAt) continue;
				float Mul = 1.f;
				if (R.Kind == 1)
				{
					const double Beat = R.NextAt - 0.12;
					const bool bPerfect = R.bCpu ? FMath::FRand() < 0.6f : (R.LastTapAt >= Beat - 0.18 && R.LastTapAt <= Beat + 0.12);
					Mul = bPerfect ? 1.6f : 0.85f;
					if (bPerfect) { ++R.Perfect; G->PublicPopup(TEXT("PERFECT!"), B->BallLocation() + FVector(0, 0, 170.f), GatGold); }
					R.NextAt += R.BeatLen;
				}
				else R.NextAt += R.Gap;
				{ FTuneScope Tn2(R.Pow, FName(R.Kind == 1 ? TEXT("gattling_beat") : TEXT("gattling_burst"))); GatFire(G, R, Mul); }
				++R.Fired;
				if (R.Fired >= R.Total)
				{
					if (R.Kind == 1) G->PublicPopup(FString::Printf(TEXT("%d / %d PERFECT"), R.Perfect, R.Total), B->BallLocation() + FVector(0, 0, 210.f), GatGold);
					UE_LOG(LogZoogi, Log, TEXT("SKILL gattling %s P%d done (%d shots, %d perfect)"), R.Kind == 1 ? TEXT("beat") : TEXT("burst"), R.Pi + 1, R.Fired, R.Perfect);
					GatRuns.RemoveAt(i);
				}
			}
			// TURRETS: fire at any rival rolling near, for 2 of the owner's turns
			for (int32 i = Turrets.Num() - 1; i >= 0; --i)
			{
				FTurret& T = Turrets[i];
				AActor* A = T.A.Get();
				bool bOutOfAmmo = false; // b223 (Evan: "Gatling turrets only get 10 shots"): gone after its 10th shot lands
				if (A && T.ShotsLeft <= 0) { bool bFly = false; for (const FTurSlash& Ts : TurSlashes) if (Ts.Turret.Get() == A) bFly = true; bOutOfAmmo = !bFly && Tn >= T.NextShot + 0.6; }
				if (!A || G->TurnCounter >= T.EndTurn || bOutOfAmmo)
				{
					if (A) { AZoogiFX::Spawn(W, T.P, GatGold, 160.f, 0.4f, true, 8.f); A->Destroy(); }
					UE_LOG(LogZoogi, Log, TEXT("SKILL gattling turret of P%d expires"), T.Owner + 1);
					Turrets.RemoveAt(i); continue;
				}
				if (G->TurnCounter != T.Turn) { T.Turn = G->TurnCounter; T.Shots = 0; AZoogiFX::Spawn(W, T.P, FLinearColor(0.75f, 0.35f, 1.f), 520.f, 1.f, true, 5.f); }
				static const bool bOldTurret = FParse::Param(FCommandLine::Get(), TEXT("ZoogiTurretBolts"));
				const AZoogiBall* Ow = G->Balls.IsValidIndex(T.Owner) ? G->Balls[T.Owner].Get() : nullptr;
				if (!bOldTurret)
				{	// b202 (Evan: "the turret should rotate toward the nearest orb or player and shoot it automatically with a slash projectile"):
					// it always turns to face the nearest rival or orb in reach, and while things roll it fires a flying slash at it
					AActor* Best = nullptr; float Bd = 1000.f;
					for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->PlayerIndex != T.Owner && !(Ow && G->IsAlly(Ow, B))) { const float D = FVector::Dist2D(B->BallLocation(), T.P); if (D < Bd) { Bd = D; Best = B; } }
					for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), T.P); if (D < Bd) { Bd = D; Best = O; } }
					// b223 (Evan: "turrets also shoot at nearest orbs, opposing player and boss on raid only"): in a raid the boss is a target too
					bool bBossT = false; FVector BossP = FVector::ZeroVector;
					if (ZoogiRaid::Active() && ZoogiBossAttack::LockOnTarget(G, BossP)) { const float Dbs = FVector::Dist2D(BossP, T.P); if (Dbs < 2600.f && (!Best || Dbs < Bd)) { bBossT = true; Best = nullptr; Bd = Dbs; } }
					if (!Best && !bBossT) continue;
					const FVector Tp = bBossT ? BossP : Cast<AZoogiBall>(Best) ? Cast<AZoogiBall>(Best)->BallLocation() : Cast<AZoogiOrb>(Best) ? Cast<AZoogiOrb>(Best)->OrbLocation() : Best->GetActorLocation();
					const float TurYaw = TurYawOff(); // b221
					const float WantYaw = (Tp - T.P).Rotation().Yaw + TurYaw;
					const float CurYaw = A->GetActorRotation().Yaw;
					const float NewYaw = CurYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(CurYaw, WantYaw), -540.f * 0.016f, 540.f * 0.016f); // turns ~540 deg/s
					A->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
					{	// b202 (Evan: "make it jolt and vibrate when it shoots"): a recoil kick back off the barrel + a short rattle
						const float Ka = (float)(Tn - T.KickAt);
						FVector Off = FVector::ZeroVector;
						if (Ka >= 0.f && Ka < 0.32f)
						{
							const float Kk = 1.f - Ka / 0.32f;
							Off = -T.KickDir * 28.f * Kk * Kk + FVector(FMath::FRandRange(-5.f, 5.f), FMath::FRandRange(-5.f, 5.f), FMath::FRandRange(-2.f, 3.f)) * Kk;
						}
						A->SetActorLocation(T.Home + Off);
					}
					bool bInFlight = false; for (const FTurSlash& Ts : TurSlashes) if (Ts.Turret.Get() == A) bInFlight = true; // b202 (Evan: "one shot at a time")
					if (bInFlight || (G->Phase != EZoogiPhase::Rolling && !ZoogiSkillLab::Open()) || Tn < T.NextShot || T.ShotsLeft <= 0 || FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantYaw)) > 20.f) continue;
					T.NextShot = Tn + 0.35 / ZoogiSkillLab::TurretRate(); ++T.Shots; --T.ShotsLeft; // b223: 10 shots in all; b225 lab FIRE RATE
					T.KickAt = Tn; T.KickDir = (Tp - T.P).GetSafeNormal2D();
					TurretSlash(G, T, Best, Tp, bBossT);
					UE_LOG(LogZoogi, Log, TEXT("TURRET of P%d fires at %s - %d shots left"), T.Owner + 1, bBossT ? TEXT("the BOSS") : *GetNameSafe(Best), T.ShotsLeft);
					if (Ow && !Ow->bCPU) ZoogiHaptics::Kick(0.35f, W, TEXT("turret shot"));
					continue;
				}
				if (G->Phase != EZoogiPhase::Rolling || Tn < T.NextShot || T.Shots >= 8) continue;
				AZoogiBall* Best = nullptr; float Bd = 520.f;
				for (AZoogiBall* B : G->Balls)
					if (B && !B->bOut && B->PlayerIndex != T.Owner && !(Ow && G->IsAlly(Ow, B)) && B->GetBallVelocity().Size2D() > 60.f)
					{ const float D = FVector::Dist2D(B->BallLocation(), T.P); if (D < Bd) { Bd = D; Best = B; } }
				if (!Best) continue;
				T.NextShot = Tn + 0.45; ++T.Shots;
				if (AActor* Ta = T.A.Get()) Ta->SetActorRotation(FRotator(0.f, (Best->BallLocation() - T.P).Rotation().Yaw, 0.f));
				{ FTuneScope Tn3(T.Pow, FName(TEXT("gattling_turret"))); if (AZoogiBall* Owb = G->Balls.IsValidIndex(T.Owner) ? G->Balls[T.Owner].Get() : nullptr) BoltFireAt(G, Owb, T.P + FVector(0, 0, 150.f), Best); } // b195: a bolt from the orb
			}
		}
	}
	int32 RapidMarks(TArray<FVector>& OutP, TArray<float>& OutK)
	{	// b200: K < 0 = seconds until the bolt lands, K >= 0 = seconds since it popped
		const double Tn = FPlatformTime::Seconds();
		RapidMarkList.RemoveAll([&](const FRapidMark& M) { return Tn - M.PopAt > 0.35; });
		for (const FRapidMark& M : RapidMarkList) { OutP.Add(M.P); OutK.Add((float)(Tn - M.PopAt)); }
		return RapidMarkList.Num();
	}
	bool DashSlowMo()
	{	// b228 (Evan picked "short slow-motion"): ~0.35 s of slow-mo after each human dash, to aim the next one
		static float Sl = -1.f; if (Sl < 0.f) { Sl = 0.35f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRelaySlowMo="), Sl); }
		for (const FDashRun& R : DashRuns) if (!R.bCpu && R.B.IsValid() && FPlatformTime::Seconds() - R.LastDashAt < Sl) return true;
		return false;
	}
	bool DashActive(int32* OutLeft)
	{	// b223: the human's GATTLING RAPID dashes
		for (const FDashRun& R : DashRuns) if (!R.bCpu && R.B.IsValid() && R.Left > 0) { if (OutLeft) *OutLeft = R.Left; return true; }
		return false;
	}
	bool DashGo(AZoogiGameMode* G)
	{
		for (FDashRun& R : DashRuns)
			if (!R.bCpu && R.B.IsValid() && R.Left > 0)
			{
				AZoogiBall* Db = R.B.Get();
				if (G->Phase != EZoogiPhase::Rolling || G->GetActiveBall() != Db) { G->PublicPopup(TEXT("LAUNCH, THEN SUPERPOWER TO DASH"), Db->BallLocation() + FVector(0, 0, 190.f), GatGold); return true; }
				DashOnce(G, R, R.bHasTap ? R.Tap : Db->BallLocation() + Db->GetBallVelocity().GetSafeNormal2D() * 500.f);
				return true;
			}
		return false;
	}
	bool RapidActive(int32* OutShots, float* OutLeft)
	{	// b197: a human's RAPID run (the CPU's runs don't pause the world)
		for (const FGatRun& R : GatRuns)
			if (R.Kind == 1 && !R.bCpu && R.B.IsValid())
			{
				if (OutShots) *OutShots = R.Fired;
				if (OutLeft) *OutLeft = (float)FMath::Max(0.0, R.T0 + 6.3 - FPlatformTime::Seconds());
				return true;
			}
		return false;
	}
	void GatStart(AZoogiGameMode* G, AZoogiBall* Caster, int32 Kind)
	{
		static const bool bRapidBolts = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidBolts"));
		static const bool bRapidReaction = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRapidReaction")); // b223: the b200 version
		if (Kind == 1 && !bRapidBolts && !bRapidReaction) { DashStart(G, Caster); return; } // b223: 10 SUPERPOWER dashes
		if (Kind == 1 && !bRapidBolts)
		{	// b200 (Evan: "make gattling rapid exactly the same as REACTION - we're no longer shooting but rolling ourselves, the only
			// difference is you get 10 shots - basically you just get 10 crystals to use"): +10 crystals right now (past the cap, like
			// Energy Buff), and if your Zoogi is already rolling the REACTION pick opens at once
			const int32 Pi = FMath::Clamp(Caster->PlayerIndex, 0, 3), Was = G->AwakenCharge[Pi];
			if (G->AwakenChargeOn()) G->AwakenCharge[Pi] = Was + 10;
			G->PublicPopup(TEXT("+10 CRYSTALS"), Caster->BallLocation() + FVector(0, 0, 190.f), GatGold);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (!Caster->bCPU) F->PublicCallout(TEXT("GATTLING RAPID!"), TEXT("10 REACTIONS - REDIRECT YOURSELF AGAIN AND AGAIN"), GatGold, 1.2f, 0.9f);
			AZoogiFX::Spawn(G->GetWorld(), Caster->BallLocation(), GatGold, 180.f, 0.4f, true, 10.f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 0.7f);
			const bool bNow = !Caster->bCPU && G->Phase == EZoogiPhase::Rolling && G->GetActiveBall() == Caster && ZoogiRedirect::Available(G) && ZoogiRedirect::Begin(G);
			UE_LOG(LogZoogi, Log, TEXT("SKILL gattling rapid P%d: crystals %d -> %d%s"), Pi + 1, Was, G->AwakenCharge[Pi], bNow ? TEXT(" - REACTION opened") : TEXT(""));
			return;
		}
		FGatRun R; R.B = Caster; R.Kind = Kind; R.Pi = Caster->PlayerIndex; R.T0 = FPlatformTime::Seconds(); R.Pow = GSkillPow; R.bCpu = Caster->bCPU; R.bPrevDown = GatTapNow(G->GetWorld());
		if (Kind == 1)
		{	// b194: RAPID FIRE - 3 s, one shot per tap (as fast as you can tap); the orb generator keeps refilling
			R.Total = 999; R.NextAt = 0.0; R.Perfect = 0;
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (!Caster->bCPU) F->PublicCallout(TEXT("GATTLING RAPID FIRE!"), TEXT("TIME STOPS - TAP AS FAST AS YOU CAN FOR 6 SECONDS!"), GatGold, 1.2f, 0.9f);
		}
		else { R.Total = 6; R.Gap = 0.17f; R.NextAt = R.T0 + 0.45; }
		AZoogiFX::Spawn(G->GetWorld(), Caster->BallLocation(), GatGold, 180.f, 0.4f, true, 10.f);
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 0.7f);
		GatRuns.Add(R);
		UE_LOG(LogZoogi, Log, TEXT("SKILL gattling %s by P%d (%d shots)"), Kind == 1 ? TEXT("beat") : TEXT("burst"), R.Pi + 1, R.Total);
	}
	AActor* TurretDemoSpawn(AZoogiGameMode* G, const FVector& Floor, float& OutTipFwd, float& OutTipUp)
	{	// b221: the turret model alone (no owner, no auto fire) for -ZoogiDemoTurret
		UWorld* W = G ? G->GetWorld() : nullptr; if (!W) return nullptr;
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/Turret/SM_Turret.SM_Turret"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) return nullptr;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Floor), SP);
		if (!A) return nullptr;
		A->SetMobility(EComponentMobility::Movable);
		const FBox Bb = M->GetBoundingBox(); const float Hh = FMath::Max(1.f, (float)Bb.GetSize().Z), Sc = 240.f / Hh;
		A->GetStaticMeshComponent()->SetStaticMesh(M); A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); A->GetStaticMeshComponent()->SetWorldScale3D(FVector(Sc));
		A->SetActorLocation(Floor - FVector(0, 0, Bb.Min.Z * Sc));
		OutTipFwd = FMath::Max(Bb.Max.Y, Bb.Max.X) * Sc + 10.f; OutTipUp = 0.62f * Hh * Sc;
		return A;
	}
	float TurretYawOffset() { return TurYawOff(); }
	float TurretShoot(AZoogiGameMode* G, AActor* Turret, const FVector& Floor, float TipFwd, float TipUp, const FVector& Target, int32 Style)
	{
		return BarrelShot(G, Turret, Floor, TipFwd, TipUp, Target, -1, 1.f, Style, nullptr);
	}
	void TurretShotsTick(AZoogiGameMode* G, float Dt) { if (G) TurSlashTick(G, Dt); }
	void TurretStart(AZoogiGameMode* G, AZoogiBall* Caster, const FVector& Spot)
	{
		UWorld* W = G->GetWorld(); if (!W) return;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector P(Spot.X, Spot.Y, Caster->BallLocation().Z - ZoogiConst::ZoogiRadius + 2.f);
		AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(P), SP);
		if (!A) return;
		A->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* Base = A->GetStaticMeshComponent();
		// b195 (Evan: "no turret comes out"): a big, bright tower - gold pedestal, a glowing purple bolt orb on top, a gold barrel
		const FLinearColor TurPurple(0.75f, 0.35f, 1.f);
		Base->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder"))); Base->SetWorldScale3D(FVector(1.1f, 1.1f, 1.2f));
		Base->SetCollisionEnabled(ECollisionEnabled::NoCollision); Base->SetMaterial(0, ZoogiUtil::SurfaceMID(A, FLinearColor(0.95f, 0.72f, 0.2f), 0.3f, FLinearColor(1.f, 0.6f, 0.1f), 0.6f));
		A->SetActorLocation(P + FVector(0, 0, 60.f)); // the cylinder's centre - it stands on the floor
		auto Part = [&](const TCHAR* Shape, const FVector& Rel, const FVector& Scl, UMaterialInterface* M) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A); C->SetStaticMesh(ZoogiUtil::BasicShape(Shape)); C->SetupAttachment(A->GetRootComponent());
			C->SetUsingAbsoluteScale(true); C->SetRelativeScale3D(Scl); C->SetRelativeLocation(Rel); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); C->SetMaterial(0, M);
			return C;
		};
		static UStaticMesh* TurMesh = nullptr; static bool bTurTried = false;
		if (!bTurTried) { bTurTried = true; if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldTurret"))) TurMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/Turret/SM_Turret.SM_Turret"), nullptr, LOAD_NoWarn | LOAD_Quiet); UE_LOG(LogZoogi, Log, TEXT("TURRET model: %s"), TurMesh ? TEXT("Evan's Tripo turret") : TEXT("placeholder tower")); }
		if (TurMesh)
		{	// b202 (Evan's Tripo turret, retopo 9.2k tris): the model, ~2.4 m tall, standing on the floor; the actor turns to aim
			const FBox Bb = TurMesh->GetBoundingBox(); const float Hh = FMath::Max(1.f, (float)Bb.GetSize().Z), Sc = 240.f / Hh;
			Base->SetStaticMesh(TurMesh); Base->EmptyOverrideMaterials(); Base->SetWorldScale3D(FVector(Sc));
			A->SetActorLocation(P - FVector(0, 0, Bb.Min.Z * Sc));
			GTurTipFwd = FMath::Max(Bb.Max.Y, Bb.Max.X) * Sc + 10.f; GTurTipUp = 0.62f * Hh * Sc; // b202: the barrel end - the front of the model, at the gem
		}
		else
		{
		Part(TEXT("Sphere"), FVector(0.f, 0.f, 79.f), FVector(0.9f), ZoogiUtil::FxMID(A, TurPurple, 10.f)); // the bolt orb on top (root-relative: x the root scale 1.2 = ~95 cm above the centre)
		Part(TEXT("Cube"), FVector(50.f, 0.f, 79.f), FVector(1.2f, 0.28f, 0.28f), ZoogiUtil::FxMID(A, GatGold, 4.f)); // the barrel
		}
		FTurret T; T.A = A; T.P = P; T.Owner = Caster->PlayerIndex; T.Pow = GSkillPow; T.Turn = G->TurnCounter; T.Home = A->GetActorLocation(); if (TurMesh) { T.TipFwd = GTurTipFwd; T.TipUp = GTurTipUp; }
		T.ShotsLeft = ZoogiSkillLab::TurretShots(); // b225: the lab SHOTS row (10)
		T.EndTurn = G->TurnCounter + 5 * FMath::Max(1, G->Balls.Num()); // b223: it stays until its 10 shots are used (5 of the owner's turns at most)
		Turrets.Add(T);
		AZoogiFX::Spawn(W, P, GatGold, 220.f, 0.5f, true, 12.f);
		AZoogiFX::Spawn(W, P, TurPurple, 520.f, 1.2f, true, 6.f); // its reach
		AZoogiFX::Niagara(W, TEXT("NS_Burst_Gold"), P + FVector(0, 0, 80.f), FRotator::ZeroRotator, 1.4f);
		ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.8f, 0.8f);
		G->PublicPopup(FString::Printf(TEXT("TURRET UP - %d SHOTS"), T.ShotsLeft), P + FVector(0, 0, 200.f), GatGold); // b223
		UE_LOG(LogZoogi, Log, TEXT("SKILL gattling turret by P%d at (%.0f, %.0f) until turn %d"), T.Owner + 1, P.X, P.Y, T.EndTurn);
	}

	static FName GSbId; static int32 GSbP = -1; static double GSbAt = -100.0; // b205 skill banner
	bool LastCast(FName& OutId, int32& OutPlayer, double& OutAt) { OutId = GSbId; OutPlayer = GSbP; OutAt = GSbAt; return !GSbId.IsNone(); }
	void Execute(AZoogiGameMode* G, AZoogiBall* Caster, FName Id)
	{
		if (!G || !Caster) { Reset(); return; }
		if (Id != FName(TEXT("skill_lab"))) { GSbId = Id; GSbP = Caster->PlayerIndex; GSbAt = FPlatformTime::Seconds(); } // b205: the SKILL ACTIVATED banner
		ZoogiSkillLab::SetCurrent(Id);
		const bool bImpulse = Id == FName(TEXT("meteor_rain")) && ImpulseNow(); // b136: read before GAwakenNext resets
		GSkillPow = (GAwakenNext ? 1.f : CardPow()) * ZoogiSkillLab::Strength(); GAwakenNext = false; // b100; b107 x the lab tuning
		if (ElemBonusOf(Caster->PlayerIndex) == (int32)EZoogiCardElement::Fire) GSkillPow *= 1.1f; // b271 FIRE element bonus
		UE_LOG(LogZoogi, Log, TEXT("SKILL strength x%.2f (%s)"), GSkillPow, GSkillPow >= 0.99f ? TEXT("awaken power") : TEXT("card skill"));
		UWorld* W = G->GetWorld();
		const int32 Pi = Caster->PlayerIndex;
		const FPick P = S; // copy, the pick closes now
		Reset();
		const FVector Me = Caster->BallLocation(), MeFloor(Me.X, Me.Y, Me.Z - ZoogiConst::ZoogiRadius + 2.f);
		const float FZ = MeFloor.Z;
		auto At = [FZ](const FVector& V) { return FVector(V.X, V.Y, FZ); };
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		UE_LOG(LogZoogi, Log, TEXT("SKILL cast %s by P%d"), *Id.ToString(), Pi + 1);
		const int32 NPl = FMath::Max(2, G->Balls.Num());
		static const bool bOldReview = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSkillReview"));
		{	// b272 (Evan: "build everything as mentioned in the review doc"): the 9 new skills
			const FLinearColor Cy(0.5f, 0.85f, 1.f);
			auto Done = [&]() { GLastCastAt = FPlatformTime::Seconds(); };
			auto FirstRival = [&]() -> AZoogiBall* { for (const TWeakObjectPtr<AActor>& T : P.Targets) if (AZoogiBall* Rb = Cast<AZoogiBall>(T.Get())) if (Rb->PlayerIndex != Pi && !Rb->bOut) return Rb; return nullptr; };
			auto NearestRival = [&](const AZoogiBall* Not) -> AZoogiBall* { AZoogiBall* Best = nullptr; float Bd = 1e9f; for (AZoogiBall* Rb : G->Balls) if (Rb && Rb != Caster && Rb != Not && !Rb->bOut) { const float D = FVector::Dist2D(Rb->BallLocation(), Me); if (D < Bd) { Bd = D; Best = Rb; } } return Best; };
			if (Id == FName(TEXT("grapple_hook")))
			{
				AActor* T = P.Targets.Num() ? P.Targets[0].Get() : nullptr;
				const FVector Tp = T ? At(ActorPos(T)) : At(P.Ctr);
				FVector D = Tp - Me; D.Z = 0.f; const float Ds = D.Size(); D = Ds > 1.f ? D / Ds : FVector::ForwardVector;
				if (Caster->Sphere) { Caster->Sphere->WakeAllRigidBodies(); Caster->Sphere->SetPhysicsLinearVelocity(D * FMath::Clamp(900.f + Ds * 0.9f, 1200.f, 2200.f) + FVector(0, 0, 60.f)); }
				for (int32 k = 1; k <= 6; ++k) AZoogiFX::Spawn(W, FMath::Lerp(Me, Tp, k / 7.f) + FVector(0, 0, 40.f), Cy, 40.f, 0.35f, false, 10.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.4f); ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.9f, 1.f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL grapple hook P%d: yanked %.0f cm"), Pi + 1, Ds);
				Done(); return;
			}
			if (Id == FName(TEXT("edge_save")))
			{
				FNewFx F; F.Kind = NK_EdgeSave; F.B = Caster; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, FLinearColor(0.45f, 1.f, 0.55f), 140.f, 0.5f, true, 9.f);
				G->PublicPopup(TEXT("EDGE SAVE READY"), Me + FVector(0, 0, 190.f), FLinearColor(0.45f, 1.f, 0.55f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL edge save armed P%d"), Pi + 1);
				Done(); return;
			}
			if (Id == FName(TEXT("bounce_pad")))
			{
				FNewFx F; F.Kind = NK_Pad; F.P = At(P.Spots.Num() ? P.Spots[0] : P.Ctr); F.Owner = Pi; F.Until = G->TurnCounter + 2 * NPl; NewFx.Add(F);
				AZoogiFX::SpawnMark(W, F.P + FVector(0, 0, 4.f), FLinearColor(1.f, 0.7f, 0.25f), 120.f, 2.f);
				ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.6f, 1.3f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL bounce pad P%d at (%.0f, %.0f)"), Pi + 1, F.P.X, F.P.Y);
				Done(); return;
			}
			if (Id == FName(TEXT("ricochet")))
			{
				FNewFx F; F.Kind = NK_Rico; F.B = Caster; F.Owner = Pi; F.Until = G->TurnCounter; F.PrevV = Caster->GetBallVelocity(); NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, FLinearColor(1.f, 0.45f, 0.25f), 130.f, 0.4f, true, 9.f);
				G->PublicPopup(TEXT("RICOCHET: NEXT WALL x2"), Me + FVector(0, 0, 190.f), FLinearColor(1.f, 0.5f, 0.3f));
				Done(); return;
			}
			if (Id == FName(TEXT("decoy")))
			{
				FNewFx F; F.Kind = NK_Decoy; F.P = Me; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, ZoogiUtil::PlayerColor(Pi), ZoogiConst::ZoogiRadius * 1.3f, 0.6f, false, 10.f);
				G->PublicPopup(TEXT("DECOY"), Me + FVector(0, 0, 190.f), FLinearColor(0.7f, 0.5f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL decoy P%d at (%.0f, %.0f)"), Pi + 1, Me.X, Me.Y);
				Done(); return;
			}
			if (Id == FName(TEXT("tether")))
			{
				AZoogiBall* A1 = FirstRival(); AZoogiBall* A2 = nullptr;
				for (const TWeakObjectPtr<AActor>& T : P.Targets) if (AZoogiBall* Rb = Cast<AZoogiBall>(T.Get())) if (Rb != A1 && Rb->PlayerIndex != Pi && !Rb->bOut) { A2 = Rb; break; }
				if (!A1) A1 = NearestRival(nullptr);
				if (!A2) A2 = NearestRival(A1);
				if (A1 && A2)
				{
					FNewFx F; F.Kind = NK_Tether; F.B = A1; F.B2 = A2; F.Owner = Pi; F.Until = G->TurnCounter + NPl; F.Sp1 = A1->GetBallVelocity().Size2D(); F.Sp2 = A2->GetBallVelocity().Size2D(); NewFx.Add(F);
					ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.6f, 0.8f);
					UE_LOG(LogZoogi, Log, TEXT("SKILL tether P%d: P%d <-> P%d"), Pi + 1, A1->PlayerIndex + 1, A2->PlayerIndex + 1);
				}
				Done(); return;
			}
			if (Id == FName(TEXT("tide_shift")))
			{
				const FVector Ctr0 = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
				FVector D = At(P.Ctr) - FVector(Ctr0.X, Ctr0.Y, FZ); D.Z = 0.f; if (!D.Normalize()) D = (At(P.Ctr) - Me).GetSafeNormal2D();
				FNewFx F; F.Kind = NK_Tide; F.Dir = D; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(TEXT("TIDE SHIFT!"), TEXT("THE ARENA TILTS FOR A ROUND - EVERYTHING DRIFTS"), Cy, 1.8f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.7f, 1.2f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL tide shift P%d dir (%.2f, %.2f)"), Pi + 1, D.X, D.Y);
				Done(); return;
			}
			if (Id == FName(TEXT("hex")))
			{
				AZoogiBall* V = FirstRival(); if (!V) V = NearestRival(nullptr);
				if (V)
				{
					FNewFx F; F.Kind = NK_Hex; F.B = V; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
					AZoogiFX::Spawn(W, V->BallLocation(), FLinearColor(0.75f, 0.25f, 1.f), 150.f, 0.6f, true, 10.f);
					G->PublicPopup(TEXT("HEXED: NO AIM LINE, 70% POWER"), V->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.8f, 0.4f, 1.f));
					UE_LOG(LogZoogi, Log, TEXT("SKILL hex P%d on P%d"), Pi + 1, V->PlayerIndex + 1);
				}
				Done(); return;
			}
			if (Id == FName(TEXT("star_magnet")))
			{
				FNewFx F; F.Kind = NK_Magnet; F.B = Caster; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, FLinearColor(1.f, 0.85f, 0.3f), 200.f, 0.5f, true, 10.f);
				G->PublicPopup(TEXT("STAR MAGNET"), Me + FVector(0, 0, 190.f), FLinearColor(1.f, 0.85f, 0.3f));
				Done(); return;
			}
		}
		if (!bOldReview && Id == FName(TEXT("frost_spikes")))
		{	// b271 (review): FROST SPIKES = a trap - hidden in the ice where you drag it, it launches the next ball that rolls over it (2 rounds)
			FTrap Tr; Tr.P = At(P.Ctr); Tr.R = 190.f; Tr.Owner = Pi; Tr.Until = G->TurnCounter + 2 * NPl; Tr.Pow = GSkillPow; Tr.ArmAt = FPlatformTime::Seconds() + 0.6; Traps.Add(Tr);
			AZoogiFX::SpawnMark(W, Tr.P + FVector(0, 0, 4.f), FLinearColor(0.6f, 0.85f, 1.f), Tr.R, 2.f);
			ZoogiUtil::PlaySfx(G, TEXT("StingFrost"), 0.6f, 1.2f);
			G->PublicPopup(TEXT("FROST TRAP SET"), Tr.P + FVector(0, 0, 160.f), FLinearColor(0.6f, 0.85f, 1.f));
			UE_LOG(LogZoogi, Log, TEXT("SKILL frost trap P%d at (%.0f, %.0f) until turn %d"), Pi + 1, Tr.P.X, Tr.P.Y, Tr.Until);
			GLastCastAt = FPlatformTime::Seconds(); return;
		}
		if (!bOldReview && Id == FName(TEXT("cross_slash")))
		{	// b271 (review): CROSS SLASH = a counter - until your next turn, the next rival who rams you is slashed away
			Counters.RemoveAll([Caster](const FCounter& X) { return X.B.Get() == Caster; });
			FCounter Cn; Cn.B = Caster; Cn.Until = G->TurnCounter + NPl; Cn.Pow = GSkillPow; Counters.Add(Cn);
			AZoogiFX::Spawn(W, Me, FLinearColor(1.f, 0.45f, 0.15f), 160.f, 0.5f, true, 10.f);
			ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.8f, 1.2f);
			if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(TEXT("COUNTER READY!"), TEXT("THE NEXT RIVAL WHO HITS YOU GETS SLASHED"), FLinearColor(1.f, 0.45f, 0.15f), 1.6f, 1.f);
			UE_LOG(LogZoogi, Log, TEXT("SKILL cross slash counter armed P%d until turn %d"), Pi + 1, Cn.Until);
			GLastCastAt = FPlatformTime::Seconds(); return;
		}
		if (!bOldReview && Id == FName(TEXT("sword_slash")))
		{	// b271 (review: 0 rivals hit in 9 casts - the ring was too small): a SPIN DASH - 1.5 s of slashes along your path + a burst forward
			FSpin Sp; Sp.B = Caster; Sp.End = FPlatformTime::Seconds() + 1.5; Sp.Next = 0.0; Sp.Pow = GSkillPow; Sp.Pi = Pi; Spins.Add(Sp);
			AZoogiFabFX::Play(W, FX(TEXT("NS_Sword_AoE")), MeFloor, Sc(Id, 0.35f), 2.f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius + 2.f));
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Sword_air_attack")), Me, 1.f);
			if (G->Phase == EZoogiPhase::Rolling && Caster->Sphere) Caster->Sphere->SetPhysicsLinearVelocity(Caster->GetBallVelocity() * 1.35f);
			UE_LOG(LogZoogi, Log, TEXT("SKILL sword slash spin dash P%d (1.5 s)"), Pi + 1);
			GLastCastAt = FPlatformTime::Seconds(); return;
		}
		if (ExecPack(G, Caster, Id, P, FZ))
		{
			GLastCastAt = FPlatformTime::Seconds(); // b176a
			if (!bOldReview && Id == FName(TEXT("ice_shards")))
			{	// b271 (review): the shards leave slippery ice patches for 2 rounds - orbs slide further, toward the edge
				for (const FVector& Sp : P.Spots) { FPatch Pt; Pt.P = At(Sp); Pt.R = 240.f; Pt.Owner = Pi; Pt.Until = G->TurnCounter + 2 * NPl; Patches.Add(Pt); AZoogiFX::SpawnMark(W, Pt.P + FVector(0, 0, 3.f), FLinearColor(0.7f, 0.92f, 1.f), Pt.R, 3.f); }
				UE_LOG(LogZoogi, Log, TEXT("SKILL ice shards: %d slippery patches"), P.Spots.Num());
			}
			if (!bOldReview && Id == FName(TEXT("pillar_field")))
				for (const FVector& Sp : P.Spots) { FNewFx F; F.Kind = NK_Pad; F.P = At(Sp); F.Owner = Pi; F.Until = G->TurnCounter + 2 * NPl; F.NextFx = 1e18; F.Sp2 = 185.f; NewFx.Add(F); } // b273: PINBALL pillars - balls bounce off them 40% faster
			if (!bOldReview && Id == FName(TEXT("fault_line")))
			{	// b273: the outer side of the crack tips toward the edge for 2.5 s
				const FVector A0 = At(P.A), B0 = At(P.B), Mid = (A0 + B0) * 0.5f; FVector Dl = (B0 - A0).GetSafeNormal2D(); if (Dl.IsNearlyZero()) Dl = FVector::ForwardVector;
				FVector N(-Dl.Y, Dl.X, 0.f); const FVector C0 = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector; if (FVector::DotProduct(Mid - C0, N) < 0.f) N = -N;
				FNewFx F; F.Kind = NK_Fault; F.P = Mid; F.Dir = N; F.Owner = Pi; F.Until = G->TurnCounter + NPl; F.NextFx = FPlatformTime::Seconds() + 2.5; NewFx.Add(F);
				UE_LOG(LogZoogi, Log, TEXT("SKILL fault line P%d: the outer side tips toward the edge"), Pi + 1);
			}
			if (!bOldReview && Id == FName(TEXT("sapphire_wall"))) { FPlusWall Pw; Pw.A = At(P.A); Pw.B = At(P.B); Pw.Owner = Pi; Pw.Until = G->TurnCounter + 2 * NPl; PlusWalls.Add(Pw); }
			if (!bOldReview && Id == FName(TEXT("glacier_lance")) && P.Targets.Num())
				if (AActor* T0 = P.Targets[0].Get())
				{	// b271 (review: good KO rate, bad on orbs): the lance PIERCES - two more spears carry on behind the target
					const FVector Tp = At(T0->GetActorLocation()); FVector Dl = Tp - Me; Dl.Z = 0.f; Dl = Dl.GetSafeNormal(); if (Dl.IsNearlyZero()) Dl = FVector::ForwardVector;
					const float Pw0 = GSkillPow;
					for (int32 k = 1; k <= 2; ++k)
					{
						const FVector Hk = Tp + Dl * 230.f * k;
						Later(G, 0.25f + 0.15f * k, [WG, Hk, Dl, Pi, Pw0]()
						{
							AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
							FTuneScope Tn(Pw0, FName(TEXT("glacier_lance")));
							AZoogiFX::Spawn(Gm->GetWorld(), Hk + FVector(0, 0, 6.f), FLinearColor(0.7f, 0.92f, 1.f), 170.f, 0.4f, true, 10.f);
							const int32 Np = Blast(Gm, Hk - Dl * 90.f, 170.f, 1300.f, 700.f, nullptr, Pi);
							ZoogiUtil::PlaySfx(Gm, TEXT("RockShatter"), 0.6f, 1.3f);
							UE_LOG(LogZoogi, Log, TEXT("SKILL glacier lance pierce: %d pushed"), Np);
						});
					}
				}
			if (!bOldReview && Id == FName(TEXT("crescent_slash")))
			{	// b271 (review: the best rival hunter): rivals in the slashes' path take an extra shove (orbs do not)
				const float Pw0 = GSkillPow;
				for (const TWeakObjectPtr<AActor>& Tw : P.Targets)
					if (AZoogiBall* Rb = Cast<AZoogiBall>(Tw.Get()))
						if (Rb->PlayerIndex != Pi && Rb->Sphere)
						{
							TWeakObjectPtr<AZoogiBall> WR = Rb;
							Later(G, 0.35f, [WR, Me, Pw0]() { AZoogiBall* R = WR.Get(); if (!R || R->bOut || !R->Sphere) return; FVector D = R->BallLocation() - Me; D.Z = 0.f; D = D.GetSafeNormal(); R->Sphere->WakeAllRigidBodies(); R->Sphere->SetPhysicsLinearVelocity(R->GetBallVelocity() + (D * 520.f + FVector(0, 0, 120.f)) * Pw0); });
						}
			}
			return;
		}
		GLastCastAt = FPlatformTime::Seconds(); // b76: the menu stays hidden while the effect plays out
		UZoogiAnimeSubsystem* AnS = W ? W->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr;
		static const bool bWolfRedirects = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfSkillRedirects"));
		if (Id == FName(TEXT("wolf_pack")) && !bWolfRedirects)
		{
			// b77 (Evan): the skill arms the WOLF button (in CONTROL's old slot); tapping it during the roll summons the pack
			// b88 (Evan: "increase Wolf Pack SP to 3 but you get three"): one cast banks 3 wolves (-ZoogiWolfOne = 1)
			static const bool bOneWolf = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOne"));
			// b96d (Evan: "Max level is 3 starting from lvl 1 with one wolf improved by using it"): Lv1 = 1 wolf, Lv2 = 2, Lv3 = 3;
			// each cast levels it up (per player, per match). -ZoogiWolfNoLevels = 3 every cast.
			static const bool bNoLv = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfNoLevels"));
			static TWeakObjectPtr<const AZoogiGameMode> LvG; static TMap<int32, int32> WolfLv; static int32 LvTurn = 0;
			if (LvG.Get() != G || G->TurnCounter < LvTurn) { WolfLv.Reset(); LvG = G; }
			LvTurn = G->TurnCounter;
			int32& Lv = WolfLv.FindOrAdd(Pi); Lv = FMath::Clamp(Lv, 1, 3);
			const int32 Give = bOneWolf ? 1 : (bNoLv ? 3 : Lv);
			for (int32 k = 0; k < Give; ++k) ArmWolf(G, Pi);
			if (UZoogiWolfPackSys* WPf = UZoogiWolfPackSys::Get(G)) WPf->SummonField(G, Caster, WolfCount(G, Pi)); // b119c (Evan): the wolves come onto the field now and stay with the player
			const FString LvTitle = bNoLv ? FString(TEXT("3 WOLVES READY!")) : FString::Printf(TEXT("WOLF PACK LV%d: %d WOLF%s READY!"), Lv, Give, Give == 1 ? TEXT("") : TEXT("VES"));
			const FString LvSub = Caster->bCPU ? FString(TEXT("THE PACK IS COMING")) : (Lv < 3 && !bNoLv ? FString::Printf(TEXT("LAUNCH, THEN TAP SUPERPOWER - NEXT CAST LV%d"), Lv + 1) : FString(TEXT("LAUNCH, THEN TAP SUPERPOWER - MAX LEVEL")));
			UE_LOG(LogZoogi, Log, TEXT("WOLF PACK level %d for P%d: %d wolf(s) banked"), Lv, Pi + 1, Give);
			if (!bNoLv) Lv = FMath::Min(3, Lv + 1);
			AZoogiFX::Spawn(W, MeFloor, FLinearColor(0.75f, 0.45f, 1.f), 300.f, 0.6f, true, 8.f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 0.8f);
			// b116 (Evan: "the Energy Buff animation on the player too, like the skill does, when Wolf Pack activates"): the same
			// effect, size and sound as casting ENERGY BUFF (visual only)
			if (AZoogiFabFX* Ef = AZoogiFabFX::Play(W, FX(TEXT("NS_Energy_buff")), MeFloor, Sc(FName(TEXT("energy_buff")), 0.18f), 5.f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius + 2.f))) Ef->StopAfter = 1.2f; // b119: the loop plays once
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Energy_buff")), Me, 1.f);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(LvTitle, LvSub, FLinearColor(0.75f, 0.45f, 1.f), 1.8f, 1.f);
		}
		else if (Id == FName(TEXT("wolf_pack")))
		{
			// the Wolf Pack intro plays out in full (first time per match), then 3 free REACTION redirects this turn
			// b70 (Evan): the game stops under one clip of the intro, then the effect
			auto Fx = [WG, Pi, MeFloor]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				ZoogiRedirect::GrantFree(Pi, 3);
				AZoogiFX::Spawn(Gm->GetWorld(), MeFloor, FLinearColor(0.75f, 0.45f, 1.f), 420.f, 0.8f, true, 10.f);
				ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 0.8f, 0.6f);
				if (UZoogiFunSys* F = UZoogiFunSys::Get(Gm)) F->PublicCallout(TEXT("WOLF PACK!"), TEXT("+3 FREE REDIRECTS THIS TURN - TAP REACTION WHILE ROLLING"), FLinearColor(0.75f, 0.45f, 1.f), 2.f, 1.f);
			};
			if (AnS && AnS->PlayPowerIntro(TEXT("WolfPack"), Pi, FLinearColor(0.75f, 0.45f, 1.f))) AnS->AfterCrash(Fx); else Fx();
		}
		else if (Id == FName(TEXT("bolt_balls")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoltBalls")))
		{
			// b80: 3 bolt shots - the intro clip, then the lightning stems appear round the Zoogi
			TWeakObjectPtr<AZoogiBall> WCasterB = Caster;
			auto Fx = [WG, Pi, WCasterB]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				ArmBolts(Gm, Pi, 3);
				if (AZoogiBall* Cb = WCasterB.Get()) AZoogiFX::Stylized(Gm->GetWorld(), TEXT("ElectricShock"), Cb->BallLocation(), 1.2f); // b119b: one yellow flash on the cast
				ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 1.f, 1.3f);
				if (UZoogiFunSys* F = UZoogiFunSys::Get(Gm)) F->PublicCallout(TEXT("BOLT BALLS x3!"), TEXT("TAP SUPERPOWER - AIMING OR ROLLING - ONE BOLT PER TAP"), FLinearColor(0.6f, 0.55f, 1.f), 2.f, 1.f);
			};
			if (AnS && AnS->PlayPowerIntro(TEXT("BoltBalls"), Pi, FLinearColor(0.6f, 0.55f, 1.f))) AnS->AfterCrash(Fx); else Fx();
		}
		else if (Id == FName(TEXT("bolt_balls")))
		{
			// the old classic StaticShock electric effect (VFX_PackVol1 P_ElectricShock) + Grok's Bolt Balls intro
			TWeakObjectPtr<AZoogiBall> WCaster = Caster;
			auto Fx = [WG, WCaster, Pi]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				ZoogiRedirect::ArmBolt(Pi);
				if (AZoogiBall* Cb = WCaster.Get()) AZoogiFX::Stylized(Gm->GetWorld(), TEXT("ElectricShock"), Cb->BallLocation(), 1.4f);
				ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 1.f, 1.3f);
				if (UZoogiFunSys* F = UZoogiFunSys::Get(Gm)) F->PublicCallout(TEXT("BOLT BALLS!"), TEXT("THE LAUNCH AND EVERY REDIRECT SHOCK THE 3 NEAREST"), FLinearColor(1.f, 0.9f, 0.3f), 2.f, 1.f);
			};
			if (AnS && AnS->PlayPowerIntro(TEXT("BoltBalls"), Pi, FLinearColor(1.f, 0.9f, 0.3f))) AnS->AfterCrash(Fx); else Fx(); // b70: clip first, then the effect
		}
		else if (bImpulse)
		{	// b136 (Evan): IMPULSE - one wolf-pack blast from where the Zoogi is right now, no aiming, no slow-down; its anime always plays
			const FVector Bp = Caster->BallLocation();
			const float SpK = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSkillReview")) ? 1.f : FMath::Clamp(1.f + Caster->GetBallVelocity().Size2D() / 1500.f, 1.f, 2.f); // b271 (review): faster roll = bigger blast
			WolfBlast(G, FVector(Bp.X, Bp.Y, FZ + 4.f), Pi, SpK / FMath::Max(0.1f, CardPow())); // one wolf at lab strength 1 (WolfBlast applies GSkillPow)
			AZoogiFX::Spawn(W, Bp, FLinearColor(0.7f, 0.85f, 1.f), 260.f, 0.35f, false, 16.f);
			AZoogiFX::Shockwave(W, FVector(Bp.X, Bp.Y, FZ + 4.f), EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, nullptr, false);
			ZoogiHaptics::Kick(0.6f, W, TEXT("impulse"));
			if (AnS && !AnS->PlayPowerIntro(TEXT("Impulse"), Pi, FLinearColor(0.7f, 0.85f, 1.f))) AnS->PlayPowerIntro(TEXT("WolfPack"), Pi, FLinearColor(0.7f, 0.85f, 1.f)); // mandatory anime (placeholder: the Wolf Pack clip)
			UE_LOG(LogZoogi, Log, TEXT("SKILL impulse: P%d wolf blast at (%.0f, %.0f) + anime"), Pi + 1, Bp.X, Bp.Y);
		}
		else if (Id == FName(TEXT("meteor_rain")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeteorRain")))
		{	// b90f (Evan: "Meteor Rain becomes one big strike zooming fast down like a missile, a sonic wave, a blast equal to a Wolf Pack blast")
			// b91b (Evan: "replace Meteor Rain with this effect" - KriptoFX Realistic Effects 4 portal meteor): a dark purple portal tears open
			// in the air, a boulder wreathed in purple lightning drops out of it onto the target, the ground shatters into rock shards,
			// a Wolf Pack blast. (KriptoFX is a Unity pack - rebuilt here from engine shapes + the Inferno rocks.) -ZoogiOldMeteorRain = 7 meteors.
			const FVector Hit = At(P.Ctr);
			FVector Dir = Hit - Me; Dir.Z = 0.f; if (!Dir.Normalize()) Dir = FVector::ForwardVector;
			const FVector Portal = Hit - Dir * 1100.f + FVector(0.f, 0.f, 1500.f); // b95b (Evan): much higher and further away - the meteor cam follows it
			const FLinearColor Pu(0.6f, 0.3f, 1.f);
			struct FVoid { TWeakObjectPtr<AActor> Disc, Ring, Rock, Arcs, Cam, PrevView; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f, Spark = 0.f, Arc = 0.f, DiscMul = 1.f; bool bHit = false, bCamBack = false; FVector Side = FVector::RightVector, Fwd = FVector::ForwardVector; };
			TSharedRef<FVoid> V = MakeShared<FVoid>();
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FRotator Face = FRotationMatrix::MakeFromZ((Hit - Portal).GetSafeNormal()).Rotator(); // the portal faces its target
			auto Shape = [&](const TCHAR* Mesh, const FLinearColor& C, float Glow, const FVector& L, const FRotator& R) -> AStaticMeshActor*
			{
				AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(R, L), SP);
				if (!A) return nullptr;
				A->SetMobility(EComponentMobility::Movable);
				UStaticMeshComponent* Mc = A->GetStaticMeshComponent();
				Mc->SetStaticMesh(ZoogiUtil::BasicShape(Mesh)); Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mc->SetCastShadow(false);
				Mc->SetMaterial(0, ZoogiUtil::FxMID(A, C, Glow));
				A->SetActorScale3D(FVector(0.01f));
				return A;
			};
			// b97a: the real KriptoFX Effect5 prefab, its rock lane laid from the portal onto the target
			static const bool bNoKfx5 = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiMeteorKFX")); // b97: opt-in until the camera / scale is fixed (the first capture filled the screen)
			float Kfx5Scale = 0.f;
			if (!bNoKfx5)
				if (UNiagaraSystem* Sys5 = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/NS_KFX_Effect5.NS_KFX_Effect5")))
				{
					const FVector D5 = Hit - Portal;
					Kfx5Scale = FMath::Clamp((float)D5.Size() / 535.f, 1.2f, 4.5f);
					const float ElevA = FMath::RadiansToDegrees(FMath::Atan2(-190.f, 500.f)), ElevD = FMath::RadiansToDegrees(FMath::Atan2((float)D5.Z, (float)D5.Size2D()));
					const FRotator R5(ElevA - ElevD, Dir.Rotation().Yaw + 180.f, 0.f);
					const FVector O5 = Portal - R5.RotateVector(FVector(0.f, 0.f, 190.f * Kfx5Scale));
					if (UNiagaraComponent* N5 = UNiagaraFunctionLibrary::SpawnSystemAtLocation(W, Sys5, O5, R5, FVector(Kfx5Scale), true, true, ENCPoolMethod::None))
					{
						TWeakObjectPtr<UNiagaraComponent> WN(N5); FTimerHandle H1, H2;
						W->GetTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([WN]() { if (WN.IsValid()) WN->Deactivate(); }), 3.2f, false);
						W->GetTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([WN]() { if (WN.IsValid()) WN->DestroyComponent(); }), 7.f, false);
					}
					if (AZoogiKFXExtras* X5 = AZoogiKFXExtras::Spawn(W, TEXT("Effect5"), O5, Kfx5Scale, 3.f, nullptr)) X5->SetActorRotation(R5);
					if (USoundBase* So = LoadObject<USoundBase>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_PortalOut.S_PortalOut"))) UGameplayStatics::PlaySoundAtLocation(W, So, Portal, 1.f);
					UE_LOG(LogZoogi, Log, TEXT("SKILL meteor_rain: KriptoFX Effect5 at scale %.2f, pitch %.1f"), Kfx5Scale, R5.Pitch);
				}
			V->Disc = Shape(TEXT("Cylinder"), FLinearColor(0.08f, 0.02f, 0.18f), 1.f, Portal, Face);
			if (AStaticMeshActor* Dk = Cast<AStaticMeshActor>(V->Disc.Get())) Dk->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::SurfaceMID(Dk, FLinearColor(0.02f, 0.f, 0.05f), 0.9f, FLinearColor(0.25f, 0.05f, 0.5f), 0.6f)); // b95d: an opaque black-violet void (the glowing ring shows only round its rim)
			V->Ring = Shape(TEXT("Cylinder"), Pu, 6.f, Portal - (Hit - Portal).GetSafeNormal() * 4.f, Face);
			if (Kfx5Scale > 0.f) { if (AActor* Rg = V->Ring.Get()) Rg->SetActorHiddenInGame(true); V->DiscMul = 1.54f * Kfx5Scale / 3.6f; } // the KFX swirl disc is the rim now
			if (AStaticMeshActor* Rk = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Portal), SP))
			{
				Rk->SetMobility(EComponentMobility::Movable);
				UStaticMeshComponent* Mc = Rk->GetStaticMeshComponent();
				UStaticMesh* RockM = Kfx5Scale > 0.f ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/Meshes/SM_Rock.SM_Rock")) : nullptr; // b97a: the demo's rock
				if (!RockM) RockM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockBig_001.SM_RockBig_001"));
				Mc->SetStaticMesh(RockM ? RockM : ZoogiUtil::BasicShape(TEXT("Sphere")));
				if (Kfx5Scale > 0.f) if (UMaterialInterface* RkMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/VFX/KriptoFX/Effect5/Materials/MI_KFX_Effect5_MeshRock2mat.MI_KFX_Effect5_MeshRock2mat"))) for (int32 m = 0; m < FMath::Max(1, Mc->GetNumMaterials()); ++m) Mc->SetMaterial(m, RkMat);
				Mc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				const float Ext = RockM ? FMath::Max(1.f, (float)RockM->GetBounds().SphereRadius) : 50.f;
				Rk->SetActorScale3D(FVector(110.f / Ext));
				Rk->SetActorHiddenInGame(true);
				V->Rock = Rk;
			}
			if (AActor* Ar = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Portal), SP))
			{	// purple lightning around the boulder (and over the crater after the hit)
				USceneComponent* Root = NewObject<USceneComponent>(Ar, TEXT("Root")); Root->SetMobility(EComponentMobility::Movable); Ar->SetRootComponent(Root); Root->RegisterComponent();
				UMaterialInterface* Core = ZoogiUtil::FxMID(Ar, FLinearColor(0.9f, 0.75f, 1.f), 40.f);
				UMaterialInterface* Glo = ZoogiUtil::FxMID(Ar, Pu, 18.f);
				for (int32 i = 0; i < 4 * 7 * 2; ++i)
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Ar);
					C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder"))); C->SetMobility(EComponentMobility::Movable);
					C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
					C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
					C->SetupAttachment(Root); C->RegisterComponent(); C->SetMaterial(0, (i % 2) ? Glo : Core); C->SetVisibility(false);
					V->Segs.Add(C);
				}
				V->Arcs = Ar;
			}
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), Portal, 0.8f);
			AZoogiFX::Spawn(W, Hit + FVector(0, 0, 4.f), Pu, 300.f, 0.9f, true, 8.f); // target ring
			// b95b: the meteor cam - beside the target looking up at the portal, then down the boulder's dive (-ZoogiNoMeteorCam = none)
			V->Fwd = Dir; V->Side = FVector::CrossProduct(FVector::UpVector, Dir);
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoMeteorCam")))
				if (APlayerController* PCm = W->GetFirstPlayerController())
					if (ACameraActor* Cm = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Hit + Dir * 900.f + V->Side * 650.f + FVector(0, 0, 380.f)), SP))
					{
						Cm->GetCameraComponent()->SetFieldOfView(80.f);
						Cm->GetCameraComponent()->bConstrainAspectRatio = false;
						Cm->SetActorRotation((Portal - Cm->GetActorLocation()).Rotation());
						V->PrevView = PCm->GetViewTarget(); V->Cam = Cm;
						PCm->SetViewTargetWithBlend(Cm, 0.35f, VTBlend_Cubic);
					}
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, V, Portal, Hit, Pi, Pu](float Dt) -> bool
			{
				AZoogiGameMode* Gm = WG.Get();
				auto Kill = [&V]() { for (auto* Wp : { &V->Disc, &V->Ring, &V->Rock, &V->Arcs }) if (AActor* A = Wp->Get()) A->Destroy(); };
				if (!Gm) { Kill(); return false; }
				UWorld* Wm = Gm->GetWorld();
				V->T += FMath::Min(Dt, 0.05f);
				const float T = V->T;
				// portal: opens 0-0.35 s, holds, closes 1.3-1.6 s
				const float Open = T < 0.4f ? T / 0.4f : (T > 1.9f ? FMath::Max(0.f, 1.f - (T - 1.9f) / 0.35f) : 1.f);
				const float Wob = 1.f + 0.06f * FMath::Sin(T * 23.f);
				if (AActor* D = V->Disc.Get()) D->SetActorScale3D(FVector(3.6f * V->DiscMul * Open * Wob, 3.6f * V->DiscMul * Open / Wob, 0.03f));
				if (AActor* R = V->Ring.Get()) { R->SetActorScale3D(FVector(4.2f * Open, 4.2f * Open, 0.015f)); R->AddActorLocalRotation(FRotator(0.f, 400.f * Dt, 0.f)); }
				V->Spark += Dt;
				if (Open > 0.05f && V->Spark > 0.04f) { V->Spark = 0.f; const float A = FMath::FRandRange(0.f, 2.f * PI); AZoogiFX::Spawn(Wm, Portal + FVector(FMath::Cos(A) * 180.f * Open, FMath::Sin(A) * 180.f * Open, FMath::FRandRange(-60.f, 60.f)), Pu, 40.f, 0.4f, false, 12.f); }
				// the boulder: out of the portal at 0.6 s, slams down at 1.35 s (b95b: the longer drop)
				const float Kf = FMath::Clamp((T - 0.6f) / 0.75f, 0.f, 1.f);
				FVector RockAt = FMath::Lerp(Portal, Hit + FVector(0.f, 0.f, 35.f), Kf * Kf);
				if (AActor* Rk = V->Rock.Get())
				{
					Rk->SetActorHiddenInGame(T < 0.6f);
					if (!V->bHit) { Rk->SetActorLocation(RockAt); Rk->AddActorWorldRotation(FRotator(260.f * Dt, 190.f * Dt, 0.f)); }
					else { RockAt = Rk->GetActorLocation(); Rk->SetActorLocation(RockAt - FVector(0.f, 0.f, 30.f * Dt)); } // settles into the crater
				}
				// lightning: around the falling boulder, then crawling over the crater
				V->Arc += Dt;
				const bool bArcs = T > 0.6f && T < 2.5f;
				if (V->Arc >= 0.05f)
				{
					V->Arc = 0.f;
					const FVector Src = V->bHit ? Hit + FVector(0, 0, 40.f) : RockAt;
					for (int32 k = 0; k < 4; ++k)
					{
						const float A = FMath::FRandRange(0.f, 2.f * PI);
						const FVector End = V->bHit ? Hit + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * FMath::FRandRange(160.f, 320.f) : Src + FVector(FMath::Cos(A) * 170.f, FMath::Sin(A) * 170.f, FMath::FRandRange(-120.f, 160.f));
						FVector Prev = Src;
						for (int32 i = 1; i <= 7; ++i)
						{
							FVector Pt = FMath::Lerp(Src, End, i / 7.f);
							if (i < 7) Pt += FVector(FMath::FRandRange(-22.f, 22.f), FMath::FRandRange(-22.f, 22.f), FMath::FRandRange(-18.f, 18.f));
							for (int32 Lr = 0; Lr < 2; ++Lr)
								if (UStaticMeshComponent* C = V->Segs.IsValidIndex(((k * 7) + i - 1) * 2 + Lr) ? V->Segs[((k * 7) + i - 1) * 2 + Lr].Get() : nullptr)
								{
									const FVector Dv = Pt - Prev; const float Ln = Dv.Size();
									if (!bArcs || Ln < 1.f) { C->SetVisibility(false); continue; }
									C->SetVisibility(true);
									C->SetWorldLocationAndRotation((Pt + Prev) * 0.5f, FRotationMatrix::MakeFromZ(Dv / Ln).Rotator());
									const float Th = Lr ? 9.f : 3.f;
									C->SetWorldScale3D(FVector(Th / 100.f, Th / 100.f, Ln / 100.f));
								}
							Prev = Pt;
						}
					}
				}
				if (!V->bHit && Kf >= 1.f)
				{
					V->bHit = true;
					// impact: the ground shatters, rock shards fly, a purple blast (a Wolf Pack blast)
					for (int32 s = 0; s < 9; ++s)
					{
						static const TCHAR* Sh[] = { TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockSmall_001.SM_RockSmall_001"), TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockMid_002.SM_RockMid_002") };
						UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Sh[s % 2]);
						if (!M) continue;
						const float A = 2.f * PI * s / 9.f + FMath::FRandRange(-0.3f, 0.3f);
						const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
						FActorSpawnParameters Sp2; Sp2.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						if (AStaticMeshActor* Sd = Wm->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Hit + Out * 90.f + FVector(0, 0, 30.f)), Sp2))
						{
							Sd->SetMobility(EComponentMobility::Movable);
							UStaticMeshComponent* Mc = Sd->GetStaticMeshComponent();
							Mc->SetStaticMesh(M);
							Sd->SetActorScale3D(FVector(FMath::FRandRange(28.f, 46.f) / FMath::Max(1.f, (float)M->GetBounds().SphereRadius)));
							Mc->SetCollisionProfileName(TEXT("PhysicsActor"));
							Mc->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
							Mc->SetSimulatePhysics(true);
							Mc->SetPhysicsLinearVelocity(Out * FMath::FRandRange(500.f, 900.f) + FVector(0, 0, FMath::FRandRange(500.f, 850.f)));
							Mc->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f), 0.f));
							Sd->SetLifeSpan(2.6f);
						}
					}
					AZoogiFX::Stylized(Wm, SkillBlastFx(), Hit, 0.6f * SkillBlastSc());
					AZoogiFX::Spawn(Wm, Hit + FVector(0, 0, 6.f), Pu, 520.f, 0.7f, true, 12.f);
					UZoogiScreenFx::Shockwave(Wm, Hit, 800.f, Pu);
					ZoogiUtil::PlaySfx(Gm, TEXT("RockCrack"), 1.f, 0.7f); ZoogiUtil::PlaySfx(Gm, TEXT("RockCrack"), 0.8f, 0.5f); // stone crush
					ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 0.9f, 0.6f);
					for (const TCHAR* Sp5 : { TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_RockExplosion.S_RockExplosion"), TEXT("/Game/VFX/KriptoFX/Effect5/Audio/S_RockImpact.S_RockImpact") }) // b97a: the demo's impact
						if (USoundBase* So = LoadObject<USoundBase>(nullptr, Sp5)) UGameplayStatics::PlaySoundAtLocation(Wm, So, Hit, 1.f);
					WolfBlast(Gm, Hit, Pi, 1.f);
					ZoogiEpic::Hit(Gm, ZoogiEpic::EHit::Big); // b103
					if (AActor* Rk = V->Rock.Get()) Rk->SetLifeSpan(1.4f);
					UE_LOG(LogZoogi, Log, TEXT("SKILL meteor_rain: the void meteor strikes (%.0f, %.0f)"), Hit.X, Hit.Y);
				}
				// the meteor cam: eyes on the portal, then on the boulder, then the crater; back to the match camera at 2.4 s
				if (AActor* Cm = V->Cam.Get())
				{
					const FVector Want = V->bHit ? Hit + V->Fwd * 620.f + V->Side * 420.f + FVector(0, 0, 260.f) : FMath::Lerp(Hit + V->Fwd * 900.f + V->Side * 650.f + FVector(0, 0, 380.f), Hit + V->Fwd * 700.f + V->Side * 480.f + FVector(0, 0, 300.f), Kf);
					Cm->SetActorLocation(FMath::VInterpTo(Cm->GetActorLocation(), Want, Dt, 6.f));
					const FVector Look = V->bHit ? Hit + FVector(0, 0, 60.f) : (T < 0.6f ? Portal : RockAt);
					Cm->SetActorRotation(FMath::RInterpTo(Cm->GetActorRotation(), (Look - Cm->GetActorLocation()).Rotation(), Dt, V->bHit ? 5.f : 9.f));
					if (!V->bCamBack && T > 2.4f)
					{
						V->bCamBack = true;
						if (APlayerController* PCm = Wm->GetFirstPlayerController()) if (PCm->GetViewTarget() == Cm) { if (AActor* Pv = V->PrevView.Get()) PCm->SetViewTargetWithBlend(Pv, 0.45f, VTBlend_Cubic); }
					}
				}
				if (T < 2.95f) return true;
				if (AActor* Cm = V->Cam.Get()) { if (APlayerController* PCm = Wm->GetFirstPlayerController()) if (PCm->GetViewTarget() == Cm) { if (AActor* Pv = V->PrevView.Get()) PCm->SetViewTarget(Pv); } Cm->Destroy(); }
				Kill();
				return false;
			}));
			if (UZoogiFunSys* Fm = UZoogiFunSys::Get(G)) Fm->PublicCallout(TEXT("VOID METEOR!"), TEXT("THE PORTAL OPENS"), FLinearColor(0.65f, 0.35f, 1.f), 1.2f, 1.f);
		}
		else if (Id == FName(TEXT("meteor_rain")))
		{
			// b66 (Evan): the Meteor Strike clip belongs to this skill; b70: the game stops under one clip of it, then the meteors fall
			const FPick PM = P;
			auto Fx = [WG, Pi, PM, Id, At]()
			{
				AZoogiGameMode* MG = WG.Get(); if (!MG) return;
				UWorld* MW = MG->GetWorld();
				
			const int32 V = 1 + (Pi * 3 + FMath::RandRange(0, 2)) % 12;
			AZoogiFabFX::Play(MW, *FString::Printf(TEXT("/Game/MeteorRain/Particles/NiagaraSystems/NS_MeteorRainV%02d.NS_MeteorRainV%02d"), V, V), At(PM.Ctr), Sc(Id, 0.25f), 4.5f); // ~2060 cm footprint at scale 1
			AZoogiFabFX::Sound(MW, SFX(TEXT("S_Fire_meteors")), At(PM.Ctr), 0.8f);
			for (int32 k = 0; k < 7; ++k)
			{
				const float A = FMath::FRandRange(0.f, 2.f * PI), Rr = FMath::Sqrt(FMath::FRand()) * PM.R * 0.85f;
				const FVector Hit = At(PM.Ctr) + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 0.f);
				Later(MG, 0.55f + 0.2f * k, [WG, Hit, Pi]() { if (AZoogiGameMode* Gm = WG.Get()) { Blast(Gm, Hit, 210.f, 520.f, 220.f, nullptr, Pi); AZoogiFX::Spawn(Gm->GetWorld(), Hit, FLinearColor(1.f, 0.5f, 0.15f), 180.f, 0.35f, true, 8.f); } });
			}
			};
			if (AnS && AnS->PlayPowerIntro(TEXT("MeteorStrike"), Pi, FLinearColor(1.f, 0.5f, 0.2f))) AnS->AfterCrash(Fx); else Fx();
		}
		else if (Id == FName(TEXT("fire_meteors")))
		{
			AZoogiFabFX::Play(W, FX(TEXT("NS_Fire_meteors")), At(P.Ctr), Sc(Id, 0.3f), 3.5f);
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), At(P.Ctr), 1.f);
			for (int32 k = 0; k < 3; ++k)
			{
				const float A = 2.f * PI * k / 3.f + 0.4f;
				const FVector Hit = At(P.Ctr) + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * P.R * 0.45f;
				Later(G, 0.6f + 0.22f * k, [WG, Hit, Pi, k]() { if (AZoogiGameMode* Gm = WG.Get()) { Blast(Gm, Hit, 260.f, 820.f, 300.f, nullptr, Pi); AZoogiFabFX::Play(Gm->GetWorld(), FX(k % 2 ? TEXT("NS_Meteor_hit") : TEXT("NS_Meteor_hit_2")), Hit, Sc(FName(TEXT("meteor_hit")), 0.22f), 2.5f); } });
			}
		}
		else if (Id == FName(TEXT("ice_wall")))
		{
			// b63 (Evan): a solid ice wall - blocks rolls, cracks on hits, bursts into ice blocks (AZoogiIceBarrier)
			// b124 (Evan: "ice walls built from players stay to the end of the match unless destroyed after three hits"): no round limit
			static const bool bOldLife = FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallOneRound"));
			AZoogiIceBarrier::Spawn(W, At(P.A), At(P.B), 220.f * ZoogiSkillLab::Radius(), Pi, bOldLife ? 1 : 0);
			{ FPlusWall Pw; Pw.A = At(P.A); Pw.B = At(P.B); Pw.Owner = Pi; Pw.Until = G->TurnCounter + 6 * FMath::Max(2, G->Balls.Num()); PlusWalls.Add(Pw); } // b271 wall payoff // b136: lab RADIUS = wall height // b71 (Grok): taller spires like the Fab demo (was 150)
		}
		else if (Id == FName(TEXT("water_cannon")))
		{
			AActor* T = P.Targets.Num() ? P.Targets[0].Get() : nullptr;
			if (T)
			{
				const FVector Tp = ActorPos(T); const FVector Dir = (Tp - Me).GetSafeNormal2D();
				AZoogiFabFX* WcFx = AZoogiFabFX::Play(W, FX(TEXT("NS_Water_cannon")), Me + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(Id, 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation() + FRotator(0.f, WcYaw(), 0.f)); // b142c: its water ball along the cast
				WcProbe(W, WcFx, Me + FVector(0, 0, ZoogiConst::ZoogiRadius), Dir);
				AZoogiFabFX::Sound(W, SFX(TEXT("S_Water_cannon")), Me, 1.f);
				TWeakObjectPtr<AActor> WT = T;
				const float WcAt = WcHitT() + FVector::Dist2D(Tp, Me) / 5000.f; // b142h: when the water ball gets there
				UE_LOG(LogZoogi, Log, TEXT("SKILL water_cannon: yaw offset %.0f, target %.0f cm away, hit at %.2f s"), WcYaw(), FVector::Dist2D(Tp, Me), WcAt);
				Later(G, WcAt, [WG, WT, Dir, Pi]()
				{
					AZoogiGameMode* Gm = WG.Get(); AActor* Tt = WT.Get(); if (!Gm || !Tt) return;
					AZoogiFabFX::Play(Gm->GetWorld(), FX(TEXT("NS_Water_hit")), ActorPos(Tt) - FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_hit")), 0.4f), 3.f); // b71 (Grok / Evan): from the target's floor, longer (size: FxTune)
					LabImpact(Gm->GetWorld(), ActorPos(Tt)); // b139b
					AZoogiFabFX::Sound(Gm->GetWorld(), SFX(TEXT("S_Water_cannon_hit")), ActorPos(Tt), 1.f);
					// b87 (Evan: "Water Cannon force increase to equal the launch button"): the LAUNCH speed (full pull); -ZoogiOldWaterCannon = 900 / 820
					static const bool bOldWc = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWaterCannon"));
					// b105 (Evan: "bring back Water Cannon how it was in build 87"): full LAUNCH force again and not toned down by the b100
					// 60% skill strength. -ZoogiWaterCannonDrive = the b89-b104 DRIVE force (and the 60%)
					static const bool bDriveWc = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWaterCannonDrive"));
					const float Wf = bDriveWc ? ZoogiComboTune::DriveFrac : 1.f;
					const float Vb = bOldWc ? 900.f : ZoogiConst::LaunchSpeedForPull(Wf), Vo = bOldWc ? 820.f : ZoogiConst::LaunchSpeedForPull(Wf);
					const float Undo = bDriveWc ? 1.f : 1.f / FMath::Max(0.05f, GSkillPow); // Kick scales by the skill strength - cancel it (b87 had none)
					if (AZoogiBall* Bb = Cast<AZoogiBall>(Tt)) { Kick(Bb->Sphere, (Dir * Vb + FVector(0, 0, 120.f)) * Undo); UE_LOG(LogZoogi, Log, TEXT("SKILL water_cannon hits P%d at %.0f (b87 launch force)"), Bb->PlayerIndex + 1, Vb); }
					else if (AZoogiOrb* O = Cast<AZoogiOrb>(Tt)) { Kick(O->Sphere, (Dir * Vo + FVector(0, 0, 100.f)) * Undo); O->LastHitBy = Pi; }
				});
			}
		}
		else if (Id == FName(TEXT("precision")))
		{	// b137b (Evan): up to 3 targets, a water-cannon blast at each (the water cannon's launch force), 0.2 s apart
			int32 k = 0;
			for (const TWeakObjectPtr<AActor>& WT : P.Targets)
			{
				if (!WT.IsValid()) continue;
				TWeakObjectPtr<AZoogiBall> WC = Caster;
				Later(G, 0.2f * k++, [WG, WT, WC, Pi]()
				{
					AZoogiGameMode* Gm = WG.Get(); AActor* Tt = WT.Get(); AZoogiBall* Cb = WC.Get(); if (!Gm || !Tt || !Cb) return;
					UWorld* Wq = Gm->GetWorld();
					const FVector Mq = Cb->BallLocation();
					const FVector Dir = (ActorPos(Tt) - Mq).GetSafeNormal2D();
					AZoogiFabFX::Play(Wq, FX(TEXT("NS_Water_cannon")), Mq + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_cannon")), 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation() + FRotator(0.f, WcYaw(), 0.f)); // b142c
					AZoogiFabFX::Sound(Wq, SFX(TEXT("S_Water_cannon")), Mq, 0.8f);
					Later(Gm, WcHitT() + FVector::Dist2D(ActorPos(Tt), Mq) / 5000.f, [WG = TWeakObjectPtr<AZoogiGameMode>(Gm), WT, Dir, Pi]()
					{
						AZoogiGameMode* Gn = WG.Get(); AActor* Tu = WT.Get(); if (!Gn || !Tu) return;
						AZoogiFabFX::Play(Gn->GetWorld(), FX(TEXT("NS_Water_hit")), ActorPos(Tu) - FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_hit")), 0.4f), 3.f);
						LabImpact(Gn->GetWorld(), ActorPos(Tu)); // b139b
						AZoogiFabFX::Sound(Gn->GetWorld(), SFX(TEXT("S_Water_cannon_hit")), ActorPos(Tu), 0.9f);
						const float V = ZoogiConst::LaunchSpeedForPull(1.f), Undo = 1.f / FMath::Max(0.05f, GSkillPow);
						if (AZoogiBall* Bb = Cast<AZoogiBall>(Tu)) { Kick(Bb->Sphere, (Dir * V + FVector(0, 0, 120.f)) * Undo); Bb->LastHitPlayer = Pi; }
						else if (AZoogiOrb* O = Cast<AZoogiOrb>(Tu)) { Kick(O->Sphere, (Dir * V + FVector(0, 0, 100.f)) * Undo); O->LastHitBy = Pi; }
					});
				});
			}
			UE_LOG(LogZoogi, Log, TEXT("SKILL precision: P%d water cannon x%d"), Pi + 1, k);
		}
		else if (Id == FName(TEXT("magic_rays")))
		{
			AZoogiFabFX::Play(W, FX(TEXT("NS_Magic_rays_attack")), MeFloor, Sc(Id, 0.35f), 2.f);
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Magic_ray")), Me, 1.f);
			for (int32 k = 0; k < P.Targets.Num(); ++k)
			{
				TWeakObjectPtr<AActor> WT = P.Targets[k];
				Later(G, 0.3f + 0.22f * k, [WG, WT, Me, Pi]()
				{
					AZoogiGameMode* Gm = WG.Get(); AActor* Tt = WT.Get(); if (!Gm || !Tt) return;
					const FVector Tp = ActorPos(Tt); const FVector Dir = (Tp - Me).GetSafeNormal2D();
					AZoogiFabFX::Play(Gm->GetWorld(), FX(TEXT("NS_Magic_ray")), Tp, Sc(FName(TEXT("magic_ray")), 0.15f), 1.6f);
					LabImpact(Gm->GetWorld(), Tp); // b139b
					if (AZoogiBall* Bb = Cast<AZoogiBall>(Tt)) Kick(Bb->Sphere, Dir * 380.f + FVector(0, 0, 380.f));
					else if (AZoogiOrb* O = Cast<AZoogiOrb>(Tt)) { Kick(O->Sphere, Dir * 340.f + FVector(0, 0, 300.f)); O->LastHitBy = Pi; }
				});
			}
		}
		else if (Id == FName(TEXT("sword_slash")))
		{
			AZoogiFabFX::Play(W, FX(TEXT("NS_Sword_AoE")), MeFloor, Sc(Id, 0.35f), 2.f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius + 2.f));
			AZoogiFabFX::Play(W, FX(TEXT("NS_Sword_crater")), MeFloor, Sc(FName(TEXT("sword_crater")), 0.35f), 6.f);
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Sword_air_attack")), Me, 1.f);
			Blast(G, Me, 380.f, 700.f, 150.f, Caster, Pi);
			FCrater Cr; Cr.P = MeFloor; Cr.R = 300.f; Cr.Turn = G->TurnCounter; Craters.Add(Cr);
		}
		else if (Id == FName(TEXT("energy_shockwave")))
		{
			AZoogiFabFX::Play(W, FX(TEXT("NS_Energy_shockwave")), At(P.Ctr), Sc(Id, 0.45f), 2.f);
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Energy_shockwave")), At(P.Ctr), 1.f);
			const FVector Pc = At(P.Ctr);
			static const bool bOldSw = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldShockwave"));
			if (bOldSw)
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !O->Sphere) continue;
				const FVector D = Pc - O->OrbLocation(); const float Dd = D.Size2D();
				if (Dd > P.R || Dd < 30.f) continue;
				Kick(O->Sphere, D.GetSafeNormal2D() * (120.f + 380.f * Dd / P.R));
				O->LastHitBy = Pi;
			}
			else
			{	// b88 (Evan: "increase energy shockwave gravity suck, then explode 2x the force of Wolf Pack explosions"): two strong pulls
				// (orbs and rival Zoogis), then a blast at twice a wolf's (420 + 1380 K, K = 1 at the centre) with twice its hop
				const float Rw = P.R * 1.25f * ZoogiSkillLab::Radius(); // b107
				const FLinearColor Pu(0.65f, 0.35f, 1.f);
				auto Suck = [WG, Pc, Rw, Pi, Pu]()
				{
					AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
					for (AZoogiOrb* O : Gm->Orbs)
					{
						if (!O || O->bOut || !O->Sphere) continue;
						const FVector D = Pc - O->OrbLocation(); const float Dd = D.Size2D();
						if (Dd > Rw || Dd < 25.f) continue;
						Kick(O->Sphere, D.GetSafeNormal2D() * (380.f + 900.f * Dd / Rw) * ZoogiSkillLab::Pull()); // b107: the lab pull
						O->LastHitBy = Pi;
					}
					for (AZoogiBall* Bz : Gm->Balls)
					{
						if (!Bz || Bz->bOut || Bz->PlayerIndex == Pi || !Bz->Sphere) continue;
						const FVector D = Pc - Bz->BallLocation(); const float Dd = D.Size2D();
						if (Dd > Rw || Dd < 25.f) continue;
						Kick(Bz->Sphere, D.GetSafeNormal2D() * (260.f + 640.f * Dd / Rw) * ZoogiSkillLab::Pull());
					}
					AZoogiFX::Spawn(Gm->GetWorld(), Pc + FVector(0, 0, 6.f), Pu, Rw, 0.35f, true, 8.f);
				};
				Suck();
				Later(G, 0.3f, Suck);
				Later(G, 0.85f, [WG, Pc, Rw, Pi, Pu]()
				{
					AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
					UWorld* Wb = Gm->GetWorld();
					int32 Nb = 0;
					auto Push = [&](UPrimitiveComponent* Body, const FVector& L)
					{
						if (!Body || !Body->IsSimulatingPhysics()) return false;
						FVector D = L - Pc; D.Z = 0.f;
						const float Dist = D.Size();
						if (Dist > Rw) return false;
						const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
						const float K = 1.f - Dist / Rw;
						const FVector V = Body->GetPhysicsLinearVelocity();
						Body->WakeAllRigidBodies();
						Body->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + (Dir * 2.f * (420.f + 1380.f * K) + FVector(0.f, 0.f, 320.f * K)) * GSkillPow);
						++Nb; return true;
					};
					for (AZoogiOrb* O : Gm->Orbs) if (O && !O->bOut && Push(O->Sphere, O->OrbLocation())) O->LastHitBy = Pi;
					for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz->PlayerIndex != Pi && Push(Bz->Sphere, Bz->BallLocation())) Bz->LastHitPlayer = Pi;
					AZoogiFabFX::Play(Wb, FX(TEXT("NS_Energy_shockwave")), Pc, Sc(FName(TEXT("energy_shockwave")), 0.45f) * 1.4f, 2.f);
					AZoogiFX::Stylized(Wb, SkillBlastFx(), Pc, 0.7f * SkillBlastSc());
					AZoogiFX::Spawn(Wb, Pc + FVector(0, 0, 6.f), Pu, Rw * 1.2f, 0.6f, true, 10.f);
					UZoogiScreenFx::Shockwave(Wb, Pc, Rw + 60.f, Pu);
					ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 1.f, 0.85f);
					ZoogiHaptics::Kick(0.8f, Wb, TEXT("shockwave_blast"));
					UE_LOG(LogZoogi, Log, TEXT("SKILL energy_shockwave P%d: suck x2 then blast (2x wolf) - %d pushed"), Pi + 1, Nb);
				});
			}
		}
		else if (Id == FName(TEXT("gattling_burst"))) GatStart(G, Caster, 0); // b190
		else if (Id == FName(TEXT("gattling_beat"))) GatStart(G, Caster, 1); // b190
		else if (Id == FName(TEXT("gattling_turret"))) TurretStart(G, Caster, ZoogiSkillLab::Open() && P.Spots.Num() ? FMath::Lerp(Caster->BallLocation(), P.Spots[0], 0.42f) : P.Spots.Num() ? P.Spots[0] : Caster->BallLocation() + FRotator(0.f, Caster->AimYaw, 0.f).Vector() * 400.f); // b190; b225: lab spot
		else if (Id == FName(TEXT("energy_buff")))
		{
			if (AZoogiFabFX* Ef = AZoogiFabFX::Play(W, FX(TEXT("NS_Energy_buff")), MeFloor, Sc(Id, 0.18f), 5.f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius + 2.f))) Ef->StopAfter = 1.2f; // b119 (Evan: "only play the loop effect once")
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Energy_buff")), Me, 1.f);
			FBuff Bf; Bf.B = Caster; Bf.SavedMass = Caster->Sphere ? Caster->Sphere->GetMass() : 0.f; Bf.Turn = G->TurnCounter;
			if (Caster->Sphere) Caster->Sphere->SetMassOverrideInKg(NAME_None, Bf.SavedMass * 1.5f, true);
			if (G->Phase == EZoogiPhase::Rolling) Kick(Caster->Sphere, Caster->GetBallVelocity() * 0.2f);
			else Bf.bVelPending = true;
			Buffs.Add(Bf);
			// b115 (Evan: "Energy Buff as a skill increases crystal by one"); b119 (Evan: "by crystal I meant the circles under the SP
			// count"): +1 Awaken charge pip (not SP; the card costs 1 SP again)
			// b188 (Evan: "energy buff is broken - it should give its crystals, not one now and one free at my next turn - cost 3, give 3"):
			// exactly +3 crystals right now, past the cap and the one-per-turn limit (the tap itself adds none - see CardsTapPlay)
			const int32 CrPi = FMath::Clamp(Caster->PlayerIndex, 0, 3), CrWas = G->AwakenCharge[CrPi];
			G->CardsAddAwakenCharge(Caster->PlayerIndex, 1, TEXT("energy buff")); // feeds the AWAKEN meter
			if (G->AwakenChargeOn()) G->AwakenCharge[CrPi] = CrWas + 3;
			UE_LOG(LogZoogi, Log, TEXT("AWAKEN charge P%d %d -> %d (energy buff +3)"), CrPi + 1, CrWas, G->AwakenCharge[CrPi]);
			G->PublicPopup(TEXT("+3 CRYSTALS"), Caster->BallLocation() + FVector(0, 0, 190.f), FLinearColor(1.f, 0.85f, 0.45f));
		}
		else if (Id == FName(TEXT("wood_shield")))
		{
			AZoogiFabFX* WoodFx = AZoogiFabFX::Play(W, FX(TEXT("NS_Wood_shield")), MeFloor, 0.3f, WoodStays() ? 9999.f : 1.6f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius));
			if (WoodFx && !WoodStays()) WoodFx->StopAfter = 1.4f; // b71 (Grok): bigger, stays up longer
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Wood_shield")), Me, 1.f);
			for (FShield& Old : Shields) if (Old.B.Get() == Caster) DropShieldFx(Old);
			Shields.RemoveAll([Caster](const FShield& X) { return X.B.Get() == Caster; });
			FShield Sh; Sh.B = Caster; Sh.SavedMass = Caster->Sphere ? Caster->Sphere->GetMass() : 0.f; Sh.Turn = G->TurnCounter; Sh.NextFx = Now(G) + 1.3f; Sh.Fx = WoodFx; Sh.CastAt = FPlatformTime::Seconds(); Shields.Add(Sh);
			if (WoodStays())
			{	// b90b (Evan): the cast ends the turn; the shield stays until this Zoogi's next turn
				const int32 Tc = G->TurnCounter;
				if (UZoogiFunSys* Fs = UZoogiFunSys::Get(G)) Fs->PublicCallout(TEXT("WOOD SHIELD!"), TEXT("TURN OVER - THE SHIELD HOLDS UNTIL YOUR NEXT TURN"), FLinearColor(0.55f, 0.85f, 0.35f), 1.6f, 1.f);
				Later(G, 1.2f, [WG, Tc]() { AZoogiGameMode* Gm = WG.Get(); if (Gm && Gm->TurnCounter == Tc && Gm->Phase == EZoogiPhase::Aiming) { UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield ends P%d's turn"), Gm->ActivePlayer + 1); Gm->SkillEndsTurn(); } });
			}
		}
		else if (Id == FName(TEXT("apple_bombs")))
		{
			for (const FVector& Sp : P.Spots)
			{
				FMine M; M.P = At(Sp); M.Owner = Pi; M.Pow = GSkillPow;
				M.Fx = AZoogiFabFX::Play(W, FX(TEXT("NS_Apple")), M.P, Sc(Id, 0.5f), 600.f);
				Mines.Add(M);
			}
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_bombs")), Me, 0.6f);
		}
		else if (Id == FName(TEXT("halo_disc")))
		{	// b137: the HALO DISC weapon - cuts along the swiped line and back, knocking everything near the line aside
			const FVector A0 = At(P.A), B0 = At(P.B);
			const FVector2D Ab(B0.X - A0.X, B0.Y - A0.Y);
			const float Ln = FMath::Max(1.f, Ab.Size());
			const FVector2D Dn = Ab / Ln, Nn(-Dn.Y, Dn.X);
			auto Cut = [&](UPrimitiveComponent* Sp, const FVector& Lc, float Pw) -> bool
			{
				const FVector2D Q(Lc.X - A0.X, Lc.Y - A0.Y);
				const float Tq = FVector2D::DotProduct(Q, Dn), Dd = FVector2D::DotProduct(Q, Nn);
				if (Tq < -80.f || Tq > Ln + 80.f || FMath::Abs(Dd) > 170.f) return false;
				const FVector2D Side = Dd >= 0.f ? Nn : -Nn;
				Kick(Sp, FVector(Side.X, Side.Y, 0.f) * Pw + FVector(Dn.X, Dn.Y, 0.f) * 250.f + FVector(0.f, 0.f, 140.f));
				return true;
			};
			int32 Hit = 0;
			for (AZoogiBall* Ob : G->Balls) if (Ob && Ob != Caster && !Ob->bOut && Ob->Sphere && Cut(Ob->Sphere, Ob->BallLocation(), 900.f)) { Ob->LastHitPlayer = Pi; ++Hit; }
			for (AZoogiOrb* Oo : G->Orbs) if (Oo && !Oo->bOut && Oo->Sphere && Cut(Oo->Sphere, Oo->OrbLocation(), 1100.f)) { Oo->LastHitBy = Pi; ++Hit; }
			for (int32 k = 0; k <= 8; ++k) AZoogiFX::Spawn(W, FMath::Lerp(A0, B0, k / 8.f) + FVector(0.f, 0.f, 40.f), FLinearColor(1.f, 0.85f, 0.35f), 90.f, 0.35f, true, 8.f);
			SkillStyl(W, TEXT("Mid_Explosion"), B0, 0.3f); // b139b
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.9f, 1.4f);
			UE_LOG(LogZoogi, Log, TEXT("ITEM halo disc P%d: %.0f cm cut, %d hit"), Pi + 1, Ln, Hit);
		}
		else if (Id == FName(TEXT("golden_apple")))
		{
			AActor* TgA = P.Targets.Num() ? P.Targets[0].Get() : nullptr;
			AZoogiOrb* O = Cast<AZoogiOrb>(TgA);
			const bool bAtShield = !O && Cast<AZoogiBall>(TgA) != nullptr; // b188: thrown at a wood shield
			if (O || bAtShield)
			{
				// b79 (Evan: "Golden Apple's effect should fire for the player, not the orbs; the target orb's blast should act the same
				// as a single explosion from a wolf"): Apple of Eden on the caster, then a wolf-clone detonation at the orb
				static const bool bOldApple = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGoldenApple"));
				if (bOldApple && O)
				{
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), O->OrbLocation(), Sc(Id, 0.3f), 3.f, O);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), O->OrbLocation(), 1.f);
				}
				else if (!O || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAppleInstant")))
				{	// b134 (Evan: "the blast happens where the apple lands and only after the apple does its animation"): form -> lob -> land -> blast
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), Caster->BallLocation(), Sc(Id, 0.3f), 3.f, Caster);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), Caster->BallLocation(), 1.f);
					const FVector From = Caster->BallLocation() + FVector(0.f, 0.f, 110.f);
					const FVector TgP = ActorPos(TgA);
					const FVector To(TgP.X, TgP.Y, FZ + 30.f); // where the orb is now - it lands there even if the orb moves
					const float FormT = 0.3f, FlyT = FMath::Clamp(0.4f + FVector::Dist2D(From, To) / 3400.f, 0.45f, 0.9f); // b190 FAST FX: was 0.7 / 0.65-1.3
					FActorSpawnParameters SPa; SPa.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					TWeakObjectPtr<AActor> WApple;
					if (AStaticMeshActor* Ap = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(From), SPa))
					{
						Ap->SetMobility(EComponentMobility::Movable);
						UStaticMeshComponent* Am = Ap->GetStaticMeshComponent();
						if (UStaticMesh* AppleM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StylizedToonVFX3/Models/SM_Apple.SM_Apple"), nullptr, LOAD_NoWarn | LOAD_Quiet))
						{
							Am->SetStaticMesh(AppleM);
							Am->SetRelativeScale3D(FVector(70.f / FMath::Max(1.f, AppleM->GetBoundingBox().GetSize().GetMax())));
						}
						else { Am->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); Ap->SetActorScale3D(FVector(0.6f)); }
						// b142 (Evan: "make the apple itself solid gold"): opaque metallic gold with a faint warm glow so it reads on dark floors
						if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldAppleLook"))) Am->SetMaterial(0, ZoogiUtil::FxMID(Ap, FLinearColor(1.f, 0.78f, 0.2f), 4.f));
						else { const int32 Nm = FMath::Max(1, Am->GetNumMaterials()); for (int32 m = 0; m < Nm; ++m) Am->SetMaterial(m, ZoogiUtil::SurfaceMID(Ap, FLinearColor(1.f, 0.7f, 0.16f), 0.18f, FLinearColor(1.f, 0.62f, 0.12f), 0.35f, 1.f)); Am->SetCastShadow(true); }
						Am->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						Am->SetCastShadow(false);
						Ap->SetActorHiddenInGame(true);
						WApple = Ap;
					}
					const double T0 = FPlatformTime::Seconds();
					const int32 PiA = Pi; const float FZA = FZ;
					// b136 (Evan: the lab's strength / radius must work): the tunings of this cast, used when the apple lands
					const float StrA = GSkillPow / FMath::Max(0.1f, CardPow()), RadA = ZoogiSkillLab::Radius(), FxA = ZoogiSkillLab::FxSize();
					FLinearColor GoldA(1.f, 0.8f, 0.25f); ZoogiSkillLab::Tint(GoldA);
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, WApple, From, To, FormT, FlyT, T0, PiA, FZA, StrA, RadA, FxA, GoldA](float) -> bool
					{
						AZoogiGameMode* Gm = WG.Get();
						AActor* Ap = WApple.Get();
						if (!Gm) { if (Ap) Ap->Destroy(); return false; }
						const float T = (float)(FPlatformTime::Seconds() - T0);
						if (T < FormT) return true; // the Apple of Eden forms on the caster
						const float K = FMath::Clamp((T - FormT) / FlyT, 0.f, 1.f);
						FVector P = FMath::Lerp(From, To, K); P.Z += 4.f * FMath::Max(260.f, FVector::Dist2D(From, To) * 0.32f) * K * (1.f - K); // the lob
						if (Ap) { Ap->SetActorHiddenInGame(false); Ap->SetActorLocation(P); Ap->AddActorWorldRotation(FRotator(0.f, 9.f, 4.f)); }
						static double LastSpark = 0.0;
						if (FPlatformTime::Seconds() - LastSpark > 0.05) { LastSpark = FPlatformTime::Seconds(); AZoogiFX::Spawn(Gm->GetWorld(), P, FLinearColor(1.f, 0.8f, 0.25f), 55.f, 0.3f, false, 10.f); }
						if (K < 1.f) return true;
						static const bool bNoLeft = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAppleLeft"));
						if (Ap && !bNoLeft)
						{	// b138c (Evan): the golden apple stays where it landed - touching it sets off a medium blast
							Ap->SetActorLocation(FVector(To.X, To.Y, FZA + 36.f));
							FGoldApple Ga; Ga.A = Ap; Ga.P = FVector(To.X, To.Y, FZA + 36.f); Ga.Owner = PiA; Ga.ArmAt = FPlatformTime::Seconds() + 0.6;
							GoldApples.Add(Ga);
							UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: the apple stays at (%.0f, %.0f) - touch = blast"), To.X, To.Y);
						}
						else if (Ap) Ap->Destroy();
						// lands: the blast (bigger than a wolf's, the tapped orb goes flying too)
						UWorld* Wg = Gm->GetWorld();
						const FVector Oc(To.X, To.Y, FZA + 40.f);
						const float R = 650.f * RadA;
						BreakShieldsNear(Gm, Oc, R); // b188
						int32 Pushed = 0;
						for (AZoogiOrb* Ob : Gm->Orbs)
						{
							if (!Ob || Ob->bOut || !Ob->Sphere || !Ob->Sphere->IsSimulatingPhysics()) continue;
							FVector D = Ob->OrbLocation() - Oc; D.Z = 0.f;
							const float Dist = D.Size();
							if (Dist > R) continue;
							const FVector Dir = Dist > 1.f ? D / Dist : FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
							const float Kk = 1.f - Dist / R;
							const FVector V = Ob->Sphere->GetPhysicsLinearVelocity();
							Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + (Dir * (520.f + 1700.f * Kk) + FVector(0.f, 0.f, 200.f * Kk)) * StrA);
							Ob->LastHitBy = PiA;
							++Pushed;
						}
						const FVector Ground(Oc.X, Oc.Y, FZA + 4.f);
						const FLinearColor Gold = GoldA;
						AZoogiFabFX::Play(Wg, FX(TEXT("NS_Apple_of_Eden")), Ground, Sc(FName(TEXT("golden_apple")), 0.3f) * 1.3f * FxA, 2.f);
						AZoogiFX::Spawn(Wg, Oc, FLinearColor(1.f, 0.95f, 0.7f), 160.f, 0.25f, false, 16.f);
						AZoogiFX::Spawn(Wg, Ground, Gold, R, 0.6f, true, 10.f);
						AZoogiFX::Shockwave(Wg, Ground, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Gold, false);
						UZoogiScreenFx::Shockwave(Wg, Ground, R + 80.f, Gold);
						{ FTuneScope Tg(1.f, FName(TEXT("golden_apple"))); SkillStyl(Wg, TEXT("Mid_Explosion"), Ground, 0.55f * FxA); } // b139b
						ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 0.9f, 1.05f);
						UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: the apple lands - blast r %.0f x%.2f strength, pushed %d orbs"), R, StrA, Pushed);
						return false;
					}));
					UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: Eden forms on P%d, lob %.0f cm in %.2f s"), Pi + 1, FVector::Dist2D(From, To), FlyT);
				}
				else
				{
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), Caster->BallLocation(), Sc(Id, 0.3f), 3.f, Caster);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), Caster->BallLocation(), 1.f);
					const FVector Oc = O->OrbLocation();
					const float R = 430.f; // AZoogiClone ShockR (one wolf, no pack multipliers)
					BreakShieldsNear(G, Oc, R); // b188
					int32 Pushed = 0;
					for (AZoogiOrb* Ob : G->Orbs)
					{
						if (!Ob || Ob == O || Ob->bOut || !Ob->Sphere || !Ob->Sphere->IsSimulatingPhysics()) continue;
						FVector D = Ob->OrbLocation() - Oc; D.Z = 0.f;
						const float Dist = D.Size();
						if (Dist > R) continue;
						const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
						const float K = 1.f - Dist / R;
						const FVector V = Ob->Sphere->GetPhysicsLinearVelocity();
						Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (420.f + 1380.f * K) + FVector(0.f, 0.f, 160.f * K));
						Ob->LastHitBy = Pi;
						++Pushed;
					}
					const FVector Ground(Oc.X, Oc.Y, FZ + 4.f);
					const FLinearColor Gold(1.f, 0.8f, 0.25f);
					AZoogiFX::Spawn(W, Oc, FLinearColor(0.75f, 0.9f, 1.f), 120.f, 0.22f, false, 14.f);
					AZoogiFX::Spawn(W, Ground, Gold, R, 0.55f, true, 10.f);
					AZoogiFX::Shockwave(W, Ground, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Gold, false);
					UZoogiScreenFx::Shockwave(W, Ground, R + 60.f, Gold);
					ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.1f);
					SkillStyl(W, TEXT("Mid_Explosion"), Ground, 0.42f); // b139b
					UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: Eden on P%d, wolf blast at the orb pushed %d orbs"), Pi + 1, Pushed);
				}
				if (O && (bOldApple || FParse::Param(FCommandLine::Get(), TEXT("ZoogiAppleInstant"))))
				{	// b134 (Evan: "it doesn't turn gold, it just gets blasted away"): only the old versions make a gold orb
					O->bStar = true; O->PowerColor = FLinearColor(1.f, 0.8f, 0.25f);
					AZoogiFX::Spawn(W, O->OrbLocation(), FLinearColor(1.f, 0.8f, 0.25f), 120.f, 30.f, true, 6.f, O);
				}
			}
		}
		if (!G->ShotDir.IsEmpty()) { const FString N = FString::Printf(TEXT("skill_%s"), *Id.ToString()); G->TakeShotDelayed(N + TEXT("_a"), 0.45f); G->TakeShotDelayed(N + TEXT("_b"), 1.1f); }
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (G) GatTick(G); // b190 GATTLING
		{	// b144b: -ZoogiItemTest - press FIRE on the human's grenade throw pick
			static const bool bIt = FParse::Param(FCommandLine::Get(), TEXT("ZoogiItemTest"));
			static double CardShotFor = -1.0; // b178g: a frame of the HOW TO USE card while the pick is open
			if (bIt && G && Picking() && S.bGrenade && !S.bCpu && CardShotFor != S.OpenedAt && FPlatformTime::Seconds() - S.OpenedAt > 0.9) { CardShotFor = S.OpenedAt; G->TakeShotDelayed(TEXT("grenade_card"), 0.f); }
			if (bIt && G && Picking() && S.bGrenade && !S.bCpu && S.FireBtn.bIsValid && FPlatformTime::Seconds() - S.OpenedAt > 1.6)
			{
				UE_LOG(LogZoogi, Log, TEXT("ITEM TEST: presses FIRE on the grenade pick"));
				G->TakeShotDelayed(TEXT("grenade_pick"), 0.f);
				Input(G, nullptr, S.FireBtn.GetCenter(), true, false, false);
				G->TakeShotDelayed(TEXT("grenade_air"), 0.35f);
				G->TakeShotDelayed(TEXT("grenade_boom"), 1.1f);
			}
		}
		if (G && G->Arena && !G->bRoyaleMode)
		{	// b139a (Evan): the Volcano fires meteors at random rolls - more every two rounds - a warning circle first
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVolcanoMeteors"));
			static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVolcanoMeteorsTest"));
			if (!bOff && (bTest || G->Arena->GetTheme() == EZoogiArena::Volcanic) && G->Phase == EZoogiPhase::Rolling && VolcTurn != G->TurnCounter)
			{
				VolcTurn = G->TurnCounter;
				FRandomStream Rv(HashCombine(GetTypeHash(G->GameSeed), GetTypeHash(G->TurnCounter * 7919)));
				const float Chance = FMath::Min(0.85f, 0.25f + 0.15f * ((G->RoundNumber - 1) / 2)); // b139a (Evan): it happens more often every two rounds
				if (bTest || Rv.FRand() < Chance)
				{
					const int32 N = 5; // b139a (Evan 10:37: "when the event happens, 5 in random spots in the arena")
					const FVector C0 = G->Arena->GetActorLocation();
					const float ArR = 1350.f * FMath::Max(1.f, G->Arena->GetActorScale3D().X);
					float Fz = C0.Z;
					if (const AZoogiBall* Ab = G->GetActiveBall()) Fz = Ab->BallLocation().Z - ZoogiConst::ZoogiRadius;
					for (int32 k = 0; k < N; ++k)
					{
						FVolcWarn Wv;
						for (int32 Try = 0; Try < 12; ++Try)
						{
							const float A = Rv.FRandRange(0.f, 2.f * PI), Rr = ArR * FMath::Sqrt(Rv.FRandRange(0.05f, 0.75f));
							Wv.P = FVector(C0.X + FMath::Cos(A) * Rr, C0.Y + FMath::Sin(A) * Rr, Fz + 2.f);
							bool bClose = false; for (const FVolcWarn& O : VolcWarns) if (FVector::Dist2D(O.P, Wv.P) < 420.f) bClose = true;
							if (!bClose) break;
						}
						Wv.T0 = Now(G) + 0.25 * k; Wv.FireAt = Wv.T0 + 1.6; // b178i: game time - a REACTION / skill pick (the world waits) freezes the meteors too
						VolcWarns.Add(Wv);
					}
					ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.5f, 0.6f);
					UE_LOG(LogZoogi, Log, TEXT("VOLCANO meteors: %d strike(s) this roll (round %d)"), N, G->RoundNumber);
				}
			}
			for (int32 i = VolcWarns.Num() - 1; i >= 0; --i)
			{
				FVolcWarn& Wv = VolcWarns[i];
				if (!Wv.bFired && Now(G) >= Wv.FireAt) { Wv.bFired = true; WorldFireMeteors(G, Wv.P, Wv.R); }
				if (Now(G) > Wv.FireAt + 1.6) VolcWarns.RemoveAt(i);
			}
		}
		WolfTick(G);
		ZoogiSkillLab::Tick(G, RealDt); // b107
		if (Picking() && S.bCpu && FPlatformTime::Seconds() - S.OpenedAt > 10.0) Cancel(TEXT("watchdog: the CPU's pick ran over 10 s")); // b95c
		if (Picking() && S.bCpu && G)
		{	// b67: the CPU's cursor glides to its choice, picks the targets one by one, then presses FIRE
			const double Tn = FPlatformTime::Seconds();
			const float K = FMath::Clamp((float)(Tn - S.OpenedAt - 0.35) / 0.9f, 0.f, 1.f), E = FMath::InterpEaseInOut(0.f, 1.f, K, 2.f);
			S.Ctr = FMath::Lerp(S.Origin, S.CpuCtr, E); S.A = FMath::Lerp(S.Origin, S.CpuA, E); S.B = FMath::Lerp(S.Origin, S.CpuB, E);
			const int32 NT = FMath::Clamp(FMath::FloorToInt((float)(Tn - S.OpenedAt - 0.7) / 0.35f) + 1, 0, 99);
			while (S.Targets.Num() < FMath::Min(NT, S.CpuTargets.Num())) { S.Targets.Add(S.CpuTargets[S.Targets.Num()]); ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.6f, 1.2f); }
			while (S.Spots.Num() < FMath::Min(NT, S.CpuSpots.Num())) { S.Spots.Add(S.CpuSpots[S.Spots.Num()]); ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.6f, 1.2f); }
			if (Tn >= S.CpuFireAt)
			{
				AZoogiBall* Cb = S.Caster.Get();
				S.Targets = S.CpuTargets; S.Spots = S.CpuSpots; S.Ctr = S.CpuCtr; S.A = S.CpuA; S.B = S.CpuB;
				S.bConfirmed = true;
				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.2f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL cpu P%d FIRE %s"), Cb ? Cb->PlayerIndex + 1 : 0, *S.Id.ToString());
				if (Cb) Execute(G, Cb, S.Id); else Reset();
			}
		}
		if (Picking() && FPlatformTime::Seconds() - S.OpenedAt > 30.0) Cancel(TEXT("30 s timeout")); // b67 (Evan): the game waits up to 30 s for the target
		if (!G) return;
		if (ForGame.Get() != G || ForSeed != G->GameSeed)
		{
			for (FWall& Wl : Walls) if (AActor* A = Wl.Collider.Get()) A->Destroy();
			for (FMine& M : Mines) if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
			DropGoldApples(); DropTurrets(); // b138c; b190
			AZoogiIceBarrier::DestroyAll(G->GetWorld());
			Walls.Reset(); Mines.Reset(); Craters.Reset(); Buffs.Reset(); for (FShield& S0 : Shields) DropShieldFx(S0); Shields.Reset(); Events.Reset(); PlusWalls.Reset(); for (FPatch& Pt0 : Patches) if (AZoogiFabFX* F0 = Pt0.Fx.Get()) F0->Destroy(); Patches.Reset(); Traps.Reset(); Counters.Reset(); Spins.Reset(); PlusPrevVel.Reset(); PlusWallCool.Reset(); NewFx.Reset(); PadCool.Reset(); /* b271 / b272 */ Reset();
			ForGame = G; ForSeed = G->GameSeed; LastTurn = G->TurnCounter;
		}
		const float T = Now(G);
		for (int32 i = 0; i < Events.Num(); )
		{
			if (T >= Events[i].At) { TFunction<void()> Fn = MoveTemp(Events[i].Fn); Events.RemoveAt(i); if (Fn) Fn(); }
			else ++i;
		}
		// a pick left open when the turn moved on closes
		if (Picking() && (G->GetActiveBall() != S.Caster.Get())) Cancel(TEXT("turn over"));
		const bool bNewTurn = G->TurnCounter != LastTurn; LastTurn = G->TurnCounter;
		for (int32 i = Walls.Num() - 1; i >= 0; --i)
		{
			bool bAlive = T < Walls[i].Until;
			if (bAlive) { bAlive = false; for (const TWeakObjectPtr<AZoogiFabFX>& F : Walls[i].Fx) if (F.IsValid() && F->NC && F->NC->IsActive()) bAlive = true; }
			if (!bAlive) { if (AActor* A = Walls[i].Collider.Get()) A->Destroy(); Walls.RemoveAt(i); }
		}
		for (int32 i = GoldApples.Num() - 1; i >= 0; --i)
		{	// b138c: a Zoogi touching a golden apple left on the court -> a medium blast
			FGoldApple& Ga = GoldApples[i];
			AActor* Aa = Ga.A.Get();
			if (!Aa) { GoldApples.RemoveAt(i); continue; }
			Ga.Spin += 90.f * G->GetWorld()->GetDeltaSeconds();
			Aa->SetActorLocationAndRotation(Ga.P + FVector(0.f, 0.f, 8.f * FMath::Sin(Ga.Spin * 0.05f)), FRotator(0.f, Ga.Spin, 0.f));
			if (FPlatformTime::Seconds() < Ga.ArmAt) continue;
			AZoogiBall* Toucher = nullptr;
			for (AZoogiBall* Bt : G->Balls) if (Bt && !Bt->bOut && FVector::Dist2D(Bt->BallLocation(), Ga.P) < ZoogiConst::ZoogiRadius + 45.f) { Toucher = Bt; break; }
			if (!Toucher) continue;
			const FVector Gp(Ga.P.X, Ga.P.Y, Ga.P.Z - 32.f);
			BreakShieldsNear(G, Gp, 430.f); // b188
			{ FTuneScope Tn(1.f, FName(TEXT("golden_apple"))); Blast(G, Gp, 430.f, 1000.f, 260.f, nullptr, Ga.Owner); } // one wolf-size blast (lab radius / strength apply)
			const FLinearColor Gold(1.f, 0.8f, 0.25f);
			AZoogiFX::Spawn(G->GetWorld(), Gp, Gold, 430.f, 0.5f, true, 10.f);
			AZoogiFX::Shockwave(G->GetWorld(), Gp, EZoogiShockwaveSize::Normal, EZoogiShockwaveKind::Flat, &Gold, false);
			{ FTuneScope Tg(1.f, FName(TEXT("golden_apple"))); SkillStyl(G->GetWorld(), TEXT("Mid_Explosion"), Gp, 0.4f); } // b139b
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.2f);
			UE_LOG(LogZoogi, Log, TEXT("SKILL golden apple touched by P%d - blast (owner P%d)"), Toucher->PlayerIndex + 1, Ga.Owner + 1);
			Aa->Destroy();
			GoldApples.RemoveAt(i);
		}
		for (int32 i = Mines.Num() - 1; i >= 0; --i)
		{
			const FMine M = Mines[i];
			bool bBoom = false;
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), M.P) < 85.f) bBoom = true;
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), M.P) < 70.f) bBoom = true;
			if (!bBoom) continue;
			if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
			AZoogiFabFX::Play(G->GetWorld(), FX(TEXT("NS_Apple_bombs")), M.P, Sc(FName(TEXT("apple_boom")), 0.4f), 2.5f);
			AZoogiFabFX::Sound(G->GetWorld(), SFX(TEXT("S_Apple_bombs")), M.P, 1.f);
			{ FTuneScope Tn(M.Pow, FName(TEXT("apple_bombs"))); Blast(G, M.P, 280.f, 650.f, 260.f, nullptr, M.Owner); } // b136: the lab tuning
			UE_LOG(LogZoogi, Log, TEXT("SKILL apple mine P%d exploded at (%.0f, %.0f)"), M.Owner + 1, M.P.X, M.P.Y);
			Mines.RemoveAt(i);
		}
		{	// b271 (Evan: "build everything as mentioned in the review doc"): wall payoff, ice patches, frost traps, cross-slash counters, sword spin dash
			UWorld* Wp = G->GetWorld(); const float Dtp = Wp ? Wp->GetDeltaSeconds() : 0.f; const double Nw = FPlatformTime::Seconds(); const int32 Tn = G->TurnCounter;
			{	// b271 ELEMENT BONUS: worked out once per match from each player's deck
				static const bool bNoElem = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoElementBonus"));
				if (ElemG.Get() != G || Tn < ElemTurn) { ElemG = G; bElemDone = false; for (int32& E : ElemOf) E = 0; for (bool& Bo : ElemOut) Bo = false; }
				ElemTurn = Tn;
				UZoogiCardSubsystem* Cs = UZoogiCardSubsystem::Get(G);
				if (!bElemDone && !bNoElem && Cs && G->Phase == EZoogiPhase::Aiming)
				{
					bElemDone = true;
					for (int32 Pi2 = 0; Pi2 < FMath::Min(4, G->Balls.Num()); ++Pi2)
					{
						const FZoogiCardPlayerState* Ps = Cs->Rules().Player(Pi2); if (!Ps) continue;
						int32 Cnt[5] = { 0, 0, 0, 0, 0 }; TSet<FName> Seen;
						auto Count = [&](const TArray<FZoogiCardInstance>& L) { for (const FZoogiCardInstance& Ci : L) { if (Seen.Contains(Ci.CardId)) continue; Seen.Add(Ci.CardId); if (const FZoogiCardDef* D = Cs->Def(Ci.CardId)) ++Cnt[FMath::Clamp((int32)D->Element, 0, 4)]; } };
						Count(Ps->Hand); Count(Ps->DrawPile); Count(Ps->Discard);
						int32 Best = 0; for (int32 e = 1; e < 5; ++e) if (Cnt[e] >= 2 && (Best == 0 || Cnt[e] > Cnt[Best])) Best = e;
						ElemOf[Pi2] = Best;
						if (Best == 0) continue;
						const EZoogiCardElement E = (EZoogiCardElement)Best;
						const TCHAR* Txt = E == EZoogiCardElement::Fire ? TEXT("FIRE BONUS: +10% SKILL KNOCKBACK") : E == EZoogiCardElement::Frost ? TEXT("FROST BONUS: ORBS YOU HIT SLIDE FURTHER") : E == EZoogiCardElement::Shock ? TEXT("SHOCK BONUS: +1 REDIRECT CRYSTAL EVERY TURN") : TEXT("VOID BONUS: +1 CRYSTAL PER KNOCKOUT");
						if (G->Balls[Pi2] && !G->Balls[Pi2]->bCPU) G->PublicPopup(Txt, G->Balls[Pi2]->BallLocation() + FVector(0, 0, 220.f), FLinearColor(1.f, 0.85f, 0.4f));
						UE_LOG(LogZoogi, Log, TEXT("SKILL element bonus P%d: %s (%d skills)"), Pi2 + 1, Txt, Cnt[Best]);
					}
				}
				static int32 ShockTurn = -1;
				if (Tn != ShockTurn && G->Phase == EZoogiPhase::Aiming)
				{	// SHOCK: +1 free redirect crystal at the start of each of your turns (after the redirect system's own turn reset)
					ShockTurn = Tn;
					if (AZoogiBall* Ab = G->GetActiveBall()) if (ElemBonusOf(Ab->PlayerIndex) == (int32)EZoogiCardElement::Shock) { const int32 Sp = Ab->PlayerIndex; TWeakObjectPtr<AZoogiGameMode> WGs = G; Later(G, 0.4f, [WGs, Sp]() { if (WGs.IsValid()) ZoogiRedirect::GrantFree(Sp, 1); }); }
				}
				for (int32 i = 0; i < FMath::Min(8, G->Balls.Num()); ++i)
				{	// VOID: +1 crystal to whoever knocked a Zoogi out
					AZoogiBall* B = G->Balls[i]; const bool bOutNow = B && B->bOut;
					if (bOutNow && !ElemOut[i] && B->LastHitPlayer >= 0 && B->LastHitPlayer < 4 && B->LastHitPlayer != i && ElemBonusOf(B->LastHitPlayer) == (int32)EZoogiCardElement::Void)
					{
						const int32 A = B->LastHitPlayer; G->AwakenCharge[A] = FMath::Min(AZoogiGameMode::AwakenChargeMax, G->AwakenCharge[A] + 1);
						UE_LOG(LogZoogi, Log, TEXT("SKILL VOID bonus: P%d +1 crystal for the knockout"), A + 1);
					}
					ElemOut[i] = bOutNow;
				}
				for (AZoogiOrb* O : G->Orbs)
				{	// FROST: orbs you knocked keep sliding a little further
					if (!O || O->bOut || !O->Sphere || ElemBonusOf(O->LastHitBy) != (int32)EZoogiCardElement::Frost) continue;
					const FVector V = O->Sphere->GetPhysicsLinearVelocity(); const float Sp = V.Size2D();
					if (Sp > 60.f && Sp < 1400.f) O->Sphere->SetPhysicsLinearVelocity(FVector(V.X * (1.f + 0.22f * Dtp), V.Y * (1.f + 0.22f * Dtp), V.Z));
				}
			}
			PlusWalls.RemoveAll([Tn](const FPlusWall& X) { return X.Until < Tn; });
			{	// b272 new skills
				NewFx.RemoveAll([Tn](const FNewFx& X) { return X.Until < Tn; });
				const FVector Ac = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
				for (int32 k = NewFx.Num() - 1; k >= 0; --k)
				{
					FNewFx& F = NewFx[k]; bool bDrop = false;
					if (F.Kind == NK_EdgeSave)
					{
						AZoogiBall* B = F.B.Get(); if (!B || B->bOut || !B->Sphere) continue;
						const FVector V = B->GetBallVelocity(); const FVector2D V2(V.X, V.Y);
						if (V2.Size() > 180.f && G->Arena && G->Arena->DistanceToEdge(FVector2D(B->BallLocation()), V2.GetSafeNormal()) < 150.f)
						{
							FVector In = Ac - B->BallLocation(); In.Z = 0.f; In.Normalize();
							B->Sphere->SetPhysicsLinearVelocity(In * FMath::Max(500.f, V2.Size() * 0.6f) + FVector(0, 0, 120.f));
							AZoogiFX::Spawn(Wp, B->BallLocation(), FLinearColor(0.45f, 1.f, 0.55f), 160.f, 0.5f, true, 12.f);
							G->PublicPopup(TEXT("EDGE SAVE!"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.45f, 1.f, 0.55f));
							ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.8f, 1.1f);
							UE_LOG(LogZoogi, Log, TEXT("SKILL edge save: P%d pulled back from the edge"), B->PlayerIndex + 1);
							bDrop = true;
						}
					}
					else if (F.Kind == NK_Pad)
					{
						if (Nw > F.NextFx) { F.NextFx = Nw + 0.9; AZoogiFX::SpawnMark(Wp, F.P + FVector(0, 0, 4.f), FLinearColor(1.f, 0.7f, 0.25f), 120.f, 1.f); }
						auto Bounce = [&](AActor* A, UPrimitiveComponent* Body, const FVector& L)
						{
							if (!Body || !Body->IsSimulatingPhysics() || FVector::Dist2D(L, F.P) > (F.Sp2 > 0.f ? F.Sp2 : 125.f)) return; // b273: Sp2 = the pillar reach
							const FVector V = Body->GetPhysicsLinearVelocity(); if (V.Size2D() < 80.f) return;
							double& Cd = PadCool.FindOrAdd(A); if (Nw - Cd < 0.6) return; Cd = Nw;
							FVector N = L - F.P; N.Z = 0.f; if (!N.Normalize()) N = -V.GetSafeNormal2D();
							FVector R = V; if (FVector::DotProduct(V, N) < 0.f) R = V - 2.f * FVector::DotProduct(V, N) * N;
							R.Z = 0.f; R = R.GetClampedToMaxSize(1850.f) * 1.4f; R.Z = 150.f;
							Body->SetPhysicsLinearVelocity(R);
							AZoogiFX::Spawn(Wp, F.P + FVector(0, 0, 6.f), FLinearColor(1.f, 0.7f, 0.25f), 140.f, 0.3f, true, 11.f);
							ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.5f, 1.5f);
						};
						for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Bounce(B, B->Sphere, B->BallLocation());
						for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Bounce(O, O->Sphere, O->OrbLocation());
					}
					else if (F.Kind == NK_Rico)
					{
						AZoogiBall* B = F.B.Get(); if (!B || B->bOut || !B->Sphere) { bDrop = true; }
						else
						{
							const FVector V = B->GetBallVelocity(); const FVector V0 = F.PrevV; F.PrevV = V;
							bool bNearBall = false; for (AZoogiBall* O2 : G->Balls) if (O2 && O2 != B && !O2->bOut && FVector::Dist2D(O2->BallLocation(), B->BallLocation()) < 2.4f * ZoogiConst::ZoogiRadius) bNearBall = true;
							if (!bNearBall && V0.Size2D() > 300.f && V.Size2D() > 100.f && FVector::DotProduct(V0.GetSafeNormal2D(), V.GetSafeNormal2D()) < 0.35f)
							{
								FVector R = V * 2.f; R = R.GetClampedToMaxSize(2600.f); B->Sphere->SetPhysicsLinearVelocity(R);
								AZoogiFX::Spawn(Wp, B->BallLocation(), FLinearColor(1.f, 0.45f, 0.25f), 150.f, 0.35f, true, 12.f);
								G->PublicPopup(TEXT("RICOCHET x2!"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(1.f, 0.5f, 0.3f));
								ZoogiUtil::PlaySfx(G, TEXT("Launch"), 0.8f, 1.3f);
								UE_LOG(LogZoogi, Log, TEXT("SKILL ricochet: P%d off a wall at x2"), B->PlayerIndex + 1);
								bDrop = true;
							}
						}
					}
					else if (F.Kind == NK_Decoy)
					{
						if (Nw > F.NextFx) { F.NextFx = Nw + 0.5; AZoogiFX::Spawn(Wp, F.P, ZoogiUtil::PlayerColor(F.Owner).CopyWithNewOpacity(0.6f), ZoogiConst::ZoogiRadius * 1.1f, 0.45f, false, 6.f); }
						for (AZoogiBall* R : G->Balls)
						{
							if (!R || R->bOut || R->PlayerIndex == F.Owner || !R->Sphere) continue;
							if (FVector::Dist2D(R->BallLocation(), F.P) > 2.f * ZoogiConst::ZoogiRadius + 20.f || R->GetBallVelocity().Size2D() < 150.f) continue;
							R->Sphere->SetPhysicsLinearVelocity(R->GetBallVelocity() * 0.05f);
							AZoogiFX::Spawn(Wp, F.P, FLinearColor(0.7f, 0.5f, 1.f), 180.f, 0.4f, true, 12.f);
							G->PublicPopup(TEXT("DECOY! STOPPED DEAD"), F.P + FVector(0, 0, 190.f), FLinearColor(0.7f, 0.5f, 1.f));
							ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.7f, 0.8f);
							UE_LOG(LogZoogi, Log, TEXT("SKILL decoy: P%d stopped dead"), R->PlayerIndex + 1);
							bDrop = true; break;
						}
					}
					else if (F.Kind == NK_Tether)
					{
						AZoogiBall* A1 = F.B.Get(); AZoogiBall* A2 = F.B2.Get();
						if (!A1 || !A2 || A1->bOut || A2->bOut || !A1->Sphere || !A2->Sphere) { bDrop = true; }
						else
						{
							if (Nw > F.NextFx) { F.NextFx = Nw + 0.35; for (int32 j = 1; j <= 4; ++j) AZoogiFX::Spawn(Wp, FMath::Lerp(A1->BallLocation(), A2->BallLocation(), j / 5.f), FLinearColor(0.5f, 0.85f, 1.f), 22.f, 0.3f, false, 6.f); }
							const float S1 = A1->GetBallVelocity().Size2D(), S2 = A2->GetBallVelocity().Size2D();
							AZoogiBall* Hit = (F.Sp1 < 220.f && S1 > 520.f) ? A1 : (F.Sp2 < 220.f && S2 > 520.f) ? A2 : nullptr;
							F.Sp1 = S1; F.Sp2 = S2;
							if (Hit)
							{
								AZoogiBall* Other = Hit == A1 ? A2 : A1;
								Other->Sphere->WakeAllRigidBodies();
								Other->Sphere->SetPhysicsLinearVelocity(Hit->GetBallVelocity() * 0.85f + FVector(0, 0, 80.f));
								Other->LastHitPlayer = F.Owner; Hit->LastHitPlayer = F.Owner;
								G->PublicPopup(TEXT("TETHER!"), Other->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.5f, 0.85f, 1.f));
								ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.8f, 1.f);
								UE_LOG(LogZoogi, Log, TEXT("SKILL tether: P%d hit - P%d dragged after it"), Hit->PlayerIndex + 1, Other->PlayerIndex + 1);
								bDrop = true;
							}
						}
					}
					else if (F.Kind == NK_Tide)
					{
						if (Nw > F.NextFx) { F.NextFx = Nw + 0.9; for (int32 j = -1; j <= 1; ++j) AZoogiFX::Spawn(Wp, Ac + F.Dir * 600.f * j + FVector(0, 0, 8.f), FLinearColor(0.5f, 0.8f, 1.f), 90.f, 0.6f, true, 5.f); }
						auto Drift = [&](UPrimitiveComponent* Body) { if (Body && Body->IsSimulatingPhysics()) Body->SetPhysicsLinearVelocity(Body->GetPhysicsLinearVelocity() + F.Dir * 75.f * Dtp); };
						for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Drift(B->Sphere);
						for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Drift(O->Sphere);
					}
					else if (F.Kind == NK_Fault)
					{	// b273 FAULT LINE: for 2.5 s everything on the outer side of the crack slides toward the edge
						if (Nw > F.NextFx) bDrop = true;
						else
						{
							auto Tip = [&](UPrimitiveComponent* Body, const FVector& L) { if (Body && Body->IsSimulatingPhysics() && FVector::DotProduct(L - F.P, F.Dir) > 0.f && FVector::Dist2D(L, F.P) < 1400.f) Body->SetPhysicsLinearVelocity(Body->GetPhysicsLinearVelocity() + F.Dir * 420.f * Dtp); };
							for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Tip(B->Sphere, B->BallLocation());
							for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Tip(O->Sphere, O->OrbLocation());
						}
					}
					else if (F.Kind == NK_Magnet)
					{
						AZoogiBall* C = F.B.Get(); if (!C || C->bOut) { bDrop = true; }
						else for (AZoogiOrb* O : G->Orbs)
						{
							if (!O || O->bOut || !O->bStar || !O->Sphere || !O->Sphere->IsSimulatingPhysics()) continue;
							FVector D = C->BallLocation() - O->OrbLocation(); D.Z = 0.f; if (D.Size() < 150.f) continue;
							O->Sphere->SetPhysicsLinearVelocity(O->Sphere->GetPhysicsLinearVelocity() + D.GetSafeNormal() * 140.f * Dtp);
						}
					}
					if (bDrop) NewFx.RemoveAt(k);
				}
			}
			for (int32 k = Patches.Num() - 1; k >= 0; --k) if (Patches[k].Until < Tn) { if (AZoogiFabFX* F = Patches[k].Fx.Get()) F->Destroy(); Patches.RemoveAt(k); }
			Traps.RemoveAll([Tn](const FTrap& X) { return X.Until < Tn; });
			Counters.RemoveAll([Tn](const FCounter& X) { return X.Until < Tn || !X.B.IsValid(); });
			Spins.RemoveAll([Nw](const FSpin& X) { return Nw > X.End || !X.B.IsValid(); });
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || B->bOut || !B->Sphere) continue;
				const FVector V = B->GetBallVelocity();
				const FVector* Pv = PlusPrevVel.Find(B); const FVector V0 = Pv ? *Pv : V; PlusPrevVel.Add(B, V);
				for (const FPlusWall& Pw : PlusWalls)
				{
					FVector Ab = Pw.B - Pw.A; Ab.Z = 0.f; const float L = Ab.Size(); if (L < 10.f) continue;
					const FVector Dn = Ab / L, Nn(-Dn.Y, Dn.X, 0.f), Rel = B->BallLocation() - Pw.A;
					const float Along = FVector::DotProduct(Rel, Dn), Across = FVector::DotProduct(Rel, Nn);
					if (Along < -60.f || Along > L + 60.f || FMath::Abs(Across) > ZoogiConst::ZoogiRadius + 110.f) continue;
					const float Side = Across >= 0.f ? 1.f : -1.f;
					if (FVector::DotProduct(V0, Nn) * Side < -150.f && FVector::DotProduct(V, Nn) * Side > 40.f)
					{	// it just bounced off the wall
						double& Cd = PlusWallCool.FindOrAdd(B); if (Nw - Cd < 0.6) continue; Cd = Nw;
						if (B->PlayerIndex == Pw.Owner) { B->Sphere->SetPhysicsLinearVelocity(V * 1.25f); G->PublicPopup(TEXT("BANK BOOST +25%"), B->BallLocation() + FVector(0, 0, 170.f), FLinearColor(0.6f, 0.9f, 1.f)); }
						else { if (UZoogiCardSubsystem* Cs = UZoogiCardSubsystem::Get(G)) Cs->AddEnergyDebt(B->PlayerIndex, 1); G->PublicPopup(TEXT("-1 SP"), B->BallLocation() + FVector(0, 0, 170.f), FLinearColor(1.f, 0.4f, 0.4f)); }
						AZoogiFX::Spawn(Wp, B->BallLocation(), FLinearColor(0.6f, 0.9f, 1.f), 120.f, 0.3f, true, 10.f);
						UE_LOG(LogZoogi, Log, TEXT("SKILL wall payoff: P%d bounced off P%d's wall (%s)"), B->PlayerIndex + 1, Pw.Owner + 1, B->PlayerIndex == Pw.Owner ? TEXT("+25% speed") : TEXT("-1 SP"));
					}
				}
			}
			for (const FPatch& Pt : Patches)
			{	// slippery: what rolls across keeps its speed
				auto Slide = [&](UPrimitiveComponent* Body, const FVector& L)
				{
					if (!Body || !Body->IsSimulatingPhysics() || FVector::Dist2D(L, Pt.P) > Pt.R) return;
					const FVector V = Body->GetPhysicsLinearVelocity(); const float Sp = V.Size2D();
					if (Sp > 40.f && Sp < 1600.f) Body->SetPhysicsLinearVelocity(FVector(V.X * (1.f + 0.55f * Dtp), V.Y * (1.f + 0.55f * Dtp), V.Z));
				};
				for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Slide(O->Sphere, O->OrbLocation());
				for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Slide(B->Sphere, B->BallLocation());
			}
			for (int32 k = Traps.Num() - 1; k >= 0; --k)
			{
				const FTrap Tr = Traps[k];
				if (Nw < Tr.ArmAt) continue;
				bool bHit = false;
				for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), Tr.P) < Tr.R && B->GetBallVelocity().Size2D() > 60.f) bHit = true;
				for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->Sphere && FVector::Dist2D(O->OrbLocation(), Tr.P) < Tr.R && O->Sphere->GetPhysicsLinearVelocity().Size2D() > 60.f) bHit = true;
				if (!bHit) continue;
				AZoogiFabFX::Play(Wp, TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV06.NS_IceAttackV06"), Tr.P, 0.8f, 6.f);
				int32 Np = 0; { FTuneScope Tt(Tr.Pow, FName(TEXT("frost_spikes"))); Np = Blast(G, Tr.P, 300.f, 1100.f, 650.f, nullptr, Tr.Owner); }
				ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.9f, 1.25f); ZoogiUtil::PlaySfx(G, TEXT("StingFrost"), 0.7f, 1.f);
				G->PublicPopup(TEXT("FROST TRAP!"), Tr.P + FVector(0, 0, 180.f), FLinearColor(0.6f, 0.85f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL frost trap P%d sprung: %d launched"), Tr.Owner + 1, Np);
				Traps.RemoveAt(k);
			}
			for (int32 k = Counters.Num() - 1; k >= 0; --k)
			{
				AZoogiBall* Me = Counters[k].B.Get(); if (!Me || Me->bOut) continue;
				for (AZoogiBall* R : G->Balls)
				{
					if (!R || R == Me || R->bOut || !R->Sphere) continue;
					const FVector D = R->BallLocation() - Me->BallLocation();
					if (D.Size2D() > 2.f * ZoogiConst::ZoogiRadius + 40.f) continue;
					const FVector Dn = D.GetSafeNormal2D();
					if (FVector::DotProduct(R->GetBallVelocity() - Me->GetBallVelocity(), -Dn) < 250.f) continue; // it is ramming me
					R->Sphere->WakeAllRigidBodies();
					R->Sphere->SetPhysicsLinearVelocity((Dn * 1400.f + FVector(0, 0, 300.f)) * FMath::Max(0.6f, Counters[k].Pow));
					R->LastHitPlayer = Me->PlayerIndex;
					AZoogiFabFX::Play(Wp, TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_CrossSlash08.NS_CrossSlash08"), R->BallLocation(), 1.8f, 4.f);
					ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.9f, 1.f);
					G->PublicPopup(TEXT("COUNTER!"), Me->BallLocation() + FVector(0, 0, 190.f), FLinearColor(1.f, 0.45f, 0.15f));
					UE_LOG(LogZoogi, Log, TEXT("SKILL cross slash counter: P%d slashed P%d away"), Me->PlayerIndex + 1, R->PlayerIndex + 1);
					Counters.RemoveAt(k);
					break;
				}
			}
			for (FSpin& Sp : Spins)
			{
				AZoogiBall* B = Sp.B.Get(); if (!B || B->bOut || Nw < Sp.Next) continue;
				Sp.Next = Nw + 0.25;
				const FVector Fl = B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 2.f);
				int32 Np = 0; { FTuneScope Ts(Sp.Pow, FName(TEXT("sword_slash"))); Np = Blast(G, Fl, 240.f, 850.f, 200.f, B, Sp.Pi); }
				AZoogiFX::Spawn(Wp, Fl + FVector(0, 0, 8.f), FLinearColor(0.85f, 0.9f, 1.f), 240.f, 0.22f, true, 9.f);
				ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.5f, 1.3f);
				if (Np) UE_LOG(LogZoogi, Log, TEXT("SKILL sword spin P%d: %d pushed"), Sp.Pi + 1, Np);
			}
		}
		for (int32 i = Craters.Num() - 1; i >= 0; --i)
		{
			if (Craters[i].Turn != G->TurnCounter) { Craters.RemoveAt(i); continue; }
			const float Dt = G->GetWorld()->GetDeltaSeconds();
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->Sphere && FVector::Dist2D(B->BallLocation(), Craters[i].P) < Craters[i].R) B->Sphere->SetPhysicsLinearVelocity(B->GetBallVelocity() * FMath::Max(0.f, 1.f - 1.6f * Dt));
		}
		for (int32 i = Buffs.Num() - 1; i >= 0; --i)
		{
			AZoogiBall* B = Buffs[i].B.Get();
			if (!B || Buffs[i].Turn != G->TurnCounter) { if (B && B->Sphere && Buffs[i].SavedMass > 0.f) B->Sphere->SetMassOverrideInKg(NAME_None, Buffs[i].SavedMass, true); Buffs.RemoveAt(i); continue; }
			if (Buffs[i].bVelPending && G->Phase == EZoogiPhase::Rolling && G->GetActiveBall() == B && B->GetBallVelocity().Size2D() > 50.f) { Buffs[i].bVelPending = false; Kick(B->Sphere, B->GetBallVelocity() * 0.2f); }
		}
		for (int32 i = Shields.Num() - 1; i >= 0; --i)
		{
			FShield& Sh = Shields[i]; AZoogiBall* B = Sh.B.Get();
			if (!B || B->bOut || !B->Sphere) { DropShieldFx(Sh); Shields.RemoveAt(i); continue; }
			AZoogiBall* Act = G->GetActiveBall();
			const bool bEnemyRoll = Act && Act != B && G->Phase == EZoogiPhase::Rolling;
			if (bEnemyRoll && !Sh.bHeavy) { Sh.bHeavy = true; B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass * 6.f, true); }
			if (!bEnemyRoll && Sh.bHeavy && Sh.HitAt < 0.f) { Sh.bHeavy = false; B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); }
			if (bEnemyRoll && Sh.HitAt < 0.f && FVector::Dist(Act->BallLocation(), B->BallLocation()) < 2.f * ZoogiConst::ZoogiRadius + 18.f)
			{
				Sh.HitAt = T;
				if (WoodStays()) AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor(0.55f, 0.85f, 0.35f), 180.f, 0.3f, false, 10.f); // b96b: a flash, the shield stays closed
				else if (AZoogiFabFX* F = AZoogiFabFX::Play(G->GetWorld(), FX(TEXT("NS_Wood_shield")), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius), 0.3f, 1.6f, B, FVector(0, 0, -ZoogiConst::ZoogiRadius))) F->StopAfter = 1.4f; // b71 (Grok)
				UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield on P%d took the hit"), B->PlayerIndex + 1);
			}
			if (WoodStays())
			{	// b90b: it stays up - glow refreshed, a hit only flashes it - until the caster's own next turn
				// b96b (Evan): no replay - the forming plays once, then it is paused fully closed
				if (!Sh.bFrozen && FPlatformTime::Seconds() - Sh.CastAt > 0.9) { Sh.bFrozen = true; if (AZoogiFabFX* F = Sh.Fx.Get()) if (F->NC) F->NC->SetPaused(true); }
				if (Sh.HitAt >= 0.f && T - Sh.HitAt > 0.6f) Sh.HitAt = -1.f;
				if (WoodLock() && G->ActivePlayer != B->PlayerIndex)
				{	// b177k (Evan: "the wood shield can't move no matter what"): once it settles it is a fixed wall - shots bounce off it,
					// blasts and pushes don't move it - until this Zoogi's own next turn
					if (!Sh.bLocked && (B->GetBallVelocity().Size() < 40.f || FPlatformTime::Seconds() - Sh.CastAt > 2.0))
					{
						Sh.bLocked = true; Sh.Anchor = B->Sphere->GetComponentLocation();
						B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
						B->Sphere->SetSimulatePhysics(false);
						UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield: P%d locked in place"), B->PlayerIndex + 1);
					}
					if (Sh.bLocked && FVector::DistSquared(B->Sphere->GetComponentLocation(), Sh.Anchor) > 1.f) B->Sphere->SetWorldLocation(Sh.Anchor, false, nullptr, ETeleportType::TeleportPhysics);
				}
				if (G->ActivePlayer == B->PlayerIndex && G->TurnCounter > Sh.Turn) { if (Sh.bLocked) { B->Sphere->SetSimulatePhysics(true); Sh.bLocked = false; } B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield on P%d drops (its turn)"), B->PlayerIndex + 1); DropShieldFx(Sh); Shields.RemoveAt(i); }
				continue;
			}
			if (Sh.HitAt >= 0.f && T - Sh.HitAt > 0.5f) { B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); DropShieldFx(Sh); Shields.RemoveAt(i); }
		}
		(void)bNewTurn; (void)RealDt;
		// test: -ZoogiSkillTest casts every skill in turn (3.2 s apart) for the Zoogi whose turn it is
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillTest"));
		static float NextTest = 6.f; static int32 TestIdx = 0;
		static FString OnlySkill; static bool bOnlyRead = false; if (!bOnlyRead) { bOnlyRead = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiSkillOnly="), OnlySkill); }
		static TArray<FString> OnlyList; if (OnlyList.Num() == 0 && !OnlySkill.IsEmpty()) OnlySkill.ParseIntoArray(OnlyList, TEXT(","));
		if (bTest && !OnlySkill.IsEmpty() && !Picking() && G->GetActiveBall() && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && T >= NextTest && TestIdx < FMath::Max(4, OnlyList.Num()))
		{	// b97a: -ZoogiSkillOnly=<id> - that skill only, every 6 s, frames through the effect; b101: "a+b" = a combo (b 1.8 s after a)
			NextTest = T + 7.f; ++TestIdx;
			const FString ThisCast = OnlyList.Num() ? OnlyList[(TestIdx - 1) % OnlyList.Num()] : OnlySkill; // b101b: "a,b,c" = one per cast in turn
			UE_LOG(LogZoogi, Log, TEXT("SKILL_TEST only %s #%d"), *ThisCast, TestIdx);
			TArray<FString> Combo; ThisCast.ParseIntoArray(Combo, TEXT("+"));
			ZoogiFilm::Begin(FString::Printf(TEXT("skill_%s"), *ThisCast.Replace(TEXT("+"), TEXT("_"))), Combo.Num() > 1 ? 8.5f : 6.5f);
			if (Combo.Num() > 1) NextTest = T + 9.f;
			if (Combo.Num() > 0) AutoCast(G, FName(*Combo[0]));
			for (int32 c = 1; c < Combo.Num(); ++c) { const FName Nx(*Combo[c]); TWeakObjectPtr<AZoogiGameMode> WGc = G; Later(G, 1.8f * c, [WGc, Nx]() { if (AZoogiGameMode* Gc = WGc.Get()) { ZoogiFilm::Mark(Nx.ToString()); AutoCast(Gc, Nx); } }); }
			if (!G->ShotDir.IsEmpty() && !ZoogiFilm::On()) for (float St : { 0.3f, 0.6f, 0.9f, 1.15f, 1.4f, 1.7f, 2.2f }) G->TakeShotDelayed(FString::Printf(TEXT("only_%s_%d_%03d"), *ThisCast, TestIdx, FMath::RoundToInt(St * 100.f)), St);
		}
		if (bTest && OnlySkill.IsEmpty() && !Picking() && TestIdx < (int32)UE_ARRAY_COUNT(Defs) && G->GetActiveBall() && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && T >= NextTest)
		{
			NextTest = T + 3.2f;
			const FName Id(Defs[TestIdx++].Id);
			UE_LOG(LogZoogi, Log, TEXT("SKILL_TEST cast %s"), *Id.ToString());
			AutoCast(G, Id);
		}
	}

	bool EffectBusy() { const double D = FPlatformTime::Seconds() - GLastCastAt; return D < 0.7 || (D < 2.5 && Events.Num() > 0); } // b190 FAST FX: was 1.6 / 4.0
	bool CpuPicking() { return Picking() && S.bCpu; }
	void CpuCast(AZoogiGameMode* G, FName Id)
	{
		if (Id == FName(TEXT("skill_lab"))) return; // b107: a CPU never opens the lab
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B || !IsSkill(Id)) return;
		const FDef* D = Find(Id);
		Reset();
		S.Id = Id; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius; S.Range = FMath::Max(400.f, G->ArcRange()) * RangeMul(Id); // b178k
		if (D->Aim == EAim::Self) { S.bOn = true; S.bConfirmed = true; Execute(G, B, Id); return; }
		TArray<AActor*> Cand; Candidates(G, Cand);
		// a person picks rivals first, then whatever is closest
		Cand.Sort([&](const AActor& X, const AActor& Y)
		{
			const float Sx = FVector::Dist2D(ActorPos(const_cast<AActor*>(&X)), S.Origin) - (Cast<AZoogiBall>(&X) ? 600.f : 0.f);
			const float Sy = FVector::Dist2D(ActorPos(const_cast<AActor*>(&Y)), S.Origin) - (Cast<AZoogiBall>(&Y) ? 600.f : 0.f);
			return Sx < Sy;
		});
		const bool bSmart = ZoogiCPUSmart::On();
		if (bSmart)
		{	// b124: never a teammate; rivals near their edge / the leader first
			Cand.RemoveAll([&](AActor* X) { const AZoogiBall* Xb = Cast<AZoogiBall>(X); return Xb && G->IsAlly(B, Xb); });
			Cand.Sort([&](const AActor& X, const AActor& Y) { return ZoogiCPUSmart::TapRank(G, B, const_cast<AActor*>(&X)) < ZoogiCPUSmart::TapRank(G, B, const_cast<AActor*>(&Y)); });
		}
		FVector Fwd = Cand.Num() ? ActorPos(Cand[0]) : S.Origin + FVector(S.Range * 0.5f, 0, 0);
		if (bSmart && (D->Aim == EAim::Area || D->Aim == EAim::Spots) && Cand.Num() > 1)
		{	// b124: area skills land on the biggest bunch (rivals count double)
			float BestN = -1.f;
			for (AActor* X : Cand)
			{
				FVector Sum = FVector::ZeroVector; float N = 0.f, W = 0.f;
				for (AActor* Y : Cand) if (FVector::Dist2D(ActorPos(X), ActorPos(Y)) < FMath::Max(250.f, D->R * 0.8f)) { const float Wy = Cast<AZoogiBall>(Y) ? 2.f : 1.f; N += Wy; Sum += ActorPos(Y) * Wy; W += Wy; }
				if (N > BestN) { BestN = N; Fwd = Sum / FMath::Max(W, 1.f); }
			}
		}
		S.CpuCtr = ClampRange(Fwd);
		S.CpuA = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f + FVector(0, 250.f, 0)); S.CpuB = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f - FVector(0, 250.f, 0));
		if (bSmart && D->Aim == EAim::Line)
		{	// b124: a person walls off their OWN nearest edge (a backstop), not the line of their own shot
			FVector2D Ed(1.f, 0.f);
			const float E = ZoogiCPUSmart::EdgeDist(G, FVector2D(S.Origin), &Ed);
			const float Off = FMath::Clamp(E * 0.65f, 160.f, FMath::Min(S.Range * 0.8f, 650.f));
			const FVector Ctr = S.Origin + FVector(Ed.X, Ed.Y, 0.f) * Off, Perp(-Ed.Y, Ed.X, 0.f);
			S.CpuA = ClampRange(Ctr + Perp * 280.f); S.CpuB = ClampRange(Ctr - Perp * 280.f);
		}
		bool bBlock = false;
		if (bSmart && D->Aim == EAim::Line && ZoogiCPUSmart::IsEarthBlock(Id))
		{	// b244 (Evan: "block opposing player with earth bending"): the wall rises across the nearest rival's way to the orbs
			AZoogiBall* Rv = nullptr; float Rd = 1e9f;
			for (AZoogiBall* X : G->Balls) if (X && X != B && !X->bOut && !G->IsAlly(B, X)) { const float Dd = FVector::Dist2D(X->BallLocation(), S.Origin); if (Dd < Rd) { Rd = Dd; Rv = X; } }
			if (Rv)
			{
				const FVector Rp = Rv->BallLocation();
				FVector Goal = G->Arena ? G->Arena->GetActorLocation() : S.Origin; float Od = 1e9f;
				for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float Dd = FVector::Dist2D(O->OrbLocation(), Rp); if (Dd < Od) { Od = Dd; Goal = O->OrbLocation(); } }
				FVector Dir = (Goal - Rp).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = (S.Origin - Rp).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
				const FVector Ctr = Rp + Dir * FMath::Clamp(Od * 0.5f, 180.f, 320.f), Perp(-Dir.Y, Dir.X, 0.f);
				S.CpuA = ClampRange(Ctr + Perp * 300.f); S.CpuB = ClampRange(Ctr - Perp * 300.f);
				bBlock = true;
				G->PublicPopup(FString::Printf(TEXT("%s: EARTH WALL - BLOCKED!"), *B->Def.Name.ToUpper()), Rp + FVector(0.f, 0.f, 150.f), FLinearColor(0.55f, 0.85f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL cpu P%d EARTH BLOCK %s in front of P%d (toward the orb at %.0f, %.0f)"), B->PlayerIndex + 1, *Id.ToString(), Rv->PlayerIndex + 1, Goal.X, Goal.Y);
			}
		}
		if (D->Aim == EAim::Line && IsPackLine(Id) && !bBlock)
		{	// b176e: attack lines run from the caster through the target
			FVector Dir = (Fwd - S.Origin).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			S.CpuA = ClampRange(S.Origin + Dir * 70.f); S.CpuB = ClampRange(Fwd + Dir * 160.f);
		}
		for (int32 k = 0; k < Cand.Num() && k < D->Count; ++k) S.CpuTargets.Add(Cand[k]);
		for (int32 k = 0; k < D->Count; ++k) S.CpuSpots.Add(ClampRange(Fwd + FVector(FMath::Cos(k * 2.1f), FMath::Sin(k * 2.1f), 0.f) * 220.f));
		S.Ctr = S.A = S.B = S.Origin;
		S.bOn = true; S.bCpu = true; S.OpenedAt = FPlatformTime::Seconds();
		const int32 N = (S.Aim == EAim::Tap || S.Aim == EAim::MultiTap) ? S.CpuTargets.Num() : (S.Aim == EAim::Spots ? S.CpuSpots.Num() : 1);
		S.CpuFireAt = bBlock ? S.OpenedAt + 0.35 + FMath::FRandRange(0.f, 0.15f) /* b244: a quick reaction, still rolling */ : S.OpenedAt + 1.5 + 0.35 * FMath::Max(0, N - 1) + FMath::FRandRange(0.f, 0.5f);
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.3f);
		if (!G->ShotDir.IsEmpty()) { static int32 Nsp = 0; if (Nsp++ < 6) G->TakeShotDelayed(FString::Printf(TEXT("skillpick_%s"), *Id.ToString()), 0.6f); } // b178g: the HOW TO USE card
		UE_LOG(LogZoogi, Log, TEXT("SKILL cpu P%d pick %s: aim %d, %d targets"), B->PlayerIndex + 1, *Id.ToString(), (int32)S.Aim, S.CpuTargets.Num());
	}

	// ---- b77 wolf button
	static TWeakObjectPtr<const AZoogiGameMode> GWolfG;
	static TMap<int32, int32> GWolfArmed; // player -> summons banked
	void ArmWolf(AZoogiGameMode* G, int32 Player)
	{
		if (GWolfG.Get() != G) { GWolfArmed.Reset(); GWolfG = G; }
		++GWolfArmed.FindOrAdd(Player);
		UE_LOG(LogZoogi, Log, TEXT("WOLF button armed for P%d (%d banked)"), Player + 1, GWolfArmed[Player]);
	}
	int32 WolfCount(const AZoogiGameMode* G, int32 Player) { const int32* N = (G && GWolfG.Get() == G) ? GWolfArmed.Find(Player) : nullptr; return N ? *N : 0; }
	bool WolfArmed(const AZoogiGameMode* G, int32 Player) { return WolfCount(G, Player) > 0; }
	bool WolfSlot(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B || B->bCPU || B->bOut || !WolfArmed(G, B->PlayerIndex)) return false;
		return (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle) || G->Phase == EZoogiPhase::Rolling;
	}
	bool WolfReady(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		const bool bAimNow = G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle; // b119c (Evan): "activated while aiming or rolling"
		if (!(B && !B->bOut && (G->Phase == EZoogiPhase::Rolling || bAimNow) && WolfArmed(G, B->PlayerIndex))) return false;
		const UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G);
		return !(WP && (WP->IsPainting() || (WP->PackActive() && WP->PackOwnerIndex() == B->PlayerIndex))); // b87: own pack out -> SUPERPOWER recalls it
	}
	bool WolfGo(AZoogiGameMode* G)
	{
		if (!WolfReady(G)) return false;
		AZoogiBall* B = G->GetActiveBall();
		// b87 (Evan: "fix Wolf Pack ... if I have multiple I should be able to select targets up to how many I have"): one press
		// spends every banked wolf (-ZoogiWolfOnePerPress = one per press); a rival's pack still out is cleared first (it used to
		// swallow the summon - bank spent, no wolves)
		static const bool bOnePer = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOnePerPress"));
		int32 Spend = 1;
		if (int32* N = GWolfArmed.Find(B->PlayerIndex)) { Spend = bOnePer ? 1 : FMath::Max(1, *N); *N -= Spend; if (*N <= 0) GWolfArmed.Remove(B->PlayerIndex); }
		if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G)) { if (WP->PackActive()) WP->ForceClear(); WP->BankedWolves = Spend; }
		UE_LOG(LogZoogi, Log, TEXT("WOLF button: P%d summons the pack mid-roll (%d banked wolves)"), B->PlayerIndex + 1, Spend);
		G->FireWolfSkill(B);
		return true;
	}
	// ---- b80 bolt balls
	struct FBoltShot { TWeakObjectPtr<AActor> Vis; FVector P = FVector::ZeroVector, D = FVector::ForwardVector; float Age = 0.f, Spark = 0.f; int32 Owner = -1; float GroundZ = -1e9f; bool b3D = false; bool bStop = false; FVector Stop = FVector::ZeroVector; }; // b152a: GroundZ / b3D - aimed down at the floor
	static TWeakObjectPtr<const AZoogiGameMode> GBoltG;
	static TMap<int32, int32> GBolts;
	static TArray<FBoltShot> GBoltShots;
	static float GStemT = 0.f;
	// b81 (Evan: "Bolt Balls look amazing but still not the intended purple lightning"): KriptoFX-style purple ground-strike arcs,
	// generated live: jagged cylinder chains from the Zoogi's top arcing down to the floor, re-struck every 50 ms
	static const FLinearColor GBoltPurple(0.62f, 0.32f, 1.f);
	struct FStem { TWeakObjectPtr<AActor> A; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f; };
	static TMap<int32, FStem> GStems;
	constexpr int32 StemSegs = 9, StemMax = 3;
	static FStem MakeStem(UWorld* W, const FVector& At)
	{
		FStem St;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), SP);
		if (!A) return St;
		USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Movable);
		A->SetRootComponent(Root); Root->RegisterComponent();
		UMaterialInterface* Core = ZoogiUtil::FxMID(A, FLinearColor(0.85f, 0.7f, 1.f), 40.f);
		UMaterialInterface* Glow = ZoogiUtil::FxMID(A, GBoltPurple, 18.f);
		UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder"));
		for (int32 i = 0; i < StemMax * StemSegs * 2; ++i)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
			C->SetStaticMesh(Cyl); C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
			C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
			C->SetupAttachment(Root); C->RegisterComponent();
			C->SetMaterial(0, (i % 2) ? Glow : Core);
			C->SetVisibility(false);
			St.Segs.Add(C);
		}
		St.A = A;
		return St;
	}
	static void SetSeg(UStaticMeshComponent* C, const FVector& P0, const FVector& P1, float Th)
	{
		const FVector D = P1 - P0; const float L = D.Size();
		if (!C || L < 1.f) { if (C) C->SetVisibility(false); return; }
		C->SetVisibility(true);
		C->SetWorldLocationAndRotation((P0 + P1) * 0.5f, FRotationMatrix::MakeFromZ(D / L).Rotator());
		C->SetWorldScale3D(FVector(Th / 100.f, Th / 100.f, L / 100.f));
	}
	static void StrikeStems(UWorld* W, FStem& St, const AZoogiBall* B, int32 N, float Tm)
	{
		const FVector Top = B->BallLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.55f);
		const float FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius + 2.f;
		for (int32 k = 0; k < StemMax; ++k)
		{
			const bool bOn = k < N;
			const float Ang = FMath::DegreesToRadians(Tm * 40.f + k * 120.f + FMath::FRandRange(-12.f, 12.f));
			const float Reach = FMath::FRandRange(150.f, 210.f);
			const FVector End(B->BallLocation().X + FMath::Cos(Ang) * Reach, B->BallLocation().Y + FMath::Sin(Ang) * Reach, FloorZ);
			const FVector Mid = (Top + End) * 0.5f + FVector(0.f, 0.f, FMath::FRandRange(70.f, 120.f));
			FVector Prev = Top;
			for (int32 i = 1; i <= StemSegs; ++i)
			{
				const float t = (float)i / StemSegs;
				FVector Pt = FMath::Lerp(FMath::Lerp(Top, Mid, t), FMath::Lerp(Mid, End, t), t); // quadratic arc
				if (i < StemSegs) Pt += FVector(FMath::FRandRange(-16.f, 16.f), FMath::FRandRange(-16.f, 16.f), FMath::FRandRange(-12.f, 12.f));
				UStaticMeshComponent* CoreC = St.Segs.IsValidIndex((k * StemSegs + i - 1) * 2) ? St.Segs[(k * StemSegs + i - 1) * 2].Get() : nullptr;
				UStaticMeshComponent* GlowC = St.Segs.IsValidIndex((k * StemSegs + i - 1) * 2 + 1) ? St.Segs[(k * StemSegs + i - 1) * 2 + 1].Get() : nullptr;
				if (bOn) { SetSeg(CoreC, Prev, Pt, 2.6f); SetSeg(GlowC, Prev, Pt, 7.5f); }
				else { if (CoreC) CoreC->SetVisibility(false); if (GlowC) GlowC->SetVisibility(false); }
				Prev = Pt;
			}
			if (bOn && FMath::FRand() < 0.35f) AZoogiFX::Spawn(W, End + FVector(0.f, 0.f, 4.f), GBoltPurple, 70.f, 0.18f, true, 10.f); // ground strike flash
		}
	}
	// ---- b120g COMBO: armed skills, cast one per SUPERPOWER tap after the launch
	static TWeakObjectPtr<const AZoogiGameMode> GComboG;
	static TMap<int32, TArray<FName>> GCombo;
	static TMap<int32, int32> GComboTotal, GComboDone;
	static void ComboResetFor(const AZoogiGameMode* G) { if (GComboG.Get() != G) { GCombo.Reset(); GComboTotal.Reset(); GComboDone.Reset(); GComboG = G; } }
	void ComboArm(AZoogiGameMode* G, int32 Player, FName Id)
	{
		if (!G) return;
		ComboResetFor(G);
		TArray<FName>& Q = GCombo.FindOrAdd(Player);
		if (Q.Num() == 0) { GComboTotal.Add(Player, 0); GComboDone.Add(Player, 0); }
		Q.Add(Id); GComboTotal.FindOrAdd(Player) += 1;
		UE_LOG(LogZoogi, Log, TEXT("COMBO P%d armed %s (%d waiting)"), Player + 1, *Id.ToString(), Q.Num());
	}
	void WorldFireMeteors(AZoogiGameMode* G, const FVector& At, float R)
	{	// b139a: the old FIRE METEORS effect as a world event - nobody's credit, full strength
		if (!G || !G->GetWorld()) return;
		UWorld* W = G->GetWorld();
		AZoogiFabFX::Play(W, FX(TEXT("NS_Fire_meteors")), At, Sc(FName(TEXT("fire_meteors")), 0.3f), 3.5f);
		AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), At, 1.f);
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		for (int32 k = 0; k < 3; ++k)
		{
			const float A = 2.f * PI * k / 3.f + 0.4f;
			const FVector Hit = At + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * R * 0.45f;
			Later(G, 0.6f + 0.22f * k, [WG, Hit, k]() { if (AZoogiGameMode* Gm = WG.Get()) { FTuneScope Tn(1.f, FName(TEXT("fire_meteors"))); static const bool bOrbsToo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVolcanoHitsOrbs")); if (bOrbsToo) Blast(Gm, Hit, 260.f, 820.f, 300.f, nullptr, -1); else VolcanoBlast(Gm, Hit, 260.f * ZoogiSkillLab::Radius(), 820.f, 300.f); AZoogiFabFX::Play(Gm->GetWorld(), FX(k % 2 ? TEXT("NS_Meteor_hit") : TEXT("NS_Meteor_hit_2")), Hit, Sc(FName(TEXT("meteor_hit")), 0.22f), 2.5f); ZoogiHaptics::Kick(0.4f, Gm->GetWorld(), TEXT("volcano_meteor")); } });
		}
		UE_LOG(LogZoogi, Log, TEXT("VOLCANO meteor strike at (%.0f, %.0f)"), At.X, At.Y);
	}
	bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName W)
	{
		if (!G || !B || Picking() || ZoogiItems::UsedThisTurn(G, B->PlayerIndex) || ZoogiItems::Count(G, B->PlayerIndex, W) <= 0) return false;
		if (W == FName(TEXT("grenade"))) return BeginGrenade(G, B);
		const FName Sk = W == FName(TEXT("apple")) ? FName(TEXT("golden_apple")) : W == FName(TEXT("sword")) ? FName(TEXT("sword_slash")) : W == FName(TEXT("staff")) ? FName(TEXT("water_cannon")) : W == FName(TEXT("gatling")) ? FName(TEXT("gattling_turret")) : FName(TEXT("halo_disc"));
		const FDef* D = Find(Sk);
		if (!D) return false;
		Reset();
		S.Slot = -1; S.Id = Sk; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B; S.bWeapon = true; S.WeaponId = W;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
		S.Range = FMath::Max(400.f, G->ArcRange());
		if (D->Aim == EAim::Self)
		{	// the sword: straight away
			if (!ZoogiItems::Take(G, B->PlayerIndex, W)) { Reset(); return false; }
			S.bOn = true; S.bConfirmed = true; GAwakenNext = true;
			UE_LOG(LogZoogi, Log, TEXT("ITEM %s used (%s)"), *W.ToString(), *Sk.ToString());
			Execute(G, B, Sk);
			return true;
		}
		S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
		FVector Aim = S.Origin + FRotator(0.f, B->AimYaw, 0.f).Vector() * S.Range * 0.5f;
		float Best = 1e9f;
		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < Best) { Best = Dd; Aim = O->BallLocation(); } }
		S.Ctr = ClampRange(Aim, 0.f); S.A = S.B = S.Ctr;
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("ITEM pick open %s P%d (%s)"), *W.ToString(), B->PlayerIndex + 1, *Sk.ToString());
		return true;
	}
	bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B)
	{
		const FName Sw(TEXT("energy_shockwave"));
		const FDef* Dg = Find(Sw);
		if (!G || !B || !Dg || Picking() || ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("grenade"))) <= 0 || ZoogiItems::UsedThisTurn(G, B->PlayerIndex)) return false; // b137: saved grenades too
		Reset();
		S.Slot = -1; S.Id = Sw; S.Aim = EAim::Area; S.R = Dg->R; S.Count = 1; S.Caster = B; S.bGrenade = true;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
		S.Range = 2.f * FMath::Max(400.f, G->ArcRange()); // b130 (Evan): "the arc line is 2x farther than normal"
		S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
		FVector AimAt = S.Origin + FRotator(0.f, B->AimYaw, 0.f).Vector() * S.Range * 0.5f;
		float BestD = 1e9f;
		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < BestD) { BestD = Dd; AimAt = O->BallLocation(); } }
		S.Ctr = ClampRange(AimAt, 0.f); S.A = S.B = S.Ctr;
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("GRENADE pick open P%d (range %.0f)"), B->PlayerIndex + 1, S.Range);
		return true;
	}
	void GrenadeBoom(AZoogiGameMode* G, AZoogiBall* B, const FVector& At)
	{
		const FName Sw(TEXT("energy_shockwave"));
		const FDef* Dg = Find(Sw);
		if (!G || !B || !Dg) return;
		const FPick Saved = S; // a pick the player opened meanwhile survives the blast
		Reset();
		S.Slot = -1; S.Id = Sw; S.Aim = EAim::Area; S.R = Dg->R; S.Count = 1; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = At.Z; S.Ctr = At; S.A = S.B = At;
		S.bOn = true; S.bConfirmed = true;
		Execute(G, B, Sw);
		S = Saved;
	}
	void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id)
	{
		if (!G || GComboG.Get() != G) return;
		GCombo.FindOrAdd(Player).Insert(Id, 0);
		if (int32* D = GComboDone.Find(Player)) *D = FMath::Max(0, *D - 1);
		if (!GComboTotal.Contains(Player)) { GComboTotal.Add(Player, GCombo[Player].Num()); GComboDone.Add(Player, 0); }
		UE_LOG(LogZoogi, Log, TEXT("COMBO P%d %s back in the queue (%d waiting)"), Player + 1, *Id.ToString(), GCombo[Player].Num());
	}
	TArray<FName> ComboQueue(const AZoogiGameMode* G, int32 Player) { const TArray<FName>* Q = (G && GComboG.Get() == G) ? GCombo.Find(Player) : nullptr; return Q ? *Q : TArray<FName>(); } // b276
	bool ComboGoAt(AZoogiGameMode* G, int32 Index)
	{	// b276 (Evan: pick which combo skill to cast during a reaction pause)
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B || GComboG.Get() != G) return false;
		TArray<FName>* Q = GCombo.Find(B->PlayerIndex);
		if (!Q || !Q->IsValidIndex(Index)) return false;
		if (Index > 0) { const FName Id = (*Q)[Index]; Q->RemoveAt(Index); Q->Insert(Id, 0); }
		return ComboGo(G);
	}
	int32 ComboCount(const AZoogiGameMode* G, int32 Player) { const TArray<FName>* Q = (G && GComboG.Get() == G) ? GCombo.Find(Player) : nullptr; return Q ? Q->Num() : 0; }
	int32 ComboTotal(const AZoogiGameMode* G, int32 Player) { const int32* T = (G && GComboG.Get() == G) ? GComboTotal.Find(Player) : nullptr; return T ? *T : 0; }
	bool ComboReady(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		static const bool bComboFirst = FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboFirst"));
		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && !Picking() && ComboCount(G, B->PlayerIndex) > 0 && (bComboFirst || !PowerLeft(G, B->PlayerIndex)); // b128: bolts / wolves first
	}
	bool PowerLeft(const AZoogiGameMode* G, int32 Player)
	{
		if (!G) return false;
		if (BoltCount(G, Player) > 0 || WolfArmed(G, Player)) return true;
		const UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G);
		return WP && WP->PackOwnerIndex() == Player && (WP->IsPainting() || WP->PackActive());
	}
	bool ComboGo(AZoogiGameMode* G)
	{
		if (!ComboReady(G)) return false;
		AZoogiBall* B = G->GetActiveBall();
		TArray<FName>& Q = GCombo.FindOrAdd(B->PlayerIndex);
		const FName Id = Q[0]; Q.RemoveAt(0);
		const int32 Nth = ++GComboDone.FindOrAdd(B->PlayerIndex);
		bool bOk = false;
		const FDef* Dc = Find(Id);
		if (Id == FName(TEXT("__awaken"))) bOk = ZoogiShowtime::FireArmed(G); // b195: AWAKEN in its combo place
		else if (Id == FName(TEXT("__showtime"))) bOk = ZoogiGattling::StartFromMenu(G, true); // b195: SHOWTIME in its combo place (mid-roll)
		else if (Id == FName(TEXT("precision")) && OldPrecision()) { if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(G)) { Wp->BeginPrecision(G, B, 3); bOk = true; } }
		else if (Dc && Dc->Aim != EAim::Self && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboAutoAim")))
		{	// b122c (Evan: "skills that need target selection, a line drawn or an area selected - that still needs to happen"):
			// the normal pick from where the Zoogi is now (the world waits); FIRE casts it, CANCEL puts it back in the queue
			Reset();
			S.Slot = -1; S.Id = Id; S.Aim = Dc->Aim; S.R = Dc->R; S.Count = Dc->Count; S.Caster = B; S.bComboCast = true;
			S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
			S.Range = FMath::Max(400.f, G->ArcRange());
			S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
			FVector AimAt = S.Origin + B->GetBallVelocity().GetSafeNormal2D() * S.Range * 0.5f;
			float BestD = 1e9f;
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < BestD) { BestD = Dd; AimAt = O->BallLocation(); } }
			S.Ctr = ClampRange(AimAt, 0.f); S.A = S.B = S.Ctr;
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.3f);
			bOk = true;
		}
		else bOk = AutoCast(G, Id); // self skills: at once, from where the Zoogi is now
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("CAST %d!"), Nth), Id.ToString().Replace(TEXT("_"), TEXT(" ")).TrimStartAndEnd().ToUpper(), FLinearColor(1.f, 0.82f, 0.15f), 1.2f, 1.f);
		ZoogiHaptics::Kick(0.6f, G->GetWorld(), TEXT("combo cast"));
		UE_LOG(LogZoogi, Log, TEXT("COMBO P%d cast %d: %s (%s, %d left)"), B->PlayerIndex + 1, Nth, *Id.ToString(), bOk ? TEXT("ok") : TEXT("failed"), Q.Num());
		if (Q.Num() == 0) { GComboTotal.Remove(B->PlayerIndex); GComboDone.Remove(B->PlayerIndex); }
		return true;
	}

	void ArmBolts(AZoogiGameMode* G, int32 Player, int32 N)
	{
		if (GBoltG.Get() != G) { GBolts.Reset(); GBoltShots.Reset(); GBoltG = G; }
		GBolts.FindOrAdd(Player) += N;
		UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d has %d bolts"), Player + 1, GBolts[Player]);
	}
	int32 BoltCount(const AZoogiGameMode* G, int32 Player) { const int32* N = (G && GBoltG.Get() == G) ? GBolts.Find(Player) : nullptr; return N ? *N : 0; }
	bool BoltReady(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		// b119b (Evan: "Bolt Balls can be used before launch"): while aiming (the battle step) as well as rolling
		const bool bAimNow = G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle && !ZoogiSkills::Picking();
		return B && !B->bOut && (G->Phase == EZoogiPhase::Rolling || bAimNow) && BoltCount(G, B->PlayerIndex) > 0;
	}
	bool BoltGo(AZoogiGameMode* G)
	{
		if (!BoltReady(G)) return false;
		AZoogiBall* B = G->GetActiveBall();
		if (int32* N = GBolts.Find(B->PlayerIndex)) { if (--(*N) <= 0) GBolts.Remove(B->PlayerIndex); }
		return BoltFireAt(G, B, B->BallLocation(), nullptr);
	}
	// b195 (Evan: "Gattling burst / rapid should look and work like Bolt Balls"): one Bolt Balls shot from From (the Zoogi, or a
	// turret), at Prefer (or the closest thing in reach) - the same purple bolt, the same blast where it hits
	bool BoltFireAt(AZoogiGameMode* G, AZoogiBall* B, const FVector& From, AActor* Prefer, const FVector* AimPt)
	{
		if (!G || !B) return false;
		if (GBoltG.Get() != G) { GBolts.Reset(); GBoltShots.Reset(); GBoltG = G; }
		UWorld* W = G->GetWorld();
		// straight at the nearest rival (else the nearest orb, else along the roll)
		AActor* Best = nullptr; float BestD = 1e12f;
		static const bool bOldAim = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoltAim"));
		if (bOldAim)
		{
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::DistSquared2D(O->BallLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
			if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		}
		else
		{	// b128 (Evan: "an orb right next to the player - bolt balls doesn't see it"): the closest thing in reach, rivals favoured
			const float Reach = FMath::Max(900.f, G->ArcRange());
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::Dist2D(O->BallLocation(), From); if (D < Reach && D * 0.7f < BestD) { BestD = D * 0.7f; Best = O; } }
			for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::Dist2D(O->OrbLocation(), From); if (D < Reach && D < BestD) { BestD = D; Best = O; } }
			if (!Best) for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::Dist2D(O->BallLocation(), From); if (D < BestD) { BestD = D; Best = O; } } // nothing in reach: the nearest rival
		}
		if (G->TargetLock.IsValid() && !G->bSoftLock && G->TargetLock.Get() != B) Best = G->TargetLock.Get(); // b120: the target picked while aiming
		if (Prefer) Best = Prefer; // b195
		if (AimPt) Best = nullptr; // b200: straight at the tapped circle
		FVector Dir = AimPt ? (*AimPt - From).GetSafeNormal2D() : (Best ? (Best->GetActorLocation() - From).GetSafeNormal2D() : B->GetBallVelocity().GetSafeNormal2D());
		if (Dir.IsNearlyZero()) Dir = FRotator(0.f, B->AimYaw, 0.f).Vector();
		FBoltShot Bs; Bs.P = bOldAim ? From + Dir * (ZoogiConst::ZoogiRadius + 20.f) : From; Bs.D = Dir; Bs.Owner = B->PlayerIndex; // b128: from the Zoogi's centre (the path check covers the first metre)
		static const bool bFlat = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFlatBolts"));
		if (!bFlat && !bOldAim)
		{	// b152a (Evan): always at ground level - from the air the bolt flies down to its target on the floor
			FVector T; float Gz;
			if (Best)
			{
				T = Best->GetActorLocation();
				if (const AZoogiBall* Tb = Cast<AZoogiBall>(Best)) { T = Tb->BallLocation(); Gz = T.Z - ZoogiConst::ZoogiRadius; }
				else if (const AZoogiOrb* To = Cast<AZoogiOrb>(Best)) { T = To->OrbLocation(); Gz = T.Z - ZoogiConst::OrbRadius; }
				else Gz = T.Z - 50.f;
			}
			else if (AimPt) { T = *AimPt; Gz = AimPt->Z; T.Z += ZoogiConst::ZoogiRadius; } // b200: the circle on the floor
			else
			{	// nothing to aim at: the floor ~10 m ahead
				const FVector Ahead = From + Dir * 1000.f;
				FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiBoltFloor), false); Q.AddIgnoredActor(B);
				Gz = W->LineTraceSingleByChannel(H, Ahead + FVector(0, 0, 300.f), Ahead - FVector(0, 0, 6000.f), ECC_WorldStatic, Q) ? (float)H.ImpactPoint.Z : (float)(From.Z - ZoogiConst::ZoogiRadius);
				T = FVector(Ahead.X, Ahead.Y, Gz + ZoogiConst::ZoogiRadius);
			}
			const FVector D3 = (T - From).GetSafeNormal();
			if (!D3.IsNearlyZero() && From.Z - T.Z > 60.f) { Bs.D = D3; Bs.GroundZ = Gz; Bs.b3D = true; UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d fires from %.0f cm up - down at the target on the floor"), B->PlayerIndex + 1, From.Z - Gz); }
		}
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Bs.P), SP))
		{
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetCastShadow(false);
			M->SetMaterial(0, ZoogiUtil::FxMID(A, GBoltPurple, 16.f)); // b81: purple
			A->SetActorScale3D(FVector(0.55f, 0.55f, 0.55f));
			Bs.Vis = A;
		}
		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Bs.P, 0.6f);
		{ FTuneScope Tb(GSkillPow, FName(TEXT("bolt_balls"))); LabImpact(W, Bs.P); } // b139b
		ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 1.5f);
		if (AimPt) { Bs.bStop = true; Bs.Stop = *AimPt; }
		GBoltShots.Add(Bs);
		UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d fires (%d left) at %s"), B->PlayerIndex + 1, BoltCount(G, B->PlayerIndex), Best ? *Best->GetName() : TEXT("ahead"));
		return true;
	}
	static void BoltBlast(AZoogiGameMode* G, const FVector& At, int32 Owner, AZoogiBall* Struck)
	{
		UWorld* W = G->GetWorld();
		const float R = 430.f; // one wolf (AZoogiClone ShockR)
		for (AZoogiOrb* Ob : G->Orbs)
		{
			if (!Ob || Ob->bOut || !Ob->Sphere || !Ob->Sphere->IsSimulatingPhysics()) continue;
			FVector D = Ob->OrbLocation() - At; D.Z = 0.f;
			const float Dist = D.Size(); if (Dist > R) continue;
			const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
			const float K = 1.f - Dist / R;
			const FVector V = Ob->Sphere->GetPhysicsLinearVelocity();
			Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + (Dir * (420.f + 1380.f * K) + FVector(0.f, 0.f, 160.f * K)) * GSkillPow);
			Ob->LastHitBy = Owner;
		}
		if (Struck)
		{
			const FVector D = (Struck->BallLocation() - At).GetSafeNormal2D();
			Struck->LastHitPlayer = Owner;
			Struck->SetPlanarVelocity(Struck->GetBallVelocity() + D * 900.f * GSkillPow, 120.f * GSkillPow);
		}
		const FVector Ground(At.X, At.Y, At.Z - ZoogiConst::ZoogiRadius + 4.f);
		const FLinearColor Col = GBoltPurple;
		AZoogiFX::Spawn(W, At, FLinearColor(0.85f, 0.9f, 1.f), 130.f, 0.22f, false, 14.f);
		AZoogiFX::Spawn(W, Ground, Col, R, 0.55f, true, 10.f);
		UZoogiScreenFx::Shockwave(W, Ground, R + 60.f, Col);
		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Ground, 1.2f);
		{ FTuneScope Tb(GSkillPow, FName(TEXT("bolt_balls"))); LabImpact(W, Ground, 0.45f); } // b139b
		ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.25f);
	}
	static void BoltClearAll(const TCHAR* Why)
	{
		if (GBolts.Num() || GBoltShots.Num() || GStems.Num()) UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS cleared (%s)"), Why);
		for (auto& Kv : GStems) if (AActor* A = Kv.Value.A.Get()) A->Destroy();
		for (FBoltShot& Bs : GBoltShots) if (AActor* V = Bs.Vis.Get()) V->Destroy();
		GStems.Reset(); GBoltShots.Reset(); GBolts.Reset();
	}
	void ClearField(AZoogiGameMode* G)
	{
		for (FWall& Wl : Walls) { if (AActor* A = Wl.Collider.Get()) A->Destroy(); for (const TWeakObjectPtr<AZoogiFabFX>& F : Wl.Fx) if (AZoogiFabFX* X = F.Get()) X->Destroy(); }
		for (FMine& M : Mines) if (AZoogiFabFX* F = M.Fx.Get()) F->Destroy();
		DropGoldApples(); DropTurrets(); // b138c; b190
		for (FBuff& Bf : Buffs) if (AZoogiBall* B = Bf.B.Get()) if (B->Sphere && Bf.SavedMass > 0.f) B->Sphere->SetMassOverrideInKg(NAME_None, Bf.SavedMass, true);
		for (FShield& S0 : Shields) { if (AZoogiBall* B = S0.B.Get()) if (B->Sphere && S0.SavedMass > 0.f) B->Sphere->SetMassOverrideInKg(NAME_None, S0.SavedMass, true); DropShieldFx(S0); }
		Walls.Reset(); Mines.Reset(); Craters.Reset(); Buffs.Reset(); for (FShield& S0 : Shields) DropShieldFx(S0); Shields.Reset(); Events.Reset(); PlusWalls.Reset(); for (FPatch& Pt0 : Patches) if (AZoogiFabFX* F0 = Pt0.Fx.Get()) F0->Destroy(); Patches.Reset(); Traps.Reset(); Counters.Reset(); Spins.Reset(); PlusPrevVel.Reset(); PlusWallCool.Reset(); NewFx.Reset(); PadCool.Reset(); /* b271 / b272 */ // b177k: unlock first
		if (Picking()) Reset();
		BoltClearAll(TEXT("lab reset"));
		GWolfArmed.Reset();
		if (G)
		{
			AZoogiIceBarrier::DestroyAll(G->GetWorld());
			if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G)) if (WP->PackActive()) WP->ForceClear();
			if (UWorld* W = G->GetWorld()) for (TActorIterator<AZoogiFabFX> It(W); It; ++It) It->Destroy();
		}
		UE_LOG(LogZoogi, Log, TEXT("SKILL field cleared"));
	}
	static void BoltTick(AZoogiGameMode* G)
	{
		// b86 (Evan: "the yellow electric keeps flashing after exiting and returning to the game"): a new match / the menu / game over
		// wipes every bolt, stem and shot; the wolf button state goes with it
		{
			static int32 BcLastRound = -1, BcLastTurn = -1;
			const bool bNewMatch = G && (G->RoundNumber < BcLastRound || G->TurnCounter < BcLastTurn);
			if (!G || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver || bNewMatch || GBoltG.Get() != G)
			{
				BoltClearAll(!G ? TEXT("no game") : (bNewMatch ? TEXT("new match") : TEXT("menu / game over")));
				if (bNewMatch || (G && G->Phase == EZoogiPhase::Menu)) { GWolfArmed.Reset(); GCombo.Reset(); GComboTotal.Reset(); GComboDone.Reset(); } // b120g: the combo goes with it
				if (G) { BcLastRound = G->RoundNumber; BcLastTurn = G->TurnCounter; }
				if (!G || GBoltG.Get() != G) return;
			}
			BcLastRound = G->RoundNumber; BcLastTurn = G->TurnCounter;
		}
		UWorld* W = G->GetWorld();
		const float Dt = FMath::Min(W->GetDeltaSeconds(), 0.05f);
		// b86 (Evan: "if the player is knocked out they lose the remaining bolts"): out = no bolts, no flashes
		for (auto It = GBolts.CreateIterator(); It; ++It)
		{
			bool bOut = true;
			for (const AZoogiBall* B : G->Balls) if (B && B->PlayerIndex == It.Key() && !B->bOut) bOut = false;
			if (bOut) { UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d knocked out - %d bolts lost"), It.Key() + 1, It.Value()); It.RemoveCurrent(); }
		}
		// b86 (Evan: "flickering vibrations when equipped with Bolt Balls"): the human holding bolts feels an uneven electric buzz
		{
			static double NextBuzz = 0.0;
			const double Nw = FPlatformTime::Seconds();
			bool bHuman = false;
			for (const TPair<int32, int32>& Kv : GBolts) for (const AZoogiBall* B : G->Balls) if (B && B->PlayerIndex == Kv.Key && !B->bCPU && Kv.Value > 0) bHuman = true;
			if (bHuman && Nw >= NextBuzz && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBoltBuzz")))
			{
				ZoogiHaptics::Kick(FMath::FRandRange(0.25f, 0.55f), W, TEXT("bolt flicker"));
				NextBuzz = Nw + FMath::FRandRange(0.09, 0.32);
			}
		}
		// b81: purple lightning arcs, one per bolt left (re-struck every 50 ms); gone with the last bolt
		for (auto It = GStems.CreateIterator(); It; ++It)
		{
			const int32 N = BoltCount(G, It.Key());
			AZoogiBall* Hb = nullptr; for (AZoogiBall* B : G->Balls) if (B && B->PlayerIndex == It.Key()) Hb = B;
			if (N <= 0 || !Hb || Hb->bOut || !It.Value().A.IsValid()) { if (AActor* A = It.Value().A.Get()) A->Destroy(); It.RemoveCurrent(); continue; }
		}
		for (const TPair<int32, int32>& Kv : GBolts)
			for (AZoogiBall* B : G->Balls)
				if (B && B->PlayerIndex == Kv.Key && !B->bOut)
				{
					FStem* Sm = GStems.Find(Kv.Key);
					if (!Sm) Sm = &GStems.Add(Kv.Key, MakeStem(W, B->BallLocation()));
					Sm->T += Dt;
					if (Sm->T >= 0.05f) { Sm->T = 0.f; StrikeStems(W, *Sm, B, FMath::Min(Kv.Value, StemMax), W->GetTimeSeconds()); }
				}
		// the lightning stems: one per bolt left, round the Zoogi that holds them
		GStemT += Dt;
		// b119b (Evan 16:59: "fix bolt ball yellow electrics flash to how they were before the continuous flashes"): back to the b84
		// 11:05 look - the yellow ElectricShock flashes on the cast and on each bolt shot only; -ZoogiBoltSparks = the constant sparks
		static const bool bSparks = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoltSparks")); // b84 (Evan 11:30: "forget what I said about the yellow electricity, I like it now"): b82/b83 constant sparks back
		if (bSparks && GStemT >= 0.3f)
		{
			GStemT = 0.f;
			for (const TPair<int32, int32>& Kv : GBolts)
				for (AZoogiBall* B : G->Balls)
					if (B && B->PlayerIndex == Kv.Key && !B->bOut)
						for (int32 k = 0; k < FMath::Min(Kv.Value, 3); ++k)
						{
							const float A = FMath::DegreesToRadians(W->GetTimeSeconds() * 90.f + k * 120.f);
							const FVector At = B->BallLocation() + FVector(FMath::Cos(A) * 95.f, FMath::Sin(A) * 95.f, -ZoogiConst::ZoogiRadius + 6.f);
							AZoogiFX::Stylized(W, TEXT("ElectricShock"), At, 0.45f);
						}
		}
		// missiles: straight line, 2600 cm/s, blast on the first rival / orb they reach (or the court edge / 1.4 s)
		const float KR = ZoogiConst::KnockoffRadius * (G->Arena ? G->Arena->GetActorScale3D().X : 1.f);
		const FVector Ctr = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
		// b200 (Evan: "in rapid fire the game doesn't freeze - you see the bolt balls shoot toward the circle we tap"): while RAPID
		// holds the world still the bolts fly in real time
		const float BDt = RapidActive() ? FMath::Min((float)FApp::GetDeltaTime(), 0.05f) : Dt;
		for (int32 i = GBoltShots.Num() - 1; i >= 0; --i)
		{
			FBoltShot& Bs = GBoltShots[i];
			Bs.Age += BDt; Bs.Spark += BDt;
			const FVector Prev = Bs.P;
			Bs.P += Bs.D * 2600.f * BDt;
			if (AActor* V = Bs.Vis.Get()) V->SetActorLocation(Bs.P);
			// b128: swept - anything the bolt passed this frame counts (an orb touching the Zoogi was skipped by the start offset / a long frame)
			auto Near = [&](const FVector& Q, float Rr) { if (Bs.b3D) return FMath::PointDistToSegment(Q, Prev, Bs.P) < Rr; const FVector2D A(Prev), Bp(Bs.P), Qq(Q); return FMath::PointDistToSegment(FVector(Qq, 0.f), FVector(A, 0.f), FVector(Bp, 0.f)) < Rr; }; // b152a: a downward bolt tests its real path
			if (Bs.Spark > 0.05f) { Bs.Spark = 0.f; AZoogiFX::Spawn(W, Bs.P, GBoltPurple, 70.f, 0.25f, false, 10.f); }
			AZoogiBall* Hit = nullptr; bool bBoom = false;
			for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex != Bs.Owner && !O->bOut && Near(O->BallLocation(), ZoogiConst::ZoogiRadius + 40.f)) { Hit = O; bBoom = true; Bs.P = O->BallLocation() - Bs.D * ZoogiConst::ZoogiRadius; break; }
			if (!bBoom) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Near(O->OrbLocation(), ZoogiConst::OrbRadius + 45.f)) { bBoom = true; Bs.P = FVector(O->OrbLocation().X, O->OrbLocation().Y, Bs.P.Z) - Bs.D * ZoogiConst::OrbRadius; break; } // just in front: the blast throws it along the bolt
			if (!bBoom && Bs.b3D && Bs.P.Z <= Bs.GroundZ + 15.f) { Bs.P.Z = Bs.GroundZ + 15.f; bBoom = true; } // b152a: it reached the floor - blast there
			if (!bBoom && Bs.bStop && FVector::DotProduct(Bs.Stop - Bs.P, Bs.D) <= 0.f) { Bs.P = FVector(Bs.Stop.X, Bs.Stop.Y, FMath::Max(Bs.P.Z, Bs.Stop.Z + 15.f)); bBoom = true; } // b200: the tapped circle
			if (!bBoom && (Bs.Age > 1.4f || FVector::Dist2D(Bs.P, Ctr) > KR + 150.f)) bBoom = true;
			if (bBoom)
			{
				BoltBlast(G, Bs.P, Bs.Owner, Hit);
				if (AActor* V = Bs.Vis.Get()) V->Destroy();
				GBoltShots.RemoveAt(i);
			}
		}
	}

	// ---- b83 GUARD barrier
	struct FGuardDome { int32 ArmTurn = -1; bool bUp = false; int32 UpTurn = -1; float UpAt = 0.f; float Dmg = 0.f; TWeakObjectPtr<AActor> A; TWeakObjectPtr<AZoogiBall> Ball; float OldMass = 0.f, OldLin = 0.f, OldAng = 0.f; FVector Anchor = FVector::ZeroVector; TMap<TWeakObjectPtr<AActor>, double> BumpAt; };
	/** b87 (Evan: "Guard is now solid and can't be destroyed and becomes a bumper to others and protects the player" / "Guard form is
	 *  stationary and can not roll or fly away"): -ZoogiGuardBreakable = the b83 cracking dome on a heavy (movable) Zoogi. */
	static bool GuardBreakable() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuardBreakable")); return b; }
	static TWeakObjectPtr<const AZoogiGameMode> GGuardG;
	static TMap<int32, FGuardDome> GGuards;
	constexpr float GuardHP = 6.f; // 3 perfect (1 each) + 2 critical (1.5 each)
	static float GuardR() { return 2.f * (ZoogiConst::ZoogiRadius + 36.f); } // b84: twice the b83 dome, about an ice wall tall
	static bool GuardOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGuard")); return b; }
	static void GuardReset(const AZoogiGameMode* G) { if (GGuardG.Get() != G) { for (auto& Kv : GGuards) if (AActor* A = Kv.Value.A.Get()) A->Destroy(); GGuards.Reset(); GGuardG = G; } }
	bool GuardBusy(const AZoogiGameMode* G, int32 Player) { if (!GuardOn()) return false; GuardReset(G); return GGuards.Contains(Player); }
	void GuardArm(AZoogiGameMode* G, int32 Player)
	{
		if (!GuardOn() || !G) return;
		GuardReset(G);
		if (GGuards.Contains(Player)) return;
		FGuardDome D; D.ArmTurn = G->TurnCounter;
		GGuards.Add(Player, D);
		if (const AZoogiBall* B = G->GetActiveBall()) G->PublicPopup(TEXT("GUARD SET  -  BARRIER WHEN YOUR TURN ENDS"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.55f, 0.85f, 1.f));
		UE_LOG(LogZoogi, Log, TEXT("GUARD armed P%d (turn %d)"), Player + 1, G->TurnCounter);
	}
	static void GuardCrackDome(FGuardDome& D, const FVector& From, int32 Arms)
	{
		AActor* A = D.A.Get(); AZoogiBall* B = D.Ball.Get();
		if (!A || !B) return;
		const FVector C = B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f);
		const float Rd = GuardR();
		FVector N = (From - C).GetSafeNormal(); if (N.IsNearlyZero()) N = FVector(1.f, 0.f, 0.f);
		N = (N + FVector(0.f, 0.f, 0.25f)).GetSafeNormal();
		FVector Ua = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal(); if (Ua.IsNearlyZero()) Ua = FVector(0.f, 1.f, 0.f);
		const FVector Va = FVector::CrossProduct(N, Ua).GetSafeNormal();
		UMaterialInterface* M = ZoogiUtil::FxMID(A, FLinearColor(0.9f, 0.97f, 1.f), 14.f);
		UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
		auto OnDome = [&](const FVector2D& P2) { return C + (N * Rd + Ua * P2.X + Va * P2.Y).GetSafeNormal() * (Rd + 1.5f); };
		auto Seg = [&](const FVector2D& P0, const FVector2D& P1, float Th)
		{
			const FVector W0 = OnDome(P0), W1 = OnDome(P1), Dd = W1 - W0; const float L = Dd.Size(); if (L < 1.f) return;
			UStaticMeshComponent* Cm = NewObject<UStaticMeshComponent>(A);
			Cm->SetStaticMesh(Cube); Cm->SetCollisionEnabled(ECollisionEnabled::NoCollision); Cm->SetCastShadow(false);
			Cm->SetupAttachment(A->GetRootComponent()); Cm->RegisterComponent(); Cm->SetMaterial(0, M);
			const FVector Nm = ((W0 + W1) * 0.5f - C).GetSafeNormal();
			Cm->SetWorldLocationAndRotation((W0 + W1) * 0.5f, FRotationMatrix::MakeFromXZ(Dd / L, Nm).Rotator());
			Cm->SetWorldScale3D(FVector((L + Th) / 100.f, Th / 100.f, 0.01f));
		};
		for (int32 k = 0; k < Arms; ++k)
		{
			FVector2D At(0.f, 0.f); float Ang = FMath::FRandRange(0.f, UE_TWO_PI), Left = FMath::FRandRange(80.f, 170.f), Th = 5.f;
			while (Left > 5.f)
			{
				const float St = FMath::Min(Left, FMath::FRandRange(14.f, 28.f));
				Ang += FMath::DegreesToRadians(FMath::FRandRange(-28.f, 28.f));
				const FVector2D Nx = At + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * St;
				Seg(At, Nx, Th); At = Nx; Left -= St; Th = FMath::Max(1.2f, Th * 0.88f);
			}
		}
	}
	static void GuardDown(AZoogiGameMode* G, int32 Player, bool bShatter)
	{
		FGuardDome* D = GGuards.Find(Player);
		if (!D) return;
		if (AZoogiBall* B = D->Ball.Get())
			if (B->Sphere && D->OldMass > 0.f)
			{
				B->Sphere->SetMassOverrideInKg(NAME_None, D->OldMass, true);
				B->Sphere->SetLinearDamping(D->OldLin); B->Sphere->SetAngularDamping(D->OldAng);
			}
		if (AActor* A = D->A.Get())
		{
			const FVector C = A->GetActorLocation();
			if (bShatter)
			{
				UWorld* W = A->GetWorld();
				AZoogiFX::Spawn(W, C, FLinearColor(0.8f, 0.95f, 1.f), 260.f, 0.5f, false, 14.f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), C, FRotator::ZeroRotator, 1.4f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), C, FRotator::ZeroRotator, 1.0f);
				ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 1.f, 1.2f);
				G->PublicPopup(TEXT("GUARD SHATTERED!"), C + FVector(0, 0, 170.f), FLinearColor(0.7f, 0.9f, 1.f));
			}
			A->Destroy();
		}
		UE_LOG(LogZoogi, Log, TEXT("GUARD barrier %s for P%d (damage %.1f / %.0f)"), bShatter ? TEXT("SHATTERED") : TEXT("down"), Player + 1, D->Dmg, GuardHP);
		GGuards.Remove(Player);
	}
	void GuardHit(AZoogiGameMode* G, AZoogiBall* Victim, const FVector& From, float Damage, const TCHAR* Why)
	{
		if (!GuardOn() || !G || !Victim) return;
		GuardReset(G);
		FGuardDome* D = GGuards.Find(Victim->PlayerIndex);
		if (!D || !D->bUp) return;
		if (!GuardBreakable())
		{	// b87: unbreakable - the hit just flashes off the dome
			AZoogiFX::Spawn(G->GetWorld(), Victim->BallLocation(), FLinearColor(0.6f, 0.9f, 1.f), 1.6f * GuardR(), 0.25f, false, 10.f);
			UE_LOG(LogZoogi, Log, TEXT("GUARD P%d barrier shrugs off a %s hit (unbreakable)"), Victim->PlayerIndex + 1, Why);
			return;
		}
		D->Dmg += Damage;
		UE_LOG(LogZoogi, Log, TEXT("GUARD P%d barrier takes a %s hit: %.1f / %.0f"), Victim->PlayerIndex + 1, Why, D->Dmg, GuardHP);
		if (D->Dmg >= GuardHP - 0.01f) { GuardDown(G, Victim->PlayerIndex, true); return; }
		GuardCrackDome(*D, From, Damage > 1.2f ? 5 : 3);
		ZoogiUtil::PlaySfx(G, TEXT("HitCrack"), 0.9f, 1.1f);
		AZoogiFX::Spawn(G->GetWorld(), Victim->BallLocation(), FLinearColor(0.7f, 0.9f, 1.f), 160.f, 0.3f, false, 10.f);
		if (UMaterialInstanceDynamic* Mi = D->A.IsValid() ? Cast<UMaterialInstanceDynamic>(Cast<AStaticMeshActor>(D->A.Get()) ? Cast<AStaticMeshActor>(D->A.Get())->GetStaticMeshComponent()->GetMaterial(0) : nullptr) : nullptr)
			Mi->SetScalarParameterValue(TEXT("Strength"), 0.35f + 1.2f * (D->Dmg / GuardHP)); // brighter / whiter as it fails
	}
	static void GuardTick(AZoogiGameMode* G)
	{
		if (!GuardOn() || !G) return;
		GuardReset(G);
		TArray<int32> Down;
		for (auto& Kv : GGuards)
		{
			FGuardDome& D = Kv.Value;
			AZoogiBall* B = nullptr; for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex == Kv.Key) B = O;
			if (!B || B->bOut) { Down.Add(Kv.Key); continue; }
			static const bool bOldGuardTiming = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuardTurnBarrier"));
			if (!D.bUp && (bOldGuardTiming ? (G->TurnCounter > D.ArmTurn && G->ActivePlayer != Kv.Key) : true))
			{	// b120 (Evan: "Guard create a force field around player for 10 s"): up at once (b83: after the guarding Zoogi's turn)
				D.bUp = true; D.UpTurn = G->TurnCounter; D.UpAt = G->GetWorld()->GetTimeSeconds(); D.Ball = B; D.Anchor = B->BallLocation();
				if (B->Sphere && !GuardBreakable()) { B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); }
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* A = G->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(B->BallLocation()), SP))
				{
					A->SetMobility(EComponentMobility::Movable);
					UStaticMeshComponent* M = A->GetStaticMeshComponent();
					M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
					M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(false);
					M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.45f, 0.75f, 1.f), 0.9f));
					A->SetActorScale3D(FVector(2.f * GuardR() / 100.f));
					// b84: the base sits on the floor (a dome, not a floating ball) and an ice-wall burst raises it
					A->SetActorLocation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f));
					AZoogiFabFX::Play(G->GetWorld(), TEXT("/Game/StylizedToonVFX3/Effects/NS_Ice_wall.NS_Ice_wall"), B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), 0.6f, 1.5f);
					D.A = A;
				}
				if (B->Sphere && GuardBreakable())
				{	// solid: rivals bounce off, the guarded Zoogi does not budge
					D.OldMass = B->Sphere->GetMass(); D.OldLin = B->Sphere->GetLinearDamping(); D.OldAng = B->Sphere->GetAngularDamping();
					B->Sphere->SetMassOverrideInKg(NAME_None, D.OldMass * 40.f, true);
					B->Sphere->SetLinearDamping(8.f); B->Sphere->SetAngularDamping(8.f);
				}
				AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor(0.55f, 0.85f, 1.f), 2.f * GuardR(), 0.6f, false, 12.f);
				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 0.7f);
				G->PublicPopup(bOldGuardTiming ? TEXT("GUARD BARRIER UP") : TEXT("FORCE FIELD - 10 S"), B->BallLocation() + FVector(0, 0, 170.f), FLinearColor(0.55f, 0.85f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("GUARD barrier up for P%d (turn %d)"), Kv.Key + 1, G->TurnCounter);
			}
			else if (D.bUp)
			{
				// b120: during the guarding player's own roll the field rides along (no pin) and only bumps rival Zoogis
				const bool bOwnRoll = !bOldGuardTiming && G->ActivePlayer == Kv.Key && G->Phase == EZoogiPhase::Rolling;
				if (bOwnRoll) D.Anchor = B->BallLocation();
				if (!bOwnRoll && !GuardBreakable() && B->Sphere && B->Sphere->IsSimulatingPhysics())
				{	// b87: stationary - pinned to the spot it was guarding (nothing rolls it or launches it away)
					if (FVector::Dist(B->BallLocation(), D.Anchor) > 0.5f) B->Sphere->SetWorldLocation(D.Anchor, false, nullptr, ETeleportType::TeleportPhysics);
					B->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); B->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				}
				if (AActor* A = D.A.Get()) A->SetActorLocation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f));
				if (!GuardBreakable())
				{	// b87: the dome is a solid bumper - Zoogis, orbs and wolves bounce off its surface
					const double Now = FPlatformTime::Seconds();
					const float Rd = GuardR();
					auto Bump = [&](AActor* Who, UPrimitiveComponent* Body, float Rad, bool bOrb)
					{
						if (!Who || !Body || !Body->IsSimulatingPhysics()) return;
						const FVector Pw = Body->GetComponentLocation();
						FVector N = Pw - D.Anchor; N.Z = 0.f;
						const float Dist = N.Size(), Reach = Rd + Rad - 4.f;
						if (Dist >= Reach || Pw.Z > D.Anchor.Z + Rd) return;
						N = Dist > 1.f ? N / Dist : FVector(1.f, 0.f, 0.f);
						Body->SetWorldLocation(FVector(D.Anchor.X, D.Anchor.Y, Pw.Z) + N * (Reach + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
						FVector V = Body->GetPhysicsLinearVelocity();
						const float Vn = FVector::DotProduct(V, N);
						if (Vn < 0.f) V -= 2.f * Vn * N; // reflect
						const float Out = FVector::DotProduct(V, N);
						if (Out < 650.f) V += N * (650.f - Out); // bumper kick
						Body->SetPhysicsLinearVelocity(V);
						const double* Last = D.BumpAt.Find(Who);
						if (!Last || Now - *Last > 0.3)
						{
							D.BumpAt.Add(Who, Now);
							AZoogiFX::Spawn(G->GetWorld(), D.Anchor + N * Rd, FLinearColor(0.55f, 0.9f, 1.f), 140.f, 0.3f, false, 10.f);
							ZoogiUtil::PlaySfx(G, TEXT("Clack"), 0.8f, bOrb ? 1.5f : 1.1f);
							ZoogiHaptics::Kick(0.35f, G->GetWorld(), TEXT("guard_bump"));
							UE_LOG(LogZoogi, Log, TEXT("GUARD P%d dome bumps %s (in %.0f -> out %.0f)"), Kv.Key + 1, *Who->GetName(), -Vn, V.Size2D());
						}
					};
					for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut) Bump(O, O->Sphere, ZoogiConst::ZoogiRadius, false);
					if (!bOwnRoll) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Bump(O, O->Sphere, ZoogiConst::OrbRadius, true);
					if (!bOwnRoll) for (AZoogiClone* Cl : G->Clones) if (Cl && Cl->Sphere) Bump(Cl, Cl->Sphere, Cl->Sphere->GetScaledSphereRadius(), false);
				}
				if (bOldGuardTiming) { if (G->ActivePlayer == Kv.Key && G->TurnCounter > D.UpTurn) Down.Add(Kv.Key); } // its next turn: the barrier drops
				else
				{	// b120: 10 s, the last 2 s it flickers
					const float Left = 10.f - (G->GetWorld()->GetTimeSeconds() - D.UpAt);
					if (AActor* Fa = D.A.Get()) Fa->SetActorHiddenInGame(Left < 2.f && FMath::Fmod(Left, 0.25f) < 0.08f);
					if (Left <= 0.f) Down.Add(Kv.Key);
				}
			}
		}
		for (int32 P : Down) GuardDown(G, P, false);
	}

	void WolfTick(AZoogiGameMode* G)
	{
		BoltTick(G);
		GuardTick(G);
		{	// the CPU fires its bolts through its own roll
			const AZoogiBall* Bc = G ? G->GetActiveBall() : nullptr;
			if (Bc && Bc->bCPU && G->Phase == EZoogiPhase::Rolling && BoltReady(G) && G->PhaseTime > 0.6f + 0.55f * (3 - BoltCount(G, Bc->PlayerIndex))) BoltGo(G);
		}
		// b87 -ZoogiWolfBankTest: the human banks 3 wolves on its first aim and fires them 0.5 s into the roll
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfBankTest")) && G)
			if (const AZoogiBall* Bt = G->GetActiveBall())
				if (!Bt->bCPU)
				{
					static int32 BankStep = 0;
					if (BankStep == 0 && G->Phase == EZoogiPhase::Aiming) { for (int32 k = 0; k < 3; ++k) ArmWolf(G, Bt->PlayerIndex); BankStep = 1; UE_LOG(LogZoogi, Log, TEXT("WOLFBANK test: P%d banks %d"), Bt->PlayerIndex + 1, WolfCount(G, Bt->PlayerIndex)); G->TakeShotDelayed(TEXT("wolfbank_a_badge"), 0.5f); }
					else if (BankStep == 1 && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.5f)
					{
						const bool bGo = WolfGo(G); BankStep = 2;
						UE_LOG(LogZoogi, Log, TEXT("WOLFBANK test: SUPERPOWER -> WolfGo %d (left %d)"), bGo ? 1 : 0, WolfCount(G, Bt->PlayerIndex));
						G->TakeShotDelayed(TEXT("wolfbank_b_paint"), 0.4f); G->TakeShotDelayed(TEXT("wolfbank_c_out"), 1.6f); G->TakeShotDelayed(TEXT("wolfbank_d_hunt"), 2.6f);
					}
				}
		// the CPU taps its wolf a moment into its own roll, like a player would
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (B && B->bCPU && WolfReady(G) && G->PhaseTime > 0.75f) WolfGo(G);
	}

	void WolfBlast(AZoogiGameMode* G, const FVector& At, int32 Owner, float Mul)
	{
		if (!G) return;
		UWorld* W = G->GetWorld();
		Mul *= GSkillPow; // b100: a card skill's blasts are 0.6
		const float R = 430.f * FMath::Sqrt(FMath::Max(Mul, 0.1f)) * ZoogiSkillLab::Radius(); // a wolf's ShockR; b107 x the lab radius
		int32 N = 0;
		auto Push = [&](UPrimitiveComponent* Body, const FVector& L, float Scale) -> bool
		{
			if (!Body || !Body->IsSimulatingPhysics()) return false;
			FVector D = L - At; D.Z = 0.f;
			const float Dist = D.Size();
			if (Dist > R) return false;
			const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
			const float K = 1.f - Dist / R;
			const FVector V = Body->GetPhysicsLinearVelocity();
			Body->WakeAllRigidBodies();
			Body->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (420.f + 1380.f * K) * Mul * Scale + FVector(0.f, 0.f, 160.f * K * Mul));
			++N; return true;
		};
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Push(O->Sphere, O->OrbLocation(), 1.f)) O->LastHitBy = Owner;
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->PlayerIndex != Owner && Push(B->Sphere, B->BallLocation(), 0.6f)) B->LastHitPlayer = Owner;
		const FLinearColor Col(1.f, 0.5f, 0.15f);
		AZoogiFX::Spawn(W, At + FVector(0, 0, 40.f), FLinearColor(1.f, 0.9f, 0.7f), 120.f, 0.22f, false, 14.f);
		AZoogiFX::Spawn(W, At + FVector(0, 0, 4.f), Col, R, 0.55f, true, 10.f);
		AZoogiFX::Shockwave(W, At, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Col, false);
		UZoogiScreenFx::Shockwave(W, At, R + 60.f, Col);
		AZoogiFX::Stylized(W, SkillBlastFx(), At, 0.45f * FMath::Max(Mul, 0.6f) * SkillBlastSc());
		ZoogiEpic::DebrisBurst(W, At, 0.45f * Mul); // b103: rock chunks out of the floor
		ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 1.f);
		ZoogiHaptics::Kick(0.6f, W, TEXT("wolf blast"));
		UE_LOG(LogZoogi, Log, TEXT("SKILL wolf-size blast x%.2f at (%.0f, %.0f): %d pushed"), Mul, At.X, At.Y, N);
	}

	void MeteorRain(AZoogiGameMode* G, const FVector& Ctr, float R, int32 Owner, int32 Count, float Mul)
	{
		if (!G) return;
		UWorld* W = G->GetWorld();
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		const int32 V = 1 + (FMath::Max(Owner, 0) * 3 + FMath::RandRange(0, 2)) % 12;
		AZoogiFabFX::Play(W, *FString::Printf(TEXT("/Game/MeteorRain/Particles/NiagaraSystems/NS_MeteorRainV%02d.NS_MeteorRainV%02d"), V, V), Ctr, 0.25f * FMath::Max(1.f, R / 1030.f), 4.5f);
		AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), Ctr, 0.9f);
		// aim at the rivals and the orbs first, then fill in at random
		TArray<FVector> Spots;
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->PlayerIndex != Owner && FVector::Dist2D(B->BallLocation(), Ctr) < R) Spots.Add(B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius));
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Spots.Num() < Count && FVector::Dist2D(O->OrbLocation(), Ctr) < R && FMath::FRand() < 0.5f) Spots.Add(O->OrbLocation());
		while (Spots.Num() < Count) { const float A = FMath::FRandRange(0.f, 2.f * PI), Rr = FMath::Sqrt(FMath::FRand()) * R * 0.85f; Spots.Add(Ctr + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 0.f)); }
		if (Spots.Num() > Count) Spots.SetNum(Count);
		for (int32 k = 0; k < Spots.Num(); ++k)
		{
			const FVector Hit = Spots[k];
			Later(G, 0.55f + 0.22f * k, [WG, Hit, Owner, Mul, k]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				AZoogiFabFX::Play(Gm->GetWorld(), FX(k % 2 ? TEXT("NS_Meteor_hit") : TEXT("NS_Meteor_hit_2")), Hit, Sc(FName(TEXT("meteor_hit")), 0.22f) * 1.5f, 2.5f);
				ZoogiUtil::PlaySfx(Gm, TEXT("RockCrack"), 0.95f, FMath::FRandRange(0.6f, 0.85f)); // b90h (Evan): stone crush as each meteor hits
				WolfBlast(Gm, Hit, Owner, Mul);
			});
		}
		UE_LOG(LogZoogi, Log, TEXT("SKILL meteor rain: %d meteors over r %.0f (x%.2f wolf blasts)"), Spots.Num(), R, Mul);
	}

	bool Hexed(const AZoogiGameMode* G, int32 Player) { if (!G) return false; for (const FNewFx& F : NewFx) if (F.Kind == NK_Hex && F.Until >= G->TurnCounter && F.B.IsValid() && F.B->PlayerIndex == Player) return true; return false; } // b272 HEX
	float SkillPow() { return GSkillPow; }
	void SetAwakenStrength() { GSkillPow = 1.f; }
	void AwakenCast(AZoogiGameMode* G, AZoogiBall* B, FName Id)
	{
		if (!G || !B) return;
		UE_LOG(LogZoogi, Log, TEXT("AWAKEN power P%d: %s at full strength"), B->PlayerIndex + 1, *Id.ToString());
		if (Id == FName(TEXT("wolf_pack")))
		{	// three wolves straight out (no level, no button): the pick, then the hunt
			GSkillPow = 1.f;
			for (int32 k = 0; k < 3; ++k) ArmWolf(G, B->PlayerIndex);
			if (!WolfGo(G)) UE_LOG(LogZoogi, Log, TEXT("AWAKEN void pack: wolves banked (SUPERPOWER releases them)"));
			return;
		}
		GAwakenNext = true;
		if (!AutoCast(G, Id)) GAwakenNext = false;
	}
	void LabCast(AZoogiGameMode* G, AZoogiBall* B, FName Id)
	{
		if (!G || !B || !IsSkill(Id)) return;
		const FDef* D = Find(Id);
		Reset();
		S.bOn = true; S.bConfirmed = true; S.Id = Id; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius; S.Range = FMath::Max(1400.f, G->ArcRange());
		TArray<AActor*> Cand; Candidates(G, Cand);
		Cand.RemoveAll([B](AActor* X) { return X == B; });
		Cand.Sort([&](const AActor& X, const AActor& Y) { return FVector::Dist2D(ActorPos(const_cast<AActor*>(&X)), S.Origin) < FVector::Dist2D(ActorPos(const_cast<AActor*>(&Y)), S.Origin); });
		const FVector Fwd = Cand.Num() ? ActorPos(Cand[0]) : S.Origin + FVector(600.f, 0, 0);
		S.Ctr = Fwd; S.A = S.Origin + (Fwd - S.Origin) * 0.6f + FVector(0, 300.f, 0); S.B = S.Origin + (Fwd - S.Origin) * 0.6f - FVector(0, 300.f, 0);
		for (int32 k = 0; k < Cand.Num() && k < D->Count; ++k) S.Targets.Add(Cand[k]);
		for (int32 k = 0; k < D->Count; ++k) S.Spots.Add(Fwd + FVector(FMath::Cos(k * 2.1f), FMath::Sin(k * 2.1f), 0.f) * 220.f);
		if (D->Aim == EAim::Line && IsPackLine(Id))
		{	// b176i: attack lines through the rack in the lab too
			FVector Dir = (Fwd - S.Origin).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			S.A = S.Origin + Dir * 70.f; S.B = Fwd + Dir * 160.f;
		}
		Execute(G, B, Id);
	}
	bool AutoCast(AZoogiGameMode* G, FName Id)
	{
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B || !IsSkill(Id)) return false;
		const FDef* D = Find(Id);
		Reset();
		S.bOn = true; S.bConfirmed = true; S.Id = Id; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius; S.Range = FMath::Max(400.f, G->ArcRange());
		TArray<AActor*> Cand; Candidates(G, Cand);
		Cand.Sort([&](const AActor& X, const AActor& Y) { return FVector::Dist2D(ActorPos(const_cast<AActor*>(&X)), S.Origin) < FVector::Dist2D(ActorPos(const_cast<AActor*>(&Y)), S.Origin); });
		const FVector Fwd = Cand.Num() ? ActorPos(Cand[0]) : S.Origin + FVector(S.Range * 0.5f, 0, 0);
		S.Ctr = ClampRange(Fwd); S.A = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f + FVector(0, 250.f, 0)); S.B = ClampRange(S.Origin + (Fwd - S.Origin) * 0.5f - FVector(0, 250.f, 0));
		if (D->Aim == EAim::Line && IsPackLine(Id))
		{	// b176e: attack lines run from the caster through the target
			FVector Dir = (Fwd - S.Origin).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			S.A = ClampRange(S.Origin + Dir * 70.f); S.B = ClampRange(Fwd + Dir * 160.f);
		}
		for (int32 k = 0; k < Cand.Num() && k < D->Count; ++k) S.Targets.Add(Cand[k]);
		for (int32 k = 0; k < D->Count; ++k) S.Spots.Add(ClampRange(S.Origin + FVector(FMath::Cos(k * 2.1f), FMath::Sin(k * 2.1f), 0.f) * S.Range * 0.55f));
		Execute(G, B, Id);
		return true;
	}
}
