// b156 (Evan Nov 6 07:27 "daily shot goes in"): DAILY TRICK SHOT - one hand-set shot per day, the same layout for everyone.
//  - MODE SELECT -> DAILY TRICK SHOT. Today's arena + layout come from the date (a new shot every day at 4 am local).
//  - You vs 3 rivals that never shoot. ONE shot per try, 3 tries; your best try counts.
//    1 star = knock 1 rival off, 2 stars = 2 in one shot, 3 stars = all 3 in one shot. Falling off yourself = 0 stars that try.
//  - Rewards only for beating today's best: +20 coins and +20 XP per new star, +5 gems for the first 3 stars of the day.
//  - Streak: play on consecutive days; every 7th day in a row = +20 gems.
//  Saved in GameUserSettings [ZoogiRoll.TrickShot]. -ZoogiTrickShot = start today's shot from the command line (test),
//  -ZoogiTrickDay=N = pretend today is day N.
#pragma once
#include "CoreMinimal.h"
#include "ZoogiTypes.h"

class AZoogiGameMode;
class UCanvas;

namespace ZoogiTrickShot
{
	/** the mode card: arm, set up the match (players, arena, rounds) - the caller fires Start */
	ZOOGIROLL_API void Arm(AZoogiGameMode* G);
	ZOOGIROLL_API bool Active();
	/** today's shot number (#1 = Nov 6 2026), arena, streak, today's best stars */
	ZOOGIROLL_API int32 TodayNumber();
	ZOOGIROLL_API EZoogiArena TodayArena();
	ZOOGIROLL_API int32 Streak();
	ZOOGIROLL_API int32 BestToday();
	/** b157: the start-up pop-up - due once a day; mark it shown */
	ZOOGIROLL_API bool PopupDue();
	ZOOGIROLL_API void MarkPopupShown();
	/** BeginTurn: the human always shoots; places the layout at the start of each try */
	ZOOGIROLL_API int32 FixTurn(AZoogiGameMode* G, int32 PlayerIdx);
	/** EndTurn: true = the trick shot owns the turn end (scores the try, then the next try / the end) */
	ZOOGIROLL_API bool OnEndTurn(AZoogiGameMode* G);
	ZOOGIROLL_API void Tick(AZoogiGameMode* G, float RealDt);
	ZOOGIROLL_API void Draw(AZoogiGameMode* G, UCanvas* C);
	ZOOGIROLL_API void OnFinished(AZoogiGameMode* G);
	ZOOGIROLL_API void OnMenu(AZoogiGameMode* G);
}
