# b149b (Evan 21:03 "the aerial assist we added can only apply within the bounds of the arena circle; anything flying outside is
# knockout, but there needs to be a cap on the distance they travel before being considered out - not to be confused with
# slamming into a crowd, stand, building"):
#  - SKY DIVE cam: starts only over the court, and lets go the moment the Zoogi leaves the arena (the KO camera takes over)
#  - FLIGHT CAP: a Zoogi off the court that has travelled 6 m past its last spot on the court is OUT right away, even mid-air -
#    unless the stands are catching it (stands crash, unchanged) or a building / wall is right ahead of it (it slams into that first;
#    once it has bounced off and is still outside, the cap applies). -ZoogiFlightCap=600 (cm), -ZoogiNoFlightCap = off
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
AR = 'Private/ZoogiArenaReact.cpp'
ed1(AR, '''	bool StandsCheck(AZoogiGameMode* G, AZoogiBall* B, const FVector& L)
	{''', '''	bool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L)
	{	// b149b: off the court and 6 m past its last spot on it = out, unless a building / wall is right ahead (it hits that first)
		static const bool bOff = ReactFlag(TEXT("ZoogiNoFlightCap"));
		static float Cap = -1.f; if (Cap < 0.f) { Cap = 600.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiFlightCap="), Cap); }
		if (bOff || !G || !B || !G->Arena || !B->Sphere) return false;
		static TMap<TWeakObjectPtr<AZoogiBall>, FVector> GLastOn;
		const FVector2D P(L.X, L.Y);
		if (!G->Arena->IsOffCourt(P) && L.Z > ZoogiConst::FallOutZ) { GLastOn.Add(B, L); return false; }
		const FVector* On = GLastOn.Find(B);
		if (!On) return false;
		const float Gone = FVector::Dist2D(L, *On);
		if (Gone < Cap) return false;
		const FVector V = B->GetBallVelocity(); const FVector V2(V.X, V.Y, 0.f);
		if (V2.Size() > 150.f)
		{
			FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiFlightCap), false);
			for (AZoogiBall* Zb : G->Balls) if (Zb) Q.AddIgnoredActor(Zb);
			for (AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
			FCollisionObjectQueryParams Ob; Ob.AddObjectTypesToQuery(ECC_WorldStatic); Ob.AddObjectTypesToQuery(ECC_WorldDynamic);
			FHitResult H;
			const float Ahead = FMath::Clamp(V2.Size() * 0.4f, 250.f, 900.f);
			if (G->GetWorld()->SweepSingleByObjectType(H, L, L + V2.GetSafeNormal() * Ahead, FQuat::Identity, Ob, FCollisionShape::MakeSphere(ZoogiConst::ZoogiRadius * 0.8f), Q) && !H.bStartPenetrating)
				return false; // about to slam into a building / wall / crowd: let it hit
		}
		UE_LOG(LogZoogi, Log, TEXT("FLIGHT CAP: P%d is %.0f cm past the court (cap %.0f, z %.0f) - out"), B->PlayerIndex + 1, Gone, Cap, L.Z);
		GLastOn.Remove(B);
		return true;
	}

	bool StandsCheck(AZoogiGameMode* G, AZoogiBall* B, const FVector& L)
	{''')
ed1('Public/ZoogiArenaReact.h', '''namespace ZoogiArenaReact
{''', '''namespace ZoogiArenaReact
{
	/** b149b: off the court and past the flight cap (6 m from its last spot on the court) = out, unless it is about to hit a building / wall. */
	ZOOGIROLL_API bool FlightCapOut(AZoogiGameMode* G, AZoogiBall* B, const FVector& L);''')
GM = 'Private/ZoogiGameMode.cpp'
ed1(GM, '''		if (!bZeroG && ZoogiEdgeHang::Track(this, B, L)) continue; // b58 (Evan): dropped off the edge -> its Aurex hangs on the lip (tap to pull up)''', '''		if (!bZeroG && ZoogiEdgeHang::Track(this, B, L)) continue; // b58 (Evan): dropped off the edge -> its Aurex hangs on the lip (tap to pull up)
		const bool bCapOut = !bZeroG && ZoogiArenaReact::FlightCapOut(this, B, L); // b149b (Evan): flew too far outside the arena = out, mid-air too''')
ed1(GM, '''		if (bZeroG || AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300''', '''		if (bZeroG || bCapOut || AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300''')
SD = 'Private/ZoogiSkyDiveCam.cpp'
ed1(SD, '''			if (Over > TriggerH() && !ZoogiPhysics::IsGrounded(B->Sphere, ZoogiConst::ZoogiRadius)) Start(G, B);''', '''			if (Over > TriggerH() && !ZoogiPhysics::IsGrounded(B->Sphere, ZoogiConst::ZoogiRadius) && G->Arena && !G->Arena->IsOffCourt(FVector2D(B->BallLocation().X, B->BallLocation().Y))) Start(G, B); // b149b: only over the arena''')
ed1(SD, '''		if (B->bOut) { Stop(W, TEXT("out")); return; }''', '''		if (B->bOut) { Stop(W, TEXT("out")); return; }
		if (G->Arena && G->Arena->IsOffCourt(FVector2D(B->BallLocation().X, B->BallLocation().Y))) { Stop(W, TEXT("left the arena")); return; } // b149b (Evan): the aerial cam only inside the arena''')
ed1(SD, '#include "ZoogiArcCam.h"', '#include "ZoogiArcCam.h"\n#include "ZoogiArena.h"')
print('b149b ok')
