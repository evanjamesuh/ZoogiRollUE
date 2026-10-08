// Episode core: shot_id selection from the spec section 15 library (+ Titan Meteor 11.4 A-E). Deterministic and
// recency-weighted. Every shot is justified by a real event in the truth record (Rule Zero: dramatize, never invent).
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeShots.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiArena.h"
#include "ZoogiRoll.h"

namespace ZoogiEpisodePick
{
	struct FEpShotCand
	{
		const TCHAR* Id;
		EZoogiShotSubject Subj;
	};

	const TCHAR* PowerSymbol(EZoogiArtifact A)
	{
		switch (A)
		{
		case EZoogiArtifact::Storm: return TEXT("power_storm_symbol");
		case EZoogiArtifact::Ember: return TEXT("power_ember_symbol");
		case EZoogiArtifact::Frost: return TEXT("power_frost_symbol");
		case EZoogiArtifact::Titan: return TEXT("power_titan_symbol");
		case EZoogiArtifact::Void: return TEXT("power_void_symbol");
		default: return TEXT("power_signature_setup");
		}
	}

	const TCHAR* PowerArchetype(EZoogiArtifact A)
	{
		switch (A)
		{
		case EZoogiArtifact::Storm: return TEXT("arch_lightning_corridor");
		case EZoogiArtifact::Ember: return TEXT("arch_fire_horizon");
		case EZoogiArtifact::Frost: return TEXT("arch_ice_wall");
		case EZoogiArtifact::Titan: return TEXT("arch_meteor_descent");
		case EZoogiArtifact::Void: return TEXT("arch_void_isolation");
		default: return TEXT("arch_symbolic_wave");
		}
	}

	constexpr EZoogiShotSubject ShA = EZoogiShotSubject::Actor, ShT = EZoogiShotSubject::Target, ShP = EZoogiShotSubject::Partner,
		ShS = EZoogiShotSubject::Secondary, ShW = EZoogiShotSubject::World;
}


int32 UZoogiEpisodeSubsystem::SubjectBody(EZoogiShotSubject S) const
{
	const AZoogiGameMode* G = GM();
	int32 Deep = -1, DeepD = -1;
	for (const FZoogiContactEvent& E : Truth.Contacts)
		if (E.bMeaningful && E.OtherBody >= 0 && E.OtherBody != Truth.Actor && E.Depth + 1 > DeepD) { DeepD = E.Depth + 1; Deep = E.OtherBody; }
	const int32 Tgt = Truth.PowerHitTarget >= 0 ? Truth.PowerHitTarget : (ShooterFirstContact >= 0 ? ShooterFirstContact : Deep);
	switch (S)
	{
	case EZoogiShotSubject::Actor: return Truth.Actor;
	case EZoogiShotSubject::Target: return Tgt >= 0 && Tgt < 100 ? Tgt : -1;
	case EZoogiShotSubject::Secondary: return Deep >= 0 && Deep < 100 && Deep != Tgt ? Deep : -1;
	case EZoogiShotSubject::Partner:
	{
		if (!G) return -1;
		const int32 T = Tgt >= 0 && Tgt < 100 ? Tgt : Truth.Actor;
		if (G->bComboMode) { const int32 P = T ^ 1; return G->Balls.IsValidIndex(P) && G->Balls[P] && !G->Balls[P]->bOut ? P : -1; }
		// classic: another Zoogi watching from elsewhere in the Arena (nearest to the target, not the shooter)
		int32 Best = -1; float BestD = TNumericLimits<float>::Max();
		const FVector TP = G->Balls.IsValidIndex(T) && G->Balls[T] ? G->Balls[T]->BallLocation() : FVector::ZeroVector;
		for (int32 i = 0; i < G->Balls.Num(); ++i)
		{
			if (i == T || i == Truth.Actor || !G->Balls[i] || G->Balls[i]->bOut) continue;
			const float D = FVector::DistSquared(G->Balls[i]->BallLocation(), TP);
			if (D < BestD) { BestD = D; Best = i; }
		}
		return Best;
	}
	default: return -1;
	}
}

void UZoogiEpisodeSubsystem::NoteShots(const FZoogiEpisodeCue& Cue)
{
	for (const FZoogiEpisodeShot& S : Cue.Shots)
	{
		RecentShots.Add(S.ShotId);
		Plan.Shots.Add(S);
	}
	if (RecentShots.Num() > 12) RecentShots.RemoveAt(0, RecentShots.Num() - 12);
}

