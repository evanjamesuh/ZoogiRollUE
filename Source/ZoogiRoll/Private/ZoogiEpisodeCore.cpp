#include "ZoogiEpisodeCore.h"
#include "Misc/Crc.h"

const TCHAR* ZoogiEpBranchName(EZoogiEpBranch B)
{
	switch (B) { case EZoogiEpBranch::Success: return TEXT("SUCCESS"); case EZoogiEpBranch::Failure: return TEXT("FAILURE"); case EZoogiEpBranch::Recovery: return TEXT("RECOVERY");
	case EZoogiEpBranch::Elimination: return TEXT("ELIMINATION"); case EZoogiEpBranch::Victory: return TEXT("VICTORY"); default: return TEXT("LOSS"); }
}
const TCHAR* ZoogiEpRoleName(EZoogiEpRole R)
{
	switch (R) { case EZoogiEpRole::Opening: return TEXT("OPENING"); case EZoogiEpRole::FollowThrough: return TEXT("FOLLOW_THROUGH"); case EZoogiEpRole::ReceivingImpact: return TEXT("RECEIVING_IMPACT");
	case EZoogiEpRole::Miss: return TEXT("MISS"); default: return TEXT("PREPARING_RESPONSE"); }
}
const TCHAR* ZoogiRecapName(EZoogiRecap R) { return R == EZoogiRecap::Full ? TEXT("FULL") : R == EZoogiRecap::Partial ? TEXT("PARTIAL") : TEXT("NONE"); }

FString FZoogiEpFact::Key() const
{
	FString T, R;
	for (int32 X : Targets) T += FString::Printf(TEXT("%d,"), X);
	for (int32 X : RingOuts) R += FString::Printf(TEXT("%d,"), X);
	return FString::Printf(TEXT("%s|%d|%d|%d|%d|%s|%s|%d%d%d%d%d%d|%s"), *Kind, Turn, Round, ActionId, Actor, *T, *R, bCollision ? 1 : 0, bFollowUp ? 1 : 0, bShattered ? 1 : 0,
		bObjectiveFailed ? 1 : 0, bActorRecovered ? 1 : 0, bMatchOver ? 1 : 0, *FinalHash);
}

int32 FZoogiEpPlayback::PerspectiveIndex() const
{
	if (Ep.Perspectives.Num() == 0) return -1;
	for (int32 i = Ep.Perspectives.Num() - 1; i >= 0; --i) if (T >= Ep.Perspectives[i].Start) return i;
	return 0;
}

void FZoogiEpisodeCore::Reset(int32 InPlayers, bool bInTeams, const TArray<FString>& InNames)
{
	Num = FMath::Max(1, InPlayers); bTeams = bInTeams; Names = InNames;
	FactsV.Reset(); Keys.Reset(); EpisodesV.Reset(); Spent.Reset();
	ContribV.Init(FZoogiEpContribution(), Num);
	Dups = 0; NumActions = 0; Winner = -1; bEnded = false;
}

FString FZoogiEpisodeCore::Nm(int32 P) const
{
	return FString::Printf(TEXT("P%d %s"), P + 1, Names.IsValidIndex(P) ? *Names[P] : TEXT("Zoogi"));
}

const FZoogiEpFact* FZoogiEpisodeCore::PrevAction(const FZoogiEpFact& F) const
{
	const FZoogiEpFact* Prev = nullptr;
	for (const FZoogiEpFact& X : FactsV)
	{
		if (X.Seq >= F.Seq) break;
		if (X.Kind == TEXT("ACTION")) Prev = &X;
	}
	return Prev;
}

EZoogiEpRole FZoogiEpisodeCore::RoleOf(const FZoogiEpFact& F, int32 P) const
{
	if (P == F.Actor)
	{
		if (!F.bCollision && F.Targets.Num() == 0) return EZoogiEpRole::Miss;
		return F.bFollowUp ? EZoogiEpRole::FollowThrough : EZoogiEpRole::Opening;
	}
	if (F.Targets.Contains(P) || F.RingOuts.Contains(P)) return EZoogiEpRole::ReceivingImpact;
	return EZoogiEpRole::PreparingResponse;
}

bool FZoogiEpisodeCore::AddFact(const FZoogiEpFact& F)
{
	bool bDup = false;
	Keys.Add(F.Key(), &bDup);
	if (bDup) { ++Dups; return false; }
	const int32 i = FactsV.Add(F);
	const FZoogiEpFact& X = FactsV[i];
	if (X.Kind == TEXT("MATCH_END")) { bEnded = true; Winner = X.Actor; return true; }
	if (X.Kind != TEXT("ACTION")) return true;
	++NumActions;
	for (int32 p = 0; p < Num; ++p)
	{
		FZoogiEpContribution& C = ContribV[p];
		switch (RoleOf(X, p))
		{
		case EZoogiEpRole::Opening: ++C.Opening; ++C.ActiveWithContact; break;
		case EZoogiEpRole::FollowThrough: ++C.FollowThrough; ++C.ActiveWithContact; break;
		case EZoogiEpRole::ReceivingImpact: ++C.Receiving; break;
		case EZoogiEpRole::Miss: ++C.Misses; break;
		default: ++C.Preparing; break;
		}
		if (X.RingOuts.Contains(p)) ++C.RungOut;
	}
	return true;
}

