# b120e (Evan 18:32: "CPU are able to use multiple skills at once before launch - how can a player do the same as an option? Also during
# gameplay pauses the player can rotate the camera around the player, then it smoothly transitions back to normal play"):
#  1) P5 skill list: COMBO toggle (bottom left). With COMBO on, taps tick skills (order badges, running SP total, unaffordable rows
#     greyed); CAST n! plays them back to back (0.12 s apart; a targeted skill opens its pick, the queue continues after FIRE;
#     CANCEL drops the rest). -ZoogiNoCombo hides it.
#  2) ZoogiPauseOrbit: while the world is paused (pause menu, anime freeze-frames), a drag orbits the camera around the human's Zoogi
#     (yaw free, height -15..40 deg); on resume the orbit eases back to the normal camera (~0.7 s). -ZoogiNoPauseOrbit = off.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
C = 'Private/ZoogiP5Command.cpp'
# widget state
ed1(C, '''		int32 Mode = 0;                  // 0 commands, 1 skills, 2 items''', '''		int32 Mode = 0;                  // 0 commands, 1 skills, 2 items
		bool bCombo = false; TArray<int32> ComboPicks; // b120: COMBO multi-pick (rows ticked, in pick order)''')
ed1(C, '''		TArray<FSkillRow> CpuRows; TArray<FName> CpuCasts;''', '''		TArray<FSkillRow> CpuRows; TArray<FName> CpuCasts;
		// b120 COMBO: rows ticked in the skill list, then the queue of card ids played back to back
		bool bCombo = false; TArray<int32> ComboSel; TArray<FName> ComboQueue; int32 ComboTurn = -1; double ComboNextAt = 0.0;''')
# paint: header, ticks, COMBO / CAST plates
ed1(C, '''				CText(Out, Geo, L + 1, Mode == 1 ? FString::Printf(TEXT("SKILL   SP %d"), FMath::Max(SP, 0)) : FString(TEXT("ITEM")), ZoogiP5::Font(0, 34.f * S), P0 + FVector2D(150.f * S, -28.f * S), -3.f, Red);''',
'''				int32 ComboCost = 0; for (int32 Ci : ComboPicks) if (Skills.IsValidIndex(Ci)) ComboCost += Skills[Ci].Cost;
				const FString Hdr = Mode != 1 ? FString(TEXT("ITEM")) : bCombo ? FString::Printf(TEXT("COMBO   SP %d / %d"), ComboCost, FMath::Max(SP, 0)) : FString::Printf(TEXT("SKILL   SP %d"), FMath::Max(SP, 0));
				CText(Out, Geo, L + 1, Hdr, ZoogiP5::Font(0, 34.f * S), P0 + FVector2D((bCombo ? 190.f : 150.f) * S, -28.f * S), -3.f, Red);''')
ed1(C, '''					Hits.Add({ C, FVector2D((PW - 30.f * S) * 0.5f, (RowH - 10.f * S) * 0.5f), -2.f, 1, r });''', '''					Hits.Add({ C, FVector2D((PW - 30.f * S) * 0.5f, (RowH - 10.f * S) * 0.5f), -2.f, 1, r });
					const int32 PickN = bCombo ? ComboPicks.IndexOfByKey(r) : INDEX_NONE;
					if (PickN != INDEX_NONE)
					{	// b120: COMBO order badge on the ticked row
						const FVector2D Bd = C + FVector2D(-PW * 0.5f + 6.f * S, 0.f);
						Plate(Out, Geo, L + 3, Bd, 44.f * S, 44.f * S, 6.f, 6.f * S, Red);
						CText(Out, Geo, L + 4, FString::FromInt(PickN + 1), ZoogiP5::Font(0, 30.f * S), Bd, 6.f, Wht);
					}''')
