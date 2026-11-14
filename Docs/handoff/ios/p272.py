# B272 (Claude): the 9 new skills from the skills review - applied to Source + Content/Data on the PC (run from Source/ZoogiRoll/Private)
import os
P = os.path.dirname(os.path.abspath(__file__)) if False else os.getcwd()

def edit(p, pairs):
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (p, a[:90], s.count(a))
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)

NEW = ['grapple_hook', 'edge_save', 'bounce_pad', 'ricochet', 'decoy', 'tether', 'tide_shift', 'hex', 'star_magnet']

# ---- ZoogiSkills.cpp
edit('ZoogiSkills.cpp', [
# Defs (aim, radius, count, hint) - after sword_slash's def
('			{ TEXT("sword_slash"), EAim::Self, 380.f, 1, TEXT("") },',
 '''			{ TEXT("sword_slash"), EAim::Self, 380.f, 1, TEXT("") },
			// b272 (review "New skills")
			{ TEXT("grapple_hook"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB - YOU ARE YANKED TO IT") },
			{ TEXT("edge_save"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("bounce_pad"), EAim::Spots, 0.f, 1, TEXT("TAP WHERE THE BOUNCE PAD GOES") },
			{ TEXT("ricochet"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("decoy"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("tether"), EAim::MultiTap, 0.f, 2, TEXT("TAP TWO RIVALS TO CHAIN TOGETHER") },
			{ TEXT("tide_shift"), EAim::Area, 300.f, 1, TEXT("DRAG THE CIRCLE - THE ARENA TILTS THAT WAY") },
			{ TEXT("hex"), EAim::Tap, 0.f, 1, TEXT("TAP THE RIVAL TO HEX") },
			{ TEXT("star_magnet"), EAim::Self, 0.f, 1, TEXT("") },'''),
# state
('		TMap<TWeakObjectPtr<AZoogiBall>, double> PlusWallCool;\n',
 '''		TMap<TWeakObjectPtr<AZoogiBall>, double> PlusWallCool;
		// b272 new skills
		struct FNewFx { int32 Kind = 0; TWeakObjectPtr<AZoogiBall> B, B2; FVector P = FVector::ZeroVector, Dir = FVector::ZeroVector, PrevV = FVector::ZeroVector; int32 Owner = -1, Until = 0; double NextFx = 0.0; float Sp1 = 0.f, Sp2 = 0.f; bool bArmed = false; };
		enum { NK_EdgeSave = 1, NK_Pad, NK_Rico, NK_Decoy, NK_Tether, NK_Tide, NK_Hex, NK_Magnet };
		TArray<FNewFx> NewFx;
		TMap<TWeakObjectPtr<AActor>, double> PadCool;
'''),
])
s = open('ZoogiSkills.cpp', encoding='utf-8').read()
assert s.count('PlusPrevVel.Reset(); PlusWallCool.Reset(); /* b271 */') == 2
s = s.replace('PlusPrevVel.Reset(); PlusWallCool.Reset(); /* b271 */', 'PlusPrevVel.Reset(); PlusWallCool.Reset(); NewFx.Reset(); PadCool.Reset(); /* b271 / b272 */')
open('ZoogiSkills.cpp', 'w', encoding='utf-8').write(s)

