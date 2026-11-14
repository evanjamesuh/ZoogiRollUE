# b142g (Evan 11:51 "I'm still not seeing the awaken activate. After SHOWTIME is pressed for Hotstreak make the last superpower press
# [fire the] meteor rain effect, then the full screen anime plays before resuming gameplay"): SHOWTIME now ARMS the Awaken (rumble,
# seals, dim, target pick - no clip up front). The SUPERPOWER button lights; pressing it (aiming or rolling, same turn) fires the
# Awaken power - Hotstreak: a 10-meteor rain over the arena at full strength (the others: their Awaken skill) - then the full screen
# anime (Anime_Showtime_<Power>_01, _02) plays 1.8 s later, then the game carries on. Not used by the end of the turn = the AWAKEN
# meter comes back. -ZoogiShowtimeCutFirst = the b134 order (clips first, sky cam meteor strike).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiShowtime.h'
ed1(H, '''	ZOOGIROLL_API void Tap(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Screen);''', '''	ZOOGIROLL_API void Tap(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Screen);
	/** b142: SHOWTIME armed this player's Awaken for this turn (the SUPERPOWER button fires it) */
	ZOOGIROLL_API bool Armed(const AZoogiGameMode* G, int32 Player);
	/** b142: SUPERPOWER with an armed Awaken: the power (Hotstreak = meteor rain), then the full screen anime. True = fired. */
	ZOOGIROLL_API bool FireArmed(AZoogiGameMode* G);''')
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '''#include "Misc/Parse.h"''', '''#include "Misc/Parse.h"
#include "ZoogiFront.h"''')
ed1(S, '''	float GSpin = 0.f;''', '''	float GSpin = 0.f;
	// b142: the armed Awaken (SHOWTIME -> SUPERPOWER)
	TWeakObjectPtr<AZoogiGameMode> GArmG; int32 GArmP = -1, GArmTurn = -1; FString GArmKey; bool GArmWatch = false;
	TArray<FString> GFireClips; double GFireT0 = -1.0; TWeakObjectPtr<AZoogiGameMode> GFireG; bool GFireTick = false;''')
ed1(S, '''	GClips.Reset(); GSkyCam = false; // b134
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeCut")))
	{''', '''	GClips.Reset(); GSkyCam = false; // b134
	static const bool bCutFirst = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeCutFirst"));
	if (!bCutFirst)
	{	// b142 (Evan): SHOWTIME arms the Awaken - the SUPERPOWER press fires it, then the anime
		const EZoogiPower Pa = B->Def.PowerKind;
		GArmKey = Pa == EZoogiPower::Explosion ? FString(TEXT("MeteorStrike")) : Pa == EZoogiPower::WolfPack ? FString(TEXT("WolfPack")) : Pa == EZoogiPower::StaticShock ? FString(TEXT("BoltBalls"))
			: StaticEnum<EZoogiPower>()->GetNameStringByValue((int64)Pa);
		GArmG = G; GArmP = B->PlayerIndex; GArmTurn = G->TurnCounter;
		if (UZoogiFunSys* Fa = UZoogiFunSys::Get(G)) Fa->PublicCallout(TEXT("AWAKEN ARMED!"), TEXT("PRESS SUPERPOWER TO UNLEASH IT"), FLinearColor(1.f, 0.6f, 0.2f), 2.2f, 1.1f);
		UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: Awaken armed (%s) - SUPERPOWER fires it this turn"), B->PlayerIndex + 1, *GArmKey);
		if (!GArmWatch)
		{	// not used by the end of the turn: the AWAKEN meter comes back
			GArmWatch = true;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool
			{
				AZoogiGameMode* Ga = GArmG.Get();
				if (GArmP < 0) return true;
				if (!Ga) { GArmP = -1; return true; }
				if (Ga->TurnCounter != GArmTurn || Ga->Phase == EZoogiPhase::Menu || Ga->Phase == EZoogiPhase::GameOver)
				{
					if (UZoogiFunSys* Fr = UZoogiFunSys::Get(Ga)) Fr->FillMeterForTest(Ga, GArmP);
					UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: armed Awaken not used this turn - AWAKEN meter refunded"), GArmP + 1);
					GArmP = -1;
				}
				return true;
			}));
		}
	}
	if (bCutFirst && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeCut")))
	{''')
