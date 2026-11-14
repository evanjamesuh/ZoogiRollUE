# b89c (Evan 14:57: "not seeing the new cosmos thing come up on the field on mobile"): the marker's floor height came from the turn
# ball, which can be parked under the launch pad at a round start (marker buried); now a trace finds the court floor at the spot.
# The first marker also rises a few seconds into the match (round 1), then one more every round.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiCosmicMarkers.cpp', [
 ('#include "Misc/Parse.h"\n', '#include "Misc/Parse.h"\n#include "CollisionQueryParams.h"\n#include "Engine/EngineTypes.h"\n'),
 ('''	void OnMatchStart(AZoogiGameMode* G)
	{
		for (FMarker& M : Markers) if (AActor* A = M.A.Get()) A->Destroy();
		Markers.Reset();
	}''',
  '''	void OnMatchStart(AZoogiGameMode* G)
	{
		for (FMarker& M : Markers) if (AActor* A = M.A.Get()) A->Destroy();
		Markers.Reset();
		// b89c: the first marker rises a few seconds into round 1
		if (Off() || !G) return;
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		const double At = FPlatformTime::Seconds() + 4.0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, At](float) -> bool
		{
			AZoogiGameMode* Gm = WG.Get();
			if (!Gm) return false;
			if (FPlatformTime::Seconds() < At) return true;
			if (Gm->Arena && Gm->Arena->IsForming()) return true;
			if (Gm->RoundNumber <= 1 && Gm->Phase != EZoogiPhase::Menu) OnRoundStart(Gm);
			return false;
		}));
	}'''),
 ('''		const AZoogiBall* Ref = G->GetActiveBall();
		const float FloorZ = Ref ? Ref->BallLocation().Z - ZoogiConst::ZoogiRadius : Ar->GetActorLocation().Z;''',
  '''		// b89c: the court floor from Zoogis standing on it (not one parked under a pad); each spot is traced below
		float FloorZ = Ar->GetActorLocation().Z;
		{
			TArray<float> Zs;
			for (const AZoogiBall* B : G->Balls) if (B && !B->bOut && !B->bParked && !B->bOnLaunchPad) Zs.Add(B->BallLocation().Z - ZoogiConst::ZoogiRadius);
			if (Zs.Num()) { Zs.Sort(); FloorZ = Zs[Zs.Num() / 2]; }
		}
		FCollisionQueryParams Qp(TEXT("ZoogiCosmicMarker"), false);
		for (AZoogiBall* B : G->Balls) if (B) Qp.AddIgnoredActor(B);
		for (AZoogiOrb* O : G->Orbs) if (O) Qp.AddIgnoredActor(O);
		for (const FMarker& Mq : Markers) if (AActor* Am = Mq.A.Get()) Qp.AddIgnoredActor(Am);
		auto FloorAt = [&](const FVector2D& Xy)
		{
			FHitResult Hr;
			if (W->LineTraceSingleByChannel(Hr, FVector(Xy, FloorZ + 600.f), FVector(Xy, FloorZ - 600.f), ECC_Visibility, Qp)) return (float)Hr.ImpactPoint.Z;
			return FloorZ;
		};'''),
 ('''			if (bClear) { Spot = P; bFound = true; }''', '''			if (bClear) { Spot = P; Spot.Z = FloorAt(FVector2D(P)); bFound = true; }'''),
])
print('b89c ok')
