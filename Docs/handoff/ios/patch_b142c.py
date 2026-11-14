# b142c (Evan 11:37 "Fix water cannon to make the blue ball that forms shoot in the same direction and impact measures up to the
# animation, currently the blue ball shoots perpendicular and after the blast"): the NS_Water_cannon system turns by a yaw offset
# (-ZoogiWaterCannonYaw=<deg>, default WcYawDefault) and the hit lands at WcHitT (-ZoogiWaterCannonHitT=<s>) - first a measuring
# pass: -ZoogiWcYawTest makes the Skill Lab test fire it at 0 / 90 / -90 / 180 from the TOP camera (frames wc_y<yaw>_<t>) and logs
# the system bounds (where its water ball is) every 0.1 s in the cast frame (along / side).
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''namespace ZoogiSkills
{
	namespace
	{''', '''float GZoogiWcYawTest = -999.f; // b142c: the Skill Lab measuring pass sets the water cannon yaw offset
namespace ZoogiSkills
{
	namespace
	{
		// b142c: the water cannon system's own facing / its water ball's arrival (measured in the lab: see patch_b142c)
		constexpr float WcYawDefault = 0.f, WcHitDefault = 0.35f;
		float WcYaw() { if (GZoogiWcYawTest > -900.f) return GZoogiWcYawTest; static float Y = WcYawDefault; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWaterCannonYaw="), Y); } return Y; }
		float WcHitT() { static float T = WcHitDefault; static bool b = false; if (!b) { b = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWaterCannonHitT="), T); } return T; }
		void WcProbe(UWorld* W, AZoogiFabFX* Fx, const FVector& From, const FVector& Dir)
		{	// b142c measuring: where the system's bounds are (along the cast / to its side) every 0.1 s
			if (!Fx || !W || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWcYawTest"))) return;
			TWeakObjectPtr<AZoogiFabFX> WF = Fx; const double T0 = FPlatformTime::Seconds(); const float Yw = WcYaw();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WF, T0, From, Dir, Yw](float) -> bool
			{
				AZoogiFabFX* F = WF.Get(); const double T = FPlatformTime::Seconds() - T0;
				if (!F || !F->NC || T > 1.7) return false;
				const FBoxSphereBounds Bd = F->NC->Bounds; const FVector D = Bd.Origin - From; const FVector Side(-Dir.Y, Dir.X, 0.f);
				UE_LOG(LogZoogi, Log, TEXT("WC PROBE yaw %+.0f t %.2f: bounds along %.0f side %.0f up %.0f r %.0f ext (%.0f,%.0f,%.0f)"), Yw, T, FVector::DotProduct(D, Dir), FVector::DotProduct(D, Side), D.Z, Bd.SphereRadius, Bd.BoxExtent.X, Bd.BoxExtent.Y, Bd.BoxExtent.Z);
				return true;
			}), 0.1f);
		}''')
ed1(S, '''				const FVector Tp = ActorPos(T); const FVector Dir = (Tp - Me).GetSafeNormal2D();
				AZoogiFabFX::Play(W, FX(TEXT("NS_Water_cannon")), Me + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(Id, 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation());
				AZoogiFabFX::Sound(W, SFX(TEXT("S_Water_cannon")), Me, 1.f);
				TWeakObjectPtr<AActor> WT = T;
				Later(G, 0.35f, [WG, WT, Dir, Pi]()''', '''				const FVector Tp = ActorPos(T); const FVector Dir = (Tp - Me).GetSafeNormal2D();
				AZoogiFabFX* WcFx = AZoogiFabFX::Play(W, FX(TEXT("NS_Water_cannon")), Me + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(Id, 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation() + FRotator(0.f, WcYaw(), 0.f)); // b142c: its water ball along the cast
				WcProbe(W, WcFx, Me + FVector(0, 0, ZoogiConst::ZoogiRadius), Dir);
				AZoogiFabFX::Sound(W, SFX(TEXT("S_Water_cannon")), Me, 1.f);
				TWeakObjectPtr<AActor> WT = T;
				UE_LOG(LogZoogi, Log, TEXT("SKILL water_cannon: yaw offset %.0f, target %.0f cm away, hit at %.2f s"), WcYaw(), FVector::Dist2D(Tp, Me), WcHitT());
				Later(G, WcHitT(), [WG, WT, Dir, Pi]()''')
ed1(S, '''					AZoogiFabFX::Play(Wq, FX(TEXT("NS_Water_cannon")), Mq + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_cannon")), 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation());
					AZoogiFabFX::Sound(Wq, SFX(TEXT("S_Water_cannon")), Mq, 0.8f);
					Later(Gm, 0.35f,''', '''					AZoogiFabFX::Play(Wq, FX(TEXT("NS_Water_cannon")), Mq + FVector(0, 0, ZoogiConst::ZoogiRadius), Sc(FName(TEXT("water_cannon")), 0.4f), 1.6f, nullptr, FVector::ZeroVector, Dir.Rotation() + FRotator(0.f, WcYaw(), 0.f)); // b142c
					AZoogiFabFX::Sound(Wq, SFX(TEXT("S_Water_cannon")), Mq, 0.8f);
					Later(Gm, WcHitT(),''')
L = 'Private/ZoogiSkillLab.cpp'
ed1(L, '''void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);''', '''void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col);
extern float GZoogiWcYawTest; // b142c (ZoogiSkills.cpp)''')
ed1(L, '''		if (TestOn() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)''', '''		static int32 WcStep = 0;
		if (TestOn() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiWcYawTest")) && WcStep < 4 && FPlatformTime::Seconds() >= GTestNext)
		{	// b142c measuring pass: the water cannon at yaw offsets 0 / 90 / -90 / 180 from the TOP camera
			const float Yaws[4] = { 0.f, 90.f, -90.f, 180.f };
			GZoogiWcYawTest = Yaws[WcStep];
			GCam = 1; ApplyPreset();
			for (int32 s = 0; s < NumSkills; ++s) if (FCString::Strcmp(Skills[s], TEXT("water_cannon")) == 0) GSel = s;
			CastNow(G);
			for (int32 k = 0; k < 5; ++k) G->TakeShotDelayed(FString::Printf(TEXT("wc_y%+.0f_%d"), Yaws[WcStep], k), 0.25f + 0.25f * k);
			++WcStep; GTestNext = FPlatformTime::Seconds() + 3.5;
			if (WcStep == 4) { GCam = 0; }
			return;
		}
		if (WcStep == 4) { WcStep = 5; GZoogiWcYawTest = -999.f; ApplyPreset(); }
		if (TestOn() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)''')
print('b142c ok')
