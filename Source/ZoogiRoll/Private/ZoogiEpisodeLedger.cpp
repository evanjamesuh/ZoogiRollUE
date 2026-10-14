#include "ZoogiEpisodeLedger.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiEpisodeDirector.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString Q(const FString& S) { return FString::Printf(TEXT("\"%s\""), *S.ReplaceCharWithEscapedChar()); }
	FString Arr(const TArray<FString>& A) { FString O; for (const FString& S : A) O += (O.IsEmpty() ? TEXT("") : TEXT(",")) + Q(S); return TEXT("[") + O + TEXT("]"); }
	FString ArrI(const TArray<int32>& A) { FString O; for (int32 V : A) O += FString::Printf(TEXT("%s%d"), O.IsEmpty() ? TEXT("") : TEXT(","), V); return TEXT("[") + O + TEXT("]"); }
	FString ArrF(const TArray<float>& A) { FString O; for (float V : A) O += FString::Printf(TEXT("%s%.1f"), O.IsEmpty() ? TEXT("") : TEXT(","), V); return TEXT("[") + O + TEXT("]"); }
	FString B(bool b) { return b ? TEXT("true") : TEXT("false"); }
	FString Slice(const FString& Zoogi, const TCHAR* Emotion) { return FString::Printf(TEXT("slice_eye_%s_%s"), *Zoogi.ToLower().Replace(TEXT(" "), TEXT("_")), Emotion); }
}

// =====================================================================================================================
// ledger

FString UZoogiEpisodeLedger::ToJson(const FZoogiEpisodeLedgerEvent& E)
{
	return FString::Printf(TEXT("{\"event_id\":%s,\"seq\":%d,\"turn_index\":%d,\"round\":%d,\"action_id\":%d,\"game_time\":%.2f,\"kind\":%s,\"actor_id\":%s,\"actor_player\":%d,\"target_ids\":%s,")
		TEXT("\"command\":%s,\"minigame\":%s,\"objective_result\":%s,\"collision\":%s,\"ring_out\":%s,\"ring_out_ids\":%s,\"orb_outs\":%d,\"score_delta\":%s,\"tags\":%s,\"tier\":%d,")
		TEXT("\"episode_energy_delta\":%s,\"episode_segment_id\":%s,\"clip_id\":%s,\"shot_ids\":%s,\"slice_ids\":%s,\"zones\":%s,\"comic_state\":%s,\"next_state\":%s,\"spotlight\":%s,\"variant\":%s,\"final_hash\":%s}"),
		*Q(E.event_id), E.seq, E.turn_index, E.round, E.action_id, E.game_time, *Q(E.kind), *Q(E.actor_id), E.actor_player, *Arr(E.target_ids),
		*Q(E.command), *Q(E.minigame), *Q(E.objective_result), *B(E.collision), *B(E.ring_out), *Arr(E.ring_out_ids), E.orb_outs, *ArrI(E.score_delta), *Arr(E.tags), E.tier,
		*ArrF(E.episode_energy_delta), *Q(E.episode_segment_id), *Q(E.clip_id), *Arr(E.shot_ids), *Arr(E.slice_ids), *Arr(E.zones), *Q(E.comic_state), *Q(E.next_state), *ArrI(E.spotlight), *Q(E.variant), *Q(E.final_hash));
}

const FZoogiEpisodeLedgerEvent& UZoogiEpisodeLedger::Append(FZoogiEpisodeLedgerEvent E)
{
	E.seq = Events.Num();
	E.event_id = FString::Printf(TEXT("ev_%04d_%s"), E.seq, *E.kind.ToLower());
	const int32 i = Events.Add(E);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE_LEDGER %s"), *ToJson(Events[i]));
	return Events[i];
}

uint32 UZoogiEpisodeLedger::Hash() const
{
	uint32 H = 0;
	for (FZoogiEpisodeLedgerEvent E : Events)
	{
		E.game_time = 0.f; // timing is not part of the replay identity
		const FString J = ToJson(E);
		H = FCrc::StrCrc32(*J, H);
	}
	return H;
}

