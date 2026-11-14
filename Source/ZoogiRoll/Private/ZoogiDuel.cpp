// Nov 2 b66 (Evan): the DUEL - "when it becomes time for that marble to be hit anime cutscene set in up to 30 secs, players enters a
// rpg style stance (turn player lower left near sight, opposing mid to top far sight), the initial action options appear if I haven't
// pressed reaction up to this point which would have triggered the skill; I use a skill and an animation sequence begins and a
// reaction timer and button appear to tap reaction at the time to do the skill add-on, phone vibrates for 5 sec, response anime".
// Built on the power clash (fighters, finisher, loser flung + crash clip). Stages: 0 intro, 1 action menu, 2 move + timing ring,
// 3 the rival's response, then FinishClash / EndClash as before. -ZoogiDuelTest picks SKILL and taps on time automatically.
#include "ZoogiActionFun.h"
#include "ZoogiFilm.h"
#include "ZoogiEpic.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiFX.h"
#include "ZoogiFabFX.h"
#include "ZoogiSkills.h"
#include "ZoogiHaptics.h"
#include "ZoogiBanter.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiCPUBrain.h"
#include "ZoogiProgression.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);

namespace
{
	constexpr float DuelIntro = 1.8f, DuelMenuMax = 10.f, DuelMove = 3.4f, DuelResponse = 2.4f;
	bool DuelTestOn() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelTest")); return b; }
	// b67: how far off a CPU's timing tap lands (by tier) - a person misses sometimes
	float DuelCpuTapOffset()
	{
		const EZoogiCPUTier T = ZoogiCPU::ActiveTier();
		const float Spread = T == EZoogiCPUTier::Easy ? 0.34f : T == EZoogiCPUTier::Normal ? 0.22f : T == EZoogiCPUTier::Hard ? 0.13f : 0.08f;
		return FMath::FRandRange(-Spread, Spread);
	}
	int32 DuelGradeOf(float Off) { const float D = FMath::Abs(Off); return D <= 0.13f ? 2 : (D <= 0.28f ? 1 : 0); }
	const TCHAR* DuelGradeName(int32 G) { return G == 2 ? TEXT("PERFECT!") : G == 1 ? TEXT("GOOD") : TEXT("MISS"); }
	const TCHAR* DuelChoiceNames[4] = { TEXT("ATTACK"), TEXT("SKILL"), TEXT("GUARD"), TEXT("ITEM") };
	const TCHAR* DuelChoiceSubs[4] = { TEXT("Signature strike"), TEXT("Cast a skill from your hand"), TEXT("Brace - lose less"), TEXT("+25 SUPER") };
	FName DuelFirstSkill(AZoogiGameMode* G, int32 Player, FString& Name)
	{
		const UZoogiCardSubsystem* CS = G ? UZoogiCardSubsystem::Get(G) : nullptr;
		const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(Player) : nullptr;
		if (PS)
			for (const auto& H : PS->Hand)
				if (ZoogiSkills::IsSkill(H.CardId))
				{
					const FZoogiCardDef* D = CS->Def(H.CardId);
					Name = D ? D->Name.ToUpper() : H.CardId.ToString().ToUpper();
					return H.CardId;
				}
		return NAME_None;
	}
	const TCHAR* DuelSkillFx(FName Id)
	{
		const FString S = Id.ToString();
		if (S.Contains(TEXT("water"))) return TEXT("/Game/StylizedToonVFX3/Effects/NS_Water_hit.NS_Water_hit");
		if (S.Contains(TEXT("ray"))) return TEXT("/Game/StylizedToonVFX3/Effects/NS_Magic_rays_attack.NS_Magic_rays_attack");
		if (S.Contains(TEXT("sword"))) return TEXT("/Game/StylizedToonVFX3/Effects/NS_Sword_crater.NS_Sword_crater");
		if (S.Contains(TEXT("shock")) || S.Contains(TEXT("energy"))) return TEXT("/Game/StylizedToonVFX3/Effects/NS_Energy_shockwave.NS_Energy_shockwave");
		if (S.Contains(TEXT("apple"))) return TEXT("/Game/StylizedToonVFX3/Effects/NS_Apple_bombs.NS_Apple_bombs");
		return TEXT("/Game/StylizedToonVFX3/Effects/NS_Fire_meteors.NS_Fire_meteors");
	}
}

