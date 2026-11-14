# b134a (Evan 07:17: "that anime works, but add one clip of one big final meteor coming, then scene cut to actual gameplay starting with
# the cam panning toward the sky to watch the meteor strike the arena as it does now"):
# SHOWTIME of a Meteor Strike Zoogi = Anime_Showtime_MeteorStrike_01 (10 s) -> _02 (the big final meteor, when Grok's clip is in) ->
# hard cut to the game: a sky camera at the arena edge pans up into the sky, the in-game meteor strike (MeteorRain over the arena, as the
# ultimate does it) falls, the camera follows it down onto the court, then blends back -> rune seals / target pick as before.
# -ZoogiNoShowtimeMeteorCam = no sky camera / meteors.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '#include "ZoogiAnimeSubsystem.h"\n', '#include "ZoogiAnimeSubsystem.h"\n#include "ZoogiSkills.h"\n#include "ZoogiArena.h"\n#include "Camera/CameraActor.h"\n#include "Camera/CameraComponent.h"\n#include "Camera/PlayerCameraManager.h"\n')
ed1(S, '''	float GSpin = 0.f;''', '''	float GSpin = 0.f;
	// b134: the SHOWTIME cutscene chain (clip, clip, then the in-game sky camera for the meteor strike)
	TArray<FString> GClips;
	bool GSkyCam = false; double GSkyT0 = -1.0; bool GSkyRained = false;
	TWeakObjectPtr<ACameraActor> GSkyCamActor; TWeakObjectPtr<AActor> GSkyPrev;
	FVector GSkyPos = FVector::ZeroVector, GSkyCtr = FVector::ZeroVector; float GSkyYaw = 0.f;
	void EndSkyCam(float Blend)
	{
		UWorld* W = GG.IsValid() ? GG->GetWorld() : nullptr;
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (PC && GSkyPrev.IsValid() && GSkyCamActor.IsValid() && PC->GetViewTarget() == GSkyCamActor.Get()) PC->SetViewTargetWithBlend(GSkyPrev.Get(), Blend, VTBlend_Cubic);
		if (ACameraActor* C = GSkyCamActor.Get()) C->SetLifeSpan(Blend + 0.5f);
		GSkyCamActor.Reset(); GSkyPrev.Reset(); GSkyT0 = -1.0;
	}''')
ed1(S, '''		GLights.Reset(); GSkies.Reset(); GStage = EStage::Off;
	}''', '''		GLights.Reset(); GSkies.Reset(); GStage = EStage::Off;
		GClips.Reset(); GSkyCam = false; EndSkyCam(0.2f);
	}''')
