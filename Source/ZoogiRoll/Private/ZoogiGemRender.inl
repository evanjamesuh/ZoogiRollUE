// Pure C++ (no engine types) ray tracer for the rotating artifact stones on the Awaken cards.
// Included by ZoogiUIFx.cpp; kept engine-free so it can be unit-rendered outside UE.
// Each stone is a convex polyhedron (list of planes); an orthographic camera looks at it from slightly above while it spins
// around its vertical axis. Shading: facet-tinted base, internal "thickness" glow, key + rim speculars, fresnel and bright
// facet edges (an anime-style cel-crystal look). Void adds a banded accretion ring.
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>

namespace ZoogiGemCore
{
	struct V3 { float x, y, z; };
	inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline V3 Cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	inline V3 Norm(V3 a) { const float l = std::sqrt(Dot(a, a)); return l > 1e-8f ? a * (1.f / l) : V3{ 0, 0, 0 }; }
	inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
	inline float Smooth(float e0, float e1, float x) { const float t = Clamp01((x - e0) / (e1 - e0)); return t * t * (3.f - 2.f * t); }
	inline float Mix(float a, float b, float t) { return a + (b - a) * t; }

	struct RGBA { float r, g, b, a; }; // linear, straight alpha
	struct Plane { V3 n; float d; };

	struct Shape
	{
		std::vector<Plane> Planes;
		float Radius = 0.5f;      // nominal radius (edge width, ring)
		bool bRing = false;
		float PeriodDeg = 360.f;
	};

	inline void AddFace(Shape& S, V3 a, V3 b, V3 c)
	{
		V3 n = Norm(Cross(b - a, c - a));
		float d = Dot(n, a);
		if (d < 0.f) { n = n * -1.f; d = -d; }
		S.Planes.push_back({ n, d });
	}

	// n-sided bipyramid / column: ring of radius R spanning y in [Off-Hm, Off+Hm]; apex Top above, Bot below.
	// Table > 0 cuts a flat top at height Table above the ring with radius TableR (brilliant cut crown).
	inline void Bipyramid(Shape& S, int n, float R, float Hm, float Top, float Bot, float Off, float Table = 0.f, float TableR = 0.f)
	{
		const float Pi = 3.14159265f;
		for (int k = 0; k < n; ++k)
		{
			const float a0 = 2.f * Pi * k / n, a1 = 2.f * Pi * (k + 1) / n;
			const V3 t0{ R * std::cos(a0), Off + Hm, R * std::sin(a0) }, t1{ R * std::cos(a1), Off + Hm, R * std::sin(a1) };
			const V3 b0{ t0.x, Off - Hm, t0.z }, b1{ t1.x, Off - Hm, t1.z };
			if (Hm > 1e-4f) AddFace(S, b0, t0, t1);
			if (Table > 0.f)
			{
				const V3 u0{ TableR * std::cos(a0), Off + Hm + Table, TableR * std::sin(a0) }, u1{ TableR * std::cos(a1), Off + Hm + Table, TableR * std::sin(a1) };
				AddFace(S, t0, t1, u1);
				(void)u0;
			}
			else AddFace(S, t0, t1, V3{ 0.f, Off + Hm + Top, 0.f });
			AddFace(S, b0, b1, V3{ 0.f, Off - Hm - Bot, 0.f });
		}
		if (Table > 0.f) S.Planes.push_back({ V3{ 0.f, 1.f, 0.f }, Off + Hm + Table });
		S.Radius = R;
	}

