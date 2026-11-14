# Nov 11 (Claude): B247 - Evan 10:09 "Think about how we automate taking cover" + 10:05 "Firing happens automatically while aiming".
# Auto cover on the Voyager touch pad: let go of the move stick next to cover (Voyager's BP_CoverPlane) -> you snap into cover
# (IA_JumpAndCover injected once). In cover (IMC_Cover active): AIM = IA_AimCover (pop out, release = back in), pull the stick back = IA_LeaveCover.
# A COVER hint shows over the stick when cover is in reach. -ZoogiNoAutoCover = off.
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(rel, pairs, incs=()):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    for inc in incs:
        if '#include "%s"' % inc not in t:
            k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b247\n' % inc + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
patch('Private/ZoogiVoyagerLevels.cpp', [
('		float StickR = 60.f;\n', '''		float StickR = 60.f;
		bool bInCover = false, bCoverNear = false, bWasStick = false; double ReleasedAt = 0.0, LastCover = 0.0, BackSince = 0.0, NextNear = 0.0; // b247 auto cover
		TArray<TWeakObjectPtr<AActor>> Covers; TWeakObjectPtr<UWorld> CoverWorld;
'''),
('		if (!bLive) Text(TEXT("MOVE"), O + FVector2D(0.f, Rr * 1.6f), S * 0.03f, FLinearColor(1.f, 1.f, 1.f, 0.7f), Layer + 3);',
'''		if (!bLive) Text(TEXT("MOVE"), O + FVector2D(0.f, Rr * 1.6f), S * 0.03f, FLinearColor(1.f, 1.f, 1.f, 0.7f), Layer + 3);
		if (G.bInCover) Text(TEXT("IN COVER - pull back to leave"), O - FVector2D(0.f, Rr * 1.75f), S * 0.03f, FLinearColor(0.35f, 1.f, 0.45f, 0.95f), Layer + 3);
		else if (G.bCoverNear) Text(TEXT("COVER - let go to duck"), O - FVector2D(0.f, Rr * 1.75f), S * 0.03f, FLinearColor(1.f, 0.85f, 0.2f, 0.95f), Layer + 3);'''),
('''	for (int32 b = 0; b < B_Num; ++b)
		if (b == B_Sprint ? G.bSprintOn : G.Held[b]) if (UInputAction* Ia = A(Act[b], false)) Ei->InjectInputForAction(Ia, FInputActionValue(true), NoMods, NoTrig);''',
'''	// b247: in cover = Voyager's IMC_Cover is on
	static TWeakObjectPtr<UInputMappingContext> ImcCover;
	if (!ImcCover.IsValid()) ImcCover = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Voyager/Inputs/IMC_Cover.IMC_Cover"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	{ int32 Pri = 0; G.bInCover = ImcCover.IsValid() && Ei->HasMappingContext(ImcCover.Get(), Pri); }
	for (int32 b = 0; b < B_Num; ++b)
		if (b == B_Sprint ? G.bSprintOn : G.Held[b]) if (UInputAction* Ia = A(b == B_Aim && G.bInCover ? TEXT("IA_AimCover") : Act[b], false)) Ei->InjectInputForAction(Ia, FInputActionValue(true), NoMods, NoTrig);
	// b247: auto cover
	static const bool bNoAuto = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAutoCover"));
	const double Now = FPlatformTime::Seconds();
	APawn* Me = W->GetFirstPlayerController() ? W->GetFirstPlayerController()->GetPawn() : nullptr;
	if (!bNoAuto && Me)
	{
		if (G.CoverWorld.Get() != W)
		{
			G.CoverWorld = W; G.Covers.Reset();
			for (TActorIterator<AActor> It(W); It; ++It) if (It->GetClass()->GetName().Contains(TEXT("CoverPlane"))) G.Covers.Add(*It);
			UE_LOG(LogZoogi, Log, TEXT("VOYPAD auto cover: %d cover spots on this map"), G.Covers.Num());
		}
		if (Now >= G.NextNear)
		{
			G.NextNear = Now + 0.2; G.bCoverNear = false;
			const FVector P = Me->GetActorLocation();
			for (const TWeakObjectPtr<AActor>& Cv : G.Covers)
				if (AActor* C = Cv.Get()) { const FVector D = C->GetActorLocation() - P; if (FVector2D(D.X, D.Y).Size() < 260.f && FMath::Abs(D.Z) < 220.f) { G.bCoverNear = true; break; } }
		}
		const bool bStick = G.MovePtr != INDEX_NONE;
		if (!G.bInCover)
		{
			if (bStick) { G.bWasStick = true; G.ReleasedAt = Now; }
			else if (G.bWasStick && Now - G.ReleasedAt > 0.15)
			{
				G.bWasStick = false;
				if (G.bCoverNear && Now - G.LastCover > 1.5)
					if (UInputAction* Ia = A(TEXT("IA_JumpAndCover"), false)) { Ei->InjectInputForAction(Ia, FInputActionValue(true), NoMods, NoTrig); G.LastCover = Now; UE_LOG(LogZoogi, Log, TEXT("VOYPAD auto cover: snap in")); }
			}
			G.BackSince = 0.0;
		}
		else
		{
			G.bWasStick = false;
			const double Dy = bStick ? (G.MoveNow.Y - G.MoveOrigin.Y) / FMath::Max(1.f, G.StickR) : 0.0;
			if (Dy > 0.7) { if (G.BackSince <= 0.0) G.BackSince = Now; } else G.BackSince = 0.0;
			if (G.BackSince > 0.0 && Now - G.BackSince > 0.25 && Now - G.LastCover > 0.8)
				if (UInputAction* Ia = A(TEXT("IA_LeaveCover"), false)) { Ei->InjectInputForAction(Ia, FInputActionValue(true), NoMods, NoTrig); G.LastCover = Now; G.BackSince = 0.0; UE_LOG(LogZoogi, Log, TEXT("VOYPAD auto cover: leave")); }
		}
	}'''),
], incs=('InputMappingContext.h', 'GameFramework/Pawn.h'))
patch('Private/ZoogiVoyager.cpp', [('				const bool bEm = Nm.Contains(TEXT("Emis")) || Nm.Contains(TEXT("Glow"));',
'				if (Nm.Contains(TEXT("Flash"))) continue; // b247: keep the Voyager hit flash\n				const bool bEm = Nm.Contains(TEXT("Emis")) || Nm.Contains(TEXT("Glow"));')])
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B246"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B247"), TEXT("AUTO COVER"), TEXT("Voyager touch controls take cover for you: let go of the move stick next to cover and your hero ducks in. In cover, hold AIM to pop out and shoot, let go to drop back, pull the stick back to leave. A COVER hint shows when cover is in reach.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
