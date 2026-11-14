# b142q: -ZoogiStandsTest puts P1 on Frozen's Ring 2 in front of the nearest grandstand before flinging it (from mid-court the
# Ring 1 wall stopped it).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
A = 'Private/ZoogiArenaReact.cpp'
ed1(A, '''			for (const FBox& Bx : GReactStandBoxes) { const float D = FVector::Dist2D(Bx.GetCenter(), L); if (D < BestD) { BestD = D; Tg = Bx.GetCenter(); } }
			if (BestD < 1e8f)
			{
				const FVector Dir = FVector(Tg.X - L.X, Tg.Y - L.Y, 0.f).GetSafeNormal();
				B->Sphere->SetPhysicsLinearVelocity(Dir * 2600.f + FVector(0.f, 0.f, 380.f));''', '''			for (const FBox& Bx : GReactStandBoxes) { const float D = FVector::Dist2D(Bx.GetCenter(), L); if (D < BestD) { BestD = D; Tg = Bx.GetCenter(); } }
			if (BestD < 1e8f)
			{
				const FVector Ctr = G->Arena->GetActorLocation();
				const FVector Dir = FVector(Tg.X - Ctr.X, Tg.Y - Ctr.Y, 0.f).GetSafeNormal();
				B->Sphere->SetWorldLocation(Ctr + Dir * 2180.f + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 20.f), false, nullptr, ETeleportType::TeleportPhysics);
				B->Sphere->SetPhysicsLinearVelocity(Dir * 2200.f + FVector(0.f, 0.f, 320.f));''')
print('b142q ok')
