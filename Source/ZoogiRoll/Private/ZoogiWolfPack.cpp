// Nov 1: Wolf Pack 2.0 (see ZoogiWolfPack.h for the 20-point list).
#include "ZoogiWolfPack.h"
#include "ZoogiFilm.h"
#include "ZoogiSkills.h"
#include "Kismet/GameplayStatics.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiClone.h"
#include "ZoogiArena.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "NiagaraComponent.h"
#include "ZoogiReactCam.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiActionFun.h"
#include "ZoogiFrontProfile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "CanvasItem.h"
#include "TextureResource.h"
#include "Engine/Texture2D.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ZoogiP5Style.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "ZoogiPhysics.h"
#include "Components/StaticMeshComponent.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/Package.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "ZoogiAurexPuppet.h"
#include "ZoogiAnimeSubsystem.h"

namespace
{
	int32 GHumanSummons = 0;
	constexpr float PaintPx = 120.f;      // tap radius on a target (screen px)
	constexpr float PileWindow = 1.5f;
	constexpr float ParryDist = 420.f;  // b55: was 230 - the close-up opens earlier (time is slowed to give 5 s)
	constexpr float TouchKOWindow = 2.2f;
	const FLinearColor PackGold(1.f, 0.82f, 0.25f);

	FVector WolfPos(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}
}

// ------------------------------------------------------------------ static data
const TArray<ZoogiWolf::FSkin>& ZoogiWolf::Skins()
{
	static const TArray<FSkin> S = {
		{ TEXT("frost"),   TEXT("FROST WOLVES"),   ZoogiUtil::Hex(TEXT("#6FD3FF")), 1.00f, 0,    0 },
		{ TEXT("inferno"), TEXT("INFERNO HOUNDS"), ZoogiUtil::Hex(TEXT("#FF6A1A")), 0.88f, 2500, 0 },
		{ TEXT("shadow"),  TEXT("SHADOW PACK"),    ZoogiUtil::Hex(TEXT("#8A4CFF")), 0.80f, 120,  1 },
		{ TEXT("golden"),  TEXT("GOLDEN ALPHA"),   ZoogiUtil::Hex(TEXT("#FFC83A")), 0.95f, 250,  1 },
		{ TEXT("cyber"),   TEXT("CYBER WOLVES"),   ZoogiUtil::Hex(TEXT("#34FFD0")), 1.10f, 4000, 0 },
		{ TEXT("spirit"),  TEXT("SPIRIT FOXES"),   ZoogiUtil::Hex(TEXT("#FF8AD8")), 1.25f, 180,  1 },
	};
	return S;
}

const ZoogiWolf::FSkin& ZoogiWolf::SkinById(FName Id)
{
	for (const FSkin& S : Skins()) if (S.Id == Id) return S;
	return Skins()[0];
}

int32 ZoogiWolf::LevelForXP(int32 XP, int32* OutBase, int32* OutNext)
{
	static const int32 Need[] = { 0, 100, 300, 700, 1500 };
	int32 L = 1;
	for (int32 i = 1; i < 5; ++i) if (XP >= Need[i]) L = i + 1;
	if (OutBase) *OutBase = Need[L - 1];
	if (OutNext) *OutNext = L < 5 ? Need[L] : Need[4];
	return L;
}

bool ZoogiWolf::BloodMoon()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBloodMoon"))) return false;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiBloodMoon"))) return true;
	return false; // b171 (Evan: "Remove the blood moon"): no weekend Blood Moon any more; -ZoogiBloodMoon still forces it for tests
}

bool ZoogiWolf::On() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWolfPack")); return b; }
int32 ZoogiWolf::HumanSummonsThisMatch() { return GHumanSummons; }

FString ZoogiWolf::BadgeTitle(FName Badge)
{
	if (Badge == TEXT("triple_hunt")) return TEXT("TRIPLE HUNT");
	if (Badge == TEXT("blood_moon")) return TEXT("BLOOD MOON");
	if (Badge == TEXT("lone_wolf")) return TEXT("LONE WOLF");
	if (Badge == TEXT("alpha_strike")) return TEXT("ALPHA STRIKE");
	return Badge.ToString().ToUpper();
}

void ZoogiWolf::AwardBadge(const UObject* Ctx, FName Badge)
{
	UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(Ctx);
	if (!P || P->Data().PackBadges.Contains(Badge)) return;
	P->Mutable().PackBadges.Add(Badge);
	P->Save();
	UE_LOG(LogZoogi, Log, TEXT("WOLF badge earned: %s"), *BadgeTitle(Badge));
}

// ------------------------------------------------------------------ subsystem
UZoogiWolfPackSys* UZoogiWolfPackSys::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UZoogiWolfPackSys>() : nullptr;
}

void UZoogiWolfPackSys::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HudHandle = AHUD::OnHUDPostRender.AddUObject(this, &UZoogiWolfPackSys::DrawHud);
	GHumanSummons = 0;
	bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfTest"));
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), ShotDir);
}

void UZoogiWolfPackSys::Deinitialize()
{
	if (HudHandle.IsValid()) AHUD::OnHUDPostRender.Remove(HudHandle);
	RestoreDil();
	Super::Deinitialize();
}

AZoogiGameMode* UZoogiWolfPackSys::GM() const
{
	UWorld* W = GetWorld();
	return W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
}

float UZoogiWolfPackSys::RealNow() const { const UWorld* W = GetWorld(); return W ? (float)W->GetRealTimeSeconds() : 0.f; }

void UZoogiWolfPackSys::SetDil(float D)
{
	UWorld* W = GetWorld(); if (!W || !W->GetWorldSettings()) return;
	if (UZoogiFunSys* F = UZoogiFunSys::Get(this)) if (F->ClashOn() || F->IsReplaying()) return;
	if (!bDilSet) { SavedDil = W->GetWorldSettings()->TimeDilation; bDilSet = true; }
	DilValue = D;
	W->GetWorldSettings()->SetTimeDilation(D);
}

void UZoogiWolfPackSys::RestoreDil()
{
	UWorld* W = GetWorld();
	if (!bDilSet || !W || !W->GetWorldSettings()) { bDilSet = false; return; }
	if (FMath::IsNearlyEqual(W->GetWorldSettings()->TimeDilation, DilValue, 0.001f)) W->GetWorldSettings()->SetTimeDilation(FMath::Max(SavedDil, 1.f));
	bDilSet = false; DilUntil = -1.f;
}

void UZoogiWolfPackSys::Toast(const FString& T, const FString& S, const FLinearColor& C, float Life, float Size)
{
	FToast X; X.Text = T; X.Sub = S; X.Col = C; X.Life = Life; X.Size = Size;
	Toasts.Add(X);
	if (Toasts.Num() > 4) Toasts.RemoveAt(0);
}

void UZoogiWolfPackSys::Howl(float Pitch, float Vol) { ZoogiUtil::PlaySfx(this, TEXT("Howl"), Vol, Pitch); }

FLinearColor UZoogiWolfPackSys::PackTint(const AZoogiBall* B) const
{
	if (ZoogiWolf::BloodMoon()) return FLinearColor(1.f, 0.22f, 0.18f);
	if (B && !B->bCPU)
	{
		if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this)) return ZoogiWolf::SkinById(P->Data().EquippedPackSkin).Tint;
		return ZoogiWolf::Skins()[0].Tint;
	}
	return B ? B->Def.Color : FLinearColor(0.3f, 0.55f, 1.f);
}

TArray<AActor*> UZoogiWolfPackSys::Targets(const AZoogiBall* For) const
{
	TArray<AActor*> Out;
	AZoogiGameMode* G = GM(); if (!G) return Out;
	for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Out.Add(O);
	for (AZoogiBall* B : G->Balls) if (B && B != For && !B->bOut && !G->IsAlly(B, For)) Out.Add(B);
	return Out;
}

FVector2D UZoogiWolfPackSys::Project(const FVector& Wp, bool& bOk) const
{
	FVector2D S(-1.f, -1.f); bOk = false;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) bOk = PC->ProjectWorldLocationToScreen(Wp, S, false);
	return S;
}

bool UZoogiWolfPackSys::HumanPackOut() const { return Phase == EPhase::Out && bHumanOwner; }

// ------------------------------------------------------------------ summon
void UZoogiWolfPackSys::SummonField(AZoogiGameMode* G, AZoogiBall* B, int32 Want)
{
	if (!G || !B || B->bOut || !GetWorld()) return;
	TArray<TWeakObjectPtr<AZoogiClone>>& F = FieldWolves.FindOrAdd(B->PlayerIndex);
	F.RemoveAll([](const TWeakObjectPtr<AZoogiClone>& Wk) { return !Wk.IsValid(); });
	const int32 Need = FMath::Clamp(Want, 0, 5) - F.Num();
	const FLinearColor Pu(0.62f, 0.3f, 1.f);
	const FVector L = B->BallLocation();
	for (int32 k = 0; k < Need; ++k)
	{
		const int32 Idx = F.Num();
		const FVector At = L + FRotator(0.f, B->AimYaw + 90.f + 120.f * Idx, 0.f).Vector() * (ZoogiConst::ZoogiRadius + 130.f) + FVector(0, 0, 8.f);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AZoogiClone* Cl = GetWorld()->SpawnActor<AZoogiClone>(AZoogiClone::StaticClass(), FTransform(FRotator::ZeroRotator, At), SP);
		if (!Cl) continue;
		Cl->InitClone(nullptr, nullptr, B->PlayerIndex, 1750.f, Pu);
		Cl->PackIdx = Idx; Cl->bAlpha = Idx == 0;
		Cl->HitMul = (Cl->bAlpha ? 1.5f : 1.f) * ZoogiSkills::SkillPow();
		Cl->BlastMul = (Cl->bAlpha ? 1.4f : 1.f) * ZoogiSkills::SkillPow();
		if (Cl->bAlpha) Cl->SetBodyScale(1.3f);
		Cl->MakeOrb(); Cl->Hold();
		G->Clones.Add(Cl); F.Add(Cl);
		if (AZoogiFabFX* Ef = AZoogiFabFX::Play(GetWorld(), TEXT("/Game/StylizedToonVFX3/Effects/NS_Energy_buff.NS_Energy_buff"), At - FVector(0, 0, 40.f), Cl->bAlpha ? 0.17f : 0.13f, 3.f, Cl, FVector(0, 0, -40.f)))
		{
			Ef->StopAfter = 1.2f; // one loop
			if (Ef->NC) { Ef->NC->SetVariableLinearColor(FName(TEXT("User.Color")), Pu); Ef->NC->SetVariableLinearColor(FName(TEXT("Color")), Pu); Ef->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), Pu); }
		}
		AZoogiFX::Spawn(GetWorld(), At, Pu, 110.f, 0.45f, false, 8.f);
	}
	if (Need > 0) { Howl(1.f, 0.7f); UE_LOG(LogZoogi, Log, TEXT("WOLF field: P%d summons %d (now %d waiting)"), B->PlayerIndex + 1, Need, F.Num()); }
}