ed1(C, '''				Hits.Add({ Bc, FVector2D(75.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -1 });''', '''				Hits.Add({ Bc, FVector2D(75.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -1 });
				static const bool bNoCombo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCombo"));
				if (Mode == 1 && !bNoCombo)
				{	// b120 (Evan: "CPUs use multiple skills at once - how can a player do the same"): COMBO toggle + CAST
					const FVector2D Cc = P0 + FVector2D(105.f * S, RowH * (Skills.Num() + 0.6f));
					Plate(Out, Geo, L + 1, Cc, 180.f * S, RowH - 14.f * S, -2.f, 10.f * S, bCombo ? Red : FLinearColor(0.12f, 0.12f, 0.12f));
					CText(Out, Geo, L + 2, bCombo ? TEXT("COMBO ON") : TEXT("COMBO"), ZoogiP5::Font(0, 26.f * S), Cc, -2.f, Wht);
					Hits.Add({ Cc, FVector2D(90.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -2 });
					if (bCombo && ComboPicks.Num() > 0)
					{
						const FVector2D Kc = P0 + FVector2D(310.f * S, RowH * (Skills.Num() + 0.6f));
						const float Pu = 1.f + 0.05f * FMath::Sin(T * 9.f);
						Plate(Out, Geo, L + 1, Kc, 200.f * S * Pu, (RowH - 14.f * S) * Pu, -2.f, 10.f * S, FLinearColor(1.f, 0.82f, 0.15f));
						CText(Out, Geo, L + 2, FString::Printf(TEXT("CAST %d!"), ComboPicks.Num()), ZoogiP5::Font(0, 28.f * S), Kc, -2.f, Blk);
						Hits.Add({ Kc, FVector2D(100.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -3 });
					}
				}''')
# DoSkill: COMBO toggle / CAST / ticking
ed1(C, '''		if (R < 0 || W.Mode == 2) { W.Mode = 0; GS.SkillsTurn = -1;''', '''		if (R == -2 && W.Mode == 1)
		{	// b120: COMBO on / off
			GS.bCombo = !GS.bCombo; GS.ComboSel.Reset();
			ZoogiUtil::PlaySfx(G, GS.bCombo ? TEXT("UiConfirm") : TEXT("UiCancel"), 0.7f, GS.bCombo ? 1.3f : 1.f);
			UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO %s"), GS.bCombo ? TEXT("on") : TEXT("off"));
			return;
		}
		if (R == -3 && W.Mode == 1)
		{	// b120: CAST - the ticked skills play back to back (Tick works the queue)
			const UZoogiCardSubsystem* CSq = UZoogiCardSubsystem::Get(G);
			const AZoogiBall* Hq = Human(G);
			const FZoogiCardPlayerState* PSq = (CSq && Hq) ? CSq->Rules().Player(Hq->PlayerIndex) : nullptr;
			GS.ComboQueue.Reset();
			if (PSq) for (int32 Ri : GS.ComboSel) if (W.Skills.IsValidIndex(Ri) && PSq->Hand.IsValidIndex(W.Skills[Ri].Slot)) GS.ComboQueue.Add(PSq->Hand[W.Skills[Ri].Slot].CardId);
			GS.ComboTurn = G->TurnCounter; GS.ComboNextAt = 0.0;
			UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO cast %d: %s"), GS.ComboQueue.Num(), *FString::JoinBy(GS.ComboQueue, TEXT(", "), [](const FName& N) { return N.ToString(); }));
			GS.bCombo = false; GS.ComboSel.Reset();
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.35f);
			if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(TEXT("COMBO!"), FString::Printf(TEXT("%d SKILLS AT ONCE"), GS.ComboQueue.Num()), FLinearColor(1.f, 0.82f, 0.15f), 1.6f, 1.f);
			Dismiss(G);
			return;
		}
		if (R >= 0 && GS.bCombo && W.Mode == 1)
		{	// b120: tick / untick a row (only what the SP covers)
			if (!W.Skills.IsValidIndex(R)) return;
			if (GS.ComboSel.Contains(R)) { GS.ComboSel.Remove(R); ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.5f, 1.2f); return; }
			int32 Sum = W.Skills[R].Cost; for (int32 Ci : GS.ComboSel) if (W.Skills.IsValidIndex(Ci)) Sum += W.Skills[Ci].Cost;
			if (!W.Skills[R].bOn || Sum > W.SP) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			GS.ComboSel.Add(R); ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.0f + 0.1f * GS.ComboSel.Num());
			return;
		}
		if (R < 0 || W.Mode == 2) { GS.bCombo = false; GS.ComboSel.Reset(); W.Mode = 0; GS.SkillsTurn = -1;''')
