# b120f (Evan 18:53, REDIRECT pick screenshot: "I'm not talking about the pause menu or an anime freeze-frame, forget that idea";
# 18:54: "Guard create a force field around player for 10 s"):
#  1) ZoogiPauseOrbit reworked: the in-play pick pauses (REDIRECT - tap a glowing target, the superpower bullet-time target pick) let a
#     drag turn the camera round the turn player's Zoogi; when the pick ends the match camera eases back (~0.7 s). The pause-menu /
#     anime-freeze version is gone. Those picks now choose on RELEASE (a tap), so a drag never picks a target.
#  2) GUARD: a force field goes up around the player right away and lasts 10 s (world time), then drops. It bumps rivals / orbs /
#     wolves away; the player is pinned while it stands, except during their own roll (it rides along and only bumps rival Zoogis).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
open(R + 'Public/ZoogiPauseOrbit.h', 'w', newline='\r\n').write('''#pragma once
// b120 (Evan, REDIRECT pick screenshot: "during gameplay pauses player can rotate the camera around the player then smoothly
// transitions back to normal play"): while an in-play pick pauses the game (REDIRECT - tap a glowing target, the superpower bullet-time
// target pick) a drag turns the camera round the turn player's Zoogi; when the pick ends the match camera eases back. Those picks choose
// on release, so a drag never picks. -ZoogiNoPauseOrbit = off.
#include "CoreMinimal.h"

class UWorld;

namespace ZoogiPauseOrbit
{
	/** registers the drag reader once (cheap to call every frame) */
	ZOOGIROLL_API void Ensure();
	/** true while an in-play pick pause allows the orbit */
	ZOOGIROLL_API bool Active(UWorld* World);
	/** the current / last press moved far enough to be a drag (a pick ignores its release) */
	ZOOGIROLL_API bool WasDrag();
	/** match camera: turns the computed view round the turn player's Zoogi by the orbit; eases it back to zero once the pick ends */
	ZOOGIROLL_API void Apply(UWorld* World, FVector& Loc, FRotator& Rot, float Dt);
}
''')
open(R + 'Private/ZoogiPauseOrbit.cpp', 'w', newline='\r\n').write('''// b120 pick-pause camera orbit - see ZoogiPauseOrbit.h
#include "ZoogiPauseOrbit.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRedirect.h"
#include "ZoogiActionFun.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

namespace
{
	float GOrbYaw = 0.f, GOrbPitch = 0.f;
	FVector2D GOrbPending = FVector2D::ZeroVector;
	bool GOrbDown = false, GOrbDragging = false, GOrbWasDrag = false, GOrbActiveSeen = false;
	FVector2D GOrbPress = FVector2D::ZeroVector, GOrbLast = FVector2D::ZeroVector;

	bool OrbOff() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPauseOrbit")); return b; }
	UWorld* OrbWorld() { return (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr; }
	AZoogiBall* OrbBall(UWorld* W)
	{
		AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return (B && !B->bCPU) ? B : nullptr;
	}
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
			GOrbWasDrag = false;
			if (ZoogiPauseOrbit::Active(OrbWorld()) && (E.IsTouchEvent() || E.GetEffectingButton() == EKeys::LeftMouseButton))
			{ GOrbDown = true; GOrbDragging = false; GOrbPress = GOrbLast = E.GetScreenSpacePosition(); }
			return false; // never consumes
		}
		virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& E) override
		{
			if (!GOrbDown) return false;
			const FVector2D P = E.GetScreenSpacePosition();
			if (!GOrbDragging && FVector2D::Distance(P, GOrbPress) > 18.f) { GOrbDragging = true; GOrbWasDrag = true; } // a tap is not a drag
			if (GOrbDragging) GOrbPending += P - GOrbLast;
			GOrbLast = P;
			return false;
		}
		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { GOrbDown = GOrbDragging = false; return false; }
		virtual const TCHAR* GetDebugName() const override { return TEXT("ZoogiPauseOrbit"); }
	};
}

void ZoogiPauseOrbit::Ensure()
{
	static bool bDone = false;
	if (bDone || OrbOff() || !FSlateApplication::IsInitialized()) return;
	bDone = true;
	static TSharedPtr<FOrbitInput> GIn = MakeShared<FOrbitInput>();
	FSlateApplication::Get().RegisterInputPreProcessor(GIn);
	UE_LOG(LogZoogi, Log, TEXT("PICK ORBIT on: drag during a REDIRECT / superpower target pick to turn the camera"));
}

bool ZoogiPauseOrbit::Active(UWorld* World)
{
	if (OrbOff() || !World || !OrbBall(World)) return false;
	if (ZoogiRedirect::Picking()) return true;
	if (const UZoogiFunSys* F = UZoogiFunSys::Get(World)) if (F->UltAimOn()) return true;
	return false;
}

bool ZoogiPauseOrbit::WasDrag() { return GOrbWasDrag; }

void ZoogiPauseOrbit::Apply(UWorld* World, FVector& Loc, FRotator& Rot, float Dt)
{
	const bool bOn = Active(World);
	AZoogiBall* Hb = OrbBall(World);
	if (bOn && !GOrbPending.IsZero())
	{
		FIntPoint Vp(1920, 1080); if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) Vp = GEngine->GameViewport->Viewport->GetSizeXY();
		const float K = 300.f / FMath::Max(1, FMath::Min(Vp.X, Vp.Y)); // ~ one screen width = a full turn
		GOrbYaw = FMath::Fmod(GOrbYaw + GOrbPending.X * K, 360.f);
		GOrbPitch = FMath::Clamp(GOrbPitch + GOrbPending.Y * K * 0.5f, -15.f, 40.f);
		if (!GOrbActiveSeen) UE_LOG(LogZoogi, Log, TEXT("PICK ORBIT: camera turning round P%d"), Hb ? Hb->PlayerIndex + 1 : 0);
		GOrbActiveSeen = true;
	}
	GOrbPending = FVector2D::ZeroVector;
	if (GOrbYaw == 0.f && GOrbPitch == 0.f) { GOrbActiveSeen = false; return; }
	if (!Hb) { GOrbYaw = GOrbPitch = 0.f; return; }
	if (GOrbYaw > 180.f) GOrbYaw -= 360.f; else if (GOrbYaw < -180.f) GOrbYaw += 360.f; // the short way round
	OrbitRig(Hb->BallLocation(), Loc, Rot, GOrbYaw, GOrbPitch);
	if (!bOn)
	{	// the pick is over: ease back into normal play (~0.7 s)
		GOrbYaw = FMath::FInterpTo(GOrbYaw, 0.f, Dt, 4.5f); GOrbPitch = FMath::FInterpTo(GOrbPitch, 0.f, Dt, 4.5f);
		if (FMath::Abs(GOrbYaw) < 0.05f && FMath::Abs(GOrbPitch) < 0.05f) GOrbYaw = GOrbPitch = 0.f;
	}
}
''')
# FunSys: UltAimOn + pick on release
FH = 'Public/ZoogiActionFun.h'; FC = 'Private/ZoogiActionFun.cpp'
ed1(FH, '''	bool OnTap(const FVector2D& Screen = FVector2D(-1.f, -1.f));''', '''	bool OnTap(const FVector2D& Screen = FVector2D(-1.f, -1.f));
	/** b120: the superpower bullet-time target pick is open / a release (tap, not a drag) picks there */
	bool UltAimOn() const { return bUltAim; }
	void UltAimRelease(const FVector2D& Screen);''')
