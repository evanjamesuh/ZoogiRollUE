# B273 (Claude): PARTY MODES + play-style loadouts + Pillar Field / Fault Line rework. Run from Source/ZoogiRoll/Private.
def edit(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:90], s.count(a))
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)

# GameMode: public orb spawn
edit('../Public/ZoogiGameMode.h', [
('	void PublicFinishGame() { FinishGame(); } // b151: RAID ends the match on a win / out of lives',
 '	void PublicFinishGame() { FinishGame(); } // b151: RAID ends the match on a win / out of lives\n	/** b273 PARTY: one more orb at L (ORB STORM rain, TRICK COURSE reset) */\n	class AZoogiOrb* PublicSpawnOrb(const FVector& L, bool bStar);'),
])
s = open('ZoogiGameMode.cpp', encoding='utf-8').read()
s += '''

// b273 PARTY: one more orb (ORB STORM rain, TRICK COURSE reset)
AZoogiOrb* AZoogiGameMode::PublicSpawnOrb(const FVector& L, bool bStar)
{
	UWorld* W = GetWorld(); if (!W) return nullptr;
	const float Decel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiOrb* O = W->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L), SP);
	if (O) { O->InitOrb(OrbPM, Decel); if (bStar) O->MakeStar(); Orbs.Add(O); }
	return O;
}
'''
open('ZoogiGameMode.cpp', 'w', encoding='utf-8').write(s)

# tick hook
edit('ZoogiActionFun.cpp', [
('	ZoogiSpectacle::Tick(G, RealDt); // b268',
 '	ZoogiParty::Tick(G, RealDt); // b273 PARTY MODES\n	ZoogiSpectacle::Tick(G, RealDt); // b268'),
('#include "ZoogiSpectacle.h" // b268', '#include "ZoogiSpectacle.h" // b268\n#include "ZoogiParty.h" // b273'),
])

# raid: boss rush chain + carried lives
edit('ZoogiRaid.cpp', [
('			S.Max = S.HP = FMath::Max(10, Hp); S.Lives = TeamLives;',
 '			S.Max = S.HP = FMath::Max(10, Hp); S.Lives = ZoogiParty::CarryLives() > 0 ? ZoogiParty::CarryLives() : TeamLives; // b273 BOSS RUSH carries lives'),
('		if (S.EndIn >= 0.f) { S.EndIn -= RealDt; if (S.EndIn < 0.f && G->Phase != EZoogiPhase::GameOver) G->PublicFinishGame(); return; }',
 '		if (S.EndIn >= 0.f) { S.EndIn -= RealDt; if (S.EndIn < 0.f && G->Phase != EZoogiPhase::GameOver) { S.bOn = false; if (!ZoogiParty::BossRushNext(G, S.bCleared, S.Lives)) G->PublicFinishGame(); } return; } // b273 BOSS RUSH: the next titan'),
])
s = open('ZoogiRaid.cpp', encoding='utf-8').read()
s = s.replace('#include "ZoogiBounty.h"', '#include "ZoogiBounty.h"\n#include "ZoogiParty.h" // b273', 1)
open('ZoogiRaid.cpp', 'w', encoding='utf-8').write(s)

