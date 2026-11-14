// b122 SHOWTIME sequence - see ZoogiShowtime.h
#include "ZoogiShowtime.h"
#include "ZoogiAwakenFX.h" // b266 Awaken cinematic
#include "ZoogiRaid.h"
#include "ZoogiFilm.h" // b215
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiHaptics.h"
#include "ZoogiArtifacts.h"
#include "ZoogiActionFun.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiSkills.h"
#include "ZoogiArena.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiFront.h"

namespace
{
	enum class EStage : uint8 { Off, Rise, Pick, Fade };
	EStage GStage = EStage::Off;
	double GT0 = 0.0, GFadeT0 = 0.0, GLastGlow = 0.0;
	TWeakObjectPtr<AZoogiGameMode> GG;
	TWeakObjectPtr<AZoogiBall> GBall;
	int32 GTurn = -1;
	TWeakObjectPtr<AActor> GSealMe, GSealArena;
	TWeakObjectPtr<UMaterialInstanceDynamic> GMidMe, GMidArena;
	TWeakObjectPtr<APostProcessVolume> GPP;
	TArray<TPair<TWeakObjectPtr<ULightComponent>, float>> GLights;
	TArray<TPair<TWeakObjectPtr<USkyLightComponent>, float>> GSkies;
	float GSpin = 0.f;
	// b142: the armed Awaken (SHOWTIME -> SUPERPOWER)
	TWeakObjectPtr<AZoogiGameMode> GArmG; int32 GArmP = -1, GArmTurn = -1; FString GArmKey; bool GArmWatch = false;
	bool GFireTop = true; FString GFireCap; int32 GFireP = -1; // b145a
	TArray<FString> GFireClips; double GFireT0 = -1.0; TWeakObjectPtr<AZoogiGameMode> GFireG; bool GFireTick = false;
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
	}
	bool GTicker = false;
	double Now() { return FPlatformTime::Seconds(); }

	AActor* SpawnSeal(UWorld* W, const FVector& At, float Size, const FLinearColor& Tint, TWeakObjectPtr<UMaterialInstanceDynamic>& OutMid)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Arenas/M_FrozenDecal.M_FrozenDecal"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UTexture* Tex = LoadObject<UTexture>(nullptr, TEXT("/Game/Env/Arenas/T_FrozenSealHalo.T_FrozenSealHalo"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!W || !Base || !Tex) { UE_LOG(LogZoogi, Warning, TEXT("SHOWTIME: Frozen seal art missing")); return nullptr; }
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), SP);
		if (!A) return nullptr;
		UStaticMeshComponent* Q = NewObject<UStaticMeshComponent>(A, TEXT("Seal"));
		Q->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane")));
		Q->SetCollisionEnabled(ECollisionEnabled::NoCollision); Q->SetCastShadow(false); Q->SetReceivesDecals(false);
		Q->ComponentTags.Add(TEXT("NoCamBlock"));
		UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, A);
		D->SetTextureParameterValue(TEXT("Tex"), Tex);
		D->SetVectorParameterValue(TEXT("Tint"), Tint);
		D->SetScalarParameterValue(TEXT("Bright"), 1.2f);
		D->SetScalarParameterValue(TEXT("Opacity"), 0.f);
		D->SetScalarParameterValue(TEXT("Emissive"), 1.6f);
		Q->SetMaterial(0, D);
		A->SetRootComponent(Q); Q->RegisterComponent();
		A->SetActorScale3D(FVector(Size / 100.f, Size / 100.f, 1.f));
		OutMid = D;
		return A;
	}
	void DimLights(UWorld* W, bool bOn)
	{
		if (bOn)
		{
			GLights.Reset();
			GSkies.Reset();
			for (TActorIterator<AActor> It(W); It; ++It)
			{	// b244: light COMPONENTS on any actor (the arenas build their lights as components)
				TInlineComponentArray<UDirectionalLightComponent*> Dl(*It); for (UDirectionalLightComponent* L : Dl) if (L) GLights.Add({ L, L->Intensity });
				TInlineComponentArray<USkyLightComponent*> Sl(*It); for (USkyLightComponent* L : Sl) if (L) GSkies.Add({ L, L->Intensity });
			}
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (APostProcessVolume* P = W->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SP))
			{
				P->bUnbound = true; P->Priority = 50.f; P->BlendWeight = 0.f;
				P->Settings.bOverride_AutoExposureBias = true; P->Settings.AutoExposureBias = -1.5f;
				P->Settings.bOverride_SceneColorTint = true; P->Settings.SceneColorTint = FLinearColor(0.42f, 0.42f, 0.55f, 1.f); // b244: the dim shows even without auto exposure
				GPP = P;
			}
		}
	}
	void SetDim(float K) // 0 = normal, 1 = full showtime dim
	{
		for (auto& Pr : GLights) if (ULightComponent* L = Pr.Key.Get()) L->SetIntensity(Pr.Value * FMath::Lerp(1.f, 0.3f, K));
		for (auto& Pr : GSkies) if (USkyLightComponent* L = Pr.Key.Get()) L->SetIntensity(Pr.Value * FMath::Lerp(1.f, 0.4f, K));
		if (APostProcessVolume* P = GPP.Get()) P->BlendWeight = K;
	}
	void EndAll()
	{
		SetDim(0.f);
		if (AActor* A = GSealMe.Get()) A->Destroy();
		if (AActor* A = GSealArena.Get()) A->Destroy();
		if (APostProcessVolume* P = GPP.Get()) P->Destroy();
		GLights.Reset(); GSkies.Reset(); GStage = EStage::Off;
		GClips.Reset(); GSkyCam = false; EndSkyCam(0.2f);
	}
	AActor* Nearest(AZoogiGameMode* G, AZoogiBall* B)
	{
		TArray<AActor*> T = G->PublicLiveTargets(B);
		return T.Num() ? T[0] : nullptr;
	}
	void Lock(AZoogiGameMode* G, AActor* T, const TCHAR* Why)
	{
		if (!G || !T) return;
		G->LockOn(T, false);
		AZoogiFX::Spawn(G->GetWorld(), T->GetActorLocation(), FLinearColor(1.f, 0.85f, 0.3f), 220.f, 0.5f, true, 10.f, T);
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("SHOWTIME target locked (%s): %s"), Why, *T->GetName());
	}

	bool Tick(float Dt)
	{
		if (GStage == EStage::Off) return true;
		AZoogiGameMode* G = GG.Get(); AZoogiBall* B = GBall.Get();
		if (!G || !B || B->bOut || G->TurnCounter != GTurn || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver) { EndAll(); return true; }
		const double T = Now() - GT0;
		GSpin = FMath::Fmod(GSpin + Dt * 30.f, 360.f);
		const FVector Floor = B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - 1.5f);
		if (AActor* S = GSealMe.Get()) S->SetActorLocationAndRotation(Floor, FRotator(0.f, GSpin, 0.f));
		if (AActor* S = GSealArena.Get()) S->SetActorRotation(FRotator(0.f, -GSpin * 0.25f, 0.f));
		float Glow = 1.f, Dim = 1.f;
		if (GStage == EStage::Rise)
		{
			// b133: the SHOWTIME cutscene plays first (full screen) - the seals / dim / pick wait for it
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
			}
			Glow = Dim = FMath::Clamp((float)T / 0.5f, 0.f, 1.f);
			if (T >= 1.2)
			{
				if (G->PublicLiveTargets(B).Num() == 0 || G->Phase != EZoogiPhase::Aiming) { GStage = EStage::Fade; GFadeT0 = Now(); }
				else
				{
					GStage = EStage::Pick;
					if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("AWAKEN!"), TEXT("TAP YOUR TARGET"), FLinearColor(1.f, 0.85f, 0.3f), 2.f, 1.1f);
				}
			}
		}
		else if (GStage == EStage::Pick)
		{
			if (Now() - GLastGlow > 0.45)
			{	// the targets glow while you choose
				GLastGlow = Now();
				static const bool bOrbGlow = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeOrbGlow"));
				for (AActor* Tt : G->PublicLiveTargets(B)) if (bOrbGlow || !Cast<AZoogiOrb>(Tt)) AZoogiFX::Spawn(G->GetWorld(), Tt->GetActorLocation(), FLinearColor(1.f, 0.8f, 0.3f), 120.f, 0.4f, true, 6.f, Tt); // b128: no pulse on orbs
			}
			static const bool bStTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest"));
			if (G->Phase != EZoogiPhase::Aiming) { GStage = EStage::Fade; GFadeT0 = Now(); }
			else if (T > (bStTest ? 2.7 : 13.2)) { Lock(G, Nearest(G, B), bStTest ? TEXT("test pick") : TEXT("time up - nearest")); GStage = EStage::Fade; GFadeT0 = Now(); }
		}
		else if (GStage == EStage::Fade)
		{
			const float K = 1.f - FMath::Clamp((float)(Now() - GFadeT0) / 0.8f, 0.f, 1.f);
			Glow = Dim = K;
			if (K <= 0.f) { EndAll(); return true; }
		}
		const float Pulse = 0.85f + 0.15f * FMath::Sin((float)T * 6.f);
		if (UMaterialInstanceDynamic* M = GMidMe.Get()) M->SetScalarParameterValue(TEXT("Opacity"), Glow * Pulse);
		if (UMaterialInstanceDynamic* M = GMidArena.Get()) M->SetScalarParameterValue(TEXT("Opacity"), Glow * 0.55f);
		SetDim(Dim);
		return true;
	}
}

