#pragma once
// Oct 31 (Evan: "I don't want a card system. I'd rather a P5 arrangement like this" + "I don't wanna see cards, decor, graveyard ...
// the red/black that appear on screen, comment bubbles appear, the actions tied to the player like that I can just tap easily").
// P5-STYLE COMMAND MENU for the human's turn in the turn-based modes (Classic, 2v2, Story missions): slanted red / black / white
// command plates fanned around the active Zoogi (projected from the world every frame), one tap = do it:
//   ATTACK (aim the normal shot) / SKILL (the old card effects as skills with an SP = card-energy cost, text rows only) / GUARD /
//   ITEM / SUPERPOWER / SHOWTIME (Awaken when the charge is full, with a SHOWTIME splash).
// Plus a jagged speech bubble from a portrait, the target name tag and a party strip (portraits, score, SP). No card hand, deck,
// discard or graveyard is drawn while it is on. The card system still runs underneath (skills = MenuAction CardPlay).
// Self-contained viewport overlay (ZoogiP5Command.cpp: own ticker, no game-mode hooks). Keys 1-6 / Esc (back).
//   -ZoogiOldCards (or -ZoogiCardHandUI / -ZoogiOldMenu) = the previous card UI.  -ZoogiP5CmdTest = scripted capture run.
#include "CoreMinimal.h"

class AZoogiGameMode;

namespace ZoogiP5Cmd
{
	/** The P5 command menu replaces every card visual (default on with the P5 style). */
	ZOOGIROLL_API bool On();
	/** True while the command menu is on screen (other HUD bits can step aside). */
	ZOOGIROLL_API bool Showing();
	/** b77 (Evan: "selecting skill buttons should not move the player camera - taps pass through the menu"): a menu press is
	 *  held (or just ended) - the game ignores that touch */
	ZOOGIROLL_API bool InputBlocked();
	/** Oct 31 b46 (Evan): tapping the locked target again cancels the launch - bring the command menu back for this turn. */
	ZOOGIROLL_API void Reopen();
	/** b67 (Evan): cancelling a skill target pick brings the menu back on the SKILL list so another skill can be chosen. */
	ZOOGIROLL_API void BackToSkills();
	/** b79 (Evan: "after FIRE the skill menu should hide to the SKILL button so people can see the effect"): fold to the SKILL button */
	ZOOGIROLL_API void ToSkillButton();
	/** b67 (Evan: "the CPU must play as a real person would"): the CPU's turn opens the same command menu, its cursor walks
	 *  SKILL -> its cards -> ATTACK, then its skills go through the target pick. True while that is on screen (the CPU waits to aim).
	 *  -ZoogiCpuInstant = the old instant CPU. */
	ZOOGIROLL_API bool CpuHold(AZoogiGameMode* G);
}
