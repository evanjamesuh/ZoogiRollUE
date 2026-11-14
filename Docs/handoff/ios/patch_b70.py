# b70 patch (Claude): skill videos = freeze + one random clip + effect after; redirect hit feel; replay cam toggle; stance only at turn start
import os, sys
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def ed(rel, pairs, inc=None, after=None):
    p = os.path.join(ROOT, rel)
    raw = open(p, 'rb').read(); bom = raw[:3] == b'\xef\xbb\xbf'; s = raw.decode('utf-8-sig'); nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in pairs:
        a = a.replace('\n', nl); b = b.replace('\n', nl)
        assert s.count(a) == 1, (rel, a[:80], s.count(a)); s = s.replace(a, b)
    if inc and inc not in s:
        i = s.index(after); j = s.index(nl, i) + len(nl); s = s[:j] + inc + nl + s[j:]
    open(p, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline='').write(s)

# ---- ZoogiUtil::PlaySfxShort
ed('Public/ZoogiTypes.h', [('	ZOOGIROLL_API void PlaySfx(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f, float StartTime = 0.f);\n',
   '	ZOOGIROLL_API void PlaySfx(const UObject* WorldContext, const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f, float StartTime = 0.f);\n	/** b70 (Evan: "sound doesn\'t linger"): the same sound, faded out after MaxSeconds. */\n	ZOOGIROLL_API void PlaySfxShort(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch, float MaxSeconds);\n')])
ed('Private/ZoogiTypes.cpp', [('UStaticMesh* ZoogiUtil::BasicShape(const TCHAR* Name)\n', '''void ZoogiUtil::PlaySfxShort(const UObject* WorldContext, const TCHAR* Name, float Volume, float Pitch, float MaxSeconds)
{
	if (GSfxVolume <= 0.001f) return;
	USoundBase* S = nullptr; float V = Volume;
	if (const FRealSfx* R = RealSfx().Find(Name)) { S = RealVariant(R->Main, Name); V *= R->Vol; }
	if (!S) S = LoadSound(Name);
	if (!S) return;
	UAudioComponent* AC = UGameplayStatics::SpawnSound2D(WorldContext, S, FMath::Min(V * GSfxVolume, 3.f), Pitch, 0.f, nullptr, true, true);
	if (!AC) return;
	TWeakObjectPtr<UAudioComponent> WAC = AC;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WAC](float) { if (UAudioComponent* A = WAC.Get()) A->FadeOut(0.08f, 0.f); return false; }), FMath::Max(0.05f, MaxSeconds));
}

UStaticMesh* ZoogiUtil::BasicShape(const TCHAR* Name)
''')], '#include "Components/AudioComponent.h"\n#include "Containers/Ticker.h"', '#include "ZoogiTypes.h"')

# ---- redirect hits: swifter, hard tap + shake + short rock crunch
ed('Private/ZoogiRedirect.cpp', [
 ('		bool bCpuPick = false; TWeakObjectPtr<AActor> CpuGoal; double CpuTapAt = 0.0;\n',
  '		bool bCpuPick = false; TWeakObjectPtr<AActor> CpuGoal; double CpuTapAt = 0.0;\n		TWeakObjectPtr<AActor> HitWatch; double HitWatchUntil = 0.0; // b70: the redirect\'s impact (feel)\n'),
 ('			const float Spd = FMath::Clamp(FMath::Max(Cur, 1300.f) * 1.15f, 1300.f, 2600.f);',
  '			const float Spd = FMath::Clamp(FMath::Max(Cur, 1500.f) * 1.25f, 1500.f, 3000.f); // b70 (Evan): "powerful and swift"'),
 ('			Homing = T; HomingUntil = Now() + 0.9;\n', '			Homing = T; HomingUntil = Now() + 0.9;\n			HitWatch = T; HitWatchUntil = Now() + 1.8;\n'),
 ('		// Bolt Balls: the launch itself zaps once the ball is moving\n', '''		// b70 (Evan: "each hit of a sequence reaction should feel powerful and swift - hard vibration each hit, camera shake and a slight
		// rock-crushing sound that doesn't linger")
		if (AActor* Ht = HitWatch.Get())
		{
			if (!A || Now() > HitWatchUntil || G->Phase != EZoogiPhase::Rolling) HitWatch.Reset();
			else if (FVector::Dist2D(Loc(Ht), A->BallLocation()) < ZoogiConst::ZoogiRadius * 2.f + 30.f)
			{
				HitWatch.Reset();
				UWorld* W = G->GetWorld();
				ZoogiHaptics::Rumble(0.16f, 1.f, W, TEXT("redirect hit"));
				if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(W->GetFirstPlayerController())) PC->OnImpactFeel(0.9f);
				ZoogiUtil::PlaySfxShort(G, TEXT("RockCrack"), 0.85f, FMath::FRandRange(1.0f, 1.15f), 0.3f);
				ZoogiUtil::PlaySfx(G, TEXT("HitClick"), 0.9f, 1.15f);
				AZoogiFX::Spawn(W, (Loc(Ht) + A->BallLocation()) * 0.5f, FLinearColor(1.f, 0.9f, 0.7f), 160.f, 0.18f, true, 10.f);
				UE_LOG(LogZoogi, Log, TEXT("REDIRECT hit %s (combo %d)"), *Ht->GetName(), ComboN);
			}
		}
		// Bolt Balls: the launch itself zaps once the ball is moving
'''),
], '#include "ZoogiPlayerController.h"', '#include "ZoogiHaptics.h"')

