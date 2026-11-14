// ZoogiRoll.Hud.VideoTopSplit (Oct 22): crit / anime cut-in videos play at the top centre (not screen centre) in portrait
// and landscape, and the panel split eases 0 -> 1 smoothly (monotonic, symmetric) over ZoogiArenaHUD::SplitSeconds.
#include "ZoogiArenaHUDWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoogiVideoTopSplitTest, "ZoogiRoll.Hud.VideoTopSplit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZoogiVideoTopSplitTest::RunTest(const FString&)
{
	const FVector2D Sizes[] = { FVector2D(1080, 1920), FVector2D(1080, 2400), FVector2D(1920, 1080), FVector2D(2400, 1080) };
	for (const FVector2D& V : Sizes)
	{
		const FBox2D R = ZoogiArenaHUD::AnimeVideoRect(V);
		const FString K = FString::Printf(TEXT("%.0fx%.0f"), V.X, V.Y);
		TestTrue(K + TEXT(": horizontally centred"), FMath::IsNearlyEqual(R.GetCenter().X, V.X * 0.5f, 0.5f));
		TestTrue(K + TEXT(": in the upper half"), R.Max.Y <= V.Y * 0.5f);
		TestTrue(K + TEXT(": near the top edge"), R.Min.Y >= 0.f && R.Min.Y <= V.Y * 0.05f);
		TestTrue(K + TEXT(": 16:9"), FMath::IsNearlyEqual(R.GetSize().X / R.GetSize().Y, 16.f / 9.f, 0.01f));
		TestTrue(K + TEXT(": inside the screen"), R.Min.X >= 0.f && R.Max.X <= V.X);
	}
	TestEqual(TEXT("ease(0)"), ZoogiArenaHUD::SplitEase(0.f), 0.f);
	TestEqual(TEXT("ease(1)"), ZoogiArenaHUD::SplitEase(1.f), 1.f);
	TestTrue(TEXT("ease(0.5) = 0.5"), FMath::IsNearlyEqual(ZoogiArenaHUD::SplitEase(0.5f), 0.5f, 1e-4f));
	float Prev = 0.f;
	for (int32 k = 1; k <= 20; ++k) { const float E = ZoogiArenaHUD::SplitEase(k / 20.f); TestTrue(TEXT("monotonic"), E >= Prev); Prev = E; }
	TestTrue(TEXT("split takes ~0.3 s"), FMath::IsNearlyEqual(ZoogiArenaHUD::SplitSeconds, 0.3f, 0.05f));
	return true;
}

#endif
