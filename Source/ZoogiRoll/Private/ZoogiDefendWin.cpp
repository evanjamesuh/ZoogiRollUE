// b106 (Evan: "for duel bring back the view of the two marbles - the cam slows as the turn player heads toward the rival; in that
// time the rival can activate DEFEND, the turn player can not because it is in mid flight" + "pure physics hit"):
// classic matches - no duel menus. A shot about to hit a rival slows the world under a side-on camera framing both marbles.
// b107b (Evan: "you dont see the tap mini game anymore - at that moment if the circle mini game is good the black and white effect
// enables for 0.7 s, the turn player gets options to select a target around or direct to the opponent, and the rival has a chance to
// select GUARD and a holographic shield appears if GUARD is tapped at or before the hit; the loser's 3D model still flies back and the
// smash animation plays"):
//   stage 0  the timing ring closes on the turn player's marble - tap when they meet (the CPU taps too)
//   stage 1  GOOD / PERFECT: the world all but stops, 0.7 s black and white, DIRECT (harder straight hit) or AROUND (the shot bends to
//            the nearest other rival / orb); no pick in 3 s = DIRECT
//   stage 2  the flight goes on (MISS goes straight here)
//   any time before contact the rival may GUARD (2 SP): the holographic guard dome forms and it turns heavy through the hit
//   contact  pure physics; the loser (the rival, or the attacker bouncing off a guard) flies back in its 3D form and splats
// Story missions keep the duel scene. -ZoogiNoDefendWindow = off, -ZoogiDefendNoRing = the b106 window, -ZoogiDefendNoFlung = no form.
#include "ZoogiActionFun.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiStory.h"
#include "ZoogiEpic.h"
#include "ZoogiPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);

namespace
{
	bool DwNoRing() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDefendNoRing")); return b; }
	bool DwNoFlung() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDefendNoFlung")); return b; }
	int32 DwGrade(float Off) { const float D = FMath::Abs(Off); return D <= 0.11f ? 2 : (D <= 0.26f ? 1 : 0); }
	const TCHAR* DwGradeName(int32 Gr) { return Gr == 2 ? TEXT("PERFECT!") : Gr == 1 ? TEXT("GOOD!") : TEXT("MISS"); }
	constexpr float DwRingT = 1.05f;   // real seconds from the window opening until the ring meets the circle
	constexpr float DwBwTime = 0.7f;   // Evan: black and white for 0.7 s
	constexpr float DwChoiceMax = 3.f; // no pick = DIRECT
}

bool UZoogiFunSys::ClassicDefendMode(const AZoogiGameMode* G)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDefendWindow"));
	return G && !bOff && !ZoogiStory::InMission() && !G->bRoyaleMode && !G->bComboMode;
}

float UZoogiFunSys::DefWinDilation() const
{
	if (DwNoRing()) return 0.12f;
	return DefWin.Stage == 0 ? 0.06f : (DefWin.Stage == 1 ? 0.004f : 0.15f);
}

void UZoogiFunSys::DefWinBW(bool bOn)
{
	ACameraActor* C = Cast<ACameraActor>(DefWin.Cam.Get());
	if (!C || !C->GetCameraComponent()) return;
	FPostProcessSettings& PP = C->GetCameraComponent()->PostProcessSettings;
	PP.bOverride_ColorSaturation = bOn; PP.ColorSaturation = bOn ? FVector4(0.f, 0.f, 0.f, 1.f) : FVector4(1.f, 1.f, 1.f, 1.f);
	PP.bOverride_ColorContrast = bOn; PP.ColorContrast = bOn ? FVector4(1.25f, 1.25f, 1.25f, 1.f) : FVector4(1.f, 1.f, 1.f, 1.f);
	C->GetCameraComponent()->PostProcessBlendWeight = 1.f;
	DefWin.bBw = bOn;
}

