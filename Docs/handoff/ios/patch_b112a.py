# b112a (Evan: "in skill lab add a radius circle on the floor and a half-circle dashed arc for height - the limit of the distance an
# orb or player will travel"): the lab measures every PLAY - the farthest an orb got from the rack and the highest it flew - and draws
# a solid floor ring at that reach + a dashed half-circle arc standing up to that height, with the numbers.
import os
L = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiSkillLab.cpp')
def ed1(p, o, n):
    b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:70]); open(p, 'wb').write(b.replace(o, n))
ed1(L, '''		FVector GSpot = FVector::ZeroVector, GCasterAt = FVector::ZeroVector; float GBaseYaw = 0.f;''',
'''		FVector GSpot = FVector::ZeroVector, GCasterAt = FVector::ZeroVector; float GBaseYaw = 0.f;
		// b112: the reach of the last PLAY (farthest orb from the rack centre, highest above its start), measured live
		float GReach = 0.f, GHeight = 0.f, GFloorZ = 0.f; bool GMeasuring = false; TMap<TWeakObjectPtr<AZoogiOrb>, float> GStartZ;''')
# reset the measurement on every PLAY (after the rack is placed)
ed1(L, '''			else ZoogiSkills::LabCast(G, Me, Id);
			GCastAt = FPlatformTime::Seconds();''', '''			else ZoogiSkills::LabCast(G, Me, Id);
			GCastAt = FPlatformTime::Seconds();
			GReach = 0.f; GHeight = 0.f; GMeasuring = true; GStartZ.Reset();
			float Zs = 0.f; int32 Nz = 0;
			for (AZoogiOrb* O : G->Orbs) if (O) { GStartZ.Add(O, O->GetActorLocation().Z); Zs += O->GetActorLocation().Z; ++Nz; }
			GFloorZ = Nz ? Zs / Nz - 30.f : GSpot.Z;''')
# measure every tick while open
ed1(L, '''		HideCaster(G);
		UpdateCam(G, RealDt);''', '''		HideCaster(G);
		UpdateCam(G, RealDt);
		if (GMeasuring)
		{
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O) continue;
				const FVector P = O->GetActorLocation();
				if (O->bOut || O->IsHidden()) continue; // knocked out of the arena: the ring stops at the arena's edge
				GReach = FMath::Max(GReach, FVector::Dist2D(P, GSpot));
				if (const float* Z0 = GStartZ.Find(O)) GHeight = FMath::Max(GHeight, P.Z - *Z0);
			}
			if (FPlatformTime::Seconds() - GCastAt > 8.0) GMeasuring = false; // settled: the marks stay until the next PLAY / RESET
		}''')
ed1(L, '''		case 21: ZoogiSkills::ClearField(G); ResetScene(G); GCastAt = 0.0;''', '''		case 21: ZoogiSkills::ClearField(G); ResetScene(G); GCastAt = 0.0; GReach = GHeight = 0.f; GMeasuring = false;''')
# draw: floor ring + dashed height arc + labels, before the panels
ed1(L, '''		// title plate (P5: black slant, gold lettering)''', '''		// b112: the reach marks (world-space, projected)
		if (PC && GReach > 40.f)
		{
			const FLinearColor RingCol(1.f, 0.82f, 0.25f, 0.95f), ArcCol(0.45f, 0.9f, 1.f, 0.95f);
			const FVector Ctr(GSpot.X, GSpot.Y, GFloorZ);
			auto Seg = [&](const FVector& A, const FVector& B, const FLinearColor& Col, float Th)
			{
				FVector2D Sa, Sb;
				if (PC->ProjectWorldLocationToScreen(A, Sa, true) && PC->ProjectWorldLocationToScreen(B, Sb, true)) { FCanvasLineItem Li(Sa, Sb); Li.LineThickness = Th; Li.SetColor(Col); C->DrawItem(Li); }
			};
			const int32 N = 72;
			for (int32 k = 0; k < N; ++k) // the floor ring: how far the farthest orb went
			{
				const float A0 = 2.f * PI * k / N, A1 = 2.f * PI * (k + 1) / N;
				Seg(Ctr + FVector(FMath::Cos(A0), FMath::Sin(A0), 0.f) * GReach, Ctr + FVector(FMath::Cos(A1), FMath::Sin(A1), 0.f) * GReach, RingCol, 4.f * U);
			}
			// the height arc: a dashed half circle standing on the ring, side-on to the camera, up to the highest an orb flew
			FVector Side = FVector::RightVector;
			if (ACameraActor* Cm = GCamActor.Get()) { Side = FVector::CrossProduct(FVector::UpVector, (Ctr - Cm->GetActorLocation()).GetSafeNormal2D()); if (!Side.Normalize()) Side = FVector::RightVector; }
			const float Hh = FMath::Max(GHeight, 20.f);
			const int32 Na = 32;
			for (int32 k = 0; k < Na; k += 2)
			{
				const float A0 = PI * k / Na, A1 = PI * (k + 1) / Na;
				Seg(Ctr + Side * FMath::Cos(A0) * GReach + FVector(0, 0, FMath::Sin(A0) * Hh), Ctr + Side * FMath::Cos(A1) * GReach + FVector(0, 0, FMath::Sin(A1) * Hh), ArcCol, 3.f * U);
			}
			Seg(Ctr, Ctr + FVector(0, 0, Hh), FLinearColor(0.45f, 0.9f, 1.f, 0.5f), 2.f * U); // the height pole
			FVector2D Ls, Hs;
			if (PC->ProjectWorldLocationToScreen(Ctr + Side * GReach, Ls, true)) ZoogiFunCanvasText(C, FString::Printf(TEXT("REACH %.1f m"), GReach / 100.f), Ls.X, Ls.Y + 8.f * U, 26.f * U, RingCol, 1, 1);
			if (PC->ProjectWorldLocationToScreen(Ctr + FVector(0, 0, Hh), Hs, true)) ZoogiFunCanvasText(C, FString::Printf(TEXT("HEIGHT %.1f m"), GHeight / 100.f), Hs.X, Hs.Y - 34.f * U, 26.f * U, ArcCol, 1, 1);
		}
		// title plate (P5: black slant, gold lettering)''')
b = open(L, 'rb').read()
if b'#include "CanvasItem.h"' not in b:
    nl = b'\r\n' if b'\r\n' in b else b'\n'
    b = b.replace(b'#include "Engine/Canvas.h"' + nl, b'#include "Engine/Canvas.h"' + nl + b'#include "CanvasItem.h"' + nl, 1)
    open(L, 'wb').write(b)
print('b112a ok')