namespace ZoogiDuelV2 { bool On(); }

void UZoogiFunSys::StartDuel(AZoogiGameMode* G, AZoogiBall* Att, AZoogiBall* Def)
{
	if (!G || !Att || !Def || Clash.bOn) return;
	if (UZoogiAnimeSubsystem* An0 = G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) if (An0->CrashOn()) { UE_LOG(LogZoogi, Log, TEXT("DUEL skipped (a clip is playing)")); return; }
	StartClash(G, Att, Def);
	if (!Clash.bOn) return;
	if (ZoogiFilm::On() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiFilmDuels"))) ZoogiFilm::Begin(TEXT("duel"), 16.f); // b101
	ZoogiEpic::Hit(G, ZoogiEpic::EHit::Riser); // b103: a riser into the duel
	Clash.bDuel = true; Clash.DStage = 0; Clash.DAge = 0.f; Clash.bGo = false;
	Clash.bV2 = ZoogiDuelV2::On(); // b90: circle match -> SP -> move -> rival SP answer -> HP
	PlayFighter(0, TEXT("idle"), 1.f, true);
	PlayFighter(1, TEXT("idle"), 1.f, true);
	ZoogiUtil::PlaySfx(G, TEXT("Clack"), 1.f, 0.7f);
	ZoogiBanter::Exchange(G, Att->PlayerIndex, Def->PlayerIndex, ZoogiBanter::EKind::DuelOpen, ZoogiBanter::EKind::DuelReply, true); // b67
	UE_LOG(LogZoogi, Log, TEXT("DUEL start P%d %s vs P%d %s"), Att->PlayerIndex + 1, *Att->Def.Name, Def->PlayerIndex + 1, *Def->Def.Name);
	if (G->ShotDir.Len()) G->TakeShot(TEXT("duel_a_intro"));
}

void UZoogiFunSys::DuelChoose(AZoogiGameMode* G, int32 Choice)
{
	if (!Clash.bDuel || Clash.DStage > 1) return;
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	Clash.DChoice = FMath::Clamp(Choice, 0, 3);
	Clash.DStage = 2; Clash.DAge = 0.f; Clash.bDQteDone = false; Clash.DGrade = -1;
	Clash.DQteAt = 1.7f;
	Clash.DAttG = -1; Clash.DDefG = -1;
	Clash.DCpuTapAt = Clash.bHumanAtt ? -1.f : Clash.DQteAt + DuelCpuTapOffset();  // b67: the CPU taps too
	Clash.DDefTapAt = Clash.bHumanDef ? -1.f : Clash.DQteAt + DuelCpuTapOffset();
	const int32 Fa = Clash.F[0].Form;
	FString Sk; FName SkId = NAME_None;
	if (Clash.DChoice == 1 && A) SkId = DuelFirstSkill(G, A->PlayerIndex, Sk);
	switch (Clash.DChoice)
	{
	case 0: Clash.DMove = TEXT("ATTACK"); Clash.DMoveSub = TEXT("SIGNATURE STRIKE"); PlayFighter(0, FormSignature(Fa), 0.8f, false); break;
	case 1: Clash.DMove = SkId != NAME_None ? Sk : FString(TEXT("SPECIAL")); Clash.DMoveSub = TEXT("SKILL"); PlayFighter(0, FormSignature(Fa), 0.7f, false); break;
	case 2: Clash.DMove = TEXT("GUARD"); Clash.DMoveSub = TEXT("BRACE FOR THE HIT"); PlayFighter(0, TEXT("idle"), 1.f, true); Clash.bDQteDone = true; Clash.DGrade = 1; break;
	default: Clash.DMove = TEXT("ITEM"); Clash.DMoveSub = TEXT("+25 SUPER"); if (A) AddMeter(G, A->PlayerIndex, 25.f, TEXT("duel item")); PlayFighter(0, TEXT("box_02"), 0.8f, false); break;
	}
	// the move lands on the rival's form
	UWorld* W = GetWorld();
	if (D && Clash.DChoice != 2)
	{
		AActor* Fd = Clash.F[1].Actor.Get();
		const FVector P = Fd ? Fd->GetActorLocation() : D->BallLocation();
		AZoogiFabFX::Play(W, Clash.DChoice == 1 ? DuelSkillFx(SkId) : TEXT("/Game/StylizedToonVFX3/Effects/NS_Sword_crater.NS_Sword_crater"), P, 0.3f, 2.5f); // b71 (Grok): 0.12 -> 0.3
		if (SkId == FName(TEXT("bolt_balls"))) AZoogiFX::Stylized(W, TEXT("ElectricShock"), P + FVector(0, 0, 60.f), 1.3f);
	}
	ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.9f, 0.8f);
	UE_LOG(LogZoogi, Log, TEXT("DUEL choice %s (%s)"), DuelChoiceNames[Clash.DChoice], *Clash.DMove);
	if (G->ShotDir.Len()) G->TakeShot(TEXT("duel_c_move"));
}

