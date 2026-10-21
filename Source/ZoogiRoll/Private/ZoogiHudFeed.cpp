// hud-feed: see ZoogiHudFeed.h
#include "ZoogiHudFeed.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "ZoogiHudArt.h"
#include "ZoogiUIStyle.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateBrush.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

// ------------------------------------------------------------------ hook state (ZoogiFramedLayout.h)
namespace ZoogiFramed
{
	static TWeakObjectPtr<UTextureRenderTarget2D> GFeedTex;
	static bool bGFeedActive = false;
	void SetHudFeedTexture(UTextureRenderTarget2D* Texture) { GFeedTex = Texture; }
	UTextureRenderTarget2D* GetHudFeedTexture() { return GFeedTex.Get(); }
	void SetHudFeedActive(bool bActive) { bGFeedActive = bActive; }
	bool IsHudFeedActive() { return bGFeedActive; }

	bool HudFeedSlot(const FLayout& L, FBox2D& OutRect)
	{
		OutRect = FBox2D(ForceInit);
		if (!L.bValid || L.Viewport.X <= 0.f || L.Viewport.Y <= 0.f) return false;
		if (L.bPortrait || L.Viewport.Y > L.Viewport.X) return false; // HUD hidden = portrait play layout / portrait viewport
		// belt and braces: a layout without side columns (full-screen arena) has no tray slot either
		if (L.Left.GetSize().X < 1.f || L.Right.GetSize().X < 1.f) return false;
		// the overlay's left strip keeps its shape: x scales by T, y by the safe height / 768 (equal unless width-limited)
		const float X0 = L.Safe.Left, Y0 = L.Safe.Top;
		const float Sy = FMath::Max(0.1f, (L.Viewport.Y - L.Safe.Top - L.Safe.Bottom) / 768.f);
		const float Side = HudFeedSlotSize * FMath::Min(L.T, Sy);
		const FVector2D C(X0 + (HudFeedSlotX0 + HudFeedSlotSize * 0.5f) * L.T, Y0 + (HudFeedSlotY0 + HudFeedSlotSize * 0.5f) * Sy);
		OutRect = FBox2D(C - FVector2D(Side, Side) * 0.5f, C + FVector2D(Side, Side) * 0.5f);
		return Side >= 8.f;
	}
}

// ------------------------------------------------------------------ panel
namespace ZoogiHudFeed
{
	static FName GShot = NAME_None;
	static double GShotAt = -100.0;
	void NotifyShot(FName ShotType) { GShot = ShotType; GShotAt = FPlatformTime::Seconds(); }
	FName LastShot(float& OutAge) { OutAge = (float)(FPlatformTime::Seconds() - GShotAt); return GShot; }

	FPlan Plan(const ZoogiFramed::FLayout& L)
	{
		FPlan P;
		if (!ZoogiFramed::HudFeedSlot(L, P.Slot)) return P;
		const float Side = P.Slot.GetSize().X, K = Side / ZoogiFramed::HudFeedSlotSize; // px per slot design px
		P.Radius = 10.f * K;
		const float Inset = 6.f * K;
		P.Image = FBox2D(P.Slot.Min + FVector2D(Inset, Inset), P.Slot.Max - FVector2D(Inset, Inset));
		P.ImageRadius = 6.f * K;
		UTextureRenderTarget2D* T = ZoogiFramed::GetHudFeedTexture();
		P.State = (T && ZoogiFramed::IsHudFeedActive()) ? EPanel::Live : EPanel::Idle;
		if (P.State == EPanel::Live)
		{
			P.Texture = T;
			// fit: centre crop (fill the square, never letterbox) when the feed is not square; 256x256 maps 1:1
			const float TA = (T->SizeX > 0 && T->SizeY > 0) ? (float)T->SizeX / (float)T->SizeY : 1.f;
			const FVector2D IS = P.Image.GetSize();
			const float SA = IS.Y > 0.f ? IS.X / IS.Y : 1.f;
			if (TA > SA + 1e-3f) { const float W = SA / TA; P.UV = FBox2f(FVector2f(0.5f - W * 0.5f, 0.f), FVector2f(0.5f + W * 0.5f, 1.f)); }
			else if (TA < SA - 1e-3f) { const float H = TA / SA; P.UV = FBox2f(FVector2f(0.f, 0.5f - H * 0.5f), FVector2f(1.f, 0.5f + H * 0.5f)); }
		}
		return P;
	}

