import os
B=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
p=B+'Private/ZoogiLoadScreen.cpp'; s=open(p,encoding='utf-8').read()
def ed(old,new,n=1):
    global s; assert s.count(old)==n,(old[:70],s.count(old)); s=s.replace(old,new)
ed('#include "ZoogiArena.h"\n','#include "ZoogiArena.h"\n#include "ZoogiParty.h"\n#include "ZoogiRoyaleMix.h"\n#include "ZoogiTrickShot.h"\n')
ed("			bool bArmed = false, bRaid = false,","			int32 Mode = 0; // b283: EMode\n			bool bArmed = false, bRaid = false,")
ed("		FState S;\n","""		FState S;
		// b283 (Evan: "add loading screens for every mode with updated instructions")
		enum EMode : int32 { MClassic, MRaid, MRoyale, MMulti, MTraining, MKoth, MHotOrb, MOrbStorm, MBossRush, MTrickCourse, MTrickShot, MCount };
		const TCHAR* ModeKey(int32 M) { static const TCHAR* K[] = { TEXT("classic"), TEXT("raid"), TEXT("royale"), TEXT("multi"), TEXT("training"), TEXT("koth"), TEXT("hotorb"), TEXT("orbstorm"), TEXT("bossrush"), TEXT("trickcourse"), TEXT("trickshot") }; return (M >= 0 && M < MCount) ? K[M] : K[0]; }
		const TCHAR* ModeTitle(int32 M) { static const TCHAR* K[] = { TEXT("CLASSIC"), TEXT("RAID"), TEXT("BATTLE ROYALE"), TEXT("MULTIPLAYER"), TEXT("TRAINING"), TEXT("KING OF THE HILL"), TEXT("HOT ORB"), TEXT("ORB STORM"), TEXT("BOSS RUSH"), TEXT("TRICK COURSE"), TEXT("DAILY TRICK SHOT") }; return (M >= 0 && M < MCount) ? K[M] : K[0]; }
""")
# accent
ed("			const FLinearColor Acc = S.bRaid ? Red : Cyan;",
   "			const FLinearColor Acc = S.bRaid ? Red : (S.Mode == MRoyale ? Orange : (S.Mode >= MKoth && S.Mode <= MOrbStorm) ? Gold : Cyan);")
ed("				const FString Kind = S.bRaid ? TEXT(\"RAID\") : TEXT(\"CLASSIC\");",
   "				const FString Kind = ModeTitle(S.Mode); // b283: every mode names itself")