# replace the press-pick block with a pass-through (the pick moves to UltAimRelease)
_b = open(R + FC, 'rb').read()
_i = _b.index(b'\tif (bUltAim) // build 25: bullet-time target pick')
_j = _b.index(b'\tif (Rep.bOn)', _i)
_nl = b'\r\n' if b'\r\n' in _b[_i:_j] else b'\n'
_b = _b[:_i] + b'\tif (bUltAim) return true; // b120: the bullet-time pick happens on release (UltAimRelease) so a drag can turn the camera' + _nl + _b[_j:]
open(R + FC, 'wb').write(_b)
ed1(FC, '''bool UZoogiFunSys::OnTap(const FVector2D& Screen)
{''', '''void UZoogiFunSys::UltAimRelease(const FVector2D& Screen)
{	// b120: build 25 bullet-time target pick, on release
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!bUltAim || !G) return;
	AZoogiBall* B = G->GetActiveBall();
	APlayerController* PC = W->GetFirstPlayerController();
	if (!B || !PC || Screen.X < 0.f) return;
	AActor* PickA = nullptr; float BestD = 1e9f;
	for (AActor* T : G->LiveTargets(B))
	{
		FVector2D S;
		const FVector TL = Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation();
		if (!PC->ProjectWorldLocationToScreen(TL, S, false)) continue;
		const float D = FVector2D::Distance(S, Screen);
		if (D < BestD) { BestD = D; PickA = T; }
	}
	int32 Vx = 0, Vy = 0; PC->GetViewportSize(Vx, Vy);
	if (PickA && BestD < 0.12f * FMath::Max(1, FMath::Min(Vx, Vy)) + 40.f) { bUltAim = false; FireUltimate(G, B, UltPower, PickA); }
}

bool UZoogiFunSys::OnTap(const FVector2D& Screen)
{''')
# controller: release -> ult pick
P = 'Private/ZoogiPlayerController.cpp'
ed1(P, '''	if (!bFunTap && bPtrPress && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick())))
		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) bFunTap = Fun->OnTap(Ptr);''', '''	if (!bFunTap && bPtrPress && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick())))
		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) bFunTap = Fun->OnTap(Ptr);
	if (bPtrRelease && !ZoogiPauseOrbit::WasDrag() && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick()))) // b120: the bullet-time pick on release (a drag turns the camera)
		if (UZoogiFunSys* FunR = UZoogiFunSys::Get(this)) if (FunR->UltAimOn()) FunR->UltAimRelease(Ptr);''')
