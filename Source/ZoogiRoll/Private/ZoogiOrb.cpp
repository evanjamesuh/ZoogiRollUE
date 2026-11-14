#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "NiagaraComponent.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiTypes.h"
#include "ZoogiPhysics.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiBall.h"
#include "ZoogiGameMode.h"
#include "ZoogiClone.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
AZoogiOrb::AZoogiOrb()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(ZoogiConst::OrbRadius);
	Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	Sphere->SetSimulatePhysics(true);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->BodyInstance.bUseCCD = true;
	Sphere->SetLinearDamping(0.02f);
	Sphere->SetAngularDamping(0.4f);
	RootComponent = Sphere;
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Sphere);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Sphere);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Sphere);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(8.f);
	Glow->SetAttenuationRadius(260.f);
	Glow->SetCastShadows(false);
	Glow->SetLightColor(ZoogiUtil::Hex(TEXT("#87CEEB")));
	// Oct 22 QQ #28: soft landing blob shadow (plane under the orb; opacity follows floor contact).
	LandShadow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandShadow"));
	LandShadow->SetupAttachment(Sphere);
	LandShadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LandShadow->SetCastShadow(false);
	LandShadow->SetReceivesDecals(false);
	LandShadow->SetRelativeLocation(FVector(0.f, 0.f, -ZoogiConst::OrbRadius + 1.5f));
	LandShadow->SetRelativeScale3D(FVector(0.01f));
	LandShadow->SetVisibility(false);
}
void AZoogiOrb::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentHit.AddDynamic(this, &AZoogiOrb::OnSphereHit);
	Phase = FMath::FRand() * 6.28f;
	// -ZoogiOrbCloseup (visual QA only): the first 3 orbs each get a short close-up camera + screenshot.
	static int32 CloseN = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiOrbCloseup")) && CloseN < 3)
	{
		const int32 N = CloseN++;
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, N]()
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
			if (!PC || bOut) return;
			const FVector L = OrbLocation();
			const FVector Eye = L + FVector(-3.2f, -1.6f, 2.0f) * ZoogiConst::OrbRadius;
			ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(Eye, (L - Eye).Rotation());
			if (!Cam) return;
			AActor* Prev = PC->GetViewTarget();
			PC->SetViewTarget(Cam);
			FTimerHandle H2, H3;
			GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [N, this]()
			{
				FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Shots/orb_close_%d%s.png"), N, bStar ? TEXT("_star") : TEXT("")), false, false);
			}), 1.0f, false);
			GetWorldTimerManager().SetTimer(H3, FTimerDelegate::CreateWeakLambda(this, [PC, Prev, Cam]()
			{
				if (IsValid(PC) && IsValid(Prev)) PC->SetViewTarget(Prev);
				if (IsValid(Cam)) Cam->Destroy();
			}), 2.0f, false);
		}), 5.0f + 3.0f * N, false);
	}
}
void AZoogiOrb::InitOrb(UPhysicalMaterial* PhysMat, float InRollingDecel)
{
	RollingDecel = InRollingDecel;
	if (PhysMat) Sphere->SetPhysMaterialOverride(PhysMat);
	Sphere->SetMassOverrideInKg(NAME_None, ZoogiConst::OrbMassRatio * ZoogiConst::MassKg, true);
	// Oct 24 (Evan): restored the Oct 7 Blue-shell glass orb (cfc9388, packaged Oct 7 12:41 PM) as the arena orb in all
	// arenas/modes: engine sphere at OrbRadius wearing Blue's translucent cyan glass shell MI, Blue's glowing resonance
	// core inside. Tripo stone /Game/Orbs/SM_Orb and /Game/Legacy012/SM_OrbBlue_Glow stay in the project, unused.
	ZoogiUtil::ApplyBlueMonkeyOrb(this); // Oct 24 (Evan): Blue monkey face orb (SM_OrbMonkey + Blue glass shell)
	// Oct 23 (Evan t770u): kill black LandShadow disc that rolled with field orbs — keep component but never show.
	if (LandShadow)
	{
		LandShadow->SetStaticMesh(nullptr);
		LandShadow->SetVisibility(false);
		ShadowMID = nullptr;
	}
}
void AZoogiOrb::MakeStar()
{
	bStar = true;
	const FLinearColor Gold(1.f, 0.72f, 0.12f);
	// Oct 7 star look (cfc9388): Blue glass shell kept, gold glowing core + gold light.
	for (int32 i = 0; i < Core->GetNumMaterials(); ++i) Core->SetMaterial(i, ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.45f, 0.02f), 3.f));
	Glow->SetLightColor(Gold);
	Glow->SetAttenuationRadius(320.f);
	// -ZoogiStarCloseup (visual QA only): the first gold star orb gets a close-up camera; the screenshot is requested on
	// several frames with distinct names so a capture harness request in the same frame cannot swallow it.
	static int32 StarN = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiStarCloseup")) && StarN < 2)
	{
		const int32 N = StarN++;
		float Delay = 6.f + 4.f * N;
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiStarCloseupAt="), Delay);
		UE_LOG(LogTemp, Display, TEXT("ZOOGI_STAR_CLOSEUP scheduled %d at +%.1fs (%s)"), N, Delay + 4.f * N, *GetName());
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, N]()
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
			if (!PC || bOut) { UE_LOG(LogTemp, Display, TEXT("ZOOGI_STAR_CLOSEUP %d skipped (pc=%d out=%d)"), N, PC != nullptr, bOut); return; }
			const FVector L = OrbLocation();
			const FVector Eye = L + FVector(-3.2f, -1.6f, 2.0f) * ZoogiConst::OrbRadius;
			ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(Eye, (L - Eye).Rotation());
			if (!Cam) return;
			AActor* Prev = PC->GetViewTarget();
			PC->SetViewTarget(Cam);
			for (int32 k = 0; k < 3; ++k)
			{
				FTimerHandle Hs;
				GetWorldTimerManager().SetTimer(Hs, FTimerDelegate::CreateWeakLambda(this, [N, k, PC, Cam]()
				{
					if (IsValid(PC) && IsValid(Cam)) PC->SetViewTarget(Cam);
					FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Shots/orb_star_close_%d_%d.png"), N, k), false, false);
					UE_LOG(LogTemp, Display, TEXT("ZOOGI_STAR_CLOSEUP %d shot %d requested"), N, k);
				}), 0.8f + 0.35f * k, false);
			}
			FTimerHandle H3;
			GetWorldTimerManager().SetTimer(H3, FTimerDelegate::CreateWeakLambda(this, [PC, Prev, Cam]()
			{
				if (IsValid(PC) && IsValid(Prev)) PC->SetViewTarget(Prev);
				if (IsValid(Cam)) Cam->Destroy();
			}), 2.4f, false);
		}), Delay + 4.f * N, false);
	}
}
void AZoogiOrb::MakePower(const FLinearColor& Color)
{
	bStar = true;
	bPower = true;
	PowerColor = Color;
	ApplyPowerLook();
}