edit('ZoogiSkills.cpp', [
# execute
('		if (!bOldReview && Id == FName(TEXT("frost_spikes")))',
 r'''		{	// b272 (Evan: "build everything as mentioned in the review doc"): the 9 new skills
			const FLinearColor Cy(0.5f, 0.85f, 1.f);
			auto Done = [&]() { GLastCastAt = FPlatformTime::Seconds(); };
			auto FirstRival = [&]() -> AZoogiBall* { for (const TWeakObjectPtr<AActor>& T : P.Targets) if (AZoogiBall* Rb = Cast<AZoogiBall>(T.Get())) if (Rb->PlayerIndex != Pi && !Rb->bOut) return Rb; return nullptr; };
			auto NearestRival = [&](const AZoogiBall* Not) -> AZoogiBall* { AZoogiBall* Best = nullptr; float Bd = 1e9f; for (AZoogiBall* Rb : G->Balls) if (Rb && Rb != Caster && Rb != Not && !Rb->bOut) { const float D = FVector::Dist2D(Rb->BallLocation(), Me); if (D < Bd) { Bd = D; Best = Rb; } } return Best; };
			if (Id == FName(TEXT("grapple_hook")))
			{
				AActor* T = P.Targets.Num() ? P.Targets[0].Get() : nullptr;
				const FVector Tp = T ? At(ActorPos(T)) : At(P.Ctr);
				FVector D = Tp - Me; D.Z = 0.f; const float Ds = D.Size(); D = Ds > 1.f ? D / Ds : FVector::ForwardVector;
				if (Caster->Sphere) { Caster->Sphere->WakeAllRigidBodies(); Caster->Sphere->SetPhysicsLinearVelocity(D * FMath::Clamp(900.f + Ds * 0.9f, 1200.f, 2200.f) + FVector(0, 0, 60.f)); }
				for (int32 k = 1; k <= 6; ++k) AZoogiFX::Spawn(W, FMath::Lerp(Me, Tp, k / 7.f) + FVector(0, 0, 40.f), Cy, 40.f, 0.35f, false, 10.f);
				ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.8f, 1.4f); ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.9f, 1.f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL grapple hook P%d: yanked %.0f cm"), Pi + 1, Ds);
				Done(); return;
			}
			if (Id == FName(TEXT("edge_save")))
			{
				FNewFx F; F.Kind = NK_EdgeSave; F.B = Caster; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, FLinearColor(0.45f, 1.f, 0.55f), 140.f, 0.5f, true, 9.f);
				G->PublicPopup(TEXT("EDGE SAVE READY"), Me + FVector(0, 0, 190.f), FLinearColor(0.45f, 1.f, 0.55f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL edge save armed P%d"), Pi + 1);
				Done(); return;
			}
			if (Id == FName(TEXT("bounce_pad")))
			{
				FNewFx F; F.Kind = NK_Pad; F.P = At(P.Spots.Num() ? P.Spots[0] : P.Ctr); F.Owner = Pi; F.Until = G->TurnCounter + 2 * NPl; NewFx.Add(F);
				AZoogiFX::SpawnMark(W, F.P + FVector(0, 0, 4.f), FLinearColor(1.f, 0.7f, 0.25f), 120.f, 2.f);
				ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.6f, 1.3f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL bounce pad P%d at (%.0f, %.0f)"), Pi + 1, F.P.X, F.P.Y);
				Done(); return;
			}
			if (Id == FName(TEXT("ricochet")))
			{
				FNewFx F; F.Kind = NK_Rico; F.B = Caster; F.Owner = Pi; F.Until = G->TurnCounter; F.PrevV = Caster->GetBallVelocity(); NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, FLinearColor(1.f, 0.45f, 0.25f), 130.f, 0.4f, true, 9.f);
				G->PublicPopup(TEXT("RICOCHET: NEXT WALL x2"), Me + FVector(0, 0, 190.f), FLinearColor(1.f, 0.5f, 0.3f));
				Done(); return;
			}
			if (Id == FName(TEXT("decoy")))
			{
				FNewFx F; F.Kind = NK_Decoy; F.P = Me; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, ZoogiUtil::PlayerColor(Pi), ZoogiConst::ZoogiRadius * 1.3f, 0.6f, false, 10.f);
				G->PublicPopup(TEXT("DECOY"), Me + FVector(0, 0, 190.f), FLinearColor(0.7f, 0.5f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("SKILL decoy P%d at (%.0f, %.0f)"), Pi + 1, Me.X, Me.Y);
				Done(); return;
			}
			if (Id == FName(TEXT("tether")))
			{
				AZoogiBall* A1 = FirstRival(); AZoogiBall* A2 = nullptr;
				for (const TWeakObjectPtr<AActor>& T : P.Targets) if (AZoogiBall* Rb = Cast<AZoogiBall>(T.Get())) if (Rb != A1 && Rb->PlayerIndex != Pi && !Rb->bOut) { A2 = Rb; break; }
				if (!A1) A1 = NearestRival(nullptr);
				if (!A2) A2 = NearestRival(A1);
				if (A1 && A2)
				{
					FNewFx F; F.Kind = NK_Tether; F.B = A1; F.B2 = A2; F.Owner = Pi; F.Until = G->TurnCounter + NPl; F.Sp1 = A1->GetBallVelocity().Size2D(); F.Sp2 = A2->GetBallVelocity().Size2D(); NewFx.Add(F);
					ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.6f, 0.8f);
					UE_LOG(LogZoogi, Log, TEXT("SKILL tether P%d: P%d <-> P%d"), Pi + 1, A1->PlayerIndex + 1, A2->PlayerIndex + 1);
				}
				Done(); return;
			}
			if (Id == FName(TEXT("tide_shift")))
			{
				const FVector Ctr0 = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
				FVector D = At(P.Ctr) - FVector(Ctr0.X, Ctr0.Y, FZ); D.Z = 0.f; if (!D.Normalize()) D = (At(P.Ctr) - Me).GetSafeNormal2D();
				FNewFx F; F.Kind = NK_Tide; F.Dir = D; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(TEXT("TIDE SHIFT!"), TEXT("THE ARENA TILTS FOR A ROUND - EVERYTHING DRIFTS"), Cy, 1.8f, 1.f);
				ZoogiUtil::PlaySfx(G, TEXT("Rumble"), 0.7f, 1.2f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL tide shift P%d dir (%.2f, %.2f)"), Pi + 1, D.X, D.Y);
				Done(); return;
			}
			if (Id == FName(TEXT("hex")))
			{
				AZoogiBall* V = FirstRival(); if (!V) V = NearestRival(nullptr);
				if (V)
				{
					FNewFx F; F.Kind = NK_Hex; F.B = V; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
					AZoogiFX::Spawn(W, V->BallLocation(), FLinearColor(0.75f, 0.25f, 1.f), 150.f, 0.6f, true, 10.f);
					G->PublicPopup(TEXT("HEXED: NO AIM LINE, 70% POWER"), V->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.8f, 0.4f, 1.f));
					UE_LOG(LogZoogi, Log, TEXT("SKILL hex P%d on P%d"), Pi + 1, V->PlayerIndex + 1);
				}
				Done(); return;
			}
			if (Id == FName(TEXT("star_magnet")))
			{
				FNewFx F; F.Kind = NK_Magnet; F.B = Caster; F.Owner = Pi; F.Until = G->TurnCounter + NPl; NewFx.Add(F);
				AZoogiFX::Spawn(W, Me, FLinearColor(1.f, 0.85f, 0.3f), 200.f, 0.5f, true, 10.f);
				G->PublicPopup(TEXT("STAR MAGNET"), Me + FVector(0, 0, 190.f), FLinearColor(1.f, 0.85f, 0.3f));
				Done(); return;
			}
		}
		if (!bOldReview && Id == FName(TEXT("frost_spikes")))'''),
# tick
('			PlusWalls.RemoveAll([Tn](const FPlusWall& X) { return X.Until < Tn; });\n',
 r'''			PlusWalls.RemoveAll([Tn](const FPlusWall& X) { return X.Until < Tn; });
			{	// b272 new skills
				NewFx.RemoveAll([Tn](const FNewFx& X) { return X.Until < Tn; });
				const FVector Ac = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
				for (int32 k = NewFx.Num() - 1; k >= 0; --k)
				{
					FNewFx& F = NewFx[k]; bool bDrop = false;
					if (F.Kind == NK_EdgeSave)
					{
						AZoogiBall* B = F.B.Get(); if (!B || B->bOut || !B->Sphere) continue;
						const FVector V = B->GetBallVelocity(); const FVector2D V2(V.X, V.Y);
						if (V2.Size() > 180.f && G->Arena && G->Arena->DistanceToEdge(FVector2D(B->BallLocation()), V2.GetSafeNormal()) < 150.f)
						{
							FVector In = Ac - B->BallLocation(); In.Z = 0.f; In.Normalize();
							B->Sphere->SetPhysicsLinearVelocity(In * FMath::Max(500.f, V2.Size() * 0.6f) + FVector(0, 0, 120.f));
							AZoogiFX::Spawn(Wp, B->BallLocation(), FLinearColor(0.45f, 1.f, 0.55f), 160.f, 0.5f, true, 12.f);
							G->PublicPopup(TEXT("EDGE SAVE!"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.45f, 1.f, 0.55f));
							ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.8f, 1.1f);
							UE_LOG(LogZoogi, Log, TEXT("SKILL edge save: P%d pulled back from the edge"), B->PlayerIndex + 1);
							bDrop = true;
						}
					}
					else if (F.Kind == NK_Pad)
					{
						if (Nw > F.NextFx) { F.NextFx = Nw + 0.9; AZoogiFX::SpawnMark(Wp, F.P + FVector(0, 0, 4.f), FLinearColor(1.f, 0.7f, 0.25f), 120.f, 1.f); }
						auto Bounce = [&](AActor* A, UPrimitiveComponent* Body, const FVector& L)
						{
							if (!Body || !Body->IsSimulatingPhysics() || FVector::Dist2D(L, F.P) > 125.f) return;
							const FVector V = Body->GetPhysicsLinearVelocity(); if (V.Size2D() < 80.f) return;
							double& Cd = PadCool.FindOrAdd(A); if (Nw - Cd < 0.6) return; Cd = Nw;
							FVector N = L - F.P; N.Z = 0.f; if (!N.Normalize()) N = -V.GetSafeNormal2D();
							FVector R = V; if (FVector::DotProduct(V, N) < 0.f) R = V - 2.f * FVector::DotProduct(V, N) * N;
							R.Z = 0.f; R = R.GetClampedToMaxSize(1850.f) * 1.4f; R.Z = 150.f;
							Body->SetPhysicsLinearVelocity(R);
							AZoogiFX::Spawn(Wp, F.P + FVector(0, 0, 6.f), FLinearColor(1.f, 0.7f, 0.25f), 140.f, 0.3f, true, 11.f);
							ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.5f, 1.5f);
						};
						for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Bounce(B, B->Sphere, B->BallLocation());
						for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Bounce(O, O->Sphere, O->OrbLocation());
					}
					else if (F.Kind == NK_Rico)
					{
						AZoogiBall* B = F.B.Get(); if (!B || B->bOut || !B->Sphere) { bDrop = true; }
						else
						{
							const FVector V = B->GetBallVelocity(); const FVector V0 = F.PrevV; F.PrevV = V;
							bool bNearBall = false; for (AZoogiBall* O2 : G->Balls) if (O2 && O2 != B && !O2->bOut && FVector::Dist2D(O2->BallLocation(), B->BallLocation()) < 2.4f * ZoogiConst::ZoogiRadius) bNearBall = true;
							if (!bNearBall && V0.Size2D() > 300.f && V.Size2D() > 100.f && FVector::DotProduct(V0.GetSafeNormal2D(), V.GetSafeNormal2D()) < 0.35f)
							{
								FVector R = V * 2.f; R = R.GetClampedToMaxSize(2600.f); B->Sphere->SetPhysicsLinearVelocity(R);
								AZoogiFX::Spawn(Wp, B->BallLocation(), FLinearColor(1.f, 0.45f, 0.25f), 150.f, 0.35f, true, 12.f);
								G->PublicPopup(TEXT("RICOCHET x2!"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(1.f, 0.5f, 0.3f));
								ZoogiUtil::PlaySfx(G, TEXT("Launch"), 0.8f, 1.3f);
								UE_LOG(LogZoogi, Log, TEXT("SKILL ricochet: P%d off a wall at x2"), B->PlayerIndex + 1);
								bDrop = true;
							}
						}
					}
					else if (F.Kind == NK_Decoy)
					{
						if (Nw > F.NextFx) { F.NextFx = Nw + 0.5; AZoogiFX::Spawn(Wp, F.P, ZoogiUtil::PlayerColor(F.Owner).CopyWithNewOpacity(0.6f), ZoogiConst::ZoogiRadius * 1.1f, 0.45f, false, 6.f); }
						for (AZoogiBall* R : G->Balls)
						{
							if (!R || R->bOut || R->PlayerIndex == F.Owner || !R->Sphere) continue;
							if (FVector::Dist2D(R->BallLocation(), F.P) > 2.f * ZoogiConst::ZoogiRadius + 20.f || R->GetBallVelocity().Size2D() < 150.f) continue;
							R->Sphere->SetPhysicsLinearVelocity(R->GetBallVelocity() * 0.05f);
							AZoogiFX::Spawn(Wp, F.P, FLinearColor(0.7f, 0.5f, 1.f), 180.f, 0.4f, true, 12.f);
							G->PublicPopup(TEXT("DECOY! STOPPED DEAD"), F.P + FVector(0, 0, 190.f), FLinearColor(0.7f, 0.5f, 1.f));
							ZoogiUtil::PlaySfx(G, TEXT("HitCrush"), 0.7f, 0.8f);
							UE_LOG(LogZoogi, Log, TEXT("SKILL decoy: P%d stopped dead"), R->PlayerIndex + 1);
							bDrop = true; break;
						}
					}
					else if (F.Kind == NK_Tether)
					{
						AZoogiBall* A1 = F.B.Get(); AZoogiBall* A2 = F.B2.Get();
						if (!A1 || !A2 || A1->bOut || A2->bOut || !A1->Sphere || !A2->Sphere) { bDrop = true; }
						else
						{
							if (Nw > F.NextFx) { F.NextFx = Nw + 0.35; for (int32 j = 1; j <= 4; ++j) AZoogiFX::Spawn(Wp, FMath::Lerp(A1->BallLocation(), A2->BallLocation(), j / 5.f), FLinearColor(0.5f, 0.85f, 1.f), 22.f, 0.3f, false, 6.f); }
							const float S1 = A1->GetBallVelocity().Size2D(), S2 = A2->GetBallVelocity().Size2D();
							AZoogiBall* Hit = (F.Sp1 < 220.f && S1 > 520.f) ? A1 : (F.Sp2 < 220.f && S2 > 520.f) ? A2 : nullptr;
							F.Sp1 = S1; F.Sp2 = S2;
							if (Hit)
							{
								AZoogiBall* Other = Hit == A1 ? A2 : A1;
								Other->Sphere->WakeAllRigidBodies();
								Other->Sphere->SetPhysicsLinearVelocity(Hit->GetBallVelocity() * 0.85f + FVector(0, 0, 80.f));
								Other->LastHitPlayer = F.Owner; Hit->LastHitPlayer = F.Owner;
								G->PublicPopup(TEXT("TETHER!"), Other->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.5f, 0.85f, 1.f));
								ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 0.8f, 1.f);
								UE_LOG(LogZoogi, Log, TEXT("SKILL tether: P%d hit - P%d dragged after it"), Hit->PlayerIndex + 1, Other->PlayerIndex + 1);
								bDrop = true;
							}
						}
					}
					else if (F.Kind == NK_Tide)
					{
						if (Nw > F.NextFx) { F.NextFx = Nw + 0.9; for (int32 j = -1; j <= 1; ++j) AZoogiFX::Spawn(Wp, Ac + F.Dir * 600.f * j + FVector(0, 0, 8.f), FLinearColor(0.5f, 0.8f, 1.f), 90.f, 0.6f, true, 5.f); }
						auto Drift = [&](UPrimitiveComponent* Body) { if (Body && Body->IsSimulatingPhysics()) Body->SetPhysicsLinearVelocity(Body->GetPhysicsLinearVelocity() + F.Dir * 75.f * Dtp); };
						for (AZoogiBall* B : G->Balls) if (B && !B->bOut) Drift(B->Sphere);
						for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) Drift(O->Sphere);
					}
					else if (F.Kind == NK_Magnet)
					{
						AZoogiBall* C = F.B.Get(); if (!C || C->bOut) { bDrop = true; }
						else for (AZoogiOrb* O : G->Orbs)
						{
							if (!O || O->bOut || !O->bStar || !O->Sphere || !O->Sphere->IsSimulatingPhysics()) continue;
							FVector D = C->BallLocation() - O->OrbLocation(); D.Z = 0.f; if (D.Size() < 150.f) continue;
							O->Sphere->SetPhysicsLinearVelocity(O->Sphere->GetPhysicsLinearVelocity() + D.GetSafeNormal() * 140.f * Dtp);
						}
					}
					if (bDrop) NewFx.RemoveAt(k);
				}
			}
'''),
# public: hex queries
('	float SkillPow() { return GSkillPow; }',
 '''	bool Hexed(const AZoogiGameMode* G, int32 Player) { if (!G) return false; for (const FNewFx& F : NewFx) if (F.Kind == NK_Hex && F.Until >= G->TurnCounter && F.B.IsValid() && F.B->PlayerIndex == Player) return true; return false; } // b272 HEX
	float SkillPow() { return GSkillPow; }'''),
])