AActor* UZoogiFunSys::DefWinAroundTarget(AZoogiGameMode* G) const
{
	const AZoogiBall* A = DefWin.A.Get(); const AZoogiBall* R = DefWin.R.Get();
	if (!G || !A) return nullptr;
	AActor* Pick = nullptr; float PickD = 1600.f;
	for (AZoogiBall* O : G->Balls)
	{
		if (!O || O == A || O == R || O->bOut || G->IsAlly(O, A)) continue;
		const float D = FVector::Dist2D(O->BallLocation(), A->BallLocation()) - 300.f; // a rival beats an orb at the same range
		if (D < PickD) { PickD = D; Pick = O; }
	}
	for (AZoogiOrb* O : G->Orbs)
	{
		if (!O || O->IsHidden()) continue;
		const float D = FVector::Dist2D(O->GetActorLocation(), A->BallLocation());
		if (D < PickD && D > 120.f) { PickD = D; Pick = O; }
	}
	return Pick;
}

void UZoogiFunSys::TickDefendWindow(AZoogiGameMode* G, float RealDt)
{
	if (!G) return;
	if (!ClassicDefendMode(G)) { if (DefWin.bOn) EndDefendWindow(G, TEXT("mode")); return; }
	// b119 (Evan: "remove the slow motion circle game, let gameplay play normal"): the window is off (classic still skips the
	// duel scene: ClassicDefendMode stays true). -ZoogiDefendWindow = the b107 window back.
	static const bool bWindowOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDefendWindow"));
	if (!bWindowOn) { if (DefWin.bOn) EndDefendWindow(G, TEXT("off")); return; }
	UWorld* W = GetWorld();
	const double Now = FPlatformTime::Seconds();
	const float Rad = ZoogiConst::ZoogiRadius;
	if (!DefWin.bOn)
	{
		if (G->Phase != EZoogiPhase::Rolling || bDefWinThisShot || Clash.bOn || Rep.bOn) return;
		AZoogiBall* A = G->GetActiveBall();
		if (!A || A->bOut || !A->Sphere) return;
		const int32 Si = A->PlayerIndex;
		static const bool bClashTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTest"));
		const bool bPowered = bUltThisShot || G->bPowerUsed || (Players.IsValidIndex(Si) && (Players[Si].bFire || Players[Si].bAwake));
		const FVector Va = A->GetBallVelocity();
		AZoogiBall* Tgt = nullptr; float TgtT = 1e9f;
		for (AZoogiBall* R : G->Balls)
		{
			if (!R || R == A || R->bOut || G->IsAlly(R, A)) continue;
			// b107b: a powered shot always gets the mini game; otherwise only when the rival could GUARD (2 SP)
			if (!bClashTest && !bPowered && DuelSp(G, R->PlayerIndex) < 2) continue;
			FVector D = R->BallLocation() - A->BallLocation(); D.Z = 0.f;
			FVector Vr = Va - R->GetBallVelocity(); Vr.Z = 0.f;
			const float Dist = D.Size(); const float Closing = FVector::DotProduct(Vr, D.GetSafeNormal());
			if (Closing < 450.f || Dist < 2.f * Rad + 10.f) continue;
			const float Miss = FMath::Abs(D.X * Vr.Y - D.Y * Vr.X) / FMath::Max(1.f, Vr.Size()); // closest-approach distance
			const float TimeTo = (Dist - 2.f * Rad) / Closing;
			if (Miss < 1.7f * Rad && TimeTo < 0.5f && TimeTo < TgtT) { TgtT = TimeTo; Tgt = R; }
		}
		if (!Tgt) return;
		DefWin = FDefWin(); DefWin.bOn = true; DefWin.A = A; DefWin.R = Tgt; DefWin.T0 = Now; DefWin.GameT0 = W ? W->GetTimeSeconds() : 0.0; DefWin.GameLimit = TgtT * 1.8f + 0.3f; bDefWinThisShot = true;
		DefWin.Stage = DwNoRing() ? 2 : 0;
		const bool bHumanR = !Tgt->bCPU;
		const int32 Sp = DuelSp(G, Tgt->PlayerIndex);
		if (!bHumanR) DefWin.CpuAt = (Sp >= 2 && FMath::FRand() < 0.65f) ? FMath::FRandRange(0.5f, DwRingT + 1.6f) : -1.f;
		if (A->bCPU) DefWin.CpuTapAt = DwRingT + FMath::FRandRange(-0.3f, 0.32f); // a person's spread: about half land GOOD or better
		if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACameraActor* C = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP))
			{
				C->GetCameraComponent()->SetFieldOfView(62.f); C->GetCameraComponent()->bConstrainAspectRatio = false;
				DefWin.Cam = C; DefWin.bCamPlaced = false;
				PC->SetViewTargetWithBlend(C, 0.12f, VTBlend_EaseOut);
			}
		}
		ZoogiEpic::Hit(G, ZoogiEpic::EHit::Riser);
		if (bHumanR) Callout(TEXT("GUARD?"), FString::Printf(TEXT("TAP GUARD BEFORE THE HIT  -  2 SP (YOU HAVE %d)"), Sp), FLinearColor(0.45f, 0.85f, 1.f), 1.6f, 1.f, Tgt->PlayerIndex);
		else if (!A->bCPU) Callout(TEXT("TIMING!"), TEXT("TAP WHEN THE RING MEETS THE CIRCLE"), FLinearColor(1.f, 0.85f, 0.25f), 1.4f, 1.f, A->PlayerIndex);
		else Callout(TEXT("INCOMING!"), FString::Printf(TEXT("%s CAN GUARD (%d SP)"), *Tgt->Def.Name.ToUpper(), Sp), FLinearColor(0.45f, 0.85f, 1.f), 1.4f, 1.f, Tgt->PlayerIndex);
		UE_LOG(LogZoogi, Log, TEXT("DEFEND window: P%d -> P%d in %.2f s (rival SP %d, %s; ring %s)"), A->PlayerIndex + 1, Tgt->PlayerIndex + 1, TgtT, Sp, bHumanR ? TEXT("person") : (DefWin.CpuAt > 0.f ? TEXT("CPU will guard") : TEXT("CPU takes it")), A->bCPU ? TEXT("CPU taps") : TEXT("person taps"));
		return;
	}
	AZoogiBall* A = DefWin.A.Get(); AZoogiBall* R = DefWin.R.Get();
	if (!A || !R || A->bOut || R->bOut || G->Phase != EZoogiPhase::Rolling) { EndDefendWindow(G, TEXT("gone")); return; }
	const float Age = (float)(Now - DefWin.T0);
	// the two-marble camera: side-on to the line between them, both in frame
	if (ACameraActor* C = Cast<ACameraActor>(DefWin.Cam.Get()))
	{
		const FVector Pa = A->BallLocation(), Pr = R->BallLocation();
		FVector LineDir = Pr - Pa; LineDir.Z = 0.f; const float Gap = LineDir.Size(); if (!LineDir.Normalize()) LineDir = FVector::ForwardVector;
		if (!DefWin.bCamPlaced) { DefWin.CamSide = FVector::CrossProduct(FVector::UpVector, LineDir); DefWin.bCamPlaced = true; }
		const FVector Mid = (Pa + Pr) * 0.5f;
		const float Dd = FMath::Max(520.f, Gap * 1.25f + 380.f);
		const FVector Eye = Mid + DefWin.CamSide * Dd + FVector(0, 0, Dd * 0.42f) - LineDir * Dd * 0.18f;
		C->SetActorLocation(FMath::VInterpTo(C->GetActorLocation().IsNearlyZero() ? Eye : C->GetActorLocation(), Eye, RealDt, 10.f));
		C->SetActorRotation((Mid + FVector(0, 0, 20.f) - C->GetActorLocation()).Rotation());
	}
	if (!DefWin.bDecided && !DefWin.bContact && DefWin.CpuAt > 0.f && Age >= DefWin.CpuAt) DefendNow(G);
	// stage 0: the timing ring
	if (DefWin.Stage == 0)
	{
		if (DefWin.CpuTapAt > 0.f && Age >= DefWin.CpuTapAt) DefWinRingTap(G);
		else if (Age > DwRingT + 0.32f) { DefWin.Grade = 0; DefWin.GradeAt = Now; DefWin.Stage = 2; UE_LOG(LogZoogi, Log, TEXT("DEFEND ring: no tap -> MISS")); }
	}
	// stage 1: black and white, DIRECT / AROUND
	if (DefWin.Stage == 1)
	{
		if (DefWin.bBw && Now - DefWin.BwAt >= DwBwTime) DefWinBW(false);
		if (DefWin.CpuChoiceAt > 0.f && Now - DefWin.BwAt >= DefWin.CpuChoiceAt) DefWinChoose(G, (DefWin.bAroundOk && FMath::FRand() < 0.4f) ? 1 : 0);
		else if (Now - DefWin.BwAt > DwChoiceMax) DefWinChoose(G, 0);
		if (!DefWin.bOn) return; // AROUND ends the window
		DefWin.GameT0 += RealDt * DefWinDilation(); // the near-stop does not use up the flight time
	}
	if (!DefWin.bContact)
	{
		const float Dist = FVector::Dist2D(A->BallLocation(), R->BallLocation());
		if (Dist <= 2.f * Rad + 12.f)
		{
			DefWin.bContact = true; DefWin.ContactAt = Now; if (DefWin.bBw) DefWinBW(false);
			if (DefWin.Stage < 2) DefWin.Stage = 2;
			ZoogiEpic::Hit(G, DefWin.bDefend ? ZoogiEpic::EHit::Impact : ZoogiEpic::EHit::Big);
		}
		else if ((W && W->GetTimeSeconds() - DefWin.GameT0 > DefWin.GameLimit) || Age > 14.f || (DefWin.Stage != 1 && A->GetBallVelocity().Size2D() < 150.f)) { EndDefendWindow(G, TEXT("no contact")); return; } // game time: the world is slowed
	}
	else
	{
		if (DefWin.bDefend && R->Sphere && Now - DefWin.ContactAt < 0.05) { const FVector V = R->Sphere->GetPhysicsLinearVelocity(); R->Sphere->SetPhysicsLinearVelocity(V * 0.3f); } // the dome soaks the knock
		if (!DefWin.bLoserDone && Now - DefWin.ContactAt > 0.06)
		{	// the loser flies back in its 3D form (pure physics decides how far)
			DefWin.bLoserDone = true;
			AZoogiBall* L = DefWin.bDefend ? A : R; AZoogiBall* Wn = DefWin.bDefend ? R : A;
			if (L->GetBallVelocity().Size2D() > 220.f) DefWinFlyLoser(G, L, Wn);
			else UE_LOG(LogZoogi, Log, TEXT("DEFEND contact: P%d barely moved (%.0f) - no fly-back"), L->PlayerIndex + 1, L->GetBallVelocity().Size2D());
		}
		if (Now - DefWin.ContactAt > 0.7) EndDefendWindow(G, TEXT("hit"));
	}
}

