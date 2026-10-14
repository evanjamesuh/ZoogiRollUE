#include "ZoogiMatchValidate.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiEpisodeLedger.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiReactionComponent.h"
#include "ZoogiEpisodeDirector.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/NameAsStringProxyArchive.h"

namespace
{
	template <class E> void SerE(FArchive& Ar, E& V) { uint8 B = (uint8)V; Ar << B; if (Ar.IsLoading()) V = (E)B; }
	void SerF(FArchive& Ar, float& V, bool bHash, float Q = 10.f)
	{
		if (bHash) { int32 I = FMath::RoundToInt(V * Q); Ar << I; return; }
		Ar << V;
	}
	void SerV(FArchive& Ar, FVector& V, bool bHash, float Q = 10.f)
	{
		if (bHash) { for (int32 k = 0; k < 3; ++k) { int32 I = FMath::RoundToInt(V[k] * Q); Ar << I; } return; }
		Ar << V;
	}
	template <class T> void SerStruct(FArchive& Ar, T& S) { T::StaticStruct()->SerializeBin(Ar, &S); }
	template <class T> void SerStructs(FArchive& Ar, TArray<T>& A)
	{
		int32 N = A.Num(); Ar << N; if (Ar.IsLoading()) A.SetNum(N);
		for (T& S : A) SerStruct(Ar, S);
	}
	void SerBody(FArchive& Ar, USphereComponent* Sp, bool bHash)
	{
		FVector L = Sp ? Sp->GetComponentLocation() : FVector::ZeroVector;
		FRotator R = Sp ? Sp->GetComponentRotation() : FRotator::ZeroRotator;
		const bool bSim = Sp && Sp->IsSimulatingPhysics();
		FVector V = bSim ? Sp->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		FVector W = bSim ? Sp->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;
		SerV(Ar, L, bHash);
		if (!bHash) Ar << R;           // rotation is presentation for a sphere; not part of the hash
		SerV(Ar, V, bHash, 1.f);
		if (!bHash) Ar << W;
		bool bAwake = bSim && Sp->RigidBodyIsAwake();
		if (!bHash) Ar << bAwake;       // sleep state: a resumed body that was asleep goes back to sleep
		if (Ar.IsLoading() && Sp)
		{
			// a body already in the saved state is left untouched (a no-op write would still wake it and drop its contacts)
			const bool bSame = Sp->GetComponentLocation().Equals(L, 0.f) && Sp->GetComponentRotation().Equals(R, 0.f)
				&& (!Sp->IsSimulatingPhysics() || (Sp->GetPhysicsLinearVelocity().Equals(V, 0.f) && Sp->GetPhysicsAngularVelocityInDegrees().Equals(W, 0.f)));
			if (!bSame)
			{
				Sp->SetWorldLocationAndRotation(L, R, false, nullptr, ETeleportType::TeleportPhysics);
				if (Sp->IsSimulatingPhysics()) { Sp->SetPhysicsLinearVelocity(V); Sp->SetPhysicsAngularVelocityInDegrees(W); }
			}
			if (Sp->IsSimulatingPhysics() && !bAwake && Sp->RigidBodyIsAwake()) Sp->PutRigidBodyToSleep();
		}
	}
}

