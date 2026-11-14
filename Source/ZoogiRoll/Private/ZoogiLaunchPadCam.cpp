#include "ZoogiLaunchPadCam.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiLaunchPad.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/UnrealType.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"

namespace ZoogiLaunchPadCam
{
	static TAutoConsoleVariable<int32> CVarPadView(TEXT("zoogi.Cam.PadView"), 0, TEXT("Launch pad camera special path (0 = soft-off: no 7398f61 SolvePose. Clear shoot from a hovering pad uses the normal AIM view while IsOnLaunchPad — see pad-clear-aim in ZoogiPlayerController; 1 = legacy pad aim/pose). AimYaw drives aim; ring-centre clamp stays bypassed while IsOnLaunchPad."));
	static TAutoConsoleVariable<float> CVarPadRadius(TEXT("zoogi.Cam.PadRadius"), 160.f, TEXT("Launch pad camera: a Zoogi within this many cm (2D) of a pad actor counts as on it (when the pad has no GetOccupant)."));
	static TAutoConsoleVariable<float> CVarPadBlend(TEXT("zoogi.Cam.PadBlend"), 0.5f, TEXT("Launch pad camera: seconds to blend into the normal camera once the Zoogi lands in the arena."));
	static TAutoConsoleVariable<float> CVarPadMaxFlight(TEXT("zoogi.Cam.PadMaxFlight"), 3.f, TEXT("Launch pad camera: the pad view ends at most this many seconds after leaving the pad."));

	static TAutoConsoleVariable<float> CVarPadBoom(TEXT("zoogi.Cam.PadBoom"), 0.f, TEXT("Launch pad camera (landscape): boom on the pad, cm (0 = the aim view's zoogi.Cam.AimBoom)."));
	static TAutoConsoleVariable<float> CVarPadPitch(TEXT("zoogi.Cam.PadPitch"), 0.f, TEXT("Launch pad camera (landscape): pitch down on the pad, deg (0 = the aim view's)."));
	static TAutoConsoleVariable<float> CVarPadScreenY(TEXT("zoogi.Cam.PadScreenY"), 0.f, TEXT("Launch pad camera (landscape): Zoogi height on screen on the pad, 0 bottom..1 top (0 = the aim view's)."));
	static TAutoConsoleVariable<int32> CVarPadPortrait(TEXT("zoogi.Cam.PadPortrait"), 1, TEXT("Launch pad camera in portrait: 1 = portrait aim chase behind the pad along the aim, 0 = off (portrait default camera)."));
	float Boom(float Def) { const float V = CVarPadBoom.GetValueOnGameThread(); return V > 0.f ? FMath::Clamp(V, 200.f, 4000.f) : Def; }
	float Pitch(float Def) { const float V = CVarPadPitch.GetValueOnGameThread(); return V > 0.f ? FMath::Clamp(V, 2.f, 80.f) : Def; }
	float ScreenY(float Def) { const float V = CVarPadScreenY.GetValueOnGameThread(); return V > 0.f ? FMath::Clamp(V, 0.05f, 0.95f) : Def; }
	bool PortraitOn() { return CVarPadPortrait.GetValueOnGameThread() != 0; }

	static bool GTest = false, GReal = true;
	void SetRealHooks(bool bOn) { GReal = bOn; }
	static FVector GTestLoc = FVector::ZeroVector, GTestInto = FVector::ForwardVector;

	bool Enabled() { return CVarPadView.GetValueOnGameThread() != 0; }
	float Radius() { return FMath::Clamp(CVarPadRadius.GetValueOnGameThread(), 20.f, 2000.f); }
	float BlendTime() { return FMath::Clamp(CVarPadBlend.GetValueOnGameThread(), 0.05f, 3.f); }
	float MaxFlight() { return FMath::Clamp(CVarPadMaxFlight.GetValueOnGameThread(), 0.2f, 10.f); }

	void SetTestPad(bool bOn, const FVector& Loc, const FVector& Into)
	{
		GTest = bOn; GTestLoc = Loc; GTestInto = Into.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Into.GetSafeNormal2D();
	}