# tips
old_tips_start = "			TArray<FTip> Tips;\n			if (S.bRaid)\n"
i=s.index(old_tips_start); j=s.index("			float Tx0, Ty0, Tw, Th, Gap;", i)
new_tips = """			TArray<FTip> Tips;
			switch (S.Mode)
			{	// b283: updated how-to for every mode
			case MRaid: case MBossRush:
			{
				const bool bDrag = S.Arena == EZoogiArena::Frozen;
				Tips.Add({ TEXT("HIT THE WEAK POINT"), bDrag ? TEXT("Roll into her glowing heart crystal. Every orb you knock out fires a volley at her - star orbs hit harder.") : TEXT("Roll into its glowing weak point. Every orb you knock out fires a volley at it - star orbs hit harder."), Gold });
				Tips.Add({ TEXT("SURVIVE ITS ATTACKS"), TEXT("A red zone or rings = an attack after this turn. Get clear, hide behind cover, or hit the weak point to interrupt it."), Red });
				if (S.Mode == MBossRush) Tips.Add({ TEXT("3 TITANS, ONE POOL OF LIVES"), TEXT("Volcanic Pit, Frozen Ring, then the Tomb - back to back. Lives you lose carry over, so play safe."), Cyan });
				else Tips.Add({ TEXT("KEEP YOUR TEAM LIVES"), FString::Printf(TEXT("5 lives, 10 rounds. Every Zoogi that falls costs one. Beat %s before the rounds run out."), bDrag ? TEXT("her") : TEXT("it")), Cyan });
				break;
			}
			case MRoyale:
				Tips.Add({ TEXT("TAKE YOUR TURNS"), TEXT("Every round plays like Classic: drag back, let go, knock orbs and rivals off the arena for points."), Cyan });
				Tips.Add({ TEXT("NEW ARENA EVERY ROUND"), TEXT("Each round moves to the next arena with its own obstacles and boss - read the map before you shoot."), Orange });
				Tips.Add({ TEXT("POINTS CARRY OVER"), TEXT("Your points add up round after round. The highest total after the last round wins."), Gold });
				break;
			case MMulti:
				Tips.Add({ TEXT("PASS AND PLAY"), TEXT("Take your turn, then hand the phone on. YOUR TURN on a player card shows whose go it is."), Cyan });
				Tips.Add({ TEXT("KNOCK ORBS AND RIVALS OFF"), TEXT("Orbs off the arena score, gold star orbs score more. Knocking a rival over the edge is a big score."), Orange });
				Tips.Add({ TEXT("SKILLS AND REACTION"), TEXT("Pick skills in the menu before you roll. Mid-roll, tap REACTION to pause and redirect, or cast a COMBO skill."), Gold });
				break;
			case MTraining:
				Tips.Add({ TEXT("TRY EVERY SKILL"), TEXT("Open SKILL in the menu and tap one - how it works shows along the bottom. No score, no clock."), Cyan });
				Tips.Add({ TEXT("PRACTISE REACTION"), TEXT("Launch, then tap REACTION mid-roll to pause and redirect into a glowing target."), Gold });
				Tips.Add({ TEXT("ADD RIVALS"), TEXT("Settings - TRAINING BOTS adds CPU Zoogis so you can practise duels and knockouts."), Orange });
				break;
			case MKoth:
				Tips.Add({ TEXT("HOLD THE RING"), TEXT("End your roll inside the centre ring. Every Zoogi sitting in it when the round ends scores 10."), Gold });
				Tips.Add({ TEXT("BUMP THEM OUT"), TEXT("Knock rivals out of the ring - or right off the arena - before the round ends."), Orange });
				Tips.Add({ TEXT("GUARD YOUR SPOT"), TEXT("Skills like ICE WALL and GUARD help you stay in the ring when rivals come for you."), Cyan });
				break;
			case MHotOrb:
				Tips.Add({ TEXT("THE BOMB ORB"), TEXT("One orb glows hot. After 4 turns it blows up - whoever touched it last is blasted toward the edge."), Red });
				Tips.Add({ TEXT("PASS IT ON"), TEXT("Touched it? Knock it into a rival so they touch it last before it blows."), Gold });
				Tips.Add({ TEXT("SCORE AS USUAL"), TEXT("Every other orb you knock off still scores, and knockouts still count."), Cyan });
				break;
			case MOrbStorm:
				Tips.Add({ TEXT("90 SECONDS"), TEXT("The clock ends the match, not the rounds. Shoot fast - every turn counts."), Gold });
				Tips.Add({ TEXT("ORBS KEEP RAINING"), TEXT("New orbs keep dropping onto the court. Aim where they pile up for big chains."), Cyan });
				Tips.Add({ TEXT("MOST POINTS WINS"), TEXT("Orbs off the arena score, gold star orbs score more. Highest score at 0:00 wins."), Orange });
				break;
			case MTrickCourse:
				Tips.Add({ TEXT("FIND THE MARKED ORB"), TEXT("Each hole has one marked orb. Knock it into the hole - nothing else counts."), Gold });
				Tips.Add({ TEXT("FEWEST SHOTS"), TEXT("Every launch is a shot. Use walls and the BANK! preview to line it up."), Cyan });
				Tips.Add({ TEXT("3 HOLES"), TEXT("Solo, no rivals. Your total over all 3 holes is your score - beat your best."), Orange });
				break;
			case MTrickShot:
				Tips.Add({ TEXT("ONE SHOT A TRY"), TEXT("Today's layout is the same for everyone. You get 3 tries - your best one counts."), Gold });
				Tips.Add({ TEXT("STARS"), TEXT("Knock 1 rival off = 1 star, 2 in one shot = 2 stars, all 3 in one shot = 3 stars. Falling off = 0."), Cyan });
				Tips.Add({ TEXT("COME BACK TOMORROW"), TEXT("A new shot every day. Play days in a row - every 7th day pays gems."), Orange });
				break;
			default:
				Tips.Add({ TEXT("KNOCK ORBS OUT"), TEXT("Drag back and let go to roll. Every orb you knock off the arena scores - gold star orbs score more."), Cyan });
				Tips.Add({ TEXT("KNOCK RIVALS OFF"), TEXT("Hit rivals over the edge - a knockout is a big score and they lose their turn."), Orange });
				Tips.Add({ TEXT("SKILLS AND REACTION"), TEXT("Pick skills in the menu before you roll. Mid-roll, tap REACTION to pause and redirect, or cast a COMBO skill."), Gold });
				break;
			}
"""
s=s[:i]+new_tips+s[j:]
ed("""					TEXT("TIP: one ITEM a turn - use it before you roll"),
				};""","""					TEXT("TIP: one ITEM a turn - use it before you roll"),
					TEXT("TIP: tap a skill and how it works shows at the bottom of the screen"),
					TEXT("TIP: SUPERPOWER casts your armed COMBO skills - one per tap"),
					TEXT("TIP: after a COMBO skill mid-roll, REACTION is ready again"),
				};""")
