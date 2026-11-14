import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n): EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            if b.count(o2) == 1: ob, nb = o2, n2
            else:
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b171a ok', len(EDITS))
# b171a (Evan Nov 7 08:41 "Pick a arena and draw a circle line to represent the distance from the centre, trigger 3m instead of 6,
#  slightly tilt camera ... show me how that looks with examples"): test switches only, the live game is unchanged -
#  -ZoogiKOLine (white dashed line = the court edge, red line = where a flying Zoogi is out), -ZoogiFallOut=cm (out when that far
#  below the floor off the court, not Ancient), -ZoogiKOShow (the shooter's Zoogi is pushed off the ring on its 1st roll; frames
#  every 0.15 s), roll camera pitch may now go down to 55 (-ZoogiRollPitch=).
AH = 'Public/ZoogiArenaReact.h'; AR = 'Private/ZoogiArenaReact.cpp'; GM = 'Private/ZoogiGameMode.cpp'; PC = 'Private/ZoogiPlayerController.cpp'
ed1(AH, '\tZOOGIROLL_API bool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);\n',
        '\tZOOGIROLL_API bool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);\n\t/** b171 test switches: -ZoogiKOLine circles, -ZoogiKOShow forced knockout + frames, -ZoogiFallOut=cm */\n\tZOOGIROLL_API void KOShowTick(AZoogiGameMode* G);\n\tZOOGIROLL_API bool FallOut(AZoogiGameMode* G, const FVector& L);\n')
ed1(AR, '\tbool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L)\n', r'''	bool FallOut(AZoogiGameMode* G, const FVector& L)
	{
		static float Fo = -2.f; if (Fo < -1.f) { Fo = 0.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiFallOut="), Fo); }
		if (Fo <= 0.f || !G || !G->Arena || G->Arena->GetTheme() == EZoogiArena::Ancient) return false;
		return L.Z < -Fo && G->Arena->IsOffCourt(FVector2D(L.X, L.Y));
	}

	void KOShowTick(AZoogiGameMode* G)
	{
		if (!G || !G->Arena) return;
		static const bool bLine = ReactFlag(TEXT("ZoogiKOLine")), bShow = ReactFlag(TEXT("ZoogiKOShow"));
		static TWeakObjectPtr<AZoogiArena> GLined;
		if (bLine && GLined.Get() != G->Arena)
		{
			GLined = G->Arena;
			float Cap = 600.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiFlightCap="), Cap);
			const float Edge = G->Arena->DistanceToEdge(FVector2D::ZeroVector, FVector2D(1.f, 0.f));
			auto Ring = [&](float R, const FLinearColor& Col, bool bDash, float Wd)
			{
				const int32 N = 160;
				for (int32 i = 0; i < N; ++i)
				{
					if (bDash && (i % 2)) continue;
					const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N, Am = (A0 + A1) * 0.5f;
					const float SegL = R * (A1 - A0) * (bDash ? 0.8f : 1.05f);
					UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(G->Arena);
					M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
					M->SetMaterial(0, ZoogiUtil::GlowMID(G->Arena, Col, 3.f));
					M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					M->SetCastShadow(false);
					M->RegisterComponent();
					M->SetWorldLocationAndRotation(FVector(FMath::Cos(Am) * R, FMath::Sin(Am) * R, 4.f), FRotator(0.f, FMath::RadiansToDegrees(Am) + 90.f, 0.f));
					M->SetWorldScale3D(FVector(SegL / 100.f, Wd / 100.f, 0.03f));
				}
			};
			Ring(Edge, FLinearColor::White, true, 22.f);
			Ring(Edge + Cap, FLinearColor(1.f, 0.1f, 0.1f), false, 30.f);
			UE_LOG(LogZoogi, Log, TEXT("KO LINE: court edge r%.0f (white), out line r%.0f (red, +%.0f cm)"), Edge, Edge + Cap, Cap);
		}
		if (!bShow) return;
		static int32 Stage = 0; static double T0 = 0.0; static int32 NShot = 0; static TWeakObjectPtr<AZoogiBall> GB;
		const double Now = G->GetWorld()->GetTimeSeconds();
		if (Stage == 0 && G->Phase == EZoogiPhase::Aiming && Now > 4.0) { Stage = 1; T0 = Now; }
		if (Stage == 1 && Now - T0 > 1.5) { G->TakeShot(TEXT("koshow_aim_view")); Stage = 2; }
		if (Stage == 2 && G->Phase == EZoogiPhase::Rolling)
		{
			AZoogiBall* B = G->Balls.IsValidIndex(G->ActivePlayer) ? G->Balls[G->ActivePlayer] : nullptr;
			if (B && B->Sphere && !B->bOut)
			{
				FVector2D D(B->BallLocation().X, B->BallLocation().Y); if (D.SizeSquared() < 1.f) D = FVector2D(1.f, 0.f); D.Normalize();
				const float Edge = G->Arena->DistanceToEdge(FVector2D::ZeroVector, D);
				const FVector P(D.X * (Edge - 260.f), D.Y * (Edge - 260.f), B->BallLocation().Z);
				B->Sphere->SetWorldLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
				B->Sphere->SetPhysicsLinearVelocity(FVector(D.X, D.Y, 0.f) * 1500.f + FVector(0.f, 0.f, 220.f));
				GB = B; Stage = 3; T0 = Now; NShot = 0;
				UE_LOG(LogZoogi, Log, TEXT("KO SHOW: P%d pushed off the ring toward (%.2f, %.2f), edge r%.0f"), B->PlayerIndex + 1, D.X, D.Y, Edge);
			}
		}
		if (Stage == 3)
		{
			if (Now - T0 >= 0.15 * NShot && NShot < 18) { G->TakeShot(FString::Printf(TEXT("koshow_%02d"), NShot)); ++NShot; }
			if (NShot >= 18) Stage = 4;
		}
	}

	bool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L)
''')
ed1(GM, '''	for (AZoogiBall* B : Balls)
	{
		if (!B || B->bOut || IsOnLaunchPad(B) || IsParked(B) || IsPadAirborne(B)) continue; // on pad / parked under pad / pad flight: not a ring-out''',
'''	ZoogiArenaReact::KOShowTick(this); // b171: -ZoogiKOLine / -ZoogiKOShow test switches
	for (AZoogiBall* B : Balls)
	{
		if (!B || B->bOut || IsOnLaunchPad(B) || IsParked(B) || IsPadAirborne(B)) continue; // on pad / parked under pad / pad flight: not a ring-out''')
ed1(GM, 'if (bZeroG || bCapOut || AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300',
        'if (bZeroG || bCapOut || AZoogiArena::IsRingOut(L) || ZoogiArenaReact::FallOut(this, L)) // Frozen Ring 2: also out past r2300; b171 -ZoogiFallOut=')
ed1(PC, 'WantPitch = FMath::Clamp(FMath::Max(WantPitch, RollPitch), 65.f, 88.f);',
        'WantPitch = FMath::Clamp(RollPitch < 65.f ? RollPitch : FMath::Max(WantPitch, RollPitch), 55.f, 88.f); // b171: -ZoogiRollPitch below 65 = a fixed lower tilt')
run()
