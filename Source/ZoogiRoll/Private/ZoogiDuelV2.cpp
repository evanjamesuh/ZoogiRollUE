// Nov 3 b90 (Evan 15:18): the clash button-mash is gone. New duel sequence (every Zoogi-vs-Zoogi clash / redirect duel):
//  1 CIRCLE MATCH - the turn player taps REACTION as the rings meet. MISS = the duel ends, the hit plays out normally.
//  2 GOOD = +3 SP, PERFECT = +5 SP; the turn player picks ATTACK (2 SP, 25 HP) / SKILL (3 SP, 35 HP) / GUARD / ITEM.
//  3 the rival spends the SP it has right then: DEFEND (2 SP, half damage) and/or COUNTER (2 SP, 20 HP back) - or takes the blow.
//  4 the hits land on the HP bars (100 HP per Zoogi, kept for the match); 0 HP = KNOCKOUT (blasted off the arena, back at 100).
// -ZoogiOldDuel = the b66 duel, -ZoogiOldClash = the tap-tug clash again.
#include "ZoogiActionFun.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiSkills.h"
#include "ZoogiHaptics.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiClashSplit.h"
#include "ZoogiCPUBrain.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Components/SphereComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiScreenFx.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace ZoogiDuelV2
{
	bool On() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldDuel")); return b; }
	bool ReplacesClash() { static const bool b = On() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldClash")); return b; }
	bool Test() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelTest")); return b; }
	constexpr float VIntro = 1.2f, VRing = 1.6f, VMenuMax = 10.f, VRivalMax = 6.f, VResolve = 2.4f, VResolveGuard = 3.1f;
	constexpr int32 CostAttack = 2, CostSkill = 3, CostDefend = 2, CostCounter = 2;
	constexpr float DmgAttack = 25.f, DmgSkill = 35.f, DmgCounter = 20.f;
	const TCHAR* PickName[4] = { TEXT("ATTACK"), TEXT("SKILL"), TEXT("GUARD"), TEXT("ITEM") };
	const TCHAR* PickSub[4] = { TEXT("2 SP  -  25 DAMAGE"), TEXT("3 SP  -  35 DAMAGE"), TEXT("FREE  -  HALF A COUNTER"), TEXT("FREE  -  +25 SUPER") };
	const TCHAR* RivalName[4] = { TEXT("DEFEND"), TEXT("COUNTER"), TEXT("BOTH"), TEXT("TAKE IT") };
	const TCHAR* RivalSub[4] = { TEXT("2 SP  -  HALF DAMAGE"), TEXT("2 SP  -  20 DAMAGE BACK"), TEXT("4 SP  -  DEFEND + COUNTER"), TEXT("NO SP") };
	int32 RivalCost(int32 i) { return i == 0 ? CostDefend : i == 1 ? CostCounter : i == 2 ? CostDefend + CostCounter : 0; }
	int32 PickCost(int32 i) { return i == 0 ? CostAttack : i == 1 ? CostSkill : 0; }
	float CpuOffset()
	{
		const EZoogiCPUTier T = ZoogiCPU::ActiveTier();
		const float Spread = T == EZoogiCPUTier::Easy ? 0.34f : T == EZoogiCPUTier::Normal ? 0.22f : T == EZoogiCPUTier::Hard ? 0.13f : 0.08f;
		return FMath::FRandRange(-Spread, Spread);
	}
	int32 GradeOf(float Off) { const float D = FMath::Abs(Off); return D <= 0.13f ? 2 : (D <= 0.28f ? 1 : 0); }
}

int32 UZoogiFunSys::DuelSp(AZoogiGameMode* G, int32 P) const
{
	const UZoogiCardSubsystem* CS = G ? UZoogiCardSubsystem::Get(G) : nullptr;
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(P) : nullptr;
	static const bool bNoReserve = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDuelReserve"));
	const int32 Rsv = (!bNoReserve && Players.IsValidIndex(P)) ? Players[P].DuelReserve : 0; // b96: the 2 SP duel reserve
	return PS ? FMath::Max(0, PS->Energy) + Rsv : 99; // cards off: SP is no limit
}

bool UZoogiFunSys::DuelSpend(AZoogiGameMode* G, int32 P, int32 N)
{
	if (N <= 0) return true;
	UZoogiCardSubsystem* CS = G ? UZoogiCardSubsystem::Get(G) : nullptr;
	if (!CS || !G->CardsOn()) return true;
	if (DuelSp(G, P) < N) return false;
	// b96: card SP first, then the duel reserve
	const FZoogiCardPlayerState* PS = CS->Rules().Player(P);
	const int32 FromE = FMath::Min(N, PS ? FMath::Max(0, PS->Energy) : 0);
	if (FromE > 0) CS->SpendEnergy(P, FromE);
	if (N > FromE && Players.IsValidIndex(P)) Players[P].DuelReserve = FMath::Max(0, Players[P].DuelReserve - (N - FromE));
	return true;
}

float UZoogiFunSys::DuelHP(int32 P) const { return Players.IsValidIndex(P) ? Players[P].HP : 100.f; }

void UZoogiFunSys::DuelV2Tap(AZoogiGameMode* G)
{
	if (Clash.DStage != 10 || Clash.bDQteDone || !Clash.bHumanAtt) return;
	Clash.bDQteDone = true;
	Clash.DGrade = ZoogiDuelV2::GradeOf(Clash.DAge - Clash.DQteAt);
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 circle tap %+.2f s -> %d"), Clash.DAge - Clash.DQteAt, Clash.DGrade);
}

void UZoogiFunSys::DuelV2Pick(AZoogiGameMode* G, int32 i)
{
	using namespace ZoogiDuelV2;
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	if (!A || !D) return;
	UWorld* W = GetWorld();
	if (Clash.DStage == 11)
	{
		i = FMath::Clamp(i, 0, 3);
		if (DuelSp(G, A->PlayerIndex) < PickCost(i)) { ZoogiUtil::PlaySfx(G, TEXT("UiBack"), 0.7f, 1.f); return; } // can't afford it
		DuelSpend(G, A->PlayerIndex, PickCost(i));
		Clash.VPick = i;
		Clash.DMove = PickName[i];
		const int32 Fa = Clash.F[0].Form;
		if (i <= 1) PlayFighter(0, FormSignature(Fa), i == 1 ? 0.7f : 0.8f, false);
		else if (i == 2) PlayFighter(0, TEXT("idle"), 1.f, true);
		else { AddMeter(G, A->PlayerIndex, 25.f, TEXT("duel item")); PlayFighter(0, TEXT("box_02"), 0.8f, false); }
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.1f);
		Clash.DStage = 12; Clash.DAge = 0.f; Clash.DHover = -1;
		// the rival's choice: a CPU decides with what it has, a person gets the menu
		const int32 Rs = DuelSp(G, D->PlayerIndex);
		const bool bHit = i <= 1;
		if (!Clash.bHumanDef)
		{
			int32 Want = 3;
			if (bHit && Rs >= CostDefend + CostCounter) Want = FMath::FRand() < 0.6f ? 2 : 0;
			else if (bHit && Rs >= CostDefend) Want = FMath::FRand() < 0.6f ? 0 : 1;
			else if (!bHit && Rs >= CostCounter) Want = FMath::FRand() < 0.5f ? 1 : 3;
			Clash.VRivalWant = Want; Clash.DCpuPickAt = FMath::FRandRange(1.f, 1.8f);
		}
		UE_LOG(LogZoogi, Log, TEXT("DUEL2 P%d picks %s (SP left %d); rival P%d has %d SP"), A->PlayerIndex + 1, PickName[i], DuelSp(G, A->PlayerIndex), D->PlayerIndex + 1, Rs);
		if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_c_pick"));
		return;
	}
	if (Clash.DStage == 12)
	{
		i = FMath::Clamp(i, 0, 3);
		if (Clash.VPick > 1 && (i == 0 || i == 2)) i = i == 2 ? 1 : 3; // nothing to defend against
		if (DuelSp(G, D->PlayerIndex) < RivalCost(i)) { ZoogiUtil::PlaySfx(G, TEXT("UiBack"), 0.7f, 1.f); if (Clash.bHumanDef) return; i = 3; }
		DuelSpend(G, D->PlayerIndex, RivalCost(i));
		Clash.bVDefend = i == 0 || i == 2; Clash.bVCounter = i == 1 || i == 2;
		Clash.DStage = 13; Clash.DAge = 0.f; Clash.bVLanded = Clash.bVCountered = false;
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 0.9f);
		if (Clash.bVDefend) PlayFighter(1, TEXT("idle"), 1.f, true);
		UE_LOG(LogZoogi, Log, TEXT("DUEL2 rival P%d: %s (SP left %d)"), D->PlayerIndex + 1, RivalName[i], DuelSp(G, D->PlayerIndex));
		if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_d_rival"));
	}
}

