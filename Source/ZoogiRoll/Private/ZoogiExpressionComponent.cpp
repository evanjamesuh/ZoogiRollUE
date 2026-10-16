#include "ZoogiExpressionComponent.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiArtifactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UZoogiExpressionComponent::UZoogiExpressionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

const TCHAR* UZoogiExpressionComponent::ExprName(EZoogiExpr E)
{
	static const TCHAR* N[] = { TEXT("neutral"), TEXT("blink"), TEXT("focused"), TEXT("determined"), TEXT("excited"), TEXT("dizzy"),
		TEXT("worried"), TEXT("ko"), TEXT("happy"), TEXT("sad"), TEXT("guard"), TEXT("flourish") };
	const int32 I = (int32)E;
	return (I >= 0 && I < (int32)EZoogiExpr::Num) ? N[I] : TEXT("none");
}

bool UZoogiExpressionComponent::FaceCamera()
{
	static int32 V = -1;
	if (V < 0)
	{
		bool b = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Characters"), TEXT("FaceCamera"), b, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFaceCam"))) b = false;
		V = b ? 1 : 0;
	}
	return V == 1;
}

float UZoogiExpressionComponent::FaceMaxTilt()
{
	static float V = -1.f;
	if (V < 0.f)
	{
		V = 32.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.Characters"), TEXT("FaceMaxTiltDeg"), V, GGameIni);
		V = FMath::Clamp(V, 0.f, 60.f);
	}
	return V;
}

AZoogiBall* UZoogiExpressionComponent::Ball() const { return Cast<AZoogiBall>(GetOwner()); }

void UZoogiExpressionComponent::Setup()
{
	AZoogiBall* B = Ball();
	if (!B || !B->Visual) return;
	EyeMID = nullptr;
	CoreMID = nullptr;
	const TArray<FName> Slots = B->Visual->GetMaterialSlotNames();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString S = Slots[i].ToString();
		UMaterialInterface* M = B->Visual->GetMaterial(i);
		if (!M) continue;
		if (!EyeMID && (S == TEXT("Eyes") || S.EndsWith(TEXT("_Eyes")))) EyeMID = B->Visual->CreateDynamicMaterialInstance(i, M);
		else if (!CoreMID && (S == TEXT("Core") || S.EndsWith(TEXT("_Core")))) CoreMID = B->Visual->CreateDynamicMaterialInstance(i, M);
	}
	if (CoreMID) { float V = 1.f; if (CoreMID->GetScalarParameterValue(TEXT("EmissiveBoost"), V)) CoreBoost0 = V; }
	if (EyeMID) { float V = 1.f; if (EyeMID->GetScalarParameterValue(TEXT("EmissiveBoost"), V)) EyeBoost0 = V; }

	// Personalities (character bible): Vex smug, Orin stoic, Luma hyper, Sera serene, Tavi cheerful, Moss calm, Blue earnest, Dark menacing.
	Persona = FPersona();
	const FName Id = B->Def.Id;
	if (Id == TEXT("luma"))           { Persona.BlinkMin = 1.6f; Persona.BlinkMax = 3.f; Persona.IdleFlourishMin = 5.f; Persona.IdleFlourishMax = 8.f; Persona.Energy = 1.6f; Persona.Tempo = 1.5f; }
	else if (Id == TEXT("orin"))      { Persona.BlinkMin = 4.f; Persona.BlinkMax = 7.f; Persona.IdleFlourishMin = 7.f; Persona.IdleFlourishMax = 11.f; Persona.Energy = 0.45f; Persona.Tempo = 0.6f; }
	else if (Id == TEXT("vex"))       { Persona.IdleFlourishMin = 5.f; Persona.IdleFlourishMax = 9.f; Persona.Energy = 1.1f; }
	else if (Id == TEXT("sera"))      { Persona.BlinkMin = 3.2f; Persona.BlinkMax = 5.5f; Persona.Energy = 0.7f; Persona.Tempo = 0.75f; }
	else if (Id == TEXT("tavi"))      { Persona.BlinkMin = 2.f; Persona.BlinkMax = 3.8f; Persona.IdleFlourishMin = 6.f; Persona.IdleFlourishMax = 10.f; Persona.Energy = 1.35f; Persona.Tempo = 1.2f; }
	else if (Id == TEXT("moss"))      { Persona.BlinkMin = 3.f; Persona.BlinkMax = 5.f; Persona.Energy = 0.75f; Persona.Tempo = 0.8f; }
	else if (Id == TEXT("darkzoogy")) { Persona.bVisor = true; Persona.IdleFlourishMin = 6.f; Persona.IdleFlourishMax = 10.f; Persona.Energy = 0.6f; Persona.Tempo = 0.7f; }
	NextBlink = FMath::FRandRange(Persona.BlinkMin, Persona.BlinkMax);
	NextIdleFlourish = FMath::FRandRange(Persona.IdleFlourishMin, Persona.IdleFlourishMax);
	LastKOs = B->KnockOuts; LastOrbs = B->OrbsScored; LastStun = B->StunTurns; LastBound = B->BoundShots; LastScore = B->Score;
	SpawnZ = B->GetActorLocation().Z;
	bSetup = true;
	Logged = EZoogiExpr::Num;
	UE_LOG(LogZoogi, Log, TEXT("Expr setup P%d %s: eyes %s, core %s"), B->PlayerIndex + 1, *B->Def.Name, EyeMID ? TEXT("atlas") : TEXT("missing"), CoreMID ? TEXT("yes") : TEXT("no"));
}

