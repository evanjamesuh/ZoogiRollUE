// Oct 24 (Evan): star bumpers on every map + a movable layout (dev mode, ZoogiDevMode.h) that can be saved to
// C:\dev\zoogi-layouts\<slug>.json and baked back into ZoogiLayoutBaked.inc (C:\dev\bake-layouts.ps1).
// Bumpers use the Teaser rules: invisible collider r322 / h140 (world cm) + the 0.1.2 SM_StarBumper visual, restitution 0.55.
#include "ZoogiArena.h"
#include "ZoogiLaunchPad.h"
#include "ZoogiDevMode.h"
#include "ZoogiDevConfig.h"
#include "ZoogiRoll.h"
#include "ZoogiPhysics.h"
#include "ZoogiBumpers.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiObstacle.h"
#include "ZoogiRock.h"
#include "ZoogiAerialProps.h"
#include "EngineUtils.h"
#include "ZoogiStageForming.h"
#include "ZoogiFrozenFun.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
	struct FBakedProp { const TCHAR* Slug; const TCHAR* Type; float X, Y, Z, Yaw, Scale; };
	static const FBakedProp GBakedProps[] = {
#define ZOOGI_BAKED(Slug, Type, X, Y, Z, Yaw, Scale) { Slug, Type, X, Y, Z, Yaw, Scale },
#include "ZoogiLayoutBaked.inc"
#undef ZOOGI_BAKED
		{ nullptr, nullptr, 0, 0, 0, 0, 0 }
	};
	constexpr float StarR = 322.f, StarH = 140.f, StarRest = 0.55f; // world cm (Teaser)
}

FString AZoogiArena::GetArenaSlug() const
{
	static const TCHAR* Slugs[] = { TEXT("meadow"), TEXT("frozen"), TEXT("volcanic"), TEXT("cosmic"), TEXT("arabian"), TEXT("tomb"), TEXT("neon"), TEXT("training"), TEXT("teaser"), TEXT("ancient") };
	const int32 I = (int32)Theme;
	return (I >= 0 && I < UE_ARRAY_COUNT(Slugs)) ? FString(Slugs[I]) : FString::FromInt(I);
}

int32 AZoogiArena::AddStarBumperProp(const FVector& WorldLoc, float Yaw, float Scale, const FString& Id)
{
	FZoogiDevProp P;
	P.Type = TEXT("star_bumper");
	P.Id = Id;
	if (P.Id.IsEmpty())
	{
		// stable ids: bumper_1, bumper_2, ... in creation order (the same every run for the same layout)
		int32 MaxN = 0;
		for (const FZoogiDevProp& O : DevProps) if (O.Id.StartsWith(TEXT("bumper_"))) MaxN = FMath::Max(MaxN, FCString::Atoi(*O.Id.RightChop(7)));
		P.Id = FString::Printf(TEXT("bumper_%d"), MaxN + 1);
	}
	P.Location = FVector(WorldLoc.X, WorldLoc.Y, 0.f);
	P.Yaw = Yaw;
	P.Scale = FMath::Clamp(Scale, 0.25f, 4.f);
	const int32 Idx = DevProps.Add(P);
	UpdateDevProp(Idx);
	return Idx;
}

void AZoogiArena::RemoveDevProp(int32 Index)
{
	if (!DevProps.IsValidIndex(Index) || IsFixedPropType(DevProps[Index].Type)) return; // ship / spawn / launch points move, never vanish
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : DevProps[Index].Comps)
	{
		if (C.IsValid()) { Built.Remove(C.Get()); C->DestroyComponent(); }
	}
	DevProps.RemoveAt(Index);
}

void AZoogiArena::ClearStarBumperProps()
{
	for (int32 i = DevProps.Num() - 1; i >= 0; --i) if (DevProps[i].Type == TEXT("star_bumper")) RemoveDevProp(i);
}

