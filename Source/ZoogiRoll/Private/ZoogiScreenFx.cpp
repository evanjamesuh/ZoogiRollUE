// Screen shatter + shockwave refraction (see ZoogiScreenFx.h). Visual only.
#include "ZoogiScreenFx.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiClone.h"
#include "Engine/StaticMesh.h"
#include "ZoogiRoll.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SphereComponent.h"
#include "CanvasItem.h"
#include "UnrealClient.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace ZoogiScreenFxNS
{
	constexpr float GrowT = 0.14f;     // cracks race to the edges
	constexpr float FirstFall = 0.52f; // the frozen frame holds, then the pieces drop (staggered)
	constexpr float FallStep = 0.07f;
	constexpr float FallLife = 1.25f;  // a piece is gone this long after it starts falling
	constexpr float WaveT = 0.62f;     // shockwave lifetime
	constexpr float Cooldown = 2.5f;

	void Tri(TArray<FCanvasUVTri>& Out, const FVector2D& A, const FVector2D& B, const FVector2D& C,
		const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
	{
		FCanvasUVTri T;
		T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
		T.V0_UV = UA; T.V1_UV = UB; T.V2_UV = UC;
		T.V0_Color = CA; T.V1_Color = CB; T.V2_Color = CC;
		Out.Add(T);
	}
	void Seg(TArray<FCanvasUVTri>& Out, const FVector2D& A, const FVector2D& B, float Thick, const FLinearColor& C)
	{
		const FVector2D D = B - A;
		const float L = D.Size();
		if (L < 0.5f) return;
		const FVector2D N = FVector2D(-D.Y, D.X) / L * (Thick * 0.5f);
		const FVector2D Z(0.f, 0.f);
		Tri(Out, A - N, A + N, B + N, Z, Z, Z, C, C, C);
		Tri(Out, A - N, B + N, B - N, Z, Z, Z, C, C, C);
	}
	FVector2D Rot(const FVector2D& V, float A) { const float c = FMath::Cos(A), s = FMath::Sin(A); return FVector2D(V.X * c - V.Y * s, V.X * s + V.Y * c); }
}

bool UZoogiScreenFx::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UZoogiScreenFx::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GConfig)
	{
		GConfig->GetBool(TEXT("ZoogiRoll.ScreenFx"), TEXT("bShatter"), bShatterOn, GGameIni);
		GConfig->GetBool(TEXT("ZoogiRoll.ScreenFx"), TEXT("bShockwave"), bWaveOn, GGameIni);
	}
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("ZoogiNoScreenShatter"))) bShatterOn = false;
	if (FParse::Param(Cmd, TEXT("ZoogiNoScreenWave"))) bWaveOn = false;
	bTest = FParse::Param(Cmd, TEXT("ZoogiScreenFxTest"));
	FParse::Value(Cmd, TEXT("ZoogiShots="), ShotDir);
	hHUD = AHUD::OnHUDPostRender.AddUObject(this, &UZoogiScreenFx::OnHUDPostRender);
}

void UZoogiScreenFx::Deinitialize()
{
	if (hHUD.IsValid()) AHUD::OnHUDPostRender.Remove(hHUD);
	hHUD.Reset();
	Super::Deinitialize();
}

TStatId UZoogiScreenFx::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UZoogiScreenFx, STATGROUP_Tickables);
}

void UZoogiScreenFx::Shatter(UWorld* World, const TCHAR* Why)
{
	UZoogiScreenFx* S = World ? World->GetSubsystem<UZoogiScreenFx>() : nullptr;
	if (!S || !S->bShatterOn) return;
	const double Now = FPlatformTime::Seconds();
	if (S->bPendingShatter || Now - S->LastShatter < ZoogiScreenFxNS::Cooldown) return;
	S->bPendingShatter = true; // the frame is grabbed in the next HUD draw (exact camera of that frame)
	S->PendingWhy = Why ? Why : TEXT("?");
}