	static void Box(FSlateWindowElementList& Out, int32 Ly, const FGeometry& G, const FVector2D& P, const FVector2D& S, const FSlateBrush& B, const FLinearColor& C = FLinearColor::White)
	{
		if (S.X <= 0.f || S.Y <= 0.f) return;
		FSlateDrawElement::MakeBox(Out, Ly, G.ToPaintGeometry(FVector2f(S), FSlateLayoutTransform(FVector2f(P))), &B, ESlateDrawEffect::None, C);
	}

	void Paint(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer, const ZoogiFramed::FLayout& L, float S, float Time)
	{
		namespace HA = ZoogiHudArt;
		const FPlan Pl = Plan(L);
		if (Pl.State == EPanel::Hidden) return;
		const FVector2D P0 = Pl.Slot.Min * S, PS = Pl.Slot.GetSize() * S;
		const FVector2D I0 = Pl.Image.Min * S, IS = Pl.Image.GetSize() * S;
		const float K = PS.X / ZoogiFramed::HudFeedSlotSize; // local units per slot design px
		const float R = Pl.Radius * S, IR = Pl.ImageRadius * S;
		const bool bLive = Pl.State == EPanel::Live;
		auto Font = [&](float Sz) { return ZoogiUIStyle::BoldFont(FMath::Max(7, FMath::RoundToInt(Sz * K)), FMath::Max(1.f, 1.2f * K)); };

		// frame: soft drop into the stone, steel bezel, dark well (the picture / idle art sits in the well)
		ZoogiUIStyle::SBox(Out, Layer, Geo, P0 - FVector2D(3.f, 2.f) * K, PS + FVector2D(6.f, 7.f) * K, FLinearColor(0.f, 0.f, 0.f, 0.55f), FLinearColor::Transparent, R + 3.f * K, 0.f);
		ZoogiUIStyle::SBox(Out, Layer, Geo, P0, PS, FLinearColor(0.02f, 0.025f, 0.035f, 1.f), FLinearColor(0.36f, 0.4f, 0.46f, 1.f), R, 2.5f * K);
		ZoogiUIStyle::SBox(Out, Layer, Geo, P0 + FVector2D(2.5f, 2.5f) * K, PS - FVector2D(5.f, 5.f) * K, FLinearColor(0.02f, 0.025f, 0.035f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.85f), R - 2.5f * K, 1.f * K);

		if (bLive)
		{
			// the live render target, rounded mask (rounded-box brush with the texture as its resource)
			FSlateBrush B;
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.SetResourceObject(Pl.Texture);
			B.ImageSize = FVector2D(Pl.Texture->SizeX, Pl.Texture->SizeY);
			B.SetUVRegion(Pl.UV);
			B.OutlineSettings = FSlateBrushOutlineSettings(IR);
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			B.TintColor = FSlateColor(FLinearColor::White);
			ZoogiUIStyle::SBox(Out, Layer + 1, Geo, I0 - FVector2D(1.5f, 1.5f) * K, IS + FVector2D(3.f, 3.f) * K, FLinearColor(HA::Cyan.R, HA::Cyan.G, HA::Cyan.B, 0.8f), FLinearColor::Transparent, IR + 1.5f * K, 0.f);
			Box(Out, Layer + 2, Geo, I0, IS, B);
			// cut flash + shot label (Game Camera's OnGameplayPipShot)
			float Age = 99.f; const FName Shot = LastShot(Age);
			if (Age < 0.22f) ZoogiUIStyle::SBox(Out, Layer + 3, Geo, I0, IS, FLinearColor(1.f, 1.f, 1.f, 0.45f * (1.f - Age / 0.22f)), FLinearColor::Transparent, IR, 0.f);
			if (Shot != NAME_None && Age < 1.8f)
			{
				const float A = FMath::Clamp((1.8f - Age) / 0.4f, 0.f, 1.f);
				FString Lb = Shot.ToString().ToUpper();
				if (Lb.Len() > 14) Lb = Lb.Left(13) + TEXT(".");
				const FSlateFontInfo F = Font(8.f);
				const FVector2D TS = HA::Measure(Lb, F);
				const FVector2D LP(I0.X + IS.X * 0.5f - TS.X * 0.5f - 6.f * K, I0.Y + IS.Y - TS.Y - 9.f * K);
				ZoogiUIStyle::SBox(Out, Layer + 3, Geo, LP, FVector2D(TS.X + 12.f * K, TS.Y + 4.f * K), FLinearColor(0.01f, 0.03f, 0.06f, 0.7f * A), FLinearColor::Transparent, 4.f * K, 0.f);
				HA::Text(Out, Layer + 3, Geo, LP + FVector2D(6.f * K, 2.f * K), Lb, F, FLinearColor(1.f, 1.f, 1.f, A));
			}
		}
		else
		{
			// idle: dark well, faint Zoogi Roll knot emblem, a slow scan sheen, LIVE pill switched off
			ZoogiUIStyle::SBox(Out, Layer + 1, Geo, I0, IS, FLinearColor(0.035f, 0.045f, 0.06f, 1.f), FLinearColor(0.4f, 0.48f, 0.58f, 0.45f), IR, 1.f * K);
			ZoogiUIStyle::SBox(Out, Layer + 1, Geo, I0 + FVector2D(0.f, IS.Y * 0.55f), FVector2D(IS.X, IS.Y * 0.45f), FLinearColor(0.f, 0.f, 0.f, 0.25f), FLinearColor::Transparent, IR, 0.f);
			const FVector2D EmS(50.f * K * 1.25f, 60.f * K * 1.25f);
			HA::Part(Out, Layer + 2, Geo, FVector2D(I0.X + IS.X * 0.5f - EmS.X * 0.5f, I0.Y + IS.Y * 0.42f - EmS.Y * 0.5f), EmS, TEXT("emblem"),
				FBox2D(FVector2D(72.f, 5.f), FVector2D(122.f, 65.f)), FLinearColor(0.55f, 0.75f, 0.9f, 0.32f));
			HA::Text(Out, Layer + 2, Geo, FVector2D(I0.X + IS.X * 0.5f, I0.Y + IS.Y * 0.78f), TEXT("ZOOGI ROLL"), Font(9.f), FLinearColor(0.62f, 0.72f, 0.82f, 0.45f), 0.5f, 0.5f);
			const float Sw = FMath::Fmod(Time * 0.18f, 1.f);
			ZoogiUIStyle::SBox(Out, Layer + 2, Geo, FVector2D(I0.X + 3.f * K, I0.Y + IS.Y * Sw), FVector2D(IS.X - 6.f * K, 2.f * K), FLinearColor(0.5f, 0.85f, 1.f, 0.05f), FLinearColor::Transparent, 1.f * K, 0.f);
		}

		// LIVE pill, top-left of the picture: red dot breathing while live, grey "LIVE" when idle
		{
			const FSlateFontInfo F = Font(7.5f);
			const FVector2D TS = HA::Measure(TEXT("LIVE"), F);
			const FVector2D PP(I0.X + 6.f * K, I0.Y + 6.f * K), PSz(TS.X + 22.f * K, FMath::Max(TS.Y, 12.f * K) + 2.f * K);
			ZoogiUIStyle::SBox(Out, Layer + 3, Geo, PP, PSz, FLinearColor(0.01f, 0.02f, 0.04f, bLive ? 0.72f : 0.55f), bLive ? FLinearColor(1.f, 0.35f, 0.3f, 0.6f) : FLinearColor(0.4f, 0.45f, 0.52f, 0.4f), PSz.Y * 0.5f, 1.f * K);
			const float Dd = 6.f * K, Br = bLive ? 0.65f + 0.35f * FMath::Sin(Time * 5.f) : 1.f;
			ZoogiUIStyle::SBox(Out, Layer + 3, Geo, FVector2D(PP.X + 6.f * K, PP.Y + PSz.Y * 0.5f - Dd * 0.5f), FVector2D(Dd, Dd), bLive ? FLinearColor(1.f, 0.18f, 0.15f, Br) : FLinearColor(0.32f, 0.35f, 0.4f, 0.8f), FLinearColor::Transparent, Dd * 0.5f, 0.f);
			HA::Text(Out, Layer + 3, Geo, FVector2D(PP.X + 15.f * K, PP.Y + PSz.Y * 0.5f), TEXT("LIVE"), F, bLive ? FLinearColor::White : FLinearColor(0.55f, 0.6f, 0.66f, 0.85f), 0.f, 0.5f);
		}
	}

