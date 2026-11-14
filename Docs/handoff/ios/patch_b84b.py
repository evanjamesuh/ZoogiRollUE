# b84b (Evan 11:15: "guard after the turn isn't visible - 2x larger, like the size of the ice walls"): dome radius x2, brighter, ice-wall burst as it forms
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('	constexpr float GuardHP = 6.f; // 3 perfect (1 each) + 2 critical (1.5 each)\n',
  '	constexpr float GuardHP = 6.f; // 3 perfect (1 each) + 2 critical (1.5 each)\n	static float GuardR() { return 2.f * (ZoogiConst::ZoogiRadius + 36.f); } // b84: twice the b83 dome, about an ice wall tall\n'),
 ('		const FVector C = B->BallLocation();\n		const float Rd = ZoogiConst::ZoogiRadius + 36.f;\n', '		const FVector C = B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f);\n		const float Rd = GuardR();\n'),
 ('			FVector2D At(0.f, 0.f); float Ang = FMath::FRandRange(0.f, UE_TWO_PI), Left = FMath::FRandRange(40.f, 85.f), Th = 3.2f;',
  '			FVector2D At(0.f, 0.f); float Ang = FMath::FRandRange(0.f, UE_TWO_PI), Left = FMath::FRandRange(80.f, 170.f), Th = 5.f;'),
 ('				const float St = FMath::Min(Left, FMath::FRandRange(8.f, 16.f));', '				const float St = FMath::Min(Left, FMath::FRandRange(14.f, 28.f));'),
 ('					M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.45f, 0.75f, 1.f), 0.35f));\n					A->SetActorScale3D(FVector(2.f * (ZoogiConst::ZoogiRadius + 36.f) / 100.f));',
  '''					M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.45f, 0.75f, 1.f), 0.9f));
					A->SetActorScale3D(FVector(2.f * GuardR() / 100.f));
					// b84: the base sits on the floor (a dome, not a floating ball) and an ice-wall burst raises it
					A->SetActorLocation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f));
					AZoogiFabFX::Play(G->GetWorld(), TEXT("/Game/StylizedToonVFX3/Effects/NS_Ice_wall.NS_Ice_wall"), B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), 0.6f, 1.5f);'''),
 ('				if (AActor* A = D.A.Get()) A->SetActorLocation(B->BallLocation());', '				if (AActor* A = D.A.Get()) A->SetActorLocation(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius * 0.6f));'),
 ('				AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor(0.55f, 0.85f, 1.f), 220.f, 0.5f, false, 12.f);\n				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 0.7f);',
  '				AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor(0.55f, 0.85f, 1.f), 2.f * GuardR(), 0.6f, false, 12.f);\n				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 0.7f);'),
])
print('b84b ok')
