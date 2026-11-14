#include "ZoogiEpic.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiActionFun.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"

namespace ZoogiEpic
{
	namespace
	{
		TArray<TWeakObjectPtr<AActor>> GDebris;
		double GLastBig = 0.0;
		bool HitsOff() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoEpicHits")); return b; }
	}

	void Tick(AZoogiGameMode* G, float RealDt) { GDebris.RemoveAll([](const TWeakObjectPtr<AActor>& X) { return !X.IsValid(); }); }

	void Hit(const UObject* Ctx, EHit H)
	{
		if (HitsOff() || !Ctx) return;
		const double Now = FPlatformTime::Seconds();
		switch (H)
		{
		case EHit::Impact: ZoogiUtil::PlaySfx(Ctx, TEXT("EpicImpact"), 0.95f); break;
		case EHit::Big: if (Now - GLastBig > 0.6) { GLastBig = Now; ZoogiUtil::PlaySfx(Ctx, TEXT("EpicBig"), 1.f); } break; // never two stacked
		case EHit::Riser: ZoogiUtil::PlaySfx(Ctx, TEXT("EpicRiser"), 0.8f); break;
		}
	}

	void OnMatchEvent(AZoogiGameMode* G, EZoogiMatchEvent Ev, int32 Who, float Mag)
	{
		if (!G || G->bRoyaleMode) return;
		switch (Ev)
		{
		case EZoogiMatchEvent::RingOut: case EZoogiMatchEvent::KnockedOut: Hit(G, EHit::Big); break;
		case EZoogiMatchEvent::FinalRound: Hit(G, EHit::Riser); break;
		case EZoogiMatchEvent::GuardBreak: Hit(G, EHit::Impact); break;
		case EZoogiMatchEvent::ClimaxLand: Hit(G, EHit::Big); break;
		default: break;
		}
	}

	void DebrisBurst(UWorld* W, const FVector& At, float Power)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDebris"));
		if (!W || bOff || Power <= 0.f) return;
		GDebris.RemoveAll([](const TWeakObjectPtr<AActor>& X) { return !X.IsValid(); });
		const int32 Room = 36 - GDebris.Num(); // a phone-safe cap on live chunks
		const int32 N = FMath::Min(Room, FMath::RoundToInt(3.f + 7.f * FMath::Min(Power, 1.6f)));
		static const TCHAR* Meshes[] = { TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockSmall_001.SM_RockSmall_001"), TEXT("/Game/Inferno_World_Free/Meshes/Rocks/SM_RockMid_002.SM_RockMid_002") };
		for (int32 s = 0; s < N; ++s)
		{
			UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Meshes[s % 3 == 2 ? 1 : 0], nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!M) break;
			const float A = FMath::FRandRange(0.f, 2.f * PI);
			const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* R = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), At + Out * FMath::FRandRange(30.f, 70.f) + FVector(0, 0, 25.f)), SP);
			if (!R) continue;
			R->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* C = R->GetStaticMeshComponent();
			C->SetStaticMesh(M);
			R->SetActorScale3D(FVector(FMath::FRandRange(10.f, 26.f + 10.f * FMath::Min(Power, 1.5f)) / FMath::Max(1.f, (float)M->GetBounds().SphereRadius)));
			C->SetCollisionProfileName(TEXT("PhysicsActor"));
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			C->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore); // never nudges a ball or an orb
			C->SetCastShadow(false);
			C->SetSimulatePhysics(true);
			C->SetPhysicsLinearVelocity(Out * FMath::FRandRange(260.f, 620.f) * (0.6f + 0.4f * Power) + FVector(0, 0, FMath::FRandRange(380.f, 760.f) * (0.6f + 0.4f * Power)));
			C->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-540.f, 540.f), FMath::FRandRange(-540.f, 540.f), FMath::FRandRange(-360.f, 360.f)));
			R->SetLifeSpan(FMath::FRandRange(1.8f, 2.6f));
			GDebris.Add(R);
		}
		AZoogiFX::Stylized(W, Power > 0.9f ? TEXT("Smoke_v2") : TEXT("Smoke"), At, 0.35f + 0.3f * FMath::Min(Power, 1.5f));
		ZoogiUtil::PlaySfx(W, TEXT("Debris"), FMath::Clamp(0.45f + 0.35f * Power, 0.f, 1.f));
	}
}