// =====================================================================================================================
// state machine -> ledger

void UZoogiBattleStateMachine::LedgerAction(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, const FZoogiCinematicPlan& Plan)
{
	AZoogiGameMode* G = GM();
	if (!G || !Ledger) return;
	FZoogiEpisodeLedgerEvent E;
	E.kind = TEXT("ACTION");
	E.turn_index = Truth.Turn;
	E.round = Truth.Round;
	E.action_id = Truth.ActionId;
	E.game_time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	E.actor_player = Truth.Actor;
	E.actor_id = FString::Printf(TEXT("P%d_%s"), Truth.Actor + 1, *Truth.ActorName.ToLower());
	TSet<int32> Targets;
	for (const FZoogiContactEvent& C : Truth.Contacts)
	{
		if (C.Body == Truth.Actor && C.OtherBody >= 0) Targets.Add(C.OtherBody);
		else if (C.OtherBody == Truth.Actor && C.Body >= 0) Targets.Add(C.Body);
		if (C.Body >= 0 && C.Body < 100 && C.OtherBody >= 0) E.collision = true;
	}
	TArray<int32> TL = Targets.Array();
	TL.Sort();
	for (int32 T : TL) E.target_ids.Add(T >= 100 ? FString::Printf(TEXT("O%d"), T - 99) : FString::Printf(TEXT("P%d_%s"), T + 1, Players.IsValidIndex(T) ? *Players[T].Name.ToLower() : TEXT("zoogi")));
	// the segment that rode on this action (completed or shattered), if any
	const FZoogiPendingSegment* Seg = nullptr;
	for (int32 i = History.Num() - 1; i >= 0; --i) if (History[i].ActionId == Truth.ActionId && History[i].Challenge != EZoogiChallenge::AirReaction) { Seg = &History[i]; break; }
	E.command = Truth.bFollowUp ? TEXT("FOLLOW_UP") : TEXT("SHOT");
	if (Seg) E.command = FString::Printf(TEXT("COMBO:%s"), *Seg->ComboId.ToString());
	E.minigame = Seg ? ZoogiBattle::ChallengeName(Seg->Challenge) : TEXT("None");
	E.objective_result = Seg ? ZoogiBattle::ResultName(Seg->Result) : TEXT("NONE");
	E.comic_state = Seg ? ZoogiBattle::ComicName(Seg->Comic) : TEXT("NONE");
	E.episode_segment_id = Seg ? Seg->SegmentId : FString();
	E.clip_id = (Seg && Seg->Comic == EZoogiComicState::Completed) ? Seg->ClipId.ToString() : FString(); // a shattered segment never carries its clip
	for (int32 R : Truth.RingOuts) { E.ring_out = true; E.ring_out_ids.Add(FString::Printf(TEXT("P%d_%s"), R + 1, Players.IsValidIndex(R) ? *Players[R].Name.ToLower() : TEXT("zoogi"))); }
	E.orb_outs = Truth.OrbOuts;
	E.score_delta = Truth.ScoreDelta;
	for (EZoogiEpisodeTag T : Rec.Tags) E.tags.Add(ZoogiEpisode::TagName(T));
	E.tier = Rec.Tier;
	for (int32 p = 0; p < Players.Num(); ++p) { const float* D = ActionEnergyDelta.Find(p); E.episode_energy_delta.Add(D ? *D : 0.f); }
	for (const FZoogiEpisodeShot& S : Plan.Shots) E.shot_ids.Add(S.ShotId.ToString());
	// comic slice placeholders (ref comic_slice_pack1_eyes): actor + every rung-out Zoogi
	const TCHAR* ActorEmotion = (Seg && Seg->Comic == EZoogiComicState::Shattered) ? TEXT("angry") : (Seg ? TEXT("targeting") : (Rec.Tier >= 3 ? TEXT("surprised") : TEXT("calm")));
	E.slice_ids.Add(Slice(Truth.ActorName, ActorEmotion));
	for (int32 R : Truth.RingOuts) if (R != Truth.Actor && Players.IsValidIndex(R)) E.slice_ids.Add(Slice(Players[R].Name, TEXT("surprised")));
	// zone tags (ref frozen_zones_aerial): court always; aerial if anything flew; recovery if anything crossed the boundary
	E.zones.Add(TEXT("court"));
	if (Truth.Air.Num() > 0) E.zones.Add(TEXT("aerial"));
	if (Truth.Boundary.Num() > 0) E.zones.Add(TEXT("recovery"));
	for (const FString& Z : ActionZoneTags) E.zones.AddUnique(Z);            // T3 debug zone volumes (aerial props / layers)
	for (const FString& P : ActionPropTags) E.tags.Add(FString::Printf(TEXT("PROP:%s"), *P)); // halo pass / rim, bumper, rail window
	E.spotlight.Add(Truth.Actor);
	if (Rec.Tier >= 3) for (int32 T : TL) if (T < 100 && !E.spotlight.Contains(T)) E.spotlight.Add(T);
	E.variant = FString::Printf(TEXT("v_%s_t%d_%d"), Rec.Tags.Num() ? ZoogiEpisode::TagName(Rec.Tags[0]) : TEXT("plain"), Rec.Tier, Truth.ActionId % 3);
	E.next_state = G->Phase == EZoogiPhase::GameOver ? TEXT("MATCH_END") : TEXT("NEXT_TURN");
	FString Fin;
	for (int32 i = 0; i < Truth.FinalBallPos.Num(); ++i)
		Fin += FString::Printf(TEXT("%d:%.0f,%.0f,%d,%d;"), i, Truth.FinalBallPos[i].X, Truth.FinalBallPos[i].Y, Truth.FinalBallOut.IsValidIndex(i) && Truth.FinalBallOut[i] ? 1 : 0, G->Balls.IsValidIndex(i) && G->Balls[i] ? G->Balls[i]->Score : 0);
	E.final_hash = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Fin));
	for (int32 T : TL) if (Players.IsValidIndex(T)) Players[T].Contributions++;
	if (Players.IsValidIndex(Truth.Actor)) Players[Truth.Actor].Contributions++;
	Ledger->Append(E);
	if (UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(this)) D->OnLedgerEvent(this, Ledger->Events.Last()); // v3 P5 (read-only consumer)
}

