# Frozen Ring: outer action ring (Ring 2)

Built Oct 14 2026 from the approved plan (`frozen-polish/OUTER_RING_PLAN.md`, `outer_ring_plan.png`, `outer_ring_plan_zoom.png`).
Code: `Source/ZoogiRoll/Private/ZoogiFrozenRing2.cpp` (geometry, pockets/orbs, bumpers, wings, ramps), layout constants in
`Public/ZoogiFrozenLayout.h`. `-ZoogiFrozenRing2=Off` skips Ring 2 (the constant moves listed below still apply).
Units are cm. UE yaw 0 = +X; 90 = +Y (camera side).

| element | geometry |
|---|---|
| Ring 1 (court) | Unchanged: floor and knockout seal r1550, hub, pillars (unbreakable), obelisks, walls WallR 1455 h95. The Ring 1 wall arcs open ±4° at the 6 ramp yaws: fill arcs (6.5–26) (34–38.5) (51.5–63.5) (71.5–108.5) (116.5–128.5) (141.5–146) (186.5–218.5) (231.5–236) (244–262) (304–308.5), W arc 154–173.5, N arc 262–296, E arc 321.5–353.5 |
| Crevasse | r1550–1750, no floor. Ice cliff faces at r1550 and r1750 going down to z −500 (visual). The snow lip / mounds / drapes that sat at r1553–1780 are not built. A Zoogi that falls in drops below FallOutZ and is out |
| Ring 2 floor | r1750–2060 (to 2100 under the wall), top z −30, packed snow, collision (AZoogiObstacle `Ring2_Floor`) |
| Ice slide arcs | Full width at yaw 335–25 and 155–205; inner half (r1765–1905) at 240–300. Zone trigger: rolling decel 115 |
| Inner curb | r1750–1765, h15. Open ±3.6° at the bridge mouths and ±3.3° at the ramp mouths |
| Outer bank walls | Inner face r2060, 30 thick, h70 (top z +40). Ring 1 wall restitution (0.9). Open ±3.5° at the 6 bridges (span B) and ±3.1° at the 6 pocket mouths |
| Bridges (6: 0/45/135/180/225/315) | The existing bridges (r1535 → islet apron) now cross the crevasse and Ring 2 and continue to the islets (islets moved out, so the decks are longer) |
| Re-entry ramps (6) | Yaws 30, 67.5, 112.5, 150, 240, 300. From r1765 (z −30) to r1520 (z 0), 245 long, ~7°, half width 85, ice, side curbs h20, no kicker |
| Orb pockets (6) | Yaws 22.5, 90, 157.5, 202.5, 270, 337.5. Alcove floor at r2150, concave back wall radius 112 around r2150, h70. Outer orb r40 in a cup at r2150 |
| Outer orbs | Pickup trigger sphere r40: +10 Episode Energy, recharge 12 s (orb hidden while spent) |
| Crystal bumpers (6) | r1905 at yaws 78, 102, 258, 282, 191.25, 348.75. Collider r35 h120, bounce gain 1.2 (max out 1400 cm/s) |
| Curved deflector wings (12) | 2 per bridge mouth at r1995, ±150 lateral from the bridge axis, 35° to the axis, ~150 long (3 facets bent along r200), h45 |
| Islets | IsletDist 2750 → 2900 (front edge 2260) |
| Outcrop | r1980 → r2650 at yaw 270 |
| Rim falls | Yaw 67.5/112.5 → 80/100 (into the crevasse) |
| Ring-out | OUT when the Zoogi falls below FallOutZ (crevasse, edges) **or** its centre goes past r2300 (`AZoogiArena::OuterOutR`, `AZoogiArena::IsRingOut`, used by the game mode). Ring 2 and the pockets are in play. Test: `ZoogiRoll.Frozen.Ring2RingOut` |
| Camera | Frozen overview/tactical booms ×1.39 (`AZoogiArena::CamScale`), so the frame holds r2300 (all of Ring 2 and the pockets) |

Not built here (the plan gives these to the aerial / episode-engine worker): halos above Ring 2, Rescue Boulders r3850, Orbital Rail r4150, bridge lane surface roles (ice deck / crossfall / curbs on spans A and B), Recovery Gates on span B.
Destruction stays off; nothing in Ring 2 is a destructible piece or in Crystal Collapse.