int32 AZoogiArena::AddLateForming()
{
	// Oct 30 (Evan, build 28): "in the opening stage animation include all new map additions"
	if (!Forming) return 0;
	UWorld* W = GetWorld();
	const float AS = FMath::Max(GetActorScale3D().X, 0.01f);
	int32 N = 0, Star = 0, Obs = 0;
	// Oct 31: Ancient Collision Meadows obstacles ARE the floors / bridges / pads - lowering them for the rise intro dropped every
	// Zoogi and orb into the void before the first shot. Only its bumpers / rebounders join the intro.
	// Oct 31 b41 (Evan: "a 14 hit combo instantly on first launch"): the same drop hit Frozen Ring 2 - Ring2_Floor / ramps / curbs were
	// lowered too, the 12 outer-ring orbs fell below FallOutZ during the intro and the first shot scored them all as one COMBO x12+.
	// Anything that holds orbs or Zoogis up (floors, ramps, curbs, lips, banks, decks, bridges, or any wide flat collider) stays put.
	auto Structural = [this](const AZoogiObstacle* O)
	{
		if (!O) return false;
		const FString N = O->ObsName.ToString();
		if (N.Contains(TEXT("bumper")) || N.Contains(TEXT("rebounder"))) return false;
		if (Theme == EZoogiArena::Ancient) return true;
		static const TCHAR* Holds[] = { TEXT("Floor"), TEXT("Ramp"), TEXT("Curb"), TEXT("Lip"), TEXT("Bank"), TEXT("Deck"), TEXT("Bridge"), TEXT("Ledge"), TEXT("Path"), TEXT("Platform") };
		for (const TCHAR* H : Holds) if (N.Contains(H)) return true;
		FVector Org, Ext; O->GetActorBounds(true, Org, Ext);
		return FMath::Max(Ext.X, Ext.Y) > 500.f && Ext.Z * 4.f < FMath::Max(Ext.X, Ext.Y);
	};
	for (const FZoogiDevProp& P : DevProps)
	{
		if (P.Type == TEXT("star_bumper"))
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& C : P.Comps)
				if (UPrimitiveComponent* Pc = C.Get())
					if (Pc->GetVisibleFlag() && !Forming->Contains(Pc)) { Forming->AddComponent(Pc, EZoogiFormStyle::Rise, FVector(0.f, 0.f, -(StarH * P.Scale + 90.f) / AS), 0.9f + 0.12f * Star, 0.6f, 10.f); ++N; }
			++Star;
		}
		else if (P.Type == TEXT("obstacle"))
			if (AZoogiObstacle* O = Cast<AZoogiObstacle>(P.Actor.Get()))
				if (!Forming->Contains(O) && !Structural(O))
				{
					FVector Org, Ext; O->GetActorBounds(true, Org, Ext);
					Forming->AddActor(O, EZoogiFormStyle::Rise, FVector(0.f, 0.f, -(Ext.Z * 2.f + 120.f)), 0.45f + 0.06f * Obs++, 0.7f, 6.f); ++N;
				}
	}
	for (AZoogiObstacle* O : Obstacles)
		if (O && !Forming->Contains(O) && !Structural(O))
		{
			FVector Org, Ext; O->GetActorBounds(true, Org, Ext);
			Forming->AddActor(O, EZoogiFormStyle::Rise, FVector(0.f, 0.f, -(Ext.Z * 2.f + 120.f)), 0.45f + 0.06f * Obs++, 0.7f, 6.f); ++N;
		}
	if (W) for (TActorIterator<AZoogiFrozenCrowd> It(W); It; ++It) if (!Forming->Contains(*It)) { Forming->AddActor(*It, EZoogiFormStyle::Rise, FVector(0.f, 0.f, -260.f), 1.7f, 0.9f, 4.f); ++N; }
	if (LaunchPad && !Forming->Contains(LaunchPad)) { Forming->AddActor(LaunchPad, EZoogiFormStyle::Drop, FVector(0.f, 0.f, 1800.f), 1.0f, 1.1f, 8.f); ++N; }
	UE_LOG(LogZoogi, Log, TEXT("STAGE FORMING: +%d newer map additions (%d star bumpers, %d obstacles%s)"), N, Star, Obs, LaunchPad ? TEXT(", ship pad") : TEXT(""));
	return N;
}