void UZoogiWolfPackSys::TickField(AZoogiGameMode* G, float RealDt)
{
	for (auto It = FieldWolves.CreateIterator(); It; ++It)
	{
		TArray<TWeakObjectPtr<AZoogiClone>>& F = It.Value();
		F.RemoveAll([](const TWeakObjectPtr<AZoogiClone>& Wk) { return !Wk.IsValid() || !Wk->bWaiting; });
		if (!F.Num()) continue;
		AZoogiBall* B = nullptr;
		for (AZoogiBall* X : G->Balls) if (X && X->PlayerIndex == It.Key()) B = X;
		if (!B || B->bOut || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)
		{	// the player is knocked out (or the match is over): its waiting wolves go
			for (const TWeakObjectPtr<AZoogiClone>& Wk : F) if (AZoogiClone* Cw = Wk.Get()) Cw->Pop();
			UE_LOG(LogZoogi, Log, TEXT("WOLF field: P%d's %d waiting wolves vanish"), It.Key() + 1, F.Num());
			F.Reset(); continue;
		}
		const FVector L = B->BallLocation();
		const float Spin = GetWorld()->GetRealTimeSeconds() * 40.f;
		for (int32 i = 0; i < F.Num(); ++i)
			if (AZoogiClone* Cw = F[i].Get())
			{	// they circle the player and keep up with it
				const FVector Slot = L + FRotator(0.f, Spin + 360.f * i / F.Num(), 0.f).Vector() * (ZoogiConst::ZoogiRadius + 130.f) + FVector(0.f, 0.f, 10.f * FMath::Sin(Spin * 0.1f + i));
				Cw->SetActorLocation(FMath::VInterpTo(Cw->GetActorLocation(), Slot, RealDt, 7.f));
			}
	}
}

void UZoogiWolfPackSys::BeginPrecision(AZoogiGameMode* G, AZoogiBall* B, int32 N)
{
	if (!G || !B || Phase != EPhase::Idle) { UE_LOG(LogZoogi, Log, TEXT("PRECISION volley skipped (a pack is out)")); return; }
	BankedWolves = FMath::Max(1, N); bShotNext = true;
	UE_LOG(LogZoogi, Log, TEXT("PRECISION volley P%d: %d missiles"), B->PlayerIndex + 1, BankedWolves);
	BeginSummon(G, B);
}

void UZoogiWolfPackSys::BeginSummon(AZoogiGameMode* G, AZoogiBall* B)
{
	if (!G || !B || Phase != EPhase::Idle) { bShotNext = false; return; }
	bShot = bShotNext; bShotNext = false;
	Caster = B; Owner = B->PlayerIndex; bHumanOwner = !B->bCPU;
	Painted.Reset(); Wolves.Reset(); WolfHits.Reset(); WolfPopped.Reset(); Pile.Reset(); CpuParryRolled.Reset();
	SummonHits = 0; SummonKOs = 0; bHeld = bCharging = false;
	int32 Level = 1;
	if (bHumanOwner) { if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this)) Level = ZoogiWolf::LevelForXP(P->Data().PackXP); ++GHumanSummons; }
	NumWolves = 3 + (Level >= 5 ? 1 : 0) + (ZoogiWolf::BloodMoon() ? 1 : 0);
	PaintMax = BankedWolves > 0 ? BankedWolves : NumWolves; // b87: one painted target per banked wolf
	static const bool bOldPaint = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOldPaint"));
	if (bOldPaint) NumWolves = FMath::Max(NumWolves, BankedWolves);
	else if (BankedWolves > 0) NumWolves = BankedWolves; // b96d (Evan): never more wolves than the counter
	bBirdPick = !bOldPaint && bHumanOwner;
	const int32 Banked = BankedWolves; BankedWolves = 0;
	// #19 2v2: summoning right after your partner did = PACK COMBO -> Dire Wolf
	bDire = false;
	if (G->bComboMode && LastSummoner >= 0 && LastSummoner != Owner && RealNow() - LastSummonAt < 45.f)
		for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex == LastSummoner && G->IsAlly(O, B)) bDire = true;
	LastSummoner = Owner; LastSummonAt = RealNow();
	// #1 howl summon: slow-mo, cut-in, spirit burst, howl
	const FLinearColor T = PackTint(B);
	CutInAge = 0.f; CutInCol = T;
	if (!bShot) if (UZoogiAnimeSubsystem* An = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) An->PlayPowerIntro(TEXT("WolfPack"), B->PlayerIndex, T); // b62: 15 s Wolf Pack intro (when the clip exists)
	CutInTitle = bShot ? TEXT("PRECISION!") : (bDire ? TEXT("DIRE WOLF!") : TEXT("WOLF PACK!"));
	CutInSub = FString::Printf(TEXT("%s  -  AWOOOO!"), *B->Def.Name.ToUpper());
	const FVector L = B->BallLocation();
	AZoogiFX::Spawn(GetWorld(), L, T, 260.f, 0.9f, false, 12.f, B);
	AZoogiFX::Spawn(GetWorld(), L - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), T, 420.f, 1.0f, true, 9.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_White"), L, FRotator::ZeroRotator, 1.4f);
	const float SkinPitch = bHumanOwner ? [this]() { UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this); return P ? ZoogiWolf::SkinById(P->Data().EquippedPackSkin).Pitch : 1.f; }() : 1.f;
	Howl(SkinPitch, 1.1f);
	if (bHumanOwner) ZoogiFilm::Begin(TEXT("wolves"), 16.f); // b101 (only with -ZoogiFilm)
	if (bShot) SetDil(0.2f); // b97 slow-mo; b119c (Evan: "remove the slow down"): only a PRECISION volley slows the world
	Phase = EPhase::Paint; PhaseT0 = RealNow();
	PaintLimit = bHumanOwner ? (PaintSeen.Contains(Owner) ? 4.f : 5.f) + 0.8f * FMath::Max(0, PaintMax - 3) : 0.7f; // b55 (Evan: "more time to select targets"): was 1.6 / 2.8 s; b87: more per extra target
	if (bBirdPick) { PaintLimit = 30.f; if (bShot) SetDil(0.05f); StartBird(); Toast(bShot ? FString::Printf(TEXT("PRECISION x%d!"), NumWolves) : FString::Printf(TEXT("%d %s!"), NumWolves, NumWolves == 1 ? TEXT("WOLF") : TEXT("WOLVES")), FString::Printf(TEXT("PICK ALL %d TARGET%s - 30 SECONDS"), PaintNeed(), PaintNeed() == 1 ? TEXT("") : TEXT("S")), PackTint(B), 2.f, 1.f); } // b117 (Evan): 30 s, the world crawls meanwhile
	else if (bHumanOwner) Toast(FString::Printf(TEXT("%d %s!"), NumWolves, NumWolves == 1 ? TEXT("WOLF") : TEXT("WOLVES")), FString::Printf(TEXT("TAP UP TO %d TARGET%s"), PaintMax, PaintMax == 1 ? TEXT("") : TEXT("S")), PackTint(B), 2.f, 1.f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: %d banked -> paint up to %d, %d wolves"), Owner + 1, Banked, PaintMax, NumWolves);
	PaintSeen.Add(Owner);
	static const bool bLateSpawn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfLateSpawn"));
	bFieldPack = !bShot && !bLateSpawn; // b119 (Evan): "make the wolves summon on the field with the purple Energy Buff effect"
	if (bFieldPack) { SummonField(G, B, NumWolves); NumWolves = FieldWolves.FindOrAdd(Owner).Num(); PaintMax = FMath::Min(PaintMax, FMath::Max(1, NumWolves)); } // b119c: the wolves already waiting with the player
	// b120 (Evan: "if target selecting during aiming that counts as a target and if I push superpower it activates the effect"):
	// the target tapped while aiming (hard lock) gets the closest wolf at once - no pick step; the rest stay with the player
	if (bFieldPack && bHumanOwner && G->Phase == EZoogiPhase::Aiming && G->TargetLock.IsValid() && !G->bSoftLock && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfNoLockFire")))
	{
		AActor* Lk = G->TargetLock.Get();
		if (Targets(B).Contains(Lk))
		{
			PickActor(Lk);
			static const bool bStrikeOnPick = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfStrikeOnPick"));
			if (bStrikeOnPick || Painted.Num() >= PaintNeed())
			{
				UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s locked -> the closest wolf strikes it now"), Owner + 1, *Lk->GetName());
				Toast(TEXT("WOLF STRIKE!"), TEXT("YOUR TARGET"), PackTint(B), 1.4f, 1.f);
				Release();
				return;
			}
			// b129 (Evan: "I want to choose three different targets first"): the locked target is pick 1, pick the rest
			UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s is pick 1 of %d"), Owner + 1, *Lk->GetName(), PaintNeed());
			Toast(TEXT("TARGET 1 SET"), FString::Printf(TEXT("PICK %d MORE"), PaintNeed() - Painted.Num()), PackTint(B), 1.4f, 1.f);
		}
	}
	if (bDire) Toast(TEXT("PACK COMBO!"), TEXT("YOUR PARTNER'S PACK JOINS - DIRE WOLF"), PackGold, 2.f, 1.1f);
	if (G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen) Toast(TEXT("ICE PACK"), TEXT("WOLVES SLIDE FARTHER ON ICE"), FLinearColor(0.6f, 0.9f, 1.f), 1.4f, 0.8f);
	if (G->Arena && G->Arena->GetTheme() == EZoogiArena::Volcanic) Toast(TEXT("FIRE PACK"), TEXT("LAVA WOLVES BLAST HARDER"), FLinearColor(1.f, 0.5f, 0.15f), 1.4f, 0.8f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d %s: %d wolves%s, level %d, paint %.1fs"), Owner + 1, *B->Def.Name, NumWolves, bDire ? TEXT(" + DIRE") : TEXT(""), Level, PaintLimit);
}

void UZoogiWolfPackSys::PaintPick(const FVector2D& Screen, float MaxPx)
{
	if (Phase != EPhase::Paint || !bHumanOwner || Painted.Num() >= PaintMax) return;
	AActor* Best = nullptr; float BestD = MaxPx;
	for (AActor* A : Targets(Caster.Get()))
	{
		bool bOk = false; const FVector2D S = Project(WolfPos(A), bOk);
		if (!bOk) continue;
		const float D = FVector2D::Distance(S, Screen);
		if (D < BestD && !Painted.Contains(A)) { BestD = D; Best = A; }
	}
	if (!Best) return;
	PickActor(Best);
}

void UZoogiWolfPackSys::PickActor(AActor* Best)
{
	if (Phase != EPhase::Paint || !Best || Painted.Num() >= PaintMax) return;
	Painted.Add(Best);
	static const bool bRunPaint = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfMissile")); // b119 (Evan): "back to what it was"
	const bool bAlpha = (bRunPaint && !bShot) ? Painted.Num() == 1 : Painted.Num() == 3; // b117 (Evan): the alpha is the third (b119: PRECISION volleys)
	AZoogiFX::Spawn(GetWorld(), WolfPos(Best), bAlpha ? PackGold : PackTint(Caster.Get()), bAlpha ? 160.f : 120.f, 0.5f, true, 8.f, Best);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.7f, 1.0f + 0.15f * Painted.Num());
	UE_LOG(LogZoogi, Log, TEXT("WOLF paint %d: %s%s"), Painted.Num(), *Best->GetName(), bAlpha ? TEXT(" (ALPHA)") : TEXT(""));
	static const bool bStrikeEach = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfStrikeOnPick"));
	if (bFieldPack && bStrikeEach)
	{	// b119 (Evan): "when chosen the closest wolf strikes" (b129: only with -ZoogiWolfStrikeOnPick - the pack now waits for every pick)
		AZoogiClone* Near = nullptr; float NearD = 1e12f;
		for (const TWeakObjectPtr<AZoogiClone>& Wk : FieldWolves.FindOrAdd(Owner)) if (AZoogiClone* Cw = Wk.Get()) if (Cw->bWaiting) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(Best)); if (Dd < NearD) { NearD = Dd; Near = Cw; } }
		if (Near) { FieldWolves.FindOrAdd(Owner).Remove(Near); Wolves.Add(Near); WolfHits.Add(0); WolfPopped.Add(0); Near->Strike(Best); }
	}
}

bool UZoogiWolfPackSys::OnTap(const FVector2D& Screen)
{
	if (Phase == EPhase::Paint && bHumanOwner) { PaintPick(Screen, PaintPx); return true; }
	if (ParryOpen()) return TryParry();
	return false;
}

void UZoogiWolfPackSys::OnDrag(const FVector2D& Screen) { if (Phase == EPhase::Paint && bHumanOwner) PaintPick(Screen, PaintPx * 0.7f); }