void UZoogiFunSys::DefWinRingTap(AZoogiGameMode* G)
{
	if (!DefWin.bOn || DefWin.Stage != 0) return;
	const double Now = FPlatformTime::Seconds();
	const float Off = (float)(Now - DefWin.T0) - DwRingT;
	DefWin.Grade = DwGrade(Off); DefWin.GradeAt = Now;
	AZoogiBall* A = DefWin.A.Get();
	ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.9f, DefWin.Grade == 2 ? 1.45f : (DefWin.Grade == 1 ? 1.15f : 0.8f));
	UE_LOG(LogZoogi, Log, TEXT("DEFEND ring: P%d tap %+.2f s -> %s"), A ? A->PlayerIndex + 1 : 0, Off, DwGradeName(DefWin.Grade));
	if (DefWin.Grade <= 0) { DefWin.Stage = 2; return; }
	// GOOD / PERFECT: the world all but stops, 0.7 s black and white, the turn player picks DIRECT or AROUND
	DefWin.Stage = 1; DefWin.BwAt = Now;
	DefWin.bAroundOk = DefWinAroundTarget(G) != nullptr;
	DefWin.CpuChoiceAt = (A && A->bCPU) ? FMath::FRandRange(0.55f, 1.1f) : -1.f;
	DefWinBW(true);
	ZoogiEpic::Hit(G, ZoogiEpic::EHit::Impact);
	ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.8f, 0.6f);
}