void UZoogiExpressionComponent::Trigger(EZoogiExpr E, float Duration, const TCHAR* Why)
{
	// Keep a stronger reaction (KO / dizzy) over a weaker one that arrives right after.
	if (EventLeft > 0.15f && (EventExpr == EZoogiExpr::KO || EventExpr == EZoogiExpr::Dizzy) && E != EZoogiExpr::KO && E != EZoogiExpr::Dizzy) return;
	EventExpr = E;
	EventLeft = Duration;
	EventWhy = Why;
}

void UZoogiExpressionComponent::OnImpact(float Impulse, AActor* Other)
{
	AZoogiBall* B = Ball();
	if (!B || !bSetup) return;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastImpact < 0.25f) return;
	LastImpact = Now;
	AZoogiBall* OB = Cast<AZoogiBall>(Other);
	AZoogiGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (B->IsActiveTurn() && OB && !(GM && GM->bComboMode && GM->IsAlly(B, OB)))
	{
		if (Impulse > 4000.f) Trigger(EZoogiExpr::Excited, 0.9f, TEXT("landed hit"));
		return;
	}
	if (B->Artifact && B->Artifact->Guard != EZoogiArtifact::None && OB) { Trigger(EZoogiExpr::Guard, 0.7f, TEXT("guard hit")); return; }
	if (Impulse > 9000.f) Trigger(EZoogiExpr::Dizzy, 1.1f + FMath::Min(Impulse / 40000.f, 0.8f), TEXT("impact"));
	else if (Impulse > 4500.f) Trigger(EZoogiExpr::Worried, 0.6f, TEXT("bump"));
}

bool UZoogiExpressionComponent::IsFalling() const
{
	const AZoogiBall* B = Ball();
	if (!B) return false;
	if (B->bOut || B->bKnockedOut) return true;
	// Dropping off the rim: well below the last resting height, falling, and at / past the edge (ramps and jumps don't count).
	if (B->GetActorLocation().Z > SpawnZ - 45.f || B->GetBallVelocity().Z > -150.f) return false;
	AZoogiGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	return EdgeDistance(GM) < ZoogiConst::ZoogiRadius * 1.5f;
}

float UZoogiExpressionComponent::EdgeDistance(AZoogiGameMode* GM) const
{
	const AZoogiBall* B = Ball();
	const FVector L = B->GetActorLocation();
	const FVector2D P(L.X, L.Y);
	if (GM && GM->Arena)
	{
		const FVector2D Out = P.GetSafeNormal();
		if (!Out.IsNearlyZero()) return GM->Arena->DistanceToEdge(P, Out);
	}
	return ZoogiConst::FloorRadius - P.Size();
}