void AZoogiArena::UpdateDevProp(int32 Index)
{
	if (!DevProps.IsValidIndex(Index)) return;
	FZoogiDevProp& P = DevProps[Index];
	if (P.Type == TEXT("spawn") || P.Type == TEXT("launch")) { UpdateRoamProp(Index); return; } // Oct 27 Frozen Ring roam
	if (P.Type == TEXT("obstacle"))
	{
		// Oct 29: move / turn / size the real actor (its colliders come along); pitch / roll stay as built
		if (AActor* Ac = P.Actor.Get())
		{
			const FRotator BR = P.Base.Rotator();
			Ac->SetActorTransform(FTransform(FRotator(BR.Pitch, P.Yaw, BR.Roll), P.Location, P.Base.GetScale3D() * P.Scale), false, nullptr, ETeleportType::TeleportPhysics);
		}
		ZoogiBumpers::Sync(this, Index);
		return;
	}
	if (P.Type == TEXT("ship"))
	{
		if (!LaunchPad) return;
		const FVector2D XY(P.Location);
		FVector2D Dir = (-XY).GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = FVector2D(1, 0);
		const float KR = GetKnockoffRadius();
		FVector2D Land = XY + Dir * ZoogiLaunch::FlightDist;
		if (IsOffCourt(Land) || Land.Size() > KR * 0.6f) Land = -Dir * (KR * 0.45f); // keep the landing on the field
		LaunchPad->Relocate(FVector(XY, ZoogiLaunch::PadHeight), P.Yaw, P.Scale, FVector(Land, ZoogiConst::ZoogiRadius));
		return;
	}
	// star bumper: rebuild its collider + visual (local actor space; the arena may be scaled 1.3x)
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : P.Comps) if (C.IsValid()) { Built.Remove(C.Get()); C->DestroyComponent(); }
	P.Comps.Reset();
	if (!StarBumperPM) StarBumperPM = ZoogiPhysics::MakeMaterial(this, 0.3f, StarRest, false);
	const float AS = FMath::Max(GetActorScale3D().X, 0.01f);
	const FVector L = GetActorTransform().InverseTransformPosition(FVector(P.Location.X, P.Location.Y, GetActorLocation().Z));
	const float R = StarR * P.Scale / AS, H = StarH * P.Scale / AS;
	UStaticMeshComponent* Col = AddCylinder(FVector2D(L), R, H, H, nullptr, true);
	Col->SetVisibility(false);
	Col->SetPhysMaterialOverride(StarBumperPM);
	Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Col->ComponentTags.Add(TEXT("NoCamBlock"));
	Col->ComponentTags.Add(TEXT("ZoogiStarBumper"));
	P.Comps.Add(Col);
	static TWeakObjectPtr<UStaticMesh> Cached;
	UStaticMesh* Star = Cached.Get();
	if (!Star) { Star = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Legacy012/SM_StarBumper.SM_StarBumper")); Cached = Star; }
	UStaticMeshComponent* V = nullptr;
	if (Star)
	{
		const FBox B = Star->GetBoundingBox();
		const FVector Ext = B.GetExtent();
		const float SXY = R / FMath::Max(FMath::Max(Ext.X, Ext.Y), 1.f), SZ = H / FMath::Max(Ext.Z * 2.f, 1.f);
		V = AddMesh(Star, nullptr, false);
		V->SetRelativeScale3D(FVector(SXY, SXY, SZ));
		V->SetRelativeRotation(FRotator(0, P.Yaw, 0));
		V->SetRelativeLocation(FVector(L.X, L.Y, -B.Min.Z * SZ));
	}
	else
	{
		V = AddMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")), ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#D13F38")), 0.5f), false);
		V->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, H / 100.f));
		V->SetRelativeLocation(FVector(L.X, L.Y, H * 0.5f));
	}
	V->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	V->ComponentTags.Add(TEXT("NoCamBlock"));
	P.Comps.Add(V);
	ZoogiBumpers::Sync(this, Index); // the collider is new: re-apply an active bumper setting
}

