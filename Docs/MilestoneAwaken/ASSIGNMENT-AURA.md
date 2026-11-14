# Assignment for the visual design AI (Aura): Milestone "Awaken", visual spec

Copy and paste this whole file as the task. Attach the images from `ref/` in this folder.

## The game and its look (match this; don't reinvent it)

**Zoogi Roll** is a top-down-ish 3D marble battler. Each player's character, a **Zoogi**, is a glossy painted sphere about 1 m across. Players knock **glowing blue jelly orbs** (soft, translucent, bright cyan-blue, with a blue ground glow) and **gold star orbs** (double points) off floating island arenas, and try to knock rivals off the edge.

- **References** (in `ref/`):

| File | Shows |
|---|---|
| `01_menu_roster` | UMG menu with portraits |
| `02_overview_standby` | elevated overview |
| `03_battle_lockon` | lock-on with dotted path, cyan player ring under the Zoogi, dark-navy translucent HUD panels with coloured accent bars |
| `04_target_closeup` | |
| `05_shot_follow_vfx` | |
| `06_knockout` | big KNOCKOUT! callout |
| `07_results` | results and WINNER badge |
| `08_portraits_all` | the 7 Zoogis on their spawn pads |

- **Roster.** Use these exact names. Where the spec said "Flare" (e.g. Zoogy_Flare), **use Hotstreak**, our fire Zoogi; Evan confirmed this.

| Zoogi | Type | Colours | Signature power |
|---|---|---|---|
| **Wolfgang** | Wolf | #6B7280 / #9CA3AF | Wolf Pack |
| **Hotstreak** | Lava | #EF4444 / #F97316 | Instant Explosion |
| **Lars** | Bouncer | #3B82F6 / #93C5FD | Ricochet |
| **Pinpoint** | Precision | #8B5CF6 / #A78BFA | Laser Trajectory |
| **Bolt** | Electric | #FBBF24 / #FDE047 | Static Shock |
| **Wraps** | Mummy | #8B7355 / #D4C4B0 | Bandage Bind |
| **Nightshade** | Shadow | #6B46C1 / #9F7AEA | Shadow Stun; has small horns on the shell |

