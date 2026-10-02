# Zoogi Roll alpha: game camera spec (follow after shot + reset before next turn)

Source: `/workspace/zoogi-video/alpha.mp4` (640x306, 558 s, Unity build: the Game-view bar "Maximize on Play / Stats / Gizmos" shows in every frame). Timestamps are seconds into the video. Measured from frames sampled at 5 to 30 fps around each release and cut, plus a per-frame image-difference motion profile (`cam/md.py`). Contact sheet: `/workspace/zoogi-video/alpha-camera-contact.png`.
Units: D = one Zoogi diameter. In the current game D = 1.0 world unit (`ZOOGI_DRAW_RADIUS = 0.5`).

## Shots measured

| # | release | cam reacts | marble path | rest / out | cut | aim pose settled |
|---|---|---|---|---|---|---|
| 1 | 122.8 | 122.8 (same frame) | long roll, second swing at 128.6 | cam still by 132.4 | 134.73 (to a start pad) | ~137.2 |
| 2 | 159.3 | 159.4 | roll + deflection 163.0 | cam still ~164.8 | 165.9 | ~168.2 |
| 3 | 183.3 | 183.4 | straight roll, hits an orb ~184-185 | cam still ~189.0 | 191.6 | ~193.8 |
| 5 | 250.25 | 250.3 | rolls off the edge ~251.3, below the island 252.3 | falls out of view ~252.9 | 253.0 | ~255.0 |
| 6 | 270.3 | 270.4 | bounces (trail changes direction 271-272) | cam mostly still ~275.0, slow yaw 275.6-277.4 | 277.9 | ~280.2 |
| 9 | 344.2 | 344.2 | bounce/ricochet | slow drift 347-350 | 350.3 | ~352 (player starts aiming at once) |

The separate video-description pass gave coarser times (01:57 / 02:11 / 02:14 ...). Where they disagree, this table follows the frames.

## Spec (numbered, implementable)

### A. Aim pose (what each turn resets to)
1. **Placement.** The camera sits behind the active Zoogi, on the aim heading, and looks along the aim arrow. The arrow always points straight up the screen (136.8, 170.0, 192.6, 254.0, 351.6).
2. **Pitch:** about 30° down (range 25-35°). Only the marble, its pad or ground, and the arena ahead are in view (136.8, 170.0).
3. **Distance:** about 7 D from the camera to the marble center (6.6 D at 136.8, 7.6 D at 170.0). This assumes a 60° vertical FOV, which is Unity's default. With the current 44° match FOV, use about 10 D to get the same framing. Offset: back ≈ 6.1 D, up ≈ 3.5 D (for 7 D at 30°).
4. **Framing:** the look-at is the marble, with the marble slightly below screen center (about 52-55% down the frame).
5. **Default heading** at turn start: toward the arena center (description pass, and 134.8 to 136.8 on a start pad). A marble that starts its turn on the court faces its last aim heading, or the arena center.
6. **Rotation while aiming:** in the alpha, the camera orbits the marble, eased, to stay behind the aim arrow as it rotates (166.5-174.3: the view swings around the marble as the arrow turns, no cuts). The alpha rotated its aim with an arrow. Our game aims with a ground-plane slingshot drag that raycasts through this camera (`Zoogi.tsx` handlePointerMove), so see A7.
7. **Slingshot constraint (game-specific, not in the video):** while `isAiming` is true, the camera must not change the ground-plane mapping under the finger in a way that breaks the pull. Do not orbit during a drag. A full pull is `FULL_PULL_DISTANCE = 15` units behind the marble, but a 7 D / 30° pose only shows about 3 units behind the marble. So while a drag is active, ease the camera back and up along the same heading (λ ≈ 6/s) just far enough for a full pull to stay on screen, then return to the aim pose if the drag is cancelled. Do not change any input or launch math.
8. **No shake** while aiming. The alpha shows none anywhere.

### B. Follow after the shot (release → everything at rest)
9. **Start:** the camera reacts in the same frame as the release, or the next one. Lag is ≤ 0.1 s (122.8, 159.3→159.4, 183.3→183.4, 250.25→250.3, 270.3→270.4, 344.2).
10. **Target:** the shot Zoogi only, for the whole shot. It stays the target through collisions: at 184-190, after hitting an orb, the shot marble stays centered and the orb leaves frame. It stays the target through bounces (271-273, 345-347). No group framing, no switching to the hit body.
11. **Look-at:** tight on the shot marble, which stays centered (184.0, 184.6, 190.0). Use near-direct tracking (λ_look ≈ 10/s) or a direct lookAt.
12. **Rise and pull back:** from the aim offset, eased, to a high overview offset. Pitch goes 30° → about 55° (range 50-65°; shot 1 got close to top-down, about 70°, at 130-134). Distance goes 7 D → about 24 D (range 20-30 D; roughly the whole island in frame at 190.0 and 132.0). Exponential ease with λ_rise ≈ 0.7/s: about 60% there 1.3 s after release (184.6), about 93% at 3.7 s (187.0), settled by about 5-6 s.
13. **Position lag:** the camera follows target+offset with an eased, not rigid, follow. λ_pos ≈ 2.5/s on XZ. The marble runs ahead of the camera, up the screen, while the look-at keeps it centered (183.5-185.5).
14. **Heading:** keep the release heading. A straight shot shows no yaw change (shot 3, 183.3-190). After bounces the heading drifts slowly toward the marble's travel direction: about 60-90° over about 6 s in shot 6 (271→277.6) and about 45-60° over about 5 s in shot 9 (345→349.8). Implement as yaw damped toward the planar velocity heading only while speed > 0.5 u/s, λ_yaw ≈ 0.5/s, rate cap 20°/s. Never snap or flip 180°. (The description pass said "no rotation to face travel". That matches straight shots. The frames show the slow drift after bounces.)
15. **Falling off the edge:** keep following the marble down. The look-at tracks the marble's Y and the camera position follows with the same lag, so the camera dips below the island and looks at the falling marble against the sky from under the island (250.8-252.9). The alpha did no collision avoidance: the camera clipped through rock at 251.4-252.2. Recommended: skip clipping avoidance, but clamp the camera so it never goes more than about 8 units below y=0. When the marble is knocked out or hidden (`isKnockedOut`, `isRespawning`, or y < `FALL_OUT_Y`), hold the last camera pose until the turn changes.
16. **Linger:** after the marble stops, the camera just keeps easing: it finishes its rise and any yaw drift, then holds. The image is essentially still for 2.3-3.0 s before the cut (132.4→134.73, 189.0→191.6, ~275→277.9, ~347.2→350.3). The fall case (253.0) cut about 0.1-0.7 s after the marble left view. The camera does **not** run its own linger timer: the cut happens on the game's turn-change event (C17).

