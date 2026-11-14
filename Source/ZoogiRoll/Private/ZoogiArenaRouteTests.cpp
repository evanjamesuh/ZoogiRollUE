// ZoogiRoll.Arena.RouteEveryMode (Oct 22): every arena pick loads that arena in every mode - Home BATTLE (matchmaking),
// Custom classic + 2v2, Trophy Road / Arenas gate, web Story + web arena screen - and TRAINING always loads the Training
// Arena. Also: the game mode's ComboArenaFor (used by ComboSetMode / ComboStartMatch) and the web id table agree, and
// building an AZoogiArena for each resolved pick reports that theme (the "loaded map").
#include "ZoogiArenaRoute.h"
#include "ZoogiArena.h"
#include "ZoogiGameMode.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiArenaRouteTest, "ZoogiRoll.Arena.RouteEveryMode", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZoogiArenaRouteTest::RunTest(const FString&)
{
	static const TCHAR* RouteNames[] = { TEXT("HomeBattle"), TEXT("CustomClassic"), TEXT("Custom2v2"), TEXT("TrophyRoad"), TEXT("Story"), TEXT("WebArena"), TEXT("Training") };
	static_assert(UE_ARRAY_COUNT(RouteNames) == (int32)EZoogiMatchRoute::MAX, "route names");

	// the build target for every (mode, pick) pair
	TMap<EZoogiArena, EZoogiArena> NeedBuilt; // resolved arena -> itself (set of arenas the matrix needs)
	for (int32 r = 0; r < (int32)EZoogiMatchRoute::MAX; ++r)
		for (int32 a = 0; a < (int32)EZoogiArena::MAX; ++a)
		{
			const EZoogiMatchRoute R = (EZoogiMatchRoute)r;
			const EZoogiArena Pick = (EZoogiArena)a;
			// what the entry point hands the game mode: web routes go through the page's arena id, trophy through the gate index
			EZoogiArena Entered = Pick;
			if (R == EZoogiMatchRoute::Story || R == EZoogiMatchRoute::WebArena) Entered = ZoogiArenaRoute::ForWebId(ZoogiArenaRoute::WebId(Pick));
			if (R == EZoogiMatchRoute::TrophyRoad) Entered = ZoogiArenaRoute::ForTrophyGate(a);
			const EZoogiArena Got = ZoogiArenaRoute::Resolve(R, Entered);
			EZoogiArena Want = R == EZoogiMatchRoute::Training ? EZoogiArena::Teaser : Pick;
			if (Pick == EZoogiArena::Ancient && Got != EZoogiArena::Ancient) Want = EZoogiArena::Meadow; // Oct 31: Ancient = Battle Royale only (combo routes fall back to Meadow)
			TestEqual(FString::Printf(TEXT("%s: pick %s loads %s"), RouteNames[r], *ZoogiUtil::ArenaName(Pick), *ZoogiUtil::ArenaName(Want)), (int32)Got, (int32)Want);
			NeedBuilt.Add(Got, Got);
		}

	// the game mode's own combo choice (ComboSetMode / ComboStartMatch) keeps the pick; the scripted demo stays on Meadow
	for (int32 a = 0; a < (int32)EZoogiArena::MAX; ++a)
		TestEqual(FString::Printf(TEXT("ComboArenaFor(%s)"), *ZoogiUtil::ArenaName((EZoogiArena)a)), (int32)AZoogiGameMode::ComboArenaFor((EZoogiArena)a, 0), (EZoogiArena)a == EZoogiArena::Ancient ? (int32)EZoogiArena::Meadow : a);
	TestEqual(TEXT("combo demo plays Meadow"), (int32)AZoogiGameMode::ComboArenaFor(EZoogiArena::Frozen, 1), (int32)EZoogiArena::Meadow);
	TestEqual(TEXT("web skyfall -> Frozen Ring (no level yet)"), (int32)ZoogiArenaRoute::ForWebId(TEXT("skyfall")), (int32)EZoogiArena::Frozen);
	TestEqual(TEXT("web pharaoh -> Pharaoh's Tomb"), (int32)ZoogiArenaRoute::ForWebId(TEXT("pharaoh")), (int32)EZoogiArena::Tomb);
	TestEqual(TEXT("web night -> Night Circuit"), (int32)ZoogiArenaRoute::ForWebId(TEXT("night")), (int32)EZoogiArena::Neon);

	// the loaded map: build each resolved arena in a scratch world and check the arena actor reports that theme
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ZoogiArenaRouteTest"));
	if (TestNotNull(TEXT("scratch world"), W))
	{
		FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
		Ctx.SetCurrentWorld(W);
		W->InitializeActorsForPlay(FURL());
		if (AZoogiArena* Ar = W->SpawnActor<AZoogiArena>(AZoogiArena::StaticClass(), FTransform::Identity))
		{
			for (const TPair<EZoogiArena, EZoogiArena>& P : NeedBuilt)
			{
				Ar->Build(P.Key);
				TestEqual(FString::Printf(TEXT("built %s"), *ZoogiUtil::ArenaName(P.Key)), (int32)Ar->GetTheme(), (int32)P.Key);
			}
			Ar->Destroy();
		}
		else AddError(TEXT("could not spawn AZoogiArena"));
		GEngine->DestroyWorldContext(W);
		W->DestroyWorld(false);
	}
	return true;
}

#endif