void UZoogiExpressionComponent::PollEvents(AZoogiGameMode* GM)
{
	AZoogiBall* B = Ball();
	if (B->KnockOuts > LastKOs) Trigger(EZoogiExpr::Happy, 1.8f, TEXT("knock-out credit"));
	else if (B->OrbsScored > LastOrbs) Trigger(EZoogiExpr::Happy, 1.2f, TEXT("orb"));
	else if (B->Score > LastScore && B->IsActiveTurn()) Trigger(EZoogiExpr::Happy, 1.0f, TEXT("scored"));
	if (B->StunTurns > LastStun) Trigger(EZoogiExpr::Dizzy, 1.6f, TEXT("stunned"));
	if (B->BoundShots > LastBound) Trigger(EZoogiExpr::Sad, 1.3f, TEXT("bound"));
	LastKOs = B->KnockOuts; LastOrbs = B->OrbsScored; LastStun = B->StunTurns; LastBound = B->BoundShots; LastScore = B->Score;

	if (!GM || !GM->bComboMode) return;
	// 2v2 combo events arrive as callouts ("ALLY BUMP", "PARTNER FOLLOW-UP!  <NAME>").
	const float Now = GetWorld()->GetTimeSeconds();
	for (const FZoogiCallout& C : GM->Callouts)
	{
		if (C.Age > 0.35f) continue;
		if (const float* At = SeenCallouts.Find(C.Text)) { if (Now - *At < 2.f) continue; }
		SeenCallouts.Add(C.Text, Now);
		AZoogiBall* Active = GM->GetActiveBall();
		const bool bMyTeam = Active && (Active == B || GM->IsAlly(Active, B));
		if (C.Text.Contains(TEXT("ALLY BUMP")) && bMyTeam) Trigger(EZoogiExpr::Flourish, 1.4f, TEXT("ally bump"));
		else if (C.Text.Contains(TEXT("PARTNER FOLLOW-UP!")))
		{
			if (C.Text.Contains(B->Def.Name.ToUpper())) Trigger(EZoogiExpr::Determined, 1.6f, TEXT("partner follow-up"));
			else if (bMyTeam) Trigger(EZoogiExpr::Excited, 1.4f, TEXT("partner follow-up (ally)"));
		}
	}
}

EZoogiExpr UZoogiExpressionComponent::ChooseBase(AZoogiGameMode* GM, FString& Why) const
{
	const AZoogiBall* B = Ball();
	if (IsFalling()) { Why = TEXT("out"); return EZoogiExpr::KO; }
	if (!GM) { Why = TEXT("idle"); return EZoogiExpr::Neutral; }
	if (GM->Phase == EZoogiPhase::GameOver || GM->bComboMatchOver)
	{
		const bool bWin = GM->bComboMode ? (GM->ComboWinner >= 0 && B->TeamIndex == GM->ComboWinner) : (GM->WinnerIndex == B->PlayerIndex);
		Why = TEXT("match result");
		return bWin ? EZoogiExpr::Happy : EZoogiExpr::Sad;
	}
	if (B->StunTurns > 0) { Why = TEXT("stunned"); return EZoogiExpr::Dizzy; }
	const bool bGuarded = B->Artifact && B->Artifact->Guard != EZoogiArtifact::None;
	const float Speed = B->PlanarSpeed();
	if (B->IsActiveTurn())
	{
		switch (GM->Stage)
		{
		case EZoogiStage::Draw: Why = TEXT("draw"); return EZoogiExpr::Flourish;
		case EZoogiStage::Awaken:
			if (B->Artifact && B->Artifact->Infused != EZoogiArtifact::None) { Why = TEXT("awaken infuse"); return EZoogiExpr::Determined; }
			if (bGuarded) { Why = TEXT("awaken guard"); return EZoogiExpr::Guard; }
			Why = TEXT("awaken"); return EZoogiExpr::Neutral;
		case EZoogiStage::Battle:
			if (GM->Phase == EZoogiPhase::Aiming)
			{
				if (GM->BattleStep == EZoogiBattleStep::PowerUp) { Why = TEXT("power-up"); return EZoogiExpr::Determined; }
				Why = TEXT("aim"); return EZoogiExpr::Focused;
			}
			if (Speed > 180.f) { Why = TEXT("rolling"); return EZoogiExpr::Excited; }
			break;
		default: break;
		}
	}
	else if (GM->Phase == EZoogiPhase::Rolling)
	{
		if (bGuarded) { Why = TEXT("braced (guard)"); return EZoogiExpr::Guard; }
		if (const AZoogiBall* A = GM->GetActiveBall())
		{
			const FVector D = B->GetActorLocation() - A->GetActorLocation();
			const FVector V = A->GetBallVelocity();
			const bool bAlly = GM->bComboMode && GM->IsAlly(A, B);
			if (!bAlly && D.Size2D() < 520.f && V.Size2D() > 300.f && FVector::DotProduct(D.GetSafeNormal2D(), V.GetSafeNormal2D()) > 0.6f) { Why = TEXT("incoming"); return EZoogiExpr::Worried; }
		}
		if (Speed > 260.f) { Why = TEXT("knocked"); return EZoogiExpr::Worried; }
	}
	if (GM->Phase == EZoogiPhase::Aiming || GM->Phase == EZoogiPhase::Rolling || GM->Phase == EZoogiPhase::Resolving)
	{
		if (EdgeDistance(GM) < ZoogiConst::ZoogiRadius * 2.2f) { Why = TEXT("near edge"); return EZoogiExpr::Worried; }
	}
	if (B->BoundShots > 0 && B->IsActiveTurn()) { Why = TEXT("bound"); return EZoogiExpr::Sad; }
	Why = TEXT("idle");
	return EZoogiExpr::Neutral;
}

void UZoogiExpressionComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	AZoogiBall* B = Ball();
	if (!B || !bSetup) return;
	AZoogiGameMode* GM = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	Clock += Dt;
	if (B->IsAtRest() && !B->bOut) SpawnZ = B->GetActorLocation().Z; // last resting height (falling-out test)
	TickDemo(GM, Dt);
	PollEvents(GM);

	FString Why;
	EZoogiExpr Base = ChooseBase(GM, Why);
	if (Why == TEXT("out") || Why == TEXT("match result")) EventLeft = 0.f; // these outrank any short reaction
	EZoogiExpr Shown = Base;
	if (EventLeft > 0.f)
	{
		EventLeft -= Dt;
		Shown = EventExpr;
		Why = EventWhy;
	}
	// Idle personality beat.
	if (Shown == EZoogiExpr::Neutral)
	{
		NeutralTime += Dt;
		if (IdleFlourishLeft > 0.f) { IdleFlourishLeft -= Dt; Shown = EZoogiExpr::Flourish; Why = TEXT("idle flourish"); }
		else if (NeutralTime > NextIdleFlourish)
		{
			NeutralTime = 0.f;
			IdleFlourishLeft = 1.5f;
			NextIdleFlourish = FMath::FRandRange(Persona.IdleFlourishMin, Persona.IdleFlourishMax);
		}
	}
	else { NeutralTime = 0.f; IdleFlourishLeft = 0.f; }
	if (Forced != EZoogiExpr::Num) { Shown = Forced; Why = TEXT("forced"); }

	if (Shown != Current) { StateAge = 0.f; BeatClock = 0.f; }
	StateAge += Dt;
	Current = Shown;
	if (Shown != Logged)
	{
		Logged = Shown;
		UE_LOG(LogZoogi, Log, TEXT("Expr P%d %s -> %s (%s)"), B->PlayerIndex + 1, *B->Def.Name, ExprName(Shown), *Why);
	}

	// Blinks on the open-eyed states (the visor never blinks).
	EZoogiExpr Cell = Shown;
	const bool bBlinkable = !Persona.bVisor && Forced == EZoogiExpr::Num &&
		(Shown == EZoogiExpr::Neutral || Shown == EZoogiExpr::Focused || Shown == EZoogiExpr::Worried || Shown == EZoogiExpr::Excited || Shown == EZoogiExpr::Determined);
	NextBlink -= Dt;
	if (BlinkLeft > 0.f) { BlinkLeft -= Dt; if (bBlinkable) Cell = EZoogiExpr::Blink; }
	else if (NextBlink <= 0.f)
	{
		BlinkLeft = 0.12f;
		NextBlink = FMath::FRandRange(Persona.BlinkMin, Persona.BlinkMax) * (Shown == EZoogiExpr::Focused ? 1.6f : 1.f);
		if (FMath::FRand() < 0.18f) NextBlink = 0.28f; // occasional double blink
	}
	Apply(Cell, Dt);
	TickFace(GM, Dt);
}