# Arm + Begin
ed("""		S.bArmed = true; S.bRaid = bRaid; S.Map = MapName.ToUpper(); S.Art = Art;
	}""","""		S.bArmed = true; S.bRaid = bRaid; S.Map = MapName.ToUpper(); S.Art = Art;
	}
	void ArmMode()
	{	// b283: any other mode card - the mode is worked out when the match starts
		if (Flag(TEXT("ZoogiNoLoadScreen"))) return;
		S.bArmed = true; S.bRaid = false; S.Map.Reset(); S.Art.Reset();
	}""")
ed("""		S.Arena = G->MenuArena;
		if (S.Art.IsEmpty()) S.Art = ArtFor(S.Arena);""","""		S.Arena = G->MenuArena;
		{	// b283: which mode is this
			int32 Hum = 0; for (int32 i = 0; i < FMath::Min(G->MenuNumPlayers, G->MenuPlayers.Num()); ++i) if (!G->MenuPlayers[i].bCPU) ++Hum;
			const ZoogiParty::EKind Pk = ZoogiParty::Kind();
			S.Mode = Pk == ZoogiParty::EKind::Koth ? MKoth : Pk == ZoogiParty::EKind::HotOrb ? MHotOrb : Pk == ZoogiParty::EKind::OrbStorm ? MOrbStorm
				: Pk == ZoogiParty::EKind::BossRush ? MBossRush : Pk == ZoogiParty::EKind::TrickCourse ? MTrickCourse
				: (S.bRaid || ZoogiRaid::Armed()) ? MRaid : ZoogiRoyaleMix::Active() ? MRoyale : ZoogiTrickShot::Active() ? MTrickShot
				: G->bSoloTraining ? MTraining : Hum > 1 ? MMulti : MClassic;
			FString Fm; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiLoadMode="), Fm)) for (int32 m = 0; m < MCount; ++m) if (Fm == ModeKey(m)) S.Mode = m; // previews
			S.bRaid = S.Mode == MRaid || S.Mode == MBossRush;
			if (S.Map.IsEmpty()) S.Map = ZoogiUnderrealm::Picked() ? FString(TEXT("DARK REALM")) : ZoogiUtil::ArenaName(G->MenuArena).ToUpper();
		}
		if (S.Art.IsEmpty()) S.Art = ArtFor(S.Arena);""")
