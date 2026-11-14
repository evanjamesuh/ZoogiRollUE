# b101a: MASTERCLASS film hooks (all inert without -ZoogiFilm / the test flags)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiGameMode.cpp', [
 ('	if (FParse::Param(Cmd, TEXT("ZoogiKFXPreview"))) GetWorld()->SpawnActor<AZoogiKFXPreview>(); // review captures of the KriptoFX ports',
  '	if (FParse::Param(Cmd, TEXT("ZoogiKFXPreview"))) GetWorld()->SpawnActor<AZoogiKFXPreview>(); // review captures of the KriptoFX ports\n	if (ZoogiFilm::On()) ZoogiFilm::Start(GetWorld()); // b101: the MASTERCLASS film recorder'),
 ('#include "ZoogiKFXPreview.h"\n', '#include "ZoogiKFXPreview.h"\n#include "ZoogiFilm.h"\n'),
])
ed(P + 'ZoogiSkills.cpp', [
 ('''		{	// b97a: -ZoogiSkillOnly=<id> - that skill only, every 6 s, frames through the effect
			NextTest = T + 6.f; ++TestIdx;
			UE_LOG(LogZoogi, Log, TEXT("SKILL_TEST only %s #%d"), *OnlySkill, TestIdx);
			AutoCast(G, FName(*OnlySkill));''', '''		{	// b97a: -ZoogiSkillOnly=<id> - that skill only, every 6 s, frames through the effect; b101: "a+b" = a combo (b 1.8 s after a)
			NextTest = T + 7.f; ++TestIdx;
			UE_LOG(LogZoogi, Log, TEXT("SKILL_TEST only %s #%d"), *OnlySkill, TestIdx);
			TArray<FString> Combo; OnlySkill.ParseIntoArray(Combo, TEXT("+"));
			ZoogiFilm::Begin(FString::Printf(TEXT("skill_%s"), *OnlySkill.Replace(TEXT("+"), TEXT("_"))), Combo.Num() > 1 ? 8.5f : 6.5f);
			if (Combo.Num() > 0) AutoCast(G, FName(*Combo[0]));
			for (int32 c = 1; c < Combo.Num(); ++c) { const FName Nx(*Combo[c]); TWeakObjectPtr<AZoogiGameMode> WGc = G; Later(G, 1.8f * c, [WGc, Nx]() { if (AZoogiGameMode* Gc = WGc.Get()) { ZoogiFilm::Mark(Nx.ToString()); AutoCast(Gc, Nx); } }); }'''),
 ('#include "ZoogiKFXExtras.h"\n', '#include "ZoogiKFXExtras.h"\n#include "ZoogiFilm.h"\n'),
])
ed(P + 'ZoogiDuel.cpp', [
 ('''	StartClash(G, Att, Def);
	if (!Clash.bOn) return;
	Clash.bDuel = true;''', '''	StartClash(G, Att, Def);
	if (!Clash.bOn) return;
	if (ZoogiFilm::On() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmDuels"))) ZoogiFilm::Begin(TEXT("duel"), 16.f); // b101
	Clash.bDuel = true;'''),
])
s=open(os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiDuel.cpp'),encoding='utf-8-sig').read()
first=[l for l in s.splitlines() if l.startswith('#include')][0]
ed(P + 'ZoogiDuel.cpp', [(first+'\n', first+'\n#include "ZoogiFilm.h"\n')])
ed(P + 'ZoogiWolfPack.cpp', [
 ('''	if (Phase == EPhase::Paint)
	{
		// CPU: picks its targets one by one during the short howl''', '''	if (Phase == EPhase::Paint)
	{
		// b101 film: the human's pick taps its targets one by one (rivals first, then the nearest orbs)
		static const bool bFilmTaps = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmWolfTaps"));
		if (bFilmTaps && bHumanOwner && Now - PhaseT0 > 1.8f + 1.1f * Painted.Num() && Painted.Num() < PaintNeed())
		{
			TArray<AActor*> Ts = Targets(Caster.Get());
			const FVector From = Caster.IsValid() ? Caster->BallLocation() : FVector::ZeroVector;
			Ts.Sort([&From](const AActor& A, const AActor& Bq) { const bool Ab = Cast<AZoogiBall>(&A) != nullptr, Bb = Cast<AZoogiBall>(&Bq) != nullptr; if (Ab != Bb) return Ab; return FVector::DistSquared2D(WolfPos(&A), From) < FVector::DistSquared2D(WolfPos(&Bq), From); });
			for (AActor* Tt : Ts) if (!Painted.Contains(Tt)) { bool bOk = false; const FVector2D Sp = Project(WolfPos(Tt), bOk); if (bOk) { PaintPick(Sp, PaintPx * 2.f); break; } }
		}
		// CPU: picks its targets one by one during the short howl'''),
 ('''	Howl(SkinPitch, 1.1f);
	SetDil(0.2f);''', '''	Howl(SkinPitch, 1.1f);
	if (bHumanOwner) ZoogiFilm::Begin(TEXT("wolves"), 16.f); // b101 (only with -ZoogiFilm)
	SetDil(0.2f);'''),
 ('#include "ZoogiWolfPack.h"\n', '#include "ZoogiWolfPack.h"\n#include "ZoogiFilm.h"\n'),
])
# P1 rolls as the filmed Zoogi
ed(P + 'ZoogiFront.cpp', [
 ('''		if (!B->bCPU)
		{
			const UZoogiFrontProfileSubsystem* Fp = UZoogiFrontProfileSubsystem::Get(Ctx);''', '''		if (!B->bCPU)
		{
			FString FilmBall; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiAwakenBall="), FilmBall)) if (const FAwaken* Fb = AwakenForBall(FilmBall)) return Fb; // b101 film
			const UZoogiFrontProfileSubsystem* Fp = UZoogiFrontProfileSubsystem::Get(Ctx);'''),
])
ed(P + 'ZoogiBall.cpp', [
 ('''		if (!OverrideTN.IsEmpty()) TN = *OverrideTN;''', '''		{ FString FilmBall; if (!bInCPU && FParse::Value(FCommandLine::Get(), TEXT("ZoogiAwakenBall="), FilmBall)) OverrideTN = TEXT("T_Ball_") + FilmBall; } // b101 film
		if (!OverrideTN.IsEmpty()) TN = *OverrideTN;'''),
])
print('b101a ok')