void UZoogiScreenFx::Shockwave(UWorld* World, const FVector& Center, float Radius, const FLinearColor& Tint)
{
	UZoogiScreenFx* S = World ? World->GetSubsystem<UZoogiScreenFx>() : nullptr;
	if (!S || !S->bWaveOn) return;
	if (S->WaveList.Num() >= 6) S->WaveList.RemoveAt(0);
	S->WaveList.Add({ Center, FMath::Max(Radius, 50.f), Tint, FPlatformTime::Seconds() });
	++S->Waves;
	if (S->bTest && !S->ShotDir.IsEmpty() && S->Waves <= 3)
		for (const double Dl : { 0.10, 0.24, 0.42 })
			S->ShotQ.Add(TPair<FString, double>(FPaths::Combine(S->ShotDir, FString::Printf(TEXT("screenfx_%02d_wave%d.png"), ++S->ShotN, S->Waves)), FPlatformTime::Seconds() + Dl));
	UE_LOG(LogZoogi, Log, TEXT("SCREENFX shockwave #%d at (%.0f,%.0f,%.0f) r=%.0f"), S->Waves, Center.X, Center.Y, Center.Z, Radius);
}

void UZoogiScreenFx::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	if (const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this))
	{
		if (SeenSm < 0 || SM->ShatterCount < SeenSm) SeenSm = SM->ShatterCount;
		else if (SM->ShatterCount > SeenSm) { SeenSm = SM->ShatterCount; } // b57 (Evan): the screen shatter is only the transition into an anime cut-in
	}
	WaveList.RemoveAll([Now](const FWave& W) { return Now - W.Start > ZoogiScreenFxNS::WaveT; });
	if (bTest) TickTest(Now);
	for (int32 i = ShotQ.Num() - 1; i >= 0; --i)
		if (Now >= ShotQ[i].Value)
		{
			FScreenshotRequest::RequestScreenshot(ShotQ[i].Key, true, false);
			UE_LOG(LogZoogi, Log, TEXT("SCREENFX screenshot requested: %s"), *ShotQ[i].Key);
			ShotQ.RemoveAt(i);
			break;
		}
}

void UZoogiScreenFx::TickTest(double Now)
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || G->Phase == EZoogiPhase::Menu || G->Balls.Num() == 0) return;
	if (TestT0 < 0.0) TestT0 = Now;
	auto Shot = [&](const TCHAR* Name, double Dl)
	{
		if (ShotDir.IsEmpty()) return;
		ShotQ.Add(TPair<FString, double>(FPaths::Combine(ShotDir, FString::Printf(TEXT("screenfx_%02d_%s.png"), ++ShotN, Name)), Now + Dl));
	};
	const double T = Now - TestT0;
	if (TestStage == 0 && T > 22.0) // after the stage has formed (its first-turn setup hitches on PC)
	{
		TestStage = 1;
		Shot(TEXT("before"), 0.0);
		Shatter(W, TEXT("test"));
		Shot(TEXT("cracks"), 0.30); Shot(TEXT("falling"), 0.85); Shot(TEXT("falling2"), 1.25); Shot(TEXT("after"), 2.4);
	}
	else if (TestStage == 1 && T > 26.0)
	{
		TestStage = 2;
		// test wave on the floor at the middle of the view
		FVector C = FVector::ZeroVector, L, D;
		APlayerController* PC = W->GetFirstPlayerController();
		int32 VX = 0, VY = 0;
		if (PC) PC->GetViewportSize(VX, VY);
		float Z = 40.f;
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut) { Z = B->BallLocation().Z; break; }
		if (PC && VX > 0 && PC->DeprojectScreenPositionToWorld(VX * 0.5f, VY * 0.6f, L, D) && D.Z < -0.05f) C = L + D * ((Z - L.Z) / D.Z);
		Shockwave(W, C, 420.f, FLinearColor(0.3f, 0.55f, 1.f));
	}
	else if (TestStage == 2 && T > 29.0)
	{
		// a Wolf Pack clone aimed at an orb: it hits, rolls to a stop and detonates (screenshots from Shockwave)
		TestStage = 3;
		for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut) continue;
			const FVector OL = O->OrbLocation();
			FVector2D Dir = FVector2D(OL).GetSafeNormal();
			if (Dir.IsNearlyZero()) Dir = FVector2D(1.f, 0.f);
			const FVector From(OL.X - Dir.X * 650.f, OL.Y - Dir.Y * 650.f, OL.Z + 10.f);
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AZoogiClone* Cl = W->SpawnActor<AZoogiClone>(AZoogiClone::StaticClass(), FTransform(FVector(Dir, 0.f).Rotation(), From), SP))
			{
				Cl->InitClone(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Zoogis/wolfgang/SM_wolfgang.SM_wolfgang")), O, 0, 1300.f, FLinearColor(0.3f, 0.55f, 1.f));
				Cl->Sphere->SetPhysicsLinearVelocity(FVector(Dir, 0.f) * 900.f); // as the Wolf Pack launch does
				UE_LOG(LogZoogi, Log, TEXT("SCREENFX test clone spawned at (%.0f,%.0f) toward orb at (%.0f,%.0f)"), From.X, From.Y, OL.X, OL.Y);
				Shot(TEXT("clone_launch"), 0.4);
			}
			break;
		}
	}
}