	inline Shape MakeShape(int Kind)
	{
		Shape S;
		const float Pi = 3.14159265f;
		switch (Kind)
		{
		case 1: // Ember Stone: tall 5-sided flame shard
			Bipyramid(S, 5, 0.46f, 0.06f, 0.70f, 0.34f, -0.15f);
			S.PeriodDeg = 72.f;
			break;
		case 2: // Storm Eye: octahedron
			Bipyramid(S, 4, 0.62f, 0.0f, 0.70f, 0.70f, 0.f);
			S.PeriodDeg = 90.f;
			break;
		case 3: // Frost Crystal: hexagonal quartz column
			Bipyramid(S, 6, 0.38f, 0.38f, 0.34f, 0.26f, -0.04f);
			S.PeriodDeg = 60.f;
			break;
		case 4: // Titan Core: octagonal brilliant (table, crown, girdle, pavilion)
			Bipyramid(S, 8, 0.70f, 0.08f, 0.f, 0.58f, 0.16f, 0.22f, 0.40f);
			S.PeriodDeg = 45.f;
			break;
		case 5: // Void Shard: dodecahedron (face up, 5-fold about Y) + accretion ring
		default:
		{
			const float Rin = 0.44f;
			S.Planes.push_back({ V3{ 0, 1, 0 }, Rin });
			S.Planes.push_back({ V3{ 0, -1, 0 }, Rin });
			const float El = std::atan(0.5f);
			for (int k = 0; k < 5; ++k)
			{
				const float Au = 2.f * Pi * k / 5.f, Al = Au + Pi / 5.f;
				S.Planes.push_back({ V3{ std::cos(El) * std::cos(Au), std::sin(El), std::cos(El) * std::sin(Au) }, Rin });
				S.Planes.push_back({ V3{ std::cos(El) * std::cos(Al), -std::sin(El), std::cos(El) * std::sin(Al) }, Rin });
			}
			S.Radius = 0.50f;
			S.bRing = true;
			S.PeriodDeg = 72.f;
			break;
		}
		}
		return S;
	}

	inline float PeriodDeg(int Kind) { return MakeShape(Kind).PeriodDeg; }

	// view = Rx(tilt) * Ry(spin) * object
	struct Rot
	{
		float cs, ss, ct, st;
		V3 ToView(V3 o) const
		{
			const V3 a{ o.x * cs + o.z * ss, o.y, -o.x * ss + o.z * cs };
			return { a.x, a.y * ct - a.z * st, a.y * st + a.z * ct };
		}
		V3 ToObject(V3 v) const
		{
			const V3 a{ v.x, v.y * ct + v.z * st, -v.y * st + v.z * ct };
			return { a.x * cs - a.z * ss, a.y, a.x * ss + a.z * cs };
		}
	};

	inline unsigned Hash(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }

	// One sample: returns straight-alpha linear colour.
	inline RGBA Sample(const Shape& S, const Rot& R, float vx, float vy, const float Core[3], const float Rim[3])
	{
		const V3 Ov{ vx, vy, 4.f }, Dv{ 0.f, 0.f, -1.f };
		const V3 O = R.ToObject(Ov), D = R.ToObject(Dv);
		float tN = -1e9f, tF = 1e9f;
		int iN = -1;
		bool bHit = true;
		for (int i = 0; i < (int)S.Planes.size(); ++i)
		{
			const Plane& P = S.Planes[i];
			const float den = Dot(P.n, D), dist = P.d - Dot(P.n, O);
			if (std::fabs(den) < 1e-7f) { if (dist < 0.f) { bHit = false; break; } continue; }
			const float t = dist / den;
			if (den < 0.f) { if (t > tN) { tN = t; iN = i; } }
			else if (t < tF) tF = t;
			if (tN > tF) { bHit = false; break; }
		}
		if (iN < 0) bHit = false;

		RGBA Gem{ 0, 0, 0, 0 };
		if (bHit)
		{
			const V3 Po = O + D * tN;
			const V3 No = S.Planes[iN].n;
			const V3 N = R.ToView(No);
			// distance to the nearest other face plane = closeness to a facet edge
			float Edge = 1e9f;
			for (int j = 0; j < (int)S.Planes.size(); ++j)
			{
				if (j == iN) continue;
				Edge = std::min(Edge, S.Planes[j].d - Dot(S.Planes[j].n, Po));
			}
			const V3 L = Norm(V3{ -0.45f, 0.75f, 0.55f }), L2 = Norm(V3{ 0.75f, -0.25f, 0.45f }), V{ 0.f, 0.f, 1.f };
			const V3 H = Norm(L + V), H2 = Norm(L2 + V);
			const float f = ((Hash((unsigned)iN * 2654435761u + 17u) & 1023) / 1023.f) * 2.f - 1.f; // facet variation
			const float Thick = Clamp01((tF - tN) / (2.f * S.Radius + 1e-4f));
			const float Diff = 0.28f + 0.72f * std::max(0.f, Dot(N, L));
			const float Spec = std::pow(std::max(0.f, Dot(N, H)), 48.f) * 1.25f;
			const float Spec2 = std::pow(std::max(0.f, Dot(N, H2)), 14.f) * 0.55f;
			const float Fres = std::pow(1.f - std::max(0.f, N.z), 3.f);
			const float BaseT = Clamp01(0.16f + 0.16f * f);
			float c[3];
			for (int k = 0; k < 3; ++k)
			{
				const float Base = Mix(Core[k], Rim[k], BaseT);
				const float Glow = Core[k] * (0.1f + 0.5f * Thick);
				c[k] = Base * Diff * 0.85f + Glow * 0.55f + Spec + Spec2 * Rim[k] + Fres * Rim[k] * 0.4f;
			}
			const float EdgeW = 0.022f;
			const float e = 1.f - Smooth(0.f, EdgeW, Edge);
			for (int k = 0; k < 3; ++k) c[k] = Mix(c[k], Mix(Rim[k], 1.f, 0.3f) * 1.1f, 0.65f * e);
			Gem = { c[0], c[1], c[2], 1.f };
		}

		if (S.bRing && std::fabs(D.y) > 1e-5f)
		{
			const float t = -O.y / D.y;
			const V3 P = O + D * t;
			const float r = std::sqrt(P.x * P.x + P.z * P.z);
			const float R0 = S.Radius * 1.2f, R1 = S.Radius * 1.75f;
			if (r > R0 && r < R1 && (!bHit || t < tN))
			{
				const float u = (r - R0) / (R1 - R0);
				const float Ang = std::atan2(P.z, P.x);
				const float Band = 0.55f + 0.45f * std::sin(15.f * Ang + u * 6.f);
				const float Soft = Smooth(0.f, 0.18f, u) * Smooth(1.f, 0.7f, u);
				const float A = Clamp01(0.9f * Soft * (0.55f + 0.45f * Band));
				const float Lum = 0.75f + 0.5f * Band;
				RGBA Ring{ Mix(Rim[0], 1.f, 0.35f) * Lum, Mix(Rim[1], 1.f, 0.35f) * Lum, Mix(Rim[2], 1.f, 0.35f) * Lum, A };
				// ring in front of the gem (or no gem): ring over gem
				const float a = Ring.a + Gem.a * (1.f - Ring.a);
				if (a > 1e-5f)
				{
					Gem = { (Ring.r * Ring.a + Gem.r * Gem.a * (1.f - Ring.a)) / a, (Ring.g * Ring.a + Gem.g * Gem.a * (1.f - Ring.a)) / a, (Ring.b * Ring.a + Gem.b * Gem.a * (1.f - Ring.a)) / a, a };
				}
			}
		}
		return Gem;
	}

	/** Renders one Size x Size frame (2x2 supersampled) into Out (straight-alpha linear). Extent: view half-width. */
	inline void RenderFrame(int Kind, const float Core[3], const float Rim[3], int Size, float SpinDeg, float TiltDeg, RGBA* Out)
	{
		const Shape S = MakeShape(Kind);
		const float Pi = 3.14159265f;
		Rot R{ std::cos(SpinDeg * Pi / 180.f), std::sin(SpinDeg * Pi / 180.f), std::cos(TiltDeg * Pi / 180.f), std::sin(TiltDeg * Pi / 180.f) };
		const float Extent = 0.98f;
		const int SS = 2;
		for (int y = 0; y < Size; ++y)
		{
			for (int x = 0; x < Size; ++x)
			{
				float pr = 0, pg = 0, pb = 0, pa = 0; // premultiplied accumulation
				for (int sy = 0; sy < SS; ++sy)
					for (int sx = 0; sx < SS; ++sx)
					{
						const float fx = (x + (sx + 0.5f) / SS) / Size * 2.f - 1.f;
						const float fy = 1.f - (y + (sy + 0.5f) / SS) / Size * 2.f;
						const RGBA c = Sample(S, R, fx * Extent, fy * Extent, Core, Rim);
						pr += c.r * c.a; pg += c.g * c.a; pb += c.b * c.a; pa += c.a;
					}
				const float n = float(SS * SS);
				pa /= n;
				RGBA o{ 0, 0, 0, pa };
				if (pa > 1e-5f) { o.r = pr / n / pa; o.g = pg / n / pa; o.b = pb / n / pa; }
				Out[y * Size + x] = o;
			}
		}
	}
}