namespace { bool GDarkOn = false; TWeakObjectPtr<UWorld> GDarkW; }
void ZoogiShowtime::DarkTick(AZoogiGameMode* G)
{	// b243: DARK ARENA - the showtime dim, permanent (SHOWTIME itself takes the lights while it runs)
	UWorld* W = G ? G->GetWorld() : nullptr;
	if (!W || GStage != EStage::Off) return;
	const bool bWant = ZoogiControls::DarkArena() && G->Phase != EZoogiPhase::Menu;
	if (bWant)
	{
		static double NextTry = 0.0; const double Tn = FPlatformTime::Seconds();
		if (!GDarkOn || GDarkW.Get() != W || !GPP.IsValid() || (GLights.Num() == 0 && Tn > NextTry)) { if (GDarkOn) { SetDim(0.f); if (APostProcessVolume* Po = GPP.Get()) Po->Destroy(); } NextTry = Tn + 2.0; DimLights(W, true); GDarkW = W; GDarkOn = true; UE_LOG(LogZoogi, Log, TEXT("DARK ARENA on (%d lights)"), GLights.Num()); }
		SetDim(1.f);
	}
	else if (GDarkOn) { EndAll(); GDarkOn = false; UE_LOG(LogZoogi, Log, TEXT("DARK ARENA off")); }
}