void UZoogiFunSys::DuelV2Damage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* What)
{
	if (!Victim || !Players.IsValidIndex(Victim->PlayerIndex)) return;
	float& Hp = Players[Victim->PlayerIndex].HP;
	Hp = FMath::Max(0.f, Hp - Dmg);
	const int32 Side = Victim == Clash.Def.Get() ? 1 : 0;
	(Side ? Clash.VDmgD : Clash.VDmgA) += Dmg; // b94: who lost the exchange
	PlayFighter(Side, TEXT("hit_to_body_01"), 1.f, false);
	UWorld* W = GetWorld();
	AActor* Fv = Clash.F[Side].Actor.Get();
	const FVector P = Fv ? Fv->GetActorLocation() : Victim->BallLocation();
	G->AddPopup(FString::Printf(TEXT("-%d HP"), FMath::RoundToInt(Dmg)), P + FVector(0, 0, 220.f), FLinearColor(1.f, 0.35f, 0.3f));
	AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), P + FVector(0, 0, 40.f), 0.35f);
	ZoogiUtil::PlaySfx(G, TEXT("HitClick"), 1.f, 1.f);
	if (Clash.bHumanAtt || Clash.bHumanDef) ZoogiHaptics::Kick(0.8f, W, TEXT("duel hit"));
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 %s: P%d -%0.f HP -> %.0f"), What, Victim->PlayerIndex + 1, Dmg, Hp);
}