bool UZoogiScreenFx::Capture(AHUD* HUD, TObjectPtr<UTextureRenderTarget2D>& RT, float Div)
{
	UWorld* W = GetWorld();
	APlayerController* PC = HUD ? HUD->GetOwningPlayerController() : nullptr;
	if (!W || !PC || !PC->PlayerCameraManager) return false;
	int32 VX = 0, VY = 0;
	PC->GetViewportSize(VX, VY);
	if (VX < 16 || VY < 16) return false;
	const float Sc = FMath::Min(1.f / Div, 1280.f / VX);
	const int32 TW = FMath::Max(16, FMath::RoundToInt(VX * Sc)), TH = FMath::Max(16, FMath::RoundToInt(VY * Sc));
	if (!RT || RT->SizeX != TW || RT->SizeY != TH)
	{
		RT = NewObject<UTextureRenderTarget2D>(this);
		RT->RenderTargetFormat = RTF_RGBA8;
		RT->ClearColor = FLinearColor::Black;
		RT->bAutoGenerateMips = false;
		RT->InitAutoFormat(TW, TH);
		RT->UpdateResourceImmediate(true);
	}
	if (!CapActor)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.ObjectFlags |= RF_Transient;
		CapActor = W->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, SP);
		if (!CapActor) return false;
		USceneCaptureComponent2D* SC = CapActor->GetCaptureComponent2D();
		SC->bCaptureEveryFrame = false;
		SC->bCaptureOnMovement = false;
		SC->bAlwaysPersistRenderingState = true;
		SC->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	}
	USceneCaptureComponent2D* SC = CapActor->GetCaptureComponent2D();
	// the exact horizontal FOV of the view on screen (whatever the aspect constraint / portrait)
	float Fov = PC->PlayerCameraManager->GetFOVAngle();
	FVector L0, D0, L1, D1;
	if (PC->DeprojectScreenPositionToWorld(0.f, VY * 0.5f, L0, D0) && PC->DeprojectScreenPositionToWorld((float)VX, VY * 0.5f, L1, D1))
		Fov = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((float)FVector::DotProduct(D0, D1), -1.f, 1.f)));
	SC->FOVAngle = FMath::Clamp(Fov, 5.f, 170.f);
	SC->SetWorldLocationAndRotation(PC->PlayerCameraManager->GetCameraLocation(), PC->PlayerCameraManager->GetCameraRotation());
	SC->TextureTarget = RT;
	SC->CaptureScene();
	return true;
}

