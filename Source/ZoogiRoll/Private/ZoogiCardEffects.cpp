#include "ZoogiCardEffects.h"
#include "Misc/Crc.h"

void FZoogiCardEffectCore::Reset()
{
	Active.Reset(); Applied.Reset(); Bursts.Reset(); SavedMass.Reset(); Frost.Reset(); OrbScored.Reset(); Pairs.Reset();
}

FZoogiFxArmed& FZoogiCardEffectCore::Arm(FName Card, int64 EffectId, int32 Owner, int32 Turn, float A, float B, EZoogiCardExpiry Expiry)
{
	// one live instance of a card per owner: re-arming refreshes it (never stacks)
	for (int32 i = Active.Num() - 1; i >= 0; --i) if (Active[i].Card == Card && Active[i].Owner == Owner) Active.RemoveAt(i);
	FZoogiFxArmed& F = Active.AddDefaulted_GetRef();
	F.Card = Card; F.EffectId = EffectId; F.Owner = Owner; F.Turn = Turn; F.A = A; F.B = B; F.Expiry = Expiry;
	F.bArmed = Card != FName(TEXT("anchor_guard"));
	return F;
}

FZoogiFxArmed* FZoogiCardEffectCore::Find(FName Card, int32 Owner)
{
	for (FZoogiFxArmed& F : Active) if (F.Card == Card && F.Owner == Owner && !F.bConsumed) return &F;
	return nullptr;
}

const FZoogiFxArmed* FZoogiCardEffectCore::Find(FName Card, int32 Owner) const
{
	for (const FZoogiFxArmed& F : Active) if (F.Card == Card && F.Owner == Owner && !F.bConsumed) return &F;
	return nullptr;
}

TArray<FZoogiFxArmed> FZoogiCardEffectCore::Expire(EZoogiCardExpiry Kind, int32 Owner)
{
	TArray<FZoogiFxArmed> Out;
	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		const FZoogiFxArmed& F = Active[i];
		const bool bKind = F.Expiry == Kind || (Kind == EZoogiCardExpiry::LeadActionEnd && (F.Expiry == EZoogiCardExpiry::FirstQualifiedContactOrLeadActionEnd || F.Expiry == EZoogiCardExpiry::CommitPreparationThenLeadAction));
		if (bKind && (Owner < 0 || F.Owner == Owner)) { Out.Insert(F, 0); Active.RemoveAt(i); }
	}
	return Out;
}

void FZoogiCardEffectCore::Remove(int64 EffectId)
{
	Active.RemoveAll([EffectId](const FZoogiFxArmed& F) { return F.EffectId == EffectId; });
}

static uint64 PairKey(int32 A, int32 B)
{
	const uint32 Lo = (uint32)FMath::Min(A, B), Hi = (uint32)FMath::Max(A, B);
	return ((uint64)Hi << 32) | Lo;
}

int32 FZoogiCardEffectCore::ContactEpisode(int32 A, int32 B, float Now)
{
	FPairT& P = Pairs.FindOrAdd(PairKey(A, B));
	if (Now - P.Last > ZoogiCardFxTune::EpisodeGapS) ++P.Episode;
	P.Last = Now;
	return P.Episode;
}

uint64 FZoogiCardEffectCore::ContactId(int32 A, int32 B, int32 Episode)
{
	// order independent: both sides of one physical contact produce the same id
	const uint32 Lo = (uint32)(FMath::Min(A, B) & 0xFFFF), Hi = (uint32)(FMath::Max(A, B) & 0xFFFF);
	return ((uint64)(uint32)Episode << 32) | ((uint64)Hi << 16) | Lo;
}

static uint64 ApplyKey(int64 EffectId, uint64 ContactId)
{
	return (uint64)EffectId * 0x9E3779B97F4A7C15ull ^ (ContactId + 0x632BE59BD9B4E019ull + ((uint64)EffectId << 6));
}

bool FZoogiCardEffectCore::ApplyOnce(int64 EffectId, uint64 Contact)
{
	bool bDup = false;
	Applied.Add(ApplyKey(EffectId, Contact), &bDup);
	return !bDup;
}

bool FZoogiCardEffectCore::WasApplied(int64 EffectId, uint64 Contact) const { return Applied.Contains(ApplyKey(EffectId, Contact)); }

bool FZoogiCardEffectCore::TryBurst(uint64 Contact)
{
	int32& N = Bursts.FindOrAdd(Contact);
	if (N >= 2) return false;
	++N;
	return true;
}

bool FZoogiCardEffectCore::SaveMass(int32 Player, float Actual)
{
	if (SavedMass.Contains(Player)) return false; // keep the first (real) previous value; never cache a modified one
	SavedMass.Add(Player, Actual);
	return true;
}

bool FZoogiCardEffectCore::RestoreMass(int32 Player, float& Out)
{
	if (const float* M = SavedMass.Find(Player)) { Out = *M; SavedMass.Remove(Player); return true; }
	return false;
}

FIntPoint FZoogiCardEffectCore::Cell(const FVector& P)
{
	return FIntPoint(FMath::FloorToInt(P.X / ZoogiCardFxTune::FrostCell), FMath::FloorToInt(P.Y / ZoogiCardFxTune::FrostCell));
}