void ZoogiShowtime::Begin(AZoogiGameMode* G, AZoogiBall* B)
{
	if (GDarkOn) { SetDim(0.f); if (APostProcessVolume* P = GPP.Get()) P->Destroy(); GLights.Reset(); GSkies.Reset(); GDarkOn = false; } // b243: hand the lights back first (no double dim)
	if (!G || !B || B->bCPU || FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldShowtime"))) return;
	UWorld* W = G->GetWorld();
	if (GStage != EStage::Off) EndAll();
	if (!GTicker) { GTicker = true; FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.f); }
	GG = G; GBall = B; GTurn = G->TurnCounter; GT0 = Now(); GStage = EStage::Rise; GSpin = 0.f;
	ZoogiFilm::Moment(TEXT("Awaken")); // b215: -ZoogiFilmAwaken=<s> films the whole Awaken (trailer footage)
	ZoogiHaptics::Rumble(2.f, 1.f, W, TEXT("showtime")); // 2 s
	// b133 (Evan: "an epic anime cutscene when SHOWTIME is pressed - first is the meteor strike")
	GClips.Reset(); GSkyCam = false; // b134
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
				static const bool bAutoFire = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest"));
				if (bAutoFire && Ga->Phase == EZoogiPhase::Rolling && Ga->PhaseTime > 0.8f && Ga->TurnCounter == GArmTurn) { UE_LOG(LogZoogi, Log, TEXT("SHOWTIME TEST: SUPERPOWER press")); Ga->TryPower(); Ga->TakeShotDelayed(TEXT("awaken_fire"), 0.9f); Ga->TakeShotDelayed(TEXT("awaken_anime"), 2.6f); return true; }
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
	{
		const EZoogiPower Pw = B->Def.PowerKind;
		const FString Key = Pw == EZoogiPower::Explosion ? FString(TEXT("MeteorStrike")) : Pw == EZoogiPower::WolfPack ? FString(TEXT("WolfPack")) : Pw == EZoogiPower::StaticShock ? FString(TEXT("BoltBalls"))
			: StaticEnum<EZoogiPower>()->GetNameStringByValue((int64)Pw);
		FString Clip = FString::Printf(TEXT("Anime_Showtime_%s_01"), *Key);
		FString Forced; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiShowtimeCut="), Forced)) Clip = Forced; // test: any clip
		if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Clip + TEXT(".mp4"))))
			if (UZoogiAnimeSubsystem* An = W ? W->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr)
				if (An->PlayCrash(Clip, FLinearColor(1.f, 0.6f, 0.25f)))
				{
					UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: cutscene %s (full screen)"), B->PlayerIndex + 1, *Clip);
					// b134: the meteor chain - the big final meteor clip (when it exists), then the in-game sky camera + meteor strike
					if (Key == TEXT("MeteorStrike"))
					{
						const FString Clip2 = FString::Printf(TEXT("Anime_Showtime_%s_02"), *Key);
						if (FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies"), Clip2 + TEXT(".mp4")))) GClips.Add(Clip2);
						GSkyCam = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoShowtimeMeteorCam"));
					}
				}
	}
	const FLinearColor Tint = FMath::Lerp(ZoogiArtifacts::Color(ZoogiArtifacts::Affinity(B->Def.Id)), FLinearColor(0.75f, 0.95f, 1.f), 0.35f);
	const FVector Floor = B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - 1.5f);
	GSealMe = SpawnSeal(W, Floor, 900.f, Tint, GMidMe);
	GSealArena = SpawnSeal(W, FVector(0.f, 0.f, Floor.Z - 0.5f), 2.f * ZoogiConst::KnockoffRadius, Tint, GMidArena);
	DimLights(W, true);
	if (AZoogiFabFX* Ef = AZoogiFabFX::Play(W, TEXT("/Game/StylizedToonVFX3/Effects/NS_Energy_buff.NS_Energy_buff"), Floor, 0.18f, 5.f, B, FVector(0, 0, -ZoogiConst::ZoogiRadius + 2.f))) Ef->StopAfter = 1.2f;
	AZoogiFabFX::Sound(W, TEXT("/Game/StylizedToonVFX3/Audio/Cues/S_Energy_buff_Cue.S_Energy_buff_Cue"), B->BallLocation(), 1.f);
	UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: rumble 2 s, rune seals, lights dim (%d), energy buff, then the target pick"), B->PlayerIndex + 1, GLights.Num());
}

