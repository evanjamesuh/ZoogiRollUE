// Oct 31 (Evan): BATTLE ROYALE heroes + relics.
//  "for battle royale add the new zoogi models as the players and use the current old transform models as bots"
//  "the new models have advanced animation for walking and running jumping, shooting, the old bots stay as is movement wise and
//   leave relics around for pickup to use as weapon"
// HERO: the human's Zoogi is shown as its new crystal-mech model (the Tripo rigs in /Game/Zoogis/Ultimates, form picked like the
//   ultimate strike: Frosti/Blue -> ult_frost_blue, Vex/Hotstreak -> ult_ember_red, Lumi -> ult_fox_gold_A ...) instead of the
//   SK_ZoogiBlue ball/robot. Physics, controls and the transform rules are unchanged; the model stands on the ball, faces the aim
//   (robot) or the motion (ball) and animates: idle, walk (run clip slowed), run, jump, fall, shoot (cast), hit, cheer.
//   CPU bots keep the old transform model exactly as before.
// RELICS: bots leave relics behind (each life a bot loses, and one every ~18 s from a roaming bot while fewer than 3 lie around):
//   Crystal Blade (melee slash), Sage Staff (heavy magic bolts), Jewel Lantern (area blast), Rune Emblem (shield). Only the human
//   picks them up (roll over one); the relic shows in the hero's hand and spends a charge per use. -ZoogiNoHero = old look.
#include "ZoogiGameMode.h"
#include "ZoogiShooter.h" // b236
#include "ZoogiVoyager.h" // b233
#include "ZoogiRoyale.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
#include "ZoogiMorph.h"
#include "ZoogiFX.h"
#include "ZoogiPlayerController.h"
#include "ZoogiFrontProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Engine/SkeletalMeshSocket.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiHero
{
	struct FForm { const TCHAR* Folder; const TCHAR* Prefix; };
	static const FForm Forms[] = { // same order as ZoogiStrikeData::Forms / UZoogiFunSys::FormIndex
		{ TEXT("ult_ember_red"),  TEXT("tripo_convert_8a2b4778-1099-43cf-9973-f7d95d0322e2") },
		{ TEXT("ult_fox_gold_A"), TEXT("tripo_convert_8f3e854a-c2aa-4bf0-899e-b9f9ee139d53") },
		{ TEXT("ult_fox_gold_B"), TEXT("tripo_convert_d01c5bdd-e9ed-47d4-8a65-86c9e7665b68") },
		{ TEXT("ult_frost_blue"), TEXT("tripo_convert_c56d5f01-e4d4-43ca-98a0-612995efea9a") },
	};
	static const TCHAR* BallFor(const FString& Name)
	{
		const FString N = Name.ToLower();
		if (N == TEXT("luma") || N == TEXT("lumi")) return TEXT("T_Ball_Lumi");
		if (N == TEXT("hotstreak")) return TEXT("T_Ball_Hotstreak");
		if (N == TEXT("bolt")) return TEXT("T_Ball_Bolt");
		if (N == TEXT("vex") || N == TEXT("dark zoogy") || N == TEXT("darkzoogy")) return TEXT("T_Ball_Vex");
		// Nov 1: Evan's buzzy ball set (8): + Blue / Terra / Zephyr / Ember
		if (N == TEXT("blue")) return TEXT("T_Ball_Blue");
		if (N == TEXT("terra") || N == TEXT("moss")) return TEXT("T_Ball_Terra");
		if (N == TEXT("zephyr")) return TEXT("T_Ball_Zephyr");
		if (N == TEXT("ember")) return TEXT("T_Ball_Ember");
		return TEXT("T_Ball_Frosti");
	}
	static int32 FormFor(const FString& Name)
	{
		const FString N = Name.ToLower();
		if (N == TEXT("vex") || N == TEXT("hotstreak") || N == TEXT("dark zoogy") || N == TEXT("darkzoogy")) return 0;
		if (N == TEXT("luma") || N == TEXT("lumi") || N == TEXT("tavi") || N == TEXT("wolfgang")) return 1;
		if (N == TEXT("sera") || N == TEXT("moss") || N == TEXT("nightshade")) return 2;
		return 3; // blue / frosti and everyone else: the ice crystal robot
	}
	struct FState
	{
		TWeakObjectPtr<AActor> Actor;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TWeakObjectPtr<UStaticMeshComponent> BallSkin; // Oct 31: Evan's ball designs - the hero's ball form (TRANSFORM)
		bool bWasRobot = true;
		bool bStartMech = true; // flip to the mech once the fight starts
		TArray<TStrongObjectPtr<UObject>> Keep;
		TMap<FString, UAnimSequence*> Clips;
		int32 Form = -1, Owner = -1;
		FString Cur;
		float Rate = 1.f, Yaw = 0.f, ActionLeft = 0.f, LastHurt = -100.f;
		FName HandBone = NAME_None;
		float Height = 185.f;
		// Oct 31 b42: Epic Game Animation Sample loops retargeted onto the rig (Scripts/royale/retarget_gasp.py -> <rig>/GASP/)
		bool bGasp = false;
		bool bRifle = false; // Oct 31 b44: Lyra rifle set on the generic mannequin (mech / standing form = third-person shooter)
		TWeakObjectPtr<UStaticMeshComponent> Rifle;
		float RifleKick = 0.f;
		TWeakObjectPtr<USceneComponent> GunSpin; float SpinAng = 0.f, LastShot = -10.f; // b45 Gatling barrels
		float PrevVz = 0.f, AirT = 0.f, DirHold = 0.f;
		FString DirCand, DirCur = TEXT("F");
	};
	FState H;

	enum ERelic : int32 { Blade, Staff, Lantern, Emblem, NumRelics };
	// Oct 31 b45 (Evan: "Gatling gun, no sword or melee attacks"): the relics are power-ups for the gun now (it never leaves the hands);
	// the blade is out of the drop rotation and its slash path is never reached.
	static const TCHAR* RelicName[NumRelics] = { TEXT("CRYSTAL BLADE"), TEXT("HEAVY ROUNDS"), TEXT("SHOCK BLAST"), TEXT("RUNE SHIELD") };
	static const TCHAR* RelicMesh[NumRelics] = { TEXT("/Game/Relics/Sword/SM_RelicSword.SM_RelicSword"), TEXT("/Game/Relics/Staff/SM_RelicStaff.SM_RelicStaff"),
		TEXT("/Game/Relics/Lantern/SM_RelicLantern.SM_RelicLantern"), TEXT("/Game/Relics/Emblem/SM_RelicEmblem.SM_RelicEmblem") };
	static const int32 RelicCharges[NumRelics] = { 6, 45, 3, 1 };
	static const FLinearColor RelicCol[NumRelics] = { FLinearColor(0.45f, 0.85f, 1.f), FLinearColor(0.75f, 0.45f, 1.f), FLinearColor(1.f, 0.8f, 0.3f), FLinearColor(0.4f, 1.f, 0.6f) };
	struct FRelic { int32 Type = 0; TWeakObjectPtr<AActor> Actor; FVector Pos; float Born = 0.f; };
	TArray<FRelic> Relics;
	struct FHeld { int32 Type = -1; int32 Charges = 0; TWeakObjectPtr<UStaticMeshComponent> InHand; };
	FHeld Held;
	float NextDrop = 18.f;
	int32 NextType = 0;

	UStaticMesh* LoadRelic(int32 T) { return LoadObject<UStaticMesh>(nullptr, RelicMesh[T], nullptr, LOAD_NoWarn | LOAD_Quiet); }
	void FitMesh(UStaticMeshComponent* C, UStaticMesh* M, float MaxDim)
	{
		if (!C || !M) return;
		const FVector E = M->GetBounds().BoxExtent * 2.f;
		C->SetRelativeScale3D(FVector(MaxDim / FMath::Max(1.f, E.GetMax())));
	}
	// ---- b120 weapons (the robot form's weapon, picked in the inventory WEAPON tab)
	enum EWeapon : int32 { WGatling, WGrenade, WSword, WStaff, WDisc, NumWeapons };
	static const TCHAR* WeaponKey[NumWeapons] = { TEXT("gatling"), TEXT("grenade"), TEXT("sword"), TEXT("staff"), TEXT("disc") };
	static const TCHAR* WeaponName[NumWeapons] = { TEXT("GATLING"), TEXT("PRISM GRENADE"), TEXT("CRYSTAL SWORD"), TEXT("CRYSTAL STAFF"), TEXT("HALO DISC") };
	static const int32 WeaponRelic[NumWeapons] = { -1, Lantern, Blade, Staff, Emblem }; // the Tripo meshes (/Game/Relics/*)
	static const float WeaponSize[NumWeapons] = { 0.f, 46.f, 125.f, 125.f, 82.f };   // longest side in the hand, cm
	static const FLinearColor WeaponCol(0.78f, 0.62f, 1.f);
	int32 Weapon = WGatling;
	float WSwing = 0.f;
	TWeakObjectPtr<UStaticMeshComponent> WeaponMesh;
	struct FThrow { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Dir = FVector::ForwardVector; float Age = 0.f, Spin = 0.f; int32 Kind = 0; bool bBack = false; TSet<int32> Hit; };
	TArray<FThrow> Throws;
	bool DiscOut() { for (const FThrow& Tw : Throws) if (Tw.Kind == WDisc) return true; return false; }
	int32 WeaponFromKey(const FString& K) { for (int32 i = 0; i < NumWeapons; ++i) if (K.Equals(WeaponKey[i], ESearchCase::IgnoreCase)) return i; return WGatling; }
	/** puts a fitted relic mesh so its bounds centre lands on Ctr */
	void PlaceMesh(UStaticMeshComponent* C, const FVector& Ctr, const FRotator& Rot)
	{
		if (!C || !C->GetStaticMesh()) return;
		const FVector O = C->GetStaticMesh()->GetBounds().Origin * C->GetComponentScale();
		C->SetWorldLocationAndRotation(Ctr - Rot.RotateVector(O), Rot);
	}
	void BuildWeaponMesh(AActor* A, USkeletalMeshComponent* SK)
	{
		if (UStaticMeshComponent* Old = WeaponMesh.Get()) Old->DestroyComponent();
		WeaponMesh = nullptr;
		if (!A || !SK || Weapon == WGatling) return;
		UStaticMesh* M = LoadRelic(WeaponRelic[Weapon]);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(true);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
		if (M) { C->SetStaticMesh(M); FitMesh(C, M, WeaponSize[Weapon]); }
		else { C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cone"))); C->SetRelativeScale3D(FVector(0.3f)); C->SetMaterial(0, ZoogiUtil::GlowMID(A, WeaponCol, 2.f)); }
		C->RegisterComponent(); C->AttachToComponent(SK, FAttachmentTransformRules::KeepWorldTransform);
		WeaponMesh = C;
		UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: %s in hand (%s)"), WeaponName[Weapon], M ? *M->GetName() : TEXT("mesh missing - glow cone"));
	}
	AActor* SpawnThrown(UWorld* W, int32 Kind, const FVector& At)
	{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), SP);
		if (!A) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A, TEXT("Thrown"));
		UStaticMesh* M = LoadRelic(WeaponRelic[Kind]);
		if (M) { C->SetStaticMesh(M); FitMesh(C, M, WeaponSize[Kind] * (Kind == WDisc ? 1.25f : 1.f)); }
		else { C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); C->SetRelativeScale3D(FVector(0.35f)); C->SetMaterial(0, ZoogiUtil::GlowMID(A, WeaponCol, 2.f)); }
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(true);
		A->SetRootComponent(C); C->RegisterComponent();
		return A;
	}
}

