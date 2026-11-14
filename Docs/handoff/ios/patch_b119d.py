# b119d (Evan 16:23: "For Frozen Ring increase the diameter of the inner arena to close the gap and remove the bridges and all wall
# blocks"). ZoogiFrozen::OpenRing() (default ON; -ZoogiFrozenClassicRing = the old map):
#  - the crevasse r1550-1750 is filled with a gentle snow slope (court z 0 -> Ring 2 z -30): one continuous field out to r2060
#  - no bridges (spans A / B), no Ring 1 ice / igloo walls, no rim-gap lips, no rim ice-block clusters, no Ring 2 curb / bank walls /
#    crevasse lip; the Frozen roam (bridge launches) is off by default with it
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
L = 'Public/ZoogiFrozenLayout.h'; R2 = 'Private/ZoogiFrozenRing2.cpp'; F = 'Private/ZoogiArenaFrozen.cpp'; RM = 'Private/ZoogiFrozenRoam.cpp'
ed1(L, '''	ZOOGIROLL_API bool Ring2On();''', '''	ZOOGIROLL_API bool Ring2On();
	/** b119 (Evan): the open ring - crevasse filled, no bridges, no wall blocks. -ZoogiFrozenClassicRing = off. */
	ZOOGIROLL_API bool OpenRing();''')
ed1(R2, '''namespace ZoogiRing2
{''', '''bool ZoogiFrozen::OpenRing()
{
	static const bool bOn = []
	{
		const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrozenClassicRing"));
		UE_LOG(LogZoogi, Log, TEXT("FROZEN open ring (gap filled, no bridges / wall blocks): %s"), b ? TEXT("ON (-ZoogiFrozenClassicRing reverts)") : TEXT("OFF"));
		return b;
	}();
	return bOn;
}

namespace ZoogiRing2
{''')
ed1(R2, '''	// ---- crevasse faces (visual): inner ice cliff r1555 and outer r1748 down to z -500
	{''', '''	const bool bOpen = ZoogiFrozen::OpenRing();
	if (bOpen)
	{	// b119 (Evan: "increase the diameter of the inner arena to close the gap"): a gentle snow slope over the old crevasse,
		// court edge z 0 -> Ring 2 floor z -30, one continuous field
		TArray<TArray<FVector>> SlopeH;
		const float RIn = CrevIn - 14.f, ROut = R2In + 10.f;
		const int32 Ns = 120;
		for (int32 i = 0; i < Ns; ++i)
		{
			const float A0 = 360.f * i / Ns, A1 = 360.f * (i + 1) / Ns + 0.2f;
			const float Ro = ROut / FMath::Cos(FMath::DegreesToRadians(0.5f * (A1 - A0)));
			TArray<FVector>& Pts = SlopeH.AddDefaulted_GetRef();
			for (float Aa : { A0, A1 })
			{
				Pts.Add(P(RIn, Aa, 0.f)); Pts.Add(P(Ro, Aa, R2Z));
				Pts.Add(P(RIn, Aa, R2Z - 40.f)); Pts.Add(P(Ro, Aa, R2Z - 40.f));
			}
		}
		if (AZoogiObstacle* O = Spawn(TEXT("Ring2_GapFill"))) O->BuildCollider(SlopeH, 1.f, FloorPMr);
		FZoogiMeshBuilder Sv;
		for (int32 b = 0; b < 10; ++b)
		{
			const float R0 = FMath::Lerp(RIn, ROut, b / 10.f), R1 = FMath::Lerp(RIn, ROut, (b + 1) / 10.f);
			const float Z = FMath::Lerp(0.f, R2Z, (b + 0.5f) / 10.f);
			Sv.ArcSlab(R0, R1 + 1.f, 0.f, 360.f, Z - 24.f, Z + 1.f, 144);
		}
		Vis(Sv, Snow);
		UE_LOG(LogZoogi, Log, TEXT("FROZEN open ring: crevasse filled r%.0f-%.0f"), RIn, ROut);
	}
	// ---- crevasse faces (visual): inner ice cliff r1555 and outer r1748 down to z -500
	if (!bOpen)
	{''')
ed1(R2, '''		for (const FVector2D& A : OpenArcs(CurbGaps))
		{''', '''		for (const FVector2D& A : OpenArcs(CurbGaps))
		{
			if (bOpen) break; // b119: no curb''')
ed1(R2, '''		for (const FVector2D& A : OpenArcs(WallGaps))
		{''', '''		for (const FVector2D& A : OpenArcs(WallGaps))
		{
			if (bOpen) break; // b119: no bank walls''')
ed1(R2, '''		for (const FVector2D& G : ZoogiFrozen::RimGapArcs())
		{''', '''		for (const FVector2D& G : ZoogiFrozen::RimGapArcs())
		{
			if (bOpen) break; // b119: no crevasse, no lip''')
ed1(F, '''	BuildWallCut(WallNA0, WallNA1, true);
	BuildWallCut(WallWA0, WallWA1, false);
	BuildWallCut(WallEA0, WallEA1, false);''', '''	const bool bOpenRing = ZoogiFrozen::OpenRing(); // b119 (Evan): "remove the bridges and all wall blocks"
	if (!bOpenRing)
	{
		BuildWallCut(WallNA0, WallNA1, true);
		BuildWallCut(WallWA0, WallWA1, false);
		BuildWallCut(WallEA0, WallEA1, false);
	}''')
ed1(F, '''	for (const FVector2D& Arc : WallFillArcs) BuildWallCut(Arc.X, Arc.Y, false);''', '''	if (!bOpenRing) for (const FVector2D& Arc : WallFillArcs) BuildWallCut(Arc.X, Arc.Y, false);''')
ed1(F, '''	// ---- rim gaps: low sloped ice lip + ice-edge trim + crystal posts + a soft glow line (indestructible) ----
	if (RimGaps().Num() > 0)''', '''	// ---- rim gaps: low sloped ice lip + ice-edge trim + crystal posts + a soft glow line (indestructible) ----
	if (RimGaps().Num() > 0 && !bOpenRing)''')
ed1(F, '''	// ---- rim ice-block clusters + snow drifts (no collision, destructible by ring-outs / powers) ----
	{''', '''	// ---- rim ice-block clusters + snow drifts (no collision, destructible by ring-outs / powers) ----
	if (!bOpenRing)
	{''')
ed1(F, '''		for (int32 Sp = 0; Sp < (bR2 ? 2 : 1); ++Sp)
		{
			const float RA = (bR2 && Sp == 1) ? R2Out : ZoogiConst::KnockoffRadius - 15.f;''', '''		for (int32 Sp = 0; Sp < (bR2 ? 2 : 1) && !bOpenRing; ++Sp) // b119: no bridges
		{
			const float RA = (bR2 && Sp == 1) ? R2Out : ZoogiConst::KnockoffRadius - 15.f;''')
ed1(RM, '''		bool b = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Frozen"), TEXT("bOuterRoam"), b, GGameIni);''', '''		bool b = !ZoogiFrozen::OpenRing(); // b119: the open ring has no bridges to roam to (-ZoogiFrozenRoam=On still forces it)
		if (GConfig && !ZoogiFrozen::OpenRing()) GConfig->GetBool(TEXT("ZoogiRoll.Frozen"), TEXT("bOuterRoam"), b, GGameIni);''')
print('b119d ok')