void FZoogiSaveAccess::Visit(FArchive& Ar, AZoogiGameMode* G, bool bHash)
{
	FString Ver = Version(); Ar << Ver;
	if (Ar.IsLoading() && Ver != Version()) { Ar.SetError(); return; }
	// ---- game flow
	SerE(Ar, G->Phase); SerE(Ar, G->Stage); SerE(Ar, G->BattleStep);
	Ar << G->ActivePlayer << G->TurnCounter << G->RoundNumber << G->WinnerIndex;
	if (!bHash) Ar << G->StageTime << G->StepTime << G->PhaseTime << G->SettleTimer << G->AwakenDoneAt << G->FollowUpOfferAt;
	Ar << G->bAwakenDone << G->bStandbyAwaken << G->AwakenChoiceText << G->AimPowerStep;
	Ar.Serialize(&G->GameRng, sizeof(FRandomStream));
	SerE(Ar, G->ComboForce); SerE(Ar, G->ComboShotForce);
	for (int32 t = 0; t < 2; ++t)
	{
		Ar << G->ComboNextPick[t] << G->ComboResonance[t] << G->ComboOrbOuts[t];
		SerF(Ar, G->ComboEnergy[t], bHash); SerF(Ar, G->ArenaControl[t], bHash);
	}
	Ar << G->bFollowUpUsed << G->bFollowUpOffered << G->bInFollowUp << G->bFollowUpBroken << G->ShotTriggers << G->OfferTriggers << G->bComboMatchOver << G->ComboWinner << G->ComboOverReason;
	// ---- bodies
	int32 NB = G->Balls.Num(); Ar << NB;
	if (Ar.IsLoading() && NB != G->Balls.Num()) { Ar.SetError(); return; }
	for (AZoogiBall* B : G->Balls)
	{
		if (!B) continue;
		const bool bOutBefore = B->bOut;
		Ar << B->Score << B->bOut << B->LastHitPlayer << B->KnockOuts << B->RingMarks << B->bKnockedOut << B->bPendingRecovery << B->bEdgeSave;
		SerF(Ar, B->AimYaw, bHash); SerF(Ar, B->RollingDecel, bHash);
		if (Ar.IsLoading() && bOutBefore != B->bOut) UE_LOG(LogZoogi, Warning, TEXT("SAVE restore: P%d out-state changed %d -> %d (body visibility not re-run)"), B->PlayerIndex + 1, bOutBefore, B->bOut);
		SerBody(Ar, B->Sphere, bHash);
	}
	int32 NO = G->Orbs.Num(); Ar << NO;
	if (Ar.IsLoading() && NO != G->Orbs.Num()) { Ar.SetError(); return; }
	for (AZoogiOrb* O : G->Orbs) { if (!O) continue; Ar << O->bOut << O->LastHitBy; SerBody(Ar, O->Sphere, bHash); }
	if (G->Arena)
	{
		const auto& Pieces = G->Arena->GetPieces();
		int32 NP = Pieces.Num(); Ar << NP;
		for (int32 i = 0; i < NP && i < Pieces.Num(); ++i)
		{
			AZoogiArenaPiece* Pc = Pieces[i];
			EZoogiPieceState S = Pc ? Pc->GetPieceState() : EZoogiPieceState();
			SerE(Ar, S);
			if (Ar.IsLoading() && Pc && Pc->GetPieceState() != S) Pc->SetPieceState(S, Pc->GetActorLocation(), FVector::ForwardVector, false);
		}
	}
	// ---- battle flow, ledger, gauges, Awakening
	if (UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(G))
	{
		SerStructs(Ar, SM->Players);
		int32 NE = SM->Ledger ? SM->Ledger->Events.Num() : 0; Ar << NE;
		if (bHash) { uint32 LH = SM->Ledger ? SM->Ledger->Hash() : 0u; Ar << LH; } // ledger CRC without wall-clock fields
		else if (SM->Ledger) { if (Ar.IsLoading()) SM->Ledger->Events.SetNum(NE); for (FZoogiEpisodeLedgerEvent& E : SM->Ledger->Events) SerStruct(Ar, E); }
		if (!bHash) SerStructs(Ar, SM->History);
		SerE(Ar, SM->State);
		Ar << SM->CurrentActionId << SM->CurrentActor << SM->NextSegment << SM->AwakenSeenTurn << SM->bMatchEnded;
		if (!bHash) Ar << SM->MatchId; // wall-clock match id: saved, not hashed
		for (int32 p = 0; p < SM->Players.Num(); ++p)
		{
			UZoogiEpisodeEnergyComponent* En = SM->Energy(p);
			float V = En ? En->Value : 0.f, TG = En ? En->TotalGained : 0.f, TS = En ? En->TotalSpent : 0.f;
			SerF(Ar, V, bHash); SerF(Ar, TG, bHash); SerF(Ar, TS, bHash);
			if (Ar.IsLoading() && En) { En->Value = V; En->TotalGained = TG; En->TotalSpent = TS; }
			UZoogiReactionComponent* R = SM->Reaction(p);
			bool bA = R && R->bAwakened; int32 RL = R ? R->ReactionsLeft : 0, OT = R ? R->OwnTurnsAtActivation : 0, TL = R ? R->TurnsToLast : 0, AC = R ? R->Activations : 0, RU = R ? R->ReactionsUsed : 0;
			Ar << bA << RL << OT << TL << AC << RU;
			if (Ar.IsLoading() && R) { R->bAwakened = bA; R->ReactionsLeft = RL; R->OwnTurnsAtActivation = OT; R->TurnsToLast = TL; R->Activations = AC; R->ReactionsUsed = RU; }
		}
		if (UZoogiEpisodeSubsystem* Ep = SM->Episode()) Ar << Ep->NextActionId;
	}
	// ---- cards
	if (UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(G))
	{
		C->RulesV.Serialize(Ar);
		C->Fx.Serialize(Ar);
		Ar << C->FxUses;
	}
	// ---- episode facts + playback
	if (UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(G))
	{
		D->CoreV.Serialize(Ar);
		if (!bHash) Ar << D->MatchKey << D->Played << D->Skipped;
		else { int32 Shown = D->Played + D->Skipped; Ar << Shown; } // watch vs skip hash the same
		int32 NQ = D->Queue.Num(); Ar << NQ;
		if (Ar.IsLoading()) D->Queue.Reset();
		for (int32 i = 0; i < NQ; ++i)
		{
			FZoogiEpisode E = Ar.IsLoading() ? FZoogiEpisode() : D->Queue[i].Ep;
			float T = Ar.IsLoading() ? 0.f : D->Queue[i].T; bool bS = Ar.IsLoading() ? false : D->Queue[i].bSkipped;
			Ar << E; SerF(Ar, T, bHash); Ar << bS;
			if (Ar.IsLoading()) { FZoogiEpPlayback P(E); P.T = T; P.bSkipped = bS; D->Queue.Add(P); }
		}
	}
}

