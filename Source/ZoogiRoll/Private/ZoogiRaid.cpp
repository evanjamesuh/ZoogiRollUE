// b151: RAID MODE - see ZoogiRaid.h.
#include "ZoogiRaid.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiTypes.h"
#include "ZoogiHaptics.h"
#include "ZoogiCosmosBoss.h"
#include "ZoogiDragonBoss.h"
#include "ZoogiFrozenLayout.h"
#include "ZoogiActionFun.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Canvas.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CanvasItem.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ZoogiBossAttack.h"
#include "ZoogiGattling.h"
#include "ZoogiBounty.h"
#include "ZoogiParty.h" // b273
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Engine/Texture2D.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col); // b207
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);

namespace ZoogiRaid
{
	void ResetHud(); // b207
	namespace
	{
		constexpr float HandR = 150.f;   // the weak point's collision radius
		constexpr int32 TeamLives = 5;
		constexpr int32 Rounds = 10;
		struct FRaid
		{
			bool bArmed = false, bOn = false, bResult = false, bCleared = false, bEnding = false; int32 SavedRounds = -1;
			TWeakObjectPtr<AZoogiGameMode> G; TWeakObjectPtr<AZoogiBall> First;
			int32 HP = 0, Max = 1, Lives = TeamLives, Phase = 1, LastRound = -1, LastTurn = -1;
			float Ang = 0.f, Pulse = 0.f, EndIn = -1.f;
			TArray<int32> Dmg, Hits;
			TWeakObjectPtr<AStaticMeshActor> Hand, Crystal; // b179: Crystal = Dracoria's heart crystal (Frozen raid)
			TMap<TWeakObjectPtr<AActor>, double> HitCool;
			TSet<TWeakObjectPtr<AZoogiBall>> OutSeen;
			TSet<TWeakObjectPtr<AZoogiOrb>> OrbOutSeen; bool bVolleyInit = false; // b269 ORB VOLLEY
			TSet<int32> Rescued; int32 VolleyTurn = -1; // b271 (review): one free rescue per player, weak-point combo after a volley
			TArray<TWeakObjectPtr<AStaticMeshActor>> Armor; int32 ArmorDown = 0; int32 ExposeUntil = -1; // b200
		};
		FRaid S;
		const FLinearColor Gold(1.f, 0.78f, 0.2f), Red(1.f, 0.15f, 0.1f);