ed1(S, '''			// b133: the SHOWTIME cutscene plays first (full screen) - the seals / dim / pick wait for it
			if (UZoogiAnimeSubsystem* An = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr) if (An->CrashOn()) { GT0 = Now(); SetDim(0.f); return true; }''',
'''			// b133: the SHOWTIME cutscene plays first (full screen) - the seals / dim / pick wait for it
			if (UZoogiAnimeSubsystem* An = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr)
			{
				if (An->CrashOn()) { GT0 = Now(); SetDim(0.f); return true; }
				if (GClips.Num())
				{	// b134: the next clip of the chain (the big final meteor)
					const FString Next = GClips[0]; GClips.RemoveAt(0);
					if (An->PlayCrash(Next, FLinearColor(1.f, 0.6f, 0.25f))) { UE_LOG(LogZoogi, Log, TEXT("SHOWTIME cutscene %s"), *Next); GT0 = Now(); SetDim(0.f); return true; }
				}
			}
			if (GSkyCam)
			{	// b134 (Evan: "scene cut to actual gameplay, the cam pans toward the sky to watch the meteor strike the arena"): a hard cut to a
				// camera at the arena edge; it tilts up into the sky (0-1.3 s), the meteors fall (MeteorRain from 0.9 s), it follows them down
				// onto the court (2.4-3.6 s), holds, then blends back to the match camera
				UWorld* Ws = G->GetWorld(); APlayerController* PCs = Ws ? Ws->GetFirstPlayerController() : nullptr;
				if (GSkyT0 < 0.0 && PCs)
				{
					GSkyT0 = Now(); GSkyRained = false;
					GSkyCtr = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
					const float KR = G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f;
					const FVector CamNow = PCs->PlayerCameraManager ? PCs->PlayerCameraManager->GetCameraLocation() : GSkyCtr + FVector(-KR, 0.f, 600.f);
					FVector Back = CamNow - GSkyCtr; Back.Z = 0.f; Back = Back.GetSafeNormal(); if (Back.IsNearlyZero()) Back = FVector(-1.f, 0.f, 0.f);
					GSkyPos = GSkyCtr + Back * KR * 1.05f + FVector(0.f, 0.f, 220.f);
					GSkyYaw = (GSkyCtr - GSkyPos).Rotation().Yaw;
					FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (ACameraActor* Cam = Ws->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator(-12.f, GSkyYaw, 0.f), GSkyPos), SP))
					{
						Cam->GetCameraComponent()->SetFieldOfView(80.f);
						Cam->GetCameraComponent()->bConstrainAspectRatio = false;
						GSkyCamActor = Cam; GSkyPrev = PCs->GetViewTarget();
						PCs->SetViewTargetWithBlend(Cam, 0.f); // the scene cut
					}
					UE_LOG(LogZoogi, Log, TEXT("SHOWTIME sky camera: pan up, meteor strike, follow it down"));
				}
				const float Ts = (float)(Now() - GSkyT0);
				auto Ease = [](float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); };
				const float Pitch = Ts < 1.3f ? FMath::Lerp(-12.f, 42.f, Ease(Ts / 1.3f)) : (Ts < 2.4f ? 42.f : FMath::Lerp(42.f, -32.f, Ease((Ts - 2.4f) / 1.2f)));
				if (ACameraActor* Cam = GSkyCamActor.Get()) Cam->SetActorLocationAndRotation(GSkyPos + FVector(0.f, 0.f, 120.f * Ease(Ts / 3.6f)), FRotator(Pitch, GSkyYaw, 0.f));
				if (!GSkyRained && Ts >= 0.9f)
				{
					GSkyRained = true;
					ZoogiSkills::SetAwakenStrength();
					const float KR2 = G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f;
					ZoogiSkills::MeteorRain(G, GSkyCtr, KR2 * 0.75f, B->PlayerIndex, 8, 1.2f); // the ultimate's meteor strike, as it plays now
					ZoogiHaptics::Rumble(1.5f, 0.8f, Ws, TEXT("showtime_meteors"));
				}
				if (Ts >= 4.6f) { GSkyCam = false; EndSkyCam(0.6f); GT0 = Now(); }
				GT0 = Now(); SetDim(0.f);
				return true;
			}''')
ed1(S, '''				if (An->PlayCrash(Clip, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: cutscene %s (full screen)"), B->PlayerIndex + 1, *Clip);''',
'''				if (An->PlayCrash(Clip, FLinearColor(1.f, 0.6f, 0.25f)))
				{
					UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: cutscene %s (full screen)"), B->PlayerIndex + 1, *Clip);
					// b134: the meteor chain - the big final meteor clip (when it exists), then the in-game sky camera + meteor strike
					if (Key == TEXT("MeteorStrike"))
					{
						const FString Clip2 = FString::Printf(TEXT("Anime_Showtime_%s_02"), *Key);
						if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Clip2 + TEXT(".mp4")))) GClips.Add(Clip2);
						GSkyCam = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeMeteorCam"));
					}
				}''')
print('b134a ok')
ed1(S, '''	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeCut")))
	{
		const EZoogiPower Pw''', '''	GClips.Reset(); GSkyCam = false; // b134
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeCut")))
	{
		const EZoogiPower Pw''')
print('b134a part 2 ok')