void UZoogiBattleStateMachine::LedgerSimple(const FString& Kind, int32 Actor, const FString& Detail)
{
	AZoogiGameMode* G = GM();
	if (!Ledger) return;
	FZoogiEpisodeLedgerEvent E;
	E.kind = Kind;
	E.turn_index = G ? G->TurnCounter : 0;
	E.round = G ? G->RoundNumber : 0;
	E.action_id = CurrentActionId;
	E.game_time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	E.actor_player = Actor;
	E.actor_id = Actor >= 0 ? FString::Printf(TEXT("P%d_%s"), Actor + 1, Players.IsValidIndex(Actor) ? *Players[Actor].Name.ToLower() : TEXT("zoogi")) : TEXT("-");
	E.command = Detail;
	if (Kind == TEXT("SHATTER"))
	{
		TArray<FString> Parts;
		Detail.ParseIntoArray(Parts, TEXT("|"), false);
		E.episode_segment_id = Parts.IsValidIndex(0) ? Parts[0] : FString();
		E.command = Parts.IsValidIndex(1) ? FString::Printf(TEXT("COMBO:%s"), *Parts[1]) : Detail;
		E.objective_result = TEXT("FAILED");
		E.comic_state = TEXT("SHATTERED");
		E.next_state = TEXT("NEXT_TURN");
		if (Players.IsValidIndex(Actor)) E.slice_ids.Add(Slice(Players[Actor].Name, TEXT("angry")));
	}
	else if (Kind == TEXT("REACTION"))
	{
		E.minigame = TEXT("AirReaction");
		E.objective_result = Detail.Contains(TEXT("TIMEOUT")) ? TEXT("TIMEOUT") : TEXT("PENDING");
		E.zones.Add(TEXT("aerial"));
		if (Players.IsValidIndex(Actor)) E.slice_ids.Add(Slice(Players[Actor].Name, TEXT("targeting")));
		E.next_state = TEXT("PHYSICS_RESOLVE");
	}
	for (int32 p = 0; p < Players.Num(); ++p) E.episode_energy_delta.Add(0.f);
	if (Actor >= 0) E.spotlight.Add(Actor);
	Ledger->Append(E);
	if (UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(this)) D->OnLedgerEvent(this, Ledger->Events.Last()); // v3 P5 (read-only consumer)
}