old_sub = s[s.index("		S.Sub = S.bRaid ?"):s.index("\n", s.index("MOST POINTS WINS\"), G->MenuNumPlayers, G->MenuRounds);"))+1]
new_sub = """		switch (S.Mode)
		{	// b283
		case MRaid: S.Sub = FString::Printf(TEXT("CO-OP  -  YOU + %d ALLIES VS %s"), FMath::Max(0, G->MenuNumPlayers - 1), S.Arena == EZoogiArena::Frozen ? TEXT("THE DRAGON") : TEXT("THE TITAN")); break;
		case MBossRush: S.Sub = FString::Printf(TEXT("CO-OP  -  YOU + %d ALLIES  -  3 TITANS BACK TO BACK"), FMath::Max(0, G->MenuNumPlayers - 1)); break;
		case MRoyale: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  A NEW ARENA EVERY ROUND  -  MOST POINTS WINS"), G->MenuNumPlayers); break;
		case MMulti: S.Sub = FString::Printf(TEXT("%d PLAYERS  -  PASS THE PHONE  -  %d ROUNDS"), G->MenuNumPlayers, G->MenuRounds); break;
		case MTraining: S.Sub = TEXT("PRACTICE  -  NO SCORE, NO CLOCK"); break;
		case MKoth: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  HOLD THE CENTRE RING  -  %d ROUNDS"), G->MenuNumPlayers, G->MenuRounds); break;
		case MHotOrb: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  DON'T TOUCH THE BOMB LAST"), G->MenuNumPlayers); break;
		case MOrbStorm: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  90 SECONDS OF RAINING ORBS"), G->MenuNumPlayers); break;
		case MTrickCourse: S.Sub = TEXT("SOLO  -  3 HOLES  -  FEWEST SHOTS WINS"); break;
		case MTrickShot: S.Sub = TEXT("ONE SHOT  -  3 TRIES  -  UP TO 3 STARS"); break;
		default: S.Sub = FString::Printf(TEXT("%d ZOOGIS  -  %d ROUNDS  -  MOST POINTS WINS"), G->MenuNumPlayers, G->MenuRounds); break;
		}
"""
s=s.replace(old_sub,new_sub)
ed("""		UE_LOG(LogZoogi, Log, TEXT("LOADING SCREEN: %s %s (%s)"), S.bRaid ? TEXT("RAID") : TEXT("CLASSIC"), S.bRaid ? *S.Boss : *S.Map, *S.Art);""",
   """		UE_LOG(LogZoogi, Log, TEXT("LOADING SCREEN: %s %s (%s)"), ModeTitle(S.Mode), S.bRaid ? *S.Boss : *S.Map, *S.Art);""")
ed("""G->TakeShot(S.bRaid ? TEXT("load_raid_a") : TEXT("load_classic_a"));""","""G->TakeShot(FString::Printf(TEXT("load_%s_a"), ModeKey(S.Mode)));""")
ed("""G->TakeShot(S.bRaid ? TEXT("load_raid_b_tap") : TEXT("load_classic_b_tap"));""","""G->TakeShot(FString::Printf(TEXT("load_%s_b_tap"), ModeKey(S.Mode)));""")
open(p,'w',encoding='utf-8').write(s)
# header
h=B+'Public/ZoogiLoadScreen.h'; t=open(h,encoding='utf-8').read()
a="	ZOOGIROLL_API void Arm(bool bRaid, const FString& MapName, const FString& Art);\n"; assert t.count(a)==1
t=t.replace(a,a+"	/** b283: every other mode card (Battle Royale, party modes, training, trick shot) - the mode is read when the match starts. */\n	ZOOGIROLL_API void ArmMode();\n")
open(h,'w',encoding='utf-8').write(t)
# mode select arming
m=B+'Private/ZoogiModeSelect.cpp'; u=open(m,encoding='utf-8').read()
def em(old,new):
    global u; assert u.count(old)==1,(old[:60],u.count(old)); u=u.replace(old,new)
em("		ZoogiParty::Arm(G, K);\n		Fire(\"Mode\", 0);","		ZoogiParty::Arm(G, K);\n		ZoogiLoadScreen::ArmMode(); // b283: a loading screen with this mode's how-to\n		Fire(\"Mode\", 0);")
em("	case 0: // BATTLE ROYALE: 4 Zoogis, you + 3 CPU - Oct 31 (Evan: \"the larger field we are building is for battle royale only\"): on Ancient Collision Meadows\n		Fire(\"Mode\", 2);",
   "	case 0: // BATTLE ROYALE: 4 Zoogis, you + 3 CPU - Oct 31 (Evan: \"the larger field we are building is for battle royale only\"): on Ancient Collision Meadows\n		ZoogiLoadScreen::ArmMode(); // b283\n		Fire(\"Mode\", 2);")
em("		UE_LOG(LogZoogi, Log, TEXT(\"MAP SELECT training: LEAGUE COURT\"));\n		G2->ClassicStartTraining();","		UE_LOG(LogZoogi, Log, TEXT(\"MAP SELECT training: LEAGUE COURT\"));\n		ZoogiLoadScreen::ArmMode(); // b283\n		G2->ClassicStartTraining();")
em("		G->ClassicStartTraining(); // b63: solo on the picked map","		ZoogiLoadScreen::ArmMode(); // b283\n		G->ClassicStartTraining(); // b63: solo on the picked map")
em("	ZoogiTrickShot::Arm(G);\n	Fire(\"Start\", 0);\n}","	ZoogiTrickShot::Arm(G);\n	ZoogiLoadScreen::ArmMode(); // b283\n	Fire(\"Start\", 0);\n}")
open(m,'w',encoding='utf-8').write(u)
print('ok')