bool FZoogiSaveAccess::Capture(AZoogiGameMode* G, TArray<uint8>& Out)
{
	if (!G) return false;
	Out.Reset();
	FMemoryWriter W(Out, true);
	FNameAsStringProxyArchive Ar(W);
	Visit(Ar, G, false);
	return !Ar.IsError();
}

bool FZoogiSaveAccess::Restore(AZoogiGameMode* G, const TArray<uint8>& In, FString* OutErr)
{
	if (!G || In.Num() == 0) { if (OutErr) *OutErr = TEXT("empty"); return false; }
	FMemoryReader R(In, true);
	FNameAsStringProxyArchive Ar(R);
	Visit(Ar, G, false);
	if (Ar.IsError()) { if (OutErr) *OutErr = TEXT("version / layout mismatch"); return false; }
	UE_LOG(LogZoogi, Log, TEXT("SAVE restored %d bytes: turn %d P%d stage %d"), In.Num(), G->TurnCounter, G->ActivePlayer + 1, (int32)G->Stage);
	return true;
}

uint32 FZoogiSaveAccess::FullHash(AZoogiGameMode* G)
{
	TArray<uint8> B;
	FMemoryWriter W(B, true);
	FNameAsStringProxyArchive Ar(W);
	Visit(Ar, G, true);
	return FCrc::MemCrc32(B.GetData(), B.Num());
}

bool FZoogiSaveAccess::SaveSlot(AZoogiGameMode* G, const FString& Slot, const FString& Point, TArray<uint8>* OutBlob)
{
	UZoogiMatchSave* S = Cast<UZoogiMatchSave>(UGameplayStatics::CreateSaveGameObject(UZoogiMatchSave::StaticClass()));
	if (!S || !Capture(G, S->Blob)) return false;
	S->Version = Version(); S->Point = Point; S->Turn = G->TurnCounter; S->StateHash = FullHash(G);
	if (OutBlob) *OutBlob = S->Blob;
	const bool bOk = UGameplayStatics::SaveGameToSlot(S, Slot, 0);
	UE_LOG(LogZoogi, Log, TEXT("SAVE slot=%s point=%s turn=%d bytes=%d hash=%08x ok=%d"), *Slot, *Point, S->Turn, S->Blob.Num(), S->StateHash, bOk ? 1 : 0);
	return bOk;
}

bool FZoogiSaveAccess::LoadSlot(AZoogiGameMode* G, const FString& Slot, FString* OutErr)
{
	UZoogiMatchSave* S = Cast<UZoogiMatchSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!S) { if (OutErr) *OutErr = TEXT("no slot"); return false; }
	if (S->Version != Version()) { if (OutErr) *OutErr = TEXT("version"); return false; }
	return Restore(G, S->Blob, OutErr);
}