void UZoogiExpressionComponent::Apply(EZoogiExpr Shown, float Dt)
{
	AZoogiBall* B = Ball();
	const float T = Clock * Persona.Tempo;
	const EZoogiExpr Mood = Current; // body / nub motion follows the mood, not the blink cell
	if (EyeMID)
	{
		EyeMID->SetScalarParameterValue(TEXT("ExprIndex"), (float)(int32)Shown);
		float Glow = 1.f;
		switch (Mood)
		{
		case EZoogiExpr::Determined: Glow = 1.5f + 0.25f * FMath::Sin(T * 10.f); break;
		case EZoogiExpr::Excited: case EZoogiExpr::Happy: Glow = 1.25f; break;
		case EZoogiExpr::KO: Glow = 0.35f; break;
		case EZoogiExpr::Sad: Glow = 0.75f; break;
		default: break;
		}
		if (Persona.bVisor)
		{
			// Dark Zoogy's optic: slow menacing pulse, quicker and hotter when focused / charging.
			const bool bHot = Mood == EZoogiExpr::Focused || Mood == EZoogiExpr::Determined || Mood == EZoogiExpr::Flourish;
			Glow *= bHot ? (1.35f + 0.45f * FMath::Sin(Clock * 7.5f)) : (0.95f + 0.3f * FMath::Sin(Clock * 2.1f));
			const FLinearColor Tint = Mood == EZoogiExpr::Determined ? FLinearColor(1.25f, 0.85f, 0.7f) : (Mood == EZoogiExpr::KO ? FLinearColor(0.5f, 0.35f, 0.4f) : FLinearColor(1.f, 1.f, 1.f));
			EyeMID->SetVectorParameterValue(TEXT("GlowTint"), Tint);
		}
		EyeMID->SetScalarParameterValue(TEXT("EmissiveBoost"), EyeBoost0 * Glow);
	}
	if (CoreMID)
	{
		float K = 1.f;
		switch (Mood)
		{
		case EZoogiExpr::Determined: K = 1.7f + 0.3f * FMath::Sin(T * 12.f); break;
		case EZoogiExpr::Excited: case EZoogiExpr::Happy: K = 1.3f; break;
		case EZoogiExpr::Guard: K = 1.2f; break;
		case EZoogiExpr::Sad: K = 0.6f; break;
		case EZoogiExpr::KO: K = 0.25f; break;
		default: K = 1.f + 0.08f * FMath::Sin(T * 2.f); break; // breathing
		}
		CoreMID->SetScalarParameterValue(TEXT("EmissiveBoost"), CoreBoost0 * K);
	}
	// Flipper nubs (visual-only child of Visual; mesh pivot = ball centre, face along +X, nubs on +-Y).
	if (B->Nubs && B->Nubs->IsVisible())
	{
		const float E = Persona.Energy;
		FVector S(1.f), L(0.f);
		switch (Mood)
		{
		case EZoogiExpr::Excited: case EZoogiExpr::Happy: case EZoogiExpr::Flourish:
		{
			const float F = FMath::Sin(T * (Mood == EZoogiExpr::Flourish ? 9.f : 15.f));
			S.Y = 1.f + 0.06f * E * (0.5f + 0.5f * F); L.Z = 3.f * E * F;
			break;
		}
		case EZoogiExpr::Determined: S.Y = 1.04f; L.Z = 5.f * FMath::Min(E, 1.f); break;
		case EZoogiExpr::Focused: L.X = 3.f; break;
		case EZoogiExpr::Worried: S.Y = 1.f + 0.025f * FMath::Sin(Clock * 42.f); L.Z = -1.f; break;
		case EZoogiExpr::Dizzy: L.Z = 4.f * FMath::Sin(Clock * 9.f); S.Y = 1.f + 0.02f * FMath::Cos(Clock * 9.f); break;
		case EZoogiExpr::Sad: L.Z = -6.f; S.Y = 0.97f; break;
		case EZoogiExpr::Guard: case EZoogiExpr::KO: S = FVector(0.9f, 0.86f, 0.9f); break; // tucked into the shell
		default: L.Z = 1.2f * E * FMath::Sin(T * 2.2f); break;
		}
		// Ease toward the target.
		const float A = 1.f - FMath::Exp(-Dt * 14.f);
		const FTransform Cur = B->Nubs->GetRelativeTransform();
		B->Nubs->SetRelativeLocation(FMath::Lerp(Cur.GetLocation(), L, A));
		B->Nubs->SetRelativeScale3D(FMath::Lerp(Cur.GetScale3D(), S, A));
	}
	// Small body beats through the existing hooks (all visual).
	BeatClock += Dt;
	if (Mood == EZoogiExpr::Happy && B->PlanarSpeed() < 60.f && !IsFalling() && Persona.Energy > 0.5f && BeatClock > 0.62f / Persona.Tempo)
	{
		BeatClock = 0.f;
		B->Squash(0.05f * FMath::Min(Persona.Energy, 1.3f), FVector::UpVector); // happy hop
	}
	else if (Mood == EZoogiExpr::Dizzy && BeatClock > 0.45f)
	{
		BeatClock = 0.f;
		B->Wobble(2.5f, FRotator(0.f, Clock * 400.f, 0.f).Vector()); // woozy circle
	}
	else if (Mood == EZoogiExpr::Flourish && StateAge < Dt * 1.5f && Persona.Energy > 1.f)
	{
		B->Squash(0.06f, FVector::UpVector);
	}
}