edit('../Public/ZoogiSkills.h', [
('	ZOOGIROLL_API bool GuardBusy(const AZoogiGameMode* G, int32 Player);',
 '	ZOOGIROLL_API bool GuardBusy(const AZoogiGameMode* G, int32 Player);\n	/** b272 HEX: this player\'s next turn has no aim line and 70% max power */\n	ZOOGIROLL_API bool Hexed(const AZoogiGameMode* G, int32 Player);'),
])

# HEX hooks: no aim line for a hexed human, 70% power cap on the shot
edit('ZoogiTargeting.cpp', [
('	AZoogiBall* Me = GetActiveBall();\n	if (!Me || !bAimActive) return;\n	float Frac = 0.f;',
 '	AZoogiBall* Me = GetActiveBall();\n	if (!Me || !bAimActive) return;\n	if (!Me->bCPU && ZoogiSkills::Hexed(this, Me->PlayerIndex)) return; // b272 HEX: no aim line this turn\n	float Frac = 0.f;'),
])
s = open('ZoogiTargeting.cpp', encoding='utf-8').read()
if '#include "ZoogiSkills.h"' not in s:
    s = s.replace('#include "ZoogiGameMode.h"', '#include "ZoogiGameMode.h"\n#include "ZoogiSkills.h" // b272', 1)
    open('ZoogiTargeting.cpp', 'w', encoding='utf-8').write(s)