	void Update(APlayerController* PC, bool bHudShown, float Dt)
	{
		if (!PC || !PC->GetWorld() || !PC->GetWorld()->IsGameWorld()) return;
		UZoogiHudFeedGlue* Glue = PC->FindComponentByClass<UZoogiHudFeedGlue>();
		if (!Glue)
		{
			Glue = NewObject<UZoogiHudFeedGlue>(PC, TEXT("ZoogiHudFeedGlue"));
			Glue->RegisterComponent();
		}
		Glue->Drive(bHudShown, Dt);
	}
}

// ------------------------------------------------------------------ glue
UZoogiHudFeedGlue::UZoogiHudFeedGlue()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

void UZoogiHudFeedGlue::ResolveHooks()
{
	if (bResolved) return;
	bResolved = true;
	AActor* PC = GetOwner();
	if (!PC) return;
	// HOOK (Game Camera): looked up by name so this compiles and runs without their branch; can become direct calls once merged
	if (UFunction* Fn = PC->FindFunction(FName(TEXT("GetGameplayPipTexture"))))
	{
		FObjectPropertyBase* Ret = CastField<FObjectPropertyBase>(Fn->GetReturnProperty());
		bHasGetTex = Ret && Fn->NumParms == 1 && Ret->PropertyClass && Ret->PropertyClass->IsChildOf(UTextureRenderTarget2D::StaticClass());
	}
	if (UFunction* Fn = PC->FindFunction(FName(TEXT("SetGameplayPipActive"))))
		bHasSetActive = Fn->NumParms == 1 && CastField<FBoolProperty>(Fn->ChildProperties) != nullptr;
	bool bShotBound = false;
	if (FMulticastDelegateProperty* D = FindFProperty<FMulticastDelegateProperty>(PC->GetClass(), FName(TEXT("OnGameplayPipShot"))))
	{
		const UFunction* Sig = D->SignatureFunction;
		if (Sig && Sig->NumParms == 1 && CastField<FNameProperty>(Sig->ChildProperties))
		{
			FScriptDelegate Del; Del.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UZoogiHudFeedGlue, OnPipShot));
			D->AddDelegate(MoveTemp(Del), PC);
			bShotBound = true;
		}
	}