void UZoogiFunSys::DuelV2End(AZoogiGameMode* G, bool bFought)
{
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	ZoogiClashSplit::End();
	TArray<AZoogiBall*> KOs;
	if (bFought) for (AZoogiBall* B : { A, D }) if (B && Players.IsValidIndex(B->PlayerIndex) && Players[B->PlayerIndex].HP <= 0.f) KOs.Add(B);
	// b94 (Evan): the loser of the exchange gets the old ending - sent flying in its form, then its crash anime clip (not after a
	// successful DEFEND - that played the guard forming - and not on a knockout, which blasts it off the arena)
	// b96 (Evan): one ending, at most one full-screen clip - KO > DEFENDED > hit landed (loser flies + crash clip) > counter only
	static const bool bOldEnding = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelOldEnding"));
	enum class EEnd : uint8 { None, Loser, Guard, Counter, KO };
	EEnd End = EEnd::None;
	if (bFought)
	{
		if (KOs.Num()) End = EEnd::KO;
		else if (Clash.bVDefend) End = EEnd::Guard;
		else if (Clash.VDmgD <= 0.f && Clash.VDmgA > 0.f && !bOldEnding) End = EEnd::Counter;
		else if (Clash.VDmgD > 0.f || Clash.VDmgA > 0.f) End = EEnd::Loser;
	}
	FString EndClip; FLinearColor EndTint = FLinearColor::White;
	if (!bOldEnding)
	{
		if (End == EEnd::KO) { EndClip = FString::Printf(TEXT("Anime_Any_Any_KO_%02d"), FMath::RandRange(1, 3)); EndTint = FLinearColor(0.7f, 0.35f, 1.f); }
		else if (End == EEnd::Guard) { EndClip = TEXT("Anime_Any_Any_Infuse_01"); EndTint = FLinearColor(0.45f, 0.85f, 1.f); }
		else if (End == EEnd::Counter && D) { EndClip = FString::Printf(TEXT("Anime_Any_Any_Critical_%02d"), FMath::RandRange(1, 3)); EndTint = ZoogiUtil::PlayerColor(D->PlayerIndex); PlayFighter(0, TEXT("hit_to_body_01"), 1.f, false); }
	}
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 ending %d (clip %s)"), (int32)End, EndClip.IsEmpty() ? TEXT("-") : *EndClip);
	if (End == EEnd::Loser && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelNoLoser")))
	{
		const bool bAttWinsX = Clash.VDmgD >= Clash.VDmgA;
		AZoogiBall* Lb = bAttWinsX ? D : A; AZoogiBall* Wb = bAttWinsX ? A : D;
		if (Lb && Lb->Sphere && !Lb->bOut)
		{
			const FVector Away = bAttWinsX ? Clash.Axis : -Clash.Axis;
			Lb->Sphere->WakeRigidBody();
			Lb->Sphere->SetPhysicsLinearVelocity(Away * 1150.f + FVector(0.f, 0.f, 480.f));
			if (Wb) Lb->LastHitPlayer = Wb->PlayerIndex;
			StartFlung(bAttWinsX ? 1 : 0); // takes the loser's fighter over: backstroke flight, then the crash clip on landing
			UE_LOG(LogZoogi, Log, TEXT("DUEL2 loser P%d sent flying (the classic ending + crash clip)"), Lb->PlayerIndex + 1);
		}
	}
	EndFighters();
	LastDuelEndAt = FPlatformTime::Seconds(); // b98b: the camera watchdog checks the match view for the next 30 s
	const FVector At = Clash.Center;
	Clash = FClash();
	bResolveAfterClash = bFought && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashKeepRolling"));
	for (AZoogiBall* L : KOs) HpKnockout(G, L, L == A ? D : A);
	if (!EndClip.IsEmpty()) if (UZoogiAnimeSubsystem* An = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) An->PlayCrash(EndClip, EndTint); // b96: full screen, refused if a clip is already up (never two)
	AZoogiFX::Spawn(GetWorld(), At, FLinearColor(1.f, 0.9f, 0.6f), 160.f, 0.35f, true, 10.f);
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 over (%s)"), bFought ? TEXT("fought") : TEXT("missed the circle - normal hit"));
}

void UZoogiFunSys::DuelV2GuardForm(AZoogiGameMode* G, AZoogiBall* Def)
{
	// b94 (Evan: "if the opposing player defends successfully a new animation plays off the guard effect forming and taking shape in epic
	// anime action style"): 14 glowing shield shards rush in from all sides, lock into a dome around the defender's form, a white-hot
	// flash, a shock ring, then the dome pulses and fades; the defender's GuardBlock cut-in plays (in its profile corner)
	if (!G || !Def) return;
	UWorld* W = GetWorld();
	AActor* Fd = Clash.F[1].Actor.Get();
	const FVector C = (Fd ? Fd->GetActorLocation() : Def->BallLocation()) + FVector(0.f, 0.f, 70.f);
	const FLinearColor Cy(0.45f, 0.85f, 1.f), Pc = ZoogiUtil::PlayerColor(Def->PlayerIndex);
	struct FShard { TWeakObjectPtr<AActor> A; FVector From, To; FRotator Spin; };
	struct FForm { TArray<FShard> Shards; TWeakObjectPtr<AActor> Dome; float T = 0.f; bool bLocked = false; };
	TSharedRef<FForm> Fm = MakeShared<FForm>();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float R = 150.f;
	for (int32 k = 0; k < 14; ++k)
	{
		const float Az = 2.f * PI * k / 7.f + (k >= 7 ? PI / 7.f : 0.f), El = k >= 7 ? 0.95f : 0.35f;
		const FVector Dir(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El));
		FShard S; S.To = C + Dir * R; S.From = C + Dir * FMath::FRandRange(520.f, 720.f) + FVector(0, 0, FMath::FRandRange(-60.f, 200.f));
		S.Spin = FRotator(FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-720.f, 720.f), 0.f);
		if (AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Dir.Rotation(), S.From), SP))
		{
			A->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->GetStaticMeshComponent()->SetCastShadow(false);
			A->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(A, k % 2 ? Cy : FMath::Lerp(Cy, Pc, 0.5f), 12.f));
			A->SetActorScale3D(FVector(0.75f, 0.75f, 0.06f)); // a flat hex-ish plate
			S.A = A;
		}
		Fm->Shards.Add(S);
	}
	// b96: the defender's clip plays full screen once the duel scene ends (one clip at a time - DuelV2End)
	Callout(TEXT("GUARD FORMED!"), FString::Printf(TEXT("%s BLOCKS THE BLOW"), *Def->Def.Name.ToUpper()), Cy, 1.6f, 1.1f);
	ZoogiUtil::PlaySfx(G, TEXT("AwakenCharge"), 0.8f, 1.4f);
	TWeakObjectPtr<UWorld> WW = W; TWeakObjectPtr<AZoogiGameMode> WGm = G;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Fm, WW, WGm, C, R, Cy](float Dt) -> bool
	{
		UWorld* Wl = WW.Get();
		auto Kill = [&Fm]() { for (FShard& S : Fm->Shards) if (AActor* A = S.A.Get()) A->Destroy(); if (AActor* D = Fm->Dome.Get()) D->Destroy(); };
		if (!Wl) { Kill(); return false; }
		Fm->T += FMath::Min(Dt, 0.05f);
		const float T = Fm->T;
		const float K = FMath::Clamp(T / 0.45f, 0.f, 1.f), Ke = 1.f - FMath::Pow(1.f - K, 4.f); // they slam into place
		for (FShard& S : Fm->Shards)
			if (AActor* A = S.A.Get())
			{
				A->SetActorLocation(FMath::Lerp(S.From, S.To, Ke));
				if (K < 1.f) A->AddActorWorldRotation(S.Spin * Dt * (1.f - K));
				else A->SetActorRotation((S.To - C).Rotation() + FRotator(90.f, 0.f, 0.f)); // locked flat onto the dome
			}
		if (!Fm->bLocked && K >= 1.f)
		{
			Fm->bLocked = true;
			FActorSpawnParameters Sp2; Sp2.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* Dm = Wl->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(C), Sp2))
			{
				Dm->SetMobility(EComponentMobility::Movable);
				Dm->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
				Dm->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Dm->GetStaticMeshComponent()->SetCastShadow(false);
				Dm->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(Dm, Cy, 1.2f));
				Dm->SetActorScale3D(FVector(2.f * R / 100.f));
				Fm->Dome = Dm;
			}
			AZoogiFX::Spawn(Wl, C, FLinearColor(1.f, 1.f, 1.f), 2.4f * R, 0.25f, false, 20.f); // white-hot lock flash
			AZoogiFX::Spawn(Wl, C - FVector(0, 0, 70.f), Cy, 3.f * R, 0.6f, true, 10.f);
			UZoogiScreenFx::Shockwave(Wl, C, 3.f * R, Cy);
			AZoogiFX::Stylized(Wl, TEXT("ElectricShock"), C, 1.2f);
			if (AZoogiGameMode* Gs = WGm.Get()) ZoogiUtil::PlaySfx(Gs, TEXT("GuardClang"), 1.f, 1.f);
			ZoogiHaptics::Kick(0.9f, Wl, TEXT("guard formed"));
		}
		if (AActor* D = Fm->Dome.Get())
		{
			const float P = 1.f + 0.06f * FMath::Sin(T * 18.f);
			D->SetActorScale3D(FVector(2.f * R / 100.f * P * FMath::Max(0.f, 1.f - FMath::Max(0.f, T - 1.6f) / 0.4f)));
		}
		if (T < 2.f) return true;
		Kill();
		return false;
	}));
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 guard forms around P%d"), Def->PlayerIndex + 1);
}