edit('ZoogiGameMode.cpp', [
('void AZoogiGameMode::FireShot(const FVector2D& Velocity)\n{\n	AZoogiBall* Ball = GetActiveBall();\n	if (!Ball) return;',
 'void AZoogiGameMode::FireShot(const FVector2D& InVelocity)\n{\n	AZoogiBall* Ball = GetActiveBall();\n	if (!Ball) return;\n	FVector2D Velocity = InVelocity;\n	if (ZoogiSkills::Hexed(this, Ball->PlayerIndex)) { const float Cap = ZoogiConst::LaunchSpeedForPull(0.7f); if (Velocity.Size() > Cap) { Velocity = Velocity.GetSafeNormal() * Cap; UE_LOG(LogZoogi, Log, TEXT("HEX: P%d shot capped at 70%% power"), Ball->PlayerIndex + 1); } } // b272'),
])

s = open('ZoogiGameMode.cpp', encoding='utf-8').read()
if '#include "ZoogiSkills.h"' not in s:
    s = s.replace('#include "ZoogiGameMode.h"', '#include "ZoogiGameMode.h"\n#include "ZoogiSkills.h" // b272 HEX', 1)
    open('ZoogiGameMode.cpp', 'w', encoding='utf-8').write(s)

# CPU
edit('ZoogiCPUSmart.cpp', [
('		else if (K == TEXT("fault_line")) V = Ag * (S.RivalsIn ? 40.f + 25.f * RivalEdgy : (S.OrbsIn ? 26.f : 0.f));',
 '''		else if (K == TEXT("fault_line")) V = Ag * (S.RivalsIn ? 40.f + 25.f * RivalEdgy : (S.OrbsIn ? 26.f : 0.f));
		// b272 new skills
		else if (K == TEXT("grapple_hook")) V = Ag * (S.RivalsIn ? 34.f + 20.f * RivalEdgy : (S.OrbsIn ? 18.f : 0.f));
		else if (K == TEXT("edge_save")) V = Df * (10.f + 60.f * Exposed);
		else if (K == TEXT("bounce_pad")) V = (S.OrbsIn + S.RivalsIn) ? 26.f : 10.f;
		else if (K == TEXT("ricochet")) V = Ag * (S.MyEdge < 1200.f ? 30.f : 18.f);
		else if (K == TEXT("decoy")) V = Df * (RivalClose > 0.3f ? 25.f + 40.f * RivalClose : 6.f);
		else if (K == TEXT("tether")) V = S.RivalsAlive >= 2 ? Ag * (30.f + 20.f * RivalEdgy) : 0.f;
		else if (K == TEXT("tide_shift")) V = S.RivalsAlive ? 22.f + 20.f * RivalEdgy : 0.f;
		else if (K == TEXT("hex")) V = S.RivalsAlive ? 28.f : 0.f;
		else if (K == TEXT("star_magnet")) V = 20.f;'''),
('		if (K == TEXT("cross_slash")) return true; // b271: the counter goes up straight away',
 '		if (K == TEXT("cross_slash")) return true; // b271: the counter goes up straight away\n		if (K == TEXT("edge_save") || K == TEXT("decoy") || K == TEXT("hex") || K == TEXT("star_magnet") || K == TEXT("tide_shift") || K == TEXT("tether") || K == TEXT("ricochet") || K == TEXT("bounce_pad")) return true; // b272\n		if (K == TEXT("grapple_hook")) return S.RivalsIn + S.OrbsIn > 0;'),
])

