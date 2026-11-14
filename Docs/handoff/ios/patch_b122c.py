# b122c (Evan 21:16: "for skills that require target selection, a line drawn or area selected - that still needs to happen"):
#  a COMBO cast whose skill aims (tap targets / swipe a line / drag an area / place spots) opens the normal skill pick from where the Zoogi
#  is at that moment (the world waits, as for any pick); FIRE casts it (SP was paid when armed - nothing more is spent), play resumes.
#  CANCEL (or the 30 s timeout) puts the cast back at the front of the queue. Self-cast skills still fire at once.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
SH = 'Public/ZoogiSkills.h'; S = 'Private/ZoogiSkills.cpp'
ed1(SH, '''	ZOOGIROLL_API bool ComboGo(AZoogiGameMode* G);''', '''	ZOOGIROLL_API bool ComboGo(AZoogiGameMode* G);
	/** b122c: a COMBO cast's pick was cancelled - the skill goes back to the front of the queue */
	ZOOGIROLL_API void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id);''')
ed1(S, '''			bool bCpu = false; double CpuFireAt = 0.0;''', '''			bool bCpu = false; double CpuFireAt = 0.0;
			bool bComboCast = false; // b122c: a COMBO cast's pick - FIRE casts it (already paid), CANCEL puts it back''')
ed1(S, '''	void Cancel(const TCHAR* Why) { if (S.bOn) UE_LOG(LogZoogi, Log, TEXT("SKILL pick %s cancelled (%s)"), *S.Id.ToString(), Why); Reset(); }''',
'''	void Cancel(const TCHAR* Why)
	{
		if (S.bOn) UE_LOG(LogZoogi, Log, TEXT("SKILL pick %s cancelled (%s)"), *S.Id.ToString(), Why);
		if (S.bOn && !S.bConfirmed && S.bComboCast) if (AZoogiBall* Cb = S.Caster.Get()) if (AZoogiGameMode* Gc = Cb->GetWorld()->GetAuthGameMode<AZoogiGameMode>()) ComboPutBack(Gc, Cb->PlayerIndex, S.Id); // b122c
		Reset();
	}''')
ed1(S, '''		if (bPress && S.CancelBtn.bIsValid && S.CancelBtn.IsInside(Ptr)) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f); Cancel(TEXT("CANCEL")); ZoogiP5Cmd::BackToSkills(); return; }''',
'''		if (bPress && S.CancelBtn.bIsValid && S.CancelBtn.IsInside(Ptr)) { const bool bWasCombo = S.bComboCast; ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f); Cancel(TEXT("CANCEL")); if (!bWasCombo) ZoogiP5Cmd::BackToSkills(); return; }
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bComboCast)
		{	// b122c: a COMBO cast - already paid when armed, cast it now and play resumes
			if (!CanFire()) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			S.bConfirmed = true;
			if (AZoogiBall* Cb = S.Caster.Get()) { UE_LOG(LogZoogi, Log, TEXT("COMBO cast %s fired from its pick"), *S.Id.ToString()); Execute(G, Cb, S.Id); }
			return;
		}''')
ed1(S, '''		if (Id == FName(TEXT("precision"))) { if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(G)) { Wp->BeginPrecision(G, B, 3); bOk = true; } }
		else bOk = AutoCast(G, Id); // relative to where the Zoogi is now: nearest targets / spots round the ball''',
'''		const FDef* Dc = Find(Id);
		if (Id == FName(TEXT("precision"))) { if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(G)) { Wp->BeginPrecision(G, B, 3); bOk = true; } }
		else if (Dc && Dc->Aim != EAim::Self && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboAutoAim")))
		{	// b122c (Evan: "skills that need target selection, a line drawn or an area selected - that still needs to happen"):
			// the normal pick from where the Zoogi is now (the world waits); FIRE casts it, CANCEL puts it back in the queue
			Reset();
			S.Slot = -1; S.Id = Id; S.Aim = Dc->Aim; S.R = Dc->R; S.Count = Dc->Count; S.Caster = B; S.bComboCast = true;
			S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
			S.Range = FMath::Max(400.f, G->ArcRange());
			S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
			FVector AimAt = S.Origin + B->GetBallVelocity().GetSafeNormal2D() * S.Range * 0.5f;
			float BestD = 1e9f;
			for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < BestD) { BestD = Dd; AimAt = O->BallLocation(); } }
			S.Ctr = ClampRange(AimAt, 0.f); S.A = S.B = S.Ctr;
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.3f);
			bOk = true;
		}
		else bOk = AutoCast(G, Id); // self skills: at once, from where the Zoogi is now''')
ed1(S, '''	int32 ComboCount(const AZoogiGameMode* G, int32 Player) {''', '''	void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id)
	{
		if (!G || GComboG.Get() != G) return;
		GCombo.FindOrAdd(Player).Insert(Id, 0);
		if (int32* D = GComboDone.Find(Player)) *D = FMath::Max(0, *D - 1);
		if (!GComboTotal.Contains(Player)) { GComboTotal.Add(Player, GCombo[Player].Num()); GComboDone.Add(Player, 0); }
		UE_LOG(LogZoogi, Log, TEXT("COMBO P%d %s back in the queue (%d waiting)"), Player + 1, *Id.ToString(), GCombo[Player].Num());
	}
	int32 ComboCount(const AZoogiGameMode* G, int32 Player) {''')
print('b122c ok')