void UZoogiFunSys::HpDamage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* Why)
{
	if (!G || !Victim || Victim->bOut || !Players.IsValidIndex(Victim->PlayerIndex) || Dmg <= 0.f) return;
	float& Hp = Players[Victim->PlayerIndex].HP;
	Hp = FMath::Max(0.f, Hp - Dmg);
	G->AddPopup(FString::Printf(TEXT("-%d HP"), FMath::RoundToInt(Dmg)), Victim->BallLocation() + FVector(0, 0, 250.f), FLinearColor(1.f, 0.35f, 0.3f));
	UE_LOG(LogZoogi, Log, TEXT("HP P%d -%.0f (%s) -> %.0f"), Victim->PlayerIndex + 1, Dmg, Why, Hp);
	if (Hp <= 0.f) HpKnockout(G, Victim, By);
}

void UZoogiFunSys::HpKnockout(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn)
{
	if (!G || !L || !Players.IsValidIndex(L->PlayerIndex)) return;
	{	// 0 HP = KNOCKOUT: blasted off the arena (the ring-out credits the other fighter), back at full HP
		Players[L->PlayerIndex].HP = 100.f;
		if (L->Sphere && !L->bOut)
		{
			FVector Out = L->BallLocation() - (G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector); Out.Z = 0.f;
			if (!Out.Normalize()) Out = FVector::ForwardVector;
			L->Sphere->WakeRigidBody();
			L->Sphere->SetPhysicsLinearVelocity(Out * 2800.f + FVector(0.f, 0.f, 700.f));
			if (Wn) L->LastHitPlayer = Wn->PlayerIndex;
		}
		if (Wn && Players.IsValidIndex(Wn->PlayerIndex))
		{
			Players[Wn->PlayerIndex].ClashWins++;
			FZoogiLedgerEntry E; E.Player = Wn->PlayerIndex; E.Points = ClashPoints; G->Ledger.Add(E);
		}
		AZoogiFX::Stylized(GetWorld(), TEXT("Big_Explosion_v2"), L->BallLocation(), 0.8f);
		Callout(TEXT("KNOCKOUT!"), FString::Printf(TEXT("%s IS OUT OF HP"), *L->Def.Name.ToUpper()), FLinearColor(1.f, 0.3f, 0.25f), 2.f, 1.2f);
		ZoogiHaptics::Rumble(1.2f, 1.f, GetWorld(), TEXT("duel KO"));
		UE_LOG(LogZoogi, Log, TEXT("HP KNOCKOUT P%d (by P%d)"), L->PlayerIndex + 1, Wn ? Wn->PlayerIndex + 1 : 0);
	}
}