void UZoogiFunSys::DefWinChoose(AZoogiGameMode* G, int32 Choice)
{
	if (!DefWin.bOn || DefWin.Stage != 1) return;
	AZoogiBall* A = DefWin.A.Get(); AZoogiBall* R = DefWin.R.Get();
	if (DefWin.bBw) DefWinBW(false);
	DefWin.Stage = 2; DefWin.Choice = Choice;
	if (!A || !A->Sphere || !R) return;
	const FVector V = A->Sphere->GetPhysicsLinearVelocity();
	const float Sp = FMath::Max(700.f, V.Size2D());
	const float Boost = DefWin.Grade == 2 ? 1.45f : 1.25f; // PERFECT hits harder
	AActor* T = Choice == 1 ? DefWinAroundTarget(G) : nullptr;
	if (Choice == 1 && T)
	{
		FVector D = (Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation()) - A->BallLocation(); D.Z = 0.f; D.Normalize();
		A->Sphere->SetPhysicsLinearVelocity(D * Sp * Boost + FVector(0.f, 0.f, V.Z));
		for (int32 k = 1; k <= 7; ++k) AZoogiFX::Spawn(GetWorld(), A->BallLocation() + D * 60.f * k + FVector(0, 0, 25.f), FLinearColor(1.f, 0.85f, 0.3f), 60.f + 8.f * k, 0.3f, false, 10.f);
		Callout(TEXT("AROUND!"), FString::Printf(TEXT("%s BENDS TO %s"), *A->Def.Name.ToUpper(), Cast<AZoogiBall>(T) ? *Cast<AZoogiBall>(T)->Def.Name.ToUpper() : TEXT("AN ORB")), FLinearColor(1.f, 0.85f, 0.3f), 1.1f, 1.05f, A->PlayerIndex);
		ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 1.f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("DEFEND choice: P%d AROUND -> %s"), A->PlayerIndex + 1, *T->GetName());
		EndDefendWindow(G, TEXT("around"));
		return;
	}
	FVector D = R->BallLocation() - A->BallLocation(); D.Z = 0.f; D.Normalize();
	A->Sphere->SetPhysicsLinearVelocity(D * Sp * Boost + FVector(0.f, 0.f, V.Z));
	for (int32 k = 1; k <= 7; ++k) AZoogiFX::Spawn(GetWorld(), A->BallLocation() + D * 60.f * k + FVector(0, 0, 25.f), FLinearColor(1.f, 0.3f, 0.25f), 60.f + 8.f * k, 0.3f, false, 10.f);
	Callout(TEXT("DIRECT!"), FString::Printf(TEXT("%s GOES STRAIGHT AT %s"), *A->Def.Name.ToUpper(), *R->Def.Name.ToUpper()), FLinearColor(1.f, 0.35f, 0.3f), 1.1f, 1.05f, A->PlayerIndex);
	ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 1.f, 0.9f);
	UE_LOG(LogZoogi, Log, TEXT("DEFEND choice: P%d DIRECT x%.2f"), A->PlayerIndex + 1, Boost);
}