#if !UE_BUILD_SHIPPING
	bStub = !bHasGetTex && FParse::Param(FCommandLine::Get(), TEXT("ZoogiFeedStub"));
#endif
	UE_LOG(LogZoogi, Log, TEXT("HUDFEED hooks on %s: GetGameplayPipTexture=%d SetGameplayPipActive=%d OnGameplayPipShot=%d%s%s"), *PC->GetClass()->GetName(),
		bHasGetTex ? 1 : 0, bHasSetActive ? 1 : 0, bShotBound ? 1 : 0, bHasGetTex ? TEXT("") : TEXT(" (Game Camera branch not merged - feed calls skipped)"), bStub ? TEXT(" DEV STUB ON (-ZoogiFeedStub)") : TEXT(""));
}

UTextureRenderTarget2D* UZoogiHudFeedGlue::CallGetTexture()
{
	AActor* PC = GetOwner();
	UFunction* Fn = PC ? PC->FindFunction(FName(TEXT("GetGameplayPipTexture"))) : nullptr;
	if (!Fn) return nullptr;
	FObjectPropertyBase* Ret = CastField<FObjectPropertyBase>(Fn->GetReturnProperty());
	if (!Ret) return nullptr;
	uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max<int32>(1, (int32)Fn->ParmsSize));
	FMemory::Memzero(Parms, FMath::Max<int32>(1, (int32)Fn->ParmsSize));
	PC->ProcessEvent(Fn, Parms);
	return Cast<UTextureRenderTarget2D>(Ret->GetObjectPropertyValue_InContainer(Parms));
}

