// b163 (Evan Nov 6 13:32 "Finish the sound scape"): see ZoogiSoundscape.h. Presentation only.
#include "ZoogiSoundscape.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiMusicSubsystem.h"
#include "ZoogiAnimeSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiSoundscape
{
	namespace
	{
		struct FLoop { TWeakObjectPtr<UAudioComponent> AC; FName Name; float Vol = 0.f; };
		FLoop GBed, GCrowd, GDrum;
		TWeakObjectPtr<UWorld> GWorld;
		float GExcite = 0.f, GNextAccent = 4.f, GFinalT = 0.f;
		// miss groan bookkeeping
		bool GShotLive = false; int32 GShotPlayer = -1, GShotOrbs = 0, GShotOut = 0;
		EZoogiPhase GPrevPhase = EZoogiPhase::Menu;

		bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSoundscape")); return b; }
		USoundBase* Snd(const FString& Name)
		{
			static TMap<FString, TStrongObjectPtr<USoundBase>> Cache;
			if (TStrongObjectPtr<USoundBase>* F = Cache.Find(Name)) return F->Get();
			const FString P = FString::Printf(TEXT("/Game/Audio/Sounds/Amb/SW_%s.SW_%s"), *Name, *Name);
			USoundBase* S = LoadObject<USoundBase>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!S) UE_LOG(LogZoogi, Warning, TEXT("SOUNDSCAPE: %s missing"), *P);
			Cache.Add(Name, TStrongObjectPtr<USoundBase>(S));
			return S;
		}
		void One(UWorld* W, const FString& Name, float Vol, float Pitch = 1.f)
		{
			const float Sfx = ZoogiUtil::GetSfxVolume();
			if (!W || Sfx <= 0.001f) return;
			if (USoundBase* S = Snd(Name)) UGameplayStatics::PlaySound2D(W, S, Vol * Sfx, Pitch, 0.f, nullptr, nullptr, true);
		}
		void Loop(FLoop& L, UWorld* W, FName Name, float Target, float Dt, float Rate)
		{
			UAudioComponent* AC = L.AC.Get();
			if (Name != L.Name || (!AC && !Name.IsNone()))
			{
				if (AC) AC->FadeOut(1.8f, 0.f);
				L.AC = nullptr; L.Name = Name; L.Vol = 0.f; AC = nullptr;
				if (!Name.IsNone())
					if (USoundBase* S = Snd(Name.ToString()))
						if ((AC = UGameplayStatics::CreateSound2D(W, S, 0.001f, 1.f, 0.f, nullptr, false, false)) != nullptr)
						{
							AC->bIsUISound = false;
							AC->Play(FMath::FRandRange(0.f, 20.f)); // a different spot of the loop each match
							L.AC = AC;
						}
			}
			if (!AC) return;
			L.Vol += (Target - L.Vol) * FMath::Min(1.f, Dt * Rate);
			AC->SetVolumeMultiplier(FMath::Max(0.001f, L.Vol * ZoogiUtil::GetSfxVolume()));
		}
		FName BedFor(EZoogiArena A)
		{
			switch (A)
			{
			case EZoogiArena::Meadow: case EZoogiArena::Teaser: return TEXT("AmbMeadow_Loop");
			case EZoogiArena::Frozen: return TEXT("AmbFrozen_Loop");
			case EZoogiArena::Volcanic: return TEXT("AmbVolcano_Loop");
			case EZoogiArena::Cosmic: return TEXT("AmbCosmos_Loop");
			case EZoogiArena::Arabian: return TEXT("AmbArabian_Loop");
			case EZoogiArena::Tomb: return TEXT("AmbTomb_Loop");
			case EZoogiArena::Neon: return TEXT("AmbNeon_Loop");
			case EZoogiArena::Training: return TEXT("AmbCourt_Loop");
			case EZoogiArena::Ancient: return TEXT("AmbAncient_Loop");
			default: return NAME_None;
			}
		}
		// world accents: a random one every few seconds
		void Accent(UWorld* W, EZoogiArena A)
		{
			auto Pick = [](const TCHAR* Stem, int32 N) { return FString::Printf(TEXT("%s_%02d"), Stem, FMath::RandRange(1, N)); };
			switch (A)
			{
			case EZoogiArena::Meadow: case EZoogiArena::Teaser: case EZoogiArena::Training: One(W, Pick(TEXT("AmbBird"), 4), FMath::FRandRange(0.18f, 0.32f), FMath::FRandRange(0.9f, 1.15f)); break;
			case EZoogiArena::Frozen: One(W, Pick(TEXT("AmbIceCreak"), 3), FMath::FRandRange(0.25f, 0.4f), FMath::FRandRange(0.85f, 1.1f)); break;
			case EZoogiArena::Volcanic: One(W, Pick(TEXT("AmbLavaPop"), 3), FMath::FRandRange(0.25f, 0.4f), FMath::FRandRange(0.85f, 1.15f)); break;
			case EZoogiArena::Cosmic: One(W, Pick(TEXT("AmbSpaceBlip"), 2), FMath::FRandRange(0.15f, 0.25f), FMath::FRandRange(0.8f, 1.2f)); break;
			case EZoogiArena::Arabian: One(W, Pick(TEXT("AmbChime"), 3), FMath::FRandRange(0.15f, 0.25f), FMath::FRandRange(0.95f, 1.05f)); break;
			case EZoogiArena::Tomb: One(W, Pick(TEXT("AmbDrip"), 3), FMath::FRandRange(0.25f, 0.4f), FMath::FRandRange(0.85f, 1.2f)); break;
			case EZoogiArena::Neon: One(W, FMath::RandBool() ? Pick(TEXT("AmbCarPass"), 2) : Pick(TEXT("AmbZap"), 2), FMath::FRandRange(0.15f, 0.28f), FMath::FRandRange(0.9f, 1.1f)); break;
			case EZoogiArena::Ancient: One(W, FMath::FRand() < 0.6f ? Pick(TEXT("AmbBird"), 4) : Pick(TEXT("AmbLavaPop"), 3), FMath::FRandRange(0.18f, 0.3f), FMath::FRandRange(0.9f, 1.1f)); break;
			default: break;
			}
		}
		bool HasCrowd(EZoogiArena A) { return A != EZoogiArena::Tomb && A != EZoogiArena::Cosmic; }
		int32 OutCount(AZoogiGameMode* G, int32 Except) { int32 N = 0; for (int32 i = 0; i < G->Balls.Num(); ++i) if (i != Except && G->Balls[i] && G->Balls[i]->bOut) ++N; return N; }
	}

	void Excite(float Amount) { GExcite = FMath::Clamp(GExcite + Amount, 0.f, 1.f); }

	void OnKnockOff(AActor* Ball)
	{
		if (Off() || !Ball) return;
		UWorld* W = Ball->GetWorld(); if (!W) return;
		One(W, FString::Printf(TEXT("FallWhoosh_%02d"), FMath::RandRange(1, 2)), 0.5f, FMath::FRandRange(0.92f, 1.08f));
		TWeakObjectPtr<UWorld> WW = W;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WW](float) { if (UWorld* X = WW.Get()) One(X, FString::Printf(TEXT("UnderBlast_%02d"), FMath::RandRange(1, 2)), 0.45f, FMath::FRandRange(0.9f, 1.05f)); return false; }), 0.95f);
		Excite(0.45f);
	}

	void Tick(AZoogiGameMode* G, float Dt)
	{
		if (Off() || !G || !G->Arena) return;
		UWorld* W = G->GetWorld(); if (!W) return;
		if (GWorld.Get() != W) { GWorld = W; GBed = FLoop(); GCrowd = FLoop(); GDrum = FLoop(); GExcite = 0.f; GFinalT = 0.f; GShotLive = false; GPrevPhase = G->Phase; GNextAccent = 4.f; UE_LOG(LogZoogi, Log, TEXT("SOUNDSCAPE: %s"), *ZoogiUtil::ArenaName(G->Arena->GetTheme())); }
		const EZoogiArena A = G->Arena->GetTheme();
		const bool bMenu = G->Phase == EZoogiPhase::Menu, bOver = G->Phase == EZoogiPhase::GameOver;
		const UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>();
		const float Duck = (An && An->IsBusy()) ? 0.3f : 1.f;
		// 1) the map's ambience
		Loop(GBed, W, BedFor(A), (bMenu ? 0.22f : 0.34f) * Duck, Dt, 0.8f);
		// 2) the crowd: quiet early, loud in the closing round, lifts on knockouts
		GExcite = FMath::Max(0.f, GExcite - Dt * 0.18f);
		float Prog = 0.5f;
		if (G->MenuRounds > 1) Prog = FMath::Clamp((G->RoundNumber - 1.f) / (G->MenuRounds - 1.f), 0.f, 1.f);
		const bool bCrowd = HasCrowd(A) && !bMenu;
		const float CrowdT = bCrowd ? (0.07f + 0.17f * Prog + 0.30f * GExcite) * Duck * (A == EZoogiArena::Training ? 0.5f : 1.f) * (bOver ? 0.6f : 1.f) : 0.f;
		Loop(GCrowd, W, bCrowd ? FName(TEXT("AmbCrowd_Loop")) : NAME_None, CrowdT, Dt, 1.5f);
		// 3) the drumroll in the final moments (rises the longer it lasts)
		const bool bFinal = !bMenu && !bOver && G->MenuRounds > 1 && G->RoundNumber >= G->MenuRounds; // the closing round
		GFinalT = bFinal ? GFinalT + Dt : 0.f;
		Loop(GDrum, W, bFinal ? FName(TEXT("AmbDrumroll_Loop")) : NAME_None, bFinal ? FMath::Clamp(0.05f + GFinalT * 0.004f, 0.05f, 0.2f) * Duck : 0.f, Dt, 0.6f);
		// 4) world accents
		GNextAccent -= Dt;
		if (GNextAccent <= 0.f) { GNextAccent = FMath::FRandRange(5.f, 12.f); if (Duck > 0.5f) Accent(W, A); }
		// 5) a groan when the human's shot scores nothing
		if (!bMenu && !G->IsRoyaleMatch())
		{
			if (G->Phase == EZoogiPhase::Rolling && GPrevPhase != EZoogiPhase::Rolling)
			{
				AZoogiBall* S = G->Balls.IsValidIndex(G->ActivePlayer) ? G->Balls[G->ActivePlayer] : nullptr;
				GShotLive = S && !S->bCPU; GShotPlayer = G->ActivePlayer; GShotOrbs = G->OrbsLeft(); GShotOut = OutCount(G, G->ActivePlayer);
			}
			else if (GShotLive && (G->Phase == EZoogiPhase::Aiming || bOver) && GPrevPhase != G->Phase)
			{
				GShotLive = false;
				AZoogiBall* S = G->Balls.IsValidIndex(GShotPlayer) ? G->Balls[GShotPlayer] : nullptr;
				const bool bNothing = G->OrbsLeft() == GShotOrbs && OutCount(G, GShotPlayer) == GShotOut && S && !S->bOut;
				if (bNothing && HasCrowd(A) && FMath::FRand() < 0.6f) { One(W, FString::Printf(TEXT("CrowdGroan_%02d"), FMath::RandRange(1, 2)), 0.32f, FMath::FRandRange(0.95f, 1.05f)); UE_LOG(LogZoogi, Log, TEXT("SOUNDSCAPE: miss groan (P%d)"), GShotPlayer + 1); }
			}
		}
		GPrevPhase = G->Phase;
	}
}