# mode select: the VOYAGER slot becomes PARTY MODES with 5 sub-cards (the shooter cards are gone)
edit('ZoogiModeSelect.cpp', [
('	enum class ECardId : uint8 { Royale, Multi, Classic, Raid, Training, Story, TrickShot, ShooterHold, ShooterRoyale, ShooterTeam, Voyager }; // b234, b241 Voyager',
 '	enum class ECardId : uint8 { Royale, Multi, Classic, Raid, Training, Story, TrickShot, ShooterHold, ShooterRoyale, ShooterTeam, Voyager, Party, Koth, HotOrb, OrbStorm, BossRush, TrickCourse }; // b234, b241 Voyager, b273 PARTY'),
('''		{ TEXT("ModeSelect/ms_card_voy_hold"), TEXT("HOLD THE LINE"), TEXT("VOYAGER - SURVIVE THE WAVES"), TEXT("Fire"), ECardId::ShooterHold }, // b234 (Evan: "We need a mode for 3rd person shooter")
		{ TEXT("ModeSelect/ms_card_voy_royale"), TEXT("SHOOTER ROYALE"), TEXT("VOYAGER - AI SQUAD ARENA"), TEXT("Shock"), ECardId::ShooterRoyale },
		{ TEXT("ModeSelect/ms_card_voy_team"), TEXT("TEAM DEATHMATCH"), TEXT("VOYAGER - COVER BATTLE"), TEXT("Frost"), ECardId::ShooterTeam },''',
'''		// b273 (Evan: "finish still to do" - the review's new modes): PARTY MODES, a card that opens 5 modes (the old shooter cards are gone)
		{ TEXT("ModeSelect/ms_card_multi"), TEXT("PARTY MODES"), TEXT("5 NEW WAYS TO PLAY"), TEXT("Shock"), ECardId::Party },
		{ TEXT("ModeSelect/ms_card_classic"), TEXT("KING OF THE HILL"), TEXT("HOLD THE CENTRE RING"), TEXT("Fire"), ECardId::Koth },
		{ TEXT("ModeSelect/ms_card_royale"), TEXT("HOT ORB"), TEXT("DON'T TOUCH THE BOMB LAST"), TEXT("Fire"), ECardId::HotOrb },
		{ TEXT("ModeSelect/ms_card_training"), TEXT("ORB STORM"), TEXT("90 SECONDS OF RAINING ORBS"), TEXT("Frost"), ECardId::OrbStorm },
		{ TEXT("ModeSelect/ms_card_raid"), TEXT("BOSS RUSH"), TEXT("3 TITANS, ONE POOL OF LIVES"), TEXT("Void"), ECardId::BossRush },
		{ TEXT("ModeSelect/ms_card_story"), TEXT("TRICK COURSE"), TEXT("3 HOLES - FEWEST SHOTS WINS"), TEXT("Shock"), ECardId::TrickCourse },'''),
('	bool IsVoyMode(ECardId Id) { return Id == ECardId::ShooterHold || Id == ECardId::ShooterRoyale || Id == ECardId::ShooterTeam; }',
 '	bool IsVoyMode(ECardId Id) { return Id == ECardId::Koth || Id == ECardId::HotOrb || Id == ECardId::OrbStorm || Id == ECardId::BossRush || Id == ECardId::TrickCourse; } // b273: the PARTY sub-cards (was the shooter cards)'),
('	S.TitleA = GCountSel ? TEXT("PLAYERS") : bMapSelect ? TEXT("MAP") : GVoySel ? TEXT("VOYAGER") : TEXT("MODE");',
 '	S.TitleA = GCountSel ? TEXT("PLAYERS") : bMapSelect ? TEXT("MAP") : GVoySel ? TEXT("PARTY") : TEXT("MODE");'),
('	if (Id == ZoogiModeSel::ECardId::Voyager)\n	{	// b241: open the VOYAGER modes',
 '''	if (Id != ZoogiModeSel::ECardId::BossRush) ZoogiParty::Arm(G, ZoogiParty::EKind::None); // b273: any other card clears a party mode
	if (ZoogiModeSel::IsVoyMode(Id))
	{	// b273 PARTY MODES
		ZoogiModeSel::GVoySel = false; ZoogiModeSel::GRaidMaps = false; ZoogiModeSel::GMultiN = 0; ZoogiModeSel::GCountSel = false; bMapSelect = false; bTrainingMaps = false;
		ZoogiShooter::SetKind(0);
		const ZoogiParty::EKind K = Id == ZoogiModeSel::ECardId::Koth ? ZoogiParty::EKind::Koth : Id == ZoogiModeSel::ECardId::HotOrb ? ZoogiParty::EKind::HotOrb : Id == ZoogiModeSel::ECardId::OrbStorm ? ZoogiParty::EKind::OrbStorm : Id == ZoogiModeSel::ECardId::BossRush ? ZoogiParty::EKind::BossRush : ZoogiParty::EKind::TrickCourse;
		ZoogiParty::Arm(G, K);
		Fire("Mode", 0);
		if (K == ZoogiParty::EKind::TrickCourse)
		{	// solo, no bots
			if (G->MenuArena != EZoogiArena::Meadow) Fire("Arena", (int32)EZoogiArena::Meadow);
			G->ClassicStartTraining();
			return;
		}
		if (K == ZoogiParty::EKind::BossRush)
		{	// you + 2 CPU allies, the Volcanic Pit titan first
			Fire("Players", 3); CPUs();
			if (G->MenuArena != EZoogiArena::Volcanic) Fire("Arena", (int32)EZoogiArena::Volcanic);
			ZoogiRaid::Arm(G);
			Fire("Start", 0);
			return;
		}
		Fire("Players", 4); CPUs();
		if (G->MenuArena != EZoogiArena::Meadow) Fire("Arena", (int32)EZoogiArena::Meadow);
		if (K == ZoogiParty::EKind::OrbStorm) Fire("Rounds", 30); // the clock ends it, not the rounds
		Fire("Start", 0);
		return;
	}
	if (Id == ZoogiModeSel::ECardId::Party)
	{	// b273: open the PARTY modes (the b241 VOYAGER sub-list)'''),
])
s = open('ZoogiModeSelect.cpp', encoding='utf-8').read()
s = s.replace('#include "ZoogiRaid.h"', '#include "ZoogiRaid.h"\n#include "ZoogiParty.h" // b273', 1) if '#include "ZoogiParty.h"' not in s else s
open('ZoogiModeSelect.cpp', 'w', encoding='utf-8').write(s)
print('p273 part 1 applied')