void AZoogiGameMode::RoyaleHeroStart()
{
	using namespace ZoogiHero;
	if (AActor* A = H.Actor.Get()) A->Destroy();
	for (FRelic& R : Relics) if (AActor* A = R.Actor.Get()) A->Destroy();
	Relics.Reset(); Held = FHeld(); H = FState(); NextDrop = 18.f; NextType = 0;
	for (FThrow& Tw : Throws) if (AActor* Ta = Tw.A.Get()) Ta->Destroy();
	Throws.Reset(); WeaponMesh = nullptr; WSwing = 0.f;
	{	// b120: the equipped weapon (inventory WEAPON tab); -ZoogiWeapon=<key> overrides
		FString Wk;
		if (const UZoogiFrontProfileSubsystem* FPs = UZoogiFrontProfileSubsystem::Get(this)) if (!FPs->Data().EquippedWeapon.IsNone()) Wk = FPs->Data().EquippedWeapon.ToString();
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiWeapon="), Wk);
		Weapon = WeaponFromKey(Wk);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiWeaponCycle"))) Weapon = WGrenade;
		UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon equipped: %s"), WeaponName[Weapon]);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHero")) || !Balls.IsValidIndex(Royale.HumanIndex)) return;
	AZoogiBall* B = Balls[Royale.HumanIndex];
	if (!B) return;
	H.Form = FormFor(B->Def.Name); H.Owner = Royale.HumanIndex;
	const FForm& Fm = Forms[H.Form];
	// Oct 31 b44 (Evan: "not liking how the model looks - a generic model for now"): Epic's UEFN mannequin, coloured per Zoogi, plays the Game
	// Animation Sample clips natively (no retarget) + the crystal heroes' actions retargeted onto it (Scripts/royale/retarget_actions.py).
	// -ZoogiHeroTripo = the crystal Tripo rigs again (they stay in /Game/Zoogis/Ultimates for the final models).
	static const TCHAR* GenMeshPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	const bool bTripoHero = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHeroTripo"));
	// Nov 1 (Evan: "we will use this for all characters"): Fab "Mecha Robot: Aurex" (Fieldy Panda) is every Zoogi's hero model, recoloured per
	// Zoogi; the mannequin clips are retargeted onto it by Scripts/royale/aurex_setup.py into /Game/Mecha_Aurex/Anims (same sub-folders / names).
	// -ZoogiNoAurex = the UEFN mannequin stand-in again.
	// b233 (Evan: Voyager "B"): the Voyager character for this Zoogi is the hero (-ZoogiNoVoyagerHero = the Aurex mech of b45)
	FString VoyName;
	USkeletalMesh* VoyMesh = (bTripoHero || !ZoogiShooter::On(this) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVoyagerHero"))) ? nullptr : ZoogiVoyager::Character(B->Def.Name, H.Owner, &VoyName);
	const bool bVoy = VoyMesh != nullptr;
	USkeletalMesh* Mesh = (bVoy || bTripoHero || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAurex"))) ? nullptr
		: LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex.SK_Mecha_Aurex"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	const bool bAurex = Mesh != nullptr;
	if (!Mesh && !bTripoHero && !bVoy) Mesh = LoadObject<USkeletalMesh>(nullptr, GenMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const bool bGeneric = Mesh != nullptr;
	if (bVoy) Mesh = VoyMesh; // b233
	const FString CR = bAurex ? FString(TEXT("/Game/Mecha_Aurex/Anims")) : FString(TEXT("/Game/Characters/UEFN_Mannequin"));
	if (!Mesh) Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/%s.%s"), Fm.Folder, Fm.Folder, Fm.Folder), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mesh) { UE_LOG(LogZoogi, Warning, TEXT("ROYALE hero: %s missing - old model stays"), Fm.Folder); return; }
	H.Keep.Emplace(Mesh);
	if (!bVoy) for (const TCHAR* C : { TEXT("idle"), TEXT("run"), TEXT("jump"), TEXT("fall"), TEXT("cast_a_spell"), TEXT("box_01"), TEXT("box_02"), TEXT("front_kick_01"), TEXT("slash"), TEXT("hit_to_body_01"), TEXT("cheer") })
	{
		const FString An = bGeneric ? FString::Printf(TEXT("GEN_%s"), C) : FString::Printf(TEXT("%s_Anim_%s"), Fm.Prefix, C);
		const FString Dir = bGeneric ? CR + TEXT("/Actions") : FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s"), Fm.Folder);
		if (UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Dir, *An, *An), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{ H.Keep.Emplace(S); H.Clips.Add(C, S); }
	}
	// Oct 31 b42: natural locomotion from Epic's Game Animation Sample (walk / run / strafe / sprint / jump / land), -ZoogiNoGasp = old clips
	if (!bVoy && (bGeneric || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoGasp"))))
	{
		static const TCHAR* G[][2] = {
			{ TEXT("g_idle"), TEXT("Stand_Idle_Loop") }, { TEXT("g_sprint"), TEXT("Sprint_Loop_F") }, { TEXT("g_fall"), TEXT("Jump_Loop_Fall") },
			{ TEXT("g_jump_run"), TEXT("Jump_F_Start_Run_Lfoot") }, { TEXT("g_jump_stand"), TEXT("Jump_F_Start_Stand_Lfoot") },
			{ TEXT("g_land_run"), TEXT("Jump_F_Land_Run_Light_Lfoot") }, { TEXT("g_land_light"), TEXT("Jump_F_Land_Stand_Light_Lfoot") },
			{ TEXT("g_land_heavy"), TEXT("Jump_F_Land_Stand_Heavy_Lfoot") }, { TEXT("g_land_roll"), TEXT("Jump_F_Land_Roll_Lfoot") } };
		auto Load = [&](const FString& Key, const FString& Name)
		{
			FString Path;
			if (bGeneric)
			{
				const TCHAR* Sub = Name.StartsWith(TEXT("Stand_Idle")) ? TEXT("Idle") : Name.StartsWith(TEXT("Walk")) ? TEXT("Walk") : Name.StartsWith(TEXT("Run")) ? TEXT("Run") : Name.StartsWith(TEXT("Sprint")) ? TEXT("Sprint") : TEXT("Jump");
				const FString An = TEXT("M_Neutral_") + Name;
				Path = FString::Printf(TEXT("%s/Animations/%s/%s.%s"), *CR, Sub, *An, *An);
			}
			else { const FString An = TEXT("GASP_M_Neutral_") + Name; Path = FString::Printf(TEXT("/Game/Zoogis/Ultimates/%s/GASP/%s.%s"), Fm.Folder, *An, *An); }
			if (UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{ H.Keep.Emplace(S); H.Clips.Add(Key, S); }
		};
		for (const auto& Pr : G) Load(Pr[0], Pr[1]);
		for (const TCHAR* D : { TEXT("F"), TEXT("B"), TEXT("LL"), TEXT("RR"), TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") })
		{ Load(FString(TEXT("g_walk_")) + D, FString(TEXT("Walk_Loop_")) + D); Load(FString(TEXT("g_run_")) + D, FString(TEXT("Run_Loop_")) + D); }
		H.bGasp = H.Clips.Contains(TEXT("g_run_F")) && H.Clips.Contains(TEXT("g_idle"));
		UE_LOG(LogZoogi, Log, TEXT("ROYALE hero GASP locomotion: %s"), H.bGasp ? TEXT("on") : TEXT("missing - Tripo clips"));
	}
	// Oct 31 b44 (Evan: "when standing it's 3rd person view like this with a gun"): Lyra's rifle locomotion / fire / melee / hit reacts,
	// retargeted onto the mannequin (Scripts/royale/retarget_rifle.py -> /Game/Characters/UEFN_Mannequin/Rifle/R_*), + Lyra's rifle mesh
	if (bGeneric && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoRifle")))
	{
		static const TCHAR* RC[][2] = {
			{ TEXT("r_idle"), TEXT("MM_Rifle_Idle_Hipfire") }, { TEXT("r_ads"), TEXT("MM_Rifle_Idle_ADS") },
			{ TEXT("r_jog_F"), TEXT("MM_Rifle_Jog_Fwd") }, { TEXT("r_jog_B"), TEXT("MM_Rifle_Jog_Bwd") }, { TEXT("r_jog_L"), TEXT("MM_Rifle_Jog_Left") }, { TEXT("r_jog_R"), TEXT("MM_Rifle_Jog_Right") },
			{ TEXT("r_walk_F"), TEXT("MM_Rifle_Walk_Fwd") }, { TEXT("r_walk_B"), TEXT("MM_Rifle_Walk_Bwd") }, { TEXT("r_walk_L"), TEXT("MM_Rifle_Walk_Left") }, { TEXT("r_walk_R"), TEXT("MM_Rifle_Walk_Right") },
			{ TEXT("r_jump"), TEXT("MM_Rifle_Jump_Start") }, { TEXT("r_fall"), TEXT("MM_Rifle_Jump_Fall_Loop") }, { TEXT("r_land"), TEXT("MM_Rifle_Jump_Fall_Land") },
			{ TEXT("r_fire"), TEXT("MM_Rifle_Fire") }, { TEXT("r_melee"), TEXT("MM_Rifle_Melee") }, { TEXT("r_hit"), TEXT("MM_HitReact_Front_Lgt_01") },
			{ TEXT("r_cheer"), TEXT("MF_Emote_FingerGuns") }, { TEXT("r_dash"), TEXT("MM_Dash_Forward") } };
		for (const auto& Pr : RC)
		{
			const FString An = FString(TEXT("R_")) + Pr[1];
			if (UAnimSequence* S = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/Rifle/%s.%s"), *CR, *An, *An), nullptr, LOAD_NoWarn | LOAD_Quiet)) { H.Keep.Emplace(S); H.Clips.Add(Pr[0], S); }
		}
		H.bRifle = H.Clips.Contains(TEXT("r_idle")) && H.Clips.Contains(TEXT("r_jog_F"));
		if (H.bRifle)
		{
			// the shooter actions replace the crystal hero's brawl clips (every HeroAction call keeps its key)
			auto Alias = [&](const TCHAR* To, const TCHAR* From) { if (UAnimSequence* const* S = H.Clips.Find(From)) H.Clips.Add(To, *S); };
			// (b45: no fire pose - the Gatling fires continuously from the hip-fire pose; no melee clips)
			Alias(TEXT("hit_to_body_01"), TEXT("r_hit")); Alias(TEXT("cheer"), TEXT("r_cheer"));
		}
		UE_LOG(LogZoogi, Log, TEXT("ROYALE hero rifle set: %s"), H.bRifle ? TEXT("on") : TEXT("missing"));
	}
	if (bVoy)
	{	// b233: Voyager's own clips (UE4 mannequin skeleton, notify tracks stripped) under the keys the hero tick plays
		static const TCHAR* VC[][2] = {
			{ TEXT("r_idle"), TEXT("Soldier/Anim_Idle_Rifle_Hip") }, { TEXT("r_ads"), TEXT("Soldier/Anim_Idle_Rifle_Hip") },
			{ TEXT("r_jog_F"), TEXT("Soldier/Anim_Jog_Fwd_Rifle") }, { TEXT("r_jog_B"), TEXT("Soldier/Anim_Jog_Bwd_Rifle") }, { TEXT("r_jog_L"), TEXT("Soldier/Anim_Jog_Lt_Rifle") }, { TEXT("r_jog_R"), TEXT("Soldier/Anim_Jog_Rt_Rifle") },
			{ TEXT("r_walk_F"), TEXT("Soldier/Anim_Jog_Fwd_Rifle") }, { TEXT("r_walk_B"), TEXT("Soldier/Anim_Jog_Bwd_Rifle") }, { TEXT("r_walk_L"), TEXT("Soldier/Anim_Jog_Lt_Rifle") }, { TEXT("r_walk_R"), TEXT("Soldier/Anim_Jog_Rt_Rifle") },
			{ TEXT("r_jump"), TEXT("Soldier/Anim_Jump_From_Jog") }, { TEXT("r_fall"), TEXT("Soldier/ThirdPerson/Anim_ThirdPersonJump_Loop") }, { TEXT("r_land"), TEXT("CC0Animations/Anim_AL_Standard_Anim_Rig_Jump_Land") },
			{ TEXT("r_melee"), TEXT("Soldier/MotionCC0/Anim_Punch_Cross") }, { TEXT("r_hit"), TEXT("Soldier/MotionCC0/Anim_Hit_Chest") },
			{ TEXT("r_cheer"), TEXT("ActionRPG/Anim_Sword_Cheer") }, { TEXT("r_dash"), TEXT("ActionRPG/Anim_Dodge_Small_Forward_Roll") },
			{ TEXT("idle"), TEXT("Soldier/ThirdPerson/Anim_ThirdPersonIdle") }, { TEXT("run"), TEXT("Soldier/ThirdPerson/Anim_ThirdPersonRun") },
			{ TEXT("jump"), TEXT("Soldier/ThirdPerson/Anim_ThirdPersonJump_Start") }, { TEXT("fall"), TEXT("Soldier/ThirdPerson/Anim_ThirdPersonJump_Loop") },
			{ TEXT("cast_a_spell"), TEXT("CC0Animations/Anim_AL_Standard_Anim_Rig_Spell_Simple_Shoot") }, { TEXT("box_01"), TEXT("Soldier/MotionCC0/Anim_Fighting_Left_Jab") },
			{ TEXT("box_02"), TEXT("Soldier/MotionCC0/Anim_Fighting_Right_Jab") }, { TEXT("front_kick_01"), TEXT("ActionRPG/Anim_Chestopen_Kick") },
			{ TEXT("slash"), TEXT("ActionRPG/Anim_Combat_Blade_Swing_Far") }, { TEXT("hit_to_body_01"), TEXT("Soldier/MotionCC0/Anim_Hit_Chest") }, { TEXT("cheer"), TEXT("ActionRPG/Anim_Sword_Cheer") } };
		for (const auto& Pr : VC) if (UAnimSequence* S = ZoogiVoyager::Anim(Pr[1])) { H.Keep.Emplace(S); H.Clips.Add(Pr[0], S); }
		H.bGasp = false; H.bRifle = H.Clips.Contains(TEXT("r_idle")) && H.Clips.Contains(TEXT("r_jog_F"));
		UE_LOG(LogZoogi, Log, TEXT("ROYALE hero: Voyager %s for %s (%d clips, rifle %s)"), *VoyName, *B->Def.Name, H.Clips.Num(), H.bRifle ? TEXT("on") : TEXT("missing"));
	}
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(B->BallLocation()), SP);
	if (!A) return;
	USkeletalMeshComponent* SK = NewObject<USkeletalMeshComponent>(A, TEXT("RoyaleHero"));
	SK->SetSkeletalMesh(Mesh);
	if (bAurex)
	{
		// Nov 1: Aurex colour zones (ID mask R / G / B) + glow per Zoogi, matched to Evan's ball designs
		if (UMaterialInterface* AB = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor.MI_MechaAurex_CustomColor"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const FString N = B->Def.Name.ToLower();
			FLinearColor Z[4];
			auto Pal = [&Z](FLinearColor Ra, FLinearColor Ga, FLinearColor Ba, FLinearColor Ea) { Z[0] = Ra; Z[1] = Ga; Z[2] = Ba; Z[3] = Ea; };
			if (N.Contains(TEXT("fros")) || N == TEXT("pinpoint")) Pal(FLinearColor(0.12f, 0.42f, 1.f), FLinearColor(0.86f, 0.9f, 0.96f), FLinearColor(0.02f, 0.04f, 0.12f), FLinearColor(0.3f, 0.85f, 1.f));
			else if (N == TEXT("luma") || N == TEXT("lumi")) Pal(FLinearColor(1.f, 0.62f, 0.08f), FLinearColor(0.9f, 0.88f, 0.82f), FLinearColor(0.05f, 0.04f, 0.03f), FLinearColor(1.f, 0.8f, 0.3f));
			else if (N == TEXT("vex") || N.Contains(TEXT("dark"))) Pal(FLinearColor(0.36f, 0.08f, 0.78f), FLinearColor(0.03f, 0.02f, 0.05f), FLinearColor(0.15f, 0.15f, 0.18f), FLinearColor(0.62f, 0.22f, 1.f));
			else if (N == TEXT("hotstreak")) Pal(FLinearColor(0.03f, 0.02f, 0.02f), FLinearColor(1.f, 0.34f, 0.04f), FLinearColor(0.12f, 0.1f, 0.1f), FLinearColor(1.f, 0.45f, 0.05f));
			else if (N == TEXT("blue")) Pal(FLinearColor(0.05f, 0.2f, 0.78f), FLinearColor(0.26f, 0.28f, 0.33f), FLinearColor(0.02f, 0.03f, 0.1f), FLinearColor(0.2f, 0.6f, 1.f));
			else if (N == TEXT("terra") || N == TEXT("moss")) Pal(FLinearColor(0.16f, 0.2f, 0.13f), FLinearColor(0.2f, 0.55f, 0.12f), FLinearColor(0.05f, 0.05f, 0.04f), FLinearColor(0.3f, 1.f, 0.2f));
			else if (N == TEXT("zephyr")) Pal(FLinearColor(0.04f, 0.72f, 0.74f), FLinearColor(0.8f, 0.82f, 0.86f), FLinearColor(0.02f, 0.12f, 0.14f), FLinearColor(0.3f, 1.f, 0.95f));
			else if (N == TEXT("ember")) Pal(FLinearColor(0.85f, 0.1f, 0.55f), FLinearColor(0.2f, 0.04f, 0.15f), FLinearColor(0.05f, 0.02f, 0.04f), FLinearColor(1.f, 0.3f, 0.7f));
			else if (N == TEXT("bolt")) Pal(FLinearColor(1.f, 0.78f, 0.06f), FLinearColor(0.03f, 0.03f, 0.035f), FLinearColor(0.2f, 0.2f, 0.22f), FLinearColor(0.2f, 0.8f, 1.f));
			else { const FLinearColor Pc = ZoogiUtil::PlayerColor(H.Owner); Pal(Pc, FLinearColor(0.82f, 0.84f, 0.88f), FLinearColor(0.03f, 0.03f, 0.04f), Pc); }
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(AB, A);
			MID->SetVectorParameterValue(TEXT("Color_Zone_R"), Z[0]); MID->SetVectorParameterValue(TEXT("Color_Zone_G"), Z[1]);
			MID->SetVectorParameterValue(TEXT("Color_Zone_B"), Z[2]); MID->SetVectorParameterValue(TEXT("Emissive_Color"), Z[3]);
			for (int32 m = 0; m < SK->GetNumMaterials(); ++m) SK->SetMaterial(m, MID);
		}
	}
	else if (bGeneric)
	{
		// per-Zoogi colours on the mannequin's body-part mask: segments = the Zoogi colour, joints = a darker shade
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Characters/UEFN_Mannequin/Materials/MI_UEFN_Mannequin_Blue.MI_UEFN_Mannequin_Blue"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const FString N = B->Def.Name.ToLower();
			FLinearColor Col = N == TEXT("luma") || N == TEXT("lumi") ? FLinearColor(0.12f, 0.75f, 0.25f) : N == TEXT("hotstreak") ? FLinearColor(1.f, 0.32f, 0.05f)
				: N == TEXT("bolt") ? FLinearColor(1.f, 0.78f, 0.06f) : (N == TEXT("vex") || N.Contains(TEXT("dark"))) ? FLinearColor(0.75f, 0.06f, 0.12f)
				: (N == TEXT("blue") || N.Contains(TEXT("fros"))) ? FLinearColor(0.12f, 0.36f, 1.f) : ZoogiUtil::PlayerColor(H.Owner);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, A);
			static const TCHAR* Joints[] = { TEXT("Neck"), TEXT("Waist"), TEXT("Shoulder"), TEXT("Elbow"), TEXT("Wrist"), TEXT("Hip"), TEXT("Knee"), TEXT("Ankle") };
			static const TCHAR* Parts[] = { TEXT("Head"), TEXT("Neck"), TEXT("Torso"), TEXT("Waist"), TEXT("Pelvis"), TEXT("L Shoulder"), TEXT("L UpperArm"), TEXT("L Elbow"), TEXT("L LowerArm"), TEXT("L Wrist"), TEXT("L Hand"),
				TEXT("R Shoulder"), TEXT("R UpperArm"), TEXT("R Elbow"), TEXT("R LowerArm"), TEXT("R Wrist"), TEXT("R Hand"), TEXT("L Hip"), TEXT("L Thigh"), TEXT("L Knee"), TEXT("L Calf"), TEXT("L Ankle"), TEXT("L Foot"),
				TEXT("R Hip"), TEXT("R Thigh"), TEXT("R Knee"), TEXT("R Calf"), TEXT("R Ankle"), TEXT("R Foot") };
			const FLinearColor Dark(Col.R * 0.22f + 0.03f, Col.G * 0.22f + 0.03f, Col.B * 0.22f + 0.04f, 1.f);
			for (const TCHAR* P : Parts)
			{
				bool bJoint = false; for (const TCHAR* J : Joints) if (FCString::Strstr(P, J)) bJoint = true;
				MID->SetVectorParameterValue(P, bJoint ? Dark : Col);
			}
			for (int32 m = 0; m < SK->GetNumMaterials(); ++m) SK->SetMaterial(m, MID);
		}
	}
	SK->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SK->SetCastShadow(true);
	SK->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SK->bEnableUpdateRateOptimizations = false;
	SK->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	SK->RegisterComponent();
	A->SetRootComponent(SK);
	if (H.bRifle)
	{
		// b45 (Evan): a Gatling gun - built from parts so it needs no asset: a gun root along +Y (the aim), receiver + ammo drum + grip,
		// six barrels on a spinning carrier, a muzzle ring in the Zoogi's colour. -ZoogiRifleMesh = Lyra's rifle instead.
		UStaticMeshComponent* RC2 = NewObject<UStaticMeshComponent>(A, TEXT("HeroGun"));
		RC2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RC2->SetUsingAbsoluteLocation(true); RC2->SetUsingAbsoluteRotation(true); RC2->SetUsingAbsoluteScale(true);
		RC2->RegisterComponent(); RC2->AttachToComponent(SK, FAttachmentTransformRules::KeepWorldTransform); RC2->SetWorldScale3D(FVector(1.f));
		H.Rifle = RC2;
		UStaticMesh* RM = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRifleMesh")) ? LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Weapons/Rifle/Mesh/SM_Rifle.SM_Rifle"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		USkeletalMesh* VoyGun = (bVoy && !RM && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGatling"))) ? ZoogiVoyager::Gun(TEXT("HeavyMachineGun")) : nullptr; // b233 (C)
		if (RM) { RC2->SetStaticMesh(RM); H.Keep.Emplace(RM); }
		else if (VoyGun)
		{	// b233: Voyager's Heavy Machine Gun, its long axis along the aim (+Y of the gun root), centred on the hand
			USkeletalMeshComponent* Gc = NewObject<USkeletalMeshComponent>(A, TEXT("VoyagerGun"));
			Gc->SetSkeletalMeshAsset(VoyGun); Gc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Gc->SetupAttachment(RC2);
			static float GunYaw = -1000.f; if (GunYaw < -999.f) { GunYaw = 0.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiVoyGunYaw="), GunYaw); }
			const FBox Gb = VoyGun->GetBounds().GetBox(); const FRotator Gr(0.f, GunYaw, 0.f);
			Gc->SetRelativeRotation(Gr); Gc->SetRelativeLocation(-Gr.RotateVector(Gb.GetCenter()) + FVector(0.f, 12.f, 0.f));
			Gc->RegisterComponent(); H.Keep.Emplace(VoyGun);
		}
		else
		{
			UStaticMesh* Cyl = ZoogiUtil::BasicShape(TEXT("Cylinder")); UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
			UMaterialInterface* Metal = ZoogiUtil::SurfaceMID(A, ZoogiUtil::Hex(TEXT("#3B4048")), 0.35f, ZoogiUtil::Hex(TEXT("#3B4048")), 0.f, 0.85f);
			UMaterialInterface* Dark = ZoogiUtil::SurfaceMID(A, ZoogiUtil::Hex(TEXT("#22252B")), 0.4f, ZoogiUtil::Hex(TEXT("#22252B")), 0.f, 0.9f);
			UMaterialInterface* Glow = ZoogiUtil::GlowMID(A, ZoogiUtil::PlayerColor(H.Owner), 2.2f);
			auto Part = [&](USceneComponent* Parent, UStaticMesh* M, UMaterialInterface* Mat, const FVector& Loc, const FVector& Size, bool bAlongY)
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
				C->SetStaticMesh(M); C->SetMaterial(0, Mat); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false);
				C->SetupAttachment(Parent); C->SetRelativeLocation(Loc); C->SetRelativeRotation(bAlongY ? FRotator(0.f, 0.f, 90.f) : FRotator::ZeroRotator);
				C->SetRelativeScale3D(Size / 100.f); C->RegisterComponent();
				return C;
			};
			Part(RC2, Cyl, Metal, FVector(0.f, -2.f, 0.f), FVector(15.f, 15.f, 34.f), true);       // receiver
			Part(RC2, Cyl, Dark, FVector(0.f, -6.f, -14.f), FVector(17.f, 17.f, 12.f), true);      // ammo drum under it
			Part(RC2, Cube, Dark, FVector(0.f, -14.f, -9.f), FVector(4.f, 5.f, 12.f), false);      // grip
			Part(RC2, Cube, Metal, FVector(0.f, 2.f, 9.f), FVector(3.f, 22.f, 3.f), false);       // carry rail
			USceneComponent* Spin = NewObject<USceneComponent>(A); Spin->SetupAttachment(RC2); Spin->SetRelativeLocation(FVector(0.f, 15.f, 0.f)); Spin->RegisterComponent();
			for (int32 b = 0; b < 6; ++b) { const float An = PI / 3.f * b; Part(Spin, Cyl, Dark, FVector(FMath::Cos(An) * 4.6f, 22.f, FMath::Sin(An) * 4.6f), FVector(3.2f, 3.2f, 44.f), true); }
			Part(Spin, Cyl, Metal, FVector(0.f, 8.f, 0.f), FVector(13.f, 13.f, 2.5f), true);       // barrel clamp
			Part(Spin, Cyl, Glow, FVector(0.f, 41.f, 0.f), FVector(13.5f, 13.5f, 2.f), true);      // muzzle ring
			H.GunSpin = Spin;
		}
		for (int32 s = 0; s < Mesh->NumSockets(); ++s) if (const USkeletalMeshSocket* So = Mesh->GetSocketByIndex(s)) UE_LOG(LogZoogi, Log, TEXT("ROYALE hero socket %s on %s"), *So->SocketName.ToString(), *So->BoneName.ToString());
	}
	const float Hm = FMath::Max(10.f, (float)Mesh->GetBounds().BoxExtent.Z * 2.f);
	SK->SetWorldScale3D(FVector(H.Height / Hm));
	if (bVoy) ZoogiVoyager::Dress(SK, B); // b235: your Zoogi's colours + its ball as the head
	// right hand bone (relics ride in it): Mixamo-style names first
	const FReferenceSkeleton& RS = Mesh->GetRefSkeleton();
	for (int32 i = 0; i < RS.GetNum() && H.HandBone.IsNone(); ++i)
	{
		const FString Bn = RS.GetBoneName(i).ToString().ToLower();
		if ((Bn.Contains(TEXT("righthand")) || Bn.Contains(TEXT("hand_r")) || Bn.Contains(TEXT("r_hand")) || Bn.EndsWith(TEXT("hand.r"))) && !Bn.Contains(TEXT("index")) && !Bn.Contains(TEXT("thumb"))) H.HandBone = RS.GetBoneName(i);
	}
	H.Actor = A; H.Mesh = SK; H.Yaw = Royale.P.IsValidIndex(H.Owner) ? Royale.P[H.Owner].FacingYaw : 0.f;
	BuildWeaponMesh(A, SK); // b120
	// ball form = Evan's ball design for this Zoogi (M_ZoogiBallSkin + T_Ball_<name>, Scripts/royale/make_ball_kit.py)
	{
		UMaterialInterface* BM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Zoogis/Balls/M_ZoogiBallSkin.M_ZoogiBallSkin"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const FString TexN = BallFor(B->Def.Name);
		UTexture* BT = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/Zoogis/Balls/%s.%s"), *TexN, *TexN), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (BM && BT)
		{
			UStaticMeshComponent* BS = NewObject<UStaticMeshComponent>(A, TEXT("HeroBallSkin"));
			BS->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); BS->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BM, A); MID->SetTextureParameterValue(TEXT("Tex"), BT); BS->SetMaterial(0, MID);
			BS->SetUsingAbsoluteLocation(true); BS->SetUsingAbsoluteRotation(true); BS->SetUsingAbsoluteScale(true);
			BS->RegisterComponent(); BS->AttachToComponent(SK, FAttachmentTransformRules::KeepWorldTransform);
			BS->SetWorldScale3D(FVector(ZoogiConst::ZoogiRadius * 2.04f / 100.f)); BS->SetHiddenInGame(true);
			H.BallSkin = BS; H.Keep.Emplace(BT);
		}
		UE_LOG(LogZoogi, Log, TEXT("ROYALE hero ball form: %s"), (BM && BT) ? *TexN : TEXT("missing (ball kit not imported) - the mech stays on in ball form"));
	}
	// the player starts as the crystal mech (walk / run / jump / shoot); TRANSFORM rolls up into the ball
	// (RoyaleHeroTick flips it once Royale.Step == Fight - RoyaleTransform refuses before that)
	UE_LOG(LogZoogi, Log, TEXT("ROYALE hero: P%d %s -> %s (%d clips, scale %.2f, hand bone %s); bots keep the transform model"),
		H.Owner + 1, *B->Def.Name, bAurex ? TEXT("Mecha Aurex") : bGeneric ? TEXT("UEFN mannequin (generic stand-in)") : Fm.Folder, H.Clips.Num(), H.Height / Hm, *H.HandBone.ToString());
}

static void HeroPlay(const FString& Clip, bool bLoop, float Rate)
{
	using namespace ZoogiHero;
	USkeletalMeshComponent* SK = H.Mesh.Get();
	UAnimSequence* const* S = H.Clips.Find(Clip);
	if (!SK || !S || !*S) return;
	if (H.Cur != Clip) { SK->PlayAnimation(*S, bLoop); H.Cur = Clip; }
	if (!FMath::IsNearlyEqual(H.Rate, Rate, 0.02f)) { SK->SetPlayRate(Rate); H.Rate = Rate; }
}

static void HeroAction(const FString& Clip, float Seconds, float Rate = 1.2f)
{
	using namespace ZoogiHero;
	H.Cur.Reset(); // restart even if it is the same clip
	HeroPlay(Clip, false, Rate);
	H.ActionLeft = Seconds;
}

void AZoogiGameMode::RoyaleHeroTick(float Dt)
{
	using namespace ZoogiHero;
	const float Now = GetWorld()->GetTimeSeconds();
	// ---- hero model
	if (USkeletalMeshComponent* SK = H.Mesh.Get())
	{
		AZoogiBall* B = Balls.IsValidIndex(H.Owner) ? Balls[H.Owner].Get() : nullptr;
		if (H.bStartMech && Royale.Step == EZoogiRoyaleStep::Fight && Royale.P.IsValidIndex(H.Owner))
		{
			if (!Royale.P[H.Owner].bRobot) RoyaleTransform(H.Owner, TEXT("hero start"));
			if (Royale.P[H.Owner].bRobot) { Royale.P[H.Owner].MorphCD = 0.f; Royale.P[H.Owner].FireCD = 0.f; H.bStartMech = false; H.bWasRobot = true; if (Weapon != WGatling && B) AddPopup(FString::Printf(TEXT("%s!"), WeaponName[Weapon]), B->BallLocation() + FVector(0, 0, 170.f), WeaponCol); }
		}
		const FZoogiRoyalePlayer* R = Royale.P.IsValidIndex(H.Owner) ? &Royale.P[H.Owner] : nullptr;
		if (B && R)
		{
			// the old ball / robot of the human stays simulated but is not drawn
			if (B->Morph && B->Morph->Mesh) B->Morph->Mesh->SetHiddenInGame(true);
			if (B->Visual) B->Visual->SetHiddenInGame(true);
			const bool bShow = !B->bOut && R->RespawnIn < 0.f && (R->ShieldLeft <= 0.f || FMath::Fmod(Now, 0.2f) < 0.12f); // respawn shield blink
			UStaticMeshComponent* BS = H.BallSkin.Get();
			const bool bBallForm = BS && !R->bRobot;
			SK->SetHiddenInGame(!bShow || bBallForm, true); // b237: the head / pads hide with the soldier (the ball skin and gun are set right after)
			if (BS)
			{
				BS->SetHiddenInGame(!bShow || !bBallForm);
				if (bBallForm && B->Sphere) BS->SetWorldLocationAndRotation(B->BallLocation(), B->Sphere->GetComponentQuat());
				if (R->bRobot != H.bWasRobot) { AZoogiFX::Spawn(GetWorld(), B->BallLocation(), FLinearColor(0.6f, 0.85f, 1.f), 180.f, 0.35f, false, 10.f); H.bWasRobot = R->bRobot; if (R->bRobot) HeroAction(TEXT("cheer"), 0.5f, 1.8f); }
			}
			const FVector L = B->BallLocation(), V = B->GetBallVelocity();
			const float Sp = FVector2D(V).Size();
			float WantYaw = H.Yaw;
			if (R->bRobot || R->Lock >= 0) WantYaw = R->FacingYaw;
			else if (Sp > 90.f) WantYaw = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
			H.Yaw = FMath::FixedTurn(H.Yaw, WantYaw, 720.f * Dt);
			static float YawOff = -90.f; static bool bInit = false;
			if (!bInit) { bInit = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiHeroYaw="), YawOff); }
			SK->SetWorldLocationAndRotation(L - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), FRotator(0.f, H.Yaw + YawOff, 0.f));
			// b44 rifle: rides the right hand, barrel along the aim (the crosshair ray), kicks back on each shot; hidden while a relic is held
			if (UStaticMeshComponent* Rf = H.Rifle.Get())
			{
				const bool bRifleOn = !SK->bHiddenInGame && R->bRobot && !Held.InHand.IsValid() && Weapon == WGatling; // b120: another weapon equipped -> no Gatling
				Rf->SetHiddenInGame(!bRifleOn);
				if (bRifleOn)
				{
					const FVector Hand = !H.HandBone.IsNone() ? SK->GetBoneLocation(H.HandBone) : L + FVector(0, 0, 60.f);
					FVector Aim = (R->AimPoint - Hand).GetSafeNormal();
					// a crosshair hit right in front of the gun (a wall at arm's length) would twist it toward the camera: then just point ahead
					if (Aim.IsNearlyZero() || FVector::Dist(R->AimPoint, Hand) < 600.f || FVector::DotProduct(Aim, FRotator(0.f, H.Yaw, 0.f).Vector()) < 0.6f) Aim = FRotator(0.f, H.Yaw, 0.f).Vector();
					H.RifleKick = FMath::Max(0.f, H.RifleKick - Dt * 9.f);
					if (USceneComponent* Sp2 = H.GunSpin.Get()) { const bool bFiring = Now - H.LastShot < 0.25f; H.SpinAng = FMath::Fmod(H.SpinAng + Dt * (bFiring ? 1800.f : 140.f), 360.f); Sp2->SetRelativeRotation(FRotator(H.SpinAng, 0.f, 0.f)); }
					Rf->SetWorldLocationAndRotation(Hand + Aim * (10.f - 3.f * H.RifleKick) + FVector(0, 0, -4.f), FRotationMatrix::MakeFromYZ(Aim, FVector::UpVector).Rotator());
				}
			}
			// b120: the equipped weapon in the right hand (sword swings with each cut; the disc is gone while it flies)
			{
				static const bool bCycle = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWeaponCycle"));
				static float CycleAt = 9.f;
				if (bCycle && Royale.Step == EZoogiRoyaleStep::Fight && Royale.FightTime > CycleAt)
				{
					CycleAt = Royale.FightTime + 9.f;
					Weapon = Weapon % (NumWeapons - 1) + 1; // grenade -> sword -> staff -> disc -> grenade
					BuildWeaponMesh(H.Actor.Get(), SK);
					AddPopup(FString::Printf(TEXT("%s!"), WeaponName[Weapon]), L + FVector(0, 0, 170.f), WeaponCol);
				}
			}
			WSwing = FMath::Max(0.f, WSwing - Dt * 3.2f);
			if (UStaticMeshComponent* Wm = WeaponMesh.Get())
			{
				const bool bWOn = !SK->bHiddenInGame && R->bRobot && !Held.InHand.IsValid() && !(Weapon == WDisc && DiscOut());
				Wm->SetHiddenInGame(!bWOn);
				if (bWOn)
				{
					const FVector Hand = !H.HandBone.IsNone() ? SK->GetBoneLocation(H.HandBone) : L + FVector(0, 0, 60.f);
					const FVector Fwd = FRotator(0.f, H.Yaw, 0.f).Vector(), Up = FVector::UpVector;
					const float Sz = WeaponSize[Weapon];
					if (Weapon == WSword)
					{	// raised over the shoulder, cutting down and forward on a swing
						const float K = FMath::Sin(FMath::Clamp(1.f - WSwing, 0.f, 1.f) * PI * 0.5f);
						const FVector Ax = WSwing > 0.f ? FMath::Lerp(Fwd * 0.9f - Up * 0.35f, Up * 0.85f + Fwd * 0.45f, K).GetSafeNormal() : (Up * 0.85f + Fwd * 0.45f).GetSafeNormal();
						PlaceMesh(Wm, Hand + Ax * Sz * 0.36f, FRotationMatrix::MakeFromZX(Ax, Fwd).Rotator());
					}
					else if (Weapon == WStaff) { const FVector Ax = (Up + Fwd * 0.18f).GetSafeNormal(); PlaceMesh(Wm, Hand + Ax * Sz * 0.18f, FRotationMatrix::MakeFromZX(-Ax, Fwd).Rotator()); } // the Tripo staff's orb is at its -Z end
					else if (Weapon == WDisc) PlaceMesh(Wm, Hand + Fwd * 18.f + Up * 8.f, FRotator(0.f, H.Yaw - 90.f, 0.f)); // ring facing the camera behind
					else PlaceMesh(Wm, Hand + Fwd * 8.f + Up * 4.f, FRotator(0.f, H.Yaw, 0.f));
				}
			}
			if (R->HurtAt > H.LastHurt + 0.01f && Now - R->HurtAt < 0.2f) { H.LastHurt = R->HurtAt; if (H.ActionLeft <= 0.f) HeroAction(TEXT("hit_to_body_01"), 0.4f, 1.4f); }
			H.ActionLeft -= Dt;
			if (Royale.Step == EZoogiRoyaleStep::Over && Royale.Winner == H.Owner) HeroPlay(TEXT("cheer"), true, 1.f);
			else if (H.bRifle && R->bRobot)
			{
				// ---- Oct 31 b44 shooter locomotion: rifle walk / jog in 4 directions relative to the aim, jump -> fall -> land
				const bool bAir = FMath::Abs(V.Z) > 160.f && Sp < 2500.f;
				H.AirT = bAir ? H.AirT + Dt : 0.f;
				if (!bAir && H.PrevVz < -380.f && H.ActionLeft <= 0.f) HeroAction(TEXT("r_land"), 0.3f, 1.3f);
				H.PrevVz = V.Z;
				if (H.ActionLeft <= 0.f)
				{
					if (bAir) { if (V.Z > 0.f && H.AirT < 0.3f) HeroPlay(TEXT("r_jump"), false, 1.2f); else HeroPlay(TEXT("r_fall"), true, 1.f); }
					else if (Sp < 60.f) HeroPlay(TEXT("r_idle"), true, 1.f);
					else
					{
						const float Rel = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) - H.Yaw);
						const float Ar = FMath::Abs(Rel);
						const TCHAR* Dir = Ar < 50.f ? TEXT("F") : Ar < 130.f ? (Rel > 0.f ? TEXT("R") : TEXT("L")) : TEXT("B");
						if (H.DirCand != Dir) { H.DirCand = Dir; H.DirHold = 0.f; }
						H.DirHold += Dt;
						if (H.DirHold > 0.12f || H.Cur == TEXT("r_idle")) H.DirCur = H.DirCand;
						if (Sp > 330.f) HeroPlay(TEXT("r_jog_") + H.DirCur, true, FMath::Clamp(Sp / 560.f, 0.7f, 1.6f));
						else HeroPlay(TEXT("r_walk_") + H.DirCur, true, FMath::Clamp(Sp / 230.f, 0.6f, 1.5f));
					}
				}
			}
			else if (H.bGasp)
			{
				// ---- Oct 31 b42 GASP locomotion: direction relative to the body (strafe while aiming), walk / run / sprint by speed, jump -> fall -> land
				const bool bAir = FMath::Abs(V.Z) > 160.f && Sp < 2500.f;
				H.AirT = bAir ? H.AirT + Dt : 0.f;
				if (!bAir && H.PrevVz < -380.f && H.ActionLeft <= 0.f)
				{
					const bool bHeavy = H.PrevVz < -1100.f;
					HeroAction(bHeavy ? (Sp > 500.f ? TEXT("g_land_roll") : TEXT("g_land_heavy")) : (Sp > 250.f ? TEXT("g_land_run") : TEXT("g_land_light")), bHeavy ? 0.55f : 0.3f, 1.25f);
				}
				H.PrevVz = V.Z;
				if (H.ActionLeft <= 0.f)
				{
					if (bAir) { if (V.Z > 0.f && H.AirT < 0.35f) HeroPlay(Sp > 250.f ? TEXT("g_jump_run") : TEXT("g_jump_stand"), false, 1.2f); else HeroPlay(TEXT("g_fall"), true, 1.f); }
					else if (Sp < 60.f) HeroPlay(TEXT("g_idle"), true, 1.f);
					else
					{
						const float Rel = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)) - H.Yaw);
						const float Ar = FMath::Abs(Rel);
						const TCHAR* Dir = Ar < 30.f ? TEXT("F") : Ar < 70.f ? (Rel > 0.f ? TEXT("FR") : TEXT("FL")) : Ar < 115.f ? (Rel > 0.f ? TEXT("RR") : TEXT("LL")) : Ar < 155.f ? (Rel > 0.f ? TEXT("BR") : TEXT("BL")) : TEXT("B");
						// hold a new direction for a moment before switching (no pops on the 30 / 70 deg lines)
						if (H.DirCand != Dir) { H.DirCand = Dir; H.DirHold = 0.f; }
						H.DirHold += Dt;
						if (H.DirHold > 0.12f || H.Cur == TEXT("g_idle")) H.DirCur = H.DirCand;
						if (Sp > 950.f && H.DirCur == TEXT("F")) HeroPlay(TEXT("g_sprint"), true, FMath::Clamp(Sp / 1150.f, 0.8f, 1.5f));
						else if (Sp > 330.f) HeroPlay(TEXT("g_run_") + H.DirCur, true, FMath::Clamp(Sp / 620.f, 0.7f, 1.6f));
						else HeroPlay(TEXT("g_walk_") + H.DirCur, true, FMath::Clamp(Sp / 240.f, 0.6f, 1.5f));
					}
				}
			}
			else if (H.ActionLeft <= 0.f)
			{
				if (FMath::Abs(V.Z) > 160.f && Sp < 2500.f) HeroPlay(V.Z > 0.f ? TEXT("jump") : TEXT("fall"), false, 1.f);
				else if (Sp < 70.f) HeroPlay(TEXT("idle"), true, 1.f);
				else HeroPlay(TEXT("run"), true, FMath::Clamp(Sp / 650.f, 0.45f, 1.5f)); // < ~300 cm/s reads as a walk
			}
		}
	}
	// ---- b120 thrown weapons: the grenade (arc, bursts on landing) and the halo disc (boomerang)
	if (Throws.Num())
	{
		const FZoogiRoyaleTune& Tn = FZoogiRoyaleTune::Get();
		const int32 Me = H.Owner;
		AZoogiBall* MyBall = Balls.IsValidIndex(Me) ? Balls[Me].Get() : nullptr;
		auto HitJ = [&](int32 j, const FVector& Push, int32 Dmg)
		{
			FZoogiRoyaleKick K; K.Victim = j; K.DeltaV = Push; Royale.Kicks.Add(K);
			Royale.P[j].LastHitBy = Me; Royale.P[j].LastHitAt = Clock; Royale.P[j].StunLeft = FMath::Max(Royale.P[j].StunLeft, 0.5f);
			RoyaleDamage(j, Me, Dmg, true, Balls[j]->BallLocation());
		};
		auto ForRivals = [&](TFunctionRef<void(int32, const FVector&)> Fn)
		{
			for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num(); ++j)
				if (j != Me && Balls[j] && !Balls[j]->bOut && Royale.P[j].IsIn() && Royale.P[j].RespawnIn < 0.f && Royale.P[j].ShieldLeft <= 0.f) Fn(j, Balls[j]->BallLocation());
		};
		FCollisionQueryParams Q(FName(TEXT("ZoogiThrow")), false);
		for (int32 k = Throws.Num() - 1; k >= 0; --k)
		{
			FThrow& Tw = Throws[k];
			AActor* A = Tw.A.Get();
			Tw.Age += Dt;
			bool bDone = !A || Royale.Step == EZoogiRoyaleStep::Over;
			if (!bDone && Tw.Kind == WGrenade)
			{
				const FVector P0 = Tw.P;
				Tw.V.Z -= 1800.f * Dt;
				Tw.P += Tw.V * Dt;
				FHitResult Hr;
				bool bBoom = GetWorld()->LineTraceSingleByObjectType(Hr, P0, Tw.P, FCollisionObjectQueryParams(ECC_WorldStatic), Q);
				if (bBoom) Tw.P = Hr.ImpactPoint + Hr.ImpactNormal * 10.f;
				ForRivals([&](int32 j, const FVector& Pj) { if (FVector::Dist(Pj, Tw.P) < ZoogiConst::ZoogiRadius + 30.f) bBoom = true; });
				if (Tw.Age > 3.f || Tw.P.Z < ZoogiConst::FallOutZ) bBoom = true;
				A->SetActorLocationAndRotation(Tw.P, FRotator(-Tw.Age * 620.f, Tw.Dir.Rotation().Yaw, 0.f));
				if (FMath::Fmod(Tw.Age, 0.05f) < Dt) AZoogiFX::Spawn(GetWorld(), Tw.P, WeaponCol, 34.f, 0.25f, false, 8.f); // sparkle trail
				if (bBoom)
				{
					int32 N = 0;
					ForRivals([&](int32 j, const FVector& Pj) { const FVector D = Pj - Tw.P; if (D.Size2D() < 430.f && FMath::Abs(D.Z) < 400.f) { HitJ(j, D.GetSafeNormal2D() * Tn.BoltKnock * 2.6f + FVector(0, 0, 260.f), 85); ++N; } });
					AZoogiFX::Spawn(GetWorld(), Tw.P, WeaponCol, 430.f, 0.6f, true, 10.f);
					AZoogiFX::Spawn(GetWorld(), Tw.P + FVector(0, 0, 50.f), FLinearColor::White, 200.f, 0.25f, false, 14.f);
					AZoogiFX::Stylized(GetWorld(), TEXT("ElectricShock"), Tw.P, 1.1f);
					ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.6f, 1.1f);
					UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: grenade burst at (%.0f,%.0f,%.0f) hit %d"), Tw.P.X, Tw.P.Y, Tw.P.Z, N);
					bDone = true;
				}
			}
			else if (!bDone && Tw.Kind == WDisc)
			{
				const FVector P0 = Tw.P;
				if (!Tw.bBack)
				{
					Tw.P += Tw.Dir * 1900.f * Dt;
					FHitResult Hr;
					if (Tw.Age > 0.6f || GetWorld()->LineTraceSingleByObjectType(Hr, P0, Tw.P, FCollisionObjectQueryParams(ECC_WorldStatic), Q)) { Tw.bBack = true; Tw.Hit.Reset(); }
				}
				else
				{
					const FVector Home = MyBall ? MyBall->BallLocation() + FVector(0, 0, 70.f) : Tw.P;
					const FVector D = Home - Tw.P;
					if (!MyBall || D.Size() < 120.f || Tw.Age > 3.5f) { bDone = true; if (Royale.P.IsValidIndex(Me)) Royale.P[Me].FireCD = FMath::Min(Royale.P[Me].FireCD, 0.12f); }
					else Tw.P += D.GetSafeNormal() * 2300.f * Dt;
				}
				const FVector Mv = (Tw.P - P0).GetSafeNormal2D();
				ForRivals([&](int32 j, const FVector& Pj) { if (!Tw.Hit.Contains(j) && FVector::Dist(Pj, Tw.P) < ZoogiConst::ZoogiRadius + 95.f) { Tw.Hit.Add(j); HitJ(j, Mv * Tn.BoltKnock * 1.8f + FVector(0, 0, 140.f), 45); AZoogiFX::Spawn(GetWorld(), Pj, WeaponCol, 150.f, 0.3f, false, 12.f); } });
				Tw.Spin += 1500.f * Dt;
				A->SetActorLocationAndRotation(Tw.P, FRotator(0.f, Tw.Spin, 90.f)); // flat, spinning
				if (FMath::Fmod(Tw.Age, 0.04f) < Dt) AZoogiFX::Spawn(GetWorld(), Tw.P, WeaponCol, 60.f, 0.2f, false, 10.f);
			}
			if (bDone) { if (A) A->Destroy(); Throws.RemoveAtSwap(k); }
		}
	}
	// ---- relics on the field: spin / bob, expire, picked up by the human
	for (int32 k = Relics.Num() - 1; k >= 0; --k)
	{
		FRelic& Re = Relics[k];
		AActor* A = Re.Actor.Get();
		if (!A || Now - Re.Born > 30.f) { if (A) A->Destroy(); Relics.RemoveAtSwap(k); continue; }
		A->SetActorLocationAndRotation(Re.Pos + FVector(0, 0, 70.f + 18.f * FMath::Sin((Now - Re.Born) * 3.f)), FRotator(0.f, (Now - Re.Born) * 120.f, 0.f));
		if (FMath::Fmod(Now - Re.Born, 1.2f) < Dt) AZoogiFX::Spawn(GetWorld(), Re.Pos + FVector(0, 0, 4.f), RelicCol[Re.Type], 120.f, 0.5f, true, 4.f);
		AZoogiBall* Hb = Balls.IsValidIndex(Royale.HumanIndex) ? Balls[Royale.HumanIndex].Get() : nullptr;
		if (!Hb || Hb->bOut || !Royale.P.IsValidIndex(Royale.HumanIndex) || Royale.P[Royale.HumanIndex].RespawnIn >= 0.f) continue;
		const FVector P = Hb->BallLocation();
		if (FVector::Dist2D(P, Re.Pos) > 150.f || FMath::Abs(P.Z - ZoogiConst::ZoogiRadius - Re.Pos.Z) > 160.f) continue;
		// pick up
		if (UStaticMeshComponent* Old = Held.InHand.Get()) Old->DestroyComponent();
		Held = FHeld(); Held.Type = Re.Type; Held.Charges = RelicCharges[Re.Type];
		FZoogiRoyalePlayer& RH = Royale.P[Royale.HumanIndex];
		if (Re.Type == Emblem) { RH.ShieldLeft = FMath::Max(RH.ShieldLeft, 8.f); Held.Charges = 0; }
		if (USkeletalMeshComponent* SK = H.Mesh.Get())
			if (UStaticMesh* M = LoadRelic(Re.Type))
				if (Re.Type != Emblem && FParse::Param(FCommandLine::Get(), TEXT("ZoogiRelicInHand")))
				{
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(SK->GetOwner(), TEXT("RelicInHand"));
					C->SetStaticMesh(M); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent();
					C->AttachToComponent(SK, FAttachmentTransformRules::SnapToTargetNotIncludingScale, H.HandBone);
					C->SetWorldScale3D(FVector(1.f)); FitMesh(C, M, Re.Type == Lantern ? 45.f : 110.f);
					Held.InHand = C;
				}
		AddPopup(FString::Printf(TEXT("%s!"), RelicName[Re.Type]), P + FVector(0, 0, 140.f), RelicCol[Re.Type]);
		AddFeed(FString::Printf(TEXT("%s picked up the %s"), *Hb->Def.Name, RelicName[Re.Type]), RelicCol[Re.Type]);
		AZoogiFX::Spawn(GetWorld(), Re.Pos, RelicCol[Re.Type], 260.f, 0.6f, true, 9.f);
		ZoogiUtil::PlaySfx(this, TEXT("Score"), 0.7f, 1.3f);
		if (H.Mesh.IsValid()) HeroAction(TEXT("cheer"), 0.6f, 1.6f);
		UE_LOG(LogZoogi, Log, TEXT("ROYALE relic: P%d picked up %s (%d charges)"), Royale.HumanIndex + 1, RelicName[Re.Type], Held.Charges);
		A->Destroy(); Relics.RemoveAtSwap(k);
	}
	// ---- -ZoogiRelicTest (capture runs): a bot drops each relic type next to the human at 3 / 12 / 21 / 30 s of the fight
	{
		static const bool bRelicTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRelicTest"));
		static int32 TestDrops = 0;
		if (bRelicTest && Royale.Step == EZoogiRoyaleStep::Fight && TestDrops < 4 && Royale.FightTime > 3.f + 9.f * TestDrops && Balls.IsValidIndex(Royale.HumanIndex) && Balls[Royale.HumanIndex] && !Balls[Royale.HumanIndex]->bOut)
		{
			const int32 Bot = Royale.HumanIndex == 0 ? 1 : 0;
			NextType = TestDrops;
			RoyaleRelicDrop(Bot, Balls[Royale.HumanIndex]->BallLocation());
			++TestDrops;
		}
	}
	// ---- bots leave relics around while roaming
	if (Royale.Step == EZoogiRoyaleStep::Fight && Royale.FightTime > NextDrop)
	{
		NextDrop = Royale.FightTime + 18.f;
		if (Relics.Num() < 3)
		{
			TArray<int32> Bots;
			for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num(); ++j)
				if (j != Royale.HumanIndex && Balls[j] && !Balls[j]->bOut && Royale.P[j].IsIn() && Royale.P[j].RespawnIn < 0.f) Bots.Add(j);
			if (Bots.Num()) { const int32 j = Bots[FMath::RandRange(0, Bots.Num() - 1)]; RoyaleRelicDrop(j, Balls[j]->BallLocation()); }
		}
	}
}

