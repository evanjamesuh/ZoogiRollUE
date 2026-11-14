// b233 (Evan Nov 10: bought Fab "Voyager: Third Person Shooter v3", "A,B,C,D implement all"): the Voyager pieces in Zoogi Roll.
// The assets were migrated from C:\dev\Voyager by ios/voy/voy_migrate.py: meshes under /Game/Voyager/..., the anim sequences under
// /Game/ZoogiExport/Anims/... (notify tracks stripped - Voyager's notifies are Blueprints). Everything is C++-driven (no Voyager Blueprints).
//  A  ZoogiWallCine: the fighters are Voyager characters (punch, hit, ragdoll into the rock)
//  B  ZoogiRoyaleHero: the hero is the Voyager character for the Zoogi (rifle locomotion, Voyager Heavy Machine Gun)
//  C  ZoogiSkills turret ammo: Voyager trails + explosions (rocket / plasma / tracers / laser)
//  D  ZoogiCosmosBoss: the TOMB titan is the Greater Spider; in a RAID a Voyager trooper + minigun turret fight beside you
// -ZoogiNoVoyager turns all of it off (the b232 looks).
#pragma once
#include "CoreMinimal.h"
class AZoogiGameMode;
class USkeletalMesh;
class UAnimSequence;
class AActor;
class UWorld;
class USkeletalMeshComponent;
class AZoogiBall;

namespace ZoogiVoyager
{
	ZOOGIROLL_API bool On();
	/** the Voyager character for a Zoogi (Blue -> Hero, Frosti -> Wraith, Vex / Dark -> Shadow Ops, Hotstreak -> Murdock Wasteland, Lumi -> Murdock, else by index) */
	ZOOGIROLL_API USkeletalMesh* Character(const FString& ZoogiName, int32 Fallback, FString* OutName = nullptr);
	/** an anim under /Game/ZoogiExport/Anims, e.g. "Soldier/Anim_Idle_Rifle_Hip" (cached) */
	ZOOGIROLL_API UAnimSequence* Anim(const TCHAR* Rel);
	/** a Voyager Niagara effect by short name (RocketTrail, PlasmaTrail, MachinegunTrail, SniperTrail, RocketBoom, PlasmaBoom, GrenadeBoom, ImpactMetal,
	 *  ImpactConcrete, Flamethrower, Shield, Powerup, SwordTrail, Slash) - plays via AZoogiFabFX (Follow keeps it on an actor) */
	ZOOGIROLL_API void Fx(UWorld* W, const TCHAR* Short, const FVector& At, float Scale, float Life, AActor* Follow = nullptr, const FRotator& Rot = FRotator::ZeroRotator);
	/** b235 (Evan: "make the soldier look like zoogi characters" - 1 + 2): the Zoogi's colours on the armour (tint params + shoulder pads + a
	 *  glowing chest core) and the Zoogi's own ball as the head. bHollow = a VOID HOLLOW (black / purple, no Zoogi head). */
	ZOOGIROLL_API void Dress(USkeletalMeshComponent* Sk, const AZoogiBall* B, bool bHollow = false);
	ZOOGIROLL_API USkeletalMesh* Gun(const TCHAR* Short); // HeavyMachineGun, Launcher, GrimFlameThrower, GrimElectricGun, PlasmaRifleV2, AssaultRifle
	/** D: the raid companions (a trooper with the heavy machine gun + a minigun turret) */
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
}