void UZoogiFunSys::DefWinFlyLoser(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn)
{
	if (DwNoFlung() || !G || !L || !Wn || L->bOut || Clash.bOn || Flung.Actor.IsValid()) return;
	const int32 F = FormIndex(L);
	if (F < 0) return;
	FVector Ax = Wn->BallLocation() - L->BallLocation(); Ax.Z = 0.f;
	if (!Ax.Normalize()) Ax = -L->GetBallVelocity().GetSafeNormal2D();
	// StartFlung reads the clash fields: side 0 = the loser, the axis points at the winner (the loser flies the other way)
	Clash.Att = L; Clash.Def = Wn; Clash.Axis = Ax; Clash.AxisYaw = Ax.Rotation().Yaw;
	SpawnFighter(0, L, F);
	if (Clash.F[0].Actor.IsValid()) StartFlung(0);
	for (FFighter& Fi : Clash.F) { for (const TWeakObjectPtr<UPrimitiveComponent>& P : Fi.Hidden) if (P.IsValid()) P->SetVisibility(true, false); if (AActor* X = Fi.Actor.Get()) X->Destroy(); Fi = FFighter(); }
	Clash.Att.Reset(); Clash.Def.Reset();
	UE_LOG(LogZoogi, Log, TEXT("DEFEND contact: loser P%d flies back in its form (%s)"), L->PlayerIndex + 1, Flung.Actor.IsValid() ? TEXT("ok") : TEXT("no form"));
}