void FZoogiSaveAccess::Perturb(AZoogiGameMode* G)
{
	for (AZoogiBall* B : G->Balls) if (B && B->Sphere && !B->bOut) B->Sphere->SetWorldLocation(B->Sphere->GetComponentLocation() + FVector(40.f, -25.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	G->ComboEnergy[0] += 13.f;
	if (G->Balls.Num() && G->Balls[0]) G->Balls[0]->Score += 7;
	G->GameRng.FRand(); G->GameRng.FRand();
	if (UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(G))
	{
		if (UZoogiEpisodeEnergyComponent* En = SM->Energy(0)) En->Value = FMath::Min(En->Cap, En->Value + 7.f);
		if (SM->Ledger) { FZoogiEpisodeLedgerEvent E; E.kind = TEXT("PERTURB"); SM->Ledger->Events.Add(E); }
	}
	if (UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(G)) C->FxUses.FindOrAdd(FName(TEXT("perturb")))++;
}

// =====================================================================================================================
// validator

void UZoogiMatchValidator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiMatchTest="), MatchTag);
	FString Pts;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiSaveTest="), Pts, false)) Pts.ParseIntoArray(SavePoints, TEXT(","));
	if (!SavePoints.IsEmpty() && MatchTag.IsEmpty()) MatchTag = TEXT("save");
}

void UZoogiMatchValidator::Deinitialize()
{
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!MatchTag.IsEmpty() && !bDone && G && G->Phase != EZoogiPhase::Menu) { Check(false, TEXT("match did not reach a declared winner before quit")); Finish(G, TEXT("quit")); }
	Super::Deinitialize();
}

void UZoogiMatchValidator::Check(bool bOk, const FString& What)
{
	++Checks;
	if (!bOk) { ++Fails; if (FirstFail.IsEmpty()) FirstFail = What; UE_LOG(LogZoogi, Warning, TEXT("MATCH_TEST check FAIL: %s"), *What); }
}

void UZoogiMatchValidator::TrySavePoint(AZoogiGameMode* G, const FString& Point)
{
	if (SavesDone.Contains(Point)) return;
	UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(G);
	bool bAt = false;
	if (G->TurnCounter >= 2 && G->Phase == EZoogiPhase::Aiming)
	{
		if (Point == TEXT("boundary")) bAt = G->Stage == EZoogiStage::Awaken && !G->bAwakenDone;
		else if (Point == TEXT("commit")) bAt = G->Stage == EZoogiStage::Awaken && G->bAwakenDone; // right after the Standby commit, before Battle
	}
	if (Point == TEXT("followup")) bAt = G->bFollowUpOffered && !G->bInFollowUp;
	if (Point == TEXT("episode")) bAt = D && D->Current() && !D->Current()->Done() && D->Current()->T > 1.f;
	if (!bAt) return;
	SavesDone.Add(Point);
	++SavesTried;
	const FString Slot = FString::Printf(TEXT("ZoogiP6_%s"), *Point);
	TArray<uint8> B0;
	const uint32 H0 = FZoogiSaveAccess::FullHash(G);
	const bool bSaved = FZoogiSaveAccess::SaveSlot(G, Slot, Point, &B0);
	FZoogiSaveAccess::Perturb(G);
	const uint32 Hp = FZoogiSaveAccess::FullHash(G);
	FString Err;
	const bool bLoaded = FZoogiSaveAccess::LoadSlot(G, Slot, &Err);
	const uint32 H1 = FZoogiSaveAccess::FullHash(G);
	TArray<uint8> B1;
	FZoogiSaveAccess::Capture(G, B1);
	const bool bOk = bSaved && bLoaded && Hp != H0 && H1 == H0 && B1 == B0;
	Check(bSaved, FString::Printf(TEXT("%s: SaveGame written"), *Point));
	Check(bLoaded, FString::Printf(TEXT("%s: SaveGame loaded (%s)"), *Point, *Err));
	Check(Hp != H0, FString::Printf(TEXT("%s: perturbation changed the hash"), *Point));
	Check(H1 == H0, FString::Printf(TEXT("%s: load restores the hash %08x == %08x"), *Point, H1, H0));
	Check(B1 == B0, FString::Printf(TEXT("%s: re-captured state is byte-identical (%d vs %d bytes)"), *Point, B1.Num(), B0.Num()));
	SavesOk += bOk ? 1 : 0;
	UE_LOG(LogZoogi, Log, TEXT("SAVE_TEST point=%s turn=%d P%d result=%s saved=%08x perturbed=%08x loaded=%08x bytes=%d"), *Point, G->TurnCounter, G->ActivePlayer + 1, bOk ? TEXT("PASS") : TEXT("FAIL"), H0, Hp, H1, B0.Num());
}