void UZoogiExpressionComponent::TickFace(AZoogiGameMode* GM, float Dt)
{
	// Facing rules (Evan, Oct 14):
	//  - the shooter, while aiming / charging, eases its face toward the chosen target (~0.3 s at rate 10/s) before launch;
	//  - otherwise only the local player's own Zoogi (lowest-index human) faces the camera;
	//  - every other Zoogi keeps its natural roll orientation (face turn off).
	AZoogiBall* B = Ball();
	if (!FaceCamera()) { B->SetFaceTarget(false, FQuat::Identity); return; }
	const bool bSettled = !IsFalling() && B->PlanarSpeed() < 70.f && FMath::Abs(B->GetBallVelocity().Z) < 60.f;
	if (!bSettled) { B->SetFaceTarget(false, FQuat::Identity); return; }

	if (GM && GM->Phase == EZoogiPhase::Aiming && GM->GetActiveBall() == B)
	{
		FVector D = FVector::ZeroVector;
		if (GM->Preview.bValid && GM->Preview.ContactActor.IsValid() && GM->Preview.ContactActor.Get() != B)
			D = GM->Preview.ContactActor->GetActorLocation() - B->GetActorLocation();
		else if (GM->Preview.bValid) D = GM->Preview.Dir;
		else if (GM->bFreeAim) D = GM->FreeAimPoint - B->GetActorLocation();
		if (D.Size2D() > 1.f)
		{
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			B->SetFaceTarget(true, FRotator(0.f, Yaw, 0.f).Quaternion(), 10.f);
			return;
		}
	}

	bool bLocalOwn = false;
	if (GM)
	{
		const AZoogiBall* Own = nullptr;
		for (const AZoogiBall* O : GM->Balls) if (O && !O->bCPU && (!Own || O->PlayerIndex < Own->PlayerIndex)) Own = O;
		if (!Own) for (const AZoogiBall* O : GM->Balls) if (O && O->PlayerIndex == 0) Own = O; // CPU-only (auto / attract): P1's seat
		bLocalOwn = Own == B;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!bLocalOwn || !PC || !PC->PlayerCameraManager) { B->SetFaceTarget(false, FQuat::Identity); return; }
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FVector D = Cam - B->GetActorLocation();
	if (D.Size2D() < 1.f) { B->SetFaceTarget(false, FQuat::Identity); return; }
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	const float Pitch = FMath::Min(FMath::RadiansToDegrees(FMath::Atan2(D.Z, D.Size2D())), FaceMaxTilt());
	// Model frame: +X = face, +Z = up. Positive pitch tips the face up toward the elevated gameplay camera.
	const FQuat Target = FRotator(FMath::Max(Pitch, 0.f), Yaw, 0.f).Quaternion();
	B->SetFaceTarget(true, Target, 3.5f * FMath::Clamp(Persona.Tempo, 0.7f, 1.4f));
}

