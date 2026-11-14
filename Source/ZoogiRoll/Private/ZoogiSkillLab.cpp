#include "ZoogiSkillLab.h"
#include "ZoogiRoll.h"
#include "ZoogiSkills.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiActionFun.h"
#include "ZoogiWolfPack.h"
#include "ZoogiArenaReact.h"
#include "ZoogiPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);
extern float GZoogiWcYawTest; // b142c (ZoogiSkills.cpp)
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);

namespace ZoogiSkillLab
{
	namespace
	{
		struct FTune { float Strength = 1.f, Radius = 1.f, Pull = 1.f, Size = 1.f; int32 Color = 0, Blast = 0; float BlastSz = 1.f; float Height = 1.f; int32 Shots = 10, Ammo = 0; float Rate = 1.f; }; // b225: the turret rows (SHOTS / AMMO / FIRE RATE) // b177o: HEIGHT (how high the skill throws things) // b139b: BLAST SIZE
		TMap<FName, FTune> GTunes; bool GLoaded = false;
		FName GCur;
		bool GOpen = false, GPending = false, GTestDone = false;
		TWeakObjectPtr<AZoogiGameMode> GG;
		int32 GSel = 0, GCam = 0, GOrbN = 0, GTestIdx = 0; double GCastAt = 0.0, GTestNext = 0.0;
		TWeakObjectPtr<ACameraActor> GCamActor;
		// b108 (Evan: "swiping on screen rotates the camera around the skill performance"): the camera orbits the target; a preset sets
		// yaw / pitch / distance, a swipe turns it from there
		float GYaw = -147.f, GPitch = 35.f, GDist = 1590.f, GWantYaw = -147.f, GWantPitch = 35.f, GWantDist = 1590.f;
		bool GDrag = false; FVector2D GDragLast = FVector2D::ZeroVector;
		FVector GTarget = FVector::ZeroVector;
		struct FBtn { FBox2D Box; int32 Id; };
		TArray<FBtn> GBtns;
		const TCHAR* Skills[] = { TEXT("energy_shockwave"), TEXT("water_cannon"), TEXT("meteor_rain"), TEXT("fire_meteors"), TEXT("ice_wall"), TEXT("magic_rays"), TEXT("sword_slash"), TEXT("apple_bombs"), TEXT("wood_shield"), TEXT("energy_buff"), TEXT("golden_apple"), TEXT("wolf_pack"), TEXT("bolt_balls"),
			TEXT("frost_spikes"), TEXT("glacier_lance"), TEXT("ice_shards"), TEXT("stone_pillar"), TEXT("earthquake"), TEXT("rock_rampart"), TEXT("crescent_slash"), TEXT("cross_slash"), TEXT("ground_slash"), // b176i
			TEXT("boulder_block"), TEXT("sandstone_spire"), TEXT("stone_barricade"), TEXT("desert_rubble"), TEXT("jade_boulder"), TEXT("rock_burst"), TEXT("boulder_fountain"), TEXT("slab_row"), TEXT("jade_wall"), TEXT("snowcap_pillar"), TEXT("granite_block"), TEXT("moss_tower"), TEXT("frostrock_grove"), TEXT("obsidian_spike"), TEXT("obsidian_wall"), TEXT("obsidian_cluster"), TEXT("obsidian_burst"), TEXT("sapphire_pillar"), TEXT("sapphire_burst"), TEXT("sapphire_block"), TEXT("gold_rush"), TEXT("gold_nugget"), TEXT("gold_wall"), TEXT("rune_pillar"), TEXT("rune_wall"), TEXT("rune_monolith"), TEXT("stone_teeth"), TEXT("toppled_columns"), TEXT("landslide"), TEXT("starburst_spikes"), TEXT("rock_geyser"), TEXT("crater_ring"), TEXT("gold_column"), TEXT("jade_columns"), TEXT("sapphire_wall"), TEXT("sandstone_ruins"), TEXT("obsidian_star"), TEXT("earth_flash"), TEXT("rubble_storm"), TEXT("stone_cage"), TEXT("pillar_field"), TEXT("fault_line"), TEXT("shatter_star"), TEXT("slab_slam"), TEXT("rift_crack"), TEXT("long_rampart"), TEXT("boulder_pile"), // b178a Earthbender
			TEXT("gattling_turret"), /* b225 */ TEXT("ko_orb"), TEXT("ko_player") };
		constexpr int32 NumSkills = UE_ARRAY_COUNT(Skills);
		const TCHAR* ColorNames[] = { TEXT("DEFAULT"), TEXT("FIRE"), TEXT("ICE"), TEXT("VOID"), TEXT("GOLD"), TEXT("TOXIC"), TEXT("PINK"), TEXT("WHITE") };
		const FLinearColor Colors[] = { FLinearColor::Black, FLinearColor(1.f, 0.35f, 0.05f), FLinearColor(0.35f, 0.8f, 1.f), FLinearColor(0.6f, 0.2f, 1.f), FLinearColor(1.f, 0.8f, 0.2f), FLinearColor(0.4f, 1.f, 0.2f), FLinearColor(1.f, 0.3f, 0.7f), FLinearColor(1.f, 1.f, 1.f) };
		const TCHAR* BlastNames[] = { TEXT("DEFAULT"), TEXT("SMALL BILLOW"), TEXT("BILLOW 2"), TEXT("BILLOW 3"), TEXT("BIG BLAST"), TEXT("BIG BLAST 2"), TEXT("BIG BLAST 3"), TEXT("FIRE BURST"), TEXT("ELECTRIC"), TEXT("SMOKE"), TEXT("NONE") };
		const TCHAR* BlastFxs[] = { nullptr, TEXT("Mid_Explosion"), TEXT("Mid_Explosion_v2"), TEXT("Mid_Explosion_v3"), TEXT("Big_Explosion"), TEXT("Big_Explosion_v2"), TEXT("Big_Explosion_v3"), TEXT("Fire_v2"), TEXT("ElectricShock"), TEXT("Smoke_v2"), TEXT("") };
		const TCHAR* CamNames[] = { TEXT("WIDE"), TEXT("TOP"), TEXT("SIDE"), TEXT("BEHIND"), TEXT("ORBIT"), TEXT("CLOSE") };
		// b110 (Evan: "there is a pillar in the centre in the way"): the lab spot is off the middle, between the centre and the player's
		// spawn; the presets are relative to that spot's outward direction (side-on views keep the middle out of the frame)
		FVector GSpot = FVector::ZeroVector, GCasterAt = FVector::ZeroVector; float GBaseYaw = 0.f;
		// b112: the reach of the last PLAY (farthest orb from the rack centre, highest above its start), measured live
		float GReach = 0.f, GHeight = 0.f, GFloorZ = 0.f, GOrbZ = -1e9f; bool GMeasuring = false; TMap<TWeakObjectPtr<AZoogiOrb>, float> GStartZ;
		const float CamYaw[] = { 70.f, 0.f, 90.f, 0.f, 70.f, 60.f }, CamPitch[] = { 36.f, 88.f, 14.f, 12.f, 23.f, 22.f }, CamDist[] = { 1500.f, 2400.f, 1500.f, 1250.f, 1650.f, 700.f };
		void ApplyPreset() { const int32 k = FMath::Clamp(GCam, 0, 5); GWantYaw = GBaseYaw + CamYaw[k]; GWantPitch = CamPitch[k]; GWantDist = CamDist[k]; GYaw = FRotator::NormalizeAxis(GYaw); }
		constexpr int32 NumColors = UE_ARRAY_COUNT(ColorNames), NumBlasts = UE_ARRAY_COUNT(BlastNames), NumCams = UE_ARRAY_COUNT(CamNames);
		bool TestOn() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillLabTest")); return b; }
		FString SkillName(int32 i)
		{
			const FString Id = Skills[FMath::Clamp(i, 0, NumSkills - 1)];
			if (Id == TEXT("ko_orb")) return TEXT("ORB KNOCKOUT"); // b142 (Evan: "in Skills Lab also add orb and player ... swap out explosion types")
			if (Id == TEXT("ko_player")) return TEXT("PLAYER KNOCKOUT");
			return Id.Replace(TEXT("_"), TEXT(" ")).ToUpper();
		}
		FString File() { return FPaths::ProjectSavedDir() / TEXT("SkillLab.ini"); }
		void Load()
		{
			if (GLoaded) return; GLoaded = true;
			TArray<FString> L; FFileHelper::LoadFileToStringArray(L, *File());
			for (const FString& Ln : L)
			{
				FString K, V; if (!Ln.Split(TEXT("="), &K, &V)) continue;
				TArray<FString> P; V.ParseIntoArray(P, TEXT(","));
				if (P.Num() < 6) continue;
				FTune T; T.Strength = FCString::Atof(*P[0]); T.Radius = FCString::Atof(*P[1]); T.Pull = FCString::Atof(*P[2]); T.Size = FCString::Atof(*P[3]); T.Color = FCString::Atoi(*P[4]); T.Blast = FCString::Atoi(*P[5]); if (P.Num() > 6) T.BlastSz = FCString::Atof(*P[6]); if (P.Num() > 7) T.Height = FCString::Atof(*P[7]); if (P.Num() > 10) { T.Shots = FCString::Atoi(*P[8]); T.Ammo = FCString::Atoi(*P[9]); T.Rate = FCString::Atof(*P[10]); } // b225
				GTunes.Add(FName(*K.TrimStartAndEnd()), T);
			}
			UE_LOG(LogZoogi, Log, TEXT("SKILL LAB tunings loaded: %d"), GTunes.Num());
		}
		FString Dump()
		{
			FString S;
			for (const TPair<FName, FTune>& P : GTunes) S += FString::Printf(TEXT("%s=%.2f,%.2f,%.2f,%.2f,%d,%d,%.2f,%.2f,%d,%d,%.2f\n"), *P.Key.ToString(), P.Value.Strength, P.Value.Radius, P.Value.Pull, P.Value.Size, P.Value.Color, P.Value.Blast, P.Value.BlastSz, P.Value.Height, P.Value.Shots, P.Value.Ammo, P.Value.Rate);
			return S;
		}
		void Save() { FFileHelper::SaveStringToFile(Dump(), *File()); UE_LOG(LogZoogi, Log, TEXT("SKILL LAB saved:\n%s"), *Dump()); }
		const FTune* Cur() { Load(); return GTunes.Find(GCur); }
		FTune& Sel() { Load(); return GTunes.FindOrAdd(FName(Skills[GSel])); }
		AZoogiBall* Caster(AZoogiGameMode* G) { for (AZoogiBall* B : G->Balls) if (B && !B->bCPU) return B; return G->GetActiveBall(); }
		FVector Center(AZoogiGameMode* G) { return G->Arena ? FVector(G->Arena->GetActorLocation().X, G->Arena->GetActorLocation().Y, 0.f) : FVector::ZeroVector; }
		// "no player": the caster stays as the skills' origin but is never seen and nothing bumps into it
		void HideCaster(AZoogiGameMode* G)
		{
			AZoogiBall* Me = Caster(G);
			if (!Me) return;
			if (!Me->IsHidden()) Me->SetActorHiddenInGame(true);
			TArray<AActor*> Att; Me->GetAttachedActors(Att, true, true);
			for (AActor* A : Att) if (A && !A->IsHidden()) A->SetActorHiddenInGame(true);
			if (Me->Sphere && Me->Sphere->IsSimulatingPhysics()) { Me->Sphere->SetSimulatePhysics(false); Me->Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); }
		}
		void FindSpot(AZoogiGameMode* G)
		{
			const FVector C0 = Center(G);
			AZoogiBall* Me = Caster(G);
			const FVector Sp = Me ? Me->SpawnLocation : C0 - FVector(1200.f, 0.f, 0.f);
			FVector Out = Sp - C0; Out.Z = 0.f; const float Dist = Out.Size();
			if (!Out.Normalize()) Out = -FVector::ForwardVector;
			const float TgD = FMath::Clamp(Dist - 650.f, 450.f, 900.f); // the rack well clear of the middle (pillars / holograms)
			GSpot = C0 + Out * TgD; GSpot.Z = 0.f;
			GCasterAt = GSpot + Out * 650.f;
			GBaseYaw = Out.Rotation().Yaw;
			UE_LOG(LogZoogi, Log, TEXT("SKILL LAB spot %.0f cm off the middle (spawn %.0f), facing yaw %.0f"), TgD, Dist, GBaseYaw);
		}
		void ResetScene(AZoogiGameMode* G)
		{
			// the orbs re-racked on the lab spot (a fresh set if any were knocked out / scored); the hidden caster farther out
			const FVector C0 = GSpot;
			if (AZoogiBall* Me = Caster(G)) Me->SetActorLocation(FVector(GCasterAt.X, GCasterAt.Y, Me->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			bool bLost = G->Orbs.Num() < GOrbN;
			for (AZoogiOrb* O : G->Orbs) if (!O || O->bOut || O->IsHidden() || !O->Sphere) bLost = true;
			if (bLost) G->LabFreshOrbs();
			while (G->Orbs.Num() > GOrbN && G->Orbs.Num() > 0) { if (AZoogiOrb* X = G->Orbs.Last()) X->Destroy(); G->Orbs.Pop(); } // a tidy rack, not the whole training set
			int32 n = 0;
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || !O->Sphere) continue;
				// b113: a spaced rack (1 + 6 + 11) - b108-b112 packed 9 orbs on a 70 cm ring, they overlapped and burst apart on their own
				const int32 Ring = n == 0 ? 0 : (n <= 6 ? 1 : 2), InRing = n == 0 ? 0 : (n <= 6 ? n - 1 : n - 7), PerRing = Ring == 1 ? 6 : 11;
				const float A = Ring == 0 ? 0.f : 2.f * PI * InRing / PerRing + (Ring == 2 ? 0.3f : 0.f), Rr = 125.f * Ring;
				O->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); O->Sphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				O->SetActorLocation(FVector(C0.X + FMath::Cos(A) * Rr, C0.Y + FMath::Sin(A) * Rr, GOrbZ > -1e8f ? GOrbZ : O->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
				++n;
			}
			GTarget = C0;
		}
		void CastNow(AZoogiGameMode* G)
		{
			ZoogiSkills::ClearField(G); // b108 (Evan): a clean field every time - no leftovers from the skill before
			ResetScene(G);
			const FName Id(Skills[GSel]);
			AZoogiBall* Me = Caster(G);
			if (Id == FName(TEXT("ko_orb")) || Id == FName(TEXT("ko_player")))
			{	// b142: the knock-out explosion on the rack spot (what plays when an orb / a Zoogi is knocked out in a match)
				const FVector At = GSpot + FVector(0.f, 0.f, (GOrbZ > -1e8f ? GOrbZ : 40.f) + 10.f);
				if (Id == FName(TEXT("ko_orb"))) ZoogiArenaReact::OrbOutBlast(G->GetWorld(), At, false);
				else ZoogiArenaReact::PlayerKOBlast(G->GetWorld(), At, FLinearColor(1.f, 0.35f, 0.3f));
			}
			else if (Id == FName(TEXT("wolf_pack")))
			{	// the pack straight out (the roll button is not needed in the lab)
				if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G)) { WP->BankedWolves = 3; ZoogiSkillLab::SetCurrent(Id); const bool bWasCpu = Me->bCPU; Me->bCPU = true; WP->BeginSummon(G, Me); Me->bCPU = bWasCpu; } // b115: the pack picks its own targets in the lab (no paint view)
			}
			else ZoogiSkills::LabCast(G, Me, Id);
			GCastAt = FPlatformTime::Seconds();
			GReach = 0.f; GHeight = 0.f; GMeasuring = true; GStartZ.Reset();
			float Zs = 0.f; int32 Nz = 0;
			for (AZoogiOrb* O : G->Orbs) if (O) { GStartZ.Add(O, O->GetActorLocation().Z); Zs += O->GetActorLocation().Z; ++Nz; }
			GFloorZ = Nz ? Zs / Nz - 30.f : GSpot.Z;
			UE_LOG(LogZoogi, Log, TEXT("SKILL LAB play %s"), Skills[GSel]);
		}
		void UpdateCam(AZoogiGameMode* G, float Dt)
		{
			UWorld* W = G->GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
			if (!PC) return;
			if (!GOpen) { if (ACameraActor* C = GCamActor.Get()) { if (AZoogiPlayerController* ZP = Cast<AZoogiPlayerController>(PC)) ZP->RestoreMatchCam(0.f); C->Destroy(); } GCamActor.Reset(); return; }
			if (!GCamActor.IsValid())
			{
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				GCamActor = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP);
				if (ACameraActor* C = GCamActor.Get()) { C->GetCameraComponent()->bConstrainAspectRatio = false; C->GetCameraComponent()->SetFieldOfView(70.f); C->SetActorLocation(GTarget + FVector(-1100.f, -700.f, 900.f)); PC->SetViewTargetWithBlend(C, 0.25f); }
			}
			ACameraActor* C = GCamActor.Get(); if (!C) return;
			const FVector Tg = GTarget;
			if (GCam == 4 && !GDrag) GWantYaw += Dt * 25.f; // ORBIT turns by itself
			const float A = FMath::Clamp(Dt * 6.f, 0.f, 1.f);
			GYaw += FRotator::NormalizeAxis(GWantYaw - GYaw) * A; GPitch = FMath::Lerp(GPitch, GWantPitch, A); GDist = FMath::Lerp(GDist, GWantDist, A);
			const FVector Eye = Tg + FRotator(GPitch, GYaw, 0.f).Vector() * GDist;
			C->SetActorLocation(Eye);
			C->SetActorRotation((Tg + FVector(0, 0, 40.f) - Eye).Rotation());
			if (PC->GetViewTarget() != C) PC->SetViewTarget(C);
		}
	}

	bool Open() { return GOpen; }
	void RequestLab() { GPending = true; UE_LOG(LogZoogi, Log, TEXT("SKILL LAB requested (front button)")); }
	bool Pending() { return GPending || (TestOn() && !GTestDone); }
	void SetCurrent(FName Skill) { GCur = Skill; }
	FName Current() { return GCur; }
	float StrengthOf(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->Strength, 0.f, 5.f) : 1.f; }
	float RadiusOf(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->Radius, 0.2f, 4.f) : 1.f; }
	float Strength() { const FTune* T = Cur(); return T ? FMath::Clamp(T->Strength, 0.f, 5.f) : 1.f; }
	float Radius() { const FTune* T = Cur(); return T ? FMath::Clamp(T->Radius, 0.2f, 4.f) : 1.f; }
	float Pull() { const FTune* T = Cur(); return T ? FMath::Clamp(T->Pull, 0.f, 5.f) : 1.f; }
	float FxSize() { const FTune* T = Cur(); return T ? FMath::Clamp(T->Size, 0.2f, 4.f) : 1.f; }
	bool Tint(FLinearColor& Out) { const FTune* T = Cur(); if (!T || T->Color <= 0 || T->Color >= NumColors) return false; Out = Colors[T->Color]; return true; }
	float BlastSize() { const FTune* T = Cur(); return T ? FMath::Clamp(T->BlastSz, 0.2f, 4.f) : 1.f; }
	float Height() { const FTune* T = Cur(); return T ? FMath::Clamp(T->Height, 0.f, 5.f) : 1.f; } // b177o
	const TCHAR* ChosenBlast() { const FTune* T = Cur(); if (!T || T->Blast <= 0 || T->Blast >= NumBlasts) return nullptr; return BlastFxs[T->Blast]; }
	const TCHAR* BlastFor(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); if (!T || T->Blast <= 0 || T->Blast >= NumBlasts) return nullptr; return BlastFxs[T->Blast]; }
	float BlastSizeFor(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->BlastSz * T->Size, 0.2f, 6.f) : 1.f; }
	bool TintFor(FName Skill, FLinearColor& Out) { Load(); const FTune* T = GTunes.Find(Skill); if (!T || T->Color <= 0 || T->Color >= NumColors) return false; Out = Colors[T->Color]; return true; }
	// b225: the GATLING TURRET's own lab rows
	namespace { const FTune* TurTune() { Load(); return GTunes.Find(FName(TEXT("gattling_turret"))); } const int32 AmmoStyles[] = { 0, 5, 2, 3, 4, 6 }; const TCHAR* AmmoNames[] = { TEXT("INVENTORY"), TEXT("ROCKET"), TEXT("TRACERS"), TEXT("PLASMA"), TEXT("LASER"), TEXT("RING") }; }
	int32 TurretShots() { const FTune* T = TurTune(); return T ? FMath::Clamp(T->Shots, 1, 40) : 10; }
	int32 TurretAmmo() { const FTune* T = TurTune(); return T ? AmmoStyles[FMath::Clamp(T->Ammo, 0, 5)] : 0; }
	float TurretRate() { const FTune* T = TurTune(); return T ? FMath::Clamp(T->Rate, 0.2f, 4.f) : 1.f; }
	float SizeOf(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->Size, 0.2f, 4.f) : 1.f; }
	const TCHAR* BlastFx(const TCHAR* DefaultFx) { const FTune* T = Cur(); if (!T || T->Blast <= 0 || T->Blast >= NumBlasts) return DefaultFx; return BlastFxs[T->Blast]; }

	void OpenLab(AZoogiGameMode* G)
	{
		if (!G || GOpen) return;
		Load(); GOpen = true; GPending = false; GTestDone = true; GG = G; GCam = 0; FindSpot(G); ApplyPreset(); GYaw = GWantYaw; GPitch = GWantPitch; GDist = GWantDist; GDrag = false;
		GOrbN = FMath::Min(G->Orbs.Num(), 18);
		GOrbZ = -1e9f; for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { GOrbZ = O->GetActorLocation().Z; break; } // resting height (no drops from mid-air)
		HideCaster(G);
		ResetScene(G);
		if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("SKILL LAB"), TEXT("PICK A SKILL  -  PRESS PLAY"), FLinearColor(1.f, 0.82f, 0.25f), 1.6f, 1.f);
		GTestNext = FPlatformTime::Seconds() + 1.5; GTestIdx = 0;
		UE_LOG(LogZoogi, Log, TEXT("SKILL LAB open (%d orbs, caster hidden)"), GOrbN);
	}

	void CloseLab(AZoogiGameMode* G)
	{
		if (!GOpen) return;
		GOpen = false;
		if (G) UpdateCam(G, 0.f);
		Save();
		UE_LOG(LogZoogi, Log, TEXT("SKILL LAB closed"));
	}

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		if (!GOpen)
		{
			if (Pending() && G->Phase == EZoogiPhase::Aiming && G->Balls.Num() > 0 && !G->IsRoyaleMatch()) OpenLab(G);
			return;
		}
		if (GG.Get() != G || G->Phase == EZoogiPhase::Menu) { GOpen = false; GCamActor.Reset(); return; }
		HideCaster(G);
		UpdateCam(G, RealDt);
		if (GMeasuring)
		{
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O) continue;
				const FVector P = O->GetActorLocation();
				if (O->bOut || O->IsHidden()) continue; // knocked out of the arena: the ring stops at the arena's edge
				GReach = FMath::Max(GReach, FVector::Dist2D(P, GSpot));
				if (const float* Z0 = GStartZ.Find(O)) GHeight = FMath::Max(GHeight, P.Z - *Z0);
			}
			if (FPlatformTime::Seconds() - GCastAt > 8.0) GMeasuring = false; // settled: the marks stay until the next PLAY / RESET
		}
		static int32 WcStep = 0;
		if (TestOn() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiWcYawTest")) && WcStep < 4 && FPlatformTime::Seconds() >= GTestNext)
		{	// b142c measuring pass: the water cannon at yaw offsets 0 / 90 / -90 / 180 from the TOP camera
			const float Yaws[4] = { 0.f, 90.f, -90.f, 180.f };
			GZoogiWcYawTest = Yaws[WcStep];
			GCam = 1; ApplyPreset();
			for (int32 s = 0; s < NumSkills; ++s) if (FCString::Strcmp(Skills[s], TEXT("water_cannon")) == 0) GSel = s;
			CastNow(G);
			for (int32 k = 0; k < 5; ++k) G->TakeShotDelayed(FString::Printf(TEXT("wc_y%+.0f_%d"), Yaws[WcStep], k), 0.25f + 0.25f * k);
			++WcStep; GTestNext = FPlatformTime::Seconds() + 3.5;
			if (WcStep == 4) { GCam = 0; }
			return;
		}
		if (WcStep == 4) { WcStep = 5; GZoogiWcYawTest = -999.f; ApplyPreset(); }
		{	// b225 preview: -ZoogiLabOnly=<skill> PLAYs that one skill and takes 6 frames over 6 s
			static FString Only; static bool bOnlyRead = false; if (!bOnlyRead) { bOnlyRead = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiLabOnly="), Only); }
			if (TestOn() && !Only.IsEmpty() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)
			{
				for (int32 s = 0; s < NumSkills; ++s) if (Only == Skills[s]) GSel = s;
				GCam = 0; ApplyPreset(); CastNow(G);
				for (int32 k = 0; k < 6; ++k) G->TakeShotDelayed(FString::Printf(TEXT("lab_%s_%d"), *Only, k), 0.6f + 1.0f * k);
				GTestIdx = NumSkills; return;
			}
		}
		if (TestOn() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)
		{	// capture run: PLAY every skill once, a frame at the peak of each
			GSel = GTestIdx; CastNow(G);
			G->TakeShotDelayed(FString::Printf(TEXT("lab_%02d_%s"), GTestIdx, Skills[GTestIdx]), 1.1f);
			++GTestIdx; GTestNext = FPlatformTime::Seconds() + 4.5;
		}
	}

	void Draw(AZoogiGameMode* G, UCanvas* C, APlayerController* PC)
	{
		if (!GOpen || !C) return;
		const float Wd = C->ClipX, Ht = C->ClipY;
		const float U = FMath::Min(Wd, Ht) / 1080.f;
		GBtns.Reset();
		const FLinearColor Ink(0.03f, 0.02f, 0.04f, 0.92f), Gold(1.f, 0.82f, 0.25f), Red(0.91f, 0.13f, 0.16f), White = FLinearColor::White, Shade(0.f, 0.f, 0.f, 0.55f);
		auto AddBtn = [&](const FVector2D& Ctr, float Wb, float Hb, int32 Id) { FBtn B; B.Box = FBox2D(Ctr - FVector2D(Wb, Hb) * 0.5f, Ctr + FVector2D(Wb, Hb) * 0.5f); B.Id = Id; GBtns.Add(B); };
		// b112: the reach marks (world-space, projected)
		if (PC && GReach > 40.f)
		{
			const FLinearColor RingCol(1.f, 0.15f, 0.35f, 1.f), ArcCol(0.3f, 0.95f, 1.f, 1.f); // b113: hot pink - the arena lines are gold
			const FVector Ctr(GSpot.X, GSpot.Y, GFloorZ);
			auto Seg = [&](const FVector& A, const FVector& B, const FLinearColor& Col, float Th)
			{
				FVector2D Sa, Sb;
				if (PC->ProjectWorldLocationToScreen(A, Sa, true) && PC->ProjectWorldLocationToScreen(B, Sb, true)) { FCanvasLineItem Li(Sa, Sb); Li.LineThickness = Th; Li.SetColor(Col); C->DrawItem(Li); }
			};
			const int32 N = 72;
			for (int32 k = 0; k < N; ++k) // the floor ring: how far the farthest orb went
			{
				const float A0 = 2.f * PI * k / N, A1 = 2.f * PI * (k + 1) / N;
				Seg(Ctr + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.f) * GReach, Ctr + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.f) * GReach, RingCol, 6.f * U);
			}
			// the height arc: a dashed half circle standing on the ring, side-on to the camera, up to the highest an orb flew
			FVector Side = FVector::RightVector;
			if (ACameraActor* Cm = GCamActor.Get()) { Side = FVector::CrossProduct(FVector::UpVector, (Ctr - Cm->GetActorLocation()).GetSafeNormal2D()); if (!Side.Normalize()) Side = FVector::RightVector; }
			const float Hh = FMath::Max(GHeight, 20.f);
			const int32 Na = 32;
			for (int32 k = 0; k < Na; k += 2)
			{
				const float A0 = PI * k / Na, A1 = PI * (k + 1) / Na;
				Seg(Ctr + Side * FMath::Cos(A0) * GReach + FVector(0, 0, FMath::Sin(A0) * Hh), Ctr + Side * FMath::Cos(A1) * GReach + FVector(0, 0, FMath::Sin(A1) * Hh), ArcCol, 5.f * U);
			}
			Seg(Ctr, Ctr + FVector(0, 0, Hh), FLinearColor(0.45f, 0.9f, 1.f, 0.5f), 2.f * U); // the height pole
			FVector2D Ls, Hs;
			FVector Front = FVector::BackwardVector; if (ACameraActor* Cf = GCamActor.Get()) { Front = (Cf->GetActorLocation() - Ctr).GetSafeNormal2D(); if (Front.IsNearlyZero()) Front = FVector::BackwardVector; }
			if (PC->ProjectWorldLocationToScreen(Ctr + Front * GReach, Ls, true)) ZoogiFunCanvasText(C, FString::Printf(TEXT("REACH %.1f m"), GReach / 100.f), Ls.X, Ls.Y + 8.f * U, 26.f * U, RingCol, 1, 1);
			if (PC->ProjectWorldLocationToScreen(Ctr + FVector(0, 0, Hh), Hs, true)) ZoogiFunCanvasText(C, FString::Printf(TEXT("HEIGHT %.1f m"), GHeight / 100.f), Hs.X, Hs.Y - 34.f * U, 26.f * U, ArcCol, 1, 1);
		}
		// title plate (P5: black slant, gold lettering)
		ZoogiFunCanvasQuad(C, FVector2D(250.f * U, 64.f * U), 420.f * U, 84.f * U, -4.f, 16.f * U, Ink);
		ZoogiFunCanvasText(C, TEXT("SKILL LAB"), 250.f * U, 38.f * U, 54.f * U, Gold, 1, 1);
		// EXIT (top right)
		{
			const FVector2D Ec(Wd - 130.f * U, 64.f * U);
			ZoogiFunCanvasQuad(C, Ec + FVector2D(6.f, 6.f) * U, 200.f * U, 76.f * U, 4.f, 14.f * U, Shade);
			ZoogiFunCanvasQuad(C, Ec, 200.f * U, 76.f * U, 4.f, 14.f * U, Ink);
			ZoogiFunCanvasText(C, TEXT("EXIT"), Ec.X, Ec.Y - 22.f * U, 42.f * U, White, 1, 1);
			AddBtn(Ec, 220.f * U, 90.f * U, 20);
		}
		// tuning panel (left)
		const float X0 = 24.f * U, W0 = 570.f * U, Rh = 62.f * U;
		float Y = 130.f * U;
		const bool bTur = FCString::Strcmp(Skills[FMath::Clamp(GSel, 0, NumSkills - 1)], TEXT("gattling_turret")) == 0; // b225
		ZoogiFunCanvasRect(C, X0 - 8.f * U, Y - 10.f * U, W0 + 16.f * U, Rh * (bTur ? 12.4f : 10.4f), FLinearColor(0.03f, 0.02f, 0.04f, 0.72f));
		FTune& T = Sel();
		auto SmallBtn = [&](float X, float Yb, float Wb, const FString& L, int32 Id, const FLinearColor& Bg)
		{
			ZoogiFunCanvasRect(C, X, Yb, Wb, Rh - 12.f * U, Bg);
			ZoogiFunCanvasText(C, L, X + Wb * 0.5f, Yb + 5.f * U, 32.f * U, White, 1, 1);
			FBtn B; B.Box = FBox2D(FVector2D(X, Yb) - FVector2D(6.f, 6.f) * U, FVector2D(X + Wb, Yb + Rh - 12.f * U) + FVector2D(6.f, 6.f) * U); B.Id = Id; GBtns.Add(B);
		};
		auto Row = [&](const TCHAR* Label, const FString& Val, int32 IdMinus, int32 IdPlus)
		{
			ZoogiFunCanvasText(C, Label, X0, Y + 10.f * U, 28.f * U, Gold, 0, 1);
			SmallBtn(X0 + 222.f * U, Y, 64.f * U, TEXT("<"), IdMinus, Red);
			ZoogiFunCanvasText(C, Val, X0 + 394.f * U, Y + 8.f * U, 30.f * U, White, 1, 1);
			SmallBtn(X0 + 506.f * U, Y, 64.f * U, TEXT(">"), IdPlus, Red);
			Y += Rh;
		};
		Row(TEXT("STRENGTH"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Strength * 100.f)), 2, 3);
		Row(TEXT("RADIUS"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Radius * 100.f)), 4, 5);
		Row(TEXT("HEIGHT"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Height * 100.f)), 32, 33); // b177o (Evan: "add height adjustment to change how high they fly up")
		if (bTur)
		{	// b225: the turret - FIRE RATE (instead of PULL), SHOTS, AMMO (INVENTORY = the player's pick)
			Row(TEXT("FIRE RATE"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Rate * 100.f)), 34, 35);
			Row(TEXT("SHOTS"), FString::Printf(TEXT("%d"), FMath::Clamp(T.Shots, 1, 40)), 36, 37);
			Row(TEXT("AMMO"), AmmoNames[FMath::Clamp(T.Ammo, 0, 5)], 38, 39);
		}
		else Row(TEXT("PULL"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Pull * 100.f)), 6, 7);
		Row(TEXT("FX SIZE"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Size * 100.f)), 8, 9);
		Row(TEXT("COLOUR"), ColorNames[FMath::Clamp(T.Color, 0, NumColors - 1)], 10, 11);
		Row(TEXT("BLAST"), BlastNames[FMath::Clamp(T.Blast, 0, NumBlasts - 1)], 12, 13);
		Row(TEXT("BLAST SIZE"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.BlastSz * 100.f)), 30, 31); // b139b
		Row(TEXT("CAMERA"), CamNames[FMath::Clamp(GCam, 0, NumCams - 1)], 14, 15);
		const float Bw = (W0 - 10.f * U) / 2.f;
		SmallBtn(X0, Y, Bw, TEXT("DEFAULTS"), 18, FLinearColor(0.3f, 0.28f, 0.3f));
		SmallBtn(X0 + Bw + 10.f * U, Y, Bw, TEXT("COPY"), 19, FLinearColor(0.55f, 0.4f, 0.1f));
		// bottom: [RESET] [<] [SKILL NAME] [>] [PLAY]
		{
			const float ResW = 190.f * U, ArW = 96.f * U, PlayW = 240.f * U, Gap = 18.f * U, BarH = 116.f * U;
			const float NameW = FMath::Clamp(Wd - 60.f * U - (ResW + PlayW + 2.f * ArW + 4.f * Gap), 300.f * U, 560.f * U);
			const float Total = ResW + ArW + NameW + ArW + PlayW + 4.f * Gap;
			float X = (Wd - Total) * 0.5f;
			const float Yc = Ht - 50.f * U - BarH * 0.5f;
			auto Plate = [&](float W, const FLinearColor& Col, float Ang) -> FVector2D
			{
				const FVector2D Ct(X + W * 0.5f, Yc);
				ZoogiFunCanvasQuad(C, Ct + FVector2D(8.f, 8.f) * U, W, BarH, Ang, 16.f * U, Shade);
				ZoogiFunCanvasQuad(C, Ct, W, BarH, Ang, 16.f * U, Col);
				X += W + Gap;
				return Ct;
			};
			const FVector2D Rc = Plate(ResW, Ink, 4.f);
			ZoogiFunCanvasText(C, TEXT("RESET"), Rc.X, Rc.Y - 24.f * U, 42.f * U, White, 1, 1);
			AddBtn(Rc, ResW + 10.f * U, BarH + 10.f * U, 21);
			const FVector2D La = Plate(ArW, Red, -3.f);
			ZoogiFunCanvasText(C, TEXT("<"), La.X, La.Y - 34.f * U, 64.f * U, White, 1, 1);
			AddBtn(La, ArW + 10.f * U, BarH + 10.f * U, 0);
			const FVector2D Nc = Plate(NameW, Ink, -3.f);
			const FString Nm = SkillName(GSel);
			const float Fs = FMath::Min(48.f * U, (NameW - 30.f * U) / (0.95f * FMath::Max(1, Nm.Len())));
			ZoogiFunCanvasText(C, Nm, Nc.X, Nc.Y - Fs * 0.75f, Fs, White, 1, 1);
			ZoogiFunCanvasText(C, FString::Printf(TEXT("%d / %d"), GSel + 1, NumSkills), Nc.X, Nc.Y + 22.f * U, 22.f * U, Gold, 1, 0);
			const FVector2D Ra = Plate(ArW, Red, -3.f);
			ZoogiFunCanvasText(C, TEXT(">"), Ra.X, Ra.Y - 34.f * U, 64.f * U, White, 1, 1);
			AddBtn(Ra, ArW + 10.f * U, BarH + 10.f * U, 1);
			const bool bPlaying = FPlatformTime::Seconds() - GCastAt < 1.2;
			const FVector2D Pc = Plate(PlayW, bPlaying ? FLinearColor(0.6f, 0.08f, 0.1f) : Red, 4.f);
			ZoogiFunCanvasText(C, TEXT("PLAY"), Pc.X, Pc.Y - 30.f * U, 54.f * U, White, 1, 1);
			AddBtn(Pc, PlayW + 10.f * U, BarH + 10.f * U, 17);
			ZoogiFunCanvasText(C, TEXT("SWIPE THE SCREEN TO TURN THE CAMERA"), Wd * 0.5f, Yc - BarH * 0.5f - 34.f * U, 22.f * U, FLinearColor(1.f, 1.f, 1.f, 0.8f), 1, 0);
		}
	}

	void Input(AZoogiGameMode* G, APlayerController* PC, const FVector2D& Ptr, bool bPress, bool bDown, bool bRelease)
	{
		if (!GOpen) return;
		if (bRelease) GDrag = false;
		if (GDrag && bDown && !bPress)
		{	// swipe: left / right turns around the skill, up / down tilts
			const FVector2D D = Ptr - GDragLast; GDragLast = Ptr;
			GWantYaw += D.X * 0.3f; GWantPitch = FMath::Clamp(GWantPitch + D.Y * 0.2f, 4.f, 88.f);
			GYaw += D.X * 0.3f; GPitch = FMath::Clamp(GPitch + D.Y * 0.2f, 4.f, 88.f);
			return;
		}
		if (!bPress) return;
		int32 Id = -1;
		for (const FBtn& B : GBtns) if (B.Box.IsInside(Ptr)) { Id = B.Id; break; }
		UE_LOG(LogZoogi, Log, TEXT("SKILL LAB tap (%.0f, %.0f) -> %d (%d buttons)"), Ptr.X, Ptr.Y, Id, GBtns.Num());
		if (Id < 0) { GDrag = true; GDragLast = Ptr; return; } // not on a button: a swipe starts
		FTune& T = Sel();
		auto Step = [](float& V, float D, float Lo, float Hi) { V = FMath::Clamp(FMath::RoundToFloat((V + D) * 100.f) / 100.f, Lo, Hi); };
		switch (Id)
		{
		case 0: GSel = (GSel + NumSkills - 1) % NumSkills; ZoogiSkills::ClearField(G); ResetScene(G); GCastAt = 0.0; break; // b108 (Evan): a new skill stops the last one
		case 1: GSel = (GSel + 1) % NumSkills; ZoogiSkills::ClearField(G); ResetScene(G); GCastAt = 0.0; break;
		case 2: Step(T.Strength, -0.1f, 0.f, 5.f); break; case 3: Step(T.Strength, 0.1f, 0.f, 5.f); break;
		case 4: Step(T.Radius, -0.1f, 0.2f, 4.f); break; case 5: Step(T.Radius, 0.1f, 0.2f, 4.f); break;
		case 6: Step(T.Pull, -0.1f, 0.f, 5.f); break; case 7: Step(T.Pull, 0.1f, 0.f, 5.f); break;
		case 8: Step(T.Size, -0.1f, 0.2f, 4.f); break; case 9: Step(T.Size, 0.1f, 0.2f, 4.f); break;
		case 10: T.Color = (T.Color + NumColors - 1) % NumColors; break; case 11: T.Color = (T.Color + 1) % NumColors; break;
		case 12: T.Blast = (T.Blast + NumBlasts - 1) % NumBlasts; break; case 13: T.Blast = (T.Blast + 1) % NumBlasts; break;
		case 32: Step(T.Height, -0.1f, 0.f, 5.f); break; case 33: Step(T.Height, 0.1f, 0.f, 5.f); break; // b177o
		case 34: Step(T.Rate, -0.1f, 0.2f, 4.f); break; case 35: Step(T.Rate, 0.1f, 0.2f, 4.f); break; // b225 turret
		case 36: T.Shots = FMath::Clamp(T.Shots - 1, 1, 40); break; case 37: T.Shots = FMath::Clamp(T.Shots + 1, 1, 40); break;
		case 38: T.Ammo = (T.Ammo + 5) % 6; break; case 39: T.Ammo = (T.Ammo + 1) % 6; break;
		case 30: Step(T.BlastSz, -0.1f, 0.2f, 4.f); break; case 31: Step(T.BlastSz, 0.1f, 0.2f, 4.f); break; // b139b
		case 14: GCam = (GCam + NumCams - 1) % NumCams; ApplyPreset(); break; case 15: GCam = (GCam + 1) % NumCams; ApplyPreset(); break;
		case 17: CastNow(G); break;
		case 18: T = FTune(); break;
		case 21: ZoogiSkills::ClearField(G); ResetScene(G); GCastAt = 0.0; GReach = GHeight = 0.f; GMeasuring = false; UE_LOG(LogZoogi, Log, TEXT("SKILL LAB reset (field cleaned)")); break;
		case 19: { const FString D = Dump(); FPlatformApplicationMisc::ClipboardCopy(*D); if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("COPIED"), TEXT("PASTE IT TO CLAUDE TO BAKE IT IN"), FLinearColor(1.f, 0.82f, 0.25f), 1.4f, 0.9f); break; }
		case 20: ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f); CloseLab(G); if (G) G->ReturnToMenu(); return;
		default: return;
		}
		ZoogiUtil::PlaySfx(G, Id == 17 ? TEXT("UiConfirm") : TEXT("UiClick"), 0.7f, 1.1f);
		if ((Id >= 2 && Id <= 13) || Id == 18 || (Id >= 30 && Id <= 39)) Save(); // b225: + the height / blast size / turret rows
	}
}