# play-style loadouts: one tap sets a suggested hand (INVENTORY > SKILLS, landscape)
edit('ZoogiFront.cpp', [
('			ZTextC(Out, G, L + 3, Hc, Hf, FVector2D(X + 10.f + Hw * 0.5f, 159.f), Paper);\n		}',
 """			ZTextC(Out, G, L + 3, Hc, Hf, FVector2D(X + 10.f + Hw * 0.5f, 159.f), Paper);
			if (DH <= DW)
			{	// b273 (skills review "Play styles"): one tap sets a suggested hand
				struct FStyle { const TCHAR* Name; const TCHAR* Ids[5]; };
				static const FStyle Styles[] = {
					{ TEXT("BRUISER"), { TEXT("crescent_slash"), TEXT("glacier_lance"), TEXT("water_cannon"), TEXT("cross_slash"), TEXT("grapple_hook") } },
					{ TEXT("SWEEPER"), { TEXT("energy_shockwave"), TEXT("bolt_balls"), TEXT("gattling_burst"), TEXT("ice_shards"), TEXT("star_magnet") } },
					{ TEXT("FORTRESS"), { TEXT("ice_wall"), TEXT("wood_shield"), TEXT("decoy"), TEXT("edge_save"), TEXT("sapphire_wall") } },
					{ TEXT("TRICKSTER"), { TEXT("apple_bombs"), TEXT("frost_spikes"), TEXT("hex"), TEXT("tether"), TEXT("tide_shift") } },
					{ TEXT("SPEEDSTER"), { TEXT("gattling_beat"), TEXT("grapple_hook"), TEXT("ricochet"), TEXT("bounce_pad"), TEXT("sword_slash") } } };
				const FSlateFontInfo Sf = Font(0, 22.f);
				float Sx = X + 10.f + Hw + 30.f;
				for (const FStyle& St : Styles)
				{
					const float Sw = ZMeasure(St.Name, Sf).X + 26.f;
					ZPanel(Out, G, L, FVector2D(Sx, 139.f), FVector2D(Sw, 40.f), Ink, Mag(), 3.f, 4.f);
					ZTextC(Out, G, L + 3, St.Name, Sf, FVector2D(Sx + Sw * 0.5f, 159.f), Paper);
					const FStyle* Sp = &St;
					AddHit(FVector2D(Sx, 139.f), FVector2D(Sw, 40.f), [Self, Sp]()
					{
						UZoogiFrontProfileSubsystem* Pp = Self->FP(); if (!Pp) return;
						TArray<FName> Hand; for (const TCHAR* Id : Sp->Ids) { const FName N(Id); if (Pp->IsCardOwned(N) && Hand.Num() < UZoogiFrontProfileSubsystem::DeckSize) Hand.AddUnique(N); }
						Pp->SetDeck(Pp->Data().ActiveDeck, Hand);
						Self->Say(FString::Printf(TEXT("%s HAND SET - %d SKILLS"), Sp->Name, Hand.Num()));
						UE_LOG(LogZoogi, Log, TEXT("FRONT play style %s: %d skills"), Sp->Name, Hand.Num());
					});
					Sx += Sw + 10.f;
				}
			}
		}"""),
])