bool AZoogiArena::FZoogiDevProp::IsMovedObstacle() const
{
	if (Type != TEXT("obstacle")) return false;
	return FVector::Dist(Location, Base.GetLocation()) > 0.5f || FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Base.Rotator().Yaw)) > 0.05f || FMath::Abs(Scale - 1.f) > 0.001f;
}

void AZoogiArena::RegisterObstacleProps()
{
	UWorld* W = GetWorld();
	if (!W) return;
	const bool bFirst = !bObstaclesRegistered;
	bObstaclesRegistered = true;
	const float KR = GetKnockoffRadius();
	TSet<const AActor*> Have;
	TMap<FString, int32> Counts;
	for (const FZoogiDevProp& P : DevProps)
		if (P.Type == TEXT("obstacle"))
		{
			if (P.Actor.IsValid()) Have.Add(P.Actor.Get());
			FString K; FString N;
			if (P.Id.Split(TEXT("_"), &K, &N, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) Counts.FindOrAdd(K) = FMath::Max(Counts.FindRef(K), FCString::Atoi(*N));
		}
	int32 Added = 0;
	auto Reg = [&](AActor* Ac, FString Kind)
	{
		if (!IsValid(Ac) || Have.Contains(Ac)) return;
		const FVector L = Ac->GetActorLocation();
		if (FVector2D(L).Size() > KR * 1.35f) return; // the court and its rim, not the scenery
		Kind = Kind.ToLower().Replace(TEXT(" "), TEXT("-"));
		const int32 N = ++Counts.FindOrAdd(Kind);
		FZoogiDevProp P;
		P.Type = TEXT("obstacle");
		P.Id = FString::Printf(TEXT("%s_%d"), *Kind, N);
		P.Actor = Ac;
		P.Base = Ac->GetActorTransform();
		P.Location = L;
		P.Yaw = Ac->GetActorRotation().Yaw;
		P.Scale = 1.f;
		FVector O, E;
		Ac->GetActorBounds(false, O, E);
		P.PickZ = E.IsNearlyZero() ? 70.f : FMath::Clamp((float)(O.Z - L.Z), 20.f, 400.f);
		DevProps.Add(P);
		Have.Add(Ac);
		++Added;
	};
	static const TCHAR* Skip[] = { TEXT("stand"), TEXT("islet_rail"), TEXT("bridge"), TEXT("rim_ice"), TEXT("rimgap"), TEXT("floor"), TEXT("piece") };
	for (AZoogiArenaPiece* Pc : Pieces)
	{
		if (!Pc) continue;
		const FString K = Pc->GetKind().ToString();
		bool bSkip = false;
		for (const TCHAR* S : Skip) if (K == S) bSkip = true;
		if (!bSkip) Reg(Pc, K.StartsWith(TEXT("pillar")) ? FString(TEXT("pillar")) : K);
	}
	for (AZoogiObstacle* Ob : Obstacles) if (Ob) Reg(Ob, Ob->ObsName.IsNone() ? FString(TEXT("obstacle")) : Ob->ObsName.ToString());
	for (AZoogiRock* Rk : Rocks) if (Rk) Reg(Rk, TEXT("rock"));
	for (TActorIterator<AZoogiAerialProp> It(W); It; ++It)
	{
		AZoogiAerialProp* Pr = *It;
		if (!Pr || Pr->bAerialRing) continue;
		Reg(Pr, Pr->Kind == EZoogiPropKind::SkyBumper ? TEXT("sky_bumper") : (Pr->Kind == EZoogiPropKind::ResonanceHalo ? TEXT("halo") : (Pr->Kind == EZoogiPropKind::Gate ? TEXT("gate") : TEXT("launch_rail"))));
	}
	// saved dev-layout entries for obstacles that did not exist yet
	for (int32 i = PendingObstacles.Num() - 1; i >= 0; --i) if (ApplyObstacleEntry(PendingObstacles[i])) PendingObstacles.RemoveAt(i);
	if (Added) UE_LOG(LogZoogi, Log, TEXT("LAYOUT: %s %d obstacle(s) are dev objects%s (%d pending saved edits)"), *GetArenaSlug(), Added, bFirst ? TEXT("") : TEXT(" (late)"), PendingObstacles.Num());
}

bool AZoogiArena::ApplyObstacleEntry(const FZoogiDevProp& E)
{
	for (int32 i = 0; i < DevProps.Num(); ++i)
	{
		FZoogiDevProp& P = DevProps[i];
		if (P.Type != TEXT("obstacle") || P.Id != E.Id) continue;
		P.Location = E.Location; P.Yaw = E.Yaw; P.Scale = E.Scale; P.bBumper = E.bBumper; P.Bounce = E.Bounce;
		UpdateDevProp(i);
		return true;
	}
	return false;
}

void AZoogiArena::PlaceStarProps()
{
	// b62 (Evan: "Delete all red star bumpers from everywhere"): off unless -ZoogiStarProps (saved layouts / Teaser / stage shift too)
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiStarProps"))) return;
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoStarProps"))) return;
	if (Theme == EZoogiArena::Teaser) return; // Teaser builds its own four (BuildTeaser -> AddStarBumperProp)
	if (Theme == EZoogiArena::Ancient) return; // Oct 31: Ancient has its own hub bumpers / rebounders
	if (Theme == EZoogiArena::Frozen && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFrozenFun"))) return; // Oct 29 (Evan, build 20): no red bumpers on the Frozen Ring
	UWorld* W = GetWorld();
	if (!W) return;
	// Two default bumpers on the far side of the Tac camera (camera on +Y), between the 0/90/180/270 spawns.
	const float KR = GetKnockoffRadius();
	auto Pol = [](float R, float YawDeg) { const float A = FMath::DegreesToRadians(YawDeg); return FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R); };
	static const float BaseYaws[] = { 225.f, 315.f };
	static const float Nudge[] = { 0.f, 12.f, -12.f, 24.f, -24.f, 36.f, -36.f };
	static const float Fr[] = { 0.62f, 0.55f, 0.70f, 0.48f };
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiStarProps), false, this);
	FCollisionObjectQueryParams All(FCollisionObjectQueryParams::AllObjects);
	int32 Placed = 0;
	for (float BY : BaseYaws)
	{
		bool bDone = false;
		for (float F : Fr) { for (float N : Nudge)
		{
			if (bDone) break;
			const FVector2D C = Pol(KR * F, BY + N);
			if (IsOffCourt(C + C.GetSafeNormal() * (StarR + 60.f))) continue;
			bool bOk = true;
			for (int32 s = 0; s < 4 && bOk; ++s) bOk = FVector2D::Distance(C, FVector2D(GetSpawnPoint(s))) > StarR + 260.f;
			for (const FZoogiDevProp& O : DevProps) if (bOk) bOk = FVector2D::Distance(C, FVector2D(O.Location)) > StarR * 2.f + 250.f;
			if (!bOk) continue;
			// nothing solid in the footprint above the floor (obstacles, pieces, posts, the orb hub)
			if (W->OverlapAnyTestByObjectType(FVector(C, 140.f), FQuat::Identity, All, FCollisionShape::MakeBox(FVector(StarR + 40.f, StarR + 40.f, 60.f)), Q)) continue;
			AddStarBumperProp(FVector(C, 0.f), 0.f, 1.f);
			++Placed; bDone = true;
		} }
	}
	UE_LOG(LogZoogi, Log, TEXT("STAR PROPS: %s placed %d default star bumpers"), *GetArenaSlug(), Placed);
}