void UZoogiFunSys::TickDuelV2(AZoogiGameMode* G, float RealDt)
{
	using namespace ZoogiDuelV2;
	Clash.DAge += RealDt;
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	if (!A || !D) { DuelV2End(G, false); return; }
	switch (Clash.DStage)
	{
	case 0:
		if (Clash.DAge >= VIntro)
		{
			Clash.DStage = 10; Clash.DAge = 0.f; Clash.bDQteDone = false; Clash.DGrade = -1; Clash.DQteAt = VRing;
			Clash.DCpuTapAt = Clash.bHumanAtt ? -1.f : VRing + CpuOffset();
		}
		break;
	case 10: // circle match
		if (Test() && Clash.bHumanAtt && !Clash.bDQteDone && Clash.DAge >= Clash.DQteAt - 0.03f) DuelV2Tap(G);
		if (!Clash.bHumanAtt && !Clash.bDQteDone && Clash.DCpuTapAt >= 0.f && Clash.DAge >= Clash.DCpuTapAt) { Clash.bDQteDone = true; Clash.DGrade = GradeOf(Clash.DAge - Clash.DQteAt); }
		if (!Clash.bDQteDone && Clash.DAge > Clash.DQteAt + 0.36f) { Clash.bDQteDone = true; Clash.DGrade = 0; }
		if (Clash.bDQteDone && Clash.DGrade >= 0 && !Clash.bVGraded)
		{
			Clash.bVGraded = true; Clash.VGradeAt = Clash.DAge;
			ZoogiUtil::PlaySfx(G, Clash.DGrade > 0 ? TEXT("UiConfirm") : TEXT("UiBack"), 0.9f, Clash.DGrade == 2 ? 1.4f : 1.f);
			if (Clash.DGrade > 0)
			{
				const int32 Gain = Clash.DGrade == 2 ? 5 : 3;
				if (UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G)) CS->AddEnergyNow(A->PlayerIndex, Gain);
				G->AddPopup(FString::Printf(TEXT("+%d SP"), Gain), A->BallLocation() + FVector(0, 0, 220.f), FLinearColor(0.75f, 0.45f, 1.f));
			}
			UE_LOG(LogZoogi, Log, TEXT("DUEL2 circle P%d: %s"), A->PlayerIndex + 1, Clash.DGrade == 2 ? TEXT("PERFECT +5 SP") : Clash.DGrade == 1 ? TEXT("GOOD +3 SP") : TEXT("MISS - duel ends"));
			if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_a_circle"));
		}
		if (Clash.bVGraded && Clash.DAge >= Clash.VGradeAt + 0.7f)
		{
			if (Clash.DGrade <= 0) { DuelV2End(G, false); return; }
			Clash.DStage = 11; Clash.DAge = 0.f; Clash.DHover = -1;
			// CPU: the strongest move it can pay for
			const int32 Sp = DuelSp(G, A->PlayerIndex);
			Clash.DCpuWant = Sp >= CostSkill ? 1 : (Sp >= CostAttack ? 0 : 3);
			Clash.DCpuPickAt = FMath::FRandRange(1.4f, 2.4f);
			if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_b_menu"));
		}
		break;
	case 11: // the turn player's move
		if (!Clash.bHumanAtt)
		{
			const int32 Was = Clash.DHover;
			if (Clash.DAge > 0.35f) Clash.DHover = Clash.DAge < Clash.DCpuPickAt - 0.5f ? FMath::FloorToInt((Clash.DAge - 0.35f) / 0.4f) % 4 : Clash.DCpuWant;
			if (Clash.DHover != Was) ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.4f, 1.3f);
			if (Clash.DAge >= Clash.DCpuPickAt) DuelV2Pick(G, Clash.DCpuWant);
		}
		else if (Test() && Clash.DAge > 1.2f) DuelV2Pick(G, DuelSp(G, A->PlayerIndex) >= CostSkill ? 1 : 0);
		else if (Clash.DAge > VMenuMax) DuelV2Pick(G, DuelSp(G, A->PlayerIndex) >= CostSkill ? 1 : (DuelSp(G, A->PlayerIndex) >= CostAttack ? 0 : 3));
		break;
	case 12: // the rival answers with the SP it has
		if (!Clash.bHumanDef)
		{
			const int32 Was = Clash.DHover;
			if (Clash.DAge > 0.3f) Clash.DHover = Clash.DAge < Clash.DCpuPickAt - 0.4f ? FMath::FloorToInt((Clash.DAge - 0.3f) / 0.35f) % 4 : Clash.VRivalWant;
			if (Clash.DHover != Was) ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.4f, 1.2f);
			if (Clash.DAge >= Clash.DCpuPickAt) DuelV2Pick(G, Clash.VRivalWant);
		}
		else if (Clash.DAge > VRivalMax) DuelV2Pick(G, 3);
		break;
	case 13: // the hits land
	{
		const bool bHit = Clash.VPick <= 1;
		if (!Clash.bVLanded && Clash.DAge >= 0.35f)
		{
			Clash.bVLanded = true;
			if (bHit)
			{
				float Dmg = (Clash.VPick == 1 ? DmgSkill : DmgAttack) * (Clash.DGrade == 2 ? 1.2f : 1.f) * (Clash.bVDefend ? 0.5f : 1.f);
				AActor* Fd = Clash.F[1].Actor.Get();
				if (Clash.bField)
				{	// b104: the strike crosses the field from the attacker to the defender
					const FVector Pa = A->BallLocation(), Pd = D->BallLocation();
					const FLinearColor Sc = Clash.VPick == 1 ? FLinearColor(1.f, 0.45f, 0.15f) : ZoogiUtil::PlayerColor(A->PlayerIndex);
					for (int32 k = 1; k <= 8; ++k) AZoogiFX::Spawn(GetWorld(), FMath::Lerp(Pa, Pd, k / 8.f) + FVector(0, 0, 30.f), Sc, 70.f + 10.f * k, 0.25f + 0.03f * k, false, 12.f);
					AZoogiFX::Spawn(GetWorld(), Pd - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), Sc, 260.f, 0.6f, true, 10.f);
					ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.9f, 0.9f);
				}
				AZoogiFabFX::Play(GetWorld(), Clash.VPick == 1 ? TEXT("/Game/StylizedToonVFX3/Effects/NS_Fire_meteors.NS_Fire_meteors") : TEXT("/Game/StylizedToonVFX3/Effects/NS_Sword_crater.NS_Sword_crater"), Fd ? Fd->GetActorLocation() : D->BallLocation(), 0.3f, 2.5f);
				DuelV2Damage(G, D, A, Dmg, Clash.bVDefend ? TEXT("defended hit") : TEXT("hit"));
				if (Clash.bVDefend && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelNoGuardForm"))) DuelV2GuardForm(G, D); // b94: the guard takes shape
				Callout(Clash.bVDefend ? TEXT("DEFENDED!") : (Clash.VPick == 1 ? TEXT("SKILL HIT!") : TEXT("DIRECT HIT!")), FString::Printf(TEXT("%s  -%d HP"), *D->Def.Name.ToUpper(), FMath::RoundToInt(Dmg)), FLinearColor(1.f, 0.6f, 0.25f), 1.2f, 1.f);
			}
		}
		if (Clash.bVCounter && !Clash.bVCountered && Clash.DAge >= 1.2f)
		{
			Clash.bVCountered = true;
			PlayFighter(1, FormSignature(Clash.F[1].Form), 0.9f, false);
			if (Clash.bField) { const FVector Pa = A->BallLocation(), Pd = D->BallLocation(); for (int32 k = 1; k <= 8; ++k) AZoogiFX::Spawn(GetWorld(), FMath::Lerp(Pd, Pa, k / 8.f) + FVector(0, 0, 30.f), FLinearColor(0.4f, 0.85f, 1.f), 70.f + 10.f * k, 0.25f + 0.03f * k, false, 12.f); ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.9f, 1.1f); } // b104
			const float Dmg = DmgCounter * (Clash.VPick == 2 ? 0.5f : 1.f);
			DuelV2Damage(G, A, D, Dmg, TEXT("counter"));
			Callout(TEXT("COUNTER!"), FString::Printf(TEXT("%s HITS BACK  -%d HP"), *D->Def.Name.ToUpper(), FMath::RoundToInt(Dmg)), FLinearColor(0.4f, 0.85f, 1.f), 1.2f, 1.f);
		}
		if (Clash.DAge >= (Clash.bVDefend ? VResolveGuard : VResolve)) { if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_e_result")); DuelV2End(G, true); return; }
		break;
	}
	default: break;
	}
}

