// Oct 27 (Evan's Build 9 corrections): Frozen Ring touch controls - LEFT stick rolls on the outer ring (and turns the aim a little
// on a bridge), RIGHT stick pull-back + release launches (bridge launch point, later turns in the arena), a tap picks an orb or a
// player and nothing else. HUD buttons (SUPERPOWER / REACTION) and UMG (TAC, settings, cards) keep their own presses, and a press
// never launches by itself: only a pull on the right stick that is let go past MIN PULL does. Replaces the classic drag aim here.
#include "ZoogiPlayerController.h"
#include "ZoogiGameMode.h"
#include "ZoogiFrozenRoam.h"
#include "ZoogiRoyale.h"
#include "ZoogiHUD.h"
#include "ZoogiBall.h"
#include "ZoogiTune.h"
#include "ZoogiRoll.h"
#include "Camera/PlayerCameraManager.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"

void AZoogiPlayerController::FrozenStickLayout(const FVector2D& View, FVector2D& OutLHome, float& OutLR, FVector2D& OutRHome, float& OutRR)
{
	const ZoogiRoyaleUI::FPad Pad = ZoogiRoyaleUI::Layout(View);
	const float K = Pad.U * (1080.f / 923.f);
	OutLR = Pad.StickR * ZoogiTune::Get(TEXT("lstick.size"));
	OutRR = Pad.StickR * ZoogiTune::Get(TEXT("rstick.size"));
	OutLHome = Pad.StickHome + FVector2D(ZoogiTune::Get(TEXT("lstick.x")), -ZoogiTune::Get(TEXT("lstick.y"))) * K;
	OutRHome = FVector2D(View.X - 262.f * K, View.Y - 266.f * K) + FVector2D(ZoogiTune::Get(TEXT("rstick.x")), -ZoogiTune::Get(TEXT("rstick.y"))) * K;
}