bool FZoogiEpisodeCore::IsWinnerSide(int32 P) const
{
	if (!bEnded || Winner < 0) return false;
	return bTeams ? (P / 2) == (Winner / 2) : P == Winner;
}

EZoogiEpBranch FZoogiEpisodeCore::BranchFor(const FZoogiEpFact& F, int32 Spender) const
{
	if (F.bMatchOver || F.Kind == TEXT("MATCH_END")) return IsWinnerSide(Spender) ? EZoogiEpBranch::Victory : EZoogiEpBranch::Loss;
	if (F.RingOuts.Contains(Spender)) return EZoogiEpBranch::Failure;
	if (F.RingOuts.Num() > 0) return EZoogiEpBranch::Elimination;
	if (F.bShattered || F.bObjectiveFailed) return EZoogiEpBranch::Failure;
	if (F.bActorRecovered && F.Actor == Spender) return EZoogiEpBranch::Recovery;
	if (F.Actor == Spender && RoleOf(F, Spender) == EZoogiEpRole::Miss) return EZoogiEpBranch::Failure;
	if (F.Actor != Spender && F.Targets.Contains(Spender)) return EZoogiEpBranch::Recovery; // took the hit and is still in
	return EZoogiEpBranch::Success;
}

FString FZoogiEpisodeCore::PerspectiveLine(const FZoogiEpFact& F, int32 P, EZoogiEpRole R, EZoogiEpBranch B, int32 Spender) const
{
	FString Tg;
	for (int32 T : F.Targets) Tg += (Tg.IsEmpty() ? TEXT("") : TEXT(", ")) + Nm(T);
	const bool bOut = F.RingOuts.Contains(P);
	const FZoogiEpFact* Pa = PrevAction(F);
	const FString Prev = Pa && Pa->Actor >= 0 && Pa->Actor != F.Actor ? Nm(Pa->Actor) : FString(TEXT("the setup"));
	FString L;
	switch (R)
	{
	case EZoogiEpRole::Opening: L = Tg.IsEmpty() ? FString::Printf(TEXT("%s opens and connects"), *Nm(P)) : FString::Printf(TEXT("%s opens: strikes %s"), *Nm(P), *Tg); break;
	case EZoogiEpRole::FollowThrough: L = FString::Printf(TEXT("%s follows through on %s"), *Nm(P), *Prev) + (Tg.IsEmpty() ? FString() : FString::Printf(TEXT(", hitting %s"), *Tg)); break;
	case EZoogiEpRole::Miss: L = FString::Printf(TEXT("%s commits... and misses - nothing connects"), *Nm(P)); break;
	case EZoogiEpRole::ReceivingImpact: L = FString::Printf(TEXT("%s takes the impact from %s%s"), *Nm(P), *Nm(F.Actor), bOut ? TEXT(" and is knocked out of the ring") : TEXT(" and holds on")); break;
	default: L = FString::Printf(TEXT("%s watches it unfold, preparing a response"), *Nm(P)); break;
	}
	if (bOut && P == F.Actor) L += TEXT(" and rolls out of the ring");
	if (F.bShattered && P == F.Actor) L += TEXT(" (the combo shatters)");
	if (F.bActorRecovered && P == F.Actor && !bOut) L += TEXT(" - recovers at the edge");
	if (B == EZoogiEpBranch::Victory || B == EZoogiEpBranch::Loss) L += IsWinnerSide(P) ? TEXT("  [VICTORY]") : TEXT("  [LOSS]");
	if (P == Spender) L = TEXT("* ") + L;
	return L;
}

bool FZoogiEpisodeCore::TrySpend(int32 P, float Gauge, int32 FactIndex, FZoogiEpisode& Out)
{
	if (P < 0 || P >= Num || !FactsV.IsValidIndex(FactIndex) || Gauge + KINDA_SMALL_NUMBER < ZoogiEpTune::GaugeCost) return false;
	const FZoogiEpFact& F = FactsV[FactIndex];
	if (F.Kind != TEXT("ACTION") && F.Kind != TEXT("MATCH_END")) return false;
	bool bDup = false;
	Spent.Add(FString::Printf(TEXT("%d|%s"), P, *F.Key()), &bDup);
	if (bDup) return false;
	FZoogiEpisode E;
	E.EpisodeId = EpisodesV.Num() + 1;
	E.Spender = P;
	E.FactEventId = F.EventId;
	E.FactSeq = F.Seq;
	E.Branch = BranchFor(F, P);
	TArray<int32> Order; Order.Add(P);
	for (int32 i = 0; i < Num; ++i) if (i != P) Order.Add(i);
	for (int32 k = 0; k < 4; ++k) // always four 3 s perspectives (fewer players: the order repeats)
	{
		FZoogiEpPerspective V;
		V.Player = Order[k % Order.Num()];
		V.Role = F.Kind == TEXT("ACTION") ? RoleOf(F, V.Player) : EZoogiEpRole::PreparingResponse;
		V.Start = k * ZoogiEpTune::PerspectiveSeconds;
		V.Line = PerspectiveLine(F, V.Player, V.Role, E.Branch, P);
		E.Perspectives.Add(V);
	}
	E.Duration = E.Perspectives.Num() * ZoogiEpTune::PerspectiveSeconds;
	Out = E;
	EpisodesV.Add(E);
	return true;
}