void UZoogiBattleStateMachine::OnMatchEnd(const FString& Why)
{
	AZoogiGameMode* G = GM();
	if (!G || !Ledger || !Assembler) return;
	int32 WinP = G->WinnerIndex, WinT = -1;
	if (G->bComboMode) { WinT = G->ComboWinner; WinP = WinT >= 0 ? WinT * 2 : -1; }
	LedgerSimple(TEXT("MATCH_END"), WinP, Why);
	Assembler->Assemble(this, Ledger, WinP, WinT, G->bComboMode, MatchId, Why);
	if (!TestCase.IsEmpty()) TestOnMatchEnd();
}

// =====================================================================================================================
// assembler

void UZoogiEpisodeAssembler::Assemble(UZoogiBattleStateMachine* SM, const UZoogiEpisodeLedger* L, int32 WinnerPlayer, int32 WinnerTeam, bool bTeams, const FString& MatchId, const FString& Why)
{
	Playlist.Reset();
	PlayerResults.Reset();
	if (!SM || !L) return;
	const FZoogiBattleTuning& T = SM->GetTuning();
	const AZoogiGameMode* G = SM->GetGame();
	// canonical playlist: ledger order; completed segments, spotlights (tier >= 3) and KOs; bridges over gaps
	int32 PrevSeq = -1;
	for (const FZoogiEpisodeLedgerEvent& E : L->Events)
	{
		if (E.kind != TEXT("ACTION")) continue;
		const bool bSeg = E.comic_state == TEXT("COMPLETED");
		const bool bSpot = E.tier >= 3;
		const bool bKO = E.ring_out && E.ring_out_ids.Num() > 0;
		if (!bSeg && !bSpot && !bKO) continue;
		if (PrevSeq >= 0 && E.seq > PrevSeq + 1)
		{
			FZoogiPlaylistEntry Br; Br.kind = TEXT("bridge"); Br.clip_id = FString::Printf(TEXT("bridge_%04d_%04d"), PrevSeq, E.seq);
			Playlist.Add(Br);
		}
		FZoogiPlaylistEntry P;
		P.seq = E.seq;
		P.kind = bSeg ? TEXT("segment") : (bKO ? TEXT("ko") : TEXT("spotlight"));
		P.clip_id = bSeg ? E.clip_id : FString::Printf(TEXT("clip_%s_%s"), *E.actor_id.ToLower(), *P.kind);
		P.actor_id = E.actor_id;
		P.actor_player = E.actor_player;
		P.players = E.spotlight;
		if (!P.players.Contains(E.actor_player)) P.players.Add(E.actor_player);
		for (const FString& R : E.ring_out_ids) { const int32 Idx = FCString::Atoi(*R.Mid(1)) - 1; if (Idx >= 0 && !P.players.Contains(Idx)) P.players.Add(Idx); }
		P.slice_ids = E.slice_ids;
		Playlist.Add(P);
		PrevSeq = E.seq;
	}
	{
		FZoogiPlaylistEntry F; F.kind = TEXT("finale"); F.clip_id = TEXT("clip_match_finale"); F.seq = L->Events.Num() - 1; F.actor_player = WinnerPlayer;
		Playlist.Add(F);
	}
	// per player
	for (const FZoogiBattlePlayerState& PS : SM->Players)
	{
		FZoogiPlayerAssembly A;
		A.player = PS.Player;
		A.name = PS.Name;
		A.winner = bTeams ? (WinnerTeam >= 0 && AZoogiGameMode::TeamOfIndex(PS.Player) == WinnerTeam) : PS.Player == WinnerPlayer;
		A.contributions = PS.Contributions;
		A.spotlights = PS.Spotlights;
		A.segments_completed = PS.SegmentsCompleted;
		A.segments_shattered = PS.SegmentsShattered;
		A.kos = PS.KOs;
		const int32 Score = G && G->Balls.IsValidIndex(PS.Player) && G->Balls[PS.Player] ? G->Balls[PS.Player]->Score : 0;
		A.performance = Score * T.PerfWeightScore + PS.KOs * T.PerfWeightKO + PS.SegmentsCompleted * T.PerfWeightSegment + PS.Spotlights * T.PerfWeightSpotlight - PS.SegmentsShattered * T.PerfWeightShatter;
		const FString Me = FString::Printf(TEXT("P%d_"), PS.Player + 1);
		int32 FragN = 0;
		auto AddFrag = [&](const FString& Kind, const FString& Clip, int32 Seq, bool bUnlocked)
		{
			FZoogiEpisodeFragment F; F.fragment_id = FString::Printf(TEXT("frag_p%d_%02d_%s"), PS.Player + 1, FragN++, *Kind); F.player = PS.Player; F.kind = Kind; F.clip_id = Clip; F.source_seq = Seq; F.unlocked = bUnlocked;
			A.fragments.Add(F);
		};
		for (const FZoogiEpisodeLedgerEvent& E : L->Events)
		{
			if (E.kind != TEXT("ACTION")) continue;
			const bool bActor = E.actor_player == PS.Player;
			bool bVictim = false;
			for (const FString& R : E.ring_out_ids) if (R.StartsWith(Me)) bVictim = true;
			bool bTarget = false;
			for (const FString& Tg : E.target_ids) if (Tg.StartsWith(Me)) bTarget = true;
			if (bActor && E.comic_state == TEXT("COMPLETED")) AddFrag(TEXT("signature"), E.clip_id, E.seq, true);           // earned segments are always kept
			else if (bActor && E.tier >= 3) AddFrag(TEXT("spotlight"), FString::Printf(TEXT("clip_%s_spotlight"), *E.actor_id), E.seq, A.performance >= T.FragmentPerf);
			if (bVictim && !bActor) AddFrag(TEXT("defeat"), FString::Printf(TEXT("clip_p%d_defeat_by_%s"), PS.Player + 1, *E.actor_id), E.seq, true);
			else if (bTarget && !bActor && E.tier >= 3) AddFrag(TEXT("witness"), FString::Printf(TEXT("clip_p%d_witness_%04d"), PS.Player + 1, E.seq), E.seq, true);
		}
		const bool bFull = A.winner && A.performance >= T.FullEpisodePerf && PS.SegmentsCompleted >= T.FullEpisodeMinSegments && PS.SegmentsShattered <= T.FullEpisodeMaxShatters;
		if (bFull) AddFrag(TEXT("full_episode"), TEXT("clip_full_episode"), L->Events.Num() - 1, true);
		bool bAnyFrag = false;
		for (const FZoogiEpisodeFragment& F : A.fragments) bAnyFrag |= F.unlocked;
		A.unlock = bFull ? TEXT("full") : (bAnyFrag ? TEXT("partial") : TEXT("none"));
		for (int32 i = 0; i < Playlist.Num(); ++i)
		{
			const FZoogiPlaylistEntry& P = Playlist[i];
			if (P.kind == TEXT("bridge") || P.kind == TEXT("finale") || P.players.Contains(PS.Player)) A.edit.Add(i);
		}
		FString Frags;
		for (const FZoogiEpisodeFragment& F : A.fragments) Frags += FString::Printf(TEXT("%s%s:%s%s"), Frags.IsEmpty() ? TEXT("") : TEXT(","), *F.kind, *F.clip_id, F.unlocked ? TEXT("") : TEXT("(locked)"));
		UE_LOG(LogZoogi, Log, TEXT("BATTLE assemble player=%d name=%s winner=%d perf=%.2f contributions=%d spotlights=%d segments=%d shattered=%d kos=%d unlock=%s edit=%d fragments=[%s]"),
			PS.Player + 1, *PS.Name, A.winner ? 1 : 0, A.performance, A.contributions, A.spotlights, A.segments_completed, A.segments_shattered, A.kos, *A.unlock, A.edit.Num(), *Frags);
		PlayerResults.Add(A);
	}
	FString PL;
	for (const FZoogiPlaylistEntry& P : Playlist) PL += FString::Printf(TEXT("%s%d:%s:%s"), PL.IsEmpty() ? TEXT("") : TEXT(" "), P.seq, *P.kind, *P.clip_id);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE playlist match=%s why=%s entries=%d ledger_events=%d ledger_hash=%08x [%s]"), *MatchId, *Why, Playlist.Num(), L->Events.Num(), L->Hash(), *PL);
	LastPath = Dump(L, MatchId);
}

