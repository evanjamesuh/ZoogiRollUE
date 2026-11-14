# b89b (Evan 14:55: "critical hit animation play on the scoreboard"): the critical clip plays on the HoloSpire scoreboard drum
# (the spire comes back for it) instead of the top card / avatar; Blue's P5 crit cut-in hands over to the clip. -ZoogiCritTop = old.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiHoloSpire.h', [
 ('''	ZOOGIROLL_API bool IsSP(const AZoogiOrb* O);
}''', '''	ZOOGIROLL_API bool IsSP(const AZoogiOrb* O);
}
/** b89b (Evan: "critical hit animation play on the scoreboard"): true while a critical anime clip plays and a spire is up to show it
 *  on its scoreboard drum (the HUD then draws no card for it). -ZoogiCritTop = the old top card / P5 cut-in. */
class UWorld;
namespace ZoogiCritSpire
{
	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API bool On(const UWorld* W);
}'''),
 ('	TArray<USceneComponent*> OrbRingPivots;\n};', '	TArray<USceneComponent*> OrbRingPivots;\n	bool bCritTex = false; // b89b: the drum shows the critical clip\n};'),
])
ed(P + 'ZoogiHoloSpire.cpp', [
 ('#include "ZoogiCardSubsystem.h"\n', '#include "ZoogiCardSubsystem.h"\n#include "ZoogiAnimeSubsystem.h"\n#include "MediaTexture.h"\n'),
 ('''void AZoogiHoloSpire::Tick(float Dt)''',
  '''namespace ZoogiCritSpire
{
	bool Enabled() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCritTop")); return b; }
	static UMediaTexture* Clip(const UWorld* W)
	{
		if (!Enabled() || !W) return nullptr;
		const UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>();
		if (!An || !An->bActive || !An->HasMovie() || An->Current.Event != FName(TEXT("Critical")) || !An->MovieTexture) return nullptr;
		return An->MovieTexture.Get();
	}
	bool On(const UWorld* W)
	{
		if (!Clip(W)) return false;
		const UZoogiHoloSpireSys* Ss = W->GetSubsystem<UZoogiHoloSpireSys>();
		return Ss && Ss->Spire.IsValid();
	}
}

void AZoogiHoloSpire::Tick(float Dt)'''),
 ('''	if (bPortraits != bBandIsPortraits) { SetTex(QDrum, bPortraits ? RT_Band : RT_Score); bBandIsPortraits = bPortraits; }''',
  '''	if (bPortraits != bBandIsPortraits) { SetTex(QDrum, bPortraits ? RT_Band : RT_Score); bBandIsPortraits = bPortraits; }
	if (UMediaTexture* Ct = ZoogiCritSpire::Clip(W))
	{	// b89b: the critical clip on the scoreboard drum
		if (Mids.IsValidIndex(QDrum) && Mids[QDrum]) Mids[QDrum]->SetTextureParameterValue(TEXT("Tex"), Ct);
		if (!bCritTex) UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE critical clip on the scoreboard"));
		bCritTex = true;
	}
	else if (bCritTex) { SetTex(QDrum, bBandIsPortraits ? RT_Band : RT_Score); bCritTex = false; }'''),
 ('''		const bool bShow = bAlways || (bShowRound && !bLaunched) || ZoogiSpireOrbs::Emitting(); // b86: up while it throws the new orbs
		const float Was = FadeK;
		FadeK = FMath::FInterpConstantTo(FadeK, bShow ? 1.f : 0.f, W->GetDeltaSeconds(), 1.f / 0.8f);''',
  '''		const bool bCritUp = ZoogiCritSpire::On(W); // b89b: up for a critical clip on the scoreboard
		const bool bShow = bAlways || (bShowRound && !bLaunched) || ZoogiSpireOrbs::Emitting() || bCritUp; // b86: up while it throws the new orbs
		const float Was = FadeK;
		FadeK = FMath::FInterpConstantTo(FadeK, bShow ? 1.f : 0.f, W->GetDeltaSeconds(), bCritUp ? 1.f / 0.2f : 1.f / 0.8f);'''),
])
ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('	bP5CutIn = !E.bRewardShatter && Ev == TEXT("Critical") && ZoogiP5::CutInFor(E.ZoogiName); // reward shatter always hands off to the movie',
  '	bP5CutIn = !E.bRewardShatter && Ev == TEXT("Critical") && ZoogiP5::CutInFor(E.ZoogiName) && !ZoogiCritSpire::Enabled(); // reward shatter always hands off to the movie; b89b: crits play their clip on the spire scoreboard'),
 ('	else if (CrashState == 0 && bActive && bMovie && !bP5CutIn && !bPanelMode && MovieTexture && MovieHasFrames()) Pl = Current.Player; // b88b: combo / character cut-ins',
  '	else if (CrashState == 0 && bActive && bMovie && !bP5CutIn && !bPanelMode && MovieTexture && MovieHasFrames() && !ZoogiCritSpire::On(GetWorld())) Pl = Current.Player; // b88b: combo / character cut-ins (b89b: not the crit on the spire)'),
])
cp = os.path.join(ROOT, P + 'ZoogiAnimeSubsystem.cpp')
s = open(cp, 'rb').read().decode('utf-8-sig')
if '#include "ZoogiHoloSpire.h"' not in s:
    ed(P + 'ZoogiAnimeSubsystem.cpp', [], '#include "ZoogiHoloSpire.h"', '#include "ZoogiArenaHUDWidget.h"')
ed(P + 'ZoogiHUD.cpp', [
 ('	{ int32 Ap = -1; if (Anime->AviClip(Ap)) return; } // b88b', '	if (ZoogiCritSpire::On(GetWorld())) return; // b89b: the critical clip plays on the spire scoreboard\n	{ int32 Ap = -1; if (Anime->AviClip(Ap)) return; } // b88b'),
])
cp = os.path.join(ROOT, P + 'ZoogiHUD.cpp')
s = open(cp, 'rb').read().decode('utf-8-sig')
if '#include "ZoogiHoloSpire.h"' not in s:
    i = s.index('#include "ZoogiAnimeSubsystem.h"')
    ed(P + 'ZoogiHUD.cpp', [], '#include "ZoogiHoloSpire.h"', '#include "ZoogiAnimeSubsystem.h"')
print('b89b ok')