# ---- anime: power intro = one random clip of the compilation, world stopped, callbacks after
ed('Public/ZoogiAnimeSubsystem.h', [
 ('	TSet<FString> IntroSeen;\n', '''	TSet<FString> IntroSeen;
	/** b70 (Evan): a skill's anime is on - gameplay stops under it */
	bool PowerIntroOn() const { return CrashState > 0 && CrashId.StartsWith(TEXT("Anime_Awaken_")); }
	/** b70: runs Fn when the current crash / power clip has ended (now when none is playing) */
	void AfterCrash(TFunction<void()> Fn);
	TArray<TFunction<void()>> AfterCrashFns;
	float CrashSeekTo = -1.f, CrashSegStart = 0.f;
	TMap<FString, int32> LastIntroSeg;
'''),
])
ed('Private/ZoogiAnimeSubsystem.cpp', [
 ('	if (IntroSeen.Contains(Seen) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerIntroAlways"))) return false;\n	if (!PlayCrash(Id, Tint)) return false;\n	IntroSeen.Add(Seen);\n',
  '''	static const bool bOnce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerIntroOnce")); // b70: every cast plays (one clip, different each time)
	if (bOnce && IntroSeen.Contains(Seen)) return false;
	if (!PlayCrash(Id, Tint)) return false;
	IntroSeen.Add(Seen);
	// b70 (Evan: "from these videos only play one of the clips of the compilation, at random, different every time"): the
	// Anime_Awaken_* files are 4 clips of ~3.77 s back to back (scene cuts at 3.77 / 7.53 / 11.30 s)
	static const bool bFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerIntroFull"));
	const float* Secs = MovieSeconds.Find(Id);
	const float Total = Secs ? *Secs : 15.067f;
	const int32 N = FMath::Max(1, FMath::RoundToInt(Total / 3.7667f));
	if (!bFull && N > 1)
	{
		const float Seg = Total / N;
		int32& Last = LastIntroSeg.FindOrAdd(Key, -1);
		int32 Pick = FMath::RandRange(0, N - 1);
		if (Pick == Last) Pick = (Pick + 1 + FMath::RandRange(0, N - 2)) % N;
		Last = Pick;
		CrashSegStart = Pick * Seg; CrashSeekTo = Pick > 0 ? CrashSegStart : -1.f;
		CrashLen = Seg - 0.06f;
		UE_LOG(LogZoogi, Log, TEXT("Anime power intro %s: clip %d of %d (%.2f - %.2f s)"), *Id, Pick + 1, N, CrashSegStart, CrashSegStart + Seg);
	}
	else CrashSegStart = 0.f;
'''),
 ('		if (CrashState == 1 && CrashAge >= 0.35f) {',
  '		if (CrashSeekTo >= 0.f && MoviePlayer && MoviePlayer->IsReady() && MoviePlayer->Seek(FTimespan::FromSeconds(CrashSeekTo))) CrashSeekTo = -1.f; // b70: jump to the picked clip\n		if (CrashState == 1 && CrashAge >= 0.35f) {'),
 ('			const bool bFrames = MoviePlayer && MoviePlayer->IsPlaying() && MoviePlayer->GetTime() > FTimespan::FromMilliseconds(30);',
  '			const bool bFrames = MoviePlayer && MoviePlayer->IsPlaying() && MoviePlayer->GetTime() > FTimespan::FromMilliseconds(30) && CrashSeekTo < 0.f && MoviePlayer->GetTime() >= FTimespan::FromSeconds(FMath::Max(0.f, CrashSegStart - 0.05f));'),
 ('			if (CrashAge >= 0.45f) { CrashState = 0; if (MoviePlayer) MoviePlayer->Close(); bMovie = false; CurrentMovie.Reset(); UE_LOG(LogZoogi, Log, TEXT("Anime crash %s done"), *CrashId); }',
  '			if (CrashAge >= 0.45f)\n			{\n				CrashState = 0; CrashSeekTo = -1.f; CrashSegStart = 0.f; if (MoviePlayer) MoviePlayer->Close(); bMovie = false; CurrentMovie.Reset(); UE_LOG(LogZoogi, Log, TEXT("Anime crash %s done"), *CrashId);\n				TArray<TFunction<void()>> Fns = MoveTemp(AfterCrashFns); AfterCrashFns.Reset(); // b70: the skill\'s effect now that the view is back\n				for (TFunction<void()>& Fn : Fns) if (Fn) Fn();\n			}'),
 ('bool UZoogiAnimeSubsystem::PlayPowerIntro(const FString& Key, int32 Player, const FLinearColor& Tint)\n',
  'void UZoogiAnimeSubsystem::AfterCrash(TFunction<void()> Fn)\n{\n	if (!Fn) return;\n	if (CrashState > 0) AfterCrashFns.Add(MoveTemp(Fn)); else Fn();\n}\n\nbool UZoogiAnimeSubsystem::PlayPowerIntro(const FString& Key, int32 Player, const FLinearColor& Tint)\n'),
])
# world stops under a skill clip
ed('Private/ZoogiActionFun.cpp', [
 ('			else if (UZoogiAnimeSubsystem* AnC = W->GetSubsystem<UZoogiAnimeSubsystem>(); AnC && AnC->CrashOn()) Want = 0.03f;',
  '			else if (UZoogiAnimeSubsystem* AnC = W->GetSubsystem<UZoogiAnimeSubsystem>(); AnC && AnC->CrashOn()) Want = AnC->PowerIntroOn() ? 0.0001f : 0.03f; // b70: a skill\'s clip stops the game'),
 # replay cam toggle
 ('	if (!bFinisher && (!bOn || !Best.bAction || Best.Frames.Num() < 10)) { if (bRoundEnd) Best.Reset(); return false; }',
  '	if (!bFinisher && (!bOn || !Best.bAction || Best.Frames.Num() < 10)) { if (bRoundEnd) Best.Reset(); return false; }\n	if (!ZoogiControls::ReplayCam()) { if (bRoundEnd) Best.Reset(); return false; } // b70 (Evan): replay cam off unless switched on in Settings'),
])
ed('Public/ZoogiFront.h', [('	ZOOGIROLL_API float FlashMul();', '	ZOOGIROLL_API bool ReplayCam();    // b70 (Evan): the replay camera, off by default (Settings > REPLAY CAM)\n	ZOOGIROLL_API float FlashMul();')])
ed('Private/ZoogiFront.cpp', [
 ('	float FlashMul() { return Get(TEXT("ReduceFlash"), 0.f, 0.f, 1.f) > 0.5f ? 0.f : 0.45f; }',
  '	float FlashMul() { return Get(TEXT("ReduceFlash"), 0.f, 0.f, 1.f) > 0.5f ? 0.f : 0.45f; }\n	bool ReplayCam() { return FParse::Param(FCommandLine::Get(), TEXT("ZoogiReplayCam")) || Get(TEXT("ReplayCam"), 0.f, 0.f, 1.f) > 0.5f; }'),
 ('		Toggle(TEXT("REDUCE FLASHES"), bNoFlash ? TEXT("ON") : TEXT("OFF"), bNoFlash, [Click, bNoFlash]() { ZoogiControls::Set(TEXT("ReduceFlash"), bNoFlash ? 0.f : 1.f); if (Click) Click(1.f); });',
  '		Toggle(TEXT("REDUCE FLASHES"), bNoFlash ? TEXT("ON") : TEXT("OFF"), bNoFlash, [Click, bNoFlash]() { ZoogiControls::Set(TEXT("ReduceFlash"), bNoFlash ? 0.f : 1.f); if (Click) Click(1.f); });\n		const bool bRep = ZoogiControls::ReplayCam(); // b70 (Evan): replay cam optional\n		Toggle(TEXT("REPLAY CAM"), bRep ? TEXT("ON") : TEXT("OFF"), bRep, [Click, bRep]() { ZoogiControls::Set(TEXT("ReplayCam"), bRep ? 0.f : 1.f); if (Click) Click(1.f); });'),
])
print('patched part 1')
