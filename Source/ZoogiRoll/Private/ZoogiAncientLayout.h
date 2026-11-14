// Oct 31 (Evan brief "Ancient Collision Meadows", refs C:\dev\art\ancient_meadows): the multi-island arena LAYOUT, shared by the
// builder (ZoogiArenaAncient.cpp: collision + blockout visuals), the off-court / edge rules (AZoogiArena::IsOffCourt) and the
// traversal test. All numbers are WORLD cm (arena actor scale 1). Art orientation as every other map: the match camera sits on
// +Y looking -Y, so screen up = -Y and screen right = +X. Reference image 1 (overhead): North Ruins (red) top-left, Sky Citadel
// (blue) top-right, Emerald Meadows (green) bottom-left, Sunstone Ruins (orange) bottom-right, the spire hub in the middle.
//
// Scale (ZoogiConst): Zoogi r50, full flick 3600 cm/s, rolling decel 450 -> ~144 m on flat stone; ring-out = below FallOutZ -600
// (or r > 8000). So the world spans ~110 m and three levels sit inside the fall line:
//   LOWER  (recovery)     top z -320  : 12 small islets with emergency springs back to the main islands
//   MIDDLE (battle)       top z 0     : hub r1900, Emerald, Sunstone (+ North Ruins raised to +150)
//   UPPER  (high risk)    top z +450  : Sky Citadel, the North Ruins overlook terrace and the narrow high bridge between them
#pragma once
#include "CoreMinimal.h"
#include "Algo/Reverse.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiAncient
{
	enum ERegion : uint8 { Hub, North, Citadel, Emerald, Sunstone, Bridge, Recovery, Terrace, NumRegions };

	/** Convex floor slab: polygon (CCW or CW, convex), top Z, thickness. */
	struct FPlat { TArray<FVector2D> P; float Z = 0.f; float Thick = 60.f; ERegion Region = Hub; FName Name; };
	/** Straight sloped deck from edge-centre A to edge-centre B (Z = top), half width, side curb height (0 = open).
	 *  ExtendA / ExtendB push the deck past an end along its own slope (only ever into the LOWER floor: no lip). */
	struct FRamp { FVector A, B; float HalfW = 260.f; float Curb = 0.f; float Thick = 40.f; float ExtendA = 0.f, ExtendB = 0.f; ERegion Region = Bridge; FName Name; bool bMomentum = false; float Polish = 260.f; };
	// Polish = rolling decel on a momentum lane (cm/s^2; normal stone 450). The east link drops 450 over 42 m (~104 cm/s^2 of
	// gravity along it; a rolling ball only gets 5/7 of that, ~74), so its 60 lets a Zoogi coast all the way down (test: 110 stalled
	// mid-ramp); climbing it needs ~1100 cm/s at the foot.
	/** Recovery islet: octagon at C (Z = top), radius, spring target (Z = floor top there). */
	struct FIslet { FVector C; float R = 420.f; FVector Target; FName Name; };
	/** Scoring portal arch: centre on the floor (Z = floor top), yaw of the arch's width axis (local X = across the opening,
	 *  local Y = through it, toward the hub). An orb inside the opening volume counts as knocked out (existing orb scoring). */
	struct FPortal { FVector C; float Yaw = 0.f; FName Name; const TCHAR* Glow = TEXT("#FFFFFF"); };
	constexpr float PortalHalfW = 180.f, PortalHalfD = 55.f, PortalH = 380.f, PortalPillarX = 235.f;
	/** Solid set piece (collision + art): post (column / tree trunk), wall (battlement run), rock (breakable AZoogiRock). */
	struct FPiece { int32 Kind = 0; FVector A, B; float R = 50.f; float H = 380.f; ERegion Region = Hub; }; // 0 column, 1 palm, 2 tree, 3 wall A->B, 4 breakable rock

	struct FLayout
	{
		TArray<FPlat> Plats;
		TArray<FRamp> Ramps;
		TArray<FIslet> Islets;
		TArray<FVector> OrbSpots;   // orb centre (Z = floor + orb radius + 1)
		TArray<FPortal> Portals;
		TArray<FPiece> Pieces;
		float HubR = 1900.f;
		float SpireR = 330.f;
	};

	/** Nov 1: the v2 sunset meadow look (ZoogiArenaAncientMeadow.cpp) is the default; -ZoogiAncientV1 = the Oct 31 look. */
	inline bool Meadow2() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV1")); return b; }

	inline FVector2D Rot2(const FVector2D& V, float Deg) { const float A = FMath::DegreesToRadians(Deg); return FVector2D(V.X * FMath::Cos(A) - V.Y * FMath::Sin(A), V.X * FMath::Sin(A) + V.Y * FMath::Cos(A)); }

	/** Nov 1 (Evan: "Import [Grok's v14 arena]"): the Blender diorama arena_full_v14 is the Ancient Meadows playfield (default).
	 *  -ZoogiAncientV2 = the v2 procedural meadow on the Oct 31 layout. */
	inline bool V14() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV2")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV1")); return b; }

	#include "ZoogiAncientV14Data.inl"
	#include "ZoogiAncientV15Data.inl"
	/** b162: the v15 arena (2x plaza ring) is the default; -ZoogiAncientV14 = the v14 diorama. */
	inline bool V15() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV15")); return b; } // b164 (Evan): back to v14 by default

	/** v14 rules layout (world cm, plaza on the origin): matches the v14 walk surfaces so the off-court / region / orb-floor rules
	 *  agree with the mesh collision (arena_report_v14.txt). Collision itself comes from the meshes (complex as simple). */
	inline FLayout MakeV14()
	{
		FLayout O;
		const bool b15 = V15(); // b162
		O.HubR = b15 ? 3470.f : 1720.f; O.SpireR = 0.f;
		auto Circle = [&](const TCHAR* N, ERegion Rg, FVector2D C, float R, float Z, int32 Sides, float Phase)
		{
			FPlat P; P.Name = N; P.Region = Rg; P.Z = Z; P.Thick = Z + 640.f;
			const float Rc = R / FMath::Cos(PI / Sides);
			for (int32 i = 0; i < Sides; ++i) { const float A = FMath::DegreesToRadians(Phase) + 2.f * PI * i / Sides; P.P.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Sides == 8 ? R : Rc)); }
			O.Plats.Add(P);
		};
		if (b15)
		{	// b162: layout_v15.json (x 100, y (y - 16) x 100)
			Circle(TEXT("hub"), Hub, FVector2D(0.f, 0.f), 3470.f, 0.f, 48, 3.75f);
			Circle(TEXT("volcano_platform"), Hub, FVector2D(0.f, -5450.f), 1900.f, -180.f, 32, 5.625f);
			Circle(TEXT("island_ruins"), North, FVector2D(-4139.f, -4829.f), 950.f, 450.f, 8, 22.5f);
			Circle(TEXT("island_citadel"), Citadel, FVector2D(4139.f, -4829.f), 950.f, 450.f, 8, 22.5f);
			Circle(TEXT("island_emerald"), Emerald, FVector2D(-5043.f, 458.f), 750.f, 0.f, 8, 22.5f);
			Circle(TEXT("island_sunstone"), Sunstone, FVector2D(5160.f, -1122.f), 750.f, 0.f, 8, 22.5f);
		}
		else
		{
		Circle(TEXT("hub"), Hub, FVector2D(0.f, 0.f), 1720.f, 0.f, 32, 5.625f);
		Circle(TEXT("volcano_platform"), Hub, FVector2D(0.f, -3700.f), 1900.f, -180.f, 32, 5.625f);
		Circle(TEXT("island_ruins"), North, FVector2D(-3000.f, -3500.f), 950.f, 450.f, 8, 22.5f);
		Circle(TEXT("island_citadel"), Citadel, FVector2D(3000.f, -3500.f), 950.f, 450.f, 8, 22.5f);
		Circle(TEXT("island_emerald"), Emerald, FVector2D(-3300.f, 300.f), 750.f, 0.f, 8, 22.5f);
		Circle(TEXT("island_sunstone"), Sunstone, FVector2D(3450.f, -750.f), 750.f, 0.f, 8, 22.5f);
		}
		const TCHAR* RN[4] = { TEXT("ramp_ruins"), TEXT("ramp_citadel"), TEXT("ramp_emerald"), TEXT("ramp_sunstone") };
		for (int32 r = 0; r < 4; ++r)
			for (int32 i = 0; i < 16; ++i)
			{
				FRamp R; R.Name = FName(RN[r], i); R.A = b15 ? GV15Ramp[r][i] : GV14Ramp[r][i]; R.B = b15 ? GV15Ramp[r][i + 1] : GV14Ramp[r][i + 1]; R.HalfW = 300.f; R.Curb = 115.f; R.Region = Bridge;
				O.Ramps.Add(R);
			}
		{ FRamp R; R.Name = TEXT("volcano_ramp"); R.A = b15 ? FVector(0.f, -3470.f, 0.f) : FVector(0.f, -1720.f, 0.f); R.B = b15 ? FVector(0.f, -4350.f, -180.f) : FVector(0.f, -2600.f, -180.f); R.HalfW = 300.f; R.Region = Bridge; O.Ramps.Add(R); }
		{ FRamp R; R.Name = TEXT("front_stairs"); R.A = b15 ? FVector(0.f, 3500.f, 0.f) : FVector(0.f, 1750.f, 0.f); R.B = b15 ? FVector(0.f, 5480.f, -640.f) : FVector(0.f, 3730.f, -640.f); R.HalfW = 275.f; R.Region = Hub; O.Ramps.Add(R); }
		const FVector Via14[4] = { FVector(-2400.f, -4200.f, 450.f), FVector(-1200.f, -5850.f, 450.f), FVector(1200.f, -5850.f, 450.f), FVector(2400.f, -4200.f, 450.f) };
		const FVector Via15[4] = { FVector(-3539.f, -5529.f, 450.f), FVector(-1200.f, -7600.f, 450.f), FVector(1200.f, -7600.f, 450.f), FVector(3539.f, -5529.f, 450.f) };
		const FVector* Via = b15 ? Via15 : Via14;
		for (int32 i = 0; i < 3; ++i) { FRamp R; R.Name = FName(TEXT("viaduct"), i); R.A = Via[i]; R.B = Via[i + 1]; R.HalfW = 230.f; R.Region = Bridge; R.ExtendA = 60.f; R.ExtendB = 60.f; O.Ramps.Add(R); }
		// island orbs: at each ramp landing (Grok's island spawn spots) and two beside it, clear of the landmarks
		for (int32 k = 8; k < 12; ++k)
		{
			const FVector S = b15 ? GV15Spawns[k] : GV14Spawns[k];
			const FVector2D In = FVector2D(S).GetSafeNormal(), Side(-In.Y, In.X);
			for (int32 j = -1; j <= 1; ++j) O.OrbSpots.Add(FVector(FVector2D(S) + Side * (j * 230.f), S.Z + 41.f));
		}
		return O;
	}

	inline const FLayout& Get()
	{
		if (V14()) { static const FLayout L14 = MakeV14(); return L14; }
		static const FLayout L = []
		{
			FLayout O;
			// ---- hub: 32-gon circumscribing r1900 (apothem = r1900, so the round rules never see past a facet)
			{
				FPlat H; H.Name = TEXT("hub"); H.Region = Hub; H.Z = 0.f; H.Thick = 80.f;
				const int32 N = 32; const float Rc = O.HubR / FMath::Cos(PI / N);
				for (int32 i = 0; i < N; ++i) { const float A = 2.f * PI * (i + 0.5f) / N; H.P.Add(FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rc); }
				O.Plats.Add(H);
			}
			// ---- four regions: rectangles X[2100,4700] x Y[2100,4500] mirrored, inner corner chamfered 700 (the hub bridge lands
			//      square on the chamfer, on the diagonal)
			struct FReg { const TCHAR* Name; float SX, SY, Z; ERegion R; };
			const FReg Regs[] = { { TEXT("north_ruins"), -1.f, -1.f, 150.f, North }, { TEXT("sky_citadel"), 1.f, -1.f, 450.f, Citadel },
				{ TEXT("emerald_meadows"), -1.f, 1.f, 0.f, Emerald }, { TEXT("sunstone_ruins"), 1.f, 1.f, 0.f, Sunstone } };
			for (const FReg& Rg : Regs)
			{
				FPlat P; P.Name = Rg.Name; P.Region = Rg.R; P.Z = Rg.Z; P.Thick = 80.f;
				const float X0 = 2100.f, X1 = 4700.f, Y0 = 2100.f, Y1 = 4500.f, Ch = 700.f;
				const FVector2D Q[] = { { X0 + Ch, Y0 }, { X1, Y0 }, { X1, Y1 }, { X0, Y1 }, { X0, Y0 + Ch } };
				for (const FVector2D& V : Q) P.P.Add(FVector2D(V.X * Rg.SX, V.Y * Rg.SY));
				if (Rg.SX * Rg.SY < 0.f) Algo::Reverse(P.P); // keep the winding consistent
				O.Plats.Add(P);
				// hub bridge on the diagonal: hub rim -> chamfer midpoint (2450, 2450)
				FRamp B; B.Name = *FString::Printf(TEXT("bridge_hub_%s"), Rg.Name); B.Region = Bridge; B.HalfW = 300.f; B.Curb = 60.f; // curbs taller than the Zoogi radius (50): a low curb is hit on its top edge and pops the ball up and over
				const FVector2D Dir = FVector2D(Rg.SX, Rg.SY).GetSafeNormal();
				B.A = FVector(Dir * O.HubR, 0.f);
				B.B = FVector(FVector2D(2450.f * Rg.SX, 2450.f * Rg.SY), Rg.Z);
				B.ExtendA = 120.f;                       // into the hub (at or below the hub floor)
				B.ExtendB = Rg.Z <= 0.f ? 60.f : 0.f;    // flat bridges overlap both floors
				O.Ramps.Add(B);
			}
			// ---- ring links between neighbouring regions (square onto the facing edges)
			{
				FRamp W; W.Name = TEXT("link_west"); W.A = FVector(-3400.f, -2100.f, 150.f); W.B = FVector(-3400.f, 2100.f, 0.f); W.HalfW = 260.f; W.Curb = 60.f; W.ExtendB = 60.f; W.bMomentum = true; W.Polish = 200.f; O.Ramps.Add(W);
				FRamp E; E.Name = TEXT("link_east_momentum"); E.A = FVector(3400.f, -2100.f, 450.f); E.B = FVector(3400.f, 2100.f, 0.f); E.HalfW = 280.f; E.Curb = 60.f; E.ExtendB = 60.f; E.bMomentum = true; E.Polish = 60.f; O.Ramps.Add(E);
				FRamp S; S.Name = TEXT("link_south"); S.A = FVector(-2100.f, 3300.f, 0.f); S.B = FVector(2100.f, 3300.f, 0.f); S.HalfW = 260.f; S.Curb = 60.f; S.ExtendA = 60.f; S.ExtendB = 60.f; S.bMomentum = true; S.Polish = 200.f; if (!Meadow2()) O.Ramps.Add(S); // Nov 1 v2: no front link (ref: the plaza's front is the gate + stairs)
			}
			// ---- UPPER: North Ruins overlook terrace (z +450) with a ramp up from the region floor, then the narrow high bridge to the Citadel
			{
				FPlat T; T.Name = TEXT("north_terrace"); T.Region = Terrace; T.Z = 450.f; T.Thick = 300.f; // solid block down to the region floor
				T.P = { { -4700.f, -4500.f }, { -3700.f, -4500.f }, { -3700.f, -3500.f }, { -4700.f, -3500.f } };
				O.Plats.Add(T);
				FRamp U; U.Name = TEXT("ramp_north_terrace"); U.Region = Terrace; U.A = FVector(-4200.f, -2300.f, 150.f); U.B = FVector(-4200.f, -3500.f, 450.f); U.HalfW = 250.f; U.Curb = 60.f; U.Thick = 300.f; O.Ramps.Add(U);
				FRamp H; H.Name = TEXT("high_bridge"); H.A = FVector(-3700.f, -4000.f, 450.f); H.B = FVector(2100.f, -4000.f, 450.f); H.HalfW = 180.f; H.Curb = 0.f; H.ExtendA = 60.f; H.ExtendB = 60.f; O.Ramps.Add(H);
			}
			// ---- LOWER recovery islets (top z -320) + where each emergency spring throws the Zoogi (a safe main-floor spot)
			auto Is = [&](const TCHAR* N, float X, float Y, float TX, float TY, float TZ) { FIslet I; I.Name = N; I.C = FVector(X, Y, -320.f); I.Target = FVector(TX, TY, TZ); O.Islets.Add(I); };
			Is(TEXT("rec_hub_s"), Meadow2() ? -1000.f : 0.f, Meadow2() ? 2650.f : 2550.f,     0.f,  1250.f,   0.f);
			Is(TEXT("rec_hub_n"),    0.f, -2550.f,     0.f, -1250.f,   0.f);
			Is(TEXT("rec_hub_w"), -2600.f,     0.f, -1250.f,     0.f,   0.f);
			Is(TEXT("rec_hub_e"),  2600.f,     0.f,  1250.f,     0.f,   0.f);
			Is(TEXT("rec_link_w"), -4300.f,    0.f, -3700.f, -2900.f, 150.f);
			Is(TEXT("rec_link_e"),  4300.f,    0.f,  3700.f,  2900.f,   0.f);
			Is(TEXT("rec_link_s"), Meadow2() ? -1700.f : 0.f, Meadow2() ? 5100.f : 4400.f,     0.f,  1250.f,   0.f);
			Is(TEXT("rec_high_n"),     0.f,-4750.f,     0.f, -1250.f,   0.f);
			Is(TEXT("rec_out_sw"), -5500.f, 3300.f, -3600.f,  3300.f,   0.f);
			Is(TEXT("rec_out_se"),  5500.f, 3300.f,  3600.f,  3300.f,   0.f);
			Is(TEXT("rec_out_nw"), -5500.f,-3000.f, -3600.f, -2900.f, 150.f);
			Is(TEXT("rec_out_ne"),  5500.f,-3000.f,  3600.f, -2900.f, 450.f);
			// ---- scoring portals on each region's far corner, facing the hub (North's on the upper terrace = high-risk shot)
			auto Pt = [&](const TCHAR* N, float X, float Y, float Z, const TCHAR* G) { FPortal P; P.Name = N; P.C = FVector(X, Y, Z); P.Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Y, -X)) + 90.f; P.Glow = G; O.Portals.Add(P); };
			Pt(TEXT("portal_north"), -4250.f, -4250.f, 450.f, TEXT("#4FB4FF")); // blue fortress: blue portal
			Pt(TEXT("portal_citadel"), 4250.f, -4250.f, 450.f, TEXT("#FF8A3A")); // red fortress: orange portal
			Pt(TEXT("portal_emerald"), -4250.f, 4250.f, 0.f, TEXT("#3FE07A"));
			Pt(TEXT("portal_sunstone"), 4250.f, 4250.f, 0.f, TEXT("#FF8A2A"));
			// ---- set pieces (each with a purpose): ruin columns = posts to bank off (North), Citadel battlements on the outer
			//      edges with gaps (precision island: a backstop behind its portal), breakable ruins (Sunstone = destruction island,
			//      a few in the North), palms / trees on the edges (trunks collide)
			auto Pc = [&](int32 K, float X, float Y, float Z, float R, float H, ERegion Rg) { FPiece P; P.Kind = K; P.A = FVector(X, Y, Z); P.B = P.A; P.R = R; P.H = H; P.Region = Rg; O.Pieces.Add(P); };
			auto Wall = [&](float X0, float Y0, float X1, float Y1, float Z, float H) { FPiece P; P.Kind = 3; P.A = FVector(X0, Y0, Z); P.B = FVector(X1, Y1, Z); P.R = 30.f; P.H = H; P.Region = Citadel; O.Pieces.Add(P); };
			Pc(0, -4580.f, -2280.f, 150.f, 55.f, 420.f, North); Pc(0, -2350.f, -4380.f, 150.f, 55.f, 360.f, North);
			Pc(0, -3300.f, -3150.f, 150.f, 55.f, 300.f, North); Pc(0, -2700.f, -3550.f, 150.f, 55.f, 240.f, North);
			Wall(2250.f, -4440.f, 3050.f, -4440.f, 450.f, 120.f); Wall(3350.f, -4440.f, 3950.f, -4440.f, 450.f, 120.f);
			Wall(4640.f, -3850.f, 4640.f, -3150.f, 450.f, 120.f); Wall(4640.f, -2850.f, 4640.f, -2250.f, 450.f, 120.f);
			Pc(4, 3950.f, 2650.f, 0.f, 95.f, 130.f, Sunstone); Pc(4, 4350.f, 2950.f, 0.f, 80.f, 110.f, Sunstone);
			Pc(4, 2750.f, 4050.f, 0.f, 95.f, 130.f, Sunstone); Pc(4, 3250.f, 4250.f, 0.f, 80.f, 110.f, Sunstone);
			Pc(4, 3900.f, 3850.f, 0.f, 75.f, 100.f, Sunstone); Pc(4, 4400.f, 3550.f, 0.f, 75.f, 100.f, Sunstone);
			Pc(4, -2750.f, -3950.f, 150.f, 80.f, 110.f, North); Pc(4, -3100.f, -4380.f, 150.f, 80.f, 110.f, North);
			Pc(1, 4550.f, 2300.f, 0.f, 35.f, 520.f, Sunstone); Pc(1, 2300.f, 4350.f, 0.f, 35.f, 480.f, Sunstone); Pc(1, 3650.f, 4400.f, 0.f, 35.f, 560.f, Sunstone);
			Pc(2, -4500.f, 2300.f, 0.f, 40.f, 460.f, Emerald); Pc(2, -2300.f, 4350.f, 0.f, 40.f, 420.f, Emerald); Pc(2, -3450.f, 4380.f, 0.f, 40.f, 480.f, Emerald); Pc(2, -4550.f, 4350.f, 0.f, 40.f, 440.f, Emerald);
			Pc(1, 4550.f, -2300.f, 450.f, 35.f, 480.f, Citadel);
			// ---- orbs off the hub: 4 spots per region (centre + 3 at r650; the game mode spawns a PAIR per extra spot -> 32 island orbs),
			//      Z = floor + orb r + 1 (the hub ring of 12 comes from the game mode; total ~48 like every map)
			const FVector RC[] = { { -3400.f, -3300.f, 150.f }, { 3400.f, -3300.f, 450.f }, { -3400.f, 3300.f, 0.f }, { 3400.f, 3300.f, 0.f } };
			for (const FVector& C : RC)
			{
				O.OrbSpots.Add(C + FVector(0, 0, 41.f));
				for (int32 k = 0; k < 3; ++k) { const FVector2D D = Rot2(FVector2D(650.f, 0.f), 30.f + 120.f * k); O.OrbSpots.Add(C + FVector(D, 41.f)); }
			}
			return O;
		}();
		return L;
	}

	inline bool InConvex(const TArray<FVector2D>& P, const FVector2D& Q, float Margin)
	{
		// works for either winding: Q must be on the same side of every edge, at least Margin inside (negative = grow)
		int32 Sign = 0;
		for (int32 i = 0; i < P.Num(); ++i)
		{
			const FVector2D A = P[i], B = P[(i + 1) % P.Num()];
			const FVector2D E = B - A; const float Len = E.Size();
			if (Len < 1.f) continue;
			const float D = FVector2D::CrossProduct(E, Q - A) / Len;
			const int32 S = D >= 0.f ? 1 : -1;
			if (Sign == 0) Sign = S == 1 ? 1 : -1;
			if (D * Sign < Margin) return false;
		}
		return true;
	}
	inline void RampQuad(const FRamp& R, TArray<FVector2D>& Out)
	{
		const FVector2D A(R.A), B(R.B), D = (B - A).GetSafeNormal(), S(-D.Y, D.X);
		const FVector2D A2 = A - D * R.ExtendA, B2 = B + D * R.ExtendB;
		Out = { A2 + S * R.HalfW, B2 + S * R.HalfW, B2 - S * R.HalfW, A2 - S * R.HalfW };
	}
	/** True if a floor (any level) lies under P. Margin > 0 shrinks every footprint (safe-spot tests). */
	inline bool OnFloor(const FVector2D& P, float Margin = 0.f)
	{
		const FLayout& L = Get();
		for (const FPlat& Pl : L.Plats) if (InConvex(Pl.P, P, Margin)) return true;
		TArray<FVector2D> Q;
		for (const FRamp& R : L.Ramps) { RampQuad(R, Q); if (InConvex(Q, P, Margin)) return true; }
		for (const FIslet& I : L.Islets) if (FVector2D::Distance(P, FVector2D(I.C)) <= I.R - Margin) return true;
		return false;
	}
	/** Highest floor top under P (or -1e6 if none). */
	inline float FloorZ(const FVector2D& P)
	{
		const FLayout& L = Get();
		float Z = -1e6f;
		for (const FPlat& Pl : L.Plats) if (InConvex(Pl.P, P, 0.f)) Z = FMath::Max(Z, Pl.Z);
		TArray<FVector2D> Q;
		for (const FRamp& R : L.Ramps)
		{
			RampQuad(R, Q);
			if (!InConvex(Q, P, 0.f)) continue;
			const FVector2D A(R.A), B(R.B); const float Len = FVector2D::Distance(A, B);
			const float T = Len > 1.f ? FVector2D::DotProduct(P - A, (B - A) / Len) / Len : 0.f;
			Z = FMath::Max(Z, FMath::Lerp(R.A.Z, R.B.Z, FMath::Clamp(T, 0.f, 1.f)));
		}
		for (const FIslet& I : L.Islets) if (FVector2D::Distance(P, FVector2D(I.C)) <= I.R) Z = FMath::Max(Z, I.C.Z);
		return Z;
	}
	/** Name of the surface a resting body (centre L, radius Rad) sits on: plat / ramp / islet name, "OUT" below the fall line,
	 *  "air" otherwise. Used by the traversal test. */
	inline FString Classify(const FVector& L, float Rad)
	{
		const FLayout& Lay = Get();
		const FVector2D P(L);
		const float Foot = L.Z - Rad;
		if (L.Z < -600.f) return TEXT("OUT");
		FString Best; float BestD = 70.f;
		for (const FPlat& Pl : Lay.Plats) if (InConvex(Pl.P, P, 0.f) && FMath::Abs(Foot - Pl.Z) < BestD) { BestD = FMath::Abs(Foot - Pl.Z); Best = Pl.Name.ToString(); }
		TArray<FVector2D> Q;
		for (const FRamp& R : Lay.Ramps)
		{
			RampQuad(R, Q);
			if (!InConvex(Q, P, 0.f)) continue;
			const FVector2D A(R.A), B(R.B); const float Len = FVector2D::Distance(A, B);
			const float T = Len > 1.f ? FMath::Clamp(FVector2D::DotProduct(P - A, (B - A) / Len) / Len, 0.f, 1.f) : 0.f;
			const float Z = FMath::Lerp(R.A.Z, R.B.Z, T);
			if (FMath::Abs(Foot - Z) < BestD) { BestD = FMath::Abs(Foot - Z); Best = R.Name.ToString(); }
		}
		for (const FIslet& I : Lay.Islets) if (FVector2D::Distance(P, FVector2D(I.C)) <= I.R && FMath::Abs(Foot - I.C.Z) < BestD) { BestD = FMath::Abs(Foot - I.C.Z); Best = I.Name.ToString(); }
		return Best.IsEmpty() ? FString(TEXT("air")) : Best;
	}
	/** Ramp height profile (t 0..1 along the deck): half linear, half smoothstep - the deck eases in at the foot and rolls
	 *  over at the crest (a straight 16 deg ramp threw fast Zoogis ~2.7 m off its top edge). Max slope 1.25 x the average. */
	inline float RampProfile(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 0.5f * (T + T * T * (3.f - 2.f * T)); }
	/** True if L (orb centre) is inside one of the portal openings. */
	inline bool InPortal(const FVector& L, int32* OutIndex = nullptr)
	{
		const FLayout& Lay = Get();
		for (int32 i = 0; i < Lay.Portals.Num(); ++i)
		{
			const FPortal& P = Lay.Portals[i];
			const FVector2D D = Rot2(FVector2D(L - P.C), -P.Yaw);
			const float Z = L.Z - P.C.Z;
			if (FMath::Abs(D.X) < PortalHalfW && FMath::Abs(D.Y) < PortalHalfD && Z > 0.f && Z < PortalH) { if (OutIndex) *OutIndex = i; return true; }
		}
		return false;
	}
	// ---- Battle Royale CPU routing (Oct 31: Ancient is the Royale field). Areas: 0 hub, 1 North (+terrace), 2 Citadel, 3 Emerald,
	//      4 Sunstone, 10+i ramp i, 100+j recovery islet j, -1 nowhere. Region bridges run on the diagonals (hub rim -> chamfer at 2450).
	inline int32 RegionArea(ERegion R) { return R == Hub ? 0 : R == North || R == Terrace ? 1 : R == Citadel ? 2 : R == Emerald ? 3 : R == Sunstone ? 4 : -1; }
	inline FVector2D RegionDiag(int32 Area) { return Area == 1 ? FVector2D(-1, -1) : Area == 2 ? FVector2D(1, -1) : Area == 3 ? FVector2D(-1, 1) : FVector2D(1, 1); }
	inline int32 AreaOf(const FVector2D& P)
	{
		const FLayout& L = Get();
		for (const FPlat& Pl : L.Plats) if (InConvex(Pl.P, P, 0.f)) return RegionArea(Pl.Region);
		TArray<FVector2D> Q;
		for (int32 i = 0; i < L.Ramps.Num(); ++i) { RampQuad(L.Ramps[i], Q); if (InConvex(Q, P, 0.f)) return 10 + i; }
		for (int32 j = 0; j < L.Islets.Num(); ++j) if (FVector2D::Distance(P, FVector2D(L.Islets[j].C)) <= L.Islets[j].R) return 100 + j;
		return -1;
	}
	/** The island area a body in Area belongs to for routing (a ramp -> the area at its nearer end; islets -> -1). */
	inline int32 RouteArea(int32 Area, const FVector2D& P)
	{
		if (Area < 10 || Area >= 100) return Area < 100 ? Area : -1;
		const FRamp& R = Get().Ramps[Area - 10];
		const FVector2D E = FVector2D::Distance(P, FVector2D(R.A)) < FVector2D::Distance(P, FVector2D(R.B)) ? FVector2D(R.A) : FVector2D(R.B);
		return AreaOf(E + (E - FVector2D(R.A + R.B) * 0.5f).GetSafeNormal() * 120.f);
	}
	/** Direction a CPU Zoogi at P should drive to get toward island area To (0 = hub), along floors and bridges. */
	inline FVector2D PathStep(const FVector2D& P, int32 To)
	{
		const FLayout& L = Get();
		const int32 Here = AreaOf(P);
		auto Toward = [&](const FVector2D& Q) { const FVector2D D = Q - P; return D.IsNearlyZero() ? FVector2D::ZeroVector : D.GetSafeNormal(); };
		auto Bridge = [&](int32 Area, FVector2D& HubEnd, FVector2D& RegEnd, FVector2D& Axis)
		{
			Axis = RegionDiag(Area).GetSafeNormal();
			HubEnd = Axis * (L.HubR - 250.f); RegEnd = FVector2D(2450.f * RegionDiag(Area).X, 2450.f * RegionDiag(Area).Y) + Axis * 350.f;
		};
		if (Here >= 10 && Here < 100) // on a bridge / link: ride it toward the end that leads on
		{
			const FRamp& R = L.Ramps[Here - 10];
			const FVector2D A(R.A), B(R.B);
			const int32 AreaA = AreaOf(A + (A - B).GetSafeNormal() * 120.f), AreaB = AreaOf(B + (B - A).GetSafeNormal() * 120.f);
			const bool bToB = AreaB == To || (AreaA != To && (To == 0 ? B.Size() < A.Size() : FVector2D::Distance(B, FVector2D(RegionDiag(FMath::Max(To, 1))) * 3400.f) < FVector2D::Distance(A, FVector2D(RegionDiag(FMath::Max(To, 1))) * 3400.f)));
			const FVector2D End = bToB ? B + (B - A).GetSafeNormal() * 200.f : A + (A - B).GetSafeNormal() * 200.f;
			const FVector2D Ax = (End - P).GetSafeNormal();
			// stay near the deck centre line
			const FVector2D Dir = (B - A).GetSafeNormal(); const FVector2D Side(-Dir.Y, Dir.X);
			const float Off = FVector2D::DotProduct(P - A, Side);
			return (Ax - Side * FMath::Clamp(Off / 200.f, -1.f, 1.f) * 0.6f).GetSafeNormal();
		}
		if (Here == To || Here < 0 || Here >= 100) return FVector2D::ZeroVector; // there (or off the map / on an islet: the spring decides)
		FVector2D HubEnd, RegEnd, Axis;
		if (Here == 0) // hub -> the target region's bridge: line up on its axis, then go
		{
			Bridge(To, HubEnd, RegEnd, Axis);
			const FVector2D Side(-Axis.Y, Axis.X);
			const float Off = FVector2D::DotProduct(P, Side);
			if (FMath::Abs(Off) > 140.f || FVector2D::DotProduct(P, Axis) < L.HubR * 0.35f) return Toward(HubEnd);
			return Toward(RegEnd);
		}
		// in a region: head for its hub bridge (the chamfer), then cross
		Bridge(Here, HubEnd, RegEnd, Axis);
		const FVector2D Chamfer(2450.f * RegionDiag(Here).X, 2450.f * RegionDiag(Here).Y);
		if (FVector2D::Distance(P, Chamfer) > 260.f) return Toward(Chamfer - Axis * 40.f);
		return Toward(HubEnd);
	}
}