ed1(S, '''bool ZoogiShowtime::Picking() { return GStage == EStage::Pick; }''', '''bool ZoogiShowtime::Picking() { return GStage == EStage::Pick; }

bool ZoogiShowtime::Armed(const AZoogiGameMode* G, int32 Player)
{
	return G && GArmP >= 0 && GArmG.Get() == G && GArmP == Player && G->TurnCounter == GArmTurn;
}

bool ZoogiShowtime::FireArmed(AZoogiGameMode* G)
{
	AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	if (!B || !Armed(G, B->PlayerIndex) || GStage == EStage::Pick) return false;
	GArmP = -1;
	UWorld* W = G->GetWorld();
	const FVector Ctr = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
	const float KR = G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f;
	ZoogiSkills::SetAwakenStrength();
	const ZoogiFront::FAwaken* Aw = ZoogiFront::AwakenFor(B, G);
	if (GArmKey == TEXT("MeteorStrike") || !Aw || !Aw->Skill || !*Aw->Skill)
	{	// Hotstreak's METEOR STRIKE: the meteor rain over the arena, full strength
		ZoogiSkills::MeteorRain(G, Ctr, KR * 0.75f, B->PlayerIndex, 10, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: SUPERPOWER - Awaken METEOR RAIN (10 meteors, full strength)"), B->PlayerIndex + 1);
	}
	else
	{
		ZoogiSkills::AwakenCast(G, B, FName(Aw->Skill));
		UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: SUPERPOWER - Awaken %s"), B->PlayerIndex + 1, Aw->Skill);
	}
	ZoogiHaptics::Rumble(1.5f, 0.9f, W, TEXT("awaken_fire"));
	ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.7f);
	if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(Aw && Aw->Name ? FString(Aw->Name) : FString(TEXT("METEOR STRIKE")), TEXT("AWAKEN!"), FLinearColor(1.f, 0.5f, 0.15f), 1.8f, 1.2f);
	// then the full screen anime, once the effect has landed
	GFireClips.Reset();
	for (int32 k = 1; k <= 2; ++k)
	{
		const FString Clip = FString::Printf(TEXT("Anime_Showtime_%s_%02d"), *GArmKey, k);
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Clip + TEXT(".mp4")))) GFireClips.Add(Clip);
	}
	GFireT0 = Now(); GFireG = G;
	if (!GFireTick && GFireClips.Num())
	{
		GFireTick = true;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool
		{
			AZoogiGameMode* Gf = GFireG.Get();
			if (!Gf || GFireClips.Num() == 0) { GFireClips.Reset(); GFireTick = false; return false; }
			if (Now() - GFireT0 < 1.8) return true;
			UZoogiAnimeSubsystem* An = Gf->GetWorld() ? Gf->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr;
			if (!An) { GFireClips.Reset(); GFireTick = false; return false; }
			if (An->CrashOn()) return true;
			const FString Next = GFireClips[0]; GFireClips.RemoveAt(0);
			if (An->PlayCrash(Next, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: Awaken anime %s (full screen)"), *Next);
			return true;
		}));
	}
	if (GFireClips.Num() == 0) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: no Anime_Showtime_%s clip - effect only"), *GArmKey);
	return true;
}''')
P = 'Private/ZoogiPowers.cpp'
ed1(P, '''	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) if (Fun->TryUltimate(this)) return true; // build 25: full SUPER meter -> signature ultimate''', '''	if (ZoogiShowtime::FireArmed(this)) return true; // b142: SHOWTIME armed the Awaken -> SUPERPOWER fires it
	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) if (Fun->TryUltimate(this)) return true; // build 25: full SUPER meter -> signature ultimate''')
ed1(P, '''	if (!B || B->bCPU || B->bOut) return false;
	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) if (Fun->UltimateReady(this, B->PlayerIndex)) return true; // build 25: ultimate ready''', '''	if (!B || B->bCPU || B->bOut) return false;
	if (ZoogiShowtime::Armed(this, B->PlayerIndex)) return true; // b142: the armed Awaken
	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) if (Fun->UltimateReady(this, B->PlayerIndex)) return true; // build 25: ultimate ready''')
ed1(P, '''	if (!B || B->bCPU || bRoyaleMode) return false;
	if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(this)) if (WP->HumanPackOut()) { WP->OnPowerTap(); return true; } // Nov 1: tap again = RECALL''', '''	if (!B || B->bCPU || bRoyaleMode) return false;
	if (ZoogiShowtime::FireArmed(this)) return true; // b142: SHOWTIME armed the Awaken -> SUPERPOWER fires it
	if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(this)) if (WP->HumanPackOut()) { WP->OnPowerTap(); return true; } // Nov 1: tap again = RECALL''')
print('b142g ok')