int32 UZoogiWolfPackSys::PaintNeed() const { return FMath::Max(1, FMath::Min(PaintMax, Targets(Caster.Get()).Num())); }

void UZoogiWolfPackSys::StartBird()
{
	UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; AZoogiGameMode* G = GM();
	if (!PC || !G) return;
	if (!BirdCam.IsValid()) { FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; BirdCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP); }
	ACameraActor* Cam = BirdCam.Get(); if (!Cam) return;
	// frame the whole knockoff circle (+ margin) on whichever screen axis is tighter
	int32 Vx = 16, Vy = 9; PC->GetViewportSize(Vx, Vy); Vx = FMath::Max(Vx, 1); Vy = FMath::Max(Vy, 1);
	const float Fov = 70.f, Th = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)), Tv = Th * (float)Vy / (float)Vx;
	const float R = (G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius) * 1.08f;
	const FVector Ctr = G->Arena ? FVector(G->Arena->GetActorLocation().X, G->Arena->GetActorLocation().Y, 0.f) : FVector::ZeroVector;
	const float Yaw = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : 0.f;
	// b97 (Evan: "a more angled view"): a high three-quarter view of the whole arena from behind the player's camera, not straight down
	const float PitchDeg = 55.f, Dd = 0.82f * R / FMath::Max(0.05f, FMath::Min(Th, Tv));
	const FVector Back = FRotator(0.f, Yaw, 0.f).Vector();
	Cam->SetActorLocation(Ctr - Back * Dd * FMath::Cos(FMath::DegreesToRadians(PitchDeg)) + FVector(0.f, 0.f, Dd * FMath::Sin(FMath::DegreesToRadians(PitchDeg))));
	Cam->SetActorRotation(FRotator(-PitchDeg, Yaw, 0.f));
	Cam->GetCameraComponent()->SetFieldOfView(Fov);
	Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	if (!BirdPrev.IsValid()) BirdPrev = PC->GetViewTarget();
	PC->SetViewTargetWithBlend(Cam, 0.3f, VTBlend_Cubic);
	UE_LOG(LogZoogi, Log, TEXT("WOLF wide pick view: P%d picks %d target(s), 12 s, slow-mo"), Owner + 1, PaintNeed());
}

void UZoogiWolfPackSys::EndBird()
{
	UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (PC && BirdPrev.IsValid() && BirdCam.IsValid() && PC->GetViewTarget() == BirdCam.Get()) PC->SetViewTargetWithBlend(BirdPrev.Get(), 0.25f, VTBlend_Cubic);
	BirdPrev.Reset();
}

void UZoogiWolfPackSys::Release()
{
	AZoogiGameMode* G = GM();
	EndBird();
	RestoreDil();
	if (bFieldPack)
	{	// b119: the pack is already out; wolves nobody sent take the nearest target nobody picked (else the nearest at all)
		TArray<TWeakObjectPtr<AZoogiClone>>& F = FieldWolves.FindOrAdd(Owner);
		if (bHumanOwner && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfStrikeOnPick")))
		{	// b129: all targets chosen - each picked target gets the closest waiting wolf, all at once
			for (const TWeakObjectPtr<AActor>& Pw : Painted)
			{
				AActor* Tp = Pw.Get(); if (!Tp) continue;
				AZoogiClone* Near = nullptr; float NearD = 1e12f;
				for (const TWeakObjectPtr<AZoogiClone>& Wk : F) if (AZoogiClone* Cw = Wk.Get()) if (Cw->bWaiting) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(Tp)); if (Dd < NearD) { NearD = Dd; Near = Cw; } }
				if (Near) { F.Remove(Near); Wolves.Add(Near); WolfHits.Add(0); WolfPopped.Add(0); Near->Strike(Tp); }
			}
			UE_LOG(LogZoogi, Log, TEXT("WOLF P%d: %d target(s) chosen -> the pack runs"), Owner + 1, Wolves.Num());
		}
		if (!bHumanOwner)
		{	// a CPU sends its whole pack at the nearest open targets
			TArray<AActor*> Open = Targets(Caster.Get());
			for (const TWeakObjectPtr<AActor>& P : Painted) Open.Remove(P.Get());
			TArray<TWeakObjectPtr<AZoogiClone>> Send = F; F.Reset();
			for (const TWeakObjectPtr<AZoogiClone>& Wk : Send)
			{
				AZoogiClone* Cw = Wk.Get(); if (!Cw || !Cw->bWaiting) continue;
				TArray<AActor*> Pool = Open.Num() ? Open : Targets(Caster.Get());
				AActor* Pick = nullptr; float PickD = 1e12f;
				for (AActor* A : Pool) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(A)); if (Dd < PickD) { PickD = Dd; Pick = A; } }
				if (Pick) { Open.Remove(Pick); Wolves.Add(Cw); WolfHits.Add(0); WolfPopped.Add(0); Cw->Strike(Pick); } else F.Add(Cw);
			}
		}
		// b119c (Evan: "summoned wolves stay with the player until used"): the rest stay - and stay on the WOLF button
		int32 Kept = 0;
		for (const TWeakObjectPtr<AZoogiClone>& Wk : F) if (Wk.IsValid()) { ZoogiSkills::ArmWolf(G, Owner); ++Kept; }
		if (Kept) UE_LOG(LogZoogi, Log, TEXT("WOLF field: P%d keeps %d wolves waiting"), Owner + 1, Kept);
		bFieldPack = false;
	}
	else SpawnPack();
	Phase = Wolves.Num() ? EPhase::Out : EPhase::Idle;
	PhaseT0 = RealNow();
}

void UZoogiWolfPackSys::SpawnPack()
{
	AZoogiGameMode* G = GM(); AZoogiBall* B = Caster.Get();
	if (!G || !B || B->bOut) return;
	const FVector L = B->BallLocation();
	// #11 scent trail: rank the remaining targets by how close they lie to the path you rolled (and to you)
	TArray<AActor*> Pool = Targets(B);
	auto TrailDist = [&](const FVector& P)
	{
		float Best = FVector::Dist2D(P, L);
		for (const FVector& Q : Trail) Best = FMath::Min(Best, (float)FVector::Dist2D(P, Q));
		return Best;
	};
	Pool.Sort([&](const AActor& A, const AActor& C) { return TrailDist(WolfPos(&A)) * 0.6f + FVector::Dist2D(WolfPos(&A), L) * 0.4f - (Cast<AZoogiBall>(&A) ? 250.f : 0.f) < TrailDist(WolfPos(&C)) * 0.6f + FVector::Dist2D(WolfPos(&C), L) * 0.4f - (Cast<AZoogiBall>(&C) ? 250.f : 0.f); });
	TArray<AActor*> Assign;
	for (const TWeakObjectPtr<AActor>& P : Painted) if (P.IsValid()) Assign.Add(P.Get());
	for (AActor* A : Pool) { if (Assign.Num() >= NumWolves) break; if (!Assign.Contains(A)) Assign.Add(A); }
	// CPU alpha: the rival easiest to knock off goes first
	if (!bHumanOwner && Assign.Num() > 1)
	{
		int32 Bi = 0; float BestE = 1e9f;
		for (int32 i = 0; i < Assign.Num(); ++i)
			if (Cast<AZoogiBall>(Assign[i]) && G->Arena)
			{
				const FVector2D P(WolfPos(Assign[i]));
				const float E = G->Arena->DistanceToEdge(P, (P - FVector2D(L)).GetSafeNormal());
				if (E < BestE) { BestE = E; Bi = i; }
			}
		Assign.Swap(0, Bi);
	}
	// level perks
	int32 Level = 1;
	if (bHumanOwner) if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this)) Level = ZoogiWolf::LevelForXP(P->Data().PackXP);
	const bool bIce = G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen;
	const bool bFire = G->Arena && G->Arena->GetTheme() == EZoogiArena::Volcanic;
	const FLinearColor Tint = bFire && !ZoogiWolf::BloodMoon() ? FLinearColor(1.f, 0.42f, 0.08f) : PackTint(B);
	// #3 V formation behind the caster, running along its heading
	const FVector V = B->GetBallVelocity();
	FVector Fwd = FVector(V.X, V.Y, 0.f).GetSafeNormal();
	if (Fwd.IsNearlyZero()) Fwd = FRotator(0.f, B->AimYaw, 0.f).Vector();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd);
	// b114 (Evan: "add the Energy Buff effect to Wolf Pack"): the Energy Buff glow under the caster as the pack is called, and on
	// every wolf as it runs. Visual + sound only (no stat change). -ZoogiWolfNoEnergy = off.
	static const bool bWolfEnergy = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfNoEnergy"));
	static const TCHAR* EnergyFx = TEXT("/Game/StylizedToonVFX3/Effects/NS_Energy_buff.NS_Energy_buff");
	if (bWolfEnergy)
	{
		// b119 (Evan: "only show one of the two times it plays"): the caster's glow plays once, when Wolf Pack is cast (ZoogiSkills) - not again here
	}
	UStaticMesh* WolfMesh = ZoogiUtil::LoadMesh(TEXT("/Game/Zoogis/wolfgang/SM_wolfgang.SM_wolfgang"));
	if (!WolfMesh) WolfMesh = B->Visual ? B->Visual->GetStaticMesh() : nullptr;
	const int32 N = bDire ? 3 : NumWolves;
	const float SkinPitch = bHumanOwner ? [this]() { UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this); return P ? ZoogiWolf::SkinById(P->Data().EquippedPackSkin).Pitch : 1.f; }() : 1.f;
	for (int32 i = 0; i < N; ++i)
	{
		const bool bThisDire = bDire && i == 0;
		const int32 Row = (i + 1) / 2, Side = i == 0 ? 0 : (i % 2 ? 1 : -1);
		const FVector Off = -Fwd * (110.f + 90.f * Row) + Right * (Side * 120.f * Row) + (i >= 3 ? -Fwd * 120.f : FVector::ZeroVector);
		static const bool bRunCfg = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfMissile")); // b119 (Evan): the b116 run is back; b117 missiles = -ZoogiWolfMissile
		const bool bRun = bRunCfg && !bShot; // b119: a PRECISION volley flies the b117 missiles
		AActor* Aim = Assign.IsValidIndex(i) ? Assign[i] : (Assign.Num() ? Assign[0] : nullptr);
		FVector AimDir = Aim ? (WolfPos(Aim) - L).GetSafeNormal2D() : Fwd; if (AimDir.IsNearlyZero()) AimDir = Fwd;
		// b117: missiles leave from the Zoogi itself, each on the straight line to its own target (a little side offset so they never overlap)
		const FVector SpawnL = bRun ? (bFieldPack ? L + FRotator(0.f, B->AimYaw + 90.f + 360.f * i / FMath::Max(1, N), 0.f).Vector() * (ZoogiConst::ZoogiRadius + 150.f) + FVector(0, 0, 8.f) : L + Off + FVector(0, 0, bThisDire ? 30.f : 8.f))
			: L + AimDir * (ZoogiConst::ZoogiRadius + 70.f) + FVector::CrossProduct(FVector::UpVector, AimDir) * (Side * 80.f) + AimDir * (40.f * i) + FVector(0, 0, bThisDire ? 30.f : 8.f);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AZoogiClone* Cl = GetWorld()->SpawnActor<AZoogiClone>(AZoogiClone::StaticClass(), FTransform(Fwd.Rotation(), SpawnL), SP);
		if (!Cl) continue;
		AActor* T = Assign.IsValidIndex(i) ? Assign[i] : (Assign.Num() ? Assign[0] : nullptr);
		if (bThisDire)
		{
			// the Dire Wolf charges along the caster's aim at the rival nearest that line
			AActor* Line = nullptr; float BestS = 1e9f;
			for (AActor* A : Pool) if (Cast<AZoogiBall>(A)) { const FVector D = WolfPos(A) - L; const float Along = FVector::DotProduct(D.GetSafeNormal2D(), Fwd); const float S = D.Size2D() * (1.6f - Along); if (S < BestS) { BestS = S; Line = A; } }
			if (Line) T = Line;
		}
		Cl->InitClone(WolfMesh, T, Owner, bThisDire ? 2500.f : (bIce ? 2000.f : 1750.f), Tint);
		Cl->PackIdx = i;
		Cl->bAlpha = bRun ? (i == 0 && !bDire) : (i == 2 && !bDire); // b117 (Evan): "the alpha (third) hits harder"
		Cl->bDire = bThisDire;
		Cl->HitMul = bThisDire ? 2.5f : (Cl->bAlpha ? 1.5f : 1.f);
		Cl->BlastMul = (bThisDire ? 1.6f : (Cl->bAlpha ? 1.4f : 1.f)) * (Level >= 3 ? 1.15f : 1.f) * (bFire ? 1.3f : 1.f);
		Cl->ExtraLife = (Level >= 2 ? 0.6f : 0.f) + (bIce ? 0.6f : 0.f);
		if (bBirdPick && T) Cl->ExtraLife += FMath::Clamp(FVector::Dist2D(WolfPos(T), L) / 1750.f, 0.f, 2.5f); // b96d: reaches anywhere on the arena
		Cl->Rebounds = Level >= 4 ? 1 : 0;
		Cl->BlastMul *= ZoogiSkills::SkillPow(); Cl->HitMul *= ZoogiSkills::SkillPow(); // b100: a card Wolf Pack is 0.6, the VOID PACK Awaken power 1
		Cl->bIce = bIce; Cl->bFire = bFire;
		Cl->FormDir = Fwd; Cl->FormUntil = 0.32f; Cl->FormSpeed = FMath::Max(B->PlanarSpeed(), 1300.f);
		if (Cl->bAlpha || bThisDire) Cl->SetBodyScale(bThisDire ? 1.85f : 1.3f);
		if (bRun) { Cl->MakeOrb(); if (bFieldPack) Cl->Hold(); else Cl->Sphere->SetPhysicsLinearVelocity(Fwd * Cl->FormSpeed + Right * (Side * 260.f)); } // b119 (Evan): "just don't bring back the micro wolf"
		else
		{	// b117: fired - no V run, straight out at missile speed
			const float Ms = bThisDire ? 3000.f : 2600.f;
			static const bool bWolfBody = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMissileWolves"));
			Cl->SetMissile(Ms, !bWolfBody); // b117 (Evan): purple orbs, no micro wolf (-ZoogiMissileWolves = keep the wolf body)
			Cl->Sphere->SetPhysicsLinearVelocity(AimDir * Ms);
		}
		G->Clones.Add(Cl);
		Wolves.Add(Cl); WolfHits.Add(0); WolfPopped.Add(0);
		if (bWolfEnergy && !bShot) { const float Wr = Cl->Sphere ? Cl->Sphere->GetScaledSphereRadius() : 40.f; if (AZoogiFabFX* Ef = AZoogiFabFX::Play(GetWorld(), EnergyFx, SpawnL - FVector(0, 0, Wr), (bThisDire ? 0.22f : (Cl->bAlpha ? 0.17f : 0.13f)), 4.f, Cl, FVector(0, 0, -Wr + 2.f))) { Ef->StopAfter = 1.2f; if (bFieldPack && Ef->NC) { const FLinearColor Pu(0.62f, 0.3f, 1.f); Ef->NC->SetVariableLinearColor(FName(TEXT("User.Color")), Pu); Ef->NC->SetVariableLinearColor(FName(TEXT("Color")), Pu); Ef->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), Pu); } } } // b119: one loop, purple on the field
		AZoogiFX::Spawn(GetWorld(), SpawnL, Tint, Cl->bAlpha ? 140.f : 100.f, 0.45f, false, 8.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_White"), SpawnL, FRotator::ZeroRotator, Cl->bAlpha ? 0.9f : 0.6f);
		// #4 the howl chord: one pitch per wolf, staggered
		HowlQueue.Add(TPair<float, float>(RealNow() + 0.07f * (i + 1), SkinPitch * (bThisDire ? 0.7f : (0.9f + 0.11f * i))));
	}
	UE_LOG(LogZoogi, Log, TEXT("WOLF pack out P%d: %d wolves (painted %d, level %d%s%s%s)"), Owner + 1, Wolves.Num(), Painted.Num(), Level, bIce ? TEXT(", ice") : TEXT(""), bFire ? TEXT(", fire") : TEXT(""), bDire ? TEXT(", DIRE") : TEXT(""));
}