void UZoogiScreenFx::BuildPieces(const FVector2D& Size)
{
	using namespace ZoogiScreenFxNS;
	FRandomStream R(9173 + Serial * 7919);
	const float W = Size.X, H = Size.Y, Sh = FMath::Min(W, H);
	const FVector2D C(W * R.FRandRange(0.44f, 0.56f), H * R.FRandRange(0.40f, 0.52f));
	CrackC = C;
	const FVector2D E[4] = { FVector2D(W * R.FRandRange(0.30f, 0.70f), 0.f), FVector2D(W, H * R.FRandRange(0.28f, 0.66f)),
		FVector2D(W * R.FRandRange(0.30f, 0.70f), H), FVector2D(0.f, H * R.FRandRange(0.32f, 0.70f)) };
	const FVector2D Corner[4] = { FVector2D(W, 0.f), FVector2D(W, H), FVector2D(0.f, H), FVector2D(0.f, 0.f) };
	constexpr int32 N = 8;
	TArray<FVector2D> Crack[4];
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector2D D = E[k] - C, Perp = FVector2D(-D.Y, D.X).GetSafeNormal();
		for (int32 j = 0; j <= N; ++j)
		{
			const float f = (float)j / N;
			FVector2D P = C + D * f;
			if (j > 0 && j < N) P += Perp * R.FRandRange(-1.f, 1.f) * Sh * 0.03f * FMath::Sqrt(FMath::Sin(f * PI));
			Crack[k].Add(P);
		}
	}
	Pieces.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		FPiece P;
		const int32 n = (i + 1) % 4;
		for (int32 j = 0; j <= N; ++j) { P.Poly.Add(Crack[i][j]); P.F.Add((float)j / N); }
		P.Poly.Add(Corner[i]); P.F.Add(-1.f);
		for (int32 j = N; j >= 1; --j) { P.Poly.Add(Crack[n][j]); P.F.Add((float)j / N); }
		P.Anchor = (C + E[i] + Corner[i] + E[n]) * 0.25f;
		P.Out = (P.Anchor - C).GetSafeNormal();
		P.V0 = P.Out * Sh * R.FRandRange(0.20f, 0.34f) + FVector2D(0.f, -Sh * 0.12f);
		P.Spin = (P.Out.X >= 0.f ? 1.f : -1.f) * R.FRandRange(0.9f, 1.8f);
		Pieces.Add(P);
	}
	// bottom pieces drop first
	TArray<int32> Order = { 0, 1, 2, 3 };
	Order.Sort([this](int32 A, int32 B) { return Pieces[A].Anchor.Y > Pieces[B].Anchor.Y; });
	for (int32 k = 0; k < 4; ++k) Pieces[Order[k]].FallAt = FirstFall + FallStep * k + R.FRandRange(0.f, 0.03f);
	// hairline branches off each main crack, owned by the piece on their side
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector2D D = (E[k] - C).GetSafeNormal();
		for (int32 j = 1; j < N; ++j)
		{
			if (R.FRand() > 0.6f) continue;
			const float Side = R.FRand() < 0.5f ? 1.f : -1.f;
			const FVector2D Dir = Rot(D, Side * FMath::DegreesToRadians(R.FRandRange(25.f, 55.f)));
			const FVector2D A = Crack[k][j], B = A + Dir * Sh * R.FRandRange(0.03f, 0.09f);
			FPiece& Owner = Pieces[Side > 0.f ? k : (k + 3) % 4];
			Owner.Lines.Add({ A, B, (float)j / N });
			if (R.FRand() < 0.4f) { const FVector2D Dir2 = Rot(Dir, -Side * FMath::DegreesToRadians(R.FRandRange(20.f, 40.f))); Owner.Lines.Add({ B, B + Dir2 * Sh * R.FRandRange(0.02f, 0.05f), (float)j / N + 0.05f }); }
		}
		// a little spider-web ring near the impact
		const FVector2D A = C + Rot(D, 0.35f) * Sh * 0.045f, B = C + Rot(D, 1.2f) * Sh * 0.05f;
		Pieces[k].Lines.Add({ A, B, 0.2f });
	}
}