void UZoogiFunSys::DrawDuelV2(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U)
{
	using namespace ZoogiDuelV2;
	const AZoogiBall* A = Clash.Att.Get(); const AZoogiBall* D = Clash.Def.Get();
	if (!A || !D) return;
	const bool bPort = Ht > Wd;
	const float Bar = Ht * (bPort ? 0.07f : 0.09f);
	ZoogiFunCanvasRect(C, 0.f, 0.f, Wd, Bar, FLinearColor(0.f, 0.f, 0.f, 0.92f));
	ZoogiFunCanvasRect(C, 0.f, Ht - Bar, Wd, Bar, FLinearColor(0.f, 0.f, 0.f, 0.92f));
	// HP bars: the turn player lower left, the rival upper right
	auto HpBar = [&](const AZoogiBall* B, bool bLow)
	{
		const float BW = FMath::Min(Wd * 0.42f, 520.f * U), BH = 22.f * U;
		const float X = bLow ? 30.f * U : Wd - 30.f * U - BW, Y = bLow ? Ht - Bar - 46.f * U : Bar + 18.f * U;
		const float Hp = DuelHP(B->PlayerIndex), K = FMath::Clamp(Hp / 100.f, 0.f, 1.f);
		ZoogiFunCanvasRect(C, X - 3.f * U, Y - 3.f * U, BW + 6.f * U, BH + 6.f * U, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		ZoogiFunCanvasRect(C, X, Y, BW * K, BH, K > 0.5f ? FLinearColor(0.3f, 0.95f, 0.4f) : (K > 0.25f ? FLinearColor(1.f, 0.8f, 0.2f) : FLinearColor(1.f, 0.25f, 0.2f)));
		ZoogiFunCanvasText(C, FString::Printf(TEXT("P%d %s   HP %d   SP %d"), B->PlayerIndex + 1, *B->Def.Name.ToUpper(), FMath::RoundToInt(Hp), FMath::Min(99, DuelSp(G, B->PlayerIndex))), bLow ? X : X + BW, bLow ? Y + BH + 22.f * U : Y + BH + 22.f * U, 26.f * U, ZoogiUtil::PlayerColor(B->PlayerIndex), bLow ? 0 : 2, 0);
	};
	const bool bNearAtt = Clash.bHumanAtt || !Clash.bHumanDef;
	HpBar(bNearAtt ? A : D, true); HpBar(bNearAtt ? D : A, false);
	if (Clash.DStage == 0)
	{
		const float In = FMath::Clamp(Clash.DAge / 0.3f, 0.f, 1.f);
		ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Ht * 0.42f), Wd * 0.9f * In, 150.f * U, -6.f, 30.f * U, FLinearColor(0.85f, 0.05f, 0.1f, 0.95f));
		ZoogiFunCanvasText(C, TEXT("DUEL!"), Wd * 0.5f, Ht * 0.42f - 14.f * U, 110.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, Clash.bHumanAtt ? TEXT("TAP REACTION WHEN THE RINGS MEET") : FString::Printf(TEXT("%s GOES FIRST"), *A->Def.Name.ToUpper()), Wd * 0.5f, Ht * 0.42f + 50.f * U, 28.f * U, FLinearColor(1.f, 0.85f, 0.3f), 1, 0);
		return;
	}
	if (Clash.DStage == 10)
	{
		const FVector2D Cc(Wd * 0.5f, Ht * 0.45f);
		const float R0 = 80.f * U;
		const float K = FMath::Clamp((Clash.DQteAt - Clash.DAge) / 1.5f, 0.f, 1.f);
		auto RingAt = [&](float R, float Th, const FLinearColor& Col)
		{
			for (int32 k = 0; k < 48; ++k)
			{
				const float a0 = 2.f * PI * k / 48, a1 = 2.f * PI * (k + 1) / 48;
				FCanvasLineItem L(Cc + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * R, Cc + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * R);
				L.LineThickness = Th; L.SetColor(Col); C->DrawItem(L);
			}
		};
		ZoogiFunCanvasText(C, TEXT("CIRCLE MATCH"), Wd * 0.5f, Bar + 50.f * U, 50.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, TEXT("GOOD = +3 SP     PERFECT = +5 SP"), Wd * 0.5f, Bar + 96.f * U, 26.f * U, FLinearColor(0.8f, 0.6f, 1.f), 1, 0);
		if (!Clash.bVGraded)
		{
			RingAt(R0, 8.f * U, FLinearColor(1.f, 1.f, 1.f, 0.95f));
			RingAt(R0 * (1.f + 3.f * K), 10.f * U, K < 0.08f ? FLinearColor(1.f, 0.85f, 0.2f, 1.f) : FLinearColor(1.f, 0.3f, 0.3f, 0.9f));
			ZoogiFunCanvasText(C, Clash.bHumanAtt ? FString(TEXT("TAP REACTION!")) : FString::Printf(TEXT("%s - TIMING..."), *A->Def.Name.ToUpper()), Cc.X, Cc.Y + R0 * (1.f + 3.f * K) + 18.f * U, 40.f * U, FLinearColor(1.f, 0.85f, 0.25f), 1, 0);
		}
		else ZoogiFunCanvasText(C, Clash.DGrade == 2 ? TEXT("PERFECT!  +5 SP") : Clash.DGrade == 1 ? TEXT("GOOD  +3 SP") : TEXT("MISS"), Cc.X, Cc.Y - 30.f * U, 70.f * U, Clash.DGrade == 2 ? FLinearColor(1.f, 0.82f, 0.2f) : (Clash.DGrade == 1 ? FLinearColor(0.8f, 0.6f, 1.f) : FLinearColor(1.f, 0.4f, 0.4f)), 1, 0);
		return;
	}
	if (Clash.DStage == 11 || Clash.DStage == 12)
	{
		const bool bRival = Clash.DStage == 12;
		const AZoogiBall* Who = bRival ? D : A;
		const bool bHuman = bRival ? Clash.bHumanDef : Clash.bHumanAtt;
		for (FBox2D& Bx : DuelBtn) Bx = FBox2D(ForceInit);
		const FString Title = bHuman ? (bRival ? FString::Printf(TEXT("%s ATTACKS - YOUR ANSWER?"), *A->Def.Name.ToUpper()) : FString(TEXT("CHOOSE YOUR MOVE"))) : FString::Printf(TEXT("P%d %s IS CHOOSING..."), Who->PlayerIndex + 1, *Who->Def.Name.ToUpper());
		ZoogiFunCanvasText(C, Title, Wd * 0.5f, Bar + 92.f * U, 42.f * U, bHuman ? FLinearColor::White : ZoogiUtil::PlayerColor(Who->PlayerIndex), 1, 0);
		const int32 Sp = DuelSp(G, Who->PlayerIndex);
		const float S = bPort ? 0.9f : 1.f;
		const float BW = 440.f * U * S, BH = 96.f * U * S;
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 Cost = bRival ? RivalCost(i) : PickCost(i);
			const bool bNoTarget = bRival && Clash.VPick > 1 && (i == 0 || i == 2);
			const bool bOk = Sp >= Cost && !bNoTarget;
			const FVector2D Ctr(Wd - BW * 0.55f - 40.f * U - i * 22.f * U, Ht * (bPort ? 0.5f : 0.34f) + i * (BH + 18.f * U));
			if (!bHuman && Clash.DHover == i) ZoogiFunCanvasQuad(C, Ctr, BW * 1.08f, BH * 1.2f, -4.f, 14.f * U, FLinearColor(1.f, 0.85f, 0.25f, 0.9f));
			ZoogiFunCanvasQuad(C, Ctr + FVector2D(8.f, 8.f) * U, BW, BH, -4.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
			ZoogiFunCanvasQuad(C, Ctr, BW, BH, -4.f, 14.f * U, !bOk ? FLinearColor(0.15f, 0.15f, 0.17f, 0.85f) : (i == (bRival ? 0 : 1) ? FLinearColor(0.85f, 0.05f, 0.12f, 0.97f) : FLinearColor(0.05f, 0.05f, 0.07f, 0.95f)));
			ZoogiFunCanvasText(C, bRival ? RivalName[i] : PickName[i], Ctr.X, Ctr.Y - BH * 0.14f, 46.f * U * S, bOk ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.55f), 1, 0);
			ZoogiFunCanvasText(C, bRival ? RivalSub[i] : PickSub[i], Ctr.X, Ctr.Y + BH * 0.3f, 20.f * U * S, bOk ? FLinearColor(1.f, 0.85f, 0.35f) : FLinearColor(0.5f, 0.5f, 0.55f), 1, 0);
			if (bHuman && bOk) DuelBtn[i] = FBox2D(Ctr - FVector2D(BW, BH) * 0.55f, Ctr + FVector2D(BW, BH) * 0.55f);
		}
		if (bHuman) ZoogiFunCanvasText(C, FString::Printf(TEXT("%d"), FMath::CeilToInt((bRival ? VRivalMax : VMenuMax) - Clash.DAge)), 60.f * U, Bar + 40.f * U, 54.f * U, FLinearColor(1.f, 0.85f, 0.3f), 1, 0);
		return;
	}
	if (Clash.DStage == 13)
		ZoogiFunCanvasText(C, Clash.DMove, Wd * 0.5f, Bar + 60.f * U, 56.f * U, FLinearColor::White, 1, 0);
}
