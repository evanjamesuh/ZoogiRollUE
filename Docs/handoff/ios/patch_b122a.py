# b122a (Evan 19:45: "When SHOWTIME is pressed the phone should vibrate for 2 s, the ancient symbols like the frozen arena glow (use the art
# from the Frozen map), the lights get dimmed and the player gets the Energy Buff effect, then they select a target"):
#  ZoogiShowtime (new): on the human's SHOWTIME - 2 s rumble, the Frozen rune seal (T_FrozenSealHalo on M_FrozenDecal) glows under the
#  Zoogi (spinning, the Awaken element tint) + a faint seal over the whole arena, the lights dim (directional / sky x0.3 + exposure -1.5),
#  the Energy Buff effect plays on the Zoogi; after 1.2 s "TAP YOUR TARGET" - the tap locks the shot's target (12 s, else the nearest);
#  then the lights come back and the seals fade. -ZoogiOldShowtime = the plain splash only.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
open(R + 'Public/ZoogiShowtime.h', 'w', newline='\r\n').write('''#pragma once
// b122 (Evan: "when SHOWTIME is pressed the phone vibrates for 2 s, the ancient symbols glow like the Frozen arena, the lights dim and the
// player gets the Energy Buff effect, then they select a target"). -ZoogiOldShowtime = off.
#include "CoreMinimal.h"

class AZoogiGameMode;
class AZoogiBall;
class APlayerController;

namespace ZoogiShowtime
{
	ZOOGIROLL_API void Begin(AZoogiGameMode* G, AZoogiBall* B);
	/** the "tap your target" step is open (the player controller sends field taps here) */
	ZOOGIROLL_API bool Picking();
	ZOOGIROLL_API void Tap(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Screen);
}
''')
open(R + 'Private/ZoogiShowtime.cpp', 'w', newline='\r\n').write('''// b122 SHOWTIME sequence - see ZoogiShowtime.h
#include "ZoogiShowtime.h"
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
			for (TActorIterator<ADirectionalLight> It(W); It; ++It) if (ULightComponent* L = It->GetLightComponent()) GLights.Add({ L, L->Intensity });
			GSkies.Reset();
			for (TActorIterator<ASkyLight> It(W); It; ++It) if (USkyLightComponent* L = It->GetLightComponent()) GSkies.Add({ L, L->Intensity });
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (APostProcessVolume* P = W->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SP))
			{
				P->bUnbound = true; P->Priority = 50.f; P->BlendWeight = 0.f;
				P->Settings.bOverride_AutoExposureBias = true; P->Settings.AutoExposureBias = -1.5f;
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
	}
	AActor* Nearest(AZoogiGameMode* G, AZoogiBall* B)
	{
		TArray<AActor*> T = G->LiveTargets(B);
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
			Glow = Dim = FMath::Clamp((float)T / 0.5f, 0.f, 1.f);
			if (T >= 1.2)
			{
				if (G->LiveTargets(B).Num() == 0 || G->Phase != EZoogiPhase::Aiming) { GStage = EStage::Fade; GFadeT0 = Now(); }
				else
				{
					GStage = EStage::Pick;
					if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("SHOWTIME!"), TEXT("TAP YOUR TARGET"), FLinearColor(1.f, 0.85f, 0.3f), 2.f, 1.1f);
				}
			}
		}
		else if (GStage == EStage::Pick)
		{
			if (Now() - GLastGlow > 0.45)
			{	// the targets glow while you choose
				GLastGlow = Now();
				for (AActor* Tt : G->LiveTargets(B)) AZoogiFX::Spawn(G->GetWorld(), Tt->GetActorLocation(), FLinearColor(1.f, 0.8f, 0.3f), 120.f, 0.4f, true, 6.f, Tt);
			}
			if (G->Phase != EZoogiPhase::Aiming) { GStage = EStage::Fade; GFadeT0 = Now(); }
			else if (T > 13.2) { Lock(G, Nearest(G, B), TEXT("time up - nearest")); GStage = EStage::Fade; GFadeT0 = Now(); }
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

void ZoogiShowtime::Begin(AZoogiGameMode* G, AZoogiBall* B)
{
	if (!G || !B || B->bCPU || FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldShowtime"))) return;
	UWorld* W = G->GetWorld();
	if (GStage != EStage::Off) EndAll();
	if (!GTicker) { GTicker = true; FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.f); }
	GG = G; GBall = B; GTurn = G->TurnCounter; GT0 = Now(); GStage = EStage::Rise; GSpin = 0.f;
	ZoogiHaptics::Rumble(2.f, 1.f, W, TEXT("showtime")); // 2 s
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

void ZoogiShowtime::Tap(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Screen)
{
	AZoogiBall* B = GBall.Get();
	if (GStage != EStage::Pick || !G || !PC || !B) return;
	AActor* Best = nullptr; float BestD = 1e9f;
	for (AActor* T : G->LiveTargets(B))
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
''')
C = 'Private/ZoogiP5Command.cpp'
ed1(C, '''		case Showtime: G->MenuAction(TEXT("AwakenUse"), 0);''', '''		case Showtime: G->MenuAction(TEXT("AwakenUse"), 0); ZoogiShowtime::Begin(G, B); // b122: rumble, rune seals, lights dim, energy buff, then the target pick
			''')
b = open(R + C, 'rb').read(); nl = b'\r\n' if b'\r\n' in b else b'\n'
if b'#include "ZoogiShowtime.h"' not in b:
    b = b.replace(b'#include "ZoogiEdgeHang.h"' + nl, b'#include "ZoogiEdgeHang.h"' + nl + b'#include "ZoogiShowtime.h"' + nl, 1); open(R + C, 'wb').write(b)
P = 'Private/ZoogiPlayerController.cpp'
ed1(P, '''	if (ZoogiRedirect::Picking()) { if (bPickPtr''', '''	if (ZoogiShowtime::Picking()) { if (bPtrPress && bPickPtr) ZoogiShowtime::Tap(G, this, Ptr); HoverTarget.Reset(); return; } // b122: SHOWTIME - tap your target
	if (ZoogiRedirect::Picking()) { if (bPickPtr''')
ed1(P, '''#include "ZoogiPauseOrbit.h"''', '''#include "ZoogiPauseOrbit.h"
#include "ZoogiShowtime.h"''')
print('b122a ok')