void UZoogiScreenFx::OnHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	if (!HUD || !Canvas || HUD->GetWorld() != GetWorld()) return;
	const double Now = FPlatformTime::Seconds();
	if (bPendingShatter)
	{
		bPendingShatter = false;
		if (Capture(HUD, ShatterRT, 2.f) && ShatterRT)
		{
			++Serial; ++Shatters;
			CapSize = FVector2D(Canvas->ClipX, Canvas->ClipY);
			BuildPieces(CapSize);
			ShatterStart = Now; LastShatter = Now; bShatterLive = true;
			UE_LOG(LogZoogi, Log, TEXT("SCREENFX shatter #%d (%s) %dx%d capture %dx%d"), Shatters, *PendingWhy, (int32)CapSize.X, (int32)CapSize.Y, ShatterRT->SizeX, ShatterRT->SizeY);
		}
	}
	if (WaveList.Num()) { if (Capture(HUD, WaveRT, 2.f)) DrawWaves(HUD, Canvas, Now); }
	if (bShatterLive)
	{
		const float t = float(Now - ShatterStart);
		float End = 0.f;
		for (const FPiece& P : Pieces) End = FMath::Max(End, P.FallAt + ZoogiScreenFxNS::FallLife);
		if (t > End || !ShatterRT) bShatterLive = false;
		else DrawShatter(Canvas, t);
	}
}