	// ---- by-name calls (no link dependency); false when the function is absent or has another signature
	static bool CallBool(UObject* O, FName Name, bool& Out)
	{
		UFunction* F = O ? O->FindFunction(Name) : nullptr;
		if (!F || F->NumParms != 1) return false;
		FBoolProperty* R = CastField<FBoolProperty>(F->GetReturnProperty());
		if (!R) return false;
		uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Buf, F->ParmsSize);
		O->ProcessEvent(F, Buf);
		Out = R->GetPropertyValue_InContainer(Buf);
		return true;
	}
	static bool CallBoolWithObject(UObject* O, FName Name, UObject* Arg, bool& Out)
	{
		UFunction* F = O ? O->FindFunction(Name) : nullptr;
		if (!F || F->NumParms != 2) return false;
		FBoolProperty* R = CastField<FBoolProperty>(F->GetReturnProperty());
		FObjectPropertyBase* P = nullptr;
		for (TFieldIterator<FProperty> It(F); It && (It->PropertyFlags & CPF_Parm); ++It)
			if (!(It->PropertyFlags & CPF_ReturnParm)) P = CastField<FObjectPropertyBase>(*It);
		if (!R || !P || (Arg && !Arg->IsA(P->PropertyClass))) return false;
		uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Buf, F->ParmsSize);
		P->SetObjectPropertyValue_InContainer(Buf, Arg);
		O->ProcessEvent(F, Buf);
		Out = R->GetPropertyValue_InContainer(Buf);
		return true;
	}
	static bool CallObject(UObject* O, FName Name, UObject*& Out)
	{
		UFunction* F = O ? O->FindFunction(Name) : nullptr;
		if (!F || F->NumParms != 1) return false;
		FObjectPropertyBase* R = CastField<FObjectPropertyBase>(F->GetReturnProperty());
		if (!R) return false;
		uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Buf, F->ParmsSize);
		O->ProcessEvent(F, Buf);
		Out = R->GetObjectPropertyValue_InContainer(Buf);
		return true;
	}
	static bool CallVector(UObject* O, FName Name, FVector& Out)
	{
		UFunction* F = O ? O->FindFunction(Name) : nullptr;
		if (!F || F->NumParms != 1) return false;
		FStructProperty* R = CastField<FStructProperty>(F->GetReturnProperty());
		if (!R || R->Struct != TBaseStructure<FVector>::Get()) return false;
		uint8* Buf = (uint8*)FMemory_Alloca(FMath::Max<int32>(F->ParmsSize, 1));
		FMemory::Memzero(Buf, F->ParmsSize);
		O->ProcessEvent(F, Buf);
		Out = *R->ContainerPtrToValuePtr<FVector>(Buf);
		return true;
	}

	// pad actors, cached per world (re-scanned once a second)
	static TArray<TWeakObjectPtr<AActor>> GPads;
	static TWeakObjectPtr<UWorld> GPadWorld;
	static double GPadScanT = -1.0;
	static const TArray<TWeakObjectPtr<AActor>>& Pads(UWorld* W)
	{
		const double Now = FPlatformTime::Seconds();
		if (GPadWorld.Get() != W || Now - GPadScanT > 1.0)
		{
			GPadWorld = W; GPadScanT = Now; GPads.Reset();
			if (W) for (TActorIterator<AActor> It(W); It; ++It)
				if (It->GetClass()->GetName().Contains(TEXT("LaunchPad"))) GPads.Add(*It);
		}
		return GPads;
	}

	static FVector IntoArena(AActor* Pad, const FVector& PadLoc)
	{
		FVector D = FVector::ZeroVector;
		if (Pad && CallVector(Pad, TEXT("GetLaunchDirection"), D) && !D.GetSafeNormal2D().IsNearlyZero()) return D.GetSafeNormal2D();
		const FVector C = FVector(-PadLoc.X, -PadLoc.Y, 0.f).GetSafeNormal(); // the arena centre is the world origin
		if (!C.IsNearlyZero()) return C;
		return Pad ? Pad->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	}

	float TargetScreenY(bool bPortrait)
	{
		const float V = CVarPadScreenY.GetValueOnGameThread();
		return FMath::Clamp(V > 0.f ? V : (bPortrait ? 0.66f : 0.62f), 0.55f, 0.75f);
	}

	// the pad, its runway and anything attached: never pulls the pad camera in
	static void AddPadIgnores(UWorld* W, FCollisionQueryParams& Q)
	{
		if (!W) return;
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			const FString N = It->GetClass()->GetName() + TEXT(" ") + It->GetName();
			if (!N.Contains(TEXT("LaunchPad")) && !N.Contains(TEXT("Runway"))) continue;
			Q.AddIgnoredActor(*It);
			TArray<AActor*> Kids;
			It->GetAttachedActors(Kids, true, true);
			for (AActor* K : Kids) Q.AddIgnoredActor(K);
			if (AActor* Par = It->GetAttachParentActor()) Q.AddIgnoredActor(Par);
		}
	}

	void SolvePose(UWorld* World, const AActor* Shooter, const FVector& ZoogiLoc, float Yaw, float TanV, float TargetY, float ExtraPitch,
		FVector& OutLoc, FRotator& OutRot, float& OutPitch, float& OutBoom)
	{
		const float TV = FMath::Clamp(TanV, 0.05f, 5.f);
		float Theta = FMath::Clamp(Pitch(36.f) + ExtraPitch, 20.f, 75.f);   // look pitch down
		float Bm = Boom(900.f);
		// the Zoogi this many degrees above the optical axis (0 bottom..1 top screen convention)
		const float Off = FMath::RadiansToDegrees(FMath::Atan((2.f * FMath::Clamp(TargetY, 0.05f, 0.95f) - 1.f) * TV));
		float Phi = Theta - Off;                                              // boom elevation over the Zoogi
		if (Phi < 10.f) { Theta += 10.f - Phi; Phi = 10.f; }
		const float MaxUp = FMath::RadiansToDegrees(FMath::Atan(0.9f * TV));
		for (int32 i = 0; i < 12; ++i)                                        // the point 200 cm ahead on screen (top edge margin)
		{
			const float Pr = FMath::DegreesToRadians(Phi);
			const float Alpha = FMath::RadiansToDegrees(FMath::Atan2(Bm * FMath::Sin(Pr), Bm * FMath::Cos(Pr) + 200.f));
			if (Theta - Alpha <= MaxUp || Bm >= 3000.f) break;
			Bm = FMath::Min(Bm * 1.15f, 3000.f);
		}
		const FVector F = FRotator(0.f, Yaw, 0.f).Vector();
		const float PhiR = FMath::DegreesToRadians(Phi);
		const FVector Want = ZoogiLoc - F * Bm * FMath::Cos(PhiR) + FVector(0.f, 0.f, Bm * FMath::Sin(PhiR));
		FVector Loc = Want;
		if (World)
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiPadCam), false, Shooter);
			AddPadIgnores(World, Q);
			FHitResult Hit;
			const FVector From = ZoogiLoc + (Want - ZoogiLoc).GetSafeNormal() * 60.f;   // along the same ray: the screen height holds
			if (World->SweepSingleByChannel(Hit, From, Want, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Q) && Hit.bBlockingHit)
				Loc = From + (Want - From) * FMath::Max(Hit.Time - 0.02f, 0.f);
			if (FVector::Dist(Loc, ZoogiLoc) < 250.f) Loc = ZoogiLoc + (Want - ZoogiLoc).GetSafeNormal() * 250.f;
		}
		OutLoc = Loc; OutRot = FRotator(-Theta, Yaw, 0.f); OutPitch = Theta; OutBoom = (float)FVector::Dist(Loc, ZoogiLoc);
	}

	bool Query(const UObject* WorldCtx, const AActor* GameMode, const AZoogiBall* Shooter, FVector& OutPadLoc, FVector& OutInto, FString& OutSource)
	{
		// Occupancy only (Evan Oct 23 FIX3). PadView Enabled() soft-offs SolvePose / legacy pad pose; PadPhase
		// tracking in UpdatePadView must still see pad occupancy when PadView=0 so behind-cam covers pad->flight
		// only (not all ROLL ACTIVE after land). 6f277b3 removed UpdatePadView Enabled early-out but left this gate.
		if (!Shooter) return false;
		const FVector SL = Shooter->BallLocation();
		if (GTest)
		{
			if (FVector::Dist2D(SL, GTestLoc) > Radius()) return false;
			OutPadLoc = GTestLoc; OutInto = GTestInto; OutSource = TEXT("test pad");
			return true;
		}
		if (!GReal) return false;
		// the real hooks (AZoogiGameMode launch flow + the arena's AZoogiLaunchPad)
		if (const AZoogiGameMode* GMode = Cast<AZoogiGameMode>(GameMode))
		{
			const bool bFlow = GMode->bLaunchFlow;
			const bool bOn = Shooter->IsOnLaunchPad() && (!bFlow || GMode->IsOnLaunchPad(Shooter));
			const AZoogiLaunchPad* LP = GMode->Arena ? GMode->Arena->GetLaunchPad() : nullptr;
			if (LP || bFlow)
			{
				OutSource = bFlow ? TEXT("AZoogiBall::IsOnLaunchPad + AZoogiGameMode::IsOnLaunchPad") : TEXT("AZoogiBall::IsOnLaunchPad (no launch flow)");
				if (!bOn) return false;
				if (LP)
				{
					OutPadLoc = LP->GetLaunchTransform().GetLocation();
					OutInto = LP->GetLaunchDirection().GetSafeNormal2D();
					OutSource += LP->GetOccupant() == Shooter ? TEXT(" | AZoogiLaunchPad GetLaunchDirection, occupant ok") : TEXT(" | AZoogiLaunchPad GetLaunchDirection, occupant other");
				}
				else OutPadLoc = SL;
				if (OutInto.IsNearlyZero()) OutInto = FVector(-OutPadLoc.X, -OutPadLoc.Y, 0.f).GetSafeNormal();
				if (OutInto.IsNearlyZero()) OutInto = FVector::ForwardVector;
				return true;
			}
		}
		UWorld* W = WorldCtx ? WorldCtx->GetWorld() : nullptr;
		AZoogiBall* Ball = const_cast<AZoogiBall*>(Shooter);
		const TArray<TWeakObjectPtr<AActor>>& P = Pads(W);
		AActor* Nearest = nullptr; float Best = 1e12f;
		for (const TWeakObjectPtr<AActor>& A : P)
			if (A.IsValid()) { const float D = FVector::DistSquared2D(A->GetActorLocation(), SL); if (D < Best) { Best = D; Nearest = A.Get(); } }
		// authoritative flags from the launch flow / the ball
		bool bOn = false, bHook = false;
		if (CallBool(Ball, TEXT("IsOnLaunchPad"), bOn)) { bHook = true; OutSource = TEXT("ball IsOnLaunchPad()"); }
		else if (FBoolProperty* BP = FindFProperty<FBoolProperty>(Ball->GetClass(), TEXT("bOnLaunchPad"))) { bHook = true; bOn = BP->GetPropertyValue_InContainer(Ball); OutSource = TEXT("ball bOnLaunchPad"); }
		else if (GameMode && CallBoolWithObject(const_cast<AActor*>(GameMode), TEXT("IsBallOnLaunchPad"), Ball, bOn)) { bHook = true; OutSource = TEXT("mode IsBallOnLaunchPad()"); }
		if (bHook)
		{
			if (!bOn) return false;
			OutPadLoc = Nearest ? Nearest->GetActorLocation() : SL;
			OutInto = IntoArena(Nearest, OutPadLoc);
			return true;
		}
		// pad actors: occupant hook, else distance
		for (const TWeakObjectPtr<AActor>& A : P)
		{
			if (!A.IsValid()) continue;
			UObject* Occ = nullptr;
			bool bThis = false;
			if (CallObject(A.Get(), TEXT("GetOccupant"), Occ)) { bThis = Occ == Ball; OutSource = TEXT("pad GetOccupant()"); }
			else
			{
				const FVector PL = A->GetActorLocation();
				bThis = FVector::Dist2D(PL, SL) <= Radius() && FMath::Abs(PL.Z - SL.Z) < 300.f;
				OutSource = TEXT("pad distance");
			}
			if (bThis) { OutPadLoc = A->GetActorLocation(); OutInto = IntoArena(A.Get(), OutPadLoc); return true; }
		}
		return false;
	}
}
