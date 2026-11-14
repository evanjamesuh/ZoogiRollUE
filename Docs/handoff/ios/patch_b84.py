# b84 (Evan 11:03): the yellow electricity does not keep going - one flash when Bolt Balls is FIRED, one flash each time a bolt is shot;
# the purple arcs stay. -ZoogiBoltSparks = the b82 constant yellow sparks
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('''		static const bool bNoSparks = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBoltSparks")); // b82 (Evan): the yellow electric flashes stay on top of the purple arcs
		if (!bNoSparks && GStemT >= 0.3f)''',
  '''		static const bool bSparks = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoltSparks")); // b84 (Evan): no constant yellow sparks - flashes only on FIRE and on each shot
		if (bSparks && GStemT >= 0.3f)'''),
 ('''				ArmBolts(Gm, Pi, 3);
				ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 1.f, 1.3f);''',
  '''				ArmBolts(Gm, Pi, 3);
				ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 1.f, 1.3f);
				for (AZoogiBall* Cb : Gm->Balls) if (Cb && Cb->PlayerIndex == Pi)
				{	// b84: one yellow electric flash as it is fired (round the Zoogi), then only the purple arcs
					AZoogiFX::Stylized(Gm->GetWorld(), TEXT("ElectricShock"), Cb->BallLocation(), 1.1f);
					for (int32 k = 0; k < 3; ++k) { const float A = FMath::DegreesToRadians(k * 120.f); AZoogiFX::Stylized(Gm->GetWorld(), TEXT("ElectricShock"), Cb->BallLocation() + FVector(FMath::Cos(A) * 95.f, FMath::Sin(A) * 95.f, -ZoogiConst::ZoogiRadius + 6.f), 0.55f); }
				}'''),
 ('''		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Bs.P, 0.6f);''',
  '''		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Bs.P, 0.6f);
		AZoogiFX::Stylized(W, TEXT("ElectricShock"), From, 0.9f); // b84: the yellow flash on the Zoogi as each bolt goes'''),
])
print('b84 ok')