FString UZoogiEpisodeAssembler::Dump(const UZoogiEpisodeLedger* L, const FString& MatchId) const
{
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Episodes"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = FPaths::Combine(Dir, MatchId + TEXT(".json"));
	FString J = FString::Printf(TEXT("{\n\"match_id\":%s,\n\"ledger_hash\":\"%08x\",\n\"ledger\":[\n"), *Q(MatchId), L->Hash());
	for (int32 i = 0; i < L->Events.Num(); ++i) J += UZoogiEpisodeLedger::ToJson(L->Events[i]) + (i + 1 < L->Events.Num() ? TEXT(",\n") : TEXT("\n"));
	J += TEXT("],\n\"playlist\":[\n");
	for (int32 i = 0; i < Playlist.Num(); ++i)
	{
		const FZoogiPlaylistEntry& P = Playlist[i];
		J += FString::Printf(TEXT("{\"seq\":%d,\"kind\":%s,\"clip_id\":%s,\"actor_id\":%s,\"players\":%s,\"slice_ids\":%s}%s\n"), P.seq, *Q(P.kind), *Q(P.clip_id), *Q(P.actor_id), *ArrI(P.players), *Arr(P.slice_ids), i + 1 < Playlist.Num() ? TEXT(",") : TEXT(""));
	}
	J += TEXT("],\n\"players\":[\n");
	for (int32 i = 0; i < PlayerResults.Num(); ++i)
	{
		const FZoogiPlayerAssembly& A = PlayerResults[i];
		FString Fr;
		for (const FZoogiEpisodeFragment& F : A.fragments)
			Fr += FString::Printf(TEXT("%s{\"fragment_id\":%s,\"kind\":%s,\"clip_id\":%s,\"source_seq\":%d,\"unlocked\":%s}"), Fr.IsEmpty() ? TEXT("") : TEXT(","), *Q(F.fragment_id), *Q(F.kind), *Q(F.clip_id), F.source_seq, *B(F.unlocked));
		J += FString::Printf(TEXT("{\"player\":%d,\"name\":%s,\"winner\":%s,\"performance\":%.2f,\"contributions\":%d,\"spotlights\":%d,\"segments_completed\":%d,\"segments_shattered\":%d,\"kos\":%d,\"unlock\":%s,\"edit\":%s,\"fragments\":[%s]}%s\n"),
			A.player + 1, *Q(A.name), *B(A.winner), A.performance, A.contributions, A.spotlights, A.segments_completed, A.segments_shattered, A.kos, *Q(A.unlock), *ArrI(A.edit), *Fr, i + 1 < PlayerResults.Num() ? TEXT(",") : TEXT(""));
	}
	J += TEXT("]\n}\n");
	FFileHelper::SaveStringToFile(J, *Path);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE episode dump %s"), *Path);
	return Path;
}
