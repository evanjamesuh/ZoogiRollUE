#pragma once
// Oct 30 (Evan: "You have complete autonomy for story mode, but it must be more than a ring battle: you need to create
// environments. Tap to travel is enough. Fixed party for missions. Story battles have cards on by default.")
// STORY MODE: 4 episodes, 12 stops, 12 battles, fully voiced in sound effects (no VO yet), P5-style screens over the live 3D arenas.
// Data: Content/Data/Story/{Chapters,Spots,Missions,Dialogue}.csv. Code: ZoogiStoryMode.inl (included by ZoogiP5Style.cpp so it
// shares the P5 paint helpers). Progress: Saved/SaveGames/ZoogiStory.txt (the cleared stop ids).
//  -ZoogiStoryTest  scripted capture run (fresh save; an all-CPU battle that the test ends ~14 s in)
//  -ZoogiStoryLose  the test battle is lost (MISSION FAILED screen)      -ZoogiStoryAll  every episode unlocked
#include "CoreMinimal.h"

class UWorld;
class AZoogiGameMode;
enum class EZoogiCPUTier : uint8;

namespace ZoogiStory
{
	/** the MODE SELECT "STORY" card */
	ZOOGIROLL_API void Open(UWorld* World);
	ZOOGIROLL_API bool IsOpen();
	/** a story battle is on (the arena HUD skips its own results screen: the story shows MISSION CLEAR / FAILED) */
	ZOOGIROLL_API bool InMission();
	/** a story screen covers the game: the canvas HUD and the menu stay off */
	ZOOGIROLL_API bool BlocksHud(const AZoogiGameMode* G);
	/** the mission's CPU tier (ZoogiCPU::ActiveTier) */
	ZOOGIROLL_API bool TierOverride(EZoogiCPUTier& Out);
	/** -ZoogiStoryTest: true once, when the scripted battle should end */
	ZOOGIROLL_API bool TestWantsFinish();
	/** -ZoogiStoryTest: opens the story a moment after the menu appears (AZoogiHUD, menu phase) */
	ZOOGIROLL_API void AutoOpenForTest(UWorld* World);
}