EZoogiRecap FZoogiEpisodeCore::RecapUnlock(int32 P, FString* OutWhy) const
{
	if (P < 0 || P >= Num) return EZoogiRecap::None;
	const FZoogiEpContribution& C = ContribV[P];
	const bool U1 = bEnded, U2 = NumActions >= ZoogiEpTune::RecapMinActions, U3 = C.ActiveWithContact >= ZoogiEpTune::RecapMinActive || IsWinnerSide(P);
	const bool bAny = C.Opening + C.FollowThrough + C.Receiving + C.Misses > 0;
	const EZoogiRecap R = (U1 && U2 && U3) ? EZoogiRecap::Full : (U1 && bAny) ? EZoogiRecap::Partial : EZoogiRecap::None;
	if (OutWhy) *OutWhy = FString::Printf(TEXT("U1_match_end=%d U2_actions=%d>=%d:%d U3_active=%d>=%d|winner=%d:%d any_role=%d U4_watch_ignored=1"),
		U1 ? 1 : 0, NumActions, ZoogiEpTune::RecapMinActions, U2 ? 1 : 0, C.ActiveWithContact, ZoogiEpTune::RecapMinActive, IsWinnerSide(P) ? 1 : 0, U3 ? 1 : 0, bAny ? 1 : 0);
	return R;
}

uint32 FZoogiEpisodeCore::Hash() const
{
	uint32 H = 0;
	for (const FZoogiEpFact& F : FactsV) H = FCrc::StrCrc32(*F.Key(), H);
	for (const FZoogiEpisode& E : EpisodesV)
	{
		H = FCrc::StrCrc32(*FString::Printf(TEXT("%d|%d|%s|%d"), E.EpisodeId, E.Spender, *E.FactEventId, (int32)E.Branch), H);
		for (const FZoogiEpPerspective& V : E.Perspectives) H = FCrc::StrCrc32(*V.Line, H);
	}
	for (const FZoogiEpContribution& C : ContribV) H = FCrc::StrCrc32(*FString::Printf(TEXT("%d.%d.%d.%d.%d.%d.%d"), C.Opening, C.FollowThrough, C.Receiving, C.Preparing, C.Misses, C.ActiveWithContact, C.RungOut), H);
	return H;
}

// ------------------------------------------------------------------ v3 P6 save / resume
FArchive& operator<<(FArchive& Ar, FZoogiEpFact& F)
{
	Ar << F.EventId << F.Seq << F.Kind << F.Turn << F.Round << F.ActionId << F.Actor << F.Targets << F.RingOuts << F.bCollision << F.bFollowUp << F.bShattered
		<< F.bObjectiveFailed << F.bActorRecovered << F.bMatchOver << F.FinalHash;
	return Ar;
}
FArchive& operator<<(FArchive& Ar, FZoogiEpisode& E)
{
	uint8 Br = (uint8)E.Branch;
	Ar << E.EpisodeId << E.Spender << E.FactEventId << E.FactSeq << Br << E.Duration;
	E.Branch = (EZoogiEpBranch)Br;
	int32 N = E.Perspectives.Num(); Ar << N; if (Ar.IsLoading()) E.Perspectives.SetNum(N);
	for (FZoogiEpPerspective& V : E.Perspectives) { uint8 R = (uint8)V.Role; Ar << V.Player << R << V.Line << V.Start << V.Duration; V.Role = (EZoogiEpRole)R; }
	return Ar;
}

void FZoogiEpisodeCore::Serialize(FArchive& Ar)
{
	uint32 Ver = 1; Ar << Ver;
	Ar << Num << bTeams << Names << FactsV << Keys;
	int32 NC = ContribV.Num(); Ar << NC; if (Ar.IsLoading()) ContribV.SetNum(NC);
	for (FZoogiEpContribution& C : ContribV) Ar << C.Opening << C.FollowThrough << C.Receiving << C.Preparing << C.Misses << C.ActiveWithContact << C.RungOut;
	Ar << EpisodesV << Spent << Dups << NumActions << Winner << bEnded;
}
