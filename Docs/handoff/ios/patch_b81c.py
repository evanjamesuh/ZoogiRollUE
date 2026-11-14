# b81c (Evan): Frozen launch rail ("the coloured slide") deleted; stage-shift ice walls stay and grow every round
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiAerialPlay.cpp', [
 ('	if (AZoogiAerialProp* Rail = SpawnProp(EZoogiPropKind::LaunchRail, S, F, 60.f, false, TEXT("map_rail")))',
  '	if (AZoogiAerialProp* Rail = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMapRail")) ? SpawnProp(EZoogiPropKind::LaunchRail, S, F, 60.f, false, TEXT("map_rail")) : nullptr) // b81 (Evan: "delete the coloured slide"): -ZoogiMapRail brings it back'),
])
ed(P + 'ZoogiStageShift.cpp', [
 ('''	void OnRoundStart(AZoogiGameMode* G)
	{
		if (Off() || bShifted || !G || !G->Arena || G->bRoyaleMode || G->bComboMode) return;''',
  '''	void OnRoundStart(AZoogiGameMode* G)
	{
		// b81 (Evan: "the ice walls that form as the stage changes increase in size each round"): +25% height / +10% thickness per round (cap 2.5x)
		if (bShifted && G && G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiIceWallsFixed")))
		{
			int32 Grown = 0;
			for (TActorIterator<AZoogiIceBarrier> It(G->GetWorld()); It; ++It)
			{
				const FVector Sc = It->GetActorScale3D();
				It->SetActorScale3D(FVector(Sc.X, FMath::Min(Sc.Y * 1.1f, 1.6f), FMath::Min(Sc.Z * 1.25f, 2.5f)));
				AZoogiFX::Spawn(G->GetWorld(), It->GetActorLocation(), FLinearColor(0.7f, 0.9f, 1.f), 220.f, 0.5f, true, 8.f);
				++Grown;
			}
			if (Grown) UE_LOG(LogZoogi, Log, TEXT("STAGE SHIFT: %d ice walls grow for round %d"), Grown, G->RoundNumber);
		}
		if (Off() || bShifted || !G || !G->Arena || G->bRoyaleMode || G->bComboMode) return;'''),
 ('AZoogiIceBarrier::Spawn(GM->GetWorld(), Floor - Tan * 150.f, Floor + Tan * 150.f, 160.f, -1, 2);',
  'AZoogiIceBarrier::Spawn(GM->GetWorld(), Floor - Tan * 150.f, Floor + Tan * 150.f, 160.f, -1, 99); // b81: they stay for the rest of the match (growing each round)'),
], inc='#include "EngineUtils.h"', after='#include "ZoogiStageShift.h"')
print('b81c ok')