void AZoogiOrb::ApplyPowerLook()
{
	if (!bPower) return;
	// Oct 29 (Evan): the old pillar orbs - bright glowing core in the element colour, tinted glass rim, matching light
	if (Core) for (int32 i = 0; i < Core->GetNumMaterials(); ++i) Core->SetMaterial(i, ZoogiUtil::GlowMID(this, PowerColor, 4.f));
	if (Visual)
		for (int32 i = 0; i < Visual->GetNumMaterials(); ++i)
			if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(Visual->GetMaterial(i))) { M->SetVectorParameterValue(TEXT("RimColor"), PowerColor); M->SetScalarParameterValue(TEXT("RimStrength"), 6.f); }
	if (Glow) { Glow->SetLightColor(PowerColor); Glow->SetAttenuationRadius(380.f); }
}

void AZoogiOrb::ApplyFloorGlowTint()
{
	const FZoogiOrbFloorGlow& G = AZoogiArena::ActiveOrbGlow;
	GlowSerial = AZoogiArena::ActiveOrbGlowSerial;
	GlowScale = G.Scale;
	if (bStar) return; // star orbs keep the gold MakeStar set
	Glow->SetLightColor((GetUniqueID() & 1) ? G.AltColor : G.Color);
	Glow->SetAttenuationRadius(G.Radius);
}
void AZoogiOrb::SetOut()
{
	if (!bOut && GetWorld() && Sphere)
	{
		const FVector Exit = OrbLocation();
		AZoogiArenaPiece::RingOutCrash(GetWorld(), Exit, Sphere->GetPhysicsLinearVelocity(), 25.f, nullptr);
		AZoogiArena::SpawnRingOutRipple(GetWorld(), Exit); // Oct 22 QQ #29
	}
	const bool bWasOut = bOut;
	const FVector SinkV = Sphere ? Sphere->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	bOut = true;
	if (BlueRibbon.IsValid()) { AZoogiFX::ReleaseArrowTrail(BlueRibbon.Get()); BlueRibbon.Reset(); }
	Sphere->SetSimulatePhysics(false);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const bool bSink = !bWasOut && AZoogiArena::OutsSinkThroughGround(GetWorld());
	if (bSink) { AZoogiArena::SinkThroughGround(GetWorld(), Sphere, SinkV); SetLifeSpan(0.f); FTimerHandle Th; TWeakObjectPtr<AZoogiOrb> Wo = this; GetWorldTimerManager().SetTimer(Th, [Wo]() { if (AZoogiOrb* O = Wo.Get()) if (O->bOut) { O->SetActorHiddenInGame(true); O->Sphere->SetSimulatePhysics(false); O->Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); } }, 1.1f, false); } // b179: Meadow - the orb drops through the plain, then hides
	else SetActorHiddenInGame(true);
	Glow->SetIntensity(0.f);
	if (LandShadow) LandShadow->SetVisibility(false);
}
void AZoogiOrb::Respawn(const FVector& Location)
{
	bOut = false;
	LastHitBy = -1;
	SetActorHiddenInGame(false);
	AZoogiArena::RestoreAfterSink(Sphere); // b179
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
	Sphere->SetSimulatePhysics(true);
	Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Sphere->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Glow->SetIntensity(bStar ? 12.f : 7.f);
}
bool AZoogiOrb::IsAtRest() const
{
	if (bOut) return true;
	return Sphere->GetPhysicsLinearVelocity().Size() < ZoogiConst::RestSpeed && ZoogiPhysics::IsGrounded(Sphere, ZoogiConst::OrbRadius);
}
FVector AZoogiOrb::OrbLocation() const
{
	return Sphere->GetComponentLocation();
}

