// b107 (Evan: "create a special mode for me to test and change the skill powers - strength, colour, explosion type, blast radius,
// gravity pull strength - cycle through and choose ... preset follow cameras to view it from different views" / "Lab + I bake them in").
// b108 (Evan: "make skill lab not a skill you play in game - remove it from the skill list - a black P5 button to access it. In the lab
// just the effect, no player, only orbs that reset when the skill does. At the bottom the skill name with left / right arrows to sort
// through them; I press a PLAY button to start the effect"):
// the home screen's black SKILL LAB button starts a solo match with no bots; the caster is hidden (the skills still come from its spot),
// the orbs are re-racked (fresh set if any were lost) on every PLAY. Left panel: STRENGTH / RADIUS / PULL / FX SIZE / COLOUR / BLAST /
// CAMERA + RESET / COPY; EXIT (top right) saves and goes back home. Tunings: Saved/SkillLab.ini, used in real matches too.
// -ZoogiSkillLabTest = open the lab at start and PLAY every skill in turn (captures).
#pragma once
#include "CoreMinimal.h"

class AZoogiGameMode;
class APlayerController;
class UCanvas;

namespace ZoogiSkillLab
{
	ZOOGIROLL_API bool Open();
	/** the front's SKILL LAB button: the next match is the lab (solo, no bots) */
	ZOOGIROLL_API void RequestLab();
	ZOOGIROLL_API bool Pending();
	ZOOGIROLL_API void OpenLab(AZoogiGameMode* G);
	ZOOGIROLL_API void CloseLab(AZoogiGameMode* G);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC);
	ZOOGIROLL_API void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease);
	// tunings (1 = default), for the skill playing now (ZoogiSkills sets it when a skill executes)
	ZOOGIROLL_API void SetCurrent(FName Skill);
	/** b136: the skill the tunings below read */
	ZOOGIROLL_API FName Current();
	/** b136: one skill's tunings, whatever plays now (wolves / bolts / delayed effects) */
	ZOOGIROLL_API float StrengthOf(FName Skill);
	ZOOGIROLL_API float RadiusOf(FName Skill);
	ZOOGIROLL_API float Strength();
	ZOOGIROLL_API float Radius();
	ZOOGIROLL_API float Pull();
	ZOOGIROLL_API float FxSize();
	/** b177o: the HEIGHT row - scales the upward part of every skill knock (1 = default) */
	ZOOGIROLL_API float Height();
	/** True with the colour override for the current skill. */
	ZOOGIROLL_API bool Tint(FLinearColor& Out);
	/** The blast effect for the current skill (Default = DefaultFx). */
	ZOOGIROLL_API const TCHAR* BlastFx(const TCHAR* DefaultFx);
	/** b139b: the BLAST SIZE row (1 = default) */
	ZOOGIROLL_API float BlastSize();
	/** b139b: the BLAST picked for the current skill, nullptr = DEFAULT ("" = NONE) */
	ZOOGIROLL_API const TCHAR* ChosenBlast();
	/** b142: any entry's BLAST pick (nullptr = DEFAULT, "" = NONE), its BLAST SIZE x FX SIZE, its COLOUR - "ko_orb" / "ko_player" = the knock-out explosions */
	ZOOGIROLL_API const TCHAR* BlastFor(FName Skill);
	ZOOGIROLL_API float BlastSizeFor(FName Skill);
	ZOOGIROLL_API bool TintFor(FName Skill, FLinearColor& Out);
	/** b225: the GATLING TURRET lab rows - shots per turret, forced ammo style (0 = the player's inventory pick), fire-rate scale; FX SIZE of any skill */
	ZOOGIROLL_API int32 TurretShots();
	ZOOGIROLL_API int32 TurretAmmo();
	ZOOGIROLL_API float TurretRate();
	ZOOGIROLL_API float SizeOf(FName Skill);
}