void UZoogiScreenFx::DrawShatter(UCanvas* Cv, float t)
{
	using namespace ZoogiScreenFxNS;
	const FTexture* Tex = ShatterRT ? ShatterRT->GetResource() : nullptr;
	const FTexture* White = Cv->DefaultTexture ? Cv->DefaultTexture->GetResource() : nullptr;
	if (!Tex || !White) return;
	const FVector2D Scale(Cv->ClipX / CapSize.X, Cv->ClipY / CapSize.Y); // if the window changed size since the grab
	const float Sh = FMath::Min(Cv->ClipX, Cv->ClipY), U = Sh / 1080.f;
	const float G = FMath::Clamp(t / GrowT, 0.f, 1.f);
	const FVector2D Grav(0.f, Sh * 2.6f);
	TArray<FCanvasUVTri> Img, Lines;
	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		const FPiece& P = Pieces[i];
		const float dt = t - P.FallAt;
		FVector2D Off = P.Out * FMath::Min(FMath::Max(t - 0.16f, 0.f) * 40.f, 6.f) * U; // the pieces part a hair
		if (t < 0.26f) Off += FVector2D(FMath::Sin(t * 95.f + i * 1.7f), FMath::Cos(t * 83.f + i * 2.3f)) * 3.5f * U * (1.f - t / 0.26f);
		float Ang = 0.f, Scl = 1.f, Shade = 1.f, A = 1.f;
		if (dt > 0.f)
		{
			Off += P.V0 * dt + Grav * (0.5f * dt * dt);
			Ang = P.Spin * dt;
			Scl = 1.f - 0.07f * dt;
			Shade = 1.f - 0.22f * FMath::Min(dt * 1.4f, 1.f);
			A = 1.f - FMath::Clamp((dt - 0.75f) / (FallLife - 0.75f), 0.f, 1.f);
		}
		if (A <= 0.f) continue;
		const FVector2D An = P.Anchor * Scale;
		auto Xf = [&](const FVector2D& Q) { return An + Rot((Q * Scale - An) * Scl, Ang) + Off; };
		auto UV = [&](const FVector2D& Q) { return FVector2D(Q.X / CapSize.X, Q.Y / CapSize.Y); };
		const FLinearColor Col(Shade, Shade, Shade, A);
		const FVector2D XA = Xf(P.Anchor), UA = UV(P.Anchor);
		const int32 Nv = P.Poly.Num();
		for (int32 k = 0; k < Nv; ++k)
		{
			const FVector2D& Q0 = P.Poly[k];
			const FVector2D& Q1 = P.Poly[(k + 1) % Nv];
			Tri(Img, XA, Xf(Q0), Xf(Q1), UA, UV(Q0), UV(Q1), Col, Col, Col);
		}
		// crack edges (both ends on a crack, grown in so far): dark groove + bright glass edge
		for (int32 k = 0; k < Nv; ++k)
		{
			const int32 k1 = (k + 1) % Nv;
			const float F0 = P.F[k], F1 = P.F[k1];
			if (F0 < 0.f || F1 < 0.f) continue;
			const bool bSameCrack = !(F0 == 0.f && F1 == 0.f);
			if (!bSameCrack) continue;
			if (FMath::Max(F0, F1) > G + 0.001f) continue;
			const FVector2D A0 = Xf(P.Poly[k]), A1 = Xf(P.Poly[k1]);
			Seg(Lines, A0, A1, 8.f * U, FLinearColor(0.02f, 0.03f, 0.05f, 0.75f * A));
			Seg(Lines, A0, A1, 3.2f * U, FLinearColor(0.9f, 0.97f, 1.f, A));
		}
		for (const FPiece::FLine& L : P.Lines)
		{
			if (L.F > G + 0.001f) continue;
			const FVector2D A0 = Xf(L.A), A1 = Xf(L.B);
			Seg(Lines, A0, A1, 4.5f * U, FLinearColor(0.02f, 0.03f, 0.05f, 0.5f * A));
			Seg(Lines, A0, A1, 1.8f * U, FLinearColor(0.92f, 0.98f, 1.f, 0.85f * A));
		}
	}
	if (Img.Num())
	{
		FCanvasTriangleItem It(Img, Tex);
		It.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(It);
	}
	if (Lines.Num())
	{
		FCanvasTriangleItem It(Lines, White);
		It.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(It);
	}
	// impact flash at the crack centre
	if (t < 0.16f)
	{
		const float k = 1.f - t / 0.16f;
		const FVector2D C = CrackC * Scale;
		const float Rr = Sh * (0.03f + 0.12f * (1.f - k));
		TArray<FCanvasUVTri> Fl;
		const FVector2D Z(0.f, 0.f);
		for (int32 s = 0; s < 20; ++s)
		{
			const float a0 = 2.f * PI * s / 20.f, a1 = 2.f * PI * (s + 1) / 20.f;
			Tri(Fl, C, C + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * Rr, C + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * Rr, Z, Z, Z,
				FLinearColor(1.f, 1.f, 1.f, 0.9f * k), FLinearColor(0.7f, 0.9f, 1.f, 0.f), FLinearColor(0.7f, 0.9f, 1.f, 0.f));
		}
		FCanvasTriangleItem It(Fl, White);
		It.BlendMode = SE_BLEND_Additive;
		Cv->DrawItem(It);
	}
}