// ------------------------------------------------------------------ recall / charge / parry
void UZoogiWolfPackSys::OnPowerPress() { if (!HumanPackOut()) return; bHeld = true; bCharging = false; PressAt = RealNow(); }

void UZoogiWolfPackSys::OnPowerRelease()
{
	if (!bHeld) return;
	bHeld = false;
	if (bCharging) { bCharging = false; DetonateAll(); }
	else Recall();
}

void UZoogiWolfPackSys::Recall()
{
	if (Phase != EPhase::Out) return;
	AZoogiBall* B = Caster.Get(); if (!B) return;
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiClone>& W : Wolves) if (AZoogiClone* C = W.Get()) { C->StartRecall(B); ++N; }
	if (!N) return;
	Toast(TEXT("RECALL!"), TEXT("THE PACK RUNS BACK TO YOU"), PackTint(B), 1.0f, 0.8f);
	Howl(1.15f, 0.7f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF recall P%d: %d wolves"), Owner + 1, N);
}

void UZoogiWolfPackSys::ForceClear()
{
	if (Phase == EPhase::Idle) return;
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiClone>& W : Wolves) if (AZoogiClone* C = W.Get()) { C->Pop(); ++N; }
	RestoreDil(); EndKillCam(); EndBird();
	UE_LOG(LogZoogi, Log, TEXT("WOLF pack of P%d cleared for a new summon (%d wolves popped, phase %d)"), Owner + 1, N, (int32)Phase);
	Phase = EPhase::Idle; Wolves.Reset(); Painted.Reset(); bHeld = bCharging = false;
}

void UZoogiWolfPackSys::DetonateAll()
{
	int32 N = 0;
	for (const TWeakObjectPtr<AZoogiClone>& W : Wolves) if (AZoogiClone* C = W.Get()) { C->BlowNow(); ++N; }
	UE_LOG(LogZoogi, Log, TEXT("WOLF charged blast P%d: %d wolves"), Owner + 1, N);
}

bool UZoogiWolfPackSys::ParryOpen() const
{
	if (Phase != EPhase::Out) return false;
	const AZoogiGameMode* G = GM(); if (!G) return false;
	for (const TWeakObjectPtr<AZoogiClone>& W : Wolves)
	{
		const AZoogiClone* C = W.Get(); if (!C || C->IsRecalling()) continue;
		for (const AZoogiBall* B : G->Balls)
		{
			if (!B || B->bCPU || B->bOut || B->PlayerIndex == Owner || G->IsAlly(B, Caster.Get())) continue;
			const FVector D = B->BallLocation() - C->GetActorLocation();
			if (D.Size2D() < ParryDist && FVector::DotProduct(C->GetVelocity().GetSafeNormal2D(), D.GetSafeNormal2D()) > 0.3f) return true;
		}
	}
	return false;
}

bool UZoogiWolfPackSys::TryParry()
{
	AZoogiGameMode* G = GM(); if (!G || Phase != EPhase::Out) return false;
	for (int32 i = 0; i < Wolves.Num(); ++i)
	{
		AZoogiClone* C = Wolves[i].Get(); if (!C) continue;
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bCPU || B->bOut || B->PlayerIndex == Owner) continue;
			if (FVector::Dist2D(B->BallLocation(), C->GetActorLocation()) > ParryDist + 40.f) continue;
			WolfPopped[i] = 1;
			G->PublicPopup(TEXT("PARRY!"), B->BallLocation() + FVector(0, 0, 150.f), FLinearColor(0.55f, 0.95f, 1.f));
			C->Pop();
			UZoogiFunSys::ParryShield(GetWorld(), B); // b63: wood shield dome on the parry
			ZoogiUtil::PlaySfx(this, TEXT("Clack"), 0.9f, 1.4f);
			UE_LOG(LogZoogi, Log, TEXT("WOLF parried by P%d (wolf %d)"), B->PlayerIndex + 1, i);
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------ clone callbacks
void UZoogiWolfPackSys::OnCloneHit(AZoogiClone* C, AActor* Other)
{
	if (!C) return;
	const int32 i = Wolves.IndexOfByKey(C);
	if (i != INDEX_NONE && WolfHits.IsValidIndex(i)) ++WolfHits[i];
	++SummonHits;
	if (bHumanOwner) if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this)) { P->Mutable().PackXP += 5; }
	AZoogiBall* Ball = Cast<AZoogiBall>(Other);
	if (!Ball) return;
	FTouch& T = LastTouch.FindOrAdd(Ball); T.At = RealNow(); T.Owner = C->OwnerPlayer;
	// #8 Pack Tactics: two wolves on the same rival within 1.5 s
	FPile& Pl = Pile.FindOrAdd(Ball);
	if (RealNow() - Pl.First > PileWindow) { Pl.First = RealNow(); Pl.Hits = 0; Pl.bDone = false; }
	++Pl.Hits;
	if (Pl.Hits >= 2 && !Pl.bDone)
	{
		Pl.bDone = true;
		const FVector D = (Ball->BallLocation() - C->GetActorLocation()).GetSafeNormal2D();
		Ball->SetPlanarVelocity(Ball->GetBallVelocity() * 1.0f + D * 900.f, 160.f);
		if (AZoogiGameMode* G = GM()) G->PublicPopup(TEXT("PILE-ON! x2"), Ball->BallLocation() + FVector(0, 0, 160.f), PackGold);
		AZoogiFX::Spawn(GetWorld(), Ball->BallLocation(), PackGold, 170.f, 0.4f, false, 12.f, Ball);
		ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.6f, 1.3f);
		UE_LOG(LogZoogi, Log, TEXT("WOLF pile-on on P%d"), Ball->PlayerIndex + 1);
	}
}

void UZoogiWolfPackSys::OnCloneBump(AZoogiClone* C)
{
	if (RealNow() - LastBumpHowl < 0.6f) return;
	LastBumpHowl = RealNow();
	Howl(1.3f, 0.45f);
}

void UZoogiWolfPackSys::OnCloneGone(AZoogiClone* C, bool bDetonated) {}

AActor* UZoogiWolfPackSys::PickReboundTarget(AZoogiClone* C, const AActor* Exclude) const
{
	if (!C) return nullptr;
	AActor* Best = nullptr; float BestD = 1600.f;
	for (AActor* A : Targets(Caster.Get())) { if (A == Exclude) continue; const float D = FVector::Dist2D(WolfPos(A), C->GetActorLocation()); if (D < BestD) { BestD = D; Best = A; } }
	return Best;
}

// ------------------------------------------------------------------ KO, kill-cam, end
void UZoogiWolfPackSys::PackKO(AZoogiBall* Victim, int32 KOOwner)
{
	++SummonKOs;
	RoundPackKOs.FindOrAdd(KOOwner) += 1;
	const bool bHuman = KOOwner == Owner && bHumanOwner;
	if (bHuman) if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this)) P->Mutable().PackXP += 25;
	Toast(TEXT("PACK HUNT!"), FString::Printf(TEXT("%s WAS RUN DOWN"), Victim ? *Victim->Def.Name.ToUpper() : TEXT("RIVAL")), PackTint(Caster.Get()), 1.8f, 1.35f);
	Howl(0.95f, 1.f);
	// #5 kill-cam: 1 s of slow motion with the camera on the knockout
	if (Victim) StartKillCam(Victim->BallLocation());
	if (bHuman && Painted.Num() && Painted[0].Get() == Victim) ZoogiWolf::AwardBadge(this, TEXT("alpha_strike"));
	UE_LOG(LogZoogi, Log, TEXT("WOLF PACK HUNT: P%d knocked out by P%d's pack (%d this summon)"), Victim ? Victim->PlayerIndex + 1 : 0, KOOwner + 1, SummonKOs);
}