# widget mirrors the combo state each refresh
ed1(C, '''		W.SP = PS ? PS->Energy : -1;
		W.Skills.Reset();''', '''		W.SP = PS ? PS->Energy : -1;
		W.Skills.Reset();
		W.bCombo = GS.bCombo; W.ComboPicks = GS.ComboSel; // b120''')
# CANCEL during a combo pick drops the rest of the queue
ed1(C, '''	void BackToSkills()
	{
		Reopen();''', '''	void BackToSkills()
	{
		if (GS.ComboQueue.Num()) UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO queue dropped (%d left)"), GS.ComboQueue.Num());
		GS.ComboQueue.Reset(); // b120
		Reopen();''')
# Tick: work the combo queue
ed1(C, '''			W.bMenu = false; W.Mode = 0; W.Hover = -1; GShowing = false; // b95c: don't leave the CPU's menu drawn over a turn that moved on
		}
		AZoogiBall* B = Human(G);''', '''			W.bMenu = false; W.Mode = 0; W.Hover = -1; GShowing = false; // b95c: don't leave the CPU's menu drawn over a turn that moved on
		}
		AZoogiBall* B = Human(G);
		if (GS.ComboQueue.Num())
		{	// b120 COMBO: one skill every 0.12 s; a target pick holds the queue until FIRE
			if (!B || G->TurnCounter != GS.ComboTurn || G->Phase != EZoogiPhase::Aiming) GS.ComboQueue.Reset();
			else if (!ZoogiSkills::Picking() && Now >= GS.ComboNextAt)
			{
				const FName Id = GS.ComboQueue[0]; GS.ComboQueue.RemoveAt(0);
				const UZoogiCardSubsystem* CSc = UZoogiCardSubsystem::Get(G);
				const FZoogiCardPlayerState* PSc = CSc ? CSc->Rules().Player(B->PlayerIndex) : nullptr;
				int32 Slot = INDEX_NONE;
				if (PSc) for (int32 k = 0; k < PSc->Hand.Num(); ++k) if (PSc->Hand[k].CardId == Id) { Slot = k; break; }
				if (Slot != INDEX_NONE && G->CardsSlotPlayableNow(Slot)) { G->MenuAction(TEXT("CardPlay"), Slot); UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO plays %s (slot %d, %d left)"), *Id.ToString(), Slot, GS.ComboQueue.Num()); }
				else UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO skips %s (not playable now)"), *Id.ToString());
				GS.ComboNextAt = Now + 0.12;
			}
		}''')