void UZoogiFunSys::DefWinTap(AZoogiGameMode* G, const FVector2D& Screen)
{
	AZoogiBall* A = DefWin.A.Get(); AZoogiBall* R = DefWin.R.Get();
	if (!A || !R) return;
	const bool bGuardOpen = !R->bCPU && !DefWin.bDecided && !DefWin.bContact;
	if (bGuardOpen && (A->bCPU || DefWin.GuardBtn.IsInside(Screen))) { DefendNow(G); return; } // the rival: GUARD
	if (A->bCPU) return;
	if (DefWin.Stage == 0) { DefWinRingTap(G); return; } // the turn player: the timing ring
	if (DefWin.Stage == 1)
	{
		if (DefWin.Btn[0].bIsValid && DefWin.Btn[0].IsInside(Screen)) DefWinChoose(G, 0);
		else if (DefWin.bAroundOk && DefWin.Btn[1].bIsValid && DefWin.Btn[1].IsInside(Screen)) DefWinChoose(G, 1);
	}
}

void UZoogiFunSys::DrawDefendWindow(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U)
{
	if (!DefWin.bOn || !C || !G) return;
	AZoogiBall* A = DefWin.A.Get(); AZoogiBall* R = DefWin.R.Get();
	if (!A || !R) return;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	const double Now = FPlatformTime::Seconds();
	const float Age = (float)(Now - DefWin.T0);
	FVector2D Cc(Wd * 0.4f, Ht * 0.45f);
	if (PC) { FVector2D S; if (PC->ProjectWorldLocationToScreen(A->BallLocation(), S, true)) Cc = S; }
	const int32 Seg = 48;
	auto RingAt = [&](const FVector2D& Ctr, float Rr, float Th, const FLinearColor& Col)
	{
		for (int32 k = 0; k < Seg; ++k)
		{
			const float a0 = 2.f * PI * k / Seg, a1 = 2.f * PI * (k + 1) / Seg;
			FCanvasLineItem Li(Ctr + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * Rr, Ctr + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * Rr);
			Li.LineThickness = Th; Li.SetColor(Col); C->DrawItem(Li);
		}
	};
	const float R0 = 70.f * U;
	if (DefWin.Stage == 0)
	{
		const float K = FMath::Clamp((DwRingT - Age) / DwRingT, 0.f, 1.f);
		RingAt(Cc, R0, 8.f * U, FLinearColor(1.f, 1.f, 1.f, 0.95f));
		RingAt(Cc, R0 * (1.f + 3.f * K), 10.f * U, K < 0.1f ? FLinearColor(1.f, 0.85f, 0.2f, 1.f) : FLinearColor(1.f, 0.3f, 0.3f, 0.9f));
		ZoogiFunCanvasText(C, A->bCPU ? FString::Printf(TEXT("%s - TIMING..."), *A->Def.Name.ToUpper()) : FString(TEXT("TAP!")), Cc.X, Cc.Y + R0 * (1.f + 3.f * K) + 16.f * U, 40.f * U, FLinearColor(1.f, 0.85f, 0.25f), 1, 0);
	}
	else if (DefWin.Grade >= 0 && Now - DefWin.GradeAt < 0.9)
		ZoogiFunCanvasText(C, DwGradeName(DefWin.Grade), Cc.X, Cc.Y - 60.f * U, 64.f * U, DefWin.Grade == 2 ? FLinearColor(1.f, 0.82f, 0.2f) : FLinearColor::White, 1, 0);
	if (DefWin.Stage == 1)
	{	// DIRECT / AROUND (P5 slanted plates)
		const bool bCpu = A->bCPU;
		const float Bw = FMath::Min(Wd * 0.3f, 380.f * U), Bh = 110.f * U, Y = Ht * 0.78f;
		const FLinearColor Cols[2] = { FLinearColor(0.85f, 0.05f, 0.12f, 0.97f), FLinearColor(0.05f, 0.05f, 0.07f, 0.95f) };
		const TCHAR* Names[2] = { TEXT("DIRECT"), TEXT("AROUND") };
		const FString Subs[2] = { FString::Printf(TEXT("STRAIGHT AT %s"), *R->Def.Name.ToUpper()), DefWin.bAroundOk ? FString(TEXT("BEND TO THE NEXT TARGET")) : FString(TEXT("NOTHING NEAR")) };
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector2D Ctr(Wd * (i == 0 ? 0.32f : 0.68f), Y);
			const float Dim = (i == 1 && !DefWin.bAroundOk) ? 0.35f : 1.f;
			ZoogiFunCanvasQuad(C, Ctr + FVector2D(8.f, 8.f) * U, Bw, Bh, -4.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f * Dim));
			FLinearColor Cq = Cols[i]; Cq.A *= Dim;
			ZoogiFunCanvasQuad(C, Ctr, Bw, Bh, -4.f, 14.f * U, Cq);
			ZoogiFunCanvasText(C, Names[i], Ctr.X, Ctr.Y - Bh * 0.14f, 46.f * U, FLinearColor(1.f, 1.f, 1.f, Dim), 1, 0);
			ZoogiFunCanvasText(C, Subs[i], Ctr.X, Ctr.Y + Bh * 0.3f, 20.f * U, FLinearColor(1.f, 0.85f, 0.35f, Dim), 1, 0);
			DefWin.Btn[i] = bCpu ? FBox2D(ForceInit) : FBox2D(Ctr - FVector2D(Bw, Bh) * 0.55f, Ctr + FVector2D(Bw, Bh) * 0.55f);
		}
		ZoogiFunCanvasText(C, bCpu ? FString::Printf(TEXT("%s IS CHOOSING..."), *A->Def.Name.ToUpper()) : FString::Printf(TEXT("%d"), FMath::Max(1, FMath::CeilToInt(DwChoiceMax - (float)(Now - DefWin.BwAt)))), Wd * 0.5f, Y - Bh * 0.95f, 40.f * U, FLinearColor(1.f, 0.85f, 0.3f), 1, 0);
	}
	// the rival's GUARD button (until it guards or the hit lands)
	if (!R->bCPU && !DefWin.bDecided && !DefWin.bContact)
	{
		const FVector2D Gc(Wd - 170.f * U, Ht - 150.f * U);
		const float Pulse = 1.f + 0.06f * FMath::Sin(Age * 12.f);
		ZoogiFunCanvasQuad(C, Gc + FVector2D(8.f, 8.f) * U, 260.f * U * Pulse, 120.f * U * Pulse, -5.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		ZoogiFunCanvasQuad(C, Gc, 260.f * U * Pulse, 120.f * U * Pulse, -5.f, 14.f * U, FLinearColor(0.1f, 0.55f, 0.95f, 0.95f));
		ZoogiFunCanvasText(C, TEXT("GUARD"), Gc.X, Gc.Y - 18.f * U, 52.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, FString::Printf(TEXT("2 SP  (HAVE %d)"), DuelSp(G, R->PlayerIndex)), Gc.X, Gc.Y + 30.f * U, 22.f * U, FLinearColor(0.8f, 0.95f, 1.f), 1, 0);
		DefWin.GuardBtn = FBox2D(Gc - FVector2D(150.f, 75.f) * U, Gc + FVector2D(150.f, 75.f) * U);
		if (PC) { FVector2D S; if (PC->ProjectWorldLocationToScreen(R->BallLocation(), S, true)) RingAt(S, 55.f * U * Pulse, 5.f * U, FLinearColor(0.45f, 0.85f, 1.f, 0.85f)); }
	}
}