### C. Reset before the next turn
17. **Trigger:** the actual turn change in store state, meaning the active-actor key changes: `gameMode==="local_multiplayer" ? currentLocalPlayerIndex : (isPlayerTurn ? "p" : "e"+turnIndex)`, plus `currentRound`. Do not fire off `allMovementStopped` or a separate timer. Linger length then comes from the game's settle rule (main: `SETTLE_DELAY_SECONDS = 1.5`; PR #34: rest + 1.2 s).
18. **Transition = hard cut, then an eased swoop into the aim pose.** Every cut is a one-frame jump with no fade or pan (134.70→134.73, 165.9, 191.5→191.6, 253.0, 277.8→277.9, 350.3). The first frame after the cut is **not** the final aim pose:
    - The camera teleports to `newTarget + currentOffset`. It keeps the overview offset it had at the end of the follow (same distance and pitch, same heading), so the first frame shows the new marble from high up. From a start pad after a long shot that is near top-down with a small marble (134.8, contact frame 8); after shorter shots it is about 55-60° (191.6, contact frame 6).
    - Yaw then eases to the new aim heading: 90% in about 0.5 s (arrow swings to straight up by 135.2 and 192.2). λ ≈ 4.6/s.
    - Distance and pitch ease down to the aim pose: 90% in about 1.2 s, fully settled in about 2.2 s (134.73→~137.2, 191.6→~193.8, 253.0→~255.0, 277.9→~280.2). λ ≈ 1.9/s.
19. **Input:** the aim arrow and control are live from the cut frame. The player can start aiming during the swoop (134.8, 191.6: arrow already drawn, and 351.4 the player is already aiming). The camera must not block input while it eases.
20. **AI turns** use the same cut, swoop, aim hold, and follow. The aim hold lasts only as long as the AI's own delay.

### D. Smoothing and implementation rules
21. All easing is exponential and frame-rate independent: `x += (target - x) * (1 - exp(-λ·dt))`. Use the existing `damp()` in `cameraRig.ts`, driven by the render-loop `delta` (clamped to 0.05 s), so 30/60/144 Hz agree. Yaw goes through a shortest-angle wrap.
22. Suggested constants: `AIM_PITCH_DEG=30`, `AIM_DIST=7` (at FOV 60), `FOLLOW_PITCH_DEG=55`, `FOLLOW_DIST=24`, `λ_rise=0.7`, `λ_pos=2.5`, `λ_look=10`, `λ_yawFollow=0.5` (cap 20°/s, only above 0.5 u/s), `λ_resetYaw=4.6`, `λ_resetDolly=1.9`, `FALL_CAM_MIN_Y=-8`. Scale distances by `arenaScaleFor(map)` if the map is scaled.
23. **Shake/effects:** the alpha shows none. Keep the existing trauma/hit shake and ability offsets only as additive on top of the new rig, unchanged. Do not add new shake.

## Unclear or estimated
- FOV is assumed to be 60° vertical (Unity default). The distances in D depend on it.
- Pitch values are eyeballed from floor perspective (±5-10°). The overview pitch varies per shot (about 50-70°), maybe because the alpha's height ramp depends on shot speed or time.
- Heading drift after bounces (B14) could be velocity-following or a long-lag SmoothFollow chasing the marble's rotation. Velocity-following fits the frames best. The slow yaw seen right before the cut in shots 6 and 9 may be a still-rolling marble.
- Linger: the alpha's rest→cut was about 2.5-3 s. Main passes the turn at rest + 1.5 s and PR #34 at rest + 1.2 s, so turns will cut about 1-1.5 s sooner than the alpha, possibly while the camera is still rising. That is a timing mismatch in the turn rule, not in the camera. The camera just follows the event.
- Aim rotation in the alpha came from an arrow-rotate input. Our slingshot drag makes aim-orbiting unsafe, so see A6-A7.
- The cut pose "keep current offset" is inferred from the first post-cut frames. The exact height the alpha teleported to is not certain (134.8 is much higher than 191.6).