bool ZoogiShowtime::Picking() { return GStage == EStage::Pick; }

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
	// b266: SetAwakenStrength moved into the cinematic's landing (right before the cast)
	const ZoogiFront::FAwaken* Aw = ZoogiFront::AwakenFor(B, G);
	if (Aw) GArmKey = (!Aw->Skill || !*Aw->Skill) ? FString(TEXT("MeteorStrike")) : FCString::Strcmp(Aw->Skill, TEXT("wolf_pack")) == 0 ? FString(TEXT("WolfPack")) : FCString::Strcmp(Aw->Skill, TEXT("bolt_balls")) == 0 ? FString(TEXT("BoltBalls")) : FString(Aw->Skill);
	// b266 (Evan: "check awaken cinematic"): sky vortex -> meteor touches down on the Zoogi -> THEN its Awaken fires -> explosions
	const bool bRain = GArmKey == TEXT("MeteorStrike") || !Aw || !Aw->Skill || !*Aw->Skill;
	const FName AwSkill = (!bRain && Aw) ? FName(Aw->Skill) : NAME_None;
	TWeakObjectPtr<AZoogiGameMode> WG(G); TWeakObjectPtr<AZoogiBall> WB(B);
	AZoogiAwakenFX::PlayCinematic(W, B, [WG, WB, bRain, AwSkill, Ctr, KR]()
	{
		AZoogiGameMode* Gx = WG.Get(); AZoogiBall* Bx = WB.Get();
		if (!Gx || !Bx) return;
		ZoogiSkills::SetAwakenStrength();
		if (bRain)
		{	// Hotstreak's METEOR STRIKE: the meteor rain over the arena, full strength
			ZoogiSkills::MeteorRain(Gx, Ctr, KR * 0.75f, Bx->PlayerIndex, 10, 1.2f);
			UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: SUPERPOWER - Awaken METEOR RAIN (10 meteors, full strength)"), Bx->PlayerIndex + 1);
		}
		else
		{
			ZoogiSkills::AwakenCast(Gx, Bx, AwSkill);
			UE_LOG(LogZoogi, Log, TEXT("SHOWTIME P%d: SUPERPOWER - Awaken %s"), Bx->PlayerIndex + 1, *AwSkill.ToString());
		}
		ZoogiHaptics::Rumble(1.5f, 0.9f, Gx->GetWorld(), TEXT("awaken_fire"));
		ZoogiUtil::PlaySfx(Gx, TEXT("CritBoom"), 0.9f, 0.7f);
	});
	ZoogiRaid::OnAwaken(G, B); // b151: SHOWTIME hits the raid titan for 20%
	if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(Aw && Aw->Name ? FString(Aw->Name) : FString(TEXT("METEOR STRIKE")), TEXT("AWAKEN!"), FLinearColor(1.f, 0.5f, 0.15f), 1.8f, 1.2f);
	// then the full screen anime, once the effect has landed
	GFireClips.Reset();
	static const bool bFullScr = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeFull")); // b145a: the b143 full-screen _01 + _02
	GFireTop = !bFullScr; GFireCap = Aw && Aw->Name ? FString(Aw->Name) : FString(TEXT("METEOR STRIKE")); GFireP = B->PlayerIndex;
	for (int32 k = 1; k <= (bFullScr ? 2 : 1); ++k) // b145a (Evan): "remove the second anime clip for the hotstreak awaken"
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
			if (GFireTop) { if (An->PlayTopCard(Next, GFireCap, GFireP, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: Awaken anime %s (top card)"), *Next); }
			else if (An->PlayCrash(Next, FLinearColor(1.f, 0.6f, 0.25f))) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: Awaken anime %s (full screen)"), *Next);
			return true;
		}));
	}
	if (GFireClips.Num() == 0) UE_LOG(LogZoogi, Log, TEXT("SHOWTIME: no Anime_Showtime_%s clip - effect only"), *GArmKey);
	return true;
}

void ZoogiShowtime::Tap(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Screen)
{
	AZoogiBall* B = GBall.Get();
	if (GStage != EStage::Pick || !G || !PC || !B) return;
	AActor* Best = nullptr; float BestD = 1e9f;
	for (AActor* T : G->PublicLiveTargets(B))
	{
		FVector2D S;
		if (!PC->ProjectWorldLocationToScreen(T->GetActorLocation(), S, false)) continue;
		const float D = FVector2D::Distance(S, Screen);
		if (D < BestD) { BestD = D; Best = T; }
	}
	int32 Vx = 0, Vy = 0; PC->GetViewportSize(Vx, Vy);
	if (!Best || BestD > 0.2f * FMath::Max(1, FMath::Min(Vx, Vy)) + 80.f) return;
	Lock(G, Best, TEXT("tap"));
	GStage = EStage::Fade; GFadeT0 = Now();
}