void UZoogiWolfPackSys::StartKillCam(const FVector& At)
{
	UWorld* W = GetWorld(); if (!W) return;
	if (UZoogiFunSys* F = UZoogiFunSys::Get(this)) if (F->IsReplaying() || F->ClashOn()) return;
	APlayerController* PC = W->GetFirstPlayerController(); if (!PC) return;
	SetDil(0.25f); DilUntil = RealNow() + 1.0f;
	if (!KillCam.IsValid())
	{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		KillCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP);
	}
	ACameraActor* Cam = KillCam.Get(); if (!Cam) return;
	FVector From = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : At + FVector(-800, 0, 600);
	FVector Dir = (At - From).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
	const FVector CamL = At - Dir * 520.f + FVector::CrossProduct(FVector::UpVector, Dir) * 220.f + FVector(0, 0, 230.f);
	Cam->SetActorLocation(CamL);
	Cam->SetActorRotation((At - CamL).Rotation());
	Cam->GetCameraComponent()->SetFieldOfView(62.f);
	if (!PrevView.IsValid()) PrevView = PC->GetViewTarget();
	PC->SetViewTargetWithBlend(Cam, 0.12f);
	KillCamEnd = RealNow() + 1.1f;
	if (bTest && !ShotDir.IsEmpty()) FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotDir, FString::Printf(TEXT("wolf_%02d_packhunt.png"), Shots++)), true, false);
}

void UZoogiWolfPackSys::EndKillCam()
{
	KillCamEnd = -1.f;
	UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (PC && PrevView.IsValid() && KillCam.IsValid() && PC->GetViewTarget() == KillCam.Get()) PC->SetViewTargetWithBlend(PrevView.Get(), 0.25f);
	PrevView.Reset();
}

void UZoogiWolfPackSys::EndPack()
{
	// #20 tally, #17 badges
	int32 Hunted = 0; for (int32 h : WolfHits) if (h > 0) ++Hunted;
	TallyScore = SummonHits + 2 * SummonKOs; TallyAge = 0.f; TallyCol = PackTint(Caster.Get());
	if (bHumanOwner)
	{
		if (Hunted >= 3) { ZoogiWolf::AwardBadge(this, TEXT("triple_hunt")); Toast(TEXT("BADGE: TRIPLE HUNT"), TEXT("ALL YOUR WOLVES LANDED A HIT"), PackGold, 2.f, 0.9f); }
		if (SummonKOs >= 3) { ZoogiWolf::AwardBadge(this, TEXT("blood_moon")); Toast(TEXT("BADGE: BLOOD MOON"), TEXT("3 KNOCKOUTS FROM ONE PACK"), FLinearColor(1.f, 0.25f, 0.2f), 2.f, 0.9f); }
		if (UZoogiFrontProfileSubsystem* P = UZoogiFrontProfileSubsystem::Get(this))
		{
			const int32 Before = ZoogiWolf::LevelForXP(P->Data().PackXP - SummonHits * 5 - SummonKOs * 25), After = ZoogiWolf::LevelForXP(P->Data().PackXP);
			if (After > Before)
			{
				static const TCHAR* Perk[] = { TEXT(""), TEXT(""), TEXT("LONGER HUNT"), TEXT("BIGGER BLAST"), TEXT("WOLVES REBOUND ONCE"), TEXT("A 4TH WOLF JOINS") };
				Toast(FString::Printf(TEXT("PACK LEVEL %d!"), After), Perk[FMath::Clamp(After, 0, 5)], PackGold, 2.2f, 1.f);
			}
			P->Save();
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("WOLF pack done P%d: hits %d, KOs %d, wolves that hunted %d/%d, score x%d"), Owner + 1, SummonHits, SummonKOs, Hunted, Wolves.Num(), TallyScore);
	Phase = EPhase::Idle; Wolves.Reset(); bHeld = bCharging = false;
}

// b172c arena weather (one per map): 0 none, 1 Blizzard, 2 Eruption, 3 Bloom, 4 Low Gravity
static int32 GWeather = 0, GWeatherRound = 0;
static float GWeatherBaseR = 0.f, GWeatherGravity = 0.f;
static TArray<TWeakObjectPtr<UStaticMeshComponent>> GWeatherProps;
static void WeatherClear(UWorld* W)
{
	if (GWeather == 2 && GWeatherBaseR > 0.f) AZoogiArena::ActiveKnockoffR = GWeatherBaseR;
	if (GWeather == 4 && W) if (AWorldSettings* WS = W->GetWorldSettings()) { WS->bGlobalGravitySet = GWeatherGravity != 0.f; WS->GlobalGravityZ = GWeatherGravity; }
	for (TWeakObjectPtr<UStaticMeshComponent>& C : GWeatherProps) if (C.IsValid()) C->DestroyComponent();
	GWeatherProps.Reset(); GWeather = 0; GWeatherRound = 0; GWeatherBaseR = 0.f;
}

void UZoogiWolfPackSys::ApplyBloodMoon(AZoogiGameMode* G)
{
	if (G && (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)) { if (GWeather) WeatherClear(GetWorld()); if (G->Phase == EZoogiPhase::Menu) bBloodDone = false; } // b172c: next match rolls again
	if (G && GWeather == 2 && G->Arena && G->RoundNumber != GWeatherRound && G->Phase != EZoogiPhase::Menu)
	{	// ERUPTION: the edge crumbles in a little every new round (down to 80 %)
		const bool bNew = GWeatherRound > 0; GWeatherRound = G->RoundNumber;
		if (bNew)
		{
			AZoogiArena::ActiveKnockoffR = FMath::Max(GWeatherBaseR * 0.8f, AZoogiArena::ActiveKnockoffR * 0.94f);
			Toast(TEXT("THE EDGE CRUMBLES"), FString::Printf(TEXT("ERUPTION - THE RING IS %d%% OF ITS SIZE"), FMath::RoundToInt(100.f * AZoogiArena::ActiveKnockoffR / FMath::Max(1.f, GWeatherBaseR))), FLinearColor(1.f, 0.45f, 0.1f), 2.4f, 1.f);
			UE_LOG(LogZoogi, Log, TEXT("WEATHER eruption: round %d, knockout radius %.0f"), G->RoundNumber, AZoogiArena::ActiveKnockoffR);
		}
	}
	if (bBloodDone || !G || !G->Arena || G->bRoyaleMode || G->Phase == EZoogiPhase::Menu) return;
	bBloodDone = true;
	if (!ZoogiWolf::BloodMoon())
	{	// b172c: no Blood Moon - some matches get the arena's own weather instead (-ZoogiWeather = always, -ZoogiNoWeather = never)
		WeatherClear(GetWorld());
		static const bool bForce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWeather"));
		static const bool bNoWeather = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWeather"));
		float Chance = 0.35f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWeatherChance="), Chance);
		if (bNoWeather || (!bForce && FMath::FRand() > Chance)) { UE_LOG(LogZoogi, Log, TEXT("WEATHER clear skies")); return; }
		switch (G->Arena->GetTheme())
		{
		case EZoogiArena::Frozen:
			GWeather = 1;
			for (AZoogiBall* Bx : G->Balls) if (Bx && Bx->Sphere) { Bx->Sphere->SetLinearDamping(0.004f); Bx->Sphere->SetAngularDamping(0.05f); }
			if (G->Arena->Fog) G->Arena->Fog->SetFogInscatteringColor(FLinearColor(0.75f, 0.85f, 1.f));
			Toast(TEXT("BLIZZARD"), TEXT("THE ICE IS SLICK - MARBLES SLIDE FURTHER"), FLinearColor(0.55f, 0.85f, 1.f), 3.f, 1.2f);
			break;
		case EZoogiArena::Volcanic:
			GWeather = 2; GWeatherBaseR = AZoogiArena::ActiveKnockoffR; GWeatherRound = G->RoundNumber;
			if (G->Arena->Sun) G->Arena->Sun->SetLightColor(FLinearColor(1.f, 0.6f, 0.4f));
			Toast(TEXT("ERUPTION"), TEXT("THE EDGE CRUMBLES IN EVERY ROUND"), FLinearColor(1.f, 0.45f, 0.1f), 3.f, 1.2f);
			break;
		case EZoogiArena::Meadow:
		{	// BLOOM: four flower bumpers pop up on the court (solid, bouncy)
			GWeather = 3;
			UStaticMesh* Sph = ZoogiUtil::BasicShape(TEXT("Sphere"));
			UMaterialInterface* Petal = ZoogiUtil::SurfaceMID(G->Arena, ZoogiUtil::Hex(TEXT("#F472B6")), 0.5f, ZoogiUtil::Hex(TEXT("#F472B6")), 0.35f);
			UPhysicalMaterial* Bouncy = ZoogiPhysics::MakeMaterial(G->Arena, 0.3f, 0.75f, false);
			const float Sc = G->Arena->GetActorScale3D().X > 0.f ? G->Arena->GetActorScale3D().X : 1.f;
			const float Rr = AZoogiArena::ActiveKnockoffR * 0.55f / Sc, Off = FMath::FRandRange(0.f, 90.f);
			for (int32 k = 0; k < 4 && Sph; ++k)
			{
				const float A = FMath::DegreesToRadians(Off + 45.f + k * 90.f);
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(G->Arena);
				C->SetStaticMesh(Sph); C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(G->Arena->GetRootComponent());
				C->SetCollisionProfileName(TEXT("BlockAll")); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				C->SetRelativeLocation(FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 40.f)); C->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.9f));
				C->RegisterComponent(); if (Petal) C->SetMaterial(0, Petal); if (Bouncy) C->SetPhysMaterialOverride(Bouncy);
				GWeatherProps.Add(C);
			}
			Toast(TEXT("BLOOM"), TEXT("FLOWER BUMPERS POP UP ON THE COURT"), FLinearColor(1.f, 0.45f, 0.75f), 3.f, 1.2f);
			break;
		}
		case EZoogiArena::Cosmic:
			if (AWorldSettings* WS = GetWorld()->GetWorldSettings())
			{
				GWeather = 4; GWeatherGravity = WS->bGlobalGravitySet ? WS->GlobalGravityZ : 0.f;
				const float Gz = WS->GetGravityZ(); WS->bGlobalGravitySet = true; WS->GlobalGravityZ = Gz * 0.55f;
			}
			Toast(TEXT("LOW GRAVITY"), TEXT("MARBLES FLOAT - BIG HITS FLY FURTHER"), FLinearColor(0.7f, 0.55f, 1.f), 3.f, 1.2f);
			break;
		default: break;
		}
		UE_LOG(LogZoogi, Log, TEXT("WEATHER %d on arena %d"), GWeather, (int32)G->Arena->GetTheme());
		return;
	}
	if (G->Arena->Sun) G->Arena->Sun->SetLightColor(FLinearColor(1.f, 0.55f, 0.45f));
	if (G->Arena->Fog) G->Arena->Fog->SetFogInscatteringColor(FLinearColor(0.75f, 0.18f, 0.2f));
	Toast(TEXT("BLOOD MOON"), TEXT("THE PACK GROWS - EVERY WOLF PACK HAS 4 WOLVES"), FLinearColor(1.f, 0.22f, 0.18f), 3.f, 1.2f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF Blood Moon is up"));
}