void AZoogiOrb::UpdateBlueRibbon()
{
	// Free Arrow Trail: soft blue Water ribbon behind each active arena orb (continuous while !bOut).
	if (bOut)
	{
		if (BlueRibbon.IsValid()) { AZoogiFX::ReleaseArrowTrail(BlueRibbon.Get()); BlueRibbon.Reset(); }
		return;
	}
	if (!BlueRibbon.IsValid())
	{
		BlueRibbon = AZoogiFX::ArrowTrail(GetWorld(), this, TEXT("Water"), 0.4f, 0.f);
		if (!BlueRibbon.IsValid())
		{
			// No Water / pool full — try Basic (still a soft ribbon; Water is preferred blue).
			BlueRibbon = AZoogiFX::ArrowTrail(GetWorld(), this, TEXT("Basic"), 0.4f, 0.f);
		}
	}
	else if (UNiagaraComponent* R = BlueRibbon.Get())
	{
		if (!R->IsActive()) R->Activate(true);
	}
}

void AZoogiOrb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOut)
	{
		UpdateBlueRibbon();
		return;
	}
	if (PopStrength > 0.f && Sphere && Sphere->IsSimulatingPhysics())
	{
		const FVector V = Sphere->GetPhysicsLinearVelocity();
		const float Spd = V.Size2D();
		if (Spd > 120.f)
		{
			// a little hop + a random fan-out of the path + spin, scaled by how hard it was hit
			const FVector2D P = FVector2D(V.X, V.Y).GetRotated(FMath::FRandRange(-10.f, 10.f) * PopStrength); // b62: no speed gain (pops chained into chaos)
			const float Hop = FMath::Clamp(Spd * 0.22f, 90.f, 340.f) * PopStrength;
			Sphere->SetPhysicsLinearVelocity(FVector(P.X, P.Y, FMath::Max(V.Z, 0.f) + Hop));
			Sphere->SetPhysicsAngularVelocityInDegrees(Sphere->GetPhysicsAngularVelocityInDegrees() + FMath::VRand() * 900.f * PopStrength);
			LastPop = Phase;
		}
		PopStrength = 0.f;
	}
	const ZoogiPhysics::FRollResult Roll = ZoogiPhysics::ApplyRolling(Sphere, ZoogiConst::OrbRadius, RollingDecel, DeltaSeconds, ZoogiConst::FullLaunchSpeed);
	Phase += DeltaSeconds;
	// Item 12: tinted per map; full pool while the orb touches the floor, dimmer while airborne (reuses the roll trace).
	if (GlowSerial != AZoogiArena::ActiveOrbGlowSerial) ApplyFloorGlowTint();
	Contact = FMath::FInterpTo(Contact, Roll.bGrounded ? 1.f : 0.35f, DeltaSeconds, 8.f);
	Glow->SetIntensity(GlowScale * Contact * ((bStar ? 12.f : 7.f) + (bStar ? 5.f : 2.5f) * FMath::Sin(Phase * (bStar ? 4.f : 2.2f))));
	UpdateBlueRibbon();

	// Oct 23 (Evan t770u): LandShadow disabled (was black disc under field orbs).
	if (LandShadow && LandShadow->IsVisible())
	{
		LandShadow->SetVisibility(false);
	}
}
void AZoogiOrb::OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Oct 29 (build 20): fast orbs knock igloo blocks off too
	if (AZoogiIglooWall* Ig = Cast<AZoogiIglooWall>(OtherActor))
	{
		const FVector V = Sphere ? Sphere->GetPhysicsLinearVelocity() : GetVelocity();
		Ig->OnOrbHit(this, Hit.ImpactPoint, Hit.ImpactNormal, FMath::Abs(FVector::DotProduct(V, Hit.ImpactNormal)) + 0.35f * V.Size2D());
	}
	// b57 (Evan: "I want more playful scatter of marbles"): queue a pop for the next tick (post-collision velocity)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoPlayfulOrbs"));
		const float Imp = NormalImpulse.Size();
		// b62: only Zoogi / wolf hits pop (orb-on-orb pops fed each other)
		if (!bOff && !bOut && (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiClone>(OtherActor)) && Imp > 1500.f && Phase - LastPop > 0.3f)
			PopStrength = FMath::Max(PopStrength, FMath::Clamp(Imp / 15000.f, 0.25f, 1.f));
		// b62 (Evan: "orbs aren't scattering after a high impact hit ... but not too chaotic to where one hit blasts all orbs"): a hard Zoogi hit
		// also nudges the closest orbs around the struck one outward - at most 3, within 3.2 m, fading with distance, once per 0.3 s
		static double LastScatter = 0.0;
		const double Tn = FPlatformTime::Seconds();
		if (!bOff && !bOut && Cast<AZoogiBall>(OtherActor) && Imp > 5000.f && Tn - LastScatter > 0.3)
			if (AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr)
			{
				LastScatter = Tn;
				const FVector C = OrbLocation();
				TArray<TPair<float, AZoogiOrb*>> Near;
				for (AZoogiOrb* O : G->Orbs)
					if (O && O != this && !O->bOut && O->Sphere && O->Sphere->IsSimulatingPhysics())
					{
						const float D = FVector::Dist2D(O->OrbLocation(), C);
						if (D < 320.f && O->Sphere->GetPhysicsLinearVelocity().Size2D() < 250.f) Near.Add({ D, O });
					}
				Near.Sort([](const TPair<float, AZoogiOrb*>& A, const TPair<float, AZoogiOrb*>& B) { return A.Key < B.Key; });
				const float Base = FMath::Clamp(Imp / 40.f, 160.f, 420.f);
				for (int32 k = 0; k < FMath::Min(3, Near.Num()); ++k)
				{
					AZoogiOrb* O = Near[k].Value;
					const FVector2D Dir = FVector2D(O->OrbLocation() - C).GetSafeNormal().GetRotated(FMath::FRandRange(-20.f, 20.f));
					const float Sp = Base * (1.f - Near[k].Key / 400.f);
					const FVector V0 = O->Sphere->GetPhysicsLinearVelocity();
					O->Sphere->SetPhysicsLinearVelocity(V0 + FVector(Dir * Sp, FMath::FRandRange(40.f, 110.f)));
					if (O->LastHitBy < 0) O->LastHitBy = LastHitBy;
				}
			}
	}
	// b59 (Evan: realistic sounds): glass orbs clink off each other (rate limited across all orbs)
	if (Cast<AZoogiOrb>(OtherActor) && !bOut)
	{
		static double LastClink = 0.0;
		const float Imp = NormalImpulse.Size();
		const double Tn = FPlatformTime::Seconds();
		if (Imp > 900.f && Tn - LastClink > 0.05)
		{
			LastClink = Tn;
			ZoogiUtil::PlaySfx(this, TEXT("OrbClink"), FMath::Clamp(Imp / 12000.f, 0.15f, 0.8f), FMath::FRandRange(0.9f, 1.15f));
		}
	}
	// Chain credit: an orb pushed by an orb inherits that orb's hitter.
	if (AZoogiOrb* Other = Cast<AZoogiOrb>(OtherActor))
	{
		if (Other->LastHitBy >= 0 && LastHitBy < 0) LastHitBy = Other->LastHitBy;
		else if (LastHitBy >= 0 && Other->LastHitBy < 0) Other->LastHitBy = LastHitBy;
	}
}