# ---- pause orbit
open(R + 'Public/ZoogiPauseOrbit.h', 'w', newline='\r\n').write('''#pragma once
// b120 (Evan: "during gameplay pauses player can rotate the camera around the player then smoothly transitions back to normal play"):
// while the world is paused (pause menu, anime freeze-frames) a one-finger drag orbits the view around the human's Zoogi; when play
// resumes the match camera eases the orbit back out. -ZoogiNoPauseOrbit = off.
#include "CoreMinimal.h"

class UWorld;

namespace ZoogiPauseOrbit
{
	/** registers the drag reader + the paused-world ticker once (cheap to call every frame) */
	ZOOGIROLL_API void Ensure();
	/** match camera (unpaused): rotates the computed view around the human's Zoogi by what is left of the orbit, easing it to zero */
	ZOOGIROLL_API void Apply(UWorld* World, FVector& Loc, FRotator& Rot, float Dt);
}
''')
open(R + 'Private/ZoogiPauseOrbit.cpp', 'w', newline='\r\n').write('''// b120 pause orbit - see ZoogiPauseOrbit.h
#include "ZoogiPauseOrbit.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiSkillLab.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace
{
	float GOrbYaw = 0.f, GOrbPitch = 0.f;
	FVector2D GOrbPending = FVector2D::ZeroVector;
	bool GOrbDown = false, GOrbDragging = false;
	FVector2D GOrbPress = FVector2D::ZeroVector, GOrbLast = FVector2D::ZeroVector;
	TWeakObjectPtr<AActor> GOrbVT;
	FTransform GOrbBase;
	bool GOrbWasPaused = false;

	bool OrbOff() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPauseOrbit")); return b; }
	UWorld* OrbWorld() { return (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr; }
	AZoogiBall* OrbHuman(UWorld* W)
	{
		AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		if (!G || G->IsRoyaleMatch() || ZoogiSkillLab::Open()) return nullptr;
		AZoogiBall* Act = G->GetActiveBall();
		if (Act && !Act->bCPU && !Act->bOut) return Act; // the turn player if it is the human
		for (AZoogiBall* B : G->Balls) if (B && !B->bCPU && !B->bOut) return B;
		return nullptr;
	}
	bool PausedOk(UWorld* W) { return !OrbOff() && W && W->IsPaused() && OrbHuman(W) != nullptr; }
	/** the whole camera rig turned around the pivot: yaw about the vertical, height about the view's right axis */
	void OrbitRig(const FVector& Pivot, FVector& Loc, FRotator& Rot, float Yaw, float Pitch)
	{
		const FVector Right = FRotationMatrix(FRotator(0.f, Rot.Yaw, 0.f)).GetScaledAxis(EAxis::Y);
		const FQuat Q = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw)) * FQuat(Right, FMath::DegreesToRadians(Pitch));
		const float Roll = Rot.Roll;
		Loc = Pivot + Q.RotateVector(Loc - Pivot);
		Loc.Z = FMath::Max(Loc.Z, Pivot.Z + 40.f);
		Rot = (Q * Rot.Quaternion()).Rotator();
		Rot.Roll = Roll;
	}

	class FOrbitInput : public IInputProcessor
	{
	public:
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& E) override
		{
			if (PausedOk(OrbWorld()) && (E.IsTouchEvent() || E.GetEffectingButton() == EKeys::LeftMouseButton))
			{ GOrbDown = true; GOrbDragging = false; GOrbPress = GOrbLast = E.GetScreenSpacePosition(); }
			return false; // never consumes: buttons under the finger still work
		}
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& E) override
		{
			if (!GOrbDown) return false;
			const FVector2D P = E.GetScreenSpacePosition();
			if (!GOrbDragging && FVector2D::Distance(P, GOrbPress) > 14.f) GOrbDragging = true; // a tap is not a drag
			if (GOrbDragging) GOrbPending += P - GOrbLast;
			GOrbLast = P;
			return false;
		}
		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { GOrbDown = GOrbDragging = false; return false; }
		virtual const TCHAR* GetDebugName() const override { return TEXT("ZoogiPauseOrbit"); }
	};

	bool OrbTick(float Dt)
	{
		UWorld* W = OrbWorld();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		AZoogiBall* Hb = OrbHuman(W);
		if (PausedOk(W) && PC && Hb)
		{
			AActor* VT = PC->GetViewTarget();
			if (!GOrbWasPaused || VT != GOrbVT.Get())
			{
				GOrbWasPaused = true; GOrbVT = VT; GOrbPending = FVector2D::ZeroVector;
				if (VT) GOrbBase = VT->GetActorTransform();
				// the view at the pause already carries the orbit left over from the last one: take it out of the base
				if (VT && (GOrbYaw != 0.f || GOrbPitch != 0.f)) { FVector L = GOrbBase.GetLocation(); FRotator Rr = GOrbBase.Rotator(); OrbitRig(Hb->BallLocation(), L, Rr, 0.f, -GOrbPitch); OrbitRig(Hb->BallLocation(), L, Rr, -GOrbYaw, 0.f); GOrbBase = FTransform(Rr, L); }
			}
			if (!GOrbPending.IsZero())
			{
				FIntPoint Vp(1920, 1080); if (GEngine->GameViewport && GEngine->GameViewport->Viewport) Vp = GEngine->GameViewport->Viewport->GetSizeXY();
				const float K = 300.f / FMath::Max(1, FMath::Min(Vp.X, Vp.Y)); // ~ one screen width = a full turn
				GOrbYaw = FMath::Fmod(GOrbYaw + GOrbPending.X * K, 360.f);
				GOrbPitch = FMath::Clamp(GOrbPitch + GOrbPending.Y * K * 0.5f, -15.f, 40.f);
				GOrbPending = FVector2D::ZeroVector;
			}
			if (ACameraActor* CamA = Cast<ACameraActor>(VT))
			{
				FVector L = GOrbBase.GetLocation(); FRotator Rr = GOrbBase.Rotator();
				OrbitRig(Hb->BallLocation(), L, Rr, GOrbYaw, GOrbPitch);
				CamA->SetActorLocationAndRotation(L, Rr);
				if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(FMath::Min(Dt, 0.05f)); // the camera manager does not tick while paused
			}
		}
		else if (GOrbWasPaused)
		{
			GOrbWasPaused = false; GOrbDown = GOrbDragging = false;
			if (GOrbYaw > 180.f) GOrbYaw -= 360.f; else if (GOrbYaw < -180.f) GOrbYaw += 360.f; // ease back the short way round
			if (GOrbYaw != 0.f || GOrbPitch != 0.f) UE_LOG(LogZoogi, Log, TEXT("PAUSE ORBIT resume: easing back from yaw %.0f / height %.0f"), GOrbYaw, GOrbPitch);
		}
		return true;
	}
}

void ZoogiPauseOrbit::Ensure()
{
	static bool bDone = false;
	if (bDone || OrbOff() || !FSlateApplication::IsInitialized()) return;
	bDone = true;
	static TSharedPtr<FOrbitInput> GIn = MakeShared<FOrbitInput>();
	FSlateApplication::Get().RegisterInputPreProcessor(GIn);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&OrbTick), 0.f);
	UE_LOG(LogZoogi, Log, TEXT("PAUSE ORBIT on: drag while paused to turn the camera round your Zoogi"));
}

void ZoogiPauseOrbit::Apply(UWorld* World, FVector& Loc, FRotator& Rot, float Dt)
{
	if (GOrbWasPaused || (GOrbYaw == 0.f && GOrbPitch == 0.f)) return;
	AZoogiBall* Hb = OrbHuman(World);
	if (!Hb) { GOrbYaw = GOrbPitch = 0.f; return; }
	if (GOrbYaw > 180.f) GOrbYaw -= 360.f; else if (GOrbYaw < -180.f) GOrbYaw += 360.f;
	OrbitRig(Hb->BallLocation(), Loc, Rot, GOrbYaw, GOrbPitch);
	GOrbYaw = FMath::FInterpTo(GOrbYaw, 0.f, Dt, 4.5f); GOrbPitch = FMath::FInterpTo(GOrbPitch, 0.f, Dt, 4.5f); // ~0.7 s back to the normal view
	if (FMath::Abs(GOrbYaw) < 0.05f && FMath::Abs(GOrbPitch) < 0.05f) GOrbYaw = GOrbPitch = 0.f;
}
''')
P = 'Private/ZoogiPlayerController.cpp'
ed1(P, '''#include "ZoogiArcCam.h"''', '''#include "ZoogiArcCam.h"
#include "ZoogiPauseOrbit.h"''')
ed1(P, '''	if (PadPhase > 0) { PadFromLoc = Loc; PadFromRot = Rot; PadFromFov = FinalFov; }
	Cam->SetActorLocation(Loc);''', '''	if (PadPhase > 0) { PadFromLoc = Loc; PadFromRot = Rot; PadFromFov = FinalFov; }
	ZoogiPauseOrbit::Ensure(); ZoogiPauseOrbit::Apply(GetWorld(), Loc, Rot, Dt); // b120: a pause orbit eases back into the match camera
	Cam->SetActorLocation(Loc);''')
print('b120e ok')