void AZoogiGameMode::RoyaleRelicDrop(int32 FromBot, const FVector& Near)
{
	using namespace ZoogiHero;
	if (FromBot == Royale.HumanIndex || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoRelics"))) return;
	// a safe floor spot near where the bot was (on the Ancient islands the floor height follows the layout)
	FVector2D P(Near);
	const bool bAnc = Arena && Arena->GetTheme() == EZoogiArena::Ancient;
	int32 Tries = 0;
	while ((RoyaleDanger(P) || (Arena && !Arena->IsSpotClear(P, 70.f))) && Tries < 24)
	{
		P = FVector2D(Near) * (1.f - 0.08f * (Tries + 1)) + FVector2D(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f)) * (Tries < 12 ? 1.f : 0.f);
		if (Tries >= 12) P = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRandRange(400.f, 1300.f);
		++Tries;
	}
	if (RoyaleDanger(P)) return;
	const float Z = bAnc ? FMath::Max(ZoogiAncient::FloorZ(P), -400.f) : RoyaleFloorZ;
	if (NextType == Blade) NextType = Staff;
	const int32 Type = NextType; NextType = NextType + 1 >= NumRelics ? Staff : NextType + 1;
	UStaticMesh* M = LoadRelic(Type);
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(P, Z + 70.f)), SP);
	if (!A) return;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A, TEXT("Relic"));
	if (M) { C->SetStaticMesh(M); FitMesh(C, M, 120.f); }
	else { C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cone"))); C->SetRelativeScale3D(FVector(0.6f)); C->SetMaterial(0, ZoogiUtil::GlowMID(A, RelicCol[Type], 2.f)); }
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent(); A->SetRootComponent(C);
	FRelic R; R.Type = Type; R.Actor = A; R.Pos = FVector(P, Z); R.Born = GetWorld()->GetTimeSeconds();
	Relics.Add(R);
	AZoogiFX::Spawn(GetWorld(), R.Pos, RelicCol[Type], 200.f, 0.5f, true, 8.f);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE relic: P%d left a %s at (%.0f,%.0f,%.0f)%s"), FromBot + 1, RelicName[Type], P.X, P.Y, Z, M ? TEXT("") : TEXT(" (mesh missing: glow cone)"));
}

