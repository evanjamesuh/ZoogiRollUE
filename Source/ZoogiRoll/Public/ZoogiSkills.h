// Nov 2 b63 (Evan): the Fab VFX skills (Hovl Stylized Toon VFX 3 + Gabriel Aguiar Meteor Rain). A skill is magic cast inside the
// Zoogi's ARC range ring ("skills are now magic that activates in the arc radius"): picking one from the SKILL list slows the
// world, shows the arc range ring and asks for its target - an area circle, a swiped line, one tapped target, up to three tapped
// targets, three tapped spots, or the Zoogi itself. FIRE spends it (the normal card commit), CANCEL gives it back.
#pragma once

#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class APlayerController;
class UCanvas;

namespace ZoogiSkills
{
	enum class EAim : uint8 { Self, Area, Line, Tap, MultiTap, Spots };
	ZOOGIROLL_API bool IsSkill(FName Id);
	/** b179 (Evan Nov 7): the Earthbender effects kept OUT of the skills list (map events / Skill Lab only) - all but Sapphire Wall, Pillar Field, Long Rampart, Fault Line. -ZoogiAllEarthSkills = all in the list */
	ZOOGIROLL_API bool IsEventOnlyEarth(FName Id);
	/** b107c: every skill a match hand can hold (not the SKILL LAB) */
	ZOOGIROLL_API TArray<FName> AllSkillIds();
	/** b108 SKILL LAB RESET: every skill leftover off the field (walls, mines, shields, buffs, bolts, wolves, timed hits, Fab effects) */
	ZOOGIROLL_API void ClearField(AZoogiGameMode* G);
	ZOOGIROLL_API EAim AimOf(FName Id);
	ZOOGIROLL_API bool Picking();
	/** CardsTapPlay: true = the target pick opened (nothing spent yet); false = play it now (self skills / a confirmed pick). */
	ZOOGIROLL_API bool WantPick(AZoogiGameMode* G, int32 Slot, FName Id);
	/** After the card commit succeeded: runs the skill on the confirmed pick. */
	ZOOGIROLL_API void Execute(AZoogiGameMode* G, AZoogiBall* Caster, FName Id);
	ZOOGIROLL_API void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Cancel(const TCHAR* Why);
	/** Test: casts Id for the human with an automatic pick (nearest rival / orb / point ahead). */
	ZOOGIROLL_API bool AutoCast(AZoogiGameMode* G, FName Id);
	/** b67 (Evan: "the CPU must play as a real person would"): the CPU opens the same target pick, its cursor glides to the
	 *  targets it chose, then FIRE - the skill runs (the card was already paid by its commit). Self skills run straight away. */
	ZOOGIROLL_API void CpuCast(AZoogiGameMode* G, FName Id);
	/** b67: a CPU pick is on screen */
	ZOOGIROLL_API bool CpuPicking();
	/** b76: a skill's effect is still playing out (the menu stays hidden meanwhile) */
	ZOOGIROLL_API bool EffectBusy();
	/** b197: GATTLING RAPID is running (the world pauses like a REACTION; every tap is a bolt for 6 s) - shots so far, seconds left */
	/** b223: GATTLING RAPID = dashes - the human has some left (OutLeft); DashGo = SUPERPOWER: dash toward the last court tap */
	ZOOGIROLL_API bool DashActive(int32* OutLeft = nullptr);
	/** b228: true for a moment after each ORB RELAY dash (the world runs slowed) */
	ZOOGIROLL_API bool DashSlowMo();
	ZOOGIROLL_API bool DashGo(AZoogiGameMode* G);
	ZOOGIROLL_API bool RapidActive(int32* OutShots = nullptr, float* OutLeft = nullptr);
	/** b200: the RAPID tap circles (K < 0 = the bolt is on its way, K >= 0 = popped K s ago) */
	ZOOGIROLL_API int32 RapidMarks(TArray<FVector>& OutP, TArray<float>& OutK);
	/** b205 (Evan picked option F): the last skill that went off - id, caster player, FPlatformTime when it fired (the banner) */
	ZOOGIROLL_API bool LastCast(FName& OutId, int32& OutPlayer, double& OutAt);
	/** b100 (Evan: "tune the skills down"): the strength of the skill playing now - 0.6 for a card skill, 1 for an Awaken power. */
	ZOOGIROLL_API float SkillPow();
	/** b100: the next pushes run at full (Awaken / ultimate) strength. */
	ZOOGIROLL_API void SetAwakenStrength();
	/** b100: a Zoogi's Awaken power = one of the skills at its full (pre-b100) strength, auto-aimed, no SP. */
	ZOOGIROLL_API void AwakenCast(AZoogiGameMode* G, AZoogiBall* B, FName Id);
	/** b107 SKILL LAB: cast Id from Caster (auto-aimed, card strength x the lab tuning), whoever's turn it is. */
	ZOOGIROLL_API void LabCast(AZoogiGameMode* G, AZoogiBall* Caster, FName Id);
	/** b77 (Evan: "when Wolf Pack is selected you get a separate wolf icon that replaces the CONTROL button and activates the
	 *  effect during the roll"): casting wolf_pack arms the WOLF button for that player; tapping it while their shot rolls
	 *  summons the old Wolf Pack (howl, wolves). -ZoogiWolfSkillRedirects = the b66 skill (3 free redirects). */
	ZOOGIROLL_API void ArmWolf(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool WolfArmed(const AZoogiGameMode* G, int32 Player);
	/** b77 (Evan: "bring back the wolf badges that tell me how many wolves I have left"): summons banked */
	ZOOGIROLL_API int32 WolfCount(const AZoogiGameMode* G, int32 Player);
	/** the WOLF slot shows for the human on the turn ball (aiming: in the mode bar, rolling: same spot) */
	ZOOGIROLL_API bool WolfSlot(const AZoogiGameMode* G);
	/** the turn ball is rolling and its player has the wolf armed */
	ZOOGIROLL_API bool WolfReady(const AZoogiGameMode* G);
	ZOOGIROLL_API bool WolfGo(AZoogiGameMode* G);
	ZOOGIROLL_API void WolfTick(AZoogiGameMode* G);
	/** b90f: a Wolf Pack clone's detonation at At (orbs + rival Zoogis), Mul scales radius / force (1 = a wolf) */
	ZOOGIROLL_API void WolfBlast(AZoogiGameMode* G, const FVector& At, int32 Owner, float Mul);
	/** b90f: meteor rain over Ctr (radius R): Count meteors land one after another, each a WolfBlast(Mul) */
	ZOOGIROLL_API void MeteorRain(AZoogiGameMode* G, const FVector& Ctr, float R, int32 Owner, int32 Count, float Mul);
	/** b80 (Evan: Bolt Balls "works like Wolf Pack, the balls act like straight missiles, 3 shots, a lightning stem round the Zoogi
	 *  per shot left"): banked bolt shots per player; SUPERPOWER fires one while rolling. -ZoogiOldBoltBalls = the b66 redirect bolts. */
	ZOOGIROLL_API void ArmBolts(AZoogiGameMode* G, int32 Player, int32 N);
	ZOOGIROLL_API int32 BoltCount(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool BoltReady(const AZoogiGameMode* G);
	ZOOGIROLL_API bool BoltGo(AZoogiGameMode* G);
	/** b120g COMBO: skills armed before the launch; each SUPERPOWER tap after the launch casts the next one from the Zoogi */
	ZOOGIROLL_API void ComboArm(AZoogiGameMode* G, int32 Player, FName Id);
	ZOOGIROLL_API int32 ComboCount(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API int32 ComboTotal(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool ComboReady(const AZoogiGameMode* G);
	/** b128: this player still has bolts / banked wolves / its own pack out - the next COMBO cast waits until they're used up */
	ZOOGIROLL_API bool PowerLeft(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool ComboGo(AZoogiGameMode* G);
	/** b276: the queued COMBO skills of a player (in order), and cast the Index-th one now (it jumps the queue) */
	ZOOGIROLL_API TArray<FName> ComboQueue(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool ComboGoAt(AZoogiGameMode* G, int32 Index);
	/** b122c: a COMBO cast's pick was cancelled - the skill goes back to the front of the queue */
	ZOOGIROLL_API void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id);
	/** b221: -ZoogiDemoTurret rig - the turret model alone, a barrel shot of Style (1-6, straight from the barrel tip), its flight tick */
	ZOOGIROLL_API AActor* TurretDemoSpawn(AZoogiGameMode* G, const FVector& Floor, float& OutTipFwd, float& OutTipUp);
	ZOOGIROLL_API float TurretYawOffset();
	ZOOGIROLL_API float TurretShoot(AZoogiGameMode* G, AActor* Turret, const FVector& Floor, float TipFwd, float TipUp, const FVector& Target, int32 Style);
	ZOOGIROLL_API void TurretShotsTick(AZoogiGameMode* G, float Dt);
	/** b130: ITEM -> PRISM GRENADE - a target circle at twice the arc range; FIRE lobs it (ZoogiGrenade::Lob) */
	ZOOGIROLL_API bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B);
	/** b137: ITEM -> a weapon from the collection (grenade / sword / staff / gatling / disc): its pick, or straight away (sword) */
	ZOOGIROLL_API bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName Weapon);
	/** b139a: the Volcano's world meteor strike (the old FIRE METEORS effect) at At */
	ZOOGIROLL_API void WorldFireMeteors(AZoogiGameMode* G, const FVector& At, float R);
	/** b130: a thrown grenade landed - the Energy Shockwave (pull, then blast) bursts at At */
	ZOOGIROLL_API void GrenadeBoom(AZoogiGameMode* G, AZoogiBall* B, const FVector& At);
	/** b83 (Evan: GUARD "can only be selected once; once the turn ends it activates and the Zoogi is covered by a solid barrier until
	 *  its next turn; it cracks more and more over three perfect hits and two critical hits"). -ZoogiOldGuard = the card only. */
	ZOOGIROLL_API bool GuardBusy(const AZoogiGameMode* G, int32 Player);
	/** b272 HEX: this player's next turn has no aim line and 70% max power */
	ZOOGIROLL_API bool Hexed(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void GuardArm(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void GuardHit(AZoogiGameMode* G, AZoogiBall* Victim, const FVector& From, float Damage, const TCHAR* Why);
}