void UZoogiHudFeedGlue::CallSetActive(bool bActive)
{
	if (SentActive == (bActive ? 1 : 0)) return;
	SentActive = bActive ? 1 : 0;
	AActor* PC = GetOwner();
	UFunction* Fn = (bHasSetActive && PC) ? PC->FindFunction(FName(TEXT("SetGameplayPipActive"))) : nullptr;
	FBoolProperty* Arg = Fn ? CastField<FBoolProperty>(Fn->ChildProperties) : nullptr;
	if (!Fn || !Arg)
	{
		UE_LOG(LogZoogi, Log, TEXT("HUDFEED SetGameplayPipActive(%d): not available (Game Camera branch not merged) - skipped%s"), SentActive, bStub ? TEXT(", dev stub follows it") : TEXT(""));
		return;
	}
	uint8* Parms = (uint8*)FMemory_Alloca(FMath::Max<int32>(1, (int32)Fn->ParmsSize));
	FMemory::Memzero(Parms, FMath::Max<int32>(1, (int32)Fn->ParmsSize));
	Arg->SetPropertyValue_InContainer(Parms, bActive);
	PC->ProcessEvent(Fn, Parms);
	UE_LOG(LogZoogi, Log, TEXT("HUDFEED SetGameplayPipActive(%d) called"), SentActive);
}

void UZoogiHudFeedGlue::OnPipShot(FName ShotType)
{
	ZoogiHudFeed::NotifyShot(ShotType);
}

void UZoogiHudFeedGlue::Drive(bool bHudShown, float Dt)
{
	LastDrive = FPlatformTime::Seconds();
	ResolveHooks();
	UWorld* W = GetWorld();
	const AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	FBox2D Slot;
	const bool bSlot = ZoogiFramed::HudFeedSlot(ZoogiFramed::Current(), Slot);
	const bool bBattle = G && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving);
	const bool bWant = bHudShown && bSlot && bBattle;

	UTextureRenderTarget2D* Tex = nullptr;
	if (bHasGetTex)
	{
		// persistent RT (Game Camera: valid all match, never reallocated): query until we have it, then re-check now and then
		const double Now = FPlatformTime::Seconds();
		if (!CamTex.IsValid() || Now - LastTexQuery > 2.0) { LastTexQuery = Now; CamTex = CallGetTexture(); }
		Tex = CamTex.Get();
	}
	else if (bStub) Tex = TickStub(bWant, Dt);

	ZoogiFramed::SetHudFeedTexture(Tex);
	ZoogiFramed::SetHudFeedActive(bWant);
	CallSetActive(bWant);
	TickShots(bHudShown, !bHudShown || !bSlot ? ZoogiHudFeed::EPanel::Hidden : (Tex && bWant ? ZoogiHudFeed::EPanel::Live : ZoogiHudFeed::EPanel::Idle), Dt);
}

void UZoogiHudFeedGlue::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	// the HUD stopped driving us (widget gone / not ticking): switch the feed off
	if (SentActive == 1 && FPlatformTime::Seconds() - LastDrive > 0.5) Drive(false, Dt);
}

void UZoogiHudFeedGlue::EndPlay(const EEndPlayReason::Type Reason)
{
	if (SentActive == 1) CallSetActive(false);
	ZoogiFramed::SetHudFeedTexture(nullptr);
	ZoogiFramed::SetHudFeedActive(false);
	Super::EndPlay(Reason);
}