void UZoogiEpisodeSubsystem::PickShots(FZoogiEpisodeCue& Cue)
{
	using namespace ZoogiEpisodePick;
	const AZoogiGameMode* G = GM();
	const bool bFrozenRing = G && G->Arena && G->Arena->GetTheme() == EZoogiArena::Frozen;
	TArray<FZoogiEpisodeShot>& Out = Cue.Shots;

	auto CharOf = [&](int32 Body) { return (G && Body >= 0 && Body < 100 && G->Balls.IsValidIndex(Body) && G->Balls[Body]) ? G->Balls[Body]->Def.Name.ToLower() : FString(); };
	const uint8 Pow = (uint8)Truth.Awaken;
	// recency weighting: candidate order is preference; a recently used id costs more the newer it is; a pick Comic
	// already delivered an asset for gets a small discount
	auto Add = [&](std::initializer_list<FEpShotCand> Cands, EZoogiEpisodeTag Reason, bool bForce = false) -> bool
	{
		const FEpShotCand* Best = nullptr;
		float BestCost = TNumericLimits<float>::Max();
		int32 Idx = 0;
		for (const FEpShotCand& C : Cands)
		{
			const FName Id(C.Id);
			const int32 Body = C.Subj == ShW ? -1 : SubjectBody(C.Subj);
			float Cost = (float)Idx++;
			if (C.Subj != ShW && Body < 0) continue;                  // no such subject in this action: never invent one
			if (Out.ContainsByPredicate([&](const FZoogiEpisodeShot& S) { return S.ShotId == Id && S.Body == Body; })) continue;
			const int32 Last = RecentShots.FindLast(Id);
			if (Last != INDEX_NONE && !bForce) Cost += 6.f * (float)(Last + 1) / (float)RecentShots.Num() + 1.f;
			if (!ZoogiEpisodeShots::FindAsset(Id, CharOf(Body), Pow).IsNone()) Cost -= 0.5f;
			if (Cost < BestCost) { BestCost = Cost; Best = &C; }
		}
		if (!Best) return false;
		FZoogiEpisodeShot S;
		S.ShotId = FName(Best->Id);
		const ZoogiEpisodeShots::FShotDef* Def = ZoogiEpisodeShots::Find(S.ShotId);
		ensureMsgf(Def, TEXT("Episode shot_id %s missing from the library"), Best->Id);
		S.Family = Def ? FName(Def->Family) : NAME_None;
		S.Subject = Best->Subj;
		S.Body = Best->Subj == ShW ? -1 : SubjectBody(Best->Subj);
		S.SubjectActor = BodyActor(S.Body);
		S.Character = CharOf(S.Body);
		S.AssetKey = ZoogiEpisodeShots::FindAsset(S.ShotId, S.Character, Pow);
		S.Reason = Reason;
		Out.Add(S);
		return true;
	};

	// the motion / world shot that shows a real tag
	auto TagShot = [&](EZoogiEpisodeTag T)
	{
		switch (T)
		{
		case EZoogiEpisodeTag::Bank: case EZoogiEpisodeTag::DoubleBank: return Add({ { TEXT("motion_wall_impact"), ShA }, { TEXT("motion_rebound"), ShA } }, T);
		case EZoogiEpisodeTag::WallRedirect: return Add({ { TEXT("motion_wall_impact"), ShS }, { TEXT("motion_wall_impact"), ShT }, { TEXT("motion_rebound"), ShT } }, T);
		case EZoogiEpisodeTag::AllyBump: return Add({ { TEXT("motion_partner_approach"), ShA }, { TEXT("char_focus"), ShA } }, T);
		case EZoogiEpisodeTag::OrbRelay: return Add({ { TEXT("world_arena_orb"), ShW }, { TEXT("motion_rebound"), ShS } }, T);
		case EZoogiEpisodeTag::BoundarySave: return Add({ { TEXT("world_boundary"), ShW }, { TEXT("motion_skid"), ShT } }, T);
		case EZoogiEpisodeTag::Airborne: return Add({ { TEXT("motion_airborne_drift"), ShT }, { TEXT("motion_airborne_drift"), ShA }, { TEXT("motion_floor_impact"), ShT } }, T);
		case EZoogiEpisodeTag::PowerHit: return Add({ { TEXT("power_collision"), ShT }, { PowerSymbol(Truth.Awaken), ShA } }, T);
		case EZoogiEpisodeTag::GuardBreak: return Add({ { TEXT("motion_guard_impact"), ShT }, { TEXT("power_guard_buildup"), ShT } }, T);
		case EZoogiEpisodeTag::OrbOut: return Add({ { TEXT("world_arena_orb"), ShW }, { TEXT("world_boundary"), ShW } }, T);
		case EZoogiEpisodeTag::RingOut: return Add({ { TEXT("world_boundary"), ShW }, { TEXT("world_ring_mark_break"), ShW } }, T);
		case EZoogiEpisodeTag::Terrain: return bFrozenRing ? Add({ { TEXT("world_cracked_floor"), ShW }, { TEXT("world_environment_shift"), ShW } }, T)
			: Add({ { TEXT("world_environment_shift"), ShW }, { TEXT("world_cracked_floor"), ShW } }, T);
		case EZoogiEpisodeTag::PartnerFollowUp: return Add({ { TEXT("motion_partner_approach"), ShA }, { TEXT("char_rivalry_glance"), ShA } }, T);
		case EZoogiEpisodeTag::NearMiss: return Add({ { TEXT("char_surprise"), ShP }, { TEXT("char_comedy"), ShA }, { TEXT("char_surprise"), ShA } }, T);
		case EZoogiEpisodeTag::FunnyBounce: return Add({ { TEXT("char_comedy"), ShA }, { TEXT("motion_rebound"), ShA }, { TEXT("motion_spin"), ShA } }, T);
		case EZoogiEpisodeTag::OrbAlmostOut: return Add({ { TEXT("world_boundary"), ShW }, { TEXT("world_arena_orb"), ShW } }, T);
		default: return false;
		}
	};
	auto HeadlineTag = [&]() -> EZoogiEpisodeTag
	{
		float Best = -1.f; EZoogiEpisodeTag T = EZoogiEpisodeTag::Num;
		for (int32 i = 0; i < LiveRec.Tags.Num(); ++i) if (LiveRec.TagPoints.IsValidIndex(i) && LiveRec.TagPoints[i] > Best) { Best = LiveRec.TagPoints[i]; T = LiveRec.Tags[i]; }
		return T;
	};
	auto ActorChar = [&]() { return Add({ { TEXT("char_confidence"), ShA }, { TEXT("char_focus"), ShA }, { TEXT("char_eye_closeup"), ShA } }, EZoogiEpisodeTag::Num); };
	auto TargetReact = [&]() { return Add({ { TEXT("char_surprise"), ShT }, { TEXT("char_fear"), ShT }, { TEXT("char_irritation"), ShT } }, EZoogiEpisodeTag::Num); };

	switch (Cue.Kind)
	{
	case EZoogiEpisodeCueKind::Callout:
		TagShot(Cue.Tag);
		break;
	case EZoogiEpisodeCueKind::Tier:
		if (Cue.Tier <= 2) { TagShot(HeadlineTag()); if (Cue.Tier == 2) ActorChar(); }
		break;
	case EZoogiEpisodeCueKind::Camera:
	{
		// the main-view moment: the headline tag on its real body, falling back to the shooter's launch
		if (!TagShot(Cue.Tag != EZoogiEpisodeTag::Num ? Cue.Tag : HeadlineTag())) Add({ { TEXT("motion_launch"), ShA } }, EZoogiEpisodeTag::Num);
		break;
	}
	case EZoogiEpisodeCueKind::Panels:
	{
		const int32 N = FMath::Clamp(Cue.PanelCount, 1, 4);
		ActorChar();                                                     // panel 0: the shooter
		if (Out.Num() < N && !TargetReact()) TagShot(HeadlineTag());     // panel 1: the struck Zoogi reacts
		if (Out.Num() < N)
		{
			if (Cue.Tier >= 4) Add({ { PowerSymbol(Truth.Awaken), ShA }, { TEXT("power_collision"), ShT } }, EZoogiEpisodeTag::PowerHit);
			else
			{
				// panel 2: the strongest real tag not shown yet
				TArray<int32> Order;
				for (int32 i = 0; i < LiveRec.Tags.Num(); ++i) Order.Add(i);
				Order.Sort([&](int32 A, int32 B) { return LiveRec.TagPoints.IsValidIndex(A) && LiveRec.TagPoints.IsValidIndex(B) && LiveRec.TagPoints[A] > LiveRec.TagPoints[B]; });
				for (int32 i : Order) if (TagShot(LiveRec.Tags[i])) break;
			}
		}
		if (Out.Num() < N) Add({ { TEXT("char_rivalry_glance"), ShS }, { TEXT("char_surprise"), ShP }, { TEXT("power_collision"), ShT }, { TEXT("motion_launch"), ShA } }, EZoogiEpisodeTag::Num);
		while (Out.Num() < N && Add({ { TEXT("world_arena_orb"), ShW }, { TEXT("world_boundary"), ShW }, { TEXT("world_resonance_hub"), ShW } }, EZoogiEpisodeTag::Num)) {}
		Out.SetNum(FMath::Min(Out.Num(), N));
		break;
	}
	case EZoogiEpisodeCueKind::Power:
		Add({ { TEXT("power_infuse_buildup"), ShA } }, EZoogiEpisodeTag::PowerHit, true);
		Add({ { PowerSymbol(Truth.Awaken), ShA } }, EZoogiEpisodeTag::PowerHit, true);
		Add({ { TEXT("power_collision"), ShT } }, EZoogiEpisodeTag::PowerHit, true);
		if (Cue.Tier >= 5)
		{
			Add({ { PowerArchetype(Truth.Awaken), ShW } }, EZoogiEpisodeTag::PowerHit, true);
			Add({ { TEXT("arch_allout_finisher"), ShW } }, EZoogiEpisodeTag::PowerHit, true);
		}
		break;
	case EZoogiEpisodeCueKind::Archetype:
		if (Cue.Archetype == EZoogiEpisodeArchetype::TitanMeteor)
		{
			// spec 11.4: build scale in stages, A..E in order; C only if somebody else is really on the field
			Add({ { TEXT("titan_meteor_a_reaction"), ShT } }, EZoogiEpisodeTag::PowerHit, true);
			Add({ { TEXT("titan_meteor_b_eye_panel"), ShT } }, EZoogiEpisodeTag::PowerHit, true);
			Add({ { TEXT("titan_meteor_c_partner_reaction"), ShP } }, EZoogiEpisodeTag::PowerHit, true);
			Add({ { TEXT("titan_meteor_d_silhouette"), ShW } }, EZoogiEpisodeTag::PowerHit, true);
			Add({ { TEXT("titan_meteor_e_scale_reveal"), ShW } }, EZoogiEpisodeTag::PowerHit, true);
		}
		break;
	case EZoogiEpisodeCueKind::Epilogue:
	{
		bool bRivalOut = false;
		for (int32 P : Truth.RingOuts) if (P != Truth.Actor) bRivalOut = true;
		if (bRivalOut)
		{
			Add({ { TEXT("world_boundary"), ShW } }, EZoogiEpisodeTag::RingOut, true);
			Add({ { TEXT("char_celebration"), ShA } }, EZoogiEpisodeTag::RingOut, true);
			Add({ { TEXT("char_fear"), ShT }, { TEXT("char_fear"), ShS } }, EZoogiEpisodeTag::RingOut, true);
		}
		else if (Truth.OrbOuts > 0)
		{
			Add({ { TEXT("world_arena_orb"), ShW } }, EZoogiEpisodeTag::OrbOut, true);
			Add({ { TEXT("char_celebration"), ShA } }, EZoogiEpisodeTag::OrbOut, true);
		}
		else if (Plan.bMeteor)
		{
			Add({ { TEXT("char_exhaustion"), ShT } }, EZoogiEpisodeTag::Num, true);
			Add({ { TEXT("char_rivalry_glance"), ShT } }, EZoogiEpisodeTag::Num, true);
		}
		else ActorChar();
		break;
	}
	default: break;
	}
	// suggested per-shot hold inside the cue
	const float Total = Cue.Seconds > 0.f ? Cue.Seconds : (Cue.Kind == EZoogiEpisodeCueKind::Callout ? Tune.CalloutGap * 2.f : 0.9f);
	for (FZoogiEpisodeShot& S : Out) S.Seconds = Cue.Kind == EZoogiEpisodeCueKind::Panels ? Total : Total / FMath::Max(1, Out.Num());
}