void AZoogiArena::ApplyLayout(const TArray<FZoogiDevProp>& Layout)
{
	ClearStarBumperProps(); // a layout lists every bumper on the map
	// Oct 29: obstacles go back to how they were built; the layout lists only the ones that were changed
	PendingObstacles.Reset();
	for (int32 i = 0; i < DevProps.Num(); ++i)
	{
		FZoogiDevProp& O = DevProps[i];
		if (O.Type != TEXT("obstacle") || (!O.IsMovedObstacle() && !O.bBumper)) continue;
		O.Location = O.Base.GetLocation(); O.Yaw = O.Base.Rotator().Yaw; O.Scale = 1.f; O.bBumper = false; O.Bounce = 5;
		UpdateDevProp(i);
	}
	for (const FZoogiDevProp& P : Layout)
	{
		if (P.Type == TEXT("star_bumper"))
		{
			if (Theme == EZoogiArena::Frozen && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFrozenFun"))) continue; // Oct 29: no red bumpers on the Frozen Ring
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiStarProps"))) continue; // b62: red star bumpers removed everywhere
			const int32 I = AddStarBumperProp(P.Location, P.Yaw, P.Scale, P.Id);
			if (DevProps.IsValidIndex(I) && P.bBumper) { DevProps[I].bBumper = true; DevProps[I].Bounce = P.Bounce; ZoogiBumpers::Sync(this, I); }
		}
		else if (P.Type == TEXT("obstacle")) { if (!ApplyObstacleEntry(P)) PendingObstacles.Add(P); }
		else if (P.Type == TEXT("spawn") || P.Type == TEXT("launch"))
		{
			for (int32 i = 0; i < DevProps.Num(); ++i) if (DevProps[i].Type == P.Type && DevProps[i].Id == P.Id)
			{
				DevProps[i].Location = P.Location; DevProps[i].Yaw = P.Yaw; DevProps[i].Scale = P.Scale;
				UpdateDevProp(i);
			}
		}
		else if (P.Type == TEXT("ship"))
		{
			for (int32 i = 0; i < DevProps.Num(); ++i) if (DevProps[i].Type == TEXT("ship"))
			{
				DevProps[i].Location = P.Location; DevProps[i].Yaw = P.Yaw; DevProps[i].Scale = P.Scale;
				UpdateDevProp(i);
			}
		}
	}
}

void AZoogiArena::ApplyLayoutOverrides()
{
	AZoogiDevMode::Ensure(GetWorld()); // dev-mode toggles (F10 / 5 taps) + overlay when -ZoogiDev
	// the ship (launch pad) is a dev prop too
	if (LaunchPad)
	{
		FZoogiDevProp S; S.Type = TEXT("ship"); S.Id = TEXT("ship"); S.Location = LaunchPad->GetActorLocation(); S.Yaw = LaunchPad->GetActorRotation().Yaw; S.Scale = LaunchPad->ShipScale;
		DevProps.Add(S);
	}
	const FString Slug = GetArenaSlug();
	TArray<FZoogiDevProp> Baked;
	for (const FBakedProp& B : GBakedProps)
	{
		if (!B.Slug || Slug != B.Slug) continue;
		FZoogiDevProp P; P.Type = FName(B.Type); P.Location = FVector(B.X, B.Y, B.Z); P.Yaw = B.Yaw; P.Scale = B.Scale;
		Baked.Add(P);
	}
	if (Baked.Num()) { ApplyLayout(Baked); UE_LOG(LogZoogi, Log, TEXT("LAYOUT: %s applied %d baked props"), *Slug, Baked.Num()); }
	BaselineProps = DevProps; // the shipped layout: Dev Mode RESET returns here
	// Oct 27: a dev-unlocked device runs its saved dev layout (ZoogiDevConfig, app storage); ordinary players never do
	if (ZoogiDevGate::IsUnlocked())
	{
		TArray<FZoogiDevProp> Dev;
		if (ZoogiLayout::FromJson(ZoogiDevConfig::Get(TEXT("level:") + Slug, ZoogiLayout::LayoutKey()), Dev) && Dev.Num())
		{
			ApplyLayout(Dev);
			UE_LOG(LogZoogi, Log, TEXT("LAYOUT: %s dev store override applied (%d props)"), *Slug, Dev.Num());
			return;
		}
	}
	if (ZoogiLayout::IsDevMode())
	{
		TArray<FZoogiDevProp> Dev;
		if (ZoogiLayout::Load(Slug, Dev)) { ApplyLayout(Dev); UE_LOG(LogZoogi, Log, TEXT("LAYOUT: %s dev JSON override applied (%d props) from %s"), *Slug, Dev.Num(), *ZoogiLayout::PathFor(Slug)); }
		else UE_LOG(LogZoogi, Log, TEXT("LAYOUT: %s no dev JSON at %s"), *Slug, *ZoogiLayout::PathFor(Slug));
	}
}