int32 FZoogiCardEffectCore::AddFrostLane(const FVector& Start, const FVector& Dir, int32 N)
{
	const FVector D = FVector(Dir.X, Dir.Y, 0.f).GetSafeNormal();
	if (D.IsNearlyZero()) return 0;
	int32 Added = 0;
	for (int32 i = 0; i < N * 2; ++i) // half-cell steps so diagonal lanes are continuous
	{
		bool bDup = false;
		Frost.Add(Cell(Start + D * (ZoogiCardFxTune::FrostCell * 0.5f * (i + 1))), &bDup);
		Added += bDup ? 0 : 1;
	}
	return Added;
}

int32 FZoogiCardEffectCore::ClearFrostNear(const FVector& P, float R)
{
	int32 N = 0;
	for (auto It = Frost.CreateIterator(); It; ++It)
	{
		const FVector C((It->X + 0.5f) * ZoogiCardFxTune::FrostCell, (It->Y + 0.5f) * ZoogiCardFxTune::FrostCell, 0.f);
		const float H = ZoogiCardFxTune::FrostCell * 0.5f;
		const float Dx = FMath::Max(0.f, FMath::Abs(P.X - C.X) - H), Dy = FMath::Max(0.f, FMath::Abs(P.Y - C.Y) - H);
		if (Dx * Dx + Dy * Dy <= R * R) { It.RemoveCurrent(); ++N; } // overlap only
	}
	return N;
}

bool FZoogiCardEffectCore::ScoreOrbOnce(int32 Orb, int32 Life)
{
	bool bDup = false;
	OrbScored.Add(((uint64)(uint32)Life << 32) | (uint32)Orb, &bDup);
	return !bDup;
}

FVector FZoogiCardEffectCore::ScaleDelta(const FVector& Prev, const FVector& Post, float K)
{
	const FVector D = Post - Prev;
	return FVector(Prev.X + D.X * K, Prev.Y + D.Y * K, Post.Z); // planar only; vertical left to physics
}

bool FZoogiCardEffectCore::MomentumSteal(FVector& Shooter, FVector& Victim, const FVector& Normal, float Frac)
{
	const FVector N = FVector(Normal.X, Normal.Y, 0.f).GetSafeNormal();
	const FVector V(Shooter.X, Shooter.Y, 0.f);
	if (N.IsNearlyZero() || V.SizeSquared() < 1.f) return false;
	if (FMath::Abs(FVector::DotProduct(V.GetSafeNormal(), N)) >= ZoogiCardFxTune::GlancingDot) return false;
	const FVector T = (V - N * FVector::DotProduct(V, N)) * FMath::Clamp(Frac, 0.f, 0.25f); // book: <= 25 %
	Shooter -= T; Victim += T;
	return true;
}

FVector2D FZoogiCardEffectCore::ShotScale(const FVector2D& V, float Mul, float Cap)
{
	const float S = V.Size();
	if (S <= 0.f) return V;
	return V / S * FMath::Min(S * Mul, FMath::Max(S, Cap)); // never cut a shot that was already above the cap
}

FVector FZoogiCardEffectCore::MagnetStep(const FVector& OrbP, const FVector& Start, const FVector& Target, float MaxMove, float Accel, float Dt)
{
	const FVector D = FVector(Target.X - OrbP.X, Target.Y - OrbP.Y, 0.f);
	if (FVector::Dist2D(OrbP, Start) >= MaxMove || D.SizeSquared() < 1.f || Dt <= 0.f) return FVector::ZeroVector;
	return D.GetSafeNormal() * (Accel * Dt);
}

uint32 FZoogiCardEffectCore::Hash() const
{
	uint32 H = 0x13579BDF;
	auto Mix = [&H](uint32 V) { H = FCrc::MemCrc32(&V, sizeof(V), H); };
	for (const FZoogiFxArmed& F : Active) { Mix(FCrc::StrCrc32(*F.Card.ToString())); Mix((uint32)F.EffectId); Mix((uint32)F.Owner); Mix(F.bConsumed); Mix((uint32)F.Uses); }
	Mix(Applied.Num()); Mix(Frost.Num()); Mix(OrbScored.Num());
	return H;
}

// ------------------------------------------------------------------ v3 P6 save / resume
void FZoogiCardEffectCore::Serialize(FArchive& Ar)
{
	uint32 Ver = 1; Ar << Ver;
	int32 N = Active.Num(); Ar << N; if (Ar.IsLoading()) Active.SetNum(N);
	for (FZoogiFxArmed& F : Active)
	{
		uint8 Ex = (uint8)F.Expiry;
		Ar << F.Card << F.EffectId << F.Owner << F.Turn << F.A << F.B << Ex << F.bArmed << F.bConsumed << F.bBankSetup << F.Uses;
		F.Expiry = (EZoogiCardExpiry)Ex;
	}
	Ar << Applied << Bursts << SavedMass << Frost << OrbScored;
	int32 NP = Pairs.Num(); Ar << NP;
	if (Ar.IsLoading())
	{
		Pairs.Reset();
		for (int32 i = 0; i < NP; ++i) { uint64 K = 0; FPairT P; Ar << K << P.Episode << P.Last; Pairs.Add(K, P); }
	}
	else for (TPair<uint64, FPairT>& P : Pairs) { uint64 K = P.Key; Ar << K << P.Value.Episode << P.Value.Last; }
}