void AZoogiPlayerController::FrozenInput(AZoogiGameMode* G, AZoogiHUD* Hud, float Dt)
{
	if (!G) return;
	AZoogiBall* Me = G->GetActiveBall();
	int32 VX = 1920, VY = 1080;
	GetViewportSize(VX, VY);
	const FVector2D View((float)FMath::Max(VX, 64), (float)FMath::Max(VY, 64));
	FVector2D LH, RH; float LR = 100.f, RR = 100.f;
	FrozenStickLayout(View, LH, LR, RH, RR);
	const float U = FMath::Min(View.X, View.Y) / 1080.f;
	const bool bCanLaunch = G->FrozenCanLaunch();
	const bool bRoam = G->FrozenRoamActive() && (G->RoamState == EZoogiRoamState::FreeRoam || G->RoamState == EZoogiRoamState::BridgeApproach);
	const bool bPrep = G->FrozenRoamActive() && G->RoamState == EZoogiRoamState::LaunchPrep;

	for (int32 t = 0; t < 5; ++t)
	{
		float X = 0.f, Y = 0.f; bool bDown = false;
		GetInputTouchState((ETouchIndex::Type)t, X, Y, bDown);
#if PLATFORM_DESKTOP
		// mouse-as-touch on a PC: the real cursor is the finger (Touch1 keeps a stale position)
		float MX = 0.f, MY = 0.f;
		if (t == 0 && GetMousePosition(MX, MY) && (bDown || IsInputKeyDown(EKeys::LeftMouseButton))) { X = MX; Y = MY; bDown = true; }
#endif
		const FVector2D Pt(X, Y);
		if (bDown && !FrSlotDown[t])
		{
			FrSlotStart[t] = Pt;
			FrSlotTime[t] = FPlatformTime::Seconds();
			FrSlotRole[t] = 4; // a tap until it proves otherwise
			FName Action; int32 Index = 0;
			if (Hud && Hud->HitTest(Pt, Action, Index)) { G->MenuAction(Action, Index); FrSlotRole[t] = 1; }
			else if (Hud && Hud->IsOverUI(Pt)) FrSlotRole[t] = 1; // TAC / settings / cards: theirs
			else if (Pt.X < View.X * 0.5f && FrMoveSlot < 0)
			{
				FrMoveSlot = t; FrSlotRole[t] = 2;
				// press on / near the stick: it stays home; anywhere else on the left half: the stick comes to the thumb
				FrMoveBase = FVector2D::Distance(Pt, LH) <= LR * 1.4f ? LH : FVector2D(FMath::Clamp(Pt.X, LR, View.X * 0.5f - LR * 0.5f), FMath::Clamp(Pt.Y, LR + 90.f * U, View.Y - LR));
				FrMoveNow = Pt;
			}
			else if (bCanLaunch && FrPullSlot < 0 && FVector2D::Distance(Pt, RH) <= RR * 1.5f)
			{
				FrPullSlot = t; FrSlotRole[t] = 3;
				FrPullBase = Pt; FrPullNow = Pt; FrPullFrac = 0.f;
				FrPullRefYaw = G->FrozenNeedsEntry(G->ActivePlayer) ? G->FrozenPrepYaw() : (Me ? Me->AimYaw : 0.f);
			}
		}
		else if (bDown)
		{
			if (t == FrMoveSlot) FrMoveNow = Pt;
			if (t == FrPullSlot) FrPullNow = Pt;
			if (FrSlotRole[t] == 4 && FVector2D::Distance(Pt, FrSlotStart[t]) > 28.f * FMath::Max(U, 0.5f)) FrSlotRole[t] = 0; // a drag is not a tap
		}
		else if (FrSlotDown[t]) FrozenRelease(G, t, Pt);
		FrSlotDown[t] = bDown;
	}

	// ---- left stick: roll (free roam / bridge approach), small aim turn on the bridge launch point
	FVector2D Stick = FVector2D::ZeroVector;
	if (FrMoveSlot >= 0) Stick = (FrMoveNow - FrMoveBase) / FMath::Max(LR, 1.f);
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Stick.Y -= 1.f;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Stick.Y += 1.f;
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Stick.X -= 1.f;
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Stick.X += 1.f;
	const float Mag = FMath::Min(Stick.Size(), 1.f);
	const float Dead = ZoogiTune::Get(TEXT("lstick.dead")), Curve = ZoogiTune::Get(TEXT("lstick.curve")), Sens = ZoogiTune::Get(TEXT("lstick.sens"));
	const float Shaped = Mag <= Dead ? 0.f : FMath::Min(1.f, FMath::Pow((Mag - Dead) / FMath::Max(1.f - Dead, 0.01f), Curve) * Sens);
	const FVector2D Dir2 = Stick.GetSafeNormal();
	if (bRoam && Shaped > 0.f && PlayerCameraManager)
	{
		const float Cy = FMath::DegreesToRadians(PlayerCameraManager->GetCameraRotation().Yaw);
		const FVector2D Fwd(FMath::Cos(Cy), FMath::Sin(Cy)), Right(-FMath::Sin(Cy), FMath::Cos(Cy));
		G->FrozenSetMoveInput((Fwd * -Dir2.Y + Right * Dir2.X) * Shaped);
	}
	else G->FrozenSetMoveInput(FVector2D::ZeroVector);
	if (bPrep && Shaped > 0.f) G->FrozenAdjustAim(Dir2.X * Shaped * 60.f * Dt);

	// ---- right stick: pull back = power, the opposite way = the shot's heading (inside the bridge's aim cone)
	if (FrPullSlot >= 0 && !bCanLaunch) { FrPullSlot = -1; FrPullFrac = 0.f; }
	if (FrPullSlot >= 0)
	{
		const FVector2D P = FrPullNow - FrPullBase;
		const float L = P.Size() / FMath::Max(RR * 1.2f, 1.f);
		const float PDead = ZoogiTune::Get(TEXT("rstick.dead")), PCurve = ZoogiTune::Get(TEXT("rstick.curve")), PMax = ZoogiTune::Get(TEXT("rstick.max"));
		FrPullFrac = L <= PDead ? 0.f : FMath::Pow((FMath::Min(L, 1.f) - PDead) / FMath::Max(1.f - PDead, 0.01f), PCurve) * PMax;
		float Ang = 0.f; // 0 = straight up the screen
		if (P.Size() > 4.f) { const FVector2D S = -P.GetSafeNormal(); Ang = FMath::RadiansToDegrees(FMath::Atan2(S.X, -S.Y)); }
		if (G->FrozenNeedsEntry(G->ActivePlayer))
		{
			const float Cone = ZoogiTune::Get(TEXT("bridge.cone"));
			FrPullYaw = FRotator::NormalizeAxis(G->FrozenPrepYaw() + FMath::Clamp(Ang, -Cone, Cone) * 0.5f);
		}
		else if (G->TargetLock.IsValid() && Me) FrPullYaw = Me->AimYaw; // a picked target sets the heading; the pull sets the power
		else { FrPullYaw = FRotator::NormalizeAxis(FrPullRefYaw + FMath::Clamp(Ang, -100.f, 100.f)); if (Me) Me->AimYaw = FrPullYaw; }
	}
	// PC: hold Space to charge, let go to launch (same rules as the right stick)
	if (bCanLaunch && IsInputKeyDown(EKeys::SpaceBar)) FrKeyCharge = FMath::Min(1.f, FrKeyCharge + Dt / 1.2f);
	else if (FrKeyCharge > 0.f)
	{
		if (bCanLaunch && Me && FrKeyCharge > 0.05f) G->FrozenHumanLaunch(G->FrozenNeedsEntry(G->ActivePlayer) ? G->FrozenPrepYaw() : Me->AimYaw, FrKeyCharge);
		FrKeyCharge = 0.f;
	}
}

void AZoogiPlayerController::FrozenRelease(AZoogiGameMode* G, int32 Slot, const FVector2D& Pt)
{
	const uint8 SlotKind = FrSlotRole[Slot];
	FrSlotRole[Slot] = 0;
	if (Slot == FrMoveSlot) { FrMoveSlot = -1; if (G) G->FrozenSetMoveInput(FVector2D::ZeroVector); }
	if (Slot == FrPullSlot)
	{
		FrPullSlot = -1;
		const float Frac = FrPullFrac;
		FrPullFrac = 0.f;
		if (G && Frac > 0.f) G->FrozenHumanLaunch(FrPullYaw, Frac);
		else UE_LOG(LogZoogi, Log, TEXT("FROZEN INPUT: right stick let go without a pull - no launch"));
		return;
	}
	if (SlotKind != 4 || !G) return;
	if (FPlatformTime::Seconds() - FrSlotTime[Slot] > 0.45) return;
	// a tap: an orb or a player becomes the target; empty ground / scenery does nothing
	if (G->FrozenNeedsEntry(G->ActivePlayer) || !G->IsAimStep()) return;
	const FVector2D At = FrSlotStart[Slot];
	AActor* T = PickTarget(At.X, At.Y);
	if (T && G->IsValidTarget(T))
	{
		G->LockOn(T, false);
		UE_LOG(LogZoogi, Log, TEXT("FROZEN INPUT: tap targets %s"), *T->GetName());
	}
	else UE_LOG(LogZoogi, Log, TEXT("FROZEN INPUT: tap at (%.0f, %.0f) hit no orb / player - ignored"), At.X, At.Y);
	(void)Pt;
}
