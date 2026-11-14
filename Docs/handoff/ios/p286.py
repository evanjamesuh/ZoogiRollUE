import os
B=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def edf(f,pairs):
    p=B+f; s=open(p,encoding='utf-8').read()
    for old,new in pairs:
        assert s.count(old)==1,(f,old[:70],s.count(old)); s=s.replace(old,new)
    open(p,'w',encoding='utf-8').write(s)
# IceWall: remember who hit it last
edf('Public/ZoogiIceWall.h',[("	int32 OwnerP = -1;\n","	int32 OwnerP = -1;\n	int32 LastHitP = -1; // b286: the last Zoogi that knocked it (KING OF THE HILL wall-break points)\n")])
edf('Private/ZoogiIceWall.cpp',[("	if (!SegLive.IsValidIndex(Seg) || !SegLive[Seg]) return;\n	const FVector V = PrevVel","	if (!SegLive.IsValidIndex(Seg) || !SegLive[Seg]) return;\n	LastHitP = Ball->PlayerIndex; // b286\n	const FVector V = PrevVel")])
# Party: the fort
edf('Private/ZoogiParty.cpp',[
('#include "ZoogiActionFun.h"\n','#include "ZoogiActionFun.h"\n#include "ZoogiIceWall.h"\n'),
("		const FLinearColor Gold(1.f, 0.8f, 0.25f), Red(1.f, 0.25f, 0.15f);",
"""		// KING OF THE HILL - b286 (Evan: "that mode should be about building barriers and breaking through them amongst the normal chaos"):
		// the centre ring starts walled in (a FORT of ice walls - 3 knocks shatter a wall), every 3rd round the broken fort walls rise again,
		// and a Zoogi that ends its turn in or near the ring raises its own wall between itself and the nearest rival (2 walls each).
		// Breaking a wall someone else built (or the fort) = +5. Off: -ZoogiKothNoFort
		struct FKWall { TWeakObjectPtr<AZoogiIceWall> W; int32 Owner = -1; int32 Slot = -1; bool bPaid = false; };
		TArray<FKWall> KWalls; int32 KLastTurn = -1, KTurnPlayer = -1, KFortRound = -1; int64 KEff = 860000;
		const FLinearColor Gold(1.f, 0.8f, 0.25f), Red(1.f, 0.25f, 0.15f);"""),
("		Stage = 0; Carry = -1; // a new pick from the menu always starts at titan 1",
"		Stage = 0; Carry = -1; // a new pick from the menu always starts at titan 1\n		KWalls.Reset(); KLastTurn = -1; KTurnPlayer = -1; KFortRound = -1;"),
("			if (GKind == EKind::Koth) Callout(G, TEXT(\"KING OF THE HILL\"), TEXT(\"END EACH ROUND IN THE GOLD CENTRE RING = +10\"), Gold);",
"			if (GKind == EKind::Koth) Callout(G, TEXT(\"KING OF THE HILL\"), TEXT(\"BREAK INTO THE FORT - HOLD THE RING = +10 - BUILD WALLS\"), Gold);"),
("""			const float ZoneR = R * 0.3f;
			if (Now > Next) { Next = Now + 1.4; AZoogiFX::SpawnMark(W, C + FVector(0, 0, 5.f), Gold, ZoneR, 1.3f); }""",
"""			const float ZoneR = R * 0.3f;
			if (Now > Next) { Next = Now + 1.4; AZoogiFX::SpawnMark(W, C + FVector(0, 0, 5.f), Gold, ZoneR, 1.3f); }
			static const bool bNoFort = FParse::Param(FCommandLine::Get(), TEXT("ZoogiKothNoFort"));
			if (!bNoFort)
			{
				const float FortR = ZoneR + 90.f, Len = AZoogiIceWall::Tune().Length;
				const int32 NF = FMath::Clamp(FMath::CeilToInt(2.f * PI * FortR / (Len * 0.92f)), 5, 12);
				// the FORT: raised at the start, broken slots rise again every 3rd round
				if (KFortRound < 0 || (G->RoundNumber != KFortRound && (G->RoundNumber - 1) % 3 == 0 && Ph == EZoogiPhase::Aiming))
				{
					const bool bFirst = KFortRound < 0; KFortRound = G->RoundNumber; int32 Raised = 0;
					for (int32 k = 0; k < NF; ++k)
					{
						bool bLive = false;
						for (const FKWall& Kw : KWalls) if (Kw.Slot == k && Kw.W.IsValid() && Kw.W->LiveCount() > 0) bLive = true;
						if (bLive) continue;
						const float A = 2.f * PI * k / NF;
						const FVector Rad(FMath::Cos(A), FMath::Sin(A), 0.f);
						if (AZoogiIceWall* Nw = AZoogiIceWall::SpawnWall(W, C + Rad * FortR + FVector(0, 0, 2.f), Rad, -1, ++KEff)) { KWalls.Add({ Nw, -1, k, false }); ++Raised; }
					}
					if (Raised > 0) { Callout(G, bFirst ? TEXT("THE FORT IS UP") : TEXT("THE FORT REBUILDS"), TEXT("3 KNOCKS SHATTER A WALL - BREAK IN!"), FLinearColor(0.5f, 0.85f, 1.f)); ZoogiUtil::PlaySfx(G, TEXT("RockShatter"), 0.6f, 0.7f); }
					UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH fort: %d / %d walls raised (round %d, r%.0f)"), Raised, NF, G->RoundNumber, FortR);
				}
				// BUILD: a Zoogi that ends its turn in or near the ring raises its own wall toward the nearest rival
				if (Ph == EZoogiPhase::Rolling && G->GetActiveBall()) KTurnPlayer = G->GetActiveBall()->PlayerIndex;
				if (G->TurnCounter != KLastTurn)
				{
					const int32 Was = KTurnPlayer; KLastTurn = G->TurnCounter; KTurnPlayer = -1;
					AZoogiBall* Bb = G->Balls.IsValidIndex(Was) ? G->Balls[Was] : nullptr;
					if (Bb && !Bb->bOut && FVector::Dist2D(Bb->BallLocation(), C) < ZoneR * 1.6f)
					{
						AZoogiBall* Rv = nullptr; float Rd = 1e9f;
						for (AZoogiBall* O : G->Balls) if (O && O != Bb && !O->bOut) { const float D = FVector::Dist2D(O->BallLocation(), Bb->BallLocation()); if (D < Rd) { Rd = D; Rv = O; } }
						FVector Dir = Rv ? Rv->BallLocation() - Bb->BallLocation() : Bb->BallLocation() - C; Dir.Z = 0.f; if (!Dir.Normalize()) Dir = FVector::ForwardVector;
						int32 Mine = 0; for (const FKWall& Kw : KWalls) if (Kw.Owner == Was && Kw.W.IsValid() && Kw.W->LiveCount() > 0) ++Mine;
						if (Mine >= 2) for (FKWall& Kw : KWalls) if (Kw.Owner == Was && Kw.W.IsValid() && Kw.W->LiveCount() > 0) { Kw.bPaid = true; Kw.W->Destroy(); break; } // the oldest goes
						if (AZoogiIceWall* Nw = AZoogiIceWall::SpawnWall(W, Bb->BallLocation() + Dir * 210.f, Dir, Was, ++KEff))
						{
							KWalls.Add({ Nw, Was, -1, false });
							G->PublicPopup(TEXT("BARRIER UP!"), Bb->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.5f, 0.85f, 1.f));
							UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH: P%d built a wall toward %s"), Was + 1, Rv ? *Rv->Def.Name : TEXT("the edge"));
						}
					}
				}
				// BREAK: +5 for shattering a wall you didn't build
				for (FKWall& Kw : KWalls)
				{
					if (Kw.bPaid) continue;
					AZoogiIceWall* Iw = Kw.W.Get();
					if (Iw && Iw->LiveCount() > 0) continue;
					Kw.bPaid = true;
					const int32 By = Iw ? Iw->LastHitP : -1;
					if (By >= 0 && By != Kw.Owner && G->Balls.IsValidIndex(By) && G->Balls[By])
					{
						G->Balls[By]->Score += 5;
						G->PublicPopup(Kw.Owner < 0 ? TEXT("FORT BREACHED +5") : TEXT("WALL SMASHED +5"), G->Balls[By]->BallLocation() + FVector(0, 0, 190.f), Gold);
						UE_LOG(LogZoogi, Log, TEXT("PARTY KOTH: P%d broke %s (+5, score %d)"), By + 1, Kw.Owner < 0 ? TEXT("a fort wall") : *FString::Printf(TEXT("P%d's wall"), Kw.Owner + 1), G->Balls[By]->Score);
					}
				}
			}"""),
])
# loading screen tips
edf('Private/ZoogiLoadScreen.cpp',[(
"""				Tips.Add({ TEXT("HOLD THE RING"), TEXT("End your roll inside the centre ring. Every Zoogi sitting in it when the round ends scores 10."), Gold });
				Tips.Add({ TEXT("BUMP THEM OUT"), TEXT("Knock rivals out of the ring - or right off the arena - before the round ends."), Orange });
				Tips.Add({ TEXT("GUARD YOUR SPOT"), TEXT("Skills like ICE WALL and GUARD help you stay in the ring when rivals come for you."), Cyan });""",
"""				Tips.Add({ TEXT("BREAK INTO THE FORT"), TEXT("The centre ring starts walled in. Three knocks shatter a wall - breaking one you didn't build = +5."), Cyan });
				Tips.Add({ TEXT("HOLD THE RING"), TEXT("Every Zoogi inside the centre ring when the round ends scores 10. Knock rivals out of it."), Gold });
				Tips.Add({ TEXT("BUILD YOUR BARRIER"), TEXT("End your turn in or near the ring and a wall rises between you and the nearest rival (2 walls each)."), Orange });""")])
f=B+'Private/ZoogiFront.cpp'; t=open(f,encoding='utf-8').read(); a='{ TEXT("B285"),'; assert t.count(a)==1
t=t.replace(a,'{ TEXT("B286"), TEXT("KING OF THE HILL: BUILD AND BREAK"), TEXT("King of the Hill is now a siege: the centre ring starts inside a fort of ice walls (3 knocks shatter one, and they rise again every 3 rounds). End your turn in or near the ring and you build your own wall toward the nearest rival. Smash a wall you didn\'t build for +5, and every Zoogi in the ring at the end of a round still scores 10.") },\n'+a)
open(f,'w',encoding='utf-8').write(t)
print('ok')