// ------------------------------------------------------------------ tick
void UZoogiWolfPackSys::Tick(float DeltaTime)
{
	const float Now = RealNow();
	const float RealDt = FMath::Clamp(Now - LastReal, 0.f, 0.1f); LastReal = Now;
	CutInAge += RealDt; TallyAge += RealDt;
	for (FToast& T : Toasts) T.Age += RealDt;
	Toasts.RemoveAll([](const FToast& T) { return T.Age > T.Life; });
	AZoogiGameMode* G = GM();
	if (!G || !ZoogiWolf::On()) return;
	TickField(G, RealDt); // b119c
	ApplyBloodMoon(G);
	for (int32 i = HowlQueue.Num() - 1; i >= 0; --i) if (Now >= HowlQueue[i].Key) { Howl(HowlQueue[i].Value, 0.55f); HowlQueue.RemoveAt(i); }
	if (DilUntil > 0.f && Now > DilUntil) RestoreDil();
	if (KillCamEnd > 0.f && Now > KillCamEnd) EndKillCam();
	// b55: a rival wolf closing on the human's Zoogi -> slow-mo + side close-up on the hit; time scales so it lands ~5 s later
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactSlow"));
		AZoogiClone* Inc = nullptr; AZoogiBall* Vic = nullptr; float Best = 1e9f;
		if (!bOff && Phase == EPhase::Out)
			if (AZoogiGameMode* Gp = GM())
				for (const TWeakObjectPtr<AZoogiClone>& Wp : Wolves)
				{
					AZoogiClone* C = Wp.Get(); if (!C || C->IsRecalling()) continue;
					for (AZoogiBall* B : Gp->Balls)
					{
						if (!B || B->bCPU || B->bOut || B->PlayerIndex == Owner || Gp->IsAlly(B, Caster.Get())) continue;
						const FVector D = B->BallLocation() - C->GetActorLocation();
						if (D.Size2D() < ParryDist && D.Size2D() < Best && FVector::DotProduct(C->GetVelocity().GetSafeNormal2D(), D.GetSafeNormal2D()) > 0.3f) { Best = D.Size2D(); Inc = C; Vic = B; }
					}
				}
		if (Inc && Vic)
		{
			UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			const bool bStart = ParryFocusT0 < 0.f;
			if (bStart)
			{
				ParryFocusT0 = Now;
				if (!KillCam.IsValid() && W) { FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; KillCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP); }
				if (PC && KillCam.IsValid()) { if (!PrevView.IsValid()) PrevView = PC->GetViewTarget(); PC->SetViewTargetWithBlend(KillCam.Get(), 0.f); } // instant: a blend would crawl under the slow-mo
				Toast(TEXT("INCOMING!"), TEXT("TAP REACTION TO PARRY"), FLinearColor(0.55f, 0.95f, 1.f), 1.4f, 0.9f);
				UE_LOG(LogZoogi, Log, TEXT("WOLF incoming on P%d: slow-mo close-up, %.0f s to parry"), Vic->PlayerIndex + 1, ZoogiReactCam::Window);
				// b58 (Evan): the Zoogi transforms into its Aurex in the parry slow cut (clearly visible before the hit)
				if (!ParryPup.IsValid() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiReactAurex"))) // b67: no transform before a hit
					if (AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(W, Vic, Vic->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), (Inc->GetActorLocation() - Vic->BallLocation()).Rotation().Yaw, EZoogiPuppetPose::Guard))
					{ P->bFollowBall = true; ParryPup = P; }
			}
			if (AZoogiAurexPuppet* P = ParryPup.Get()) { const FVector Fd = (Inc->GetActorLocation() - Vic->BallLocation()).GetSafeNormal2D(); if (!Fd.IsNearlyZero()) P->FaceDir = Fd; }
			ZoogiReactCam::Frame(KillCam.Get(), Inc->GetActorLocation(), Vic->BallLocation() + FVector(0.f, 0.f, ParryPup.IsValid() ? 60.f : 0.f), RealDt, bStart);
			const float Left = FMath::Max(ZoogiReactCam::Window - (Now - ParryFocusT0), 0.25f);
			const float Spd = FMath::Max(Inc->GetVelocity().Size2D(), 300.f);
			SetDil(FMath::Clamp(FMath::Max(Best - 70.f, 10.f) / Spd / Left, 0.01f, 1.f));
			DilUntil = Now + 0.15f; KillCamEnd = Now + 0.35f; // both lapse on their own once the wolf is gone / parried
			if (bTest && bStart && !ShotDir.IsEmpty()) FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotDir, FString::Printf(TEXT("wolf_%02d_incoming.png"), Shots++)), true, false);
		}
		else { ParryFocusT0 = -1.f; if (AZoogiAurexPuppet* P = ParryPup.Get()) P->Dismiss(true); ParryPup.Reset(); }
	}
	// scent trail of the active shooter
	if (AZoogiBall* A = G->GetActiveBall())
	{
		if (TrailBall.Get() != A || G->Phase == EZoogiPhase::Aiming) { Trail.Reset(); TrailBall = A; }
		if (G->Phase == EZoogiPhase::Rolling && (!Trail.Num() || FVector::Dist2D(Trail.Last(), A->BallLocation()) > 90.f)) Trail.Add(A->BallLocation());
	}
	// knockouts by a wolf
	for (AZoogiBall* B : G->Balls)
	{
		if (!B) continue;
		if (B->bOut && !WasOut.Contains(B))
		{
			WasOut.Add(B);
			if (const FTouch* T = LastTouch.Find(B)) if (Now - T->At < TouchKOWindow && T->Owner != B->PlayerIndex) PackKO(B, T->Owner);
		}
		else if (!B->bOut) WasOut.Remove(B);
	}
	// rounds: the ALPHA HOWL goes to the round's pack-KO leader
	const bool bOver = G->Phase == EZoogiPhase::GameOver;
	if (SeenRound < 0) SeenRound = G->RoundNumber;
	if (G->RoundNumber != SeenRound || (bOver && !bSeenGameOver))
	{
		SeenRound = G->RoundNumber; bSeenGameOver = bOver;
		int32 Best = -1, BestN = 0;
		for (const TPair<int32, int32>& P : RoundPackKOs) if (P.Value > BestN) { BestN = P.Value; Best = P.Key; }
		RoundPackKOs.Reset();
		if (Best >= 0)
			for (AZoogiBall* B : G->Balls) if (B && B->PlayerIndex == Best)
			{
				Toast(TEXT("ALPHA HOWL!"), FString::Printf(TEXT("%s LEADS THE PACK (%d PACK KO%s)"), *B->Def.Name.ToUpper(), BestN, BestN > 1 ? TEXT("S") : TEXT("")), PackGold, 2.6f, 1.3f);
				AZoogiFX::Spawn(GetWorld(), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), PackGold, 380.f, 1.2f, true, 10.f);
				AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Gold"), B->BallLocation(), FRotator::ZeroRotator, 1.4f);
				Howl(0.9f, 1.1f); HowlQueue.Add(TPair<float, float>(Now + 0.35f, 1.12f));
				UE_LOG(LogZoogi, Log, TEXT("WOLF ALPHA HOWL: P%d (%d pack KOs this round)"), Best + 1, BestN);
			}
	}
	if (Phase == EPhase::Paint)
	{
		// b101 film: the human's pick taps its targets one by one (rivals first, then the nearest orbs)
		static const bool bFilmTaps = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmWolfTaps"));
		if (bFilmTaps && bHumanOwner && Now - PhaseT0 > 1.8f + 1.1f * Painted.Num() && Painted.Num() < PaintNeed())
		{
			TArray<AActor*> Ts = Targets(Caster.Get());
			const FVector From = Caster.IsValid() ? Caster->BallLocation() : FVector::ZeroVector;
			Ts.Sort([&From](const AActor& A, const AActor& Bq) { const bool Ab = Cast<AZoogiBall>(&A) != nullptr, Bb = Cast<AZoogiBall>(&Bq) != nullptr; if (Ab != Bb) return Ab; return FVector::DistSquared2D(WolfPos(&A), From) < FVector::DistSquared2D(WolfPos(&Bq), From); });
			for (AActor* Tt : Ts) if (!Painted.Contains(Tt)) { bool bOk = false; const FVector2D Sp = Project(WolfPos(Tt), bOk); if (bOk) { PaintPick(Sp, PaintPx * 2.f); break; } }
		}
		// CPU: picks its targets one by one during the short howl
		if (!bHumanOwner && Now - PhaseT0 > 0.5f) Release();
		else if (Now - PhaseT0 > PaintLimit || Painted.Num() >= (bBirdPick ? PaintNeed() : PaintMax)) Release(); // b96d: the full amount (or time out -> auto-fill)
		if (!Caster.IsValid() || Caster->bOut) { EndBird(); RestoreDil(); Phase = EPhase::Idle; }
	}
	else if (Phase == EPhase::Out)
	{
		int32 Alive = 0;
		for (int32 i = 0; i < Wolves.Num(); ++i)
		{
			AZoogiClone* C = Wolves[i].Get(); if (!C) continue;
			++Alive;
			// #12 CPU parry: one roll per wolf as it closes on a CPU rival
			for (AZoogiBall* B : G->Balls)
			{
				if (!B || !B->bCPU || B->bOut || B->PlayerIndex == Owner || G->IsAlly(B, Caster.Get()) || CpuParryRolled.Contains(C)) continue;
				const FVector D = B->BallLocation() - C->GetActorLocation();
				if (D.Size2D() < 170.f && FVector::DotProduct(C->GetVelocity().GetSafeNormal2D(), D.GetSafeNormal2D()) > 0.4f)
				{
					CpuParryRolled.Add(C);
					if (FMath::FRand() < (C->bAlpha || C->bDire ? 0.08f : 0.18f))
					{
						WolfPopped[i] = 1;
						G->PublicPopup(TEXT("PARRY!"), B->BallLocation() + FVector(0, 0, 150.f), FLinearColor(0.55f, 0.95f, 1.f));
						ZoogiUtil::PlaySfx(this, TEXT("Clack"), 0.8f, 1.4f);
						C->Pop();
						UE_LOG(LogZoogi, Log, TEXT("WOLF CPU parry by P%d"), B->PlayerIndex + 1);
					}
				}
			}
		}
		// #10 hold to charge
		if (bHeld && !bCharging && Now - PressAt > 0.25f) { bCharging = true; ZoogiUtil::PlaySfx(this, TEXT("AwakenCharge"), 0.6f, 1.2f); }
		if (bCharging) for (const TWeakObjectPtr<AZoogiClone>& W : Wolves) if (AZoogiClone* C = W.Get()) C->ChargeMul = FMath::Min(1.8f, 1.f + (Now - PressAt - 0.25f) * 0.8f);
		if (!Alive) EndPack();
	}
	if (bTest) TestTick(G);
}