void UZoogiFunSys::DuelTap(AZoogiGameMode* G)
{
	if (!Clash.bDuel) return;
	if (Clash.bV2) { DuelV2Tap(G); return; } // b90
	if (Clash.DStage == 0) { Clash.bDPreReact = true; return; } // REACTION before the menu = straight to the skill
	if (Clash.DStage == 1) { if (Clash.bHumanAtt) DuelChoose(G, 1); return; }
	if (Clash.DStage != 2 || Clash.bDQteDone) return;
	if (!(Clash.bHumanAtt || Clash.bHumanDef)) return;
	Clash.bDQteDone = true;
	const float Dt = FMath::Abs(Clash.DAge - Clash.DQteAt);
	Clash.DGrade = Dt <= 0.13f ? 2 : (Dt <= 0.28f ? 1 : 0);
	UE_LOG(LogZoogi, Log, TEXT("DUEL timing tap %+.2f s -> %s"), Clash.DAge - Clash.DQteAt, Clash.DGrade == 2 ? TEXT("PERFECT") : Clash.DGrade == 1 ? TEXT("GOOD") : TEXT("MISS"));
}

void UZoogiFunSys::TickDuel(AZoogiGameMode* G, float RealDt)
{
	if (Clash.bV2) { TickDuelV2(G, RealDt); return; } // b90
	Clash.DAge += RealDt;
	Clash.Ratio = FMath::FInterpTo(Clash.Ratio, 0.5f, RealDt, 4.f);
	AZoogiBall* A = Clash.Att.Get(); AZoogiBall* D = Clash.Def.Get();
	UWorld* W = GetWorld();
	switch (Clash.DStage)
	{
	case 0:
		if (Clash.DAge >= DuelIntro)
		{
			Clash.DStage = 1; Clash.DAge = 0.f;
			Clash.DHover = -1;
			{	// b67: what the CPU will pick, and when (after a look at the menu)
				FString Sk0;
				Clash.DCpuWant = (A && DuelFirstSkill(G, A->PlayerIndex, Sk0) != NAME_None && FMath::FRand() < 0.75f) ? 1 : (FMath::FRand() < 0.2f ? 2 : 0);
				Clash.DCpuPickAt = FMath::FRandRange(1.8f, 2.8f);
			}
			if (G->ShotDir.Len()) G->TakeShot(TEXT("duel_b_menu"));
			if (Clash.bDPreReact) DuelChoose(G, 1);
		}
		break;
	case 1:
		if (!Clash.bHumanAtt)
		{	// b67: the CPU's cursor looks over the menu, settles on its choice, confirms
			const int32 Was = Clash.DHover;
			if (Clash.DAge > 0.35f) Clash.DHover = Clash.DAge < Clash.DCpuPickAt - 0.6f ? FMath::FloorToInt((Clash.DAge - 0.35f) / 0.42f) % 3 : Clash.DCpuWant;
			if (Clash.DHover != Was) ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.4f, 1.3f);
			if (Clash.DAge >= Clash.DCpuPickAt) { ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.2f); DuelChoose(G, Clash.DCpuWant); }
		}
		else if (Clash.bHumanAtt && DuelTestOn() && Clash.DAge > 1.2f) DuelChoose(G, 1);
		else if (Clash.DAge > DuelMenuMax) DuelChoose(G, 0);
		break;
	case 2:
	{
		if (DuelTestOn() && !Clash.bDQteDone && Clash.DAge >= Clash.DQteAt - 0.03f) DuelTap(G);
		// b67: the CPU side(s) tap the timing ring too
		if (Clash.DChoice != 2 && Clash.DAttG < 0 && Clash.DCpuTapAt >= 0.f && Clash.DAge >= Clash.DCpuTapAt) { Clash.DAttG = DuelGradeOf(Clash.DAge - Clash.DQteAt); ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.8f, Clash.DAttG == 2 ? 1.4f : 1.f); UE_LOG(LogZoogi, Log, TEXT("DUEL cpu attacker tap %+.2f -> %s"), Clash.DAge - Clash.DQteAt, DuelGradeName(Clash.DAttG)); }
		if (Clash.DChoice != 2 && Clash.DDefG < 0 && Clash.DDefTapAt >= 0.f && Clash.DAge >= Clash.DDefTapAt) { Clash.DDefG = DuelGradeOf(Clash.DAge - Clash.DQteAt); ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.7f, Clash.DDefG == 2 ? 1.3f : 0.9f); UE_LOG(LogZoogi, Log, TEXT("DUEL cpu defender tap %+.2f -> %s"), Clash.DAge - Clash.DQteAt, DuelGradeName(Clash.DDefG)); }
		if (!Clash.bHumanAtt && !Clash.bHumanDef && !Clash.bDQteDone && Clash.DAttG >= 0) { Clash.bDQteDone = true; Clash.DGrade = Clash.DAttG; }
		if (!Clash.bDQteDone && Clash.DAge > Clash.DQteAt + 0.36f)
		{
			Clash.bDQteDone = true;
			Clash.DGrade = (Clash.bHumanAtt || Clash.bHumanDef) ? 0 : FMath::Max(0, Clash.DAttG);
		}
		static bool bLanded = false;
		if (Clash.DAge < 0.05f) bLanded = false;
		if (!bLanded && Clash.DAge >= Clash.DQteAt + 0.4f)
		{
			bLanded = true;
			// the move lands: attacker's push by grade (GUARD = small, safe); a human defender's grade blunts it instead
			float Push = Clash.DChoice == 2 ? 0.15f : 0.35f;
			if (Clash.bHumanDef && !Clash.bHumanAtt) Push = 0.45f - 0.25f * Clash.DGrade + 0.1f * FMath::Max(0, Clash.DAttG) + FMath::FRandRange(-0.05f, 0.05f); // b67: the CPU attacker's tap counts
			else { Push += Clash.DGrade == 2 ? 0.5f : (Clash.DGrade == 1 ? 0.25f : 0.f); if (!Clash.bHumanDef) Push -= 0.12f * FMath::Max(0, Clash.DDefG); } // b67: a CPU defender's guard tap blunts it
			// b67 (Evan: "If opponent counters successfully a new anime plays and clash begins"): a PERFECT guard tap by the rival
			static const bool bNoCounter = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDuelCounter"));
			const bool bCounter = !bNoCounter && Clash.DChoice != 2 && ((Clash.bHumanDef && !Clash.bHumanAtt && Clash.DGrade == 2) || (!Clash.bHumanDef && Clash.DDefG == 2));
			if (bCounter && D && A)
			{
				Clash.bDuel = false; Clash.bDCounter = true; Clash.Age = 0.f; Clash.bGo = false; Clash.Bar = 0.f; Clash.TapsA = Clash.TapsD = 0;
				PlayFighter(1, FormSignature(Clash.F[1].Form), 0.9f, false);
				PlayFighter(0, TEXT("hit_to_body_01"), 1.f, false);
				Callout(TEXT("COUNTER!"), FString::Printf(TEXT("%s TURNS IT AROUND - CLASH!"), *D->Def.Name.ToUpper()), FLinearColor(0.4f, 0.9f, 1.f), 1.8f, 1.f);
				if (UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>())
				{
					FZoogiCombatEvent E; E.Event = FName(TEXT("Critical")); E.Player = D->PlayerIndex; E.TargetPlayer = A->PlayerIndex;
					E.ZoogiName = D->Def.Name; E.Location = D->BallLocation(); E.Magnitude = 1.f;
					An->Emit(E); // the counter's own anime, then the tap clash
				}
				ZoogiHaptics::Rumble(1.5f, 1.f, W, TEXT("duel counter"));
				ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 1.f, 1.1f);
				UE_LOG(LogZoogi, Log, TEXT("DUEL COUNTER by P%d -> anime, then a classic clash"), D->PlayerIndex + 1);
				if (G->ShotDir.Len()) G->TakeShot(TEXT("duel_f_counter"));
				break;
			}
			Clash.Bar = FMath::Clamp(Clash.Bar + Push, -1.f, 1.f);
			Clash.Hit = 1.6f;
			if (Clash.DChoice != 2) PlayFighter(1, TEXT("hit_to_body_01"), 1.f, false);
			const bool bHumanIn = Clash.bHumanAtt || Clash.bHumanDef;
			if (bHumanIn) ZoogiHaptics::Rumble(5.f, Clash.DGrade == 2 ? 1.f : 0.7f, W, TEXT("duel move"));
			Callout(Clash.DGrade == 2 ? (Clash.bHumanDef && !Clash.bHumanAtt ? TEXT("PERFECT GUARD!") : TEXT("ADD-ON!")) : Clash.DGrade == 1 ? TEXT("GOOD!") : TEXT("MISSED THE TIMING"),
				FString::Printf(TEXT("%s  %s"), *Clash.DMoveSub, *Clash.DMove), Clash.DGrade == 2 ? FLinearColor(1.f, 0.82f, 0.2f) : FLinearColor(0.7f, 0.85f, 1.f), 1.4f, 1.f);
			ZoogiUtil::PlaySfx(G, TEXT("HitClick"), 1.f, Clash.DGrade == 2 ? 1.3f : 1.f);
			UE_LOG(LogZoogi, Log, TEXT("DUEL move lands: grade %d push %+.2f bar %+.2f"), Clash.DGrade, Push, Clash.Bar);
			if (G->ShotDir.Len()) G->TakeShot(TEXT("duel_d_land"));
		}
		if (Clash.DAge >= DuelMove)
		{
			Clash.DStage = 3; Clash.DAge = 0.f;
			// the rival answers (response anime)
			PlayFighter(1, FormSignature(Clash.F[1].Form), 0.9f, false);
			PlayFighter(0, TEXT("idle"), 1.f, true);
			const float Back = Clash.bHumanDef ? 0.2f + 0.15f * FMath::Max(0, Clash.DGrade) : FMath::FRandRange(0.1f, 0.4f) * (Clash.DChoice == 2 ? 0.5f : 1.f);
			Clash.Bar = FMath::Clamp(Clash.Bar - Back, -1.f, 1.f);
			if (UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>())
			{
				FZoogiCombatEvent E; E.Event = FName(TEXT("Combo")); E.Player = D ? D->PlayerIndex : -1; E.TargetPlayer = A ? A->PlayerIndex : -1;
				E.ZoogiName = D ? D->Def.Name : FString(); E.Location = D ? D->BallLocation() : FVector::ZeroVector; E.Magnitude = 1.f;
				An->Emit(E); // response anime cut-in (triangle)
			}
			Callout(TEXT("RESPONSE!"), D ? FString::Printf(TEXT("%s ANSWERS"), *D->Def.Name.ToUpper()) : FString(), FLinearColor(0.4f, 0.85f, 1.f), 1.6f, 0.9f);
			UE_LOG(LogZoogi, Log, TEXT("DUEL response %.2f -> bar %+.2f"), Back, Clash.Bar);
			if (G->ShotDir.Len()) G->TakeShot(TEXT("duel_e_response"));
		}
		break;
	}
	case 3:
		if (Clash.DAge >= DuelResponse) { Clash.DStage = 4; FinishClash(G); }
		break;
	default: break;
	}
}

