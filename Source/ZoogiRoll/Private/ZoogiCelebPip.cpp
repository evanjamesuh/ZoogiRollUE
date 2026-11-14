// Nov 2 b74 - see ZoogiCelebPip.h.
#include "ZoogiCelebPip.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
	TWeakObjectPtr<UWorld> GPipWorld;
	TWeakObjectPtr<AActor> GPipFocus;
	TWeakObjectPtr<ASceneCapture2D> GPipCam;
	TStrongObjectPtr<UTextureRenderTarget2D> GPipRT;
	FTSTicker::FDelegateHandle GPipTick;
	float GPipYaw = 0.f, GPipDist = 420.f;
	double GPipT0 = 0.0, GPipEnd = -1.0;
	bool bPipOn = false;

	FVector PipFocusLoc(AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}

	bool PipTick(float Dt)
	{
		ASceneCapture2D* Cam = GPipCam.Get();
		AActor* F = GPipFocus.Get();
		if (!Cam) { GPipTick.Reset(); return false; }
		if (GPipEnd > 0.0 && FPlatformTime::Seconds() > GPipEnd) { Cam->Destroy(); GPipCam.Reset(); GPipTick.Reset(); return false; }
		if (F)
		{
			const float T = (float)(FPlatformTime::Seconds() - GPipT0);
			const float Yaw = GPipYaw + 14.f * T; // a slow orbit, like the old close-up
			const FVector C = PipFocusLoc(F) + FVector(0.f, 0.f, 70.f);
			const FVector Eye = C - FRotator(-10.f, Yaw, 0.f).Vector() * GPipDist + FVector(0.f, 0.f, 40.f);
			Cam->SetActorLocationAndRotation(Eye, (C - Eye).Rotation());
		}
		return true;
	}
}

namespace ZoogiCelebPip
{
	bool Enabled() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeFullCam")); return !bOld; }

	void Start(UWorld* W, AActor* Focus, float Yaw, float DistMul)
	{
		if (!Enabled() || !W || !Focus) return;
		GPipWorld = W; GPipFocus = Focus; GPipYaw = Yaw; GPipDist = 380.f * FMath::Clamp(DistMul, 0.5f, 2.5f);
		GPipT0 = FPlatformTime::Seconds(); GPipEnd = -1.0; bPipOn = true;
		if (!GPipRT.IsValid())
		{
			GPipRT.Reset(NewObject<UTextureRenderTarget2D>(GetTransientPackage()));
			GPipRT->RenderTargetFormat = RTF_RGBA8;
			GPipRT->ClearColor = FLinearColor::Black;
			GPipRT->InitAutoFormat(512, 288);
			GPipRT->UpdateResourceImmediate(true);
		}
		ASceneCapture2D* Cam = GPipCam.Get();
		if (!Cam || Cam->GetWorld() != W)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Cam = W->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, SP);
			if (!Cam) { bPipOn = false; return; }
			USceneCaptureComponent2D* CC = Cam->GetCaptureComponent2D();
			CC->TextureTarget = GPipRT.Get();
			CC->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			CC->FOVAngle = 42.f;
			CC->bCaptureEveryFrame = true;
			CC->bCaptureOnMovement = false;
			CC->bAlwaysPersistRenderingState = true;
			GPipCam = Cam;
		}
		Cam->GetCaptureComponent2D()->bCaptureEveryFrame = true;
		PipTick(0.f);
		if (!GPipTick.IsValid()) GPipTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PipTick), 0.f);
		UE_LOG(LogZoogi, Log, TEXT("CELEB PIP on (%s)"), *Focus->GetName());
	}

	void Stop()
	{
		if (!bPipOn) return;
		bPipOn = false;
		GPipEnd = FPlatformTime::Seconds() + 0.25; // the window fades out, then the capture goes
		if (ASceneCapture2D* Cam = GPipCam.Get()) Cam->GetCaptureComponent2D()->bCaptureEveryFrame = false;
		UE_LOG(LogZoogi, Log, TEXT("CELEB PIP off"));
	}

	void Draw(UCanvas* C)
	{
		if (!C || !GPipRT.IsValid() || !GPipCam.IsValid()) return;
		const double Now = FPlatformTime::Seconds();
		const float In = FMath::Clamp((float)(Now - GPipT0) / 0.18f, 0.f, 1.f);
		const float Out = bPipOn ? 1.f : FMath::Clamp((float)(GPipEnd - Now) / 0.25f, 0.f, 1.f);
		const float K = In * Out;
		if (K <= 0.01f) return;
		const float W = C->ClipX, H = C->ClipY, U = FMath::Min(W, H) / 1080.f;
		const bool bPort = H > W;
		const float PW = bPort ? W * 0.56f : FMath::Min(W * 0.3f, 520.f * U * 1.4f), PH = PW * 9.f / 16.f;
		const float X = (bPort ? W * 0.5f - PW * 0.5f : 28.f * U) - (1.f - In) * 60.f * U, Y = bPort ? H * 0.6f : H * 0.5f;
		// P5 frame: red backing pushed out, white rim, black edge
		FCanvasTileItem Red(FVector2D(X + 10.f * U, Y + 10.f * U), FVector2D(PW, PH), FLinearColor(0.88f, 0.06f, 0.12f, 0.95f * K)); Red.BlendMode = SE_BLEND_Translucent; C->DrawItem(Red);
		FCanvasTileItem Rim(FVector2D(X - 5.f * U, Y - 5.f * U), FVector2D(PW + 10.f * U, PH + 10.f * U), FLinearColor(1.f, 1.f, 1.f, K)); Rim.BlendMode = SE_BLEND_Translucent; C->DrawItem(Rim);
		FCanvasTileItem Img(FVector2D(X, Y), GPipRT->GetResource(), FVector2D(PW, PH), FLinearColor(1.f, 1.f, 1.f, K)); Img.BlendMode = SE_BLEND_Opaque; C->DrawItem(Img);
	}
}