// ------------------------------------------------------------------ test script (-ZoogiWolfTest, with -ZoogiHumanTest)
void UZoogiWolfPackSys::TestTick(AZoogiGameMode* G)
{
	AZoogiBall* A = G->GetActiveBall();
	auto Shot = [&](const TCHAR* N) { if (!ShotDir.IsEmpty()) FScreenshotRequest::RequestScreenshot(FPaths::Combine(ShotDir, FString::Printf(TEXT("wolf_%02d_%s.png"), Shots++, N)), true, false); };
	const float Now = RealNow();
	if (!A || A->bCPU || A->bOut) return;
	// -ZoogiWolfLook: Micro Werewolf turntable (front / side / back) on a clean camera before the summon
	static int32 LookStep = 0; static float LookT = 0.f; static TArray<TWeakObjectPtr<AActor>> LookActors;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfLook")) && LookStep < 3 && G->Phase == EZoogiPhase::Aiming)
	{
		UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		if (LookStep == 0 && PC)
		{
			USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/MicroWerewolf/SK_MicroWerewolf.SK_MicroWerewolf"));
			UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/MicroWerewolf/A_MW_idle.A_MW_idle"));
			UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/MicroWerewolf/M_MicroWerewolf.M_MicroWerewolf"));
			float YawOff = 180.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWolfYaw="), YawOff);
			const FVector Ctr = A->GetActorLocation() - FVector(0.f, 0.f, 38.f);
			for (int32 i = 0; i < 3 && SK; ++i)
			{
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				ASkeletalMeshActor* Ac = W->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), FTransform(FRotator(0.f, i * 90.f + YawOff, 0.f), Ctr + FVector(0.f, (i - 1) * 140.f, 0.f), FVector(0.8f)), SP);
				if (!Ac) continue;
				USkeletalMeshComponent* Sk = Ac->GetSkeletalMeshComponent();
				Sk->SetMobility(EComponentMobility::Movable); Sk->SetSkeletalMesh(SK); if (M) Sk->SetMaterial(0, M);
				Sk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				if (Idle) { Sk->SetAnimationMode(EAnimationMode::AnimationSingleNode); Sk->PlayAnimation(Idle, true); }
				LookActors.Add(Ac);
			}
			if (!KillCam.IsValid()) { FActorSpawnParameters CP; CP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; KillCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, CP); }
			if (!PrevView.IsValid()) PrevView = PC->GetViewTarget();
			if (KillCam.IsValid()) PC->SetViewTargetWithBlend(KillCam.Get(), 0.f);
			if (ACameraActor* Cam = KillCam.Get())
			{
				const FVector Cl = Ctr + FVector(330.f, 0.f, 95.f);
				Cam->SetActorLocation(Cl); Cam->SetActorRotation((Ctr + FVector(0.f, 0.f, 45.f) - Cl).Rotation());
				KillCamEnd = RealNow() + 3.f; Cam->GetCameraComponent()->SetFieldOfView(60.f);
			}
			LookStep = 1; LookT = Now;
		}
		else if (LookStep == 1 && Now - LookT > 1.0f) { Shot(TEXT("look")); LookStep = 2; LookT = Now; }
		else if (LookStep == 2 && Now - LookT > 0.4f) { for (auto& L : LookActors) if (L.IsValid()) L->Destroy(); LookActors.Reset(); EndKillCam(); LookStep = 3; }
		return;
	}
	// -ZoogiStyTest: each Stylized VFX Pack effect on the court, one shot each (kill-cam framing)
	static int32 StyStep = 0; static float StyT = 0.f;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiStyTest")) && StyStep < 13 && (G->Phase == EZoogiPhase::Aiming || StyStep >= 12))
	{
		static const TCHAR* Fx[] = { TEXT("Big_Explosion"), TEXT("Mid_Explosion"), TEXT("Fire_v2"), TEXT("ElectricShock"), TEXT("Big_Explosion_v3"), TEXT("Smoke_v2") };
		const int32 Which = StyStep / 2;
		const FVector At = A->GetActorLocation() + FVector(0.f, 0.f, -36.f);
		if (Which >= UE_ARRAY_COUNT(Fx)) { if (Now - StyT > 4.5f) { Shot(TEXT("sty_after")); EndKillCam(); StyStep = 13; } else KillCamEnd = RealNow() + 1.f; return; } // b55: 4.5 s later every blast must be gone
		static bool bPre = false; // warm-up: spawn each once out of sight so the Niagara compile is done before the shots
		if (!bPre) { bPre = true; for (const TCHAR* F : Fx) AZoogiFX::Stylized(GetWorld(), F, At - FVector(0, 0, 3000.f), 0.1f); StyT = Now + 4.f; return; }
		if ((StyStep & 1) == 0 && Now - StyT > 0.6f)
		{
			AZoogiFX::Stylized(GetWorld(), Fx[Which], At, Which == 0 || Which == 4 ? 0.8f : 0.5f);
			if (StyStep == 0) { StartKillCam(At); }
			if (ACameraActor* Cam = KillCam.Get()) { const FVector Cl = At + FVector(520.f, 380.f, 330.f); Cam->SetActorLocation(Cl); Cam->SetActorRotation((At + FVector(0, 0, 90.f) - Cl).Rotation()); KillCamEnd = RealNow() + 5.f; }
			DilUntil = RealNow() + 0.01f;
			++StyStep; StyT = Now;
		}
		else if ((StyStep & 1) == 1 && Now - StyT > 0.35f) { Shot(*FString::Printf(TEXT("sty_%s"), Fx[Which])); ++StyStep; StyT = Now; }
		return;
	}
	switch (TestStep)
	{
	case 0: // the human's shot is rolling: summon
		if (G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.15f && Phase == EPhase::Idle) { A->Charges = FMath::Max(A->Charges, 2); G->bPowerUsed = false; if (G->TryPower()) { TestStep = 1; TestT = Now; } }
		break;
	case 1: // cut-in, then paint two targets
		if (Now - TestT > 0.25f && Shots == 0) Shot(TEXT("cutin"));
		if (Now - TestT > 0.6f && Painted.Num() == 0) { for (AActor* T : Targets(A)) if (Cast<AZoogiBall>(T)) { bool b; const FVector2D S = Project(WolfPos(T), b); if (b) { OnTap(S); break; } } }
		if (Now - TestT > 1.25f && Painted.Num() == 1) { for (AActor* T : Targets(A)) if (!Painted.Contains(T)) { bool b; const FVector2D S = Project(WolfPos(T), b); if (b) { OnTap(S); break; } } Shot(TEXT("paint")); }
		if (Phase == EPhase::Out) { TestStep = 2; TestT = Now; }
		break;
	case 2: // pack running
		if (Now - TestT > 0.35f) { Shot(TEXT("pack")); TestStep = 20; TestT = Now; for (const TWeakObjectPtr<AZoogiClone>& Wf : Wolves) if (Wf.IsValid()) { StartKillCam(Wf->GetActorLocation()); break; } }
		break;
	case 20: // Micro Werewolf close-up (kill-cam framing on the first wolf)
		if (ACameraActor* Cam = KillCam.Get()) for (const TWeakObjectPtr<AZoogiClone>& Wf : Wolves) if (Wf.IsValid()) // front view: in the wolf's path, looking back at it
		{
			const FVector Wl = Wf->GetActorLocation(); FVector Fw = Wf->Sphere->GetPhysicsLinearVelocity().GetSafeNormal2D(); if (Fw.IsNearlyZero()) Fw = FVector(1, 0, 0);
			const FVector Cl = Wl + Fw * 300.f + FVector::CrossProduct(FVector::UpVector, Fw) * 90.f + FVector(0, 0, 110.f);
			Cam->SetActorLocation(Cl); Cam->SetActorRotation((Wl + FVector(0, 0, 20.f) - Cl).Rotation()); break;
		}
		if (Now - TestT > 0.3f) { Shot(TEXT("close")); TestStep = 3; TestT = Now; }
		break;
	case 3: // hold to charge, release = blast
		if (Now - TestT > 0.3f && !bHeld) OnPowerPress();
		if (Now - TestT > 1.3f && bHeld) { Shot(TEXT("charge")); OnPowerRelease(); TestStep = 4; TestT = Now; }
		break;
	case 4:
		if (TallyAge < 0.5f) { Shot(TEXT("tally")); TestStep = 5; }
		break;
	default: break;
	}
}

// ------------------------------------------------------------------ HUD
// Nov 1 pass 2: HUD text in the P5 font (Anton), wrapped once in a runtime UFont so the canvas can draw it at real pixel size
static FSlateFontInfo WolfFont(float Px)
{
	static UFont* F = nullptr; static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const FSlateFontInfo F0 = ZoogiP5::Font(0, 24.f, 0.f);
		if (F0.CompositeFont.IsValid())
		{
			F = NewObject<UFont>(GetTransientPackage(), TEXT("ZoogiWolfHudFont"));
			F->FontCacheType = EFontCacheType::Runtime;
			F->CompositeFont = *F0.CompositeFont;
			F->AddToRoot();
		}
	}
	FSlateFontInfo Fi = F ? FSlateFontInfo(F, FMath::Max(6.f, FMath::RoundToFloat(Px)), FName(TEXT("Regular"))) : GEngine->GetLargeFont()->GetLegacySlateFontInfo();
	if (!F) { Fi.Size = FMath::Max(6.f, FMath::RoundToFloat(Px)); Fi.TypefaceFontName = FName(TEXT("Bold")); }
	Fi.OutlineSettings.OutlineSize = FMath::RoundToInt(FMath::Max(1.5f, Px * 0.09f));
	Fi.OutlineSettings.OutlineColor = FLinearColor(0.04f, 0.03f, 0.05f, 1.f);
	return Fi;
}

void UZoogiWolfPackSys::DrawWolfIcon(UCanvas* C, const FVector2D& At, float S, const FLinearColor& Col0, bool bCrown, bool bDim) const
{
	const FLinearColor Col = bDim ? FLinearColor(0.25f, 0.25f, 0.3f, 0.9f) : Col0;
	auto Tri = [&](const FVector2D& A, const FVector2D& B, const FVector2D& D, const FLinearColor& K)
	{
		FCanvasTriangleItem T(A, B, D, (C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr)); T.SetColor(K); T.BlendMode = SE_BLEND_Translucent; C->DrawItem(T);
	};
	auto Disc = [&](const FVector2D& Ctr, float R, const FLinearColor& K)
	{
		const int32 N = 18;
		for (int32 i = 0; i < N; ++i) { const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N; Tri(Ctr, Ctr + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, Ctr + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, K); }
	};
	const FLinearColor Ink(0.05f, 0.04f, 0.06f, 1.f);
	// outline
	Disc(At, S * 0.56f, Ink);
	Tri(At + FVector2D(-S * 0.55f, -S * 0.05f), At + FVector2D(-S * 0.42f, -S * 0.78f), At + FVector2D(-S * 0.08f, -S * 0.42f), Ink);
	Tri(At + FVector2D(S * 0.55f, -S * 0.05f), At + FVector2D(S * 0.42f, -S * 0.78f), At + FVector2D(S * 0.08f, -S * 0.42f), Ink);
	// head + ears + snout + eyes
	Disc(At, S * 0.48f, Col);
	Tri(At + FVector2D(-S * 0.46f, -S * 0.08f), At + FVector2D(-S * 0.40f, -S * 0.66f), At + FVector2D(-S * 0.12f, -S * 0.38f), Col);
	Tri(At + FVector2D(S * 0.46f, -S * 0.08f), At + FVector2D(S * 0.40f, -S * 0.66f), At + FVector2D(S * 0.12f, -S * 0.38f), Col);
	Disc(At + FVector2D(0.f, S * 0.18f), S * 0.22f, FLinearColor::LerpUsingHSV(Col, FLinearColor::White, 0.6f));
	Disc(At + FVector2D(0.f, S * 0.1f), S * 0.07f, Ink);
	Tri(At + FVector2D(-S * 0.28f, -S * 0.08f), At + FVector2D(-S * 0.10f, -S * 0.02f), At + FVector2D(-S * 0.22f, -S * 0.16f), Ink);
	Tri(At + FVector2D(S * 0.28f, -S * 0.08f), At + FVector2D(S * 0.10f, -S * 0.02f), At + FVector2D(S * 0.22f, -S * 0.16f), Ink);
	if (bCrown)
	{
		const FVector2D B = At + FVector2D(0.f, -S * 0.62f);
		for (int32 k = -1; k <= 1; ++k) Tri(B + FVector2D(k * S * 0.16f - S * 0.1f, 0.f), B + FVector2D(k * S * 0.16f + S * 0.1f, 0.f), B + FVector2D(k * S * 0.16f, -S * (k == 0 ? 0.32f : 0.24f)), PackGold);
	}
}