UTextureRenderTarget2D* UZoogiHudFeedGlue::TickStub(bool bLive, float Dt)
{
#if UE_BUILD_SHIPPING
	return nullptr;
#else
	// ===== DEV STUB (-ZoogiFeedStub): stands in for Game Camera's GetGameplayPipTexture / SetGameplayPipActive until their
	// branch is merged. Game Camera replaces this; it never runs when their functions exist and is compiled out of Shipping.
	// 256x256 SceneCapture2D close-up on the acting Zoogi's face; captures only while the panel is live.
	UWorld* W = GetWorld();
	if (!W) return nullptr;
	if (!StubRT)
	{
		StubRT = NewObject<UTextureRenderTarget2D>(this, TEXT("HudFeedStubRT"));
		StubRT->RenderTargetFormat = RTF_RGBA8;
		StubRT->ClearColor = FLinearColor(0.02f, 0.03f, 0.05f, 1.f);
		StubRT->InitAutoFormat(256, 256);
		StubRT->UpdateResourceImmediate(true);
	}
	if (!StubCam)
	{
		FActorSpawnParameters SP; SP.Name = TEXT("ZoogiHudFeedStubCam"); SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.ObjectFlags |= RF_Transient;
		StubCam = W->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, SP);
		if (!StubCam) return nullptr;
		USceneCaptureComponent2D* C = StubCam->GetCaptureComponent2D();
		C->TextureTarget = StubRT;
		C->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		C->FOVAngle = 34.f;
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->ShowFlags.SetMotionBlur(false);
		UE_LOG(LogZoogi, Log, TEXT("HUDFEED dev stub: SceneCapture2D 256x256 spawned (stand-in for Game Camera's PIP camera)"));
	}
	USceneCaptureComponent2D* C = StubCam->GetCaptureComponent2D();
	C->bCaptureEveryFrame = bLive;
	if (!bLive) return StubRT;
	const AZoogiGameMode* G = W->GetAuthGameMode<AZoogiGameMode>();
	const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
	if (B)
	{
		const float R = (B->Sphere ? B->Sphere->GetScaledSphereRadius() : 50.f);
		const FVector Loc = B->BallLocation();
		// face = the model's local +X (ZoogiBall.h); while it rolls fast, chase from the front of the motion instead
		FVector Dir = B->Visual ? B->Visual->GetForwardVector() : FVector::ForwardVector;
		const FVector V = B->GetBallVelocity();
		if (V.Size2D() > 150.f) Dir = V.GetSafeNormal2D();
		Dir.Z = 0.f;
		if (!Dir.Normalize()) Dir = FVector::ForwardVector;
		const FVector Want = Loc + Dir * R * 3.4f + FVector::UpVector * R * 0.9f;
		const FRotator WantRot = (Loc + FVector::UpVector * R * 0.25f - Want).Rotation();
		if (!bStubPlaced) { StubPos = Want; StubRot = WantRot; bStubPlaced = true; }
		StubPos = FMath::VInterpTo(StubPos, Want, Dt, 6.f);
		StubRot = FMath::RInterpTo(StubRot, WantRot, Dt, 6.f);
		StubCam->SetActorLocationAndRotation(StubPos, StubRot);
	}
	return StubRT;
#endif
}

void UZoogiHudFeedGlue::TickShots(bool bHudShown, ZoogiHudFeed::EPanel State, float Dt)
{
#if !UE_BUILD_SHIPPING
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFeedShots"));
	if (!bOn) return;
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || G->ShotDir.IsEmpty() || !ZoogiFramed::IsActive(GetWorld())) { ShotState = -1; return; }
	const int32 St = (int32)State;
	if (St != ShotState) { ShotState = St; ShotStateT = 0.f; }
	ShotStateT += Dt;
	static const TCHAR* Names[] = { TEXT("hidden"), TEXT("idle"), TEXT("live") };
	if (G->Phase == EZoogiPhase::Aiming && ShotStateT > 3.f) G->TakeShot(FString::Printf(TEXT("hudfeed_%s_aim"), Names[St]));
	if (G->Phase == EZoogiPhase::Rolling && ShotStateT > 1.2f) G->TakeShot(FString::Printf(TEXT("hudfeed_%s_rolling"), Names[St]));
#endif
}
