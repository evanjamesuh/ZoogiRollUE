# b96b (Evan 18:33: "pause the animation of guard so it doesn't keep restarting and looks closed"): the WOOD SHIELD plays its
# forming once and then holds, closed (Niagara paused at 0.9 s) until it drops - no more replay every 1.3 s.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('		struct FShield { TWeakObjectPtr<AZoogiBall> B; float SavedMass = 0.f; bool bHeavy = false; float HitAt = -1.f; int32 Turn = -1; float NextFx = 0.f; };',
  '		struct FShield { TWeakObjectPtr<AZoogiBall> B; float SavedMass = 0.f; bool bHeavy = false; float HitAt = -1.f; int32 Turn = -1; float NextFx = 0.f; TWeakObjectPtr<AZoogiFabFX> Fx; double CastAt = 0.0; bool bFrozen = false; };\n		static void DropShieldFx(FShield& Sh) { if (AZoogiFabFX* F = Sh.Fx.Get()) F->Destroy(); Sh.Fx.Reset(); }'),
 ('''			if (AZoogiFabFX* F = AZoogiFabFX::Play(W, FX(TEXT("NS_Wood_shield")), MeFloor, 0.3f, 1.6f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius))) F->StopAfter = 1.4f; // b71 (Grok): bigger, stays up longer
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Wood_shield")), Me, 1.f);
			Shields.RemoveAll([Caster](const FShield& X) { return X.B.Get() == Caster; });
			FShield Sh; Sh.B = Caster; Sh.SavedMass = Caster->Sphere ? Caster->Sphere->GetMass() : 0.f; Sh.Turn = G->TurnCounter; Sh.NextFx = Now(G) + 1.3f; Shields.Add(Sh);''',
  '''			AZoogiFabFX* WoodFx = AZoogiFabFX::Play(W, FX(TEXT("NS_Wood_shield")), MeFloor, 0.3f, WoodStays() ? 9999.f : 1.6f, Caster, FVector(0, 0, -ZoogiConst::ZoogiRadius));
			if (WoodFx && !WoodStays()) WoodFx->StopAfter = 1.4f; // b71 (Grok): bigger, stays up longer
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Wood_shield")), Me, 1.f);
			for (FShield& Old : Shields) if (Old.B.Get() == Caster) DropShieldFx(Old);
			Shields.RemoveAll([Caster](const FShield& X) { return X.B.Get() == Caster; });
			FShield Sh; Sh.B = Caster; Sh.SavedMass = Caster->Sphere ? Caster->Sphere->GetMass() : 0.f; Sh.Turn = G->TurnCounter; Sh.NextFx = Now(G) + 1.3f; Sh.Fx = WoodFx; Sh.CastAt = FPlatformTime::Seconds(); Shields.Add(Sh);'''),
 ('''			if (!B || B->bOut || !B->Sphere) { Shields.RemoveAt(i); continue; }
			AZoogiBall* Act = G->GetActiveBall();''', '''			if (!B || B->bOut || !B->Sphere) { DropShieldFx(Sh); Shields.RemoveAt(i); continue; }
			AZoogiBall* Act = G->GetActiveBall();'''),
 ('''				Sh.HitAt = T;
				if (AZoogiFabFX* F = AZoogiFabFX::Play(G->GetWorld(), FX(TEXT("NS_Wood_shield")), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius), 0.3f, 1.6f, B, FVector(0, 0, -ZoogiConst::ZoogiRadius))) F->StopAfter = 1.4f; // b71 (Grok)''',
  '''				Sh.HitAt = T;
				if (WoodStays()) AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor(0.55f, 0.85f, 0.35f), 180.f, 0.3f, false, 10.f); // b96b: a flash, the shield stays closed
				else if (AZoogiFabFX* F = AZoogiFabFX::Play(G->GetWorld(), FX(TEXT("NS_Wood_shield")), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius), 0.3f, 1.6f, B, FVector(0, 0, -ZoogiConst::ZoogiRadius))) F->StopAfter = 1.4f; // b71 (Grok)'''),
 ('''				if (T >= Sh.NextFx) { Sh.NextFx = T + 1.3f; if (AZoogiFabFX* F = AZoogiFabFX::Play(G->GetWorld(), FX(TEXT("NS_Wood_shield")), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius), 0.3f, 1.6f, B, FVector(0, 0, -ZoogiConst::ZoogiRadius))) F->StopAfter = 1.4f; }''',
  '''				// b96b (Evan): no replay - the forming plays once, then it is paused fully closed
				if (!Sh.bFrozen && FPlatformTime::Seconds() - Sh.CastAt > 0.9) { Sh.bFrozen = true; if (AZoogiFabFX* F = Sh.Fx.Get()) if (F->NC) F->NC->SetPaused(true); }'''),
 ('''				if (G->ActivePlayer == B->PlayerIndex && G->TurnCounter > Sh.Turn) { B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield on P%d drops (its turn)"), B->PlayerIndex + 1); Shields.RemoveAt(i); }''',
  '''				if (G->ActivePlayer == B->PlayerIndex && G->TurnCounter > Sh.Turn) { B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield on P%d drops (its turn)"), B->PlayerIndex + 1); DropShieldFx(Sh); Shields.RemoveAt(i); }'''),
])
print('b96b ok')
