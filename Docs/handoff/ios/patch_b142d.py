# b142d (Evan 11:23 "I want the boss to always, when my turn comes around, be in front of me ... see his head looking down ...
# looking over the edge, fingers on the edge of the arena, looking at me"): the Cosmos boss is now a giant posed Aurex (puppet, Peek
# pose) hanging on the far rim of the court in front of the camera - both hands on the edge (fingers drumming), chest over the lip,
# head turned to look at the camera. Every human turn it ducks under the rim and comes up again in front of the new camera (and
# slides along the rim while you turn the aim). KO = slams the rim, combo = leans in, final round = rears up roaring. The boss cam
# shows its face from the court. -ZoogiBossTitan = the b136 170 m titan far out.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiAurexPuppet.h'
ed1(H, '''enum class EZoogiPuppetPose : uint8 { Stand, Guard, Hang, Climb, Drop, Cheer, Backstroke, Splat };''', '''enum class EZoogiPuppetPose : uint8 { Stand, Guard, Hang, Climb, Drop, Cheer, Backstroke, Splat, Peek }; // b142 Peek: the Cosmos boss over the rim''')
ed1(H, '''	/** Pops out (scale down + flash when bFx) then destroys itself; the ball's visuals come back. */''', '''	/** b142: a puppet with no Zoogi behind it (the Cosmos boss) - mesh as given, the caller sets the materials. */
	static AZoogiAurexPuppet* SpawnBoss(UWorld* W, USkeletalMesh* SM, const FVector& Feet, float Yaw, float Height);
	/** Pops out (scale down + flash when bFx) then destroys itself; the ball's visuals come back. */''')
ed1(H, '''	FLinearColor Tint = FLinearColor::White;
	TWeakObjectPtr<AZoogiBall> Ball;''', '''	FLinearColor Tint = FLinearColor::White;
	bool bLookAt = false;          // b142: the head turns to LookAt (world), up to 60 deg
	FVector LookAt = FVector::ZeroVector;
	float Lean = 0.f;              // b142 Peek: extra lean over the edge
	TWeakObjectPtr<AZoogiBall> Ball;''')
ed1(H, '''	FVector HandMidCS = FVector::ZeroVector;''', '''	FVector HandMidCS = FVector::ZeroVector;
	FQuat BindHeadCS = FQuat::Identity, HeadTurn = FQuat::Identity; // b142 look-at''')
C = 'Private/ZoogiAurexPuppet.cpp'
ed1(C, '''	bReady = true;
	UE_LOG(LogZoogi, Log, TEXT("PUPPET %s ready''', '''	if (Idx[Head] != INDEX_NONE) BindHeadCS = CS[Idx[Head]].GetRotation(); // b142 look-at
	bReady = true;
	UE_LOG(LogZoogi, Log, TEXT("PUPPET %s ready''')
ed1(C, '''void AZoogiAurexPuppet::Dismiss(bool bFx)''', '''AZoogiAurexPuppet* AZoogiAurexPuppet::SpawnBoss(UWorld* W, USkeletalMesh* SM, const FVector& Feet, float Yaw, float InHeight)
{
	if (!W || !SM) return nullptr;
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiAurexPuppet* P = W->SpawnActor<AZoogiAurexPuppet>(AZoogiAurexPuppet::StaticClass(), FTransform(FRotator(0.f, Yaw, 0.f), Feet), SP);
	if (!P) return nullptr;
	P->Pose = EZoogiPuppetPose::Peek; P->Height = InHeight; P->FaceDir = FRotator(0.f, Yaw, 0.f).Vector(); P->PopIn = 0.f;
	P->Tint = FLinearColor(0.6f, 0.25f, 1.f);
	if (!P->Setup(SM)) { P->Destroy(); return nullptr; }
	P->Body->SetCastShadow(false);
	P->Body->SetBoundsScale(3.f);
	P->Glow->SetVisibility(false);
	P->Solve(0.f, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("PUPPET boss %s out (%.0f cm)"), *SM->GetName(), InHeight);
	return P;
}

void AZoogiAurexPuppet::Dismiss(bool bFx)''')
ed1(C, '''	default: // Stand
		for (int32 s = 0; s < 2; ++s)
		{
			Set(s ? UArmR : UArmL, -U + S(s) * 0.3f + F * 0.05f);''', '''	case EZoogiPuppetPose::Peek: // b142: the Cosmos boss - hands on the rim in front of it, chest over the lip, fingers drumming
	{
		const float Sl = FMath::Clamp(Pull, 0.f, 1.f); // a slam: the arms rise
		for (int32 s = 0; s < 2; ++s)
		{
			const float Tap = FMath::Max(0.f, FMath::Sin(T * 3.1f + s * 2.4f)) * 0.08f;
			Set(s ? UArmR : UArmL, F * 0.8f + U * (0.22f + 0.8f * Sl) + S(s) * 0.62f);
			Set(s ? LArmR : LArmL, F * 0.55f - U * (0.85f - Tap - 1.2f * Sl) - S(s) * 0.12f);
		}
		Legs(0.06f, 1.3f, 0.2f, 0.16f);
		Set(Spine3, U + F * (0.45f + Lean));
		Set(Neck, U + F * (0.3f + Lean * 0.5f));
		Rate = 5.f;
		break;
	}
	default: // Stand
		for (int32 s = 0; s < 2; ++s)
		{
			Set(s ? UArmR : UArmL, -U + S(s) * 0.3f + F * 0.05f);''')
ed1(C, '''	for (int32 i : Mod) Body->SetBoneTransformByName(Names[i], CS[i], EBoneSpaces::ComponentSpace);
	HandMidCS = (CS[Idx[HandL]].GetLocation() + CS[Idx[HandR]].GetLocation()) * 0.5f;''', '''	for (int32 i : Mod) Body->SetBoneTransformByName(Names[i], CS[i], EBoneSpaces::ComponentSpace);
	HandMidCS = (CS[Idx[HandL]].GetLocation() + CS[Idx[HandR]].GetLocation()) * 0.5f;
	if (bLookAt && Idx[Head] != INDEX_NONE)
	{	// b142: the head turns to look at LookAt (the camera / the shot), at most 60 deg off the body, smoothed
		const int32 Hb = Idx[Head];
		const FVector LookCS = Body->GetComponentTransform().InverseTransformPosition(LookAt);
		const FVector Want = (LookCS - CS[Hb].GetLocation()).GetSafeNormal();
		const FVector Cur = (CS[Hb].GetRotation() * BindHeadCS.Inverse()).RotateVector(F).GetSafeNormal();
		if (!Want.IsNearlyZero() && !Cur.IsNearlyZero())
		{
			FQuat Turn = FQuat::FindBetweenNormals(Cur, Want);
			FVector Ax; float Ang; Turn.ToAxisAndAngle(Ax, Ang);
			Turn = FQuat(Ax, FMath::Min(Ang, FMath::DegreesToRadians(60.f)));
			HeadTurn = FQuat::Slerp(HeadTurn, Turn, FMath::Clamp(Dt * 4.f, 0.f, 1.f)).GetNormalized();
			FTransform Hx = CS[Hb]; Hx.SetRotation((HeadTurn * CS[Hb].GetRotation()).GetNormalized());
			Body->SetBoneTransformByName(Names[Hb], Hx, EBoneSpaces::ComponentSpace);
		}
	}''')
print('b142d puppet ok')