static bool GGatlingShot = false;
bool ZoogiRoyaleGatlingShot() { return GGatlingShot; } // b45: the last RoyaleHeroFire was a Gatling round (ZoogiRoyale.cpp: faster, longer streak, a little spread)

bool AZoogiGameMode::RoyaleHeroFire(int32 Idx, FVector& Muzzle, const FVector& Dir, float& Power)
{
	using namespace ZoogiHero;
	Power = 1.f;
	GGatlingShot = false;
	if (Idx != Royale.HumanIndex) return false;
	USkeletalMeshComponent* SK = H.Mesh.Get();
	if (SK)
	{
		Muzzle = !H.HandBone.IsNone() ? SK->GetBoneLocation(H.HandBone) : SK->GetComponentLocation() + Dir * 60.f + FVector(0, 0, H.Height * 0.6f);
		if (const UStaticMeshComponent* Rf = H.Rifle.Get()) if (!Rf->bHiddenInGame) Muzzle = Rf->GetComponentLocation() + Rf->GetRightVector() * (H.GunSpin.IsValid() ? 58.f : 52.f); // b44/45: out of the barrels
	}
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	AZoogiBall* Me = Balls[Idx];
	auto Spend = [&]()
	{
		if (--Held.Charges > 0) { AddPopup(FString::Printf(TEXT("%s x%d"), RelicName[Held.Type], Held.Charges), Me->BallLocation() + FVector(0, 0, 150.f), RelicCol[Held.Type]); return; }
		AddPopup(TEXT("RELIC SPENT"), Me->BallLocation() + FVector(0, 0, 150.f), FLinearColor(0.8f, 0.8f, 0.8f));
		if (UStaticMeshComponent* C = Held.InHand.Get()) C->DestroyComponent();
		Held = FHeld();
	};
	auto Hit = [&](int32 j, const FVector& Push, int32 Dmg)
	{
		FZoogiRoyaleKick K; K.Victim = j; K.DeltaV = Push; Royale.Kicks.Add(K);
		Royale.P[j].LastHitBy = Idx; Royale.P[j].LastHitAt = Clock; Royale.P[j].StunLeft = FMath::Max(Royale.P[j].StunLeft, 0.5f);
		RoyaleDamage(j, Idx, Dmg, true, Balls[j]->BallLocation());
	};
	auto Rivals = [&](TFunctionRef<void(int32, const FVector&)> Fn)
	{
		for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num(); ++j)
			if (j != Idx && Balls[j] && !Balls[j]->bOut && Royale.P[j].IsIn() && Royale.P[j].RespawnIn < 0.f && Royale.P[j].ShieldLeft <= 0.f) Fn(j, Balls[j]->BallLocation());
	};
	const FVector L = Me->BallLocation();
	FVector Flat = Dir; Flat.Z = 0.f; Flat = Flat.GetSafeNormal();
	if (Held.Type == Blade) // melee: a lunging slash - the hero dashes along the aim and cuts everything in a wide arc ahead (reach 4.5 m)
	{
		int32 N = 0;
		{ FZoogiRoyaleKick Self; Self.Victim = Idx; Self.DeltaV = Flat * 1100.f; Royale.Kicks.Add(Self); }
		Rivals([&](int32 j, const FVector& P) { const FVector D = P - L; if (D.Size2D() < 450.f && FVector::DotProduct(D.GetSafeNormal2D(), Flat) > 0.3f) { Hit(j, Flat * T.BoltKnock * 3.f + FVector(0, 0, 180.f), 120); ++N; } });
		for (int32 a = -2; a <= 2; ++a) AZoogiFX::Spawn(GetWorld(), L + Flat.RotateAngleAxis(a * 22.f, FVector::UpVector) * 220.f + FVector(0, 0, 40.f), RelicCol[Blade], 70.f, 0.2f, false, 12.f);
		if (SK) HeroAction(TEXT("slash"), 0.5f, 1.5f);
		ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.6f, 1.1f);
		R.FireCD = 0.55f; Spend();
		UE_LOG(LogZoogi, Log, TEXT("ROYALE relic: blade slash hit %d"), N);
		return true;
	}
	if (Held.Type == Lantern) // area blast around the hero
	{
		int32 N = 0;
		Rivals([&](int32 j, const FVector& P) { const FVector D = P - L; if (D.Size2D() < 430.f) { Hit(j, D.GetSafeNormal2D() * T.BoltKnock * 2.6f + FVector(0, 0, 220.f), 90); ++N; } });
		AZoogiFX::Spawn(GetWorld(), L - FVector(0, 0, 40.f), RelicCol[Lantern], 430.f, 0.6f, true, 10.f);
		AZoogiFX::Spawn(GetWorld(), L + FVector(0, 0, 60.f), FLinearColor::White, 160.f, 0.25f, false, 14.f);
		if (SK) HeroAction(TEXT("cast_a_spell"), 0.6f, 1.6f);
		ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.5f, 1.2f);
		R.FireCD = 1.1f; Spend();
		UE_LOG(LogZoogi, Log, TEXT("ROYALE relic: lantern blast hit %d"), N);
		return true;
	}
	// b120: the equipped weapon (relics above still take over while held)
	if (Weapon != WGatling && Me && R.bRobot)
	{
		const FVector Hand = SK && !H.HandBone.IsNone() ? SK->GetBoneLocation(H.HandBone) : L + FVector(0, 0, 60.f);
		if (Weapon == WGrenade)
		{	// lob it at the crosshair (3.5 - 17 m), ~1 s in the air
			const float Dist = FMath::Clamp(FVector::Dist2D(R.AimPoint, L), 350.f, 1700.f);
			const float Vz = 760.f, Tf = (Vz + FMath::Sqrt(Vz * Vz + 2.f * 1800.f * 120.f)) / 1800.f;
			FThrow Tw; Tw.Kind = WGrenade; Tw.P = Hand + Flat * 30.f; Tw.Dir = Flat; Tw.V = Flat * (Dist / Tf) + FVector(0, 0, Vz);
			Tw.A = SpawnThrown(GetWorld(), WGrenade, Tw.P);
			Throws.Add(Tw);
			if (SK && H.ActionLeft <= 0.1f) HeroAction(TEXT("cast_a_spell"), 0.4f, 1.9f);
			ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.55f, 1.25f);
			R.FireCD = 0.85f;
			UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: grenade thrown (%.0f cm)"), Dist);
			return true;
		}
		if (Weapon == WSword)
		{	// dash + a wide cut in front (reach 4.6 m)
			int32 N = 0;
			{ FZoogiRoyaleKick Sk; Sk.Victim = Idx; Sk.DeltaV = Flat * 1000.f; Royale.Kicks.Add(Sk); }
			Rivals([&](int32 j, const FVector& Pj) { const FVector D = Pj - L; if (D.Size2D() < 460.f && FVector::DotProduct(D.GetSafeNormal2D(), Flat) > 0.25f) { Hit(j, Flat * T.BoltKnock * 3.f + FVector(0, 0, 200.f), 110); ++N; } });
			for (int32 a = -3; a <= 3; ++a) AZoogiFX::Spawn(GetWorld(), L + Flat.RotateAngleAxis(a * 18.f, FVector::UpVector) * 230.f + FVector(0, 0, 50.f), WeaponCol, 75.f, 0.22f, false, 12.f);
			if (SK) HeroAction(TEXT("slash"), 0.45f, 1.6f);
			WSwing = 1.f;
			ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.7f, 1.05f);
			R.FireCD = 0.5f;
			UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: sword slash hit %d"), N);
			return true;
		}
		if (Weapon == WDisc)
		{	// boomerang: out ~11 m along the aim, back to the hand; one disc at a time
			if (DiscOut()) { R.FireCD = 0.1f; return true; }
			FThrow Tw; Tw.Kind = WDisc; Tw.P = Hand + Flat * 40.f; Tw.Dir = Flat;
			Tw.A = SpawnThrown(GetWorld(), WDisc, Tw.P);
			Throws.Add(Tw);
			if (SK && H.ActionLeft <= 0.1f) HeroAction(TEXT("cast_a_spell"), 0.35f, 2.f);
			ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.6f, 1.4f);
			R.FireCD = 5.f; // the return resets it
			UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: disc thrown"));
			return true;
		}
		if (Weapon == WStaff)
		{	// heavy magic bolts out of the orb on top (HEAVY ROUNDS relic stacks on it)
			Power = 2.2f;
			if (Held.Type == Staff) { Power *= 2.5f; Spend(); }
			if (const UStaticMeshComponent* Wm = WeaponMesh.Get()) Muzzle = Wm->Bounds.Origin + FVector(0.f, 0.f, WeaponSize[WStaff] * 0.42f); // the orb on top
			if (SK && H.ActionLeft <= 0.1f) HeroAction(TEXT("cast_a_spell"), 0.25f, 2.f);
			AZoogiFX::Spawn(GetWorld(), Muzzle, WeaponCol, 60.f, 0.15f, false, 14.f);
			ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.35f, FMath::FRandRange(1.3f, 1.5f));
			R.FireCD = 0.32f;
			return false;
		}
	}
	if (Held.Type == Staff) { Power = 2.5f; Spend(); }
	if (H.Rifle.IsValid())
	{
		// b45 Gatling: ~13 rounds a second, each a thin fast streak; per-round damage / push scaled so the damage per second matches the old cannons
		GGatlingShot = true;
		R.FireCD = 0.075f; Power *= 0.3f; H.LastShot = GetWorld()->GetTimeSeconds(); H.RifleKick = 1.f;
		ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.18f, FMath::FRandRange(1.9f, 2.2f));
		return false;
	}
	// shooting (b44 rifle: the full-body fire pose only while standing still - running keeps its legs; the rifle kicks instead)
	const bool bStill = !H.bRifle || (Balls.IsValidIndex(Idx) && Balls[Idx] && Balls[Idx]->GetBallVelocity().Size2D() < 60.f);
	if (SK && H.ActionLeft <= 0.1f && (Held.Type == Staff || bStill)) HeroAction(Held.Type == Staff ? TEXT("cast_a_spell") : (R.bFireLeft ? TEXT("box_01") : TEXT("box_02")), H.bRifle ? 0.22f : 0.32f, H.bRifle ? 1.f : 1.8f);
	if (H.Rifle.IsValid()) H.RifleKick = 1.f;
	return false;
}