void UZoogiScreenFx::DrawWaves(AHUD* HUD, UCanvas* Cv, double Now)
{
	using namespace ZoogiScreenFxNS;
	const FTexture* Tex = WaveRT ? WaveRT->GetResource() : nullptr;
	const FTexture* White = Cv->DefaultTexture ? Cv->DefaultTexture->GetResource() : nullptr;
	APlayerController* PC = HUD->GetOwningPlayerController();
	if (!Tex || !White || !PC || !PC->PlayerCameraManager) return;
	const FVector CamL = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamF = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector2D Clip(Cv->ClipX, Cv->ClipY);
	const float U = FMath::Min(Clip.X, Clip.Y) / 1080.f;
	bool bOk = true;
	auto Proj = [&](const FVector& P) -> FVector2D
	{
		if (FVector::DotProduct(P - CamL, CamF) < 10.f) bOk = false;
		const FVector S = Cv->Project(P);
		return FVector2D(S.X, S.Y);
	};
	for (const FWave& W : WaveList)
	{
		const float p = FMath::Clamp(float(Now - W.Start) / WaveT, 0.f, 1.f);
		const float r = W.R * (1.f - FMath::Pow(1.f - p, 2.4f));
		const float w = FMath::Lerp(95.f, 40.f, p);
		const float Amp = 34.f * U * (1.f - p);
		const float Al = 1.f - p * p;
		const FVector Gc(W.C.X, W.C.Y, W.C.Z - 30.f);
		bOk = true;
		const FVector2D Cs = Proj(Gc);
		constexpr int32 NS = 40;
		TArray<FVector2D> In, Mid, Out, MidUV;
		for (int32 s = 0; s <= NS; ++s)
		{
			const float a = 2.f * PI * s / NS;
			const FVector D(FMath::Cos(a), FMath::Sin(a), 0.f);
			In.Add(Proj(Gc + D * FMath::Max(r - w, 0.f)));
			Mid.Add(Proj(Gc + D * r));
			Out.Add(Proj(Gc + D * (r + w)));
			const FVector2D Rd = (Mid.Last() - Cs).GetSafeNormal();
			MidUV.Add((Mid.Last() - Rd * Amp) / Clip);
		}
		if (!bOk) continue; // part of the ring behind the camera: skip the screen pass (the world ring still plays)
		TArray<FCanvasUVTri> Band, Rim;
		const FLinearColor C0(1.f, 1.f, 1.f, 0.f), CM(1.04f, 1.08f, 1.16f, Al);
		// crest: tinted (translucent, so it reads on snow as well as on lava); trough: a soft dark band just inside it
		const FLinearColor T0(W.Tint.R, W.Tint.G, W.Tint.B, 0.f), TM(W.Tint.R * 0.75f + 0.25f, W.Tint.G * 0.75f + 0.25f, W.Tint.B * 0.75f + 0.25f, 0.62f * Al);
		const FLinearColor D0(0.03f, 0.06f, 0.16f, 0.f), DM(0.03f, 0.06f, 0.16f, 0.30f * Al);
		TArray<FCanvasUVTri> Dark;
		const FVector2D Z(0.f, 0.f);
		for (int32 s = 0; s < NS; ++s)
		{
			const int32 n = s + 1;
			Tri(Band, In[s], Mid[s], Mid[n], In[s] / Clip, MidUV[s], MidUV[n], C0, CM, CM);
			Tri(Band, In[s], Mid[n], In[n], In[s] / Clip, MidUV[n], In[n] / Clip, C0, CM, C0);
			Tri(Band, Mid[s], Out[s], Out[n], MidUV[s], Out[s] / Clip, Out[n] / Clip, CM, C0, C0);
			Tri(Band, Mid[s], Out[n], Mid[n], MidUV[s], Out[n] / Clip, MidUV[n], CM, C0, CM);
			Tri(Dark, In[s], Mid[s], Mid[n], Z, Z, Z, D0, DM, DM);
			Tri(Dark, In[s], Mid[n], In[n], Z, Z, Z, D0, DM, D0);
			// tinted leading edge: strongest at the crest, fading outward and inward
			const FVector2D Ma = (Mid[s] + Out[s]) * 0.5f, Mb = (Mid[n] + Out[n]) * 0.5f;
			Tri(Rim, Mid[s], Ma, Mb, Z, Z, Z, T0, TM, TM);
			Tri(Rim, Mid[s], Mb, Mid[n], Z, Z, Z, T0, TM, T0);
			Tri(Rim, Ma, Out[s], Out[n], Z, Z, Z, TM, T0, T0);
			Tri(Rim, Ma, Out[n], Mb, Z, Z, Z, TM, T0, TM);
		}
		FCanvasTriangleItem B(Band, Tex);
		B.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(B);
		FCanvasTriangleItem Dk(Dark, White);
		Dk.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(Dk);
		FCanvasTriangleItem R(Rim, White);
		R.BlendMode = SE_BLEND_Translucent;
		Cv->DrawItem(R);
	}
}