void UZoogiFunSys::DefendNow(AZoogiGameMode* G)
{
	AZoogiBall* R = DefWin.R.Get();
	if (!DefWin.bOn || DefWin.bDecided || DefWin.bContact || !R) return;
	DefWin.bDecided = true;
	if (!DuelSpend(G, R->PlayerIndex, 2)) { Callout(TEXT("NO SP!"), TEXT("NEEDS 2 SP TO GUARD"), FLinearColor(1.f, 0.4f, 0.3f), 1.f, 0.9f, R->PlayerIndex); return; }
	DefWin.bDefend = true;
	if (R->Sphere) { DefWin.SavedMass = R->Sphere->GetMass(); R->Sphere->SetMassOverrideInKg(NAME_None, DefWin.SavedMass * 8.f, true); }
	DuelV2GuardForm(G, R); // the holographic guard dome
	Callout(TEXT("GUARD!"), FString::Printf(TEXT("%s RAISES ITS SHIELD"), *R->Def.Name.ToUpper()), FLinearColor(0.45f, 0.85f, 1.f), 1.2f, 1.05f, R->PlayerIndex);
	ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 1.f, 0.9f);
	UE_LOG(LogZoogi, Log, TEXT("DEFEND: P%d guards (SP left %d)"), R->PlayerIndex + 1, DuelSp(G, R->PlayerIndex));
}

void UZoogiFunSys::EndDefendWindow(AZoogiGameMode* G, const TCHAR* Why)
{
	if (!DefWin.bOn) return;
	if (DefWin.bBw) DefWinBW(false);
	if (AZoogiBall* R = DefWin.R.Get()) if (R->Sphere && DefWin.SavedMass > 0.f) R->Sphere->SetMassOverrideInKg(NAME_None, DefWin.SavedMass, true);
	if (AActor* C = DefWin.Cam.Get())
	{
		if (UWorld* W = GetWorld()) if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(W->GetFirstPlayerController())) PC->RestoreMatchCam(0.25f);
		C->SetLifeSpan(0.6f); // after the blend
	}
	LastDuelEndAt = FPlatformTime::Seconds();
	UE_LOG(LogZoogi, Log, TEXT("DEFEND window over (%s%s, ring %s)"), Why, DefWin.bDefend ? TEXT(", guarded") : TEXT(""), DefWin.Grade < 0 ? TEXT("-") : DwGradeName(DefWin.Grade));
	DefWin = FDefWin();
}
