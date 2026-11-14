# b86 (Evan 12:08): bolt state cleared on leaving / restarting a match (no more stray yellow flashes), a knocked-out Zoogi loses its bolts,
# flickering vibration while armed with Bolt Balls, a bolt counter badge beside the wolf badge on SUPERPOWER
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('''	static void BoltTick(AZoogiGameMode* G)
	{
		if (!G || GBoltG.Get() != G) return;
		UWorld* W = G->GetWorld();
		const float Dt = FMath::Min(W->GetDeltaSeconds(), 0.05f);''',
  '''	static void BoltClearAll(const TCHAR* Why)
	{
		if (GBolts.Num() || GBoltShots.Num() || GStems.Num()) UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS cleared (%s)"), Why);
		for (auto& Kv : GStems) if (AActor* A = Kv.Value.A.Get()) A->Destroy();
		for (FBoltShot& Bs : GBoltShots) if (AActor* V = Bs.Vis.Get()) V->Destroy();
		GStems.Reset(); GBoltShots.Reset(); GBolts.Reset();
	}
	static void BoltTick(AZoogiGameMode* G)
	{
		// b86 (Evan: "the yellow electric keeps flashing after exiting and returning to the game"): a new match / the menu / game over
		// wipes every bolt, stem and shot; the wolf button state goes with it
		{
			static int32 LastRound = -1, LastTurn = -1;
			const bool bNewMatch = G && (G->RoundNumber < LastRound || G->TurnCounter < LastTurn);
			if (!G || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver || bNewMatch || GBoltG.Get() != G)
			{
				BoltClearAll(!G ? TEXT("no game") : (bNewMatch ? TEXT("new match") : TEXT("menu / game over")));
				if (bNewMatch || (G && G->Phase == EZoogiPhase::Menu)) GWolfArmed.Reset();
				if (G) { LastRound = G->RoundNumber; LastTurn = G->TurnCounter; }
				if (!G || GBoltG.Get() != G) return;
			}
			LastRound = G->RoundNumber; LastTurn = G->TurnCounter;
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
		}'''),
], inc='#include "ZoogiHaptics.h"\n#include "HAL/PlatformTime.h"', after='#include "ZoogiSkills.h"')
ed(P + 'ZoogiClassicPad.cpp', [
 ('		static UTexture2D* Tex[2] = { nullptr, nullptr }; // rooted: kept for the whole session',
  '		static UTexture2D* Tex[3] = { nullptr, nullptr, nullptr }; // rooted: kept for the whole session (b86: 2 = lightning bolt)'),
 ('''		if (Which == 0)
		{
			Poly = { {64, 10}, {112, 62}, {82, 62}, {82, 104}, {46, 104}, {46, 62}, {16, 62} };
		}''',
  '''		if (Which == 0)
		{
			Poly = { {64, 10}, {112, 62}, {82, 62}, {82, 104}, {46, 104}, {46, 62}, {16, 62} };
		}
		else if (Which == 2)
		{
			Poly = { {74, 6}, {28, 70}, {58, 70}, {44, 122}, {100, 50}, {70, 50}, {88, 6} }; // b86: lightning bolt
		}'''),
 ('''			// Wolf Pack charges
			if (bPower)''',
  '''			// b86 (Evan: "if both wolf and Bolt Balls are active, show the bolt counter next to the wolf's, separately round SUPERPOWER"):
			// bolts left on a yellow-ringed purple badge, top-right (the wolf badge is top-left)
			if (bPower)
				if (AZoogiGameMode* G = PadGM())
					if (const AZoogiBall* Bb = G->GetActiveBall())
						if (const int32 NB = ZoogiSkills::BoltCount(G, Bb->PlayerIndex))
						{
							const FVector2D BC = C + FVector2D(R * 0.86f, -R * 0.76f);
							const FLinearColor Yel = FLinearColor::FromSRGBColor(FColor(255, 222, 60));
							Disc(BC, R * 0.36f, FLinearColor::FromSRGBColor(FColor(90, 40, 170)), Yel, R * 0.06f, Layer + 5);
							if (UTexture2D* Bt = Icon(2))
							{
								FSlateBrush BB; BB.SetResourceObject(Bt); BB.ImageSize = FVector2D(128.f, 128.f);
								const float Is = R * 0.42f;
								FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2D(Is, Is), FSlateLayoutTransform(1.f, BC - FVector2D(Is, Is) * 0.5f - FVector2D(R * 0.08f, R * 0.06f))), &BB, ESlateDrawEffect::None, Yel);
							}
							Label(FString::Printf(TEXT("x%d"), NB), FMath::Max(7.f, R0 * 0.24f), BC + FVector2D(R * 0.3f, R * 0.3f), FLinearColor::White, Layer + 7);
						}
			// Wolf Pack charges
			if (bPower)'''),
])
print('b86 ok')
# b86 (Evan 12:10: "skills don't wait for clips to end to trigger"): the skill effect fires as the clip starts. -ZoogiSkillAfterClip = b70 wait
ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('	if (CrashState > 0) AfterCrashFns.Add(MoveTemp(Fn)); else Fn();',
  '	static const bool bWait = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillAfterClip")); // b86 (Evan): skills trigger with the clip, not after it\n	if (bWait && CrashState > 0) AfterCrashFns.Add(MoveTemp(Fn)); else Fn();'),
])
print('b86 skills-now ok')