# redirect: pick on release
RD = 'Private/ZoogiRedirect.cpp'
ed1(RD, '''		if (!bPick || bCpuPick || !bPress || !G || !PC) return;''', '''		// b120: picks on release (a tap) - a drag turns the camera round the Zoogi instead (ZoogiPauseOrbit)
		if (!bPick || bCpuPick || !bRelease || ZoogiPauseOrbit::WasDrag() || !G || !PC) return;''')
b = open(R + RD, 'rb').read()
nl = b'\r\n' if b'\r\n' in b else b'\n'
if b'#include "ZoogiPauseOrbit.h"' not in b:
    i = b.index(b'#include "ZoogiRedirect.h"') ; j = b.index(nl, i) + len(nl)
    b = b[:j] + b'#include "ZoogiPauseOrbit.h"' + nl + b[j:]; open(R + RD, 'wb').write(b)
# GUARD: 10 s force field, up right away
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''struct FGuardDome { int32 ArmTurn = -1; bool bUp = false; int32 UpTurn = -1;''', '''struct FGuardDome { int32 ArmTurn = -1; bool bUp = false; int32 UpTurn = -1; float UpAt = 0.f;''')
ed1(S, '''			if (!D.bUp && G->TurnCounter > D.ArmTurn && G->ActivePlayer != Kv.Key)
			{	// the guarding Zoogi's turn is over: up goes the barrier
				D.bUp = true; D.UpTurn = G->TurnCounter; D.Ball = B; D.Anchor = B->BallLocation();''', '''			static const bool bOldGuardTiming = FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuardTurnBarrier"));
			if (!D.bUp && (bOldGuardTiming ? (G->TurnCounter > D.ArmTurn && G->ActivePlayer != Kv.Key) : true))
			{	// b120 (Evan: "Guard create a force field around player for 10 s"): up at once (b83: after the guarding Zoogi's turn)
				D.bUp = true; D.UpTurn = G->TurnCounter; D.UpAt = G->GetWorld()->GetTimeSeconds(); D.Ball = B; D.Anchor = B->BallLocation();''')
ed1(S, '''				G->PublicPopup(TEXT("GUARD BARRIER UP"), B->BallLocation() + FVector(0, 0, 170.f), FLinearColor(0.55f, 0.85f, 1.f));''', '''				G->PublicPopup(bOldGuardTiming ? TEXT("GUARD BARRIER UP") : TEXT("FORCE FIELD - 10 S"), B->BallLocation() + FVector(0, 0, 170.f), FLinearColor(0.55f, 0.85f, 1.f));''')
ed1(S, '''				if (!GuardBreakable() && B->Sphere && B->Sphere->IsSimulatingPhysics())
				{	// b87: stationary - pinned to the spot it was guarding (nothing rolls it or launches it away)''', '''				// b120: during the guarding player's own roll the field rides along (no pin) and only bumps rival Zoogis
				const bool bOwnRoll = !bOldGuardTiming && G->ActivePlayer == Kv.Key && G->Phase == EZoogiPhase::Rolling;
				if (bOwnRoll) D.Anchor = B->BallLocation();
				if (!bOwnRoll && !GuardBreakable() && B->Sphere && B->Sphere->IsSimulatingPhysics())
				{	// b87: stationary - pinned to the spot it was guarding (nothing rolls it or launches it away)''')
ed1(S, '''					for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Bump(O, O->Sphere, ZoogiConst::OrbRadius, true);
					for (AZoogiClone* Cl : G->Clones) if (Cl && Cl->Sphere) Bump(Cl, Cl->Sphere, Cl->Sphere->GetScaledSphereRadius(), false);''', '''					if (!bOwnRoll) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Bump(O, O->Sphere, ZoogiConst::OrbRadius, true);
					if (!bOwnRoll) for (AZoogiClone* Cl : G->Clones) if (Cl && Cl->Sphere) Bump(Cl, Cl->Sphere, Cl->Sphere->GetScaledSphereRadius(), false);''')
ed1(S, '''				if (G->ActivePlayer == Kv.Key && G->TurnCounter > D.UpTurn) Down.Add(Kv.Key); // its next turn: the barrier drops''', '''				if (bOldGuardTiming) { if (G->ActivePlayer == Kv.Key && G->TurnCounter > D.UpTurn) Down.Add(Kv.Key); } // its next turn: the barrier drops
				else
				{	// b120: 10 s, the last 2 s it flickers
					const float Left = 10.f - (G->GetWorld()->GetTimeSeconds() - D.UpAt);
					if (AActor* Fa = D.A.Get()) Fa->SetActorHiddenInGame(Left < 2.f && FMath::Fmod(Left, 0.25f) < 0.08f);
					if (Left <= 0.f) Down.Add(Kv.Key);
				}''')
print('b120f ok')