void UZoogiWolfPackSys::DrawHud(AHUD* HUD, UCanvas* C)
{
	if (!C || !HUD || HUD->GetWorld() != GetWorld()) return;
	if (UGameplayStatics::IsGamePaused(GetWorld())) return; // b79: nothing over the pause menu
	AZoogiGameMode* G = GM(); if (!G || !ZoogiWolf::On()) return;
	const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd / 1600.f, Ht / 900.f) * 1.f;
	UFont* Big = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Big) return;
	const FLinearColor Ink(0.04f, 0.03f, 0.05f, 1.f);
	// Scale 1 = 24 px at 1600x900
	auto Text = [&](const FString& S, float X, float Y, float Scale, const FLinearColor& Col, bool bCenter = true, bool bShadow = true)
	{
		if (S.IsEmpty() || Col.A <= 0.01f) return;
		const FSlateFontInfo Fi = WolfFont(24.f * Scale);
		FVector2D Sz(0.f, 0.f);
		if (FSlateApplication::IsInitialized()) Sz = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Fi);
		FCanvasTextItem T(FVector2D(bCenter ? X - Sz.X * 0.5f : X, Y - Sz.Y * 0.5f), FText::FromString(S), Fi, Col);
		if (bShadow) T.EnableShadow(FLinearColor(Ink.R, Ink.G, Ink.B, 0.8f * Col.A), FVector2D(3.f, 4.f) * U);
		C->DrawItem(T);
	};
	auto Quad = [&](const FVector2D& A, const FVector2D& B, const FVector2D& D, const FVector2D& E, const FLinearColor& K)
	{
		FCanvasTriangleItem T1(A, B, D, (C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr)); T1.SetColor(K); T1.BlendMode = SE_BLEND_Translucent; C->DrawItem(T1);
		FCanvasTriangleItem T2(A, D, E, (C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr)); T2.SetColor(K); T2.BlendMode = SE_BLEND_Translucent; C->DrawItem(T2);
	};
	// b119 (Evan: "add the wolf avi icon above the purple orbs - they act as the wolves"): the pack's wolf head over each orb
	if (APlayerController* IconPC = HUD->GetOwningPlayerController())
		for (const TWeakObjectPtr<AZoogiClone>& Wk : [this]() { TArray<TWeakObjectPtr<AZoogiClone>> All = Wolves; for (const auto& Kv : FieldWolves) All.Append(Kv.Value); return All; }())
		{
			const AZoogiClone* Cl = Wk.Get();
			if (!Cl || Cl->IsHidden() || (bShot && !Cl->bWaiting)) continue; // b119: missiles carry no wolf icon; b119c: waiting wolves always do
			FVector2D Sp;
			if (!IconPC->ProjectWorldLocationToScreen(Cl->GetActorLocation() + FVector(0.f, 0.f, 95.f * (Cl->bAlpha || Cl->bDire ? 1.3f : 1.f)), Sp, true)) continue;
			if (Sp.X < -60.f || Sp.Y < -60.f || Sp.X > Wd + 60.f || Sp.Y > Ht + 60.f) continue;
			DrawWolfIcon(C, Sp, (Cl->bAlpha || Cl->bDire ? 46.f : 36.f) * U, FLinearColor(0.62f, 0.3f, 1.f), Cl->bAlpha || Cl->bDire, false);
		}
	// #1 P5 cut-in: ink band + colour slash + huge title + wolf head, slides in, holds, slides out
	if (CutInAge < 1.1f)
	{
		const float In = FMath::Clamp(CutInAge / 0.18f, 0.f, 1.f), Out = FMath::Clamp((1.1f - CutInAge) / 0.2f, 0.f, 1.f);
		const float K = FMath::Min(In, Out);
		const float Cy = Ht * 0.25f, Hb = 130.f * U * K;
		const float Sx = (1.f - In) * -Wd + (1.f - Out) * Wd;
		Quad(FVector2D(Sx - 50.f, Cy - Hb * 0.5f + 30.f * U), FVector2D(Sx + Wd + 50.f, Cy - Hb * 0.5f - 30.f * U), FVector2D(Sx + Wd + 50.f, Cy + Hb * 0.5f - 30.f * U), FVector2D(Sx - 50.f, Cy + Hb * 0.5f + 30.f * U), FLinearColor(0.03f, 0.02f, 0.05f, 0.92f));
		Quad(FVector2D(Sx - 50.f, Cy + Hb * 0.5f + 24.f * U), FVector2D(Sx + Wd + 50.f, Cy + Hb * 0.5f - 36.f * U), FVector2D(Sx + Wd + 50.f, Cy + Hb * 0.5f - 18.f * U), FVector2D(Sx - 50.f, Cy + Hb * 0.5f + 42.f * U), CutInCol);
		if (K > 0.6f)
		{
			DrawWolfIcon(C, FVector2D(Sx + Wd * 0.18f, Cy), 120.f * U, CutInCol, bDire, false);
			Text(CutInTitle, Sx + Wd * 0.55f, Cy - 10.f * U, 3.6f * U, FLinearColor::White);
			Text(CutInSub, Sx + Wd * 0.55f, Cy + 46.f * U, 1.1f * U, CutInCol);
		}
	}
	// #6 paint prompt + target rings
	if (Phase == EPhase::Paint && bHumanOwner)
	{
		const float Left = FMath::Max(0.f, PaintLimit - (RealNow() - PhaseT0));
		if (bBirdPick) Text(FString::Printf(TEXT("PICK %d TARGET%s  -  %d / %d   %d s"), PaintNeed(), PaintNeed() == 1 ? TEXT("") : TEXT("S"), Painted.Num(), PaintNeed(), FMath::CeilToInt(Left)), Wd * 0.5f, Ht * 0.9f, 1.35f * U, FLinearColor::White);
		else Text(FString::Printf(TEXT("TAP UP TO %d TARGETS  -  %d / %d PICKED"), PaintMax, Painted.Num(), PaintMax), Wd * 0.5f, Ht * 0.66f, 1.25f * U, FLinearColor::White);
		const float Bw = 420.f * U, Bx = Wd * 0.5f - Bw * 0.5f, By = bBirdPick ? Ht * 0.93f : Ht * 0.70f;
		Quad(FVector2D(Bx, By), FVector2D(Bx + Bw, By), FVector2D(Bx + Bw, By + 10.f * U), FVector2D(Bx, By + 10.f * U), FLinearColor(0.f, 0.f, 0.f, 0.6f));
		Quad(FVector2D(Bx, By), FVector2D(Bx + Bw * Left / PaintLimit, By), FVector2D(Bx + Bw * Left / PaintLimit, By + 10.f * U), FVector2D(Bx, By + 10.f * U), CutInCol);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(RealNow() * 10.f);
		TArray<AActor*> Ring = Targets(Caster.Get());
		{
			const FVector From = Caster.IsValid() ? Caster->GetActorLocation() : FVector::ZeroVector;
			TArray<AActor*> Orbs, Keep;
			for (AActor* T : Ring) { if (Cast<AZoogiBall>(T) || Painted.Contains(T)) Keep.Add(T); else Orbs.Add(T); }
			Orbs.Sort([&](const AActor& A, const AActor& B) { return FVector::DistSquared2D(A.GetActorLocation(), From) < FVector::DistSquared2D(B.GetActorLocation(), From); });
			for (int32 i = 0; i < FMath::Min(bBirdPick ? 40 : 8, Orbs.Num()); ++i) Keep.Add(Orbs[i]); // b96d: every target on the arena
			Ring = MoveTemp(Keep);
		}
		for (AActor* T : Ring)
		{
			bool bOk; const FVector2D S = Project(WolfPos(T), bOk); if (!bOk) continue;
			const int32 Idx = Painted.IndexOfByKey(T);
			const float R = (Idx == 0 ? 46.f : 34.f) * U * (Idx == INDEX_NONE ? 0.9f + 0.15f * Pulse : 1.1f);
			const FLinearColor K = Idx == 0 ? PackGold : (Idx > 0 ? CutInCol : FLinearColor(1.f, 1.f, 1.f, 0.55f + 0.3f * Pulse));
			for (int32 i = 0; i < 24; ++i) { const float A0 = 2.f * PI * i / 24, A1 = 2.f * PI * (i + 1) / 24; C->K2_DrawLine(S + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, S + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R, 4.f * U, K); }
			if (Idx != INDEX_NONE) Text(Idx == 0 ? TEXT("ALPHA") : FString::FromInt(Idx + 1), S.X, S.Y - R - 16.f * U, 0.9f * U, K);
		}
	}
	// #20 pack counter: one wolf head per wolf, lit on a hit, dark when parried
	if (Phase == EPhase::Out || (TallyAge < 1.8f))
	{
		const int32 N = FMath::Max(Wolves.Num(), WolfHits.Num());
		const float S = 40.f * U, X0 = 70.f * U, Y = 250.f * U;
		if (N > 0)
		{
			Quad(FVector2D(X0 - S * 1.4f, Y - S * 0.85f), FVector2D(X0 + (N - 1) * S * 1.24f + S * 1.4f, Y - S * 0.95f), FVector2D(X0 + (N - 1) * S * 1.24f + S * 1.2f, Y + S * 0.75f), FVector2D(X0 - S * 1.2f, Y + S * 0.8f), FLinearColor(0.03f, 0.02f, 0.05f, 0.75f));
			for (int32 i = 0; i < N; ++i)
			{
				const bool bHit = WolfHits.IsValidIndex(i) && WolfHits[i] > 0, bPop = WolfPopped.IsValidIndex(i) && WolfPopped[i];
				DrawWolfIcon(C, FVector2D(X0 + i * S * 1.24f, Y + 4.f * U), S * (i == 0 && !bDire ? 1.12f : 0.95f), bHit ? TallyCol : FLinearColor(TallyCol.R * 0.45f, TallyCol.G * 0.45f, TallyCol.B * 0.45f, 1.f), i == 0 && !bDire, bPop);
			}
			Text(TEXT("WOLF PACK"), X0 - S * 0.9f, Y - S * 1.2f, 0.8f * U, FLinearColor::White, false);
		}
		if (TallyAge < 1.8f && Phase != EPhase::Out)
		{
			const float Sl = 1.f + 0.4f * FMath::Clamp(1.f - TallyAge / 0.2f, 0.f, 1.f);
			Text(FString::Printf(TEXT("PACK SCORE x%d"), TallyScore), X0 - S * 0.9f, Y + S * 1.4f, 1.5f * U * Sl, PackGold, false);
		}
	}
	// #12 parry prompt
	if (ParryOpen()) Text(TEXT("PARRY!  TAP REACTION"), Wd * 0.5f, Ht * 0.78f, 1.4f * U * (1.f + 0.08f * FMath::Sin(RealNow() * 18.f)), FLinearColor(0.55f, 0.95f, 1.f));
	// #10 charging
	if (bCharging) Text(TEXT("CHARGING  -  RELEASE TO BLOW"), Wd * 0.5f, Ht * 0.72f, 1.2f * U, PackGold);
	// toasts (PACK HUNT, PILE-ON stamps live as popups; these are the big ones)
	float Ty = Ht * 0.36f; // below the game's own centre banners
	for (const FToast& T : Toasts)
	{
		const float In = FMath::Clamp(T.Age / 0.15f, 0.f, 1.f), Out = FMath::Clamp((T.Life - T.Age) / 0.25f, 0.f, 1.f);
		const float A = FMath::Min(In, Out);
		const FLinearColor K(T.Col.R, T.Col.G, T.Col.B, A);
		Text(T.Text, Wd * 0.5f, Ty, 2.f * U * T.Size * (1.f + 0.3f * (1.f - In)), K);
		if (!T.Sub.IsEmpty()) Text(T.Sub, Wd * 0.5f, Ty + 44.f * U * T.Size, 0.9f * U, FLinearColor(1.f, 1.f, 1.f, A));
		Ty += 100.f * U * T.Size;
	}
}