- **Style:** bright, saturated, toy-like. Glossy shells, soft bloom, a cyan player ring on the floor, Roboto UI text, and dark navy panels (about #0B1530 at 85% opacity) with a 4 px coloured accent edge.
- **ABSOLUTE RULE: no limbs.** Zoogis stay **spheres**: no arms, legs, faces that pop out, or morphing into creatures, in gameplay **and** in anime.
  - Expression comes only from roll, spin, wobble, squash and stretch, shell pulse and expansion, core light, cracks, orbiting particles, aura, trails, impact compression, shockwaves, debris and elemental bursts.
  - The existing ears and horns on the models stay as they are.
- **Transformations are temporary states on the existing models**, not new models. Each artifact must read clearly on all 7 differently painted shells. Use an overlay layer: rim, emissive cracks or patterns, a translucent shell. Don't repaint the base art.

## 1. Awaken UI

Context: Awaken happens after Draw. The player doesn't move. They choose how to use the artifact drawn this turn.

- **Layout** (1920×1080 reference; must scale down to 1280×720 and up to 4K):
  - **Bottom centre:** the **drawn artifact card** (large, about 260×340), plus up to 2 **saved** cards (smaller, about 170×220) to its left.
  - **Above the cards:** three big action buttons, **INFUSE** (attack), **GUARD** (defend) and **SAVE** (keep). Each about 220×72, with an icon, label and a one-line effect text taken from the artifact.
  - **Top left:** the existing scoreboard, unchanged.
  - **Top centre:** the phase tracker changes to **DRAW · AWAKEN · BATTLE · RESOLVE · END**, using the current tracker style.
  - **Zoogi selection.** For now it's one Zoogi per player, so show the active Zoogi's portrait chip with a "selected" ring. Also design the multi-Zoogi version: clickable portrait chips plus an in-world selection ring.
- **States to cover:** default, hover, pressed, disabled (for example Guard when it's not allowed), the selected artifact, the "Saved" stamp, the hand-full discard warning, and a CPU deciding (a ghosted UI showing the CPU's choice for 0.6 s).
- **Battle aiming controls** (new, clickable):
  - ◀ ▶ cut-angle nudge buttons next to the lock card.
  - Power steps: SOFT / MED / FULL.
  - CONFIRM and CANCEL.
  - Preview graphics: the dotted path (existing), **ghost-ball contact circle**, **target departure arrow**, **shooter deflection arrow** and **bank bounce marker**.
- **Deliverables:**
  - Mockups: PNG, 1920×1080, one per state.
  - A layered source file: Figma link or PSD.
  - Button and panel 9-slice textures: PNG, power-of-two-friendly, with a 9-slice margins spec.
  - A colour and type spec (Markdown).

## 2. Artifact icons (5)

- Ember Stone, Storm Eye, Frost Crystal, Titan Core and Void Shard.
- Each icon is a single readable silhouette on a round gem badge, readable at 48 px.
- **Files:**
  - `T_Icon_Artifact_<Ember|Storm|Frost|Titan|Void>.png`: 512×512, transparent, sRGB.
  - Optionally an SVG.
  - Card art `T_Card_Artifact_<Name>.png`: 512×680.
- **Palette:**

| Artifact | Palette |
|---|---|
| Ember | #FF5A1F core, #FFC14D sparks |
| Storm | #7FD8FF core, #FFFFFF arcs |
| Frost | #9EF0FF, #E8FBFF, slight lilac shadow |
| Titan | #8C7A63 stone, #FFB347 seam glow |
| Void | #2A0F4F, #B06CFF rim, black core |

## 3. The 5 transformations (one board per artifact, every stage)

The visual language runs in this order: **core glow → shell change → aura → particles → movement trail → powered collision → impact**. Give each stage a frame or short loop, a description, and colour and intensity values.

| Stage | Ember Stone | Storm Eye | Frost Crystal | Titan Core | Void Shard |
|---|---|---|---|---|---|
| Core glow | orange-red ember light inside, slow heartbeat pulse | blue-white core, sharp flicker pulse | pale cyan, slow breathing | deep amber seam glow, very slow | core goes dark with a violet rim; centre "lensing" distortion |
| Shell change | glowing **cracks** spread over the shell (emissive crack mask) | **electric veins crawl** over the shell | **frost spreads** from the bottom up (crystalline mask, rougher, white) | **stone plates** lock on (overlay plates, heavier, darker, slight scale-up of 4%) | shell turns glassy-dark, stars or noise inside, faint ripple |
| Aura | heat haze shimmer + ember motes rising | 2-3 **orbiting arcs** around the ball | cold mist pooling at the base | dust settles around it; ground darkens under it | 2 dark **orbiting rings** (tilted), slow |
| Particles | embers, small fire licks | sparks jumping to the floor | tiny snow and ice glitter | pebbles float up and drop | inward-drifting dark motes |
| Trail (when rolling) | fire ribbon + scorch decals | jagged lightning ribbon | **temporary ice trail** on the floor (fades in about 2 s) | ground cracks / dust plume, heavier roll (less wobble) | dark smear ribbon with stars |
| Powered collision | squash, then a flash of the cracks | white flash + chain arcs to nearby targets | shards crystallise at the contact point | big compression, a ground-thump ring | brief **gravity pull**: nearby things lean inward, then snap |
| Impact | **circular flame burst** ring | **lightning burst / chain** | **ice shards** spray in a fan | **strong ground shockwave** + debris chunks | implosion ring → release pulse |

**Guard look** (one per artifact): a translucent protective shell (bubble) around the Zoogi in the artifact palette. Low intensity while waiting, then a bright flash plus a cracking shield when the guard triggers.

- Ember: a flame-ring shield
- Storm: an arc cage
- Frost: an ice shell
- Titan: rock plating
- Void: a ring-warp barrier

**Normal (unpowered) attacks stay fast**: just the existing sparks and flash. Powered effects must be clearly bigger, but still read in under 1 s.

**Deliverables per artifact:**
- One board PNG (3840×2160) showing all stages on at least 2 different Zoogis (for example a light one like Lars and a dark one like Nightshade).
- Looping reference GIF or MP4 (512 px, 2-3 s) for aura, trail and impact.
- Texture masks, provided or described, that the Unreal AI can build:
  - crack mask, frost mask, plate mask, vein mask
  - noise textures, tileable 1024², linear
  - flipbooks for flame, arcs, shards and debris: 8×8, 2048², PNG with alpha

## 4. Impact language (shared rules)

- Scale with impact speed:
  - A normal hit is a small spark and a 0.15 s flash.
  - A powered hit adds the artifact burst, shake at 40% and an optional hit-stop of 0.06-0.1 s.
  - A **crit** adds a white rim flash, a radial speed-line frame overlay and a bigger burst (×1.4).
- **KO language:** the existing KNOCKOUT! callout and edge burst. Add an artifact tint to the edge burst when the KO came from a powered hit.
- **Camera:** the gameplay camera stays elevated and tactical. Emphasis is small. Drama belongs in anime sequences, not gameplay.

## 5. Anime storyboards (first sequences)

The game result is already decided. Anime **only dramatizes** it and must **start and end on the exact gameplay frame**. Match the current transformation state: if the Zoogi has the Void glow in the arena, it has it in the anime. Use stylised cel or anime lighting, speed lines, impact frames and smear frames, **all with spheres only**.

Storyboard these first. For each one deliver 8-16 panels, timing notes per panel, camera notes, a colour script, and the in and out frames matched to gameplay.

| Sequence id | Tier / length | Beat outline |
|---|---|---|
| `Anime_Any_Ember_Infuse_01` | Micro 1.2 s | dark frame → ember gem slams into the shell → cracks race across → flare → back to arena |
| `Anime_Hotstreak_Ember_Infuse_01` | Standard 2.5 s | affinity: Hotstreak's shell erupts, a lava ring spins up around it, heat-haze close-up, logo flash |
| `Anime_Hotstreak_Ember_Critical_01` | Standard 3.5 s | (Evan's example, Flare = Hotstreak) contact freeze → white impact frame → Hotstreak's cracked shell blazes, the circular flame burst rolls outward in a cel-shaded ring → target flung toward the edge → smoke clears on the exact gameplay frame |
| `Anime_Any_Storm_PoweredHit_01` | Micro 1.0 s | freeze at contact → white impact frame → lightning forks to the targets → snap back |
| `Anime_Any_Frost_Guard_01` | Micro 0.8 s | frost blooms over the shell, the ice bubble seals with a chime |
| `Anime_Any_Titan_Critical_01` | Standard 3 s | slow push-in on the plated Zoogi → compression smear → shockwave ripping the floor → debris settling |
| `Anime_Nightshade_Void_Critical_01` | Special 5 s | horns silhouette against violet rings → the gravity well bends the frame → target pulled in → release blast |
| `Anime_Any_Any_KO_01` | Standard 2.5 s | target flies off the edge in slow motion, the shell spins past the camera, KNOCKOUT! type slam |

**Deliverables:**
- Storyboards: PDF or PNG sequence.
- An animatic for each: MP4, 1920×1080, 30 fps, H.264, with temp sound optional.
- Style frames: 3840×2160 PNG.
- A shot list Markdown file per sequence, saying which parts can be real-time in-engine (camera, Niagara, materials) and which need hand-drawn or pre-rendered overlays.

## File naming and hand-off

- Put everything in `Art/MilestoneAwaken/` in the repo, or send it as a zip with that structure:
  - `UI/`, `Icons/`, `Boards/`, `Masks/`, `Flipbooks/`, `Storyboards/`, `Animatics/`
- Unreal-bound textures follow `T_<Thing>_<Variant>.png`.
- Add a `README.md` mapping each file to its use and any sRGB or linear notes.

## Definition of done

- [ ] Awaken UI mockups for every state, including the angled-aim controls, at 1080p, with a layered source and the 9-slice spec.
- [ ] 5 artifact icons and 5 cards.
- [ ] 5 transformation boards, each showing all 7 stages on 2 different Zoogis.
- [ ] 5 Guard-look boards.
- [ ] Masks and flipbooks listed per artifact, usable as overlay materials and Niagara sprites.
- [ ] Impact-language sheet: normal, powered, crit and KO.
- [ ] 8 storyboards + animatics for the sequences above, with in and out frames matching gameplay.
- [ ] **No limbs anywhere.** Spheres only. All 7 real roster names used, with "Flare" mapped to Hotstreak.
- [ ] Colours checked for readability on all 7 arenas: Meadow, Frozen, Volcanic, Cosmic, Arabian, Tomb, Neon. Frost must still read on the Frozen arena.
