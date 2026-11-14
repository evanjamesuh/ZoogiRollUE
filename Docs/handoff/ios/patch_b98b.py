# b98b (Evan 19:31: "game still doesn't return to normal gameplay after duel clip ... doesn't return until my turn again. Fix"):
# root cause: the duel (and its flung loser) hand the view back to the camera that was live when the duel began (Clash.PrevView) -
# often a temporary cam (react / follow) that is gone or frozen - and the only camera watchdog ran on the HUMAN's aim step.
# Fix: the duel / flung endings go straight back to the match camera, and the watchdog also covers the CPUs' aim steps and the
# 30 s after any duel (unless a real special camera is up: duel, flung, replay, react cam, wolf pick / kill cam, a clip, a skill effect).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiActionFun.h', [
 ('	bool ClashOn() const { return Clash.bOn; }', '	bool ClashOn() const { return Clash.bOn; }\n	bool SpecialCamOn() const { return Clash.bOn || Flung.Actor.IsValid() || Rep.bOn || ReactCam.IsValid(); } // b98b\n	double LastDuelEndAt = 0.0; // b98b: FPlatformTime seconds of the last duel / flung ending'),
])
ed('Source/ZoogiRoll/Public/ZoogiPlayerController.h', [
 ('	float CamBadT = 0.f, ViewBadT = 0.f; int32 CamWatchFixes = 0;', '	float CamBadT = 0.f, ViewBadT = 0.f, PostDuelBadT = 0.f; int32 CamWatchFixes = 0;\npublic:\n	/** b98b: back to the match camera (after a duel / flung loser). */\n	void RestoreMatchView(float Blend) { if (Cam && GetViewTarget() != Cam) SetViewTargetWithBlend(Cam, Blend); }\nprotected:'),
])
ed(P + 'ZoogiStrikeFun.cpp', [
 ('''		if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr) if (AActor* Pv = Clash.PrevView.Get()) PC->SetViewTarget(Pv);
		C->Destroy();''',
  '''		if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		{	// b98b: the match camera, not whatever cam was live when the duel began (it could be gone or frozen)
			if (AZoogiPlayerController* ZPC = Cast<AZoogiPlayerController>(PC)) ZPC->RestoreMatchView(0.f);
			else if (AActor* Pv = Clash.PrevView.Get()) PC->SetViewTarget(Pv);
		}
		C->Destroy();
		LastDuelEndAt = FPlatformTime::Seconds();'''),
 ('''void UZoogiFunSys::EndFlung(bool bFx)
{
	ReleaseFlungCam();''', '''void UZoogiFunSys::EndFlung(bool bFx)
{
	ReleaseFlungCam();
	if (Flung.Actor.IsValid()) LastDuelEndAt = FPlatformTime::Seconds(); // b98b'''),
])
ed(P + 'ZoogiDuelV2.cpp', [('	EndFighters();\n	const FVector At = Clash.Center;', '	EndFighters();\n	LastDuelEndAt = FPlatformTime::Seconds(); // b98b: the camera watchdog checks the match view for the next 30 s\n	const FVector At = Clash.Center;')])
ed(P + 'ZoogiPlayerController.cpp', [
 ('''	else ViewBadT = 0.f;''', '''	else ViewBadT = 0.f;
	{	// b98b (Evan: "doesn't return to normal gameplay after the duel clip until my turn again"): the CPUs' aim steps and the 30 s after
		// a duel get the same watchdog, unless a real special camera is up
		const UZoogiFunSys* FunV = UZoogiFunSys::Get(this);
		const UZoogiWolfPackSys* WpV = UZoogiWolfPackSys::Get(this);
		const bool bSpecialV = (FunV && FunV->SpecialCamOn()) || (WpV && WpV->PackActive()) || bAnimeShot || (Anime && Anime->IsBusy()) || ZoogiSkills::EffectBusy() || ZoogiRedirect::Picking();
		const bool bAfterDuel = FunV && FunV->LastDuelEndAt > 0.0 && FPlatformTime::Seconds() - FunV->LastDuelEndAt < 30.0;
		const bool bCpuAim = G->IsAimStep() && !G->IsHumanAiming();
		if ((bAfterDuel || bCpuAim) && G->Phase == EZoogiPhase::Aiming && !bSpecialV && Cam && GetViewTarget() != Cam)
		{
			PostDuelBadT += Dt;
			if (PostDuelBadT > 0.6f) { UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH view target was %s (cpu aim %d, after duel %d) -> match camera restored"), *GetNameSafe(GetViewTarget()), bCpuAim ? 1 : 0, bAfterDuel ? 1 : 0); SetViewTargetWithBlend(Cam, 0.3f); PostDuelBadT = 0.f; ++CamWatchFixes; }
		}
		else PostDuelBadT = 0.f;
	}'''),
])
print('b98b ok')