void UZoogiExpressionComponent::TickDemo(AZoogiGameMode* GM, float Dt)
{
	static const bool bDemo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiExprDemo"));
	AZoogiBall* Me = Ball();
	if (!bDemo || !GM || !Me || Me->PlayerIndex != 0 || DemoStep == -2) return;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	if (DemoStep == -1)
	{
		// Start in the first Draw (before any artifact shell / orbiters cover a face) once everything is at rest;
		// freeze the turn flow (the game mode stops ticking) until every Zoogi has been photographed.
		if (!(GM->Phase == EZoogiPhase::Aiming && GM->Stage == EZoogiStage::Draw && GM->StageTime > 0.3f)) return;
		for (AZoogiBall* O : GM->Balls) if (O && !O->bOut && !O->IsAtRest()) return;
		GM->SetActorTickEnabled(false);
		// The active-turn light (player colour, 60 cd right above the ball) would tint the shell under test: off for the shots.
		for (AZoogiBall* O : GM->Balls) if (O && O->TurnLight) O->TurnLight->SetVisibility(false);
		DemoPrevView = PC->GetViewTarget();
		DemoCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		if (DemoCam)
		{
			if (ACameraActor* PrevCam = Cast<ACameraActor>(DemoPrevView.Get()))
			{
				DemoCam->GetCameraComponent()->PostProcessSettings = PrevCam->GetCameraComponent()->PostProcessSettings;
				DemoCam->GetCameraComponent()->PostProcessBlendWeight = PrevCam->GetCameraComponent()->PostProcessBlendWeight;
			}
			DemoCam->GetCameraComponent()->SetFieldOfView(26.f);
			DemoCam->GetCameraComponent()->bConstrainAspectRatio = false;
			PC->SetViewTarget(DemoCam);
		}
		DemoStep = 0; DemoBall = 0; DemoClock = 0.f;
		UE_LOG(LogZoogi, Log, TEXT("ExprDemo start (%d Zoogis)"), GM->Balls.Num());
	}
	DemoClock += Dt;
	while (DemoBall < GM->Balls.Num() && (!GM->Balls[DemoBall] || GM->Balls[DemoBall]->bOut)) { ++DemoBall; DemoStep = 0; DemoClock = 0.f; }
	if (DemoBall >= GM->Balls.Num())
	{
		for (AZoogiBall* O : GM->Balls) if (O && O->Expression) O->Expression->ForceExpr(EZoogiExpr::Num);
		if (DemoPrevView.IsValid()) PC->SetViewTarget(DemoPrevView.Get());
		if (DemoCam) DemoCam->Destroy();
		DemoCam = nullptr;
		for (AZoogiBall* O : GM->Balls) if (O && O->TurnLight) O->TurnLight->SetVisibility(true);
		GM->SetActorTickEnabled(true);
		DemoStep = -2;
		UE_LOG(LogZoogi, Log, TEXT("ExprDemo done"));
		return;
	}
	AZoogiBall* T = GM->Balls[DemoBall];
	// Gameplay-angle close-up: 50 degrees down, from the arena centre side so the background is the floor.
	const FVector C = T->BallLocation();
	FVector Dir = (-FVector(C.X, C.Y, 0.f)).GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = FVector(-1, 0, 0);
	const FVector Eye = C + (Dir * FMath::Cos(FMath::DegreesToRadians(50.f)) + FVector(0, 0, FMath::Sin(FMath::DegreesToRadians(50.f)))) * (ZoogiConst::ZoogiRadius * 11.f);
	if (DemoCam) DemoCam->SetActorLocationAndRotation(Eye, (C - Eye).Rotation());
	// Step 2k: set expression k (k = 0..11) at 1.0 + 0.5 k s; step 2k+1: photograph it 0.3 s later.
	const int32 N = (int32)EZoogiExpr::Num;
	const int32 K = DemoStep / 2;
	if (K >= N)
	{
		if (T->Expression) T->Expression->ForceExpr(EZoogiExpr::Num);
		++DemoBall; DemoStep = 0; DemoClock = 0.f;
		return;
	}
	const float At = 1.0f + 0.5f * K + ((DemoStep & 1) ? 0.3f : 0.f);
	if (DemoClock < At) return;
	if ((DemoStep & 1) == 0) { if (T->Expression) T->Expression->ForceExpr((EZoogiExpr)K); }
	else GM->TakeShot(FString::Printf(TEXT("exprdemo_%s_%02d_%s"), *T->Def.Id.ToString(), K, ExprName((EZoogiExpr)K)));
	++DemoStep;
}