# new skills are everyone's from the start
edit('ZoogiFrontProfile.cpp', [
('	if (Id.ToString().StartsWith(TEXT("gattling_"))) return true;',
 '	if (Id.ToString().StartsWith(TEXT("gattling_"))) return true;\n	{ static const TSet<FName> NewSk = { FName(TEXT("grapple_hook")), FName(TEXT("edge_save")), FName(TEXT("bounce_pad")), FName(TEXT("ricochet")), FName(TEXT("decoy")), FName(TEXT("tether")), FName(TEXT("tide_shift")), FName(TEXT("hex")), FName(TEXT("star_magnet")) }; if (NewSk.Contains(Id)) return true; } // b272: the review\'s new skills are everyone\'s'),
])

# Cards.csv
cp = '../../../Content/Data/Cards.csv'
s = open(cp, encoding='utf-8').read().rstrip('\n')
rows = [
 'grapple_hook,GRAPPLE HOOK,2,Utility,ActingZoogi,LeadActionEnd,0,0,0,true,Shock,Tap a rival or an orb inside your arc range (also mid-roll): your Zoogi is yanked toward it and slingshots past.',
 'edge_save,EDGE SAVE,2,Defense,ActingZoogi,LeadActionEnd,0,0,0,true,Neutral,Cast before or during your roll: if you are about to roll off the edge before your next turn, a vine pulls you back onto the court.',
 'bounce_pad,BOUNCE PAD,2,Terrain,ActingZoogi,LeadActionEnd,0,0,0,true,Neutral,Tap a spot inside your arc range: a bounce pad for 2 rounds - anything that rolls onto it rebounds 40% faster.',
 'ricochet,RICOCHET,1,Infusion,ActingZoogi,TurnEnd,2,0,0,true,Fire,Cast on yourself: the next wall you hit this turn doubles your speed instead of slowing you.',
 'decoy,DECOY,2,Defense,ActingZoogi,LeadActionEnd,0,0,0,true,Void,Cast on yourself: a ghost of your Zoogi stays where you are - the next rival who rolls into it is stopped dead.',
 'tether,TETHER,2,Control,ActingZoogi,LeadActionEnd,0,0,0,true,Shock,Tap two rivals: they are chained until your next turn - hit one and the other is dragged after it.',
 'tide_shift,TIDE SHIFT,3,Terrain,ActingZoogi,LeadActionEnd,0,0,0,true,Frost,Drag the circle toward a side: the arena tilts that way for a round - every ball and orb drifts toward it.',
 'hex,HEX,2,Control,ActingZoogi,LeadActionEnd,0.7,0,0,true,Void,Tap a rival: on its next turn it has no aim line and its shot is capped at 70% power.',
 'star_magnet,STAR MAGNET,2,Utility,ActingZoogi,LeadActionEnd,0,0,0,true,Neutral,Cast on yourself: until your next turn every star orb on the court drifts toward you.',
]
for r in rows:
    if r.split(',')[0] + ',' not in s: s += '\n' + r
open(cp, 'w', encoding='utf-8').write(s + '\n')
print('p272 applied')