void UZoogiFunSys::DrawDuel(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U)
{
	if (Clash.bV2) { DrawDuelV2(C, G, Wd, Ht, U); return; } // b90
	const AZoogiBall* A = Clash.Att.Get(); const AZoogiBall* D = Clash.Def.Get();
	const float Rt = GetWorld()->GetRealTimeSeconds();
	const bool bPort = Ht > Wd;
	// letterbox: it is a movie now
	const float Bar = Ht * (bPort ? 0.07f : 0.09f);
	ZoogiFunCanvasRect(C, 0.f, 0.f, Wd, Bar, FLinearColor(0.f, 0.f, 0.f, 0.92f));
	ZoogiFunCanvasRect(C, 0.f, Ht - Bar, Wd, Bar, FLinearColor(0.f, 0.f, 0.f, 0.92f));
	// names: near (lower left) vs far (upper right)
	const bool bNearAtt = Clash.bHumanAtt || !Clash.bHumanDef;
	const AZoogiBall* Near = bNearAtt ? A : D; const AZoogiBall* Far = bNearAtt ? D : A;
	if (Near) ZoogiFunCanvasText(C, FString::Printf(TEXT("P%d %s"), Near->PlayerIndex + 1, *Near->Def.Name.ToUpper()), 30.f * U, Ht - Bar * 0.5f, 34.f * U, ZoogiUtil::PlayerColor(Near->PlayerIndex), 0, 0);
	if (Far) ZoogiFunCanvasText(C, FString::Printf(TEXT("P%d %s"), Far->PlayerIndex + 1, *Far->Def.Name.ToUpper()), Wd - 30.f * U, Bar * 0.5f, 34.f * U, ZoogiUtil::PlayerColor(Far->PlayerIndex), 2, 0);
	// tug meter (the result): attacker side fills from the left
	{
		const float BW = FMath::Min(Wd * 0.7f, 900.f * U), BX = (Wd - BW) * 0.5f, BY = Ht - Bar - 40.f * U;
		ZoogiFunCanvasRect(C, BX - 4.f * U, BY - 4.f * U, BW + 8.f * U, 22.f * U, FLinearColor(0.f, 0.f, 0.f, 0.8f));
		const float K = FMath::Clamp(0.5f + 0.5f * Clash.Bar, 0.f, 1.f);
		ZoogiFunCanvasRect(C, BX, BY, BW * K, 14.f * U, A ? ZoogiUtil::PlayerColor(A->PlayerIndex) : FLinearColor::White);
		ZoogiFunCanvasRect(C, BX + BW * K, BY, BW * (1.f - K), 14.f * U, D ? ZoogiUtil::PlayerColor(D->PlayerIndex) : FLinearColor::Gray);
	}
	if (Clash.bFinish) return;
	if (Clash.DStage == 0)
	{
		const float In = FMath::Clamp(Clash.DAge / 0.3f, 0.f, 1.f);
		ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Ht * 0.42f), Wd * 0.9f * In, 150.f * U, -6.f, 30.f * U, FLinearColor(0.85f, 0.05f, 0.1f, 0.95f));
		ZoogiFunCanvasText(C, TEXT("DUEL!"), Wd * 0.5f, Ht * 0.42f - 14.f * U, 110.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, Clash.bHumanAtt ? TEXT("TAP REACTION NOW TO JUMP STRAIGHT TO YOUR SKILL") : TEXT("GET READY TO GUARD"), Wd * 0.5f, Ht * 0.42f + 50.f * U, 28.f * U, FLinearColor(1.f, 0.85f, 0.3f), 1, 0);
		return;
	}
	if (Clash.DStage == 1)
	{
		for (FBox2D& Bx : DuelBtn) Bx = FBox2D(ForceInit);
		const bool bCpuMenu = !Clash.bHumanAtt; // b67: the CPU goes through the same menu (its cursor highlights)
		ZoogiFunCanvasText(C, bCpuMenu ? (A ? FString::Printf(TEXT("P%d %s IS CHOOSING..."), A->PlayerIndex + 1, *A->Def.Name.ToUpper()) : FString()) : FString(TEXT("CHOOSE YOUR MOVE")), Wd * 0.5f, Bar + 40.f * U, 44.f * U, bCpuMenu && A ? ZoogiUtil::PlayerColor(A->PlayerIndex) : FLinearColor::White, 1, 0);
		const float S = bPort ? 0.9f : 1.f;
		const float BW = 420.f * U * S, BH = 96.f * U * S;
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector2D Ctr(Wd - BW * 0.55f - 40.f * U - i * 22.f * U, Ht * (bPort ? 0.52f : 0.36f) + i * (BH + 18.f * U));
			const bool bSkill = bCpuMenu ? Clash.DHover == i : i == 1;
			if (bCpuMenu && Clash.DHover == i) ZoogiFunCanvasQuad(C, Ctr, BW * 1.08f, BH * 1.2f, -4.f, 14.f * U, FLinearColor(1.f, 0.85f, 0.25f, 0.9f));
			ZoogiFunCanvasQuad(C, Ctr + FVector2D(8.f, 8.f) * U, BW, BH, -4.f, 14.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
			ZoogiFunCanvasQuad(C, Ctr, BW, BH, -4.f, 14.f * U, bSkill ? FLinearColor(0.85f, 0.05f, 0.12f, 0.97f) : FLinearColor(0.05f, 0.05f, 0.07f, 0.95f));
			ZoogiFunCanvasText(C, DuelChoiceNames[i], Ctr.X, Ctr.Y - BH * 0.14f, 46.f * U * S, FLinearColor::White, 1, 0);
			ZoogiFunCanvasText(C, DuelChoiceSubs[i], Ctr.X, Ctr.Y + BH * 0.3f, 20.f * U * S, FLinearColor(1.f, 0.85f, 0.35f), 1, 0);
			if (!bCpuMenu) DuelBtn[i] = FBox2D(Ctr - FVector2D(BW, BH) * 0.55f, Ctr + FVector2D(BW, BH) * 0.55f);
		}
		if (!bCpuMenu) ZoogiFunCanvasText(C, FString::Printf(TEXT("%d"), FMath::CeilToInt(DuelMenuMax - Clash.DAge)), 60.f * U, Bar + 40.f * U, 54.f * U, FLinearColor(1.f, 0.85f, 0.3f), 1, 0);
		return;
	}
	if (Clash.DStage == 2)
	{
		ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Bar + 70.f * U), FMath::Min(Wd * 0.9f, 900.f * U), 100.f * U, -3.f, 16.f * U, FLinearColor(0.f, 0.f, 0.f, 0.88f));
		ZoogiFunCanvasText(C, Clash.DMove, Wd * 0.5f, Bar + 58.f * U, 54.f * U, FLinearColor::White, 1, 0);
		ZoogiFunCanvasText(C, Clash.DMoveSub, Wd * 0.5f, Bar + 98.f * U, 22.f * U, FLinearColor(1.f, 0.85f, 0.3f), 1, 0);
		if (Clash.DChoice != 2) // b67: the ring shows for CPU duels too (the CPU taps it)
		{
			const bool bHumanTaps = Clash.bHumanAtt || Clash.bHumanDef;
			// the timing ring closes on the target circle at DQteAt: tap REACTION when they meet
			const FVector2D Cc = Clash.HitScreen.X >= 0.f ? Clash.HitScreen : FVector2D(Wd * 0.62f, Ht * 0.4f);
			const float R0 = 70.f * U;
			const float K = FMath::Clamp((Clash.DQteAt - Clash.DAge) / 1.5f, 0.f, 1.f);
			const int32 Seg = 48;
			auto RingAt = [&](float R, float Th, const FLinearColor& Col)
			{
				for (int32 k = 0; k < Seg; ++k)
				{
					const float a0 = 2.f * PI * k / Seg, a1 = 2.f * PI * (k + 1) / Seg;
					FCanvasLineItem L(Cc + FVector2D(FMath::Cos(a0), FMath::Sin(a0)) * R, Cc + FVector2D(FMath::Cos(a1), FMath::Sin(a1)) * R);
					L.LineThickness = Th; L.SetColor(Col); C->DrawItem(L);
				}
			};
			const bool bCpuOpen = !bHumanTaps && Clash.DAttG < 0 && Clash.DAge < Clash.DQteAt + 0.36f;
			if (bHumanTaps ? !Clash.bDQteDone : bCpuOpen)
			{
				RingAt(R0, 8.f * U, FLinearColor(1.f, 1.f, 1.f, 0.95f));
				RingAt(R0 * (1.f + 3.f * K), 10.f * U, K < 0.08f ? FLinearColor(1.f, 0.85f, 0.2f, 1.f) : FLinearColor(1.f, 0.3f, 0.3f, 0.9f));
				ZoogiFunCanvasText(C, bHumanTaps ? FString(TEXT("TAP REACTION!")) : (A ? FString::Printf(TEXT("%s - TIMING..."), *A->Def.Name.ToUpper()) : FString()), Cc.X, Cc.Y + R0 * (1.f + 3.f * K) + 16.f * U, 40.f * U, FLinearColor(1.f, 0.85f, 0.25f), 1, 0);
			}
			else if (Clash.DGrade >= 0)
				ZoogiFunCanvasText(C, Clash.DGrade == 2 ? TEXT("PERFECT!") : Clash.DGrade == 1 ? TEXT("GOOD") : TEXT("MISS"), Cc.X, Cc.Y - 30.f * U, 64.f * U, Clash.DGrade == 2 ? FLinearColor(1.f, 0.82f, 0.2f) : FLinearColor::White, 1, 0);
			// b67: the CPU's own taps (attacker / defender) read out under the ring
			if (bHumanTaps)
			{
				const AZoogiBall* Cpu = Clash.bHumanAtt ? D : A; const int32 Cg = Clash.bHumanAtt ? Clash.DDefG : Clash.DAttG;
				if (Cpu && Cg >= 0) ZoogiFunCanvasText(C, FString::Printf(TEXT("%s: %s"), *Cpu->Def.Name.ToUpper(), DuelGradeName(Cg)), Cc.X, Cc.Y + R0 + 70.f * U, 32.f * U, ZoogiUtil::PlayerColor(Cpu->PlayerIndex), 1, 0);
			}
			else if (Clash.DDefG >= 0 && D) ZoogiFunCanvasText(C, FString::Printf(TEXT("%s: %s"), *D->Def.Name.ToUpper(), DuelGradeName(Clash.DDefG)), Cc.X, Cc.Y + R0 + 70.f * U, 32.f * U, ZoogiUtil::PlayerColor(D->PlayerIndex), 1, 0);
		}
	}
}