		FVector Ctr(const AZoogiGameMode* G) { return G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector; }
		float ER(const AZoogiGameMode* G) { return G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f; }
		// b179 (Evan: "yes"): Frozen RAID - Dracoria sits on her islet, so the weak point is a glowing HEART CRYSTAL on the rim in
		// front of her (not a floating titan hand); it stays there until she is ENRAGED (33%), then it jumps around the rim.
		bool Dragon() { return ZoogiDragon::Active() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidHand")); }
		float DragonAng() { return ZoogiFrozen::IsletYaw[FMath::Max(0, ZoogiDragon::SeatIslet())]; }
		FVector HandPos(const AZoogiGameMode* G)
		{
			const FVector C = Ctr(G);
			const float Z = G->Balls.Num() > 0 && G->Balls[0] ? (float)G->Balls[0]->SpawnLocation.Z : C.Z + ZoogiConst::ZoogiRadius;
			return FVector(C.X, C.Y, Z) + FRotator(0.f, S.Ang, 0.f).Vector() * (ER(G) - 90.f);
		}
		void PlaceHand(AZoogiGameMode* G)
		{
			AStaticMeshActor* H = S.Hand.Get();
			if (!H)
			{
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				H = G->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(HandPos(G)), SP);
				if (!H) return;
				H->SetMobility(EComponentMobility::Movable);
				UStaticMeshComponent* M = H->GetStaticMeshComponent();
				M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
				M->SetCollisionProfileName(TEXT("BlockAll"));
				M->SetVisibility(false); // the puppet's hand is the picture; this is its solid knuckle on the court edge
				H->SetActorScale3D(FVector(HandR / 50.f));
				S.Hand = H;
			}
			H->SetActorLocation(HandPos(G));
			if (Dragon())
			{	// the heart crystal: tall glowing ice spikes on the rim (no collision - the invisible knuckle sphere is the hit box)
				AStaticMeshActor* Cr = S.Crystal.Get();
				if (!Cr)
				{
					FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					Cr = G->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(HandPos(G)), SP);
					if (Cr)
					{
						Cr->SetMobility(EComponentMobility::Movable);
						UStaticMeshComponent* M = Cr->GetStaticMeshComponent();
						UStaticMesh* Sm = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/IceAttack/IceAttack/Models/SM_IceSpikes01_Spike01.SM_IceSpikes01_Spike01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
						M->SetStaticMesh(Sm ? Sm : ZoogiUtil::BasicShape(TEXT("Cone")));
						M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						const FBox Bb = M->GetStaticMesh() ? M->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50), FVector(50));
						const float Hh = FMath::Max(1.f, Bb.GetSize().Z);
						Cr->SetActorScale3D(FVector(260.f / Hh)); // about 2.6 m tall
						S.Crystal = Cr;
					}
				}
				if (Cr) Cr->SetActorLocation(HandPos(G));
			}
		}
		void Popup(AZoogiGameMode* G, const FString& T, const FVector& At, const FLinearColor& C) { G->PublicPopup(T, At + FVector(0, 0, 200.f), C); }
		void Move(AZoogiGameMode* G, const TCHAR* Why)
		{	// a new spot on the rim, at least 60 deg away
			if (Dragon() && S.Phase < 3) { S.Ang = DragonAng(); PlaceHand(G); return; } // Dracoria guards her islet until she is enraged
			const float Step = FMath::FRandRange(70.f, 150.f) * (FMath::RandBool() ? 1.f : -1.f);
			S.Ang = FRotator::NormalizeAxis(S.Ang + Step);
			ZoogiCosmosBoss::HoldAt(true, S.Ang);
			PlaceHand(G);
			UE_LOG(LogZoogi, Log, TEXT("RAID: the titan moves to yaw %.0f (%s)"), S.Ang, Why);
		}
		void Finish(AZoogiGameMode* G, bool bWin, float Delay)
		{
			if (S.bEnding) return;
			S.bEnding = true; S.bCleared = bWin; S.EndIn = Delay;
			if (bWin) ZoogiCosmosBoss::Defeat();
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(bWin ? TEXT("RAID CLEARED!") : TEXT("RAID FAILED"), bWin ? FString::Printf(TEXT("THE %s FALLS!"), *ZoogiCosmosBoss::Name()) : TEXT("THE TEAM IS OUT OF LIVES"), bWin ? Gold : Red, 3.f, 1.4f);
			ZoogiUtil::PlaySfx(G, bWin ? TEXT("Win") : TEXT("CritBoom"), 1.f, bWin ? 1.f : 0.5f);
			ZoogiHaptics::Kick(1.f, G->GetWorld(), TEXT("raid_end"));
			UE_LOG(LogZoogi, Log, TEXT("RAID %s (boss %d / %d, lives %d)"), bWin ? TEXT("CLEARED") : TEXT("FAILED"), S.HP, S.Max, S.Lives);
			{	// b271 (review): the damage race - an MVP card for the top damage dealer
				int32 Best = -1;
				for (int32 i = 0; i < S.Dmg.Num(); ++i) if (Best < 0 || S.Dmg[i] > S.Dmg[Best]) Best = i;
				if (Best >= 0 && S.Dmg[Best] > 0 && G->Balls.IsValidIndex(Best) && G->Balls[Best])
				{
					const FString Mvp = FString::Printf(TEXT("MVP: P%d %s - %d DAMAGE"), Best + 1, *G->Balls[Best]->Def.Name.ToUpper(), S.Dmg[Best]);
					TWeakObjectPtr<AZoogiGameMode> WG = G;
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, Mvp](float) -> bool { if (AZoogiGameMode* Gm = WG.Get()) if (UZoogiFunSys* F2 = UZoogiFunSys::Get(Gm)) F2->PublicCallout(Mvp, TEXT("TOP DAMAGE THIS RAID"), FLinearColor(1.f, 0.85f, 0.3f), 2.6f, 1.2f); return false; }), FMath::Max(0.5f, Delay * 0.6f));
					UE_LOG(LogZoogi, Log, TEXT("RAID %s"), *Mvp);
				}
			}
		}
		void Damage(AZoogiGameMode* G, int32 Player, int32 Amt, const FVector& At, const TCHAR* What)
		{
			if (Amt <= 0 || S.bEnding) return;
			if (S.VolleyTurn == G->TurnCounter && (FCString::Strcmp(What, TEXT("Zoogi slam")) == 0 || FCString::Strcmp(What, TEXT("orb")) == 0 || FCString::Strcmp(What, TEXT("star orb")) == 0))
			{	// b271 (review): a weak-point hit in the same shot as an orb volley = COMBO x2
				Amt *= 2; G->PublicPopup(TEXT("COMBO x2"), At + FVector(0, 0, 300.f), FLinearColor(1.f, 0.85f, 0.3f));
			}
			if (S.ExposeUntil >= G->TurnCounter) { Amt *= 2; G->PublicPopup(TEXT("EXPOSED x2"), At + FVector(0, 0, 260.f), FLinearColor(0.55f, 0.9f, 1.f)); } // b200 ARMOR BREAK
			ZoogiBounty::OnRaidHit(G, Player, What, At); // b200 raid challenges
			S.HP = FMath::Max(0, S.HP - Amt);
			if (S.Dmg.IsValidIndex(Player)) { S.Dmg[Player] += Amt; S.Hits[Player]++; }
			UWorld* W = G->GetWorld();
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Gold"), At, FRotator::ZeroRotator, 1.2f);
			AZoogiFX::Spawn(W, At, Gold, 260.f, 0.45f, true, 12.f);
			Popup(G, FString::Printf(TEXT("-%d"), Amt), At, Gold);
			ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.7f, 1.2f);
			ZoogiCosmosBoss::Flinch();
			if (G->Balls.IsValidIndex(Player) && G->Balls[Player] && !G->Balls[Player]->bCPU) ZoogiHaptics::Kick(0.7f, W, TEXT("raid_hit"));
			UE_LOG(LogZoogi, Log, TEXT("RAID hit: P%d %s -%d -> boss %d / %d"), Player + 1, What, Amt, S.HP, S.Max);
			const float Frac = (float)S.HP / FMath::Max(1, S.Max);
			if (S.Phase == 1 && Frac <= 0.66f) { S.Phase = 2; if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("PHASE 2!"), Dragon() ? TEXT("DRACORIA TAKES TO THE AIR") : TEXT("THE TITAN MOVES AFTER EVERY TURN"), Red, 2.f, 1.2f); UE_LOG(LogZoogi, Log, TEXT("RAID phase 2")); }
			if (S.Phase == 2 && Frac <= 0.33f) { S.Phase = 3; if (Dragon()) ZoogiDragon::Event(TEXT("Roar")); if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("ENRAGED!"), TEXT("BIGGER ATTACKS - FINISH IT!"), Red, 2.2f, 1.3f); UE_LOG(LogZoogi, Log, TEXT("RAID phase 3 (enraged)")); }
			if (S.HP <= 0) Finish(G, true, 3.f);
		}
		void Begin(AZoogiGameMode* G)
		{
			const int32 N = G->Balls.Num();
			S.bOn = true; S.bResult = false; S.bCleared = false; S.bEnding = false; S.EndIn = -1.f;
			S.G = G; S.First = N > 0 ? G->Balls[0].Get() : nullptr;
			int32 Hp = 80 * FMath::Max(1, N); FParse::Value(FCommandLine::Get(), TEXT("ZoogiRaidHP="), Hp);
			S.Max = S.HP = FMath::Max(10, Hp); S.Lives = ZoogiParty::CarryLives() > 0 ? ZoogiParty::CarryLives() : TeamLives; // b273 BOSS RUSH carries lives S.Phase = 1; S.LastRound = G->RoundNumber; S.LastTurn = -1;
			S.Dmg.Init(0, N); S.Hits.Init(0, N); S.HitCool.Reset(); S.OutSeen.Reset(); S.OrbOutSeen.Reset(); S.bVolleyInit = false; S.Rescued.Reset(); S.VolleyTurn = -1; // b269 / b271
			ResetHud(); // b207
			for (AZoogiBall* B : G->Balls) if (B) B->TeamIndex = 0; // one team (IsAlly)
			S.Ang = Dragon() ? DragonAng() : ZoogiCosmosBoss::Angle();
			ZoogiCosmosBoss::HoldAt(true, S.Ang);
			PlaceHand(G);
			if (NewHud()) HowTo(0); // b207 D: the WEAK POINT card at the first turn (the loading screen told the rest)
			else if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(FString::Printf(TEXT("RAID: %s"), *ZoogiCosmosBoss::Name()), Dragon() ? TEXT("HIT HER HEART CRYSTAL - ROLL INTO IT OR KNOCK ORBS INTO IT") : TEXT("HIT ITS GLOWING HAND - ROLL INTO IT OR KNOCK ORBS INTO IT"), Gold, 3.f, 1.3f);
			UE_LOG(LogZoogi, Log, TEXT("RAID begins: %s, health %d, %d Zoogis, %d team lives, %d rounds, hand at yaw %.0f"), *ZoogiCosmosBoss::Name(), S.Max, N, S.Lives, Rounds, S.Ang);
		}
		void DropArmor() { for (TWeakObjectPtr<AStaticMeshActor>& A : S.Armor) if (AStaticMeshActor* Ac = A.Get()) Ac->Destroy(); S.Armor.Reset(); }
		void End(AZoogiGameMode* G)
		{
			DropArmor(); S.ExposeUntil = -1;
			if (AStaticMeshActor* H = S.Hand.Get()) H->Destroy();
			S.Hand.Reset();
			if (AStaticMeshActor* Cr = S.Crystal.Get()) Cr->Destroy();
			S.Crystal.Reset();
			ZoogiCosmosBoss::HoldAt(false, 0.f);
			S.bOn = false;
		}
	}

	void Arm(AZoogiGameMode* G) { S.bArmed = true; S.bResult = false; if (G) { if (S.SavedRounds < 0) S.SavedRounds = G->MenuRounds; G->MenuRounds = Rounds; } }
	bool Armed() { return S.bArmed; }
	bool Active() { return S.bOn; }
	float Rage() { return S.bOn && S.Phase >= 3 ? 1.3f : 1.f; }
	AActor* HandActor() { return S.bOn ? S.Hand.Get() : nullptr; }
	bool HasResult() { return S.bResult; }
	bool Cleared() { return S.bCleared; }
	int32 DamageOf(int32 P) { return S.Dmg.IsValidIndex(P) ? S.Dmg[P] : 0; }
	int32 HitsOf(int32 P) { return S.Hits.IsValidIndex(P) ? S.Hits[P] : 0; }
	int32 LivesLeft() { return S.Lives; }
	int32 HPLeft() { return S.HP; }
	int32 MaxHP() { return S.Max; }

	void BonusDamage(AZoogiGameMode* G, int32 Player, int32 Amt, const TCHAR* What) { if (S.bOn && G) Damage(G, Player, Amt, HandPos(G), What); }
	void ClearArmor() { DropArmor(); }
	int32 ArmorBroken() { return S.ArmorDown; }
	void Expose(AZoogiGameMode* G, int32 Turns) { if (S.bOn && G) { S.ExposeUntil = G->TurnCounter + Turns; UE_LOG(LogZoogi, Log, TEXT("RAID: weak point EXPOSED (x2) until turn %d"), S.ExposeUntil); } }
	void SpawnArmor(AZoogiGameMode* G)
	{	// b200 ARMOR BREAK: two ice shards on the court in front of the weak point - smash both (your Zoogi or an orb)
		DropArmor(); S.ArmorDown = 0;
		if (!S.bOn || !G || !G->GetWorld()) return;
		const FVector Hp = HandPos(G), C = Ctr(G);
		const FVector In = FVector(C.X - Hp.X, C.Y - Hp.Y, 0.f).GetSafeNormal(), Side(-In.Y, In.X, 0.f);
		UStaticMesh* Sm = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/IceAttack/IceAttack/Models/SM_IceSpikes01_Spike01.SM_IceSpikes01_Spike01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (int32 k = -1; k <= 1; k += 2)
		{
			const FVector P = Hp + In * 330.f + Side * (k * 260.f) - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f);
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* A = G->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(P), SP);
			if (!A) continue;
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			M->SetStaticMesh(Sm ? Sm : ZoogiUtil::BasicShape(TEXT("Cone")));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			const FBox Bb = M->GetStaticMesh() ? M->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50), FVector(50));
			A->SetActorScale3D(FVector(150.f / FMath::Max(1.f, (float)Bb.GetSize().Z))); // ~1.5 m
			M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.45f, 0.85f, 1.f), 3.f));
			S.Armor.Add(A);
		}
		UE_LOG(LogZoogi, Log, TEXT("RAID: ARMOR BREAK - %d ice shards guard the weak point"), S.Armor.Num());
	}

	void OnAwaken(AZoogiGameMode* G, AZoogiBall* B)
	{
		if (!S.bOn || !G || !B) return;
		Damage(G, B->PlayerIndex, FMath::Max(1, FMath::RoundToInt(S.Max * 0.2f)), HandPos(G), TEXT("SHOWTIME"));
	}

	void PreFinish(AZoogiGameMode* G)
	{	// FinishGame picks the winner from Score: the damage, not this turn's ledger (falls)
		if (!S.bOn || !G) return;
		for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i]) G->Balls[i]->Score = DamageOf(i);
	}

	void OnFinished(AZoogiGameMode* G)
	{
		if (!S.bOn || !G) return;
		if (!S.bEnding) { S.bCleared = S.HP <= 0; UE_LOG(LogZoogi, Log, TEXT("RAID over after %d rounds: %s"), Rounds, S.bCleared ? TEXT("cleared") : TEXT("FAILED - the titan outlasted the team")); }
		S.bResult = true;
		int32 Mvp = -1; for (int32 i = 0; i < S.Dmg.Num(); ++i) if (Mvp < 0 || S.Dmg[i] > S.Dmg[Mvp]) Mvp = i;
		G->Banner = S.bCleared ? (G->Balls.IsValidIndex(Mvp) ? FString::Printf(TEXT("RAID CLEARED!  MVP %s"), *G->Balls[Mvp]->Def.Name.ToUpper()) : FString(TEXT("RAID CLEARED!"))) : FString(TEXT("RAID FAILED"));
		End(G);
	}

	void OnMenu(AZoogiGameMode* Gm) { if (S.bOn) { if (AZoogiGameMode* G = S.G.Get()) End(G); } if (Gm && S.SavedRounds > 0) Gm->MenuRounds = S.SavedRounds; S = FRaid(); }

	void Tick(AZoogiGameMode* G, float RealDt)
	{
		if (!G) return;
		static bool bCmd = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaid"));
		if (bCmd) { bCmd = false; S.bArmed = true; G->MenuRounds = Rounds; } // (the match already started with -ZoogiRounds) // test: the command-line match is a raid
		AZoogiBall* First = G->Balls.Num() > 0 ? G->Balls[0].Get() : nullptr;
		if (S.bOn && (S.G.Get() != G || S.First.Get() != First)) End(G); // a different match
		if (!S.bOn)
		{
			if (S.bArmed && First && G->Phase != EZoogiPhase::Menu && G->Phase != EZoogiPhase::GameOver && !G->bRoyaleMode && !G->bComboMode && ZoogiCosmosBoss::Present(G))
			{ S.bArmed = false; Begin(G); }
			return;
		}
		UWorld* W = G->GetWorld();
		for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i]) G->Balls[i]->Score = DamageOf(i); // scoreboard = damage
		if (S.EndIn >= 0.f) { S.EndIn -= RealDt; if (S.EndIn < 0.f && G->Phase != EZoogiPhase::GameOver) { S.bOn = false; if (!ZoogiParty::BossRushNext(G, S.bCleared, S.Lives)) G->PublicFinishGame(); } return; } // b273 BOSS RUSH: the next titan
		PlaceHand(G);
		const FVector Hp = HandPos(G);
		// the weak point glows
		S.Pulse -= RealDt;
		if (S.Pulse <= 0.f) { S.Pulse = 0.7f; AZoogiFX::Spawn(W, Hp + FVector(0, 0, -40.f), Gold, HandR + 60.f, 0.65f, true, 10.f); AZoogiFX::Spawn(W, Hp, Gold, 120.f, 0.4f, false, 8.f); }
		// hits: a Zoogi or an orb driving into the hand
		const double Now = W->GetTimeSeconds();
		for (AZoogiBall* B : G->Balls)
		{
			if (!B || B->bOut || !B->Sphere) continue;
			const FVector P = B->BallLocation();
			if (FVector::Dist(P, Hp) > HandR + ZoogiConst::ZoogiRadius + 25.f) continue;
			const float Sp = B->GetBallVelocity().Size();
			double& Cd = S.HitCool.FindOrAdd(B);
			if (Sp < 220.f || Now - Cd < 0.6) continue;
			Cd = Now;
			Damage(G, B->PlayerIndex, FMath::Min(20, FMath::RoundToInt(6.f + Sp / 120.f)), (P + Hp) * 0.5f, TEXT("Zoogi slam"));
		}
		for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut || !O->Sphere) continue;
			const FVector P = O->OrbLocation();
			if (FVector::Dist(P, Hp) > HandR + ZoogiConst::OrbRadius + 25.f) continue;
			const float Sp = O->Sphere->GetPhysicsLinearVelocity().Size();
			double& Cd = S.HitCool.FindOrAdd(O);
			if (Sp < 180.f || Now - Cd < 0.6) continue;
			Cd = Now;
			const int32 By = G->Balls.IsValidIndex(O->LastHitBy) ? O->LastHitBy : G->ActivePlayer;
			Damage(G, By, FMath::Min(O->bStar ? 25 : 12, FMath::RoundToInt((4.f + Sp / 250.f) * (O->bStar ? 2.5f : 1.f))), (P + Hp) * 0.5f, O->bStar ? TEXT("star orb") : TEXT("orb"));
		}
		for (int32 k = S.Armor.Num() - 1; k >= 0; --k)
		{	// b200 ARMOR BREAK: a Zoogi or an orb rolling into a shard smashes it
			AStaticMeshActor* A = S.Armor[k].Get(); if (!A) { S.Armor.RemoveAt(k); continue; }
			const FVector Ap = A->GetActorLocation();
			bool bHit = false;
			for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), Ap) < 110.f && B->GetBallVelocity().Size() > 150.f) { bHit = true; break; }
			if (!bHit) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->Sphere && FVector::Dist2D(O->OrbLocation(), Ap) < 95.f && O->Sphere->GetPhysicsLinearVelocity().Size() > 150.f) { bHit = true; break; }
			if (!bHit) continue;
			AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), Ap + FVector(0, 0, 60.f), FRotator::ZeroRotator, 1.3f);
			AZoogiFX::Spawn(W, Ap + FVector(0, 0, 40.f), FLinearColor(0.55f, 0.9f, 1.f), 220.f, 0.4f, true, 12.f);
			ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.9f, 1.3f);
			++S.ArmorDown; A->Destroy(); S.Armor.RemoveAt(k);
			Popup(G, FString::Printf(TEXT("SHARD %d / 2"), S.ArmorDown), Ap, FLinearColor(0.55f, 0.9f, 1.f));
			UE_LOG(LogZoogi, Log, TEXT("RAID: armor shard smashed (%d)"), S.ArmorDown);
		}
		// scoreboard = damage; team lives
		for (int32 i = 0; i < G->Balls.Num(); ++i)
		{
			AZoogiBall* B = G->Balls[i];
			if (!B) continue;
			B->Score = DamageOf(i);
			if (B->bOut && !S.OutSeen.Contains(B))
			{
				S.OutSeen.Add(B);
				if (!S.Rescued.Contains(i) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidNoRescue")))
				{	// b271 (review: falls cost the most - 3 of 5 lives in the playtest): each Zoogi's first fall is a free rescue
					S.Rescued.Add(i);
					Popup(G, TEXT("FREE RESCUE - NO LIFE LOST"), B->SpawnLocation, Gold);
					Ticker(TEXT("FREE RESCUE - THE NEXT FALL COSTS A TEAM LIFE"), Gold);
					UE_LOG(LogZoogi, Log, TEXT("RAID: P%d fell - free rescue (lives stay %d)"), i + 1, S.Lives);
					continue;
				}
				--S.Lives;
				Popup(G, FString::Printf(TEXT("TEAM LIFE LOST - %d LEFT"), FMath::Max(0, S.Lives)), B->SpawnLocation, Red);
				Ticker(FString::Printf(TEXT("TEAM LIFE LOST - %d LEFT"), FMath::Max(0, S.Lives)), Red); // b207
				UE_LOG(LogZoogi, Log, TEXT("RAID: P%d fell - %d team lives left"), i + 1, S.Lives);
				if (S.Lives <= 0) Finish(G, false, 2.f);
			}
			else if (!B->bOut) S.OutSeen.Remove(B);
		}
		// b269 (Evan: "play test raid to learn how to increase combat" - the playtest: 2 hits in 13 turns, orb knock-outs did nothing):
		// ORB VOLLEY - every orb knocked out of the arena flies into the titan for 3 damage
		{
			static const bool bNoVolley = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRaidNoVolley"));
			int32 NewOut = 0, By = -1; FVector From = FVector::ZeroVector;
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O) continue;
				if (O->bOut) { if (!S.OrbOutSeen.Contains(O)) { S.OrbOutSeen.Add(O); ++NewOut; From = O->OrbLocation(); if (G->Balls.IsValidIndex(O->LastHitBy)) By = O->LastHitBy; } }
				else S.OrbOutSeen.Remove(O);
			}
			if (!S.bVolleyInit) { S.bVolleyInit = true; NewOut = 0; } // orbs already out when the raid began
			if (NewOut > 0 && !bNoVolley && !S.bEnding)
			{
				if (By < 0) By = G->ActivePlayer;
				const FVector Hv = HandPos(G);
				for (int32 k = 1; k <= 5; ++k) AZoogiFX::Spawn(W, FMath::Lerp(From, Hv, k / 6.f) + FVector(0, 0, 260.f * FMath::Sin(PI * k / 6.f)), Gold, 70.f, 0.4f, false, 10.f);
				Damage(G, By, FMath::Min(15, 3 * NewOut), Hv, NewOut > 1 ? TEXT("orb volley") : TEXT("orb out"));
				S.VolleyTurn = G->TurnCounter; // b271: the weak point takes double for the rest of this shot // b270: max 15 a volley (a 15-orb shot did 45 - the titan fell in ~4 rounds)
			}
		}
		// the titan's turn: it moves after every round (phase 2+: after every turn)
		if (G->Phase == EZoogiPhase::Aiming && G->TurnCounter != S.LastTurn)
		{
			S.LastTurn = G->TurnCounter;
			if (G->RoundNumber != S.LastRound) { S.LastRound = G->RoundNumber; Move(G, TEXT("new round")); }
			else if (S.Phase >= 2) Move(G, TEXT("phase 2 - every turn"));
		}
	}

	namespace
	{	// b200 (Evan: "rim glow, radar, boss window - all three"): where the raid boss is
		TWeakObjectPtr<ASceneCapture2D> GCap; TWeakObjectPtr<UTextureRenderTarget2D> GCapRT; int32 GCapN = 0;
		void DropCap() { if (ASceneCapture2D* A = GCap.Get()) A->Destroy(); GCap.Reset(); if (UTextureRenderTarget2D* R = GCapRT.Get()) R->RemoveFromRoot(); GCapRT.Reset(); }
		bool LocOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoRaidLocator")); return b; }
		void Ring(UCanvas* C, const FVector2D& Cc, float R, const FLinearColor& Col, float Th, int32 N = 40)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N;
				FCanvasLineItem Li(Cc + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R, Cc + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * R);
				Li.SetColor(Col); Li.LineThickness = Th; Li.BlendMode = SE_BLEND_Translucent; C->DrawItem(Li);
			}
		}
		void Dot(UCanvas* C, const FVector2D& Cc, float R, const FLinearColor& Col)
		{
			FCanvasNGonItem Ng(Cc, FVector2D(R, R), 16, Col); Ng.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ng);
		}
		void DrawLocators(AZoogiGameMode* G, UCanvas* C, const FVector& Bp)
		{
			APlayerController* Pc = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr;
			if (!Pc || !Pc->PlayerCameraManager) return;
			const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
			const bool bPort = Ht > Wd;
			const FVector CamL = Pc->PlayerCameraManager->GetCameraLocation(); const FRotator CamR = Pc->PlayerCameraManager->GetCameraRotation();
			const FVector Fw = CamR.Vector(), Rt = FRotationMatrix(CamR).GetUnitAxis(EAxis::Y), Up = FRotationMatrix(CamR).GetUnitAxis(EAxis::Z);
			const double Tn = FPlatformTime::Seconds();
			const AZoogiBall* Me = nullptr; for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) { Me = B; break; }
			const float DistM = Me ? FVector::Dist2D(Me->BallLocation(), Bp) / 100.f : 0.f;
			// ---- F: RIM GLOW - the screen edge on the boss side glows red when the boss is off-screen
			{
				const FVector D = Bp - CamL; const float Fz = FVector::DotProduct(D, Fw), Rx = FVector::DotProduct(D, Rt), Uy = FVector::DotProduct(D, Up);
				const FVector Sp = C->Project(Bp);
				const bool bOnScreen = Fz > 0.f && Sp.X > Wd * 0.06f && Sp.X < Wd * 0.94f && Sp.Y > Ht * 0.06f && Sp.Y < Ht * 0.94f;
				if (!bOnScreen)
				{
					FVector2D Dir(Rx, -Uy); if (Fz < 0.f && Dir.Size() < 1.f) Dir = FVector2D(0.f, 1.f);
					Dir.Normalize();
					const float Pu = 0.75f + 0.25f * FMath::Sin((float)Tn * 6.f);
					const int32 Ns = 18; const float Sw = 7.f * U;
					for (int32 i = 0; i < Ns; ++i)
					{
						const float Al = 0.42f * Pu * FMath::Pow(1.f - (float)i / Ns, 1.6f);
						if (Dir.X > 0.15f) ZoogiFunCanvasRect(C, Wd - (i + 1) * Sw, 0.f, Sw, Ht, FLinearColor(1.f, 0.08f, 0.1f, Al * FMath::Min(1.f, Dir.X * 1.3f)));
						if (Dir.X < -0.15f) ZoogiFunCanvasRect(C, i * Sw, 0.f, Sw, Ht, FLinearColor(1.f, 0.08f, 0.1f, Al * FMath::Min(1.f, -Dir.X * 1.3f)));
						if (Dir.Y > 0.15f) ZoogiFunCanvasRect(C, 0.f, Ht - (i + 1) * Sw, Wd, Sw, FLinearColor(1.f, 0.08f, 0.1f, Al * FMath::Min(1.f, Dir.Y * 1.3f)));
						if (Dir.Y < -0.15f) ZoogiFunCanvasRect(C, 0.f, i * Sw, Wd, Sw, FLinearColor(1.f, 0.08f, 0.1f, 0.7f * Al * FMath::Min(1.f, -Dir.Y * 1.3f)));
					}
				}
			}
			// ---- F: AIM HINT - while you aim, a faint red dashed line from your Zoogi toward the boss's weak point
			if (G->IsHumanAiming())
				if (const AZoogiBall* A = G->GetActiveBall())
				{
					const FVector P0 = A->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius * 0.7f);
					FVector D = Bp - P0; D.Z = 0.f; const float L = FMath::Min(D.Size(), 1500.f); D = D.GetSafeNormal();
					const float Off = FMath::Fmod((float)Tn * 160.f, 120.f);
					for (float T = ZoogiConst::ZoogiRadius + Off; T + 60.f < L; T += 120.f)
					{
						const FVector S0 = C->Project(P0 + D * T), S1 = C->Project(P0 + D * (T + 60.f));
						if (S0.Z <= 0.f || S1.Z <= 0.f) continue;
						FCanvasLineItem Li(FVector2D(S0.X, S0.Y), FVector2D(S1.X, S1.Y)); Li.SetColor(FLinearColor(1.f, 0.15f, 0.2f, 0.55f * (1.f - T / 1600.f)));
						Li.LineThickness = 6.f * U; Li.BlendMode = SE_BLEND_Translucent; C->DrawItem(Li);
					}
				}
			// ---- B: RADAR - camera-up minimap: the court, every Zoogi, the boss's weak point in red
			{
				const float R = 96.f * U; const FVector2D Cc(28.f * U + R, bPort ? Ht * 0.70f : Ht * 0.62f);
				const FVector Ct = Ctr(G); const float Er = ER(G);
				const FVector Fh = FVector(Fw.X, Fw.Y, 0.f).GetSafeNormal(), Rh = FVector(-Fh.Y, Fh.X, 0.f);
				auto Map = [&](const FVector& P) { const FVector D = P - Ct; FVector2D Q(FVector::DotProduct(D, Rh), -FVector::DotProduct(D, Fh)); Q *= R * 0.86f / FMath::Max(Er, 1.f); if (Q.Size() > R * 0.95f) Q = Q.GetSafeNormal() * R * 0.95f; return Cc + Q; };
				Dot(C, Cc, R + 5.f * U, FLinearColor(0.02f, 0.02f, 0.05f, 0.78f));
				Dot(C, Cc, R, FLinearColor(0.1f, 0.22f, 0.34f, 0.7f));
				Ring(C, Cc, R, FLinearColor(0.5f, 0.85f, 1.f, 0.95f), 3.f * U);
				Ring(C, Cc, R * 0.86f, FLinearColor(0.5f, 0.85f, 1.f, 0.45f), 2.f * U);
				const float Sw = FMath::Fmod((float)Tn * 2.4f, 2.f * PI);
				for (int32 k = 0; k < 10; ++k)
				{	// radar sweep
					const float A0 = Sw - k * 0.04f;
					FCanvasLineItem Li(Cc, Cc + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * R); Li.SetColor(FLinearColor(0.5f, 0.9f, 1.f, 0.3f * (1.f - k / 10.f)));
					Li.LineThickness = 3.f * U; Li.BlendMode = SE_BLEND_Translucent; C->DrawItem(Li);
				}
				if (Me)
				{	// your view: a soft wedge from your Zoogi pointing up (where the camera looks)
					const FVector2D Mp = Map(Me->BallLocation());
					for (int32 s = -1; s <= 1; s += 2) { FCanvasLineItem Li(Mp, Mp + FVector2D(s * 0.55f, -1.f) * R * 0.5f); Li.SetColor(FLinearColor(1.f, 1.f, 1.f, 0.4f)); Li.LineThickness = 2.f * U; Li.BlendMode = SE_BLEND_Translucent; C->DrawItem(Li); }
				}
				for (const AZoogiBall* B : G->Balls)
				{
					if (!B || B->bOut) continue;
					const bool bMe = B == Me;
					const FVector2D Q = Map(B->BallLocation());
					Dot(C, Q, (bMe ? 9.f : 7.f) * U, FLinearColor::White);
					Dot(C, Q, (bMe ? 7.f : 5.f) * U, bMe ? FLinearColor(0.25f, 0.65f, 1.f, 1.f) : FLinearColor(0.3f, 0.95f, 0.45f, 1.f));
				}
				const FVector2D Bq = Map(Bp);
				const float Pz = 0.5f + 0.5f * FMath::Sin((float)Tn * 8.f);
				Ring(C, Bq, (10.f + 8.f * Pz) * U, FLinearColor(1.f, 0.15f, 0.1f, 1.f - 0.6f * Pz), 3.f * U, 20);
				Dot(C, Bq, 9.f * U, FLinearColor(1.f, 0.12f, 0.1f, 1.f));
				ZoogiFunCanvasText(C, TEXT("BOSS"), Bq.X, Bq.Y - 22.f * U, 18.f * U, FLinearColor(1.f, 0.4f, 0.35f, 1.f), 1, 0);
				ZoogiFunCanvasText(C, FString::Printf(TEXT("%s  %.0f m"), *ZoogiCosmosBoss::Name(), DistM), Cc.X, Cc.Y + R + 22.f * U, 20.f * U, FLinearColor(1.f, 0.5f, 0.45f, 1.f), 1, 0);
			}
			// ---- E: BOSS WINDOW - a small live camera on the boss in the corner
			{
				UWorld* W = G->GetWorld();
				FVector Head = Bp; ZoogiBossAttack::LockOnTarget(G, Head);
				FVector CamPos, CamLook;
				if (!(Dragon() && ZoogiDragon::CamSpot(CamPos, CamLook)))
				{
					const FVector Out = (FVector(Head.X, Head.Y, 0.f) - FVector(Ctr(G).X, Ctr(G).Y, 0.f)).GetSafeNormal();
					CamLook = Head; CamPos = Head - Out * 1600.f + FVector(0, 0, 350.f);
					FVector Tc; float Th; if (ZoogiCosmosBoss::TitanFocus(Tc, Th)) { CamLook = Tc; CamPos = Tc - Out * FMath::Max(1600.f, Th * 3.4f) + FVector(0, 0, 150.f); } // b216: the whole titan in the window
				}
				if (!GCap.IsValid() && W)
				{
					FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					if (ASceneCapture2D* A = W->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform(CamPos), Sp))
					{
						UTextureRenderTarget2D* Rt2 = UKismetRenderingLibrary::CreateRenderTarget2D(W, 384, 216, RTF_RGBA8);
						if (Rt2) Rt2->AddToRoot();
						USceneCaptureComponent2D* Sc = A->GetCaptureComponent2D();
						Sc->TextureTarget = Rt2; Sc->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
						Sc->bCaptureEveryFrame = false; Sc->bCaptureOnMovement = false; Sc->FOVAngle = 42.f;
						Sc->ShowFlags.SetMotionBlur(false); Sc->ShowFlags.SetBloom(false);
						GCap = A; GCapRT = Rt2;
						UE_LOG(LogZoogi, Log, TEXT("RAID boss window: live capture on (%s)"), Dragon() ? TEXT("Dracoria cam spot") : TEXT("titan"));
					}
				}
				ASceneCapture2D* A = GCap.Get(); UTextureRenderTarget2D* Rt2 = GCapRT.Get();
				if (A && Rt2)
				{
					A->SetActorLocationAndRotation(CamPos, (CamLook - CamPos).Rotation());
					if ((GCapN++ % 3) == 0) A->GetCaptureComponent2D()->CaptureScene(); // every 3rd frame: cheap enough on phones
					const float PW = (bPort ? 300.f : 330.f) * U, PH = PW * 9.f / 16.f;
					const FVector2D P0(Wd - PW - 26.f * U, bPort ? Ht * 0.20f : Ht * 0.31f);
					ZoogiFunCanvasRect(C, P0.X - 6.f * U, P0.Y - 6.f * U, PW + 12.f * U, PH + 12.f * U, FLinearColor(0.9f, 0.08f, 0.1f, 1.f));
					if (Rt2->GetResource()) { FCanvasTileItem It(P0, Rt2->GetResource(), FVector2D(PW, PH), FLinearColor::White); It.BlendMode = SE_BLEND_Opaque; C->DrawItem(It); }
					if (FMath::Fmod((float)Tn, 1.f) < 0.6f) Dot(C, P0 + FVector2D(16.f, 16.f) * U, 7.f * U, FLinearColor(1.f, 0.1f, 0.1f, 1.f));
					ZoogiFunCanvasText(C, TEXT("LIVE"), P0.X + 28.f * U, P0.Y + 16.f * U, 18.f * U, FLinearColor::White, 0, 0);
					// where it is from your view
					const FVector D = Bp - CamL; const float Fz = FVector::DotProduct(D, Fw), Rx = FVector::DotProduct(D, Rt);
					const TCHAR* Side = Fz > 0.f && FMath::Abs(Rx) < Fz * 0.8f ? TEXT("AHEAD") : (Fz < -FMath::Abs(Rx) ? TEXT("BEHIND YOU") : (Rx > 0.f ? TEXT("RIGHT  >") : TEXT("<  LEFT")));
					ZoogiFunCanvasText(C, FString::Printf(TEXT("%s  %.0f m  %s"), *ZoogiCosmosBoss::Name(), DistM, Side), P0.X + PW * 0.5f, P0.Y + PH + 22.f * U, 20.f * U, FLinearColor::White, 1, 0);
				}
			}
		}
	}
	// ---- b207 (Evan Nov 9 "Raid hud A ... how to hurt yes put these in game"):
	//  A = ONE BOSS PLATE (top centre): the boss face, name, HP with phase ticks at 66% / 33%, SHOWTIME + BOSS ANGER as thin meters,
	//      TEAM LIVES as hearts at the left end, ROUND x/10 as a pill at the right end; boss warnings in ONE ticker slot under it.
	//  D = HOW TO HURT IT: a card slides in under the plate for 3 s when a boss event is announced (what to do + what you get).
	//  -ZoogiOldRaidHud = the b151 bar.
	namespace
	{
		struct FHowCard { int32 Kind = 0; FString Title; double At = -1.0; };
		TArray<FHowCard> GHow;
		FString GTickLine; FLinearColor GTickCol = FLinearColor::White; double GTickAt = -100.0;
		UTexture2D* GFace[2] = { nullptr, nullptr }; bool GFaceTried[2] = { false, false };
		float GHpShow = 1.f;
		constexpr float CardSec = 3.f;
		bool OldHud() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRaidHud")); return b; }
		UTexture2D* Face()
		{
			const int32 I = Dragon() ? 0 : 1;
			if (!GFaceTried[I])
			{
				GFaceTried[I] = true;
				const FString P = FPaths::ProjectContentDir() / (I == 0 ? TEXT("Data/MenuArt/Raid/face_frozen.png") : TEXT("Data/MenuArt/Raid/face_generic.png"));
				if (FPaths::FileExists(P)) GFace[I] = FImageUtils::ImportFileAsTexture2D(P);
				if (GFace[I]) GFace[I]->AddToRoot();
			}
			return GFace[I];
		}
		void Tri(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& D, const FLinearColor& Col)
		{
			const FTexture* Wt = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
			TArray<FCanvasUVTri> Tris; FCanvasUVTri T; T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = D; T.V0_Color = T.V1_Color = T.V2_Color = Col; Tris.Add(T);
			FCanvasTriangleItem Ti(Tris, Wt); Ti.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ti);
		}
		void Heart(UCanvas* C, const FVector2D& P, float Sz, const FLinearColor& Col)
		{
			Dot(C, P + FVector2D(-Sz * 0.25f, -Sz * 0.1f), Sz * 0.29f, Col);
			Dot(C, P + FVector2D(Sz * 0.25f, -Sz * 0.1f), Sz * 0.29f, Col);
			Tri(C, P + FVector2D(-Sz * 0.53f, -Sz * 0.02f), P + FVector2D(Sz * 0.53f, -Sz * 0.02f), P + FVector2D(0.f, Sz * 0.55f), Col);
		}
		// b210 (Evan: "Add edge arrow" - raid locator option A): boss off-screen -> a red P5 arrow plate on the screen edge points at it
		// (face, distance, HP); on-screen -> red lock-on brackets around the weak point. -ZoogiNoEdgeArrow = off.
		void DrawEdgeArrow(AZoogiGameMode* G, UCanvas* C, const FVector& Bp)
		{
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoEdgeArrow"));
			if (bOff) return;
			APlayerController* Pc = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr;
			if (!Pc || !Pc->PlayerCameraManager) return;
			const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
			const double Tn = FPlatformTime::Seconds();
			const FVector CamL = Pc->PlayerCameraManager->GetCameraLocation(); const FRotator CamR = Pc->PlayerCameraManager->GetCameraRotation();
			const FVector Fw = CamR.Vector(), Rt = FRotationMatrix(CamR).GetUnitAxis(EAxis::Y), Up = FRotationMatrix(CamR).GetUnitAxis(EAxis::Z);
			const FVector D = Bp - CamL; const float Fz = FVector::DotProduct(D, Fw);
			const FVector Sp = C->Project(Bp);
			const FLinearColor Rd(0.9f, 0.05f, 0.1f, 1.f), Wh(1.f, 1.f, 1.f, 1.f), Ink(0.02f, 0.02f, 0.04f, 0.95f);
			const AZoogiBall* Me = nullptr; for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) { Me = B; break; }
			const float DistM = Me ? FVector::Dist2D(Me->BallLocation(), Bp) / 100.f : FVector::Dist2D(CamL, Bp) / 100.f;
			const float Mx = 70.f * U, MyT = Ht * 0.24f, MyB = Ht * 0.86f; // keep clear of the boss plate / cards on top and the buttons at the bottom
			const bool bOn = Fz > 0.f && Sp.X > Mx && Sp.X < Wd - Mx && Sp.Y > MyT * 0.5f && Sp.Y < MyB;
			if (bOn)
			{	// lock-on brackets, shrinking a little with distance, gently pulsing
				const float Pz = 1.f + 0.08f * FMath::Sin((float)Tn * 7.f);
				const float Hs = FMath::Clamp(5200.f / FMath::Max(300.f, Fz), 34.f, 120.f) * U * Pz, Ln = Hs * 0.42f, Th = 5.f * U;
				const FVector2D Cc(Sp.X, Sp.Y);
				for (int32 sx = -1; sx <= 1; sx += 2) for (int32 sy = -1; sy <= 1; sy += 2)
				{
					const FVector2D K = Cc + FVector2D(sx * Hs, sy * Hs);
					ZoogiFunCanvasRect(C, sx < 0 ? K.X : K.X - Ln, K.Y - Th * 0.5f, Ln, Th, Rd);
					ZoogiFunCanvasRect(C, K.X - Th * 0.5f, sy < 0 ? K.Y : K.Y - Ln, Th, Ln, Rd);
				}
				ZoogiFunCanvasText(C, FString::Printf(TEXT("WEAK POINT  %.0f m"), DistM), Cc.X, Cc.Y + Hs + 22.f * U, 22.f * U, FLinearColor(1.f, 0.35f, 0.3f, 1.f), 1, 0);
				return;
			}
			// direction on screen toward the boss (behind the camera = flip)
			FVector2D Dir(FVector::DotProduct(D, Rt), -FVector::DotProduct(D, Up));
			if (Fz < 0.f) { if (Dir.Size() < 1.f) Dir = FVector2D(0.f, 1.f); }
			else Dir = FVector2D(Sp.X - Wd * 0.5f, Sp.Y - Ht * 0.5f);
			Dir.Normalize(); if (Dir.IsNearlyZero()) Dir = FVector2D(1.f, 0.f);
			// slide from the centre along Dir until it hits the inset box
			const FVector2D Ct(Wd * 0.5f, Ht * 0.55f);
			const float PW = 230.f * U, PH = 78.f * U;
			const float Lx = Wd * 0.5f - Mx - PW * 0.5f, LyT = Ct.Y - MyT - PH * 0.5f, LyB = MyB - Ct.Y - PH * 0.5f;
			const float Tx = FMath::Abs(Dir.X) > 1e-3f ? Lx / FMath::Abs(Dir.X) : 1e9f, Ty = Dir.Y > 1e-3f ? LyB / Dir.Y : (Dir.Y < -1e-3f ? LyT / -Dir.Y : 1e9f);
			const FVector2D P = Ct + Dir * FMath::Min(Tx, Ty);
			const float Bob = 6.f * U * FMath::Sin((float)Tn * 6.f);
			const FVector2D Pc2 = P + Dir * Bob;
			// the arrow tip (outside the plate, toward the boss)
			const FVector2D Nrm(-Dir.Y, Dir.X);
			const float Rx = PW * 0.5f * FMath::Abs(Dir.X) + PH * 0.5f * FMath::Abs(Dir.Y);
			const FVector2D Base = Pc2 + Dir * (Rx - 4.f * U), Tip = Pc2 + Dir * (Rx + 44.f * U);
			Tri(C, Base + Nrm * 30.f * U, Base - Nrm * 30.f * U, Tip + Dir * 6.f * U, Wh);
			Tri(C, Base + Nrm * 22.f * U, Base - Nrm * 22.f * U, Tip, Rd);
			// the plate: white rim, red body, ink shadow; face, distance, HP
			const FVector2D TL = Pc2 - FVector2D(PW, PH) * 0.5f;
			ZoogiFunCanvasRect(C, TL.X + 7.f * U, TL.Y + 7.f * U, PW, PH, Ink);
			ZoogiFunCanvasRect(C, TL.X - 4.f * U, TL.Y - 4.f * U, PW + 8.f * U, PH + 8.f * U, Wh);
			ZoogiFunCanvasRect(C, TL.X, TL.Y, PW, PH, Rd);
			const float Fs = PH - 14.f * U;
			ZoogiFunCanvasRect(C, TL.X + 6.f * U, TL.Y + 6.f * U, Fs + 2.f * U, Fs + 2.f * U, Wh);
			if (UTexture2D* Ft = Face(); Ft && Ft->GetResource()) { FCanvasTileItem It(FVector2D(TL.X + 7.f * U, TL.Y + 7.f * U), Ft->GetResource(), FVector2D(Fs, Fs), FLinearColor::White); It.BlendMode = SE_BLEND_Opaque; C->DrawItem(It); }
			const float Tx0 = TL.X + Fs + 20.f * U, Tw = PW - Fs - 30.f * U;
			ZoogiFunCanvasText(C, FString::Printf(TEXT("%.0f m"), DistM), Tx0 + Tw * 0.5f, TL.Y + PH * 0.36f, 32.f * U, Wh, 1, 0);
			const float Frac = FMath::Clamp((float)S.HP / FMath::Max(1, S.Max), 0.f, 1.f);
			ZoogiFunCanvasRect(C, Tx0, TL.Y + PH * 0.68f, Tw, 9.f * U, Ink);
			ZoogiFunCanvasRect(C, Tx0, TL.Y + PH * 0.68f, Tw * Frac, 9.f * U, FLinearColor(1.f, 0.6f, 0.15f, 1.f));
		}
		void CardText(int32 Kind, FString& Title, FString& Act, FString& Rew, FString& Glyph)
		{
			const bool bD = Dragon();
			switch (Kind)
			{
			case 0: Title = bD ? TEXT("HEART CRYSTAL - HER WEAK POINT") : TEXT("WEAK POINT"); Act = bD ? TEXT("Roll into her glowing heart crystal or knock orbs into it") : TEXT("Roll into the glowing weak point or knock orbs into it"); Rew = TEXT("star orbs hit x2.5"); Glyph = TEXT("x2.5"); break;
			case 1: Title = TEXT("HAND SWEEP"); Act = TEXT("Get out of the red zone or hide behind cover"); Rew = TEXT("hit the weak point before it lands = INTERRUPT"); Glyph = TEXT("!"); break;
			case 2: Title = TEXT("GROUND SLAM"); Act = TEXT("Roll clear of the rings"); Rew = TEXT("a dodge = +1 crystal"); Glyph = TEXT("!"); break;
			case 3: Title = TEXT("BOSS GRAB"); Act = TEXT("TAP FAST to break free"); Rew = TEXT("win = +3 SP"); Glyph = TEXT("TAP"); break;
			case 4: Title = TEXT("ANGER DUEL"); Act = TEXT("Tap the moment it flashes - not before"); Rew = TEXT("win 2 of 3 - or lose 50 points"); Glyph = TEXT("VS"); break;
			case 5: Title = TEXT("BOSS ATTACK"); Act = TEXT("Score high in the SHOWTIME minigame"); Rew = TEXT("your grade = the damage"); Glyph = TEXT("S"); break;
			default: Title = TEXT("INVASION: FIST SLAM"); Act = TEXT("Roll clear of the rings"); Rew = TEXT("a dodge = +1 crystal"); Glyph = TEXT("!"); break;
			}
		}
		void DrawCard(UCanvas* C, float U, float Cx, float Y, float W, const FHowCard& Cd, double Tn)
		{
			const float Age = (float)(Tn - Cd.At);
			const float In = FMath::Clamp(Age / 0.22f, 0.f, 1.f), Out = FMath::Clamp((CardSec - Age) / 0.25f, 0.f, 1.f);
			const float Ein = 1.f - FMath::Pow(1.f - In, 3.f);
			const float A = FMath::Min(In * 2.f, Out);
			if (A <= 0.f) return;
			const float H = 116.f * U, Sl = 14.f * U;
			const float X = Cx - W * 0.5f - (1.f - Ein) * 120.f * U;
			const FVector2D Ctr(X + W * 0.5f, Y + H * 0.5f);
			FString Title, Act, Rew, Glyph; CardText(Cd.Kind, Title, Act, Rew, Glyph);
			if (!Cd.Title.IsEmpty()) Title = Cd.Title;
			ZoogiFunCanvasQuad(C, Ctr + FVector2D(8.f, 8.f) * U, W, H, 0.f, Sl, FLinearColor(0.85f, 0.05f, 0.1f, 0.95f * A));
			ZoogiFunCanvasQuad(C, Ctr, W + 6.f * U, H + 6.f * U, 0.f, Sl, FLinearColor(1.f, 1.f, 1.f, 0.95f * A));
			ZoogiFunCanvasQuad(C, Ctr, W, H, 0.f, Sl, FLinearColor(0.03f, 0.025f, 0.06f, 0.96f * A));
			// the icon: a red disc with the event's mark
			const FVector2D Ic(X + 58.f * U, Y + H * 0.5f);
			Dot(C, Ic, 38.f * U, FLinearColor(1.f, 1.f, 1.f, A));
			Dot(C, Ic, 33.f * U, FLinearColor(0.88f, 0.08f, 0.12f, A));
			ZoogiFunCanvasText(C, Glyph, Ic.X, Ic.Y, (Glyph.Len() > 2 ? 24.f : 40.f) * U, FLinearColor(1.f, 1.f, 1.f, A), 1, 0);
			const float Tx = X + 112.f * U, Tw = W - 130.f * U;
			auto Fit = [&](const FString& Str, float Px) { const float Mw = ZoogiFunCanvasMeasure(Str, Px, 0).X; return Mw > Tw ? Px * Tw / Mw : Px; };
			ZoogiFunCanvasText(C, Title, Tx, Y + 24.f * U, Fit(Title, 28.f * U), FLinearColor(1.f, 0.3f, 0.25f, A), 0, 0);
			ZoogiFunCanvasText(C, TEXT("HOW TO HURT IT"), X + W - 26.f * U, Y + 20.f * U, 15.f * U, FLinearColor(0.75f, 0.75f, 0.82f, A), 2, 0);
			ZoogiFunCanvasText(C, Act, Tx, Y + 60.f * U, Fit(Act, 26.f * U), FLinearColor(1.f, 1.f, 1.f, A), 0, 0);
			ZoogiFunCanvasText(C, Rew, Tx, Y + 94.f * U, Fit(Rew, 22.f * U), FLinearColor(Gold.R, Gold.G, Gold.B, A), 0, 0);
		}
		void DrawPlate(AZoogiGameMode* G, UCanvas* C)
		{
			const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
			const bool bPort = Ht > Wd;
			const double Tn = FPlatformTime::Seconds();
			const bool bShow = ZoogiGattling::Active(); // SHOWTIME owns the top of the screen: only the card
			const float PW = bPort ? Wd * 0.94f : FMath::Min(Wd * 0.50f, 1000.f * U), PH = 122.f * U;
			const float Px0 = (Wd - PW) * 0.5f, Py0 = bPort ? Ht * 0.115f : 16.f * U;
			float Under = Py0 + PH + 12.f * U; // the ticker / card slot
			if (!bShow)
			{
				const float Sl = 16.f * U; const FVector2D Pc(Px0 + PW * 0.5f, Py0 + PH * 0.5f);
				ZoogiFunCanvasQuad(C, Pc + FVector2D(9.f, 8.f) * U, PW, PH, 0.f, Sl, FLinearColor(0.85f, 0.05f, 0.1f, 0.95f));
				ZoogiFunCanvasQuad(C, Pc, PW + 6.f * U, PH + 6.f * U, 0.f, Sl, FLinearColor(1.f, 1.f, 1.f, 0.95f));
				ZoogiFunCanvasQuad(C, Pc, PW, PH, 0.f, Sl, FLinearColor(0.03f, 0.025f, 0.06f, 0.94f));
				// the face
				const float Fs = PH - 20.f * U, Fx = Px0 + 22.f * U, Fy = Py0 + 10.f * U;
				ZoogiFunCanvasRect(C, Fx - 4.f * U, Fy - 4.f * U, Fs + 8.f * U, Fs + 8.f * U, S.Phase >= 3 ? FLinearColor(1.f, 0.1f, 0.05f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 1.f));
				if (UTexture2D* Ft = Face(); Ft && Ft->GetResource()) { FCanvasTileItem It(FVector2D(Fx, Fy), Ft->GetResource(), FVector2D(Fs, Fs), FLinearColor::White); It.BlendMode = SE_BLEND_Opaque; C->DrawItem(It); }
				else ZoogiFunCanvasRect(C, Fx, Fy, Fs, Fs, FLinearColor(0.5f, 0.05f, 0.08f, 1.f));
				const float Cx = Fx + Fs + 20.f * U, Cw = Px0 + PW - Cx - 30.f * U;
				// row 1: the name + health
				const float Frac = FMath::Clamp((float)S.HP / FMath::Max(1, S.Max), 0.f, 1.f);
				ZoogiFunCanvasText(C, ZoogiCosmosBoss::Name(), Cx, Py0 + 24.f * U, 30.f * U, FLinearColor::White, 0, 0);
				ZoogiFunCanvasText(C, FString::Printf(TEXT("%d / %d"), S.HP, S.Max), Cx + Cw, Py0 + 24.f * U, 26.f * U, Gold, 2, 0);
				// row 2: the health bar with the phase ticks (66% = PHASE 2, 33% = ENRAGED) and a white trail for the last hit
				const float By = Py0 + 46.f * U, Bh = 22.f * U;
				GHpShow = GHpShow > Frac ? FMath::Max(Frac, GHpShow - 0.5f * (float)FMath::Min(0.1, (double)FApp::GetDeltaTime())) : Frac;
				ZoogiFunCanvasRect(C, Cx - 3.f * U, By - 3.f * U, Cw + 6.f * U, Bh + 6.f * U, FLinearColor(0.f, 0.f, 0.f, 1.f));
				ZoogiFunCanvasRect(C, Cx, By, Cw, Bh, FLinearColor(0.25f, 0.05f, 0.06f, 1.f));
				ZoogiFunCanvasRect(C, Cx, By, Cw * GHpShow, Bh, FLinearColor(1.f, 0.95f, 0.85f, 0.9f));
				const float Pl = S.Phase >= 3 ? 0.8f + 0.2f * FMath::Sin((float)Tn * 10.f) : 1.f;
				ZoogiFunCanvasRect(C, Cx, By, Cw * Frac, Bh, S.Phase >= 3 ? FLinearColor(1.f * Pl, 0.1f, 0.05f, 1.f) : (S.Phase == 2 ? FLinearColor(1.f, 0.42f, 0.08f, 1.f) : FLinearColor(1.f, 0.55f, 0.15f, 1.f)));
				ZoogiFunCanvasRect(C, Cx, By, Cw * Frac, Bh * 0.3f, FLinearColor(1.f, 1.f, 1.f, 0.25f));
				for (const float Tk : { 0.66f, 0.33f })
				{
					const float X = Cx + Cw * Tk;
					ZoogiFunCanvasRect(C, X - 1.5f * U, By - 3.f * U, 3.f * U, Bh + 6.f * U, FLinearColor::White);
					Tri(C, FVector2D(X - 7.f * U, By - 11.f * U), FVector2D(X + 7.f * U, By - 11.f * U), FVector2D(X, By - 2.f * U), Frac > Tk ? FLinearColor::White : FLinearColor(1.f, 0.2f, 0.15f, 1.f));
				}
				// row 3: SHOWTIME + BOSS ANGER, thin
				{
					const float My = Py0 + 84.f * U, Mh = 8.f * U, Lw = 88.f * U, Hw = Cw * 0.5f - 12.f * U;
					int32 Hu = 0; for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) { Hu = B->PlayerIndex; break; }
					const float Sm = ZoogiGattling::Meter(Hu);
					bool bAr = false; int32 AngBy = -1; const float An = ZoogiBossAttack::AngerFrac(&bAr, &AngBy);
					const float Ap = bAr ? 0.65f + 0.35f * FMath::Sin((float)Tn * 9.f) : 1.f;
					ZoogiFunCanvasText(C, Sm >= 0.999f ? TEXT("SHOWTIME!") : TEXT("SHOWTIME"), Cx, My + Mh * 0.5f, 15.f * U, Gold, 0, 0);
					ZoogiFunCanvasRect(C, Cx + Lw, My, Hw - Lw, Mh, FLinearColor(0.15f, 0.13f, 0.1f, 1.f));
					ZoogiFunCanvasRect(C, Cx + Lw, My, (Hw - Lw) * FMath::Clamp(Sm, 0.f, 1.f), Mh, Gold);
					const float Ax = Cx + Cw * 0.5f + 12.f * U;
					ZoogiFunCanvasText(C, bAr ? TEXT("FURIOUS!") : TEXT("ANGER"), Ax, My + Mh * 0.5f, 15.f * U, FLinearColor(1.f, 0.35f, 0.25f, 1.f), 0, 0);
					ZoogiFunCanvasRect(C, Ax + Lw * 0.8f, My, Hw - Lw * 0.8f, Mh, FLinearColor(0.15f, 0.06f, 0.06f, 1.f));
					ZoogiFunCanvasRect(C, Ax + Lw * 0.8f, My, (Hw - Lw * 0.8f) * FMath::Clamp(An, 0.f, 1.f), Mh, FLinearColor(1.f * Ap, (0.55f - 0.45f * An) * Ap, 0.08f, 1.f));
				}
				// row 4: TEAM LIVES hearts (left end) + the ROUND pill (right end)
				{
					const float Ry = Py0 + 106.f * U, Hs = 17.f * U;
					for (int32 i = 0; i < TeamLives; ++i)
					{
						const FVector2D Hp(Cx + 10.f * U + i * 25.f * U, Ry);
						Heart(C, Hp, Hs + 3.f * U, FLinearColor(0.f, 0.f, 0.f, 0.9f));
						Heart(C, Hp, Hs, i < S.Lives ? FLinearColor(1.f, 0.18f, 0.25f, 1.f) : FLinearColor(0.3f, 0.28f, 0.32f, 1.f));
					}
					ZoogiFunCanvasText(C, TEXT("TEAM"), Cx + 10.f * U + TeamLives * 25.f * U, Ry, 14.f * U, FLinearColor(0.8f, 0.8f, 0.86f, 1.f), 0, 0);
					const FString Rs = FString::Printf(TEXT("ROUND %d / %d"), FMath::Min(G->RoundNumber, Rounds), Rounds);
					const float Pw = ZoogiFunCanvasMeasure(Rs, 17.f * U, 0).X + 26.f * U;
					ZoogiFunCanvasQuad(C, FVector2D(Cx + Cw - Pw * 0.5f, Ry), Pw, 22.f * U, 0.f, 6.f * U, Gold);
					ZoogiFunCanvasText(C, Rs, Cx + Cw - Pw * 0.5f, Ry, 17.f * U, FLinearColor(0.03f, 0.02f, 0.05f, 1.f), 1, 0);
				}
			}
			else Under = Ht * 0.12f;
			// ---- D: the HOW TO HURT IT card (one at a time; the next waits its turn)
			const bool bLive = G->Phase != EZoogiPhase::Forming && G->Phase != EZoogiPhase::DropIntro && G->Phase != EZoogiPhase::Menu;
			while (GHow.Num() > 0 && GHow[0].At >= 0.0 && Tn - GHow[0].At > CardSec) GHow.RemoveAt(0);
			if (GHow.Num() > 0 && GHow[0].At < 0.0 && bLive)
			{
				GHow[0].At = Tn;
				UE_LOG(LogZoogi, Log, TEXT("RAID HOW-TO card: kind %d %s"), GHow[0].Kind, *GHow[0].Title);
				static const bool bShots = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAuto"));
				if (bShots && !G->ShotDir.IsEmpty()) G->TakeShotDelayed(FString::Printf(TEXT("raidhud_card_%d"), GHow[0].Kind), 0.6f);
			}
			if (GHow.Num() > 0 && GHow[0].At >= 0.0) { DrawCard(C, U, Wd * 0.5f, Under, bPort ? Wd * 0.94f : FMath::Max(PW, 760.f * U), GHow[0], Tn); return; }
			if (bShow) return;
			// ---- A: the ONE warning slot (the latest event line, else the coming attack, else the boss's anger)
			FString Line; FLinearColor Col = FLinearColor(1.f, 0.25f, 0.2f, 1.f); bool bPulse = false;
			if (Tn - GTickAt < 3.0) { Line = GTickLine; Col = GTickCol; }
			else if (ZoogiBossAttack::Warning(G, Line)) bPulse = true;
			else { bool bAr = false; int32 By = -1; ZoogiBossAttack::AngerFrac(&bAr, &By); if (bAr && By >= 0) { Line = FString::Printf(TEXT("%s IS FURIOUS - P%d DUELS IT ON THEIR TURN"), *ZoogiCosmosBoss::Name(), By + 1); bPulse = true; } }
			if (Line.IsEmpty() && S.ExposeUntil >= G->TurnCounter) { Line = TEXT("WEAK POINT EXPOSED - EVERY HIT x2"); Col = FLinearColor(0.55f, 0.9f, 1.f, 1.f); }
			if (Line.IsEmpty()) return;
			const float Px = 26.f * U; float Tw = ZoogiFunCanvasMeasure(Line, Px, 0).X; const float MaxW = bPort ? Wd * 0.94f : PW;
			const float Ps = Tw + 70.f * U > MaxW ? Px * (MaxW - 70.f * U) / Tw : Px; Tw = FMath::Min(Tw, MaxW - 70.f * U);
			const float Ty = Under + 22.f * U, Pu = bPulse ? 0.8f + 0.2f * FMath::Sin((float)Tn * 7.f) : 1.f;
			ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f + 5.f * U, Ty + 5.f * U), Tw + 60.f * U, 40.f * U, 0.f, 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f));
			ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Ty), Tw + 60.f * U, 40.f * U, 0.f, 12.f * U, FLinearColor(Col.R * 0.85f * Pu, Col.G * 0.85f * Pu, Col.B * 0.85f * Pu, 0.95f));
			ZoogiFunCanvasText(C, FString::Printf(TEXT("!  %s  !"), *Line), Wd * 0.5f, Ty, Ps, FLinearColor::White, 1, 0);
		}
	}

	void ResetHud() { GHow.Reset(); GTickAt = -100.0; GHpShow = 1.f; }
	bool NewHud() { return S.bOn && !OldHud(); }
	bool CardUp() { return GHow.Num() > 0 && GHow[0].At >= 0.0; }
	void HowTo(int32 Kind, const FString& Title)
	{
		if (!S.bOn || OldHud()) return;
		for (int32 i = 1; i < GHow.Num(); ++i) if (GHow[i].Kind == Kind) return; // already waiting
		if (GHow.Num() > 0 && GHow[0].Kind == Kind && GHow[0].At < 0.0) return;
		if (GHow.Num() >= 3) GHow.RemoveAt(1);
		FHowCard Cd; Cd.Kind = Kind; Cd.Title = Title.ToUpper(); GHow.Add(Cd);
	}
	void Ticker(const FString& Line, const FLinearColor& Col) { if (!S.bOn) return; GTickLine = Line.ToUpper(); GTickCol = Col; GTickAt = FPlatformTime::Seconds(); }
	void ShowtimeHit(AZoogiGameMode* G, int32 Player, float K)
	{	// b207 BOSS ATTACK: in a raid the SHOWTIME minigame's grade is the damage
		if (!S.bOn || !G || S.bEnding) return;
		K = FMath::Clamp(K, 0.f, 1.f);
		const TCHAR* Gr = K >= 0.85f ? TEXT("S") : K >= 0.6f ? TEXT("A") : K >= 0.35f ? TEXT("B") : TEXT("C");
		const int32 Amt = FMath::Max(1, FMath::RoundToInt(S.Max * (0.06f + 0.24f * K)));
		Damage(G, Player, Amt, HandPos(G), TEXT("boss attack"));
		Ticker(FString::Printf(TEXT("BOSS ATTACK GRADE %s  -  %d DAMAGE"), Gr, Amt), Gold);
		UE_LOG(LogZoogi, Log, TEXT("RAID BOSS ATTACK (SHOWTIME) P%d grade %s (%.2f) = %d damage"), Player + 1, Gr, K, Amt);
	}

	void Draw(AZoogiGameMode* G, UCanvas* C)
	{
		if (!G || !C || (!S.bOn && !S.bEnding) || G->Phase == EZoogiPhase::GameOver) { if (GCap.IsValid()) DropCap(); }
		if (!G || !C || (!S.bOn && !S.bEnding)) return;
		if (G->Phase == EZoogiPhase::GameOver) return;
		if (S.bOn && LocOn() && !ZoogiGattling::Active())
			if (AActor* Hb = S.Hand.Get()) DrawLocators(G, C, Hb->GetActorLocation());
		if (S.bOn && LocOn() && !ZoogiGattling::Active() && G->Phase != EZoogiPhase::Forming)
			if (AActor* Hb2 = S.Hand.Get()) DrawEdgeArrow(G, C, Hb2->GetActorLocation()); // b210 locator A
		if (!OldHud()) { DrawPlate(G, C); return; } // b207 HUD A + D
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
		const float BW = FMath::Min(Wd * 0.46f, 900.f * U), BH = 26.f * U, X = (Wd - BW) * 0.5f, Y = Ht * 0.17f; // under the turn / round labels
		const float Frac = FMath::Clamp((float)S.HP / FMath::Max(1, S.Max), 0.f, 1.f);
		ZoogiFunCanvasRect(C, X - 6.f * U, Y - 6.f * U, BW + 12.f * U, BH + 12.f * U, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		ZoogiFunCanvasRect(C, X, Y, BW, BH, FLinearColor(0.25f, 0.05f, 0.05f, 0.9f));
		ZoogiFunCanvasRect(C, X, Y, BW * Frac, BH, S.Phase >= 3 ? FLinearColor(1.f, 0.1f, 0.05f, 1.f) : FLinearColor(1.f, 0.35f, 0.1f, 1.f));
		ZoogiFunCanvasText(C, FString::Printf(TEXT("%s   %d / %d"), *ZoogiCosmosBoss::Name(), S.HP, S.Max), Wd * 0.5f, Y - 22.f * U, 30.f * U, FLinearColor::White, 1, 0);
		FString Lives; for (int32 i = 0; i < TeamLives; ++i) Lives += i < S.Lives ? TEXT("O ") : TEXT("- ");
		ZoogiFunCanvasText(C, FString::Printf(TEXT("TEAM LIVES  %s   ROUND %d / %d"), *Lives, FMath::Min(G->RoundNumber, Rounds), Rounds), Wd * 0.5f, Y + BH + 22.f * U, 24.f * U, Gold, 1, 0);
	}
}