# Pillar Field = pinball pillars, Fault Line = the outer side tips toward the edge
edit('ZoogiSkills.cpp', [
('		enum { NK_EdgeSave = 1, NK_Pad, NK_Rico, NK_Decoy, NK_Tether, NK_Tide, NK_Hex, NK_Magnet };',
 '		enum { NK_EdgeSave = 1, NK_Pad, NK_Rico, NK_Decoy, NK_Tether, NK_Tide, NK_Hex, NK_Magnet, NK_Fault }; // b273 NK_Fault'),
('							if (!Body || !Body->IsSimulatingPhysics() || FVector::Dist2D(L, F.P) > 125.f) return;',
 '							if (!Body || !Body->IsSimulatingPhysics() || FVector::Dist2D(L, F.P) > (F.Sp2 > 0.f ? F.Sp2 : 125.f)) return; // b273: Sp2 = the pillar reach'),
('					else if (F.Kind == NK_Magnet)',
 """					else if (F.Kind == NK_Fault)
					{	// b273 FAULT LINE: for 2.5 s everything on the outer side of the crack slides toward the edge
						if (Nw > F.NextFx) bDrop = true;
						else
						{
							auto Tip = [&](UPrimitiveComponent* Body, const FVector& L) { if (Body && Body->IsSimulatingPhysics() && FVector::DotProduct(L - F.P, F.Dir) > 0.f && FVector::Dist2D(L, F.P) < 1400.f) Body->SetPhysicsLinearVelocity(Body->GetPhysicsLinearVelocity() + F.Dir * 420.f * Dtp); };
							for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Tip(B->Sphere, B->BallLocation());
							for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Tip(O->Sphere, O->OrbLocation());
						}
					}
					else if (F.Kind == NK_Magnet)"""),
('			if (!bOldReview && Id == FName(TEXT("sapphire_wall"))) {',
 """			if (!bOldReview && Id == FName(TEXT("pillar_field")))
				for (const FVector& Sp : P.Spots) { FNewFx F; F.Kind = NK_Pad; F.P = At(Sp); F.Owner = Pi; F.Until = G->TurnCounter + 2 * NPl; F.NextFx = 1e18; F.Sp2 = 185.f; NewFx.Add(F); } // b273: PINBALL pillars - balls bounce off them 40% faster
			if (!bOldReview && Id == FName(TEXT("fault_line")))
			{	// b273: the outer side of the crack tips toward the edge for 2.5 s
				const FVector A0 = At(P.A), B0 = At(P.B), Mid = (A0 + B0) * 0.5f; FVector Dl = (B0 - A0).GetSafeNormal2D(); if (Dl.IsNearlyZero()) Dl = FVector::ForwardVector;
				FVector N(-Dl.Y, Dl.X, 0.f); const FVector C0 = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector; if (FVector::DotProduct(Mid - C0, N) < 0.f) N = -N;
				FNewFx F; F.Kind = NK_Fault; F.P = Mid; F.Dir = N; F.Owner = Pi; F.Until = G->TurnCounter + NPl; F.NextFx = FPlatformTime::Seconds() + 2.5; NewFx.Add(F);
				UE_LOG(LogZoogi, Log, TEXT("SKILL fault line P%d: the outer side tips toward the edge"), Pi + 1);
			}
			if (!bOldReview && Id == FName(TEXT("sapphire_wall"))) {"""),
])
cp = '../../../Content/Data/Cards.csv'
s = open(cp, encoding='utf-8').read()
L = s.split('\n')
for i, l in enumerate(L):
    if l.startswith('pillar_field,'): L[i] = ','.join(l.split(',')[:11]) + ',Tap 3 spots inside your arc range: PINBALL pillars rise for 2 rounds - any ball that hits one bounces off 40% faster.'
    if l.startswith('fault_line,'): L[i] = ','.join(l.split(',')[:11]) + ',Swipe a line inside your arc range: the ground splits and everything on the outer side tips toward the edge for 2.5 s.'
open(cp, 'w', encoding='utf-8').write('\n'.join(L))
print('p273 part 2 applied')
