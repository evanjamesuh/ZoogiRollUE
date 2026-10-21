// framed: see ZoogiViewportClient.h
#include "ZoogiViewportClient.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiFramedHUDWidget.h"
#include "ZoogiArenaHUDWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UZoogiViewportClient::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	ZoogiFramed::TickShots(W);
	UZoogiArenaHUDWidget::Ensure(W); // arena HUD layer (no-op unless HudStyle=Arena)
	if (!ZoogiFramed::IsEnabled() || !W) return;
	ZoogiFramed::TickPulse(DeltaTime);
	AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>();
	TrayOpen = FMath::FInterpTo(TrayOpen, ZoogiFramed::WantTrayOpen(G) ? 1.f : 0.f, DeltaTime, 7.f);
	ZoogiFramed::TickAimCheck(W);
	// The framed UMG layer: one per game world, under the Awaken (5) and card hand (6) widgets so they draw over the tray.
	if (!Widget.IsValid() || Widget->GetWorld() != W)
	{
		APlayerController* PC = W->GetFirstPlayerController();
		if (PC && PC->IsLocalController() && G)
		{
			UZoogiFramedHUDWidget* NW = CreateWidget<UZoogiFramedHUDWidget>(PC, UZoogiFramedHUDWidget::StaticClass());
			if (NW) { NW->AddToViewport(4); Widget = NW; UE_LOG(LogZoogi, Log, TEXT("FRAMED HUD layer up (-ZoogiFramedHUD)")); }
		}
	}
}

void UZoogiViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	UWorld* W = GetWorld();
	ULocalPlayer* LP = GetOuterUEngine() ? GetOuterUEngine()->GetFirstGamePlayer(W) : nullptr;
	const FIntPoint Sz = InViewport ? InViewport->GetSizeXY() : FIntPoint::ZeroValue;
	// The engine's on-screen debug messages ("Preparing Textures (n)", "'DisableAllScreenMessages' to suppress") draw at
	// the viewport's top-left, i.e. over the left team column header in the framed layout: hide them while framed
	// (kept with -ZoogiDebugUI / -ZoogiScreenMessages; non-shipping builds only ever show them).
	{
		static const bool bKeep = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDebugUI")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiScreenMessages"));
		const bool bHide = !bKeep && ZoogiFramed::IsActive(W);
		if (bHide != bMsgsHidden) { GAreScreenMessagesEnabled = !bHide; bMsgsHidden = bHide; }
	}
	if (LP && Sz.X > 0 && Sz.Y > 0 && ZoogiFramed::IsActive(W))
	{
		const FVector2D V(Sz.X, Sz.Y);
		const ZoogiFramed::FLayout L = ZoogiFramed::Compute(V, ZoogiFramed::SafeInsets(V), TrayOpen);
		ZoogiFramed::SetCurrent(L);
		if (!bRectApplied) SavedAxis = LP->AspectRatioAxisConstraint;
		LP->Origin = FVector2D(L.Arena.Min.X / V.X, L.Arena.Min.Y / V.Y);
		LP->Size = FVector2D((L.Arena.Max.X - L.Arena.Min.X) / V.X, (L.Arena.Max.Y - L.Arena.Min.Y) / V.Y);
		// Wider than 16:9: keep the 16:9 vertical FOV so the ring is never cropped top / bottom; narrower: keep the horizontal FOV.
		const float A = (L.Arena.Max.X - L.Arena.Min.X) / FMath::Max(1.f, L.Arena.Max.Y - L.Arena.Min.Y);
		LP->AspectRatioAxisConstraint = A > 16.f / 9.f ? AspectRatio_MaintainYFOV : AspectRatio_MaintainXFOV;
		bRectApplied = true;
		if (!bLoggedFirst)
		{
			bLoggedFirst = true;
			UE_LOG(LogZoogi, Log, TEXT("FRAMED view rect: viewport %dx%d arena [%.0f,%.0f - %.0f,%.0f] (%.3f aspect) compact=%d safe L%.0f T%.0f R%.0f B%.0f"),
				Sz.X, Sz.Y, L.Arena.Min.X, L.Arena.Min.Y, L.Arena.Max.X, L.Arena.Max.Y, A, L.bCompact ? 1 : 0, L.Safe.Left, L.Safe.Top, L.Safe.Right, L.Safe.Bottom);
		}
	}
	else if (bRectApplied && LP)
	{
		LP->AspectRatioAxisConstraint = SavedAxis;
		bRectApplied = false;
		ZoogiFramed::Clear();
		LayoutPlayers(); // back to the engine's own (full-screen) layout
	}
	Super::Draw(InViewport, SceneCanvas);
}