void UZoogiMatchValidator::Tick(float Dt)
{
	if (MatchTag.IsEmpty()) return;
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return;
	if (QuitIn >= 0.f) { QuitIn -= Dt; if (QuitIn < 0.f) if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit")); return; }
	if (bDone || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming) return;
	for (const FString& P : SavePoints) TrySavePoint(G, P);
	if (G->bFollowUpOffered && !bWasOffered) ++FollowOffers;
	bWasOffered = G->bFollowUpOffered;
	if (G->TurnCounter != LastTurn) { LastTurn = G->TurnCounter; SinceTurn = 0.f; }
	else { SinceTurn += Dt; MaxTurnGap = FMath::Max(MaxTurnGap, SinceTurn); }
	if (SinceTurn > 90.f) { Check(false, FString::Printf(TEXT("stall: turn %d unchanged for 90 s"), G->TurnCounter)); Finish(G, TEXT("stall")); return; }
	if (G->Phase == EZoogiPhase::GameOver) Finish(G, TEXT("game_over"));
}

void UZoogiMatchValidator::Finish(AZoogiGameMode* G, const FString& Why)
{
	bDone = true;
	UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(G);
	UZoogiCardSubsystem* C = UZoogiCardSubsystem::Get(G);
	UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(G);
	const int32 Winner = G->bComboMode ? G->ComboWinner : G->WinnerIndex;
	const bool bOver = G->Phase == EZoogiPhase::GameOver;
	Check(bOver, TEXT("match reached GameOver"));
	const bool bDraw = Winner < 0 && bOver; // equal top scores (FFA) / round-limit tie or mutual final KO (2v2) is a declared draw
	Check(Winner >= 0 || bDraw, FString::Printf(TEXT("result declared (winner %d%s)"), Winner, bDraw ? TEXT(", draw") : TEXT("")));
	for (const FString& P : SavePoints)
	{
		// follow-up / episode points only exist if the match produced one; otherwise reported, not failed
		const bool bExists = P == TEXT("followup") ? FollowOffers > 0 : (P == TEXT("episode") ? (D && D->Played + D->Skipped > 0) : true);
		if (bExists) Check(SavesDone.Contains(P), FString::Printf(TEXT("save point %s reached"), *P));
		else UE_LOG(LogZoogi, Log, TEXT("SAVE_TEST point=%s result=NOT_REACHED (no natural %s this match)"), *P, *P);
	}
	int32 Actions = 0, Shatters = 0, Recov = 0, Follow = 0, EpEvents = 0, Awaken = 0;
	if (SM && SM->Ledger)
		for (const FZoogiEpisodeLedgerEvent& E : SM->Ledger->Events)
		{
			Actions += E.kind == TEXT("ACTION"); Shatters += E.kind == TEXT("SHATTER") || E.comic_state == TEXT("SHATTERED");
			Recov += E.kind == TEXT("ACTION") && E.zones.Contains(TEXT("recovery")) && !E.ring_out_ids.Contains(E.actor_id);
			Follow += E.command == TEXT("FOLLOW_UP"); EpEvents += E.kind == TEXT("EPISODE");
		}
	if (SM) for (int32 p = 0; p < SM->Players.Num(); ++p) if (UZoogiReactionComponent* R = SM->Reaction(p)) Awaken += R->Activations;
	FString Branches;
	if (D) for (const FZoogiEpisode& E : D->Core().Episodes()) Branches += FString::Printf(TEXT("%s%s"), Branches.IsEmpty() ? TEXT("") : TEXT("+"), ZoogiEpBranchName(E.Branch));
	if (C) C->DumpCoverage(MatchTag);
	const uint32 Full = FZoogiSaveAccess::FullHash(G);
	UE_LOG(LogZoogi, Log, TEXT("MATCH_TEST case=%s result=%s why=%s mode=%s winner=%d turns=%d actions=%d follow_offers=%d follow_ups=%d shatters=%d recoveries=%d awakenings=%d episodes=%d played=%d branches=%s dup_inputs=%d max_turn_gap=%.0fs saves=%d/%d ledger=%08x cards=%08x final=%08x checks=%d fails=%d first_fail=\"%s\""),
		*MatchTag, Fails == 0 ? TEXT("PASS") : TEXT("FAIL"), *Why, G->bComboMode ? TEXT("2v2") : TEXT("ffa"), Winner, G->TurnCounter, Actions, FollowOffers, Follow, Shatters, Recov, Awaken, EpEvents, D ? D->Played : 0,
		Branches.IsEmpty() ? TEXT("-") : *Branches, G->DuplicateInputsIgnored, MaxTurnGap, SavesOk, SavesTried, SM && SM->Ledger ? SM->Ledger->Hash() : 0u, C ? C->Rules().StateHash() : 0u, Full, Checks, Fails, *FirstFail);
	QuitIn = 3.f;
}
