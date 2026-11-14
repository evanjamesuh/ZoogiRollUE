# b133a (Evan 06:53: "we need to create an epic anime cutscene when SHOWTIME is pressed - first is the meteor strike"):
# SHOWTIME plays Content/Anime/Movies/Anime_Showtime_<Power>_01.mp4 FULL SCREEN (smoke wipe, not skippable) when that clip exists,
# then the rune seals / dim / target pick follow. Power keys: Explosion = MeteorStrike, WolfPack = WolfPack, StaticShock = BoltBalls,
# others by their enum name. Grok is making Anime_Showtime_MeteorStrike_01 first. -ZoogiNoShowtimeCut = off.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '#include "ZoogiActionFun.h"\n', '#include "ZoogiActionFun.h"\n#include "ZoogiAnimeSubsystem.h"\n#include "Misc/Paths.h"\n')
ed1(S, '''		if (GStage == EStage::Rise)
		{''', '''		if (GStage == EStage::Rise)
		{
			// b133: the SHOWTIME cutscene plays first (full screen) - the seals / dim / pick wait for it
			if (UZoogiAnimeSubsystem* An = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr) if (An->CrashOn()) { GT0 = Now(); SetDim(0.f); return true; }''')
ed1(S, '''	ZoogiHaptics::Rumble(2.f, 1.f, W, TEXT("showtime")); // 2 s''', '''	ZoogiHaptics::Rumble(2.f, 1.f, W, TEXT("showtime")); // 2 s
	// b133 (Evan: "an epic anime cutscene when SHOWTIME is pressed - first is the meteor strike")
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeCut")))
	{
		const EZoogiPower Pw = B->Def.PowerKind;
		const FString Key = Pw == EZoogiPower::Explosion ? FString(TEXT("MeteorStrike")) : Pw == EZoogiPower::WolfPack ? FString(TEXT("WolfPack")) : Pw == EZoogiPower::StaticShock ? FString(TEXT("BoltBalls"))
			: StaticEnum<EZoogiPower>()->GetNameStringByValue((int64)Pw);
		FString Clip = FString::Printf(TEXT("Anime_Showtime_%s_01"), *Key);
		FString Forced; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiShowtimeCut="), Forced)) Clip = Forced; // test: any clip
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Clip + TEXT(".mp4"))))
			if (UZoogiAnimeSubsystem* An = W ? W->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr)
				if (An->PlayCrash(Clip, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: cutscene %s (full screen)"), B->PlayerIndex + 1, *Clip);
	}''')
print('b133a ok')
